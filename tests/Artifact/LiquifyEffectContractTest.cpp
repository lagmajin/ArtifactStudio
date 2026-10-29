#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Liquify;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixels = std::array<float, 64>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 4, 4);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(LiquifyEffectContractTest, ZeroAmountPreservesAllPixels)
{
    Pixels pixels{};
    for (std::size_t pixel = 0; pixel < 16; ++pixel) {
        pixels[pixel * 4] = static_cast<float>(pixel) / 15.0f;
        pixels[pixel * 4 + 1] = 0.25f;
        pixels[pixel * 4 + 2] = 1.0f - static_cast<float>(pixel) / 15.0f;
        pixels[pixel * 4 + 3] = 0.2f + static_cast<float>(pixel) * 0.05f;
    }
    Artifact::LiquifyEffect effect;
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

TEST(LiquifyEffectContractTest, SettersClampBrushAndGeometryParameters)
{
    Artifact::LiquifyEffect effect;
    effect.setBrushType(static_cast<Artifact::LiquifyBrushType>(99));
    EXPECT_EQ(static_cast<int>(effect.brushType()), 5);
    effect.setAmount(200.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 100.0f);
    effect.setRadius(0.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.01f);
    effect.setRadius(2.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 1.0f);
    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterY(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);
    effect.setTurbulenceSeed(20000);
    EXPECT_EQ(effect.turbulenceSeed(), 9999);
    effect.setMeshDensity(1);
    EXPECT_EQ(effect.meshDensity(), 4);
}
