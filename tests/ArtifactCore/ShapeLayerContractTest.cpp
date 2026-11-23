#include <gtest/gtest.h>

#include <QPointF>
#include <QRectF>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>

import Shape.Layer;

namespace {

using ArtifactCore::PathShape;
using ArtifactCore::ShapeLayer;
using ArtifactCore::ShapePath;

ShapeLayer makeRectangleLayer()
{
    return ShapeLayer::createRectangle(QRectF(10.0, 20.0, 30.0, 40.0));
}

void expectRect(const QRectF& actual, const QRectF& expected)
{
    EXPECT_DOUBLE_EQ(actual.x(), expected.x());
    EXPECT_DOUBLE_EQ(actual.y(), expected.y());
    EXPECT_DOUBLE_EQ(actual.width(), expected.width());
    EXPECT_DOUBLE_EQ(actual.height(), expected.height());
}

void expectRectNear(const QRectF& actual, const QRectF& expected,
                    double tolerance = 1.0e-8)
{
    EXPECT_NEAR(actual.x(), expected.x(), tolerance);
    EXPECT_NEAR(actual.y(), expected.y(), tolerance);
    EXPECT_NEAR(actual.width(), expected.width(), tolerance);
    EXPECT_NEAR(actual.height(), expected.height(), tolerance);
}

// PathShape::boundingRect() grows by half the stroke width (default 1.0) so the
// outline is inside the reported bounds.
QRectF paddedBounds(const QRectF& geometry)
{
    constexpr double kStrokePadding = 0.5;
    return geometry.adjusted(-kStrokePadding, -kStrokePadding, kStrokePadding,
                             kStrokePadding);
}

QPointF mapShapePoint(const QPointF& point, const QPointF& anchor,
                      const QPointF& position, double scaleX, double scaleY,
                      double rotationDegrees)
{
    constexpr double kPi = 3.14159265358979323846;
    const double radians = rotationDegrees * kPi / 180.0;
    const double scaledX = (point.x() - anchor.x()) * scaleX;
    const double scaledY = (point.y() - anchor.y()) * scaleY;
    return QPointF(position.x() + scaledX * std::cos(radians) -
                       scaledY * std::sin(radians),
                   position.y() + scaledX * std::sin(radians) +
                       scaledY * std::cos(radians));
}

QRectF mapRectByCorners(const QRectF& rect, const QPointF& anchor,
                        const QPointF& position, double scaleX, double scaleY,
                        double rotationDegrees)
{
    const std::array<QPointF, 4> corners = {
        rect.topLeft(), rect.topRight(), rect.bottomLeft(), rect.bottomRight(),
    };
    double left = std::numeric_limits<double>::infinity();
    double top = std::numeric_limits<double>::infinity();
    double right = -std::numeric_limits<double>::infinity();
    double bottom = -std::numeric_limits<double>::infinity();
    for (const QPointF& corner : corners) {
        const QPointF mapped = mapShapePoint(corner, anchor, position, scaleX,
                                             scaleY, rotationDegrees);
        left = std::min(left, mapped.x());
        top = std::min(top, mapped.y());
        right = std::max(right, mapped.x());
        bottom = std::max(bottom, mapped.y());
    }
    return QRectF(left, top, right - left, bottom - top);
}

} // namespace

TEST(ShapeLayerContractTest, EmptyLayerHasNoGeometryAndCanBeClearedRepeatedly)
{
    ShapeLayer layer = ShapeLayer::createEmpty();

    EXPECT_EQ(layer.shapeCount(), 0);
    EXPECT_TRUE(layer.boundingRect().isNull());
    EXPECT_TRUE(layer.toPainterPath().isEmpty());
    EXPECT_FALSE(layer.contains(QPointF(0.0, 0.0)));

    layer.clearContent();
    layer.clearContent();
    EXPECT_EQ(layer.shapeCount(), 0);
}

TEST(ShapeLayerContractTest, PrimitiveFactoriesCreateExpectedBoundsAndHitRegions)
{
    ShapeLayer rectangle = makeRectangleLayer();
    ASSERT_EQ(rectangle.shapeCount(), 1);
    expectRect(rectangle.boundingRect(), paddedBounds(QRectF(10.0, 20.0, 30.0, 40.0)));
    EXPECT_TRUE(rectangle.contains(QPointF(25.0, 40.0)));
    EXPECT_FALSE(rectangle.contains(QPointF(5.0, 40.0)));

    ShapeLayer ellipse = ShapeLayer::createEllipse(QRectF(-8.0, -4.0, 16.0, 8.0));
    ASSERT_EQ(ellipse.shapeCount(), 1);
    EXPECT_TRUE(ellipse.contains(QPointF(0.0, 0.0)));
    EXPECT_FALSE(ellipse.contains(QPointF(7.0, 3.0)));

    ShapeLayer triangle = ShapeLayer::createTriangle(QRectF(0.0, 0.0, 20.0, 16.0));
    ASSERT_EQ(triangle.shapeCount(), 1);
    EXPECT_TRUE(triangle.contains(QPointF(10.0, 10.0)));
    EXPECT_FALSE(triangle.contains(QPointF(1.0, 1.0)));
}

