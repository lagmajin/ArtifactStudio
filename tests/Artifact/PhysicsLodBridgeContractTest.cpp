#include <gtest/gtest.h>

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QString>

#include <algorithm>
#include <cmath>

#include "DiligentHeadlessRenderer.hpp"

import Artifact.Layer.PhysicsBridge;
import Artifact.Layer.Shape;
import Artifact.Render.CompositionViewDrawing;
import Color.Float;
import Property.Abstract;
import Property.Group;
import Physics.Fluid;
import Physics.Mpm2D;
import Physics.SoftBody;
import Physics.System;
import Utils.Id;

using namespace ArtifactCore;

namespace {

class PhysicsLodSettingsScope {
public:
    PhysicsLodSettingsScope()
        : system_(PhysicsSystem::instance())
        , previous_(system_.physicsLODSettings())
    {
    }

    ~PhysicsLodSettingsScope()
    {
        system_.setPhysicsLODSettings(previous_);
    }

    PhysicsSystem& system() const { return system_; }

private:
    PhysicsSystem& system_;
    PhysicsLODSettings previous_;
};

} // namespace

TEST(PhysicsLodBridgeContractTest, FractureScalesFollowPhysicsLodPreset)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Full;
    scope.system().setPhysicsLODSettings(settings);

    auto fullScale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(fullScale.shards, 1.0f);
    EXPECT_FLOAT_EQ(fullScale.debris, 1.0f);

    settings = PhysicsLODSettings{};
    settings.level = PhysicsLODLevel::Reduced;
    scope.system().setPhysicsLODSettings(settings);
    const auto reducedScale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(reducedScale.shards, 1.0f);
    EXPECT_FLOAT_EQ(reducedScale.debris, 1.0f);

    settings = PhysicsLODSettings{};
    settings.level = PhysicsLODLevel::Minimal;
    scope.system().setPhysicsLODSettings(settings);
    const auto minimalScale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(minimalScale.shards, 0.5f);
    EXPECT_FLOAT_EQ(minimalScale.debris, 0.25f);

    settings = PhysicsLODSettings{};
    settings.level = PhysicsLODLevel::Reduced;
    scope.system().setPhysicsLODSettings(settings);
    const auto restoredReducedScale =
        Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(restoredReducedScale.shards, 1.0f);
    EXPECT_FLOAT_EQ(restoredReducedScale.debris, 1.0f);

    settings.level = PhysicsLODLevel::Full;
    scope.system().setPhysicsLODSettings(settings);
    const auto restoredFullScale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(restoredFullScale.shards, 1.0f);
    EXPECT_FLOAT_EQ(restoredFullScale.debris, 1.0f);
}

