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
import Artifact.Effect.WhiteBalance;
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

TEST(WhiteBalanceEffectContractTest, WhiteBalanceUnpremultipliesCorrectsAndRepremultipliesSafely)
{
    const float pixels[] = {
        1.2f, -0.2f, 0.3f, 0.0f,
        0.1f, 0.15f, 0.2f, 0.5f,
    };
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 2, 1,
                         SurfaceColorDescriptor::canonicalLinearPremultiplied());
    const auto source = ImageF32x4RGBAWithCache(image);
    ImageF32x4RGBAWithCache output;
    WhiteBalanceEffect effect;
    effect.setTemperature(6500.0f);
    effect.setTint(0.5f);
    effect.setBrightness(1.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(1.2f, -0.2f, 0.3f, 0.0f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.15f, 0.375f, 0.3f, 0.5f));
    expectPixelNear(source, 1, 0, FloatRGBA(0.1f, 0.15f, 0.2f, 0.5f));
    EXPECT_EQ(output.image().colorDescriptor(), image.colorDescriptor());
}

TEST(WhiteBalanceEffectContractTest, WhiteBalanceSettersClampAndResetNonFiniteValues)
{
    WhiteBalanceEffect effect;
    effect.setTemperature(500.0f);
    EXPECT_FLOAT_EQ(effect.temperature(), 1000.0f);
    effect.setTemperature(30000.0f);
    EXPECT_FLOAT_EQ(effect.temperature(), 20000.0f);
    effect.setTemperature(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.temperature(), 6500.0f);

    effect.setTint(-2.0f);
    EXPECT_FLOAT_EQ(effect.tint(), -1.0f);
    effect.setTint(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.tint(), 0.0f);
    effect.setBrightness(2.0f);
    EXPECT_FLOAT_EQ(effect.brightness(), 1.0f);
    effect.setBrightness(-2.0f);
    EXPECT_FLOAT_EQ(effect.brightness(), -1.0f);
}

TEST(WhiteBalanceEffectContractTest, PresetPropertyUpdatesTemperatureAndPreservesOtherAdjustments)
{
    WhiteBalanceEffect effect;
    effect.setTint(0.35f);
    effect.setBrightness(-0.25f);
    effect.setPropertyValue(UniString(QStringLiteral("Preset")), QVariant(4));

    EXPECT_FLOAT_EQ(effect.temperature(), 3200.0f);
    EXPECT_FLOAT_EQ(effect.tint(), 0.35f);
    EXPECT_FLOAT_EQ(effect.brightness(), -0.25f);

    effect.setPropertyValue(UniString(QStringLiteral("Preset")), QVariant(0));
    effect.setPropertyValue(UniString(QStringLiteral("Preset")), QVariant(99));
    EXPECT_FLOAT_EQ(effect.temperature(), 3200.0f);
    EXPECT_FLOAT_EQ(effect.tint(), 0.35f);
    EXPECT_FLOAT_EQ(effect.brightness(), -0.25f);
}

TEST(WhiteBalanceEffectContractTest, WhiteBalanceGpuPlanCarriesTemperatureTintAndBrightness)
{
    WhiteBalanceEffect effect;
    effect.setTemperature(7200.0f);
    effect.setTint(-0.25f);
    effect.setBrightness(0.5f);
    GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey, WhiteBalanceEffect::kGpuGenericKey);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 7200.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], -0.25f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 0.5f);
}
