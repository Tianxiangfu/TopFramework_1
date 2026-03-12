#include "NodeEditor.h"
#include "NodeRegistry.h"
#include "../utils/Logger.h"
#include "imgui.h"
#include "imnodes.h"
#include <algorithm>
#include <cstring>

namespace TopOpt {

NodeEditor::NodeEditor() {}

NodeEditor::~NodeEditor() {
    if (initialized_) {
        ImNodes::DestroyContext();
    }
}

void NodeEditor::draw() {
    if (!initialized_) {
        ImNodes::CreateContext();

        // Style matching geo-software dark theme
        ImNodesStyle& style = ImNodes::GetStyle();
        style.Colors[ImNodesCol_NodeBackground]         = IM_COL32( 40,  40,  70, 230);
        style.Colors[ImNodesCol_NodeBackgroundHovered]   = IM_COL32( 50,  50,  85, 240);
        style.Colors[ImNodesCol_NodeBackgroundSelected]  = IM_COL32( 55,  55,  90, 245);
        style.Colors[ImNodesCol_NodeOutline]             = IM_COL32( 68,  68, 102, 255);
        style.Colors[ImNodesCol_TitleBar]                = IM_COL32( 58, 124, 165, 255);
        style.Colors[ImNodesCol_TitleBarHovered]         = IM_COL32( 70, 140, 185, 255);
        style.Colors[ImNodesCol_TitleBarSelected]        = IM_COL32(  0, 212, 170, 255);
        style.Colors[ImNodesCol_Link]                    = IM_COL32( 78, 205, 196, 200);
        style.Colors[ImNodesCol_LinkHovered]             = IM_COL32(  0, 240, 192, 255);
        style.Colors[ImNodesCol_LinkSelected]            = IM_COL32(  0, 212, 170, 255);
        style.Colors[ImNodesCol_Pin]                     = IM_COL32(160, 160, 184, 255);
        style.Colors[ImNodesCol_PinHovered]              = IM_COL32(200, 200, 220, 255);
        style.Colors[ImNodesCol_BoxSelector]             = IM_COL32( 78, 205, 196,  60);
        style.Colors[ImNodesCol_BoxSelectorOutline]      = IM_COL32( 78, 205, 196, 200);
        style.Colors[ImNodesCol_GridBackground]          = IM_COL32( 30,  30,  32, 255);
        style.Colors[ImNodesCol_GridLine]                = IM_COL32( 48,  48,  52, 255);
        style.Colors[ImNodesCol_MiniMapBackground]       = IM_COL32( 24,  24,  26, 200);
        style.Colors[ImNodesCol_MiniMapBackgroundHovered]= IM_COL32( 24,  24,  26, 240);
        style.Colors[ImNodesCol_MiniMapOutline]          = IM_COL32( 58,  58,  62, 255);
        style.Colors[ImNodesCol_MiniMapOutlineHovered]   = IM_COL32( 78, 205, 196, 255);
        style.Colors[ImNodesCol_MiniMapNodeBackground]   = IM_COL32( 45,  45,  48, 200);
        style.Colors[ImNodesCol_MiniMapNodeBackgroundHovered] = IM_COL32(0, 212, 170, 200);
        style.Colors[ImNodesCol_MiniMapNodeBackgroundSelected]= IM_COL32(0, 212, 170, 255);
        style.Colors[ImNodesCol_MiniMapNodeOutline]      = IM_COL32( 68,  68,  72, 255);
        style.Colors[ImNodesCol_MiniMapLink]             = IM_COL32( 78, 205, 196, 150);
        style.Colors[ImNodesCol_MiniMapLinkSelected]     = IM_COL32(  0, 212, 170, 255);
        style.Colors[ImNodesCol_MiniMapCanvas]           = IM_COL32( 30,  30,  32, 180);
        style.Colors[ImNodesCol_MiniMapCanvasOutline]    = IM_COL32( 78, 205, 196, 100);

        style.NodeCornerRounding  = 8.0f;
        style.NodePadding         = ImVec2(8.f, 4.f);
        style.NodeBorderThickness = 2.0f;
        style.LinkThickness       = 2.5f;
        style.PinCircleRadius     = 5.0f;
        style.PinOffset           = 0.0f;
        style.Flags               = ImNodesStyleFlags_GridLines;

        initialized_ = true;
    }

    // --- Zoom: scale style values before render ---
    applyZoomBeforeRender();

    ImNodes::BeginNodeEditor();

    // Scale font INSIDE the imnodes child window (SetWindowFontScale is per-window)
    if (zoom_ != 1.0f) {
        ImGui::SetWindowFontScale(zoom_);
    }

    // Cache canvas origin for zoom-towards-mouse calculation
    canvasOriginCache_ = ImGui::GetWindowPos();

    // Set zoomed grid positions for all nodes
    for (auto& node : nodes_) {
        ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.posX * zoom_, node.posY * zoom_));
    }

    // Draw grid hint when empty
    if (nodes_.empty()) {
        ImVec2 canvasCenter = ImGui::GetWindowSize();
        canvasCenter.x *= 0.5f;
        canvasCenter.y *= 0.5f;
        ImGui::SetCursorPos(ImVec2(canvasCenter.x - 120, canvasCenter.y - 10));
        ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f),
            "Right-click to add nodes, or drag from panel");
    }

    // Right-click context menu
    {
        bool openPopup = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                         && ImNodes::IsEditorHovered()
                         && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        if (openPopup) {
            ImGui::OpenPopup("NodeEditorContextMenu");
        }

        if (ImGui::BeginPopup("NodeEditorContextMenu")) {
            const ImVec2 clickPos = ImGui::GetMousePosOnOpeningCurrentPopup();
            auto cats = NodeRegistry::instance().categorized();
            for (auto& [cat, typeNames] : cats) {
                if (ImGui::BeginMenu(cat.c_str())) {
                    for (auto& tn : typeNames) {
                        auto* def = NodeRegistry::instance().findType(tn);
                        if (def && ImGui::MenuItem(def->displayName.c_str())) {
                            addNode(tn, clickPos.x, clickPos.y);
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::EndPopup();
        }
    }

    // Draw all nodes
    for (auto& node : nodes_) {
        drawNode(node);
    }

    // Draw all connections
    for (auto& conn : connections_) {
        ImNodes::Link(conn.id, conn.startAttrId(), conn.endAttrId());
    }

    // Minimap
    ImNodes::MiniMap(0.15f, ImNodesMiniMapLocation_BottomRight);

    // Capture scroll wheel for zoom INSIDE the editor child window
    // (ImGui::IsWindowHovered only works while the imnodes child window is active)
    {
        pendingZoomWheel_ = 0.0f;
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        ImVec2 winPos   = ImGui::GetWindowPos();
        ImVec2 winSize  = ImGui::GetWindowSize();
        bool mouseInEditor = mousePos.x >= winPos.x && mousePos.x <= winPos.x + winSize.x &&
                             mousePos.y >= winPos.y && mousePos.y <= winPos.y + winSize.y;
        if (mouseInEditor) {
            pendingZoomWheel_ = ImGui::GetIO().MouseWheel;
        }
    }

    ImNodes::EndNodeEditor();

    // --- Zoom: read back positions and restore style ---
    applyZoomAfterRender();

    // --- Zoom: handle scroll wheel ---
    handleZoomInput();

    // Handle interactions
    handleNewLinks();
    handleDeletedLinks();

    // Track selection
    int numSelected = ImNodes::NumSelectedNodes();
    if (numSelected == 1) {
        int selId;
        ImNodes::GetSelectedNodes(&selId);
        selectedNodeId_ = selId;
    } else if (numSelected == 0) {
        selectedNodeId_ = -1;
    }

    // Delete key
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_X)) {
        removeSelectedNodes();
    }
}

// ================================================================
//  Zoom helpers
// ================================================================

void NodeEditor::setZoom(float z) {
    zoom_ = std::clamp(z, 0.25f, 3.0f);
}

void NodeEditor::applyZoomBeforeRender() {
    if (zoom_ == 1.0f) return;

    // Save and scale imnodes style
    ImNodesStyle& style = ImNodes::GetStyle();
    savedGridSpacing_         = style.GridSpacing;
    savedNodeCornerRounding_  = style.NodeCornerRounding;
    savedNodePadding_         = style.NodePadding;
    savedNodeBorderThickness_ = style.NodeBorderThickness;
    savedLinkThickness_       = style.LinkThickness;
    savedLinkHoverDistance_   = style.LinkHoverDistance;
    savedPinCircleRadius_     = style.PinCircleRadius;
    savedPinQuadSideLength_   = style.PinQuadSideLength;
    savedPinTriangleSideLength_ = style.PinTriangleSideLength;
    savedPinLineThickness_    = style.PinLineThickness;
    savedPinHoverRadius_      = style.PinHoverRadius;
    savedPinOffset_           = style.PinOffset;

    style.GridSpacing           *= zoom_;
    style.NodeCornerRounding    *= zoom_;
    style.NodePadding.x         *= zoom_;
    style.NodePadding.y         *= zoom_;
    style.NodeBorderThickness   *= zoom_;
    style.LinkThickness         *= zoom_;
    style.LinkHoverDistance     *= zoom_;
    style.PinCircleRadius       *= zoom_;
    style.PinQuadSideLength     *= zoom_;
    style.PinTriangleSideLength *= zoom_;
    style.PinLineThickness      *= zoom_;
    style.PinHoverRadius        *= zoom_;
    style.PinOffset             *= zoom_;
}

void NodeEditor::applyZoomAfterRender() {
    // Read back real (un-zoomed) grid-space positions
    for (auto& n : nodes_) {
        ImVec2 gridPos = ImNodes::GetNodeGridSpacePos(n.id);
        n.posX = gridPos.x / zoom_;
        n.posY = gridPos.y / zoom_;
    }

    if (zoom_ == 1.0f) return;

    // Restore imnodes style
    ImNodesStyle& style = ImNodes::GetStyle();
    style.GridSpacing           = savedGridSpacing_;
    style.NodeCornerRounding    = savedNodeCornerRounding_;
    style.NodePadding           = savedNodePadding_;
    style.NodeBorderThickness   = savedNodeBorderThickness_;
    style.LinkThickness         = savedLinkThickness_;
    style.LinkHoverDistance     = savedLinkHoverDistance_;
    style.PinCircleRadius       = savedPinCircleRadius_;
    style.PinQuadSideLength     = savedPinQuadSideLength_;
    style.PinTriangleSideLength = savedPinTriangleSideLength_;
    style.PinLineThickness      = savedPinLineThickness_;
    style.PinHoverRadius        = savedPinHoverRadius_;
    style.PinOffset             = savedPinOffset_;
}

void NodeEditor::handleZoomInput() {
    if (pendingZoomWheel_ == 0.0f) return;

    float oldZoom = zoom_;
    zoom_ *= (1.0f + pendingZoomWheel_ * 0.1f);
    zoom_ = std::clamp(zoom_, 0.25f, 3.0f);

    if (zoom_ == oldZoom) return;

    // Adjust panning so the point under the mouse cursor stays fixed
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    ImVec2 pan = ImNodes::EditorContextGetPanning();

    // Mouse position relative to canvas origin (editor space)
    ImVec2 mouseEditorPos = ImVec2(mousePos.x - canvasOriginCache_.x,
                                   mousePos.y - canvasOriginCache_.y);

    float zoomRatio = zoom_ / oldZoom;
    ImVec2 newPan = ImVec2(
        mouseEditorPos.x * (1.0f - zoomRatio) + pan.x * zoomRatio,
        mouseEditorPos.y * (1.0f - zoomRatio) + pan.y * zoomRatio
    );
    ImNodes::EditorContextResetPanning(newPan);
}

// ================================================================
//  Node rendering
// ================================================================

void NodeEditor::drawNode(NodeInstance& node) {
    auto* def = NodeRegistry::instance().findType(node.typeName);
    if (!def) return;

    // Calculate minimum node content width from all text
    float nodePad = 20.0f;  // pin circle + spacing on each side
    char tmpBuf[128];

    // Title width
    snprintf(tmpBuf, sizeof(tmpBuf), "%s (%s)", node.label.c_str(), def->icon.c_str());
    float minNodeW = ImGui::CalcTextSize(tmpBuf).x + nodePad;

    // Input port widths
    for (auto& port : def->inputs) {
        snprintf(tmpBuf, sizeof(tmpBuf), "%s (%s)", port.label.c_str(), portDataTypeName(port.dataType));
        float w = ImGui::CalcTextSize(tmpBuf).x + nodePad;
        if (w > minNodeW) minNodeW = w;
    }
    // Output port widths
    for (auto& port : def->outputs) {
        snprintf(tmpBuf, sizeof(tmpBuf), "(%s) %s", portDataTypeName(port.dataType), port.label.c_str());
        float w = ImGui::CalcTextSize(tmpBuf).x + nodePad;
        if (w > minNodeW) minNodeW = w;
    }
    // Param widths
    for (auto& p : node.params) {
        snprintf(tmpBuf, sizeof(tmpBuf), "%s: value", p.name.c_str());
        float w = ImGui::CalcTextSize(tmpBuf).x + nodePad;
        if (w > minNodeW) minNodeW = w;
    }

    // Push per-node title bar color
    ImNodes::PushColorStyle(ImNodesCol_TitleBar,         def->headerColor);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,  def->headerColor);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, def->headerColor);

    ImNodes::BeginNode(node.id);

    // Title bar
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted(node.label.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1,1,1,0.4f), "(%s)", def->icon.c_str());
    ImNodes::EndNodeTitleBar();

    // Input ports
    for (int i = 0; i < (int)def->inputs.size(); ++i) {
        auto& port = def->inputs[i];
        ImNodes::PushColorStyle(ImNodesCol_Pin, portDataTypeColor(port.dataType));
        ImNodes::BeginInputAttribute(node.inputAttrId(i));
        ImGui::TextColored(ImVec4(0.88f, 0.88f, 0.88f, 1.0f), "%s", port.label.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "(%s)", portDataTypeName(port.dataType));
        ImNodes::EndInputAttribute();
        ImNodes::PopColorStyle();
    }

    // Output ports — right-aligned using dynamic node width
    float indentBase = minNodeW;
    for (int i = 0; i < (int)def->outputs.size(); ++i) {
        auto& port = def->outputs[i];
        ImNodes::PushColorStyle(ImNodesCol_Pin, portDataTypeColor(port.dataType));
        ImNodes::BeginOutputAttribute(node.outputAttrId(i));
        snprintf(tmpBuf, sizeof(tmpBuf), "(%s) %s", portDataTypeName(port.dataType), port.label.c_str());
        float textWidth = ImGui::CalcTextSize(tmpBuf).x;
        float indent = indentBase - textWidth - nodePad;
        if (indent > 0) ImGui::Indent(indent);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "(%s)", portDataTypeName(port.dataType));
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.88f, 0.88f, 0.88f, 1.0f), "%s", port.label.c_str());
        ImNodes::EndOutputAttribute();
        ImNodes::PopColorStyle();
    }

    // Compact param preview
    if (!node.params.empty()) {
        ImGui::Separator();
        for (auto& p : node.params) {
            ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f), "%s:", p.name.c_str());
            ImGui::SameLine();
            switch (p.type) {
                case ParamType::Float:
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.75f, 1.0f), "%.4g", p.floatVal);
                    break;
                case ParamType::Int:
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.75f, 1.0f), "%d", p.intVal);
                    break;
                case ParamType::Bool:
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.75f, 1.0f), p.boolVal ? "true" : "false");
                    break;
                case ParamType::String:
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.75f, 1.0f), "%s",
                        p.stringVal.empty() ? "(empty)" : p.stringVal.c_str());
                    break;
                case ParamType::Enum:
                    if (p.enumIndex >= 0 && p.enumIndex < (int)p.enumOptions.size())
                        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.75f, 1.0f), "%s", p.enumOptions[p.enumIndex].c_str());
                    break;
                case ParamType::Color3:
                    {
                        ImVec4 c(p.color3[0], p.color3[1], p.color3[2], 1.0f);
                        ImGui::ColorButton("##c", c, ImGuiColorEditFlags_NoTooltip, ImVec2(40 * zoom_, 12 * zoom_));
                    }
                    break;
            }
        }
    }

    ImNodes::EndNode();
    ImNodes::PopColorStyle(); // TitleBarSelected
    ImNodes::PopColorStyle(); // TitleBarHovered
    ImNodes::PopColorStyle(); // TitleBar
}

