#include <gtest/gtest.h>

import Graphics.RenderIndex;
import Graphics.RenderPipelineFoundation;
import Graphics.SurfaceColorContract;

using namespace ArtifactCore;

namespace {

RenderInputSnapshot makeSnapshot()
{
    RenderInputSnapshot snapshot;
    snapshot.compositionId = 101;
    snapshot.viewId = 7;
    snapshot.sceneRevision = 4;
    snapshot.renderIndexGeneration = 12;
    snapshot.settingsRevision = 3;
    snapshot.frameIndex = 48;
    snapshot.frameRateNumerator = 30000;
    snapshot.frameRateDenominator = 1001;
    snapshot.width = 1920;
    snapshot.height = 1080;
    snapshot.requestedBackend = RenderBackendKind::DiligentGPU;
    snapshot.requiresCompute = true;
    snapshot.requiresHDR = true;
    snapshot.usesTemporalHistory = true;
    snapshot.color = SurfaceColorDescriptor::canonicalLinearPremultiplied(
        SurfaceColorPrimaries::Rec2020_D65);
    return snapshot;
}

RenderBackendCapabilities gpuCapabilities()
{
    return {RenderBackendKind::DiligentGPU,
            RenderCapability::Raster | RenderCapability::Compute |
                RenderCapability::Float32Targets | RenderCapability::TemporalHistory,
            8192};
}

RenderBackendCapabilities softwareCapabilities()
{
    return {RenderBackendKind::Software,
            RenderCapability::Raster | RenderCapability::Compute |
                RenderCapability::Float32Targets | RenderCapability::HDR |
                RenderCapability::TemporalHistory,
            4096};
}

} // namespace

TEST(RenderPipelineContractTest, FallsBackWhenRequestedBackendLacksRequiredCapability)
{
    const auto snapshot = makeSnapshot();
    const auto gpu = gpuCapabilities();
    const auto software = softwareCapabilities();

    ASSERT_TRUE(snapshot.isValid());
    EXPECT_EQ(gpu.failureReason(snapshot), RenderFallbackReason::MissingHDR);
    EXPECT_FALSE(gpu.canRender(snapshot));
    EXPECT_TRUE(software.canRender(snapshot));

    const auto selection = RenderPipelineContract::selectBackend(snapshot, gpu, software);
    EXPECT_EQ(selection.requested, RenderBackendKind::DiligentGPU);
    EXPECT_EQ(selection.selected, RenderBackendKind::Software);
    EXPECT_TRUE(selection.usedFallback());
    EXPECT_EQ(selection.fallbackReason, RenderFallbackReason::MissingHDR);
}

TEST(RenderPipelineContractTest, RejectsInvalidSnapshotAndIncompleteColorContract)
{
    const auto snapshot = makeSnapshot();
    auto incompleteColor = snapshot;
    incompleteColor.color = SurfaceColorDescriptor::unknown();
    auto invalidFrame = snapshot;
    invalidFrame.width = 0;
    const auto gpu = gpuCapabilities();

    EXPECT_EQ(gpu.failureReason(incompleteColor),
              RenderFallbackReason::ColorContractIncomplete);
    EXPECT_EQ(gpu.failureReason(invalidFrame), RenderFallbackReason::InvalidSnapshot);
}

