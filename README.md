# TopOptFrameC

TopOptFrameC 是一个基于 C++17、CMake、GLFW、Dear ImGui、OpenGL 的桌面应用，用于搭建和执行 FEM / Topology Optimization 相关节点图流程。

当前仓库包含：
- 节点编辑器与属性面板
- 3D 视图与日志面板
- FEM 求解器
- TopOpt 求解流程
- 可选的 NVIDIA AmgX GPU 线性求解后端

## 项目结构

主要目录如下：

- `src/app/`：应用主循环与窗口管理
- `src/node_editor/`：节点编辑器、节点注册与交互逻辑
- `src/execution/`：节点执行、数据结构与图执行流程
- `src/fem/`：FEM 与 TopOpt 求解器
- `src/panels/`：各类 UI 面板
- `src/serialization/`：工程文件读写
- `src/utils/`：日志、文件对话框等工具
- `model/`：示例模型资源
- `configs/`：运行时配置文件
- `examples/`：示例工程
- `tests/`：当前包含 GPU AmgX 烟雾测试

程序入口位于 `src/main.cpp`。

## 功能概览

- 基于 ImGui / imnodes 的节点式界面
- FEM 求解
- TopOpt 求解
- 普通 CPU 求解路径
- 可选 GPU AmgX 求解路径
- 工程保存与加载

## 环境要求

推荐环境：

- Windows 10 / 11
- Visual Studio 2019，安装 C++ 桌面开发组件
- CMake 3.20 或更高
- 可用的 OpenGL 运行环境
- Git

项目依赖通过 CMake `FetchContent` 自动拉取，包括：

- GLFW
- Dear ImGui
- imnodes
- nlohmann/json
- Eigen 3.4

首次配置时需要联网下载这些依赖。

## 获取代码

```powershell
git clone https://github.com/Tianxiangfu/TopFramework_1.git
cd TopFramework_1
```

如果你已经在现有工作区内开发，可以跳过这一步。

## 普通版编译

普通版不启用 AmgX，适合日常 UI、CPU FEM、CPU TopOpt 开发与验证。

### Debug

```powershell
cmake --preset debug
cmake --build --preset debug
```

生成的可执行文件通常位于：

- `build/Debug/TopOptFrameC.exe`

### Release

```powershell
cmake --preset release
cmake --build --preset release
```

生成的可执行文件通常位于：

- `build/Release/TopOptFrameC.exe`

## GPU AmgX 版编译

如果需要启用 NVIDIA AmgX 后端，需要本机已经准备好 AmgX 与 CUDA 环境。

当前仓库的 GPU 预设会使用：

- `AMGX_ROOT=D:/AmgX`

因此你需要保证该目录下至少存在：

- `include/amgx_c.h`
- `lib/amgxsh.lib` 或 `lib/amgx.lib`
- `bin/amgxsh.dll`，或等效的 AmgX DLL

同时还需要：

- NVIDIA CUDA Toolkit
- 能与当前 AmgX 构建匹配的 MSVC 工具链

### Debug GPU

```powershell
cmake --preset debug-gpu
cmake --build --preset debug-gpu
```

生成的可执行文件通常位于：

- `build-gpu/Debug/TopOptFrameC.exe`

### Release GPU

```powershell
cmake --preset release-gpu
cmake --build --preset release-gpu
```

生成的可执行文件通常位于：

- `build-gpu/Release/TopOptFrameC.exe`

如果 CMake 输出中出现类似下面的信息，说明 AmgX 已被正确启用：

```text
AmgX enabled: D:/AmgX/lib/amgxsh.lib
```

如果没有启用，则 GPU 相关逻辑不会真正链接到 AmgX。

## 运行方式

编译完成后，可直接运行对应目录中的：

- `TopOptFrameC.exe`

建议：

- CPU / 普通功能验证时运行 `build/Debug/TopOptFrameC.exe`
- GPU AmgX 验证时运行 `build-gpu/Debug/TopOptFrameC.exe`

注意：普通版 `build/` 不是 GPU 验证入口。如果在普通版中选择 `Backend = GPU AmgX`，通常只会得到“未启用 AmgX”的报错。

## GPU 烟雾测试

仓库当前包含一个 GPU AmgX 烟雾测试目标：

- `topframe_gpu_amgx_smoke`

编译 GPU 版本后，可在对应输出目录运行：

```powershell
.\build-gpu\Debug\topframe_gpu_amgx_smoke.exe
```

该测试用于快速确认：

- AmgX 动态库是否可加载
- GPU 求解路径是否已正确连通

测试源码位于：

- `tests/gpu_amgx_smoke.cpp`

## 示例文件

仓库当前提供的示例工程文件位于 `examples/`，例如：

- `examples/cantilever_gpu_test.topopt`

你可以在程序中加载这些文件来验证节点图流程。

## 常见问题

### 1. 依赖下载失败

如果配置阶段 `FetchContent` 下载失败，通常是网络或权限问题，不一定是源码本身有误。

### 2. 选择 GPU AmgX 后提示不可用

如果日志中出现类似：

```text
AmgX backend unavailable: build without TOPFRAME_ENABLE_AMGX or AmgX dependency not found
```

通常说明当前运行的是普通构建 `build/` 下的程序，而不是 `build-gpu/` 下启用了 AmgX 的程序。

### 3. 找不到运行时配置

构建脚本会在编译后自动复制 `configs/` 到输出目录。若你手动移动可执行文件，可能会导致配置文件路径失效。

## 开发说明

- 语言标准：C++17
- 构建系统：CMake
- UI：GLFW + Dear ImGui + OpenGL
- 节点编辑：imnodes
- JSON：nlohmann/json
- 线性代数：Eigen

新增功能建议按模块放入 `src/<feature>/`，并保持 `.h` / `.cpp` 成对组织。

## 测试

当前仓库还没有完整的一方自动化测试体系，但建议至少完成以下验证：

- Debug 构建成功
- 应用可正常启动
- 工程可打开和保存
- 节点执行流程可运行
- GPU 场景下可通过 `topframe_gpu_amgx_smoke.exe` 做基础验证

如果后续启用 CTest，可使用类似命令运行：

```powershell
ctest --test-dir build -C Debug
```

## 许可证

当前仓库未在根目录看到明确的许可证文件。如需开源发布，建议补充 `LICENSE`。
