#include "TopOptSolver.h"
#include "../utils/Logger.h"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>

namespace TopOpt {

namespace {

void storeDensitySnapshot(
    DensityFieldData& densityResult,
    const std::vector<double>& densities)
{
    densityResult.densities = densities;
    densityResult.iteration = static_cast<int>(densityResult.history.size());

    double totalDensity = 0.0;
    for (double x : densities) {
        totalDensity += x;
    }

    densityResult.volFrac = densities.empty() ? 0.0 : totalDensity / densities.size();
    densityResult.objective =
        densityResult.history.empty() ? 0.0 : densityResult.history.back();
}

void appendDensityFrame(
    DensityFieldData& densityResult,
    const std::vector<double>& densities,
    int iteration,
    double objective,
    double volFrac,
    double maxChange)
{
    densityResult.densityFrames.push_back(densities);
    densityResult.frameInfo.push_back(DensityFrameInfo{
        iteration,
        objective,
        volFrac,
        maxChange
    });
}

} // namespace

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
    densityResult_.densityFrames.clear();
    densityResult_.frameInfo.clear();

    double totalSolverMs = 0.0;
    double totalAssemblyMs = 0.0;
    double totalBcMs = 0.0;
    double totalPostMs = 0.0;
    double totalFilterMs = 0.0;
    double totalOcMs = 0.0;
    double totalIterationMs = 0.0;
    int completedIterations = 0;

    FEMSolver solver;
    solver.setMesh(mesh_);
    solver.setMaterial(mat_);
    solver.setConfig(solverConfig_);

    for (int iter = 0; iter < maxIter; iter++) {
        const auto iterStart = std::chrono::steady_clock::now();
        double iterSolverMs = 0.0;
        double iterAssemblyMs = 0.0;
        double iterBcMs = 0.0;
        double iterPostMs = 0.0;
        // Set densities and solve for each load case
        solver.setDensities(x, penalty, 1e-9 * mat_.E);

        double totalCompliance = 0;
        std::vector<double> dc(nElem, 0.0);

        for (auto& lc : loadCases_) {
            solver.setLoadCase(lc);
            if (!solver.solve(false)) {
                feResult_ = solver.result();
                iterAssemblyMs += feResult_.assemblyTimeMs;
                iterBcMs += feResult_.boundaryConditionTimeMs;
                iterSolverMs += feResult_.solveTimeMs;
                iterPostMs += feResult_.postProcessTimeMs;
                totalAssemblyMs += feResult_.assemblyTimeMs;
                totalBcMs += feResult_.boundaryConditionTimeMs;
                totalSolverMs += feResult_.solveTimeMs;
                totalPostMs += feResult_.postProcessTimeMs;
                double currentVol = 0.0;
                for (double xi : x) currentVol += xi;
                currentVol = nElem > 0 ? currentVol / nElem : 0.0;
                const int failureIteration = static_cast<int>(densityResult_.history.size());
                const double lastObjective =
                    densityResult_.history.empty() ? 0.0 : densityResult_.history.back();
                appendDensityFrame(densityResult_, x, failureIteration, lastObjective, currentVol, 0.0);
                storeDensitySnapshot(densityResult_, x);
                return false;
            }

            // Accumulate compliance and sensitivity
            auto& res = solver.result();
            iterAssemblyMs += res.assemblyTimeMs;
            iterBcMs += res.boundaryConditionTimeMs;
            iterSolverMs += res.solveTimeMs;
            iterPostMs += res.postProcessTimeMs;
            totalAssemblyMs += res.assemblyTimeMs;
            totalBcMs += res.boundaryConditionTimeMs;
            totalSolverMs += res.solveTimeMs;
            totalPostMs += res.postProcessTimeMs;
            totalCompliance += lc.weight * res.compliance;

            // Sensitivity: dc/dx = -p * x^(p-1) * ue^T * K0e * ue
            for (int e = 0; e < nElem; e++) {
                double ce = solver.elementStrainEnergyFromReferenceKe(e);
                if (passiveSolidMask[e] || passiveVoidMask[e]) continue;

                dc[e] += lc.weight * (-penalty * std::pow(x[e], penalty - 1.0) * ce);
            }
        }

        // Apply filter
        const auto filterStart = std::chrono::steady_clock::now();
        if (filterType == 0) {
            // Density filter
            std::vector<double> xFiltered;
            applyDensityFilter(xFiltered, x);
            applySensitivityFilter(dc, xFiltered);
        } else {
            // Sensitivity filter only
            applySensitivityFilter(dc, x);
        }
        const auto filterEnd = std::chrono::steady_clock::now();
        const double filterMs =
            std::chrono::duration<double, std::milli>(filterEnd - filterStart).count();
        totalFilterMs += filterMs;

        // OC update
        std::vector<double> xOld = x;
        const auto ocStart = std::chrono::steady_clock::now();
        ocUpdate(x, dc);
        const auto ocEnd = std::chrono::steady_clock::now();
        const double ocMs =
            std::chrono::duration<double, std::milli>(ocEnd - ocStart).count();
        totalOcMs += ocMs;

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

        // Convergence check
        double change = 0;
        for (int i = 0; i < nElem; i++) {
            change = std::max(change, std::abs(x[i] - xOld[i]));
        }

        densityResult_.history.push_back(totalCompliance);
        appendDensityFrame(densityResult_, x, iter + 1, totalCompliance, currentVol, change);

        const auto iterEnd = std::chrono::steady_clock::now();
        const double iterationMs =
            std::chrono::duration<double, std::milli>(iterEnd - iterStart).count();
        totalIterationMs += iterationMs;
        completedIterations++;

        std::ostringstream iterLog;
        iterLog << "[TopOpt Timing] iter=" << (iter + 1)
                << ", iterMs=" << iterationMs
                << ", assemblyMs=" << iterAssemblyMs
                << ", bcMs=" << iterBcMs
                << ", solveMs=" << iterSolverMs
                << ", postMs=" << iterPostMs
                << ", filterMs=" << filterMs
                << ", ocMs=" << ocMs
                << ", compliance=" << totalCompliance
                << ", change=" << change;
        Logger::instance().info(iterLog.str());

        if (iter > 10 && change < tolConverge) {
            break;
        }
    }

    // Store final results
    storeDensitySnapshot(densityResult_, x);

    // Final FEA solve for result output
    solver.setDensities(x, penalty, 1e-9 * mat_.E);
    solver.setLoadCase(loadCases_[0]);
    if (!solver.solve(true)) {
        feResult_ = solver.result();
        storeDensitySnapshot(densityResult_, x);
        return false;
    }
    feResult_ = solver.result();

    std::ostringstream summary;
    summary << "[TopOpt Timing] summary"
            << ", iterations=" << completedIterations
            << ", totalIterMs=" << totalIterationMs
            << ", totalAssemblyMs=" << totalAssemblyMs
            << ", totalBcMs=" << totalBcMs
            << ", totalSolveMs=" << totalSolverMs
            << ", totalPostMs=" << totalPostMs
            << ", totalFilterMs=" << totalFilterMs
            << ", totalOcMs=" << totalOcMs
            << ", finalSolveAssemblyMs=" << feResult_.assemblyTimeMs
            << ", finalSolveBcMs=" << feResult_.boundaryConditionTimeMs
            << ", finalSolveMs=" << feResult_.solveTimeMs
            << ", finalSolvePostMs=" << feResult_.postProcessTimeMs
            << ", finalSolveTotalMs=" << feResult_.totalTimeMs;
    Logger::instance().info(summary.str());

    return true;
}

} // namespace TopOpt
