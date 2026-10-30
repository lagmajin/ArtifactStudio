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

TEST(ChannelMixerEffectContractTest, ChannelMixerAppliesMatrixAndStrengthWithoutChangingAlpha)
{
    const float pixels[] = {0.2f, 0.4f, 0.8f, 0.35f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ChannelMixerEffect effect;
    effect.setMatrix(0.0f, 1.0f, 0.0f,
                     0.0f, 0.0f, 1.0f,
                     1.0f, 0.0f, 0.0f);
    effect.setStrength(0.5f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.3f, 0.6f, 0.5f, 0.35f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.2f, 0.4f, 0.8f, 0.35f));

    effect.setMatrix(10.0f, 0.0f, 0.0f,
                     0.0f, 1.0f, 0.0f,
                     0.0f, 0.0f, 1.0f);
    effect.setStrength(1.0f);
    effect.applyCPUOnly(source, output);
    expectPixelNear(output, 0, 0, FloatRGBA(1.0f, 0.4f, 0.8f, 0.35f));
}

TEST(ChannelMixerEffectContractTest, ChannelMixerClampsStrengthAndSanitizesNonFiniteMatrixEntries)
{
    ChannelMixerEffect effect;
    effect.setStrength(-1.0f);
    EXPECT_FLOAT_EQ(effect.settings().strength, 0.0f);
    effect.setStrength(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.settings().strength, 1.0f);

    effect.setMatrix(std::numeric_limits<float>::quiet_NaN(), 2.0f, -1.0f,
                     0.0f, 1.0f, 0.0f,
                     0.0f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(effect.settings().matrix[0][0], 1.0f);
    EXPECT_FLOAT_EQ(effect.settings().matrix[0][1], 2.0f);
    EXPECT_FLOAT_EQ(effect.settings().matrix[0][2], -1.0f);

    effect.setPreset(100);
    EXPECT_EQ(effect.preset(), 5);
}

TEST(ChannelMixerEffectContractTest, ChannelMixerGpuPlanCarriesMatrixClampAndRejectsUnsupportedSettings)
{
    ChannelMixerEffect effect;
    effect.setPreset(1);
    PointwiseEffectStack stack;
    std::uint32_t slot = 0;

    ASSERT_TRUE(effect.appendGpuPointwiseNodes(stack, slot));
    ASSERT_EQ(stack.nodes().size(), 2u);
    EXPECT_EQ(stack.nodes()[0].kind, PointwiseNodeKind::ColorMatrix);
    EXPECT_EQ(stack.nodes()[1].kind, PointwiseNodeKind::Clamp);
    EXPECT_FLOAT_EQ(stack.parameters()[0][0], 1.0f);
    EXPECT_FLOAT_EQ(stack.parameters()[1][1], 1.0f);
    EXPECT_FLOAT_EQ(stack.parameters()[2][2], 1.0f);
    EXPECT_FLOAT_EQ(stack.parameters()[3][0], 0.0f);
    EXPECT_FLOAT_EQ(stack.parameters()[4][0], 1.0f);
    EXPECT_EQ(slot, 5u);

    effect.setMonochrome(true);
    PointwiseEffectStack unsupported;
    slot = 0;
    EXPECT_FALSE(effect.appendGpuPointwiseNodes(unsupported, slot));
    EXPECT_TRUE(unsupported.nodes().empty());
    EXPECT_EQ(slot, 0u);

    effect.setMonochrome(false);
    effect.setPreserveLuma(true);
    EXPECT_FALSE(effect.appendGpuPointwiseNodes(unsupported, slot));
    effect.setPreserveLuma(false);
    effect.setStrength(0.5f);
    EXPECT_FALSE(effect.appendGpuPointwiseNodes(unsupported, slot));
}
