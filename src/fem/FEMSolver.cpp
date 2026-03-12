#include "FEMSolver.h"
#include "GpuAmgXSolverBackend.h"
#include <Eigen/SparseCholesky>
#include <chrono>
#include <cmath>

namespace TopOpt {

class CpuEigenSolverBackend : public FEMSolverBackend {
public:
    bool solve(FEMSolver& solver, FEResultData& result) override {
        solver.assembleGlobal();
        solver.applyBCs();
        solver.K_.makeCompressed();

        auto start = std::chrono::steady_clock::now();

        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> linearSolver;
        linearSolver.compute(solver.K_);
        if (linearSolver.info() != Eigen::Success) {
            result.backendUsed = name();
            result.converged = false;
            result.solverMessage = "CPU factorization failed";
            return false;
        }

        solver.U_ = linearSolver.solve(solver.F_);
        auto end = std::chrono::steady_clock::now();

        result.backendUsed = name();
        result.solveTimeMs =
            std::chrono::duration<double, std::milli>(end - start).count();
        result.iterationCount = 1;
        result.residualNorm = (solver.K_ * solver.U_ - solver.F_).norm();

        if (linearSolver.info() != Eigen::Success) {
            result.converged = false;
            result.solverMessage = "CPU solve failed";
            return false;
        }

        result.converged = true;
        result.solverMessage = "Solved with Eigen SimplicialLDLT";
        return true;
    }

