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
#include <array>
#include <cstdlib>
#include <initializer_list>
#include <filesystem>
#include <functional>

namespace TopOpt {

namespace {

struct ScopedFont {
    explicit ScopedFont(ImFont* font) : active_(font != nullptr) {
        if (active_) {
            ImGui::PushFont(font);
        }
    }

    ~ScopedFont() {
        if (active_) {
            ImGui::PopFont();
        }
    }

private:
    bool active_ = false;
};

bool fileExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::string windowsFontPath(const char* fileName) {
    const char* windir = std::getenv("WINDIR");
    const std::string baseDir = (windir && *windir) ? windir : "C:\\Windows";
    return baseDir + "\\Fonts\\" + fileName;
}

ImVec4 workflowStatusColor(WorkflowStepStatus status) {
    switch (status) {
    case WorkflowStepStatus::NotStarted:
        return ImVec4(0.42f, 0.45f, 0.50f, 1.0f);
    case WorkflowStepStatus::Pending:
        return ImVec4(0.84f, 0.70f, 0.28f, 1.0f);
    case WorkflowStepStatus::Completed:
        return ImVec4(0.34f, 0.74f, 0.48f, 1.0f);
    case WorkflowStepStatus::ConfigurationError:
        return ImVec4(0.88f, 0.38f, 0.38f, 1.0f);
    }

    return ImVec4(0.65f, 0.68f, 0.74f, 1.0f);
}

const char* workflowStatusTextZh(WorkflowStepStatus status) {
    switch (status) {
    case WorkflowStepStatus::NotStarted:
        return u8"\u672a\u5f00\u59cb";
    case WorkflowStepStatus::Pending:
        return u8"\u5f85\u5b8c\u6210";
    case WorkflowStepStatus::Completed:
        return u8"\u5df2\u5b8c\u6210";
    case WorkflowStepStatus::ConfigurationError:
        return u8"\u914d\u7f6e\u5f02\u5e38";
    }

    return u8"\u672a\u77e5";
}

} // namespace

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

static std::string resolveProjectPath(const std::string& relativePath) {
    namespace fs = std::filesystem;

    if (relativePath.empty()) {
        return "";
    }

    std::error_code ec;
    fs::path current = fs::current_path(ec);
    if (ec) {
        return "";
    }

    for (int depth = 0; depth < 6; ++depth) {
        fs::path candidate = current / relativePath;
        if (fs::exists(candidate, ec)) {
            return candidate.lexically_normal().string();
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    return "";
}

// ================================================================
//  Application
// ================================================================

Application::Application() {
    g_appInstance = this;
    tutorialWorkflow_ = TutorialWorkflow::makeStandardTopologyWorkflow();
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

    loadFonts();

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
    buildTutorialCases();

    Logger::instance().info("TopOptFrame initialized");
    Logger::instance().info("Right-click canvas or use the Nodes panel to add nodes");

    running_ = true;
    updateWindowTitle();
    return true;
}

void Application::loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesChineseFull();
    const std::array<std::string, 4> fontCandidates = {
        windowsFontPath("msyh.ttc"),
        windowsFontPath("msyhbd.ttc"),
        windowsFontPath("simhei.ttf"),
        windowsFontPath("simsun.ttc")
    };

    io.Fonts->Clear();
    titleFont_ = nullptr;
    bodyFont_ = nullptr;
    smallFont_ = nullptr;

    for (const std::string& fontPath : fontCandidates) {
        if (!fileExists(fontPath)) {
            continue;
        }

        bodyFont_ = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 20.0f, nullptr, glyphRanges);
        titleFont_ = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 28.0f, nullptr, glyphRanges);
        smallFont_ = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f, nullptr, glyphRanges);
        if (bodyFont_ && titleFont_ && smallFont_) {
            io.FontDefault = bodyFont_;
            Logger::instance().info("Loaded UI fonts: " + fontPath);
            return;
        }

        io.Fonts->Clear();
        titleFont_ = nullptr;
        bodyFont_ = nullptr;
        smallFont_ = nullptr;
    }

    bodyFont_ = io.Fonts->AddFontDefault();
    titleFont_ = bodyFont_;
    smallFont_ = bodyFont_;
    io.FontDefault = bodyFont_;
    Logger::instance().warn("Falling back to the built-in ImGui font; Chinese glyph coverage may be incomplete");
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
        if (activeScreen_ == AppScreen::Workspace) {
            drawWorkspace();
        } else {
            drawTutorialHome();
        }

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

void Application::buildTutorialCases() {
    tutorialCases_.clear();
    tutorialCases_.push_back({
        u8"\u60ac\u81c2\u6881\u62d3\u6251\u4f18\u5316",
        u8"\u8fd0\u884c\u4e00\u4e2a\u5b8c\u6574\u7684\u3001\u652f\u6301 GPU \u7684\u62d3\u6251\u4f18\u5316\u6559\u5b66\u6848\u4f8b\u3002",
        u8"\u5165\u95e8",
        "examples/cantilever_gpu_test.topopt",
        u8"\u7406\u89e3\u6807\u51c6\u7684\u8bbe\u8ba1\u57df\u3001\u6750\u6599\u3001\u8f7d\u8377\u3001SIMP \u4f18\u5316\u548c\u5bc6\u5ea6\u7ed3\u679c\u8f93\u51fa\u6d41\u7a0b\u3002",
        u8"\u91cd\u70b9\u89c2\u5bdf\u5bc6\u5ea6\u6f14\u5316\u3001\u4f53\u79ef\u5206\u6570\u53d8\u5316\uff0c\u4ee5\u53ca\u6700\u7ec8\u7ed3\u6784\u7684\u4f20\u529b\u8def\u5f84\u3002",
        !resolveProjectPath("examples/cantilever_gpu_test.topopt").empty()
    });
    tutorialCases_.push_back({
        u8"MBB \u6881\u57fa\u51c6\u6848\u4f8b",
        u8"\u540e\u7eed\u8bfe\u7a0b\u4f1a\u52a0\u5165\u7684\u5bf9\u79f0\u6027\u57fa\u51c6\u6848\u4f8b\u3002",
        u8"\u8fdb\u9636",
        "",
        u8"\u5bf9\u6bd4\u7ecf\u5178\u57fa\u51c6\u95ee\u9898\u4e2d\u7684\u652f\u6491\u65b9\u5f0f\u4e0e\u5bf9\u79f0\u6027\u5bf9\u7ed3\u679c\u7684\u5f71\u54cd\u3002",
        u8"\u91cd\u70b9\u5173\u6ce8\u5bf9\u79f0\u8fb9\u754c\u3001\u652f\u6491\u4f4d\u7f6e\u548c\u62d3\u6251\u5206\u652f\u5f62\u6001\u3002",
        false
    });
    tutorialCases_.push_back({
        u8"\u6709\u9650\u5143\u57fa\u7840\u5206\u6790",
        u8"\u5728\u8fdb\u5165\u4f18\u5316\u4e4b\u524d\u5148\u5b66\u4e60\u4f4d\u79fb\u5206\u6790\u3002",
        u8"\u5165\u95e8",
        "",
        u8"\u5b66\u4e60\u652f\u6491\u3001\u8f7d\u8377\u548c\u6750\u6599\u53c2\u6570\u5982\u4f55\u5f71\u54cd\u7ed3\u6784\u4f4d\u79fb\u573a\u3002",
        u8"\u91cd\u70b9\u89c2\u5bdf\u53d8\u5f62\u5f62\u6001\uff0c\u5e76\u628a\u8f7d\u8377\u8bbe\u7f6e\u548c\u54cd\u5e94\u7ed3\u679c\u5bf9\u5e94\u8d77\u6765\u3002",
        false
    });
}

