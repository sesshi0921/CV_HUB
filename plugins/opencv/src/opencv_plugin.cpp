#include "cvhub/plugins/opencv/opencv_plugin.hpp"

#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cvhub/runtime/function_descriptor.hpp"
#include "cvhub/runtime/generic_node.hpp"

namespace cvhub::plugins::opencv {

namespace {

// ---------------------------------------------------------------------------
// Mat / ImageValue helpers
// ---------------------------------------------------------------------------

std::vector<std::byte> copyBytes(const cv::Mat& mat) {
  const auto byteCount = static_cast<std::size_t>(mat.total() * mat.elemSize());
  std::vector<std::byte> bytes(byteCount);
  std::memcpy(bytes.data(), mat.data, byteCount);
  return bytes;
}

cv::Mat imageToMat(const ImageValue& image) {
  const int type =
      (image.pixelFormat() == PixelFormat::Gray8) ? CV_8UC1 : CV_8UC3;
  return cv::Mat(image.height(), image.width(), type,
                 const_cast<std::byte*>(image.bytes().data()),
                 static_cast<std::size_t>(image.strideBytes()));
}

std::shared_ptr<ImageValue> matToImage(cv::Mat mat, PixelFormat format) {
  if (!mat.isContinuous()) mat = mat.clone();
  return std::make_shared<ImageValue>(mat.cols, mat.rows, mat.channels(),
                                      static_cast<int>(mat.step), format,
                                      copyBytes(mat));
}

cv::Mat toGray(const cv::Mat& src, PixelFormat fmt) {
  if (fmt == PixelFormat::Gray8) return src;
  cv::Mat gray;
  cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
  return gray;
}

// ---------------------------------------------------------------------------
// Introspector
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
        // Geometry
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "resize",
            .qualifiedName = "cv::resize",
            .displayName = "cv::resize",
            .category = "geometry",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "width",
                     .displayName = "Width",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 320,
                     .required = true,
                     .minValue = 1.0,
                     .maxValue = 4096.0},
                    {.name = "height",
                     .displayName = "Height",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 180,
                     .required = true,
                     .minValue = 1.0,
                     .maxValue = 4096.0},
                    {.name = "interpolation",
                     .displayName = "Interpolation",
                     .semanticType = SemanticType::Enum,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string("linear"),
                     .required = false,
                     .options = {{"nearest", "Nearest"},
                                 {"linear", "Linear"},
                                 {"cubic", "Cubic"},
                                 {"area", "Area"}}},
                },
            .factoryKey = "opencv.resize",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "flip",
            .qualifiedName = "cv::flip",
            .displayName = "cv::flip",
            .category = "geometry",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "direction",
                     .displayName = "Direction",
                     .semanticType = SemanticType::Enum,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string("horizontal"),
                     .required = false,
                     .options = {{"horizontal", "Horizontal"},
                                 {"vertical", "Vertical"},
                                 {"both", "Both"}}},
                },
            .factoryKey = "opencv.flip",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "rotate",
            .qualifiedName = "cv::rotate",
            .displayName = "cv::rotate",
            .category = "geometry",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "angle",
                     .displayName = "Angle",
                     .semanticType = SemanticType::Enum,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string("cw90"),
                     .required = false,
                     .options = {{"cw90", "90° CW"},
                                 {"180", "180°"},
                                 {"ccw90", "90° CCW"}}},
                },
            .factoryKey = "opencv.rotate",
            .explicitKind = NodeKind::Transform,
        },
        // Color
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "grayscale",
            .qualifiedName = "cv::cvtColor_BGR2GRAY",
            .displayName = "cv::cvtColor (BGR2GRAY)",
            .category = "color",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                },
            .factoryKey = "opencv.grayscale",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "cvt_color",
            .qualifiedName = "cv::cvtColor",
            .displayName = "cv::cvtColor",
            .category = "color",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "conversion",
                     .displayName = "Conversion",
                     .semanticType = SemanticType::Enum,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string("bgr2rgb"),
                     .required = true,
                     .options = {{"bgr2rgb", "BGR → RGB"},
                                 {"bgr2hsv", "BGR → HSV"},
                                 {"bgr2lab", "BGR → LAB"},
                                 {"bgr2ycrcb", "BGR → YCrCb"},
                                 {"gray2bgr", "Gray → BGR"},
                                 {"bgr2hls", "BGR → HLS"}}},
                },
            .factoryKey = "opencv.cvt_color",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "equalize_hist",
            .qualifiedName = "cv::equalizeHist",
            .displayName = "cv::equalizeHist",
            .category = "color",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image (Gray)",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                },
            .factoryKey = "opencv.equalize_hist",
            .explicitKind = NodeKind::Transform,
        },
        // Filter
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "gaussian_blur",
            .qualifiedName = "cv::GaussianBlur",
            .displayName = "cv::GaussianBlur",
            .category = "filter",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "kernel",
                     .displayName = "Kernel Size",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 7,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 99.0},
                },
            .factoryKey = "opencv.gaussian_blur",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "blur",
            .qualifiedName = "cv::blur",
            .displayName = "cv::blur",
            .category = "filter",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "kernel",
                     .displayName = "Kernel Size",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 5,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 99.0},
                },
            .factoryKey = "opencv.blur",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "median_blur",
            .qualifiedName = "cv::medianBlur",
            .displayName = "cv::medianBlur",
            .category = "filter",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "kernel",
                     .displayName = "Kernel Size",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 5,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 99.0},
                },
            .factoryKey = "opencv.median_blur",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "bilateral_filter",
            .qualifiedName = "cv::bilateralFilter",
            .displayName = "cv::bilateralFilter",
            .category = "filter",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "d",
                     .displayName = "Diameter",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 9,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 30.0},
                    {.name = "sigma_color",
                     .displayName = "Sigma Color",
                     .semanticType = SemanticType::Float,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 75.0,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 250.0},
                    {.name = "sigma_space",
                     .displayName = "Sigma Space",
                     .semanticType = SemanticType::Float,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 75.0,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 250.0},
                },
            .factoryKey = "opencv.bilateral_filter",
            .explicitKind = NodeKind::Transform,
        },
        // Edge
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "canny",
            .qualifiedName = "cv::Canny",
            .displayName = "cv::Canny",
            .category = "edge",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "threshold1",
                     .displayName = "Threshold 1",
                     .semanticType = SemanticType::Float,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 50.0,
                     .required = false,
                     .minValue = 0.0,
                     .maxValue = 255.0},
                    {.name = "threshold2",
                     .displayName = "Threshold 2",
                     .semanticType = SemanticType::Float,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 150.0,
                     .required = false,
                     .minValue = 0.0,
                     .maxValue = 255.0},
                    {.name = "aperture",
                     .displayName = "Aperture",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 3,
                     .required = false,
                     .minValue = 3.0,
                     .maxValue = 7.0},
                },
            .factoryKey = "opencv.canny",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "sobel",
            .qualifiedName = "cv::Sobel",
            .displayName = "cv::Sobel",
            .category = "edge",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "dx",
                     .displayName = "dx",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 1,
                     .required = false,
                     .minValue = 0.0,
                     .maxValue = 1.0},
                    {.name = "dy",
                     .displayName = "dy",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 0,
                     .required = false,
                     .minValue = 0.0,
                     .maxValue = 1.0},
                    {.name = "ksize",
                     .displayName = "Kernel Size",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 3,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 7.0},
                },
            .factoryKey = "opencv.sobel",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "laplacian",
            .qualifiedName = "cv::Laplacian",
            .displayName = "cv::Laplacian",
            .category = "edge",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "ksize",
                     .displayName = "Kernel Size",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 3,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 7.0},
                },
            .factoryKey = "opencv.laplacian",
            .explicitKind = NodeKind::Transform,
        },
        // Threshold
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "threshold",
            .qualifiedName = "cv::threshold",
            .displayName = "cv::threshold",
            .category = "threshold",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "thresh",
                     .displayName = "Threshold",
                     .semanticType = SemanticType::Float,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 127.0,
                     .required = false,
                     .minValue = 0.0,
                     .maxValue = 255.0},
                    {.name = "maxval",
                     .displayName = "Max Value",
                     .semanticType = SemanticType::Float,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 255.0,
                     .required = false,
                     .minValue = 0.0,
                     .maxValue = 255.0},
                    {.name = "type",
                     .displayName = "Type",
                     .semanticType = SemanticType::Enum,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string("binary"),
                     .required = false,
                     .options = {{"binary", "Binary"},
                                 {"binary_inv", "Binary Inv"},
                                 {"trunc", "Truncate"},
                                 {"tozero", "To Zero"},
                                 {"otsu", "Otsu"}}},
                },
            .factoryKey = "opencv.threshold",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "adaptive_threshold",
            .qualifiedName = "cv::adaptiveThreshold",
            .displayName = "cv::adaptiveThreshold",
            .category = "threshold",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image (Gray)",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "maxval",
                     .displayName = "Max Value",
                     .semanticType = SemanticType::Float,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 255.0,
                     .required = false,
                     .minValue = 0.0,
                     .maxValue = 255.0},
                    {.name = "method",
                     .displayName = "Method",
                     .semanticType = SemanticType::Enum,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string("gaussian"),
                     .required = false,
                     .options = {{"mean", "Mean"}, {"gaussian", "Gaussian"}}},
                    {.name = "type",
                     .displayName = "Type",
                     .semanticType = SemanticType::Enum,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string("binary"),
                     .required = false,
                     .options = {{"binary", "Binary"},
                                 {"binary_inv", "Binary Inv"}}},
                    {.name = "block_size",
                     .displayName = "Block Size",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 11,
                     .required = false,
                     .minValue = 3.0,
                     .maxValue = 99.0},
                    {.name = "c",
                     .displayName = "C",
                     .semanticType = SemanticType::Float,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 2.0,
                     .required = false,
                     .minValue = -100.0,
                     .maxValue = 100.0},
                },
            .factoryKey = "opencv.adaptive_threshold",
            .explicitKind = NodeKind::Transform,
        },
        // Morphology
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "erode",
            .qualifiedName = "cv::erode",
            .displayName = "cv::erode",
            .category = "morphology",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "kernel",
                     .displayName = "Kernel Size",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 3,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 31.0},
                    {.name = "iterations",
                     .displayName = "Iterations",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 1,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 10.0},
                },
            .factoryKey = "opencv.erode",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "dilate",
            .qualifiedName = "cv::dilate",
            .displayName = "cv::dilate",
            .category = "morphology",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "kernel",
                     .displayName = "Kernel Size",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 3,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 31.0},
                    {.name = "iterations",
                     .displayName = "Iterations",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 1,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 10.0},
                },
            .factoryKey = "opencv.dilate",
            .explicitKind = NodeKind::Transform,
        },
        FunctionDescriptor{
            .library = "opencv",
            .namespaceName = "cv",
            .functionName = "morphology_ex",
            .qualifiedName = "cv::morphologyEx",
            .displayName = "cv::morphologyEx",
            .category = "morphology",
            .arguments =
                {
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Input},
                    {.name = "image",
                     .displayName = "Image",
                     .semanticType = SemanticType::Image,
                     .direction = ArgumentDirection::Output},
                    {.name = "op",
                     .displayName = "Operation",
                     .semanticType = SemanticType::Enum,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = std::string("open"),
                     .required = false,
                     .options = {{"open", "Open"},
                                 {"close", "Close"},
                                 {"gradient", "Gradient"},
                                 {"tophat", "Top Hat"},
                                 {"blackhat", "Black Hat"}}},
                    {.name = "kernel",
                     .displayName = "Kernel Size",
                     .semanticType = SemanticType::Integer,
                     .direction = ArgumentDirection::Parameter,
                     .defaultValue = 3,
                     .required = false,
                     .minValue = 1.0,
                     .maxValue = 31.0},
                },
            .factoryKey = "opencv.morphology_ex",
            .explicitKind = NodeKind::Transform,
        },
    };
  }
};

