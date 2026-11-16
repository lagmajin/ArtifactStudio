#include <QApplication>
#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPointF>
#include <QString>
#include <QShowEvent>
#include <QWidget>

#include <algorithm>
#include <array>

import Artifact.Render.IRenderer;

namespace {

constexpr int CanvasWidth = 1200;
constexpr int CanvasHeight = 650;

struct ClipSpec {
  const char* name;
  float x;
  float width;
  ArtifactCore::FloatColor color;
};

class TimelineClipBarWindow final : public QWidget {
public:
  TimelineClipBarWindow() {
    setWindowTitle(QStringLiteral("Timeline Clip Bar · Diligent Playground"));
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFixedSize(CanvasWidth, CanvasHeight);
    setFocusPolicy(Qt::StrongFocus);
  }

  ~TimelineClipBarWindow() override {
    if (rendererReady_) renderer_.destroy();
  }

protected:
  void showEvent(QShowEvent* event) override {
    QWidget::showEvent(event);
    if (rendererAttempted_) return;
    rendererAttempted_ = true;
    renderer_.initialize(this);
    rendererReady_ = renderer_.isInitialized();
    if (rendererReady_) {
      renderer_.setCanvasSize(static_cast<float>(CanvasWidth),
                              static_cast<float>(CanvasHeight));
      renderer_.setViewportSize(static_cast<float>(width() * devicePixelRatioF()),
                                static_cast<float>(height() * devicePixelRatioF()));
      renderer_.fitToViewport(0.0f);
    } else {
      setWindowTitle(QStringLiteral(
          "Timeline Clip Bar · Diligent initialization failed"));
    }
    update();
  }

  void paintEvent(QPaintEvent*) override {
    if (!rendererReady_) return;
    renderer_.setViewportSize(static_cast<float>(width() * devicePixelRatioF()),
                              static_cast<float>(height() * devicePixelRatioF()));
    renderer_.setClearColor(color(17, 21, 27));
    renderer_.clear();
    drawHeader();
    drawVariant(0, 92, QStringLiteral("A · Current"), 0);
    drawVariant(1, 264, QStringLiteral("B · Editorial"), 1);
    drawVariant(2, 436, QStringLiteral("C · Compact"), 2);
    renderer_.flush();
    renderer_.present();
  }

  void mousePressEvent(QMouseEvent* event) override {
    const QPointF point = event->position();
    if (point.x() < 150.0) return;
    const int variant = std::clamp(static_cast<int>((point.y() - 92.0) / 172.0), 0, 2);
    selectedVariant_ = variant;
    update();
  }

  void keyPressEvent(QKeyEvent* event) override {
    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_3) {
      selectedVariant_ = event->key() - Qt::Key_1;
      update();
      return;
    }
    if (event->key() == Qt::Key_D) {
      dense_ = !dense_;
      update();
      return;
    }
    QWidget::keyPressEvent(event);
  }

private:
  static ArtifactCore::FloatColor color(int r, int g, int b, int a = 255) {
    return ArtifactCore::FloatColor(r / 255.0f, g / 255.0f, b / 255.0f,
                                    a / 255.0f);
  }

  void text(float x, float y, float w, float h, const QString& value,
            const ArtifactCore::FloatColor& ink, int size = 12,
            bool bold = false) {
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(size);
    font.setBold(bold);
    renderer_.drawText(QRectF(x, y, w, h), value, font, ink,
                       Qt::AlignLeft | Qt::AlignVCenter);
  }

  void rect(float x, float y, float w, float h,
            const ArtifactCore::FloatColor& fill,
            const ArtifactCore::FloatColor& edge = color(0, 0, 0, 0),
            float radius = 3.0f, float edgeWidth = 0.0f) {
    renderer_.drawRoundedPanel(x, y, w, h, radius, fill, edge, 1.0f, edgeWidth);
  }

  void drawHeader() {
    text(28, 20, 650, 30, QStringLiteral("TIMELINE CLIP BAR · DILIGENT GPU"),
         color(229, 235, 242), 20, true);
    text(28, 53, 920, 22,
         QStringLiteral("Same sample timeline · click a row to compare selection · D: density · 1–3: select design"),
         color(142, 155, 170), 12);
    text(1000, 27, 170, 28, dense_ ? QStringLiteral("DENSE") : QStringLiteral("NORMAL"),
         color(112, 190, 255), 12, true);
  }

