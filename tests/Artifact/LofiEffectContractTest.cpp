#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Artifact.Effect.Context;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Time.Rational;

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

TEST(LofiEffectContractTest, GlitchCpuOutputIsDeterministicAndPreservesAlpha)
{
    constexpr int width = 40;
    constexpr int height = 19;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x) / static_cast<float>(width - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[offset + 2] = static_cast<float>((x * 3 + y * 5) % 17) / 16.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 2) % 13) / 12.0f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    ArtifactGlitchEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.apply(source, firstOutput);
    effect.apply(source, secondOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* second = secondOutput.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_EQ(firstOutput.width(), width);
    ASSERT_EQ(firstOutput.height(), height);

    bool changedRgb = false;
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width * height); ++pixel) {
        for (int channel = 0; channel < 4; ++channel) {
            const std::size_t index = pixel * 4 + static_cast<std::size_t>(channel);
            EXPECT_FLOAT_EQ(first[index], second[index])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_TRUE(std::isfinite(first[index]))
                << "pixel=" << pixel << " channel=" << channel;
            if (channel == 3) {
                EXPECT_FLOAT_EQ(first[index], pixels[index]) << "pixel=" << pixel;
            } else {
                changedRgb = changedRgb || std::abs(first[index] - pixels[index]) > 1.0e-5f;
            }
        }
    }
    EXPECT_TRUE(changedRgb);
}

TEST(LofiEffectContractTest, GlitchCpuSeparatesRgbAroundTheGreenReferenceSample)
{
    constexpr int width = 64;
    constexpr int height = 12;
    constexpr int row = 5;
    constexpr int x = 24;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int column = 0; column < width; ++column) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + column) * 4);
            pixels[offset + 0] = static_cast<float>(column) / (width - 1);
            pixels[offset + 1] = static_cast<float>(column + 1) / (width + 1);
            pixels[offset + 2] = static_cast<float>(column + 2) / (width + 2);
            pixels[offset + 3] = static_cast<float>((column + y) % 7) / 6.0f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactGlitchEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    const std::size_t outputOffset =
        static_cast<std::size_t>((row * width + x) * 4);
    const std::size_t sourceOffset =
        static_cast<std::size_t>(row * width * 4);
    const float green = actual[outputOffset + 1];
    const int referenceX = static_cast<int>(std::lround(
        green * (width + 1))) - 1;
    ASSERT_GE(referenceX, 0);
    ASSERT_LT(referenceX, width);
    const int redX = static_cast<int>(std::lround(
        actual[outputOffset + 0] * (width - 1)));
    const int blueX = static_cast<int>(std::lround(
        actual[outputOffset + 2] * (width + 2))) - 2;
    const int redShift = referenceX - redX;
    const int blueShift = blueX - referenceX;
    EXPECT_GE(redShift, 3);
    EXPECT_LE(redShift, 7);
    EXPECT_EQ(blueShift, redShift);
    EXPECT_FLOAT_EQ(actual[outputOffset + 3], pixels[outputOffset + 3]);
}

TEST(LofiEffectContractTest, GlitchCpuInPlaceMatchesOutOfPlaceWithoutAliasing)
{
    constexpr int width = 37;
    constexpr int height = 21;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>((x * 7 + y) % 31) / 30.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 5) % 29) / 28.0f;
            pixels[offset + 2] = static_cast<float>((x * 3 + y * 11) % 23) / 22.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 2) % 13) / 12.0f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache inPlace(image);
    ImageF32x4RGBAWithCache outOfPlace;
    ArtifactGlitchEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, outOfPlace);
    effect.applyCPUOnly(inPlace, inPlace);

    const float* sourcePixels = source.image().rgba32fData();
    const float* outOfPlacePixels = outOfPlace.image().rgba32fData();
    const float* inPlacePixels = inPlace.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(outOfPlacePixels, nullptr);
    ASSERT_NE(inPlacePixels, nullptr);
    ASSERT_EQ(inPlace.width(), width);
    ASSERT_EQ(inPlace.height(), height);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(sourcePixels[channel], pixels[channel])
            << "source channel=" << channel;
        EXPECT_FLOAT_EQ(outOfPlacePixels[channel], inPlacePixels[channel])
            << "output channel=" << channel;
    }
    EXPECT_NE(sourcePixels, inPlacePixels);
}

TEST(LofiEffectContractTest,
     GlitchCpuEmptyInputReplacesPreviouslyPopulatedOutput)
{
    const ImageF32x4RGBAWithCache source;
    ImageF32x4RGBAWithCache output = makePatternSurface();
    ASSERT_FALSE(output.image().isEmpty());
    ArtifactGlitchEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, output);

    EXPECT_TRUE(output.image().isEmpty());
    EXPECT_EQ(output.image().width(), source.image().width());
    EXPECT_EQ(output.image().height(), source.image().height());
    EXPECT_EQ(output.image().colorDescriptor(), source.image().colorDescriptor());
}

TEST(LofiEffectContractTest,
     GlitchCpuSingleColumnClampsEveryChannelSampleToTheSourcePixel)
{
    constexpr int width = 1;
    constexpr int height = 9;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        const std::size_t offset = static_cast<std::size_t>(y * 4);
        pixels[offset] = static_cast<float>(y + 1) / 10.0f;
        pixels[offset + 1] = static_cast<float>(y + 2) / 12.0f;
        pixels[offset + 2] = static_cast<float>(y + 3) / 14.0f;
        pixels[offset + 3] = static_cast<float>(y + 4) / 16.0f;
    }
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    image.setColorDescriptor(descriptor);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactGlitchEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.width(), width);
    ASSERT_EQ(output.height(), height);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
    EXPECT_NE(actual, source.image().rgba32fData());
}

TEST(LofiEffectContractTest, OldTvCpuReferencePreservesConfiguredComputeMode)
{
    const auto source = makePatternSurface();
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::GPU);

    ASSERT_TRUE(effect.supportsCPU());
    ASSERT_TRUE(effect.supportsGPU());
    effect.applyCPUOnly(source, output);

    EXPECT_EQ(effect.computeMode(), ComputeMode::GPU);
    ASSERT_EQ(output.image().width(), source.image().width());
    ASSERT_EQ(output.image().height(), source.image().height());
    const float* pixels = output.image().rgba32fData();
    ASSERT_NE(pixels, nullptr);
    for (std::size_t channel = 0; channel < 8 * 8 * 4; ++channel) {
        EXPECT_TRUE(std::isfinite(pixels[channel])) << "channel=" << channel;
    }
}

TEST(LofiEffectContractTest,
     OldTvOutputIsStableAcrossCompositionFrames)
{
    const auto source = makePatternSurface();
    ImageF32x4RGBAWithCache frameZero;
    ImageF32x4RGBAWithCache laterFrame;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    EffectContext context;
    context.compositionFrame = 0;
    effect.setContext(context);
    effect.applyCPUOnly(source, frameZero);
    context.compositionFrame = 17;
    effect.setContext(context);
    effect.applyCPUOnly(source, laterFrame);

    const float* first = frameZero.image().rgba32fData();
    const float* later = laterFrame.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(later, nullptr);
    for (std::size_t channel = 0; channel < 8 * 8 * 4; ++channel) {
        EXPECT_FLOAT_EQ(first[channel], later[channel])
            << "channel=" << channel;
    }
}

TEST(LofiEffectContractTest, OldTvPreservesInputColorDescriptor)
{
    constexpr std::array<float, 16> pixels{
        0.1f, 0.2f, 0.3f, 0.4f,
        0.2f, 0.3f, 0.4f, 0.5f,
        0.3f, 0.4f, 0.5f, 0.6f,
        0.4f, 0.5f, 0.6f, 0.7f,
    };
    const auto descriptor =
        SurfaceColorDescriptor::linearStraightRgba32Float();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 2);
    image.setColorDescriptor(descriptor);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, output);

    EXPECT_EQ(source.image().colorDescriptor(), descriptor);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
}

TEST(LofiEffectContractTest, OldTvOutputDoesNotSharePixelStorageWithInput)
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        pixels[channel] = 0.2f + static_cast<float>(channel % 17) * 0.01f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, output);

    float* sourcePixels = source.image().rgba32fData();
    float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(outputPixels, nullptr);
    const float originalOutputChannel0 = outputPixels[0];
    const float originalSourceChannel4 = sourcePixels[4];

    sourcePixels[0] = -17.0f;
    EXPECT_FLOAT_EQ(outputPixels[0], originalOutputChannel0);
    outputPixels[4] = 23.0f;
    EXPECT_FLOAT_EQ(sourcePixels[4], originalSourceChannel4);
}

TEST(LofiEffectContractTest,
     FilmGrungeZeroStrengthControlsPreserveTheInputExactly)
{
    constexpr int width = 7;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        pixels[channel] = 0.1f + static_cast<float>(channel % 23) * 0.025f;
    }
    const auto descriptor =
        SurfaceColorDescriptor::linearStraightRgba32Float();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    image.setColorDescriptor(descriptor);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(LofiEffectContractTest, FilmGrungeInPlaceMatchesOutOfPlaceAndKeepsIndependentStorage)
{
    constexpr int width = 13;
    constexpr int height = 9;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>((x * 7 + y * 3) % 19) / 18.0f;
            pixels[offset + 1] = static_cast<float>((x * 2 + y * 11) % 23) / 22.0f;
            pixels[offset + 2] = static_cast<float>((x * 5 + y * 13) % 29) / 28.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 2) % 7) / 6.0f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache inPlace(image);
    ImageF32x4RGBAWithCache output;
    ArtifactFilmGrungeEffect outOfPlaceEffect;
    ArtifactFilmGrungeEffect inPlaceEffect;
    outOfPlaceEffect.setComputeMode(ComputeMode::CPU);
    inPlaceEffect.setComputeMode(ComputeMode::CPU);
    outOfPlaceEffect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    inPlaceEffect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));

    outOfPlaceEffect.applyCPUOnly(source, output);
    inPlaceEffect.applyCPUOnly(inPlace, inPlace);

    const float* sourcePixels = source.image().rgba32fData();
    const float* outputPixels = output.image().rgba32fData();
    const float* inPlacePixels = inPlace.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(outputPixels, nullptr);
    ASSERT_NE(inPlacePixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(sourcePixels[channel], pixels[channel])
            << "source channel=" << channel;
        EXPECT_FLOAT_EQ(outputPixels[channel], inPlacePixels[channel])
            << "output channel=" << channel;
    }
    EXPECT_NE(outputPixels, sourcePixels);
    EXPECT_NE(inPlacePixels, sourcePixels);
}

