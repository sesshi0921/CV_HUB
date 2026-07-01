#define GL_SILENCE_DEPRECATION

#include "cvhub/app/application.hpp"
#include "cvhub/app/sample_graph.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>

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
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct GuiNodeState {
    cvhub::GraphNode node;
    cvhub::NodeDescriptor descriptor;
    ImVec2 position;
};

struct GuiGraphState {
    std::vector<GuiNodeState> nodes;
    std::vector<cvhub::GraphEdge> edges;
    int selectedIndex{0};
};

void applyTheme()
{
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

cvhub::ParameterValue defaultParameterValue(const cvhub::ParameterDescriptor& parameter)
{
    return parameter.defaultValue;
}

GuiGraphState createGuiGraphState(const cvhub::PipelineGraph& graph, const std::shared_ptr<cvhub::INodeCatalog>& catalog)
{
    GuiGraphState state;
    state.edges = graph.edges;

    const ImVec2 positions[] = {ImVec2(40.0f, 80.0f), ImVec2(300.0f, 80.0f), ImVec2(560.0f, 80.0f), ImVec2(820.0f, 80.0f)};
    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        auto graphNode = graph.nodes[i];
        const auto descriptor = catalog->descriptor(graphNode.nodeId);
        for (const auto& parameter : descriptor.parameters) {
            graphNode.parameters.try_emplace(parameter.name, defaultParameterValue(parameter));
        }
        state.nodes.push_back(GuiNodeState{
            .node = std::move(graphNode),
            .descriptor = descriptor,
            .position = i < std::size(positions) ? positions[i] : ImVec2(40.0f + static_cast<float>(i) * 260.0f, 80.0f),
        });
    }

    return state;
}

cvhub::PipelineGraph buildPipelineGraph(const GuiGraphState& state)
{
    cvhub::PipelineGraph graph;
    graph.edges = state.edges;
    for (const auto& node : state.nodes) {
        graph.nodes.push_back(node.node);
    }
    return graph;
}

int nodeUiId(std::size_t nodeIndex)
{
    return static_cast<int>(nodeIndex + 1);
}

int inputAttributeId(std::size_t nodeIndex, std::size_t portIndex)
{
    return static_cast<int>((nodeIndex + 1) * 1000 + 100 + portIndex);
}

int outputAttributeId(std::size_t nodeIndex, std::size_t portIndex)
{
    return static_cast<int>((nodeIndex + 1) * 1000 + 200 + portIndex);
}

int findNodeIndex(const GuiGraphState& state, const cvhub::NodeInstanceId& instanceId)
{
    const auto it = std::ranges::find_if(state.nodes, [&](const GuiNodeState& node) {
        return node.node.instanceId == instanceId;
    });
    return it == state.nodes.end() ? -1 : static_cast<int>(std::distance(state.nodes.begin(), it));
}

int findInputPortIndex(const cvhub::NodeDescriptor& descriptor, const cvhub::PortId& portId)
{
    const auto it = std::ranges::find_if(descriptor.inputs, [&](const cvhub::PortDescriptor& port) {
        return port.id == portId;
    });
    return it == descriptor.inputs.end() ? -1 : static_cast<int>(std::distance(descriptor.inputs.begin(), it));
}

int findOutputPortIndex(const cvhub::NodeDescriptor& descriptor, const cvhub::PortId& portId)
{
    const auto it = std::ranges::find_if(descriptor.outputs, [&](const cvhub::PortDescriptor& port) {
        return port.id == portId;
    });
    return it == descriptor.outputs.end() ? -1 : static_cast<int>(std::distance(descriptor.outputs.begin(), it));
}

