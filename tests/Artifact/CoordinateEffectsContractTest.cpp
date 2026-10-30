#include <gtest/gtest.h>

#include <array>
#include <cmath>

import Artifact.Effect.Abstract;
import Artifact.Effect.Distort.DisplacementMap;
import Artifact.Effect.Rasterizer.Ripple;
import Artifact.Effect.Wave;
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

TEST(CoordinateEffectsContractTest, WaveUsesCosineOffsetAndClampsAtImageEdge)
{
    constexpr std::array<float, 20> pixels = {
        0.0f, 0.1f, 0.2f, 1.0f,
        0.2f, 0.3f, 0.4f, 1.0f,
        0.4f, 0.5f, 0.6f, 1.0f,
        0.6f, 0.7f, 0.8f, 1.0f,
        0.8f, 0.9f, 1.0f, 1.0f,
    };
    const auto source = makeSurface(pixels.data(), 5, 1);
    ImageF32x4RGBAWithCache output;
    WaveEffect effect;
    effect.setAmplitude(1.0f);
    effect.setFrequency(0.0f);
    effect.setPhase(0.0f);
    effect.setWaveType(1);
    effect.setOrientation(0);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(0, 0).r(),
                source.image().getPixel(1, 0).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(3, 0).g(),
                source.image().getPixel(4, 0).g(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(4, 0).b(),
                source.image().getPixel(4, 0).b(), 1.0e-6f);
    EXPECT_FLOAT_EQ(source.image().getPixel(0, 0).r(), 0.0f);
}

TEST(CoordinateEffectsContractTest, DisplacementMapSamplesClampedAlphaDrivenOffset)
{
    constexpr std::array<float, 16> pixels = {
        0.1f, 0.2f, 0.3f, 1.0f,
        0.3f, 0.4f, 0.5f, 1.0f,
        0.5f, 0.6f, 0.7f, 1.0f,
        0.7f, 0.8f, 0.9f, 1.0f,
    };
    const auto source = makeSurface(pixels.data(), 4, 1);
    ImageF32x4RGBAWithCache output;
    DisplacementMapEffect effect;
    effect.setMaxHorizontal(2.0f);
    effect.setMaxVertical(0.0f);
    effect.setHorizontalChannel(DisplaceChannel::Alpha);
    effect.setVerticalChannel(DisplaceChannel::Alpha);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(0, 0).r(),
                source.image().getPixel(1, 0).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(2, 0).g(),
                source.image().getPixel(3, 0).g(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(3, 0).b(),
                source.image().getPixel(3, 0).b(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(0, 0).a(), 1.0f, 1.0e-6f);
    EXPECT_FLOAT_EQ(source.image().getPixel(0, 0).r(), 0.1f);
}

TEST(CoordinateEffectsContractTest, RipplePreservesCenterAndOpaqueAlpha)
{
    constexpr int width = 16;
    constexpr int height = 16;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = static_cast<float>(x) / static_cast<float>(width - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[offset + 2] = 0.3f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    RippleEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setAmplitude(4.0f);
    effect.setFrequency(0.25f);
    effect.setDecay(0.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(8, 8).r(),
                source.image().getPixel(8, 8).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(8, 8).g(),
                source.image().getPixel(8, 8).g(), 1.0e-6f);
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    bool changedRgb = false;
    for (int pixel = 0; pixel < width * height; ++pixel) {
        changedRgb = changedRgb ||
            std::abs(outputPixels[pixel * 4] - pixels[pixel * 4]) > 1.0e-5f ||
            std::abs(outputPixels[pixel * 4 + 1] - pixels[pixel * 4 + 1]) > 1.0e-5f;
        EXPECT_NEAR(outputPixels[pixel * 4 + 3], 1.0f, 1.0e-6f)
            << "alpha pixel=" << pixel;
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.0f, 1.0e-6f);
}
