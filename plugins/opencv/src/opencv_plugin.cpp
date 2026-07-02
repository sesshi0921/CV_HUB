#include "cvhub/plugins/opencv/opencv_plugin.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>
#include <opencv2/videoio.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cvhub/runtime/function_descriptor.hpp"
#include "cvhub/runtime/generic_node.hpp"
#include "opencv_helpers.hpp"

namespace cvhub::plugins::opencv {

// Declarations from the build-time-generated file (opencv_auto_nodes.cpp).
std::vector<FunctionDescriptor> getAutoOpenCVDescriptors();
std::unordered_map<std::string, FactoryFn> getAutoOpenCVRegistry();

namespace {

// ---------------------------------------------------------------------------
// Introspector — source & stateful nodes only; transforms come from generated
// ---------------------------------------------------------------------------

class OpenCVIntrospector final : public ILibraryFunctionIntrospector {
 public:
  std::string libraryId() const override { return "opencv"; }

  std::vector<FunctionDescriptor> inspect() const override {
    return {
        // Source nodes
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cvhub",
            .functionName = "test_image",
            .qualifiedName = "cvhub::test_image",
            .displayName = "cvhub::test_image",
            .category = "source",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "width",
                     .displayName = "Width",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 640,
                     .required = true,
                     .minValue = 1.0,
                     .maxValue = 4096.0},
                    {.name = "height",
                     .displayName = "Height",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 360,
                     .required = true,
                     .minValue = 1.0,
                     .maxValue = 4096.0},
                },
            .factoryKey = "opencv.test_image",
            .explicitKind = NodeKind::Source,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cvhub",
            .functionName = "image_file_source",
            .qualifiedName = "cvhub::image_file_source",
            .displayName = "cvhub::image_file_source",
            .category = "source",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "path",
                     .displayName = "Path",
                     .semanticType = SemanticType::FilePath,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string(""),
                     .required = true},
                    {.name = "reload",
                     .displayName = "Reload",
                     .semanticType = SemanticType::Boolean,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = false,
                     .required = false},
                },
            .factoryKey = "opencv.image_file_source",
            .explicitKind = NodeKind::Source,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cvhub",
            .functionName = "video_file_source",
            .qualifiedName = "cvhub::video_file_source",
            .displayName = "cvhub::video_file_source",
            .category = "source",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "path",
                     .displayName = "Path",
                     .semanticType = SemanticType::FilePath,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string(""),
                     .required = true},
                },
            .factoryKey = "opencv.video_file_source",
            .explicitKind = NodeKind::Source,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cvhub",
            .functionName = "camera_source",
            .qualifiedName = "cvhub::camera_source",
            .displayName = "cvhub::camera_source",
            .category = "source",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "device",
                     .displayName = "Device",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 0,
                     .required = false,
                     .minValue = 0.0,
                     .maxValue = 10.0},
                },
            .factoryKey = "opencv.camera_source",
            .explicitKind = NodeKind::Source,
        },
        // Sink nodes
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cvhub",
            .functionName = "image_saver",
            .qualifiedName = "cvhub::image_saver",
            .displayName = "cvhub::image_saver",
            .category = "sink",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "folder",
                     .displayName = "Folder",
                     .semanticType = SemanticType::FilePath,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string("./saved"),
                     .required = false},
                    {.name = "interval_ms",
                     .displayName = "Interval (ms)",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 1000,
                     .required = false,
                     .minValue = 0.0,
                     .maxValue = 60000.0},
                },
            .factoryKey = "opencv.image_saver",
            .explicitKind = NodeKind::Sink,
        },
    };
  }
};

// ---------------------------------------------------------------------------
// Generic node types
// ---------------------------------------------------------------------------

// All stateless OpenCV source nodes (no image input, produce image)
// ---------------------------------------------------------------------------
// Stateful node state — shared across instances via closure capture.
// ---------------------------------------------------------------------------

struct ImageFileState {
  std::mutex mu;
  std::string lastPath;
  std::shared_ptr<ImageValue> cachedImage;
};

struct VideoFileState {
  std::mutex mu;
  std::string lastPath;
  cv::VideoCapture cap;
};

struct CameraState {
  std::mutex mu;
  int lastDevice{-1};
  cv::VideoCapture cap;
};

