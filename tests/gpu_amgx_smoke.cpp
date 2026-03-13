#include "../src/fem/FEMSolver.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

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

FEMeshData makeSingleHexMesh() {
    FEMeshData mesh;
    mesh.nodes = {
        {0, 0.0, 0.0, 0.0},
        {1, 1.0, 0.0, 0.0},
        {2, 1.0, 1.0, 0.0},
        {3, 0.0, 1.0, 0.0},
        {4, 0.0, 0.0, 1.0},
        {5, 1.0, 0.0, 1.0},
        {6, 1.0, 1.0, 1.0},
        {7, 0.0, 1.0, 1.0},
    };

    FEElement elem;
    elem.id = 0;
    elem.type = 0;
    elem.nodeIds = {0, 1, 2, 3, 4, 5, 6, 7};
    mesh.elements.push_back(elem);
    return mesh;
}

LoadCaseData makeLoadCase() {
    BCData fixed;
    fixed.type = 0;
    fixed.nodeIds = {0, 3, 4, 7};
    fixed.fixX = true;
    fixed.fixY = true;
    fixed.fixZ = true;

    BCData force;
    force.type = 2;
    force.nodeIds = {1, 2, 5, 6};
    force.fixX = false;
    force.fixY = false;
    force.fixZ = false;
    force.valY = -250.0;

    LoadCaseData lc;
    lc.name = "gpu-smoke";
    lc.conditions = {fixed, force};
    return lc;
}

std::filesystem::path defaultConfigPath() {
    return executableDirectory() / "configs" / "amgx" / "default.json";
}

} // namespace

} // namespace TopOpt

int main(int argc, char** argv) {
    using namespace TopOpt;

    const std::filesystem::path configPath =
        argc > 1 ? std::filesystem::path(argv[1]) : defaultConfigPath();

    MaterialData material;
    material.E = 210000.0;
    material.nu = 0.3;
    material.rho = 7850.0;

    FESolverConfig config;
    config.backend = SolverBackend::GPU_AmgX;
    config.gpuEnabled = true;
    config.fallbackToCpu = false;
    config.maxIterations = 1000;
    config.tolerance = 1e-6;
    config.amgxConfigPath = configPath.string();

    FEMSolver solver;
    solver.setMesh(makeSingleHexMesh());
    solver.setMaterial(material);
    solver.setLoadCase(makeLoadCase());
    solver.setConfig(config);

    const bool ok = solver.solve();
    const auto& result = solver.result();

    std::cout << "config=" << configPath.string() << "\n";
    std::cout << "converged=" << (ok ? "true" : "false") << "\n";
    std::cout << "backend=" << result.backendUsed << "\n";
    std::cout << "fallback=" << (result.usedFallback ? "true" : "false") << "\n";
    std::cout << "iterations=" << result.iterationCount << "\n";
    std::cout << "residual=" << result.residualNorm << "\n";
    std::cout << "time_ms=" << result.solveTimeMs << "\n";
    std::cout << "message=" << result.solverMessage << "\n";
    if (!result.dispY.empty()) {
        std::cout << "dispY_node_1=" << result.dispY[1] << "\n";
    }

    if (!ok) {
        return 1;
    }
    if (result.backendUsed != "gpu-amgx") {
        std::cerr << "unexpected backend: " << result.backendUsed << "\n";
        return 2;
    }
    if (result.usedFallback) {
        std::cerr << "unexpected CPU fallback\n";
        return 3;
    }
    return 0;
}
