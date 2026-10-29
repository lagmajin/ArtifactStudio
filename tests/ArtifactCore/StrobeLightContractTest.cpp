#include <gtest/gtest.h>

#include <array>
#include <limits>

import ImageProcessing.StrobeLight;

using namespace ArtifactCore;

TEST(StrobeLightContractTest, AppliesLogicalRgbaColorToBgraStorageAndPreservesBlend)
{
    std::array<float4, 1> pixel = {float4{0.1f, 0.2f, 0.3f, 0.4f}};
    StrobeLightSettings settings;
    settings.frequency = 30.0f;
    settings.duration = 1.0f;
    settings.color = {0.8f, 0.6f, 0.4f, 0.2f};
    settings.blendWithOriginal = 0.0f;
    StrobeLight effect;
    effect.process(pixel.data(), 1, 1, settings);

    EXPECT_FLOAT_EQ(pixel[0].x, 0.4f);
    EXPECT_FLOAT_EQ(pixel[0].y, 0.6f);
    EXPECT_FLOAT_EQ(pixel[0].z, 0.8f);
    EXPECT_FLOAT_EQ(pixel[0].w, 0.2f);
}

TEST(StrobeLightContractTest, BlendsWithOriginalAtConfiguredAmount)
{
    std::array<float4, 1> pixel = {float4{0.2f, 0.4f, 0.6f, 0.8f}};
    StrobeLightSettings settings;
    settings.frequency = 30.0f;
    settings.duration = 1.0f;
    settings.color = {1.0f, 0.0f, 0.5f, 0.25f};
    settings.blendWithOriginal = 0.5f;
    StrobeLight effect;
    effect.process(pixel.data(), 1, 1, settings);

    EXPECT_FLOAT_EQ(pixel[0].x, 0.35f);
    EXPECT_FLOAT_EQ(pixel[0].y, 0.2f);
    EXPECT_FLOAT_EQ(pixel[0].z, 0.8f);
    EXPECT_FLOAT_EQ(pixel[0].w, 0.525f);
}

TEST(StrobeLightContractTest, FrequencyAndDurationDefineRepeatableOnOffFrames)
{
    StrobeLightSettings settings;
    settings.frequency = 10.0f;
    settings.duration = 0.34f;
    settings.color = {1.0f, 1.0f, 1.0f, 1.0f};
    settings.blendWithOriginal = 0.0f;
    StrobeLight effect;
    const std::array<float, 4> input = {0.1f, 0.2f, 0.3f, 0.4f};
    const std::array<float, 4> expected = {1.0f, 0.2f, 0.3f, 1.0f};

    for (std::size_t frame = 0; frame < input.size(); ++frame) {
        std::array<float4, 1> pixel = {float4{input[frame]}};
        effect.process(pixel.data(), 1, 1, settings);
        EXPECT_FLOAT_EQ(pixel[0].x, expected[frame]) << "frame " << frame;
    }
}

TEST(StrobeLightContractTest, ZeroDurationNeverFlashesAndFullDurationAlwaysFlashes)
{
    StrobeLight effect;
    StrobeLightSettings settings;
    settings.frequency = 10.0f;
    settings.duration = 0.0f;
    settings.color = float4{1.0f};
    std::array<float4, 1> pixel = {float4{0.25f}};
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 0.25f);

    settings.duration = 2.0f;
    for (int frame = 0; frame < 4; ++frame) {
        pixel[0] = float4{0.25f};
        effect.process(pixel.data(), 1, 1, settings);
        EXPECT_FLOAT_EQ(pixel[0].x, 1.0f);
    }
}

TEST(StrobeLightContractTest, ResetRestartsTheFirstOnFrame)
{
    StrobeLightSettings settings;
    settings.frequency = 10.0f;
    settings.duration = 0.34f;
    settings.color = float4{1.0f};
    settings.blendWithOriginal = 0.0f;
    StrobeLight effect;
    std::array<float4, 1> pixel = {float4{0.25f}};
    effect.process(pixel.data(), 1, 1, settings);
    pixel[0] = float4{0.25f};
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 0.25f);

    effect.reset();
    pixel[0] = float4{0.25f};
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 1.0f);
}

TEST(StrobeLightContractTest, InvalidFrequencyFallsBackToEveryFrame)
{
    constexpr std::array<float, 4> frequencies = {
        0.0f, -1.0f, std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()};
    for (const float frequency : frequencies) {
        StrobeLight effect;
        StrobeLightSettings settings;
        settings.frequency = frequency;
        settings.duration = 1.0f;
        settings.color = float4{1.0f};
        settings.blendWithOriginal = 0.0f;
        std::array<float4, 1> pixel = {float4{0.25f}};
        effect.process(pixel.data(), 1, 1, settings);
        EXPECT_FLOAT_EQ(pixel[0].x, 1.0f);
    }
}

TEST(StrobeLightContractTest, TinyPositiveFrequencySaturatesPeriodWithoutOverflow)
{
    StrobeLightSettings settings;
    settings.frequency = std::numeric_limits<float>::denorm_min();
    settings.duration = 1.0f;
    settings.color = float4{1.0f};
    settings.blendWithOriginal = 0.0f;
    StrobeLight effect;
    std::array<float4, 1> pixel = {float4{0.25f}};
    effect.process(pixel.data(), 1, 1, settings);

    EXPECT_FLOAT_EQ(pixel[0].x, 1.0f);
}

TEST(StrobeLightContractTest, ClampsBlendAndNonfiniteBlendToSafeRange)
{
    StrobeLightSettings settings;
    settings.frequency = 30.0f;
    settings.duration = 1.0f;
    settings.color = float4{1.0f};
    settings.blendWithOriginal = 2.0f;
    StrobeLight effect;
    std::array<float4, 1> pixel = {float4{0.25f}};
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 0.25f);

    effect.reset();
    settings.blendWithOriginal = std::numeric_limits<float>::quiet_NaN();
    pixel[0] = float4{0.25f};
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 1.0f);
}

TEST(StrobeLightContractTest, NullAndEmptyInputDoNotAdvanceTheStrobe)
{
    StrobeLightSettings settings;
    settings.frequency = 10.0f;
    settings.duration = 0.34f;
    settings.color = float4{1.0f};
    settings.blendWithOriginal = 0.0f;
    StrobeLight effect;
    effect.process(nullptr, 1, 1, settings);

    float4 pixel{0.25f};
    effect.process(&pixel, 0, 1, settings);
    effect.process(&pixel, 1, 0, settings);
    effect.process(&pixel, 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel.x, 1.0f);
}

