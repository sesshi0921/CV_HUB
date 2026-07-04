# AGENTS.md

## Project Context

CV_HUB is a C++20 image-processing pipeline platform. Treat image-processing library functions as graph nodes, group library integrations as plugins, wire runtime dependencies through a Hypodermic DI container, and expose graph editing through Dear ImGui + imnodes.

The primary architecture reference is `docs/ARCHITECTURE.md`. Keep README limited to installation and execution steps.

## Plugin Auto-Generation Pattern（最重要）

新しいライブラリプラグインを追加するときは以下の設計に従うこと。

### 原則
プラグインのC++ソースは「そのライブラリにどんな関数があるか」を知らない。
ビルド時に Python スクリプトがヘッダを解析し、ノード登録コードを自動生成する。

### ファイル構成
```
plugins/<lib>/CMakeLists.txt           # add_custom_command でスクリプトを呼ぶ
plugins/<lib>/src/<lib>_helpers.hpp    # 型変換ヘルパー (copyBytes / toImage / fromImage) [inline]
plugins/<lib>/src/<lib>_plugin.cpp     # stateful / source ノードのみ手書き
plugins/<lib>/tools/gen_nodes.py       # ヘッダ解析 → *_auto_nodes.cpp 生成（ライブラリ固有）
```

### gen スクリプトの出力 (2関数)
- `getAutoDescriptors()` → `std::vector<FunctionDescriptor>`
- `getAutoRegistry()` → `std::unordered_map<std::string, FactoryFn>`

### CMakeLists.txt パターン
```cmake
add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/<lib>_auto_nodes.cpp
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_nodes.py
            --headers <header_paths> --output <output>
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_nodes.py <header_paths>
)
target_sources(<target> PRIVATE src/<lib>_plugin.cpp ${GEN_OUTPUT})
```

### plugin.cpp 統合パターン
```cpp
std::unordered_map<std::string, FactoryFn> kRegistry = { /* stateful/source only */ };
for (auto& [k, v] : getAutoRegistry()) kRegistry.emplace(k, std::move(v));

auto allDesc = introspector.inspect();
auto autoDesc = getAutoDescriptors();
allDesc.insert(allDesc.end(), std::make_move_iterator(autoDesc.begin()),
                              std::make_move_iterator(autoDesc.end()));

for (const auto& node : services.functionNodeGenerator()->generate(allDesc))
    if (auto it = kRegistry.find(node.factoryKey); it != kRegistry.end())
        services.nodeCatalog()->registerNode(node, it->second(node));
```

### 参照実装
- `plugins/opencv/tools/gen_nodes.py`
- `plugins/opencv/src/opencv_helpers.hpp`
- `plugins/opencv/src/opencv_plugin.cpp`
- `plugins/opencv/CMakeLists.txt`

---

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