TEST(PhysicsLodBridgeContractTest,
     FluidAndLiquidSettingsAreExposedInTheComponentsSurface)
{
    Artifact::ArtifactShapeLayer layer;
    const auto groups = layer.getComponentPropertyGroups();
    const auto findGroup = [&groups](const QString& name)
        -> const PropertyGroup* {
        const auto it = std::find_if(
            groups.begin(), groups.end(), [&name](const PropertyGroup& group) {
                return group.name() == name;
            });
        return it == groups.end() ? nullptr : &*it;
    };

    const PropertyGroup* fluid = findGroup(QStringLiteral("Fluid"));
    const PropertyGroup* liquid = findGroup(QStringLiteral("Liquid"));
    ASSERT_NE(fluid, nullptr);
    ASSERT_NE(liquid, nullptr);
    EXPECT_EQ(fluid->propertyCount(), 21U);
    EXPECT_EQ(liquid->propertyCount(), 18U);

    const auto expectProperty = [](const PropertyGroup* group,
                                   const QString& path,
                                   const PropertyType expectedType) {
        const auto property = group->findProperty(path);
        ASSERT_TRUE(property) << path.toStdString();
        EXPECT_EQ(property->getType(), expectedType);
    };
    expectProperty(fluid, QStringLiteral("component.fluid.mode"),
                   PropertyType::Integer);
    expectProperty(fluid, QStringLiteral("component.fluid.gridWidth"),
                   PropertyType::Integer);
    expectProperty(fluid, QStringLiteral("component.fluid.viscosity"),
                   PropertyType::Float);
    expectProperty(fluid, QStringLiteral("component.fluid.emitterX"),
                   PropertyType::Float);
    expectProperty(fluid, QStringLiteral("component.fluid.emitterY"),
                   PropertyType::Float);
    expectProperty(fluid, QStringLiteral("component.fluid.emitterRadius"),
                   PropertyType::Float);
    expectProperty(fluid, QStringLiteral("component.fluid.emitterCount"),
                   PropertyType::Integer);
    expectProperty(fluid, QStringLiteral("component.fluid.emitterDensity"),
                   PropertyType::Float);
    expectProperty(fluid, QStringLiteral("component.fluid.emitterVelocityX"),
                   PropertyType::Float);
    expectProperty(fluid, QStringLiteral("component.fluid.emitterVelocityY"),
                   PropertyType::Float);
    expectProperty(fluid, QStringLiteral("component.fluid.smokeColor"),
                   PropertyType::Color);
    expectProperty(liquid,
                   QStringLiteral("component.fluid.liquidFillAmount"),
                   PropertyType::Float);
    expectProperty(liquid,
                   QStringLiteral("component.fluid.liquidSurfaceTension"),
                   PropertyType::Float);
    expectProperty(liquid, QStringLiteral("component.fluid.liquidColor"),
                   PropertyType::Color);
    expectProperty(liquid,
                   QStringLiteral("component.fluid.liquidFoamColor"),
                   PropertyType::Color);

    const auto emitterCount = fluid->findProperty(
        QStringLiteral("component.fluid.emitterCount"));
    ASSERT_TRUE(emitterCount);
    EXPECT_EQ(emitterCount->metadata().hardMin.toInt(), 1);
    EXPECT_EQ(emitterCount->metadata().hardMax.toInt(), 64);
    EXPECT_EQ(emitterCount->getValue().toInt(), 7);
    const auto emitterRadius = fluid->findProperty(
        QStringLiteral("component.fluid.emitterRadius"));
    ASSERT_TRUE(emitterRadius);
    EXPECT_FLOAT_EQ(emitterRadius->getValue().toFloat(), 0.16f);
    const auto emitterDensity = fluid->findProperty(
        QStringLiteral("component.fluid.emitterDensity"));
    ASSERT_TRUE(emitterDensity);
    EXPECT_FLOAT_EQ(emitterDensity->getValue().toFloat(), 0.0625f);
    const auto emitterTemperature = fluid->findProperty(
        QStringLiteral("component.fluid.emitterTemperature"));
    ASSERT_TRUE(emitterTemperature);
    EXPECT_FLOAT_EQ(emitterTemperature->getValue().toFloat(), 1.0f);
    const auto emitterVelocityY = fluid->findProperty(
        QStringLiteral("component.fluid.emitterVelocityY"));
    ASSERT_TRUE(emitterVelocityY);
    EXPECT_FLOAT_EQ(emitterVelocityY->getValue().toFloat(), -0.75f);

    const QColor coffeeColor(102, 61, 31, 255);
    const QColor cremaColor(224, 194, 151, 220);
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.mode"), 1));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.emitterCount"), 9));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidColor"), coffeeColor));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidFoamColor"), cremaColor));
    const auto updatedGroups = layer.getComponentPropertyGroups();
    const auto liquidGroup = std::find_if(
        updatedGroups.begin(), updatedGroups.end(), [](const PropertyGroup& group) {
            return group.name() == QStringLiteral("Liquid");
        });
    ASSERT_NE(liquidGroup, updatedGroups.end());
    const auto updatedFluidGroup = std::find_if(
        updatedGroups.begin(), updatedGroups.end(), [](const PropertyGroup& group) {
            return group.name() == QStringLiteral("Fluid");
        });
    ASSERT_NE(updatedFluidGroup, updatedGroups.end());
    const auto updatedEmitterCount = updatedFluidGroup->findProperty(
        QStringLiteral("component.fluid.emitterCount"));
    ASSERT_TRUE(updatedEmitterCount);
    EXPECT_EQ(updatedEmitterCount->getValue().toInt(), 9);
    const auto updatedMode = updatedFluidGroup->findProperty(
        QStringLiteral("component.fluid.mode"));
    ASSERT_TRUE(updatedMode);
    EXPECT_EQ(updatedMode->getValue().toInt(), 1)
        << "the cached Component property should refresh after editing";
    const auto updatedLiquidColor = liquidGroup->findProperty(
        QStringLiteral("component.fluid.liquidColor"));
    ASSERT_TRUE(updatedLiquidColor);
    EXPECT_EQ(updatedLiquidColor->getColorValue(), coffeeColor);

    const auto updatedModeValue = updatedFluidGroup->findProperty(
        QStringLiteral("component.fluid.mode"));
    ASSERT_TRUE(updatedModeValue);
    EXPECT_EQ(updatedModeValue->getValue().toInt(), 1);
}

