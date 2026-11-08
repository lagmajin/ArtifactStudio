#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

#include <QString>
#include <QStringList>
#include <QObject>

import Artifact.Render.FrameCache;
import Frame.Position;
import Memory.SharedPtr;
import Render.Quality;

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

TEST(FrameCacheContractTest,
     ReplacingFrameUpdatesMemoryAccountingWithoutDuplicatingTheKey)
{
    Artifact::FrameCache cache;
    cache.setMaxFrameCount(2);
    cache.setMaxMemoryBytes(48);
    const auto replacedFrame = ArtifactCore::FramePosition(1);
    auto original = makeEntry(1);
    auto unaffected = makeEntry(2);
    cache.put(original);
    cache.put(unaffected);

    auto replacement = ArtifactCore::makeShared<Artifact::FrameCacheEntry>(
        2, 1, replacedFrame);
    ASSERT_EQ(original->memorySize, 16u);
    ASSERT_EQ(replacement->memorySize, 32u);
    cache.put(replacement);

    EXPECT_EQ(cache.currentFrameCount(), 2);
    EXPECT_EQ(cache.currentMemoryUsage(), 48u);
    EXPECT_EQ(cache.get(replacedFrame).get(), replacement.get());
    EXPECT_EQ(cache.get(ArtifactCore::FramePosition(2)).get(), unaffected.get());
}

TEST(FrameCacheContractTest, LoweringConfiguredLimitsEvictsExistingOldestFrames)
{
    Artifact::FrameCache cache;
    cache.setPolicy(Artifact::CachePolicy::FIFO);
    cache.put(makeEntry(1));
    cache.put(makeEntry(2));
    cache.put(makeEntry(3));
    ASSERT_EQ(cache.currentFrameCount(), 3);
    ASSERT_EQ(cache.currentMemoryUsage(), 48u);

    cache.setMaxMemoryBytes(32);

    EXPECT_EQ(cache.currentFrameCount(), 2);
    EXPECT_EQ(cache.currentMemoryUsage(), 32u);
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(1)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(2)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(3)));

    cache.setMaxFrameCount(1);

    EXPECT_EQ(cache.currentFrameCount(), 1);
    EXPECT_EQ(cache.currentMemoryUsage(), 16u);
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(2)));
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

TEST(FrameCacheContractTest,
     LfuEvictionIgnoresStaleFrequencyCandidatesAfterKeyReuse)
{
    Artifact::FrameCache cache;
    cache.setMaxFrameCount(3);
    cache.setPolicy(Artifact::CachePolicy::LFU);
    const auto removedFrame = ArtifactCore::FramePosition(20);
    cache.put(makeEntry(100));
    cache.put(makeEntry(10));
    cache.put(makeEntry(20));
    for (int access = 0; access < 3; ++access)
        ASSERT_TRUE(cache.get(removedFrame));
    ASSERT_EQ(cache.cacheHitCount(), 3u);

    cache.invalidate(removedFrame);
    auto reinserted = makeEntry(20);
    cache.put(reinserted);
    for (int access = 0; access < 4; ++access)
        ASSERT_TRUE(cache.get(removedFrame));
    ASSERT_EQ(cache.cacheHitCount(), 7u);

    cache.put(makeEntry(200));

    EXPECT_EQ(cache.currentFrameCount(), 3);
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(10)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(20)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(100)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(200)));
    EXPECT_EQ(cache.get(removedFrame).get(), reinserted.get());
}

