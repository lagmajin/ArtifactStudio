#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cmath>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makePatternSurface()
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = 0.4f;
            pixels[offset + 1] = 0.5f;
            pixels[offset + 2] = 0.6f;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    return ImageF32x4RGBAWithCache(image);
}

TEST(LofiEffectContractTest, OldTvPatternedInputProducesFiniteChangedOutput)
{
    constexpr int width = 48;
    constexpr int height = 32;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            const float value = (x / 2 + y / 3) % 2 == 0 ? 0.2f : 0.8f;
            pixels[offset] = value;
            pixels[offset + 1] = 1.0f - value;
            pixels[offset + 2] = (x % 5) / 4.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.apply(source, output);

    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    bool changed = false;
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_TRUE(std::isfinite(outputPixels[channel]))
            << "channel=" << channel;
        changed = changed ||
                  std::abs(outputPixels[channel] - sourcePixels[channel]) > 1.0e-5f;
    }
    EXPECT_TRUE(changed);
}

} // namespace

TEST(LofiEffectContractTest, OldTvKeepsAlphaAndSourceAndAppliesFourRowScanline)
{
    constexpr int width = 8;
    constexpr int height = 8;
    constexpr std::size_t channelCount = width * height * 4;
    const auto source = makePatternSurface();
    std::array<float, channelCount> original{};
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    std::copy_n(sourcePixels, channelCount, original.begin());

    ImageF32x4RGBAWithCache first;
    ImageF32x4RGBAWithCache repeated;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.apply(source, first);

    ArtifactOldTVEffect repeatedEffect;
    repeatedEffect.setComputeMode(ComputeMode::CPU);
    repeatedEffect.apply(source, repeated);

    ASSERT_EQ(first.image().width(), width);
    ASSERT_EQ(first.image().height(), height);
    const float* output = first.image().rgba32fData();
    const float* repeatedOutput = repeated.image().rgba32fData();
    const float* unchangedSource = source.image().rgba32fData();
    ASSERT_NE(output, nullptr);
    ASSERT_NE(repeatedOutput, nullptr);
    ASSERT_NE(unchangedSource, nullptr);
    constexpr std::array<float, 3> baseBgra = {0.6f, 0.5f, 0.4f};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            const float scanline = y % 4 == 0 ? 0.7f : 1.0f;
            for (int channel = 0; channel < 3; ++channel) {
                EXPECT_NEAR(output[offset + channel], baseBgra[channel] * scanline, 0.006f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
                EXPECT_FLOAT_EQ(output[offset + channel], repeatedOutput[offset + channel]);
                EXPECT_FLOAT_EQ(unchangedSource[offset + channel], original[offset + channel]);
            }
            EXPECT_FLOAT_EQ(output[offset + 3], original[offset + 3]);
            EXPECT_FLOAT_EQ(repeatedOutput[offset + 3], original[offset + 3]);
            EXPECT_FLOAT_EQ(unchangedSource[offset + 3], original[offset + 3]);
        }
    }
}

TEST(LofiEffectContractTest, OldTvSupportsInPlaceProcessingWithoutChangingAlpha)
{
    constexpr int width = 9;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / width;
            pixels[offset + 1] = static_cast<float>(y + 1) / height;
            pixels[offset + 2] = 0.25f;
            pixels[offset + 3] = static_cast<float>(x + y + 1) /
                                 static_cast<float>(width + height);
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    ImageF32x4RGBAWithCache inPlace(image);
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.apply(inPlace, inPlace);

    ASSERT_EQ(inPlace.image().width(), width);
    ASSERT_EQ(inPlace.image().height(), height);
    const float* output = inPlace.image().rgba32fData();
    ASSERT_NE(output, nullptr);
    bool colorChanged = false;
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width * height);
         ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(output[pixel * 4 + channel]));
            colorChanged = colorChanged ||
                std::abs(output[pixel * 4 + channel] - pixels[pixel * 4 + channel]) >
                    1.0e-5f;
        }
        EXPECT_FLOAT_EQ(output[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "pixel=" << pixel;
    }
    EXPECT_TRUE(colorChanged);
}
