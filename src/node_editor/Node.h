#pragma once
#include <string>
#include <vector>
#include <variant>
#include <map>

namespace TopOpt {

// ============================================================
//  Port data types
// ============================================================
enum class PortDataType {
    Number,
    Vector,
    Mesh,
    Field,
    Matrix,
    Boolean,
    String,
    Generic,
    // FEA/Topology types
    FEMesh,
    Material,
    BoundaryCondition,
    LoadCase,
    FEResult,
    DensityField
};

const char* portDataTypeName(PortDataType t);
unsigned int portDataTypeColor(PortDataType t); // Returns IM_COL32 color

// ============================================================
//  Port definition
// ============================================================
struct PortDef {
    std::string  id;
    std::string  label;
    PortDataType dataType = PortDataType::Generic;
};

// ============================================================
//  Parameter definition
// ============================================================
enum class ParamType { Float, Int, Bool, String, Enum, Color3 };

struct ParamDef {
    std::string name;
    ParamType   type = ParamType::Float;
    float       floatVal  = 0.0f;
    int         intVal    = 0;
    bool        boolVal   = false;
    std::string stringVal;
    int         enumIndex = 0;
    std::vector<std::string> enumOptions;
    float       color3[3] = {1.f, 1.f, 1.f};
    float       minVal    = -1e9f;
    float       maxVal    =  1e9f;
    float       step      = 0.1f;
};

// ============================================================
//  Node type definition (template/blueprint)
// ============================================================
struct NodeTypeDef {
    std::string           typeName;
    std::string           displayName;
    std::string           category;
    std::string           icon;       // Short text icon
    std::string           description;
    unsigned int          headerColor; // IM_COL32
    std::vector<PortDef>  inputs;
    std::vector<PortDef>  outputs;
    std::vector<ParamDef> defaultParams;
};

// ============================================================
//  Node instance (live node on canvas)
// ============================================================
struct NodeInstance {
    int                   id = -1;
    std::string           typeName;
    std::string           label;
    float                 posX = 0, posY = 0;
    std::vector<ParamDef> params;

    // Attribute IDs for imnodes: input[i] = id*1000 + i, output[i] = id*1000 + 500 + i
    int inputAttrId(int portIndex)  const { return id * 1000 + portIndex; }
    int outputAttrId(int portIndex) const { return id * 1000 + 500 + portIndex; }
};

// ============================================================
//  Connection (link between ports)
// ============================================================
struct Connection {
    int id = -1;
    int startNodeId  = -1;
    int startPortIdx = -1;
    int endNodeId    = -1;
    int endPortIdx   = -1;

    int startAttrId() const { return startNodeId * 1000 + 500 + startPortIdx; }
    int endAttrId()   const { return endNodeId   * 1000 + endPortIdx; }
};

} // namespace TopOpt
