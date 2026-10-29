#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Voronoi;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 16;
constexpr int kHeight = 12;
constexpr std::size_t kChannelCount =
    static_cast<std::size_t>(kWidth * kHeight * 4);

ImageF32x4RGBAWithCache makeSolidSurface(float red, float green, float blue, float alpha)
{
    std::array<float, kChannelCount> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = red;
        pixels[i + 1] = green;
        pixels[i + 2] = blue;
        pixels[i + 3] = alpha;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(VoronoiEffectContractTest, FixedSeedGeneratesOpaqueGrayscaleIndependentOfInputColor)
{
    const auto firstInput = makeSolidSurface(0.1f, 0.2f, 0.3f, 0.4f);
    const auto secondInput = makeSolidSurface(0.8f, 0.6f, 0.4f, 0.9f);
    VoronoiEffect effect;
    effect.setScale(5.0f);
    effect.setJitter(0.8f);
    effect.setMode(3);
    effect.setSeed(123);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;

    effect.applyCPUOnly(firstInput, firstOutput);
    effect.applyCPUOnly(secondInput, secondOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* second = secondOutput.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            EXPECT_FLOAT_EQ(first[offset], second[offset]);
            EXPECT_FLOAT_EQ(first[offset + 1], first[offset]);
            EXPECT_FLOAT_EQ(first[offset + 2], first[offset]);
            EXPECT_FLOAT_EQ(first[offset + 3], 1.0f);
        }
    }
}

TEST(VoronoiEffectContractTest, EveryModeProducesBoundedOpaqueGrayscale)
{
    const auto source = makeSolidSurface(0.2f, 0.4f, 0.8f, 0.3f);
    VoronoiEffect effect;
    effect.setScale(4.0f);
    effect.setJitter(1.0f);
    effect.setSeed(21);

    for (int mode = 0; mode <= 3; ++mode) {
        effect.setMode(mode);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);

        const float* pixels = output.image().rgba32fData();
        ASSERT_NE(pixels, nullptr) << "mode=" << mode;
        for (std::size_t pixel = 0; pixel < kChannelCount; pixel += 4) {
            EXPECT_GE(pixels[pixel], 0.0f) << "mode=" << mode;
            EXPECT_LE(pixels[pixel], 1.0f) << "mode=" << mode;
            EXPECT_FLOAT_EQ(pixels[pixel + 1], pixels[pixel])
                << "mode=" << mode;
            EXPECT_FLOAT_EQ(pixels[pixel + 2], pixels[pixel])
                << "mode=" << mode;
            EXPECT_FLOAT_EQ(pixels[pixel + 3], 1.0f)
                << "mode=" << mode;
        }
    }
}

TEST(VoronoiEffectContractTest, ScaleJitterModeAndSeedClampToSupportedRanges)
{
    VoronoiEffect effect;
    effect.setScale(-1.0f);
    effect.setJitter(3.0f);
    effect.setMode(-1);
    effect.setSeed(-10);
    EXPECT_FLOAT_EQ(effect.scale(), 1.0f);
    EXPECT_FLOAT_EQ(effect.jitter(), 2.0f);
    EXPECT_EQ(effect.mode(), 0);
    EXPECT_EQ(effect.seed(), 0);

    effect.setScale(250.0f);
    effect.setJitter(-1.0f);
    effect.setMode(8);
    effect.setSeed(12000);
    EXPECT_FLOAT_EQ(effect.scale(), 200.0f);
    EXPECT_FLOAT_EQ(effect.jitter(), 0.0f);
    EXPECT_EQ(effect.mode(), 3);
    EXPECT_EQ(effect.seed(), 9999);

    effect.setScale(std::numeric_limits<float>::quiet_NaN());
    effect.setJitter(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.scale(), 20.0f);
    EXPECT_FLOAT_EQ(effect.jitter(), 1.0f);
}
