#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

import ImageProcessing.Emboss;

using namespace ArtifactCore;

namespace {

float luma(const float4& pixel)
{
    return pixel.z * 0.299f + pixel.y * 0.587f + pixel.x * 0.114f;
}

std::array<float4, 9> referenceEmboss(const std::array<float4, 9>& input,
                                     float intensity,
                                     float angle)
{
    constexpr float horizontal[3][3] = {
        {-1.0f, 0.0f, 1.0f}, {-2.0f, 0.0f, 2.0f}, {-1.0f, 0.0f, 1.0f}};
    constexpr float vertical[3][3] = {
        {1.0f, 2.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {-1.0f, -2.0f, -1.0f}};
    std::array<float4, 9> output = input;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            float gx = 0.0f;
            float gy = 0.0f;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int sampleX = std::clamp(x + dx, 0, 2);
                    const int sampleY = std::clamp(y + dy, 0, 2);
                    const float value = luma(input[static_cast<std::size_t>(sampleY * 3 + sampleX)]);
                    gx += value * horizontal[dy + 1][dx + 1];
                    gy += value * vertical[dy + 1][dx + 1];
                }
            }
            const float edge = gx * std::cos(angle) + gy * std::sin(angle);
            const std::size_t index = static_cast<std::size_t>(y * 3 + x);
            output[index].x = std::clamp(input[index].x + edge * intensity, 0.0f, 1.0f);
            output[index].y = std::clamp(input[index].y + edge * intensity, 0.0f, 1.0f);
            output[index].z = std::clamp(input[index].z + edge * intensity, 0.0f, 1.0f);
        }
    }
    return output;
}

}

TEST(EmbossEffectContractTest, FlatImageHasNoDirectionalEdgeAndPreservesAlpha)
{
    std::array<float4, 9> pixels{};
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        pixels[index] = float4{0.35f, 0.35f, 0.35f, static_cast<float>(index) / 8.0f};
    }
    const auto original = pixels;
    EmbossSettings settings;
    settings.intensity = 4.0f;
    settings.angle = 1.2f;
    Emboss effect;
    effect.process(pixels.data(), 3, 3, settings);

    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_FLOAT_EQ(pixels[index].x, original[index].x);
        EXPECT_FLOAT_EQ(pixels[index].y, original[index].y);
        EXPECT_FLOAT_EQ(pixels[index].z, original[index].z);
        EXPECT_FLOAT_EQ(pixels[index].w, original[index].w);
    }
}

TEST(EmbossEffectContractTest, HorizontalAndVerticalSobelResponsesFollowAngle)
{
    std::array<float4, 9> horizontalEdge = {
        float4{0.0f}, float4{0.0f}, float4{1.0f},
        float4{0.0f}, float4{0.0f}, float4{1.0f},
        float4{0.0f}, float4{0.0f}, float4{1.0f},
    };
    EmbossSettings settings;
    settings.intensity = 0.1f;
    settings.angle = 0.0f;
    Emboss effect;
    effect.process(horizontalEdge.data(), 3, 3, settings);
    EXPECT_NEAR(horizontalEdge[4].x, 0.4f, 1.0e-6f);

    std::array<float4, 9> verticalEdge = {
        float4{1.0f}, float4{1.0f}, float4{1.0f},
        float4{0.0f}, float4{0.0f}, float4{0.0f},
        float4{0.0f}, float4{0.0f}, float4{0.0f},
    };
    settings.angle = 1.57079632679f;
    effect.process(verticalEdge.data(), 3, 3, settings);
    EXPECT_NEAR(verticalEdge[4].x, 0.4f, 1.0e-5f);
}

TEST(EmbossEffectContractTest, FullConvolutionMatchesIndependentReferenceAtClampedEdges)
{
    std::array<float4, 9> pixels = {
        float4{0.05f, 0.2f, 0.6f, 0.1f}, float4{0.3f, 0.1f, 0.4f, 0.2f}, float4{0.8f, 0.5f, 0.2f, 0.3f},
        float4{0.1f, 0.7f, 0.4f, 0.4f}, float4{0.4f, 0.3f, 0.8f, 0.5f}, float4{0.9f, 0.2f, 0.1f, 0.6f},
        float4{0.2f, 0.4f, 0.1f, 0.7f}, float4{0.5f, 0.8f, 0.3f, 0.8f}, float4{0.7f, 0.1f, 0.9f, 0.9f},
    };
    const auto expected = referenceEmboss(pixels, 0.35f, 0.62f);
    EmbossSettings settings;
    settings.intensity = 0.35f;
    settings.angle = 0.62f;
    Emboss effect;
    effect.process(pixels.data(), 3, 3, settings);

    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_NEAR(pixels[index].x, expected[index].x, 1.0e-6f);
        EXPECT_NEAR(pixels[index].y, expected[index].y, 1.0e-6f);
        EXPECT_NEAR(pixels[index].z, expected[index].z, 1.0e-6f);
        EXPECT_FLOAT_EQ(pixels[index].w, expected[index].w);
    }
}

TEST(EmbossEffectContractTest, SaturatesColorChannelsButNotAlpha)
{
    std::array<float4, 9> pixels{};
    for (auto& pixel : pixels) {
        pixel = float4{0.0f, 0.0f, 0.0f, 0.63f};
    }
    pixels[2] = float4{1.0f, 1.0f, 1.0f, 0.63f};
    EmbossSettings settings;
    settings.intensity = 100.0f;
    settings.angle = 0.0f;
    Emboss effect;
    effect.process(pixels.data(), 3, 3, settings);

    for (const float4& pixel : pixels) {
        EXPECT_GE(pixel.x, 0.0f);
        EXPECT_LE(pixel.x, 1.0f);
        EXPECT_GE(pixel.y, 0.0f);
        EXPECT_LE(pixel.y, 1.0f);
        EXPECT_GE(pixel.z, 0.0f);
        EXPECT_LE(pixel.z, 1.0f);
        EXPECT_FLOAT_EQ(pixel.w, 0.63f);
    }
}

TEST(EmbossEffectContractTest, NullAndInvalidDimensionsAreNoOps)
{
    Emboss effect;
    EmbossSettings settings;
    effect.process(nullptr, 1, 1, settings);

    float4 pixel{0.2f, 0.4f, 0.6f, 0.8f};
    const float4 original = pixel;
    effect.process(&pixel, 0, 1, settings);
    effect.process(&pixel, 1, 0, settings);
    effect.process(&pixel, -1, 1, settings);
    effect.process(&pixel, 1, -1, settings);
    EXPECT_EQ(pixel, original);
}