    const char* name() const override { return "cpu-direct"; }
};

FEMSolver::FEMSolver() {}

bool FEMSolver::sameMeshTopology(const FEMeshData& other) const {
    if (mesh_.nodes.size() != other.nodes.size() || mesh_.elements.size() != other.elements.size()) {
        return false;
    }
    for (size_t i = 0; i < mesh_.nodes.size(); ++i) {
        const auto& a = mesh_.nodes[i];
        const auto& b = other.nodes[i];
        if (a.x != b.x || a.y != b.y || a.z != b.z) {
            return false;
        }
    }
    for (size_t i = 0; i < mesh_.elements.size(); ++i) {
        if (mesh_.elements[i].nodeIds != other.elements[i].nodeIds) {
            return false;
        }
    }
    return true;
}

void FEMSolver::setMesh(const FEMeshData& mesh) {
    bool unchanged = sameMeshTopology(mesh);
    mesh_ = mesh;
    if (!unchanged) {
        keCacheValid_ = false;
        cachedMesh_ = FEMeshData{};
        keCache_.clear();
    }
}

void FEMSolver::setMaterial(const MaterialData& mat) {
    mat_ = mat;
    D_ = constitutiveD();
}

void FEMSolver::setLoadCase(const LoadCaseData& lc) { loadCase_ = lc; }

void FEMSolver::setDensities(const std::vector<double>& densities, double penalty, double Emin) {
    densities_ = densities;
    penalty_ = penalty;
    Emin_ = Emin;
    useDensity_ = true;
}

FEMSolver::ShapeEval FEMSolver::evalShape(double xi, double eta, double zeta) {
    ShapeEval se{};
    double xim[2] = {1.0 - xi, 1.0 + xi};
    double etm[2] = {1.0 - eta, 1.0 + eta};
    double zem[2] = {1.0 - zeta, 1.0 + zeta};

    int xi_sign[8]   = {0,1,1,0,0,1,1,0};
    int eta_sign[8]  = {0,0,1,1,0,0,1,1};
    int zeta_sign[8] = {0,0,0,0,1,1,1,1};

    double xi_d[8]   = {-1,1,1,-1,-1,1,1,-1};
    double eta_d[8]  = {-1,-1,1,1,-1,-1,1,1};
    double zeta_d[8] = {-1,-1,-1,-1,1,1,1,1};

    for (int i = 0; i < 8; i++) {
        se.N[i] = 0.125 * xim[xi_sign[i]] * etm[eta_sign[i]] * zem[zeta_sign[i]];
        se.dNdxi[i]   = 0.125 * xi_d[i]   * etm[eta_sign[i]] * zem[zeta_sign[i]];
        se.dNdeta[i]  = 0.125 * xim[xi_sign[i]] * eta_d[i]  * zem[zeta_sign[i]];
        se.dNdzeta[i] = 0.125 * xim[xi_sign[i]] * etm[eta_sign[i]] * zeta_d[i];
    }
    return se;
}

Eigen::Matrix<double, 6, 6> FEMSolver::constitutiveD() const {
    double E = mat_.E;
    double nu = mat_.nu;
    double c = E / ((1.0 + nu) * (1.0 - 2.0 * nu));

    Eigen::Matrix<double, 6, 6> D = Eigen::Matrix<double, 6, 6>::Zero();
    D(0,0) = D(1,1) = D(2,2) = c * (1.0 - nu);
    D(0,1) = D(0,2) = D(1,0) = D(1,2) = D(2,0) = D(2,1) = c * nu;
    D(3,3) = D(4,4) = D(5,5) = c * (1.0 - 2.0 * nu) / 2.0;
    return D;
}

Eigen::Matrix<double, 6, 24> FEMSolver::computeB(
    const ShapeEval& se, const double coords[][3], double& detJ) const
{
    Eigen::Matrix3d J = Eigen::Matrix3d::Zero();
    for (int i = 0; i < 8; i++) {
        J(0,0) += se.dNdxi[i]   * coords[i][0];
        J(0,1) += se.dNdxi[i]   * coords[i][1];
        J(0,2) += se.dNdxi[i]   * coords[i][2];
        J(1,0) += se.dNdeta[i]  * coords[i][0];
        J(1,1) += se.dNdeta[i]  * coords[i][1];
        J(1,2) += se.dNdeta[i]  * coords[i][2];
        J(2,0) += se.dNdzeta[i] * coords[i][0];
        J(2,1) += se.dNdzeta[i] * coords[i][1];
        J(2,2) += se.dNdzeta[i] * coords[i][2];
    }

    detJ = J.determinant();
    Eigen::Matrix3d Jinv = J.inverse();

    double dNdx[8], dNdy[8], dNdz[8];
    for (int i = 0; i < 8; i++) {
        dNdx[i] = Jinv(0,0)*se.dNdxi[i] + Jinv(0,1)*se.dNdeta[i] + Jinv(0,2)*se.dNdzeta[i];
        dNdy[i] = Jinv(1,0)*se.dNdxi[i] + Jinv(1,1)*se.dNdeta[i] + Jinv(1,2)*se.dNdzeta[i];
        dNdz[i] = Jinv(2,0)*se.dNdxi[i] + Jinv(2,1)*se.dNdeta[i] + Jinv(2,2)*se.dNdzeta[i];
    }

    Eigen::Matrix<double, 6, 24> B = Eigen::Matrix<double, 6, 24>::Zero();
    for (int i = 0; i < 8; i++) {
        int c = i * 3;
        B(0, c)     = dNdx[i];
        B(1, c + 1) = dNdy[i];
        B(2, c + 2) = dNdz[i];
        B(3, c)     = dNdy[i];
        B(3, c + 1) = dNdx[i];
        B(4, c + 1) = dNdz[i];
        B(4, c + 2) = dNdy[i];
        B(5, c)     = dNdz[i];
        B(5, c + 2) = dNdx[i];
    }
    return B;
}

void FEMSolver::rebuildCachesIfNeeded() {
    if (keCacheValid_ && sameMeshTopology(cachedMesh_)) {
        return;
    }

    const int nElem = static_cast<int>(mesh_.elements.size());
    keCache_.assign(nElem, Eigen::Matrix<double, 24, 24>::Zero());

    static const double gp = 1.0 / std::sqrt(3.0);
    static const double gpts[2] = {-gp, gp};

    for (int elemIdx = 0; elemIdx < nElem; ++elemIdx) {
        const auto& elem = mesh_.elements[elemIdx];
        double coords[8][3];
        for (int i = 0; i < 8; i++) {
            int nid = elem.nodeIds[i];
            coords[i][0] = mesh_.nodes[nid].x;
            coords[i][1] = mesh_.nodes[nid].y;
            coords[i][2] = mesh_.nodes[nid].z;
        }

        Eigen::Matrix<double, 24, 24> Ke = Eigen::Matrix<double, 24, 24>::Zero();
        for (int gi = 0; gi < 2; gi++) {
            for (int gj = 0; gj < 2; gj++) {
                for (int gk = 0; gk < 2; gk++) {
                    auto se = evalShape(gpts[gi], gpts[gj], gpts[gk]);
                    double detJ = 0.0;
                    auto B = computeB(se, coords, detJ);
                    Ke.noalias() += B.transpose() * D_ * B * std::abs(detJ);
                }
            }
        }
        keCache_[elemIdx] = Ke;
    }

    cachedMesh_ = mesh_;
    keCacheValid_ = true;
}

Eigen::Matrix<double, 24, 24> FEMSolver::hex8Ke(int elemIdx) const {
    if (keCacheValid_ && elemIdx >= 0 && elemIdx < static_cast<int>(keCache_.size())) {
        return keCache_[elemIdx];
    }

    auto& elem = mesh_.elements[elemIdx];
    double coords[8][3];
    for (int i = 0; i < 8; i++) {
        int nid = elem.nodeIds[i];
        coords[i][0] = mesh_.nodes[nid].x;
        coords[i][1] = mesh_.nodes[nid].y;
        coords[i][2] = mesh_.nodes[nid].z;
    }

    static const double gp = 1.0 / std::sqrt(3.0);
    static const double gpts[2] = {-gp, gp};

    Eigen::Matrix<double, 24, 24> Ke = Eigen::Matrix<double, 24, 24>::Zero();
    for (int gi = 0; gi < 2; gi++) {
        for (int gj = 0; gj < 2; gj++) {
            for (int gk = 0; gk < 2; gk++) {
                auto se = evalShape(gpts[gi], gpts[gj], gpts[gk]);
                double detJ = 0.0;
                auto B = computeB(se, coords, detJ);
                Ke.noalias() += B.transpose() * D_ * B * std::abs(detJ);
            }
        }
    }
    return Ke;
}

double FEMSolver::densityScaleForElement(int elemIdx) const {
    if (!useDensity_ || elemIdx >= static_cast<int>(densities_.size())) {
        return 1.0;
    }
    double rho = densities_[elemIdx];
    return Emin_ + std::pow(rho, penalty_) * (1.0 - Emin_);
}

Eigen::Matrix<double, 24, 1> FEMSolver::elementDisp(int elemIdx) const {
    Eigen::Matrix<double, 24, 1> ue;
    auto& elem = mesh_.elements[elemIdx];
    for (int i = 0; i < 8; i++) {
        int nid = elem.nodeIds[i];
        ue(i * 3)     = U_(nid * 3);
        ue(i * 3 + 1) = U_(nid * 3 + 1);
        ue(i * 3 + 2) = U_(nid * 3 + 2);
    }
    return ue;
}

double FEMSolver::elementStrainEnergyFromReferenceKe(int elemIdx) const {
    auto ue = elementDisp(elemIdx);
    const auto& Ke0 = keCache_[elemIdx];
    return (ue.transpose() * Ke0 * ue)(0, 0);
}

void FEMSolver::assembleGlobal() {
    int nNodes = static_cast<int>(mesh_.nodes.size());
    numDofs_ = nNodes * 3;
    rebuildCachesIfNeeded();

    int nElem = static_cast<int>(mesh_.elements.size());
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(nElem * 24 * 24);

    for (int e = 0; e < nElem; e++) {
        const auto& Ke0 = keCache_[e];
        const double scale = densityScaleForElement(e);
        auto& elem = mesh_.elements[e];

        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                int gi = elem.nodeIds[i] * 3;
                int gj = elem.nodeIds[j] * 3;
                for (int di = 0; di < 3; di++) {
                    for (int dj = 0; dj < 3; dj++) {
                        double val = Ke0(i * 3 + di, j * 3 + dj) * scale;
                        if (std::abs(val) > 1e-20) {
                            triplets.emplace_back(gi + di, gj + dj, val);
                        }
                    }
                }
            }
        }
    }

    K_.resize(numDofs_, numDofs_);
    K_.setFromTriplets(triplets.begin(), triplets.end());
}

