#include <gtest/gtest.h>

#include <QPointF>
#include <QString>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

import Text.Animator;
import FloatRGBA;

using namespace ArtifactCore;

namespace {

std::vector<GlyphItem> makeGlyphs(const QString& text)
{
    std::vector<GlyphItem> glyphs;
    glyphs.reserve(static_cast<size_t>(text.size()));
    for (qsizetype index = 0; index < text.size(); ++index) {
        GlyphItem glyph{};
        glyph.charCode = text.at(index).unicode();
        glyph.index = static_cast<int>(index);
        glyph.clusterIndex = static_cast<int>(index);
        glyph.lineIndex = 0;
        glyph.paragraphIndex = 0;
        glyph.clusterId = QString::number(index);
        glyph.clusterText = text.mid(index, 1);
        glyphs.push_back(std::move(glyph));
    }
    return glyphs;
}

} // namespace

TEST(TextAnimatorContractTest, PercentageRampIncludesBothEndpoints)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abcd"));
    const SelectorEvaluationContext context{
        QStringLiteral("abcd"), glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.shape = SelectorShape::RampUp;

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 4);
    EXPECT_FLOAT_EQ(result.weights[0], 0.0f);
    EXPECT_NEAR(result.weights[1], 1.0f / 3.0f, 1e-6f);
    EXPECT_NEAR(result.weights[2], 2.0f / 3.0f, 1e-6f);
    EXPECT_FLOAT_EQ(result.weights[3], 1.0f);
    EXPECT_TRUE(result.diagnostic.isEmpty());
}

TEST(TextAnimatorContractTest, EverySelectorShapeMatchesItsBoundaryAndMidpointContract)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abcd"));
    const SelectorEvaluationContext context{
        QStringLiteral("abcd"), glyphs, TextSelectorOrder::Logical};
    const SelectorShape shapes[] = {
        SelectorShape::Square, SelectorShape::RampUp,
        SelectorShape::RampDown, SelectorShape::Triangle,
        SelectorShape::Round, SelectorShape::Smooth,
    };
    const float expected[][4] = {
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f},
        {1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 0.0f},
        {0.0f, 2.0f / 3.0f, 2.0f / 3.0f, 0.0f},
        {0.0f, 0.94280904f, 0.94280904f, 0.0f},
        {0.0f, 0.25f, 0.75f, 1.0f},
    };

    for (size_t shapeIndex = 0; shapeIndex < std::size(shapes); ++shapeIndex) {
        RangeSelector selector;
        selector.shape = shapes[shapeIndex];
        const auto result = TextAnimatorEngine::evaluateSelector(context, selector);
        ASSERT_EQ(result.weights.size(), 4);
        for (qsizetype glyphIndex = 0; glyphIndex < result.weights.size(); ++glyphIndex) {
            EXPECT_NEAR(result.weights[glyphIndex],
                        expected[shapeIndex][glyphIndex], 1e-6f)
                << "shape=" << shapeIndex << " glyph=" << glyphIndex;
        }
    }
}

TEST(TextAnimatorContractTest, NonFiniteRangeBoundarySelectsNoGlyphs)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.start = std::numeric_limits<float>::quiet_NaN();

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    for (const float weight : result.weights) {
        EXPECT_FLOAT_EQ(weight, 0.0f);
    }
}

TEST(TextAnimatorContractTest, NonFiniteOffsetCannotEmitNonFiniteWeights)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.units = SelectorUnits::Index;
    selector.start = 0.0f;
    selector.end = 2.0f;
    selector.offset = std::numeric_limits<float>::infinity();
    selector.easeHigh = std::numeric_limits<float>::quiet_NaN();
    selector.easeLow = std::numeric_limits<float>::infinity();

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    for (const float weight : result.weights) {
        EXPECT_TRUE(std::isfinite(weight));
        EXPECT_FLOAT_EQ(weight, 0.0f);
    }
}

TEST(TextAnimatorContractTest, InfiniteEaseValuesKeepEveryWeightFiniteAndBounded)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.units = SelectorUnits::Index;
    selector.start = 0.0f;
    selector.end = 2.0f;
    selector.shape = SelectorShape::RampUp;
    selector.easeHigh = std::numeric_limits<float>::infinity();
    selector.easeLow = std::numeric_limits<float>::infinity();

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    for (const float weight : result.weights) {
        EXPECT_TRUE(std::isfinite(weight));
        EXPECT_GE(weight, 0.0f);
        EXPECT_LE(weight, 1.0f);
    }
}

