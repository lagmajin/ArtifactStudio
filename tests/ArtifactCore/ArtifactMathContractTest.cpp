#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>

import Core.ArtifactMath;

using namespace ArtifactCore;

TEST(ArtifactMathContractTest, NumericTraitsReportLimitsAndCategories)
{
    EXPECT_TRUE(NumericTraits<int>::isInteger);
    EXPECT_TRUE(NumericTraits<int>::isSigned);
    EXPECT_EQ(NumericTraits<int>::maxValue(), std::numeric_limits<int>::max());
    EXPECT_EQ(NumericTraits<int>::minValue(), std::numeric_limits<int>::lowest());
    EXPECT_FALSE(NumericTraits<unsigned>::isSigned);
    EXPECT_EQ(NumericTraits<unsigned>::minValue(), 0u);
    EXPECT_FALSE(NumericTraits<float>::isInteger);
    EXPECT_TRUE(NumericTraits<long double>::isSigned);
    EXPECT_EQ(NumericTraits<unsigned char>::maxValue(),
              std::numeric_limits<unsigned char>::max());
}

TEST(ArtifactMathContractTest, MinAndMaxChooseFirstValueOnTies)
{
    const int first = 5;
    const int second = 5;
    EXPECT_EQ(&artifactMin(first, second), &first);
    EXPECT_EQ(&artifactMax(first, second), &first);
    EXPECT_EQ(artifactMin(8, 3, 3, 9), 3);
    EXPECT_EQ(artifactMax(8, 3, 9, 9), 9);
}

TEST(ArtifactMathContractTest, MinMaxAndClampSupportFloatingValues)
{
    EXPECT_DOUBLE_EQ(artifactMin(-2.5, 1.25), -2.5);
    EXPECT_DOUBLE_EQ(artifactMax(-2.5, 1.25), 1.25);
    EXPECT_FLOAT_EQ(artifactClamp(1.5f, 0.0f, 1.0f), 1.0f);
}

TEST(ArtifactMathContractTest, ClampIncludesBothEndpointsAndInteriorValues)
{
    EXPECT_EQ(artifactClamp(-1, 0, 10), 0);
    EXPECT_EQ(artifactClamp(0, 0, 10), 0);
    EXPECT_EQ(artifactClamp(7, 0, 10), 7);
    EXPECT_EQ(artifactClamp(10, 0, 10), 10);
    EXPECT_EQ(artifactClamp(11, 0, 10), 10);
    EXPECT_DOUBLE_EQ(artifactClamp(0.5, 0.0, 1.0), 0.5);
    EXPECT_DOUBLE_EQ(artifactClamp(-0.5, 0.0, 1.0), 0.0);
}

TEST(ArtifactMathContractTest, AbsPreservesIntegralAndFloatingMagnitudes)
{
    EXPECT_EQ(artifactAbs(-7), 7);
    EXPECT_EQ(artifactAbs(0), 0);
    EXPECT_DOUBLE_EQ(artifactAbs(-2.5), 2.5);
    EXPECT_EQ(artifactAbs(3u), 3u);
}

TEST(ArtifactMathContractTest, AbsClearsTheSignBitOfNegativeZero)
{
    const double result = artifactAbs(-0.0);

    EXPECT_EQ(result, 0.0);
    EXPECT_FALSE(std::signbit(result));
}

TEST(ArtifactMathContractTest, FiniteAndNanChecksCoverFloatingAndIntegralValues)
{
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(artifactIsFinite(12));
    EXPECT_TRUE(artifactIsFinite(1.0f));
    EXPECT_FALSE(artifactIsFinite(infinity));
    EXPECT_FALSE(artifactIsFinite(nan));
    EXPECT_FALSE(artifactIsNaN(12));
    EXPECT_TRUE(artifactIsNaN(nan));
    EXPECT_FALSE(artifactIsNaN(infinity));
}

TEST(ArtifactMathContractTest, SqrtAndPowPreserveFloatAndPromoteIntegralArguments)
{
    static_assert(std::is_same_v<decltype(artifactSqrt(4)), double>);
    static_assert(std::is_same_v<decltype(artifactPow(2, 3)), double>);
    static_assert(std::is_same_v<decltype(artifactSin(2)), double>);
    static_assert(std::is_same_v<decltype(artifactFmod(2, 1.0f)), double>);
    EXPECT_DOUBLE_EQ(artifactSqrt(9), 3.0);
    EXPECT_FLOAT_EQ(artifactSqrt(4.0f), 2.0f);
    EXPECT_DOUBLE_EQ(artifactPow(2, 10), 1024.0);
    EXPECT_FLOAT_EQ(artifactPow(3.0f, 2.0f), 9.0f);
    EXPECT_DOUBLE_EQ(artifactPow(2, -3), 0.125);
    EXPECT_DOUBLE_EQ(artifactPow(-2, 3), -8.0);
}

TEST(ArtifactMathContractTest, MixedArithmeticOverloadsPromoteToDouble)
{
    static_assert(std::is_same_v<decltype(artifactPow(2.0f, 3)), double>);
    static_assert(std::is_same_v<decltype(artifactAtan2(1, 1.0f)), double>);
    EXPECT_DOUBLE_EQ(artifactPow(2.0f, 3), 8.0);
    EXPECT_NEAR(artifactAtan2(1, 1.0f), 0.7853981633974483, 1e-12);
}

