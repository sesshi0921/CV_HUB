# CV_HUB Architecture Summary

Use `docs/ARCHITECTURE.md` as the canonical document. This file is a compact reminder for Codex work.

## Core Idea

CV_HUB is a C++20 image-processing pipeline platform. Each image-processing function can become one graph node. Libraries are integrated as plugins. Runtime objects are created through Hypodermic DI. Graph editing UI is built with Dear ImGui and imnodes.

## Layers

- Application: CLI, config loading, bootstrap, DI build, execution start.
- Core: interfaces and value objects only.
- Runtime: node catalog, graph builder, executor, data packets, Python subprocess runtime.
- Services: logging, database, artifact store, metrics.
- Plugins: OpenCV, Python ML, and custom function-to-node adapters.
- Presentation: Dear ImGui + imnodes graph editor, fixed selected-node property panel, and optional menu-controlled windows.
- Runtime graph: DAG validation, pointer-based `ValueBase` transfer, and parallel execution through a bounded worker pool.

## Plugin Contract

Each plugin should provide:

- CMake target and plugin option.
- DI registration function/module.
- Node descriptors.
- Node factories.
- Implementations hidden behind core interfaces.
- Library-specific function introspection that emits common `FunctionDescriptor` objects.

## Function to Node Contract

Use a shared `FunctionDescriptor -> NodeDescriptor` generator in the foundation/runtime layer. Keep header parsing, Python reflection, and manifest override collection inside each plugin adapter. Define one `ILibraryFunctionIntrospector` implementation per plugin by default. Do not rely on raw function signatures as the only source of truth.

Large plugins should expose one plugin facade introspector and split internally into plugin-specific resolver/policy interfaces for raw functions, types, enums, flags, overloads, parameters, and manifest overrides.

## Node Contract

Each node should define:

- Stable ID, for example `opencv.resize`.
- Input ports.
- Output ports.
- Parameter schema.
- Factory key.
- Implementation dependencies injected by DI.
- UI metadata for compact imnodes rendering.

Infer `Source`, `Transform`, and `Sink` from input/output ports when possible, but store the final node kind explicitly in `NodeDescriptor`.

## UI Contract

Each imnodes node should show:

- Title: `<Library>: <function>()`.
- Output image preview when available.
- Compact parameter list.
- Input slots for image inputs.
- Output slots for image outputs.

Keep detailed edits in the fixed left property panel. Show logs and secondary tools only when enabled from the menu bar.

## Graph Runtime Contract

Use `ValueBase` as the root for node payloads. Images should be represented by an `ImageValue` with byte buffer, width, height, channels, stride, and pixel format. Pass values between nodes by smart pointer. Use immutable shared values by default and allow in-place mutation only when graph analysis proves exclusive ownership.

Initial graph execution should assume a DAG, validate types and cycles before running, compute ready nodes, and run independent nodes on a bounded worker pool.

## Python Subprocess Contract

Use subprocesses for PyTorch and other Python ML libraries. Prefer JSON-like structured control messages and file-path artifact transfer at first. Add shared memory only after profiling shows the need.
