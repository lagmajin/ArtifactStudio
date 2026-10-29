#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.LuminescenceCaustics;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 11;
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

std::array<float, kChannelCount> makePattern(float alpha)
{
    std::array<float, kChannelCount> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = (x < kWidth / 2) ? 0.15f : 0.9f;
            pixels[offset] = value;
            pixels[offset + 1] = value * 0.8f;
            pixels[offset + 2] = value * 0.6f;
            pixels[offset + 3] = alpha;
        }
    }
    return pixels;
}

} // namespace

TEST(LuminescenceCausticsEffectContractTest,
     ZeroIntensityPreservesNonUniformPixelsExactly)
{
    const auto pixels = makePattern(0.65f);
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::LuminescenceCausticsEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setPropertyValue(ArtifactCore::UniString("Intensity"),
                            QVariant(0.0));

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto expected = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), expected.r());
            EXPECT_FLOAT_EQ(actual.g(), expected.g());
            EXPECT_FLOAT_EQ(actual.b(), expected.b());
            EXPECT_FLOAT_EQ(actual.a(), expected.a());
        }
    }
}

TEST(LuminescenceCausticsEffectContractTest,
     FullyTransparentSourceDoesNotReceiveCaustics)
{
    const auto pixels = makePattern(0.0f);
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::LuminescenceCausticsEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setPropertyValue(ArtifactCore::UniString("Threshold"),
                            QVariant(0.1));
    effect.setPropertyValue(ArtifactCore::UniString("Edge Weight"),
                            QVariant(2.0));
    effect.setPropertyValue(ArtifactCore::UniString("Intensity"),
                            QVariant(3.0));

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto expected = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), expected.r());
            EXPECT_FLOAT_EQ(actual.g(), expected.g());
            EXPECT_FLOAT_EQ(actual.b(), expected.b());
            EXPECT_FLOAT_EQ(actual.a(), 0.0f);
        }
    }
}
