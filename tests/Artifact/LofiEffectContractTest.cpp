#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Artifact.Effect.Context;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Render.CompositionViewDrawing;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Memory.SharedPtr;
import Composition3D;
import Color.Float;
import Time.Rational;
import Utils.Id;

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

TEST(LofiEffectContractTest,
     GlitchCpuOutputDoesNotDependOnCompositionFrame)
{
    constexpr int width = 23;
    constexpr int height = 16;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = 0.08f + 0.012f * x + 0.001f * y;
            pixels[offset + 1] = 0.16f + 0.009f * x + 0.002f * y;
            pixels[offset + 2] = 0.24f + 0.006f * x + 0.003f * y;
            pixels[offset + 3] = 0.2f + 0.01f * ((x + y) % 10);
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactGlitchEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    EffectContext context;
    context.compositionFrame = -13;
    effect.setContext(context);
    ImageF32x4RGBAWithCache firstFrame;
    effect.applyCPUOnly(source, firstFrame);
    context.compositionFrame = 208;
    effect.setContext(context);
    ImageF32x4RGBAWithCache laterFrame;
    effect.applyCPUOnly(source, laterFrame);

    const float* first = firstFrame.image().rgba32fData();
    const float* later = laterFrame.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(later, nullptr);
    ASSERT_EQ(firstFrame.width(), width);
    ASSERT_EQ(firstFrame.height(), height);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(first[channel], later[channel])
            << "channel=" << channel;
    }
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
     GlitchCpuCombinesSymmetricColorOffsetsAroundCenterAlpha)
{
    constexpr int width = 64;
    constexpr int height = 15;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = 0.1f + 0.003f * x + 0.0002f * y;
            pixels[offset + 1] = 0.2f + 0.004f * x + 0.0003f * y;
            pixels[offset + 2] = 0.3f + 0.005f * x + 0.0004f * y;
            pixels[offset + 3] =
                x % 3 == 0 ? -0.5f : x % 3 == 1 ? 0.4f : 1.5f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactGlitchEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (int y = 0; y < height; ++y) {
        const int referenceX = width / 2;
        const std::size_t referenceOffset =
            static_cast<std::size_t>((y * width + referenceX) * 4);
        const float referenceCenter =
            (result[referenceOffset + 1] - 0.2f - 0.0003f * y) / 0.004f;
        const int referenceSourceX = static_cast<int>(std::lround(referenceCenter));
        const int rowOffset = referenceSourceX - referenceX;
        const float referenceRight =
            (result[referenceOffset + 2] - 0.3f - 0.0004f * y) / 0.005f;
        const int referenceRightX = static_cast<int>(std::lround(referenceRight));
        const int chromaOffset = referenceRightX - referenceSourceX;
        ASSERT_GE(chromaOffset, 3) << "y=" << y;
        ASSERT_LE(chromaOffset, 7) << "y=" << y;

        for (int x = 0; x < width; ++x) {
            const int centerX = std::clamp(x + rowOffset, 0, width - 1);
            const int leftX = std::clamp(centerX - chromaOffset, 0, width - 1);
            const int rightX = std::clamp(centerX + chromaOffset, 0, width - 1);
            const std::size_t outputOffset =
                static_cast<std::size_t>((y * width + x) * 4);
            const std::size_t centerOffset =
                static_cast<std::size_t>((y * width + centerX) * 4);
            const std::size_t leftOffset =
                static_cast<std::size_t>((y * width + leftX) * 4);
            const std::size_t rightOffset =
                static_cast<std::size_t>((y * width + rightX) * 4);
            EXPECT_NEAR(result[outputOffset + 0], pixels[leftOffset + 0], 1.0e-6f)
                << "left color sample x=" << x << " y=" << y;
            EXPECT_NEAR(result[outputOffset + 1], pixels[centerOffset + 1], 1.0e-6f)
                << "center green sample x=" << x << " y=" << y;
            EXPECT_NEAR(result[outputOffset + 2], pixels[rightOffset + 2], 1.0e-6f)
                << "right color sample x=" << x << " y=" << y;
            EXPECT_FLOAT_EQ(result[outputOffset + 3], pixels[centerOffset + 3])
                << "center alpha sample x=" << x << " y=" << y;
        }
    }
}

TEST(LofiEffectContractTest,
     GlitchCpuPreservesColorDescriptorAndIndependentPixelStorage)
{
    constexpr int width = 19;
    constexpr int height = 11;
    constexpr std::size_t channelCount =
        static_cast<std::size_t>(width) * height * 4;
    std::array<float, channelCount> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = 0.08f + 0.01f * x;
            pixels[offset + 1] = 0.12f + 0.015f * y;
            pixels[offset + 2] = 0.2f + 0.005f * (x + y);
            pixels[offset + 3] = 0.1f + 0.01f * ((x + y) % 20);
        }
    }
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float(
        SurfaceColorPrimaries::DisplayP3_D65);
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    image.setColorDescriptor(descriptor);
    ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactGlitchEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.width(), width);
    ASSERT_EQ(output.height(), height);
    EXPECT_EQ(source.image().colorDescriptor(), descriptor);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
    float* sourcePixels = source.image().rgba32fData();
    float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(outputPixels, nullptr);
    ASSERT_NE(sourcePixels, outputPixels);
    std::array<float, channelCount> outputSnapshot{};
    std::copy_n(outputPixels, channelCount, outputSnapshot.begin());

    std::fill_n(sourcePixels, channelCount, -7.0f);
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(outputPixels[channel], outputSnapshot[channel])
            << "source mutation reached output channel=" << channel;
    }
    outputPixels[0] = 23.0f;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(sourcePixels[channel], -7.0f)
            << "output mutation reached source channel=" << channel;
    }
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
     FilmGrungeZeroStrengthControlsPreserveNegativeAndHdrInputExactly)
{
    constexpr int width = 7;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        pixels[channel] = static_cast<float>(channel % 9) * 0.5f - 1.0f;
    }
    EXPECT_LT(*std::min_element(pixels.begin(), pixels.end()), 0.0f);
    EXPECT_GT(*std::max_element(pixels.begin(), pixels.end()), 1.0f);
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

TEST(LofiEffectContractTest, FilmGrungeActiveGrainPreservesOutOfRangeAlpha)
{
    constexpr int width = 24;
    constexpr int height = 12;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>((x * 3 + y) % 17) / 16.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 2) % 13) / 12.0f;
            pixels[offset + 2] = static_cast<float>((x * 5 + y * 3) % 19) / 18.0f;
            pixels[offset + 3] =
                x % 3 == 0 ? -0.5f : x % 3 == 1 ? 0.4f : 1.5f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.3f));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(1234));

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    bool changedRgb = false;
    bool observedNegativeAlpha = false;
    bool observedAboveOneAlpha = false;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width * height); ++pixel) {
        const std::size_t offset = pixel * 4;
        for (int channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(actual[offset + channel]))
                << "pixel=" << pixel << " channel=" << channel;
            changedRgb = changedRgb ||
                std::abs(actual[offset + channel] - pixels[offset + channel]) >
                    1.0e-5f;
        }
        EXPECT_FLOAT_EQ(actual[offset + 3], pixels[offset + 3])
            << "pixel=" << pixel;
        observedNegativeAlpha = observedNegativeAlpha || pixels[offset + 3] < 0.0f;
        observedAboveOneAlpha = observedAboveOneAlpha || pixels[offset + 3] > 1.0f;
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_TRUE(observedNegativeAlpha);
    EXPECT_TRUE(observedAboveOneAlpha);
}

TEST(LofiEffectContractTest, FilmGrungeMixInterpolatesSourceAndEffectPixels)
{
    constexpr int width = 48;
    constexpr int height = 32;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>((x * 3 + y) % 23) / 22.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 5) % 19) / 18.0f;
            pixels[offset + 2] = static_cast<float>((x * 7 + y * 2) % 29) / 28.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 3) % 11) / 10.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.35f));
    effect.setPropertyValue(UniString("scratchAmount"), QVariant(0.8f));
    effect.setPropertyValue(UniString("dustAmount"), QVariant(0.7f));
    effect.setPropertyValue(UniString("seed"), QVariant(129));
    EffectContext context;
    context.compositionFrame = 12;
    effect.setContext(context);

    ImageF32x4RGBAWithCache fullMix;
    ImageF32x4RGBAWithCache halfMix;
    ImageF32x4RGBAWithCache zeroMix;
    effect.setMix(1.0f);
    effect.applyCPUOnly(source, fullMix);
    effect.setMix(0.5f);
    effect.applyCPUOnly(source, halfMix);
    effect.setMix(0.0f);
    effect.applyCPUOnly(source, zeroMix);

    const float* full = fullMix.image().rgba32fData();
    const float* half = halfMix.image().rgba32fData();
    const float* zero = zeroMix.image().rgba32fData();
    ASSERT_NE(full, nullptr);
    ASSERT_NE(half, nullptr);
    ASSERT_NE(zero, nullptr);
    bool effectChangedRgb = false;
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(zero[channel], pixels[channel])
            << "zero mix channel=" << channel;
        EXPECT_NEAR(half[channel],
                    pixels[channel] * 0.5f + full[channel] * 0.5f,
                    1.0e-6f)
            << "half mix channel=" << channel;
        if (channel % 4 != 3) {
            effectChangedRgb = effectChangedRgb ||
                std::abs(full[channel] - pixels[channel]) > 1.0e-5f;
        }
    }
    EXPECT_TRUE(effectChangedRgb);
}

TEST(LofiEffectContractTest,
     FilmGrungeAnimatedMixInterpolatesAtCompositionFrames)
{
    const auto source = makePatternSurface();
    ArtifactFilmGrungeEffect effect;
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
    ImageF32x4RGBAWithCache zeroMix;
    effect.applyCPUOnly(source, zeroMix);

    context.compositionFrame = 15;
    effect.setContext(context);
    EXPECT_FLOAT_EQ(effect.mix(), 0.5f);
    ImageF32x4RGBAWithCache halfMix;
    effect.applyCPUOnly(source, halfMix);
    const auto refreshedProperties = effect.editableProperties();
    EXPECT_TRUE(mix->hasKeyFrames());
    EXPECT_TRUE(std::find(refreshedProperties.begin(), refreshedProperties.end(), mix) !=
                refreshedProperties.end())
        << "refreshing editable properties must retain animated FilmGrunge Mix";

    context.compositionFrame = 30;
    effect.setContext(context);
    EXPECT_FLOAT_EQ(effect.mix(), 1.0f);
    ImageF32x4RGBAWithCache fullMix;
    effect.applyCPUOnly(source, fullMix);

    ArtifactFilmGrungeEffect fullAtHalfFrameEffect;
    fullAtHalfFrameEffect.setComputeMode(ComputeMode::CPU);
    context.compositionFrame = 15;
    fullAtHalfFrameEffect.setContext(context);
    ImageF32x4RGBAWithCache expectedHalfEffect;
    fullAtHalfFrameEffect.applyCPUOnly(source, expectedHalfEffect);

    ArtifactFilmGrungeEffect fullAtEndFrameEffect;
    fullAtEndFrameEffect.setComputeMode(ComputeMode::CPU);
    context.compositionFrame = 30;
    fullAtEndFrameEffect.setContext(context);
    ImageF32x4RGBAWithCache expectedFullEffect;
    fullAtEndFrameEffect.applyCPUOnly(source, expectedFullEffect);

    const float* sourcePixels = source.image().rgba32fData();
    const float* zeroPixels = zeroMix.image().rgba32fData();
    const float* halfPixels = halfMix.image().rgba32fData();
    const float* fullPixels = fullMix.image().rgba32fData();
    const float* expectedHalfPixels = expectedHalfEffect.image().rgba32fData();
    const float* expectedFullPixels = expectedFullEffect.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(zeroPixels, nullptr);
    ASSERT_NE(halfPixels, nullptr);
    ASSERT_NE(fullPixels, nullptr);
    ASSERT_NE(expectedHalfPixels, nullptr);
    ASSERT_NE(expectedFullPixels, nullptr);
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(zeroPixels[channel], sourcePixels[channel])
            << "zero-mix channel=" << channel;
        EXPECT_NEAR(halfPixels[channel],
                    sourcePixels[channel] * 0.5f +
                        expectedHalfPixels[channel] * 0.5f,
                    1.0e-6f)
            << "half-mix channel=" << channel;
        EXPECT_FLOAT_EQ(fullPixels[channel], expectedFullPixels[channel])
            << "full-mix channel=" << channel;
    }
}

TEST(LofiEffectContractTest,
     FilmGrungeMixClampsFiniteValuesAndResetsNonFiniteValues)
{
    constexpr int width = 12;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>((x * 3 + y) % 17) / 16.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 2) % 13) / 12.0f;
            pixels[offset + 2] = static_cast<float>((x * 5 + y * 3) % 19) / 18.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 3) % 11) / 10.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect fullEffect;
    fullEffect.setComputeMode(ComputeMode::CPU);
    fullEffect.setPropertyValue(UniString("seed"), QVariant(417));
    EffectContext context;
    context.compositionFrame = 11;
    fullEffect.setContext(context);
    ImageF32x4RGBAWithCache full;
    fullEffect.applyCPUOnly(source, full);
    const float* fullPixels = full.image().rgba32fData();
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
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setPropertyValue(UniString("seed"), QVariant(417));
        effect.setContext(context);
        effect.setMix(mixCase.requested);
        EXPECT_FLOAT_EQ(effect.mix(), mixCase.effective);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        const float* actual = output.image().rgba32fData();
        ASSERT_NE(actual, nullptr);
        for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
            const float expected = pixels[channel] * (1.0f - mixCase.effective) +
                fullPixels[channel] * mixCase.effective;
            EXPECT_NEAR(actual[channel], expected, 1.0e-6f)
                << "channel=" << channel;
        }
    }
}

