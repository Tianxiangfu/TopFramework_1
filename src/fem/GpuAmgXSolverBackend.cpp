#include "GpuAmgXSolverBackend.h"

#if defined(TOPFRAME_ENABLE_AMGX)
#include <amgx_c.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

#include <chrono>
#include <filesystem>
#include <memory>
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

AmgxScope& globalAmgxScope() {
    static AmgxScope scope;
    return scope;
}

void checkAmgx(AMGX_RC rc, const char* call) {
    if (rc == AMGX_RC_OK) {
        return;
    }
    std::ostringstream os;
    os << call << " failed with code " << static_cast<int>(rc);
    throw std::runtime_error(os.str());
}

class CachedAmgxSession {
public:
    CachedAmgxSession() = default;

    ~CachedAmgxSession() {
        reset();
    }

    void ensure(const std::filesystem::path& configPath,
                const std::string& overrides,
                int rows,
                int nnz) {
        const bool mustRebuild =
            !cfg_ ||
            configPath_ != configPath ||
            overrides_ != overrides ||
            rows_ != rows ||
            nnz_ != nnz;

        if (!mustRebuild) {
            return;
        }

        reset();

        configPath_ = configPath;
        overrides_ = overrides;
        rows_ = rows;
        nnz_ = nnz;

        checkAmgx(AMGX_config_create_from_file(&cfg_, configPath.string().c_str()), "AMGX_config_create_from_file");
        checkAmgx(AMGX_config_add_parameters(&cfg_, overrides.c_str()), "AMGX_config_add_parameters");
        checkAmgx(AMGX_resources_create_simple(&rsrc_, cfg_), "AMGX_resources_create_simple");
        checkAmgx(AMGX_matrix_create(&A_, rsrc_, AMGX_mode_dDDI), "AMGX_matrix_create");
        checkAmgx(AMGX_vector_create(&b_, rsrc_, AMGX_mode_dDDI), "AMGX_vector_create(b)");
        checkAmgx(AMGX_vector_create(&x_, rsrc_, AMGX_mode_dDDI), "AMGX_vector_create(x)");
        checkAmgx(AMGX_solver_create(&solver_, rsrc_, AMGX_mode_dDDI, cfg_), "AMGX_solver_create");
    }

    void reset() {
        if (solver_) AMGX_solver_destroy(solver_);
        if (x_) AMGX_vector_destroy(x_);
        if (b_) AMGX_vector_destroy(b_);
        if (A_) AMGX_matrix_destroy(A_);
        if (rsrc_) AMGX_resources_destroy(rsrc_);
        if (cfg_) AMGX_config_destroy(cfg_);

        solver_ = nullptr;
        x_ = nullptr;
        b_ = nullptr;
        A_ = nullptr;
        rsrc_ = nullptr;
        cfg_ = nullptr;
        rows_ = 0;
        nnz_ = 0;
        configPath_.clear();
        overrides_.clear();
    }

    AMGX_matrix_handle matrix() const { return A_; }
    AMGX_vector_handle rhs() const { return b_; }
    AMGX_vector_handle solution() const { return x_; }
    AMGX_solver_handle solver() const { return solver_; }

private:
    std::filesystem::path configPath_;
    std::string overrides_;
    int rows_ = 0;
    int nnz_ = 0;
    AMGX_config_handle cfg_ = nullptr;
    AMGX_resources_handle rsrc_ = nullptr;
    AMGX_matrix_handle A_ = nullptr;
    AMGX_vector_handle b_ = nullptr;
    AMGX_vector_handle x_ = nullptr;
    AMGX_solver_handle solver_ = nullptr;
};

CachedAmgxSession& cachedSession() {
    static CachedAmgxSession session;
    return session;
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
        (void)globalAmgxScope();

        const std::string overrides = normalizeConfigOverrides(config_);
        CachedAmgxSession& session = cachedSession();
        session.ensure(configPath, overrides, rows, nnz);

        solver.U_ = Eigen::VectorXd::Zero(rows);

        const auto* rowPtr = solver.K_.outerIndexPtr();
        const auto* colIdx = solver.K_.innerIndexPtr();
        const auto* values = solver.K_.valuePtr();

        TOPFRAME_AMGX_CALL(AMGX_matrix_upload_all(
            session.matrix(),
            rows,
            nnz,
            1,
            1,
            rowPtr,
            colIdx,
            values,
            nullptr
        ));

        TOPFRAME_AMGX_CALL(AMGX_vector_bind(session.rhs(), session.matrix()));
        TOPFRAME_AMGX_CALL(AMGX_vector_bind(session.solution(), session.matrix()));
        TOPFRAME_AMGX_CALL(AMGX_vector_upload(session.rhs(), rows, 1, solver.F_.data()));
        TOPFRAME_AMGX_CALL(AMGX_vector_upload(session.solution(), rows, 1, solver.U_.data()));

        TOPFRAME_AMGX_CALL(AMGX_solver_setup(session.solver(), session.matrix()));
        TOPFRAME_AMGX_CALL(AMGX_solver_solve(session.solver(), session.rhs(), session.solution()));
        TOPFRAME_AMGX_CALL(AMGX_vector_download(session.solution(), solver.U_.data()));

        AMGX_SOLVE_STATUS status = AMGX_SOLVE_FAILED;
        TOPFRAME_AMGX_CALL(AMGX_solver_get_status(session.solver(), &status));
        if (status != AMGX_SOLVE_SUCCESS) {
            result.converged = false;
            result.solverMessage =
                std::string("AmgX solve did not converge: ") + statusToString(status);
        } else {
            result.converged = true;
            result.solverMessage = "Solved with NVIDIA AmgX";
        }

        TOPFRAME_AMGX_CALL(AMGX_solver_get_iterations_number(session.solver(), &result.iterationCount));

        const auto end = std::chrono::steady_clock::now();
        result.solveTimeMs =
            std::chrono::duration<double, std::milli>(end - start).count();
        result.residualNorm = (solver.K_ * solver.U_ - solver.F_).norm();
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
