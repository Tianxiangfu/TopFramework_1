#include "PropertyPanel.h"
#include "TeachingContent.h"
#include "../node_editor/NodeRegistry.h"
#include "../node_editor/Node.h"
#include "../commands/Command.h"
#include "../commands/NodeCommands.h"
#include "imgui.h"

#include <cstring>
#include <string>

namespace TopOpt {

namespace {

void drawTeachingNodeCard(const TeachingNodeInfo& info) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.13f, 0.16f, 1.0f));
    ImGui::BeginChild("TeachingNodeCard", ImVec2(0, 168), ImGuiChildFlags_Border);
    ImGui::PopStyleColor();

    ImGui::TextColored(ImVec4(0.55f, 0.70f, 0.92f, 1.0f), "%s", info.title.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("%s", info.summary.c_str());
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.84f, 0.86f, 0.90f, 1.0f), "%s", u8"\u8bfe\u7a0b\u4f5c\u7528");
    ImGui::TextWrapped("%s", info.lessonRole.c_str());
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.84f, 0.86f, 0.90f, 1.0f), "%s", u8"\u89c2\u5bdf\u70b9");
    ImGui::TextWrapped("%s", info.experimentHint.c_str());

    ImGui::EndChild();
}

void drawTeachingParamNote(const TeachingParamInfo& info) {
    if (ImGui::TreeNodeEx(u8"\u6559\u5b66\u8bf4\u660e##ParamNote", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(8.0f);

        ImGui::TextColored(ImVec4(0.72f, 0.75f, 0.82f, 1.0f), "%s", u8"\u542b\u4e49");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", info.meaning.c_str());

        ImGui::TextColored(ImVec4(0.72f, 0.75f, 0.82f, 1.0f), "%s", u8"\u63a8\u8350");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", info.recommended.c_str());

        ImGui::TextColored(ImVec4(0.72f, 0.75f, 0.82f, 1.0f), "%s", info.changeUpLabel.c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", info.increaseEffect.c_str());

        ImGui::TextColored(ImVec4(0.72f, 0.75f, 0.82f, 1.0f), "%s", info.changeDownLabel.c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", info.decreaseEffect.c_str());

        ImGui::TextColored(ImVec4(0.88f, 0.78f, 0.42f, 1.0f), "%s", u8"\u63d0\u793a");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", info.caution.c_str());

        ImGui::Unindent(8.0f);
        ImGui::TreePop();
    }
}

std::string widgetLabel(const ParamDef& param, const TeachingParamInfo* teachingInfo) {
    if (!teachingInfo || teachingInfo->displayName.empty()) {
        return param.name;
    }
    return teachingInfo->displayName + "##" + param.name;
}

} // namespace

void PropertyPanel::draw(NodeEditor& editor) {
    NodeInstance* sel = editor.selectedNode();

    if (!sel) {
        ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f), "Select a node to view properties");
        return;
    }

    const NodeTypeDef* def = NodeRegistry::instance().findType(sel->typeName);
    if (!def) {
        return;
    }

    CommandHistory* cmdHist = editor.commandHistory();
    const TeachingNodeInfo* teachingInfo = findTeachingNodeInfo(sel->typeName);

    ImVec4 hdrCol = ImGui::ColorConvertU32ToFloat4(def->headerColor);
    ImGui::TextColored(hdrCol, "%s", def->displayName.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f), "(id: %d)", sel->id);
    ImGui::Separator();

    if (teachingInfo) {
        drawTeachingNodeCard(*teachingInfo);
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Basic", ImGuiTreeNodeFlags_DefaultOpen)) {
        char labelBuf[128];
        strncpy(labelBuf, sel->label.c_str(), sizeof(labelBuf) - 1);
        labelBuf[sizeof(labelBuf) - 1] = '\0';
        if (ImGui::InputText("Label", labelBuf, sizeof(labelBuf))) {
            sel->label = labelBuf;
        }
        ImGui::LabelText("Type", "%s", sel->typeName.c_str());
        ImGui::LabelText("Category", "%s", def->category.c_str());
    }

    if (!sel->params.empty() && ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(-100);
        for (int i = 0; i < static_cast<int>(sel->params.size()); ++i) {
            ParamDef& p = sel->params[i];
            const TeachingParamInfo* paramTeachingInfo =
                teachingInfo ? findTeachingParamInfo(*teachingInfo, p.name) : nullptr;
            const std::string controlLabel = widgetLabel(p, paramTeachingInfo);

            ImGui::PushID(i);
            ParamDef oldVal = p;

            switch (p.type) {
            case ParamType::Float:
                ImGui::DragFloat(controlLabel.c_str(), &p.floatVal, p.step, p.minVal, p.maxVal, "%.4g");
                break;
            case ParamType::Int:
                ImGui::DragInt(controlLabel.c_str(), &p.intVal, p.step, static_cast<int>(p.minVal), static_cast<int>(p.maxVal));
                break;
            case ParamType::Bool:
                if (ImGui::Checkbox(controlLabel.c_str(), &p.boolVal)) {
                    if (cmdHist) {
                        ParamDef newVal = p;
                        p = oldVal;
                        auto cmd = std::make_unique<ChangeParamCmd>(editor, sel->id, i, oldVal, newVal);
                        cmdHist->execute(std::move(cmd));
                    }
                }
                break;
            case ParamType::String: {
                char buf[256];
                strncpy(buf, p.stringVal.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText(controlLabel.c_str(), buf, sizeof(buf))) {
                    p.stringVal = buf;
                }
                break;
            }
            case ParamType::Enum:
                if (!p.enumOptions.empty()) {
                    const char* preview =
                        p.enumIndex >= 0 && p.enumIndex < static_cast<int>(p.enumOptions.size())
                            ? p.enumOptions[p.enumIndex].c_str()
                            : "";
                    if (ImGui::BeginCombo(controlLabel.c_str(), preview)) {
                        for (int j = 0; j < static_cast<int>(p.enumOptions.size()); ++j) {
                            const bool selected = (j == p.enumIndex);
                            if (ImGui::Selectable(p.enumOptions[j].c_str(), selected)) {
                                if (cmdHist && j != oldVal.enumIndex) {
                                    ParamDef newVal = p;
                                    newVal.enumIndex = j;
                                    p = oldVal;
                                    auto cmd = std::make_unique<ChangeParamCmd>(editor, sel->id, i, oldVal, newVal);
                                    cmdHist->execute(std::move(cmd));
                                } else {
                                    p.enumIndex = j;
                                }
                            }
                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                break;
            case ParamType::Color3:
                ImGui::ColorEdit3(controlLabel.c_str(), p.color3);
                break;
            }

            if (paramTeachingInfo) {
                drawTeachingParamNote(*paramTeachingInfo);
                ImGui::Spacing();
            }

            if (p.type == ParamType::Float || p.type == ParamType::Int || p.type == ParamType::Color3) {
                if (ImGui::IsItemDeactivatedAfterEdit() && cmdHist) {
                    ParamDef newVal = p;
                    bool changed = false;
                    if (p.type == ParamType::Float) {
                        changed = (oldVal.floatVal != newVal.floatVal);
                    } else if (p.type == ParamType::Int) {
                        changed = (oldVal.intVal != newVal.intVal);
                    } else if (p.type == ParamType::Color3) {
                        changed = (oldVal.color3[0] != newVal.color3[0] ||
                                   oldVal.color3[1] != newVal.color3[1] ||
                                   oldVal.color3[2] != newVal.color3[2]);
                    }

                    if (changed) {
                        p = oldVal;
                        auto cmd = std::make_unique<ChangeParamCmd>(editor, sel->id, i, oldVal, newVal);
                        cmdHist->execute(std::move(cmd));
                    }
                }
            }

            ImGui::PopID();
        }
        ImGui::PopItemWidth();
    }

    if (ImGui::CollapsingHeader("Ports", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!def->inputs.empty()) {
            ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f), "Inputs:");
            for (const auto& port : def->inputs) {
                ImVec4 pCol = ImGui::ColorConvertU32ToFloat4(portDataTypeColor(port.dataType));
                ImGui::TextColored(pCol, "  [%s] %s", portDataTypeName(port.dataType), port.label.c_str());
            }
        }
        if (!def->outputs.empty()) {
            ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f), "Outputs:");
            for (const auto& port : def->outputs) {
                ImVec4 pCol = ImGui::ColorConvertU32ToFloat4(portDataTypeColor(port.dataType));
                ImGui::TextColored(pCol, "  [%s] %s", portDataTypeName(port.dataType), port.label.c_str());
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Delete Node", ImVec2(-1, 0))) {
        if (cmdHist) {
            auto cmd = std::make_unique<RemoveNodeCmd>(editor, sel->id);
            cmdHist->execute(std::move(cmd));
        } else {
            editor.removeNode(sel->id);
        }
    }
    ImGui::PopStyleColor(2);
}

} // namespace TopOpt
