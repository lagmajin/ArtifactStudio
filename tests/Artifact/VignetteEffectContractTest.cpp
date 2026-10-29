#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Vignette;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 9;
constexpr int kHeight = 9;

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(VignetteEffectContractTest, VignetteDarkensCornersAndPreservesAlphaAndInput)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        pixels[pixel * 4] = 0.8f;
        pixels[pixel * 4 + 1] = 0.6f;
        pixels[pixel * 4 + 2] = 0.4f;
        pixels[pixel * 4 + 3] = 0.75f;
    }
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    VignetteEffect effect;
    effect.applyCPUOnly(source, output);

    const auto corner = output.image().getPixel(0, 0);
    const auto center = output.image().getPixel(kWidth / 2, kHeight / 2);
    EXPECT_LT(corner.r(), center.r());
    EXPECT_NEAR(corner.g() / corner.r(), 0.75f, 1.0e-5f);
    EXPECT_NEAR(corner.b() / corner.r(), 0.5f, 1.0e-5f);
    EXPECT_NEAR(corner.a(), 0.75f, 1.0e-6f);
    EXPECT_NEAR(center.a(), 0.75f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.8f, 1.0e-6f);
}

TEST(VignetteEffectContractTest, ZeroAmountPreservesTheEntireImage)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        pixels[pixel * 4] = static_cast<float>(pixel % kWidth) / kWidth;
        pixels[pixel * 4 + 1] = static_cast<float>(pixel / kWidth) / kHeight;
        pixels[pixel * 4 + 2] = 0.3f;
        pixels[pixel * 4 + 3] = 0.4f + 0.05f * static_cast<float>(pixel % 5);
    }
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    VignetteEffect effect;
    effect.setAmount(0.0f);

    effect.applyCPUOnly(source, output);

    const float* sourcePixels = source.image().rgba32fData();
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(outputPixels[channel], sourcePixels[channel])
            << "channel=" << channel;
    }
}

TEST(VignetteEffectContractTest, ConfiguredCenterPixelRemainsUnchangedAtMaximumAmount)
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 0.8f;
        pixels[pixel * 4 + 1] = 0.45f;
        pixels[pixel * 4 + 2] = 0.2f;
        pixels[pixel * 4 + 3] = 0.63f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    VignetteEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setAmount(1.0f);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(4, 4);
    EXPECT_FLOAT_EQ(center.r(), 0.8f);
    EXPECT_FLOAT_EQ(center.g(), 0.45f);
    EXPECT_FLOAT_EQ(center.b(), 0.2f);
    EXPECT_FLOAT_EQ(center.a(), 0.63f);
}