TEST(LofiEffectContractTest, FilmGrungeEffectRegionLimitsGrainToPixelCenters)
{
    constexpr int width = 32;
    constexpr int height = 20;
    const QRectF region(8.0, 5.0, 12.0, 8.0);
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width * height); ++pixel) {
        pixels[pixel * 4 + 0] = 0.25f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.75f;
        pixels[pixel * 4 + 3] = 0.625f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.35f));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(129));
    effect.setEffectRegion(region);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    bool changedInside = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const bool inside = region.contains(QPointF(x + 0.5, y + 0.5));
            for (int channel = 0; channel < 3; ++channel) {
                if (inside) {
                    changedInside = changedInside ||
                        std::abs(actual[offset + channel] - pixels[offset + channel]) >
                            1.0e-5f;
                } else {
                    EXPECT_FLOAT_EQ(actual[offset + channel], pixels[offset + channel])
                        << "outside region x=" << x << " y=" << y
                        << " channel=" << channel;
                }
            }
            EXPECT_FLOAT_EQ(actual[offset + 3], pixels[offset + 3])
                << "x=" << x << " y=" << y;
        }
    }
    EXPECT_TRUE(changedInside);
}

TEST(LofiEffectContractTest,
     FilmGrungeEffectRegionBoundsCombinedEffectsAtFractionalPixelCenters)
{
    constexpr int width = 128;
    constexpr int height = 96;
    const QRectF region(31.25, 19.25, 69.5, 47.5);
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>((x * 3 + y) % 23) / 22.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 5) % 19) / 18.0f;
            pixels[offset + 2] = static_cast<float>((x * 7 + y * 2) % 29) / 28.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 3) % 11) / 10.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.28f));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("scratchAmount"), QVariant(0.7f));
    effect.setPropertyValue(UniString("dustAmount"), QVariant(0.65f));
    effect.setPropertyValue(UniString("flicker"), QVariant(0.2f));
    effect.setPropertyValue(UniString("gateWeave"), QVariant(3.0f));
    effect.setPropertyValue(UniString("vignette"), QVariant(0.35f));
    effect.setPropertyValue(UniString("stain"), QVariant(0.2f));
    effect.setPropertyValue(UniString("seed"), QVariant(129));
    EffectContext context;
    context.compositionFrame = 11;
    effect.setContext(context);
    effect.setEffectRegion(region);

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    bool changedInside = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const bool inside = region.contains(QPointF(x + 0.5, y + 0.5));
            for (int channel = 0; channel < 4; ++channel) {
                if (inside) {
                    if (channel < 3) {
                        changedInside = changedInside ||
                            std::abs(actual[offset + channel] -
                                     pixels[offset + channel]) > 1.0e-5f;
                    }
                } else {
                    EXPECT_FLOAT_EQ(actual[offset + channel],
                                    pixels[offset + channel])
                        << "outside fractional region x=" << x << " y=" << y
                        << " channel=" << channel;
                }
            }
        }
    }
    EXPECT_TRUE(changedInside)
        << "combined FilmGrunge effects should alter pixels inside the region";
}

TEST(LofiEffectContractTest,
     FilmGrungeCompositionStackUsesCurrentFrameAndMatchesDirectCpuOutput)
{
    constexpr int width = 40;
    constexpr int height = 28;
    constexpr std::int64_t frame = 17;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>((x * 3 + y) % 23) / 22.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 5) % 19) / 18.0f;
            pixels[offset + 2] = static_cast<float>((x * 7 + y * 2) % 29) / 28.0f;
            pixels[offset + 3] = static_cast<float>((x + y * 3) % 11) / 10.0f;
        }
    }
    ImageF32x4_RGBA sourceImage;
    sourceImage.setFromRGBA32F(pixels.data(), width, height);
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    sourceImage.setColorDescriptor(descriptor);
    ImageF32x4_RGBA expectedImage(sourceImage);

    ArtifactCompositionInitParams params(
        UniString(QStringLiteral("FilmGrunge composition stack")),
        FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(32);
    auto composition = makeShared<ArtifactComposition3D>(
        CompositionID(QStringLiteral("film-grunge-composition-stack")), params);
    ASSERT_TRUE(composition);
    composition->goToFrame(frame);

    auto stackEffect = makeShared<ArtifactFilmGrungeEffect>();
    ASSERT_TRUE(stackEffect);
    stackEffect->setComputeMode(ComputeMode::CPU);
    stackEffect->setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    stackEffect->setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    stackEffect->setPropertyValue(UniString("scratchAmount"), QVariant(0.8f));
    stackEffect->setPropertyValue(UniString("dustAmount"), QVariant(0.7f));
    stackEffect->setPropertyValue(UniString("flicker"), QVariant(0.25f));
    stackEffect->setPropertyValue(UniString("gateWeave"), QVariant(2.0f));
    stackEffect->setPropertyValue(UniString("vignette"), QVariant(0.3f));
    stackEffect->setPropertyValue(UniString("stain"), QVariant(0.15f));
    stackEffect->setPropertyValue(UniString("seed"), QVariant(431));
    const auto stackMix =
        stackEffect->editableProperty(QStringLiteral("Effect Mix"));
    ASSERT_TRUE(stackMix);
    stackMix->addKeyFrame(RationalTime(0, 30), 0.0);
    stackMix->addKeyFrame(RationalTime(34, 30), 1.0);
    composition->addEffect(stackEffect);

    auto secondStackEffect = makeShared<ArtifactFilmGrungeEffect>();
    ASSERT_TRUE(secondStackEffect);
    secondStackEffect->setComputeMode(ComputeMode::CPU);
    secondStackEffect->setPropertyValue(UniString("grainAmount"), QVariant(0.12f));
    secondStackEffect->setPropertyValue(UniString("grainSize"), QVariant(2.0f));
    secondStackEffect->setPropertyValue(UniString("scratchAmount"), QVariant(0.35f));
    secondStackEffect->setPropertyValue(UniString("dustAmount"), QVariant(0.4f));
    secondStackEffect->setPropertyValue(UniString("flicker"), QVariant(0.1f));
    secondStackEffect->setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    secondStackEffect->setPropertyValue(UniString("vignette"), QVariant(0.15f));
    secondStackEffect->setPropertyValue(UniString("stain"), QVariant(0.08f));
    secondStackEffect->setPropertyValue(UniString("seed"), QVariant(932));
    composition->addEffect(secondStackEffect);

    ArtifactFilmGrungeEffect directEffect;
    directEffect.setComputeMode(ComputeMode::CPU);
    directEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    directEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    directEffect.setPropertyValue(UniString("scratchAmount"), QVariant(0.8f));
    directEffect.setPropertyValue(UniString("dustAmount"), QVariant(0.7f));
    directEffect.setPropertyValue(UniString("flicker"), QVariant(0.25f));
    directEffect.setPropertyValue(UniString("gateWeave"), QVariant(2.0f));
    directEffect.setPropertyValue(UniString("vignette"), QVariant(0.3f));
    directEffect.setPropertyValue(UniString("stain"), QVariant(0.15f));
    directEffect.setPropertyValue(UniString("seed"), QVariant(431));
    directEffect.setMix(0.5f);
    EffectContext directContext;
    directContext.compositionFrame = frame;
    directEffect.setContext(directContext);
    const ImageF32x4RGBAWithCache directSource(sourceImage);
    ImageF32x4RGBAWithCache directIntermediate;
    ImageF32x4RGBAWithCache directOutput;
    directEffect.applyCPUOnly(directSource, directIntermediate);

    ArtifactFilmGrungeEffect secondDirectEffect;
    secondDirectEffect.setComputeMode(ComputeMode::CPU);
    secondDirectEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.12f));
    secondDirectEffect.setPropertyValue(UniString("grainSize"), QVariant(2.0f));
    secondDirectEffect.setPropertyValue(UniString("scratchAmount"), QVariant(0.35f));
    secondDirectEffect.setPropertyValue(UniString("dustAmount"), QVariant(0.4f));
    secondDirectEffect.setPropertyValue(UniString("flicker"), QVariant(0.1f));
    secondDirectEffect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    secondDirectEffect.setPropertyValue(UniString("vignette"), QVariant(0.15f));
    secondDirectEffect.setPropertyValue(UniString("stain"), QVariant(0.08f));
    secondDirectEffect.setPropertyValue(UniString("seed"), QVariant(932));
    secondDirectEffect.setContext(directContext);
    secondDirectEffect.applyCPUOnly(directIntermediate, directOutput);
    expectedImage = directOutput.image();

    ASSERT_TRUE(applyCompositionFinalEffectsToBuffer(
        composition.get(), sourceImage));
    EXPECT_FLOAT_EQ(stackEffect->mix(), 0.5f)
        << "the Composition frame must evaluate the animated Effect Mix";
    EXPECT_EQ(sourceImage.width(), width);
    EXPECT_EQ(sourceImage.height(), height);
    EXPECT_EQ(sourceImage.colorDescriptor(), descriptor);
    const float* actual = sourceImage.rgba32fData();
    const float* expected = expectedImage.rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel])
            << "composition stack channel=" << channel;
    }
    EXPECT_TRUE(stackMix->hasKeyFrames());
    const auto refreshedStackProperties = stackEffect->editableProperties();
    EXPECT_TRUE(std::find(refreshedStackProperties.begin(),
                          refreshedStackProperties.end(), stackMix) !=
                refreshedStackProperties.end())
        << "Composition rendering must retain the animated Effect Mix property";

    stackEffect->setEnabled(false);
    secondStackEffect->setEnabled(false);
    ImageF32x4_RGBA disabledBuffer;
    disabledBuffer.setFromRGBA32F(pixels.data(), width, height);
    disabledBuffer.setColorDescriptor(descriptor);
    EXPECT_FALSE(applyCompositionFinalEffectsToBuffer(
        composition.get(), disabledBuffer));
    const float* disabledPixels = disabledBuffer.rgba32fData();
    ASSERT_NE(disabledPixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(disabledPixels[channel], pixels[channel])
            << "disabled composition effect changed channel=" << channel;
    }
}

