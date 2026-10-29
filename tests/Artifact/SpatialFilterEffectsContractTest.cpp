#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.FindEdges;
import Artifact.Effect.Spherize;
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

TEST(SpatialFilterEffectsContractTest, SpherizePreservesCenterAndPixelsOutsideRadius)
{
    constexpr int width = 7;
    constexpr int height = 7;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = static_cast<float>(x) / 6.0f;
            pixels[offset + 1] = static_cast<float>(y) / 6.0f;
            pixels[offset + 2] = 0.25f;
            pixels[offset + 3] = 0.6f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    SpherizeEffect effect;
    effect.setAmount(100.0f);
    effect.setRadius(0.5f);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(3, 3);
    EXPECT_NEAR(center.r(), source.image().getPixel(3, 3).r(), 1.0e-6f);
    EXPECT_NEAR(center.g(), source.image().getPixel(3, 3).g(), 1.0e-6f);
    const auto outsideRadius = output.image().getPixel(0, 0);
    const auto sourceOutsideRadius = source.image().getPixel(0, 0);
    EXPECT_NEAR(outsideRadius.r(), sourceOutsideRadius.r(), 1.0e-6f);
    EXPECT_NEAR(outsideRadius.g(), sourceOutsideRadius.g(), 1.0e-6f);
    EXPECT_NEAR(outsideRadius.b(), sourceOutsideRadius.b(), 1.0e-6f);
    EXPECT_NEAR(outsideRadius.a(), sourceOutsideRadius.a(), 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(3, 3).a(), 0.6f, 1.0e-6f);
}

TEST(SpatialFilterEffectsContractTest, FindEdgesDetectsStepAndPreservesAlphaAndInput)
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
