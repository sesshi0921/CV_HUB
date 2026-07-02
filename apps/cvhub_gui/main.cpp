#define GL_SILENCE_DEPRECATION

// Swizzle constants not always exposed by macOS gl3.h
#ifndef GL_TEXTURE_SWIZZLE_R
#define GL_TEXTURE_SWIZZLE_R 0x8E42
#define GL_TEXTURE_SWIZZLE_G 0x8E43
#define GL_TEXTURE_SWIZZLE_B 0x8E44
#define GL_TEXTURE_SWIZZLE_A 0x8E45
#endif

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <nlohmann/json.hpp>

#include "cvhub/app/application.hpp"

#if __has_include(<imnodes.h>)
#include <imnodes.h>
#else
#include <imnodes/imnodes.h>
#endif

#if __has_include(<backends/imgui_impl_glfw.h>)
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#elif __has_include(<imgui/backends/imgui_impl_glfw.h>)
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#else
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Pipeline worker — runs the pipeline on a dedicated thread so the ImGui
// render loop is never blocked by node execution.
// ---------------------------------------------------------------------------

struct PipelineWorker {
  std::thread thread;
  std::mutex graphMtx;
  cvhub::PipelineGraph pendingGraph;
  bool graphDirty{false};
  std::condition_variable graphCv;

  std::mutex resultMtx;
  cvhub::PipelineRunResult latestResult;
  bool hasResult{false};

  std::atomic<bool> active{false};

  void start(std::shared_ptr<cvhub::IPipelineExecutor> exec,
             cvhub::PipelineGraph initial) {
    active = true;
    {
      std::lock_guard lk(graphMtx);
      pendingGraph = std::move(initial);
      graphDirty = true;
    }
    thread = std::thread([this, exec = std::move(exec)]() {
      cvhub::PipelineGraph graph;
      while (active) {
        {
          std::unique_lock lk(graphMtx);
          graphCv.wait(lk, [this] { return graphDirty || !active; });
          if (!active) break;
          graph = pendingGraph;
          graphDirty = false;
        }
        while (active) {
          {
            std::lock_guard lk(graphMtx);
            if (graphDirty) break;
          }
          auto result = exec->run(graph);
          {
            std::lock_guard lk(resultMtx);
            latestResult = std::move(result);
            hasResult = true;
          }
        }
      }
    });
  }

  void stop() {
    active = false;
    graphCv.notify_all();
    if (thread.joinable()) thread.join();
  }

  void updateGraph(cvhub::PipelineGraph g) {
    {
      std::lock_guard lk(graphMtx);
      pendingGraph = std::move(g);
      graphDirty = true;
    }
    graphCv.notify_one();
  }

  bool poll(cvhub::PipelineRunResult& out) {
    std::lock_guard lk(resultMtx);
    if (!hasResult) return false;
    out = std::move(latestResult);
    hasResult = false;
    return true;
  }
};

struct GuiNodeState {
  cvhub::GraphNode node;
  cvhub::NodeDescriptor descriptor;
  ImVec2 position;
  bool positionSet{false};
  bool running{true};
  bool previewEnabled{true};
};

struct GuiGraphState {
  std::vector<GuiNodeState> nodes;
  std::vector<cvhub::GraphEdge> edges;
  int selectedIndex{0};
  float zoom{1.0f};
};

struct NodePreviewTexture {
  GLuint id{0};
  int width{0};
  int height{0};
};

void uploadNodePreviews(
    const cvhub::PipelineRunResult& result,
    std::unordered_map<std::string, NodePreviewTexture>& previews) {
  for (const auto& [instanceId, ports] : result.outputs) {
    for (const auto& [portId, valuePtr] : ports) {
      const auto img = cvhub::valueCast<cvhub::ImageValue>(valuePtr);
      if (!img) continue;

      auto& tex = previews[instanceId];
      if (tex.id == 0) glGenTextures(1, &tex.id);
      glBindTexture(GL_TEXTURE_2D, tex.id);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      const int bpp = img->channels();
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glPixelStorei(GL_UNPACK_ROW_LENGTH,
                    img->strideBytes() / std::max(1, bpp));
      const auto* data = reinterpret_cast<const GLubyte*>(img->bytes().data());

      if (img->channels() == 1) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, img->width(), img->height(), 0,
                     GL_RED, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
      } else if (img->pixelFormat() == cvhub::PixelFormat::BGR8) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img->width(), img->height(), 0,
                     GL_BGR, GL_UNSIGNED_BYTE, data);
      } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img->width(), img->height(), 0,
                     GL_RGB, GL_UNSIGNED_BYTE, data);
      }

      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      glBindTexture(GL_TEXTURE_2D, 0);
      tex.width = img->width();
      tex.height = img->height();
      break;  // first output port only
    }
  }
}

void cleanupPreviews(
    std::unordered_map<std::string, NodePreviewTexture>& previews) {
  for (auto& [id, tex] : previews) {
    if (tex.id != 0) {
      glDeleteTextures(1, &tex.id);
      tex.id = 0;
    }
  }
  previews.clear();
}

