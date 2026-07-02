# Copilot Instructions

## 言語

- レビューコメント・提案・説明はすべて **日本語** で記述すること。
- コード中の識別子・コメントは英語でよい。

## レビュー観点

### 共通（全言語）

- スタイルガイドの原則を優先する。
- 命名は明確・一貫・意図が伝わること。
- 関数・クラスは単一責任原則（SRP）に従うこと。
- マジックナンバー・マジック文字列は定数化すること。
- 深いネストは早期リターン／ガード節で解消すること。
- 例外・エラーを握り潰さないこと（`catch(...){}` / `except: pass` 禁止）。

### C++ — Google C++ Style Guide

- 命名規則に従うこと（クラス: `UpperCamelCase`、変数/関数: `lower_snake_case`、定数: `kConstantName`）。
- `#include` の順序を守ること（関連ヘッダ → C系 → C++標準 → サードパーティ → プロジェクト内）。
- `using namespace` をヘッダファイルで使用しないこと。
- 生ポインタより `std::unique_ptr` / `std::shared_ptr` を優先すること。
- コピーコンストラクタ・代入演算子は明示的に定義するか `= delete` すること。
- `explicit` を単引数コンストラクタに付けること。
- `const` を積極的に使用すること（引数・メンバ関数・変数）。
- ゼロ警告を維持すること（`-Wall -Wextra`）。

## CV_HUB プラグイン設計パターン

新しいライブラリのプラグインを追加する場合は以下のパターンに従うこと。

### 原則
- プラグインのC++ソースは「ライブラリにどんな関数があるか」を知らない。
- 関数一覧はビルド時に `tools/gen_<lib>_nodes.py` がヘッダを解析して `_auto_nodes.cpp` を生成する。
- 手書きが許されるのは **statefulなノード**（ファイルI/O・デバイス・キャッシュ）と **sourceノード** のみ。

### ファイル構成
```
plugins/<lib>/CMakeLists.txt         # add_custom_command で gen スクリプトを呼ぶ
plugins/<lib>/src/<lib>_helpers.hpp  # imageToMat / matToImage 相当 (inline)
plugins/<lib>/src/<lib>_plugin.cpp   # stateful/source のみ手書き
plugins/<lib>/tools/gen_nodes.py     # ヘッダ解析 → auto_nodes.cpp 生成（ライブラリ固有）
```

### 参照実装
- `plugins/opencv/tools/gen_nodes.py`
- `plugins/opencv/src/opencv_helpers.hpp`
- `plugins/opencv/src/opencv_plugin.cpp`
- `plugins/opencv/CMakeLists.txt`

---

### Python — PEP 8

- インデント: スペース4つ。
- 命名規則に従うこと（関数/変数: `snake_case`、クラス: `UpperCamelCase`、定数: `UPPER_SNAKE_CASE`）。
- 1行の長さは 79 文字以内（ドキュメント文字列は 72 文字）。
- `import` はファイル先頭にまとめ、標準 → サードパーティ → ローカルの順に空行で区切ること。
- 型ヒント（`typing` / PEP 526）を使用すること。
- `f-string` を優先すること（`%` フォーマット・`.format()` より）。
