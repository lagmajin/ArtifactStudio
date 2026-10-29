#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.VectorBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 6;

ImageF32x4RGBAWithCache makePatternSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / static_cast<float>(kWidth + 1);
            pixels[offset + 1] = static_cast<float>(y + 1) / static_cast<float>(kHeight + 1);
            pixels[offset + 2] = static_cast<float>(x + y + 1) /
                                 static_cast<float>(kWidth + kHeight + 1);
            pixels[offset + 3] = 0.55f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(VectorBlurEffectContractTest, ZeroShutterAnglePreservesAllChannels)
{
    const auto source = makePatternSurface();
    VectorBlurEffect effect;
    effect.setShutterAngle(0.0f);
    effect.setExposureCompensation(2.0f);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto expected = source.image().getPixel(x, y);
            EXPECT_NEAR(actual.r(), expected.r(), 1.0e-6f);
            EXPECT_NEAR(actual.g(), expected.g(), 1.0e-6f);
            EXPECT_NEAR(actual.b(), expected.b(), 1.0e-6f);
            EXPECT_NEAR(actual.a(), expected.a(), 1.0e-6f);
        }
    }
}

TEST(VectorBlurEffectContractTest, MissingPreviousFrameSamplerPreservesInput)
{
    const auto source = makePatternSurface();
    VectorBlurEffect effect;
    effect.setShutterAngle(180.0f);
    effect.setExposureCompensation(1.0f);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto expected = source.image().getPixel(x, y);
            EXPECT_NEAR(actual.r(), expected.r(), 1.0e-6f);
            EXPECT_NEAR(actual.g(), expected.g(), 1.0e-6f);
            EXPECT_NEAR(actual.b(), expected.b(), 1.0e-6f);
            EXPECT_NEAR(actual.a(), expected.a(), 1.0e-6f);
        }
    }
}

TEST(VectorBlurEffectContractTest, SettingsClampAndResetNonFiniteValues)
{
    VectorBlurEffect effect;
    effect.setShutterAngle(900.0f);
    effect.setSamples(40);
    effect.setExposureCompensation(-1.0f);
    EXPECT_FLOAT_EQ(effect.shutterAngle(), 720.0f);
    EXPECT_EQ(effect.samples(), 32);
    EXPECT_FLOAT_EQ(effect.exposureCompensation(), 0.0f);

    effect.setShutterAngle(-1.0f);
    effect.setSamples(-4);
    effect.setExposureCompensation(5.0f);
    EXPECT_FLOAT_EQ(effect.shutterAngle(), 0.0f);
    EXPECT_EQ(effect.samples(), 2);
    EXPECT_FLOAT_EQ(effect.exposureCompensation(), 4.0f);

    effect.setShutterAngle(std::numeric_limits<float>::quiet_NaN());
    effect.setExposureCompensation(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.shutterAngle(), 180.0f);
    EXPECT_FLOAT_EQ(effect.exposureCompensation(), 1.0f);
}
\n
