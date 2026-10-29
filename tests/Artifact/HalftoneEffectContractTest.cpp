#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Halftone;
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

TEST(HalftoneEffectContractTest, HalftoneCreatesDotCentersAndPreservesSource)
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 1.0f;
        pixels[pixel * 4 + 1] = 1.0f;
        pixels[pixel * 4 + 2] = 1.0f;
        pixels[pixel * 4 + 3] = 0.5f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    HalftoneEffect effect;
    effect.setDotSize(4.0f);
    effect.setAngle(0.0f);
    effect.setContrast(1.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).r(), 0.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(2, 0).r(), 1.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).a(), 1.0f);
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(0, 0).a(), 0.5f, 1.0e-6f);
}

TEST(HalftoneEffectContractTest, ProducesOpaqueMonochromePixelsForColoredInput)
{
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr std::array<float, width * height * 4> pixels = {
        1.0f, 0.0f, 0.0f, 0.1f,
        0.0f, 1.0f, 0.0f, 0.3f,
        0.0f, 0.0f, 1.0f, 0.5f,
        0.2f, 0.4f, 0.8f, 0.7f,
        0.8f, 0.3f, 0.1f, 0.9f,
        0.4f, 0.5f, 0.6f, 0.0f,
    };
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    HalftoneEffect effect;
    effect.setDotSize(3.0f);
    effect.setAngle(17.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(pixel.r(), pixel.g());
            EXPECT_FLOAT_EQ(pixel.g(), pixel.b());
            EXPECT_TRUE(pixel.r() == 0.0f || pixel.r() == 1.0f);
            EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
        }
    }
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(0, 0).a(), 0.1f, 1.0e-6f);
}

TEST(HalftoneEffectContractTest, ZeroContrastRemovesInputLuminanceFromDotPattern)
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::array<float, width * height * 4> blackPixels{};
    std::array<float, width * height * 4> whitePixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        blackPixels[pixel * 4 + 3] = 1.0f;
        whitePixels[pixel * 4] = 1.0f;
        whitePixels[pixel * 4 + 1] = 1.0f;
        whitePixels[pixel * 4 + 2] = 1.0f;
        whitePixels[pixel * 4 + 3] = 1.0f;
    }
    const auto blackSource = makeSurface(blackPixels.data(), width, height);
    const auto whiteSource = makeSurface(whitePixels.data(), width, height);
    ImageF32x4RGBAWithCache blackOutput;
    ImageF32x4RGBAWithCache whiteOutput;
    HalftoneEffect effect;
    effect.setDotSize(4.0f);
    effect.setAngle(0.0f);
    effect.setContrast(0.0f);
    effect.applyCPUOnly(blackSource, blackOutput);
    effect.applyCPUOnly(whiteSource, whiteOutput);

    const float* black = blackOutput.image().rgba32fData();
    const float* white = whiteOutput.image().rgba32fData();
    ASSERT_NE(black, nullptr);
    ASSERT_NE(white, nullptr);
    for (std::size_t component = 0;
         component < static_cast<std::size_t>(width * height * 4); ++component) {
        EXPECT_FLOAT_EQ(black[component], white[component])
            << "component=" << component;
    }
}

TEST(HalftoneEffectContractTest, OnePixelDotSizePlacesEveryPixelAtADotCenter)
{
    constexpr int width = 4;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 1.0f;
        pixels[pixel * 4 + 1] = 1.0f;
        pixels[pixel * 4 + 2] = 1.0f;
        pixels[pixel * 4 + 3] = 0.4f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    HalftoneEffect effect;
    effect.setDotSize(1.0f);
    effect.setAngle(0.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(pixel.r(), 0.0f);
            EXPECT_FLOAT_EQ(pixel.g(), 0.0f);
            EXPECT_FLOAT_EQ(pixel.b(), 0.0f);
            EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
        }
    }
    EXPECT_FLOAT_EQ(source.image().getPixel(0, 0).r(), 1.0f);
    EXPECT_FLOAT_EQ(source.image().getPixel(0, 0).a(), 0.4f);
}
\n
