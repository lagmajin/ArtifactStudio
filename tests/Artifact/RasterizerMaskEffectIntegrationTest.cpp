#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <tuple>

#include <QCoreApplication>
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QString>

import Artifact.Effect.Rasterizer.Blur;
import Artifact.Effect.Rasterizer.DropShadow;
import Artifact.Effect.Context;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Render.CompositionViewDrawing;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4_RGBA;
import Memory.SharedPtr;

namespace {

class RasterizerMaskEffectEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "RasterizerMaskEffectIntegrationTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QCoreApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QCoreApplication* application_ = nullptr;
};

RasterizerMaskEffectEnvironment* const environment =
    new RasterizerMaskEffectEnvironment();
const bool environmentRegistered = [] {
    testing::AddGlobalTestEnvironment(environment);
    return true;
}();

} // namespace

TEST(RasterizerMaskEffectIntegrationTest, RasterizerBlurRunsAfterLayerMask)
{
    constexpr int width = 32;
    constexpr int height = 16;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.4f;
        pixels[i + 1] = 0.4f;
        pixels[i + 2] = 0.4f;
        pixels[i + 3] = 1.0f;
    }

    Artifact::ArtifactSolidLayerInitParams params(QStringLiteral("MaskBlurFixture"));
    params.setWidth(width);
    params.setHeight(height);
    Artifact::ArtifactLayerFactory factory;
    const auto created = factory.createLayer(params);
    ASSERT_TRUE(created.success);
    ASSERT_NE(created.layer.get(), nullptr);

    Artifact::MaskPath path;
    path.setClosed(true);
    path.setFeather({1.0f});
    path.addVertex({QPointF(0.0, 0.0), QPointF(), QPointF()});
    path.addVertex({QPointF(16.0, 0.0), QPointF(), QPointF()});
    path.addVertex({QPointF(16.0, 16.0), QPointF(), QPointF()});
    path.addVertex({QPointF(0.0, 16.0), QPointF(), QPointF()});
    Artifact::LayerMask mask;
    mask.addMaskPath(path);
    created.layer->addMask(mask);

    auto blur = ArtifactCore::makeShared<Artifact::BlurEffect>();
    blur->setRadius(4.0f);
    blur->setStrength(1.0f);
    created.layer->addEffect(blur);

    ArtifactCore::ImageF32x4_RGBA surface;
    surface.setFromRGBA32F(pixels.data(), width, height);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), surface, Artifact::DetailLevel::High);

    const auto interior = surface.getPixel(8, 8);
    const auto boundary = surface.getPixel(17, 8);
    const auto outside = surface.getPixel(25, 8);
    const auto beyondMaskWithBlur = surface.getPixel(20, 8);
    EXPECT_GT(interior.a(), boundary.a());
    EXPECT_GT(boundary.a(), outside.a());
    EXPECT_GT(boundary.a(), 0.0f);

    created.layer->removeEffect(blur->effectID());
    ASSERT_EQ(created.layer->effectCount(), 0);
    ArtifactCore::ImageF32x4_RGBA maskOnlySurface;
    maskOnlySurface.setFromRGBA32F(pixels.data(), width, height);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), maskOnlySurface, Artifact::DetailLevel::High);

    const auto beyondMaskWithoutBlur = maskOnlySurface.getPixel(20, 8);
    EXPECT_FLOAT_EQ(beyondMaskWithoutBlur.a(), 0.0f);
    EXPECT_GT(beyondMaskWithBlur.a(), beyondMaskWithoutBlur.a());

    for (const Artifact::DetailLevel detail : {
             Artifact::DetailLevel::Low,
             Artifact::DetailLevel::Medium}) {
        created.layer->addEffect(blur);
        ArtifactCore::ImageF32x4_RGBA blurredSurface;
        blurredSurface.setFromRGBA32F(pixels.data(), width, height);
        Artifact::applyRasterizerEffectsAndMasksToSurface(
            created.layer.get(), blurredSurface, detail);

        const float resolutionScale =
            static_cast<float>(blurredSurface.width()) / width;
        const int edgeX = static_cast<int>(std::round(16.0f * resolutionScale));
        const int sampleX = std::min(
            blurredSurface.width() - 1,
            edgeX + std::max(1, static_cast<int>(std::round(4.0f * resolutionScale))));
        const int sampleY = static_cast<int>(std::round(8.0f * resolutionScale));
        const float blurredOutsideAlpha =
            blurredSurface.getPixel(sampleX, sampleY).a();

        created.layer->removeEffect(blur->effectID());
        ArtifactCore::ImageF32x4_RGBA maskOnlyAtDetail;
        maskOnlyAtDetail.setFromRGBA32F(pixels.data(), width, height);
        Artifact::applyRasterizerEffectsAndMasksToSurface(
            created.layer.get(), maskOnlyAtDetail, detail);
        const float maskOnlyOutsideAlpha =
            maskOnlyAtDetail.getPixel(sampleX, sampleY).a();

        EXPECT_FLOAT_EQ(maskOnlyOutsideAlpha, 0.0f)
            << "the scaled sample should remain outside the mask at LOD "
            << static_cast<int>(detail);
        EXPECT_GT(blurredOutsideAlpha, maskOnlyOutsideAlpha)
            << "the blur should spread beyond the mask at LOD "
            << static_cast<int>(detail);
    }
}

