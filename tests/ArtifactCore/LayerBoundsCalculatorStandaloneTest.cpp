#include <gtest/gtest.h>

#include <algorithm>
#include <array>

#include <limits>

#include <QPointF>
#include <QRectF>

import Geometry.LayerBounds;

using namespace ArtifactCore;

TEST(LayerBoundsCalculatorStandaloneTest, UntransformedCornersUsePositionAndDimensions)
{
    const auto corners = LayerBoundsCalculator2D::calculate({10.0, 20.0}, {}, 100.0f, 50.0f, 0.0f);
    EXPECT_DOUBLE_EQ(corners.topLeft.x(), 10.0);
    EXPECT_DOUBLE_EQ(corners.topLeft.y(), 20.0);
    EXPECT_DOUBLE_EQ(corners.topRight.x(), 110.0);
    EXPECT_DOUBLE_EQ(corners.topRight.y(), 20.0);
    EXPECT_DOUBLE_EQ(corners.bottomLeft.x(), 10.0);
    EXPECT_DOUBLE_EQ(corners.bottomLeft.y(), 70.0);
    EXPECT_DOUBLE_EQ(corners.bottomRight.x(), 110.0);
    EXPECT_DOUBLE_EQ(corners.bottomRight.y(), 70.0);
}

TEST(LayerBoundsCalculatorStandaloneTest, AnchorOffsetsLocalCornersAroundWorldPosition)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {50.0, 40.0}, {10.0, 5.0}, 30.0f, 20.0f, 0.0f);
    EXPECT_DOUBLE_EQ(corners.topLeft.x(), 40.0);
    EXPECT_DOUBLE_EQ(corners.topLeft.y(), 35.0);
    EXPECT_DOUBLE_EQ(corners.bottomRight.x(), 70.0);
    EXPECT_DOUBLE_EQ(corners.bottomRight.y(), 55.0);
}

TEST(LayerBoundsCalculatorStandaloneTest, RotationTransformsCornersAroundAnchorPosition)
{
    const auto corners = LayerBoundsCalculator2D::calculate({}, {}, 10.0f, 20.0f, 90.0f);
    EXPECT_NEAR(corners.topRight.x(), 0.0, 1e-5);
    EXPECT_NEAR(corners.topRight.y(), 10.0, 1e-5);
    EXPECT_NEAR(corners.bottomLeft.x(), -20.0, 1e-5);
    EXPECT_NEAR(corners.bottomLeft.y(), 0.0, 1e-5);
}

TEST(LayerBoundsCalculatorStandaloneTest, ScaleIsAppliedIndependentlyOnEachAxis)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {}, {}, 10.0f, 20.0f, 0.0f, 2.0f, 3.0f);
    EXPECT_DOUBLE_EQ(corners.bottomRight.x(), 20.0);
    EXPECT_DOUBLE_EQ(corners.bottomRight.y(), 60.0);
}

TEST(LayerBoundsCalculatorStandaloneTest, NegativeScaleFlipsCornerCoordinates)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {5.0, 7.0}, {}, 10.0f, 4.0f, 0.0f, -2.0f, 1.0f);
    EXPECT_DOUBLE_EQ(corners.topRight.x(), -15.0);
    EXPECT_DOUBLE_EQ(corners.bottomRight.x(), -15.0);
    EXPECT_DOUBLE_EQ(corners.bottomRight.y(), 11.0);
}

TEST(LayerBoundsCalculatorStandaloneTest, ContainsWorksForNegativeScaleMirroring)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {5.0, 7.0}, {}, 10.0f, 4.0f, 0.0f, -2.0f, 1.0f);

    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {-5.0, 9.0}));
    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {-15.0, 11.0}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {6.0, 9.0}));
}

TEST(LayerBoundsCalculatorStandaloneTest, AabbForUnrotatedRectangleMatchesItsCorners)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {-5.0, 8.0}, {}, 12.0f, 6.0f, 0.0f);
    const QRectF bounds = LayerBoundsCalculator2D::calculateAABB(corners);
    EXPECT_DOUBLE_EQ(bounds.x(), -5.0);
    EXPECT_DOUBLE_EQ(bounds.y(), 8.0);
    EXPECT_DOUBLE_EQ(bounds.width(), 12.0);
    EXPECT_DOUBLE_EQ(bounds.height(), 6.0);
}

TEST(LayerBoundsCalculatorStandaloneTest, AabbContainsAllCornersAfterRotation)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {}, {}, 10.0f, 20.0f, 90.0f);
    const QRectF bounds = LayerBoundsCalculator2D::calculateAABB(corners);
    EXPECT_NEAR(bounds.left(), -20.0, 1e-5);
    EXPECT_NEAR(bounds.top(), 0.0, 1e-5);
    EXPECT_NEAR(bounds.right(), 0.0, 1e-5);
    EXPECT_NEAR(bounds.bottom(), 10.0, 1e-5);
}

