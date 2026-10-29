#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Magnify;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 8;

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

std::array<float, kWidth * kHeight * 4> makeGradient()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const int offset = (y * kWidth + x) * 4;
            pixels[offset] = static_cast<float>(x) / 7.0f;
            pixels[offset + 1] = static_cast<float>(y) / 7.0f;
            pixels[offset + 2] = 0.2f;
            pixels[offset + 3] = 0.7f;
        }
    }
    return pixels;
}

} // namespace

TEST(MagnifyEffectContractTest, LensExpandsCenterSamplingAndLeavesOutsideRadiusUntouched)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    MagnifyEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setRadius(0.5f);
    effect.setAmount(50.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(4, 4).r(),
                source.image().getPixel(4, 4).r(), 1.0e-6f);
    EXPECT_GT(output.image().getPixel(6, 4).r(),
              source.image().getPixel(6, 4).r());
    EXPECT_NEAR(output.image().getPixel(6, 4).a(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(0, 0).r(),
                source.image().getPixel(0, 0).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(0, 0).a(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(6, 4).r(), 6.0f / 7.0f, 1.0e-6f);
}

TEST(MagnifyEffectContractTest, ZeroRadiusPreservesTheEntireInput)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    MagnifyEffect effect;
    effect.setRadius(0.0f);

    effect.applyCPUOnly(source, output);

    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(outputPixels[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(MagnifyEffectContractTest, NegativeAmountContractsSamplingTowardTheCenter)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    MagnifyEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setRadius(0.5f);
    effect.setAmount(-50.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_LT(output.image().getPixel(6, 4).r(),
              source.image().getPixel(6, 4).r());
    EXPECT_NEAR(output.image().getPixel(6, 4).a(), 0.7f, 1.0e-6f);
}

TEST(MagnifyEffectContractTest, CenterRadiusAndAmountClampAndResetNonFiniteValues)
{
    MagnifyEffect effect;
    effect.setCenterX(-1.0f);
    effect.setCenterY(2.0f);
    effect.setRadius(-0.5f);
    effect.setAmount(120.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 100.0f);

    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    effect.setCenterY(std::numeric_limits<float>::infinity());
    effect.setRadius(std::numeric_limits<float>::quiet_NaN());
    effect.setAmount(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.5f);
    EXPECT_FLOAT_EQ(effect.amount(), 50.0f);
}
\n