TEST(RasterizerMaskEffectIntegrationTest, EffectSurfaceResolutionTracksDetailLevel)
{
    constexpr int width = 41;
    constexpr int height = 25;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float value = x < width / 2 ? 0.0f : 1.0f;
            pixels[offset] = value;
            pixels[offset + 1] = 0.25f;
            pixels[offset + 2] = 0.5f;
            pixels[offset + 3] = 1.0f;
        }
    }

    Artifact::ArtifactSolidLayerInitParams params(QStringLiteral("LodBlurFixture"));
    params.setWidth(width);
    params.setHeight(height);
    Artifact::ArtifactLayerFactory factory;
    const auto created = factory.createLayer(params);
    ASSERT_TRUE(created.success);
    ASSERT_NE(created.layer.get(), nullptr);

    auto blur = ArtifactCore::makeShared<Artifact::BlurEffect>();
    blur->setRadius(3.0f);
    blur->setStrength(1.0f);
    created.layer->addEffect(blur);

    ArtifactCore::ImageF32x4_RGBA low;
    low.setFromRGBA32F(pixels.data(), width, height);
    const auto surfaceDescriptor =
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultipliedBgra();
    low.setColorDescriptor(surfaceDescriptor);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), low, Artifact::DetailLevel::Low);
    EXPECT_EQ(low.width(), 21);
    EXPECT_EQ(low.height(), 13);
    EXPECT_EQ(low.colorDescriptor(), surfaceDescriptor);

    ArtifactCore::ImageF32x4_RGBA medium;
    medium.setFromRGBA32F(pixels.data(), width, height);
    medium.setColorDescriptor(surfaceDescriptor);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), medium, Artifact::DetailLevel::Medium);
    EXPECT_EQ(medium.width(), 31);
    EXPECT_EQ(medium.height(), 19);
    EXPECT_EQ(medium.colorDescriptor(), surfaceDescriptor);

    ArtifactCore::ImageF32x4_RGBA high;
    high.setFromRGBA32F(pixels.data(), width, height);
    high.setColorDescriptor(surfaceDescriptor);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), high, Artifact::DetailLevel::High);
    EXPECT_EQ(high.width(), width);
    EXPECT_EQ(high.height(), height);
    EXPECT_EQ(high.colorDescriptor(), surfaceDescriptor);

    ArtifactCore::ImageF32x4_RGBA unknownDetail;
    unknownDetail.setFromRGBA32F(pixels.data(), width, height);
    unknownDetail.setColorDescriptor(surfaceDescriptor);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), unknownDetail,
        static_cast<Artifact::DetailLevel>(99));
    EXPECT_EQ(unknownDetail.width(), width);
    EXPECT_EQ(unknownDetail.height(), height);
    EXPECT_EQ(unknownDetail.colorDescriptor(), surfaceDescriptor)
        << "unknown detail values should use the full-resolution fallback";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto expected = high.getPixel(x, y);
            const auto actual = unknownDetail.getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), expected.r());
            EXPECT_FLOAT_EQ(actual.g(), expected.g());
            EXPECT_FLOAT_EQ(actual.b(), expected.b());
            EXPECT_FLOAT_EQ(actual.a(), expected.a());
        }
    }

    const std::array<float, 8> tinyPixels{
        0.25f, 0.5f, 0.75f, 1.0f,
        0.75f, 0.5f, 0.25f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA tinyLow;
    tinyLow.setFromRGBA32F(tinyPixels.data(), 1, 2);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), tinyLow, Artifact::DetailLevel::Low);
    EXPECT_EQ(tinyLow.width(), 1);
    EXPECT_EQ(tinyLow.height(), 1);

    ArtifactCore::ImageF32x4_RGBA tinyMedium;
    tinyMedium.setFromRGBA32F(tinyPixels.data(), 1, 2);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), tinyMedium, Artifact::DetailLevel::Medium);
    EXPECT_EQ(tinyMedium.width(), 1);
    EXPECT_EQ(tinyMedium.height(), 2);

    ArtifactCore::ImageF32x4_RGBA tinyHigh;
    tinyHigh.setFromRGBA32F(tinyPixels.data(), 1, 2);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), tinyHigh, Artifact::DetailLevel::High);
    EXPECT_EQ(tinyHigh.width(), 1);
    EXPECT_EQ(tinyHigh.height(), 2);

    const std::array<float, 8> tinyHorizontalPixels{
        0.25f, 0.5f, 0.75f, 1.0f,
        0.75f, 0.5f, 0.25f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA tinyHorizontalLow;
    tinyHorizontalLow.setFromRGBA32F(tinyHorizontalPixels.data(), 2, 1);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), tinyHorizontalLow, Artifact::DetailLevel::Low);
    EXPECT_EQ(tinyHorizontalLow.width(), 1);
    EXPECT_EQ(tinyHorizontalLow.height(), 1);

    ArtifactCore::ImageF32x4_RGBA tinyHorizontalMedium;
    tinyHorizontalMedium.setFromRGBA32F(tinyHorizontalPixels.data(), 2, 1);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), tinyHorizontalMedium, Artifact::DetailLevel::Medium);
    EXPECT_EQ(tinyHorizontalMedium.width(), 2);
    EXPECT_EQ(tinyHorizontalMedium.height(), 1);

    ArtifactCore::ImageF32x4_RGBA tinyHorizontalHigh;
    tinyHorizontalHigh.setFromRGBA32F(tinyHorizontalPixels.data(), 2, 1);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), tinyHorizontalHigh, Artifact::DetailLevel::High);
    EXPECT_EQ(tinyHorizontalHigh.width(), 2);
    EXPECT_EQ(tinyHorizontalHigh.height(), 1);

    QImage qLow(width, height, QImage::Format_RGBA8888);
    qLow.fill(QColor(180, 120, 60, 255));
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), qLow, Artifact::DetailLevel::Low);
    EXPECT_EQ(qLow.size(), QSize(21, 13));

    QImage qMedium(width, height, QImage::Format_RGBA8888);
    qMedium.fill(QColor(180, 120, 60, 255));
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), qMedium, Artifact::DetailLevel::Medium);
    EXPECT_EQ(qMedium.size(), QSize(31, 19));

    QImage qHigh(width, height, QImage::Format_RGBA8888);
    qHigh.fill(QColor(180, 120, 60, 255));
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), qHigh, Artifact::DetailLevel::High);
    EXPECT_EQ(qHigh.size(), QSize(width, height));

    QImage qUnknown(width, height, QImage::Format_RGBA8888);
    qUnknown.fill(QColor(180, 120, 60, 255));
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), qUnknown,
        static_cast<Artifact::DetailLevel>(99));
    EXPECT_EQ(qUnknown.size(), QSize(width, height))
        << "unknown detail values should use the full-resolution fallback";
    EXPECT_TRUE(qUnknown == qHigh)
        << "unknown detail values should reproduce the High-resolution "
           "rasterizer output";
}

