#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

#include "cvhub/core/types.hpp"

namespace cvhub {

class IPythonRuntime {
 public:
  struct Request {
    std::string fn;
    ParameterMap params;
    int w{}, h{}, c{};
    PixelFormat fmt{};
    std::vector<std::byte> imageBytes;
  };

  struct Response {
    bool ok{false};
    std::string error;
    int w{}, h{}, c{};
    PixelFormat fmt{};
    std::vector<std::byte> imageBytes;
  };

  virtual ~IPythonRuntime() = default;
  virtual Response call(const Request& req) = 0;
  virtual void shutdown() = 0;
};

class PythonSubprocessRuntime final : public IPythonRuntime {
 public:
  PythonSubprocessRuntime(std::string pythonPath, std::string workerPath);
  ~PythonSubprocessRuntime() override;

  Response call(const Request& req) override;
  void shutdown() override;

 private:
  void ensureStarted();

  std::mutex mutex_;
  std::string pythonPath_;
  std::string workerPath_;
  int toWorkerFd_{-1};
  int fromWorkerFd_{-1};
  int workerPid_{-1};
  bool started_{false};
};

}  // namespace cvhub
