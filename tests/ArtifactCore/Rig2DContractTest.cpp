#include <gtest/gtest.h>

#include <QJsonObject>
#include <QJsonValue>
#include <QVector2D>

#include <cmath>
#include <map>
#include <memory>
#include <utility>
#include <vector>

import ArtifactCore.Rig2D;
import Utils.Id;

using namespace ArtifactCore;

namespace {

BoneTransform transformAt(float x, float y, float rotation = 0.0f,
                          float scaleX = 1.0f, float scaleY = 1.0f)
{
    return {QVector2D(x, y), rotation, QVector2D(scaleX, scaleY)};
}

void expectVectorNear(const QVector2D& actual, float x, float y, float tolerance = 1.0e-4f)
{
    EXPECT_NEAR(actual.x(), x, tolerance);
    EXPECT_NEAR(actual.y(), y, tolerance);
}

} // namespace

TEST(Rig2DContractTest, TransformJsonRoundTripAndPartialFieldsUseFallback)
{
    const BoneTransform source = transformAt(12.5f, -8.0f, 37.0f, 1.5f, 0.75f);
    const BoneTransform restored = rigTransformFromJson(
        rigTransformToJson(source), BoneTransform{});
    expectVectorNear(restored.position, 12.5f, -8.0f);
    EXPECT_FLOAT_EQ(restored.rotation, 37.0f);
    expectVectorNear(restored.scale, 1.5f, 0.75f);

    const BoneTransform fallback = transformAt(3.0f, 4.0f, 5.0f, 2.0f, 6.0f);
    const QJsonObject partial{
        {QStringLiteral("position"), QJsonObject{{QStringLiteral("x"), 10.0}}},
        {QStringLiteral("rotation"), 25.0},
    };
    const BoneTransform result = rigTransformFromJson(partial, fallback);
    expectVectorNear(result.position, 10.0f, 4.0f);
    EXPECT_FLOAT_EQ(result.rotation, 25.0f);
    expectVectorNear(result.scale, 2.0f, 6.0f);
    EXPECT_EQ(rigTransformFromJson(QJsonValue(QStringLiteral("invalid")), fallback).position,
              fallback.position);
}

TEST(Rig2DContractTest, StretchyMidpointUsesPerpendicularBendAndClampsDescriptor)
{
    const QVector2D root(2.0f, 3.0f);
    const QVector2D tip(12.0f, 3.0f);
    const StretchyLimbDescriptor positive{8.0f, 1.5f, 0.5f, false};
    expectVectorNear(computeStretchyMidpoint(root, tip, positive), 7.0f, 4.5f);

    const StretchyLimbDescriptor negative{8.0f, 1.5f, -0.5f, false};
    expectVectorNear(computeStretchyMidpoint(root, tip, negative), 7.0f, 1.5f);

    const StretchyLimbDescriptor rubberhose{8.0f, 1.5f, 1.0f, true};
    expectVectorNear(computeStretchyMidpoint(root, tip, rubberhose), 7.0f, 7.2f);

    const StretchyLimbDescriptor clamped{8.0f, 1.5f, 4.0f, false};
    expectVectorNear(computeStretchyMidpoint(root, tip, clamped), 7.0f, 6.0f);
    EXPECT_EQ(computeStretchyMidpoint(root, root, positive), root);
}

TEST(Rig2DContractTest, HierarchyComposesParentTransformsAndKeepsLimitedEditValue)
{
    Bone2D root(QStringLiteral("root"));
    Bone2D child(QStringLiteral("child"));
    root.setLocalTransform(transformAt(10.0f, 0.0f, 90.0f));
    child.setLocalTransform(transformAt(5.0f, 0.0f, 90.0f));
    root.addChild(&child);
    root.updateHierarchy();

    expectVectorNear(QVector2D(child.globalMatrix().column(3).x(),
                               child.globalMatrix().column(3).y()), 10.0f, 5.0f);
    EXPECT_NEAR(std::atan2(child.globalMatrix()(1, 0), child.globalMatrix()(0, 0))
                    * 180.0f / 3.14159265358979323846f,
                180.0f, 1.0e-3f);

    child.setRotationLimits(-20.0f, 30.0f);
    child.setLocalRotation(75.0f);
    child.syncResolvedToLocal();
    child.updateHierarchy();
    EXPECT_FLOAT_EQ(child.localTransform().rotation, 75.0f);
    EXPECT_FLOAT_EQ(child.resolvedTransform().rotation, 75.0f);
    EXPECT_NEAR(std::atan2(child.globalMatrix()(1, 0), child.globalMatrix()(0, 0))
                    * 180.0f / 3.14159265358979323846f,
                120.0f, 1.0e-3f);
}

