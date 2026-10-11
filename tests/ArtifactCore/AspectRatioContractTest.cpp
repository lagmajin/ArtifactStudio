#include <gtest/gtest.h>

#include <QString>
#include <limits>
#include <type_traits>
#include <utility>

import Core.AspectRatio;
import Utils.String.UniString;

using namespace ArtifactCore;

static_assert(std::is_copy_constructible_v<AspectRatio>);
static_assert(std::is_copy_assignable_v<AspectRatio>);
static_assert(std::is_nothrow_move_constructible_v<AspectRatio>);
static_assert(std::is_nothrow_move_assignable_v<AspectRatio>);

namespace {
void expectAspect(const AspectRatio& aspect, const QString& text, double ratio)
{
    EXPECT_EQ(aspect.toString().toQString(), text);
    EXPECT_DOUBLE_EQ(aspect.ratio(), ratio);
}
}

TEST(AspectRatioContractTest, DefaultIsSquare)
{
    const AspectRatio aspect;
    expectAspect(aspect, QStringLiteral("1:1"), 1.0);
}

TEST(AspectRatioContractTest, ConstructorSimplifiesCommonFactors)
{
    const AspectRatio aspect(1920, 1080);
    expectAspect(aspect, QStringLiteral("16:9"), 16.0 / 9.0);
}

TEST(AspectRatioContractTest, ConstructorKeepsAlreadySimplifiedPair)
{
    const AspectRatio aspect(4, 3);
    expectAspect(aspect, QStringLiteral("4:3"), 4.0 / 3.0);
}

TEST(AspectRatioContractTest, SimplifiesMaximumRepresentableEqualDimensions)
{
    constexpr int maximum = std::numeric_limits<int>::max();
    const AspectRatio aspect(maximum, maximum);

    expectAspect(aspect, QStringLiteral("1:1"), 1.0);
}

TEST(AspectRatioContractTest, ParsesColonSeparatedPair)
{
    AspectRatio aspect;
    aspect.setFromString(UniString(QStringLiteral("21:9")));
    expectAspect(aspect, QStringLiteral("7:3"), 7.0 / 3.0);
}

TEST(AspectRatioContractTest, ParsesSlashSeparatedPair)
{
    AspectRatio aspect;
    aspect.setFromString(UniString(QStringLiteral("32/18")));
    expectAspect(aspect, QStringLiteral("16:9"), 16.0 / 9.0);
}

TEST(AspectRatioContractTest, ParsesPixelDimensions)
{
    AspectRatio aspect;
    aspect.setFromString(UniString(QStringLiteral("1920x1080")));
    expectAspect(aspect, QStringLiteral("16:9"), 16.0 / 9.0);
}

TEST(AspectRatioContractTest, PixelDimensionSeparatorIsCaseInsensitive)
{
    AspectRatio aspect;
    aspect.setFromString(UniString(QStringLiteral("640X480")));
    expectAspect(aspect, QStringLiteral("4:3"), 4.0 / 3.0);
}

TEST(AspectRatioContractTest, ParsesDecimalAsBoundedRationalApproximation)
{
    AspectRatio aspect;
    aspect.setFromString(UniString(QStringLiteral("1.7777778")));
    expectAspect(aspect, QStringLiteral("16:9"), 16.0 / 9.0);
}

TEST(AspectRatioContractTest, ParsesDecimalBelowOne)
{
    AspectRatio aspect;
    aspect.setFromString(UniString(QStringLiteral("0.625")));
    expectAspect(aspect, QStringLiteral("5:8"), 0.625);
}

TEST(AspectRatioContractTest, SmallPositiveDecimalUsesBestBoundedPositiveApproximation)
{
    AspectRatio aspect;
    aspect.setFromString(UniString(QStringLiteral("0.004")));
    expectAspect(aspect, QStringLiteral("1:100"), 0.01);
}

TEST(AspectRatioContractTest, TrimsWhitespaceAroundInput)
{
    AspectRatio aspect;
    aspect.setFromString(UniString(QStringLiteral("  16:9  ")));
    expectAspect(aspect, QStringLiteral("16:9"), 16.0 / 9.0);
}

