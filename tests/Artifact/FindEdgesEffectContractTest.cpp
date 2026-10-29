#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.FindEdges;
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

TEST(FindEdgesEffectContractTest, FindEdgesDetectsStepAndPreservesAlphaAndInput)
{
    constexpr int width = 5;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float value = x < 2 ? 0.0f : 1.0f;
            const int offset = (y * width + x) * 4;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 0.4f + 0.1f * static_cast<float>(x);
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    FindEdgesEffect effect;

    effect.applyCPUOnly(source, output);

    const auto flat = output.image().getPixel(0, 1);
    const auto edge = output.image().getPixel(2, 1);
    EXPECT_GT(edge.r(), flat.r());
    EXPECT_NEAR(edge.r(), edge.g(), 1.0e-6f);
    EXPECT_NEAR(edge.g(), edge.b(), 1.0e-6f);
    for (int x = 0; x < width; ++x) {
        EXPECT_NEAR(output.image().getPixel(x, 1).a(),
                    0.4f + 0.1f * static_cast<float>(x), 1.0e-6f);
    }
    EXPECT_NEAR(source.image().getPixel(0, 1).r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(4, 1).r(), 1.0f, 1.0e-6f);
}

TEST(FindEdgesEffectContractTest, InvertedFlatImageProducesWhiteWithoutChangingAlpha)
{
    constexpr int width = 3;
    constexpr int height = 2;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 0.25f;
        pixels[pixel * 4 + 1] = 0.4f;
        pixels[pixel * 4 + 2] = 0.7f;
        pixels[pixel * 4 + 3] = 0.1f * static_cast<float>(pixel + 1);
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    FindEdgesEffect effect;
    effect.setInvert(true);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto result = output.image().getPixel(x, y);
            EXPECT_NEAR(result.r(), 1.0f, 1.0e-6f);
            EXPECT_NEAR(result.g(), 1.0f, 1.0e-6f);
            EXPECT_NEAR(result.b(), 1.0f, 1.0e-6f);
            EXPECT_NEAR(result.a(), 0.1f * static_cast<float>(y * width + x + 1),
                        1.0e-6f);
        }
    }
    EXPECT_NEAR(source.image().getPixel(1, 1).r(), 0.25f, 1.0e-6f);
}

TEST(FindEdgesEffectContractTest, FlatImageProducesBlackWithoutChangingPerPixelAlpha)
{
    constexpr int width = 4;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 0.35f;
        pixels[pixel * 4 + 1] = 0.35f;
        pixels[pixel * 4 + 2] = 0.35f;
        pixels[pixel * 4 + 3] = static_cast<float>(pixel) / 11.0f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    FindEdgesEffect effect;

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto result = output.image().getPixel(x, y);
            EXPECT_NEAR(result.r(), 0.0f, 1.0e-6f);
            EXPECT_NEAR(result.g(), 0.0f, 1.0e-6f);
            EXPECT_NEAR(result.b(), 0.0f, 1.0e-6f);
            EXPECT_NEAR(result.a(), pixels[(y * width + x) * 4 + 3], 1.0e-6f);
        }
    }
}
\n
