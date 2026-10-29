#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>

#include <vector>

import Artifact.Layer.Shape;

namespace {

QCoreApplication* ensureApplication()
{
    static int argc = 1;
    static char appName[] = "ArtifactShapeLayerContentManagementContractTest";
    static char* argv[] = {appName, nullptr};
    static QCoreApplication app(argc, argv);
    return &app;
}

Artifact::ShapeContent makeContent(const QString& name, int width,
                                   const ArtifactCore::FloatColor& color)
{
    Artifact::ShapeContent content;
    content.name = name;
    content.geometry.type = Artifact::ShapeType::Ellipse;
    content.geometry.width = width;
    content.geometry.height = width + 2;
    content.transform.position = QPointF(width, -width);
    content.fill.color = color;
    content.fill.gradientStops = {
        {0.0f, color},
        {0.5f, ArtifactCore::FloatColor(0.2f, 0.3f, 0.4f, 0.5f)},
        {1.0f, ArtifactCore::FloatColor(0.9f, 0.8f, 0.7f, 1.0f)},
    };
    content.stroke.enabled = true;
    content.stroke.width = 3.5f;
    content.stroke.dashPattern = {2.0f, 1.0f, 4.0f};
    content.visible = false;
    content.opacity = 0.65f;
    content.merge = Artifact::ShapeContentMerge::Difference;
    return content;
}

} // namespace

TEST(ShapeLayerContentManagementContractTest,
     FirstContentSnapshotsLegacyPrimitiveAndAssignsStableDefaultName)
{
    ensureApplication();
    Artifact::ArtifactShapeLayer layer;
    layer.setShapeType(Artifact::ShapeType::Triangle);
    layer.setSize(74, 36);
    const ArtifactCore::FloatColor legacyColor(0.15f, 0.3f, 0.6f, 0.8f);
    layer.setFillColor(legacyColor);

    auto added = makeContent(QString(), 28,
                             ArtifactCore::FloatColor(0.8f, 0.1f, 0.2f, 1.0f));
    EXPECT_EQ(layer.addShapeContent(added), 1);
    ASSERT_EQ(layer.shapeContentCount(), 2);

    const auto legacy = layer.shapeContentAt(0);
    EXPECT_EQ(legacy.name, QStringLiteral("Shape 1"));
    EXPECT_EQ(legacy.geometry.type, Artifact::ShapeType::Triangle);
    EXPECT_EQ(legacy.geometry.width, 74);
    EXPECT_EQ(legacy.geometry.height, 36);
    EXPECT_EQ(legacy.fill.color, legacyColor);

    const auto created = layer.shapeContentAt(1);
    EXPECT_EQ(created.name, QStringLiteral("Shape 2"));
    EXPECT_EQ(created.geometry.width, 28);
    EXPECT_EQ(created.geometry.height, 30);
    EXPECT_EQ(created.merge, Artifact::ShapeContentMerge::Difference);
    EXPECT_FALSE(created.visible);
    EXPECT_FLOAT_EQ(created.opacity, 0.65f);
}

TEST(ShapeLayerContentManagementContractTest,
     ContentMoveUsesFinalIndexAndKeepsStackReferencesAttachedToContents)
{
    ensureApplication();
    Artifact::ArtifactShapeLayer layer;
    EXPECT_EQ(layer.addShapeContent(makeContent(
                  QStringLiteral("A"), 10, ArtifactCore::FloatColor(1, 0, 0, 1))), 1);
    EXPECT_EQ(layer.addShapeContent(makeContent(
                  QStringLiteral("B"), 20, ArtifactCore::FloatColor(0, 1, 0, 1))), 2);
    layer.resetShapeStackOrder();
    ASSERT_EQ(layer.shapeStackNodeCount(), 9);
    ASSERT_TRUE(layer.setActiveContentIndex(1));

    ASSERT_TRUE(layer.moveShapeContent(0, 2));
    EXPECT_EQ(layer.shapeContentAt(0).name, QStringLiteral("A"));
    EXPECT_EQ(layer.shapeContentAt(1).name, QStringLiteral("B"));
    EXPECT_EQ(layer.shapeContentAt(2).name, QStringLiteral("Shape 1"));
    EXPECT_EQ(layer.shapeStackNodeAt(0).contentIndex, 2);
    EXPECT_EQ(layer.shapeStackNodeAt(1).contentIndex, 0);
    EXPECT_EQ(layer.shapeStackNodeAt(2).contentIndex, 1);
    EXPECT_EQ(layer.shapeStackNodeAt(3).contentIndex, 2);
    EXPECT_EQ(layer.shapeStackNodeAt(4).contentIndex, 0);
    EXPECT_EQ(layer.shapeStackNodeAt(5).contentIndex, 1);
    EXPECT_EQ(layer.activeContentIndex(), 0);

    ASSERT_TRUE(layer.moveShapeContent(2, 0));
    EXPECT_EQ(layer.shapeContentAt(0).name, QStringLiteral("Shape 1"));
    EXPECT_EQ(layer.shapeContentAt(1).name, QStringLiteral("A"));
    EXPECT_EQ(layer.shapeContentAt(2).name, QStringLiteral("B"));
    EXPECT_EQ(layer.shapeStackNodeAt(0).contentIndex, 0);
    EXPECT_EQ(layer.shapeStackNodeAt(1).contentIndex, 1);
    EXPECT_EQ(layer.shapeStackNodeAt(2).contentIndex, 2);
    EXPECT_EQ(layer.activeContentIndex(), 1);
}

