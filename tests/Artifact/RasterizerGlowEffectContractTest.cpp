#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Glow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixels = std::array<float, 36>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 3, 3);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(RasterizerGlowEffectContractTest, ZeroIntensityPreservesAllChannels)
{
    const Pixels pixels{
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 0.8f, 0.7f, 0.6f,
        0.3f, 0.2f, 0.1f, 1.0f, 0.9f, 0.1f, 0.4f, 0.3f, 0.7f, 0.6f, 0.5f, 0.4f,
        0.5f, 0.5f, 0.5f, 0.5f, 0.2f, 0.3f, 0.4f, 0.6f, 0.8f, 0.7f, 0.6f, 0.2f,
    };
    Artifact::RasterizerGlowEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setIntensity(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel]) << "channel=" << channel;
    }
}

TEST(RasterizerGlowEffectContractTest, BrightPointAddsRgbWithoutChangingAlpha)
{
    Pixels pixels{};
    for (std::size_t index = 0; index < pixels.size(); index += 4) {
        pixels[index + 3] = 0.4f;
    }
    const std::size_t center = (4u * 3u + 1u) * 4u;
    pixels[center + 0] = 1.0f;
    pixels[center + 1] = 1.0f;
    pixels[center + 2] = 1.0f;

    Artifact::RasterizerGlowEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setThreshold(0.1f);
    effect.setRadius(1.0f);
    effect.setIntensity(2.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    const std::size_t darkPixel = (4u * 3u + 0u) * 4u;
    EXPECT_GT(actual[darkPixel + 0], pixels[darkPixel + 0]);
    EXPECT_GT(actual[darkPixel + 1], pixels[darkPixel + 1]);
    EXPECT_GT(actual[darkPixel + 2], pixels[darkPixel + 2]);
    for (std::size_t index = 3; index < pixels.size(); index += 4) {
        EXPECT_FLOAT_EQ(actual[index], pixels[index]);
    }
}
