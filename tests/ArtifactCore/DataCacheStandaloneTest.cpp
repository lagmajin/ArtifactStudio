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
