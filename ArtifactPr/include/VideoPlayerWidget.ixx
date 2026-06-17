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

export module ArtifactPr.VideoPlayerWidget;

import ArtifactPr.VideoSurface;

export namespace ArtifactPr {

/// 自前 paintEvent で最新 frame を描画する内部 widget。
/// QVideoWidget を置換する。QAbstractVideoSurface + QPixmap で高速描画。
class VideoCanvas : public QWidget {
public:
    explicit VideoCanvas(QWidget* parent = nullptr);

    void setFrame(const QPixmap& pix, qint64 ptsMs);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap frame_;
    qint64 ptsMs_ = 0;
};

} // namespace ArtifactPr

export class VideoPlayerWidget : public QWidget
{
public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr);
    ~VideoPlayerWidget() override;

    void loadFile(const QString& filePath);
    void play();
    void pause();
    void stop();
    void seek(qint64 positionMs);
    qint64 position() const;
    qint64 duration() const;
    bool hasVideo() const;
    QMediaPlayer* player() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QStackedWidget* stackedWidget_ = nullptr;
    QLabel* placeholderLabel_ = nullptr;
    ArtifactPr::VideoCanvas* videoCanvas_ = nullptr;
    QMediaPlayer* player_ = nullptr;
    ArtifactPr::PrVideoSurface* surface_ = nullptr;

    void onFramePresented(ArtifactPr::PrFrameSnapshot snap);
};