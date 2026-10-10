#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIODevice>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>

import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Project.Exporter;
import Artifact.Project.Health;
import Artifact.Project.Importer;
import Artifact.Project.Manager;
import Color.Float;
import Utils.String.UniString;

namespace {

class ProjectRoundTripEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "ArtifactProjectRoundTripTest";
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

ProjectRoundTripEnvironment* const projectRoundTripEnvironment =
    new ProjectRoundTripEnvironment();
const bool projectRoundTripEnvironmentRegistered = [] {
    testing::AddGlobalTestEnvironment(projectRoundTripEnvironment);
    return true;
}();

// Closes the singleton project on scope exit so no state leaks between tests.
struct ProjectCleanup {
    Artifact::ArtifactProjectManager& projects;
    ~ProjectCleanup()
    {
        projects.closeCurrentProject();
    }
};

Artifact::ArtifactCompositionInitParams makeCompParams(const QString& name,
                                                       int width, int height)
{
    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(name));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(48);
    params.setBackgroundColor(
        ArtifactCore::FloatColor(0.012f, 0.018f, 0.032f, 1.0f));
    return params;
}

void isolateProjectRoot(const QTemporaryDir& dir)
{
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", dir.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioProjectTest"));
    QStandardPaths::setTestModeEnabled(true);
}

bool writeGarbageFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write("{ not json {{{") > 0;
}

} // namespace

TEST(ArtifactProjectRoundTripTest, CreateSaveLoadPreservesProject)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};

    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("RoundTripProbe"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    auto project = projects.getCurrentProjectSharedPtr();
    ASSERT_TRUE(project);
    EXPECT_EQ(project->settings().projectName(), QStringLiteral("RoundTripProbe"));
    project->setAINotes(QStringLiteral("notes-123"));
    QJsonObject extension;
    extension.insert(QStringLiteral("_test_flag"), true);
    extension.insert(QStringLiteral("_test_tag"), QStringLiteral("roundtrip"));
    project->setExtensionData(extension);

    const auto compA =
        projects.createComposition(makeCompParams(QStringLiteral("MainComp"), 320, 180));
    ASSERT_TRUE(compA.success);
    const auto compB =
        projects.createComposition(makeCompParams(QStringLiteral("SecondComp"), 640, 360));
    ASSERT_TRUE(compB.success);
    EXPECT_EQ(projects.compositionCount(), 2);

    auto compAPtr = projects.findComposition(compA.id).ptr.lock();
    ASSERT_TRUE(compAPtr);
    compAPtr->setCompositionNote(QStringLiteral("note-A"));

    Artifact::ArtifactLayerInitParams layerParams(QStringLiteral("SolidProbe"),
                                                 Artifact::LayerType::Solid);
    const auto layerResult = projects.addLayerToComposition(compA.id, layerParams);
    ASSERT_TRUE(layerResult.success);
    ASSERT_NE(layerResult.layer.get(), nullptr);
    EXPECT_EQ(layerResult.layer->layerName(), QStringLiteral("SolidProbe"));
    EXPECT_EQ(compAPtr->layerCount(), 1);

    const int itemCount = projects.projectItems().size();
    const qsizetype healthIssuesBefore =
        Artifact::ArtifactProjectHealthChecker::check(project.get()).issues.size();
    const bool treeValidBefore = project->validateProjectTree(nullptr);

    const QString savePath = dir.filePath(QStringLiteral("roundtrip.artifact"));
    const auto saveResult = projects.saveToFile(savePath);
    EXPECT_TRUE(saveResult.success);
    EXPECT_TRUE(saveResult.errorMessage.isEmpty());
    ASSERT_TRUE(QFileInfo::exists(savePath));
    EXPECT_EQ(projects.currentProjectPath(), QFileInfo(savePath).absoluteFilePath());

    projects.closeCurrentProject();
    EXPECT_FALSE(projects.isProjectCreated());

    ASSERT_TRUE(projects.loadFromFile(savePath));
    EXPECT_TRUE(projects.isProjectCreated());
    EXPECT_EQ(projects.compositionCount(), 2);

    const auto foundA = projects.findComposition(compA.id);
    EXPECT_TRUE(foundA.success);
    auto reloadedA = foundA.ptr.lock();
    ASSERT_TRUE(reloadedA);
    EXPECT_EQ(reloadedA->settings().compositionName().toQString(),
              QStringLiteral("MainComp"));
    EXPECT_EQ(reloadedA->layerCount(), 1);
    auto frontLayer = reloadedA->frontMostLayer();
    ASSERT_NE(frontLayer.get(), nullptr);
    EXPECT_EQ(frontLayer->layerName(), QStringLiteral("SolidProbe"));
    EXPECT_EQ(reloadedA->compositionNote(), QStringLiteral("note-A"));

    const auto foundB = projects.findComposition(compB.id);
    EXPECT_TRUE(foundB.success);
    auto reloadedB = foundB.ptr.lock();
    ASSERT_TRUE(reloadedB);
    EXPECT_EQ(reloadedB->settings().compositionName().toQString(),
              QStringLiteral("SecondComp"));
    EXPECT_EQ(reloadedB->layerCount(), 0);

    auto reloaded = projects.getCurrentProjectSharedPtr();
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->settings().projectName(), QStringLiteral("RoundTripProbe"));
    EXPECT_EQ(reloaded->aiNotes(), QStringLiteral("notes-123"));
    EXPECT_EQ(reloaded->extensionData().value(QStringLiteral("_test_tag")).toString(),
              QStringLiteral("roundtrip"));
    EXPECT_EQ(projects.projectItems().size(), itemCount);
    EXPECT_EQ(Artifact::ArtifactProjectHealthChecker::check(reloaded.get()).issues.size(),
              healthIssuesBefore);
    EXPECT_EQ(reloaded->validateProjectTree(nullptr), treeValidBefore);
}

