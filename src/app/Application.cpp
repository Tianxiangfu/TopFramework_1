#include "Application.h"
#include "../node_editor/NodeEditor.h"
#include "../node_editor/NodeRegistry.h"
#include "../panels/NodeListPanel.h"
#include "../panels/PropertyPanel.h"
#include "../panels/LogPanel.h"
#include "../panels/ModulePanel.h"
#include "../panels/View3DPanel.h"
#include "../utils/Logger.h"
#include "../utils/FileDialog.h"
#include "../commands/Command.h"
#include "../commands/NodeCommands.h"
#include "../serialization/ProjectSerializer.h"
#include "../execution/GraphExecutor.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <functional>

namespace TopOpt {

// Global app pointer for GLFW callbacks (single instance expected)
static Application* g_appInstance = nullptr;

// ================================================================
//  GLFW error callback
// ================================================================
static void glfwErrorCb(int error, const char* desc) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
}

// ================================================================
//  GLFW drop callback
// ================================================================
static void glfwDropCb(GLFWwindow* window, int pathCount, const char** paths) {
    if (g_appInstance) {
        g_appInstance->handleDroppedFiles(pathCount, paths);
    }
}

// ================================================================
//  Application
// ================================================================

Application::Application() {
    g_appInstance = this;
}

Application::~Application() {
    shutdown();
}

