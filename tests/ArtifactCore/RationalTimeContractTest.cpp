#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>

import Time.Rational;

using ArtifactCore::RationalTime;

TEST(RationalTimeStandaloneTest, NonpositiveConstructorScaleFallsBackToOne)
{
    EXPECT_EQ(RationalTime(7, 0).scale(), 1);
    EXPECT_EQ(RationalTime(7, -3).scale(), 1);
    EXPECT_EQ(RationalTime(-9, 0), RationalTime(-9, 1));
}

TEST(RationalTimeStandaloneTest, FrameCountConversionRoundsHalfAwayFromZero)
{
    EXPECT_EQ(RationalTime(1, 8).toFrameCount(30), 4);
    EXPECT_EQ(RationalTime(-1, 8).toFrameCount(30), -4);
    EXPECT_EQ(RationalTime(1, 3).toFrameCount(30), 10);
    EXPECT_EQ(RationalTime(45, 15).rescaledTo(30), 90);
}

TEST(RationalTimeStandaloneTest, FrameCountOverflowFallbackRoundsLargeSignedValues)
{
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto positive = RationalTime(maximum, 4).toFrameCount(2);
    const auto negative = RationalTime(-maximum, 4).toFrameCount(2);

    EXPECT_EQ(positive, maximum / 2 + 1);
    EXPECT_EQ(negative, -(maximum / 2 + 1));
}

TEST(RationalTimeStandaloneTest, RoundingBiasDoesNotOverflowAtInt64NumeratorLimit)
{
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto roundedHalf = maximum / 2 + 1;

    EXPECT_EQ(RationalTime(maximum, 2).rescaledTo(1), roundedHalf);
    EXPECT_EQ(RationalTime(-maximum, 2).rescaledTo(1), -roundedHalf);
}

TEST(RationalTimeStandaloneTest, RescalingMinimumNumeratorDoesNotNegateOverflow)
{
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();

    EXPECT_EQ(RationalTime(minimum, 1).rescaledTo(1), minimum);
    EXPECT_EQ(RationalTime(minimum, 2).rescaledTo(1), minimum / 2);
}

TEST(RationalTimeStandaloneTest, InvalidFrameRatesReturnSafeResults)
{
    EXPECT_EQ(RationalTime(3, 2).toFrameCount(0), 0);
    EXPECT_EQ(RationalTime(3, 2).toFrameCount(-24), 0);
    EXPECT_EQ(RationalTime(3, 2).rescaledTo(0), 0);
}

TEST(RationalTimeStandaloneTest, NegativeTargetScaleReturnsZeroLikeInvalidFrameRate)
{
    EXPECT_EQ(RationalTime(1, 2).rescaledTo(-30), 0);
    EXPECT_EQ(RationalTime(-1, 2).rescaledTo(-30), 0);
}

TEST(RationalTimeStandaloneTest, FrameCountFactoryFallsBackToUnitScaleForInvalidFps)
{
    EXPECT_EQ(RationalTime::fromFrameCount(24, 30), RationalTime(24, 30));
    EXPECT_EQ(RationalTime::fromFrameCount(24, 0), RationalTime(24, 1));
    EXPECT_EQ(RationalTime::fromFrameCount(-12, -24), RationalTime(-12, 1));
}

TEST(RationalTimeStandaloneTest, CrossScaleAdditionAndSubtractionStayExact)
{
    const RationalTime third(1, 3);
    const RationalTime sixth(1, 6);

    EXPECT_EQ(third + sixth, RationalTime(1, 2));
    EXPECT_EQ(third - sixth, RationalTime(1, 6));
    EXPECT_EQ(sixth - third, RationalTime(-1, 6));
    EXPECT_EQ((RationalTime(100, 30) + RationalTime(50, 30)).value(), 150);
    EXPECT_EQ((RationalTime(100, 30) + RationalTime(50, 30)).scale(), 30);
}