// ---------------------------------------------------------------------------
// Generic node types
// ---------------------------------------------------------------------------

// All stateless OpenCV source nodes (no image input, produce image)
// ---------------------------------------------------------------------------
// cvhub stateful nodes (exceptions — these keep individual class definitions)
// ---------------------------------------------------------------------------

struct ImageFileState {
  std::mutex mu;
  std::string lastPath;
  std::shared_ptr<ImageValue> cachedImage;
};

class ImageFileSourceNode final : public INode {
 public:
  ImageFileSourceNode(NodeDescriptor descriptor,
                      std::shared_ptr<ImageFileState> state)
      : descriptor_(std::move(descriptor)), state_(std::move(state)) {}

  const NodeDescriptor& descriptor() const override { return descriptor_; }

  void execute(NodeExecutionContext& context) override {
    const auto path = parameterAs<std::string>(context.parameters, "path", "");
    const auto reload = parameterAs<bool>(context.parameters, "reload", false);

    std::lock_guard lock(state_->mu);
    if (path != state_->lastPath || reload || !state_->cachedImage) {
      const cv::Mat mat = cv::imread(path, cv::IMREAD_UNCHANGED);
      if (mat.empty())
        throw std::runtime_error("ImageFileSourceNode: failed to load: " +
                                 path);
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
      state_->cachedImage = std::make_shared<ImageValue>(
          out.cols, out.rows, out.channels(), static_cast<int>(out.step), fmt,
          copyBytes(out));
      state_->lastPath = path;
    }
    context.outputs["image"] = state_->cachedImage;
  }

