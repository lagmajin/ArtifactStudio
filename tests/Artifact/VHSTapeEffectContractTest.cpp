#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.VHSTape;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

void setNeutralVhsProperties(VHSTapeEffect& effect)
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

} // namespace

TEST(VHSTapeEffectContractTest, NeutralSettingsPreserveRgbaAndDoNotMutateSource)
{
    constexpr std::array<float, 16> pixels = {
        0.10f, 0.20f, 0.30f, 0.15f,
        0.90f, 0.40f, 0.20f, 0.35f,
        0.25f, 0.70f, 0.45f, 0.65f,
        0.80f, 0.15f, 0.55f, 0.95f,
    };
    const auto source = makeSurface(pixels.data(), 2, 2);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralVhsProperties(effect);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 2);
    ASSERT_EQ(output.image().height(), 2);
    const float* outputPixels = output.image().rgba32fData();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    ASSERT_NE(sourcePixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(outputPixels[channel], pixels[channel], 1.0e-6f)
            << "output channel=" << channel;
        EXPECT_FLOAT_EQ(sourcePixels[channel], pixels[channel])
            << "source channel=" << channel;
    }
}

TEST(VHSTapeEffectContractTest, ScanlineSettingDarkensConfiguredRows)
{
    constexpr int width = 3;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.8f;
        pixels[i + 1] = 0.6f;
        pixels[i + 2] = 0.4f;
        pixels[i + 3] = 1.0f;
    }

    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralVhsProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Gap")), 2.0);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Intensity")), 0.5);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    for (int y = 0; y < height; ++y) {
        const float expectedRed = (y % 2 == 0) ? 0.4f : 0.8f;
        for (int x = 0; x < width; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), expectedRed, 1.0e-5f)
                << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.g(), (y % 2 == 0) ? 0.3f : 0.6f, 1.0e-5f)
                << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.b(), (y % 2 == 0) ? 0.2f : 0.4f, 1.0e-5f)
                << "x=" << x << " y=" << y;
        }
    }
}

TEST(VHSTapeEffectContractTest, ScanlineGapDarkensEveryConfiguredRow)
{
    constexpr int width = 3;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = 0.5f;
        pixels[offset + 1] = 0.5f;
        pixels[offset + 2] = 0.5f;
        pixels[offset + 3] = 1.0f;
    }

    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralVhsProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Intensity")), 0.2);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Gap")), 2.0);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    for (int y = 0; y < height; ++y) {
        const float expected = (y % 2 == 0) ? 0.4f : 0.5f;
        for (int x = 0; x < width; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), expected, 1.0e-6f) << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.g(), expected, 1.0e-6f) << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.b(), expected, 1.0e-6f) << "x=" << x << " y=" << y;
        }
    }
}

TEST(VHSTapeEffectContractTest, DefaultLofiPresetProducesFiniteBoundedImage)
{
    constexpr int width = 64;
    constexpr int height = 48;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x) / static_cast<float>(width - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[offset + 2] = static_cast<float>(x + y) /
                                  static_cast<float>(width + height - 2);
            pixels[offset + 3] = 1.0f;
        }
    }

    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    bool changed = false;
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        ASSERT_TRUE(std::isfinite(outputPixels[channel])) << "channel=" << channel;
        EXPECT_GE(outputPixels[channel], 0.0f) << "channel=" << channel;
        EXPECT_LE(outputPixels[channel], 1.0f) << "channel=" << channel;
        changed = changed || std::abs(outputPixels[channel] - pixels[channel]) > 1.0e-5f;
    }
    EXPECT_TRUE(changed);

    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(sourcePixels[channel], pixels[channel])
            << "source channel=" << channel;
    }
}

TEST(VHSTapeEffectContractTest, WobbleEvolutionIsRepeatablePerSeedAndChangesAcrossFrames)
{
    constexpr int width = 48;
    constexpr int height = 32;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x) / static_cast<float>(width - 1);
            pixels[offset + 1] = static_cast<float>(x % 7) / 6.0f;
            pixels[offset + 2] = static_cast<float>(y % 5) / 4.0f;
            pixels[offset + 3] = 1.0f;
        }
    }

    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache firstFrame;
    ImageF32x4RGBAWithCache repeatedFrame;
    ImageF32x4RGBAWithCache nextFrame;

    auto configureWobble = [](VHSTapeEffect& effect, double evolution) {
        setNeutralVhsProperties(effect);
        effect.setPropertyValue(UniString(QStringLiteral("Tracking Error")), 0.0);
        effect.setPropertyValue(UniString(QStringLiteral("Bottom Jitter")), 0.0);
        effect.setPropertyValue(UniString(QStringLiteral("Wobble")), 2.0);
        effect.setPropertyValue(UniString(QStringLiteral("Evolution")), evolution);
        effect.setPropertyValue(UniString(QStringLiteral("Seed")), 17);
    };

    VHSTapeEffect firstEffect;
    configureWobble(firstEffect, 0.0);
    firstEffect.applyCPUOnly(source, firstFrame);

    VHSTapeEffect repeatedEffect;
    configureWobble(repeatedEffect, 0.0);
    repeatedEffect.applyCPUOnly(source, repeatedFrame);

    VHSTapeEffect nextEffect;
    configureWobble(nextEffect, 1.0);
    nextEffect.applyCPUOnly(source, nextFrame);

    const float* firstPixels = firstFrame.image().rgba32fData();
    const float* repeatedPixels = repeatedFrame.image().rgba32fData();
    const float* nextPixels = nextFrame.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(repeatedPixels, nullptr);
    ASSERT_NE(nextPixels, nullptr);

    bool evolutionChangedImage = false;
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(firstPixels[channel], repeatedPixels[channel])
            << "repeat channel=" << channel;
        evolutionChangedImage = evolutionChangedImage ||
                                std::abs(firstPixels[channel] - nextPixels[channel]) > 1.0e-5f;
    }
    EXPECT_TRUE(evolutionChangedImage);
}

TEST(VHSTapeEffectContractTest, OSDSettingAddsVisibleRetroOverlay)
{
    constexpr int width = 96;
    constexpr int height = 64;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = 0.2f;
        pixels[offset + 1] = 0.25f;
        pixels[offset + 2] = 0.3f;
        pixels[offset + 3] = 1.0f;
    }

    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache withoutOsd;
    ImageF32x4RGBAWithCache withOsd;
    VHSTapeEffect effect;
    setNeutralVhsProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Color Bleed")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Evolution")), 3661.5);
    effect.applyCPUOnly(source, withoutOsd);

    effect.setPropertyValue(UniString(QStringLiteral("Show OSD")), true);
    effect.setPropertyValue(UniString(QStringLiteral("OSD Mode")), 3);
    effect.applyCPUOnly(source, withOsd);

    ASSERT_EQ(withoutOsd.image().width(), width);
    ASSERT_EQ(withoutOsd.image().height(), height);
    ASSERT_EQ(withOsd.image().width(), width);
    ASSERT_EQ(withOsd.image().height(), height);
    const float* plainPixels = withoutOsd.image().rgba32fData();
    const float* osdPixels = withOsd.image().rgba32fData();
    ASSERT_NE(plainPixels, nullptr);
    ASSERT_NE(osdPixels, nullptr);
    bool overlayChangedImage = false;
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        overlayChangedImage = overlayChangedImage ||
                              std::abs(plainPixels[channel] - osdPixels[channel]) > 1.0e-5f;
    }
    EXPECT_TRUE(overlayChangedImage);
}
