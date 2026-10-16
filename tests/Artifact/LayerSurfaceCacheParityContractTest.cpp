#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>

#include <QHash>
#include <QColor>
#include <QImage>
#include <QString>

#include "DiligentHeadlessRenderer.hpp"

import Artifact.Effect.Rasterizer.Glow;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.IRenderer;
import Color.Float;
import Memory.SharedPtr;

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
    auto glow = ArtifactCore::makeShared<Artifact::RasterizerGlowEffect>();
    glow->setRadius(3.0f);
    glow->setIntensity(1.0f);
    layer->addEffect(glow);

    const auto draw = [&](QHash<QString, Artifact::LayerSurfaceCacheEntry>* cache) {
        renderer.clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), &renderer, 1.0f, nullptr, cache, nullptr, 0);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };

    QHash<QString, Artifact::LayerSurfaceCacheEntry> surfaceCache;
    const QImage cacheBuildImage = draw(&surfaceCache);
    ASSERT_FALSE(cacheBuildImage.isNull());
    ASSERT_FALSE(surfaceCache.isEmpty());
    const QString cacheOwnerId = layer->id().toString();
    ASSERT_TRUE(surfaceCache.contains(cacheOwnerId));
    const auto builtCacheEntry = surfaceCache.value(cacheOwnerId);
    ASSERT_FALSE(builtCacheEntry.cacheSignature.isEmpty());

    const QImage cacheReuseImage = draw(&surfaceCache);
    ASSERT_FALSE(cacheReuseImage.isNull());
    const auto reusedCacheEntry = surfaceCache.value(cacheOwnerId);
    const QImage bypassImage = draw(nullptr);
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

    const QString originalSignature = builtCacheEntry.cacheSignature;
    QImage replacement(48, 48, QImage::Format_RGBA8888);
    replacement.fill(Qt::transparent);
    for (int y = 8; y < 36; ++y) {
        for (int x = 5; x < 27; ++x) {
            replacement.setPixelColor(x, y, QColor(80, 170, 255, 255));
        }
    }
    layer->setFromQImage(replacement);

    const QImage cachedAfterSourceEdit = draw(&surfaceCache);
    const QImage bypassAfterSourceEdit = draw(nullptr);
    ASSERT_FALSE(cachedAfterSourceEdit.isNull());
    ASSERT_FALSE(bypassAfterSourceEdit.isNull());
    const QString updatedSignature =
        surfaceCache.value(cacheOwnerId).cacheSignature;
    EXPECT_NE(updatedSignature, originalSignature);
    EXPECT_TRUE(imagesMatchExactly(cachedAfterSourceEdit, bypassAfterSourceEdit));
    EXPECT_FALSE(imagesMatchExactly(bypassImage, bypassAfterSourceEdit));
}
