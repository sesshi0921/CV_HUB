#pragma once

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

#include "cvhub/core/node.hpp"
#include "cvhub/core/types.hpp"
#include "cvhub/core/values.hpp"
#include "cvhub/runtime/function_descriptor.hpp"

namespace cvhub::plugins::common {

using FactoryFn = std::function<std::shared_ptr<INodeFactory>(NodeDescriptor)>;

inline std::vector<std::byte> copyBytes(const cv::Mat& mat) {
    const auto n = static_cast<std::size_t>(mat.total() * mat.elemSize());
    std::vector<std::byte> bytes(n);
    std::memcpy(bytes.data(), mat.data, n);
    return bytes;
}

inline cv::Mat imageToMat(const ImageValue& image) {
    const int type = (image.pixelFormat() == PixelFormat::Gray8) ? CV_8UC1 : CV_8UC3;
    return cv::Mat(image.height(), image.width(), type,
                   const_cast<std::byte*>(image.bytes().data()),
                   static_cast<std::size_t>(image.strideBytes()));
}

inline std::shared_ptr<ImageValue> matToImage(cv::Mat mat, PixelFormat fmt) {
    if (!mat.isContinuous())
        mat = mat.clone();
    return std::make_shared<ImageValue>(mat.cols, mat.rows, mat.channels(),
                                        static_cast<int>(mat.step), fmt, copyBytes(mat));
}

} // namespace cvhub::plugins::common
