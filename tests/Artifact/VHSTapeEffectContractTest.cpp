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
import Property.Abstract;
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

TEST(VHSTapeEffectContractTest,
     NeutralSettingsHandleSinglePixelSurfaceAndPreserveAlpha)
{
    constexpr std::array<float, 4> pixel = {0.1f, 0.35f, 0.8f, 0.4f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixel.data(), 1, 1);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 1);
    ASSERT_EQ(output.image().height(), 1);
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    EXPECT_NE(actual, source.image().rgba32fData());
    EXPECT_NEAR(actual[0], pixel[2], 1.0e-6f);
    EXPECT_NEAR(actual[1], pixel[1], 1.0e-6f);
    EXPECT_NEAR(actual[2], pixel[0], 1.0e-6f);
    EXPECT_NEAR(actual[3], pixel[3], 1.0e-6f);
}

TEST(VHSTapeEffectContractTest,
     OsdDrawingOnSinglePixelSurfaceRemainsFiniteAndDoesNotMutateSource)
{
    constexpr std::array<float, 4> pixel = {0.1f, 0.35f, 0.8f, 0.4f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixel.data(), 1, 1);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
    effect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 3);
    effect.setPropertyValue(UniString(QStringLiteral("Evolution")), 3661.5);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 1);
    ASSERT_EQ(output.image().height(), 1);
    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    for (std::size_t channel = 0; channel < pixel.size(); ++channel) {
        EXPECT_TRUE(std::isfinite(actual[channel])) << "channel=" << channel;
        EXPECT_FLOAT_EQ(original[channel], pixel[channel])
            << "source channel=" << channel;
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

TEST(VHSTapeEffectContractTest,
     ScanlineIntensityPreservesPerPixelAlpha)
{
    constexpr int width = 9;
    constexpr int height = 8;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>(y * width + x) * 4;
            pixels[offset + 0] = 0.3f;
            pixels[offset + 1] = 0.45f;
            pixels[offset + 2] = 0.6f;
            pixels[offset + 3] = static_cast<float>(y * width + x) /
                                 static_cast<float>(pixelCount - 1);
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Gap")), 2.0);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Intensity")), 0.75);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "scanline intensity must not change alpha at pixel=" << pixel;
    }
}

TEST(VHSTapeEffectContractTest, EmptyInputIsPreservedAndInPlaceProcessingStaysFinite)
{
    ImageF32x4_RGBA emptyImage;
    emptyImage.setFromRGBA32F(nullptr, 0, 4);
    const ImageF32x4RGBAWithCache emptySource(emptyImage);
    auto emptyOutput = makeUniformSurface();
    ASSERT_FALSE(emptyOutput.image().isEmpty());
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.applyCPUOnly(emptySource, emptyOutput);
    EXPECT_TRUE(emptyOutput.image().isEmpty());
    EXPECT_EQ(emptyOutput.image().width(), emptySource.image().width());
    EXPECT_EQ(emptyOutput.image().height(), emptySource.image().height());
    EXPECT_EQ(emptyOutput.image().colorDescriptor(),
              emptySource.image().colorDescriptor());

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

TEST(VHSTapeEffectContractTest,
     InPlaceProcessingMatchesOutOfPlaceAndPreservesSource)
{
    constexpr int width = 5;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
        const std::size_t offset = pixel * 4;
        pixels[offset] = static_cast<float>(pixel + 1) / 22.0f;
        pixels[offset + 1] = static_cast<float>(pixel + 2) / 23.0f;
        pixels[offset + 2] = static_cast<float>(pixel + 3) / 24.0f;
        pixels[offset + 3] = static_cast<float>(pixel + 1) / 21.0f;
    }
    const auto descriptor =
        SurfaceColorDescriptor::linearStraightRgba32Float();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    image.setColorDescriptor(descriptor);
    const ImageF32x4RGBAWithCache source(image);
    auto inPlace = source;
    ImageF32x4RGBAWithCache outOfPlace;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Wobble")), 0.8);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Intensity")), 0.25);
    effect.setPropertyValue(UniString(QStringLiteral("Seed")), 31);
    effect.setPropertyValue(UniString(QStringLiteral("Evolution")), 0.4);

    effect.applyCPUOnly(source, outOfPlace);
    effect.applyCPUOnly(inPlace, inPlace);

    const float* original = source.image().rgba32fData();
    const float* inPlacePixels = inPlace.image().rgba32fData();
    const float* outOfPlacePixels = outOfPlace.image().rgba32fData();
    ASSERT_NE(original, nullptr);
    ASSERT_NE(inPlacePixels, nullptr);
    ASSERT_NE(outOfPlacePixels, nullptr);
    EXPECT_EQ(source.image().colorDescriptor(), descriptor);
    EXPECT_EQ(inPlace.image().colorDescriptor(), descriptor);
    EXPECT_EQ(outOfPlace.image().colorDescriptor(), descriptor);
    EXPECT_NE(inPlacePixels, original);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(original[channel], pixels[channel])
            << "source channel=" << channel;
        EXPECT_FLOAT_EQ(inPlacePixels[channel], outOfPlacePixels[channel])
            << "output channel=" << channel;
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
    effect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), -1);
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
    EXPECT_DOUBLE_EQ(value(QStringLiteral("OSD Mode")), 0.0);

    effect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 9);
    effect.setPropertyValue(UniString(QStringLiteral("Evolution")),
                            std::numeric_limits<double>::quiet_NaN());
    effect.setPropertyValue(UniString(QStringLiteral("Seed")), -37);
    effect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("OSD Mode")), 3.0);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Evolution")), 0.0);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Seed")), -37.0);
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Show OSD")), 1.0);

    effect.setPropertyValue(UniString(QStringLiteral("Tracking Error")),
                            std::numeric_limits<double>::quiet_NaN());
    EXPECT_DOUBLE_EQ(value(QStringLiteral("Tracking Error")), 0.3);
}

