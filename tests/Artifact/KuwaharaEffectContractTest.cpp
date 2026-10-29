#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
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

TEST(KuwaharaEffectContractTest, KuwaharaPreservesUniformImage)
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

TEST(KuwaharaEffectContractTest, AppliesSelectedQuadrantWeightsToAlphaAtAnEdge)
{
    constexpr int width = 5;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = 0.4f;
            pixels[offset + 1] = 0.5f;
            pixels[offset + 2] = 0.6f;
            pixels[offset + 3] = x < 2 ? 0.0f : 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    KuwaharaEffect effect;
    effect.setRadius(1.0f);
    effect.setSharpness(0.5f);

    effect.applyCPUOnly(source, output);

    const auto edgePixel = output.image().getPixel(2, 2);
    EXPECT_NEAR(edgePixel.r(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(edgePixel.g(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(edgePixel.b(), 0.6f, 1.0e-6f);
    EXPECT_NEAR(edgePixel.a(), 0.5f, 1.0e-6f);
    EXPECT_FLOAT_EQ(source.image().getPixel(2, 2).a(), 1.0f);
}

TEST(KuwaharaEffectContractTest, SettersClampRangesAndRestoreNonFiniteDefaults)
{
    KuwaharaEffect effect;
    effect.setRadius(-4.0f);
    effect.setSharpness(2.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 1.0f);
    EXPECT_FLOAT_EQ(effect.sharpness(), 1.0f);

    effect.setRadius(std::numeric_limits<float>::infinity());
    effect.setSharpness(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.radius(), 5.0f);
    EXPECT_FLOAT_EQ(effect.sharpness(), 0.5f);
}
