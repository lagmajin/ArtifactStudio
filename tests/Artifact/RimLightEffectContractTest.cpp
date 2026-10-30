#include <gtest/gtest.h>

#include <QColor>

#include <array>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Edge;
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

TEST(RimLightEffectContractTest, DirectionalRimTargetsAlphaEdgeAndPreservesAlpha)
{
    constexpr int width = 9;
    constexpr int height = 9;
    std::array<float, width * height * 4> pixels{};
    for (int y = 2; y <= 6; ++y) {
        for (int x = 2; x <= 6; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = 1.0f;
            pixels[offset + 1] = 1.0f;
            pixels[offset + 2] = 1.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    RimLightEffect effect;
    effect.setAngle(0.0f);
    effect.setWidth(2.0f);
    effect.setSoftness(0.0f);
    effect.setIntensity(1.0f);
    effect.setColor(QColor(255, 0, 0, 255));
    effect.setMix(1.0f);

    effect.applyCPUOnly(source, output);

    const auto edge = output.image().getPixel(6, 4);
    const auto interior = output.image().getPixel(4, 4);
    EXPECT_GT(edge.r(), interior.r());
    EXPECT_NEAR(interior.r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(edge.a(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(interior.a(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(6, 4).r(), 1.0f, 1.0e-6f);
}

TEST(RimLightEffectContractTest, ReversingLightAngleMovesRimToOppositeSilhouette)
{
    constexpr int width = 9;
    constexpr int height = 9;
    std::array<float, width * height * 4> pixels{};
    for (int y = 2; y <= 6; ++y) {
        for (int x = 2; x <= 6; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = 1.0f;
            pixels[offset + 1] = 1.0f;
            pixels[offset + 2] = 1.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    RimLightEffect effect;
    effect.setAngle(180.0f);
    effect.setWidth(2.0f);
    effect.setSoftness(0.0f);
    effect.setIntensity(1.0f);
    effect.setColor(QColor(255, 0, 0, 255));
    effect.setMix(1.0f);

    effect.applyCPUOnly(source, output);

    const auto leftEdge = output.image().getPixel(2, 4);
    const auto rightEdge = output.image().getPixel(6, 4);
    EXPECT_GT(leftEdge.r(), rightEdge.r());
    EXPECT_NEAR(rightEdge.r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(leftEdge.a(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(rightEdge.a(), 1.0f, 1.0e-6f);
}

TEST(RimLightEffectContractTest, SettingsNormalizeAnglesAndClampRanges)
{
    RimLightEffect effect;
    effect.setAngle(-90.0f);
    effect.setWidth(0.0f);
    effect.setSoftness(2.0f);
    effect.setIntensity(8.0f);
    effect.setMix(-1.0f);
    EXPECT_FLOAT_EQ(effect.angle(), 270.0f);
    EXPECT_FLOAT_EQ(effect.width(), 1.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 1.0f);
    EXPECT_FLOAT_EQ(effect.intensity(), 5.0f);
    EXPECT_FLOAT_EQ(effect.mix(), 0.0f);

    effect.setAngle(450.0f);
    effect.setWidth(std::numeric_limits<float>::infinity());
    effect.setSoftness(std::numeric_limits<float>::quiet_NaN());
    effect.setIntensity(std::numeric_limits<float>::infinity());
    effect.setMix(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.angle(), 90.0f);
    EXPECT_FLOAT_EQ(effect.width(), 8.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 0.55f);
    EXPECT_FLOAT_EQ(effect.intensity(), 1.5f);
    EXPECT_FLOAT_EQ(effect.mix(), 1.0f);
}
\n