TEST(TextAnimatorContractTest, InvertedRangeHasSameSelectionAsAscendingRange)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abcde"));
    const SelectorEvaluationContext context{
        QStringLiteral("abcde"), glyphs, TextSelectorOrder::Logical};
    RangeSelector ascending;
    ascending.units = SelectorUnits::Index;
    ascending.start = 1.0f;
    ascending.end = 3.0f;
    ascending.shape = SelectorShape::Square;
    RangeSelector inverted = ascending;
    inverted.start = 3.0f;
    inverted.end = 1.0f;

    const auto forward = TextAnimatorEngine::evaluateSelector(context, ascending);
    const auto reverse = TextAnimatorEngine::evaluateSelector(context, inverted);

    ASSERT_EQ(forward.weights.size(), reverse.weights.size());
    for (qsizetype index = 0; index < forward.weights.size(); ++index) {
        EXPECT_FLOAT_EQ(forward.weights[index], reverse.weights[index])
            << "glyph index " << index;
    }
    EXPECT_FLOAT_EQ(forward.weights[0], 0.0f);
    EXPECT_FLOAT_EQ(forward.weights[1], 1.0f);
    EXPECT_FLOAT_EQ(forward.weights[3], 1.0f);
    EXPECT_FLOAT_EQ(forward.weights[4], 0.0f);
}

TEST(TextAnimatorContractTest, RangeEaseHighAndLowWarpShapeProgress)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.units = SelectorUnits::Index;
    selector.start = 0.0f;
    selector.end = 2.0f;
    selector.shape = SelectorShape::RampUp;
    selector.easeHigh = 10.0f;
    const auto high = TextAnimatorEngine::evaluateSelector(context, selector);
    selector.easeLow = 10.0f;
    const auto both = TextAnimatorEngine::evaluateSelector(context, selector);
    selector.easeHigh = 0.0f;
    const auto low = TextAnimatorEngine::evaluateSelector(context, selector);

    EXPECT_FLOAT_EQ(high.weights[0], 0.0f);
    EXPECT_NEAR(high.weights[1], 0.25f, 1e-6f);
    EXPECT_FLOAT_EQ(high.weights[2], 1.0f);
    EXPECT_NEAR(both.weights[1], 0.4375f, 1e-6f);
    EXPECT_NEAR(low.weights[1], 0.75f, 1e-6f);
}

TEST(TextAnimatorContractTest, InvalidRegexReportsFailureAndSelectsNothing)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.regexEnabled = true;
    selector.selectorPattern = QStringLiteral("(");

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    EXPECT_FALSE(result.diagnostic.isEmpty());
    for (const float weight : result.weights) {
        EXPECT_FLOAT_EQ(weight, 0.0f);
    }
}