bool Application::loadProjectFromPath(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    ViewState view;
    if (!ProjectSerializer::loadFromFile(path, *nodeEditor_, view)) {
        Logger::instance().error("Failed to load project: " + path);
        return false;
    }

    horizontalSplitRatio_ = view.horizontalSplitRatio;
    leftVertSplitRatio_   = view.leftVertSplitRatio;
    centerVertSplitRatio_ = view.centerVertSplitRatio;
    rightPanelRatio_      = view.rightPanelRatio;
    view3D_->setCameraState(view.camDistance, view.camYaw, view.camPitch,
                            view.camCenter[0], view.camCenter[1], view.camCenter[2]);
    currentFilePath_ = path;
    activeScreen_ = AppScreen::Workspace;
    cmdHistory_->clear();
    cmdHistory_->markClean();
    tutorialWorkflow_.reset();
    prevSelectedNodeId_ = -1;
    prevParamHash_ = 0;
    densityPlayback_ = {};
    updateWindowTitle();
    return true;
}

bool Application::openTutorialCase(int caseIndex) {
    if (caseIndex < 0 || caseIndex >= static_cast<int>(tutorialCases_.size())) {
        return false;
    }

    const TutorialCase& tutorialCase = tutorialCases_[caseIndex];
    const std::string resolved = resolveProjectPath(tutorialCase.relativeProjectPath);
    if (resolved.empty()) {
        Logger::instance().warn(std::string(u8"\u8bfe\u7a0b\u6848\u4f8b\u6682\u672a\u5f00\u653e: ") + tutorialCase.title);
        return false;
    }

    if (!loadProjectFromPath(resolved)) {
        return false;
    }

    activeTutorialCaseIndex_ = caseIndex;
    Logger::instance().info(std::string(u8"\u5df2\u6253\u5f00\u8bfe\u7a0b\u6848\u4f8b: ") + tutorialCase.title);
    return true;
}

