#include "GraphExecutor.h"
#include "../node_editor/NodeEditor.h"
#include "../node_editor/NodeRegistry.h"
#include "../panels/View3DPanel.h"
#include "../utils/Logger.h"
#include "../fem/FEMSolver.h"
#include "../fem/TopOptSolver.h"

#include <cmath>
#include <algorithm>
#include <queue>
#include <set>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TopOpt {

GraphExecutor::GraphExecutor() {}
GraphExecutor::~GraphExecutor() {}

// ================================================================
//  Topological sort (Kahn's algorithm)
// ================================================================

bool GraphExecutor::buildExecutionOrder(std::vector<int>& order) {
    order.clear();
    if (!editor_) return false;

    auto& nodes = editor_->nodes();
    auto& conns = editor_->connections();

    std::set<int> nodeIds;
    for (auto& n : nodes) nodeIds.insert(n.id);

    std::map<int, int> inDegree;
    for (int id : nodeIds) inDegree[id] = 0;

    std::map<int, std::vector<int>> adj;
    for (auto& c : conns) {
        if (nodeIds.count(c.startNodeId) && nodeIds.count(c.endNodeId)) {
            adj[c.startNodeId].push_back(c.endNodeId);
            inDegree[c.endNodeId]++;
        }
    }

    std::queue<int> q;
    for (auto& [id, deg] : inDegree) {
        if (deg == 0) q.push(id);
    }

    while (!q.empty()) {
        int cur = q.front(); q.pop();
        order.push_back(cur);
        for (int next : adj[cur]) {
            inDegree[next]--;
            if (inDegree[next] == 0) q.push(next);
        }
    }

    if (order.size() != nodeIds.size()) {
        Logger::instance().error("Cycle detected in node graph! Cannot execute.");
        return false;
    }
    return true;
}

// ================================================================
//  Get input data by tracing connections upstream
// ================================================================

NodeData GraphExecutor::getInputData(int nodeId, int portIdx) {
    if (!editor_) return NodeData();

    for (auto& c : editor_->connections()) {
        if (c.endNodeId == nodeId && c.endPortIdx == portIdx) {
            OutputKey key{c.startNodeId, c.startPortIdx};
            auto it = outputData_.find(key);
            if (it != outputData_.end()) return it->second;
            break;
        }
    }
    return NodeData();
}

// ================================================================
//  Helper: find a parameter by name
// ================================================================