bool Application::init(int width, int height) {
    glfwSetErrorCallback(glfwErrorCb);
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SAMPLES, 4);

    window_ = glfwCreateWindow(width, height,
        "TopOptFrame - Node Visual Programming", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    // Enable drag & drop for file loading
    glfwSetDropCallback(window_, glfwDropCb);

    // --- ImGui setup ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "imgui_layout.ini";

    // Load fonts - large size for readability
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 26.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    io.FontDefault = io.Fonts->Fonts.back();

    // Polished dark theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 6.0f;
    style.FrameRounding     = 5.0f;
    style.GrabRounding      = 5.0f;
    style.TabRounding       = 5.0f;
    style.ScrollbarRounding = 8.0f;
    style.ScrollbarSize     = 14.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.ItemSpacing       = ImVec2(10, 7);
    style.ItemInnerSpacing  = ImVec2(8, 4);
    style.CellPadding       = ImVec2(8, 5);
    style.WindowPadding     = ImVec2(10, 10);
    style.FramePadding      = ImVec2(10, 6);
    style.IndentSpacing     = 22.0f;
    style.GrabMinSize       = 12.0f;
    style.SeparatorTextBorderSize = 2.0f;
    style.SeparatorTextAlign = ImVec2(0.5f, 0.5f);
    style.DisplaySafeAreaPadding = ImVec2(4, 4);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]           = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    c[ImGuiCol_ChildBg]            = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_PopupBg]            = ImVec4(0.16f, 0.17f, 0.20f, 0.97f);
    c[ImGuiCol_Border]             = ImVec4(0.22f, 0.24f, 0.28f, 0.70f);
    c[ImGuiCol_FrameBg]            = ImVec4(0.16f, 0.17f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBgHovered]     = ImVec4(0.22f, 0.24f, 0.29f, 1.00f);
    c[ImGuiCol_FrameBgActive]      = ImVec4(0.28f, 0.30f, 0.36f, 1.00f);
    c[ImGuiCol_TitleBg]            = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_TitleBgActive]      = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]   = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_MenuBarBg]          = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_ScrollbarBg]        = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]      = ImVec4(0.30f, 0.32f, 0.38f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.40f, 0.42f, 0.50f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]= ImVec4(0.50f, 0.52f, 0.60f, 1.00f);
    c[ImGuiCol_CheckMark]          = ImVec4(0.45f, 0.65f, 0.95f, 1.00f);
    c[ImGuiCol_SliderGrab]         = ImVec4(0.45f, 0.65f, 0.95f, 0.90f);
    c[ImGuiCol_SliderGrabActive]   = ImVec4(0.55f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_Button]             = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    c[ImGuiCol_ButtonHovered]      = ImVec4(0.28f, 0.32f, 0.40f, 1.00f);
    c[ImGuiCol_ButtonActive]       = ImVec4(0.38f, 0.55f, 0.82f, 1.00f);
    c[ImGuiCol_Header]             = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_HeaderHovered]      = ImVec4(0.24f, 0.28f, 0.35f, 1.00f);
    c[ImGuiCol_HeaderActive]       = ImVec4(0.38f, 0.55f, 0.82f, 0.85f);
    c[ImGuiCol_Tab]                = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
    c[ImGuiCol_TabHovered]         = ImVec4(0.28f, 0.32f, 0.40f, 1.00f);
    c[ImGuiCol_TabSelected]        = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
    c[ImGuiCol_TabDimmed]          = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    c[ImGuiCol_TabDimmedSelected]  = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
    c[ImGuiCol_ResizeGrip]         = ImVec4(0.45f, 0.65f, 0.95f, 0.20f);
    c[ImGuiCol_ResizeGripHovered]  = ImVec4(0.45f, 0.65f, 0.95f, 0.60f);
    c[ImGuiCol_ResizeGripActive]   = ImVec4(0.45f, 0.65f, 0.95f, 0.90f);
    c[ImGuiCol_Separator]          = ImVec4(0.22f, 0.24f, 0.28f, 0.80f);
    c[ImGuiCol_SeparatorHovered]   = ImVec4(0.35f, 0.42f, 0.55f, 1.00f);
    c[ImGuiCol_SeparatorActive]    = ImVec4(0.45f, 0.65f, 0.95f, 1.00f);
    c[ImGuiCol_TextSelectedBg]     = ImVec4(0.38f, 0.55f, 0.82f, 0.35f);
    c[ImGuiCol_Text]               = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
    c[ImGuiCol_TextDisabled]       = ImVec4(0.48f, 0.50f, 0.55f, 1.00f);
    c[ImGuiCol_DragDropTarget]     = ImVec4(0.45f, 0.65f, 0.95f, 0.80f);
    c[ImGuiCol_NavHighlight]       = ImVec4(0.45f, 0.65f, 0.95f, 0.80f);
    c[ImGuiCol_NavWindowingHighlight]= ImVec4(0.45f, 0.65f, 0.95f, 0.60f);
    c[ImGuiCol_NavWindowingDimBg]  = ImVec4(0.10f, 0.11f, 0.13f, 0.60f);
    c[ImGuiCol_ModalWindowDimBg]   = ImVec4(0.08f, 0.09f, 0.10f, 0.65f);

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // --- Create components ---
    cmdHistory_  = new CommandHistory();
    nodeEditor_  = new NodeEditor();
    nodeEditor_->setCommandHistory(cmdHistory_);
    nodeList_    = new NodeListPanel();
    propPanel_   = new PropertyPanel();
    logPanel_    = new LogPanel();
    modulePanel_ = new ModulePanel();
    view3D_      = new View3DPanel();

    executor_    = new GraphExecutor();
    executor_->setEditor(nodeEditor_);
    executor_->setView3D(view3D_);

    Logger::instance().info("TopOptFrame initialized");
    Logger::instance().info("Right-click canvas or use the Nodes panel to add nodes");

    running_ = true;
    updateWindowTitle();
    return true;
}