TEST(VHSTapeEffectContractTest,
     PublishedPropertySchemaKeepsStableNamesOrderAndTypes)
{
    VHSTapeEffect effect;
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 13u);
    constexpr std::array<const char*, 13> expectedNames{
        "Tracking Error", "Noise Amount", "Color Bleed", "Scanline Gap",
        "Scanline Intensity", "Wobble", "Saturation", "Sharpness",
        "Bottom Jitter", "Show OSD", "OSD Mode", "Evolution", "Seed"};
    constexpr std::array<PropertyType, 13> expectedTypes{
        PropertyType::Float, PropertyType::Float, PropertyType::Float,
        PropertyType::Float, PropertyType::Float, PropertyType::Float,
        PropertyType::Float, PropertyType::Float, PropertyType::Float,
        PropertyType::Boolean, PropertyType::Integer, PropertyType::Float,
        PropertyType::Integer};
    for (std::size_t index = 0; index < properties.size(); ++index) {
        EXPECT_EQ(properties[index].getName(),
                  QString::fromLatin1(expectedNames[index]))
            << "property=" << index;
        EXPECT_EQ(properties[index].getType(), expectedTypes[index])
            << "property=" << expectedNames[index];
    }
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
     NoiseAmountPreservesPerPixelAlpha)
{
    constexpr int width = 32;
    constexpr int height = 24;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4 + 0] = 0.45f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.55f;
        pixels[pixel * 4 + 3] = static_cast<float>(pixel % 11) / 10.0f;
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
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "noise must not alter alpha at pixel=" << pixel;
    }
}

