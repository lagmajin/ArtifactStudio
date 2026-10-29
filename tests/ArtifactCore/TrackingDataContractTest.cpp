#include <gtest/gtest.h>

#include <QPointF>
#include <QRectF>

#include <array>
#include <limits>
#include <vector>

import Tracking.MotionTracker;

using namespace ArtifactCore;

namespace {

TrackPoint point(int id, double x, double y, double confidence = 1.0,
                 bool active = true)
{
    TrackPoint value(id, QPointF(x, y));
    value.confidence = confidence;
    value.active = active;
    return value;
}

} // namespace

TEST(TrackingDataContractTest, PointLookupSortingAndHomographyProjectionAreConsistent)
{
    TrackFrame frame;
    frame.points = {point(9, 9.0, 1.0), point(2, 2.0, 3.0)};
    ASSERT_NE(frame.findPoint(9), nullptr);
    EXPECT_EQ(frame.findPoint(2)->position, QPointF(2.0, 3.0));
    EXPECT_EQ(frame.findPoint(100), nullptr);
    frame.sortPointsById();
    ASSERT_EQ(frame.points.size(), 2u);
    EXPECT_EQ(frame.points[0].id, 2);
    EXPECT_EQ(frame.points[1].id, 9);

    frame.hasHomography = true;
    frame.homography = {2.0, 0.0, 3.0,
                        0.0, 2.0, -1.0,
                        0.0, 0.0, 1.0};
    QPointF projected;
    ASSERT_TRUE(frame.projectPoint(QPointF(4.0, 5.0), projected));
    EXPECT_EQ(projected, QPointF(11.0, 9.0));

    std::array<QPointF, 4> corners;
    ASSERT_TRUE(frame.projectRect(QRectF(1.0, 2.0, 3.0, 4.0), corners));
    EXPECT_EQ(corners[0], QPointF(5.0, 3.0));
    EXPECT_EQ(corners[2], QPointF(11.0, 11.0));
}

TEST(TrackingDataContractTest, FailedProjectionLeavesCallerOutputUntouched)
{
    TrackFrame frame;
    frame.hasHomography = true;
    frame.homography = {1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        1.0, 0.0, 0.0};
    QPointF projected(17.0, 19.0);
    EXPECT_FALSE(frame.projectPoint(QPointF(0.0, 1.0), projected));
    EXPECT_EQ(projected, QPointF(17.0, 19.0));

    std::array<QPointF, 4> corners;
    corners.fill(QPointF(23.0, 29.0));
    EXPECT_FALSE(frame.projectRect(QRectF(-1.0, 0.0, 1.0, 1.0), corners));
    for (const QPointF& corner : corners) EXPECT_EQ(corner, QPointF(23.0, 29.0));
}

TEST(TrackingDataContractTest, InterpolationUsesSharedIdsAndClampsAtTimeBounds)
{
    TrackResult result;
    TrackFrame first;
    first.time = 1.0;
    first.points = {point(3, 0.0, 2.0, 0.2), point(8, 5.0, 5.0, 0.8)};
    first.overallConfidence = 0.25;
    TrackFrame last;
    last.time = 3.0;
    last.points = {point(3, 10.0, 6.0, 0.6), point(11, 20.0, 20.0)};
    last.overallConfidence = 0.75;
    result.frames = {first, last};

    const TrackFrame middle = result.interpolateAt(2.0);
    EXPECT_DOUBLE_EQ(middle.time, 2.0);
    EXPECT_DOUBLE_EQ(middle.overallConfidence, 0.5);
    ASSERT_EQ(middle.points.size(), 1u);
    EXPECT_EQ(middle.points.front().id, 3);
    EXPECT_EQ(middle.points.front().position, QPointF(5.0, 4.0));
    EXPECT_DOUBLE_EQ(middle.points.front().confidence, 0.4);
    EXPECT_EQ(result.interpolateAt(0.0).time, 1.0);
    EXPECT_EQ(result.interpolateAt(5.0).time, 3.0);
    EXPECT_EQ(result.interpolateAt(std::numeric_limits<double>::quiet_NaN()).points.size(), 0u);
}

TEST(TrackingDataContractTest, NormalizeOrdersAndSanitizesFramesAndFailureTimes)
{
    TrackResult result;
    TrackFrame later;
    later.time = 2.0;
    later.overallConfidence = std::numeric_limits<double>::infinity();
    later.hasHomography = true;
    later.homography[0] = std::numeric_limits<double>::quiet_NaN();
    later.points = {
        point(9, 9.0, 9.0, 2.0),
        point(2, std::numeric_limits<double>::infinity(), 1.0),
    };
    TrackFrame earlier;
    earlier.time = 1.0;
    earlier.points = {point(4, 4.0, 4.0, -1.0)};
    TrackFrame invalidTime;
    invalidTime.time = std::numeric_limits<double>::quiet_NaN();
    result.frames = {later, invalidTime, earlier};
    result.failureFrames = {2.0, std::numeric_limits<double>::infinity(),
                            1.0, 2.0 + 1.0e-10};

    result.normalize();

    ASSERT_EQ(result.frames.size(), 2u);
    EXPECT_DOUBLE_EQ(result.startTime, 1.0);
    EXPECT_DOUBLE_EQ(result.endTime, 2.0);
    EXPECT_TRUE(result.isValid);
    ASSERT_EQ(result.frames[0].points.size(), 1u);
    EXPECT_DOUBLE_EQ(result.frames[0].points[0].confidence, 0.0);
    ASSERT_EQ(result.frames[1].points.size(), 2u);
    EXPECT_EQ(result.frames[1].points[0].id, 2);
    EXPECT_FALSE(result.frames[1].points[0].active);
    EXPECT_DOUBLE_EQ(result.frames[1].points[0].confidence, 0.0);
    EXPECT_DOUBLE_EQ(result.frames[1].points[1].confidence, 1.0);
    EXPECT_DOUBLE_EQ(result.frames[1].overallConfidence, 0.0);
    EXPECT_FALSE(result.frames[1].hasHomography);
    EXPECT_EQ(result.failureFrames, (std::vector<double>{1.0, 2.0}));
}

TEST(TrackingDataContractTest, SetFrameReplacesNearTimestampAndMotionPathFillsOutput)
{
    TrackResult result;
    TrackFrame first;
    first.time = 1.0;
    first.points = {point(5, 1.0, 2.0)};
    result.setFrame(first);
    TrackFrame replacement;
    replacement.time = 1.0 + 1.0e-10;
    replacement.points = {point(5, 7.0, 8.0)};
    result.setFrame(replacement);

    EXPECT_EQ(result.frameCount(), 1u);
    EXPECT_EQ(result.frames.front().points.front().position, QPointF(7.0, 8.0));
    TrackFrame next;
    next.time = 2.0;
    next.points = {point(5, 9.0, 10.0)};
    result.setFrame(next);

    std::vector<QPointF> path{QPointF(-1.0, -1.0)};
    EXPECT_TRUE(result.motionPath(5, path));
    EXPECT_EQ(path, (std::vector<QPointF>{QPointF(7.0, 8.0), QPointF(9.0, 10.0)}));
    EXPECT_FALSE(result.motionPath(99, path));
    EXPECT_TRUE(path.empty());
}
