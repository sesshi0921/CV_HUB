#include "cvhub/plugins/opencv/opencv_plugin.hpp"

#include "cvhub/runtime/function_descriptor.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

namespace cvhub::plugins::opencv {

namespace {

std::vector<std::byte> copyBytes(const cv::Mat& mat)
{
    const auto byteCount = static_cast<std::size_t>(mat.total() * mat.elemSize());
    std::vector<std::byte> bytes(byteCount);
    std::memcpy(bytes.data(), mat.data, byteCount);
    return bytes;
}

cv::Mat imageToMat(const ImageValue& image)
{
    int type = CV_8UC3;
    if (image.pixelFormat() == PixelFormat::Gray8) {
        type = CV_8UC1;
    }
    return cv::Mat(image.height(), image.width(), type, const_cast<std::byte*>(image.bytes().data()), image.strideBytes());
}

MutableValuePtr matToImage(cv::Mat mat, PixelFormat format)
{
    if (!mat.isContinuous()) {
        mat = mat.clone();
    }
    const int channels = mat.channels();
    return std::make_shared<ImageValue>(mat.cols, mat.rows, channels, static_cast<int>(mat.step), format, copyBytes(mat));
}

const ImageValue& requireImage(const NodeExecutionContext& context, const PortId& port)
{
    const auto it = context.inputs.find(port);
    if (it == context.inputs.end()) {
        throw std::runtime_error("Missing image input: " + port);
    }
    const auto image = valueCast<ImageValue>(it->second);
    if (!image) {
        throw std::runtime_error("Input is not an image: " + port);
    }
    return *image;
}

class OpenCVIntrospector final : public ILibraryFunctionIntrospector {
public:
    std::string libraryId() const override { return "opencv"; }

    std::vector<FunctionDescriptor> inspect() const override
    {
        return {
            FunctionDescriptor{
                .library = "opencv",
                .namespaceName = "cvhub",
                .functionName = "test_image",
                .qualifiedName = "cvhub::test_image",
                .displayName = "OpenCV Test Image",
                .category = "source",
                .arguments = {
                    {.name = "image", .displayName = "Image", .semanticType = SemanticType::Image, .direction = ArgumentDirection::Output},
                    {.name = "width", .displayName = "Width", .semanticType = SemanticType::Integer, .direction = ArgumentDirection::Parameter, .defaultValue = 640, .required = true, .minValue = 1.0, .maxValue = 4096.0},
                    {.name = "height", .displayName = "Height", .semanticType = SemanticType::Integer, .direction = ArgumentDirection::Parameter, .defaultValue = 360, .required = true, .minValue = 1.0, .maxValue = 4096.0},
                },
                .factoryKey = "opencv.test_image",
                .explicitKind = NodeKind::Source,
            },
            FunctionDescriptor{
                .library = "opencv",
                .namespaceName = "cv",
                .functionName = "resize",
                .qualifiedName = "cv::resize",
                .displayName = "OpenCV Resize",
                .category = "geometry",
                .arguments = {
                    {.name = "image", .displayName = "Image", .semanticType = SemanticType::Image, .direction = ArgumentDirection::Input},
                    {.name = "image", .displayName = "Image", .semanticType = SemanticType::Image, .direction = ArgumentDirection::Output},
                    {.name = "width", .displayName = "Width", .semanticType = SemanticType::Integer, .direction = ArgumentDirection::Parameter, .defaultValue = 320, .required = true, .minValue = 1.0, .maxValue = 4096.0},
                    {.name = "height", .displayName = "Height", .semanticType = SemanticType::Integer, .direction = ArgumentDirection::Parameter, .defaultValue = 180, .required = true, .minValue = 1.0, .maxValue = 4096.0},
                    {.name = "interpolation", .displayName = "Interpolation", .semanticType = SemanticType::Enum, .direction = ArgumentDirection::Parameter, .defaultValue = std::string("linear"), .required = true, .options = {{"nearest", "Nearest"}, {"linear", "Linear"}, {"cubic", "Cubic"}, {"area", "Area"}}},
                },
                .factoryKey = "opencv.resize",
                .explicitKind = NodeKind::Transform,
            },
            FunctionDescriptor{
                .library = "opencv",
                .namespaceName = "cv",
                .functionName = "grayscale",
                .qualifiedName = "cv::cvtColor_BGR2GRAY",
                .displayName = "OpenCV Grayscale",
                .category = "color",
                .arguments = {
                    {.name = "image", .displayName = "Image", .semanticType = SemanticType::Image, .direction = ArgumentDirection::Input},
                    {.name = "image", .displayName = "Image", .semanticType = SemanticType::Image, .direction = ArgumentDirection::Output},
                },
                .factoryKey = "opencv.grayscale",
                .explicitKind = NodeKind::Transform,
            },
            FunctionDescriptor{
                .library = "opencv",
                .namespaceName = "cv",
                .functionName = "gaussian_blur",
                .qualifiedName = "cv::GaussianBlur",
                .displayName = "OpenCV Gaussian Blur",
                .category = "filter",
                .arguments = {
                    {.name = "image", .displayName = "Image", .semanticType = SemanticType::Image, .direction = ArgumentDirection::Input},
                    {.name = "image", .displayName = "Image", .semanticType = SemanticType::Image, .direction = ArgumentDirection::Output},
                    {.name = "kernel", .displayName = "Kernel", .semanticType = SemanticType::Integer, .direction = ArgumentDirection::Parameter, .defaultValue = 7, .required = true, .minValue = 1.0, .maxValue = 99.0},
                },
                .factoryKey = "opencv.gaussian_blur",
                .explicitKind = NodeKind::Transform,
            },
        };
    }
};

class TestImageNode final : public INode {
public:
    explicit TestImageNode(NodeDescriptor descriptor) : descriptor_(std::move(descriptor)) {}
    const NodeDescriptor& descriptor() const override { return descriptor_; }

