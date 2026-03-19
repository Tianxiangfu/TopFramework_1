# FEM 加速会话总结

## 目标
- 为 FEM/TopOpt 引入求解后端抽象。
- 为后续 `CUDA + AmgX` GPU 路径做准备。
- 保持现有 CPU 路径可用，并先做一批立即生效的加速。
- 将后端选择和求解参数接入节点参数与 UI 流程。

## 已完成内容
- 在 `src/execution/NodeData.h` 中新增了 `SolverBackend` 和 `FESolverConfig`。
- 扩展了 `FEResultData`，新增：
  - `backendUsed`
  - `iterationCount`
  - `solveTimeMs`
  - `usedFallback`
  - `residualNorm`
  - `solverMessage`
- 重构了 `src/fem/FEMSolver.*`：
  - 引入面向后端的求解流程
  - 增加基于 Eigen `SimplicialLDLT` 的 CPU 后端实现
  - 增加单元参考刚度矩阵 `Ke` 缓存
  - 在装配、compliance 和灵敏度相关计算中复用缓存的 `Ke`
- 新增 GPU 后端占位实现：
  - `src/fem/GpuAmgXSolverBackend.h`
  - `src/fem/GpuAmgXSolverBackend.cpp`
  - 当前行为：当 GPU 后端启用但不可用或未完成时，返回说明并回退到 CPU
- 改进了 `src/fem/TopOptSolver.*`：
  - 增加求解配置透传
  - 预计算滤波邻域
  - 去掉每次滤波时重复的全对全距离计算
  - 复用基于参考刚度缓存的单元应变能计算
- 更新了执行/UI 路径：
  - `src/execution/GraphExecutor.*`
  - `src/node_editor/NodeRegistry.cpp`
  - 新增节点参数，例如 `Backend`、`EnableGPU`、`FallbackToCPU`、`SolverMaxIter`、`SolverTol`、`AmgXPath`
- 更新了 `CMakeLists.txt`：
  - 增加可选开关 `TOPFRAME_ENABLE_AMGX`
  - 增加可选路径 `AMGX_ROOT`
  - 在缺少 AmgX 时仍可正常构建

## 验证情况
- 重构后 `cmake --build --preset debug` 已通过。
- 在当前环境检查中，没有发现可直接使用的 AmgX 头文件或库文件。
- 结论：GPU 后端目前未实际启用，真实求解仍然走 CPU 路径。

## 当前状态
- 代码结构上已经支持后端切换。
- 当前实际可见的性能收益主要来自：
  - CPU 侧 `Ke` 缓存
  - 滤波邻域预计算
- GPU 后端目前只是集成占位层，还不是可运行的 matrix-free AMG-PCG 实现。

## 下一步
- 如果后续拿到可用的 AmgX：
  - 将 `GpuAmgXSolverBackend` 接入真实的 AmgX 初始化与求解调用
  - 决定是先走“已装配稀疏矩阵的 GPU 求解”，还是直接进入 matrix-free 算子路径
- 如果后续仍然没有 AmgX：
  - 继续优化 CPU 路径
  - 或先实现 CPU 版 matrix-free / 迭代求解，作为后续 GPU 迁移的算法原型

## 关键提交
- 首次 FEM 后端重构与优化提交：
  - `644c8bc` `feat(fem): add backend abstraction and optimized topology solve`
## 2026-03-13 本次对话新增实现
- 实现了 `GpuAmgXSolverBackend` 的真实 GPU 求解路径骨架，不再是纯占位返回失败。
- 当前路线为“CPU 组装 + GPU AmgX 线性求解”，未实现 matrix-free，也未实现 GPU 组装。
- `GpuAmgXSolverBackend` 现已支持：
  - 读取 AmgX 配置文件路径
  - 创建 AmgX config/resources/matrix/vector/solver
  - 将 `FEMSolver` 组装后的 CSR 矩阵 `K_` 与向量 `F_` 上传到 AmgX
  - 执行 `setup/solve/download`
  - 将解写回 `solver.U_`
  - 回填 `FEResultData` 中的 `backendUsed`、`converged`、`iterationCount`、`residualNorm`、`solveTimeMs`、`solverMessage`

## 2026-03-13 本次构建与资源改动
- 更新了 `CMakeLists.txt`：
  - 通过 `AMGX_ROOT` 查找 AmgX `include/`、`lib/`、可选 `dll`
  - 启用 `find_package(CUDAToolkit REQUIRED)`
  - 为程序定义默认配置相对路径 `configs/amgx/default.json`
  - 构建后复制 `configs/` 到输出目录
  - 若发现 `amgx*.dll`，复制到可执行目录
- 新增默认配置文件：
  - `configs/amgx/default.json`

## 2026-03-13 本次验证结果
- 在当前机器上确认：
  - `CUDA_PATH` 已存在，指向 `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.4`
  - 未发现 `AMGX_ROOT`
  - 未确认到可直接使用的本机 AmgX 安装目录
