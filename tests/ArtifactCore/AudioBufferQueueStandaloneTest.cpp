#include <gtest/gtest.h>

#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>

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

TEST(AudioBufferQueueStandaloneTest, ConcurrentProducerAndConsumerPreserveEveryQueuedSegment)
{
    constexpr qint64 itemCount = 2048;
    AudioBufferQueue queue;
    std::atomic<bool> producerFinished{false};
    std::atomic<bool> producerTimedOut{false};

    std::thread producer([&] {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(10);
        for (qint64 index = 0; index < itemCount; ++index) {
            const float value = static_cast<float>(index);
            const auto segment = makeSegment(
                index * 4, {value, value + 0.25f}, {-value, -value - 0.25f});
            while (!queue.push(segment)) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    producerTimedOut.store(true, std::memory_order_release);
                    producerFinished.store(true, std::memory_order_release);
                    return;
                }
                std::this_thread::yield();
            }
        }
        producerFinished.store(true, std::memory_order_release);
    });

    qint64 consumed = 0;
    AudioSegment output;
    while (true) {
        if (queue.pop(output)) {
            const float expected = static_cast<float>(consumed);
            EXPECT_EQ(output.startFrame, consumed * 4);
            EXPECT_EQ(output.sampleRate, 48000);
            EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
            EXPECT_EQ(output.channelData.size(), 2);
            if (output.channelData.size() == 2) {
                EXPECT_EQ(output.channelData[0],
                          (QVector<float>{expected, expected + 0.25f}));
                EXPECT_EQ(output.channelData[1],
                          (QVector<float>{-expected, -expected - 0.25f}));
            }
            ++consumed;
        } else if (producerFinished.load(std::memory_order_acquire)) {
            break;
        } else {
            std::this_thread::yield();
        }
    }

    producer.join();
    EXPECT_FALSE(producerTimedOut.load(std::memory_order_acquire));
    EXPECT_EQ(consumed, itemCount);
    EXPECT_TRUE(queue.isEmpty());
}