TEST(PhysicsLodBridgeContractTest,
     LiquidLayerRendersAndWritesCoffeeColoredPngCapture)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(320, 320)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }
    renderer.setCanvasSize(320.0f, 320.0f);
    renderer.setViewportRect(320.0f, 320.0f);
    renderer.setPan(0.0f, 0.0f);
    renderer.setZoom(1.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(
        0.035f, 0.045f, 0.060f, 1.0f));

    Artifact::ArtifactShapeLayer layer;
    layer.setSize(220, 220);
    layer.setFillEnabled(false);
    layer.setStrokeEnabled(false);
    const QColor coffeeColor(102, 61, 31, 255);
    const QColor cremaColor(224, 194, 151, 220);
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.mode"), 1));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidFillAmount"), 0.62));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidColor"), coffeeColor));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidFoamColor"), cremaColor));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidInflowRate"), 24.0));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidInflowSpeed"), 1.2));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidFoamAmount"), 0.35));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidSurfaceOpacity"), 1.0));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidEdgeOpacity"), 0.42));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.enabled"), true));

    const auto captureFrame = [&](std::int64_t frame,
                                  float rotationDegrees = 0.0f) {
        layer.goToFrame(frame);
        layer.transform3D().setPosition(
            ArtifactCore::RationalTime(frame, 30), 50.0f, 50.0f);
        layer.transform3D().setRotation(
            ArtifactCore::RationalTime(frame, 30), rotationDegrees);
        renderer.clear();
        Artifact::drawLayerForCompositionView(&layer, &renderer);
        renderer.flushAndWait();
        return renderer.readbackToImage();
    };
    const QImage initialCapture = captureFrame(0);
    ASSERT_FALSE(initialCapture.isNull());
    const QImage capture = captureFrame(12);
    ASSERT_FALSE(capture.isNull());
    ASSERT_EQ(capture.size(), initialCapture.size());
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidFoamAmount"), 0.0));
    const QImage noFoamCapture = captureFrame(12);
    ASSERT_FALSE(noFoamCapture.isNull());
    ASSERT_EQ(noFoamCapture.size(), capture.size());
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidFillAmount"), 0.35));
    const QImage lowerFillCapture = captureFrame(12);
    ASSERT_FALSE(lowerFillCapture.isNull());
    ASSERT_EQ(lowerFillCapture.size(), capture.size());
    const QImage rotatedLowerFillCapture = captureFrame(12, 25.0f);
    ASSERT_FALSE(rotatedLowerFillCapture.isNull());
    ASSERT_EQ(rotatedLowerFillCapture.size(), capture.size());
    const QColor waterColor(36, 112, 166, 255);
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidColor"), waterColor));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidFoamColor"),
        QColor(192, 224, 239, 220)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("component.fluid.liquidFillAmount"), 0.62));
    const QImage waterCapture = captureFrame(12);
    ASSERT_FALSE(waterCapture.isNull());
    ASSERT_EQ(waterCapture.size(), capture.size());

    int coffeePixels = 0;
    int coffeeStreamPixels = 0;
    int foamContributionPixels = 0;
    int settledSurfaceFoamPixels = 0;
    int foamMinimumX = capture.width();
    int foamMaximumX = -1;
    int foamMinimumY = capture.height();
    int foamMaximumY = -1;
    int coffeeCorePixels = 0;
    int coffeeRedSum = 0;
    int coffeeGreenSum = 0;
    int coffeeBlueSum = 0;
    int coffeeMinX = capture.width();
    int coffeeMaxX = -1;
    int coffeeMaxY = -1;
    int outsideContainerPixels = 0;
    int changedPixels = 0;
    int darkestSurface = 255;
    int brightestSurface = 0;
    for (int y = 0; y < capture.height(); ++y) {
        for (int x = 0; x < capture.width(); ++x) {
            const QRgb pixel = capture.pixel(x, y);
            if ((x < 48 || x > 271 || y > 272) && y >= 100) {
                const QRgb background = capture.pixel(10, 160);
                if (std::abs(qRed(pixel) - qRed(background)) > 12 ||
                    std::abs(qGreen(pixel) - qGreen(background)) > 12 ||
                    std::abs(qBlue(pixel) - qBlue(background)) > 12) {
                    ++outsideContainerPixels;
                }
            }
            if (qAlpha(pixel) > 16 && qRed(pixel) > qGreen(pixel) + 12 &&
                qGreen(pixel) > qBlue(pixel) + 12) {
                ++coffeePixels;
                coffeeMinX = std::min(coffeeMinX, x);
                coffeeMaxX = std::max(coffeeMaxX, x);
                coffeeMaxY = std::max(coffeeMaxY, y);
                if (y < 120 &&
                    std::abs(x - capture.width() / 2) < 40) {
                    ++coffeeStreamPixels;
                }
                if (qRed(pixel) < 145 && qGreen(pixel) < 110 &&
                    qBlue(pixel) < 85) {
                    ++coffeeCorePixels;
                    coffeeRedSum += qRed(pixel);
                    coffeeGreenSum += qGreen(pixel);
                    coffeeBlueSum += qBlue(pixel);
                }
            }
            if (qAlpha(pixel) > 16 && qRed(pixel) > qGreen(pixel) + 4 &&
                qGreen(pixel) > qBlue(pixel) + 4) {
                const int luminance =
                    (qRed(pixel) * 3 + qGreen(pixel) * 6 + qBlue(pixel)) / 10;
                darkestSurface = std::min(darkestSurface, luminance);
                brightestSurface = std::max(brightestSurface, luminance);
            }
            const QRgb initialPixel = initialCapture.pixel(x, y);
            if (std::abs(qRed(pixel) - qRed(initialPixel)) > 2 ||
                std::abs(qGreen(pixel) - qGreen(initialPixel)) > 2 ||
                std::abs(qBlue(pixel) - qBlue(initialPixel)) > 2 ||
                std::abs(qAlpha(pixel) - qAlpha(initialPixel)) > 2) {
                ++changedPixels;
            }
            const QRgb noFoamPixel = noFoamCapture.pixel(x, y);
            if (std::abs(qRed(pixel) - qRed(noFoamPixel)) > 3 ||
                std::abs(qGreen(pixel) - qGreen(noFoamPixel)) > 3 ||
                std::abs(qBlue(pixel) - qBlue(noFoamPixel)) > 3) {
                ++foamContributionPixels;
                foamMinimumX = std::min(foamMinimumX, x);
                foamMaximumX = std::max(foamMaximumX, x);
                foamMinimumY = std::min(foamMinimumY, y);
                foamMaximumY = std::max(foamMaximumY, y);
                if (std::abs(y - capture.height() / 2) <
                        capture.height() * 0.06 &&
                    x > capture.width() / 5 &&
                    x < capture.width() * 4 / 5) {
                    ++settledSurfaceFoamPixels;
                }
            }
        }
    }
    EXPECT_GT(coffeePixels, 0)
        << "the rendered layer should contain pixels tinted by liquidColor";
    EXPECT_GT(coffeeMinX, 20)
        << "the liquid capture should leave a visible left margin";
    EXPECT_LT(coffeeMaxX, capture.width() - 20)
        << "the liquid capture should leave a visible right margin";
    EXPECT_GE(coffeeMinX, 49)
        << "the reconstructed liquid must stay inside the left container wall";
    EXPECT_LE(coffeeMaxX, 270)
        << "the reconstructed liquid must stay inside the right container wall";
    EXPECT_LE(coffeeMaxY, 271)
        << "the reconstructed liquid must stay above the closed container bottom";
    EXPECT_LE(outsideContainerPixels, 4)
        << "liquid surface and contour pixels must stay inside the container walls";
    EXPECT_GT(coffeeStreamPixels, 100)
        << "the configured inflow should render above the settled liquid surface";
    EXPECT_GT(foamContributionPixels, 50)
        << "the rendered foam should change pixels independently of surface shading";
    EXPECT_GT(settledSurfaceFoamPixels, 30)
        << "non-zero foam should leave a visible crema band on settled liquid";
    EXPECT_GT(foamMaximumX - foamMinimumX, foamMaximumY - foamMinimumY)
        << "foam over a downward stream should spread sideways along the liquid surface";
    ASSERT_GT(coffeeCorePixels, 0)
        << "the liquid color should remain visible beneath the highlights";
    EXPECT_LT(std::abs(coffeeRedSum / coffeeCorePixels - coffeeColor.red()), 35);
    EXPECT_LT(std::abs(coffeeGreenSum / coffeeCorePixels - coffeeColor.green()), 30);
    EXPECT_LT(std::abs(coffeeBlueSum / coffeeCorePixels - coffeeColor.blue()), 25);
    EXPECT_GT(brightestSurface - darkestSurface, 18)
        << "the liquid surface should contain visible shading variation";
    const auto luminance = [](QRgb pixel) {
        return (qRed(pixel) * 3 + qGreen(pixel) * 6 + qBlue(pixel)) / 10;
    };
    const int contactReflectionLuminance = luminance(
        noFoamCapture.pixel(noFoamCapture.width() / 2,
                            noFoamCapture.height() / 2 + 10));
    const int lateralPoolLuminance = luminance(
        noFoamCapture.pixel(noFoamCapture.width() / 4,
                            noFoamCapture.height() / 2 + 10));
    EXPECT_GT(contactReflectionLuminance, lateralPoolLuminance + 6)
        << "the liquid should keep a broad contact reflection when foam is disabled";
    const auto poolSurfaceY = [](const QImage& image) {
        const int x = image.width() / 3;
        for (int y = image.height() / 4; y < image.height() * 3 / 4; ++y) {
            const QRgb pixel = image.pixel(x, y);
            if (qAlpha(pixel) > 16 && qRed(pixel) > qGreen(pixel) + 12 &&
                qGreen(pixel) > qBlue(pixel) + 12) {
                return y;
            }
        }
        return -1;
    };
    const int fullFillSurfaceY = poolSurfaceY(noFoamCapture);
    const int lowerFillSurfaceY = poolSurfaceY(lowerFillCapture);
    ASSERT_GE(fullFillSurfaceY, 0)
        << "the default fill should have a detectable liquid edge";
    ASSERT_GE(lowerFillSurfaceY, 0)
        << "the reduced fill should have a detectable liquid edge";
    int waterSurfaceY = -1;
    for (int y = waterCapture.height() / 4;
         y < waterCapture.height() * 3 / 4; ++y) {
        const QRgb pixel = waterCapture.pixel(waterCapture.width() / 3, y);
        if (qAlpha(pixel) > 16 && qBlue(pixel) > qGreen(pixel) + 12 &&
            qGreen(pixel) > qRed(pixel) + 12) {
            waterSurfaceY = y;
            break;
        }
    }
    ASSERT_GE(waterSurfaceY, 0)
        << "the water capture should have a detectable pool surface";
    EXPECT_LT(std::abs(waterSurfaceY - fullFillSurfaceY), 15)
        << "changing liquid color should preserve the configured fill height";
    EXPECT_GT(lowerFillSurfaceY, fullFillSurfaceY + 20)
        << "the rendered liquid surface should follow the configured fill amount";
    const int lowerFillContactReflectionLuminance = luminance(
        lowerFillCapture.pixel(lowerFillCapture.width() / 2,
                               lowerFillSurfaceY + 10));
    const int lowerFillLateralPoolLuminance = luminance(
        lowerFillCapture.pixel(lowerFillCapture.width() / 4,
                               lowerFillSurfaceY + 10));
    EXPECT_GT(lowerFillContactReflectionLuminance,
              lowerFillLateralPoolLuminance + 8)
        << "the contact reflection should follow the reduced liquid surface";
    int rotatedCoffeePixels = 0;
    int rotatedDarkestSurface = 255;
    int rotatedBrightestSurface = 0;
    for (int y = 0; y < rotatedLowerFillCapture.height(); ++y) {
        for (int x = 0; x < rotatedLowerFillCapture.width(); ++x) {
            const QRgb pixel = rotatedLowerFillCapture.pixel(x, y);
            if (qAlpha(pixel) > 16 && qRed(pixel) > qGreen(pixel) + 4 &&
                qGreen(pixel) > qBlue(pixel) + 4) {
                ++rotatedCoffeePixels;
                const int pixelLuminance = luminance(pixel);
                rotatedDarkestSurface =
                    std::min(rotatedDarkestSurface, pixelLuminance);
                rotatedBrightestSurface =
                    std::max(rotatedBrightestSurface, pixelLuminance);
            }
        }
    }
    EXPECT_GT(rotatedCoffeePixels, 100)
        << "rotating the liquid layer should preserve its rendered surface";
    EXPECT_GT(rotatedBrightestSurface - rotatedDarkestSurface, 18)
        << "rotated liquid shading should retain visible tonal variation";
    int waterPixels = 0;
    int waterRedSum = 0;
    int waterGreenSum = 0;
    int waterBlueSum = 0;
    for (int y = 0; y < waterCapture.height(); ++y) {
        for (int x = 0; x < waterCapture.width(); ++x) {
            const QRgb pixel = waterCapture.pixel(x, y);
            if (qAlpha(pixel) > 16 && qGreen(pixel) > qRed(pixel) + 12 &&
                qBlue(pixel) > qGreen(pixel) + 12 && qBlue(pixel) < 220) {
                ++waterPixels;
                waterRedSum += qRed(pixel);
                waterGreenSum += qGreen(pixel);
                waterBlueSum += qBlue(pixel);
            }
        }
    }
    ASSERT_GT(waterPixels, 100)
        << "the same liquid surface path should render water colors";
    EXPECT_LT(std::abs(waterRedSum / waterPixels - waterColor.red()), 45);
    EXPECT_LT(std::abs(waterGreenSum / waterPixels - waterColor.green()), 45);
    EXPECT_LT(std::abs(waterBlueSum / waterPixels - waterColor.blue()), 45);
    EXPECT_GT(changedPixels, 1000)
        << "the liquid should visibly change between frame 0 and frame 12";

    QString captureDirectory = qEnvironmentVariable(
        "ARTIFACT_FLUID_CAPTURE_DIR");
    if (captureDirectory.isEmpty()) {
        captureDirectory = QDir::tempPath() +
                           QStringLiteral("/ArtifactStudioFluidCapture");
    }
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const QString initialCapturePath = QDir(captureDirectory).filePath(
        QStringLiteral("liquid-coffee-frame-00.png"));
    ASSERT_TRUE(initialCapture.save(initialCapturePath, "PNG"))
        << initialCapturePath.toStdString();
    const QString capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("liquid-coffee-frame-12.png"));
    ASSERT_TRUE(capture.save(capturePath, "PNG")) << capturePath.toStdString();
    const QString noFoamCapturePath = QDir(captureDirectory).filePath(
        QStringLiteral("liquid-coffee-frame-12-no-foam.png"));
    ASSERT_TRUE(noFoamCapture.save(noFoamCapturePath, "PNG"))
        << noFoamCapturePath.toStdString();
    const QString lowerFillCapturePath = QDir(captureDirectory).filePath(
        QStringLiteral("liquid-coffee-frame-12-lower-fill.png"));
    ASSERT_TRUE(lowerFillCapture.save(lowerFillCapturePath, "PNG"))
        << lowerFillCapturePath.toStdString();
    const QString rotatedLowerFillCapturePath = QDir(captureDirectory).filePath(
        QStringLiteral("liquid-coffee-frame-12-lower-fill-rotated.png"));
    ASSERT_TRUE(rotatedLowerFillCapture.save(rotatedLowerFillCapturePath, "PNG"))
        << rotatedLowerFillCapturePath.toStdString();
    const QString waterCapturePath = QDir(captureDirectory).filePath(
        QStringLiteral("liquid-water-frame-12.png"));
    ASSERT_TRUE(waterCapture.save(waterCapturePath, "PNG"))
        << waterCapturePath.toStdString();
    EXPECT_GT(QFileInfo(capturePath).size(), 0);
    SUCCEED() << "PNG capture: " << capturePath.toStdString()
              << "; coffee-colored pixels: " << coffeePixels
              << "; surface luminance span: "
              << brightestSurface - darkestSurface
              << "; pixels changed since frame 0: " << changedPixels;
}

