#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

import Frame.Rate;

using ArtifactCore::FrameRate;

TEST(FrameRateStorageScaleTest, RoundsPositiveRatesToNearestIntegralScale)
{
    EXPECT_EQ(FrameRate::storageScaleForFps(23.976), 24);
    EXPECT_EQ(FrameRate::storageScaleForFps(29.97), 30);
    EXPECT_EQ(FrameRate::storageScaleForFps(24.5), 25);
    EXPECT_EQ(FrameRate::storageScaleForFps(24.49), 24);
}

TEST(FrameRateStorageScaleTest, RoundingChangesAtExactHalfRateBoundary)
{
    const double tie = 24.5;
    const double justBelow = std::nextafter(tie, 24.0);
    const double justAbove = std::nextafter(tie, 25.0);

    EXPECT_EQ(FrameRate::storageScaleForFps(justBelow), 24);
    EXPECT_EQ(FrameRate::storageScaleForFps(tie), 25);
    EXPECT_EQ(FrameRate::storageScaleForFps(justAbove), 25);
}

TEST(FrameRateStorageScaleTest, HalfBoundarySweepAcrossNominalRates)
{
    for (int nominalRate = 1; nominalRate <= 120; ++nominalRate) {
        const double tie = static_cast<double>(nominalRate) + 0.5;
        const double justBelow = std::nextafter(
            tie, static_cast<double>(nominalRate));
        const double justAbove = std::nextafter(
            tie, static_cast<double>(nominalRate + 1));

        SCOPED_TRACE(::testing::Message() << "nominalRate=" << nominalRate);
        EXPECT_EQ(FrameRate::storageScaleForFps(justBelow), nominalRate);
        EXPECT_EQ(FrameRate::storageScaleForFps(tie), nominalRate + 1);
        EXPECT_EQ(FrameRate::storageScaleForFps(justAbove), nominalRate + 1);
    }
}

TEST(FrameRateStorageScaleTest, PositiveRatesBelowHalfStillProduceAtLeastOne)
{
    EXPECT_EQ(FrameRate::storageScaleForFps(0.01), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(0.49), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(0.5), 1);
}

TEST(FrameRateStorageScaleTest, InvalidRatesUseCallerFallback)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    EXPECT_EQ(FrameRate::storageScaleForFps(0.0, 48), 48);
    EXPECT_EQ(FrameRate::storageScaleForFps(-24.0, 48), 48);
    EXPECT_EQ(FrameRate::storageScaleForFps(nan, 48), 48);
    EXPECT_EQ(FrameRate::storageScaleForFps(infinity, 48), 48);
    EXPECT_EQ(FrameRate::storageScaleForFps(-infinity, 48), 48);
}

TEST(FrameRateStorageScaleTest, InvalidRatesDefaultToThirty)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    EXPECT_EQ(FrameRate::storageScaleForFps(0.0), 30);
    EXPECT_EQ(FrameRate::storageScaleForFps(-1.0), 30);
    EXPECT_EQ(FrameRate::storageScaleForFps(nan), 30);
    EXPECT_EQ(FrameRate::storageScaleForFps(infinity), 30);
    EXPECT_EQ(FrameRate::storageScaleForFps(-infinity), 30);
}

TEST(FrameRateStorageScaleTest, InvalidFallbackIsClampedToOne)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();

    EXPECT_EQ(FrameRate::storageScaleForFps(0.0, 0), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(0.0, -30), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(0.0, minimum), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(nan, 0), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(nan, maximum), maximum);
}

TEST(FrameRateStorageScaleTest, LargeRepresentableRateRemainsExact)
{
    constexpr double rate = 9007199254740000.0;

    EXPECT_EQ(FrameRate::storageScaleForFps(rate), 9007199254740000LL);
}

TEST(FrameRateStorageScaleTest, LargestRepresentableDoubleBelowInt64LimitStaysInRange)
{
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const double belowLimit = std::nextafter(
        static_cast<double>(maximum), 0.0);
    const auto expected = static_cast<std::int64_t>(belowLimit);

    EXPECT_LT(expected, maximum);
    EXPECT_EQ(FrameRate::storageScaleForFps(belowLimit), expected);
}

TEST(FrameRateStorageScaleTest, RatesAtOrBeyondInt64LimitSaturateToMaximum)
{
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const double roundedLimit = static_cast<double>(maximum);
    const double largestFinite = std::numeric_limits<double>::max();

    EXPECT_EQ(FrameRate::storageScaleForFps(roundedLimit), maximum);
    EXPECT_EQ(FrameRate::storageScaleForFps(roundedLimit, 48), maximum);
    EXPECT_EQ(FrameRate::storageScaleForFps(largestFinite), maximum);
    EXPECT_EQ(FrameRate::storageScaleForFps(largestFinite, 48), maximum);
}

TEST(FrameRateStorageScaleTest, FiniteRateGridMatchesRoundedAndClampedReference)
{
    constexpr std::int64_t fallback = 17;
    for (int eighths = -40; eighths <= 800; ++eighths) {
        const double fps = static_cast<double>(eighths) / 8.0;
        const auto expected = fps <= 0.0
            ? fallback
            : std::max<std::int64_t>(
                  1, static_cast<std::int64_t>(std::llround(fps)));

        SCOPED_TRACE(::testing::Message() << "fps=" << fps);
        EXPECT_EQ(FrameRate::storageScaleForFps(fps, fallback), expected);
    }
}
