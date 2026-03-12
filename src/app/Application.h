#pragma once
#include <string>

struct GLFWwindow;

namespace TopOpt {

class NodeEditor;
class NodeListPanel;
class PropertyPanel;
class LogPanel;
class ModulePanel;
class View3DPanel;
class CommandHistory;
class GraphExecutor;

// Active tool for left toolbar
enum class Tool { Select, Zoom, Rotate, Camera };

class Application {
public:
    Application();
    ~Application();

    bool init(int width = 1400, int height = 900);
    void run();
    void shutdown();

    // Drag-drop file handling (public for GLFW callback)
    void handleDroppedFiles(int pathCount, const char** paths);

private:
    void drawMenuBar();
    void drawToolbar();
    void drawStatusBar();
    void drawLeftToolbar();
    void handleKeyboardShortcuts();

    // File operations
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void updateWindowTitle();

    GLFWwindow*     window_       = nullptr;
    NodeEditor*     nodeEditor_   = nullptr;
    NodeListPanel*  nodeList_     = nullptr;
    PropertyPanel*  propPanel_    = nullptr;
    LogPanel*       logPanel_     = nullptr;
    ModulePanel*    modulePanel_  = nullptr;
    View3DPanel*    view3D_       = nullptr;
    CommandHistory* cmdHistory_   = nullptr;
    GraphExecutor*  executor_     = nullptr;

    bool running_     = false;
    bool isExecuting_ = false;

    // File state
    std::string currentFilePath_;

    // Tool state
    Tool activeTool_ = Tool::Select;
    bool gridSnapEnabled_ = false;

    // Splitter state (ratios for proportional resizing)
    float horizontalSplitRatio_ = 0.55f;   // left panel width ratio
    float leftVertSplitRatio_   = 0.65f;
    float centerVertSplitRatio_ = 0.65f;
    float rightPanelRatio_      = 0.15f;   // right panel width ratio

    int activeTab_ = 0;

    // Live preview tracking
    int prevSelectedNodeId_ = -1;
    int prevParamHash_ = 0;
    int computeParamHash(int nodeId) const;
    void updateLivePreview();
};

} // namespace TopOpt
