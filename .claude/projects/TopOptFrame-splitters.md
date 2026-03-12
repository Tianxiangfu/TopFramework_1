# TopOptFrame 分割条拖动功能实现

## 概述
在 Application.h 中添加了三个分割条状态变量，并在 Application.cpp 中实现了完整的拖动功能。

## 状态变量 (Application.h)

```cpp
// Horizontal splitter between left & center panels (left panel width ratio)
float horizontalSplitPos_ = 0.40f;

// Vertical splitter in left panel (3D view ratio vs resource browser)
float leftVerticalSplitPos_  = 0.65f;

// Vertical splitter in center panel (canvas ratio vs properties panel)
float centerVerticalSplitPos_ = 0.65f;
```

## 实现的拖动功能

### 1. 水平分割条 (左右面板之间)
- **状态变量**: `horizontalSplitPos_`
- **范围**: 0.25 ~ 0.55 (左面板占总宽度的25%~55%)
- **光标效果**: ResizeEW (左右调整)
- **拖动检测**: `ImGui::IsMouseDragging(ImGuiMouseButton_Left)`

```cpp
// 在 Application.cpp 第 250-263 行
if (ImGui::IsItemHovered()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
}
if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    float mouseDelta = ImGui::GetIO().MouseDelta.x;
    float newRatio = horizontalSplitPos_ + mouseDelta / totalW;
    horizontalSplitPos_ = ImClamp(newRatio, 0.25f, 0.55f);
}
```

### 2. 垂直分割条 - 左面板 (3D视图 vs 资源浏览器)
- **状态变量**: `leftVerticalSplitPos_`
- **范围**: 0.2 ~ 0.8 (3D视图占左面板的20%~80%)
- **光标效果**: ResizeNS (上下调整)
- **拖动检测**: `ImGui::IsMouseDragging(ImGuiMouseButton_Left)`

```cpp
// 在 Application.cpp 第 213-221 行
if (ImGui::IsItemHovered()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
}
if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    float mouseDelta = ImGui::GetIO().MouseDelta.y;
    float newSplitPos = leftVerticalSplitPos_ + mouseDelta / totalH;
    leftVerticalSplitPos_ = ImClamp(newSplitPos, 0.2f, 0.8f);
}
```

### 3. 垂直分割条 - 中心面板 (画布 vs 属性面板)
- **状态变量**: `centerVerticalSplitPos_`
- **范围**: 0.2 ~ 0.8 (画布占中心面板的20%~80%)
- **光标效果**: ResizeNS (上下调整)
- **拖动检测**: `ImGui::IsMouseDragging(ImGuiMouseButton_Left)`

```cpp
// 在 Application.cpp 第 310-318 行
if (ImGui::IsItemHovered()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
}
if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    float mouseDelta = ImGui::GetIO().MouseDelta.y;
    float newSplitPos = centerVerticalSplitPos_ + mouseDelta / centerH;
    centerVerticalSplitPos_ = ImClamp(newSplitPos, 0.2f, 0.8f);
}
```

## 改进要点

### 1. **正确的ImGui事件处理**
   - 在 `ImGui::EndChild()` 之后立即调用 `IsItemHovered()` 和 `IsItemActive()`
   - 使用 `ImGui::IsMouseDragging()` 检查拖动状态，提高响应精度

### 2. **鼠标光标反馈**
   - 水平分割条: `ImGuiMouseCursor_ResizeEW` (←→)
   - 垂直分割条: `ImGuiMouseCursor_ResizeNS` (↕)
   - 提升用户体验

### 3. **约束范围**
   - 水平分割: 25% ~ 55%
   - 垂直分割: 20% ~ 80%
   - 防止面板过小或过大

### 4. **实时计算**
   - 基于鼠标移动delta动态计算新位置
   - 使用 `ImClamp()` 限制在有效范围内

## View Menu 集成
在 View 菜单中添加了左面板宽度的滑块控制：
```cpp
// 在 Application.cpp 第 433-439 行
if (ImGui::BeginMenu("View")) {
    float pct = horizontalSplitPos_ * 100.0f;
    if (ImGui::SliderFloat("Left Panel", &pct, 25.0f, 55.0f, "%.0f%%")) {
        horizontalSplitPos_ = pct / 100.0f;
    }
    ImGui::EndMenu();
}
```

## 注意事项
- 所有分割条状态在运行时持久化（由ImGui的 imgui_layout.ini 负责）
- 拖动功能仅在左键拖动时激活
- 约束范围设置防止面板尺寸不合理
