#include <gtest/gtest.h>

#include <array>

import ImageProcessing.MotionTrail;

using namespace ArtifactCore;

TEST(MotionTrailContractTest, FirstFrameSeedsHistoryWithoutChangingInput)
{
    std::array<float4, 2> pixels = {
        float4{0.8f, 0.4f, 0.2f, 0.6f},
        float4{0.1f, 0.3f, 0.7f, 0.9f},
    };
    const auto original = pixels;
    MotionTrail trail;
    trail.process(pixels.data(), 2, 1, {});

    EXPECT_EQ(pixels, original);
}

TEST(MotionTrailContractTest, BlendInterpolatesOnceBetweenCurrentAndDecayedHistory)
{
    std::array<float4, 1> pixel = {float4{0.8f, 0.6f, 0.4f, 0.2f}};
    MotionTrail trail;
    trail.process(pixel.data(), 1, 1, {});

    pixel[0] = {0.2f, 0.4f, 0.6f, 0.8f};
    MotionTrailSettings settings;
    settings.decay = 0.5f;
    settings.intensity = 0.5f;
    settings.mode = MotionTrailMode::Blend;
    trail.process(pixel.data(), 1, 1, settings);

    EXPECT_FLOAT_EQ(pixel[0].x, 0.3f);
    EXPECT_FLOAT_EQ(pixel[0].y, 0.35f);
    EXPECT_FLOAT_EQ(pixel[0].z, 0.4f);
    EXPECT_FLOAT_EQ(pixel[0].w, 0.45f);
}

TEST(MotionTrailContractTest, AdditiveModeClampsEachChannelAndAlpha)
{
    std::array<float4, 1> pixel = {float4{0.8f, 0.2f, 0.4f, 0.6f}};
    MotionTrail trail;
    trail.process(pixel.data(), 1, 1, {});

    pixel[0] = {0.4f, 0.3f, 0.2f, 0.1f};
    MotionTrailSettings settings;
    settings.decay = 0.8f;
    settings.intensity = 1.0f;
    settings.mode = MotionTrailMode::Additive;
    trail.process(pixel.data(), 1, 1, settings);

    EXPECT_FLOAT_EQ(pixel[0].x, 1.0f);
    EXPECT_FLOAT_EQ(pixel[0].y, 0.46f);
    EXPECT_FLOAT_EQ(pixel[0].z, 0.52f);
    EXPECT_FLOAT_EQ(pixel[0].w, 0.58f);
}

TEST(MotionTrailContractTest, MaximumModeComparesCurrentWithScaledHistory)
{
    std::array<float4, 1> pixel = {float4{0.8f, 0.2f, 0.4f, 0.6f}};
    MotionTrail trail;
    trail.process(pixel.data(), 1, 1, {});

    pixel[0] = {0.4f, 0.3f, 0.5f, 0.1f};
    MotionTrailSettings settings;
    settings.decay = 0.5f;
    settings.intensity = 0.5f;
    settings.mode = MotionTrailMode::Maximum;
    trail.process(pixel.data(), 1, 1, settings);

    EXPECT_FLOAT_EQ(pixel[0].x, 0.4f);
    EXPECT_FLOAT_EQ(pixel[0].y, 0.3f);
    EXPECT_FLOAT_EQ(pixel[0].z, 0.5f);
    EXPECT_FLOAT_EQ(pixel[0].w, 0.15f);
}

TEST(MotionTrailContractTest, ResetSeedsHistoryFromTheNextFrame)
{
    std::array<float4, 1> pixel = {float4{0.8f, 0.6f, 0.4f, 0.2f}};
    MotionTrail trail;
    trail.process(pixel.data(), 1, 1, {});
    trail.reset();

    pixel[0] = {0.2f, 0.4f, 0.6f, 0.8f};
    trail.process(pixel.data(), 1, 1, {});
    EXPECT_FLOAT_EQ(pixel[0].x, 0.2f);
    EXPECT_FLOAT_EQ(pixel[0].y, 0.4f);
    EXPECT_FLOAT_EQ(pixel[0].z, 0.6f);
    EXPECT_FLOAT_EQ(pixel[0].w, 0.8f);
}

TEST(MotionTrailContractTest, DimensionChangeSeedsNewHistoryWithoutBlendingOldSize)
{
    std::array<float4, 2> pixels = {
        float4{0.8f, 0.6f, 0.4f, 0.2f},
        float4{0.7f, 0.5f, 0.3f, 0.1f},
    };
    MotionTrail trail;
    trail.process(pixels.data(), 2, 1, {});

    pixels[0] = {0.1f, 0.2f, 0.3f, 0.4f};
    MotionTrailSettings settings;
    settings.decay = 1.0f;
    settings.intensity = 1.0f;
    settings.mode = MotionTrailMode::Additive;
    trail.process(pixels.data(), 1, 1, settings);

    EXPECT_FLOAT_EQ(pixels[0].x, 0.1f);
    EXPECT_FLOAT_EQ(pixels[0].y, 0.2f);
    EXPECT_FLOAT_EQ(pixels[0].z, 0.3f);
    EXPECT_FLOAT_EQ(pixels[0].w, 0.4f);
}

TEST(MotionTrailContractTest, DecayAndIntensityAreClampedToUnitInterval)
{
    std::array<float4, 1> pixel = {float4{0.8f, 0.6f, 0.4f, 0.2f}};
    MotionTrail trail;
    trail.process(pixel.data(), 1, 1, {});

    pixel[0] = {0.2f, 0.4f, 0.6f, 0.8f};
    MotionTrailSettings settings;
    settings.decay = 2.0f;
    settings.intensity = -1.0f;
    settings.mode = MotionTrailMode::Blend;
    trail.process(pixel.data(), 1, 1, settings);

    EXPECT_FLOAT_EQ(pixel[0].x, 0.2f);
    EXPECT_FLOAT_EQ(pixel[0].y, 0.4f);
    EXPECT_FLOAT_EQ(pixel[0].z, 0.6f);
    EXPECT_FLOAT_EQ(pixel[0].w, 0.8f);
}

TEST(MotionTrailContractTest, InvalidDimensionsAndNullInputDoNotChangeBuffer)
{
    std::array<float4, 1> pixel = {float4{0.1f, 0.2f, 0.3f, 0.4f}};
    const auto original = pixel;
    MotionTrail trail;
    trail.process(nullptr, 1, 1, {});
    trail.process(pixel.data(), 0, 1, {});
    trail.process(pixel.data(), 1, 0, {});
    EXPECT_EQ(pixel, original);
}

