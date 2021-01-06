#ifndef GALOIS_LIBGALOIS_GALOIS_GRAPHUPDATE_H_
#define GALOIS_LIBGALOIS_GALOIS_GRAPHUPDATE_H_

namespace galois {

/// A GraphUpdate object is constructed from an OpLog and it contains pointers into
/// the log, represented as log indices.  It is constructed by playing the log to
/// resolve redundant or contradictory operations.  The GraphUpdate object is mutable
/// unlike a graph.
///
/// Changes in the GraphUpdate object are applied to the graph by repartitioning.
///
/// The GraphModify object 
/// A graph update object is constructed from a log to represent the graph state
/// obtained by playing the log.  If the log contains redundant or contradictory
/// operations, these are resolved as the operations are played into the GraphUpdate
/// object (which is mutable, unlike a graph).
///
/// The GraphUpdate object maintains pointers into the log, represented as log indices.
///
/// The ingest process takes a GraphUpdate object and its log and merges it into an existing
/// graph.
class GALOIS_EXPORT GraphUpdate {
  // A vector of node and edge property updates, one per property
  // Each property update has an entry for each local node/edge.
  // Each update is an index into an OpLog
  std::vector<std::vector<uint64_t>> nprop_;
  std::vector<std::string> nprop_names_;
  std::vector<std::vector<uint64_t>> eprop_;
  std::vector<std::string> eprop_names_;
  uint64_t num_nodes_;
  uint64_t num_edges_;

  uint32_t RegisterProp(
      const std::string& name, uint64_t num,
      std::vector<std::vector<uint64_t>>& prop,
      std::vector<std::string>& names) {
    uint32_t index = names.size();
    names.emplace_back(name);
    std::vector<uint64_t> initial(num);
    GALOIS_LOG_ASSERT((uint64_t)index == prop.size());
    prop.emplace_back(initial);
    return index;
  }
  /// Set the value of a property
  void SetProp(
      uint32_t pnum, uint64_t index, uint64_t op_log_index,
      std::vector<std::vector<uint64_t>>& prop) {
    if (pnum >= prop.size()) {
      GALOIS_LOG_DEBUG(
          "Property number {} is out of bounds ({})", pnum, prop.size());
      return;
    }
    if (index >= prop[pnum].size()) {
      GALOIS_LOG_DEBUG(
          "Property index {} is out of bounds ({})", index, prop[pnum].size());
      return;
    }
    prop[pnum][index] = op_log_index;
  }

public:
  GraphUpdate(uint64_t num_nodes, uint64_t num_edges)
      : num_nodes_(num_nodes), num_edges_(num_edges) {}

  uint32_t num_nprop() const { return nprop_.size(); }
  uint32_t num_eprop() const { return eprop_.size(); }

  bool empty() const { 
     return nprop_.empty()
        && nprop_names_.empty()
        && eprop_.empty()
        && eprop_names_.empty();
  }

  /// When a new node/edge property is added, call these functions to register it and get back
  /// the property index.
  uint32_t RegisterNodeProp(const std::string& name) {
    return RegisterProp(name, num_nodes_, nprop_, nprop_names_);
  }
  uint32_t RegisterEdgeProp(const std::string& name) {
    return RegisterProp(name, num_edges_, eprop_, eprop_names_);
  }
  std::string GetNodePropName(uint32_t pnum) {
    if (pnum >= nprop_names_.size()) {
      GALOIS_LOG_DEBUG(
          "Property number {} is out of bounds ({})", pnum,
          nprop_names_.size());
      return "";
    }
    return nprop_names_[pnum];
  }
  std::vector<uint64_t> GetNodePropIndices(uint32_t pnum) {
    if (pnum >= nprop_names_.size()) {
      GALOIS_LOG_DEBUG(
          "Property number {} is out of bounds ({})", pnum,
          nprop_names_.size());
      return {};
    }
    return nprop_[pnum];
  }
  std::string GetEdgePropName(uint32_t pnum) {
    if (pnum >= eprop_names_.size()) {
      GALOIS_LOG_DEBUG(
          "Property number {} is out of bounds ({})", pnum,
          eprop_names_.size());
      return "";
    }
    return eprop_names_[pnum];
  }
  std::vector<uint64_t> GetEdgePropIndices(uint32_t pnum) {
    if (pnum >= eprop_names_.size()) {
      GALOIS_LOG_DEBUG(
          "Property number {} is out of bounds ({})", pnum,
          eprop_names_.size());
      return {};
    }
    return eprop_[pnum];
  }

  void SetNodeProp(uint32_t pnum, uint64_t index, uint64_t op_log_index) {
    SetProp(pnum, index, op_log_index, nprop_);
  }
  void SetEdgeProp(uint32_t pnum, uint64_t index, uint64_t op_log_index) {
    SetProp(pnum, index, op_log_index, eprop_);
  }
};
    
} // namespace galois

#endif