TEST(FrameCacheContractTest,
     ConcurrentUniqueFrameInsertionAndLookupRemainConsistent)
{
    constexpr int workerCount = 6;
    constexpr int framesPerWorker = 128;
    constexpr int totalFrames = workerCount * framesPerWorker;
    Artifact::FrameCache cache;
    cache.setMaxFrameCount(totalFrames);
    cache.setMaxMemoryBytes(static_cast<std::size_t>(totalFrames) * 16u);

    std::atomic<int> ready{0};
    std::atomic<bool> start{false};
    std::atomic<int> missingOrMismatchedFrames{0};
    std::thread workers[workerCount];
    for (int worker = 0; worker < workerCount; ++worker) {
        workers[worker] = std::thread([&, worker] {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int localFrame = 0; localFrame < framesPerWorker; ++localFrame) {
                const int frameNumber = worker * framesPerWorker + localFrame;
                auto entry = makeEntry(frameNumber);
                cache.put(entry);
                const auto retrieved = cache.get(entry->frame);
                if (retrieved.get() != entry.get()) {
                    missingOrMismatchedFrames.fetch_add(
                        1, std::memory_order_relaxed);
                }
            }
        });
    }

    while (ready.load(std::memory_order_acquire) != workerCount) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);
    for (auto& worker : workers) worker.join();

    EXPECT_EQ(missingOrMismatchedFrames.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(cache.currentFrameCount(), totalFrames);
    EXPECT_EQ(cache.currentMemoryUsage(),
              static_cast<std::size_t>(totalFrames) * 16u);
    EXPECT_EQ(cache.cacheHitCount(), static_cast<std::size_t>(totalFrames));
    EXPECT_EQ(cache.cacheMissCount(), 0u);
    EXPECT_FLOAT_EQ(cache.hitRate(), 1.0f);
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

TEST(FrameCacheContractTest, ZeroFrameCapacityRejectsStoreWithoutMutation)
{
    Artifact::FrameCache cache;
    cache.setMaxFrameCount(0);

    EXPECT_THROW(cache.put(makeEntry(1)), std::invalid_argument);
    EXPECT_EQ(cache.currentFrameCount(), 0);
    EXPECT_EQ(cache.currentMemoryUsage(), 0u);
    EXPECT_EQ(cache.maxFrameCount(), 0);
}

TEST(FrameCacheContractTest, TrimToSizeEvictsOldestEntriesUntilWithinByteTarget)
{
    Artifact::FrameCache cache;
    cache.setPolicy(Artifact::CachePolicy::FIFO);
    cache.put(makeEntry(1));
    cache.put(makeEntry(2));
    cache.put(makeEntry(3));
    ASSERT_EQ(cache.currentMemoryUsage(), 48u);

    cache.trimToSize(16u);

    EXPECT_EQ(cache.currentMemoryUsage(), 16u);
    EXPECT_EQ(cache.currentFrameCount(), 1);
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(1)));
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(2)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(3)));
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

TEST(FrameCacheContractTest,
     CompositionRangeInvalidationWithoutDependencyIdsRemovesAllMatchingFrames)
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
    add(40, QStringLiteral("comp-a"), QStringLiteral("layer-a"), QStringLiteral("fx-a"));
    add(41, QStringLiteral("comp-a"), QStringLiteral("layer-b"), QStringLiteral("fx-b"));
    add(42, QStringLiteral("comp-a"), QStringLiteral("layer-c"), QStringLiteral("fx-c"));
    add(43, QStringLiteral("comp-b"), QStringLiteral("layer-a"), QStringLiteral("fx-a"));

    Artifact::FrameCacheInvalidation invalidation;
    invalidation.compositionId = QStringLiteral("comp-a");
    invalidation.range = {40, 41};
    cache.invalidate(invalidation);

    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(40)));
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(41)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(42)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(43)));
    EXPECT_EQ(cache.currentFrameCount(), 2);
}

TEST(FrameCacheContractTest, TargetedInvalidationOrMatchesIdsWithinEachDependencyKind)
{
    Artifact::FrameCache cache;
    const auto add = [&cache](int frame, const QString& layer,
                              const QString& effect) {
        auto entry = makeEntry(frame);
        entry->compositionId = QStringLiteral("comp-a");
        entry->layerIds = QStringList{layer};
        entry->effectIds = QStringList{effect};
        cache.put(entry);
    };
    add(30, QStringLiteral("layer-a"), QStringLiteral("fx-a"));
    add(31, QStringLiteral("layer-b"), QStringLiteral("fx-b"));
    add(32, QStringLiteral("layer-a"), QStringLiteral("fx-other"));
    add(33, QStringLiteral("layer-other"), QStringLiteral("fx-b"));

    Artifact::FrameCacheInvalidation invalidation;
    invalidation.compositionId = QStringLiteral("comp-a");
    invalidation.range = {30, 33};
    invalidation.layerIds = QStringList{
        QStringLiteral("layer-a"), QStringLiteral("layer-b")};
    invalidation.effectIds = QStringList{
        QStringLiteral("fx-a"), QStringLiteral("fx-b")};
    cache.invalidate(invalidation);

    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(30)));
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(31)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(32)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(33)));
    EXPECT_EQ(cache.currentFrameCount(), 2);
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

