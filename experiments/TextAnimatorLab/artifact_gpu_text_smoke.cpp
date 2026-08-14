#include <QGuiApplication>
#include <QColor>
#include <QImage>
#include <QFile>
#include <QString>
#include <cstdio>
#include <vector>

import Artifact.Render.IRenderer;
import Text.Animator;
import Text.GlyphLayout;
import Text.Style;
import Utils.String.UniString;

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QString text = argc > 1 ? QString::fromLocal8Bit(argv[1])
                          : QStringLiteral("Text Sample1");
  if (text.startsWith(QStringLiteral("@"))) {
    QFile input(text.mid(1));
    if (!input.open(QIODevice::ReadOnly)) {
      std::fprintf(stderr, "gpu-smoke: failed to open UTF-8 text file: %s\n",
                   text.mid(1).toLocal8Bit().constData());
      return 2;
    }
    text = QString::fromUtf8(input.readAll());
    while (text.endsWith(QChar('\n')) || text.endsWith(QChar('\r'))) {
      text.chop(1);
    }
  }
  const QString output = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                 : QStringLiteral("artifact_gpu_text.png");

  ArtifactCore::TextStyle style;
  style.fontSize = 64.0f;
  style.pixelSize = 64.0f;
  ArtifactCore::ParagraphStyle paragraph;
  auto glyphs = ArtifactCore::TextLayoutEngine::layout(
      ArtifactCore::UniString(text), style, paragraph);

  ArtifactCore::RangeSelector selector;
  selector.start = 0.0f;
  selector.end = 100.0f;
  selector.shape = ArtifactCore::SelectorShape::RampUp;
  selector.order = ArtifactCore::SelectorOrder::Natural;
  ArtifactCore::WigglySelector wiggly;
  ArtifactCore::AnimatorProperties properties;
  properties.rotation = 90.0f;
  properties.opacity = 0.5f;
  ArtifactCore::TextAnimatorEngine::applyAnimator(
      glyphs, selector, wiggly, properties, 0.0f);

  Artifact::ArtifactIRenderer renderer;
  renderer.initializeHeadless(1400, 240);
  renderer.clear();
  renderer.drawGlyphs(glyphs, style, ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
  renderer.flushAndWait();
  const QImage image = renderer.readbackToImage();
  int nonZeroAlpha = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixelColor(x, y).alpha() > 0) ++nonZeroAlpha;
    }
  }
  const bool saved = !image.isNull() && nonZeroAlpha > 0 && image.save(output);
  std::fprintf(stderr, "gpu-smoke: glyphs=%zu image=%dx%d nonzeroAlpha=%d saved=%d path=%s\n",
               glyphs.size(), image.width(), image.height(), nonZeroAlpha, saved ? 1 : 0,
               output.toLocal8Bit().constData());
  renderer.destroy();
  return saved ? 0 : 1;
}
