#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

import ImageProcessing.Scatter;

using namespace ArtifactCore;

TEST(ScatterEffectContractTest, SameSeedProducesIdenticalPixelMapping)
{
    std::array<float4, 63> source{};
    for (std::size_t index = 0; index < source.size(); ++index) {
        const float value = static_cast<float>(index);
        source[index] = {value, value + 100.0f, value + 200.0f, value + 300.0f};
    }
    auto first = source;
    auto second = source;
    ScatterSettings settings;
    settings.amount = 7.0f;
    settings.seed = 827;
    Scatter effect;
    effect.process(first.data(), 9, 7, settings);
    effect.process(second.data(), 9, 7, settings);

    EXPECT_EQ(first, second);
}

TEST(ScatterEffectContractTest, OutputPixelsAreSelectedFromTheOriginalImage)
{
    std::array<float4, 35> source{};
    for (std::size_t index = 0; index < source.size(); ++index) {
        const float value = static_cast<float>(index);
        source[index] = {value, -value, value * 2.0f, static_cast<float>(index % 5) / 4.0f};
    }
    auto output = source;
    ScatterSettings settings;
    settings.amount = 1000.0f;
    settings.seed = 19;
    Scatter effect;
    effect.process(output.data(), 7, 5, settings);

    for (const float4& pixel : output) {
        EXPECT_NE(std::find(source.begin(), source.end(), pixel), source.end());
    }
}

TEST(ScatterEffectContractTest, ZeroAmountPreservesEveryPixelExactly)
{
    std::array<float4, 20> pixels{};
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const float value = static_cast<float>(index) / 19.0f;
        pixels[index] = {value, value * 0.3f, 1.0f - value, value * 0.7f};
    }
    const auto original = pixels;
    ScatterSettings settings;
    settings.amount = 0.0f;
    Scatter effect;
    effect.process(pixels.data(), 5, 4, settings);

    EXPECT_EQ(pixels, original);
}

TEST(ScatterEffectContractTest, ZeroAndNegativeDimensionsLeaveBufferUntouched)
{
    const float4 original{0.2f, 0.4f, 0.6f, 0.8f};
    auto pixel = original;
    Scatter effect;
    effect.process(nullptr, 1, 1, {});
    effect.process(&pixel, 0, 1, {});
    effect.process(&pixel, 1, 0, {});
    effect.process(&pixel, -1, 1, {});
    effect.process(&pixel, 1, -1, {});
    EXPECT_EQ(pixel, original);
}

