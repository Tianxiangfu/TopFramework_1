#include "Node.h"
#include "imgui.h"

namespace TopOpt {

const char* portDataTypeName(PortDataType t) {
    switch (t) {
        case PortDataType::Number:            return "number";
        case PortDataType::Vector:            return "vector";
        case PortDataType::Mesh:              return "mesh";
        case PortDataType::Field:             return "field";
        case PortDataType::Matrix:            return "matrix";
        case PortDataType::Boolean:           return "boolean";
        case PortDataType::String:            return "string";
        case PortDataType::Generic:           return "generic";
        case PortDataType::FEMesh:            return "femesh";
        case PortDataType::Material:          return "material";
        case PortDataType::BoundaryCondition: return "bc";
        case PortDataType::LoadCase:          return "loadcase";
        case PortDataType::FEResult:          return "feresult";
        case PortDataType::DensityField:      return "densityfield";
    }
    return "unknown";
}

unsigned int portDataTypeColor(PortDataType t) {
    switch (t) {
        case PortDataType::Number:            return IM_COL32( 78, 205, 196, 255);
        case PortDataType::Vector:            return IM_COL32(255, 107, 157, 255);
        case PortDataType::Mesh:              return IM_COL32(255, 167,  38, 255);
        case PortDataType::Field:             return IM_COL32(171,  71, 188, 255);
        case PortDataType::Matrix:            return IM_COL32( 66, 165, 245, 255);
        case PortDataType::Boolean:           return IM_COL32(102, 187, 106, 255);
        case PortDataType::String:            return IM_COL32(239,  83,  80, 255);
        case PortDataType::Generic:           return IM_COL32(160, 160, 184, 255);
        case PortDataType::FEMesh:            return IM_COL32(255, 200,  60, 255); // gold
        case PortDataType::Material:          return IM_COL32(140, 200, 100, 255); // teal-green
        case PortDataType::BoundaryCondition: return IM_COL32(255, 120,  80, 255); // coral
        case PortDataType::LoadCase:          return IM_COL32(100, 180, 220, 255); // sky blue
        case PortDataType::FEResult:          return IM_COL32(200, 100, 200, 255); // purple
        case PortDataType::DensityField:      return IM_COL32(220, 180, 100, 255); // amber
    }
    return IM_COL32(160, 160, 184, 255);
}

} // namespace TopOpt