TEST(RenderPipelineContractTest, RejectsMissingIdentityInvalidRateAndSchemaVersion)
{
    const auto snapshot = makeSnapshot();
    const auto gpu = gpuCapabilities();
    const auto software = softwareCapabilities();

    auto invalid = snapshot;
    invalid.compositionId = 0;
    EXPECT_EQ(gpu.failureReason(invalid), RenderFallbackReason::InvalidSnapshot);
    invalid = snapshot;
    invalid.viewId = 0;
    EXPECT_EQ(gpu.failureReason(invalid), RenderFallbackReason::InvalidSnapshot);
    invalid = snapshot;
    invalid.height = 0;
    EXPECT_EQ(gpu.failureReason(invalid), RenderFallbackReason::InvalidSnapshot);
    invalid = snapshot;
    invalid.frameRateNumerator = 0;
    EXPECT_EQ(gpu.failureReason(invalid), RenderFallbackReason::InvalidSnapshot);
    invalid = snapshot;
    invalid.frameRateNumerator = -1;
    EXPECT_EQ(gpu.failureReason(invalid), RenderFallbackReason::InvalidSnapshot);
    invalid = snapshot;
    invalid.frameRateDenominator = 0;
    EXPECT_EQ(gpu.failureReason(invalid), RenderFallbackReason::InvalidSnapshot);
    invalid = snapshot;
    invalid.frameRateDenominator = -1;
    EXPECT_EQ(gpu.failureReason(invalid), RenderFallbackReason::InvalidSnapshot);
    invalid = snapshot;
    ++invalid.schemaVersion;
    EXPECT_EQ(gpu.failureReason(invalid), RenderFallbackReason::InvalidSnapshot);

    const auto selection =
        RenderPipelineContract::selectBackend(invalid, gpu, software);
    EXPECT_FALSE(selection.resolved());
    EXPECT_EQ(selection.fallbackReason, RenderFallbackReason::InvalidSnapshot);
}

TEST(RenderPipelineContractTest, RequiredCapabilitiesFollowSnapshotFeaturesAndStorage)
{
    auto snapshot = makeSnapshot();
    EXPECT_EQ(snapshot.requiredCapabilities(),
              capabilityMask(RenderCapability::Raster) |
                  capabilityMask(RenderCapability::Compute) |
                  capabilityMask(RenderCapability::Float32Targets) |
                  capabilityMask(RenderCapability::HDR) |
                  capabilityMask(RenderCapability::TemporalHistory));

    snapshot.requiresCompute = false;
    snapshot.requiresHDR = false;
    snapshot.usesTemporalHistory = false;
    snapshot.color = SurfaceColorDescriptor::linearStraightRgba16Float();
    EXPECT_EQ(snapshot.requiredCapabilities(),
              capabilityMask(RenderCapability::Raster) |
                  capabilityMask(RenderCapability::Float16Targets));
}

TEST(RenderPipelineContractTest, ReportsMissingCapabilitiesAndResolutionLimit)
{
    const auto snapshot = makeSnapshot();
    constexpr RenderCapabilityMask required =
        RenderCapability::Raster | RenderCapability::Compute |
        RenderCapability::Float32Targets | RenderCapability::HDR |
        RenderCapability::TemporalHistory;
    RenderBackendCapabilities capabilities{
        RenderBackendKind::Software, required, 4096};

    capabilities.supported = required & ~capabilityMask(RenderCapability::Raster);
    EXPECT_EQ(capabilities.failureReason(snapshot), RenderFallbackReason::MissingRaster);
    capabilities.supported = required & ~capabilityMask(RenderCapability::Compute);
    EXPECT_EQ(capabilities.failureReason(snapshot), RenderFallbackReason::MissingCompute);
    capabilities.supported = required & ~capabilityMask(RenderCapability::Float32Targets);
    EXPECT_EQ(capabilities.failureReason(snapshot),
              RenderFallbackReason::MissingFloat32Targets);
    capabilities.supported = required & ~capabilityMask(RenderCapability::HDR);
    EXPECT_EQ(capabilities.failureReason(snapshot), RenderFallbackReason::MissingHDR);
    capabilities.supported = required &
        ~capabilityMask(RenderCapability::TemporalHistory);
    EXPECT_EQ(capabilities.failureReason(snapshot),
              RenderFallbackReason::MissingTemporalHistory);

    capabilities.supported = required;
    capabilities.maxTextureDimension = 1024;
    EXPECT_EQ(capabilities.failureReason(snapshot),
              RenderFallbackReason::ResolutionExceeded);

    auto tallSnapshot = snapshot;
    tallSnapshot.width = 512;
    tallSnapshot.height = 2048;
    EXPECT_EQ(capabilities.failureReason(tallSnapshot),
              RenderFallbackReason::ResolutionExceeded);
}

