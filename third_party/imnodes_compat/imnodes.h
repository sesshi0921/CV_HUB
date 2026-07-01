#pragma once

struct ImVec2;

namespace ImNodes {

void CreateContext();
void DestroyContext();
void BeginNodeEditor();
void EndNodeEditor();
void SetNodeGridSpacePos(int nodeId, ImVec2 position);
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

} // namespace ImNodes
