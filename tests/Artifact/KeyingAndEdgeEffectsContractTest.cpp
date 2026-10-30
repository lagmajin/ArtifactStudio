#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.Rasterizer.Edge;
import FloatRGBA;
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

TEST(KeyingAndEdgeEffectsContractTest, ChromaKeyRemovesConfiguredScreenColorAndKeepsForegroundAlpha)
{
    constexpr std::array<float, 8> pixels = {
        0.0f, 1.0f, 0.0f, 0.8f,
        1.0f, 0.0f, 0.0f, 0.8f,
    };
    const auto source = makeSurface(pixels.data(), 2, 1);
    ImageF32x4RGBAWithCache output;
    ChromaKeyEffect effect;
    effect.setKeyColor(FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f));
    effect.setHueTolerance(0.1f);
    effect.setEdgeSoftness(0.1f);
    effect.setDespillStrength(0.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(0, 0).a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 0).a(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 0).r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(0, 0).a(), 0.8f, 1.0e-6f);
}

TEST(KeyingAndEdgeEffectsContractTest, SobelEdgeFindsStepAndLeavesImageBorderUntouched)
{
    constexpr int width = 5;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float value = x < 2 ? 0.0f : 1.0f;
            const int offset = (y * width + x) * 4;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 0.5f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    EdgeEffect effect;
    effect.setMode(0.0f);
    effect.setIntensity(1.0f);
    effect.setThreshold(0.1f);

    effect.applyCPUOnly(source, output);

    EXPECT_GT(output.image().getPixel(2, 2).r(), 0.1f);
    EXPECT_NEAR(output.image().getPixel(3, 2).r(), 0.0f, 1.0e-6f);
    const auto border = output.image().getPixel(0, 2);
    EXPECT_NEAR(border.r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(border.a(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(2, 2).a(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(2, 2).r(), 1.0f, 1.0e-6f);
}