TEST(TextAnimatorContractTest, RegexMatchesSourceGlyphAndReportsCoverage)
{
    const auto glyphs = makeGlyphs(QString::fromUtf8("A猫B"));
    const SelectorEvaluationContext context{
        QString::fromUtf8("A猫B"), glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.regexEnabled = true;
    selector.selectorPattern = QString::fromUtf8("猫");

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    EXPECT_FLOAT_EQ(result.weights[0], 0.0f);
    EXPECT_FLOAT_EQ(result.weights[1], 1.0f);
    EXPECT_FLOAT_EQ(result.weights[2], 0.0f);
    EXPECT_NE(result.diagnostic.indexOf(QStringLiteral("regex")), -1);
}

TEST(TextAnimatorContractTest, RegexMapsAstralCodepointAcrossUtf16SurrogatePair)
{
    const QString source = QString::fromUtf8("A😀B");
    std::vector<GlyphItem> glyphs(4);
    const int logicalIndices[] = {0, 1, 1, 2};
    const int clusterIndices[] = {0, 1, 1, 2};
    const char32_t codepoints[] = {'A', 0x1F600, 0x1F600, 'B'};
    const QString clusterTexts[] = {
        QStringLiteral("A"), QString::fromUtf8("😀"),
        QString::fromUtf8("😀"), QStringLiteral("B"),
    };
    for (size_t index = 0; index < glyphs.size(); ++index) {
        glyphs[index].index = logicalIndices[index];
        glyphs[index].clusterIndex = clusterIndices[index];
        glyphs[index].lineIndex = 0;
        glyphs[index].paragraphIndex = 0;
        glyphs[index].charCode = codepoints[index];
        glyphs[index].clusterText = clusterTexts[index];
        glyphs[index].clusterId = QString::number(clusterIndices[index]);
    }
    const SelectorEvaluationContext context{
        source, glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.regexEnabled = true;
    selector.selectorPattern = QString::fromUtf8("😀");

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 4);
    EXPECT_FLOAT_EQ(result.weights[0], 0.0f);
    EXPECT_FLOAT_EQ(result.weights[1], 1.0f);
    EXPECT_FLOAT_EQ(result.weights[2], 1.0f);
    EXPECT_FLOAT_EQ(result.weights[3], 0.0f);
}

TEST(TextAnimatorContractTest, PercentageRangeTreatsMultipleGlyphsAsOneCluster)
{
    std::vector<GlyphItem> glyphs(4);
    const int logicalIndices[] = {0, 1, 1, 2};
    const int clusterIndices[] = {0, 1, 1, 2};
    for (size_t index = 0; index < glyphs.size(); ++index) {
        glyphs[index].index = logicalIndices[index];
        glyphs[index].clusterIndex = clusterIndices[index];
        glyphs[index].lineIndex = 0;
        glyphs[index].paragraphIndex = 0;
    }
    const SelectorEvaluationContext context{
        QString::fromUtf8("A😀B"), glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.start = 0.0f;
    selector.end = 50.0f;
    selector.shape = SelectorShape::Square;

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 4);
    EXPECT_FLOAT_EQ(result.weights[0], 1.0f);
    EXPECT_FLOAT_EQ(result.weights[1], 1.0f);
    EXPECT_FLOAT_EQ(result.weights[2], 1.0f);
    EXPECT_FLOAT_EQ(result.weights[3], 0.0f);
}

TEST(TextAnimatorContractTest, ClusterLineTagUnitsAndOffsetSelectTheirLogicalDomains)
{
    auto glyphs = makeGlyphs(QStringLiteral("abcde"));
    const int clusterIndices[] = {0, 0, 1, 2, 2};
    const int lineIndices[] = {0, 0, 1, 1, 2};
    const QString tags[] = {
        QStringLiteral("latin"), QStringLiteral("latin"),
        QStringLiteral("cjk"), QStringLiteral("cjk"),
        QStringLiteral("emoji"),
    };
    for (size_t index = 0; index < glyphs.size(); ++index) {
        glyphs[index].clusterIndex = clusterIndices[index];
        glyphs[index].lineIndex = lineIndices[index];
        glyphs[index].selectorTag = tags[index];
    }
    const SelectorEvaluationContext context{
        QStringLiteral("abcde"), glyphs, TextSelectorOrder::Logical};
    const auto evaluateUnit = [&](SelectorUnits units, float start, float offset = 0.0f) {
        RangeSelector selector;
        selector.units = units;
        selector.start = start;
        selector.end = start;
        selector.offset = offset;
        selector.shape = SelectorShape::Square;
        return TextAnimatorEngine::evaluateSelector(context, selector);
    };

    const auto clusters = evaluateUnit(SelectorUnits::Cluster, 1.0f);
    const auto lines = evaluateUnit(SelectorUnits::Line, 1.0f);
    const auto tagsResult = evaluateUnit(SelectorUnits::Tag, 1.0f);
    const auto offset = evaluateUnit(SelectorUnits::Index, 0.0f, 1.0f);

    const float expectedCluster[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    const float expectedLine[] = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f};
    const float expectedOffset[] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    for (qsizetype index = 0; index < 5; ++index) {
        EXPECT_FLOAT_EQ(clusters.weights[index], expectedCluster[index]);
        EXPECT_FLOAT_EQ(lines.weights[index], expectedLine[index]);
        EXPECT_FLOAT_EQ(tagsResult.weights[index], expectedLine[index]);
        EXPECT_FLOAT_EQ(offset.weights[index], expectedOffset[index]);
    }
}

TEST(TextAnimatorContractTest, ExpressionUsesOneBasedIndexAndClampsWeights)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical,
        0, 3, 0.0f};
    ExpressionSelector selector;
    selector.enabled = true;
    selector.expression = QStringLiteral("textIndex / textTotal");

    const auto result = TextAnimatorEngine::evaluateExpressionSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    EXPECT_NEAR(result.weights[0], 1.0f / 3.0f, 1e-6f);
    EXPECT_NEAR(result.weights[1], 2.0f / 3.0f, 1e-6f);
    EXPECT_FLOAT_EQ(result.weights[2], 1.0f);
    EXPECT_TRUE(result.diagnostic.isEmpty());
}

