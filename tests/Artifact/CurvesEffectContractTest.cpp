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

TEST(CurvesEffectContractTest, CurvesInvertHandlesPremultipliedAndTransparentPixels)
{
    const float pixels[] = {
        0.13f, 0.17f, 0.21f, 0.0f,
        0.2f, 0.1f, 0.05f, 0.5f,
    };
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 2, 1, descriptor);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    Artifact::CurvesEffect effect;
    effect.setPreset(4);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.13f, 0.17f, 0.21f, 0.0f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.3f, 0.4f, 0.45f, 0.5f), 2e-5f);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
    expectPixelNear(source, 1, 0, FloatRGBA(0.2f, 0.1f, 0.05f, 0.5f));
}


TEST(CurvesEffectContractTest, IdentityPresetPreservesEntirePremultipliedImage)
{
    constexpr float pixels[] = {
        0.12f, 0.08f, 0.04f, 0.0f,
        0.05f, 0.10f, 0.15f, 0.25f,
        0.20f, 0.30f, 0.10f, 0.5f,
        0.15f, 0.25f, 0.35f, 0.75f,
        0.4f, 0.2f, 0.1f, 1.0f,
        0.3f, 0.3f, 0.3f, 0.4f,
    };
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 3, 2, descriptor);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    CurvesEffect effect;
    effect.setPreset(0);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < std::size(pixels); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
}
