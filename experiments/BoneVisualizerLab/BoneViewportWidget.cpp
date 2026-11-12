#include "BoneViewportWidget.h"
#include <cmath>

static const char* vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec3 aNormal;
    layout(location = 2) in vec4 aColor;

    uniform mat4 uMVP;
    uniform mat4 uModel;

    out vec3 vNormal;
    out vec4 vColor;
    out vec3 vWorldPos;

    void main() {
        gl_Position = uMVP * vec4(aPos, 1.0);
        vNormal = mat3(uModel) * aNormal;
        vColor = aColor;
        vWorldPos = vec3(uModel * vec4(aPos, 1.0));
    }
)";

static const char* fragmentShaderSource = R"(
    #version 330 core
    in vec3 vNormal;
    in vec4 vColor;
    in vec3 vWorldPos;

    uniform vec3 uLightDir;
    uniform float uAlphaMultiplier;
    uniform int uUnlit;

    out vec4 FragColor;

    void main() {
        if (uUnlit == 1) {
            FragColor = vec4(vColor.rgb, vColor.a * uAlphaMultiplier);
            return;
        }

        vec3 N = normalize(vNormal);
        vec3 L = normalize(uLightDir);
        float diff = max(dot(N, L), 0.0) * 0.7 + 0.3; // ambient 0.3

        vec3 base = vColor.rgb * diff;
        FragColor = vec4(base, vColor.a * uAlphaMultiplier);
    }
)";

BoneViewportWidget::BoneViewportWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
}

BoneViewportWidget::~BoneViewportWidget() {
    makeCurrent();
    delete shader_;
    vboSolid_.destroy();
    iboSolid_.destroy();
    vboWire_.destroy();
    iboWire_.destroy();
    vboJoint_.destroy();
    iboJoint_.destroy();
    vboAxis_.destroy();
    iboAxis_.destroy();
    vboMockMesh_.destroy();
    iboMockMesh_.destroy();
    doneCurrent();
}

void BoneViewportWidget::setSkeleton(const SkeletonData& skeleton) {
    skeleton_ = skeleton;
    if (skeleton.valid) {
        cameraTarget_ = (skeleton.boundsMin + skeleton.boundsMax) * 0.5f;
        float diag = (skeleton.boundsMax - skeleton.boundsMin).length();
        cameraDistance_ = std::max(1.5f, diag * 1.5f);
    }
    rebuildGeometry();
    update();
}

void BoneViewportWidget::setDepthMode(DepthMode mode) {
    depthMode_ = mode;
    update();
}

void BoneViewportWidget::setBoneStyle(BoneStyle style) {
    boneStyle_ = style;
    update();
}

void BoneViewportWidget::setShowJoints(bool show) {
    showJoints_ = show;
    update();
}

void BoneViewportWidget::setShowAxes(bool show) {
    showAxes_ = show;
    update();
}

void BoneViewportWidget::setShowMockMesh(bool show) {
    showMockMesh_ = show;
    update();
}

void BoneViewportWidget::setBoneThickness(float t) {
    boneThickness_ = t;
    rebuildGeometry();
    update();
}

void BoneViewportWidget::setGhostAlpha(float alpha) {
    ghostAlpha_ = alpha;
    update();
}

void BoneViewportWidget::resetCamera() {
    if (skeleton_.valid) {
        cameraTarget_ = (skeleton_.boundsMin + skeleton_.boundsMax) * 0.5f;
        float diag = (skeleton_.boundsMax - skeleton_.boundsMin).length();
        cameraDistance_ = std::max(1.5f, diag * 1.5f);
    } else {
        cameraTarget_ = QVector3D(0.0f, 1.0f, 0.0f);
        cameraDistance_ = 2.8f;
    }
    cameraYaw_ = 25.0f;
    cameraPitch_ = 15.0f;
    update();
}

