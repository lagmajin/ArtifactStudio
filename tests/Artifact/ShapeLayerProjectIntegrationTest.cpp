#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPointF>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>

#include <vector>

import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Layer.Shape;
import Artifact.Project.Health;
import Artifact.Project.Manager;
import Shape.Operator;
import Color.Float;
import Utils.String.UniString;

namespace {

class ShapeLayerProjectEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "ShapeLayerProjectIntegrationTest";
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

ShapeLayerProjectEnvironment* const shapeLayerProjectEnvironment =
    new ShapeLayerProjectEnvironment();
const bool shapeLayerProjectEnvironmentRegistered = [] {
    testing::AddGlobalTestEnvironment(shapeLayerProjectEnvironment);
    return true;
}();

struct ProjectCleanup {
    Artifact::ArtifactProjectManager& projects;
    ~ProjectCleanup() { projects.closeCurrentProject(); }
};

std::vector<Artifact::CustomPathVertex> pathAt(double offset)
{
    return {
        {QPointF(offset, 2.0), QPointF(-1.0, 0.0), QPointF(2.0, -1.0), true},
        {QPointF(18.0 + offset, 3.0), QPointF(-2.0, -1.0),
         QPointF(1.0, 2.0), false},
        {QPointF(9.0 + offset, 18.0), QPointF(1.0, -2.0),
         QPointF(-1.0, -2.0), true},
    };
}

} // namespace