TEST(ArtifactProjectRoundTripTest, SaveLoadGuardsRejectBadInput)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    ASSERT_FALSE(projects.isProjectCreated());

    // Saving without a project or without a path must fail, not crash.
    EXPECT_FALSE(projects.saveToFile(QString()).success);
    EXPECT_FALSE(
        projects.saveToFile(dir.filePath(QStringLiteral("no-project.artifact"))).success);
    EXPECT_FALSE(projects.isProjectCreated());

    // Loading a missing file must fail and leave the manager untouched.
    EXPECT_FALSE(
        projects.loadFromFile(dir.filePath(QStringLiteral("missing.artifact"))));
    EXPECT_FALSE(projects.isProjectCreated());

    // A corrupt document must fail validation and the load.
    const QString garbagePath = dir.filePath(QStringLiteral("garbage.artifact"));
    ASSERT_TRUE(writeGarbageFile(garbagePath));
    Artifact::ArtifactProjectImporter importer;
    importer.setInputPath(garbagePath);
    EXPECT_FALSE(importer.validateFile(garbagePath));
    EXPECT_FALSE(projects.loadFromFile(garbagePath));
    EXPECT_FALSE(projects.isProjectCreated());

    // Null projects are reported unhealthy instead of crashing the checker.
    EXPECT_FALSE(Artifact::ArtifactProjectHealthChecker::check(nullptr).isHealthy);
}

TEST(ArtifactProjectRoundTripTest, ResaveAfterLoadIsStable)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};

    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("ResaveProbe"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    const auto comp =
        projects.createComposition(makeCompParams(QStringLiteral("Only"), 320, 180));
    ASSERT_TRUE(comp.success);

    const QString firstPath = dir.filePath(QStringLiteral("first.artifact"));
    ASSERT_TRUE(projects.saveToFile(firstPath).success);

    // Saving over the same path exercises the backup branch.
    ASSERT_TRUE(projects.saveToFile(firstPath).success);

    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(firstPath));
    EXPECT_EQ(projects.compositionCount(), 1);

    const QString secondPath = dir.filePath(QStringLiteral("second.artifact"));
    ASSERT_TRUE(projects.saveToFile(secondPath).success);
    ASSERT_TRUE(QFileInfo::exists(firstPath));
    ASSERT_TRUE(QFileInfo::exists(secondPath));

    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(secondPath));
    EXPECT_EQ(projects.compositionCount(), 1);
    const auto found = projects.findComposition(comp.id);
    EXPECT_TRUE(found.success);
    auto reloaded = found.ptr.lock();
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->settings().compositionName().toQString(),
              QStringLiteral("Only"));
}
