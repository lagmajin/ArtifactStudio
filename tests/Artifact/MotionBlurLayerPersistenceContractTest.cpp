#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>
#include <QVariant>

#include <cmath>
#include <memory>

import Artifact.Layer.Camera;
import Artifact.Layer.Solid2D;
import Math.Vec;
import Memory.SharedPtr;
import Undo.UndoManager;

namespace {

const QString kMotionBlurEnabled = QStringLiteral("layer.2_5d.motionBlurEnabled");
const QString kMotionBlurShutter = QStringLiteral("layer.2_5d.motionBlurShutterAngle");
const QString kMotionBlurSamples = QStringLiteral("layer.2_5d.motionBlurSamples");

} // namespace

TEST(MotionBlurLayerPersistenceContractTest, TwoPointFiveDSettingsSurviveProjectJson)
{
    auto layer = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(layer);
    ASSERT_TRUE(layer->setLayerPropertyValue(
        QStringLiteral("layer.2_5d.enabled"), true));
    ASSERT_TRUE(layer->setLayerPropertyValue(kMotionBlurEnabled, true));
    ASSERT_TRUE(layer->setLayerPropertyValue(kMotionBlurShutter, 270.0));
    ASSERT_TRUE(layer->setLayerPropertyValue(kMotionBlurSamples, 6));

    const QJsonObject json = layer->toJson();
    const QJsonObject twoPointFiveD = json.value(QStringLiteral("twoPointFiveD")).toObject();
    ASSERT_FALSE(twoPointFiveD.isEmpty());
    EXPECT_TRUE(twoPointFiveD.value(QStringLiteral("motionBlurEnabled")).toBool());
    EXPECT_NEAR(twoPointFiveD.value(QStringLiteral("motionBlurShutterAngle")).toDouble(),
                270.0, 1e-4);
    EXPECT_EQ(twoPointFiveD.value(QStringLiteral("motionBlurSamples")).toInt(), 6);

    auto restored = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(restored);
    restored->fromJsonProperties(json);
    // The 2.5D setters write layer state directly, so the property cache is
    // populated by building the groups, as the Inspector does on refresh.
    (void)restored->getLayerPropertyGroups();

    const auto enabled = restored->getProperty(kMotionBlurEnabled);
    ASSERT_NE(enabled.get(), nullptr);
    EXPECT_TRUE(enabled->value().toBool());

    const auto shutter = restored->getProperty(kMotionBlurShutter);
    ASSERT_NE(shutter.get(), nullptr);
    EXPECT_NEAR(shutter->value().toDouble(), 270.0, 1e-4);

    const auto samples = restored->getProperty(kMotionBlurSamples);
    ASSERT_NE(samples.get(), nullptr);
    EXPECT_EQ(samples->value().toInt(), 6);
}

TEST(MotionBlurLayerPersistenceContractTest, MotionBlurToggleIsUndoable)
{
    auto layer = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(layer);
    ASSERT_TRUE(layer->setLayerPropertyValue(
        QStringLiteral("layer.2_5d.enabled"), true));
    ASSERT_TRUE(layer->setLayerPropertyValue(kMotionBlurEnabled, true));
    // Register the cached property the way the Inspector does on refresh; the
    // 2.5D setters write layer state directly instead of the property cache.
    (void)layer->getLayerPropertyGroups();

    Artifact::UndoManager manager;
    ASSERT_TRUE(manager.push(std::make_unique<Artifact::SetLayerPropertyValueCommand>(
        layer, kMotionBlurEnabled, QVariant(true), QVariant(false),
        QStringLiteral("Disable 2.5D Motion Blur"))));
    (void)layer->getLayerPropertyGroups();
    {
        const auto disabled = layer->getProperty(kMotionBlurEnabled);
        ASSERT_NE(disabled.get(), nullptr);
        EXPECT_FALSE(disabled->value().toBool());
    }

    manager.undo();
    (void)layer->getLayerPropertyGroups();
    {
        const auto restored = layer->getProperty(kMotionBlurEnabled);
        ASSERT_NE(restored.get(), nullptr);
        EXPECT_TRUE(restored->value().toBool());
    }

    manager.redo();
    (void)layer->getLayerPropertyGroups();
    {
        const auto redisabled = layer->getProperty(kMotionBlurEnabled);
        ASSERT_NE(redisabled.get(), nullptr);
        EXPECT_FALSE(redisabled->value().toBool());
    }
}

TEST(MotionBlurLayerPersistenceContractTest, CameraMotionBlurSurvivesProjectJson)
{
    auto camera = ArtifactCore::makeShared<Artifact::ArtifactCameraLayer>();
    ASSERT_TRUE(camera);
    camera->setMotionBlur(true);
    camera->setBlurAmount(ArtifactCore::Units::Percent{75.0f});

    EXPECT_TRUE(camera->motionBlur());
    EXPECT_NEAR(camera->blurAmount().value, 75.0, 1e-4);

    const QJsonObject json = camera->toJson();
    EXPECT_TRUE(json.value(QStringLiteral("cameraMotionBlur")).toBool());
    EXPECT_NEAR(json.value(QStringLiteral("cameraBlurAmount")).toDouble(),
                75.0, 1e-4);

    auto restored = ArtifactCore::makeShared<Artifact::ArtifactCameraLayer>();
    ASSERT_TRUE(restored);
    restored->fromJsonProperties(json);

    EXPECT_TRUE(restored->motionBlur());
    EXPECT_NEAR(restored->blurAmount().value, 75.0, 1e-4);
}
