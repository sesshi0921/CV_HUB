#pragma once

#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cvhub/core/node.hpp"
#include "cvhub/core/types.hpp"
#include "cvhub/core/values.hpp"
#include "cvhub/runtime/generic_node.hpp"
#include "cvhub/runtime/python_runtime.hpp"

namespace cvhub::plugins::pytorch {

using FactoryFn = std::function<std::shared_ptr<INodeFactory>(NodeDescriptor)>;

inline std::shared_ptr<IPythonRuntime>& pythonRuntimeSlot() {
  static std::shared_ptr<IPythonRuntime> runtime;
  return runtime;
}

inline void setPythonRuntime(std::shared_ptr<IPythonRuntime> runtime) {
  pythonRuntimeSlot() = std::move(runtime);
}

inline std::shared_ptr<IPythonRuntime> requirePythonRuntime() {
  auto runtime = pythonRuntimeSlot();
  if (!runtime) {
    throw std::logic_error("PyTorch plugin runtime is not initialized");
  }
  return runtime;
}

inline IPythonRuntime::Request imageToRequest(const std::string& fn,
                                              const ImageValue& img,
                                              const ParameterMap& params) {
  IPythonRuntime::Request req;
  req.fn = fn;
  req.params = params;
  req.w = img.width();
  req.h = img.height();
  req.c = img.channels();
  req.fmt = img.pixelFormat();
  const auto rowBytes =
      static_cast<std::size_t>(img.width()) * img.channels();
  const auto bytes = img.bytes();
  req.imageBytes.resize(rowBytes * img.height());
  for (int row = 0; row < img.height(); ++row) {
    std::memcpy(req.imageBytes.data() + static_cast<std::size_t>(row) * rowBytes,
                bytes.data() +
                    static_cast<std::size_t>(row) * img.strideBytes(),
                rowBytes);
  }
  return req;
}

inline std::shared_ptr<ImageValue> responseToImage(
    const IPythonRuntime::Response& resp) {
  if (!resp.ok) throw std::runtime_error("Python worker error: " + resp.error);
  return std::make_shared<ImageValue>(resp.w, resp.h, resp.c, resp.w * resp.c,
                                      resp.fmt, resp.imageBytes);
}

inline std::shared_ptr<INodeFactory> makePythonTransformFactory(
    NodeDescriptor descriptor, std::string fnName) {
  auto runtime = requirePythonRuntime();
  ImageTransformFn fn =
      [runtime, fnName = std::move(fnName)](
          std::shared_ptr<const ImageValue> input,
          const ParameterMap& params) -> std::shared_ptr<ImageValue> {
    return responseToImage(
        runtime->call(imageToRequest(fnName, *input, params)));
  };
  return std::make_shared<GenericImageTransformFactory>(std::move(descriptor),
                                                        std::move(fn));
}

}  // namespace cvhub::plugins::pytorch
