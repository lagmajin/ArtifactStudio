#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>

import Graphics.RenderIndex;
import Graphics.RenderPipelineFoundation;
import Graphics.GIResources;
import Graphics.SurfaceColorContract;
import Graphics.TemporalHistory;

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

TEST(RenderPipelineContractTest, CpuImageLayoutCanBeKnownBeforeColorSemantics)
{
    const auto unknownBgra = SurfaceColorDescriptor::unknownBgra32Float();
    EXPECT_FALSE(unknownBgra.isFullySpecified());
    EXPECT_FALSE(unknownBgra.isCanonical());
    EXPECT_TRUE(unknownBgra.isCanonicalCpuImage());

    const auto canonicalBgra =
        SurfaceColorDescriptor::canonicalLinearPremultipliedBgra(
            SurfaceColorPrimaries::Rec2020_D65);
    EXPECT_TRUE(canonicalBgra.isFullySpecified());
    EXPECT_FALSE(canonicalBgra.isCanonical());
    EXPECT_TRUE(canonicalBgra.isCanonicalCpuImage());
    EXPECT_EQ(canonicalBgra.primaries, SurfaceColorPrimaries::Rec2020_D65);
    EXPECT_EQ(canonicalBgra.alphaMode, SurfaceAlphaMode::Premultiplied);
    EXPECT_EQ(canonicalBgra.range, SurfaceColorRange::SceneReferred);
}

