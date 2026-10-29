#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>

import Artifact.Effect.Wave;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(WaveEffectContractTest, WaveUsesCosineOffsetAndClampsAtImageEdge)
{
    constexpr std::array<float, 20> pixels = {
        0.0f, 0.1f, 0.2f, 1.0f,
        0.2f, 0.3f, 0.4f, 1.0f,
        0.4f, 0.5f, 0.6f, 1.0f,
        0.6f, 0.7f, 0.8f, 1.0f,
        0.8f, 0.9f, 1.0f, 1.0f,
    };
    const auto source = makeSurface(pixels.data(), 5, 1);
    ImageF32x4RGBAWithCache output;
    WaveEffect effect;
    effect.setAmplitude(1.0f);
    effect.setFrequency(0.0f);
    effect.setPhase(0.0f);
    effect.setWaveType(1);
    effect.setOrientation(0);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(0, 0).r(),
                source.image().getPixel(1, 0).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(3, 0).g(),
                source.image().getPixel(4, 0).g(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(4, 0).b(),
                source.image().getPixel(4, 0).b(), 1.0e-6f);
    EXPECT_FLOAT_EQ(source.image().getPixel(0, 0).r(), 0.0f);
}

TEST(WaveEffectContractTest, VerticalSineUsesRowCoordinateForOffset)
{
    constexpr std::array<float, 16> pixels = {
        0.0f, 0.1f, 0.2f, 0.2f,
        0.2f, 0.3f, 0.4f, 0.4f,
        0.4f, 0.5f, 0.6f, 0.6f,
        0.6f, 0.7f, 0.8f, 0.8f,
    };
    const auto source = makeSurface(pixels.data(), 1, 4);
    ImageF32x4RGBAWithCache output;
    WaveEffect effect;
    effect.setAmplitude(1.0f);
    effect.setFrequency(0.0f);
    effect.setPhase(1.57079632679f);
    effect.setWaveType(0);
    effect.setOrientation(1);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(0, 0).r(),
                source.image().getPixel(0, 1).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(0, 2).g(),
                source.image().getPixel(0, 3).g(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(0, 3).b(),
                source.image().getPixel(0, 3).b(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(0, 1).a(),
                source.image().getPixel(0, 2).a(), 1.0e-6f);
}

TEST(WaveEffectContractTest, NumericSettingsClampAndResetNonFiniteValues)
{
    WaveEffect effect;
    effect.setAmplitude(5000.0f);
    effect.setFrequency(-20.0f);
    effect.setWaveType(4);
    effect.setOrientation(-1);
    EXPECT_FLOAT_EQ(effect.amplitude(), 4096.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), -10.0f);
    EXPECT_EQ(effect.waveType(), 1);
    EXPECT_EQ(effect.orientation(), 0);

    effect.setAmplitude(std::numeric_limits<float>::infinity());
    effect.setFrequency(std::numeric_limits<float>::quiet_NaN());
    effect.setPhase(std::numeric_limits<float>::quiet_NaN());
    effect.setOrientation(4);
    EXPECT_FLOAT_EQ(effect.amplitude(), 10.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), 0.1f);
    EXPECT_FLOAT_EQ(effect.phase(), 0.0f);
    EXPECT_EQ(effect.orientation(), 1);
}
\n