void Application::drawTutorialHome() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    ImGui::BeginChild("TutorialHome", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    dl->AddRectFilledMultiColor(
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(24, 28, 34, 255),
        IM_COL32(28, 38, 54, 255),
        IM_COL32(16, 18, 22, 255),
        IM_COL32(20, 24, 32, 255));

    {
        ScopedFont titleFont(titleFont_);
        ImGui::TextColored(ImVec4(0.55f, 0.78f, 0.98f, 1.0f), u8"\u62d3\u6251\u4f18\u5316\u8bfe\u7a0b\u5de5\u4f5c\u53f0");
    }
    ImGui::TextUnformatted(u8"\u4f60\u53ef\u4ee5\u5148\u4ece\u6559\u5b66\u6848\u4f8b\u5f00\u59cb\uff0c\u4e5f\u53ef\u4ee5\u76f4\u63a5\u8fdb\u5165\u5b8c\u6574\u5de5\u4f5c\u533a\u3002");
    ImGui::Spacing();
    {
        ScopedFont smallFont(smallFont_);
        ImGui::TextColored(
            ImVec4(0.72f, 0.75f, 0.82f, 1.0f),
            u8"\u5f53\u524d\u7248\u672c\u5df2\u7ecf\u53ef\u4ee5\u8fd0\u884c\u62d3\u6251\u4f18\u5316\u6848\u4f8b\u3002Phase 1 \u7684\u91cd\u70b9\u662f\u628a\u5165\u53e3\u6539\u9020\u6210\u9002\u5408\u6559\u5b66\u7684\u5f62\u5f0f\u3002");
    }
    ImGui::Spacing();

    if (ImGui::Button(u8"\u6253\u5f00\u5df2\u6709\u5de5\u7a0b", ImVec2(220, 0))) {
        activeTutorialCaseIndex_ = -1;
        openProject();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", u8"\u4ece\u78c1\u76d8\u6253\u5f00\u4efb\u610f .topopt \u5de5\u7a0b");
    ImGui::SameLine();
    if (ImGui::Button(u8"\u8fdb\u5165\u7a7a\u767d\u5de5\u4f5c\u533a", ImVec2(220, 0))) {
        newProject();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", u8"\u8df3\u8fc7\u6559\u5b66\u5165\u53e3\uff0c\u76f4\u63a5\u7f16\u8f91\u8282\u70b9\u56fe");

    ImGui::Spacing();
    ImGui::SeparatorText(u8"\u6559\u5b66\u6848\u4f8b");

    const float gap = 14.0f;
    const float availW = ImGui::GetContentRegionAvail().x;
    const float cardW = (availW - gap * 2.0f) / 3.0f;
    const float cardH = 280.0f;

    for (int i = 0; i < static_cast<int>(tutorialCases_.size()); ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, gap);
        }

        const TutorialCase& tutorialCase = tutorialCases_[i];
        ImGui::BeginChild(i + 1000, ImVec2(cardW, cardH), ImGuiChildFlags_Border);
        {
            ScopedFont smallFont(smallFont_);
            ImGui::TextColored(ImVec4(0.52f, 0.70f, 0.94f, 1.0f), "%s", tutorialCase.difficulty.c_str());
        }
        ImGui::Spacing();
        {
            ScopedFont titleFont(titleFont_);
            ImGui::TextWrapped("%s", tutorialCase.title.c_str());
        }
        ImGui::Spacing();
        {
            ScopedFont smallFont(smallFont_);
            ImGui::TextColored(ImVec4(0.67f, 0.70f, 0.76f, 1.0f), "%s", tutorialCase.subtitle.c_str());
        }
        ImGui::Spacing();
        ImGui::Separator();
        {
            ScopedFont smallFont(smallFont_);
            ImGui::TextColored(ImVec4(0.86f, 0.88f, 0.92f, 1.0f), u8"\u5b66\u4e60\u76ee\u6807");
        }
        ImGui::TextWrapped("%s", tutorialCase.objective.c_str());
        ImGui::Spacing();
        {
            ScopedFont smallFont(smallFont_);
            ImGui::TextColored(ImVec4(0.86f, 0.88f, 0.92f, 1.0f), u8"\u89c2\u5bdf\u91cd\u70b9");
        }
        ImGui::TextWrapped("%s", tutorialCase.observation.c_str());
        ImGui::Dummy(ImVec2(0, 8));

        if (!tutorialCase.available) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(tutorialCase.available ? u8"\u5f00\u59cb\u5b66\u4e60" : u8"\u51c6\u5907\u4e2d", ImVec2(-1, 0))) {
            openTutorialCase(i);
        }
        if (!tutorialCase.available) {
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
    }

    ImGui::Spacing();
    ImGui::SeparatorText(u8"\u5f53\u524d\u9636\u6bb5\u8bf4\u660e");
    {
        ScopedFont smallFont(smallFont_);
        ImGui::TextWrapped(
            "%s",
            u8"\u8fd9\u4e2a\u9996\u9875\u662f\u6559\u7a0b\u5316\u6539\u9020\u7684\u7b2c\u4e00\u5c42\uff1a\u8bfe\u7a0b\u5165\u53e3\u3001\u6848\u4f8b\u9009\u62e9\u548c\u8bfe\u7a0b\u8bf4\u660e\u4e0a\u4e0b\u6587\u3002\u540e\u7eed\u9636\u6bb5\u4f1a\u7ee7\u7eed\u8865\u5145\u6d41\u7a0b\u5f15\u5bfc\u548c\u53c2\u6570\u89e3\u91ca\u3002");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void Application::drawWorkspace() {
    updateTutorialWorkflowState();
    drawToolbar();

    float statusH  = 34.f;
    float contentH = ImGui::GetContentRegionAvail().y - statusH;
    float splitterThickness = 6.0f;

    ImGui::BeginChild("ContentArea", ImVec2(0, contentH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    {
        float totalW = ImGui::GetContentRegionAvail().x;
        float contentAvH = ImGui::GetContentRegionAvail().y;
        float usableW = totalW - splitterThickness * 2;

        float leftW   = usableW * horizontalSplitRatio_;
        float rightW  = usableW * rightPanelRatio_;
        float centerW = usableW - leftW - rightW;

        if (leftW   < 200.0f) leftW   = 200.0f;
        if (rightW  < 120.0f) rightW  = 120.0f;
        if (centerW < 200.0f) centerW = 200.0f;

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
                        if (activeTutorialCaseIndex_ >= 0 && ImGui::BeginTabItem("Lesson")) {
                            drawLessonTab();
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem(u8"\u6d41\u7a0b")) {
                            drawWorkflowTab();
                            ImGui::EndTabItem();
                        }
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
                            ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 0.7f), u8"\u63a7\u5236\u53f0\u8f93\u51fa");
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
                {
                    ScopedFont titleFont(titleFont_);
                    ImGui::TextColored(ImVec4(0.55f, 0.70f, 0.92f, 1.0f), "Properties");
                }
                ImGui::Separator();
                ImGui::BeginChild("PropertyContent", ImVec2(0, 0), ImGuiChildFlags_None);
                propPanel_->draw(*nodeEditor_);
                drawDensityPlaybackControls();
                updateDensityPlayback();
                ImGui::EndChild();
            }
            ImGui::EndChild();

            updateLivePreview();
        }
        ImGui::EndChild();

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
        ImGui::BeginChild("NodeLibrary", ImVec2(rightW, 0), ImGuiChildFlags_Border);
        {
            {
                ScopedFont titleFont(titleFont_);
                ImGui::TextColored(ImVec4(0.55f, 0.70f, 0.92f, 1.0f), "Node Library");
            }
            ImGui::Separator();
            ImGui::BeginChild("NodeLibContent", ImVec2(0, 0), ImGuiChildFlags_None);
            nodeList_->draw(*nodeEditor_);
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    drawStatusBar();
}

void Application::drawLessonTab() const {
    if (activeTutorialCaseIndex_ < 0 || activeTutorialCaseIndex_ >= static_cast<int>(tutorialCases_.size())) {
        ImGui::TextDisabled("%s", u8"\u6253\u5f00\u4e00\u4e2a\u6559\u5b66\u6848\u4f8b\u540e\uff0c\u8fd9\u91cc\u4f1a\u663e\u793a\u8bfe\u7a0b\u8bf4\u660e\u3002");
        return;
    }

    const TutorialCase& tutorialCase = tutorialCases_[activeTutorialCaseIndex_];
    {
        ScopedFont titleFont(titleFont_);
        ImGui::TextColored(ImVec4(0.55f, 0.70f, 0.92f, 1.0f), "%s", tutorialCase.title.c_str());
    }
    ImGui::SameLine();
    {
        ScopedFont smallFont(smallFont_);
        ImGui::TextColored(ImVec4(0.45f, 0.48f, 0.54f, 1.0f), "[%s]", tutorialCase.difficulty.c_str());
    }
    ImGui::Spacing();
    ImGui::TextWrapped("%s", tutorialCase.subtitle.c_str());
    ImGui::Separator();
    {
        ScopedFont smallFont(smallFont_);
        ImGui::TextColored(ImVec4(0.84f, 0.86f, 0.90f, 1.0f), u8"\u5b66\u4e60\u76ee\u6807");
    }
    ImGui::TextWrapped("%s", tutorialCase.objective.c_str());
    ImGui::Spacing();
    {
        ScopedFont smallFont(smallFont_);
        ImGui::TextColored(ImVec4(0.84f, 0.86f, 0.90f, 1.0f), u8"\u89c2\u5bdf\u91cd\u70b9");
    }
    ImGui::TextWrapped("%s", tutorialCase.observation.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    {
        ScopedFont smallFont(smallFont_);
        ImGui::TextColored(ImVec4(0.68f, 0.72f, 0.78f, 1.0f), u8"\u5efa\u8bae\u5b66\u4e60\u6b65\u9aa4");
    }
    ImGui::BulletText("%s", u8"\u5148\u89c2\u5bdf\u8bbe\u8ba1\u57df\u3001\u652f\u6491\u548c\u8f7d\u8377\u8bbe\u7f6e\u3002");
    ImGui::BulletText("%s", u8"\u8fd0\u884c\u8282\u70b9\u56fe\uff0c\u67e5\u770b\u5bc6\u5ea6\u52a8\u753b\u64ad\u653e\u8fc7\u7a0b\u3002");
    ImGui::BulletText("%s", u8"\u6bd4\u8f83\u4f53\u79ef\u5206\u6570\u3001\u76ee\u6807\u51fd\u6570\u548c\u6700\u7ec8\u62d3\u6251\u5f62\u6001\u3002");
}

void Application::drawWorkflowTab() {
    {
        ScopedFont titleFont(titleFont_);
        ImGui::TextColored(ImVec4(0.55f, 0.70f, 0.92f, 1.0f), u8"\u6559\u5b66\u6d41\u7a0b");
    }
    ImGui::SameLine();
    {
        ScopedFont smallFont(smallFont_);
        ImGui::TextColored(
            ImVec4(0.45f, 0.48f, 0.54f, 1.0f),
            "%d / %d",
            static_cast<int>(tutorialWorkflow_.completedRequiredStepCount()),
            static_cast<int>(tutorialWorkflow_.requiredStepCount()));
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "%s",
        u8"\u8fd9\u91cc\u4f1a\u6309\u7167\u6559\u5b66\u987a\u5e8f\u5c55\u793a\u5f53\u524d\u6848\u4f8b\u7684\u5b8c\u6210\u8def\u5f84\u3002\u7b2c\u4e00\u7248\u5148\u663e\u793a\u6b65\u9aa4\u9aa8\u67b6\u4e0e\u72b6\u6001\uff0c\u540e\u7eed\u4f1a\u518d\u63a5\u5165\u81ea\u52a8\u68c0\u67e5\u3002");

    ImGui::Spacing();
    ImGui::Separator();

    if (tutorialWorkflow_.hasBlockingIssues()) {
        ImGui::TextColored(
            ImVec4(0.88f, 0.38f, 0.38f, 1.0f),
            "%s",
            u8"\u5f53\u524d\u6d41\u7a0b\u4e2d\u5b58\u5728\u914d\u7f6e\u5f02\u5e38\u6b65\u9aa4\uff0c\u540e\u7eed\u9762\u677f\u4f1a\u5728\u8fd9\u91cc\u7ed9\u51fa\u5177\u4f53\u4fee\u590d\u63d0\u793a\u3002");
    } else {
        ImGui::TextColored(
            ImVec4(0.66f, 0.70f, 0.76f, 1.0f),
            "%s",
            u8"\u5f53\u524d\u9ed8\u8ba4\u72b6\u6001\u662f\uff1a\u7b2c 1 \u6b65\u4e3a\u201c\u5f85\u5b8c\u6210\u201d\uff0c\u540e\u7eed\u6b65\u9aa4\u968f\u6d41\u7a0b\u9010\u6b65\u63a8\u8fdb\u3002");
    }

    ImGui::Spacing();

    for (std::size_t i = 0; i < tutorialWorkflow_.stepCount(); ++i) {
        const WorkflowStep* step = tutorialWorkflow_.stepAt(i);
        if (!step) {
            continue;
        }

        ImGui::PushID(static_cast<int>(i));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.16f, 1.0f));
        ImGui::BeginChild("WorkflowStepCard", ImVec2(0, 126), ImGuiChildFlags_Border);
        ImGui::PopStyleColor();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cardMin = ImGui::GetWindowPos();
        ImVec2 cardMax = ImGui::GetWindowPos() + ImGui::GetWindowSize();
        dl->AddRectFilled(
            cardMin,
            ImVec2(cardMin.x + 5.0f, cardMax.y),
            ImGui::ColorConvertFloat4ToU32(workflowStatusColor(step->status)));

        {
            ScopedFont smallFont(smallFont_);
            ImGui::TextColored(
                ImVec4(0.52f, 0.70f, 0.94f, 1.0f),
                "%s %d",
                u8"\u6b65\u9aa4",
                static_cast<int>(i + 1));
        }
        ImGui::SameLine();
        ImGui::TextColored(workflowStatusColor(step->status), "[%s]", workflowStatusTextZh(step->status));

        {
            ScopedFont titleFont(titleFont_);
            ImGui::TextWrapped("%s", step->title.c_str());
        }

        {
            ScopedFont smallFont(smallFont_);
            ImGui::TextWrapped("%s", step->description.c_str());
        }

        if (!step->requiredNodeTypes.empty()) {
            ImGui::Spacing();
            {
                ScopedFont smallFont(smallFont_);
                ImGui::TextColored(ImVec4(0.72f, 0.75f, 0.82f, 1.0f), "%s", u8"\u5173\u952e\u8282\u70b9");
            }

            for (std::size_t nodeIndex = 0; nodeIndex < step->requiredNodeTypes.size(); ++nodeIndex) {
                if (nodeIndex > 0) {
                    ImGui::SameLine(0.0f, 6.0f);
                }
                ImGui::TextColored(
                    ImVec4(0.60f, 0.64f, 0.70f, 1.0f),
                    "%s",
                    step->requiredNodeTypes[nodeIndex].c_str());
            }
        }

        if (!step->issues.empty()) {
            ImGui::Spacing();
            {
                ScopedFont smallFont(smallFont_);
                ImGui::TextColored(ImVec4(0.88f, 0.38f, 0.38f, 1.0f), "%s", u8"\u5f02\u5e38");
            }
            for (const WorkflowIssue& issue : step->issues) {
                ImGui::BulletText("%s", issue.message.c_str());
            }
        }

        ImGui::EndChild();
        ImGui::PopID();
        ImGui::Dummy(ImVec2(0, 6));
    }
}

void Application::updateTutorialWorkflowState() {
    tutorialWorkflow_.reset();
    if (!nodeEditor_) {
        return;
    }

    const auto& nodes = nodeEditor_->nodes();
    const auto& connections = nodeEditor_->connections();

    auto typeMatches = [](const std::string& actual, std::initializer_list<const char*> typeNames) {
        for (const char* typeName : typeNames) {
            if (actual == typeName) {
                return true;
            }
        }
        return false;
    };

    auto findNodeById = [&](int nodeId) -> const NodeInstance* {
        for (const NodeInstance& node : nodes) {
            if (node.id == nodeId) {
                return &node;
            }
        }
        return nullptr;
    };

    auto findNodesByTypes = [&](std::initializer_list<const char*> typeNames) {
        std::vector<const NodeInstance*> matched;
        for (const NodeInstance& node : nodes) {
            if (typeMatches(node.typeName, typeNames)) {
                matched.push_back(&node);
            }
        }
        return matched;
    };

    auto inputPortIndex = [&](const NodeInstance& node, const char* inputId) {
        const NodeTypeDef* def = NodeRegistry::instance().findType(node.typeName);
        if (!def) {
            return -1;
        }

        for (std::size_t i = 0; i < def->inputs.size(); ++i) {
            if (def->inputs[i].id == inputId) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };

    auto hasConnectedInput = [&](const NodeInstance& node, const char* inputId) {
        const int inputIdx = inputPortIndex(node, inputId);
        if (inputIdx < 0) {
            return false;
        }

        for (const Connection& conn : connections) {
            if (conn.endNodeId == node.id && conn.endPortIdx == inputIdx) {
                return true;
            }
        }
        return false;
    };

    auto hasConnectedInputFromTypes = [&](const NodeInstance& node, const char* inputId,
                                          std::initializer_list<const char*> sourceTypes) {
        const int inputIdx = inputPortIndex(node, inputId);
        if (inputIdx < 0) {
            return false;
        }

        for (const Connection& conn : connections) {
            if (conn.endNodeId != node.id || conn.endPortIdx != inputIdx) {
                continue;
            }

            const NodeInstance* srcNode = findNodeById(conn.startNodeId);
            if (srcNode && typeMatches(srcNode->typeName, sourceTypes)) {
                return true;
            }
        }
        return false;
    };

    auto hasOutgoingConnectionToTypes = [&](const NodeInstance& node,
                                            std::initializer_list<const char*> targetTypes) {
        for (const Connection& conn : connections) {
            if (conn.startNodeId != node.id) {
                continue;
            }

            const NodeInstance* dstNode = findNodeById(conn.endNodeId);
            if (dstNode && typeMatches(dstNode->typeName, targetTypes)) {
                return true;
            }
        }
        return false;
    };

    auto anyNodeSatisfies = [&](const std::vector<const NodeInstance*>& matchedNodes, auto&& predicate) {
        for (const NodeInstance* node : matchedNodes) {
            if (node && predicate(*node)) {
                return true;
            }
        }
        return false;
    };

    const auto domainNodes = findNodesByTypes({"domain-box", "domain-lshape", "domain-from-mesh", "domain-import"});
    const auto materialNodes = findNodesByTypes({"fea-material"});
    const auto supportNodes = findNodesByTypes({"fea-fixed-support", "fea-displacement-bc"});
    const auto loadNodes = findNodesByTypes({"fea-point-force", "fea-pressure-load", "fea-body-force"});
    const auto loadCaseNodes = findNodesByTypes({"fea-load-case"});
    const auto solverNodes = findNodesByTypes({"fea-solver", "topo-simp", "topo-beso"});
    const auto optimizerNodes = findNodesByTypes({"topo-simp", "topo-beso"});
    const auto resultNodes = findNodesByTypes({
        "output-display", "output-viewer", "output-export",
        "post-density-view", "post-extract-field", "post-convergence", "post-export"
    });

    const bool hasDomain = !domainNodes.empty();
    const bool hasMaterial = !materialNodes.empty();
    const bool hasSupport = !supportNodes.empty();
    const bool hasLoad = !loadNodes.empty();
    const bool hasLoadCase = !loadCaseNodes.empty();
    const bool hasSolver = !solverNodes.empty();
    const bool hasOptimizer = !optimizerNodes.empty();
    const bool hasResultsNode = !resultNodes.empty();

    const bool validDomainChain = anyNodeSatisfies(
        domainNodes,
        [&](const NodeInstance& node) {
            return hasOutgoingConnectionToTypes(
                node,
                {"fea-fixed-support", "fea-displacement-bc", "fea-point-force", "fea-pressure-load",
                 "fea-body-force", "fea-solver", "topo-simp", "topo-beso", "topo-passive-region",
                 "post-density-view", "post-export"});
        });

    if (validDomainChain) {
        tutorialWorkflow_.setStepStatus(WorkflowStepId::GeometryDomain, WorkflowStepStatus::Completed);
    } else if (hasDomain) {
        tutorialWorkflow_.setStepIssues(
            WorkflowStepId::GeometryDomain,
            {{"unused-domain", u8"\u5df2\u6709\u8bbe\u8ba1\u57df\u8282\u70b9\uff0c\u4f46\u8fd8\u6ca1\u6709\u63a5\u5165\u652f\u6491\u3001\u8f7d\u8377\u6216\u6c42\u89e3\u94fe\u8def"}});
    }

    const bool validMaterialChain = anyNodeSatisfies(
        materialNodes,
        [&](const NodeInstance& node) {
            return hasOutgoingConnectionToTypes(node, {"fea-solver", "topo-simp", "topo-beso"});
        });

    if (validMaterialChain) {
        tutorialWorkflow_.setStepStatus(WorkflowStepId::Material, WorkflowStepStatus::Completed);
    } else if (hasMaterial) {
        tutorialWorkflow_.setStepIssues(
            WorkflowStepId::Material,
            {{"unused-material", u8"\u5df2\u6709 `fea-material` \u8282\u70b9\uff0c\u4f46\u8fd8\u6ca1\u6709\u63a5\u5165\u6c42\u89e3\u5668\u6216\u4f18\u5316\u5668"}});
    }

    const bool validSupportChain = anyNodeSatisfies(
        supportNodes,
        [&](const NodeInstance& node) {
            return hasConnectedInputFromTypes(node, "femesh", {"domain-box", "domain-lshape", "domain-from-mesh", "domain-import"}) &&
                   hasOutgoingConnectionToTypes(node, {"fea-load-case"});
        });
    const bool validLoadChain = anyNodeSatisfies(
        loadNodes,
        [&](const NodeInstance& node) {
            return hasConnectedInputFromTypes(node, "femesh", {"domain-box", "domain-lshape", "domain-from-mesh", "domain-import"}) &&
                   hasOutgoingConnectionToTypes(node, {"fea-load-case"});
        });
    const bool validLoadCaseChain = anyNodeSatisfies(
        loadCaseNodes,
        [&](const NodeInstance& node) {
            const bool hasSupportInput =
                hasConnectedInputFromTypes(node, "bc0", {"fea-fixed-support", "fea-displacement-bc"}) ||
                hasConnectedInputFromTypes(node, "bc1", {"fea-fixed-support", "fea-displacement-bc"}) ||
                hasConnectedInputFromTypes(node, "bc2", {"fea-fixed-support", "fea-displacement-bc"}) ||
                hasConnectedInputFromTypes(node, "bc3", {"fea-fixed-support", "fea-displacement-bc"});
            const bool hasLoadInput =
                hasConnectedInputFromTypes(node, "bc0", {"fea-point-force", "fea-pressure-load", "fea-body-force"}) ||
                hasConnectedInputFromTypes(node, "bc1", {"fea-point-force", "fea-pressure-load", "fea-body-force"}) ||
                hasConnectedInputFromTypes(node, "bc2", {"fea-point-force", "fea-pressure-load", "fea-body-force"}) ||
                hasConnectedInputFromTypes(node, "bc3", {"fea-point-force", "fea-pressure-load", "fea-body-force"});
            return hasSupportInput && hasLoadInput;
        });

    if (validSupportChain && validLoadChain && validLoadCaseChain) {
        tutorialWorkflow_.setStepStatus(WorkflowStepId::BoundaryConditions, WorkflowStepStatus::Completed);
    } else if (hasSupport || hasLoad || hasLoadCase) {
        std::vector<WorkflowIssue> issues;
        if (!hasSupport) {
            issues.push_back({"missing-support", u8"\u7f3a\u5c11\u652f\u6491\u6761\u4ef6\u8282\u70b9"});
        } else if (!validSupportChain) {
            issues.push_back({"unconnected-support", u8"\u652f\u6491\u8282\u70b9\u9700\u8981\u540c\u65f6\u63a5\u4e0a `FEMesh` \u548c `fea-load-case`"});
        }
        if (!hasLoad) {
            issues.push_back({"missing-load", u8"\u7f3a\u5c11\u8f7d\u8377\u8282\u70b9"});
        } else if (!validLoadChain) {
            issues.push_back({"unconnected-load", u8"\u8f7d\u8377\u8282\u70b9\u9700\u8981\u540c\u65f6\u63a5\u4e0a `FEMesh` \u548c `fea-load-case`"});
        }
        if (!hasLoadCase) {
            issues.push_back({"missing-loadcase", u8"\u7f3a\u5c11 `fea-load-case` \u8282\u70b9"});
        } else if (!validLoadCaseChain) {
            issues.push_back({"empty-loadcase", u8"`fea-load-case` \u9700\u8981\u540c\u65f6\u63a5\u5165\u652f\u6491\u548c\u8f7d\u8377"});
        }
        tutorialWorkflow_.setStepIssues(WorkflowStepId::BoundaryConditions, std::move(issues));
    }

    const bool validSolverChain = anyNodeSatisfies(
        solverNodes,
        [&](const NodeInstance& node) {
            const bool hasFemeshInput = hasConnectedInputFromTypes(
                node, "femesh", {"domain-box", "domain-lshape", "domain-from-mesh", "domain-import", "topo-passive-region"});
            const bool hasMaterialInput = hasConnectedInputFromTypes(node, "material", {"fea-material"});
            const bool hasLoadCaseInput =
                hasConnectedInputFromTypes(node, "lc0", {"fea-load-case"}) ||
                hasConnectedInputFromTypes(node, "lc1", {"fea-load-case"}) ||
                hasConnectedInputFromTypes(node, "lc2", {"fea-load-case"});
            return hasFemeshInput && hasMaterialInput && hasLoadCaseInput;
        });

    if (validSolverChain) {
        tutorialWorkflow_.setStepStatus(WorkflowStepId::Solver, WorkflowStepStatus::Completed);
    } else if (hasSolver) {
        std::vector<WorkflowIssue> issues;
        issues.push_back({"missing-femesh", u8"\u6c42\u89e3\u5668\u6216\u4f18\u5316\u5668\u672a\u63a5\u5165 `FEMesh`"});
        issues.push_back({"missing-material-link", u8"\u6c42\u89e3\u5668\u6216\u4f18\u5316\u5668\u672a\u63a5\u5165 `fea-material`"});
        issues.push_back({"missing-loadcase-link", u8"\u6c42\u89e3\u5668\u6216\u4f18\u5316\u5668\u672a\u63a5\u5165 `fea-load-case`"});
        tutorialWorkflow_.setStepIssues(WorkflowStepId::Solver, std::move(issues));
    }

    if (hasOptimizer) {
        std::vector<WorkflowIssue> issues;
        for (NodeInstance& node : nodeEditor_->nodes()) {
            if (node.typeName != "topo-simp" && node.typeName != "topo-beso") {
                continue;
            }

            if (!hasConnectedInput(node, "femesh")) {
                issues.push_back({"missing-topo-femesh", u8"\u4f18\u5316\u5668\u672a\u63a5\u5165 `FEMesh`"});
            }
            if (!hasConnectedInput(node, "material")) {
                issues.push_back({"missing-topo-material", u8"\u4f18\u5316\u5668\u672a\u63a5\u5165 `fea-material`"});
            }
            if (!hasConnectedInput(node, "lc0") &&
                !hasConnectedInput(node, "lc1") &&
                !hasConnectedInput(node, "lc2")) {
                issues.push_back({"missing-topo-loadcase", u8"\u4f18\u5316\u5668\u672a\u63a5\u5165 `fea-load-case`"});
            }

            const ParamDef* pVolFrac = findNodeParam(node, "VolFrac");
            const ParamDef* pPenalty = findNodeParam(node, "Penalty");
            const ParamDef* pFilterR = findNodeParam(node, "FilterR");

            if (pVolFrac && (pVolFrac->floatVal <= 0.0f || pVolFrac->floatVal >= 1.0f)) {
                issues.push_back({"invalid-volfrac", u8"`VolFrac` \u5e94\u5728 0 \u4e0e 1 \u4e4b\u95f4"});
            }
            if (pPenalty && pPenalty->floatVal < 1.0f) {
                issues.push_back({"invalid-penalty", u8"`Penalty` \u4e0d\u5e94\u5c0f\u4e8e 1"});
            }
            if (pFilterR && pFilterR->floatVal <= 0.0f) {
                issues.push_back({"invalid-filterr", u8"`FilterR` \u5fc5\u987b\u5927\u4e8e 0"});
            }
        }

        if (issues.empty()) {
            tutorialWorkflow_.setStepStatus(WorkflowStepId::OptimizationParameters, WorkflowStepStatus::Completed);
        } else {
            tutorialWorkflow_.setStepIssues(WorkflowStepId::OptimizationParameters, std::move(issues));
        }
    }

    const bool validResultsChain = anyNodeSatisfies(
        resultNodes,
        [&](const NodeInstance& node) {
            if (node.typeName == "post-density-view") {
                return hasConnectedInput(node, "density") && hasConnectedInput(node, "femesh");
            }
            if (node.typeName == "post-extract-field") {
                return hasConnectedInput(node, "result");
            }
            if (node.typeName == "post-convergence") {
                return hasConnectedInput(node, "density");
            }
            if (node.typeName == "post-export") {
                return hasConnectedInput(node, "femesh") ||
                       hasConnectedInput(node, "density") ||
                       hasConnectedInput(node, "result");
            }
            if (node.typeName == "output-viewer") {
                return hasConnectedInput(node, "mesh") || hasConnectedInput(node, "field");
            }
            if (node.typeName == "output-display" || node.typeName == "output-export") {
                return hasConnectedInput(node, "data");
            }
            return false;
        });

    if (validResultsChain) {
        tutorialWorkflow_.setStepStatus(WorkflowStepId::Results, WorkflowStepStatus::Completed);
    } else if (hasResultsNode) {
        tutorialWorkflow_.setStepIssues(
            WorkflowStepId::Results,
            {{"unconnected-results", u8"\u7ed3\u679c\u8282\u70b9\u5df2\u5b58\u5728\uff0c\u4f46\u8fd8\u6ca1\u6709\u63a5\u4e0a\u5bc6\u5ea6\u3001\u7ed3\u679c\u6216\u7f51\u683c\u8f93\u51fa"}});
    } else if (validSolverChain || hasOptimizer) {
        tutorialWorkflow_.setStepIssues(
            WorkflowStepId::Results,
            {{"missing-results-view", u8"\u5df2\u6709\u6c42\u89e3\u94fe\u8def\uff0c\u4f46\u7f3a\u5c11\u7ed3\u679c\u5c55\u793a\u6216\u5bfc\u51fa\u8282\u70b9"}});
    }

    tutorialWorkflow_.refreshProgression();
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
    activeScreen_ = AppScreen::Workspace;
    activeTutorialCaseIndex_ = -1;
    currentFilePath_.clear();
    tutorialWorkflow_.reset();
    prevSelectedNodeId_ = -1;
    prevParamHash_ = 0;
    densityPlayback_ = {};
    if (view3D_) view3D_->clearModel();
    cmdHistory_->markClean();
    updateWindowTitle();
    Logger::instance().info("New project created");
}

void Application::openProject() {
    std::string path = FileDialog::openFile(window_);
    if (path.empty()) return;

    activeTutorialCaseIndex_ = -1;
    loadProjectFromPath(path);
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
        {
            ScopedFont titleFont(titleFont_);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.65f, 0.95f, 1.0f));
            ImGui::TextUnformatted("TopOpt");
            ImGui::PopStyleColor();
        }
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

        if (ImGui::BeginMenu("Course")) {
            if (ImGui::MenuItem(u8"\u6559\u7a0b\u9996\u9875")) {
                activeScreen_ = AppScreen::TutorialHome;
            }
            if (activeTutorialCaseIndex_ >= 0 &&
                ImGui::MenuItem(u8"\u91cd\u65b0\u52a0\u8f7d\u5f53\u524d\u8bfe\u7a0b\u6848\u4f8b")) {
                openTutorialCase(activeTutorialCaseIndex_);
            }
            ImGui::EndMenu();
        }

        if (activeScreen_ == AppScreen::Workspace && ImGui::BeginMenu("Edit")) {
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

        if (activeScreen_ == AppScreen::Workspace && ImGui::BeginMenu("View")) {
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
        {
            ScopedFont smallFont(smallFont_);
            if (isExecuting_) {
                ImGui::TextColored(ImVec4(0.45f, 0.75f, 0.50f, 1.0f), "Running");
            } else {
                ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.58f, 1.0f), "Ready");
            }
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
    if (ImGui::Button(u8"\u9996\u9875")) { activeScreen_ = AppScreen::TutorialHome; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", u8"\u8fd4\u56de\u8bfe\u7a0b\u9996\u9875");
    ImGui::SameLine();
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
            prevSelectedNodeId_ = -1;
            prevParamHash_ = 0;
            densityPlayback_ = {};
        }
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(isExecuting_ ? "Stop Execution" : "Run Node Graph");
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        if (executor_) executor_->stepOne();
        prevSelectedNodeId_ = -1;
        prevParamHash_ = 0;
        densityPlayback_ = {};
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Execute One Step");
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        isExecuting_ = false;
        if (executor_) executor_->reset();
        if (view3D_) view3D_->clearModel();
        prevSelectedNodeId_ = -1;
        prevParamHash_ = 0;
        densityPlayback_ = {};
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
    {
        ScopedFont smallFont(smallFont_);

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
    }

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

ParamDef* Application::findNodeParam(NodeInstance& node, const std::string& name) const {
    for (auto& param : node.params) {
        if (param.name == name) {
            return &param;
        }
    }
    return nullptr;
}

const ParamDef* Application::findNodeParam(const NodeInstance& node, const std::string& name) const {
    for (const auto& param : node.params) {
        if (param.name == name) {
            return &param;
        }
    }
    return nullptr;
}

void Application::drawDensityPlaybackControls() {
    NodeInstance* selected = nodeEditor_ ? nodeEditor_->selectedNode() : nullptr;
    if (!selected || selected->typeName != "post-density-view") {
        return;
    }

    ParamDef* pAnimate = findNodeParam(*selected, "Animate");
    ParamDef* pLoop = findNodeParam(*selected, "Loop");
    ParamDef* pFrame = findNodeParam(*selected, "Frame");
    if (!pFrame) {
        return;
    }

    const DensityFieldData* density =
        executor_ ? executor_->cachedDensityFieldForNode(selected->id) : nullptr;
    const int frameCount = density
        ? (!density->densityFrames.empty()
            ? static_cast<int>(density->densityFrames.size())
            : (density->densities.empty() ? 0 : 1))
        : 0;
    const int maxFrame = std::max(0, frameCount - 1);
    pFrame->intVal = std::clamp(pFrame->intVal, 0, maxFrame);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.55f, 0.70f, 0.92f, 1.0f), "Playback");

    if (frameCount <= 0) {
        densityPlayback_.playing = false;
        ImGui::TextDisabled("Run the graph to generate density animation frames.");
        return;
    }

    ImGui::TextDisabled("Frame %d / %d", pFrame->intVal + 1, frameCount);

    const bool animateEnabled = pAnimate && pAnimate->boolVal;
    if (!animateEnabled) {
        ImGui::TextDisabled("Enable 'Animate' above to auto-play; the timeline can still scrub frames.");
    }

    if (!animateEnabled) ImGui::BeginDisabled();
    const char* playLabel = densityPlayback_.playing ? "Pause" : "Play";
    if (ImGui::Button(playLabel)) {
        if (!densityPlayback_.playing && pFrame->intVal >= maxFrame && !(pLoop && pLoop->boolVal)) {
            pFrame->intVal = 0;
        }
        densityPlayback_.nodeId = selected->id;
        densityPlayback_.playing = !densityPlayback_.playing;
        densityPlayback_.accumulator = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        densityPlayback_.nodeId = selected->id;
        densityPlayback_.playing = false;
        densityPlayback_.accumulator = 0.0f;
        pFrame->intVal = 0;
    }
    if (!animateEnabled) ImGui::EndDisabled();

    int frameValue = pFrame->intVal;
    if (ImGui::SliderInt("Timeline", &frameValue, 0, maxFrame)) {
        pFrame->intVal = frameValue;
        densityPlayback_.nodeId = selected->id;
        densityPlayback_.accumulator = 0.0f;
    }

    if (density && pFrame->intVal >= 0 &&
        pFrame->intVal < static_cast<int>(density->frameInfo.size())) {
        const DensityFrameInfo& info = density->frameInfo[pFrame->intVal];
        ImGui::Text("Iteration: %d", info.iteration);
        ImGui::Text("Objective: %.6g", info.objective);
        ImGui::Text("VolFrac: %.4f", info.volFrac);
        ImGui::Text("MaxChange: %.6g", info.maxChange);
    } else if (density) {
        ImGui::Text("Iteration: %d", pFrame->intVal + 1);
        ImGui::Text("Objective: %.6g", density->objective);
        ImGui::Text("VolFrac: %.4f", density->volFrac);
    }
}

