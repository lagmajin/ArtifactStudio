#include <gtest/gtest.h>

#include <QColor>

import Artifact.Effect.Abstract;
import ColorTintEffect;
import FloatRGBA;
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

void expectPixelNear(const ImageF32x4RGBAWithCache& surface, int x, int y,
                     const FloatRGBA& expected, float tolerance = 1.0e-6f)
{
    const FloatRGBA actual = surface.image().getPixel(x, y);
    EXPECT_NEAR(actual.r(), expected.r(), tolerance);
    EXPECT_NEAR(actual.g(), expected.g(), tolerance);
    EXPECT_NEAR(actual.b(), expected.b(), tolerance);
    EXPECT_NEAR(actual.a(), expected.a(), tolerance);
}

} // namespace

TEST(ColorTintEffectContractTest, MapsMidtonesBetweenBlackAndWhiteColors)
{
    const float pixels[] = {0.5f, 0.5f, 0.5f, 0.35f};
    const auto source = makeSurface(pixels, 1, 1);
    ImageF32x4RGBAWithCache output;
    ColorTintEffect effect;
    effect.setMapBlackTo(QColor::fromRgbF(1.0, 0.0, 0.0));
    effect.setMapWhiteTo(QColor::fromRgbF(0.0, 0.0, 1.0));

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.5f, 0.0f, 0.5f, 0.35f),
                    1.0e-4f);
}

TEST(ColorTintEffectContractTest, MapsBlackAndWhiteLumaEndpointsAndKeepsAlpha)
{
    const float pixels[] = {0.0f, 0.0f, 0.0f, 0.0f,
                            1.0f, 1.0f, 1.0f, 0.63f};
    const auto source = makeSurface(pixels, 2, 1);
    ImageF32x4RGBAWithCache output;
    ColorTintEffect effect;
    effect.setMapBlackTo(QColor::fromRgbF(0.8, 0.1, 0.2));
    effect.setMapWhiteTo(QColor::fromRgbF(0.1, 0.3, 0.9));

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.8f, 0.1f, 0.2f, 0.0f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.1f, 0.3f, 0.9f, 0.63f));
}

TEST(ColorTintEffectContractTest, TintAmountBlendsMappedColorWithOriginal)
{
    const float pixels[] = {1.0f, 0.0f, 0.0f, 0.7f};
    const auto source = makeSurface(pixels, 1, 1);
    ImageF32x4RGBAWithCache output;
    ColorTintEffect effect;
    effect.setMapBlackTo(QColor::fromRgbF(0.0, 0.0, 0.0));
    effect.setMapWhiteTo(QColor::fromRgbF(0.0, 1.0, 0.0));
    effect.setAmountToTint(0.5f);

    effect.applyCPUOnly(source, output);

    constexpr float luma = 0.299f;
    expectPixelNear(output, 0, 0,
                    FloatRGBA(1.0f - 0.5f * luma, 0.5f * luma, 0.0f, 0.7f),
                    1.0e-4f);
    expectPixelNear(source, 0, 0, FloatRGBA(1.0f, 0.0f, 0.0f, 0.7f));
}

TEST(ColorTintEffectContractTest, ZeroAmountPreservesHdrRgbAndSetterClamps)
{
    const float pixels[] = {1.4f, -0.2f, 0.6f, 0.4f};
    const auto source = makeSurface(pixels, 1, 1);
    ImageF32x4RGBAWithCache output;
    ColorTintEffect effect;
    effect.setAmountToTint(0.0f);
    effect.applyCPUOnly(source, output);
    expectPixelNear(output, 0, 0, FloatRGBA(1.4f, -0.2f, 0.6f, 0.4f));

    effect.setAmountToTint(-1.0f);
    EXPECT_FLOAT_EQ(effect.amountToTint(), 0.0f);
    effect.setAmountToTint(2.0f);
    EXPECT_FLOAT_EQ(effect.amountToTint(), 1.0f);
}

TEST(ColorTintEffectContractTest, DisabledEffectPreservesHdrAndTransparentPixelsExactly)
{
    const float pixels[] = {1.4f, -0.2f, 0.6f, 0.0f,
                            0.3f, 0.5f, 0.7f, 0.42f};
    const auto source = makeSurface(pixels, 2, 1);
    ImageF32x4RGBAWithCache output;
    ColorTintEffect effect;
    effect.setMapBlackTo(QColor::fromRgbF(1.0, 0.0, 0.0));
    effect.setMapWhiteTo(QColor::fromRgbF(0.0, 0.0, 1.0));
    effect.setAmountToTint(1.0f);
    effect.setEnabled(false);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(1.4f, -0.2f, 0.6f, 0.0f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.3f, 0.5f, 0.7f, 0.42f));
}

TEST(ColorTintEffectContractTest, GpuSpatialDescriptorCarriesColorsAndAmount)
{
    ColorTintEffect effect;
    effect.setMapBlackTo(QColor::fromRgbF(0.1, 0.2, 0.3));
    effect.setMapWhiteTo(QColor::fromRgbF(0.7, 0.8, 0.9));
    effect.setAmountToTint(0.4f);
    GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey,
              gpuGenericKeyFromString("color_tint"));
    EXPECT_NEAR(stack.nodes[0].parameters[0], 0.1f, 1.0e-4f);
    EXPECT_NEAR(stack.nodes[0].parameters[1], 0.2f, 1.0e-4f);
    EXPECT_NEAR(stack.nodes[0].parameters[2], 0.3f, 1.0e-4f);
    EXPECT_NEAR(stack.nodes[0].parameters[3], 0.7f, 1.0e-4f);
    EXPECT_NEAR(stack.nodes[0].parameters[4], 0.8f, 1.0e-4f);
    EXPECT_NEAR(stack.nodes[0].parameters[5], 0.9f, 1.0e-4f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[6], 0.4f);
}
