#include "View3DPanel.h"
#include "../utils/Logger.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TopOpt {

View3DPanel::View3DPanel() {}
View3DPanel::~View3DPanel() {}

// ================================================================
//  Perspective projection: 3D world -> 2D screen
// ================================================================
struct Vec3 { float x, y, z; };

static Vec3 rotateYX(float x, float y, float z, float yawRad, float pitchRad) {
    // Rotate around Y (yaw)
    float rx =  x * cosf(yawRad) + z * sinf(yawRad);
    float ry =  y;
    float rz = -x * sinf(yawRad) + z * cosf(yawRad);
    // Rotate around X (pitch)
    float fx = rx;
    float fy = ry * cosf(pitchRad) - rz * sinf(pitchRad);
    float fz = ry * sinf(pitchRad) + rz * cosf(pitchRad);
    return { fx, fy, fz };
}

static ImVec2 projectPerspective(float x, float y, float z,
                                  ImVec2 center, float scale,
                                  float yawRad, float pitchRad,
                                  float camDist, float fovFactor)
{
    Vec3 r = rotateYX(x, y, z, yawRad, pitchRad);
    // Perspective divide: objects further away appear smaller
    float depth = camDist + r.z;
    if (depth < 0.1f) depth = 0.1f;
    float perspScale = fovFactor / depth;
    return ImVec2(center.x + r.x * scale * perspScale,
                  center.y - r.y * scale * perspScale);
}

static float depthAfterRotation(float x, float y, float z, float yawRad, float pitchRad) {
    Vec3 r = rotateYX(x, y, z, yawRad, pitchRad);
    return r.z;
}

// Simple 3D dot product
static float dot3(const float* a, const float* b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static void normalize3(float* v) {
    float len = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 1e-8f) { v[0]/=len; v[1]/=len; v[2]/=len; }
}

static void cross3(const float* a, const float* b, float* out) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

