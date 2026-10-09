#include <gtest/gtest.h>

#include <limits>

import Artifact.Effect.Abstract;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import PosterizeEffect;

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

TEST(PosterizeEffectContractTest, QuantizesChannelsWithNearestLevelAndKeepsAlpha)
{
    const float pixels[] = {
        0.0f, 0.49f, 0.5f, 0.25f,
        1.0f, 0.2f, 0.8f, 0.75f,
    };
    const auto source = makeSurface(pixels, 2, 1);
    ImageF32x4RGBAWithCache output;
    PosterizeEffect effect;
    effect.setLevels(4.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0,
                    FloatRGBA(0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 0.25f));
    expectPixelNear(output, 1, 0,
                    FloatRGBA(1.0f, 1.0f / 3.0f, 2.0f / 3.0f, 0.75f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.0f, 0.49f, 0.5f, 0.25f));
}

TEST(PosterizeEffectContractTest, TwoLevelsUseHalfUpBoundary)
{
    const float pixels[] = {0.499f, 0.5f, 0.501f, 0.4f};
    const auto source = makeSurface(pixels, 1, 1);
    ImageF32x4RGBAWithCache output;
    PosterizeEffect effect;
    effect.setLevels(2.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.0f, 1.0f, 1.0f, 0.4f));
}

TEST(PosterizeEffectContractTest, LevelSetterClampsRangeAndResetsNonFiniteInput)
{
    PosterizeEffect effect;
    effect.setLevels(1.0f);
    EXPECT_FLOAT_EQ(effect.levels(), 2.0f);
    effect.setLevels(100.0f);
    EXPECT_FLOAT_EQ(effect.levels(), 64.0f);
    effect.setLevels(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.levels(), 4.0f);
}

TEST(PosterizeEffectContractTest, GpuSpatialDescriptorCarriesConfiguredLevels)
{
    PosterizeEffect effect;
    effect.setLevels(12.0f);
    GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey,
              gpuGenericKeyFromString("posterize"));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 12.0f);
}