TEST(RasterizerMaskEffectIntegrationTest,
     IsolatedCornerPixelsSurviveOddSizeResolutionLODDownsampling)
{
    constexpr int width = 41;
    constexpr int height = 25;
    std::array<float, width * height * 4> pixels{};
    const auto setOpaquePixel = [&](const int x, const int y) {
        const std::size_t offset =
            static_cast<std::size_t>((y * width + x) * 4);
        pixels[offset] = 1.0f;
        pixels[offset + 1] = 1.0f;
        pixels[offset + 2] = 1.0f;
        pixels[offset + 3] = 1.0f;
    };
    setOpaquePixel(0, 0);
    setOpaquePixel(width - 1, height - 1);

    Artifact::ArtifactSolidLayerInitParams params(
        QStringLiteral("CornerPixelLodFixture"));
    params.setWidth(width);
    params.setHeight(height);
    Artifact::ArtifactLayerFactory factory;
    const auto created = factory.createLayer(params);
    ASSERT_TRUE(created.success);
    ASSERT_NE(created.layer.get(), nullptr);

    for (const auto [detail, expectedWidth, expectedHeight] : {
             std::tuple{Artifact::DetailLevel::Low, 21, 13},
             std::tuple{Artifact::DetailLevel::Medium, 31, 19},
             std::tuple{Artifact::DetailLevel::High, width, height}}) {
        ArtifactCore::ImageF32x4_RGBA surface;
        surface.setFromRGBA32F(pixels.data(), width, height);
        Artifact::applyRasterizerEffectsAndMasksToSurface(
            created.layer.get(), surface, detail);

        ASSERT_EQ(surface.width(), expectedWidth);
        ASSERT_EQ(surface.height(), expectedHeight);
        const auto hasVisibleEnergyNear = [&](const int cornerX,
                                              const int cornerY) {
            const int xBegin = std::max(0, cornerX - 1);
            const int yBegin = std::max(0, cornerY - 1);
            for (int y = yBegin; y <= cornerY; ++y) {
                for (int x = xBegin; x <= cornerX; ++x) {
                    if (surface.getPixel(x, y).a() > 0.01f) {
                        return true;
                    }
                }
            }
            return false;
        };

        EXPECT_TRUE(hasVisibleEnergyNear(0, 0))
            << "the first source pixel should remain visible at LOD "
            << static_cast<int>(detail);
        EXPECT_TRUE(hasVisibleEnergyNear(expectedWidth - 1,
                                         expectedHeight - 1))
            << "the last source pixel should remain visible at LOD "
            << static_cast<int>(detail);

        QImage qImage(width, height, QImage::Format_RGBA8888);
        qImage.fill(Qt::transparent);
        qImage.setPixelColor(0, 0, QColor(255, 255, 255, 255));
        qImage.setPixelColor(width - 1, height - 1,
                             QColor(255, 255, 255, 255));
        Artifact::applyRasterizerEffectsAndMasksToSurface(
            created.layer.get(), qImage, detail);
        ASSERT_EQ(qImage.size(), QSize(expectedWidth, expectedHeight));
        const auto qImageHasVisibleEnergyNear = [&](const int cornerX,
                                                    const int cornerY) {
            const int xBegin = std::max(0, cornerX - 1);
            const int yBegin = std::max(0, cornerY - 1);
            for (int y = yBegin; y <= cornerY; ++y) {
                for (int x = xBegin; x <= cornerX; ++x) {
                    if (qImage.pixelColor(x, y).alpha() > 2) {
                        return true;
                    }
                }
            }
            return false;
        };
        EXPECT_TRUE(qImageHasVisibleEnergyNear(0, 0))
            << "the QImage path should keep the first source pixel visible at LOD "
            << static_cast<int>(detail);
        EXPECT_TRUE(qImageHasVisibleEnergyNear(expectedWidth - 1,
                                               expectedHeight - 1))
            << "the QImage path should keep the last source pixel visible at LOD "
            << static_cast<int>(detail);
    }
}

