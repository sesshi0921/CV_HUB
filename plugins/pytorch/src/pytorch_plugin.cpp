#include "cvhub/plugins/pytorch/pytorch_plugin.hpp"

#include <memory>

#include "cvhub/runtime/function_descriptor.hpp"
#include "cvhub/runtime/generic_node.hpp"
#include "cvhub/runtime/python_runtime.hpp"
#include "pytorch_helpers.hpp"

namespace cvhub::plugins::pytorch {

std::vector<FunctionDescriptor> getAutoDescriptors();
std::unordered_map<std::string, FactoryFn> getAutoRegistry();

void registerPyTorchPlugin(ServiceContainer& services, const std::string& pythonPath,
                           const std::string& workerPath) {
    auto runtime = std::make_shared<PythonSubprocessRuntime>(pythonPath, workerPath);
    setPythonRuntime(runtime);

    std::unordered_map<std::string, FactoryFn> kRegistry = {};
    auto autoRegistry = getAutoRegistry();
    for (auto& [key, factory] : autoRegistry) {
        kRegistry.emplace(key, std::move(factory));
    }

    const auto nodes = services.functionNodeGenerator()->generate(getAutoDescriptors());
    for (const auto& node : nodes) {
        const auto it = kRegistry.find(node.factoryKey);
        if (it != kRegistry.end()) {
            services.nodeCatalog()->registerNode(node, it->second(node));
        }
    }
}

} // namespace cvhub::plugins::pytorch
