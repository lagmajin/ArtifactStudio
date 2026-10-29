#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Mosaic;
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

TEST(MosaicEffectContractTest, ReplacesEachCellWithItsRgbaAverage)
{
    constexpr std::array<float, 16> pixels = {
        0.0f, 0.2f, 0.4f, 0.2f,
        1.0f, 0.4f, 0.6f, 0.4f,
        0.2f, 0.6f, 0.8f, 0.6f,
        0.8f, 0.8f, 1.0f, 0.8f,
    };
    const auto source = makeSurface(pixels.data(), 2, 2);
    ImageF32x4RGBAWithCache output;
    MosaicEffect effect;
    effect.setCellSize(2.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 2);
    ASSERT_EQ(output.image().height(), 2);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), 0.5f, 1.0e-6f);
            EXPECT_NEAR(pixel.g(), 0.5f, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), 0.7f, 1.0e-6f);
            EXPECT_NEAR(pixel.a(), 0.5f, 1.0e-6f);
        }
    }
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(1, 1).b(), 1.0f, 1.0e-6f);
}

TEST(MosaicEffectContractTest, AveragesPartialEdgeCellsUsingOnlyTheirRgbaPixels)
{
    constexpr std::array<float, 24> pixels = {
        0.0f, 0.1f, 0.2f, 0.2f,
        1.0f, 0.3f, 0.4f, 0.6f,
        0.8f, 0.5f, 0.6f, 0.8f,
        0.4f, 0.7f, 0.8f, 0.4f,
        0.2f, 0.9f, 1.0f, 0.6f,
        1.0f, 0.1f, 0.0f, 1.0f,
    };
    const auto source = makeSurface(pixels.data(), 3, 2);
    ImageF32x4RGBAWithCache output;
    MosaicEffect effect;
    effect.setCellSize(2.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 3);
    ASSERT_EQ(output.image().height(), 2);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), 0.5f, 1.0e-6f);
            EXPECT_NEAR(pixel.g(), 0.4f, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), 0.5f, 1.0e-6f);
            EXPECT_NEAR(pixel.a(), 0.5f, 1.0e-6f);
        }
    }
    for (int y = 0; y < 2; ++y) {
        const auto edgePixel = output.image().getPixel(2, y);
        EXPECT_NEAR(edgePixel.r(), 0.9f, 1.0e-6f);
        EXPECT_NEAR(edgePixel.g(), 0.6f, 1.0e-6f);
        EXPECT_NEAR(edgePixel.b(), 0.3f, 1.0e-6f);
        EXPECT_NEAR(edgePixel.a(), 0.9f, 1.0e-6f);
    }
    EXPECT_NEAR(source.image().getPixel(2, 0).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(2, 1).r(), 1.0f, 1.0e-6f);
}

TEST(MosaicEffectContractTest, ShapeModeChangesOnlyTheDiamondWithinEachCell)
{
    constexpr int width = 4;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(y * width + x) / 15.0f;
            pixels[offset + 1] = 0.25f;
            pixels[offset + 2] = 0.75f;
            pixels[offset + 3] = 0.2f + static_cast<float>(x + y) * 0.1f;
        }
    }

    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    MosaicEffect effect;
    effect.setCellSize(4.0f);
    effect.setShapeMode(true);

    effect.applyCPUOnly(source, output);

    constexpr int centerX = 2;
    constexpr int centerY = 2;
    constexpr int radius = 2;
    std::array<float, 4> average{};
    int includedPixels = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (std::abs(x - centerX) + std::abs(y - centerY) > radius) {
                continue;
            }
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            for (int channel = 0; channel < 4; ++channel) {
                average[channel] += pixels[offset + channel];
            }
            ++includedPixels;
        }
    }
    ASSERT_GT(includedPixels, 0);
    for (float& channel : average) {
        channel /= static_cast<float>(includedPixels);
    }

    bool diamondChanged = false;
    bool outsidePreserved = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            const bool inDiamond = std::abs(x - centerX) + std::abs(y - centerY) <= radius;
            const std::array<float, 4> expected = inDiamond
                ? average
                : std::array<float, 4>{pixels[offset], pixels[offset + 1],
                                       pixels[offset + 2], pixels[offset + 3]};
            EXPECT_NEAR(actual.r(), expected[0], 1.0e-6f);
            EXPECT_NEAR(actual.g(), expected[1], 1.0e-6f);
            EXPECT_NEAR(actual.b(), expected[2], 1.0e-6f);
            EXPECT_NEAR(actual.a(), expected[3], 1.0e-6f);
            if (inDiamond) {
                diamondChanged = diamondChanged ||
                    std::abs(actual.r() - pixels[offset]) > 1.0e-5f;
            } else {
                outsidePreserved = outsidePreserved ||
                    std::abs(actual.r() - pixels[offset]) <= 1.0e-6f;
            }
        }
    }
    EXPECT_TRUE(diamondChanged);
    EXPECT_TRUE(outsidePreserved);
}
\n
