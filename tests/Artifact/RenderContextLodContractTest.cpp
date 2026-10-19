#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

#include <QString>
#include <QSize>
#include <QSizeF>

import Artifact.Render.Context;
import Artifact.Render.ROI;

TEST(RenderContextLodContractTest, RenderModesSelectExpectedResolutionScales)
{
    Artifact::RenderContext context;

    context.setMode(Artifact::RenderMode::Editor);
    EXPECT_FLOAT_EQ(context.resolutionScale, 0.5f);

    context.setMode(Artifact::RenderMode::Preview);
    EXPECT_FLOAT_EQ(context.resolutionScale, 1.0f);

    context.setMode(Artifact::RenderMode::Final);
    EXPECT_FLOAT_EQ(context.resolutionScale, 1.0f);
}

TEST(RenderContextLodContractTest, CustomResolutionScaleHasMinimumPositiveFloor)
{
    Artifact::RenderContext context;

    context.setResolutionScale(0.25f);
    EXPECT_FLOAT_EQ(context.resolutionScale, 0.25f);

    context.setResolutionScale(0.0f);
    EXPECT_FLOAT_EQ(context.resolutionScale, 0.01f);

    context.setResolutionScale(-2.0f);
    EXPECT_FLOAT_EQ(context.resolutionScale, 0.01f);
}

TEST(RenderContextLodContractTest,
     ZoomLODHasMinimumPositiveFloorAndPreservesHigherMagnification)
{
    Artifact::RenderContext context;

    context.setZoom(0.0f);
    EXPECT_FLOAT_EQ(context.zoom, 0.01f);

    context.setZoom(-2.0f);
    EXPECT_FLOAT_EQ(context.zoom, 0.01f);

    constexpr std::array<float, 5> zoomLevels{0.125f, 0.75f, 1.0f, 2.0f, 8.0f};
    for (const float zoom : zoomLevels) {
        context.setZoom(zoom);
        EXPECT_FLOAT_EQ(context.zoom, zoom);
    }
}

TEST(RenderContextLodContractTest, NonFiniteZoomFallsBackToFullResolution)
{
    Artifact::RenderContext context;
    const float infinity = std::numeric_limits<float>::infinity();
    const float notANumber = std::numeric_limits<float>::quiet_NaN();

    context.setZoom(notANumber);
    EXPECT_FLOAT_EQ(context.zoom, 1.0f);

    context.setZoom(infinity);
    EXPECT_FLOAT_EQ(context.zoom, 1.0f);

    context.setZoom(-infinity);
    EXPECT_FLOAT_EQ(context.zoom, 1.0f);
}

TEST(RenderContextLodContractTest, SnapshotRoundTripPreservesZoomLOD)
{
    constexpr std::array<float, 5> zoomLevels{0.01f, 0.125f, 0.5f, 1.0f, 4.0f};
    for (const float zoom : zoomLevels) {
        Artifact::RenderContext context;
        context.setZoom(zoom);

        const auto snapshot = Artifact::createRenderContextSnapshot(
            context, Artifact::RenderPurpose::EditorInteractive);
        const auto restored = Artifact::restoreRenderContext(snapshot);

        ASSERT_TRUE(restored);
        EXPECT_FLOAT_EQ(snapshot.zoom, zoom);
        EXPECT_FLOAT_EQ(restored->zoom, zoom);
    }
}

TEST(RenderContextLodContractTest,
     RestoringNonFiniteZoomAndResolutionLODUsesFullResolutionFallback)
{
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<float, 3> invalidValues{notANumber, infinity, -infinity};

    for (const float invalidValue : invalidValues) {
        Artifact::RenderContextSnapshot snapshot;
        snapshot.zoom = invalidValue;
        snapshot.resolutionScale = invalidValue;

        const auto restored = Artifact::restoreRenderContext(snapshot);

        ASSERT_TRUE(restored);
        EXPECT_FLOAT_EQ(restored->zoom, 1.0f)
            << "invalid zoom should not leave restored LOD state non-finite";
        EXPECT_FLOAT_EQ(restored->resolutionScale, 1.0f)
            << "invalid resolution scale should use the full-resolution fallback";
    }
}

