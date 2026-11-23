#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

import Data.CsvDataSource;
import Data.DataCache;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
void resetCache(DataCache& cache)
{
    cache.clear();
    cache.setMaxEntries(64);
    cache.setTtlMs(0);
}

DataSourcePtr makeSource(const char* uri)
{
    return makeCsvDataSource(String(uri));
}
}

TEST(DataCacheStandaloneTest, MissingKeyReturnsNullAndIsStale)
{
    auto& cache = DataCache::instance();
    resetCache(cache);

    EXPECT_EQ(cache.get(String("missing.csv")), nullptr);
    EXPECT_TRUE(cache.isStale(String("missing.csv"), 123));
}

TEST(DataCacheStandaloneTest, PutThenGetReturnsTheSameSource)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    const auto source = makeSource("cache-value.csv");

    cache.put(String("cache-value.csv"), source, 10);

    EXPECT_EQ(cache.get(String("cache-value.csv")), source);
    EXPECT_EQ(cache.size(), 1);
}

TEST(DataCacheStandaloneTest, CacheKeysUseFullUri)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    const auto first = makeSource("first.csv");
    const auto second = makeSource("second.csv");

    cache.put(String("first.csv"), first, 1);
    cache.put(String("second.csv"), second, 1);

    EXPECT_EQ(cache.get(String("first.csv")), first);
    EXPECT_EQ(cache.get(String("second.csv")), second);
}

TEST(DataCacheStandaloneTest, MatchingModificationTimeIsNotStale)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.put(String("stable.csv"), makeSource("stable.csv"), 42);

    EXPECT_FALSE(cache.isStale(String("stable.csv"), 42));
}

TEST(DataCacheStandaloneTest, DifferentModificationTimeIsStale)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.put(String("changed.csv"), makeSource("changed.csv"), 42);

    EXPECT_TRUE(cache.isStale(String("changed.csv"), 43));
}

TEST(DataCacheStandaloneTest, InvalidateRemovesOnlyTheSelectedEntry)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.put(String("keep.csv"), makeSource("keep.csv"), 1);
    cache.put(String("remove.csv"), makeSource("remove.csv"), 1);

    cache.invalidate(String("remove.csv"));

    EXPECT_EQ(cache.get(String("remove.csv")), nullptr);
    EXPECT_NE(cache.get(String("keep.csv")), nullptr);
    EXPECT_EQ(cache.size(), 1);
}

TEST(DataCacheStandaloneTest, InvalidateAllAndClearEmptyTheCache)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.put(String("one.csv"), makeSource("one.csv"), 1);
    cache.put(String("two.csv"), makeSource("two.csv"), 1);

    cache.invalidateAll();
    EXPECT_EQ(cache.size(), 0);
    cache.put(String("three.csv"), makeSource("three.csv"), 1);
    cache.clear();
    EXPECT_EQ(cache.size(), 0);
}

TEST(DataCacheStandaloneTest, UpdatingExistingKeyReplacesSourceAndTimestamp)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    const auto oldSource = makeSource("old.csv");
    const auto newSource = makeSource("new.csv");
    cache.put(String("same.csv"), oldSource, 1);

    cache.put(String("same.csv"), newSource, 2);

    EXPECT_EQ(cache.size(), 1);
    EXPECT_EQ(cache.get(String("same.csv")), newSource);
    EXPECT_FALSE(cache.isStale(String("same.csv"), 2));
}

TEST(DataCacheStandaloneTest, ReplacingEntryAtCapacityDoesNotEvictUnrelatedKey)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.setMaxEntries(2);
    const auto unrelated = makeSource("unrelated.csv");
    const auto replacement = makeSource("replacement.csv");
    cache.put(String("unrelated.csv"), unrelated, 1);
    cache.put(String("replace.csv"), makeSource("old-replacement.csv"), 2);

    cache.put(String("replace.csv"), replacement, 3);

    EXPECT_EQ(cache.size(), 2);
    EXPECT_EQ(cache.get(String("unrelated.csv")), unrelated);
    EXPECT_EQ(cache.get(String("replace.csv")), replacement);
    resetCache(cache);
}

TEST(DataCacheStandaloneTest, ReplacingKeyStartsANewTtlWindow)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.setTtlMs(100);
    const auto oldSource = makeSource("old-ttl.csv");
    const auto newSource = makeSource("new-ttl.csv");
    cache.put(String("same-ttl.csv"), oldSource, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(70));

    cache.put(String("same-ttl.csv"), newSource, 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    EXPECT_EQ(cache.get(String("same-ttl.csv")), newSource);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(cache.get(String("same-ttl.csv")), nullptr);
    resetCache(cache);
}

TEST(DataCacheStandaloneTest, MaximumEntryLimitEvictsBeforeAddingNextEntry)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.setMaxEntries(1);
    cache.put(String("first.csv"), makeSource("first.csv"), 1);

    cache.put(String("second.csv"), makeSource("second.csv"), 2);

    EXPECT_EQ(cache.size(), 1);
    EXPECT_EQ(cache.get(String("second.csv")) != nullptr, true);
    EXPECT_TRUE(cache.isStale(String("first.csv"), 1));
    cache.setMaxEntries(64);
}

