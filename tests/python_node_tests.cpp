#include "cvhub/core/values.hpp"
#include "cvhub/runtime/generic_node.hpp"
#include "cvhub/runtime/python_runtime.hpp"

// helpers under test (header-only, so include directly)
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "../plugins/pytorch/src/pytorch_helpers.hpp"

namespace {

// ---- test helpers ----------------------------------------------------------

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

std::shared_ptr<cvhub::ImageValue> makeImage(int w, int h, int c, cvhub::PixelFormat fmt) {
    std::vector<std::byte> bytes(static_cast<std::size_t>(w * h * c), std::byte{128});
    return std::make_shared<cvhub::ImageValue>(w, h, c, w * c, fmt, std::move(bytes));
}

// ---- MockPythonRuntime -----------------------------------------------------

struct RecordedCall {
    std::string fn;
    cvhub::ParameterMap params;
    int w, h, c;
    cvhub::PixelFormat fmt;
    std::vector<std::byte> imageBytes;
};

class MockPythonRuntime final : public cvhub::IPythonRuntime {
  public:
    Response nextResponse;
    std::optional<RecordedCall> lastCall;

    Response call(const Request& req) override {
        lastCall = RecordedCall{req.fn, req.params, req.w, req.h, req.c, req.fmt, req.imageBytes};
        return nextResponse;
    }
    void shutdown() override {}
};

cvhub::IPythonRuntime::Response successResponse(int w, int h, int c, cvhub::PixelFormat fmt) {
    std::vector<std::byte> bytes(static_cast<std::size_t>(w * h * c), std::byte{200});
    return {true, "", w, h, c, fmt, std::move(bytes)};
}

// Build a GenericImageTransformFactory backed by MockPythonRuntime
std::shared_ptr<cvhub::INodeFactory> makeFactory(std::shared_ptr<MockPythonRuntime> runtime,
                                                 const std::string& fnName) {
    using namespace cvhub;
    using namespace cvhub::plugins::pytorch;

    NodeDescriptor desc{
        .id = "pytorch." + fnName,
        .displayName = fnName,
        .library = "pytorch",
        .functionName = fnName,
        .kind = NodeKind::Transform,
        .inputs = {{"image", "Image", SemanticType::Image, true}},
        .outputs = {{"image", "Image", SemanticType::Image, true}},
    };

    ImageTransformFn fn = [runtime,
                           fnName](std::shared_ptr<const ImageValue> input,
                                   const ParameterMap& params) -> std::shared_ptr<ImageValue> {
        return responseToImage(runtime->call(imageToRequest(fnName, *input, params)));
    };

    return std::make_shared<GenericImageTransformFactory>(desc, std::move(fn));
}

// ---- helper unit tests -----------------------------------------------------

void test_imageToRequest_metadata() {
    using namespace cvhub::plugins::pytorch;
    const auto img = makeImage(8, 6, 3, cvhub::PixelFormat::BGR8);
    cvhub::ParameterMap params{{"k", 3}};
    const auto req = imageToRequest("gaussian_blur", *img, params);

    require(req.fn == "gaussian_blur", "fn name");
    require(req.w == 8, "w");
    require(req.h == 6, "h");
    require(req.c == 3, "c");
    require(req.fmt == cvhub::PixelFormat::BGR8, "fmt");
    require(req.imageBytes.size() == 8 * 6 * 3, "image bytes size");
    require(req.params.count("k") == 1, "params forwarded");
}

void test_responseToImage_success() {
    using namespace cvhub::plugins::pytorch;
    const auto resp = successResponse(10, 20, 1, cvhub::PixelFormat::Gray8);
    const auto img = responseToImage(resp);

    require(img->width() == 10, "width");
    require(img->height() == 20, "height");
    require(img->channels() == 1, "channels");
    require(img->pixelFormat() == cvhub::PixelFormat::Gray8, "fmt");
}

void test_responseToImage_throws_on_error() {
    using namespace cvhub::plugins::pytorch;
    cvhub::IPythonRuntime::Response resp{false, "bad things happened", 0, 0, 0, {}, {}};
    bool threw = false;
    try {
        responseToImage(resp);
    } catch (const std::runtime_error& e) {
        threw = true;
        require(std::string(e.what()).find("bad things happened") != std::string::npos,
                "error message should propagate");
    }
    require(threw, "should throw on error response");
}

void test_imageToRequest_tight_packs_strided_image() {
    using namespace cvhub::plugins::pytorch;
    std::vector<std::byte> bytes{
        std::byte{1},  std::byte{2},  std::byte{3}, std::byte{4}, std::byte{5}, std::byte{6},
        std::byte{0},  std::byte{0},  std::byte{7}, std::byte{8}, std::byte{9}, std::byte{10},
        std::byte{11}, std::byte{12}, std::byte{0}, std::byte{0},
    };
    cvhub::ImageValue img(2, 2, 3, 8, cvhub::PixelFormat::RGB8, std::move(bytes));

    const auto req = imageToRequest("resize", img, {});
    const std::vector<std::byte> expected{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},  std::byte{5},  std::byte{6},
        std::byte{7}, std::byte{8}, std::byte{9}, std::byte{10}, std::byte{11}, std::byte{12},
    };
    require(req.imageBytes == expected, "image bytes should be tightly packed");
}

