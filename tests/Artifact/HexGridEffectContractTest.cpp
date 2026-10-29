#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.HexGrid;
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

TEST(HexGridEffectContractTest, HexGridDrawsCellBoundaryAndInterior)
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 0.3f;
        pixels[pixel * 4 + 1] = 0.4f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = 0.6f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    HexGridEffect effect;
    effect.setCellSize(8.0f);
    effect.setLineWidth(2.0f);
    effect.setAngle(0.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).r(), 0.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(4, 3).r(), 1.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(4, 3).a(), 1.0f);
    EXPECT_NEAR(source.image().getPixel(4, 3).r(), 0.3f, 1.0e-6f);
}


TEST(HexGridEffectContractTest,
     GeneratedGridIsBinaryGrayscaleAndOpaqueWithoutMutatingSource)
{
    constexpr int width = 5;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (int index = 0; index < width * height; ++index) {
        pixels[index * 4] = static_cast<float>(index) / (width * height);
        pixels[index * 4 + 1] = 0.2f;
        pixels[index * 4 + 2] = 0.8f;
        pixels[index * 4 + 3] = static_cast<float>(index % 4) / 4.0f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    HexGridEffect effect;
    effect.setCellSize(8.0f);
    effect.setLineWidth(1.0f);
    effect.setAngle(30.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto original = source.image().getPixel(x, y);
            EXPECT_TRUE(actual.r() == 0.0f || actual.r() == 1.0f);
            EXPECT_FLOAT_EQ(actual.r(), actual.g());
            EXPECT_FLOAT_EQ(actual.g(), actual.b());
            EXPECT_FLOAT_EQ(actual.a(), 1.0f);
            EXPECT_FLOAT_EQ(original.r(), pixels[(y * width + x) * 4]);
            EXPECT_FLOAT_EQ(original.g(), pixels[(y * width + x) * 4 + 1]);
            EXPECT_FLOAT_EQ(original.b(), pixels[(y * width + x) * 4 + 2]);
            EXPECT_FLOAT_EQ(original.a(), pixels[(y * width + x) * 4 + 3]);
        }
    }
}

TEST(HexGridEffectContractTest, SettersClampDimensionsAndResetNonFiniteValues)
{
    HexGridEffect effect;
    effect.setCellSize(-1.0f);
    effect.setLineWidth(0.0f);
    effect.setAngle(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.cellSize(), 4.0f);
    EXPECT_FLOAT_EQ(effect.lineWidth(), 0.5f);
    EXPECT_FLOAT_EQ(effect.angle(), 0.0f);

    effect.setCellSize(std::numeric_limits<float>::infinity());
    effect.setLineWidth(std::numeric_limits<float>::infinity());
    effect.setAngle(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.cellSize(), 32.0f);
    EXPECT_FLOAT_EQ(effect.lineWidth(), 2.0f);
    EXPECT_FLOAT_EQ(effect.angle(), 0.0f);
}
