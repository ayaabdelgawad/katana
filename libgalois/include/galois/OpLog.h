#ifndef GALOIS_LIBGALOIS_GALOIS_OPLOG_H_
#define GALOIS_LIBGALOIS_GALOIS_OPLOG_H_

#include <variant>

#include "galois/BuildGraph.h"
#include "galois/GraphModify.h"
#include "galois/GraphUpdate.h"
#include "galois/Uri.h"
#include "galois/graphs/PropertyFileGraph.h"

namespace galois {

// GlobalNodeID duplicated from libgluon/include/galois/graphs
using GlobalNodeID = uint64_t;
constexpr GlobalNodeID kUnknownHost = std::numeric_limits<GlobalNodeID>::max();
// A new (local) ID is either the nth created node, or has a unique string name
using NewNodeID = std::variant<int64_t, std::string>;

// https://neo4j.com/docs/cypher-manual/current/clauses/create/
enum class Op {
  kInvalid = 0,
  // New (local) nodes are implicitly named by their creation number, starting at 0.
  kNodeAdd,         // int64: number of new nodes to add
  kNodeAddID,       // string: identifier for new node
  kNodeDel,         // NewNodeID: new node
  kNodeDelExisting, // GlobalNodeID: delete

  kEdgeAdd,         // NewNodeID:    src        , NewNodeID:    dst
  kEdgeAddFrom,     // NewNodeID:    src        , GlobalNodeID: dst
  kEdgeAddTo,       // GlobalNodeID: src        , NewNodeID:    dst
  kEdgeAddExisting, // GlobalNodeID: src        , GlobalNodeID: dst

  kEdgeDel,         // NewNodeID:    src        , NewNodeID:    dst
  kEdgeDelFrom,     // NewNodeID:    src        , GlobalNodeID: dst
  kEdgeDelTo,       // GlobalNodeID: src        , NewNodeID:    dst
  kEdgeDelExisting, // GlobalNodeID: src        , GlobalNodeID: dst

  kNodePropDel,     // string: node prop name
  kEdgePropDel,     // string: edge prop name
  kNodePropVal,     // string: node prop name   , NewNodeID:    id   , ImportData: value
  kNodePropValExisting,// string: node prop name, GlobalNodeID: id   , ImportData: value
  kEdgePropVal,     // string: edge prop name   , NewNodeID:    id   , ImportData: value
  kEdgePropValExisting,// string: edge prop name, GlobalNodeID: id   , ImportData: value

  // Transactions do not nest
  kBeginTx,
  kAbortTx,
  kCommitTx,        // string: newRDG (optional)
};

class GALOIS_EXPORT Operation {
  Op opcode_{0};
  int64_t i_val_{UINT64_C(0)}; // I don't use LocalNodeID, because that is a 32-bit number
  std::string s_val_;
  using ID = std::variant<NewNodeID, GlobalNodeID>;
  ID id1_;
  ID id2_;
  galois::ImportData data_{galois::ImportDataType::kUnsupported, false};

public:
  Operation(Op opcode, int64_t i_val) 
      : opcode_(opcode), i_val_(i_val) {}
  Operation(Op opcode, const std::string& s_val) 
      : opcode_(opcode), s_val_(s_val) {}
  Operation(Op opcode, const ID& id)
      : opcode_(opcode), id1_(id) {}
  Operation(Op opcode, const ID& id1, const ID& id2)
      : opcode_(opcode), id1_(id1), id2_(id2) {}
  Operation(Op opcode, const std::string& s_val, const ID& id, galois::ImportData data)
      : opcode_(opcode), s_val_(s_val), id1_(id), data_(std::move(data)) {}

  Op          opcode() const { return opcode_; }
  int64_t     i_val()  const { return i_val_; }
  std::string s_val()  const { return s_val_; }  
  uint64_t id1() const {
     const auto *pval = std::get_if<GlobalNodeID>(&id1_);
     if (pval != NULL) {
       return *pval;
     } else {       
        const auto *new_id = std::get_if<NewNodeID>(&id1_);
        if (new_id != NULL) {
           const auto *new_int = std::get_if<int64_t>(new_id);
           if (new_int != NULL) {
             return *new_int;
           } else {
              const auto *new_str = std::get_if<std::string>(new_id);
              if (new_str != NULL) {
               return std::stoul(*new_str, nullptr, 0);
              } else {
                 return kUnknownHost;
              }
           }
        } else {
           return kUnknownHost;          
        }          
     }
  }     

  galois::ImportData data() const { return data_; }
};

class GALOIS_EXPORT OpLog {
  std::vector<Operation> log_;

public:
  OpLog() = default;
  /// Read/write operation log to URI
  OpLog(const galois::Uri& uri);
  /// Read an operation at the given index
  Operation GetOp(uint64_t idx) const;
  /// Write an operation, return the log offset that was written
  uint64_t AppendOp(const Operation& op);
  /// Get the number of log entries
  uint64_t size() const;
  /// Erase log contents
  void Clear();
};

/// Process the log into a GraphModify and GraphUpdate object
std::pair<std::unique_ptr<GraphModify>,std::unique_ptr<GraphUpdate>>
CreateGraphModifyUpdate(const galois::graphs::PropertyFileGraph* g, const OpLog& log);

}  // namespace galois
#endif
