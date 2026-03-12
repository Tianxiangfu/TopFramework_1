#include "PropertyPanel.h"
#include "../node_editor/NodeRegistry.h"
#include "../node_editor/Node.h"
#include "../commands/Command.h"
#include "../commands/NodeCommands.h"
#include "imgui.h"
#include <cstring>

namespace TopOpt {

void PropertyPanel::draw(NodeEditor& editor) {
    NodeInstance* sel = editor.selectedNode();

    if (!sel) {
        ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.0f), "Select a node to view properties");
        return;
    }

    auto* def = NodeRegistry::instance().findType(sel->typeName);
    if (!def) return;

    CommandHistory* cmdHist = editor.commandHistory();

    // Header
    ImVec4 hdrCol = ImGui::ColorConvertU32ToFloat4(def->headerColor);
    ImGui::TextColored(hdrCol, "%s", def->displayName.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.f), "(id: %d)", sel->id);
    ImGui::Separator();

    // --- Basic section ---
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

    // --- Parameters section ---
    if (!sel->params.empty() && ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(-100);
        for (int i = 0; i < (int)sel->params.size(); ++i) {
            auto& p = sel->params[i];
            ImGui::PushID(i);

            // Capture old value when editing begins
            ParamDef oldVal = p;

            switch (p.type) {
                case ParamType::Float:
                    ImGui::DragFloat(p.name.c_str(), &p.floatVal, p.step, p.minVal, p.maxVal, "%.4g");
                    break;
                case ParamType::Int:
                    ImGui::DragInt(p.name.c_str(), &p.intVal, p.step, (int)p.minVal, (int)p.maxVal);
                    break;
                case ParamType::Bool:
                    if (ImGui::Checkbox(p.name.c_str(), &p.boolVal)) {
                        // Bool changes are instant - create undo command immediately
                        if (cmdHist) {
                            ParamDef newVal = p;
                            // Swap back to old so command's execute() applies the new value
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
                    if (ImGui::InputText(p.name.c_str(), buf, sizeof(buf))) {
                        p.stringVal = buf;
                    }
                    break;
                }
                case ParamType::Enum:
                    if (!p.enumOptions.empty()) {
                        if (ImGui::BeginCombo(p.name.c_str(),
                                p.enumIndex < (int)p.enumOptions.size()
                                    ? p.enumOptions[p.enumIndex].c_str() : "")) {
                            for (int j = 0; j < (int)p.enumOptions.size(); ++j) {
                                bool selected = (j == p.enumIndex);
                                if (ImGui::Selectable(p.enumOptions[j].c_str(), selected)) {
                                    if (cmdHist && j != oldVal.enumIndex) {
                                        ParamDef newVal = p;
                                        newVal.enumIndex = j;
                                        p = oldVal; // revert, let command apply
                                        auto cmd = std::make_unique<ChangeParamCmd>(editor, sel->id, i, oldVal, newVal);
                                        cmdHist->execute(std::move(cmd));
                                    } else {
                                        p.enumIndex = j;
                                    }
                                }
                                if (selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }
                    break;
                case ParamType::Color3:
                    ImGui::ColorEdit3(p.name.c_str(), p.color3);
                    break;
            }

            // For drag/slider controls: create undo command when editing finishes
            if (p.type == ParamType::Float || p.type == ParamType::Int || p.type == ParamType::Color3) {
                if (ImGui::IsItemDeactivatedAfterEdit() && cmdHist) {
                    ParamDef newVal = p;
                    // Check if actually changed
                    bool changed = false;
                    if (p.type == ParamType::Float) changed = (oldVal.floatVal != newVal.floatVal);
                    else if (p.type == ParamType::Int) changed = (oldVal.intVal != newVal.intVal);
                    else if (p.type == ParamType::Color3) {
                        changed = (oldVal.color3[0] != newVal.color3[0] ||
                                   oldVal.color3[1] != newVal.color3[1] ||
                                   oldVal.color3[2] != newVal.color3[2]);
                    }
                    if (changed) {
                        // Don't re-execute; value is already set. Push directly to undo stack.
                        // Create a command that just records the change
                        p = oldVal; // revert so execute() applies new value
                        auto cmd = std::make_unique<ChangeParamCmd>(editor, sel->id, i, oldVal, newVal);
                        cmdHist->execute(std::move(cmd));
                    }
                }
            }

            ImGui::PopID();
        }
        ImGui::PopItemWidth();
    }

    // --- Ports section ---
    if (ImGui::CollapsingHeader("Ports", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!def->inputs.empty()) {
            ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.f), "Inputs:");
            for (auto& port : def->inputs) {
                ImVec4 pCol = ImGui::ColorConvertU32ToFloat4(portDataTypeColor(port.dataType));
                ImGui::TextColored(pCol, "  [%s] %s", portDataTypeName(port.dataType), port.label.c_str());
            }
        }
        if (!def->outputs.empty()) {
            ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.53f, 1.f), "Outputs:");
            for (auto& port : def->outputs) {
                ImVec4 pCol = ImGui::ColorConvertU32ToFloat4(portDataTypeColor(port.dataType));
                ImGui::TextColored(pCol, "  [%s] %s", portDataTypeName(port.dataType), port.label.c_str());
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Delete button (with undo support)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.f));
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
