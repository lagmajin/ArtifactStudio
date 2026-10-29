#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Sharpen;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 9;
constexpr int kHeight = 9;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeImpulseSurface()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            const float value = (x == kWidth / 2 && y == kHeight / 2)
                ? 0.8f : 0.5f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 0.37f;
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(SharpenEffectContractTest, EnhancesImpulseAndPreservesAlpha)
{
    const auto source = makeImpulseSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::SharpenEffect effect;
    effect.setAmount(1.0f);
    effect.setSigma(1.0f);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(kWidth / 2, kHeight / 2);
    const auto neighbor = output.image().getPixel(kWidth / 2 - 1, kHeight / 2);
    EXPECT_GT(center.r(), 0.8f);
    EXPECT_GT(center.g(), 0.8f);
    EXPECT_GT(center.b(), 0.8f);
    EXPECT_LT(neighbor.r(), 0.5f);
    EXPECT_LT(neighbor.g(), 0.5f);
    EXPECT_LT(neighbor.b(), 0.5f);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            EXPECT_FLOAT_EQ(output.image().getPixel(x, y).a(), 0.37f);
        }
    }
}

TEST(SharpenEffectContractTest, HighThresholdProtectsTheInputImage)
{
    const auto source = makeImpulseSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::SharpenEffect effect;
    effect.setAmount(1.0f);
    effect.setSigma(1.0f);
    effect.setThreshold(1.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const float expected = (x == kWidth / 2 && y == kHeight / 2)
                ? 0.8f : 0.5f;
            const auto actual = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), expected);
            EXPECT_FLOAT_EQ(actual.g(), expected);
            EXPECT_FLOAT_EQ(actual.b(), expected);
            EXPECT_FLOAT_EQ(actual.a(), 0.37f);
        }
    }
}

TEST(SharpenEffectContractTest, GpuSpatialDescriptorCarriesSettings)
{
    Artifact::SharpenEffect effect;
    effect.setAmount(2.5f);
    effect.setSigma(1.25f);
    effect.setThreshold(0.2f);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, Artifact::GpuSpatialEffectKind::Sharpen);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 2.5f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 1.25f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 0.2f);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 1);
}

TEST(SharpenEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::SharpenEffect effect;
    effect.setAmount(-1.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 0.0f);
    effect.setAmount(12.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 10.0f);
    effect.setAmount(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.amount(), 1.0f);

    effect.setSigma(-1.0f);
    EXPECT_FLOAT_EQ(effect.sigma(), 0.0f);
    effect.setSigma(12.0f);
    EXPECT_FLOAT_EQ(effect.sigma(), 10.0f);
    effect.setSigma(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.sigma(), 1.0f);

    effect.setThreshold(-1.0f);
    EXPECT_FLOAT_EQ(effect.threshold(), 0.0f);
    effect.setThreshold(2.0f);
    EXPECT_FLOAT_EQ(effect.threshold(), 1.0f);
    effect.setThreshold(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.threshold(), 0.0f);
}