TEST(LofiEffectContractTest,
     FilmGrungeCompositionImagePathIsDeterministicPerFrame)
{
    constexpr int width = 48;
    constexpr int height = 32;
    ArtifactCompositionInitParams params(
        UniString(QStringLiteral("FilmGrunge image path")),
        FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(24);
    auto composition = makeShared<ArtifactComposition3D>(
        CompositionID(QStringLiteral("film-grunge-image-path")), params);
    ASSERT_TRUE(composition);

    auto effect = makeShared<ArtifactFilmGrungeEffect>();
    ASSERT_TRUE(effect);
    effect->setComputeMode(ComputeMode::CPU);
    effect->setPropertyValue(UniString("grainAmount"), QVariant(0.5f));
    effect->setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect->setPropertyValue(UniString("scratchAmount"), QVariant(0.4f));
    effect->setPropertyValue(UniString("dustAmount"), QVariant(0.5f));
    effect->setPropertyValue(UniString("flicker"), QVariant(0.35f));
    effect->setPropertyValue(UniString("gateWeave"), QVariant(2.0f));
    effect->setPropertyValue(UniString("vignette"), QVariant(0.4f));
    effect->setPropertyValue(UniString("stain"), QVariant(0.2f));
    effect->setPropertyValue(UniString("seed"), QVariant(731));
    composition->addEffect(effect);

    QImage source(width, height, QImage::Format_RGBA8888);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            source.setPixelColor(
                x, y,
                QColor((x * 17 + y * 3) % 256,
                       (x * 5 + y * 19) % 256,
                       (x * 11 + y * 7) % 256,
                       64 + (x * 13 + y * 9) % 192));
        }
    }

    const auto renderAtFrame = [&](const std::int64_t frame,
                                   const DetailLevel detail) {
        composition->goToFrame(frame);
        QImage output = source;
        EXPECT_TRUE(applyCompositionFinalEffectsToImage(
            composition.get(), output, detail));
        return output;
    };
    const QImage frame12 = renderAtFrame(12, DetailLevel::High);
    const QImage repeatedFrame12 = renderAtFrame(12, DetailLevel::High);
    const QImage frame13 = renderAtFrame(13, DetailLevel::High);
    const QImage lowFrame12 = renderAtFrame(12, DetailLevel::Low);
    const QImage repeatedLowFrame12 = renderAtFrame(12, DetailLevel::Low);
    const QImage mediumFrame12 = renderAtFrame(12, DetailLevel::Medium);
    const QImage repeatedMediumFrame12 = renderAtFrame(12, DetailLevel::Medium);

    EXPECT_EQ(frame12.size(), QSize(width, height));
    EXPECT_EQ(lowFrame12.size(), QSize(24, 16));
    EXPECT_EQ(mediumFrame12.size(), QSize(36, 24));
    EXPECT_EQ(frame12, repeatedFrame12)
        << "re-rendering the same composition frame must be deterministic";
    EXPECT_NE(frame12, frame13)
        << "changing the composition frame must change the FilmGrunge image";
    EXPECT_EQ(lowFrame12, repeatedLowFrame12)
        << "Low detail output must be deterministic at a fixed frame";
    EXPECT_EQ(mediumFrame12, repeatedMediumFrame12)
        << "Medium detail output must be deterministic at a fixed frame";

    for (const QImage::Format format : {
             QImage::Format_ARGB32,
             QImage::Format_ARGB32_Premultiplied,
             QImage::Format_RGB32}) {
        SCOPED_TRACE(static_cast<int>(format));
        const QImage formattedSource = source.convertToFormat(format);
        ASSERT_EQ(formattedSource.format(), format);
        const auto renderFormattedFrame = [&]() {
            composition->goToFrame(12);
            QImage output = formattedSource;
            EXPECT_TRUE(applyCompositionFinalEffectsToImage(
                composition.get(), output, DetailLevel::High));
            EXPECT_EQ(output.size(), QSize(width, height));
            return output;
        };
        const QImage formattedOutput = renderFormattedFrame();
        const QImage repeatedFormattedOutput = renderFormattedFrame();
        EXPECT_EQ(formattedOutput, repeatedFormattedOutput)
            << "QImage format must retain fixed-frame determinism";
        if (format == QImage::Format_RGB32) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    EXPECT_EQ(qAlpha(formattedOutput.pixel(x, y)), 255)
                        << "opaque input acquired alpha at x=" << x << " y=" << y;
                }
            }
        }
    }

    // The public image path normalizes every processed frame to RGBA8888
    // straight alpha. With the effect stack present but visually neutral,
    // equivalent straight and premultiplied inputs must decode to the same
    // visible pixels instead of darkening the semi-transparent edge.
    auto neutralEffect = makeShared<ArtifactFilmGrungeEffect>();
    ASSERT_TRUE(neutralEffect);
    neutralEffect->setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "gateWeave", "vignette", "stain"}) {
        neutralEffect->setPropertyValue(UniString(property), QVariant(0.0f));
    }
    composition->addEffect(neutralEffect);
    QImage alphaEdge(2, 1, QImage::Format_RGBA8888);
    alphaEdge.setPixelColor(0, 0, QColor(220, 120, 40, 128));
    alphaEdge.setPixelColor(1, 0, QColor(30, 180, 240, 64));
    const QImage straightAlphaEdge = alphaEdge.convertToFormat(QImage::Format_ARGB32);
    const QImage premultipliedAlphaEdge =
        alphaEdge.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    ASSERT_EQ(straightAlphaEdge.format(), QImage::Format_ARGB32);
    ASSERT_EQ(premultipliedAlphaEdge.format(),
              QImage::Format_ARGB32_Premultiplied);
    ASSERT_NE(straightAlphaEdge.pixel(0, 0), premultipliedAlphaEdge.pixel(0, 0))
        << "fixture must contain premultiplied channel storage";
    composition->goToFrame(12);
    QImage straightAlphaOutput = straightAlphaEdge;
    QImage premultipliedAlphaOutput = premultipliedAlphaEdge;
    ASSERT_TRUE(applyCompositionFinalEffectsToImage(
        composition.get(), straightAlphaOutput, DetailLevel::High));
    ASSERT_TRUE(applyCompositionFinalEffectsToImage(
        composition.get(), premultipliedAlphaOutput, DetailLevel::High));
    EXPECT_EQ(straightAlphaOutput.format(), QImage::Format_RGBA8888);
    EXPECT_EQ(premultipliedAlphaOutput.format(), QImage::Format_RGBA8888);
    EXPECT_EQ(straightAlphaOutput, premultipliedAlphaOutput)
        << "premultiplied storage must decode to the same straight-alpha image";
    EXPECT_EQ(straightAlphaOutput.pixelColor(0, 0), QColor(220, 120, 40, 128));
    EXPECT_EQ(straightAlphaOutput.pixelColor(1, 0), QColor(30, 180, 240, 64));

    effect->setEnabled(false);
    QImage disabledOutput = source;
    EXPECT_FALSE(applyCompositionFinalEffectsToImage(
        composition.get(), disabledOutput, DetailLevel::High));
    EXPECT_EQ(disabledOutput, source)
        << "a disabled rasterizer stack must leave the input image untouched";

    effect->setEnabled(true);
    effect->setPipelineStage(EffectPipelineStage::PreProcess);
    QImage nonRasterizerOutput = source;
    EXPECT_FALSE(applyCompositionFinalEffectsToImage(
        composition.get(), nonRasterizerOutput, DetailLevel::High));
    EXPECT_EQ(nonRasterizerOutput, source)
        << "a non-rasterizer effect must not enter the rasterizer image path";

    QImage nullCompositionOutput = source;
    EXPECT_FALSE(applyCompositionFinalEffectsToImage(
        nullptr, nullCompositionOutput, DetailLevel::High));
    EXPECT_EQ(nullCompositionOutput, source)
        << "a null Composition must leave the input image untouched";

    QImage emptyImage;
    EXPECT_FALSE(applyCompositionFinalEffectsToImage(
        composition.get(), emptyImage, DetailLevel::High));
    EXPECT_TRUE(emptyImage.isNull());
}

TEST(LofiEffectContractTest, FilmGrungeStretchesPrimaryEffectMaskAcrossSurface)
{
    constexpr int width = 24;
    constexpr int height = 16;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>((x * 3 + y) % 17) / 16.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 2) % 13) / 12.0f;
            pixels[offset + 2] = static_cast<float>((x * 5 + y * 3) % 19) / 18.0f;
            pixels[offset + 3] = 0.7f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.4f));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(987));
    EffectContext context;
    context.compositionFrame = 23;
    effect.setContext(context);

    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache disabledMaskEffect;
    ImageF32x4RGBAWithCache maskedEffect;
    ImageF32x4RGBAWithCache opacityMaskedEffect;
    ImageF32x4RGBAWithCache invertedMaskedEffect;
    ImageF32x4RGBAWithCache maximumOpacityMaskedEffect;
    ImageF32x4RGBAWithCache minimumOpacityMaskedEffect;
    ImageF32x4RGBAWithCache reDisabledMaskEffect;
    effect.applyCPUOnly(source, fullEffect);

    constexpr std::array<float, 16> maskPixels{
        0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.25f,
        0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.5f};
    auto mask = makeShared<ImageF32x4_RGBA>();
    mask->setFromRGBA32F(maskPixels.data(), 2, 2);
    effect.setMaskImage(mask);
    effect.applyCPUOnly(source, disabledMaskEffect);
    effect.setMaskEnabled(true);
    effect.applyCPUOnly(source, maskedEffect);
    effect.setMaskOpacity(0.5f);
    effect.applyCPUOnly(source, opacityMaskedEffect);
    effect.setMaskInverted(true);
    effect.applyCPUOnly(source, invertedMaskedEffect);
    effect.setMaskInverted(false);
    effect.setMaskOpacity(2.0f);
    effect.applyCPUOnly(source, maximumOpacityMaskedEffect);
    effect.setMaskOpacity(-0.5f);
    effect.applyCPUOnly(source, minimumOpacityMaskedEffect);
    effect.setMaskEnabled(false);
    effect.applyCPUOnly(source, reDisabledMaskEffect);

    const float* full = fullEffect.image().rgba32fData();
    const float* disabledMask = disabledMaskEffect.image().rgba32fData();
    const float* masked = maskedEffect.image().rgba32fData();
    const float* opacityMasked = opacityMaskedEffect.image().rgba32fData();
    const float* invertedMasked = invertedMaskedEffect.image().rgba32fData();
    const float* maximumOpacityMasked =
        maximumOpacityMaskedEffect.image().rgba32fData();
    const float* minimumOpacityMasked =
        minimumOpacityMaskedEffect.image().rgba32fData();
    const float* reDisabledMask = reDisabledMaskEffect.image().rgba32fData();
    ASSERT_NE(full, nullptr);
    ASSERT_NE(disabledMask, nullptr);
    ASSERT_NE(masked, nullptr);
    ASSERT_NE(opacityMasked, nullptr);
    ASSERT_NE(invertedMasked, nullptr);
    ASSERT_NE(maximumOpacityMasked, nullptr);
    ASSERT_NE(minimumOpacityMasked, nullptr);
    ASSERT_NE(reDisabledMask, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(disabledMask[channel], full[channel])
            << "disabled mask channel=" << channel;
        EXPECT_FLOAT_EQ(reDisabledMask[channel], full[channel])
            << "re-disabled mask channel=" << channel;
        EXPECT_FLOAT_EQ(maximumOpacityMasked[channel], masked[channel])
            << "maximum opacity channel=" << channel;
        EXPECT_FLOAT_EQ(minimumOpacityMasked[channel], full[channel])
            << "minimum opacity channel=" << channel;
    }
    constexpr std::array<float, 4> maskAlpha{0.0f, 0.25f, 0.5f, 1.0f};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int maskX = std::min(
                static_cast<int>((static_cast<float>(x) + 0.5f) * 2.0f / width),
                1);
            const int maskY = std::min(
                static_cast<int>((static_cast<float>(y) + 0.5f) * 2.0f / height),
                1);
            const float alpha = maskAlpha[static_cast<std::size_t>(maskY * 2 + maskX)];
            const float opacityAlpha = alpha * 0.5f;
            const float invertedAlpha = (1.0f - alpha) * 0.5f;
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (int channel = 0; channel < 4; ++channel) {
                EXPECT_NEAR(masked[offset + channel],
                            pixels[offset + channel] * (1.0f - alpha) +
                                full[offset + channel] * alpha,
                            1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
                EXPECT_NEAR(opacityMasked[offset + channel],
                            pixels[offset + channel] * (1.0f - opacityAlpha) +
                                full[offset + channel] * opacityAlpha,
                            1.0e-6f)
                    << "opacity x=" << x << " y=" << y
                    << " channel=" << channel;
                EXPECT_NEAR(invertedMasked[offset + channel],
                            pixels[offset + channel] * (1.0f - invertedAlpha) +
                                full[offset + channel] * invertedAlpha,
                            1.0e-6f)
                    << "inverted x=" << x << " y=" << y
                    << " channel=" << channel;
            }
        }
    }
}

TEST(LofiEffectContractTest,
     FilmGrungeCombinesMixRegionAndPrimaryMaskCoverage)
{
    constexpr int width = 16;
    constexpr int height = 12;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>((x * 3 + y) % 17) / 16.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 2) % 13) / 12.0f;
            pixels[offset + 2] = static_cast<float>((x * 5 + y * 3) % 19) / 18.0f;
            pixels[offset + 3] = 0.75f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.35f));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(731));
    EffectContext context;
    context.compositionFrame = 9;
    effect.setContext(context);

    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache combinedEffect;
    effect.applyCPUOnly(source, fullEffect);

    const QRectF region(4.0, 3.0, 8.0, 6.0);
    constexpr std::array<float, 16> maskPixels{
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.25f};
    auto mask = makeShared<ImageF32x4_RGBA>();
    mask->setFromRGBA32F(maskPixels.data(), 2, 2);
    effect.setMix(0.5f);
    effect.setEffectRegion(region);
    effect.setMaskImage(mask);
    effect.setMaskEnabled(true);
    effect.setMaskOpacity(0.5f);
    effect.applyCPUOnly(source, combinedEffect);

    const float* full = fullEffect.image().rgba32fData();
    const float* combined = combinedEffect.image().rgba32fData();
    ASSERT_NE(full, nullptr);
    ASSERT_NE(combined, nullptr);
    constexpr std::array<float, 4> maskAlpha{0.0f, 0.5f, 1.0f, 0.25f};
    bool observedEffectInsideCoverage = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int maskX = std::min(
                static_cast<int>((static_cast<float>(x) + 0.5f) * 2.0f / width),
                1);
            const int maskY = std::min(
                static_cast<int>((static_cast<float>(y) + 0.5f) * 2.0f / height),
                1);
            const bool inside = region.contains(QPointF(x + 0.5, y + 0.5));
            const float coverage = inside
                ? 0.5f * maskAlpha[static_cast<std::size_t>(maskY * 2 + maskX)] *
                      0.5f
                : 0.0f;
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (int channel = 0; channel < 4; ++channel) {
                if (coverage > 0.0f && channel < 3) {
                    observedEffectInsideCoverage =
                        observedEffectInsideCoverage ||
                        std::abs(full[offset + channel] -
                                 pixels[offset + channel]) > 1.0e-5f;
                }
                EXPECT_NEAR(combined[offset + channel],
                            pixels[offset + channel] * (1.0f - coverage) +
                                full[offset + channel] * coverage,
                            1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
            }
        }
    }
    EXPECT_TRUE(observedEffectInsideCoverage);
}