  void drawVariant(int index, int y, const QString& title, int style) {
    const auto panel = color(25, 31, 40);
    const auto border = index == selectedVariant_ ? color(91, 169, 245) : color(48, 59, 72);
    rect(22, static_cast<float>(y), 1156, 157, panel, border, 8, index == selectedVariant_ ? 2.0f : 1.0f);
    text(38, static_cast<float>(y + 7), 300, 22, title, color(225, 233, 242), 14, true);
    const int rowHeight = dense_ ? 27 : 31;
    const int rulerY = y + 32;
    text(38, static_cast<float>(rulerY), 100, 20,
         style == 0 ? QStringLiteral("REFERENCE") : QStringLiteral("DESIGN OPTION"),
         color(126, 140, 156), 10, true);
    for (int tick = 0; tick < 8; ++tick) {
      const float x = 160.0f + tick * 143.0f;
      text(x, static_cast<float>(rulerY), 70, 20,
           QStringLiteral("00:%1").arg(tick * 2, 2, 10, QLatin1Char('0')),
           color(126, 140, 156), 10);
      renderer_.drawSolidRect(x, static_cast<float>(rulerY + 19), 1.0f,
                              static_cast<float>(rowHeight * 4 + 2), color(39, 48, 59));
    }
    const std::array<ArtifactCore::FloatColor, 4> colors = {
        color(66, 124, 177), color(50, 148, 112),
        color(128, 83, 164), color(154, 105, 53)};
    const std::array<ClipSpec, 4> clips = {{
        {"City footage", 190.0f, 370.0f, colors[0]},
        {"Ambience", 190.0f, 690.0f, colors[1]},
        {"Opening title · Fade in", 300.0f, 275.0f, colors[2]},
        {"Accent shape", 595.0f, 230.0f, colors[3]}}};
    const std::array<QString, 4> tracks = {QStringLiteral("01 · City footage"),
        QStringLiteral("02 · Ambience"), QStringLiteral("03 · Opening title"),
        QStringLiteral("04 · Accent shape")};
    for (int row = 0; row < 4; ++row) {
      const float ry = static_cast<float>(rulerY + 21 + row * rowHeight);
      renderer_.drawSolidRect(30.0f, ry + rowHeight - 1.0f, 1138.0f, 1.0f,
                              color(38, 47, 58));
      text(39.0f, ry, 145.0f, static_cast<float>(rowHeight), tracks[row],
           color(181, 194, 208), 11);
      const float h = style == 2 ? static_cast<float>(rowHeight - 5) : static_cast<float>(rowHeight - 3);
      const float cy = ry + (rowHeight - h) * 0.5f;
      const auto& clip = clips[static_cast<std::size_t>(row)];
      const bool selected = row == 2 && index == selectedVariant_;
      const float radius = style == 0 ? 2.0f : style == 1 ? 7.0f : 3.0f;
      const auto fill = row == 2 && selected ? color(125, 91, 157) : clip.color;
      rect(clip.x, cy, clip.width, h, fill,
           selected ? color(240, 248, 255) : clip.color,
           radius, selected ? 2.0f : (style == 1 ? 1.5f : 0.7f));
      // Fixed-width trim affordances are part of the visual comparison only.
      const float gripWidth = style == 1 ? 4.0f : style == 2 ? 2.0f : 1.5f;
      renderer_.drawSolidRect(clip.x + 4.0f, cy + 5.0f, gripWidth, h - 10.0f,
                              color(235, 244, 252, style == 0 ? 145 : 220));
      renderer_.drawSolidRect(clip.x + clip.width - 4.0f - gripWidth, cy + 5.0f,
                              gripWidth, h - 10.0f, color(235, 244, 252, style == 0 ? 145 : 220));
      if (row == 1) {
        for (int bar = 0; bar < 70; ++bar) {
          const float bx = clip.x + 12.0f + bar * 9.0f;
          if (bx >= clip.x + clip.width - 10.0f) break;
          const float bh = 3.0f + static_cast<float>((bar * 17) % 13);
          renderer_.drawSolidRect(bx, cy + (h - bh) * 0.5f, 2.0f, bh,
                                  color(214, 250, 229, 155));
        }
      } else {
        text(clip.x + 11.0f, cy, clip.width - 22.0f, h,
             QString::fromLatin1(clip.name), color(246, 249, 252), 11, true);
      }
    }
    const float playheadX = 744.0f;
    renderer_.drawSolidRect(playheadX, static_cast<float>(rulerY), 1.5f,
                            static_cast<float>(rowHeight * 4 + 22), color(255, 91, 109));
  }

  Artifact::ArtifactIRenderer renderer_;
  int selectedVariant_ = 1;
  bool dense_ = false;
  bool rendererAttempted_ = false;
  bool rendererReady_ = false;
};

} // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  TimelineClipBarWindow window;
  window.show();
  return app.exec();
}
