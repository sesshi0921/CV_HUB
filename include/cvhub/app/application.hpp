#pragma once

#include "cvhub/runtime/service_container.hpp"

#include <memory>
#include <string>
#include <vector>

namespace cvhub::app {

struct ApplicationOptions {
    std::string logFile{"cvhub.log"};
    int workerCount{4};
};

std::shared_ptr<ServiceContainer> createApplication(const ApplicationOptions& options);
int runCli(int argc, char** argv);

} // namespace cvhub::app