TEST(LofiEffectContractTest,
     FilmGrungeCombinesValidMasksAndIgnoresInvalidAdditionalMasks)
{
    constexpr int width = 16;
    constexpr int height = 12;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    std::array<float, pixelCount * 4> firstMaskPixels{};
    std::array<float, pixelCount * 4> secondMaskPixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t pixel = static_cast<std::size_t>(y * width + x);
            const std::size_t offset = pixel * 4;
            pixels[offset + 0] = static_cast<float>((x * 3 + y) % 17) / 16.0f;
            pixels[offset + 1] = static_cast<float>((x + y * 2) % 13) / 12.0f;
            pixels[offset + 2] = static_cast<float>((x * 5 + y * 3) % 19) / 18.0f;
            pixels[offset + 3] =
                x % 3 == 0 ? -0.5f : x % 3 == 1 ? 0.6f : 1.5f;
            firstMaskPixels[offset + 3] =
                static_cast<float>((x + y) % 5) * 0.5f - 0.5f;
            secondMaskPixels[offset + 3] =
                static_cast<float>((x * 2 + y) % 7) * 0.25f - 0.25f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.35f));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(431));
    EffectContext context;
    context.compositionFrame = 17;
    effect.setContext(context);

    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache maskedEffect;
    effect.applyCPUOnly(source, fullEffect);

    constexpr std::array<float, 4> primaryMaskPixel{0.0f, 0.0f, 0.0f, 0.8f};
    auto primaryMask = makeShared<ImageF32x4_RGBA>();
    primaryMask->setFromRGBA32F(primaryMaskPixel.data(), 1, 1);
    auto firstMask = makeShared<ImageF32x4_RGBA>();
    firstMask->setFromRGBA32F(firstMaskPixels.data(), width, height);
    auto secondMask = makeShared<ImageF32x4_RGBA>();
    secondMask->setFromRGBA32F(secondMaskPixels.data(), width, height);
    effect.setMaskImage(primaryMask);
    effect.setMaskEnabled(true);
    effect.setMaskOpacity(0.5f);
    effect.addEffectMaskImage(firstMask);
    effect.addEffectMaskImage(secondMask);
    constexpr std::array<float, 4> mismatchedMaskPixel{
        0.0f, 0.0f, 0.0f, 1.0f};
    auto mismatchedMask = makeShared<ImageF32x4_RGBA>();
    mismatchedMask->setFromRGBA32F(mismatchedMaskPixel.data(), 1, 1);
    effect.addEffectMaskImage(mismatchedMask);
    effect.addEffectMaskImage(SharedPtr<ImageF32x4_RGBA>{});
    effect.applyCPUOnly(source, maskedEffect);

    const float* full = fullEffect.image().rgba32fData();
    const float* masked = maskedEffect.image().rgba32fData();
    ASSERT_NE(full, nullptr);
    ASSERT_NE(masked, nullptr);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const float combinedAlpha = 0.8f * 0.5f *
            std::clamp(firstMaskPixels[pixel * 4 + 3], 0.0f, 1.0f) *
            std::clamp(secondMaskPixels[pixel * 4 + 3], 0.0f, 1.0f);
        for (int channel = 0; channel < 4; ++channel) {
            const std::size_t offset =
                pixel * 4 + static_cast<std::size_t>(channel);
            EXPECT_NEAR(masked[offset],
                        pixels[offset] * (1.0f - combinedAlpha) +
                            full[offset] * combinedAlpha,
                        1.0e-6f)
                << "pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(masked[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "alpha pixel=" << pixel;
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
     FilmGrungeVignetteTreatsSinglePixelAsTheSurfaceCenter)
{
    constexpr std::array<float, 4> pixels{0.6f, 0.4f, 0.2f, 0.35f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                 "flicker", "gateWeave", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("vignette"), QVariant(1.0f));

    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel])
            << "a one-pixel surface has no off-center pixel";
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
     FilmGrungeVignetteCentersTheCollapsedAxisForSingleRowsAndColumns)
{
    constexpr int extent = 5;
    constexpr float sourceValue = 0.4f;
    constexpr float alpha = 0.65f;
    const auto renderDegenerateSurface = [=](const int width, const int height) {
        const std::size_t pixelCount =
            static_cast<std::size_t>(width) * height;
        std::array<float, extent * 4> pixels{};
        for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel * 4 + 0] = sourceValue;
            pixels[pixel * 4 + 1] = sourceValue;
            pixels[pixel * 4 + 2] = sourceValue;
            pixels[pixel * 4 + 3] = alpha;
        }
        ImageF32x4_RGBA image;
        image.setFromRGBA32F(pixels.data(), width, height);
        const ImageF32x4RGBAWithCache source(image);
        ImageF32x4RGBAWithCache output;
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                     "flicker", "gateWeave", "stain"}) {
            effect.setPropertyValue(UniString(property), QVariant(0.0f));
        }
        effect.setPropertyValue(UniString("vignette"), QVariant(1.0f));
        effect.applyCPUOnly(source, output);
        return output;
    };

    const auto singleRow = renderDegenerateSurface(extent, 1);
    const auto singleColumn = renderDegenerateSurface(1, extent);
    const float* rowPixels = singleRow.image().rgba32fData();
    const float* columnPixels = singleColumn.image().rgba32fData();
    ASSERT_NE(rowPixels, nullptr);
    ASSERT_NE(columnPixels, nullptr);
    for (int index = 0; index < extent; ++index) {
        const float normalized = (2.0f * index / (extent - 1)) - 1.0f;
        const float expected = sourceValue * std::clamp(
            1.0f - std::pow(normalized * normalized, 1.35f), 0.0f, 1.0f);
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(rowPixels[static_cast<std::size_t>(index) * 4 + channel],
                        expected, 1.0e-6f)
                << "single row index=" << index << " channel=" << channel;
            EXPECT_NEAR(columnPixels[static_cast<std::size_t>(index) * 4 + channel],
                        expected, 1.0e-6f)
                << "single column index=" << index << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(rowPixels[static_cast<std::size_t>(index) * 4 + 3], alpha);
        EXPECT_FLOAT_EQ(columnPixels[static_cast<std::size_t>(index) * 4 + 3], alpha);
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
     FilmGrungeGrainSizeRoundsAtHalfStepBoundaries)
{
    constexpr int width = 19;
    constexpr int height = 13;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = 0.75f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache sizeOne;
    ImageF32x4RGBAWithCache belowHalfStep;
    ImageF32x4RGBAWithCache sizeTwo;
    ImageF32x4RGBAWithCache atHalfStep;
    const auto renderWithGrainSize = [&source](const float grainSize,
                                               ImageF32x4RGBAWithCache& output) {
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
        effect.setPropertyValue(UniString("grainSize"), QVariant(grainSize));
        effect.setPropertyValue(UniString("seed"), QVariant(77));
        for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                     "gateWeave", "vignette", "stain"}) {
            effect.setPropertyValue(UniString(property), QVariant(0.0f));
        }
        EffectContext context;
        context.compositionFrame = 18;
        effect.setContext(context);
        effect.applyCPUOnly(source, output);
    };
    renderWithGrainSize(1.0f, sizeOne);
    renderWithGrainSize(1.49f, belowHalfStep);
    renderWithGrainSize(2.0f, sizeTwo);
    renderWithGrainSize(1.5f, atHalfStep);

    const float* sizeOnePixels = sizeOne.image().rgba32fData();
    const float* belowHalfStepPixels = belowHalfStep.image().rgba32fData();
    const float* sizeTwoPixels = sizeTwo.image().rgba32fData();
    const float* atHalfStepPixels = atHalfStep.image().rgba32fData();
    ASSERT_NE(sizeOnePixels, nullptr);
    ASSERT_NE(belowHalfStepPixels, nullptr);
    ASSERT_NE(sizeTwoPixels, nullptr);
    ASSERT_NE(atHalfStepPixels, nullptr);
    bool observedBlockSizeChange = false;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 4; ++channel) {
            EXPECT_FLOAT_EQ(sizeOnePixels[offset + channel],
                            belowHalfStepPixels[offset + channel])
                << "grainSize 1.49 should round to block size 1";
            EXPECT_FLOAT_EQ(sizeTwoPixels[offset + channel],
                            atHalfStepPixels[offset + channel])
                << "grainSize 1.5 should round to block size 2";
        }
        observedBlockSizeChange = observedBlockSizeChange ||
            sizeOnePixels[offset + 1] != sizeTwoPixels[offset + 1];
    }
    EXPECT_TRUE(observedBlockSizeChange)
        << "the rounded grain block size should change the spatial pattern";
}

TEST(LofiEffectContractTest,
     FilmGrungeGrainSizeClampsToItsMinimumAndMaximum)
{
    constexpr int width = 17;
    constexpr int height = 10;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = 0.8f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    const auto renderWithGrainSize = [&source](const float grainSize) {
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setPropertyValue(UniString("grainAmount"), QVariant(0.35f));
        effect.setPropertyValue(UniString("grainSize"), QVariant(grainSize));
        effect.setPropertyValue(UniString("seed"), QVariant(419));
        for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                     "gateWeave", "vignette", "stain"}) {
            effect.setPropertyValue(UniString(property), QVariant(0.0f));
        }
        EffectContext context;
        context.compositionFrame = 23;
        effect.setContext(context);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };

    const auto belowMinimum = renderWithGrainSize(-4.0f);
    const auto minimum = renderWithGrainSize(1.0f);
    const auto aboveMaximum = renderWithGrainSize(12.0f);
    const auto maximum = renderWithGrainSize(8.0f);
    const float* belowMinimumPixels = belowMinimum.image().rgba32fData();
    const float* minimumPixels = minimum.image().rgba32fData();
    const float* aboveMaximumPixels = aboveMaximum.image().rgba32fData();
    const float* maximumPixels = maximum.image().rgba32fData();
    ASSERT_NE(belowMinimumPixels, nullptr);
    ASSERT_NE(minimumPixels, nullptr);
    ASSERT_NE(aboveMaximumPixels, nullptr);
    ASSERT_NE(maximumPixels, nullptr);
    for (std::size_t channel = 0; channel < pixelCount * 4; ++channel) {
        EXPECT_FLOAT_EQ(belowMinimumPixels[channel], minimumPixels[channel])
            << "below-range grainSize should clamp to 1 at channel=" << channel;
        EXPECT_FLOAT_EQ(aboveMaximumPixels[channel], maximumPixels[channel])
            << "above-range grainSize should clamp to 8 at channel=" << channel;
    }
}

TEST(LofiEffectContractTest,
     FilmGrungeUnitGrainSizeRetainsPixelScaleVariation)
{
    constexpr int width = 14;
    constexpr int height = 9;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = 0.6f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.3f));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(593));
    for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    EffectContext context;
    context.compositionFrame = 31;
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    bool foundAdjacentPixelVariation = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x + 1 < width; ++x) {
            const std::size_t left =
                static_cast<std::size_t>((y * width + x) * 4);
            const std::size_t right = left + 4;
            foundAdjacentPixelVariation = foundAdjacentPixelVariation ||
                result[left + 1] != result[right + 1];
            EXPECT_FLOAT_EQ(result[left + 3], 0.6f)
                << "x=" << x << " y=" << y;
        }
    }
    EXPECT_TRUE(foundAdjacentPixelVariation)
        << "grainSize 1 should permit grain variation at adjacent pixels";
}

TEST(LofiEffectContractTest,
     FilmGrungeGrainAmountScalesOutputAndClampsAtItsPropertyBounds)
{
    constexpr int width = 16;
    constexpr int height = 8;
    constexpr std::size_t channelCount =
        static_cast<std::size_t>(width) * height * 4;
    std::array<float, channelCount> pixels{};
    for (std::size_t pixel = 0; pixel < channelCount / 4; ++pixel) {
        pixels[pixel * 4 + 0] = 0.4f;
        pixels[pixel * 4 + 1] = 0.4f;
        pixels[pixel * 4 + 2] = 0.4f;
        pixels[pixel * 4 + 3] = 0.7f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    EffectContext context;
    context.compositionFrame = 17;
    const auto renderWithGrainAmount = [&](const QVariant& amount) {
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setContext(context);
        for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                     "gateWeave", "vignette", "stain"}) {
            effect.setPropertyValue(UniString(property), QVariant(0.0f));
        }
        effect.setPropertyValue(UniString("seed"), QVariant(1977));
        effect.setPropertyValue(UniString("grainAmount"), amount);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };

    const auto noGrain = renderWithGrainAmount(QVariant(0.0f));
    const auto halfGrain = renderWithGrainAmount(QVariant(0.25f));
    const auto maximumGrain = renderWithGrainAmount(QVariant(0.5f));
    const auto clampedLow = renderWithGrainAmount(QVariant(-1.0f));
    const auto clampedHigh = renderWithGrainAmount(QVariant(1.0f));
    const auto nonFinite = renderWithGrainAmount(
        QVariant(std::numeric_limits<double>::quiet_NaN()));
    const float* noGrainPixels = noGrain.image().rgba32fData();
    const float* halfGrainPixels = halfGrain.image().rgba32fData();
    const float* maximumGrainPixels = maximumGrain.image().rgba32fData();
    const float* clampedLowPixels = clampedLow.image().rgba32fData();
    const float* clampedHighPixels = clampedHigh.image().rgba32fData();
    const float* nonFinitePixels = nonFinite.image().rgba32fData();
    ASSERT_NE(noGrainPixels, nullptr);
    ASSERT_NE(halfGrainPixels, nullptr);
    ASSERT_NE(maximumGrainPixels, nullptr);
    ASSERT_NE(clampedLowPixels, nullptr);
    ASSERT_NE(clampedHighPixels, nullptr);
    ASSERT_NE(nonFinitePixels, nullptr);
    bool observedGrain = false;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(noGrainPixels[channel], pixels[channel])
            << "zero amount channel=" << channel;
        EXPECT_FLOAT_EQ(clampedLowPixels[channel], noGrainPixels[channel])
            << "low clamp channel=" << channel;
        EXPECT_FLOAT_EQ(nonFinitePixels[channel], noGrainPixels[channel])
            << "non-finite reset channel=" << channel;
        EXPECT_FLOAT_EQ(clampedHighPixels[channel], maximumGrainPixels[channel])
            << "high clamp channel=" << channel;
        EXPECT_NEAR(halfGrainPixels[channel],
                    noGrainPixels[channel] +
                        (maximumGrainPixels[channel] - noGrainPixels[channel]) *
                            0.5f,
                    1.0e-6f)
            << "half amount channel=" << channel;
        if (channel % 4 != 3) {
            observedGrain = observedGrain ||
                std::abs(maximumGrainPixels[channel] - noGrainPixels[channel]) >
                    1.0e-5f;
        } else {
            EXPECT_FLOAT_EQ(maximumGrainPixels[channel], pixels[channel])
                << "alpha channel=" << channel;
        }
    }
    EXPECT_TRUE(observedGrain);
}