TEST(TextAnimatorContractTest, ExpressionClampsValuesBelowZeroAndAboveOne)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    ExpressionSelector selector;
    selector.enabled = true;
    selector.expression = QStringLiteral("2 * textIndex - 3");

    const auto result = TextAnimatorEngine::evaluateExpressionSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    EXPECT_FLOAT_EQ(result.weights[0], 0.0f);
    EXPECT_FLOAT_EQ(result.weights[1], 1.0f);
    EXPECT_FLOAT_EQ(result.weights[2], 1.0f);
    EXPECT_TRUE(result.diagnostic.isEmpty());
}

TEST(TextAnimatorContractTest, CombinedExpressionWeightsClampAfterCombineAndExtraWeight)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abcd"));
    const SelectorEvaluationContext context{
        QStringLiteral("abcd"), glyphs, TextSelectorOrder::Logical};
    AnimatorSelectorSet set;
    set.range.units = SelectorUnits::Index;
    set.range.start = 0.0f;
    set.range.end = 3.0f;
    set.range.shape = SelectorShape::Square;
    set.expression.enabled = true;
    set.expression.expression = QStringLiteral("textIndex - 2");
    set.combine = SelectorCombineMode::Add;
    const float extraWeights[] = {2.0f, 0.5f, -1.0f};

    const auto weights = TextAnimatorEngine::evaluateAnimatorWeights(
        context, set, extraWeights);

    ASSERT_EQ(weights.size(), 4u);
    EXPECT_FLOAT_EQ(weights[0], 0.0f);  // max(0 + (-1), 0) * 2
    EXPECT_FLOAT_EQ(weights[1], 0.5f);  // (1 + 0) * 0.5
    EXPECT_FLOAT_EQ(weights[2], 0.0f);  // (1 + 1) * -1, then clamp
    EXPECT_FLOAT_EQ(weights[3], 1.0f);  // (1 + 2) * missing-extra default, clamp
}

TEST(TextAnimatorContractTest, ExpressionFailureZerosTheWholeSelectorResult)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    ExpressionSelector selector;
    selector.enabled = true;
    selector.expression = QStringLiteral("textIndex == 2 ? 1 / 0 : 0.5");

    const auto result = TextAnimatorEngine::evaluateExpressionSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    EXPECT_FALSE(result.diagnostic.isEmpty());
    for (const float weight : result.weights) {
        EXPECT_FLOAT_EQ(weight, 0.0f);
    }
}

TEST(TextAnimatorContractTest, ExpressionReceivesFixedEvaluationTime)
{
    const auto glyphs = makeGlyphs(QStringLiteral("ab"));
    const SelectorEvaluationContext context{
        QStringLiteral("ab"), glyphs, TextSelectorOrder::Logical,
        0, 2, 0.375f};
    ExpressionSelector selector;
    selector.enabled = true;
    selector.expression = QStringLiteral("time");

    const auto result = TextAnimatorEngine::evaluateExpressionSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 2);
    EXPECT_FLOAT_EQ(result.weights[0], 0.375f);
    EXPECT_FLOAT_EQ(result.weights[1], 0.375f);
}

TEST(TextAnimatorContractTest, MalformedExpressionReportsFailureAndZerosAllWeights)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    ExpressionSelector selector;
    selector.enabled = true;
    selector.expression = QStringLiteral("textIndex +");

    const auto result = TextAnimatorEngine::evaluateExpressionSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    EXPECT_FALSE(result.diagnostic.isEmpty());
    for (const float weight : result.weights) {
        EXPECT_FLOAT_EQ(weight, 0.0f);
    }
}

