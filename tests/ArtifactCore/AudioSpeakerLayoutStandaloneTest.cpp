#include <gtest/gtest.h>

#include <cmath>

import Audio.Spatial.SpeakerLayout;

using namespace ArtifactCore;
using namespace ArtifactCore::Audio::Spatial;

namespace {
Vec3 directionFromDegrees(float azimuth, float elevation)
{
    constexpr float degreesToRadians = 0.01745329251994329577f;
    const float az = azimuth * degreesToRadians;
    const float el = elevation * degreesToRadians;
    const float horizontal = std::cos(el);
    return {std::sin(az) * horizontal, std::sin(el), -std::cos(az) * horizontal};
}

float gainEnergy(const SpeakerGains& gains)
{
    float energy = 0.0f;
    for (int i = 0; i < gains.channelCount; ++i) {
        energy += gains.values[i] * gains.values[i];
    }
    return energy;
}
}

TEST(AudioSpeakerLayoutStandaloneTest, MonoAndStereoExposeExpectedChannelPositions)
{
    const auto mono = speakerLayout(AudioChannelLayout::Mono);
    ASSERT_EQ(mono.channelCount, 1);
    EXPECT_EQ(mono.speakers[0].channelIndex, 0);
    EXPECT_TRUE(mono.speakers[0].receivesObjectPanning);

    const auto stereo = speakerLayout(AudioChannelLayout::Stereo);
    ASSERT_EQ(stereo.channelCount, 2);
    EXPECT_FLOAT_EQ(stereo.speakers[0].azimuthDegrees, -30.0f);
    EXPECT_FLOAT_EQ(stereo.speakers[1].azimuthDegrees, 30.0f);
}

TEST(AudioSpeakerLayoutStandaloneTest, Surround51MarksLfeAsNonPanning)
{
    const auto layout = speakerLayout(AudioChannelLayout::Surround51);
    ASSERT_EQ(layout.channelCount, 6);
    EXPECT_FALSE(layout.speakers[3].receivesObjectPanning);
    EXPECT_TRUE(layout.speakers[2].receivesObjectPanning);
    EXPECT_FLOAT_EQ(layout.speakers[4].azimuthDegrees, -110.0f);
    EXPECT_FLOAT_EQ(layout.speakers[5].azimuthDegrees, 110.0f);
}

TEST(AudioSpeakerLayoutStandaloneTest, Surround71And714ExposeTheirFullChannelCounts)
{
    const auto layout71 = speakerLayout(AudioChannelLayout::Surround71);
    ASSERT_EQ(layout71.channelCount, 8);
    EXPECT_FALSE(layout71.speakers[3].receivesObjectPanning);
    EXPECT_FLOAT_EQ(layout71.speakers[6].azimuthDegrees, -150.0f);

    const auto layout714 = speakerLayout(AudioChannelLayout::Surround714);
    ASSERT_EQ(layout714.channelCount, 12);
    EXPECT_FLOAT_EQ(layout714.speakers[8].elevationDegrees, 45.0f);
    EXPECT_FLOAT_EQ(layout714.speakers[11].azimuthDegrees, 135.0f);
}

TEST(AudioSpeakerLayoutStandaloneTest, UnsupportedLayoutsUseStereoFallbackDescriptor)
{
    const auto custom = speakerLayout(AudioChannelLayout::Custom10ch);
    const auto ambisonics = speakerLayout(AudioChannelLayout::Ambisonics);
    EXPECT_EQ(custom.channelCount, 2);
    EXPECT_EQ(ambisonics.channelCount, 2);
}

TEST(AudioSpeakerLayoutStandaloneTest, MonoGainIsUnitAndDoesNotUseFallback)
{
    const auto gains = calculateSpeakerGains(AudioChannelLayout::Mono, {1.0f, 0.5f, 0.0f});
    ASSERT_EQ(gains.channelCount, 1);
    EXPECT_FLOAT_EQ(gains.values[0], 1.0f);
    EXPECT_FALSE(gains.usedFallback);
    EXPECT_FLOAT_EQ(gainEnergy(gains), 1.0f);
}

