# CV_HUB — Claude Code Project Instructions

## プラグイン設計パターン（最重要）

### 原則
- プラグインのソースは「どの関数があるか」を知らない。
- 関数の一覧は **ビルド時に対象ライブラリのヘッダを解析して自動生成** する。
- 手書きが許されるのは **statefulなノード**（キャッシュ/デバイス/ファイルI/O等）と **sourceノード** のみ。

### 新しいライブラリプラグインを作る手順

```
plugins/<libname>/
  CMakeLists.txt          ← add_custom_command で gen_nodes.py を呼ぶ
  include/cvhub/plugins/<libname>/<libname>_plugin.hpp
  src/
    <libname>_helpers.hpp ← imageToMat/matToImage相当のヘルパー（inline）
    <libname>_plugin.cpp  ← stateful/sourceノードのみ手書き
                             getAutoDescriptors() / getAutoRegistry() を呼ぶ

plugins/<libname>/tools/gen_nodes.py   ← ヘッダ解析 → *_auto_nodes.cpp 生成（ライブラリ固有）
```

### `plugins/<libname>/tools/gen_nodes.py` の責務
- **ライブラリ固有**のスクリプト。他のプラグインと共有しない。
- 指定ヘッダ（`--headers`）からそのライブラリのexportマクロ付き関数を抽出
- `(ImageInput src, ImageOutput dst, scalar_params...)` パターンにマッチする関数を選択
- 各関数について以下を生成:
  - `FunctionDescriptor` エントリ（introspector用）
  - `FactoryFn` エントリ（`GenericImageTransformFactory` + lambda）
- 出力: `getAutoDescriptors()` + `getAutoRegistry()` を含む `.cpp`

### CMakeLists.txt パターン
```cmake
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GEN_HEADERS ...)
set(GEN_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/<libname>_auto_nodes.cpp)
add_custom_command(
    OUTPUT  ${GEN_OUTPUT}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_nodes.py
            --headers ${GEN_HEADERS} --output ${GEN_OUTPUT}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_nodes.py ${GEN_HEADERS}
)
target_sources(<target> PRIVATE src/<libname>_plugin.cpp ${GEN_OUTPUT})
```

### `_plugin.cpp` の登録フロー
```cpp
// generated declarations
std::vector<FunctionDescriptor> getAutoDescriptors();
std::unordered_map<std::string, FactoryFn> getAutoRegistry();

void register<Lib>Plugin(ServiceContainer& services) {
    // 手書き: stateful/source のみ
    std::unordered_map<std::string, FactoryFn> kRegistry = { /* ... */ };

    // auto-generated をマージ（手書きが優先）
    for (auto& [k, v] : getAutoRegistry()) kRegistry.emplace(k, std::move(v));

    auto allDesc = manualIntrospector.inspect();
    auto autoDesc = getAutoDescriptors();
    allDesc.insert(allDesc.end(),
        std::make_move_iterator(autoDesc.begin()),
        std::make_move_iterator(autoDesc.end()));

    const auto nodes = services.functionNodeGenerator()->generate(allDesc);
    for (const auto& node : nodes) {
        if (auto it = kRegistry.find(node.factoryKey); it != kRegistry.end())
            services.nodeCatalog()->registerNode(node, it->second(node));
    }
}
```

### 参照実装
- Generator: `plugins/opencv/tools/gen_nodes.py`
- Helpers:   `plugins/opencv/src/opencv_helpers.hpp`
- Plugin:    `plugins/opencv/src/opencv_plugin.cpp`
- CMake:     `plugins/opencv/CMakeLists.txt`