void applyTheme() {
  ImGui::StyleColorsDark();
  auto& style = ImGui::GetStyle();
  style.WindowRounding = 3.0f;
  style.FrameRounding = 3.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 3.0f;

  auto& colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.13f, 0.14f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.16f, 0.17f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.82f, 0.36f, 0.10f, 0.70f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.92f, 0.45f, 0.15f, 0.85f);
  colors[ImGuiCol_HeaderActive] = ImVec4(1.00f, 0.50f, 0.18f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.82f, 0.36f, 0.10f, 0.85f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.92f, 0.45f, 0.15f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.50f, 0.18f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.50f, 0.18f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.92f, 0.45f, 0.15f, 1.00f);
  colors[ImGuiCol_Tab] = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.82f, 0.36f, 0.10f, 0.80f);
  colors[ImGuiCol_TabActive] = ImVec4(0.32f, 0.25f, 0.21f, 1.00f);
}

// ---------------------------------------------------------------------------
// Toggle switch widget
// ---------------------------------------------------------------------------

bool ToggleSwitch(const char* id, bool* value) {
  constexpr float kW = 28.0f, kH = 14.0f, kR = kH * 0.5f;
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, ImVec2(kW, kH));
  const bool clicked = ImGui::IsItemClicked();
  if (clicked) *value = !*value;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImU32 bg =
      *value ? IM_COL32(38, 139, 210, 255) : IM_COL32(80, 80, 85, 255);
  dl->AddRectFilled(pos, ImVec2(pos.x + kW, pos.y + kH), bg, kR);
  const float knobX = *value ? pos.x + kW - kR : pos.x + kR;
  dl->AddCircleFilled(ImVec2(knobX, pos.y + kR), kR - 2.0f,
                      IM_COL32(255, 255, 255, 230));
  return clicked;
}

// ---------------------------------------------------------------------------
// Graph serialization / deserialization
// ---------------------------------------------------------------------------

using json = nlohmann::json;

json serializeParam(const cvhub::ParameterValue& v) {
  return std::visit(
      [](const auto& val) -> json {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, int>)
          return {{"t", "i"}, {"v", val}};
        else if constexpr (std::is_same_v<T, double>)
          return {{"t", "d"}, {"v", val}};
        else if constexpr (std::is_same_v<T, bool>)
          return {{"t", "b"}, {"v", val}};
        else
          return {{"t", "s"}, {"v", val}};
      },
      v);
}

cvhub::ParameterValue deserializeParam(const json& j) {
  const auto t = j.at("t").get<std::string>();
  if (t == "i") return j.at("v").get<int>();
  if (t == "d") return j.at("v").get<double>();
  if (t == "b") return j.at("v").get<bool>();
  return j.at("v").get<std::string>();
}

void saveGraph(const std::string& path, const GuiGraphState& state) {
  json nodesArr = json::array();
  for (const auto& n : state.nodes) {
    json params = json::object();
    for (const auto& [k, v] : n.node.parameters) params[k] = serializeParam(v);
    nodesArr.push_back({
        {"instanceId", n.node.instanceId},
        {"nodeId", n.node.nodeId},
        {"x", n.position.x},
        {"y", n.position.y},
        {"running", n.running},
        {"parameters", params},
    });
  }
  json edgesArr = json::array();
  for (const auto& e : state.edges) {
    edgesArr.push_back({
        {"fromNode", e.fromNode},
        {"fromPort", e.fromPort},
        {"toNode", e.toNode},
        {"toPort", e.toPort},
    });
  }
  std::ofstream out(path);
  out << json{{"nodes", nodesArr}, {"edges", edgesArr}}.dump(2);
}

GuiGraphState loadGraph(const std::string& path,
                        const std::shared_ptr<cvhub::INodeCatalog>& catalog) {
  std::ifstream in(path);
  const json j = json::parse(in);
  GuiGraphState state;
  for (const auto& jn : j.at("nodes")) {
    const auto nodeId = jn.at("nodeId").get<std::string>();
    const auto descriptor = catalog->descriptor(nodeId);
    cvhub::GraphNode node;
    node.instanceId = jn.at("instanceId").get<std::string>();
    node.nodeId = nodeId;
    for (const auto& p : descriptor.parameters)
      node.parameters[p.name] = p.defaultValue;
    for (const auto& [k, v] : jn.at("parameters").items())
      node.parameters[k] = deserializeParam(v);
    GuiNodeState gs;
    gs.node = std::move(node);
    gs.descriptor = descriptor;
    gs.position = ImVec2(jn.at("x").get<float>(), jn.at("y").get<float>());
    gs.running = jn.at("running").get<bool>();
    state.nodes.push_back(std::move(gs));
  }
  for (const auto& je : j.at("edges")) {
    state.edges.push_back({
        .fromNode = je.at("fromNode").get<std::string>(),
        .fromPort = je.at("fromPort").get<std::string>(),
        .toNode = je.at("toNode").get<std::string>(),
        .toPort = je.at("toPort").get<std::string>(),
    });
  }
  return state;
}

cvhub::ParameterValue defaultParameterValue(
    const cvhub::ParameterDescriptor& parameter) {
  return parameter.defaultValue;
}

