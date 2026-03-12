#include "ModulePanel.h"
#include "imgui.h"

namespace TopOpt {

void ModulePanel::draw() {
    ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f), "Resource Browser");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.35f, 0.35f, 0.45f, 1.0f), "Project Files");
    ImGui::Separator();

    // Split into two columns: file tree and preview
    float treeWidth = 160.f;
    float avail = ImGui::GetContentRegionAvail().x;

    // File tree (left side)
    ImGui::BeginChild("FileTree", ImVec2(treeWidth, 0), ImGuiChildFlags_Border);
    {
        // Root folder
        if (ImGui::TreeNode("Project Root")) {
            if (ImGui::TreeNode("models")) {
                if (ImGui::Selectable("bunny.stl")) {
                    ImGui::SetTooltip("Click to load bunny.stl");
                }
                if (ImGui::Selectable("cube.obj")) {
                    ImGui::SetTooltip("Click to load cube.obj");
                }
                if (ImGui::Selectable("sphere.vdb")) {
                    ImGui::SetTooltip("Click to load sphere.vdb");
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("textures")) {
                if (ImGui::Selectable("diffuse.png")) {
                    ImGui::SetTooltip("diffuse.png");
                }
                if (ImGui::Selectable("normal.png")) {
                    ImGui::SetTooltip("normal.png");
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("scenes")) {
                if (ImGui::Selectable("test.scene")) {
                    ImGui::SetTooltip("test.scene");
                }
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Preview area (right side)
    ImGui::BeginChild("PreviewArea", ImVec2(0, 0), ImGuiChildFlags_Border);
    {
        float previewW = ImGui::GetContentRegionAvail().x;
        float previewH = ImGui::GetContentRegionAvail().y;

        // Draw placeholder preview
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();

        // Preview background
        ImU32 bgColor = IM_COL32(30, 30, 30, 255);
        drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bgColor);

        // Checkered pattern for transparency
        const float checkSize = 8.0f;
        for (float y = 0; y < size.y; y += checkSize) {
            for (float x = 0; x < size.x; x += checkSize) {
                bool isWhite = ((int)(x / checkSize) + (int)(y / checkSize)) % 2 == 0;
                if (isWhite) {
                    drawList->AddRectFilled(
                        ImVec2(pos.x + x, pos.y + y),
                        ImVec2(pos.x + x + checkSize, pos.y + y + checkSize),
                        IM_COL32(45, 45, 45, 255)
                    );
                }
            }
        }

        // Center placeholder icon
        ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
        float iconSize = 48.0f;

        // Draw file icon (simple rectangle with folded corner)
        ImU32 iconColor = IM_COL32(100, 100, 100, 255);
        ImVec2 iconTL = ImVec2(center.x - iconSize * 0.4f, center.y - iconSize * 0.5f);
        ImVec2 iconBR = ImVec2(center.x + iconSize * 0.5f, center.y + iconSize * 0.5f);
        drawList->AddRectFilled(iconTL, iconBR, iconColor, 2.0f);

        // Folded corner
        drawList->AddQuadFilled(
            ImVec2(iconBR.x - 12, iconBR.y),
            ImVec2(iconBR.x, iconBR.y),
            ImVec2(iconBR.x, iconBR.y - 12),
            ImVec2(iconBR.x - 12, iconBR.y - 12),
            IM_COL32(70, 70, 70, 255)
        );

        // File info text
        const char* infoText = "Select a file to preview";
        ImVec2 textSize = ImGui::CalcTextSize(infoText);
        ImVec2 textPos = ImVec2(center.x - textSize.x * 0.5f, center.y + iconSize * 0.6f);
        drawList->AddText(textPos, IM_COL32(150, 150, 150, 255), infoText);

        // File details
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + size.y - 60);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "File Details");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Name: -");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Size: -");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Type: -");
    }
    ImGui::EndChild();
}

} // namespace TopOpt
