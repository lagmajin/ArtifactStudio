#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.Strobe;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

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

TEST(StrobeEffectContractTest, AlternatesBetweenCapturedReferenceAndCurrentFrameByTime)
{
    const auto reference = makeUniformFrame(0.2f, 0.3f);
    const auto current = makeUniformFrame(0.8f, 0.7f);
    StrobeEffect effect;
    effect.setFrequency(4.0f);
    effect.setMixAmount(1.0f);
    ImageF32x4RGBAWithCache output;

    EffectContext context;
    context.timeSeconds = 0.0;
    effect.setContext(context);
    effect.applyCPUOnly(reference, output);

    context.timeSeconds = 0.125;
    effect.setContext(context);
    effect.applyCPUOnly(current, output);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.7f, 1.0e-6f);

    context.timeSeconds = 0.0625;
    effect.setContext(context);
    effect.applyCPUOnly(current, output);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.3f, 1.0e-6f);
}

TEST(StrobeEffectContractTest, MixAmountInterpolatesReferenceRgbaDuringOnPhase)
{
    const auto reference = makeUniformFrame(0.2f, 0.3f);
    const auto current = makeUniformFrame(0.8f, 0.7f);
    StrobeEffect effect;
    effect.setFrequency(4.0f);
    effect.setMixAmount(0.5f);
    ImageF32x4RGBAWithCache output;

    EffectContext context;
    context.timeSeconds = 0.0;
    effect.setContext(context);
    effect.applyCPUOnly(reference, output);
    effect.applyCPUOnly(current, output);

    const auto mixed = output.image().getPixel(1, 1);
    EXPECT_NEAR(mixed.r(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(mixed.g(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(mixed.b(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(mixed.a(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(reference.image().getPixel(1, 1).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(current.image().getPixel(1, 1).r(), 0.8f, 1.0e-6f);
}

TEST(StrobeEffectContractTest, ReturnsToReferenceAtExactCycleBoundary)
{
    const auto reference = makeUniformFrame(0.2f, 0.3f);
    const auto current = makeUniformFrame(0.8f, 0.7f);
    StrobeEffect effect;
    effect.setFrequency(4.0f);
    effect.setMixAmount(1.0f);
    ImageF32x4RGBAWithCache output;

    EffectContext context;
    context.timeSeconds = 0.0;
    effect.setContext(context);
    effect.applyCPUOnly(reference, output);
    context.timeSeconds = 0.25;
    effect.setContext(context);
    effect.applyCPUOnly(current, output);

    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.3f, 1.0e-6f);
}

TEST(StrobeEffectContractTest, SwitchesToCurrentFrameAtExactHalfCycleBoundary)
{
    const auto reference = makeUniformFrame(0.2f, 0.3f);
    const auto current = makeUniformFrame(0.8f, 0.7f);
    StrobeEffect effect;
    effect.setFrequency(4.0f);
    effect.setMixAmount(1.0f);
    ImageF32x4RGBAWithCache output;

    EffectContext context;
    context.timeSeconds = 0.0;
    effect.setContext(context);
    effect.applyCPUOnly(reference, output);
    context.timeSeconds = 0.125;
    effect.setContext(context);
    effect.applyCPUOnly(current, output);

    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.7f, 1.0e-6f);
}

TEST(StrobeEffectContractTest, FrequencyHasMinimumAndMixAmountClampsToUnitRange)
{
    StrobeEffect effect;
    effect.setFrequency(0.0f);
    effect.setMixAmount(-0.5f);
    EXPECT_FLOAT_EQ(effect.frequency(), 0.1f);
    EXPECT_FLOAT_EQ(effect.mixAmount(), 0.0f);

    effect.setFrequency(12.0f);
    effect.setMixAmount(1.5f);
    EXPECT_FLOAT_EQ(effect.frequency(), 12.0f);
    EXPECT_FLOAT_EQ(effect.mixAmount(), 1.0f);
}
\n
\n
