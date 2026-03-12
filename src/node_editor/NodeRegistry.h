#pragma once
#include "Node.h"
#include <map>
#include <vector>
#include <string>

namespace TopOpt {

/**
 * NodeRegistry - Central registration of all available node types.
 *
 * Categories:
 *   Input, Output, Math, Process, Filter, Data, Topology
 *
 * To add a new node type for future development:
 *   registry.registerType(NodeTypeDef{ ... });
 */
class NodeRegistry {
public:
    static NodeRegistry& instance();

    void registerType(const NodeTypeDef& def);
    const NodeTypeDef* findType(const std::string& typeName) const;

    // All type names, grouped by category
    std::map<std::string, std::vector<std::string>> categorized() const;

    const std::map<std::string, NodeTypeDef>& allTypes() const { return types_; }

private:
    NodeRegistry();
    void registerBuiltinTypes();
    std::map<std::string, NodeTypeDef> types_;
};

} // namespace TopOpt
