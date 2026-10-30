#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Artifact.Effect.Rasterizer.Glitch;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 16;
constexpr int kHeight = 12;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeColorPattern()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            pixels[offset] = 0.05f * static_cast<float>(x + 1) +
                             0.001f * static_cast<float>(y);
            pixels[offset + 1] = 0.2f + 0.01f * static_cast<float>(y);
            pixels[offset + 2] = 0.9f - 0.03f * static_cast<float>(x) -
                                 0.001f * static_cast<float>(y);
            pixels[offset + 3] = 0.1f + 0.02f * static_cast<float>(x);
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(GlitchEffectContractTest, SeededChannelShiftIsRepeatableAndPreservesAlpha)
{
    const auto source = makeColorPattern();
    ArtifactCore::ImageF32x4RGBAWithCache first;
    ArtifactCore::ImageF32x4RGBAWithCache second;
    Artifact::GlitchEffect effect;
    effect.setIntensity(0.0f);
    effect.setScanlines(0.0f);
    effect.setColorShift(1.0f);
    effect.setSeed(73);

    effect.applyCPUOnly(source, first);
    effect.applyCPUOnly(source, second);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto input = source.image().getPixel(x, y);
            const auto a = first.image().getPixel(x, y);
            const auto b = second.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(a.r(), b.r());
            EXPECT_FLOAT_EQ(a.g(), b.g());
            EXPECT_FLOAT_EQ(a.b(), b.b());
            EXPECT_FLOAT_EQ(a.a(), input.a());

            if (x == 0 || x == kWidth - 1) {
                EXPECT_FLOAT_EQ(a.r(), input.r());
                EXPECT_FLOAT_EQ(a.b(), input.b());
            } else {
                const bool unchanged = a.r() == input.r() && a.b() == input.b();
                const bool swapped = a.r() == input.b() && a.b() == input.r();
                EXPECT_TRUE(unchanged || swapped);
            }
            EXPECT_FLOAT_EQ(a.g(), input.g());
        }
    }
}

TEST(GlitchEffectContractTest,
     SeededGlitchInPlaceMatchesOutOfPlaceWithoutChangingSource)
{
    const auto source = makeColorPattern();
    auto inPlace = source;
    const float* sourceBefore = source.image().rgba32fData();
    ASSERT_NE(sourceBefore, nullptr);
    std::array<float, kWidth * kHeight * 4> originalSource{};
    std::copy_n(sourceBefore, originalSource.size(), originalSource.begin());
    ArtifactCore::ImageF32x4RGBAWithCache outOfPlace;
    Artifact::GlitchEffect effect;
    effect.setIntensity(0.7f);
    effect.setColorShift(0.8f);
    effect.setScanlines(0.6f);
    effect.setSeed(19);

    effect.applyCPUOnly(source, outOfPlace);
    effect.applyCPUOnly(inPlace, inPlace);

    const float* sourcePixels = source.image().rgba32fData();
    const float* outOfPlacePixels = outOfPlace.image().rgba32fData();
    const float* inPlacePixels = inPlace.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(outOfPlacePixels, nullptr);
    ASSERT_NE(inPlacePixels, nullptr);
    EXPECT_NE(sourcePixels, inPlacePixels);
    for (std::size_t channel = 0; channel < kWidth * kHeight * 4; ++channel) {
        EXPECT_FLOAT_EQ(sourcePixels[channel], originalSource[channel])
            << "source channel=" << channel;
        EXPECT_FLOAT_EQ(inPlacePixels[channel], outOfPlacePixels[channel])
            << "output channel=" << channel;
    }
}