- 在“无 AmgX 环境”下重新构建过 Debug：
  - CPU 路径未被破坏
  - `build/Debug/TopOptFrameC.exe` 可生成
  - `build/Debug/configs/amgx/default.json` 已成功复制

## 2026-03-13 当前限制
- 还没有在本机上把 AmgX 环境配置到可验证状态
- 还没有实际跑通一次 `GPU AmgX` 求解
- 当前仓库没有 `debug-gpu` preset
- 当前实现仍依赖现有 CPU 侧 `assembleGlobal()` 和 `applyBCs()`

## 2026-03-13 下一步
- 配置本机 `AMGX_ROOT`
- 确认 AmgX 安装目录中存在：
  - `include/amgx_c.h`
  - `lib/amgx*.lib`
  - `bin` 或 `lib/x64` 下的 `amgx*.dll`
- 重新运行 CMake 配置，确认输出中出现 `AmgX enabled: ...`
- 之后再验证节点图中 `Backend = GPU AmgX` 的真实求解路径

## 2026-03-13 关键提交
- 本次 AmgX 求解路径接入提交：
  - `24c9275` `feat(fem): implement AmgX solver path`

## 2026-03-13 GPU AmgX 环境打通与 UI 验证

### 本次已完成工作
- 将 `D:\AmgX` 固定为本机 AmgX 源码、构建与安装目录。
- 克隆了官方 `NVIDIA/AMGX` 仓库到 `D:\AmgX`。
- 由于当前机器环境为：
  - GPU: `NVIDIA GeForce RTX 4070`
  - 驱动: `560.94`
  - CUDA Toolkit: `11.4`
  - Visual Studio: `2019` / `2022`
  主线 `AMGX` 仓库要求 `CUDAToolkit >= 12.0`，因此切换到兼容 CUDA 11 的提交：
  - `e317f96` `Fixing build with CUDA 12.8. Fixed some warnings. Fixed deprecation of NVTX. Min version of CUDA changed to 11.`
- 初始化并同步了 `thrust` / `cub` / `libcudacxx` 子模块。
- 在 `D:\AmgX\build-vs2019-cuda11` 下，以 `Visual Studio 2019 + CUDA 11.4` 成功编出了核心共享库：
  - `D:\AmgX\build-vs2019-cuda11\Release\amgxsh.dll`
  - `D:\AmgX\build-vs2019-cuda11\Release\amgxsh.lib`
- 将运行所需产物整理到：
  - `D:\AmgX\bin\amgxsh.dll`
  - `D:\AmgX\lib\amgxsh.lib`
- 新建了带 AmgX 的独立构建目录：
  - `build-gpu/`
- 在 `build-gpu` 配置中确认 CMake 已输出：
  - `AmgX enabled: D:/AmgX/lib/amgxsh.lib`

### 本次新增代码与验证入口
- 新增无头 GPU 烟雾测试：
  - `tests/gpu_amgx_smoke.cpp`
- 更新 `CMakeLists.txt`：
  - 为主程序和烟雾测试统一复制 `configs/amgx/default.json`
  - 为目标复制 `amgxsh.dll`
  - 新增目标 `topframe_gpu_amgx_smoke`
- 新增可导入的节点图工程文件：
  - `examples/simp_solver_compare.json`
  - `examples/simp_solver_compare.topopt`
  - `examples/simp_gpu_quickcheck.topopt`
  - `examples/fea_gpu_quickcheck.topopt`
- 更新 `TopOptSolver.cpp`：
  - 在 `runSIMP()` 失败时保留内部 `FEMSolver` 的真实失败结果，而不是只返回默认空结果
- 更新 `GraphExecutor.cpp`：
  - 增加 `SIMP debug` 诊断日志，尝试在 UI 中直接暴露内部失败原因

### 本次命令行验证结果
- 在 `build-gpu/Debug/` 下运行：
  - `topframe_gpu_amgx_smoke.exe`
- 实测结果：
  - `converged=true`
  - `backend=gpu-amgx`
  - `fallback=false`
  - `iterations=1`
  - `residual=3.65315e-13`
  - `message=Solved with NVIDIA AmgX`
- 反向验证：
  - 传入不存在的配置文件路径时，程序会按预期报：
    - `AmgX config not found: ...`
  - 且不会回退到 CPU

### 本次 UI 端验证结果
- 使用 `build-gpu/Debug/TopOptFrameC.exe` 加载 `.topopt` 工程进行测试。
- 对 `simp_gpu_quickcheck.topopt` 的运行结果：
  - `topo-simp` 启动了，但立即失败
  - 日志持续显示：
    - `optimization failed, backend=cpu-direct, compliance=0, timeMs=0, residual=0`
  - `Density View` 仍显示完整盒体，说明优化失败后密度场没有产生有效更新
