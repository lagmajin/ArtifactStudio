#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include "BoneGeometryBuilder.h"

enum class DepthMode {
    Normal = 0,    // Standard depth test (occluded by 3D model)
    XRay,          // Always drawn on top (no depth test)
    GhostDCC       // Dual-pass DCC style: hidden parts ghosted/translucent, visible parts solid
};

enum class BoneStyle {
    OctahedralSolid = 0,
    WireframeOnly,
    SolidWithWire,
    StickOnly
};

class BoneViewportWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit BoneViewportWidget(QWidget* parent = nullptr);
    ~BoneViewportWidget() override;

    void setSkeleton(const SkeletonData& skeleton);
    void setDepthMode(DepthMode mode);
    void setBoneStyle(BoneStyle style);
    void setShowJoints(bool show);
    void setShowAxes(bool show);
    void setShowMockMesh(bool show);
    void setBoneThickness(float t);
    void setGhostAlpha(float alpha);

    void resetCamera();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void rebuildGeometry();
    void renderMockMesh(const QMatrix4x4& mvp);
    void renderBones(const QMatrix4x4& mvp, bool isGhostPass);

    SkeletonData skeleton_;
    BoneMeshBuffer boneBuffer_;

    DepthMode depthMode_ = DepthMode::GhostDCC;
    BoneStyle boneStyle_ = BoneStyle::SolidWithWire;
    bool showJoints_ = true;
    bool showAxes_ = true;
    bool showMockMesh_ = true;
    float boneThickness_ = 0.04f;
    float ghostAlpha_ = 0.35f;

    // Camera parameters
    QVector3D cameraTarget_{0.0f, 1.0f, 0.0f};
    float cameraDistance_ = 2.8f;
    float cameraYaw_ = 25.0f;
    float cameraPitch_ = 15.0f;
    QPoint lastMousePos_;

    // OpenGL Resources
    QOpenGLShaderProgram* shader_ = nullptr;
    QOpenGLBuffer vboSolid_{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer iboSolid_{QOpenGLBuffer::IndexBuffer};
    QOpenGLBuffer vboWire_{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer iboWire_{QOpenGLBuffer::IndexBuffer};
    QOpenGLBuffer vboJoint_{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer iboJoint_{QOpenGLBuffer::IndexBuffer};
    QOpenGLBuffer vboAxis_{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer iboAxis_{QOpenGLBuffer::IndexBuffer};

    // Mock Body Mesh (Dummy skin cylinder/capsule for depth testing)
    std::vector<VertexPN> mockMeshVerts_;
    std::vector<unsigned int> mockMeshIndices_;
    QOpenGLBuffer vboMockMesh_{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer iboMockMesh_{QOpenGLBuffer::IndexBuffer};

    void setupMockMesh();
};