TEST(PhysicsLodBridgeContractTest, ExplicitFractureScalesPassThroughTheBridge)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Reduced;
    settings.fractureShardScale = 0.7f;
    settings.fractureDebrisScale = 0.4f;
    scope.system().setPhysicsLODSettings(settings);

    const auto scale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(scale.shards, 0.7f);
    EXPECT_FLOAT_EQ(scale.debris, 0.4f);
}

TEST(PhysicsLodBridgeContractTest,
     ReducedAndMinimalPresetsScaleSimulationBudgetsAndAllowOverrides)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Reduced;
    scope.system().setPhysicsLODSettings(settings);

    const auto& reduced = scope.system().physicsLODSettings();
    EXPECT_FLOAT_EQ(reduced.targetHz, 30.0f);
    EXPECT_EQ(reduced.rigidBodySubSteps, 2);
    EXPECT_EQ(reduced.softBodyMaxSubSteps, 4);
    EXPECT_EQ(reduced.softBodyConstraintIterations, 3);
    EXPECT_EQ(reduced.softBodyCollisionIterations, 1);
    EXPECT_FLOAT_EQ(reduced.softBodyGridScale, 0.75f);
    EXPECT_EQ(reduced.fluidSolverIterations, 10);
    EXPECT_FLOAT_EQ(reduced.fluidResolutionScale, 0.5f);
    EXPECT_EQ(reduced.materialMaxSubSteps, 256);
    EXPECT_TRUE(reduced.applySleepPolicy);
    EXPECT_FLOAT_EQ(reduced.sleepThreshold, 0.5f);

    settings = PhysicsLODSettings{};
    settings.level = PhysicsLODLevel::Minimal;
    settings.rigidBodySubSteps = 3;
    settings.fluidResolutionScale = 0.4f;
    scope.system().setPhysicsLODSettings(settings);

    const auto& minimal = scope.system().physicsLODSettings();
    EXPECT_FLOAT_EQ(minimal.targetHz, 15.0f);
    EXPECT_EQ(minimal.rigidBodySubSteps, 3)
        << "explicit substep budgets should override preset defaults";
    EXPECT_EQ(minimal.softBodyMaxSubSteps, 2);
    EXPECT_EQ(minimal.softBodyConstraintIterations, 1);
    EXPECT_EQ(minimal.softBodyCollisionIterations, 1);
    EXPECT_FLOAT_EQ(minimal.softBodyGridScale, 0.5f);
    EXPECT_EQ(minimal.fluidSolverIterations, 5);
    EXPECT_FLOAT_EQ(minimal.fluidResolutionScale, 0.4f)
        << "explicit fluid resolution should override preset defaults";
    EXPECT_EQ(minimal.materialMaxSubSteps, 128);
    EXPECT_TRUE(minimal.disableSoftBodySelfCollision);
    EXPECT_TRUE(minimal.disableContinuousCollision);
    EXPECT_TRUE(minimal.simplifyCollisionMesh);
    EXPECT_TRUE(minimal.disableFracture);
    EXPECT_TRUE(minimal.applySleepPolicy);
    EXPECT_FLOAT_EQ(minimal.sleepThreshold, 1.0f);
}

