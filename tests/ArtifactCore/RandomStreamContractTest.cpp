#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

import Math.RandomStream;

namespace {

using ArtifactCore::RandomStream;

} // namespace

TEST(RandomStreamContractTest, SeedsReproduceSequencesAndZeroUsesDefaultSeed)
{
    RandomStream first{0x123456789abcdef0ull};
    RandomStream second{0x123456789abcdef0ull};
    RandomStream different{0x123456789abcdef1ull};
    bool differs = false;
    for (int i = 0; i < 32; ++i) {
        const auto value = first.nextU64();
        EXPECT_EQ(value, second.nextU64());
        differs = differs || value != different.nextU64();
    }
    EXPECT_TRUE(differs);

    RandomStream zero{0};
    RandomStream defaultSeed{RandomStream::defaultSeed()};
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(zero.nextU64(), defaultSeed.nextU64());
    }
}

TEST(RandomStreamContractTest, UnitAndRangeMethodsRespectHalfOpenAndInclusiveBounds)
{
    RandomStream stream{42};
    for (int i = 0; i < 256; ++i) {
        const float unit = stream.unitFloat();
        const double unitDouble = stream.unitDouble();
        EXPECT_GE(unit, 0.0f);
        EXPECT_LT(unit, 1.0f);
        EXPECT_GE(unitDouble, 0.0);
        EXPECT_LT(unitDouble, 1.0);

        const float ordered = stream.range(-3.0f, 7.0f);
        const float reversed = stream.range(7.0f, -3.0f);
        EXPECT_GE(ordered, -3.0f);
        EXPECT_LE(ordered, 7.0f);
        EXPECT_GE(reversed, -3.0f);
        EXPECT_LE(reversed, 7.0f);

        const int integer = stream.rangeInclusive(-5, 5);
        const int reversedInteger = stream.rangeInclusive(5, -5);
        EXPECT_GE(integer, -5);
        EXPECT_LE(integer, 5);
        EXPECT_GE(reversedInteger, -5);
        EXPECT_LE(reversedInteger, 5);
    }
    EXPECT_EQ(stream.rangeInclusive(9, 9), 9);
}

TEST(RandomStreamContractTest, BoundaryChancesDoNotConsumeRandomState)
{
    RandomStream stream{77};
    const auto initialState = stream.state();
    EXPECT_FALSE(stream.chance(0.0f));
    EXPECT_FALSE(stream.chance(-1.0f));
    EXPECT_TRUE(stream.chance(1.0f));
    EXPECT_TRUE(stream.chance(2.0f));
    EXPECT_EQ(stream.state(), initialState);

    (void)stream.chance(0.5f);
    RandomStream reference{77};
    (void)reference.unitFloat();
    EXPECT_EQ(stream.state(), reference.state());
    EXPECT_EQ(stream.nextU64(), reference.nextU64());
}

TEST(RandomStreamContractTest, ForksAreRepeatableAndLeaveTheParentUnchanged)
{
    RandomStream parent{1234};
    const auto stateBeforeFork = parent.state();
    auto first = parent.fork(99);
    auto repeat = parent.fork(99);
    auto other = parent.fork(100);
    EXPECT_EQ(parent.state(), stateBeforeFork);

    bool otherDiffers = false;
    for (int i = 0; i < 16; ++i) {
        const auto value = first.nextU64();
        EXPECT_EQ(value, repeat.nextU64());
        otherDiffers = otherDiffers || value != other.nextU64();
    }
    EXPECT_TRUE(otherDiffers);
}

TEST(RandomStreamContractTest, ShuffleAndPickStayWithinTheirInputCollections)
{
    const std::vector<int> input{0, 1, 2, 3, 4, 5, 6, 7};
    auto first = input;
    auto repeat = input;
    RandomStream a{2026};
    RandomStream b{2026};
    a.shuffle(first);
    b.shuffle(repeat);
    EXPECT_EQ(first, repeat);
    auto sorted = first;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, input);

    const int& selected = a.pick(input);
    EXPECT_NE(std::find(input.begin(), input.end(), selected), input.end());
    const std::array<int, 4> fixed{11, 22, 33, 44};
    const int& fixedSelected = a.pick(fixed);
    EXPECT_NE(std::find(fixed.begin(), fixed.end(), fixedSelected), fixed.end());
}

TEST(RandomStreamContractTest, JitterAndGaussianHelpersStayFiniteAndRespectScale)
{
    RandomStream stream{808};
    EXPECT_FLOAT_EQ(stream.gaussian(3.0f, 0.0f), 3.0f);
    for (int i = 0; i < 128; ++i) {
        const float signedValue = stream.signedUnit();
        EXPECT_GE(signedValue, -1.0f);
        EXPECT_LT(signedValue, 1.0f);

        const auto jitter2 = stream.jitter2D(2.0f);
        EXPECT_LE(std::abs(jitter2.x), 2.0f);
        EXPECT_LE(std::abs(jitter2.y), 2.0f);

        const auto jitter3 = stream.jitter3D(2.0f);
        EXPECT_LE(std::abs(jitter3.x), 2.0f);
        EXPECT_LE(std::abs(jitter3.y), 2.0f);
        EXPECT_LE(std::abs(jitter3.z), 2.0f);
    }
}

TEST(RandomStreamContractTest, VectorLerpClampsInterpolationParameter)
{
    const ArtifactCore::float2 a2{0.0f, 10.0f};
    const ArtifactCore::float2 b2{20.0f, 30.0f};
    EXPECT_EQ(ArtifactCore::float2::lerp(a2, b2, -1.0f), a2);
    EXPECT_EQ(ArtifactCore::float2::lerp(a2, b2, 0.5f),
              ArtifactCore::float2(10.0f, 20.0f));
    EXPECT_EQ(ArtifactCore::float2::lerp(a2, b2, 2.0f), b2);

    const ArtifactCore::float3 a3{1.0f, 2.0f, 3.0f};
    const ArtifactCore::float3 b3{5.0f, 6.0f, 7.0f};
    EXPECT_EQ(ArtifactCore::float3::lerp(a3, b3, 0.5f),
              ArtifactCore::float3(3.0f, 4.0f, 5.0f));
}