TEST(TextAnimatorContractTest, RandomStableOrderIsDeterministicAndComplete)
{
    constexpr int count = 128;
    const auto first = TextAnimatorEngine::createOrderMap(
        count, SelectorOrder::RandomStable, 918273);
    const auto repeat = TextAnimatorEngine::createOrderMap(
        count, SelectorOrder::RandomStable, 918273);
    const auto otherSeed = TextAnimatorEngine::createOrderMap(
        count, SelectorOrder::RandomStable, 918274);

    ASSERT_EQ(first.size(), count);
    EXPECT_EQ(first, repeat);
    EXPECT_NE(first, otherSeed);
    auto sorted = first;
    std::sort(sorted.begin(), sorted.end());
    for (int index = 0; index < count; ++index) {
        EXPECT_EQ(sorted[static_cast<size_t>(index)], index);
    }
}

TEST(TextAnimatorContractTest, SelectorOrdersProduceExpectedPermutation)
{
    const std::vector<std::pair<SelectorOrder, std::vector<int>>> cases{
        {SelectorOrder::Natural, {0, 1, 2, 3, 4}},
        {SelectorOrder::LeftToRight, {0, 1, 2, 3, 4}},
        {SelectorOrder::Reverse, {4, 3, 2, 1, 0}},
        {SelectorOrder::RightToLeft, {4, 3, 2, 1, 0}},
        {SelectorOrder::CenterOut, {2, 1, 3, 0, 4}},
        {SelectorOrder::EdgeIn, {0, 4, 1, 3, 2}},
    };
    for (const auto& [order, expected] : cases) {
        EXPECT_EQ(TextAnimatorEngine::createOrderMap(5, order), expected);
    }
    EXPECT_TRUE(TextAnimatorEngine::createOrderMap(0, SelectorOrder::CenterOut).empty());
    EXPECT_TRUE(TextAnimatorEngine::createOrderMap(-1, SelectorOrder::EdgeIn).empty());
}

TEST(TextAnimatorContractTest, LogicalAndVisualOrdersSelectDifferentGlyphForTheSameRank)
{
    auto visualGlyphs = makeGlyphs(QStringLiteral("abc"));
    std::swap(visualGlyphs[0], visualGlyphs[2]);
    RangeSelector selector;
    selector.units = SelectorUnits::Index;
    selector.start = 0.0f;
    selector.end = 0.0f;
    selector.shape = SelectorShape::Square;

    const SelectorEvaluationContext logicalContext{
        QStringLiteral("abc"), visualGlyphs, TextSelectorOrder::Logical};
    const SelectorEvaluationContext visualContext{
        QStringLiteral("abc"), visualGlyphs, TextSelectorOrder::Visual};
    const auto logical = TextAnimatorEngine::evaluateSelector(logicalContext, selector);
    const auto visual = TextAnimatorEngine::evaluateSelector(visualContext, selector);

    ASSERT_EQ(logical.weights.size(), 3);
    ASSERT_EQ(visual.weights.size(), 3);
    EXPECT_FLOAT_EQ(logical.weights[0], 0.0f);
    EXPECT_FLOAT_EQ(logical.weights[1], 0.0f);
    EXPECT_FLOAT_EQ(logical.weights[2], 1.0f);
    EXPECT_FLOAT_EQ(visual.weights[0], 1.0f);
    EXPECT_FLOAT_EQ(visual.weights[1], 0.0f);
    EXPECT_FLOAT_EQ(visual.weights[2], 0.0f);
}

TEST(TextAnimatorContractTest, WigglyWeightIsRepeatableAndAlwaysFiniteAndBounded)
{
    WigglySelector selector;
    selector.enabled = true;
    selector.wigglesPerSecond = 2.5f;
    selector.correlation = 35.0f;
    selector.phase = 0.17f;
    selector.seed = 7821;

    const float first = TextAnimatorEngine::calculateWigglyWeight(7, 1.375f, selector);
    const float repeat = TextAnimatorEngine::calculateWigglyWeight(7, 1.375f, selector);
    EXPECT_FLOAT_EQ(first, repeat);
    EXPECT_TRUE(std::isfinite(first));
    EXPECT_GE(first, 0.0f);
    EXPECT_LE(first, 1.0f);

    selector.wigglesPerSecond = std::numeric_limits<float>::infinity();
    selector.correlation = std::numeric_limits<float>::quiet_NaN();
    selector.phase = std::numeric_limits<float>::infinity();
    const float sanitized = TextAnimatorEngine::calculateWigglyWeight(
        7, std::numeric_limits<float>::quiet_NaN(), selector);
    EXPECT_TRUE(std::isfinite(sanitized));
    EXPECT_GE(sanitized, 0.0f);
    EXPECT_LE(sanitized, 1.0f);
}