TEST(RenderPipelineContractTest, TextureDimensionLimitIsInclusivePerAxis)
{
    auto snapshot = makeSnapshot();
    auto capabilities = softwareCapabilities();
    capabilities.maxTextureDimension = 1024;

    snapshot.width = 1024;
    snapshot.height = 1024;
    EXPECT_EQ(capabilities.failureReason(snapshot), RenderFallbackReason::None);

    snapshot.width = 1025;
    EXPECT_EQ(capabilities.failureReason(snapshot),
              RenderFallbackReason::ResolutionExceeded);
    snapshot.width = 1024;
    snapshot.height = 1025;
    EXPECT_EQ(capabilities.failureReason(snapshot),
              RenderFallbackReason::ResolutionExceeded);
}

TEST(RenderPipelineContractTest, ReportsMissingFloat16TargetCapability)
{
    auto snapshot = makeSnapshot();
    snapshot.color = SurfaceColorDescriptor::linearStraightRgba16Float();
    auto capabilities = softwareCapabilities();
    capabilities.supported &= ~capabilityMask(RenderCapability::Float16Targets);

    EXPECT_EQ(capabilities.failureReason(snapshot),
              RenderFallbackReason::MissingFloat16Targets);
    capabilities.supported |= capabilityMask(RenderCapability::Float16Targets);
    EXPECT_EQ(capabilities.failureReason(snapshot), RenderFallbackReason::None);
}

TEST(RenderPipelineContractTest, LeavesBackendUnresolvedWhenBothCandidatesFail)
{
    const auto snapshot = makeSnapshot();
    auto gpu = gpuCapabilities();
    auto software = softwareCapabilities();
    software.supported &= ~capabilityMask(RenderCapability::Compute);

    const auto selection = RenderPipelineContract::selectBackend(snapshot, gpu, software);
    EXPECT_FALSE(selection.resolved());
    EXPECT_EQ(selection.selected, RenderBackendKind::Auto);
    EXPECT_EQ(selection.fallbackReason, RenderFallbackReason::MissingHDR);
}

TEST(RenderPipelineContractTest, KeepsRequestedGpuWhenBothCandidatesCanRender)
{
    const auto snapshot = makeSnapshot();
    auto gpu = gpuCapabilities();
    gpu.supported |= capabilityMask(RenderCapability::HDR);
    const auto software = softwareCapabilities();

    const auto selection = RenderPipelineContract::selectBackend(snapshot, gpu, software);
    EXPECT_TRUE(selection.resolved());
    EXPECT_EQ(selection.selected, RenderBackendKind::DiligentGPU);
    EXPECT_FALSE(selection.usedFallback());
    EXPECT_EQ(selection.fallbackReason, RenderFallbackReason::None);
}

TEST(RenderPipelineContractTest, AutoSelectsGpuAndFallsBackOnlyWhenNeeded)
{
    auto snapshot = makeSnapshot();
    snapshot.requestedBackend = RenderBackendKind::Auto;
    auto gpu = gpuCapabilities();
    auto software = softwareCapabilities();

    auto selection = RenderPipelineContract::selectBackend(snapshot, gpu, software);
    ASSERT_TRUE(selection.resolved());
    EXPECT_EQ(selection.requested, RenderBackendKind::Auto);
    EXPECT_EQ(selection.selected, RenderBackendKind::DiligentGPU);
    EXPECT_FALSE(selection.usedFallback());

    gpu.supported &= ~capabilityMask(RenderCapability::Float32Targets);
    selection = RenderPipelineContract::selectBackend(snapshot, gpu, software);
    ASSERT_TRUE(selection.resolved());
    EXPECT_EQ(selection.selected, RenderBackendKind::Software);
    EXPECT_FALSE(selection.usedFallback());
    EXPECT_EQ(selection.fallbackReason, RenderFallbackReason::MissingFloat32Targets);
}

TEST(RenderPipelineContractTest, ExplicitSoftwareRequestPrefersSoftwareWhenGpuIsAvailable)
{
    auto snapshot = makeSnapshot();
    snapshot.requestedBackend = RenderBackendKind::Software;
    auto gpu = gpuCapabilities();
    gpu.supported |= capabilityMask(RenderCapability::HDR);
    const auto software = softwareCapabilities();

    const auto selection = RenderPipelineContract::selectBackend(snapshot, gpu, software);
    ASSERT_TRUE(selection.resolved());
    EXPECT_EQ(selection.selected, RenderBackendKind::Software);
    EXPECT_FALSE(selection.usedFallback());
    EXPECT_EQ(selection.fallbackReason, RenderFallbackReason::None);
}

