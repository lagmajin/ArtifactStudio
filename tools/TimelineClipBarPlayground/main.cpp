#include <QDir>
#include <QByteArray>
#include <QImage>
#include <QString>

#include <array>
#include <cmath>
#include <cstdio>

import Artifact.Render.IRenderer;
import Color.Float;

namespace {

constexpr int Width = 1200;
constexpr int Height = 650;

ArtifactCore::FloatColor color(int r, int g, int b, int a = 255) {
  const auto linear = [](int channel) {
    const float srgb = static_cast<float>(channel) / 255.0f;
    return srgb <= 0.04045f ? srgb / 12.92f
                            : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
  };
  return ArtifactCore::FloatColor(linear(r), linear(g), linear(b),
                                  a / 255.0f);
}

std::array<unsigned char, 7> glyph(char ch) {
  switch (ch) {
  case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
  case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
  case 'E': return {31,16,16,30,16,16,31}; case 'G': return {14,17,16,23,17,17,15};
  case 'I': return {31,4,4,4,4,4,31}; case 'L': return {16,16,16,16,16,16,31};
  case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
  case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
  case 'R': return {30,17,17,30,20,18,17}; case 'S': return {15,16,16,14,1,1,30};
  case 'T': return {31,4,4,4,4,4,4}; case 'U': return {17,17,17,17,17,17,14};
  case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
  case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
  case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
  case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
  case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
  case ':': return {0,4,4,0,4,4,0}; case '-': return {0,0,0,31,0,0,0};
  case '.': return {0,0,0,0,0,12,12}; default: return {0,0,0,0,0,0,0};
  }
}

void label(Artifact::ArtifactIRenderer& renderer, float x, float y,
           const char* value, const ArtifactCore::FloatColor& ink,
           float pixel = 1.5f) {
  float cursor = x;
  while (*value != '\0') {
    const char ch = *value++;
    if (ch == ' ') { cursor += pixel * 4.0f; continue; }
    const auto rows = glyph(ch);
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if ((rows[static_cast<std::size_t>(row)] & (1u << (4 - col))) != 0) {
          renderer.drawSolidRect(cursor + col * pixel, y + row * pixel,
                                 pixel, pixel, ink);
        }
      }
    }
    cursor += pixel * 6.0f;
  }
}

void rounded(Artifact::ArtifactIRenderer& renderer, float x, float y, float w,
             float h, float radius, const ArtifactCore::FloatColor& fill,
             const ArtifactCore::FloatColor& edge, float edgeWidth) {
  renderer.drawRoundedPanel(x, y, w, h, radius, fill, edge, 1.0f, edgeWidth);
}

