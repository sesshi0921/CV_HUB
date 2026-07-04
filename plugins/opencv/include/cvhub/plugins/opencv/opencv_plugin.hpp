#pragma once

#include "cvhub/runtime/service_container.hpp"

namespace cvhub::plugins::opencv {

void registerOpenCVPlugin(ServiceContainer& services);

} // namespace cvhub::plugins::opencv