const ParamDef* GraphExecutor::findParam(int nodeId, const std::string& name) {
    auto* node = editor_->findNode(nodeId);
    if (!node) return nullptr;
    for (auto& p : node->params) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

// ================================================================
//  Evaluate a single node (dispatch by typeName)
// ================================================================

void GraphExecutor::evaluateNode(int nodeId) {
    auto* node = editor_->findNode(nodeId);
    if (!node) return;

    const std::string& t = node->typeName;

    // Input
    if      (t == "input-number")        evalInputNumber(nodeId);
    else if (t == "input-vector")        evalInputVector(nodeId);
    else if (t == "input-boolean")       evalInputBoolean(nodeId);
    else if (t == "input-file")          evalInputFile(nodeId);
    // Output
    else if (t == "output-display")      evalOutputDisplay(nodeId);
    else if (t == "output-export")       evalOutputExport(nodeId);
    else if (t == "output-viewer")       evalOutputViewer(nodeId);
    // Data
    else if (t == "data-mesh-gen")       evalDataMeshGen(nodeId);
    // Domain
    else if (t == "domain-box")          evalDomainBox(nodeId);
    else if (t == "domain-lshape")       evalDomainLShape(nodeId);
    else if (t == "domain-from-mesh")    evalDomainFromMesh(nodeId);
    else if (t == "domain-import")       evalDomainImport(nodeId);
    // FEA
    else if (t == "fea-material")        evalFEAMaterial(nodeId);
    else if (t == "fea-fixed-support")   evalFEAFixedSupport(nodeId);
    else if (t == "fea-displacement-bc") evalFEADisplacementBC(nodeId);
    else if (t == "fea-point-force")     evalFEAPointForce(nodeId);
    else if (t == "fea-pressure-load")   evalFEAPressureLoad(nodeId);
    else if (t == "fea-body-force")      evalFEABodyForce(nodeId);
    else if (t == "fea-load-case")       evalFEALoadCase(nodeId);
    else if (t == "fea-solver")          evalFEASolver(nodeId);
    // Topology
    else if (t == "topo-simp")           evalTopoSIMP(nodeId);
    else if (t == "topo-beso")           evalTopoBESO(nodeId);
    else if (t == "topo-constraint")     evalTopoConstraint(nodeId);
    else if (t == "topo-passive-region") evalTopoPassiveRegion(nodeId);
    // PostProcess
    else if (t == "post-density-view")   evalPostDensityView(nodeId);
    else if (t == "post-extract-field")  evalPostExtractField(nodeId);
    else if (t == "post-convergence")    evalPostConvergence(nodeId);
    else if (t == "post-export")         evalPostExport(nodeId);
    else {
        Logger::instance().warn("[Exec] Unknown node type: " + t);
    }
}

// ================================================================
//  runAll / stepOne / reset
// ================================================================

void GraphExecutor::runAll() {
    if (!editor_) return;

    outputData_.clear();
    stepIndex_ = 0;

    std::vector<int> order;
    if (!buildExecutionOrder(order)) return;

    Logger::instance().info("[Exec] Running all " + std::to_string(order.size()) + " nodes...");

    for (int id : order) {
        auto* node = editor_->findNode(id);
        std::string label = node ? node->label : ("Node#" + std::to_string(id));
        Logger::instance().info("[Exec] Evaluating: " + label + " (" +
                                (node ? node->typeName : "?") + ")");
        evaluateNode(id);
    }

    executionOrder_ = order;
    stepIndex_ = (int)order.size();
    Logger::instance().info("[Exec] Execution complete.");
}

void GraphExecutor::stepOne() {
    if (!editor_) return;

    if (executionOrder_.empty() || stepIndex_ >= (int)executionOrder_.size()) {
        outputData_.clear();
        stepIndex_ = 0;
        if (!buildExecutionOrder(executionOrder_)) return;
        Logger::instance().info("[Exec] Step mode: " + std::to_string(executionOrder_.size()) + " nodes queued.");
    }

    if (stepIndex_ < (int)executionOrder_.size()) {
        int id = executionOrder_[stepIndex_];
        auto* node = editor_->findNode(id);
        std::string label = node ? node->label : ("Node#" + std::to_string(id));
        Logger::instance().info("[Step " + std::to_string(stepIndex_ + 1) + "/" +
                                std::to_string(executionOrder_.size()) + "] " + label);
        evaluateNode(id);
        stepIndex_++;

        if (stepIndex_ >= (int)executionOrder_.size()) {
            Logger::instance().info("[Exec] All steps complete.");
        }
    }
}

void GraphExecutor::reset() {
    outputData_.clear();
    executionOrder_.clear();
    stepIndex_ = 0;
    Logger::instance().info("[Exec] Execution state reset.");
}

// ================================================================
//  INPUT NODES
// ================================================================

void GraphExecutor::evalInputNumber(int nodeId) {
    auto* p = findParam(nodeId, "Value");
    double val = p ? (double)p->floatVal : 0.0;
    outputData_[{nodeId, 0}] = NodeData::makeNumber(val);
}

void GraphExecutor::evalInputVector(int nodeId) {
    auto* px = findParam(nodeId, "X");
    auto* py = findParam(nodeId, "Y");
    auto* pz = findParam(nodeId, "Z");
    double x = px ? (double)px->floatVal : 0.0;
    double y = py ? (double)py->floatVal : 0.0;
    double z = pz ? (double)pz->floatVal : 0.0;
    outputData_[{nodeId, 0}] = NodeData::makeVector(x, y, z);
}

void GraphExecutor::evalInputBoolean(int nodeId) {
    auto* p = findParam(nodeId, "Value");
    bool val = p ? p->boolVal : false;
    outputData_[{nodeId, 0}] = NodeData::makeBoolean(val);
}

void GraphExecutor::evalInputFile(int nodeId) {
    auto* p = findParam(nodeId, "FilePath");
    std::string path = p ? p->stringVal : "";
    Logger::instance().info("[Exec] File input path: " + (path.empty() ? "(empty)" : path));
    outputData_[{nodeId, 0}] = NodeData::makeGeneric("file:" + path);
}

// ================================================================
//  OUTPUT NODES
// ================================================================

void GraphExecutor::evalOutputDisplay(int nodeId) {
    NodeData in = getInputData(nodeId, 0);
    auto* p = findParam(nodeId, "Title");
    std::string title = p ? p->stringVal : "Output";
    Logger::instance().info("[Display] " + title + ": " + in.describe());
}

void GraphExecutor::evalOutputExport(int nodeId) {
    auto* p = findParam(nodeId, "FilePath");
    std::string path = p ? p->stringVal : "";
    auto* pFmt = findParam(nodeId, "Format");
    std::string fmt = (pFmt && pFmt->enumIndex < (int)pFmt->enumOptions.size())
                      ? pFmt->enumOptions[pFmt->enumIndex] : "obj";
    Logger::instance().info("[Exec] Export to: " + (path.empty() ? "(no path)" : path) +
                            " format=" + fmt + " (placeholder)");
}

void GraphExecutor::evalOutputViewer(int nodeId) {
    NodeData inMesh = getInputData(nodeId, 0);
    if (inMesh.isMesh() && view3D_) {
        view3D_->setTriangles(inMesh.asMesh());
        Logger::instance().info("[Exec] 3D Viewer: displaying " +
                                std::to_string(inMesh.asMesh().size()) + " triangles");
    } else {
        Logger::instance().warn("[Exec] 3D Viewer: no mesh data on input");
    }
}

// ================================================================
//  DATA NODES
// ================================================================

static void computeTriNormal(Triangle3D& tri) {
    float e1[3] = { tri.v1[0]-tri.v0[0], tri.v1[1]-tri.v0[1], tri.v1[2]-tri.v0[2] };
    float e2[3] = { tri.v2[0]-tri.v0[0], tri.v2[1]-tri.v0[1], tri.v2[2]-tri.v0[2] };
    tri.normal[0] = e1[1]*e2[2] - e1[2]*e2[1];
    tri.normal[1] = e1[2]*e2[0] - e1[0]*e2[2];
    tri.normal[2] = e1[0]*e2[1] - e1[1]*e2[0];
    float len = std::sqrt(tri.normal[0]*tri.normal[0] + tri.normal[1]*tri.normal[1] + tri.normal[2]*tri.normal[2]);
    if (len > 1e-8f) { tri.normal[0]/=len; tri.normal[1]/=len; tri.normal[2]/=len; }
}

static void addQuad(std::vector<Triangle3D>& tris,
                    float ax, float ay, float az,
                    float bx, float by, float bz,
                    float cx, float cy, float cz,
                    float dx, float dy, float dz)
{
    Triangle3D t0, t1;
    t0.v0[0]=ax; t0.v0[1]=ay; t0.v0[2]=az;
    t0.v1[0]=bx; t0.v1[1]=by; t0.v1[2]=bz;
    t0.v2[0]=cx; t0.v2[1]=cy; t0.v2[2]=cz;
    computeTriNormal(t0);

    t1.v0[0]=ax; t1.v0[1]=ay; t1.v0[2]=az;
    t1.v1[0]=cx; t1.v1[1]=cy; t1.v1[2]=cz;
    t1.v2[0]=dx; t1.v2[1]=dy; t1.v2[2]=dz;
    computeTriNormal(t1);

    tris.push_back(t0);
    tris.push_back(t1);
}

void GraphExecutor::evalDataMeshGen(int nodeId) {
    auto* pShape = findParam(nodeId, "Shape");
    auto* pRes   = findParam(nodeId, "Resolution");
    int shapeIdx  = pShape ? pShape->enumIndex : 0;
    int resolution = pRes ? pRes->intVal : 10;
    if (resolution < 2) resolution = 2;

    std::vector<Triangle3D> tris;
    const char* shapeName = "?";
    switch (shapeIdx) {
        case 0: tris = generateBox(resolution);      shapeName = "Box"; break;
        case 1: tris = generateSphere(resolution);    shapeName = "Sphere"; break;
        case 2: tris = generateCylinder(resolution);  shapeName = "Cylinder"; break;
        case 3: tris = generatePlane(resolution);      shapeName = "Plane"; break;
        case 4: tris = generateSphere(resolution);     shapeName = "Torus(fallback:Sphere)"; break;
    }

    outputData_[{nodeId, 0}] = NodeData::makeMesh(std::move(tris));
    Logger::instance().info(std::string("[Exec] Mesh Generator: ") + shapeName +
                            " (" + std::to_string(outputData_[{nodeId, 0}].asMesh().size()) + " triangles)");
}

// ================================================================
//  DOMAIN NODES
// ================================================================

void GraphExecutor::evalDomainBox(int nodeId) {
    auto* pLX = findParam(nodeId, "LengthX");
    auto* pLY = findParam(nodeId, "LengthY");
    auto* pLZ = findParam(nodeId, "LengthZ");
    auto* pNX = findParam(nodeId, "ElemsX");
    auto* pNY = findParam(nodeId, "ElemsY");
    auto* pNZ = findParam(nodeId, "ElemsZ");

    double lx = pLX ? (double)pLX->floatVal : 10.0;
    double ly = pLY ? (double)pLY->floatVal : 5.0;
    double lz = pLZ ? (double)pLZ->floatVal : 1.0;
    int nx = pNX ? pNX->intVal : 20;
    int ny = pNY ? pNY->intVal : 10;
    int nz = pNZ ? pNZ->intVal : 1;
    if (nx < 1) nx = 1;
    if (ny < 1) ny = 1;
    if (nz < 1) nz = 1;

    FEMeshData mesh;

    // Generate nodes
    double dx = lx / nx;
    double dy = ly / ny;
    double dz = lz / nz;

    int nodeCount = (nx + 1) * (ny + 1) * (nz + 1);
    mesh.nodes.reserve(nodeCount);

    auto nodeIndex = [&](int ix, int iy, int iz) -> int {
        return iz * (nx + 1) * (ny + 1) + iy * (nx + 1) + ix;
    };

    for (int iz = 0; iz <= nz; iz++) {
        for (int iy = 0; iy <= ny; iy++) {
            for (int ix = 0; ix <= nx; ix++) {
                FENode nd;
                nd.id = nodeIndex(ix, iy, iz);
                nd.x = ix * dx;
                nd.y = iy * dy;
                nd.z = iz * dz;
                mesh.nodes.push_back(nd);
            }
        }
    }

    // Generate hex8 elements
    int elemCount = nx * ny * nz;
    mesh.elements.reserve(elemCount);
    int eid = 0;
    for (int iz = 0; iz < nz; iz++) {
        for (int iy = 0; iy < ny; iy++) {
            for (int ix = 0; ix < nx; ix++) {
                FEElement elem;
                elem.id = eid++;
                elem.type = 0; // Hex8
                elem.nodeIds = {
                    nodeIndex(ix,   iy,   iz),
                    nodeIndex(ix+1, iy,   iz),
                    nodeIndex(ix+1, iy+1, iz),
                    nodeIndex(ix,   iy+1, iz),
                    nodeIndex(ix,   iy,   iz+1),
                    nodeIndex(ix+1, iy,   iz+1),
                    nodeIndex(ix+1, iy+1, iz+1),
                    nodeIndex(ix,   iy+1, iz+1)
                };
                mesh.elements.push_back(elem);
            }
        }
    }

    Logger::instance().info("[Exec] Box Domain: " + std::to_string(mesh.nodes.size()) + " nodes, " +
                            std::to_string(mesh.elements.size()) + " hex8 elements");

    outputData_[{nodeId, 0}] = NodeData::makeFEMesh(std::move(mesh));
}

void GraphExecutor::evalDomainLShape(int nodeId) {
    auto* pW  = findParam(nodeId, "Width");
    auto* pH  = findParam(nodeId, "Height");
    auto* pD  = findParam(nodeId, "Depth");
    auto* pCF = findParam(nodeId, "CutFraction");
    auto* pES = findParam(nodeId, "ElemSize");

    double W  = pW  ? (double)pW->floatVal  : 10.0;
    double H  = pH  ? (double)pH->floatVal  : 10.0;
    double D  = pD  ? (double)pD->floatVal  : 1.0;
    double cf = pCF ? (double)pCF->floatVal : 0.5;
    double es = pES ? (double)pES->floatVal : 1.0;
    if (es < 0.01) es = 0.01;

    int nxTotal = std::max(1, (int)std::round(W / es));
    int nyTotal = std::max(1, (int)std::round(H / es));
    int nzTotal = std::max(1, (int)std::round(D / es));

    double dx = W / nxTotal;
    double dy = H / nyTotal;
    double dz = D / nzTotal;

    int nxCut = (int)(nxTotal * cf);
    int nyCut = (int)(nyTotal * cf);

    // Generate nodes for the full grid
    auto nodeIndex = [&](int ix, int iy, int iz) -> int {
        return iz * (nxTotal + 1) * (nyTotal + 1) + iy * (nxTotal + 1) + ix;
    };

    FEMeshData mesh;
    int totalNodes = (nxTotal + 1) * (nyTotal + 1) * (nzTotal + 1);
    mesh.nodes.resize(totalNodes);

    for (int iz = 0; iz <= nzTotal; iz++) {
        for (int iy = 0; iy <= nyTotal; iy++) {
            for (int ix = 0; ix <= nxTotal; ix++) {
                int idx = nodeIndex(ix, iy, iz);
                mesh.nodes[idx] = {idx, ix * dx, iy * dy, iz * dz};
            }
        }
    }

    // Generate elements, skipping the cut-out region (upper-right corner)
    int eid = 0;
    for (int iz = 0; iz < nzTotal; iz++) {
        for (int iy = 0; iy < nyTotal; iy++) {
            for (int ix = 0; ix < nxTotal; ix++) {
                // Skip if in cut region: ix >= (nxTotal - nxCut) && iy >= (nyTotal - nyCut)
                if (ix >= (nxTotal - nxCut) && iy >= (nyTotal - nyCut)) continue;

                FEElement elem;
                elem.id = eid++;
                elem.type = 0;
                elem.nodeIds = {
                    nodeIndex(ix,   iy,   iz),
                    nodeIndex(ix+1, iy,   iz),
                    nodeIndex(ix+1, iy+1, iz),
                    nodeIndex(ix,   iy+1, iz),
                    nodeIndex(ix,   iy,   iz+1),
                    nodeIndex(ix+1, iy,   iz+1),
                    nodeIndex(ix+1, iy+1, iz+1),
                    nodeIndex(ix,   iy+1, iz+1)
                };
                mesh.elements.push_back(elem);
            }
        }
    }

    Logger::instance().info("[Exec] L-Shape Domain: " + std::to_string(mesh.nodes.size()) + " nodes, " +
                            std::to_string(mesh.elements.size()) + " hex8 elements");

    outputData_[{nodeId, 0}] = NodeData::makeFEMesh(std::move(mesh));
}

void GraphExecutor::evalDomainFromMesh(int nodeId) {
    // Placeholder: converting surface mesh to volumetric FE mesh is complex
    // For now, output an empty FEMesh with a warning
    Logger::instance().warn("[Exec] Domain from Mesh: volumetric meshing not yet implemented (placeholder)");
    outputData_[{nodeId, 0}] = NodeData::makeFEMesh(FEMeshData{});
}

void GraphExecutor::evalDomainImport(int nodeId) {
    auto* pPath = findParam(nodeId, "FilePath");
    std::string path = pPath ? pPath->stringVal : "";
    Logger::instance().warn("[Exec] Import FE Mesh: file import not yet implemented (path=" + path + ")");
    outputData_[{nodeId, 0}] = NodeData::makeFEMesh(FEMeshData{});
}

// ================================================================
//  Node selection helper (for BC nodes)
// ================================================================

std::vector<int> GraphExecutor::selectNodes(const FEMeshData& mesh, int nodeId) {
    std::vector<int> selected;

    auto* pMode = findParam(nodeId, "SelectionMode");
    int mode = pMode ? pMode->enumIndex : 0;

    if (mode == 0) {
        // Face selection: X-, X+, Y-, Y+, Z-, Z+
        auto* pFace = findParam(nodeId, "Face");
        int face = pFace ? pFace->enumIndex : 0;
        auto* pTol = findParam(nodeId, "Tolerance");
        double tol = pTol ? (double)pTol->floatVal : 1e-6;

        // Find bounding box
        double minX = 1e30, maxX = -1e30;
        double minY = 1e30, maxY = -1e30;
        double minZ = 1e30, maxZ = -1e30;
        for (auto& n : mesh.nodes) {
            minX = std::min(minX, n.x); maxX = std::max(maxX, n.x);
            minY = std::min(minY, n.y); maxY = std::max(maxY, n.y);
            minZ = std::min(minZ, n.z); maxZ = std::max(maxZ, n.z);
        }

        for (auto& n : mesh.nodes) {
            bool match = false;
            switch (face) {
                case 0: match = std::abs(n.x - minX) < tol; break; // X-
                case 1: match = std::abs(n.x - maxX) < tol; break; // X+
                case 2: match = std::abs(n.y - minY) < tol; break; // Y-
                case 3: match = std::abs(n.y - maxY) < tol; break; // Y+
                case 4: match = std::abs(n.z - minZ) < tol; break; // Z-
                case 5: match = std::abs(n.z - maxZ) < tol; break; // Z+
            }
            if (match) selected.push_back(n.id);
        }
    } else {
        // Coordinate range selection
        auto* pAxis = findParam(nodeId, "CoordAxis");
        auto* pMin  = findParam(nodeId, "CoordMin");
        auto* pMax  = findParam(nodeId, "CoordMax");
        int axis = pAxis ? pAxis->enumIndex : 0;
        double cmin = pMin ? (double)pMin->floatVal : 0.0;
        double cmax = pMax ? (double)pMax->floatVal : 0.0;

        for (auto& n : mesh.nodes) {
            double val = (axis == 0) ? n.x : (axis == 1) ? n.y : n.z;
            if (val >= cmin && val <= cmax) {
                selected.push_back(n.id);
            }
        }
    }

    return selected;
}

// ================================================================
//  FEA NODES
// ================================================================

void GraphExecutor::evalFEAMaterial(int nodeId) {
    auto* pE   = findParam(nodeId, "E");
    auto* pNu  = findParam(nodeId, "nu");
    auto* pRho = findParam(nodeId, "Density");

    MaterialData mat;
    mat.E   = pE   ? (double)pE->floatVal   : 210000.0;
    mat.nu  = pNu  ? (double)pNu->floatVal  : 0.3;
    mat.rho = pRho ? (double)pRho->floatVal : 7850.0;

    outputData_[{nodeId, 0}] = NodeData::makeMaterial(mat);
    Logger::instance().info("[Exec] Material: E=" + std::to_string(mat.E) +
                            " nu=" + std::to_string(mat.nu) +
                            " rho=" + std::to_string(mat.rho));
}

void GraphExecutor::evalFEAFixedSupport(int nodeId) {
    NodeData inMesh = getInputData(nodeId, 0);
    if (!inMesh.isFEMesh()) {
        Logger::instance().warn("[Exec] Fixed Support: no FEMesh input");
        outputData_[{nodeId, 0}] = NodeData::makeBC(BCData{});
        return;
    }

    auto& mesh = inMesh.asFEMesh();
    auto selected = selectNodes(mesh, nodeId);

    auto* pFX = findParam(nodeId, "FixX");
    auto* pFY = findParam(nodeId, "FixY");
    auto* pFZ = findParam(nodeId, "FixZ");

    BCData bc;
    bc.type = 0; // Fixed
    bc.nodeIds = selected;
    bc.fixX = pFX ? pFX->boolVal : true;
    bc.fixY = pFY ? pFY->boolVal : true;
    bc.fixZ = pFZ ? pFZ->boolVal : true;

    outputData_[{nodeId, 0}] = NodeData::makeBC(bc);
    Logger::instance().info("[Exec] Fixed Support: " + std::to_string(selected.size()) + " nodes selected");
}

void GraphExecutor::evalFEADisplacementBC(int nodeId) {
    NodeData inMesh = getInputData(nodeId, 0);
    if (!inMesh.isFEMesh()) {
        Logger::instance().warn("[Exec] Displacement BC: no FEMesh input");
        outputData_[{nodeId, 0}] = NodeData::makeBC(BCData{});
        return;
    }

    auto& mesh = inMesh.asFEMesh();
    auto selected = selectNodes(mesh, nodeId);

    auto* pDX = findParam(nodeId, "DispX");
    auto* pDY = findParam(nodeId, "DispY");
    auto* pDZ = findParam(nodeId, "DispZ");
    auto* pAX = findParam(nodeId, "ApplyX");
    auto* pAY = findParam(nodeId, "ApplyY");
    auto* pAZ = findParam(nodeId, "ApplyZ");

    BCData bc;
    bc.type = 1; // Displacement
    bc.nodeIds = selected;
    bc.fixX = pAX ? pAX->boolVal : true;
    bc.fixY = pAY ? pAY->boolVal : true;
    bc.fixZ = pAZ ? pAZ->boolVal : true;
    bc.valX = pDX ? (double)pDX->floatVal : 0.0;
    bc.valY = pDY ? (double)pDY->floatVal : 0.0;
    bc.valZ = pDZ ? (double)pDZ->floatVal : 0.0;

    outputData_[{nodeId, 0}] = NodeData::makeBC(bc);
    Logger::instance().info("[Exec] Displacement BC: " + std::to_string(selected.size()) + " nodes");
}

void GraphExecutor::evalFEAPointForce(int nodeId) {
    NodeData inMesh = getInputData(nodeId, 0);
    if (!inMesh.isFEMesh()) {
        Logger::instance().warn("[Exec] Point Force: no FEMesh input");
        outputData_[{nodeId, 0}] = NodeData::makeBC(BCData{});
        return;
    }

    auto& mesh = inMesh.asFEMesh();
    auto selected = selectNodes(mesh, nodeId);

    auto* pFX = findParam(nodeId, "ForceX");
    auto* pFY = findParam(nodeId, "ForceY");
    auto* pFZ = findParam(nodeId, "ForceZ");
    auto* pDist = findParam(nodeId, "Distribution");

    double fx = pFX ? (double)pFX->floatVal : 0.0;
    double fy = pFY ? (double)pFY->floatVal : -1.0;
    double fz = pFZ ? (double)pFZ->floatVal : 0.0;
    int distMode = pDist ? pDist->enumIndex : 0;

    // If "Total", distribute force evenly across selected nodes
    if (distMode == 0 && !selected.empty()) {
        int n = (int)selected.size();
        fx /= n;
        fy /= n;
        fz /= n;
    }

    BCData bc;
    bc.type = 2; // Point force
    bc.nodeIds = selected;
    bc.valX = fx;
    bc.valY = fy;
    bc.valZ = fz;

    outputData_[{nodeId, 0}] = NodeData::makeBC(bc);
    Logger::instance().info("[Exec] Point Force: " + std::to_string(selected.size()) +
                            " nodes, F=(" + std::to_string(fx) + "," +
                            std::to_string(fy) + "," + std::to_string(fz) + ")");
}

void GraphExecutor::evalFEAPressureLoad(int nodeId) {
    NodeData inMesh = getInputData(nodeId, 0);
    if (!inMesh.isFEMesh()) {
        Logger::instance().warn("[Exec] Pressure Load: no FEMesh input");
        outputData_[{nodeId, 0}] = NodeData::makeBC(BCData{});
        return;
    }

    auto& mesh = inMesh.asFEMesh();

    auto* pFace = findParam(nodeId, "Face");
    auto* pPres = findParam(nodeId, "Pressure");
    auto* pDir  = findParam(nodeId, "Direction");

    int face = pFace ? pFace->enumIndex : 0;
    double pressure = pPres ? (double)pPres->floatVal : 1.0;
    int dirMode = pDir ? pDir->enumIndex : 0;

    // Find face nodes (reuse face logic)
    double minX = 1e30, maxX = -1e30;
    double minY = 1e30, maxY = -1e30;
    double minZ = 1e30, maxZ = -1e30;
    for (auto& n : mesh.nodes) {
        minX = std::min(minX, n.x); maxX = std::max(maxX, n.x);
        minY = std::min(minY, n.y); maxY = std::max(maxY, n.y);
        minZ = std::min(minZ, n.z); maxZ = std::max(maxZ, n.z);
    }

    double tol = 1e-6;
    std::vector<int> selected;
    for (auto& n : mesh.nodes) {
        bool match = false;
        switch (face) {
            case 0: match = std::abs(n.x - minX) < tol; break;
            case 1: match = std::abs(n.x - maxX) < tol; break;
            case 2: match = std::abs(n.y - minY) < tol; break;
            case 3: match = std::abs(n.y - maxY) < tol; break;
            case 4: match = std::abs(n.z - minZ) < tol; break;
            case 5: match = std::abs(n.z - maxZ) < tol; break;
        }
        if (match) selected.push_back(n.id);
    }

    // Convert pressure to forces based on direction
    double fx = 0, fy = 0, fz = 0;
    if (dirMode == 0) {
        // Normal direction
        switch (face) {
            case 0: fx = -pressure; break; // X- face, normal is -X
            case 1: fx =  pressure; break;
            case 2: fy = -pressure; break;
            case 3: fy =  pressure; break;
            case 4: fz = -pressure; break;
            case 5: fz =  pressure; break;
        }
    } else {
        // Custom direction
        if (dirMode == 1) fx = pressure;
        else if (dirMode == 2) fy = pressure;
        else if (dirMode == 3) fz = pressure;
    }

    // Distribute among nodes
    if (!selected.empty()) {
        int n = (int)selected.size();
        fx /= n; fy /= n; fz /= n;
    }

    BCData bc;
    bc.type = 3; // Pressure
    bc.nodeIds = selected;
    bc.valX = fx;
    bc.valY = fy;
    bc.valZ = fz;

    outputData_[{nodeId, 0}] = NodeData::makeBC(bc);
    Logger::instance().info("[Exec] Pressure Load: " + std::to_string(selected.size()) + " nodes, P=" +
                            std::to_string(pressure));
}

void GraphExecutor::evalFEABodyForce(int nodeId) {
    NodeData inMesh = getInputData(nodeId, 0);
    if (!inMesh.isFEMesh()) {
        Logger::instance().warn("[Exec] Body Force: no FEMesh input");
        outputData_[{nodeId, 0}] = NodeData::makeBC(BCData{});
        return;
    }

    auto& mesh = inMesh.asFEMesh();
    auto* pGX = findParam(nodeId, "GravityX");
    auto* pGY = findParam(nodeId, "GravityY");
    auto* pGZ = findParam(nodeId, "GravityZ");

    double gx = pGX ? (double)pGX->floatVal : 0.0;
    double gy = pGY ? (double)pGY->floatVal : -9810.0;
    double gz = pGZ ? (double)pGZ->floatVal : 0.0;

    // Apply to all nodes
    std::vector<int> allNodes;
    allNodes.reserve(mesh.nodes.size());
    for (auto& n : mesh.nodes) allNodes.push_back(n.id);

    BCData bc;
    bc.type = 4; // Body force
    bc.nodeIds = allNodes;
    bc.valX = gx;
    bc.valY = gy;
    bc.valZ = gz;

    outputData_[{nodeId, 0}] = NodeData::makeBC(bc);
    Logger::instance().info("[Exec] Body Force: gravity=(" + std::to_string(gx) + "," +
                            std::to_string(gy) + "," + std::to_string(gz) + ")");
}

void GraphExecutor::evalFEALoadCase(int nodeId) {
    auto* pName   = findParam(nodeId, "Name");
    auto* pWeight = findParam(nodeId, "Weight");

    LoadCaseData lc;
    lc.name   = pName   ? pName->stringVal   : "LC1";
    lc.weight = pWeight ? (double)pWeight->floatVal : 1.0;

    // Collect BCs from up to 4 input ports
    for (int i = 0; i < 4; i++) {
        NodeData in = getInputData(nodeId, i);
        if (in.isBC()) {
            lc.conditions.push_back(in.asBC());
        }
    }

    outputData_[{nodeId, 0}] = NodeData::makeLoadCase(lc);
    Logger::instance().info("[Exec] Load Case '" + lc.name + "': " +
                            std::to_string(lc.conditions.size()) + " BCs, weight=" +
                            std::to_string(lc.weight));
}

void GraphExecutor::evalFEASolver(int nodeId) {
    NodeData inMesh = getInputData(nodeId, 0);
    NodeData inMat  = getInputData(nodeId, 1);

    if (!inMesh.isFEMesh()) {
        Logger::instance().error("[Exec] FEA Solver: no FEMesh input");
        outputData_[{nodeId, 0}] = NodeData::makeFEResult(FEResultData{});
        outputData_[{nodeId, 1}] = NodeData::makeFEMesh(FEMeshData{});
        return;
    }
    if (!inMat.isMaterial()) {
        Logger::instance().error("[Exec] FEA Solver: no Material input");
        outputData_[{nodeId, 0}] = NodeData::makeFEResult(FEResultData{});
        outputData_[{nodeId, 1}] = inMesh;
        return;
    }

    // Collect load cases from up to 3 ports (ports 2,3,4)
    std::vector<LoadCaseData> loadCases;
    for (int i = 2; i <= 4; i++) {
        NodeData in = getInputData(nodeId, i);
        if (in.isLoadCase()) {
            loadCases.push_back(in.asLoadCase());
        }
    }

    if (loadCases.empty()) {
        Logger::instance().error("[Exec] FEA Solver: no LoadCase input");
        outputData_[{nodeId, 0}] = NodeData::makeFEResult(FEResultData{});
        outputData_[{nodeId, 1}] = inMesh;
        return;
    }

    auto& mesh = inMesh.asFEMesh();
    auto& mat  = inMat.asMaterial();

    Logger::instance().info("[Exec] FEA Solver: " + std::to_string(mesh.nodes.size()) + " nodes, " +
                            std::to_string(mesh.elements.size()) + " elements, " +
                            std::to_string(loadCases.size()) + " load case(s)");

    // Solve using first load case (multi-load case would need multiple solves)
    FEMSolver solver;
    solver.setMesh(mesh);
    solver.setMaterial(mat);
    solver.setLoadCase(loadCases[0]);

    bool ok = solver.solve();
    if (ok) {
        Logger::instance().info("[Exec] FEA Solver: converged, compliance=" +
                                std::to_string(solver.result().compliance));
    } else {
        Logger::instance().error("[Exec] FEA Solver: failed to converge");
    }

    outputData_[{nodeId, 0}] = NodeData::makeFEResult(solver.result());
    outputData_[{nodeId, 1}] = inMesh;
}

// ================================================================
//  TOPOLOGY NODES
// ================================================================

void GraphExecutor::evalTopoSIMP(int nodeId) {
    NodeData inMesh = getInputData(nodeId, 0);
    NodeData inMat  = getInputData(nodeId, 1);

    if (!inMesh.isFEMesh() || !inMat.isMaterial()) {
        Logger::instance().error("[Exec] SIMP: missing FEMesh or Material input");
        outputData_[{nodeId, 0}] = NodeData::makeDensityField(DensityFieldData{});
        outputData_[{nodeId, 1}] = NodeData::makeFEResult(FEResultData{});
        return;
    }

    // Collect load cases from ports 2,3,4
    std::vector<LoadCaseData> loadCases;
    for (int i = 2; i <= 4; i++) {
        NodeData in = getInputData(nodeId, i);
        if (in.isLoadCase()) loadCases.push_back(in.asLoadCase());
    }

    if (loadCases.empty()) {
        Logger::instance().error("[Exec] SIMP: no LoadCase input");
        outputData_[{nodeId, 0}] = NodeData::makeDensityField(DensityFieldData{});
        outputData_[{nodeId, 1}] = NodeData::makeFEResult(FEResultData{});
        return;
    }

    auto* pVF  = findParam(nodeId, "VolFrac");
    auto* pP   = findParam(nodeId, "Penalty");
    auto* pFR  = findParam(nodeId, "FilterR");
    auto* pFT  = findParam(nodeId, "FilterType");
    auto* pMI  = findParam(nodeId, "MaxIter");
    auto* pMD  = findParam(nodeId, "MinDensity");

    TopOptSolver opt;
    opt.volFrac      = pVF ? (double)pVF->floatVal : 0.5;
    opt.penalty      = pP  ? (double)pP->floatVal  : 3.0;
    opt.filterRadius = pFR ? (double)pFR->floatVal : 1.5;
    opt.filterType   = pFT ? pFT->enumIndex : 0;
    opt.maxIter      = pMI ? pMI->intVal : 200;
    opt.minDensity   = pMD ? (double)pMD->floatVal : 0.001;

    opt.setMesh(inMesh.asFEMesh());
    opt.setMaterial(inMat.asMaterial());
    opt.setLoadCases(loadCases);

    Logger::instance().info("[Exec] SIMP: starting optimization (volFrac=" +
                            std::to_string(opt.volFrac) + ", penalty=" +
                            std::to_string(opt.penalty) + ", maxIter=" +
                            std::to_string(opt.maxIter) + ")...");

    bool ok = opt.runSIMP();

    if (ok) {
        auto& dr = opt.densityResult();
        Logger::instance().info("[Exec] SIMP: completed in " + std::to_string(dr.iteration) +
                                " iterations, final volFrac=" + std::to_string(dr.volFrac) +
                                ", objective=" + std::to_string(dr.objective));
    } else {
        Logger::instance().error("[Exec] SIMP: optimization failed");
    }

    outputData_[{nodeId, 0}] = NodeData::makeDensityField(opt.densityResult());
    outputData_[{nodeId, 1}] = NodeData::makeFEResult(opt.feResult());
}

void GraphExecutor::evalTopoBESO(int nodeId) {
    // BESO: simplified - use SIMP as placeholder with different parameters
    Logger::instance().warn("[Exec] BESO: using SIMP as fallback (BESO not yet fully implemented)");

    NodeData inMesh = getInputData(nodeId, 0);
    NodeData inMat  = getInputData(nodeId, 1);

    if (!inMesh.isFEMesh() || !inMat.isMaterial()) {
        Logger::instance().error("[Exec] BESO: missing FEMesh or Material input");
        outputData_[{nodeId, 0}] = NodeData::makeDensityField(DensityFieldData{});
        outputData_[{nodeId, 1}] = NodeData::makeFEResult(FEResultData{});
        return;
    }

    std::vector<LoadCaseData> loadCases;
    for (int i = 2; i <= 3; i++) {
        NodeData in = getInputData(nodeId, i);
        if (in.isLoadCase()) loadCases.push_back(in.asLoadCase());
    }

    if (loadCases.empty()) {
        Logger::instance().error("[Exec] BESO: no LoadCase input");
        outputData_[{nodeId, 0}] = NodeData::makeDensityField(DensityFieldData{});
        outputData_[{nodeId, 1}] = NodeData::makeFEResult(FEResultData{});
        return;
    }

    auto* pVF = findParam(nodeId, "VolFrac");
    auto* pFR = findParam(nodeId, "FilterR");
    auto* pMI = findParam(nodeId, "MaxIter");

    TopOptSolver opt;
    opt.volFrac      = pVF ? (double)pVF->floatVal : 0.5;
    opt.filterRadius = pFR ? (double)pFR->floatVal : 1.5;
    opt.maxIter      = pMI ? pMI->intVal : 200;
    opt.penalty      = 3.0;
    opt.filterType   = 1; // sensitivity filter

    opt.setMesh(inMesh.asFEMesh());
    opt.setMaterial(inMat.asMaterial());
    opt.setLoadCases(loadCases);

    Logger::instance().info("[Exec] BESO (SIMP fallback): starting...");
    bool ok = opt.runSIMP();

    if (ok) {
        Logger::instance().info("[Exec] BESO: completed, " +
                                std::to_string(opt.densityResult().iteration) + " iterations");
    } else {
        Logger::instance().error("[Exec] BESO: failed");
    }

    outputData_[{nodeId, 0}] = NodeData::makeDensityField(opt.densityResult());
    outputData_[{nodeId, 1}] = NodeData::makeFEResult(opt.feResult());
}

void GraphExecutor::evalTopoConstraint(int nodeId) {
    NodeData inDensity = getInputData(nodeId, 0);
    if (!inDensity.isDensityField()) {
        Logger::instance().warn("[Exec] Design Constraint: no density field input");
        outputData_[{nodeId, 0}] = NodeData::makeDensityField(DensityFieldData{});
        return;
    }

    // Pass through for now - constraint application is a placeholder
    auto* pType = findParam(nodeId, "ConstraintType");
    int ctype = pType ? pType->enumIndex : 0;
    const char* ctypeNames[] = {"Symmetry","MinSize","Extrusion","Overhang"};

    Logger::instance().info("[Exec] Design Constraint: " +
                            std::string(ctype < 4 ? ctypeNames[ctype] : "?") +
                            " (placeholder, passing through)");

    outputData_[{nodeId, 0}] = inDensity;
}

void GraphExecutor::evalTopoPassiveRegion(int nodeId) {
    NodeData inMesh = getInputData(nodeId, 0);
    if (!inMesh.isFEMesh()) {
        Logger::instance().warn("[Exec] Passive Region: no FEMesh input");
        outputData_[{nodeId, 0}] = NodeData::makeFEMesh(FEMeshData{});
        return;
    }

    FEMeshData mesh = inMesh.asFEMesh(); // copy

    auto* pRegion = findParam(nodeId, "RegionType");
    auto* pSelMode = findParam(nodeId, "SelectionMode");
    auto* pCX = findParam(nodeId, "CenterX");
    auto* pCY = findParam(nodeId, "CenterY");
    auto* pCZ = findParam(nodeId, "CenterZ");

    int regionType = pRegion ? pRegion->enumIndex : 0; // 0=Solid, 1=Void
    int selMode = pSelMode ? pSelMode->enumIndex : 0;  // 0=Box, 1=Sphere

    double cx = pCX ? (double)pCX->floatVal : 0.0;
    double cy = pCY ? (double)pCY->floatVal : 0.0;
    double cz = pCZ ? (double)pCZ->floatVal : 0.0;

    std::vector<int> selectedElems;

    if (selMode == 0) {
        // Box selection
        auto* pSX = findParam(nodeId, "SizeX");
        auto* pSY = findParam(nodeId, "SizeY");
        auto* pSZ = findParam(nodeId, "SizeZ");
        double sx = pSX ? (double)pSX->floatVal : 1.0;
        double sy = pSY ? (double)pSY->floatVal : 1.0;
        double sz = pSZ ? (double)pSZ->floatVal : 1.0;

        for (int e = 0; e < (int)mesh.elements.size(); e++) {
            // Compute element center
            double ecx = 0, ecy = 0, ecz = 0;
            int nn = (int)mesh.elements[e].nodeIds.size();
            for (int nid : mesh.elements[e].nodeIds) {
                ecx += mesh.nodes[nid].x;
                ecy += mesh.nodes[nid].y;
                ecz += mesh.nodes[nid].z;
            }
            ecx /= nn; ecy /= nn; ecz /= nn;

            if (std::abs(ecx - cx) <= sx / 2.0 &&
                std::abs(ecy - cy) <= sy / 2.0 &&
                std::abs(ecz - cz) <= sz / 2.0) {
                selectedElems.push_back(e);
            }
        }
    } else {
        // Sphere selection
        auto* pR = findParam(nodeId, "Radius");
        double radius = pR ? (double)pR->floatVal : 1.0;

        for (int e = 0; e < (int)mesh.elements.size(); e++) {
            double ecx = 0, ecy = 0, ecz = 0;
            int nn = (int)mesh.elements[e].nodeIds.size();
            for (int nid : mesh.elements[e].nodeIds) {
                ecx += mesh.nodes[nid].x;
                ecy += mesh.nodes[nid].y;
                ecz += mesh.nodes[nid].z;
            }
            ecx /= nn; ecy /= nn; ecz /= nn;

            double dist = std::sqrt((ecx-cx)*(ecx-cx) + (ecy-cy)*(ecy-cy) + (ecz-cz)*(ecz-cz));
            if (dist <= radius) {
                selectedElems.push_back(e);
            }
        }
    }

    if (regionType == 0) {
        mesh.passiveSolid.insert(mesh.passiveSolid.end(), selectedElems.begin(), selectedElems.end());
    } else {
        mesh.passiveVoid.insert(mesh.passiveVoid.end(), selectedElems.begin(), selectedElems.end());
    }

    Logger::instance().info("[Exec] Passive Region: " + std::to_string(selectedElems.size()) +
                            " elements marked as " + (regionType == 0 ? "solid" : "void"));

    outputData_[{nodeId, 0}] = NodeData::makeFEMesh(std::move(mesh));
}

// ================================================================
//  POSTPROCESS NODES
// ================================================================

void GraphExecutor::evalPostDensityView(int nodeId) {
    NodeData inDensity = getInputData(nodeId, 0);
    NodeData inMesh    = getInputData(nodeId, 1);

    if (!inDensity.isDensityField() || !inMesh.isFEMesh()) {
        Logger::instance().warn("[Exec] Density View: missing inputs");
        outputData_[{nodeId, 0}] = NodeData::makeMesh({});
        return;
    }

    auto& density = inDensity.asDensityField();
    auto& mesh    = inMesh.asFEMesh();

    auto* pThresh = findParam(nodeId, "Threshold");
    double threshold = pThresh ? (double)pThresh->floatVal : 0.5;

    // Use boundary-aware rendering
    auto tris = feMeshDensityToTriangles(mesh, density, threshold);

    Logger::instance().info("[Exec] Density View: " + std::to_string(tris.size()) +
                            " triangles (threshold=" + std::to_string(threshold) + ")");

    outputData_[{nodeId, 0}] = NodeData::makeMesh(std::move(tris));
}

void GraphExecutor::evalPostExtractField(int nodeId) {
    NodeData inResult = getInputData(nodeId, 0);
    if (!inResult.isFEResult()) {
        Logger::instance().warn("[Exec] Extract Field: no FEResult input");
        outputData_[{nodeId, 0}] = NodeData::makeField({});
        return;
    }

    auto& result = inResult.asFEResult();
    auto* pType = findParam(nodeId, "FieldType");
    int fieldType = pType ? pType->enumIndex : 0;

    std::vector<double> field;
    const char* fieldName = "?";

    switch (fieldType) {
        case 0: { // DispMagnitude
            fieldName = "DispMagnitude";
            int n = (int)result.dispX.size();
            field.resize(n);
            for (int i = 0; i < n; i++) {
                field[i] = std::sqrt(result.dispX[i]*result.dispX[i] +
                                     result.dispY[i]*result.dispY[i] +
                                     result.dispZ[i]*result.dispZ[i]);
            }
            break;
        }
        case 1: field = result.dispX; fieldName = "DispX"; break;
        case 2: field = result.dispY; fieldName = "DispY"; break;
        case 3: field = result.dispZ; fieldName = "DispZ"; break;
        case 4: field = result.vonMises; fieldName = "VonMises"; break;
        case 5: field = result.strainEnergy; fieldName = "StrainEnergy"; break;
    }

    Logger::instance().info(std::string("[Exec] Extract Field: ") + fieldName +
                            " (" + std::to_string(field.size()) + " values)");

    outputData_[{nodeId, 0}] = NodeData::makeField(std::move(field));
}

void GraphExecutor::evalPostConvergence(int nodeId) {
    NodeData inDensity = getInputData(nodeId, 0);
    if (!inDensity.isDensityField()) {
        Logger::instance().warn("[Exec] Convergence: no density field input");
        return;
    }

    auto& density = inDensity.asDensityField();
    Logger::instance().info("[Exec] Convergence Plot: " +
                            std::to_string(density.history.size()) + " iterations recorded");

    // Log convergence history
    if (!density.history.empty()) {
        Logger::instance().info("[Exec]   Initial objective: " + std::to_string(density.history.front()));
        Logger::instance().info("[Exec]   Final objective:   " + std::to_string(density.history.back()));
        Logger::instance().info("[Exec]   Final volFrac:     " + std::to_string(density.volFrac));
    }
}

void GraphExecutor::evalPostExport(int nodeId) {
    auto* pPath = findParam(nodeId, "FilePath");
    auto* pFmt  = findParam(nodeId, "Format");
    auto* pThresh = findParam(nodeId, "DensityThreshold");

    std::string path = pPath ? pPath->stringVal : "";
    int fmtIdx = pFmt ? pFmt->enumIndex : 0;
    const char* fmtNames[] = {"VTK", "STL"};
    double threshold = pThresh ? (double)pThresh->floatVal : 0.5;

    Logger::instance().info("[Exec] Export Results: format=" +
                            std::string(fmtIdx < 2 ? fmtNames[fmtIdx] : "?") +
                            " path=" + (path.empty() ? "(not set)" : path) +
                            " threshold=" + std::to_string(threshold) +
                            " (placeholder - file I/O not yet implemented)");
}

// ================================================================
//  MESH GENERATION HELPERS (for data-mesh-gen)
// ================================================================

std::vector<Triangle3D> GraphExecutor::generateBox(int /*resolution*/) {
    std::vector<Triangle3D> tris;
    float s = 0.5f;

    addQuad(tris, -s,-s, s,  s,-s, s,  s, s, s, -s, s, s);
    addQuad(tris,  s,-s,-s, -s,-s,-s, -s, s,-s,  s, s,-s);
    addQuad(tris,  s,-s, s,  s,-s,-s,  s, s,-s,  s, s, s);
    addQuad(tris, -s,-s,-s, -s,-s, s, -s, s, s, -s, s,-s);
    addQuad(tris, -s, s, s,  s, s, s,  s, s,-s, -s, s,-s);
    addQuad(tris, -s,-s,-s,  s,-s,-s,  s,-s, s, -s,-s, s);

    return tris;
}

std::vector<Triangle3D> GraphExecutor::generateSphere(int resolution) {
    std::vector<Triangle3D> tris;
    int stacks = resolution;
    int slices = resolution * 2;
    float r = 0.5f;

    auto sphereVert = [&](int stack, int slice, float* out) {
        float phi   = (float)M_PI * stack / stacks;
        float theta = 2.0f * (float)M_PI * slice / slices;
        out[0] = r * std::sin(phi) * std::cos(theta);
        out[1] = r * std::cos(phi);
        out[2] = r * std::sin(phi) * std::sin(theta);
    };

    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            float p00[3], p10[3], p11[3], p01[3];
            sphereVert(i,   j,   p00);
            sphereVert(i+1, j,   p10);
            sphereVert(i+1, j+1, p11);
            sphereVert(i,   j+1, p01);

            if (i != 0) {
                Triangle3D t;
                std::copy(p00, p00+3, t.v0);
                std::copy(p10, p10+3, t.v1);
                std::copy(p11, p11+3, t.v2);
                computeTriNormal(t);
                tris.push_back(t);
            }
            if (i != stacks - 1) {
                Triangle3D t;
                std::copy(p00, p00+3, t.v0);
                std::copy(p11, p11+3, t.v1);
                std::copy(p01, p01+3, t.v2);
                computeTriNormal(t);
                tris.push_back(t);
            }
        }
    }
    return tris;
}

