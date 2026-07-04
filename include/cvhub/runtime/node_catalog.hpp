#pragma once

#include "cvhub/core/node.hpp"

#include <map>
#include <mutex>

namespace cvhub {

class NodeCatalog final : public INodeCatalog {
  public:
    void registerNode(NodeDescriptor descriptor,
                      std::shared_ptr<const INodeFactory> factory) override;
    const NodeDescriptor& descriptor(const NodeId& nodeId) const override;
    std::unique_ptr<INode> create(const NodeId& nodeId) const override;
    std::vector<NodeDescriptor> list() const override;

  private:
    struct Entry {
        NodeDescriptor descriptor;
        std::shared_ptr<const INodeFactory> factory;
    };

    mutable std::mutex mutex_;
    std::map<NodeId, Entry> entries_;
};

} // namespace cvhub
