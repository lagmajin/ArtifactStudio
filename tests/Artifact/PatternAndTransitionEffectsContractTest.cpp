#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Bricks;
import Artifact.Effect.Rasterizer.LinearWipe;
import Artifact.Effect.Rasterizer.Stripes;
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

TEST(PatternAndTransitionEffectsContractTest, StripesGenerateBinaryPatternFromFrequencyAndThickness)
{
    constexpr int width = 8;
    constexpr std::array<float, width * 4> pixels = {
        0.2f, 0.3f, 0.4f, 0.6f, 0.2f, 0.3f, 0.4f, 0.6f,
        0.2f, 0.3f, 0.4f, 0.6f, 0.2f, 0.3f, 0.4f, 0.6f,
        0.2f, 0.3f, 0.4f, 0.6f, 0.2f, 0.3f, 0.4f, 0.6f,
        0.2f, 0.3f, 0.4f, 0.6f, 0.2f, 0.3f, 0.4f, 0.6f,
    };
    const auto source = makeSurface(pixels.data(), width, 1);
    ImageF32x4RGBAWithCache output;
    StripesEffect effect;
    effect.setFrequency(1.0f);
    effect.setThickness(0.5f);
    effect.setAngle(0.0f);
    effect.setOffset(0.0f);

    effect.applyCPUOnly(source, output);

    for (int x = 0; x < width; ++x) {
        const float expected = x < 4 ? 1.0f : 0.0f;
        const auto pixel = output.image().getPixel(x, 0);
        EXPECT_NEAR(pixel.r(), expected, 1.0e-6f) << "x=" << x;
        EXPECT_NEAR(pixel.g(), expected, 1.0e-6f) << "x=" << x;
        EXPECT_NEAR(pixel.b(), expected, 1.0e-6f) << "x=" << x;
        EXPECT_FLOAT_EQ(pixel.a(), 1.0f) << "x=" << x;
    }
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.2f, 1.0e-6f);
}
TEST(PatternAndTransitionEffectsContractTest, BricksPlacesMortarAndOffsetsAlternateRows)
{
    constexpr int width = 16;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 0.4f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.6f;
        pixels[pixel * 4 + 3] = 0.7f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    BricksEffect effect;
    effect.setBrickWidth(8.0f);
    effect.setBrickHeight(4.0f);
    effect.setMortarWidth(1.0f);
    effect.setOffset(0.5f);

    effect.applyCPUOnly(source, output);

    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).r(), 0.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(2, 2).r(), 1.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(4, 4).r(), 0.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(6, 6).r(), 1.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(6, 6).a(), 1.0f);
    EXPECT_NEAR(source.image().getPixel(2, 2).r(), 0.4f, 1.0e-6f);
}

TEST(PatternAndTransitionEffectsContractTest, LinearWipeReducesAlphaAcrossImageAndKeepsRgb)
{
    constexpr int width = 4;
    constexpr std::array<float, width * 4> pixels = {
        0.8f, 0.4f, 0.2f, 0.75f,
        0.8f, 0.4f, 0.2f, 0.75f,
        0.8f, 0.4f, 0.2f, 0.75f,
        0.8f, 0.4f, 0.2f, 0.75f,
    };
    const auto source = makeSurface(pixels.data(), width, 1);
    ImageF32x4RGBAWithCache output;
    LinearWipeEffect effect;
    effect.setAngle(0.0f);
    effect.setSoftness(1.0f);

    effect.applyCPUOnly(source, output);

    const auto first = output.image().getPixel(0, 0);
    const auto middle = output.image().getPixel(2, 0);
    const auto last = output.image().getPixel(3, 0);
    EXPECT_NEAR(first.a(), 0.75f, 1.0e-6f);
    EXPECT_NEAR(middle.a(), 0.375f, 1.0e-6f);
    EXPECT_NEAR(last.a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(middle.r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(middle.g(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(2, 0).a(), 0.75f, 1.0e-6f);
}