void NodeEditor::handleNewLinks() {
    int startAttr, endAttr;
    if (ImNodes::IsLinkCreated(&startAttr, &endAttr)) {
        // Decode attr IDs
        int srcNode = startAttr / 1000;
        int srcPort = startAttr % 1000 - 500;
        int dstNode = endAttr / 1000;
        int dstPort = endAttr % 1000;

        if (srcPort < 0 || dstPort >= 500) {
            // Swapped direction
            std::swap(startAttr, endAttr);
            srcNode = startAttr / 1000;
            srcPort = startAttr % 1000 - 500;
            dstNode = endAttr / 1000;
            dstPort = endAttr % 1000;
        }

        // Port type compatibility check
        auto* srcNodeInst = findNode(srcNode);
        auto* dstNodeInst = findNode(dstNode);
        bool compatible = true;
        if (srcNodeInst && dstNodeInst) {
            auto* srcDef = NodeRegistry::instance().findType(srcNodeInst->typeName);
            auto* dstDef = NodeRegistry::instance().findType(dstNodeInst->typeName);
            if (srcDef && dstDef &&
                srcPort >= 0 && srcPort < (int)srcDef->outputs.size() &&
                dstPort >= 0 && dstPort < (int)dstDef->inputs.size()) {
                auto srcType = srcDef->outputs[srcPort].dataType;
                auto dstType = dstDef->inputs[dstPort].dataType;
                if (srcType != dstType && srcType != PortDataType::Generic && dstType != PortDataType::Generic) {
                    compatible = false;
                    Logger::instance().warn("Cannot connect: type mismatch (" +
                        std::string(portDataTypeName(srcType)) + " -> " +
                        std::string(portDataTypeName(dstType)) + ")");
                }
            }
        }

        if (compatible) {
            Connection c;
            c.id           = nextConnId_++;
            c.startNodeId  = srcNode;
            c.startPortIdx = srcPort;
            c.endNodeId    = dstNode;
            c.endPortIdx   = dstPort;
            connections_.push_back(c);

            Logger::instance().info("Connected node " + std::to_string(srcNode)
                + " -> node " + std::to_string(dstNode));
        }
    }
}

