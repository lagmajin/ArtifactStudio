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

TEST(BrightnessEffectContractTest, BrightnessOffsetsRgbAndPreservesAlphaIncludingTransparentPixels)
{
    const float pixels[] = {0.20f, 0.50f, 0.90f, 0.0f,
                            0.95f, 0.25f, 0.75f, 0.4f};
    auto source = makeSurface(2, 1, pixels);
    ImageF32x4RGBAWithCache output;
    BrightnessEffect effect;
    effect.setBrightness(0.10f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.30f, 0.60f, 1.0f, 0.0f));
    expectPixelNear(output, 1, 0, FloatRGBA(1.0f, 0.35f, 0.85f, 0.4f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.20f, 0.50f, 0.90f, 0.0f));
}

TEST(BrightnessEffectContractTest, BrightnessContrastHighlightsAndShadowsFollowCpuFormula)
{
    const float pixels[] = {
        0.20f, 0.53333336f, 0.50f, 0.2f,
        0.80f, 0.41666666f, 0.50f, 0.8f,
    };
    const auto source = makeSurface(2, 1, pixels);
    ImageF32x4RGBAWithCache output;
    BrightnessEffect effect;
    effect.setContrast(0.5f);
    effect.setHighlights(0.5f);
    effect.setShadows(-0.5f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.0f, 0.65f, 0.5f, 0.2f));
    expectPixelNear(output, 1, 0, FloatRGBA(1.0f, 0.125f, 0.5f, 0.8f));
}

TEST(BrightnessEffectContractTest, MaximumContrastKeepsMidpointAndSeparatesNearbyValues)
{
    const float pixels[] = {0.49f, 0.50f, 0.51f, 0.2f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    BrightnessEffect effect;
    effect.setContrast(1.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.0f, 0.5f, 1.0f, 0.2f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.49f, 0.50f, 0.51f, 0.2f));
}

TEST(BrightnessEffectContractTest, BrightnessSettersClampAndResetNonFiniteValues)
{
    BrightnessEffect effect;
    effect.setBrightness(2.0f);
    EXPECT_FLOAT_EQ(effect.brightness(), 1.0f);
    effect.setBrightness(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.brightness(), 0.0f);
    effect.setContrast(-2.0f);
    EXPECT_FLOAT_EQ(effect.contrast(), -1.0f);
    effect.setContrast(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.contrast(), 0.0f);
    effect.setHighlights(2.0f);
    EXPECT_FLOAT_EQ(effect.highlights(), 1.0f);
    effect.setShadows(-2.0f);
    EXPECT_FLOAT_EQ(effect.shadows(), -1.0f);
}

TEST(BrightnessEffectContractTest, BrightnessGpuPlanEncodesOffsetAndContrastAndRejectsToneControls)
{
    BrightnessEffect effect;
    effect.setBrightness(0.125f);
    effect.setContrast(0.5f);
    PointwiseEffectStack stack;
    std::uint32_t slot = 0;

    ASSERT_TRUE(effect.appendGpuPointwiseNodes(stack, slot));
    ASSERT_EQ(stack.nodes().size(), 2u);
    EXPECT_EQ(stack.nodes()[0].kind, PointwiseNodeKind::Offset);
    EXPECT_EQ(stack.nodes()[1].kind, PointwiseNodeKind::Contrast);
    EXPECT_FLOAT_EQ(stack.parameters()[0][0], 0.125f);
    EXPECT_FLOAT_EQ(stack.parameters()[1][0], 3.0f);
    EXPECT_EQ(slot, 2u);

    effect.setHighlights(0.25f);
    PointwiseEffectStack unsupportedStack;
    slot = 0;
    EXPECT_FALSE(effect.appendGpuPointwiseNodes(unsupportedStack, slot));
    EXPECT_TRUE(unsupportedStack.nodes().empty());
    EXPECT_EQ(slot, 0u);
}


TEST(BrightnessEffectContractTest, NeutralControlsPreserveEveryRgbaChannel)
{
    constexpr float pixels[] = {
        0.0f, 0.1f, 0.2f, 0.0f,
        0.2f, 0.4f, 0.6f, 0.25f,
        0.3f, 0.5f, 0.7f, 0.5f,
        0.4f, 0.6f, 0.8f, 0.75f,
        0.8f, 0.9f, 1.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 0.4f,
    };
    const auto source = makeSurface(3, 2, pixels);
    ImageF32x4RGBAWithCache output;
    BrightnessEffect effect;
    effect.setBrightness(0.0f);
    effect.setContrast(0.0f);
    effect.setHighlights(0.0f);
    effect.setShadows(0.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < sizeof(pixels) / sizeof(pixels[0]); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}
