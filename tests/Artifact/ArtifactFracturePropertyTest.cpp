#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>
#include <QVariant>

import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layers.SolidImage;
import Artifact.Layer.InitParams;
import Undo.UndoManager;
import Color.Float;

namespace {

class FracturePropertyTestEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "ArtifactFracturePropertyTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QGuiApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QGuiApplication* application_ = nullptr;
};

FracturePropertyTestEnvironment* const fracturePropertyTestEnvironment =
    new FracturePropertyTestEnvironment();
const bool fracturePropertyTestEnvironmentRegistered = [] {
    testing::AddGlobalTestEnvironment(fracturePropertyTestEnvironment);
    return true;
}();

Artifact::ArtifactAbstractLayerPtr makeFractureSolid()
{
    Artifact::ArtifactLayerFactory layerFactory;
    Artifact::ArtifactSolidLayerInitParams solidParams(QStringLiteral("FractureProps"));
    solidParams.setWidth(320);
    solidParams.setHeight(180);
    solidParams.setColor(ArtifactCore::FloatColor(0.5f, 0.5f, 0.5f, 1.0f));
    const auto result = layerFactory.createLayer(solidParams);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.layer.get(), nullptr);
    return result.layer;
}

void setFractureProps(Artifact::ArtifactAbstractLayer* layer)
{
    EXPECT_TRUE(layer->setLayerPropertyValue(QStringLiteral("fracture.enabled"), QVariant(true)));
    EXPECT_TRUE(layer->setLayerPropertyValue(QStringLiteral("fracture.preGenerate"), QVariant(true)));
    EXPECT_TRUE(layer->setLayerPropertyValue(QStringLiteral("fracture.triggerFrame"), QVariant(7)));
    EXPECT_TRUE(layer->setLayerPropertyValue(QStringLiteral("fracture.shardCount"), QVariant(20)));
    EXPECT_TRUE(layer->setLayerPropertyValue(QStringLiteral("fracture.preset"), QVariant(2)));
    EXPECT_TRUE(layer->setLayerPropertyValue(QStringLiteral("fracture.shardGravity"), QVariant(250.0)));
    EXPECT_TRUE(layer->setLayerPropertyValue(QStringLiteral("fracture.shardDamping"), QVariant(0.9)));
    EXPECT_TRUE(layer->setLayerPropertyValue(QStringLiteral("fracture.impactSensitivity"), QVariant(1.5)));
}

QVariant fractureValue(Artifact::ArtifactAbstractLayer* layer, const char* path)
{
    // getProperty()はキャッシュ参照のみ。Inspector同様にグループ構築で実体化する。
    (void)layer->getLayerPropertyGroups();
    const auto property = layer->getProperty(QString::fromLatin1(path));
    EXPECT_NE(property, nullptr) << path;
    return property ? property->getValue() : QVariant{};
}

} // namespace

TEST(ArtifactFracturePropertyTest, SettingsSurviveJsonRoundTrip)
{
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactFracturePropertyTest"));
    QStandardPaths::setTestModeEnabled(true);

    const auto source = makeFractureSolid();
    ASSERT_NE(source.get(), nullptr);
    setFractureProps(source.get());

    const QJsonObject state = source->toJson();
    const QJsonObject fracture = state.value(QStringLiteral("fracture")).toObject();
    ASSERT_FALSE(fracture.isEmpty());
    EXPECT_TRUE(fracture.value(QStringLiteral("enabled")).toBool());
    EXPECT_TRUE(fracture.value(QStringLiteral("preGenerate")).toBool());
    EXPECT_EQ(fracture.value(QStringLiteral("triggerFrame")).toVariant().toLongLong(), 7);
    EXPECT_EQ(fracture.value(QStringLiteral("shardCount")).toInt(), 20);
    EXPECT_EQ(fracture.value(QStringLiteral("preset")).toInt(), 2);
    EXPECT_FLOAT_EQ(static_cast<float>(fracture.value(QStringLiteral("shardGravity")).toDouble()), 250.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(fracture.value(QStringLiteral("shardDamping")).toDouble()), 0.9f);
    EXPECT_FLOAT_EQ(static_cast<float>(fracture.value(QStringLiteral("impactSensitivity")).toDouble()), 1.5f);

    const auto restored = makeFractureSolid();
    ASSERT_NE(restored.get(), nullptr);
    restored->fromJsonProperties(state);

    EXPECT_EQ(fractureValue(restored.get(), "fracture.enabled").toBool(), true);
    EXPECT_EQ(fractureValue(restored.get(), "fracture.preGenerate").toBool(), true);
    EXPECT_EQ(fractureValue(restored.get(), "fracture.triggerFrame").toLongLong(), 7);
    EXPECT_EQ(fractureValue(restored.get(), "fracture.shardCount").toInt(), 20);
    EXPECT_EQ(fractureValue(restored.get(), "fracture.preset").toInt(), 2);
    EXPECT_FLOAT_EQ(fractureValue(restored.get(), "fracture.shardGravity").toFloat(), 250.0f);
    EXPECT_FLOAT_EQ(fractureValue(restored.get(), "fracture.shardDamping").toFloat(), 0.9f);
    EXPECT_FLOAT_EQ(fractureValue(restored.get(), "fracture.impactSensitivity").toFloat(), 1.5f);
}

TEST(ArtifactFracturePropertyTest, ShardCountChangeUndoesAndRedoes)
{
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactFracturePropertyTest"));
    QStandardPaths::setTestModeEnabled(true);

    const auto layer = makeFractureSolid();
    ASSERT_NE(layer.get(), nullptr);
    ASSERT_TRUE(layer->setLayerPropertyValue(
        QStringLiteral("fracture.shardCount"), QVariant(12)));
    ASSERT_EQ(fractureValue(layer.get(), "fracture.shardCount").toInt(), 12);

    Artifact::SetLayerPropertyValueCommand command(
        layer, QStringLiteral("fracture.shardCount"), QVariant(12), QVariant(24));
    command.redo();
    EXPECT_TRUE(command.lastOperationSucceeded());
    EXPECT_EQ(fractureValue(layer.get(), "fracture.shardCount").toInt(), 24);

    command.undo();
    EXPECT_TRUE(command.lastOperationSucceeded());
    EXPECT_EQ(fractureValue(layer.get(), "fracture.shardCount").toInt(), 12);
}

TEST(ArtifactFracturePropertyTest, EnableToggleUndoesWithoutTouchingShardCount)
{
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactFracturePropertyTest"));
    QStandardPaths::setTestModeEnabled(true);

    const auto layer = makeFractureSolid();
    ASSERT_NE(layer.get(), nullptr);
    ASSERT_TRUE(layer->setLayerPropertyValue(
        QStringLiteral("fracture.shardCount"), QVariant(12)));
    ASSERT_TRUE(layer->setLayerPropertyValue(
        QStringLiteral("fracture.enabled"), QVariant(false)));

    Artifact::SetLayerPropertyValueCommand command(
        layer, QStringLiteral("fracture.enabled"), QVariant(false), QVariant(true));
    command.redo();
    EXPECT_TRUE(command.lastOperationSucceeded());
    EXPECT_EQ(fractureValue(layer.get(), "fracture.enabled").toBool(), true);
    EXPECT_EQ(fractureValue(layer.get(), "fracture.shardCount").toInt(), 12);

    command.undo();
    EXPECT_TRUE(command.lastOperationSucceeded());
    EXPECT_EQ(fractureValue(layer.get(), "fracture.enabled").toBool(), false);
    EXPECT_EQ(fractureValue(layer.get(), "fracture.shardCount").toInt(), 12);
}
