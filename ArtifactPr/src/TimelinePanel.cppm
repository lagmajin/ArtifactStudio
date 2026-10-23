module;
#include <wobjectdefs.h>
#include <wobjectimpl.h>
#include <QAction>
#include <QColor>
#include <QFont>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QRect>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QHash>
#include <QEvent>
#include <QPointF>
#include <QSizePolicy>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

module ArtifactPr.TimelinePanel;

import ArtifactPr.AppTheme;
import ArtifactPr.EditorEngine;
import ArtifactPr.Translation;

using ArtifactPr::trText;
using FramePosition = ArtifactPr::FramePosition;

int FRAME_WIDTH = 2;
const int MIN_CLIP_WIDTH = 20;
const int TRIM_HANDLE_WIDTH = 6;

int sequenceFrameRate(const ArtifactPr::DemoSequence& sequence)
{
    bool ok = false;
    const double parsed = sequence.frameRate.section(QChar(' '), 0, 0).toDouble(&ok);
    if (!ok || parsed <= 0.0) return 30;
    return qMax(1, static_cast<int>(parsed + 0.5));
}

class TimelineClipWidget : public QWidget
{
    W_OBJECT(TimelineClipWidget)
public:
    TimelineClipWidget(const QString& clipId, const QString& name, const QString& color, bool selected, bool isAudio, QWidget* parent = nullptr)
        : QWidget(parent), clipId_(clipId), name_(name), color_(color), selected_(selected), isAudio_(isAudio)
    {
        setMinimumHeight(24);
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        setCursor(Qt::SizeHorCursor);
        setAttribute(Qt::WA_Hover);

        if (isAudio_) {
            generateWaveform();
        }
    }

    QString clipId() const { return clipId_; }
    void setSelected(bool s) { selected_ = s; update(); }

    void setSpeed(double speed) { speed_ = speed; }
    void setReversed(bool reversed) { reversed_ = reversed; }

    /// 長さ (フレーム)。ズーム時に幅を算出し直すために保持する。
    void setDuration(int duration) { duration_ = duration; }
    int duration() const { return duration_; }

    /// 名前変更を既存 widget へ反映するための setter。
    void setName(const QString& name) { name_ = name; update(); }

    /// 実波形 peak envelope を差し替え (AudioPreviewMixer のデコード結果)。
    /// peaks は 0..1 正規化。空ならランダムのまま描画しない。
    void setWaveformPeaks(const QVector<float>& peaks)
    {
        if (peaks.isEmpty()) return;
        waveform_ = peaks;
        update();
    }

signals:
    void clipSelected(const QString& clipId) W_SIGNAL(clipSelected, clipId);
    void clipMoved(const QString& clipId, int deltaX) W_SIGNAL(clipMoved, clipId, deltaX);
    void clipTrimLeft(const QString& clipId, int deltaX) W_SIGNAL(clipTrimLeft, clipId, deltaX);
    void clipTrimRight(const QString& clipId, int deltaX) W_SIGNAL(clipTrimRight, clipId, deltaX);
    void clipRightClicked(const QString& clipId, const QPoint& pos) W_SIGNAL(clipRightClicked, clipId, pos);

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QRect r = rect().adjusted(1, 1, -1, -1);

        QColor baseColor(color_);
        QColor bgColor = selected_ ? baseColor.lighter(120) : baseColor;
        bgColor.setAlpha(220);
        p.setBrush(bgColor);

        QColor borderColor = selected_ ? Qt::white : baseColor.darker(140);
        p.setPen(QPen(borderColor, selected_ ? 2 : 1));
        p.drawRoundedRect(r, 3, 3);

        if (selected_) {
            QColor highlightColor(255, 255, 255, 60);
            p.fillRect(r.adjusted(2, 2, -2, -2), highlightColor);
        }

        if (isAudio_ && !waveform_.isEmpty()) {
            drawWaveform(p, r);
        }

        p.setPen(selected_ ? Qt::white : Qt::white);
        QFont f = p.font();
        f.setPointSize(9);
        p.setFont(f);

        QString displayName = name_;
        if (width() < 60) {
            displayName = name_.left(3);
        } else if (width() < 100) {
            displayName = name_.left(6);
        }
        p.drawText(r.adjusted(6, 0, -6, 0), Qt::AlignLeft | Qt::AlignVCenter, displayName);
    }

