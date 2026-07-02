#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace cvhub {

using NodeId = std::string;
using PortId = std::string;
using NodeInstanceId = std::string;

enum class NodeKind {
    Source,
    Transform,
    Sink,
    Utility
};

enum class SemanticType {
    Image,
    Integer,
    Float,
    Boolean,
    String,
    Enum,
    FilePath
};

enum class PixelFormat {
    Gray8,
    RGB8,
    BGR8
};

using ParameterValue = std::variant<int, double, bool, std::string>;

struct ParameterOption {
    std::string id;
    std::string displayName;
};

struct ParameterDescriptor {
    std::string name;
    std::string displayName;
    SemanticType type{};
    ParameterValue defaultValue{};
    bool required{false};
    std::vector<ParameterOption> options;
    std::optional<double> minValue;
    std::optional<double> maxValue;
};

struct PortDescriptor {
    std::string id;
    std::string displayName;
    SemanticType type{};
    bool required{true};
};

struct NodeUiMetadata {
    std::string title;
    std::string summary;
    bool previewOutput{true};
};

struct NodeDescriptor {
    NodeId id;
    std::string displayName;
    std::string library;
    std::string functionName;
    std::string category;
    NodeKind kind{};
    std::vector<PortDescriptor> inputs;
    std::vector<PortDescriptor> outputs;
    std::vector<ParameterDescriptor> parameters;
    NodeUiMetadata ui;
    std::string factoryKey;
};

using ParameterMap = std::unordered_map<std::string, ParameterValue>;

template <typename T>
T parameterAs(const ParameterMap& values, const std::string& key, T fallback)
{
    const auto it = values.find(key);
    if (it == values.end()) {
        return fallback;
    }
    if (const auto* value = std::get_if<T>(&it->second)) {
        return *value;
    }
    throw std::runtime_error("Parameter '" + key + "' has an unexpected type");
}

} // namespace cvhub
