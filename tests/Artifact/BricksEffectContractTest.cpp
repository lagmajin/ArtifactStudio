#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Bricks;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 16;
constexpr int kHeight = 8;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeTransparentInput()
{
    Pixels pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.25f;
        pixels[i + 1] = 0.5f;
        pixels[i + 2] = 0.75f;
        pixels[i + 3] = 0.2f;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void expectGrayPixel(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                     int x, int y, float expected)
{
    const auto pixel = image.image().getPixel(x, y);
    EXPECT_FLOAT_EQ(pixel.r(), expected);
    EXPECT_FLOAT_EQ(pixel.g(), expected);
    EXPECT_FLOAT_EQ(pixel.b(), expected);
    EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
}

} // namespace

TEST(BricksEffectContractTest, HalfOffsetAlternatesRowsAndWritesOpaquePattern)
{
    const auto source = makeTransparentInput();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::BricksEffect effect;
    effect.setBrickWidth(8.0f);
    effect.setBrickHeight(4.0f);
    effect.setMortarWidth(1.0f);
    effect.setOffset(0.5f);

    effect.applyCPUOnly(source, output);

    expectGrayPixel(output, 4, 1, 1.0f);
    expectGrayPixel(output, 0, 1, 0.0f);
    expectGrayPixel(output, 4, 5, 0.0f);
    expectGrayPixel(output, 0, 5, 1.0f);
}

TEST(BricksEffectContractTest, GpuDescriptorCarriesPatternParametersAndScaleMask)
{
    Artifact::BricksEffect effect;
    effect.setBrickWidth(24.0f);
    effect.setBrickHeight(12.0f);
    effect.setMortarWidth(2.0f);
    effect.setOffset(0.25f);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, Artifact::GpuSpatialEffectKind::Bricks);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 24.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 12.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 2.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 0.25f);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1) | (1u << 2));
}

TEST(BricksEffectContractTest, SettersEnforceMinimumsAndResetNonFiniteValues)
{
    Artifact::BricksEffect effect;
    effect.setBrickWidth(1.0f);
    EXPECT_FLOAT_EQ(effect.brickWidth(), 8.0f);
    effect.setBrickWidth(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.brickWidth(), 64.0f);

    effect.setBrickHeight(-1.0f);
    EXPECT_FLOAT_EQ(effect.brickHeight(), 4.0f);
    effect.setBrickHeight(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.brickHeight(), 32.0f);

    effect.setMortarWidth(-1.0f);
    EXPECT_FLOAT_EQ(effect.mortarWidth(), 0.0f);
    effect.setMortarWidth(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.mortarWidth(), 3.0f);

    effect.setOffset(-1.0f);
    EXPECT_FLOAT_EQ(effect.offset(), 0.0f);
    effect.setOffset(2.0f);
    EXPECT_FLOAT_EQ(effect.offset(), 1.0f);
    effect.setOffset(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.offset(), 0.5f);
}