TEST(VHSTapeEffectContractTest,
     NoiseAmountUsesConfiguredSeedDeterministically)
{
    constexpr int width = 32;
    constexpr int height = 24;
    constexpr std::size_t channelCount =
        static_cast<std::size_t>(width) * height * 4;
    std::array<float, channelCount> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset + 0] = 0.45f;
        pixels[offset + 1] = 0.5f;
        pixels[offset + 2] = 0.55f;
        pixels[offset + 3] = 0.7f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache first;
    ImageF32x4RGBAWithCache repeated;
    ImageF32x4RGBAWithCache differentSeed;
    const auto renderWithSeed = [&source](const int seed,
                                          ImageF32x4RGBAWithCache& output) {
        VHSTapeEffect effect;
        setNeutralTapeProperties(effect);
        effect.setPropertyValue(UniString(QStringLiteral("Noise Amount")), 1.0);
        effect.setPropertyValue(UniString(QStringLiteral("Seed")), seed);
        effect.setPropertyValue(UniString(QStringLiteral("Evolution")), 0.4);
        effect.applyCPUOnly(source, output);
    };

    renderWithSeed(41, first);
    renderWithSeed(41, repeated);
    renderWithSeed(42, differentSeed);

    const float* firstPixels = first.image().rgba32fData();
    const float* repeatedPixels = repeated.image().rgba32fData();
    const float* differentSeedPixels = differentSeed.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(repeatedPixels, nullptr);
    ASSERT_NE(differentSeedPixels, nullptr);
    bool seedChangedOutput = false;
    for (std::size_t offset = 0; offset < channelCount; offset += 4) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(firstPixels[offset + channel],
                            repeatedPixels[offset + channel])
                << "pixel=" << offset / 4 << " channel=" << channel;
            seedChangedOutput = seedChangedOutput ||
                std::abs(firstPixels[offset + channel] -
                         differentSeedPixels[offset + channel]) > 1.0e-6f;
        }
        EXPECT_FLOAT_EQ(firstPixels[offset + 3], pixels[offset + 3]);
        EXPECT_FLOAT_EQ(repeatedPixels[offset + 3], pixels[offset + 3]);
        EXPECT_FLOAT_EQ(differentSeedPixels[offset + 3], pixels[offset + 3]);
    }
    EXPECT_TRUE(seedChangedOutput)
        << "changing the configured seed should change tape noise";
}

TEST(VHSTapeEffectContractTest,
     HigherNoiseAmountIncreasesRgbNoiseEnergy)
{
    constexpr int width = 64;
    constexpr int height = 48;
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
    ImageF32x4RGBAWithCache lowNoise;
    ImageF32x4RGBAWithCache highNoise;
    VHSTapeEffect lowEffect;
    setNeutralTapeProperties(lowEffect);
    lowEffect.setPropertyValue(UniString(QStringLiteral("Noise Amount")), 0.1);
    lowEffect.applyCPUOnly(source, lowNoise);
    VHSTapeEffect highEffect;
    setNeutralTapeProperties(highEffect);
    highEffect.setPropertyValue(UniString(QStringLiteral("Noise Amount")), 1.0);
    highEffect.applyCPUOnly(source, highNoise);

    double lowEnergy = 0.0;
    double highEnergy = 0.0;
    std::size_t sampleCount = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto low = lowNoise.image().getPixel(x, y);
            const auto high = highNoise.image().getPixel(x, y);
            const std::array<float, 3> lowRgb{low.r(), low.g(), low.b()};
            const std::array<float, 3> highRgb{high.r(), high.g(), high.b()};
            for (const float channel : lowRgb) {
                const double delta = static_cast<double>(channel) - 0.5;
                lowEnergy += delta * delta;
            }
            for (const float channel : highRgb) {
                const double delta = static_cast<double>(channel) - 0.5;
                highEnergy += delta * delta;
            }
            sampleCount += 3;
        }
    }
    ASSERT_GT(sampleCount, 0u);
    const double lowRms = std::sqrt(lowEnergy / static_cast<double>(sampleCount));
    const double highRms = std::sqrt(highEnergy / static_cast<double>(sampleCount));
    EXPECT_GT(lowRms, 0.0);
    EXPECT_GT(highRms, lowRms * 1.5);
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

TEST(VHSTapeEffectContractTest,
     SharpnessReductionPreservesPerPixelAlpha)
{
    constexpr int width = 33;
    constexpr int height = 9;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>(y * width + x) * 4;
            const float value = x < width / 2 ? 0.1f : 0.9f;
            pixels[offset + 0] = value;
            pixels[offset + 1] = 1.0f - value;
            pixels[offset + 2] = value * 0.6f;
            pixels[offset + 3] = static_cast<float>((x + y) % 17) / 16.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height,
                         SurfaceColorDescriptor::unknownBgra32Float());
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Sharpness")), 0.0);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "sharpness must not blur alpha at pixel=" << pixel;
    }
}

