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
import Artifact.Effect.LiftGammaGain;
import Math.Vec;
import PhotoFilterEffect;
import Render.PointwiseEffectFusion;
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

} // namespace

TEST(ColorCorrectionEffectContractTest, InvertSelectsOneChannelAndBlendsByStrength)
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

TEST(ColorCorrectionEffectContractTest, InvertCanTargetAlphaWithoutChangingRgb)
{
    const float pixels[] = {0.20f, 0.35f, 0.80f, 0.25f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    InvertEffect effect;
    effect.setChannel(4);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.20f, 0.35f, 0.80f, 0.75f));
}

TEST(ColorCorrectionEffectContractTest, DisabledEffectsDeepCopyPixelsAndKeepColorDescriptor)
{
    const float pixels[] = {1.25f, -0.20f, 0.35f, 0.0f};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 1, 1, descriptor);
    auto source = ImageF32x4RGBAWithCache(image);

    InvertEffect invert;
    invert.setEnabled(false);
    GrayscaleEffect grayscale;
    grayscale.setStrength(0.25f);
    grayscale.setEnabled(false);
    ChannelMixerEffect channelMixer;
    channelMixer.setMatrix(0.0f, 1.0f, 0.0f,
                          0.0f, 0.0f, 1.0f,
                          1.0f, 0.0f, 0.0f);
    channelMixer.setEnabled(false);
    WhiteBalanceEffect whiteBalance;
    whiteBalance.setTemperature(10000.0f);
    whiteBalance.setEnabled(false);
    ColorBalanceEffect colorBalance;
    colorBalance.setPreset(4);
    colorBalance.setEnabled(false);
    LevelsEffect levels;
    levels.setInputGamma(2.0f);
    levels.setEnabled(false);
    BrightnessEffect brightness;
    brightness.setBrightness(1.0f);
    brightness.setEnabled(false);

    ImageF32x4RGBAWithCache invertOutput;
    ImageF32x4RGBAWithCache grayscaleOutput;
    ImageF32x4RGBAWithCache channelMixerOutput;
    ImageF32x4RGBAWithCache brightnessOutput;
    ImageF32x4RGBAWithCache whiteBalanceOutput;
    ImageF32x4RGBAWithCache colorBalanceOutput;
    ImageF32x4RGBAWithCache levelsOutput;
    invert.applyCPUOnly(source, invertOutput);
    grayscale.applyCPUOnly(source, grayscaleOutput);
    channelMixer.applyCPUOnly(source, channelMixerOutput);
    brightness.applyCPUOnly(source, brightnessOutput);
    whiteBalance.applyCPUOnly(source, whiteBalanceOutput);
    colorBalance.applyCPUOnly(source, colorBalanceOutput);
    levels.applyCPUOnly(source, levelsOutput);

    const FloatRGBA expected(1.25f, -0.20f, 0.35f, 0.0f);
    expectPixelNear(invertOutput, 0, 0, expected);
    expectPixelNear(grayscaleOutput, 0, 0, expected);
    expectPixelNear(channelMixerOutput, 0, 0, expected);
    expectPixelNear(brightnessOutput, 0, 0, expected);
    expectPixelNear(whiteBalanceOutput, 0, 0, expected);
    expectPixelNear(colorBalanceOutput, 0, 0, expected);
    expectPixelNear(levelsOutput, 0, 0, expected);
    EXPECT_EQ(invertOutput.image().colorDescriptor(), descriptor);
    EXPECT_EQ(grayscaleOutput.image().colorDescriptor(), descriptor);
    EXPECT_EQ(channelMixerOutput.image().colorDescriptor(), descriptor);
    EXPECT_EQ(brightnessOutput.image().colorDescriptor(), descriptor);
    EXPECT_EQ(whiteBalanceOutput.image().colorDescriptor(), descriptor);
    EXPECT_EQ(colorBalanceOutput.image().colorDescriptor(), descriptor);
    EXPECT_EQ(levelsOutput.image().colorDescriptor(), descriptor);

    vibranceOutput.image().setPixel(0, 0, FloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
    expectPixelNear(source, 0, 0, expected);
}

TEST(ColorCorrectionEffectContractTest, InvertGpuPlanSupportsRgbOnlyAndEncodesStrength)
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

