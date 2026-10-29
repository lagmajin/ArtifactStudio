#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.ChromaticAberration;
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
            pixels[offset] = (value + 1.0f) / 65.0f;
            pixels[offset + 1] = 0.2f + value / 100.0f;
            pixels[offset + 2] = 1.0f - value / 65.0f;
            pixels[offset + 3] = 0.4f + (value - 1.0f) * 0.005f;
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

TEST(ChromaticAberrationEffectContractTest,
     ShiftsRedAndBlueOutwardWhileKeepingGreenAndAlphaAtThePixel)
{
    const Pixels pixels = makeMarkedPixels();
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::ChromaticAberrationEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setRedShift(10.0f);
    effect.setBlueShift(10.0f);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(4, 4);
    const std::size_t centerOffset = pixelOffset(4, 4);
    EXPECT_FLOAT_EQ(center.r(), pixels[centerOffset]);
    EXPECT_FLOAT_EQ(center.g(), pixels[centerOffset + 1]);
    EXPECT_FLOAT_EQ(center.b(), pixels[centerOffset + 2]);
    EXPECT_FLOAT_EQ(center.a(), pixels[centerOffset + 3]);

    // On the horizontal radius, the configured red and blue offsets round
    // to source x=7 and x=2 respectively; green and alpha stay at x=6.
    const auto fringe = output.image().getPixel(6, 4);
    const std::size_t redOffset = pixelOffset(7, 4);
    const std::size_t blueOffset = pixelOffset(2, 4);
    const std::size_t greenAlphaOffset = pixelOffset(6, 4);
    EXPECT_FLOAT_EQ(fringe.r(), pixels[redOffset]);
    EXPECT_FLOAT_EQ(fringe.g(), pixels[greenAlphaOffset + 1]);
    EXPECT_FLOAT_EQ(fringe.b(), pixels[blueOffset + 2]);
    EXPECT_FLOAT_EQ(fringe.a(), pixels[greenAlphaOffset + 3]);
}

TEST(ChromaticAberrationEffectContractTest,
     GpuSpatialDescriptorCarriesShiftsAndCenter)
{
    Artifact::ChromaticAberrationEffect effect;
    effect.setRedShift(3.0f);
    effect.setBlueShift(4.0f);
    effect.setCenterX(0.25f);
    effect.setCenterY(0.75f);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind,
              Artifact::GpuSpatialEffectKind::ChromaticAberration);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 3.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 4.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 0.25f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 0.75f);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1));
}

TEST(ChromaticAberrationEffectContractTest,
     SettersClampFiniteValuesAndResetNonFiniteValues)
{
    Artifact::ChromaticAberrationEffect effect;
    effect.setRedShift(-1.0f);
    EXPECT_FLOAT_EQ(effect.redShift(), 0.0f);
    effect.setRedShift(60.0f);
    EXPECT_FLOAT_EQ(effect.redShift(), 50.0f);
    effect.setRedShift(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.redShift(), 2.0f);

    effect.setBlueShift(-1.0f);
    EXPECT_FLOAT_EQ(effect.blueShift(), 0.0f);
    effect.setBlueShift(60.0f);
    EXPECT_FLOAT_EQ(effect.blueShift(), 50.0f);
    effect.setBlueShift(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.blueShift(), 2.0f);

    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    effect.setCenterY(2.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);
    effect.setCenterY(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);
}
