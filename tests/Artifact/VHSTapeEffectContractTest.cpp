#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cmath>
#include <limits>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.VHSTape;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 4;

ImageF32x4RGBAWithCache makeUniformSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = 0.5f;
        pixels[offset + 1] = 0.5f;
        pixels[offset + 2] = 0.5f;
        pixels[offset + 3] = 1.0f;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

void setNeutralTapeProperties(VHSTapeEffect& effect)
{
    effect.setPropertyValue(UniString(QStringLiteral("Tracking Error")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Noise Amount")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Color Bleed")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Intensity")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Wobble")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Saturation")), 1.0);
    effect.setPropertyValue(UniString(QStringLiteral("Sharpness")), 1.0);
    effect.setPropertyValue(UniString(QStringLiteral("Bottom Jitter")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Show OSD")), false);
}

TEST(VHSTapeEffectContractTest, NeutralSettingsUseBgraContractAndPreserveSource)
{
    constexpr std::array<float, 8> pixels = {
        0.1f, 0.2f, 0.3f, 0.15f, 0.8f, 0.4f, 0.2f, 0.75f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 1);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    ASSERT_NE(original, nullptr);
    for (std::size_t pixel = 0; pixel < 2; ++pixel) {
        const std::size_t offset = pixel * 4;
        const std::array<float, 4> expectedBgra = {
            pixels[offset + 2], pixels[offset + 1],
            pixels[offset], pixels[offset + 3]};
        for (std::size_t channel = 0; channel < expectedBgra.size(); ++channel) {
            EXPECT_NEAR(result[offset + channel], expectedBgra[channel], 1.0e-6f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(original[offset + channel], expectedBgra[channel])
                << "source pixel=" << pixel << " channel=" << channel;
        }
    }
}

TEST(VHSTapeEffectContractTest, ScanlineIntensityDarkensRowsAndPreservesAlphaContract)
{
    const auto source = makeUniformSurface();
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Gap")), 2.0);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Intensity")), 0.2);
    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            const float expected = y % 2 == 0 ? 0.4f : 0.5f;
            EXPECT_NEAR(pixel.r(), expected, 1.0e-6f);
            EXPECT_NEAR(pixel.g(), expected, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), expected, 1.0e-6f);
            EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
        }
    }
}

TEST(VHSTapeEffectContractTest, EmptyInputIsPreservedAndInPlaceProcessingStaysFinite)
{
    ImageF32x4_RGBA emptyImage;
    emptyImage.setFromRGBA32F(nullptr, 0, 4);
    const ImageF32x4RGBAWithCache emptySource(emptyImage);
    ImageF32x4RGBAWithCache emptyOutput;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.applyCPUOnly(emptySource, emptyOutput);
    EXPECT_TRUE(emptyOutput.image().isEmpty());

    auto inPlace = makeUniformSurface();
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Intensity")), 0.25);
    effect.applyCPUOnly(inPlace, inPlace);
    ASSERT_EQ(inPlace.image().width(), kWidth);
    ASSERT_EQ(inPlace.image().height(), kHeight);
    const float* output = inPlace.image().rgba32fData();
    ASSERT_NE(output, nullptr);
    for (std::size_t i = 0; i < inPlace.image().totalPixels() * 4; ++i) {
        EXPECT_TRUE(std::isfinite(output[i])) << "channel=" << i;
    }
}

TEST(VHSTapeEffectContractTest, NumericPropertiesClampAndRecoverFromNonFiniteValues)
{
    VHSTapeEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Tracking Error")), -1.0);
    effect.setPropertyValue(UniString(QStringLiteral("Noise Amount")), 2.0);
    effect.setPropertyValue(UniString(QStringLiteral("Color Bleed")), 3.0);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Gap")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Wobble")), 3.0);
    effect.setPropertyValue(UniString(QStringLiteral("Saturation")), -1.0);
    effect.setPropertyValue(UniString(QStringLiteral("Sharpness")), 2.0);
    effect.setPropertyValue(UniString(QStringLiteral("Bottom Jitter")), 3.0);
    const auto value = [&effect](const QString& name) {
        for (const auto& property : effect.getProperties()) {
            if (property.getName() == name) return property.getValue().toDouble();
        }
        return std::numeric_limits<double>::quiet_NaN();
    };
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Tracking Error")), 0.0);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Noise Amount")), 1.0);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Color Bleed")), 2.0);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Scanline Gap")), 1.0);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Wobble")), 2.0);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Saturation")), 0.0);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Sharpness")), 1.0);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Bottom Jitter")), 2.0);

    effect.setPropertyValue(UniString(QStringLiteral("Tracking Error")),
                            std::numeric_limits<double>::quiet_NaN());
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Tracking Error")), 0.3);
}