TEST(ShapeLayerContractTest, LayerTransformUpdatesBoundsAndHitTestingTogether)
{
    ShapeLayer layer = makeRectangleLayer();
    layer.setPosition({5.0, -10.0});
    layer.setScale({2.0, 0.5});

    // Padded geometry (9.5, 19.5, 31, 41) scaled by (2, 0.5) then moved by (5, -10).
    expectRect(layer.boundingRect(), QRectF(24.0, -0.25, 62.0, 20.5));
    EXPECT_TRUE(layer.contains(QPointF(40.0, 10.0)));
    EXPECT_FALSE(layer.contains(QPointF(20.0, 10.0)));

    layer.setRotation(90.0);
    EXPECT_NEAR(layer.boundingRect().center().x(), -15.0, 1e-9);
    EXPECT_NEAR(layer.boundingRect().center().y(), 40.0, 1e-9);
    EXPECT_NEAR(layer.boundingRect().width(), 20.5, 1e-9);
    EXPECT_NEAR(layer.boundingRect().height(), 62.0, 1e-9);
    EXPECT_TRUE(layer.contains(QPointF(-15.0, 40.0)));
    EXPECT_FALSE(layer.contains(QPointF(10.0, 40.0)));
}

TEST(ShapeLayerContractTest, AnchoredBoundsAndHitTestingMatchIndependentTransformGrid)
{
    struct ScaleCase {
        double x;
        double y;
    };
    constexpr std::array<ScaleCase, 4> scales = {
        ScaleCase{1.0, 1.0}, ScaleCase{2.0, 0.5},
        ScaleCase{-1.0, 1.5}, ScaleCase{-0.75, -2.0},
    };
    constexpr std::array<double, 5> rotations = {
        -179.0, -45.0, 0.0, 37.0, 180.0,
    };
    const std::array<QPointF, 3> anchors = {
        QPointF(10.0, 20.0), QPointF(25.0, 40.0), QPointF(70.0, -5.0),
    };
    const QRectF geometry(10.0, 20.0, 30.0, 40.0);
    const QRectF padded = paddedBounds(geometry);
    const QPointF localInside(25.0, 40.0);
    const QPointF localOutside(100.0, 100.0);

    for (std::size_t anchorIndex = 0; anchorIndex < anchors.size(); ++anchorIndex) {
        const QPointF position(-17.0 + static_cast<double>(anchorIndex) * 51.0,
                               31.0 - static_cast<double>(anchorIndex) * 23.0);
        for (const ScaleCase& scale : scales) {
            for (const double rotation : rotations) {
                ShapeLayer layer = ShapeLayer::createRectangle(geometry);
                layer.setAnchorPoint({anchors[anchorIndex].x(),
                                      anchors[anchorIndex].y()});
                layer.setPosition({position.x(), position.y()});
                layer.setScale({scale.x, scale.y});
                layer.setRotation(rotation);

                const QRectF expectedBounds = mapRectByCorners(
                    padded, anchors[anchorIndex], position, scale.x, scale.y,
                    rotation);
                expectRectNear(layer.boundingRect(), expectedBounds);

                const QPointF mappedAnchor = mapShapePoint(
                    anchors[anchorIndex], anchors[anchorIndex], position,
                    scale.x, scale.y, rotation);
                EXPECT_NEAR(mappedAnchor.x(), position.x(), 1.0e-12);
                EXPECT_NEAR(mappedAnchor.y(), position.y(), 1.0e-12);

                const QPointF mappedInside = mapShapePoint(
                    localInside, anchors[anchorIndex], position, scale.x,
                    scale.y, rotation);
                const QPointF mappedOutside = mapShapePoint(
                    localOutside, anchors[anchorIndex], position, scale.x,
                    scale.y, rotation);
                EXPECT_TRUE(layer.contains(mappedInside));
                EXPECT_FALSE(layer.contains(mappedOutside));
            }
        }
    }
}

