#pragma once
#include "../panels/View3DPanel.h"   // Triangle3D
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <sstream>
#include <cmath>

namespace TopOpt {

// ============================================================
//  Concrete data payloads (original)
// ============================================================

struct NumberData  { double value = 0.0; };
struct VectorData  { double x = 0, y = 0, z = 0; };
struct BooleanData { bool   value = false; };
struct StringData  { std::string value; };
struct MeshData    { std::vector<Triangle3D> triangles; };
struct FieldData   { std::vector<double> values; };
struct GenericData { std::string description; };

// ============================================================
//  FEA / Topology data payloads (new)
// ============================================================

struct FENode { int id; double x, y, z; };
struct FEElement { int id; std::vector<int> nodeIds; int type = 0; }; // 0=Hex8

struct FEMeshData {
    std::vector<FENode> nodes;
    std::vector<FEElement> elements;
    std::map<std::string, std::vector<int>> nodeSets;
    std::map<std::string, std::vector<int>> elementSets;
    std::vector<int> passiveSolid, passiveVoid; // non-design regions
};

struct MaterialData {
    double E = 210000.0;
    double nu = 0.3;
    double rho = 7850.0;
    int type = 0; // 0=isotropic
};

struct BCData {
    int type = 0; // 0=Fixed, 1=Displacement, 2=PointForce, 3=Pressure, 4=BodyForce
    std::vector<int> nodeIds;
    bool fixX = true, fixY = true, fixZ = true;
    double valX = 0, valY = 0, valZ = 0;
};

struct LoadCaseData {
    std::string name;
    std::vector<BCData> conditions;
    double weight = 1.0;
};

enum class SolverBackend {
    Auto = 0,
    CPU = 1,
    GPU_AmgX = 2
};

struct FESolverConfig {
    SolverBackend backend = SolverBackend::Auto;
    bool gpuEnabled = true;
    bool fallbackToCpu = true;
    int maxIterations = 1000;
    double tolerance = 1e-6;
    std::string amgxConfigPath;
};

struct FEResultData {
    std::vector<double> dispX, dispY, dispZ; // per node
    std::vector<double> vonMises, strainEnergy; // per element
    double compliance = 0;
    bool converged = false;
    std::string backendUsed = "cpu-direct";
    int iterationCount = 0;
    double solveTimeMs = 0.0;
    bool usedFallback = false;
    double residualNorm = 0.0;
    std::string solverMessage;
};

struct DensityFieldData {
    std::vector<double> densities; // per element [0,1]
    double volFrac = 0;
    double objective = 0;
    int iteration = 0;
    std::vector<double> history; // objective history
    std::vector<std::vector<double>> densityFrames; // per-iteration density snapshots
};

// ============================================================
//  NodeData - variant wrapper with convenience API
// ============================================================

using NodeDataVariant = std::variant<
    NumberData, VectorData, BooleanData, StringData,
    MeshData, FieldData, GenericData,
    FEMeshData, MaterialData, BCData, LoadCaseData,
    FEResultData, DensityFieldData
>;

class NodeData {
public:
    NodeData() : data_(GenericData{"empty"}) {}
    explicit NodeData(NodeDataVariant d) : data_(std::move(d)) {}

    // ---- Type checks ----
    bool isNumber()       const { return std::holds_alternative<NumberData>(data_); }
    bool isVector()       const { return std::holds_alternative<VectorData>(data_); }
    bool isBoolean()      const { return std::holds_alternative<BooleanData>(data_); }
    bool isString()       const { return std::holds_alternative<StringData>(data_); }
    bool isMesh()         const { return std::holds_alternative<MeshData>(data_); }
    bool isField()        const { return std::holds_alternative<FieldData>(data_); }
    bool isGeneric()      const { return std::holds_alternative<GenericData>(data_); }
    bool isFEMesh()       const { return std::holds_alternative<FEMeshData>(data_); }
    bool isMaterial()     const { return std::holds_alternative<MaterialData>(data_); }
    bool isBC()           const { return std::holds_alternative<BCData>(data_); }
    bool isLoadCase()     const { return std::holds_alternative<LoadCaseData>(data_); }
    bool isFEResult()     const { return std::holds_alternative<FEResultData>(data_); }
    bool isDensityField() const { return std::holds_alternative<DensityFieldData>(data_); }

    // ---- Accessors (const ref) ----
    double                         asNumber()       const { return std::get<NumberData>(data_).value; }
    const VectorData&              asVector()       const { return std::get<VectorData>(data_); }
    bool                           asBoolean()      const { return std::get<BooleanData>(data_).value; }
    const std::string&             asString()       const { return std::get<StringData>(data_).value; }
    const std::vector<Triangle3D>& asMesh()         const { return std::get<MeshData>(data_).triangles; }
    const std::vector<double>&     asField()        const { return std::get<FieldData>(data_).values; }
    const std::string&             asGeneric()      const { return std::get<GenericData>(data_).description; }
    const FEMeshData&              asFEMesh()       const { return std::get<FEMeshData>(data_); }
    const MaterialData&            asMaterial()     const { return std::get<MaterialData>(data_); }
    const BCData&                  asBC()           const { return std::get<BCData>(data_); }
    const LoadCaseData&            asLoadCase()     const { return std::get<LoadCaseData>(data_); }
    const FEResultData&            asFEResult()     const { return std::get<FEResultData>(data_); }
    const DensityFieldData&        asDensityField() const { return std::get<DensityFieldData>(data_); }