#if defined(__unix__) || defined(__APPLE__)
void test_python_runtime_unescapes_error_string() {
    namespace fs = std::filesystem;
    const fs::path worker = fs::temp_directory_path() / "cvhub_python_runtime_error_worker.py";
    std::ofstream out(worker);
    out << R"(#!/usr/bin/env python3
import json
import struct
import sys

header_len_raw = sys.stdin.buffer.read(4)
if len(header_len_raw) == 4:
    (header_len,) = struct.unpack("<I", header_len_raw)
    sys.stdin.buffer.read(header_len)
    error_text = "line1\n\"quoted\"\nline2 " + "\\"
    payload = json.dumps({"ok": False, "error": error_text}).encode("utf-8")
    sys.stdout.buffer.write(struct.pack("<I", len(payload)))
    sys.stdout.buffer.write(payload)
    sys.stdout.buffer.flush()
)";
    out.close();

    const char* pythonPath = std::getenv("PYTHON");
    if (pythonPath == nullptr || *pythonPath == '\0') {
        pythonPath = "/usr/bin/python3";
    }
    cvhub::PythonSubprocessRuntime runtime(pythonPath, worker.string());
    cvhub::IPythonRuntime::Request req;
    req.fn = "noop";
    req.w = 1;
    req.h = 1;
    req.c = 1;
    req.fmt = cvhub::PixelFormat::Gray8;
    req.imageBytes = {std::byte{0}};

    const auto resp = runtime.call(req);
    require(!resp.ok, "response should be an error");
    require(resp.error.find("line1\n\"quoted\"\nline2") != std::string::npos,
            "escaped newlines and quotes should be preserved");
    require(!resp.error.empty() && resp.error.back() == '\\',
            "escaped trailing backslash should be preserved");
}
#endif

// ---- integration tests via GenericImageTransformFactory --------------------

void test_factory_creates_node() {
    auto runtime = std::make_shared<MockPythonRuntime>();
    runtime->nextResponse = successResponse(4, 4, 3, cvhub::PixelFormat::RGB8);
    const auto node = makeFactory(runtime, "resize")->create();
    require(node != nullptr, "create() non-null");
    require(node->descriptor().id == "pytorch.resize", "descriptor id");
}

void test_execute_sends_fn_name_and_metadata() {
    auto runtime = std::make_shared<MockPythonRuntime>();
    runtime->nextResponse = successResponse(4, 4, 3, cvhub::PixelFormat::RGB8);

    auto node = makeFactory(runtime, "hflip")->create();
    cvhub::NodeExecutionContext ctx;
    ctx.inputs["image"] = makeImage(4, 4, 3, cvhub::PixelFormat::RGB8);
    node->execute(ctx);

    require(runtime->lastCall.has_value(), "runtime called");
    require(runtime->lastCall->fn == "hflip", "fn name");
    require(runtime->lastCall->w == 4, "w forwarded");
    require(runtime->lastCall->fmt == cvhub::PixelFormat::RGB8, "fmt forwarded");
}