TEST(VHSTapeEffectContractTest, ColorBleedSoftensHorizontalChromaEdgeAndPreservesAlpha)
{
    constexpr int width = 65;
    constexpr int height = 3;
    std::array<float, width * height * 4> bgra{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            const bool left = x < width / 2;
            bgra[offset] = 0.0f;
            bgra[offset + 1] = left ? 0.0f : 1.0f;
            bgra[offset + 2] = left ? 1.0f : 0.0f;
            bgra[offset + 3] = static_cast<float>(y + 1) / 4.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(bgra.data(), width, height,
                         SurfaceColorDescriptor::unknownBgra32Float());
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache plain;
    ImageF32x4RGBAWithCache bled;
    VHSTapeEffect noBleed;
    setNeutralTapeProperties(noBleed);
    noBleed.applyCPUOnly(source, plain);
    VHSTapeEffect withBleed;
    setNeutralTapeProperties(withBleed);
    withBleed.setPropertyValue(UniString(QStringLiteral("Color Bleed")), 1.0);
    withBleed.applyCPUOnly(source, bled);

    const auto leftPlain = plain.image().getPixel(width / 2 - 1, 1);
    const auto rightPlain = plain.image().getPixel(width / 2, 1);
    const auto leftBled = bled.image().getPixel(width / 2 - 1, 1);
    const auto rightBled = bled.image().getPixel(width / 2, 1);
    const float plainDelta = std::abs(leftPlain.r() - rightPlain.r()) +
                             std::abs(leftPlain.g() - rightPlain.g());
    const float bledDelta = std::abs(leftBled.r() - rightBled.r()) +
                            std::abs(leftBled.g() - rightBled.g());
    EXPECT_LT(bledDelta, plainDelta);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            EXPECT_FLOAT_EQ(bled.image().getPixel(x, y).a(),
                            source.image().getPixel(x, y).a());
        }
    }
}

TEST(VHSTapeEffectContractTest, NoiseAmountPerturbsColorAndKeepsOutputFinite)
{
    constexpr int width = 32;
    constexpr int height = 24;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = 0.5f;
        pixels[offset + 1] = 0.5f;
        pixels[offset + 2] = 0.5f;
        pixels[offset + 3] = 1.0f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Noise Amount")), 1.0);
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    bool colorChanged = false;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const float value = result[pixel * 4 + channel];
            EXPECT_TRUE(std::isfinite(value))
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_GE(value, 0.0f);
            EXPECT_LE(value, 1.0f);
            if (channel < 3) {
                colorChanged = colorChanged ||
                    std::abs(value - pixels[pixel * 4 + channel]) > 1.0e-5f;
            }
        }
    }
    EXPECT_TRUE(colorChanged)
        << "nonzero tape noise should perturb at least one color sample";
}

TEST(VHSTapeEffectContractTest,
     ZeroSaturationProducesMonochromeAndPreservesAlpha)
{
    constexpr int width = 7;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / 8.0f;
            pixels[offset + 1] = static_cast<float>(y + 1) / 4.0f;
            pixels[offset + 2] = static_cast<float>(width - x) / 8.0f;
            pixels[offset + 3] = static_cast<float>(x + y + 1) / 12.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height,
                         SurfaceColorDescriptor::unknownBgra32Float());
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Saturation")), 0.0);
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        EXPECT_NEAR(result[offset], result[offset + 1], 1.0e-6f)
            << "pixel=" << pixel;
        EXPECT_NEAR(result[offset + 1], result[offset + 2], 1.0e-6f)
            << "pixel=" << pixel;
        EXPECT_FLOAT_EQ(result[offset + 3], pixels[offset + 3])
            << "pixel=" << pixel;
    }
}

