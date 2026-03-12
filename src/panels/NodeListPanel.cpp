#include "NodeListPanel.h"
#include "../node_editor/NodeRegistry.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace TopOpt {

static bool containsCI(const std::string& hay, const char* needle) {
    if (!needle || needle[0] == '\0') return true;
    std::string h = hay, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

void NodeListPanel::draw(NodeEditor& editor) {

    float fontSize = ImGui::GetFontSize();

    // ---- Search bar ----
    ImGui::PushItemWidth(-1);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 5));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.15f, 0.17f, 1.0f));
    ImGui::InputTextWithHint("##search", "Search...", searchBuf_, sizeof(searchBuf_));
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::PopItemWidth();
    ImGui::Spacing();

    auto cats = NodeRegistry::instance().categorized();
    float cardH = fontSize * 2.6f;       // card height adapts to font
    float accentW = 4.0f;
    float cardRound = 5.0f;
    float cardGap = 3.0f;

    for (auto& [cat, typeNames] : cats) {
        // Count visible items
        int visible = 0;
        for (auto& tn : typeNames) {
            auto* def = NodeRegistry::instance().findType(tn);
            if (def && containsCI(def->displayName, searchBuf_)) visible++;
            else if (def && containsCI(def->category, searchBuf_)) visible++;
        }
        if (visible == 0) continue;

        // Category header
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.68f, 0.75f, 1.0f));
        bool open = ImGui::TreeNodeEx(cat.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.38f, 0.40f, 0.48f, 1.0f), "(%d)", visible);

        if (open) {
            for (auto& tn : typeNames) {
                auto* def = NodeRegistry::instance().findType(tn);
                if (!def) continue;
                if (!containsCI(def->displayName, searchBuf_) && !containsCI(def->category, searchBuf_))
                    continue;

                ImGui::PushID(tn.c_str());

                float availW = ImGui::GetContentRegionAvail().x;
                ImVec2 cardMin = ImGui::GetCursorScreenPos();
                ImVec2 cardMax = ImVec2(cardMin.x + availW, cardMin.y + cardH);

                // Full-width invisible button for click + drag
                if (ImGui::InvisibleButton("##card", ImVec2(availW, cardH))) {
                    editor.addNode(tn, 300 + (rand() % 200), 200 + (rand() % 200));
                }
                bool hovered = ImGui::IsItemHovered();
                bool active  = ImGui::IsItemActive();

                ImDrawList* dl = ImGui::GetWindowDrawList();

                // Card background with hover/active states
                ImU32 cardBg;
                if (active)
                    cardBg = IM_COL32(58, 60, 68, 255);
                else if (hovered)
                    cardBg = IM_COL32(48, 50, 56, 255);
                else
                    cardBg = IM_COL32(34, 36, 40, 255);
                dl->AddRectFilled(cardMin, cardMax, cardBg, cardRound);

                // Subtle border on hover
                if (hovered) {
                    ImVec4 hdrF = ImGui::ColorConvertU32ToFloat4(def->headerColor);
                    ImU32 borderCol = IM_COL32((int)(hdrF.x * 255 * 0.6f),
                                               (int)(hdrF.y * 255 * 0.6f),
                                               (int)(hdrF.z * 255 * 0.6f), 120);
                    dl->AddRect(cardMin, cardMax, borderCol, cardRound, 0, 1.2f);
                }

                // Colored left accent bar
                dl->AddRectFilled(cardMin, ImVec2(cardMin.x + accentW, cardMax.y),
                                  def->headerColor, cardRound, ImDrawFlags_RoundCornersLeft);

                // Layout: icon and name on first row, description on second
                float contentX = cardMin.x + accentW + 10;
                float row1Y = cardMin.y + (cardH * 0.5f - fontSize) * 0.5f + 1;
                float row2Y = row1Y + fontSize + 2;

                // Icon with accent color tint
                dl->AddText(ImVec2(contentX, row1Y), def->headerColor, def->icon.c_str());

                // Display name
                ImVec2 iconSize = ImGui::CalcTextSize(def->icon.c_str());
                float nameX = contentX + iconSize.x + 8;
                dl->AddText(ImVec2(nameX, row1Y), IM_COL32(225, 228, 234, 255),
                            def->displayName.c_str());

                // Description (truncated to card width)
                float descMaxW = cardMax.x - contentX - 6;
                const char* descStr = def->description.c_str();
                const char* descEnd = nullptr;
                // Find wrap point
                ImVec2 descSize = ImGui::CalcTextSize(descStr);
                if (descSize.x > descMaxW) {
                    // Truncate with "..."
                    std::string truncDesc = def->description;
                    while (truncDesc.size() > 3) {
                        truncDesc.pop_back();
                        ImVec2 ts = ImGui::CalcTextSize((truncDesc + "...").c_str());
                        if (ts.x <= descMaxW) {
                            truncDesc += "...";
                            dl->AddText(ImVec2(contentX, row2Y),
                                        IM_COL32(110, 112, 122, 255), truncDesc.c_str());
                            break;
                        }
                    }
                } else {
                    dl->AddText(ImVec2(contentX, row2Y),
                                IM_COL32(110, 112, 122, 255), descStr);
                }

                // Drag source on the whole card
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("NODE_TYPE", tn.c_str(), tn.length() + 1);
                    // Mini card preview in drag tooltip
                    ImVec4 hdrF = ImGui::ColorConvertU32ToFloat4(def->headerColor);
                    ImGui::ColorButton("##dragcol", hdrF, ImGuiColorEditFlags_NoTooltip, ImVec2(12, 12));
                    ImGui::SameLine();
                    ImGui::TextUnformatted(def->displayName.c_str());
                    ImGui::EndDragDropSource();
                }

                // Tooltip on hover
                if (hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.95f, 1.0f), "%s", def->displayName.c_str());
                    if (!def->description.empty()) {
                        ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.65f, 1.0f), "%s", def->description.c_str());
                    }
                    int ni = (int)def->inputs.size();
                    int no = (int)def->outputs.size();
                    if (ni > 0 || no > 0) {
                        ImGui::Separator();
                        if (ni > 0) ImGui::TextColored(ImVec4(0.50f, 0.70f, 0.85f, 1.0f), "Inputs: %d", ni);
                        if (no > 0) ImGui::TextColored(ImVec4(0.50f, 0.85f, 0.60f, 1.0f), "Outputs: %d", no);
                    }
                    ImGui::EndTooltip();
                }

                // Gap between cards
                ImGui::Dummy(ImVec2(0, cardGap));
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
}

} // namespace TopOpt