TEST(RationalTimeStandaloneTest, AdditionFallsBackWhenCommonScaleOverflows)
{
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const RationalTime lhs(1, maximum - 1);
    const RationalTime rhs(1, maximum - 2);

    const RationalTime expected = RationalTime::fromSeconds(
        lhs.toSeconds() + rhs.toSeconds());
    const RationalTime actual = lhs + rhs;

    EXPECT_EQ(actual, expected);
}

TEST(RationalTimeStandaloneTest, SubtractionFallsBackWhenCommonScaleOverflows)
{
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const RationalTime lhs(2, maximum - 1);
    const RationalTime rhs(1, maximum - 2);

    const RationalTime expected = RationalTime::fromSeconds(
        lhs.toSeconds() - rhs.toSeconds());
    const RationalTime actual = lhs - rhs;

    EXPECT_EQ(actual, expected);
}

TEST(RationalTimeStandaloneTest, ArithmeticMatchesSmallIntegerReferenceGrid)
{
    for (std::int64_t lhsValue = -3; lhsValue <= 3; ++lhsValue) {
        for (std::int64_t rhsValue = -3; rhsValue <= 3; ++rhsValue) {
            for (std::int64_t lhsScale = 1; lhsScale <= 5; ++lhsScale) {
                for (std::int64_t rhsScale = 1; rhsScale <= 5; ++rhsScale) {
                    const RationalTime lhs(lhsValue, lhsScale);
                    const RationalTime rhs(rhsValue, rhsScale);
                    const auto commonScale = lhsScale * rhsScale;
                    const RationalTime expectedSum(
                        lhsValue * rhsScale + rhsValue * lhsScale, commonScale);
                    const RationalTime expectedDifference(
                        lhsValue * rhsScale - rhsValue * lhsScale, commonScale);

                    SCOPED_TRACE(::testing::Message()
                        << "lhs=" << lhsValue << '/' << lhsScale
                        << " rhs=" << rhsValue << '/' << rhsScale);
                    EXPECT_EQ(lhs + rhs, expectedSum);
                    EXPECT_EQ(lhs - rhs, expectedDifference);
                }
            }
        }
    }
}

TEST(RationalTimeStandaloneTest, AdditionIsAssociativeAcrossSmallRationalGrid)
{
    constexpr std::int64_t values[] = {-2, 0, 3};
    for (const std::int64_t a : values) {
        for (const std::int64_t b : values) {
            for (const std::int64_t c : values) {
                for (std::int64_t scaleA = 1; scaleA <= 4; ++scaleA) {
                    for (std::int64_t scaleB = 1; scaleB <= 4; ++scaleB) {
                        for (std::int64_t scaleC = 1; scaleC <= 4; ++scaleC) {
                            const RationalTime lhs(a, scaleA);
                            const RationalTime middle(b, scaleB);
                            const RationalTime rhs(c, scaleC);

                            SCOPED_TRACE(::testing::Message()
                                << "a=" << a << '/' << scaleA
                                << " b=" << b << '/' << scaleB
                                << " c=" << c << '/' << scaleC);
                            EXPECT_EQ((lhs + middle) + rhs,
                                      lhs + (middle + rhs));
                        }
                    }
                }
            }
        }
    }
}

TEST(RationalTimeStandaloneTest, ComparisonMatchesIntegerCrossProductGrid)
{
    for (std::int64_t lhsValue = -5; lhsValue <= 5; ++lhsValue) {
        for (std::int64_t rhsValue = -5; rhsValue <= 5; ++rhsValue) {
            for (std::int64_t lhsScale = 1; lhsScale <= 7; ++lhsScale) {
                for (std::int64_t rhsScale = 1; rhsScale <= 7; ++rhsScale) {
                    const RationalTime lhs(lhsValue, lhsScale);
                    const RationalTime rhs(rhsValue, rhsScale);
                    const std::int64_t lhsCross = lhsValue * rhsScale;
                    const std::int64_t rhsCross = rhsValue * lhsScale;

                    SCOPED_TRACE(::testing::Message()
                        << "lhs=" << lhsValue << '/' << lhsScale
                        << " rhs=" << rhsValue << '/' << rhsScale);
                    EXPECT_EQ(lhs < rhs, lhsCross < rhsCross);
                    EXPECT_EQ(lhs > rhs, lhsCross > rhsCross);
                    EXPECT_EQ(lhs <= rhs, lhsCross <= rhsCross);
                    EXPECT_EQ(lhs >= rhs, lhsCross >= rhsCross);
                    EXPECT_EQ(lhs == rhs, lhsCross == rhsCross);
                    EXPECT_EQ(lhs != rhs, lhsCross != rhsCross);
                }
            }
        }
    }
}

