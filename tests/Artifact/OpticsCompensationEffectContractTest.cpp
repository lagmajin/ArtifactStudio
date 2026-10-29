#include <gtest/gtest.h>

#include <array>
#include <cmath>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.OpticsCompensation;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 8;

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

std::array<float, kWidth * kHeight * 4> makeGradient()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const int offset = (y * kWidth + x) * 4;
            pixels[offset] = static_cast<float>(x) / 7.0f;
            pixels[offset + 1] = static_cast<float>(y) / 7.0f;
            pixels[offset + 2] = 0.25f;
            pixels[offset + 3] = 1.0f;
        }
    }
    return pixels;
}

} // namespace

TEST(OpticsCompensationEffectContractTest, DirectionReversesTheMappedGradientAndPreservesAlpha)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache undistorted;
    ImageF32x4RGBAWithCache distorted;

    OpticsCompensationEffect positiveDirection;
    positiveDirection.setCenterX(0.5f);
    positiveDirection.setCenterY(0.5f);
    positiveDirection.setFov(45.0f);
    positiveDirection.setDirection(1);
    positiveDirection.applyCPUOnly(source, undistorted);

    OpticsCompensationEffect negativeDirection;
    negativeDirection.setCenterX(0.5f);
    negativeDirection.setCenterY(0.5f);
    negativeDirection.setFov(45.0f);
    negativeDirection.setDirection(-1);
    negativeDirection.applyCPUOnly(source, distorted);

    const float* positivePixels = undistorted.image().rgba32fData();
    const float* negativePixels = distorted.image().rgba32fData();
    ASSERT_NE(positivePixels, nullptr);
    ASSERT_NE(negativePixels, nullptr);
    bool directionsDiffer = false;
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        const int offset = pixel * 4;
        directionsDiffer = directionsDiffer ||
            std::abs(positivePixels[offset] - negativePixels[offset]) > 1.0e-5f ||
            std::abs(positivePixels[offset + 1] - negativePixels[offset + 1]) > 1.0e-5f;
        EXPECT_NEAR(positivePixels[offset + 3], 1.0f, 1.0e-6f);
        EXPECT_NEAR(negativePixels[offset + 3], 1.0f, 1.0e-6f);
    }
    EXPECT_TRUE(directionsDiffer);
    EXPECT_NEAR(source.image().getPixel(4, 4).r(), 4.0f / 7.0f, 1.0e-6f);
}

TEST(OpticsCompensationEffectContractTest, SettersClampCentersAndFovAndNormalizeDirection)
{
    OpticsCompensationEffect effect;

    effect.setCenterX(-0.25f);
    effect.setCenterY(1.25f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);

    effect.setFov(0.0f);
    EXPECT_FLOAT_EQ(effect.fov(), 1.0f);
    effect.setFov(200.0f);
    EXPECT_FLOAT_EQ(effect.fov(), 180.0f);

    effect.setDirection(0);
    EXPECT_EQ(effect.direction(), 1);
    effect.setDirection(-7);
    EXPECT_EQ(effect.direction(), -1);
}