void Application::run() {
    while (running_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Handle keyboard shortcuts
        handleKeyboardShortcuts();

        // Full-window host
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("MainHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        drawMenuBar();
        drawToolbar();

        float statusH  = 34.f;
        float contentH = ImGui::GetContentRegionAvail().y - statusH;
        float splitterThickness = 6.0f;

        ImGui::BeginChild("ContentArea", ImVec2(0, contentH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        {
            float totalW    = ImGui::GetContentRegionAvail().x;
            float contentAvH = ImGui::GetContentRegionAvail().y;
            float usableW   = totalW - splitterThickness * 2;

            // Compute panel widths from ratios
            float leftW   = usableW * horizontalSplitRatio_;
            float rightW  = usableW * rightPanelRatio_;
            float centerW = usableW - leftW - rightW;

            // Enforce minimums
            if (leftW   < 200.0f) leftW   = 200.0f;
            if (rightW  < 120.0f) rightW  = 120.0f;
            if (centerW < 200.0f) centerW = 200.0f;

            // ==========================================
            //  LEFT PANEL
            // ==========================================
            ImGui::BeginChild("LeftPanel", ImVec2(leftW, 0), ImGuiChildFlags_Border);
            {
                drawLeftToolbar();
                ImGui::SameLine();

                ImGui::BeginChild("LeftPanelContent", ImVec2(0, 0), ImGuiChildFlags_None);
                {
                    float leftPanelH = ImGui::GetContentRegionAvail().y;
                    float leftAvailW = ImGui::GetContentRegionAvail().x;
                    float view3DH = (leftPanelH - splitterThickness) * leftVertSplitRatio_;

                    ImGui::BeginChild("View3DContainer", ImVec2(0, view3DH), ImGuiChildFlags_Border);
                    view3D_->draw();
                    ImGui::EndChild();

                    ImGui::InvisibleButton("##VSplitLeft", ImVec2(leftAvailW, splitterThickness));
                    {
                        bool hov = ImGui::IsItemHovered();
                        bool act = ImGui::IsItemActive();
                        if (hov || act) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                        if (act) {
                            leftVertSplitRatio_ += ImGui::GetIO().MouseDelta.y / (leftPanelH - splitterThickness);
                            leftVertSplitRatio_ = ImClamp(leftVertSplitRatio_, 0.15f, 0.85f);
                        }
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        ImVec2 pMin = ImGui::GetItemRectMin();
                        ImVec2 pMax = ImGui::GetItemRectMax();
                        ImU32 col = (hov || act) ? IM_COL32(102, 153, 230, 255) : IM_COL32(46, 49, 56, 255);
                        dl->AddRectFilled(pMin, pMax, col);
                    }

                    ImGui::BeginChild("ResourceBrowser", ImVec2(0, 0), ImGuiChildFlags_Border);
                    {
                        if (ImGui::BeginTabBar("BrowserTabs")) {
                            if (ImGui::BeginTabItem("Browse")) {
                                ImGui::BeginChild("BrowserScroll");
                                modulePanel_->draw();
                                ImGui::EndChild();
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Log")) {
                                logPanel_->draw();
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Console")) {
                                ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 0.7f), "Console output");
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                    }
                    ImGui::EndChild();
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();

            // ==========================================
            //  HORIZONTAL SPLITTER
            // ==========================================
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::InvisibleButton("##HSplit", ImVec2(splitterThickness, contentAvH));
            {
                bool hov = ImGui::IsItemHovered();
                bool act = ImGui::IsItemActive();
                if (hov || act) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                if (act && usableW > 0.0f) {
                    horizontalSplitRatio_ += ImGui::GetIO().MouseDelta.x / usableW;
                    horizontalSplitRatio_ = ImClamp(horizontalSplitRatio_, 0.15f, 0.80f);
                }
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 pMin = ImGui::GetItemRectMin();
                ImVec2 pMax = ImGui::GetItemRectMax();
                ImU32 col = (hov || act) ? IM_COL32(102, 153, 230, 255) : IM_COL32(46, 49, 56, 255);
                dl->AddRectFilled(pMin, pMax, col);
            }

            ImGui::SameLine(0.0f, 0.0f);

            // ==========================================
            //  CENTER PANEL
            // ==========================================
            ImGui::BeginChild("CenterPanel", ImVec2(centerW, 0), ImGuiChildFlags_None);
            {
                float centerPanelH = ImGui::GetContentRegionAvail().y;
                float centerAvailW = ImGui::GetContentRegionAvail().x;
                float canvasH = (centerPanelH - splitterThickness) * centerVertSplitRatio_;

                ImGui::BeginChild("NodeCanvasPanel", ImVec2(0, canvasH), ImGuiChildFlags_Border);
                {
                    if (ImGui::SmallButton("+")) {
                        Logger::instance().info("Add node via context menu");
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add Node (right-click canvas)");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("-")) {
                        // Use command for undo support
                        int numSel = 0;
                        if (nodeEditor_->isInitialized()) {
                            // Will be handled by NodeEditor
                        }
                        nodeEditor_->removeSelectedNodes();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove Selected (Del)");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) {
                        auto cmd = std::make_unique<ClearAllCmd>(*nodeEditor_);
                        cmdHistory_->execute(std::move(cmd));
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear All Nodes");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.40f, 0.40f, 0.45f, 0.5f), "Right-click to add nodes");
                    ImGui::Separator();

                    ImGui::BeginChild("NodeCanvas", ImVec2(0, 0), ImGuiChildFlags_None);
                    nodeEditor_->draw();
                    ImGui::EndChild();

                    // Accept drag-drop from NodeListPanel
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_TYPE")) {
                            std::string typeName((const char*)payload->Data);
                            ImVec2 dropPos = ImGui::GetMousePos();
                            nodeEditor_->addNode(typeName, dropPos.x, dropPos.y);
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
                ImGui::EndChild();

                ImGui::InvisibleButton("##VSplitCenter", ImVec2(centerAvailW, splitterThickness));
                {
                    bool hov = ImGui::IsItemHovered();
                    bool act = ImGui::IsItemActive();
                    if (hov || act) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    if (act) {
                        centerVertSplitRatio_ += ImGui::GetIO().MouseDelta.y / (centerPanelH - splitterThickness);
                        centerVertSplitRatio_ = ImClamp(centerVertSplitRatio_, 0.15f, 0.85f);
                    }
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 pMin = ImGui::GetItemRectMin();
                    ImVec2 pMax = ImGui::GetItemRectMax();
                    ImU32 col = (hov || act) ? IM_COL32(102, 153, 230, 255) : IM_COL32(46, 49, 56, 255);
                    dl->AddRectFilled(pMin, pMax, col);
                }

                ImGui::BeginChild("PropertyPanel", ImVec2(0, 0), ImGuiChildFlags_Border);
                {
                    ImGui::TextColored(ImVec4(0.55f, 0.70f, 0.92f, 1.0f), "Properties");
                    ImGui::Separator();
                    ImGui::BeginChild("PropertyContent", ImVec2(0, 0), ImGuiChildFlags_None);
                    propPanel_->draw(*nodeEditor_);
                    ImGui::EndChild();
                }
                ImGui::EndChild();

                // Live preview: update 3D view when selection or params change
                updateLivePreview();
            }
            ImGui::EndChild();

            // ==========================================
            //  RIGHT SPLITTER
            // ==========================================
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::InvisibleButton("##HSplitRight", ImVec2(splitterThickness, contentAvH));
            {
                bool hov = ImGui::IsItemHovered();
                bool act = ImGui::IsItemActive();
                if (hov || act) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                if (act && usableW > 0.0f) {
                    rightPanelRatio_ -= ImGui::GetIO().MouseDelta.x / usableW;
                    rightPanelRatio_ = ImClamp(rightPanelRatio_, 0.08f, 0.40f);
                }
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 pMin = ImGui::GetItemRectMin();
                ImVec2 pMax = ImGui::GetItemRectMax();
                ImU32 col = (hov || act) ? IM_COL32(102, 153, 230, 255) : IM_COL32(46, 49, 56, 255);
                dl->AddRectFilled(pMin, pMax, col);
            }

            ImGui::SameLine(0.0f, 0.0f);

            // ==========================================
            //  RIGHT SIDEBAR
            // ==========================================
            ImGui::BeginChild("NodeLibrary", ImVec2(rightW, 0), ImGuiChildFlags_Border);
            {
                ImGui::TextColored(ImVec4(0.55f, 0.70f, 0.92f, 1.0f), "Node Library");
                ImGui::Separator();
                ImGui::BeginChild("NodeLibContent", ImVec2(0, 0), ImGuiChildFlags_None);
                nodeList_->draw(*nodeEditor_);
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        drawStatusBar();

        ImGui::End(); // MainHost

        ImGui::Render();
        int dispW, dispH;
        glfwGetFramebufferSize(window_, &dispW, &dispH);
        glViewport(0, 0, dispW, dispH);
        glClearColor(0.09f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

void Application::shutdown() {
    delete executor_;    executor_    = nullptr;
    delete cmdHistory_;  cmdHistory_  = nullptr;
    delete nodeEditor_;  nodeEditor_  = nullptr;
    delete nodeList_;    nodeList_    = nullptr;
    delete propPanel_;   propPanel_   = nullptr;
    delete logPanel_;    logPanel_    = nullptr;
    delete modulePanel_; modulePanel_ = nullptr;
    delete view3D_;      view3D_      = nullptr;

    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }
}

// ================================================================
//  File operations
// ================================================================

void Application::newProject() {
    auto cmd = std::make_unique<ClearAllCmd>(*nodeEditor_);
    cmdHistory_->execute(std::move(cmd));
    currentFilePath_.clear();
    cmdHistory_->markClean();
    updateWindowTitle();
    Logger::instance().info("New project created");
}

void Application::openProject() {
    std::string path = FileDialog::openFile(window_);
    if (path.empty()) return;

    ViewState view;
    if (ProjectSerializer::loadFromFile(path, *nodeEditor_, view)) {
        horizontalSplitRatio_ = view.horizontalSplitRatio;
        leftVertSplitRatio_   = view.leftVertSplitRatio;
        centerVertSplitRatio_ = view.centerVertSplitRatio;
        rightPanelRatio_      = view.rightPanelRatio;
        view3D_->setCameraState(view.camDistance, view.camYaw, view.camPitch,
                                view.camCenter[0], view.camCenter[1], view.camCenter[2]);
        currentFilePath_ = path;
        cmdHistory_->clear();
        cmdHistory_->markClean();
        updateWindowTitle();
    }
}

void Application::saveProject() {
    if (currentFilePath_.empty()) {
        saveProjectAs();
        return;
    }

    Logger::instance().info("Attempting to save to: " + currentFilePath_);

    ViewState view;
    view.horizontalSplitRatio = horizontalSplitRatio_;
    view.leftVertSplitRatio   = leftVertSplitRatio_;
    view.centerVertSplitRatio = centerVertSplitRatio_;
    view.rightPanelRatio      = rightPanelRatio_;
    view3D_->getCameraState(view.camDistance, view.camYaw, view.camPitch,
                            view.camCenter[0], view.camCenter[1], view.camCenter[2]);

    bool success = ProjectSerializer::saveToFile(currentFilePath_, *nodeEditor_, view);
    if (success) {
        cmdHistory_->markClean();
        updateWindowTitle();
        Logger::instance().info("Save successful!");
    } else {
        Logger::instance().error("Save failed - check previous error messages");
    }
}

void Application::saveProjectAs() {
    std::string path = FileDialog::saveFile(window_);
    Logger::instance().info("Save dialog returned path: '" + path + "'");
    if (path.empty()) {
        Logger::instance().warn("Save cancelled or dialog failed");
        return;
    }

    currentFilePath_ = path;
    Logger::instance().info("Setting current file path to: " + currentFilePath_);
    saveProject();
}

void Application::updateWindowTitle() {
    std::string title = "TopOptFrame";
    if (!currentFilePath_.empty()) {
        // Extract filename from path
        size_t pos = currentFilePath_.find_last_of("/\\");
        std::string fname = (pos != std::string::npos)
            ? currentFilePath_.substr(pos + 1) : currentFilePath_;
        title += " - " + fname;
    }
    if (cmdHistory_ && cmdHistory_->isDirty()) {
        title += " *";
    }
    if (window_) {
        glfwSetWindowTitle(window_, title.c_str());
    }
}

// ================================================================
//  Keyboard shortcuts
// ================================================================

void Application::handleKeyboardShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        newProject();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        openProject();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        saveProject();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        cmdHistory_->undo();
        updateWindowTitle();
        Logger::instance().info("Undo: " + cmdHistory_->redoDescription());
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        cmdHistory_->redo();
        updateWindowTitle();
        Logger::instance().info("Redo: " + cmdHistory_->undoDescription());
    }
}

// ================================================================
//  Menu bar
// ================================================================
void Application::drawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        // Brand logo with accent color
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.65f, 0.95f, 1.0f));
        ImGui::TextUnformatted("TopOpt");
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::TextColored(ImVec4(0.40f, 0.42f, 0.48f, 1.0f), "|");
        ImGui::SameLine(0.0f, 6.0f);

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                newProject();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                openProject();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                saveProject();
            }
            if (ImGui::MenuItem("Save As...")) {
                saveProjectAs();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                running_ = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, cmdHistory_->canUndo())) {
                cmdHistory_->undo();
                updateWindowTitle();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, cmdHistory_->canRedo())) {
                cmdHistory_->redo();
                updateWindowTitle();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected", "Del")) {
                nodeEditor_->removeSelectedNodes();
            }
            if (ImGui::MenuItem("Clear All")) {
                auto cmd = std::make_unique<ClearAllCmd>(*nodeEditor_);
                cmdHistory_->execute(std::move(cmd));
                updateWindowTitle();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::SeparatorText("Panel Layout");
            float leftPct2 = horizontalSplitRatio_ * 100.0f;
            if (ImGui::SliderFloat("Left Panel", &leftPct2, 15.0f, 80.0f, "%.0f%%")) {
                horizontalSplitRatio_ = leftPct2 / 100.0f;
            }
            float leftPct = leftVertSplitRatio_ * 100.0f;
            if (ImGui::SliderFloat("3D View", &leftPct, 15.0f, 85.0f, "%.0f%%")) {
                leftVertSplitRatio_ = leftPct / 100.0f;
            }
            float centerPct = centerVertSplitRatio_ * 100.0f;
            if (ImGui::SliderFloat("Canvas", &centerPct, 15.0f, 85.0f, "%.0f%%")) {
                centerVertSplitRatio_ = centerPct / 100.0f;
            }
            ImGui::SeparatorText("Options");
            ImGui::Checkbox("Grid Snap", &gridSnapEnabled_);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                Logger::instance().info("TopOpt v0.1.0 - Node Visual Programming Framework");
            }
            ImGui::EndMenu();
        }

        // Right-aligned status in menu bar
        float barW = ImGui::GetWindowWidth();
        float rightTextW = 90.0f;
        ImGui::SameLine(barW - rightTextW);
        if (isExecuting_) {
            ImGui::TextColored(ImVec4(0.45f, 0.75f, 0.50f, 1.0f), "Running");
        } else {
            ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.58f, 1.0f), "Ready");
        }

        ImGui::EndMenuBar();
    }
}

