#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>
#include <QVariant>

import Artifact.Layer.Light;
import Artifact.Layers.Model3D;
import Color.Float;
import Math.Vec;

namespace {

class LayerTestEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "ArtifactModel3DAndLightLayerTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QGuiApplication(argc, argv);
        QCoreApplication::setApplicationName(
            QStringLiteral("ArtifactModel3DAndLightLayerTest"));
        QStandardPaths::setTestModeEnabled(true);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QGuiApplication* application_ = nullptr;
};

auto* const layerTestEnvironment = static_cast<LayerTestEnvironment*>(
    testing::AddGlobalTestEnvironment(new LayerTestEnvironment()));

QString modelFixturePath()
{
    return QString::fromUtf8(ARTIFACT_TEST_MODEL_OBJ_PATH);
}

} // namespace

TEST(Model3DLayerTest, CreatesDefaultGeometryAndRoundTripsSettings)
{
    Artifact::Artifact3DLayer source;
    ASSERT_TRUE(source.is3D());
    EXPECT_EQ(source.fixedGeometry(), Artifact::FixedGeometry3D::Auto);
    EXPECT_EQ(source.mesh().vertexCount(), 24);
    EXPECT_EQ(source.mesh().polygonCount(), 6);

    source.setFixedGeometry(Artifact::FixedGeometry3D::Sphere);
    source.setRenderMode(Artifact::ModelRenderMode::Wireframe);
    source.setAffectedByLights(false);

    Artifact::Artifact3DLayer restored;
    restored.fromJsonProperties(source.toJson());

    EXPECT_EQ(restored.fixedGeometry(), Artifact::FixedGeometry3D::Sphere);
    EXPECT_EQ(restored.renderMode(), Artifact::ModelRenderMode::Wireframe);
    EXPECT_GT(restored.mesh().vertexCount(), 0);
    EXPECT_FALSE(restored.toJson().value(
        QStringLiteral("render.affectedByLights")).toBool());
}

TEST(Model3DLayerTest, LoadsObjFixtureAndRestoresSource)
{
    Artifact::Artifact3DLayer source;
    source.loadFromFile(modelFixturePath());

    ASSERT_EQ(source.sourcePath(), modelFixturePath());
    ASSERT_GT(source.mesh().vertexCount(), 0);
    EXPECT_FALSE(source.localBounds().isEmpty());

    Artifact::Artifact3DLayer restored;
    restored.fromJsonProperties(source.toJson());

    EXPECT_EQ(restored.sourcePath(), source.sourcePath());
    EXPECT_GT(restored.mesh().vertexCount(), 0);
}

TEST(LightLayerTest, CreatesAsNonRenderingThreeDimensionalLayer)
{
    Artifact::ArtifactLightLayer light;

    EXPECT_TRUE(light.is3D());
    EXPECT_FALSE(light.shouldIncludeInFinalRender());
    EXPECT_EQ(light.className().toQString(),
              QStringLiteral("ArtifactLightLayer"));
    EXPECT_EQ(light.lightType(), Artifact::LightType::Point);
}

TEST(LightLayerTest, ClampsAndRoundTripsSpotAndLinkSettings)
{
    Artifact::ArtifactLightLayer source;
    source.setLightType(Artifact::LightType::Spot);
    source.setColor(ArtifactCore::FloatColor(0.25f, 0.5f, 0.75f, 1.0f));
    source.setIntensity(ArtifactCore::Units::Percent{135.0f});
    source.setRange(ArtifactCore::Units::Pixels{420.0f});
    source.setConeAngle(ArtifactCore::Units::Degrees{62.0f});
    source.setConeFeather(ArtifactCore::Units::Degrees{18.0f});
    source.setConeLength(ArtifactCore::Units::Pixels{640.0f});
    source.setGoboTexturePath(QStringLiteral("textures/pattern.png"));
    source.setGoboIntensity(1.5f);
    source.setGoboRotation(ArtifactCore::Units::Degrees{725.0f});
    source.setGoboInvert(true);
    source.setLightLinkMode(Artifact::LightLinkMode::IncludeOnly);
    source.setLinkedLayerIdsText(QStringLiteral("layer-a,layer-b"));

    EXPECT_FLOAT_EQ(source.goboIntensity(), 1.0f);
    EXPECT_FLOAT_EQ(source.goboRotation().value, 5.0f);

    Artifact::ArtifactLightLayer restored;
    restored.fromJsonProperties(source.toJson());

    EXPECT_EQ(restored.lightType(), Artifact::LightType::Spot);
    EXPECT_FLOAT_EQ(restored.color().r(), 0.25f);
    EXPECT_FLOAT_EQ(restored.color().g(), 0.5f);
    EXPECT_FLOAT_EQ(restored.intensity().value, 135.0f);
    EXPECT_FLOAT_EQ(restored.range().value, 420.0f);
    EXPECT_FLOAT_EQ(restored.coneAngle().value, 62.0f);
    EXPECT_FLOAT_EQ(restored.coneFeather().value, 18.0f);
    EXPECT_FLOAT_EQ(restored.coneLength().value, 640.0f);
    EXPECT_EQ(restored.goboTexturePath(),
              QStringLiteral("textures/pattern.png"));
    EXPECT_FLOAT_EQ(restored.goboIntensity(), 1.0f);
    EXPECT_FLOAT_EQ(restored.goboRotation().value, 5.0f);
    EXPECT_TRUE(restored.goboInvert());
    EXPECT_EQ(restored.lightLinkMode(), Artifact::LightLinkMode::IncludeOnly);
    EXPECT_EQ(restored.linkedLayerIdsText(),
              QStringLiteral("layer-a,layer-b"));
}

TEST(LightLayerTest, PropertyEditsUpdateLayerAndSerializedState)
{
    Artifact::ArtifactLightLayer light;

    ASSERT_TRUE(light.setLayerPropertyValue(
        QStringLiteral("Light/Type"),
        QVariant(static_cast<int>(Artifact::LightType::Area))));
    ASSERT_TRUE(light.setLayerPropertyValue(
        QStringLiteral("Light/Range"), QVariant(275.0)));
    ASSERT_TRUE(light.setLayerPropertyValue(
        QStringLiteral("Light/Area Width"), QVariant(180.0)));

    EXPECT_EQ(light.lightType(), Artifact::LightType::Area);
    EXPECT_FLOAT_EQ(light.range().value, 275.0f);

    const QJsonObject state = light.toJson();
    EXPECT_EQ(state.value(QStringLiteral("light.type")).toInt(),
              static_cast<int>(Artifact::LightType::Area));
    EXPECT_DOUBLE_EQ(state.value(QStringLiteral("light.range")).toDouble(),
                     275.0);
    EXPECT_DOUBLE_EQ(
        state.value(QStringLiteral("light.areaWidth")).toDouble(), 180.0);

    Artifact::ArtifactLightLayer restored;
    restored.fromJsonProperties(state);
    EXPECT_EQ(restored.lightType(), Artifact::LightType::Area);
    EXPECT_FLOAT_EQ(restored.areaWidth().value, 180.0f);
}