TEST(LofiEffectContractTest,
     FilmGrungeGrainAppliesStableChannelSpecificChromaticWeights)
{
    constexpr std::array<float, 4> pixels{0.4f, 0.4f, 0.4f, 0.65f};
    constexpr float grainAmount = 0.25f;
    constexpr int seed = 123;
    constexpr std::int64_t frame = 12;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(grainAmount));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(seed));
    for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    EffectContext context;
    context.compositionFrame = frame;
    effect.setContext(context);

    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    const auto frameSeed = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(seed) * 1664525ull +
        static_cast<std::uint64_t>(frame) * 1013904223ull);
    const float grain =
        (static_cast<float>(frameSeed & 0xffffu) / 32767.5f - 1.0f) *
        grainAmount;
    constexpr std::array<float, 3> channelWeights{0.92f, 1.0f, 1.05f};
    for (std::size_t channel = 0; channel < channelWeights.size(); ++channel) {
        EXPECT_NEAR(result[channel], pixels[channel] + grain * channelWeights[channel],
                    1.0e-6f)
            << "channel=" << channel;
    }
    EXPECT_FLOAT_EQ(result[3], pixels[3]);
}

TEST(LofiEffectContractTest,
     FilmGrungeActiveGrainPreservesOutOfRangeRgb)
{
    constexpr std::array<float, 4> pixels{4.0f, -3.0f, 2.0f, 1.5f};
    constexpr float grainAmount = 0.5f;
    constexpr int seed = 3841;
    constexpr std::int64_t frame = 27;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(grainAmount));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(seed));
    for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    EffectContext context;
    context.compositionFrame = frame;
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    const auto frameSeed = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(seed) * 1664525ull +
        static_cast<std::uint64_t>(frame) * 1013904223ull);
    const float grain =
        (static_cast<float>(frameSeed & 0xffffu) / 32767.5f - 1.0f) *
        grainAmount;
    constexpr std::array<float, 3> channelWeights{0.92f, 1.0f, 1.05f};
    for (std::size_t channel = 0; channel < channelWeights.size(); ++channel) {
        EXPECT_NEAR(result[channel],
                    pixels[channel] + grain * channelWeights[channel],
                    1.0e-6f)
            << "channel=" << channel;
    }
    EXPECT_GT(result[0], 1.0f);
    EXPECT_LT(result[1], 0.0f);
    EXPECT_GT(result[2], 1.0f);
    EXPECT_FLOAT_EQ(result[3], pixels[3]);
}

TEST(LofiEffectContractTest,
     FilmGrungeMixInterpolatesActiveGrainOnHdrRgb)
{
    constexpr std::array<float, 4> pixels{4.0f, -3.0f, 2.0f, 1.5f};
    constexpr float grainAmount = 0.5f;
    constexpr float mix = 0.4f;
    constexpr int seed = 3841;
    constexpr std::int64_t frame = 27;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(grainAmount));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(seed));
    for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                 "gateWeave", "vignette", "stain"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setMix(mix);
    EffectContext context;
    context.compositionFrame = frame;
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    const auto frameSeed = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(seed) * 1664525ull +
        static_cast<std::uint64_t>(frame) * 1013904223ull);
    const float grain =
        (static_cast<float>(frameSeed & 0xffffu) / 32767.5f - 1.0f) *
        grainAmount;
    constexpr std::array<float, 3> channelWeights{0.92f, 1.0f, 1.05f};
    for (std::size_t channel = 0; channel < channelWeights.size(); ++channel) {
        const float effectValue =
            pixels[channel] + grain * channelWeights[channel];
        const float expected = pixels[channel] * (1.0f - mix) +
                               effectValue * mix;
        EXPECT_NEAR(result[channel], expected, 1.0e-6f)
            << "channel=" << channel;
    }
    EXPECT_GT(result[0], 1.0f);
    EXPECT_LT(result[1], 0.0f);
    EXPECT_GT(result[2], 1.0f);
    EXPECT_FLOAT_EQ(result[3], pixels[3]);
}

TEST(LofiEffectContractTest,
     FilmGrungeDustAddsSharedRgbDamageAfterGrainVignetteAndStain)
{
    constexpr int width = 96;
    constexpr int height = 72;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    constexpr float alpha = 0.7f;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    const auto renderWithDust = [&source](const float dustAmount) {
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setPropertyValue(UniString("grainAmount"), QVariant(0.3f));
        effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
        effect.setPropertyValue(UniString("dustAmount"), QVariant(dustAmount));
        effect.setPropertyValue(UniString("scratchAmount"), QVariant(0.0f));
        effect.setPropertyValue(UniString("flicker"), QVariant(0.0f));
        effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
        effect.setPropertyValue(UniString("vignette"), QVariant(0.25f));
        effect.setPropertyValue(UniString("stain"), QVariant(0.15f));
        effect.setPropertyValue(UniString("seed"), QVariant(823));
        EffectContext context;
        context.compositionFrame = 19;
        effect.setContext(context);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };

    const auto grainOnly = renderWithDust(0.0f);
    const auto grainAndDust = renderWithDust(1.0f);
    const float* grainPixels = grainOnly.image().rgba32fData();
    const float* combinedPixels = grainAndDust.image().rgba32fData();
    ASSERT_NE(grainPixels, nullptr);
    ASSERT_NE(combinedPixels, nullptr);
    bool observedDust = false;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = pixel * 4;
        const float redDelta = combinedPixels[offset] - grainPixels[offset];
        const float greenDelta =
            combinedPixels[offset + 1] - grainPixels[offset + 1];
        const float blueDelta =
            combinedPixels[offset + 2] - grainPixels[offset + 2];
        EXPECT_NEAR(redDelta, greenDelta, 1.0e-6f)
            << "dust delta should be shared by RGB at pixel=" << pixel;
        EXPECT_NEAR(greenDelta, blueDelta, 1.0e-6f)
            << "dust delta should be shared by RGB at pixel=" << pixel;
        EXPECT_LE(greenDelta, 1.0e-6f)
            << "dust should not brighten pixel=" << pixel;
        observedDust = observedDust || greenDelta < -1.0e-6f;
        EXPECT_FLOAT_EQ(grainPixels[offset + 3], alpha);
        EXPECT_FLOAT_EQ(combinedPixels[offset + 3], alpha);
    }
    EXPECT_TRUE(observedDust);
}

TEST(LofiEffectContractTest,
     FilmGrungeScratchProcessesTransparentRgbWithoutChangingAlpha)
{
    constexpr int width = 1;
    constexpr int height = 1;
    constexpr std::array<float, 4> pixels{0.25f, 0.5f, 0.75f, 0.0f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.0f));
    effect.setPropertyValue(UniString("scratchAmount"), QVariant(1.0f));
    effect.setPropertyValue(UniString("dustAmount"), QVariant(0.0f));
    effect.setPropertyValue(UniString("flicker"), QVariant(0.0f));
    effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    effect.setPropertyValue(UniString("vignette"), QVariant(0.0f));
    effect.setPropertyValue(UniString("stain"), QVariant(0.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(1001));
    EffectContext context;
    context.compositionFrame = 4;
    effect.setContext(context);

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(output.image().width(), width);
    EXPECT_EQ(output.image().height(), height);
    EXPECT_GT(result[0], pixels[0]);
    EXPECT_GT(result[1], pixels[1]);
    EXPECT_GT(result[2], pixels[2]);
    EXPECT_FLOAT_EQ(result[3], 0.0f);
}

TEST(LofiEffectContractTest,
     FilmGrungeDustProcessesTransparentRgbWithoutChangingAlpha)
{
    constexpr int width = 128;
    constexpr int height = 96;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    constexpr std::array<float, 4> sourcePixel{0.8f, 0.6f, 0.4f, 0.0f};
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        std::copy(sourcePixel.begin(), sourcePixel.end(),
                  pixels.begin() + static_cast<std::ptrdiff_t>(pixel * 4));
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.0f));
    effect.setPropertyValue(UniString("scratchAmount"), QVariant(0.0f));
    effect.setPropertyValue(UniString("dustAmount"), QVariant(1.0f));
    effect.setPropertyValue(UniString("flicker"), QVariant(0.0f));
    effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    effect.setPropertyValue(UniString("vignette"), QVariant(0.0f));
    effect.setPropertyValue(UniString("stain"), QVariant(0.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(1002));
    EffectContext context;
    context.compositionFrame = 4;
    effect.setContext(context);

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(output.image().width(), width);
    EXPECT_EQ(output.image().height(), height);
    bool observedDust = false;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = pixel * 4;
        EXPECT_FLOAT_EQ(result[offset + 3], 0.0f) << "pixel=" << pixel;
        const bool changed = result[offset] < pixels[offset];
        if (!changed) {
            EXPECT_FLOAT_EQ(result[offset + 1], pixels[offset + 1]);
            EXPECT_FLOAT_EQ(result[offset + 2], pixels[offset + 2]);
            continue;
        }
        observedDust = true;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_GE(result[offset + channel], 0.0f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_LT(result[offset + channel], pixels[offset + channel])
                << "pixel=" << pixel << " channel=" << channel;
        }
    }
    EXPECT_TRUE(observedDust);
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
     FilmGrungeScratchCountCrossesIntegerThresholdAtHalfAmount)
{
    constexpr int width = 36;
    constexpr int height = 64;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    constexpr float sourceValue = 0.5f;
    constexpr float alpha = 0.7f;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    const auto renderWithScratch = [&source](const float amount) {
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setPropertyValue(UniString("grainAmount"), QVariant(0.0f));
        effect.setPropertyValue(UniString("scratchAmount"), QVariant(amount));
        effect.setPropertyValue(UniString("dustAmount"), QVariant(0.0f));
        effect.setPropertyValue(UniString("flicker"), QVariant(0.0f));
        effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
        effect.setPropertyValue(UniString("vignette"), QVariant(0.0f));
        effect.setPropertyValue(UniString("stain"), QVariant(0.0f));
        effect.setPropertyValue(UniString("seed"), QVariant(43));
        EffectContext context;
        context.compositionFrame = 8;
        effect.setContext(context);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };
    const auto belowThreshold = renderWithScratch(0.49f);
    const auto atThreshold = renderWithScratch(0.5f);

    const float* belowPixels = belowThreshold.image().rgba32fData();
    const float* thresholdPixels = atThreshold.image().rgba32fData();
    ASSERT_NE(belowPixels, nullptr);
    ASSERT_NE(thresholdPixels, nullptr);
    std::size_t changedPixels = 0;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(belowPixels[offset + channel], sourceValue)
                << "0.49 * (36 / 18) truncates to zero scratches";
            if (thresholdPixels[offset + channel] != sourceValue) {
                changedPixels += channel == 1 ? 1u : 0u;
            }
        }
        EXPECT_FLOAT_EQ(belowPixels[offset + 3], alpha);
        EXPECT_FLOAT_EQ(thresholdPixels[offset + 3], alpha);
    }
    EXPECT_GT(changedPixels, 0u)
        << "0.5 * (36 / 18) creates one three-pixel-wide scratch";
    EXPECT_LE(changedPixels, static_cast<std::size_t>(height) * 3);
}

TEST(LofiEffectContractTest,
     FilmGrungeScratchOnSingleColumnStaysInBoundsAndPreservesAlpha)
{
    constexpr int width = 1;
    constexpr int height = 13;
    constexpr float alpha = 0.55f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4 + 0] = 0.6f;
        pixels[pixel * 4 + 1] = 0.6f;
        pixels[pixel * 4 + 2] = 0.6f;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.0f));
    effect.setPropertyValue(UniString("scratchAmount"), QVariant(1.0f));
    effect.setPropertyValue(UniString("dustAmount"), QVariant(0.0f));
    effect.setPropertyValue(UniString("flicker"), QVariant(0.0f));
    effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    effect.setPropertyValue(UniString("vignette"), QVariant(0.0f));
    effect.setPropertyValue(UniString("stain"), QVariant(0.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(947));
    EffectContext context;
    context.compositionFrame = 6;
    effect.setContext(context);

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    EXPECT_EQ(output.image().width(), width);
    EXPECT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    bool observedScratch = false;
    for (int y = 0; y < height; ++y) {
        const std::size_t offset = static_cast<std::size_t>(y) * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(result[offset + channel]))
                << "y=" << y << " channel=" << channel;
            EXPECT_GE(result[offset + channel], pixels[offset + channel])
                << "scratch damage brightens narrow input";
            observedScratch = observedScratch ||
                result[offset + channel] > pixels[offset + channel];
        }
        EXPECT_FLOAT_EQ(result[offset + 3], alpha) << "y=" << y;
    }
    EXPECT_TRUE(observedScratch)
        << "the one generated scratch column is clipped onto the image column";
}

