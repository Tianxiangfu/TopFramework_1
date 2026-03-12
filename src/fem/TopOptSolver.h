#pragma once
#include "../execution/NodeData.h"
#include "FEMSolver.h"
#include <vector>
#include <functional>

namespace TopOpt {

class TopOptSolver {
public:
    // SIMP parameters
    double volFrac = 0.5;
    double penalty = 3.0;
    double filterRadius = 1.5;
    int maxIter = 200;
    double minDensity = 0.001;
    double tolConverge = 0.01; // relative change threshold
    int filterType = 0; // 0=Density, 1=Sensitivity

    // Set problem
    void setMesh(const FEMeshData& mesh);
    void setMaterial(const MaterialData& mat);
    void setLoadCases(const std::vector<LoadCaseData>& loadCases);

    // Run SIMP optimization
    bool runSIMP();

    // Get results
    const DensityFieldData& densityResult() const { return densityResult_; }
    const FEResultData& feResult() const { return feResult_; }

private:
    // Density filter (sphere-weighted average)
    void applyDensityFilter(std::vector<double>& filtered, const std::vector<double>& raw);

    // Sensitivity filter
    void applySensitivityFilter(std::vector<double>& dc, const std::vector<double>& x);

    // OC update
    void ocUpdate(std::vector<double>& x, const std::vector<double>& dc);

    // Compute element centers for filtering
    void computeElementCenters();

    FEMeshData mesh_;
    MaterialData mat_;
    std::vector<LoadCaseData> loadCases_;

    // Element centers (for filter distance)
    std::vector<double> elemCenterX_, elemCenterY_, elemCenterZ_;

    DensityFieldData densityResult_;
    FEResultData feResult_;
};

} // namespace TopOpt
