#include <gtest/gtest.h>

#include <QVariant>
#include <QColor>

#include <cmath>
#include <cstdint>
#include <limits>

import FloatRGBA;
import Artifact.Effect.Abstract;
import BrightnessEffect;
import ChannelMixerEffect;
import ColoramaEffect;
import ColorBalanceEffect;
import ColorWheelsEffect;
import CurvesEffect;
import FillEffect;
import GradientRampEffect;
import GrayscaleEffect;
import HueAndSaturation;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import ImageProcessing.ColorTransform.Colorama;
import ImageProcessing.ColorTransform.SelectiveColor;
import InvertEffect;
import LevelsEffect;
import Math.Vec;
import PhotoFilterEffect;
import Artifact.Render.PointwiseEffectFusion;
import SelectiveColorEffect;
import ShadowHighlightEffect;
import TritoneEffect;
import Utils.String.UniString;
import Graphics.SurfaceColorContract;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(int width, int height, const float* rgba)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

void expectPixelNear(const ImageF32x4RGBAWithCache& surface, int x, int y,
                     const FloatRGBA& expected, float tolerance = 1e-6f)
{
    const FloatRGBA actual = surface.image().getPixel(x, y);
    EXPECT_NEAR(actual.r(), expected.r(), tolerance);
    EXPECT_NEAR(actual.g(), expected.g(), tolerance);
    EXPECT_NEAR(actual.b(), expected.b(), tolerance);
    EXPECT_NEAR(actual.a(), expected.a(), tolerance);
}

FloatRGBA pixel(const ImageF32x4RGBAWithCache& surface, int x, int y)
{
    return surface.image().getPixel(x, y);
}

} // namespace

TEST(ColorBalanceEffectContractTest, ColorBalanceAppliesShadowMidtoneAndHighlightBands)
{
    const float pixels[] = {
        0.33f, 0.33f, 0.33f, 0.2f,
        0.50f, 0.50f, 0.50f, 0.5f,
        0.90f, 0.90f, 0.90f, 0.8f,
    };
    ImageF32x4_RGBA image;
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    image.setFromRGBA32F(pixels, 3, 1, descriptor);
    const auto source = ImageF32x4RGBAWithCache(image);
    ImageF32x4RGBAWithCache output;
    ColorBalanceEffect effect;
    effect.setPreset(1);
    effect.setShadowBalance(0.2f, 0.0f, 0.0f);
    effect.setMidtoneBalance(0.0f, 0.1f, 0.0f);
    effect.setHighlightBalance(0.0f, 0.0f, 0.3f);
    effect.setMasterStrength(0.5f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.38f, 0.33f, 0.33f, 0.2f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.50f, 0.55f, 0.50f, 0.5f));
    expectPixelNear(output, 2, 0, FloatRGBA(0.90f, 0.90f, 1.0f, 0.8f));
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
}

TEST(ColorBalanceEffectContractTest, ColorBalancePreserveLumaKeepsInputLuminanceWhenUnclipped)
{
    const float pixels[] = {0.30f, 0.40f, 0.50f, 0.65f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ColorBalanceEffect effect;
    effect.setPreset(1);
    effect.setShadowBalance(0.2f, -0.1f, 0.05f);
    effect.setMidtoneBalance(0.1f, 0.15f, -0.2f);
    effect.setHighlightBalance(-0.1f, 0.05f, 0.1f);
    effect.setPreserveLuma(true);

    effect.applyCPUOnly(source, output);

    const FloatRGBA before = pixel(source, 0, 0);
    const FloatRGBA after = pixel(output, 0, 0);
    const float sourceLuma = 0.2126f * before.r() + 0.7152f * before.g() + 0.0722f * before.b();
    const float outputLuma = 0.2126f * after.r() + 0.7152f * after.g() + 0.0722f * after.b();
    EXPECT_NEAR(outputLuma, sourceLuma, 1e-5f);
    EXPECT_NE(after.r(), before.r());
    EXPECT_NEAR(after.a(), before.a(), 1e-6f);
}

TEST(ColorBalanceEffectContractTest, ColorBalancePremultipliedPixelsAreUnpremultipliedAndTransparentRgbIsUntouched)
{
    const float pixels[] = {
        1.2f, -0.2f, 0.3f, 0.0f,
        0.1f, 0.2f, 0.3f, 0.5f,
    };
    ImageF32x4_RGBA image;
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    image.setFromRGBA32F(pixels, 2, 1, descriptor);
    const auto source = ImageF32x4RGBAWithCache(image);
    ImageF32x4RGBAWithCache output;
    ColorBalanceEffect effect;
    effect.setPreset(1);
    effect.setShadowRange(0.1f);
    effect.setHighlightRange(0.9f);
    effect.setMidtoneBalance(0.0f, 0.1f, 0.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(1.2f, -0.2f, 0.3f, 0.0f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.1f, 0.25f, 0.3f, 0.5f));
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
}

TEST(ColorBalanceEffectContractTest, ColorBalanceZeroStrengthIsAnExactCopyIncludingHdrAndDescriptor)
{
    const float pixels[] = {1.25f, -0.20f, 0.35f, 0.0f};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 1, 1, descriptor);
    const auto source = ImageF32x4RGBAWithCache(image);
    ImageF32x4RGBAWithCache output;
    ColorBalanceEffect effect;
    effect.setPreset(4);
    effect.setMasterStrength(0.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(1.25f, -0.20f, 0.35f, 0.0f));
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
}

TEST(ColorBalanceEffectContractTest, ColorBalanceGpuDescriptorCarriesBandParametersAndLumaMode)
{
    ColorBalanceEffect effect;
    effect.setPreset(1);
    effect.setShadowBalance(0.1f, 0.2f, 0.3f);
    effect.setMidtoneBalance(0.4f, 0.5f, 0.6f);
    effect.setHighlightBalance(0.7f, 0.8f, 0.9f);
    effect.setShadowRange(0.25f);
    effect.setHighlightRange(0.75f);
    effect.setMasterStrength(0.5f);
    effect.setPreserveLuma(true);

    GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey, effect.gpuGenericKey());
    const float expected[] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f,
        0.7f, 0.8f, 0.9f, 0.25f, 0.75f, 0.5f, 1.0f,
    };
    for (size_t index = 0; index < std::size(expected); ++index) {
        EXPECT_FLOAT_EQ(stack.nodes[0].parameters[index], expected[index])
            << "parameter slot " << index;
    }
}
