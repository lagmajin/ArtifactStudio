#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>
#include <QVariant>

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Artifact.Layers.SolidImage;
import Color.Float;
import Utils.Id;
import Utils.String.UniString;

namespace {

class ArtifactScriptLayerTestEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "ArtifactScriptLayerIntegrationTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QCoreApplication(argc, argv);
        QCoreApplication::setApplicationName(
            QStringLiteral("ArtifactScriptLayerIntegrationTest"));
        QStandardPaths::setTestModeEnabled(true);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QCoreApplication* application_ = nullptr;
};

ArtifactScriptLayerTestEnvironment* const environment =
    new ArtifactScriptLayerTestEnvironment();
const bool environmentRegistered = [] {
    testing::AddGlobalTestEnvironment(environment);
    return true;
}();

Artifact::ArtifactCompositionInitParams makeCompositionParams()
{
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("ScriptLayerFixture")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    params.setResolution(32, 32);
    params.setFrameRate(24.0);
    params.setDurationFrames(24);
    return params;
}

Artifact::ArtifactAbstractLayerPtr makeSolidLayer(const QString& name)
{
    Artifact::ArtifactLayerFactory factory;
    Artifact::ArtifactSolidLayerInitParams params(name);
    params.setWidth(32);
    params.setHeight(32);
    const auto result = factory.createLayer(params);
    if (!result.success) return {};
    return result.layer;
}

QVariant layerPropertyValue(Artifact::ArtifactAbstractLayer* layer,
                            const QString& propertyPath)
{
    if (!layer) return {};
    (void)layer->getLayerPropertyGroups();
    const auto property = layer->getProperty(propertyPath);
    return property ? property->getValue() : QVariant{};
}

} // namespace

TEST(ArtifactScriptLayerIntegrationTest,
     BoundScriptReadsAndWritesAnotherCompositionLayer)
{
    const QString fixturePath = qEnvironmentVariable(
        "ARTIFACT_SCRIPT_LAYER_TEST_FILE").trimmed();
    ASSERT_FALSE(fixturePath.isEmpty());
    const QString scriptPath = QFileInfo(fixturePath).absoluteFilePath();
    ASSERT_TRUE(QFileInfo::exists(scriptPath)) << scriptPath.toStdString();

    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(QStringLiteral("script-layer-integration")),
        makeCompositionParams());

    const auto driver = makeSolidLayer(QStringLiteral("Driver"));
    const auto target = makeSolidLayer(QStringLiteral("Target"));
    ASSERT_NE(driver.get(), nullptr);
    ASSERT_NE(target.get(), nullptr);
    ASSERT_TRUE(composition.appendLayerTop(driver).success);
    ASSERT_TRUE(composition.appendLayerTop(target).success);

    ASSERT_DOUBLE_EQ(layerPropertyValue(target.get(),
                                        QStringLiteral("layer.opacity")).toDouble(),
                     1.0);

    ASSERT_TRUE(driver->setLayerPropertyValue(
        QStringLiteral("component.script.enabled"), true));
    driver->setScriptBinding(QJsonObject{
        {QStringLiteral("path"), scriptPath}});
    ASSERT_TRUE(driver->hasScriptBinding());
    ASSERT_TRUE(driver->advanceScriptLifecycle(
        Artifact::ArtifactAbstractLayer::ScriptRunState::Enabled))
        << driver->scriptLastError();
    EXPECT_TRUE(driver->scriptLastError().empty());

    EXPECT_DOUBLE_EQ(layerPropertyValue(target.get(),
                                        QStringLiteral("layer.opacity")).toDouble(),
                     0.375);
}
