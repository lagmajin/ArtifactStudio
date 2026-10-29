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

TEST(GradientRampEffectContractTest, GradientRampInterpolatesSpatiallyAndPreservesAlphaOnRequest)
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


TEST(GradientRampEffectContractTest, ZeroOpacityPreservesWholeImageAndAlpha)
{
    constexpr float pixels[] = {
        0.1f, 0.2f, 0.3f, 0.0f,
        0.2f, 0.4f, 0.6f, 0.25f,
        0.3f, 0.5f, 0.7f, 0.5f,
        0.4f, 0.6f, 0.8f, 0.75f,
        0.8f, 0.9f, 1.0f, 1.0f,
        0.6f, 0.4f, 0.2f, 0.4f,
    };
    const auto source = makeSurface(3, 2, pixels);
    ImageF32x4RGBAWithCache output;
    GradientRampEffect effect;
    effect.setStartColor(QColor(255, 0, 0));
    effect.setEndColor(QColor(0, 0, 255));
    effect.setOpacity(0.0f);
    effect.setPreserveAlpha(false);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < sizeof(pixels) / sizeof(pixels[0]); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(GradientRampEffectContractTest, CoincidentEndpointsUseStartColorAcrossTheImage)
{
    constexpr float pixels[] = {
        0.1f, 0.2f, 0.3f, 0.25f,
        0.4f, 0.5f, 0.6f, 0.75f,
    };
    const auto source = makeSurface(2, 1, pixels);
    ImageF32x4RGBAWithCache output;
    GradientRampEffect effect;
    effect.setStartColor(QColor(255, 0, 0));
    effect.setEndColor(QColor(0, 0, 255));
    effect.setStartPoint(0.5f, 0.5f);
    effect.setEndPoint(0.5f, 0.5f);
    effect.setOpacity(1.0f);
    effect.setPreserveAlpha(true);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(1.0f, 0.0f, 0.0f, 0.25f));
    expectPixelNear(output, 1, 0, FloatRGBA(1.0f, 0.0f, 0.0f, 0.75f));
}
