#include <iostream>
#include <stdexcept>

#include "cvhub/app/application.hpp"
#include "cvhub/app/sample_graph.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    auto services = cvhub::app::createApplication(
        {.logFile = "cvhub-tests.log", .workerCount = 2});
    const auto nodes = services->nodeCatalog()->list();
    require(!nodes.empty(), "node catalog should not be empty");
    require(services->nodeCatalog()->descriptor("opencv.resize").id ==
                "opencv.resize",
            "opencv.resize should be registered");

    const auto result = services->pipelineExecutor()->run(
        cvhub::app::createOpenCVSampleGraph());
    require(result.ok, result.message.c_str());
    const auto blurIt = result.outputs.find("blur");
    require(blurIt != result.outputs.end(), "blur output should exist");
    const auto imageIt = blurIt->second.find("image");
    require(imageIt != blurIt->second.end(), "blur image output should exist");
    const auto image = cvhub::valueCast<cvhub::ImageValue>(imageIt->second);
    require(image != nullptr, "blur output should be an ImageValue");
    require(image->width() == 320, "final image width should be 320");
    require(image->height() == 180, "final image height should be 180");
    require(image->channels() == 3, "final image should be 3-channel BGR");
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }

  return 0;
}
