#pragma once

#include <functional>
#include <memory>

#include "cvhub/core/node.hpp"
#include "cvhub/core/values.hpp"

namespace cvhub {

/// Produces an image from parameters only (stateless source).
using ImageSourceFn =
    std::function<std::shared_ptr<ImageValue>(const ParameterMap&)>;

/// Transforms one image into another (stateless transform).
using ImageTransformFn = std::function<std::shared_ptr<ImageValue>(
    std::shared_ptr<const ImageValue>, const ParameterMap&)>;

// ---------------------------------------------------------------------------

class GenericImageSourceNode final : public INode {
 public:
  GenericImageSourceNode(NodeDescriptor descriptor, ImageSourceFn fn)
      : descriptor_(std::move(descriptor)), fn_(std::move(fn)) {}

  const NodeDescriptor& descriptor() const override { return descriptor_; }

  void execute(NodeExecutionContext& context) override {
    auto image = fn_(context.parameters);
    if (image) context.outputs["image"] = std::move(image);
  }

 private:
  NodeDescriptor descriptor_;
  ImageSourceFn fn_;
};

class GenericImageTransformNode final : public INode {
 public:
  GenericImageTransformNode(NodeDescriptor descriptor, ImageTransformFn fn)
      : descriptor_(std::move(descriptor)), fn_(std::move(fn)) {}

  const NodeDescriptor& descriptor() const override { return descriptor_; }

  void execute(NodeExecutionContext& context) override {
    const auto it = context.inputs.find("image");
    if (it == context.inputs.end()) return;
    const auto input = valueCast<ImageValue>(it->second);
    if (!input) return;
    auto output = fn_(input, context.parameters);
    if (output) context.outputs["image"] = std::move(output);
  }

 private:
  NodeDescriptor descriptor_;
  ImageTransformFn fn_;
};

// ---------------------------------------------------------------------------

class GenericImageSourceFactory final : public INodeFactory {
 public:
  GenericImageSourceFactory(NodeDescriptor descriptor, ImageSourceFn fn)
      : descriptor_(std::move(descriptor)), fn_(std::move(fn)) {}

  std::unique_ptr<INode> create() const override {
    return std::make_unique<GenericImageSourceNode>(descriptor_, fn_);
  }

 private:
  NodeDescriptor descriptor_;
  ImageSourceFn fn_;
};

class GenericImageTransformFactory final : public INodeFactory {
 public:
  GenericImageTransformFactory(NodeDescriptor descriptor, ImageTransformFn fn)
      : descriptor_(std::move(descriptor)), fn_(std::move(fn)) {}

  std::unique_ptr<INode> create() const override {
    return std::make_unique<GenericImageTransformNode>(descriptor_, fn_);
  }

 private:
  NodeDescriptor descriptor_;
  ImageTransformFn fn_;
};

/// Factory for nodes that share a single TState instance across all instances.
template <typename TNode, typename TState>
class SharedStateFactory final : public INodeFactory {
 public:
  SharedStateFactory(NodeDescriptor descriptor, std::shared_ptr<TState> state)
      : descriptor_(std::move(descriptor)), state_(std::move(state)) {}

  std::unique_ptr<INode> create() const override {
    return std::make_unique<TNode>(descriptor_, state_);
  }

 private:
  NodeDescriptor descriptor_;
  std::shared_ptr<TState> state_;
};

}  // namespace cvhub
