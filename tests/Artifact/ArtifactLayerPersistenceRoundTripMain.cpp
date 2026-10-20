#include <QGuiApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>

import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Project.Manager;
import Memory.SharedPtr;
import Utils.String.UniString;

namespace {

bool fail(const char* message)
{
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

struct ProjectCleanup {
    Artifact::ArtifactProjectManager& manager;
    ~ProjectCleanup() { manager.closeCurrentProject(); }
};

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return fail("could not create temporary directory") ? 0 : 1;

    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", tempDir.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactLayerRoundTrip"));
    QStandardPaths::setTestModeEnabled(true);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("LayerPersistenceProbe"), true);
    if (!projects.isProjectCreated()) return fail("project creation failed") ? 0 : 1;

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("RoundTripComposition")));
    compositionParams.setResolution(320, 180);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(48);
    const auto compositionResult = projects.createComposition(compositionParams);
    if (!compositionResult.success) return fail("composition creation failed") ? 0 : 1;

    Artifact::ArtifactLayerInitParams layerParams(
        QStringLiteral("RoundTripSolid"), Artifact::LayerType::Solid);
    const auto layerResult = projects.addLayerToComposition(
        compositionResult.id, layerParams);
    if (!layerResult.success || !layerResult.layer)
        return fail("solid layer creation failed") ? 0 : 1;

    constexpr float expectedOpacity = 0.375f;
    layerResult.layer->setOpacity(expectedOpacity);
    const QString projectPath = tempDir.filePath(QStringLiteral("layer-roundtrip.artifact"));
    const auto saveResult = projects.saveToFile(projectPath);
    if (!saveResult.success || !QFileInfo(projectPath).isFile())
        return fail("project save failed") ? 0 : 1;

    projects.closeCurrentProject();
    if (!projects.loadFromFile(projectPath)) return fail("project reload failed") ? 0 : 1;

    const auto reloadedCompositionResult = projects.findComposition(compositionResult.id);
    if (!reloadedCompositionResult.success)
        return fail("saved composition was not found after reload") ? 0 : 1;
    auto reloadedComposition = reloadedCompositionResult.ptr.lock();
    if (!reloadedComposition || reloadedComposition->layerCount() != 1)
        return fail("reloaded composition does not contain exactly one layer") ? 0 : 1;

    const auto reloadedLayer = reloadedComposition->frontMostLayer();
    if (!reloadedLayer) return fail("reloaded layer is null") ? 0 : 1;
    if (reloadedLayer->layerName() != QStringLiteral("RoundTripSolid"))
        return fail("reloaded layer name differs") ? 0 : 1;
    if (std::abs(reloadedLayer->opacity() - expectedOpacity) > 1.0e-6f)
        return fail("reloaded layer opacity differs") ? 0 : 1;

    std::cout << "PASS: solid layer name and opacity survived project save/reload\n";
    return 0;
}
