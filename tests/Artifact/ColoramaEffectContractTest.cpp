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

TEST(ColoramaEffectContractTest, ColoramaHuePaletteMapsExpectedColorAndPreservesAlpha)
{
    const float pixels[] = {1.0f, 0.0f, 0.0f, 0.37f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ColoramaEffect effect;
    effect.setSourceMode(ColoramaSourceMode::Hue);
    effect.setPalette(ColoramaPalette::Rainbow);
    effect.setPhase(0.0f);
    effect.setSpread(1.0f);
    effect.setStrength(1.0f);
    effect.setSaturationBoost(1.0f);
    effect.setContrast(1.0f);
    effect.setPreserveLuma(false);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(1.0f, 0.2f, 0.2f, 0.37f));
}

TEST(ColoramaEffectContractTest, ColoramaPresetAndSetterBoundariesStayCoherent)
{
    ColoramaEffect effect;
    effect.setPreset(2);
    EXPECT_EQ(effect.preset(), 2);
    EXPECT_EQ(effect.settings().palette, ColoramaPalette::Ocean);
    EXPECT_FLOAT_EQ(effect.settings().saturationBoost, 1.05f);
    EXPECT_FLOAT_EQ(effect.settings().contrast, 0.95f);

    effect.setPhase(2.0f);
    effect.setSpread(-1.0f);
    effect.setStrength(std::numeric_limits<float>::quiet_NaN());
    effect.setSaturationBoost(3.0f);
    effect.setContrast(-1.0f);
    EXPECT_EQ(effect.preset(), 0);
    EXPECT_FLOAT_EQ(effect.settings().phase, 1.0f);
    EXPECT_FLOAT_EQ(effect.settings().spread, 0.0f);
    EXPECT_FLOAT_EQ(effect.settings().strength, 1.0f);
    EXPECT_FLOAT_EQ(effect.settings().saturationBoost, 2.5f);
    EXPECT_FLOAT_EQ(effect.settings().contrast, 0.0f);
}

TEST(ColoramaEffectContractTest, PresetSetterClampsToAvailablePresets)
{
    ColoramaEffect effect;
    effect.setPreset(-1);
    EXPECT_EQ(effect.preset(), 0);
    EXPECT_EQ(effect.settings().palette, ColoramaPalette::Rainbow);

    effect.setPreset(6);
    EXPECT_EQ(effect.preset(), 5);
    EXPECT_EQ(effect.settings().palette, ColoramaPalette::Sunset);
}

TEST(ColoramaEffectContractTest, ColoramaGpuDescriptorCarriesEveryProcessorSetting)
{
    ColoramaEffect effect;
    effect.setSourceMode(ColoramaSourceMode::Hue);
    effect.setPalette(ColoramaPalette::Neon);
    effect.setPhase(0.2f);
    effect.setSpread(1.5f);
    effect.setStrength(0.6f);
    effect.setSaturationBoost(1.2f);
    effect.setContrast(0.8f);
    effect.setPreserveLuma(false);
    GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey, effect.gpuGenericKey());
    EXPECT_EQ(stack.nodes[0].genericKey,
              gpuGenericKeyFromString("colorama"));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0],
                    static_cast<float>(ColoramaSourceMode::Hue));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1],
                    static_cast<float>(ColoramaPalette::Neon));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 0.2f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 1.5f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], 0.6f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[5], 1.2f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[6], 0.8f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[7], 0.0f);
}