TEST(PhysicsLodBridgeContractTest,
     PhysicsLODClampsResolutionScalesAndNegativeBudgets)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Full;
    settings.targetHz = -5.0f;
    settings.rigidBodySubSteps = -1;
    settings.softBodyMaxSubSteps = -2;
    settings.softBodyConstraintIterations = -3;
    settings.softBodyCollisionIterations = -4;
    settings.softBodyGridScale = 0.1f;
    settings.fluidSolverIterations = -5;
    settings.fluidResolutionScale = 0.05f;
    settings.materialMaxSubSteps = -6;
    scope.system().setPhysicsLODSettings(settings);

    auto actual = scope.system().physicsLODSettings();
    EXPECT_FLOAT_EQ(actual.targetHz, 0.0f);
    EXPECT_EQ(actual.rigidBodySubSteps, 0);
    EXPECT_EQ(actual.softBodyMaxSubSteps, 0);
    EXPECT_EQ(actual.softBodyConstraintIterations, 0);
    EXPECT_EQ(actual.softBodyCollisionIterations, 0);
    EXPECT_FLOAT_EQ(actual.softBodyGridScale, 0.25f);
    EXPECT_EQ(actual.fluidSolverIterations, 0);
    EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 0.125f);
    EXPECT_EQ(actual.materialMaxSubSteps, 0);

    settings.softBodyGridScale = 2.0f;
    settings.fluidResolutionScale = 2.0f;
    scope.system().setPhysicsLODSettings(settings);
    actual = scope.system().physicsLODSettings();
    EXPECT_FLOAT_EQ(actual.softBodyGridScale, 1.0f);
    EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 1.0f);
}

