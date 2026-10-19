#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>

#include "DiligentHeadlessRenderer.hpp"

import Artifact.Effect.Glow;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Shape;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.Context;
import Artifact.Render.IRenderer;
import Color.Float;
import Memory.SharedPtr;
import Composition3D;
import Utils.Id;
import Utils.String.UniString;

namespace {

bool imagesMatchExactly(const QImage& left, const QImage& right)
{
    if (left.isNull() || right.isNull() || left.size() != right.size()) {
        return false;
    }

    const QImage leftRgba = left.convertToFormat(QImage::Format_RGBA8888);
    const QImage rightRgba = right.convertToFormat(QImage::Format_RGBA8888);
    const std::size_t rowBytes =
        static_cast<std::size_t>(leftRgba.width()) * 4u;
    for (int row = 0; row < leftRgba.height(); ++row) {
        if (std::memcmp(leftRgba.constScanLine(row),
                        rightRgba.constScanLine(row), rowBytes) != 0) {
            return false;
        }
    }
    return true;
}

int visiblePixelCount(const QImage& image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 8) {
                ++count;
            }
        }
    }
    return count;
}

} // namespace

TEST(LayerSurfaceCacheParityContractTest,
     CachedRasterizerLayerMatchesCacheBypassPixels)
{
    constexpr int width = 96;
    constexpr int height = 96;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QImage source(48, 48, QImage::Format_RGBA8888);
    source.fill(Qt::transparent);
    for (int y = 14; y < 34; ++y) {
        for (int x = 14; x < 34; ++x) {
            source.setPixelColor(x, y, QColor(255, 205, 90, 255));
        }
    }

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(layer);
    layer->setLayerName(QStringLiteral("Cache Parity Image"));
    layer->setFromQImage(source);
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    // Keep the comparison focused on the explicit layer surface cache. This
    // disables the independent process-wide static GPU layer cache.
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    glow->setBaseSigma(3.0f);
    glow->setGlowGain(1.0f);
    layer->addEffect(glow);

    const auto draw = [&](QHash<QString, Artifact::LayerSurfaceCacheEntry>* cache,
                          const Artifact::DetailLevel lod) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, cache, nullptr, 0,
            false, lod);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };

    QHash<QString, Artifact::LayerSurfaceCacheEntry> surfaceCache;
    const QImage cacheBuildImage =
        draw(&surfaceCache, Artifact::DetailLevel::High);
    ASSERT_FALSE(cacheBuildImage.isNull());
    ASSERT_FALSE(surfaceCache.isEmpty());
    const QString cacheOwnerId = layer->id().toString();
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto builtCacheEntry = surfaceCache.value(cacheOwnerId);
    ASSERT_FALSE(builtCacheEntry.cacheSignature.isEmpty());
    const auto processedSize = [](const Artifact::LayerSurfaceCacheEntry& entry) {
        if (entry.processedBuffer) {
            return QSize(entry.processedBuffer->width(),
                         entry.processedBuffer->height());
        }
        return entry.processedSurface.size();
    };
    EXPECT_EQ(processedSize(builtCacheEntry), QSize(48, 48));

    const QImage cacheReuseImage =
        draw(&surfaceCache, Artifact::DetailLevel::High);
    ASSERT_FALSE(cacheReuseImage.isNull());
    const auto reusedCacheEntry = surfaceCache.value(cacheOwnerId);
    const QImage bypassImage = draw(nullptr, Artifact::DetailLevel::High);
    ASSERT_FALSE(bypassImage.isNull());

    EXPECT_TRUE(imagesMatchExactly(cacheBuildImage, bypassImage));
    EXPECT_TRUE(imagesMatchExactly(cacheReuseImage, bypassImage));
    const bool reusedBuffer = builtCacheEntry.processedBuffer &&
        reusedCacheEntry.processedBuffer &&
        builtCacheEntry.processedBuffer.get() ==
            reusedCacheEntry.processedBuffer.get();
    const bool reusedSurface = !builtCacheEntry.processedSurface.isNull() &&
        builtCacheEntry.processedSurface.cacheKey() ==
            reusedCacheEntry.processedSurface.cacheKey();
    EXPECT_TRUE(reusedBuffer || reusedSurface);

    const QImage lowDetailCacheBuild =
        draw(&surfaceCache, Artifact::DetailLevel::Low);
    ASSERT_FALSE(lowDetailCacheBuild.isNull());
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto lowDetailCacheEntry = surfaceCache.value(cacheOwnerId);
    EXPECT_EQ(processedSize(lowDetailCacheEntry), QSize(24, 24));
    EXPECT_NE(lowDetailCacheEntry.cacheSignature,
              builtCacheEntry.cacheSignature);

    const QImage lowDetailCacheReuse =
        draw(&surfaceCache, Artifact::DetailLevel::Low);
    const QImage lowDetailBypass =
        draw(nullptr, Artifact::DetailLevel::Low);
    ASSERT_FALSE(lowDetailCacheReuse.isNull());
    ASSERT_FALSE(lowDetailBypass.isNull());
    EXPECT_TRUE(imagesMatchExactly(lowDetailCacheBuild, lowDetailBypass));
    EXPECT_TRUE(imagesMatchExactly(lowDetailCacheReuse, lowDetailBypass));

    const QImage mediumDetailCacheBuild =
        draw(&surfaceCache, Artifact::DetailLevel::Medium);
    ASSERT_FALSE(mediumDetailCacheBuild.isNull());
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto mediumDetailCacheEntry = surfaceCache.value(cacheOwnerId);
    EXPECT_EQ(processedSize(mediumDetailCacheEntry), QSize(36, 36));
    EXPECT_NE(mediumDetailCacheEntry.cacheSignature,
              builtCacheEntry.cacheSignature);
    EXPECT_NE(mediumDetailCacheEntry.cacheSignature,
              lowDetailCacheEntry.cacheSignature);

    const QImage mediumDetailCacheReuse =
        draw(&surfaceCache, Artifact::DetailLevel::Medium);
    const QImage mediumDetailBypass =
        draw(nullptr, Artifact::DetailLevel::Medium);
    ASSERT_FALSE(mediumDetailCacheReuse.isNull());
    ASSERT_FALSE(mediumDetailBypass.isNull());
    EXPECT_TRUE(imagesMatchExactly(mediumDetailCacheBuild, mediumDetailBypass));
    EXPECT_TRUE(imagesMatchExactly(mediumDetailCacheReuse, mediumDetailBypass));

    const QImage restoredLowCacheBuild =
        draw(&surfaceCache, Artifact::DetailLevel::Low);
    ASSERT_FALSE(restoredLowCacheBuild.isNull());
    const auto restoredLowCacheEntry = surfaceCache.value(cacheOwnerId);
    EXPECT_EQ(processedSize(restoredLowCacheEntry), QSize(24, 24));
    EXPECT_EQ(restoredLowCacheEntry.cacheSignature,
              lowDetailCacheEntry.cacheSignature);
    EXPECT_TRUE(imagesMatchExactly(restoredLowCacheBuild, lowDetailCacheBuild));
    const QImage restoredLowCacheReuse =
        draw(&surfaceCache, Artifact::DetailLevel::Low);
    const QImage restoredLowBypass =
        draw(nullptr, Artifact::DetailLevel::Low);
    ASSERT_FALSE(restoredLowCacheReuse.isNull());
    ASSERT_FALSE(restoredLowBypass.isNull());
    EXPECT_TRUE(imagesMatchExactly(restoredLowCacheReuse, restoredLowBypass));

    const QImage restoredHighCacheBuild =
        draw(&surfaceCache, Artifact::DetailLevel::High);
    ASSERT_FALSE(restoredHighCacheBuild.isNull());
    const auto restoredHighCacheEntry = surfaceCache.value(cacheOwnerId);
    EXPECT_EQ(processedSize(restoredHighCacheEntry), QSize(48, 48));
    EXPECT_EQ(restoredHighCacheEntry.cacheSignature,
              builtCacheEntry.cacheSignature);
    const QImage restoredHighCacheReuse =
        draw(&surfaceCache, Artifact::DetailLevel::High);
    const QImage restoredHighBypass =
        draw(nullptr, Artifact::DetailLevel::High);
    ASSERT_FALSE(restoredHighCacheReuse.isNull());
    ASSERT_FALSE(restoredHighBypass.isNull());
    EXPECT_TRUE(imagesMatchExactly(restoredHighCacheBuild, cacheBuildImage));
    EXPECT_TRUE(imagesMatchExactly(restoredHighCacheReuse, restoredHighBypass));

}

