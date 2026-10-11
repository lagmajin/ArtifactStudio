#include <gtest/gtest.h>

#include <QVector>

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
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

TEST(AudioCacheStandaloneTest, ClearRemovesAllFramesAndMemoryUsage)
{
    AudioCache cache;
    cache.addCache(1, makeMono({1.0f, 2.0f}));
    cache.addCache(2, makeMono({3.0f}));
    cache.clear();

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

