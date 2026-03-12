#pragma once
#include <memory>
#include <vector>
#include <string>

namespace TopOpt {

class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string description() const = 0;
};

class CommandHistory {
public:
    void execute(std::unique_ptr<Command> cmd);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    std::string undoDescription() const;
    std::string redoDescription() const;
    void clear();
    bool isDirty() const { return dirty_; }
    void markClean() { dirty_ = false; }

private:
    std::vector<std::unique_ptr<Command>> undoStack_;
    std::vector<std::unique_ptr<Command>> redoStack_;
    bool dirty_ = false;
    static const size_t kMaxHistory = 100;
};

} // namespace TopOpt