TEST(PhysicsLodBridgeContractTest, FrozenLODStopsRegisteredMaterialSimulation)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-frozen-probe"));
    struct MaterialSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~MaterialSolverCleanup() { system.unregisterMaterialSolver(layerId); }
    } cleanup{system, layerId};

    auto solver = system.createMaterialGrid(
        layerId, 20.0f, 20.0f, 40.0f, 40.0f, 4, 4,
        MpmMaterialPreset::Flesh, 1000.0f);
    ASSERT_TRUE(solver);
    ASSERT_GT(solver->particleCount(), 0);
    solver->setGravity(0.0f, 980.0f);

    PhysicsLODSettings fullSettings;
    fullSettings.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(fullSettings);
    for (int step = 0; step < 4; ++step) {
        system.update(1.0f / 60.0f, 0.0f, 980.0f);
    }
    const MpmVec2 evolvedPosition = solver->particle(0).pos;
    const MpmVec2 evolvedVelocity = solver->particle(0).vel;
    ASSERT_TRUE(std::isfinite(evolvedPosition.x));
    ASSERT_TRUE(std::isfinite(evolvedPosition.y));
    ASSERT_NE(evolvedPosition.y, 20.0f)
        << "the probe material should evolve before testing Frozen LOD";

    PhysicsLODSettings frozenSettings;
    frozenSettings.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozenSettings);
    for (int step = 0; step < 12; ++step) {
        system.update(1.0f / 30.0f, 0.0f, 980.0f);
    }

    EXPECT_FLOAT_EQ(solver->particle(0).pos.x, evolvedPosition.x);
    EXPECT_FLOAT_EQ(solver->particle(0).pos.y, evolvedPosition.y);
    EXPECT_FLOAT_EQ(solver->particle(0).vel.x, evolvedVelocity.x);
    EXPECT_FLOAT_EQ(solver->particle(0).vel.y, evolvedVelocity.y);
}

