#include <gtest/gtest.h>

#include <QPointF>
#include <QRectF>

#include <limits>
#include <vector>

import Geometry.LayerBounds;
import Geometry.LayerAlignment;

using namespace ArtifactCore;

namespace {
AlignmentObject object(int id, QRectF bounds)
{
    return {id, bounds, bounds.topLeft()};
}

LayoutCollisionObject collisionObject(int id, QRectF bounds, int priority,
                                       bool movable = true, bool scalable = false)
{
    LayoutCollisionObject value;
    value.id = id;
    value.bounds = bounds;
    value.currentPosition = bounds.topLeft();
    value.priority = priority;
    value.movable = movable;
    value.scalable = scalable;
    return value;
}
}

TEST(LayerAlignmentStandaloneTest, EmptyAlignmentInputIsUnchanged)
{
    std::vector<AlignmentObject> objects;
    LayerAlignment::align(objects, AlignType::Left, AlignmentTarget::Selection, QRectF(0, 0, 10, 10));
    EXPECT_TRUE(objects.empty());
}

TEST(LayerAlignmentStandaloneTest, SelectionAlignmentUsesUnionLeftEdge)
{
    std::vector<AlignmentObject> objects{
        object(1, QRectF(10, 5, 20, 10)),
        object(2, QRectF(50, 30, 10, 12))};
    LayerAlignment::align(objects, AlignType::Left, AlignmentTarget::Selection, QRectF());
    EXPECT_DOUBLE_EQ(objects[0].bounds.left(), 10.0);
    EXPECT_DOUBLE_EQ(objects[1].bounds.left(), 10.0);
    EXPECT_DOUBLE_EQ(objects[1].currentPosition.x(), 10.0);
}

TEST(LayerAlignmentStandaloneTest, CompositionAlignmentUsesContainerCenter)
{
    std::vector<AlignmentObject> objects{object(1, QRectF(0, 0, 20, 10))};
    LayerAlignment::align(objects, AlignType::CenterHorizontal,
        AlignmentTarget::Composition, QRectF(100, 0, 200, 100));
    EXPECT_DOUBLE_EQ(objects[0].bounds.center().x(), 200.0);
    EXPECT_DOUBLE_EQ(objects[0].currentPosition.x(), 190.0);
}

TEST(LayerAlignmentStandaloneTest, KeyObjectRemainsFixedWhileOtherObjectsAlignToIt)
{
    std::vector<AlignmentObject> objects{
        object(1, QRectF(40, 20, 20, 10)),
        object(2, QRectF(100, 50, 10, 10))};
    LayerAlignment::align(objects, AlignType::Bottom,
        AlignmentTarget::KeyObject, QRectF(), 1);
    EXPECT_DOUBLE_EQ(objects[0].bounds.bottom(), 30.0);
    EXPECT_DOUBLE_EQ(objects[1].bounds.bottom(), 30.0);
    EXPECT_DOUBLE_EQ(objects[0].currentPosition.y(), 20.0);
}

TEST(LayerAlignmentStandaloneTest, MissingKeyObjectFallsBackToSelectionAlignment)
{
    std::vector<AlignmentObject> objects{
        object(1, QRectF(5, 0, 10, 10)),
        object(2, QRectF(25, 0, 10, 10))};
    LayerAlignment::align(objects, AlignType::Right,
        AlignmentTarget::KeyObject, QRectF(), 99);
    EXPECT_DOUBLE_EQ(objects[0].bounds.right(), 35.0);
    EXPECT_DOUBLE_EQ(objects[1].bounds.right(), 35.0);
}

TEST(LayerAlignmentStandaloneTest, DistributeMovesOnlyInteriorObjectsAlongChosenAxis)
{
    std::vector<AlignmentObject> objects{
        object(1, QRectF(0, 0, 10, 10)),
        object(2, QRectF(25, 30, 10, 10)),
        object(3, QRectF(100, 70, 10, 10))};
    LayerAlignment::distribute(objects, DistributeType::CenterHorizontal);
    EXPECT_DOUBLE_EQ(objects[0].bounds.center().x(), 5.0);
    EXPECT_DOUBLE_EQ(objects[1].bounds.center().x(), 55.0);
    EXPECT_DOUBLE_EQ(objects[2].bounds.center().x(), 105.0);
    EXPECT_DOUBLE_EQ(objects[1].bounds.top(), 30.0);
}

TEST(LayerAlignmentStandaloneTest, DistributeWithFewerThanThreeObjectsIsUnchanged)
{
    std::vector<AlignmentObject> objects{
        object(1, QRectF(0, 0, 10, 10)), object(2, QRectF(40, 0, 10, 10))};
    LayerAlignment::distribute(objects, DistributeType::Left);
    EXPECT_DOUBLE_EQ(objects[0].bounds.left(), 0.0);
    EXPECT_DOUBLE_EQ(objects[1].bounds.left(), 40.0);
}