TEST(RenderContextLodContractTest,
     ResetRestoresDefaultZoomAndResolutionLODAfterChanges)
{
    Artifact::RenderContext context;
    context.setZoom(0.125f);
    context.setResolutionScale(0.25f);
    ASSERT_FLOAT_EQ(context.zoom, 0.125f);
    ASSERT_FLOAT_EQ(context.resolutionScale, 0.25f);

    context.reset();

    EXPECT_EQ(context.mode, Artifact::RenderMode::Preview);
    EXPECT_FLOAT_EQ(context.zoom, 1.0f);
    EXPECT_FLOAT_EQ(context.resolutionScale, 1.0f)
        << "reset should discard the previous custom resolution LOD";
}

TEST(RenderContextLodContractTest, RenderModeChangesPreserveCurrentZoomLOD)
{
    Artifact::RenderContext context;
    context.setZoom(0.375f);

    context.setMode(Artifact::RenderMode::Editor);
    EXPECT_FLOAT_EQ(context.zoom, 0.375f);

    context.setMode(Artifact::RenderMode::Final);
    EXPECT_FLOAT_EQ(context.zoom, 0.375f);
}

TEST(RenderContextLodContractTest, NonFiniteResolutionScaleFallsBackToFullResolution)
{
    Artifact::RenderContext context;
    const float infinity = std::numeric_limits<float>::infinity();
    const float notANumber = std::numeric_limits<float>::quiet_NaN();

    context.setResolutionScale(notANumber);
    EXPECT_FLOAT_EQ(context.resolutionScale, 1.0f);

    context.setResolutionScale(infinity);
    EXPECT_FLOAT_EQ(context.resolutionScale, 1.0f);

    context.setResolutionScale(-infinity);
    EXPECT_FLOAT_EQ(context.resolutionScale, 1.0f);
}

TEST(RenderContextLodContractTest, SwitchingRenderModeReplacesCustomScaleWithModeDefault)
{
    Artifact::RenderContext context;
    context.setResolutionScale(0.375f);
    ASSERT_FLOAT_EQ(context.resolutionScale, 0.375f);

    context.setMode(Artifact::RenderMode::Editor);

    EXPECT_FLOAT_EQ(context.resolutionScale, 0.5f);
}

TEST(RenderContextLodContractTest, SnapshotRoundTripPreservesResolutionScale)
{
    Artifact::RenderContext context;
    context.setResolutionScale(0.375f);

    const auto snapshot = Artifact::createRenderContextSnapshot(
        context, Artifact::RenderPurpose::EditorInteractive);
    const auto restored = Artifact::restoreRenderContext(snapshot);

    ASSERT_TRUE(restored);
    EXPECT_FLOAT_EQ(snapshot.resolutionScale, 0.375f);
    EXPECT_FLOAT_EQ(restored->resolutionScale, 0.375f);
}

TEST(RenderContextLodContractTest,
     SnapshotRoundTripPreservesMinimumAndUpscaledResolutionScales)
{
    constexpr std::array<float, 3> scales{0.01f, 1.0f, 4.0f};
    for (const float scale : scales) {
        Artifact::RenderContext context;
        context.setResolutionScale(scale);

        const auto snapshot = Artifact::createRenderContextSnapshot(
            context, Artifact::RenderPurpose::FinalExport);
        const auto restored = Artifact::restoreRenderContext(snapshot);

        ASSERT_TRUE(restored);
        EXPECT_FLOAT_EQ(snapshot.resolutionScale, scale);
        EXPECT_FLOAT_EQ(restored->resolutionScale, scale);
    }
}

