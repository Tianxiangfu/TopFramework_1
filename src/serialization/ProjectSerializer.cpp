#include "ProjectSerializer.h"
#include "../node_editor/NodeEditor.h"
#include "../node_editor/Node.h"
#include "../node_editor/NodeRegistry.h"
#include "../utils/Logger.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>

using json = nlohmann::json;

namespace TopOpt {

namespace {

void mergeMissingParamsFromRegistry(NodeInstance& node) {
    const NodeTypeDef* def = NodeRegistry::instance().findType(node.typeName);
    if (!def) {
        return;
    }

    for (const ParamDef& defaultParam : def->defaultParams) {
        auto it = std::find_if(
            node.params.begin(),
            node.params.end(),
            [&](const ParamDef& existing) { return existing.name == defaultParam.name; }
        );
        if (it == node.params.end()) {
            node.params.push_back(defaultParam);
            continue;
        }

        if (it->type == ParamType::Enum) {
            it->enumOptions = defaultParam.enumOptions;
            if (!it->enumOptions.empty()) {
                it->enumIndex = std::clamp(
                    it->enumIndex,
                    0,
                    static_cast<int>(it->enumOptions.size()) - 1
                );
            } else {
                it->enumIndex = 0;
            }
        }
    }
}

} // namespace

// ================================================================
//  ParamDef JSON
// ================================================================

static const char* paramTypeName(ParamType t) {
    switch (t) {
        case ParamType::Float:  return "Float";
        case ParamType::Int:    return "Int";
        case ParamType::Bool:   return "Bool";
        case ParamType::String: return "String";
        case ParamType::Enum:   return "Enum";
        case ParamType::Color3: return "Color3";
    }
    return "Float";
}

static ParamType parseParamType(const std::string& s) {
    if (s == "Int")    return ParamType::Int;
    if (s == "Bool")   return ParamType::Bool;
    if (s == "String") return ParamType::String;
    if (s == "Enum")   return ParamType::Enum;
    if (s == "Color3") return ParamType::Color3;
    return ParamType::Float;
}

static json paramToJson(const ParamDef& p) {
    json j;
    j["name"] = p.name;
    j["type"] = paramTypeName(p.type);
    switch (p.type) {
        case ParamType::Float:
            j["value"] = p.floatVal;
            j["min"]   = p.minVal;
            j["max"]   = p.maxVal;
            j["step"]  = p.step;
            break;
        case ParamType::Int:
            j["value"] = p.intVal;
            j["min"]   = p.minVal;
            j["max"]   = p.maxVal;
            j["step"]  = p.step;
            break;
        case ParamType::Bool:
            j["value"] = p.boolVal;
            break;
        case ParamType::String:
            j["value"] = p.stringVal;
            break;
        case ParamType::Enum:
            j["index"]   = p.enumIndex;
            j["options"] = p.enumOptions;
            break;
        case ParamType::Color3:
            j["value"] = { p.color3[0], p.color3[1], p.color3[2] };
            break;
    }
    return j;
}

static ParamDef paramFromJson(const json& j) {
    ParamDef p;
    p.name = j.value("name", "");
    p.type = parseParamType(j.value("type", "Float"));
    switch (p.type) {
        case ParamType::Float:
            p.floatVal = j.value("value", 0.0f);
            p.minVal   = j.value("min", -1e9f);
            p.maxVal   = j.value("max",  1e9f);
            p.step     = j.value("step", 0.1f);
            break;
        case ParamType::Int:
            p.intVal = j.value("value", 0);
            p.minVal = j.value("min", -1e9f);
            p.maxVal = j.value("max",  1e9f);
            p.step   = j.value("step", 1.0f);
            break;
        case ParamType::Bool:
            p.boolVal = j.value("value", false);
            break;
        case ParamType::String:
            p.stringVal = j.value("value", "");
            break;
        case ParamType::Enum:
            p.enumIndex = j.value("index", 0);
            if (j.contains("options"))
                p.enumOptions = j["options"].get<std::vector<std::string>>();
            break;
        case ParamType::Color3:
            if (j.contains("value") && j["value"].is_array() && j["value"].size() == 3) {
                p.color3[0] = j["value"][0];
                p.color3[1] = j["value"][1];
                p.color3[2] = j["value"][2];
            }
            break;
    }
    return p;
}

// ================================================================
//  NodeInstance JSON
// ================================================================

static json nodeToJson(const NodeInstance& n) {
    json j;
    j["id"]       = n.id;
    j["typeName"] = n.typeName;
    j["label"]    = n.label;
    j["posX"]     = n.posX;
    j["posY"]     = n.posY;
    json params = json::array();
    for (auto& p : n.params) {
        params.push_back(paramToJson(p));
    }
    j["params"] = params;
    return j;
}

static NodeInstance nodeFromJson(const json& j) {
    NodeInstance n;
    n.id       = j.value("id", -1);
    n.typeName = j.value("typeName", "");
    n.label    = j.value("label", "");
    n.posX     = j.value("posX", 0.0f);
    n.posY     = j.value("posY", 0.0f);
    if (j.contains("params") && j["params"].is_array()) {
        for (auto& pj : j["params"]) {
            n.params.push_back(paramFromJson(pj));
        }
    }
    mergeMissingParamsFromRegistry(n);
    return n;
}

// ================================================================
//  Connection JSON
// ================================================================

static json connToJson(const Connection& c) {
    return {
        {"id",           c.id},
        {"startNodeId",  c.startNodeId},
        {"startPortIdx", c.startPortIdx},
        {"endNodeId",    c.endNodeId},
        {"endPortIdx",   c.endPortIdx}
    };
}

static Connection connFromJson(const json& j) {
    Connection c;
    c.id           = j.value("id", -1);
    c.startNodeId  = j.value("startNodeId", -1);
    c.startPortIdx = j.value("startPortIdx", -1);
    c.endNodeId    = j.value("endNodeId", -1);
    c.endPortIdx   = j.value("endPortIdx", -1);
    return c;
}

// ================================================================
//  Save / Load
// ================================================================

bool ProjectSerializer::saveToFile(const std::string& path,
                                    NodeEditor& editor,
                                    const ViewState& view) {
    try {
        // Sync node positions from imnodes before saving
        editor.syncPositionsFromImnodes();

        json root;
        root["version"] = "1.0";

        // Nodes
        json nodesArr = json::array();
        for (auto& n : editor.nodes()) {
            nodesArr.push_back(nodeToJson(n));
        }
        root["nodes"] = nodesArr;

        // Connections
        json connsArr = json::array();
        for (auto& c : editor.connections()) {
            connsArr.push_back(connToJson(c));
        }
        root["connections"] = connsArr;

        // Editor state
        root["nextNodeId"] = editor.nextNodeId();
        root["nextConnId"] = editor.nextConnId();

        // imnodes editor state
        root["imnodesState"] = editor.saveImnodesState();

        // View state
        json vs;
        vs["horizontalSplitRatio"] = view.horizontalSplitRatio;
        vs["leftVertSplitRatio"]   = view.leftVertSplitRatio;
        vs["centerVertSplitRatio"] = view.centerVertSplitRatio;
        vs["rightPanelRatio"]      = view.rightPanelRatio;
        vs["camDistance"] = view.camDistance;
        vs["camYaw"]      = view.camYaw;
        vs["camPitch"]    = view.camPitch;
        vs["camCenter"]   = { view.camCenter[0], view.camCenter[1], view.camCenter[2] };
        root["viewState"] = vs;

        // Write to file
        std::ofstream ofs(path, std::ios::out | std::ios::binary);
        if (!ofs.is_open()) {
            Logger::instance().error("Failed to open file for writing: " + path);
            return false;
        }
        std::string jsonStr = root.dump(2);
        ofs.write(jsonStr.c_str(), jsonStr.size());
        ofs.flush();
        ofs.close();

        // Verify file was created
        std::ifstream verify(path);
        if (!verify.is_open()) {
            Logger::instance().error("File was not created successfully: " + path);
            return false;
        }
        verify.close();

        Logger::instance().info("Project saved: " + path);
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Save failed: ") + e.what());
        return false;
    }
}

bool ProjectSerializer::loadFromFile(const std::string& path,
                                      NodeEditor& editor,
                                      ViewState& view) {
    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            Logger::instance().error("Failed to open file: " + path);
            return false;
        }