std::vector<Triangle3D> GraphExecutor::generateCylinder(int resolution) {
    std::vector<Triangle3D> tris;
    int slices = resolution * 2;
    float r = 0.5f;
    float h = 1.0f;
    float halfH = h * 0.5f;

    for (int j = 0; j < slices; j++) {
        float theta0 = 2.0f * (float)M_PI * j / slices;
        float theta1 = 2.0f * (float)M_PI * (j + 1) / slices;

        float x0 = r * std::cos(theta0), z0 = r * std::sin(theta0);
        float x1 = r * std::cos(theta1), z1 = r * std::sin(theta1);

        addQuad(tris,
                x0, -halfH, z0,
                x1, -halfH, z1,
                x1,  halfH, z1,
                x0,  halfH, z0);

        Triangle3D tt;
        tt.v0[0]=0; tt.v0[1]=halfH; tt.v0[2]=0;
        tt.v1[0]=x0; tt.v1[1]=halfH; tt.v1[2]=z0;
        tt.v2[0]=x1; tt.v2[1]=halfH; tt.v2[2]=z1;
        computeTriNormal(tt);
        tris.push_back(tt);

        Triangle3D tb;
        tb.v0[0]=0; tb.v0[1]=-halfH; tb.v0[2]=0;
        tb.v1[0]=x1; tb.v1[1]=-halfH; tb.v1[2]=z1;
        tb.v2[0]=x0; tb.v2[1]=-halfH; tb.v2[2]=z0;
        computeTriNormal(tb);
        tris.push_back(tb);
    }
    return tris;
}