// ================================================================
//  Main draw
// ================================================================
void View3DPanel::draw() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 10 || avail.y < 10) return;

    viewportWidth_  = (int)avail.x;
    viewportHeight_ = (int)avail.y;

    // Count FPS
    static float fpsTimer = 0.0f;
    static int   fpsCount = 0;
    fpsTimer += ImGui::GetIO().DeltaTime;
    fpsCount++;
    if (fpsTimer >= 0.5f) {
        fps_ = (float)fpsCount / fpsTimer;
        fpsTimer = 0.0f;
        fpsCount = 0;
    }

    ImGui::BeginChild("Viewport3D", avail, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();

    // ===================== Camera interaction =====================
    bool isHovered = ImGui::IsWindowHovered();
    bool isDragging = false;
    if (isHovered) {
        // Left drag: orbit
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            camYaw_   += ImGui::GetIO().MouseDelta.x * 0.4f;
            camPitch_ += ImGui::GetIO().MouseDelta.y * 0.4f;
            camPitch_ = ImClamp(camPitch_, -89.0f, 89.0f);
            isDragging = true;
        }
        // Right drag: also orbit (common convention)
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            camYaw_   += ImGui::GetIO().MouseDelta.x * 0.4f;
            camPitch_ += ImGui::GetIO().MouseDelta.y * 0.4f;
            camPitch_ = ImClamp(camPitch_, -89.0f, 89.0f);
            isDragging = true;
        }
        // Middle drag: pan
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            camCenter_[0] -= ImGui::GetIO().MouseDelta.x * 0.01f * camDistance_;
            camCenter_[1] += ImGui::GetIO().MouseDelta.y * 0.01f * camDistance_;
            isDragging = true;
        }
        // Scroll: zoom
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            camDistance_ *= (1.0f - wheel * 0.12f);
            camDistance_ = ImClamp(camDistance_, 0.5f, 100.0f);
        }
    }

    // ===================== Background gradient =====================
    // Neutral gray gradient matching reference style
    ImU32 bgTop    = IM_COL32(72, 75, 80, 255);
    ImU32 bgBottom = IM_COL32(155, 158, 162, 255);
    dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                                bgTop, bgTop, bgBottom, bgBottom);

    dl->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);

    // Projection center offset slightly upward (camera looks at target above ground)
    ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.55f);
    float baseScale = fminf(size.x, size.y) * 0.08f;
    float yr = camYaw_   * (float)M_PI / 180.0f;
    float pr = camPitch_ * (float)M_PI / 180.0f;
    float fovFactor = camDistance_ * 0.9f;

    // Compute eye position for info display
    float eyeX = camCenter_[0] + camDistance_ * cosf(pr) * sinf(yr);
    float eyeY = camCenter_[1] - camDistance_ * sinf(pr);
    float eyeZ = camCenter_[2] - camDistance_ * cosf(pr) * cosf(yr);

    // ===================== Ground grid (XZ plane, Y=0) =====================
    if (showGrid_) {
        int gridN = 20;
        float gridStep = 0.5f;
        float gridExtent = gridN * gridStep;

        // Dark gray grid lines matching reference
        for (int i = -gridN; i <= gridN; i++) {
            float fi = i * gridStep;
            // Fade out grid lines further from center
            float dist = fabsf((float)i) / (float)gridN;
            int alpha = (int)(150.0f * (1.0f - dist * 0.4f));
            if (i == 0) alpha = 0; // skip center lines (drawn as colored axes)
            if (alpha < 5) continue;
            ImU32 col = IM_COL32(50, 52, 58, alpha);

            ImVec2 a = projectPerspective(fi, 0, -gridExtent, center, baseScale, yr, pr, camDistance_, fovFactor);
            ImVec2 b = projectPerspective(fi, 0,  gridExtent, center, baseScale, yr, pr, camDistance_, fovFactor);
            dl->AddLine(a, b, col, 1.0f);
            a = projectPerspective(-gridExtent, 0, fi, center, baseScale, yr, pr, camDistance_, fovFactor);
            b = projectPerspective( gridExtent, 0, fi, center, baseScale, yr, pr, camDistance_, fovFactor);
            dl->AddLine(a, b, col, 1.0f);
        }

        // Colored axis lines on ground plane
        ImVec2 ox  = projectPerspective(0, 0, 0, center, baseScale, yr, pr, camDistance_, fovFactor);
        ImVec2 pxP = projectPerspective( gridExtent, 0, 0, center, baseScale, yr, pr, camDistance_, fovFactor);
        ImVec2 pxN = projectPerspective(-gridExtent, 0, 0, center, baseScale, yr, pr, camDistance_, fovFactor);
        ImVec2 pzP = projectPerspective(0, 0,  gridExtent, center, baseScale, yr, pr, camDistance_, fovFactor);
        ImVec2 pzN = projectPerspective(0, 0, -gridExtent, center, baseScale, yr, pr, camDistance_, fovFactor);

        // X axis: red (positive bright, negative dim)
        dl->AddLine(ox, pxP, IM_COL32(220, 60, 60, 180), 1.8f);
        dl->AddLine(ox, pxN, IM_COL32(220, 60, 60, 60),  1.0f);
        // Z axis: blue (positive bright, negative dim)
        dl->AddLine(ox, pzP, IM_COL32(60, 100, 220, 180), 1.8f);
        dl->AddLine(ox, pzN, IM_COL32(60, 100, 220, 60),  1.0f);

        // Y axis: green vertical line at origin
        if (showAxes_) {
            float yLen = 5.0f;
            ImVec2 pyP = projectPerspective(0,  yLen, 0, center, baseScale, yr, pr, camDistance_, fovFactor);
            ImVec2 pyN = projectPerspective(0, -yLen, 0, center, baseScale, yr, pr, camDistance_, fovFactor);
            dl->AddLine(ox, pyP, IM_COL32(60, 200, 60, 180), 1.8f);
            dl->AddLine(ox, pyN, IM_COL32(60, 200, 60, 60),  1.0f);
        }
    }

    // ===================== Model rendering =====================
    if (hasModel_ && !triangles_.empty()) {
        struct SortEntry { int idx; float depth; };
        std::vector<SortEntry> sortedTris(triangles_.size());

        for (int i = 0; i < (int)triangles_.size(); i++) {
            auto& tri = triangles_[i];
            float cx = (tri.v0[0] + tri.v1[0] + tri.v2[0]) / 3.0f;
            float cy = (tri.v0[1] + tri.v1[1] + tri.v2[1]) / 3.0f;
            float cz = (tri.v0[2] + tri.v1[2] + tri.v2[2]) / 3.0f;
            float mx = (cx + modelOffset_[0]) * modelScale_;
            float my = (cy + modelOffset_[1]) * modelScale_;
            float mz = (cz + modelOffset_[2]) * modelScale_;
            sortedTris[i] = { i, depthAfterRotation(mx, my, mz, yr, pr) };
        }

        std::sort(sortedTris.begin(), sortedTris.end(),
                  [](const SortEntry& a, const SortEntry& b) { return a.depth < b.depth; });

        float lightDir[3] = { 0.4f, 0.7f, 0.5f };
        normalize3(lightDir);
        float fillDir[3] = { -0.3f, 0.2f, -0.6f };
        normalize3(fillDir);
        float baseR = 0.50f, baseG = 0.65f, baseB = 0.78f;

        for (auto& entry : sortedTris) {
            auto& tri = triangles_[entry.idx];

            float sv0[3] = { (tri.v0[0]+modelOffset_[0])*modelScale_,
                             (tri.v0[1]+modelOffset_[1])*modelScale_,
                             (tri.v0[2]+modelOffset_[2])*modelScale_ };
            float sv1[3] = { (tri.v1[0]+modelOffset_[0])*modelScale_,
                             (tri.v1[1]+modelOffset_[1])*modelScale_,
                             (tri.v1[2]+modelOffset_[2])*modelScale_ };
            float sv2[3] = { (tri.v2[0]+modelOffset_[0])*modelScale_,
                             (tri.v2[1]+modelOffset_[1])*modelScale_,
                             (tri.v2[2]+modelOffset_[2])*modelScale_ };

            ImVec2 p0 = projectPerspective(sv0[0], sv0[1], sv0[2], center, baseScale, yr, pr, camDistance_, fovFactor);
            ImVec2 p1 = projectPerspective(sv1[0], sv1[1], sv1[2], center, baseScale, yr, pr, camDistance_, fovFactor);
            ImVec2 p2 = projectPerspective(sv2[0], sv2[1], sv2[2], center, baseScale, yr, pr, camDistance_, fovFactor);

            float* n = tri.normal;
            float ambient  = 0.15f;
            float diffuse  = 0.65f * ImClamp(dot3(n, lightDir), 0.0f, 1.0f);
            float fill     = 0.20f * ImClamp(dot3(n, fillDir),  0.0f, 1.0f);

            float viewDir[3] = { 0, 0, 1 };
            float halfVec[3] = { lightDir[0]+viewDir[0], lightDir[1]+viewDir[1], lightDir[2]+viewDir[2] };
            normalize3(halfVec);
            float spec = powf(ImClamp(dot3(n, halfVec), 0.0f, 1.0f), 32.0f) * 0.3f;

            float brightness = ImClamp(ambient + diffuse + fill + spec, 0.0f, 1.2f);
            int r = (int)ImClamp(baseR * brightness * 255.0f, 0.0f, 255.0f);
            int g = (int)ImClamp(baseG * brightness * 255.0f, 0.0f, 255.0f);
            int b = (int)ImClamp(baseB * brightness * 255.0f, 0.0f, 255.0f);

            if (!wireframe_) {
                ImVec2 triPts[3] = { p0, p1, p2 };
                dl->AddConvexPolyFilled(triPts, 3, IM_COL32(r, g, b, 230));
            }
            if (wireframe_) {
                ImU32 edgeCol = IM_COL32(60, 62, 68, 150);
                dl->AddLine(p0, p1, edgeCol, 1.0f);
                dl->AddLine(p1, p2, edgeCol, 1.0f);
                dl->AddLine(p2, p0, edgeCol, 1.0f);
            }
        }
    }

    // ===================== Axes gizmo (bottom-left corner) =====================
    {
        ImVec2 gizmoCenter = ImVec2(pos.x + 50, pos.y + size.y - 80);
        float  axLen = 36.0f;

        // Soft dark background disc
        dl->AddCircleFilled(gizmoCenter, 48, IM_COL32(35, 36, 40, 120), 32);
        dl->AddCircle(gizmoCenter, 48, IM_COL32(80, 82, 88, 50), 32, 1.2f);

        struct AxisInfo { float dx, dy, dz; ImU32 col; ImU32 colDim; const char* label; float depth; };
        AxisInfo axes[3] = {
            { 1, 0, 0, IM_COL32(230, 80, 80, 255),  IM_COL32(230, 80, 80, 100),  "X", 0 },
            { 0, 1, 0, IM_COL32(80, 210, 80, 255),   IM_COL32(80, 210, 80, 100),  "Y", 0 },
            { 0, 0, 1, IM_COL32(70, 130, 240, 255),  IM_COL32(70, 130, 240, 100), "Z", 0 },
        };

        for (auto& ax : axes)
            ax.depth = depthAfterRotation(ax.dx, ax.dy, ax.dz, yr, pr);
        std::sort(axes, axes + 3, [](const AxisInfo& a, const AxisInfo& b) { return a.depth < b.depth; });

        for (auto& ax : axes) {
            Vec3 tip3d = rotateYX(ax.dx * axLen, ax.dy * axLen, ax.dz * axLen, yr, pr);
            ImVec2 tip = ImVec2(gizmoCenter.x + tip3d.x, gizmoCenter.y - tip3d.y);
            // Dim back-facing axes
            bool frontFacing = ax.depth > 0.0f;
            ImU32 lineCol = frontFacing ? ax.col : ax.colDim;
            float lineW   = frontFacing ? 2.8f : 1.5f;
            dl->AddLine(gizmoCenter, tip, lineCol, lineW);
            // Arrow head: filled triangle instead of circle
            ImVec2 dir = ImVec2(tip.x - gizmoCenter.x, tip.y - gizmoCenter.y);
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (len > 0.01f) {
                dir.x /= len; dir.y /= len;
                ImVec2 perp = ImVec2(-dir.y, dir.x);
                float arrowSize = frontFacing ? 6.0f : 4.0f;
                ImVec2 a1 = ImVec2(tip.x - dir.x * arrowSize * 2 + perp.x * arrowSize,
                                   tip.y - dir.y * arrowSize * 2 + perp.y * arrowSize);
                ImVec2 a2 = ImVec2(tip.x - dir.x * arrowSize * 2 - perp.x * arrowSize,
                                   tip.y - dir.y * arrowSize * 2 - perp.y * arrowSize);
                dl->AddTriangleFilled(tip, a1, a2, lineCol);
            }
            // Label offset away from center
            float labelOff = frontFacing ? 10.0f : 8.0f;
            ImVec2 labelPos = ImVec2(tip.x + dir.x * labelOff - 4, tip.y + dir.y * labelOff - 7);
            dl->AddText(labelPos, lineCol, ax.label);
        }
    }

    // ===================== Bottom info bar =====================
    {
        float barH = 24.0f;
        float barY = pos.y + size.y - barH;
        dl->AddRectFilled(ImVec2(pos.x, barY),
                          ImVec2(pos.x + size.x, pos.y + size.y),
                          IM_COL32(40, 42, 46, 160));

        ImU32 infoCol = IM_COL32(200, 202, 208, 220);
        char buf[512];
        snprintf(buf, sizeof(buf),
                 " Eye: (%.2f, %.2f, %.2f) | Target: (%.2f, %.2f, %.2f) | %.1f FPS",
                 eyeX, eyeY, eyeZ,
                 camCenter_[0], camCenter_[1], camCenter_[2],
                 fps_);
        dl->AddText(ImVec2(pos.x + 6, barY + 5), infoCol, buf);

        if (hasModel_) {
            char mbuf[128];
            snprintf(mbuf, sizeof(mbuf), "Tris: %d  Verts: %d", triangleCount_, vertexCount_);
            ImVec2 ts = ImGui::CalcTextSize(mbuf);
            dl->AddText(ImVec2(pos.x + size.x - ts.x - 10, barY + 5), infoCol, mbuf);
        }
    }

    // ===================== Empty state: hints =====================
    if (!hasModel_) {
        const char* hint1 = "Drag & Drop STL / OBJ";
        ImVec2 ts1 = ImGui::CalcTextSize(hint1);
        float cx = pos.x + size.x * 0.5f;
        float cy = pos.y + size.y * 0.55f;
        dl->AddText(ImVec2(cx - ts1.x * 0.5f, cy), IM_COL32(55, 58, 62, 100), hint1);

        const char* hint2 = "LMB/RMB: Orbit  |  MMB: Pan  |  Scroll: Zoom";
        ImVec2 ts2 = ImGui::CalcTextSize(hint2);
        dl->AddText(ImVec2(cx - ts2.x * 0.5f, cy + 22), IM_COL32(65, 68, 72, 70), hint2);
    }

    // Pop clip rect BEFORE toolbar so ImGui buttons are not clipped
    dl->PopClipRect();

    // ===================== Toolbar overlay (top-right) =====================
    {
        ImVec2 framePad(8, 4);
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        const char* labels[] = { "Reset", "Top", "Front", "Right", "Persp", " W " };
        int nLabels = 6;
        float totalW = 0;
        for (int i = 0; i < nLabels; i++) {
            totalW += ImGui::CalcTextSize(labels[i]).x + framePad.x * 2;
            if (i < nLabels - 1) totalW += spacing;
        }

        // Use window bounds (not content cursor) for accurate positioning
        ImVec2 winPos  = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        float btnY = winPos.y + 6;
        float btnX = winPos.x + winSize.x - totalW - 10;
        if (btnX < winPos.x + 10) btnX = winPos.x + 10;
        ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.23f, 0.25f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.36f, 0.40f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.46f, 0.50f, 1.00f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePad);

        if (ImGui::Button("Reset")) resetCamera();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Camera (Home)");
        ImGui::SameLine();
        if (ImGui::Button("Top"))   setViewMode(1);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Top View");
        ImGui::SameLine();
        if (ImGui::Button("Front")) setViewMode(2);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Front View");
        ImGui::SameLine();
        if (ImGui::Button("Right")) setViewMode(3);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right View");
        ImGui::SameLine();
        if (ImGui::Button("Persp")) setViewMode(0);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Perspective View");
        ImGui::SameLine();
        if (ImGui::Button(wireframe_ ? "[W]" : " W ")) {
            wireframe_ = !wireframe_;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Wireframe");

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }

    ImGui::EndChild();
}