TEST(RenderContextLodContractTest,
     ExportAndProxyFactoriesPreserveResolutionLOD)
{
    const QSize viewportSize{3840, 2160};
    const QSizeF canvasSize{1920.0, 1080.0};
    const Artifact::RenderROI roi(0, 0, 1920, 1080);

    const auto exportContext = Artifact::createFinalExportContext(
        41, viewportSize, canvasSize, roi, 2.0f);
    ASSERT_TRUE(exportContext);
    EXPECT_EQ(exportContext->mode, Artifact::RenderMode::Final);
    EXPECT_EQ(exportContext->currentFrame, 41);
    EXPECT_EQ(exportContext->viewportSize, viewportSize);
    EXPECT_FLOAT_EQ(exportContext->resolutionScale, 2.0f);
    EXPECT_FALSE(exportContext->interactive);

    const auto exportSnapshot = Artifact::createRenderContextSnapshot(
        *exportContext, Artifact::RenderPurpose::FinalExport);
    const auto restoredExport = Artifact::restoreRenderContext(exportSnapshot);
    ASSERT_TRUE(restoredExport);
    EXPECT_EQ(exportSnapshot.purpose, Artifact::RenderPurpose::FinalExport);
    EXPECT_EQ(restoredExport->mode, Artifact::RenderMode::Final);
    EXPECT_FLOAT_EQ(restoredExport->resolutionScale, 2.0f);
    EXPECT_FALSE(restoredExport->interactive);

    const auto proxyContext = Artifact::createProxyBuildContext(
        7, QSize{480, 270}, canvasSize, roi, 0.25f);
    ASSERT_TRUE(proxyContext);
    EXPECT_EQ(proxyContext->mode, Artifact::RenderMode::Preview);
    EXPECT_EQ(proxyContext->currentFrame, 7);
    EXPECT_FLOAT_EQ(proxyContext->resolutionScale, 0.25f);
    EXPECT_FALSE(proxyContext->interactive);

    const auto proxySnapshot = Artifact::createRenderContextSnapshot(
        *proxyContext, Artifact::RenderPurpose::ProxyBuild);
    const auto restoredProxy = Artifact::restoreRenderContext(proxySnapshot);
    ASSERT_TRUE(restoredProxy);
    EXPECT_EQ(proxySnapshot.purpose, Artifact::RenderPurpose::ProxyBuild);
    EXPECT_EQ(restoredProxy->mode, Artifact::RenderMode::Preview);
    EXPECT_FLOAT_EQ(restoredProxy->resolutionScale, 0.25f);
    EXPECT_FALSE(restoredProxy->interactive);
}

TEST(RenderContextLodContractTest,
     InteractiveStateIsRestrictedByRenderPurpose)
{
    constexpr std::array<Artifact::RenderPurpose, 5> purposes{
        Artifact::RenderPurpose::EditorInteractive,
        Artifact::RenderPurpose::EditorPreview,
        Artifact::RenderPurpose::FinalExport,
        Artifact::RenderPurpose::ProxyBuild,
        Artifact::RenderPurpose::Thumbnail};
    constexpr QSize viewportSize{640, 360};
    constexpr QSizeF canvasSize{1920.0, 1080.0};
    const Artifact::RenderROI roi(0, 0, 320, 180);

    for (const Artifact::RenderPurpose purpose : purposes) {
        for (const bool requestedInteractive : {false, true}) {
            const auto context = Artifact::createRenderContextForPurpose(
                purpose, Artifact::RenderMode::Preview, 12, viewportSize,
                canvasSize, roi, 0.5f, requestedInteractive);

            ASSERT_TRUE(context);
            const bool purposeAllowsInteraction =
                purpose == Artifact::RenderPurpose::EditorInteractive ||
                purpose == Artifact::RenderPurpose::EditorPreview;
            EXPECT_EQ(context->interactive,
                      requestedInteractive && purposeAllowsInteraction)
                << "purpose=" << static_cast<int>(purpose)
                << " requestedInteractive=" << requestedInteractive;
            EXPECT_FLOAT_EQ(context->resolutionScale, 0.5f);
            EXPECT_EQ(context->currentFrame, 12);

            const auto snapshot = Artifact::createRenderContextSnapshot(
                *context, purpose);
            const auto restored = Artifact::restoreRenderContext(snapshot);
            ASSERT_TRUE(restored);
            EXPECT_EQ(restored->interactive, context->interactive)
                << "restoring the context should preserve purpose-gated interaction";
            EXPECT_FLOAT_EQ(restored->resolutionScale, 0.5f);
        }
    }
}

TEST(RenderContextLodContractTest,
     CacheKeyDistinguishesScalesWithDifferentPixelSizes)
{
    const auto& registry = Artifact::RenderContextRegistry::instance();
    constexpr int baseWidth = 4096;
    constexpr float firstScale = 0.5000f;
    constexpr float secondScale = 0.5004f;
    ASSERT_NE(static_cast<int>(baseWidth * firstScale),
              static_cast<int>(baseWidth * secondScale));

    const QString firstKey = registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive,
        QStringLiteral("lod-cache-precision"), 17, firstScale);
    const QString secondKey = registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive,
        QStringLiteral("lod-cache-precision"), 17, secondScale);

    EXPECT_NE(firstKey, secondKey);

    Artifact::RenderContext firstContext;
    firstContext.currentFrame = 17;
    firstContext.setResolutionScale(firstScale);
    const auto firstSnapshot = Artifact::createRenderContextSnapshot(
        firstContext, Artifact::RenderPurpose::EditorInteractive, firstKey);
    Artifact::RenderContext secondContext;
    secondContext.currentFrame = 17;
    secondContext.setResolutionScale(secondScale);
    const auto secondSnapshot = Artifact::createRenderContextSnapshot(
        secondContext, Artifact::RenderPurpose::EditorInteractive, secondKey);

    EXPECT_EQ(registry.registerSnapshot(firstSnapshot), firstKey);
    EXPECT_EQ(registry.registerSnapshot(secondSnapshot), secondKey);
    ASSERT_TRUE(registry.contains(firstKey));
    ASSERT_TRUE(registry.contains(secondKey));
    EXPECT_FLOAT_EQ(registry.snapshot(firstKey).resolutionScale, firstScale);
    EXPECT_FLOAT_EQ(registry.snapshot(secondKey).resolutionScale, secondScale);
}

