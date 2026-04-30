module;
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaPlayer>
#include <QStackedWidget>
#include <QString>
#include <QUrl>
#include <QVideoWidget>

module ArtifactPr.VideoPlayerWidget;

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    stackedWidget_ = new QStackedWidget(this);
    layout->addWidget(stackedWidget_);

    placeholderLabel_ = new QLabel(QStringLiteral("No media loaded\n\nDrag a video file here or use Import"));
    placeholderLabel_->setAlignment(Qt::AlignCenter);
    placeholderLabel_->setStyleSheet(QStringLiteral("background-color: #1a1a1a; color: #555;"));
    placeholderLabel_->setMinimumSize(320, 180);
    stackedWidget_->addWidget(placeholderLabel_);

    videoWidget_ = new QVideoWidget();
    stackedWidget_->addWidget(videoWidget_);

    player_ = new QMediaPlayer(this);
    player_->setVideoOutput(videoWidget_);

    stackedWidget_->setCurrentIndex(0);
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