TEST(DataCacheStandaloneTest, ZeroEntryLimitIsClampedToOne)
{
    auto& cache = DataCache::instance();
    resetCache(cache);

    cache.setMaxEntries(0);
    EXPECT_EQ(cache.maxEntries(), 1);
    cache.put(String("only-entry.csv"), makeSource("only-entry.csv"), 1);
    cache.put(String("replacement-entry.csv"), makeSource("replacement-entry.csv"), 2);

    EXPECT_EQ(cache.size(), 1);
    EXPECT_EQ(cache.get(String("only-entry.csv")), nullptr);
    EXPECT_NE(cache.get(String("replacement-entry.csv")), nullptr);
    resetCache(cache);
}

TEST(DataCacheStandaloneTest, EvictionKeepsMostRecentlyAccessedEntry)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.setMaxEntries(2);
    cache.put(String("older-but-hit.csv"), makeSource("older-but-hit.csv"), 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    cache.put(String("least-recent.csv"), makeSource("least-recent.csv"), 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_NE(cache.get(String("older-but-hit.csv")), nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    cache.put(String("new-entry.csv"), makeSource("new-entry.csv"), 1);

    EXPECT_NE(cache.get(String("older-but-hit.csv")), nullptr);
    EXPECT_EQ(cache.get(String("least-recent.csv")), nullptr);
    EXPECT_NE(cache.get(String("new-entry.csv")), nullptr);
    EXPECT_EQ(cache.size(), 2);
    resetCache(cache);
}

TEST(DataCacheStandaloneTest,
     ShrinkingCapacityEvictsMultipleLeastRecentlyUsedEntriesOnNextPut)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.setMaxEntries(4);
    const auto refreshed = makeSource("refreshed-before-shrink.csv");
    const auto second = makeSource("second-before-shrink.csv");
    const auto third = makeSource("third-before-shrink.csv");
    const auto fourth = makeSource("fourth-before-shrink.csv");
    cache.put(String("refreshed-before-shrink.csv"), refreshed, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    cache.put(String("second-before-shrink.csv"), second, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    cache.put(String("third-before-shrink.csv"), third, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    cache.put(String("fourth-before-shrink.csv"), fourth, 1);
    EXPECT_EQ(cache.get(String("refreshed-before-shrink.csv")), refreshed);

    cache.setMaxEntries(2);
    const auto newest = makeSource("newest-after-shrink.csv");
    cache.put(String("newest-after-shrink.csv"), newest, 1);

    EXPECT_EQ(cache.size(), 2);
    EXPECT_EQ(cache.get(String("refreshed-before-shrink.csv")), refreshed);
    EXPECT_EQ(cache.get(String("newest-after-shrink.csv")), newest);
    EXPECT_EQ(cache.get(String("second-before-shrink.csv")), nullptr);
    EXPECT_EQ(cache.get(String("third-before-shrink.csv")), nullptr);
    EXPECT_EQ(cache.get(String("fourth-before-shrink.csv")), nullptr);
    resetCache(cache);
}

TEST(DataCacheStandaloneTest, MaxEntriesAndTtlConfigurationRoundTrip)
{
    auto& cache = DataCache::instance();
    resetCache(cache);

    cache.setMaxEntries(17);
    cache.setTtlMs(9000);

    EXPECT_EQ(cache.maxEntries(), 17);
    EXPECT_EQ(cache.ttlMs(), 9000);
    cache.setMaxEntries(64);
    cache.setTtlMs(0);
}

TEST(DataCacheStandaloneTest, EntryExpiresAfterConfiguredTtl)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.setTtlMs(1);
    cache.put(String("expires.csv"), makeSource("expires.csv"), 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_EQ(cache.get(String("expires.csv")), nullptr);
    EXPECT_EQ(cache.size(), 0);
    cache.setTtlMs(0);
}

TEST(DataCacheStandaloneTest, CacheHitDoesNotExtendLoadedEntryTtl)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    cache.setTtlMs(50);
    const auto source = makeSource("fixed-ttl.csv");
    cache.put(String("fixed-ttl.csv"), source, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(cache.get(String("fixed-ttl.csv")), source);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    EXPECT_EQ(cache.get(String("fixed-ttl.csv")), nullptr);
    cache.setTtlMs(0);
}

TEST(DataCacheStandaloneTest, ConcurrentIndependentUpdatesRemainReadable)
{
    auto& cache = DataCache::instance();
    resetCache(cache);
    constexpr int workerCount = 8;
    constexpr int updatesPerWorker = 200;
    cache.setMaxEntries(workerCount * 2);

    std::thread workers[workerCount];
    for (int worker = 0; worker < workerCount; ++worker) {
        workers[worker] = std::thread([worker, &cache] {
            const std::string key = "parallel-" + std::to_string(worker) + ".csv";
            const String uri(key.c_str());
            const auto source = makeCsvDataSource(uri);

            for (int update = 0; update < updatesPerWorker; ++update) {
                cache.put(uri, source, update);
                EXPECT_EQ(cache.get(uri), source);
                if (update % 17 == 0) {
                    cache.invalidate(uri);
                }
            }
            cache.put(uri, source, updatesPerWorker);
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(cache.size(), workerCount);
    for (int worker = 0; worker < workerCount; ++worker) {
        const std::string key = "parallel-" + std::to_string(worker) + ".csv";
        EXPECT_NE(cache.get(String(key.c_str())), nullptr);
    }
    resetCache(cache);
}
