#pragma once
#include "NodeData.h"
#include "../node_editor/Node.h"
#include <map>
#include <vector>
#include <string>

namespace TopOpt {

class NodeEditor;
class View3DPanel;

// Key for output data storage: {nodeId, portIndex}
struct OutputKey {
    int nodeId;
    int portIdx;
    bool operator<(const OutputKey& o) const {
        if (nodeId != o.nodeId) return nodeId < o.nodeId;
        return portIdx < o.portIdx;
    }
};

class GraphExecutor {
public:
    GraphExecutor();
    ~GraphExecutor();

    // Set references to other systems
    void setEditor(NodeEditor* editor)   { editor_ = editor; }
    void setView3D(View3DPanel* view3D)  { view3D_ = view3D; }

    // Execution control
    void runAll();      // Execute all nodes in topological order
    void stepOne();     // Execute the next unexecuted node
    void reset();       // Clear all computed data

    // Live preview: evaluate a node and its upstream deps, push result to View3D
    void previewNode(int nodeId);
    bool previewDensityViewFromCache(int nodeId);
    int cachedDensityFrameCountForNode(int nodeId) const;
    const DensityFieldData* cachedDensityFieldForNode(int nodeId) const;

    // Helper: find param by name on a node
    const ParamDef* findParam(int nodeId, const std::string& name);

private:
    // Build topological order (Kahn's algorithm), returns false if cycle detected
    bool buildExecutionOrder(std::vector<int>& order);

    // Get input data for a specific port of a node (follows connections upstream)
    NodeData getInputData(int nodeId, int portIdx);

    // Evaluate a single node, storing results in outputData_
    void evaluateNode(int nodeId);

    // ---- Per-type evaluation functions ----
    // Input
    void evalInputNumber(int nodeId);
    void evalInputVector(int nodeId);
    void evalInputBoolean(int nodeId);
    void evalInputFile(int nodeId);
    // Output
    void evalOutputDisplay(int nodeId);
    void evalOutputExport(int nodeId);
    void evalOutputViewer(int nodeId);
    // Data
    void evalDataMeshGen(int nodeId);
    // Domain
    void evalDomainBox(int nodeId);
    void evalDomainLShape(int nodeId);
    void evalDomainFromMesh(int nodeId);
    void evalDomainImport(int nodeId);
    // FEA
    void evalFEAMaterial(int nodeId);
    void evalFEAFixedSupport(int nodeId);
    void evalFEADisplacementBC(int nodeId);
    void evalFEAPointForce(int nodeId);
    void evalFEAPressureLoad(int nodeId);
    void evalFEABodyForce(int nodeId);
    void evalFEALoadCase(int nodeId);
    void evalFEASolver(int nodeId);
    // Topology
    void evalTopoSIMP(int nodeId);
    void evalTopoBESO(int nodeId);
    void evalTopoConstraint(int nodeId);
    void evalTopoPassiveRegion(int nodeId);
    // PostProcess
    void evalPostDensityView(int nodeId);
    void evalPostExtractField(int nodeId);
    void evalPostConvergence(int nodeId);
    void evalPostExport(int nodeId);

    // ---- Helpers ----
    // Convert FEMeshData surface to Triangle3D for visualization
    std::vector<Triangle3D> feMeshToTriangles(const FEMeshData& mesh);
    // Convert FEMeshData + density to Triangle3D (only elements above threshold)
    std::vector<Triangle3D> feMeshDensityToTriangles(const FEMeshData& mesh,
                                                      const DensityFieldData& density,
                                                      double threshold = 0.5);
    // Highlight selected nodes on FEMesh (small spheres at node positions)
    std::vector<Triangle3D> highlightNodes(const FEMeshData& mesh,
                                            const std::vector<int>& nodeIds,
                                            float r = 0.0f, float g = 1.0f, float b = 0.0f);

    // Select nodes from FEMesh based on face or coord range
    std::vector<int> selectNodes(const FEMeshData& mesh, int nodeId);

    // Evaluate a node and all its upstream dependencies (for preview)
    void evaluateUpstream(int targetNodeId);
    const NodeData* findCachedOutput(int nodeId, int portIdx) const;
    const DensityFieldData* findCachedDensityInput(int nodeId) const;
    const FEMeshData* findCachedFEMeshInput(int nodeId) const;

    // Mesh generation helpers (for data-mesh-gen)
    std::vector<Triangle3D> generateBox(int resolution);
    std::vector<Triangle3D> generateSphere(int resolution);
    std::vector<Triangle3D> generateCylinder(int resolution);
    std::vector<Triangle3D> generatePlane(int resolution);

    NodeEditor*  editor_ = nullptr;
    View3DPanel* view3D_ = nullptr;

    // Computed output data, keyed by {nodeId, portIndex}
    std::map<OutputKey, NodeData> outputData_;
    std::map<OutputKey, NodeData> runOutputData_;

    // Step execution state
    std::vector<int> executionOrder_;
    int stepIndex_ = 0;
};

} // namespace TopOpt
