<h1 align="center">
  <p align="center">
    <img src="docs/assets/favicon.png" alt="CV_HUB Logo" width="128">
    <br>CV_HUB
  </p>
</h1>

<p align="center">
  Visual image-processing pipelines with OpenCV, Python/ML workers, and node graphs.
  <br />
  A C++20 desktop workspace for building, editing, saving, and running computer-vision pipelines.
  <br />
  <a href="#about">About</a>
  ·
  <a href="#download">Download</a>
  ·
  <a href="docs/index.html">Documentation</a>
  ·
  <a href="#building-from-source">Building from Source</a>
  ·
  <a href="#launching">Launching</a>
</p>

## About

CV_HUB is a C++20 image-processing pipeline platform. It treats image-processing library functions as graph nodes, groups library integrations as build-time selectable plugins, and exposes graph editing through Dear ImGui and imnodes.

The current implementation focuses on:

- OpenCV function nodes generated from headers at build time
- A shared `FunctionDescriptor -> NodeDescriptor` path
- Runtime node execution through descriptors and factories
- JSON graph save/load from the GUI
- macOS app bundle support with a native application icon
- GitHub Pages documentation under `docs/`

## Download

Prebuilt packages are not available yet.

Planned targets:

- Windows
- macOS
- Linux

For now, build from source using the steps below.

## Documentation

The documentation site is located in [`docs/`](docs/index.html).

Run it locally with:

```bash
cd /path/to/CV_HUB
python3 -m http.server 8000 --directory docs
```

Then open:

```text
http://localhost:8000/index.html
```

## Building from Source

### Requirements

- CMake 3.24 or newer
- A C++20-capable compiler
- OpenCV
- spdlog
- Git
- For the GUI: GLFW and OpenGL
- Optional: `uv` for the Python/PyTorch plugin environment

On macOS with Homebrew:

```bash
brew install cmake opencv spdlog glfw
```

If Homebrew commands are not available in your shell, add Homebrew to `PATH` first:

```bash
export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:$PATH"
```

Homebrew-enabled macOS builds automatically prepend `brew --prefix` to `CMAKE_PREFIX_PATH` during CMake configuration.

### Python environment

If the PyTorch plugin is enabled, prepare the Python environment before running CMake:

```bash
uv sync --project .
```

If needed, specify the Python executable explicitly:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCVHUB_PYTORCH_PYTHON="$PWD/.venv/bin/python3"
```

### Configure

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

Useful options:

```bash
# Build without the GUI
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCVHUB_BUILD_GUI=OFF

# Build only selected plugins
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCVHUB_BUILD_PLUGIN_OPENCV=ON
```

GUI dependencies Dear ImGui and imnodes are fetched with CMake FetchContent when CMake packages are not available and `CVHUB_FETCH_GUI_DEPS=ON`.

### Build

```bash
cmake --build build --config Release
```

## Launching

### CLI

```bash
./build/apps/cvhub/cvhub
```

List available nodes:

```bash
./build/apps/cvhub/cvhub --list-nodes
```

Run the OpenCV sample pipeline:

```bash
./build/apps/cvhub/cvhub --run-sample --log-file cvhub.log
```

### GUI

macOS:

```bash
open build/apps/cvhub_gui/CV_HUB.app
```

Linux / Windows or other non-bundled builds:

```bash
./build/apps/cvhub_gui/CV_HUB
```

On macOS, `build/apps/cvhub_gui/CV_HUB.app` is the correct launch target. Starting an old raw executable directly can bypass the application icon and native file-picker integration.

## Testing

```bash
ctest --test-dir build --output-on-failure
```

## Contributing and Developing

See the project architecture and repository rules before changing core architecture, plugin registration, node execution, or GUI behavior:

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- [`docs/REPOSITORY_RULES.md`](docs/REPOSITORY_RULES.md)

## Roadmap and Status

| # | Step | Status |
| :-: | --- | :---: |
| 1 | Core node descriptors, values, and runtime execution | ✅ |
| 2 | OpenCV plugin with generated nodes | ✅ |
| 3 | Dear ImGui / imnodes graph editor | ✅ |
| 4 | JSON save/load and native file picker integration | ✅ |
| 5 | macOS app bundle icon and launch path | ✅ |
| 6 | Public documentation site under `docs/` | ✅ |
| 7 | Prebuilt Windows/macOS/Linux packages | 🚧 |