// ================================================================
//  Camera state
// ================================================================

void View3DPanel::setCameraState(float dist, float yaw, float pitch, float cx, float cy, float cz) {
    camDistance_ = dist;
    camYaw_ = yaw;
    camPitch_ = pitch;
    camCenter_[0] = cx;
    camCenter_[1] = cy;
    camCenter_[2] = cz;
}

void View3DPanel::getCameraState(float& dist, float& yaw, float& pitch, float& cx, float& cy, float& cz) const {
    dist = camDistance_;
    yaw = camYaw_;
    pitch = camPitch_;
    cx = camCenter_[0];
    cy = camCenter_[1];
    cz = camCenter_[2];
}

void View3DPanel::resize(int width, int height) {
    viewportWidth_ = width;
    viewportHeight_ = height;
}

void View3DPanel::resetCamera() {
    camDistance_ = 5.0f;
    camYaw_ = 45.0f;
    camPitch_ = -35.26f;
    camCenter_[0] = 0.0f;
    camCenter_[1] = 0.0f;
    camCenter_[2] = 0.0f;
}

void View3DPanel::setViewMode(int mode) {
    switch (mode) {
        case 0: camYaw_ =  45.0f; camPitch_ = -35.26f; break;
        case 1: camYaw_ =   0.0f; camPitch_ = 89.0f; break;
        case 2: camYaw_ =   0.0f; camPitch_ =  0.0f; break;
        case 3: camYaw_ =  90.0f; camPitch_ =  0.0f; break;
    }
}

