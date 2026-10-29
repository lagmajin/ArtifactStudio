#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

import ImageProcessing.LeaveColor;

using namespace ArtifactCore;

namespace {

float normalizedRgbDistance(const std::array<float, 3>& color,
                           const std::array<float, 3>& key)
{
    const float red = color[0] - key[0];
    const float green = color[1] - key[1];
    const float blue = color[2] - key[2];
    return std::sqrt(red * red + green * green + blue * blue) * 0.57735f;
}

std::array<float, 3> expectedLeaveColor(const std::array<float, 3>& color,
                                        const std::array<float, 3>& key,
                                        float tolerance,
                                        float softness,
                                        float amount)
{
    const float distance = normalizedRgbDistance(color, key);
    const float mix = std::clamp((distance - tolerance) / std::max(softness, 0.001f), 0.0f, 1.0f);
    const float gray = color[0] * 0.299f + color[1] * 0.587f + color[2] * 0.114f;
    return {
        color[0] + (gray - color[0]) * mix * amount,
        color[1] + (gray - color[1]) * mix * amount,
        color[2] + (gray - color[2]) * mix * amount,
    };
}

}

TEST(LeaveColorEffectContractTest, KeyColorIsPreservedAndAlphaIsUntouched)
{
    std::array<float4, 1> pixel = {float4{0.0f, 1.0f, 0.0f, 0.42f}};
    LeaveColorSettings settings;
    settings.keyColor = {0.0f, 1.0f, 0.0f, 1.0f};
    settings.tolerance = 0.0f;
    settings.softness = 0.1f;
    LeaveColor effect;
    effect.process(pixel.data(), 1, 1, settings);

    EXPECT_FLOAT_EQ(pixel[0].x, 0.0f);
    EXPECT_FLOAT_EQ(pixel[0].y, 1.0f);
    EXPECT_FLOAT_EQ(pixel[0].z, 0.0f);
    EXPECT_FLOAT_EQ(pixel[0].w, 0.42f);
}

TEST(LeaveColorEffectContractTest, UsesLogicalRgbKeyAgainstBgraStoredPixel)
{
    constexpr std::array<float, 3> color = {0.85f, 0.3f, 0.1f};
    std::array<float4, 1> pixel = {float4{color[2], color[1], color[0], 0.7f}};
    LeaveColorSettings settings;
    settings.keyColor = {0.8f, 0.2f, 0.1f, 1.0f};
    settings.tolerance = 0.15f;
    settings.softness = 0.25f;
    settings.desaturateAmount = 0.6f;
    LeaveColor effect;
    effect.process(pixel.data(), 1, 1, settings);

    const auto expected = expectedLeaveColor(color, {0.8f, 0.2f, 0.1f},
        settings.tolerance, settings.softness, settings.desaturateAmount);
    EXPECT_NEAR(pixel[0].z, expected[0], 1.0e-6f);
    EXPECT_NEAR(pixel[0].y, expected[1], 1.0e-6f);
    EXPECT_NEAR(pixel[0].x, expected[2], 1.0e-6f);
    EXPECT_FLOAT_EQ(pixel[0].w, 0.7f);
}

TEST(LeaveColorEffectContractTest, DesaturatesColorsOnlyAfterTheKeyTolerance)
{
    constexpr std::array<float, 3> key = {0.2f, 0.7f, 0.1f};
    constexpr std::array<float, 3> color = {0.9f, 0.1f, 0.5f};
    const float distance = normalizedRgbDistance(color, key);
    constexpr std::array<float, 3> mixes = {0.0f, 0.5f, 1.0f};
    constexpr std::array<float, 3> alphas = {0.2f, 0.4f, 0.6f};
    LeaveColor effect;
    const float gray = color[0] * 0.299f + color[1] * 0.587f + color[2] * 0.114f;
    for (std::size_t index = 0; index < mixes.size(); ++index) {
        std::array<float4, 1> pixel = {float4{color[2], color[1], color[0], alphas[index]}};
        LeaveColorSettings settings;
        settings.keyColor = {key[0], key[1], key[2], 1.0f};
        settings.softness = distance;
        settings.tolerance = distance - mixes[index] * settings.softness;
        settings.desaturateAmount = 1.0f;
        effect.process(pixel.data(), 1, 1, settings);

        const auto expected = expectedLeaveColor(color, key, settings.tolerance,
            settings.softness, 1.0f);
        EXPECT_NEAR(pixel[0].z, expected[0], 1.0e-6f);
        EXPECT_NEAR(pixel[0].y, expected[1], 1.0e-6f);
        EXPECT_NEAR(pixel[0].x, expected[2], 1.0e-6f);
        EXPECT_FLOAT_EQ(pixel[0].w, alphas[index]);
        if (index == 2) {
            EXPECT_NEAR(pixel[0].x, gray, 1.0e-6f);
            EXPECT_NEAR(pixel[0].y, gray, 1.0e-6f);
            EXPECT_NEAR(pixel[0].z, gray, 1.0e-6f);
        }
    }
}

TEST(LeaveColorEffectContractTest, ZeroDesaturationAmountIsExactRgbPassthrough)
{
    std::array<float4, 2> pixels = {
        float4{0.1f, 0.7f, 0.3f, 0.15f},
        float4{0.9f, 0.2f, 0.6f, 0.85f},
    };
    const auto original = pixels;
    LeaveColorSettings settings;
    settings.tolerance = 0.0f;
    settings.softness = 0.001f;
    settings.desaturateAmount = 0.0f;
    LeaveColor effect;
    effect.process(pixels.data(), 2, 1, settings);

    EXPECT_FLOAT_EQ(pixels[0].x, original[0].x);
    EXPECT_FLOAT_EQ(pixels[0].y, original[0].y);
    EXPECT_FLOAT_EQ(pixels[0].z, original[0].z);
    EXPECT_FLOAT_EQ(pixels[0].w, original[0].w);
    EXPECT_FLOAT_EQ(pixels[1].x, original[1].x);
    EXPECT_FLOAT_EQ(pixels[1].y, original[1].y);
    EXPECT_FLOAT_EQ(pixels[1].z, original[1].z);
    EXPECT_FLOAT_EQ(pixels[1].w, original[1].w);
}

TEST(LeaveColorEffectContractTest, NullAndEmptyInputAreNoOps)
{
    LeaveColor effect;
    LeaveColorSettings settings;
    effect.process(nullptr, 1, 1, settings);

    float4 pixel{0.2f, 0.4f, 0.6f, 0.8f};
    const float4 original = pixel;
    effect.process(&pixel, 0, 1, settings);
    effect.process(&pixel, 1, 0, settings);
    EXPECT_EQ(pixel, original);
}