TEST(LofiEffectContractTest,
     FilmGrungeAnimatesScratchAmountWithFixedColorTransforms)
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
    for (const char* property : {"dustAmount", "flicker", "gateWeave"}) {
        animatedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    animatedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    animatedEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    animatedEffect.setPropertyValue(UniString("vignette"), QVariant(0.2f));
    animatedEffect.setPropertyValue(UniString("stain"), QVariant(0.15f));
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
    for (const char* property : {"dustAmount", "flicker", "gateWeave"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    fixedEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    fixedEffect.setPropertyValue(UniString("vignette"), QVariant(0.2f));
    fixedEffect.setPropertyValue(UniString("stain"), QVariant(0.15f));
    ImageF32x4RGBAWithCache noScratchFrame;
    fixedEffect.setPropertyValue(UniString("scratchAmount"), QVariant(0.0f));
    fixedEffect.setContext(context);
    fixedEffect.applyCPUOnly(source, noScratchFrame);

    fixedEffect.setPropertyValue(UniString("scratchAmount"), QVariant(0.5f));
    ImageF32x4RGBAWithCache fixedFrame;
    fixedEffect.applyCPUOnly(source, fixedFrame);

    const float* animated = animatedFrame.image().rgba32fData();
    const float* noScratch = noScratchFrame.image().rgba32fData();
    const float* fixed = fixedFrame.image().rgba32fData();
    ASSERT_NE(animated, nullptr);
    ASSERT_NE(noScratch, nullptr);
    ASSERT_NE(fixed, nullptr);
    std::array<bool, width> changedColumns{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(animated[offset], fixed[offset])
                << "pixel=" << pixel << " channel=" << channel;
        }
        if (animated[pixel * 4 + 1] != noScratch[pixel * 4 + 1]) {
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
     FilmGrungeDustCountScalesWithWhole900PixelAreas)
{
    constexpr int width = 90;
    constexpr int height = 100;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    constexpr float sourceValue = 0.5f;
    constexpr float alpha = 0.7f;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    const auto renderWithDust = [&source](const float amount) {
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setPropertyValue(UniString("grainAmount"), QVariant(0.0f));
        effect.setPropertyValue(UniString("scratchAmount"), QVariant(0.0f));
        effect.setPropertyValue(UniString("dustAmount"), QVariant(amount));
        effect.setPropertyValue(UniString("flicker"), QVariant(0.0f));
        effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
        effect.setPropertyValue(UniString("vignette"), QVariant(0.0f));
        effect.setPropertyValue(UniString("stain"), QVariant(0.0f));
        effect.setPropertyValue(UniString("seed"), QVariant(91));
        EffectContext context;
        context.compositionFrame = 7;
        effect.setContext(context);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };
    const auto belowFirstDust = renderWithDust(0.0999f);
    const auto scaledDust = renderWithDust(0.2f);

    const float* belowPixels = belowFirstDust.image().rgba32fData();
    const float* scaledPixels = scaledDust.image().rgba32fData();
    ASSERT_NE(belowPixels, nullptr);
    ASSERT_NE(scaledPixels, nullptr);
    bool observedScaledDust = false;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(belowPixels[offset + channel], sourceValue)
                << "amount below one yields zero whole particles; pixel=" << pixel;
            EXPECT_LE(scaledPixels[offset + channel], sourceValue)
                << "dust only darkens; pixel=" << pixel;
            observedScaledDust = observedScaledDust ||
                scaledPixels[offset + channel] < sourceValue;
        }
        EXPECT_FLOAT_EQ(belowPixels[offset + 3], alpha);
        EXPECT_FLOAT_EQ(scaledPixels[offset + 3], alpha);
    }
    EXPECT_TRUE(observedScaledDust)
        << "a 9,000-pixel image at amount 0.2 creates two dust particles";
}

TEST(LofiEffectContractTest,
     FilmGrungeDustAmountAddsMonotonicDamageAndClampsAtItsBounds)
{
    constexpr int width = 128;
    constexpr int height = 96;
    constexpr float sourceValue = 0.5f;
    constexpr float alpha = 0.7f;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    EffectContext context;
    context.compositionFrame = 12;
    const auto renderWithDustAmount = [&](const QVariant& amount) {
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setContext(context);
        for (const char* property : {"grainAmount", "scratchAmount", "flicker",
                                     "gateWeave", "vignette", "stain"}) {
            effect.setPropertyValue(UniString(property), QVariant(0.0f));
        }
        effect.setPropertyValue(UniString("seed"), QVariant(1977));
        effect.setPropertyValue(UniString("dustAmount"), amount);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };

    const auto noDust = renderWithDustAmount(QVariant(0.0f));
    const auto halfDust = renderWithDustAmount(QVariant(0.5f));
    const auto maximumDust = renderWithDustAmount(QVariant(1.0f));
    const auto clampedLow = renderWithDustAmount(QVariant(-1.0f));
    const auto clampedHigh = renderWithDustAmount(QVariant(2.0f));
    const auto nonFinite = renderWithDustAmount(
        QVariant(std::numeric_limits<double>::quiet_NaN()));
    const float* noDustPixels = noDust.image().rgba32fData();
    const float* halfDustPixels = halfDust.image().rgba32fData();
    const float* maximumDustPixels = maximumDust.image().rgba32fData();
    const float* clampedLowPixels = clampedLow.image().rgba32fData();
    const float* clampedHighPixels = clampedHigh.image().rgba32fData();
    const float* nonFinitePixels = nonFinite.image().rgba32fData();
    ASSERT_NE(noDustPixels, nullptr);
    ASSERT_NE(halfDustPixels, nullptr);
    ASSERT_NE(maximumDustPixels, nullptr);
    ASSERT_NE(clampedLowPixels, nullptr);
    ASSERT_NE(clampedHighPixels, nullptr);
    ASSERT_NE(nonFinitePixels, nullptr);
    bool observedDust = false;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const float noDustValue = noDustPixels[offset + channel];
            const float halfDustValue = halfDustPixels[offset + channel];
            const float maximumDustValue = maximumDustPixels[offset + channel];
            EXPECT_FLOAT_EQ(noDustValue, sourceValue)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_LE(halfDustValue, noDustValue)
                << "half dust brightened pixel=" << pixel
                << " channel=" << channel;
            EXPECT_LE(maximumDustValue, halfDustValue)
                << "maximum dust brightened pixel=" << pixel
                << " channel=" << channel;
            EXPECT_FLOAT_EQ(clampedLowPixels[offset + channel], noDustValue)
                << "low clamp pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(nonFinitePixels[offset + channel], noDustValue)
                << "non-finite reset pixel=" << pixel
                << " channel=" << channel;
            EXPECT_FLOAT_EQ(clampedHighPixels[offset + channel],
                            maximumDustValue)
                << "high clamp pixel=" << pixel << " channel=" << channel;
            observedDust = observedDust || maximumDustValue < noDustValue;
        }
        EXPECT_FLOAT_EQ(noDustPixels[offset + 3], alpha);
        EXPECT_FLOAT_EQ(halfDustPixels[offset + 3], alpha);
        EXPECT_FLOAT_EQ(maximumDustPixels[offset + 3], alpha);
    }
    EXPECT_TRUE(observedDust);
}

TEST(LofiEffectContractTest,
     FilmGrungeDustOnTinySurfaceStaysBoundedAndPreservesAlpha)
{
    constexpr int width = 2;
    constexpr int height = 3;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    constexpr float alpha = 0.45f;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = 0.7f;
        pixels[pixel * 4 + 1] = 0.7f;
        pixels[pixel * 4 + 2] = 0.7f;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.0f));
    effect.setPropertyValue(UniString("scratchAmount"), QVariant(0.0f));
    effect.setPropertyValue(UniString("dustAmount"), QVariant(1.0f));
    effect.setPropertyValue(UniString("flicker"), QVariant(0.0f));
    effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    effect.setPropertyValue(UniString("vignette"), QVariant(0.0f));
    effect.setPropertyValue(UniString("stain"), QVariant(0.0f));
    effect.setPropertyValue(UniString("seed"), QVariant(709));
    EffectContext context;
    context.compositionFrame = 5;
    effect.setContext(context);

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    EXPECT_EQ(output.image().width(), width);
    EXPECT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    bool observedDust = false;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(result[offset + channel]))
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_LE(result[offset + channel], pixels[offset + channel])
                << "dust should not brighten tiny surface pixels";
            observedDust = observedDust ||
                result[offset + channel] < pixels[offset + channel];
        }
        EXPECT_FLOAT_EQ(result[offset + 3], alpha) << "pixel=" << pixel;
    }
    EXPECT_TRUE(observedDust)
        << "the single generated particle overlaps this tiny surface";
}