TEST(RasterizerMaskEffectIntegrationTest,
     InteractiveBlurFootprintTracksResolutionLODScale)
{
    constexpr int width = 33;
    constexpr int height = 33;
    constexpr int centerX = width / 2;
    constexpr int centerY = height / 2;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 3] = 1.0f;
        }
    }
    pixels[static_cast<std::size_t>((centerY * width + centerX) * 4)] = 1.0f;

    ArtifactCore::ImageF32x4_RGBA sourceImage;
    sourceImage.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(sourceImage);

    const auto radialVariance = [&](const float scale,
                                    const bool interactive) {
        Artifact::BlurEffect blur;
        blur.setRadius(12.0f);
        blur.setStrength(1.0f);
        Artifact::EffectContext context;
        context.isInteractive = interactive;
        context.resolutionScale = scale;
        blur.setContext(context);

        ArtifactCore::ImageF32x4RGBAWithCache output;
        blur.applyCPUOnly(source, output);
        EXPECT_EQ(output.width(), width);
        EXPECT_EQ(output.height(), height);
        if (output.width() != width || output.height() != height) {
            return 0.0;
        }

        double totalWeight = 0.0;
        double weightedSquaredRadius = 0.0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const double weight = output.image().getPixel(x, y).r();
                const int dx = x - centerX;
                const int dy = y - centerY;
                totalWeight += weight;
                weightedSquaredRadius +=
                    weight * static_cast<double>(dx * dx + dy * dy);
            }
        }
        EXPECT_GT(totalWeight, 0.0);
        return totalWeight > 0.0
            ? weightedSquaredRadius / totalWeight
            : 0.0;
    };

    const double lowVariance = radialVariance(0.25f, true);
    const double mediumVariance = radialVariance(0.75f, true);
    const double highVariance = radialVariance(1.0f, true);
    const double belowMinimumVariance = radialVariance(0.01f, true);
    const double minimumVariance = radialVariance(0.125f, true);
    const double aboveMaximumVariance = radialVariance(2.0f, true);
    const double offlineLowVariance = radialVariance(0.25f, false);
    const double offlineHighVariance = radialVariance(1.0f, false);

    EXPECT_GT(lowVariance, 0.0);
    EXPECT_LT(lowVariance, mediumVariance);
    EXPECT_LT(mediumVariance, highVariance);
    EXPECT_DOUBLE_EQ(belowMinimumVariance, minimumVariance)
        << "Interactive blur scale should clamp at the supported minimum";
    EXPECT_DOUBLE_EQ(aboveMaximumVariance, highVariance)
        << "Interactive blur scale should clamp at full resolution";
    EXPECT_DOUBLE_EQ(offlineLowVariance, offlineHighVariance)
        << "Offline blur quality must ignore interactive preview resolution LOD";
}

