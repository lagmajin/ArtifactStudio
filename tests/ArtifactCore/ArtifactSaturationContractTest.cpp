#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

import Core.ArtifactSaturation;

using namespace ArtifactCore;

static_assert(addSat<std::int8_t>(120, 7) == 127);
static_assert(addSat<std::int8_t>(-120, -8) == -128);
static_assert(subSat<std::int8_t>(-120, 20) == -128);
static_assert(mulSat<std::int8_t>(-100, 2) == -128);
static_assert(mulSat<std::int8_t>(-100, -2) == 127);
static_assert(addSat<std::uint8_t>(250, 5) == 255);
static_assert(subSat<std::uint8_t>(0, 1) == 0);
static_assert(mulSat<std::int16_t>(-200, 2) == -400);
static_assert(saturateCast<std::int8_t>(std::int64_t{500}) == 127);
static_assert(saturateCast<std::uint8_t>(std::int64_t{-1}) == 0);
static_assert(saturateCast<std::int64_t>(std::uint64_t{0xFFFFFFFFFFFFFFFFull}) ==
              std::numeric_limits<std::int64_t>::max());

TEST(ArtifactSaturationContractTest, SignedAdditionClampsAtBothLimits)
{
    constexpr int high = std::numeric_limits<int>::max();
    constexpr int low = std::numeric_limits<int>::lowest();
    EXPECT_EQ(addSat(high, 1), high);
    EXPECT_EQ(addSat(low, -1), low);
    EXPECT_EQ(addSat(high - 1, 1), high);
    EXPECT_EQ(addSat(low + 1, -1), low);
    EXPECT_EQ(addSat(-20, 7), -13);
}

TEST(ArtifactSaturationContractTest, UnsignedAdditionClampsOnlyOnOverflow)
{
    constexpr unsigned high = std::numeric_limits<unsigned>::max();
    EXPECT_EQ(addSat(0u, 0u), 0u);
    EXPECT_EQ(addSat(high - 2u, 2u), high);
    EXPECT_EQ(addSat(high - 1u, 2u), high);
}

TEST(ArtifactSaturationContractTest, NarrowSignedAdditionHandlesExactAndAdjacentLimits)
{
    EXPECT_EQ(addSat<std::int8_t>(126, 1), 127);
    EXPECT_EQ(addSat<std::int8_t>(127, 0), 127);
    EXPECT_EQ(addSat<std::int8_t>(-127, -1), -128);
    EXPECT_EQ(addSat<std::int8_t>(-128, 0), -128);
    EXPECT_EQ(addSat<std::int8_t>(-128, 1), -127);
}

TEST(ArtifactSaturationContractTest, NarrowUnsignedAdditionSaturatesAtMaximum)
{
    EXPECT_EQ(addSat<std::uint8_t>(250, 5), 255);
    EXPECT_EQ(addSat<std::uint8_t>(250, 6), 255);
    EXPECT_EQ(addSat<std::uint8_t>(0, 255), 255);
}

TEST(ArtifactSaturationContractTest, SignedSubtractionClampsAtBothLimits)
{
    constexpr int high = std::numeric_limits<int>::max();
    constexpr int low = std::numeric_limits<int>::lowest();
    EXPECT_EQ(subSat(low, 1), low);
    EXPECT_EQ(subSat(high, -1), high);
    EXPECT_EQ(subSat(low + 1, 1), low);
    EXPECT_EQ(subSat(high - 1, -1), high);
    EXPECT_EQ(subSat(5, 8), -3);
}

TEST(ArtifactSaturationContractTest, UnsignedSubtractionFloorsAtZero)
{
    EXPECT_EQ(subSat(5u, 3u), 2u);
    EXPECT_EQ(subSat(3u, 3u), 0u);
    EXPECT_EQ(subSat(2u, 3u), 0u);
}

TEST(ArtifactSaturationContractTest, NarrowSubtractionHandlesSignedAndUnsignedBoundaries)
{
    EXPECT_EQ(subSat<std::int8_t>(-127, 1), -128);
    EXPECT_EQ(subSat<std::int8_t>(-128, 0), -128);
    EXPECT_EQ(subSat<std::int8_t>(126, -1), 127);
    EXPECT_EQ(subSat<std::uint8_t>(0, 0), 0);
    EXPECT_EQ(subSat<std::uint8_t>(255, 255), 0);
}

TEST(ArtifactSaturationContractTest, SignedMultiplicationClampsAllOverflowSigns)
{
    constexpr int high = std::numeric_limits<int>::max();
    constexpr int low = std::numeric_limits<int>::lowest();
    EXPECT_EQ(mulSat(high, 2), high);
    EXPECT_EQ(mulSat(low, 2), low);
    EXPECT_EQ(mulSat(high, -2), low);
    EXPECT_EQ(mulSat(low, -1), high);
    EXPECT_EQ(mulSat(-12, -3), 36);
    EXPECT_EQ(mulSat(-12, 0), 0);
}