TEST(RenderPipelineContractTest, CacheKeyIsStableAndChangesWithFrame)
{
    auto snapshot = makeSnapshot();
    const auto first = snapshot.cacheKey(RenderBackendKind::DiligentGPU);
    const auto same = snapshot.cacheKey(RenderBackendKind::DiligentGPU);
    const auto software = snapshot.cacheKey(RenderBackendKind::Software);

    EXPECT_EQ(first, same);
    EXPECT_EQ(first.stableHash(), same.stableHash());
    EXPECT_NE(first, software);

    ++snapshot.frameIndex;
    EXPECT_NE(first, snapshot.cacheKey(RenderBackendKind::DiligentGPU));
}

TEST(RenderPipelineContractTest, CacheKeyTracksSceneIndexSettingsAndDimensions)
{
    const auto snapshot = makeSnapshot();
    const auto key = snapshot.cacheKey(RenderBackendKind::DiligentGPU);

    auto changed = snapshot;
    ++changed.sceneRevision;
    EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
    changed = snapshot;
    ++changed.renderIndexGeneration;
    EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
    changed = snapshot;
    ++changed.settingsRevision;
    EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
    changed = snapshot;
    ++changed.width;
    EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
    changed = snapshot;
    ++changed.height;
    EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
}

TEST(RenderPipelineContractTest, CacheKeyTracksFrameRateQualityAndColorContract)
{
    const auto snapshot = makeSnapshot();
    const auto key = snapshot.cacheKey(RenderBackendKind::DiligentGPU);

    auto changed = snapshot;
    ++changed.frameRateNumerator;
    EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
    changed = snapshot;
    ++changed.frameRateDenominator;
    EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
    changed = snapshot;
    changed.quality = RenderQuality::Final;
    EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
    changed = snapshot;
    changed.color.primaries = SurfaceColorPrimaries::DisplayP3_D65;
    EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
    changed = snapshot;
    changed.color.alphaMode = SurfaceAlphaMode::Straight;
    EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
}

TEST(RenderPipelineContractTest, CacheKeyTracksRemainingIdentityColorAndFeatureFields)
{
    const auto snapshot = makeSnapshot();
    const auto key = snapshot.cacheKey(RenderBackendKind::DiligentGPU);
    const auto expectChanged = [&snapshot, &key](auto mutate) {
        auto changed = snapshot;
        mutate(changed);
        EXPECT_NE(key, changed.cacheKey(RenderBackendKind::DiligentGPU));
    };

    expectChanged([](auto& value) { ++value.schemaVersion; });
    expectChanged([](auto& value) { ++value.compositionId; });
    expectChanged([](auto& value) { ++value.viewId; });
    expectChanged([](auto& value) {
        value.color.storage = SurfacePixelStorage::RGBA16Float;
    });
    expectChanged([](auto& value) {
        value.color.channelOrder = SurfaceChannelOrder::BGRA;
    });
    expectChanged([](auto& value) {
        value.color.transfer = TransferFunction::sRGB;
    });
    expectChanged([](auto& value) {
        value.color.range = SurfaceColorRange::DisplayReferred;
    });
    expectChanged([](auto& value) { value.color.transferKnown = false; });
    expectChanged([](auto& value) { value.requiresCompute = false; });
    expectChanged([](auto& value) { value.requiresHDR = false; });
    expectChanged([](auto& value) { value.usesTemporalHistory = false; });
}

TEST(RenderPipelineContractTest, RenderIndexSnapshotSortsByProxyId)
{
    RenderIndex index;
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{20}));
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{3}));
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{11}));

    const auto snapshot = index.snapshot();
    ASSERT_EQ(snapshot.proxies.size(), 3u);
    EXPECT_EQ(snapshot.proxies[0].descriptor.id, 3u);
    EXPECT_EQ(snapshot.proxies[1].descriptor.id, 11u);
    EXPECT_EQ(snapshot.proxies[2].descriptor.id, 20u);
}

