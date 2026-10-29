#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

#include <QColor>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Stroke;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 3;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeCenterPixel()
{
    Pixels pixels{};
    const std::size_t offset = (static_cast<std::size_t>(kHeight / 2) * kWidth + kWidth / 2) * 4;
    pixels[offset] = 0.2f;
    pixels[offset + 1] = 0.4f;
    pixels[offset + 2] = 0.8f;
    pixels[offset + 3] = 1.0f;
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(StrokeEffectContractTest, ZeroWidthPreservesSourcePixels)
{
    const auto source = makeCenterPixel();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::StrokeEffect effect;
    effect.setWidth(0.0f);
    effect.setStrokeColor(QColor(255, 0, 0, 255));
    effect.setOpacity(100.0f);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(1, 1);
    EXPECT_FLOAT_EQ(center.r(), 0.2f);
    EXPECT_FLOAT_EQ(center.g(), 0.4f);
    EXPECT_FLOAT_EQ(center.b(), 0.8f);
    EXPECT_FLOAT_EQ(center.a(), 1.0f);
    const auto transparent = output.image().getPixel(0, 0);
    EXPECT_FLOAT_EQ(transparent.r(), 0.0f);
    EXPECT_FLOAT_EQ(transparent.g(), 0.0f);
    EXPECT_FLOAT_EQ(transparent.b(), 0.0f);
    EXPECT_FLOAT_EQ(transparent.a(), 0.0f);
}

TEST(StrokeEffectContractTest, AddsConfiguredColorToOpaquePixelBoundary)
{
    const auto source = makeCenterPixel();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::StrokeEffect effect;
    effect.setWidth(1.0f);
    effect.setStrokeColor(QColor(255, 0, 0, 255));
    effect.setOpacity(100.0f);

    effect.applyCPUOnly(source, output);

    const auto edge = output.image().getPixel(1, 0);
    EXPECT_NEAR(edge.r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(edge.g(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(edge.b(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(edge.a(), 1.0f, 1.0e-6f);

    const auto center = output.image().getPixel(1, 1);
    EXPECT_FLOAT_EQ(center.r(), 0.2f);
    EXPECT_FLOAT_EQ(center.g(), 0.4f);
    EXPECT_FLOAT_EQ(center.b(), 0.8f);
    EXPECT_FLOAT_EQ(center.a(), 1.0f);
}

TEST(StrokeEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::StrokeEffect effect;
    effect.setWidth(-1.0f);
    EXPECT_FLOAT_EQ(effect.width(), 0.0f);
    effect.setWidth(100.0f);
    EXPECT_FLOAT_EQ(effect.width(), 64.0f);
    effect.setWidth(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.width(), 3.0f);

    effect.setOpacity(-1.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 0.0f);
    effect.setOpacity(120.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 100.0f);
    effect.setOpacity(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.opacity(), 100.0f);
}