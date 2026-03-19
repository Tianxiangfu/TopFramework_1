# TopOptFrame 项目记忆

## 项目结构
- `/src/app/Application.h|cpp` - 主窗口和 ImGui 循环
- `/src/panels/` - UI 面板组件
- `/src/node_editor/` - 节点编辑器
- `/src/utils/` - 工具类

## 分割条功能
- `Application` 中已有三组 splitter 状态：
  - `horizontalSplitPos_`
  - `leftVerticalSplitPos_`
  - `centerVerticalSplitPos_`
- 左右、左侧上下、中心上下分栏拖拽功能已实现。

## 编译信息
- 项目可通过 CMake 正常配置和编译。
- 如遇 `FetchContent` 下载失败，优先判断为网络/权限问题，不是源码语法错误。

## GPU AmgX 现状
- `D:\AmgX` 是当前本机 AmgX 的安装/构建位置。
- `build-gpu/` 是当前用于 GPU AmgX 验证的有效构建目录。
- `build/` 是普通构建目录，不适合作为 GPU 验证入口。
- `build-gpu/Debug/topframe_gpu_amgx_smoke.exe` 已多次验证真实跑通 `gpu-amgx`。
- UI 里 `topo-simp` 的 GPU 路径仍需单独验证，不应仅凭普通 `build/` 中的报错判断 GPU 不可用。

## 2026-03-19 最新记忆
- 最新普通版可执行文件：
  - `build/Debug/TopOptFrameC.exe`
  - `build/Release/TopOptFrameC.exe`
- 最新 GPU 版可执行文件：
  - `build-gpu/Debug/TopOptFrameC.exe`
  - `build-gpu/Release/TopOptFrameC.exe`
- 如果 UI 中选择 `Backend = GPU AmgX` 后报：
  - `AmgX backend unavailable: build without TOPFRAME_ENABLE_AMGX or AmgX dependency not found`
  通常说明运行的是 `build/` 下的普通版，而不是 `build-gpu/` 下的 GPU 版。
- 当前最可信的 GPU 验证入口仍然是：
  - `build-gpu/Debug/topframe_gpu_amgx_smoke.exe`
- 当前点击节点预览的现状：
  - `topo-simp` / `topo-beso` / `fea-solver` 在代码中被显式跳过，不会因为选中节点就自动 live preview
  - `domain-box` 和 `post-density-view` 理论上仍应支持点击预览，如果无反应需要单独排查
- 为了历史对比，另外保留了最初版本的独立构建目录：
  - `build-root-debug/`
  - `build-root-release/`