TEST(FrameCacheContractTest,
     TargetedInvalidationDoesNotNarrowLargeFramePositionsIntoRange)
{
    Artifact::FrameCache cache;
    const auto aliasedFrameNumber = (std::int64_t{1} << 32) + 10;
    auto aliasedFrame = ArtifactCore::makeShared<Artifact::FrameCacheEntry>(
        1, 1, ArtifactCore::FramePosition(aliasedFrameNumber));
    aliasedFrame->compositionId = QStringLiteral("comp-wide-frame");
    cache.put(aliasedFrame);

    Artifact::FrameCacheInvalidation invalidation;
    invalidation.compositionId = QStringLiteral("comp-wide-frame");
    invalidation.range = {10, 10};
    cache.invalidate(invalidation);

    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(aliasedFrameNumber)))
        << "a frame outside FrameRange's int domain must not alias frame 10";
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

TEST(FrameCacheContractTest,
     ReplacingStaleFrameMakesNewGenerationVisibleAndRetainable)
{
    Artifact::FrameCache cache;
    const auto frame = ArtifactCore::FramePosition(12);
    auto stale = makeEntry(12);
    cache.put(stale);

    const auto currentGeneration = cache.bumpGeneration(
        QStringLiteral("device-recreated"));
    EXPECT_EQ(stale->generation, currentGeneration - 1u);
    EXPECT_EQ(cache.get(frame).get(), nullptr);
    ASSERT_EQ(cache.currentFrameCount(), 1);

    auto refreshed = makeEntry(12);
    cache.put(refreshed);

    EXPECT_EQ(refreshed->generation, currentGeneration);
    EXPECT_EQ(cache.currentFrameCount(), 1);
    EXPECT_EQ(cache.currentMemoryUsage(), refreshed->memorySize);
    EXPECT_EQ(cache.get(frame).get(), refreshed.get());
    cache.invalidateStaleGenerations(currentGeneration);
    EXPECT_EQ(cache.get(frame).get(), refreshed.get());
    EXPECT_EQ(cache.currentFrameCount(), 1);
}

TEST(FrameCacheContractTest,
     SizeEvictionSkipsStalePriorityCandidateAfterGenerationReplacement)
{
    Artifact::FrameCache cache;
    cache.setPolicy(Artifact::CachePolicy::Size);
    cache.setMaxFrameCount(2);
    const auto replacedFrame = ArtifactCore::FramePosition(20);
    auto staleSmall = makeEntry(20);
    auto smallerLive = ArtifactCore::makeShared<Artifact::FrameCacheEntry>(
        2, 1, ArtifactCore::FramePosition(21));
    cache.put(staleSmall);
    cache.put(smallerLive);

    cache.bumpGeneration(QStringLiteral("device-recreated"));
    auto refreshedLarge = ArtifactCore::makeShared<Artifact::FrameCacheEntry>(
        3, 1, replacedFrame);
    cache.put(refreshedLarge);
    ASSERT_EQ(refreshedLarge->memorySize, 48u);
    ASSERT_EQ(cache.currentFrameCount(), 2);

    cache.put(makeEntry(22));

    EXPECT_EQ(cache.currentFrameCount(), 2);
    EXPECT_EQ(cache.currentMemoryUsage(), 64u);
    EXPECT_FALSE(cache.contains(ArtifactCore::FramePosition(21)));
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(22)));
    EXPECT_EQ(cache.get(replacedFrame).get(), refreshedLarge.get());

    cache.invalidateStaleGenerations(cache.generation());
    EXPECT_EQ(cache.currentFrameCount(), 2);
    EXPECT_EQ(cache.currentMemoryUsage(), 64u);
    EXPECT_EQ(cache.get(replacedFrame).get(), refreshedLarge.get());
    EXPECT_TRUE(cache.contains(ArtifactCore::FramePosition(22)));
}

