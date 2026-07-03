#include "cvhub/runtime/python_runtime.hpp"

#include <stdexcept>
#include <utility>

namespace cvhub {

namespace {

constexpr const char* kUnsupportedMessage =
    "PythonSubprocessRuntime is only supported on UNIX platforms";

}  // namespace

PythonSubprocessRuntime::PythonSubprocessRuntime(std::string pythonPath,
                                                 std::string workerPath)
    : pythonPath_(std::move(pythonPath)), workerPath_(std::move(workerPath)) {}

PythonSubprocessRuntime::~PythonSubprocessRuntime() = default;

void PythonSubprocessRuntime::ensureStarted() {
  throw std::runtime_error(kUnsupportedMessage);
}

IPythonRuntime::Response PythonSubprocessRuntime::call(const Request&) {
  throw std::runtime_error(kUnsupportedMessage);
}

void PythonSubprocessRuntime::shutdown() {}

}  // namespace cvhub
