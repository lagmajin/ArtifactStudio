#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Transform.Bend;
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

} // namespace

TEST(BendTransformContractTest, ZeroAmountPreservesEveryRgbaPixel)
{
    Pixels pixels{};
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<float>(i) / static_cast<float>(pixels.size());
    }
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::BendTransform effect;

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            EXPECT_FLOAT_EQ(actual.r(), pixels[offset]);
            EXPECT_FLOAT_EQ(actual.g(), pixels[offset + 1]);
            EXPECT_FLOAT_EQ(actual.b(), pixels[offset + 2]);
            EXPECT_FLOAT_EQ(actual.a(), pixels[offset + 3]);
        }
    }
}

TEST(BendTransformContractTest, DirectionAndPeriodProduceExpectedIntegerSample)
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            const float value = static_cast<float>(y * kWidth + x);
            pixels[offset] = value;
            pixels[offset + 1] = value + 20.0f;
            pixels[offset + 2] = value + 40.0f;
            pixels[offset + 3] = 0.25f + value * 0.01f;
        }
    }
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::BendTransform effect;
    effect.setAngle(1.0f);
    effect.setDirection(0.0f);
    effect.setSize(4.0f);

    effect.applyCPUOnly(source, output);

    const auto actual = output.image().getPixel(1, 0);
    const std::size_t expectedOffset = (1 * kWidth + 1) * 4;
    EXPECT_FLOAT_EQ(actual.r(), pixels[expectedOffset]);
    EXPECT_FLOAT_EQ(actual.g(), pixels[expectedOffset + 1]);
    EXPECT_FLOAT_EQ(actual.b(), pixels[expectedOffset + 2]);
    EXPECT_FLOAT_EQ(actual.a(), pixels[expectedOffset + 3]);
}
