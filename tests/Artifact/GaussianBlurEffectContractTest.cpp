#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.GauusianBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 17;
constexpr int kHeight = 17;

ImageF32x4RGBAWithCache makeOpaqueImpulse()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        pixels[pixel + 3] = 1.0f;
    }
    constexpr int center = (kHeight / 2 * kWidth + kWidth / 2) * 4;
    pixels[center] = 1.0f;
    pixels[center + 1] = 1.0f;
    pixels[center + 2] = 1.0f;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(GaussianBlurEffectContractTest, SpreadsImpulseAndKeepsOpaqueCoverage)
{
    const auto source = makeOpaqueImpulse();
    ImageF32x4RGBAWithCache output;
    GaussianBlur effect;
    effect.setSigma(1.5f);
    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(8, 8);
    const auto neighbor = output.image().getPixel(9, 8);
    EXPECT_GT(neighbor.r(), 0.0f);
    EXPECT_LT(center.r(), 1.0f);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            EXPECT_NEAR(output.image().getPixel(x, y).a(), 1.0f, 1.0e-6f);
        }
    }
    EXPECT_FLOAT_EQ(source.image().getPixel(9, 8).r(), 0.0f);
}

TEST(GaussianBlurEffectContractTest, ZeroSigmaPreservesEveryRgbaChannel)
{
    const auto source = makeOpaqueImpulse();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ImageF32x4RGBAWithCache output;
    GaussianBlur effect;
    effect.setSigma(0.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_NEAR(outputPixels[channel], sourcePixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}

TEST(GaussianBlurEffectContractTest, TransparentSourceColorDoesNotBleedIntoBlurredEdge)
{
    constexpr int width = 9;
    std::array<float, width * 4> pixels{};
    pixels[2 * 4] = 1.0f; // Fully transparent red must not contribute color.
    pixels[5 * 4 + 2] = 1.0f;
    pixels[5 * 4 + 3] = 1.0f;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, 1);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    GaussianBlur effect;
    effect.setSigma(1.0f);

    effect.applyCPUOnly(source, output);

    const auto edge = output.image().getPixel(3, 0);
    EXPECT_GT(edge.a(), 0.0f);
    EXPECT_NEAR(edge.r(), 0.0f, 1.0e-6f);
    EXPECT_GT(edge.b(), 0.0f);
    EXPECT_FLOAT_EQ(source.image().getPixel(2, 0).r(), 1.0f);
}

TEST(GaussianBlurEffectContractTest, SigmaClampsAndResetsNonFiniteValues)
{
    GaussianBlur effect;
    effect.setSigma(-1.0f);
    EXPECT_FLOAT_EQ(effect.sigma(), 0.0f);
    effect.setSigma(80.0f);
    EXPECT_FLOAT_EQ(effect.sigma(), 64.0f);
    effect.setSigma(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.sigma(), 5.0f);
    effect.setSigma(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.sigma(), 5.0f);
}