void BoneViewportWidget::initializeGL() {
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_ = new QOpenGLShaderProgram(this);
    shader_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    shader_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    shader_->link();

    vboSolid_.create();
    iboSolid_.create();
    vboWire_.create();
    iboWire_.create();
    vboJoint_.create();
    iboJoint_.create();
    vboAxis_.create();
    iboAxis_.create();
    vboMockMesh_.create();
    iboMockMesh_.create();

    setupMockMesh();
    rebuildGeometry();
}

void BoneViewportWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void BoneViewportWidget::setupMockMesh() {
    // Generate a simple translucent capsule/cylinder proxy representing a character mesh
    mockMeshVerts_.clear();
    mockMeshIndices_.clear();

    const int slices = 16;
    const int stacks = 16;
    float radius = 0.22f;
    float height = 1.6f;
    float yOffset = 0.1f;
    QVector4D skinColor(0.45f, 0.55f, 0.65f, 0.65f);

    for (int i = 0; i <= stacks; ++i) {
        float v = float(i) / float(stacks);
        float y = yOffset + v * height;
        // tapering around waist
        float r = radius * (1.0f - 0.25f * std::sin(v * 3.14159f));

        for (int j = 0; j <= slices; ++j) {
            float u = float(j) / float(slices);
            float angle = u * 2.0f * 3.14159265f;
            float x = std::cos(angle) * r;
            float z = std::sin(angle) * r;
            QVector3D p(x, y, z);
            QVector3D n(std::cos(angle), 0.0f, std::sin(angle));
            mockMeshVerts_.push_back({p, n, skinColor});
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            unsigned int p0 = i * (slices + 1) + j;
            unsigned int p1 = p0 + 1;
            unsigned int p2 = (i + 1) * (slices + 1) + j;
            unsigned int p3 = p2 + 1;

            mockMeshIndices_.push_back(p0);
            mockMeshIndices_.push_back(p2);
            mockMeshIndices_.push_back(p1);

            mockMeshIndices_.push_back(p1);
            mockMeshIndices_.push_back(p2);
            mockMeshIndices_.push_back(p3);
        }
    }

    if (vboMockMesh_.isCreated()) {
        vboMockMesh_.bind();
        vboMockMesh_.allocate(mockMeshVerts_.data(), static_cast<int>(mockMeshVerts_.size() * sizeof(VertexPN)));
        iboMockMesh_.bind();
        iboMockMesh_.allocate(mockMeshIndices_.data(), static_cast<int>(mockMeshIndices_.size() * sizeof(unsigned int)));
    }
}

void BoneViewportWidget::rebuildGeometry() {
    BoneGeometryBuilder::buildSkeletonGeometry(skeleton_, boneBuffer_, boneThickness_);

    if (!isValid()) return;
    makeCurrent();

    if (!boneBuffer_.solidVertices.empty()) {
        vboSolid_.bind();
        vboSolid_.allocate(boneBuffer_.solidVertices.data(), static_cast<int>(boneBuffer_.solidVertices.size() * sizeof(VertexPN)));
        iboSolid_.bind();
        iboSolid_.allocate(boneBuffer_.solidIndices.data(), static_cast<int>(boneBuffer_.solidIndices.size() * sizeof(unsigned int)));
    }
    if (!boneBuffer_.wireVertices.empty()) {
        vboWire_.bind();
        vboWire_.allocate(boneBuffer_.wireVertices.data(), static_cast<int>(boneBuffer_.wireVertices.size() * sizeof(VertexPN)));
        iboWire_.bind();
        iboWire_.allocate(boneBuffer_.wireIndices.data(), static_cast<int>(boneBuffer_.wireIndices.size() * sizeof(unsigned int)));
    }
    if (!boneBuffer_.jointVertices.empty()) {
        vboJoint_.bind();
        vboJoint_.allocate(boneBuffer_.jointVertices.data(), static_cast<int>(boneBuffer_.jointVertices.size() * sizeof(VertexPN)));
        iboJoint_.bind();
        iboJoint_.allocate(boneBuffer_.jointIndices.data(), static_cast<int>(boneBuffer_.jointIndices.size() * sizeof(unsigned int)));
    }
    if (!boneBuffer_.axisVertices.empty()) {
        vboAxis_.bind();
        vboAxis_.allocate(boneBuffer_.axisVertices.data(), static_cast<int>(boneBuffer_.axisVertices.size() * sizeof(VertexPN)));
        iboAxis_.bind();
        iboAxis_.allocate(boneBuffer_.axisIndices.data(), static_cast<int>(boneBuffer_.axisIndices.size() * sizeof(unsigned int)));
    }

    doneCurrent();
}