 private:
  NodeDescriptor descriptor_;
  std::shared_ptr<ImageFileState> state_;
};

struct VideoFileState {
  std::mutex mu;
  std::string lastPath;
  cv::VideoCapture cap;
};

class VideoFileSourceNode final : public INode {
 public:
  VideoFileSourceNode(NodeDescriptor descriptor,
                      std::shared_ptr<VideoFileState> state)
      : descriptor_(std::move(descriptor)), state_(std::move(state)) {}

  const NodeDescriptor& descriptor() const override { return descriptor_; }

  void execute(NodeExecutionContext& context) override {
    const auto path = parameterAs<std::string>(context.parameters, "path", "");
    std::lock_guard lock(state_->mu);
    if (path != state_->lastPath) {
      state_->cap.open(path);
      if (!state_->cap.isOpened())
        throw std::runtime_error("VideoFileSourceNode: failed to open: " +
                                 path);
      state_->lastPath = path;
    }
    cv::Mat frame;
    if (!state_->cap.read(frame)) {
      state_->cap.set(cv::CAP_PROP_POS_FRAMES, 0);
      if (!state_->cap.read(frame))
        throw std::runtime_error("VideoFileSourceNode: failed to read frame: " +
                                 path);
    }
    context.outputs["image"] = matToImage(frame, PixelFormat::BGR8);
  }