void Application::updateDensityPlayback() {
    NodeInstance* selected = nodeEditor_ ? nodeEditor_->selectedNode() : nullptr;
    if (!selected || selected->typeName != "post-density-view") {
        densityPlayback_ = {};
        return;
    }

    ParamDef* pAnimate = findNodeParam(*selected, "Animate");
    ParamDef* pAutoPlay = findNodeParam(*selected, "AutoPlay");
    ParamDef* pLoop = findNodeParam(*selected, "Loop");
    ParamDef* pFps = findNodeParam(*selected, "FPS");
    ParamDef* pFrame = findNodeParam(*selected, "Frame");
    if (!pFrame) {
        densityPlayback_ = {};
        return;
    }

    const int frameCount = executor_ ? executor_->cachedDensityFrameCountForNode(selected->id) : 0;
    const int maxFrame = std::max(0, frameCount - 1);
    pFrame->intVal = std::clamp(pFrame->intVal, 0, maxFrame);

    if (densityPlayback_.nodeId != selected->id) {
        densityPlayback_.nodeId = selected->id;
        densityPlayback_.accumulator = 0.0f;
        densityPlayback_.playing =
            frameCount > 1 &&
            pAnimate && pAnimate->boolVal &&
            pAutoPlay && pAutoPlay->boolVal;
    }

    if (frameCount <= 1 || !(pAnimate && pAnimate->boolVal)) {
        densityPlayback_.playing = false;
        densityPlayback_.accumulator = 0.0f;
        return;
    }

    if (!densityPlayback_.playing) {
        return;
    }

    const float fps = pFps ? std::max(pFps->floatVal, 1.0f) : 6.0f;
    const float frameDuration = 1.0f / fps;
    densityPlayback_.accumulator += ImGui::GetIO().DeltaTime;

    while (densityPlayback_.accumulator >= frameDuration) {
        densityPlayback_.accumulator -= frameDuration;

        int nextFrame = pFrame->intVal + 1;
        if (nextFrame > maxFrame) {
            if (pLoop && pLoop->boolVal) {
                nextFrame = 0;
            } else {
                nextFrame = maxFrame;
                densityPlayback_.playing = false;
                densityPlayback_.accumulator = 0.0f;
            }
        }

        if (nextFrame == pFrame->intVal) {
            break;
        }

        pFrame->intVal = nextFrame;
    }
}

void Application::updateLivePreview() {
    int selId = nodeEditor_->selectedNodeId();
    if (selId < 0) {
        prevSelectedNodeId_ = -1;
        densityPlayback_ = {};
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
            if (t == "post-density-view") {
                executor_->previewDensityViewFromCache(selId);
                return;
            }
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