std::vector<Triangle3D> GraphExecutor::generatePlane(int resolution) {
    std::vector<Triangle3D> tris;
    int n = resolution;
    float size = 1.0f;
    float half = size * 0.5f;
    float step = size / n;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float x0 = -half + i * step;
            float z0 = -half + j * step;
            float x1 = x0 + step;
            float z1 = z0 + step;

            addQuad(tris,
                    x0, 0, z0,
                    x1, 0, z0,
                    x1, 0, z1,
                    x0, 0, z1);
        }
    }
    return tris;
}

// ================================================================
//  FEMesh -> Surface Triangle3D conversion
// ================================================================

std::vector<Triangle3D> GraphExecutor::feMeshToTriangles(const FEMeshData& mesh) {
    std::vector<Triangle3D> tris;
    if (mesh.nodes.empty() || mesh.elements.empty()) return tris;

    // For each hex8 element, determine which faces are on the boundary
    // Build a face -> element count map
    // Face is identified by sorted 4-node tuple
    struct Face4 {
        int n[4];
        bool operator<(const Face4& o) const {
            for (int i = 0; i < 4; i++) {
                if (n[i] != o.n[i]) return n[i] < o.n[i];
            }
            return false;
        }
    };

    // Hex8 face node indices (local)
    static const int faceNodes[6][4] = {
        {0,3,2,1}, // Z- (bottom)
        {4,5,6,7}, // Z+ (top)
        {0,1,5,4}, // Y- (front)
        {2,3,7,6}, // Y+ (back)
        {0,4,7,3}, // X- (left)
        {1,2,6,5}  // X+ (right)
    };

    std::map<Face4, int> faceCount;

    for (auto& elem : mesh.elements) {
        if ((int)elem.nodeIds.size() != 8) continue;
        for (int f = 0; f < 6; f++) {
            Face4 face;
            for (int i = 0; i < 4; i++) {
                face.n[i] = elem.nodeIds[faceNodes[f][i]];
            }
            // Sort to create canonical key
            std::sort(face.n, face.n + 4);
            faceCount[face]++;
        }
    }

    // Boundary faces appear exactly once
    for (auto& elem : mesh.elements) {
        if ((int)elem.nodeIds.size() != 8) continue;

        double nd[8][3];
        for (int i = 0; i < 8; i++) {
            int nid = elem.nodeIds[i];
            if (nid < 0 || nid >= (int)mesh.nodes.size()) continue;
            nd[i][0] = mesh.nodes[nid].x;
            nd[i][1] = mesh.nodes[nid].y;
            nd[i][2] = mesh.nodes[nid].z;
        }

        for (int f = 0; f < 6; f++) {
            Face4 face;
            for (int i = 0; i < 4; i++) {
                face.n[i] = elem.nodeIds[faceNodes[f][i]];
            }
            std::sort(face.n, face.n + 4);

            if (faceCount[face] == 1) {
                // Boundary face -> 2 triangles
                int* fi = (int*)faceNodes[f];
                addQuad(tris,
                    (float)nd[fi[0]][0], (float)nd[fi[0]][1], (float)nd[fi[0]][2],
                    (float)nd[fi[1]][0], (float)nd[fi[1]][1], (float)nd[fi[1]][2],
                    (float)nd[fi[2]][0], (float)nd[fi[2]][1], (float)nd[fi[2]][2],
                    (float)nd[fi[3]][0], (float)nd[fi[3]][1], (float)nd[fi[3]][2]);
            }
        }
    }

    return tris;
}

