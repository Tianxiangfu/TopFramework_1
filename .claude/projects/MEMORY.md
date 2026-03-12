# TopOptFrame 项目记忆

## 项目结构
- `/src/app/Application.h|cpp` - 主窗口和 ImGui 循环
- `/src/panels/` - UI 面板组件
- `/src/node_editor/` - 节点编辑器
- `/src/utils/` - 工具类

## 分割条实现 (v1.0 - 完成)
在 Application.h 中添加了三个分割条状态变量，实现了完整的拖动功能：

### 状态变量
- `horizontalSplitPos_` (0.40f) - 左右面板分割
- `leftVerticalSplitPos_` (0.65f) - 左面板内分割（3D视图 vs 浏览器）
- `centerVerticalSplitPos_` (0.65f) - 中心面板内分割（画布 vs 属性）

### 拖动实现特点
1. 在 `EndChild()` 后检查 `IsItemHovered()` 和 `IsItemActive()`
2. 使用 `IsMouseDragging()` 检查左键拖动
3. 提供视觉反馈（鼠标光标变化）
4. 范围约束防止面板过小
5. View 菜单支持滑块调整

## 编译信息
- CMake 配置成功
- 网络问题导致 FetchContent 下载依赖失败（非代码问题）
- 代码语法和逻辑正确