TEST(Rig2DContractTest, SmartBoneInterpolatesSharedTargetsAndUnionsOneSidedTargets)
{
    const Id sharedId(QStringLiteral("shared"));
    const Id firstOnlyId(QStringLiteral("first"));
    const Id secondOnlyId(QStringLiteral("second"));
    SmartBoneController controller;
    controller.addKey(90.0f, {{sharedId, transformAt(10.0f, 0.0f, 90.0f)}});
    controller.addKey(-90.0f, {
        {sharedId, transformAt(0.0f, 10.0f, -90.0f)},
        {firstOnlyId, transformAt(2.0f, 4.0f)},
    });
    controller.addKey(90.0005f, {{sharedId, transformAt(10.0f, 0.0f, 90.0f)}});

    ASSERT_EQ(controller.keyCount(), 2u);
    std::map<Id, BoneTransform> result{{Id(QStringLiteral("stale")), BoneTransform{}}};
    controller.evaluate(0.0f, result);
    EXPECT_EQ(result.size(), 2u);
    ASSERT_TRUE(result.contains(sharedId));
    ASSERT_TRUE(result.contains(firstOnlyId));
    expectVectorNear(result.at(sharedId).position, 5.0f, 5.0f);
    EXPECT_FLOAT_EQ(result.at(sharedId).rotation, 0.0f);

    controller.addKey(180.0f, {{secondOnlyId, transformAt(7.0f, 8.0f)}});
    controller.evaluate(200.0f, result);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_TRUE(result.contains(secondOnlyId));
}

TEST(Rig2DContractTest, ControlsClampReversedRangesAndRoundTripPointValues)
{
    RigControl2D slider(QStringLiteral("gain"), RigControlKind::Slider);
    slider.setRange(1.0, -1.0);
    slider.setValue(4.0);
    EXPECT_DOUBLE_EQ(slider.value().toDouble(), 1.0);
    slider.setValue(-4.0);
    EXPECT_DOUBLE_EQ(slider.value().toDouble(), -1.0);

    RigControl2D point(QStringLiteral("pivot"), RigControlKind::Point);
    point.setRange(QVariant::fromValue(QVector2D(10.0f, -2.0f)),
                   QVariant::fromValue(QVector2D(-10.0f, 2.0f)));
    point.setValue(QVariant::fromValue(QVector2D(25.0f, -8.0f)));
    const RigControl2D restored = RigControl2D::fromJson(point.toJson());
    EXPECT_EQ(restored.kind(), RigControlKind::Point);
    EXPECT_EQ(restored.value().value<QVector2D>(), QVector2D(10.0f, -2.0f));
    EXPECT_EQ(restored.minValue().value<QVector2D>(), QVector2D(10.0f, -2.0f));
    EXPECT_EQ(restored.maxValue().value<QVector2D>(), QVector2D(-10.0f, 2.0f));
}

TEST(Rig2DContractTest, SkinMeshAutoBindNormalizesWeightsAndDeformPreservesUnboundRest)
{
    Rig2D rig;
    Bone2D* nearBone = rig.addBone(QStringLiteral("near"));
    Bone2D* farBone = rig.addBone(QStringLiteral("far"));
    nearBone->setLocalPosition(QVector2D(0.0f, 0.0f));
    farBone->setLocalPosition(QVector2D(10.0f, 0.0f));
    rig.update();
    farBone->updateHierarchy();

    SkinMesh mesh;
    SkinVertex nearVertex;
    nearVertex.position = QVector2D(1.0f, 0.0f);
    mesh.setVertices({nearVertex});
    mesh.autoBind(&rig, 2);

    const auto& vertices = mesh.vertices();
    ASSERT_EQ(vertices.size(), 1u);
    EXPECT_EQ(vertices[0].boneIndices[0], 0);
    EXPECT_EQ(vertices[0].boneIndices[1], 1);
    EXPECT_NEAR(vertices[0].weights[0] + vertices[0].weights[1], 1.0f, 1.0e-6f);

    nearBone->setLocalPosition(QVector2D(2.0f, 0.0f));
    farBone->setLocalPosition(QVector2D(12.0f, 0.0f));
    rig.update();
    farBone->updateHierarchy();
    std::vector<QVector2D> deformed;
    mesh.deform(&rig, deformed);
    ASSERT_EQ(deformed.size(), 1u);
    expectVectorNear(deformed[0], 3.0f, 0.0f);

    SkinMesh partialMesh;
    SkinVertex partialVertex;
    partialVertex.position = QVector2D(3.0f, 4.0f);
    partialVertex.weights[0] = 0.5f;
    partialVertex.boneIndices[0] = 0;
    SkinVertex unboundVertex;
    unboundVertex.position = QVector2D(6.0f, 7.0f);
    unboundVertex.weights[0] = 0.0f;
    unboundVertex.boneIndices[0] = -1;
    partialMesh.setVertices({partialVertex, unboundVertex});
    partialMesh.deform(&rig, deformed);
    ASSERT_EQ(deformed.size(), 2u);
    expectVectorNear(deformed[0], 4.0f, 4.0f);
    expectVectorNear(deformed[1], 6.0f, 7.0f);
}