TEST(VHSTapeEffectContractTest, OsdVisibilityAndModeChangeRenderedPixels)
{
    constexpr int width = 320;
    constexpr int height = 180;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = 0.12f;
        pixels[offset + 1] = 0.18f;
        pixels[offset + 2] = 0.24f;
        pixels[offset + 3] = 1.0f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height,
                         SurfaceColorDescriptor::unknownBgra32Float());
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache hidden;
    ImageF32x4RGBAWithCache play;
    ImageF32x4RGBAWithCache record;
    ImageF32x4RGBAWithCache pause;
    ImageF32x4RGBAWithCache autoTrack;

    VHSTapeEffect hiddenEffect;
    setNeutralTapeProperties(hiddenEffect);
    hiddenEffect.applyCPUOnly(source, hidden);
    VHSTapeEffect playEffect;
    setNeutralTapeProperties(playEffect);
    playEffect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
    playEffect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 0);
    playEffect.applyCPUOnly(source, play);
    VHSTapeEffect recordEffect;
    setNeutralTapeProperties(recordEffect);
    recordEffect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
    recordEffect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 1);
    recordEffect.applyCPUOnly(source, record);
    VHSTapeEffect pauseEffect;
    setNeutralTapeProperties(pauseEffect);
    pauseEffect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
    pauseEffect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 2);
    pauseEffect.applyCPUOnly(source, pause);
    VHSTapeEffect autoTrackEffect;
    setNeutralTapeProperties(autoTrackEffect);
    autoTrackEffect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
    autoTrackEffect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 3);
    autoTrackEffect.applyCPUOnly(source, autoTrack);

    ASSERT_EQ(hidden.image().width(), width);
    ASSERT_EQ(hidden.image().height(), height);
    ASSERT_EQ(play.image().width(), width);
    ASSERT_EQ(play.image().height(), height);
    ASSERT_EQ(record.image().width(), width);
    ASSERT_EQ(record.image().height(), height);
    ASSERT_EQ(pause.image().width(), width);
    ASSERT_EQ(pause.image().height(), height);
    ASSERT_EQ(autoTrack.image().width(), width);
    ASSERT_EQ(autoTrack.image().height(), height);
    const float* hiddenPixels = hidden.image().rgba32fData();
    const float* playPixels = play.image().rgba32fData();
    const float* recordPixels = record.image().rgba32fData();
    const float* pausePixels = pause.image().rgba32fData();
    const float* autoTrackPixels = autoTrack.image().rgba32fData();
    ASSERT_NE(hiddenPixels, nullptr);
    ASSERT_NE(playPixels, nullptr);
    ASSERT_NE(recordPixels, nullptr);
    ASSERT_NE(pausePixels, nullptr);
    ASSERT_NE(autoTrackPixels, nullptr);
    bool osdVisible = false;
    bool modeChanged = false;
    bool pauseChanged = false;
    bool autoTrackChanged = false;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            osdVisible = osdVisible ||
                std::abs(playPixels[offset] - hiddenPixels[offset]) > 1.0e-5f;
            modeChanged = modeChanged ||
                std::abs(playPixels[offset] - recordPixels[offset]) > 1.0e-5f;
            pauseChanged = pauseChanged ||
                std::abs(playPixels[offset] - pausePixels[offset]) > 1.0e-5f;
            autoTrackChanged = autoTrackChanged ||
                std::abs(playPixels[offset] - autoTrackPixels[offset]) > 1.0e-5f;
        }
    }
    EXPECT_TRUE(osdVisible);
    EXPECT_TRUE(modeChanged);
    EXPECT_TRUE(pauseChanged);
    EXPECT_TRUE(autoTrackChanged);
}

TEST(VHSTapeEffectContractTest,
     OsdRaisesAlphaOnlyWhereTheOverlayIsDrawn)
{
    constexpr int width = 320;
    constexpr int height = 180;
    constexpr float sourceAlpha = 0.2f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = 0.12f;
        pixels[offset + 1] = 0.18f;
        pixels[offset + 2] = 0.24f;
        pixels[offset + 3] = sourceAlpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height,
                         SurfaceColorDescriptor::unknownBgra32Float());
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
    effect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 1);

    effect.applyCPUOnly(source, output);

    int overlayAlphaPixels = 0;
    int unchangedAlphaPixels = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float alpha = output.image().getPixel(x, y).a();
            EXPECT_TRUE(std::isfinite(alpha));
            EXPECT_GE(alpha, sourceAlpha);
            EXPECT_LE(alpha, 1.0f);
            if (alpha > sourceAlpha + 1.0e-5f) {
                ++overlayAlphaPixels;
            } else {
                EXPECT_FLOAT_EQ(alpha, sourceAlpha);
                ++unchangedAlphaPixels;
            }
        }
    }
    EXPECT_GT(overlayAlphaPixels, 0);
    EXPECT_GT(unchangedAlphaPixels, 0);
}