TEST(LofiEffectContractTest,
     FilmGrungeEmptyInputReplacesPreviouslyPopulatedOutput)
{
    const ImageF32x4RGBAWithCache source;
    ImageF32x4RGBAWithCache output = makePatternSurface();
    ASSERT_FALSE(output.image().isEmpty());
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, output);

    EXPECT_TRUE(output.image().isEmpty());
    EXPECT_EQ(output.image().width(), source.image().width());
    EXPECT_EQ(output.image().height(), source.image().height());
    EXPECT_EQ(output.image().colorDescriptor(), source.image().colorDescriptor());
}

TEST(LofiEffectContractTest,
     FilmGrungeProcessesSinglePixelSurfaceWithFiniteColorAndStableAlpha)
{
    constexpr std::array<float, 4> pixels{0.25f, 0.5f, 0.75f, 0.375f};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    image.setColorDescriptor(descriptor);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("gateWeave"), QVariant(12.0f));
    EffectContext context;
    context.compositionFrame = 9;
    effect.setContext(context);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.width(), 1);
    ASSERT_EQ(output.height(), 1);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (int channel = 0; channel < 3; ++channel) {
        EXPECT_TRUE(std::isfinite(result[channel])) << "channel=" << channel;
    }
    EXPECT_FLOAT_EQ(result[3], pixels[3]);
}

TEST(LofiEffectContractTest,
     FilmGrungeProcessesSingleRowAndColumnWithFinitePixels)
{
    constexpr int extent = 100;
    std::array<float, extent * 4> pixels{};
    for (int pixel = 0; pixel < extent; ++pixel) {
        const std::size_t offset = static_cast<std::size_t>(pixel * 4);
        pixels[offset] = static_cast<float>((pixel * 3) % 17) / 16.0f;
        pixels[offset + 1] = static_cast<float>((pixel * 5) % 19) / 18.0f;
        pixels[offset + 2] = static_cast<float>((pixel * 7) % 23) / 22.0f;
        pixels[offset + 3] = static_cast<float>((pixel * 2) % 11) / 10.0f;
    }
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("gateWeave"), QVariant(12.0f));
    EffectContext context;
    context.compositionFrame = 14;
    effect.setContext(context);

    for (const auto [width, height] :
         std::array<std::pair<int, int>, 2>{{{extent, 1}, {1, extent}}}) {
        ImageF32x4_RGBA image;
        image.setFromRGBA32F(pixels.data(), width, height);
        image.setColorDescriptor(descriptor);
        const ImageF32x4RGBAWithCache source(image);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);

        ASSERT_EQ(output.width(), width);
        ASSERT_EQ(output.height(), height);
        EXPECT_EQ(output.image().colorDescriptor(), descriptor);
        const float* result = output.image().rgba32fData();
        ASSERT_NE(result, nullptr);
        for (int pixel = 0; pixel < extent; ++pixel) {
            const std::size_t offset = static_cast<std::size_t>(pixel * 4);
            for (int channel = 0; channel < 3; ++channel) {
                EXPECT_TRUE(std::isfinite(result[offset + channel]))
                    << "width=" << width << " height=" << height
                    << " pixel=" << pixel << " channel=" << channel;
            }
            EXPECT_FLOAT_EQ(result[offset + 3], pixels[offset + 3])
                << "width=" << width << " height=" << height
                << " pixel=" << pixel;
        }
    }
}

TEST(LofiEffectContractTest,
     FilmGrungeIsDeterministicWhenFramesAreRevisitedAndChangesAcrossFrames)
{
    constexpr int width = 64;
    constexpr int height = 48;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>((x * 3 + y) % 31) / 30.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 5) % 29) / 28.0f;
            pixels[offset + 2] = static_cast<float>((x * 7 + y * 2) % 37) / 36.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache firstFrame;
    ImageF32x4RGBAWithCache repeatedFrame;
    ImageF32x4RGBAWithCache nextFrame;
    ImageF32x4RGBAWithCache revisitedFrame;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.applyCPUOnly(source, firstFrame);
    effect.applyCPUOnly(source, repeatedFrame);

    context.compositionFrame = 13;
    effect.setContext(context);
    effect.applyCPUOnly(source, nextFrame);
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.applyCPUOnly(source, revisitedFrame);

    const float* firstPixels = firstFrame.image().rgba32fData();
    const float* repeatedPixels = repeatedFrame.image().rgba32fData();
    const float* nextPixels = nextFrame.image().rgba32fData();
    const float* revisitedPixels = revisitedFrame.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(repeatedPixels, nullptr);
    ASSERT_NE(nextPixels, nullptr);
    ASSERT_NE(revisitedPixels, nullptr);
    std::size_t changedRgbSamples = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(firstPixels[offset], repeatedPixels[offset])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(firstPixels[offset], revisitedPixels[offset])
                << "revisited pixel=" << pixel << " channel=" << channel;
            if (channel < 3 && firstPixels[offset] != nextPixels[offset]) {
                ++changedRgbSamples;
            }
        }
    }
    EXPECT_GT(changedRgbSamples, 0u);
}

TEST(LofiEffectContractTest,
     FilmGrungeFrameUpdatesPreserveDimensionsDescriptorAndAlpha)
{
    constexpr int width = 17;
    constexpr int height = 11;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>((x * 3 + y) % 13) / 12.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 5) % 17) / 16.0f;
            pixels[offset + 2] = static_cast<float>((x * 7 + y * 2) % 19) / 18.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 3) % 9) / 8.0f;
        }
    }
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    image.setColorDescriptor(descriptor);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache frame12;
    ImageF32x4RGBAWithCache repeatedFrame12;
    ImageF32x4RGBAWithCache frame13;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.applyCPUOnly(source, frame12);
    effect.applyCPUOnly(source, repeatedFrame12);
    context.compositionFrame = 13;
    effect.setContext(context);
    effect.applyCPUOnly(source, frame13);

    const std::array<const ImageF32x4RGBAWithCache*, 3> outputs{
        &frame12, &repeatedFrame12, &frame13};
    const float* firstFramePixels = frame12.image().rgba32fData();
    const float* repeatedFramePixels = repeatedFrame12.image().rgba32fData();
    const float* nextFramePixels = frame13.image().rgba32fData();
    ASSERT_NE(firstFramePixels, nullptr);
    ASSERT_NE(repeatedFramePixels, nullptr);
    ASSERT_NE(nextFramePixels, nullptr);
    bool nextFrameChangesRgb = false;
    for (const auto* output : outputs) {
        ASSERT_NE(output->image().rgba32fData(), nullptr);
        EXPECT_EQ(output->width(), width);
        EXPECT_EQ(output->height(), height);
        EXPECT_EQ(output->image().colorDescriptor(), descriptor);
        const float* result = output->image().rgba32fData();
        for (std::size_t pixel = 0;
             pixel < static_cast<std::size_t>(width) * height; ++pixel) {
            EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3])
                << "pixel=" << pixel;
        }
    }
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(firstFramePixels[channel], repeatedFramePixels[channel])
            << "channel=" << channel;
        if (channel % 4 != 3 && firstFramePixels[channel] != nextFramePixels[channel]) {
            nextFrameChangesRgb = true;
        }
    }
    EXPECT_TRUE(nextFrameChangesRgb);
}

TEST(LofiEffectContractTest,
     FilmGrungeGateWeaveDisplacesAlphaWithTheColorSample)
{
    constexpr int width = 32;
    constexpr int height = 24;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float pattern =
                static_cast<float>((x * 3 + y * 7) % 47) / 46.0f;
            pixels[offset + 0] = static_cast<float>(x) / (width - 1);
            pixels[offset + 1] = static_cast<float>(y) / (height - 1);
            pixels[offset + 2] = pattern;
            pixels[offset + 3] = pattern;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("gateWeave"), QVariant(4.0f));
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);

    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    std::size_t displacedSamples = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        EXPECT_FLOAT_EQ(result[pixel * 4 + 2], result[pixel * 4 + 3])
            << "pixel=" << pixel;
        if (std::abs(result[pixel * 4 + 2] - pixels[pixel * 4 + 2]) > 1.0e-5f) {
            ++displacedSamples;
        }
    }
    EXPECT_GT(displacedSamples, 0u);
}

TEST(LofiEffectContractTest,
     FilmGrungeAnimatesGateWeaveWithColorAndAlphaInLockstep)
{
    constexpr int width = 32;
    constexpr int height = 24;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float pattern =
                static_cast<float>((x * 3 + y * 7) % 47) / 46.0f;
            pixels[offset] = static_cast<float>(x) / (width - 1);
            pixels[offset + 1] = static_cast<float>(y) / (height - 1);
            pixels[offset + 2] = pattern;
            pixels[offset + 3] = pattern;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);

    ArtifactFilmGrungeEffect animatedEffect;
    animatedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "vignette", "stain"}) {
        animatedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    auto* gateWeave =
        animatedEffect.editableProperty(QStringLiteral("gateWeave"));
    ASSERT_NE(gateWeave, nullptr);
    gateWeave->addKeyFrame(RationalTime(0, 30), 0.0);
    gateWeave->addKeyFrame(RationalTime(30, 30), 4.0);

    EffectContext context;
    context.compositionFrame = 15;
    animatedEffect.setContext(context);
    ImageF32x4RGBAWithCache animatedFrame;
    animatedEffect.applyCPUOnly(source, animatedFrame);

    ArtifactFilmGrungeEffect fixedEffect;
    fixedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "vignette", "stain"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("gateWeave"), QVariant(2.0f));
    fixedEffect.setContext(context);
    ImageF32x4RGBAWithCache fixedFrame;
    fixedEffect.applyCPUOnly(source, fixedFrame);

    const float* animated = animatedFrame.image().rgba32fData();
    const float* fixed = fixedFrame.image().rgba32fData();
    ASSERT_NE(animated, nullptr);
    ASSERT_NE(fixed, nullptr);
    std::size_t displacedPixels = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(animated[offset], fixed[offset])
                << "pixel=" << pixel << " channel=" << channel;
        }
        if (std::abs(animated[pixel * 4 + 2] - pixels[pixel * 4 + 2]) >
            1.0e-5f) {
            ++displacedPixels;
        }
        EXPECT_FLOAT_EQ(animated[pixel * 4 + 2], animated[pixel * 4 + 3])
            << "pixel=" << pixel;
    }
    EXPECT_GT(displacedPixels, 0u);
}

