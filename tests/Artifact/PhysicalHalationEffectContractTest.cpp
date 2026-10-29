#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.PhysicalHalation;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

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

TEST(PhysicalHalationEffectContractTest,
     BelowThresholdImagePassesThroughUnchanged)
{
    std::array<float, kChannelCount> pixels{};
    for (std::size_t pixel = 0; pixel < kChannelCount / 4; ++pixel) {
        pixels[pixel * 4] = 0.2f;
        pixels[pixel * 4 + 1] = 0.2f;
        pixels[pixel * 4 + 2] = 0.2f;
        pixels[pixel * 4 + 3] = 0.4f;
    }
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::PhysicalHalationEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);

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

TEST(PhysicalHalationEffectContractTest,
     HighlightDiffusesToNeighborWithoutChangingAlpha)
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
    Artifact::PhysicalHalationEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setPropertyValue(ArtifactCore::UniString("Threshold"),
                            QVariant(0.4f));
    effect.setPropertyValue(ArtifactCore::UniString("Spread"),
                            QVariant(1.0f));
    effect.setPropertyValue(ArtifactCore::UniString("Red Diffusion"),
                            QVariant(1.0f));
    effect.setPropertyValue(ArtifactCore::UniString("Intensity"),
                            QVariant(1.0f));

    effect.applyCPUOnly(source, output);

    const auto originalNeighbor = source.image().getPixel(3, 4);
    const auto diffusedNeighbor = output.image().getPixel(3, 4);
    EXPECT_GT(diffusedNeighbor.r(), originalNeighbor.r());
    EXPECT_FLOAT_EQ(diffusedNeighbor.g(), originalNeighbor.g());
    EXPECT_FLOAT_EQ(diffusedNeighbor.b(), originalNeighbor.b());
    EXPECT_FLOAT_EQ(diffusedNeighbor.a(), 0.25f);
    EXPECT_FLOAT_EQ(output.image().getPixel(4, 4).a(), 0.8f);
}
