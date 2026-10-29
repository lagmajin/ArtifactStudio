#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Liquify;
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

TEST(LiquifyEffectContractTest, PushMovesCenterAlongBrushAngleAndLeavesOutsideRadiusUnchanged)
{
    constexpr int width = 32;
    constexpr int height = 32;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = static_cast<float>(x) / static_cast<float>(width - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[offset + 2] = 0.25f;
            pixels[offset + 3] = 0.7f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    LiquifyEffect effect;
    effect.setBrushType(LiquifyBrushType::Push);
    effect.setAmount(100.0f);
    effect.setRadius(0.5f);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setAngle(0.0f);

    effect.applyCPUOnly(source, output);

    const auto movedCenter = output.image().getPixel(16, 16);
    EXPECT_NEAR(movedCenter.r(), source.image().getPixel(31, 16).r(), 1.0e-6f);
    EXPECT_NEAR(movedCenter.g(), source.image().getPixel(31, 16).g(), 1.0e-6f);
    EXPECT_NEAR(movedCenter.a(), 0.7f, 1.0e-6f);
    const auto outside = output.image().getPixel(0, 0);
    EXPECT_NEAR(outside.r(), source.image().getPixel(0, 0).r(), 1.0e-6f);
    EXPECT_NEAR(outside.g(), source.image().getPixel(0, 0).g(), 1.0e-6f);
    EXPECT_NEAR(outside.a(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(16, 16).r(), 16.0f / 31.0f, 1.0e-6f);
}

TEST(LiquifyEffectContractTest, ZeroAmountIsIdentityForEveryBrushType)
{
    constexpr int width = 32;
    constexpr int height = 32;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x) / static_cast<float>(width - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[offset + 2] = static_cast<float>(x + y) /
                                 static_cast<float>(width + height - 2);
            pixels[offset + 3] = 0.7f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    constexpr std::array brushTypes = {
        LiquifyBrushType::Push,
        LiquifyBrushType::Pinch,
        LiquifyBrushType::Bloat,
        LiquifyBrushType::Twirl,
        LiquifyBrushType::Turbulence,
        LiquifyBrushType::Pucker,
    };

    LiquifyEffect effect;
    effect.setAmount(0.0f);
    effect.setRadius(0.5f);
    for (const LiquifyBrushType brushType : brushTypes) {
        effect.setBrushType(brushType);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);

        ASSERT_EQ(output.image().width(), width);
        ASSERT_EQ(output.image().height(), height);
        const float* outputPixels = output.image().rgba32fData();
        ASSERT_NE(outputPixels, nullptr);
        for (std::size_t channel = 0;
             channel < static_cast<std::size_t>(width * height * 4); ++channel) {
            EXPECT_NEAR(outputPixels[channel], sourcePixels[channel], 1.0e-6f)
                << "brush=" << static_cast<int>(brushType)
                << " channel=" << channel;
        }
    }
}

TEST(LiquifyEffectContractTest, TurbulenceIsRepeatableForTheSameSeedAndChangesWithSeed)
{
    constexpr int width = 32;
    constexpr int height = 32;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x) / static_cast<float>(width - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[offset + 2] = static_cast<float>(x + y) /
                                 static_cast<float>(width + height - 2);
            pixels[offset + 3] = 0.7f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    LiquifyEffect effect;
    effect.setBrushType(LiquifyBrushType::Turbulence);
    effect.setAmount(100.0f);
    effect.setRadius(0.5f);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setTurbulenceSeed(23);

    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache repeatedOutput;
    ImageF32x4RGBAWithCache changedSeedOutput;
    effect.applyCPUOnly(source, firstOutput);
    effect.applyCPUOnly(source, repeatedOutput);
    effect.setTurbulenceSeed(24);
    effect.applyCPUOnly(source, changedSeedOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* repeated = repeatedOutput.image().rgba32fData();
    const float* changedSeed = changedSeedOutput.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(repeated, nullptr);
    ASSERT_NE(changedSeed, nullptr);
    bool seedChangedPixels = false;
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(width * height * 4); ++channel) {
        EXPECT_FLOAT_EQ(first[channel], repeated[channel]) << "channel=" << channel;
        seedChangedPixels = seedChangedPixels ||
                            std::abs(first[channel] - changedSeed[channel]) > 1.0e-6f;
    }
    EXPECT_TRUE(seedChangedPixels);
}