TEST(LayerSurfaceCacheParityContractTest,
     StaticGpuSurfaceCacheSeparatesResolutionLODsAndReusesCurrentLevel)
{
    constexpr int width = 96;
    constexpr int height = 96;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    struct StaticCacheReset final {
        ~StaticCacheReset() { Artifact::clearStaticLayerGpuCache(); }
    } resetStaticCache;
    Artifact::clearStaticLayerGpuCache();

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QImage source(48, 48, QImage::Format_RGBA8888);
    source.fill(Qt::transparent);
    for (int y = 12; y < 36; ++y) {
        for (int x = 12; x < 36; ++x) {
            source.setPixelColor(x, y, QColor(255, 210, 90, 255));
        }
    }

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(layer);
    layer->setLayerName(QStringLiteral("Static LOD Cache Image"));
    layer->setFromQImage(source);
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Enabled);
    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(4.0f);
    glow->setGlowGain(1.0f);
    layer->addEffect(glow);
    ASSERT_TRUE(Artifact::layerUsesStaticLayerGpuCacheForCompositionView(
        layer.get()));

    const auto drawAtDetail = [&](const Artifact::DetailLevel detail) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, detail);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };

    const QImage low = drawAtDetail(Artifact::DetailLevel::Low);
    const auto afterLowBuild = Artifact::staticLayerGpuCacheDiagnostics();
    const QImage lowHit = drawAtDetail(Artifact::DetailLevel::Low);
    const auto afterLowHit = Artifact::staticLayerGpuCacheDiagnostics();
    const QImage medium = drawAtDetail(Artifact::DetailLevel::Medium);
    const auto afterMediumBuild = Artifact::staticLayerGpuCacheDiagnostics();
    const QImage high = drawAtDetail(Artifact::DetailLevel::High);
    const auto afterHighBuild = Artifact::staticLayerGpuCacheDiagnostics();
    const QImage highHit = drawAtDetail(Artifact::DetailLevel::High);
    const auto afterHighHit = Artifact::staticLayerGpuCacheDiagnostics();
    const QImage unknownDetail = drawAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const auto afterUnknownDetail = Artifact::staticLayerGpuCacheDiagnostics();
    const QImage highAfterUnknown = drawAtDetail(Artifact::DetailLevel::High);
    const QImage lowAfterHigh = drawAtDetail(Artifact::DetailLevel::Low);
    const auto afterLowReturn = Artifact::staticLayerGpuCacheDiagnostics();

    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(lowHit.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(highHit.isNull());
    ASSERT_FALSE(unknownDetail.isNull());
    ASSERT_FALSE(highAfterUnknown.isNull());
    ASSERT_FALSE(lowAfterHigh.isNull());
    EXPECT_EQ(afterLowBuild.entryCount, 1);
    EXPECT_GE(afterLowBuild.missCount, 1u);
    EXPECT_GT(afterLowHit.hitCount, afterLowBuild.hitCount);
    EXPECT_GT(afterMediumBuild.missCount, afterLowHit.missCount)
        << "Medium resolution should not reuse the Low-resolution cache entry";
    EXPECT_GT(afterHighBuild.missCount, afterMediumBuild.missCount)
        << "High resolution should not reuse the Medium-resolution cache entry";
    EXPECT_GT(afterHighHit.hitCount, afterHighBuild.hitCount);
    EXPECT_GT(afterUnknownDetail.hitCount, afterHighHit.hitCount)
        << "unknown detail should reuse the High-resolution cache entry";
    EXPECT_GT(afterLowReturn.hitCount, afterUnknownDetail.hitCount)
        << "returning to Low after a High draw should reuse the Low cache entry";
    EXPECT_EQ(afterLowReturn.missCount, afterUnknownDetail.missCount)
        << "switching back to Low should not rebuild or alias a detail cache entry";
    EXPECT_EQ(afterUnknownDetail.missCount, afterHighHit.missCount)
        << "unknown detail should not create a duplicate High cache entry";
    EXPECT_TRUE(imagesMatchExactly(low, lowHit));
    EXPECT_TRUE(imagesMatchExactly(high, highHit));
    EXPECT_TRUE(imagesMatchExactly(high, unknownDetail))
        << "unknown detail should use the full-resolution cached surface";
    EXPECT_TRUE(imagesMatchExactly(high, highAfterUnknown));
    EXPECT_TRUE(imagesMatchExactly(low, lowAfterHigh))
        << "returning to Low after High should reproduce the original Low output";

    const std::uint64_t previousEffectRevision = layer->effectRevision();
    auto additionalGlow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(additionalGlow);
    additionalGlow->setBaseSigma(8.0f);
    additionalGlow->setGlowGain(1.0f);
    layer->addEffect(additionalGlow);
    EXPECT_GT(layer->effectRevision(), previousEffectRevision)
        << "effect edits should advance the cache identity revision";
    const QImage lowAfterEffectChange = drawAtDetail(Artifact::DetailLevel::Low);
    const auto afterLowEffectChange = Artifact::staticLayerGpuCacheDiagnostics();
    const QImage lowAfterEffectChangeHit = drawAtDetail(Artifact::DetailLevel::Low);
    const auto afterLowEffectChangeHit = Artifact::staticLayerGpuCacheDiagnostics();
    const QImage mediumAfterEffectChange = drawAtDetail(Artifact::DetailLevel::Medium);
    const auto afterMediumEffectChange = Artifact::staticLayerGpuCacheDiagnostics();
    const QImage mediumAfterEffectChangeHit =
        drawAtDetail(Artifact::DetailLevel::Medium);
    const auto afterMediumEffectChangeHit =
        Artifact::staticLayerGpuCacheDiagnostics();
    const QImage highAfterEffectChange = drawAtDetail(Artifact::DetailLevel::High);
    const auto afterHighEffectChange = Artifact::staticLayerGpuCacheDiagnostics();
    const QImage highAfterEffectChangeHit = drawAtDetail(Artifact::DetailLevel::High);
    const auto afterHighEffectChangeHit = Artifact::staticLayerGpuCacheDiagnostics();

    ASSERT_FALSE(lowAfterEffectChange.isNull());
    ASSERT_FALSE(lowAfterEffectChangeHit.isNull());
    ASSERT_FALSE(mediumAfterEffectChange.isNull());
    ASSERT_FALSE(mediumAfterEffectChangeHit.isNull());
    ASSERT_FALSE(highAfterEffectChange.isNull());
    ASSERT_FALSE(highAfterEffectChangeHit.isNull());
    EXPECT_GT(afterLowEffectChange.missCount, afterHighHit.missCount)
        << "changing an effect should rebuild the Low-resolution cache entry";
    EXPECT_GT(afterLowEffectChangeHit.hitCount, afterLowEffectChange.hitCount);
    EXPECT_GT(afterMediumEffectChange.missCount,
              afterLowEffectChangeHit.missCount)
        << "changing an effect should rebuild the Medium-resolution entry";
    EXPECT_GT(afterMediumEffectChangeHit.hitCount,
              afterMediumEffectChange.hitCount);
    EXPECT_GT(afterHighEffectChange.missCount,
              afterMediumEffectChangeHit.missCount)
        << "changing an effect should rebuild the High-resolution entry";
    EXPECT_GT(afterHighEffectChangeHit.hitCount, afterHighEffectChange.hitCount);
    EXPECT_FALSE(imagesMatchExactly(low, lowAfterEffectChange))
        << "a changed effect should not return the previous Low-LOD pixels";
    EXPECT_FALSE(imagesMatchExactly(medium, mediumAfterEffectChange))
        << "a changed effect should not return the previous Medium-LOD pixels";
    EXPECT_FALSE(imagesMatchExactly(high, highAfterEffectChange))
        << "a changed effect should not return the previous High-LOD pixels";
    EXPECT_TRUE(imagesMatchExactly(lowAfterEffectChange,
                                   lowAfterEffectChangeHit));
    EXPECT_TRUE(imagesMatchExactly(mediumAfterEffectChange,
                                   mediumAfterEffectChangeHit));
    EXPECT_TRUE(imagesMatchExactly(highAfterEffectChange,
                                   highAfterEffectChangeHit));

    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    const QImage highCacheBypass = drawAtDetail(Artifact::DetailLevel::High);
    ASSERT_FALSE(highCacheBypass.isNull());
    EXPECT_TRUE(imagesMatchExactly(highAfterEffectChange, highCacheBypass))
        << "the rebuilt cached High surface should match the uncached result";
}