TEST(VHSTapeEffectContractTest,
     ZeroSharpnessSoftensColorEdgeAndKeepsOpaqueAlpha)
{
    constexpr int width = 65;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float value = x < width / 2 ? 0.0f : 1.0f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height,
                         SurfaceColorDescriptor::unknownBgra32Float());
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache sharp;
    ImageF32x4RGBAWithCache softened;
    VHSTapeEffect sharpEffect;
    setNeutralTapeProperties(sharpEffect);
    sharpEffect.applyCPUOnly(source, sharp);
    VHSTapeEffect softEffect;
    setNeutralTapeProperties(softEffect);
    softEffect.setPropertyValue(UniString(QStringLiteral("Sharpness")), 0.0);
    softEffect.applyCPUOnly(source, softened);

    const int leftX = width / 2 - 1;
    const int rightX = width / 2;
    const float sharpDelta = std::abs(
        sharp.image().getPixel(leftX, height / 2).r() -
        sharp.image().getPixel(rightX, height / 2).r());
    const float softDelta = std::abs(
        softened.image().getPixel(leftX, height / 2).r() -
        softened.image().getPixel(rightX, height / 2).r());
    EXPECT_GT(sharpDelta, 0.9f);
    EXPECT_LT(softDelta, sharpDelta);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            EXPECT_FLOAT_EQ(softened.image().getPixel(x, y).a(), 1.0f)
                << "x=" << x << " y=" << y;
        }
    }
}

TEST(VHSTapeEffectContractTest, WobbleOutputIsRepeatableForSameSeedAndEvolution)
{
    constexpr int width = 32;
    constexpr int height = 24;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            const float value = (x / 2 + y / 2) % 2 == 0 ? 0.2f : 0.8f;
            pixels[offset] = value;
            pixels[offset + 1] = 1.0f - value;
            pixels[offset + 2] = (x + y) % 3 == 0 ? 0.9f : 0.1f;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache first;
    ImageF32x4RGBAWithCache repeated;

    const auto applyFixedSettings = [&source](ImageF32x4RGBAWithCache& output) {
        VHSTapeEffect effect;
        setNeutralTapeProperties(effect);
        effect.setPropertyValue(UniString(QStringLiteral("Wobble")), 2.0);
        effect.setPropertyValue(UniString(QStringLiteral("Seed")), 29);
        effect.setPropertyValue(UniString(QStringLiteral("Evolution")), 0.4);
        effect.applyCPUOnly(source, output);
    };
    applyFixedSettings(first);
    applyFixedSettings(repeated);

    ASSERT_EQ(first.image().width(), width);
    ASSERT_EQ(first.image().height(), height);
    const float* firstPixels = first.image().rgba32fData();
    const float* repeatedPixels = repeated.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(repeatedPixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(firstPixels[channel], repeatedPixels[channel])
            << "channel=" << channel;
    }
}

TEST(VHSTapeEffectContractTest, WobbleOutputChangesAcrossEvolutionFrames)
{
    constexpr int width = 64;
    constexpr int height = 48;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float value = (x / 2 + y / 2) % 2 == 0 ? 0.15f : 0.85f;
            pixels[offset] = value;
            pixels[offset + 1] = 1.0f - value;
            pixels[offset + 2] = (x + y) % 2 == 0 ? 0.9f : 0.1f;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache firstFrame;
    ImageF32x4RGBAWithCache laterFrame;

    const auto renderAtEvolution = [&source](
        const float evolution, ImageF32x4RGBAWithCache& output) {
        VHSTapeEffect effect;
        setNeutralTapeProperties(effect);
        effect.setPropertyValue(UniString(QStringLiteral("Wobble")), 2.0);
        effect.setPropertyValue(UniString(QStringLiteral("Seed")), 29);
        effect.setPropertyValue(UniString(QStringLiteral("Evolution")), evolution);
        effect.applyCPUOnly(source, output);
    };
    renderAtEvolution(0.4f, firstFrame);
    renderAtEvolution(0.8f, laterFrame);

    ASSERT_EQ(firstFrame.image().width(), width);
    ASSERT_EQ(firstFrame.image().height(), height);
    ASSERT_EQ(laterFrame.image().width(), width);
    ASSERT_EQ(laterFrame.image().height(), height);
    const float* first = firstFrame.image().rgba32fData();
    const float* later = laterFrame.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(later, nullptr);
    bool changed = false;
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_TRUE(std::isfinite(first[channel])) << "channel=" << channel;
        EXPECT_TRUE(std::isfinite(later[channel])) << "channel=" << channel;
        changed = changed || std::abs(first[channel] - later[channel]) > 1.0e-5f;
    }
    EXPECT_TRUE(changed)
        << "fixed-seed wobble should evolve when the frame time changes";
}

} // namespace

TEST(VHSTapeEffectContractTest, ScanlineGapDarkensEveryConfiguredRow)
{
    const auto source = makeUniformSurface();
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Gap")), 2.0);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Intensity")), 0.2);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        const float expected = (y % 2 == 0) ? 0.4f : 0.5f;
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), expected, 1.0e-6f) << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.g(), expected, 1.0e-6f) << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.b(), expected, 1.0e-6f) << "x=" << x << " y=" << y;
        }
    }
}
