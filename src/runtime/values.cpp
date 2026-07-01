#include "cvhub/core/values.hpp"

#include <utility>

namespace cvhub {

ImageValue::ImageValue(int width, int height, int channels, int strideBytes, PixelFormat pixelFormat, std::vector<std::byte> bytes)
    : width_(width)
    , height_(height)
    , channels_(channels)
    , strideBytes_(strideBytes)
    , pixelFormat_(pixelFormat)
    , bytes_(std::move(bytes))
{
}

} // namespace cvhub
