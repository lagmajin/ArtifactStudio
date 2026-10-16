#include <gtest/gtest.h>

#include <stdexcept>

#include <QString>
#include <QStringList>

import Artifact.Render.FrameCache;
import Frame.Position;
import Memory.SharedPtr;

namespace {

ArtifactCore::SharedPtr<Artifact::FrameCacheEntry> makeEntry(int frame)
{
    return ArtifactCore::makeShared<Artifact::FrameCacheEntry>(
        1, 1, ArtifactCore::FramePosition(frame));
}

} // namespace

TEST(FrameCacheContractTest, FrameCountLimitEvictsOldestFrame)
{
    Artifact::FrameCache cache;
    cache.setMaxFrameCount(2);
    cache.put(makeEntry(1));
    cache.put(makeEntry(2));
    cache.put(makeEntry(3));

    EXPECT_EQ(cache.currentFrameCount(), 2);
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(1)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(2)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(3)));
}

TEST(FrameCacheContractTest, MemoryLimitEvictsUntilTheNewEntryFits)
{
    Artifact::FrameCache cache;
    cache.setMaxMemoryBytes(32);
    cache.put(makeEntry(1));
    cache.put(makeEntry(2));
    cache.put(makeEntry(3));

    EXPECT_LE(cache.currentMemoryUsage(), cache.maxMemoryBytes());
    EXPECT_EQ(cache.currentFrameCount(), 2);
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(1)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(3)));
}

TEST(FrameCacheContractTest, LruPolicyEvictsLeastRecentlyAccessedFrame)
{
    Artifact::FrameCache cache;
    cache.setMaxFrameCount(2);
    cache.setPolicy(Artifact::CachePolicy::LRU);
    cache.put(makeEntry(1));
    cache.put(makeEntry(2));
    ASSERT_TRUE(cache.get(ArtifactCore::FramePosition(1)));

    cache.put(makeEntry(3));

    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(1)));
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(2)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(3)));
}

TEST(FrameCacheContractTest, RejectsEntryLargerThanConfiguredMemoryBudget)
{
    Artifact::FrameCache cache;
    cache.setMaxMemoryBytes(16);
    auto oversized = ArtifactCore::makeShared<Artifact::FrameCacheEntry>(
        2, 1, ArtifactCore::FramePosition(1));

    EXPECT_THROW(cache.put(oversized), std::invalid_argument);
    EXPECT_EQ(cache.currentFrameCount(), 0);
    EXPECT_EQ(cache.currentMemoryUsage(), 0u);
}

TEST(FrameCacheContractTest, TargetedInvalidationHonorsCompositionRangeLayerAndEffect)
{
    Artifact::FrameCache cache;
    const auto add = [&cache](int frame, const QString& composition,
                              const QString& layer, const QString& effect) {
        auto entry = makeEntry(frame);
        entry->compositionId = composition;
        entry->layerIds = QStringList{layer};
        entry->effectIds = QStringList{effect};
        cache.put(entry);
    };
    add(10, QStringLiteral("comp-a"), QStringLiteral("layer-a"), QStringLiteral("fx-a"));
    add(11, QStringLiteral("comp-a"), QStringLiteral("layer-b"), QStringLiteral("fx-a"));
    add(20, QStringLiteral("comp-b"), QStringLiteral("layer-a"), QStringLiteral("fx-a"));
    add(12, QStringLiteral("comp-a"), QStringLiteral("layer-a"), QStringLiteral("fx-a"));

    Artifact::FrameCacheInvalidation invalidation;
    invalidation.compositionId = QStringLiteral("comp-a");
    invalidation.range = {10, 11};
    invalidation.layerIds = QStringList{QStringLiteral("layer-a")};
    invalidation.effectIds = QStringList{QStringLiteral("fx-a")};
    cache.invalidate(invalidation);

    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(10)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(11)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(12)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(20)));
    EXPECT_EQ(cache.currentFrameCount(), 3);
}

TEST(FrameCacheContractTest, FullCompositionInvalidationIgnoresFrameRange)
{
    Artifact::FrameCache cache;
    auto first = makeEntry(1);
    first->compositionId = QStringLiteral("comp-a");
    auto later = makeEntry(100);
    later->compositionId = QStringLiteral("comp-a");
    auto other = makeEntry(101);
    other->compositionId = QStringLiteral("comp-b");
    cache.put(first);
    cache.put(later);
    cache.put(other);

    Artifact::FrameCacheInvalidation invalidation;
    invalidation.compositionId = QStringLiteral("comp-a");
    invalidation.range = {1, 1};
    invalidation.fullComposition = true;
    cache.invalidate(invalidation);

    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(1)));
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(100)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(101)));
    EXPECT_EQ(cache.currentFrameCount(), 1);
}

TEST(FrameCacheContractTest, GenerationBumpHidesOldEntriesUntilStaleEntriesAreRemoved)
{
    Artifact::FrameCache cache;
    cache.put(makeEntry(5));
    ASSERT_TRUE(cache.contains(ArtifactCore::FramePosition(5)));
    const auto oldGeneration = cache.generation();

    EXPECT_EQ(cache.bumpGeneration(QStringLiteral("device-recreated")),
              oldGeneration + 1u);
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(5)));
    EXPECT_EQ(cache.currentFrameCount(), 1);

    cache.invalidateStaleGenerations(cache.generation());
    EXPECT_EQ(cache.currentFrameCount(), 0);
    EXPECT_EQ(cache.currentMemoryUsage(), 0u);
}

} // namespace