private:
    void generateWaveform()
    {
        waveform_.clear();
        int sampleCount = 100;
        for (int i = 0; i < sampleCount; ++i) {
            float amplitude = 0.3f + 0.7f * static_cast<float>(QRandomGenerator::global()->generateDouble());
            waveform_.append(amplitude);
        }
    }

    void drawWaveform(QPainter& p, const QRect& r)
    {
        QColor waveColor(255, 255, 255, 120);
        p.setPen(waveColor);

        int centerY = r.center().y();
        int waveHeight = r.height() * 0.6f;

        QPainterPath path;
        path.moveTo(r.left(), centerY);

        for (int i = 0; i < waveform_.size(); ++i) {
            float x = r.left() + (static_cast<float>(i) / waveform_.size()) * r.width();
            float y = centerY - waveform_[i] * waveHeight * 0.5f;
            if (i == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }

        for (int i = waveform_.size() - 1; i >= 0; --i) {
            float x = r.left() + (static_cast<float>(i) / waveform_.size()) * r.width();
            float y = centerY + waveform_[i] * waveHeight * 0.5f;
            path.lineTo(x, y);
        }
        path.closeSubpath();

        QColor fillColor(200, 200, 200, 40);
        p.setBrush(fillColor);
        p.drawPath(path);

        p.setPen(QColor(255, 255, 255, 80));
        p.setBrush(Qt::NoBrush);

        for (int i = 0; i < waveform_.size(); ++i) {
            float x = r.left() + (static_cast<float>(i) / waveform_.size()) * r.width();
            float yTop = centerY - waveform_[i] * waveHeight * 0.5f;
            float yBottom = centerY + waveform_[i] * waveHeight * 0.5f;
            p.drawLine(QPointF(x, yTop), QPointF(x, yBottom));
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::RightButton) {
            Q_EMIT clipRightClicked(clipId_, event->globalPos());
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            dragStartPos_ = event->pos();
            isDragging_ = false;
            isTrimmingLeft_ = false;
            isTrimmingRight_ = false;

            if (event->pos().x() < TRIM_HANDLE_WIDTH) {
                isTrimmingLeft_ = true;
            } else if (event->pos().x() > width() - TRIM_HANDLE_WIDTH) {
                isTrimmingRight_ = true;
            }

            Q_EMIT clipSelected(clipId_);
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (event->buttons() & Qt::LeftButton) {
            if (!isTrimmingLeft_ && !isTrimmingRight_) {
                int diff = event->pos().x() - dragStartPos_.x();
                if (!isDragging_ && qAbs(diff) > 5) {
                    isDragging_ = true;
                }
                if (isDragging_) {
                    Q_EMIT clipMoved(clipId_, diff);
                    dragStartPos_ = event->pos();
                }
            } else {
                int diff = event->pos().x() - dragStartPos_.x();
                if (qAbs(diff) > 2) {
                    if (isTrimmingLeft_) {
                        Q_EMIT clipTrimLeft(clipId_, diff);
                    } else {
                        Q_EMIT clipTrimRight(clipId_, diff);
                    }
                    dragStartPos_ = event->pos();
                }
            }
        }
    }

    QString clipId_;
    QString name_;
    QString color_;
    bool selected_;
    bool isAudio_;
    QVector<float> waveform_;
    double speed_ = 1.0;
    bool reversed_ = false;
    int duration_ = 0;
    QPoint dragStartPos_;
    bool isDragging_;
    bool isTrimmingLeft_;
    bool isTrimmingRight_;
};

W_OBJECT_IMPL(TimelineClipWidget)

class TimelineRulerWidget : public QWidget
{
    W_OBJECT(TimelineRulerWidget)
public:
    TimelineRulerWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setProperty(ArtifactPr::kPropSurfaceKind, QString::fromUtf8(ArtifactPr::kSurfaceTimelineRuler));
        setAutoFillBackground(true);
        setAttribute(Qt::WA_Hover);
        setMouseTracking(true);
    }

    void setMarkers(const QVector<ArtifactPr::Marker>& markers) { markers_ = markers; update(); }
    void setFrameRate(int fps) { frameRate_ = qMax(1, fps); update(); }
    void setTransitions(const QVector<ArtifactPr::Transition>& transitions) { transitions_ = transitions; }

signals:
    void markerClicked(const QString& markerId) W_SIGNAL(markerClicked, markerId);
    void markerRightClicked(const QString& markerId, const QPoint& pos) W_SIGNAL(markerRightClicked, markerId, pos);
    void markerMoved(const QString& markerId, int newFrame) W_SIGNAL(markerMoved, markerId, newFrame);

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QRect rect = this->rect();

        p.fillRect(rect, QColor(42, 42, 42));

        QFont font = p.font();
        font.setPointSize(8);
        p.setFont(font);

        int duration = 400;
        const int tickStep = frameRate_;

        for (int frame = 0; frame < duration; frame += tickStep) {
            int x = frame * FRAME_WIDTH;

            p.setPen(QColor(80, 80, 80));
            p.drawLine(x, rect.height() - 8, x, rect.height());

            p.setPen(QColor(150, 150, 150));
            int seconds = frame / frameRate_;
            p.drawText(x + 2, 10, QStringLiteral("%1s").arg(seconds));
        }

        for (const auto& marker : markers_) {
            int x = static_cast<int>(marker.position) * FRAME_WIDTH;
            QColor markerColor = marker.color;
            markerColor.setAlpha(220);
            p.setPen(markerColor);
            p.drawLine(x, 4, x, rect.height() - 8);

            p.setBrush(markerColor);
            QPoint triangle[3] = {
                QPoint(x - 4, 0),
                QPoint(x + 4, 0),
                QPoint(x, 6)
            };
            p.drawPolygon(triangle, 3);

            if (!marker.name.isEmpty() && marker.name != QStringLiteral("Marker %1").arg(1)) {
                p.setPen(Qt::white);
                p.drawText(x + 6, 12, marker.name);
            }
        }

        for (const auto& trans : transitions_) {
            int x = static_cast<int>(trans.startFrame) * FRAME_WIDTH;
            int w = qMax(4, static_cast<int>(trans.duration) * FRAME_WIDTH);

            QColor transColor;
            switch (trans.type) {
            case ArtifactPr::TransitionType::Crossfade:
                transColor = QColor(200, 150, 100);
                break;
            case ArtifactPr::TransitionType::DipToBlack:
                transColor = QColor(100, 100, 100);
                break;
            case ArtifactPr::TransitionType::WipeLeft:
            case ArtifactPr::TransitionType::WipeRight:
                transColor = QColor(150, 100, 200);
                break;
            default:
                transColor = QColor(180, 180, 100);
            }
            transColor.setAlpha(180);
            p.fillRect(x, rect.height() - 6, w, 4, transColor);
        }

        auto* engine = ArtifactPr::EditorEngine::instance();
        int playheadX = static_cast<int>(engine->currentFrame()) * FRAME_WIDTH;
        p.setPen(QColor(255, 50, 50));
        p.drawLine(playheadX, 0, playheadX, rect.height());
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            int clickX = event->pos().x();
            for (const auto& marker : markers_) {
                int markerX = static_cast<int>(marker.position) * FRAME_WIDTH;
                if (qAbs(clickX - markerX) < 8) {
                    selectedMarkerId_ = marker.id;
                    dragStartX_ = clickX;
                    isDraggingMarker_ = true;
                    Q_EMIT markerClicked(marker.id);
                    return;
                }
            }
        } else if (event->button() == Qt::RightButton) {
            int clickX = event->pos().x();
            for (const auto& marker : markers_) {
                int markerX = static_cast<int>(marker.position) * FRAME_WIDTH;
                if (qAbs(clickX - markerX) < 8) {
                    Q_EMIT markerRightClicked(marker.id, event->globalPos());
                    return;
                }
            }
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (isDraggingMarker_ && (event->buttons() & Qt::LeftButton)) {
            int diff = event->pos().x() - dragStartX_;
            int frameDelta = diff / FRAME_WIDTH;
            if (qAbs(frameDelta) > 0) {
                auto* engine = ArtifactPr::EditorEngine::instance();
                for (const auto& marker : markers_) {
                    if (marker.id == selectedMarkerId_) {
                        FramePosition newPos = qMax(0, marker.position + frameDelta);
                        Q_EMIT markerMoved(selectedMarkerId_, static_cast<int>(newPos));
                        dragStartX_ = event->pos().x();
                        break;
                    }
                }
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        isDraggingMarker_ = false;
        selectedMarkerId_.clear();
    }

private:
    QVector<ArtifactPr::Marker> markers_;
    QVector<ArtifactPr::Transition> transitions_;
    int frameRate_ = 30;
    QString selectedMarkerId_;
    int dragStartX_;
    bool isDraggingMarker_ = false;
};

W_OBJECT_IMPL(TimelineRulerWidget)

class TimelineRulerWidget;

class TransitionWidget : public QWidget
{
    W_OBJECT(TransitionWidget)
public:
    TransitionWidget(const ArtifactPr::Transition& trans, QWidget* parent = nullptr)
        : QWidget(parent), trans_(trans), originalDuration_(trans.duration)
    {
        setMinimumHeight(24);
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }

    ArtifactPr::Transition transition() const { return trans_; }
    void setTransition(const ArtifactPr::Transition& trans) { trans_ = trans; update(); }

signals:
    void transitionResized(const QString& transitionId, int newDuration) W_SIGNAL(transitionResized, transitionId, newDuration);
    void transitionRightClicked(const QString& transitionId, const QPoint& pos) W_SIGNAL(transitionRightClicked, transitionId, pos);

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QColor transColor;
        switch (trans_.type) {
        case ArtifactPr::TransitionType::Crossfade:
            transColor = QColor(200, 150, 100);
            break;
        case ArtifactPr::TransitionType::DipToBlack:
            transColor = QColor(100, 100, 100);
            break;
        case ArtifactPr::TransitionType::WipeLeft:
        case ArtifactPr::TransitionType::WipeRight:
            transColor = QColor(150, 100, 200);
            break;
        default:
            transColor = QColor(180, 180, 100);
        }

        QRect r = rect().adjusted(1, 1, -1, -1);
        transColor.setAlpha(160);
        p.setBrush(transColor);
        p.setPen(QColor(255, 255, 255, 100));
        p.drawRect(r);

        QColor handleColor(255, 255, 255, 60);
        p.fillRect(r.left(), r.top(), 4, r.height(), handleColor);
        p.fillRect(r.right() - 4, r.top(), 4, r.height(), handleColor);

        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPointSize(7);
        p.setFont(f);

        QString name;
        switch (trans_.type) {
        case ArtifactPr::TransitionType::Crossfade:
            name = "XF";
            break;
        case ArtifactPr::TransitionType::DipToBlack:
            name = "DIP";
            break;
        case ArtifactPr::TransitionType::WipeLeft:
            name = "W<L";
            break;
        case ArtifactPr::TransitionType::WipeRight:
            name = "W>R";
            break;
        default:
            name = "TR";
        }
        p.drawText(r, Qt::AlignCenter, name);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            dragStartPos_ = event->pos();
            isResizingLeft_ = (event->pos().x() < 8);
            isResizingRight_ = (event->pos().x() > width() - 8);
            isDragging_ = false;
        } else if (event->button() == Qt::RightButton) {
            Q_EMIT transitionRightClicked(trans_.id, event->globalPos());
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (event->buttons() & Qt::LeftButton) {
            int diff = event->pos().x() - dragStartPos_.x();
            if (qAbs(diff) > 3) {
                int durationDelta = diff / FRAME_WIDTH;
                int newDuration = qMax(6, originalDuration_ + durationDelta);
                Q_EMIT transitionResized(trans_.id, newDuration);
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        originalDuration_ = trans_.duration;
    }

private:
    ArtifactPr::Transition trans_;
    int originalDuration_;
    QPoint dragStartPos_;
    bool isResizingLeft_;
    bool isResizingRight_;
    bool isDragging_;
};

W_OBJECT_IMPL(TransitionWidget)

struct ArtifactPrTimelinePanel::Impl
{
    TimelineRulerWidget* ruler_ = nullptr;
    QLabel* sequenceInfo_ = nullptr;
    QSlider* zoomSlider_ = nullptr;
    QLabel* zoomLevelLabel_ = nullptr;
    float zoomLevel_ = 1.0f;
    QScrollArea* scrollArea_ = nullptr;
    QWidget* timelineHost_ = nullptr;
    QVBoxLayout* timelineLayout_ = nullptr;
    QHash<QString, TimelineClipWidget*> clipWidgets_;
    QHash<QString, TransitionWidget*> transitionWidgets_;
    QHash<QString, QWidget*> trackContentWidgets_;
    QHash<QString, QPushButton*> trackMuteButtons_;
    QHash<QString, QPushButton*> trackSoloButtons_;
    std::function<const QVector<float>*(const QString&)> waveformProvider_;
    // 現在の widget 木が表す構造。構造が変わったときだけ作り直す。
    QString structureSignature_;
};

W_OBJECT_IMPL(ArtifactPrTimelinePanel)

bool ArtifactPrTimelinePanel::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == impl_->ruler_ && event->type() == QEvent::MouseButtonDblClick) {
        auto* me = static_cast<QMouseEvent*>(event);
        FramePosition frame = me->pos().x() / FRAME_WIDTH;
        auto* engine = ArtifactPr::EditorEngine::instance();
        engine->addMarker(frame);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void ArtifactPrTimelinePanel::refreshTimeline(const ArtifactPr::DemoSequence& seq)
    {
        // widget 木は構造 (トラック / クリップ / トランジションの集合) が変わった
        // ときだけ作り直す。ズームやトリムのような見た目だけの変更は、既存
        // widget の幅と名前だけを更新する。作り直すとスクロール位置が毎回
        // リセットされ、ズームのたびに全 clip widget が破棄されていた。
        const QString signature = structureSignature(seq);
        if (signature != impl_->structureSignature_) {
            rebuildTimeline(seq);
            impl_->structureSignature_ = signature;
        }

        impl_->sequenceInfo_->setText(
            trText("pr.timeline.sequence_info",
                   "%1 | %2 | Duration: %3 | Markers: %4 | Transitions: %5")
                .arg(seq.name).arg(seq.frameRate).arg(seq.duration)
                .arg(seq.markers.size()).arg(seq.transitions.size()));

        impl_->ruler_->setFrameRate(sequenceFrameRate(seq));
        impl_->ruler_->setMarkers(seq.markers);
        impl_->ruler_->setTransitions(seq.transitions);
        impl_->ruler_->update();

        updateTimelineGeometry(seq);
    }

void ArtifactPrTimelinePanel::rebuildTimeline(const ArtifactPr::DemoSequence& seq)
    {
        impl_->clipWidgets_.clear();
        impl_->transitionWidgets_.clear();
        impl_->trackContentWidgets_.clear();
        impl_->trackMuteButtons_.clear();
        impl_->trackSoloButtons_.clear();
        while (impl_->timelineLayout_->count() > 0) {
            auto* item = impl_->timelineLayout_->takeAt(0);
            if (item->widget())
                delete item->widget();
            delete item;
        }

        for (const auto& track : seq.videoTracks) {
            addTrackRow(track, seq.transitions);
        }
        for (const auto& track : seq.audioTracks) {
            addTrackRow(track, seq.transitions);
        }

        impl_->timelineLayout_->addStretch(1);
    }

void ArtifactPrTimelinePanel::updateTimelineGeometry(const ArtifactPr::DemoSequence& seq)
    {
        const auto updateTrack = [this](const ArtifactPr::DemoTrack& track) {
            for (const auto& clip : track.clips) {
                auto it = impl_->clipWidgets_.find(clip.id);
                if (it == impl_->clipWidgets_.end()) continue;
                TimelineClipWidget* widget = it.value();
                widget->setDuration(clip.duration);
                widget->setName(clip.name);
                widget->setSpeed(clip.speed);
                widget->setReversed(clip.reversed);
                if (track.kind == QStringLiteral("audio") && impl_->waveformProvider_) {
                    const QVector<float>* peaks = impl_->waveformProvider_(clip.sourceFile);
                    if (peaks) widget->setWaveformPeaks(*peaks);
                }
                const int width = qMax(MIN_CLIP_WIDTH, static_cast<int>(clip.duration * FRAME_WIDTH));
                widget->setMinimumWidth(width);
                widget->setMaximumWidth(width);
            }
        };
        for (const auto& track : seq.videoTracks) updateTrack(track);
        for (const auto& track : seq.audioTracks) updateTrack(track);

        for (const auto& trans : seq.transitions) {
            auto it = impl_->transitionWidgets_.find(trans.id);
            if (it == impl_->transitionWidgets_.end()) continue;
            TransitionWidget* widget = it.value();
            widget->setTransition(trans);
            const int width = qMax(MIN_CLIP_WIDTH, static_cast<int>(trans.duration * FRAME_WIDTH));
            widget->setMinimumWidth(width);
            widget->setMaximumWidth(width);
        }

        // プログラム側で mute / solo を変えた場合にボタンへ反映する。
        // setChecked は toggled を発火するため、エンジンへの逆流を防ぐ。
        const auto updateTrackState = [this](const ArtifactPr::DemoTrack& track) {
            auto mute = impl_->trackMuteButtons_.find(track.id);
            if (mute != impl_->trackMuteButtons_.end()) {
                const QSignalBlocker blocker(mute.value());
                mute.value()->setChecked(track.muted);
            }
            auto solo = impl_->trackSoloButtons_.find(track.id);
            if (solo != impl_->trackSoloButtons_.end()) {
                const QSignalBlocker blocker(solo.value());
                solo.value()->setChecked(track.solo);
            }
        };
        for (const auto& track : seq.videoTracks) updateTrackState(track);
        for (const auto& track : seq.audioTracks) updateTrackState(track);

        const int totalWidth = qMax(800, static_cast<int>(seq.duration * FRAME_WIDTH) + 100);
        for (auto it = impl_->trackContentWidgets_.cbegin(); it != impl_->trackContentWidgets_.cend(); ++it) {
            it.value()->setMinimumWidth(totalWidth);
        }
    }

QString ArtifactPrTimelinePanel::structureSignature(const ArtifactPr::DemoSequence& seq)
    {
        // 構造が変わったかどうかの判定用。トラック / クリップ / トランジションの
        // id 集合と、トラックごとの名前と種別だけを見る。
        QStringList parts;
        const auto appendTrack = [&parts, &seq](const ArtifactPr::DemoTrack& track) {
            QStringList clipIds;
            for (const auto& clip : track.clips) clipIds << clip.id;
            QStringList transIds;
            for (const auto& trans : seq.transitions) {
                if (trans.trackId == track.id) transIds << trans.id;
            }
            parts << (track.kind + QLatin1Char(':') + track.id + QLatin1Char(':') + track.name
                      + QLatin1Char('|') + clipIds.join(QLatin1Char(','))
                      + QLatin1Char('|') + transIds.join(QLatin1Char(',')));
        };
        for (const auto& track : seq.videoTracks) appendTrack(track);
        for (const auto& track : seq.audioTracks) appendTrack(track);
        return parts.join(QLatin1Char(';'));
    }

void ArtifactPrTimelinePanel::onFrameChanged(ArtifactPr::FramePosition) { impl_->ruler_->update(); }
void ArtifactPrTimelinePanel::onClipSelectionChanged(const QString& clipId)
    {
        for (auto it = impl_->clipWidgets_.cbegin(); it != impl_->clipWidgets_.cend(); ++it) {
            it.value()->setSelected(it.key() == clipId);
        }
    }
void ArtifactPrTimelinePanel::onMarkerChanged() { impl_->ruler_->update(); }
void ArtifactPrTimelinePanel::onTransitionChanged() { refreshTimeline(ArtifactPr::EditorEngine::instance()->currentSequence()); }

void ArtifactPrTimelinePanel::onMarkerClicked(const QString& markerId)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        for (const auto& marker : engine->markers()) {
            if (marker.id == markerId) {
                engine->seekToFrame(marker.position);
                break;
            }
        }
    }

void ArtifactPrTimelinePanel::onMarkerRightClicked(const QString& markerId, const QPoint& pos)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();

        auto* menu = new QMenu();

        auto* renameAction = menu->addAction(trText("pr.action.rename_marker", "Rename Marker..."));
        connect(renameAction, &QAction::triggered, [this, engine, markerId]() {
            for (const auto& marker : engine->markers()) {
                if (marker.id == markerId) {
                    bool ok;
                    QString newName = QInputDialog::getText(this, trText("pr.dialog.rename_marker.title", "Rename Marker"),
                        trText("pr.field.marker_name", "Marker Name:"), QLineEdit::Normal, marker.name, &ok);
                    if (ok && !newName.isEmpty()) {
                        engine->setMarkerName(markerId, newName);
                    }
                    break;
                }
            }
        });

        auto* goToAction = menu->addAction(trText("pr.action.go_to_marker", "Go To Marker"));
        connect(goToAction, &QAction::triggered, [engine, markerId]() {
            for (const auto& marker : engine->markers()) {
                if (marker.id == markerId) {
                    engine->seekToFrame(marker.position);
                    break;
                }
            }
        });

        menu->addSeparator();

        auto* deleteAction = menu->addAction(trText("pr.action.delete_marker", "Delete Marker"));
        connect(deleteAction, &QAction::triggered, [engine, markerId]() {
            engine->deleteMarker(markerId);
        });

        menu->exec(pos);
        delete menu;
    }