struct SaverState {
  std::mutex mu;
  std::chrono::steady_clock::time_point lastSave;
  int counter{0};
  bool initialized{false};
};

}  // namespace

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void registerOpenCVPlugin(ServiceContainer& services) {
  auto imageFileState = std::make_shared<ImageFileState>();
  auto videoFileState = std::make_shared<VideoFileState>();
  auto cameraState = std::make_shared<CameraState>();
  auto saverState = std::make_shared<SaverState>();

  using FactoryFn =
      std::function<std::shared_ptr<INodeFactory>(NodeDescriptor)>;
  std::unordered_map<std::string, FactoryFn> kRegistry = {
      // ---- stateful sources (lambda captures shared state) ----
      {"opencv.image_file_source",
       [imageFileState](NodeDescriptor d) {
         return std::make_shared<GenericNodeFactory>(
             std::move(d), [imageFileState](NodeExecutionContext& ctx) {
               const auto path =
                   parameterAs<std::string>(ctx.parameters, "path", "");
               const auto reload =
                   parameterAs<bool>(ctx.parameters, "reload", false);
               std::lock_guard lock(imageFileState->mu);
               if (path != imageFileState->lastPath || reload ||
                   !imageFileState->cachedImage) {
                 const cv::Mat mat = cv::imread(path, cv::IMREAD_UNCHANGED);
                 if (mat.empty())
                   throw std::runtime_error(
                       "image_file_source: failed to load: " + path);
                 PixelFormat fmt = PixelFormat::BGR8;
                 cv::Mat out;
                 if (mat.channels() == 1) {
                   fmt = PixelFormat::Gray8;
                   out = mat;
                 } else if (mat.channels() != 3) {
                   cv::cvtColor(mat, out, cv::COLOR_BGRA2BGR);
                 } else {
                   out = mat;
                 }
                 if (!out.isContinuous()) out = out.clone();
                 imageFileState->cachedImage = std::make_shared<ImageValue>(
                     out.cols, out.rows, out.channels(),
                     static_cast<int>(out.step), fmt, copyBytes(out));
                 imageFileState->lastPath = path;
               }
               ctx.outputs["image"] = imageFileState->cachedImage;
             });
       }},
      {"opencv.video_file_source",
       [videoFileState](NodeDescriptor d) {
         return std::make_shared<GenericNodeFactory>(
             std::move(d), [videoFileState](NodeExecutionContext& ctx) {
               const auto path =
                   parameterAs<std::string>(ctx.parameters, "path", "");
               std::lock_guard lock(videoFileState->mu);
               if (path != videoFileState->lastPath) {
                 videoFileState->cap.open(path);
                 if (!videoFileState->cap.isOpened())
                   throw std::runtime_error(
                       "video_file_source: failed to open: " + path);
                 videoFileState->lastPath = path;
               }
               cv::Mat frame;
               if (!videoFileState->cap.read(frame)) {
                 videoFileState->cap.set(cv::CAP_PROP_POS_FRAMES, 0);
                 if (!videoFileState->cap.read(frame))
                   throw std::runtime_error(
                       "video_file_source: failed to read frame: " + path);
               }
               ctx.outputs["image"] = matToImage(frame, PixelFormat::BGR8);
             });
       }},
      {"opencv.camera_source",
       [cameraState](NodeDescriptor d) {
         return std::make_shared<GenericNodeFactory>(
             std::move(d), [cameraState](NodeExecutionContext& ctx) {
               const int device = parameterAs<int>(ctx.parameters, "device", 0);
               std::lock_guard lock(cameraState->mu);
               if (device != cameraState->lastDevice) {
                 cameraState->cap.open(device);
                 if (!cameraState->cap.isOpened())
                   throw std::runtime_error(
                       "camera_source: failed to open device: " +
                       std::to_string(device));
                 cameraState->lastDevice = device;
               }
               cv::Mat frame;
               if (!cameraState->cap.read(frame))
                 throw std::runtime_error(
                     "camera_source: failed to read from device: " +
                     std::to_string(device));
               ctx.outputs["image"] = matToImage(frame, PixelFormat::BGR8);
             });
       }},
      {"opencv.image_saver",
       [saverState](NodeDescriptor d) {
         return std::make_shared<GenericNodeFactory>(
             std::move(d), [saverState](NodeExecutionContext& ctx) {
               const auto it = ctx.inputs.find("image");
               if (it == ctx.inputs.end())
                 throw std::runtime_error("image_saver: missing input 'image'");
               const auto image = valueCast<ImageValue>(it->second);
               if (!image)
                 throw std::runtime_error(
                     "image_saver: input is not an ImageValue");
               const auto folder = parameterAs<std::string>(
                   ctx.parameters, "folder", "./saved");
               const int interval_ms =
                   parameterAs<int>(ctx.parameters, "interval_ms", 1000);
               std::lock_guard lock(saverState->mu);
               const auto now = std::chrono::steady_clock::now();
               if (!saverState->initialized) {
                 saverState->lastSave = now;
                 saverState->initialized = true;
               }
               const auto elapsed =
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - saverState->lastSave)
                       .count();
               if (elapsed < static_cast<long long>(interval_ms)) return;
               std::filesystem::create_directories(folder);
               char filename[32];
               std::snprintf(filename, sizeof(filename), "frame_%06d.png",
                             saverState->counter);
               const std::string outPath = folder + "/" + filename;
               const cv::Mat mat = imageToMat(*image);
               if (!cv::imwrite(outPath, mat))
                 throw std::runtime_error("image_saver: cv::imwrite failed: " +
                                          outPath);
               ++saverState->counter;
               saverState->lastSave = now;
             });
       }},
      // ---- OpenCV source (generic) ----
      {"opencv.test_image",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageSourceFactory>(
             std::move(d), [](const ParameterMap& p) {
               const int w = parameterAs<int>(p, "width", 640);
               const int h = parameterAs<int>(p, "height", 360);
               cv::Mat image(h, w, CV_8UC3);
               for (int y = 0; y < h; ++y)
                 for (int x = 0; x < w; ++x)
                   image.at<cv::Vec3b>(y, x) =
                       cv::Vec3b(static_cast<unsigned char>(x % 256),
                                 static_cast<unsigned char>(y % 256),
                                 static_cast<unsigned char>((x + y) % 256));
               return matToImage(image, PixelFormat::BGR8);
             });
       }},
  };

  // Merge auto-generated factories (manual entries take precedence via insert).
  auto autoRegistry = getAutoOpenCVRegistry();
  for (auto& [k, v] : autoRegistry) kRegistry.emplace(k, std::move(v));

  // Combine introspector descriptors with auto-generated ones.
  OpenCVIntrospector introspector;
  auto allDescriptors = introspector.inspect();
  auto autoDescs = getAutoOpenCVDescriptors();
  allDescriptors.insert(allDescriptors.end(),
                        std::make_move_iterator(autoDescs.begin()),
                        std::make_move_iterator(autoDescs.end()));

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
