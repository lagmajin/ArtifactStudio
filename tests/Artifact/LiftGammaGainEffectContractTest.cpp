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

TEST(LiftGammaGainEffectContractTest, LiftGammaGainAppliesIndependentChannelsAndPreservesAlpha)
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

TEST(LiftGammaGainEffectContractTest, LiftGammaGainSpatialDescriptorCarriesAllThreeChannelGroups)
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


TEST(LiftGammaGainEffectContractTest, NeutralControlsPreserveWholeRgbaImage)
{
    constexpr float pixels[] = {
        0.05f, 0.2f, 0.8f, 0.0f,
        0.9f, 0.1f, 0.2f, 0.25f,
        0.3f, 0.7f, 0.4f, 0.5f,
        0.6f, 0.6f, 0.6f, 0.75f,
        1.0f, 0.9f, 0.7f, 1.0f,
        0.1f, 0.1f, 0.1f, 0.4f,
    };
    const auto source = makeSurface(3, 2, pixels);
    ImageF32x4RGBAWithCache output;
    LiftGammaGainEffect effect;
    effect.setLift(0.0f, 0.0f, 0.0f);
    effect.setGamma(1.0f, 1.0f, 1.0f);
    effect.setGain(1.0f, 1.0f, 1.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < sizeof(pixels) / sizeof(pixels[0]); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}