std::vector<Triangle3D> GraphExecutor::feMeshDensityToTriangles(
    const FEMeshData& mesh, const DensityFieldData& density, double threshold)
{
    std::vector<Triangle3D> tris;
    if (mesh.nodes.empty() || mesh.elements.empty()) return tris;

    // Same boundary extraction but only for elements above threshold
    struct Face4 {
        int n[4];
        bool operator<(const Face4& o) const {
            for (int i = 0; i < 4; i++) {
                if (n[i] != o.n[i]) return n[i] < o.n[i];
            }
            return false;
        }
    };

    static const int faceNodes[6][4] = {
        {0,3,2,1}, {4,5,6,7}, {0,1,5,4}, {2,3,7,6}, {0,4,7,3}, {1,2,6,5}
    };

    // Only consider elements above threshold
    std::map<Face4, int> faceCount;
    for (int e = 0; e < (int)mesh.elements.size(); e++) {
        if (e < (int)density.densities.size() && density.densities[e] < threshold) continue;
        auto& elem = mesh.elements[e];
        if ((int)elem.nodeIds.size() != 8) continue;
        for (int f = 0; f < 6; f++) {
            Face4 face;
            for (int i = 0; i < 4; i++) face.n[i] = elem.nodeIds[faceNodes[f][i]];
            std::sort(face.n, face.n + 4);
            faceCount[face]++;
        }
    }

    for (int e = 0; e < (int)mesh.elements.size(); e++) {
        if (e < (int)density.densities.size() && density.densities[e] < threshold) continue;
        auto& elem = mesh.elements[e];
        if ((int)elem.nodeIds.size() != 8) continue;

        double nd[8][3];
        for (int i = 0; i < 8; i++) {
            int nid = elem.nodeIds[i];
            if (nid < 0 || nid >= (int)mesh.nodes.size()) continue;
            nd[i][0] = mesh.nodes[nid].x;
            nd[i][1] = mesh.nodes[nid].y;
            nd[i][2] = mesh.nodes[nid].z;
        }

        for (int f = 0; f < 6; f++) {
            Face4 face;
            for (int i = 0; i < 4; i++) face.n[i] = elem.nodeIds[faceNodes[f][i]];
            std::sort(face.n, face.n + 4);
            if (faceCount[face] == 1) {
                int* fi = (int*)faceNodes[f];
                addQuad(tris,
                    (float)nd[fi[0]][0], (float)nd[fi[0]][1], (float)nd[fi[0]][2],
                    (float)nd[fi[1]][0], (float)nd[fi[1]][1], (float)nd[fi[1]][2],
                    (float)nd[fi[2]][0], (float)nd[fi[2]][1], (float)nd[fi[2]][2],
                    (float)nd[fi[3]][0], (float)nd[fi[3]][1], (float)nd[fi[3]][2]);
            }
        }
    }

    return tris;
}

