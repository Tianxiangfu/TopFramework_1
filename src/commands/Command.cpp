#include "Command.h"

namespace TopOpt {

void CommandHistory::execute(std::unique_ptr<Command> cmd) {
    cmd->execute();
    undoStack_.push_back(std::move(cmd));
    redoStack_.clear();
    dirty_ = true;

    // Trim undo stack if too large
    if (undoStack_.size() > kMaxHistory) {
        undoStack_.erase(undoStack_.begin());
    }
}

void CommandHistory::undo() {
    if (undoStack_.empty()) return;
    auto cmd = std::move(undoStack_.back());
    undoStack_.pop_back();
    cmd->undo();
    redoStack_.push_back(std::move(cmd));
    dirty_ = true;
}

void CommandHistory::redo() {
    if (redoStack_.empty()) return;
    auto cmd = std::move(redoStack_.back());
    redoStack_.pop_back();
    cmd->execute();
    undoStack_.push_back(std::move(cmd));
    dirty_ = true;
}

bool CommandHistory::canUndo() const {
    return !undoStack_.empty();
}

bool CommandHistory::canRedo() const {
    return !redoStack_.empty();
}

std::string CommandHistory::undoDescription() const {
    if (undoStack_.empty()) return "";
    return undoStack_.back()->description();
}

std::string CommandHistory::redoDescription() const {
    if (redoStack_.empty()) return "";
    return redoStack_.back()->description();
}

void CommandHistory::clear() {
    undoStack_.clear();
    redoStack_.clear();
}

} // namespace TopOpt
