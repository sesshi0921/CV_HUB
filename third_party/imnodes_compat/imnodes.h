#pragma once

#include <imgui.h>

namespace ImNodes {

struct ImNodesEmulateThreeButtonMouse {
  const bool* Modifier{nullptr};
};

struct ImNodesIO {
  ImNodesEmulateThreeButtonMouse EmulateThreeButtonMouse;
  int AltMouseButton{2};
  float AutoPanningSpeed{8.0f};
};

struct ImNodesStyle {
  float GridSpacing{32.0f};
  float NodeCornerRounding{4.0f};
  float NodePaddingHorizontal{8.0f};
  float NodePaddingVertical{8.0f};
  float NodeBorderThickness{1.0f};
  float LinkThickness{3.0f};
  float LinkLineSegmentsPerLength{0.1f};
  float LinkHoverDistance{10.0f};
  float PinCircleRadius{4.0f};
  float PinQuadSideLength{7.0f};
  float PinTriangleSideLength{9.5f};
  float PinLineThickness{1.0f};
  float PinHoverRadius{10.0f};
  float PinOffset{0.0f};
};

ImNodesIO& GetIO();
ImNodesStyle& GetStyle();

void CreateContext();
void DestroyContext();
void BeginNodeEditor();
void EndNodeEditor();
void SetNodeGridSpacePos(int nodeId, ImVec2 position);
ImVec2 GetNodeGridSpacePos(int nodeId);
void BeginNode(int nodeId);
void EndNode();
void BeginNodeTitleBar();
void EndNodeTitleBar();
void BeginInputAttribute(int attributeId);
void EndInputAttribute();
void BeginOutputAttribute(int attributeId);
void EndOutputAttribute();
void Link(int linkId, int startAttributeId, int endAttributeId);
int NumSelectedNodes();
void GetSelectedNodes(int* nodeIds);
bool IsLinkCreated(int* startedAtAttributeId, int* endedAtAttributeId);
bool IsLinkDestroyed(int* linkId);
ImVec2 EditorContextGetPanning();
void EditorContextResetPanning(const ImVec2& pos);

}  // namespace ImNodes
