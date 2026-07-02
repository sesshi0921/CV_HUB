#include "cvhub/runtime/service_container.hpp"

#include <stdexcept>

#include "cvhub/runtime/node_catalog.hpp"
#include "cvhub/runtime/pipeline_executor.hpp"

namespace cvhub {

namespace {

class ConsoleLogger final : public ILogger {
 public:
  void info(std::string_view, std::string_view) override {}
  void warn(std::string_view, std::string_view) override {}
  void error(std::string_view, std::string_view) override {}
};

}  // namespace

ServiceContainer::ServiceContainer()
    : nodeCatalog_(std::make_shared<NodeCatalog>()),
      functionNodeGenerator_(std::make_shared<FunctionNodeGenerator>()),
      logger_(std::make_shared<ConsoleLogger>()) {}

void ServiceContainer::setLogger(std::shared_ptr<ILogger> logger) {
  if (!logger) {
    throw std::invalid_argument("logger must not be null");
  }
  logger_ = std::move(logger);
}

void ServiceContainer::buildRuntime(int workerCount) {
  pipelineExecutor_ =
      std::make_shared<PipelineExecutor>(nodeCatalog_, logger_, workerCount);
}

}  // namespace cvhub