void test_execute_passes_parameters() {
    auto runtime = std::make_shared<MockPythonRuntime>();
    runtime->nextResponse = successResponse(2, 2, 3, cvhub::PixelFormat::RGB8);

    auto node = makeFactory(runtime, "adjust_brightness")->create();
    cvhub::NodeExecutionContext ctx;
    ctx.inputs["image"] = makeImage(2, 2, 3, cvhub::PixelFormat::RGB8);
    ctx.parameters["brightness_factor"] = 1.5;
    node->execute(ctx);

    const auto& p = runtime->lastCall->params;
    require(p.count("brightness_factor") == 1, "param forwarded");
    require(std::get<double>(p.at("brightness_factor")) == 1.5, "param value");
}

void test_execute_builds_output_image() {
    auto runtime = std::make_shared<MockPythonRuntime>();
    runtime->nextResponse = successResponse(16, 8, 3, cvhub::PixelFormat::RGB8);

    auto node = makeFactory(runtime, "resize")->create();
    cvhub::NodeExecutionContext ctx;
    ctx.inputs["image"] = makeImage(4, 4, 3, cvhub::PixelFormat::RGB8);
    node->execute(ctx);

    const auto out = cvhub::valueCast<cvhub::ImageValue>(ctx.outputs.at("image"));
    require(out != nullptr, "output is ImageValue");
    require(out->width() == 16, "output width");
    require(out->height() == 8, "output height");
}

void test_execute_no_output_on_missing_input() {
    // GenericImageTransformNode silently skips when input is absent
    auto runtime = std::make_shared<MockPythonRuntime>();
    runtime->nextResponse = successResponse(4, 4, 3, cvhub::PixelFormat::RGB8);

    auto node = makeFactory(runtime, "resize")->create();
    cvhub::NodeExecutionContext ctx; // no input
    node->execute(ctx);

    require(!runtime->lastCall.has_value(), "runtime should NOT be called when input missing");
    require(ctx.outputs.empty(), "no output produced");
}

void test_boundary_1x1_gray() {
    auto runtime = std::make_shared<MockPythonRuntime>();
    runtime->nextResponse = successResponse(1, 1, 1, cvhub::PixelFormat::Gray8);

    auto node = makeFactory(runtime, "autocontrast")->create();
    cvhub::NodeExecutionContext ctx;
    ctx.inputs["image"] = makeImage(1, 1, 1, cvhub::PixelFormat::Gray8);
    node->execute(ctx);

    const auto out = cvhub::valueCast<cvhub::ImageValue>(ctx.outputs.at("image"));
    require(out->width() == 1 && out->height() == 1, "1x1 round-trip");
}

} // namespace

int main() {
    struct Test {
        const char* name;
        void (*fn)();
    };
    const Test tests[] = {
        {"imageToRequest_metadata", test_imageToRequest_metadata},
        {"responseToImage_success", test_responseToImage_success},
        {"responseToImage_throws_on_error", test_responseToImage_throws_on_error},
        {"imageToRequest_tight_packs_strided_image", test_imageToRequest_tight_packs_strided_image},
#if defined(__unix__) || defined(__APPLE__)
        {"python_runtime_unescapes_error_string", test_python_runtime_unescapes_error_string},
#endif
        {"factory_creates_node", test_factory_creates_node},
        {"execute_sends_fn_name_and_metadata", test_execute_sends_fn_name_and_metadata},
        {"execute_passes_parameters", test_execute_passes_parameters},
        {"execute_builds_output_image", test_execute_builds_output_image},
        {"execute_no_output_on_missing_input", test_execute_no_output_on_missing_input},
        {"boundary_1x1_gray", test_boundary_1x1_gray},
    };

    int failed = 0;
    for (const auto& t : tests) {
        try {
            t.fn();
            std::cout << "[PASS] " << t.name << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[FAIL] " << t.name << ": " << e.what() << "\n";
            ++failed;
        }
    }
    return failed > 0 ? 1 : 0;
}
