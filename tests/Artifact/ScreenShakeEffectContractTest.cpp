#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.ScreenShake;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixels = std::array<float, 16>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 2);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void expectPixelsEqual(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                       const Pixels& expected)
{
    const float* actual = image.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < expected.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel]);
    }
}

} // namespace

TEST(ScreenShakeEffectContractTest, ZeroAmplitudePreservesPixelsForEveryWrapMode)
{
    const Pixels pixels{
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 0.8f, 0.7f, 0.6f, 0.3f, 0.2f, 0.1f, 1.0f,
    };
    Artifact::EffectContext context;
    context.timeSeconds = 1.25;

    for (int wrapMode = 0; wrapMode <= 2; ++wrapMode) {
        Artifact::ScreenShakeEffect effect;
        effect.setAmplitudeX(0.0f);
        effect.setAmplitudeY(0.0f);
        effect.setWrapMode(wrapMode);
        effect.setContext(context);
        ArtifactCore::ImageF32x4RGBAWithCache output;

        effect.applyCPUOnly(makeImage(pixels), output);

        expectPixelsEqual(output, pixels);
    }
}

TEST(ScreenShakeEffectContractTest, SettersClampNonNegativeRangesAndWrapMode)
{
    Artifact::ScreenShakeEffect effect;
    effect.setAmplitudeX(-1.0f);
    EXPECT_FLOAT_EQ(effect.amplitudeX(), 0.0f);
    effect.setAmplitudeY(-1.0f);
    EXPECT_FLOAT_EQ(effect.amplitudeY(), 0.0f);
    effect.setFrequency(0.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), 0.01f);
    effect.setDecay(-1.0f);
    EXPECT_FLOAT_EQ(effect.decay(), 0.0f);
    effect.setWrapMode(-1);
    EXPECT_EQ(effect.wrapMode(), 0);
    effect.setWrapMode(5);
    EXPECT_EQ(effect.wrapMode(), 2);
}