// ================================================================
//  Binary STL loader
// ================================================================
bool View3DPanel::loadSTL(const char* filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        Logger::instance().error(std::string("Failed to open STL: ") + filepath);
        return false;
    }

    // Read header (80 bytes)
    char header[80];
    file.read(header, 80);

    // Read triangle count
    uint32_t numTriangles = 0;
    file.read(reinterpret_cast<char*>(&numTriangles), 4);

    if (numTriangles == 0 || numTriangles > 5000000) {
        Logger::instance().error("Invalid STL triangle count: " + std::to_string(numTriangles));
        return false;
    }

    triangles_.resize(numTriangles);

    for (uint32_t i = 0; i < numTriangles; i++) {
        Triangle3D& tri = triangles_[i];
        // Normal (3 floats)
        file.read(reinterpret_cast<char*>(tri.normal), 12);
        // Vertex 0
        file.read(reinterpret_cast<char*>(tri.v0), 12);
        // Vertex 1
        file.read(reinterpret_cast<char*>(tri.v1), 12);
        // Vertex 2
        file.read(reinterpret_cast<char*>(tri.v2), 12);
        // Attribute byte count (skip)
        uint16_t attr;
        file.read(reinterpret_cast<char*>(&attr), 2);

        // Recompute normal if zero
        float len = sqrtf(tri.normal[0]*tri.normal[0] + tri.normal[1]*tri.normal[1] + tri.normal[2]*tri.normal[2]);
        if (len < 1e-6f) {
            float e1[3] = { tri.v1[0]-tri.v0[0], tri.v1[1]-tri.v0[1], tri.v1[2]-tri.v0[2] };
            float e2[3] = { tri.v2[0]-tri.v0[0], tri.v2[1]-tri.v0[1], tri.v2[2]-tri.v0[2] };
            cross3(e1, e2, tri.normal);
            normalize3(tri.normal);
        } else {
            normalize3(tri.normal);
        }
    }

    triangleCount_ = (int)numTriangles;
    vertexCount_   = triangleCount_ * 3;
    hasModel_ = true;

    computeBounds();
    centerAndScale();

    Logger::instance().info("Loaded STL: " + std::string(filepath)
        + " (" + std::to_string(triangleCount_) + " triangles)");

    if (triangleCount_ > 20000) {
        Logger::instance().warn("Large mesh (" + std::to_string(triangleCount_)
            + " triangles) may affect rendering performance");
    }

    return true;
}

