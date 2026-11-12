#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include "BoneViewportWidget.h"

class BoneVisualizerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit BoneVisualizerWindow(QWidget* parent = nullptr);
    ~BoneVisualizerWindow() override = default;

private slots:
    void openFbxFile();
    void loadDefaultHumanoid();
    void onDepthModeChanged(int index);
    void onBoneStyleChanged(int index);
    void onJointsToggled(bool checked);
    void onAxesToggled(bool checked);
    void onMockMeshToggled(bool checked);
    void onThicknessChanged(int value);
    void onGhostAlphaChanged(int value);

private:
    void setupUI();

    BoneViewportWidget* viewport_ = nullptr;
    QComboBox* depthModeCombo_ = nullptr;
    QComboBox* boneStyleCombo_ = nullptr;
    QCheckBox* checkJoints_ = nullptr;
    QCheckBox* checkAxes_ = nullptr;
    QCheckBox* checkMockMesh_ = nullptr;
    QSlider* sliderThickness_ = nullptr;
    QSlider* sliderGhostAlpha_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};
