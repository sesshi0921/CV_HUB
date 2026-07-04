#pragma once

#include <optional>
#include <string>

namespace cvhub::gui {

std::optional<std::string> openJsonFileDialog(const std::string& currentPath);
std::optional<std::string> saveJsonFileDialog(const std::string& currentPath);
void applyNativeAppIcon();

}  // namespace cvhub::gui
