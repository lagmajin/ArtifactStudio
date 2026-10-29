#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Mosaic;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeCornerImpulse()
{
    Pixels pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 3] = 0.2f;
    }
    pixels[0] = 1.0f;
    pixels[1] = 0.5f;
    pixels[2] = 0.25f;
    pixels[3] = 0.8f;
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void expectPixel(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                 int x, int y, float r, float g, float b, float a)
{
    const auto pixel = image.image().getPixel(x, y);
    EXPECT_NEAR(pixel.r(), r, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), g, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), b, 1.0e-6f);
    EXPECT_NEAR(pixel.a(), a, 1.0e-6f);
}

} // namespace

TEST(MosaicEffectContractTest, RectangularCellsUseTheCellWideRgbaAverage)
{
    const auto source = makeCornerImpulse();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::MosaicEffect effect;
    effect.setCellSize(4.0f);
    effect.setShapeMode(false);

    effect.applyCPUOnly(source, output);

    const float averageAlpha = (0.8f + 15.0f * 0.2f) / 16.0f;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            expectPixel(output, x, y, 1.0f / 16.0f, 0.5f / 16.0f,
                        0.25f / 16.0f, averageAlpha);
        }
    }
}

TEST(MosaicEffectContractTest, ShapeModeAveragesOnlyDiamondAndKeepsOutsidePixels)
{
    const auto source = makeCornerImpulse();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::MosaicEffect effect;
    effect.setCellSize(4.0f);
    effect.setShapeMode(true);

    effect.applyCPUOnly(source, output);

    expectPixel(output, 2, 2, 0.0f, 0.0f, 0.0f, 0.2f);
    expectPixel(output, 0, 0, 1.0f, 0.5f, 0.25f, 0.8f);
    expectPixel(output, 3, 3, 0.0f, 0.0f, 0.0f, 0.2f);
}

TEST(MosaicEffectContractTest, NamedPropertiesUpdateCellSizeAndShapeMode)
{
    Artifact::MosaicEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Cell Size"), QVariant(12.0));
    effect.setPropertyValue(ArtifactCore::UniString("Shape Mode"), QVariant(true));

    EXPECT_FLOAT_EQ(effect.cellSize(), 12.0f);
    EXPECT_TRUE(effect.shapeMode());
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 2u);
    EXPECT_EQ(properties[0].getName(), QStringLiteral("Cell Size"));
    EXPECT_EQ(properties[1].getName(), QStringLiteral("Shape Mode"));
    EXPECT_FLOAT_EQ(properties[0].getValue().toFloat(), 12.0f);
    EXPECT_TRUE(properties[1].getValue().toBool());
}
