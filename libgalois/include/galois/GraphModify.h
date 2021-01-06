#ifndef GALOIS_LIBGALOIS_GALOIS_GRAPHMODIFY_H_
#define GALOIS_LIBGALOIS_GALOIS_GRAPHMODIFY_H_

namespace galois {

/// A GraphModify object is constructed from an OpLog and it contains pointers into
/// the log, represented as log indices.  It is constructed by playing the log to
/// resolve redundant or contradictory operations. The GraphModify object is mutable
/// unlike a graph.
///
/// Changes in the GraphModify object are applied to the graph by node communication.
///
/// The GraphModify object maintains pointers into the log, represented as log indices.
class GALOIS_EXPORT GraphModify {
public:
  GraphModify() = default;

  bool empty() const { return true; }
};
    
} // namespace galois

#endif