void NodeEditor::handleDeletedLinks() {
    int linkId;
    if (ImNodes::IsLinkDestroyed(&linkId)) {
        connections_.erase(
            std::remove_if(connections_.begin(), connections_.end(),
                [linkId](const Connection& c) { return c.id == linkId; }),
            connections_.end()
        );
        Logger::instance().info("Removed connection " + std::to_string(linkId));
    }
}

int NodeEditor::addNode(const std::string& typeName, float x, float y) {
    auto* def = NodeRegistry::instance().findType(typeName);
    if (!def) return -1;

    NodeInstance node;
    node.id       = nextNodeId_++;
    node.typeName = typeName;
    node.label    = def->displayName;
    node.params   = def->defaultParams;

    // Set screen-space position in imnodes
    ImNodes::SetNodeScreenSpacePos(node.id, ImVec2(x, y));

    // Read back grid-space position and un-zoom to get real position
    ImVec2 gridPos = ImNodes::GetNodeGridSpacePos(node.id);
    node.posX = gridPos.x / zoom_;
    node.posY = gridPos.y / zoom_;

    nodes_.push_back(node);

    Logger::instance().info("Added node: " + def->displayName + " (id=" + std::to_string(node.id) + ")");
    return node.id;
}

void NodeEditor::removeNode(int nodeId) {
    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(),
            [nodeId](const NodeInstance& n) { return n.id == nodeId; }),
        nodes_.end()
    );
    // Remove connections involving this node
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [nodeId](const Connection& c) {
                return c.startNodeId == nodeId || c.endNodeId == nodeId;
            }),
        connections_.end()
    );
    if (selectedNodeId_ == nodeId) selectedNodeId_ = -1;
    Logger::instance().info("Removed node " + std::to_string(nodeId));
}

