#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <QColor>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Satin;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

ArtifactCore::ImageF32x4RGBAWithCache makeTransparentColoredPixel()
{
    const std::array<float, 4> pixels{0.8f, 0.4f, 0.2f, 0.0f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(SatinEffectContractTest, InvertFillsTransparentMaskAndMultipliesRgb)
{
    const auto source = makeTransparentColoredPixel();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::SatinEffect effect;
    effect.setSatinColor(QColor(128, 128, 128, 255));
    effect.setDistance(0.0f);
    effect.setSoftness(0.0f);
    effect.setOpacity(50.0f);

    effect.applyCPUOnly(source, output);
    const auto withoutInvert = output.image().getPixel(0, 0);
    EXPECT_FLOAT_EQ(withoutInvert.r(), 0.0f);
    EXPECT_FLOAT_EQ(withoutInvert.g(), 0.0f);
    EXPECT_FLOAT_EQ(withoutInvert.b(), 0.0f);
    EXPECT_FLOAT_EQ(withoutInvert.a(), 0.0f);

    effect.setInvert(true);
    effect.applyCPUOnly(source, output);
    const auto withInvert = output.image().getPixel(0, 0);
    const float tint = 128.0f / 255.0f;
    EXPECT_NEAR(withInvert.r(), 0.8f * tint, 1.0e-6f);
    EXPECT_NEAR(withInvert.g(), 0.4f * tint, 1.0e-6f);
    EXPECT_NEAR(withInvert.b(), 0.2f * tint, 1.0e-6f);
    EXPECT_NEAR(withInvert.a(), 0.5f, 1.0e-6f);
}

TEST(SatinEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::SatinEffect effect;
    effect.setDistance(-1.0f);
    EXPECT_FLOAT_EQ(effect.distance(), 0.0f);
    effect.setDistance(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.distance(), 0.0f);

    effect.setSoftness(-1.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 0.0f);
    effect.setSoftness(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.softness(), 5.0f);

    effect.setOpacity(-1.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 0.0f);
    effect.setOpacity(120.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 100.0f);
    effect.setOpacity(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.opacity(), 50.0f);

    effect.setAngle(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.angle(), 0.0f);
}
