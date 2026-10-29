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

TEST(InvertEffectContractTest, InvertSelectsOneChannelAndBlendsByStrength)
{
    const float pixels[] = {0.20f, 0.35f, 0.80f, 0.25f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    InvertEffect effect;
    effect.setChannel(1);
    effect.setStrength(0.5f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.50f, 0.35f, 0.80f, 0.25f));
    effect.setChannel(99);
    EXPECT_EQ(effect.channel(), 4);
    effect.setStrength(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.strength(), 1.0f);
}

TEST(InvertEffectContractTest, InvertCanTargetAlphaWithoutChangingRgb)
{
    const float pixels[] = {0.20f, 0.35f, 0.80f, 0.25f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    InvertEffect effect;
    effect.setChannel(4);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.20f, 0.35f, 0.80f, 0.75f));
}

TEST(InvertEffectContractTest, RgbModeInvertsAllColorChannelsAndLeavesAlphaUntouched)
{
    const float pixels[] = {0.0f, 0.25f, 0.75f, 0.0f,
                            0.2f, 0.5f, 1.0f, 0.63f};
    const auto source = makeSurface(2, 1, pixels);
    ImageF32x4RGBAWithCache output;
    InvertEffect effect;
    effect.setChannel(0);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(1.0f, 0.75f, 0.25f, 0.0f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.8f, 0.5f, 0.0f, 0.63f));
    expectPixelNear(source, 1, 0, FloatRGBA(0.2f, 0.5f, 1.0f, 0.63f));
}

TEST(InvertEffectContractTest, InvertGpuPlanSupportsRgbOnlyAndEncodesStrength)
{
    InvertEffect effect;
    effect.setChannel(0);
    effect.setStrength(0.5f);
    PointwiseEffectStack stack;
    std::uint32_t slot = 0;

    ASSERT_TRUE(effect.appendGpuPointwiseNodes(stack, slot));
    ASSERT_EQ(stack.nodes().size(), 1u);
    EXPECT_EQ(stack.nodes()[0].kind, PointwiseNodeKind::Contrast);
    EXPECT_FLOAT_EQ(stack.parameters()[0][0], 0.0f);
    EXPECT_EQ(slot, 1u);

    effect.setChannel(2);
    PointwiseEffectStack unsupportedStack;
    slot = 0;
    EXPECT_FALSE(effect.appendGpuPointwiseNodes(unsupportedStack, slot));
    EXPECT_TRUE(unsupportedStack.nodes().empty());
    EXPECT_EQ(slot, 0u);
}
