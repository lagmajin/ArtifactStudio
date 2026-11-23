#include "BoneVisualizerWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QStatusBar>

BoneVisualizerWindow::BoneVisualizerWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Bone Visualizer Lab - FBX & DCC Depth Control Prototype");
    resize(1280, 800);
    setupUI();
    loadDefaultHumanoid();
}

void BoneVisualizerWindow::setupUI() {
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 1. 3D Viewport on the left
    viewport_ = new BoneViewportWidget(this);
    mainLayout->addWidget(viewport_, 1);

    // 2. Control Panel on the right
    QWidget* sidePanel = new QWidget(this);
    sidePanel->setFixedWidth(300);
    QVBoxLayout* sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(12);

    // Group A: FBX Load & Model
    QGroupBox* grpModel = new QGroupBox("Model & Skeleton", sidePanel);
    QVBoxLayout* vModel = new QVBoxLayout(grpModel);

    QPushButton* btnOpenFbx = new QPushButton("Open FBX File...", grpModel);
    connect(btnOpenFbx, &QPushButton::clicked, this, &BoneVisualizerWindow::openFbxFile);
    vModel->addWidget(btnOpenFbx);

    QPushButton* btnHumanoid = new QPushButton("Load Biped Humanoid Rig", grpModel);
    connect(btnHumanoid, &QPushButton::clicked, this, &BoneVisualizerWindow::loadDefaultHumanoid);
    vModel->addWidget(btnHumanoid);

    QPushButton* btnResetCam = new QPushButton("Reset Camera", grpModel);
    connect(btnResetCam, &QPushButton::clicked, viewport_, &BoneViewportWidget::resetCamera);
    vModel->addWidget(btnResetCam);

    checkMockMesh_ = new QCheckBox("Show Character Proxy Mesh", grpModel);
    checkMockMesh_->setChecked(true);
    connect(checkMockMesh_, &QCheckBox::toggled, this, &BoneVisualizerWindow::onMockMeshToggled);
    vModel->addWidget(checkMockMesh_);

    sideLayout->addWidget(grpModel);

    // Group B: Depth Control (DCC features)
    QGroupBox* grpDepth = new QGroupBox("Depth Control (DCC Mode)", sidePanel);
    QVBoxLayout* vDepth = new QVBoxLayout(grpDepth);

    vDepth->addWidget(new QLabel("Depth Display Mode:"));
    depthModeCombo_ = new QComboBox(grpDepth);
    depthModeCombo_->addItem("Normal (Occluded by Mesh)");
    depthModeCombo_->addItem("X-Ray (Always on Top)");
    depthModeCombo_->addItem("DCC Ghosting (Dual-Pass Ghosted)");
    depthModeCombo_->setCurrentIndex(2); // Default to DCC Ghosting
    connect(depthModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BoneVisualizerWindow::onDepthModeChanged);
    vDepth->addWidget(depthModeCombo_);

    vDepth->addWidget(new QLabel("Ghost Opacity (Occluded bones):"));
    sliderGhostAlpha_ = new QSlider(Qt::Horizontal, grpDepth);
    sliderGhostAlpha_->setRange(5, 100);
    sliderGhostAlpha_->setValue(35);
    connect(sliderGhostAlpha_, &QSlider::valueChanged, this, &BoneVisualizerWindow::onGhostAlphaChanged);
    vDepth->addWidget(sliderGhostAlpha_);

    sideLayout->addWidget(grpDepth);

    // Group C: Bone Style & Gizmos
    QGroupBox* grpStyle = new QGroupBox("Bone Style & Gizmos", sidePanel);
    QVBoxLayout* vStyle = new QVBoxLayout(grpStyle);

    vStyle->addWidget(new QLabel("Bone Geometry:"));
    boneStyleCombo_ = new QComboBox(grpStyle);
    boneStyleCombo_->addItem("Octahedral Solid");
    boneStyleCombo_->addItem("Wireframe Only");
    boneStyleCombo_->addItem("Solid + Wire Edges");
    boneStyleCombo_->addItem("Stick (Line)");
    boneStyleCombo_->setCurrentIndex(2); // Default to Solid + Wire
    connect(boneStyleCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BoneVisualizerWindow::onBoneStyleChanged);
    vStyle->addWidget(boneStyleCombo_);

    vStyle->addWidget(new QLabel("Bone Thickness:"));
    sliderThickness_ = new QSlider(Qt::Horizontal, grpStyle);
    sliderThickness_->setRange(10, 150);
    sliderThickness_->setValue(40);
    connect(sliderThickness_, &QSlider::valueChanged, this, &BoneVisualizerWindow::onThicknessChanged);
    vStyle->addWidget(sliderThickness_);

    checkJoints_ = new QCheckBox("Show Joint Spheres", grpStyle);
    checkJoints_->setChecked(true);
    connect(checkJoints_, &QCheckBox::toggled, this, &BoneVisualizerWindow::onJointsToggled);
    vStyle->addWidget(checkJoints_);

    checkAxes_ = new QCheckBox("Show Local Joint Axes (XYZ)", grpStyle);
    checkAxes_->setChecked(true);
    connect(checkAxes_, &QCheckBox::toggled, this, &BoneVisualizerWindow::onAxesToggled);
    vStyle->addWidget(checkAxes_);

    sideLayout->addWidget(grpStyle);

    // Status label
    statusLabel_ = new QLabel("Ready.", sidePanel);
    statusLabel_->setWordWrap(true);
    sideLayout->addWidget(statusLabel_);

    sideLayout->addStretch(1);
    mainLayout->addWidget(sidePanel, 0);

    statusBar()->showMessage("Left click: Orbit | Right/Middle click: Pan | Wheel: Zoom");
}

void BoneVisualizerWindow::openFbxFile() {
    QString path = QFileDialog::getOpenFileName(
        this,
        "Select FBX File",
        QString(),
        "FBX Files (*.fbx);;All Files (*.*)"
    );
    if (path.isEmpty()) return;

    SkeletonData skel = FbxSkeletonLoader::loadFbx(path);
    if (!skel.valid) {
        statusLabel_->setText(QString("Failed to load skeleton from: %1").arg(path));
        return;
    }

    viewport_->setSkeleton(skel);
    statusLabel_->setText(QString("Loaded: %1\nBones: %2")
        .arg(QFileInfo(path).fileName())
        .arg(skel.bones.size()));
}

void BoneVisualizerWindow::loadDefaultHumanoid() {
    SkeletonData skel = FbxSkeletonLoader::createTestHumanoidSkeleton();
    viewport_->setSkeleton(skel);
    statusLabel_->setText(QString("Loaded standard humanoid biped rig (%1 bones).").arg(skel.bones.size()));
}

void BoneVisualizerWindow::onDepthModeChanged(int index) {
    viewport_->setDepthMode(static_cast<DepthMode>(index));
}

void BoneVisualizerWindow::onBoneStyleChanged(int index) {
    viewport_->setBoneStyle(static_cast<BoneStyle>(index));
}

void BoneVisualizerWindow::onJointsToggled(bool checked) {
    viewport_->setShowJoints(checked);
}

void BoneVisualizerWindow::onAxesToggled(bool checked) {
    viewport_->setShowAxes(checked);
}

void BoneVisualizerWindow::onMockMeshToggled(bool checked) {
    viewport_->setShowMockMesh(checked);
}

void BoneVisualizerWindow::onThicknessChanged(int value) {
    viewport_->setBoneThickness(float(value) * 0.001f);
}

void BoneVisualizerWindow::onGhostAlphaChanged(int value) {
    viewport_->setGhostAlpha(float(value) / 100.0f);
}
