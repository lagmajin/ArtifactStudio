#include <gtest/gtest.h>

#include <QVector>

#include <initializer_list>
#include <utility>

import Audio.RingBuffer;
import Audio.Segment;

using namespace ArtifactCore;

namespace {
AudioSegment makeStereo(std::initializer_list<float> left,
                        std::initializer_list<float> right)
{
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Stereo;

    QVector<float> leftSamples;
    for (const float sample : left)
        leftSamples.push_back(sample);
    QVector<float> rightSamples;
    for (const float sample : right)
        rightSamples.push_back(sample);
    segment.channelData.push_back(std::move(leftSamples));
    segment.channelData.push_back(std::move(rightSamples));
    return segment;
}
}

TEST(AudioRingBufferStandaloneTest, CapacityNeverFallsBelowOneFrame)
{
    AudioRingBuffer zeroCapacity(0);
    AudioRingBuffer oneFrameCapacity(1);

    EXPECT_EQ(zeroCapacity.capacity(), 1u);
    EXPECT_EQ(zeroCapacity.freeSpace(), 1u);
    EXPECT_TRUE(zeroCapacity.isEmpty());
    EXPECT_EQ(oneFrameCapacity.capacity(), 1u);
}

TEST(AudioRingBufferStandaloneTest, PartialReadsConsumeOnlyRequestedFrames)
{
    AudioRingBuffer buffer(5);
    ASSERT_TRUE(buffer.write(makeStereo({1.0f, 2.0f, 3.0f},
                                        {-1.0f, -2.0f, -3.0f})));
    EXPECT_EQ(buffer.available(), 3u);
    EXPECT_EQ(buffer.freeSpace(), 2u);

    AudioSegment output;
    ASSERT_TRUE(buffer.read(output, 2));
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_EQ(output.channelData[0], (QVector<float>{1.0f, 2.0f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{-1.0f, -2.0f}));
    EXPECT_EQ(buffer.available(), 1u);

    ASSERT_TRUE(buffer.read(output, 4));
    EXPECT_EQ(output.channelData[0], (QVector<float>{3.0f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{-3.0f}));
    EXPECT_TRUE(buffer.isEmpty());
    EXPECT_FALSE(buffer.isFull());
}

TEST(AudioRingBufferStandaloneTest, ReadsFramesCorrectlyAcrossPhysicalWrapAround)
{
    AudioRingBuffer buffer(4);
    ASSERT_TRUE(buffer.write(makeStereo({1.0f, 2.0f, 3.0f},
                                        {-1.0f, -2.0f, -3.0f})));
    AudioSegment output;
    ASSERT_TRUE(buffer.read(output, 2));
    EXPECT_EQ(output.channelData[0], (QVector<float>{1.0f, 2.0f}));

    ASSERT_TRUE(buffer.write(makeStereo({4.0f, 5.0f, 6.0f},
                                        {-4.0f, -5.0f, -6.0f})));
    EXPECT_EQ(buffer.available(), 4u);
    EXPECT_TRUE(buffer.isFull());
    ASSERT_TRUE(buffer.read(output, 4));
    EXPECT_EQ(output.channelData[0], (QVector<float>{3.0f, 4.0f, 5.0f, 6.0f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{-3.0f, -4.0f, -5.0f, -6.0f}));
    EXPECT_TRUE(buffer.isEmpty());
}

TEST(AudioRingBufferStandaloneTest, CapacityRejectionLeavesPreviouslyWrittenFramesIntact)
{
    AudioRingBuffer buffer(4);
    ASSERT_TRUE(buffer.write(makeStereo({1.0f, 2.0f, 3.0f},
                                        {-1.0f, -2.0f, -3.0f})));

    EXPECT_FALSE(buffer.write(makeStereo({4.0f, 5.0f}, {-4.0f, -5.0f})));
    EXPECT_EQ(buffer.available(), 3u);
    EXPECT_EQ(buffer.freeSpace(), 1u);

    AudioSegment output;
    ASSERT_TRUE(buffer.read(output, 4));
    EXPECT_EQ(output.channelData[0], (QVector<float>{1.0f, 2.0f, 3.0f}));
}

TEST(AudioRingBufferStandaloneTest, ClearDiscardsOldFramesButRetainsLaterWrites)
{
    AudioRingBuffer buffer(8);
    ASSERT_TRUE(buffer.write(makeStereo({1.0f, 2.0f}, {-1.0f, -2.0f})));
    buffer.clear();
    ASSERT_TRUE(buffer.write(makeStereo({3.0f, 4.0f}, {-3.0f, -4.0f})));

    AudioSegment output;
    EXPECT_FALSE(buffer.read(output, 8));
    EXPECT_TRUE(output.channelData.isEmpty());
    EXPECT_EQ(buffer.available(), 2u);

    ASSERT_TRUE(buffer.read(output, 8));
    EXPECT_EQ(output.channelData[0], (QVector<float>{3.0f, 4.0f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{-3.0f, -4.0f}));
}

TEST(AudioRingBufferStandaloneTest, CapacityChangeClearsBufferedFramesAndClampsToOne)
{
    AudioRingBuffer buffer(3);
    ASSERT_TRUE(buffer.write(makeStereo({1.0f, 2.0f}, {-1.0f, -2.0f})));

    buffer.setCapacity(0);

    EXPECT_EQ(buffer.capacity(), 1u);
    EXPECT_EQ(buffer.available(), 0u);
    EXPECT_EQ(buffer.freeSpace(), 1u);
    EXPECT_TRUE(buffer.isEmpty());
    EXPECT_TRUE(buffer.write(makeStereo({3.0f}, {-3.0f})));
    EXPECT_TRUE(buffer.isFull());
}

TEST(AudioRingBufferStandaloneTest, EmptyWritesAndZeroFrameReadsDoNotChangeBufferedData)
{
    AudioRingBuffer buffer(4);
    ASSERT_TRUE(buffer.write(makeStereo({1.0f, 2.0f}, {-1.0f, -2.0f})));
    const AudioSegment empty;
    EXPECT_TRUE(buffer.write(empty));

    AudioSegment output;
    EXPECT_FALSE(buffer.read(output, 0));
    EXPECT_TRUE(output.channelData.isEmpty());
    EXPECT_EQ(buffer.available(), 2u);
    ASSERT_TRUE(buffer.read(output, 2));
    EXPECT_EQ(output.channelData[0], (QVector<float>{1.0f, 2.0f}));
}

TEST(AudioRingBufferStandaloneTest, MonoInputIsDuplicatedIntoStereoOutput)
{
    AudioRingBuffer buffer(4);
    AudioSegment mono;
    mono.layout = AudioChannelLayout::Mono;
    mono.channelData.push_back(QVector<float>{0.25f, -0.5f});
    ASSERT_TRUE(buffer.write(mono));

    AudioSegment output;
    ASSERT_TRUE(buffer.read(output, 2));
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioRingBufferStandaloneTest, ChannelCountGrowsOnlyWhenEmptyAndDoesNotShrink)
{
    AudioRingBuffer buffer(4);
    ASSERT_TRUE(buffer.write(makeStereo({1.0f, 2.0f}, {-1.0f, -2.0f})));

    AudioSegment surround51;
    surround51.layout = AudioChannelLayout::Surround51;
    for (int channel = 0; channel < 6; ++channel)
        surround51.channelData.push_back(QVector<float>{static_cast<float>(channel + 10)});
    EXPECT_FALSE(buffer.write(surround51));
    EXPECT_EQ(buffer.available(), 2u);

    AudioSegment output;
    ASSERT_TRUE(buffer.read(output, 2));
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);

    ASSERT_TRUE(buffer.write(surround51));
    ASSERT_TRUE(buffer.read(output, 1));
    ASSERT_EQ(output.channelData.size(), 6);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
    for (int channel = 0; channel < 6; ++channel)
        EXPECT_FLOAT_EQ(output.channelData[channel][0], static_cast<float>(channel + 10));

    ASSERT_TRUE(buffer.write(makeStereo({7.0f}, {-7.0f})));
    ASSERT_TRUE(buffer.read(output, 1));
    ASSERT_EQ(output.channelData.size(), 6);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
    EXPECT_FLOAT_EQ(output.channelData[0][0], 7.0f);
    EXPECT_FLOAT_EQ(output.channelData[1][0], -7.0f);
    for (int channel = 2; channel < 6; ++channel)
        EXPECT_FLOAT_EQ(output.channelData[channel][0], 0.0f);
}
