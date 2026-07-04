#include "file_dialog.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shobjidl.h>
#include <windows.h>
#endif

namespace cvhub::gui {
namespace {

std::string trim(std::string value) {
  while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) value.pop_back();
  std::size_t first = 0;
  while (first < value.size() && (value[first] == ' ' || value[first] == '\t')) ++first;
  return value.substr(first);
}

std::string runCommandCapture(const std::string& command) {
  std::array<char, 256> buffer{};
  std::string output;
#if defined(_WIN32)
  FILE* pipe = _popen(command.c_str(), "r");
#else
  FILE* pipe = popen(command.c_str(), "r");
#endif
  if (!pipe) return {};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) output += buffer.data();
#if defined(_WIN32)
  _pclose(pipe);
#else
  pclose(pipe);
#endif
  return trim(output);
}

std::string escapeDoubleQuotes(std::string value) {
  std::size_t pos = 0;
  while ((pos = value.find('"', pos)) != std::string::npos) {
    value.replace(pos, 1, "\\\"");
    pos += 2;
  }
  return value;
}

#ifdef _WIN32
std::wstring widen(const std::string& value) {
  if (value.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), size);
  if (!out.empty() && out.back() == L'\0') out.pop_back();
  return out;
}

std::string narrow(const std::wstring& value) {
  if (value.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), size, nullptr, nullptr);
  if (!out.empty() && out.back() == '\0') out.pop_back();
  return out;
}

std::optional<std::string> windowsDialog(bool saveDialog, const std::string& currentPath) {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return std::nullopt;
  std::optional<std::string> result;
  IFileDialog* dialog = nullptr;
  const CLSID clsid = saveDialog ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
  const IID iid = saveDialog ? IID_IFileSaveDialog : IID_IFileOpenDialog;
  if (SUCCEEDED(CoCreateInstance(clsid, nullptr, CLSCTX_ALL, iid, reinterpret_cast<void**>(&dialog)))) {
    COMDLG_FILTERSPEC filters[] = {{L"JSON Files", L"*.json"}};
    dialog->SetFileTypes(1, filters);
    dialog->SetDefaultExtension(L"json");
    dialog->SetTitle(saveDialog ? L"Save Graph JSON" : L"Load Graph JSON");
    if (!currentPath.empty()) dialog->SetFileName(widen(std::filesystem::path(currentPath).filename().string()).c_str());
    if (SUCCEEDED(dialog->Show(nullptr))) {
      IShellItem* item = nullptr;
      if (SUCCEEDED(dialog->GetResult(&item))) {
        PWSTR path = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
          result = narrow(path);
          CoTaskMemFree(path);
        }
        item->Release();
      }
    }
    dialog->Release();
  }
  if (SUCCEEDED(hr)) CoUninitialize();
  if (saveDialog && result) {
    std::filesystem::path p{*result};
    if (p.extension() != ".json") p += ".json";
    result = p.string();
  }
  return result;
}
#endif

std::optional<std::string> linuxDialog(bool saveDialog, const std::string& currentPath) {
  const auto escaped = escapeDoubleQuotes(currentPath);
  std::ostringstream zenity;
  zenity << "zenity --file-selection " << (saveDialog ? "--save --confirm-overwrite " : "")
         << "--file-filter='JSON files | *.json' ";
  if (!escaped.empty()) zenity << "--filename=\"" << escaped << "\" ";
  zenity << "2>/dev/null";
  auto selected = runCommandCapture(zenity.str());
  if (selected.empty()) {
    std::ostringstream kdialog;
    kdialog << "kdialog " << (saveDialog ? "--getsavefilename " : "--getopenfilename ")
            << "\"" << escaped << "\" \"*.json|JSON files\" 2>/dev/null";
    selected = runCommandCapture(kdialog.str());
  }
  if (selected.empty()) return std::nullopt;
  std::filesystem::path p{selected};
  if (saveDialog && p.extension() != ".json") p += ".json";
  return p.string();
}

}  // namespace

std::optional<std::string> openJsonFileDialog(const std::string& currentPath) {
#ifdef _WIN32
  return windowsDialog(false, currentPath);
#else
  return linuxDialog(false, currentPath);
#endif
}

std::optional<std::string> saveJsonFileDialog(const std::string& currentPath) {
#ifdef _WIN32
  return windowsDialog(true, currentPath);
#else
  return linuxDialog(true, currentPath);
#endif
}

void applyNativeAppIcon() {}

}  // namespace cvhub::gui
