#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Kaleidoscope;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 8;
using Pixels = std::array<float, kWidth * kHeight * 4>;

std::size_t pixelOffset(int x, int y)
{
    return (static_cast<std::size_t>(y) * kWidth + x) * 4;
}

Pixels makeMarkedPixels()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset = pixelOffset(x, y);
            const float value = static_cast<float>(y * kWidth + x);
            pixels[offset] = value / 64.0f;
            pixels[offset + 1] = (value + 16.0f) / 80.0f;
            pixels[offset + 2] = (value + 32.0f) / 96.0f;
            pixels[offset + 3] = 0.3f + value * 0.005f;
        }
    }
    return pixels;
}

ArtifactCore::ImageF32x4RGBAWithCache makeSurface(const Pixels& rgba)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(KaleidoscopeEffectContractTest, MirrorReflectsOppositeRadialSector)
{
    const Pixels pixels = makeMarkedPixels();
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache mirroredOutput;
    ArtifactCore::ImageF32x4RGBAWithCache rotatedOutput;
    Artifact::KaleidoscopeEffect mirrored;
    mirrored.setSegments(4);
    mirrored.applyCPUOnly(source, mirroredOutput);

    Artifact::KaleidoscopeEffect rotated;
    rotated.setSegments(4);
    rotated.setMirror(false);
    rotated.applyCPUOnly(source, rotatedOutput);

    // With four mirrored sectors, the left radial axis folds onto +X.
    // Turning mirroring off keeps the same point on the left radial axis.
    const auto leftMirrored = mirroredOutput.image().getPixel(2, 4);
    const std::size_t rightOffset = pixelOffset(6, 4);
    EXPECT_FLOAT_EQ(leftMirrored.r(), pixels[rightOffset]);
    EXPECT_FLOAT_EQ(leftMirrored.g(), pixels[rightOffset + 1]);
    EXPECT_FLOAT_EQ(leftMirrored.b(), pixels[rightOffset + 2]);
    EXPECT_FLOAT_EQ(leftMirrored.a(), pixels[rightOffset + 3]);

    const auto leftRotated = rotatedOutput.image().getPixel(2, 4);
    const std::size_t leftOffset = pixelOffset(2, 4);
    EXPECT_FLOAT_EQ(leftRotated.r(), pixels[leftOffset]);
    EXPECT_FLOAT_EQ(leftRotated.g(), pixels[leftOffset + 1]);
    EXPECT_FLOAT_EQ(leftRotated.b(), pixels[leftOffset + 2]);
    EXPECT_FLOAT_EQ(leftRotated.a(), pixels[leftOffset + 3]);
}

TEST(KaleidoscopeEffectContractTest, GpuSpatialDescriptorCarriesSettings)
{
    Artifact::KaleidoscopeEffect effect;
    effect.setSegments(9);
    effect.setCenterX(0.35f);
    effect.setCenterY(0.6f);
    effect.setRotation(25.0f);
    effect.setZoom(1.25f);
    effect.setFeather(0.4f);
    effect.setMirror(false);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind,
              Artifact::GpuSpatialEffectKind::Kaleidoscope);
    const std::array<float, 7> expected{
        9.0f, 0.35f, 0.6f, 25.0f, 1.25f, 0.4f, 0.0f};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FLOAT_EQ(stack.nodes[0].parameters[i], expected[i]);
    }
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 0u);
}

TEST(KaleidoscopeEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::KaleidoscopeEffect effect;
    effect.setSegments(1);
    EXPECT_EQ(effect.segments(), 2);
    effect.setSegments(100);
    EXPECT_EQ(effect.segments(), 64);

    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    effect.setCenterY(2.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);
    effect.setCenterY(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);

    effect.setRotation(450.0f);
    EXPECT_FLOAT_EQ(effect.rotation(), 90.0f);
    effect.setRotation(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.rotation(), 0.0f);
    effect.setZoom(-1.0f);
    EXPECT_FLOAT_EQ(effect.zoom(), 0.01f);
    effect.setZoom(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.zoom(), 1.0f);
    effect.setFeather(2.0f);
    EXPECT_FLOAT_EQ(effect.feather(), 1.0f);
    effect.setFeather(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.feather(), 0.0f);
    effect.setMirror(false);
    EXPECT_FALSE(effect.mirror());
}