TEST(RenderPipelineContractTest, SrgbBgraAndLinearBgra16FactoriesKeepTheirFormats)
{
    const auto encoded = SurfaceColorDescriptor::encodedSrgbBgra8Straight();
    EXPECT_TRUE(encoded.isFullySpecified());
    EXPECT_FALSE(encoded.isCanonicalCpuImage());
    EXPECT_EQ(encoded.storage, SurfacePixelStorage::RGBA8UNormSrgb);
    EXPECT_EQ(encoded.channelOrder, SurfaceChannelOrder::BGRA);
    EXPECT_EQ(encoded.transfer, TransferFunction::sRGB);
    EXPECT_EQ(encoded.alphaMode, SurfaceAlphaMode::Straight);
    EXPECT_EQ(encoded.range, SurfaceColorRange::DisplayReferred);

    const auto linear = SurfaceColorDescriptor::linearStraightBgra16Float(
        SurfaceColorPrimaries::DisplayP3_D65);
    EXPECT_TRUE(linear.isFullySpecified());
    EXPECT_FALSE(linear.isCanonical());
    EXPECT_TRUE(linear.isCanonicalCpuImage());
    EXPECT_EQ(linear.storage, SurfacePixelStorage::RGBA16Float);
    EXPECT_EQ(linear.channelOrder, SurfaceChannelOrder::BGRA);
    EXPECT_EQ(linear.transfer, TransferFunction::Linear);
    EXPECT_EQ(linear.primaries, SurfaceColorPrimaries::DisplayP3_D65);
    EXPECT_EQ(linear.alphaMode, SurfaceAlphaMode::Straight);
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

TEST(RenderPipelineContractTest, ZeroTextureDimensionLimitIsUnbounded)
{
    auto snapshot = makeSnapshot();
    snapshot.width = std::numeric_limits<std::uint32_t>::max();
    snapshot.height = std::numeric_limits<std::uint32_t>::max();
    auto capabilities = gpuCapabilities();
    capabilities.supported |= capabilityMask(RenderCapability::HDR);
    capabilities.maxTextureDimension = 0;

    EXPECT_EQ(capabilities.failureReason(snapshot), RenderFallbackReason::None);
    EXPECT_TRUE(capabilities.canRender(snapshot));
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

TEST(RenderPipelineContractTest, CacheKeyUsesResolvedBackendAfterFallback)
{
    const auto snapshot = makeSnapshot();
    EXPECT_EQ(snapshot.cacheKey().backend, RenderBackendKind::DiligentGPU);

    auto gpu = gpuCapabilities();
    gpu.supported &= ~capabilityMask(RenderCapability::HDR);
    const auto selection = RenderPipelineContract::selectBackend(
        snapshot, gpu, softwareCapabilities());

    ASSERT_TRUE(selection.resolved());
    EXPECT_EQ(selection.selected, RenderBackendKind::Software);
    EXPECT_TRUE(selection.usedFallback());
    EXPECT_EQ(selection.cacheKey(snapshot).backend, RenderBackendKind::Software);
    EXPECT_NE(snapshot.cacheKey().stableHash(),
              selection.cacheKey(snapshot).stableHash());
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
    EXPECT_NE(first.stableHash(), software.stableHash());

    ++snapshot.frameIndex;
    const auto nextFrame = snapshot.cacheKey(RenderBackendKind::DiligentGPU);
    EXPECT_NE(first, nextFrame);
    EXPECT_NE(first.stableHash(), nextFrame.stableHash());
}

TEST(RenderPipelineContractTest, CacheKeyTracksSceneIndexSettingsAndDimensions)
{
    const auto snapshot = makeSnapshot();
    const auto key = snapshot.cacheKey(RenderBackendKind::DiligentGPU);
    const auto expectChanged = [&snapshot, &key](auto mutate) {
        auto changed = snapshot;
        mutate(changed);
        const auto changedKey = changed.cacheKey(RenderBackendKind::DiligentGPU);
        EXPECT_NE(key, changedKey);
        EXPECT_NE(key.stableHash(), changedKey.stableHash());
    };

    expectChanged([](auto& value) { ++value.sceneRevision; });
    expectChanged([](auto& value) { ++value.renderIndexGeneration; });
    expectChanged([](auto& value) { ++value.settingsRevision; });
    expectChanged([](auto& value) { ++value.width; });
    expectChanged([](auto& value) { ++value.height; });
}

TEST(RenderPipelineContractTest, CacheKeyTracksFrameRateQualityAndColorContract)
{
    const auto snapshot = makeSnapshot();
    const auto key = snapshot.cacheKey(RenderBackendKind::DiligentGPU);
    const auto expectChanged = [&snapshot, &key](auto mutate) {
        auto changed = snapshot;
        mutate(changed);
        const auto changedKey = changed.cacheKey(RenderBackendKind::DiligentGPU);
        EXPECT_NE(key, changedKey);
        EXPECT_NE(key.stableHash(), changedKey.stableHash());
    };

    expectChanged([](auto& value) { ++value.frameRateNumerator; });
    expectChanged([](auto& value) { ++value.frameRateDenominator; });
    expectChanged([](auto& value) { value.quality = RenderQuality::Final; });
    expectChanged([](auto& value) {
        value.color.primaries = SurfaceColorPrimaries::DisplayP3_D65;
    });
    expectChanged([](auto& value) {
        value.color.alphaMode = SurfaceAlphaMode::Straight;
    });
}

TEST(RenderPipelineContractTest,
     TemporalHistorySeparatesPreviewAndFinalQualityAtTheSameResolution)
{
    const TemporalHistoryKey previewKey{
        101, 7, "gi.diffuse", 960, 540,
        static_cast<std::uint32_t>(RenderQuality::Preview)};
    const TemporalHistoryKey finalKey{
        101, 7, "gi.diffuse", 960, 540,
        static_cast<std::uint32_t>(RenderQuality::Final)};
    EXPECT_NE(previewKey, finalKey);

    TemporalHistoryRegistry registry;
    const auto& previewEntry = registry.store(previewKey, 401, 48);
    ASSERT_TRUE(previewEntry.valid);
    const auto& finalEntry = registry.store(finalKey, 902, 48);
    ASSERT_TRUE(finalEntry.valid);

    EXPECT_EQ(registry.size(), 2u);
    ASSERT_NE(registry.find(previewKey), nullptr);
    ASSERT_NE(registry.find(finalKey), nullptr);
    EXPECT_EQ(registry.find(previewKey)->resourceToken, 401u);
    EXPECT_EQ(registry.find(finalKey)->resourceToken, 902u);
    EXPECT_TRUE(registry.isReusable(previewKey, 49));
    EXPECT_TRUE(registry.isReusable(finalKey, 49));

    EXPECT_TRUE(registry.invalidate(
        previewKey, TemporalInvalidationReason::QualityChanged));
    ASSERT_NE(registry.find(previewKey), nullptr);
    EXPECT_FALSE(registry.find(previewKey)->valid);
    EXPECT_EQ(registry.find(previewKey)->invalidationReason,
              TemporalInvalidationReason::QualityChanged);
    ASSERT_NE(registry.find(finalKey), nullptr);
    EXPECT_TRUE(registry.find(finalKey)->valid)
        << "invalidating Preview history must preserve the Final history";

    const std::uint64_t previousRevision = registry.revision();
    const auto& restoredPreview = registry.store(previewKey, 511, 49);
    EXPECT_TRUE(restoredPreview.valid);
    EXPECT_EQ(restoredPreview.resourceToken, 511u);
    EXPECT_EQ(restoredPreview.invalidationReason,
              TemporalInvalidationReason::None);
    EXPECT_GT(registry.revision(), previousRevision);
    EXPECT_TRUE(registry.isReusable(previewKey, 50));
    EXPECT_EQ(registry.find(finalKey)->resourceToken, 902u)
        << "restoring Preview history must not replace the Final resource";
}

TEST(RenderPipelineContractTest,
     TemporalHistorySeparatesResolutionLevelsAtTheSameQuality)
{
    const std::uint32_t previewQuality =
        static_cast<std::uint32_t>(RenderQuality::Preview);
    const TemporalHistoryKey lowResolutionKey{
        101, 7, "gi.diffuse", 480, 270, previewQuality};
    const TemporalHistoryKey fullResolutionKey{
        101, 7, "gi.diffuse", 1920, 1080, previewQuality};
    EXPECT_NE(lowResolutionKey, fullResolutionKey);

    TemporalHistoryRegistry registry;
    registry.store(lowResolutionKey, 210, 80);
    registry.store(fullResolutionKey, 840, 80);

    EXPECT_EQ(registry.size(), 2u);
    ASSERT_NE(registry.find(lowResolutionKey), nullptr);
    ASSERT_NE(registry.find(fullResolutionKey), nullptr);
    EXPECT_EQ(registry.find(lowResolutionKey)->resourceToken, 210u);
    EXPECT_EQ(registry.find(fullResolutionKey)->resourceToken, 840u);
    EXPECT_TRUE(registry.invalidate(
        lowResolutionKey, TemporalInvalidationReason::ResolutionChanged));
    EXPECT_FALSE(registry.find(lowResolutionKey)->valid);
    EXPECT_EQ(registry.find(lowResolutionKey)->invalidationReason,
              TemporalInvalidationReason::ResolutionChanged);
    EXPECT_TRUE(registry.find(fullResolutionKey)->valid)
        << "invalidating a low-resolution history must preserve full resolution";
}

TEST(RenderPipelineContractTest,
     TemporalHistoryDoesNotTreatSignedFrameWraparoundAsReusable)
{
    const TemporalHistoryKey key{
        101, 7, "gi.diffuse", 960, 540,
        static_cast<std::uint32_t>(RenderQuality::Preview)};
    TemporalHistoryRegistry registry;
    registry.store(key, 401,
                   std::numeric_limits<std::int64_t>::max());

    EXPECT_FALSE(registry.isReusable(
        key, std::numeric_limits<std::int64_t>::min()));
}

TEST(RenderPipelineContractTest, CacheKeyTracksRemainingIdentityColorAndFeatureFields)
{
    const auto snapshot = makeSnapshot();
    const auto key = snapshot.cacheKey(RenderBackendKind::DiligentGPU);
    const auto expectChanged = [&snapshot, &key](auto mutate) {
        auto changed = snapshot;
        mutate(changed);
        const auto changedKey = changed.cacheKey(RenderBackendKind::DiligentGPU);
        EXPECT_NE(key, changedKey);
        EXPECT_NE(key.stableHash(), changedKey.stableHash());
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

TEST(RenderPipelineContractTest,
     SnapshotRemainsStableAfterIndexMutation)
{
    RenderIndex index;
    RenderProxyDescriptor first{};
    first.id = 5;
    first.kind = RenderProxyKind::Mesh;
    first.geometryKey = "geometry-before";
    first.materialKey = "material-before";
    ASSERT_TRUE(index.upsert(first));
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{8}));

    const RenderIndexSnapshot snapshot = index.snapshot();
    ASSERT_EQ(snapshot.proxies.size(), 2u);
    const auto snapshotGeneration = snapshot.generation;

    RenderProxyDescriptor updated = first;
    updated.geometryKey = "geometry-after";
    updated.visible = false;
    ASSERT_TRUE(index.upsert(updated, RenderDirty::Geometry));
    ASSERT_TRUE(index.erase(8));
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{13}));

    EXPECT_NE(index.generation(), snapshotGeneration);
    ASSERT_EQ(snapshot.proxies.size(), 2u);
    EXPECT_EQ(snapshot.generation, snapshotGeneration);
    EXPECT_EQ(snapshot.proxies[0].descriptor.id, 5u);
    EXPECT_EQ(snapshot.proxies[0].descriptor.geometryKey, "geometry-before");
    EXPECT_EQ(snapshot.proxies[0].descriptor.materialKey, "material-before");
    EXPECT_TRUE(snapshot.proxies[0].descriptor.visible);
    EXPECT_EQ(snapshot.proxies[1].descriptor.id, 8u);
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

TEST(RenderPipelineContractTest,
     ClearingOneDirtyFlagPreservesOtherFlagsAndRevision)
{
    RenderIndex index;
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{13}));
    index.clearDirty(13);
    ASSERT_TRUE(index.markDirty(13, RenderDirty::Transform));
    ASSERT_TRUE(index.markDirty(13, RenderDirty::Material));

    const auto generationBeforeClear = index.generation();
    const auto revisionBeforeClear = index.find(13)->revision;
    index.clearDirty(13, RenderDirty::Transform);

    const RenderProxyRecord* record = index.find(13);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->dirty, RenderDirty::Material);
    EXPECT_EQ(record->revision, revisionBeforeClear);
    EXPECT_EQ(index.generation(), generationBeforeClear);

    const auto dirtySnapshot = index.snapshot(true);
    ASSERT_EQ(dirtySnapshot.proxies.size(), 1u);
    EXPECT_EQ(dirtySnapshot.proxies.front().descriptor.id, 13u);
    EXPECT_EQ(dirtySnapshot.proxies.front().dirty, RenderDirty::Material);
}