// ================================================================
//  Highlight nodes (small octahedra at node positions)
// ================================================================

std::vector<Triangle3D> GraphExecutor::highlightNodes(
    const FEMeshData& mesh, const std::vector<int>& nodeIds,
    float r, float g, float b)
{
    std::vector<Triangle3D> tris;
    if (nodeIds.empty() || mesh.nodes.empty()) return tris;

    // Estimate marker size from mesh bounding box
    double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30, minZ = 1e30, maxZ = -1e30;
    for (auto& n : mesh.nodes) {
        minX = std::min(minX, n.x); maxX = std::max(maxX, n.x);
        minY = std::min(minY, n.y); maxY = std::max(maxY, n.y);
        minZ = std::min(minZ, n.z); maxZ = std::max(maxZ, n.z);
    }
    double diag = std::sqrt((maxX-minX)*(maxX-minX) + (maxY-minY)*(maxY-minY) + (maxZ-minZ)*(maxZ-minZ));
    float sz = (float)(diag * 0.008);
    if (sz < 0.01f) sz = 0.01f;

    // For each selected node, draw a small octahedron
    for (int nid : nodeIds) {
        if (nid < 0 || nid >= (int)mesh.nodes.size()) continue;
        float cx = (float)mesh.nodes[nid].x;
        float cy = (float)mesh.nodes[nid].y;
        float cz = (float)mesh.nodes[nid].z;

        // 6 vertices of octahedron
        float pts[6][3] = {
            {cx+sz, cy, cz}, {cx-sz, cy, cz},
            {cx, cy+sz, cz}, {cx, cy-sz, cz},
            {cx, cy, cz+sz}, {cx, cy, cz-sz}
        };
        // 8 faces
        int faces[8][3] = {
            {0,2,4},{0,4,3},{0,3,5},{0,5,2},
            {1,4,2},{1,3,4},{1,5,3},{1,2,5}
        };
        for (int f = 0; f < 8; f++) {
            Triangle3D t;
            std::copy(pts[faces[f][0]], pts[faces[f][0]]+3, t.v0);
            std::copy(pts[faces[f][1]], pts[faces[f][1]]+3, t.v1);
            std::copy(pts[faces[f][2]], pts[faces[f][2]]+3, t.v2);
            computeTriNormal(t);
            tris.push_back(t);
        }
    }
    return tris;
}

