#pragma once

#include "cvhub/core/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cvhub {

enum class ArgumentDirection {
    Input,
    Output,
    InputOutput,
    Parameter
};

struct FunctionArgumentDescriptor {
    std::string name;
    std::string displayName;
    SemanticType semanticType{};
    ArgumentDirection direction{};
    ParameterValue defaultValue{};
    bool required{true};
    std::optional<double> minValue;
    std::optional<double> maxValue;
    std::vector<ParameterOption> options;
};

struct FunctionDescriptor {
    std::string library;
    std::string namespaceName;
    std::string functionName;
    std::string qualifiedName;
    std::string displayName;
    std::string category;
    std::vector<FunctionArgumentDescriptor> arguments;
    std::vector<std::string> tags;
    std::string factoryKey;
    NodeKind explicitKind{NodeKind::Transform};
};

class ILibraryFunctionIntrospector {
public:
    virtual ~ILibraryFunctionIntrospector() = default;
    virtual std::string libraryId() const = 0;
    virtual std::vector<FunctionDescriptor> inspect() const = 0;
};

class IFunctionNodeGenerator {
public:
    virtual ~IFunctionNodeGenerator() = default;
    virtual std::vector<NodeDescriptor> generate(const std::vector<FunctionDescriptor>& functions) const = 0;
};

class FunctionNodeGenerator final : public IFunctionNodeGenerator {
public:
    std::vector<NodeDescriptor> generate(const std::vector<FunctionDescriptor>& functions) const override;
};

} // namespace cvhub
