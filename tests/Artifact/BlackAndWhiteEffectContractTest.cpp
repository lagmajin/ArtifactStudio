#include <gtest/gtest.h>

#include <QColor>
#include <cstddef>

import Artifact.Effect.Abstract;
import BlackAndWhiteEffect;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

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

TEST(BlackAndWhiteEffectContractTest, HueWeightsConvertPrimariesAndKeepNeutralLuma)
{
    const float pixels[] = {
        1.0f, 0.0f, 0.0f, 0.2f,
        0.0f, 1.0f, 0.0f, 0.4f,
        0.0f, 0.0f, 1.0f, 0.6f,
        0.5f, 0.5f, 0.5f, 0.8f,
    };
    const auto source = makeSurface(pixels, 4, 1);
    ImageF32x4RGBAWithCache output;
    BlackAndWhiteEffect effect;

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.4f, 0.4f, 0.4f, 0.2f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.4f, 0.4f, 0.4f, 0.4f));
    expectPixelNear(output, 2, 0, FloatRGBA(0.2f, 0.2f, 0.2f, 0.6f));
    expectPixelNear(output, 3, 0, FloatRGBA(0.5f, 0.5f, 0.5f, 0.8f));
}

TEST(BlackAndWhiteEffectContractTest, InterpolatesAdjacentHueWeights)
{
    const float pixels[] = {1.0f, 0.5f, 0.0f, 0.7f};
    const auto source = makeSurface(pixels, 1, 1);
    ImageF32x4RGBAWithCache output;
    BlackAndWhiteEffect effect;
    effect.setReds(0.1f);
    effect.setYellows(0.3f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.2f, 0.2f, 0.2f, 0.7f));
}

TEST(BlackAndWhiteEffectContractTest, TintAppliesToMonochromeResultWithoutChangingAlpha)
{
    const float pixels[] = {1.0f, 0.0f, 0.0f, 0.65f};
    const auto source = makeSurface(pixels, 1, 1);
    ImageF32x4RGBAWithCache output;
    BlackAndWhiteEffect effect;
    effect.setTintColor(QColor::fromRgbF(1.0, 0.5, 0.0));
    effect.setTintAmount(1.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.4f, 0.2f, 0.0f, 0.65f));
    expectPixelNear(source, 0, 0, FloatRGBA(1.0f, 0.0f, 0.0f, 0.65f));
}

TEST(BlackAndWhiteEffectContractTest, GpuSpatialDescriptorCarriesHueWeightsAndTintAmount)
{
    BlackAndWhiteEffect effect;
    effect.setReds(0.1f);
    effect.setYellows(0.2f);
    effect.setGreens(0.3f);
    effect.setCyans(0.4f);
    effect.setBlues(0.5f);
    effect.setMagentas(0.6f);
    effect.setTintAmount(0.7f);
    GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey, effect.gpuGenericKey());
    EXPECT_EQ(stack.nodes[0].genericKey,
              gpuGenericKeyFromString("black_and_white"));
    for (std::size_t i = 0; i < 7; ++i) {
        EXPECT_FLOAT_EQ(stack.nodes[0].parameters[i],
                        static_cast<float>(i + 1) / 10.0f);
    }
}
