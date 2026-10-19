module;
#include <QEvent>
#include <QPoint>
#include <QString>
#include <QVector>
#include <QWidget>
#include <functional>
#include <wobjectdefs.h>

export module ArtifactPr.TimelinePanel;

import ArtifactPr.EditorEngine;

export class ArtifactPrTimelinePanel : public QWidget
{
    W_OBJECT(ArtifactPrTimelinePanel)
public:
    explicit ArtifactPrTimelinePanel(QWidget* parent = nullptr);
    ~ArtifactPrTimelinePanel();
    void setWaveformProvider(std::function<const QVector<float>*(const QString&)> provider);
    bool eventFilter(QObject* obj, QEvent* event) override;

private Q_SLOTS:
    void refreshTimeline(const ArtifactPr::DemoSequence& seq);
    void onFrameChanged(ArtifactPr::FramePosition frame);
    void onClipSelectionChanged(const QString& clipId);
    void onMarkerChanged();
    void onTransitionChanged();
    void onMarkerClicked(const QString& markerId);
    void onMarkerRightClicked(const QString& markerId, const QPoint& pos);
    void onMarkerMoved(const QString& markerId, int newFrame);
    void onZoomChanged(int value);
    void onClipSelected(const QString& clipId);
    void onClipRightClicked(const QString& clipId, const QPoint& pos);
    void onClipMoved(const QString& clipId, int deltaX);
    void onClipTrimLeft(const QString& clipId, int deltaX);
    void onClipTrimRight(const QString& clipId, int deltaX);
    void onTransitionResized(const QString& transitionId, int newDuration);
    void onTransitionRightClicked(const QString& transitionId, const QPoint& pos);

private:
    struct Impl;
    Impl* impl_ = nullptr;
    void addTrackRow(const ArtifactPr::DemoTrack& track, const QVector<ArtifactPr::Transition>& transitions);
};
