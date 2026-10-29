#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

import ImageProcessing.Duotone;

using namespace ArtifactCore;

namespace {

float expectedMappedChannel(float source,
                            float luminance,
                            float shadow,
                            float highlight,
                            float blend)
{
    const float mapped = shadow * (1.0f - luminance) + highlight * luminance;
    return source * (1.0f - blend) + mapped * blend;
}

}

TEST(DuotoneEffectContractTest, MapsBlackMidGrayAndWhiteBetweenConfiguredEndpoints)
{
    std::array<float4, 3> pixels = {
        float4{0.0f, 0.0f, 0.0f, 0.2f},
        float4{0.5f, 0.5f, 0.5f, 0.4f},
        float4{1.0f, 1.0f, 1.0f, 0.6f},
    };
    DuotoneSettings settings;
    settings.shadowColor = {0.1f, 0.2f, 0.3f, 0.0f};
    settings.highlightColor = {0.9f, 0.8f, 0.7f, 0.0f};
    settings.blend = 1.0f;
    Duotone effect;
    effect.process(pixels.data(), 3, 1, settings);

    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const float luminance = static_cast<float>(index) * 0.5f;
        EXPECT_NEAR(pixels[index].z, expectedMappedChannel(0.5f, luminance,
            0.1f, 0.9f, 1.0f), 1.0e-6f);
        EXPECT_NEAR(pixels[index].y, expectedMappedChannel(0.5f, luminance,
            0.2f, 0.8f, 1.0f), 1.0e-6f);
        EXPECT_NEAR(pixels[index].x, expectedMappedChannel(0.5f, luminance,
            0.3f, 0.7f, 1.0f), 1.0e-6f);
    }
    EXPECT_FLOAT_EQ(pixels[0].z, 0.1f);
    EXPECT_FLOAT_EQ(pixels[2].z, 0.9f);
    EXPECT_FLOAT_EQ(pixels[0].w, 0.2f);
    EXPECT_FLOAT_EQ(pixels[1].w, 0.4f);
    EXPECT_FLOAT_EQ(pixels[2].w, 0.6f);
}

TEST(DuotoneEffectContractTest, UsesRgbLuminanceForColoredPixelsAndBgraStorage)
{
    std::array<float4, 1> pixel = {float4{0.1f, 0.3f, 0.8f, 0.45f}};
    const float luminance = 0.8f * 0.299f + 0.3f * 0.587f + 0.1f * 0.114f;
    DuotoneSettings settings;
    settings.shadowColor = {0.05f, 0.1f, 0.15f, 1.0f};
    settings.highlightColor = {0.95f, 0.8f, 0.65f, 1.0f};
    settings.blend = 0.7f;
    Duotone effect;
    effect.process(pixel.data(), 1, 1, settings);

    EXPECT_NEAR(pixel[0].z, expectedMappedChannel(0.8f, luminance,
        0.05f, 0.95f, 0.7f), 1.0e-6f);
    EXPECT_NEAR(pixel[0].y, expectedMappedChannel(0.3f, luminance,
        0.1f, 0.8f, 0.7f), 1.0e-6f);
    EXPECT_NEAR(pixel[0].x, expectedMappedChannel(0.1f, luminance,
        0.15f, 0.65f, 0.7f), 1.0e-6f);
    EXPECT_FLOAT_EQ(pixel[0].w, 0.45f);
}

TEST(DuotoneEffectContractTest, ZeroBlendIsExactPassthroughAndOutOfRangeBlendClamps)
{
    const std::array<float4, 2> original = {
        float4{0.1f, 0.4f, 0.9f, 0.3f}, float4{0.8f, 0.2f, 0.5f, 0.7f}};
    auto pixels = original;
    DuotoneSettings settings;
    settings.blend = 0.0f;
    Duotone effect;
    effect.process(pixels.data(), 2, 1, settings);
    EXPECT_EQ(pixels, original);

    pixels = original;
    settings.blend = -2.0f;
    effect.process(pixels.data(), 2, 1, settings);
    EXPECT_EQ(pixels, original);

    pixels = original;
    settings.blend = 3.0f;
    effect.process(pixels.data(), 2, 1, settings);
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const float luminance = original[index].z * 0.299f
            + original[index].y * 0.587f + original[index].x * 0.114f;
        EXPECT_NEAR(pixels[index].z, expectedMappedChannel(original[index].z, luminance,
            settings.shadowColor.x, settings.highlightColor.x, 1.0f), 1.0e-6f);
        EXPECT_NEAR(pixels[index].y, expectedMappedChannel(original[index].y, luminance,
            settings.shadowColor.y, settings.highlightColor.y, 1.0f), 1.0e-6f);
        EXPECT_NEAR(pixels[index].x, expectedMappedChannel(original[index].x, luminance,
            settings.shadowColor.z, settings.highlightColor.z, 1.0f), 1.0e-6f);
        EXPECT_FLOAT_EQ(pixels[index].w, original[index].w);
    }
}

TEST(DuotoneEffectContractTest, ClampsOutOfRangeLuminanceForEndpointInterpolation)
{
    std::array<float4, 2> pixels = {
        float4{-0.5f, -0.5f, -0.5f, 0.25f},
        float4{1.5f, 1.5f, 1.5f, 0.75f},
    };
    DuotoneSettings settings;
    settings.shadowColor = {0.2f, 0.3f, 0.4f, 1.0f};
    settings.highlightColor = {0.8f, 0.7f, 0.6f, 1.0f};
    settings.blend = 1.0f;
    Duotone effect;
    effect.process(pixels.data(), 2, 1, settings);

    EXPECT_NEAR(pixels[0].z, 0.2f, 1.0e-6f);
    EXPECT_NEAR(pixels[0].y, 0.3f, 1.0e-6f);
    EXPECT_NEAR(pixels[0].x, 0.4f, 1.0e-6f);
    EXPECT_NEAR(pixels[1].z, 0.8f, 1.0e-6f);
    EXPECT_NEAR(pixels[1].y, 0.7f, 1.0e-6f);
    EXPECT_NEAR(pixels[1].x, 0.6f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixels[0].w, 0.25f);
    EXPECT_FLOAT_EQ(pixels[1].w, 0.75f);
}

TEST(DuotoneEffectContractTest, NullAndEmptyInputAreNoOps)
{
    Duotone effect;
    DuotoneSettings settings;
    effect.process(nullptr, 1, 1, settings);

    float4 pixel{0.1f, 0.2f, 0.3f, 0.4f};
    const float4 original = pixel;
    effect.process(&pixel, 0, 1, settings);
    effect.process(&pixel, 1, 0, settings);
    EXPECT_EQ(pixel, original);
}