        json root = json::parse(ifs);
        ifs.close();

        // Clear current state
        editor.clear();

        // Load nodes
        if (root.contains("nodes") && root["nodes"].is_array()) {
            for (auto& nj : root["nodes"]) {
                NodeInstance n = nodeFromJson(nj);
                editor.addNodeDirect(n);
            }
        }

        // Load connections
        if (root.contains("connections") && root["connections"].is_array()) {
            for (auto& cj : root["connections"]) {
                Connection c = connFromJson(cj);
                editor.addConnectionDirect(c);
            }
        }

        // Restore editor IDs
        editor.setNextNodeId(root.value("nextNodeId", 1));
        editor.setNextConnId(root.value("nextConnId", 1));

        // Restore imnodes state
        if (root.contains("imnodesState")) {
            editor.loadImnodesState(root["imnodesState"].get<std::string>());
        }

        // Restore view state
        if (root.contains("viewState")) {
            auto& vs = root["viewState"];
            view.horizontalSplitRatio = vs.value("horizontalSplitRatio", 0.55f);
            view.leftVertSplitRatio   = vs.value("leftVertSplitRatio", 0.65f);
            view.centerVertSplitRatio = vs.value("centerVertSplitRatio", 0.65f);
            view.rightPanelRatio      = vs.value("rightPanelRatio", 0.15f);
            view.camDistance = vs.value("camDistance", 5.0f);
            view.camYaw      = vs.value("camYaw", 45.0f);
            view.camPitch    = vs.value("camPitch", 30.0f);
            if (vs.contains("camCenter") && vs["camCenter"].is_array() && vs["camCenter"].size() == 3) {
                view.camCenter[0] = vs["camCenter"][0];
                view.camCenter[1] = vs["camCenter"][1];
                view.camCenter[2] = vs["camCenter"][2];
            }
        }

        Logger::instance().info("Project loaded: " + path);
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Load failed: ") + e.what());
        return false;
    }
}

} // namespace TopOpt
