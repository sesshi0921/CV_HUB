#include "cvhub/runtime/pipeline_executor.hpp"

#include <algorithm>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <stdexcept>
#include <unordered_set>

namespace cvhub {

namespace {

const PortDescriptor* findPort(const std::vector<PortDescriptor>& ports, const PortId& id) {
    const auto it =
        std::ranges::find_if(ports, [&](const PortDescriptor& port) { return port.id == id; });
    return it == ports.end() ? nullptr : &*it;
}

std::map<NodeInstanceId, GraphNode> indexNodes(const PipelineGraph& graph) {
    std::map<NodeInstanceId, GraphNode> nodes;
    for (const auto& node : graph.nodes) {
        if (node.instanceId.empty()) {
            throw std::runtime_error("Graph contains a node with an empty instance id");
        }
        if (!nodes.emplace(node.instanceId, node).second) {
            throw std::runtime_error("Duplicate node instance id: " + node.instanceId);
        }
    }
    return nodes;
}

} // namespace

PipelineExecutor::PipelineExecutor(std::shared_ptr<INodeCatalog> catalog,
                                   std::shared_ptr<ILogger> logger, int workerCount)
    : catalog_(std::move(catalog)), logger_(std::move(logger)),
      workerCount_(std::max(1, workerCount)) {}

PipelineRunResult PipelineExecutor::run(const PipelineGraph& graph) {
    ScopedLogger log(logger_, "runtime.pipeline");
    PipelineRunResult result;

    try {
        const auto nodeIndex = indexNodes(graph);
        std::map<NodeInstanceId, NodeDescriptor> descriptors;
        for (const auto& [instanceId, node] : nodeIndex) {
            descriptors.emplace(instanceId, catalog_->descriptor(node.nodeId));
        }

        std::map<NodeInstanceId, std::vector<GraphEdge>> incoming;
        std::map<NodeInstanceId, std::vector<GraphEdge>> outgoing;
        std::map<NodeInstanceId, int> indegree;
        for (const auto& [instanceId, _] : nodeIndex) {
            indegree[instanceId] = 0;
        }

        for (const auto& edge : graph.edges) {
            if (!nodeIndex.contains(edge.fromNode)) {
                throw std::runtime_error("Edge references unknown source node: " + edge.fromNode);
            }
            if (!nodeIndex.contains(edge.toNode)) {
                throw std::runtime_error("Edge references unknown target node: " + edge.toNode);
            }

            const auto& fromDescriptor = descriptors.at(edge.fromNode);
            const auto& toDescriptor = descriptors.at(edge.toNode);
            const auto* fromPort = findPort(fromDescriptor.outputs, edge.fromPort);
            const auto* toPort = findPort(toDescriptor.inputs, edge.toPort);
            if (!fromPort) {
                throw std::runtime_error("Edge references unknown output port: " + edge.fromNode +
                                         "." + edge.fromPort);
            }
            if (!toPort) {
                throw std::runtime_error("Edge references unknown input port: " + edge.toNode +
                                         "." + edge.toPort);
            }
            if (fromPort->type != toPort->type) {
                throw std::runtime_error("Edge port type mismatch: " + edge.fromNode + "." +
                                         edge.fromPort + " -> " + edge.toNode + "." + edge.toPort);
            }

            incoming[edge.toNode].push_back(edge);
            outgoing[edge.fromNode].push_back(edge);
            ++indegree[edge.toNode];
        }

        for (const auto& [instanceId, descriptor] : descriptors) {
            for (const auto& input : descriptor.inputs) {
                const bool connected =
                    std::ranges::any_of(incoming[instanceId], [&](const GraphEdge& edge) {
                        return edge.toPort == input.id;
                    });
                if (input.required && !connected) {
                    throw std::runtime_error("Required input is not connected: " + instanceId +
                                             "." + input.id);
                }
            }
        }

        std::queue<NodeInstanceId> ready;
        for (const auto& [instanceId, degree] : indegree) {
            if (degree == 0) {
                ready.push(instanceId);
            }
        }

        std::mutex outputMutex;
        std::map<NodeInstanceId, std::unordered_map<PortId, ValuePtr>> outputs;
        int visited = 0;

        while (!ready.empty()) {
            std::vector<NodeInstanceId> batch;
            while (!ready.empty() && static_cast<int>(batch.size()) < workerCount_) {
                batch.push_back(ready.front());
                ready.pop();
            }

            std::vector<std::future<void>> futures;
            futures.reserve(batch.size());
            for (const auto& instanceId : batch) {
                futures.push_back(std::async(std::launch::async, [&, instanceId] {
                    const auto& graphNode = nodeIndex.at(instanceId);
                    auto node = catalog_->create(graphNode.nodeId);
                    NodeExecutionContext context;
                    context.parameters = graphNode.parameters;

                    {
                        std::scoped_lock lock(outputMutex);
                        for (const auto& edge : incoming[instanceId]) {
                            context.inputs.emplace(edge.toPort,
                                                   outputs.at(edge.fromNode).at(edge.fromPort));
                        }
                    }

                    node->execute(context);

                    std::unordered_map<PortId, ValuePtr> nodeOutputs;
                    for (auto& [portId, value] : context.outputs) {
                        nodeOutputs.emplace(portId, std::move(value));
                    }

                    std::scoped_lock lock(outputMutex);
                    outputs[instanceId] = std::move(nodeOutputs);
                }));
            }

            for (auto& future : futures) {
                future.get();
            }

            for (const auto& instanceId : batch) {
                ++visited;
                for (const auto& edge : outgoing[instanceId]) {
                    --indegree[edge.toNode];
                    if (indegree[edge.toNode] == 0) {
                        ready.push(edge.toNode);
                    }
                }
            }
        }

        if (visited != static_cast<int>(graph.nodes.size())) {
            throw std::runtime_error("Graph contains a cycle");
        }

        result.ok = true;
        result.message = "pipeline completed";
        for (auto& [nodeId, nodeOutputs] : outputs) {
            result.outputs.emplace(nodeId, std::move(nodeOutputs));
        }
        log.info("Pipeline completed");
        return result;
    } catch (const std::exception& ex) {
        result.ok = false;
        result.message = ex.what();
        log.error(result.message);
        return result;
    }
}

} // namespace cvhub