TEST(ShapeLayerContractTest, CopyConstructionAndCloneDeepCopyShapesAndAssignFreshIds)
{
    ShapeLayer source = makeRectangleLayer();
    source.setName(QStringLiteral("source"));
    const int sourceId = source.layerId();

    ShapeLayer copied(source);
    ShapeLayer cloned = source.clone();
    ASSERT_NE(copied.layerId(), sourceId);
    ASSERT_NE(cloned.layerId(), sourceId);
    EXPECT_EQ(copied.name(), QStringLiteral("source"));
    EXPECT_EQ(cloned.name(), QStringLiteral("source"));
    ASSERT_EQ(copied.shapeCount(), 1);
    ASSERT_EQ(cloned.shapeCount(), 1);

    auto* copiedPath = dynamic_cast<PathShape*>(copied.content()->childAt(0));
    auto* clonedPath = dynamic_cast<PathShape*>(cloned.content()->childAt(0));
    ASSERT_NE(copiedPath, nullptr);
    ASSERT_NE(clonedPath, nullptr);
    copiedPath->path().setRectangle(QRectF(100.0, 100.0, 2.0, 2.0));
    clonedPath->path().clear();

    expectRect(source.boundingRect(), paddedBounds(QRectF(10.0, 20.0, 30.0, 40.0)));
    expectRect(copied.boundingRect(), paddedBounds(QRectF(100.0, 100.0, 2.0, 2.0)));
    EXPECT_TRUE(cloned.boundingRect().isNull());
}

TEST(ShapeLayerContractTest, CopyAssignmentPreservesDestinationIdentityAndReplacesItsState)
{
    ShapeLayer source = makeRectangleLayer();
    source.setName(QStringLiteral("copied"));
    source.setVisible(false);

    ShapeLayer destination = ShapeLayer::createEllipse(QRectF(0.0, 0.0, 2.0, 2.0));
    const int destinationId = destination.layerId();
    destination = source;

    EXPECT_EQ(destination.layerId(), destinationId);
    EXPECT_EQ(destination.name(), QStringLiteral("copied"));
    EXPECT_FALSE(destination.isVisible());
    ASSERT_EQ(destination.shapeCount(), 1);
    expectRect(destination.boundingRect(), paddedBounds(QRectF(10.0, 20.0, 30.0, 40.0)));

    auto* destinationPath = dynamic_cast<PathShape*>(destination.content()->childAt(0));
    ASSERT_NE(destinationPath, nullptr);
    destinationPath->path().clear();
    expectRect(source.boundingRect(), paddedBounds(QRectF(10.0, 20.0, 30.0, 40.0)));
}

TEST(ShapeLayerContractTest, MoveConstructionAndAssignmentTransferOwnedGeometry)
{
    ShapeLayer source = makeRectangleLayer();
    source.setName(QStringLiteral("moving"));
    const int transferredId = source.layerId();

    ShapeLayer moved(std::move(source));
    EXPECT_EQ(moved.layerId(), transferredId);
    EXPECT_EQ(moved.name(), QStringLiteral("moving"));
    ASSERT_EQ(moved.shapeCount(), 1);
    expectRect(moved.boundingRect(), paddedBounds(QRectF(10.0, 20.0, 30.0, 40.0)));

    ShapeLayer target = ShapeLayer::createEmpty();
    target = std::move(moved);
    EXPECT_EQ(target.layerId(), transferredId);
    EXPECT_EQ(target.name(), QStringLiteral("moving"));
    ASSERT_EQ(target.shapeCount(), 1);
    expectRect(target.boundingRect(), paddedBounds(QRectF(10.0, 20.0, 30.0, 40.0)));
}

TEST(ShapeLayerContractTest, NullShapeInsertionsDoNotChangeContent)
{
    ShapeLayer layer = ShapeLayer::createEmpty();
    layer.addShape(nullptr);

    EXPECT_EQ(layer.shapeCount(), 0);
    EXPECT_TRUE(layer.boundingRect().isNull());
}

TEST(ShapePathContractTest, InterpolatesUnequalOpenLinesAtEquidistantCorrespondences)
{
    ShapePath from;
    from.moveTo(0.0, 2.0);
    from.lineTo(8.0, 2.0);
    ShapePath to;
    to.moveTo(10.0, 6.0);
    to.lineTo(26.0, 6.0);

    const ShapePath midpoint = ShapePath::interpolate(from, to, 0.5, 5);

    EXPECT_FALSE(midpoint.isClosed());
    EXPECT_EQ(midpoint.commandCount(), 5);
    EXPECT_NEAR(midpoint.length(), 12.0, 1e-8);
    const auto samples = midpoint.sampleEquidistant(5);
    ASSERT_EQ(samples.size(), 5u);
    const QPointF expected[] = {
        QPointF(5.0, 4.0), QPointF(8.0, 4.0), QPointF(11.0, 4.0),
        QPointF(14.0, 4.0), QPointF(17.0, 4.0),
    };
    for (std::size_t index = 0; index < samples.size(); ++index) {
        EXPECT_NEAR(samples[index].x(), expected[index].x(), 1e-8)
            << "sample=" << index;
        EXPECT_NEAR(samples[index].y(), expected[index].y(), 1e-8)
            << "sample=" << index;
    }
}