void ArtifactPrTimelinePanel::onMarkerMoved(const QString& markerId, int newFrame)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        engine->moveMarker(markerId, newFrame);
        impl_->ruler_->update();
    }

void ArtifactPrTimelinePanel::onZoomChanged(int value)
    {
        impl_->zoomLevel_ = value / 5.0f;
        impl_->zoomLevelLabel_->setText(QStringLiteral("%1x").arg(impl_->zoomLevel_, 0, 'f', 1));

        FRAME_WIDTH = qMax(1, static_cast<int>(2 * impl_->zoomLevel_));
        refreshTimeline(ArtifactPr::EditorEngine::instance()->currentSequence());
    }

void ArtifactPrTimelinePanel::onClipSelected(const QString& clipId)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        engine->selectClip(clipId);
    }

void ArtifactPrTimelinePanel::onClipRightClicked(const QString& clipId, const QPoint& pos)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        engine->selectClip(clipId);

        auto* clip = engine->findClip(clipId);
        if (!clip) return;

        auto* menu = new QMenu();

        auto* speedMenu = menu->addMenu(trText("pr.menu.speed", "Speed"));

        auto* speed100 = speedMenu->addAction(QStringLiteral("100%"));
        connect(speed100, &QAction::triggered, [engine, clipId]() { engine->setClipSpeed(clipId, 1.0); });

        auto* speed200 = speedMenu->addAction(QStringLiteral("200%"));
        connect(speed200, &QAction::triggered, [engine, clipId]() { engine->setClipSpeed(clipId, 2.0); });

        auto* speed50 = speedMenu->addAction(QStringLiteral("50%"));
        connect(speed50, &QAction::triggered, [engine, clipId]() { engine->setClipSpeed(clipId, 0.5); });

        auto* speed25 = speedMenu->addAction(QStringLiteral("25%"));
        connect(speed25, &QAction::triggered, [engine, clipId]() { engine->setClipSpeed(clipId, 0.25); });

        auto* speed400 = speedMenu->addAction(QStringLiteral("400%"));
        connect(speed400, &QAction::triggered, [engine, clipId]() { engine->setClipSpeed(clipId, 4.0); });

        menu->addSeparator();

        auto* reverseAction = menu->addAction(trText("pr.action.reverse", "Reverse"));
        reverseAction->setCheckable(true);
        reverseAction->setChecked(clip->reversed);
        connect(reverseAction, &QAction::triggered, [engine, clipId](bool checked) { engine->setClipReversed(clipId, checked); });

        menu->addSeparator();

        // V/A リンク編集 (NLE LinkingService 経由)
        {
            engine->selectClip(clipId);
            const quint64 groupId = engine->selectedClipLinkGroupId();
            const QVector<QString> peers = engine->linkedPeerClipIds();

            if (groupId == 0) {
                auto* linkAction = menu->addAction(trText("pr.action.link_av", "Link AV"));
                linkAction->setToolTip(trText("pr.clip.link_av_tooltip", "Link this clip so trim, slide and selection propagate to peers."));
                connect(linkAction, &QAction::triggered, [engine]() {
                    engine->linkSelectedClips();
                });
            }
            else {
                auto* unlinkAction = menu->addAction(trText("pr.action.unlink_av", "Unlink AV"));
                unlinkAction->setToolTip(trText("pr.label.linked_peers", "Linked peers: %1").arg(peers.size()));
                connect(unlinkAction, &QAction::triggered, [engine]() {
                    engine->unlinkSelectedClips();
                });

                auto* clearPeerMenu = menu->addMenu(trText("pr.menu.linked_clips", "Linked Clips"));
                for (const QString& peerId : peers) {
                    auto* peerAction = clearPeerMenu->addAction(peerId);
                    connect(peerAction, &QAction::triggered, [engine, peerId]() {
                        engine->selectClip(peerId);
                    });
                }
            }
        }

        menu->addSeparator();

        auto* volumeMenu = menu->addMenu(trText("pr.menu.volume", "Volume"));

        auto* vol100 = volumeMenu->addAction(QStringLiteral("100%"));
        connect(vol100, &QAction::triggered, [engine, clipId]() { engine->setClipVolume(clipId, 1.0); });

        auto* vol150 = volumeMenu->addAction(QStringLiteral("150%"));
        connect(vol150, &QAction::triggered, [engine, clipId]() { engine->setClipVolume(clipId, 1.5); });

        auto* vol200 = volumeMenu->addAction(QStringLiteral("200%"));
        connect(vol200, &QAction::triggered, [engine, clipId]() { engine->setClipVolume(clipId, 2.0); });

        auto* vol50 = volumeMenu->addAction(QStringLiteral("50%"));
        connect(vol50, &QAction::triggered, [engine, clipId]() { engine->setClipVolume(clipId, 0.5); });

        auto* vol0 = volumeMenu->addAction(trText("pr.timeline.mute_level", "0% (Mute)"));
        connect(vol0, &QAction::triggered, [engine, clipId]() { engine->setClipVolume(clipId, 0.0); });

        menu->addSeparator();

        auto* nameAction = menu->addAction(trText("pr.action.rename_clip", "Rename Clip..."));
        connect(nameAction, &QAction::triggered, [this, engine, clipId]() {
            auto* clip = engine->findClip(clipId);
            if (!clip) return;
            bool ok;
            QString newName = QInputDialog::getText(
                this,
                trText("pr.dialog.rename_clip.title", "Rename Clip"),
                trText("pr.field.clip_name", "Clip Name:"),
                QLineEdit::Normal, clip->name, &ok);
            if (ok && !newName.isEmpty()) {
                engine->setClipName(clipId, newName);
                refreshTimeline(engine->currentSequence());
            }
        });

        menu->addSeparator();

        auto* deleteAction = menu->addAction(trText("pr.action.delete", "Delete"));
        deleteAction->setShortcut(QKeySequence::Delete);
        connect(deleteAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::deleteSelectedClip);

        menu->exec(pos);
        delete menu;
    }

