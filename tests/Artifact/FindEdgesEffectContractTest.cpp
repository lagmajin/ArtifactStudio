#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.FindEdges;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixels = std::array<float, 36>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 3, 3);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

Pixels makeStepImage()
{
    Pixels pixels{};
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * 3 + x) * 4);
            const float value = x == 1 ? 1.0f : 0.0f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 0.2f + 0.1f * static_cast<float>(y * 3 + x);
        }
    }
    return pixels;
}

} // namespace

TEST(FindEdgesEffectContractTest, ZeroAmountPreservesInputExactly)
{
    const Pixels pixels = makeStepImage();
    Artifact::FindEdgesEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setAmount(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel]) << "channel=" << channel;
    }
}

TEST(FindEdgesEffectContractTest, InvertMapsFlatImageToWhiteAndPreservesAlpha)
{
    Pixels pixels{};
    for (std::size_t pixel = 0; pixel < 9; ++pixel) {
        pixels[pixel * 4] = 0.2f;
        pixels[pixel * 4 + 1] = 0.2f;
        pixels[pixel * 4 + 2] = 0.2f;
        pixels[pixel * 4 + 3] = 0.1f + static_cast<float>(pixel) * 0.08f;
    }
    Artifact::FindEdgesEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setInvert(true);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t pixel = 0; pixel < 9; ++pixel) {
        EXPECT_FLOAT_EQ(actual[pixel * 4], 1.0f);
        EXPECT_FLOAT_EQ(actual[pixel * 4 + 1], 1.0f);
        EXPECT_FLOAT_EQ(actual[pixel * 4 + 2], 1.0f);
        EXPECT_FLOAT_EQ(actual[pixel * 4 + 3], pixels[pixel * 4 + 3]);
    }
}
