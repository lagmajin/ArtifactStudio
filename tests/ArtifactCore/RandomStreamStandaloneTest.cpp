#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
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

TEST(RandomStreamStandaloneTest, NextU32UsesUpperHalfOfOneNextU64Draw)
{
    RandomStream wide(0xACEDu);
    RandomStream narrow(0xACEDu);

    for (int i = 0; i < 16; ++i) {
        const uint64_t value = wide.nextU64();
        EXPECT_EQ(narrow.nextU32(), static_cast<uint32_t>(value >> 32));
        EXPECT_EQ(narrow.state(), wide.state());
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

TEST(RandomStreamStandaloneTest, InclusiveRangeSupportsTheEntireIntDomain)
{
    RandomStream first(0xA55Au);
    RandomStream second(0xA55Au);
    constexpr int minValue = std::numeric_limits<int>::min();
    constexpr int maxValue = std::numeric_limits<int>::max();

    for (int i = 0; i < 64; ++i) {
        const int value = first.rangeInclusive(minValue, maxValue);
        EXPECT_GE(value, minValue);
        EXPECT_LE(value, maxValue);
        EXPECT_EQ(value, second.rangeInclusive(minValue, maxValue));
    }
}

TEST(RandomStreamStandaloneTest, ChanceBoundaryProbabilitiesAreCertain)
{
    RandomStream stream(2u);
    const auto state = stream.state();
    EXPECT_FALSE(stream.chance(0.0f));
    EXPECT_FALSE(stream.chance(-1.0f));
    EXPECT_FALSE(stream.chance(-std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(stream.chance(1.0f));
    EXPECT_TRUE(stream.chance(2.0f));
    EXPECT_TRUE(stream.chance(std::numeric_limits<float>::infinity()));
    EXPECT_EQ(stream.state(), state);
}

TEST(RandomStreamStandaloneTest, ChanceRejectsNaNProbability)
{
    RandomStream stream(2u);
    EXPECT_FALSE(stream.chance(std::numeric_limits<float>::quiet_NaN()));
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

TEST(RandomStreamStandaloneTest, GaussianNegativeDeviationMirrorsAroundTheMean)
{
    RandomStream positiveDeviation(0x51A7u);
    RandomStream negativeDeviation(0x51A7u);
    constexpr float mean = 3.25f;
    constexpr float deviation = 1.75f;

    for (int i = 0; i < 16; ++i) {
        const float positive = positiveDeviation.gaussian(mean, deviation);
        const float negative = negativeDeviation.gaussian(mean, -deviation);
        EXPECT_FLOAT_EQ(positive + negative, 2.0f * mean);
    }
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

TEST(RandomStreamStandaloneTest, PickReturnsReferencesToMoveOnlyElements)
{
    RandomStream stream(15u);
    std::vector<std::unique_ptr<int>> vectorValues;
    vectorValues.push_back(std::make_unique<int>(11));
    vectorValues.push_back(std::make_unique<int>(22));
    vectorValues.push_back(std::make_unique<int>(33));
    std::array<std::unique_ptr<int>, 2> arrayValues{
        std::make_unique<int>(44), std::make_unique<int>(55)};

    static_assert(std::is_same_v<decltype(stream.pick(vectorValues)),
                                 const std::unique_ptr<int>&>);
    static_assert(std::is_same_v<decltype(stream.pick(arrayValues)),
                                 const std::unique_ptr<int>&>);
    const auto& fromVector = stream.pick(vectorValues);
    const auto& fromArray = stream.pick(arrayValues);

    EXPECT_TRUE(&fromVector == &vectorValues[0] ||
                &fromVector == &vectorValues[1] ||
                &fromVector == &vectorValues[2]);
    EXPECT_TRUE(&fromArray == &arrayValues[0] ||
                &fromArray == &arrayValues[1]);
    EXPECT_NE(fromVector, nullptr);
    EXPECT_NE(fromArray, nullptr);
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

TEST(RandomStreamStandaloneTest, ShuffleDoesNotAdvanceStateForZeroOrOneElement)
{
    RandomStream stream(21u);
    std::vector<int> empty;
    std::vector<int> singleton{42};
    const auto initialState = stream.state();

    stream.shuffle(empty);
    EXPECT_EQ(stream.state(), initialState);
    stream.shuffle(singleton);
    EXPECT_EQ(stream.state(), initialState);
}

TEST(RandomStreamStandaloneTest, ShuffleIsReproducibleForEqualSeeds)
{
    RandomStream first(0xC0FFEEu);
    RandomStream second(0xC0FFEEu);
    const std::vector<int> original{0, 1, 2, 3, 4, 5, 6, 7};
    auto firstValues = original;
    auto secondValues = original;

    first.shuffle(firstValues);
    second.shuffle(secondValues);

    EXPECT_EQ(firstValues, secondValues);
    EXPECT_EQ(first.state(), second.state());
}
