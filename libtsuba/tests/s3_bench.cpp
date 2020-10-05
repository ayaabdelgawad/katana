#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <list>

#include "bench_utils.h"
#include "galois/FileSystem.h"
#include "galois/Logging.h"
#include "galois/Result.h"
#include "tsuba/file.h"
#include "tsuba/s3_internal.h"
#include "tsuba/tsuba.h"

// Benchmarks both tsuba interface and S3 internal interface

constexpr static const char* const s3bucket = "witchel-tests-east2";
constexpr static const char* const kSepStr = "/";
constexpr static const char* const tmpDir = "/tmp/s3_test/";

// TODO: 2020/06/15 - Across different regions

/******************************************************************************/
/* Utilities */

// Thank you, fmt!
std::string
CntStr(int32_t i, int32_t width) {
  return fmt::format("{:0{}d}", i, width);
}
std::string
MkS3obj(int32_t i, int32_t width) {
  constexpr static const char* const s3obj_base = "s3_test/test-";
  std::string url(s3obj_base);
  return url.append(CntStr(i, width));
}
std::string
MkS3URL(const std::string& bucket, const std::string& object) {
  constexpr static const char* const s3urlstart = "s3://";
  return s3urlstart + bucket + kSepStr + object;
}

class Experiment {
public:
  std::string name_{};
  uint64_t size_{UINT64_C(0)};
  std::vector<uint8_t> buffer_;
  int batch_{8};
  int numTransfers_{4};  // For stats

  Experiment(const std::string& name, uint64_t size)
      : name_(name), size_(size) {
    buffer_.reserve(size_);
    buffer_.assign(size_, 0);
    init_data(buffer_.data(), size_);
  }
};

/******************************************************************************/
// Storage interaction
//    Each function is a timed test, returns vector of times in microseconds
//    (int64_ts)

std::vector<int64_t>
test_mem(const Experiment& exp) {
  std::vector<int32_t> fds(exp.batch_, 0);
  std::vector<int64_t> results;

  struct timespec start;
  for (auto j = 0; j < exp.numTransfers_; ++j) {
    start = now();
    for (auto i = 0; i < exp.batch_; ++i) {
      fds[i] = memfd_create(CntStr(i, 4).c_str(), 0);
      if (fds[i] < 0) {
        GALOIS_WARN_ONCE(
            "memfd_create: fd {:04d}: {}", i, galois::ResultErrno().message());
      }
      ssize_t bwritten = write(fds[i], exp.buffer_.data(), exp.size_);
      if (bwritten != (ssize_t)exp.size_) {
        GALOIS_WARN_ONCE(
            "Short write tried {:d} wrote {:d}: {}", exp.size_, bwritten,
            galois::ResultErrno().message());
      }
    }
    results.push_back(timespec_to_us(timespec_sub(now(), start)));

    for (auto i = 0; i < exp.batch_; ++i) {
      int sysret = close(fds[i]);
      if (sysret < 0) {
        GALOIS_WARN_ONCE("close: {}", galois::ResultErrno().message());
      }
    }
  }
  return results;
}

std::vector<int64_t>
test_tmp(const Experiment& exp) {
  std::vector<int32_t> fds(exp.batch_, 0);
  std::vector<std::string> fnames;
  std::vector<int64_t> results;
  // TOCTTOU, but I think harmless
  if (access(tmpDir, R_OK) != 0) {
    if (mkdir(tmpDir, 0775) != 0) {
      GALOIS_LOG_WARN("mkdir {}: {}", tmpDir, galois::ResultErrno().message());
      exit(EXIT_FAILURE);
    }
  }

  for (auto i = 0; i < exp.batch_; ++i) {
    std::string s(tmpDir);
    fnames.push_back(s.append(CntStr(i, 4)));
  }

  struct timespec start;
  for (auto j = 0; j < exp.numTransfers_; ++j) {
    start = now();
    for (auto i = 0; i < exp.batch_; ++i) {
      fds[i] = open(
          fnames[i].c_str(), O_CREAT | O_TRUNC | O_RDWR, S_IRWXU | S_IRWXG);
      if (fds[i] < 0) {
        GALOIS_WARN_ONCE(
            "/tmp O_CREAT: fd {:d}: {}", i, galois::ResultErrno().message());
      }
      ssize_t bwritten = write(fds[i], exp.buffer_.data(), exp.size_);
      if (bwritten != (ssize_t)exp.size_) {
        GALOIS_WARN_ONCE(
            "Short write tried {:d} wrote {:d}: {}", exp.size_, bwritten,
            galois::ResultErrno().message());
      }
      // Make all data and directory changes persistent
      // sync is overkill, could sync fd and parent directory, but I'm being
      // lazy
      sync();
    }
    for (auto i = 0; i < exp.batch_; ++i) {
      int sysret = close(fds[i]);
      if (sysret < 0) {
        GALOIS_LOG_WARN("close: {}", galois::ResultErrno().message());
      }
    }
    results.push_back(timespec_to_us(timespec_sub(now(), start)));

    for (auto i = 0; i < exp.batch_; ++i) {
      int sysret = unlink(fnames[i].c_str());
      if (sysret < 0) {
        GALOIS_LOG_WARN("unlink: {}", galois::ResultErrno().message());
      }
    }
  }
  return results;
}

