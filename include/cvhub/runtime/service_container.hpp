#pragma once

#include "cvhub/core/logging.hpp"
#include "cvhub/core/node.hpp"
#include "cvhub/core/graph.hpp"
#include "cvhub/runtime/function_descriptor.hpp"

#include <memory>

namespace cvhub {

class ServiceContainer {
public:
    ServiceContainer();

    std::shared_ptr<INodeCatalog> nodeCatalog() const { return nodeCatalog_; }
    std::shared_ptr<IFunctionNodeGenerator> functionNodeGenerator() const { return functionNodeGenerator_; }
    std::shared_ptr<ILogger> logger() const { return logger_; }
    std::shared_ptr<IPipelineExecutor> pipelineExecutor() const { return pipelineExecutor_; }

    void setLogger(std::shared_ptr<ILogger> logger);
    void buildRuntime(int workerCount);

private:
    std::shared_ptr<INodeCatalog> nodeCatalog_;
    std::shared_ptr<IFunctionNodeGenerator> functionNodeGenerator_;
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<IPipelineExecutor> pipelineExecutor_;
};

} // namespace cvhub