TEST(LayerSurfaceCacheParityContractTest,
     LargeImageRasterizerGlowMatchesCachedAndBypassOutput)
{
    constexpr int width = 1280;
    constexpr int height = 720;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QImage source(width, height, QImage::Format_RGBA8888);
    source.fill(Qt::transparent);
    for (int y = 320; y < 400; ++y) {
        for (int x = 600; x < 680; ++x) {
            source.setPixelColor(x, y, QColor(255, 210, 100, 255));
        }
    }

    auto effectedLayer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    auto baselineLayer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(effectedLayer);
    ASSERT_TRUE(baselineLayer);
    effectedLayer->setLayerName(QStringLiteral("Large Glow Image"));
    baselineLayer->setLayerName(QStringLiteral("Large Baseline Image"));
    effectedLayer->setFromQImage(source);
    baselineLayer->setFromQImage(source);
    ASSERT_TRUE(effectedLayer->hasCurrentFrameBuffer());
    ASSERT_TRUE(baselineLayer->hasCurrentFrameBuffer());
    effectedLayer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    baselineLayer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(12.0f);
    glow->setGlowGain(2.0f);
    effectedLayer->addEffect(glow);

    const auto draw = [&](Artifact::ArtifactImageLayer* layer,
                          QHash<QString, Artifact::LayerSurfaceCacheEntry>* cache) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer, &renderer, 1.0f, nullptr, cache, nullptr, 0);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };

    QHash<QString, Artifact::LayerSurfaceCacheEntry> surfaceCache;
    const QImage cachedEffect = draw(effectedLayer.get(), &surfaceCache);
    ASSERT_FALSE(cachedEffect.isNull());
    EXPECT_EQ(cachedEffect.width(), width);
    EXPECT_EQ(cachedEffect.height(), height);
    const QImage bypassEffect = draw(effectedLayer.get(), nullptr);
    ASSERT_FALSE(bypassEffect.isNull());
    const QImage baseline = draw(baselineLayer.get(), nullptr);
    ASSERT_FALSE(baseline.isNull());

    EXPECT_TRUE(imagesMatchExactly(cachedEffect, bypassEffect));
    EXPECT_FALSE(imagesMatchExactly(cachedEffect, baseline));
}

TEST(LayerSurfaceCacheParityContractTest,
     RasterizerEffectOutputChangesAcrossAllDetailLevelsAndRestoresHigh)
{
    constexpr int width = 96;
    constexpr int height = 96;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QImage source(48, 48, QImage::Format_RGBA8888);
    source.fill(Qt::transparent);
    for (int y = 12; y < 36; ++y) {
        for (int x = 12; x < 36; ++x) {
            source.setPixelColor(x, y, QColor(255, 210, 90, 255));
        }
    }

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(layer);
    layer->setLayerName(QStringLiteral("LOD Glow Image"));
    layer->setFromQImage(source);
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(5.0f);
    glow->setGlowGain(1.5f);
    layer->addEffect(glow);

    const auto draw = [&](const Artifact::DetailLevel lod) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, lod);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };

    const QImage low = draw(Artifact::DetailLevel::Low);
    const QImage medium = draw(Artifact::DetailLevel::Medium);
    const QImage high = draw(Artifact::DetailLevel::High);
    const QImage restoredHigh = draw(Artifact::DetailLevel::High);

    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(restoredHigh.isNull());
    EXPECT_EQ(low.size(), QSize(width, height));
    EXPECT_EQ(medium.size(), high.size());
    EXPECT_FALSE(imagesMatchExactly(low, medium))
        << "Low and Medium should render the rasterizer effect at distinct scales";
    EXPECT_FALSE(imagesMatchExactly(medium, high))
        << "Medium and High should render the rasterizer effect at distinct scales";
    EXPECT_FALSE(imagesMatchExactly(low, high));
    EXPECT_TRUE(imagesMatchExactly(high, restoredHigh))
        << "returning to High should restore the full-resolution effect output";
}

