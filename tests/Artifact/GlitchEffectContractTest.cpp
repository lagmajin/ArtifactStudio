#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.Glitch;
import Graphics.SurfaceColorContract;
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

    std::size_t shiftedPixels = 0;
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
                if (swapped && !unchanged) ++shiftedPixels;
            }
            EXPECT_FLOAT_EQ(a.g(), input.g());
        }
    }
    EXPECT_GT(shiftedPixels, 0u);
}

TEST(GlitchEffectContractTest,
     ZeroAmountsAreAnExactDeepCopy)
{
    const auto source = makeColorPattern();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::GlitchEffect effect;
    effect.setIntensity(0.0f);
    effect.setScanlines(0.0f);
    effect.setColorShift(0.0f);

    effect.applyCPUOnly(source, output);

    const float* sourcePixels = source.image().rgba32fData();
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(outputPixels, nullptr);
    EXPECT_NE(sourcePixels, outputPixels);
    for (std::size_t channel = 0; channel < kWidth * kHeight * 4; ++channel) {
        EXPECT_FLOAT_EQ(outputPixels[channel], sourcePixels[channel])
            << "channel=" << channel;
    }
}

TEST(GlitchEffectContractTest,
     SeededScanlinesRepeatWithinAFrameAndChangeAcrossFrames)
{
    constexpr int width = 64;
    constexpr int height = 48;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4;
            pixels[offset] = static_cast<float>(x + 1) / (width + 1);
            pixels[offset + 1] = static_cast<float>(y + 1) / (height + 1);
            pixels[offset + 2] = static_cast<float>((x * 3 + y * 5) % 31) / 30.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 2) % 17) / 16.0f;
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    Artifact::GlitchEffect effect;
    effect.setIntensity(0.0f);
    effect.setScanlines(1.0f);
    effect.setColorShift(0.0f);

    Artifact::EffectContext context;
    context.compositionFrame = 5;
    effect.setContext(context);
    ArtifactCore::ImageF32x4RGBAWithCache firstFrame;
    ArtifactCore::ImageF32x4RGBAWithCache repeatedFrame;
    effect.applyCPUOnly(source, firstFrame);
    effect.applyCPUOnly(source, repeatedFrame);
    context.compositionFrame = 6;
    effect.setContext(context);
    ArtifactCore::ImageF32x4RGBAWithCache nextFrame;
    effect.applyCPUOnly(source, nextFrame);

    const float* first = firstFrame.image().rgba32fData();
    const float* repeated = repeatedFrame.image().rgba32fData();
    const float* next = nextFrame.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(repeated, nullptr);
    ASSERT_NE(next, nullptr);
    std::size_t changedFromPreviousFrame = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(first[offset + channel], repeated[offset + channel])
                << "same-frame pixel=" << pixel << " channel=" << channel;
            if (first[offset + channel] != next[offset + channel]) {
                ++changedFromPreviousFrame;
            }
        }
        EXPECT_FLOAT_EQ(first[offset + 3], pixels[offset + 3])
            << "pixel=" << pixel;
        EXPECT_FLOAT_EQ(next[offset + 3], pixels[offset + 3])
            << "next-frame pixel=" << pixel;
    }
    EXPECT_GT(changedFromPreviousFrame, 0u);
}

TEST(GlitchEffectContractTest,
     ColorShiftAmountMonotonicallyAddsRedBlueSwaps)
{
    constexpr int width = 64;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4;
            pixels[offset] = 0.1f + static_cast<float>(x) / 1000.0f;
            pixels[offset + 1] = static_cast<float>(y + 1) / 10.0f;
            pixels[offset + 2] = 0.8f - static_cast<float>(x) / 1000.0f;
            pixels[offset + 3] = static_cast<float>(x + y + 1) / 80.0f;
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    Artifact::GlitchEffect effect;
    effect.setIntensity(0.0f);
    effect.setScanlines(0.0f);
    effect.setSeed(37);

    ArtifactCore::ImageF32x4RGBAWithCache halfShift;
    ArtifactCore::ImageF32x4RGBAWithCache fullShift;
    effect.setColorShift(0.5f);
    effect.applyCPUOnly(source, halfShift);
    effect.setColorShift(1.0f);
    effect.applyCPUOnly(source, fullShift);

    const float* half = halfShift.image().rgba32fData();
    const float* full = fullShift.image().rgba32fData();
    ASSERT_NE(half, nullptr);
    ASSERT_NE(full, nullptr);
    std::size_t halfSwapCount = 0;
    std::size_t fullSwapCount = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        const bool halfSwapped = half[offset] == pixels[offset + 2] &&
            half[offset + 2] == pixels[offset];
        const bool fullSwapped = full[offset] == pixels[offset + 2] &&
            full[offset + 2] == pixels[offset];
        halfSwapCount += halfSwapped ? 1u : 0u;
        fullSwapCount += fullSwapped ? 1u : 0u;
        if (halfSwapped) {
            EXPECT_TRUE(fullSwapped) << "pixel=" << pixel;
        }
        EXPECT_FLOAT_EQ(half[offset + 1], pixels[offset + 1])
            << "half-shift green pixel=" << pixel;
        EXPECT_FLOAT_EQ(full[offset + 1], pixels[offset + 1])
            << "full-shift green pixel=" << pixel;
        EXPECT_FLOAT_EQ(half[offset + 3], pixels[offset + 3])
            << "half-shift alpha pixel=" << pixel;
        EXPECT_FLOAT_EQ(full[offset + 3], pixels[offset + 3])
            << "full-shift alpha pixel=" << pixel;
    }
    EXPECT_GT(halfSwapCount, 0u);
    EXPECT_GT(fullSwapCount, halfSwapCount);
}

