#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <type_traits>

import ImageProcessing.PosterizeTime;

using namespace ArtifactCore;

static_assert(!std::is_copy_constructible_v<PosterizeTime>);
static_assert(!std::is_copy_assignable_v<PosterizeTime>);

TEST(PosterizeTimeContractTest, HoldsFramesUntilTheConfiguredThirtyFpsStep)
{
    std::array<float4, 1> pixel = {float4{0.0f, 0.1f, 0.2f, 1.0f}};
    PosterizeTime effect;
    PosterizeTimeSettings settings;
    settings.frameRate = 10.0f;

    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 0.0f);
    pixel[0].x = 0.25f;
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 0.0f);
    pixel[0].x = 0.5f;
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 0.0f);
    pixel[0].x = 0.75f;
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 0.75f);
}

TEST(PosterizeTimeContractTest, RatesAtOrAboveThirtyFpsProcessEveryFrame)
{
    std::array<float4, 1> pixel = {float4{0.0f, 0.0f, 0.0f, 1.0f}};
    PosterizeTime effect;
    PosterizeTimeSettings settings;
    settings.frameRate = 60.0f;
    effect.process(pixel.data(), 1, 1, settings);

    pixel[0].x = 0.4f;
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 0.4f);
}

TEST(PosterizeTimeContractTest, ResetStartsANewSamplingPeriod)
{
    std::array<float4, 1> pixel = {float4{0.1f, 0.0f, 0.0f, 1.0f}};
    PosterizeTime effect;
    PosterizeTimeSettings settings;
    settings.frameRate = 10.0f;
    effect.process(pixel.data(), 1, 1, settings);
    pixel[0].x = 0.2f;
    effect.process(pixel.data(), 1, 1, settings);
    effect.reset();

    pixel[0].x = 0.7f;
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 0.7f);
    pixel[0].x = 0.9f;
    effect.process(pixel.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixel[0].x, 0.7f);
}

TEST(PosterizeTimeContractTest, DimensionChangeSeedsTheNewBufferImmediately)
{
    std::array<float4, 2> pixels = {float4{0.2f}, float4{0.4f}};
    PosterizeTime effect;
    PosterizeTimeSettings settings;
    settings.frameRate = 10.0f;
    effect.process(pixels.data(), 2, 1, settings);

    pixels[0] = float4{0.8f};
    effect.process(pixels.data(), 1, 1, settings);
    EXPECT_FLOAT_EQ(pixels[0].x, 0.8f);
    EXPECT_FLOAT_EQ(pixels[0].y, 0.8f);
    EXPECT_FLOAT_EQ(pixels[0].z, 0.8f);
    EXPECT_FLOAT_EQ(pixels[0].w, 0.8f);
}

TEST(PosterizeTimeContractTest, NonpositiveAndNonfiniteRatesFallBackToEveryFrame)
{
    constexpr std::array<float, 4> rates = {
        0.0f, -1.0f, std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()};
    for (const float rate : rates) {
        std::array<float4, 1> pixel = {float4{0.1f, 0.0f, 0.0f, 1.0f}};
        PosterizeTime effect;
        PosterizeTimeSettings settings;
        settings.frameRate = rate;
        effect.process(pixel.data(), 1, 1, settings);
        pixel[0].x = 0.9f;
        effect.process(pixel.data(), 1, 1, settings);
        EXPECT_FLOAT_EQ(pixel[0].x, 0.9f);
    }
}

TEST(PosterizeTimeContractTest, TinyPositiveRateDoesNotOverflowStepConversion)
{
    std::array<float4, 1> pixel = {float4{0.1f, 0.0f, 0.0f, 1.0f}};
    PosterizeTime effect;
    PosterizeTimeSettings settings;
    settings.frameRate = std::numeric_limits<float>::denorm_min();
    effect.process(pixel.data(), 1, 1, settings);
    pixel[0].x = 0.9f;
    effect.process(pixel.data(), 1, 1, settings);

    EXPECT_FLOAT_EQ(pixel[0].x, 0.1f);
}

TEST(PosterizeTimeContractTest, NullAndEmptyInputAreNoOps)
{
    PosterizeTime effect;
    PosterizeTimeSettings settings;
    effect.process(nullptr, 1, 1, settings);

    float4 pixel{0.1f, 0.2f, 0.3f, 0.4f};
    const float4 original = pixel;
    effect.process(&pixel, 0, 1, settings);
    effect.process(&pixel, 1, 0, settings);
    EXPECT_EQ(pixel, original);
}

