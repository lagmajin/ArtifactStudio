#include <gtest/gtest.h>

#include <QRectF>
#include <QImage>
#include <QSize>
#include <QString>

#include <array>

#include "DiligentHeadlessRenderer.hpp"

import Artifact.Effect.Glow;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.MpmSand2D;
import Artifact.LOD.Manager;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.Context;
import Artifact.Render.IRenderer;
import Color.Float;
import Composition3D;
import Memory.SharedPtr;
import Utils.Id;
import Utils.String.UniString;

TEST(MpmSandLodSurfaceContractTest,
     SoftwareSurfaceUsesTheRequestedLowMediumAndHighResolution)
{
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("MPM LOD Surface")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(640, 480);
    params.setDurationFrames(2);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("mpm-lod-surface")), params);
    ASSERT_TRUE(composition);

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactMpmSand2DLayer>();
    ASSERT_TRUE(layer);
    ASSERT_TRUE(composition->appendLayerTop(layer).success);
    ASSERT_TRUE(layer->isCompositionAttachmentReady());

    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(2.0f);
    glow->setGlowGain(0.75f);
    layer->addEffect(glow);

    struct ResolutionCase {
        QSize requested;
        QSize expected;
    };
    const std::array<ResolutionCase, 3> cases{{
        {QSize(320, 240), QSize(320, 240)},
        {QSize(480, 360), QSize(480, 360)},
        {QSize(640, 480), QSize(640, 480)},
    }};

    for (const ResolutionCase& testCase : cases) {
        QRectF outputBounds;
        const QImage surface = layer->getSoftwareRenderSurface(
            testCase.requested.width(), testCase.requested.height(),
            &outputBounds, true);
        ASSERT_FALSE(surface.isNull());
        EXPECT_EQ(surface.size(), testCase.expected);
        EXPECT_EQ(outputBounds, layer->particleRenderBounds());
    }

    const QImage boundedSurface = layer->getSoftwareRenderSurface(
        8192, 4096, nullptr, true);
    ASSERT_FALSE(boundedSurface.isNull());
    EXPECT_GT(boundedSurface.width(), 0);
    EXPECT_GT(boundedSurface.height(), 0);
    EXPECT_LE(boundedSurface.width(), 4096);
    EXPECT_LE(boundedSurface.height(), 4096);
    EXPECT_LE(static_cast<qint64>(boundedSurface.width()) *
                  static_cast<qint64>(boundedSurface.height()),
              4 * 1024 * 1024);
    EXPECT_TRUE(layer->getSoftwareRenderSurface(0, 4096, nullptr, true).isNull());

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(640, 480)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }
    renderer.setCanvasSize(640.0f, 480.0f);
    renderer.setViewportRect(640.0f, 480.0f);
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    Artifact::RenderContextRegistry::instance().clear();

    struct DetailCase {
        Artifact::DetailLevel detail;
        float expectedScale;
    };
    const std::array<DetailCase, 3> details{{
        {Artifact::DetailLevel::Low, 0.5f},
        {Artifact::DetailLevel::Medium, 0.75f},
        {Artifact::DetailLevel::High, 1.0f},
    }};
    constexpr DetailCase unknownDetail{
        static_cast<Artifact::DetailLevel>(99), 1.0f};
    const auto renderAtDetail = [&](const DetailCase& detailCase) {
        renderer.setDetailLevel(detailCase.detail);
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, detailCase.detail);
        renderer.flushAndWait();
        const QString key = Artifact::RenderContextRegistry::instance().makeKey(
            Artifact::RenderPurpose::EditorPreview, layer->id().toString(),
            layer->currentFrame(), detailCase.expectedScale);
        const auto& registry = Artifact::RenderContextRegistry::instance();
        EXPECT_TRUE(registry.contains(key));
        if (registry.contains(key)) {
            const auto snapshot = registry.snapshot(key);
            EXPECT_EQ(snapshot.purpose, Artifact::RenderPurpose::EditorPreview);
            EXPECT_FLOAT_EQ(snapshot.resolutionScale, detailCase.expectedScale);
        }
        return renderer.readbackToImage();
    };

    const QImage low = renderAtDetail(details[0]);
    const QImage medium = renderAtDetail(details[1]);
    const QImage high = renderAtDetail(details[2]);
    const QImage unknown = renderAtDetail(unknownDetail);
    const QImage restoredLow = renderAtDetail(details[0]);
    const QImage restoredMedium = renderAtDetail(details[1]);
    const QImage restoredHigh = renderAtDetail(details[2]);
    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(unknown.isNull());
    ASSERT_FALSE(restoredLow.isNull());
    ASSERT_FALSE(restoredMedium.isNull());
    ASSERT_FALSE(restoredHigh.isNull());
    const auto changedPixels = [](const QImage& first, const QImage& second) {
        if (first.size() != second.size()) return -1;
        int changed = 0;
        for (int y = 0; y < first.height(); ++y) {
            for (int x = 0; x < first.width(); ++x) {
                if (first.pixel(x, y) != second.pixel(x, y)) ++changed;
            }
        }
        return changed;
    };
    EXPECT_GT(changedPixels(low, medium), 0)
        << "MPM's software surface should resolve differently at Low and Medium";
    EXPECT_GT(changedPixels(medium, high), 0)
        << "MPM's software surface should resolve differently at Medium and High";
    EXPECT_EQ(changedPixels(high, unknown), 0)
        << "an unknown detail level should use full-resolution MPM output";
    EXPECT_EQ(changedPixels(low, restoredLow), 0)
        << "returning to Low after visiting other LODs should reproduce its prior MPM surface";
    EXPECT_EQ(changedPixels(medium, restoredMedium), 0)
        << "returning to Medium after visiting other LODs should reproduce its prior MPM surface";
    EXPECT_EQ(changedPixels(high, restoredHigh), 0)
        << "returning to High should reproduce its prior MPM surface";

    const auto renderOfflineAtDetail = [&](const DetailCase& detailCase) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
            0, true, detailCase.detail);
        renderer.flushAndWait();
        const QString key = Artifact::RenderContextRegistry::instance().makeKey(
            Artifact::RenderPurpose::FinalExport, layer->id().toString(),
            layer->currentFrame(), detailCase.expectedScale);
        const auto& registry = Artifact::RenderContextRegistry::instance();
        EXPECT_TRUE(registry.contains(key));
        if (registry.contains(key)) {
            const auto snapshot = registry.snapshot(key);
            EXPECT_EQ(snapshot.purpose, Artifact::RenderPurpose::FinalExport);
            EXPECT_FALSE(snapshot.interactive);
            EXPECT_FLOAT_EQ(snapshot.resolutionScale, detailCase.expectedScale);
        }
        return renderer.readbackToImage();
    };

    const QImage offlineLow = renderOfflineAtDetail(details[0]);
    const QImage offlineMedium = renderOfflineAtDetail(details[1]);
    const QImage offlineHigh = renderOfflineAtDetail(details[2]);
    const QImage offlineUnknown = renderOfflineAtDetail(unknownDetail);
    const QImage offlineRestoredLow = renderOfflineAtDetail(details[0]);
    ASSERT_FALSE(offlineLow.isNull());
    ASSERT_FALSE(offlineMedium.isNull());
    ASSERT_FALSE(offlineHigh.isNull());
    ASSERT_FALSE(offlineUnknown.isNull());
    ASSERT_FALSE(offlineRestoredLow.isNull());
    EXPECT_GT(changedPixels(offlineLow, offlineMedium), 0)
        << "the requested offline resolution LOD should change MPM sampling";
    EXPECT_GT(changedPixels(offlineMedium, offlineHigh), 0)
        << "Medium and High offline MPM scales should remain distinct";
    EXPECT_EQ(changedPixels(offlineHigh, offlineUnknown), 0)
        << "an unknown detail level should use full-resolution offline MPM output";
    EXPECT_EQ(changedPixels(offlineLow, offlineRestoredLow), 0)
        << "returning to Low should reproduce the offline MPM output";

    renderer.setDetailLevel(Artifact::DetailLevel::High);
}