void drawVariant(Artifact::ArtifactIRenderer& renderer, int index, int y,
                 const QString& title, int style, bool dense) {
  const auto panel = color(25, 31, 40);
  const auto border = index == 1 ? color(91, 169, 245) : color(48, 59, 72);
  rounded(renderer, 22, static_cast<float>(y), 1156, 157, 8, panel, border,
          index == 1 ? 2.0f : 1.0f);
  const QByteArray titleAscii = title.toLatin1().toUpper();
  label(renderer, 38, static_cast<float>(y + 9), titleAscii.constData(),
        color(225, 233, 242), 1.6f);
  const int rowHeight = dense ? 27 : 31;
  const int rulerY = y + 32;
  label(renderer, 38, static_cast<float>(rulerY + 6),
        style == 0 ? "REFERENCE" : "OPTION", color(126, 140, 156), 1.0f);
  for (int tick = 0; tick < 8; ++tick) {
    const float x = 160.0f + tick * 143.0f;
    const QByteArray time = QStringLiteral("00:%1").arg(tick * 2, 2, 10,
        QLatin1Char('0')).toLatin1();
    label(renderer, x, static_cast<float>(rulerY + 6), time.constData(),
          color(126, 140, 156), 1.0f);
    renderer.drawSolidRect(x, static_cast<float>(rulerY + 19), 1.0f,
                           static_cast<float>(rowHeight * 4 + 2), color(39, 48, 59));
  }
  const std::array<ArtifactCore::FloatColor, 4> colors = {
      color(66, 124, 177), color(50, 148, 112),
      color(128, 83, 164), color(154, 105, 53)};
  struct Clip { const char* label; float x; float width; };
  const std::array<Clip, 4> clips = {{
      {"City footage", 190.0f, 370.0f},
      {"Ambience", 190.0f, 690.0f},
      {"Opening title · Fade in", 300.0f, 275.0f},
      {"Accent shape", 595.0f, 230.0f}}};
  const std::array<const char*, 4> tracks = {"01 CITY", "02 AUDIO", "03 TITLE", "04 SHAPE"};
  for (int row = 0; row < 4; ++row) {
    const float ry = static_cast<float>(rulerY + 21 + row * rowHeight);
    renderer.drawSolidRect(30.0f, ry + rowHeight - 1.0f, 1138.0f, 1.0f,
                           color(38, 47, 58));
    label(renderer, 39.0f, ry + (rowHeight - 10.5f) * 0.5f, tracks[row],
          color(181, 194, 208), 1.5f);
    const float h = style == 2 ? static_cast<float>(rowHeight - 5)
                               : static_cast<float>(rowHeight - 3);
    const float cy = ry + (rowHeight - h) * 0.5f;
    const auto& clip = clips[static_cast<std::size_t>(row)];
    const bool selected = row == 2 && index == 1;
    const float radius = style == 0 ? 2.0f : style == 1 ? 7.0f : 3.0f;
    const auto fill = selected ? color(125, 91, 157) : colors[static_cast<std::size_t>(row)];
    rounded(renderer, clip.x, cy, clip.width, h, radius, fill,
            selected ? color(240, 248, 255) : colors[static_cast<std::size_t>(row)],
            selected ? 2.0f : (style == 1 ? 1.5f : 0.7f));
    // Fixed-width trim affordances are visual samples, not interactive handles.
    const float gripWidth = style == 1 ? 4.0f : style == 2 ? 2.0f : 1.5f;
    renderer.drawSolidRect(clip.x + 4.0f, cy + 5.0f, gripWidth, h - 10.0f,
                           color(235, 244, 252, style == 0 ? 145 : 220));
    renderer.drawSolidRect(clip.x + clip.width - 4.0f - gripWidth,
                           cy + 5.0f, gripWidth, h - 10.0f,
                           color(235, 244, 252, style == 0 ? 145 : 220));
    if (row == 1) {
      for (int bar = 0; bar < 70; ++bar) {
        const float bx = clip.x + 12.0f + bar * 9.0f;
        if (bx >= clip.x + clip.width - 10.0f) break;
        const float bh = 3.0f + static_cast<float>((bar * 17) % 13);
        renderer.drawSolidRect(bx, cy + (h - bh) * 0.5f, 2.0f, bh,
                               color(214, 250, 229, 155));
      }
    } else {
      label(renderer, clip.x + 11.0f, cy + (h - 10.5f) * 0.5f,
            clip.label, color(246, 249, 252), 1.5f);
    }
  }
  const float playheadX = 744.0f;
  renderer.drawSolidRect(playheadX, static_cast<float>(rulerY), 1.5f,
                         static_cast<float>(rowHeight * 4 + 22), color(255, 91, 109));
}

int capture(const QString& outputPath) {
  qInfo("Initializing the Diligent headless renderer");
  Artifact::ArtifactIRenderer renderer;
  renderer.initializeHeadless(Width, Height);
  qInfo() << "Diligent renderer initialized:" << renderer.isInitialized();
  if (!renderer.isInitialized()) {
    qCritical("Could not initialize the Diligent headless renderer");
    return 2;
  }
  renderer.setCanvasSize(static_cast<float>(Width), static_cast<float>(Height));
  renderer.setViewportSize(static_cast<float>(Width), static_cast<float>(Height));
  renderer.fitToViewport(0.0f);
  renderer.setClearColor(color(17, 21, 27));
  renderer.clear();
  label(renderer, 28, 24, "TIMELINE CLIP BAR DILIGENT GPU", color(229, 235, 242), 2.2f);
  label(renderer, 28, 48, "DILIGENT GPU CAPTURE - DENSE ROWS", color(142, 155, 170), 1.4f);
  label(renderer, 1000, 35, "DENSE", color(112, 190, 255), 1.5f);
  qInfo("Submitting clip bar draw calls");
  drawVariant(renderer, 0, 92, QStringLiteral("A · Current"), 0, true);
  drawVariant(renderer, 1, 264, QStringLiteral("B · Editorial"), 1, true);
  drawVariant(renderer, 2, 436, QStringLiteral("C · Compact"), 2, true);
  qInfo("Waiting for Diligent frame completion");
  renderer.flushAndWait();
  qInfo("Reading back the Diligent render target");
  const QImage image = renderer.readbackToImage();
  const bool saved = !image.isNull() && image.size() == QSize(Width, Height) &&
                     image.save(outputPath, "PNG");
  renderer.destroy();
  if (!saved) {
    qCritical("Diligent frame readback or PNG save failed");
    return 3;
  }
  qInfo().noquote() << QStringLiteral("Diligent capture saved: %1 (%2x%3)")
                           .arg(QDir::toNativeSeparators(outputPath))
                           .arg(image.width()).arg(image.height());
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--capture")) {
    return capture(QString::fromLocal8Bit(argv[2]));
  }
  qCritical("Usage: ArtifactTimelineClipBarPlayground --capture <output.png>");
  return 1;
}
