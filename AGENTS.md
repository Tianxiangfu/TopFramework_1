# 仓库指南

## 项目结构与模块组织
本仓库是一个基于 CMake、GLFW、ImGui 和 OpenGL 的 C++ 桌面应用（`TopOptFrameC`）。

- `src/`：按功能划分的业务代码（`app`、`node_editor`、`execution`、`fem`、`panels`、`serialization`、`utils`、`commands`）。
- `model/`：3D 视图使用的示例 `.stl`/`.obj` 资源。
- `third_party/`：本地第三方依赖（如存在）。
- `cmake/`：CMake 辅助脚本。
- `build/`：本地普通构建目录，属于生成目录，不提交。
- `build-gpu/`：本地 GPU/AmgX 构建目录，属于生成目录，不提交。
- `build-root-debug/`、`build-root-release/`：本地历史对比构建目录，属于生成目录，不提交。

程序入口为 `src/main.cpp`。新增功能建议放在 `src/<feature>/` 下，并保持 `.h/.cpp` 成对组织。

## 构建、测试与开发命令
使用 `CMakePresets.json` 中的预设（Visual Studio 2019，x64）：

- `cmake --preset debug`：在 `build/` 目录生成 Debug 配置。
- `cmake --build --preset debug`：编译 Debug 版本。
- `cmake --preset release`：生成 Release 配置。
- `cmake --build --preset release`：编译 Release 版本。
- `cmake --preset debug-gpu`：在 `build-gpu/` 目录生成 GPU Debug 配置。
- `cmake --build --preset debug-gpu`：编译 GPU Debug 版本。
- `cmake --preset release-gpu`：在 `build-gpu/` 目录生成 GPU Release 配置。
- `cmake --build --preset release-gpu`：编译 GPU Release 版本。

Windows 下建议通过 IDE 运行；或在构建后直接执行对应目录下的 `TopOptFrameC.exe`。

## 代码风格与命名规范
- 语言标准：C++17（`CMAKE_CXX_STANDARD 17`）。
- 缩进：4 个空格，不使用 Tab。
- 类型/类名：`PascalCase`。
- 方法/变量：方法使用 `camelCase`；成员变量以下划线结尾。
- 常量/宏：适用时使用 `UPPER_SNAKE_CASE`。
- `#include` 分组顺序：项目头文件、第三方头文件、标准库头文件。
- 优先小而专注的源文件，避免单文件承载过多职责。

## 测试指南
当前 `src/` 下暂无一方自动化测试，也没有独立 `tests/` 目录。

- 对新增的核心逻辑（如 `fem`、`execution`）补充单元测试到 `tests/`。
- 测试命名建议：`FeatureName_WhenCondition_ThenExpected`。
- 引入测试后启用 CTest，并记录运行方式：`ctest --test-dir build -C Debug`。
- 提交前至少验证：Debug 可编译，关键 UI 流程可用（打开/保存工程、节点执行、模型加载）。

## 提交与合并请求规范
- Commit 格式：`type(scope): short summary`（例如 `feat(node_editor): add drag-drop node creation`）。
- 每次提交保持原子性，且可独立构建。
- PR 需包含：变更目的、关键修改点、验证步骤；涉及 UI 的变更附截图。
- 关联对应任务/Issue，并明确后续待办。
- 每次对代码进行修改后，必须推送到 `https://github.com/Tianxiangfu/TopFramework_1.git`。

### 生成物提交禁令
- 禁止上传任何编译后的文件，不仅仅是 `build/` 目录。
- 禁止上传任何构建目录，包括但不限于：`build/`、`build-gpu/`、`build-root-debug/`、`build-root-release/`。
- 禁止上传任何编译产物或二进制文件，包括但不限于：`.exe`、`.dll`、`.lib`、`.pdb`、`.obj`、`.ilk`。
- 提交前必须确认暂存区中不包含任何构建产物、生成目录或编译后的文件。

## 安全与配置提示
- 新代码避免硬编码机器相关路径（例如本地字体路径）。
- 若后续开发需要安装依赖、工具链、SDK 或其他运行组件，默认优先安装到 `D:` 盘；除非该软件强制要求安装到系统目录，否则不要优先使用 `C:` 盘路径。
- 不要提交大体积生成文件、二进制和本地配置缓存。