 private:
  NodeDescriptor descriptor_;
  std::shared_ptr<VideoFileState> state_;
};

struct CameraState {
  std::mutex mu;
  int lastDevice{-1};
  cv::VideoCapture cap;
};

class CameraSourceNode final : public INode {
 public:
  CameraSourceNode(NodeDescriptor descriptor,
                   std::shared_ptr<CameraState> state)
      : descriptor_(std::move(descriptor)), state_(std::move(state)) {}

  const NodeDescriptor& descriptor() const override { return descriptor_; }

  void execute(NodeExecutionContext& context) override {
    const int device = parameterAs<int>(context.parameters, "device", 0);
    std::lock_guard lock(state_->mu);
    if (device != state_->lastDevice) {
      state_->cap.open(device);
      if (!state_->cap.isOpened())
        throw std::runtime_error("CameraSourceNode: failed to open device: " +
                                 std::to_string(device));
      state_->lastDevice = device;
    }
    cv::Mat frame;
    if (!state_->cap.read(frame))
      throw std::runtime_error(
          "CameraSourceNode: failed to read from device: " +
          std::to_string(device));
    context.outputs["image"] = matToImage(frame, PixelFormat::BGR8);
  }

 private:
  NodeDescriptor descriptor_;
  std::shared_ptr<CameraState> state_;
};

struct SaverState {
  std::mutex mu;
  std::chrono::steady_clock::time_point lastSave;
  int counter{0};
  bool initialized{false};
};

class ImageSaverNode final : public INode {
 public:
  ImageSaverNode(NodeDescriptor descriptor, std::shared_ptr<SaverState> state)
      : descriptor_(std::move(descriptor)), state_(std::move(state)) {}

  const NodeDescriptor& descriptor() const override { return descriptor_; }

