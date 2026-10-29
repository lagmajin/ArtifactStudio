#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Artifact.Effect.Rasterizer.Halftone;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;

ArtifactCore::ImageF32x4RGBAWithCache makeSolidSurface(float luma, float alpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = luma;
        pixels[i + 1] = luma;
        pixels[i + 2] = luma;
        pixels[i + 3] = alpha;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(HalftoneEffectContractTest, DarkInputProducesDarkDotAndBrightGaps)
{
    const auto source = makeSolidSurface(0.0f, 0.2f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::HalftoneEffect effect;
    effect.setDotSize(4.0f);
    effect.setAngle(0.0f);
    effect.setContrast(1.0f);

    effect.applyCPUOnly(source, output);

    const auto dot = output.image().getPixel(1, 1);
    EXPECT_FLOAT_EQ(dot.r(), 0.0f);
    EXPECT_FLOAT_EQ(dot.g(), 0.0f);
    EXPECT_FLOAT_EQ(dot.b(), 0.0f);
    EXPECT_FLOAT_EQ(dot.a(), 1.0f);

    const auto gap = output.image().getPixel(2, 2);
    EXPECT_FLOAT_EQ(gap.r(), 1.0f);
    EXPECT_FLOAT_EQ(gap.g(), 1.0f);
    EXPECT_FLOAT_EQ(gap.b(), 1.0f);
    EXPECT_FLOAT_EQ(gap.a(), 1.0f);
}

TEST(HalftoneEffectContractTest, BrightInputLeavesOnlyCellCentersDark)
{
    const auto source = makeSolidSurface(1.0f, 0.6f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::HalftoneEffect effect;
    effect.setDotSize(4.0f);
    effect.setAngle(0.0f);
    effect.setContrast(1.0f);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(0, 0);
    EXPECT_FLOAT_EQ(center.r(), 0.0f);
    EXPECT_FLOAT_EQ(center.g(), 0.0f);
    EXPECT_FLOAT_EQ(center.b(), 0.0f);
    EXPECT_FLOAT_EQ(center.a(), 1.0f);

    const auto outside = output.image().getPixel(1, 0);
    EXPECT_FLOAT_EQ(outside.r(), 1.0f);
    EXPECT_FLOAT_EQ(outside.g(), 1.0f);
    EXPECT_FLOAT_EQ(outside.b(), 1.0f);
    EXPECT_FLOAT_EQ(outside.a(), 1.0f);
}

TEST(HalftoneEffectContractTest,
     CreativeHalftoneUsesEightPixelCellsAndWritesOpaqueMonochrome)
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = 0.5f;
        pixels[offset + 1] = 0.5f;
        pixels[offset + 2] = 0.5f;
        pixels[offset + 3] = 0.25f;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::ArtifactHalftoneEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);

    effect.apply(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_TRUE(pixel.r() == 0.0f || pixel.r() == 1.0f);
            EXPECT_FLOAT_EQ(pixel.g(), pixel.r());
            EXPECT_FLOAT_EQ(pixel.b(), pixel.r());
            EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
        }
    }
    const auto dotCenter = output.image().getPixel(4, 4);
    EXPECT_FLOAT_EQ(dotCenter.r(), 0.0f);
    const auto gap = output.image().getPixel(7, 4);
    EXPECT_FLOAT_EQ(gap.r(), 1.0f);
    const float* unchangedSource = source.image().rgba32fData();
    ASSERT_NE(unchangedSource, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(unchangedSource[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(HalftoneEffectContractTest, SettersClampFiniteValuesAndResetNonFiniteValues)
{
    Artifact::HalftoneEffect effect;
    effect.setDotSize(0.0f);
    EXPECT_FLOAT_EQ(effect.dotSize(), 1.0f);
    effect.setDotSize(600.0f);
    EXPECT_FLOAT_EQ(effect.dotSize(), 512.0f);
    effect.setDotSize(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.dotSize(), 8.0f);

    effect.setAngle(-450.0f);
    EXPECT_FLOAT_EQ(effect.angle(), -450.0f);
    effect.setAngle(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.angle(), 45.0f);

    effect.setContrast(-1.0f);
    EXPECT_FLOAT_EQ(effect.contrast(), 0.0f);
    effect.setContrast(4.0f);
    EXPECT_FLOAT_EQ(effect.contrast(), 3.0f);
    effect.setContrast(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.contrast(), 1.0f);
}
