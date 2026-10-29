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

TEST(GrayscaleEffectContractTest, GrayscaleModesUseDistinctLuminanceAndDesaturationFormulas)
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

TEST(GrayscaleEffectContractTest, GrayscaleZeroStrengthPreservesSourceExactly)
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

TEST(GrayscaleEffectContractTest, GrayscaleParametersClampAndSanitize)
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

TEST(GrayscaleEffectContractTest, GrayscaleGpuPointwisePlanSupportsOnlyPerceptualMode)
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


TEST(GrayscaleEffectContractTest, ZeroStrengthPreservesEveryPixelInEveryMode)
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
    for (int mode = 0; mode <= 2; ++mode) {
        ImageF32x4RGBAWithCache output;
        GrayscaleEffect effect;
        effect.setMode(mode);
        effect.setStrength(0.0f);

        effect.applyCPUOnly(source, output);

        const float* actual = output.image().rgba32fData();
        ASSERT_NE(actual, nullptr) << "mode=" << mode;
        for (std::size_t channel = 0; channel < sizeof(pixels) / sizeof(pixels[0]); ++channel) {
            EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
                << "mode=" << mode << ", channel=" << channel;
        }
    }
}