// ================================================================
//  Evaluate upstream dependencies of a node
// ================================================================

void GraphExecutor::evaluateUpstream(int targetNodeId) {
    if (!editor_) return;

    // Build full execution order
    std::vector<int> fullOrder;
    if (!buildExecutionOrder(fullOrder)) return;

    // Find all ancestors of targetNodeId
    auto& conns = editor_->connections();
    std::set<int> needed;
    needed.insert(targetNodeId);

    // Traverse backwards through connections
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& c : conns) {
            if (needed.count(c.endNodeId) && !needed.count(c.startNodeId)) {
                needed.insert(c.startNodeId);
                changed = true;
            }
        }
    }

    // Execute only needed nodes in topological order
    for (int id : fullOrder) {
        if (needed.count(id)) {
            evaluateNode(id);
        }
    }
}

// ================================================================
//  Live preview for selected node
// ================================================================

void GraphExecutor::previewNode(int nodeId) {
    if (!editor_ || !view3D_) return;

    auto* node = editor_->findNode(nodeId);
    if (!node) return;

    const std::string& t = node->typeName;

    // Clear previous output data for clean preview
    outputData_.clear();

    // Evaluate this node and its upstream dependencies
    evaluateUpstream(nodeId);

    std::vector<Triangle3D> previewTris;

    // --- Domain nodes: show FEMesh surface ---
    if (t == "domain-box" || t == "domain-lshape" || t == "domain-from-mesh" || t == "domain-import") {
        auto it = outputData_.find({nodeId, 0});
        if (it != outputData_.end() && it->second.isFEMesh()) {
            previewTris = feMeshToTriangles(it->second.asFEMesh());
        }
    }
    // --- Passive Region: show FEMesh with passive zones highlighted ---
    else if (t == "topo-passive-region") {
        auto it = outputData_.find({nodeId, 0});
        if (it != outputData_.end() && it->second.isFEMesh()) {
            previewTris = feMeshToTriangles(it->second.asFEMesh());
            // Add markers for passive elements
            auto& mesh = it->second.asFEMesh();
            for (int eid : mesh.passiveSolid) {
                if (eid >= 0 && eid < (int)mesh.elements.size()) {
                    // Highlight solid passive elements
                    std::vector<int> nids = mesh.elements[eid].nodeIds;
                    auto markers = highlightNodes(mesh, nids, 0.0f, 0.0f, 1.0f);
                    previewTris.insert(previewTris.end(), markers.begin(), markers.end());
                }
            }
            for (int eid : mesh.passiveVoid) {
                if (eid >= 0 && eid < (int)mesh.elements.size()) {
                    std::vector<int> nids = mesh.elements[eid].nodeIds;
                    auto markers = highlightNodes(mesh, nids, 1.0f, 0.0f, 0.0f);
                    previewTris.insert(previewTris.end(), markers.begin(), markers.end());
                }
            }
        }
    }
    // --- BC nodes: show mesh with selected nodes highlighted ---
    else if (t == "fea-fixed-support" || t == "fea-displacement-bc" ||
             t == "fea-point-force" || t == "fea-pressure-load" || t == "fea-body-force") {
        auto it = outputData_.find({nodeId, 0});
        if (it != outputData_.end() && it->second.isBC()) {
            // Find upstream FEMesh to render
            NodeData inMesh = getInputData(nodeId, 0);
            if (inMesh.isFEMesh()) {
                previewTris = feMeshToTriangles(inMesh.asFEMesh());
                // Highlight BC nodes
                auto& bc = it->second.asBC();
                auto markers = highlightNodes(inMesh.asFEMesh(), bc.nodeIds,
                    (t == "fea-fixed-support") ? 1.0f : 0.0f,
                    (t == "fea-point-force" || t == "fea-pressure-load") ? 1.0f : 0.0f,
                    (t == "fea-displacement-bc") ? 1.0f : 0.0f);
                previewTris.insert(previewTris.end(), markers.begin(), markers.end());
            }
        }
    }
    // --- FEA Solver: show mesh ---
    else if (t == "fea-solver") {
        // Show mesh from port 1
        auto it = outputData_.find({nodeId, 1});
        if (it != outputData_.end() && it->second.isFEMesh()) {
            previewTris = feMeshToTriangles(it->second.asFEMesh());
        }
    }
    // --- SIMP/BESO: show density field ---
    else if (t == "topo-simp" || t == "topo-beso") {
        auto itD = outputData_.find({nodeId, 0});
        // Get the FEMesh from input
        NodeData inMesh = getInputData(nodeId, 0);
        if (itD != outputData_.end() && itD->second.isDensityField() && inMesh.isFEMesh()) {
            previewTris = feMeshDensityToTriangles(inMesh.asFEMesh(), itD->second.asDensityField(), 0.5);
        }
    }
    // --- Density View: show extracted mesh ---
    else if (t == "post-density-view") {
        auto it = outputData_.find({nodeId, 0});
        if (it != outputData_.end() && it->second.isMesh()) {
            previewTris = it->second.asMesh();
        }
    }
    // --- Material node: no geometry to show ---
    else if (t == "fea-material") {
        // Nothing to visualize
        return;
    }
    // --- Load Case: no geometry to show ---
    else if (t == "fea-load-case") {
        return;
    }
    // --- Data mesh gen: show surface mesh ---
    else if (t == "data-mesh-gen") {
        auto it = outputData_.find({nodeId, 0});
        if (it != outputData_.end() && it->second.isMesh()) {
            previewTris = it->second.asMesh();
        }
    }
    // --- Convergence / Extract Field: no geometry ---
    else if (t == "post-convergence" || t == "post-extract-field" || t == "post-export") {
        return;
    }
    // --- Output viewer: use its mesh ---
    else if (t == "output-viewer") {
        NodeData inMesh = getInputData(nodeId, 0);
        if (inMesh.isMesh()) {
            previewTris = inMesh.asMesh();
        }
    }

    if (!previewTris.empty()) {
        view3D_->setTriangles(previewTris);
    }
}

} // namespace TopOpt
