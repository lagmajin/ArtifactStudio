#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <type_traits>

import Math.ArtifactMinMax;

using namespace ArtifactCore;

TEST(ArtifactMinMaxStandaloneTest, IntegerMinimumAndMaximumSelectExpectedOperand)
{
    EXPECT_EQ(artifactMin(4, -2), -2);
    EXPECT_EQ(artifactMax(4, -2), 4);
}

TEST(ArtifactMinMaxStandaloneTest, MixedArithmeticArgumentsUseCommonType)
{
    const auto minimum = artifactMin(2, 3.5);
    const auto maximum = artifactMax(2, 3.5);
    static_assert(std::is_same_v<decltype(minimum), double>);
    static_assert(std::is_same_v<decltype(maximum), double>);
    EXPECT_DOUBLE_EQ(minimum, 2.0);
    EXPECT_DOUBLE_EQ(maximum, 3.5);
}

TEST(ArtifactMinMaxStandaloneTest, MinimumIgnoresOneNanOperand)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_DOUBLE_EQ(artifactMin(nan, 7.0), 7.0);
    EXPECT_DOUBLE_EQ(artifactMin(7.0, nan), 7.0);
}

TEST(ArtifactMinMaxStandaloneTest, MaximumIgnoresOneNanOperand)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_DOUBLE_EQ(artifactMax(nan, 7.0), 7.0);
    EXPECT_DOUBLE_EQ(artifactMax(7.0, nan), 7.0);
}

TEST(ArtifactMinMaxStandaloneTest, BothNanOperandsRemainNan)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(std::isnan(artifactMin(nan, nan)));
    EXPECT_TRUE(std::isnan(artifactMax(nan, nan)));
}

TEST(ArtifactMinMaxStandaloneTest, MinimumAndMaximumHandleInfinities)
{
    const double infinity = std::numeric_limits<double>::infinity();
    EXPECT_EQ(artifactMin(infinity, 5.0), 5.0);
    EXPECT_EQ(artifactMax(-infinity, 5.0), 5.0);
    EXPECT_EQ(artifactMax(infinity, 5.0), infinity);
    EXPECT_EQ(artifactMin(-infinity, infinity), -infinity);
    EXPECT_EQ(artifactMax(-infinity, infinity), infinity);
}

TEST(ArtifactMinMaxStandaloneTest, MinimumAndMaximumHandleBothNegativeInfinities)
{
    const double negativeInfinity = -std::numeric_limits<double>::infinity();
    EXPECT_EQ(artifactMin(negativeInfinity, negativeInfinity), negativeInfinity);
    EXPECT_EQ(artifactMax(negativeInfinity, negativeInfinity), negativeInfinity);
}

TEST(ArtifactMinMaxStandaloneTest, ClampKeepsValuesInsideOrderedBounds)
{
    EXPECT_EQ(clamp(-5, 0, 10), 0);
    EXPECT_EQ(clamp(5, 0, 10), 5);
    EXPECT_EQ(clamp(15, 0, 10), 10);
}

TEST(ArtifactMinMaxStandaloneTest, MixedClampBoundsConvertToValueType)
{
    EXPECT_DOUBLE_EQ(clamp(2.5, 0, 2), 2.0);
    EXPECT_DOUBLE_EQ(clamp(-1.5, 0, 2), 0.0);
}

TEST(ArtifactMinMaxStandaloneTest, SafeNormalizeRangeOrdersReversedEndpoints)
{
    const auto result = safeNormalizeRange(0.0, 9.0, -3.0);
    EXPECT_DOUBLE_EQ(result.min, -3.0);
    EXPECT_DOUBLE_EQ(result.max, 9.0);
}

TEST(ArtifactMinMaxStandaloneTest, SafeNormalizeRangeFallsBackForNanInputs)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const auto result : {safeNormalizeRange(nan, 0.0, 1.0),
                              safeNormalizeRange(0.5, nan, 1.0),
                              safeNormalizeRange(0.5, 0.0, nan)}) {
        EXPECT_DOUBLE_EQ(result.min, 0.0);
        EXPECT_DOUBLE_EQ(result.max, 1.0);
    }
}

TEST(ArtifactMinMaxStandaloneTest, SafeNormalizeRangePreservesDegenerateValue)
{
    const auto result = safeNormalizeRange(4.25, 3.0, 3.0);
    EXPECT_DOUBLE_EQ(result.min, 4.25);
    EXPECT_DOUBLE_EQ(result.max, 4.25);
}

TEST(ArtifactMinMaxStandaloneTest, SafeNormalizeRangeTreatsSubEpsilonSpanAsDegenerate)
{
    const double lower = 0.0;
    const double upper = lower + std::numeric_limits<double>::epsilon() * 0.5;
    const auto result = safeNormalizeRange(7.0, lower, upper);
    EXPECT_DOUBLE_EQ(result.min, 7.0);
    EXPECT_DOUBLE_EQ(result.max, 7.0);
}

TEST(ArtifactMinMaxStandaloneTest, SafeNormalizeRangeOrdersSpanAtOrAboveEpsilon)
{
    const double lower = 2.0;
    const double upper = lower + std::numeric_limits<double>::epsilon() * 2.0;
    const auto result = safeNormalizeRange(7.0, upper, lower);
    EXPECT_DOUBLE_EQ(result.min, lower);
    EXPECT_DOUBLE_EQ(result.max, upper);
}

TEST(ArtifactMinMaxStandaloneTest, SafeNormalizeRangePreservesSpanAtExactEpsilon)
{
    const double lower = 0.0;
    const double upper = std::numeric_limits<double>::epsilon();
    const auto result = safeNormalizeRange(7.0, lower, upper);
    EXPECT_DOUBLE_EQ(result.min, lower);
    EXPECT_DOUBLE_EQ(result.max, upper);
}

TEST(ArtifactMinMaxStandaloneTest, LerpInterpolatesAndExtrapolates)
{
    EXPECT_DOUBLE_EQ(artifactLerp(2.0, 6.0, 0.25), 3.0);
    EXPECT_DOUBLE_EQ(artifactLerp(2.0, 6.0, 1.5), 8.0);
}

TEST(ArtifactMinMaxStandaloneTest, LerpPreservesEndpointsForDescendingValues)
{
    EXPECT_DOUBLE_EQ(artifactLerp(10.0, -2.0, 0.0), 10.0);
    EXPECT_DOUBLE_EQ(artifactLerp(10.0, -2.0, 0.25), 7.0);
    EXPECT_DOUBLE_EQ(artifactLerp(10.0, -2.0, 1.0), -2.0);
}

TEST(ArtifactMinMaxStandaloneTest, IntegerLerpUsesArithmeticResultType)
{
    const auto result = artifactLerp(2, 6, 0.5);
    static_assert(std::is_same_v<decltype(result), double>);
    EXPECT_DOUBLE_EQ(result, 4.0);
}

TEST(ArtifactMinMaxStandaloneTest, LerpNanEndpointUsesTheOtherEndpoint)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_DOUBLE_EQ(artifactLerp(nan, 6.0, 0.25), 6.0);
    EXPECT_DOUBLE_EQ(artifactLerp(2.0, nan, 0.25), 2.0);
}

TEST(ArtifactMinMaxStandaloneTest, LerpNanFactorUsesFirstEndpoint)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_DOUBLE_EQ(artifactLerp(2.0, 6.0, nan), 2.0);
}

TEST(ArtifactMinMaxStandaloneTest, LerpSupportsIntegralValues)
{
    EXPECT_EQ(artifactLerp(2, 8, 0), 2);
    EXPECT_EQ(artifactLerp(2, 8, 1), 8);
}