void NodeEditor::removeSelectedNodes() {
    int numSel = ImNodes::NumSelectedNodes();
    if (numSel <= 0) return;
    std::vector<int> selIds(numSel);
    ImNodes::GetSelectedNodes(selIds.data());
    for (int id : selIds) {
        removeNode(id);
    }
    ImNodes::ClearNodeSelection();

    // Also delete selected links
    int numLinks = ImNodes::NumSelectedLinks();
    if (numLinks > 0) {
        std::vector<int> linkIds(numLinks);
        ImNodes::GetSelectedLinks(linkIds.data());
        for (int lid : linkIds) {
            removeConnection(lid);
        }
        ImNodes::ClearLinkSelection();
    }
}

void NodeEditor::removeConnection(int connId) {
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [connId](const Connection& c) { return c.id == connId; }),
        connections_.end()
    );
}

NodeInstance* NodeEditor::selectedNode() {
    for (auto& n : nodes_) {
        if (n.id == selectedNodeId_) return &n;
    }
    return nullptr;
}

const NodeInstance* NodeEditor::selectedNode() const {
    for (auto& n : nodes_) {
        if (n.id == selectedNodeId_) return &n;
    }
    return nullptr;
}

void NodeEditor::clear() {
    nodes_.clear();
    connections_.clear();
    selectedNodeId_ = -1;
    nextNodeId_ = 1;
    nextConnId_ = 1;
    Logger::instance().info("Canvas cleared");
}

