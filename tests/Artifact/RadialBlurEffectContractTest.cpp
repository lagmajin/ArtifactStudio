#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>
#include <QString>
#include <QVariant>

import Artifact.Effect.Rasterizer.RadialBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

bool differsFrom(const float* lhs, const float* rhs, std::size_t count)
{
    for (std::size_t index = 0; index < count; ++index) {
        if (std::abs(lhs[index] - rhs[index]) > 1.0e-6f) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(RadialBlurEffectContractTest, RadialBlurPullsSamplesOutwardAndClampsAtEdges)
{
    constexpr int width = 9;
    constexpr int height = 9;
    constexpr std::size_t channelCount =
        static_cast<std::size_t>(width * height * 4);
    std::array<float, channelCount> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x) / 8.0f;
            pixels[offset + 1] = 0.3f;
            pixels[offset + 2] = 0.5f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    RadialBlurEffect effect;
    effect.setAmount(1.0f);
    effect.setQuality(8);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);

    effect.applyCPUOnly(source, output);

    const auto original = source.image().getPixel(2, 4);
    const auto blurred = output.image().getPixel(2, 4);
    EXPECT_LT(blurred.r(), original.r());
    EXPECT_FLOAT_EQ(output.image().getPixel(0, 4).r(), 0.0f);
    EXPECT_FLOAT_EQ(blurred.a(), 1.0f);
    EXPECT_FLOAT_EQ(source.image().getPixel(2, 4).r(), original.r());
}

TEST(RadialBlurEffectContractTest, ZeroAmountPreservesEveryRgbaChannel)
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
    RadialBlurEffect effect;
    effect.setAmount(0.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(actual[channel], pixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}

TEST(RadialBlurEffectContractTest, SettersClampRangesAndResetNonFiniteValues)
{
    RadialBlurEffect effect;
    effect.setAmount(3.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 2.0f);
    effect.setAmount(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.amount(), 0.5f);

    effect.setQuality(1);
    EXPECT_EQ(effect.quality(), 2);
    effect.setQuality(100);
    EXPECT_EQ(effect.quality(), 32);

    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterY(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);

    effect.setMode(2.0f);
    EXPECT_FLOAT_EQ(effect.mode(), 1.0f);
    effect.setMode(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.mode(), 0.0f);
}
