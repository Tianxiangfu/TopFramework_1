#pragma once
#include "Node.h"
#include "imgui.h"
#include <vector>
#include <string>

namespace TopOpt {

// Snapshot of entire editor state (for undo/redo of ClearAll, serialization)
struct EditorSnapshot {
    std::vector<NodeInstance> nodes;
    std::vector<Connection>  connections;
    int nextNodeId = 1;
    int nextConnId = 1;
};

class CommandHistory;

/**
 * NodeEditor - manages the node canvas and all interactions.
 */
class NodeEditor {
public:
    NodeEditor();
    ~NodeEditor();

    // Main draw call
    void draw();

    // Node management (high-level, logs)
    int  addNode(const std::string& typeName, float x = 100, float y = 100);
    void removeNode(int nodeId);
    void removeSelectedNodes();

    // Connection management
    void removeConnection(int connId);

    // Direct manipulation (for Command system, no logging)
    void addNodeDirect(const NodeInstance& node);
    void removeNodeDirect(int nodeId);
    void addConnectionDirect(const Connection& conn);
    void removeConnectionDirect(int connId);

    // Position query/set (syncs with imnodes)
    void  setNodePosition(int nodeId, float x, float y);
    void  getNodePosition(int nodeId, float& x, float& y) const;
    void  syncPositionsFromImnodes();

    // Node lookup
    NodeInstance*       findNode(int nodeId);
    const NodeInstance* findNode(int nodeId) const;

    // Snapshot / restore (for serialization and undo)
    EditorSnapshot getSnapshot() const;
    void restoreSnapshot(const EditorSnapshot& snap);

    // Selection
    int  selectedNodeId() const { return selectedNodeId_; }
    NodeInstance*       selectedNode();
    const NodeInstance* selectedNode() const;

    // Accessors
    std::vector<NodeInstance>& nodes() { return nodes_; }
    const std::vector<NodeInstance>& nodes() const { return nodes_; }
    std::vector<Connection>&  connections() { return connections_; }
    const std::vector<Connection>&  connections() const { return connections_; }
    int nodeCount() const { return (int)nodes_.size(); }
    int connectionCount() const { return (int)connections_.size(); }

    int  nextNodeId() const { return nextNodeId_; }
    int  nextConnId() const { return nextConnId_; }
    void setNextNodeId(int id) { nextNodeId_ = id; }
    void setNextConnId(int id) { nextConnId_ = id; }

    void clear();

    // Command history integration
    void setCommandHistory(CommandHistory* hist) { cmdHistory_ = hist; }
    CommandHistory* commandHistory() const { return cmdHistory_; }

    // imnodes state string (for serialization)
    std::string saveImnodesState() const;
    void loadImnodesState(const std::string& state);

    bool isInitialized() const { return initialized_; }

    // Zoom
    float zoom() const { return zoom_; }
    void  setZoom(float z);

private:
    void drawNode(NodeInstance& node);
    void handleNewLinks();
    void handleDeletedLinks();

    // Apply zoom: scale style/positions before render, restore after
    void applyZoomBeforeRender();
    void applyZoomAfterRender();
    void handleZoomInput();

    std::vector<NodeInstance> nodes_;
    std::vector<Connection>  connections_;
    int nextNodeId_ = 1;
    int nextConnId_ = 1;
    int selectedNodeId_ = -1;
    bool initialized_ = false;
    CommandHistory* cmdHistory_ = nullptr;

    // Zoom state
    float zoom_ = 1.0f;
    ImVec2 canvasOriginCache_ = {0, 0};   // cached canvas origin (screen space)
    float pendingZoomWheel_ = 0.0f;       // scroll delta captured inside editor scope
    // Saved imnodes style values (restored after zoomed render)
    float savedGridSpacing_ = 0;
    float savedNodeCornerRounding_ = 0;
    ImVec2 savedNodePadding_ = {0, 0};
    float savedNodeBorderThickness_ = 0;
    float savedLinkThickness_ = 0;
    float savedLinkHoverDistance_ = 0;
    float savedPinCircleRadius_ = 0;
    float savedPinQuadSideLength_ = 0;
    float savedPinTriangleSideLength_ = 0;
    float savedPinLineThickness_ = 0;
    float savedPinHoverRadius_ = 0;
    float savedPinOffset_ = 0;
};

} // namespace TopOpt
