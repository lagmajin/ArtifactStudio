#include <gtest/gtest.h>

#include <QMatrix4x4>
#include <QTransform>
#include <QVector3D>

#include <cmath>

import Transform.Hlper;

namespace {

void expectNear(double actual, double expected, double tolerance = 1.0e-4)
{
    EXPECT_NEAR(actual, expected, tolerance);
}

} // namespace

TEST(TransformHelperContractTest, TwoDimensionalTransformMapsAnchorToLayerPosition)
{
    const auto transform = ArtifactCore::makeLayerTransform2D(
        120.0, 85.0, 37.0, 1.7, 0.65, 24.0, -13.0);
    const QPointF mappedAnchor = transform.map(QPointF(24.0, -13.0));
    expectNear(mappedAnchor.x(), 120.0);
    expectNear(mappedAnchor.y(), 85.0);

    const QPointF localPoint(31.0, 9.0);
    const QPointF mapped = transform.map(localPoint);
    EXPECT_TRUE(std::isfinite(mapped.x()));
    EXPECT_TRUE(std::isfinite(mapped.y()));
    EXPECT_NE(mapped, localPoint);
}

TEST(TransformHelperContractTest, ThreeDimensionalTransformMapsAnchorToLayerPosition)
{
    const auto transform = ArtifactCore::makeLayerTransform3D(
        120.0, 85.0, -30.0, 63.0, 1.7, 0.65, 2.2, 24.0, -13.0, 7.0);
    const QVector3D mappedAnchor = transform.map(QVector3D(24.0f, -13.0f, 7.0f));
    expectNear(mappedAnchor.x(), 120.0);
    expectNear(mappedAnchor.y(), 85.0);
    expectNear(mappedAnchor.z(), -30.0);

    const QVector3D mapped = transform.map(QVector3D(31.0f, 9.0f, 11.0f));
    EXPECT_TRUE(std::isfinite(mapped.x()));
    EXPECT_TRUE(std::isfinite(mapped.y()));
    EXPECT_TRUE(std::isfinite(mapped.z()));
    EXPECT_NE(mapped, QVector3D(31.0f, 9.0f, 11.0f));
}

TEST(TransformHelperContractTest, TwoDimensionalCombinationAppliesParentAfterLocal)
{
    const QTransform local = ArtifactCore::makeLayerTransform2D(
        10.0, 4.0, 0.0, 2.0, 2.0, 0.0, 0.0);
    const QTransform parent = ArtifactCore::makeLayerTransform2D(
        5.0, 7.0, 0.0, 1.0, 1.0, 0.0, 0.0);
    const QPointF point(3.0, 2.0);

    const QPointF expected = parent.map(local.map(point));
    const QPointF actual = ArtifactCore::combineLayerTransform2D(local, parent).map(point);
    expectNear(actual.x(), expected.x());
    expectNear(actual.y(), expected.y());
}
