#include <gtest/gtest.h>

#include <QString>
#include <QVector>

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

import Audio.Panner;
import Audio.Segment;

using namespace ArtifactCore;

namespace {
AudioSegment makeStereo(QVector<float> left, QVector<float> right)
{
    AudioSegment segment;
    segment.sampleRate = 44100;
    segment.layout = AudioChannelLayout::Stereo;
    segment.startFrame = 321;
    segment.channelData.push_back(std::move(left));
    segment.channelData.push_back(std::move(right));
    return segment;
}
}

TEST(AudioPannerStandaloneTest, DefaultsToStereoBalanceAndNamesEveryMode)
{
    const AudioPanner panner;
    EXPECT_EQ(panner.getMode(), PanningMode::StereoBalance);
    EXPECT_EQ(AudioPanner::modeName(PanningMode::StereoBalance), QStringLiteral("Stereo Balance"));
    EXPECT_EQ(AudioPanner::modeName(PanningMode::EqualPower), QStringLiteral("Equal Power"));
    EXPECT_EQ(AudioPanner::modeName(PanningMode::VBAP), QStringLiteral("VBAP"));
    EXPECT_EQ(AudioPanner::modeName(PanningMode::Ambisonics), QStringLiteral("Ambisonics"));
    EXPECT_EQ(AudioPanner::modeName(PanningMode::Binaural), QStringLiteral("Binaural"));
    EXPECT_EQ(AudioPanner::modeName(static_cast<PanningMode>(999)), QStringLiteral("Unknown"));
}

TEST(AudioPannerStandaloneTest, StereoBalanceMapsAzimuthEndpointsAndCenter)
{
    const AudioPanner panner;
    struct Case { float azimuth; float left; float right; };
    constexpr Case cases[] = {
        {-180.0f, 1.0f, 0.0f},
        {-90.0f, 1.0f, 0.5f},
        {0.0f, 1.0f, 1.0f},
        {90.0f, 0.5f, 1.0f},
        {180.0f, 0.0f, 1.0f},
    };

    for (const auto& testCase : cases) {
        const auto gain = panner.calculateGain(testCase.azimuth);
        ASSERT_EQ(gain.channelGains.size(), 2u);
        EXPECT_FLOAT_EQ(gain.channelGains[0], testCase.left) << testCase.azimuth;
        EXPECT_FLOAT_EQ(gain.channelGains[1], testCase.right) << testCase.azimuth;
    }
}

TEST(AudioPannerStandaloneTest, StereoBalanceClampsFiniteAzimuthAndCentersNonFiniteInput)
{
    const AudioPanner panner;
    const auto left = panner.calculateGain(-900.0f);
    const auto right = panner.calculateGain(900.0f);
    const auto nan = panner.calculateGain(std::numeric_limits<float>::quiet_NaN());
    const auto infinity = panner.calculateGain(std::numeric_limits<float>::infinity());

    EXPECT_EQ(left.channelGains, (std::vector<float>{1.0f, 0.0f}));
    EXPECT_EQ(right.channelGains, (std::vector<float>{0.0f, 1.0f}));
    EXPECT_EQ(nan.channelGains, (std::vector<float>{1.0f, 1.0f}));
    EXPECT_EQ(infinity.channelGains, (std::vector<float>{1.0f, 1.0f}));
}

TEST(AudioPannerStandaloneTest, ConstantPowerGainsStayOnTheUnitPowerCircle)
{
    constexpr float tolerance = 1e-6f;
    for (int step = 0; step <= 40; ++step) {
        const float pan = -1.0f + static_cast<float>(step) / 20.0f;
        const auto gain = AudioPanner::calculateConstantPowerGains(pan);
        ASSERT_EQ(gain.channelGains.size(), 2u);
        EXPECT_NEAR(gain.channelGains[0] * gain.channelGains[0]
                        + gain.channelGains[1] * gain.channelGains[1],
                    1.0f, tolerance) << pan;
    }
}

TEST(AudioPannerStandaloneTest, ConstantPowerFunctionClampsOutOfRangeAndNonFinitePan)
{
    const auto left = AudioPanner::calculateConstantPowerGains(-2.0f);
    const auto right = AudioPanner::calculateConstantPowerGains(2.0f);
    const auto nan = AudioPanner::calculateConstantPowerGains(
        std::numeric_limits<float>::quiet_NaN());

    EXPECT_NEAR(left.channelGains[0], 1.0f, 1e-6f);
    EXPECT_NEAR(left.channelGains[1], 0.0f, 1e-6f);
    EXPECT_NEAR(right.channelGains[0], 0.0f, 1e-6f);
    EXPECT_NEAR(right.channelGains[1], 1.0f, 1e-6f);
    EXPECT_NEAR(nan.channelGains[0], std::sqrt(0.5f), 1e-6f);
    EXPECT_NEAR(nan.channelGains[1], std::sqrt(0.5f), 1e-6f);
}

TEST(AudioPannerStandaloneTest, ApplyPanningTouchesOnlyMappedChannelsAndSharedFrames)
{
    AudioPanner panner;
    auto segment = makeStereo({2.0f, 4.0f, 6.0f}, {3.0f, 5.0f});
    const PanningGain gain{{0.5f}};

    panner.applyPanning(segment, gain);

    EXPECT_EQ(segment.sampleRate, 44100);
    EXPECT_EQ(segment.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(segment.startFrame, 321);
    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, 2.0f, 6.0f}));
    EXPECT_EQ(segment.channelData[1], (QVector<float>{3.0f, 5.0f}));
}

TEST(AudioPannerStandaloneTest, ApplyPanningSanitizesNonFiniteSamplesAndGains)
{
    AudioPanner panner;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeStereo({2.0f, nan}, {infinity, -4.0f});

    panner.applyPanning(segment, PanningGain{{nan, infinity}});

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.0f, 0.0f}));
    EXPECT_EQ(segment.channelData[1], (QVector<float>{0.0f, 0.0f}));
}

TEST(AudioPannerStandaloneTest, ApplyPanningSaturatesFiniteMultiplicationOverflow)
{
    AudioPanner panner;
    const float maximum = std::numeric_limits<float>::max();
    auto segment = makeStereo({maximum}, {-maximum});

    panner.applyPanning(segment, PanningGain{{2.0f, 2.0f}});

    EXPECT_EQ(segment.channelData[0][0], maximum);
    EXPECT_EQ(segment.channelData[1][0], -maximum);
}
