#pragma once

#include <QWidget>
#include <QString>
#include <QLabel>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QUrl>

class VideoPlayerWidget : public QWidget
{
public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr)
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

    ~VideoPlayerWidget() = default;

    void loadFile(const QString& filePath)
    {
        QUrl url = QUrl::fromLocalFile(filePath);
        player_->setSource(url);
        stackedWidget_->setCurrentIndex(1);
    }

    void play()
    {
        player_->play();
    }

    void pause()
    {
        player_->pause();
    }

    void stop()
    {
        player_->stop();
    }

    void seek(qint64 positionMs)
    {
        player_->setPosition(positionMs);
    }

    qint64 position() const
    {
        return player_->position();
    }

    qint64 duration() const
    {
        return player_->duration();
    }

    bool hasVideo() const
    {
        return player_->hasVideo();
    }

    QMediaPlayer* player() const { return player_; }

private:
    QStackedWidget* stackedWidget_ = nullptr;
    QLabel* placeholderLabel_ = nullptr;
    QVideoWidget* videoWidget_ = nullptr;
    QMediaPlayer* player_ = nullptr;
};
