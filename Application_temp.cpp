#include "Application.h"
#include "../node_editor/NodeEditor.h"
#include "../node_editor/NodeRegistry.h"
#include "../panels/NodeListPanel.h"
#include "../panels/PropertyPanel.h"
#include "../panels/LogPanel.h"
#include "../panels/ModulePanel.h"
#include "../panels/View3DPanel.h"
#include "../utils/Logger.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstdio>

namespace TopOpt {

// ================================================================
//  GLFW error callback
// ================================================================
static void glfwErrorCb(int error, const char* desc) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
}

// ================================================================
//  Application
// ================================================================

Application::Application() {}

Application::~Application() {
    shutdown();
}

bool Application::init(int width, int height) {
    glfwSetErrorCallback(glfwErrorCb);
    if (!glfwInit()) return false;

    // GL 3.0 + GLSL 130 (works on most systems)
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
    glfwSwapInterval(1); // vsync

    // --- ImGui setup ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "imgui_layout.ini";

    // Load fonts with better Chinese support
    float scale = 1.0f;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 15.0f * scale, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    io.FontDefault = io.Fonts->Fonts.back();

    // Dark theme matching PeriDyno Studio style (VS Code-like dark theme)
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 6.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.ItemSpacing       = ImVec2(12, 6);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.CellPadding       = ImVec2(6, 4);
    style.WindowPadding     = ImVec2(10, 10);
    style.FramePadding      = ImVec2(8, 5);
    style.IndentSpacing     = 22.0f;
    style.SeparatorTextBorderSize = 2.0f;
    style.SeparatorTextAlign = ImVec2(0.5f, 0.5f);
    style.DisplaySafeAreaPadding = ImVec2(4, 4);

    ImVec4* c = style.Colors;
    // Academic style - refined dark theme with blue accents
    c[ImGuiCol_WindowBg]           = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_ChildBg]            = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    c[ImGuiCol_PopupBg]            = ImVec4(0.15f, 0.16f, 0.19f, 0.96f);
    c[ImGuiCol_Border]             = ImVec4(0.25f, 0.26f, 0.30f, 1.00f);
    c[ImGuiCol_FrameBg]            = ImVec4(0.18f, 0.19f, 0.22f, 1.00f);
    c[ImGuiCol_FrameBgHovered]     = ImVec4(0.22f, 0.23f, 0.27f, 1.00f);
    c[ImGuiCol_FrameBgActive]      = ImVec4(0.26f, 0.27f, 0.32f, 1.00f);
    c[ImGuiCol_TitleBg]            = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_TitleBgActive]      = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]   = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_MenuBarBg]          = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_ScrollbarBg]        = ImVec4(0.06f, 0.07f, 0.08f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]      = ImVec4(0.30f, 0.31f, 0.36f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.38f, 0.39f, 0.45f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]= ImVec4(0.42f, 0.43f, 0.50f, 1.00f);
    c[ImGuiCol_CheckMark]          = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);
    c[ImGuiCol_SliderGrab]         = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);
    c[ImGuiCol_SliderGrabActive]   = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
    c[ImGuiCol_Button]             = ImVec4(0.18f, 0.19f, 0.22f, 1.00f);
    c[ImGuiCol_ButtonHovered]      = ImVec4(0.24f, 0.25f, 0.29f, 1.00f);
    c[ImGuiCol_ButtonActive]       = ImVec4(0.40f, 0.60f, 0.90f, 0.80f);
    c[ImGuiCol_Header]             = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
    c[ImGuiCol_HeaderHovered]      = ImVec4(0.20f, 0.21f, 0.25f, 1.00f);
    c[ImGuiCol_HeaderActive]       = ImVec4(0.40f, 0.60f, 0.90f, 0.80f);
    c[ImGuiCol_Tab]                = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_TabHovered]         = ImVec4(0.20f, 0.21f, 0.25f, 1.00f);
    c[ImGuiCol_TabSelected]        = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
    c[ImGuiCol_TabDimmed]          = ImVec4(0.07f, 0.08f, 0.09f, 1.00f);
    c[ImGuiCol_TabDimmedSelected]  = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_ResizeGrip]         = ImVec4(0.40f, 0.60f, 0.90f, 0.20f);
    c[ImGuiCol_ResizeGripHovered]  = ImVec4(0.40f, 0.60f, 0.90f, 0.60f);
    c[ImGuiCol_ResizeGripActive]   = ImVec4(0.40f, 0.60f, 0.90f, 0.90f);
    c[ImGuiCol_Separator]          = ImVec4(0.20f, 0.21f, 0.25f, 1.00f);
    c[ImGuiCol_SeparatorHovered]   = ImVec4(0.30f, 0.31f, 0.36f, 1.00f);
    c[ImGuiCol_SeparatorActive]    = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);
    c[ImGuiCol_TextSelectedBg]     = ImVec4(0.40f, 0.60f, 0.90f, 0.30f);
    c[ImGuiCol_Text]               = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    c[ImGuiCol_TextDisabled]       = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
    c[ImGuiCol_DragDropTarget]     = ImVec4(0.40f, 0.60f, 0.90f, 0.70f);
    c[ImGuiCol_NavHighlight]       = ImVec4(0.40f, 0.60f, 0.90f, 0.70f);
    c[ImGuiCol_NavWindowingHighlight]= ImVec4(0.40f, 0.60f, 0.90f, 0.50f);
    c[ImGuiCol_NavWindowingDimBg]  = ImVec4(0.10f, 0.11f, 0.13f, 0.50f);
    c[ImGuiCol_ModalWindowDimBg]   = ImVec4(0.08f, 0.09f, 0.10f, 0.60f);

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // --- Create components ---
    nodeEditor_  = new NodeEditor();
    nodeList_    = new NodeListPanel();
    propPanel_   = new PropertyPanel();
    logPanel_    = new LogPanel();
    modulePanel_ = new ModulePanel();
    view3D_      = new View3DPanel();

    Logger::instance().info("TopOptFrame initialized");
    Logger::instance().info("Right-click canvas or use the Nodes panel to add nodes");

    running_ = true;
    return true;
}

