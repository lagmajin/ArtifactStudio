#include <gtest/gtest.h>

#include <QVector>

#include <initializer_list>
#include <utility>

import Audio.BufferQueue;
import Audio.Segment;

using namespace ArtifactCore;

namespace {
AudioSegment makeSegment(qint64 startFrame,
                         std::initializer_list<float> left,
                         std::initializer_list<float> right)
{
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Stereo;
    segment.startFrame = startFrame;

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

TEST(AudioBufferQueueStandaloneTest, StartsEmptyAndCannotPopWithoutData)
{
    AudioBufferQueue queue;
    auto output = makeSegment(42, {0.5f}, {-0.5f});

    EXPECT_TRUE(queue.isEmpty());
    EXPECT_FALSE(queue.isFull());
    EXPECT_FALSE(queue.pop(output));
    EXPECT_TRUE(queue.isEmpty());
    EXPECT_EQ(output.startFrame, 42);
    EXPECT_FLOAT_EQ(output.channelData[0][0], 0.5f);
}

TEST(AudioBufferQueueStandaloneTest, PushAndPopPreserveSegmentPayloadAndMetadata)
{
    AudioBufferQueue queue;
    const auto input = makeSegment(1234, {0.25f, -0.75f}, {0.5f, 1.0f});
    ASSERT_TRUE(queue.push(input));
    EXPECT_FALSE(queue.isEmpty());

    AudioSegment output;
    ASSERT_TRUE(queue.pop(output));
    EXPECT_EQ(output.sampleRate, 48000);
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(output.startFrame, 1234);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f, -0.75f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.5f, 1.0f}));
    EXPECT_TRUE(queue.isEmpty());
}

TEST(AudioBufferQueueStandaloneTest, EnqueuedSegmentIsIsolatedFromLaterSourceEdits)
{
    AudioBufferQueue queue;
    auto input = makeSegment(1234, {0.25f, -0.75f}, {0.5f, 1.0f});
    ASSERT_TRUE(queue.push(input));

    input.startFrame = 9999;
    input.sampleRate = 96000;
    input.channelData[0][0] = -0.25f;
    input.channelData[1][1] = -1.0f;

    AudioSegment output;
    ASSERT_TRUE(queue.pop(output));
    EXPECT_EQ(output.startFrame, 1234);
    EXPECT_EQ(output.sampleRate, 48000);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f, -0.75f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.5f, 1.0f}));
}

TEST(AudioBufferQueueStandaloneTest, PopReplacesExistingOutputSegment)
{
    AudioBufferQueue queue;
    ASSERT_TRUE(queue.push(makeSegment(1234, {0.25f}, {0.5f})));

    AudioSegment output = makeSegment(99, {-1.0f}, {-2.0f});
    output.channelData.push_back(QVector<float>{-3.0f});
    ASSERT_TRUE(queue.pop(output));

    EXPECT_EQ(output.sampleRate, 48000);
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(output.startFrame, 1234);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.5f}));
}

TEST(AudioBufferQueueStandaloneTest, RejectsSegmentsWithoutFramesOrSampleRate)
{
    AudioBufferQueue queue;
    const AudioSegment noChannels;
    auto noRate = makeSegment(1, {0.0f}, {0.0f});
    noRate.sampleRate = 0;
    auto negativeRate = makeSegment(2, {0.0f}, {0.0f});
    negativeRate.sampleRate = -1;

    EXPECT_FALSE(queue.push(noChannels));
    EXPECT_FALSE(queue.push(noRate));
    EXPECT_FALSE(queue.push(negativeRate));
    EXPECT_TRUE(queue.isEmpty());
}

TEST(AudioBufferQueueStandaloneTest, RejectsZeroFrameChannelsWithoutChangingQueue)
{
    AudioBufferQueue queue;
    ASSERT_TRUE(queue.push(makeSegment(42, {0.25f}, {0.5f})));
    const auto emptyFrames = makeSegment(99, {}, {9.0f});

    EXPECT_FALSE(queue.push(emptyFrames));
    EXPECT_FALSE(queue.isEmpty());

    AudioSegment output;
    ASSERT_TRUE(queue.pop(output));
    EXPECT_EQ(output.startFrame, 42);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.5f}));
    EXPECT_TRUE(queue.isEmpty());
}

TEST(AudioBufferQueueStandaloneTest, InvalidPushLeavesExistingQueuedSegmentUntouched)
{
    AudioBufferQueue queue;
    ASSERT_TRUE(queue.push(makeSegment(42, {0.25f}, {0.5f})));

    const AudioSegment noChannels;
    auto noRate = makeSegment(99, {9.0f}, {-9.0f});
    noRate.sampleRate = 0;
    EXPECT_FALSE(queue.push(noChannels));
    EXPECT_FALSE(queue.push(noRate));
    EXPECT_FALSE(queue.isEmpty());

    AudioSegment output;
    ASSERT_TRUE(queue.pop(output));
    EXPECT_EQ(output.startFrame, 42);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.5f}));
    EXPECT_TRUE(queue.isEmpty());
}

