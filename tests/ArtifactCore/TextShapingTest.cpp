#include <gtest/gtest.h>

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QString>

#include <algorithm>
#include <cmath>

import Text.ShapingBackend;
import Text.Style;
import Text.Animator;
import Font.FreeFont;

using namespace ArtifactCore;

namespace {

// Arabic "hello", Hebrew "shalom", Devanagari "na", Georgian "ani".
QString arabic()   { return QString::fromUtf8("\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7"); }
QString hebrew()   { return QString::fromUtf8("\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D"); }
QString devanagari(){ return QString::fromUtf8("\xE0\xA4\xA8\xE0\xA4\xAE\xE0\xA4\xB8\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA5\x87"); }
QString georgian() { return QString::fromUtf8("\xE1\x83\x90\xE1\x83\x9B\xE1\x83\x9C\xE1\x83\xA3\xE1\x83\x9A\xE1\x83\x98"); }
QString ethiopic() { return QString::fromUtf8("\xE1\x88\x9A\xE1\x88\xAD\xE1\x88\xAD"); }
QString cherokee() { return QString::fromUcs4(U"\u13A0\u13A1\u13A2"); }
QString japanese() { return QString::fromUtf8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"); }
QString hiragana() { return QString::fromUtf8("\xE3\x81\xB2\xE3\x82\x8A"); }
QString katakana() { return QString::fromUtf8("\xE3\x82\xAB\xE3\x82\xBF"); }
QString hangul()   { return QString::fromUtf8("\xED\x95\x9C\xEA\xB5\xAD"); }
QString digits()   { return QStringLiteral("0123"); }
QString marks()    { return QString::fromUtf8("\xCC\x88"); }  // COMBINING DIAERESIS

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

namespace {

class QtGuiTestEnvironment final : public testing::Environment
{
public:
  void SetUp() override
  {
    static int argc = 1;
    static char applicationName[] = "ArtifactCoreTextShapingTest";
    static char* argv[] = {applicationName, nullptr};
    static QGuiApplication application(argc, argv);
  }
};

[[maybe_unused]] testing::Environment* const qtGuiEnvironment =
    testing::AddGlobalTestEnvironment(new QtGuiTestEnvironment());

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

TEST(TextShapingScriptTest, AnimatorTagSelectorUsesShapedScriptTags)
{
  QtShapingBackend backend;
  const QString mixed = QStringLiteral("A") + hebrew() + QStringLiteral("Z");
  const auto shaped = backend.shape(makeRequest(mixed));
  ASSERT_FALSE(shaped.glyphs.empty());
  const QString firstTag = shaped.glyphs.front().selectorTag;
  ASSERT_FALSE(firstTag.isEmpty());
  bool hasOtherTag = false;
  for (const auto& glyph : shaped.glyphs) {
    hasOtherTag = hasOtherTag || glyph.selectorTag != firstTag;
  }
  ASSERT_TRUE(hasOtherTag);

  SelectorEvaluationContext context{
      mixed, shaped.glyphs, TextSelectorOrder::Logical};
  RangeSelector selector;
  selector.units = SelectorUnits::Tag;
  selector.start = 0.0f;
  selector.end = 0.0f;
  selector.shape = SelectorShape::Square;
  const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

  ASSERT_EQ(result.weights.size(), shaped.glyphs.size());
  for (size_t index = 0; index < shaped.glyphs.size(); ++index) {
    EXPECT_FLOAT_EQ(result.weights[static_cast<qsizetype>(index)],
                    shaped.glyphs[index].selectorTag == firstTag ? 1.0f : 0.0f);
  }

  AnimatorSelectorSet animator;
  animator.range = selector;
  animator.properties.position = QPointF(4.0, 2.0);
  const std::vector<AnimatorSelectorSet> animators{animator};
  auto animatedGlyphs = shaped.glyphs;
  TextAnimatorEngine::applyAnimatorSets(
      animatedGlyphs, animators, 0.0f, mixed);
  ASSERT_EQ(animatedGlyphs.size(), shaped.glyphs.size());
  for (size_t index = 0; index < animatedGlyphs.size(); ++index) {
    const bool selected = animatedGlyphs[index].selectorTag == firstTag;
    EXPECT_EQ(animatedGlyphs[index].offsetPosition,
              selected ? QPointF(4.0, 2.0) : QPointF(0.0, 0.0));
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

TEST(TextShapingBidiTest, GlyphOrdinalMappingsAreInversePermutationsForRtl)
{
  QtShapingBackend backend;
  const auto result = backend.shape(makeRequest(arabic()));
  ASSERT_EQ(result.logicalToVisual.size(), result.glyphs.size());
  ASSERT_EQ(result.visualToLogical.size(), result.glyphs.size());
  for (int logical = 0; logical < result.logicalToVisual.size(); ++logical) {
    const int visual = result.logicalToVisual.at(logical);
    ASSERT_GE(visual, 0);
    ASSERT_LT(visual, result.visualToLogical.size());
    EXPECT_EQ(result.visualToLogical.at(visual), logical);
  }
}

TEST(TextShapingBidiTest, AnimatorIndexSelectorOrdersShapedGlyphsLogically)
{
  QtShapingBackend backend;
  const QString text = arabic();
  const auto shaped = backend.shape(makeRequest(text));
  ASSERT_GE(shaped.glyphs.size(), 2u);
  ASSERT_EQ(shaped.logicalToVisual.size(), shaped.glyphs.size());
  auto visualGlyphs = shaped.glyphs;
  std::reverse(visualGlyphs.begin(), visualGlyphs.end());
  ASSERT_NE(visualGlyphs.front().index, visualGlyphs.back().index);

  RangeSelector selector;
  selector.units = SelectorUnits::Index;
  selector.start = 0.0f;
  selector.end = 0.0f;
  selector.shape = SelectorShape::Square;
  const SelectorEvaluationContext logicalContext{
      text, visualGlyphs, TextSelectorOrder::Logical};
  const SelectorEvaluationContext visualContext{
      text, visualGlyphs, TextSelectorOrder::Visual};
  const auto logical = TextAnimatorEngine::evaluateSelector(
      logicalContext, selector);
  const auto visual = TextAnimatorEngine::evaluateSelector(
      visualContext, selector);

  ASSERT_EQ(logical.weights.size(), visualGlyphs.size());
  ASSERT_EQ(visual.weights.size(), visualGlyphs.size());
  const auto logicalFirst = std::find_if(
      visualGlyphs.begin(), visualGlyphs.end(), [](const GlyphItem& glyph) {
        return glyph.index == 0;
      });
  ASSERT_NE(logicalFirst, visualGlyphs.end());
  const auto logicalFirstVisualIndex =
      static_cast<qsizetype>(logicalFirst - visualGlyphs.begin());
  EXPECT_FLOAT_EQ(logical.weights[logicalFirstVisualIndex], 1.0f);
  EXPECT_FLOAT_EQ(visual.weights[0], 1.0f);
  for (qsizetype index = 0; index < visualGlyphs.size(); ++index) {
    if (index != logicalFirstVisualIndex) {
      EXPECT_FLOAT_EQ(logical.weights[index], 0.0f);
    }
    if (index != 0) {
      EXPECT_FLOAT_EQ(visual.weights[index], 0.0f);
    }
  }
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

TEST(TextShapingLineTest, AnimatorLineSelectorTargetsShapedLine)
{
  QtShapingBackend backend;
  const QString text = QStringLiteral("AB\nCD");
  ParagraphStyle paragraph;
  paragraph.boxWidth = 56.0f;
  paragraph.wrapMode = TextWrapMode::WrapAnywhere;
  const auto shaped = backend.shape(makeRequest(text, paragraph));
  ASSERT_FALSE(shaped.glyphs.empty());
  bool sawFirstLine = false;
  bool sawSecondLine = false;
  for (const GlyphItem& glyph : shaped.glyphs) {
    sawFirstLine = sawFirstLine || glyph.lineIndex == 0;
    sawSecondLine = sawSecondLine || glyph.lineIndex == 1;
  }
  ASSERT_TRUE(sawFirstLine);
  ASSERT_TRUE(sawSecondLine);

  RangeSelector selector;
  selector.units = SelectorUnits::Line;
  selector.start = 0.0f;
  selector.end = 0.0f;
  selector.shape = SelectorShape::Square;
  const SelectorEvaluationContext context{
      text, shaped.glyphs, TextSelectorOrder::Logical};
  const auto weights = TextAnimatorEngine::evaluateSelector(context, selector);

  ASSERT_EQ(weights.weights.size(), shaped.glyphs.size());
  for (size_t index = 0; index < shaped.glyphs.size(); ++index) {
    EXPECT_FLOAT_EQ(weights.weights[static_cast<qsizetype>(index)],
                    shaped.glyphs[index].lineIndex == 0 ? 1.0f : 0.0f);
  }

  AnimatorSelectorSet animator;
  animator.range = selector;
  animator.properties.position = QPointF(9.0, 1.0);
  const std::vector<AnimatorSelectorSet> animators{animator};
  auto animatedGlyphs = shaped.glyphs;
  TextAnimatorEngine::applyAnimatorSets(
      animatedGlyphs, animators, 0.0f, text);
  for (const GlyphItem& glyph : animatedGlyphs) {
    EXPECT_EQ(glyph.offsetPosition,
              glyph.lineIndex == 0 ? QPointF(9.0, 1.0) : QPointF(0.0, 0.0));
  }
}

TEST(TextShapingLineTest, CarriageReturnLineFeedKeepsFollowingGlyphOnNextLine)
{
  QtShapingBackend backend;
  const QString text = QStringLiteral("A\r\nB");
  const auto shaped = backend.shape(makeRequest(text));

  const auto first = std::find_if(
      shaped.glyphs.begin(), shaped.glyphs.end(),
      [](const GlyphItem& glyph) { return glyph.charCode == 'A'; });
  const auto second = std::find_if(
      shaped.glyphs.begin(), shaped.glyphs.end(),
      [](const GlyphItem& glyph) { return glyph.charCode == 'B'; });
  ASSERT_NE(first, shaped.glyphs.end());
  ASSERT_NE(second, shaped.glyphs.end());
  EXPECT_EQ(first->lineIndex, 0);
  EXPECT_EQ(second->lineIndex, 1);
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

  ASSERT_FALSE(result.glyphs.empty());
  const SelectorEvaluationContext context{
      decomposed, result.glyphs, TextSelectorOrder::Logical};
  RangeSelector selector;
  selector.regexEnabled = true;
  selector.selectorPattern = marks();
  selector.units = SelectorUnits::Index;
  selector.start = 0.0f;
  selector.end = static_cast<float>(result.glyphs.size() - 1);
  selector.shape = SelectorShape::Square;
  const auto weights = TextAnimatorEngine::evaluateSelector(context, selector);
  ASSERT_EQ(weights.weights.size(), result.glyphs.size());
  for (const float weight : weights.weights) {
    EXPECT_FLOAT_EQ(weight, 1.0f);
  }

  AnimatorSelectorSet animator;
  animator.range = selector;
  animator.properties.position = QPointF(3.0, 6.0);
  const std::vector<AnimatorSelectorSet> animators{animator};
  auto animatedGlyphs = result.glyphs;
  TextAnimatorEngine::applyAnimatorSets(
      animatedGlyphs, animators, 0.0f, decomposed);
  for (const GlyphItem& glyph : animatedGlyphs) {
    EXPECT_EQ(glyph.offsetPosition, QPointF(3.0, 6.0));
  }
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

  ASSERT_FALSE(result.glyphs.empty());
  SelectorEvaluationContext animatorContext{
      family, result.glyphs, TextSelectorOrder::Logical};
  RangeSelector selector;
  selector.units = SelectorUnits::Cluster;
  selector.start = 0.0f;
  selector.end = 0.0f;
  selector.shape = SelectorShape::Square;
  const auto weights = TextAnimatorEngine::evaluateSelector(
      animatorContext, selector);
  ASSERT_EQ(weights.weights.size(), result.glyphs.size());
  for (const float weight : weights.weights) {
    EXPECT_FLOAT_EQ(weight, 1.0f);
  }

  RangeSelector regexSelector;
  regexSelector.regexEnabled = true;
  regexSelector.selectorPattern = QString::fromUtf8("\xF0\x9F\x91\xA8");
  regexSelector.units = SelectorUnits::Index;
  regexSelector.start = 0.0f;
  regexSelector.end = static_cast<float>(result.glyphs.size() - 1);
  regexSelector.shape = SelectorShape::Square;
  const auto regexWeights = TextAnimatorEngine::evaluateSelector(
      animatorContext, regexSelector);
  ASSERT_EQ(regexWeights.weights.size(), result.glyphs.size());
  for (const float weight : regexWeights.weights) {
    EXPECT_FLOAT_EQ(weight, 1.0f);
  }

  AnimatorSelectorSet animator;
  animator.range = regexSelector;
  animator.properties.position = QPointF(5.0, -3.0);
  const std::vector<AnimatorSelectorSet> animators{animator};
  auto animatedGlyphs = result.glyphs;
  TextAnimatorEngine::applyAnimatorSets(
      animatedGlyphs, animators, 0.0f, family);
  ASSERT_EQ(animatedGlyphs.size(), result.glyphs.size());
  for (const GlyphItem& glyph : animatedGlyphs) {
    EXPECT_EQ(glyph.offsetPosition, QPointF(5.0, -3.0));
  }
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

TEST(TextShapingClusterTest, AnimatorRegexSelectsAstralShapedGlyph)
{
  QtShapingBackend backend;
  const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
  const QString text = QStringLiteral("A") + emoji + QStringLiteral("B");
  const auto shaped = backend.shape(makeRequest(text));
  ASSERT_EQ(shaped.glyphs.size(), 3u);

  SelectorEvaluationContext context{
      text, shaped.glyphs, TextSelectorOrder::Logical};
  RangeSelector selector;
  selector.regexEnabled = true;
  selector.selectorPattern = emoji;
  selector.units = SelectorUnits::Index;
  selector.start = 1.0f;
  selector.end = 1.0f;
  selector.shape = SelectorShape::Square;
  const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

  ASSERT_EQ(result.weights.size(), 3);
  EXPECT_FLOAT_EQ(result.weights[0], 0.0f);
  EXPECT_FLOAT_EQ(result.weights[1], 1.0f);
  EXPECT_FLOAT_EQ(result.weights[2], 0.0f);

  AnimatorSelectorSet animator;
  animator.range = selector;
  animator.properties.position = QPointF(7.0, -2.0);
  animator.properties.opacity = 0.4f;
  const std::vector<AnimatorSelectorSet> animators{animator};
  auto animatedGlyphs = shaped.glyphs;
  TextAnimatorEngine::applyAnimatorSets(
      animatedGlyphs, animators, 0.0f, text);

  ASSERT_EQ(animatedGlyphs.size(), 3u);
  EXPECT_EQ(animatedGlyphs[0].offsetPosition, QPointF(0.0, 0.0));
  EXPECT_FLOAT_EQ(animatedGlyphs[0].offsetOpacity, 1.0f);
  EXPECT_EQ(animatedGlyphs[1].offsetPosition, QPointF(7.0, -2.0));
  EXPECT_FLOAT_EQ(animatedGlyphs[1].offsetOpacity, 0.4f);
  EXPECT_EQ(animatedGlyphs[2].offsetPosition, QPointF(0.0, 0.0));
  EXPECT_FLOAT_EQ(animatedGlyphs[2].offsetOpacity, 1.0f);
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
  // Use the repository's Apache-2.0 OpenUSD Roboto fixture so HarfBuzz can
  // resolve font bytes without depending on fonts installed on the test host.
  QDir repositoryRoot(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
  ASSERT_TRUE(repositoryRoot.cdUp());
  ASSERT_TRUE(repositoryRoot.cdUp());
  const QString fontPath = repositoryRoot.filePath(
      QStringLiteral("libs/openusd/pxr/usdImaging/usdviewq/fonts/Roboto/Roboto-Regular.ttf"));
  ASSERT_TRUE(QFileInfo::exists(fontPath)) << fontPath.toStdString();
  ASSERT_TRUE(FontManager::loadFontFromFile(fontPath));
  const auto fontBytes = FontManager::fontFileBytes(QStringLiteral("Roboto"));
  ASSERT_TRUE(fontBytes.has_value());
  ASSERT_FALSE(fontBytes->isEmpty());

  HarfBuzzShapingBackend hb;
  QtShapingBackend qt;
  auto request = makeRequest(QStringLiteral("Text Sample1"));
  request.style.fontFamily = UniString(QStringLiteral("Roboto"));
  const auto hbResult = hb.shape(request);
  const auto qtResult = qt.shape(request);
  ASSERT_FALSE(hbResult.glyphs.empty());
  ASSERT_FALSE(qtResult.glyphs.empty());
  EXPECT_EQ(hbResult.glyphs.size(), qtResult.glyphs.size());
  EXPECT_EQ(hbResult.logicalToVisual.size(), hbResult.glyphs.size());
  EXPECT_EQ(hbResult.visualToLogical.size(), hbResult.glyphs.size());
  for (const auto& glyph : hbResult.glyphs) {
    EXPECT_TRUE(std::isfinite(glyph.basePosition.x()));
    EXPECT_TRUE(std::isfinite(glyph.basePosition.y()));
  }
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
