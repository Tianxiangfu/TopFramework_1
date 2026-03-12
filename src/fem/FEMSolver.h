#pragma once
#include "../execution/NodeData.h"
#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <vector>

namespace TopOpt {

class FEMSolver {
public:
    FEMSolver();

    // Set problem data
    void setMesh(const FEMeshData& mesh);
    void setMaterial(const MaterialData& mat);
    void setLoadCase(const LoadCaseData& lc);

    // Optional: per-element density for SIMP (default all 1.0)
    void setDensities(const std::vector<double>& densities, double penalty = 3.0, double Emin = 1e-9);

    // Solve the system
    bool solve();

    // Get results
    const FEResultData& result() const { return result_; }

    // Get element stiffness matrix (reference, no density scaling)
    Eigen::Matrix<double, 24, 24> hex8Ke(int elemIdx) const;

    // Access assembled system (for sensitivity analysis)
    int numDofs() const { return numDofs_; }
    const Eigen::VectorXd& displacement() const { return U_; }

    // Element displacement vector for element elemIdx
    Eigen::Matrix<double, 24, 1> elementDisp(int elemIdx) const;

private:
    // Hex8 shape functions and derivatives at a gauss point
    struct ShapeEval {
        double N[8];
        double dNdxi[8], dNdeta[8], dNdzeta[8];
    };
    static ShapeEval evalShape(double xi, double eta, double zeta);

    // Compute Jacobian and B-matrix at a point
    Eigen::Matrix<double, 6, 24> computeB(const ShapeEval& se,
                                           const double coords[][3],
                                           double& detJ) const;

    // Material constitutive matrix (isotropic)
    Eigen::Matrix<double, 6, 6> constitutiveD() const;

    // Assemble global stiffness
    void assembleGlobal();

    // Apply boundary conditions
    void applyBCs();

    FEMeshData mesh_;
    MaterialData mat_;
    LoadCaseData loadCase_;

    // Per-element density (for SIMP)
    std::vector<double> densities_;
    double penalty_ = 3.0;
    double Emin_ = 1e-9;
    bool useDensity_ = false;

    int numDofs_ = 0;
    Eigen::SparseMatrix<double> K_;
    Eigen::VectorXd F_;
    Eigen::VectorXd U_;
    FEResultData result_;

    // DOF mapping: node i -> dofs 3*i, 3*i+1, 3*i+2
    // Constrained DOFs
    std::vector<bool> fixedDof_;
};

} // namespace TopOpt