TEST(AudioBufferQueueStandaloneTest,
     ConcurrentClearAndDrainKeepSurvivingSegmentsCoherentAndQueueReusable)
{
    constexpr int itemCount = 2048;
    constexpr auto timeout = std::chrono::seconds(10);
    AudioBufferQueue queue;
    std::array<std::atomic<int>, itemCount> observed{};
    std::atomic<bool> producerFinished{false};
    std::atomic<bool> producerTimedOut{false};
    std::atomic<int> malformedItems{0};
    std::atomic<int> duplicateItems{0};

    std::thread producer([&] {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        for (int id = 0; id < itemCount; ++id) {
            const float sample = static_cast<float>(id);
            const auto segment = makeSegment(id, {sample, sample + 0.25f},
                                             {-sample, -sample - 0.25f});
            while (!queue.push(segment)) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    producerTimedOut.store(true, std::memory_order_release);
                    producerFinished.store(true, std::memory_order_release);
                    return;
                }
                std::this_thread::yield();
            }
        }
        producerFinished.store(true, std::memory_order_release);
    });

    std::thread clearer([&] {
        while (!producerFinished.load(std::memory_order_acquire)) {
            queue.clear();
            std::this_thread::yield();
        }
    });

    std::thread consumer([&] {
        AudioSegment segment;
        while (!producerFinished.load(std::memory_order_acquire) ||
               !queue.isEmpty()) {
            if (!queue.pop(segment)) {
                std::this_thread::yield();
                continue;
            }

            if (segment.startFrame < 0 || segment.startFrame >= itemCount ||
                segment.sampleRate != 48000 ||
                segment.layout != AudioChannelLayout::Stereo ||
                segment.channelData.size() != 2) {
                malformedItems.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            const int id = static_cast<int>(segment.startFrame);
            const float sample = static_cast<float>(id);
            if (segment.channelData[0] !=
                    (QVector<float>{sample, sample + 0.25f}) ||
                segment.channelData[1] !=
                    (QVector<float>{-sample, -sample - 0.25f})) {
                malformedItems.fetch_add(1, std::memory_order_relaxed);
            }
            if (observed[static_cast<std::size_t>(id)].fetch_add(
                    1, std::memory_order_relaxed) != 0) {
                duplicateItems.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    producer.join();
    clearer.join();
    consumer.join();

    EXPECT_FALSE(producerTimedOut.load(std::memory_order_acquire));
    EXPECT_EQ(malformedItems.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(duplicateItems.load(std::memory_order_relaxed), 0);
    EXPECT_TRUE(queue.isEmpty());
    EXPECT_FALSE(queue.isFull());
    for (int id = 0; id < itemCount; ++id)
        EXPECT_LE(observed[static_cast<std::size_t>(id)].load(
                      std::memory_order_relaxed), 1);

    for (int slot = 0; slot < 100; ++slot)
        ASSERT_TRUE(queue.push(makeSegment(slot, {0.0f}, {0.0f}))) << slot;
    EXPECT_TRUE(queue.isFull());
    EXPECT_FALSE(queue.push(makeSegment(100, {1.0f}, {-1.0f})));
}

TEST(AudioBufferQueueStandaloneTest,
     MultipleProducersAndConsumersDeliverEverySegmentExactlyOnce)
{
    constexpr int producerCount = 4;
    constexpr int consumerCount = 3;
    constexpr int itemsPerProducer = 512;
    constexpr int totalItems = producerCount * itemsPerProducer;
    constexpr auto timeout = std::chrono::seconds(10);

    AudioBufferQueue queue;
    std::array<std::atomic<int>, totalItems> observed{};
    std::atomic<int> finishedProducers{0};
    std::atomic<int> consumedItems{0};
    std::atomic<int> malformedItems{0};
    std::atomic<bool> timedOut{false};
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    std::array<std::thread, producerCount> producers;
    for (int producer = 0; producer < producerCount; ++producer) {
        producers[static_cast<std::size_t>(producer)] = std::thread([
            &queue, &finishedProducers, &timedOut, deadline, producer] {
            for (int item = 0; item < itemsPerProducer; ++item) {
                const int id = producer * itemsPerProducer + item;
                const float sample = static_cast<float>(id);
                const auto segment = makeSegment(id, {sample, sample + 0.25f},
                                                 {-sample, -sample - 0.25f});
                while (!queue.push(segment)) {
                    if (std::chrono::steady_clock::now() >= deadline) {
                        timedOut.store(true, std::memory_order_release);
                        finishedProducers.fetch_add(1, std::memory_order_release);
                        return;
                    }
                    std::this_thread::yield();
                }
            }
            finishedProducers.fetch_add(1, std::memory_order_release);
        });
    }

    std::array<std::thread, consumerCount> consumers;
    for (auto& consumer : consumers) {
        consumer = std::thread([&queue, &observed, &finishedProducers,
                                &consumedItems, &malformedItems, &timedOut,
                                deadline] {
            AudioSegment segment;
            while (true) {
                if (queue.pop(segment)) {
                    if (segment.startFrame < 0 || segment.startFrame >= totalItems ||
                        segment.sampleRate != 48000 ||
                        segment.layout != AudioChannelLayout::Stereo ||
                        segment.channelData.size() != 2) {
                        malformedItems.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        const int id = static_cast<int>(segment.startFrame);
                        const float sample = static_cast<float>(id);
                        if (segment.channelData[0] !=
                                (QVector<float>{sample, sample + 0.25f}) ||
                            segment.channelData[1] !=
                                (QVector<float>{-sample, -sample - 0.25f})) {
                            malformedItems.fetch_add(1, std::memory_order_relaxed);
                        }
                        observed[static_cast<std::size_t>(id)].fetch_add(
                            1, std::memory_order_relaxed);
                    }
                    consumedItems.fetch_add(1, std::memory_order_release);
                    continue;
                }

                if (finishedProducers.load(std::memory_order_acquire) == producerCount) {
                    break;
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    timedOut.store(true, std::memory_order_release);
                    break;
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }
    for (auto& consumer : consumers) {
        consumer.join();
    }

    EXPECT_FALSE(timedOut.load(std::memory_order_acquire));
    EXPECT_EQ(consumedItems.load(std::memory_order_acquire), totalItems);
    EXPECT_EQ(malformedItems.load(std::memory_order_relaxed), 0);
    for (const auto& count : observed) {
        EXPECT_EQ(count.load(std::memory_order_relaxed), 1);
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
