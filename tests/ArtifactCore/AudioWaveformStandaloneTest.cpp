#include <gtest/gtest.h>

#include <QVector>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Waveform;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.channelData.push_back(std::move(samples));
    return segment;
}
}

TEST(AudioWaveformStandaloneTest, ExposesExpectedDefaultIdentityAndResolution)
{
    const AudioWaveform waveform;
    EXPECT_EQ(toStdString(waveform.getName()), "Audio Waveform");
    EXPECT_EQ(waveform.getResolution(), 512);
    EXPECT_EQ(waveform.getWaveformData().size(), 512u);
    for (const float value : waveform.getWaveformData())
        EXPECT_FLOAT_EQ(value, 0.0f);
}

TEST(AudioWaveformStandaloneTest, ResolutionClampsToSupportedRange)
{
    AudioWaveform waveform;
    waveform.setResolution(0);
    EXPECT_EQ(waveform.getResolution(), 1);
    waveform.setResolution(-12);
    EXPECT_EQ(waveform.getResolution(), 1);
    waveform.setResolution((1 << 20) + 1);
    EXPECT_EQ(waveform.getResolution(), 1 << 20);
    waveform.setResolution(8);
    EXPECT_EQ(waveform.getResolution(), 8);
}

TEST(AudioWaveformStandaloneTest, MonoSamplesProducePerBucketRms)
{
    AudioWaveform waveform;
    waveform.setResolution(2);
    auto segment = makeMono(48000, QVector<float>{1.0f, 1.0f, 0.0f, 0.0f});
    const auto original = segment.channelData[0];

    waveform.process(segment);

    ASSERT_EQ(waveform.getWaveformData().size(), 2u);
    EXPECT_FLOAT_EQ(waveform.getWaveformData()[0], 1.0f);
    EXPECT_FLOAT_EQ(waveform.getWaveformData()[1], 0.0f);
    EXPECT_EQ(segment.channelData[0], original);
}

TEST(AudioWaveformStandaloneTest, StereoSamplesAreCombinedAcrossChannels)
{
    AudioWaveform waveform;
    waveform.setResolution(2);
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Stereo;
    segment.channelData.push_back(QVector<float>{1.0f, 1.0f, 0.0f, 0.0f});
    segment.channelData.push_back(QVector<float>{0.0f, 0.0f, 1.0f, 1.0f});

    waveform.process(segment);

    ASSERT_EQ(waveform.getWaveformData().size(), 2u);
    EXPECT_NEAR(waveform.getWaveformData()[0], std::sqrt(0.5f), 1e-6f);
    EXPECT_NEAR(waveform.getWaveformData()[1], std::sqrt(0.5f), 1e-6f);
}

TEST(AudioWaveformStandaloneTest, NonFiniteSamplesAreExcludedFromRms)
{
    AudioWaveform waveform;
    waveform.setResolution(2);
    auto segment = makeMono(48000, QVector<float>{3.0f, 3.0f,
                                                  std::numeric_limits<float>::quiet_NaN(),
                                                  std::numeric_limits<float>::infinity()});

    waveform.process(segment);

    ASSERT_EQ(waveform.getWaveformData().size(), 2u);
    EXPECT_FLOAT_EQ(waveform.getWaveformData()[0], 3.0f);
    EXPECT_FLOAT_EQ(waveform.getWaveformData()[1], 0.0f);
}

TEST(AudioWaveformStandaloneTest, EmptyBlockClearsPreviouslyCapturedWaveform)
{
    AudioWaveform waveform;
    waveform.setResolution(4);
    auto nonEmpty = makeMono(48000, QVector<float>{1.0f, 0.5f, 0.25f, 0.125f});
    waveform.process(nonEmpty);
    ASSERT_NE(waveform.getWaveformData()[0], 0.0f);

    auto empty = makeMono(48000, {});
    waveform.process(empty);

    ASSERT_EQ(waveform.getWaveformData().size(), 4u);
    for (const float value : waveform.getWaveformData())
        EXPECT_FLOAT_EQ(value, 0.0f);
}

TEST(AudioWaveformStandaloneTest, ResolutionAboveFrameCountProducesOneBinPerFrame)
{
    AudioWaveform waveform;
    waveform.setResolution(32);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    waveform.process(segment);

    ASSERT_EQ(waveform.getWaveformData().size(), 3u);
    EXPECT_FLOAT_EQ(waveform.getWaveformData()[0], 0.25f);
    EXPECT_FLOAT_EQ(waveform.getWaveformData()[1], 0.5f);
    EXPECT_FLOAT_EQ(waveform.getWaveformData()[2], 0.75f);
}

TEST(AudioWaveformStandaloneTest, BypassPreservesInputAndPreviousWaveform)
{
    AudioWaveform waveform;
    waveform.setResolution(2);
    auto first = makeMono(48000, QVector<float>{0.25f, 0.25f, 0.75f, 0.75f});
    waveform.process(first);
    const auto previous = waveform.getWaveformData();
    waveform.setBypass(true);
    auto bypassed = makeMono(48000, QVector<float>{1.0f, -1.0f});

    waveform.process(bypassed);

    EXPECT_EQ(bypassed.channelData[0], (QVector<float>{1.0f, -1.0f}));
    EXPECT_EQ(waveform.getWaveformData(), previous);
}

TEST(AudioWaveformStandaloneTest, ChannelsWithoutSamplesProduceSilenceBins)
{
    AudioWaveform waveform;
    waveform.setResolution(2);
    auto seed = makeMono(48000, QVector<float>{0.5f, 0.25f});
    waveform.process(seed);
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Stereo;
    segment.channelData.push_back(QVector<float>{});
    segment.channelData.push_back(QVector<float>{});

    EXPECT_NO_THROW(waveform.process(segment));
    ASSERT_EQ(waveform.getWaveformData().size(), 2u);
    for (const float value : waveform.getWaveformData())
        EXPECT_FLOAT_EQ(value, 0.0f);
}

