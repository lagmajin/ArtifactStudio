module;
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaPlayer>
#include <QStackedWidget>
#include <QString>
#include <QUrl>
#include <QVideoWidget>
#include <QWidget>

export module ArtifactPr.VideoPlayerWidget;

export class VideoPlayerWidget : public QWidget
{
public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr);
    ~VideoPlayerWidget() = default;

    void loadFile(const QString& filePath);
    void play();
    void pause();
    void stop();
    void seek(qint64 positionMs);
    qint64 position() const;
    qint64 duration() const;
    bool hasVideo() const;
    QMediaPlayer* player() const;

private:
    QStackedWidget* stackedWidget_ = nullptr;
    QLabel* placeholderLabel_ = nullptr;
    QVideoWidget* videoWidget_ = nullptr;
    QMediaPlayer* player_ = nullptr;
};
