#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.ReactiveGlow;
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

} // namespace

TEST(ReactiveGlowEffectContractTest,
     HighlightCreatesSpatialBloomAndPreservesInputAlpha)
{
    std::array<float, kChannelCount> pixels{};
    for (std::size_t pixel = 0; pixel < kChannelCount / 4; ++pixel) {
        pixels[pixel * 4 + 3] = 0.25f;
    }
    const std::size_t highlight =
        static_cast<std::size_t>((4 * kWidth + 4) * 4);
    pixels[highlight] = 1.0f;
    pixels[highlight + 1] = 1.0f;
    pixels[highlight + 2] = 1.0f;
    pixels[highlight + 3] = 0.8f;
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::ReactiveGlowEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setThreshold(0.1f);
    effect.setRadius(0.5f);
    effect.setIntensity(1.0f);
    effect.setReaction(1.0f);
    effect.setSaturationWeight(0.0f);
    effect.setTintMix(0.0f);

    effect.applyCPUOnly(source, output);

    const auto sourcePixel = source.image().getPixel(3, 4);
    const auto bloomPixel = output.image().getPixel(3, 4);
    EXPECT_FLOAT_EQ(sourcePixel.r(), 0.0f);
    EXPECT_GT(bloomPixel.r(), sourcePixel.r());
    EXPECT_GT(bloomPixel.g(), sourcePixel.g());
    EXPECT_GT(bloomPixel.b(), sourcePixel.b());
    EXPECT_FLOAT_EQ(bloomPixel.a(), 0.25f);
    EXPECT_FLOAT_EQ(output.image().getPixel(4, 4).a(), 0.8f);
}

TEST(ReactiveGlowEffectContractTest, ZeroIntensityIsAnExactColorIdentity)
{
    constexpr std::array<float, kChannelCount> pixels{
        0.2f, 0.7f, 0.4f, 0.1f,
        0.8f, 0.3f, 0.5f, 0.9f,
    };
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::ReactiveGlowEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setIntensity(0.0f);
    effect.setTintMix(0.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_FLOAT_EQ(actual[index], pixels[index])
            << "channel index=" << index;
    }
}
