# CV_HUB

## 導入方法

### 前提

- CMake 3.24 以上
- C++20 対応コンパイラ
- Python 3.10 以上
- Git

### 取得

```bash
git clone <repository-url> CV_HUB
cd CV_HUB
```

### 設定

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

任意のプラグインだけを有効化する場合は、将来的に追加される `CVHUB_BUILD_PLUGIN_*` オプションを指定します。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCVHUB_BUILD_PLUGIN_OPENCV=ON
```

### ビルド

```bash
cmake --build build --config Release
```

## 実行方法

```bash
./build/apps/cvhub/cvhub
```

設定ファイルを指定する場合:

```bash
./build/apps/cvhub/cvhub --config config/pipeline.yaml
```