    // ---- Mutable accessors ----
    std::vector<Triangle3D>& meshRef()         { return std::get<MeshData>(data_).triangles; }
    std::vector<double>&     fieldRef()        { return std::get<FieldData>(data_).values; }
    FEMeshData&              feMeshRef()       { return std::get<FEMeshData>(data_); }
    FEResultData&            feResultRef()     { return std::get<FEResultData>(data_); }
    DensityFieldData&        densityFieldRef() { return std::get<DensityFieldData>(data_); }

    // ---- Factory helpers ----
    static NodeData makeNumber(double v)            { return NodeData(NumberData{v}); }
    static NodeData makeVector(double x, double y, double z) { return NodeData(VectorData{x,y,z}); }
    static NodeData makeBoolean(bool v)             { return NodeData(BooleanData{v}); }
    static NodeData makeString(const std::string& s){ return NodeData(StringData{s}); }
    static NodeData makeMesh(std::vector<Triangle3D> tris) { return NodeData(MeshData{std::move(tris)}); }
    static NodeData makeField(std::vector<double> vals)    { return NodeData(FieldData{std::move(vals)}); }
    static NodeData makeGeneric(const std::string& d)      { return NodeData(GenericData{d}); }
    static NodeData makeFEMesh(FEMeshData m)               { return NodeData(std::move(m)); }
    static NodeData makeMaterial(MaterialData m)            { return NodeData(std::move(m)); }
    static NodeData makeBC(BCData bc)                      { return NodeData(std::move(bc)); }
    static NodeData makeLoadCase(LoadCaseData lc)          { return NodeData(std::move(lc)); }
    static NodeData makeFEResult(FEResultData r)           { return NodeData(std::move(r)); }
    static NodeData makeDensityField(DensityFieldData d)   { return NodeData(std::move(d)); }

    // ---- Human-readable description for logging ----
    std::string describe() const {
        if (isNumber()) {
            std::ostringstream os; os << asNumber(); return "Number(" + os.str() + ")";
        }
        if (isVector()) {
            auto& v = asVector();
            std::ostringstream os;
            os << v.x << ", " << v.y << ", " << v.z;
            return "Vector(" + os.str() + ")";
        }
        if (isBoolean()) return asBoolean() ? "Boolean(true)" : "Boolean(false)";
        if (isString())  return "String(\"" + asString() + "\")";
        if (isMesh())    return "Mesh(" + std::to_string(asMesh().size()) + " triangles)";
        if (isField())   return "Field(" + std::to_string(asField().size()) + " values)";
        if (isGeneric()) return "Generic(" + asGeneric() + ")";
        if (isFEMesh()) {
            auto& m = asFEMesh();
            return "FEMesh(" + std::to_string(m.nodes.size()) + " nodes, " +
                   std::to_string(m.elements.size()) + " elements)";
        }
        if (isMaterial()) {
            auto& m = asMaterial();
            std::ostringstream os;
            os << "Material(E=" << m.E << ", nu=" << m.nu << ")";
            return os.str();
        }
        if (isBC()) {
            auto& bc = asBC();
            const char* names[] = {"Fixed","Disp","Force","Pressure","BodyForce"};
            return std::string("BC(") + (bc.type < 5 ? names[bc.type] : "?") +
                   ", " + std::to_string(bc.nodeIds.size()) + " nodes)";
        }
        if (isLoadCase()) {
            auto& lc = asLoadCase();
            return "LoadCase(" + lc.name + ", " +
                   std::to_string(lc.conditions.size()) + " BCs)";
        }
        if (isFEResult()) {
            auto& r = asFEResult();
            return "FEResult(compliance=" + std::to_string(r.compliance) +
                   ", converged=" + (r.converged ? "true" : "false") +
                   ", backend=" + r.backendUsed + ")";
        }
        if (isDensityField()) {
            auto& d = asDensityField();
            return "DensityField(iter=" + std::to_string(d.iteration) +
                   ", volFrac=" + std::to_string(d.volFrac) + ")";
        }
        return "Unknown";
    }

    // ---- Try to extract numeric value (for math nodes) ----
    double toNumber(double fallback = 0.0) const {
        if (isNumber())  return asNumber();
        if (isBoolean()) return asBoolean() ? 1.0 : 0.0;
        return fallback;
    }

private:
    NodeDataVariant data_;
};

} // namespace TopOpt