TEST(RationalTimeStandaloneTest, RescalingMatchesSignedRoundingReferenceGrid)
{
    for (std::int64_t value = -7; value <= 7; ++value) {
        for (std::int64_t scale = 1; scale <= 7; ++scale) {
            for (std::int64_t targetScale = 1; targetScale <= 10; ++targetScale) {
                const RationalTime input(value, scale);
                const auto expected = static_cast<std::int64_t>(std::round(
                    static_cast<double>(value) * static_cast<double>(targetScale) /
                    static_cast<double>(scale)));

                SCOPED_TRACE(::testing::Message()
                    << "value=" << value << '/' << scale
                    << " targetScale=" << targetScale);
                EXPECT_EQ(input.rescaledTo(targetScale), expected);
            }
        }
    }
}

TEST(RationalTimeStandaloneTest, EqualityAndOrderingCompareReducedSignedFractions)
{
    EXPECT_EQ(RationalTime(1, 2), RationalTime(2, 4));
    EXPECT_LT(RationalTime(-3, 4), RationalTime(-2, 3));
    EXPECT_LT(RationalTime(-2, 3), RationalTime(1, 2));
    EXPECT_GT(RationalTime(2, 3), RationalTime(1, 2));
    EXPECT_LE(RationalTime(2, 4), RationalTime(1, 2));
    EXPECT_GE(RationalTime(2, 4), RationalTime(1, 2));
}

TEST(RationalTimeStandaloneTest, ComparisonHandlesLargeCrossScaleFractions)
{
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const RationalTime closerToOne(maximum - 1, maximum);
    const RationalTime slightlyLower(maximum - 2, maximum - 1);

    EXPECT_GT(closerToOne, slightlyLower);
    EXPECT_LT(slightlyLower, closerToOne);
    EXPECT_NE(closerToOne, slightlyLower);
}

TEST(RationalTimeStandaloneTest, ComparisonHandlesMinimumSignedNumerator)
{
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const RationalTime belowNegativeOne(minimum, maximum);

    EXPECT_LT(belowNegativeOne, RationalTime(-1, 1));
    EXPECT_LT(belowNegativeOne, RationalTime(-1, 2));
    EXPECT_GT(RationalTime(-1, 1), belowNegativeOne);
}

TEST(RationalTimeStandaloneTest, FromSecondsUsesDocumentedDefaultScale)
{
    const RationalTime positive = RationalTime::fromSeconds(1.25);
    const RationalTime negative = RationalTime::fromSeconds(-0.5);

    EXPECT_EQ(positive.value(), 12500000);
    EXPECT_EQ(positive.scale(), 10000000);
    EXPECT_DOUBLE_EQ(positive.toSeconds(), 1.25);
    EXPECT_EQ(negative.value(), -5000000);
    EXPECT_DOUBLE_EQ(negative.toDouble(), -0.5);
}

TEST(RationalTimeStandaloneTest, FromSecondsRoundsHalfDefaultScaleTickAwayFromZero)
{
    constexpr double halfTick = 0.5 / 10000000.0;

    EXPECT_EQ(RationalTime::fromSeconds(halfTick).value(), 1);
    EXPECT_EQ(RationalTime::fromSeconds(-halfTick).value(), -1);
}

TEST(RationalTimeStandaloneTest, CopyConstructionAndAssignmentPreserveValues)
{
    const RationalTime original(-17, 24);
    RationalTime copied(original);
    RationalTime assigned(3, 5);
    assigned = original;

    EXPECT_EQ(copied, original);
    EXPECT_EQ(assigned, original);
    EXPECT_EQ(copied.value(), -17);
    EXPECT_EQ(copied.scale(), 24);
    EXPECT_EQ(assigned.value(), -17);
    EXPECT_EQ(assigned.scale(), 24);
}

