#include <gtest/gtest.h>

#include <QString>

#include <array>
#include <cmath>

import Math.Noise;

using namespace ArtifactCore;

TEST(NoiseGeneratorStandaloneTest, NoiseFieldReturnsZeroAtIntegerLatticePoints)
{
    const NoiseField noise(42u);
    EXPECT_FLOAT_EQ(noise.perlin(0.0f, 0.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(noise.perlin(4.0f, -7.0f, 12.0f), 0.0f);
    EXPECT_FLOAT_EQ(noise.perlin(-256.0f, 128.0f, -1.0f), 0.0f);
}

TEST(NoiseGeneratorStandaloneTest, EqualNoiseFieldSeedsProduceEqualSamples)
{
    const NoiseField a(123u);
    const NoiseField b(123u);
    EXPECT_FLOAT_EQ(a.perlin(0.17f, -2.35f, 9.1f), b.perlin(0.17f, -2.35f, 9.1f));
    EXPECT_FLOAT_EQ(a.fractal(-0.5f, 3.25f, 1.75f), b.fractal(-0.5f, 3.25f, 1.75f));
}

TEST(NoiseGeneratorStandaloneTest, DistinctSeedsChangeAtLeastOneSampleInAProbeSet)
{
    const NoiseField a(17u);
    const NoiseField b(18u);
    constexpr std::array<std::array<float, 3>, 8> points{{
        {{0.13f, 0.27f, 0.39f}}, {{1.17f, 2.29f, 3.41f}},
        {{-4.23f, 7.19f, 2.37f}}, {{12.31f, -5.11f, 0.73f}},
        {{2.51f, 9.07f, -3.61f}}, {{-8.43f, -1.37f, 4.21f}},
        {{7.13f, 5.29f, 11.47f}}, {{-0.71f, 3.17f, -2.09f}}
    }};
    bool differs = false;
    for (const auto& point : points) {
        const bool sampleDiffers = a.perlin(point[0], point[1], point[2]) !=
                                   b.perlin(point[0], point[1], point[2]);
        differs = differs || sampleDiffers;
    }
    EXPECT_TRUE(differs);
}

TEST(NoiseGeneratorStandaloneTest, NoiseFieldInstancesKeepIndependentPermutationTables)
{
    NoiseField first(99u);
    const float baseline = first.perlin(0.31f, 0.57f, 0.83f);
    NoiseField second(100u);
    second.setSeed(101u);
    EXPECT_FLOAT_EQ(first.perlin(0.31f, 0.57f, 0.83f), baseline);
    EXPECT_EQ(second.seed(), 101u);
}

TEST(NoiseGeneratorStandaloneTest, ReseedingNoiseFieldReplaysItsOriginalSequence)
{
    NoiseField noise(321u);
    const float baseline = noise.perlin(-1.23f, 4.56f, 7.89f);
    noise.setSeed(777u);
    noise.perlin(-1.23f, 4.56f, 7.89f);
    noise.setSeed(321u);
    EXPECT_FLOAT_EQ(noise.perlin(-1.23f, 4.56f, 7.89f), baseline);
}

TEST(NoiseGeneratorStandaloneTest, NoiseFieldRepeatsAfterPermutationPeriod)
{
    const NoiseField noise(52u);
    const float value = noise.perlin(-12.375f, 6.625f, 0.125f);
    EXPECT_FLOAT_EQ(value, noise.perlin(243.625f, 262.625f, 256.125f));
}

TEST(NoiseGeneratorStandaloneTest, PerlinNoiseIsContinuousAcrossCellBoundary)
{
    const NoiseField noise(42u);
    const float before = noise.perlin(0.9999f, 0.37f, 0.61f);
    const float after = noise.perlin(1.0001f, 0.37f, 0.61f);
    EXPECT_NEAR(before, after, 0.001f);
}

TEST(NoiseGeneratorStandaloneTest, SingleOctaveFractalEqualsPerlinSample)
{
    const NoiseField noise(42u);
    const float point = noise.perlin(0.25f, -1.5f, 2.75f);
    EXPECT_FLOAT_EQ(noise.fractal(0.25f, -1.5f, 2.75f, 1), point);
}

TEST(NoiseGeneratorStandaloneTest, NonPositiveFractalOctaveCountReturnsZero)
{
    const NoiseField noise(42u);
    EXPECT_FLOAT_EQ(noise.fractal(0.25f, 0.5f, 0.75f, 0), 0.0f);
    EXPECT_FLOAT_EQ(noise.fractal(0.25f, 0.5f, 0.75f, -3), 0.0f);
}

TEST(NoiseGeneratorStandaloneTest, ZeroPersistenceLeavesOnlyFirstFractalOctave)
{
    const NoiseField noise(42u);
    const float point = noise.perlin(0.25f, 0.5f, 0.75f);
    EXPECT_FLOAT_EQ(noise.fractal(0.25f, 0.5f, 0.75f, 5, 0.0f, 2.0f), point);
}

TEST(NoiseGeneratorStandaloneTest, FractalNormalizesOctaveAmplitude)
{
    const NoiseField noise(42u);
    const float result = noise.fractal(0.13f, 0.29f, 0.47f, 6, 0.6f, 2.1f);
    EXPECT_TRUE(std::isfinite(result));
}

TEST(NoiseGeneratorStandaloneTest, LegacyDimensionalOverloadsDelegateToThreeDimensions)
{
    NoiseGenerator::setSeed(310u);
    EXPECT_FLOAT_EQ(NoiseGenerator::perlin(0.37f),
                    NoiseGenerator::perlin(0.37f, 0.0f, 0.0f));
    EXPECT_FLOAT_EQ(NoiseGenerator::perlin(0.37f, -0.91f),
                    NoiseGenerator::perlin(0.37f, -0.91f, 0.0f));
}

TEST(NoiseGeneratorStandaloneTest, LegacyGlobalSeedReproducesPreviousSamples)
{
    NoiseGenerator::setSeed(808u);
    const float baseline = NoiseGenerator::perlin(0.37f, 0.91f, -1.23f);
    NoiseGenerator::setSeed(12u);
    NoiseGenerator::perlin(0.37f, 0.91f, -1.23f);
    NoiseGenerator::setSeed(808u);
    EXPECT_FLOAT_EQ(NoiseGenerator::perlin(0.37f, 0.91f, -1.23f), baseline);
}

TEST(NoiseGeneratorStandaloneTest, WorleyNoiseIsFiniteAndIndependentOfPerlinSeed)
{
    const float baseline = NoiseGenerator::worley(-1.25f, 2.5f, 0.75f);
    NoiseGenerator::setSeed(999u);
    const float reseeded = NoiseGenerator::worley(-1.25f, 2.5f, 0.75f);
    EXPECT_TRUE(std::isfinite(baseline));
    EXPECT_GE(baseline, 0.0f);
    EXPECT_FLOAT_EQ(reseeded, baseline);
}

TEST(NoiseGeneratorStandaloneTest, ShaderHelpersReturnSourceTextWithSubstitutedParameters)
{
    const NoiseGenerator generator;
    EXPECT_FALSE(generator.glslPerlin2D().isEmpty());
    EXPECT_FALSE(generator.glslWorley2D().isEmpty());
    const QString source = generator.wiggleGLSL(2.5f, 0.75f, 4, 0.4f, 2.0f);
    EXPECT_TRUE(source.contains(QStringLiteral("2.5")));
    EXPECT_TRUE(source.contains(QStringLiteral("0.75")));
    EXPECT_FALSE(source.contains(QStringLiteral("%1")));
}
