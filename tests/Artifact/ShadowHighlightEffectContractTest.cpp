#include <gtest/gtest.h>

#include <limits>

import FloatRGBA;
import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import ShadowHighlightEffect;

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

TEST(ShadowHighlightEffectContractTest, ShadowHighlightCanBeConfiguredAsExactRgbPassthrough)
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

TEST(ShadowHighlightEffectContractTest, ShadowLiftUsesTonalWeightWithoutChangingAlpha)
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

TEST(ShadowHighlightEffectContractTest, HighlightRecoveryPullsBrightValuesAndPreservesAlpha)
{
    const float pixels[] = {1.0f, 1.0f, 1.0f, 0.35f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ShadowHighlightEffect effect;
    effect.setShadowAmount(0.0f);
    effect.setHighlightAmount(100.0f);
    effect.setHighlightTonalWidth(50.0f);
    effect.setHighlightRadius(Units::Pixels{1.0f});
    effect.setColorCorrection(0.0f);
    effect.setMidtoneContrast(0.0f);
    effect.setBlackClip(0.0f);
    effect.setWhiteClip(0.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.5f, 0.5f, 0.5f, 0.35f));
}

TEST(ShadowHighlightEffectContractTest, AmountWidthRadiusAndClipInputsStayInRange)
{
    ShadowHighlightEffect effect;
    effect.setShadowAmount(150.0f);
    effect.setShadowTonalWidth(0.0f);
    effect.setShadowRadius(Units::Pixels{-2.0f});
    effect.setHighlightAmount(-1.0f);
    effect.setHighlightTonalWidth(150.0f);
    effect.setHighlightRadius(Units::Pixels{4.0f});
    effect.setBlackClip(1.0f);
    effect.setWhiteClip(-1.0f);
    EXPECT_FLOAT_EQ(effect.shadowAmount(), 100.0f);
    EXPECT_FLOAT_EQ(effect.shadowTonalWidth(), 1.0f);
    EXPECT_FLOAT_EQ(effect.shadowRadius().value, 0.0f);
    EXPECT_FLOAT_EQ(effect.highlightAmount(), 0.0f);
    EXPECT_FLOAT_EQ(effect.highlightTonalWidth(), 100.0f);
    EXPECT_FLOAT_EQ(effect.highlightRadius().value, 4.0f);
    EXPECT_FLOAT_EQ(effect.blackClip(), 0.5f);
    EXPECT_FLOAT_EQ(effect.whiteClip(), 0.0f);

    effect.setShadowAmount(std::numeric_limits<float>::infinity());
    effect.setShadowTonalWidth(std::numeric_limits<float>::quiet_NaN());
    effect.setShadowRadius(Units::Pixels{std::numeric_limits<float>::quiet_NaN()});
    effect.setHighlightAmount(std::numeric_limits<float>::quiet_NaN());
    effect.setHighlightTonalWidth(std::numeric_limits<float>::infinity());
    effect.setHighlightRadius(Units::Pixels{std::numeric_limits<float>::infinity()});
    effect.setBlackClip(std::numeric_limits<float>::quiet_NaN());
    effect.setWhiteClip(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.shadowAmount(), 50.0f);
    EXPECT_FLOAT_EQ(effect.shadowTonalWidth(), 50.0f);
    EXPECT_FLOAT_EQ(effect.shadowRadius().value, 30.0f);
    EXPECT_FLOAT_EQ(effect.highlightAmount(), 0.0f);
    EXPECT_FLOAT_EQ(effect.highlightTonalWidth(), 50.0f);
    EXPECT_FLOAT_EQ(effect.highlightRadius().value, 30.0f);
    EXPECT_FLOAT_EQ(effect.blackClip(), 0.01f);
    EXPECT_FLOAT_EQ(effect.whiteClip(), 0.01f);
}
