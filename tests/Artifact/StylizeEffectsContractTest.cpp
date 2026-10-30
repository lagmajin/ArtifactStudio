#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Bevel;
import Artifact.Effect.Dithering;
import Artifact.Effect.Kuwahara;
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

TEST(StylizeEffectsContractTest, BayerDitheringQuantizesRgbAndPreservesAlpha)
{
    constexpr std::array<float, 4> pixels = {0.6f, 0.6f, 0.6f, 0.35f};
    const auto source = makeSurface(pixels.data(), 1, 1);
    ImageF32x4RGBAWithCache output;
    DitheringEffect effect;
    effect.setAlgorithm(DitherAlgorithm::Bayer4x4);
    effect.setColorCount(4);
    effect.setAmount(1.0f);

    effect.applyCPUOnly(source, output);

    const auto pixel = output.image().getPixel(0, 0);
    EXPECT_NEAR(pixel.r(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.5f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixel.a(), 0.35f);
    EXPECT_FLOAT_EQ(source.image().getPixel(0, 0).r(), 0.6f);
}

TEST(StylizeEffectsContractTest, KuwaharaPreservesUniformImage)
{
    constexpr int width = 5;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        pixels[pixel] = 0.25f;
        pixels[pixel + 1] = 0.5f;
        pixels[pixel + 2] = 0.75f;
        pixels[pixel + 3] = 0.4f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    KuwaharaEffect effect;
    effect.setRadius(2.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), 0.25f, 1.0e-5f) << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.g(), 0.5f, 1.0e-5f) << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.b(), 0.75f, 1.0e-5f) << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.a(), 0.4f, 1.0e-5f) << "x=" << x << " y=" << y;
        }
    }
    EXPECT_FLOAT_EQ(source.image().getPixel(2, 2).g(), 0.5f);
}

TEST(StylizeEffectsContractTest, BevelAddsRimAtAlphaBoundaryAndPreservesAlpha)
{
    constexpr int width = 9;
    constexpr int height = 9;
    std::array<float, width * height * 4> pixels{};
    for (int y = 2; y <= 6; ++y) {
        for (int x = 2; x <= 6; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = 1.0f;
            pixels[offset + 1] = 1.0f;
            pixels[offset + 2] = 1.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    BevelEffect effect;
    effect.setStrength(1.0f);
    effect.setSoftness(1.0f);
    effect.setEdgeMode(false);

    effect.applyCPUOnly(source, output);

    bool boundaryBrightened = false;
    for (int y = 2; y <= 6; ++y) {
        for (int x = 2; x <= 6; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            boundaryBrightened = boundaryBrightened || pixel.r() > 1.0f + 1.0e-5f;
            EXPECT_NEAR(pixel.a(), 1.0f, 1.0e-6f);
        }
    }
    EXPECT_TRUE(boundaryBrightened);
    EXPECT_NEAR(output.image().getPixel(0, 0).a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(4, 2).r(), 1.0f, 1.0e-6f);
}
