#include <gtest/gtest.h>

#include <limits>
#include <vector>

import Artifact.Effect.Abstract;
import BrightnessEffect;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import InvertEffect;
import Memory.SharedPtr;

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
                     const FloatRGBA& expected)
{
    const FloatRGBA actual = surface.image().getPixel(x, y);
    EXPECT_NEAR(actual.r(), expected.r(), 1e-6f);
    EXPECT_NEAR(actual.g(), expected.g(), 1e-6f);
    EXPECT_NEAR(actual.b(), expected.b(), 1e-6f);
    EXPECT_NEAR(actual.a(), expected.a(), 1e-6f);
}

} // namespace

TEST(EffectSequenceContractTest, AppliesBrightnessThenInvertAcrossEveryPixel)
{
    const float pixels[] = {
        0.20f, 0.50f, 0.90f, 0.0f,
        0.95f, 0.25f, 0.75f, 0.4f,
    };
    const auto source = makeSurface(2, 1, pixels);
    ImageF32x4RGBAWithCache brightened;
    ImageF32x4RGBAWithCache output;

    BrightnessEffect brightness;
    brightness.setBrightness(0.10f);
    brightness.applyCPUOnly(source, brightened);

    InvertEffect invert;
    invert.setStrength(0.5f);
    invert.applyCPUOnly(brightened, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.50f, 0.50f, 0.50f, 0.0f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.50f, 0.50f, 0.50f, 0.4f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.20f, 0.50f, 0.90f, 0.0f));
}

TEST(EffectSequenceContractTest, ReversingEffectOrderChangesRgbButKeepsAlpha)
{
    const float pixels[] = {0.20f, 0.40f, 0.80f, 0.35f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache first;
    ImageF32x4RGBAWithCache brightnessThenInvert;
    ImageF32x4RGBAWithCache second;
    ImageF32x4RGBAWithCache invertThenBrightness;

    BrightnessEffect brightness;
    brightness.setBrightness(0.10f);
    InvertEffect invert;
    invert.setStrength(1.0f);

    brightness.applyCPUOnly(source, first);
    invert.applyCPUOnly(first, brightnessThenInvert);
    invert.applyCPUOnly(source, second);
    brightness.applyCPUOnly(second, invertThenBrightness);

    expectPixelNear(brightnessThenInvert, 0, 0,
                    FloatRGBA(0.70f, 0.50f, 0.10f, 0.35f));
    expectPixelNear(invertThenBrightness, 0, 0,
                    FloatRGBA(0.90f, 0.70f, 0.30f, 0.35f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.20f, 0.40f, 0.80f, 0.35f));
}

TEST(EffectSequenceContractTest, MixBlendsEffectResultWithSource)
{
    const float pixels[] = {0.20f, 0.40f, 0.80f, 0.35f};
    const auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;

    BrightnessEffect brightness;
    brightness.setBrightness(0.10f);
    brightness.setMix(0.0f);
    brightness.applyCPUOnly(source, output);
    expectPixelNear(output, 0, 0, FloatRGBA(0.20f, 0.40f, 0.80f, 0.35f));

    brightness.setMix(0.25f);
    brightness.applyCPUOnly(source, output);
    expectPixelNear(output, 0, 0, FloatRGBA(0.225f, 0.425f, 0.825f, 0.35f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.20f, 0.40f, 0.80f, 0.35f));
}

TEST(EffectSequenceContractTest, EffectMixSetterClampsAndResetsNonFiniteValues)
{
    BrightnessEffect effect;

    effect.setMix(-0.25f);
    EXPECT_FLOAT_EQ(effect.mix(), 0.0f);
    effect.setMix(1.25f);
    EXPECT_FLOAT_EQ(effect.mix(), 1.0f);
    effect.setMix(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.mix(), 1.0f);
    effect.setMix(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.mix(), 1.0f);
}

TEST(EffectSequenceContractTest, SortsEffectsByPipelineStageAndKeepsSameStageOrder)
{
    auto brightness = makeShared<BrightnessEffect>();
    auto invert = makeShared<InvertEffect>();
    brightness->setPipelineStage(EffectPipelineStage::Rasterizer);
    invert->setPipelineStage(EffectPipelineStage::PreProcess);
    const std::vector<ArtifactAbstractEffectPtr> unsorted{brightness, invert};

    EXPECT_FALSE(ArtifactAbstractEffect::isStageOrderValid(unsorted));

    const auto sorted = ArtifactAbstractEffect::sortedByStage(unsorted);
    ASSERT_EQ(sorted.size(), 2u);
    EXPECT_EQ(sorted[0].get(), invert.get());
    EXPECT_EQ(sorted[1].get(), brightness.get());
    EXPECT_TRUE(ArtifactAbstractEffect::isStageOrderValid(sorted));

    auto secondRasterizer = makeShared<InvertEffect>();
    secondRasterizer->setPipelineStage(EffectPipelineStage::Rasterizer);
    const std::vector<ArtifactAbstractEffectPtr> sameStage{
        brightness, secondRasterizer};
    const auto stable = ArtifactAbstractEffect::sortedByStage(sameStage);
    ASSERT_EQ(stable.size(), 2u);
    EXPECT_EQ(stable[0].get(), brightness.get());
    EXPECT_EQ(stable[1].get(), secondRasterizer.get());
}

TEST(EffectSequenceContractTest, SortedStagesProduceTheExpectedCpuImage)
{
    const float pixels[] = {0.20f, 0.40f, 0.80f, 0.35f};
    const auto source = makeSurface(1, 1, pixels);
    auto brightness = makeShared<BrightnessEffect>();
    auto invert = makeShared<InvertEffect>();
    brightness->setBrightness(0.10f);
    brightness->setPipelineStage(EffectPipelineStage::Rasterizer);
    invert->setStrength(1.0f);
    invert->setPipelineStage(EffectPipelineStage::PreProcess);

    const std::vector<ArtifactAbstractEffectPtr> unsorted{brightness, invert};
    const auto ordered = ArtifactAbstractEffect::sortedByStage(unsorted);
    ASSERT_EQ(ordered.size(), 2u);

    ImageF32x4RGBAWithCache intermediate;
    ImageF32x4RGBAWithCache output;
    ordered[0]->applyCPUOnly(source, intermediate);
    ordered[1]->applyCPUOnly(intermediate, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.90f, 0.70f, 0.30f, 0.35f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.20f, 0.40f, 0.80f, 0.35f));
}
