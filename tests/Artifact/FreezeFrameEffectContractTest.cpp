#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.FreezeFrame;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 3;

ImageF32x4RGBAWithCache makeUniformFrame(float value, float alpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = value;
        pixels[i + 1] = value;
        pixels[i + 2] = value;
        pixels[i + 3] = alpha;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(FreezeFrameEffectContractTest, HoldsLastCapturedFrameUntilReleased)
{
    const auto captured = makeUniformFrame(0.25f, 0.35f);
    const auto later = makeUniformFrame(0.85f, 0.75f);
    const auto liveAgain = makeUniformFrame(0.6f, 0.5f);
    FreezeFrameEffect effect;
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(captured, output);
    EXPECT_FALSE(effect.isFrozen());
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.25f, 1.0e-6f);

    effect.freeze();
    EXPECT_TRUE(effect.isFrozen());
    effect.applyCPUOnly(later, output);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.25f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.35f, 1.0e-6f);

    effect.release();
    EXPECT_FALSE(effect.isFrozen());
    effect.applyCPUOnly(liveAgain, output);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.6f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.5f, 1.0e-6f);
}

TEST(FreezeFrameEffectContractTest, CapturesTheLatestLiveFrameBeforeFreeze)
{
    const auto firstLive = makeUniformFrame(0.2f, 0.3f);
    const auto latestLive = makeUniformFrame(0.7f, 0.8f);
    const auto afterFreeze = makeUniformFrame(0.9f, 0.1f);
    FreezeFrameEffect effect;
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(firstLive, output);
    effect.applyCPUOnly(latestLive, output);
    effect.freeze();
    effect.applyCPUOnly(afterFreeze, output);

    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(latestLive.image().getPixel(1, 1).r(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(afterFreeze.image().getPixel(1, 1).r(), 0.9f, 1.0e-6f);
}

TEST(FreezeFrameEffectContractTest, RepeatedFreezeKeepsTheOriginalCapturedFrame)
{
    const auto captured = makeUniformFrame(0.3f, 0.4f);
    const auto intervening = makeUniformFrame(0.6f, 0.7f);
    const auto later = makeUniformFrame(0.9f, 0.2f);
    FreezeFrameEffect effect;
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(captured, output);
    effect.freeze();
    effect.applyCPUOnly(intervening, output);
    effect.freeze();
    effect.applyCPUOnly(later, output);

    EXPECT_TRUE(effect.isFrozen());
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.4f, 1.0e-6f);
}

TEST(FreezeFrameEffectContractTest, FrozenPropertyControlsFreezeAndReleaseState)
{
    FreezeFrameEffect effect;
    effect.setPropertyValue(UniString("frozen"), QVariant(true));
    EXPECT_TRUE(effect.isFrozen());

    effect.setPropertyValue(UniString("frozen"), QVariant(false));
    EXPECT_FALSE(effect.isFrozen());

    effect.setPropertyValue(UniString("unknown"), QVariant(true));
    EXPECT_FALSE(effect.isFrozen());
}
\n