TEST(RenderContextLodContractTest,
     CacheKeyDistinguishesScalesThatShareIntegerPixelDimensions)
{
    const auto& registry = Artifact::RenderContextRegistry::instance();
    constexpr int baseWidth = 1920;
    constexpr float firstScale = 0.5000f;
    constexpr float secondScale = 0.5001f;
    ASSERT_EQ(static_cast<int>(baseWidth * firstScale),
              static_cast<int>(baseWidth * secondScale));

    const QString firstKey = registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive,
        QStringLiteral("lod-cache-same-size"), 17, firstScale);
    const QString secondKey = registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive,
        QStringLiteral("lod-cache-same-size"), 17, secondScale);

    EXPECT_NE(firstKey, secondKey)
        << "resolution-scaled effect parameters can differ even when output "
           "dimensions truncate to the same integer size";
}

TEST(RenderContextLodContractTest,
     CacheKeysSeparateLODLevelsPurposeOwnerAndFrame)
{
    const auto& registry = Artifact::RenderContextRegistry::instance();
    const QString owner = QStringLiteral("lod-cache-identity");

    const QString lowKey = registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive, owner, 23, 0.5f);
    const QString mediumKey = registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive, owner, 23, 0.75f);
    const QString highKey = registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive, owner, 23, 1.0f);

    EXPECT_EQ(lowKey, registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive, owner, 23, 0.5f));
    EXPECT_NE(lowKey, mediumKey);
    EXPECT_NE(mediumKey, highKey);
    EXPECT_NE(lowKey, highKey);
    EXPECT_NE(lowKey, registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive,
        QStringLiteral("other-lod-cache-owner"), 23, 0.5f));
    EXPECT_NE(lowKey, registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive, owner, 24, 0.5f));
    EXPECT_NE(lowKey, registry.makeKey(
        Artifact::RenderPurpose::FinalExport, owner, 23, 0.5f));

    const float lodScales[] = {0.5f, 0.75f, 1.0f};
    for (int index = 0; index < 3; ++index) {
        Artifact::RenderContext context;
        context.currentFrame = 23;
        context.setResolutionScale(lodScales[index]);
        const QString key = registry.makeKey(
            Artifact::RenderPurpose::EditorInteractive, owner, 23,
            lodScales[index]);
        const auto snapshot = Artifact::createRenderContextSnapshot(
            context, Artifact::RenderPurpose::EditorInteractive, key);
        EXPECT_EQ(registry.registerSnapshot(snapshot), key);
        ASSERT_TRUE(registry.contains(key));
        const auto restored = registry.snapshot(key);
        EXPECT_EQ(restored.purpose,
                  Artifact::RenderPurpose::EditorInteractive);
        EXPECT_EQ(restored.currentFrame, 23);
        EXPECT_FLOAT_EQ(restored.resolutionScale, lodScales[index]);
    }
}

TEST(RenderContextLodContractTest,
     CacheKeysSeparateEveryRenderPurposeAtTheSameLODScale)
{
    constexpr std::array<Artifact::RenderPurpose, 5> purposes{
        Artifact::RenderPurpose::EditorInteractive,
        Artifact::RenderPurpose::EditorPreview,
        Artifact::RenderPurpose::FinalExport,
        Artifact::RenderPurpose::ProxyBuild,
        Artifact::RenderPurpose::Thumbnail};
    const auto& registry = Artifact::RenderContextRegistry::instance();
    constexpr float lodScale = 0.5f;
    const QString owner = QStringLiteral("render-purpose-lod-cache");
    std::array<QString, purposes.size()> keys;

    for (std::size_t index = 0; index < purposes.size(); ++index) {
        keys[index] = registry.makeKey(purposes[index], owner, 31, lodScale);
        EXPECT_FALSE(keys[index].isEmpty());
        for (std::size_t previous = 0; previous < index; ++previous) {
            EXPECT_NE(keys[index], keys[previous])
                << "render purpose " << static_cast<int>(purposes[index])
                << " must not reuse the LOD cache key for purpose "
                << static_cast<int>(purposes[previous]);
        }

        Artifact::RenderContext context;
        context.currentFrame = 31;
        context.setResolutionScale(lodScale);
        const auto snapshot = Artifact::createRenderContextSnapshot(
            context, purposes[index], keys[index]);
        EXPECT_EQ(registry.registerSnapshot(snapshot), keys[index]);
        ASSERT_TRUE(registry.contains(keys[index]));
        const auto restored = registry.snapshot(keys[index]);
        EXPECT_EQ(restored.purpose, purposes[index]);
        EXPECT_EQ(restored.currentFrame, 31);
        EXPECT_FLOAT_EQ(restored.resolutionScale, lodScale);
    }
}