TEST(AspectRatioContractTest, EmptyInputPreservesCurrentValue)
{
    AspectRatio aspect(3, 2);
    aspect.setFromString(UniString(QStringLiteral("   ")));
    expectAspect(aspect, QStringLiteral("3:2"), 1.5);
}

TEST(AspectRatioContractTest, InvalidInputPreservesCurrentValue)
{
    AspectRatio aspect(5, 4);
    aspect.setFromString(UniString(QStringLiteral("not a ratio")));
    expectAspect(aspect, QStringLiteral("5:4"), 1.25);
}

TEST(AspectRatioContractTest, NonPositivePairDoesNotReplaceCurrentValue)
{
    AspectRatio aspect(16, 9);
    aspect.setFromString(UniString(QStringLiteral("0:9")));
    expectAspect(aspect, QStringLiteral("16:9"), 16.0 / 9.0);
    aspect.setFromString(UniString(QStringLiteral("-4:3")));
    expectAspect(aspect, QStringLiteral("16:9"), 16.0 / 9.0);
}

TEST(AspectRatioContractTest, MalformedPairComponentPreservesCurrentValue)
{
    AspectRatio aspect(16, 9);
    aspect.setFromString(UniString(QStringLiteral("12xnot-a-height")));
    expectAspect(aspect, QStringLiteral("16:9"), 16.0 / 9.0);

    aspect.setFromString(UniString(QStringLiteral("not-a-width/3")));
    expectAspect(aspect, QStringLiteral("16:9"), 16.0 / 9.0);
}

TEST(AspectRatioContractTest, NonPositiveDecimalDoesNotReplaceCurrentValue)
{
    AspectRatio aspect(4, 3);
    aspect.setFromString(UniString(QStringLiteral("0.0")));
    expectAspect(aspect, QStringLiteral("4:3"), 4.0 / 3.0);
    aspect.setFromString(UniString(QStringLiteral("-1.5")));
    expectAspect(aspect, QStringLiteral("4:3"), 4.0 / 3.0);
}

TEST(AspectRatioContractTest, ZeroDenominatorUsesSafeRatioFallback)
{
    const AspectRatio aspect(16, 0);
    EXPECT_DOUBLE_EQ(aspect.ratio(), 1.0);
    EXPECT_EQ(aspect.toString().toQString(), QStringLiteral("16:0"));
}

TEST(AspectRatioContractTest, CopyConstructionIsIndependent)
{
    AspectRatio source(16, 9);
    AspectRatio copy(source);
    source.setFromString(UniString(QStringLiteral("4:3")));
    expectAspect(copy, QStringLiteral("16:9"), 16.0 / 9.0);
    expectAspect(source, QStringLiteral("4:3"), 4.0 / 3.0);
}

TEST(AspectRatioContractTest, CopyAssignmentIsIndependent)
{
    AspectRatio source(3, 2);
    AspectRatio destination(1, 1);
    destination = source;
    source.setFromString(UniString(QStringLiteral("4:3")));
    expectAspect(destination, QStringLiteral("3:2"), 1.5);
}

TEST(AspectRatioContractTest, SelfCopyAssignmentPreservesValue)
{
    AspectRatio aspect(2, 1);
    aspect = aspect;
    expectAspect(aspect, QStringLiteral("2:1"), 2.0);
}

TEST(AspectRatioContractTest, MoveConstructionTransfersValueAndLeavesSafeFallback)
{
    AspectRatio source(16, 9);
    AspectRatio destination(std::move(source));
    expectAspect(destination, QStringLiteral("16:9"), 16.0 / 9.0);
    expectAspect(source, QStringLiteral("1:1"), 1.0);
}

TEST(AspectRatioContractTest, MoveAssignmentReplacesDestination)
{
    AspectRatio source(21, 9);
    AspectRatio destination(4, 3);
    destination = std::move(source);
    expectAspect(destination, QStringLiteral("7:3"), 7.0 / 3.0);
    expectAspect(source, QStringLiteral("1:1"), 1.0);
}