TEST(ShapeLayerContentManagementContractTest,
     SnapshotRestoresStylesStackAndActiveIndexAndRejectsInvalidInputAtomically)
{
    ensureApplication();
    Artifact::ArtifactShapeLayer source;
    EXPECT_EQ(source.addShapeContent(makeContent(
                  QStringLiteral("Gradient"), 40,
                  ArtifactCore::FloatColor(0.1f, 0.2f, 0.3f, 0.4f))), 1);
    source.addShapeOperator(ArtifactCore::ShapeOperatorType::TrimPaths);
    source.resetShapeStackOrder();
    ASSERT_TRUE(source.insertShapeStackNode(3,
        {Artifact::ShapeStackNodeType::Operator, -1, 0, false}));
    ASSERT_TRUE(source.setActiveContentIndex(1));

    const QJsonObject snapshot = source.shapeContentsSnapshot();
    Artifact::ArtifactShapeLayer restored;
    restored.restoreOperatorsFromJson(
        source.toJson().value(QStringLiteral("shapeOperators")).toArray());
    ASSERT_TRUE(restored.restoreShapeContentsSnapshot(snapshot));
    EXPECT_EQ(restored.shapeContentCount(), 2);
    EXPECT_EQ(restored.shapeContentAt(1).name, QStringLiteral("Gradient"));
    EXPECT_EQ(restored.shapeContentAt(1).fill.gradientStops.size(), 3u);
    EXPECT_EQ(restored.shapeContentAt(1).stroke.dashPattern,
              (std::vector<float>{2.0f, 1.0f, 4.0f}));
    EXPECT_EQ(restored.shapeStackNodeCount(), source.shapeStackNodeCount());
    EXPECT_EQ(restored.shapeStackNodeAt(3).type, Artifact::ShapeStackNodeType::Operator);
    EXPECT_EQ(restored.shapeStackNodeAt(3).operatorIndex, 0);
    EXPECT_FALSE(restored.shapeStackNodeAt(3).enabled);
    EXPECT_EQ(restored.activeContentIndex(), 1);

    const QJsonObject beforeRejectedInput = restored.shapeContentsSnapshot();
    QJsonObject invalid = snapshot;
    invalid[QStringLiteral("activeContentIndex")] = -2;
    EXPECT_FALSE(restored.restoreShapeContentsSnapshot(invalid));
    EXPECT_EQ(restored.shapeContentsSnapshot(), beforeRejectedInput);

    invalid = snapshot;
    QJsonArray invalidNodes = invalid.value(QStringLiteral("stackNodes")).toArray();
    QJsonObject invalidNode = invalidNodes.first().toObject();
    invalidNode[QStringLiteral("contentIndex")] = 99;
    invalidNodes[0] = invalidNode;
    invalid[QStringLiteral("stackNodes")] = invalidNodes;
    EXPECT_FALSE(restored.restoreShapeContentsSnapshot(invalid));
    EXPECT_EQ(restored.shapeContentsSnapshot(), beforeRejectedInput);
}

TEST(ShapeLayerContentManagementContractTest,
     ContentProxyClampsOpacityAndDuplicateKeepsIndependentGeometry)
{
    ensureApplication();
    Artifact::ArtifactShapeLayer layer;
    layer.addShapeContent(makeContent(
        QStringLiteral("Original"), 32,
        ArtifactCore::FloatColor(0.2f, 0.4f, 0.6f, 1.0f)));
    ASSERT_TRUE(layer.setActiveContentIndex(1));
    auto proxy = layer.activeContent();
    ASSERT_TRUE(proxy.isValid());
    proxy.setOpacity(4.0f);
    EXPECT_FLOAT_EQ(proxy.opacity(), 1.0f);
    proxy.setName(QStringLiteral("Edited"));
    ASSERT_TRUE(proxy.duplicate());
    ASSERT_EQ(layer.shapeContentCount(), 3);
    EXPECT_EQ(layer.shapeContentAt(2).name, QStringLiteral("Copy of Edited"));

    auto duplicate = layer.shapeContentAt(2);
    duplicate.geometry.width = 99;
    ASSERT_TRUE(layer.setShapeContentAt(2, duplicate));
    EXPECT_EQ(layer.shapeContentAt(1).geometry.width, 32);
    EXPECT_EQ(layer.shapeContentAt(2).geometry.width, 99);
    EXPECT_FALSE(layer.setActiveContentIndex(3));
    EXPECT_FALSE(layer.setShapeContentAt(-1, duplicate));
    EXPECT_FALSE(layer.removeShapeContentAt(3));
}