TEST(AudioBufferQueueStandaloneTest, PreservesFifoOrderForMultipleSegments)
{
    AudioBufferQueue queue;
    ASSERT_TRUE(queue.push(makeSegment(10, {1.0f}, {-1.0f})));
    ASSERT_TRUE(queue.push(makeSegment(20, {2.0f}, {-2.0f})));
    ASSERT_TRUE(queue.push(makeSegment(30, {3.0f}, {-3.0f})));

    for (const qint64 expectedStart : {10, 20, 30}) {
        AudioSegment output;
        ASSERT_TRUE(queue.pop(output));
        EXPECT_EQ(output.startFrame, expectedStart);
        EXPECT_FLOAT_EQ(output.channelData[0][0],
                        static_cast<float>(expectedStart / 10));
    }
    EXPECT_TRUE(queue.isEmpty());
}

TEST(AudioBufferQueueStandaloneTest, RejectsPushAtCapacityAndAcceptsAfterPop)
{
    AudioBufferQueue queue;
    for (qint64 frame = 0; frame < 100; ++frame)
        ASSERT_TRUE(queue.push(makeSegment(frame, {0.0f}, {0.0f})));
    EXPECT_TRUE(queue.isFull());
    EXPECT_FALSE(queue.push(makeSegment(100, {1.0f}, {-1.0f})));

    AudioSegment output;
    ASSERT_TRUE(queue.pop(output));
    EXPECT_EQ(output.startFrame, 0);
    EXPECT_FALSE(queue.isFull());
    EXPECT_TRUE(queue.push(makeSegment(100, {1.0f}, {-1.0f})));
    EXPECT_TRUE(queue.isFull());
}

TEST(AudioBufferQueueStandaloneTest, RejectedFullPushLeavesQueuedFifoUnchanged)
{
    AudioBufferQueue queue;
    for (qint64 frame = 0; frame < 100; ++frame) {
        ASSERT_TRUE(queue.push(makeSegment(
            frame, {static_cast<float>(frame)}, {-static_cast<float>(frame)})));
    }

    EXPECT_FALSE(queue.push(makeSegment(1000, {1000.0f}, {-1000.0f})));

    for (qint64 expectedFrame = 0; expectedFrame < 100; ++expectedFrame) {
        AudioSegment output;
        ASSERT_TRUE(queue.pop(output));
        EXPECT_EQ(output.startFrame, expectedFrame);
        EXPECT_FLOAT_EQ(output.channelData[0][0], static_cast<float>(expectedFrame));
        EXPECT_FLOAT_EQ(output.channelData[1][0], -static_cast<float>(expectedFrame));
    }
    EXPECT_TRUE(queue.isEmpty());
}

TEST(AudioBufferQueueStandaloneTest, ClearDropsQueuedSegments)
{
    AudioBufferQueue queue;
    ASSERT_TRUE(queue.push(makeSegment(1, {0.1f}, {0.2f})));
    ASSERT_TRUE(queue.push(makeSegment(2, {0.3f}, {0.4f})));

    queue.clear();

    EXPECT_TRUE(queue.isEmpty());
    AudioSegment output;
    EXPECT_FALSE(queue.pop(output));
}

TEST(AudioBufferQueueStandaloneTest, ClearingFullQueueRestoresAllCapacity)
{
    AudioBufferQueue queue;
    for (qint64 frame = 0; frame < 100; ++frame)
        ASSERT_TRUE(queue.push(makeSegment(frame, {0.0f}, {0.0f})));
    ASSERT_TRUE(queue.isFull());

    queue.clear();

    EXPECT_TRUE(queue.isEmpty());
    EXPECT_FALSE(queue.isFull());
    EXPECT_TRUE(queue.push(makeSegment(100, {1.0f}, {-1.0f})));
    AudioSegment output;
    ASSERT_TRUE(queue.pop(output));
    EXPECT_EQ(output.startFrame, 100);
    EXPECT_TRUE(queue.isEmpty());
}

TEST(AudioBufferQueueStandaloneTest, QueueAcceptsFreshSegmentsAfterClear)
{
    AudioBufferQueue queue;
    ASSERT_TRUE(queue.push(makeSegment(1, {0.1f}, {0.2f})));

    queue.clear();
    ASSERT_TRUE(queue.push(makeSegment(2, {0.3f}, {0.4f})));

    AudioSegment output;
    ASSERT_TRUE(queue.pop(output));
    EXPECT_EQ(output.startFrame, 2);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.3f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.4f}));
    EXPECT_TRUE(queue.isEmpty());
}
