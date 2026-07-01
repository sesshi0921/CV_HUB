# AGENTS.md

## Project Context

CV_HUB is a C++20 image-processing pipeline platform. Treat image-processing library functions as graph nodes, group library integrations as plugins, wire runtime dependencies through a Hypodermic DI container, and expose graph editing through Dear ImGui + imnodes.

The primary architecture reference is `docs/ARCHITECTURE.md`. Keep README limited to installation and execution steps.

## Development Rules

- Prefer interface-first design under `include/cvhub/...`.
- Keep implementations under `src/...` or `plugins/<name>/src/...`.
- Do not leak OpenCV, PyTorch, or other library-specific types into core interfaces.
- Register dependencies through Hypodermic modules instead of constructing services directly in application code.
- Treat each plugin as a build-time selectable CMake target with its own `CMakeLists.txt`.
- Register library functions as `NodeDescriptor` + `INodeFactory` pairs.
- Generate nodes through a shared `FunctionDescriptor -> NodeDescriptor` path where possible.
- Keep `FunctionNodeGenerator` in the foundation/runtime layer.
- Define one library-specific `ILibraryFunctionIntrospector` implementation per plugin unless a plugin is large enough to justify internal sub-introspectors.
- Instantiate runtime nodes through factories resolved from DI.
- Use Python subprocess boundaries for PyTorch and other Python ML libraries.
- Build the graph editor UI with Dear ImGui and imnodes.
- Render UI from `NodeDescriptor`, `PortDescriptor`, parameter schemas, and runtime preview state.
- Keep detailed node editing in the fixed left property panel; keep imnodes node bodies compact.
- Make log, metrics, DB, plugin inspector, and artifact windows optional menu-controlled windows.
- Keep plugin code depending inward on core/runtime interfaces; core must not depend on plugins.
- Use `ValueBase`-derived values for node inputs and outputs.
- Pass node values by smart pointer to avoid unnecessary image buffer copies.
- Treat graph execution as DAG-based by default; validate cycles and types before running.
- Execute independent graph nodes in parallel through a bounded worker pool.
- Keep repository protection guidance in `docs/REPOSITORY_RULES.md`.

## Expected Structure

Follow the proposed structure in `docs/ARCHITECTURE.md` unless implementation constraints force a change. If the structure changes, update that document and this file in the same change.

## Build Expectations

Use out-of-tree CMake builds:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Plugin switches should follow the `CVHUB_BUILD_PLUGIN_<NAME>` naming style.

## Testing Expectations

- Add unit tests for core graph validation, node catalog behavior, and plugin registration.
- Add integration tests for at least one OpenCV pipeline and one Python subprocess pipeline when those layers exist.
- For Python worker changes, test both successful responses and structured failure responses.

## Repository Safety

- Treat `main` as protected after the initial architecture checkpoint.
- Prefer PR-based changes after the checkpoint.
- Do not force-push protected branches.
- Keep branch protection and contribution rules aligned with `docs/REPOSITORY_RULES.md`.

## Codex Workflow

- Use the project-local `$cv-hub-dev` skill when changing architecture, CMake layout, plugin registration, node design, or Python subprocess behavior.
- Before editing architecture-sensitive code, read `docs/ARCHITECTURE.md`.
- Keep documentation and code aligned. Architecture changes should include a doc update.