    void execute(NodeExecutionContext& context) override
    {
        const int width = parameterAs<int>(context.parameters, "width", 640);
        const int height = parameterAs<int>(context.parameters, "height", 360);
        cv::Mat image(height, width, CV_8UC3);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                image.at<cv::Vec3b>(y, x) = cv::Vec3b(static_cast<unsigned char>(x % 256), static_cast<unsigned char>(y % 256), static_cast<unsigned char>((x + y) % 256));
            }
        }
        context.outputs["image"] = matToImage(image, PixelFormat::BGR8);
    }

private:
    NodeDescriptor descriptor_;
};

class ResizeNode final : public INode {
public:
    explicit ResizeNode(NodeDescriptor descriptor) : descriptor_(std::move(descriptor)) {}
    const NodeDescriptor& descriptor() const override { return descriptor_; }

    void execute(NodeExecutionContext& context) override
    {
        const auto& input = requireImage(context, "image");
        const int width = parameterAs<int>(context.parameters, "width", 320);
        const int height = parameterAs<int>(context.parameters, "height", 180);
        const auto interpolationName = parameterAs<std::string>(context.parameters, "interpolation", "linear");
        int interpolation = cv::INTER_LINEAR;
        if (interpolationName == "nearest") {
            interpolation = cv::INTER_NEAREST;
        } else if (interpolationName == "cubic") {
            interpolation = cv::INTER_CUBIC;
        } else if (interpolationName == "area") {
            interpolation = cv::INTER_AREA;
        }

        cv::Mat output;
        cv::resize(imageToMat(input), output, cv::Size(width, height), 0.0, 0.0, interpolation);
        context.outputs["image"] = matToImage(output, input.pixelFormat());
    }

private:
    NodeDescriptor descriptor_;
};

class GrayscaleNode final : public INode {
public:
    explicit GrayscaleNode(NodeDescriptor descriptor) : descriptor_(std::move(descriptor)) {}
    const NodeDescriptor& descriptor() const override { return descriptor_; }

    void execute(NodeExecutionContext& context) override
    {
        const auto& input = requireImage(context, "image");
        cv::Mat output;
        if (input.pixelFormat() == PixelFormat::Gray8) {
            output = imageToMat(input).clone();
        } else {
            cv::cvtColor(imageToMat(input), output, cv::COLOR_BGR2GRAY);
        }
        context.outputs["image"] = matToImage(output, PixelFormat::Gray8);
    }

private:
    NodeDescriptor descriptor_;
};

class GaussianBlurNode final : public INode {
public:
    explicit GaussianBlurNode(NodeDescriptor descriptor) : descriptor_(std::move(descriptor)) {}
    const NodeDescriptor& descriptor() const override { return descriptor_; }

    void execute(NodeExecutionContext& context) override
    {
        const auto& input = requireImage(context, "image");
        int kernel = parameterAs<int>(context.parameters, "kernel", 7);
        if (kernel < 1) {
            kernel = 1;
        }
        if (kernel % 2 == 0) {
            ++kernel;
        }
        cv::Mat output;
        cv::GaussianBlur(imageToMat(input), output, cv::Size(kernel, kernel), 0.0);
        context.outputs["image"] = matToImage(output, input.pixelFormat());
    }

private:
    NodeDescriptor descriptor_;
};

template <typename T>
class StaticNodeFactory final : public INodeFactory {
public:
    explicit StaticNodeFactory(NodeDescriptor descriptor) : descriptor_(std::move(descriptor)) {}

    std::unique_ptr<INode> create() const override
    {
        return std::make_unique<T>(descriptor_);
    }

private:
    NodeDescriptor descriptor_;
};

} // namespace

void registerOpenCVPlugin(ServiceContainer& services)
{
    OpenCVIntrospector introspector;
    const auto descriptors = services.functionNodeGenerator()->generate(introspector.inspect());
    for (const auto& descriptor : descriptors) {
        if (descriptor.id == "opencv.test_image") {
            services.nodeCatalog()->registerNode(descriptor, std::make_shared<StaticNodeFactory<TestImageNode>>(descriptor));
        } else if (descriptor.id == "opencv.resize") {
            services.nodeCatalog()->registerNode(descriptor, std::make_shared<StaticNodeFactory<ResizeNode>>(descriptor));
        } else if (descriptor.id == "opencv.grayscale") {
            services.nodeCatalog()->registerNode(descriptor, std::make_shared<StaticNodeFactory<GrayscaleNode>>(descriptor));
        } else if (descriptor.id == "opencv.gaussian_blur") {
            services.nodeCatalog()->registerNode(descriptor, std::make_shared<StaticNodeFactory<GaussianBlurNode>>(descriptor));
        }
    }
}

} // namespace cvhub::plugins::opencv
