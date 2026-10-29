#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Sharpen;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 9;
constexpr int kHeight = 9;
constexpr int kCenter = (kHeight / 2) * kWidth + kWidth / 2;

std::array<float, kWidth * kHeight * 4> makeImpulseImage()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        pixels[pixel * 4] = 0.1f;
        pixels[pixel * 4 + 1] = 0.2f;
        pixels[pixel * 4 + 2] = 0.3f;
        pixels[pixel * 4 + 3] = 0.6f;
    }
    pixels[kCenter * 4] = 0.6f;
    pixels[kCenter * 4 + 1] = 0.7f;
    pixels[kCenter * 4 + 2] = 0.8f;
    return pixels;
}

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(SharpenEffectContractTest, EnhancesImpulseContrastAndPreservesAlphaAndInput)
{
    const auto pixels = makeImpulseImage();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    SharpenEffect effect;
    effect.setAmount(1.0f);
    effect.setSigma(1.0f);
    effect.setThreshold(0.0f);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(kWidth / 2, kHeight / 2);
    const auto adjacent = output.image().getPixel(kWidth / 2 + 1, kHeight / 2);
    EXPECT_GT(center.r(), 0.6f);
    EXPECT_LT(adjacent.r(), 0.1f);

    const float* outputPixels = output.image().rgba32fData();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    ASSERT_NE(sourcePixels, nullptr);
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        EXPECT_NEAR(outputPixels[pixel * 4 + 3], 0.6f, 1.0e-6f)
            << "alpha pixel=" << pixel;
        EXPECT_FLOAT_EQ(sourcePixels[pixel * 4], pixels[pixel * 4])
            << "source red pixel=" << pixel;
    }
}

TEST(SharpenEffectContractTest, ZeroAmountPreservesEveryInputChannel)
{
    const auto pixels = makeImpulseImage();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    SharpenEffect effect;
    effect.setAmount(0.0f);

    effect.applyCPUOnly(source, output);

    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (int channel = 0; channel < kWidth * kHeight * 4; ++channel) {
        EXPECT_NEAR(outputPixels[channel], pixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}

TEST(SharpenEffectContractTest, SettingsClampAndResetNonFiniteValues)
{
    SharpenEffect effect;
    effect.setAmount(-1.0f);
    effect.setSigma(12.0f);
    effect.setThreshold(2.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 0.0f);
    EXPECT_FLOAT_EQ(effect.sigma(), 10.0f);
    EXPECT_FLOAT_EQ(effect.threshold(), 1.0f);

    effect.setAmount(std::numeric_limits<float>::infinity());
    effect.setSigma(std::numeric_limits<float>::quiet_NaN());
    effect.setThreshold(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.amount(), 1.0f);
    EXPECT_FLOAT_EQ(effect.sigma(), 1.0f);
    EXPECT_FLOAT_EQ(effect.threshold(), 0.0f);
}
\n
