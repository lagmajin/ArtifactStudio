#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

#include <QColor>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Stroke;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 7;
constexpr int kHeight = 7;

ImageF32x4RGBAWithCache makeSinglePixelSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    constexpr auto center = static_cast<std::size_t>((3 * kWidth + 3) * 4);
    pixels[center] = 0.0f;
    pixels[center + 1] = 0.0f;
    pixels[center + 2] = 1.0f;
    pixels[center + 3] = 1.0f;

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(StrokeEffectContractTest, DilatesAlphaIntoConfiguredColorOutline)
{
    const auto source = makeSinglePixelSurface();
    StrokeEffect effect;
    effect.setStrokeColor(QColor(255, 0, 0, 255));
    effect.setWidth(1.0f);
    effect.setOpacity(100.0f);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(3, 3);
    EXPECT_NEAR(center.r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(center.g(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(center.b(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(center.a(), 1.0f, 1.0e-6f);

    const auto outline = output.image().getPixel(4, 3);
    EXPECT_NEAR(outline.r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(outline.g(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(outline.b(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(outline.a(), 1.0f, 1.0e-6f);
}

TEST(StrokeEffectContractTest, CombinesStrokeColorAlphaWithOpacity)
{
    const auto source = makeSinglePixelSurface();
    StrokeEffect effect;
    effect.setStrokeColor(QColor(255, 0, 0, 128));
    effect.setWidth(1.0f);
    effect.setOpacity(50.0f);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    const auto outline = output.image().getPixel(4, 3);
    EXPECT_NEAR(outline.r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(outline.g(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(outline.b(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(outline.a(), (128.0f / 255.0f) * 0.5f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(3, 3).b(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(4, 3).a(), 0.0f, 1.0e-6f);
}

TEST(StrokeEffectContractTest, WidthAndOpacityClampAndResetNonFiniteValues)
{
    StrokeEffect effect;
    effect.setWidth(-1.0f);
    effect.setOpacity(150.0f);
    EXPECT_FLOAT_EQ(effect.width(), 0.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 100.0f);

    effect.setWidth(80.0f);
    effect.setOpacity(-10.0f);
    EXPECT_FLOAT_EQ(effect.width(), 64.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 0.0f);

    effect.setWidth(std::numeric_limits<float>::quiet_NaN());
    effect.setOpacity(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.width(), 3.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 100.0f);
}
\n