void ArtifactPrTimelinePanel::onClipMoved(const QString& clipId, int deltaX)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(clipId);
        if (!clip) return;

        int frameDelta = deltaX / FRAME_WIDTH;
        if (frameDelta == 0) return;

        FramePosition newStart = qMax(0, clip->startFrame + frameDelta);
        FramePosition snappedStart = engine->snapToNearest(newStart, true);
        engine->moveClip(clipId, snappedStart);
        refreshTimeline(engine->currentSequence());
    }

void ArtifactPrTimelinePanel::onClipTrimLeft(const QString& clipId, int deltaX)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(clipId);
        if (!clip) return;

        int frameDelta = deltaX / FRAME_WIDTH;
        if (frameDelta == 0) return;

        FramePosition newStart = clip->startFrame + frameDelta;
        FramePosition newDuration = clip->duration - frameDelta;

        if (newDuration < 5) return;
        if (newStart < 0) return;

        FramePosition snappedStart = engine->snapToNearest(newStart, true);
        if (snappedStart != newStart) {
            frameDelta = snappedStart - clip->startFrame;
            newStart = snappedStart;
            newDuration = clip->duration - frameDelta;
        }

        engine->trimClip(clipId,
                         newStart,
                         newDuration,
                         clip->sourceIn + frameDelta,
                         clip->sourceOut);
        refreshTimeline(engine->currentSequence());
    }