void BoneViewportWidget::paintGL() {
    // Dark DCC Viewport background
    glClearColor(0.13f, 0.14f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Compute View and Projection Matrices
    QMatrix4x4 proj;
    float aspect = float(width()) / float(height() > 0 ? height() : 1);
    proj.perspective(45.0f, aspect, 0.05f, 100.0f);

    QMatrix4x4 view;
    view.translate(0, 0, -cameraDistance_);
    view.rotate(cameraPitch_, 1, 0, 0);
    view.rotate(cameraYaw_, 0, 1, 0);
    view.translate(-cameraTarget_);

    QMatrix4x4 mvp = proj * view;

    shader_->bind();
    shader_->setUniformValue("uLightDir", QVector3D(0.4f, 0.8f, 0.5f).normalized());
    shader_->setUniformValue("uModel", QMatrix4x4());
    shader_->setUniformValue("uMVP", mvp);

    auto setupVertexAttribs = [&]() {
        shader_->enableAttributeArray(0);
        shader_->setAttributeBuffer(0, GL_FLOAT, offsetof(VertexPN, position), 3, sizeof(VertexPN));
        shader_->enableAttributeArray(1);
        shader_->setAttributeBuffer(1, GL_FLOAT, offsetof(VertexPN, normal), 3, sizeof(VertexPN));
        shader_->enableAttributeArray(2);
        shader_->setAttributeBuffer(2, GL_FLOAT, offsetof(VertexPN, color), 4, sizeof(VertexPN));
    };

    // 1. Render Mock Character Mesh first to fill the Depth Buffer
    if (showMockMesh_ && !mockMeshIndices_.empty()) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);

        vboMockMesh_.bind();
        iboMockMesh_.bind();
        setupVertexAttribs();
        shader_->setUniformValue("uUnlit", 0);
        shader_->setUniformValue("uAlphaMultiplier", 0.65f);

        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mockMeshIndices_.size()), GL_UNSIGNED_INT, nullptr);
    }

    // 2. Render Bones according to DepthMode
    if (depthMode_ == DepthMode::Normal) {
        // Normal depth test: occluded bones disappear behind mesh
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        renderBones(mvp, false);
    }
    else if (depthMode_ == DepthMode::XRay) {
        // X-Ray: Always on top
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        renderBones(mvp, false);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }
    else if (depthMode_ == DepthMode::GhostDCC) {
        // Dual-Pass DCC Ghosting:
        // Pass A: Occluded (Depth test: GL_GREATER), draw ghosted/dimmed
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_GREATER);
        glDepthMask(GL_FALSE); // Don't write to depth buffer
        renderBones(mvp, true);

        // Pass B: Visible (Depth test: GL_LEQUAL), draw full solid/highlight
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        renderBones(mvp, false);
    }

    shader_->release();
}

