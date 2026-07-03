#include "cvhub/plugins/pytorch/pytorch_plugin.hpp"

#include <memory>

#include "cvhub/runtime/function_descriptor.hpp"
#include "cvhub/runtime/generic_node.hpp"
#include "cvhub/runtime/python_runtime.hpp"
#include "pytorch_helpers.hpp"

namespace cvhub::plugins::pytorch {

std::vector<FunctionDescriptor> getAutoDescriptors();

void registerPyTorchPlugin(ServiceContainer& services,
                           const std::string& pythonPath,
                           const std::string& workerPath) {
  auto runtime =
      std::make_shared<PythonSubprocessRuntime>(pythonPath, workerPath);

  const auto nodes =
      services.functionNodeGenerator()->generate(getAutoDescriptors());
  for (const auto& node : nodes) {
    const std::string fnName = node.functionName;
    ImageTransformFn fn =
        [runtime, fnName](
            std::shared_ptr<const ImageValue> input,
            const ParameterMap& params) -> std::shared_ptr<ImageValue> {
      return responseToImage(
          runtime->call(imageToRequest(fnName, *input, params)));
    };
    services.nodeCatalog()->registerNode(
        node,
        std::make_shared<GenericImageTransformFactory>(node, std::move(fn)));
  }
}

}  // namespace cvhub::plugins::pytorch
