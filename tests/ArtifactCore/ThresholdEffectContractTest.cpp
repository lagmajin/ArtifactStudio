#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

import ImageProcessing.Threshold;

using namespace ArtifactCore;

namespace {

float expectedThreshold(float luma, float threshold, float softness)
{
    const float effectiveSoftness = std::max(0.001f, softness);
    return std::clamp((luma - std::clamp(threshold, 0.0f, 1.0f)
        + effectiveSoftness * 0.5f) / effectiveSoftness, 0.0f, 1.0f);
}

float4 grayscale(float value, float alpha)
{
    return {value, value, value, alpha};
}

}

TEST(ThresholdEffectContractTest, HardThresholdSeparatesValuesAroundEquality)
{
    std::array<float4, 3> pixels = {
        grayscale(0.499f, 0.1f), grayscale(0.5f, 0.2f), grayscale(0.501f, 0.3f)};
    ThresholdSettings settings;
    settings.threshold = 0.5f;
    settings.softness = 0.0f;
    Threshold effect;
    effect.process(pixels.data(), static_cast<int>(pixels.size()), 1, settings);

    EXPECT_FLOAT_EQ(pixels[0].x, 0.0f);
    EXPECT_FLOAT_EQ(pixels[1].x, 0.5f);
    EXPECT_FLOAT_EQ(pixels[2].x, 1.0f);
    for (const float4& pixel : pixels) {
        EXPECT_FLOAT_EQ(pixel.x, pixel.y);
        EXPECT_FLOAT_EQ(pixel.y, pixel.z);
    }
}

TEST(ThresholdEffectContractTest, SoftnessHasClampedBlackAndWhiteEndpoints)
{
    std::array<float4, 5> pixels = {
        grayscale(0.0f, 0.11f), grayscale(0.3f, 0.22f), grayscale(0.5f, 0.33f),
        grayscale(0.7f, 0.44f), grayscale(1.0f, 0.55f)};
    ThresholdSettings settings;
    settings.threshold = 0.5f;
    settings.softness = 0.4f;
    Threshold effect;
    effect.process(pixels.data(), static_cast<int>(pixels.size()), 1, settings);

    constexpr std::array<float, 5> expected = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_NEAR(pixels[index].x, expected[index], 1.0e-6f);
        EXPECT_NEAR(pixels[index].y, expected[index], 1.0e-6f);
        EXPECT_NEAR(pixels[index].z, expected[index], 1.0e-6f);
    }
}

TEST(ThresholdEffectContractTest, UsesIndependentRgbLumaAndPreservesAlpha)
{
    std::array<float4, 1> pixels = {float4{0.2f, 0.4f, 0.8f, 0.37f}};
    const float sourceLuma = 0.8f * 0.299f + 0.4f * 0.587f + 0.2f * 0.114f;
    ThresholdSettings settings;
    settings.threshold = 0.5f;
    settings.softness = 0.25f;
    Threshold effect;
    effect.process(pixels.data(), 1, 1, settings);

    const float expected = expectedThreshold(sourceLuma, settings.threshold, settings.softness);
    EXPECT_NEAR(pixels[0].x, expected, 1.0e-6f);
    EXPECT_NEAR(pixels[0].y, expected, 1.0e-6f);
    EXPECT_NEAR(pixels[0].z, expected, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixels[0].w, 0.37f);
}

TEST(ThresholdEffectContractTest, ClampsThresholdAtBothEnds)
{
    std::array<float4, 2> pixels = {grayscale(0.4f, 0.2f), grayscale(0.6f, 0.3f)};
    ThresholdSettings settings;
    settings.threshold = -4.0f;
    settings.softness = 0.0f;
    Threshold effect;
    effect.process(pixels.data(), 2, 1, settings);
    EXPECT_FLOAT_EQ(pixels[0].x, 1.0f);
    EXPECT_FLOAT_EQ(pixels[1].x, 1.0f);

    pixels = {grayscale(0.4f, 0.2f), grayscale(0.6f, 0.3f)};
    settings.threshold = 4.0f;
    effect.process(pixels.data(), 2, 1, settings);
    EXPECT_FLOAT_EQ(pixels[0].x, 0.0f);
    EXPECT_FLOAT_EQ(pixels[1].x, 0.0f);
}

TEST(ThresholdEffectContractTest, ProcessesNonSquareImagesAcrossParallelRowPartition)
{
    constexpr int width = 37;
    constexpr int height = 71;
    std::array<float4, width * height> pixels{};
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const float value = static_cast<float>((index * 37) % 103) / 102.0f;
        pixels[index] = grayscale(value, static_cast<float>(index % 17) / 16.0f);
    }
    const auto original = pixels;
    ThresholdSettings settings;
    settings.threshold = 0.37f;
    settings.softness = 0.18f;
    Threshold effect;
    effect.process(pixels.data(), width, height, settings);

    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const float expected = expectedThreshold(original[index].x,
            settings.threshold, settings.softness);
        EXPECT_NEAR(pixels[index].x, expected, 1.0e-6f);
        EXPECT_NEAR(pixels[index].y, expected, 1.0e-6f);
        EXPECT_NEAR(pixels[index].z, expected, 1.0e-6f);
        EXPECT_FLOAT_EQ(pixels[index].w, original[index].w);
    }
}

TEST(ThresholdEffectContractTest, NullOrEmptyInputIsSafe)
{
    Threshold effect;
    ThresholdSettings settings;
    effect.process(nullptr, 1, 1, settings);

    float4 sentinel = grayscale(0.25f, 0.75f);
    const float4 original = sentinel;
    effect.process(&sentinel, 0, 1, settings);
    effect.process(&sentinel, 1, 0, settings);
    EXPECT_EQ(sentinel, original);
}

