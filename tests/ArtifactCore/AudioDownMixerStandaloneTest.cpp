#include <gtest/gtest.h>

#include <QVector>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

import Audio.DownMixer;
import Audio.Segment;

using namespace ArtifactCore;

namespace {
AudioSegment makeSegment(AudioChannelLayout layout,
                         std::initializer_list<std::initializer_list<float>> channels,
                         int sampleRate = 48000,
                         qint64 startFrame = 0)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = layout;
    segment.startFrame = startFrame;
    for (const auto samples : channels) {
        QVector<float> channel;
        for (const float sample : samples)
            channel.push_back(sample);
        segment.channelData.push_back(std::move(channel));
    }
    return segment;
}
}

TEST(AudioDownMixerStandaloneTest, DefaultsToStereoAndInvalidTargetFallsBackToStereo)
{
    AudioDownMixer mixer;
    EXPECT_EQ(mixer.targetLayout(), AudioChannelLayout::Stereo);

    mixer.setTargetLayout(static_cast<AudioChannelLayout>(999));

    EXPECT_EQ(mixer.targetLayout(), AudioChannelLayout::Stereo);
}

TEST(AudioDownMixerStandaloneTest, SameLayoutPathPreservesMetadataAndSanitizesNonFiniteSamples)
{
    AudioDownMixer mixer;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const auto source = makeSegment(
        AudioChannelLayout::Stereo,
        {{1.0f, nan}, {-2.0f, infinity}}, 96000, 1234);

    const AudioSegment output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 96000);
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(output.startFrame, 1234);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_FLOAT_EQ(output.channelData[0][0], 1.0f);
    EXPECT_FLOAT_EQ(output.channelData[0][1], 0.0f);
    EXPECT_FLOAT_EQ(output.channelData[1][0], -2.0f);
    EXPECT_TRUE(std::isfinite(output.channelData[1][1]));
    EXPECT_GT(output.channelData[1][1], 0.0f);
}

TEST(AudioDownMixerStandaloneTest, MonoInputIsDuplicatedToBothStereoChannels)
{
    AudioDownMixer mixer;
    const auto source = makeSegment(AudioChannelLayout::Mono,
                                    {{0.25f, -0.5f}}, 44100, 77);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(output.sampleRate, 44100);
    EXPECT_EQ(output.startFrame, 77);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioDownMixerStandaloneTest, Surround51ToStereoUsesConfigurableChannelWeights)
{
    AudioDownMixer mixer;
    mixer.setCenterMixLevel(0.5f);
    mixer.setLFEMixLevel(0.25f);
    mixer.setSurroundMixLevel(0.125f);
    const auto source = makeSegment(AudioChannelLayout::Surround51,
                                    {{1.0f}, {2.0f}, {4.0f}, {8.0f}, {16.0f}, {32.0f}});

    const auto output = mixer.process(source);

    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_FLOAT_EQ(output.channelData[0][0], 7.0f);
    EXPECT_FLOAT_EQ(output.channelData[1][0], 10.0f);
}

TEST(AudioDownMixerStandaloneTest, StereoToMonoAveragesChannelsAndPreservesTiming)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Mono);
    const auto source = makeSegment(AudioChannelLayout::Stereo,
                                    {{1.0f, -1.0f}, {3.0f, 1.0f}}, 32000, 900);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.layout, AudioChannelLayout::Mono);
    EXPECT_EQ(output.sampleRate, 32000);
    EXPECT_EQ(output.startFrame, 900);
    ASSERT_EQ(output.channelData.size(), 1);
    EXPECT_EQ(output.channelData[0], (QVector<float>{2.0f, 0.0f}));
}

TEST(AudioDownMixerStandaloneTest, ExplicitChannelMapReordersDuplicatesAndSilencesInvalidIndices)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround51);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto source = makeSegment(AudioChannelLayout::Stereo,
                                    {{1.0f, 2.0f}, {3.0f, nan}}, 44100, 19);

    const auto output = mixer.processChannelMap(
        source, QVector<int>{1, 0, 1, -1, 9, 0});

    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
    EXPECT_EQ(output.sampleRate, 44100);
    EXPECT_EQ(output.startFrame, 19);
    ASSERT_EQ(output.channelData.size(), 6);
    EXPECT_EQ(output.channelData[0], (QVector<float>{3.0f, 0.0f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{1.0f, 2.0f}));
    EXPECT_EQ(output.channelData[2], (QVector<float>{3.0f, 0.0f}));
    EXPECT_EQ(output.channelData[3], (QVector<float>{0.0f, 0.0f}));
    EXPECT_EQ(output.channelData[4], (QVector<float>{0.0f, 0.0f}));
    EXPECT_EQ(output.channelData[5], (QVector<float>{1.0f, 2.0f}));
}

TEST(AudioDownMixerStandaloneTest, Surround71ToSurround51FoldsBackChannelsIntoSurrounds)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround51);
    mixer.setBackMixLevel(0.5f);
    const auto source = makeSegment(AudioChannelLayout::Surround71,
                                    {{1.0f}, {2.0f}, {3.0f}, {4.0f},
                                     {5.0f}, {6.0f}, {7.0f}, {8.0f}});

    const auto output = mixer.process(source);

    ASSERT_EQ(output.channelData.size(), 6);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
    EXPECT_FLOAT_EQ(output.channelData[0][0], 1.0f);
    EXPECT_FLOAT_EQ(output.channelData[1][0], 2.0f);
    EXPECT_FLOAT_EQ(output.channelData[2][0], 3.0f);
    EXPECT_FLOAT_EQ(output.channelData[3][0], 4.0f);
    EXPECT_FLOAT_EQ(output.channelData[4][0], 8.5f);
    EXPECT_FLOAT_EQ(output.channelData[5][0], 10.0f);
}

TEST(AudioDownMixerStandaloneTest, EmptyInputProducesRequestedLayoutAndKeepsMetadata)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Mono);
    const auto source = makeSegment(AudioChannelLayout::Stereo, {}, 22050, 456);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 22050);
    EXPECT_EQ(output.layout, AudioChannelLayout::Mono);
    EXPECT_EQ(output.startFrame, 456);
    EXPECT_TRUE(output.channelData.isEmpty());
}