std::vector<int64_t>
test_s3_sync(const Experiment& exp) {
  std::vector<std::string> s3objs;
  std::vector<int64_t> results;
  for (auto i = 0; i < exp.batch_; ++i) {
    s3objs.push_back(MkS3obj(i, 4));
  }

  struct timespec start;
  for (auto j = 0; j < exp.numTransfers_; ++j) {
    start = now();
    for (const auto& s3obj : s3objs) {
      // Current API rejects empty writes
      if (auto res = tsuba::internal::S3PutSingleSync(
              s3bucket, s3obj, exp.buffer_.data(), exp.size_);
          !res) {
        GALOIS_WARN_ONCE("S3PutSingleSync bad return {}", res.error());
      }
    }
    results.push_back(timespec_to_us(timespec_sub(now(), start)));
  }
  return results;
}

// This one closely tracks s3_sync, not surprisingly
std::vector<int64_t>
test_s3_async_one(const Experiment& exp) {
  std::vector<std::string> objects;
  std::vector<int64_t> results;
  for (auto i = 0; i < exp.batch_; ++i) {
    objects.push_back(MkS3obj(i, 4));
  }

  struct timespec start;
  for (auto j = 0; j < exp.numTransfers_; ++j) {
    start = now();
    for (const auto& object : objects) {
      tsuba::internal::CountingSemaphore sema;
      // Current API rejects empty writes
      if (auto res = tsuba::internal::S3PutSingleAsync(
              s3bucket, object, exp.buffer_.data(), exp.size_, &sema);
          !res) {
        GALOIS_LOG_ERROR("S3PutSingleAsync return {}", res.error());
      }
      // Only 1 outstanding store at a time
      tsuba::internal::S3PutSingleAsyncFinish(&sema);
    }
    results.push_back(timespec_to_us(timespec_sub(now(), start)));
  }
  return results;
}

std::vector<int64_t>
test_s3_single_async_batch(const Experiment& exp) {
  std::vector<std::string> objects;
  std::list<tsuba::internal::CountingSemaphore> semas;
  std::vector<int64_t> results;
  for (auto i = 0; i < exp.batch_; ++i) {
    objects.push_back(MkS3obj(i, 4));
    semas.emplace_back();
  }

  struct timespec start;
  for (auto j = 0; j < exp.numTransfers_; ++j) {
    start = now();
    int i = 0;
    for (auto& sema : semas) {
      if (auto res = tsuba::internal::S3PutSingleAsync(
              s3bucket, objects[i], exp.buffer_.data(), exp.size_, &sema);
          !res) {
        GALOIS_LOG_ERROR("S3PutSingleAsync batch bad return {}", res.error());
      }
      i++;
    }
    for (auto& sema : semas) {
      tsuba::internal::S3PutSingleAsyncFinish(&sema);
    }
    results.push_back(timespec_to_us(timespec_sub(now(), start)));
  }
  return results;
}

void
CheckFile(const std::string& bucket, const std::string& object, uint64_t size) {
  // Confirm that the data we need is present
  std::string url = MkS3URL(bucket, object);
  tsuba::StatBuf sbuf;

  if (auto res = tsuba::FileStat(url, &sbuf); !res) {
    GALOIS_LOG_ERROR(
        "tsuba::FileStat({}) returned {}. Did you remember to run the "
        "appropriate write benchmark before this read benchmark?",
        url, sbuf.size);
  }
  if (sbuf.size != size) {
    GALOIS_LOG_ERROR(
        "{} is of size {}, expected {}. Did you remember to run the "
        "appropriate write benchmark before this read benchmark?",
        url, sbuf.size, size);
  }
}

/* These next two benchmarks rely on previous writes. Make sure to call them
 * after at least one write benchmark
 */
std::vector<int64_t>
test_s3_async_get_one(const Experiment& exp) {
  std::vector<std::string> objects;
  std::vector<int64_t> results;
  std::vector<uint8_t> read_buffer(exp.size_);
  uint8_t* rbuf = read_buffer.data();

  for (auto i = 0; i < exp.batch_; ++i) {
    std::string obj = MkS3obj(i, 4);
    objects.push_back(obj);
    // Confirm that the data we need is present
    CheckFile(s3bucket, obj, exp.size_);
  }

  struct timespec start;
  for (auto j = 0; j < exp.numTransfers_; ++j) {
    memset(rbuf, 0, exp.size_);
    start = now();
    for (const auto& object : objects) {
      tsuba::internal::CountingSemaphore sema;
      if (auto res = tsuba::internal::S3GetMultiAsync(
              s3bucket, object, 0, exp.size_, rbuf, &sema);
          !res) {
        GALOIS_LOG_ERROR("S3GetMultiAsync return {}", res.error());
      }
      // Only 1 outstanding load at a time
      tsuba::internal::S3GetMultiAsyncFinish(&sema);
    }
    results.push_back(timespec_to_us(timespec_sub(now(), start)));
    GALOIS_LOG_ASSERT(!memcmp(rbuf, exp.buffer_.data(), exp.size_));
  }
  return results;
}

