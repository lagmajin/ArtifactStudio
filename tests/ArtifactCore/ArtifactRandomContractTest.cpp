#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <type_traits>

import Core.ArtifactRandom;

using ArtifactCore::ArtifactRandom;

static_assert(std::is_same_v<decltype(ArtifactRandom::between(1, 2)), int>);
static_assert(std::is_same_v<decltype(ArtifactRandom::between(1.0, 2.0)), double>);

TEST(ArtifactRandomContractTest, UnitSamplesStayInHalfOpenUnitInterval)
{
    for (int i = 0; i < 256; ++i) {
        const double value = ArtifactRandom::unit();
        EXPECT_GE(value, 0.0);
        EXPECT_LT(value, 1.0);
    }
}

TEST(ArtifactRandomContractTest, IntegerSamplesStayWithinInclusivePositiveBounds)
{
    constexpr int min = 3;
    constexpr int max = 11;
    for (int i = 0; i < 256; ++i) {
        const int value = ArtifactRandom::between(min, max);
        EXPECT_GE(value, min);
        EXPECT_LE(value, max);
    }
}

TEST(ArtifactRandomContractTest, IntegerSamplesStayWithinInclusiveNegativeBounds)
{
    constexpr int min = -17;
    constexpr int max = -4;
    for (int i = 0; i < 256; ++i) {
        const int value = ArtifactRandom::between(min, max);
        EXPECT_GE(value, min);
        EXPECT_LE(value, max);
    }
}

TEST(ArtifactRandomContractTest, IntegerSamplesSupportRangesAcrossZero)
{
    constexpr int min = -5;
    constexpr int max = 7;
    for (int i = 0; i < 256; ++i) {
        const int value = ArtifactRandom::between(min, max);
        EXPECT_GE(value, min);
        EXPECT_LE(value, max);
    }
}

TEST(ArtifactRandomContractTest, IntegerSingleValueRangeReturnsThatValue)
{
    for (int i = 0; i < 32; ++i)
        EXPECT_EQ(ArtifactRandom::between(-23, -23), -23);
}

TEST(ArtifactRandomContractTest, ZeroAndPositiveIntegerSingletonRangesReturnTheirValue)
{
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(ArtifactRandom::between(0, 0), 0);
        EXPECT_EQ(ArtifactRandom::between(7, 7), 7);
    }
}

TEST(ArtifactRandomContractTest, RealSamplesStayInHalfOpenPositiveRange)
{
    constexpr double min = 1.25;
    constexpr double max = 9.5;
    for (int i = 0; i < 256; ++i) {
        const double value = ArtifactRandom::between(min, max);
        EXPECT_GE(value, min);
        EXPECT_LT(value, max);
    }
}

TEST(ArtifactRandomContractTest, RealSamplesStayInHalfOpenNegativeRange)
{
    constexpr double min = -8.0;
    constexpr double max = -2.0;
    for (int i = 0; i < 256; ++i) {
        const double value = ArtifactRandom::between(min, max);
        EXPECT_GE(value, min);
        EXPECT_LT(value, max);
    }
}

TEST(ArtifactRandomContractTest, RealSamplesSupportRangeAcrossZero)
{
    constexpr double min = -3.5;
    constexpr double max = 4.25;
    for (int i = 0; i < 256; ++i) {
        const double value = ArtifactRandom::between(min, max);
        EXPECT_GE(value, min);
        EXPECT_LT(value, max);
    }
}

TEST(ArtifactRandomContractTest, RealSingleValueRangeReturnsThatValue)
{
    EXPECT_DOUBLE_EQ(ArtifactRandom::between(2.75, 2.75), 2.75);
}

TEST(ArtifactRandomContractTest, NextReturnsAnIntValue)
{
    EXPECT_NO_THROW(static_cast<void>(ArtifactRandom::next()));
}

TEST(ArtifactRandomContractTest, IntegerRangeSupportsMinimumRepresentableLowerBound)
{
    constexpr int min = std::numeric_limits<int>::min();
    constexpr int max = min + 2;
    for (int i = 0; i < 128; ++i) {
        const int value = ArtifactRandom::between(min, max);
        EXPECT_GE(value, min);
        EXPECT_LE(value, max);
    }
}

TEST(ArtifactRandomContractTest, IntegerRangeSupportsValuesNearMaximumWithoutOverflow)
{
    constexpr int max = std::numeric_limits<int>::max() - 1;
    constexpr int min = max - 2;
    for (int i = 0; i < 128; ++i) {
        const int value = ArtifactRandom::between(min, max);
        EXPECT_GE(value, min);
        EXPECT_LE(value, max);
    }
}

TEST(ArtifactRandomContractTest, IntegerMaximumSingletonRangeReturnsMaximum)
{
    constexpr int maximum = std::numeric_limits<int>::max();
    for (int i = 0; i < 32; ++i)
        EXPECT_EQ(ArtifactRandom::between(maximum, maximum), maximum);
}

TEST(ArtifactRandomContractTest, IntegerRangeSpanningRepresentableDomainStaysBounded)
{
    constexpr int min = std::numeric_limits<int>::min();
    constexpr int max = std::numeric_limits<int>::max() - 1;
    for (int i = 0; i < 256; ++i) {
        const int value = ArtifactRandom::between(min, max);
        EXPECT_GE(value, min);
        EXPECT_LE(value, max);
    }
}

TEST(ArtifactRandomContractTest, RealRangeSpanningFiniteExtremesStaysFiniteAndBounded)
{
    const double minimum = std::numeric_limits<double>::lowest();
    const double maximum = std::numeric_limits<double>::max();

    for (int i = 0; i < 32; ++i) {
        const double value = ArtifactRandom::between(minimum, maximum);
        EXPECT_TRUE(std::isfinite(value));
        EXPECT_GE(value, minimum);
        EXPECT_LT(value, maximum);
    }
}