TEST(LayerSurfaceCacheParityContractTest,
     TinyBrightSourceKeepsVisibleGlowAcrossResolutionLODs)
{
    constexpr int width = 96;
    constexpr int height = 96;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QImage source(48, 48, QImage::Format_RGBA8888);
    source.fill(Qt::transparent);
    source.setPixelColor(23, 23, QColor(255, 255, 255, 255));

    auto glowLayer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    auto baselineLayer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(glowLayer);
    ASSERT_TRUE(baselineLayer);
    glowLayer->setLayerName(QStringLiteral("Tiny Bright LOD Glow"));
    baselineLayer->setLayerName(QStringLiteral("Tiny Bright LOD Baseline"));
    glowLayer->setFromQImage(source);
    baselineLayer->setFromQImage(source);
    ASSERT_TRUE(glowLayer->hasCurrentFrameBuffer());
    ASSERT_TRUE(baselineLayer->hasCurrentFrameBuffer());
    glowLayer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    baselineLayer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(10.0f);
    glow->setGlowGain(3.0f);
    glowLayer->addEffect(glow);

    const auto draw = [&](Artifact::ArtifactImageLayer* layer,
                          const Artifact::DetailLevel lod) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer, &renderer, 1.0f, nullptr, nullptr, nullptr, 0, false, lod);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };
    const auto rgbEnergy = [](const QImage& image) {
        double energy = 0.0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor pixel = image.pixelColor(x, y);
                energy += pixel.red() + pixel.green() + pixel.blue();
            }
        }
        return energy;
    };

    for (const Artifact::DetailLevel lod : {Artifact::DetailLevel::Low,
                                             Artifact::DetailLevel::Medium,
                                             Artifact::DetailLevel::High}) {
        const QImage effected = draw(glowLayer.get(), lod);
        const QImage baseline = draw(baselineLayer.get(), lod);
        ASSERT_FALSE(effected.isNull());
        ASSERT_FALSE(baseline.isNull());
        EXPECT_GT(visiblePixelCount(effected), 0)
            << "the tiny bright source and its Glow should remain visible at LOD "
            << static_cast<int>(lod);
        EXPECT_GT(rgbEnergy(effected), rgbEnergy(baseline))
            << "Glow should add RGB energy around the tiny bright source at LOD "
            << static_cast<int>(lod);
        EXPECT_FALSE(imagesMatchExactly(effected, baseline))
            << "Glow should contribute to the output at LOD "
            << static_cast<int>(lod);
    }
}

TEST(LayerSurfaceCacheParityContractTest,
     ImageLayerSamplingChangesAcrossAllDetailLevels)
{
    constexpr int width = 96;
    constexpr int height = 96;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QImage source(48, 48, QImage::Format_RGBA8888);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            source.setPixelColor(x, y, ((x + y) % 2 == 0)
                ? QColor(255, 255, 255, 255)
                : QColor(0, 0, 0, 255));
        }
    }

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(layer);
    layer->setLayerName(QStringLiteral("LOD Sampling Image"));
    layer->setFromQImage(source);
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);

    const auto draw = [&](const Artifact::DetailLevel lod) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, lod);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };

    const QImage highDetail = draw(Artifact::DetailLevel::High);
    const QImage mediumDetail = draw(Artifact::DetailLevel::Medium);
    const QImage lowDetail = draw(Artifact::DetailLevel::Low);
    const QImage unknownDetail = draw(
        static_cast<Artifact::DetailLevel>(99));
    const QImage restoredHighDetail = draw(Artifact::DetailLevel::High);
    const QImage restoredMediumDetail = draw(Artifact::DetailLevel::Medium);
    const QImage restoredLowDetail = draw(Artifact::DetailLevel::Low);

    ASSERT_FALSE(highDetail.isNull());
    ASSERT_FALSE(mediumDetail.isNull());
    ASSERT_FALSE(lowDetail.isNull());
    ASSERT_FALSE(unknownDetail.isNull());
    ASSERT_FALSE(restoredHighDetail.isNull());
    ASSERT_FALSE(restoredMediumDetail.isNull());
    ASSERT_FALSE(restoredLowDetail.isNull());
    EXPECT_EQ(highDetail.size(), QSize(width, height));
    EXPECT_EQ(mediumDetail.size(), highDetail.size());
    EXPECT_EQ(lowDetail.size(), highDetail.size());
    EXPECT_FALSE(imagesMatchExactly(highDetail, mediumDetail))
        << "Medium detail should use a distinct downsampled image surface";
    EXPECT_FALSE(imagesMatchExactly(mediumDetail, lowDetail))
        << "Low and Medium detail should retain distinct sampling levels";
    EXPECT_FALSE(imagesMatchExactly(highDetail, lowDetail))
        << "Low detail should downsample the image layer before drawing";
    EXPECT_TRUE(imagesMatchExactly(highDetail, unknownDetail))
        << "unknown detail values should use full-resolution image sampling";
    EXPECT_TRUE(imagesMatchExactly(highDetail, restoredHighDetail))
        << "returning to High after Low should restore the original image output";
    EXPECT_TRUE(imagesMatchExactly(mediumDetail, restoredMediumDetail))
        << "returning to Medium should reproduce the original image output";
    EXPECT_TRUE(imagesMatchExactly(lowDetail, restoredLowDetail))
        << "returning to Low should reproduce the original image output";
}

