#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

import Core.ArtifactSaturation;

using namespace ArtifactCore;

TEST(ArtifactSaturationTest, AvoidsIntegralOverflow)
{
    EXPECT_EQ(addSat<int>(std::numeric_limits<int>::max(), 1),
              std::numeric_limits<int>::max());
    EXPECT_EQ(subSat<int>(std::numeric_limits<int>::lowest(), 1),
              std::numeric_limits<int>::lowest());
    EXPECT_EQ(mulSat<int>(std::numeric_limits<int>::max(), 2),
              std::numeric_limits<int>::max());
    EXPECT_EQ(saturateCast<unsigned char>(-1), 0);
}

TEST(ArtifactSaturationTest, SignedMultiplicationSaturatesAtBothExtremes)
{
    constexpr int minimum = std::numeric_limits<int>::lowest();
    constexpr int maximum = std::numeric_limits<int>::max();

    EXPECT_EQ(mulSat<int>(minimum, -1), maximum);
    EXPECT_EQ(mulSat<int>(minimum, 2), minimum);
    EXPECT_EQ(mulSat<int>(maximum, -2), minimum);
    EXPECT_EQ(mulSat<int>(-2, -3), 6);
}

TEST(ArtifactSaturationTest, SaturateCastHandlesCrossSignWidthBoundaries)
{
    constexpr auto signedMinimum = std::numeric_limits<std::int64_t>::lowest();
    constexpr auto signedMaximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto unsignedMaximum = std::numeric_limits<std::uint64_t>::max();

    EXPECT_EQ(saturateCast<std::uint64_t>(signedMinimum), 0u);
    EXPECT_EQ(saturateCast<std::uint64_t>(signedMaximum),
              static_cast<std::uint64_t>(signedMaximum));
    EXPECT_EQ(saturateCast<std::int64_t>(unsignedMaximum), signedMaximum);
}

TEST(ArtifactSaturationTest, UnsignedArithmeticSaturatesOverflowAndUnderflow)
{
    constexpr auto maximum = std::numeric_limits<std::uint8_t>::max();

    EXPECT_EQ(addSat<std::uint8_t>(maximum, 1), maximum);
    EXPECT_EQ(subSat<std::uint8_t>(0, 1), 0);
    EXPECT_EQ(mulSat<std::uint8_t>(maximum, 2), maximum);
    EXPECT_EQ(addSat<std::uint8_t>(40, 2), 42);
    EXPECT_EQ(subSat<std::uint8_t>(42, 40), 2);
    EXPECT_EQ(mulSat<std::uint8_t>(6, 7), 42);
}

TEST(ArtifactSaturationTest, SaturateCastClampsNarrowSignedAndUnsignedTargets)
{
    EXPECT_EQ(saturateCast<std::int8_t>(std::uint8_t{255}),
              std::numeric_limits<std::int8_t>::max());
    EXPECT_EQ(saturateCast<std::int8_t>(std::uint8_t{127}), 127);
    EXPECT_EQ(saturateCast<std::uint8_t>(std::int16_t{-1}), 0);
    EXPECT_EQ(saturateCast<std::uint8_t>(std::int16_t{256}),
              std::numeric_limits<std::uint8_t>::max());
}

TEST(ArtifactSaturationTest, SaturateCastClampsSameSignNarrowing)
{
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{-129}),
              std::numeric_limits<std::int8_t>::lowest());
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{128}),
              std::numeric_limits<std::int8_t>::max());
    EXPECT_EQ(saturateCast<std::int8_t>(std::int16_t{-128}), -128);
    EXPECT_EQ(saturateCast<std::uint8_t>(std::uint16_t{256}),
              std::numeric_limits<std::uint8_t>::max());
    EXPECT_EQ(saturateCast<std::uint8_t>(std::uint16_t{255}), 255);
}
