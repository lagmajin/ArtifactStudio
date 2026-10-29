#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVariant>
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

TEST(Rig2DContractTest, MapRangeConstraintClampsInputsAndWritesOnlySelectedChannel)
{
    Rig2D rig;
    Bone2D* target = rig.addBone(QStringLiteral("target"));
    target->setId(Id(QStringLiteral("map-target")));
    target->setLocalTransform(transformAt(2.0f, 3.0f, 15.0f, 1.5f, 2.0f));
    RigControl2D* control = rig.addSlider(QStringLiteral("driver"), 0.5, 0.0, 1.0);
    ASSERT_NE(control, nullptr);

    RigEvaluationContext2D context;
    context.indexBones(rig.bones());
    context.indexControls(rig.controls());
    MapRangeConstraint2D constraint(QStringLiteral("rotation map"), control->id(),
                                    target->id());
    constraint.setMapping(0.0, 1.0, -45.0, 45.0);
    constraint.evaluate(context);
    EXPECT_FLOAT_EQ(target->resolvedTransform().rotation, 0.0f);
    expectVectorNear(target->resolvedTransform().position, 2.0f, 3.0f);
    expectVectorNear(target->resolvedTransform().scale, 1.5f, 2.0f);

    control->setValue(2.0);
    constraint.setTargetChannel(QStringLiteral("position.x"));
    constraint.setMapping(0.0, 1.0, 10.0, 20.0);
    constraint.evaluate(context);
    EXPECT_FLOAT_EQ(target->resolvedTransform().position.x(), 20.0f);
    EXPECT_FLOAT_EQ(target->resolvedTransform().position.y(), 3.0f);
    EXPECT_FLOAT_EQ(target->resolvedTransform().rotation, 0.0f);

    control->setValue(0.25);
    constraint.setMapping(1.0, 0.0, 10.0, 20.0);
    constraint.evaluate(context);
    EXPECT_FLOAT_EQ(target->resolvedTransform().position.x(), 17.5f);

    constraint.setEnabled(false);
    const BoneTransform beforeDisabledEvaluation = target->resolvedTransform();
    control->setValue(0.75);
    constraint.evaluate(context);
    EXPECT_EQ(target->resolvedTransform().position, beforeDisabledEvaluation.position);
    EXPECT_FLOAT_EQ(target->resolvedTransform().rotation,
                    beforeDisabledEvaluation.rotation);
}

TEST(Rig2DContractTest, AimConstraintUsesParentWorldRotationAndIgnoresCoincidentTargets)
{
    Rig2D rig;
    Bone2D* parent = rig.addBone(QStringLiteral("parent"));
    parent->setId(Id(QStringLiteral("aim-parent")));
    parent->setLocalRotation(30.0f);
    parent->syncResolvedToLocal();
    Bone2D* source = rig.addBone(QStringLiteral("source"), parent);
    source->setId(Id(QStringLiteral("aim-source")));
    source->setLocalTransform(transformAt(0.0f, 0.0f, 12.0f));
    Bone2D* target = rig.addBone(QStringLiteral("target"));
    target->setId(Id(QStringLiteral("aim-target")));
    target->setLocalPosition(QVector2D(0.0f, 10.0f));
    target->syncResolvedToLocal();
    rig.update();
    target->updateHierarchy();

    RigEvaluationContext2D context;
    context.indexBones(rig.bones());
    AimConstraint2D constraint(QStringLiteral("aim"), source->id(), target->id());
    constraint.evaluate(context);
    EXPECT_NEAR(source->resolvedTransform().rotation, 60.0f, 1.0e-4f);

    source->setResolvedTransform(transformAt(0.0f, 0.0f, -20.0f));
    target->setResolvedTransform(transformAt(0.0f, 0.0f));
    constraint.evaluate(context);
    EXPECT_FLOAT_EQ(source->resolvedTransform().rotation, -20.0f);

    target->setResolvedTransform(transformAt(0.0f, 10.0f));
    constraint.setEnabled(false);
    constraint.evaluate(context);
    EXPECT_FLOAT_EQ(source->resolvedTransform().rotation, -20.0f);
}