TEST(ShapePathContractTest,
     EquidistantSamplingCorrectsNonUniformQuadraticAndCubicParameterSpeed)
{
    ShapePath quadratic;
    quadratic.moveTo(0.0, 0.0);
    quadratic.quadTo(0.0, 0.0, 10.0, 0.0);
    ShapePath cubic;
    cubic.moveTo(0.0, 0.0);
    cubic.cubicTo(0.0, 0.0, 0.0, 0.0, 10.0, 0.0);

    const double expected[] = {0.0, 2.5, 5.0, 7.5, 10.0};
    const ShapePath* paths[] = {&quadratic, &cubic};
    for (const ShapePath* path : paths) {
        EXPECT_NEAR(path->length(), 10.0, 1.0e-8);
        const auto samples = path->sampleEquidistant(5);
        ASSERT_EQ(samples.size(), 5u);
        for (std::size_t index = 0; index < samples.size(); ++index) {
            EXPECT_NEAR(samples[index].x(), expected[index], 1.0e-8)
                << "sample=" << index;
            EXPECT_NEAR(samples[index].y(), 0.0, 1.0e-8)
                << "sample=" << index;
        }
    }
}

TEST(ShapePathContractTest, InterpolatesMultipleOpenSubpathsWithoutMergingContours)
{
    ShapePath from;
    from.moveTo(0.0, 0.0);
    from.lineTo(4.0, 0.0);
    from.moveTo(20.0, 0.0);
    from.lineTo(24.0, 0.0);
    ShapePath to;
    to.moveTo(2.0, 4.0);
    to.lineTo(6.0, 4.0);
    to.moveTo(22.0, 4.0);
    to.lineTo(26.0, 4.0);

    const ShapePath midpoint = ShapePath::interpolate(from, to, 0.5, 3);
    const auto contours = midpoint.subpaths();

    ASSERT_EQ(contours.size(), 2u);
    const QPointF expected[2][3] = {
        {QPointF(1.0, 2.0), QPointF(3.0, 2.0), QPointF(5.0, 2.0)},
        {QPointF(21.0, 2.0), QPointF(23.0, 2.0), QPointF(25.0, 2.0)},
    };
    for (std::size_t contourIndex = 0; contourIndex < contours.size(); ++contourIndex) {
        EXPECT_FALSE(contours[contourIndex].isClosed());
        const auto samples = contours[contourIndex].sampleEquidistant(3);
        ASSERT_EQ(samples.size(), 3u);
        for (std::size_t pointIndex = 0; pointIndex < samples.size(); ++pointIndex) {
            EXPECT_NEAR(samples[pointIndex].x(),
                        expected[contourIndex][pointIndex].x(), 1e-8)
                << "contour=" << contourIndex << " point=" << pointIndex;
            EXPECT_NEAR(samples[pointIndex].y(),
                        expected[contourIndex][pointIndex].y(), 1e-8)
                << "contour=" << contourIndex << " point=" << pointIndex;
        }
    }
}

TEST(ShapePathContractTest, MoveOnlyDegeneratePathKeepsItsLocationWhenSampledAndInterpolated)
{
    ShapePath pointPath;
    pointPath.moveTo(5.0, 7.0);

    const auto pointSamples = pointPath.sampleEquidistant(3);
    ASSERT_EQ(pointSamples.size(), 3u);
    for (const QPointF& sample : pointSamples) {
        EXPECT_EQ(sample, QPointF(5.0, 7.0));
    }

    ShapePath linePath;
    linePath.moveTo(1.0, 1.0);
    linePath.lineTo(9.0, 1.0);
    const ShapePath midpoint = ShapePath::interpolate(pointPath, linePath, 0.5, 3);
    const auto midpointSamples = midpoint.sampleEquidistant(3);
    const QPointF expected[] = {
        QPointF(3.0, 4.0), QPointF(5.0, 4.0), QPointF(7.0, 4.0),
    };
    ASSERT_EQ(midpointSamples.size(), 3u);
    for (std::size_t index = 0; index < midpointSamples.size(); ++index) {
        EXPECT_NEAR(midpointSamples[index].x(), expected[index].x(), 1e-8);
        EXPECT_NEAR(midpointSamples[index].y(), expected[index].y(), 1e-8);
    }
}
