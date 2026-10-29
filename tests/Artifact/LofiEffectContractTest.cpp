#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cmath>

#include <QRectF>

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

TEST(LofiEffectContractTest, OldTvClampsRgbAndPreservesOutOfRangeAlpha)
{
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr std::array<float, width * height * 4> pixels{
        -0.5f, 1.5f, 0.25f, -0.2f,
        2.0f, -1.0f, 0.5f, 1.3f,
        -3.0f, 4.0f, 0.0f, 0.4f,
        1.2f, -0.3f, 3.0f, 0.8f,
        0.1f, 0.2f, 0.3f, -1.0f,
        0.8f, 0.7f, 0.6f, 2.0f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;

    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.apply(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_GE(result[pixel * 4 + channel], 0.0f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_LE(result[pixel * 4 + channel], 1.0f)
                << "pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "pixel=" << pixel;
    }
}

TEST(LofiEffectContractTest, OldTvZeroMixIsAnExactSourcePassthrough)
{
    constexpr int width = 5;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / 8.0f;
            pixels[offset + 1] = static_cast<float>(y + 2) / 7.0f;
            pixels[offset + 2] = static_cast<float>(x + y + 1) / 9.0f;
            pixels[offset + 3] = static_cast<float>(x + 2 * y + 1) / 12.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setMix(0.0f);
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(LofiEffectContractTest, OldTvHalfMixInterpolatesSourceAndEffectPixels)
{
    constexpr int width = 7;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / 10.0f;
            pixels[offset + 1] = static_cast<float>(y + 1) / 8.0f;
            pixels[offset + 2] = static_cast<float>(x + 2 * y + 1) / 16.0f;
            pixels[offset + 3] = static_cast<float>(x + y + 1) / 16.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache halfMix;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, fullEffect);
    effect.setMix(0.5f);
    effect.applyCPUOnly(source, halfMix);

    ASSERT_EQ(fullEffect.image().width(), width);
    ASSERT_EQ(fullEffect.image().height(), height);
    ASSERT_EQ(halfMix.image().width(), width);
    ASSERT_EQ(halfMix.image().height(), height);
    const float* fullEffectPixels = fullEffect.image().rgba32fData();
    const float* halfMixPixels = halfMix.image().rgba32fData();
    ASSERT_NE(fullEffectPixels, nullptr);
    ASSERT_NE(halfMixPixels, nullptr);
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            const float expected =
                pixels[offset] * 0.5f + fullEffectPixels[offset] * 0.5f;
            EXPECT_NEAR(halfMixPixels[offset], expected, 1.0e-6f)
                << "pixel=" << pixel << " channel=" << channel;
        }
    }
}

TEST(LofiEffectContractTest, OldTvEffectRegionCompositesOnlyInsidePixelBounds)
{
    constexpr int width = 6;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / 8.0f;
            pixels[offset + 1] = static_cast<float>(y + 1) / 6.0f;
            pixels[offset + 2] = static_cast<float>(x + y + 1) / 12.0f;
            pixels[offset + 3] = static_cast<float>(x + 2 * y + 1) / 16.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache regionalEffect;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, fullEffect);
    effect.setEffectRegion(QRectF(2.0, 1.0, 2.0, 2.0));
    effect.applyCPUOnly(source, regionalEffect);

    ASSERT_EQ(fullEffect.image().width(), width);
    ASSERT_EQ(fullEffect.image().height(), height);
    ASSERT_EQ(regionalEffect.image().width(), width);
    ASSERT_EQ(regionalEffect.image().height(), height);
    const float* fullEffectPixels = fullEffect.image().rgba32fData();
    const float* regionalPixels = regionalEffect.image().rgba32fData();
    ASSERT_NE(fullEffectPixels, nullptr);
    ASSERT_NE(regionalPixels, nullptr);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool inside = x >= 2 && x < 4 && y >= 1 && y < 3;
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const float expected = inside
                    ? fullEffectPixels[offset + channel]
                    : pixels[offset + channel];
                EXPECT_NEAR(regionalPixels[offset + channel], expected, 1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
            }
        }
    }
}

TEST(LofiEffectContractTest, OldTvEffectRegionClipsToImageBounds)
{
    constexpr int width = 5;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / 7.0f;
            pixels[offset + 1] = static_cast<float>(y + 1) / 6.0f;
            pixels[offset + 2] = static_cast<float>(x + y + 1) / 10.0f;
            pixels[offset + 3] = 0.65f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache clippedEffect;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, fullEffect);
    effect.setEffectRegion(QRectF(-2.0, -1.0, 4.0, 3.0));
    effect.applyCPUOnly(source, clippedEffect);

    const float* fullPixels = fullEffect.image().rgba32fData();
    const float* clippedPixels = clippedEffect.image().rgba32fData();
    ASSERT_NE(fullPixels, nullptr);
    ASSERT_NE(clippedPixels, nullptr);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool insideClippedRegion = x < 2 && y < 2;
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const float expected = insideClippedRegion
                    ? fullPixels[offset + channel]
                    : pixels[offset + channel];
                EXPECT_NEAR(clippedPixels[offset + channel], expected, 1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
            }
        }
    }
}

TEST(LofiEffectContractTest, OldTvEffectRegionUsesPixelCentersForFractionalBounds)
{
    constexpr int width = 8;
    constexpr int height = 8;
    const auto source = makePatternSurface();
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache fractionalEffect;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, fullEffect);
    effect.setEffectRegion(QRectF(2.25, 1.25, 1.0, 1.5));
    effect.applyCPUOnly(source, fractionalEffect);

    const float* original = source.image().rgba32fData();
    const float* fullPixels = fullEffect.image().rgba32fData();
    const float* fractionalPixels = fractionalEffect.image().rgba32fData();
    ASSERT_NE(original, nullptr);
    ASSERT_NE(fullPixels, nullptr);
    ASSERT_NE(fractionalPixels, nullptr);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Pixel centers are (x + 0.5, y + 0.5). The fractional region
            // contains only x=2 and y=1,2.
            const bool inside = x == 2 && (y == 1 || y == 2);
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const float expected = inside
                    ? fullPixels[offset + channel]
                    : original[offset + channel];
                EXPECT_NEAR(fractionalPixels[offset + channel], expected, 1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
            }
        }
    }
}