TEST(FrameCacheContractTest, ProgressiveRendererUpgradesThroughRequestedQualities)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Preview);

    std::vector<Artifact::RenderQuality> rendered;
    renderer.setRenderCallback([&rendered](Artifact::RenderQuality quality) {
        rendered.push_back(quality);
        return true;
    });

    renderer.requestUpgrade();

    ASSERT_EQ(rendered.size(), 1u);
    EXPECT_EQ(rendered[0], Artifact::RenderQuality::Preview);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Preview);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest,
     ProgressiveRendererSkipsProgressivePassesWhenFinalQualityIsRequested)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Final);

    int renderCount = 0;
    renderer.setRenderCallback([&renderCount](Artifact::RenderQuality) {
        ++renderCount;
        return true;
    });

    renderer.requestUpgrade();

    EXPECT_EQ(renderCount, 0);
    EXPECT_EQ(renderer.quality(), Artifact::RenderQuality::Final);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Draft);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 0.0f);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest, ProgressiveRendererIncludesCustomAsFinalRequestedStage)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Custom);

    std::vector<Artifact::RenderQuality> rendered;
    renderer.setRenderCallback([&rendered](Artifact::RenderQuality quality) {
        rendered.push_back(quality);
        return true;
    });

    renderer.requestUpgrade();

    ASSERT_EQ(rendered.size(), 3u);
    EXPECT_EQ(rendered[0], Artifact::RenderQuality::Preview);
    EXPECT_EQ(rendered[1], Artifact::RenderQuality::Final);
    EXPECT_EQ(rendered[2], Artifact::RenderQuality::Custom);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Custom);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest, ProgressiveRendererWithoutCallbackKeepsCurrentQuality)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Custom);

    renderer.requestUpgrade();

    EXPECT_EQ(renderer.quality(), Artifact::RenderQuality::Custom);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Draft);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 0.0f);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest, ProgressiveRendererDoesNotRepeatCompletedCustomPasses)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Custom);
    int renderCount = 0;
    renderer.setRenderCallback([&renderCount](Artifact::RenderQuality) {
        ++renderCount;
        return true;
    });

    renderer.requestUpgrade();
    ASSERT_EQ(renderCount, 3);
    ASSERT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Custom);
    renderer.requestUpgrade();

    EXPECT_EQ(renderCount, 3);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Custom);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest, ForceFinalRenderAdvancesQualityWithoutRenderCallback)
{
    Artifact::ProgressiveRenderer renderer;
    int renderCount = 0;
    renderer.setRenderCallback([&renderCount](Artifact::RenderQuality) {
        ++renderCount;
        return true;
    });

    renderer.forceFinalRender();

    EXPECT_EQ(renderCount, 0);
    EXPECT_EQ(renderer.quality(), Artifact::RenderQuality::Final);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Final);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest, ProgressiveRendererStopsWhenAQualityPassFails)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Custom);

    int renderCount = 0;
    renderer.setRenderCallback([&renderCount](Artifact::RenderQuality) {
        ++renderCount;
        return false;
    });

    renderer.requestUpgrade();

    EXPECT_EQ(renderCount, 1);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Draft);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest, ProgressiveRendererRetriesOnlyFailedAndLaterQualityPasses)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Custom);

    std::vector<Artifact::RenderQuality> attempted;
    bool failFinalOnce = true;
    renderer.setRenderCallback([&](Artifact::RenderQuality quality) {
        attempted.push_back(quality);
        if (quality == Artifact::RenderQuality::Final && failFinalOnce) {
            failFinalOnce = false;
            return false;
        }
        return true;
    });

    renderer.requestUpgrade();
    ASSERT_EQ(attempted.size(), 2u);
    EXPECT_EQ(attempted[0], Artifact::RenderQuality::Preview);
    EXPECT_EQ(attempted[1], Artifact::RenderQuality::Final);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Preview);
    EXPECT_FALSE(renderer.isUpgrading());

    renderer.requestUpgrade();
    ASSERT_EQ(attempted.size(), 4u);
    EXPECT_EQ(attempted[2], Artifact::RenderQuality::Final);
    EXPECT_EQ(attempted[3], Artifact::RenderQuality::Custom);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Custom);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest, ProgressiveRendererCanResumeAfterCallbackCancelsUpgrade)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Custom);

    std::vector<Artifact::RenderQuality> rendered;
    renderer.setRenderCallback([&renderer, &rendered](Artifact::RenderQuality quality) {
        rendered.push_back(quality);
        if (quality == Artifact::RenderQuality::Preview) {
            renderer.cancelUpgrade();
        }
        return true;
    });

    renderer.requestUpgrade();
    ASSERT_EQ(rendered.size(), 1u);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Preview);
    EXPECT_FALSE(renderer.isUpgrading());

    renderer.requestUpgrade();
    ASSERT_EQ(rendered.size(), 3u);
    EXPECT_EQ(rendered[1], Artifact::RenderQuality::Final);
    EXPECT_EQ(rendered[2], Artifact::RenderQuality::Custom);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Custom);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f);
}