void Application::run() {
    while (running_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

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

        // Menu bar
        drawMenuBar();

        // Toolbar
        drawToolbar();

        // Content area: Left panel (3D view + browser) + Right panel (Node editor)
        float statusH  = 24.f;
        float contentH = ImGui::GetContentRegionAvail().y - statusH;

        ImGui::BeginChild("ContentArea", ImVec2(0, contentH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        {
            float totalW = ImGui::GetContentRegionAvail().x;
            float leftPanelW = leftPanelWidth_;
            float rightPanelW = totalW - leftPanelW - 4.f;

            // ---- Left Panel: 3D View + Resource Browser ----
            ImGui::BeginChild("LeftPanel", ImVec2(leftPanelW, 0), ImGuiChildFlags_Border);
            {
                // Left toolbar (vertical icons)
                drawLeftToolbar();

                ImGui::SameLine();

                // Left panel content
                ImGui::BeginChild("LeftPanelContent", ImVec2(0, 0), ImGuiChildFlags_None);
                {
                    // 3D Viewport (top part of left panel)
                    float view3DH = ImGui::GetContentRegionAvail().y * 0.65f;
                    ImGui::BeginChild("View3DContainer", ImVec2(0, view3DH), ImGuiChildFlags_Border);
                    view3D_->draw();
                    ImGui::EndChild();

                    // Resource Browser (bottom part of left panel)
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

            ImGui::SameLine();

            // ---- Right Panel: Node Editor ----
            ImGui::BeginChild("RightPanel", ImVec2(0, 0), ImGuiChildFlags_Border);
            {
                if (ImGui::BeginTabBar("RightTabs")) {
                    if (ImGui::BeginTabItem("Node Editor")) {
                        ImGui::BeginChild("NodeCanvas");
                        nodeEditor_->draw();
                        ImGui::EndChild();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Nodes")) {
                        ImGui::BeginChild("NodeListScroll");
                        nodeList_->draw(*nodeEditor_);
                        ImGui::EndChild();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Properties")) {
                        ImGui::BeginChild("PropScroll");
                        propPanel_->draw(*nodeEditor_);
                        ImGui::EndChild();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        // Status bar
        drawStatusBar();

        ImGui::End(); // MainHost

        // Render
        ImGui::Render();
        int dispW, dispH;
        glfwGetFramebufferSize(window_, &dispW, &dispH);
        glViewport(0, 0, dispW, dispH);
        glClearColor(0.10f, 0.10f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

void Application::shutdown() {
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
//  Menu bar
// ================================================================
void Application::drawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        // Brand - simplified
        // ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.60f, 0.90f, 1.0f));
        // ImGui::TextUnformatted("TopOpt");
        // ImGui::PopStyleColor();
        // ImGui::SameLine(0.0f, 20.0f);

        // File menu
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                nodeEditor_->clear();
                Logger::instance().info("New project");
            }
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                Logger::instance().info("Open file");
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                Logger::instance().info("Save project");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                running_ = false;
            }
            ImGui::EndMenu();
        }

        // Edit menu
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                Logger::instance().info("Undo operation");
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                Logger::instance().info("Redo operation");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected", "Del")) {
                nodeEditor_->removeSelectedNodes();
            }
            if (ImGui::MenuItem("Clear"){
                nodeEditor_->clear();
            }
            ImGui::EndMenu();
        }

        // View menu
        if (ImGui::BeginMenu("View")) {
            ImGui::DragFloat("Left Panel Width", &leftPanelWidth_, 1.f, 200.f, 600.f, "%.0f");
            ImGui::EndMenu();
        }

        // Help menu
        if (ImGui::BeginMenu("Help {
            if (ImGui::MenuItem("About")) {
                Logger::instance().info("TopOpt v0.1.0";
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

// ================================================================
//  Toolbar
// ================================================================
void Application::drawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 4));

    float toolbarH = 36.f;
    ImGui::BeginChild("Toolbar", ImVec2(0, toolbarH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    // File operations group
    if (ImGui::Button("New")) {
        nodeEditor_->clear();
        Logger::instance().info("New project");
    }
    ImGui::SameLine();
    if (ImGui::Button("Open")) {
        Logger::instance().info("Open file");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        Logger::instance().info("Save project");
    }

    ImGui::SameLine(0.0f, 15.0f);

    // Undo/Redo
    if (ImGui::Button("Undo")) {
        Logger::instance().info("Undo operation");
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo")) {
        Logger::instance().info("Redo operation");
    }

    ImGui::SameLine(0.0f, 15.0f);

    // Execution controls
    if (isExecuting_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("鍋滄")) {
            isExecuting_ = false;
            Logger::instance().info("鎵ц鍋滄");
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.65f, 0.45f, 1.0f));
        if (ImGui::Button("杩愯")) {
            isExecuting_ = true;
            Logger::instance().info("鎵ц寮€濮?);
        }
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ImGui::Button("鍗曟")) {
        Logger::instance().info("鍗曟鎵ц");
    }
    ImGui::SameLine();
    if (ImGui::Button("閲嶇疆")) {
        isExecuting_ = false;
        Logger::instance().info("閲嶇疆");
    }

    // Status on right
    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    if (isExecuting_) {
        ImGui::TextColored(ImVec4(0.50f, 0.75f, 0.55f, 1.0f), "杩愯涓?..");
    } else {
        ImGui::TextColored(ImVec4(0.60f, 0.60f, 0.65f, 1.0f), "灏辩华");
    }

    ImGui::EndChild();
    ImGui::Separator();
    ImGui::PopStyleVar(2);
}

// ================================================================
//  Left toolbar
// ================================================================
void Application::drawLeftToolbar() {
    ImGui::BeginChild("LeftToolbar", ImVec2(32, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

    float buttonSize = 24.0f;

    // Select/Move tool
    ImGui::PushID("tool_select");
    if (ImGui::Button("S", ImVec2(buttonSize, buttonSize))) {
        Logger::instance().info("Select tool activated");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Select/Move");
    }
    ImGui::PopID();

    // Zoom tool
    ImGui::PushID("tool_zoom");
    if (ImGui::Button("Z", ImVec2(buttonSize, buttonSize))) {
        Logger::instance().info("Zoom tool activated");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zoom");
    }
    ImGui::PopID();

    // Rotate tool
    ImGui::PushID("tool_rotate");
    if (ImGui::Button("R", ImVec2(buttonSize, buttonSize))) {
        Logger::instance().info("Rotate tool activated");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rotate");
    }
    ImGui::PopID();

    // Grid snap
    ImGui::PushID("tool_grid");
    if (ImGui::Button("G", ImVec2(buttonSize, buttonSize))) {
        Logger::instance().info("Grid snap toggled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Grid Snap");
    }
    ImGui::PopID();

    // Camera
    ImGui::PushID("tool_camera");
    if (ImGui::Button("C", ImVec2(buttonSize, buttonSize))) {
        Logger::instance().info("Camera tool activated");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Camera");
    }
    ImGui::PopID();

    // Material
    ImGui::PushID("tool_material");
    if (ImGui::Button("M", ImVec2(buttonSize, buttonSize))) {
        Logger::instance().info("Material editor");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Material");
    }
    ImGui::PopID();

    // Node Editor toggle
    ImGui::PushID("tool_nodes");
    if (ImGui::Button("N", ImVec2(buttonSize, buttonSize))) {
        Logger::instance().info("Node Editor toggle");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Node Editor");
    }
    ImGui::PopID();

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
}

// ================================================================
//  Status bar
// ================================================================
void Application::drawStatusBar() {
    ImGui::Separator();
    ImGui::BeginChild("StatusBar", ImVec2(0, 20), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    // Left: View information
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Eye: (2.35, 1.10, 1.51) | Target: (0.00, 0.00, 0.00) | Rendered by Native OpenGL: 58.9 FPS");
    ImGui::SameLine();

    // Center: Timeline (placeholder)
    ImGui::SameLine(ImGui::GetWindowWidth() * 0.4f);
    ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f), "Frame: 1 / 100");

    // Right: Node info
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Nodes: %d | Connections: %d",
        nodeEditor_->nodeCount(),
        nodeEditor_->connectionCount());

    ImGui::EndChild();

    // Status bar line
    ImGui::Separator();
    ImGui::BeginChild("StatusText", ImVec2(0, 16), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status Bar");
    ImGui::EndChild();
}

} // namespace TopOpt