TEST(LayerSurfaceCacheParityContractTest,
     OddAndTinySurfacesKeepRoundedLODDimensionsAboveZero)
{
    constexpr int width = 32;
    constexpr int height = 32;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QImage source(5, 3, QImage::Format_RGBA8888);
    source.fill(QColor(210, 120, 50, 255));
    auto layer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(layer);
    layer->setLayerName(QStringLiteral("Odd LOD dimensions"));
    layer->setFromQImage(source);
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(1.0f);
    glow->setGlowGain(0.5f);
    layer->addEffect(glow);

    QHash<QString, Artifact::LayerSurfaceCacheEntry> surfaceCache;
    const auto drawAndGetProcessedSize = [&](const Artifact::DetailLevel lod) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, &surfaceCache, nullptr,
            0, false, lod);
        renderer.flushAndWait();
        const QImage result = renderer.readbackToImage();
        EXPECT_FALSE(result.isNull());
        if (!surfaceCache.contains(layer->id().toString())) {
            ADD_FAILURE() << "LOD surface cache entry was not produced";
            return QSize{};
        }
        const auto entry = surfaceCache.value(layer->id().toString());
        if (entry.processedBuffer) {
            return QSize(entry.processedBuffer->width(),
                         entry.processedBuffer->height());
        }
        return entry.processedSurface.size();
    };

    EXPECT_EQ(drawAndGetProcessedSize(Artifact::DetailLevel::Low), QSize(3, 2));
    EXPECT_EQ(drawAndGetProcessedSize(Artifact::DetailLevel::Medium), QSize(4, 2));
    EXPECT_EQ(drawAndGetProcessedSize(Artifact::DetailLevel::High), QSize(5, 3));
    const QString oddSizeCacheSignature =
        surfaceCache.value(layer->id().toString()).cacheSignature;

    QImage singlePixelSource(1, 1, QImage::Format_RGBA8888);
    singlePixelSource.fill(QColor(80, 180, 220, 255));
    layer->setFromQImage(singlePixelSource);
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    EXPECT_EQ(drawAndGetProcessedSize(Artifact::DetailLevel::Low), QSize(1, 1));
    EXPECT_NE(surfaceCache.value(layer->id().toString()).cacheSignature,
              oddSizeCacheSignature)
        << "Changing source dimensions should invalidate the previous LOD surface";

    const QString singlePixelCacheSignature =
        surfaceCache.value(layer->id().toString()).cacheSignature;
    const QImage beforeSameSizeContentChange = renderer.readbackToImage();
    constexpr std::array<Artifact::DetailLevel, 3> detailLevels{
        Artifact::DetailLevel::Low,
        Artifact::DetailLevel::Medium,
        Artifact::DetailLevel::High};
    std::array<QString, detailLevels.size()> signaturesBeforeContentChange;
    signaturesBeforeContentChange[0] = singlePixelCacheSignature;
    for (std::size_t index = 1; index < detailLevels.size(); ++index) {
        ASSERT_EQ(drawAndGetProcessedSize(detailLevels[index]), QSize(1, 1));
        signaturesBeforeContentChange[index] =
            surfaceCache.value(layer->id().toString()).cacheSignature;
    }

    QImage changedSinglePixelSource(1, 1, QImage::Format_RGBA8888);
    changedSinglePixelSource.fill(QColor(240, 40, 180, 255));
    layer->setFromQImage(changedSinglePixelSource);
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());

    for (std::size_t index = 0; index < detailLevels.size(); ++index) {
        ASSERT_EQ(drawAndGetProcessedSize(detailLevels[index]), QSize(1, 1));
        const QString updatedSignature =
            surfaceCache.value(layer->id().toString()).cacheSignature;
        EXPECT_NE(updatedSignature, signaturesBeforeContentChange[index])
            << "Same-size source content changes must invalidate each "
               "resolution LOD cache; index="
            << index;
    }

    ASSERT_EQ(drawAndGetProcessedSize(Artifact::DetailLevel::Low), QSize(1, 1));
    const QImage changedCachedOutput = renderer.readbackToImage();

    renderer.clear();
    Artifact::drawLayerForCompositionView(
        layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
        0, false, Artifact::DetailLevel::Low);
    renderer.flushAndWait();
    const QImage changedBypassOutput = renderer.readbackToImage();
    ASSERT_FALSE(changedCachedOutput.isNull());
    ASSERT_FALSE(changedBypassOutput.isNull());
    EXPECT_FALSE(imagesMatchExactly(beforeSameSizeContentChange,
                                    changedCachedOutput));
    EXPECT_TRUE(imagesMatchExactly(changedCachedOutput, changedBypassOutput));
}

TEST(LayerSurfaceCacheParityContractTest,
     UnknownDetailLevelFallsBackToFullResolution)
{
    constexpr int width = 32;
    constexpr int height = 32;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QImage source(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            source.setPixelColor(x, y, ((x + y) % 2 == 0)
                ? QColor(230, 170, 80, 255)
                : QColor(30, 90, 210, 255));
        }
    }
    auto layer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(layer);
    layer->setLayerName(QStringLiteral("Unknown LOD Image"));
    layer->setFromQImage(source);
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);

    auto& registry = Artifact::RenderContextRegistry::instance();
    registry.clear();
    constexpr auto unknownDetail = static_cast<Artifact::DetailLevel>(99);

    renderer.clear();
    Artifact::drawLayerForCompositionView(
        layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
        0, false, Artifact::DetailLevel::High);
    renderer.flushAndWait();
    const QImage highOutput = renderer.readbackToImage();
    ASSERT_FALSE(highOutput.isNull());

    renderer.clear();
    Artifact::drawLayerForCompositionView(
        layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
        0, false, unknownDetail);
    renderer.flushAndWait();

    const QString key = registry.makeKey(
        Artifact::RenderPurpose::EditorPreview, layer->id().toString(),
        layer->currentFrame(), 1.0f);
    ASSERT_TRUE(registry.contains(key));
    const auto snapshot = registry.snapshot(key);
    EXPECT_FLOAT_EQ(snapshot.resolutionScale, 1.0f);
    const QImage unknownOutput = renderer.readbackToImage();
    ASSERT_FALSE(unknownOutput.isNull());
    EXPECT_TRUE(imagesMatchExactly(unknownOutput, highOutput))
        << "an unknown detail level should render at the same quality as High";
}

TEST(LayerSurfaceCacheParityContractTest,
     OfflineLODRegistersFinalExportContextAtTheRequestedScale)
{
    constexpr int width = 96;
    constexpr int height = 96;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QImage source(48, 48, QImage::Format_RGBA8888);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            source.setPixelColor(x, y, ((x + y) % 2 == 0)
                ? QColor(255, 255, 255, 255)
                : QColor(0, 0, 0, 255));
        }
    }

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(layer);
    layer->setLayerName(QStringLiteral("Offline LOD Image"));
    layer->setFromQImage(source);
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    Artifact::RenderContextRegistry::instance().clear();

    const auto drawOfflineAt = [&](const Artifact::DetailLevel lod,
                                   const float expectedScale) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
            0, true, lod);
        renderer.flushAndWait();

        const QString key = Artifact::RenderContextRegistry::instance().makeKey(
            Artifact::RenderPurpose::FinalExport, layer->id().toString(), 0,
            expectedScale);
        const auto& registry = Artifact::RenderContextRegistry::instance();
        EXPECT_TRUE(registry.contains(key));
        if (!registry.contains(key)) {
            return QImage{};
        }
        const auto snapshot = registry.snapshot(key);
        EXPECT_EQ(snapshot.purpose, Artifact::RenderPurpose::FinalExport);
        EXPECT_FALSE(snapshot.interactive);
        EXPECT_FLOAT_EQ(snapshot.resolutionScale, expectedScale);
        return renderer.readbackToImage();
    };

    const QImage low = drawOfflineAt(Artifact::DetailLevel::Low, 0.5f);
    const QImage medium = drawOfflineAt(Artifact::DetailLevel::Medium, 0.75f);
    const QImage high = drawOfflineAt(Artifact::DetailLevel::High, 1.0f);
    const QImage unknown = drawOfflineAt(
        static_cast<Artifact::DetailLevel>(99), 1.0f);
    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(unknown.isNull());
    EXPECT_FALSE(imagesMatchExactly(low, medium))
        << "the requested offline resolution LOD should affect image sampling";
    EXPECT_FALSE(imagesMatchExactly(medium, high))
        << "Medium and High offline resolution scales should remain distinct";
    EXPECT_TRUE(imagesMatchExactly(high, unknown))
        << "an unknown detail level should register and render at full "
           "offline resolution";
}

