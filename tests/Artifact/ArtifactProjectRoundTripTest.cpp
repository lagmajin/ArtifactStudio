#include <gtest/gtest.h>

#include <memory>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIODevice>
#include <QJsonObject>
#include <QPointF>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QVariant>

import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layer.Matte;
import Artifact.Layer.Result;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Project.Exporter;
import Artifact.Project.Health;
import Artifact.Project.Importer;
import Artifact.Project.Manager;
import Artifact.Effect.Abstract;
import Artifact.Effect.Keying.IBKKeyer;
import BrightnessEffect;
import Color.Float;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import InvertEffect;
import Memory.SharedPtr;
import Undo.UndoManager;
import Utils.Id;
import Utils.String.UniString;

namespace {

class ProjectLoadHistoryProbeCommand final : public Artifact::UndoCommand {
public:
    ProjectLoadHistoryProbeCommand(int& value, int before, int after)
        : value_(value), before_(before), after_(after) {}

    void undo() override { value_ = before_; }
    void redo() override { value_ = after_; }
    size_t estimatedMemoryBytes() const override { return sizeof(*this); }

private:
    int& value_;
    int before_;
    int after_;
};

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

TEST(ArtifactProjectRoundTripTest, CompositionIBKPropertiesSurviveProjectReload)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    const QString cleanPlatePath =
        qEnvironmentVariable("ARTIFACT_KEYING_REAL_FIXTURE");
    ASSERT_FALSE(cleanPlatePath.isEmpty())
        << "CMake must provide the bundled keying fixture path";
    ASSERT_TRUE(QFileInfo::exists(cleanPlatePath))
        << "The bundled keying fixture path must exist";

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("IBKCompositionRoundTrip"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto created = projects.createComposition(
        makeCompParams(QStringLiteral("IBKComposition"), 96, 72));
    ASSERT_TRUE(created.success);
    auto composition = projects.findComposition(created.id).ptr.lock();
    ASSERT_TRUE(composition);

    auto effect = ArtifactCore::makeShared<Artifact::IBKKeyerEffect>();
    effect->setPropertyValue(ArtifactCore::UniString("screenCorrection"),
                             QVariant(2.25));
    effect->setPropertyValue(ArtifactCore::UniString("previewMode"), QVariant(1));
    effect->setPropertyValue(ArtifactCore::UniString("erodePixels"), QVariant(0));
    effect->setPropertyValue(ArtifactCore::UniString("dilatePixels"), QVariant(0));
    effect->setPropertyValue(ArtifactCore::UniString("cleanPlatePath"),
                             QVariant(cleanPlatePath));
    composition->addEffect(effect);
    ASSERT_EQ(composition->effectCount(), 1);

    const QString projectPath = dir.filePath(QStringLiteral("ibk-roundtrip.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    auto restoredComposition = projects.findComposition(created.id).ptr.lock();
    ASSERT_TRUE(restoredComposition);
    const auto restoredEffects = restoredComposition->getEffects();
    ASSERT_EQ(restoredEffects.size(), 1u);
    auto* restored =
        dynamic_cast<Artifact::IBKKeyerEffect*>(restoredEffects.front().get());
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->effectID().toQString(),
              QStringLiteral("Effect.Keying.IBKKeyer"));

    bool foundScreenCorrection = false;
    bool foundPreviewMode = false;
    bool foundErodePixels = false;
    bool foundDilatePixels = false;
    bool foundCleanPlatePath = false;
    for (const auto& property : restored->getProperties()) {
        if (property.getName() == QStringLiteral("screenCorrection")) {
            foundScreenCorrection = true;
            EXPECT_DOUBLE_EQ(property.getValue().toDouble(), 2.25);
        } else if (property.getName() == QStringLiteral("previewMode")) {
            foundPreviewMode = true;
            EXPECT_EQ(property.getValue().toInt(), 1);
        } else if (property.getName() == QStringLiteral("erodePixels")) {
            foundErodePixels = true;
            EXPECT_EQ(property.getValue().toInt(), 0);
        } else if (property.getName() == QStringLiteral("dilatePixels")) {
            foundDilatePixels = true;
            EXPECT_EQ(property.getValue().toInt(), 0);
        } else if (property.getName() == QStringLiteral("cleanPlatePath")) {
            foundCleanPlatePath = true;
            EXPECT_EQ(property.getValue().toString(), cleanPlatePath);
        }
    }
    EXPECT_TRUE(foundScreenCorrection);
    EXPECT_TRUE(foundPreviewMode);
    EXPECT_TRUE(foundErodePixels);
    EXPECT_TRUE(foundDilatePixels);
    EXPECT_TRUE(foundCleanPlatePath);

    // Normalize the correction after verifying its serialized value so the
    // identical source and clean plate produce a zero matte in this check.
    restored->setPropertyValue(ArtifactCore::UniString("screenCorrection"),
                               QVariant(1.0));
    ArtifactCore::ImageF32x4_RGBA sourceImage;
    ASSERT_TRUE(sourceImage.load(cleanPlatePath));
    ArtifactCore::ImageF32x4RGBAWithCache keyedOutput;
    restored->applyCPUOnly(ArtifactCore::ImageF32x4RGBAWithCache(sourceImage),
                           keyedOutput);
    const auto restoredMatte = keyedOutput.image().getPixel(270, 480);
    EXPECT_LT(restoredMatte.r(), 0.01f)
        << "the restored clean plate must be loaded and used by the keyer";
    EXPECT_FLOAT_EQ(restoredMatte.a(), 1.0f)
        << "the serialized Matte preview should remain active after reload";
}

TEST(ArtifactProjectRoundTripTest, LayerMaskStateAndPathRoundTrip)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("MaskRoundTripProbe"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto composition = projects.createComposition(
        makeCompParams(QStringLiteral("MaskComp"), 96, 72));
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactLayerInitParams layerParams(
        QStringLiteral("MaskedSolid"), Artifact::LayerType::Solid);
    const auto created = projects.addLayerToComposition(composition.id, layerParams);
    ASSERT_TRUE(created.success);
    ASSERT_NE(created.layer.get(), nullptr);

    Artifact::LayerMask mask;
    mask.setEnabled(false);
    mask.setLocked(true);
    mask.setColor(ArtifactCore::FloatColor(0.17f, 0.38f, 0.64f, 0.82f));

    Artifact::MaskPath path;
    path.addVertex({QPointF(12.0, 14.0), QPointF(-1.0, 0.5), QPointF(2.0, 0.0)});
    path.addVertex({QPointF(68.0, 14.0), {}, {}});
    path.addVertex({QPointF(68.0, 55.0), {}, {}});
    path.addVertex({QPointF(12.0, 55.0), {}, {}});
    path.setClosed(true);
    path.setName(ArtifactCore::UniString(QStringLiteral("persisted-path")));
    path.setMode(Artifact::MaskMode::Difference);
    path.setOpacity(0.63f);
    path.setFeather({2.25f});
    path.setFeatherHorizontal({1.5f});
    path.setFeatherVertical({0.75f});
    path.setFeatherInner({0.5f});
    path.setFeatherOuter({1.25f});
    path.setFalloff(Artifact::MaskFeatherFalloff::Smooth);
    path.setExpansion({-1.75f});
    path.setInverted(true);
    Artifact::MaskPathKeyframeSnapshot animated;
    animated.frame = 24;
    animated.vertices = {
        {QPointF(14.0, 16.0), {}, {}},
        {QPointF(70.0, 16.0), {}, {}},
        {QPointF(70.0, 57.0), {}, {}},
        {QPointF(14.0, 57.0), {}, {}},
    };
    animated.closed = true;
    animated.opacity = 0.42f;
    animated.mode = Artifact::MaskMode::Subtract;
    animated.name = ArtifactCore::UniString(QStringLiteral("animated-state"));
    path.setAnimationKeyframe(animated.frame, animated);
    mask.addMaskPath(path);
    created.layer->addMask(mask);

    const QString savePath = dir.filePath(QStringLiteral("mask-roundtrip.artifact"));
    ASSERT_TRUE(projects.saveToFile(savePath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(savePath));

    const auto found = projects.findComposition(composition.id);
    ASSERT_TRUE(found.success);
    const auto reloadedComposition = found.ptr.lock();
    ASSERT_TRUE(reloadedComposition);
    const auto reloadedLayer = reloadedComposition->frontMostLayer();
    ASSERT_NE(reloadedLayer.get(), nullptr);
    ASSERT_EQ(reloadedLayer->maskCount(), 1);

    const Artifact::LayerMask restored = reloadedLayer->mask(0);
    EXPECT_FALSE(restored.isEnabled());
    EXPECT_TRUE(restored.isLocked());
    EXPECT_FLOAT_EQ(restored.color().r(), 0.17f);
    EXPECT_FLOAT_EQ(restored.color().g(), 0.38f);
    EXPECT_FLOAT_EQ(restored.color().b(), 0.64f);
    EXPECT_FLOAT_EQ(restored.color().a(), 0.82f);
    ASSERT_EQ(restored.maskPathCount(), 1);

    const Artifact::MaskPath restoredPath = restored.maskPath(0);
    ASSERT_EQ(restoredPath.vertexCount(), 4);
    EXPECT_TRUE(restoredPath.isClosed());
    EXPECT_EQ(restoredPath.name().toQString(), QStringLiteral("persisted-path"));
    EXPECT_EQ(restoredPath.mode(), Artifact::MaskMode::Difference);
    EXPECT_FLOAT_EQ(restoredPath.opacity(), 0.63f);
    EXPECT_FLOAT_EQ(restoredPath.feather().value, 2.25f);
    EXPECT_FLOAT_EQ(restoredPath.featherHorizontal().value, 1.5f);
    EXPECT_FLOAT_EQ(restoredPath.featherVertical().value, 0.75f);
    EXPECT_FLOAT_EQ(restoredPath.featherInner().value, 0.5f);
    EXPECT_FLOAT_EQ(restoredPath.featherOuter().value, 1.25f);
    EXPECT_EQ(restoredPath.falloff(), Artifact::MaskFeatherFalloff::Smooth);
    EXPECT_FLOAT_EQ(restoredPath.expansion().value, -1.75f);
    EXPECT_TRUE(restoredPath.isInverted());
    const auto firstVertex = restoredPath.vertex(0);
    EXPECT_DOUBLE_EQ(firstVertex.position.x(), 12.0);
    EXPECT_DOUBLE_EQ(firstVertex.position.y(), 14.0);
    EXPECT_DOUBLE_EQ(firstVertex.inTangent.x(), -1.0);
    EXPECT_DOUBLE_EQ(firstVertex.inTangent.y(), 0.5);
    EXPECT_DOUBLE_EQ(firstVertex.outTangent.x(), 2.0);
    EXPECT_DOUBLE_EQ(firstVertex.outTangent.y(), 0.0);
    ASSERT_TRUE(restoredPath.hasAnimationKeyframes());
    const auto keyframes = restoredPath.animationKeyframes();
    ASSERT_EQ(keyframes.size(), 1u);
    EXPECT_EQ(keyframes.front().frame, 24);
    EXPECT_EQ(keyframes.front().mode, Artifact::MaskMode::Subtract);
    EXPECT_FLOAT_EQ(keyframes.front().opacity, 0.42f);
    EXPECT_EQ(keyframes.front().name.toQString(), QStringLiteral("animated-state"));
    ASSERT_EQ(keyframes.front().vertices.size(), 4u);
    EXPECT_DOUBLE_EQ(keyframes.front().vertices.front().position.x(), 14.0);
    EXPECT_DOUBLE_EQ(keyframes.front().vertices.front().position.y(), 16.0);
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

    // A failed replacement load must preserve an already active project.
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("PreservedAfterFailedLoad"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    const auto baselineProject = projects.getCurrentProjectSharedPtr();
    ASSERT_TRUE(baselineProject);
    const auto baselineComposition = projects.createComposition(
        makeCompParams(QStringLiteral("SurvivorComposition"), 320, 180));
    ASSERT_TRUE(baselineComposition.success);
    Artifact::ArtifactLayerInitParams survivorParams(
        QStringLiteral("SurvivorLayer"), Artifact::LayerType::Solid);
    const auto survivor = projects.addLayerToComposition(
        baselineComposition.id, survivorParams);
    ASSERT_TRUE(survivor.success);
    ASSERT_NE(survivor.layer.get(), nullptr);

    auto* undo = Artifact::UndoManager::instance();
    ASSERT_NE(undo, nullptr);
    undo->clearHistory();
    int historyProbe = 0;
    struct UndoHistoryCleanup final {
        Artifact::UndoManager* manager;
        ~UndoHistoryCleanup() { manager->clearHistory(); }
    };
    [[maybe_unused]] UndoHistoryCleanup undoHistoryCleanup{undo};
    ASSERT_TRUE(undo->push(std::make_unique<ProjectLoadHistoryProbeCommand>(
        historyProbe, 0, 1)));
    ASSERT_TRUE(undo->push(std::make_unique<ProjectLoadHistoryProbeCommand>(
        historyProbe, 1, 2)));
    undo->undo();
    ASSERT_EQ(historyProbe, 1);
    ASSERT_EQ(undo->undoCount(), 1u);
    ASSERT_EQ(undo->redoCount(), 1u);

    EXPECT_FALSE(projects.loadFromFile(garbagePath));
    const auto afterFailedLoad = projects.getCurrentProjectSharedPtr();
    ASSERT_TRUE(afterFailedLoad);
    EXPECT_EQ(afterFailedLoad.get(), baselineProject.get());
    EXPECT_EQ(afterFailedLoad->settings().projectName(),
              QStringLiteral("PreservedAfterFailedLoad"));
    EXPECT_TRUE(projects.findComposition(baselineComposition.id).success);
    const auto preservedComposition =
        projects.findComposition(baselineComposition.id).ptr.lock();
    ASSERT_TRUE(preservedComposition);
    EXPECT_EQ(preservedComposition->layerCount(), 1);
    EXPECT_TRUE(preservedComposition->containsLayerById(survivor.layer->id()));

    EXPECT_EQ(historyProbe, 1);
    EXPECT_EQ(undo->undoCount(), 1u);
    EXPECT_EQ(undo->redoCount(), 1u);
    undo->undo();
    EXPECT_EQ(historyProbe, 0);
    undo->redo();
    EXPECT_EQ(historyProbe, 1);
    undo->redo();
    EXPECT_EQ(historyProbe, 2);
}

TEST(ArtifactProjectRoundTripTest, DuplicateLayerCreatesIndependentIdAndCopiesProperties)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("DuplicateLayerProbe"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto composition = projects.createComposition(
        makeCompParams(QStringLiteral("Composition"), 320, 180));
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactLayerInitParams params(QStringLiteral("Source"),
                                             Artifact::LayerType::Solid);
    const auto added = projects.addLayerToComposition(composition.id, params);
    ASSERT_TRUE(added.success);
    ASSERT_NE(added.layer.get(), nullptr);
    added.layer->setOpacity(0.37f);

    const auto duplicated =
        projects.duplicateLayerInComposition(composition.id, added.layer->id());
    ASSERT_TRUE(duplicated.success);
    ASSERT_NE(duplicated.layer.get(), nullptr);
    EXPECT_NE(duplicated.layer->id(), added.layer->id());
    EXPECT_EQ(duplicated.layer->layerName(), QStringLiteral("Source Copy"));
    EXPECT_FLOAT_EQ(added.layer->opacity(), 0.37f);
    EXPECT_FLOAT_EQ(duplicated.layer->opacity(), 0.37f);
    duplicated.layer->setOpacity(0.82f);
    EXPECT_FLOAT_EQ(duplicated.layer->opacity(), 0.82f);
    EXPECT_FLOAT_EQ(added.layer->opacity(), 0.37f);

    const auto comp = projects.findComposition(composition.id).ptr.lock();
    ASSERT_TRUE(comp);
    EXPECT_EQ(comp->layerCount(), 2);
    EXPECT_TRUE(comp->containsLayerById(added.layer->id()));
    EXPECT_TRUE(comp->containsLayerById(duplicated.layer->id()));

    const auto missingId = ArtifactCore::LayerID::Nil();
    const auto rejected = projects.duplicateLayerInComposition(composition.id, missingId);
    EXPECT_FALSE(rejected.success);
    EXPECT_EQ(comp->layerCount(), 2);
}

TEST(ArtifactProjectRoundTripTest, RejectedLayerOperationsLeaveProjectTreeUnchanged)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("RejectedLayerOperationsProbe"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto composition = projects.createComposition(
        makeCompParams(QStringLiteral("Composition"), 320, 180));
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactLayerInitParams params(QStringLiteral("Survivor"),
                                             Artifact::LayerType::Solid);
    const auto added = projects.addLayerToComposition(composition.id, params);
    ASSERT_TRUE(added.success);
    ASSERT_NE(added.layer.get(), nullptr);
    auto comp = projects.findComposition(composition.id).ptr.lock();
    ASSERT_TRUE(comp);

    const auto beforeItems = projects.projectItems().size();
    const auto beforeLayerCount = comp->layerCount();
    const auto missingLayer = ArtifactCore::LayerID::Nil();
    const auto missingComposition = ArtifactCore::CompositionID::Nil();

    const auto invalidAdd = projects.addLayerToComposition(missingComposition, params);
    EXPECT_FALSE(invalidAdd.success);
    EXPECT_FALSE(projects.removeLayerFromComposition(composition.id, missingLayer));
    EXPECT_FALSE(projects.removeLayerFromComposition(missingComposition, missingLayer));
    EXPECT_FALSE(projects.duplicateLayerInComposition(composition.id, missingLayer).success);

    EXPECT_EQ(comp->layerCount(), beforeLayerCount);
    EXPECT_TRUE(comp->containsLayerById(added.layer->id()));
    EXPECT_EQ(projects.projectItems().size(), beforeItems);
    EXPECT_TRUE(projects.findComposition(composition.id).success);
}

TEST(ArtifactProjectRoundTripTest,
     RemovingMatteSourceClearsOnlyDanglingReferenceAndPreservesSurvivorRoundTrip)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("MatteReferenceProbe"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto composition = projects.createComposition(
        makeCompParams(QStringLiteral("Composition"), 320, 180));
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactLayerInitParams sourceParams(QStringLiteral("MatteSource"),
                                                   Artifact::LayerType::Solid);
    Artifact::ArtifactLayerInitParams survivingSourceParams(
        QStringLiteral("SurvivingMatteSource"), Artifact::LayerType::Solid);
    Artifact::ArtifactLayerInitParams targetParams(QStringLiteral("Target"),
                                                   Artifact::LayerType::Solid);
    const auto source = projects.addLayerToComposition(composition.id, sourceParams);
    const auto survivingSource = projects.addLayerToComposition(
        composition.id, survivingSourceParams);
    const auto target = projects.addLayerToComposition(composition.id, targetParams);
    ASSERT_TRUE(source.success);
    ASSERT_TRUE(survivingSource.success);
    ASSERT_TRUE(target.success);
    ASSERT_NE(source.layer.get(), nullptr);
    ASSERT_NE(survivingSource.layer.get(), nullptr);
    ASSERT_NE(target.layer.get(), nullptr);

    Artifact::LayerMatteReference matte;
    matte.sourceLayerId = source.layer->id();
    target.layer->addMatteReference(matte);
    Artifact::LayerMatteReference survivingMatte;
    survivingMatte.sourceLayerId = survivingSource.layer->id();
    survivingMatte.opacity = 0.35f;
    survivingMatte.invert = true;
    target.layer->addMatteReference(survivingMatte);
    ASSERT_EQ(target.layer->matteReferenceCount(), 2);
    ASSERT_TRUE(target.layer->hasEnabledExternalMatteReference());

    ASSERT_TRUE(projects.removeLayerFromComposition(composition.id,
                                                    source.layer->id()));
    const auto comp = projects.findComposition(composition.id).ptr.lock();
    ASSERT_TRUE(comp);
    EXPECT_EQ(comp->layerCount(), 2);
    EXPECT_TRUE(comp->containsLayerById(target.layer->id()));
    EXPECT_TRUE(comp->containsLayerById(survivingSource.layer->id()));
    ASSERT_EQ(target.layer->matteReferenceCount(), 1);
    const auto referencesAfterRemoval = target.layer->matteReferences();
    ASSERT_EQ(referencesAfterRemoval.size(), 1u);
    EXPECT_EQ(referencesAfterRemoval.front().sourceLayerId,
              survivingSource.layer->id());
    EXPECT_EQ(referencesAfterRemoval.front().id, survivingMatte.id);
    EXPECT_FLOAT_EQ(referencesAfterRemoval.front().opacity, 0.35f);
    EXPECT_TRUE(referencesAfterRemoval.front().invert);
    EXPECT_TRUE(target.layer->hasEnabledExternalMatteReference());
    EXPECT_FALSE(projects.removeLayerFromComposition(composition.id,
                                                     source.layer->id()));
    EXPECT_EQ(comp->layerCount(), 2);

    const QString projectPath = dir.filePath(QStringLiteral("surviving-matte.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    const auto reloadedComposition =
        projects.findComposition(composition.id).ptr.lock();
    ASSERT_TRUE(reloadedComposition);
    ASSERT_EQ(reloadedComposition->layerCount(), 2);
    Artifact::ArtifactAbstractLayerPtr reloadedTarget;
    for (const auto& layer : reloadedComposition->allLayer()) {
        if (layer && layer->id() == target.layer->id()) {
            reloadedTarget = layer;
            break;
        }
    }
    ASSERT_NE(reloadedTarget.get(), nullptr);
    ASSERT_EQ(reloadedTarget->matteReferenceCount(), 1);
    const auto reloadedReferences = reloadedTarget->matteReferences();
    ASSERT_EQ(reloadedReferences.size(), 1u);
    EXPECT_EQ(reloadedReferences.front().sourceLayerId,
              survivingSource.layer->id());
    EXPECT_EQ(reloadedReferences.front().id, survivingMatte.id);
    EXPECT_FLOAT_EQ(reloadedReferences.front().opacity, 0.35f);
    EXPECT_TRUE(reloadedReferences.front().invert);
}

TEST(ArtifactProjectRoundTripTest, ProjectSavePersistsUndoAndRedoResults)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("UndoSaveProbe"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    const auto composition = projects.createComposition(
        makeCompParams(QStringLiteral("Composition"), 320, 180));
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactLayerInitParams params(QStringLiteral("Layer"),
                                             Artifact::LayerType::Solid);
    const auto added = projects.addLayerToComposition(composition.id, params);
    ASSERT_TRUE(added.success);
    ASSERT_NE(added.layer.get(), nullptr);
    EXPECT_FLOAT_EQ(added.layer->opacity(), 1.0f);

    Artifact::UndoManager manager;
    ASSERT_TRUE(manager.push(std::make_unique<Artifact::ChangeLayerOpacityCommand>(
        added.layer, 1.0f, 0.35f)));
    EXPECT_FLOAT_EQ(added.layer->opacity(), 0.35f);
    manager.undo();
    EXPECT_FLOAT_EQ(added.layer->opacity(), 1.0f);

    const QString afterUndoPath = dir.filePath(QStringLiteral("after-undo.artifact"));
    ASSERT_TRUE(projects.saveToFile(afterUndoPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(afterUndoPath));
    auto loadedComposition = projects.findComposition(composition.id).ptr.lock();
    ASSERT_TRUE(loadedComposition);
    auto loadedLayer = loadedComposition->frontMostLayer();
    ASSERT_NE(loadedLayer.get(), nullptr);
    EXPECT_FLOAT_EQ(loadedLayer->opacity(), 1.0f);

    Artifact::UndoManager redoManager;
    ASSERT_TRUE(redoManager.push(
        std::make_unique<Artifact::ChangeLayerOpacityCommand>(
            loadedLayer, 1.0f, 0.35f)));
    redoManager.undo();
    EXPECT_FLOAT_EQ(loadedLayer->opacity(), 1.0f);
    redoManager.redo();
    EXPECT_FLOAT_EQ(loadedLayer->opacity(), 0.35f);

    const QString afterRedoPath = dir.filePath(QStringLiteral("after-redo.artifact"));
    ASSERT_TRUE(projects.saveToFile(afterRedoPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(afterRedoPath));
    loadedComposition = projects.findComposition(composition.id).ptr.lock();
    ASSERT_TRUE(loadedComposition);
    loadedLayer = loadedComposition->frontMostLayer();
    ASSERT_NE(loadedLayer.get(), nullptr);
    EXPECT_FLOAT_EQ(loadedLayer->opacity(), 0.35f);
}

TEST(ArtifactProjectRoundTripTest, EditedEffectStackOrderAndSettingsSurviveReload)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("EffectStackRoundTrip"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    const auto composition = projects.createComposition(
        makeCompParams(QStringLiteral("Composition"), 320, 180));
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactLayerInitParams params(QStringLiteral("Layer"),
                                             Artifact::LayerType::Solid);
    const auto added = projects.addLayerToComposition(composition.id, params);
    ASSERT_TRUE(added.success);
    ASSERT_NE(added.layer.get(), nullptr);

    auto removedEffect = ArtifactCore::makeShared<Artifact::BrightnessEffect>();
    auto retainedEffect = ArtifactCore::makeShared<Artifact::InvertEffect>();
    auto insertedEffect = ArtifactCore::makeShared<Artifact::BrightnessEffect>();
    removedEffect->setBrightness(0.1f);
    retainedEffect->setStrength(0.4f);
    insertedEffect->setBrightness(0.35f);
    added.layer->addEffect(removedEffect);
    added.layer->addEffect(retainedEffect);
    ASSERT_EQ(added.layer->effectCount(), 2);

    added.layer->removeEffect(removedEffect->effectID());
    added.layer->addEffect(insertedEffect);
    ASSERT_EQ(added.layer->effectCount(), 2);
    auto editedEffects = added.layer->getEffects();
    ASSERT_EQ(editedEffects.size(), 2u);
    EXPECT_EQ(editedEffects[0].get(), retainedEffect.get());
    EXPECT_EQ(editedEffects[1].get(), insertedEffect.get());

    const QString projectPath = dir.filePath(QStringLiteral("effect-stack.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    auto loadedComposition = projects.findComposition(composition.id).ptr.lock();
    ASSERT_TRUE(loadedComposition);
    auto loadedLayer = loadedComposition->frontMostLayer();
    ASSERT_NE(loadedLayer.get(), nullptr);
    const auto loadedEffects = loadedLayer->getEffects();
    ASSERT_EQ(loadedEffects.size(), 2u);
    auto* loadedInvert = dynamic_cast<Artifact::InvertEffect*>(loadedEffects[0].get());
    auto* loadedBrightness =
        dynamic_cast<Artifact::BrightnessEffect*>(loadedEffects[1].get());
    ASSERT_NE(loadedInvert, nullptr);
    ASSERT_NE(loadedBrightness, nullptr);
    EXPECT_FLOAT_EQ(loadedInvert->strength(), 0.4f);
    EXPECT_FLOAT_EQ(loadedBrightness->brightness(), 0.35f);
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

    auto loadedComposition = projects.findComposition(comp.id).ptr.lock();
    ASSERT_TRUE(loadedComposition);
    loadedComposition->setCompositionNote(QStringLiteral("edited-after-load"));
    loadedComposition->setCompositionName(
        ArtifactCore::UniString(QStringLiteral("EditedAfterLoad")));

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
              QStringLiteral("EditedAfterLoad"));
    EXPECT_EQ(reloaded->compositionNote(), QStringLiteral("edited-after-load"));
}
