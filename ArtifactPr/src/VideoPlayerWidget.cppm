module;

#include <QHBoxLayout>
#include <QLabel>
#include <QMediaPlayer>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QStackedWidget>
#include <QString>
#include <QUrl>
#include <QVideoFrame>
#include <QWidget>

module ArtifactPr.VideoPlayerWidget;

import ArtifactPr.VideoSurface;
import ArtifactPr.AppTheme;

namespace ArtifactPr {

// =====================================================================
// VideoCanvas
// ---------------------------------------------------------------------
// QVideoWidget を置換する自前描画 widget。
// paintEvent は QPixmap::scaled() で widget サイズにフィットさせる。
// QImage は paintEvent 内のみで QPixmap 化 (IO 境界用途)。
// =====================================================================
VideoCanvas::VideoCanvas(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    setMinimumSize(320, 180);
}

void VideoCanvas::setFrame(const QPixmap& pix, qint64 ptsMs) {
    frame_ = pix;
    ptsMs_ = ptsMs;
    update();
}

void VideoCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (frame_.isNull()) {
        p.setPen(QColor(120, 120, 120));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("(no frame)"));
        return;
    }
    // アスペクト比を保って widget 内に収まるように scale
    QPixmap scaled = frame_.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QRect target((width() - scaled.width()) / 2,
                 (height() - scaled.height()) / 2,
                 scaled.width(), scaled.height());
    p.drawPixmap(target, scaled);
}

} // namespace ArtifactPr

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    stackedWidget_ = new QStackedWidget(this);
    layout->addWidget(stackedWidget_);

    placeholderLabel_ = new QLabel(QStringLiteral("No media loaded\n\nDrag a video file here or use Import"));
    placeholderLabel_->setAlignment(Qt::AlignCenter);
    placeholderLabel_->setProperty(ArtifactPr::kPropSurfaceKind, QString::fromUtf8(ArtifactPr::kSurfaceMediaPlaceholder));
    placeholderLabel_->setAutoFillBackground(true);
    placeholderLabel_->setMinimumSize(320, 180);
    stackedWidget_->addWidget(placeholderLabel_);

    videoCanvas_ = new ArtifactPr::VideoCanvas();
    stackedWidget_->addWidget(videoCanvas_);

    player_ = new QMediaPlayer(this);
    surface_ = new ArtifactPr::PrVideoSurface(this);
    player_->setVideoOutput(surface_);

    // frame 更新通知 -> videoCanvas_ に転送
    connect(surface_, &ArtifactPr::PrVideoSurface::framePresented,
            this, &VideoPlayerWidget::onFramePresented);

    stackedWidget_->setCurrentIndex(0);
}

VideoPlayerWidget::~VideoPlayerWidget() {
    if (player_) {
        player_->stop();
    }
}

void VideoPlayerWidget::loadFile(const QString& filePath)
{
    QUrl url = QUrl::fromLocalFile(filePath);
    player_->setSource(url);
    stackedWidget_->setCurrentIndex(1);
}

void VideoPlayerWidget::play() { player_->play(); }
void VideoPlayerWidget::pause() { player_->pause(); }
void VideoPlayerWidget::stop() { player_->stop(); }
void VideoPlayerWidget::seek(qint64 positionMs) { player_->setPosition(positionMs); }
qint64 VideoPlayerWidget::position() const { return player_->position(); }
qint64 VideoPlayerWidget::duration() const { return player_->duration(); }
bool VideoPlayerWidget::hasVideo() const { return player_->hasVideo(); }
QMediaPlayer* VideoPlayerWidget::player() const { return player_; }

void VideoPlayerWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
}

void VideoPlayerWidget::onFramePresented(ArtifactPr::PrFrameSnapshot snap) {
    if (!snap.valid || snap.image.isNull()) return;
    if (videoCanvas_) {
        videoCanvas_->setFrame(QPixmap::fromImage(snap.image), snap.ptsMs);
    }
}