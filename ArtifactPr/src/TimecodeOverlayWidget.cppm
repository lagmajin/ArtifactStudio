module;

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QString>
#include <QWidget>

module ArtifactPr.TimecodeOverlayWidget;

import ArtifactPr.TimecodeOverlayWidget;

TimecodeOverlayWidget::TimecodeOverlayWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(180, 36);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
}

void TimecodeOverlayWidget::setCurrentFrame(int frame) {
    if (currentFrame_ == frame) return;
    currentFrame_ = frame;
    update();
}

void TimecodeOverlayWidget::setVisible(bool visible) {
    QWidget::setVisible(visible);
}

QString TimecodeOverlayWidget::frameToTimecode(int frame, int fps) {
    if (fps <= 0) fps = 30;
    const int totalSeconds = frame / fps;
    const int h = totalSeconds / 3600;
    const int m = (totalSeconds % 3600) / 60;
    const int s = totalSeconds % 60;
    const int f = frame % fps;
    return QStringLiteral("%1:%2:%3:%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(f, 2, 10, QChar('0'));
}

void TimecodeOverlayWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect r = rect().adjusted(2, 2, -2, -2);

    // 半透明背景
    p.setBrush(QColor(0, 0, 0, 180));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(r, 4, 4);

    // text
    QFont font = p.font();
    font.setBold(true);
    font.setPointSize(14);
    font.setFamily(QStringLiteral("Consolas"));
    p.setFont(font);
    p.setPen(QColor(255, 255, 100));   // AE-like yellow
    p.drawText(r, Qt::AlignCenter, frameToTimecode(currentFrame_, fps_));
}