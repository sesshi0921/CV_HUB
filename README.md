# CV_HUB

## 導入方法

### 前提

- CMake 3.24 以上
- C++20 対応コンパイラ
- OpenCV
- spdlog
- GUI をビルドする場合: GLFW、OpenGL
- Git

macOS + Homebrew の CLI 依存例:

```bash
brew install cmake opencv spdlog glfw
```

GUI 依存の Dear ImGui と imnodes は、CMake package が見つからない場合に CMake が FetchContent で取得します。`CVHUB_FETCH_GUI_DEPS=OFF` かつ imnodes package が無い場合だけ、最小互換 shim でビルドします。

### 取得

```bash
git clone <repository-url> CV_HUB
cd CV_HUB
```

### 設定

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

任意のプラグインだけを有効化する場合は、`CVHUB_BUILD_PLUGIN_*` オプションを指定します。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCVHUB_BUILD_PLUGIN_OPENCV=ON
```

GUI 依存を入れていない環境では CLI のみビルドできます。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCVHUB_BUILD_GUI=OFF
```

ネットワークなしで GUI をビルドする場合は、Dear ImGui と imnodes を CMake package として事前に導入してください。

### ビルド

```bash
cmake --build build --config Release
```

## 実行方法

```bash
./build/apps/cvhub/cvhub
```

ノード一覧を表示する場合:

```bash
./build/apps/cvhub/cvhub --list-nodes
```

OpenCV サンプルパイプラインを実行する場合:

```bash
./build/apps/cvhub/cvhub --run-sample --log-file cvhub.log
```

GUI を起動する場合:

```bash
./build/apps/cvhub_gui/cvhub_gui
```

テストを実行する場合:

```bash
ctest --test-dir build --output-on-failure
```
