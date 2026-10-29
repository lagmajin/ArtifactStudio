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

TEST(HueAndSaturationEffectContractTest, RotatesHueAndKeepsAlpha)
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

TEST(HueAndSaturationEffectContractTest, ColorizeUsesSelectedHueAndPreservesValueAndAlpha)
{
    const float pixels[] = {1.0f, 0.0f, 0.0f, 0.25f,
                            0.0f, 0.0f, 0.5f, 0.75f};
    const auto source = makeSurface(2, 1, pixels);
    ImageF32x4RGBAWithCache output;
    HueAndSaturation effect;
    effect.setHue(120.0f);
    effect.setSaturation(1.0f);
    effect.setColorize(true);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.0f, 1.0f, 0.0f, 0.25f),
                    2e-5f);
    expectPixelNear(output, 1, 0, FloatRGBA(0.0f, 0.5f, 0.0f, 0.75f),
                    2e-5f);
    expectPixelNear(source, 1, 0, FloatRGBA(0.0f, 0.0f, 0.5f, 0.75f));
}

TEST(HueAndSaturationEffectContractTest, ZeroSaturationAndPositiveLightnessProduceClampedGray)
{
    const float pixels[] = {0.4f, 0.2f, 0.1f, 0.37f,
                            0.8f, 0.3f, 0.2f, 0.83f};
    const auto source = makeSurface(2, 1, pixels);
    ImageF32x4RGBAWithCache output;
    HueAndSaturation effect;
    effect.setSaturation(0.0f);
    effect.setLightness(0.25f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.65f, 0.65f, 0.65f, 0.37f),
                    2e-5f);
    expectPixelNear(output, 1, 0, FloatRGBA(1.0f, 1.0f, 1.0f, 0.83f),
                    2e-5f);
}