void FEMSolver::applyBCs() {
    F_ = Eigen::VectorXd::Zero(numDofs_);
    fixedDof_.assign(numDofs_, false);

    double bigNum = 1e20 * std::max(mat_.E, 1.0);

    for (auto& bc : loadCase_.conditions) {
        if (bc.type == 0) {
            for (int nid : bc.nodeIds) {
                if (nid < 0 || nid * 3 + 2 >= numDofs_) continue;
                if (bc.fixX) fixedDof_[nid * 3] = true;
                if (bc.fixY) fixedDof_[nid * 3 + 1] = true;
                if (bc.fixZ) fixedDof_[nid * 3 + 2] = true;
            }
        } else if (bc.type == 1) {
            for (int nid : bc.nodeIds) {
                if (nid < 0 || nid * 3 + 2 >= numDofs_) continue;
                if (bc.fixX) {
                    fixedDof_[nid * 3] = true;
                    F_(nid * 3) = bigNum * bc.valX;
                }
                if (bc.fixY) {
                    fixedDof_[nid * 3 + 1] = true;
                    F_(nid * 3 + 1) = bigNum * bc.valY;
                }
                if (bc.fixZ) {
                    fixedDof_[nid * 3 + 2] = true;
                    F_(nid * 3 + 2) = bigNum * bc.valZ;
                }
            }
        } else if (bc.type == 2 || bc.type == 3 || bc.type == 4) {
            for (int nid : bc.nodeIds) {
                if (nid < 0 || nid * 3 + 2 >= numDofs_) continue;
                F_(nid * 3)     += bc.valX;
                F_(nid * 3 + 1) += bc.valY;
                F_(nid * 3 + 2) += bc.valZ;
            }
        }
    }

    for (int i = 0; i < numDofs_; i++) {
        if (fixedDof_[i]) {
            K_.coeffRef(i, i) += bigNum;
        }
    }
}