TEST(ColorCorrectionEffectContractTest, FillBlendsColorAndControlsAlphaIndependently)
{
    const float pixels[] = {0.2f, 0.4f, 0.6f, 0.25f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    FillEffect effect;
    effect.setColor(QColor(255, 0, 128));
    effect.setOpacity(0.5f);
    effect.setPreserveAlpha(true);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0,
                    FloatRGBA(0.6f, 0.2f, (0.6f + 128.0f / 255.0f) * 0.5f,
                              0.25f));
    effect.setPreserveAlpha(false);
    effect.applyCPUOnly(source, output);
    expectPixelNear(output, 0, 0,
                    FloatRGBA(0.6f, 0.2f, (0.6f + 128.0f / 255.0f) * 0.5f,
                              0.625f));
}

TEST(ColorCorrectionEffectContractTest, CurvesInvertHandlesPremultipliedAndTransparentPixels)
{
    const float pixels[] = {
        0.13f, 0.17f, 0.21f, 0.0f,
        0.2f, 0.1f, 0.05f, 0.5f,
    };
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 2, 1, descriptor);
    const ImageF32x4_RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    CurvesEffect effect;
    effect.setPreset(4);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.13f, 0.17f, 0.21f, 0.0f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.3f, 0.4f, 0.45f, 0.5f), 2e-5f);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
    expectPixelNear(source, 1, 0, FloatRGBA(0.2f, 0.1f, 0.05f, 0.5f));
}

TEST(ColorCorrectionEffectContractTest, GradientRampInterpolatesSpatiallyAndPreservesAlphaOnRequest)
{
    const float pixels[] = {
        0.2f, 0.3f, 0.4f, 0.2f,
        0.3f, 0.4f, 0.5f, 0.4f,
        0.4f, 0.5f, 0.6f, 0.6f,
    };
    const auto source = makeSurface(3, 1, pixels);
    ImageF32x4RGBAWithCache output;
    GradientRampEffect effect;
    effect.setStartColor(QColor(0, 0, 0));
    effect.setEndColor(QColor(255, 255, 255));
    effect.setStartPoint(0.0f, 0.0f);
    effect.setEndPoint(1.0f, 0.0f);
    effect.setOpacity(1.0f);
    effect.setPreserveAlpha(true);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.0f, 0.0f, 0.0f, 0.2f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.5f, 0.5f, 0.5f, 0.4f));
    expectPixelNear(output, 2, 0, FloatRGBA(1.0f, 1.0f, 1.0f, 0.6f));
    effect.setOpacity(0.5f);
    effect.setPreserveAlpha(false);
    effect.applyCPUOnly(source, output);
    expectPixelNear(output, 2, 0, FloatRGBA(0.7f, 0.75f, 0.8f, 0.8f));
}

TEST(ColorCorrectionEffectContractTest, ColorWheelsGammaGradesStraightRgbAndRestoresPremultipliedAlpha)
{
    const float pixels[] = {0.125f, 0.25f, 0.375f, 0.5f};
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 1, 1, descriptor);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ColorWheelsEffect effect;
    effect.setGamma(2.0f, 1.0f, 1.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.25f, 0.25f, 0.375f, 0.5f),
                    2e-5f);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
    expectPixelNear(source, 0, 0, FloatRGBA(0.125f, 0.25f, 0.375f, 0.5f));
}

TEST(ColorCorrectionEffectContractTest, ColoramaHuePaletteMapsExpectedColorAndPreservesAlpha)
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

TEST(ColorCorrectionEffectContractTest, ColoramaPresetAndSetterBoundariesStayCoherent)
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

TEST(ColorCorrectionEffectContractTest, ColoramaGpuDescriptorCarriesEveryProcessorSetting)
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

TEST(ColorCorrectionEffectContractTest, HueAndSaturationRotatesHueAndKeepsAlpha)
{
    const float pixels[] = {1.0f, 0.0f, 0.0f, 0.42f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    HueAndSaturation effect;
    effect.setHue(120.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.0f, 1.0f, 0.0f, 0.42f),
                    2e-5f);
}

TEST(ColorCorrectionEffectContractTest, PhotoFilterAppliesTintDensityWithoutChangingAlpha)
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

TEST(ColorCorrectionEffectContractTest, SelectiveColorAdjustmentTargetsMatchingColorGroup)
{
    const float pixels[] = {1.0f, 0.0f, 0.0f, 0.28f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    SelectiveColorEffect effect;
    effect.setRelativeMode(false);
    effect.setPreserveLuma(false);
    effect.setStrength(1.0f);
    effect.setAdjustment(SelectiveColorGroup::Reds, 0.2f, 0.0f, 0.0f, 0.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.8f, 0.0f, 0.0f, 0.28f));
}