// ================================================================
//  Direct manipulation (for Command system)
// ================================================================

void NodeEditor::addNodeDirect(const NodeInstance& node) {
    nodes_.push_back(node);
    if (initialized_) {
        ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.posX * zoom_, node.posY * zoom_));
    }
    if (node.id >= nextNodeId_) nextNodeId_ = node.id + 1;
}

void NodeEditor::removeNodeDirect(int nodeId) {
    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(),
            [nodeId](const NodeInstance& n) { return n.id == nodeId; }),
        nodes_.end()
    );
    if (selectedNodeId_ == nodeId) selectedNodeId_ = -1;
}

void NodeEditor::addConnectionDirect(const Connection& conn) {
    connections_.push_back(conn);
    if (conn.id >= nextConnId_) nextConnId_ = conn.id + 1;
}

void NodeEditor::removeConnectionDirect(int connId) {
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [connId](const Connection& c) { return c.id == connId; }),
        connections_.end()
    );
}

// ================================================================
//  Position query/set
// ================================================================

void NodeEditor::setNodePosition(int nodeId, float x, float y) {
    for (auto& n : nodes_) {
        if (n.id == nodeId) {
            n.posX = x;
            n.posY = y;
            if (initialized_)
                ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(x * zoom_, y * zoom_));
            return;
        }
    }
}

