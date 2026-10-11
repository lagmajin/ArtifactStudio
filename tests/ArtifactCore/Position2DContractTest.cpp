#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

import Core.Position2D;

using namespace ArtifactCore;

static_assert(std::is_copy_constructible_v<Position2D>);
static_assert(std::is_copy_assignable_v<Position2D>);
static_assert(std::is_nothrow_move_constructible_v<Position2D>);
static_assert(std::is_nothrow_move_assignable_v<Position2D>);
static_assert(std::is_copy_constructible_v<Position2DF>);
static_assert(std::is_copy_assignable_v<Position2DF>);
static_assert(std::is_nothrow_move_constructible_v<Position2DF>);
static_assert(std::is_nothrow_move_assignable_v<Position2DF>);

TEST(Position2DContractTest, IntegerPositionDefaultsToOrigin)
{
    Position2D position;
    int x = -1;
    int y = -1;
    position.Get(x, y);
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 0);
}

TEST(Position2DContractTest, IntegerPositionRoundTripsPositiveAndNegativeValues)
{
    Position2D position;
    position.Set(-1920, 1080);
    int x = 0;
    int y = 0;
    position.Get(x, y);
    EXPECT_EQ(x, -1920);
    EXPECT_EQ(y, 1080);
}

TEST(Position2DContractTest, IntegerPositionPreservesCoordinateLimits)
{
    Position2D position;
    position.Set(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    int x = 0;
    int y = 0;
    position.Get(x, y);
    EXPECT_EQ(x, std::numeric_limits<int>::min());
    EXPECT_EQ(y, std::numeric_limits<int>::max());
}

TEST(Position2DContractTest, IntegerPositionCopyConstructionIsIndependent)
{
    Position2D original;
    original.Set(12, 34);
    Position2D copy(original);
    original.Set(56, 78);
    int x = 0;
    int y = 0;
    copy.Get(x, y);
    EXPECT_EQ(x, 12);
    EXPECT_EQ(y, 34);
}

TEST(Position2DContractTest, IntegerPositionCopyAssignmentReplacesBothCoordinates)
{
    Position2D source;
    source.Set(17, -29);
    Position2D destination;
    destination.Set(1, 2);
    destination = source;
    source.Set(3, 4);
    int x = 0;
    int y = 0;
    destination.Get(x, y);
    EXPECT_EQ(x, 17);
    EXPECT_EQ(y, -29);
}

TEST(Position2DContractTest, IntegerPositionSelfCopyAssignmentPreservesValue)
{
    Position2D position;
    position.Set(23, 45);
    position = position;
    int x = 0;
    int y = 0;
    position.Get(x, y);
    EXPECT_EQ(x, 23);
    EXPECT_EQ(y, 45);
}

TEST(Position2DContractTest, IntegerPositionMoveConstructionTransfersValue)
{
    Position2D source;
    source.Set(-31, 47);
    Position2D destination(std::move(source));
    int x = 0;
    int y = 0;
    destination.Get(x, y);
    EXPECT_EQ(x, -31);
    EXPECT_EQ(y, 47);
}

TEST(Position2DContractTest, IntegerPositionMoveAssignmentReplacesDestination)
{
    Position2D source;
    source.Set(51, -63);
    Position2D destination;
    destination.Set(5, 6);
    destination = std::move(source);
    int x = 0;
    int y = 0;
    destination.Get(x, y);
    EXPECT_EQ(x, 51);
    EXPECT_EQ(y, -63);
}

TEST(Position2DContractTest, IntegerPositionSelfMoveAssignmentPreservesValue)
{
    Position2D position;
    position.Set(71, 83);
    position = std::move(position);
    int x = 0;
    int y = 0;
    position.Get(x, y);
    EXPECT_EQ(x, 71);
    EXPECT_EQ(y, 83);
}

TEST(Position2DContractTest, FloatPositionDefaultsToOrigin)
{
    Position2DF position;
    float x = -1.0f;
    float y = -1.0f;
    position.Get(x, y);
    EXPECT_FLOAT_EQ(x, 0.0f);
    EXPECT_FLOAT_EQ(y, 0.0f);
}

TEST(Position2DContractTest, FloatPositionPreservesFractionalCoordinates)
{
    Position2DF position;
    position.Set(-0.125f, 3.75f);
    float x = 0.0f;
    float y = 0.0f;
    position.Get(x, y);
    EXPECT_FLOAT_EQ(x, -0.125f);
    EXPECT_FLOAT_EQ(y, 3.75f);
}

TEST(Position2DContractTest, FloatPositionPreservesInfinityAndNan)
{
    Position2DF position;
    position.Set(std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN());
    float x = 0.0f;
    float y = 0.0f;
    position.Get(x, y);
    EXPECT_TRUE(std::isinf(x));
    EXPECT_GT(x, 0.0f);
    EXPECT_TRUE(std::isnan(y));
}

TEST(Position2DContractTest, FloatPositionPreservesNegativeZeroAndSubnormal)
{
    Position2DF position;
    const float smallestPositive = std::numeric_limits<float>::denorm_min();
    position.Set(-0.0f, smallestPositive);

    float x = 1.0f;
    float y = 0.0f;
    position.Get(x, y);
    EXPECT_FLOAT_EQ(x, 0.0f);
    EXPECT_TRUE(std::signbit(x));
    EXPECT_FLOAT_EQ(y, smallestPositive);
    EXPECT_GT(y, 0.0f);
}

TEST(Position2DContractTest, FloatPositionCopyAndMoveRemainIndependent)
{
    Position2DF original;
    original.Set(1.25f, -2.5f);
    Position2DF copy(original);
    original.Set(9.0f, 8.0f);
    Position2DF moved(std::move(copy));
    float x = 0.0f;
    float y = 0.0f;
    moved.Get(x, y);
    EXPECT_FLOAT_EQ(x, 1.25f);
    EXPECT_FLOAT_EQ(y, -2.5f);
}

TEST(Position2DContractTest, FloatPositionCopyAssignmentIsIndependent)
{
    Position2DF source;
    source.Set(4.5f, 6.25f);
    Position2DF destination;
    destination = source;
    source.Set(0.0f, 0.0f);
    float x = 0.0f;
    float y = 0.0f;
    destination.Get(x, y);
    EXPECT_FLOAT_EQ(x, 4.5f);
    EXPECT_FLOAT_EQ(y, 6.25f);
}

TEST(Position2DContractTest, FloatPositionSelfCopyAssignmentPreservesValue)
{
    Position2DF position;
    position.Set(-3.5f, 7.25f);
    position = position;
    float x = 0.0f;
    float y = 0.0f;
    position.Get(x, y);
    EXPECT_FLOAT_EQ(x, -3.5f);
    EXPECT_FLOAT_EQ(y, 7.25f);
}

TEST(Position2DContractTest, FloatPositionMoveAssignmentReplacesCoordinates)
{
    Position2DF source;
    source.Set(-8.5f, 12.75f);
    Position2DF destination;
    destination.Set(2.0f, 3.0f);
    destination = std::move(source);
    float x = 0.0f;
    float y = 0.0f;
    destination.Get(x, y);
    EXPECT_FLOAT_EQ(x, -8.5f);
    EXPECT_FLOAT_EQ(y, 12.75f);
}

TEST(Position2DContractTest, FloatPositionSelfMoveAssignmentPreservesValue)
{
    Position2DF position;
    position.Set(2.75f, -6.5f);
    position = std::move(position);
    float x = 0.0f;
    float y = 0.0f;
    position.Get(x, y);
    EXPECT_FLOAT_EQ(x, 2.75f);
    EXPECT_FLOAT_EQ(y, -6.5f);
}