GuiGraphState createGuiGraphState(
    const cvhub::PipelineGraph& graph,
    const std::shared_ptr<cvhub::INodeCatalog>& catalog) {
  GuiGraphState state;
  state.edges = graph.edges;

  const ImVec2 positions[] = {ImVec2(40.0f, 80.0f), ImVec2(300.0f, 80.0f),
                              ImVec2(560.0f, 80.0f), ImVec2(820.0f, 80.0f)};
  for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
    auto graphNode = graph.nodes[i];
    const auto descriptor = catalog->descriptor(graphNode.nodeId);
    for (const auto& parameter : descriptor.parameters) {
      graphNode.parameters.try_emplace(parameter.name,
                                       defaultParameterValue(parameter));
    }
    state.nodes.push_back(GuiNodeState{
        .node = std::move(graphNode),
        .descriptor = descriptor,
        .position = i < std::size(positions)
                        ? positions[i]
                        : ImVec2(40.0f + static_cast<float>(i) * 260.0f, 80.0f),
    });
  }

  return state;
}

cvhub::PipelineGraph buildPipelineGraph(const GuiGraphState& state) {
  cvhub::PipelineGraph graph;
  std::unordered_set<std::string> runningIds;
  for (const auto& node : state.nodes) {
    if (node.running) {
      graph.nodes.push_back(node.node);
      runningIds.insert(node.node.instanceId);
    }
  }
  for (const auto& edge : state.edges) {
    if (runningIds.count(edge.fromNode) && runningIds.count(edge.toNode)) {
      graph.edges.push_back(edge);
    }
  }
  return graph;
}

int nodeUiId(std::size_t nodeIndex) { return static_cast<int>(nodeIndex + 1); }

int inputAttributeId(std::size_t nodeIndex, std::size_t portIndex) {
  return static_cast<int>((nodeIndex + 1) * 1000 + 100 + portIndex);
}

int outputAttributeId(std::size_t nodeIndex, std::size_t portIndex) {
  return static_cast<int>((nodeIndex + 1) * 1000 + 200 + portIndex);
}

int findNodeIndex(const GuiGraphState& state,
                  const cvhub::NodeInstanceId& instanceId) {
  const auto it =
      std::ranges::find_if(state.nodes, [&](const GuiNodeState& node) {
        return node.node.instanceId == instanceId;
      });
  return it == state.nodes.end()
             ? -1
             : static_cast<int>(std::distance(state.nodes.begin(), it));
}

int findInputPortIndex(const cvhub::NodeDescriptor& descriptor,
                       const cvhub::PortId& portId) {
  const auto it = std::ranges::find_if(
      descriptor.inputs,
      [&](const cvhub::PortDescriptor& port) { return port.id == portId; });
  return it == descriptor.inputs.end()
             ? -1
             : static_cast<int>(std::distance(descriptor.inputs.begin(), it));
}

int findOutputPortIndex(const cvhub::NodeDescriptor& descriptor,
                        const cvhub::PortId& portId) {
  const auto it = std::ranges::find_if(
      descriptor.outputs,
      [&](const cvhub::PortDescriptor& port) { return port.id == portId; });
  return it == descriptor.outputs.end()
             ? -1
             : static_cast<int>(std::distance(descriptor.outputs.begin(), it));
}