TEST(FrameCacheContractTest,
     ProgressiveRendererCompletesWhenRequestedQualityIsAlreadyRendered)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Custom);

    std::vector<Artifact::RenderQuality> rendered;
    renderer.setRenderCallback([&renderer, &rendered](Artifact::RenderQuality quality) {
        rendered.push_back(quality);
        if (quality == Artifact::RenderQuality::Preview) {
            renderer.cancelUpgrade();
        }
        return true;
    });

    renderer.requestUpgrade();
    ASSERT_EQ(rendered.size(), 1u);
    ASSERT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Preview);
    ASSERT_FLOAT_EQ(renderer.currentProgress(), 1.0f / 3.0f);

    renderer.setQuality(Artifact::RenderQuality::Preview);
    renderer.requestUpgrade();

    EXPECT_EQ(rendered.size(), 1u)
        << "the already rendered requested quality should not be rendered again";
    EXPECT_EQ(renderer.quality(), Artifact::RenderQuality::Preview);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Preview);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f)
        << "a request already satisfied by the current frame should be complete";
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest,
     ProgressiveRendererTreatsDraftAsAlreadySatisfiedWithoutRendering)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Draft);

    int renderCount = 0;
    renderer.setRenderCallback([&renderCount](Artifact::RenderQuality) {
        ++renderCount;
        return true;
    });

    renderer.requestUpgrade();

    EXPECT_EQ(renderCount, 0);
    EXPECT_EQ(renderer.quality(), Artifact::RenderQuality::Draft);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Draft);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f)
        << "the initial Draft output already satisfies a Draft request";
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest,
     ProgressiveRendererCompletesWhenCurrentOutputExceedsRequestedQuality)
{
    Artifact::ProgressiveRenderer renderer;
    renderer.setQuality(Artifact::RenderQuality::Custom);

    int renderCount = 0;
    renderer.setRenderCallback([&renderCount](Artifact::RenderQuality) {
        ++renderCount;
        return true;
    });

    renderer.requestUpgrade();
    ASSERT_EQ(renderCount, 3);
    ASSERT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Custom);
    ASSERT_FLOAT_EQ(renderer.currentProgress(), 1.0f);

    renderer.setQuality(Artifact::RenderQuality::Preview);
    renderer.requestUpgrade();

    EXPECT_EQ(renderCount, 3)
        << "an already available higher-quality image should satisfy a lower request";
    EXPECT_EQ(renderer.quality(), Artifact::RenderQuality::Preview);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Custom);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f);
    EXPECT_FALSE(renderer.isUpgrading());

    renderer.setQuality(Artifact::RenderQuality::Custom);
    renderer.requestUpgrade();

    EXPECT_EQ(renderCount, 3)
        << "returning to the already rendered quality should not repeat passes";
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Custom);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest, ProgressiveRendererClampsInvalidQualityEnumValues)
{
    Artifact::ProgressiveRenderer renderer;
    std::vector<Artifact::RenderQuality> rendered;
    renderer.setRenderCallback([&rendered](Artifact::RenderQuality quality) {
        rendered.push_back(quality);
        return true;
    });

    renderer.setQuality(static_cast<Artifact::RenderQuality>(-1));
    EXPECT_EQ(renderer.quality(), Artifact::RenderQuality::Draft);
    renderer.requestUpgrade();
    EXPECT_TRUE(rendered.empty())
        << "clamping to Draft should not schedule a higher quality pass";
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Draft);

    renderer.setQuality(static_cast<Artifact::RenderQuality>(100));
    EXPECT_EQ(renderer.quality(), Artifact::RenderQuality::Custom);
    renderer.requestUpgrade();
    ASSERT_EQ(rendered.size(), 3u);
    EXPECT_EQ(rendered[0], Artifact::RenderQuality::Preview);
    EXPECT_EQ(rendered[1], Artifact::RenderQuality::Final);
    EXPECT_EQ(rendered[2], Artifact::RenderQuality::Custom);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Custom);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest,
     ProgressiveRendererNotifiesQualityTransitionsAndForcedFinal)
{
    Artifact::ProgressiveRenderer renderer;
    std::vector<Artifact::RenderQuality> notifications;
    QObject::connect(&renderer, &Artifact::ProgressiveRenderer::qualityChanged,
                     [&notifications](Artifact::RenderQuality quality) {
                         notifications.push_back(quality);
                     });

    renderer.setQuality(Artifact::RenderQuality::Draft);
    renderer.setQuality(Artifact::RenderQuality::Preview);
    renderer.setQuality(Artifact::RenderQuality::Custom);
    renderer.forceFinalRender();

    ASSERT_EQ(notifications.size(), 4u);
    EXPECT_EQ(notifications[0], Artifact::RenderQuality::Draft);
    EXPECT_EQ(notifications[1], Artifact::RenderQuality::Preview);
    EXPECT_EQ(notifications[2], Artifact::RenderQuality::Custom);
    EXPECT_EQ(notifications[3], Artifact::RenderQuality::Final);
    EXPECT_EQ(renderer.quality(), Artifact::RenderQuality::Final);
    EXPECT_EQ(renderer.currentRenderedQuality(), Artifact::RenderQuality::Final);
    EXPECT_FLOAT_EQ(renderer.currentProgress(), 1.0f);
    EXPECT_FALSE(renderer.isUpgrading());
}