TEST(ColorCorrectionEffectContractTest, TritoneMapsTheMidtoneBandAndPreservesAlpha)
{
    const float pixels[] = {0.5f, 0.5f, 0.5f, 0.71f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    TritoneEffect effect;
    effect.setShadowColor(QColor(0, 0, 0));
    effect.setMidtoneColor(QColor(0, 255, 0));
    effect.setHighlightColor(QColor(255, 255, 255));
    effect.setBalance(0.5f);
    effect.setSoftness(0.2f);
    effect.setMasterStrength(1.0f);
    effect.setColorMix(1.0f);
    effect.setPreserveLuma(false);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.0f, 1.0f, 0.0f, 0.71f),
                    2e-5f);
}

TEST(ColorCorrectionEffectContractTest, LiftGammaGainAppliesIndependentChannelsAndPreservesAlpha)
{
    const float pixels[] = {0.20f, 0.30f, 0.40f, 0.50f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    LiftGammaGainEffect effect;
    effect.setLift(0.0f, 0.0f, 0.5f);
    effect.setGamma(4.0f, 1.0f, 1.0f);
    effect.setGain(1.0f, 2.0f, 1.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0,
                    FloatRGBA(std::pow(0.20f, 0.25f), 0.60f, 0.45f, 0.50f));
}

TEST(ColorCorrectionEffectContractTest, LiftGammaGainSpatialDescriptorCarriesAllThreeChannelGroups)
{
    LiftGammaGainEffect effect;
    effect.setLift(0.1f, 0.2f, 0.3f);
    effect.setGamma(1.1f, 1.2f, 1.3f);
    effect.setGain(1.4f, 1.5f, 1.6f);
    GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey, effect.gpuGenericKey());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 0.1f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 1.1f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[6], 1.4f);
}

TEST(ColorCorrectionEffectContractTest, ShadowHighlightCanBeConfiguredAsExactRgbPassthrough)
{
    const float pixels[] = {0.12f, 0.34f, 0.56f, 0.78f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ShadowHighlightEffect effect;
    effect.setShadowAmount(0.0f);
    effect.setHighlightAmount(0.0f);
    effect.setColorCorrection(0.0f);
    effect.setMidtoneContrast(0.0f);
    effect.setBlackClip(0.0f);
    effect.setWhiteClip(0.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.12f, 0.34f, 0.56f, 0.78f));
}

TEST(ColorCorrectionEffectContractTest, ShadowLiftUsesTonalWeightWithoutChangingAlpha)
{
    const float pixels[] = {0.10f, 0.10f, 0.10f, 0.60f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ShadowHighlightEffect effect;
    effect.setShadowAmount(100.0f);
    effect.setShadowTonalWidth(100.0f);
    effect.setShadowRadius(Units::Pixels{10.0f});
    effect.setHighlightAmount(0.0f);
    effect.setColorCorrection(0.0f);
    effect.setMidtoneContrast(0.0f);
    effect.setBlackClip(0.0f);
    effect.setWhiteClip(0.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.42f, 0.42f, 0.42f, 0.60f));
}

TEST(ColorCorrectionEffectContractTest, BrightnessOffsetsRgbAndPreservesAlphaIncludingTransparentPixels)
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

TEST(ColorCorrectionEffectContractTest, BrightnessContrastHighlightsAndShadowsFollowCpuFormula)
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

TEST(ColorCorrectionEffectContractTest, BrightnessSettersClampAndResetNonFiniteValues)
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

TEST(ColorCorrectionEffectContractTest, BrightnessGpuPlanEncodesOffsetAndContrastAndRejectsToneControls)
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

TEST(ColorCorrectionEffectContractTest, GrayscaleModesUseDistinctLuminanceAndDesaturationFormulas)
{
    const float pixels[] = {0.2f, 0.4f, 0.8f, 0.35f};
    const auto source = makeSurface(1, 1, pixels);

    struct ModeCase {
        int mode;
        FloatRGBA expected;
    };
    const ModeCase cases[] = {
        {0, FloatRGBA(0.2929f, 0.3929f, 0.5929f, 0.35f)},
        {1, FloatRGBA(0.308883f, 0.408883f, 0.608883f, 0.35f)},
        {2, FloatRGBA(0.35f, 0.45f, 0.65f, 0.35f)},
    };

    for (const auto& testCase : cases) {
        ImageF32x4RGBAWithCache output;
        GrayscaleEffect effect;
        effect.setMode(testCase.mode);
        effect.setStrength(0.5f);
        effect.applyCPUOnly(source, output);
        expectPixelNear(output, 0, 0, testCase.expected, 2e-4f);
    }
}