TEST(LayerSurfaceCacheParityContractTest,
     ImageSequenceResolutionLODRestoresHighFrameOutput)
{
    constexpr int width = 64;
    constexpr int height = 64;
    ASSERT_NE(QCoreApplication::instance(), nullptr);
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    QStringList framePaths;
    for (int frame = 0; frame < 2; ++frame) {
        QImage source(width, height, QImage::Format_RGBA8888);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const bool bright = ((x + y + frame * 3) % 2) == 0;
                source.setPixelColor(x, y, bright
                    ? QColor(240, 170, 80, 255)
                    : QColor(24, 64, 180, 255));
            }
        }
        const QString path = temporaryDirectory.filePath(
            QStringLiteral("lod-frame-%1.png").arg(frame));
        ASSERT_TRUE(source.save(path, "PNG"));
        framePaths.append(path);
    }

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(layer);
    ASSERT_TRUE(layer->setImageSequence(framePaths, 0.0));
    ASSERT_TRUE(layer->isImageSequence());
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);

    const auto waitForSequenceFrame = [&](const qint64 expectedFrame) {
        QElapsedTimer sequenceTimeout;
        sequenceTimeout.start();
        while (layer->sequenceCachedFrameIndex() != expectedFrame &&
               sequenceTimeout.elapsed() < 5000) {
            QCoreApplication::processEvents();
            (void)layer->toQImage();
            QThread::msleep(1);
        }
        return layer->sequenceCachedFrameIndex() == expectedFrame;
    };
    ASSERT_TRUE(waitForSequenceFrame(0));
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }
    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const auto renderAtDetail = [&](const Artifact::DetailLevel detail) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, detail);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };

    const QImage low = renderAtDetail(Artifact::DetailLevel::Low);
    const QImage medium = renderAtDetail(Artifact::DetailLevel::Medium);
    const QImage high = renderAtDetail(Artifact::DetailLevel::High);
    const QImage restoredHigh = renderAtDetail(Artifact::DetailLevel::High);
    const QImage unknown = renderAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(restoredHigh.isNull());
    ASSERT_FALSE(unknown.isNull());
    EXPECT_FALSE(imagesMatchExactly(low, high))
        << "sequence image sampling should respond to resolution LOD";
    EXPECT_FALSE(imagesMatchExactly(low, medium))
        << "Low and Medium sequence sampling should remain distinct";
    EXPECT_FALSE(imagesMatchExactly(medium, high))
        << "Medium and High sequence sampling should remain distinct";
    EXPECT_TRUE(imagesMatchExactly(high, restoredHigh))
        << "returning to High should reproduce the sequence frame output";
    EXPECT_TRUE(imagesMatchExactly(high, unknown))
        << "an unknown detail value should use full-resolution sequence sampling";

    layer->goToFrame(1);
    ASSERT_TRUE(waitForSequenceFrame(1));
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    const QImage secondFrameLow = renderAtDetail(Artifact::DetailLevel::Low);
    const QImage secondFrameMedium =
        renderAtDetail(Artifact::DetailLevel::Medium);
    const QImage secondFrameHigh = renderAtDetail(Artifact::DetailLevel::High);
    const QImage secondFrameRestoredHigh =
        renderAtDetail(Artifact::DetailLevel::High);
    ASSERT_FALSE(secondFrameLow.isNull());
    ASSERT_FALSE(secondFrameMedium.isNull());
    ASSERT_FALSE(secondFrameHigh.isNull());
    ASSERT_FALSE(secondFrameRestoredHigh.isNull());
    EXPECT_FALSE(imagesMatchExactly(secondFrameLow, secondFrameHigh))
        << "the newly selected sequence frame should retain resolution LOD";
    EXPECT_FALSE(imagesMatchExactly(secondFrameLow, secondFrameMedium))
        << "the newly selected sequence frame should distinguish Low and Medium";
    EXPECT_FALSE(imagesMatchExactly(secondFrameMedium, secondFrameHigh))
        << "the newly selected sequence frame should distinguish Medium and High";
    EXPECT_FALSE(imagesMatchExactly(high, secondFrameHigh))
        << "changing sequence frame should replace the prior frame pixels";
    EXPECT_TRUE(imagesMatchExactly(secondFrameHigh, secondFrameRestoredHigh))
        << "returning to High should reproduce the newly selected sequence frame";

    layer->goToFrame(0);
    ASSERT_TRUE(waitForSequenceFrame(0));
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    const QImage restoredFirstFrameLow =
        renderAtDetail(Artifact::DetailLevel::Low);
    const QImage restoredFirstFrameMedium =
        renderAtDetail(Artifact::DetailLevel::Medium);
    const QImage restoredFirstFrameHigh =
        renderAtDetail(Artifact::DetailLevel::High);
    ASSERT_FALSE(restoredFirstFrameLow.isNull());
    ASSERT_FALSE(restoredFirstFrameMedium.isNull());
    ASSERT_FALSE(restoredFirstFrameHigh.isNull());
    EXPECT_TRUE(imagesMatchExactly(low, restoredFirstFrameLow))
        << "returning to the first sequence frame should restore its Low-LOD pixels";
    EXPECT_TRUE(imagesMatchExactly(medium, restoredFirstFrameMedium))
        << "returning to the first sequence frame should restore its Medium-LOD pixels";
    EXPECT_TRUE(imagesMatchExactly(high, restoredFirstFrameHigh))
        << "returning to the first sequence frame should restore its High-LOD pixels";
}

