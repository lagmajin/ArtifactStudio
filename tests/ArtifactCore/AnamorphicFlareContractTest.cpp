#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

import ImageProcessing.AnamorphicFlare;

using namespace ArtifactCore;

namespace {

float luminance(const float4& pixel)
{
    return pixel.z * 0.299f + pixel.y * 0.587f + pixel.x * 0.114f;
}

float highlight(float channel, float sourceLuma, float threshold)
{
    if (sourceLuma <= threshold) {
        return 0.0f;
    }
    const float scale = (sourceLuma - threshold) / (1.0f - threshold + 0.001f);
    return channel * scale;
}

}

TEST(AnamorphicFlareContractTest, SingleHighlightCreatesSymmetricHorizontalDecay)
{
    std::array<float4, 5> pixels = {
        float4{0.0f, 0.0f, 0.0f, 0.1f},
        float4{0.0f, 0.0f, 0.0f, 0.2f},
        float4{0.05f, 0.1f, 0.8f, 0.7f},
        float4{0.0f, 0.0f, 0.0f, 0.4f},
        float4{0.0f, 0.0f, 0.0f, 0.5f},
    };
    const auto original = pixels;
    AnamorphicFlareSettings settings;
    settings.threshold = 0.2f;
    settings.flareLength = 0.4f;
    settings.intensity = 0.5f;
    settings.tint = {0.6f, 0.3f, 0.8f, 0.0f};
    AnamorphicFlare effect;
    effect.process(pixels.data(), 5, 1, settings);

    const float sourceLuma = luminance(original[2]);
    const float decay = settings.flareLength * 0.95f + 0.04f;
    const float red = highlight(original[2].z, sourceLuma, settings.threshold);
    const float green = highlight(original[2].y, sourceLuma, settings.threshold);
    const float blue = highlight(original[2].x, sourceLuma, settings.threshold);
    for (int x = 0; x < 5; ++x) {
        const int distance = std::abs(x - 2);
        const float falloff = std::pow(decay, static_cast<float>(distance));
        const std::size_t index = static_cast<std::size_t>(x);
        EXPECT_NEAR(pixels[index].z, std::clamp(original[index].z
            + red * falloff * settings.tint.x * settings.intensity, 0.0f, 1.0f), 1.0e-6f);
        EXPECT_NEAR(pixels[index].y, std::clamp(original[index].y
            + green * falloff * settings.tint.y * settings.intensity, 0.0f, 1.0f), 1.0e-6f);
        EXPECT_NEAR(pixels[index].x, std::clamp(original[index].x
            + blue * falloff * settings.tint.z * settings.intensity, 0.0f, 1.0f), 1.0e-6f);
        EXPECT_FLOAT_EQ(pixels[index].w, original[index].w);
    }
}

TEST(AnamorphicFlareContractTest, ThresholdEqualityDoesNotGenerateAFlare)
{
    constexpr float value = 1.0f;
    std::array<float4, 3> pixels = {
        float4{value, value, value, 0.2f},
        float4{value, value, value, 0.5f},
        float4{value, value, value, 0.8f},
    };
    const auto original = pixels;
    AnamorphicFlareSettings settings;
    settings.threshold = value;
    settings.intensity = 5.0f;
    AnamorphicFlare effect;
    effect.process(pixels.data(), 3, 1, settings);

    EXPECT_EQ(pixels, original);
}

TEST(AnamorphicFlareContractTest, HighlightPropagationNeverCrossesScanlines)
{
    std::array<float4, 15> pixels{};
    pixels[7] = {0.0f, 0.0f, 0.9f, 0.6f};
    const auto original = pixels;
    AnamorphicFlareSettings settings;
    settings.threshold = 0.1f;
    settings.flareLength = 0.8f;
    settings.intensity = 1.0f;
    settings.tint = {0.0f, 0.0f, 1.0f, 1.0f};
    AnamorphicFlare effect;
    effect.process(pixels.data(), 5, 3, settings);

    for (int y = 0; y < 3; ++y) {
        if (y == 1) {
            continue;
        }
        for (int x = 0; x < 5; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * 5 + x);
            EXPECT_FLOAT_EQ(pixels[index].x, original[index].x);
            EXPECT_FLOAT_EQ(pixels[index].y, original[index].y);
            EXPECT_FLOAT_EQ(pixels[index].z, original[index].z);
            EXPECT_FLOAT_EQ(pixels[index].w, original[index].w);
        }
    }
}

TEST(AnamorphicFlareContractTest, ZeroIntensityIsExactPassthrough)
{
    std::array<float4, 3> pixels = {
        float4{0.1f, 0.2f, 0.3f, 0.4f},
        float4{0.9f, 0.8f, 0.7f, 0.6f},
        float4{0.5f, 0.4f, 0.3f, 0.2f},
    };
    const auto original = pixels;
    AnamorphicFlareSettings settings;
    settings.threshold = 0.0f;
    settings.intensity = 0.0f;
    AnamorphicFlare effect;
    effect.process(pixels.data(), 3, 1, settings);
    EXPECT_EQ(pixels, original);
}

TEST(AnamorphicFlareContractTest, NullAndInvalidDimensionsAreNoOps)
{
    AnamorphicFlare effect;
    AnamorphicFlareSettings settings;
    effect.process(nullptr, 1, 1, settings);

    float4 pixel{0.2f, 0.4f, 0.6f, 0.8f};
    const float4 original = pixel;
    effect.process(&pixel, 0, 1, settings);
    effect.process(&pixel, 1, 0, settings);
    effect.process(&pixel, -1, 1, settings);
    EXPECT_EQ(pixel, original);
}