TEST(LofiEffectContractTest, FilmGrungeGrainSizeDefinesSharedSpatialBlocks)
{
    constexpr int width = 18;
    constexpr int height = 11;
    constexpr float alpha = 0.65f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4 + 0] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ImageF32x4RGBAWithCache otherFrameOutput;
    ImageF32x4RGBAWithCache fineGrainOutput;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    effect.setPropertyValue(UniString("grainSize"), QVariant(4.0f));
    for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.applyCPUOnly(source, output);
    context.compositionFrame = 13;
    effect.setContext(context);
    effect.applyCPUOnly(source, otherFrameOutput);
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.applyCPUOnly(source, fineGrainOutput);

    const float* result = output.image().rgba32fData();
    const float* otherFrame = otherFrameOutput.image().rgba32fData();
    const float* fineGrain = fineGrainOutput.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    ASSERT_NE(otherFrame, nullptr);
    ASSERT_NE(fineGrain, nullptr);
    const float firstBlockSample = result[1];
    bool observedBlockVariation = false;
    bool observedFineVariationWithinCoarseBlock = false;
    std::size_t changedByFrame = 0;
    for (int blockY = 0; blockY < height; blockY += 4) {
        for (int blockX = 0; blockX < width; blockX += 4) {
            const std::size_t blockOffset =
                static_cast<std::size_t>((blockY * width + blockX) * 4);
            const std::array<float, 4> blockValue{
                result[blockOffset + 0], result[blockOffset + 1],
                result[blockOffset + 2], result[blockOffset + 3]};
            const float fineBlockSample = fineGrain[blockOffset + 1];
            observedBlockVariation = observedBlockVariation ||
                blockValue[1] != firstBlockSample;
            for (int y = blockY; y < std::min(blockY + 4, height); ++y) {
                for (int x = blockX; x < std::min(blockX + 4, width); ++x) {
                    const std::size_t offset =
                        static_cast<std::size_t>((y * width + x) * 4);
                    for (std::size_t channel = 0; channel < 3; ++channel) {
                        EXPECT_FLOAT_EQ(result[offset + channel], blockValue[channel])
                            << "x=" << x << " y=" << y
                            << " channel=" << channel;
                        EXPECT_FLOAT_EQ(
                            otherFrame[offset + channel],
                            otherFrame[blockOffset + channel])
                            << "other-frame grain block x=" << x << " y=" << y
                            << " channel=" << channel;
                    }
                    observedFineVariationWithinCoarseBlock =
                        observedFineVariationWithinCoarseBlock ||
                        fineGrain[offset + 1] != fineBlockSample;
                    EXPECT_FLOAT_EQ(fineGrain[offset + 3], alpha)
                        << "fine grain alpha x=" << x << " y=" << y;
                    if (result[offset + 1] != otherFrame[offset + 1]) {
                        ++changedByFrame;
                    }
                    EXPECT_FLOAT_EQ(result[offset + 3], alpha)
                        << "x=" << x << " y=" << y;
                    EXPECT_FLOAT_EQ(otherFrame[offset + 3], alpha)
                        << "other frame x=" << x << " y=" << y;
                }
            }
        }
    }
    EXPECT_TRUE(observedBlockVariation);
    EXPECT_TRUE(observedFineVariationWithinCoarseBlock);
    EXPECT_GT(changedByFrame, 0u);
}

TEST(LofiEffectContractTest,
     FilmGrungeScratchAmountAddsLocalizedSharedRgbDamage)
{
    constexpr int width = 72;
    constexpr int height = 32;
    constexpr float alpha = 0.7f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4 + 0] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache noScratch;
    ImageF32x4RGBAWithCache scratched;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("scratchAmount"), QVariant(0.0f));
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.applyCPUOnly(source, noScratch);

    effect.setPropertyValue(UniString("scratchAmount"), QVariant(1.0f));
    effect.applyCPUOnly(source, scratched);

    const float* baseline = noScratch.image().rgba32fData();
    const float* result = scratched.image().rgba32fData();
    ASSERT_NE(baseline, nullptr);
    ASSERT_NE(result, nullptr);
    int changedColumns = 0;
    for (int x = 0; x < width; ++x) {
        bool columnChanged = false;
        for (int y = 0; y < height; ++y) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            EXPECT_FLOAT_EQ(result[offset + 0], result[offset + 1])
                << "x=" << x << " y=" << y << " R-G";
            EXPECT_FLOAT_EQ(result[offset + 1], result[offset + 2])
                << "x=" << x << " y=" << y << " G-B";
            EXPECT_FLOAT_EQ(result[offset + 3], alpha)
                << "x=" << x << " y=" << y;
            columnChanged = columnChanged ||
                std::abs(result[offset + 1] - baseline[offset + 1]) > 1.0e-5f;
        }
        changedColumns += columnChanged ? 1 : 0;
    }
    EXPECT_GT(changedColumns, 0);
    EXPECT_LE(changedColumns, 12)
        << "four scratch columns may affect at most three columns each";
}

TEST(LofiEffectContractTest,
     FilmGrungeAnimatesScratchAmountAgainstFixedFrameOutput)
{
    constexpr int width = 72;
    constexpr int height = 32;
    constexpr float alpha = 0.7f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);

    ArtifactFilmGrungeEffect animatedEffect;
    animatedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        animatedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    auto* scratch =
        animatedEffect.editableProperty(QStringLiteral("scratchAmount"));
    ASSERT_NE(scratch, nullptr);
    scratch->addKeyFrame(RationalTime(0, 30), 0.0);
    scratch->addKeyFrame(RationalTime(30, 30), 1.0);

    EffectContext context;
    context.compositionFrame = 15;
    animatedEffect.setContext(context);
    ImageF32x4RGBAWithCache animatedFrame;
    animatedEffect.applyCPUOnly(source, animatedFrame);

    ArtifactFilmGrungeEffect fixedEffect;
    fixedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("scratchAmount"), QVariant(0.5f));
    fixedEffect.setContext(context);
    ImageF32x4RGBAWithCache fixedFrame;
    fixedEffect.applyCPUOnly(source, fixedFrame);

    const float* animated = animatedFrame.image().rgba32fData();
    const float* fixed = fixedFrame.image().rgba32fData();
    ASSERT_NE(animated, nullptr);
    ASSERT_NE(fixed, nullptr);
    std::array<bool, width> changedColumns{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(animated[offset], fixed[offset])
                << "pixel=" << pixel << " channel=" << channel;
        }
        if (animated[pixel * 4 + 1] != 0.5f) {
            changedColumns[pixel % width] = true;
        }
        EXPECT_FLOAT_EQ(animated[pixel * 4 + 3], alpha)
            << "pixel=" << pixel;
    }
    const std::size_t changedColumnCount = static_cast<std::size_t>(
        std::count(changedColumns.begin(), changedColumns.end(), true));
    EXPECT_GT(changedColumnCount, 0u);
    EXPECT_LE(changedColumnCount, 6u)
        << "two animated scratch columns may affect at most three columns each";
}

TEST(LofiEffectContractTest, FilmGrungeDustAmountAddsLocalizedDeterministicDamage)
{
    constexpr int width = 128;
    constexpr int height = 96;
    constexpr float alpha = 0.7f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4 + 0] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache noDust;
    ImageF32x4RGBAWithCache firstDust;
    ImageF32x4RGBAWithCache repeatedDust;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("dustAmount"), QVariant(0.0f));
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.applyCPUOnly(source, noDust);

    effect.setPropertyValue(UniString("dustAmount"), QVariant(1.0f));
    effect.applyCPUOnly(source, firstDust);
    effect.applyCPUOnly(source, repeatedDust);

    const float* baseline = noDust.image().rgba32fData();
    const float* result = firstDust.image().rgba32fData();
    const float* repeated = repeatedDust.image().rgba32fData();
    ASSERT_NE(baseline, nullptr);
    ASSERT_NE(result, nullptr);
    ASSERT_NE(repeated, nullptr);
    std::size_t changedPixels = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        EXPECT_FLOAT_EQ(result[offset + 0], result[offset + 1])
            << "pixel=" << pixel << " R-G";
        EXPECT_FLOAT_EQ(result[offset + 1], result[offset + 2])
            << "pixel=" << pixel << " G-B";
        EXPECT_FLOAT_EQ(result[offset + 3], alpha) << "pixel=" << pixel;
        for (std::size_t channel = 0; channel < 4; ++channel) {
            EXPECT_FLOAT_EQ(result[offset + channel], repeated[offset + channel])
                << "pixel=" << pixel << " channel=" << channel;
        }
        if (result[offset + 1] != baseline[offset + 1]) {
            ++changedPixels;
            EXPECT_LT(result[offset + 1], baseline[offset + 1])
                << "dust should darken pixels";
        }
    }
    EXPECT_GT(changedPixels, 0u);
    EXPECT_LT(changedPixels, static_cast<std::size_t>(width) * height);
}

TEST(LofiEffectContractTest,
     FilmGrungeAnimatesDustAmountAgainstFixedFrameOutput)
{
    constexpr int width = 128;
    constexpr int height = 96;
    constexpr float alpha = 0.7f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);

    ArtifactFilmGrungeEffect animatedEffect;
    animatedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        animatedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    auto* dust = animatedEffect.editableProperty(QStringLiteral("dustAmount"));
    ASSERT_NE(dust, nullptr);
    dust->addKeyFrame(RationalTime(0, 30), 0.0);
    dust->addKeyFrame(RationalTime(30, 30), 1.0);

    EffectContext context;
    context.compositionFrame = 15;
    animatedEffect.setContext(context);
    ImageF32x4RGBAWithCache animatedFrame;
    animatedEffect.applyCPUOnly(source, animatedFrame);

    ArtifactFilmGrungeEffect fixedEffect;
    fixedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("dustAmount"), QVariant(0.5f));
    fixedEffect.setContext(context);
    ImageF32x4RGBAWithCache fixedFrame;
    fixedEffect.applyCPUOnly(source, fixedFrame);

    const float* animated = animatedFrame.image().rgba32fData();
    const float* fixed = fixedFrame.image().rgba32fData();
    ASSERT_NE(animated, nullptr);
    ASSERT_NE(fixed, nullptr);
    std::size_t changedPixels = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 4; ++channel) {
            EXPECT_FLOAT_EQ(animated[offset + channel], fixed[offset + channel])
                << "pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(animated[offset], animated[offset + 1])
            << "pixel=" << pixel << " R-G";
        EXPECT_FLOAT_EQ(animated[offset + 1], animated[offset + 2])
            << "pixel=" << pixel << " G-B";
        EXPECT_FLOAT_EQ(animated[offset + 3], alpha) << "pixel=" << pixel;
        if (animated[offset + 1] != 0.5f) {
            ++changedPixels;
            EXPECT_LT(animated[offset + 1], 0.5f)
                << "dust should darken pixels";
        }
    }
    EXPECT_GT(changedPixels, 0u);
    EXPECT_LT(changedPixels, static_cast<std::size_t>(width) * height);
}

