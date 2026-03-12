#include "LogPanel.h"
#include "../utils/Logger.h"
#include "imgui.h"

namespace TopOpt {

void LogPanel::draw() {
    auto& logger = Logger::instance();

    // Header bar
    if (ImGui::SmallButton("Clear")) {
        logger.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &logger.autoScroll);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f),
        "(%d entries)", (int)logger.entries().size());
    ImGui::Separator();

    // Log content
    ImGui::BeginChild("LogScroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    for (auto& entry : logger.entries()) {
        ImVec4 levelCol;
        const char* levelStr;
        switch (entry.level) {
            case LogLevel::Info:
                levelCol = ImVec4(0.0f, 0.83f, 0.67f, 1.0f);
                levelStr = "INFO ";
                break;
            case LogLevel::Warn:
                levelCol = ImVec4(0.89f, 0.72f, 0.24f, 1.0f);
                levelStr = "WARN ";
                break;
            case LogLevel::Error:
                levelCol = ImVec4(1.0f, 0.42f, 0.42f, 1.0f);
                levelStr = "ERROR";
                break;
        }
        ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f), "%s", entry.timestamp.c_str());
        ImGui::SameLine();
        ImGui::TextColored(levelCol, "%s", levelStr);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.82f, 1.0f), "%s", entry.message.c_str());
    }

    if (logger.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

} // namespace TopOpt