TEST(LayerSurfaceCacheParityContractTest,
     OfflineShapeOutputUsesRequestedResolutionLOD)
{
    constexpr int width = 96;
    constexpr int height = 96;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactShapeLayer>();
    ASSERT_TRUE(layer);
    layer->setShapeType(Artifact::ShapeType::Ellipse);
    layer->setSize(48, 48);
    layer->setFillColor(ArtifactCore::FloatColor(0.9f, 0.2f, 0.1f, 1.0f));
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(4.0f);
    glow->setGlowGain(1.0f);
    layer->addEffect(glow);

    const auto drawOfflineAt = [&](const Artifact::DetailLevel lod) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
            0, true, lod);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };

    const QImage low = drawOfflineAt(Artifact::DetailLevel::Low);
    const QImage medium = drawOfflineAt(Artifact::DetailLevel::Medium);
    const QImage high = drawOfflineAt(Artifact::DetailLevel::High);
    const QImage unknown = drawOfflineAt(
        static_cast<Artifact::DetailLevel>(99));
    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(unknown.isNull());
    EXPECT_FALSE(imagesMatchExactly(low, medium))
        << "the requested offline resolution LOD should affect shape sampling";
    EXPECT_FALSE(imagesMatchExactly(medium, high))
        << "Medium and High offline shape scales should remain distinct";
    EXPECT_TRUE(imagesMatchExactly(high, unknown))
        << "an unknown detail level should use full-resolution offline shape output";
}

TEST(LayerSurfaceCacheParityContractTest,
     InteractiveShapeResolutionLODChangesSamplingAndRestoresHigh)
{
    constexpr int width = 96;
    constexpr int height = 96;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactShapeLayer>();
    ASSERT_TRUE(layer);
    layer->setShapeType(Artifact::ShapeType::Ellipse);
    layer->setSize(48, 48);
    layer->setFillColor(ArtifactCore::FloatColor(0.9f, 0.2f, 0.1f, 1.0f));
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(4.0f);
    glow->setGlowGain(1.0f);
    layer->addEffect(glow);

    QHash<QString, Artifact::LayerSurfaceCacheEntry> surfaceCache;
    const QString cacheOwnerId = layer->id().toString();
    const auto drawAtDetail = [&](const Artifact::DetailLevel lod,
                                  QHash<QString, Artifact::LayerSurfaceCacheEntry>* cache) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, cache, nullptr,
            0, false, lod);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };

    const QImage low = drawAtDetail(Artifact::DetailLevel::Low, &surfaceCache);
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto lowEntry = surfaceCache.value(cacheOwnerId);
    const QImage medium = drawAtDetail(Artifact::DetailLevel::Medium, &surfaceCache);
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto mediumEntry = surfaceCache.value(cacheOwnerId);
    const QImage high = drawAtDetail(Artifact::DetailLevel::High, &surfaceCache);
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto highEntry = surfaceCache.value(cacheOwnerId);
    const QImage unknown = drawAtDetail(
        static_cast<Artifact::DetailLevel>(99), &surfaceCache);
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto unknownEntry = surfaceCache.value(cacheOwnerId);
    const QImage restoredLow = drawAtDetail(Artifact::DetailLevel::Low, &surfaceCache);
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto restoredLowEntry = surfaceCache.value(cacheOwnerId);
    const QImage restoredMedium = drawAtDetail(Artifact::DetailLevel::Medium, &surfaceCache);
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto restoredMediumEntry = surfaceCache.value(cacheOwnerId);
    const QImage restoredHigh = drawAtDetail(Artifact::DetailLevel::High, &surfaceCache);
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto restoredHighEntry = surfaceCache.value(cacheOwnerId);

    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(unknown.isNull());
    ASSERT_FALSE(restoredLow.isNull());
    ASSERT_FALSE(restoredMedium.isNull());
    ASSERT_FALSE(restoredHigh.isNull());
    EXPECT_FALSE(imagesMatchExactly(low, medium))
        << "Low and Medium interactive shape sampling should differ";
    EXPECT_FALSE(imagesMatchExactly(medium, high))
        << "Medium and High interactive shape sampling should differ";
    EXPECT_TRUE(imagesMatchExactly(high, unknown))
        << "an unknown detail value should use full-resolution interactive shape output";
    EXPECT_TRUE(imagesMatchExactly(low, restoredLow));
    EXPECT_TRUE(imagesMatchExactly(medium, restoredMedium));
    EXPECT_TRUE(imagesMatchExactly(high, restoredHigh));

    const auto processedSize = [](const Artifact::LayerSurfaceCacheEntry& entry) {
        if (entry.processedBuffer) {
            return QSize(entry.processedBuffer->width(),
                         entry.processedBuffer->height());
        }
        return entry.processedSurface.size();
    };
    EXPECT_FALSE(lowEntry.cacheSignature.isEmpty());
    EXPECT_FALSE(mediumEntry.cacheSignature.isEmpty());
    EXPECT_FALSE(highEntry.cacheSignature.isEmpty());
    EXPECT_EQ(processedSize(lowEntry), QSize(24, 24));
    EXPECT_EQ(processedSize(mediumEntry), QSize(36, 36));
    EXPECT_EQ(processedSize(highEntry), QSize(48, 48));
    EXPECT_EQ(processedSize(unknownEntry), QSize(48, 48));
    EXPECT_EQ(unknownEntry.cacheSignature, highEntry.cacheSignature);
    EXPECT_NE(lowEntry.cacheSignature, mediumEntry.cacheSignature);
    EXPECT_NE(mediumEntry.cacheSignature, highEntry.cacheSignature);
    EXPECT_EQ(restoredLowEntry.cacheSignature, lowEntry.cacheSignature);
    EXPECT_EQ(restoredMediumEntry.cacheSignature, mediumEntry.cacheSignature);
    EXPECT_EQ(restoredHighEntry.cacheSignature, highEntry.cacheSignature);

    const QImage cachedLowReuse = drawAtDetail(
        Artifact::DetailLevel::Low, &surfaceCache);
    const QImage bypassLow = drawAtDetail(Artifact::DetailLevel::Low, nullptr);
    const QImage cachedHighReuse = drawAtDetail(
        Artifact::DetailLevel::High, &surfaceCache);
    const QImage bypassHigh = drawAtDetail(Artifact::DetailLevel::High, nullptr);
    EXPECT_TRUE(imagesMatchExactly(cachedLowReuse, bypassLow));
    EXPECT_TRUE(imagesMatchExactly(cachedHighReuse, bypassHigh));
}