void ArtifactPrTimelinePanel::onClipTrimRight(const QString& clipId, int deltaX)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(clipId);
        if (!clip) return;

        int frameDelta = deltaX / FRAME_WIDTH;
        if (frameDelta == 0) return;

        FramePosition newDuration = clip->duration + frameDelta;

        if (newDuration < 5) return;

        engine->trimClip(clipId,
                         clip->startFrame,
                         newDuration,
                         clip->sourceIn,
                         clip->sourceOut + frameDelta);
        refreshTimeline(engine->currentSequence());
    }

void ArtifactPrTimelinePanel::onTransitionResized(const QString& transitionId, int newDuration)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        if (engine->setVideoTransitionDuration(transitionId, newDuration)) {
            Q_EMIT engine->transitionChanged();
            return;
        }
        auto seq = engine->currentSequence();
        auto& transitions = seq.transitions;
        for (auto& trans : transitions) {
            if (trans.id == transitionId) {
                trans.duration = newDuration;
                engine->setCurrentSequence(seq);
                Q_EMIT engine->transitionChanged();
                Q_EMIT engine->projectModified();
                break;
            }
        }
    }

void ArtifactPrTimelinePanel::onTransitionRightClicked(const QString& transitionId, const QPoint& pos)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto seq = engine->currentSequence();

        auto* menu = new QMenu();

        auto* durationMenu = menu->addMenu(trText("pr.menu.duration", "Duration"));

        auto applyTransitionDuration = [engine, seq, transitionId](int duration) mutable {
            if (engine->setVideoTransitionDuration(transitionId, duration)) {
                Q_EMIT engine->transitionChanged();
                return;
            }
            auto updatedSeq = seq;
            for (auto& trans : updatedSeq.transitions) {
                if (trans.id == transitionId) { trans.duration = duration; break; }
            }
            engine->setCurrentSequence(updatedSeq);
            Q_EMIT engine->transitionChanged();
            Q_EMIT engine->projectModified();
        };

        auto* dur6 = durationMenu->addAction(
            trText("pr.timeline.duration_frames", "%1 frames").arg(6));
        connect(dur6, &QAction::triggered, [this, applyTransitionDuration] {
            applyTransitionDuration(6);
            refreshTimeline(ArtifactPr::EditorEngine::instance()->currentSequence());
        });

        auto* dur12 = durationMenu->addAction(
            trText("pr.timeline.duration_frames", "%1 frames").arg(12));
        connect(dur12, &QAction::triggered, [this, applyTransitionDuration] {
            applyTransitionDuration(12);
            refreshTimeline(ArtifactPr::EditorEngine::instance()->currentSequence());
        });

        auto* dur24 = durationMenu->addAction(
            trText("pr.timeline.duration_frames", "%1 frames").arg(24));
        connect(dur24, &QAction::triggered, [this, applyTransitionDuration] {
            applyTransitionDuration(24);
            refreshTimeline(ArtifactPr::EditorEngine::instance()->currentSequence());
        });

        auto* dur48 = durationMenu->addAction(
            trText("pr.timeline.duration_frames", "%1 frames").arg(48));
        connect(dur48, &QAction::triggered, [this, applyTransitionDuration] {
            applyTransitionDuration(48);
            refreshTimeline(ArtifactPr::EditorEngine::instance()->currentSequence());
        });

        menu->addSeparator();

        auto* deleteAction = menu->addAction(trText("pr.action.delete_transition", "Delete Transition"));
        connect(deleteAction, &QAction::triggered, [engine, transitionId]() {
            engine->deleteTransition(transitionId);
        });

        menu->exec(pos);
        delete menu;
    }

