#include <gtest/gtest.h>

#include <QVector>

#include <chrono>
#include <cstdint>
#include <limits>
#include <initializer_list>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

import Audio.Cache;
import Audio.Segment;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(std::initializer_list<float> samples,
                      int sampleRate = 48000,
                      qint64 startFrame = 0)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = startFrame;
    segment.channelData.push_back(QVector<float>(samples));
    return segment;
}
}

TEST(AudioCacheStandaloneTest, StartsEmptyWithConfiguredDefaultCapacity)
{
    const AudioCache cache;
    EXPECT_EQ(cache.getCacheSize(), 0u);
    EXPECT_EQ(cache.getMaxCacheFrames(), 300);
    EXPECT_EQ(cache.getMemoryUsage(), 0u);
}

TEST(AudioCacheStandaloneTest, AddAndGetPreserveAudioSegmentData)
{
    AudioCache cache;
    cache.addCache(12, makeMono({0.25f, -0.5f, 0.75f}, 44100, 90));
    AudioSegment out;

    ASSERT_TRUE(cache.getCached(12, out));
    EXPECT_EQ(out.sampleRate, 44100);
    EXPECT_EQ(out.layout, AudioChannelLayout::Mono);
    EXPECT_EQ(out.startFrame, 90);
    ASSERT_EQ(out.channelData.size(), 1);
    EXPECT_EQ(out.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioCacheStandaloneTest, RetrievedSegmentEditsDoNotMutateCachedData)
{
    AudioCache cache;
    cache.addCache(12, makeMono({0.25f, -0.5f}));

    AudioSegment retrieved;
    ASSERT_TRUE(cache.getCached(12, retrieved));
    retrieved.channelData[0][0] = 0.9f;
    retrieved.channelData[0].push_back(0.1f);

    AudioSegment fetchedAgain;
    ASSERT_TRUE(cache.getCached(12, fetchedAgain));
    ASSERT_EQ(fetchedAgain.channelData.size(), 1);
    EXPECT_EQ(fetchedAgain.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioCacheStandaloneTest, MissingKeyReturnsFalseWithoutChangingOutput)
{
    AudioCache cache;
    AudioSegment out = makeMono({0.1f, 0.2f}, 32000, 7);

    EXPECT_FALSE(cache.getCached(999, out));
    EXPECT_EQ(out.sampleRate, 32000);
    EXPECT_EQ(out.startFrame, 7);
    EXPECT_EQ(out.channelData[0], (QVector<float>{0.1f, 0.2f}));
}

TEST(AudioCacheStandaloneTest, RejectsInvalidFrameNumbersAndSegments)
{
    AudioCache cache;
    cache.addCache(-1, makeMono({1.0f}));
    cache.addCache(1, makeMono({}));
    cache.addCache(2, makeMono({1.0f}, 0));
    EXPECT_EQ(cache.getCacheSize(), 0u);
}

TEST(AudioCacheStandaloneTest, ReplacingAnEntryUpdatesItsStoredSamples)
{
    AudioCache cache;
    cache.addCache(5, makeMono({0.1f}));
    cache.addCache(5, makeMono({0.9f, 0.8f}));
    AudioSegment out;

    ASSERT_EQ(cache.getCacheSize(), 1u);
    ASSERT_TRUE(cache.getCached(5, out));
    EXPECT_EQ(out.channelData[0], (QVector<float>{0.9f, 0.8f}));
}

TEST(AudioCacheStandaloneTest, ReplacingEntryUpdatesMemoryUsageWithoutDoubleCounting)
{
    AudioCache cache;
    cache.addCache(5, makeMono({0.1f}));
    ASSERT_EQ(cache.getMemoryUsage(), sizeof(float));

    cache.addCache(5, makeMono({0.9f, 0.8f, 0.7f}));

    EXPECT_EQ(cache.getCacheSize(), 1u);
    EXPECT_EQ(cache.getMemoryUsage(), 3u * sizeof(float));
}

TEST(AudioCacheStandaloneTest, MemoryUsageCountsFloatSamplesAcrossChannels)
{
    AudioCache cache;
    AudioSegment stereo;
    stereo.sampleRate = 48000;
    stereo.layout = AudioChannelLayout::Stereo;
    stereo.channelData.push_back(QVector<float>{1.0f, 2.0f, 3.0f});
    stereo.channelData.push_back(QVector<float>{4.0f, 5.0f, 6.0f});
    cache.addCache(0, std::move(stereo));

    EXPECT_EQ(cache.getMemoryUsage(), 6u * sizeof(float));
}

TEST(AudioCacheStandaloneTest, CapacityIsAtLeastOneAndOverflowCleanupBoundsSize)
{
    AudioCache cache;
    cache.setMaxCacheFrames(0);
    EXPECT_EQ(cache.getMaxCacheFrames(), 1);
    cache.setMaxCacheFrames(2);
    cache.addCache(1, makeMono({1.0f}));
    cache.addCache(2, makeMono({2.0f}));
    cache.addCache(3, makeMono({3.0f}));

    EXPECT_LE(cache.getCacheSize(), 2u);
    EXPECT_EQ(cache.getMaxCacheFrames(), 2);
}

TEST(AudioCacheStandaloneTest, CacheHitRefreshesEntryBeforeLruEviction)
{
    AudioCache cache;
    cache.setMaxCacheFrames(2);
    cache.addCache(1, makeMono({1.0f}));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    cache.addCache(2, makeMono({2.0f}));

    AudioSegment accessed;
    ASSERT_TRUE(cache.getCached(1, accessed));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    cache.addCache(3, makeMono({3.0f}));

    AudioSegment out;
    EXPECT_TRUE(cache.getCached(1, out));
    EXPECT_FALSE(cache.getCached(2, out));
    EXPECT_TRUE(cache.getCached(3, out));
}

TEST(AudioCacheStandaloneTest, ClearRemovesAllFramesAndMemoryUsage)
{
    AudioCache cache;
    cache.addCache(1, makeMono({1.0f, 2.0f}));
    cache.addCache(2, makeMono({3.0f}));
    cache.clear();

    EXPECT_EQ(cache.getCacheSize(), 0u);
    EXPECT_EQ(cache.getMemoryUsage(), 0u);
}

TEST(AudioCacheStandaloneTest, ClearExpiredUsesLastAccessTime)
{
    AudioCache cache;
    cache.addCache(1, makeMono({1.0f}));
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    cache.addCache(2, makeMono({2.0f}));
    AudioSegment out;
    ASSERT_TRUE(cache.getCached(2, out));

    cache.clearExpired(100);

    EXPECT_EQ(cache.getCacheSize(), 1u);
    EXPECT_FALSE(cache.getCached(1, out));
    EXPECT_TRUE(cache.getCached(2, out));
    ASSERT_EQ(out.channelData.size(), 1);
    EXPECT_FLOAT_EQ(out.channelData[0][0], 2.0f);
}

TEST(AudioCacheStandaloneTest, NegativeExpiryAgeBehavesAsZeroMilliseconds)
{
    AudioCache cache;
    cache.addCache(7, makeMono({0.75f}));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    cache.clearExpired(-1);

    EXPECT_EQ(cache.getCacheSize(), 0u);
    EXPECT_EQ(cache.getMemoryUsage(), 0u);
}

TEST(AudioCacheStandaloneTest, PrefetchRequestsSequentialFramesAndCachesResults)
{
    AudioCache cache;
    std::vector<int64_t> requested;
    cache.setPrefetchProvider([&requested](int64_t frame, AudioSegment& out) {
        requested.push_back(frame);
        out = makeMono({static_cast<float>(frame)});
        return true;
    });

    cache.prefetch(20, 3);

    EXPECT_EQ(requested, (std::vector<int64_t>{20, 21, 22}));
    EXPECT_EQ(cache.getCacheSize(), 3u);
    AudioSegment out;
    ASSERT_TRUE(cache.getCached(21, out));
    EXPECT_FLOAT_EQ(out.channelData[0][0], 21.0f);
}

TEST(AudioCacheStandaloneTest, InvalidPrefetchRangeDoesNotCallProvider)
{
    AudioCache cache;
    int calls = 0;
    cache.setPrefetchProvider([&calls](int64_t, AudioSegment&) {
        ++calls;
        return false;
    });

    cache.prefetch(-1, 5);
    cache.prefetch(0, 0);
    cache.prefetch(0, -2);

    EXPECT_EQ(calls, 0);
    EXPECT_EQ(cache.getCacheSize(), 0u);
}

TEST(AudioCacheStandaloneTest, PrefetchProviderCallsAreBoundedTo4096Frames)
{
    AudioCache cache;
    int calls = 0;
    int64_t firstFrame = -1;
    int64_t lastFrame = -1;
    cache.setPrefetchProvider([&](int64_t frame, AudioSegment&) {
        if (calls == 0) firstFrame = frame;
        lastFrame = frame;
        ++calls;
        return false;
    });

    cache.prefetch(25, 5000);

    EXPECT_EQ(calls, 4096);
    EXPECT_EQ(firstFrame, 25);
    EXPECT_EQ(lastFrame, 25 + 4095);
    EXPECT_EQ(cache.getCacheSize(), 0u);
}

TEST(AudioCacheStandaloneTest, PrefetchStopsAtMaximumFrameWithoutOverflow)
{
    AudioCache cache;
    std::vector<int64_t> requested;
    cache.setPrefetchProvider([&requested](int64_t frame, AudioSegment& out) {
        requested.push_back(frame);
        out = makeMono({static_cast<float>(requested.size())});
        return true;
    });

    const auto maximum = std::numeric_limits<int64_t>::max();
    cache.prefetch(maximum - 1, 4);

    EXPECT_EQ(requested, (std::vector<int64_t>{maximum - 1, maximum}));
    EXPECT_EQ(cache.getCacheSize(), 2u);
    AudioSegment out;
    EXPECT_TRUE(cache.getCached(maximum, out));
}

TEST(AudioCacheStandaloneTest, FalseProviderResultSkipsFrameAndContinuesPrefetch)
{
    AudioCache cache;
    std::vector<int64_t> requested;
    cache.setPrefetchProvider([&requested](int64_t frame, AudioSegment& out) {
        requested.push_back(frame);
        if (frame == 41) return false;
        out = makeMono({static_cast<float>(frame)});
        return true;
    });

    cache.prefetch(40, 3);

    EXPECT_EQ(requested, (std::vector<int64_t>{40, 41, 42}));
    EXPECT_EQ(cache.getCacheSize(), 2u);
    AudioSegment out;
    EXPECT_TRUE(cache.getCached(40, out));
    EXPECT_FALSE(cache.getCached(41, out));
    ASSERT_TRUE(cache.getCached(42, out));
    EXPECT_FLOAT_EQ(out.channelData[0][0], 42.0f);
}

TEST(AudioCacheStandaloneTest, FalseProviderResultPreservesExistingCachedFrame)
{
    AudioCache cache;
    cache.addCache(41, makeMono({0.25f, 0.5f}));
    cache.setPrefetchProvider([](int64_t, AudioSegment&) { return false; });

    cache.prefetch(41, 1);

    AudioSegment out;
    ASSERT_TRUE(cache.getCached(41, out));
    EXPECT_EQ(out.channelData[0], (QVector<float>{0.25f, 0.5f}));
    EXPECT_EQ(cache.getCacheSize(), 1u);
}

TEST(AudioCacheStandaloneTest, ProviderFailureDoesNotAbortLaterPrefetchFrames)
{
    AudioCache cache;
    std::vector<int64_t> requested;
    cache.setPrefetchProvider([&requested](int64_t frame, AudioSegment& out) {
        requested.push_back(frame);
        if (frame == 31) throw std::runtime_error("decode failure");
        out = makeMono({static_cast<float>(frame)});
        return true;
    });

    EXPECT_NO_THROW(cache.prefetch(30, 3));

    EXPECT_EQ(requested, (std::vector<int64_t>{30, 31, 32}));
    EXPECT_EQ(cache.getCacheSize(), 2u);
}

