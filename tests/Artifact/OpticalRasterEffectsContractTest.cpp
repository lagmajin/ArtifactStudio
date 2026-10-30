#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.ChromaticAberration;
import Artifact.Effect.Rasterizer.Vignette;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 9;
constexpr int kHeight = 9;

std::array<float, kWidth * kHeight * 4> makeGradientSurfaceData()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const int offset = (y * kWidth + x) * 4;
            pixels[offset] = static_cast<float>(x) / 10.0f;
            pixels[offset + 1] = static_cast<float>(y) / 10.0f;
            pixels[offset + 2] = static_cast<float>(x) / 10.0f;
            pixels[offset + 3] = 0.5f;
        }
    }
    return pixels;
}

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(OpticalRasterEffectsContractTest, ChromaticAberrationShiftsRedOutwardAndBlueInward)
{
    const auto pixels = makeGradientSurfaceData();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    ChromaticAberrationEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setRedShift(4.0f);
    effect.setBlueShift(4.0f);

    effect.applyCPUOnly(source, output);

    const auto actual = output.image().getPixel(7, 4);
    const auto sourceRed = source.image().getPixel(8, 4);
    const auto sourceBlue = source.image().getPixel(5, 4);
    const auto sourceAtPixel = source.image().getPixel(7, 4);
    EXPECT_NEAR(actual.r(), sourceRed.r(), 1.0e-6f);
    EXPECT_NEAR(actual.b(), sourceBlue.b(), 1.0e-6f);
    EXPECT_NEAR(actual.g(), sourceAtPixel.g(), 1.0e-6f);
    EXPECT_NEAR(actual.a(), sourceAtPixel.a(), 1.0e-6f);
    EXPECT_NEAR(sourceAtPixel.r(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(sourceAtPixel.b(), 0.7f, 1.0e-6f);
}

TEST(OpticalRasterEffectsContractTest, VignetteDarkensCornersAndPreservesAlphaAndInput)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        pixels[pixel * 4] = 0.8f;
        pixels[pixel * 4 + 1] = 0.6f;
        pixels[pixel * 4 + 2] = 0.4f;
        pixels[pixel * 4 + 3] = 0.75f;
    }
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    VignetteEffect effect;
    effect.applyCPUOnly(source, output);

    const auto corner = output.image().getPixel(0, 0);
    const auto center = output.image().getPixel(kWidth / 2, kHeight / 2);
    EXPECT_LT(corner.r(), center.r());
    EXPECT_NEAR(corner.g() / corner.r(), 0.75f, 1.0e-5f);
    EXPECT_NEAR(corner.b() / corner.r(), 0.5f, 1.0e-5f);
    EXPECT_NEAR(corner.a(), 0.75f, 1.0e-6f);
    EXPECT_NEAR(center.a(), 0.75f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.8f, 1.0e-6f);
}
