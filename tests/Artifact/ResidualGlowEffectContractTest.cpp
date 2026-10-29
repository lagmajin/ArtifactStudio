#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.ResidualGlow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 9;
constexpr int kHeight = 9;
constexpr std::size_t kChannelCount =
    static_cast<std::size_t>(kWidth) * kHeight * 4;

ArtifactCore::ImageF32x4RGBAWithCache makeSurface(
    const std::array<float, kChannelCount>& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

std::array<float, kChannelCount> makeUniformSurface(float value, float alpha)
{
    std::array<float, kChannelCount> pixels{};
    for (std::size_t pixel = 0; pixel < kChannelCount / 4; ++pixel) {
        pixels[pixel * 4] = value;
        pixels[pixel * 4 + 1] = value;
        pixels[pixel * 4 + 2] = value;
        pixels[pixel * 4 + 3] = alpha;
    }
    return pixels;
}

} // namespace

TEST(ResidualGlowEffectContractTest, BrightHistoryContributesToFollowingDarkFrame)
{
    const auto brightPixels = makeUniformSurface(1.0f, 0.7f);
    const auto darkPixels = makeUniformSurface(0.0f, 0.35f);
    const auto bright = makeSurface(brightPixels);
    const auto dark = makeSurface(darkPixels);
    ArtifactCore::ImageF32x4RGBAWithCache ignoredBrightOutput;
    ArtifactCore::ImageF32x4RGBAWithCache historyOutput;
    ArtifactCore::ImageF32x4RGBAWithCache freshOutput;
    Artifact::ResidualGlowEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setThreshold(0.5f);
    effect.setRadius(0.5f);
    effect.setDecay(0.8f);
    effect.setIntensity(1.0f);
    effect.setHistoryMix(1.0f);

    effect.applyCPUOnly(bright, ignoredBrightOutput);
    effect.applyCPUOnly(dark, historyOutput);

    Artifact::ResidualGlowEffect freshEffect;
    freshEffect.setComputeMode(Artifact::ComputeMode::CPU);
    freshEffect.setThreshold(0.5f);
    freshEffect.setRadius(0.5f);
    freshEffect.setDecay(0.8f);
    freshEffect.setIntensity(1.0f);
    freshEffect.setHistoryMix(1.0f);
    freshEffect.applyCPUOnly(dark, freshOutput);

    const auto withHistory = historyOutput.image().getPixel(4, 4);
    const auto withoutHistory = freshOutput.image().getPixel(4, 4);
    EXPECT_GT(withHistory.r(), withoutHistory.r());
    EXPECT_GT(withHistory.g(), withoutHistory.g());
    EXPECT_GT(withHistory.b(), withoutHistory.b());
    EXPECT_FLOAT_EQ(withHistory.a(), 0.35f);
    EXPECT_FLOAT_EQ(withoutHistory.a(), 0.35f);
}

TEST(ResidualGlowEffectContractTest, ParameterChangeClearsAccumulatedHistory)
{
    const auto brightPixels = makeUniformSurface(1.0f, 0.7f);
    const auto darkPixels = makeUniformSurface(0.0f, 0.35f);
    const auto bright = makeSurface(brightPixels);
    const auto dark = makeSurface(darkPixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::ResidualGlowEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.applyCPUOnly(bright, output);
    effect.setThreshold(0.4f);

    effect.applyCPUOnly(dark, output);

    const auto actual = output.image().getPixel(4, 4);
    EXPECT_FLOAT_EQ(actual.r(), 0.0f);
    EXPECT_FLOAT_EQ(actual.g(), 0.0f);
    EXPECT_FLOAT_EQ(actual.b(), 0.0f);
    EXPECT_FLOAT_EQ(actual.a(), 0.35f);
}