void ArtifactPrTimelinePanel::addTrackRow(const ArtifactPr::DemoTrack& track, const QVector<ArtifactPr::Transition>& transitions)
    {
        QString trackColor = (track.kind == QStringLiteral("video")) ? QStringLiteral("#4a9eff") : QStringLiteral("#4eff4a");

        auto* trackWidget = new QWidget();
        trackWidget->setObjectName(QStringLiteral("artifactpr.timeline.track.%1").arg(track.id));
        trackWidget->setMinimumHeight(32);
        auto* trackLayout = new QHBoxLayout(trackWidget);
        trackLayout->setContentsMargins(0, 0, 0, 0);
        trackLayout->setSpacing(0);

        auto* nameLabel = new QLabel(track.name);
        nameLabel->setMinimumWidth(50);
        nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        nameLabel->setProperty(ArtifactPr::kPropSurfaceKind, QString::fromUtf8(ArtifactPr::kSurfacePanelToolbar));
        nameLabel->setAutoFillBackground(true);
        trackLayout->addWidget(nameLabel);

        // mute / solo トグル (TrackHeader 相当)。押下状態はトラック状態を反映。
        const QString trackId = track.id;
        auto* muteButton = new QPushButton(QStringLiteral("M"));
        muteButton->setCheckable(true);
        muteButton->setChecked(track.muted);
        muteButton->setFixedSize(20, 20);
        muteButton->setToolTip(trText("pr.action.mute_track", "Mute track"));
        QObject::connect(muteButton, &QPushButton::toggled, muteButton,
            [trackId](bool checked) {
                ArtifactPr::EditorEngine::instance()->setTrackMuted(trackId, checked);
            });
        impl_->trackMuteButtons_.insert(track.id, muteButton);
        trackLayout->addWidget(muteButton);

        auto* soloButton = new QPushButton(QStringLiteral("S"));
        soloButton->setCheckable(true);
        soloButton->setChecked(track.solo);
        soloButton->setFixedSize(20, 20);
        soloButton->setToolTip(trText("pr.action.solo_track", "Solo track"));
        QObject::connect(soloButton, &QPushButton::toggled, soloButton,
            [trackId](bool checked) {
                ArtifactPr::EditorEngine::instance()->setTrackSolo(trackId, checked);
            });
        impl_->trackSoloButtons_.insert(track.id, soloButton);
        trackLayout->addWidget(soloButton);

        auto* trackContent = new QWidget();
        trackContent->setProperty(ArtifactPr::kPropSurfaceKind, QString::fromUtf8(ArtifactPr::kSurfaceTrackContent));
        trackContent->setAutoFillBackground(true);
        trackContent->setMinimumHeight(28);
        auto* contentLayout = new QHBoxLayout(trackContent);
        contentLayout->setContentsMargins(4, 2, 4, 2);
        contentLayout->setSpacing(0);

        auto* engine = ArtifactPr::EditorEngine::instance();
        QString selectedId = engine->selectedClipId();

        for (const auto& clip : track.clips) {
            bool isAudio = (track.kind == QStringLiteral("audio"));
            auto* clipWidget = new TimelineClipWidget(clip.id, clip.name, trackColor, clip.id == selectedId, isAudio, trackContent);
            clipWidget->setObjectName(QStringLiteral("artifactpr.timeline.clip.%1").arg(clip.id));
            impl_->clipWidgets_.insert(clip.id, clipWidget);
            // 幅はズーム時に算出し直すため、clip 自身の長さを保持する。
            clipWidget->setDuration(clip.duration);
            int clipWidth = qMax(MIN_CLIP_WIDTH, static_cast<int>(clip.duration * FRAME_WIDTH));
            clipWidget->setMinimumWidth(clipWidth);
            clipWidget->setMaximumWidth(clipWidth);

            if (clip.speed != 1.0) {
                clipWidget->setSpeed(clip.speed);
            }
            if (clip.reversed) {
                clipWidget->setReversed(true);
            }

            // 音声クリップは実波形 peak envelope を適用 (B2)。
            if (isAudio && impl_->waveformProvider_) {
                const QVector<float>* peaks = impl_->waveformProvider_(clip.sourceFile);
                if (peaks) {
                    clipWidget->setWaveformPeaks(*peaks);
                }
            }

            connect(clipWidget, &TimelineClipWidget::clipSelected, this, &ArtifactPrTimelinePanel::onClipSelected);
            connect(clipWidget, &TimelineClipWidget::clipMoved, this, &ArtifactPrTimelinePanel::onClipMoved);
            connect(clipWidget, &TimelineClipWidget::clipTrimLeft, this, &ArtifactPrTimelinePanel::onClipTrimLeft);
            connect(clipWidget, &TimelineClipWidget::clipTrimRight, this, &ArtifactPrTimelinePanel::onClipTrimRight);
            connect(clipWidget, &TimelineClipWidget::clipRightClicked, this, &ArtifactPrTimelinePanel::onClipRightClicked);

            contentLayout->addWidget(clipWidget);

            for (const auto& trans : transitions) {
                if (trans.trackId == track.id && (trans.rightClipId == clip.id || trans.leftClipId == clip.id)) {
                    auto* transWidget = new TransitionWidget(trans, trackContent);
                    impl_->transitionWidgets_.insert(trans.id, transWidget);
                    int transWidth = qMax(MIN_CLIP_WIDTH, static_cast<int>(trans.duration * FRAME_WIDTH));
                    transWidget->setMinimumWidth(transWidth);
                    transWidget->setMaximumWidth(transWidth);
                    connect(transWidget, &TransitionWidget::transitionResized, this, &ArtifactPrTimelinePanel::onTransitionResized);
                    connect(transWidget, &TransitionWidget::transitionRightClicked, this, &ArtifactPrTimelinePanel::onTransitionRightClicked);
                    contentLayout->addWidget(transWidget);
                }
            }
        }
        contentLayout->addStretch(1);

        impl_->trackContentWidgets_.insert(track.id, trackContent);
        int totalWidth = qMax(800, static_cast<int>(engine->currentSequence().duration * FRAME_WIDTH) + 100);
        trackContent->setMinimumWidth(totalWidth);
        trackLayout->addWidget(trackContent, 1);
        impl_->timelineLayout_->addWidget(trackWidget);
    }

