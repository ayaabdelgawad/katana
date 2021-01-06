#include "galois/OpLog.h"

#include "galois/Logging.h"

galois::Operation
galois::OpLog::GetOp(uint64_t index) const {
  if (index >= log_.size()) {
    GALOIS_LOG_WARN(
        "Log index {} >= {}, which is log size", index, log_.size());
  }
  return log_[index];
}

uint64_t
galois::OpLog::AppendOp(const Operation& op) {
  auto sz = log_.size();
  log_.emplace_back(op);
  return sz;
}

uint64_t
galois::OpLog::size() const {
  return log_.size();
}

void
galois::OpLog::Clear() {
  log_.clear();
}

galois::OpLog::OpLog(const galois::Uri& uri) {
  GALOIS_LOG_FATAL("Persistent log not yet implemented: {}", uri);
}

// Process the log into a GraphUpdate object
std::pair<std::unique_ptr<galois::GraphModify>,std::unique_ptr<galois::GraphUpdate>>
galois::CreateGraphModifyUpdate(const graphs::PropertyFileGraph* g, const OpLog& log) {
  std::unique_ptr<GraphModify> gmd = std::make_unique<galois::GraphModify>();
  GALOIS_LOG_ASSERT(gmd->empty());
  std::unique_ptr<GraphUpdate> gup = std::make_unique<galois::GraphUpdate>(
         g->topology().num_nodes(), g->topology().num_edges());
  GALOIS_LOG_ASSERT(gup->empty());
  std::unordered_map<std::string, uint32_t> nprop_idx;
  std::unordered_map<std::string, uint32_t> eprop_idx;
  for (uint64_t i = 0; i < log.size(); ++i) {
    auto op = log.GetOp(i);
    switch (op.opcode()) {
    case galois::Op::kNodeAdd: {
      GALOIS_LOG_FATAL("Opcode not supported {}", op.opcode());
      break;
    }
    case galois::Op::kNodeDel: {
      GALOIS_LOG_FATAL("Opcode not supported {}", op.opcode());
      break;
    }
    case galois::Op::kEdgeAdd: {
      GALOIS_LOG_FATAL("Opcode not supported {}", op.opcode());
      break;
    }
    case galois::Op::kEdgeDel: {
      GALOIS_LOG_FATAL("Opcode not supported {}", op.opcode());
      break;
    }
    case galois::Op::kNodePropDel: {
      GALOIS_LOG_FATAL("Opcode not supported {}", op.opcode());
      break;
    }
    case galois::Op::kEdgePropDel: {
      GALOIS_LOG_FATAL("Opcode not supported {}", op.opcode());
      break;
    }
    case galois::Op::kNodePropVal: {
      auto name = op.s_val();
      if (nprop_idx.find(name) == nprop_idx.cend()) {
        GALOIS_LOG_DEBUG("Registering Node prop: {}", name);
        nprop_idx[name] = gup->RegisterNodeProp(name);
      }
      gup->SetNodeProp(nprop_idx[name], op.id1(), i);
      break;
    }
    case galois::Op::kEdgePropVal: {
      GALOIS_LOG_FATAL("Opcode not supported {}", op.opcode());
      break;
    }
    default: {
      GALOIS_LOG_FATAL("Opcode not supported {}", op.opcode());
    }
    }
  }
  return std::make_pair(std::move(gmd), std::move(gup));
}