TEST(LofiEffectContractTest,
     FilmGrungeAnimatesDustWithFixedColorTransforms)
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
    for (const char* property : {"scratchAmount", "flicker", "gateWeave"}) {
        animatedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    animatedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    animatedEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    animatedEffect.setPropertyValue(UniString("vignette"), QVariant(0.2f));
    animatedEffect.setPropertyValue(UniString("stain"), QVariant(0.15f));
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
    for (const char* property : {"scratchAmount", "flicker", "gateWeave"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    fixedEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    fixedEffect.setPropertyValue(UniString("vignette"), QVariant(0.2f));
    fixedEffect.setPropertyValue(UniString("stain"), QVariant(0.15f));
    ImageF32x4RGBAWithCache noDustFrame;
    fixedEffect.setPropertyValue(UniString("dustAmount"), QVariant(0.0f));
    fixedEffect.setContext(context);
    fixedEffect.applyCPUOnly(source, noDustFrame);

    fixedEffect.setPropertyValue(UniString("dustAmount"), QVariant(0.5f));
    ImageF32x4RGBAWithCache fixedFrame;
    fixedEffect.applyCPUOnly(source, fixedFrame);

    const float* animated = animatedFrame.image().rgba32fData();
    const float* noDust = noDustFrame.image().rgba32fData();
    const float* fixed = fixedFrame.image().rgba32fData();
    ASSERT_NE(animated, nullptr);
    ASSERT_NE(noDust, nullptr);
    ASSERT_NE(fixed, nullptr);
    std::size_t changedPixels = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 4; ++channel) {
            EXPECT_FLOAT_EQ(animated[offset + channel], fixed[offset + channel])
                << "pixel=" << pixel << " channel=" << channel;
        }
        const float redDelta = animated[offset] - noDust[offset];
        const float greenDelta = animated[offset + 1] - noDust[offset + 1];
        const float blueDelta = animated[offset + 2] - noDust[offset + 2];
        EXPECT_NEAR(redDelta, greenDelta, 1.0e-6f)
            << "dust delta should be shared by RGB at pixel=" << pixel;
        EXPECT_NEAR(greenDelta, blueDelta, 1.0e-6f)
            << "dust delta should be shared by RGB at pixel=" << pixel;
        EXPECT_FLOAT_EQ(animated[offset + 3], alpha) << "pixel=" << pixel;
        if (greenDelta < -1.0e-6f) {
            ++changedPixels;
            EXPECT_LT(animated[offset + 1], noDust[offset + 1])
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
     FilmGrungeFlickerScalesExposureAndClampsAtItsPropertyBounds)
{
    constexpr int width = 12;
    constexpr int height = 8;
    constexpr float sourceValue = 0.4f;
    constexpr float alpha = 0.65f;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    EffectContext context;
    context.compositionFrame = 12;
    const auto renderWithFlicker = [&](const QVariant& value) {
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setContext(context);
        for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                     "gateWeave", "vignette", "stain"}) {
            effect.setPropertyValue(UniString(property), QVariant(0.0f));
        }
        effect.setPropertyValue(UniString("flicker"), value);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };

    const auto noFlicker = renderWithFlicker(QVariant(0.0f));
    const auto halfFlicker = renderWithFlicker(QVariant(0.25f));
    const auto maximumFlicker = renderWithFlicker(QVariant(0.5f));
    const auto clampedLow = renderWithFlicker(QVariant(-1.0f));
    const auto clampedHigh = renderWithFlicker(QVariant(1.0f));
    const auto nonFinite = renderWithFlicker(
        QVariant(std::numeric_limits<double>::quiet_NaN()));
    const float* noFlickerPixels = noFlicker.image().rgba32fData();
    const float* halfFlickerPixels = halfFlicker.image().rgba32fData();
    const float* maximumFlickerPixels = maximumFlicker.image().rgba32fData();
    const float* clampedLowPixels = clampedLow.image().rgba32fData();
    const float* clampedHighPixels = clampedHigh.image().rgba32fData();
    const float* nonFinitePixels = nonFinite.image().rgba32fData();
    ASSERT_NE(noFlickerPixels, nullptr);
    ASSERT_NE(halfFlickerPixels, nullptr);
    ASSERT_NE(maximumFlickerPixels, nullptr);
    ASSERT_NE(clampedLowPixels, nullptr);
    ASSERT_NE(clampedHighPixels, nullptr);
    ASSERT_NE(nonFinitePixels, nullptr);
    EXPECT_FLOAT_EQ(noFlickerPixels[0], sourceValue);
    EXPECT_NE(maximumFlickerPixels[0], sourceValue);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(noFlickerPixels[offset + channel], sourceValue);
            EXPECT_FLOAT_EQ(clampedLowPixels[offset + channel],
                            noFlickerPixels[offset + channel]);
            EXPECT_FLOAT_EQ(nonFinitePixels[offset + channel],
                            noFlickerPixels[offset + channel]);
            EXPECT_FLOAT_EQ(clampedHighPixels[offset + channel],
                            maximumFlickerPixels[offset + channel]);
            EXPECT_NEAR(halfFlickerPixels[offset + channel],
                        noFlickerPixels[offset + channel] +
                            (maximumFlickerPixels[offset + channel] -
                             noFlickerPixels[offset + channel]) * 0.5f,
                        1.0e-6f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(maximumFlickerPixels[offset + channel],
                            maximumFlickerPixels[channel])
                << "maximum exposure must be uniform pixel=" << pixel
                << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(noFlickerPixels[offset + 3], alpha);
        EXPECT_FLOAT_EQ(halfFlickerPixels[offset + 3], alpha);
        EXPECT_FLOAT_EQ(maximumFlickerPixels[offset + 3], alpha);
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
     FilmGrungeStainStrengthScalesItsSpatialPatternAndClampsBounds)
{
    constexpr int width = 32;
    constexpr int height = 24;
    constexpr float sourceValue = 0.4f;
    constexpr float alpha = 0.65f;
    constexpr int seed = 1977;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    EffectContext context;
    context.compositionFrame = 12;
    const auto renderWithStain = [&](const QVariant& value) {
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setContext(context);
        for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                     "flicker", "gateWeave", "vignette"}) {
            effect.setPropertyValue(UniString(property), QVariant(0.0f));
        }
        effect.setPropertyValue(UniString("seed"), QVariant(seed));
        effect.setPropertyValue(UniString("stain"), value);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };

    const auto noStain = renderWithStain(QVariant(0.0f));
    const auto halfStain = renderWithStain(QVariant(0.25f));
    const auto maximumStain = renderWithStain(QVariant(0.5f));
    const auto clampedLow = renderWithStain(QVariant(-1.0f));
    const auto clampedHigh = renderWithStain(QVariant(1.0f));
    const auto nonFinite = renderWithStain(
        QVariant(std::numeric_limits<double>::quiet_NaN()));
    const float* noStainPixels = noStain.image().rgba32fData();
    const float* halfStainPixels = halfStain.image().rgba32fData();
    const float* maximumStainPixels = maximumStain.image().rgba32fData();
    const float* clampedLowPixels = clampedLow.image().rgba32fData();
    const float* clampedHighPixels = clampedHigh.image().rgba32fData();
    const float* nonFinitePixels = nonFinite.image().rgba32fData();
    ASSERT_NE(noStainPixels, nullptr);
    ASSERT_NE(halfStainPixels, nullptr);
    ASSERT_NE(maximumStainPixels, nullptr);
    ASSERT_NE(clampedLowPixels, nullptr);
    ASSERT_NE(clampedHighPixels, nullptr);
    ASSERT_NE(nonFinitePixels, nullptr);
    bool observedSpatialVariation = false;
    for (int y = 0; y < height; ++y) {
        const float ny = (2.0f * y / (height - 1)) - 1.0f;
        for (int x = 0; x < width; ++x) {
            const float nx = (2.0f * x / (width - 1)) - 1.0f;
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float stainPattern = std::sin(
                nx * 4.7f + ny * 2.9f + static_cast<float>(seed));
            const float expectedMaximum =
                sourceValue * (1.0f + 0.5f * stainPattern);
            for (std::size_t channel = 0; channel < 3; ++channel) {
                EXPECT_FLOAT_EQ(noStainPixels[offset + channel], sourceValue)
                    << "x=" << x << " y=" << y << " channel=" << channel;
                EXPECT_FLOAT_EQ(clampedLowPixels[offset + channel],
                                noStainPixels[offset + channel]);
                EXPECT_FLOAT_EQ(nonFinitePixels[offset + channel],
                                noStainPixels[offset + channel]);
                EXPECT_FLOAT_EQ(clampedHighPixels[offset + channel],
                                maximumStainPixels[offset + channel]);
                EXPECT_NEAR(maximumStainPixels[offset + channel],
                            expectedMaximum, 1.0e-6f)
                    << "maximum stain x=" << x << " y=" << y
                    << " channel=" << channel;
                EXPECT_NEAR(halfStainPixels[offset + channel],
                            sourceValue +
                                (maximumStainPixels[offset + channel] -
                                 sourceValue) * 0.5f,
                            1.0e-6f)
                    << "half stain x=" << x << " y=" << y
                    << " channel=" << channel;
                observedSpatialVariation = observedSpatialVariation ||
                    std::abs(maximumStainPixels[offset + channel] -
                             sourceValue) > 1.0e-5f;
            }
            EXPECT_FLOAT_EQ(noStainPixels[offset + 3], alpha);
            EXPECT_FLOAT_EQ(halfStainPixels[offset + 3], alpha);
            EXPECT_FLOAT_EQ(maximumStainPixels[offset + 3], alpha);
        }
    }
    EXPECT_TRUE(observedSpatialVariation);
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
     FilmGrungeVignetteStrengthScalesTheRadialProfileAndClampsBounds)
{
    constexpr int width = 17;
    constexpr int height = 13;
    constexpr float sourceValue = 0.4f;
    constexpr float alpha = 0.65f;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = sourceValue;
        pixels[pixel * 4 + 1] = sourceValue;
        pixels[pixel * 4 + 2] = sourceValue;
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    EffectContext context;
    context.compositionFrame = 12;
    const auto renderWithVignette = [&](const QVariant& value) {
        ArtifactFilmGrungeEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setContext(context);
        for (const char* property : {"grainAmount", "scratchAmount", "dustAmount",
                                     "flicker", "gateWeave", "stain"}) {
            effect.setPropertyValue(UniString(property), QVariant(0.0f));
        }
        effect.setPropertyValue(UniString("vignette"), value);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };

    const auto noVignette = renderWithVignette(QVariant(0.0f));
    const auto halfVignette = renderWithVignette(QVariant(0.5f));
    const auto maximumVignette = renderWithVignette(QVariant(1.0f));
    const auto clampedLow = renderWithVignette(QVariant(-1.0f));
    const auto clampedHigh = renderWithVignette(QVariant(2.0f));
    const auto nonFinite = renderWithVignette(
        QVariant(std::numeric_limits<double>::quiet_NaN()));
    const float* noVignettePixels = noVignette.image().rgba32fData();
    const float* halfVignettePixels = halfVignette.image().rgba32fData();
    const float* maximumVignettePixels = maximumVignette.image().rgba32fData();
    const float* clampedLowPixels = clampedLow.image().rgba32fData();
    const float* clampedHighPixels = clampedHigh.image().rgba32fData();
    const float* nonFinitePixels = nonFinite.image().rgba32fData();
    ASSERT_NE(noVignettePixels, nullptr);
    ASSERT_NE(halfVignettePixels, nullptr);
    ASSERT_NE(maximumVignettePixels, nullptr);
    ASSERT_NE(clampedLowPixels, nullptr);
    ASSERT_NE(clampedHighPixels, nullptr);
    ASSERT_NE(nonFinitePixels, nullptr);
    bool observedDarkening = false;
    for (int y = 0; y < height; ++y) {
        const float ny = (2.0f * y / (height - 1)) - 1.0f;
        for (int x = 0; x < width; ++x) {
            const float nx = (2.0f * x / (width - 1)) - 1.0f;
            const float radiusPower = std::pow(nx * nx + ny * ny, 1.35f);
            const float maximumFactor =
                std::clamp(1.0f - radiusPower, 0.0f, 1.0f);
            const float halfFactor =
                std::clamp(1.0f - 0.5f * radiusPower, 0.0f, 1.0f);
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const float expectedMaximum = sourceValue * maximumFactor;
                const float expectedHalf = sourceValue * halfFactor;
                EXPECT_FLOAT_EQ(noVignettePixels[offset + channel], sourceValue)
                    << "x=" << x << " y=" << y << " channel=" << channel;
                EXPECT_FLOAT_EQ(clampedLowPixels[offset + channel],
                                noVignettePixels[offset + channel]);
                EXPECT_FLOAT_EQ(nonFinitePixels[offset + channel],
                                noVignettePixels[offset + channel]);
                EXPECT_FLOAT_EQ(clampedHighPixels[offset + channel],
                                maximumVignettePixels[offset + channel]);
                EXPECT_NEAR(maximumVignettePixels[offset + channel],
                            expectedMaximum, 1.0e-6f)
                    << "maximum vignette x=" << x << " y=" << y
                    << " channel=" << channel;
                EXPECT_NEAR(halfVignettePixels[offset + channel],
                            expectedHalf, 1.0e-6f)
                    << "half vignette x=" << x << " y=" << y
                    << " channel=" << channel;
                EXPECT_LE(maximumVignettePixels[offset + channel],
                          noVignettePixels[offset + channel]);
                observedDarkening = observedDarkening ||
                    maximumVignettePixels[offset + channel] < sourceValue;
            }
            EXPECT_FLOAT_EQ(noVignettePixels[offset + 3], alpha);
            EXPECT_FLOAT_EQ(halfVignettePixels[offset + 3], alpha);
            EXPECT_FLOAT_EQ(maximumVignettePixels[offset + 3], alpha);
        }
    }
    EXPECT_FLOAT_EQ(maximumVignettePixels[3], alpha);
    EXPECT_FLOAT_EQ(maximumVignettePixels[
                        (static_cast<std::size_t>(height / 2) * width +
                         width / 2) * 4],
                    sourceValue);
    EXPECT_FLOAT_EQ(maximumVignettePixels[0], 0.0f)
        << "maximum radial falloff clamps the farthest corner to black";
    EXPECT_TRUE(observedDarkening);
}