TEST(LofiEffectContractTest,
     FilmGrungeFlickerChangesFrameExposureWithoutChangingAlpha)
{
    constexpr int width = 12;
    constexpr int height = 8;
    constexpr float sourceValue = 0.4f;
    constexpr float alpha = 0.65f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4 + 0] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache noFlicker;
    ImageF32x4RGBAWithCache firstFrame;
    ImageF32x4RGBAWithCache repeatedFrame;
    ImageF32x4RGBAWithCache nextFrame;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.setPropertyValue(UniString("flicker"), QVariant(0.0f));
    effect.applyCPUOnly(source, noFlicker);

    effect.setPropertyValue(UniString("flicker"), QVariant(0.5f));
    effect.applyCPUOnly(source, firstFrame);
    effect.applyCPUOnly(source, repeatedFrame);
    context.compositionFrame = 13;
    effect.setContext(context);
    effect.applyCPUOnly(source, nextFrame);

    const float* baseline = noFlicker.image().rgba32fData();
    const float* first = firstFrame.image().rgba32fData();
    const float* repeated = repeatedFrame.image().rgba32fData();
    const float* next = nextFrame.image().rgba32fData();
    ASSERT_NE(baseline, nullptr);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(repeated, nullptr);
    ASSERT_NE(next, nullptr);
    EXPECT_FLOAT_EQ(baseline[0], sourceValue);
    EXPECT_NE(first[0], baseline[0]);
    EXPECT_NE(next[0], first[0]);
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(first[offset + channel], first[0])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(first[offset + channel], repeated[offset + channel])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(next[offset + channel], next[0])
                << "pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(first[offset + 3], alpha) << "pixel=" << pixel;
        EXPECT_FLOAT_EQ(next[offset + 3], alpha) << "pixel=" << pixel;
    }
}

TEST(LofiEffectContractTest,
     FilmGrungeAnimatesFlickerAgainstFixedFrameExposure)
{
    constexpr int width = 12;
    constexpr int height = 8;
    constexpr float sourceValue = 0.4f;
    constexpr float alpha = 0.65f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);

    ArtifactFilmGrungeEffect animatedEffect;
    animatedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "gateWeave", "vignette", "stain"}) {
        animatedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    auto* flicker =
        animatedEffect.editableProperty(QStringLiteral("flicker"));
    ASSERT_NE(flicker, nullptr);
    flicker->addKeyFrame(RationalTime(0, 30), 0.0);
    flicker->addKeyFrame(RationalTime(30, 30), 0.5);

    EffectContext context;
    context.compositionFrame = 15;
    animatedEffect.setContext(context);
    ImageF32x4RGBAWithCache animatedFrame;
    animatedEffect.applyCPUOnly(source, animatedFrame);

    ArtifactFilmGrungeEffect fixedEffect;
    fixedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "gateWeave", "vignette", "stain"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("flicker"), QVariant(0.25f));
    fixedEffect.setContext(context);
    ImageF32x4RGBAWithCache fixedFrame;
    fixedEffect.applyCPUOnly(source, fixedFrame);

    const float* animated = animatedFrame.image().rgba32fData();
    const float* fixed = fixedFrame.image().rgba32fData();
    ASSERT_NE(animated, nullptr);
    ASSERT_NE(fixed, nullptr);
    EXPECT_NE(animated[0], sourceValue)
        << "the interpolated exposure flicker should affect RGB";
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(animated[offset + channel], fixed[offset + channel])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(animated[offset + channel], animated[channel])
                << "uniform exposure pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(animated[offset + 3], alpha) << "pixel=" << pixel;
    }
}

TEST(LofiEffectContractTest,
     FilmGrungeEmulsionStainAddsDeterministicSpatialColorVariation)
{
    constexpr int width = 32;
    constexpr int height = 24;
    constexpr float sourceValue = 0.4f;
    constexpr float alpha = 0.65f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4 + 0] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache noStain;
    ImageF32x4RGBAWithCache firstStain;
    ImageF32x4RGBAWithCache repeatedStain;
    ImageF32x4RGBAWithCache otherFrameStain;
    ImageF32x4RGBAWithCache otherSeedStain;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "gateWeave", "vignette"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.setPropertyValue(UniString("stain"), QVariant(0.0f));
    effect.applyCPUOnly(source, noStain);

    effect.setPropertyValue(UniString("stain"), QVariant(0.5f));
    effect.applyCPUOnly(source, firstStain);
    effect.applyCPUOnly(source, repeatedStain);
    context.compositionFrame = 13;
    effect.setContext(context);
    effect.applyCPUOnly(source, otherFrameStain);
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.setPropertyValue(UniString("seed"), QVariant(1978));
    effect.applyCPUOnly(source, otherSeedStain);

    const float* baseline = noStain.image().rgba32fData();
    const float* result = firstStain.image().rgba32fData();
    const float* repeated = repeatedStain.image().rgba32fData();
    const float* otherFrame = otherFrameStain.image().rgba32fData();
    const float* otherSeed = otherSeedStain.image().rgba32fData();
    ASSERT_NE(baseline, nullptr);
    ASSERT_NE(result, nullptr);
    ASSERT_NE(repeated, nullptr);
    ASSERT_NE(otherFrame, nullptr);
    ASSERT_NE(otherSeed, nullptr);
    float minimum = std::numeric_limits<float>::infinity();
    float maximum = -std::numeric_limits<float>::infinity();
    std::size_t changedBySeed = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        EXPECT_FLOAT_EQ(baseline[offset], sourceValue);
        EXPECT_FLOAT_EQ(baseline[offset + 1], sourceValue);
        EXPECT_FLOAT_EQ(baseline[offset + 2], sourceValue);
        EXPECT_FLOAT_EQ(result[offset + 0], result[offset + 1]);
        EXPECT_FLOAT_EQ(result[offset + 1], result[offset + 2]);
        EXPECT_FLOAT_EQ(result[offset + 3], alpha);
        for (std::size_t channel = 0; channel < 4; ++channel) {
            EXPECT_FLOAT_EQ(result[offset + channel], repeated[offset + channel])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(result[offset + channel], otherFrame[offset + channel])
                << "frame-independent stain pixel=" << pixel
                << " channel=" << channel;
            if (channel < 3 && result[offset + channel] != otherSeed[offset + channel]) {
                ++changedBySeed;
            }
        }
        minimum = std::min(minimum, result[offset]);
        maximum = std::max(maximum, result[offset]);
    }
    EXPECT_GT(maximum - minimum, 1.0e-3f);
    EXPECT_GT(changedBySeed, 0u);
}

TEST(LofiEffectContractTest,
     FilmGrungeVignetteDarkensCornersWithAStableRadialProfile)
{
    constexpr int width = 17;
    constexpr int height = 13;
    constexpr float sourceValue = 0.4f;
    constexpr float alpha = 0.65f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4 + 0] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache noVignette;
    ImageF32x4RGBAWithCache vignetted;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "gateWeave", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);
    effect.setPropertyValue(UniString("vignette"), QVariant(0.0f));
    effect.applyCPUOnly(source, noVignette);

    constexpr float vignette = 0.2f;
    effect.setPropertyValue(UniString("vignette"), QVariant(vignette));
    effect.applyCPUOnly(source, vignetted);

    const float* baseline = noVignette.image().rgba32fData();
    const float* result = vignetted.image().rgba32fData();
    ASSERT_NE(baseline, nullptr);
    ASSERT_NE(result, nullptr);
    const auto channelAt = [result, width](int x, int y, int channel) {
        return result[(static_cast<std::size_t>(y * width + x) * 4) + channel];
    };
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(baseline[offset + channel], sourceValue);
        }
        EXPECT_FLOAT_EQ(result[offset + 3], alpha) << "pixel=" << pixel;
    }
    const float center = channelAt(width / 2, height / 2, 0);
    const float edge = channelAt(width / 2, 0, 0);
    const float corner = channelAt(0, 0, 0);
    const float expectedCorner = sourceValue *
        (1.0f - vignette * std::pow(2.0f, 1.35f));
    EXPECT_NEAR(center, sourceValue, 1.0e-6f);
    EXPECT_NEAR(edge, sourceValue * (1.0f - vignette), 1.0e-6f);
    EXPECT_NEAR(corner, expectedCorner, 1.0e-6f);
    EXPECT_GT(center, edge);
    EXPECT_GT(edge, corner);
}

TEST(LofiEffectContractTest,
     FilmGrungeSeedClampsAndControlsTheDeterministicOutput)
{
    constexpr int width = 64;
    constexpr int height = 48;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>((x * 3 + y) % 31) / 30.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 5) % 29) / 28.0f;
            pixels[offset + 2] = static_cast<float>((x * 7 + y * 2) % 37) / 36.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);
    const auto currentSeed = [&effect]() {
        const auto properties = effect.getProperties();
        const auto found = std::find_if(
            properties.begin(), properties.end(), [](const auto& property) {
                return property.getName() == QStringLiteral("seed");
            });
        return found == properties.end() ? -1 : found->getValue().toInt();
    };

    EXPECT_EQ(currentSeed(), 1977);
    effect.setPropertyValue(UniString("seed"), QVariant(-25));
    EXPECT_EQ(currentSeed(), 0);
    effect.setPropertyValue(UniString("seed"), QVariant(100025));
    EXPECT_EQ(currentSeed(), 100000);

    ImageF32x4RGBAWithCache defaultSeedOutput;
    ImageF32x4RGBAWithCache changedSeedOutput;
    ImageF32x4RGBAWithCache repeatedChangedSeedOutput;
    ImageF32x4RGBAWithCache restoredSeedOutput;
    effect.setPropertyValue(UniString("seed"), QVariant(1977));
    effect.applyCPUOnly(source, defaultSeedOutput);
    effect.setPropertyValue(UniString("seed"), QVariant(1978));
    effect.applyCPUOnly(source, changedSeedOutput);
    effect.applyCPUOnly(source, repeatedChangedSeedOutput);
    effect.setPropertyValue(UniString("seed"), QVariant(1977));
    effect.applyCPUOnly(source, restoredSeedOutput);

    const float* defaultPixels = defaultSeedOutput.image().rgba32fData();
    const float* changedPixels = changedSeedOutput.image().rgba32fData();
    const float* repeatedPixels = repeatedChangedSeedOutput.image().rgba32fData();
    const float* restoredPixels = restoredSeedOutput.image().rgba32fData();
    ASSERT_NE(defaultPixels, nullptr);
    ASSERT_NE(changedPixels, nullptr);
    ASSERT_NE(repeatedPixels, nullptr);
    ASSERT_NE(restoredPixels, nullptr);
    std::size_t changedRgbSamples = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(changedPixels[offset], repeatedPixels[offset])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(defaultPixels[offset], restoredPixels[offset])
                << "restored pixel=" << pixel << " channel=" << channel;
            if (channel < 3 && defaultPixels[offset] != changedPixels[offset]) {
                ++changedRgbSamples;
            }
        }
    }
    EXPECT_GT(changedRgbSamples, 0u);
}

