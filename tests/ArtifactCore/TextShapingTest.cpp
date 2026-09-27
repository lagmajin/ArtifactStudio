#include <gtest/gtest.h>

#include <QString>

import Text.ShapingBackend;
import Text.Style;
import Font.FreeFont;

using namespace ArtifactCore;

namespace {

// Arabic "hello", Hebrew "shalom", Devanagari "na", Georgian "ani".
QString arabic()   { return QString::fromUtf8("\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7"); }
QString hebrew()   { return QString::fromUtf8("\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D"); }
QString devanagari(){ return QString::fromUtf8("\xE0\xA4\xA8\xE0\xA4\xAE\xE0\xA4\xB8\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA5\x87"); }
QString georgian() { return QString::fromUtf8("\xE1\x83\x90\xE1\x83\x9B\xE1\x83\x9C\xE1\x83\xA3\xE1\x83\x9A\xE1\x83\x98"); }
QString ethiopic() { return QString::fromUtf8("\xE1\x88\x9A\xE1\x88\xAD\xE1\x88\xAD"); }
QString cherokee() { return QString::fromUtf8("\xE1\x8A\xAE\xE1\x8B\x85\xE1\x8A\xA0"); }
QString japanese() { return QString::fromUtf8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"); }
QString hiragana() { return QString::fromUtf8("\xE3\x81\xB2\xE3\x82\x8A"); }
QString katakana() { return QString::fromUtf8("\xE3\x82\xAB\xE3\x82\xBF"); }
QString hangul()   { return QString::fromUtf8("\xED\x95\x9C\xEA\xB5\xAD"); }
QString digits()   { return QStringLiteral("0123"); }
QString marks()    { return QString::fromUtf8("\xE2\x84\x96"); }  // COMBINING DIAERESIS

// A family that exists on the CI machine matters less than the shaping
// contract, so fall back to whatever the platform reports as the default.
TextStyle testStyle()
{
  TextStyle style;
  style.fontFamily = UniString(FontManager::defaultSansSerifFamily());
  style.fontSize = 48.0f;
  style.pixelSize = 48.0f;
  return style;
}

TextShapingRequest makeRequest(const QString& text,
                               const ParagraphStyle& paragraph = {})
{
  TextShapingRequest request;
  request.text = text;
  request.style = testStyle();
  request.paragraph = paragraph;
  request.writingMode = TextWritingMode::Horizontal;
  request.baseDirection = TextDirection::Auto;
  request.locale = QStringLiteral("en-US");
  return request;
}

QStringList scriptTagsOf(const TextLayoutContract& contract)
{
  QStringList tags;
  for (const auto& run : contract.scriptRuns) tags.append(run.scriptTag);
  return tags;
}

} // namespace

// --- 1. script property ---------------------------------------------------

TEST(TextShapingScriptTest, LatinDigitsAndMarksAreNotMislabelled)
{
  QtShapingBackend backend;
  const auto latin = backend.shape(makeRequest(QStringLiteral("abc"))).contract;
  EXPECT_TRUE(scriptTagsOf(latin).contains(QStringLiteral("Latn")));

  // Digits and punctuation are Common, not Latin.  Reporting "Latn" for them
  // was the original defect.
  const auto numbers = backend.shape(makeRequest(digits())).contract;
  EXPECT_TRUE(scriptTagsOf(numbers).contains(QStringLiteral("Zyyy")));

  // A combining mark is Inherited, which is neither Latin nor a real script.
  const auto mark = backend.shape(makeRequest(marks())).contract;
  EXPECT_TRUE(scriptTagsOf(mark).contains(QStringLiteral("Zinh")));
}

TEST(TextShapingScriptTest, MajorScriptsAreReported)
{
  QtShapingBackend backend;
  struct Case { const char* label; QString text; const char* tag; };
  const Case cases[] = {
      {"arabic",    arabic(),    "Arab"},
      {"hebrew",    hebrew(),    "Hebr"},
      {"devanagari", devanagari(), "Deva"},
      {"japanese",  japanese(),  "Hani"},
      {"hangul",    hangul(),    "Hang"},
  };
  for (const auto& item : cases) {
    const auto contract = backend.shape(makeRequest(item.text)).contract;
    EXPECT_TRUE(scriptTagsOf(contract).contains(QString::fromLatin1(item.tag)))
        << item.label << " scripts=" << scriptTagsOf(contract).join(QLatin1Char(','))
        .toStdString();
  }
}

TEST(TextShapingScriptTest, KanaIsDistinguishedFromKanji)
{
  QtShapingBackend backend;
  // The previous range table merged all of U+3040..U+9FFF into "Hani", which
  // made kana-vertical metrics and kana alternates unreachable.
  const auto hira = backend.shape(makeRequest(hiragana())).contract;
  const auto kata = backend.shape(makeRequest(katakana())).contract;
  EXPECT_TRUE(scriptTagsOf(hira).contains(QStringLiteral("Hira")))
      << "scripts=" << scriptTagsOf(hira).join(QLatin1Char(',')).toStdString();
  EXPECT_TRUE(scriptTagsOf(kata).contains(QStringLiteral("Kana")))
      << "scripts=" << scriptTagsOf(kata).join(QLatin1Char(',')).toStdString();
}

TEST(TextShapingScriptTest, PreviouslyMissingScriptsAreNotReportedAsLatin)
{
  QtShapingBackend backend;
  // The core regression: these all used to fall through to the "Latn"
  // default, which then fed animator selectors and the inspector.
  struct Case { const char* label; QString text; const char* tag; };
  const Case cases[] = {
      {"georgian", georgian(), "Geor"},
      {"ethiopic", ethiopic(), "Ethi"},
      {"cherokee", cherokee(), "Cher"},
  };
  for (const auto& item : cases) {
    const auto contract = backend.shape(makeRequest(item.text)).contract;
    const QStringList tags = scriptTagsOf(contract);
    EXPECT_FALSE(tags.contains(QStringLiteral("Latn"))) << item.label;
    EXPECT_TRUE(tags.contains(QString::fromLatin1(item.tag)))
        << item.label << " scripts=" << tags.join(QLatin1Char(',')).toStdString();
  }
}

TEST(TextShapingScriptTest, CombiningMarksAreNotFlaggedAsComplexScripts)
{
  QtShapingBackend backend;
  // A combining mark used to satisfy the old "tag != Latn means complex" rule.
  const auto contract = backend.shape(makeRequest(marks())).contract;
  for (const auto& run : contract.scriptRuns) {
    EXPECT_FALSE(run.isComplexScript)
        << "tag=" << run.scriptTag.toStdString();
  }
}

// --- 2. bidi -------------------------------------------------------------

TEST(TextShapingBidiTest, PureRtlResolvesToRightToLeft)
{
  QtShapingBackend backend;
  for (const QString& text : {arabic(), hebrew()}) {
    const auto result = backend.shape(makeRequest(text));
    EXPECT_EQ(result.contract.baseDirection, TextDirection::RightToLeft)
        << text.toUtf8().constData();
  }
}

TEST(TextShapingBidiTest, MixedLineSplitsIntoMultipleRuns)
{
  QtShapingBackend backend;
  const QString mixed = QStringLiteral("abc ") + hebrew() + QStringLiteral(" def");
  const auto result = backend.shape(makeRequest(mixed));
  // Previously bidiRuns was always a single run covering the whole string.
  EXPECT_GE(result.contract.bidiRuns.size(), 3)
      << "runs=" << result.contract.bidiRuns.size();
  bool sawRtl = false;
  for (const auto& run : result.contract.bidiRuns) {
    if (run.direction == TextDirection::RightToLeft) sawRtl = true;
  }
  EXPECT_TRUE(sawRtl);
}

TEST(TextShapingBidiTest, VisualMappingIsNotIdentityForRtl)
{
  QtShapingBackend backend;
  const auto result = backend.shape(makeRequest(arabic()));
  ASSERT_FALSE(result.logicalToVisual.isEmpty());
  // The old implementation emitted a literal i -> i permutation.
  bool sawNonIdentity = false;
  for (int i = 0; i < result.logicalToVisual.size(); ++i) {
    if (result.logicalToVisual.at(i) != i) {
      sawNonIdentity = true;
      break;
    }
  }
  EXPECT_TRUE(sawNonIdentity);
}

TEST(TextShapingBidiTest, PureLtrKeepsIdentityMapping)
{
  QtShapingBackend backend;
  const auto result = backend.shape(makeRequest(QStringLiteral("abc")));
  ASSERT_EQ(result.logicalToVisual.size(), result.glyphs.size());
  for (int i = 0; i < result.logicalToVisual.size(); ++i) {
    EXPECT_EQ(result.logicalToVisual.at(i), i) << "i=" << i;
  }
}

// --- 3. grapheme clusters ------------------------------------------------

TEST(TextShapingClusterTest, CombiningMarkFormsOneCluster)
{
  QtShapingBackend backend;
  const QString decomposed = QStringLiteral("e") + marks();
  const auto result = backend.shape(makeRequest(decomposed));
  ASSERT_EQ(result.contract.clusters.size(), 1)
      << "clusters=" << result.contract.clusters.size();
  EXPECT_FALSE(result.contract.clusters.at(0).isEmojiSequence);
}

TEST(TextShapingClusterTest, EmojiZwjSequenceFormsOneCluster)
{
  QtShapingBackend backend;
  // family: man + ZWJ + woman + ZWJ + girl + ZWJ + boy
  const QString family = QString::fromUtf8(
      "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
      "\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6");
  const auto result = backend.shape(makeRequest(family));
  ASSERT_EQ(result.contract.clusters.size(), 1)
      << "clusters=" << result.contract.clusters.size();
  EXPECT_TRUE(result.contract.clusters.at(0).isEmojiSequence);
}

TEST(TextShapingClusterTest, SkinToneModifierJoinsItsCluster)
{
  QtShapingBackend backend;
  // thumbs up + medium skin tone
  const QString thumbs = QString::fromUtf8("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD");
  const auto result = backend.shape(makeRequest(thumbs));
  ASSERT_EQ(result.contract.clusters.size(), 1)
      << "clusters=" << result.contract.clusters.size();
  EXPECT_TRUE(result.contract.clusters.at(0).isEmojiSequence);
}

TEST(TextShapingClusterTest, RegionalIndicatorPairIsOneCluster)
{
  QtShapingBackend backend;
  // regional indicator J + regional indicator P
  const QString flag = QString::fromUtf8("\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5");
  const auto result = backend.shape(makeRequest(flag));
  ASSERT_EQ(result.contract.clusters.size(), 1)
      << "clusters=" << result.contract.clusters.size();
}

// --- 4. backend parity ---------------------------------------------------

TEST(TextShapingBackendTest, HarfBuzzHandlesUnwrappedSingleLine)
{
  HarfBuzzShapingBackend hb;
  QtShapingBackend qt;
  const auto request = makeRequest(QStringLiteral("Text Sample1"));
  const auto hbResult = hb.shape(request);
  const auto qtResult = qt.shape(request);
  ASSERT_FALSE(hbResult.glyphs.empty());
  // If the font file could not be resolved the HarfBuzz path silently defers
  // to Qt, which makes every comparison below vacuous.  Surface that.
  const bool looksLikeFallback =
      hbResult.glyphs.size() == qtResult.glyphs.size() &&
      hbResult.logicalToVisual == qtResult.logicalToVisual;
  EXPECT_FALSE(looksLikeFallback)
      << "HarfBuzz appears to be deferring to Qt; check FontManager::fontFileBytes";
}

TEST(TextShapingBackendTest, WrappedTextFallsBackToQt)
{
  HarfBuzzShapingBackend hb;
  QtShapingBackend qt;
  ParagraphStyle wrapped;
  wrapped.boxWidth = 200.0f;
  const auto request = makeRequest(QStringLiteral("Wrapping text sample"), wrapped);
  // HarfBuzz does not break lines, so wrapped text must be handled by Qt.
  EXPECT_EQ(hb.shape(request).logicalToVisual,
            qt.shape(request).logicalToVisual);
}

TEST(TextShapingBackendTest, ComplexScriptShapesAtLeastAsManyGlyphsAsQt)
{
  HarfBuzzShapingBackend hb;
  QtShapingBackend qt;
  const auto request = makeRequest(arabic());
  const auto hbResult = hb.shape(request);
  const auto qtResult = qt.shape(request);
  ASSERT_FALSE(hbResult.glyphs.empty());
  EXPECT_GE(hbResult.glyphs.size(), qtResult.glyphs.size());
}

TEST(TextShapingBackendTest, VerticalWritingIsLeftToTheQtPath)
{
  HarfBuzzShapingBackend hb;
  QtShapingBackend qt;
  auto request = makeRequest(japanese());
  request.writingMode = TextWritingMode::Vertical;
  EXPECT_EQ(hb.shape(request).logicalToVisual, qt.shape(request).logicalToVisual);
}
