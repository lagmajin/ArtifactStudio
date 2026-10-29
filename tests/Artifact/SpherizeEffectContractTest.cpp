#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Spherize;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeSurface(const Pixels& rgba)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

std::size_t pixelOffset(int x, int y)
{
    return (static_cast<std::size_t>(y) * kWidth + x) * 4;
}

} // namespace

TEST(SpherizeEffectContractTest, ZeroRadiusPreservesEveryRgbaPixel)
{
    Pixels pixels{};
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<float>(i) / static_cast<float>(pixels.size());
    }
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::SpherizeEffect effect;
    effect.setRadius(0.0f);
    effect.setAmount(100.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const std::size_t offset = pixelOffset(x, y);
            EXPECT_FLOAT_EQ(actual.r(), pixels[offset]);
            EXPECT_FLOAT_EQ(actual.g(), pixels[offset + 1]);
            EXPECT_FLOAT_EQ(actual.b(), pixels[offset + 2]);
            EXPECT_FLOAT_EQ(actual.a(), pixels[offset + 3]);
        }
    }
}

TEST(SpherizeEffectContractTest, PositiveAmountUsesRadialSourceMapping)
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset = pixelOffset(x, y);
            const float value = static_cast<float>(y * kWidth + x);
            pixels[offset] = value;
            pixels[offset + 1] = value + 20.0f;
            pixels[offset + 2] = value + 40.0f;
            pixels[offset + 3] = 0.25f + value * 0.01f;
        }
    }
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::SpherizeEffect effect;
    effect.setAmount(100.0f);
    effect.setRadius(0.5f);

    effect.applyCPUOnly(source, output);

    // At (1, 2), normalized distance is 0.5. The configured radial mapping
    // moves the integer source sample inward to (0, 2).
    const auto actual = output.image().getPixel(1, 2);
    const std::size_t expectedOffset = pixelOffset(0, 2);
    EXPECT_FLOAT_EQ(actual.r(), pixels[expectedOffset]);
    EXPECT_FLOAT_EQ(actual.g(), pixels[expectedOffset + 1]);
    EXPECT_FLOAT_EQ(actual.b(), pixels[expectedOffset + 2]);
    EXPECT_FLOAT_EQ(actual.a(), pixels[expectedOffset + 3]);
}

TEST(SpherizeEffectContractTest, SettersClampFiniteValuesAndResetNonFiniteValues)
{
    Artifact::SpherizeEffect effect;
    effect.setAmount(150.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 100.0f);
    effect.setAmount(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.amount(), 50.0f);

    effect.setRadius(-1.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.0f);
    effect.setRadius(2.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 1.0f);
    effect.setRadius(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.radius(), 0.5f);

    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    effect.setCenterY(2.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);
    effect.setCenterY(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);
}
