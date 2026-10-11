#include <gtest/gtest.h>

#include <limits>

import Audio.Spatial.Params;

using namespace ArtifactCore::Audio::Spatial;

namespace {
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
}

TEST(AudioSpatialParamsStandaloneTest, SanitizesMinimumAndMaximumDistance)
{
    SpatialParams params;
    params.minDistance = kNaN;
    params.maxDistance = -1.0f;
    auto result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.minDistance, 0.001f);
    EXPECT_FLOAT_EQ(result.maxDistance, 0.002f);

    params.minDistance = 4.0f;
    params.maxDistance = 3.0f;
    result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.minDistance, 4.0f);
    EXPECT_FLOAT_EQ(result.maxDistance, 4.001f);
}

TEST(AudioSpatialParamsStandaloneTest, ClampsRolloffAndSpread)
{
    SpatialParams params;
    params.rolloff = 20.0f;
    params.spread = 2.0f;
    auto result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.rolloff, 10.0f);
    EXPECT_FLOAT_EQ(result.spread, 1.0f);

    params.rolloff = kNaN;
    params.spread = -1.0f;
    result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.rolloff, 0.0f);
    EXPECT_FLOAT_EQ(result.spread, 0.0f);
}

TEST(AudioSpatialParamsStandaloneTest, ClampsStereoWidthAndUsesFiniteFallback)
{
    SpatialParams params;
    params.stereoWidthDegrees = kNaN;
    EXPECT_FLOAT_EQ(sanitizedSpatialParams(params).stereoWidthDegrees, 30.0f);

    params.stereoWidthDegrees = 180.0f;
    EXPECT_FLOAT_EQ(sanitizedSpatialParams(params).stereoWidthDegrees, 120.0f);
    params.stereoWidthDegrees = -5.0f;
    EXPECT_FLOAT_EQ(sanitizedSpatialParams(params).stereoWidthDegrees, 0.0f);
}

TEST(AudioSpatialParamsStandaloneTest, OrdersAndBoundsConeAngles)
{
    SpatialParams params;
    params.coneInnerAngle = 400.0f;
    params.coneOuterAngle = -2.0f;
    auto result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.coneInnerAngle, 360.0f);
    EXPECT_FLOAT_EQ(result.coneOuterAngle, 360.0f);

    params.coneInnerAngle = 90.0f;
    params.coneOuterAngle = 30.0f;
    result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.coneInnerAngle, 90.0f);
    EXPECT_FLOAT_EQ(result.coneOuterAngle, 90.0f);
}

TEST(AudioSpatialParamsStandaloneTest, ReplacesNonFiniteConeAnglesWithDefaults)
{
    SpatialParams params;
    params.coneInnerAngle = kNaN;
    params.coneOuterAngle = std::numeric_limits<float>::infinity();
    const auto result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.coneInnerAngle, 360.0f);
    EXPECT_FLOAT_EQ(result.coneOuterAngle, 360.0f);
}

TEST(AudioSpatialParamsStandaloneTest, ClampsConeOuterGain)
{
    SpatialParams params;
    params.coneOuterGain = 2.0f;
    EXPECT_FLOAT_EQ(sanitizedSpatialParams(params).coneOuterGain, 1.0f);
    params.coneOuterGain = -1.0f;
    EXPECT_FLOAT_EQ(sanitizedSpatialParams(params).coneOuterGain, 0.0f);
    params.coneOuterGain = kNaN;
    EXPECT_FLOAT_EQ(sanitizedSpatialParams(params).coneOuterGain, 0.0f);
}

TEST(AudioSpatialParamsStandaloneTest, SanitizesDopplerAndAirAbsorption)
{
    SpatialParams params;
    params.dopplerFactor = -1.0f;
    params.airAbsorption = kNaN;
    auto result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.dopplerFactor, 0.0f);
    EXPECT_FLOAT_EQ(result.airAbsorption, 0.0f);

    params.dopplerFactor = 2.5f;
    params.airAbsorption = 0.25f;
    result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.dopplerFactor, 2.5f);
    EXPECT_FLOAT_EQ(result.airAbsorption, 0.25f);
}

TEST(AudioSpatialParamsStandaloneTest, ClampsRoomSendAndTailDuration)
{
    SpatialParams params;
    params.roomSend = 1.5f;
    params.roomTailSeconds = 3.0f;
    auto result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.roomSend, 1.0f);
    EXPECT_FLOAT_EQ(result.roomTailSeconds, 2.0f);

    params.roomSend = -0.5f;
    params.roomTailSeconds = 0.01f;
    result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.roomSend, 0.0f);
    EXPECT_FLOAT_EQ(result.roomTailSeconds, 0.4f);
}

TEST(AudioSpatialParamsStandaloneTest, ReplacesNonFiniteRoomValues)
{
    SpatialParams params;
    params.roomSend = kNaN;
    params.roomTailSeconds = std::numeric_limits<float>::infinity();
    const auto result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.roomSend, 0.0f);
    EXPECT_FLOAT_EQ(result.roomTailSeconds, 0.4f);
}

TEST(AudioSpatialParamsStandaloneTest, ClampsLfeSendAndCutoff)
{
    SpatialParams params;
    params.lfeSend = 2.0f;
    params.lfeCutoffHz = 300.0f;
    auto result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.lfeSend, 1.0f);
    EXPECT_FLOAT_EQ(result.lfeCutoffHz, 250.0f);

    params.lfeSend = -1.0f;
    params.lfeCutoffHz = 20.0f;
    result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.lfeSend, 0.0f);
    EXPECT_FLOAT_EQ(result.lfeCutoffHz, 40.0f);
}

TEST(AudioSpatialParamsStandaloneTest, ReplacesNonFiniteLfeValues)
{
    SpatialParams params;
    params.lfeSend = kNaN;
    params.lfeCutoffHz = kNaN;
    const auto result = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(result.lfeSend, 0.0f);
    EXPECT_FLOAT_EQ(result.lfeCutoffHz, 120.0f);
}

TEST(AudioSpatialParamsStandaloneTest, ReplacesUnsupportedEnumsWithSafeDefaults)
{
    SpatialParams params;
    params.outputLayout = static_cast<AudioChannelLayout>(255);
    params.renderMode = static_cast<SpatialRenderMode>(255);
    const auto result = sanitizedSpatialParams(params);
    EXPECT_EQ(result.outputLayout, AudioChannelLayout::Stereo);
    EXPECT_EQ(result.renderMode, SpatialRenderMode::Speaker);
}

TEST(AudioSpatialParamsStandaloneTest, PreservesSupportedLayoutAndRenderMode)
{
    SpatialParams params;
    params.outputLayout = AudioChannelLayout::Surround714;
    params.renderMode = SpatialRenderMode::Headphone;
    const auto result = sanitizedSpatialParams(params);
    EXPECT_EQ(result.outputLayout, AudioChannelLayout::Surround714);
    EXPECT_EQ(result.renderMode, SpatialRenderMode::Headphone);
}
