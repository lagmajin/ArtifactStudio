#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <QColor>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.InnerShadow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

ArtifactCore::ImageF32x4RGBAWithCache makeTwoAlphaPixels()
{
    const std::array<float, 8> pixels{
        0.8f, 0.4f, 0.2f, 0.5f,
        0.3f, 0.5f, 0.7f, 0.0f,
    };
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(InnerShadowEffectContractTest, MasksShadowBySourceAlphaAndPreservesOutputAlpha)
{
    const auto source = makeTwoAlphaPixels();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::InnerShadowEffect effect;
    effect.setShadowColor(QColor(0, 0, 0, 255));
    effect.setDistance(0.0f);
    effect.setAngle(0.0f);
    effect.setSoftness(0.0f);
    effect.setOpacity(100.0f);

    effect.applyCPUOnly(source, output);

    const auto halfAlpha = output.image().getPixel(0, 0);
    EXPECT_NEAR(halfAlpha.r(), 0.8f * 0.75f, 1.0e-6f);
    EXPECT_NEAR(halfAlpha.g(), 0.4f * 0.75f, 1.0e-6f);
    EXPECT_NEAR(halfAlpha.b(), 0.2f * 0.75f, 1.0e-6f);
    EXPECT_FLOAT_EQ(halfAlpha.a(), 0.5f);

    const auto transparent = output.image().getPixel(1, 0);
    EXPECT_FLOAT_EQ(transparent.r(), 0.0f);
    EXPECT_FLOAT_EQ(transparent.g(), 0.0f);
    EXPECT_FLOAT_EQ(transparent.b(), 0.0f);
    EXPECT_FLOAT_EQ(transparent.a(), 0.0f);
}

TEST(InnerShadowEffectContractTest, GpuDescriptorCarriesOffsetColorAndOpacity)
{
    Artifact::InnerShadowEffect effect;
    effect.setShadowColor(QColor(64, 128, 192, 128));
    effect.setDistance(3.0f);
    effect.setAngle(0.0f);
    effect.setSoftness(2.0f);
    effect.setOpacity(40.0f);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, Artifact::GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey, Artifact::InnerShadowEffect::kGpuGenericKey);
    EXPECT_NEAR(stack.nodes[0].parameters[0], -3.0f, 1.0e-6f);
    EXPECT_NEAR(stack.nodes[0].parameters[1], 0.0f, 1.0e-6f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 2.0f);
    EXPECT_NEAR(stack.nodes[0].parameters[3], 0.4f * (128.0f / 255.0f), 1.0e-6f);
    EXPECT_NEAR(stack.nodes[0].parameters[4], 64.0f / 255.0f, 1.0e-6f);
    EXPECT_NEAR(stack.nodes[0].parameters[5], 128.0f / 255.0f, 1.0e-6f);
    EXPECT_NEAR(stack.nodes[0].parameters[6], 192.0f / 255.0f, 1.0e-6f);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1) | (1u << 2));

    effect.setSoftness(13.0f);
    EXPECT_FALSE(effect.appendGpuSpatialNodes(stack));
}

TEST(InnerShadowEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::InnerShadowEffect effect;
    effect.setDistance(-1.0f);
    EXPECT_FLOAT_EQ(effect.distance(), 0.0f);
    effect.setDistance(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.distance(), 5.0f);

    effect.setSoftness(-1.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 0.0f);
    effect.setSoftness(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.softness(), 8.0f);

    effect.setOpacity(-1.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 0.0f);
    effect.setOpacity(120.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 100.0f);
    effect.setOpacity(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.opacity(), 75.0f);
}
