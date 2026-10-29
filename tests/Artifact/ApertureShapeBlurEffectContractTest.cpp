#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.ApertureShapeBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

TEST(ApertureShapeBlurEffectContractTest, SpreadsRgbAndPreservesAlphaAndInput)
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

TEST(ApertureShapeBlurEffectContractTest, ZeroRadiusPreservesImageExactly)
{
    const auto source = makeCheckerboard();
    ImageF32x4RGBAWithCache output;
    ApertureShapeBlurEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Radius")), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    EXPECT_EQ(output.image().colorDescriptor(), source.image().colorDescriptor());
    const float* sourcePixels = source.image().rgba32fData();
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(outputPixels[channel], sourcePixels[channel])
            << "channel=" << channel;
    }
}

TEST(ApertureShapeBlurEffectContractTest, PentagonPsfIncludesDiagonalCornerOutsideCircularPsf)
{
    constexpr int width = 33;
    constexpr int height = 33;
    constexpr int pixelCount = width * height;
    constexpr int centerX = width / 2;
    constexpr int centerY = height / 2;
    constexpr int cornerX = centerX + 6;
    constexpr int cornerY = centerY + 3;
    std::array<float, pixelCount * 4> pixels{};
    pixels[(centerY * width + centerX) * 4] = 1.0f;
    for (int index = 0; index < pixelCount; ++index) {
        pixels[index * 4 + 3] = 0.7f;
    }

    ImageF32x4_RGBA sourceImage;
    sourceImage.setFromRGBA32F(pixels.data(), width, height);
    ImageF32x4RGBAWithCache source(sourceImage);
    ImageF32x4RGBAWithCache circularOutput;
    ImageF32x4RGBAWithCache pentagonOutput;
    ApertureShapeBlurEffect circularEffect;
    circularEffect.setPropertyValue(UniString(QStringLiteral("Radius")), QVariant(8.0));
    circularEffect.setPropertyValue(UniString(QStringLiteral("Shape")), QVariant(0));
    circularEffect.setPropertyValue(UniString(QStringLiteral("Edge Brightness")), QVariant(0.0));
    circularEffect.setPropertyValue(UniString(QStringLiteral("Highlight Boost")), QVariant(0.0));
    ApertureShapeBlurEffect pentagonEffect;
    pentagonEffect.setPropertyValue(UniString(QStringLiteral("Radius")), QVariant(8.0));
    pentagonEffect.setPropertyValue(UniString(QStringLiteral("Shape")), QVariant(1));
    pentagonEffect.setPropertyValue(UniString(QStringLiteral("Edge Brightness")), QVariant(0.0));
    pentagonEffect.setPropertyValue(UniString(QStringLiteral("Highlight Boost")), QVariant(0.0));

    circularEffect.applyCPUOnly(source, circularOutput);
    pentagonEffect.applyCPUOnly(source, pentagonOutput);

    ASSERT_NE(circularOutput.image().rgba32fData(), nullptr);
    ASSERT_NE(pentagonOutput.image().rgba32fData(), nullptr);
    const int cornerOffset = (cornerY * width + cornerX) * 4;
    EXPECT_FLOAT_EQ(circularOutput.image().rgba32fData()[cornerOffset], 0.0f);
    EXPECT_GT(pentagonOutput.image().rgba32fData()[cornerOffset], 0.0f);
    EXPECT_FLOAT_EQ(pentagonOutput.image().rgba32fData()[cornerOffset + 3], 0.7f);
}