TEST(Rig2DContractTest, PoseCaptureAndApplicationBlendBonesAndTypedControls)
{
    Rig2D rig;
    rig.setPoseName(QStringLiteral("captured pose"));
    Bone2D* bone = rig.addBone(QStringLiteral("bone"));
    bone->setId(Id(QStringLiteral("pose-bone")));
    bone->setLocalTransform(transformAt(10.0f, 20.0f, 90.0f, 3.0f, 5.0f));
    RigControl2D* slider = rig.addSlider(QStringLiteral("slider"), 0.75, -10.0, 10.0);
    RigControl2D* point = rig.addPoint(QStringLiteral("point"), QVector2D(3.0f, 4.0f));
    ASSERT_NE(slider, nullptr);
    ASSERT_NE(point, nullptr);

    const PoseSnapshot captured = capturePose(rig);
    EXPECT_EQ(captured.name, QStringLiteral("captured pose"));
    ASSERT_EQ(captured.boneTransforms.size(), 1u);
    ASSERT_EQ(captured.controlValues.size(), 2u);

    rig.setPoseName(QStringLiteral("edited pose"));
    bone->setLocalTransform(transformAt(0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
    slider->setValue(-1.0);
    point->setValue(QVariant::fromValue(QVector2D(0.0f, 0.0f)));
    applyPose(rig, captured, 0.5f);

    EXPECT_EQ(rig.poseName(), QStringLiteral("captured pose"));
    expectVectorNear(bone->localTransform().position, 5.0f, 10.0f);
    EXPECT_FLOAT_EQ(bone->localTransform().rotation, 45.0f);
    expectVectorNear(bone->localTransform().scale, 2.0f, 3.0f);
    EXPECT_DOUBLE_EQ(slider->value().toDouble(), -0.125);
    expectVectorNear(point->value().value<QVector2D>(), 1.5f, 2.0f);

    applyPose(rig, captured, 2.0f);
    EXPECT_EQ(bone->localTransform().position, QVector2D(10.0f, 20.0f));
    EXPECT_FLOAT_EQ(bone->localTransform().rotation, 90.0f);
    EXPECT_DOUBLE_EQ(slider->value().toDouble(), 0.75);
    EXPECT_EQ(point->value().value<QVector2D>(), QVector2D(3.0f, 4.0f));
}

TEST(Rig2DContractTest, PoseBlendingUsesShortestRotationAndPreservesOneSidedValues)
{
    const Id sharedBone(QStringLiteral("shared-bone"));
    const Id firstOnlyBone(QStringLiteral("first-only-bone"));
    const Id sharedSlider(QStringLiteral("shared-slider"));
    const Id firstOnlyControl(QStringLiteral("first-only-control"));
    const Id sharedPoint(QStringLiteral("shared-point"));
    PoseSnapshot first;
    first.name = QStringLiteral("first");
    first.boneTransforms[sharedBone] = transformAt(0.0f, 2.0f, 170.0f, 1.0f, 3.0f);
    first.boneTransforms[firstOnlyBone] = transformAt(7.0f, 8.0f);
    first.controlValues[sharedSlider] = 0.0;
    first.controlValues[firstOnlyControl] = QStringLiteral("left");
    first.controlValues[sharedPoint] = QVariant::fromValue(QVector2D(0.0f, 2.0f));

    PoseSnapshot second;
    second.name = QStringLiteral("second");
    second.boneTransforms[sharedBone] = transformAt(10.0f, 4.0f, -170.0f, 3.0f, 5.0f);
    second.controlValues[sharedSlider] = 1.0;
    second.controlValues[sharedPoint] = QVariant::fromValue(QVector2D(4.0f, 6.0f));

    const PoseSnapshot blended = blendPoses(first, second, 0.5f);
    EXPECT_EQ(blended.name, QStringLiteral("first -> second"));
    ASSERT_EQ(blended.boneTransforms.size(), 2u);
    expectVectorNear(blended.boneTransforms.at(sharedBone).position, 5.0f, 3.0f);
    EXPECT_NEAR(std::fabs(blended.boneTransforms.at(sharedBone).rotation), 180.0f, 1.0e-4f);
    expectVectorNear(blended.boneTransforms.at(sharedBone).scale, 2.0f, 4.0f);
    EXPECT_EQ(blended.boneTransforms.at(firstOnlyBone).position, QVector2D(7.0f, 8.0f));
    EXPECT_DOUBLE_EQ(blended.controlValues.at(sharedSlider).toDouble(), 0.5);
    EXPECT_EQ(blended.controlValues.at(firstOnlyControl).toString(), QStringLiteral("left"));
    expectVectorNear(blended.controlValues.at(sharedPoint).value<QVector2D>(), 2.0f, 4.0f);

    const PoseSnapshot endpoint = blendPoses(first, second, 2.0f);
    EXPECT_FLOAT_EQ(endpoint.boneTransforms.at(sharedBone).rotation, -170.0f);
    EXPECT_DOUBLE_EQ(endpoint.controlValues.at(sharedSlider).toDouble(), 1.0);
    EXPECT_EQ(endpoint.controlValues.at(firstOnlyControl).toString(), QStringLiteral("left"));
}

TEST(Rig2DContractTest, SkinMeshJsonSanitizesWeightsAndDropsInvalidTriangleIndices)
{
    const QJsonObject firstVertex{
        {QStringLiteral("px"), 1.0}, {QStringLiteral("py"), 2.0},
        {QStringLiteral("u"), 0.25}, {QStringLiteral("v"), 0.75},
        {QStringLiteral("w"), QJsonArray{2.0, 2.0, 0.0, 0.0}},
        {QStringLiteral("bi"), QJsonArray{0, 1, -1, -1}},
    };
    const QJsonObject zeroWeightVertex{
        {QStringLiteral("px"), 3.0}, {QStringLiteral("py"), 4.0},
        {QStringLiteral("w"), QJsonArray{0.0, 0.0, 0.0, 0.0}},
        {QStringLiteral("bi"), QJsonArray{-1, -1, -1, -1}},
    };
    const QJsonObject invalidIndexVertex{
        {QStringLiteral("px"), 5.0}, {QStringLiteral("py"), 6.0},
        {QStringLiteral("w"), QJsonArray{1.0, -2.0, 0.0, 0.0}},
        {QStringLiteral("bi"), QJsonArray{40000, 1, -1, -1}},
    };
    const QJsonObject object{
        {QStringLiteral("vertices"), QJsonArray{
            firstVertex, zeroWeightVertex, invalidIndexVertex}},
        {QStringLiteral("triangles"), QJsonArray{0, 1, 2, 0, 7, 2, 1, 2}},
    };

    const SkinMesh mesh = SkinMesh::fromJson(object);
    ASSERT_EQ(mesh.vertexCount(), 3);
    ASSERT_EQ(mesh.triangleCount(), 1);
    ASSERT_EQ(mesh.vertices().size(), 3u);
    EXPECT_EQ(mesh.vertices()[0].position, QVector2D(1.0f, 2.0f));
    EXPECT_EQ(mesh.vertices()[0].uv, QVector2D(0.25f, 0.75f));
    EXPECT_FLOAT_EQ(mesh.vertices()[0].weights[0], 0.5f);
    EXPECT_FLOAT_EQ(mesh.vertices()[0].weights[1], 0.5f);
    EXPECT_EQ(mesh.vertices()[0].boneIndices[0], 0);
    EXPECT_EQ(mesh.vertices()[0].boneIndices[1], 1);
    EXPECT_FLOAT_EQ(mesh.vertices()[1].weights[0], 1.0f);
    EXPECT_EQ(mesh.vertices()[1].boneIndices[0], 0);
    EXPECT_FLOAT_EQ(mesh.vertices()[2].weights[0], 1.0f);
    EXPECT_EQ(mesh.vertices()[2].boneIndices[0], -1);
    ASSERT_EQ(mesh.restPositions().size(), 3u);
    EXPECT_EQ(mesh.restPositions()[2], QVector2D(5.0f, 6.0f));
}
