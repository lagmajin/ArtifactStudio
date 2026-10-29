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

TEST(PhotoFilterEffectContractTest, PhotoFilterAppliesTintDensityWithoutChangingAlpha)
{
    const float pixels[] = {0.8f, 0.4f, 0.2f, 0.63f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    PhotoFilterEffect effect;
    effect.setColor(QColor(0, 255, 255));
    effect.setDensity(1.0f);
    effect.setBrightness(0.0f);
    effect.setContrast(1.0f);
    effect.setSaturationBoost(1.0f);
    effect.setPreserveLuma(false);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.0f, 0.4f, 0.2f, 0.63f));
}


TEST(PhotoFilterEffectContractTest, ZeroDensityPreservesEntireImage)
{
    constexpr float pixels[] = {
        0.1f, 0.2f, 0.3f, 0.0f,
        0.2f, 0.4f, 0.6f, 0.25f,
        0.3f, 0.5f, 0.7f, 0.5f,
        0.4f, 0.6f, 0.8f, 0.75f,
        0.5f, 0.7f, 0.9f, 1.0f,
        0.6f, 0.8f, 1.0f, 0.4f,
    };
    const auto source = makeSurface(3, 2, pixels);
    ImageF32x4RGBAWithCache output;
    PhotoFilterEffect effect;
    effect.setColor(QColor(255, 0, 255));
    effect.setDensity(0.0f);
    effect.setBrightness(0.0f);
    effect.setContrast(1.0f);
    effect.setSaturationBoost(1.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < std::size(pixels); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(PhotoFilterEffectContractTest, SelectingPresetReplacesPreviousCustomSettings)
{
    PhotoFilterEffect effect;
    effect.setColor(QColor(255, 0, 255));
    effect.setDensity(0.9f);
    effect.setBrightness(0.4f);
    effect.setContrast(0.2f);
    effect.setSaturationBoost(2.0f);
    effect.setPreserveLuma(false);

    effect.setPreset(2);

    EXPECT_EQ(effect.preset(), 2);
    EXPECT_FLOAT_EQ(effect.settings().color.r, 0.74f);
    EXPECT_FLOAT_EQ(effect.settings().color.g, 0.88f);
    EXPECT_FLOAT_EQ(effect.settings().color.b, 1.0f);
    EXPECT_FLOAT_EQ(effect.settings().density, 0.30f);
    EXPECT_FLOAT_EQ(effect.settings().brightness, 0.0f);
    EXPECT_FLOAT_EQ(effect.settings().contrast, 1.0f);
    EXPECT_FLOAT_EQ(effect.settings().saturationBoost, 1.02f);
    EXPECT_TRUE(effect.settings().preserveLuma);
}

TEST(PhotoFilterEffectContractTest, AdjustmentValuesClampAndResetNonFiniteInputs)
{
    PhotoFilterEffect effect;
    effect.setDensity(2.0f);
    effect.setBrightness(-2.0f);
    effect.setContrast(3.0f);
    effect.setSaturationBoost(-1.0f);
    EXPECT_FLOAT_EQ(effect.settings().density, 1.0f);
    EXPECT_FLOAT_EQ(effect.settings().brightness, -1.0f);
    EXPECT_FLOAT_EQ(effect.settings().contrast, 2.0f);
    EXPECT_FLOAT_EQ(effect.settings().saturationBoost, 0.0f);

    effect.setDensity(std::numeric_limits<float>::quiet_NaN());
    effect.setBrightness(std::numeric_limits<float>::infinity());
    effect.setContrast(-std::numeric_limits<float>::infinity());
    effect.setSaturationBoost(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.settings().density, 0.35f);
    EXPECT_FLOAT_EQ(effect.settings().brightness, 0.0f);
    EXPECT_FLOAT_EQ(effect.settings().contrast, 1.0f);
    EXPECT_FLOAT_EQ(effect.settings().saturationBoost, 1.0f);
}
