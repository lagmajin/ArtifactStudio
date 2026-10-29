#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Wave;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixels = std::array<float, 24>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 3, 2);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(WaveEffectContractTest, ZeroAmplitudePreservesPixelsAndAlpha)
{
    const Pixels pixels{
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 0.8f, 0.7f, 0.6f, 0.3f, 0.2f, 0.1f, 1.0f,
        0.2f, 0.4f, 0.6f, 0.8f, 0.8f, 0.6f, 0.4f, 0.2f,
    };
    Artifact::WaveEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setAmplitude(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel]) << "channel=" << channel;
    }
}

TEST(WaveEffectContractTest, SettersClampParametersAndEnumerations)
{
    Artifact::WaveEffect effect;
    effect.setAmplitude(5000.0f);
    EXPECT_FLOAT_EQ(effect.amplitude(), 4096.0f);
    effect.setFrequency(-20.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), -10.0f);
    effect.setWaveType(8);
    EXPECT_EQ(effect.waveType(), 1);
    effect.setOrientation(-2);
    EXPECT_EQ(effect.orientation(), 0);
}
