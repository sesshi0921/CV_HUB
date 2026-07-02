#pragma once

#include "cvhub/core/types.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace cvhub {

class ValueBase {
public:
    virtual ~ValueBase() = default;
    virtual SemanticType type() const = 0;
    virtual std::string_view typeName() const = 0;
    virtual bool immutable() const = 0;
};

class ImageValue final : public ValueBase {
public:
    ImageValue(int width, int height, int channels, int strideBytes, PixelFormat pixelFormat, std::vector<std::byte> bytes);

    SemanticType type() const override { return SemanticType::Image; }
    std::string_view typeName() const override { return "image"; }
    bool immutable() const override { return true; }

    int width() const { return width_; }
    int height() const { return height_; }
    int channels() const { return channels_; }
    int strideBytes() const { return strideBytes_; }
    PixelFormat pixelFormat() const { return pixelFormat_; }
    std::span<const std::byte> bytes() const { return bytes_; }

private:
    int width_{};
    int height_{};
    int channels_{};
    int strideBytes_{};
    PixelFormat pixelFormat_{};
    std::vector<std::byte> bytes_;
};

using ValuePtr = std::shared_ptr<const ValueBase>;
using MutableValuePtr = std::shared_ptr<ValueBase>;

template <typename T>
std::shared_ptr<const T> valueCast(const ValuePtr& value)
{
    return std::dynamic_pointer_cast<const T>(value);
}

} // namespace cvhub
