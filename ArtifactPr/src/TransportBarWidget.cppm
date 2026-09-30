module;
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QFont>
#include <QChar>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QHBoxLayout>
#include <QtGlobal>
#include <wobjectimpl.h>

module ArtifactPr.TransportBarWidget;

namespace {

int sequenceFrameRate(const ArtifactPr::DemoSequence& sequence)
{
    bool ok = false;
    const double parsed = sequence.frameRate.section(QChar(' '), 0, 0).toDouble(&ok);
    if (!ok || parsed <= 0.0) return 30;
    return qMax(1, static_cast<int>(parsed + 0.5));
}

} // namespace

TransportBarWidget::TransportBarWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    auto addButton = [layout](const QString& text) {
        auto* button = new QPushButton(text);
        button->setFixedWidth(40);
        button->setFixedHeight(28);
        layout->addWidget(button);
        return button;
    };

    stopBtn_ = addButton(QStringLiteral("■"));
    stepBackBtn_ = addButton(QStringLiteral("◀◀"));
    playBtn_ = addButton(QStringLiteral("▶"));
    stepFwdBtn_ = addButton(QStringLiteral("▶▶"));

    connect(stopBtn_, &QPushButton::clicked, this, &TransportBarWidget::onStopClicked);
    connect(stepBackBtn_, &QPushButton::clicked, this, &TransportBarWidget::onStepBackClicked);
    connect(playBtn_, &QPushButton::clicked, this, &TransportBarWidget::onPlayClicked);
    connect(stepFwdBtn_, &QPushButton::clicked, this, &TransportBarWidget::onStepFwdClicked);

    timecode_ = new QLabel(QStringLiteral("00:00:00:00"));
    QFont timecodeFont = timecode_->font();
    timecodeFont.setBold(true);
    timecode_->setFont(timecodeFont);
    layout->addWidget(timecode_);

    speedLabel_ = new QLabel(QStringLiteral("1x"));
    speedLabel_->setMinimumWidth(40);
    speedLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(speedLabel_);

    layout->addStretch(1);

    auto* exportButton = new QPushButton(QStringLiteral("Export"));
    connect(exportButton, &QPushButton::clicked, this, &TransportBarWidget::onExportClicked);
    layout->addWidget(exportButton);

    auto* engine = ArtifactPr::EditorEngine::instance();
    connect(engine, &ArtifactPr::EditorEngine::playbackStateChanged, this, &TransportBarWidget::updatePlayState);
    connect(engine, &ArtifactPr::EditorEngine::playbackSpeedChanged, this, &TransportBarWidget::updateSpeedDisplay);
    connect(engine, &ArtifactPr::EditorEngine::currentFrameChanged, this, &TransportBarWidget::updateTimecode);

    playbackTimer_ = new QTimer(this);
    connect(playbackTimer_, &QTimer::timeout, this, &TransportBarWidget::onPlaybackTick);
}

void TransportBarWidget::onStopClicked()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->stop();
}

void TransportBarWidget::onStepBackClicked()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->stepBackward();
}

void TransportBarWidget::onPlayClicked()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->togglePlayPause();
}

void TransportBarWidget::onStepFwdClicked()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->stepForward();
}

void TransportBarWidget::onPlaybackTick()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    playbackTimer_->setInterval(qMax(1, 1000 / sequenceFrameRate(engine->currentSequence())));
    auto frame = engine->currentFrame();
    int speed = static_cast<int>(engine->playbackSpeed());

    if (speed == static_cast<int>(ArtifactPr::PlaybackSpeed::Stop) ||
        speed == static_cast<int>(ArtifactPr::PlaybackSpeed::Pause)) {
        playbackTimer_->stop();
        return;
    }

    const auto start = engine->playbackRangeStart();
    const auto end = engine->playbackRangeEnd();
    if (end <= start) { engine->stop(); return; }
    const ArtifactPr::FramePosition step = speed > 1 ? speed / 2 : speed;
    const auto lastFrame = end - 1;
    const auto next = qBound<ArtifactPr::FramePosition>(start, frame + step, lastFrame);
    engine->setCurrentFrame(next);
    if ((step > 0 && next == lastFrame) || (step < 0 && next == start))
        engine->stop();
}

void TransportBarWidget::onExportClicked()
{
    Q_EMIT requestExport();
}

void TransportBarWidget::updateTimecode(ArtifactPr::FramePosition frame)
{
    const int fps = sequenceFrameRate(ArtifactPr::EditorEngine::instance()->currentSequence());
    int totalSeconds = static_cast<int>(frame) / fps;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    int frames = static_cast<int>(frame) % fps;

    timecode_->setText(QStringLiteral("%1:%2:%3:%4")
                           .arg(hours, 2, 10, QChar('0'))
                           .arg(minutes, 2, 10, QChar('0'))
                           .arg(seconds, 2, 10, QChar('0'))
                           .arg(frames, 2, 10, QChar('0')));
}

void TransportBarWidget::updatePlayState(bool isPlaying)
{
    playBtn_->setText(isPlaying ? QStringLiteral("⏸") : QStringLiteral("▶"));

    auto* engine = ArtifactPr::EditorEngine::instance();
    if (isPlaying) {
        const auto start = engine->playbackRangeStart();
        const auto end = engine->playbackRangeEnd();
        if (end > start && (engine->currentFrame() < start || engine->currentFrame() >= end))
            engine->setCurrentFrame(static_cast<int>(engine->playbackSpeed()) < 0 ? end - 1 : start);
        const int fps = sequenceFrameRate(engine->currentSequence());
        playbackTimer_->start(qMax(1, 1000 / fps));
    } else {
        playbackTimer_->stop();
    }
}

void TransportBarWidget::updateSpeedDisplay(ArtifactPr::PlaybackSpeed speed)
{
    int speedVal = static_cast<int>(speed);
    if (speedVal == 0) {
        speedLabel_->setText(QStringLiteral("停止"));
    } else if (speedVal == 1) {
        speedLabel_->setText(QStringLiteral("一時停止"));
    } else if (speedVal > 1) {
        speedLabel_->setText(QStringLiteral("%1x").arg(speedVal / 2));
    } else {
        speedLabel_->setText(QStringLiteral("%1x R").arg(-speedVal));
    }
}

void TransportBarWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    QMenu menu(this);
    auto* markIn = menu.addAction(QStringLiteral("Set Timeline In Here"));
    auto* markOut = menu.addAction(QStringLiteral("Set Timeline Out After This Frame"));
    auto* clear = menu.addAction(QStringLiteral("Clear Playback Range"));
    auto* start = menu.addAction(QStringLiteral("Go to Range Start"));
    auto* end = menu.addAction(QStringLiteral("Go to Range End"));
    const auto* action = menu.exec(event->globalPos());
    if (action == markIn) engine->setPlaybackRange(engine->currentFrame(), engine->outPoint());
    else if (action == markOut) engine->setPlaybackRange(engine->inPoint(), engine->currentFrame() + 1);
    else if (action == clear) engine->clearPlaybackRange();
    else if (action == start) engine->seekToFrame(engine->playbackRangeStart());
    else if (action == end) engine->seekToFrame(qMax<ArtifactPr::FramePosition>(
        engine->playbackRangeStart(), engine->playbackRangeEnd() - 1));
    setToolTip(engine->isPlaybackRangeEnabled()
        ? QStringLiteral("Playback range: [%1, %2)").arg(engine->inPoint()).arg(engine->outPoint())
        : QStringLiteral("Playback range: full sequence"));
}

W_OBJECT_IMPL(TransportBarWidget)
