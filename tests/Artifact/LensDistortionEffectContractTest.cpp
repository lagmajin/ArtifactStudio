#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.LensDistortion;
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

TEST(LensDistortionEffectContractTest, RadialMappingKeepsCenterAndMovesInteriorSamples)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    LensDistortionEffect effect;
    effect.setDistortion(100.0f);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setTransparentEdges(false);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(4, 4).r(),
                source.image().getPixel(4, 4).r(), 1.0e-6f);
    EXPECT_GT(output.image().getPixel(6, 4).r(),
              source.image().getPixel(6, 4).r());
    EXPECT_NEAR(output.image().getPixel(6, 4).a(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(6, 4).r(), 6.0f / 7.0f, 1.0e-6f);
}

TEST(LensDistortionEffectContractTest, TransparentEdgesClearSamplesMappedOutsideImage)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    LensDistortionEffect effect;
    effect.setDistortion(100.0f);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setTransparentEdges(true);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(7, 4).a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(7, 4).r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(4, 4).a(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(7, 4).a(), 1.0f, 1.0e-6f);
}

TEST(LensDistortionEffectContractTest, InvertedDistortionKeepsEdgeSamplesMappedBackInside)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    LensDistortionEffect effect;
    effect.setDistortion(100.0f);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setInvertDistortion(true);
    effect.setTransparentEdges(true);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(7, 4).a(), 1.0f, 1.0e-6f);
    EXPECT_GT(output.image().getPixel(7, 4).r(), 0.0f);
    EXPECT_NEAR(source.image().getPixel(7, 4).a(), 1.0f, 1.0e-6f);
}

TEST(LensDistortionEffectContractTest, NeutralRadialAndTangentialParametersPreserveImage)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    LensDistortionEffect effect;
    effect.setDistortion(0.0f);
    effect.setRadialQuadratic(0.0f);
    effect.setTangentialX(0.0f);
    effect.setTangentialY(0.0f);
    effect.setZoom(1.0f);
    effect.setTransparentEdges(false);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_NEAR(after.r(), before.r(), 1.0e-6f);
            EXPECT_NEAR(after.g(), before.g(), 1.0e-6f);
            EXPECT_NEAR(after.b(), before.b(), 1.0e-6f);
            EXPECT_NEAR(after.a(), before.a(), 1.0e-6f);
        }
    }
}