TEST(LayerSurfaceCacheParityContractTest,
     CompositionFinalEffectInputResolutionTracksLOD)
{
    constexpr int width = 65;
    constexpr int height = 47;
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("Composition Effect LOD")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(width, height);
    params.setDurationFrames(2);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("composition-effect-lod")),
        params);
    ASSERT_TRUE(composition);

    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(5.0f);
    glow->setGlowGain(1.0f);
    composition->addEffect(glow);

    QImage source(width, height, QImage::Format_RGBA8888);
    source.fill(Qt::transparent);
    for (int y = 14; y < 34; ++y) {
        for (int x = 22; x < 42; ++x) {
            source.setPixelColor(x, y, QColor(255, 210, 80, 255));
        }
    }

    const auto applyAtDetail = [&](const Artifact::DetailLevel detail) {
        QImage output = source;
        EXPECT_TRUE(Artifact::applyCompositionFinalEffectsToImage(
            composition.get(), output, detail));
        return output;
    };

    const QImage low = applyAtDetail(Artifact::DetailLevel::Low);
    const QImage medium = applyAtDetail(Artifact::DetailLevel::Medium);
    const QImage high = applyAtDetail(Artifact::DetailLevel::High);
    const QImage restoredHigh = applyAtDetail(Artifact::DetailLevel::High);
    const QImage unknownDetail = applyAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const QImage restoredLow = applyAtDetail(Artifact::DetailLevel::Low);
    const QImage restoredMedium = applyAtDetail(Artifact::DetailLevel::Medium);

    EXPECT_EQ(low.size(), QSize(33, 24));
    EXPECT_EQ(medium.size(), QSize(49, 35));
    EXPECT_EQ(high.size(), QSize(width, height));
    EXPECT_EQ(unknownDetail.size(), high.size());
    EXPECT_GT(visiblePixelCount(low), 0);
    EXPECT_GT(visiblePixelCount(medium), 0);
    EXPECT_GT(visiblePixelCount(high), 0);
    EXPECT_TRUE(imagesMatchExactly(high, restoredHigh))
        << "reapplying the composition effect at High should be deterministic";
    EXPECT_TRUE(imagesMatchExactly(high, unknownDetail))
        << "unknown detail values should use the full-resolution fallback";
    EXPECT_TRUE(imagesMatchExactly(low, restoredLow));
    EXPECT_TRUE(imagesMatchExactly(medium, restoredMedium));
}

TEST(LayerSurfaceCacheParityContractTest,
     CompositionFinalEffectLODClampsTinyDimensionsToOnePixel)
{
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("Tiny Composition Effect LOD")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(3, 3);
    params.setDurationFrames(1);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("tiny-composition-effect-lod")),
        params);
    ASSERT_TRUE(composition);

    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(1.0f);
    glow->setGlowGain(1.0f);
    composition->addEffect(glow);

    struct DimensionCase {
        QSize source;
        QSize low;
        QSize medium;
    };
    constexpr std::array<DimensionCase, 5> dimensionCases{{
        {{1, 1}, {1, 1}, {1, 1}},
        {{1, 3}, {1, 2}, {1, 2}},
        {{3, 1}, {2, 1}, {2, 1}},
        {{3, 3}, {2, 2}, {2, 2}},
        {{5, 3}, {3, 2}, {4, 2}},
    }};
    for (const DimensionCase& testCase : dimensionCases) {
        const QSize sourceSize = testCase.source;
        QImage source(sourceSize, QImage::Format_RGBA8888);
        source.fill(QColor(240, 160, 70, 255));

        const auto applyAtDetail = [&](const Artifact::DetailLevel detail) {
            QImage output = source;
            EXPECT_TRUE(Artifact::applyCompositionFinalEffectsToImage(
                composition.get(), output, detail));
            return output;
        };

        const QImage low = applyAtDetail(Artifact::DetailLevel::Low);
        const QImage medium = applyAtDetail(Artifact::DetailLevel::Medium);
        const QImage high = applyAtDetail(Artifact::DetailLevel::High);
        const QImage unknown = applyAtDetail(
            static_cast<Artifact::DetailLevel>(99));
        const QImage restoredLow = applyAtDetail(Artifact::DetailLevel::Low);
        const QImage restoredMedium = applyAtDetail(Artifact::DetailLevel::Medium);
        const QImage restoredHigh = applyAtDetail(Artifact::DetailLevel::High);

        ASSERT_FALSE(low.isNull()) << "source=" << sourceSize.width() << 'x'
                                   << sourceSize.height();
        ASSERT_FALSE(medium.isNull());
        ASSERT_FALSE(high.isNull());
        ASSERT_FALSE(unknown.isNull());
        ASSERT_FALSE(restoredLow.isNull());
        ASSERT_FALSE(restoredMedium.isNull());
        ASSERT_FALSE(restoredHigh.isNull());
        EXPECT_EQ(low.size(), testCase.low);
        EXPECT_EQ(medium.size(), testCase.medium);
        EXPECT_EQ(high.size(), sourceSize);
        EXPECT_EQ(unknown.size(), sourceSize)
            << "unknown detail values should use full input dimensions";
        EXPECT_TRUE(imagesMatchExactly(low, restoredLow));
        EXPECT_TRUE(imagesMatchExactly(medium, restoredMedium));
        EXPECT_TRUE(imagesMatchExactly(high, restoredHigh));
    }
}

TEST(LayerSurfaceCacheParityContractTest,
     ShapeLayerRasterSamplingChangesAcrossAllDetailLevels)
{
    constexpr int width = 96;
    constexpr int height = 96;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    renderer.setCanvasSize(static_cast<float>(width),
                           static_cast<float>(height));
    renderer.setViewportRect(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactShapeLayer>();
    ASSERT_TRUE(layer);
    layer->setShapeType(Artifact::ShapeType::Ellipse);
    layer->setSize(48, 48);
    layer->setFillColor(ArtifactCore::FloatColor(0.9f, 0.2f, 0.1f, 1.0f));

    const auto draw = [&](const Artifact::DetailLevel lod) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, lod);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };

    const QImage highDetail = draw(Artifact::DetailLevel::High);
    const QImage mediumDetail = draw(Artifact::DetailLevel::Medium);
    const QImage lowDetail = draw(Artifact::DetailLevel::Low);
    const QImage restoredHighDetail = draw(Artifact::DetailLevel::High);

    ASSERT_FALSE(highDetail.isNull());
    ASSERT_FALSE(mediumDetail.isNull());
    ASSERT_FALSE(lowDetail.isNull());
    ASSERT_FALSE(restoredHighDetail.isNull());
    EXPECT_EQ(highDetail.size(), QSize(width, height));
    EXPECT_EQ(mediumDetail.size(), highDetail.size());
    EXPECT_EQ(lowDetail.size(), highDetail.size());
    EXPECT_FALSE(imagesMatchExactly(highDetail, mediumDetail))
        << "Medium detail should use a distinct downsampled shape surface";
    EXPECT_FALSE(imagesMatchExactly(mediumDetail, lowDetail))
        << "Low and Medium detail should retain distinct sampling levels";
    EXPECT_FALSE(imagesMatchExactly(highDetail, lowDetail))
        << "Low detail should downsample the shape layer before drawing";
    EXPECT_TRUE(imagesMatchExactly(highDetail, restoredHighDetail))
        << "returning to High after Low should restore the original shape output";
}
