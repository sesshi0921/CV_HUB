#pragma once

#include <string>

#include "cvhub/runtime/service_container.hpp"

namespace cvhub::plugins::pytorch {

void registerPyTorchPlugin(ServiceContainer& services, const std::string& pythonPath,
                           const std::string& workerPath);

} // namespace cvhub::plugins::pytorch