bool View3DPanel::loadOBJ(const char* filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::instance().error(std::string("Failed to open OBJ: ") + filepath);
        return false;
    }

    std::vector<float> positions; // x,y,z triplets
    std::vector<int> faceIndices; // vertex index triplets (triangulated)

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            if (sscanf(line.c_str() + 2, "%f %f %f", &x, &y, &z) == 3) {
                positions.push_back(x);
                positions.push_back(y);
                positions.push_back(z);
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            // Parse face - support "f v", "f v/vt", "f v/vt/vn", "f v//vn"
            std::vector<int> indices;
            const char* p = line.c_str() + 2;
            while (*p) {
                while (*p == ' ') p++;
                if (!*p) break;
                int vi = 0;
                if (sscanf(p, "%d", &vi) == 1) {
                    indices.push_back(vi > 0 ? vi - 1 : (int)(positions.size()/3) + vi);
                }
                // Skip past this vertex spec
                while (*p && *p != ' ') p++;
            }
            // Triangulate (fan)
            for (int i = 2; i < (int)indices.size(); i++) {
                faceIndices.push_back(indices[0]);
                faceIndices.push_back(indices[i-1]);
                faceIndices.push_back(indices[i]);
            }
        }
    }

    int numVerts = (int)(positions.size() / 3);
    int numTris = (int)(faceIndices.size() / 3);
    if (numTris == 0) {
        Logger::instance().error("OBJ file has no faces");
        return false;
    }

    triangles_.resize(numTris);
    for (int i = 0; i < numTris; i++) {
        int i0 = faceIndices[i*3+0];
        int i1 = faceIndices[i*3+1];
        int i2 = faceIndices[i*3+2];

        auto getVert = [&](int idx, float* out) {
            if (idx >= 0 && idx < numVerts) {
                out[0] = positions[idx*3+0];
                out[1] = positions[idx*3+1];
                out[2] = positions[idx*3+2];
            }
        };

        getVert(i0, triangles_[i].v0);
        getVert(i1, triangles_[i].v1);
        getVert(i2, triangles_[i].v2);

        // Compute normal
        float e1[3] = { triangles_[i].v1[0]-triangles_[i].v0[0],
                         triangles_[i].v1[1]-triangles_[i].v0[1],
                         triangles_[i].v1[2]-triangles_[i].v0[2] };
        float e2[3] = { triangles_[i].v2[0]-triangles_[i].v0[0],
                         triangles_[i].v2[1]-triangles_[i].v0[1],
                         triangles_[i].v2[2]-triangles_[i].v0[2] };
        cross3(e1, e2, triangles_[i].normal);
        normalize3(triangles_[i].normal);
    }

    triangleCount_ = numTris;
    vertexCount_   = numVerts;
    hasModel_ = true;

    computeBounds();
    centerAndScale();

    Logger::instance().info("Loaded OBJ: " + std::string(filepath)
        + " (" + std::to_string(triangleCount_) + " triangles, "
        + std::to_string(vertexCount_) + " vertices)");

    return true;
}