TEST(RasterizerMaskEffectIntegrationTest,
     DropShadowRemainsVisibleAtEveryResolutionLODAndRestoresHigh)
{
    constexpr int width = 41;
    constexpr int height = 25;
    std::array<float, width * height * 4> pixels{};
    for (int y = 10; y < 15; ++y) {
        for (int x = 16; x < 21; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = 0.8f;
            pixels[offset + 1] = 0.4f;
            pixels[offset + 2] = 0.2f;
            pixels[offset + 3] = 1.0f;
        }
    }

    Artifact::ArtifactSolidLayerInitParams params(
        QStringLiteral("LodDropShadowFixture"));
    params.setWidth(width);
    params.setHeight(height);
    Artifact::ArtifactLayerFactory factory;
    const auto created = factory.createLayer(params);
    ASSERT_TRUE(created.success);
    ASSERT_NE(created.layer.get(), nullptr);

    auto shadow = ArtifactCore::makeShared<Artifact::DropShadowEffect>();
    ASSERT_TRUE(shadow);
    shadow->setDistance(8.0f);
    shadow->setAngle(180.0f);
    shadow->setSoftness(2.0f);
    shadow->setOpacity(100.0f);
    shadow->setShadowOnly(true);
    created.layer->addEffect(shadow);

    const auto renderAtDetail = [&](const Artifact::DetailLevel detail) {
        ArtifactCore::ImageF32x4_RGBA surface;
        surface.setFromRGBA32F(pixels.data(), width, height);
        Artifact::applyRasterizerEffectsAndMasksToSurface(
            created.layer.get(), surface, detail);
        return surface;
    };

    const ArtifactCore::ImageF32x4_RGBA low =
        renderAtDetail(Artifact::DetailLevel::Low);
    const ArtifactCore::ImageF32x4_RGBA medium =
        renderAtDetail(Artifact::DetailLevel::Medium);
    const ArtifactCore::ImageF32x4_RGBA high =
        renderAtDetail(Artifact::DetailLevel::High);
    const ArtifactCore::ImageF32x4_RGBA unknownDetail =
        renderAtDetail(static_cast<Artifact::DetailLevel>(99));
    const ArtifactCore::ImageF32x4_RGBA restoredHigh =
        renderAtDetail(Artifact::DetailLevel::High);
    const ArtifactCore::ImageF32x4_RGBA restoredLow =
        renderAtDetail(Artifact::DetailLevel::Low);

    ASSERT_EQ(low.width(), 21);
    ASSERT_EQ(low.height(), 13);
    ASSERT_EQ(medium.width(), 31);
    ASSERT_EQ(medium.height(), 19);
    ASSERT_EQ(high.width(), width);
    ASSERT_EQ(high.height(), height);
    ASSERT_EQ(unknownDetail.width(), width);
    ASSERT_EQ(unknownDetail.height(), height);
    ASSERT_EQ(restoredHigh.width(), width);
    ASSERT_EQ(restoredHigh.height(), height);
    ASSERT_EQ(restoredLow.width(), low.width());
    ASSERT_EQ(restoredLow.height(), low.height());

    const auto shadowCentroidX = [](const ArtifactCore::ImageF32x4_RGBA& image) {
        double totalAlpha = 0.0;
        double weightedX = 0.0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const double alpha = image.getPixel(x, y).a();
                totalAlpha += alpha;
                weightedX += alpha * x;
            }
        }
        return totalAlpha > 0.0 ? weightedX / totalAlpha : -1.0;
    };
    EXPECT_GT(shadowCentroidX(low), low.width() / 2.0);
    EXPECT_GT(shadowCentroidX(medium), medium.width() / 2.0);
    EXPECT_GT(shadowCentroidX(high), high.width() / 2.0);

    ASSERT_EQ(high.width(), restoredHigh.width());
    ASSERT_EQ(high.height(), restoredHigh.height());
    for (int y = 0; y < high.height(); ++y) {
        for (int x = 0; x < high.width(); ++x) {
            const auto before = high.getPixel(x, y);
            const auto after = restoredHigh.getPixel(x, y);
            EXPECT_FLOAT_EQ(before.r(), after.r());
            EXPECT_FLOAT_EQ(before.g(), after.g());
            EXPECT_FLOAT_EQ(before.b(), after.b());
            EXPECT_FLOAT_EQ(before.a(), after.a());

            const auto unknown = unknownDetail.getPixel(x, y);
            EXPECT_FLOAT_EQ(before.r(), unknown.r());
            EXPECT_FLOAT_EQ(before.g(), unknown.g());
            EXPECT_FLOAT_EQ(before.b(), unknown.b());
            EXPECT_FLOAT_EQ(before.a(), unknown.a());
        }
    }

    for (int y = 0; y < low.height(); ++y) {
        for (int x = 0; x < low.width(); ++x) {
            const auto before = low.getPixel(x, y);
            const auto after = restoredLow.getPixel(x, y);
            EXPECT_FLOAT_EQ(before.r(), after.r());
            EXPECT_FLOAT_EQ(before.g(), after.g());
            EXPECT_FLOAT_EQ(before.b(), after.b());
            EXPECT_FLOAT_EQ(before.a(), after.a());
        }
    }
}

