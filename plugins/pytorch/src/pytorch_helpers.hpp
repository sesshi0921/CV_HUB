#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "cvhub/core/types.hpp"
#include "cvhub/core/values.hpp"
#include "cvhub/runtime/python_runtime.hpp"

namespace cvhub::plugins::pytorch {

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
  const auto bytes = img.bytes();
  req.imageBytes.assign(
      reinterpret_cast<const std::byte*>(bytes.data()),
      reinterpret_cast<const std::byte*>(bytes.data()) + bytes.size());
  return req;
}

inline std::shared_ptr<ImageValue> responseToImage(
    const IPythonRuntime::Response& resp) {
  if (!resp.ok) throw std::runtime_error("Python worker error: " + resp.error);
  return std::make_shared<ImageValue>(resp.w, resp.h, resp.c, resp.w * resp.c,
                                      resp.fmt, resp.imageBytes);
}

}  // namespace cvhub::plugins::pytorch