// ================================================================
//  Toolbar
// ================================================================
void Application::drawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

    float toolbarH = 40.f;
    ImGui::BeginChild("Toolbar", ImVec2(0, toolbarH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    // --- File group ---
    if (ImGui::Button("New")) { newProject(); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("New Project (Ctrl+N)");
    ImGui::SameLine();
    if (ImGui::Button("Open")) { openProject(); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open Project (Ctrl+O)");
    ImGui::SameLine();
    if (ImGui::Button("Save")) { saveProject(); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save Project (Ctrl+S)");

    // Separator
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextColored(ImVec4(0.30f, 0.32f, 0.36f, 1.0f), "|");
    ImGui::SameLine(0.0f, 6.0f);

    // --- Undo/Redo group ---
    {
        bool canUndo = cmdHistory_->canUndo();
        if (!canUndo) ImGui::BeginDisabled();
        if (ImGui::Button("Undo")) {
            cmdHistory_->undo();
            updateWindowTitle();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            std::string tip = "Undo (Ctrl+Z)";
            if (canUndo) tip += "\n" + cmdHistory_->undoDescription();
            ImGui::SetTooltip("%s", tip.c_str());
        }
        if (!canUndo) ImGui::EndDisabled();
    }
    ImGui::SameLine();
    {
        bool canRedo = cmdHistory_->canRedo();
        if (!canRedo) ImGui::BeginDisabled();
        if (ImGui::Button("Redo")) {
            cmdHistory_->redo();
            updateWindowTitle();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            std::string tip = "Redo (Ctrl+Y)";
            if (canRedo) tip += "\n" + cmdHistory_->redoDescription();
            ImGui::SetTooltip("%s", tip.c_str());
        }
        if (!canRedo) ImGui::EndDisabled();
    }

    // Separator
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextColored(ImVec4(0.30f, 0.32f, 0.36f, 1.0f), "|");
    ImGui::SameLine(0.0f, 6.0f);

    // --- Execution group ---
    if (isExecuting_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Stop")) {
            isExecuting_ = false;
            Logger::instance().info("Execution stopped");
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.60f, 0.40f, 1.0f));
        if (ImGui::Button("Run")) {
            isExecuting_ = true;
            if (executor_) executor_->runAll();
            isExecuting_ = false;
        }
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(isExecuting_ ? "Stop Execution" : "Run Node Graph");
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        if (executor_) executor_->stepOne();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Execute One Step");
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        isExecuting_ = false;
        if (executor_) executor_->reset();
        if (view3D_) view3D_->clearModel();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Execution State");

    ImGui::EndChild();
    ImGui::Separator();
    ImGui::PopStyleVar(2);
}

// ================================================================
//  Left toolbar (with active tool highlighting)
// ================================================================
void Application::drawLeftToolbar() {
    ImGui::BeginChild("LeftToolbar", ImVec2(40, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));

    float buttonSize = 30.0f;
    ImVec4 activeColor(0.35f, 0.55f, 0.85f, 1.0f);
    ImVec4 normalColor = ImGui::GetStyleColorVec4(ImGuiCol_Button);

    // Helper lambda for tool buttons
    auto toolButton = [&](const char* label, const char* tooltip, Tool tool) {
        bool isActive = (activeTool_ == tool);
        if (isActive) ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
        ImGui::PushID(label);
        if (ImGui::Button(label, ImVec2(buttonSize, buttonSize))) {
            activeTool_ = tool;
        }
        ImGui::PopID();
        if (isActive) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    };

    toolButton("S", "Select / Move Tool", Tool::Select);
    toolButton("Z", "Zoom Tool", Tool::Zoom);
    toolButton("R", "Rotate Tool", Tool::Rotate);

    // Grid snap toggle
    {
        if (gridSnapEnabled_) ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
        ImGui::PushID("tool_grid");
        if (ImGui::Button("G", ImVec2(buttonSize, buttonSize))) {
            gridSnapEnabled_ = !gridSnapEnabled_;
            Logger::instance().info(gridSnapEnabled_ ? "Grid snap enabled" : "Grid snap disabled");
        }
        ImGui::PopID();
        if (gridSnapEnabled_) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid Snap (Toggle)");
    }

    toolButton("C", "Camera / Orbit Tool", Tool::Camera);

    ImGui::Separator();

    // View preset buttons
    ImGui::PushID("view_reset");
    if (ImGui::Button("H", ImVec2(buttonSize, buttonSize))) {
        view3D_->resetCamera();
        Logger::instance().info("Camera reset to home");
    }
    ImGui::PopID();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Camera (Home)");

    ImGui::PushID("view_fit");
    if (ImGui::Button("F", ImVec2(buttonSize, buttonSize))) {
        view3D_->setViewMode(0); // Perspective
        Logger::instance().info("Fit to view");
    }
    ImGui::PopID();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fit / Perspective View");

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
}

// ================================================================
//  Status bar
// ================================================================
void Application::drawStatusBar() {
    ImGui::Separator();
    ImGui::BeginChild("StatusBar", ImVec2(0, 30), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    // Left: node/connection counts
    ImGui::TextColored(ImVec4(0.62f, 0.64f, 0.70f, 1.0f),
        "Nodes: %d  |  Connections: %d",
        nodeEditor_->nodeCount(),
        nodeEditor_->connectionCount());

    // Center: current file
    if (!currentFilePath_.empty()) {
        ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
        size_t pos = currentFilePath_.find_last_of("/\\");
        std::string fname = (pos != std::string::npos)
            ? currentFilePath_.substr(pos + 1) : currentFilePath_;
        ImGui::TextColored(ImVec4(0.50f, 0.65f, 0.85f, 1.0f), "%s", fname.c_str());
    }

    // Right: tool + version
    ImGui::SameLine(ImGui::GetWindowWidth() - 220);
    const char* toolNames[] = { "Select", "Zoom", "Rotate", "Camera" };
    ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.70f, 1.0f), "Tool: %s", toolNames[(int)activeTool_]);
    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.52f, 1.0f), "v0.1.0");

    ImGui::EndChild();
}

