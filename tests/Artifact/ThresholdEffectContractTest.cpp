#include <gtest/gtest.h>

#include <limits>

import Artifact.Effect.Abstract;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import ThresholdEffect;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

void expectPixelNear(const ImageF32x4RGBAWithCache& surface, int x, int y,
                     const FloatRGBA& expected, float tolerance = 1.0e-6f)
{
    const FloatRGBA actual = surface.image().getPixel(x, y);
    EXPECT_NEAR(actual.r(), expected.r(), tolerance);
    EXPECT_NEAR(actual.g(), expected.g(), tolerance);
    EXPECT_NEAR(actual.b(), expected.b(), tolerance);
    EXPECT_NEAR(actual.a(), expected.a(), tolerance);
}

} // namespace

TEST(ThresholdEffectContractTest, LumaThresholdIsInclusiveAndPreservesAlpha)
{
    const float pixels[] = {
        0.5f, 0.0f, 0.0f, 0.3f,
        0.0f, 0.5f, 0.0f, 0.7f,
        0.5f, 0.5f, 0.5f, 0.9f,
    };
    const auto source = makeSurface(pixels, 3, 1);
    ImageF32x4RGBAWithCache output;
    ThresholdEffect effect;
    effect.setThreshold(0.2f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.0f, 0.0f, 0.0f, 0.3f));
    expectPixelNear(output, 1, 0, FloatRGBA(1.0f, 1.0f, 1.0f, 0.7f));

    constexpr float neutralLuma = 0.5f * 0.299f + 0.5f * 0.587f +
                                  0.5f * 0.114f;
    effect.setThreshold(neutralLuma);
    effect.applyCPUOnly(source, output);
    expectPixelNear(output, 2, 0, FloatRGBA(1.0f, 1.0f, 1.0f, 0.9f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.5f, 0.0f, 0.0f, 0.3f));
}

TEST(ThresholdEffectContractTest, SetterClampsThresholdAndResetsNonFiniteInput)
{
    ThresholdEffect effect;
    effect.setThreshold(-1.0f);
    EXPECT_FLOAT_EQ(effect.threshold(), 0.0f);
    effect.setThreshold(2.0f);
    EXPECT_FLOAT_EQ(effect.threshold(), 1.0f);
    effect.setThreshold(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.threshold(), 0.5f);
}

TEST(ThresholdEffectContractTest, GpuSpatialDescriptorCarriesThreshold)
{
    ThresholdEffect effect;
    effect.setThreshold(0.37f);
    GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey,
              gpuGenericKeyFromString("threshold"));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 0.37f);
}
