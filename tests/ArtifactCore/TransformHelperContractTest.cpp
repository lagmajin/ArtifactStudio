#include <gtest/gtest.h>

#include <QMatrix4x4>
#include <QPointF>
#include <QTransform>
#include <QVector3D>

import Transform.Hlper;
import Math.Matrix4x4;

namespace {

QVector3D mapPoint(const ArtifactCore::Matrix4x4& matrix, const QVector3D& point)
{
    const auto& m = matrix.m;
    return {
        m[0] * point.x() + m[1] * point.y() + m[2] * point.z() + m[3],
        m[4] * point.x() + m[5] * point.y() + m[6] * point.z() + m[7],
        m[8] * point.x() + m[9] * point.y() + m[10] * point.z() + m[11],
    };
}

void expectPointNear(const QVector3D& actual, const QVector3D& expected)
{
    EXPECT_NEAR(actual.x(), expected.x(), 1.0e-4f);
    EXPECT_NEAR(actual.y(), expected.y(), 1.0e-4f);
    EXPECT_NEAR(actual.z(), expected.z(), 1.0e-4f);
}

} // namespace

TEST(TransformHelperContractTest, CustomMatrixLayerTransformMatchesQtThreeDimensionalPath)
{
    const auto custom = ArtifactCore::makeLayerMatrix4x4(
        120.0, 85.0, -30.0, 63.0, 1.7, 0.65, 2.2, 24.0, -13.0, 7.0);
    const auto qt = ArtifactCore::makeLayerTransform3D(
        120.0, 85.0, -30.0, 63.0, 1.7, 0.65, 2.2, 24.0, -13.0, 7.0);

    const QVector3D anchor(24.0f, -13.0f, 7.0f);
    expectPointNear(mapPoint(custom, anchor), qt.map(anchor));

    const QVector3D probe(31.0f, 9.0f, 11.0f);
    expectPointNear(mapPoint(custom, probe), qt.map(probe));
}

TEST(TransformHelperContractTest, CustomMatrixHierarchyAppliesLocalThenParent)
{
    const auto local = ArtifactCore::makeLayerMatrix4x4(
        10.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0);
    const auto parent = ArtifactCore::makeLayerMatrix4x4(
        0.0, 0.0, 0.0, 0.0, 2.0, 2.0, 2.0, 0.0, 0.0, 0.0);

    const auto combined = ArtifactCore::combineLayerMatrix4x4(local, parent);
    expectPointNear(mapPoint(combined, QVector3D(0.0f, 0.0f, 0.0f)),
                    QVector3D(20.0f, 0.0f, 0.0f));
}

TEST(TransformHelperContractTest, ThreeLevelHierarchyComposesEachAncestorOutsideChild)
{
    const auto local = ArtifactCore::makeLayerMatrix4x4(
        2.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0);
    const auto parent = ArtifactCore::makeLayerMatrix4x4(
        0.0, 0.0, 0.0, 90.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0);
    const auto root = ArtifactCore::makeLayerMatrix4x4(
        0.0, 0.0, 0.0, 0.0, 3.0, 3.0, 3.0, 0.0, 0.0, 0.0);

    const auto childInParent = ArtifactCore::combineLayerMatrix4x4(local, parent);
    const auto childInRoot = ArtifactCore::combineLayerMatrix4x4(childInParent, root);

    expectPointNear(mapPoint(childInRoot, QVector3D(0.0f, 0.0f, 0.0f)),
                    QVector3D(0.0f, 6.0f, 0.0f));
}

TEST(TransformHelperContractTest, QtAndCustomMatrixHierarchyCompositionAgree)
{
    const auto localCustom = ArtifactCore::makeLayerMatrix4x4(
        3.0, -2.0, 1.0, 35.0, 1.25, 0.75, 2.0, 1.0, -3.0, 0.5);
    const auto parentCustom = ArtifactCore::makeLayerMatrix4x4(
        -4.0, 6.0, 2.0, -20.0, 0.8, 1.4, 0.6, -2.0, 1.0, 3.0);
    const auto customCombined = ArtifactCore::combineLayerMatrix4x4(
        localCustom, parentCustom);

    const auto localQt = ArtifactCore::makeLayerTransform3D(
        3.0, -2.0, 1.0, 35.0, 1.25, 0.75, 2.0, 1.0, -3.0, 0.5);
    const auto parentQt = ArtifactCore::makeLayerTransform3D(
        -4.0, 6.0, 2.0, -20.0, 0.8, 1.4, 0.6, -2.0, 1.0, 3.0);
    const auto qtCombined = ArtifactCore::combineLayerTransform3D(
        localQt, parentQt);
    const QVector3D probe(2.5f, -1.25f, 4.0f);

    expectPointNear(mapPoint(customCombined, probe), qtCombined.map(probe));
}

TEST(TransformHelperContractTest, TwoDimensionalTransformMapsAnchorToPosition)
{
    const QTransform transform = ArtifactCore::makeLayerTransform2D(
        120.0, 85.0, 37.0, 1.7, 0.65, 24.0, -13.0);

    const QPointF mappedAnchor = transform.map(QPointF(24.0, -13.0));
    EXPECT_NEAR(mappedAnchor.x(), 120.0, 1.0e-9);
    EXPECT_NEAR(mappedAnchor.y(), 85.0, 1.0e-9);
}

TEST(TransformHelperContractTest, TwoDimensionalHierarchyAppliesParentAfterLocal)
{
    const QTransform local = ArtifactCore::makeLayerTransform2D(
        10.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0);
    const QTransform parent = ArtifactCore::makeLayerTransform2D(
        0.0, 0.0, 0.0, 2.0, 2.0, 0.0, 0.0);

    const QTransform combined = ArtifactCore::combineLayerTransform2D(local, parent);
    const QPointF mappedOrigin = combined.map(QPointF(0.0, 0.0));

    EXPECT_NEAR(mappedOrigin.x(), 20.0, 1.0e-9);
    EXPECT_NEAR(mappedOrigin.y(), 0.0, 1.0e-9);
}