TEST(FrameCacheContractTest, PerformanceMonitorTracksQualityCountsAndFrameTimeStatistics)
{
    Artifact::RenderPerformanceMonitor monitor;

    monitor.recordFrameRender(10.0, Artifact::RenderQuality::Draft);
    monitor.recordFrameRender(20.0, Artifact::RenderQuality::Preview);
    monitor.recordFrameRender(30.0, Artifact::RenderQuality::Final);

    const auto metrics = monitor.getMetrics();
    EXPECT_EQ(metrics.framesRendered, 3);
    EXPECT_EQ(metrics.draftFrames, 1);
    EXPECT_EQ(metrics.previewFrames, 1);
    EXPECT_EQ(metrics.finalFrames, 1);
    EXPECT_DOUBLE_EQ(metrics.lastFrameTime, 30.0);
    EXPECT_DOUBLE_EQ(metrics.averageFrameTime, 20.0);
    EXPECT_DOUBLE_EQ(metrics.minFrameTime, 10.0);
    EXPECT_DOUBLE_EQ(metrics.maxFrameTime, 30.0);
    EXPECT_DOUBLE_EQ(monitor.averageFPS(), 50.0);
    EXPECT_TRUE(monitor.isPerformanceAcceptable());
}

TEST(FrameCacheContractTest, PerformanceMonitorIgnoresInvalidFrameTimesWithoutChangingQualityCounts)
{
    Artifact::RenderPerformanceMonitor monitor;
    monitor.recordFrameRender(12.0, Artifact::RenderQuality::Preview);
    monitor.recordFrameRender(-1.0, Artifact::RenderQuality::Final);
    monitor.recordFrameRender(std::numeric_limits<double>::infinity(), Artifact::RenderQuality::Draft);
    monitor.recordFrameRender(std::numeric_limits<double>::quiet_NaN(), Artifact::RenderQuality::Draft);

    const auto metrics = monitor.getMetrics();
    EXPECT_EQ(metrics.framesRendered, 1);
    EXPECT_EQ(metrics.previewFrames, 1);
    EXPECT_EQ(metrics.finalFrames, 0);
    EXPECT_EQ(metrics.draftFrames, 0);
    EXPECT_DOUBLE_EQ(metrics.lastFrameTime, 12.0);
    EXPECT_DOUBLE_EQ(metrics.averageFrameTime, 12.0);
}

TEST(FrameCacheContractTest, PerformanceMonitorResetClearsMeasurementsAndDropsButKeepsConfiguredBudget)
{
    Artifact::RenderPerformanceMonitor monitor;
    monitor.setTargetFPS(60.0);
    monitor.recordFrameRender(40.0, Artifact::RenderQuality::Final);
    monitor.recordFrameDrop();
    monitor.recordGPUAlloc(1024);
    monitor.recordSystemAlloc(2048);

    const auto populatedMetrics = monitor.getMetrics();
    EXPECT_EQ(populatedMetrics.framesDropped, 1);
    EXPECT_EQ(populatedMetrics.gpuMemoryUsed, 1024u);
    EXPECT_EQ(populatedMetrics.systemMemoryUsed, 2048u);

    monitor.reset();

    const auto metrics = monitor.getMetrics();
    EXPECT_EQ(metrics.framesRendered, 0);
    EXPECT_EQ(metrics.framesDropped, 0);
    EXPECT_EQ(metrics.finalFrames, 0);
    EXPECT_EQ(metrics.gpuMemoryUsed, 0u);
    EXPECT_EQ(metrics.systemMemoryUsed, 0u);
    EXPECT_DOUBLE_EQ(metrics.averageFrameTime, 0.0);
    EXPECT_DOUBLE_EQ(monitor.currentFPS(), 0.0);
    EXPECT_DOUBLE_EQ(monitor.averageFPS(), 0.0);
    EXPECT_FALSE(monitor.isPerformanceAcceptable());
    EXPECT_DOUBLE_EQ(monitor.targetFPS(), 60.0);
    EXPECT_NEAR(monitor.frameTimeBudget(), 1000.0 / 60.0, 1e-12);
}