TEST(RasterizerMaskEffectIntegrationTest,
     LongDropShadowReachesTheFarEdgeAtEveryResolutionLOD)
{
    constexpr int width = 65;
    constexpr int height = 33;
    std::array<float, width * height * 4> pixels{};
    for (int y = 14; y < 19; ++y) {
        for (int x = 2; x < 7; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = 1.0f;
            pixels[offset + 1] = 0.8f;
            pixels[offset + 2] = 0.2f;
            pixels[offset + 3] = 1.0f;
        }
    }

    Artifact::ArtifactSolidLayerInitParams params(
        QStringLiteral("LongLodDropShadowFixture"));
    params.setWidth(width);
    params.setHeight(height);
    Artifact::ArtifactLayerFactory factory;
    const auto created = factory.createLayer(params);
    ASSERT_TRUE(created.success);
    ASSERT_NE(created.layer.get(), nullptr);

    auto shadow = ArtifactCore::makeShared<Artifact::DropShadowEffect>();
    ASSERT_TRUE(shadow);
    shadow->setDistance(54.0f);
    shadow->setAngle(180.0f);
    shadow->setSoftness(2.0f);
    shadow->setOpacity(100.0f);
    shadow->setShadowOnly(true);
    created.layer->addEffect(shadow);

    for (const Artifact::DetailLevel detail : {Artifact::DetailLevel::Low,
                                                Artifact::DetailLevel::Medium,
                                                Artifact::DetailLevel::High}) {
        ArtifactCore::ImageF32x4_RGBA surface;
        surface.setFromRGBA32F(pixels.data(), width, height);
        Artifact::applyRasterizerEffectsAndMasksToSurface(
            created.layer.get(), surface, detail);

        ASSERT_GT(surface.width(), 0);
        ASSERT_GT(surface.height(), 0);
        const int farEdgeStart = (surface.width() * 3) / 4;
        double farEdgeAlpha = 0.0;
        for (int y = 0; y < surface.height(); ++y) {
            for (int x = farEdgeStart; x < surface.width(); ++x) {
                farEdgeAlpha += surface.getPixel(x, y).a();
            }
        }
        EXPECT_GT(farEdgeAlpha, 0.0)
            << "the long shadow should reach the far edge at LOD "
            << static_cast<int>(detail)
            << " (surface=" << surface.width() << "x" << surface.height() << ")";
    }
}