TEST(ShapeLayerProjectIntegrationTest,
     EditedShapeAnimationAndContentStackSurviveProjectSaveAndReload)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(
        QStringLiteral("ArtifactStudioShapeLayerProjectTest"));
    QStandardPaths::setTestModeEnabled(true);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("ShapeLayerRoundTrip"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("ShapeComposition")));
    compositionParams.setResolution(160, 90);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(24);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactLayerInitParams layerParams(
        QStringLiteral("AnimatedShape"), Artifact::LayerType::Shape);
    const auto added = projects.addLayerToComposition(composition.id, layerParams);
    ASSERT_TRUE(added.success);
    auto* source = dynamic_cast<Artifact::ArtifactShapeLayer*>(added.layer.get());
    ASSERT_NE(source, nullptr);

    source->setShapeType(Artifact::ShapeType::Polygon);
    source->setSize(42, 30);
    source->setPolygonSides(5);
    source->setCustomPathVertices(pathAt(0.0), true);
    source->setPathKeyframe(0, pathAt(0.0));
    source->setPathKeyframe(12, pathAt(24.0));
    source->setOpacity(0.73f);

    Artifact::ShapeContent detail;
    detail.name = QStringLiteral("CurvedDetail");
    detail.geometry.type = Artifact::ShapeType::Polygon;
    detail.geometry.width = 36;
    detail.geometry.height = 28;
    detail.geometry.polygonSides = 7;
    detail.geometry.pathVertices = pathAt(4.0);
    detail.geometry.pathClosed = true;
    detail.transform.position = QPointF(11.5, -6.25);
    detail.transform.rotation = 17.0;
    detail.fill.type = Artifact::ArtifactSolidFillType::LinearGradient;
    detail.fill.gradientStops = {
        {0.0f, ArtifactCore::FloatColor(0.9f, 0.1f, 0.2f, 1.0f)},
        {0.4f, ArtifactCore::FloatColor(0.1f, 0.8f, 0.3f, 0.75f)},
        {1.0f, ArtifactCore::FloatColor(0.1f, 0.2f, 0.9f, 1.0f)},
    };
    detail.stroke.enabled = true;
    detail.stroke.width = 3.25f;
    detail.stroke.dashPattern = {5.0f, 2.0f, 1.0f, 2.0f};
    detail.stroke.waveEnabled = true;
    detail.stroke.waveAmount = 1.5f;
    detail.stroke.waveFrequency = 2.25f;
    detail.visible = true;
    detail.opacity = 0.62f;
    detail.merge = Artifact::ShapeContentMerge::Difference;
    const int detailIndex = source->addShapeContent(detail);
    ASSERT_GE(detailIndex, 0);
    ASSERT_TRUE(source->setActiveContentIndex(detailIndex));
    source->addShapeOperator(ArtifactCore::ShapeOperatorType::TrimPaths);
    source->resetShapeStackOrder();
    ASSERT_GT(source->shapeStackNodeCount(), 0);

    const QString projectPath =
        projectRoot.filePath(QStringLiteral("shape-layer.artifact"));
    const auto saved = projects.saveToFile(projectPath);
    ASSERT_TRUE(saved.success) << saved.errorMessage.toStdString();
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    auto restoredComposition = projects.findComposition(composition.id).ptr.lock();
    ASSERT_TRUE(restoredComposition);
    ASSERT_EQ(restoredComposition->layerCount(), 1);
    auto restoredBaseLayer = restoredComposition->frontMostLayer();
    ASSERT_NE(restoredBaseLayer.get(), nullptr);
    auto* restored =
        dynamic_cast<Artifact::ArtifactShapeLayer*>(restoredBaseLayer.get());
    ASSERT_NE(restored, nullptr);

    EXPECT_EQ(restored->shapeType(), Artifact::ShapeType::Polygon);
    EXPECT_EQ(restored->polygonSides(), 5);
    EXPECT_FLOAT_EQ(restored->opacity(), 0.73f);
    ASSERT_TRUE(restored->hasPathKeyframes());
    const auto frame0 = restored->evaluatePathAt(0);
    const auto frame6 = restored->evaluatePathAt(6);
    const auto frame12 = restored->evaluatePathAt(12);
    ASSERT_EQ(frame0.size(), 3u);
    ASSERT_EQ(frame6.size(), 3u);
    ASSERT_EQ(frame12.size(), 3u);
    EXPECT_DOUBLE_EQ(frame0.front().pos.x(), 0.0);
    EXPECT_DOUBLE_EQ(frame6.front().pos.x(), 12.0);
    EXPECT_DOUBLE_EQ(frame12.front().pos.x(), 24.0);
    EXPECT_DOUBLE_EQ(frame6.front().outTangent.x(), 2.0);
    EXPECT_TRUE(frame6.front().smooth);

    ASSERT_EQ(restored->shapeContentCount(), 2);
    EXPECT_EQ(restored->activeContentIndex(), detailIndex);
    const auto restoredDetail = restored->shapeContentAt(detailIndex);
    EXPECT_EQ(restoredDetail.name, QStringLiteral("CurvedDetail"));
    EXPECT_EQ(restoredDetail.geometry.pathVertices.size(), 3u);
    EXPECT_TRUE(restoredDetail.geometry.pathClosed);
    EXPECT_DOUBLE_EQ(restoredDetail.transform.position.x(), 11.5);
    EXPECT_DOUBLE_EQ(restoredDetail.transform.position.y(), -6.25);
    EXPECT_DOUBLE_EQ(restoredDetail.transform.rotation, 17.0);
    ASSERT_EQ(restoredDetail.fill.gradientStops.size(), 3u);
    EXPECT_FLOAT_EQ(restoredDetail.fill.gradientStops[1].offset, 0.4f);
    EXPECT_FLOAT_EQ(restoredDetail.fill.gradientStops[1].color.g(), 0.8f);
    EXPECT_TRUE(restoredDetail.stroke.enabled);
    EXPECT_FLOAT_EQ(restoredDetail.stroke.width, 3.25f);
    EXPECT_EQ(restoredDetail.stroke.dashPattern,
              (std::vector<float>{5.0f, 2.0f, 1.0f, 2.0f}));
    EXPECT_TRUE(restoredDetail.stroke.waveEnabled);
    EXPECT_FLOAT_EQ(restoredDetail.stroke.waveAmount, 1.5f);
    EXPECT_FLOAT_EQ(restoredDetail.stroke.waveFrequency, 2.25f);
    EXPECT_EQ(restoredDetail.merge, Artifact::ShapeContentMerge::Difference);
    EXPECT_FLOAT_EQ(restoredDetail.opacity, 0.62f);

    ASSERT_EQ(restored->shapeOperatorCount(), 1);
    EXPECT_EQ(restored->shapeOperatorTypeAt(0),
              ArtifactCore::ShapeOperatorType::TrimPaths);
    ASSERT_EQ(restored->shapeStackNodeCount(), 7);
    EXPECT_EQ(restored->shapeStackNodeAt(0).type,
              Artifact::ShapeStackNodeType::Path);
    EXPECT_EQ(restored->shapeStackNodeAt(1).type,
              Artifact::ShapeStackNodeType::Path);
    EXPECT_EQ(restored->shapeStackNodeAt(2).type,
              Artifact::ShapeStackNodeType::Operator);
    EXPECT_EQ(restored->shapeStackNodeAt(3).type,
              Artifact::ShapeStackNodeType::Fill);
    EXPECT_EQ(restored->shapeStackNodeAt(4).type,
              Artifact::ShapeStackNodeType::Fill);
    EXPECT_EQ(restored->shapeStackNodeAt(5).type,
              Artifact::ShapeStackNodeType::Stroke);
    EXPECT_EQ(restored->shapeStackNodeAt(6).type,
              Artifact::ShapeStackNodeType::Stroke);
    bool stackReferencesOperator = false;
    int contentPathNodes = 0;
    int contentFillNodes = 0;
    int contentStrokeNodes = 0;
    for (int i = 0; i < restored->shapeStackNodeCount(); ++i) {
        const auto node = restored->shapeStackNodeAt(i);
        if (node.type == Artifact::ShapeStackNodeType::Operator) {
            EXPECT_EQ(node.operatorIndex, 0);
            stackReferencesOperator = true;
            continue;
        }
        EXPECT_GE(node.contentIndex, 0);
        EXPECT_LT(node.contentIndex, restored->shapeContentCount());
        switch (node.type) {
        case Artifact::ShapeStackNodeType::Path:
            ++contentPathNodes;
            break;
        case Artifact::ShapeStackNodeType::Fill:
            ++contentFillNodes;
            break;
        case Artifact::ShapeStackNodeType::Stroke:
            ++contentStrokeNodes;
            break;
        case Artifact::ShapeStackNodeType::Operator:
            break;
        }
    }
    EXPECT_TRUE(stackReferencesOperator);
    EXPECT_EQ(contentPathNodes, 2);
    EXPECT_EQ(contentFillNodes, 2);
    EXPECT_EQ(contentStrokeNodes, 2);
    const auto restoredProject = projects.getCurrentProjectSharedPtr();
    ASSERT_TRUE(restoredProject);
    EXPECT_TRUE(restoredProject->validateProjectTree(nullptr));
    const auto health =
        Artifact::ArtifactProjectHealthChecker::check(restoredProject.get());
    EXPECT_TRUE(health.isHealthy);
}
