#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QInputMethodEvent>
#include <QIODevice>
#include <QKeyEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>
#include <QTextEdit>
#include <QTemporaryDir>
#include <QVariant>
#include <QWidget>

import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Layer.Text;
import Artifact.Project.Exporter;
import Artifact.Project.Health;
import Artifact.Project.Importer;
import Artifact.Project.Manager;
import Artifact.Widgets.CompositionTextEditor;
import Color.Float;
import Time.Rational;
import Undo.UndoManager;
import Utils.String.UniString;

namespace {

class ProjectRoundTripEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "ArtifactProjectRoundTripTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QApplication* application_ = nullptr;
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

TEST(ArtifactProjectRoundTripTest,
     TextLayerUnicodeStyleSourceTextAndAnimatorsSurviveReload)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("TextLayerRoundTrip"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto composition = projects.createComposition(
        makeCompParams(QStringLiteral("TextComposition"), 640, 360));
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactTextLayerInitParams params(QStringLiteral("LocalizedText"));
    const auto layerResult =
        projects.addLayerToComposition(composition.id, params);
    ASSERT_TRUE(layerResult.success);
    auto* textLayer =
        dynamic_cast<Artifact::ArtifactTextLayer*>(layerResult.layer.get());
    ASSERT_NE(textLayer, nullptr);

    const QString unicodeText = QStringLiteral("日本語 e\u0301 👩‍👩‍👧‍👦");
    textLayer->setText(ArtifactCore::UniString(unicodeText));
    textLayer->setFontFamily(
        ArtifactCore::UniString(QStringLiteral("Noto Sans CJK JP")));
    textLayer->setFontSize(37.5f);
    textLayer->setTracking(1.25f);
    textLayer->setBold(true);
    textLayer->setSourceTextAtFrame(0, unicodeText);
    textLayer->setSourceTextAtFrame(12, QStringLiteral("再読込後のテキスト"));
    ASSERT_TRUE(textLayer->addAnimatorProperty(QStringLiteral("position")));
    ASSERT_GT(textLayer->animatorCount(), 0);
    (void)textLayer->getLayerPropertyGroups();
    const auto positionX =
        textLayer->getProperty(QStringLiteral("text.animators.0.positionX"));
    ASSERT_TRUE(positionX);
    positionX->addKeyFrame(ArtifactCore::RationalTime(0, 30), QVariant(0.0));
    positionX->addKeyFrame(ArtifactCore::RationalTime(12, 30), QVariant(24.0));
    const auto originalLayerId = textLayer->id();
    const int animatorCount = textLayer->animatorCount();
    const QJsonArray animatorSnapshot = textLayer->textAnimatorStackSnapshot();

    const auto duplicateResult =
        projects.duplicateLayerInComposition(composition.id, originalLayerId);
    ASSERT_TRUE(duplicateResult.success);
    ASSERT_NE(duplicateResult.layer.get(), nullptr);
    const auto duplicateLayerId = duplicateResult.layer->id();
    auto* duplicatedText = dynamic_cast<Artifact::ArtifactTextLayer*>(
        duplicateResult.layer.get());
    ASSERT_NE(duplicatedText, nullptr);
    EXPECT_EQ(duplicatedText->text().toQString(), unicodeText);
    EXPECT_EQ(duplicatedText->fontFamily().toQString(),
              QStringLiteral("Noto Sans CJK JP"));
    EXPECT_FLOAT_EQ(duplicatedText->fontSize(), 37.5f);
    EXPECT_FLOAT_EQ(duplicatedText->tracking(), 1.25f);
    EXPECT_TRUE(duplicatedText->isBold());

    const QString savePath = dir.filePath(QStringLiteral("text-layer.artifact"));
    ASSERT_TRUE(projects.saveToFile(savePath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(savePath));

    const auto loadedComposition = projects.findComposition(composition.id);
    ASSERT_TRUE(loadedComposition.success);
    auto loadedCompositionPtr = loadedComposition.ptr.lock();
    ASSERT_TRUE(loadedCompositionPtr);
    ASSERT_EQ(loadedCompositionPtr->layerCount(), 2);

    auto loadedLayer = loadedCompositionPtr->layerById(originalLayerId);
    ASSERT_NE(loadedLayer.get(), nullptr);
    auto* reloadedText =
        dynamic_cast<Artifact::ArtifactTextLayer*>(loadedLayer.get());
    ASSERT_NE(reloadedText, nullptr);

    EXPECT_EQ(reloadedText->text().toQString(), unicodeText);
    EXPECT_EQ(reloadedText->fontFamily().toQString(),
              QStringLiteral("Noto Sans CJK JP"));
    EXPECT_FLOAT_EQ(reloadedText->fontSize(), 37.5f);
    EXPECT_FLOAT_EQ(reloadedText->tracking(), 1.25f);
    EXPECT_TRUE(reloadedText->isBold());
    EXPECT_TRUE(reloadedText->hasSourceTextKeyframes());
    EXPECT_EQ(reloadedText->sourceTextAtFrame(0), unicodeText);
    EXPECT_EQ(reloadedText->sourceTextAtFrame(6), unicodeText);
    EXPECT_EQ(reloadedText->sourceTextAtFrame(12),
              QStringLiteral("再読込後のテキスト"));
    EXPECT_EQ(reloadedText->animatorCount(), animatorCount);
    EXPECT_EQ(reloadedText->textAnimatorStackSnapshot(), animatorSnapshot);
    const auto reloadedPositionX =
        reloadedText->getProperty(QStringLiteral("text.animators.0.positionX"));
    ASSERT_TRUE(reloadedPositionX);
    ASSERT_EQ(reloadedPositionX->getKeyFrames().size(), 2u);
    EXPECT_DOUBLE_EQ(
        reloadedPositionX
            ->interpolateValue(ArtifactCore::RationalTime(12, 30))
            .toDouble(),
        24.0);

    auto reloadedDuplicateLayer =
        loadedCompositionPtr->layerById(duplicateLayerId);
    ASSERT_NE(reloadedDuplicateLayer.get(), nullptr);
    auto* reloadedDuplicate = dynamic_cast<Artifact::ArtifactTextLayer*>(
        reloadedDuplicateLayer.get());
    ASSERT_NE(reloadedDuplicate, nullptr);
    EXPECT_EQ(reloadedDuplicate->text().toQString(), unicodeText);
    EXPECT_EQ(reloadedDuplicate->fontFamily().toQString(),
              QStringLiteral("Noto Sans CJK JP"));
    EXPECT_FLOAT_EQ(reloadedDuplicate->fontSize(), 37.5f);
    EXPECT_FLOAT_EQ(reloadedDuplicate->tracking(), 1.25f);
    EXPECT_TRUE(reloadedDuplicate->isBold());
}

TEST(ArtifactProjectRoundTripTest,
     InlineTextImeCommitUsesOneUndoRedoTransaction)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("InlineTextEdit"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto composition = projects.createComposition(
        makeCompParams(QStringLiteral("InlineTextComposition"), 640, 360));
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactTextLayerInitParams params(QStringLiteral("Text"));
    const auto layerResult =
        projects.addLayerToComposition(composition.id, params);
    ASSERT_TRUE(layerResult.success);
    auto* textLayer =
        dynamic_cast<Artifact::ArtifactTextLayer*>(layerResult.layer.get());
    ASSERT_NE(textLayer, nullptr);
    const QString beforeText = QStringLiteral("Original text");
    const QString committedText = QStringLiteral("日本語入力🙂");
    textLayer->setText(ArtifactCore::UniString(beforeText));

    auto* undo = Artifact::UndoManager::instance();
    ASSERT_NE(undo, nullptr);
    undo->clearHistory();

    QWidget host;
    host.show();
    ASSERT_TRUE(Artifact::editTextLayerInline(
        &host, layerResult.layer, nullptr));
    QCoreApplication::processEvents();

    auto* dialog = host.findChild<QDialog*>();
    ASSERT_NE(dialog, nullptr);
    auto* editor = dialog->findChild<QTextEdit*>();
    ASSERT_NE(editor, nullptr);
    editor->setFocus();
    editor->selectAll();

    QInputMethodEvent preedit(QStringLiteral("にほん"), {});
    QCoreApplication::sendEvent(editor, &preedit);
    EXPECT_EQ(editor->toPlainText(), beforeText);
    QInputMethodEvent commitEvent;
    commitEvent.setCommitString(committedText);
    QCoreApplication::sendEvent(editor, &commitEvent);
    EXPECT_EQ(editor->toPlainText(), committedText);

    QKeyEvent applyEvent(QEvent::KeyPress, Qt::Key_Return,
                         Qt::ControlModifier, QStringLiteral("\r"));
    QCoreApplication::sendEvent(editor, &applyEvent);
    QCoreApplication::processEvents();

    EXPECT_EQ(textLayer->text().toQString(), committedText);
    ASSERT_EQ(undo->undoCount(), 1u);
    EXPECT_EQ(undo->undoDescription(), QStringLiteral("Edit Text"));
    undo->undo();
    EXPECT_EQ(textLayer->text().toQString(), beforeText);
    undo->redo();
    EXPECT_EQ(textLayer->text().toQString(), committedText);
}