TEST(LofiEffectContractTest,
     FilmGrungeCombinesGrainVignetteAndStainInStableOrder)
{
    constexpr int width = 5;
    constexpr int height = 4;
    constexpr float grainAmount = 0.08f;
    constexpr float vignette = 0.45f;
    constexpr float stain = 0.3f;
    constexpr int seed = 997;
    constexpr std::int64_t frame = 6;
    constexpr std::array<float, 3> sourceRgb{0.3f, 0.45f, 0.6f};
    constexpr std::array<float, 3> grainWeights{0.92f, 1.0f, 1.05f};
    constexpr float alpha = 0.4f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        pixels[pixel * 4 + 0] = sourceRgb[0];
        pixels[pixel * 4 + 1] = sourceRgb[1];
        pixels[pixel * 4 + 2] = sourceRgb[2];
        pixels[pixel * 4 + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ArtifactFilmGrungeEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setPropertyValue(UniString("grainAmount"), QVariant(grainAmount));
    effect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    effect.setPropertyValue(UniString("vignette"), QVariant(vignette));
    effect.setPropertyValue(UniString("stain"), QVariant(stain));
    effect.setPropertyValue(UniString("seed"), QVariant(seed));
    for (const char* property : {"scratchAmount", "dustAmount", "flicker",
                                 "gateWeave"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    EffectContext context;
    context.compositionFrame = frame;
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    const std::uint32_t frameSeed = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(seed) * 1664525ull +
        static_cast<std::uint64_t>(frame) * 1013904223ull);
    for (int y = 0; y < height; ++y) {
        const float ny = (2.0f * y / (height - 1)) - 1.0f;
        for (int x = 0; x < width; ++x) {
            const float nx = (2.0f * x / (width - 1)) - 1.0f;
            const std::uint32_t blockHash = frameSeed ^
                static_cast<std::uint32_t>(x * 73856093) ^
                static_cast<std::uint32_t>(y * 19349663);
            const float grain =
                (static_cast<float>(blockHash & 0xffffu) / 32767.5f - 1.0f) *
                grainAmount;
            const float vignetteFactor = std::clamp(
                1.0f - vignette * std::pow(nx * nx + ny * ny, 1.35f),
                0.0f, 1.0f);
            const float stainFactor = 1.0f + stain *
                std::sin(nx * 4.7f + ny * 2.9f + static_cast<float>(seed));
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (std::size_t channel = 0; channel < sourceRgb.size(); ++channel) {
                const float expected = sourceRgb[channel] * vignetteFactor *
                    stainFactor + grain * grainWeights[channel];
                EXPECT_NEAR(result[offset + channel], expected, 1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
            }
            EXPECT_FLOAT_EQ(result[offset + 3], alpha)
                << "x=" << x << " y=" << y;
        }
    }
}

TEST(LofiEffectContractTest,
     FilmGrungeZeroGateWeavePreservesEverySourceSample)
{
    constexpr int width = 7;
    constexpr int height = 5;
    constexpr std::size_t channelCount =
        static_cast<std::size_t>(width) * height * 4;
    std::array<float, channelCount> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>(y * width + x) * 4;
            pixels[offset + 0] = 0.08f + 0.07f * x;
            pixels[offset + 1] = 0.12f + 0.11f * y;
            pixels[offset + 2] = 0.9f - 0.06f * (x + y);
            pixels[offset + 3] = (x + 2 * y) % 4 == 0 ? 0.0f : 0.25f * (x + 1);
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
    effect.setPropertyValue(UniString("gateWeave"), QVariant(0.0f));
    EffectContext context;
    context.compositionFrame = 33;
    effect.setContext(context);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(LofiEffectContractTest,
     FilmGrungeInPlaceProcessingMatchesOutOfPlaceAndPreservesAlpha)
{
    constexpr int width = 31;
    constexpr int height = 19;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>(y * width + x) * 4;
            pixels[offset + 0] = 0.15f + 0.01f * x;
            pixels[offset + 1] = 0.2f + 0.015f * y;
            pixels[offset + 2] = 0.8f - 0.01f * x;
            pixels[offset + 3] = (x + y) % 3 == 0 ? 0.35f : 0.9f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache outOfPlace;
    ImageF32x4RGBAWithCache inPlace(image);
    EffectContext context;
    context.compositionFrame = 27;
    ArtifactFilmGrungeEffect outOfPlaceEffect;
    ArtifactFilmGrungeEffect inPlaceEffect;
    for (ArtifactFilmGrungeEffect* effect :
         {&outOfPlaceEffect, &inPlaceEffect}) {
        effect->setComputeMode(ComputeMode::CPU);
        effect->setContext(context);
        effect->setPropertyValue(UniString("grainAmount"), QVariant(0.18f));
        effect->setPropertyValue(UniString("scratchAmount"), QVariant(0.75f));
        effect->setPropertyValue(UniString("dustAmount"), QVariant(0.8f));
        effect->setPropertyValue(UniString("flicker"), QVariant(0.3f));
        effect->setPropertyValue(UniString("gateWeave"), QVariant(1.5f));
        effect->setPropertyValue(UniString("vignette"), QVariant(0.35f));
        effect->setPropertyValue(UniString("stain"), QVariant(0.6f));
        effect->setPropertyValue(UniString("seed"), QVariant(23));
    }
    outOfPlaceEffect.applyCPUOnly(source, outOfPlace);
    inPlaceEffect.applyCPUOnly(inPlace, inPlace);

    const float* expected = outOfPlace.image().rgba32fData();
    const float* actual = inPlace.image().rgba32fData();
    ASSERT_NE(expected, nullptr);
    ASSERT_NE(actual, nullptr);
    bool observedEffect = false;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(actual[offset + channel], expected[offset + channel])
                << "pixel=" << pixel << " channel=" << channel;
            observedEffect = observedEffect ||
                std::abs(actual[offset + channel] - pixels[offset + channel]) >
                    1.0e-5f;
        }
        EXPECT_FLOAT_EQ(actual[offset + 3], pixels[offset + 3])
            << "alpha must be copied unchanged at pixel=" << pixel;
    }
    EXPECT_TRUE(observedEffect);
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
                                 "gateWeave"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("vignette"), QVariant(0.2f));
    effect.setPropertyValue(UniString("stain"), QVariant(0.15f));
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
                                 "gateWeave"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("vignette"), QVariant(0.2f));
    fixedEffect.setPropertyValue(UniString("stain"), QVariant(0.15f));
    fixedEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    ImageF32x4RGBAWithCache firstFixedFrame;
    fixedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.0f));
    context.compositionFrame = 0;
    fixedEffect.setContext(context);
    fixedEffect.applyCPUOnly(source, firstFixedFrame);

    fixedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.4f));
    context.compositionFrame = 30;
    fixedEffect.setContext(context);
    ImageF32x4RGBAWithCache fixedFrame;
    fixedEffect.applyCPUOnly(source, fixedFrame);

    context.compositionFrame = 15;
    fixedEffect.setContext(context);
    fixedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    ImageF32x4RGBAWithCache interpolatedFixedFrame;
    fixedEffect.applyCPUOnly(source, interpolatedFixedFrame);

    const float* firstPixels = firstFrame.image().rgba32fData();
    const float* interpolatedPixels = interpolatedFrame.image().rgba32fData();
    const float* animatedPixels = animatedFrame.image().rgba32fData();
    const float* firstFixedPixels = firstFixedFrame.image().rgba32fData();
    const float* fixedPixels = fixedFrame.image().rgba32fData();
    const float* interpolatedFixedPixels =
        interpolatedFixedFrame.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(interpolatedPixels, nullptr);
    ASSERT_NE(animatedPixels, nullptr);
    ASSERT_NE(firstFixedPixels, nullptr);
    ASSERT_NE(fixedPixels, nullptr);
    ASSERT_NE(interpolatedFixedPixels, nullptr);
    std::size_t changedRgbSamples = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(firstPixels[offset], firstFixedPixels[offset])
                << "first frame pixel=" << pixel << " channel=" << channel;
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
    for (const char* property : {"scratchAmount", "dustAmount",
                                 "flicker", "gateWeave"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    effect.setPropertyValue(UniString("vignette"), QVariant(0.15f));
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
    for (const char* property : {"scratchAmount", "dustAmount",
                                 "flicker", "gateWeave"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    fixedEffect.setPropertyValue(UniString("vignette"), QVariant(0.15f));
    fixedEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    ImageF32x4RGBAWithCache firstFixedFrame;
    context.compositionFrame = 0;
    fixedEffect.setContext(context);
    fixedEffect.applyCPUOnly(source, firstFixedFrame);

    fixedEffect.setPropertyValue(UniString("stain"), QVariant(0.4f));
    context.compositionFrame = 30;
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
    const float* firstFixedPixels = firstFixedFrame.image().rgba32fData();
    const float* fixedPixels = fixedFrame.image().rgba32fData();
    const float* interpolatedFixedPixels =
        interpolatedFixedFrame.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(interpolatedPixels, nullptr);
    ASSERT_NE(animatedPixels, nullptr);
    ASSERT_NE(firstFixedPixels, nullptr);
    ASSERT_NE(fixedPixels, nullptr);
    ASSERT_NE(interpolatedFixedPixels, nullptr);
    std::size_t changedRgbSamples = 0;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(firstPixels[offset], firstFixedPixels[offset])
                << "first frame pixel=" << pixel << " channel=" << channel;
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
                                 "flicker", "gateWeave"}) {
        effect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    effect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    effect.setPropertyValue(UniString("stain"), QVariant(0.15f));
    effect.setPropertyValue(UniString("seed"), QVariant(1977));
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
                                 "flicker", "gateWeave"}) {
        fixedEffect.setPropertyValue(UniString(property), QVariant(0.0f));
    }
    fixedEffect.setPropertyValue(UniString("grainAmount"), QVariant(0.2f));
    fixedEffect.setPropertyValue(UniString("stain"), QVariant(0.15f));
    fixedEffect.setPropertyValue(UniString("seed"), QVariant(1977));
    fixedEffect.setPropertyValue(UniString("grainSize"), QVariant(1.0f));
    fixedEffect.setPropertyValue(UniString("vignette"), QVariant(0.25f));
    ImageF32x4RGBAWithCache firstFixedFrame;
    context.compositionFrame = 0;
    fixedEffect.setContext(context);
    fixedEffect.applyCPUOnly(source, firstFixedFrame);

    ImageF32x4RGBAWithCache fixedFrame;
    context.compositionFrame = 30;
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
    const float* firstFixedPixels = firstFixedFrame.image().rgba32fData();
    const float* fixedPixels = fixedFrame.image().rgba32fData();
    const float* interpolatedFixedPixels =
        interpolatedFixedFrame.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(interpolatedPixels, nullptr);
    ASSERT_NE(animatedPixels, nullptr);
    ASSERT_NE(firstFixedPixels, nullptr);
    ASSERT_NE(fixedPixels, nullptr);
    ASSERT_NE(interpolatedFixedPixels, nullptr);
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(firstPixels[offset], firstFixedPixels[offset])
                << "first frame pixel=" << pixel << " channel=" << channel;
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
    const auto expectedRed = [](const int x, const int y, const int frame,
                                const float vignetteAmount) {
        constexpr float grainAmount = 0.2f;
        constexpr float stainAmount = 0.15f;
        constexpr int seed = 1977;
        const float nx = (2.0f * x / (width - 1)) - 1.0f;
        const float ny = (2.0f * y / (height - 1)) - 1.0f;
        const std::uint32_t frameSeed = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(seed) * 1664525ull +
            static_cast<std::uint64_t>(frame) * 1013904223ull);
        const std::uint32_t blockHash = frameSeed ^
            static_cast<std::uint32_t>(x * 73856093) ^
            static_cast<std::uint32_t>(y * 19349663);
        const float grain =
            (static_cast<float>(blockHash & 0xffffu) / 32767.5f - 1.0f) *
            grainAmount;
        const float vignetteFactor = std::clamp(
            1.0f - vignetteAmount * std::pow(nx * nx + ny * ny, 1.35f),
            0.0f, 1.0f);
        const float stainFactor = 1.0f + stainAmount *
            std::sin(nx * 4.7f + ny * 2.9f + static_cast<float>(seed));
        return sourceValue * vignetteFactor * stainFactor + grain * 0.92f;
    };
    EXPECT_NEAR(animatedPixels[center],
                expectedRed(width / 2, height / 2, 30, 0.25f), 1.0e-6f);
    EXPECT_NEAR(animatedPixels[0], expectedRed(0, 0, 30, 0.25f), 1.0e-6f);
    EXPECT_NEAR(interpolatedPixels[center],
                expectedRed(width / 2, height / 2, 15, 0.125f), 1.0e-6f);
    EXPECT_NEAR(interpolatedPixels[0], expectedRed(0, 0, 15, 0.125f),
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

TEST(LofiEffectContractTest, DisabledFilmGrungeIsAnExactPassthrough)
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
    ArtifactFilmGrungeEffect effect;
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

TEST(LofiEffectContractTest,
     OldTvInteriorRowsAreStableWhenImageWidthChanges)
{
    constexpr int narrowWidth = 48;
    constexpr int wideWidth = 64;
    constexpr int height = 17;
    std::array<float, narrowWidth * height * 4> narrowPixels{};
    std::array<float, wideWidth * height * 4> widePixels{};
    const auto fillPixels = [](auto& pixels, const int width) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t offset =
                    static_cast<std::size_t>((y * width + x) * 4);
                pixels[offset + 0] = 0.12f + 0.006f * x + 0.001f * y;
                pixels[offset + 1] = 0.24f + 0.008f * x + 0.002f * y;
                pixels[offset + 2] = 0.36f + 0.010f * x + 0.003f * y;
                pixels[offset + 3] = 0.1f + 0.01f * x + 0.005f * y;
            }
        }
    };
    fillPixels(narrowPixels, narrowWidth);
    fillPixels(widePixels, wideWidth);
    ImageF32x4_RGBA narrowImage;
    narrowImage.setFromRGBA32F(narrowPixels.data(), narrowWidth, height);
    ImageF32x4_RGBA wideImage;
    wideImage.setFromRGBA32F(widePixels.data(), wideWidth, height);
    const ImageF32x4RGBAWithCache narrowSource(narrowImage);
    const ImageF32x4RGBAWithCache wideSource(wideImage);
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache narrowOutput;
    ImageF32x4RGBAWithCache wideOutput;
    effect.applyCPUOnly(narrowSource, narrowOutput);
    effect.applyCPUOnly(wideSource, wideOutput);

    const float* narrowResult = narrowOutput.image().rgba32fData();
    const float* wideResult = wideOutput.image().rgba32fData();
    ASSERT_NE(narrowResult, nullptr);
    ASSERT_NE(wideResult, nullptr);
    for (int y = 0; y < height; ++y) {
        for (int x = 5; x < narrowWidth - 5; ++x) {
            const std::size_t narrowOffset =
                static_cast<std::size_t>((y * narrowWidth + x) * 4);
            const std::size_t wideOffset =
                static_cast<std::size_t>((y * wideWidth + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                EXPECT_FLOAT_EQ(narrowResult[narrowOffset + channel],
                                wideResult[wideOffset + channel])
                    << "x=" << x << " y=" << y << " channel=" << channel;
            }
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

TEST(LofiEffectContractTest,
     OldTvTransformsRgbIndependentlyOfAlphaIncludingTransparentPixels)
{
    constexpr int width = 5;
    constexpr int height = 8;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = 0.17f + 0.013f * static_cast<float>(pixel);
        pixels[pixel * 4 + 1] = 0.83f - 0.009f * static_cast<float>(pixel);
        pixels[pixel * 4 + 2] = 0.31f + 0.007f * static_cast<float>(pixel);
        pixels[pixel * 4 + 3] = pixel % 3 == 0 ? 0.0f :
                                 pixel % 3 == 1 ? 0.4f : 1.0f;
    }
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
    bool transparentRgbChanged = false;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = pixel * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(result[offset + channel]))
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_GE(result[offset + channel], 0.0f);
            EXPECT_LE(result[offset + channel], 1.0f);
            if (pixels[offset + 3] == 0.0f) {
                transparentRgbChanged = transparentRgbChanged ||
                    result[offset + channel] != pixels[offset + channel];
            }
        }
        EXPECT_FLOAT_EQ(result[offset + 3], pixels[offset + 3])
            << "pixel=" << pixel;
    }
    EXPECT_TRUE(transparentRgbChanged)
        << "RGB processing must be independent from zero alpha";
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

TEST(LofiEffectContractTest, OldTvCombinesMixRegionAndPrimaryMaskCoverage)
{
    const auto source = makePatternSurface();
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache combinedEffect;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, fullEffect);

    const QRectF region(2.0, 2.0, 4.0, 4.0);
    constexpr std::array<float, 4> maskPixel{0.0f, 0.0f, 0.0f, 0.5f};
    auto mask = makeShared<ImageF32x4_RGBA>();
    mask->setFromRGBA32F(maskPixel.data(), 1, 1);
    effect.setMix(0.6f);
    effect.setEffectRegion(region);
    effect.setMaskImage(mask);
    effect.setMaskEnabled(true);
    effect.applyCPUOnly(source, combinedEffect);

    const float* original = source.image().rgba32fData();
    const float* full = fullEffect.image().rgba32fData();
    const float* combined = combinedEffect.image().rgba32fData();
    ASSERT_NE(original, nullptr);
    ASSERT_NE(full, nullptr);
    ASSERT_NE(combined, nullptr);
    bool observedEffectInsideCoverage = false;
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const bool inside = region.contains(QPointF(x + 0.5, y + 0.5));
            constexpr float coverageInside = 0.6f * 0.5f;
            const float coverage = inside ? coverageInside : 0.0f;
            const std::size_t offset =
                static_cast<std::size_t>((y * source.width() + x) * 4);
            for (int channel = 0; channel < 4; ++channel) {
                if (inside && channel < 3) {
                    observedEffectInsideCoverage =
                        observedEffectInsideCoverage ||
                        std::abs(full[offset + channel] -
                                 original[offset + channel]) > 1.0e-5f;
                }
                EXPECT_NEAR(combined[offset + channel],
                            original[offset + channel] * (1.0f - coverage) +
                                full[offset + channel] * coverage,
                            1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
            }
        }
    }
    EXPECT_TRUE(observedEffectInsideCoverage);
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
