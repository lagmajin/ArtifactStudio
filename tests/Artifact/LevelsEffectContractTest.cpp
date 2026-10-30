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

TEST(LevelsEffectContractTest, LevelsMapsInputBoundsGammaAndOutputRangeWithoutChangingAlpha)
{
    const float pixels[] = {
        0.0f, 64.0f / 255.0f, 128.0f / 255.0f, 0.0f,
        192.0f / 255.0f, 1.0f, 128.0f / 255.0f, 0.65f,
    };
    const auto source = makeSurface(2, 1, pixels);
    ImageF32x4RGBAWithCache output;
    Artifact::LevelsEffect effect;
    effect.setInputBlack(64.0f);
    effect.setInputWhite(192.0f);
    effect.setInputGamma(2.0f);
    effect.setOutputBlack(32.0f);
    effect.setOutputWhite(224.0f);

    effect.applyCPUOnly(source, output);

    const float midpoint = (32.0f + std::sqrt(0.5f) * (224.0f - 32.0f)) / 255.0f;
    expectPixelNear(output, 0, 0,
                    FloatRGBA(32.0f / 255.0f, 32.0f / 255.0f, midpoint, 0.0f));
    expectPixelNear(output, 1, 0,
                    FloatRGBA(224.0f / 255.0f, 224.0f / 255.0f,
                              midpoint, 0.65f));
}

TEST(LevelsEffectContractTest, LevelsPerChannelPropertiesDriveIndependentRgbCurves)
{
    const float pixels[] = {0.5f, 0.5f, 0.5f, 0.37f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    Artifact::LevelsEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Per Channel")), QVariant(true));
    effect.setPropertyValue(UniString(QStringLiteral("Red Output White")),
                            QVariant(128.0));
    effect.setPropertyValue(UniString(QStringLiteral("Blue Input Gamma")),
                            QVariant(2.0));

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0,
                    FloatRGBA(64.0f / 255.0f, 0.5f, std::sqrt(0.5f), 0.37f),
                    2e-5f);
}

TEST(LevelsEffectContractTest, LevelsMasterSettersPreserveFiniteOutOfRangeValuesExceptGamma)
{
    Artifact::LevelsEffect effect;
    effect.setInputBlack(-10.0f);
    EXPECT_DOUBLE_EQ(effect.settings().inputBlack, -10.0);
    effect.setInputBlack(300.0f);
    EXPECT_DOUBLE_EQ(effect.settings().inputBlack, 300.0);
    effect.setInputWhite(500.0f);
    EXPECT_DOUBLE_EQ(effect.settings().inputWhite, 500.0);
    effect.setInputGamma(100.0f);
    EXPECT_DOUBLE_EQ(effect.settings().inputGamma, 10.0);
    effect.setInputGamma(std::numeric_limits<float>::quiet_NaN());
    EXPECT_DOUBLE_EQ(effect.settings().inputGamma, 1.0);
    effect.setOutputBlack(-5.0f);
    EXPECT_DOUBLE_EQ(effect.settings().outputBlack, -5.0);
    effect.setOutputWhite(300.0f);
    EXPECT_DOUBLE_EQ(effect.settings().outputWhite, 300.0);
}

TEST(LevelsEffectContractTest, LevelsSetterNonFiniteFallbacksAreDeterministic)
{
    Artifact::LevelsEffect effect;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    effect.setInputBlack(nan);
    effect.setInputWhite(infinity);
    effect.setInputGamma(-infinity);
    effect.setOutputBlack(nan);
    effect.setOutputWhite(infinity);

    EXPECT_DOUBLE_EQ(effect.settings().inputBlack, 0.0);
    EXPECT_DOUBLE_EQ(effect.settings().inputWhite, 255.0);
    EXPECT_DOUBLE_EQ(effect.settings().inputGamma, 1.0);
    EXPECT_DOUBLE_EQ(effect.settings().outputBlack, 0.0);
    EXPECT_DOUBLE_EQ(effect.settings().outputWhite, 255.0);
}

TEST(LevelsEffectContractTest, DegenerateInputRangeCannotProduceNonFinitePixels)
{
    const float boundary = 128.0f / 255.0f;
    const float pixels[] = {boundary, boundary, boundary, 0.75f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    Artifact::LevelsEffect effect;
    effect.setInputBlack(128.0f);
    effect.setInputWhite(128.0f);

    effect.applyCPUOnly(source, output);

    const FloatRGBA actual = pixel(output, 0, 0);
    EXPECT_TRUE(std::isfinite(actual.r()));
    EXPECT_TRUE(std::isfinite(actual.g()));
    EXPECT_TRUE(std::isfinite(actual.b()));
    EXPECT_TRUE(std::isfinite(actual.a()));
}

TEST(LevelsEffectContractTest, LevelsGpuPlanCarriesMasterInputBoundsAndRejectsUnsupportedModes)
{
    Artifact::LevelsEffect effect;
    effect.setPreset(1);
    effect.setInputBlack(32.0f);
    effect.setInputWhite(224.0f);
    PointwiseEffectStack stack;
    std::uint32_t slot = 0;

    ASSERT_TRUE(effect.appendGpuPointwiseNodes(stack, slot));
    ASSERT_EQ(stack.nodes().size(), 1u);
    EXPECT_EQ(stack.nodes()[0].kind, PointwiseNodeKind::Levels);
    EXPECT_FLOAT_EQ(stack.parameters()[0][0], 32.0f / 255.0f);
    EXPECT_FLOAT_EQ(stack.parameters()[1][0], 224.0f / 255.0f);
    EXPECT_EQ(slot, 2u);

    effect.setInputGamma(2.0f);
    PointwiseEffectStack unsupported;
    slot = 0;
    EXPECT_FALSE(effect.appendGpuPointwiseNodes(unsupported, slot));
    EXPECT_TRUE(unsupported.nodes().empty());
    EXPECT_EQ(slot, 0u);

    effect.setInputGamma(1.0f);
    effect.setOutputBlack(10.0f);
    EXPECT_FALSE(effect.appendGpuPointwiseNodes(unsupported, slot));
}
