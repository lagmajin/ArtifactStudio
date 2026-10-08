#include <gtest/gtest.h>

#include <QPointF>
#include <QRectF>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

import Text.Animator;
import Text.GlyphLayout;
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
    EXPECT_EQ(result.diagnostic,
              QStringLiteral("range evaluated in logical glyph order"));
}

TEST(TextAnimatorContractTest, LegacyWeightHelpersRespectDomainBoundaries)
{
    RangeSelector selector;
    selector.units = SelectorUnits::Percentage;
    selector.shape = SelectorShape::Square;
    selector.start = 0.0f;
    selector.end = 100.0f;

    EXPECT_FLOAT_EQ(TextAnimatorEngine::calculateWeight(0, 3, selector), 1.0f);
    EXPECT_FLOAT_EQ(TextAnimatorEngine::calculateWeight(2, 3, selector), 1.0f);
    EXPECT_FLOAT_EQ(TextAnimatorEngine::calculateWeight(3, 3, selector), 0.0f);
    EXPECT_FLOAT_EQ(TextAnimatorEngine::calculateWeight(0, 0, selector), 0.0f);

    auto glyphs = makeGlyphs(QStringLiteral("abc"));
    selector.units = SelectorUnits::Cluster;
    selector.start = 1.0f;
    selector.end = 1.0f;
    EXPECT_FLOAT_EQ(TextAnimatorEngine::calculateWeightForGlyph(
                        glyphs[1], 1, 3, 3, 1, 0, 1, selector),
                    1.0f);
    EXPECT_FLOAT_EQ(TextAnimatorEngine::calculateWeightForGlyph(
                        glyphs[0], 0, 3, 3, 1, 0, 1, selector),
                    0.0f);
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

TEST(TextAnimatorContractTest, SelectorWeightsStayFiniteAndBoundedAcrossRangeMatrix)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abcdefghij"));
    const SelectorEvaluationContext context{
        QStringLiteral("abcdefghij"), glyphs, TextSelectorOrder::Logical};
    const SelectorShape shapes[] = {
        SelectorShape::Square, SelectorShape::RampUp,
        SelectorShape::RampDown, SelectorShape::Triangle,
        SelectorShape::Round, SelectorShape::Smooth,
    };
    const float boundaries[] = {
        -100.0f, -1.0f, 0.0f, 0.125f, 1.0f, 2.5f, 5.0f,
        9.0f, 10.0f, 50.0f, 99.0f, 100.0f, 101.0f, 250.0f,
    };
    const float offsets[] = {
        -150.0f, -10.0f, -1.0f, 0.0f, 1.0f, 10.0f, 150.0f,
    };

    for (const auto shape : shapes) {
        for (const float start : boundaries) {
            for (const float end : boundaries) {
                for (const float offset : offsets) {
                    RangeSelector selector;
                    selector.units = SelectorUnits::Index;
                    selector.shape = shape;
                    selector.start = start;
                    selector.end = end;
                    selector.offset = offset;
                    selector.easeHigh = 4.0f;
                    selector.easeLow = 2.0f;

                    const auto result = TextAnimatorEngine::evaluateSelector(
                        context, selector);
                    ASSERT_EQ(result.weights.size(), glyphs.size());
                    for (qsizetype glyphIndex = 0;
                         glyphIndex < result.weights.size(); ++glyphIndex) {
                        const float weight = result.weights[glyphIndex];
                        EXPECT_TRUE(std::isfinite(weight))
                            << "shape=" << static_cast<int>(shape)
                            << " start=" << start << " end=" << end
                            << " offset=" << offset
                            << " glyph=" << glyphIndex;
                        EXPECT_GE(weight, 0.0f)
                            << "shape=" << static_cast<int>(shape)
                            << " start=" << start << " end=" << end
                            << " offset=" << offset
                            << " glyph=" << glyphIndex;
                        EXPECT_LE(weight, 1.0f)
                            << "shape=" << static_cast<int>(shape)
                            << " start=" << start << " end=" << end
                            << " offset=" << offset
                            << " glyph=" << glyphIndex;
                    }
                }
            }
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

TEST(TextAnimatorContractTest, RegexMatchExpandsAcrossCombiningGraphemeCluster)
{
    const QString source = QString::fromUtf8("e\xCC\x81x");
    std::vector<GlyphItem> glyphs(3);
    const int logicalIndices[] = {0, 1, 2};
    const int clusterIndices[] = {0, 0, 1};
    const char32_t codepoints[] = {'e', 0x0301, 'x'};
    for (size_t index = 0; index < glyphs.size(); ++index) {
        glyphs[index].index = logicalIndices[index];
        glyphs[index].clusterIndex = clusterIndices[index];
        glyphs[index].lineIndex = 0;
        glyphs[index].paragraphIndex = 0;
        glyphs[index].charCode = codepoints[index];
        glyphs[index].clusterId = QString::number(clusterIndices[index]);
    }
    const SelectorEvaluationContext context{
        source, glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.regexEnabled = true;
    selector.selectorPattern = QString::fromUtf8("\xCC\x81");

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    EXPECT_FLOAT_EQ(result.weights[0], 1.0f);
    EXPECT_FLOAT_EQ(result.weights[1], 1.0f);
    EXPECT_FLOAT_EQ(result.weights[2], 0.0f);
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

TEST(TextAnimatorContractTest, RampShapeProgressesAcrossClusterLineAndTagDomains)
{
    auto glyphs = makeGlyphs(QStringLiteral("abcd"));
    const int clusterIndices[] = {0, 0, 1, 2};
    const int lineIndices[] = {0, 0, 1, 2};
    const QString tags[] = {
        QStringLiteral("latin"), QStringLiteral("latin"),
        QStringLiteral("cjk"), QStringLiteral("emoji"),
    };
    for (size_t index = 0; index < glyphs.size(); ++index) {
        glyphs[index].clusterIndex = clusterIndices[index];
        glyphs[index].lineIndex = lineIndices[index];
        glyphs[index].selectorTag = tags[index];
    }
    const SelectorEvaluationContext context{
        QStringLiteral("abcd"), glyphs, TextSelectorOrder::Logical};
    const SelectorUnits units[] = {
        SelectorUnits::Percentage, SelectorUnits::Cluster,
        SelectorUnits::Line, SelectorUnits::Tag,
    };
    const float expected[] = {0.0f, 0.0f, 0.5f, 1.0f};

    for (const auto unit : units) {
        RangeSelector selector;
        selector.units = unit;
        selector.start = 0.0f;
        selector.end = unit == SelectorUnits::Percentage ? 100.0f : 2.0f;
        selector.shape = SelectorShape::RampUp;

        const auto result = TextAnimatorEngine::evaluateSelector(
            context, selector);

        ASSERT_EQ(result.weights.size(), glyphs.size());
        for (size_t index = 0; index < glyphs.size(); ++index) {
            EXPECT_FLOAT_EQ(result.weights[static_cast<qsizetype>(index)],
                            expected[index])
                << "units=" << static_cast<int>(unit)
                << " glyph=" << index;
        }
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
    set.range.start = 1.0f;
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
    EXPECT_FLOAT_EQ(weights[3], 1.0f);  // (1 + 1) * missing-extra default, clamp
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

TEST(TextAnimatorContractTest, NonFiniteBaseWeightZerosWholeExpressionResult)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    ExpressionSelector selector;
    selector.enabled = true;
    selector.expression = QStringLiteral("selectorValue + 0.25");
    const float baseWeights[] = {
        0.25f, std::numeric_limits<float>::quiet_NaN(), 0.5f};

    const auto result = TextAnimatorEngine::evaluateExpressionSelector(
        context, selector, baseWeights);

    ASSERT_EQ(result.weights.size(), 3);
    EXPECT_TRUE(result.diagnostic.contains(
        QStringLiteral("expression selector must return a finite number")));
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

TEST(TextAnimatorContractTest, ExpressionReceivesSeedExplicitTotalAndBaseWeight)
{
    const auto glyphs = makeGlyphs(QStringLiteral("ab"));
    const SelectorEvaluationContext context{
        QStringLiteral("ab"), glyphs, TextSelectorOrder::Logical,
        0, 4, 0.0f};
    ExpressionSelector selector;
    selector.enabled = true;
    selector.seed = 1;
    selector.expression = QStringLiteral("seed / textTotal + selectorValue");
    const float baseWeights[] = {0.25f, 0.5f};

    const auto result = TextAnimatorEngine::evaluateExpressionSelector(
        context, selector, baseWeights);

    ASSERT_EQ(result.weights.size(), 2);
    EXPECT_FLOAT_EQ(result.weights[0], 0.5f);
    EXPECT_FLOAT_EQ(result.weights[1], 0.75f);
    EXPECT_TRUE(result.diagnostic.isEmpty());

}

TEST(TextAnimatorContractTest, ExpressionFallsBackForNonPositiveTextTotal)
{
    const auto glyphs = makeGlyphs(QStringLiteral("ab"));
    ExpressionSelector selector;
    selector.enabled = true;
    selector.expression = QStringLiteral("1 / textTotal");

    for (const int textTotal : {0, -3}) {
        const SelectorEvaluationContext context{
            QStringLiteral("ab"), glyphs, TextSelectorOrder::Logical,
            0, textTotal, 0.0f};
        const auto result = TextAnimatorEngine::evaluateExpressionSelector(
            context, selector);

        ASSERT_EQ(result.weights.size(), 2);
        EXPECT_FLOAT_EQ(result.weights[0], 0.5f);
        EXPECT_FLOAT_EQ(result.weights[1], 0.5f);
        EXPECT_TRUE(result.diagnostic.isEmpty());
    }
}

TEST(TextAnimatorContractTest, ExpressionReceivesSourceText)
{
    const auto glyphs = makeGlyphs(QStringLiteral("ab"));
    const SelectorEvaluationContext context{
        QStringLiteral("ab"), glyphs, TextSelectorOrder::Logical};
    ExpressionSelector selector;
    selector.enabled = true;
    selector.expression = QStringLiteral("text == \"ab\" ? 0.75 : 0.25");

    const auto result = TextAnimatorEngine::evaluateExpressionSelector(
        context, selector);

    ASSERT_EQ(result.weights.size(), 2);
    EXPECT_FLOAT_EQ(result.weights[0], 0.75f);
    EXPECT_FLOAT_EQ(result.weights[1], 0.75f);
    EXPECT_TRUE(result.diagnostic.isEmpty());
}

TEST(TextAnimatorContractTest, ExpressionPreservesJapaneseSourceText)
{
    const auto glyphs = makeGlyphs(QStringLiteral("日本語"));
    const SelectorEvaluationContext context{
        QStringLiteral("日本語"), glyphs, TextSelectorOrder::Logical};
    ExpressionSelector selector;
    selector.enabled = true;
    selector.expression = QStringLiteral("text == \"日本語\" ? 1 : 0");

    const auto result = TextAnimatorEngine::evaluateExpressionSelector(
        context, selector);

    ASSERT_EQ(result.weights.size(), 3);
    for (const float weight : result.weights) {
        EXPECT_FLOAT_EQ(weight, 1.0f);
    }
    EXPECT_TRUE(result.diagnostic.isEmpty());
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

TEST(TextAnimatorContractTest, EmptyExpressionReportsFailureAndSelectsNothing)
{
    const auto glyphs = makeGlyphs(QStringLiteral("ab"));
    const SelectorEvaluationContext context{
        QStringLiteral("ab"), glyphs, TextSelectorOrder::Logical};
    ExpressionSelector selector;
    selector.enabled = true;
    selector.expression = QStringLiteral("  \t ");

    const auto result = TextAnimatorEngine::evaluateExpressionSelector(
        context, selector);

    ASSERT_EQ(result.weights.size(), 2);
    EXPECT_TRUE(result.diagnostic.contains(QStringLiteral("empty")));
    EXPECT_FLOAT_EQ(result.weights[0], 0.0f);
    EXPECT_FLOAT_EQ(result.weights[1], 0.0f);
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

TEST(TextAnimatorContractTest, SelectorOrderRanksDriveGlyphEvaluation)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abcde"));
    const SelectorEvaluationContext context{
        QStringLiteral("abcde"), glyphs, TextSelectorOrder::Logical};
    const SelectorOrder orders[] = {
        SelectorOrder::Natural, SelectorOrder::LeftToRight,
        SelectorOrder::Reverse, SelectorOrder::RightToLeft,
        SelectorOrder::RandomStable, SelectorOrder::CenterOut,
        SelectorOrder::EdgeIn,
    };

    for (const auto order : orders) {
        const auto orderMap = TextAnimatorEngine::createOrderMap(5, order);
        ASSERT_EQ(orderMap.size(), glyphs.size());
        for (int rank = 0; rank < 2; ++rank) {
            RangeSelector selector;
            selector.units = SelectorUnits::Index;
            selector.start = static_cast<float>(rank);
            selector.end = static_cast<float>(rank);
            selector.shape = SelectorShape::Square;
            selector.order = order;

            const auto result = TextAnimatorEngine::evaluateSelector(
                context, selector);

            ASSERT_EQ(result.weights.size(), glyphs.size());
            for (size_t glyphIndex = 0; glyphIndex < glyphs.size(); ++glyphIndex) {
                const float expected =
                    orderMap[static_cast<size_t>(rank)] ==
                            static_cast<int>(glyphIndex)
                        ? 1.0f
                        : 0.0f;
                EXPECT_FLOAT_EQ(result.weights[static_cast<qsizetype>(glyphIndex)],
                                expected)
                    << "order=" << static_cast<int>(order)
                    << " rank=" << rank << " glyph=" << glyphIndex;
            }
        }
    }
}

TEST(TextAnimatorContractTest, ReverseOrderSelectsWholeGlyphClusters)
{
    auto glyphs = makeGlyphs(QStringLiteral("abcde"));
    const int clusterIndices[] = {0, 0, 1, 2, 2};
    for (size_t index = 0; index < glyphs.size(); ++index) {
        glyphs[index].clusterIndex = clusterIndices[index];
    }
    const SelectorEvaluationContext context{
        QStringLiteral("abcde"), glyphs, TextSelectorOrder::Logical};
    RangeSelector selector;
    selector.units = SelectorUnits::Cluster;
    selector.order = SelectorOrder::Reverse;
    selector.start = 0.0f;
    selector.end = 0.0f;
    selector.shape = SelectorShape::Square;

    const auto result = TextAnimatorEngine::evaluateSelector(context, selector);

    ASSERT_EQ(result.weights.size(), glyphs.size());
    const float expected[] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    for (size_t index = 0; index < glyphs.size(); ++index) {
        EXPECT_FLOAT_EQ(result.weights[static_cast<qsizetype>(index)],
                        expected[index]);
    }
}

TEST(TextAnimatorContractTest, ReverseOrderUsesEachSelectorUnitDomain)
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
    struct UnitExpectation {
        SelectorUnits units;
        std::array<float, 5> weights;
    };
    const UnitExpectation cases[] = {
        {SelectorUnits::Percentage, {0.0f, 0.0f, 0.0f, 1.0f, 1.0f}},
        {SelectorUnits::Index, {0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
        {SelectorUnits::Cluster, {0.0f, 0.0f, 0.0f, 1.0f, 1.0f}},
        {SelectorUnits::Line, {0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
        {SelectorUnits::Tag, {0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
    };

    for (const auto& testCase : cases) {
        RangeSelector selector;
        selector.units = testCase.units;
        selector.order = SelectorOrder::Reverse;
        selector.start = 0.0f;
        selector.end = 0.0f;
        selector.shape = SelectorShape::Square;

        const auto result = TextAnimatorEngine::evaluateSelector(
            context, selector);

        ASSERT_EQ(result.weights.size(), glyphs.size());
        for (size_t index = 0; index < glyphs.size(); ++index) {
            EXPECT_FLOAT_EQ(result.weights[static_cast<qsizetype>(index)],
                            testCase.weights[index])
                << "units=" << static_cast<int>(testCase.units)
                << " glyph=" << index;
        }
    }
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
    EXPECT_EQ(logical.units, SelectorUnits::Index);
    EXPECT_EQ(logical.order, TextSelectorOrder::Logical);
    EXPECT_EQ(visual.order, TextSelectorOrder::Visual);
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

    selector.seed += 1;
    const float differentSeed =
        TextAnimatorEngine::calculateWigglyWeight(7, 1.375f, selector);
    EXPECT_NE(first, differentSeed);

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
        {1.0f / 3.0f, 1.0f, 2.0f},
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
        auto animatedGlyphs = glyphs;
        set.properties.opacity = 0.0f;
        const std::vector<AnimatorSelectorSet> sets{set};
        TextAnimatorEngine::applyAnimatorSets(
            animatedGlyphs, sets, 0.0f, QStringLiteral("abc"), extraWeights);
        for (size_t glyphIndex = 0; glyphIndex < weights.size(); ++glyphIndex) {
            const float expectedWeight =
                expected[modeIndex][glyphIndex] * extraWeights[glyphIndex];
            EXPECT_NEAR(weights[glyphIndex],
                        expectedWeight,
                        1e-6f)
                << "mode=" << modeIndex << " glyph=" << glyphIndex;
            EXPECT_NEAR(animatedGlyphs[glyphIndex].offsetOpacity,
                        1.0f - expectedWeight, 1e-6f)
                << "applied mode=" << modeIndex << " glyph=" << glyphIndex;
        }
    }
}

TEST(TextAnimatorContractTest, EvaluateAnimatorWeightsReplacesOutputBuffer)
{
    const auto glyphs = makeGlyphs(QStringLiteral("abc"));
    const SelectorEvaluationContext context{
        QStringLiteral("abc"), glyphs, TextSelectorOrder::Logical};
    AnimatorSelectorSet set;
    set.range.units = SelectorUnits::Index;
    set.range.start = 1.0f;
    set.range.end = 1.0f;
    set.range.shape = SelectorShape::Square;
    std::vector<float> weights{0.9f, 0.8f, 0.7f, 0.6f};

    TextAnimatorEngine::evaluateAnimatorWeights(context, set, {}, weights,
                                                nullptr);

    ASSERT_EQ(weights.size(), 3u);
    EXPECT_FLOAT_EQ(weights[0], 0.0f);
    EXPECT_FLOAT_EQ(weights[1], 1.0f);
    EXPECT_FLOAT_EQ(weights[2], 0.0f);
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

TEST(TextAnimatorContractTest, SourceAwareLegacyStackSupportsRegexAndExpression)
{
    auto regexGlyphs = makeGlyphs(QStringLiteral("abc"));
    RangeSelector regexRange;
    regexRange.regexEnabled = true;
    regexRange.selectorPattern = QStringLiteral("b");
    AnimatorProperties position;
    position.position = QPointF(6.0, -3.0);
    const std::vector<std::tuple<RangeSelector, WigglySelector,
                                 AnimatorProperties>> rangeStack{
        {regexRange, WigglySelector{}, position}};

    TextAnimatorEngine::applyAnimatorStack(
        regexGlyphs, rangeStack, 0.0f, QStringLiteral("abc"));

    EXPECT_EQ(regexGlyphs[0].offsetPosition, QPointF(0.0, 0.0));
    EXPECT_EQ(regexGlyphs[1].offsetPosition, QPointF(6.0, -3.0));
    EXPECT_EQ(regexGlyphs[2].offsetPosition, QPointF(0.0, 0.0));

    auto expressionGlyphs = makeGlyphs(QStringLiteral("abc"));
    RangeSelector fullRange;
    fullRange.shape = SelectorShape::Square;
    ExpressionSelector expression;
    expression.enabled = true;
    expression.expression = QStringLiteral("textIndex / textTotal");
    AnimatorProperties opacity;
    opacity.opacity = 0.0f;
    const std::vector<std::tuple<RangeSelector, WigglySelector,
                                 ExpressionSelector, AnimatorProperties>>
        expressionStack{{fullRange, WigglySelector{}, expression, opacity}};

    TextAnimatorEngine::applyAnimatorStack(
        expressionGlyphs, expressionStack, 0.0f, QStringLiteral("abc"));

    EXPECT_NEAR(expressionGlyphs[0].offsetOpacity, 2.0f / 3.0f, 1e-6f);
    EXPECT_NEAR(expressionGlyphs[1].offsetOpacity, 1.0f / 3.0f, 1e-6f);
    EXPECT_FLOAT_EQ(expressionGlyphs[2].offsetOpacity, 0.0f);
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

TEST(TextAnimatorContractTest, FractionalWeightInterpolatesEveryTransformChannel)
{
    auto glyphs = makeGlyphs(QStringLiteral("ab"));
    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    AnimatorProperties properties;
    properties.position = QPointF(8.0, -4.0);
    properties.scaleX = 3.0f;
    properties.scaleY = 0.25f;
    properties.rotation = 40.0f;
    properties.opacity = 0.2f;
    properties.skew = 12.0f;
    properties.z = 8.0f;
    properties.tracking = 2.0f;
    properties.colorEnabled = true;
    properties.fillColor = FloatRGBA(1.0f, 0.0f, 0.0f, 1.0f);
    properties.strokeEnabled = true;
    properties.strokeColor = FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f);
    properties.strokeWidth = 2.0f;
    properties.blur = 6.0f;
    const float weights[] = {0.25f, 0.25f};

    TextAnimatorEngine::applyAnimator(glyphs, selector, WigglySelector{},
                                      properties, 0.0f, weights);

    ASSERT_EQ(glyphs.size(), 2u);
    EXPECT_NEAR(glyphs[0].offsetPosition.x(), 2.0f, 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetPosition.y(), -1.0f, 1e-6f);
    EXPECT_NEAR(glyphs[1].offsetPosition.x(), 2.5f, 1e-6f);
    EXPECT_NEAR(glyphs[1].offsetPosition.y(), -1.0f, 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetScaleX, 1.5f, 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetScaleY, 0.8125f, 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetScale,
                std::sqrt(1.5f * 0.8125f), 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetRotation, 10.0f, 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetOpacity, 0.8f, 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetSkew, 3.0f, 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetZ, 2.0f, 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetStrokeWidth, 0.5f, 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetBlur, 1.5f, 1e-6f);
    EXPECT_TRUE(glyphs[0].hasColorOverride);
    EXPECT_EQ(glyphs[0].fillColorOverride, properties.fillColor);
    EXPECT_NEAR(glyphs[0].fillColorOverrideWeight, 0.25f, 1e-6f);
    EXPECT_TRUE(glyphs[0].hasStrokeOverride);
    EXPECT_EQ(glyphs[0].strokeColorOverride, properties.strokeColor);
    EXPECT_NEAR(glyphs[0].strokeColorOverrideWeight, 0.25f, 1e-6f);
}

TEST(TextAnimatorContractTest, UniformScaleKeepsLegacyAxisScaleBehavior)
{
    auto glyphs = makeGlyphs(QStringLiteral("a"));
    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    AnimatorProperties properties;
    properties.scale = 1.75f;

    TextAnimatorEngine::applyAnimator(glyphs, selector, WigglySelector{},
                                      properties, 0.0f);

    ASSERT_EQ(glyphs.size(), 1u);
    EXPECT_FLOAT_EQ(glyphs[0].offsetScale, 1.75f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetScaleX, 1.75f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetScaleY, 1.75f);
}

TEST(TextAnimatorContractTest, UnauthoredScaleAxisFallsBackToUniformScale)
{
    auto glyphs = makeGlyphs(QStringLiteral("a"));
    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    AnimatorProperties properties;
    properties.scale = 3.0f;
    properties.scaleX = 2.0f;

    TextAnimatorEngine::applyAnimator(glyphs, selector, WigglySelector{},
                                      properties, 0.0f);

    ASSERT_EQ(glyphs.size(), 1u);
    EXPECT_FLOAT_EQ(glyphs[0].offsetScaleX, 2.0f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetScaleY, 3.0f);
    EXPECT_NEAR(glyphs[0].offsetScale, std::sqrt(6.0f), 1e-6f);
}

TEST(TextAnimatorContractTest, WigglyWeightModulatesAppliedTransform)
{
    auto glyphs = makeGlyphs(QStringLiteral("ab"));
    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    WigglySelector wiggly;
    wiggly.enabled = true;
    wiggly.wigglesPerSecond = 1.25f;
    wiggly.correlation = 35.0f;
    wiggly.phase = 0.2f;
    wiggly.seed = 812;
    constexpr float time = 0.375f;
    AnimatorProperties properties;
    properties.position = QPointF(8.0, -4.0);

    const float weight = TextAnimatorEngine::calculateWigglyWeight(
        glyphs[0].clusterIndex, time, wiggly);
    TextAnimatorEngine::applyAnimator(glyphs, selector, wiggly,
                                      properties, time);

    EXPECT_NEAR(glyphs[0].offsetPosition.x(), 8.0f * weight, 1e-6f);
    EXPECT_NEAR(glyphs[0].offsetPosition.y(), -4.0f * weight, 1e-6f);
    EXPECT_NEAR(glyphs[1].offsetPosition.x(),
                8.0f * TextAnimatorEngine::calculateWigglyWeight(
                           glyphs[1].clusterIndex, time, wiggly),
                1e-6f);
}

TEST(TextAnimatorContractTest, AnimatorSetStackBlendsFillAndStrokeOverrides)
{
    auto glyphs = makeGlyphs(QStringLiteral("a"));
    AnimatorSelectorSet red;
    red.range.shape = SelectorShape::Square;
    red.properties.colorEnabled = true;
    red.properties.fillColor = FloatRGBA(1.0f, 0.0f, 0.0f, 1.0f);
    red.properties.strokeEnabled = true;
    red.properties.strokeColor = FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f);
    red.properties.strokeWidth = 2.0f;
    AnimatorSelectorSet blue = red;
    blue.properties.fillColor = FloatRGBA(0.0f, 0.0f, 1.0f, 1.0f);
    blue.properties.strokeColor = FloatRGBA(1.0f, 1.0f, 0.0f, 1.0f);
    blue.properties.strokeWidth = 6.0f;
    const std::vector<AnimatorSelectorSet> sets{red, blue};
    const float weights[] = {0.25f};

    TextAnimatorEngine::applyAnimatorSets(
        glyphs, sets, 0.0f, QStringLiteral("a"), weights);

    ASSERT_EQ(glyphs.size(), 1u);
    EXPECT_TRUE(glyphs[0].hasColorOverride);
    EXPECT_NEAR(glyphs[0].fillColorOverride.r(), 3.0f / 7.0f, 1e-6f);
    EXPECT_FLOAT_EQ(glyphs[0].fillColorOverride.g(), 0.0f);
    EXPECT_NEAR(glyphs[0].fillColorOverride.b(), 4.0f / 7.0f, 1e-6f);
    EXPECT_FLOAT_EQ(glyphs[0].fillColorOverrideWeight, 0.4375f);
    EXPECT_TRUE(glyphs[0].hasStrokeOverride);
    EXPECT_NEAR(glyphs[0].strokeColorOverride.r(), 4.0f / 7.0f, 1e-6f);
    EXPECT_FLOAT_EQ(glyphs[0].strokeColorOverride.g(), 1.0f);
    EXPECT_FLOAT_EQ(glyphs[0].strokeColorOverride.b(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[0].strokeColorOverrideWeight, 0.4375f);
    EXPECT_FLOAT_EQ(glyphs[0].offsetStrokeWidth, 2.0f);
}

TEST(TextAnimatorContractTest, NamedAnimatorSetsApplyCombinedWeightsToGlyphs)
{
    auto glyphs = makeGlyphs(QStringLiteral("abc"));
    AnimatorSelectorSet set;
    set.range.units = SelectorUnits::Index;
    set.range.start = 0.0f;
    set.range.end = 2.0f;
    set.range.shape = SelectorShape::Square;
    set.expression.enabled = true;
    set.expression.expression = QStringLiteral("textIndex / textTotal");
    set.properties.opacity = 0.0f;
    const std::vector<AnimatorSelectorSet> sets{set};

    TextAnimatorEngine::applyAnimatorSets(
        glyphs, sets, 0.0f, QStringLiteral("abc"));

    ASSERT_EQ(glyphs.size(), 3u);
    EXPECT_NEAR(glyphs[0].offsetOpacity, 2.0f / 3.0f, 1e-6f);
    EXPECT_NEAR(glyphs[1].offsetOpacity, 1.0f / 3.0f, 1e-6f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetOpacity, 0.0f);
}

TEST(TextAnimatorContractTest, AnimatorSetWigglyWeightsAffectAppliedTransform)
{
    auto glyphs = makeGlyphs(QStringLiteral("ab"));
    AnimatorSelectorSet set;
    set.range.shape = SelectorShape::Square;
    set.wiggly.enabled = true;
    set.wiggly.wigglesPerSecond = 1.25f;
    set.wiggly.correlation = 35.0f;
    set.wiggly.phase = 0.2f;
    set.wiggly.seed = 812;
    set.properties.position = QPointF(8.0, -4.0);
    constexpr float time = 0.375f;
    const std::vector<AnimatorSelectorSet> sets{set};

    TextAnimatorEngine::applyAnimatorSets(
        glyphs, sets, time, QStringLiteral("ab"));

    ASSERT_EQ(glyphs.size(), 2u);
    for (size_t index = 0; index < glyphs.size(); ++index) {
        const float weight = TextAnimatorEngine::calculateWigglyWeight(
            glyphs[index].clusterIndex, time, set.wiggly);
        EXPECT_NEAR(glyphs[index].offsetPosition.x(), 8.0f * weight, 1e-6f);
        EXPECT_NEAR(glyphs[index].offsetPosition.y(), -4.0f * weight, 1e-6f);
    }
}

TEST(TextAnimatorContractTest, AnimatorSetsReportSelectorFailureWithoutMutation)
{
    auto glyphs = makeGlyphs(QStringLiteral("ab"));
    AnimatorSelectorSet set;
    set.range.regexEnabled = true;
    set.range.selectorPattern = QStringLiteral("(");
    set.properties.position = QPointF(12.0, -7.0);
    const std::vector<AnimatorSelectorSet> sets{set};
    QStringList diagnostics;

    TextAnimatorEngine::applyAnimatorSets(
        glyphs, sets, 0.0f, QStringLiteral("ab"), {}, diagnostics);

    ASSERT_EQ(diagnostics.size(), 1);
    EXPECT_TRUE(diagnostics.front().contains(QStringLiteral("invalid regex")));
    ASSERT_EQ(glyphs.size(), 2u);
    EXPECT_EQ(glyphs[0].offsetPosition, QPointF(0.0, 0.0));
    EXPECT_EQ(glyphs[1].offsetPosition, QPointF(0.0, 0.0));

    AnimatorSelectorSet validSet;
    validSet.range.shape = SelectorShape::Square;
    validSet.properties.position = QPointF(3.0, 0.0);
    const std::vector<AnimatorSelectorSet> validSets{validSet};
    TextAnimatorEngine::applyAnimatorSets(
        glyphs, validSets, 0.0f, QStringLiteral("ab"), {}, diagnostics);

    for (const QString& diagnostic : diagnostics) {
        EXPECT_FALSE(diagnostic.contains(QStringLiteral("invalid regex")));
    }
    EXPECT_EQ(glyphs[0].offsetPosition, QPointF(3.0, 0.0));
    EXPECT_EQ(glyphs[1].offsetPosition, QPointF(3.0, 0.0));
}

TEST(TextAnimatorContractTest, EmptyAnimatorStackClearsDiagnosticsWithoutMutation)
{
    auto glyphs = makeGlyphs(QStringLiteral("ab"));
    glyphs[0].offsetPosition = QPointF(2.0, 3.0);
    const auto originalGlyphs = glyphs;
    const std::vector<AnimatorSelectorSet> noSets;
    QStringList diagnostics{QStringLiteral("previous selector failure")};

    TextAnimatorEngine::applyAnimatorSets(
        glyphs, noSets, 0.0f, QStringLiteral("ab"), {}, diagnostics);

    EXPECT_TRUE(diagnostics.isEmpty());
    ASSERT_EQ(glyphs.size(), originalGlyphs.size());
    EXPECT_EQ(glyphs[0].offsetPosition, originalGlyphs[0].offsetPosition);
    EXPECT_EQ(glyphs[1].offsetPosition, originalGlyphs[1].offsetPosition);
}

TEST(TextAnimatorContractTest, AnimatorSetRegexDiagnosticsStayInSetOrder)
{
    auto first = AnimatorSelectorSet{};
    first.range.regexEnabled = true;
    first.range.selectorPattern = QStringLiteral("(");
    auto second = AnimatorSelectorSet{};
    second.range.regexEnabled = true;
    second.range.selectorPattern = QStringLiteral("[");
    const std::vector<AnimatorSelectorSet> sets{first, second};
    const auto glyphs = makeGlyphs(QStringLiteral("a"));
    QStringList diagnostics;
    for (const auto& set : sets) {
        std::vector<float> weights;
        TextAnimatorEngine::evaluateAnimatorWeights(
            SelectorEvaluationContext{
                QStringLiteral("a"), glyphs, TextSelectorOrder::Logical},
            set, {}, weights, &diagnostics);
    }

    ASSERT_EQ(diagnostics.size(), 2);
    EXPECT_TRUE(diagnostics[0].contains(QStringLiteral("invalid regex")));
    EXPECT_TRUE(diagnostics[1].contains(QStringLiteral("invalid regex")));
    EXPECT_NE(diagnostics[0], diagnostics[1]);
}

TEST(TextAnimatorContractTest, InvalidAnimatorSetDoesNotBlockLaterValidSets)
{
    auto glyphs = makeGlyphs(QStringLiteral("a"));
    AnimatorSelectorSet beforeFailure;
    beforeFailure.range.shape = SelectorShape::Square;
    beforeFailure.properties.position = QPointF(1.0, 0.0);

    AnimatorSelectorSet invalidRegex;
    invalidRegex.range.regexEnabled = true;
    invalidRegex.range.selectorPattern = QStringLiteral("(");
    invalidRegex.properties.position = QPointF(11.0, 0.0);

    AnimatorSelectorSet afterFailure;
    afterFailure.range.shape = SelectorShape::Square;
    afterFailure.properties.position = QPointF(2.0, 0.0);
    const std::vector<AnimatorSelectorSet> sets{
        beforeFailure, invalidRegex, afterFailure};

    TextAnimatorEngine::applyAnimatorSets(
        glyphs, sets, 0.0f, QStringLiteral("a"));

    ASSERT_EQ(glyphs.size(), 1u);
    EXPECT_EQ(glyphs[0].offsetPosition, QPointF(3.0, 0.0));
}

TEST(TextAnimatorContractTest,
     MalformedExpressionSetReportsAndDoesNotBlockLaterSets)
{
    auto glyphs = makeGlyphs(QStringLiteral("a"));
    AnimatorSelectorSet beforeFailure;
    beforeFailure.range.shape = SelectorShape::Square;
    beforeFailure.properties.position = QPointF(1.0, 0.0);

    AnimatorSelectorSet malformedExpression;
    malformedExpression.range.shape = SelectorShape::Square;
    malformedExpression.expression.enabled = true;
    malformedExpression.expression.expression = QStringLiteral("textIndex +");
    malformedExpression.properties.position = QPointF(11.0, 0.0);

    AnimatorSelectorSet afterFailure;
    afterFailure.range.shape = SelectorShape::Square;
    afterFailure.properties.position = QPointF(2.0, 0.0);
    const std::vector<AnimatorSelectorSet> sets{
        beforeFailure, malformedExpression, afterFailure};
    QStringList diagnostics;

    TextAnimatorEngine::applyAnimatorSets(
        glyphs, sets, 0.0f, QStringLiteral("a"), {}, diagnostics);

    ASSERT_FALSE(diagnostics.isEmpty());
    EXPECT_TRUE(std::any_of(
        diagnostics.cbegin(), diagnostics.cend(),
        [](const QString& diagnostic) {
            return diagnostic.contains(
                QStringLiteral("expression selector must return a finite number"));
        }));
    ASSERT_EQ(glyphs.size(), 1u);
    EXPECT_EQ(glyphs[0].offsetPosition, QPointF(3.0, 0.0));
}

TEST(TextAnimatorContractTest, AnchorGroupingChangesRotationPivot)
{
    auto characterAnchored = makeGlyphs(QStringLiteral("ab"));
    characterAnchored[0].bounds = QRectF(0.0, 0.0, 2.0, 2.0);
    characterAnchored[1].bounds = QRectF(10.0, 0.0, 2.0, 2.0);
    auto groupAnchored = characterAnchored;
    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    AnimatorProperties properties;
    properties.rotation = 90.0f;

    selector.anchorGrouping = AnchorPointGrouping::Character;
    TextAnimatorEngine::applyAnimator(characterAnchored, selector,
                                      WigglySelector{}, properties, 0.0f);
    selector.anchorGrouping = AnchorPointGrouping::All;
    TextAnimatorEngine::applyAnimator(groupAnchored, selector,
                                      WigglySelector{}, properties, 0.0f);

    EXPECT_NEAR(characterAnchored[0].offsetPosition.x(), 0.0f, 1e-6f);
    EXPECT_NEAR(characterAnchored[0].offsetPosition.y(), 0.0f, 1e-6f);
    EXPECT_NEAR(characterAnchored[1].offsetPosition.x(), 0.0f, 1e-6f);
    EXPECT_NEAR(characterAnchored[1].offsetPosition.y(), 0.0f, 1e-6f);
    EXPECT_NEAR(groupAnchored[0].offsetPosition.x(), 5.0f, 1e-5f);
    EXPECT_NEAR(groupAnchored[0].offsetPosition.y(), -5.0f, 1e-5f);
    EXPECT_NEAR(groupAnchored[1].offsetPosition.x(), -5.0f, 1e-5f);
    EXPECT_NEAR(groupAnchored[1].offsetPosition.y(), 5.0f, 1e-5f);
}

TEST(TextAnimatorContractTest, ClusterAndLineAnchorGroupingUseTheirDomains)
{
    auto clusterGlyphs = makeGlyphs(QStringLiteral("abcd"));
    auto lineGlyphs = clusterGlyphs;
    for (size_t index = 0; index < clusterGlyphs.size(); ++index) {
        const QRectF bounds(static_cast<double>(index) * 10.0, 0.0, 2.0, 2.0);
        clusterGlyphs[index].bounds = bounds;
        lineGlyphs[index].bounds = bounds;
    }
    clusterGlyphs[1].clusterIndex = 0;
    clusterGlyphs[2].clusterIndex = 0;
    clusterGlyphs[3].clusterIndex = 1;
    lineGlyphs[1].lineIndex = 1;
    lineGlyphs[2].lineIndex = 1;
    lineGlyphs[3].lineIndex = 0;

    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    AnimatorProperties properties;
    properties.rotation = 90.0f;

    selector.anchorGrouping = AnchorPointGrouping::Cluster;
    TextAnimatorEngine::applyAnimator(clusterGlyphs, selector,
                                      WigglySelector{}, properties, 0.0f);
    selector.anchorGrouping = AnchorPointGrouping::Line;
    TextAnimatorEngine::applyAnimator(lineGlyphs, selector,
                                      WigglySelector{}, properties, 0.0f);

    EXPECT_NEAR(clusterGlyphs[0].offsetPosition.x(), 10.0f, 1e-5f);
    EXPECT_FLOAT_EQ(clusterGlyphs[1].offsetPosition.x(), 0.0f);
    EXPECT_NEAR(clusterGlyphs[2].offsetPosition.x(), -10.0f, 1e-5f);
    EXPECT_FLOAT_EQ(clusterGlyphs[3].offsetPosition.x(), 0.0f);
    EXPECT_NEAR(lineGlyphs[0].offsetPosition.x(), 15.0f, 1e-5f);
    EXPECT_NEAR(lineGlyphs[1].offsetPosition.x(), 5.0f, 1e-5f);
    EXPECT_NEAR(lineGlyphs[2].offsetPosition.x(), -5.0f, 1e-5f);
    EXPECT_NEAR(lineGlyphs[3].offsetPosition.x(), -15.0f, 1e-5f);
}

TEST(TextAnimatorContractTest, WordAnchorGroupsGlyphsAroundSeparators)
{
    auto glyphs = makeGlyphs(QStringLiteral("ab cd"));
    for (size_t index = 0; index < glyphs.size(); ++index) {
        glyphs[index].bounds = QRectF(static_cast<double>(index) * 10.0,
                                      0.0, 2.0, 2.0);
    }
    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    selector.anchorGrouping = AnchorPointGrouping::Word;
    AnimatorProperties properties;
    properties.rotation = 90.0f;

    TextAnimatorEngine::applyAnimator(glyphs, selector, WigglySelector{},
                                      properties, 0.0f);

    EXPECT_NEAR(glyphs[0].offsetPosition.x(), 5.0f, 1e-5f);
    EXPECT_NEAR(glyphs[1].offsetPosition.x(), -5.0f, 1e-5f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetPosition.x(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetPosition.y(), 0.0f);
    EXPECT_NEAR(glyphs[3].offsetPosition.x(), 5.0f, 1e-5f);
    EXPECT_NEAR(glyphs[4].offsetPosition.x(), -5.0f, 1e-5f);
}

TEST(TextAnimatorContractTest, ParagraphAnchorSpansSoftWrappedLines)
{
    auto glyphs = makeGlyphs(QStringLiteral("abcd"));
    const QRectF bounds[] = {
        QRectF(0.0, 0.0, 2.0, 2.0), QRectF(10.0, 0.0, 2.0, 2.0),
        QRectF(0.0, 10.0, 2.0, 2.0), QRectF(10.0, 10.0, 2.0, 2.0)};
    for (size_t index = 0; index < glyphs.size(); ++index) {
        glyphs[index].bounds = bounds[index];
        glyphs[index].lineIndex = static_cast<int>(index / 2);
        glyphs[index].paragraphIndex = 0;
    }
    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    selector.anchorGrouping = AnchorPointGrouping::Paragraph;
    AnimatorProperties properties;
    properties.rotation = 90.0f;

    TextAnimatorEngine::applyAnimator(glyphs, selector, WigglySelector{},
                                      properties, 0.0f);

    EXPECT_NEAR(glyphs[0].offsetPosition.x(), 10.0f, 1e-5f);
    EXPECT_NEAR(glyphs[0].offsetPosition.y(), 0.0f, 1e-5f);
    EXPECT_NEAR(glyphs[1].offsetPosition.x(), 0.0f, 1e-5f);
    EXPECT_NEAR(glyphs[1].offsetPosition.y(), 10.0f, 1e-5f);
    EXPECT_NEAR(glyphs[2].offsetPosition.x(), 0.0f, 1e-5f);
    EXPECT_NEAR(glyphs[2].offsetPosition.y(), -10.0f, 1e-5f);
    EXPECT_NEAR(glyphs[3].offsetPosition.x(), -10.0f, 1e-5f);
    EXPECT_NEAR(glyphs[3].offsetPosition.y(), 0.0f, 1e-5f);
}

TEST(TextAnimatorContractTest, SpanAnchorStartsANewPivotAtTagChanges)
{
    auto glyphs = makeGlyphs(QStringLiteral("abc"));
    glyphs[0].bounds = QRectF(0.0, 0.0, 2.0, 2.0);
    glyphs[1].bounds = QRectF(10.0, 0.0, 2.0, 2.0);
    glyphs[2].bounds = QRectF(20.0, 0.0, 2.0, 2.0);
    glyphs[0].selectorTag = QStringLiteral("latin");
    glyphs[1].selectorTag = QStringLiteral("latin");
    glyphs[2].selectorTag = QStringLiteral("symbol");
    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    selector.anchorGrouping = AnchorPointGrouping::Span;
    AnimatorProperties properties;
    properties.rotation = 90.0f;

    TextAnimatorEngine::applyAnimator(glyphs, selector, WigglySelector{},
                                      properties, 0.0f);

    EXPECT_NEAR(glyphs[0].offsetPosition.x(), 5.0f, 1e-5f);
    EXPECT_NEAR(glyphs[1].offsetPosition.x(), -5.0f, 1e-5f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetPosition.x(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetPosition.y(), 0.0f);
}

TEST(TextAnimatorContractTest, TrackingAdvancesOncePerCluster)
{
    auto glyphs = makeGlyphs(QStringLiteral("abc"));
    glyphs[1].clusterIndex = 1;
    glyphs[2].index = 1;
    glyphs[2].clusterIndex = 1;
    glyphs[2].charCode = U'b';
    glyphs[2].clusterId = QStringLiteral("1");
    glyphs[2].clusterText = QStringLiteral("b");
    glyphs.push_back(glyphs[2]);
    glyphs.back().index = 2;
    glyphs.back().clusterIndex = 2;
    glyphs.back().charCode = U'c';
    glyphs.back().clusterId = QStringLiteral("2");
    glyphs.back().clusterText = QStringLiteral("c");

    RangeSelector selector;
    selector.shape = SelectorShape::Square;
    AnimatorProperties properties;
    properties.tracking = 5.0f;

    TextAnimatorEngine::applyAnimator(glyphs, selector, WigglySelector{},
                                      properties, 0.0f);

    ASSERT_EQ(glyphs.size(), 4u);
    EXPECT_FLOAT_EQ(glyphs[0].offsetPosition.x(), 0.0f);
    EXPECT_FLOAT_EQ(glyphs[1].offsetPosition.x(), 5.0f);
    EXPECT_FLOAT_EQ(glyphs[2].offsetPosition.x(), 5.0f);
    EXPECT_FLOAT_EQ(glyphs[3].offsetPosition.x(), 10.0f);
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