TEST(Rig2DContractTest, RigJsonRoundTripRestoresBoneHierarchyControlsAndPoseName)
{
    Rig2D rig;
    rig.setPoseName(QStringLiteral("rest pose"));
    Bone2D* root = rig.addBone(QStringLiteral("root"));
    Bone2D* child = rig.addBone(QStringLiteral("child"), root);
    root->setId(Id(QStringLiteral("root-id")));
    child->setId(Id(QStringLiteral("child-id")));
    root->setLocalTransform(transformAt(4.0f, 5.0f, 12.0f));
    child->setLocalTransform(transformAt(2.0f, 0.0f));
    rig.setRootBone(root);
    rig.addSlider(QStringLiteral("weight"), 0.75, 0.0, 1.0);

    const QJsonObject serialized = rig.toJson();
    Rig2D restored = Rig2D::fromJson(serialized);
    ASSERT_NE(restored.rootBone(), nullptr);
    ASSERT_NE(restored.findBone(QStringLiteral("child")), nullptr);
    EXPECT_EQ(restored.rootBone()->id(), Id(QStringLiteral("root-id")));
    EXPECT_EQ(restored.findBone(QStringLiteral("child"))->parent(), restored.rootBone());
    EXPECT_EQ(restored.poseName(), QStringLiteral("rest pose"));
    ASSERT_EQ(restored.controlCount(), 1);
    EXPECT_DOUBLE_EQ(restored.controls().first()->value().toDouble(), 0.75);
    expectVectorNear(QVector2D(restored.rootBone()->globalMatrix().column(3).x(),
                               restored.rootBone()->globalMatrix().column(3).y()), 4.0f, 5.0f);
}

TEST(Rig2DContractTest, MoveConstructionAndAssignmentTransferAllOwnedRigState)
{
    Rig2D source;
    source.setPoseName(QStringLiteral("source pose"));
    source.addBone(QStringLiteral("source bone"));
    source.addSlider(QStringLiteral("source control"), 0.5, 0.0, 1.0);
    source.addSmartBone()->addKey(10.0f, {});
    SkinVertex vertex;
    vertex.position = QVector2D(2.0f, 3.0f);
    source.createSkinMesh()->setVertices({vertex});

    Rig2D moved(std::move(source));
    EXPECT_EQ(moved.poseName(), QStringLiteral("source pose"));
    ASSERT_EQ(moved.bones().size(), 1);
    EXPECT_EQ(moved.findBone(QStringLiteral("source bone")), moved.rootBone());
    EXPECT_EQ(moved.controlCount(), 1);
    EXPECT_EQ(moved.smartBoneCount(), 1);
    ASSERT_NE(moved.skinMesh(), nullptr);
    ASSERT_EQ(moved.skinMesh()->vertexCount(), 1);
    EXPECT_EQ(moved.skinMesh()->vertices()[0].position, QVector2D(2.0f, 3.0f));

    Rig2D destination;
    destination.setPoseName(QStringLiteral("old pose"));
    destination.addBone(QStringLiteral("old bone"));
    destination.addSlider(QStringLiteral("old control"), 0.25, 0.0, 1.0);
    destination = std::move(moved);
    EXPECT_EQ(destination.poseName(), QStringLiteral("source pose"));
    ASSERT_EQ(destination.bones().size(), 1);
    EXPECT_EQ(destination.findBone(QStringLiteral("source bone")), destination.rootBone());
    EXPECT_EQ(destination.controlCount(), 1);
    EXPECT_NE(destination.findControl(QStringLiteral("source control")), nullptr);
    EXPECT_EQ(destination.findControl(QStringLiteral("old control")), nullptr);
    EXPECT_EQ(destination.smartBoneCount(), 1);
    ASSERT_NE(destination.skinMesh(), nullptr);
    EXPECT_EQ(destination.skinMesh()->vertexCount(), 1);
}
