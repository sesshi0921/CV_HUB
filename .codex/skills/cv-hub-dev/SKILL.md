---
name: cv-hub-dev
description: CV_HUB project guidance for C++20 image-processing pipeline architecture, Hypodermic DI registration, CMake plugin layout, node descriptors/factories, OpenCV plugin nodes, Python subprocess ML nodes, Dear ImGui/imnodes graph UI, and architecture documentation updates. Use when modifying CV_HUB architecture, plugin registration, node model, runtime pipeline execution, ImGui/imnodes UI, CMake structure, or project documentation.
---

# CV_HUB Dev

## Overview

Use this skill to keep CV_HUB changes aligned with its planned architecture: C++20 core interfaces, Hypodermic-based dependency injection, build-time selectable plugins, node-graph execution, Dear ImGui/imnodes graph editing, and Python subprocess support for ML libraries.

## Required Context

Before making architecture-sensitive changes, read:

- `docs/ARCHITECTURE.md`
- `AGENTS.md`
- `references/architecture.md` inside this skill when a compact reminder is enough

## Design Rules

- Keep core interfaces free of library-specific types.
- Split interfaces and implementations.
- Put plugin-specific conversion, validation, and function wrapping inside each plugin.
- Treat one image-processing function as one node unless the function is only an internal helper.
- Use a shared `FunctionDescriptor -> NodeDescriptor` generator for function-to-node conversion when possible.
- Keep `FunctionNodeGenerator` as a foundation/runtime implementation class.
- Define one library-specific `ILibraryFunctionIntrospector` implementation per plugin unless the plugin is large enough to need internal sub-introspectors.
- Keep library-specific signature parsing, reflection, and manifest override collection inside plugin adapters.
- Register nodes as descriptors plus factories.
- Resolve services through Hypodermic instead of manual construction in application code.
- Keep runtime pipeline execution independent of concrete plugins.
- Use Python subprocesses for PyTorch and other Python ML integrations.
- Build graph editing UI with Dear ImGui and imnodes.
- Keep UI dependent on descriptors, graph state, preview state, and interfaces rather than concrete node implementations.
- Use `ValueBase`-derived values for node inputs and outputs.
- Pass values between nodes by smart pointer and avoid unnecessary image buffer copies.
- Validate graph type compatibility before execution.
- Execute independent DAG nodes in parallel through a bounded worker pool.
- Keep README limited to install and run steps.

## Change Workflow

1. Inspect the existing tree and relevant docs.
2. Identify the layer being changed: application, core, runtime, services, or plugin.
3. Add or update interfaces first when introducing new behavior.
4. Add implementation in the correct layer.
5. Register dependencies through a module/bootstrap path.
6. Add focused tests for graph validation, node registration, execution, or subprocess behavior.
7. Update `docs/ARCHITECTURE.md` if the architecture or file structure changes.

## CMake Guidance

- Use out-of-tree builds.
- Give each plugin its own `CMakeLists.txt`.
- Gate plugin builds with `CVHUB_BUILD_PLUGIN_<NAME>` options.
- Prefer static plugin registration first for cross-platform predictability.
- Keep a future path open for shared-library plugin loading through an `IPluginLoader` implementation.

## Node Guidance

Each node should have:

- Stable node ID such as `opencv.resize`.
- Node kind such as `Source`, `Transform`, `Sink`, or `Utility`.
- Input and output port descriptors.
- Parameter schema and validation.
- Factory registration.
- Implementation that depends on interfaces, not global state.

Infer node kind from ports where possible, but store the final kind explicitly in `NodeDescriptor`. UI and runtime should trust descriptors, not raw function signatures.

## Graph Runtime Guidance

- Model node input/output payloads as `ValueBase` derivatives.
- Use `ImageValue` for image buffers with width, height, channel count, stride, and pixel format.
- Connect node outputs to downstream inputs with `std::shared_ptr<const ValueBase>`.
- Make shared downstream values immutable.
- Allow mutable or in-place processing only when graph analysis proves exclusive consumption.
- Treat the initial graph model as a DAG.
- Compute ready nodes from dependency analysis and run independent nodes on a bounded thread pool.
- Add descriptor-level resource constraints for non-thread-safe nodes, Python subprocess nodes, GPU nodes, or exclusive resources.

## Function to Node Guidance

Do not assume fully automatic node generation is reliable across all libraries. Use this shape:

```text
library-specific function definition
  -> LibraryFunctionIntrospector
  -> FunctionDescriptor
  -> FunctionNodeGenerator
  -> NodeDescriptor + factory/scaffold
```

Use manifests as overrides for semantics that signatures cannot express, such as OpenCV `InputArray`/`OutputArray`, overload selection, UI-important parameters, value ranges, and artifact types.

Foundation/runtime owns:

- `IFunctionNodeGenerator`
- `FunctionNodeGenerator`
- common `FunctionDescriptor` models
- common generation diagnostics
- conversion from function descriptors to node descriptors, ports, parameters, and UI metadata

Each library plugin owns:

- one `ILibraryFunctionIntrospector` implementation by default
- library-specific parsing/reflection
- library-specific type-to-semantic mapping
- filtering public functions
- overload/template policy

## UI Guidance

For Dear ImGui + imnodes changes:

- Use `library:functionName()` as the node title, for example `OpenCV: resize()`.
- Put output image preview, compact parameter list, input slots, and output slots inside the imnodes node.
- Omit input slots for source nodes and output slots for sink nodes.
- Keep the selected node property window fixed on the left.
- Put log, metrics, DB, plugin inspector, and artifact viewer windows behind menu-bar toggles.
- Keep node rendering separate from execution, DI, and plugin registration.

## Python ML Guidance

For PyTorch or other ML libraries:

- Keep Python dependencies outside the C++ core.
- Communicate through a controlled subprocess runtime.
- Use structured request/response messages.
- Pass large artifacts by file path first; optimize later only when performance demands it.
- Test process failure, invalid response, timeout, and normal execution paths.
