#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Spherize;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(SpherizeEffectContractTest, SpherizePreservesCenterAndPixelsOutsideRadius)
{
    constexpr int width = 33;
    constexpr int height = 33;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = static_cast<float>(x) / static_cast<float>(width - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[offset + 2] = 0.25f;
            pixels[offset + 3] = 0.6f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    SpherizeEffect effect;
    effect.setAmount(100.0f);
    effect.setRadius(0.5f);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(width / 2, height / 2);
    EXPECT_NEAR(center.r(), source.image().getPixel(width / 2, height / 2).r(), 1.0e-6f);
    EXPECT_NEAR(center.g(), source.image().getPixel(width / 2, height / 2).g(), 1.0e-6f);
    const auto outsideRadius = output.image().getPixel(0, 0);
    const auto sourceOutsideRadius = source.image().getPixel(0, 0);
    EXPECT_NEAR(outsideRadius.r(), sourceOutsideRadius.r(), 1.0e-6f);
    EXPECT_NEAR(outsideRadius.g(), sourceOutsideRadius.g(), 1.0e-6f);
    EXPECT_NEAR(outsideRadius.b(), sourceOutsideRadius.b(), 1.0e-6f);
    EXPECT_NEAR(outsideRadius.a(), sourceOutsideRadius.a(), 1.0e-6f);
    const auto distorted = output.image().getPixel(22, height / 2);
    const auto undistorted = source.image().getPixel(22, height / 2);
    EXPECT_GT(std::abs(distorted.r() - undistorted.r()), 0.05f);
    EXPECT_NEAR(distorted.a(), undistorted.a(), 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(width / 2, height / 2).a(), 0.6f, 1.0e-6f);
}

TEST(SpherizeEffectContractTest, ZeroRadiusPreservesEveryRgbaChannel)
{
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr std::array<float, width * height * 4> pixels = {
        0.1f, 0.2f, 0.3f, 0.0f,
        0.4f, 0.5f, 0.6f, 0.25f,
        0.7f, 0.8f, 0.9f, 0.5f,
        0.2f, 0.4f, 0.6f, 0.75f,
        0.3f, 0.6f, 0.9f, 1.0f,
        0.9f, 0.5f, 0.1f, 0.4f,
    };
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    SpherizeEffect effect;
    effect.setAmount(100.0f);
    effect.setRadius(0.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(SpherizeEffectContractTest, AmountRadiusAndCenterClampAndResetNonFiniteValues)
{
    SpherizeEffect effect;
    effect.setAmount(120.0f);
    effect.setRadius(-0.5f);
    effect.setCenterX(-1.0f);
    effect.setCenterY(2.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 100.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);

    effect.setAmount(std::numeric_limits<float>::quiet_NaN());
    effect.setRadius(std::numeric_limits<float>::infinity());
    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    effect.setCenterY(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.amount(), 50.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.5f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);
}
