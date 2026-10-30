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

TEST(FillEffectContractTest, FillBlendsColorAndControlsAlphaIndependently)
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


TEST(FillEffectContractTest, ZeroOpacityPreservesAllSourceChannels)
{
    constexpr float pixels[] = {
        0.1f, 0.2f, 0.3f, 0.0f,
        0.25f, 0.5f, 0.75f, 0.25f,
        0.8f, 0.6f, 0.4f, 0.8f,
    };
    const auto source = makeSurface(3, 1, pixels);
    ImageF32x4RGBAWithCache output;
    FillEffect effect;
    effect.setColor(QColor(20, 180, 240));
    effect.setOpacity(0.0f);
    effect.setPreserveAlpha(false);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < std::size(pixels); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(FillEffectContractTest, OpacityClampsFiniteValuesAndResetsNonFiniteValues)
{
    FillEffect effect;

    effect.setOpacity(-0.5f);
    EXPECT_FLOAT_EQ(effect.settings().opacity, 0.0f);
    effect.setOpacity(1.5f);
    EXPECT_FLOAT_EQ(effect.settings().opacity, 1.0f);

    effect.setOpacity(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.settings().opacity, 1.0f);
    effect.setOpacity(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.settings().opacity, 1.0f);
}
