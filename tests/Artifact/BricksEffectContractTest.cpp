#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Rasterizer.Bricks;
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

TEST(BricksEffectContractTest, BricksPlacesMortarAndOffsetsAlternateRows)
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

TEST(BricksEffectContractTest, ZeroMortarFillsTheImageWithOpaqueWhite)
{
    constexpr std::array<float, 16> pixels = {
        0.1f, 0.2f, 0.3f, 0.0f,
        0.4f, 0.5f, 0.6f, 0.25f,
        0.7f, 0.8f, 0.9f, 0.75f,
        0.3f, 0.2f, 0.1f, 1.0f,
    };
    const auto source = makeSurface(pixels.data(), 2, 2);
    ImageF32x4RGBAWithCache output;
    BricksEffect effect;
    effect.setMortarWidth(0.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(pixel.r(), 1.0f);
            EXPECT_FLOAT_EQ(pixel.g(), 1.0f);
            EXPECT_FLOAT_EQ(pixel.b(), 1.0f);
            EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
        }
    }
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.1f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(0, 0).a(), 0.0f, 1.0e-6f);
}

TEST(BricksEffectContractTest, FullBrickOffsetRepeatsTheOriginalPattern)
{
    constexpr int width = 16;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 0.2f;
        pixels[pixel * 4 + 1] = 0.4f;
        pixels[pixel * 4 + 2] = 0.6f;
        pixels[pixel * 4 + 3] = 0.5f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache unshiftedOutput;
    ImageF32x4RGBAWithCache fullBrickOutput;
    BricksEffect effect;
    effect.setBrickWidth(8.0f);
    effect.setBrickHeight(4.0f);
    effect.setMortarWidth(1.0f);
    effect.setOffset(0.0f);
    effect.applyCPUOnly(source, unshiftedOutput);

    effect.setOffset(1.0f);
    effect.applyCPUOnly(source, fullBrickOutput);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto unshifted = unshiftedOutput.image().getPixel(x, y);
            const auto shifted = fullBrickOutput.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(shifted.r(), unshifted.r());
            EXPECT_FLOAT_EQ(shifted.g(), unshifted.g());
            EXPECT_FLOAT_EQ(shifted.b(), unshifted.b());
            EXPECT_FLOAT_EQ(shifted.a(), unshifted.a());
        }
    }
}
\n