TEST(PhysicsLodBridgeContractTest,
     ReducedAndMinimalLODApplyMaterialSubstepBudgetsToRegisteredSolver)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-material-budget-probe"));
    struct MaterialSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~MaterialSolverCleanup() { system.unregisterMaterialSolver(layerId); }
    } cleanup{system, layerId};

    auto solver = system.createMaterialGrid(
        layerId, 20.0f, 20.0f, 40.0f, 40.0f, 2, 2,
        MpmMaterialPreset::Flesh, 1000.0f);
    ASSERT_TRUE(solver);
    EXPECT_EQ(solver->maxSubsteps(), 512);

    PhysicsLODSettings reducedSettings;
    reducedSettings.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reducedSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->maxSubsteps(), 256)
        << "Reduced LOD should cap the registered material solver at its preset budget";

    PhysicsLODSettings minimalSettings;
    minimalSettings.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimalSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->maxSubsteps(), 128)
        << "Minimal LOD should apply the lower material solver budget";
}

TEST(PhysicsLodBridgeContractTest,
     ReducedTargetHzDefersMaterialSimulationUntilItsUpdateInterval)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-target-hz-probe"));
    struct MaterialSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~MaterialSolverCleanup() { system.unregisterMaterialSolver(layerId); }
    } cleanup{system, layerId};

    auto solver = system.createMaterialGrid(
        layerId, 20.0f, 20.0f, 40.0f, 40.0f, 4, 4,
        MpmMaterialPreset::Flesh, 1000.0f);
    ASSERT_TRUE(solver);
    solver->setGravity(0.0f, 980.0f);
    const float initialY = solver->particle(0).pos.y;
    ASSERT_TRUE(std::isfinite(initialY));

    PhysicsLODSettings fullSettings;
    fullSettings.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(fullSettings);
    PhysicsLODSettings reducedSettings;
    reducedSettings.level = PhysicsLODLevel::Reduced;
    reducedSettings.targetHz = 30.0f;
    system.setPhysicsLODSettings(reducedSettings);

    constexpr float smallFrameDelta = 0.005f;
    for (int step = 0; step < 6; ++step) {
        system.update(smallFrameDelta, 0.0f, 980.0f);
    }
    EXPECT_FLOAT_EQ(solver->particle(0).pos.y, initialY)
        << "the solver should not advance before the 30 Hz interval elapses";

    system.update(0.01f, 0.0f, 980.0f);
    EXPECT_NE(solver->particle(0).pos.y, initialY)
        << "the accumulated simulation should advance after crossing the interval";
}