TEST(AudioSpeakerLayoutStandaloneTest, StereoFrontDirectionHasBalancedNormalizedGains)
{
    const auto gains = calculateSpeakerGains(AudioChannelLayout::Stereo, {0.0f, 0.0f, -1.0f});
    ASSERT_EQ(gains.channelCount, 2);
    EXPECT_NEAR(gains.values[0], gains.values[1], 1e-6f);
    EXPECT_NEAR(gainEnergy(gains), 1.0f, 1e-6f);
    EXPECT_TRUE(gains.usedFallback);
}

TEST(AudioSpeakerLayoutStandaloneTest, StereoSideDirectionsFavorTheMatchingSpeaker)
{
    const auto left = calculateSpeakerGains(AudioChannelLayout::Stereo,
        directionFromDegrees(-30.0f, 0.0f));
    const auto right = calculateSpeakerGains(AudioChannelLayout::Stereo,
        directionFromDegrees(30.0f, 0.0f));
    EXPECT_GT(left.values[0], left.values[1]);
    EXPECT_GT(right.values[1], right.values[0]);
    EXPECT_NEAR(gainEnergy(left), 1.0f, 1e-6f);
    EXPECT_NEAR(gainEnergy(right), 1.0f, 1e-6f);
}

TEST(AudioSpeakerLayoutStandaloneTest, LfeNeverReceivesObjectPanningGain)
{
    for (const auto layout : {AudioChannelLayout::Surround51,
                              AudioChannelLayout::Surround71,
                              AudioChannelLayout::Surround714}) {
        const auto gains = calculateSpeakerGains(layout, {0.0f, 0.0f, -1.0f});
        ASSERT_GT(gains.channelCount, 3);
        EXPECT_FLOAT_EQ(gains.values[3], 0.0f);
    }
}

TEST(AudioSpeakerLayoutStandaloneTest, GainsRemainFiniteAndUnitEnergyForSurroundLayouts)
{
    for (const auto layout : {AudioChannelLayout::Surround51,
                              AudioChannelLayout::Surround71,
                              AudioChannelLayout::Surround714}) {
        const auto gains = calculateSpeakerGains(layout, {0.4f, 0.6f, -0.7f});
        EXPECT_NEAR(gainEnergy(gains), 1.0f, 1e-5f);
        for (int i = 0; i < gains.channelCount; ++i) {
            EXPECT_TRUE(std::isfinite(gains.values[i]));
            EXPECT_GE(gains.values[i], 0.0f);
        }
    }
}

TEST(AudioSpeakerLayoutStandaloneTest, ZeroDirectionUsesTheFrontDirectionConvention)
{
    const auto zero = calculateSpeakerGains(AudioChannelLayout::Stereo, {0.0f, 0.0f, 0.0f});
    const auto front = calculateSpeakerGains(AudioChannelLayout::Stereo, {0.0f, 0.0f, -1.0f});
    EXPECT_FLOAT_EQ(zero.values[0], front.values[0]);
    EXPECT_FLOAT_EQ(zero.values[1], front.values[1]);
}

TEST(AudioSpeakerLayoutStandaloneTest, UpperHemisphereDirectionProducesNormalized714Gains)
{
    const auto gains = calculateSpeakerGains(AudioChannelLayout::Surround714,
        directionFromDegrees(45.0f, 45.0f));
    ASSERT_EQ(gains.channelCount, 12);
    EXPECT_NEAR(gainEnergy(gains), 1.0f, 1e-5f);
    EXPECT_GT(gains.values[9], 0.0f);
    EXPECT_FLOAT_EQ(gains.values[3], 0.0f);
}

TEST(AudioSpeakerLayoutStandaloneTest, OppositeDirectionFallbackStillReturnsUnitGainEnergy)
{
    const auto gains = calculateSpeakerGains(AudioChannelLayout::Stereo,
        {0.0f, 0.0f, 1.0f});
    EXPECT_TRUE(gains.usedFallback);
    EXPECT_NEAR(gainEnergy(gains), 1.0f, 1e-6f);
    EXPECT_FLOAT_EQ(gains.values[0], 1.0f);
    EXPECT_FLOAT_EQ(gains.values[1], 0.0f);
}
