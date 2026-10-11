#include <gtest/gtest.h>

#include <QVector>

#include <initializer_list>
#include <limits>
#include <utility>

import Audio.Effect.StereoMixer;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeSegment(std::initializer_list<QVector<float>> channels)
{
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = channels.size() == 1
        ? AudioChannelLayout::Mono : AudioChannelLayout::Stereo;
    for (const auto& channel : channels)
        segment.channelData.push_back(channel);
    return segment;
}
}

TEST(AudioStereoMixerStandaloneTest, DefaultsToCenteredBalance)
{
    const AudioStereoMixer mixer;
    EXPECT_EQ(toStdString(mixer.getName()), "Stereo Mixer");
    EXPECT_FLOAT_EQ(mixer.getLeftRight(), 0.0f);
}

TEST(AudioStereoMixerStandaloneTest, CenterBalancePreservesBothChannels)
{
    AudioStereoMixer mixer;
    auto segment = makeSegment({QVector<float>{0.25f, -0.5f},
                                QVector<float>{0.75f, -1.0f}});

    mixer.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_EQ(segment.channelData[1], (QVector<float>{0.75f, -1.0f}));
}

TEST(AudioStereoMixerStandaloneTest, ExtremeBalanceMutesOnlyTheOppositeChannel)
{
    AudioStereoMixer mixer;
    auto leftSegment = makeSegment({QVector<float>{0.5f}, QVector<float>{0.75f}});
    mixer.setLeftRight(-1.0f);
    mixer.process(leftSegment);
    EXPECT_FLOAT_EQ(leftSegment.channelData[0][0], 0.5f);
    EXPECT_FLOAT_EQ(leftSegment.channelData[1][0], 0.0f);

    auto rightSegment = makeSegment({QVector<float>{0.5f}, QVector<float>{0.75f}});
    mixer.setLeftRight(1.0f);
    mixer.process(rightSegment);
    EXPECT_FLOAT_EQ(rightSegment.channelData[0][0], 0.0f);
    EXPECT_FLOAT_EQ(rightSegment.channelData[1][0], 0.75f);
}

TEST(AudioStereoMixerStandaloneTest, IntermediateBalanceAppliesLinearAttenuation)
{
    AudioStereoMixer mixer;
    auto segment = makeSegment({QVector<float>{1.0f}, QVector<float>{1.0f}});
    mixer.setLeftRight(0.25f);

    mixer.process(segment);

    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.75f);
    EXPECT_FLOAT_EQ(segment.channelData[1][0], 1.0f);
}

TEST(AudioStereoMixerStandaloneTest, AdditionalChannelsAndUnequalTailsRemainUntouched)
{
    AudioStereoMixer mixer;
    auto segment = makeSegment({QVector<float>{1.0f, 2.0f, 3.0f},
                                QVector<float>{4.0f, 5.0f},
                                QVector<float>{6.0f, 7.0f, 8.0f}});
    mixer.setLeftRight(0.5f);

    mixer.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.5f, 1.0f, 3.0f}));
    EXPECT_EQ(segment.channelData[1], (QVector<float>{4.0f, 5.0f}));
    EXPECT_EQ(segment.channelData[2], (QVector<float>{6.0f, 7.0f, 8.0f}));
}

TEST(AudioStereoMixerStandaloneTest, MonoInputIsLeftUnchanged)
{
    AudioStereoMixer mixer;
    auto segment = makeSegment({QVector<float>{0.25f, -0.5f}});
    mixer.setLeftRight(1.0f);

    mixer.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioStereoMixerStandaloneTest, BypassPreservesStereoSamples)
{
    AudioStereoMixer mixer;
    mixer.setLeftRight(1.0f);
    mixer.setBypass(true);
    auto segment = makeSegment({QVector<float>{0.25f}, QVector<float>{0.75f}});

    mixer.process(segment);

    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.25f);
    EXPECT_FLOAT_EQ(segment.channelData[1][0], 0.75f);
}

TEST(AudioStereoMixerStandaloneTest, NonFiniteSamplesAreReplacedWithSilence)
{
    AudioStereoMixer mixer;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeSegment({QVector<float>{nan, 0.5f},
                                QVector<float>{infinity, -0.25f}});

    mixer.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.0f, 0.5f}));
    EXPECT_EQ(segment.channelData[1], (QVector<float>{0.0f, -0.25f}));
}

TEST(AudioStereoMixerStandaloneTest, LeftRightSetterClampsAndResetsNonFiniteInput)
{
    AudioStereoMixer mixer;
    mixer.setLeftRight(-5.0f);
    EXPECT_FLOAT_EQ(mixer.getLeftRight(), -1.0f);
    mixer.setLeftRight(5.0f);
    EXPECT_FLOAT_EQ(mixer.getLeftRight(), 1.0f);
    mixer.setLeftRight(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(mixer.getLeftRight(), 0.0f);
}