TEST(TextAnimatorContractTest, WigglyChangesWithTimeButFreezesAtZeroRate)
{
    WigglySelector selector;
    selector.enabled = true;
    selector.wigglesPerSecond = 1.0f;
    selector.correlation = 100.0f;
    selector.phase = 0.0f;
    selector.seed = 17;

    const float first = TextAnimatorEngine::calculateWigglyWeight(0, 0.0f, selector);
    const float nextTick = TextAnimatorEngine::calculateWigglyWeight(0, 1.0f, selector);
    EXPECT_NE(first, nextTick);

    selector.wigglesPerSecond = 0.0f;
    const float frozen = TextAnimatorEngine::calculateWigglyWeight(0, 0.0f, selector);
    const float later = TextAnimatorEngine::calculateWigglyWeight(0, 123.5f, selector);
    EXPECT_FLOAT_EQ(frozen, later);
}

TEST(TextAnimatorContractTest, FullWigglyCorrelationSharesOneSampleAcrossGlyphs)
{
    WigglySelector selector;
    selector.enabled = true;
    selector.wigglesPerSecond = 1.25f;
    selector.correlation = 100.0f;
    selector.seed = 12345;

    const float first = TextAnimatorEngine::calculateWigglyWeight(0, 2.375f, selector);
    const float second = TextAnimatorEngine::calculateWigglyWeight(99, 2.375f, selector);

    EXPECT_FLOAT_EQ(first, second);
}

TEST(TextAnimatorContractTest, AnimatorWeightCombineModesMatchExpectedValues)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    AnimatorSelectorSet set;
    set.range.shape = SelectorShape::RampUp;
    set.expression.enabled = true;
    set.expression.expression = QStringLiteral("textIndex / textTotal");
    const SelectorCombineMode modes[] = {
        SelectorCombineMode::Multiply, SelectorCombineMode::Add,
        SelectorCombineMode::Subtract, SelectorCombineMode::Min,
        SelectorCombineMode::Max,
    };
    const float expected[][3] = {
        {0.0f, 1.0f / 3.0f, 1.0f},
        {1.0f / 3.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.5f, 1.0f},
        {1.0f / 3.0f, 2.0f / 3.0f, 1.0f},
    };
    const float extraWeights[] = {0.5f, 1.0f, 0.25f};

    for (size_t modeIndex = 0; modeIndex < std::size(modes); ++modeIndex) {
        set.combine = modes[modeIndex];
        const auto weights = TextAnimatorEngine::evaluateAnimatorWeights(
            context, set, extraWeights);
        ASSERT_EQ(weights.size(), 3u);
        for (size_t glyphIndex = 0; glyphIndex < weights.size(); ++glyphIndex) {
            EXPECT_NEAR(weights[glyphIndex],
                        expected[modeIndex][glyphIndex] * extraWeights[glyphIndex],
                        1e-6f)
                << "mode=" << modeIndex << " glyph=" << glyphIndex;
        }
    }
}

