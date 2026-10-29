#include <gtest/gtest.h>

#include <QString>
#include <QVariant>

#include <array>
#include <cmath>

import Artifact.Effect.Rasterizer.ApertureShapeBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

TEST(SpatialImageEffectContractTest,
     DepthBokehWithoutDepthOrLumaFallbackPreservesInputExactly)
{
    constexpr int width = 5;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width * height);
         ++pixel) {
        pixels[pixel * 4] = static_cast<float>(pixel) / 16.0f;
        pixels[pixel * 4 + 1] = static_cast<float>(pixel + 1) / 20.0f;
        pixels[pixel * 4 + 2] = static_cast<float>(pixel + 2) / 24.0f;
        pixels[pixel * 4 + 3] = static_cast<float>(pixel + 3) / 24.0f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;

    DepthBokehEffect effect;
    effect.setPropertyValue(
        UniString(QStringLiteral("useLumaFallback")), QVariant(false));
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

TEST(SpatialImageEffectContractTest,
     DepthBokehLumaFallbackKeepsUniformInFocusPixelsSharp)
{
    constexpr int width = 7;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width * height);
         ++pixel) {
        pixels[pixel * 4] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = static_cast<float>(pixel + 1) / 36.0f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;

    DepthBokehEffect effect;
    effect.setPropertyValue(
        UniString(QStringLiteral("focusDistance")), QVariant(0.5));
    effect.setPropertyValue(
        UniString(QStringLiteral("focusRange")), QVariant(0.0));
    effect.setPropertyValue(
        UniString(QStringLiteral("highlightBoost")), QVariant(0.0));
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

TEST(SpatialImageEffectContractTest,
     DepthBokehLumaFallbackBlursOutOfFocusRgbAndPreservesAlpha)
{
    constexpr int width = 33;
    constexpr int height = 33;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    constexpr std::size_t center =
        static_cast<std::size_t>(height / 2) * width + width / 2;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4] = 0.8f;
        pixels[pixel * 4 + 1] = 0.8f;
        pixels[pixel * 4 + 2] = 0.8f;
        pixels[pixel * 4 + 3] = 0.3f +
            0.6f * static_cast<float>(pixel) /
                static_cast<float>(pixelCount - 1);
    }
    pixels[center * 4] = 1.0f;
    pixels[center * 4 + 1] = 1.0f;
    pixels[center * 4 + 2] = 1.0f;

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;

    DepthBokehEffect effect;
    effect.setPropertyValue(
        UniString(QStringLiteral("focusDistance")), QVariant(0.0));
    effect.setPropertyValue(
        UniString(QStringLiteral("focusRange")), QVariant(0.0));
    effect.setPropertyValue(
        UniString(QStringLiteral("maxRadius")), QVariant(8.0));
    effect.setPropertyValue(
        UniString(QStringLiteral("highlightBoost")), QVariant(0.0));
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    const std::size_t adjacent = center + 1;
    EXPECT_LT(result[center * 4], pixels[center * 4]);
    EXPECT_GT(result[adjacent * 4], pixels[adjacent * 4]);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "pixel=" << pixel;
    }
}

TEST(SpatialImageEffectContractTest, ApertureBlurSpreadsRgbAndPreservesAlphaAndInput)
{
    constexpr int width = 33;
    constexpr int height = 33;
    constexpr int pixelCount = width * height;
    constexpr int center = (height / 2) * width + width / 2;
    std::array<float, pixelCount * 4> pixels{};
    for (int index = 0; index < pixelCount; ++index) {
        pixels[index * 4 + 3] = 0.25f + 0.5f *
            static_cast<float>(index % width) / static_cast<float>(width - 1);
    }
    pixels[center * 4] = 1.0f;

    ImageF32x4_RGBA sourceImage;
    sourceImage.setFromRGBA32F(pixels.data(), width, height);
    ImageF32x4RGBAWithCache source(sourceImage);
    ImageF32x4RGBAWithCache output;
    ApertureShapeBlurEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Radius")), QVariant(8.0));
    effect.setPropertyValue(UniString(QStringLiteral("Edge Brightness")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Highlight Boost")), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    EXPECT_EQ(output.image().colorDescriptor(), source.image().colorDescriptor());
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    EXPECT_FLOAT_EQ(source.image().getPixel(width / 2, height / 2).r(), 1.0f);

    int affectedPixels = 0;
    float centerRed = 0.0f;
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    for (int index = 0; index < pixelCount; ++index) {
        const float red = outputPixels[index * 4];
        const float green = outputPixels[index * 4 + 1];
        const float blue = outputPixels[index * 4 + 2];
        const float alpha = outputPixels[index * 4 + 3];
        ASSERT_TRUE(std::isfinite(red)) << "pixel=" << index;
        ASSERT_TRUE(std::isfinite(green)) << "pixel=" << index;
        ASSERT_TRUE(std::isfinite(blue)) << "pixel=" << index;
        ASSERT_TRUE(std::isfinite(alpha)) << "pixel=" << index;
        if (red > 1e-5f) {
            ++affectedPixels;
        }
        EXPECT_FLOAT_EQ(sourcePixels[index * 4], pixels[index * 4])
            << "source red pixel=" << index;
        EXPECT_FLOAT_EQ(sourcePixels[index * 4 + 1], pixels[index * 4 + 1])
            << "source green pixel=" << index;
        EXPECT_FLOAT_EQ(sourcePixels[index * 4 + 2], pixels[index * 4 + 2])
            << "source blue pixel=" << index;
        EXPECT_FLOAT_EQ(sourcePixels[index * 4 + 3], pixels[index * 4 + 3])
            << "source alpha pixel=" << index;
        EXPECT_NEAR(alpha, pixels[index * 4 + 3], 1e-6f) << "pixel=" << index;
        if (index == center) {
            centerRed = red;
        }
    }

    EXPECT_GT(affectedPixels, 1);
    EXPECT_GT(centerRed, 0.0f);
    EXPECT_LT(centerRed, 1.0f);
}