TEST(LayerBoundsCalculatorStandaloneTest, AabbIsIndependentOfCornerOrder)
{
    const LayerCorners corners{
        QPointF(10.0, 10.0), QPointF(-2.0, 4.0),
        QPointF(3.0, 20.0), QPointF(8.0, -5.0)};
    const QRectF bounds = LayerBoundsCalculator2D::calculateAABB(corners);
    EXPECT_DOUBLE_EQ(bounds.left(), -2.0);
    EXPECT_DOUBLE_EQ(bounds.top(), -5.0);
    EXPECT_DOUBLE_EQ(bounds.right(), 10.0);
    EXPECT_DOUBLE_EQ(bounds.bottom(), 20.0);
}

TEST(LayerBoundsCalculatorStandaloneTest,
     CombinedAnchorRotationAndMirroredScaleProduceCornerDerivedAabb)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {13.0, -7.0}, {2.0, 3.0}, 11.0f, 7.0f, 37.0f, -1.5f, 0.4f);
    const std::array<QPointF, 4> points{
        corners.topLeft, corners.topRight, corners.bottomLeft, corners.bottomRight};

    double minX = points[0].x();
    double maxX = points[0].x();
    double minY = points[0].y();
    double maxY = points[0].y();
    for (const QPointF& point : points) {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }

    const QRectF bounds = LayerBoundsCalculator2D::calculateAABB(corners);
    EXPECT_NEAR(bounds.left(), minX, 1e-9);
    EXPECT_NEAR(bounds.right(), maxX, 1e-9);
    EXPECT_NEAR(bounds.top(), minY, 1e-9);
    EXPECT_NEAR(bounds.bottom(), maxY, 1e-9);
    for (const QPointF& point : points) {
        EXPECT_GE(point.x(), bounds.left() - 1e-9);
        EXPECT_LE(point.x(), bounds.right() + 1e-9);
        EXPECT_GE(point.y(), bounds.top() - 1e-9);
        EXPECT_LE(point.y(), bounds.bottom() + 1e-9);
    }
}

TEST(LayerBoundsCalculatorStandaloneTest, ContainsRecognizesInteriorAndRejectsExteriorPoints)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {}, {}, 10.0f, 8.0f, 0.0f);
    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {5.0, 4.0}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {11.0, 4.0}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {-1.0, 4.0}));
}

TEST(LayerBoundsCalculatorStandaloneTest, ContainsIncludesRectangleEdgesAndVertices)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {}, {}, 10.0f, 8.0f, 0.0f);
    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {0.0, 4.0}));
    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {10.0, 8.0}));
}

TEST(LayerBoundsCalculatorStandaloneTest, ContainsWorksForRotatedConvexRectangle)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {0.0, 0.0}, {5.0, 5.0}, 10.0f, 10.0f, 45.0f);
    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {0.0, 0.0}));
    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {0.0, 6.0}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {0.0, 8.0}));
}

TEST(LayerBoundsCalculatorStandaloneTest, ZeroSizeRectangleContainsItsCollapsedPoint)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {3.0, 4.0}, {}, 0.0f, 0.0f, 0.0f);
    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {3.0, 4.0}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {3.0, 4.01}));
}

TEST(LayerBoundsCalculatorStandaloneTest,
     ZeroWidthRectangleRejectsCollinearPointsOutsideItsSegment)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {3.0, 4.0}, {}, 0.0f, 10.0f, 0.0f);

    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {3.0, 4.0}));
    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {3.0, 9.0}));
    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, {3.0, 14.0}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {3.0, 3.99}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {3.0, 14.01}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {3.01, 9.0}));
}

TEST(LayerBoundsCalculatorStandaloneTest,
     RotatedZeroWidthRectangleRejectsPointsOutsideItsSegment)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {3.0, 4.0}, {}, 0.0f, 10.0f, 45.0f);
    const QPointF midpoint(
        (corners.topLeft.x() + corners.bottomLeft.x()) * 0.5,
        (corners.topLeft.y() + corners.bottomLeft.y()) * 0.5);
    const QPointF beyondEnd(
        corners.topLeft.x() + (corners.bottomLeft.x() - corners.topLeft.x()) * 2.0,
        corners.topLeft.y() + (corners.bottomLeft.y() - corners.topLeft.y()) * 2.0);

    EXPECT_TRUE(LayerBoundsCalculator2D::contains(corners, midpoint));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, beyondEnd));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(
        corners, {midpoint.x() + 0.01, midpoint.y() + 0.01}));
}

TEST(LayerBoundsCalculatorStandaloneTest, NegativeRotationProducesExpectedEquivalentOrientation)
{
    const auto positive = LayerBoundsCalculator2D::calculate({}, {}, 10.0f, 20.0f, 270.0f);
    const auto negative = LayerBoundsCalculator2D::calculate({}, {}, 10.0f, 20.0f, -90.0f);
    EXPECT_NEAR(positive.bottomRight.x(), negative.bottomRight.x(), 1e-5);
    EXPECT_NEAR(positive.bottomRight.y(), negative.bottomRight.y(), 1e-5);
}

TEST(LayerBoundsCalculatorStandaloneTest, ContainsRejectsNonFiniteQueryPoints)
{
    const auto corners = LayerBoundsCalculator2D::calculate(
        {0.0, 0.0}, {}, 10.0f, 10.0f, 0.0f);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {nan, 5.0}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {5.0, nan}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {infinity, 5.0}));
    EXPECT_FALSE(LayerBoundsCalculator2D::contains(corners, {5.0, -infinity}));
}
