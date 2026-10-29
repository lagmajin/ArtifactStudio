#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Vignette;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;

ArtifactCore::ImageF32x4RGBAWithCache makeSolidSurface(
    const ArtifactCore::FloatRGBA& color)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = color.r();
        pixels[i + 1] = color.g();
        pixels[i + 2] = color.b();
        pixels[i + 3] = color.a();
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(VignetteEffectContractTest, DarkensCornersAndPreservesCenterAndAlpha)
{
    const ArtifactCore::FloatRGBA sourceColor(0.8f, 0.4f, 0.2f, 0.37f);
    const auto source = makeSolidSurface(sourceColor);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::VignetteEffect effect;

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(2, 2);
    EXPECT_NEAR(center.r(), sourceColor.r(), 1.0e-6f);
    EXPECT_NEAR(center.g(), sourceColor.g(), 1.0e-6f);
    EXPECT_NEAR(center.b(), sourceColor.b(), 1.0e-6f);
    EXPECT_FLOAT_EQ(center.a(), sourceColor.a());

    const auto corner = output.image().getPixel(0, 0);
    EXPECT_NEAR(corner.r(), sourceColor.r() * 0.3f, 1.0e-6f);
    EXPECT_NEAR(corner.g(), sourceColor.g() * 0.3f, 1.0e-6f);
    EXPECT_NEAR(corner.b(), sourceColor.b() * 0.3f, 1.0e-6f);
    EXPECT_FLOAT_EQ(corner.a(), sourceColor.a());
}

TEST(VignetteEffectContractTest, GpuSpatialDescriptorCarriesConfiguration)
{
    Artifact::VignetteEffect effect;
    effect.setAmount(0.25f);
    effect.setRadius(1.5f);
    effect.setFeather(0.3f);
    effect.setCenterX(0.2f);
    effect.setCenterY(0.7f);
    ArtifactCore::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, ArtifactCore::GpuSpatialEffectKind::Vignette);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 0.25f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 1.5f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 0.3f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 0.2f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], 0.7f);

    effect.setRadius(0.0f);
    EXPECT_FALSE(effect.appendGpuSpatialNodes(stack));
}

TEST(VignetteEffectContractTest, SettersClampFiniteValuesAndResetNonFiniteValues)
{
    Artifact::VignetteEffect effect;
    effect.setAmount(-1.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 0.0f);
    effect.setAmount(2.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 1.0f);
    effect.setAmount(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.amount(), 0.7f);

    effect.setRadius(-1.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.0f);
    effect.setRadius(3.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 2.0f);
    effect.setRadius(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.radius(), 0.8f);

    effect.setFeather(0.0f);
    EXPECT_FLOAT_EQ(effect.feather(), 0.01f);
    effect.setFeather(3.0f);
    EXPECT_FLOAT_EQ(effect.feather(), 2.0f);
    effect.setFeather(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.feather(), 0.4f);

    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    effect.setCenterY(2.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);
    effect.setCenterY(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);
}