TEST(RationalTimeStandaloneTest, NegativeScaleNormalizesWithoutChangingSignOfValue)
{
    const RationalTime negativeScale(7, -3);
    const RationalTime zeroScale(7, 0);

    EXPECT_EQ(negativeScale.value(), 7);
    EXPECT_EQ(negativeScale.scale(), 1);
    EXPECT_DOUBLE_EQ(negativeScale.toSeconds(), 7.0);
    EXPECT_EQ(zeroScale, RationalTime(7, 1));
}

TEST(RationalTimeStandaloneTest, RescalingUsesHalfAwayFromZeroAcrossScaleRatios)
{
    EXPECT_EQ(RationalTime(1, 4).rescaledTo(2), 1);
    EXPECT_EQ(RationalTime(-1, 4).rescaledTo(2), -1);
    EXPECT_EQ(RationalTime(1, 6).rescaledTo(3), 1);
    EXPECT_EQ(RationalTime(-1, 6).rescaledTo(3), -1);
    EXPECT_EQ(RationalTime(1, 10).rescaledTo(4), 0);
    EXPECT_EQ(RationalTime(-1, 10).rescaledTo(4), 0);
}

TEST(RationalTimeStandaloneTest, CrossScaleCancellationProducesCanonicalZero)
{
    const RationalTime first(7, 12);
    const RationalTime matching(21, 36);
    const auto difference = first - matching;
    const auto cancellation = first + RationalTime(-7, 12);

    EXPECT_EQ(difference, RationalTime(0, 1));
    EXPECT_EQ(cancellation, RationalTime(0, 1));
    EXPECT_EQ(difference.toSeconds(), 0.0);
    EXPECT_EQ(cancellation.toSeconds(), 0.0);
}

TEST(RationalTimeStandaloneTest, RationalFrameCountsRoundAcrossPositiveAndNegativeTies)
{
    EXPECT_EQ(RationalTime(1, 8).toFrameCount(4), 1);
    EXPECT_EQ(RationalTime(-1, 8).toFrameCount(4), -1);
    EXPECT_EQ(RationalTime(1, 10).toFrameCount(4), 0);
    EXPECT_EQ(RationalTime(-1, 10).toFrameCount(4), 0);
    EXPECT_EQ(RationalTime(1, 3).toFrameCount(24), 8);
    EXPECT_EQ(RationalTime(-1, 3).toFrameCount(24), -8);
}

TEST(RationalTimeStandaloneTest, AdditionThenSubtractionRecoversSignedInputs)
{
    for (std::int64_t lhsValue = -4; lhsValue <= 4; ++lhsValue) {
        for (std::int64_t rhsValue = -4; rhsValue <= 4; ++rhsValue) {
            for (std::int64_t lhsScale = 1; lhsScale <= 7; ++lhsScale) {
                for (std::int64_t rhsScale = 1; rhsScale <= 7; ++rhsScale) {
                    const RationalTime lhs(lhsValue, lhsScale);
                    const RationalTime rhs(rhsValue, rhsScale);
                    SCOPED_TRACE(::testing::Message()
                        << "lhs=" << lhsValue << '/' << lhsScale
                        << " rhs=" << rhsValue << '/' << rhsScale);
                    EXPECT_EQ((lhs + rhs) - rhs, lhs);
                }
            }
        }
    }
}

TEST(RationalTimeStandaloneTest, SecondsConversionMatchesSignedFractionGrid)
{
    for (std::int64_t value = -17; value <= 17; ++value) {
        for (std::int64_t scale = 1; scale <= 16; ++scale) {
            const RationalTime time(value, scale);
            const double expected = static_cast<double>(value) / static_cast<double>(scale);
            EXPECT_NEAR(time.toSeconds(), expected, 1e-15);
            EXPECT_NEAR(time.toDouble(), expected, 1e-15);
        }
    }
}
