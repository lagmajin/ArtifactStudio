#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

import ImageProcessing.SimpleChoker;

using namespace ArtifactCore;

namespace {

std::array<float, 9> referenceAlpha(const std::array<float, 9>& source,
                                   float choke,
                                   int radius)
{
    std::array<float, 9> result{};
    const int effectiveRadius = std::max(1, radius);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            float minimum = 1.0f;
            float maximum = 0.0f;
            for (int dy = -effectiveRadius; dy <= effectiveRadius; ++dy) {
                for (int dx = -effectiveRadius; dx <= effectiveRadius; ++dx) {
                    const int sampleX = std::clamp(x + dx, 0, 2);
                    const int sampleY = std::clamp(y + dy, 0, 2);
                    const float alpha = source[static_cast<std::size_t>(sampleY * 3 + sampleX)];
                    minimum = std::min(minimum, alpha);
                    maximum = std::max(maximum, alpha);
                }
            }
            const std::size_t index = static_cast<std::size_t>(y * 3 + x);
            const float blend = std::abs(choke);
            const float extreme = choke >= 0.0f ? minimum : maximum;
            result[index] = source[index] * (1.0f - blend) + extreme * blend;
        }
    }
    return result;
}

std::array<float4, 9> makePixels(const std::array<float, 9>& alpha)
{
    std::array<float4, 9> pixels{};
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const float value = static_cast<float>(index) / 10.0f;
        pixels[index] = {value, value + 0.1f, value + 0.2f, alpha[index]};
    }
    return pixels;
}

}

TEST(SimpleChokerContractTest, PositiveChokeMovesAlphaTowardLocalMinimum)
{
    constexpr std::array<float, 9> source = {
        0.0f, 0.8f, 0.2f,
        0.6f, 1.0f, 0.4f,
        0.3f, 0.7f, 0.5f,
    };
    auto pixels = makePixels(source);
    const auto expected = referenceAlpha(source, 0.5f, 1);
    SimpleChokerSettings settings;
    settings.choke = 0.5f;
    settings.radius = 1;
    SimpleChoker effect;
    effect.process(pixels.data(), 3, 3, settings);

    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_FLOAT_EQ(pixels[index].w, expected[index]);
    }
}

TEST(SimpleChokerContractTest, NegativeChokeMovesAlphaTowardLocalMaximum)
{
    constexpr std::array<float, 9> source = {
        0.0f, 0.8f, 0.2f,
        0.6f, 1.0f, 0.4f,
        0.3f, 0.7f, 0.5f,
    };
    auto pixels = makePixels(source);
    const auto expected = referenceAlpha(source, -0.25f, 1);
    SimpleChokerSettings settings;
    settings.choke = -0.25f;
    settings.radius = 1;
    SimpleChoker effect;
    effect.process(pixels.data(), 3, 3, settings);

    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_FLOAT_EQ(pixels[index].w, expected[index]);
    }
}

TEST(SimpleChokerContractTest, ZeroStrengthPreservesAlphaAndEveryColorChannel)
{
    constexpr std::array<float, 9> source = {
        0.0f, 0.8f, 0.2f,
        0.6f, 1.0f, 0.4f,
        0.3f, 0.7f, 0.5f,
    };
    auto pixels = makePixels(source);
    const auto original = pixels;
    SimpleChokerSettings settings;
    settings.choke = 0.0f;
    settings.radius = 1;
    SimpleChoker effect;
    effect.process(pixels.data(), 3, 3, settings);

    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_FLOAT_EQ(pixels[index].x, original[index].x);
        EXPECT_FLOAT_EQ(pixels[index].y, original[index].y);
        EXPECT_FLOAT_EQ(pixels[index].z, original[index].z);
        EXPECT_FLOAT_EQ(pixels[index].w, original[index].w);
    }
}

TEST(SimpleChokerContractTest, RadiusBelowOneUsesOnePixelNeighborhoodAndClampsEdges)
{
    constexpr std::array<float, 9> source = {
        0.1f, 0.9f, 0.4f,
        0.7f, 0.8f, 0.2f,
        0.6f, 0.3f, 1.0f,
    };
    auto pixels = makePixels(source);
    const auto expected = referenceAlpha(source, 1.0f, 0);
    SimpleChokerSettings settings;
    settings.choke = 1.0f;
    settings.radius = 0;
    SimpleChoker effect;
    effect.process(pixels.data(), 3, 3, settings);

    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_FLOAT_EQ(pixels[index].w, expected[index]);
    }
    EXPECT_FLOAT_EQ(pixels.front().w, 0.1f);
    EXPECT_FLOAT_EQ(pixels.back().w, 0.2f);
}

TEST(SimpleChokerContractTest, FractionalStrengthExtrapolationAndLargeRadiusMatchReference)
{
    constexpr std::array<float, 9> source = {
        0.1f, 0.9f, 0.4f,
        0.7f, 0.8f, 0.2f,
        0.6f, 0.3f, 1.0f,
    };
    auto pixels = makePixels(source);
    const auto expected = referenceAlpha(source, 1.25f, 4);
    SimpleChokerSettings settings;
    settings.choke = 1.25f;
    settings.radius = 4;
    SimpleChoker effect;
    effect.process(pixels.data(), 3, 3, settings);

    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_FLOAT_EQ(pixels[index].w, expected[index]);
    }
}

TEST(SimpleChokerContractTest, NullAndInvalidDimensionsLeaveBufferUntouched)
{
    float4 pixel{0.2f, 0.4f, 0.6f, 0.8f};
    const float4 original = pixel;
    SimpleChoker effect;
    SimpleChokerSettings settings;
    effect.process(nullptr, 1, 1, settings);
    effect.process(&pixel, 0, 1, settings);
    effect.process(&pixel, 1, 0, settings);
    effect.process(&pixel, -1, 1, settings);
    effect.process(&pixel, 1, -1, settings);
    EXPECT_EQ(pixel, original);
}