TEST(PhysicsLodBridgeContractTest,
     UnknownPhysicsLODLevelFallsBackToFullMaterialSimulation)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-unknown-probe"));
    struct MaterialSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~MaterialSolverCleanup() { system.unregisterMaterialSolver(layerId); }
    } cleanup{system, layerId};

    auto solver = system.createMaterialGrid(
        layerId, 20.0f, 20.0f, 40.0f, 40.0f, 4, 4,
        MpmMaterialPreset::Flesh, 1000.0f);
    ASSERT_TRUE(solver);
    solver->setGravity(0.0f, 980.0f);
    const float initialY = solver->particle(0).pos.y;

    PhysicsLODSettings unknownSettings;
    unknownSettings.level = static_cast<PhysicsLODLevel>(255);
    system.setPhysicsLODSettings(unknownSettings);
    system.update(1.0f / 60.0f, 0.0f, 980.0f);

    EXPECT_NE(solver->particle(0).pos.y, initialY)
        << "an unrecognized level should not accidentally freeze simulation";
    const auto fractureScale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(fractureScale.shards, 1.0f);
    EXPECT_FLOAT_EQ(fractureScale.debris, 1.0f);
}

TEST(PhysicsLodBridgeContractTest,
     ReducedAndMinimalLODApplyFluidIterationBudgetsToRegisteredSolver)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-fluid-budget-probe"));
    struct FluidSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~FluidSolverCleanup() { system.unregisterFluidSolver(layerId); }
    } cleanup{system, layerId};

    auto solver = system.createFluidSolver(layerId, 24, 24);
    ASSERT_TRUE(solver);
    EXPECT_EQ(solver->solverIterations(), 20);

    PhysicsLODSettings reducedSettings;
    reducedSettings.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reducedSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->solverIterations(), 10)
        << "Reduced LOD should lower the registered fluid solver iteration budget";

    PhysicsLODSettings minimalSettings;
    minimalSettings.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimalSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->solverIterations(), 5)
        << "Minimal LOD should apply the smallest preset fluid iteration budget";

    PhysicsLODSettings oversizedSettings;
    oversizedSettings.level = PhysicsLODLevel::Full;
    oversizedSettings.fluidSolverIterations = 100;
    system.setPhysicsLODSettings(oversizedSettings);
    system.update(1.0f / 60.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->solverIterations(), 12)
        << "the fluid solver should clamp an explicit LOD budget to its maximum";
}

TEST(PhysicsLodBridgeContractTest,
     SoftBodyGridLODReducesFromOriginalResolutionAndRestoresFull)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-soft-body-grid-probe"));
    struct SoftBodyCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~SoftBodyCleanup() { system.unregisterSoftBody(layerId); }
    } cleanup{system, layerId};
    const LayerID tinyLayerId(
        QStringLiteral("physics-lod-soft-body-tiny-grid-probe"));
    SoftBodyCleanup tinyCleanup{system, tinyLayerId};

    auto solver = system.createSoftBodyGrid(
        layerId, 0.0f, 0.0f, 80.0f, 80.0f, 9, 9);
    auto tinySolver = system.createSoftBodyGrid(
        tinyLayerId, 0.0f, 0.0f, 20.0f, 20.0f, 3, 3);
    ASSERT_TRUE(solver);
    ASSERT_TRUE(tinySolver);
    ASSERT_EQ(solver->gridColumns(), 9);
    ASSERT_EQ(solver->gridRows(), 9);
    ASSERT_EQ(solver->pointCount(), 81u);
    ASSERT_EQ(tinySolver->pointCount(), 9u);

    PhysicsLODSettings fullSettings;
    fullSettings.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(fullSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);

    PhysicsLODSettings reducedSettings;
    reducedSettings.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reducedSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->gridColumns(), 7);
    EXPECT_EQ(solver->gridRows(), 7);
    EXPECT_EQ(solver->pointCount(), 49u);
    EXPECT_EQ(solver->maxSubsteps(), 4);

    PhysicsLODSettings minimalSettings;
    minimalSettings.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimalSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->gridColumns(), 5)
        << "Minimal should resample the original grid, not compound Reduced";
    EXPECT_EQ(solver->gridRows(), 5);
    EXPECT_EQ(solver->pointCount(), 25u);
    EXPECT_EQ(solver->maxSubsteps(), 2);
    EXPECT_EQ(tinySolver->gridColumns(), 2)
        << "Minimal LOD should retain the solver's two-point-per-axis floor";
    EXPECT_EQ(tinySolver->gridRows(), 2);
    EXPECT_EQ(tinySolver->pointCount(), 4u);

    system.setPhysicsLODSettings(fullSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->gridColumns(), 9);
    EXPECT_EQ(solver->gridRows(), 9);
    EXPECT_EQ(solver->pointCount(), 81u);
    EXPECT_EQ(tinySolver->gridColumns(), 3);
    EXPECT_EQ(tinySolver->gridRows(), 3);
    EXPECT_EQ(tinySolver->pointCount(), 9u);

    PhysicsLODSettings oversizedBudgetSettings;
    oversizedBudgetSettings.level = PhysicsLODLevel::Full;
    oversizedBudgetSettings.softBodyMaxSubSteps = 100;
    system.setPhysicsLODSettings(oversizedBudgetSettings);
    system.update(1.0f / 60.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->maxSubsteps(), 32)
        << "the soft-body solver should clamp an explicit LOD budget to its maximum";
}