void NodeEditor::getNodePosition(int nodeId, float& x, float& y) const {
    for (auto& n : nodes_) {
        if (n.id == nodeId) {
            x = n.posX;
            y = n.posY;
            return;
        }
    }
}

void NodeEditor::syncPositionsFromImnodes() {
    if (!initialized_) return;
    for (auto& n : nodes_) {
        ImVec2 gridPos = ImNodes::GetNodeGridSpacePos(n.id);
        n.posX = gridPos.x / zoom_;
        n.posY = gridPos.y / zoom_;
    }
}

// ================================================================
//  Node lookup
// ================================================================

NodeInstance* NodeEditor::findNode(int nodeId) {
    for (auto& n : nodes_) {
        if (n.id == nodeId) return &n;
    }
    return nullptr;
}

const NodeInstance* NodeEditor::findNode(int nodeId) const {
    for (auto& n : nodes_) {
        if (n.id == nodeId) return &n;
    }
    return nullptr;
}

// ================================================================
//  Snapshot / restore
// ================================================================

EditorSnapshot NodeEditor::getSnapshot() const {
    EditorSnapshot snap;
    snap.nodes = nodes_;
    snap.connections = connections_;
    snap.nextNodeId = nextNodeId_;
    snap.nextConnId = nextConnId_;
    return snap;
}

void NodeEditor::restoreSnapshot(const EditorSnapshot& snap) {
    nodes_ = snap.nodes;
    connections_ = snap.connections;
    nextNodeId_ = snap.nextNodeId;
    nextConnId_ = snap.nextConnId;
    selectedNodeId_ = -1;

    // Restore imnodes positions (apply zoom)
    if (initialized_) {
        for (auto& n : nodes_) {
            ImNodes::SetNodeGridSpacePos(n.id, ImVec2(n.posX * zoom_, n.posY * zoom_));
        }
    }
}

// ================================================================
//  imnodes state serialization
// ================================================================

std::string NodeEditor::saveImnodesState() const {
    if (!initialized_) return "";
    const char* data = ImNodes::SaveCurrentEditorStateToIniString();
    return data ? std::string(data) : "";
}

void NodeEditor::loadImnodesState(const std::string& state) {
    if (!initialized_ || state.empty()) return;
    ImNodes::LoadCurrentEditorStateFromIniString(state.c_str(), state.size());
}

} // namespace TopOpt
