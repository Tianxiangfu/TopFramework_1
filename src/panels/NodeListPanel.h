#pragma once
#include "../node_editor/NodeEditor.h"

namespace TopOpt {

/**
 * Node palette panel - lists available nodes by category.
 * Users can click to add nodes to the canvas.
 */
class NodeListPanel {
public:
    void draw(NodeEditor& editor);

private:
    char searchBuf_[128] = {};
};

} // namespace TopOpt
