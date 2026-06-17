module;

#include <QString>
#include <QWidget>

export module ArtifactPr.TimecodeOverlayWidget;

export class TimecodeOverlayWidget : public QWidget
{
public:
    explicit TimecodeOverlayWidget(QWidget* parent = nullptr);

    /// フレーム番号を更新 (UI thread から)。
    void setCurrentFrame(int frame);

    /// FPS を設定。
    void setFps(int fps) { fps_ = fps; update(); }

    /// 表示 / 非表示。
    void setVisible(bool visible);

    /// フレーム → "HH:MM:SS:FF" 文字列。
    static QString frameToTimecode(int frame, int fps);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int currentFrame_ = 0;
    int fps_ = 30;
};