TEST(TextAnimatorContractTest, AnimatorChangesOnlyGlyphsSelectedByTheRange)
{
    auto glyphs = makeGlyphs(QStringLiteral("abc"));
    RangeSelector selector;
    selector.units = SelectorUnits::Index;
    selector.start = 1.0f;
    selector.end = 1.0f;
    selector.shape = SelectorShape::Square;
    AnimatorProperties properties;
    properties.position = QPointF(8.0, -2.0);
    properties.opacity = 0.25f;

    TextAnimatorEngine::applyAnimator(
        glyphs, selector, WigglySelector{}, properties, 0.0f);

    ASSERT_EQ(glyphs.size(), 3u);
    EXPECT_FLOAT_EQ(glyphs[0].offsetPosition.x(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetPosition.y(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetOpacity, 1.0f);
    EXPECT_FLOAT_EQ(glyphs[1].offsetPosition.x(), 8.0f);
    EXPECT_FLOAT_EQ(glyphs[1].offsetPosition.y(), -2.0f);
    EXPECT_FLOAT_EQ(glyphs[1].offsetOpacity, 0.25f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetPosition.x(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetOpacity, 1.0f);
}

TEST(TextAnimatorContractTest, AnimatorStackAccumulatesSelectedTransformsAndOpacity)
{
    auto glyphs = makeGlyphs(QStringLiteral("abc"));
    RangeSelector selector;
    selector.units = SelectorUnits::Index;
    selector.start = 1.0f;
    selector.end = 1.0f;
    selector.shape = SelectorShape::Square;
    AnimatorProperties first;
    first.position = QPointF(2.0, 0.0);
    first.opacity = 0.5f;
    AnimatorProperties second;
    second.position = QPointF(0.0, 3.0);
    second.opacity = 0.5f;
    const std::vector<std::tuple<RangeSelector, WigglySelector, AnimatorProperties>> stack{
        {selector, WigglySelector{}, first},
        {selector, WigglySelector{}, second},
    };

    TextAnimatorEngine::applyAnimatorStack(glyphs, stack, 0.0f);

    EXPECT_FLOAT_EQ(glyphs[0].offsetPosition.x(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetPosition.y(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetOpacity, 1.0f);
    EXPECT_FLOAT_EQ(glyphs[1].offsetPosition.x(), 2.0f);
    EXPECT_FLOAT_EQ(glyphs[1].offsetPosition.y(), 3.0f);
    EXPECT_FLOAT_EQ(glyphs[1].offsetOpacity, 0.25f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetPosition.x(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetPosition.y(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetOpacity, 1.0f);
}

TEST(TextAnimatorContractTest, AnimatorAppliesEveryAuthoredGlyphPropertyChannel)
{
    auto glyphs = makeGlyphs(QStringLiteral("ab"));
    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    AnimatorProperties properties;
    properties.position = QPointF(3.0, -2.0);
    properties.scaleX = 2.0f;
    properties.scaleY = 0.5f;
    properties.rotation = 15.0f;
    properties.opacity = 0.25f;
    properties.skew = 5.0f;
    properties.z = 7.0f;
    properties.tracking = 4.0f;
    properties.colorEnabled = true;
    properties.fillColor = FloatRGBA(1.0f, 0.0f, 0.0f, 1.0f);
    properties.strokeEnabled = true;
    properties.strokeColor = FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f);
    properties.strokeWidth = 2.0f;
    properties.blur = 6.0f;

    TextAnimatorEngine::applyAnimator(glyphs, selector, WigglySelector{},
                                      properties, 0.0f);

    ASSERT_EQ(glyphs.size(), 2u);
    EXPECT_FLOAT_EQ(glyphs[0].offsetPosition.x(), 3.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetPosition.y(), -2.0f);
    EXPECT_FLOAT_EQ(glyphs[1].offsetPosition.x(), 7.0f);
    EXPECT_FLOAT_EQ(glyphs[1].offsetPosition.y(), -2.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetScaleX, 2.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetScaleY, 0.5f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetScale, 1.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetRotation, 15.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetOpacity, 0.25f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetSkew, 5.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetZ, 7.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetStrokeWidth, 2.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetBlur, 6.0f);
    EXPECT_TRUE(glyphs[0].hasColorOverride);
    EXPECT_EQ(glyphs[0].fillColorOverride, properties.fillColor);
    EXPECT_FLOAT_EQ(glyphs[0].fillColorOverrideWeight, 1.0f);
    EXPECT_TRUE(glyphs[0].hasStrokeOverride);
    EXPECT_EQ(glyphs[0].strokeColorOverride, properties.strokeColor);
    EXPECT_FLOAT_EQ(glyphs[0].strokeColorOverrideWeight, 1.0f);
}

TEST(TextAnimatorContractTest, EmptyGlyphDomainReturnsDiagnosticWithoutWeights)
{
    const std::vector<GlyphItem> glyphs;
    const SelectorEvaluationContext context{
        QString(), glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    EXPECT_TRUE(result.weights.isEmpty());
    EXPECT_EQ(result.diagnostic, QStringLiteral("empty glyph domain"));
}
