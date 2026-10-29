#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Rasterizer.Stripes;
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

TEST(StripesEffectContractTest, StripesGenerateBinaryPatternFromFrequencyAndThickness)
{
    constexpr int width = 8;
    constexpr std::array<float, width * 4> pixels = {
        0.2f, 0.3f, 0.4f, 0.6f, 0.2f, 0.3f, 0.4f, 0.6f,
        0.2f, 0.3f, 0.4f, 0.6f, 0.2f, 0.3f, 0.4f, 0.6f,
        0.2f, 0.3f, 0.4f, 0.6f, 0.2f, 0.3f, 0.4f, 0.6f,
        0.2f, 0.3f, 0.4f, 0.6f, 0.2f, 0.3f, 0.4f, 0.6f,
    };
    const auto source = makeSurface(pixels.data(), width, 1);
    ImageF32x4RGBAWithCache output;
    StripesEffect effect;
    effect.setFrequency(1.0f);
    effect.setThickness(0.5f);
    effect.setAngle(0.0f);
    effect.setOffset(0.0f);

    effect.applyCPUOnly(source, output);

    for (int x = 0; x < width; ++x) {
        const float expected = x < 4 ? 1.0f : 0.0f;
        const auto pixel = output.image().getPixel(x, 0);
        EXPECT_NEAR(pixel.r(), expected, 1.0e-6f) << "x=" << x;
        EXPECT_NEAR(pixel.g(), expected, 1.0e-6f) << "x=" << x;
        EXPECT_NEAR(pixel.b(), expected, 1.0e-6f) << "x=" << x;
        EXPECT_FLOAT_EQ(pixel.a(), 1.0f) << "x=" << x;
    }
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.2f, 1.0e-6f);
}


TEST(StripesEffectContractTest, ThicknessEndpointsProduceSolidBinaryMasks)
{
    constexpr int width = 4;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int index = 0; index < width * height; ++index) {
        pixels[index * 4] = 0.2f;
        pixels[index * 4 + 1] = 0.4f;
        pixels[index * 4 + 2] = 0.6f;
        pixels[index * 4 + 3] = 0.3f + static_cast<float>(index % 5) * 0.1f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    StripesEffect effect;
    effect.setFrequency(2.0f);
    effect.setAngle(37.0f);

    for (const float thickness : std::array<float, 2>{0.0f, 1.0f}) {
        effect.setThickness(thickness);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        const float expected = thickness;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const auto pixel = output.image().getPixel(x, y);
                EXPECT_FLOAT_EQ(pixel.r(), expected);
                EXPECT_FLOAT_EQ(pixel.g(), expected);
                EXPECT_FLOAT_EQ(pixel.b(), expected);
                EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
            }
        }
    }
}

TEST(StripesEffectContractTest, IntegerOffsetRepeatsTheStripePattern)
{
    constexpr int width = 7;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 0.2f;
        pixels[pixel * 4 + 1] = 0.4f;
        pixels[pixel * 4 + 2] = 0.6f;
        pixels[pixel * 4 + 3] = 0.35f + static_cast<float>(pixel % 4) * 0.1f;
    }

    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache baseOutput;
    ImageF32x4RGBAWithCache shiftedOutput;
    StripesEffect effect;
    effect.setFrequency(3.0f);
    effect.setThickness(0.37f);
    effect.setAngle(23.0f);
    effect.setOffset(0.0f);
    effect.applyCPUOnly(source, baseOutput);

    effect.setOffset(1.0f);
    effect.applyCPUOnly(source, shiftedOutput);

    const float* base = baseOutput.image().rgba32fData();
    const float* shifted = shiftedOutput.image().rgba32fData();
    ASSERT_NE(base, nullptr);
    ASSERT_NE(shifted, nullptr);
    for (std::size_t component = 0; component < pixels.size(); ++component) {
        EXPECT_FLOAT_EQ(base[component], shifted[component])
            << "component=" << component;
    }
}

TEST(StripesEffectContractTest, SettersClampFrequencyAndThicknessAndResetNonFiniteValues)
{
    StripesEffect effect;
    effect.setFrequency(0.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), 0.5f);
    effect.setFrequency(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.frequency(), 10.0f);

    effect.setThickness(-1.0f);
    EXPECT_FLOAT_EQ(effect.thickness(), 0.0f);
    effect.setThickness(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.thickness(), 0.5f);

    effect.setAngle(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.angle(), 0.0f);
    effect.setOffset(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.offset(), 0.0f);
}
