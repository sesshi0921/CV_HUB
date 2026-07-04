#pragma once

#include "cvhub/core/node.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace cvhub {

struct GraphNode {
    NodeInstanceId instanceId;
    NodeId nodeId;
    ParameterMap parameters;
};

struct GraphEdge {
    NodeInstanceId fromNode;
    PortId fromPort;
    NodeInstanceId toNode;
    PortId toPort;
};

struct PipelineGraph {
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
};

struct PipelineRunResult {
    bool ok{false};
    std::string message;
    std::unordered_map<NodeInstanceId, std::unordered_map<PortId, ValuePtr>> outputs;
};

class IPipelineExecutor {
  public:
    virtual ~IPipelineExecutor() = default;
    virtual PipelineRunResult run(const PipelineGraph& graph) = 0;
};

} // namespace cvhub