TEST(LofiEffectContractTest, FilmGrungeAnimatesGrainAmountAcrossCompositionFrames)
{
    constexpr int width = 31;
    constexpr int height = 19;
    constexpr float sourceValue = 0.5f;
    constexpr float alpha = 0.625f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    auto* grainAmount = effect.editableProperty(QStringLiteral("grainAmount"));
    ASSERT_NE(grainAmount, nullptr);
    grainAmount->addKeyFrame(RationalTime(0, 30), 0.0);
    grainAmount->addKeyFrame(RationalTime(30, 30), 0.4);

    ImageF32x4RGBAWithCache firstFrame;
    ImageF32x4RGBAWithCache interpolatedFrame;
    ImageF32x4RGBAWithCache animatedFrame;
    EffectContext context;
    context.compositionFrame = 0;
    effect.setContext(context);
    effect.applyCPUOnly(source, firstFrame);
    context.compositionFrame = 15;
    effect.setContext(context);
    effect.applyCPUOnly(source, interpolatedFrame);
    context.compositionFrame = 30;
    effect.setContext(context);
    effect.applyCPUOnly(source, animatedFrame);

    ArtifactFilmGrungeEffect fixedEffect;
    fixedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    fixedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.4f));
    ImageF32x4RGBAWithCache fixedFrame;
    fixedEffect.setContext(context);
    fixedEffect.applyCPUOnly(source, fixedFrame);

    context.compositionFrame = 15;
    fixedEffect.setContext(context);
    fixedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    ImageF32x4RGBAWithCache interpolatedFixedFrame;
    fixedEffect.applyCPUOnly(source, interpolatedFixedFrame);

    const float* firstPixels = firstFrame.image().rgba32fData();
    const float* interpolatedPixels = interpolatedFrame.image().rgba32fData();
    const float* animatedPixels = animatedFrame.image().rgba32fData();
    const float* fixedPixels = fixedFrame.image().rgba32fData();
    const float* interpolatedFixedPixels =
        interpolatedFixedFrame.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(interpolatedPixels, nullptr);
    ASSERT_NE(animatedPixels, nullptr);
    ASSERT_NE(fixedPixels, nullptr);
    ASSERT_NE(interpolatedFixedPixels, nullptr);
    std::size_t changedRgbSamples = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(firstPixels[offset], sourceValue);
            EXPECT_TRUE(std::isfinite(animatedPixels[offset]))
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(animatedPixels[offset], fixedPixels[offset])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(interpolatedPixels[offset],
                            interpolatedFixedPixels[offset])
                << "interpolated pixel=" << pixel << " channel=" << channel;
            if (animatedPixels[offset] != sourceValue) ++changedRgbSamples;
        }
        EXPECT_FLOAT_EQ(firstPixels[pixel * 4 + 3], alpha);
        EXPECT_FLOAT_EQ(interpolatedPixels[pixel * 4 + 3], alpha);
        EXPECT_FLOAT_EQ(animatedPixels[pixel * 4 + 3], alpha);
    }
    EXPECT_GT(changedRgbSamples, 0u);
}

TEST(LofiEffectContractTest, FilmGrungeAnimatesEmulsionStainAcrossCompositionFrames)
{
    constexpr int width = 23;
    constexpr int height = 17;
    constexpr float sourceValue = 0.4f;
    constexpr float alpha = 0.7f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "gateWeave", "vignette"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    auto* stain = effect.editableProperty(QStringLiteral("stain"));
    ASSERT_NE(stain, nullptr);
    stain->addKeyFrame(RationalTime(0, 30), 0.0);
    stain->addKeyFrame(RationalTime(30, 30), 0.4);

    ImageF32x4RGBAWithCache firstFrame;
    ImageF32x4RGBAWithCache interpolatedFrame;
    ImageF32x4RGBAWithCache animatedFrame;
    EffectContext context;
    context.compositionFrame = 0;
    effect.setContext(context);
    effect.applyCPUOnly(source, firstFrame);
    context.compositionFrame = 15;
    effect.setContext(context);
    effect.applyCPUOnly(source, interpolatedFrame);
    context.compositionFrame = 30;
    effect.setContext(context);
    effect.applyCPUOnly(source, animatedFrame);

    ArtifactFilmGrungeEffect fixedEffect;
    fixedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "gateWeave", "vignette"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    fixedEffect.setPropertyValue(UniString("stain"), QVariant(0.4f));
    ImageF32x4RGBAWithCache fixedFrame;
    fixedEffect.setContext(context);
    fixedEffect.applyCPUOnly(source, fixedFrame);

    context.compositionFrame = 15;
    fixedEffect.setContext(context);
    fixedEffect.setPropertyValue(UniString("stain"), QVariant(0.2f));
    ImageF32x4RGBAWithCache interpolatedFixedFrame;
    fixedEffect.applyCPUOnly(source, interpolatedFixedFrame);

    const float* firstPixels = firstFrame.image().rgba32fData();
    const float* interpolatedPixels = interpolatedFrame.image().rgba32fData();
    const float* animatedPixels = animatedFrame.image().rgba32fData();
    const float* fixedPixels = fixedFrame.image().rgba32fData();
    const float* interpolatedFixedPixels =
        interpolatedFixedFrame.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(interpolatedPixels, nullptr);
    ASSERT_NE(animatedPixels, nullptr);
    ASSERT_NE(fixedPixels, nullptr);
    ASSERT_NE(interpolatedFixedPixels, nullptr);
    std::size_t changedRgbSamples = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(firstPixels[offset], sourceValue);
            EXPECT_FLOAT_EQ(animatedPixels[offset], fixedPixels[offset])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(interpolatedPixels[offset],
                            interpolatedFixedPixels[offset])
                << "interpolated pixel=" << pixel << " channel=" << channel;
            if (animatedPixels[offset] != sourceValue) ++changedRgbSamples;
        }
        EXPECT_FLOAT_EQ(firstPixels[pixel * 4 + 3], alpha);
        EXPECT_FLOAT_EQ(interpolatedPixels[pixel * 4 + 3], alpha);
        EXPECT_FLOAT_EQ(animatedPixels[pixel * 4 + 3], alpha);
    }
    EXPECT_GT(changedRgbSamples, 0u);
}

TEST(LofiEffectContractTest, FilmGrungeAnimatesVignetteAcrossCompositionFrames)
{
    constexpr int width = 9;
    constexpr int height = 9;
    constexpr float sourceValue = 0.4f;
    constexpr float alpha = 0.7f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "gateWeave", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    auto* vignette = effect.editableProperty(QStringLiteral("vignette"));
    ASSERT_NE(vignette, nullptr);
    vignette->addKeyFrame(RationalTime(0, 30), 0.0);
    vignette->addKeyFrame(RationalTime(30, 30), 0.25);

    ImageF32x4RGBAWithCache firstFrame;
    ImageF32x4RGBAWithCache interpolatedFrame;
    ImageF32x4RGBAWithCache animatedFrame;
    EffectContext context;
    context.compositionFrame = 0;
    effect.setContext(context);
    effect.applyCPUOnly(source, firstFrame);
    context.compositionFrame = 15;
    effect.setContext(context);
    effect.applyCPUOnly(source, interpolatedFrame);
    context.compositionFrame = 30;
    effect.setContext(context);
    effect.applyCPUOnly(source, animatedFrame);

    ArtifactFilmGrungeEffect fixedEffect;
    fixedEffect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "gateWeave", "stain"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    fixedEffect.setPropertyValue(UniString("vignette"), QVariant(0.25f));
    ImageF32x4RGBAWithCache fixedFrame;
    fixedEffect.setContext(context);
    fixedEffect.applyCPUOnly(source, fixedFrame);

    context.compositionFrame = 15;
    fixedEffect.setContext(context);
    fixedEffect.setPropertyValue(UniString("vignette"), QVariant(0.125f));
    ImageF32x4RGBAWithCache interpolatedFixedFrame;
    fixedEffect.applyCPUOnly(source, interpolatedFixedFrame);

    const float* firstPixels = firstFrame.image().rgba32fData();
    const float* interpolatedPixels = interpolatedFrame.image().rgba32fData();
    const float* animatedPixels = animatedFrame.image().rgba32fData();
    const float* fixedPixels = fixedFrame.image().rgba32fData();
    const float* interpolatedFixedPixels =
        interpolatedFixedFrame.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(interpolatedPixels, nullptr);
    ASSERT_NE(animatedPixels, nullptr);
    ASSERT_NE(fixedPixels, nullptr);
    ASSERT_NE(interpolatedFixedPixels, nullptr);
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(firstPixels[offset], sourceValue);
            EXPECT_FLOAT_EQ(animatedPixels[offset], fixedPixels[offset])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(interpolatedPixels[offset],
                            interpolatedFixedPixels[offset])
                << "interpolated pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(animatedPixels[pixel * 4 + 3], alpha);
        EXPECT_FLOAT_EQ(interpolatedPixels[pixel * 4 + 3], alpha);
    }
    const std::size_t center =
        (static_cast<std::size_t>(height / 2) * width + width / 2) * 4;
    EXPECT_NEAR(animatedPixels[center], sourceValue, 1.0e-6f);
    EXPECT_NEAR(animatedPixels[0],
                sourceValue * (1.0f - 0.25f * std::pow(2.0f, 1.35f)),
                1.0e-6f);
    EXPECT_NEAR(interpolatedPixels[center], sourceValue, 1.0e-6f);
    EXPECT_NEAR(interpolatedPixels[0],
                sourceValue * (1.0f - 0.125f * std::pow(2.0f, 1.35f)),
                1.0e-6f);
}

TEST(LofiEffectContractTest,
     FilmGrungeFloatControlsClampBoundsAndNonFiniteValues)
{
    struct FloatControl {
        const char* name;
        float minimum;
        float maximum;
    };
    constexpr std::array<FloatControl, 8> controls{{
        {"grainAmount", 0.0f, 0.5f},
        {"grainSize", 1.0f, 8.0f},
        {"scratchAmount", 0.0f, 1.0f},
        {"dustAmount", 0.0f, 1.0f},
        {"flicker", 0.0f, 0.5f},
        {"gateWeave", 0.0f, 12.0f},
        {"vignette", 0.0f, 1.0f},
        {"stain", 0.0f, 0.5f},
    }};
    ArtifactFilmGrungeEffect effect;
    const auto actualValue = [&effect](const char* name) {
        const auto properties = effect.getProperties();
        const auto found = std::find_if(
            properties.begin(), properties.end(), [name](const auto& property) {
                return property.getName() == QString::fromUtf8(name);
            });
        return found == properties.end()
            ? std::numeric_limits<float>::quiet_NaN()
            : found->getValue().toFloat();
    };
    const std::array<double, 3> nonFiniteValues{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };

    for (const auto& control : controls) {
        SCOPED_TRACE(control.name);
        effect.setPropertyValue(
            UniString(control.name), QVariant(control.minimum - 1.0f));
        EXPECT_FLOAT_EQ(actualValue(control.name), control.minimum);
        effect.setPropertyValue(
            UniString(control.name), QVariant(control.maximum + 1.0f));
        EXPECT_FLOAT_EQ(actualValue(control.name), control.maximum);
        for (const double requested : nonFiniteValues) {
            effect.setPropertyValue(UniString(control.name), QVariant(requested));
            EXPECT_FLOAT_EQ(actualValue(control.name), control.minimum);
        }
    }
}

