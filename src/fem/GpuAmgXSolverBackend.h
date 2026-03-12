#pragma once

#include "../execution/NodeData.h"
#include "FEMSolver.h"

namespace TopOpt {

class GpuAmgXSolverBackend : public FEMSolverBackend {
public:
    explicit GpuAmgXSolverBackend(const FESolverConfig& config) : config_(config) {}

    bool solve(FEMSolver& solver, FEResultData& result) override;
    const char* name() const override { return "gpu-amgx"; }

private:
    FESolverConfig config_;
};

} // namespace TopOpt