TEST(FrameCacheContractTest, PerformanceMonitorRejectsInvalidBudgetsAndAcceptsZeroFrameBudget)
{
    Artifact::RenderPerformanceMonitor monitor;
    monitor.setTargetFPS(0.0);
    monitor.setTargetFPS(-30.0);
    monitor.setTargetFPS(std::numeric_limits<double>::infinity());
    monitor.setFrameTimeBudget(-1.0);
    monitor.setFrameTimeBudget(std::numeric_limits<double>::quiet_NaN());

    EXPECT_DOUBLE_EQ(monitor.targetFPS(), 30.0);
    EXPECT_DOUBLE_EQ(monitor.frameTimeBudget(), 33.33);

    monitor.setFrameTimeBudget(0.0);
    EXPECT_DOUBLE_EQ(monitor.frameTimeBudget(), 0.0);
    EXPECT_FALSE(monitor.isPerformanceAcceptable());
    monitor.recordFrameRender(0.0, Artifact::RenderQuality::Draft);
    EXPECT_TRUE(monitor.isPerformanceAcceptable());
}

TEST(FrameCacheContractTest, PerformanceMonitorKeepsOnlyTheLatestThreeHundredFrameTimes)
{
    Artifact::RenderPerformanceMonitor monitor;
    for (int frame = 0; frame < 300; ++frame) {
        monitor.recordFrameRender(1.0, Artifact::RenderQuality::Preview);
    }
    monitor.recordFrameRender(301.0, Artifact::RenderQuality::Final);

    const auto metrics = monitor.getMetrics();
    EXPECT_EQ(metrics.framesRendered, 301);
    EXPECT_EQ(metrics.previewFrames, 300);
    EXPECT_EQ(metrics.finalFrames, 1);
    EXPECT_DOUBLE_EQ(metrics.lastFrameTime, 301.0);
    EXPECT_DOUBLE_EQ(metrics.averageFrameTime, 2.0);
    EXPECT_DOUBLE_EQ(metrics.minFrameTime, 1.0);
    EXPECT_DOUBLE_EQ(metrics.maxFrameTime, 301.0);
}

TEST(FrameCacheContractTest, CustomQualityContributesToTotalFramesWithoutEnteringNamedBuckets)
{
    Artifact::RenderPerformanceMonitor monitor;

    monitor.recordFrameRender(8.0, Artifact::RenderQuality::Custom);

    const auto metrics = monitor.getMetrics();
    EXPECT_EQ(metrics.framesRendered, 1);
    EXPECT_EQ(metrics.draftFrames, 0);
    EXPECT_EQ(metrics.previewFrames, 0);
    EXPECT_EQ(metrics.finalFrames, 0);
    EXPECT_DOUBLE_EQ(metrics.lastFrameTime, 8.0);
}

TEST(FrameCacheContractTest, PerformanceMonitorAcceptabilityUsesTheRollingAverageAtBudgetBoundary)
{
    Artifact::RenderPerformanceMonitor monitor;
    monitor.setFrameTimeBudget(20.0);
    monitor.recordFrameRender(20.0, Artifact::RenderQuality::Preview);
    EXPECT_TRUE(monitor.isPerformanceAcceptable());

    monitor.recordFrameRender(22.0, Artifact::RenderQuality::Preview);
    EXPECT_FALSE(monitor.isPerformanceAcceptable());

    monitor.recordFrameRender(18.0, Artifact::RenderQuality::Preview);
    EXPECT_TRUE(monitor.isPerformanceAcceptable());
}
