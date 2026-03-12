#pragma once
#include <vector>

namespace TopOpt {

struct Triangle3D {
    float v0[3], v1[3], v2[3];
    float normal[3];
};

class View3DPanel {
public:
    View3DPanel();
    ~View3DPanel();

    void draw();
    void resize(int width, int height);

    // Camera control
    void resetCamera();
    void setViewMode(int mode); // 0=Perspective, 1=Top, 2=Front, 3=Right

    // Camera state for serialization
    void setCameraState(float dist, float yaw, float pitch, float cx, float cy, float cz);
    void getCameraState(float& dist, float& yaw, float& pitch, float& cx, float& cy, float& cz) const;

    // Model loading
    bool loadSTL(const char* filepath);
    bool loadOBJ(const char* filepath);
    bool loadVDB(const char* filepath);
    void clearModel();

    // Set triangles directly (used by execution engine)
    void setTriangles(const std::vector<Triangle3D>& tris);

    // View options
    void setShowGrid(bool show) { showGrid_ = show; }
    void setShowAxes(bool show) { showAxes_ = show; }
    void setWireframe(bool wireframe) { wireframe_ = wireframe; }

private:
    void drawGrid();
    void drawAxes();
    void drawModel();
    void handleCameraControls();

    // Mesh utilities
    void computeBounds();
    void centerAndScale();

    // Camera parameters
    float camDistance_    = 5.0f;
    float camYaw_        = 45.0f;
    float camPitch_      = -35.26f;
    float camCenter_[3]  = {0.0f, 0.0f, 0.0f};

    // View options
    bool showGrid_    = true;
    bool showAxes_    = true;
    bool wireframe_   = false;

    // Rendering state
    int viewportWidth_  = 0;
    int viewportHeight_ = 0;
    float fps_          = 0.0f;

    // Model data
    bool hasModel_      = false;
    int vertexCount_    = 0;
    int triangleCount_  = 0;
    std::vector<Triangle3D> triangles_;

    // Bounding box
    float bboxMin_[3] = {0, 0, 0};
    float bboxMax_[3] = {0, 0, 0};
    float modelScale_ = 1.0f;
    float modelOffset_[3] = {0, 0, 0};
};

} // namespace TopOpt
