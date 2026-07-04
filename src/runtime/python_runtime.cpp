#include "cvhub/runtime/python_runtime.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace cvhub {

namespace {

static void writeAll(int fd, const void* data, std::size_t size) {
    const char* ptr = static_cast<const char*>(data);
    while (size > 0) {
        ssize_t n = ::write(fd, ptr, size);
        if (n <= 0)
            throw std::runtime_error("write() to Python worker failed");
        ptr += n;
        size -= static_cast<std::size_t>(n);
    }
}

static void readAll(int fd, void* data, std::size_t size) {
    char* ptr = static_cast<char*>(data);
    while (size > 0) {
        ssize_t n = ::read(fd, ptr, size);
        if (n <= 0)
            throw std::runtime_error("read() from Python worker failed");
        ptr += n;
        size -= static_cast<std::size_t>(n);
    }
}

static std::string fmtToStr(PixelFormat fmt) {
    switch (fmt) {
    case PixelFormat::RGB8:
        return "RGB8";
    case PixelFormat::BGR8:
        return "BGR8";
    case PixelFormat::Gray8:
        return "Gray8";
    }
    return "RGB8";
}

static PixelFormat strToFmt(const std::string& s) {
    if (s == "BGR8")
        return PixelFormat::BGR8;
    if (s == "Gray8")
        return PixelFormat::Gray8;
    return PixelFormat::RGB8;
}

static std::string escapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') {
            out += "\\\"";
        } else if (c == '\\') {
            out += "\\\\";
        } else {
            out += c;
        }
    }
    return out;
}

struct ParamJsonEncoder {
    std::string operator()(int v) {
        return std::to_string(v);
    }
    std::string operator()(double v) {
        return std::to_string(v);
    }
    std::string operator()(bool v) {
        return v ? "true" : "false";
    }
    std::string operator()(const std::string& v) {
        return "\"" + escapeJson(v) + "\"";
    }
};

static std::string buildRequestJson(const IPythonRuntime::Request& req) {
    std::ostringstream ss;
    ss << "{\"fn\":\"" << escapeJson(req.fn) << "\",\"params\":{";
    bool first = true;
    for (const auto& [k, v] : req.params) {
        if (!first)
            ss << ",";
        ss << "\"" << escapeJson(k) << "\":" << std::visit(ParamJsonEncoder{}, v);
        first = false;
    }
    ss << "},\"w\":" << req.w << ",\"h\":" << req.h << ",\"c\":" << req.c << ",\"fmt\":\""
       << fmtToStr(req.fmt) << "\"}";
    return ss.str();
}

static std::size_t findJsonValuePos(const std::string& json, const std::string& key) {
    const std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos)
        return std::string::npos;
    pos += search.size();
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos >= json.size() || json[pos] != ':')
        return std::string::npos;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    return pos;
}

static std::string extractJsonString(const std::string& json, const std::string& key) {
    auto pos = findJsonValuePos(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') {
        return {};
    }
    ++pos;

    std::string value;
    bool escaped = false;
    for (; pos < json.size(); ++pos) {
        const char c = json[pos];
        if (escaped) {
            switch (c) {
            case '"':
            case '\\':
            case '/':
                value += c;
                break;
            case 'b':
                value += '\b';
                break;
            case 'f':
                value += '\f';
                break;
            case 'n':
                value += '\n';
                break;
            case 'r':
                value += '\r';
                break;
            case 't':
                value += '\t';
                break;
            default:
                value += c;
                break;
            }
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"')
            return value;
        value += c;
    }

    return {};
}

static int extractJsonInt(const std::string& json, const std::string& key) {
    auto pos = findJsonValuePos(json, key);
    if (pos == std::string::npos)
        return 0;
    return std::stoi(json.substr(pos));
}

static bool extractJsonBool(const std::string& json, const std::string& key) {
    auto pos = findJsonValuePos(json, key);
    if (pos == std::string::npos)
        return false;
    return json.substr(pos, 4) == "true";
}

static IPythonRuntime::Response parseResponse(const std::string& json, int fd) {
    IPythonRuntime::Response resp;
    resp.ok = extractJsonBool(json, "ok");
    if (!resp.ok) {
        resp.error = extractJsonString(json, "error");
        return resp;
    }
    resp.w = extractJsonInt(json, "w");
    resp.h = extractJsonInt(json, "h");
    resp.c = extractJsonInt(json, "c");
    resp.fmt = strToFmt(extractJsonString(json, "fmt"));
    const std::size_t imgSize = static_cast<std::size_t>(resp.w) * resp.h * resp.c;
    resp.imageBytes.resize(imgSize);
    readAll(fd, resp.imageBytes.data(), imgSize);
    return resp;
}

} // namespace

PythonSubprocessRuntime::PythonSubprocessRuntime(std::string pythonPath, std::string workerPath)
    : pythonPath_(std::move(pythonPath)), workerPath_(std::move(workerPath)) {}

PythonSubprocessRuntime::~PythonSubprocessRuntime() {
    shutdown();
}

void PythonSubprocessRuntime::ensureStarted() {
    if (started_)
        return;

    // Prevent SIGPIPE from terminating the process if the worker dies
    ::signal(SIGPIPE, SIG_IGN);

    int stdinPipe[2], stdoutPipe[2];
    if (::pipe(stdinPipe) < 0 || ::pipe(stdoutPipe) < 0) {
        throw std::runtime_error("Failed to create pipes for Python worker");
    }

    workerPid_ = static_cast<int>(::fork());
    if (workerPid_ < 0) {
        throw std::runtime_error("fork() failed for Python worker");
    }

    if (workerPid_ == 0) {
        ::dup2(stdinPipe[0], STDIN_FILENO);
        ::dup2(stdoutPipe[1], STDOUT_FILENO);
        ::close(stdinPipe[0]);
        ::close(stdinPipe[1]);
        ::close(stdoutPipe[0]);
        ::close(stdoutPipe[1]);
        ::execl(pythonPath_.c_str(), pythonPath_.c_str(), workerPath_.c_str(), nullptr);
        ::_exit(127);
    }

    ::close(stdinPipe[0]);
    ::close(stdoutPipe[1]);
    toWorkerFd_ = stdinPipe[1];
    fromWorkerFd_ = stdoutPipe[0];
    started_ = true;
}

IPythonRuntime::Response PythonSubprocessRuntime::call(const Request& req) {
    std::scoped_lock lock(mutex_);
    ensureStarted();

    const std::string headerJson = buildRequestJson(req);
    const uint32_t headerLen = static_cast<uint32_t>(headerJson.size());
    writeAll(toWorkerFd_, &headerLen, sizeof(headerLen));
    writeAll(toWorkerFd_, headerJson.data(), headerJson.size());
    writeAll(toWorkerFd_, req.imageBytes.data(), req.imageBytes.size());

    uint32_t respLen = 0;
    readAll(fromWorkerFd_, &respLen, sizeof(respLen));
    std::string respJson(respLen, '\0');
    readAll(fromWorkerFd_, respJson.data(), respLen);

    return parseResponse(respJson, fromWorkerFd_);
}

void PythonSubprocessRuntime::shutdown() {
    if (!started_)
        return;
    if (toWorkerFd_ >= 0) {
        ::close(toWorkerFd_);
        toWorkerFd_ = -1;
    }
    if (fromWorkerFd_ >= 0) {
        ::close(fromWorkerFd_);
        fromWorkerFd_ = -1;
    }
    if (workerPid_ > 0) {
        ::waitpid(static_cast<pid_t>(workerPid_), nullptr, 0);
        workerPid_ = -1;
    }
    started_ = false;
}

} // namespace cvhub
