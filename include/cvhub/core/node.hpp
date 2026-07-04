#pragma once

#include "cvhub/core/types.hpp"
#include "cvhub/core/values.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace cvhub {

struct NodeExecutionContext {
    std::unordered_map<PortId, ValuePtr> inputs;
    std::unordered_map<PortId, MutableValuePtr> outputs;
    ParameterMap parameters;
};

class INode {
  public:
    virtual ~INode() = default;
    virtual const NodeDescriptor& descriptor() const = 0;
    virtual void execute(NodeExecutionContext& context) = 0;
};

class INodeFactory {
  public:
    virtual ~INodeFactory() = default;
    virtual std::unique_ptr<INode> create() const = 0;
};

class INodeCatalog {
  public:
    virtual ~INodeCatalog() = default;
    virtual void registerNode(NodeDescriptor descriptor,
                              std::shared_ptr<const INodeFactory> factory) = 0;
    virtual const NodeDescriptor& descriptor(const NodeId& nodeId) const = 0;
    virtual std::unique_ptr<INode> create(const NodeId& nodeId) const = 0;
    virtual std::vector<NodeDescriptor> list() const = 0;
};

} // namespace cvhub