// ================================================================
//  Live preview
// ================================================================

int Application::computeParamHash(int nodeId) const {
    auto* node = nodeEditor_->findNode(nodeId);
    if (!node) return 0;

    // Simple hash combining all parameter values
    int hash = 0;
    for (auto& p : node->params) {
        // Use a simple combination of parameter values
        int h = std::hash<std::string>{}(p.name);
        switch (p.type) {
            case ParamType::Float: {
                int bits;
                std::memcpy(&bits, &p.floatVal, sizeof(int));
                h ^= bits;
                break;
            }
            case ParamType::Int:    h ^= p.intVal * 2654435761; break;
            case ParamType::Bool:   h ^= p.boolVal ? 1 : 0; break;
            case ParamType::String: h ^= std::hash<std::string>{}(p.stringVal); break;
            case ParamType::Enum:   h ^= p.enumIndex * 2246822519; break;
            case ParamType::Color3: {
                int b0, b1, b2;
                std::memcpy(&b0, &p.color3[0], sizeof(int));
                std::memcpy(&b1, &p.color3[1], sizeof(int));
                std::memcpy(&b2, &p.color3[2], sizeof(int));
                h ^= b0 ^ b1 ^ b2;
                break;
            }
        }
        hash = hash * 31 + h;
    }
    return hash;
}

