#pragma once

#include "cvhub/core/graph.hpp"
#include "cvhub/core/logging.hpp"

#include <memory>

namespace cvhub {

class PipelineExecutor final : public IPipelineExecutor {
  public:
    PipelineExecutor(std::shared_ptr<INodeCatalog> catalog, std::shared_ptr<ILogger> logger,
                     int workerCount);

    PipelineRunResult run(const PipelineGraph& graph) override;

  private:
    std::shared_ptr<INodeCatalog> catalog_;
    std::shared_ptr<ILogger> logger_;
    int workerCount_{};
};

} // namespace cvhub
