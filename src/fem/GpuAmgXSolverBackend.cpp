#include "GpuAmgXSolverBackend.h"

#if defined(TOPFRAME_ENABLE_AMGX)
#include <amgx_c.h>
#endif

namespace TopOpt {

bool GpuAmgXSolverBackend::solve(FEMSolver& /*solver*/, FEResultData& result) {
    result.backendUsed = name();
    result.iterationCount = 0;
    result.residualNorm = 0.0;

#if defined(TOPFRAME_ENABLE_AMGX)
    result.converged = false;
    result.solverMessage =
        "AmgX backend is enabled in build configuration but matrix-free integration is not completed yet";
    return false;
#else
    result.converged = false;
    result.solverMessage =
        "AmgX backend unavailable: build without TOPFRAME_ENABLE_AMGX or AmgX dependency not found";
    return false;
#endif
}

} // namespace TopOpt