TEST(RenderPipelineContractTest, DirtyOnlySnapshotFiltersCleanRecordsAndKeepsGeneration)
{
    RenderIndex index;
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{9}));
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{2}));
    index.clearDirty(9);
    index.clearDirty(2);
    const auto beforeDirtyUpdate = index.generation();

    EXPECT_TRUE(index.markDirty(9, RenderDirty::Material));
    const auto generation = index.generation();
    const auto all = index.snapshot();
    const auto dirtyOnly = index.snapshot(true);

    EXPECT_EQ(generation, beforeDirtyUpdate + 1u);
    EXPECT_EQ(all.generation, generation);
    EXPECT_EQ(dirtyOnly.generation, generation);
    ASSERT_EQ(all.proxies.size(), 2u);
    ASSERT_EQ(dirtyOnly.proxies.size(), 1u);
    EXPECT_EQ(dirtyOnly.proxies.front().descriptor.id, 9u);
    EXPECT_EQ(dirtyOnly.proxies.front().dirty, RenderDirty::Material);
}

TEST(RenderPipelineContractTest, RenderIndexRejectsZeroProxyIdWithoutMutation)
{
    RenderIndex index;
    EXPECT_FALSE(index.upsert(RenderProxyDescriptor{0}));
    EXPECT_EQ(index.size(), 0u);
    EXPECT_EQ(index.generation(), 0u);
    EXPECT_FALSE(index.find(0));
}

TEST(RenderPipelineContractTest, UpsertIncrementsRevisionAndAccumulatesDirtyFlags)
{
    RenderIndex index;
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{7}));
    const auto* first = index.find(7);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->revision, 1u);
    EXPECT_EQ(first->dirty, RenderDirty::All);
    EXPECT_EQ(index.generation(), 1u);

    index.clearDirty(7);
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{7}, RenderDirty::Transform));
    const auto* updated = index.find(7);
    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->revision, 2u);
    EXPECT_EQ(updated->dirty, RenderDirty::Transform);
    EXPECT_EQ(index.generation(), 2u);

    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{7}, RenderDirty::Material));
    EXPECT_EQ(index.find(7)->dirty, RenderDirty::Transform | RenderDirty::Material);
    EXPECT_EQ(index.find(7)->revision, 3u);
}

TEST(RenderPipelineContractTest, MarkDirtyAndClearDirtyHaveBoundedEffects)
{
    RenderIndex index;
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{5}));
    index.clearDirty(5);
    const auto generationAfterClear = index.generation();

    EXPECT_FALSE(index.markDirty(99, RenderDirty::Visibility));
    EXPECT_FALSE(index.markDirty(5, RenderDirty::None));
    EXPECT_EQ(index.generation(), generationAfterClear);

    ASSERT_TRUE(index.markDirty(5, RenderDirty::Geometry));
    EXPECT_EQ(index.find(5)->dirty, RenderDirty::Geometry);
    EXPECT_EQ(index.find(5)->revision, 2u);
    EXPECT_EQ(index.generation(), generationAfterClear + 1u);

    index.clearDirty(5, RenderDirty::Geometry);
    EXPECT_FALSE(any(index.find(5)->dirty));
    EXPECT_EQ(index.find(5)->revision, 2u);
    EXPECT_EQ(index.generation(), generationAfterClear + 1u);
}

TEST(RenderPipelineContractTest, EraseChangesGenerationOnlyForExistingProxy)
{
    RenderIndex index;
    EXPECT_FALSE(index.erase(1));
    EXPECT_EQ(index.generation(), 0u);

    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{1}));
    const auto generationAfterInsert = index.generation();
    EXPECT_TRUE(index.erase(1));
    EXPECT_EQ(index.size(), 0u);
    EXPECT_EQ(index.generation(), generationAfterInsert + 1u);
    EXPECT_FALSE(index.erase(1));
    EXPECT_EQ(index.generation(), generationAfterInsert + 1u);
}