void renderNodeList(const std::vector<cvhub::NodeDescriptor>& nodes, const GuiGraphState& graphState)
{
    ImGui::BeginChild("node-list", ImVec2(260.0f, 0.0f), true);
    ImGui::TextUnformatted("Nodes");
    ImGui::Separator();
    for (const auto& node : nodes) {
        ImGui::Selectable(node.displayName.c_str(), false);
        ImGui::TextDisabled("%s", node.id.c_str());
        ImGui::Spacing();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Graph");
    for (std::size_t i = 0; i < graphState.nodes.size(); ++i) {
        const auto& graphNode = graphState.nodes[i];
        ImGui::BulletText("%s", graphNode.node.instanceId.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", graphNode.node.nodeId.c_str());
    }
    ImGui::EndChild();
}

void renderGraph(GuiGraphState& state)
{
    ImGui::BeginChild("graph-editor", ImVec2(0.0f, 0.0f), true);
    ImNodes::BeginNodeEditor();

    for (std::size_t i = 0; i < state.nodes.size(); ++i) {
        const auto& graphNode = state.nodes[i];
        const int uiNodeId = nodeUiId(i);
        ImNodes::SetNodeGridSpacePos(uiNodeId, graphNode.position);
        ImNodes::BeginNode(uiNodeId);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(graphNode.descriptor.ui.title.empty() ? graphNode.descriptor.displayName.c_str() : graphNode.descriptor.ui.title.c_str());
        ImNodes::EndNodeTitleBar();

        for (std::size_t inputIndex = 0; inputIndex < graphNode.descriptor.inputs.size(); ++inputIndex) {
            const auto& input = graphNode.descriptor.inputs[inputIndex];
            ImNodes::BeginInputAttribute(inputAttributeId(i, inputIndex));
            ImGui::TextUnformatted(input.displayName.c_str());
            ImNodes::EndInputAttribute();
        }

        ImGui::TextDisabled("%s", graphNode.node.nodeId.c_str());
        for (const auto& parameter : graphNode.descriptor.parameters) {
            const auto valueIt = graphNode.node.parameters.find(parameter.name);
            if (valueIt != graphNode.node.parameters.end()) {
                if (const auto* value = std::get_if<int>(&valueIt->second)) {
                    ImGui::TextDisabled("%s: %d", parameter.name.c_str(), *value);
                } else if (const auto* value = std::get_if<std::string>(&valueIt->second)) {
                    ImGui::TextDisabled("%s: %s", parameter.name.c_str(), value->c_str());
                }
            }
        }

        for (std::size_t outputIndex = 0; outputIndex < graphNode.descriptor.outputs.size(); ++outputIndex) {
            const auto& output = graphNode.descriptor.outputs[outputIndex];
            ImNodes::BeginOutputAttribute(outputAttributeId(i, outputIndex));
            ImGui::Indent(90.0f);
            ImGui::TextUnformatted(output.displayName.c_str());
            ImNodes::EndOutputAttribute();
        }
        ImNodes::EndNode();
    }

    for (std::size_t i = 0; i < state.edges.size(); ++i) {
        const auto& edge = state.edges[i];
        const int fromIndex = findNodeIndex(state, edge.fromNode);
        const int toIndex = findNodeIndex(state, edge.toNode);
        if (fromIndex < 0 || toIndex < 0) {
            continue;
        }
        const int fromPortIndex = findOutputPortIndex(state.nodes[static_cast<std::size_t>(fromIndex)].descriptor, edge.fromPort);
        const int toPortIndex = findInputPortIndex(state.nodes[static_cast<std::size_t>(toIndex)].descriptor, edge.toPort);
        if (fromPortIndex < 0 || toPortIndex < 0) {
            continue;
        }
        ImNodes::Link(static_cast<int>(5000 + i), outputAttributeId(static_cast<std::size_t>(fromIndex), static_cast<std::size_t>(fromPortIndex)), inputAttributeId(static_cast<std::size_t>(toIndex), static_cast<std::size_t>(toPortIndex)));
    }

    const int selectedNodeCount = ImNodes::NumSelectedNodes();
    if (selectedNodeCount > 0) {
        std::vector<int> selectedNodeIds(static_cast<std::size_t>(selectedNodeCount));
        ImNodes::GetSelectedNodes(selectedNodeIds.data());
        const int selectedNodeId = selectedNodeIds.front();
        const int selectedIndex = selectedNodeId - 1;
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(state.nodes.size())) {
            state.selectedIndex = selectedIndex;
        }
    }

    ImNodes::EndNodeEditor();
    ImGui::EndChild();
}

void renderParameterEditor(cvhub::ParameterMap& values, const cvhub::ParameterDescriptor& parameter)
{
    auto it = values.find(parameter.name);
    if (it == values.end()) {
        it = values.emplace(parameter.name, parameter.defaultValue).first;
    }

    if (parameter.type == cvhub::SemanticType::Integer) {
        int value = std::get_if<int>(&it->second) ? std::get<int>(it->second) : 0;
        const int minValue = parameter.minValue ? static_cast<int>(*parameter.minValue) : 0;
        const int maxValue = parameter.maxValue ? static_cast<int>(*parameter.maxValue) : 4096;
        if (ImGui::DragInt(parameter.displayName.c_str(), &value, 1.0f, minValue, maxValue)) {
            it->second = value;
        }
        return;
    }

    if (parameter.type == cvhub::SemanticType::Enum) {
        std::string current = std::get_if<std::string>(&it->second) ? std::get<std::string>(it->second) : std::string{};
        int currentIndex = 0;
        for (std::size_t i = 0; i < parameter.options.size(); ++i) {
            if (parameter.options[i].id == current) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }
        if (ImGui::BeginCombo(parameter.displayName.c_str(), parameter.options.empty() ? "" : parameter.options[static_cast<std::size_t>(currentIndex)].displayName.c_str())) {
            for (std::size_t i = 0; i < parameter.options.size(); ++i) {
                const bool selected = static_cast<int>(i) == currentIndex;
                if (ImGui::Selectable(parameter.options[i].displayName.c_str(), selected)) {
                    it->second = parameter.options[i].id;
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

void renderProperties(GuiGraphState& state)
{
    ImGui::BeginChild("properties", ImVec2(300.0f, 0.0f), true);
    ImGui::TextUnformatted("Parameters");
    ImGui::Separator();

    if (state.nodes.empty()) {
        ImGui::TextDisabled("No graph nodes");
        ImGui::EndChild();
        return;
    }

    state.selectedIndex = std::clamp(state.selectedIndex, 0, static_cast<int>(state.nodes.size() - 1));
    GuiNodeState& selected = state.nodes[static_cast<std::size_t>(state.selectedIndex)];
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

} // namespace

int main()
{
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
    applyTheme();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    auto services = cvhub::app::createApplication({.logFile = "cvhub-gui.log", .workerCount = 4});
    auto nodes = services->nodeCatalog()->list();
    GuiGraphState graphState = createGuiGraphState(cvhub::app::createOpenCVSampleGraph(), services->nodeCatalog());
    bool showProperties = true;
    bool showLog = true;
    std::string status = "Ready";
    std::vector<std::string> logLines = {"Ready"};

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::Button("Build")) {
                const auto result = services->pipelineExecutor()->run(buildPipelineGraph(graphState));
                status = result.message;
                logLines.push_back(result.ok ? "Build completed: " + result.message : "Build failed: " + result.message);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Parameters", nullptr, &showProperties);
                ImGui::MenuItem("Log", nullptr, &showLog);
                ImGui::EndMenu();
            }
            ImGui::TextDisabled("%s", status.c_str());
            ImGui::EndMainMenuBar();
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y));
        ImGui::Begin("CV_HUB Workspace", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        const float logHeight = showLog ? 126.0f : 0.0f;
        ImGui::BeginChild("main-row", ImVec2(0.0f, -logHeight), false);
        renderNodeList(nodes, graphState);
        ImGui::SameLine();
        renderGraph(graphState);
        if (showProperties) {
            ImGui::SameLine();
            renderProperties(graphState);
        }
        ImGui::EndChild();
        if (showLog) {
            ImGui::BeginChild("log-window", ImVec2(0.0f, 118.0f), true);
            ImGui::TextUnformatted("Log");
            ImGui::Separator();
            for (const auto& line : logLines) {
                ImGui::TextUnformatted(line.c_str());
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
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
