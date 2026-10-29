#include <gtest/gtest.h>

#include <QColor>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Edge;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 7;
constexpr int kHeight = 7;
using Pixels = std::array<float, kWidth * kHeight * 4>;

std::size_t pixelOffset(int x, int y)
{
    return (static_cast<std::size_t>(y) * kWidth + x) * 4;
}

ArtifactCore::ImageF32x4RGBAWithCache makeSurface(const Pixels& rgba)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

Pixels makeVerticalStep()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset = pixelOffset(x, y);
            const float value = x >= 3 ? 0.02f : 0.0f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 0.3f;
        }
    }
    return pixels;
}

Pixels makeOpaqueSquare()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset = pixelOffset(x, y);
            pixels[offset] = 0.1f;
            pixels[offset + 1] = 0.1f;
            pixels[offset + 2] = 0.1f;
            pixels[offset + 3] =
                (x >= 2 && x <= 4 && y >= 2 && y <= 4) ? 1.0f : 0.0f;
        }
    }
    return pixels;
}

} // namespace

TEST(EdgeEffectsContractTest, SobelThresholdControlsWeakStepResponse)
{
    const auto source = makeSurface(makeVerticalStep());
    ArtifactCore::ImageF32x4RGBAWithCache suppressedOutput;
    ArtifactCore::ImageF32x4RGBAWithCache visibleOutput;
    Artifact::EdgeEffect suppressed;
    suppressed.setMode(0.0f);
    suppressed.setThreshold(0.1f);
    suppressed.applyCPUOnly(source, suppressedOutput);

    Artifact::EdgeEffect visible;
    visible.setMode(0.0f);
    visible.setThreshold(0.01f);
    visible.applyCPUOnly(source, visibleOutput);

    const auto suppressedEdge = suppressedOutput.image().getPixel(2, 3);
    EXPECT_FLOAT_EQ(suppressedEdge.r(), 0.0f);
    EXPECT_FLOAT_EQ(suppressedEdge.g(), 0.0f);
    EXPECT_FLOAT_EQ(suppressedEdge.b(), 0.0f);
    EXPECT_FLOAT_EQ(suppressedEdge.a(), 1.0f);

    const auto visibleEdge = visibleOutput.image().getPixel(2, 3);
    EXPECT_NEAR(visibleEdge.r(), 0.08f, 1.0e-5f);
    EXPECT_NEAR(visibleEdge.g(), 0.08f, 1.0e-5f);
    EXPECT_NEAR(visibleEdge.b(), 0.08f, 1.0e-5f);
    EXPECT_FLOAT_EQ(visibleEdge.a(), 1.0f);

    const auto untouchedBorder = visibleOutput.image().getPixel(0, 0);
    EXPECT_FLOAT_EQ(untouchedBorder.r(), 0.0f);
    EXPECT_FLOAT_EQ(untouchedBorder.a(), 0.3f);
}

TEST(EdgeEffectsContractTest, RimLightAddsColorOnlyAtFacingAlphaBoundary)
{
    const auto source = makeSurface(makeOpaqueSquare());
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::RimLightEffect effect;
    effect.setAngle(0.0f);
    effect.setWidth(1.0f);
    effect.setSoftness(0.0f);
    effect.setIntensity(1.0f);
    effect.setMix(1.0f);
    effect.setColor(QColor(255, 0, 0, 255));

    effect.applyCPUOnly(source, output);

    const auto rightEdge = output.image().getPixel(4, 3);
    EXPECT_NEAR(rightEdge.r(), 1.1f, 1.0e-6f);
    EXPECT_FLOAT_EQ(rightEdge.g(), 0.1f);
    EXPECT_FLOAT_EQ(rightEdge.b(), 0.1f);
    EXPECT_FLOAT_EQ(rightEdge.a(), 1.0f);

    const auto interior = output.image().getPixel(3, 3);
    EXPECT_FLOAT_EQ(interior.r(), 0.1f);
    EXPECT_FLOAT_EQ(interior.a(), 1.0f);
}

TEST(EdgeEffectsContractTest, EdgeAndRimSettingsClampAndResetNonFiniteValues)
{
    Artifact::EdgeEffect edge;
    edge.setMode(-1.0f);
    EXPECT_FLOAT_EQ(edge.mode(), 0.0f);
    edge.setMode(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(edge.mode(), 0.0f);
    edge.setIntensity(12.0f);
    EXPECT_FLOAT_EQ(edge.intensity(), 10.0f);
    edge.setIntensity(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(edge.intensity(), 1.0f);
    edge.setThreshold(-1.0f);
    EXPECT_FLOAT_EQ(edge.threshold(), 0.0f);
    edge.setThreshold(2.0f);
    EXPECT_FLOAT_EQ(edge.threshold(), 1.0f);
    edge.setInvert(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(edge.invert(), 0.0f);

    Artifact::RimLightEffect rim;
    rim.setAngle(-90.0f);
    EXPECT_FLOAT_EQ(rim.angle(), 270.0f);
    rim.setAngle(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(rim.angle(), 315.0f);
    rim.setWidth(0.0f);
    EXPECT_FLOAT_EQ(rim.width(), 1.0f);
    rim.setWidth(100.0f);
    EXPECT_FLOAT_EQ(rim.width(), 64.0f);
    rim.setSoftness(2.0f);
    EXPECT_FLOAT_EQ(rim.softness(), 1.0f);
    rim.setIntensity(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(rim.intensity(), 1.5f);
    rim.setMix(-1.0f);
    EXPECT_FLOAT_EQ(rim.mix(), 0.0f);
}