TEST(ColorCorrectionEffectContractTest, GrayscaleZeroStrengthPreservesSourceExactly)
{
    const float pixels[] = {1.2f, -0.1f, 0.35f, 0.0f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    GrayscaleEffect effect;
    effect.setMode(2);
    effect.setStrength(0.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(1.2f, -0.1f, 0.35f, 0.0f));
}

TEST(ColorCorrectionEffectContractTest, GrayscaleParametersClampAndSanitize)
{
    GrayscaleEffect effect;
    effect.setMode(-4);
    EXPECT_EQ(effect.mode(), 0);
    effect.setMode(9);
    EXPECT_EQ(effect.mode(), 2);
    effect.setStrength(-1.0f);
    EXPECT_FLOAT_EQ(effect.strength(), 0.0f);
    effect.setStrength(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.strength(), 1.0f);
}

TEST(ColorCorrectionEffectContractTest, GrayscaleGpuPointwisePlanSupportsOnlyPerceptualMode)
{
    GrayscaleEffect effect;
    effect.setMode(0);
    effect.setStrength(0.25f);
    PointwiseEffectStack stack;
    std::uint32_t slot = 0;

    ASSERT_TRUE(effect.appendGpuPointwiseNodes(stack, slot));
    ASSERT_EQ(stack.nodes().size(), 1u);
    EXPECT_EQ(stack.nodes()[0].kind, PointwiseNodeKind::Saturation);
    EXPECT_FLOAT_EQ(stack.parameters()[0][0], 0.75f);
    EXPECT_EQ(slot, 1u);

    effect.setMode(1);
    PointwiseEffectStack unsupported;
    slot = 0;
    EXPECT_FALSE(effect.appendGpuPointwiseNodes(unsupported, slot));
    EXPECT_TRUE(unsupported.nodes().empty());
    EXPECT_EQ(slot, 0u);
}

TEST(ColorCorrectionEffectContractTest, ChannelMixerAppliesMatrixAndStrengthWithoutChangingAlpha)
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

TEST(ColorCorrectionEffectContractTest, ChannelMixerClampsStrengthAndSanitizesNonFiniteMatrixEntries)
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

TEST(ColorCorrectionEffectContractTest, ChannelMixerGpuPlanCarriesMatrixClampAndRejectsUnsupportedSettings)
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

TEST(ColorCorrectionEffectContractTest, WhiteBalanceUnpremultipliesCorrectsAndRepremultipliesSafely)
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

TEST(ColorCorrectionEffectContractTest, WhiteBalanceSettersClampAndResetNonFiniteValues)
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

TEST(ColorCorrectionEffectContractTest, WhiteBalanceGpuPlanCarriesTemperatureTintAndBrightness)
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

TEST(ColorCorrectionEffectContractTest, ColorBalanceAppliesShadowMidtoneAndHighlightBands)
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

TEST(ColorCorrectionEffectContractTest, ColorBalancePreserveLumaKeepsInputLuminanceWhenUnclipped)
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

TEST(ColorCorrectionEffectContractTest, ColorBalancePremultipliedPixelsAreUnpremultipliedAndTransparentRgbIsUntouched)
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

TEST(ColorCorrectionEffectContractTest, ColorBalanceZeroStrengthIsAnExactCopyIncludingHdrAndDescriptor)
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

TEST(ColorCorrectionEffectContractTest, ColorBalanceGpuDescriptorCarriesBandParametersAndLumaMode)
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

TEST(ColorCorrectionEffectContractTest, LevelsMapsInputBoundsGammaAndOutputRangeWithoutChangingAlpha)
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

TEST(ColorCorrectionEffectContractTest, LevelsPerChannelPropertiesDriveIndependentRgbCurves)
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

TEST(ColorCorrectionEffectContractTest, LevelsMasterSettersPreserveFiniteOutOfRangeValuesExceptGamma)
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

TEST(ColorCorrectionEffectContractTest, LevelsSetterNonFiniteFallbacksAreDeterministic)
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

TEST(ColorCorrectionEffectContractTest, DegenerateInputRangeCannotProduceNonFinitePixels)
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

TEST(ColorCorrectionEffectContractTest, LevelsGpuPlanCarriesMasterInputBoundsAndRejectsUnsupportedModes)
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
