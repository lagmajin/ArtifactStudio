#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cmath>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.Glitch;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 32;
constexpr int kHeight = 32;
constexpr std::size_t kChannelCount =
    static_cast<std::size_t>(kWidth * kHeight * 4);

std::array<float, kChannelCount> makeGradientImage()
{
    std::array<float, kChannelCount> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            pixels[offset] = static_cast<float>(x) / static_cast<float>(kWidth - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(kHeight - 1);
            pixels[offset + 2] = static_cast<float>(x + y) /
                                 static_cast<float>(kWidth + kHeight - 2);
            pixels[offset + 3] = 0.6f;
        }
    }
    return pixels;
}

ImageF32x4RGBAWithCache makeSurface(const float* rgba)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

bool differsFrom(const float* lhs, const float* rhs)
{
    for (std::size_t i = 0; i < kChannelCount; ++i) {
        if (std::abs(lhs[i] - rhs[i]) > 1.0e-6f) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(GlitchEffectContractTest, FixedFrameAndSeedAreDeterministicAndKeepAlpha)
{
    const auto pixels = makeGradientImage();
    const auto source = makeSurface(pixels.data());
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    GlitchEffect effect;
    effect.setIntensity(0.8f);
    effect.setColorShift(0.7f);
    effect.setScanlines(0.9f);
    effect.setSeed(37);

    effect.applyCPUOnly(source, firstOutput);
    effect.applyCPUOnly(source, secondOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* second = secondOutput.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(original, nullptr);
    EXPECT_FALSE(differsFrom(first, second));
    EXPECT_TRUE(differsFrom(first, original));
    EXPECT_FALSE(differsFrom(original, pixels.data()));
    for (std::size_t pixel = 0; pixel < kChannelCount; pixel += 4) {
        EXPECT_FLOAT_EQ(first[pixel + 3], 0.6f) << "pixel=" << pixel / 4;
    }
}

TEST(GlitchEffectContractTest, ZeroControlsPreserveEveryChannel)
{
    const auto pixels = makeGradientImage();
    const auto source = makeSurface(pixels.data());
    ImageF32x4RGBAWithCache output;
    GlitchEffect effect;
    effect.setIntensity(0.0f);
    effect.setColorShift(0.0f);
    effect.setScanlines(0.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    EXPECT_FALSE(differsFrom(actual, pixels.data()));
}

TEST(GlitchEffectContractTest, CompositionFrameChangesTheSeededGlitchPattern)
{
    const auto pixels = makeGradientImage();
    const auto source = makeSurface(pixels.data());
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache nextOutput;
    GlitchEffect effect;
    effect.setIntensity(0.8f);
    effect.setColorShift(0.7f);
    effect.setScanlines(0.9f);
    effect.setSeed(37);

    EffectContext context;
    context.compositionFrame = 10;
    effect.setContext(context);
    effect.applyCPUOnly(source, firstOutput);

    context.compositionFrame = 11;
    effect.setContext(context);
    effect.applyCPUOnly(source, nextOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* next = nextOutput.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(next, nullptr);
    EXPECT_TRUE(differsFrom(first, next));
    for (std::size_t pixel = 0; pixel < kChannelCount; pixel += 4) {
        EXPECT_FLOAT_EQ(first[pixel + 3], 0.6f);
        EXPECT_FLOAT_EQ(next[pixel + 3], 0.6f);
    }
}
