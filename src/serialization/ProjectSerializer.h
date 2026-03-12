#pragma once
#include <string>

namespace TopOpt {

class NodeEditor;

struct ViewState {
    float horizontalSplitRatio = 0.55f;
    float leftVertSplitRatio   = 0.65f;
    float centerVertSplitRatio = 0.65f;
    float rightPanelRatio      = 0.15f;
    float camDistance = 5.0f;
    float camYaw     = 45.0f;
    float camPitch   = 30.0f;
    float camCenter[3] = {0, 0, 0};
};

class ProjectSerializer {
public:
    static bool saveToFile(const std::string& path,
                           NodeEditor& editor,
                           const ViewState& view);

    static bool loadFromFile(const std::string& path,
                             NodeEditor& editor,
                             ViewState& view);
};

} // namespace TopOpt