void Application::updateLivePreview() {
    int selId = nodeEditor_->selectedNodeId();
    if (selId < 0) {
        prevSelectedNodeId_ = -1;
        return;
    }

    int paramHash = computeParamHash(selId);

    // Preview if selection changed or params changed
    if (selId != prevSelectedNodeId_ || paramHash != prevParamHash_) {
        prevSelectedNodeId_ = selId;
        prevParamHash_ = paramHash;

        // Skip heavy nodes (SIMP/BESO/FEA solver) for live preview - only on explicit Run
        auto* node = nodeEditor_->findNode(selId);
        if (node) {
            const std::string& t = node->typeName;
            if (t == "topo-simp" || t == "topo-beso" || t == "fea-solver") {
                return; // Skip expensive computations
            }
        }

        executor_->previewNode(selId);
    }
}

// ================================================================
//  Drag & drop file handling
// ================================================================

void Application::handleDroppedFiles(int pathCount, const char** paths) {
    if (pathCount <= 0 || !paths || !view3D_) return;

    for (int i = 0; i < pathCount; i++) {
        std::string filePath = paths[i];

        // Check file extension
        if (filePath.length() < 5) continue;
        std::string ext = filePath.substr(filePath.length() - 4);

        // Convert to lowercase for comparison
        for (auto& c : ext) c = tolower(c);

        bool loaded = false;
        if (ext == ".stl") {
            loaded = view3D_->loadSTL(filePath.c_str());
        } else if (ext == ".obj") {
            loaded = view3D_->loadOBJ(filePath.c_str());
        }

        if (loaded) {
            Logger::instance().info("Loaded 3D model: " + filePath);
        } else {
            Logger::instance().warn("Failed to load: " + filePath);
        }
    }
}

} // namespace TopOpt
