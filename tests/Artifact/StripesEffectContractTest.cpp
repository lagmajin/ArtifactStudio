#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Stripes;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 8;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeSourceSurface()
{
    Pixels pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.2f;
        pixels[i + 1] = 0.4f;
        pixels[i + 2] = 0.8f;
        pixels[i + 3] = 0.3f;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void expectStripePixel(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                       int x, int y, float expected)
{
    const auto pixel = image.image().getPixel(x, y);
    EXPECT_FLOAT_EQ(pixel.r(), expected);
    EXPECT_FLOAT_EQ(pixel.g(), expected);
    EXPECT_FLOAT_EQ(pixel.b(), expected);
    EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
}

} // namespace

TEST(StripesEffectContractTest, AngleRotatesAlternatingPatternFromColumnsToRows)
{
    const auto source = makeSourceSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::StripesEffect effect;
    effect.setFrequency(4.0f);
    effect.setThickness(0.5f);
    effect.setOffset(0.0f);
    effect.setAngle(0.0f);

    effect.applyCPUOnly(source, output);

    expectStripePixel(output, 0, 2, 1.0f);
    expectStripePixel(output, 1, 2, 0.0f);
    expectStripePixel(output, 6, 7, 1.0f);

    effect.setAngle(90.0f);
    effect.applyCPUOnly(source, output);
    expectStripePixel(output, 2, 0, 1.0f);
    expectStripePixel(output, 2, 1, 0.0f);
}

TEST(StripesEffectContractTest, OffsetMovesThePeriodicPatternAndThicknessFillsIt)
{
    const auto source = makeSourceSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::StripesEffect effect;
    effect.setFrequency(4.0f);
    effect.setAngle(0.0f);
    effect.setThickness(0.25f);
    effect.setOffset(0.5f);

    effect.applyCPUOnly(source, output);

    expectStripePixel(output, 0, 0, 0.0f);
    expectStripePixel(output, 1, 0, 1.0f);
    expectStripePixel(output, 2, 0, 0.0f);

    effect.setThickness(1.0f);
    effect.applyCPUOnly(source, output);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            expectStripePixel(output, x, y, 1.0f);
        }
    }
}

TEST(StripesEffectContractTest, GpuDescriptorCarriesPatternParameters)
{
    Artifact::StripesEffect effect;
    effect.setFrequency(12.0f);
    effect.setAngle(37.0f);
    effect.setThickness(0.3f);
    effect.setOffset(-0.2f);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, Artifact::GpuSpatialEffectKind::Stripes);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 12.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 37.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 0.3f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], -0.2f);
}

TEST(StripesEffectContractTest, SettersClampAmountsAndResetNonFiniteValues)
{
    Artifact::StripesEffect effect;
    effect.setFrequency(0.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), 0.5f);
    effect.setFrequency(1024.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), 512.0f);
    effect.setFrequency(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.frequency(), 10.0f);

    effect.setThickness(-1.0f);
    EXPECT_FLOAT_EQ(effect.thickness(), 0.0f);
    effect.setThickness(2.0f);
    EXPECT_FLOAT_EQ(effect.thickness(), 1.0f);
    effect.setThickness(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.thickness(), 0.5f);

    effect.setAngle(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.angle(), 0.0f);
    effect.setAngle(450.0f);
    EXPECT_FLOAT_EQ(effect.angle(), 450.0f);
    effect.setOffset(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.offset(), 0.0f);
}