std::vector<int64_t>
test_s3_async_get_batch(const Experiment& exp) {
  std::vector<std::string> objects;
  std::list<tsuba::internal::CountingSemaphore> semas;
  std::vector<int64_t> results;
  std::vector<uint8_t> read_buffer(exp.size_);
  uint8_t* rbuf = read_buffer.data();

  for (auto i = 0; i < exp.batch_; ++i) {
    objects.push_back(MkS3obj(i, 4));
    semas.emplace_back();
  }

  struct timespec start;
  for (auto j = 0; j < exp.numTransfers_; ++j) {
    memset(rbuf, 0, exp.size_);
    start = now();
    int i = 0;
    for (auto& sema : semas) {
      if (auto res = tsuba::internal::S3GetMultiAsync(
              s3bucket, objects[i], 0, exp.size_, rbuf, &sema);
          !res) {
        GALOIS_LOG_ERROR("S3GetMultiAsync batch bad return {}", res.error());
      }
      i++;
    }
    for (auto& sema : semas) {
      tsuba::internal::S3GetMultiAsyncFinish(&sema);
    }
    results.push_back(timespec_to_us(timespec_sub(now(), start)));
    GALOIS_LOG_ASSERT(!memcmp(rbuf, exp.buffer_.data(), exp.size_));
  }
  return results;
}

std::vector<int64_t>
test_s3_multi_async_batch(const Experiment& exp) {
  std::vector<std::string> objects;
  std::vector<tsuba::internal::PutMultiHandle> pmhs;
  pmhs.reserve(exp.batch_);
  std::vector<int64_t> results;
  for (auto i = 0; i < exp.batch_; ++i) {
    objects.push_back(MkS3obj(i, 4));
  }

  struct timespec start;
  for (auto j = 0; j < exp.numTransfers_; ++j) {
    start = now();
    for (int i = 0; i < exp.batch_; ++i) {
      pmhs[i] = tsuba::internal::S3PutMultiAsync1(
          s3bucket, objects[i], exp.buffer_.data(), exp.size_);
    }
    for (int i = 0; i < exp.batch_; ++i) {
      if (auto res =
              tsuba::internal::S3PutMultiAsync2(s3bucket, objects[i], pmhs[i]);
          !res) {
        GALOIS_LOG_ERROR("S3PutMultiAsync2 bad return {}", res.error());
      }
    }
    for (int i = 0; i < exp.batch_; ++i) {
      if (auto res =
              tsuba::internal::S3PutMultiAsync3(s3bucket, objects[i], pmhs[i]);
          !res) {
        GALOIS_LOG_ERROR("S3PutMultiAsync3 bad return {}", res.error());
      }
    }
    for (int i = 0; i < exp.batch_; ++i) {
      if (auto res = tsuba::internal::S3PutMultiAsyncFinish(
              s3bucket, objects[i], pmhs[i]);
          !res) {
        GALOIS_LOG_ERROR("S3PutMultiAsyncFinish bad return {}", res.error());
      }
    }
    results.push_back(timespec_to_us(timespec_sub(now(), start)));
  }
  return results;
}

/******************************************************************************/
/* Main */

struct Test {
  std::string name_;
  std::function<std::vector<int64_t>(const Experiment&)> func_;
  Test(
      const std::string& name,
      std::function<std::vector<int64_t>(const Experiment&)> func)
      : name_(name), func_(func) {}
};
std::vector<Test> tests = {
    Test("memfd_create", test_mem),
    Test("/tmp create", test_tmp),
    // Tracks Sync one, but slightly faster
    Test("S3 Put ASync One", test_s3_async_one),
    // Slowest
    Test("S3 Put Sync", test_s3_sync),
    Test("S3 Put Single Async Batch", test_s3_single_async_batch),
    // The next two need to follow at least one S3 write benchmark
    Test("S3 Get ASync One", test_s3_async_get_one),
    Test("S3 Get Async Batch", test_s3_async_get_batch),
    Test("S3 Put Multi Async Batch", test_s3_multi_async_batch),
};

int
main() {
  if (auto init_good = tsuba::Init(); !init_good) {
    GALOIS_LOG_FATAL("tsuba::Init: {}", init_good.error());
  }
  std::vector<Experiment> experiments{
      Experiment("  19B", 19), Experiment(" 10MB", 10 * (UINT64_C(1) << 20)),
      Experiment("100MB", 100 * (UINT64_C(1) << 20)),
      Experiment("500MB", 500 * (UINT64_C(1) << 20))
      // Trend for large files is clear at 500MB
      // Experiment("  1GB", UINT64_C(1) << 30)
  };

  fmt::print("*** VM and bucket same region\n");
  for (const auto& exp : experiments) {
    fmt::print("** size {}\n", exp.name_);

    for (const auto& test : tests) {
      std::vector<int64_t> results = test.func_(exp);
      fmt::print(
          "{:<25} ({}) {}\n", test.name_, exp.batch_,
          FmtResults(results, exp.size_));
    }
  }

  return 0;
}
