#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.ScreenShake;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 6;

ImageF32x4RGBAWithCache makePatternSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / static_cast<float>(kWidth + 1);
            pixels[offset + 1] = static_cast<float>(y + 1) / static_cast<float>(kHeight + 1);
            pixels[offset + 2] = static_cast<float>(x + y + 1) /
                                 static_cast<float>(kWidth + kHeight + 1);
            pixels[offset + 3] = 0.65f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(ScreenShakeEffectContractTest, ZeroAmplitudePreservesImageForEveryWrapMode)
{
    const auto source = makePatternSurface();
    EffectContext context;
    context.timeSeconds = 2.75;

    ScreenShakeEffect effect;
    effect.setAmplitudeX(0.0f);
    effect.setAmplitudeY(0.0f);

    for (int wrapMode = 0; wrapMode <= 2; ++wrapMode) {
        effect.setWrapMode(wrapMode);
        effect.setContext(context);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);

        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const auto actual = output.image().getPixel(x, y);
                const auto expected = source.image().getPixel(x, y);
                EXPECT_NEAR(actual.r(), expected.r(), 1.0e-6f)
                    << "wrapMode=" << wrapMode << " x=" << x << " y=" << y;
                EXPECT_NEAR(actual.g(), expected.g(), 1.0e-6f)
                    << "wrapMode=" << wrapMode << " x=" << x << " y=" << y;
                EXPECT_NEAR(actual.b(), expected.b(), 1.0e-6f)
                    << "wrapMode=" << wrapMode << " x=" << x << " y=" << y;
                EXPECT_NEAR(actual.a(), expected.a(), 1.0e-6f)
                    << "wrapMode=" << wrapMode << " x=" << x << " y=" << y;
            }
        }
    }
}

TEST(ScreenShakeEffectContractTest, FullyDecayedShakePreservesImageForEveryWrapMode)
{
    const auto source = makePatternSurface();
    EffectContext context;
    context.timeSeconds = 1.0;

    ScreenShakeEffect effect;
    effect.setAmplitudeX(12.0f);
    effect.setAmplitudeY(9.0f);
    effect.setDecay(1.0f);

    for (int wrapMode = 0; wrapMode <= 2; ++wrapMode) {
        effect.setWrapMode(wrapMode);
        effect.setContext(context);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);

        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const auto actual = output.image().getPixel(x, y);
                const auto expected = source.image().getPixel(x, y);
                EXPECT_FLOAT_EQ(actual.r(), expected.r())
                    << "wrapMode=" << wrapMode << " x=" << x << " y=" << y;
                EXPECT_FLOAT_EQ(actual.g(), expected.g())
                    << "wrapMode=" << wrapMode << " x=" << x << " y=" << y;
                EXPECT_FLOAT_EQ(actual.b(), expected.b())
                    << "wrapMode=" << wrapMode << " x=" << x << " y=" << y;
                EXPECT_FLOAT_EQ(actual.a(), expected.a())
                    << "wrapMode=" << wrapMode << " x=" << x << " y=" << y;
            }
        }
    }
}

TEST(ScreenShakeEffectContractTest, ShakeIsRepeatableForTheSameSeedAndChangesWithSeed)
{
    const auto source = makePatternSurface();
    EffectContext context;
    context.timeSeconds = 2.375;

    ScreenShakeEffect effect;
    effect.setAmplitudeX(18.0f);
    effect.setAmplitudeY(14.0f);
    effect.setFrequency(1.7f);
    effect.setDecay(0.0f);
    effect.setSeed(31);
    effect.setWrapMode(0);
    effect.setContext(context);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache repeatedOutput;
    ImageF32x4RGBAWithCache changedSeedOutput;

    effect.applyCPUOnly(source, firstOutput);
    effect.applyCPUOnly(source, repeatedOutput);
    effect.setSeed(32);
    effect.applyCPUOnly(source, changedSeedOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* repeated = repeatedOutput.image().rgba32fData();
    const float* changedSeed = changedSeedOutput.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(repeated, nullptr);
    ASSERT_NE(changedSeed, nullptr);
    bool seedChangedPixels = false;
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(first[channel], repeated[channel]) << "channel=" << channel;
        seedChangedPixels = seedChangedPixels ||
                            std::abs(first[channel] - changedSeed[channel]) > 1.0e-6f;
    }
    EXPECT_TRUE(seedChangedPixels);
}
