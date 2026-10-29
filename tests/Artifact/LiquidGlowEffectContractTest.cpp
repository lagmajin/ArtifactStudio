#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.LiquidGlow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

namespace {

constexpr int kWidth = 17;
constexpr int kHeight = 17;
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

TEST(LiquidGlowEffectContractTest,
     HighlightCreatesNeighboringGlowAndPreservesAlpha)
{
    std::array<float, kChannelCount> pixels{};
    for (std::size_t pixel = 0; pixel < kChannelCount / 4; ++pixel) {
        pixels[pixel * 4 + 3] =
            static_cast<float>((pixel % 11) + 1) / 12.0f;
    }
    const std::size_t highlight =
        static_cast<std::size_t>((8 * kWidth + 8) * 4);
    pixels[highlight] = 1.0f;
    pixels[highlight + 1] = 1.0f;
    pixels[highlight + 2] = 1.0f;
    pixels[highlight + 3] = 0.9f;
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::LiquidGlowEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setThreshold(0.1f);
    effect.setRadius(1.0f);
    effect.setDistortion(0.0f);
    effect.setIntensity(1.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const auto originalNeighbor = source.image().getPixel(8, 7);
    const auto glowNeighbor = output.image().getPixel(8, 7);
    EXPECT_GT(glowNeighbor.r(), originalNeighbor.r());
    EXPECT_GT(glowNeighbor.g(), originalNeighbor.g());
    EXPECT_GT(glowNeighbor.b(), originalNeighbor.b());
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            EXPECT_FLOAT_EQ(output.image().getPixel(x, y).a(),
                            source.image().getPixel(x, y).a())
                << "x=" << x << " y=" << y;
        }
    }
}

TEST(LiquidGlowEffectContractTest, ZeroIntensityPreservesImageExactly)
{
    std::array<float, kChannelCount> pixels{};
    for (std::size_t pixel = 0; pixel < kChannelCount / 4; ++pixel) {
        pixels[pixel * 4] = static_cast<float>((pixel * 3) % 17) / 20.0f;
        pixels[pixel * 4 + 1] = static_cast<float>((pixel * 5) % 13) / 16.0f;
        pixels[pixel * 4 + 2] = static_cast<float>((pixel * 7) % 11) / 14.0f;
        pixels[pixel * 4 + 3] = static_cast<float>((pixel % 9) + 1) / 10.0f;
    }
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::LiquidGlowEffect effect;
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