TEST(GlitchEffectContractTest,
     IntensityControlsBlockDisplacementWithoutChangingAlpha)
{
    constexpr int width = 64;
    constexpr int height = 64;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4;
            pixels[offset] = static_cast<float>((x * 7 + y * 3) % 251) / 250.0f;
            pixels[offset + 1] = static_cast<float>((x * 5 + y * 11) % 241) / 240.0f;
            pixels[offset + 2] = static_cast<float>((x * 13 + y * 2) % 239) / 238.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 3) % 29) / 28.0f;
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache noDisplacement;
    ArtifactCore::ImageF32x4RGBAWithCache displaced;
    Artifact::GlitchEffect effect;
    effect.setScanlines(0.0f);
    effect.setColorShift(0.0f);
    effect.setIntensity(0.0f);
    effect.applyCPUOnly(source, noDisplacement);
    effect.setIntensity(1.0f);
    effect.applyCPUOnly(source, displaced);

    const float* baseline = noDisplacement.image().rgba32fData();
    const float* actual = displaced.image().rgba32fData();
    ASSERT_NE(baseline, nullptr);
    ASSERT_NE(actual, nullptr);
    std::size_t changedPixels = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(baseline[offset + channel], pixels[offset + channel])
                << "baseline pixel=" << pixel << " channel=" << channel;
        }
        if (actual[offset] != baseline[offset] ||
            actual[offset + 1] != baseline[offset + 1] ||
            actual[offset + 2] != baseline[offset + 2]) {
            ++changedPixels;
        }
        EXPECT_FLOAT_EQ(actual[offset + 3], pixels[offset + 3])
            << "pixel=" << pixel;
    }
    EXPECT_GT(changedPixels, 0u);
}

TEST(GlitchEffectContractTest,
     ScanlinePatchesStayWithinTheirSourceRowAndBoundedWidth)
{
    constexpr int width = 64;
    constexpr int height = 64;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4;
            pixels[offset] = static_cast<float>(x + 1) / (width + 1);
            pixels[offset + 1] = static_cast<float>(y + 1) / (height + 1);
            pixels[offset + 2] = static_cast<float>((x * 3 + y * 5) % 31) / 30.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 2) % 17) / 16.0f;
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::GlitchEffect effect;
    effect.setIntensity(0.0f);
    effect.setColorShift(0.0f);
    effect.setScanlines(1.0f);
    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    std::size_t changedRows = 0;
    for (int y = 0; y < height; ++y) {
        std::size_t changedPixels = 0;
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4;
            const bool changed = actual[offset] != pixels[offset] ||
                actual[offset + 1] != pixels[offset + 1] ||
                actual[offset + 2] != pixels[offset + 2];
            changedPixels += changed ? 1u : 0u;
            EXPECT_FLOAT_EQ(actual[offset + 3], pixels[offset + 3])
                << "x=" << x << " y=" << y;
        }
        EXPECT_LE(changedPixels, static_cast<std::size_t>(width / 4))
            << "a scanline patch is at most one quarter of the row, y=" << y;
        changedRows += changedPixels > 0 ? 1u : 0u;
    }
    EXPECT_GT(changedRows, 0u);
}

TEST(GlitchEffectContractTest,
     SeededGlitchInPlaceMatchesOutOfPlaceWithoutChangingSource)
{
    const auto descriptor =
        ArtifactCore::SurfaceColorDescriptor::linearStraightRgba32Float();
    auto sourceImage = makeColorPattern().image();
    sourceImage.setColorDescriptor(descriptor);
    const ArtifactCore::ImageF32x4RGBAWithCache source(sourceImage);
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
    EXPECT_EQ(outOfPlace.image().colorDescriptor(), descriptor);
    EXPECT_EQ(inPlace.image().colorDescriptor(), descriptor);
    EXPECT_NE(sourcePixels, inPlacePixels);
    for (std::size_t channel = 0; channel < kWidth * kHeight * 4; ++channel) {
        EXPECT_FLOAT_EQ(sourcePixels[channel], originalSource[channel])
            << "source channel=" << channel;
        EXPECT_FLOAT_EQ(inPlacePixels[channel], outOfPlacePixels[channel])
            << "output channel=" << channel;
    }
}

TEST(GlitchEffectContractTest,
     GlitchEmptyInputReplacesPreviouslyPopulatedOutput)
{
    const ArtifactCore::ImageF32x4RGBAWithCache source;
    auto output = makeColorPattern();
    ASSERT_FALSE(output.image().isEmpty());
    Artifact::GlitchEffect effect;

    effect.applyCPUOnly(source, output);

    EXPECT_TRUE(output.image().isEmpty());
    EXPECT_EQ(output.image().width(), source.image().width());
    EXPECT_EQ(output.image().height(), source.image().height());
    EXPECT_EQ(output.image().colorDescriptor(), source.image().colorDescriptor());
}

TEST(GlitchEffectContractTest,
     GlitchHandlesSinglePixelSurfaceWithClampedSampling)
{
    constexpr std::array<float, 4> pixels{0.2f, 0.4f, 0.8f, 0.35f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::GlitchEffect effect;
    effect.setIntensity(1.0f);
    effect.setScanlines(0.0f);
    effect.setColorShift(1.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 1);
    ASSERT_EQ(output.image().height(), 1);
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    EXPECT_NE(actual, source.image().rgba32fData());
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
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