TEST(RasterizerMaskEffectIntegrationTest,
     MaskCoverageTracksDetailResolutionAndRestoresAfterQualitySwitches)
{
    constexpr int width = 41;
    constexpr int height = 25;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float value = x < width / 2 ? 0.0f : 1.0f;
            pixels[offset] = value;
            pixels[offset + 1] = 0.25f;
            pixels[offset + 2] = 0.5f;
            pixels[offset + 3] = 1.0f;
        }
    }

    Artifact::ArtifactSolidLayerInitParams params(QStringLiteral("LodMaskRecoveryFixture"));
    params.setWidth(width);
    params.setHeight(height);
    Artifact::ArtifactLayerFactory factory;
    const auto created = factory.createLayer(params);
    ASSERT_TRUE(created.success);
    ASSERT_NE(created.layer.get(), nullptr);

    Artifact::MaskPath path;
    path.setClosed(true);
    path.setFeather({1.0f});
    path.addVertex({QPointF(0.0, 0.0), QPointF(), QPointF()});
    path.addVertex({QPointF(20.0, 0.0), QPointF(), QPointF()});
    path.addVertex({QPointF(20.0, 25.0), QPointF(), QPointF()});
    path.addVertex({QPointF(0.0, 25.0), QPointF(), QPointF()});
    Artifact::LayerMask mask;
    mask.addMaskPath(path);
    created.layer->addMask(mask);

    ArtifactCore::ImageF32x4_RGBA low;
    low.setFromRGBA32F(pixels.data(), width, height);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), low, Artifact::DetailLevel::Low);
    EXPECT_EQ(low.width(), 21);
    EXPECT_EQ(low.height(), 13);

    ArtifactCore::ImageF32x4_RGBA medium;
    medium.setFromRGBA32F(pixels.data(), width, height);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), medium, Artifact::DetailLevel::Medium);
    EXPECT_EQ(medium.width(), 31);
    EXPECT_EQ(medium.height(), 19);

    ArtifactCore::ImageF32x4_RGBA high;
    high.setFromRGBA32F(pixels.data(), width, height);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), high, Artifact::DetailLevel::High);
    EXPECT_EQ(high.width(), width);
    EXPECT_EQ(high.height(), height);

    const auto matchesDirectMaskAtLod = [&](const ArtifactCore::ImageF32x4_RGBA& actual,
                                            const int expectedWidth,
                                            const int expectedHeight) {
        ArtifactCore::ImageF32x4_RGBA expected;
        expected.setFromRGBA32F(pixels.data(), expectedWidth, expectedHeight);
        auto expectedCv = expected.toCVMat();
        mask.applyToImage(expectedWidth, expectedHeight, &expectedCv,
                          0.0f, 0.0f,
                          static_cast<float>(expectedWidth) / width,
                          static_cast<float>(expectedHeight) / height);
        expected.setFromCVMat(expectedCv);
        ASSERT_EQ(actual.width(), expectedWidth);
        ASSERT_EQ(actual.height(), expectedHeight);
        for (int y = 0; y < expectedHeight; ++y) {
            for (int x = 0; x < expectedWidth; ++x) {
                EXPECT_NEAR(actual.getPixel(x, y).a(),
                            expected.getPixel(x, y).a(), 1.0e-6f);
            }
        }
    };
    matchesDirectMaskAtLod(low, 21, 13);
    matchesDirectMaskAtLod(medium, 31, 19);
    matchesDirectMaskAtLod(high, width, height);

    ArtifactCore::ImageF32x4_RGBA recovered;
    recovered.setFromRGBA32F(pixels.data(), width, height);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), recovered, Artifact::DetailLevel::High);
    EXPECT_EQ(recovered.width(), width);
    EXPECT_EQ(recovered.height(), height);
    matchesDirectMaskAtLod(recovered, width, height);
}

