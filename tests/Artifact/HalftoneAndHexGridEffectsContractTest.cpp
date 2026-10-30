#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Halftone;
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

TEST(HalftoneAndHexGridEffectsContractTest, HalftoneCreatesDotCentersAndPreservesSource)
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 1.0f;
        pixels[pixel * 4 + 1] = 1.0f;
        pixels[pixel * 4 + 2] = 1.0f;
        pixels[pixel * 4 + 3] = 0.5f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    HalftoneEffect effect;
    effect.setDotSize(4.0f);
    effect.setAngle(0.0f);
    effect.setContrast(1.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).r(), 0.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(2, 0).r(), 1.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).a(), 1.0f);
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(0, 0).a(), 0.5f, 1.0e-6f);
}

TEST(HalftoneAndHexGridEffectsContractTest, HexGridDrawsCellBoundaryAndInterior)
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
