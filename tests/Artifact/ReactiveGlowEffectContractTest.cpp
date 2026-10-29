#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.ReactiveGlow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 5;
constexpr int kHeight = 5;

ImageF32x4RGBAWithCache makeUniformSurface(float red, float green, float blue, float alpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
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

TEST(ReactiveGlowEffectContractTest, SaturationCanTriggerGlowBelowLuminanceThreshold)
{
    const auto source = makeUniformSurface(0.8f, 0.1f, 0.1f, 0.6f);
    ImageF32x4RGBAWithCache output;
    ReactiveGlowEffect effect;
    effect.setThreshold(0.9f);
    effect.setRadius(0.5f);
    effect.setIntensity(0.5f);
    effect.setReaction(0.0f);
    effect.setSaturationWeight(1.0f);
    effect.setTintMix(0.0f);

    effect.applyCPUOnly(source, output);

    const auto pixel = output.image().getPixel(2, 2);
    EXPECT_GT(pixel.g(), 0.1f);
    EXPECT_GT(pixel.r(), pixel.g());
    EXPECT_NEAR(pixel.a(), 0.6f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(2, 2).g(), 0.1f, 1.0e-6f);
}

TEST(ReactiveGlowEffectContractTest, ZeroReactionAndSaturationWeightPreserveRgba)
{
    constexpr std::array<float, 16> pixels = {
        0.1f, 0.2f, 0.3f, 0.0f,
        0.4f, 0.5f, 0.6f, 0.25f,
        0.7f, 0.8f, 0.9f, 0.75f,
        0.3f, 0.2f, 0.1f, 1.0f,
    };
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 2);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ReactiveGlowEffect effect;
    effect.setReaction(0.0f);
    effect.setSaturationWeight(0.0f);

    effect.applyCPUOnly(source, output);

    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(outputPixels[channel], pixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}

TEST(ReactiveGlowEffectContractTest, NumericSettingsClampToSupportedRanges)
{
    ReactiveGlowEffect effect;
    effect.setThreshold(-1.0f);
    effect.setRadius(0.0f);
    effect.setIntensity(5.0f);
    effect.setReaction(-1.0f);
    effect.setSaturationWeight(2.0f);
    effect.setTintMix(-1.0f);
    EXPECT_FLOAT_EQ(effect.threshold(), 0.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.5f);
    EXPECT_FLOAT_EQ(effect.intensity(), 4.0f);
    EXPECT_FLOAT_EQ(effect.reaction(), 0.0f);
    EXPECT_FLOAT_EQ(effect.saturationWeight(), 1.0f);
    EXPECT_FLOAT_EQ(effect.tintMix(), 0.0f);

    effect.setThreshold(2.0f);
    effect.setRadius(60.0f);
    effect.setIntensity(-1.0f);
    effect.setReaction(4.0f);
    effect.setSaturationWeight(-1.0f);
    effect.setTintMix(2.0f);
    EXPECT_FLOAT_EQ(effect.threshold(), 1.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 48.0f);
    EXPECT_FLOAT_EQ(effect.intensity(), 0.0f);
    EXPECT_FLOAT_EQ(effect.reaction(), 3.0f);
    EXPECT_FLOAT_EQ(effect.saturationWeight(), 0.0f);
    EXPECT_FLOAT_EQ(effect.tintMix(), 1.0f);
}
\n