TEST(VHSTapeEffectContractTest,
     OsdClipsSafelyToSinglePixelSurfaces)
{
    constexpr std::array<float, 4> pixel{0.12f, 0.18f, 0.24f, 0.6f};
    const auto descriptor = SurfaceColorDescriptor::unknownBgra32Float();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixel.data(), 1, 1, descriptor);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
    effect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 3);
    effect.setPropertyValue(UniString(QStringLiteral("Evolution")), 3599.999);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 1);
    ASSERT_EQ(output.image().height(), 1);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < pixel.size(); ++channel) {
        EXPECT_TRUE(std::isfinite(result[channel])) << "channel=" << channel;
    }
    EXPECT_GE(result[3], pixel[3]);
    EXPECT_LE(result[3], 1.0f);
}

TEST(VHSTapeEffectContractTest, OsdTimecodeAdvancesWithEvolution)
{
    constexpr int width = 320;
    constexpr int height = 180;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = 0.12f;
        pixels[offset + 1] = 0.18f;
        pixels[offset + 2] = 0.24f;
        pixels[offset + 3] = 1.0f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height,
                         SurfaceColorDescriptor::unknownBgra32Float());
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache firstSecond;
    ImageF32x4RGBAWithCache halfSecond;
    ImageF32x4RGBAWithCache secondSecond;

    const auto renderOsdAt = [&source](
        const double evolution, ImageF32x4RGBAWithCache& output) {
        VHSTapeEffect effect;
        setNeutralTapeProperties(effect);
        effect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
        effect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 0);
        effect.setPropertyValue(UniString(QStringLiteral("Evolution")), evolution);
        effect.applyCPUOnly(source, output);
    };
    renderOsdAt(0.0, firstSecond);
    renderOsdAt(0.5, halfSecond);
    renderOsdAt(1.0, secondSecond);

    ASSERT_EQ(firstSecond.image().width(), width);
    ASSERT_EQ(firstSecond.image().height(), height);
    ASSERT_EQ(halfSecond.image().width(), width);
    ASSERT_EQ(halfSecond.image().height(), height);
    ASSERT_EQ(secondSecond.image().width(), width);
    ASSERT_EQ(secondSecond.image().height(), height);
    const float* firstPixels = firstSecond.image().rgba32fData();
    const float* halfPixels = halfSecond.image().rgba32fData();
    const float* secondPixels = secondSecond.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(halfPixels, nullptr);
    ASSERT_NE(secondPixels, nullptr);
    bool subframeChanged = false;
    bool secondChanged = false;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            subframeChanged = subframeChanged ||
                std::abs(firstPixels[offset] - halfPixels[offset]) > 1.0e-5f;
            secondChanged = secondChanged ||
                std::abs(halfPixels[offset] - secondPixels[offset]) > 1.0e-5f;
        }
    }
    EXPECT_TRUE(subframeChanged)
        << "the OSD frame field should reflect fractional Evolution seconds";
    EXPECT_TRUE(secondChanged)
        << "the OSD seconds field should advance with Evolution";
}