bool View3DPanel::loadVDB(const char* filepath) {
    Logger::instance().warn("VDB loading not yet implemented");
    return false;
}

void View3DPanel::clearModel() {
    triangles_.clear();
    hasModel_ = false;
    vertexCount_ = 0;
    triangleCount_ = 0;
    modelScale_ = 1.0f;
    modelOffset_[0] = modelOffset_[1] = modelOffset_[2] = 0.0f;
}

void View3DPanel::setTriangles(const std::vector<Triangle3D>& tris) {
    triangles_ = tris;
    triangleCount_ = (int)triangles_.size();
    vertexCount_   = triangleCount_ * 3;
    hasModel_ = !triangles_.empty();

    if (hasModel_) {
        computeBounds();
        centerAndScale();
    }
}

// ================================================================
//  Bounding box & auto-center/scale
// ================================================================

void View3DPanel::computeBounds() {
    if (triangles_.empty()) return;

    bboxMin_[0] = bboxMin_[1] = bboxMin_[2] =  1e30f;
    bboxMax_[0] = bboxMax_[1] = bboxMax_[2] = -1e30f;

    for (auto& tri : triangles_) {
        const float* vv[3] = { tri.v0, tri.v1, tri.v2 };
        for (int v = 0; v < 3; v++) {
            for (int a = 0; a < 3; a++) {
                bboxMin_[a] = std::min(bboxMin_[a], vv[v][a]);
                bboxMax_[a] = std::max(bboxMax_[a], vv[v][a]);
            }
        }
    }
}

void View3DPanel::centerAndScale() {
    float cx = (bboxMin_[0] + bboxMax_[0]) * 0.5f;
    float cy = (bboxMin_[1] + bboxMax_[1]) * 0.5f;
    float cz = (bboxMin_[2] + bboxMax_[2]) * 0.5f;

    float dx = bboxMax_[0] - bboxMin_[0];
    float dy = bboxMax_[1] - bboxMin_[1];
    float dz = bboxMax_[2] - bboxMin_[2];
    float maxDim = std::max({dx, dy, dz, 0.001f});

    // Scale to fit in ~5 units
    modelScale_ = 5.0f / maxDim;
    modelOffset_[0] = -cx;
    modelOffset_[1] = -cy;
    modelOffset_[2] = -cz;

    // Reset camera to see the model
    camDistance_ = 5.0f;
    camYaw_ = 45.0f;
    camPitch_ = -35.26f;
    camCenter_[0] = 0.0f;
    camCenter_[1] = 0.0f;
    camCenter_[2] = 0.0f;
}

// Stub implementations
void View3DPanel::drawGrid()   {}
void View3DPanel::drawAxes()   {}
void View3DPanel::drawModel()  {}
void View3DPanel::handleCameraControls() {}

} // namespace TopOpt