TEST(GlitchEffectContractTest,
     CreativeGlitchOffsetsRedAndBlueFromTheGreenSourcePixel)
{
    constexpr int width = 64;
    constexpr int height = 6;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4;
            pixels[offset] = static_cast<float>(x + 1) / 128.0f;
            pixels[offset + 1] = static_cast<float>(x + 1) / 80.0f;
            pixels[offset + 2] = static_cast<float>(x + 1) / 96.0f;
            pixels[offset + 3] = 0.2f + static_cast<float>(x) / 100.0f;
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::ArtifactGlitchEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);

    effect.apply(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (int y = 0; y < height; ++y) {
        const std::size_t centerOffset =
            (static_cast<std::size_t>(y) * width + 32) * 4;
        int centerSourceX = -1;
        for (int candidate = 0; candidate < width; ++candidate) {
            if (actual[centerOffset + 1] ==
                pixels[(static_cast<std::size_t>(y) * width + candidate) * 4 + 1]) {
                centerSourceX = candidate;
                break;
            }
        }
        ASSERT_GE(centerSourceX, 0) << "green channel must identify the shifted source";
        int channelSeparation = 0;
        for (int candidate = 3; candidate <= 7; ++candidate) {
            const std::size_t redOffset =
                (static_cast<std::size_t>(y) * width + centerSourceX + candidate) * 4;
            const std::size_t blueOffset =
                (static_cast<std::size_t>(y) * width + centerSourceX - candidate) * 4;
            if (actual[centerOffset] == pixels[redOffset] &&
                actual[centerOffset + 2] == pixels[blueOffset + 2]) {
                channelSeparation = candidate;
                break;
            }
        }
        ASSERT_NE(channelSeparation, 0)
            << "red/blue channels must use the configured 3-7 pixel split";

        for (int x = 0; x < width; ++x) {
            const std::size_t outputOffset =
                (static_cast<std::size_t>(y) * width + x) * 4;
            int sourceX = -1;
            for (int candidate = 0; candidate < width; ++candidate) {
                if (actual[outputOffset + 1] ==
                    pixels[(static_cast<std::size_t>(y) * width + candidate) * 4 + 1]) {
                    sourceX = candidate;
                    break;
                }
            }
            ASSERT_GE(sourceX, 0) << "x=" << x << " y=" << y;
            const int redX = std::clamp(sourceX + channelSeparation, 0, width - 1);
            const int blueX = std::clamp(sourceX - channelSeparation, 0, width - 1);
            EXPECT_FLOAT_EQ(actual[outputOffset],
                            pixels[(static_cast<std::size_t>(y) * width + redX) * 4])
                << "red source x=" << x << " y=" << y;
            EXPECT_FLOAT_EQ(actual[outputOffset + 2],
                            pixels[(static_cast<std::size_t>(y) * width + blueX) * 4 + 2])
                << "blue source x=" << x << " y=" << y;
            EXPECT_FLOAT_EQ(actual[outputOffset + 3],
                            pixels[(static_cast<std::size_t>(y) * width + sourceX) * 4 + 3])
                << "alpha source x=" << x << " y=" << y;
        }
    }
}

TEST(GlitchEffectContractTest, SettersClampAmountsAndSeed)
{
    Artifact::GlitchEffect effect;
    effect.setIntensity(-1.0f);
    EXPECT_FLOAT_EQ(effect.intensity(), 0.0f);
    effect.setIntensity(2.0f);
    EXPECT_FLOAT_EQ(effect.intensity(), 1.0f);
    effect.setIntensity(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.intensity(), 0.3f);

    effect.setColorShift(-1.0f);
    EXPECT_FLOAT_EQ(effect.colorShift(), 0.0f);
    effect.setColorShift(2.0f);
    EXPECT_FLOAT_EQ(effect.colorShift(), 1.0f);
    effect.setColorShift(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.colorShift(), 0.5f);

    effect.setScanlines(-1.0f);
    EXPECT_FLOAT_EQ(effect.scanlines(), 0.0f);
    effect.setScanlines(2.0f);
    EXPECT_FLOAT_EQ(effect.scanlines(), 1.0f);
    effect.setScanlines(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.scanlines(), 0.5f);

    effect.setSeed(-1);
    EXPECT_EQ(effect.seed(), 0);
    effect.setSeed(10000);
    EXPECT_EQ(effect.seed(), 9999);
}
