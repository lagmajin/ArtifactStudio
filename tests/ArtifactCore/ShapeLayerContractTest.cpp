#include <gtest/gtest.h>

#include <QPointF>
#include <QRectF>

#include <memory>
#include <utility>

import Shape.Layer;

namespace {

using ArtifactCore::PathShape;
using ArtifactCore::ShapeLayer;

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
    expectRect(rectangle.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
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

    expectRect(layer.boundingRect(), QRectF(25.0, 0.0, 60.0, 20.0));
    EXPECT_TRUE(layer.contains(QPointF(40.0, 10.0)));
    EXPECT_FALSE(layer.contains(QPointF(20.0, 10.0)));

    layer.setRotation(90.0);
    EXPECT_NEAR(layer.boundingRect().center().x(), -15.0, 1e-9);
    EXPECT_NEAR(layer.boundingRect().center().y(), 40.0, 1e-9);
    EXPECT_NEAR(layer.boundingRect().width(), 20.0, 1e-9);
    EXPECT_NEAR(layer.boundingRect().height(), 60.0, 1e-9);
    EXPECT_TRUE(layer.contains(QPointF(-15.0, 40.0)));
    EXPECT_FALSE(layer.contains(QPointF(10.0, 40.0)));
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

    expectRect(source.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
    expectRect(copied.boundingRect(), QRectF(100.0, 100.0, 2.0, 2.0));
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
    expectRect(destination.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));

    auto* destinationPath = dynamic_cast<PathShape*>(destination.content()->childAt(0));
    ASSERT_NE(destinationPath, nullptr);
    destinationPath->path().clear();
    expectRect(source.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
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
    expectRect(moved.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));

    ShapeLayer target = ShapeLayer::createEmpty();
    target = std::move(moved);
    EXPECT_EQ(target.layerId(), transferredId);
    EXPECT_EQ(target.name(), QStringLiteral("moving"));
    ASSERT_EQ(target.shapeCount(), 1);
    expectRect(target.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
}

TEST(ShapeLayerContractTest, NullShapeInsertionsDoNotChangeContent)
{
    ShapeLayer layer = ShapeLayer::createEmpty();
    layer.addShape(nullptr);

    EXPECT_EQ(layer.shapeCount(), 0);
    EXPECT_TRUE(layer.boundingRect().isNull());
}