TEST(VHSTapeEffectContractTest,
     OsdTimecodeRollsFrameCounterAtTheOneSecondBoundary)
{
    constexpr int width = 320;
    constexpr int height = 180;
    constexpr std::size_t channelCount =
        static_cast<std::size_t>(width) * height * 4;
    std::array<float, channelCount> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset + 0] = 0.12f;
        pixels[offset + 1] = 0.18f;
        pixels[offset + 2] = 0.24f;
        pixels[offset + 3] = 1.0f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height,
                         SurfaceColorDescriptor::unknownBgra32Float());
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache lastFrameOfFirstSecond;
    ImageF32x4RGBAWithCache firstFrameOfNextSecond;
    const auto renderOsdAt = [&source](const double evolution,
                                       ImageF32x4RGBAWithCache& output) {
        VHSTapeEffect effect;
        setNeutralTapeProperties(effect);
        effect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
        effect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 0);
        effect.setPropertyValue(UniString(QStringLiteral("Evolution")), evolution);
        effect.applyCPUOnly(source, output);
    };
    renderOsdAt(0.999, lastFrameOfFirstSecond);
    renderOsdAt(1.0, firstFrameOfNextSecond);

    const float* before = lastFrameOfFirstSecond.image().rgba32fData();
    const float* after = firstFrameOfNextSecond.image().rgba32fData();
    ASSERT_NE(before, nullptr);
    ASSERT_NE(after, nullptr);
    bool timecodePixelsChanged = false;
    for (std::size_t offset = 0; offset < channelCount; offset += 4) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            timecodePixelsChanged = timecodePixelsChanged ||
                std::abs(before[offset + channel] - after[offset + channel]) >
                    1.0e-5f;
        }
        EXPECT_FLOAT_EQ(before[offset + 3], 1.0f);
        EXPECT_FLOAT_EQ(after[offset + 3], 1.0f);
    }
    EXPECT_TRUE(timecodePixelsChanged)
        << "00:00:00:29 must advance to 00:00:01:00 at one second";
}

TEST(VHSTapeEffectContractTest,
     WobbleOutputIsRepeatableForSameSeedAndChangesForDifferentSeed)
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
    ImageF32x4RGBAWithCache differentSeed;

    const auto applyFixedSettings = [&source](
        const int seed, ImageF32x4RGBAWithCache& output) {
        VHSTapeEffect effect;
        setNeutralTapeProperties(effect);
        effect.setPropertyValue(UniString(QStringLiteral("Wobble")), 2.0);
        effect.setPropertyValue(UniString(QStringLiteral("Seed")), seed);
        effect.setPropertyValue(UniString(QStringLiteral("Evolution")), 0.4);
        effect.applyCPUOnly(source, output);
    };
    applyFixedSettings(29, first);
    applyFixedSettings(29, repeated);
    applyFixedSettings(30, differentSeed);

    ASSERT_EQ(first.image().width(), width);
    ASSERT_EQ(first.image().height(), height);
    ASSERT_EQ(differentSeed.image().width(), width);
    ASSERT_EQ(differentSeed.image().height(), height);
    const float* firstPixels = first.image().rgba32fData();
    const float* repeatedPixels = repeated.image().rgba32fData();
    const float* differentSeedPixels = differentSeed.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(repeatedPixels, nullptr);
    ASSERT_NE(differentSeedPixels, nullptr);
    bool seedChangedOutput = false;
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(firstPixels[channel], repeatedPixels[channel])
            << "channel=" << channel;
        seedChangedOutput = seedChangedOutput ||
            std::abs(firstPixels[channel] - differentSeedPixels[channel]) >
                1.0e-5f;
    }
    EXPECT_TRUE(seedChangedOutput)
        << "changing the fixed seed should change the wobble sequence";
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