void renderNodeList(const std::vector<cvhub::NodeDescriptor>& nodes,
                    GuiGraphState& graphState, float panelWidth, bool autoRun,
                    std::unordered_map<std::string, int>& instanceCounters) {
  ImGui::BeginChild("node-list", ImVec2(panelWidth, 0.0f), true);
  ImGui::TextUnformatted("Nodes");
  ImGui::Separator();

  // Group by library
  std::map<std::string, std::vector<const cvhub::NodeDescriptor*>> byLibrary;
  for (const auto& node : nodes) {
    byLibrary[node.library].push_back(&node);
  }

  auto renderItem = [&](const cvhub::NodeDescriptor* nd) {
    ImGui::Selectable(nd->displayName.c_str(), false);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
      const std::string funcKey = nd->library + "." + nd->functionName;
      const int idx = instanceCounters[funcKey]++;
      cvhub::GraphNode newNode;
      newNode.instanceId =
          nd->library + ":" + nd->functionName + "()-" + std::to_string(idx);
      newNode.nodeId = nd->id;
      for (const auto& param : nd->parameters) {
        newNode.parameters.emplace(param.name, param.defaultValue);
      }
      GuiNodeState gs;
      gs.node = std::move(newNode);
      gs.descriptor = *nd;
      gs.position =
          ImVec2(100.0f + static_cast<float>(idx % 5) * 50.0f, 200.0f);
      gs.running = !autoRun;  // new nodes added during run start stopped
      gs.previewEnabled = true;
      graphState.nodes.push_back(std::move(gs));
    }
    ImGui::Spacing();
  };

  for (const auto& [lib, libNodes] : byLibrary) {
    if (ImGui::CollapsingHeader(lib.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
      for (const auto* nd : libNodes) renderItem(nd);
    }
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Graph");
  for (const auto& graphNode : graphState.nodes) {
    ImGui::BulletText("%s", graphNode.node.instanceId.c_str());
  }
  ImGui::EndChild();
}

void renderGraph(
    GuiGraphState& state, float reserveRight,
    const std::unordered_map<std::string, NodePreviewTexture>& previews) {
  ImGui::BeginChild("graph-editor", ImVec2(-reserveRight, 0.0f), true);

  // Scroll-wheel zoom: rescale node positions around view center
  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                             ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
      const float newZoom =
          std::clamp(state.zoom * std::pow(1.12f, wheel), 0.15f, 4.0f);
      if (std::abs(newZoom - state.zoom) > 0.001f) {
        const float factor = newZoom / state.zoom;
        const ImVec2 panning = ImNodes::EditorContextGetPanning();
        const ImVec2 winSz = ImGui::GetContentRegionAvail();
        const ImVec2 center{-panning.x + winSz.x * 0.5f,
                            -panning.y + winSz.y * 0.5f};
        for (auto& node : state.nodes) {
          node.position.x = center.x + (node.position.x - center.x) * factor;
          node.position.y = center.y + (node.position.y - center.y) * factor;
          node.positionSet = false;
        }
        state.zoom = newZoom;
      }
    }
  }
  ImNodes::GetStyle().GridSpacing = 32.0f * state.zoom;

  // Query hover/selection state outside BeginNodeEditor (scope constraint)
  std::unordered_set<int> selectedNodeUiIds;
  {
    const int n = ImNodes::NumSelectedNodes();
    if (n > 0) {
      std::vector<int> ids(static_cast<std::size_t>(n));
      ImNodes::GetSelectedNodes(ids.data());
      selectedNodeUiIds.insert(ids.begin(), ids.end());
    }
  }
  int hoveredNodeId = -1;
  ImNodes::IsNodeHovered(&hoveredNodeId);

  ImNodes::BeginNodeEditor();

  for (std::size_t i = 0; i < state.nodes.size(); ++i) {
    auto& graphNode = state.nodes[i];
    const int uiNodeId = nodeUiId(i);
    if (!graphNode.positionSet) {
      ImNodes::SetNodeGridSpacePos(uiNodeId, graphNode.position);
      graphNode.positionSet = true;
    }

    // Orange outline for selected nodes
    const bool isSelected = selectedNodeUiIds.count(uiNodeId) > 0;
    if (isSelected) {
      ImNodes::PushColorStyle(ImNodesCol_NodeOutline,
                              IM_COL32(255, 140, 0, 255));
    }
    // Title bar color: blue for running, gray for stopped
    if (graphNode.running) {
      ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(49, 108, 176, 255));
      ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                              IM_COL32(65, 130, 200, 255));
      ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                              IM_COL32(80, 150, 220, 255));
    } else {
      ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(70, 70, 75, 255));
      ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                              IM_COL32(90, 90, 95, 255));
      ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                              IM_COL32(110, 110, 115, 255));
    }

    constexpr float kNodeWidth = 220.0f;
    constexpr float kToggleW = 28.0f;

    const bool hasImageOutput = std::any_of(
        graphNode.descriptor.outputs.begin(),
        graphNode.descriptor.outputs.end(),
        [](const auto& p) { return p.type == cvhub::SemanticType::Image; });

    ImNodes::BeginNode(uiNodeId);
    ImNodes::BeginNodeTitleBar();

    // Title text + right-aligned toggle switch
    ImGui::Dummy(ImVec2(kNodeWidth, 0.0f));
    const char* titleStr = graphNode.descriptor.ui.title.empty()
                               ? graphNode.descriptor.displayName.c_str()
                               : graphNode.descriptor.ui.title.c_str();
    ImGui::TextUnformatted(titleStr);
    if (hasImageOutput) {
      ImGui::SameLine(kNodeWidth - kToggleW);
      ImGui::PushID(static_cast<int>(i) * 100 + 1);
      ToggleSwitch("##pt", &graphNode.previewEnabled);
      ImGui::PopID();
    }

    ImNodes::EndNodeTitleBar();

    // Image preview in body
    if (hasImageOutput && graphNode.previewEnabled) {
      const auto texIt = previews.find(graphNode.node.instanceId);
      if (texIt != previews.end() && texIt->second.id != 0) {
        const auto& tex = texIt->second;
        const float aspect =
            static_cast<float>(tex.height) / static_cast<float>(tex.width);
        ImGui::Image(static_cast<ImTextureID>(tex.id),
                     ImVec2{kNodeWidth, kNodeWidth * aspect});
      } else {
        constexpr float kAspect = 9.0f / 16.0f;
        const ImVec2 sz{kNodeWidth, kNodeWidth * kAspect};
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::Dummy(sz);
        ImGui::GetWindowDrawList()->AddRectFilled(
            p0, ImVec2{p0.x + sz.x, p0.y + sz.y}, IM_COL32(30, 30, 35, 210),
            2.0f);
      }
    }

    // Input / Output ports on the same row
    const auto& inputs = graphNode.descriptor.inputs;
    const auto& outputs = graphNode.descriptor.outputs;
    const std::size_t portRows = std::max(inputs.size(), outputs.size());
    for (std::size_t p = 0; p < portRows; ++p) {
      const float inW =
          p < inputs.size()
              ? ImGui::CalcTextSize(inputs[p].displayName.c_str()).x
              : 0.0f;
      const float outW =
          p < outputs.size()
              ? ImGui::CalcTextSize(outputs[p].displayName.c_str()).x
              : 0.0f;
      const float gap = std::max(kNodeWidth - inW - outW - 8.0f, 8.0f);

      if (p < inputs.size()) {
        ImNodes::BeginInputAttribute(inputAttributeId(i, p));
        ImGui::TextUnformatted(inputs[p].displayName.c_str());
        ImNodes::EndInputAttribute();
      } else {
        ImGui::Dummy(ImVec2(1.0f, ImGui::GetTextLineHeightWithSpacing()));
      }
      if (p < outputs.size()) {
        ImGui::SameLine(0.0f, gap);
        ImNodes::BeginOutputAttribute(outputAttributeId(i, p));
        ImGui::TextUnformatted(outputs[p].displayName.c_str());
        ImNodes::EndOutputAttribute();
      }
    }

    // Editable parameters
    if (!graphNode.descriptor.parameters.empty()) {
      {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(p, ImVec2{p.x + kNodeWidth, p.y},
                                            IM_COL32(80, 80, 85, 255), 1.0f);
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
      }
      ImGui::PushItemWidth(kNodeWidth);
      for (const auto& param : graphNode.descriptor.parameters) {
        auto it = graphNode.node.parameters.find(param.name);
        if (it == graphNode.node.parameters.end()) continue;
        const std::string widgetId =
            "##" + param.name + graphNode.node.instanceId;
        if (!param.options.empty()) {
          std::string* sv = std::get_if<std::string>(&it->second);
          if (sv) {
            std::string preview = *sv;
            for (const auto& opt : param.options) {
              if (opt.id == *sv) {
                preview = opt.displayName;
                break;
              }
            }
            ImGui::TextDisabled("%s", param.displayName.c_str());
            if (ImGui::BeginCombo(widgetId.c_str(), preview.c_str())) {
              for (const auto& opt : param.options) {
                const bool selected = (opt.id == *sv);
                if (ImGui::Selectable(opt.displayName.c_str(), selected)) {
                  *sv = opt.id;
                }
                if (selected) ImGui::SetItemDefaultFocus();
              }
              ImGui::EndCombo();
            }
          }
        } else if (auto* iv = std::get_if<int>(&it->second)) {
          const int minV =
              param.minValue ? static_cast<int>(*param.minValue) : 0;
          const int maxV =
              param.maxValue ? static_cast<int>(*param.maxValue) : 100;
          ImGui::DragInt(widgetId.c_str(), iv, 1.0f, minV, maxV,
                         (param.displayName + ": %d").c_str());
        } else if (auto* dv = std::get_if<double>(&it->second)) {
          float fv = static_cast<float>(*dv);
          const float minV =
              param.minValue ? static_cast<float>(*param.minValue) : 0.0f;
          const float maxV =
              param.maxValue ? static_cast<float>(*param.maxValue) : 1.0f;
          if (ImGui::DragFloat(widgetId.c_str(), &fv, 0.01f, minV, maxV,
                               (param.displayName + ": %.2f").c_str())) {
            *dv = static_cast<double>(fv);
          }
        } else if (auto* bv = std::get_if<bool>(&it->second)) {
          ImGui::Checkbox((param.displayName + widgetId).c_str(), bv);
        } else if (auto* strv = std::get_if<std::string>(&it->second)) {
          char buf[256]{};
          std::snprintf(buf, sizeof(buf), "%s", strv->c_str());
          ImGui::TextDisabled("%s", param.displayName.c_str());
          if (ImGui::InputText(widgetId.c_str(), buf, sizeof(buf))) {
            *strv = buf;
          }
        }
      }
      ImGui::PopItemWidth();
    }

    ImNodes::EndNode();

    // Pop title bar colors (3 pushed above)
    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
    // Pop selection outline if pushed
    if (isSelected) {
      ImNodes::PopColorStyle();
    }

    // Toggle run/stop on right-click
    if (hoveredNodeId == uiNodeId &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup(("##ctx" + std::to_string(i)).c_str());
    }
    if (ImGui::BeginPopup(("##ctx" + std::to_string(i)).c_str())) {
      if (ImGui::MenuItem(graphNode.running ? "Stop" : "Run")) {
        graphNode.running = !graphNode.running;
      }
      ImGui::EndPopup();
    }
  }

  for (std::size_t i = 0; i < state.edges.size(); ++i) {
    const auto& edge = state.edges[i];
    const int fromIndex = findNodeIndex(state, edge.fromNode);
    const int toIndex = findNodeIndex(state, edge.toNode);
    if (fromIndex < 0 || toIndex < 0) {
      continue;
    }
    const int fromPortIndex = findOutputPortIndex(
        state.nodes[static_cast<std::size_t>(fromIndex)].descriptor,
        edge.fromPort);
    const int toPortIndex = findInputPortIndex(
        state.nodes[static_cast<std::size_t>(toIndex)].descriptor, edge.toPort);
    if (fromPortIndex < 0 || toPortIndex < 0) {
      continue;
    }
    ImNodes::Link(static_cast<int>(5000 + i),
                  outputAttributeId(static_cast<std::size_t>(fromIndex),
                                    static_cast<std::size_t>(fromPortIndex)),
                  inputAttributeId(static_cast<std::size_t>(toIndex),
                                   static_cast<std::size_t>(toPortIndex)));
  }

  ImNodes::EndNodeEditor();

  // Sync dragged positions back to state
  for (std::size_t i = 0; i < state.nodes.size(); ++i) {
    state.nodes[i].position = ImNodes::GetNodeGridSpacePos(nodeUiId(i));
  }

  // Handle link creation (drag between ports)
  int startAttr = 0, endAttr = 0;
  if (ImNodes::IsLinkCreated(&startAttr, &endAttr)) {
    // outputAttributeId offset: 200-299, inputAttributeId offset: 100-199
    const int outAttr = ((startAttr % 1000) >= 200) ? startAttr : endAttr;
    const int inAttr = ((startAttr % 1000) >= 200) ? endAttr : startAttr;
    const int fromNodeIndex = (outAttr / 1000) - 1;
    const int fromPortIndex = (outAttr % 1000) - 200;
    const int toNodeIndex = (inAttr / 1000) - 1;
    const int toPortIndex = (inAttr % 1000) - 100;
    if (fromNodeIndex >= 0 &&
        fromNodeIndex < static_cast<int>(state.nodes.size()) &&
        toNodeIndex >= 0 &&
        toNodeIndex < static_cast<int>(state.nodes.size()) &&
        fromPortIndex >= 0 && toPortIndex >= 0) {
      const auto& fromNode =
          state.nodes[static_cast<std::size_t>(fromNodeIndex)];
      const auto& toNode = state.nodes[static_cast<std::size_t>(toNodeIndex)];
      if (fromPortIndex <
              static_cast<int>(fromNode.descriptor.outputs.size()) &&
          toPortIndex < static_cast<int>(toNode.descriptor.inputs.size())) {
        state.edges.push_back(cvhub::GraphEdge{
            .fromNode = fromNode.node.instanceId,
            .fromPort = fromNode.descriptor
                            .outputs[static_cast<std::size_t>(fromPortIndex)]
                            .id,
            .toNode = toNode.node.instanceId,
            .toPort =
                toNode.descriptor.inputs[static_cast<std::size_t>(toPortIndex)]
                    .id,
        });
      }
    }
  }

  // Handle link deletion
  int destroyedLinkId = 0;
  if (ImNodes::IsLinkDestroyed(&destroyedLinkId)) {
    const int edgeIndex = destroyedLinkId - 5000;
    if (edgeIndex >= 0 && edgeIndex < static_cast<int>(state.edges.size())) {
      state.edges.erase(state.edges.begin() + edgeIndex);
    }
  }

  // Handle node deletion (Delete / Backspace)
  if (ImGui::IsKeyPressed(ImGuiKey_Delete) ||
      ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
    const int numSelected = ImNodes::NumSelectedNodes();
    if (numSelected > 0) {
      std::vector<int> selectedIds(static_cast<std::size_t>(numSelected));
      ImNodes::GetSelectedNodes(selectedIds.data());
      std::sort(selectedIds.rbegin(), selectedIds.rend());
      for (const int id : selectedIds) {
        const int idx = id - 1;
        if (idx < 0 || idx >= static_cast<int>(state.nodes.size())) continue;
        const std::string instanceId =
            state.nodes[static_cast<std::size_t>(idx)].node.instanceId;
        state.edges.erase(std::remove_if(state.edges.begin(), state.edges.end(),
                                         [&](const cvhub::GraphEdge& e) {
                                           return e.fromNode == instanceId ||
                                                  e.toNode == instanceId;
                                         }),
                          state.edges.end());
        state.nodes.erase(state.nodes.begin() + idx);
      }
      state.selectedIndex = 0;
      for (auto& n : state.nodes) {
        n.positionSet = false;
      }
    }
  }

  // Update selected index
  const int selectedNodeCount = ImNodes::NumSelectedNodes();
  if (selectedNodeCount > 0) {
    std::vector<int> selectedNodeIds(
        static_cast<std::size_t>(selectedNodeCount));
    ImNodes::GetSelectedNodes(selectedNodeIds.data());
    const int selectedIndex = selectedNodeIds.front() - 1;
    if (selectedIndex >= 0 &&
        selectedIndex < static_cast<int>(state.nodes.size())) {
      state.selectedIndex = selectedIndex;
    }
  }

  ImGui::EndChild();
}