TEST(LofiEffectContractTest,
     FilmGrungeRoiHintTracksGateWeaveAndRequiresFullFrame)
{
    ArtifactFilmGrungeEffect effect;
    const auto expectRoiHint = [&effect](float expansion) {
        const auto hint = effect.roiHint();
        EXPECT_EQ(hint.kind, EffectROIHintKind::Displacement);
        EXPECT_FLOAT_EQ(hint.expansionPixels, expansion);
        EXPECT_FLOAT_EQ(hint.expansionFraction, 0.0f);
        EXPECT_TRUE(hint.requiresFullFrame);
    };

    expectRoiHint(3.0f);
    effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    expectRoiHint(1.0f);
    effect.setPropertyValue(UniString("gateWeave"), QVariant(0.5f));
    expectRoiHint(2.0f);
    effect.setPropertyValue(UniString("gateWeave"), QVariant(100.0f));
    expectRoiHint(13.0f);
    effect.setPropertyValue(UniString("seed"), QVariant(0));
    expectRoiHint(13.0f);
    effect.setPropertyValue(UniString("seed"), QVariant(100000));
    expectRoiHint(13.0f);
}

TEST(LofiEffectContractTest, FilmGrungePublishesStablePropertyContracts)
{
    struct PropertyContract {
        const char* name;
        const char* label;
        ArtifactCore::PropertyType type;
        double defaultValue;
        double minimum;
        double maximum;
        bool animatable;
    };
    constexpr std::array<PropertyContract, 9> contracts{{
        {"grainAmount", "Grain Amount", ArtifactCore::PropertyType::Float,
         0.075, 0.0, 0.5, true},
        {"grainSize", "Grain Size", ArtifactCore::PropertyType::Float,
         1.4, 1.0, 8.0, true},
        {"scratchAmount", "Scratches", ArtifactCore::PropertyType::Float,
         0.22, 0.0, 1.0, true},
        {"dustAmount", "Dust and Dirt", ArtifactCore::PropertyType::Float,
         0.18, 0.0, 1.0, true},
        {"flicker", "Exposure Flicker", ArtifactCore::PropertyType::Float,
         0.055, 0.0, 0.5, true},
        {"gateWeave", "Gate Weave", ArtifactCore::PropertyType::Float,
         1.2, 0.0, 12.0, true},
        {"vignette", "Vignette", ArtifactCore::PropertyType::Float,
         0.18, 0.0, 1.0, true},
        {"stain", "Emulsion Stain", ArtifactCore::PropertyType::Float,
         0.10, 0.0, 0.5, true},
        {"seed", "Random Seed", ArtifactCore::PropertyType::Integer,
         1977.0, 0.0, 100000.0, false},
    }};
    ArtifactFilmGrungeEffect effect;
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), contracts.size());
    for (const auto& contract : contracts) {
        SCOPED_TRACE(contract.name);
        const auto found = std::find_if(
            properties.begin(), properties.end(), [&contract](const auto& property) {
                return property.getName() == QString::fromUtf8(contract.name);
            });
        ASSERT_NE(found, properties.end());
        EXPECT_EQ(found->getType(), contract.type);
        EXPECT_NEAR(found->getValue().toDouble(), contract.defaultValue, 1.0e-6);
        EXPECT_NEAR(found->getDefaultValue().toDouble(), contract.defaultValue,
                    1.0e-6);
        EXPECT_EQ(found->isAnimatable(), contract.animatable);
        const auto metadata = found->metadata();
        EXPECT_EQ(metadata.displayLabel, QString::fromUtf8(contract.label));
        EXPECT_NEAR(metadata.hardMin.toDouble(), contract.minimum, 1.0e-6);
        EXPECT_NEAR(metadata.hardMax.toDouble(), contract.maximum, 1.0e-6);
    }
}

TEST(LofiEffectContractTest, CommonEditablePropertiesReflectOldTvState)
{
    ArtifactOldTVEffect effect;
    const auto findProperty = [&effect](const QString& name) {
        const auto properties = effect.editableProperties();
        const auto found = std::find_if(
            properties.begin(), properties.end(), [&name](const auto& property) {
                return property && property->getName() == name;
            });
        return found == properties.end()
            ? SharedPtr<AbstractProperty>{}
            : *found;
    };

    const auto defaultMix = findProperty(QStringLiteral("Effect Mix"));
    const auto defaultEnabled = findProperty(QStringLiteral("Effect Enabled"));
    const auto defaultRegion = findProperty(
        QStringLiteral("Effect Region Enabled"));
    const auto defaultOverscan = findProperty(QStringLiteral("Allow Overscan"));
    ASSERT_TRUE(defaultMix);
    ASSERT_TRUE(defaultEnabled);
    ASSERT_TRUE(defaultRegion);
    ASSERT_TRUE(defaultOverscan);
    EXPECT_DOUBLE_EQ(defaultMix->getValue().toDouble(), 1.0);
    EXPECT_TRUE(defaultEnabled->getValue().toBool());
    EXPECT_FALSE(defaultRegion->getValue().toBool());
    EXPECT_FALSE(defaultOverscan->getValue().toBool());

    effect.setMix(0.4f);
    effect.setEnabled(false);
    effect.setAllowOverscan(true);
    effect.setEffectRegion(QRectF(2.0, 3.0, 4.0, 5.0));

    EXPECT_NEAR(findProperty(QStringLiteral("Effect Mix"))->getValue().toDouble(),
                0.4, 1.0e-6);
    EXPECT_FALSE(findProperty(QStringLiteral("Effect Enabled"))->getValue().toBool());
    EXPECT_TRUE(findProperty(QStringLiteral("Effect Region Enabled"))->getValue().toBool());
    EXPECT_TRUE(findProperty(QStringLiteral("Allow Overscan"))->getValue().toBool());
    EXPECT_DOUBLE_EQ(findProperty(QStringLiteral("Effect Region X"))->getValue().toDouble(), 2.0);
    EXPECT_DOUBLE_EQ(findProperty(QStringLiteral("Effect Region Y"))->getValue().toDouble(), 3.0);
    EXPECT_DOUBLE_EQ(findProperty(QStringLiteral("Effect Region Width"))->getValue().toDouble(), 4.0);
    EXPECT_DOUBLE_EQ(findProperty(QStringLiteral("Effect Region Height"))->getValue().toDouble(), 5.0);
}

TEST(LofiEffectContractTest, AnimatedMixControlsOldTvCompositeAtCompositionFrames)
{
    const auto source = makePatternSurface();
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    const auto mix = effect.editableProperty(QStringLiteral("Effect Mix"));
    ASSERT_TRUE(mix);
    mix->addKeyFrame(RationalTime(0, 30), 0.0);
    mix->addKeyFrame(RationalTime(30, 30), 1.0);

    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 0;
    effect.setContext(context);
    EXPECT_FLOAT_EQ(effect.mix(), 0.0f);
    ImageF32x4RGBAWithCache atZero;
    effect.applyCPUOnly(source, atZero);
    const auto refreshedProperties = effect.editableProperties();
    EXPECT_TRUE(mix->hasKeyFrames());
    EXPECT_TRUE(std::find(refreshedProperties.begin(), refreshedProperties.end(), mix) !=
                refreshedProperties.end())
        << "refreshing editable properties must retain the animated Mix property";

    context.compositionFrame = 15;
    effect.setContext(context);
    EXPECT_FLOAT_EQ(effect.mix(), 0.5f);
    ImageF32x4RGBAWithCache atHalf;
    effect.applyCPUOnly(source, atHalf);

    context.compositionFrame = 30;
    effect.setContext(context);
    EXPECT_FLOAT_EQ(effect.mix(), 1.0f);
    ImageF32x4RGBAWithCache atOne;
    effect.applyCPUOnly(source, atOne);

    ArtifactOldTVEffect fullEffect;
    fullEffect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache expectedFullEffect;
    fullEffect.applyCPUOnly(source, expectedFullEffect);
    const float* sourcePixels = source.image().rgba32fData();
    const float* zeroMixPixels = atZero.image().rgba32fData();
    const float* halfMixPixels = atHalf.image().rgba32fData();
    const float* fullMixPixels = atOne.image().rgba32fData();
    const float* expectedPixels = expectedFullEffect.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(zeroMixPixels, nullptr);
    ASSERT_NE(halfMixPixels, nullptr);
    ASSERT_NE(fullMixPixels, nullptr);
    ASSERT_NE(expectedPixels, nullptr);
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(zeroMixPixels[channel], sourcePixels[channel])
            << "zero-mix channel=" << channel;
        EXPECT_NEAR(halfMixPixels[channel],
                    sourcePixels[channel] * 0.5f + expectedPixels[channel] * 0.5f,
                    1.0e-6f)
            << "half-mix channel=" << channel;
        EXPECT_FLOAT_EQ(fullMixPixels[channel], expectedPixels[channel])
            << "full-mix channel=" << channel;
    }
}

TEST(LofiEffectContractTest, AnimatedEnabledSwitchesOldTvBypassAtCompositionFrames)
{
    const auto source = makePatternSurface();
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    const auto enabled = effect.editableProperty(QStringLiteral("Effect Enabled"));
    ASSERT_TRUE(enabled);
    enabled->addKeyFrame(RationalTime(0, 30), false);
    enabled->addKeyFrame(RationalTime(30, 30), true);

    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 0;
    effect.setContext(context);
    EXPECT_FALSE(effect.isEnabled());
    ImageF32x4RGBAWithCache bypassed;
    effect.applyCPUOnly(source, bypassed);

    context.compositionFrame = 30;
    effect.setContext(context);
    EXPECT_TRUE(effect.isEnabled());
    ImageF32x4RGBAWithCache enabledOutput;
    effect.applyCPUOnly(source, enabledOutput);

    ArtifactOldTVEffect fullEffect;
    fullEffect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache expectedFullEffect;
    fullEffect.applyCPUOnly(source, expectedFullEffect);
    const float* sourcePixels = source.image().rgba32fData();
    const float* bypassedPixels = bypassed.image().rgba32fData();
    const float* actualEffectPixels = enabledOutput.image().rgba32fData();
    const float* expectedEffectPixels = expectedFullEffect.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(bypassedPixels, nullptr);
    ASSERT_NE(actualEffectPixels, nullptr);
    ASSERT_NE(expectedEffectPixels, nullptr);
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(bypassedPixels[channel], sourcePixels[channel])
            << "bypassed channel=" << channel;
        EXPECT_FLOAT_EQ(actualEffectPixels[channel], expectedEffectPixels[channel])
            << "enabled channel=" << channel;
    }
}

