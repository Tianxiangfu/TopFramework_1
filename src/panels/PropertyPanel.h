#pragma once
#include "../node_editor/NodeEditor.h"

namespace TopOpt {

/**
 * Property panel - shows and edits properties of the selected node.
 */
class PropertyPanel {
public:
    void draw(NodeEditor& editor);
};

} // namespace TopOpt
