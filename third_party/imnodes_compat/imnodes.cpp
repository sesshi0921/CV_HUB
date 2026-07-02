#include "imnodes.h"

#include <imgui.h>

namespace ImNodes {

namespace {
ImNodesIO gIO;
ImNodesStyle gStyle;
}  // namespace

ImNodesIO& GetIO() { return gIO; }
ImNodesStyle& GetStyle() { return gStyle; }

void CreateContext() {}
void DestroyContext() {}

void BeginNodeEditor() {
  ImGui::BeginChild("imnodes-compat-editor", ImVec2(0.0f, 0.0f), false,
                    ImGuiWindowFlags_HorizontalScrollbar);
}

void EndNodeEditor() { ImGui::EndChild(); }

void SetNodeGridSpacePos(int, ImVec2) {}

ImVec2 GetNodeGridSpacePos(int) { return ImVec2(0.0f, 0.0f); }

void BeginNode(int nodeId) {
  ImGui::PushID(nodeId);
  ImGui::BeginGroup();
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
}

void EndNode() {
  ImGui::PopStyleVar();
  ImGui::EndGroup();
  ImGui::Separator();
  ImGui::PopID();
}

void BeginNodeTitleBar() {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.72f, 0.44f, 1.0f));
}

void EndNodeTitleBar() { ImGui::PopStyleColor(); }

void BeginInputAttribute(int attributeId) {
  ImGui::PushID(attributeId);
  ImGui::TextUnformatted(">");
  ImGui::SameLine();
}

void EndInputAttribute() { ImGui::PopID(); }

void BeginOutputAttribute(int attributeId) { ImGui::PushID(attributeId); }

void EndOutputAttribute() { ImGui::PopID(); }

void Link(int, int, int) {}

int NumSelectedNodes() { return 0; }

void GetSelectedNodes(int*) {}

bool IsLinkCreated(int*, int*) { return false; }

bool IsLinkDestroyed(int*) { return false; }

ImVec2 EditorContextGetPanning() { return ImVec2(0.0f, 0.0f); }

void EditorContextResetPanning(const ImVec2&) {}

}  // namespace ImNodes