TEST(ArtifactSaturationContractTest, UnsignedMultiplicationClampsAndHandlesZero)
{
    constexpr auto high = std::numeric_limits<std::uint64_t>::max();
    EXPECT_EQ(mulSat<std::uint64_t>(high, 1), high);
    EXPECT_EQ(mulSat<std::uint64_t>(high / 2, 2), (high / 2) * 2);
    EXPECT_EQ(mulSat<std::uint64_t>(high / 2 + 1, 2), high);
    EXPECT_EQ(mulSat<std::uint64_t>(high, 0), 0u);
}

TEST(ArtifactSaturationContractTest, NarrowMultiplicationClampsAtSignedLimits)
{
    EXPECT_EQ(mulSat<std::int8_t>(63, 2), 126);
    EXPECT_EQ(mulSat<std::int8_t>(64, 2), 127);
    EXPECT_EQ(mulSat<std::int8_t>(-64, 2), -128);
    EXPECT_EQ(mulSat<std::int8_t>(-65, 2), -128);
    EXPECT_EQ(mulSat<std::int8_t>(-8, -16), 127);
}

TEST(ArtifactSaturationContractTest, NarrowMultiplicationHandlesUnitAndNegativeOne)
{
    EXPECT_EQ(mulSat<std::int8_t>(-128, 1), -128);
    EXPECT_EQ(mulSat<std::int8_t>(-128, -1), 127);
    EXPECT_EQ(mulSat<std::uint8_t>(255, 1), 255);
    EXPECT_EQ(mulSat<std::uint8_t>(255, 0), 0);
}

TEST(ArtifactSaturationContractTest, SaturateCastPreservesValuesWhenTypesMatchOrWiden)
{
    EXPECT_EQ(saturateCast<int>(-17), -17);
    EXPECT_EQ(saturateCast<std::int64_t>(std::int8_t{-100}), -100);
    EXPECT_EQ(saturateCast<std::uint64_t>(std::uint32_t{400}), 400u);
}

TEST(ArtifactSaturationContractTest, SignedToUnsignedCastMapsNonpositiveToZeroAndClampsHigh)
{
    EXPECT_EQ(saturateCast<unsigned>(-1), 0u);
    EXPECT_EQ(saturateCast<unsigned>(0), 0u);
    EXPECT_EQ(saturateCast<unsigned>(42), 42u);
    EXPECT_EQ(saturateCast<std::uint8_t>(300), std::numeric_limits<std::uint8_t>::max());
}

TEST(ArtifactSaturationContractTest, UnsignedToSignedCastClampsAboveSignedMaximum)
{
    EXPECT_EQ(saturateCast<int>(42u), 42);
    EXPECT_EQ(saturateCast<std::int8_t>(std::uint8_t{127}), 127);
    EXPECT_EQ(saturateCast<std::int8_t>(std::uint8_t{128}), 127);
}

TEST(ArtifactSaturationContractTest, SignedNarrowingClampsBothDirections)
{
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{127}), 127);
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{128}), 127);
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{-128}), -128);
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{-129}), -128);
}

TEST(ArtifactSaturationContractTest, UnsignedNarrowingClampsAboveMaximum)
{
    EXPECT_EQ(saturateCast<std::uint8_t>(std::uint16_t{255}), 255);
    EXPECT_EQ(saturateCast<std::uint8_t>(std::uint16_t{256}), 255);
    EXPECT_EQ(saturateCast<std::uint8_t>(std::uint16_t{0}), 0);
}

TEST(ArtifactSaturationContractTest, CastHandlesFullWidthSignedAndUnsignedExtremes)
{
    constexpr auto signedHigh = std::numeric_limits<std::int64_t>::max();
    constexpr auto unsignedHigh = std::numeric_limits<std::uint64_t>::max();
    constexpr auto signedLow = std::numeric_limits<std::int64_t>::lowest();
    EXPECT_EQ(saturateCast<std::int64_t>(unsignedHigh), signedHigh);
    EXPECT_EQ(saturateCast<std::uint64_t>(signedLow), 0u);
    EXPECT_EQ(saturateCast<std::int64_t>(std::uint64_t{0}), 0);
}

TEST(ArtifactSaturationContractTest, CastPreservesValuesAtAdjacentNarrowingBounds)
{
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{126}), 126);
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{129}), 127);
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{-127}), -127);
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{-130}), -128);
    EXPECT_EQ(saturateCast<std::uint8_t>(std::uint16_t{254}), 254);
}