TEST(LayerAlignmentStandaloneTest, ExplicitSpacingUsesSortedObjectOrder)
{
    std::vector<AlignmentObject> objects{
        object(3, QRectF(50, 0, 10, 10)),
        object(1, QRectF(0, 0, 10, 10)),
        object(2, QRectF(25, 0, 20, 10))};
    LayerAlignment::distributeSpacing(objects, DistributeType::Left,
        AlignmentTarget::Selection, QRectF(), 5.0f);
    EXPECT_DOUBLE_EQ(objects[0].bounds.left(), 0.0);
    EXPECT_DOUBLE_EQ(objects[1].bounds.left(), 15.0);
    EXPECT_DOUBLE_EQ(objects[2].bounds.left(), 40.0);
}

TEST(LayerAlignmentStandaloneTest, CompositionSpacingFillsContainerWithEqualGaps)
{
    std::vector<AlignmentObject> objects{
        object(1, QRectF(20, 0, 10, 10)),
        object(2, QRectF(45, 0, 20, 10)),
        object(3, QRectF(80, 0, 10, 10))};
    LayerAlignment::distributeSpacing(objects, DistributeType::Left,
        AlignmentTarget::Composition, QRectF(0, 0, 100, 20));
    EXPECT_DOUBLE_EQ(objects[0].bounds.left(), 0.0);
    EXPECT_DOUBLE_EQ(objects[1].bounds.left(), 30.0);
    EXPECT_DOUBLE_EQ(objects[2].bounds.left(), 70.0);
}

TEST(LayerAlignmentStandaloneTest, CollisionAvoidanceMovesLowerPriorityObject)
{
    std::vector<LayoutCollisionObject> objects{
        collisionObject(1, QRectF(0, 0, 10, 10), 10),
        collisionObject(2, QRectF(5, 0, 10, 10), 0)};
    const auto result = LayerAlignment::resolveCollisions(objects, QRectF(-20, -20, 50, 50));
    EXPECT_EQ(result.movedCount, 1);
    EXPECT_EQ(result.unresolvedCount, 0);
    EXPECT_DOUBLE_EQ(objects[0].bounds.left(), 0.0);
    EXPECT_DOUBLE_EQ(objects[1].bounds.right(), 5.0);
}

TEST(LayerAlignmentStandaloneTest, CollisionAvoidanceKeepsObjectsInsideSafeArea)
{
    std::vector<LayoutCollisionObject> objects{
        collisionObject(1, QRectF(-5, 10, 10, 10), 1)};
    const auto result = LayerAlignment::resolveCollisions(objects, QRectF(0, 0, 100, 100));
    EXPECT_EQ(result.movedCount, 1);
    EXPECT_DOUBLE_EQ(objects[0].bounds.left(), 0.0);
    EXPECT_DOUBLE_EQ(objects[0].currentPosition.x(), 0.0);
}

TEST(LayerAlignmentStandaloneTest, CollisionAvoidanceScalesLowerPriorityWhenAllowed)
{
    std::vector<LayoutCollisionObject> objects{
        collisionObject(1, QRectF(0, 0, 10, 10), 10, false),
        collisionObject(2, QRectF(5, 0, 10, 10), 0, true, true)};
    const auto result = LayerAlignment::resolveCollisions(objects, QRectF(-20, -20, 50, 50));
    EXPECT_EQ(result.scaledCount, 1);
    EXPECT_DOUBLE_EQ(objects[1].scale, 0.5);
    EXPECT_DOUBLE_EQ(objects[1].bounds.width(), 5.0);
}

TEST(LayerAlignmentStandaloneTest, InvalidSafeAreaLeavesCollisionObjectsUnchanged)
{
    std::vector<LayoutCollisionObject> objects{
        collisionObject(1, QRectF(0, 0, 10, 10), 1)};
    const auto before = objects[0].bounds;
    const auto result = LayerAlignment::resolveCollisions(objects, QRectF());
    EXPECT_EQ(result.movedCount, 0);
    EXPECT_EQ(result.scaledCount, 0);
    EXPECT_EQ(objects[0].bounds, before);
}

TEST(LayerAlignmentStandaloneTest, InvalidScaleIsResetToOneBeforeResolution)
{
    std::vector<LayoutCollisionObject> objects{
        collisionObject(1, QRectF(5, 5, 10, 10), 1)};
    objects[0].scale = std::numeric_limits<double>::quiet_NaN();
    LayerAlignment::resolveCollisions(objects, QRectF(0, 0, 100, 100));
    EXPECT_DOUBLE_EQ(objects[0].scale, 1.0);
}