- 结论：
  - GPU AmgX 基础线性求解链路已经由 CLI 验证为可用
  - 但 UI 端 `topo-simp` 路径仍未打通，当前不能用 `SIMP` 节点证明 GPU 路径可用

### 当前怀疑点与可能问题
- `topframe_gpu_amgx_smoke.exe` 已证明：
  - `GpuAmgXSolverBackend` 本身不是完全不可用
  - `AMGX_ROOT`、`dll`、`config`、基础求解链路已经成立
- 当前未解决问题更可能位于：
  - `TopOptSolver::runSIMP()` 的失败路径
  - `topo-simp` 节点的求解配置透传
  - UI 实际运行的可执行文件是否始终为最新 `build-gpu/Debug/TopOptFrameC.exe`
- 用户多次贴出的 UI 日志没有出现新加的 `SIMP debug: solverMessage='...'` 行，这说明至少存在以下一种情况：
  - 用户运行的并非预期的最新 `build-gpu/Debug/TopOptFrameC.exe`
  - 或 `topo-simp` 的失败路径仍绕过了新增诊断日志
- 当前可信结论：
  - `CLI GPU AmgX = 已打通`
  - `UI FEA Solver / UI TopOpt = 尚未完成验证`

### 对后续排查的建议
- 优先使用 `examples/fea_gpu_quickcheck.topopt` 验证 UI 里的 `fea-solver` 节点，而不是继续直接用 `topo-simp`
- 如果 `fea-solver` 节点能显示：
  - `backend=gpu-amgx`
  - `fallback=false`
  则问题基本可定位到 `TopOptSolver::runSIMP()` 或其包装路径
- 若 `fea-solver` 节点也失败，再继续排查：
  - 节点参数读取
  - `FESolverConfig` 透传
  - `build-gpu` 实际运行路径

### 本次相关关键提交
- `7afe87d` `test(fem): add gpu amgx smoke coverage`
- `d3405c6` `test(examples): add simp solver comparison project`
- `d4faa8b` `test(examples): add topopt project variant`
- `f342e29` `test(examples): add gpu quickcheck project`
- `04608f7` `fix(topo): preserve fe solver failure details`
- `5ad9d49` `test(examples): add fea gpu quickcheck project`

## 2026-03-19 本次对话补充

### 本次确认的构建与运行结论
- 当前仓库最新提交为：
  - `aebd37f` `feat(ui): wire simp gpu amgx flow`
- 普通构建目录 `build/` 已重新配置并成功编译：
  - `build/Debug/TopOptFrameC.exe`
  - `build/Release/TopOptFrameC.exe`
- GPU 构建目录 `build-gpu/` 已重新配置并成功编译：
  - `build-gpu/Debug/TopOptFrameC.exe`
  - `build-gpu/Release/TopOptFrameC.exe`
- 本次重新确认 `build-gpu` 的 CMake 配置输出包含：
  - `AmgX enabled: D:/AmgX/lib/amgxsh.lib`

### 本次重新验证的 GPU 结果
- 重新编译并运行：
  - `build-gpu/Debug/topframe_gpu_amgx_smoke.exe`
- 实测输出再次确认：
  - `converged=true`
  - `backend=gpu-amgx`
  - `fallback=false`
  - `message=Solved with NVIDIA AmgX`
- 结论：
  - 当前机器上的 `D:/AmgX` 仍可用于构建最新代码的 GPU 版
  - `build-gpu` 下的可执行文件是当前有效的 GPU 运行入口

### 本次定位到的 UI 现状
- 用户在普通版 `build/Debug` 或 `build/Release` 中选择 `Backend = GPU AmgX` 时，日志报错：
  - `AmgX backend unavailable: build without TOPFRAME_ENABLE_AMGX or AmgX dependency not found`
- 该现象的原因已确认：
  - 普通 `build/` 构建未启用 AmgX
  - 因此只能跑 CPU，不能作为 GPU 验证入口
- 当前应使用：
  - `build-gpu/Debug/TopOptFrameC.exe`
  - 或 `build-gpu/Release/TopOptFrameC.exe`

### 本次关于“点击节点预览”的结论
- 当前代码中：
  - `topo-simp`
  - `topo-beso`
  - `fea-solver`
  这三类重计算节点在 `Application::updateLivePreview()` 中被显式跳过，不会因为“点击一下节点”就自动预览
- `domain-box` 与 `post-density-view` 理论上仍应支持点击预览
- 因此用户提到的“Box Domain / Density View 点击后无反应”问题，与这次回退/恢复版本无直接关系，后续需要单独排查 UI 预览链路

### 本次临时对比构建
- 为了排查历史行为，另外编译了仓库根提交 `d072ca8` 的独立构建：
  - `build-root-debug/Debug/TopOptFrameC.exe`
  - `build-root-release/Release/TopOptFrameC.exe`
- 该对比说明：
  - 仓库最初版本本身也能完成 Debug/Release 编译
  - 当前问题更像是运行行为差异，而不是“新版本编不出来”