TEST(LofiEffectContractTest, AnimatedRegionEnabledTogglesWithoutCoordinateTracks)
{
    ArtifactOldTVEffect effect;
    const auto enabled =
        effect.editableProperty(QStringLiteral("Effect Region Enabled"));
    ASSERT_TRUE(enabled);
    enabled->addKeyFrame(RationalTime(0, 30), true);
    enabled->addKeyFrame(RationalTime(30, 30), false);

    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 0;
    effect.setContext(context);
    EXPECT_TRUE(effect.hasEffectRegion());

    context.compositionFrame = 30;
    effect.setContext(context);
    EXPECT_FALSE(effect.hasEffectRegion());
}

TEST(LofiEffectContractTest, AnimatedRegionCoordinatesMoveThePixelComposite)
{
    constexpr int width = 8;
    constexpr int height = 8;
    const auto source = makePatternSurface();
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    const auto enabled =
        effect.editableProperty(QStringLiteral("Effect Region Enabled"));
    const auto regionX = effect.editableProperty(QStringLiteral("Effect Region X"));
    const auto regionY = effect.editableProperty(QStringLiteral("Effect Region Y"));
    const auto regionWidth =
        effect.editableProperty(QStringLiteral("Effect Region Width"));
    const auto regionHeight =
        effect.editableProperty(QStringLiteral("Effect Region Height"));
    ASSERT_TRUE(enabled);
    ASSERT_TRUE(regionX);
    ASSERT_TRUE(regionY);
    ASSERT_TRUE(regionWidth);
    ASSERT_TRUE(regionHeight);
    enabled->addKeyFrame(RationalTime(0, 30), true);
    enabled->addKeyFrame(RationalTime(30, 30), true);
    regionX->addKeyFrame(RationalTime(0, 30), 2.0);
    regionX->addKeyFrame(RationalTime(30, 30), 3.0);
    regionY->addKeyFrame(RationalTime(0, 30), 1.0);
    regionY->addKeyFrame(RationalTime(30, 30), 2.0);
    regionWidth->addKeyFrame(RationalTime(0, 30), 3.0);
    regionWidth->addKeyFrame(RationalTime(30, 30), 4.0);
    regionHeight->addKeyFrame(RationalTime(0, 30), 2.0);
    regionHeight->addKeyFrame(RationalTime(30, 30), 3.0);

    ArtifactOldTVEffect fullEffect;
    fullEffect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache fullEffectOutput;
    fullEffect.applyCPUOnly(source, fullEffectOutput);
    const float* sourcePixels = source.image().rgba32fData();
    const float* fullPixels = fullEffectOutput.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(fullPixels, nullptr);

    const auto expectRegionOutput = [&](std::int64_t frame,
                                        const QRectF& expectedRegion) {
        EffectContext context;
        context.frameRate = 30.0;
        context.compositionFrame = frame;
        effect.setContext(context);
        ASSERT_TRUE(effect.hasEffectRegion());
        EXPECT_EQ(effect.effectRegion(), expectedRegion);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        const float* actual = output.image().rgba32fData();
        ASSERT_NE(actual, nullptr);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const bool inside = expectedRegion.contains(
                    QPointF(static_cast<qreal>(x) + 0.5,
                            static_cast<qreal>(y) + 0.5));
                const std::size_t offset =
                    static_cast<std::size_t>((y * width + x) * 4);
                for (std::size_t channel = 0; channel < 4; ++channel) {
                    const float expected = inside
                        ? fullPixels[offset + channel]
                        : sourcePixels[offset + channel];
                    EXPECT_FLOAT_EQ(actual[offset + channel], expected)
                        << "frame=" << frame << " x=" << x << " y=" << y
                        << " channel=" << channel;
                }
            }
        }
    };

    expectRegionOutput(0, QRectF(2.0, 1.0, 3.0, 2.0));
    expectRegionOutput(15, QRectF(2.5, 1.5, 3.5, 2.5));
    expectRegionOutput(30, QRectF(3.0, 2.0, 4.0, 3.0));
}

TEST(LofiEffectContractTest, AllowOverscanDoesNotExpandOldTvOutput)
{
    const auto source = makePatternSurface();
    ImageF32x4RGBAWithCache standardOutput;
    ImageF32x4RGBAWithCache overscanOutput;
    ArtifactOldTVEffect standardEffect;
    standardEffect.setComputeMode(ComputeMode::CPU);
    standardEffect.applyCPUOnly(source, standardOutput);

    ArtifactOldTVEffect overscanEffect;
    overscanEffect.setComputeMode(ComputeMode::CPU);
    overscanEffect.setAllowOverscan(true);
    ASSERT_TRUE(overscanEffect.allowOverscan());
    overscanEffect.applyCPUOnly(source, overscanOutput);

    EXPECT_EQ(overscanOutput.image().width(), source.image().width());
    EXPECT_EQ(overscanOutput.image().height(), source.image().height());
    EXPECT_EQ(overscanOutput.image().width(), standardOutput.image().width());
    EXPECT_EQ(overscanOutput.image().height(), standardOutput.image().height());
    const float* standardPixels = standardOutput.image().rgba32fData();
    const float* overscanPixels = overscanOutput.image().rgba32fData();
    ASSERT_NE(standardPixels, nullptr);
    ASSERT_NE(overscanPixels, nullptr);
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(overscanPixels[channel], standardPixels[channel])
            << "channel=" << channel;
    }
}

TEST(LofiEffectContractTest, AnimatedRegionDisableOverridesAnimatedCoordinates)
{
    const auto source = makePatternSurface();
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    const auto regionEnabled =
        effect.editableProperty(QStringLiteral("Effect Region Enabled"));
    const auto regionX = effect.editableProperty(QStringLiteral("Effect Region X"));
    const auto regionY = effect.editableProperty(QStringLiteral("Effect Region Y"));
    const auto regionWidth =
        effect.editableProperty(QStringLiteral("Effect Region Width"));
    const auto regionHeight =
        effect.editableProperty(QStringLiteral("Effect Region Height"));
    ASSERT_TRUE(regionEnabled);
    ASSERT_TRUE(regionX);
    ASSERT_TRUE(regionY);
    ASSERT_TRUE(regionWidth);
    ASSERT_TRUE(regionHeight);

    regionEnabled->addKeyFrame(RationalTime(0, 30), true);
    regionEnabled->addKeyFrame(RationalTime(30, 30), false);
    regionX->addKeyFrame(RationalTime(0, 30), 2.0);
    regionX->addKeyFrame(RationalTime(30, 30), 3.0);
    regionY->addKeyFrame(RationalTime(0, 30), 1.0);
    regionY->addKeyFrame(RationalTime(30, 30), 2.0);
    regionWidth->addKeyFrame(RationalTime(0, 30), 3.0);
    regionWidth->addKeyFrame(RationalTime(30, 30), 4.0);
    regionHeight->addKeyFrame(RationalTime(0, 30), 2.0);
    regionHeight->addKeyFrame(RationalTime(30, 30), 3.0);

    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 0;
    effect.setContext(context);
    EXPECT_TRUE(effect.hasEffectRegion());
    EXPECT_EQ(effect.effectRegion(), QRectF(2.0, 1.0, 3.0, 2.0));

    context.compositionFrame = 30;
    effect.setContext(context);
    EXPECT_FALSE(effect.hasEffectRegion())
        << "animated region coordinates must not override Region Enabled=false";

    ImageF32x4RGBAWithCache disabledRegionOutput;
    effect.applyCPUOnly(source, disabledRegionOutput);
    ArtifactOldTVEffect fullEffect;
    fullEffect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache fullEffectOutput;
    fullEffect.applyCPUOnly(source, fullEffectOutput);
    const float* actual = disabledRegionOutput.image().rgba32fData();
    const float* expected = fullEffectOutput.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel])
            << "channel=" << channel;
    }
}

TEST(LofiEffectContractTest, OldTvEmptyInputClearsPreviouslyPopulatedOutput)
{
    const ImageF32x4RGBAWithCache source;
    ImageF32x4RGBAWithCache output = makePatternSurface();
    ASSERT_FALSE(output.image().isEmpty());
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.apply(source, output);

    EXPECT_TRUE(output.image().isEmpty());
    EXPECT_EQ(output.image().width(), 0);
    EXPECT_EQ(output.image().height(), 0);
}

TEST(LofiEffectContractTest, DisabledOldTvIsAnExactPassthrough)
{
    constexpr int width = 4;
    constexpr int height = 3;
    constexpr std::array<float, width * height * 4> pixels{
        0.1f, 0.2f, 0.3f, 0.4f,
        0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 1.0f, 0.0f, 1.2f,
        -0.1f, 0.25f, 0.75f, -0.3f,
        0.3f, 0.2f, 0.1f, 0.0f,
        0.7f, 0.8f, 0.9f, 0.6f,
        0.15f, 0.35f, 0.55f, 0.75f,
        0.25f, 0.45f, 0.65f, 0.85f,
        0.95f, 0.05f, 0.4f, 0.2f,
        0.33f, 0.66f, 0.99f, 0.12f,
        0.42f, 0.24f, 0.18f, 0.36f,
        0.8f, 0.4f, 0.2f, 0.1f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setEnabled(false);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    ASSERT_NE(sourcePixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel])
            << "output channel=" << channel;
        EXPECT_FLOAT_EQ(sourcePixels[channel], pixels[channel])
            << "source channel=" << channel;
    }
}

TEST(LofiEffectContractTest, OldTvHandlesSinglePixelWideImages)
{
    constexpr int width = 1;
    constexpr int height = 5;
    constexpr std::array<float, width * height * 4> pixels{
        0.2f, 0.4f, 0.6f, 0.1f,
        0.3f, 0.5f, 0.7f, 0.3f,
        0.4f, 0.6f, 0.8f, 0.5f,
        0.5f, 0.7f, 0.9f, 0.7f,
        0.6f, 0.8f, 1.0f, 0.9f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;

    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(height);
         ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(result[pixel * 4 + channel]))
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_GE(result[pixel * 4 + channel], 0.0f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_LE(result[pixel * 4 + channel], 1.0f)
                << "pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "pixel=" << pixel;
    }
}