void BoneViewportWidget::renderBones(const QMatrix4x4& mvp, bool isGhostPass) {
    Q_UNUSED(mvp);
    auto setupVertexAttribs = [&]() {
        shader_->enableAttributeArray(0);
        shader_->setAttributeBuffer(0, GL_FLOAT, offsetof(VertexPN, position), 3, sizeof(VertexPN));
        shader_->enableAttributeArray(1);
        shader_->setAttributeBuffer(1, GL_FLOAT, offsetof(VertexPN, normal), 3, sizeof(VertexPN));
        shader_->enableAttributeArray(2);
        shader_->setAttributeBuffer(2, GL_FLOAT, offsetof(VertexPN, color), 4, sizeof(VertexPN));
    };

    float alpha = isGhostPass ? ghostAlpha_ : 1.0f;

    // A. Solid Octahedral bodies
    if ((boneStyle_ == BoneStyle::OctahedralSolid || boneStyle_ == BoneStyle::SolidWithWire)
        && !boneBuffer_.solidIndices.empty()) {
        vboSolid_.bind();
        iboSolid_.bind();
        setupVertexAttribs();
        shader_->setUniformValue("uUnlit", 0);
        shader_->setUniformValue("uAlphaMultiplier", isGhostPass ? alpha * 0.7f : 1.0f);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(boneBuffer_.solidIndices.size()), GL_UNSIGNED_INT, nullptr);
    }

    // B. Wireframe Edges
    if ((boneStyle_ == BoneStyle::WireframeOnly || boneStyle_ == BoneStyle::SolidWithWire)
        && !boneBuffer_.wireIndices.empty()) {
        vboWire_.bind();
        iboWire_.bind();
        setupVertexAttribs();
        shader_->setUniformValue("uUnlit", 1);
        shader_->setUniformValue("uAlphaMultiplier", alpha);
        glLineWidth(isGhostPass ? 1.0f : 1.5f);
        glDrawElements(GL_LINES, static_cast<GLsizei>(boneBuffer_.wireIndices.size()), GL_UNSIGNED_INT, nullptr);
    }

    // C. Joint Spheres
    if (showJoints_ && !boneBuffer_.jointIndices.empty()) {
        vboJoint_.bind();
        iboJoint_.bind();
        setupVertexAttribs();
        shader_->setUniformValue("uUnlit", 0);
        shader_->setUniformValue("uAlphaMultiplier", isGhostPass ? alpha * 0.8f : 1.0f);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(boneBuffer_.jointIndices.size()), GL_UNSIGNED_INT, nullptr);
    }

    // D. Local Coordinate Axes
    if (showAxes_ && !boneBuffer_.axisIndices.empty()) {
        vboAxis_.bind();
        iboAxis_.bind();
        setupVertexAttribs();
        shader_->setUniformValue("uUnlit", 1);
        shader_->setUniformValue("uAlphaMultiplier", alpha);
        glLineWidth(2.0f);
        glDrawElements(GL_LINES, static_cast<GLsizei>(boneBuffer_.axisIndices.size()), GL_UNSIGNED_INT, nullptr);
    }
}

void BoneViewportWidget::mousePressEvent(QMouseEvent* event) {
    lastMousePos_ = event->pos();
}

void BoneViewportWidget::mouseMoveEvent(QMouseEvent* event) {
    QPoint delta = event->pos() - lastMousePos_;
    lastMousePos_ = event->pos();

    if (event->buttons() & Qt::LeftButton) {
        // Orbit
        cameraYaw_ += delta.x() * 0.5f;
        cameraPitch_ += delta.y() * 0.5f;
        cameraPitch_ = std::clamp(cameraPitch_, -89.0f, 89.0f);
        update();
    } else if (event->buttons() & Qt::MiddleButton || event->buttons() & Qt::RightButton) {
        // Pan
        float factor = cameraDistance_ * 0.002f;
        QMatrix4x4 rot;
        rot.rotate(cameraPitch_, 1, 0, 0);
        rot.rotate(cameraYaw_, 0, 1, 0);
        QVector3D right = rot.column(0).toVector3D();
        QVector3D up = rot.column(1).toVector3D();

        cameraTarget_ -= (right * (delta.x() * factor) - up * (delta.y() * factor));
        update();
    }
}

void BoneViewportWidget::wheelEvent(QWheelEvent* event) {
    float numDegrees = event->angleDelta().y() / 8.0f;
    float numSteps = numDegrees / 15.0f;
    cameraDistance_ *= std::pow(0.9f, numSteps);
    cameraDistance_ = std::clamp(cameraDistance_, 0.1f, 50.0f);
    update();
}