TEST(VHSTapeEffectContractTest,
     TrackingErrorAndBottomJitterDisturbExpectedRows)
{
    constexpr int width = 97;
    constexpr int height = 200;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float value = static_cast<float>((x * 37 + y * 19) % width) /
                                static_cast<float>(width - 1);
            pixels[offset] = value;
            pixels[offset + 1] = 1.0f - value;
            pixels[offset + 2] = static_cast<float>((x * 13 + y * 7) % width) /
                                 static_cast<float>(width - 1);
            pixels[offset + 3] = static_cast<float>(x) /
                                 static_cast<float>(width - 1);
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height,
                         SurfaceColorDescriptor::unknownBgra32Float());
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache baseline;
    ImageF32x4RGBAWithCache tracking;
    ImageF32x4RGBAWithCache bottomJitter;

    const auto applyWith = [&source](const QString& property,
                                     const double value,
                                     ImageF32x4RGBAWithCache& output) {
        VHSTapeEffect effect;
        setNeutralTapeProperties(effect);
        effect.setPropertyValue(UniString(QStringLiteral("Seed")), 29);
        effect.setPropertyValue(UniString(QStringLiteral("Evolution")), 0.4);
        effect.setPropertyValue(UniString(property), value);
        effect.applyCPUOnly(source, output);
    };
    applyWith(QStringLiteral("Tracking Error"), 0.0, baseline);
    applyWith(QStringLiteral("Tracking Error"), 2.0, tracking);
    applyWith(QStringLiteral("Bottom Jitter"), 2.0, bottomJitter);

    ASSERT_EQ(baseline.image().width(), width);
    ASSERT_EQ(baseline.image().height(), height);
    ASSERT_EQ(tracking.image().width(), width);
    ASSERT_EQ(bottomJitter.image().width(), width);
    const float* baselinePixels = baseline.image().rgba32fData();
    const float* trackingPixels = tracking.image().rgba32fData();
    const float* bottomPixels = bottomJitter.image().rgba32fData();
    ASSERT_NE(baselinePixels, nullptr);
    ASSERT_NE(trackingPixels, nullptr);
    ASSERT_NE(bottomPixels, nullptr);

    bool trackingChanged = false;
    bool bottomChanged = false;
    bool bottomLastRowChanged = false;
    int trackingDisturbedRows = 0;
    for (int y = 0; y < height; ++y) {
        bool trackingRowChanged = false;
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            bool trackingPixelChanged = false;
            bool bottomPixelChanged = false;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                trackingPixelChanged = trackingPixelChanged ||
                    std::abs(trackingPixels[offset + channel] -
                             baselinePixels[offset + channel]) > 1.0e-5f;
                bottomPixelChanged = bottomPixelChanged ||
                    std::abs(bottomPixels[offset + channel] -
                             baselinePixels[offset + channel]) > 1.0e-5f;
            }
            trackingChanged = trackingChanged || trackingPixelChanged;
            trackingRowChanged = trackingRowChanged || trackingPixelChanged;
            if (y < height - 20) {
                EXPECT_FALSE(bottomPixelChanged)
                    << "bottom jitter modified upper pixel=" << x << "," << y;
            } else {
                bottomChanged = bottomChanged || bottomPixelChanged;
                if (y == height - 1) {
                    bottomLastRowChanged = bottomLastRowChanged ||
                                           bottomPixelChanged;
                }
            }
            int trackingSourceX = -1;
            int bottomSourceX = -1;
            for (int sourceX = 0; sourceX < width; ++sourceX) {
                const std::size_t sourceOffset =
                    static_cast<std::size_t>((y * width + sourceX) * 4);
                if (trackingPixels[offset + 1] == pixels[sourceOffset + 1]) {
                    trackingSourceX = sourceX;
                }
                if (bottomPixels[offset + 1] == pixels[sourceOffset + 1]) {
                    bottomSourceX = sourceX;
                }
            }
            ASSERT_GE(trackingSourceX, 0)
                << "tracking green channel must identify the shifted sample";
            ASSERT_GE(bottomSourceX, 0)
                << "head-switch green channel must identify the shifted sample";
            EXPECT_FLOAT_EQ(trackingPixels[offset + 3],
                            pixels[static_cast<std::size_t>(
                                (y * width + trackingSourceX) * 4 + 3)])
                << "tracking alpha must follow its green source pixel at "
                << x << "," << y;
            EXPECT_FLOAT_EQ(bottomPixels[offset + 3],
                            pixels[static_cast<std::size_t>(
                                (y * width + bottomSourceX) * 4 + 3)])
                << "head-switch alpha must follow its green source pixel at "
                << x << "," << y;
        }
        trackingDisturbedRows += trackingRowChanged ? 1 : 0;
    }
    EXPECT_TRUE(trackingChanged);
    EXPECT_GT(trackingDisturbedRows, 0);
    EXPECT_LT(trackingDisturbedRows, height)
        << "tracking error should remain a sparse set of disturbed rows";
    EXPECT_TRUE(bottomChanged);
    EXPECT_TRUE(bottomLastRowChanged)
        << "the maximum head-switch weight applies at the final image row";
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
