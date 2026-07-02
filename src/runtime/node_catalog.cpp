#include "cvhub/runtime/node_catalog.hpp"

#include <stdexcept>

namespace cvhub {

void NodeCatalog::registerNode(NodeDescriptor descriptor, std::shared_ptr<const INodeFactory> factory)
{
    if (!factory) {
        throw std::invalid_argument("Cannot register node without a factory");
    }
    if (descriptor.id.empty()) {
        throw std::invalid_argument("Cannot register node with an empty id");
    }

    const auto nodeId = descriptor.id;
    std::scoped_lock lock(mutex_);
    const auto [_, inserted] = entries_.emplace(nodeId, Entry{std::move(descriptor), std::move(factory)});
    if (!inserted) {
        throw std::runtime_error("Duplicate node id registration: " + nodeId);
    }
}

const NodeDescriptor& NodeCatalog::descriptor(const NodeId& nodeId) const
{
    std::scoped_lock lock(mutex_);
    const auto it = entries_.find(nodeId);
    if (it == entries_.end()) {
        throw std::runtime_error("Unknown node id: " + nodeId);
    }
    return it->second.descriptor;
}

std::unique_ptr<INode> NodeCatalog::create(const NodeId& nodeId) const
{
    std::shared_ptr<const INodeFactory> factory;
    {
        std::scoped_lock lock(mutex_);
        const auto it = entries_.find(nodeId);
        if (it == entries_.end()) {
            throw std::runtime_error("Unknown node id: " + nodeId);
        }
        factory = it->second.factory;
    }
    return factory->create();
}

std::vector<NodeDescriptor> NodeCatalog::list() const
{
    std::scoped_lock lock(mutex_);
    std::vector<NodeDescriptor> nodes;
    nodes.reserve(entries_.size());
    for (const auto& [_, entry] : entries_) {
        nodes.push_back(entry.descriptor);
    }
    return nodes;
}

} // namespace cvhub