  void execute(NodeExecutionContext& context) override {
    const auto it = context.inputs.find("image");
    if (it == context.inputs.end())
      throw std::runtime_error("ImageSaverNode: missing input 'image'");
    const auto image = valueCast<ImageValue>(it->second);
    if (!image)
      throw std::runtime_error("ImageSaverNode: input is not an ImageValue");

    const auto folder =
        parameterAs<std::string>(context.parameters, "folder", "./saved");
    const int interval_ms =
        parameterAs<int>(context.parameters, "interval_ms", 1000);

    std::lock_guard lock(state_->mu);
    const auto now = std::chrono::steady_clock::now();
    if (!state_->initialized) {
      state_->lastSave = now;
      state_->initialized = true;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now - state_->lastSave)
                             .count();
    if (elapsed < static_cast<long long>(interval_ms)) return;

    std::filesystem::create_directories(folder);
    char filename[32];
    std::snprintf(filename, sizeof(filename), "frame_%06d.png",
                  state_->counter);
    const std::string outPath = folder + "/" + filename;
    const cv::Mat mat = imageToMat(*image);
    if (!cv::imwrite(outPath, mat))
      throw std::runtime_error("ImageSaverNode: cv::imwrite failed: " +
                               outPath);
    ++state_->counter;
    state_->lastSave = now;
  }

