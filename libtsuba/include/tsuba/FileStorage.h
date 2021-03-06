#ifndef GALOIS_LIBTSUBA_TSUBA_FILESTORAGE_H_
#define GALOIS_LIBTSUBA_TSUBA_FILESTORAGE_H_

#include <cstdint>
#include <future>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "galois/Result.h"
#include "galois/config.h"

namespace tsuba {

struct StatBuf;

class GALOIS_EXPORT FileStorage {
  std::string uri_scheme_;

protected:
  FileStorage(std::string_view uri_scheme) : uri_scheme_(uri_scheme) {}

public:
  FileStorage(const FileStorage& no_copy) = delete;
  FileStorage(FileStorage&& no_move) = delete;
  FileStorage& operator=(const FileStorage& no_copy) = delete;
  FileStorage& operator=(FileStorage&& no_move) = delete;
  virtual ~FileStorage() = default;

  std::string_view uri_scheme() const { return uri_scheme_; }
  virtual galois::Result<void> Init() = 0;
  virtual galois::Result<void> Fini() = 0;
  virtual galois::Result<void> Stat(const std::string& uri, StatBuf* size) = 0;

  virtual galois::Result<void> GetMultiSync(
      const std::string& uri, uint64_t start, uint64_t size,
      uint8_t* result_buf) = 0;

  virtual galois::Result<void> PutMultiSync(
      const std::string& uri, const uint8_t* data, uint64_t size) = 0;

  /// Storage classes with higher priority will be tried by GlobalState earlier
  /// currently only used to enforce local fs default; GlobalState defaults
  /// to the LocalStorage when no protocol on the URI is provided
  virtual uint32_t Priority() const { return 0; }

  // get on future can potentially block (bulk synchronous parallel)
  virtual std::future<galois::Result<void>> PutAsync(
      const std::string& uri, const uint8_t* data, uint64_t size) = 0;
  virtual std::future<galois::Result<void>> GetAsync(
      const std::string& uri, uint64_t start, uint64_t size,
      uint8_t* result_buf) = 0;
  virtual std::future<galois::Result<void>> ListAsync(
      const std::string& directory, std::vector<std::string>* list,
      std::vector<uint64_t>* size) = 0;
  virtual galois::Result<void> Delete(
      const std::string& directory,
      const std::unordered_set<std::string>& files) = 0;
};

/// RegisterFileStorage adds a file storage backend to the tsuba library. File
/// storage backends must be registered before tsuba::Init. Backends need to be
/// registered for each tsuba::Init call.
GALOIS_EXPORT void RegisterFileStorage(FileStorage* fs);

}  // namespace tsuba

#endif
