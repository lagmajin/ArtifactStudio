#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.PinchBulge;
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

TEST(PinchBulgeEffectContractTest, ZeroAmountPreservesImageAndAlpha)
{
    Pixels pixels{};
    for (std::size_t pixel = 0; pixel < 9; ++pixel) {
        const float value = static_cast<float>(pixel) / 8.0f;
        pixels[pixel * 4] = value;
        pixels[pixel * 4 + 1] = 1.0f - value;
        pixels[pixel * 4 + 2] = value * 0.5f;
        pixels[pixel * 4 + 3] = 0.1f + value * 0.8f;
    }
    Artifact::PinchBulgeEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setAmount(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel]) << "channel=" << channel;
    }
}

TEST(PinchBulgeEffectContractTest, SettersClampNormalizedGeometryAndAmount)
{
    Artifact::PinchBulgeEffect effect;
    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterY(2.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);
    effect.setRadius(2.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 1.0f);
    effect.setRadius(-1.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.0f);
    effect.setAmount(200.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 100.0f);
    effect.setAmount(-200.0f);
    EXPECT_FLOAT_EQ(effect.amount(), -100.0f);
}
