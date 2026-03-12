#include "NodeCommands.h"
#include "../node_editor/NodeRegistry.h"
#include "../utils/Logger.h"

namespace TopOpt {

// ================================================================
//  AddNodeCmd
// ================================================================

AddNodeCmd::AddNodeCmd(NodeEditor& editor, const std::string& typeName, float x, float y)
    : editor_(editor), typeName_(typeName), x_(x), y_(y) {}

void AddNodeCmd::execute() {
    if (firstExec_) {
        auto* def = NodeRegistry::instance().findType(typeName_);
        if (!def) return;
        nodeData_.id       = editor_.nextNodeId();
        editor_.setNextNodeId(nodeData_.id + 1);
        nodeData_.typeName = typeName_;
        nodeData_.label    = def->displayName;
        nodeData_.posX     = x_;
        nodeData_.posY     = y_;
        nodeData_.params   = def->defaultParams;
        firstExec_ = false;
    }
    editor_.addNodeDirect(nodeData_);
    Logger::instance().info("Added node: " + nodeData_.label + " (id=" + std::to_string(nodeData_.id) + ")");
}

void AddNodeCmd::undo() {
    editor_.removeNodeDirect(nodeData_.id);
    Logger::instance().info("Undo: remove node " + std::to_string(nodeData_.id));
}

std::string AddNodeCmd::description() const {
    return "Add " + nodeData_.label;
}

// ================================================================
//  RemoveNodeCmd
// ================================================================

RemoveNodeCmd::RemoveNodeCmd(NodeEditor& editor, int nodeId)
    : editor_(editor), nodeId_(nodeId) {}

void RemoveNodeCmd::execute() {
    // Save node data before removing
    auto* node = editor_.findNode(nodeId_);
    if (node) {
        // Sync position from imnodes
        editor_.syncPositionsFromImnodes();
        savedNode_ = *node;
    }
    // Save affected connections
    savedConns_.clear();
    for (auto& c : editor_.connections()) {
        if (c.startNodeId == nodeId_ || c.endNodeId == nodeId_) {
            savedConns_.push_back(c);
        }
    }
    // Remove connections first, then node
    for (auto& c : savedConns_) {
        editor_.removeConnectionDirect(c.id);
    }
    editor_.removeNodeDirect(nodeId_);
    Logger::instance().info("Removed node " + std::to_string(nodeId_));
}

void RemoveNodeCmd::undo() {
    editor_.addNodeDirect(savedNode_);
    for (auto& c : savedConns_) {
        editor_.addConnectionDirect(c);
    }
    Logger::instance().info("Undo: restore node " + std::to_string(nodeId_));
}

std::string RemoveNodeCmd::description() const {
    return "Remove node " + std::to_string(nodeId_);
}

// ================================================================
//  AddConnectionCmd
// ================================================================

AddConnectionCmd::AddConnectionCmd(NodeEditor& editor, const Connection& conn)
    : editor_(editor), conn_(conn) {}

void AddConnectionCmd::execute() {
    editor_.addConnectionDirect(conn_);
}

void AddConnectionCmd::undo() {
    editor_.removeConnectionDirect(conn_.id);
}

std::string AddConnectionCmd::description() const {
    return "Connect " + std::to_string(conn_.startNodeId) + " -> " + std::to_string(conn_.endNodeId);
}

// ================================================================
//  RemoveConnectionCmd
// ================================================================

RemoveConnectionCmd::RemoveConnectionCmd(NodeEditor& editor, int connId)
    : editor_(editor), connId_(connId) {}

void RemoveConnectionCmd::execute() {
    // Save the connection before removing
    for (auto& c : editor_.connections()) {
        if (c.id == connId_) {
            savedConn_ = c;
            break;
        }
    }
    editor_.removeConnectionDirect(connId_);
}

void RemoveConnectionCmd::undo() {
    editor_.addConnectionDirect(savedConn_);
}

std::string RemoveConnectionCmd::description() const {
    return "Remove connection " + std::to_string(connId_);
}

// ================================================================
//  MoveNodeCmd
// ================================================================

MoveNodeCmd::MoveNodeCmd(NodeEditor& editor, int nodeId,
                         float oldX, float oldY, float newX, float newY)
    : editor_(editor), nodeId_(nodeId),
      oldX_(oldX), oldY_(oldY), newX_(newX), newY_(newY) {}

void MoveNodeCmd::execute() {
    editor_.setNodePosition(nodeId_, newX_, newY_);
}

void MoveNodeCmd::undo() {
    editor_.setNodePosition(nodeId_, oldX_, oldY_);
}

std::string MoveNodeCmd::description() const {
    return "Move node " + std::to_string(nodeId_);
}

// ================================================================
//  ChangeParamCmd
// ================================================================

ChangeParamCmd::ChangeParamCmd(NodeEditor& editor, int nodeId, int paramIndex,
                               const ParamDef& oldVal, const ParamDef& newVal)
    : editor_(editor), nodeId_(nodeId), paramIndex_(paramIndex),
      oldVal_(oldVal), newVal_(newVal) {}

void ChangeParamCmd::execute() {
    auto* node = editor_.findNode(nodeId_);
    if (node && paramIndex_ >= 0 && paramIndex_ < (int)node->params.size()) {
        node->params[paramIndex_] = newVal_;
    }
}

void ChangeParamCmd::undo() {
    auto* node = editor_.findNode(nodeId_);
    if (node && paramIndex_ >= 0 && paramIndex_ < (int)node->params.size()) {
        node->params[paramIndex_] = oldVal_;
    }
}

std::string ChangeParamCmd::description() const {
    return "Change param " + oldVal_.name;
}

// ================================================================
//  ClearAllCmd
// ================================================================

ClearAllCmd::ClearAllCmd(NodeEditor& editor)
    : editor_(editor) {}

void ClearAllCmd::execute() {
    if (!snapshotTaken_) {
        editor_.syncPositionsFromImnodes();
        savedSnap_ = editor_.getSnapshot();
        snapshotTaken_ = true;
    }
    editor_.clear();
}

void ClearAllCmd::undo() {
    editor_.restoreSnapshot(savedSnap_);
    Logger::instance().info("Undo: restored canvas");
}

std::string ClearAllCmd::description() const {
    return "Clear all";
}

} // namespace TopOpt
