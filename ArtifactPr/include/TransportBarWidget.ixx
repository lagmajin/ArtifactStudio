module;
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <wobjectdefs.h>

export module ArtifactPr.TransportBarWidget;

import ArtifactPr.EditorEngine;

export class TransportBarWidget : public QWidget
{
    W_OBJECT(TransportBarWidget)
public:
    explicit TransportBarWidget(QWidget* parent = nullptr);

Q_SIGNALS:
    void requestExport() W_SIGNAL(requestExport);

private Q_SLOTS:
    void onStopClicked();
    void onStepBackClicked();
    void onPlayClicked();
    void onStepFwdClicked();
    void onPlaybackTick();
    void onExportClicked();
    void updateTimecode(ArtifactPr::FramePosition frame);
    void updatePlayState(bool isPlaying);
    void updateSpeedDisplay(ArtifactPr::PlaybackSpeed speed);

private:
    QPushButton *stopBtn_ = nullptr;
    QPushButton *stepBackBtn_ = nullptr;
    QPushButton *playBtn_ = nullptr;
    QPushButton *stepFwdBtn_ = nullptr;
    QLabel* timecode_ = nullptr;
    QLabel* speedLabel_ = nullptr;
    QTimer* playbackTimer_ = nullptr;
};
