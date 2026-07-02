#include "cvhub/runtime/function_descriptor.hpp"

#include <algorithm>

namespace cvhub {

std::vector<NodeDescriptor> FunctionNodeGenerator::generate(const std::vector<FunctionDescriptor>& functions) const
{
    std::vector<NodeDescriptor> nodes;
    nodes.reserve(functions.size());

    for (const auto& function : functions) {
        NodeDescriptor node;
        node.id = function.library + "." + function.functionName;
        std::ranges::replace(node.id, ':', '.');
        node.displayName = function.displayName.empty() ? function.functionName : function.displayName;
        node.library = function.library;
        node.functionName = function.functionName;
        node.category = function.category;
        node.kind = function.explicitKind;
        node.factoryKey = function.factoryKey.empty() ? node.id : function.factoryKey;
        node.ui.title = function.library + ": " + function.functionName + "()";

        for (const auto& argument : function.arguments) {
            if (argument.direction == ArgumentDirection::Input || argument.direction == ArgumentDirection::InputOutput) {
                node.inputs.push_back(PortDescriptor{
                    .id = argument.name,
                    .displayName = argument.displayName.empty() ? argument.name : argument.displayName,
                    .type = argument.semanticType,
                    .required = argument.required,
                });
            }

            if (argument.direction == ArgumentDirection::Output || argument.direction == ArgumentDirection::InputOutput) {
                node.outputs.push_back(PortDescriptor{
                    .id = argument.name,
                    .displayName = argument.displayName.empty() ? argument.name : argument.displayName,
                    .type = argument.semanticType,
                    .required = argument.required,
                });
            }

            if (argument.direction == ArgumentDirection::Parameter) {
                node.parameters.push_back(ParameterDescriptor{
                    .name = argument.name,
                    .displayName = argument.displayName.empty() ? argument.name : argument.displayName,
                    .type = argument.semanticType,
                    .defaultValue = argument.defaultValue,
                    .required = argument.required,
                    .options = argument.options,
                    .minValue = argument.minValue,
                    .maxValue = argument.maxValue,
                });
            }
        }

        nodes.push_back(std::move(node));
    }

    return nodes;
}

} // namespace cvhub