TEST(LofiEffectContractTest, OldTvHandlesSinglePixelHighImagesDeterministically)
{
    constexpr int width = 5;
    constexpr int height = 1;
    constexpr std::array<float, width * height * 4> pixels{
        0.2f, 0.4f, 0.6f, 0.1f,
        0.3f, 0.5f, 0.7f, 0.3f,
        0.4f, 0.6f, 0.8f, 0.5f,
        0.5f, 0.7f, 0.9f, 0.7f,
        0.6f, 0.8f, 1.0f, 0.9f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache first;
    ImageF32x4RGBAWithCache repeated;

    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, first);
    effect.applyCPUOnly(source, repeated);

    ASSERT_EQ(first.image().width(), width);
    ASSERT_EQ(first.image().height(), height);
    const float* result = first.image().rgba32fData();
    const float* repeatedResult = repeated.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    ASSERT_NE(repeatedResult, nullptr);
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width); ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(result[pixel * 4 + channel]))
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_GE(result[pixel * 4 + channel], 0.0f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_LE(result[pixel * 4 + channel], 1.0f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(result[pixel * 4 + channel],
                            repeatedResult[pixel * 4 + channel])
                << "pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3]);
    }
}

TEST(LofiEffectContractTest, OldTvPreservesAlphaOnSinglePixelImages)
{
    constexpr std::array<float, 4> pixels{0.35f, 0.65f, 0.9f, 0.42f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 1);
    ASSERT_EQ(output.image().height(), 1);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_TRUE(std::isfinite(result[channel])) << "channel=" << channel;
        EXPECT_GE(result[channel], 0.0f) << "channel=" << channel;
        EXPECT_LE(result[channel], 1.0f) << "channel=" << channel;
    }
    EXPECT_FLOAT_EQ(result[3], pixels[3]);
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
            const float noise = output[offset] - baseBgra[0] * scanline;
            EXPECT_GE(noise, -0.005f) << "x=" << x << " y=" << y;
            EXPECT_LE(noise, 0.005f) << "x=" << x << " y=" << y;
            for (int channel = 0; channel < 3; ++channel) {
                EXPECT_NEAR(output[offset + channel],
                            baseBgra[channel] * scanline + noise, 1.0e-6f)
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

TEST(LofiEffectContractTest,
     OldTvAppliesDeterministicClampedHorizontalJitterPerScanline)
{
    constexpr int width = 64;
    constexpr int height = 64;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float ramp = 0.1f + static_cast<float>(x) * 0.005f;
            pixels[offset + 0] = ramp;
            pixels[offset + 1] = ramp * 2.0f;
            pixels[offset + 2] = ramp * 3.0f;
            pixels[offset + 3] = 0.15f + static_cast<float>(x) * 0.01f +
                                 static_cast<float>(y) * 0.001f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache first;
    ImageF32x4RGBAWithCache repeated;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, first);
    effect.applyCPUOnly(source, repeated);

    const float* result = first.image().rgba32fData();
    const float* repeatedResult = repeated.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    ASSERT_NE(repeatedResult, nullptr);
    bool observedJitter = false;
    for (int y = 0; y < height; ++y) {
        const float scanline = y % 4 == 0 ? 0.7f : 1.0f;
        constexpr int referenceX = 6;
        const std::size_t referenceOffset =
            static_cast<std::size_t>((y * width + referenceX) * 4);
        const float referenceDelta =
            result[referenceOffset + 1] - result[referenceOffset];
        const int referenceSourceX = static_cast<int>(std::lround(
            referenceDelta / (0.005f * scanline)));
        const int rowShift = referenceSourceX - referenceX;
        ASSERT_LE(std::abs(rowShift), 4) << "y=" << y;
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float redGreenDelta = result[offset + 1] - result[offset];
            const int sourceX = static_cast<int>(std::lround(
                redGreenDelta / (0.005f * scanline)));
            ASSERT_GE(sourceX, 0) << "x=" << x << " y=" << y;
            ASSERT_LT(sourceX, width) << "x=" << x << " y=" << y;
            const int shift = sourceX - x;
            EXPECT_EQ(shift, rowShift) << "x=" << x << " y=" << y;
            EXPECT_EQ(sourceX, std::clamp(x + rowShift, 0, width - 1))
                << "x=" << x << " y=" << y;
            EXPECT_LE(std::abs(rowShift), 5) << "y=" << y;
            const std::size_t sourceOffset =
                static_cast<std::size_t>((y * width + sourceX) * 4);
            EXPECT_FLOAT_EQ(result[offset + 3], pixels[sourceOffset + 3])
                << "x=" << x << " y=" << y << " sourceX=" << sourceX;
            for (std::size_t channel = 0; channel < 4; ++channel) {
                EXPECT_FLOAT_EQ(result[offset + channel],
                                repeatedResult[offset + channel])
                    << "x=" << x << " y=" << y
                    << " channel=" << channel;
            }
        }
        observedJitter = observedJitter || rowShift != 0;
    }
    EXPECT_TRUE(observedJitter);
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

TEST(LofiEffectContractTest, OldTvAddsSharedNoiseWithoutChangingRgbChannelDifferences)
{
    constexpr int width = 7;
    constexpr int height = 5;
    constexpr std::array<float, 3> baseRgb{0.2f, 0.4f, 0.6f};
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = baseRgb[0];
            pixels[offset + 1] = baseRgb[1];
            pixels[offset + 2] = baseRgb[2];
            pixels[offset + 3] = 0.75f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (int y = 0; y < height; ++y) {
        const float scanline = y % 4 == 0 ? 0.7f : 1.0f;
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            EXPECT_NEAR(result[offset + 1] - result[offset + 0],
                        (baseRgb[1] - baseRgb[0]) * scanline, 1.0e-6f)
                << "x=" << x << " y=" << y << " R-G";
            EXPECT_NEAR(result[offset + 2] - result[offset + 1],
                        (baseRgb[2] - baseRgb[1]) * scanline, 1.0e-6f)
                << "x=" << x << " y=" << y << " G-B";
            EXPECT_FLOAT_EQ(result[offset + 3], 0.75f)
                << "x=" << x << " y=" << y;
        }
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

TEST(LofiEffectContractTest, OldTvMixClampsFiniteValuesAndResetsNonFiniteValues)
{
    constexpr int width = 5;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>(x + 1) / 8.0f;
            pixels[offset + 1] = static_cast<float>(y + 2) / 7.0f;
            pixels[offset + 2] = static_cast<float>(x + y + 1) / 9.0f;
            pixels[offset + 3] = static_cast<float>(x + 2 * y + 1) / 12.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache fullEffect;
    ArtifactOldTVEffect fullEffectInstance;
    fullEffectInstance.setComputeMode(ComputeMode::CPU);
    fullEffectInstance.applyCPUOnly(source, fullEffect);
    const float* fullPixels = fullEffect.image().rgba32fData();
    ASSERT_NE(fullPixels, nullptr);

    struct MixCase {
        float requested;
        float effective;
    };
    const std::array<MixCase, 8> cases{{
        {-2.0f, 0.0f},
        {0.0f, 0.0f},
        {0.35f, 0.35f},
        {1.0f, 1.0f},
        {3.0f, 1.0f},
        {std::numeric_limits<float>::quiet_NaN(), 1.0f},
        {std::numeric_limits<float>::infinity(), 1.0f},
        {-std::numeric_limits<float>::infinity(), 1.0f},
    }};

    for (const auto& mixCase : cases) {
        SCOPED_TRACE(mixCase.requested);
        ArtifactOldTVEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setMix(mixCase.requested);
        EXPECT_FLOAT_EQ(effect.mix(), mixCase.effective);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        const float* result = output.image().rgba32fData();
        ASSERT_NE(result, nullptr);
        for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
            const float expected = pixels[channel] * (1.0f - mixCase.effective) +
                                   fullPixels[channel] * mixCase.effective;
            EXPECT_NEAR(result[channel], expected, 1.0e-6f)
                << "channel=" << channel;
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
    ArtifactOldTVEffect fullEffectInstance;
    fullEffectInstance.setComputeMode(ComputeMode::CPU);
    fullEffectInstance.applyCPUOnly(source, fullEffect);
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setMix(0.5f);
    effect.setEffectRegion(QRectF(2.0, 1.0, 2.0, 2.0));
    effect.applyCPUOnly(source, regionalEffect);

    ASSERT_EQ(fullEffect.image().width(), width);
    ASSERT_EQ(fullEffect.image().height(), height);
    ASSERT_EQ(regionalEffect.image().width(), width);
    ASSERT_EQ(regionalEffect.image().height(), height);
    const float* fullEffectPixels = fullEffect.image().rgba32fData();
    const float* regionalPixels = regionalEffect.image().rgba32fData();
    const float* unchangedSource = source.image().rgba32fData();
    ASSERT_NE(fullEffectPixels, nullptr);
    ASSERT_NE(regionalPixels, nullptr);
    ASSERT_NE(unchangedSource, nullptr);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool inside = x >= 2 && x < 4 && y >= 1 && y < 3;
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const float expected = inside
                    ? pixels[offset + channel] * 0.5f +
                          fullEffectPixels[offset + channel] * 0.5f
                    : pixels[offset + channel];
                EXPECT_NEAR(regionalPixels[offset + channel], expected, 1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
                EXPECT_FLOAT_EQ(unchangedSource[offset + channel], pixels[offset + channel])
                    << "source x=" << x << " y=" << y << " channel=" << channel;
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

TEST(LofiEffectContractTest, OldTvRegionOutsideImageLeavesSourceUntouched)
{
    constexpr int width = 5;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>(x + 1) / 7.0f;
            pixels[offset + 1] = static_cast<float>(y + 1) / 6.0f;
            pixels[offset + 2] = static_cast<float>(x + y + 1) / 10.0f;
            pixels[offset + 3] = static_cast<float>(x + 2 * y + 1) / 16.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setEffectRegion(QRectF(width + 2.0, height + 1.0, 3.0, 2.0));
    ASSERT_TRUE(effect.hasEffectRegion());

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

TEST(LofiEffectContractTest, InvalidEffectRegionClearsPreviousRegion)
{
    const auto source = makePatternSurface();
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache restoredEffect;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, fullEffect);

    effect.setEffectRegion(QRectF(2.0, 2.0, 2.0, 2.0));
    ASSERT_TRUE(effect.hasEffectRegion());
    effect.setEffectRegion(QRectF(2.0, 2.0, 0.0, 2.0));
    EXPECT_FALSE(effect.hasEffectRegion());
    effect.applyCPUOnly(source, restoredEffect);

    const float* expected = fullEffect.image().rgba32fData();
    const float* actual = restoredEffect.image().rgba32fData();
    ASSERT_NE(expected, nullptr);
    ASSERT_NE(actual, nullptr);
    ASSERT_EQ(restoredEffect.image().width(), fullEffect.image().width());
    ASSERT_EQ(restoredEffect.image().height(), fullEffect.image().height());
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel])
            << "channel=" << channel;
    }
}
