#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>

import Artifact.Effect.Rasterizer.Ripple;
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

TEST(RippleEffectContractTest, RipplePreservesCenterAndOpaqueAlpha)
{
    constexpr int width = 16;
    constexpr int height = 16;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = static_cast<float>(x) / static_cast<float>(width - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[offset + 2] = 0.3f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    RippleEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setAmplitude(4.0f);
    effect.setFrequency(0.25f);
    effect.setDecay(0.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(8, 8).r(),
                source.image().getPixel(8, 8).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(8, 8).g(),
                source.image().getPixel(8, 8).g(), 1.0e-6f);
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    bool changedRgb = false;
    for (int pixel = 0; pixel < width * height; ++pixel) {
        changedRgb = changedRgb ||
            std::abs(outputPixels[pixel * 4] - pixels[pixel * 4]) > 1.0e-5f ||
            std::abs(outputPixels[pixel * 4 + 1] - pixels[pixel * 4 + 1]) > 1.0e-5f;
        EXPECT_NEAR(outputPixels[pixel * 4 + 3], 1.0f, 1.0e-6f)
            << "alpha pixel=" << pixel;
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.0f, 1.0e-6f);
}

TEST(RippleEffectContractTest, BilinearDisplacementSamplesAlphaWithColorChannels)
{
    constexpr int width = 9;
    constexpr int height = 7;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int offset = (y * width + x) * 4;
            const float horizontal = static_cast<float>(x) / static_cast<float>(width - 1);
            pixels[offset] = horizontal;
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[offset + 2] = 0.25f;
            pixels[offset + 3] = horizontal;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    RippleEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setAmplitude(2.5f);
    effect.setFrequency(0.3f);
    effect.setDecay(0.0f);

    effect.applyCPUOnly(source, output);

    bool changedAlpha = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto result = output.image().getPixel(x, y);
            EXPECT_NEAR(result.a(), result.r(), 1.0e-6f)
                << "x=" << x << " y=" << y;
            changedAlpha = changedAlpha ||
                std::abs(result.a() - source.image().getPixel(x, y).a()) > 1.0e-5f;
        }
    }
    EXPECT_TRUE(changedAlpha);
    EXPECT_NEAR(source.image().getPixel(width - 1, height - 1).a(), 1.0f, 1.0e-6f);
}

TEST(RippleEffectContractTest, SettersClampSpatialParametersAndResetNonFiniteValues)
{
    RippleEffect effect;
    effect.setCenterX(2.0f);
    effect.setCenterY(-1.0f);
    effect.setAmplitude(5000.0f);
    effect.setFrequency(-2.0f);
    effect.setDecay(2.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 1.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 0.0f);
    EXPECT_FLOAT_EQ(effect.amplitude(), 4096.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), -1.0f);
    EXPECT_FLOAT_EQ(effect.decay(), 1.0f);

    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    effect.setCenterY(std::numeric_limits<float>::infinity());
    effect.setAmplitude(std::numeric_limits<float>::infinity());
    effect.setFrequency(std::numeric_limits<float>::quiet_NaN());
    effect.setDecay(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);
    EXPECT_FLOAT_EQ(effect.amplitude(), 10.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), 0.01f);
    EXPECT_FLOAT_EQ(effect.decay(), 0.005f);
}
\n
