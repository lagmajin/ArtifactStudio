#include <gtest/gtest.h>

#include <array>
#include <cmath>

import Artifact.Effect.Distort.DisplacementMap;
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

} // namespace

TEST(DisplacementMapEffectContractTest, DisplacementMapSamplesClampedAlphaDrivenOffset)
{
    constexpr std::array<float, 16> pixels = {
        0.1f, 0.2f, 0.3f, 1.0f,
        0.3f, 0.4f, 0.5f, 1.0f,
        0.5f, 0.6f, 0.7f, 1.0f,
        0.7f, 0.8f, 0.9f, 1.0f,
    };
    const auto source = makeSurface(pixels.data(), 4, 1);
    ImageF32x4RGBAWithCache output;
    DisplacementMapEffect effect;
    effect.setMaxHorizontal(2.0f);
    effect.setMaxVertical(0.0f);
    effect.setHorizontalChannel(DisplaceChannel::Alpha);
    effect.setVerticalChannel(DisplaceChannel::Alpha);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(0, 0).r(),
                source.image().getPixel(1, 0).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(2, 0).g(),
                source.image().getPixel(3, 0).g(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(3, 0).b(),
                source.image().getPixel(3, 0).b(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(0, 0).a(), 1.0f, 1.0e-6f);
    EXPECT_FLOAT_EQ(source.image().getPixel(0, 0).r(), 0.1f);
}

TEST(DisplacementMapEffectContractTest, WrapAroundSamplesFromOppositeImageEdge)
{
    constexpr std::array<float, 16> pixels = {
        0.1f, 0.2f, 0.3f, 1.0f,
        0.3f, 0.4f, 0.5f, 1.0f,
        0.5f, 0.6f, 0.7f, 1.0f,
        0.7f, 0.8f, 0.9f, 1.0f,
    };
    const auto source = makeSurface(pixels.data(), 4, 1);
    ImageF32x4RGBAWithCache output;
    DisplacementMapEffect effect;
    effect.setMaxHorizontal(2.0f);
    effect.setMaxVertical(0.0f);
    effect.setHorizontalChannel(DisplaceChannel::Alpha);
    effect.setVerticalChannel(DisplaceChannel::Alpha);
    effect.setWrapAround(true);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(3, 0).r(),
                source.image().getPixel(0, 0).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(3, 0).g(),
                source.image().getPixel(0, 0).g(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(3, 0).b(),
                source.image().getPixel(0, 0).b(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(3, 0).a(),
                source.image().getPixel(0, 0).a(), 1.0e-6f);
}

TEST(DisplacementMapEffectContractTest, FractionalOffsetUsesBilinearInterpolation)
{
    constexpr std::array<float, 8> pixels = {
        0.1f, 0.2f, 0.3f, 0.75f,
        0.3f, 0.4f, 0.5f, 0.5f,
    };
    const auto source = makeSurface(pixels.data(), 2, 1);
    ImageF32x4RGBAWithCache output;
    DisplacementMapEffect effect;
    effect.setMaxHorizontal(2.0f);
    effect.setMaxVertical(0.0f);
    effect.setHorizontalChannel(DisplaceChannel::Alpha);
    effect.setVerticalChannel(DisplaceChannel::Alpha);

    effect.applyCPUOnly(source, output);

    const auto interpolated = output.image().getPixel(0, 0);
    EXPECT_NEAR(interpolated.r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(interpolated.g(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(interpolated.b(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(interpolated.a(), 0.625f, 1.0e-6f);
}
\n