bool FEMSolver::hasRegularHexMesh() const {
    return !mesh_.elements.empty();
}

bool FEMSolver::solveWithConfiguredBackend() {
    std::unique_ptr<FEMSolverBackend> backend;
    bool attemptedGpu = false;

    if (config_.gpuEnabled &&
        (config_.backend == SolverBackend::Auto || config_.backend == SolverBackend::GPU_AmgX)) {
        attemptedGpu = true;
        backend = std::make_unique<GpuAmgXSolverBackend>(config_);
        if (backend->solve(*this, result_)) {
            return true;
        }
        if (!config_.fallbackToCpu || config_.backend == SolverBackend::GPU_AmgX) {
            return false;
        }
        result_.usedFallback = true;
    }

    backend = std::make_unique<CpuEigenSolverBackend>();
    bool ok = backend->solve(*this, result_);
    if (attemptedGpu && ok) {
        result_.usedFallback = true;
        if (!result_.solverMessage.empty()) {
            result_.solverMessage += " (after GPU fallback)";
        }
    }
    return ok;
}

void FEMSolver::computeResults() {
    int nNodes = static_cast<int>(mesh_.nodes.size());
    int nElem = static_cast<int>(mesh_.elements.size());

    result_.dispX.resize(nNodes);
    result_.dispY.resize(nNodes);
    result_.dispZ.resize(nNodes);
    for (int i = 0; i < nNodes; i++) {
        result_.dispX[i] = U_(i * 3);
        result_.dispY[i] = U_(i * 3 + 1);
        result_.dispZ[i] = U_(i * 3 + 2);
    }

    result_.strainEnergy.resize(nElem);
    result_.vonMises.resize(nElem);
    result_.compliance = 0.0;

    for (int e = 0; e < nElem; e++) {
        double ce = elementStrainEnergyFromReferenceKe(e);
        result_.strainEnergy[e] = ce;
        result_.compliance += densityScaleForElement(e) * ce;

        auto ue = elementDisp(e);
        auto se = evalShape(0.0, 0.0, 0.0);
        double coords[8][3];
        auto& elem = mesh_.elements[e];
        for (int i = 0; i < 8; i++) {
            int nid = elem.nodeIds[i];
            coords[i][0] = mesh_.nodes[nid].x;
            coords[i][1] = mesh_.nodes[nid].y;
            coords[i][2] = mesh_.nodes[nid].z;
        }
        double detJ = 0.0;
        auto B = computeB(se, coords, detJ);
        Eigen::Matrix<double, 6, 1> strain = B * ue;
        Eigen::Matrix<double, 6, 1> stress = D_ * strain;

        double s11 = stress(0), s22 = stress(1), s33 = stress(2);
        double s12 = stress(3), s23 = stress(4), s13 = stress(5);
        double vm = std::sqrt(0.5 * ((s11-s22)*(s11-s22) + (s22-s33)*(s22-s33) + (s33-s11)*(s33-s11)
                    + 6.0 * (s12*s12 + s23*s23 + s13*s13)));
        result_.vonMises[e] = vm;
    }
}

bool FEMSolver::solve() {
    result_ = FEResultData{};

    if (mesh_.nodes.empty() || mesh_.elements.empty()) {
        result_.converged = false;
        result_.solverMessage = "Empty mesh";
        return false;
    }

    if (D_.isZero(0)) {
        D_ = constitutiveD();
    }

    bool ok = solveWithConfiguredBackend();
    if (!ok) {
        result_.converged = false;
        if (result_.backendUsed.empty()) {
            result_.backendUsed = "unavailable";
        }
        return false;
    }

    computeResults();
    result_.converged = true;
    return true;
}

} // namespace TopOpt