ArtifactPrTimelinePanel::ArtifactPrTimelinePanel(QWidget* parent)
        : QWidget(parent), impl_(new Impl)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        impl_->ruler_ = new TimelineRulerWidget();
        setObjectName(QStringLiteral("artifactpr.timeline.panel"));
        impl_->ruler_->setObjectName(QStringLiteral("artifactpr.timeline.ruler"));
        impl_->ruler_->setMinimumHeight(24);
        layout->addWidget(impl_->ruler_);

        auto* toolbarWidget = new QWidget();
        toolbarWidget->setProperty(ArtifactPr::kPropSurfaceKind, QString::fromUtf8(ArtifactPr::kSurfacePanelToolbar));
        toolbarWidget->setAutoFillBackground(true);
        auto* toolbarLayout = new QHBoxLayout(toolbarWidget);
        toolbarLayout->setContentsMargins(4, 2, 4, 2);
        toolbarLayout->setSpacing(8);

        auto* zoomLabel = new QLabel(trText("pr.field.zoom", "Zoom:"));
        toolbarLayout->addWidget(zoomLabel);

        impl_->zoomSlider_ = new QSlider(Qt::Horizontal);
        impl_->zoomSlider_->setObjectName(QStringLiteral("zoomSlider_"));
        impl_->zoomSlider_->setMinimum(1);
        impl_->zoomSlider_->setMaximum(10);
        impl_->zoomSlider_->setValue(5);
        impl_->zoomSlider_->setMaximumWidth(120);
        impl_->zoomSlider_->setTickPosition(QSlider::NoTicks);
        connect(impl_->zoomSlider_, &QSlider::valueChanged, this, &ArtifactPrTimelinePanel::onZoomChanged);
        toolbarLayout->addWidget(impl_->zoomSlider_);

        impl_->zoomLevelLabel_ = new QLabel(QStringLiteral("1.0x"));
        impl_->zoomLevelLabel_->setObjectName(QStringLiteral("zoomLevelLabel_"));
        impl_->zoomLevelLabel_->setMinimumWidth(40);
        toolbarLayout->addWidget(impl_->zoomLevelLabel_);

        toolbarLayout->addStretch();

        impl_->sequenceInfo_ = new QLabel(trText("pr.status.no_sequence", "No sequence"));
        impl_->sequenceInfo_->setObjectName(QStringLiteral("sequenceInfo_"));
        toolbarLayout->addWidget(impl_->sequenceInfo_);

        layout->addWidget(toolbarWidget);

        impl_->scrollArea_ = new QScrollArea(this);
        impl_->scrollArea_->setWidgetResizable(true);
        impl_->scrollArea_->setFrameShape(QFrame::NoFrame);
        impl_->scrollArea_->setMinimumHeight(180);
        impl_->scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

        impl_->timelineHost_ = new QWidget();
        impl_->timelineLayout_ = new QVBoxLayout(impl_->timelineHost_);
        impl_->timelineLayout_->setContentsMargins(0, 0, 0, 0);
        impl_->timelineLayout_->setSpacing(2);

        impl_->scrollArea_->setWidget(impl_->timelineHost_);
        layout->addWidget(impl_->scrollArea_, 1);

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::sequenceChanged, this, &ArtifactPrTimelinePanel::refreshTimeline);
        connect(engine, &ArtifactPr::EditorEngine::currentFrameChanged, this, &ArtifactPrTimelinePanel::onFrameChanged);
        connect(engine, &ArtifactPr::EditorEngine::clipSelectionChanged, this, &ArtifactPrTimelinePanel::onClipSelectionChanged);
        connect(engine, &ArtifactPr::EditorEngine::markerChanged, this, &ArtifactPrTimelinePanel::onMarkerChanged);
        connect(engine, &ArtifactPr::EditorEngine::transitionChanged, this, &ArtifactPrTimelinePanel::onTransitionChanged);

        connect(impl_->ruler_, &TimelineRulerWidget::markerClicked, this, &ArtifactPrTimelinePanel::onMarkerClicked);
        connect(impl_->ruler_, &TimelineRulerWidget::markerRightClicked, this, &ArtifactPrTimelinePanel::onMarkerRightClicked);
        connect(impl_->ruler_, &TimelineRulerWidget::markerMoved, this, &ArtifactPrTimelinePanel::onMarkerMoved);

        impl_->ruler_->installEventFilter(this);

        refreshTimeline(engine->currentSequence());
    }

void ArtifactPrTimelinePanel::setWaveformProvider(std::function<const QVector<float>*(const QString&)> provider)
    {
        impl_->waveformProvider_ = std::move(provider);
        refreshTimeline(ArtifactPr::EditorEngine::instance()->currentSequence());
    }

ArtifactPrTimelinePanel::~ArtifactPrTimelinePanel()
{
    delete impl_;
    impl_ = nullptr;
}
