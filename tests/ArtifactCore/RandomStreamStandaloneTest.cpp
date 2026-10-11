#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

import Math.Random;

using namespace ArtifactCore;

TEST(RandomStreamStandaloneTest, EqualSeedsProduceEqualSequences)
{
    RandomStream a(123456u);
    RandomStream b(123456u);
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(a.nextU64(), b.nextU64());
    }
}

TEST(RandomStreamStandaloneTest, ReseedingRestoresTheSequence)
{
    RandomStream stream(17u);
    const auto first = stream.nextU64();
    static_cast<void>(stream.nextU64());
    stream.seed(17u);
    EXPECT_EQ(stream.nextU64(), first);
}

TEST(RandomStreamStandaloneTest, ZeroSeedUsesTheSameSequenceAsDefaultSeed)
{
    RandomStream zero(0u);
    RandomStream defaultSeed(RandomStream::defaultSeed());
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(zero.nextU64(), defaultSeed.nextU64());
    }
}

TEST(RandomStreamStandaloneTest, IntegerWidthsAreDeterministicAndConsistent)
{
    RandomStream a(9u);
    RandomStream b(9u);
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(a.nextU32(), b.nextU32());
    }
}

TEST(RandomStreamStandaloneTest, UnitValuesStayInHalfOpenIntervals)
{
    RandomStream stream(44u);
    for (int i = 0; i < 200; ++i) {
        const float f = stream.unitFloat();
        const double d = stream.unitDouble();
        EXPECT_GE(f, 0.0f);
        EXPECT_LT(f, 1.0f);
        EXPECT_GE(d, 0.0);
        EXPECT_LT(d, 1.0);
    }
}

TEST(RandomStreamStandaloneTest, ReversedFloatBoundsAreNormalized)
{
    RandomStream stream(5u);
    for (int i = 0; i < 100; ++i) {
        const float value = stream.range(8.0f, -2.0f);
        EXPECT_GE(value, -2.0f);
        EXPECT_LT(value, 8.0f);
    }
}

TEST(RandomStreamStandaloneTest, ReversedIntegerBoundsRemainInclusive)
{
    RandomStream stream(5u);
    for (int i = 0; i < 300; ++i) {
        const int value = stream.rangeInclusive(7, -4);
        EXPECT_GE(value, -4);
        EXPECT_LE(value, 7);
    }
}

TEST(RandomStreamStandaloneTest, IntegerSingletonRangeReturnsItsOnlyValue)
{
    RandomStream stream(3u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(stream.rangeInclusive(-11, -11), -11);
    }
}

TEST(RandomStreamStandaloneTest, ChanceBoundaryProbabilitiesAreCertain)
{
    RandomStream stream(2u);
    const auto state = stream.state();
    EXPECT_FALSE(stream.chance(0.0f));
    EXPECT_FALSE(stream.chance(-1.0f));
    EXPECT_TRUE(stream.chance(1.0f));
    EXPECT_TRUE(stream.chance(2.0f));
    EXPECT_EQ(stream.state(), state);
}

TEST(RandomStreamStandaloneTest, ForksAreRepeatableAndDoNotAdvanceParent)
{
    const RandomStream parent(100u);
    const auto parentState = parent.state();
    RandomStream first = parent.fork(7u);
    RandomStream second = parent.fork(7u);
    EXPECT_EQ(first.nextU64(), second.nextU64());
    EXPECT_EQ(parent.state(), parentState);
}

TEST(RandomStreamStandaloneTest, DifferentForkSaltsProduceDifferentInitialValues)
{
    const RandomStream parent(100u);
    EXPECT_NE(parent.fork(1u).nextU64(), parent.fork(2u).nextU64());
}

TEST(RandomStreamStandaloneTest, SignedAndMagnitudeRangesRespectTheirBounds)
{
    RandomStream stream(31u);
    for (int i = 0; i < 100; ++i) {
        const float signedValue = stream.signedUnit();
        const float rangedValue = stream.signedRange(3.0f);
        EXPECT_GE(signedValue, -1.0f);
        EXPECT_LT(signedValue, 1.0f);
        EXPECT_GE(rangedValue, -3.0f);
        EXPECT_LT(rangedValue, 3.0f);
    }
}

TEST(RandomStreamStandaloneTest, GaussianSamplingIsSeedReproducible)
{
    RandomStream a(71u);
    RandomStream b(71u);
    for (int i = 0; i < 20; ++i) {
        EXPECT_FLOAT_EQ(a.gaussian(), b.gaussian());
    }
}

TEST(RandomStreamStandaloneTest, GaussianParametersApplyMeanAndDeviation)
{
    RandomStream stream(12u);
    EXPECT_FLOAT_EQ(stream.gaussian(4.0f, 0.0f), 4.0f);
}

TEST(RandomStreamStandaloneTest, JitterComponentsStayWithinMagnitude)
{
    RandomStream stream(99u);
    for (int i = 0; i < 100; ++i) {
        const float2 j2 = stream.jitter2D(2.5f);
        const float3 j3 = stream.jitter3D(2.5f);
        EXPECT_GE(j2.x, -2.5f);
        EXPECT_LT(j2.x, 2.5f);
        EXPECT_GE(j2.y, -2.5f);
        EXPECT_LT(j2.y, 2.5f);
        EXPECT_GE(j3.x, -2.5f);
        EXPECT_LT(j3.x, 2.5f);
        EXPECT_GE(j3.y, -2.5f);
        EXPECT_LT(j3.y, 2.5f);
        EXPECT_GE(j3.z, -2.5f);
        EXPECT_LT(j3.z, 2.5f);
    }
}

TEST(RandomStreamStandaloneTest, PickSelectsOnlyAnExistingElement)
{
    RandomStream stream(8u);
    const std::vector<int> vectorValues{10, 20, 30, 40};
    const std::array<int, 3> arrayValues{2, 5, 9};
    for (int i = 0; i < 50; ++i) {
        const int fromVector = stream.pick(vectorValues);
        const int fromArray = stream.pick(arrayValues);
        EXPECT_NE(std::find(vectorValues.begin(), vectorValues.end(), fromVector), vectorValues.end());
        EXPECT_NE(std::find(arrayValues.begin(), arrayValues.end(), fromArray), arrayValues.end());
    }
}

TEST(RandomStreamStandaloneTest, ShufflePreservesElementsAndSingletons)
{
    RandomStream stream(21u);
    std::vector<int> values{0, 1, 2, 3, 4, 5};
    stream.shuffle(values);
    std::sort(values.begin(), values.end());
    EXPECT_EQ(values, (std::vector<int>{0, 1, 2, 3, 4, 5}));

    std::vector<int> singleton{42};
    stream.shuffle(singleton);
    EXPECT_EQ(singleton, (std::vector<int>{42}));
}
