#include "NodeRegistry.h"
#include "imgui.h"

namespace TopOpt {

NodeRegistry& NodeRegistry::instance() {
    static NodeRegistry inst;
    return inst;
}

NodeRegistry::NodeRegistry() {
    registerBuiltinTypes();
}

void NodeRegistry::registerType(const NodeTypeDef& def) {
    types_[def.typeName] = def;
}

const NodeTypeDef* NodeRegistry::findType(const std::string& typeName) const {
    auto it = types_.find(typeName);
    return (it != types_.end()) ? &it->second : nullptr;
}

std::map<std::string, std::vector<std::string>> NodeRegistry::categorized() const {
    std::map<std::string, std::vector<std::string>> cats;
    for (auto& [name, def] : types_) {
        cats[def.category].push_back(name);
    }
    return cats;
}

// ================================================================
//  Built-in node type definitions
// ================================================================

static unsigned int hdrInput      = IM_COL32( 58, 124, 165, 255);
static unsigned int hdrOutput     = IM_COL32(106, 212, 154, 255);
static unsigned int hdrData       = IM_COL32(212, 106, 154, 255);
static unsigned int hdrDomain     = IM_COL32(180, 140,  50, 255);
static unsigned int hdrFEA        = IM_COL32(100, 160, 200, 255);
static unsigned int hdrTopo       = IM_COL32(160, 100, 180, 255);
static unsigned int hdrPostProc   = IM_COL32(120, 180, 120, 255);

void NodeRegistry::registerBuiltinTypes() {

    // ===================== Input =====================

    registerType({
        "input-number", "Number Input", "Input", "#", "Outputs a constant number",
        hdrInput,
        {},
        {{ "value", "Value", PortDataType::Number }},
        {{ "Value", ParamType::Float, 0.0f }}
    });

    registerType({
        "input-vector", "Vector Input", "Input", "V", "Outputs a 3D vector",
        hdrInput,
        {},
        {{ "vector", "Vector", PortDataType::Vector }},
        {
            { "X", ParamType::Float, 0.0f },
            { "Y", ParamType::Float, 0.0f },
            { "Z", ParamType::Float, 0.0f }
        }
    });

    registerType({
        "input-file", "File Input", "Input", "F", "Load data from file",
        hdrInput,
        {},
        {{ "data", "Data", PortDataType::Generic }},
        {
            { "FilePath", ParamType::String },
            { "Format", ParamType::Enum, 0.f, 0, false, "", 0, {"auto","obj","stl","vtk","csv"} }
        }
    });

    registerType({
        "input-boolean", "Boolean Input", "Input", "B", "Outputs a boolean",
        hdrInput,
        {},
        {{ "value", "Value", PortDataType::Boolean }},
        {{ "Value", ParamType::Bool, 0.f, 0, false }}
    });

    // ===================== Output =====================

    registerType({
        "output-display", "Display", "Output", "D", "Display output data",
        hdrOutput,
        {{ "data", "Data", PortDataType::Generic }},
        {},
        {{ "Title", ParamType::String, 0.f, 0, false, "Output" }}
    });

    registerType({
        "output-export", "File Export", "Output", "E", "Export data to file",
        hdrOutput,
        {{ "data", "Data", PortDataType::Generic }},
        {},
        {
            { "FilePath", ParamType::String },
            { "Format", ParamType::Enum, 0.f, 0, false, "", 0, {"obj","stl","vtk","csv"} }
        }
    });

    registerType({
        "output-viewer", "3D Viewer", "Output", "3D", "Visualize mesh in 3D",
        hdrOutput,
        {
            { "mesh",  "Mesh",  PortDataType::Mesh },
            { "field", "Field", PortDataType::Field }
        },
        {},
        {
            { "ShowEdges", ParamType::Bool, 0.f, 0, true },
            { "Color", ParamType::Color3, 0.f, 0, false, "", 0, {}, {0.3f, 0.8f, 0.75f} }
        }
    });

    // ===================== Data =====================

    registerType({
        "data-mesh-gen", "Mesh Generator", "Data", "MG", "Generate primitive mesh",
        hdrData,
        {},
        {{ "mesh", "Mesh", PortDataType::Mesh }},
        {
            { "Shape", ParamType::Enum, 0.f, 0, false, "", 0, {"Box","Sphere","Cylinder","Plane","Torus"} },
            { "Resolution", ParamType::Int, 0.f, 10, false, "", 0, {}, {}, 2.f, 100.f, 1.f }
        }
    });

    // ===================== Domain (4 nodes) =====================

    registerType({
        "domain-box", "Box Domain", "Domain", "[]", "Generate a box-shaped FE domain with hex8 elements",
        hdrDomain,
        {},
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {
            { "LengthX", ParamType::Float, 10.0f, 0, false, "", 0, {}, {}, 0.1f, 1e6f, 1.0f },
            { "LengthY", ParamType::Float, 5.0f,  0, false, "", 0, {}, {}, 0.1f, 1e6f, 1.0f },
            { "LengthZ", ParamType::Float, 1.0f,  0, false, "", 0, {}, {}, 0.1f, 1e6f, 1.0f },
            { "ElemsX",  ParamType::Int, 0.f, 20, false, "", 0, {}, {}, 1.f, 500.f, 1.f },
            { "ElemsY",  ParamType::Int, 0.f, 10, false, "", 0, {}, {}, 1.f, 500.f, 1.f },
            { "ElemsZ",  ParamType::Int, 0.f, 1,  false, "", 0, {}, {}, 1.f, 500.f, 1.f }
        }
    });

    registerType({
        "domain-lshape", "L-Shape Domain", "Domain", "L", "Generate an L-shaped FE domain",
        hdrDomain,
        {},
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {
            { "Width",       ParamType::Float, 10.0f, 0, false, "", 0, {}, {}, 0.1f, 1e6f, 1.0f },
            { "Height",      ParamType::Float, 10.0f, 0, false, "", 0, {}, {}, 0.1f, 1e6f, 1.0f },
            { "Depth",       ParamType::Float, 1.0f,  0, false, "", 0, {}, {}, 0.1f, 1e6f, 1.0f },
            { "CutFraction", ParamType::Float, 0.5f,  0, false, "", 0, {}, {}, 0.1f, 0.9f, 0.1f },
            { "ElemSize",    ParamType::Float, 1.0f,  0, false, "", 0, {}, {}, 0.1f, 100.f, 0.1f }
        }
    });

    registerType({
        "domain-from-mesh", "Mesh to FE Domain", "Domain", "M>", "Convert surface mesh to FE domain",
        hdrDomain,
        {{ "mesh", "Mesh", PortDataType::Mesh }},
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {
            { "MaxElemSize", ParamType::Float, 1.0f, 0, false, "", 0, {}, {}, 0.01f, 100.f, 0.1f }
        }
    });

    registerType({
        "domain-import", "Import FE Mesh", "Domain", "IF", "Import FE mesh from file",
        hdrDomain,
        {},
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {
            { "FilePath", ParamType::String },
            { "Format", ParamType::Enum, 0.f, 0, false, "", 0, {"vtk","inp"} }
        }
    });

    // ===================== FEA (8 nodes) =====================

    registerType({
        "fea-material", "Isotropic Material", "FEA", "Mt", "Define isotropic material properties",
        hdrFEA,
        {},
        {{ "material", "Material", PortDataType::Material }},
        {
            { "E",       ParamType::Float, 210000.f, 0, false, "", 0, {}, {}, 1.f, 1e9f, 1000.f },
            { "nu",      ParamType::Float, 0.3f,     0, false, "", 0, {}, {}, 0.f, 0.49f, 0.01f },
            { "Density", ParamType::Float, 7850.f,   0, false, "", 0, {}, {}, 0.f, 1e6f, 100.f },
            { "Name",    ParamType::String, 0.f, 0, false, "Steel" }
        }
    });

    registerType({
        "fea-fixed-support", "Fixed Support", "FEA", "FX", "Apply fixed boundary condition",
        hdrFEA,
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {{ "bc", "BC", PortDataType::BoundaryCondition }},
        {
            { "SelectionMode", ParamType::Enum, 0.f, 0, false, "", 0, {"Face","CoordRange"} },
            { "Face",   ParamType::Enum, 0.f, 0, false, "", 0, {"X-","X+","Y-","Y+","Z-","Z+"} },
            { "FixX",   ParamType::Bool, 0.f, 0, true },
            { "FixY",   ParamType::Bool, 0.f, 0, true },
            { "FixZ",   ParamType::Bool, 0.f, 0, true },
            { "CoordAxis", ParamType::Enum, 0.f, 0, false, "", 0, {"X","Y","Z"} },
            { "CoordMin",  ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "CoordMax",  ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "Tolerance", ParamType::Float, 1e-6f, 0, false, "", 0, {}, {}, 0.f, 1.f, 1e-6f }
        }
    });

    registerType({
        "fea-displacement-bc", "Displacement BC", "FEA", "Dp", "Apply prescribed displacement",
        hdrFEA,
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {{ "bc", "BC", PortDataType::BoundaryCondition }},
        {
            { "SelectionMode", ParamType::Enum, 0.f, 0, false, "", 0, {"Face","CoordRange"} },
            { "Face",   ParamType::Enum, 0.f, 0, false, "", 0, {"X-","X+","Y-","Y+","Z-","Z+"} },
            { "DispX",  ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "DispY",  ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "DispZ",  ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "ApplyX", ParamType::Bool, 0.f, 0, true },
            { "ApplyY", ParamType::Bool, 0.f, 0, true },
            { "ApplyZ", ParamType::Bool, 0.f, 0, true },
            { "CoordAxis", ParamType::Enum, 0.f, 0, false, "", 0, {"X","Y","Z"} },
            { "CoordMin",  ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "CoordMax",  ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "Tolerance", ParamType::Float, 1e-6f, 0, false, "", 0, {}, {}, 0.f, 1.f, 1e-6f }
        }
    });

    registerType({
        "fea-point-force", "Point Force", "FEA", "F>", "Apply concentrated force",
        hdrFEA,
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {{ "bc", "BC", PortDataType::BoundaryCondition }},
        {
            { "SelectionMode", ParamType::Enum, 0.f, 0, false, "", 0, {"Face","CoordRange"} },
            { "Face",     ParamType::Enum, 0.f, 0, false, "", 0, {"X-","X+","Y-","Y+","Z-","Z+"} },
            { "ForceX",   ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e9f, 1e9f, 10.f },
            { "ForceY",   ParamType::Float, -1.0f, 0, false, "", 0, {}, {}, -1e9f, 1e9f, 10.f },
            { "ForceZ",   ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e9f, 1e9f, 10.f },
            { "Distribution", ParamType::Enum, 0.f, 0, false, "", 0, {"Total","PerNode"} },
            { "CoordAxis", ParamType::Enum, 0.f, 0, false, "", 0, {"X","Y","Z"} },
            { "CoordMin",  ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "CoordMax",  ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "Tolerance", ParamType::Float, 1e-6f, 0, false, "", 0, {}, {}, 0.f, 1.f, 1e-6f }
        }
    });

    registerType({
        "fea-pressure-load", "Pressure Load", "FEA", "Pr", "Apply pressure on a face",
        hdrFEA,
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {{ "bc", "BC", PortDataType::BoundaryCondition }},
        {
            { "Face",     ParamType::Enum, 0.f, 0, false, "", 0, {"X-","X+","Y-","Y+","Z-","Z+"} },
            { "Pressure", ParamType::Float, 1.0f, 0, false, "", 0, {}, {}, -1e9f, 1e9f, 1.0f },
            { "Direction", ParamType::Enum, 0.f, 0, false, "", 0, {"Normal","CustomX","CustomY","CustomZ"} }
        }
    });

    registerType({
        "fea-body-force", "Body Force", "FEA", "G", "Apply body force (gravity)",
        hdrFEA,
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {{ "bc", "BC", PortDataType::BoundaryCondition }},
        {
            { "GravityX", ParamType::Float, 0.0f,    0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "GravityY", ParamType::Float, -9810.f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "GravityZ", ParamType::Float, 0.0f,    0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f }
        }
    });

    registerType({
        "fea-load-case", "Load Case", "FEA", "LC", "Assemble boundary conditions into a load case",
        hdrFEA,
        {
            { "bc0", "BC 0", PortDataType::BoundaryCondition },
            { "bc1", "BC 1", PortDataType::BoundaryCondition },
            { "bc2", "BC 2", PortDataType::BoundaryCondition },
            { "bc3", "BC 3", PortDataType::BoundaryCondition }
        },
        {{ "loadcase", "LoadCase", PortDataType::LoadCase }},
        {
            { "Name",   ParamType::String, 0.f, 0, false, "LC1" },
            { "Weight", ParamType::Float, 1.0f, 0, false, "", 0, {}, {}, 0.f, 100.f, 0.1f }
        }
    });

    registerType({
        "fea-solver", "FEA Solver", "FEA", "FE", "Solve finite element analysis",
        hdrFEA,
        {
            { "femesh",   "FEMesh",   PortDataType::FEMesh },
            { "material", "Material", PortDataType::Material },
            { "lc0",      "LC 0",     PortDataType::LoadCase },
            { "lc1",      "LC 1",     PortDataType::LoadCase },
            { "lc2",      "LC 2",     PortDataType::LoadCase }
        },
        {
            { "result", "Result", PortDataType::FEResult },
            { "femesh", "FEMesh", PortDataType::FEMesh }
        },
        {
            { "Backend", ParamType::Enum, 0.f, 0, false, "", 0, {"Auto","CPU Direct","GPU AmgX"} },
            { "EnableGPU", ParamType::Bool, 0.f, 0, true },
            { "FallbackToCPU", ParamType::Bool, 0.f, 0, true },
            { "MaxIter", ParamType::Int, 0.f, 1000 },
            { "Tol",     ParamType::Float, 1e-6f, 0, false, "", 0, {}, {}, 1e-12f, 1.f, 1e-6f },
            { "AmgXPath", ParamType::String, 0.f, 0, false, "" }
        }
    });

    // ===================== Topology (4 nodes) =====================

    registerType({
        "topo-simp", "SIMP Optimizer", "Topology", "SP", "SIMP topology optimization",
        hdrTopo,
        {
            { "femesh",   "FEMesh",   PortDataType::FEMesh },
            { "material", "Material", PortDataType::Material },
            { "lc0",      "LC 0",     PortDataType::LoadCase },
            { "lc1",      "LC 1",     PortDataType::LoadCase },
            { "lc2",      "LC 2",     PortDataType::LoadCase }
        },
        {
            { "density", "Density", PortDataType::DensityField },
            { "result",  "Result",  PortDataType::FEResult }
        },
        {
            { "Backend",    ParamType::Enum, 0.f, 0, false, "", 0, {"Auto","CPU Direct","GPU AmgX"} },
            { "EnableGPU",  ParamType::Bool, 0.f, 0, true },
            { "FallbackToCPU", ParamType::Bool, 0.f, 0, true },
            { "VolFrac",    ParamType::Float, 0.5f,  0, false, "", 0, {}, {}, 0.01f, 0.99f, 0.01f },
            { "Penalty",    ParamType::Float, 3.0f,  0, false, "", 0, {}, {}, 1.f, 5.f, 0.5f },
            { "FilterR",    ParamType::Float, 1.5f,  0, false, "", 0, {}, {}, 0.1f, 50.f, 0.1f },
            { "FilterType", ParamType::Enum,  0.f, 0, false, "", 0, {"Density","Sensitivity"} },
            { "MaxIter",    ParamType::Int,   0.f, 200, false, "", 0, {}, {}, 1.f, 2000.f, 1.f },
            { "MinDensity", ParamType::Float, 0.001f, 0, false, "", 0, {}, {}, 1e-6f, 0.1f, 0.001f },
            { "SolverMaxIter", ParamType::Int, 0.f, 1000, false, "", 0, {}, {}, 1.f, 10000.f, 10.f },
            { "SolverTol",  ParamType::Float, 1e-6f, 0, false, "", 0, {}, {}, 1e-12f, 1.f, 1e-6f },
            { "Objective",  ParamType::Enum,  0.f, 0, false, "", 0, {"MinCompliance"} },
            { "AmgXPath",   ParamType::String, 0.f, 0, false, "" }
        }
    });

    registerType({
        "topo-beso", "BESO Optimizer", "Topology", "BE", "BESO topology optimization",
        hdrTopo,
        {
            { "femesh",   "FEMesh",   PortDataType::FEMesh },
            { "material", "Material", PortDataType::Material },
            { "lc0",      "LC 0",     PortDataType::LoadCase },
            { "lc1",      "LC 1",     PortDataType::LoadCase }
        },
        {
            { "density", "Density", PortDataType::DensityField },
            { "result",  "Result",  PortDataType::FEResult }
        },
        {
            { "Backend",       ParamType::Enum, 0.f, 0, false, "", 0, {"Auto","CPU Direct","GPU AmgX"} },
            { "EnableGPU",     ParamType::Bool, 0.f, 0, true },
            { "FallbackToCPU", ParamType::Bool, 0.f, 0, true },
            { "VolFrac",       ParamType::Float, 0.5f,  0, false, "", 0, {}, {}, 0.01f, 0.99f, 0.01f },
            { "EvolutionRate", ParamType::Float, 0.02f, 0, false, "", 0, {}, {}, 0.001f, 0.1f, 0.005f },
            { "FilterR",       ParamType::Float, 1.5f,  0, false, "", 0, {}, {}, 0.1f, 50.f, 0.1f },
            { "MaxIter",       ParamType::Int,   0.f, 200, false, "", 0, {}, {}, 1.f, 2000.f, 1.f },
            { "SolverMaxIter", ParamType::Int, 0.f, 1000, false, "", 0, {}, {}, 1.f, 10000.f, 10.f },
            { "SolverTol",     ParamType::Float, 1e-6f, 0, false, "", 0, {}, {}, 1e-12f, 1.f, 1e-6f },
            { "AmgXPath",      ParamType::String, 0.f, 0, false, "" }
        }
    });

    registerType({
        "topo-constraint", "Design Constraint", "Topology", "DC", "Apply design constraint to density field",
        hdrTopo,
        {{ "density", "Density", PortDataType::DensityField }},
        {{ "density", "Density", PortDataType::DensityField }},
        {
            { "ConstraintType", ParamType::Enum, 0.f, 0, false, "", 0, {"Symmetry","MinSize","Extrusion","Overhang"} },
            { "SymmetryAxis",   ParamType::Enum, 0.f, 0, false, "", 0, {"X","Y","Z"} },
            { "MinSize",        ParamType::Float, 2.0f, 0, false, "", 0, {}, {}, 0.1f, 50.f, 0.1f },
            { "ExtrusionAxis",  ParamType::Enum, 0.f, 0, false, "", 0, {"X","Y","Z"} },
            { "OverhangAngle",  ParamType::Float, 45.f, 0, false, "", 0, {}, {}, 0.f, 90.f, 1.f }
        }
    });

    registerType({
        "topo-passive-region", "Passive Region", "Topology", "PR", "Mark non-design region in mesh",
        hdrTopo,
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {{ "femesh", "FEMesh", PortDataType::FEMesh }},
        {
            { "RegionType",    ParamType::Enum,  0.f, 0, false, "", 0, {"Solid","Void"} },
            { "SelectionMode", ParamType::Enum,  0.f, 0, false, "", 0, {"Box","Sphere"} },
            { "CenterX",       ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "CenterY",       ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "CenterZ",       ParamType::Float, 0.0f, 0, false, "", 0, {}, {}, -1e6f, 1e6f, 0.1f },
            { "SizeX",         ParamType::Float, 1.0f, 0, false, "", 0, {}, {}, 0.f, 1e6f, 0.1f },
            { "SizeY",         ParamType::Float, 1.0f, 0, false, "", 0, {}, {}, 0.f, 1e6f, 0.1f },
            { "SizeZ",         ParamType::Float, 1.0f, 0, false, "", 0, {}, {}, 0.f, 1e6f, 0.1f },
            { "Radius",        ParamType::Float, 1.0f, 0, false, "", 0, {}, {}, 0.f, 1e6f, 0.1f }
        }
    });

    // ===================== PostProcess (4 nodes) =====================

    registerType({
        "post-density-view", "Density View", "PostProcess", "DV",
        "Extract iso-surface from density field",
        hdrPostProc,
        {
            { "density", "Density", PortDataType::DensityField },
            { "femesh",  "FEMesh",  PortDataType::FEMesh }
        },
        {{ "mesh", "Mesh", PortDataType::Mesh }},
        {
            { "Threshold",  ParamType::Float, 0.5f, 0, false, "", 0, {}, {}, 0.01f, 1.f, 0.01f },
            { "SmoothIter", ParamType::Int,   0.f, 0, false, "", 0, {}, {}, 0.f, 10.f, 1.f },
            { "Animate",    ParamType::Bool,  0.f, 0, false },
            { "AutoPlay",   ParamType::Bool,  0.f, 0, true },
            { "Loop",       ParamType::Bool,  0.f, 0, false },
            { "FPS",        ParamType::Float, 6.0f, 0, false, "", 0, {}, {}, 1.f, 60.f, 1.f },
            { "Frame",      ParamType::Int,   0.f, 0, false, "", 0, {}, {}, 0.f, 10000.f, 1.f }
        }
    });

    registerType({
        "post-extract-field", "Extract Field", "PostProcess", "EF",
        "Extract scalar field from FEA result",
        hdrPostProc,
        {{ "result", "Result", PortDataType::FEResult }},
        {{ "field", "Field", PortDataType::Field }},
        {
            { "FieldType", ParamType::Enum, 0.f, 0, false, "", 0,
              {"DispMagnitude","DispX","DispY","DispZ","VonMises","StrainEnergy"} }
        }
    });

    registerType({
        "post-convergence", "Convergence Plot", "PostProcess", "CP",
        "Display optimization convergence history",
        hdrPostProc,
        {{ "density", "Density", PortDataType::DensityField }},
        {},
        {
            { "ShowCompliance", ParamType::Bool, 0.f, 0, true },
            { "ShowVolume",     ParamType::Bool, 0.f, 0, true }
        }
    });

    registerType({
        "post-export", "Export Results", "PostProcess", "ER",
        "Export FEA/optimization results to file",
        hdrPostProc,
        {
            { "femesh",  "FEMesh",   PortDataType::FEMesh },
            { "density", "Density",  PortDataType::DensityField },
            { "result",  "Result",   PortDataType::FEResult }
        },
        {},
        {
            { "FilePath",         ParamType::String },
            { "Format",           ParamType::Enum, 0.f, 0, false, "", 0, {"VTK","STL"} },
            { "DensityThreshold", ParamType::Float, 0.5f, 0, false, "", 0, {}, {}, 0.01f, 1.f, 0.01f }
        }
    });
}

} // namespace TopOpt