 private:
  NodeDescriptor descriptor_;
  std::shared_ptr<SaverState> state_;
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
  const std::unordered_map<std::string, FactoryFn> kRegistry = {
      // ---- cvhub stateful sources (individual classes) ----
      {"opencv.image_file_source",
       [&imageFileState](NodeDescriptor d) {
         return std::make_shared<
             SharedStateFactory<ImageFileSourceNode, ImageFileState>>(
             std::move(d), imageFileState);
       }},
      {"opencv.video_file_source",
       [&videoFileState](NodeDescriptor d) {
         return std::make_shared<
             SharedStateFactory<VideoFileSourceNode, VideoFileState>>(
             std::move(d), videoFileState);
       }},
      {"opencv.camera_source",
       [&cameraState](NodeDescriptor d) {
         return std::make_shared<
             SharedStateFactory<CameraSourceNode, CameraState>>(std::move(d),
                                                                cameraState);
       }},
      {"opencv.image_saver",
       [&saverState](NodeDescriptor d) {
         return std::make_shared<
             SharedStateFactory<ImageSaverNode, SaverState>>(std::move(d),
                                                             saverState);
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
      // ---- OpenCV transforms (generic — no per-node class) ----
      {"opencv.resize",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               const int w = parameterAs<int>(p, "width", 320);
               const int h = parameterAs<int>(p, "height", 180);
               const auto interp =
                   parameterAs<std::string>(p, "interpolation", "linear");
               int flag = cv::INTER_LINEAR;
               if (interp == "nearest")
                 flag = cv::INTER_NEAREST;
               else if (interp == "cubic")
                 flag = cv::INTER_CUBIC;
               else if (interp == "area")
                 flag = cv::INTER_AREA;
               cv::Mat out;
               cv::resize(src, out, cv::Size(w, h), 0.0, 0.0, flag);
               return matToImage(out, fmt);
             });
       }},
      {"opencv.flip",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               const auto dir =
                   parameterAs<std::string>(p, "direction", "horizontal");
               int code = 1;
               if (dir == "vertical")
                 code = 0;
               else if (dir == "both")
                 code = -1;
               cv::Mat out;
               cv::flip(src, out, code);
               return matToImage(out, fmt);
             });
       }},
      {"opencv.rotate",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               const auto angle = parameterAs<std::string>(p, "angle", "cw90");
               int code = cv::ROTATE_90_CLOCKWISE;
               if (angle == "180")
                 code = cv::ROTATE_180;
               else if (angle == "ccw90")
                 code = cv::ROTATE_90_COUNTERCLOCKWISE;
               cv::Mat out;
               cv::rotate(src, out, code);
               return matToImage(out, fmt);
             });
       }},
      {"opencv.grayscale",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d),
             [](std::shared_ptr<const ImageValue> input, const ParameterMap&) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               cv::Mat out;
               if (fmt == PixelFormat::Gray8)
                 out = src.clone();
               else
                 cv::cvtColor(src, out, cv::COLOR_BGR2GRAY);
               return matToImage(out, PixelFormat::Gray8);
             });
       }},
      {"opencv.cvt_color",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const auto conv =
                   parameterAs<std::string>(p, "conversion", "bgr2rgb");
               int code = cv::COLOR_BGR2RGB;
               PixelFormat outFmt = PixelFormat::RGB8;
               if (conv == "bgr2rgb") {
                 code = cv::COLOR_BGR2RGB;
                 outFmt = PixelFormat::RGB8;
               } else if (conv == "bgr2hsv") {
                 code = cv::COLOR_BGR2HSV;
                 outFmt = PixelFormat::BGR8;
               } else if (conv == "bgr2lab") {
                 code = cv::COLOR_BGR2Lab;
                 outFmt = PixelFormat::BGR8;
               } else if (conv == "bgr2ycrcb") {
                 code = cv::COLOR_BGR2YCrCb;
                 outFmt = PixelFormat::BGR8;
               } else if (conv == "gray2bgr") {
                 code = cv::COLOR_GRAY2BGR;
                 outFmt = PixelFormat::BGR8;
               } else if (conv == "bgr2hls") {
                 code = cv::COLOR_BGR2HLS;
                 outFmt = PixelFormat::BGR8;
               }
               cv::Mat out;
               cv::cvtColor(src, out, code);
               return matToImage(out, outFmt);
             });
       }},
      {"opencv.equalize_hist",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d),
             [](std::shared_ptr<const ImageValue> input, const ParameterMap&) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               cv::Mat gray = toGray(src, fmt);
               cv::Mat out;
               cv::equalizeHist(gray, out);
               return matToImage(out, PixelFormat::Gray8);
             });
       }},
      {"opencv.gaussian_blur",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               int k = parameterAs<int>(p, "kernel", 7);
               if (k < 1) k = 1;
               if (k % 2 == 0) ++k;
               cv::Mat out;
               cv::GaussianBlur(src, out, cv::Size(k, k), 0.0);
               return matToImage(out, fmt);
             });
       }},
      {"opencv.blur",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               int k = parameterAs<int>(p, "kernel", 5);
               if (k < 1) k = 1;
               cv::Mat out;
               cv::blur(src, out, cv::Size(k, k));
               return matToImage(out, fmt);
             });
       }},
      {"opencv.median_blur",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               int k = parameterAs<int>(p, "kernel", 5);
               if (k < 1) k = 1;
               if (k % 2 == 0) ++k;
               cv::Mat out;
               cv::medianBlur(src, out, k);
               return matToImage(out, fmt);
             });
       }},
      {"opencv.bilateral_filter",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               const int d = parameterAs<int>(p, "d", 9);
               const double sc = parameterAs<double>(p, "sigma_color", 75.0);
               const double ss = parameterAs<double>(p, "sigma_space", 75.0);
               cv::Mat cont = src.isContinuous() ? src : src.clone();
               cv::Mat out;
               cv::bilateralFilter(cont, out, d, sc, ss);
               return matToImage(out, fmt);
             });
       }},
      {"opencv.canny",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               const double t1 = parameterAs<double>(p, "threshold1", 50.0);
               const double t2 = parameterAs<double>(p, "threshold2", 150.0);
               int ap = parameterAs<int>(p, "aperture", 3);
               if (ap % 2 == 0) ++ap;
               cv::Mat out;
               cv::Canny(toGray(src, fmt), out, t1, t2, ap);
               return matToImage(out, PixelFormat::Gray8);
             });
       }},
      {"opencv.sobel",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               const int dx = parameterAs<int>(p, "dx", 1);
               const int dy = parameterAs<int>(p, "dy", 0);
               int k = parameterAs<int>(p, "ksize", 3);
               if (k % 2 == 0) ++k;
               cv::Mat grad, out;
               cv::Sobel(toGray(src, fmt), grad, CV_16S, dx, dy, k);
               cv::convertScaleAbs(grad, out);
               return matToImage(out, PixelFormat::Gray8);
             });
       }},
      {"opencv.laplacian",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               int k = parameterAs<int>(p, "ksize", 3);
               if (k % 2 == 0) ++k;
               cv::Mat lap, out;
               cv::Laplacian(toGray(src, fmt), lap, CV_16S, k);
               cv::convertScaleAbs(lap, out);
               return matToImage(out, PixelFormat::Gray8);
             });
       }},
      {"opencv.threshold",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               const double thresh = parameterAs<double>(p, "thresh", 127.0);
               const double maxval = parameterAs<double>(p, "maxval", 255.0);
               const auto tn = parameterAs<std::string>(p, "type", "binary");
               int tt = cv::THRESH_BINARY;
               if (tn == "binary_inv")
                 tt = cv::THRESH_BINARY_INV;
               else if (tn == "trunc")
                 tt = cv::THRESH_TRUNC;
               else if (tn == "tozero")
                 tt = cv::THRESH_TOZERO;
               else if (tn == "otsu")
                 tt = cv::THRESH_BINARY | cv::THRESH_OTSU;
               cv::Mat out;
               cv::threshold(toGray(src, fmt), out, thresh, maxval, tt);
               return matToImage(out, PixelFormat::Gray8);
             });
       }},
      {"opencv.adaptive_threshold",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               const double maxval = parameterAs<double>(p, "maxval", 255.0);
               const auto method =
                   parameterAs<std::string>(p, "method", "gaussian");
               const auto tn = parameterAs<std::string>(p, "type", "binary");
               int bs = parameterAs<int>(p, "block_size", 11);
               const double c = parameterAs<double>(p, "c", 2.0);
               if (bs % 2 == 0) ++bs;
               if (bs < 3) bs = 3;
               const int am = (method == "mean")
                                  ? cv::ADAPTIVE_THRESH_MEAN_C
                                  : cv::ADAPTIVE_THRESH_GAUSSIAN_C;
               const int tt = (tn == "binary_inv") ? cv::THRESH_BINARY_INV
                                                   : cv::THRESH_BINARY;
               cv::Mat out;
               cv::adaptiveThreshold(toGray(src, fmt), out, maxval, am, tt, bs,
                                     c);
               return matToImage(out, PixelFormat::Gray8);
             });
       }},
      {"opencv.erode",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               int k = parameterAs<int>(p, "kernel", 3);
               if (k < 1) k = 1;
               const int iter = parameterAs<int>(p, "iterations", 1);
               const cv::Mat el =
                   cv::getStructuringElement(cv::MORPH_RECT, cv::Size(k, k));
               cv::Mat out;
               cv::erode(src, out, el, cv::Point(-1, -1), iter);
               return matToImage(out, fmt);
             });
       }},
      {"opencv.dilate",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               int k = parameterAs<int>(p, "kernel", 3);
               if (k < 1) k = 1;
               const int iter = parameterAs<int>(p, "iterations", 1);
               const cv::Mat el =
                   cv::getStructuringElement(cv::MORPH_RECT, cv::Size(k, k));
               cv::Mat out;
               cv::dilate(src, out, el, cv::Point(-1, -1), iter);
               return matToImage(out, fmt);
             });
       }},
      {"opencv.morphology_ex",
       [](NodeDescriptor d) {
         return std::make_shared<GenericImageTransformFactory>(
             std::move(d), [](std::shared_ptr<const ImageValue> input,
                              const ParameterMap& p) {
               const cv::Mat src = imageToMat(*input);
               const PixelFormat fmt = input->pixelFormat();
               const auto op = parameterAs<std::string>(p, "op", "open");
               int k = parameterAs<int>(p, "kernel", 3);
               if (k < 1) k = 1;
               int morphOp = cv::MORPH_OPEN;
               if (op == "close")
                 morphOp = cv::MORPH_CLOSE;
               else if (op == "gradient")
                 morphOp = cv::MORPH_GRADIENT;
               else if (op == "tophat")
                 morphOp = cv::MORPH_TOPHAT;
               else if (op == "blackhat")
                 morphOp = cv::MORPH_BLACKHAT;
               const cv::Mat el =
                   cv::getStructuringElement(cv::MORPH_RECT, cv::Size(k, k));
               cv::Mat out;
               cv::morphologyEx(src, out, morphOp, el);
               return matToImage(out, fmt);
             });
       }},
  };

  OpenCVIntrospector introspector;
  const auto descriptors =
      services.functionNodeGenerator()->generate(introspector.inspect());
  for (const auto& descriptor : descriptors) {
    const auto it = kRegistry.find(descriptor.factoryKey);
    if (it != kRegistry.end()) {
      services.nodeCatalog()->registerNode(descriptor, it->second(descriptor));
    }
  }
}

}  // namespace cvhub::plugins::opencv
