// ArtifactCore Text Animator runtime smoke test.
// This intentionally verifies the Core glyph pipeline before any renderer.
import Text.Animator;
import Text.GlyphAtlas;
import Text.GlyphLayout;
import Text.Style;
import Text.ShapingBackend;
import Font.FreeFont;
import Utils.String.UniString;

#include <QGuiApplication>
#include <QFile>
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
#include <cmath>
#include <tuple>
#include <vector>

namespace {

// Describes one backend's result in a form both backends can be compared on.
struct BackendDigest {
  bool produced = false;
  int glyphCount = 0;
  double totalAdvance = 0.0;
  double minBaselineY = 0.0;
  double maxBaselineY = 0.0;
  int firstShapedGlyph = -1;
  int lastShapedGlyph = -1;
  int zeroShapedGlyphs = 0;
  int bidiRunCount = 0;
  QString resolvedBase;
  QStringList scriptTags;
  std::vector<int> clusterIndexes;
  std::vector<uint32_t> shapedGlyphIndexes;
};

ArtifactCore::TextShapingRequest makeRequest(const QString &text,
                                             const ArtifactCore::TextStyle &style,
                                             const ArtifactCore::ParagraphStyle &paragraph) {
  ArtifactCore::TextShapingRequest request;
  request.text = text;
  request.style = style;
  request.paragraph = paragraph;
  request.writingMode = ArtifactCore::TextWritingMode::Horizontal;
  request.baseDirection = ArtifactCore::TextDirection::Auto;
  return request;
}

BackendDigest digest(const ArtifactCore::TextShapingResult &result) {
  BackendDigest digest;
  digest.produced = true;
  digest.glyphCount = static_cast<int>(result.glyphs.size());
  digest.clusterIndexes.reserve(result.glyphs.size());
  digest.shapedGlyphIndexes.reserve(result.glyphs.size());
  bool baselineSeen = false;
  for (const auto &glyph : result.glyphs) {
    digest.totalAdvance += glyph.basePosition.x();
    const double y = glyph.basePosition.y();
    if (!baselineSeen) {
      digest.minBaselineY = y;
      digest.maxBaselineY = y;
      baselineSeen = true;
    } else {
      digest.minBaselineY = std::min(digest.minBaselineY, y);
      digest.maxBaselineY = std::max(digest.maxBaselineY, y);
    }
    digest.clusterIndexes.push_back(glyph.clusterIndex);
    digest.shapedGlyphIndexes.push_back(glyph.shapedGlyphIndex);
    if (glyph.shapedGlyphIndex == 0) ++digest.zeroShapedGlyphs;
  }
  if (!digest.shapedGlyphIndexes.empty()) {
    for (const uint32_t value : digest.shapedGlyphIndexes) {
      if (value == 0) continue;
      if (digest.firstShapedGlyph < 0) {
        digest.firstShapedGlyph = static_cast<int>(value);
      }
      digest.lastShapedGlyph = static_cast<int>(value);
    }
  }
  digest.bidiRunCount = static_cast<int>(result.contract.bidiRuns.size());
  digest.resolvedBase = result.contract.baseDirection ==
                                ArtifactCore::TextDirection::RightToLeft
                            ? QStringLiteral("rtl")
                            : QStringLiteral("ltr");
  for (const auto &run : result.contract.scriptRuns) {
    digest.scriptTags.append(run.scriptTag);
  }
  return digest;
}

QJsonObject digestToJson(const BackendDigest &digest) {
  QJsonObject json;
  json.insert(QStringLiteral("produced"), digest.produced);
  json.insert(QStringLiteral("glyphCount"), digest.glyphCount);
  json.insert(QStringLiteral("totalAdvance"), digest.totalAdvance);
  json.insert(QStringLiteral("minBaselineY"), digest.minBaselineY);
  json.insert(QStringLiteral("maxBaselineY"), digest.maxBaselineY);
  json.insert(QStringLiteral("zeroShapedGlyphs"), digest.zeroShapedGlyphs);
  json.insert(QStringLiteral("firstShapedGlyph"), digest.firstShapedGlyph);
  json.insert(QStringLiteral("lastShapedGlyph"), digest.lastShapedGlyph);
  json.insert(QStringLiteral("bidiRunCount"), digest.bidiRunCount);
  json.insert(QStringLiteral("resolvedBase"), digest.resolvedBase);
  json.insert(QStringLiteral("scriptTags"),
              QJsonArray::fromStringList(digest.scriptTags));
  QJsonArray clusters;
  for (const int cluster : digest.clusterIndexes) clusters.append(cluster);
  json.insert(QStringLiteral("clusterIndexes"), clusters);
  QJsonArray shaped;
  for (const uint32_t value : digest.shapedGlyphIndexes) shaped.append(static_cast<qint64>(value));
  json.insert(QStringLiteral("shapedGlyphIndexes"), shaped);
  return json;
}

} // namespace

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

  // Dual-run comparison: run the same request through both shaping backends and
  // record comparable digests.  The HarfBuzz route is still opt-in at the call
  // sites, so nothing below changes the render path; this only reports what each
  // backend would produce for the same input.
  {
    struct ComparisonCase {
      const char *label;
      QString text;
    };
    const QVector<ComparisonCase> cases{
        {"latin", QStringLiteral("Text Sample1")},
        {"cjk", QString::fromUtf8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E")},
        {"hiragana", QString::fromUtf8("\xE3\x81\xB2\xE3\x82\x8A")},
        {"katakana", QString::fromUtf8("\xE3\x82\xAB\xE3\x82\xBF")},
        {"arabic", QString::fromUtf8("\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7")},
        {"hebrew", QString::fromUtf8("\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D")},
        {"devanagari", QString::fromUtf8("\xE0\xA4\xA8\xE0\xA4\xAE")},
        {"emoji-zwj", QString::fromUtf8("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD")},
    };

    ArtifactCore::QtShapingBackend qtBackend;
    ArtifactCore::HarfBuzzShapingBackend hbBackend;
    QJsonArray comparison;
    for (const auto &item : cases) {
      ArtifactCore::ParagraphStyle singleLine;   // boxWidth == 0: no wrapping
      const auto request = makeRequest(item.text, style, singleLine);
      const BackendDigest qtDigest = digest(qtBackend.shape(request));
      const BackendDigest hbDigest = digest(hbBackend.shape(request));

      QJsonObject entry;
      entry.insert(QStringLiteral("label"), QString::fromLatin1(item.label));
      entry.insert(QStringLiteral("text"), item.text);
      entry.insert(QStringLiteral("fontFamily"), style.fontFamily.toQString());
      entry.insert(QStringLiteral("qt"), digestToJson(qtDigest));
      entry.insert(QStringLiteral("harfbuzz"), digestToJson(hbDigest));
      entry.insert(QStringLiteral("glyphCountMatch"),
                   qtDigest.glyphCount == hbDigest.glyphCount);
      const bool harfBuzzIsFallback =
          hbDigest.produced && qtDigest.glyphCount == hbDigest.glyphCount &&
          hbDigest.firstShapedGlyph == qtDigest.firstShapedGlyph &&
          hbDigest.totalAdvance == qtDigest.totalAdvance;
      entry.insert(QStringLiteral("harfbuzzLooksLikeQtFallback"), harfBuzzIsFallback);
      comparison.append(entry);

      std::fprintf(stderr,
                   "dual-run: %-11s qt(glyphs=%d adv=%.2f runs=%d base=%s "
                   "scripts=%s) hb(glyphs=%d adv=%.2f runs=%d base=%s "
                   "hbFirst=%d hbZero=%d sameAsQt=%d)\n",
                   item.label, qtDigest.glyphCount, qtDigest.totalAdvance,
                   qtDigest.bidiRunCount, qtDigest.resolvedBase.toUtf8().constData(),
                   qtDigest.scriptTags.join(QLatin1Char(',')).toUtf8().constData(),
                   hbDigest.glyphCount, hbDigest.totalAdvance,
                   hbDigest.bidiRunCount, hbDigest.resolvedBase.toUtf8().constData(),
                   hbDigest.firstShapedGlyph, hbDigest.zeroShapedGlyphs,
                   harfBuzzIsFallback ? 1 : 0);
    }

    // Multi-line / wrapping cases must be handed to Qt: HarfBuzz does not break
    // lines, so the two backends are expected to differ there.
    {
      ArtifactCore::ParagraphStyle wrapped;
      wrapped.boxWidth = 200.0f;
      const QString wrappingText = QStringLiteral("Wrapping text sample");
      const auto request = makeRequest(wrappingText, style, wrapped);
      const BackendDigest qtDigest = digest(qtBackend.shape(request));
      const BackendDigest hbDigest = digest(hbBackend.shape(request));
      QJsonObject entry;
      entry.insert(QStringLiteral("label"), QStringLiteral("wrapped-boxwidth"));
      entry.insert(QStringLiteral("text"), wrappingText);
      entry.insert(QStringLiteral("qt"), digestToJson(qtDigest));
      entry.insert(QStringLiteral("harfbuzz"), digestToJson(hbDigest));
      entry.insert(QStringLiteral("expectQtFallback"), true);
      comparison.append(entry);
      std::fprintf(stderr,
                   "dual-run: %-10s qt(glyphs=%d adv=%.2f) hb(glyphs=%d adv=%.2f "
                   "expectQtFallback=1\n",
                   "wrapped", qtDigest.glyphCount, qtDigest.totalAdvance,
                   hbDigest.glyphCount, hbDigest.totalAdvance);
    }

    // Mixed-direction line: UAX #9 must split this into several bidi runs and
    // produce a non-identity visual mapping.
    {
      const QString mixed =
          QStringLiteral("abc ") + QString::fromUtf8("\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D") +
          QStringLiteral(" def");
      ArtifactCore::ParagraphStyle singleLine;
      const auto request = makeRequest(mixed, style, singleLine);
      const BackendDigest qtDigest = digest(qtBackend.shape(request));
      const BackendDigest hbDigest = digest(hbBackend.shape(request));
      QJsonObject entry;
      entry.insert(QStringLiteral("label"), QStringLiteral("mixed-bidi"));
      entry.insert(QStringLiteral("text"), mixed);
      entry.insert(QStringLiteral("qt"), digestToJson(qtDigest));
      entry.insert(QStringLiteral("harfbuzz"), digestToJson(hbDigest));
      entry.insert(QStringLiteral("expectMultipleBidiRuns"), true);
      comparison.append(entry);
      std::fprintf(stderr,
                   "dual-run: %-10s qt(runs=%d base=%s) hb(runs=%d base=%s)\n",
                   "mixed-bidi", qtDigest.bidiRunCount,
                   qtDigest.resolvedBase.toUtf8().constData(),
                   hbDigest.bidiRunCount,
                   hbDigest.resolvedBase.toUtf8().constData());
    }

    const QString comparisonPath =
        argc > 2 ? QString::fromLocal8Bit(argv[2])
                 : QStringLiteral("artifactcore_shaping_dual_run.json");
    QJsonObject payload;
    payload.insert(QStringLiteral("cases"), comparison);
    QFile reportFile(comparisonPath);
    bool comparisonSaved = false;
    if (reportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      reportFile.write(QJsonDocument(payload).toJson(QJsonDocument::Indented));
      comparisonSaved = true;
    }
    std::fprintf(stderr, "dual-run: report path=%s saved=%d\n",
                 comparisonPath.toLocal8Bit().constData(), comparisonSaved ? 1 : 0);
  }
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
