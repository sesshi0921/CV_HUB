#include "cvhub/plugins/opencv/opencv_plugin.hpp"

#include <string>
#include <unordered_map>
#include <vector>

#include "cvhub/runtime/function_descriptor.hpp"
#include "cvhub/runtime/generic_node.hpp"
#include "opencv_helpers.hpp"

namespace cvhub::plugins::opencv {

// Declarations from the build-time-generated file (opencv_auto_nodes.cpp).
std::vector<FunctionDescriptor> getAutoOpenCVDescriptors();
std::unordered_map<std::string, FactoryFn> getAutoOpenCVRegistry();

// ---------------------------------------------------------------------------
// Registration — auto-generated transform nodes only.
// Source and sink nodes (cvhub-namespaced) live in plugins/common.
// ---------------------------------------------------------------------------

void registerOpenCVPlugin(ServiceContainer& services) {
  auto kRegistry = getAutoOpenCVRegistry();

  auto allDescriptors = getAutoOpenCVDescriptors();

  const auto descriptors =
      services.functionNodeGenerator()->generate(allDescriptors);
  for (const auto& descriptor : descriptors) {
    const auto it = kRegistry.find(descriptor.factoryKey);
    if (it != kRegistry.end()) {
      services.nodeCatalog()->registerNode(descriptor, it->second(descriptor));
    }
  }
}

}  // namespace cvhub::plugins::opencv
