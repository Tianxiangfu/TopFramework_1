#include "TopOptSolver.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace TopOpt {

void TopOptSolver::setMesh(const FEMeshData& mesh) { mesh_ = mesh; }
void TopOptSolver::setMaterial(const MaterialData& mat) { mat_ = mat; }
void TopOptSolver::setLoadCases(const std::vector<LoadCaseData>& lcs) { loadCases_ = lcs; }

// ================================================================
//  Compute element centers for filtering
// ================================================================

void TopOptSolver::computeElementCenters() {
    int nElem = (int)mesh_.elements.size();
    elemCenterX_.resize(nElem);
    elemCenterY_.resize(nElem);
    elemCenterZ_.resize(nElem);

    for (int e = 0; e < nElem; e++) {
        double cx = 0, cy = 0, cz = 0;
        int nn = (int)mesh_.elements[e].nodeIds.size();
        for (int nid : mesh_.elements[e].nodeIds) {
            cx += mesh_.nodes[nid].x;
            cy += mesh_.nodes[nid].y;
            cz += mesh_.nodes[nid].z;
        }
        elemCenterX_[e] = cx / nn;
        elemCenterY_[e] = cy / nn;
        elemCenterZ_[e] = cz / nn;
    }
}

void TopOptSolver::buildFilterNeighborhood() {
    int nElem = (int)mesh_.elements.size();
    filterNeighbors_.assign(nElem, {});
    double rmin = filterRadius;
    double rmin2 = rmin * rmin;

    for (int i = 0; i < nElem; i++) {
        auto& neighbors = filterNeighbors_[i];
        for (int j = 0; j < nElem; j++) {
            double dx = elemCenterX_[i] - elemCenterX_[j];
            double dy = elemCenterY_[i] - elemCenterY_[j];
            double dz = elemCenterZ_[i] - elemCenterZ_[j];
            double dist2 = dx*dx + dy*dy + dz*dz;
            if (dist2 < rmin2) {
                double dist = std::sqrt(dist2);
                neighbors.emplace_back(j, rmin - dist);
            }
        }
    }
}

// ================================================================
//  Density filter (sphere-weighted average)
// ================================================================

void TopOptSolver::applyDensityFilter(std::vector<double>& filtered,
                                       const std::vector<double>& raw) {
    int nElem = (int)raw.size();
    filtered.resize(nElem);

    for (int i = 0; i < nElem; i++) {
        double sumW = 0, sumWx = 0;
        for (const auto& [j, w] : filterNeighbors_[i]) {
            sumW  += w;
            sumWx += w * raw[j];
        }
        filtered[i] = (sumW > 0) ? sumWx / sumW : raw[i];
    }
}

// ================================================================
//  Sensitivity filter
// ================================================================

void TopOptSolver::applySensitivityFilter(std::vector<double>& dc,
                                           const std::vector<double>& x) {
    int nElem = (int)dc.size();
    std::vector<double> dcOrig = dc;

    for (int i = 0; i < nElem; i++) {
        double sumW = 0, sumWdc = 0;
        for (const auto& [j, w] : filterNeighbors_[i]) {
            sumW   += w;
            sumWdc += w * x[j] * dcOrig[j];
        }
        if (sumW > 0 && x[i] > 0) {
            dc[i] = sumWdc / (x[i] * sumW);
        }
    }
}

// ================================================================
//  OC (Optimality Criteria) update
// ================================================================

void TopOptSolver::ocUpdate(std::vector<double>& x, const std::vector<double>& dc) {
    int nElem = (int)x.size();
    double move = 0.2;

    // Bisection to find Lagrange multiplier
    double l1 = 0, l2 = 1e9;
    double targetVol = volFrac * nElem;

    while ((l2 - l1) / (l1 + l2) > 1e-3) {
        double lmid = 0.5 * (l1 + l2);

        std::vector<double> xnew(nElem);
        for (int i = 0; i < nElem; i++) {
            double Be = -dc[i] / lmid;
            double xCandidate = x[i] * std::sqrt(Be);
            // Apply move limit
            xCandidate = std::max(std::max(minDensity, x[i] - move),
                                  std::min(std::min(1.0, x[i] + move), xCandidate));
            xnew[i] = xCandidate;
        }

        double vol = 0;
        for (int i = 0; i < nElem; i++) vol += xnew[i];

        if (vol > targetVol) l1 = lmid;
        else                 l2 = lmid;
    }

    // Final update with converged multiplier
    double lmid = 0.5 * (l1 + l2);
    for (int i = 0; i < nElem; i++) {
        double Be = -dc[i] / lmid;
        double xCandidate = x[i] * std::sqrt(std::max(Be, 1e-15));
        x[i] = std::max(std::max(minDensity, x[i] - move),
                        std::min(std::min(1.0, x[i] + move), xCandidate));
    }
}