TEST(RenderContextLodContractTest,
     ClearingRegistryRemovesEveryCachedLODAndUsesRequestedFallback)
{
    auto& registry = Artifact::RenderContextRegistry::instance();
    struct RegistryCleanup final {
        Artifact::RenderContextRegistry& registry;
        ~RegistryCleanup() { registry.clear(); }
    } cleanup{registry};
    registry.clear();
    const QString owner = QStringLiteral("lod-cache-clear-contract");
    const std::array<float, 3> scales{0.5f, 0.75f, 1.0f};
    std::array<QString, 3> keys;

    for (std::size_t index = 0; index < scales.size(); ++index) {
        Artifact::RenderContext context;
        context.currentFrame = 37;
        context.setResolutionScale(scales[index]);
        keys[index] = registry.makeKey(
            Artifact::RenderPurpose::EditorInteractive, owner, 37, scales[index]);
        const auto snapshot = Artifact::createRenderContextSnapshot(
            context, Artifact::RenderPurpose::EditorInteractive, keys[index]);
        EXPECT_EQ(registry.registerSnapshot(snapshot), keys[index]);
        ASSERT_TRUE(registry.contains(keys[index]));
    }

    registry.clear();
    for (std::size_t index = 0; index < scales.size(); ++index) {
        EXPECT_FALSE(registry.contains(keys[index]))
            << "clearing render contexts should remove stale LOD snapshots";

        Artifact::RenderContextSnapshot fallback;
        fallback.purpose = Artifact::RenderPurpose::FinalExport;
        fallback.currentFrame = 37;
        fallback.resolutionScale = scales[index];
        const auto restored = registry.snapshotOrDefault(keys[index], fallback);
        EXPECT_EQ(restored.purpose, Artifact::RenderPurpose::FinalExport);
        EXPECT_EQ(restored.currentFrame, 37);
        EXPECT_FLOAT_EQ(restored.resolutionScale, scales[index]);
    }
}

TEST(RenderContextLodContractTest,
     MissingLODCacheKeyReturnsTheRequestedQualityFallback)
{
    const auto& registry = Artifact::RenderContextRegistry::instance();
    const QString owner = QStringLiteral("lod-cache-fallback-contract");
    const QString lowKey = registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive, owner, 91, 0.5f);
    const QString highKey = registry.makeKey(
        Artifact::RenderPurpose::EditorInteractive, owner, 91, 1.0f);

    Artifact::RenderContext lowContext;
    lowContext.currentFrame = 91;
    lowContext.setResolutionScale(0.5f);
    const auto lowSnapshot = Artifact::createRenderContextSnapshot(
        lowContext, Artifact::RenderPurpose::EditorInteractive, lowKey);
    ASSERT_EQ(registry.registerSnapshot(lowSnapshot), lowKey);

    Artifact::RenderContextSnapshot highFallback;
    highFallback.purpose = Artifact::RenderPurpose::EditorInteractive;
    highFallback.currentFrame = 91;
    highFallback.resolutionScale = 1.0f;

    const auto cachedLow = registry.snapshotOrDefault(lowKey, highFallback);
    EXPECT_FLOAT_EQ(cachedLow.resolutionScale, 0.5f);
    EXPECT_EQ(cachedLow.currentFrame, 91);

    const auto missingHigh = registry.snapshotOrDefault(highKey, highFallback);
    EXPECT_FLOAT_EQ(missingHigh.resolutionScale, 1.0f);
    EXPECT_EQ(missingHigh.currentFrame, 91);
    EXPECT_EQ(missingHigh.purpose,
              Artifact::RenderPurpose::EditorInteractive);
}