void renderParameterEditor(cvhub::ParameterMap& values,
                           const cvhub::ParameterDescriptor& parameter) {
  auto it = values.find(parameter.name);
  if (it == values.end()) {
    it = values.emplace(parameter.name, parameter.defaultValue).first;
  }

  if (parameter.type == cvhub::SemanticType::Integer) {
    int value = std::get_if<int>(&it->second) ? std::get<int>(it->second) : 0;
    const int minValue =
        parameter.minValue ? static_cast<int>(*parameter.minValue) : 0;
    const int maxValue =
        parameter.maxValue ? static_cast<int>(*parameter.maxValue) : 4096;
    if (ImGui::DragInt(parameter.displayName.c_str(), &value, 1.0f, minValue,
                       maxValue)) {
      it->second = value;
    }
    return;
  }

  if (parameter.type == cvhub::SemanticType::Enum) {
    std::string current = std::get_if<std::string>(&it->second)
                              ? std::get<std::string>(it->second)
                              : std::string{};
    int currentIndex = 0;
    for (std::size_t i = 0; i < parameter.options.size(); ++i) {
      if (parameter.options[i].id == current) {
        currentIndex = static_cast<int>(i);
        break;
      }
    }
    bool changed = false;
    if (ImGui::BeginCombo(
            parameter.displayName.c_str(),
            parameter.options.empty()
                ? ""
                : parameter.options[static_cast<std::size_t>(currentIndex)]
                      .displayName.c_str())) {
      for (std::size_t i = 0; i < parameter.options.size(); ++i) {
        const bool selected = static_cast<int>(i) == currentIndex;
        if (ImGui::Selectable(parameter.options[i].displayName.c_str(),
                              selected)) {
          it->second = parameter.options[i].id;
          changed = true;
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    return;
  }

  ImGui::TextDisabled("%s", parameter.displayName.c_str());
}

void renderProperties(
    GuiGraphState& state,
    const std::unordered_map<std::string, NodePreviewTexture>& previews,
    float panelWidth) {
  ImGui::BeginChild("properties", ImVec2(panelWidth, 0.0f), true);

  if (state.nodes.empty()) {
    ImGui::TextDisabled("No graph nodes");
    ImGui::EndChild();
    return;
  }

  state.selectedIndex = std::clamp(state.selectedIndex, 0,
                                   static_cast<int>(state.nodes.size() - 1));
  GuiNodeState& selected =
      state.nodes[static_cast<std::size_t>(state.selectedIndex)];

  // Show node ID at the very top
  ImGui::TextUnformatted(selected.node.instanceId.c_str());
  ImGui::Separator();

  // Output image preview
  const float previewW = panelWidth - ImGui::GetStyle().WindowPadding.x * 2.0f;
  const auto previewIt = previews.find(selected.node.instanceId);
  if (previewIt != previews.end() && previewIt->second.id != 0) {
    const auto& tex = previewIt->second;
    const float aspect =
        static_cast<float>(tex.height) / static_cast<float>(tex.width);
    ImGui::Image(static_cast<ImTextureID>(tex.id),
                 ImVec2(previewW, previewW * aspect));
  } else {
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(previewW, previewW * 9.0f / 16.0f));
    ImGui::GetWindowDrawList()->AddRectFilled(
        p0, ImVec2{p0.x + previewW, p0.y + previewW * 9.0f / 16.0f},
        IM_COL32(45, 45, 50, 255), 4.0f);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2{p0.x + 8.0f, p0.y + previewW * 9.0f / 32.0f},
        IM_COL32(120, 120, 120, 255), "Build to preview");
  }

  ImGui::Separator();
  ImGui::TextUnformatted(selected.descriptor.displayName.c_str());
  ImGui::TextDisabled("%s", selected.node.instanceId.c_str());
  ImGui::Separator();

  if (selected.descriptor.parameters.empty()) {
    ImGui::TextDisabled("No editable parameters");
  }
  for (const auto& parameter : selected.descriptor.parameters) {
    renderParameterEditor(selected.node.parameters, parameter);
  }

  ImGui::EndChild();
}

}  // namespace

