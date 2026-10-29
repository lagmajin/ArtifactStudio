#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.ChromaticGlow;
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

TEST(ChromaticGlowEffectContractTest, BrightPixelSpreadsRgbGlowWithoutChangingAlpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    const int centerOffset = (4 * kWidth + 4) * 4;
    pixels[centerOffset] = 1.0f;
    pixels[centerOffset + 1] = 1.0f;
    pixels[centerOffset + 2] = 1.0f;
    pixels[centerOffset + 3] = 0.6f;
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    ChromaticGlowEffect effect;
    effect.setThreshold(0.2f);
    effect.setRadius(0.5f);
    effect.setIntensity(1.0f);
    effect.setDispersion(0.0f);
    effect.setTintMix(0.0f);

    effect.applyCPUOnly(source, output);

    const auto glow = output.image().getPixel(5, 4);
    EXPECT_GT(glow.r(), 0.0f);
    EXPECT_NEAR(glow.r(), glow.g(), 1.0e-6f);
    EXPECT_NEAR(glow.g(), glow.b(), 1.0e-6f);
    EXPECT_NEAR(glow.a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(4, 4).a(), 0.6f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(5, 4).r(), 0.0f, 1.0e-6f);
}

TEST(ChromaticGlowEffectContractTest, ThresholdAboveEveryPixelPreservesImage)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        pixels[pixel] = 0.1f;
        pixels[pixel + 1] = 0.2f;
        pixels[pixel + 2] = 0.3f;
        pixels[pixel + 3] = 0.5f;
    }
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    ChromaticGlowEffect effect;
    effect.setThreshold(1.0f);

    effect.applyCPUOnly(source, output);

    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(outputPixels[channel], pixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}

TEST(ChromaticGlowEffectContractTest, ZeroIntensityPreservesBrightInputExactly)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4u;
            pixels[offset] = 0.75f;
            pixels[offset + 1] = 0.65f;
            pixels[offset + 2] = 0.55f;
            pixels[offset + 3] = 0.2f + 0.05f * static_cast<float>((x + y) % 8);
        }
    }
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    ChromaticGlowEffect effect;
    effect.setThreshold(0.1f);
    effect.setRadius(0.5f);
    effect.setIntensity(0.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], original[channel])
            << "channel=" << channel;
    }
}

\n
