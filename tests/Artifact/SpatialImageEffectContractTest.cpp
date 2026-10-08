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