TEST(RenderPipelineContractTest,
     ClearDirtyIgnoresMissingProxyAndEmptyMask)
{
    RenderIndex index;
    ASSERT_TRUE(index.upsert(RenderProxyDescriptor{17}));
    const RenderProxyRecord* original = index.find(17);
    ASSERT_NE(original, nullptr);
    const auto originalDirty = original->dirty;
    const auto originalRevision = original->revision;
    const auto originalGeneration = index.generation();

    index.clearDirty(99, RenderDirty::Material);
    index.clearDirty(17, RenderDirty::None);

    const RenderProxyRecord* unchanged = index.find(17);
    ASSERT_NE(unchanged, nullptr);
    EXPECT_EQ(unchanged->dirty, originalDirty);
    EXPECT_EQ(unchanged->revision, originalRevision);
    EXPECT_EQ(index.generation(), originalGeneration);
    EXPECT_EQ(index.size(), 1u);
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

TEST(RenderPipelineContractTest, GIFrameResourceResizeAdvancesResourceGeneration)
{
    GIFrameResources resources;
    EXPECT_FALSE(resources.valid());
    EXPECT_EQ(resources.generation(), 0u);

    resources.resize(1920, 1080);
    ASSERT_TRUE(resources.valid());
    EXPECT_EQ(resources.generation(), 1u);
    EXPECT_EQ(resources.descriptor(GIResourceKind::History).width, 1920u);
    EXPECT_EQ(resources.descriptor(GIResourceKind::History).height, 1080u);

    resources.resize(1280, 720);
    EXPECT_EQ(resources.generation(), 2u);
    EXPECT_EQ(resources.width(), 1280u);
    EXPECT_EQ(resources.height(), 720u);
}

TEST(RenderPipelineContractTest,
     GIFrameContextResizeOnlyInvalidatesResourcesWhenDimensionsChange)
{
    GIFrameContext context(GISettings::fast());
    context.resize(640, 360);
    context.beginFrame(1);
    context.commitHistory();
    ASSERT_TRUE(context.historyReady());

    const auto resourceGeneration = context.resources().generation();
    const auto historyGeneration = context.accumulation().generation();
    context.resize(640, 360);
    EXPECT_EQ(context.resources().generation(), resourceGeneration);
    EXPECT_EQ(context.accumulation().generation(), historyGeneration);
    EXPECT_TRUE(context.historyReady())
        << "a duplicate resize must preserve valid temporal history";

    context.resize(800, 450);
    EXPECT_EQ(context.resources().generation(), resourceGeneration + 1u);
    EXPECT_EQ(context.resources().width(), 800u);
    EXPECT_EQ(context.resources().height(), 450u);
    EXPECT_FALSE(context.historyReady())
        << "a real size change must invalidate temporal history";
    EXPECT_EQ(context.accumulation().generation(), historyGeneration + 1u);

    const auto changedResourceGeneration = context.resources().generation();
    const auto changedHistoryGeneration = context.accumulation().generation();
    context.resize(800, 450);
    EXPECT_EQ(context.resources().generation(), changedResourceGeneration);
    EXPECT_EQ(context.accumulation().generation(), changedHistoryGeneration);
}

TEST(RenderPipelineContractTest, GIWorkingResolutionFollowsQualityScale)
{
    GIFrameResources resources;
    resources.resize(1919, 1079);

    const auto fast = GISettings::fast();
    EXPECT_EQ(resources.workingWidth(fast), 959u);
    EXPECT_EQ(resources.workingHeight(fast), 539u);

    const auto quality = GISettings::quality();
    EXPECT_EQ(resources.workingWidth(quality), 1919u);
    EXPECT_EQ(resources.workingHeight(quality), 1079u);
}

TEST(RenderPipelineContractTest, GIWorkingResolutionScalesOddDimensionsMonotonically)
{
    GIFrameResources resources;
    resources.resize(1919, 1079);

    GISettings low = GISettings::fast();
    low.resolutionScale = 0.125f;
    GISettings medium = low;
    medium.resolutionScale = 0.75f;
    const GISettings high = GISettings::quality();

    const auto lowWidth = resources.workingWidth(low);
    const auto mediumWidth = resources.workingWidth(medium);
    const auto highWidth = resources.workingWidth(high);
    const auto lowHeight = resources.workingHeight(low);
    const auto mediumHeight = resources.workingHeight(medium);
    const auto highHeight = resources.workingHeight(high);

    EXPECT_EQ(lowWidth, 239u);
    EXPECT_EQ(mediumWidth, 1439u);
    EXPECT_EQ(highWidth, 1919u);
    EXPECT_EQ(lowHeight, 134u);
    EXPECT_EQ(mediumHeight, 809u);
    EXPECT_EQ(highHeight, 1079u);
    EXPECT_LT(lowWidth, mediumWidth);
    EXPECT_LT(mediumWidth, highWidth);
    EXPECT_LT(lowHeight, mediumHeight);
    EXPECT_LT(mediumHeight, highHeight);
}

TEST(RenderPipelineContractTest, GIWorkingResolutionKeepsTinyValidImagesNonzero)
{
    GIFrameResources resources;
    resources.resize(1, 2);

    const auto fast = GISettings::fast();
    EXPECT_EQ(resources.workingWidth(fast), 1u);
    EXPECT_EQ(resources.workingHeight(fast), 1u);
}

TEST(RenderPipelineContractTest, GIWorkingResolutionSupportsScalesAboveOne)
{
    GIFrameResources resources;
    resources.resize(5, 3);

    GISettings supersampled = GISettings::quality();
    supersampled.resolutionScale = 1.5f;

    EXPECT_EQ(resources.workingWidth(supersampled), 7u);
    EXPECT_EQ(resources.workingHeight(supersampled), 4u);
}

TEST(RenderPipelineContractTest, GIWorkingResolutionIsZeroForInvalidBaseOrScale)
{
    GIFrameResources resources;
    GISettings settings = GISettings::quality();

    EXPECT_EQ(resources.workingWidth(settings), 0u);
    EXPECT_EQ(resources.workingHeight(settings), 0u);

    resources.resize(640, 360);
    settings.resolutionScale = 0.0f;
    EXPECT_EQ(resources.workingWidth(settings), 0u);
    EXPECT_EQ(resources.workingHeight(settings), 0u);

    settings.resolutionScale = -0.5f;
    EXPECT_EQ(resources.workingWidth(settings), 0u);
    EXPECT_EQ(resources.workingHeight(settings), 0u);
}

TEST(RenderPipelineContractTest, GIWorkingResolutionRejectsNonFiniteScales)
{
    GIFrameResources resources;
    resources.resize(640, 360);
    GISettings settings = GISettings::quality();

    for (const float invalidScale : {
             std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity()}) {
        settings.resolutionScale = invalidScale;
        EXPECT_EQ(resources.workingWidth(settings), 0u)
            << "invalid resolution scale " << invalidScale;
        EXPECT_EQ(resources.workingHeight(settings), 0u)
            << "invalid resolution scale " << invalidScale;
    }
}

TEST(RenderPipelineContractTest, ChangingGIResolutionScaleInvalidatesHistory)
{
    GIFrameContext context(GISettings::fast());
    context.resize(1919, 1079);
    EXPECT_EQ(context.workingWidth(), 959u);
    EXPECT_EQ(context.workingHeight(), 539u);

    context.beginFrame(1);
    context.commitHistory();
    ASSERT_TRUE(context.historyReady());
    const auto generationBeforeQualityChange =
        context.accumulation().generation();

    context.configure(GISettings::quality());

    EXPECT_EQ(context.workingWidth(), 1919u);
    EXPECT_EQ(context.workingHeight(), 1079u);
    EXPECT_FALSE(context.historyReady());
    EXPECT_GT(context.accumulation().generation(),
              generationBeforeQualityChange);

    context.beginFrame(2);
    context.commitHistory();
    ASSERT_TRUE(context.historyReady());

    const auto generationBeforeScaleDown =
        context.accumulation().generation();
    context.configure(GISettings::fast());
    EXPECT_EQ(context.workingWidth(), 959u);
    EXPECT_EQ(context.workingHeight(), 539u);
    EXPECT_FALSE(context.historyReady());
    EXPECT_GT(context.accumulation().generation(),
              generationBeforeScaleDown);

    context.beginFrame(3);
    context.commitHistory();
    ASSERT_TRUE(context.historyReady());
    context.resize(1280, 720);
    EXPECT_EQ(context.workingWidth(), 640u);
    EXPECT_EQ(context.workingHeight(), 360u);
    EXPECT_FALSE(context.historyReady());
}

TEST(RenderPipelineContractTest, GIQualityPresetsSelectExpectedPassBudgets)
{
    const GIExecutionPlan fast = GIExecutionPlan::forSettings(GISettings::fast());
    EXPECT_FALSE(fast.empty());
    EXPECT_EQ(fast.gatherSampleCount(), 4u);
    EXPECT_FALSE(fast.contains(GIPassKind::DepthPyramid));
    EXPECT_FALSE(fast.contains(GIPassKind::BilateralDenoise));
    EXPECT_TRUE(fast.contains(GIPassKind::TemporalResolve));
    EXPECT_TRUE(fast.requiresHistory());

    const GIExecutionPlan quality =
        GIExecutionPlan::forSettings(GISettings::quality());
    EXPECT_EQ(quality.gatherSampleCount(), 24u);
    EXPECT_TRUE(quality.contains(GIPassKind::DepthPyramid));
    EXPECT_TRUE(quality.contains(GIPassKind::BilateralDenoise));
    EXPECT_TRUE(quality.contains(GIPassKind::TemporalResolve));

    const GIExecutionPlan disabled =
        GIExecutionPlan::forSettings(GISettings::disabled());
    EXPECT_TRUE(disabled.empty());
    EXPECT_EQ(disabled.gatherSampleCount(), 0u);
    EXPECT_FALSE(disabled.requiresHistory());
}

TEST(RenderPipelineContractTest, GIQualityTransitionsReplacePlanAndInvalidateHistory)
{
    GIFrameContext context(GISettings::fast());
    context.resize(1280, 720);
    context.beginFrame(1);
    context.commitHistory();
    ASSERT_TRUE(context.historyReady());
    const auto resourceGeneration = context.resources().generation();

    context.configure(GISettings::quality());
    EXPECT_EQ(context.plan().gatherSampleCount(), 24u);
    EXPECT_TRUE(context.plan().contains(GIPassKind::DepthPyramid));
    EXPECT_FALSE(context.historyReady());
    EXPECT_EQ(context.resources().generation(), resourceGeneration);
    EXPECT_EQ(context.resources().width(), 1280u);
    EXPECT_EQ(context.resources().height(), 720u);

    context.commitHistory();
    ASSERT_TRUE(context.historyReady());
    context.configure(GISettings::disabled());
    EXPECT_TRUE(context.plan().empty());
    EXPECT_FALSE(context.historyReady());
    EXPECT_EQ(context.resources().generation(), resourceGeneration);

    context.configure(GISettings::fast());
    EXPECT_EQ(context.plan().gatherSampleCount(), 4u);
    EXPECT_FALSE(context.plan().contains(GIPassKind::DepthPyramid));
    EXPECT_FALSE(context.plan().contains(GIPassKind::BilateralDenoise));
    EXPECT_FALSE(context.historyReady());
    EXPECT_EQ(context.resources().generation(), resourceGeneration);
}

TEST(RenderPipelineContractTest, GIFrameResourceDescriptorsMatchEveryResourceLayout)
{
    GIFrameResources resources;
    resources.resize(640, 360);

    struct DescriptorCase {
        GIResourceKind kind;
        std::uint32_t expectedChannels;
    };
    const DescriptorCase cases[] = {
        {GIResourceKind::Depth, 1u},
        {GIResourceKind::Normal, 4u},
        {GIResourceKind::Motion, 4u},
        {GIResourceKind::DirectLighting, 4u},
        {GIResourceKind::IndirectLighting, 4u},
        {GIResourceKind::History, 4u},
    };

    for (const auto& testCase : cases) {
        const auto descriptor = resources.descriptor(testCase.kind);
        EXPECT_EQ(descriptor.kind, testCase.kind);
        EXPECT_EQ(descriptor.width, 640u);
        EXPECT_EQ(descriptor.height, 360u);
        EXPECT_EQ(descriptor.channels, testCase.expectedChannels);
    }
}

TEST(RenderPipelineContractTest, GITemporalHistoryResetsOnDiscontinuityAndCameraCut)
{
    GIAccumulationState state;
    state.beginFrame(10);
    EXPECT_FALSE(state.historyValid());
    state.commitHistory();
    ASSERT_TRUE(state.historyValid());
    const auto initialGeneration = state.generation();

    state.beginFrame(11);
    EXPECT_TRUE(state.historyValid());
    EXPECT_EQ(state.generation(), initialGeneration);

    state.beginFrame(14);
    EXPECT_FALSE(state.historyValid());
    EXPECT_EQ(state.generation(), initialGeneration + 1u);
    state.commitHistory();
    state.beginFrame(15, true);
    EXPECT_FALSE(state.historyValid());
    EXPECT_EQ(state.generation(), initialGeneration + 2u);
}
