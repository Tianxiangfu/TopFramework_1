#pragma once
#include "Command.h"
#include "../node_editor/Node.h"
#include "../node_editor/NodeEditor.h"
#include <vector>

namespace TopOpt {

// ================================================================
//  Add Node
// ================================================================
class AddNodeCmd : public Command {
public:
    AddNodeCmd(NodeEditor& editor, const std::string& typeName, float x, float y);
    void execute() override;
    void undo() override;
    std::string description() const override;
    int nodeId() const { return nodeData_.id; }

private:
    NodeEditor& editor_;
    std::string typeName_;
    float x_, y_;
    NodeInstance nodeData_;
    bool firstExec_ = true;
};

// ================================================================
//  Remove Node (stores node + affected connections)
// ================================================================
class RemoveNodeCmd : public Command {
public:
    RemoveNodeCmd(NodeEditor& editor, int nodeId);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    NodeEditor& editor_;
    int nodeId_;
    NodeInstance savedNode_;
    std::vector<Connection> savedConns_;
};

// ================================================================
//  Add Connection
// ================================================================
class AddConnectionCmd : public Command {
public:
    AddConnectionCmd(NodeEditor& editor, const Connection& conn);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    NodeEditor& editor_;
    Connection conn_;
};

// ================================================================
//  Remove Connection
// ================================================================
class RemoveConnectionCmd : public Command {
public:
    RemoveConnectionCmd(NodeEditor& editor, int connId);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    NodeEditor& editor_;
    int connId_;
    Connection savedConn_;
};

// ================================================================
//  Move Node
// ================================================================
class MoveNodeCmd : public Command {
public:
    MoveNodeCmd(NodeEditor& editor, int nodeId, float oldX, float oldY, float newX, float newY);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    NodeEditor& editor_;
    int nodeId_;
    float oldX_, oldY_, newX_, newY_;
};

// ================================================================
//  Change Parameter
// ================================================================
class ChangeParamCmd : public Command {
public:
    ChangeParamCmd(NodeEditor& editor, int nodeId, int paramIndex,
                   const ParamDef& oldVal, const ParamDef& newVal);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    NodeEditor& editor_;
    int nodeId_;
    int paramIndex_;
    ParamDef oldVal_;
    ParamDef newVal_;
};

// ================================================================
//  Clear All (snapshot-based)
// ================================================================
class ClearAllCmd : public Command {
public:
    ClearAllCmd(NodeEditor& editor);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    NodeEditor& editor_;
    EditorSnapshot savedSnap_;
    bool snapshotTaken_ = false;
};

} // namespace TopOpt
