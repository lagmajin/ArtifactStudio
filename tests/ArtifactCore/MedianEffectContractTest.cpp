#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

import ImageProcessing.Median;

using namespace ArtifactCore;

namespace {

std::array<float, 4> referenceMedian(const std::array<float4, 9>& source,
                                    int width,
                                    int height,
                                    int x,
                                    int y)
{
    std::array<std::array<float, 9>, 4> channels{};
    std::size_t sample = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int sx = std::clamp(x + dx, 0, width - 1);
            const int sy = std::clamp(y + dy, 0, height - 1);
            const float4& pixel = source[static_cast<std::size_t>(sy * width + sx)];
            channels[0][sample] = pixel.x;
            channels[1][sample] = pixel.y;
            channels[2][sample] = pixel.z;
            channels[3][sample] = pixel.w;
            ++sample;
        }
    }

    std::array<float, 4> result{};
    for (std::size_t channel = 0; channel < channels.size(); ++channel) {
        std::sort(channels[channel].begin(), channels[channel].end());
        result[channel] = channels[channel][4];
    }
    return result;
}

}

TEST(MedianEffectContractTest, RemovesSinglePixelOutlierPerChannel)
{
    std::array<float4, 9> pixels{};
    pixels.fill(float4{0.2f, 0.4f, 0.6f, 0.8f});
    pixels[4] = {1.0f, 0.0f, 1.0f, 0.0f};
    MedianSettings settings;
    settings.radius = 1;
    Median effect;
    effect.process(pixels.data(), 3, 3, settings);

    EXPECT_EQ(pixels[4], (float4{0.2f, 0.4f, 0.6f, 0.8f}));
}

TEST(MedianEffectContractTest, UsesReplicatedEdgesAndIndependentChannelMedians)
{
    std::array<float4, 9> pixels = {
        float4{0.0f, 0.9f, 0.2f, 0.1f}, float4{0.8f, 0.1f, 0.4f, 0.9f}, float4{0.3f, 0.7f, 0.8f, 0.5f},
        float4{0.4f, 0.3f, 0.6f, 0.2f}, float4{0.9f, 0.5f, 0.1f, 0.7f}, float4{0.2f, 0.8f, 0.9f, 0.4f},
        float4{0.6f, 0.2f, 0.7f, 0.8f}, float4{0.1f, 0.6f, 0.3f, 0.3f}, float4{0.7f, 0.4f, 0.5f, 0.6f},
    };
    const auto original = pixels;
    MedianSettings settings;
    settings.radius = 1;
    Median effect;
    effect.process(pixels.data(), 3, 3, settings);

    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            const auto expected = referenceMedian(original, 3, 3, x, y);
            const float4& actual = pixels[static_cast<std::size_t>(y * 3 + x)];
            EXPECT_FLOAT_EQ(actual.x, expected[0]);
            EXPECT_FLOAT_EQ(actual.y, expected[1]);
            EXPECT_FLOAT_EQ(actual.z, expected[2]);
            EXPECT_FLOAT_EQ(actual.w, expected[3]);
        }
    }
}

TEST(MedianEffectContractTest, RadiusBelowOneUsesTheThreeByThreeNeighborhood)
{
    std::array<float4, 9> pixels = {
        float4{0.0f}, float4{0.1f}, float4{0.2f},
        float4{0.3f}, float4{0.4f}, float4{0.5f},
        float4{0.6f}, float4{0.7f}, float4{1.0f},
    };
    MedianSettings settings;
    settings.radius = 0;
    Median effect;
    effect.process(pixels.data(), 3, 3, settings);

    EXPECT_FLOAT_EQ(pixels[4].x, 0.4f);
    EXPECT_FLOAT_EQ(pixels[4].w, 0.4f);
}

TEST(MedianEffectContractTest, MatchesReferenceAcrossParallelTileBoundaries)
{
    constexpr int width = 35;
    constexpr int height = 34;
    std::array<float4, width * height> pixels{};
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const float base = static_cast<float>((index * 29) % 97) / 96.0f;
        pixels[index] = {base, 1.0f - base, base * 0.5f,
                         static_cast<float>((index * 7) % 19) / 18.0f};
    }
    const auto original = pixels;
    MedianSettings settings;
    settings.radius = 1;
    Median effect;
    effect.process(pixels.data(), width, height, settings);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::array<std::array<float, 9>, 4> samples{};
            std::size_t sample = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int sx = std::clamp(x + dx, 0, width - 1);
                    const int sy = std::clamp(y + dy, 0, height - 1);
                    const float4& pixel = original[static_cast<std::size_t>(sy * width + sx)];
                    samples[0][sample] = pixel.x;
                    samples[1][sample] = pixel.y;
                    samples[2][sample] = pixel.z;
                    samples[3][sample] = pixel.w;
                    ++sample;
                }
            }
            const float4& actual = pixels[static_cast<std::size_t>(y * width + x)];
            for (std::size_t channel = 0; channel < samples.size(); ++channel) {
                std::sort(samples[channel].begin(), samples[channel].end());
                const float actualChannels[] = {actual.x, actual.y, actual.z, actual.w};
                EXPECT_FLOAT_EQ(actualChannels[channel], samples[channel][4])
                    << "pixel (" << x << ", " << y << "), channel " << channel;
            }
        }
    }
}

TEST(MedianEffectContractTest, NullAndInvalidDimensionsAreNoOps)
{
    Median effect;
    MedianSettings settings;
    effect.process(nullptr, 1, 1, settings);

    float4 pixel{0.2f, 0.4f, 0.6f, 0.8f};
    const float4 original = pixel;
    effect.process(&pixel, 0, 1, settings);
    effect.process(&pixel, 1, 0, settings);
    EXPECT_EQ(pixel, original);
}

