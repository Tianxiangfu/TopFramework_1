#include "GpuAmgXSolverBackend.h"

#if defined(TOPFRAME_ENABLE_AMGX)
#include <amgx_c.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <sstream>

namespace TopOpt {

namespace {

std::filesystem::path executableDirectory() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return std::filesystem::path(buffer).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

std::filesystem::path defaultConfigPath() {
#ifdef TOPFRAME_DEFAULT_AMGX_CONFIG_REL
    return executableDirectory() / TOPFRAME_DEFAULT_AMGX_CONFIG_REL;
#else
    return {};
#endif
}

std::string normalizeConfigOverrides(const FESolverConfig& config) {
    std::ostringstream os;
    os << "config_version=2,solver:max_iters=" << config.maxIterations
       << ",solver:tolerance=" << config.tolerance;
    return os.str();
}

#if defined(TOPFRAME_ENABLE_AMGX)
class AmgxScope {
public:
    AmgxScope() {
        AMGX_initialize();
        AMGX_initialize_plugins();
        AMGX_install_signal_handler();
    }

    ~AmgxScope() {
        AMGX_finalize_plugins();
        AMGX_finalize();
    }

    AmgxScope(const AmgxScope&) = delete;
    AmgxScope& operator=(const AmgxScope&) = delete;
};

void checkAmgx(AMGX_RC rc, const char* call) {
    if (rc == AMGX_RC_OK) {
        return;
    }
    std::ostringstream os;
    os << call << " failed with code " << static_cast<int>(rc);
    throw std::runtime_error(os.str());
}

const char* statusToString(AMGX_SOLVE_STATUS status) {
    switch (status) {
    case AMGX_SOLVE_SUCCESS:
        return "success";
    case AMGX_SOLVE_FAILED:
        return "failed";
    case AMGX_SOLVE_DIVERGED:
        return "diverged";
    default:
        return "unknown";
    }
}
#endif

} // namespace

#if defined(TOPFRAME_ENABLE_AMGX)
#define TOPFRAME_AMGX_CALL(expr) checkAmgx((expr), #expr)
#endif

bool GpuAmgXSolverBackend::solve(FEMSolver& solver, FEResultData& result) {
    result.backendUsed = name();
    result.iterationCount = 0;
    result.residualNorm = 0.0;

#if defined(TOPFRAME_ENABLE_AMGX)
    const std::filesystem::path configPath =
        !config_.amgxConfigPath.empty()
            ? std::filesystem::path(config_.amgxConfigPath)
            : defaultConfigPath();

    if (configPath.empty() || !std::filesystem::exists(configPath)) {
        result.converged = false;
        result.solverMessage = "AmgX config not found: " + configPath.string();
        return false;
    }

    solver.assembleGlobal();
    solver.applyBCs();
    solver.K_.makeCompressed();

    const int rows = solver.K_.rows();
    const int nnz = solver.K_.nonZeros();
    if (rows <= 0 || nnz <= 0) {
        result.converged = false;
        result.solverMessage = "AmgX backend received an empty linear system";
        return false;
    }

    const auto start = std::chrono::steady_clock::now();

    try {
        AmgxScope amgxScope;

        AMGX_config_handle cfg = nullptr;
        AMGX_resources_handle rsrc = nullptr;
        AMGX_matrix_handle A = nullptr;
        AMGX_vector_handle b = nullptr;
        AMGX_vector_handle x = nullptr;
        AMGX_solver_handle amgxSolver = nullptr;
        auto cleanup = [&]() {
            if (amgxSolver) AMGX_solver_destroy(amgxSolver);
            if (x) AMGX_vector_destroy(x);
            if (b) AMGX_vector_destroy(b);
            if (A) AMGX_matrix_destroy(A);
            if (rsrc) AMGX_resources_destroy(rsrc);
            if (cfg) AMGX_config_destroy(cfg);
        };

        const std::string overrides = normalizeConfigOverrides(config_);

        TOPFRAME_AMGX_CALL(AMGX_config_create_from_file(&cfg, configPath.string().c_str()));
        TOPFRAME_AMGX_CALL(AMGX_config_add_parameters(&cfg, overrides.c_str()));
        TOPFRAME_AMGX_CALL(AMGX_resources_create_simple(&rsrc, cfg));
        TOPFRAME_AMGX_CALL(AMGX_matrix_create(&A, rsrc, AMGX_mode_dDDI));
        TOPFRAME_AMGX_CALL(AMGX_vector_create(&b, rsrc, AMGX_mode_dDDI));
        TOPFRAME_AMGX_CALL(AMGX_vector_create(&x, rsrc, AMGX_mode_dDDI));
        TOPFRAME_AMGX_CALL(AMGX_solver_create(&amgxSolver, rsrc, AMGX_mode_dDDI, cfg));

        solver.U_ = Eigen::VectorXd::Zero(rows);

        const auto* rowPtr = solver.K_.outerIndexPtr();
        const auto* colIdx = solver.K_.innerIndexPtr();
        const auto* values = solver.K_.valuePtr();

        TOPFRAME_AMGX_CALL(AMGX_matrix_upload_all(
            A,
            rows,
            nnz,
            1,
            1,
            rowPtr,
            colIdx,
            values,
            nullptr
        ));

        TOPFRAME_AMGX_CALL(AMGX_vector_bind(b, A));
        TOPFRAME_AMGX_CALL(AMGX_vector_bind(x, A));
        TOPFRAME_AMGX_CALL(AMGX_vector_upload(b, rows, 1, solver.F_.data()));
        TOPFRAME_AMGX_CALL(AMGX_vector_upload(x, rows, 1, solver.U_.data()));

        TOPFRAME_AMGX_CALL(AMGX_solver_setup(amgxSolver, A));
        TOPFRAME_AMGX_CALL(AMGX_solver_solve(amgxSolver, b, x));
        TOPFRAME_AMGX_CALL(AMGX_vector_download(x, solver.U_.data()));

        AMGX_SOLVE_STATUS status = AMGX_SOLVE_FAILED;
        TOPFRAME_AMGX_CALL(AMGX_solver_get_status(amgxSolver, &status));
        if (status != AMGX_SOLVE_SUCCESS) {
            result.converged = false;
            result.solverMessage =
                std::string("AmgX solve did not converge: ") + statusToString(status);
        } else {
            result.converged = true;
            result.solverMessage = "Solved with NVIDIA AmgX";
        }

        TOPFRAME_AMGX_CALL(AMGX_solver_get_iterations_number(amgxSolver, &result.iterationCount));

        const auto end = std::chrono::steady_clock::now();
        result.solveTimeMs =
            std::chrono::duration<double, std::milli>(end - start).count();
        result.residualNorm = (solver.K_ * solver.U_ - solver.F_).norm();

        cleanup();
        return result.converged;
    } catch (const std::exception& ex) {
        result.converged = false;
        result.solverMessage = std::string("AmgX backend failed: ") + ex.what();
        return false;
    }
#else
    result.converged = false;
    result.solverMessage =
        "AmgX backend unavailable: build without TOPFRAME_ENABLE_AMGX or AmgX dependency not found";
    return false;
#endif
}

} // namespace TopOpt
