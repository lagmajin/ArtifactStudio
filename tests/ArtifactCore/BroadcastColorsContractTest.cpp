#include <gtest/gtest.h>

#include <algorithm>
#include <array>

import ImageProcessing.BroadcastColors;

using namespace ArtifactCore;

namespace {

float luma(const float4& pixel)
{
    return pixel.z * 0.299f + pixel.y * 0.587f + pixel.x * 0.114f;
}

}

TEST(BroadcastColorsContractTest, KeepsNeutralGraysWithinBroadcastLumaRange)
{
    std::array<float4, 5> pixels = {
        float4{0.0f, 0.0f, 0.0f, 0.1f},
        float4{0.1f, 0.1f, 0.1f, 0.2f},
        float4{0.5f, 0.5f, 0.5f, 0.3f},
        float4{0.9f, 0.9f, 0.9f, 0.4f},
        float4{1.0f, 1.0f, 1.0f, 0.5f},
    };
    BroadcastColors effect;
    effect.process(pixels.data(), static_cast<int>(pixels.size()), 1, {});

    constexpr float minimum = 16.0f / 255.0f;
    constexpr float maximum = 235.0f / 255.0f;
    constexpr std::array<float, 5> inputLuma = {0.0f, 0.1f, 0.5f, 0.9f, 1.0f};
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_NEAR(pixels[index].x, pixels[index].y, 1.0e-6f);
        EXPECT_NEAR(pixels[index].y, pixels[index].z, 1.0e-6f);
        EXPECT_NEAR(pixels[index].y, std::clamp(
            inputLuma[index],
            minimum, maximum), 1.0e-6f);
    }
}

TEST(BroadcastColorsContractTest, PreservesAlphaAndUsesBgraChannelOrder)
{
    std::array<float4, 1> pixel = {float4{0.12f, 0.42f, 0.83f, 0.37f}};
    const float originalLuma = luma(pixel[0]);
    BroadcastColors effect;
    effect.process(pixel.data(), 1, 1, {});

    EXPECT_FLOAT_EQ(pixel[0].w, 0.37f);
    EXPECT_NEAR(luma(pixel[0]), originalLuma, 1.0e-6f);
    EXPECT_GT(pixel[0].z, pixel[0].x);
}

TEST(BroadcastColorsContractTest, ZeroSaturationReductionProducesNeutralLuma)
{
    std::array<float4, 1> pixel = {float4{0.1f, 0.6f, 0.9f, 0.8f}};
    const float expectedLuma = std::clamp(luma(pixel[0]), 16.0f / 255.0f, 235.0f / 255.0f);
    BroadcastColorsSettings settings;
    settings.reduceSaturation = 1.0f;
    BroadcastColors effect;
    effect.process(pixel.data(), 1, 1, settings);

    EXPECT_NEAR(pixel[0].x, expectedLuma, 1.0e-6f);
    EXPECT_NEAR(pixel[0].y, expectedLuma, 1.0e-6f);
    EXPECT_NEAR(pixel[0].z, expectedLuma, 1.0e-6f);
}

TEST(BroadcastColorsContractTest, ProcessesEveryPixelAcrossTileBoundaries)
{
    constexpr int width = 67;
    constexpr int height = 35;
    std::array<float4, width * height> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float value = static_cast<float>((x * 17 + y * 31) % 101) / 100.0f;
            pixels[static_cast<std::size_t>(y * width + x)] =
                float4{value * 0.4f, value * 0.7f, value, static_cast<float>(x + y) / 200.0f};
        }
    }
    const auto original = pixels;
    BroadcastColors effect;
    effect.process(pixels.data(), width, height, {});

    for (std::size_t index = 0; index < pixels.size(); ++index) {
        EXPECT_GE(pixels[index].x, 0.0f);
        EXPECT_LE(pixels[index].x, 1.0f);
        EXPECT_GE(pixels[index].y, 0.0f);
        EXPECT_LE(pixels[index].y, 1.0f);
        EXPECT_GE(pixels[index].z, 0.0f);
        EXPECT_LE(pixels[index].z, 1.0f);
        EXPECT_FLOAT_EQ(pixels[index].w, original[index].w);
    }
}

TEST(BroadcastColorsContractTest, EmptyDimensionsDoNotTouchBuffer)
{
    const float4 sentinel{0.2f, 0.4f, 0.6f, 0.8f};
    auto pixel = sentinel;
    BroadcastColors effect;
    effect.process(&pixel, 0, 1, {});
    effect.process(&pixel, 1, 0, {});
    effect.process(&pixel, -1, 1, {});
    EXPECT_EQ(pixel, sentinel);
}