TEST(RasterizerMaskEffectIntegrationTest,
     ThinMaskCoverageSurvivesEveryResolutionLOD)
{
    constexpr int width = 41;
    constexpr int height = 25;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = 0.75f;
            pixels[offset + 1] = 0.5f;
            pixels[offset + 2] = 0.25f;
            pixels[offset + 3] = 1.0f;
        }
    }

    Artifact::ArtifactSolidLayerInitParams params(
        QStringLiteral("ThinMaskLodFixture"));
    params.setWidth(width);
    params.setHeight(height);
    Artifact::ArtifactLayerFactory factory;
    const auto created = factory.createLayer(params);
    ASSERT_TRUE(created.success);
    ASSERT_NE(created.layer.get(), nullptr);

    Artifact::MaskPath thinPath;
    thinPath.setClosed(true);
    thinPath.setFeather({0.5f});
    thinPath.addVertex({QPointF(20.0, 4.0), QPointF(), QPointF()});
    thinPath.addVertex({QPointF(21.0, 4.0), QPointF(), QPointF()});
    thinPath.addVertex({QPointF(21.0, 21.0), QPointF(), QPointF()});
    thinPath.addVertex({QPointF(20.0, 21.0), QPointF(), QPointF()});
    Artifact::LayerMask mask;
    mask.addMaskPath(thinPath);
    created.layer->addMask(mask);

    const auto renderAtDetail = [&](const Artifact::DetailLevel detail) {
        ArtifactCore::ImageF32x4_RGBA surface;
        surface.setFromRGBA32F(pixels.data(), width, height);
        Artifact::applyRasterizerEffectsAndMasksToSurface(
            created.layer.get(), surface, detail);
        return surface;
    };

    const auto low = renderAtDetail(Artifact::DetailLevel::Low);
    const auto medium = renderAtDetail(Artifact::DetailLevel::Medium);
    const auto high = renderAtDetail(Artifact::DetailLevel::High);
    const auto lowAfterRoundTrip = renderAtDetail(Artifact::DetailLevel::Low);
    const auto highAfterRoundTrip = renderAtDetail(Artifact::DetailLevel::High);
    const auto alphaCoverage = [](const ArtifactCore::ImageF32x4_RGBA& image) {
        double coverage = 0.0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                coverage += image.getPixel(x, y).a();
            }
        }
        return coverage;
    };
    const auto matchesDirectMaskAtLod = [&](const ArtifactCore::ImageF32x4_RGBA& actual) {
        std::array<float, width * height * 4> scaledPixels{};
        for (int y = 0; y < actual.height(); ++y) {
            for (int x = 0; x < actual.width(); ++x) {
                const std::size_t offset =
                    static_cast<std::size_t>((y * actual.width() + x) * 4);
                scaledPixels[offset] = 0.75f;
                scaledPixels[offset + 1] = 0.5f;
                scaledPixels[offset + 2] = 0.25f;
                scaledPixels[offset + 3] = 1.0f;
            }
        }

        ArtifactCore::ImageF32x4_RGBA expected;
        expected.setFromRGBA32F(scaledPixels.data(), actual.width(),
                                actual.height());
        auto expectedCv = expected.toCVMat();
        mask.applyToImage(actual.width(), actual.height(), &expectedCv,
                          0.0f, 0.0f,
                          static_cast<float>(actual.width()) / width,
                          static_cast<float>(actual.height()) / height);
        expected.setFromCVMat(expectedCv);

        ASSERT_EQ(actual.width(), expected.width());
        ASSERT_EQ(actual.height(), expected.height());
        for (int y = 0; y < actual.height(); ++y) {
            for (int x = 0; x < actual.width(); ++x) {
                EXPECT_NEAR(actual.getPixel(x, y).a(),
                            expected.getPixel(x, y).a(), 1.0e-6f)
                    << "thin-mask coverage drifted at " << x << ',' << y
                    << " in " << actual.width() << 'x' << actual.height();
            }
        }
    };
    const auto matchesPixels = [](const ArtifactCore::ImageF32x4_RGBA& expected,
                                  const ArtifactCore::ImageF32x4_RGBA& actual) {
        ASSERT_EQ(actual.width(), expected.width());
        ASSERT_EQ(actual.height(), expected.height());
        for (int y = 0; y < actual.height(); ++y) {
            for (int x = 0; x < actual.width(); ++x) {
                const auto expectedPixel = expected.getPixel(x, y);
                const auto actualPixel = actual.getPixel(x, y);
                EXPECT_NEAR(actualPixel.r(), expectedPixel.r(), 1.0e-6f);
                EXPECT_NEAR(actualPixel.g(), expectedPixel.g(), 1.0e-6f);
                EXPECT_NEAR(actualPixel.b(), expectedPixel.b(), 1.0e-6f);
                EXPECT_NEAR(actualPixel.a(), expectedPixel.a(), 1.0e-6f);
            }
        }
    };

    ASSERT_EQ(low.width(), 21);
    ASSERT_EQ(low.height(), 13);
    ASSERT_EQ(medium.width(), 31);
    ASSERT_EQ(medium.height(), 19);
    ASSERT_EQ(high.width(), width);
    ASSERT_EQ(high.height(), height);
    EXPECT_GT(alphaCoverage(low), 0.0)
        << "the thin mask must not disappear at Low resolution";
    EXPECT_GT(alphaCoverage(medium), 0.0)
        << "the thin mask must not disappear at Medium resolution";
    EXPECT_GT(alphaCoverage(high), 0.0)
        << "the thin mask must remain visible at High resolution";
    EXPECT_EQ(lowAfterRoundTrip.width(), low.width());
    EXPECT_EQ(lowAfterRoundTrip.height(), low.height());
    EXPECT_EQ(highAfterRoundTrip.width(), high.width());
    EXPECT_EQ(highAfterRoundTrip.height(), high.height());
    matchesDirectMaskAtLod(low);
    matchesDirectMaskAtLod(medium);
    matchesDirectMaskAtLod(high);
    matchesPixels(low, lowAfterRoundTrip);
    matchesPixels(high, highAfterRoundTrip);
}
