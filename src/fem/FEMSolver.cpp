#include "FEMSolver.h"
#include <Eigen/SparseCholesky>
#include <cmath>
#include <algorithm>

namespace TopOpt {

FEMSolver::FEMSolver() {}

void FEMSolver::setMesh(const FEMeshData& mesh) { mesh_ = mesh; }
void FEMSolver::setMaterial(const MaterialData& mat) { mat_ = mat; }
void FEMSolver::setLoadCase(const LoadCaseData& lc) { loadCase_ = lc; }

void FEMSolver::setDensities(const std::vector<double>& densities, double penalty, double Emin) {
    densities_ = densities;
    penalty_ = penalty;
    Emin_ = Emin;
    useDensity_ = true;
}

// ================================================================
//  Hex8 shape functions at (xi, eta, zeta) in [-1,1]^3
// ================================================================

FEMSolver::ShapeEval FEMSolver::evalShape(double xi, double eta, double zeta) {
    ShapeEval se{};
    // Node ordering: standard hex8
    // N_i = 1/8 * (1 +/- xi)(1 +/- eta)(1 +/- zeta)
    double xim[2] = {1.0 - xi, 1.0 + xi};
    double etm[2] = {1.0 - eta, 1.0 + eta};
    double zem[2] = {1.0 - zeta, 1.0 + zeta};

    // Node corners: (-,-,-), (+,-,-), (+,+,-), (-,+,-), (-,-,+), (+,-,+), (+,+,+), (-,+,+)
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

// ================================================================
//  Constitutive matrix D (3D isotropic)
// ================================================================

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

// ================================================================
//  B-matrix at a gauss point (strain-displacement)
// ================================================================

Eigen::Matrix<double, 6, 24> FEMSolver::computeB(
    const ShapeEval& se, const double coords[][3], double& detJ) const
{
    // Jacobian J = dX/dxi
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

    // dN/dx = Jinv * dN/dxi
    double dNdx[8], dNdy[8], dNdz[8];
    for (int i = 0; i < 8; i++) {
        dNdx[i] = Jinv(0,0)*se.dNdxi[i] + Jinv(0,1)*se.dNdeta[i] + Jinv(0,2)*se.dNdzeta[i];
        dNdy[i] = Jinv(1,0)*se.dNdxi[i] + Jinv(1,1)*se.dNdeta[i] + Jinv(1,2)*se.dNdzeta[i];
        dNdz[i] = Jinv(2,0)*se.dNdxi[i] + Jinv(2,1)*se.dNdeta[i] + Jinv(2,2)*se.dNdzeta[i];
    }

    // B-matrix (6x24)
    Eigen::Matrix<double, 6, 24> B = Eigen::Matrix<double, 6, 24>::Zero();
    for (int i = 0; i < 8; i++) {
        int c = i * 3;
        B(0, c)     = dNdx[i];                  // exx
        B(1, c + 1) = dNdy[i];                  // eyy
        B(2, c + 2) = dNdz[i];                  // ezz
        B(3, c)     = dNdy[i]; B(3, c + 1) = dNdx[i]; // gxy
        B(4, c + 1) = dNdz[i]; B(4, c + 2) = dNdy[i]; // gyz
        B(5, c)     = dNdz[i]; B(5, c + 2) = dNdx[i]; // gxz
    }
    return B;
}

// ================================================================
//  Element stiffness matrix Ke (24x24) for Hex8
// ================================================================

Eigen::Matrix<double, 24, 24> FEMSolver::hex8Ke(int elemIdx) const {
    auto& elem = mesh_.elements[elemIdx];
    double coords[8][3];
    for (int i = 0; i < 8; i++) {
        int nid = elem.nodeIds[i];
        coords[i][0] = mesh_.nodes[nid].x;
        coords[i][1] = mesh_.nodes[nid].y;
        coords[i][2] = mesh_.nodes[nid].z;
    }

    auto D = constitutiveD();

    // 2x2x2 Gauss quadrature
    static const double gp = 1.0 / std::sqrt(3.0);
    static const double gpts[2] = {-gp, gp};

    Eigen::Matrix<double, 24, 24> Ke = Eigen::Matrix<double, 24, 24>::Zero();

    for (int gi = 0; gi < 2; gi++) {
        for (int gj = 0; gj < 2; gj++) {
            for (int gk = 0; gk < 2; gk++) {
                auto se = evalShape(gpts[gi], gpts[gj], gpts[gk]);
                double detJ;
                auto B = computeB(se, coords, detJ);
                Ke += B.transpose() * D * B * std::abs(detJ); // weight=1 for 2-point rule
            }
        }
    }
    return Ke;
}

// ================================================================
//  Element displacement vector
// ================================================================

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

// ================================================================
//  Assemble global stiffness matrix
// ================================================================

void FEMSolver::assembleGlobal() {
    int nNodes = (int)mesh_.nodes.size();
    numDofs_ = nNodes * 3;

    // Estimate non-zeros: each Hex8 contributes 24*24 entries
    int nElem = (int)mesh_.elements.size();
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(nElem * 24 * 24);

    for (int e = 0; e < nElem; e++) {
        auto Ke = hex8Ke(e);

        // Apply density scaling for SIMP
        if (useDensity_ && e < (int)densities_.size()) {
            double rho = densities_[e];
            double scale = Emin_ + std::pow(rho, penalty_) * (1.0 - Emin_);
            Ke *= scale;
        }

        auto& elem = mesh_.elements[e];
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                int gi = elem.nodeIds[i] * 3;
                int gj = elem.nodeIds[j] * 3;
                for (int di = 0; di < 3; di++) {
                    for (int dj = 0; dj < 3; dj++) {
                        double val = Ke(i * 3 + di, j * 3 + dj);
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

// ================================================================
//  Apply boundary conditions (penalty method - large number)
// ================================================================

void FEMSolver::applyBCs() {
    F_ = Eigen::VectorXd::Zero(numDofs_);
    fixedDof_.assign(numDofs_, false);

    double bigNum = 1e20 * mat_.E;

    for (auto& bc : loadCase_.conditions) {
        if (bc.type == 0) {
            // Fixed support
            for (int nid : bc.nodeIds) {
                if (nid < 0 || nid * 3 + 2 >= numDofs_) continue;
                if (bc.fixX) { fixedDof_[nid * 3]     = true; }
                if (bc.fixY) { fixedDof_[nid * 3 + 1] = true; }
                if (bc.fixZ) { fixedDof_[nid * 3 + 2] = true; }
            }
        } else if (bc.type == 1) {
            // Prescribed displacement
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
        } else if (bc.type == 2) {
            // Point force
            for (int nid : bc.nodeIds) {
                if (nid < 0 || nid * 3 + 2 >= numDofs_) continue;
                F_(nid * 3)     += bc.valX;
                F_(nid * 3 + 1) += bc.valY;
                F_(nid * 3 + 2) += bc.valZ;
            }
        } else if (bc.type == 4) {
            // Body force (gravity) - distributed to all nodes
            // Simple lumped approach: each node gets volume/8 * rho * g
            // For now, apply uniform force to all nodes in the bc
            for (int nid : bc.nodeIds) {
                if (nid < 0 || nid * 3 + 2 >= numDofs_) continue;
                F_(nid * 3)     += bc.valX;
                F_(nid * 3 + 1) += bc.valY;
                F_(nid * 3 + 2) += bc.valZ;
            }
        }
        // type 3 (Pressure) handled similarly to point force for simplicity
        else if (bc.type == 3) {
            for (int nid : bc.nodeIds) {
                if (nid < 0 || nid * 3 + 2 >= numDofs_) continue;
                F_(nid * 3)     += bc.valX;
                F_(nid * 3 + 1) += bc.valY;
                F_(nid * 3 + 2) += bc.valZ;
            }
        }
    }

    // Apply fixed DOFs using penalty method
    for (int i = 0; i < numDofs_; i++) {
        if (fixedDof_[i]) {
            K_.coeffRef(i, i) += bigNum;
            // If no prescribed displacement was set, F stays 0 (zero displacement)
        }
    }
}

// ================================================================
//  Solve
// ================================================================

bool FEMSolver::solve() {
    if (mesh_.nodes.empty() || mesh_.elements.empty()) {
        result_.converged = false;
        return false;
    }

    assembleGlobal();
    applyBCs();

    K_.makeCompressed();

    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
    solver.compute(K_);

    if (solver.info() != Eigen::Success) {
        result_.converged = false;
        return false;
    }

    U_ = solver.solve(F_);
    if (solver.info() != Eigen::Success) {
        result_.converged = false;
        return false;
    }

    // Extract results
    int nNodes = (int)mesh_.nodes.size();
    int nElem = (int)mesh_.elements.size();

    result_.dispX.resize(nNodes);
    result_.dispY.resize(nNodes);
    result_.dispZ.resize(nNodes);
    for (int i = 0; i < nNodes; i++) {
        result_.dispX[i] = U_(i * 3);
        result_.dispY[i] = U_(i * 3 + 1);
        result_.dispZ[i] = U_(i * 3 + 2);
    }

    // Compute per-element strain energy and compliance
    auto D = constitutiveD();
    result_.strainEnergy.resize(nElem);
    result_.vonMises.resize(nElem);
    result_.compliance = 0;

    for (int e = 0; e < nElem; e++) {
        auto ue = elementDisp(e);
        auto Ke0 = hex8Ke(e); // unscaled Ke
        double ce = ue.transpose() * Ke0 * ue;
        result_.strainEnergy[e] = ce;

        // Apply density scaling to compliance
        if (useDensity_ && e < (int)densities_.size()) {
            double rho = densities_[e];
            double scale = Emin_ + std::pow(rho, penalty_) * (1.0 - Emin_);
            result_.compliance += scale * ce;
        } else {
            result_.compliance += ce;
        }

        // Von Mises: compute at element center (0,0,0)
        auto se = evalShape(0.0, 0.0, 0.0);
        double coords[8][3];
        auto& elem = mesh_.elements[e];
        for (int i = 0; i < 8; i++) {
            int nid = elem.nodeIds[i];
            coords[i][0] = mesh_.nodes[nid].x;
            coords[i][1] = mesh_.nodes[nid].y;
            coords[i][2] = mesh_.nodes[nid].z;
        }
        double detJ;
        auto B = computeB(se, coords, detJ);
        Eigen::Matrix<double, 6, 1> strain = B * ue;
        Eigen::Matrix<double, 6, 1> stress = D * strain;

        double s11 = stress(0), s22 = stress(1), s33 = stress(2);
        double s12 = stress(3), s23 = stress(4), s13 = stress(5);
        double vm = std::sqrt(0.5 * ((s11-s22)*(s11-s22) + (s22-s33)*(s22-s33) + (s33-s11)*(s33-s11)
                    + 6.0 * (s12*s12 + s23*s23 + s13*s13)));
        result_.vonMises[e] = vm;
    }

    result_.converged = true;
    return true;
}

} // namespace TopOpt