TEST(ArtifactMathContractTest, TrigonometricAndInverseFunctionsUseRadians)
{
    constexpr double halfPi = 1.57079632679489661923;
    EXPECT_NEAR(artifactSin(halfPi), 1.0, 1e-12);
    EXPECT_NEAR(artifactCos(0.0), 1.0, 1e-12);
    EXPECT_NEAR(artifactTan(0.0), 0.0, 1e-12);
    EXPECT_NEAR(artifactAtan(1.0), halfPi / 2.0, 1e-12);
    EXPECT_NEAR(artifactAcos(1.0), 0.0, 1e-12);
    EXPECT_NEAR(artifactAtan2(1, 0.0), halfPi, 1e-12);
    EXPECT_NEAR(artifactSin(-halfPi), -1.0, 1e-12);
    EXPECT_NEAR(artifactCos(halfPi), 0.0, 1e-12);
    EXPECT_NEAR(artifactAtan2(-1.0, 0.0), -halfPi, 1e-12);
    EXPECT_NEAR(artifactAcos(-1.0), 2.0 * halfPi, 1e-12);
}

TEST(ArtifactMathContractTest, Atan2ReturnsExpectedQuadrantAngles)
{
    constexpr double quarterPi = 0.78539816339744830962;
    EXPECT_NEAR(artifactAtan2(1.0, 1.0), quarterPi, 1e-12);
    EXPECT_NEAR(artifactAtan2(1.0, -1.0), 3.0 * quarterPi, 1e-12);
    EXPECT_NEAR(artifactAtan2(-1.0, -1.0), -3.0 * quarterPi, 1e-12);
    EXPECT_NEAR(artifactAtan2(-1.0, 1.0), -quarterPi, 1e-12);
}

TEST(ArtifactMathContractTest, FloorCeilAndRoundHandleNegativeFractions)
{
    EXPECT_DOUBLE_EQ(artifactFloor(-1.2), -2.0);
    EXPECT_DOUBLE_EQ(artifactCeil(-1.2), -1.0);
    EXPECT_DOUBLE_EQ(artifactRound(1.5), 2.0);
    EXPECT_DOUBLE_EQ(artifactRound(-1.5), -2.0);
    EXPECT_EQ(artifactLround(2.5), 3);
    EXPECT_EQ(artifactLround(-2.5), -3);
    EXPECT_EQ(artifactLlround(3.5), 4);
    EXPECT_EQ(artifactLlround(-3.5), -4);
    EXPECT_DOUBLE_EQ(artifactFloor(1.2), 1.0);
    EXPECT_DOUBLE_EQ(artifactCeil(1.2), 2.0);
    EXPECT_DOUBLE_EQ(artifactRound(2.49), 2.0);
}

TEST(ArtifactMathContractTest, FmodUsesTruncatedQuotientRemainderSign)
{
    EXPECT_DOUBLE_EQ(artifactFmod(7.0, 4.0), 3.0);
    EXPECT_DOUBLE_EQ(artifactFmod(-7.0, 4.0), -3.0);
    EXPECT_DOUBLE_EQ(artifactFmod(7, 4.0f), 3.0);
    EXPECT_DOUBLE_EQ(artifactFmod(7.0, -4.0), 3.0);
    EXPECT_DOUBLE_EQ(artifactFmod(-7.0, -4.0), -3.0);
}

TEST(ArtifactMathContractTest, HypotAvoidsIntermediateOverflowForLargeInputs)
{
    EXPECT_DOUBLE_EQ(artifactHypot(3.0, 4.0), 5.0);
    EXPECT_FLOAT_EQ(artifactHypot(5.0f, 12.0f), 13.0f);
    const double large = std::numeric_limits<double>::max() / 2.0;
    EXPECT_TRUE(artifactIsFinite(artifactHypot(large, large)));
}

TEST(ArtifactMathContractTest, SwapExchangesValuesAndSupportsSelfSwap)
{
    int first = 2;
    int second = 9;
    artifactSwap(first, second);
    EXPECT_EQ(first, 9);
    EXPECT_EQ(second, 2);
    artifactSwap(first, first);
    EXPECT_EQ(first, 9);

    std::string left = "left";
    std::string right = "right";
    artifactSwap(left, right);
    EXPECT_EQ(left, "right");
    EXPECT_EQ(right, "left");

    auto ownerA = std::make_unique<int>(1);
    auto ownerB = std::make_unique<int>(2);
    artifactSwap(ownerA, ownerB);
    ASSERT_NE(ownerA, nullptr);
    ASSERT_NE(ownerB, nullptr);
    EXPECT_EQ(*ownerA, 2);
    EXPECT_EQ(*ownerB, 1);
}

TEST(ArtifactMathContractTest, SwapSupportsSelfSwapOfOwningValue)
{
    auto owner = std::make_unique<std::string>("kept");
    artifactSwap(owner, owner);
    ASSERT_NE(owner, nullptr);
    EXPECT_EQ(*owner, "kept");
}

TEST(ArtifactMathContractTest, LerpInterpolatesAndExtrapolates)
{
    EXPECT_DOUBLE_EQ(artifactLerp(2.0, 10.0, 0.25), 4.0);
    EXPECT_DOUBLE_EQ(artifactLerp(2.0, 10.0, 1.5), 14.0);
    EXPECT_EQ(artifactLerp(2, 8, 0.5), 5.0);
    EXPECT_DOUBLE_EQ(artifactLerp(2.0, 10.0, 0.0), 2.0);
    EXPECT_DOUBLE_EQ(artifactLerp(2.0, 10.0, 1.0), 10.0);
    EXPECT_DOUBLE_EQ(artifactLerp(2.0, 10.0, -0.5), -2.0);
}

TEST(ArtifactMathContractTest, LerpPreservesFloatTypeAndEndpointValues)
{
    static_assert(std::is_same_v<decltype(artifactLerp(1.0f, 3.0f, 0.5f)), float>);
    EXPECT_FLOAT_EQ(artifactLerp(1.0f, 3.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(artifactLerp(1.0f, 3.0f, 1.0f), 3.0f);
}

