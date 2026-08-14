// ArtifactCore Text Animator runtime smoke test.
// This intentionally verifies the Core glyph pipeline before any renderer.
import Text.Animator;
import Text.GlyphAtlas;
import Text.GlyphLayout;
import Text.Style;
import Font.FreeFont;
import Utils.String.UniString;

#include <QGuiApplication>
#include <QFont>
#include <QImage>
#include <QFontDatabase>
#include <QRawFont>
#include <QPainterPath>
#include <QTextLayout>
#include <QGlyphRun>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <iostream>
#include <cstdio>
#include <tuple>
#include <vector>

int main(int argc, char **argv) {
  std::fprintf(stderr, "smoke: entered-main\n");
  std::fflush(stderr);
  QGuiApplication app(argc, argv);
  std::fprintf(stderr, "smoke: app-ready\n");
  const QString text = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                : QStringLiteral("Text Sample1");

  ArtifactCore::TextStyle style;
  const QString explicitFontPath = QStringLiteral("C:/Windows/Fonts/segoeui.ttf");
  const int fontId = QFontDatabase::addApplicationFont(explicitFontPath);
  QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/seguiemj.ttf"));
  const QStringList loadedFamilies =
      fontId >= 0 ? QFontDatabase::applicationFontFamilies(fontId) : QStringList{};
  style.fontFamily = ArtifactCore::UniString(
      loadedFamilies.isEmpty() ? ArtifactCore::FontManager::defaultSansSerifFamily()
                               : loadedFamilies.front());
  style.fontSize = 64.0f;
  style.pixelSize = 64.0f;
  {
    QTextLayout diagnosticLayout(text, ArtifactCore::FontManager::makeFont(style, text));
    diagnosticLayout.beginLayout();
    const QTextLine diagnosticLine = diagnosticLayout.createLine();
    diagnosticLayout.endLayout();
    if (diagnosticLine.isValid()) {
      const auto runs = diagnosticLine.glyphRuns(
          -1, -1, QTextLayout::RetrieveGlyphIndexes |
                        QTextLayout::RetrieveGlyphPositions |
                        QTextLayout::RetrieveStringIndexes);
      std::fprintf(stderr, "smoke: qt-glyph-runs=%d\n", runs.size());
      for (int runIndex = 0; runIndex < runs.size(); ++runIndex) {
        const auto& run = runs.at(runIndex);
        const auto indexes = run.glyphIndexes();
        const auto positions = run.positions();
        const auto stringIndexes = run.stringIndexes();
        std::fprintf(stderr, "smoke: qt-run=%d glyphs=%d source=%s\n",
                     runIndex, indexes.size(),
                     run.sourceString().toUtf8().constData());
        for (int glyphIndex = 0; glyphIndex < indexes.size(); ++glyphIndex) {
          const auto pos = glyphIndex < positions.size()
                               ? positions.at(glyphIndex)
                               : QPointF{};
          const auto sourceIndex = glyphIndex < stringIndexes.size()
                                       ? stringIndexes.at(glyphIndex)
                                       : -1;
          std::fprintf(stderr, "smoke: qt-glyph=%d index=%u source=%lld pos=%.2f,%.2f\n",
                       glyphIndex, static_cast<unsigned>(indexes.at(glyphIndex)),
                       static_cast<long long>(sourceIndex), pos.x(), pos.y());
        }
      }
    }
  }
  ArtifactCore::ParagraphStyle paragraph;
  std::fprintf(stderr, "smoke: before-layout\n");
  std::vector<ArtifactCore::GlyphItem> glyphs =
      ArtifactCore::TextLayoutEngine::layout(
      ArtifactCore::UniString(text), style, paragraph);
  std::fprintf(stderr, "smoke: after-layout glyphs=%zu\n", glyphs.size());
  const QFont emojiDiagnosticFont = ArtifactCore::FontManager::makeFont(style, text);
  const QRawFont diagnosticRaw = QRawFont::fromFont(emojiDiagnosticFont, QFontDatabase::Any);
  for (const char32_t code : text.toUcs4()) {
    if (code < 0x1F000 || code > 0x1FAFF) continue;
    const QString sample = QString::fromUcs4(&code, 1);
    const auto indices = diagnosticRaw.glyphIndexesForString(sample);
    const quint32 index = indices.isEmpty() ? 0u : indices.front();
    const QImage alpha = index == 0u
                             ? QImage{}
                             : diagnosticRaw.alphaMapForGlyph(index, QRawFont::PixelAntialiasing);
    const QPainterPath outline = index == 0u ? QPainterPath{} : diagnosticRaw.pathForGlyph(index);
    if (!alpha.isNull()) {
      alpha.save(QStringLiteral("artifactcore_emoji_alpha.png"));
    }
    std::fprintf(stderr,
                 "smoke: emoji U+%04X font=%s glyph=%u alpha=%dx%d pathEmpty=%d\n",
                 static_cast<unsigned>(code), emojiDiagnosticFont.family().toLocal8Bit().constData(),
                 static_cast<unsigned>(index), alpha.width(), alpha.height(), outline.isEmpty() ? 1 : 0);
  }

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
  std::fprintf(stderr, "smoke: after-animator\n");

  // Exercise the Core glyph rasterization boundary as well.  The atlas is the
  // existing CPU-to-GPU upload source used by the renderer; saving it here is
  // only a diagnostic artifact, not a new drawing path.
  ArtifactCore::GlyphAtlas atlas;
  int rasterizedGlyphs = 0;
  int colorGlyphs = 0;
  int colorPreservedGlyphs = 0;
  for (const auto &glyph : glyphs) {
    const QString glyphText = QString::fromUcs4(&glyph.charCode, 1);
    const QFont font = ArtifactCore::FontManager::makeFont(style, glyphText);
    ArtifactCore::GlyphKey key;
    key.codePoint = glyph.charCode;
    key.fontSize = style.fontSize;
    key.fontFamily = font.family().toStdString();
    key.renderMode = glyph.renderMode;
    const ArtifactCore::GlyphRect rect = atlas.acquire(key, font);
    if (rect.valid) ++rasterizedGlyphs;
    if (glyph.renderMode == ArtifactCore::GlyphRenderMode::ColorBitmap) {
      ++colorGlyphs;
      if (rect.colorPreserved) ++colorPreservedGlyphs;
    }
  }
  const QString atlasPath = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                     : QStringLiteral("artifactcore_text_atlas.png");
  const bool atlasSaved = atlas.atlasImage().save(atlasPath);
  bool rawSaved = false;
  const QFont diagnosticFont = ArtifactCore::FontManager::makeFont(
      style, QStringLiteral("T"));
  const QRawFont rawFont = QRawFont::fromFont(diagnosticFont, QFontDatabase::Any);
  const QVector<quint32> rawIndices = rawFont.glyphIndexesForString(QStringLiteral("T"));
  if (!rawIndices.isEmpty() && rawIndices.front() != 0) {
    rawSaved = rawFont.alphaMapForGlyph(rawIndices.front(), QRawFont::PixelAntialiasing)
                   .save(QStringLiteral("artifactcore_raw_T.png"));
  }
  std::fprintf(stderr, "smoke: atlas glyphs=%d saved=%d path=%s\n",
               rasterizedGlyphs, atlasSaved ? 1 : 0,
               atlasPath.toLocal8Bit().constData());

  QJsonArray states;
  for (const auto &glyph : glyphs) {
    QJsonObject state;
    state.insert(QStringLiteral("index"), glyph.index);
    state.insert(QStringLiteral("cluster"), glyph.clusterId);
    state.insert(QStringLiteral("clusterIndex"), glyph.clusterIndex);
    state.insert(QStringLiteral("isEmojiSequence"), glyph.isEmojiSequence);
    state.insert(QStringLiteral("renderMode"),
                 glyph.renderMode == ArtifactCore::GlyphRenderMode::ColorBitmap
                     ? QStringLiteral("ColorBitmap")
                     : glyph.renderMode == ArtifactCore::GlyphRenderMode::UnsupportedSequence
                         ? QStringLiteral("UnsupportedSequence")
                         : QStringLiteral("MonochromeCoverage"));
    state.insert(QStringLiteral("rotation"), glyph.offsetRotation);
    state.insert(QStringLiteral("opacity"), glyph.offsetOpacity);
    state.insert(QStringLiteral("x"), glyph.basePosition.x() + glyph.offsetPosition.x());
    state.insert(QStringLiteral("y"), glyph.basePosition.y() + glyph.offsetPosition.y());
    states.append(state);
  }

  QJsonObject report;
  report.insert(QStringLiteral("model"), QStringLiteral("ArtifactCore-runtime"));
  report.insert(QStringLiteral("text"), text);
  report.insert(QStringLiteral("glyphCount"), static_cast<int>(glyphs.size()));
  report.insert(QStringLiteral("rasterizedGlyphCount"), rasterizedGlyphs);
  report.insert(QStringLiteral("colorGlyphCount"), colorGlyphs);
  report.insert(QStringLiteral("colorPreservedGlyphCount"), colorPreservedGlyphs);
  report.insert(QStringLiteral("atlasSaved"), atlasSaved);
  report.insert(QStringLiteral("atlasPath"), atlasPath);
  report.insert(QStringLiteral("rawGlyphSaved"), rawSaved);
  report.insert(QStringLiteral("states"), states);
  std::cout << QJsonDocument(report).toJson(QJsonDocument::Indented).toStdString();
  return 0;
}