// ================================================================
//  Run SIMP optimization
// ================================================================

bool TopOptSolver::runSIMP() {
    if (mesh_.nodes.empty() || mesh_.elements.empty() || loadCases_.empty()) {
        return false;
    }

    int nElem = (int)mesh_.elements.size();
    computeElementCenters();
    buildFilterNeighborhood();

    // Initialize densities
    std::vector<double> x(nElem, volFrac);
    std::vector<char> passiveSolidMask(nElem, 0);
    std::vector<char> passiveVoidMask(nElem, 0);

    // Set passive regions
    for (int eid : mesh_.passiveSolid) {
        if (eid >= 0 && eid < nElem) {
            x[eid] = 1.0;
            passiveSolidMask[eid] = 1;
        }
    }
    for (int eid : mesh_.passiveVoid) {
        if (eid >= 0 && eid < nElem) {
            x[eid] = minDensity;
            passiveVoidMask[eid] = 1;
        }
    }

    densityResult_.history.clear();

    FEMSolver solver;
    solver.setMesh(mesh_);
    solver.setMaterial(mat_);
    solver.setConfig(solverConfig_);

    for (int iter = 0; iter < maxIter; iter++) {
        // Set densities and solve for each load case
        solver.setDensities(x, penalty, 1e-9 * mat_.E);

        double totalCompliance = 0;
        std::vector<double> dc(nElem, 0.0);

        for (auto& lc : loadCases_) {
            solver.setLoadCase(lc);
            if (!solver.solve()) {
                return false;
            }

            // Accumulate compliance and sensitivity
            auto& res = solver.result();
            totalCompliance += lc.weight * res.compliance;

            // Sensitivity: dc/dx = -p * x^(p-1) * ue^T * K0e * ue
            for (int e = 0; e < nElem; e++) {
                double ce = solver.elementStrainEnergyFromReferenceKe(e);
                if (passiveSolidMask[e] || passiveVoidMask[e]) continue;

                dc[e] += lc.weight * (-penalty * std::pow(x[e], penalty - 1.0) * ce);
            }
        }

        // Apply filter
        if (filterType == 0) {
            // Density filter
            std::vector<double> xFiltered;
            applyDensityFilter(xFiltered, x);
            applySensitivityFilter(dc, xFiltered);
        } else {
            // Sensitivity filter only
            applySensitivityFilter(dc, x);
        }

        // OC update
        std::vector<double> xOld = x;
        ocUpdate(x, dc);

        // Restore passive regions
        for (int eid : mesh_.passiveSolid) {
            if (eid >= 0 && eid < nElem) x[eid] = 1.0;
        }
        for (int eid : mesh_.passiveVoid) {
            if (eid >= 0 && eid < nElem) x[eid] = minDensity;
        }

        // Compute volume fraction
        double currentVol = 0;
        for (double xi : x) currentVol += xi;
        currentVol /= nElem;

        densityResult_.history.push_back(totalCompliance);

        // Convergence check
        double change = 0;
        for (int i = 0; i < nElem; i++) {
            change = std::max(change, std::abs(x[i] - xOld[i]));
        }

        if (iter > 10 && change < tolConverge) {
            break;
        }
    }

    // Store final results
    densityResult_.densities = x;
    densityResult_.iteration = (int)densityResult_.history.size();
    double finalVol = 0;
    for (double xi : x) finalVol += xi;
    densityResult_.volFrac = finalVol / nElem;
    densityResult_.objective = densityResult_.history.empty() ? 0 : densityResult_.history.back();

    // Final FEA solve for result output
    solver.setDensities(x, penalty, 1e-9 * mat_.E);
    solver.setLoadCase(loadCases_[0]);
    solver.solve();
    feResult_ = solver.result();

    return true;
}

} // namespace TopOpt