int main() {
  if (!glfwInit()) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLFWwindow* window = glfwCreateWindow(1280, 720, "CV_HUB", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImNodes::CreateContext();
  ImNodes::GetIO().EmulateThreeButtonMouse.Modifier = &ImGui::GetIO().KeyAlt;
  applyTheme();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 150");

  auto services = cvhub::app::createApplication(
      {.logFile = "cvhub-gui.log", .workerCount = 4});
  auto nodes = services->nodeCatalog()->list();
  GuiGraphState graphState;
  if (std::filesystem::exists("default.json")) {
    try {
      graphState = loadGraph("default.json", services->nodeCatalog());
    } catch (...) {
    }
  }
  bool showProperties = true;
  bool showLog = true;
  std::string status = "Ready";
  bool openSaveModal = false;
  bool openLoadModal = false;
  static char savePathBuf[512] = "graph.json";
  static char loadPathBuf[512] = "graph.json";
  std::vector<std::string> logLines = {"Ready"};
  std::unordered_map<std::string, NodePreviewTexture> previews;
  bool autoRun = false;
  int maxFps = 60;
  float smoothFps = 0.0f;
  PipelineWorker worker;

  while (!glfwWindowShouldClose(window)) {
    const auto frameStart = std::chrono::steady_clock::now();
    glfwPollEvents();

    // Poll pipeline results produced by the worker thread (non-blocking)
    if (autoRun) {
      cvhub::PipelineRunResult result;
      if (worker.poll(result)) {
        status = result.ok ? result.message : "Error: " + result.message;
        if (result.ok) uploadNodePreviews(result, previews);
      }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
      if (autoRun) {
        if (ImGui::Button("Stop")) {
          autoRun = false;
          worker.stop();
          logLines.push_back("Stopped.");
        }
      } else {
        if (ImGui::Button("Run")) {
          autoRun = true;
          for (auto& n : graphState.nodes) n.running = true;
          worker.start(services->pipelineExecutor(),
                       buildPipelineGraph(graphState));
          logLines.push_back("Run started.");
        }
      }
      ImGui::Separator();
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save Graph...")) openSaveModal = true;
        if (ImGui::MenuItem("Load Graph...")) openLoadModal = true;
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Parameters", nullptr, &showProperties);
        ImGui::MenuItem("Log", nullptr, &showLog);
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Settings")) {
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputInt("Max FPS", &maxFps, 1, 10);
        maxFps = std::clamp(maxFps, 1, 240);
        ImGui::EndMenu();
      }
      ImGui::TextDisabled("%.1f fps", smoothFps);
      ImGui::EndMainMenuBar();
    }

    // Save / Load modals
    if (openSaveModal) {
      ImGui::OpenPopup("Save Graph");
      openSaveModal = false;
    }
    if (openLoadModal) {
      ImGui::OpenPopup("Load Graph");
      openLoadModal = false;
    }

    ImGui::SetNextWindowSize(ImVec2(420, 0));
    if (ImGui::BeginPopupModal("Save Graph", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::SetNextItemWidth(380.0f);
      ImGui::InputText("##savepath", savePathBuf, sizeof(savePathBuf));
      ImGui::Spacing();
      if (ImGui::Button("Save", ImVec2(90, 0))) {
        try {
          saveGraph(savePathBuf, graphState);
          status = "Saved: " + std::string(savePathBuf);
        } catch (const std::exception& e) {
          status = std::string("Save error: ") + e.what();
        }
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(90, 0))) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(420, 0));
    if (ImGui::BeginPopupModal("Load Graph", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::SetNextItemWidth(380.0f);
      ImGui::InputText("##loadpath", loadPathBuf, sizeof(loadPathBuf));
      ImGui::Spacing();
      if (ImGui::Button("Load", ImVec2(90, 0))) {
        try {
          graphState = loadGraph(loadPathBuf, services->nodeCatalog());
          previews.clear();
          status = "Loaded: " + std::string(loadPathBuf);
        } catch (const std::exception& e) {
          status = std::string("Load error: ") + e.what();
        }
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(90, 0))) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }

    static float leftW = 270.0f;
    static float rightW = 300.0f;
    static float logH = 118.0f;
    static std::unordered_map<std::string, int> instanceCounters;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(
        ImVec2(viewport->WorkSize.x, viewport->WorkSize.y));
    ImGui::Begin("CV_HUB Workspace", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    const float logHeight = showLog ? (logH + 12.0f) : 0.0f;
    ImGui::BeginChild("main-row", ImVec2(0.0f, -logHeight), false);

    renderNodeList(nodes, graphState, leftW, autoRun, instanceCounters);
    ImGui::SameLine(0.0f, 0.0f);

    // Left splitter
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.92f, 0.45f, 0.15f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(0.92f, 0.45f, 0.15f, 1.0f));
    ImGui::Button("##lsplit", ImVec2(4.0f, -1.0f));
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemActive())
      leftW = std::clamp(leftW + ImGui::GetIO().MouseDelta.x, 100.0f, 500.0f);
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    ImGui::SameLine(0.0f, 0.0f);

    const float graphReserve = showProperties ? (rightW + 4.0f) : 0.0f;
    renderGraph(graphState, graphReserve, previews);

    if (showProperties) {
      ImGui::SameLine(0.0f, 0.0f);
      // Right splitter
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.92f, 0.45f, 0.15f, 0.8f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.92f, 0.45f, 0.15f, 1.0f));
      ImGui::Button("##rsplit", ImVec2(4.0f, -1.0f));
      ImGui::PopStyleColor(3);
      if (ImGui::IsItemActive())
        rightW =
            std::clamp(rightW - ImGui::GetIO().MouseDelta.x, 150.0f, 600.0f);
      if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
      ImGui::SameLine(0.0f, 0.0f);
      renderProperties(graphState, previews, rightW);
    }
    ImGui::EndChild();
    if (showLog) {
      // Vertical resize handle
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.92f, 0.45f, 0.15f, 0.8f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.92f, 0.45f, 0.15f, 1.0f));
      ImGui::Button("##logSplit", ImVec2(-1.0f, 4.0f));
      ImGui::PopStyleColor(3);
      if (ImGui::IsItemActive()) {
        logH = std::clamp(logH - ImGui::GetIO().MouseDelta.y, 60.0f, 400.0f);
      }
      if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
      }
      ImGui::BeginChild("log-window", ImVec2(0.0f, logH), true);
      ImGui::TextUnformatted("Log");
      ImGui::Separator();
      for (const auto& line : logLines) {
        ImGui::TextUnformatted(line.c_str());
      }
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
      }
      ImGui::EndChild();
    }
    ImGui::End();

    ImGui::Render();
    int displayW = 0;
    int displayH = 0;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    glViewport(0, 0, displayW, displayH);
    glClearColor(0.13f, 0.13f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);

    // Throttle to maxFps and compute smoothed FPS
    {
      const auto frameEnd = std::chrono::steady_clock::now();
      const std::chrono::duration<double> elapsed = frameEnd - frameStart;
      if (maxFps > 0) {
        const std::chrono::duration<double> target{1.0 / maxFps};
        if (elapsed < target) std::this_thread::sleep_for(target - elapsed);
      }
      const auto afterSleep = std::chrono::steady_clock::now();
      const double totalSec =
          std::chrono::duration<double>(afterSleep - frameStart).count();
      const float instantFps =
          totalSec > 0.0 ? static_cast<float>(1.0 / totalSec) : 0.0f;
      smoothFps = smoothFps * 0.9f + instantFps * 0.1f;
    }
  }

  worker.stop();
  cleanupPreviews(previews);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImNodes::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
