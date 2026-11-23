#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.DirectionalGlow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 25;
constexpr int kHeight = 25;

ImageF32x4RGBAWithCache makeBrightImpulse()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    constexpr int center = (kHeight / 2 * kWidth + kWidth / 2) * 4;
    pixels[center] = 1.0f;
    pixels[center + 1] = 1.0f;
    pixels[center + 2] = 1.0f;
    pixels[center + 3] = 1.0f;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(DirectionalGlowEffectContractTest, HorizontalPatternExtendsBrightPointAsStreak)
{
    const auto source = makeBrightImpulse();
    ImageF32x4RGBAWithCache output;
    DirectionalGlowEffect effect;
    effect.setThreshold(0.1f);
    effect.setIntensity(2.0f);
    effect.setLength1(8.0f);
    effect.setLength2(12.0f);
    effect.setWeight1(0.7f);
    effect.setWeight2(0.3f);
    effect.setPattern(StreakPattern::Horizontal);

    effect.applyCPUOnly(source, output);

    EXPECT_GT(output.image().getPixel(15, 12).r(),
              source.image().getPixel(15, 12).r());
    EXPECT_GT(output.image().getPixel(9, 12).r(),
              source.image().getPixel(9, 12).r());
    EXPECT_NEAR(source.image().getPixel(15, 12).r(), 0.0f, 1.0e-6f);
}

TEST(DirectionalGlowEffectContractTest, ZeroIntensityPreservesEveryInputChannel)
{
    const auto source = makeBrightImpulse();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ImageF32x4RGBAWithCache output;
    DirectionalGlowEffect effect;
    effect.setIntensity(0.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_NEAR(outputPixels[channel], sourcePixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}

TEST(DirectionalGlowEffectContractTest, NonFiniteSettingsResetToDefaults)
{
    DirectionalGlowEffect effect;
    effect.setThreshold(std::numeric_limits<float>::quiet_NaN());
    effect.setIntensity(std::numeric_limits<float>::infinity());
    effect.setLength1(std::numeric_limits<float>::quiet_NaN());
    effect.setLength2(std::numeric_limits<float>::infinity());
    effect.setLength3(std::numeric_limits<float>::quiet_NaN());
    effect.setWeight1(std::numeric_limits<float>::quiet_NaN());
    effect.setWeight2(std::numeric_limits<float>::infinity());
    effect.setWeight3(std::numeric_limits<float>::quiet_NaN());
    effect.setAngleOffset(std::numeric_limits<float>::quiet_NaN());

    EXPECT_FLOAT_EQ(effect.threshold(), 0.8f);
    EXPECT_FLOAT_EQ(effect.intensity(), 1.0f);
    EXPECT_FLOAT_EQ(effect.length1(), 64.0f);
    EXPECT_FLOAT_EQ(effect.length2(), 128.0f);
    EXPECT_FLOAT_EQ(effect.length3(), 256.0f);
    EXPECT_FLOAT_EQ(effect.weight1(), 0.6f);
    EXPECT_FLOAT_EQ(effect.weight2(), 0.4f);
    EXPECT_FLOAT_EQ(effect.weight3(), 0.15f);
    EXPECT_FLOAT_EQ(effect.angleOffset(), 0.0f);
}

TEST(DirectionalGlowEffectContractTest,
     ThirdStageExtendsStreakBeyondSecondStage)
{
    const auto source = makeBrightImpulse();
    const auto sampleThirdStage = [&source](float length3) {
        ImageF32x4RGBAWithCache output;
        DirectionalGlowEffect effect;
        effect.setThreshold(0.1f);
        effect.setIntensity(1.0f);
        effect.setLength1(4.0f);
        effect.setLength2(4.0f);
        effect.setLength3(length3);
        effect.setWeight1(0.0f);
        effect.setWeight2(0.0f);
        effect.setWeight3(1.0f);
        effect.setPattern(StreakPattern::Horizontal);
        effect.applyCPUOnly(source, output);
        return output.image().getPixel(16, 12).r();
    };

    EXPECT_GT(sampleThirdStage(20.0f), sampleThirdStage(4.0f));
}

TEST(DirectionalGlowEffectContractTest, HalfSamplingStillProducesStreak)
{
    const auto source = makeBrightImpulse();
    ImageF32x4RGBAWithCache output;
    DirectionalGlowEffect effect;
    effect.setThreshold(0.1f);
    effect.setIntensity(2.0f);
    effect.setLength1(8.0f);
    effect.setLength2(12.0f);
    effect.setWeight1(0.7f);
    effect.setWeight2(0.3f);
    effect.setPattern(StreakPattern::Horizontal);
    effect.setHalfSampling(true);

    effect.applyCPUOnly(source, output);

    EXPECT_TRUE(std::isfinite(output.image().getPixel(15, 12).r()));
    EXPECT_GT(output.image().getPixel(15, 12).r(),
              source.image().getPixel(15, 12).r());
}

TEST(DirectionalGlowEffectContractTest, Star6PresetAppliesThreeStageLook)
{
    DirectionalGlowEffect effect;
    effect.applyPreset(2);

    EXPECT_EQ(effect.preset(), 2);
    EXPECT_EQ(effect.pattern(), StreakPattern::Star6);
    EXPECT_FLOAT_EQ(effect.length1(), 48.0f);
    EXPECT_FLOAT_EQ(effect.length2(), 112.0f);
    EXPECT_FLOAT_EQ(effect.length3(), 208.0f);
    EXPECT_FLOAT_EQ(effect.weight3(), 0.16f);
}

TEST(DirectionalGlowEffectContractTest, CustomAnglesSwitchPatternAndIgnoreNonFinite)
{
    DirectionalGlowEffect effect;
    effect.setPattern(StreakPattern::Horizontal);
    effect.setCustomAngles({30.0f, std::numeric_limits<float>::quiet_NaN(),
                            120.0f});

    EXPECT_EQ(effect.pattern(), StreakPattern::Custom);
    ASSERT_EQ(effect.customAngles().size(), 2);
    EXPECT_FLOAT_EQ(effect.customAngles()[0], 30.0f);
    EXPECT_FLOAT_EQ(effect.customAngles()[1], 120.0f);
}

TEST(DirectionalGlowEffectContractTest, CustomAnglesDoNotExposeGpuNode)
{
    DirectionalGlowEffect effect;
    effect.setCustomAngles({30.0f, 120.0f});

    GpuSpatialEffectStack stack;
    EXPECT_FALSE(effect.appendGpuSpatialNodes(stack));
    EXPECT_EQ(stack.count, 0u);
}
