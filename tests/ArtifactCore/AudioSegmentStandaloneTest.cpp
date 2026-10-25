#include <gtest/gtest.h>

#include <QVector>

#include <limits>

import Audio.Segment;

using namespace ArtifactCore;

TEST(AudioSegmentStandaloneTest, DefaultSegmentHasNoChannelsOrFrames)
{
    const AudioSegment segment;

    EXPECT_EQ(segment.sampleRate, 44100);
    EXPECT_EQ(segment.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(segment.startFrame, 0);
    EXPECT_EQ(segment.channelCount(), 0);
    EXPECT_EQ(segment.frameCount(), 0);
    EXPECT_EQ(segment.constData(0), nullptr);
}

TEST(AudioSegmentStandaloneTest, CopyKeepsIndependentSamplesAndMetadata)
{
    AudioSegment source;
    source.sampleRate = 48000;
    source.layout = AudioChannelLayout::Stereo;
    source.startFrame = 120;
    source.channelData = {
        QVector<float>{0.25f, -0.5f},
        QVector<float>{0.75f, 1.0f}};

    AudioSegment copy = source;
    source.startFrame = 240;
    source.channelData[0][0] = -0.25f;
    copy.sampleRate = 96000;
    copy.channelData[1][1] = -1.0f;

    EXPECT_EQ(source.sampleRate, 48000);
    EXPECT_EQ(source.startFrame, 240);
    EXPECT_EQ(source.channelData[0], (QVector<float>{-0.25f, -0.5f}));
    EXPECT_EQ(source.channelData[1], (QVector<float>{0.75f, 1.0f}));
    EXPECT_EQ(copy.sampleRate, 96000);
    EXPECT_EQ(copy.startFrame, 120);
    EXPECT_EQ(copy.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_EQ(copy.channelData[1], (QVector<float>{0.75f, -1.0f}));
}

TEST(AudioSegmentStandaloneTest, FrameCountUsesShortestChannel)
{
    AudioSegment segment;
    segment.channelData = {
        QVector<float>{1.0f, 2.0f, 3.0f, 4.0f},
        QVector<float>{5.0f, 6.0f},
        QVector<float>{7.0f, 8.0f, 9.0f}};

    EXPECT_EQ(segment.channelCount(), 3);
    EXPECT_EQ(segment.frameCount(), 2);
}

TEST(AudioSegmentStandaloneTest, FrameCountIsZeroWhenAnyChannelIsEmpty)
{
    AudioSegment segment;
    segment.channelData = {
        QVector<float>{1.0f, 2.0f},
        QVector<float>{},
        QVector<float>{3.0f}};

    EXPECT_EQ(segment.channelCount(), 3);
    EXPECT_EQ(segment.frameCount(), 0);
}

TEST(AudioSegmentStandaloneTest, SetFrameCountResizesEveryChannel)
{
    AudioSegment segment;
    segment.channelData = {
        QVector<float>{1.0f, 2.0f},
        QVector<float>{3.0f, 4.0f, 5.0f, 6.0f}};

    segment.setFrameCount(3);

    ASSERT_EQ(segment.channelData.size(), 2);
    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, 2.0f, 0.0f}));
    EXPECT_EQ(segment.channelData[1], (QVector<float>{3.0f, 4.0f, 5.0f}));
    EXPECT_EQ(segment.frameCount(), 3);
}

TEST(AudioSegmentStandaloneTest, SetFrameCountTruncatesEveryLongerChannel)
{
    AudioSegment segment;
    segment.channelData = {
        QVector<float>{1.0f, 2.0f, 3.0f},
        QVector<float>{4.0f, 5.0f, 6.0f, 7.0f}};

    segment.setFrameCount(2);

    ASSERT_EQ(segment.channelData.size(), 2);
    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, 2.0f}));
    EXPECT_EQ(segment.channelData[1], (QVector<float>{4.0f, 5.0f}));
    EXPECT_EQ(segment.frameCount(), 2);
}

TEST(AudioSegmentStandaloneTest, SetFrameCountDoesNotCreateMissingChannels)
{
    AudioSegment segment;
    segment.sampleRate = 96000;
    segment.layout = AudioChannelLayout::Surround51;
    segment.startFrame = 321;

    segment.setFrameCount(5);

    EXPECT_EQ(segment.channelCount(), 0);
    EXPECT_EQ(segment.frameCount(), 0);
    EXPECT_EQ(segment.sampleRate, 96000);
    EXPECT_EQ(segment.layout, AudioChannelLayout::Surround51);
    EXPECT_EQ(segment.startFrame, 321);
}

TEST(AudioSegmentStandaloneTest, NegativeFrameCountClampsAllChannelsToEmpty)
{
    AudioSegment segment;
    segment.channelData = {QVector<float>{1.0f}, QVector<float>{2.0f}};

    segment.setFrameCount(std::numeric_limits<int>::min());

    ASSERT_EQ(segment.channelData.size(), 2);
    EXPECT_TRUE(segment.channelData[0].isEmpty());
    EXPECT_TRUE(segment.channelData[1].isEmpty());
    EXPECT_EQ(segment.frameCount(), 0);
}

TEST(AudioSegmentStandaloneTest, ZeroClearsSamplesWithoutChangingShapeOrMetadata)
{
    AudioSegment segment;
    segment.sampleRate = 96000;
    segment.layout = AudioChannelLayout::Surround51;
    segment.startFrame = 123;
    segment.channelData = {
        QVector<float>{1.0f, -2.0f},
        QVector<float>{3.0f, -4.0f},
        QVector<float>{5.0f, -6.0f}};

    segment.zero();

    EXPECT_EQ(segment.sampleRate, 96000);
    EXPECT_EQ(segment.layout, AudioChannelLayout::Surround51);
    EXPECT_EQ(segment.startFrame, 123);
    ASSERT_EQ(segment.channelCount(), 3);
    EXPECT_EQ(segment.frameCount(), 2);
    for (const auto& channel : segment.channelData) {
        ASSERT_EQ(channel.size(), 2);
        EXPECT_FLOAT_EQ(channel[0], 0.0f);
        EXPECT_FLOAT_EQ(channel[1], 0.0f);
    }
}

TEST(AudioSegmentStandaloneTest, ClearRemovesChannelsButKeepsSegmentMetadata)
{
    AudioSegment segment;
    segment.sampleRate = 32000;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = 99;
    segment.channelData = {QVector<float>{0.5f, -0.5f}};

    segment.clear();

    EXPECT_EQ(segment.channelCount(), 0);
    EXPECT_EQ(segment.frameCount(), 0);
    EXPECT_EQ(segment.sampleRate, 32000);
    EXPECT_EQ(segment.layout, AudioChannelLayout::Mono);
    EXPECT_EQ(segment.startFrame, 99);
}

TEST(AudioSegmentStandaloneTest, ConstDataReturnsChannelSamplesAndRejectsInvalidIndices)
{
    AudioSegment segment;
    segment.channelData = {
        QVector<float>{0.25f, 0.5f},
        QVector<float>{-0.25f, -0.5f}};
    const AudioSegment& readOnly = segment;

    ASSERT_NE(readOnly.constData(0), nullptr);
    ASSERT_NE(readOnly.constData(1), nullptr);
    EXPECT_FLOAT_EQ(readOnly.constData(0)[1], 0.5f);
    EXPECT_FLOAT_EQ(readOnly.constData(1)[0], -0.25f);
    EXPECT_EQ(readOnly.constData(-1), nullptr);
    EXPECT_EQ(readOnly.constData(2), nullptr);
}
