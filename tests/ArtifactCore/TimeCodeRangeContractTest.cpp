#include <gtest/gtest.h>

#include <limits>
#include <utility>

#include <QString>

import Time.Code;
import TimeCodeRange;

using ArtifactCore::TimeCode;
using ArtifactCore::TimeCodeRange;

TEST(TimeCodeRangeContractTest, TimeCodeConstructorPreservesFrameEndpointsAndStartRate)
{
    const TimeCodeRange range(TimeCode(25, 24.0), TimeCode(49, 24.0));

    EXPECT_TRUE(range.isValid());
    EXPECT_EQ(range.startTimeCode().frame(), 25);
    EXPECT_EQ(range.stopTimeCode().frame(), 49);
    EXPECT_DOUBLE_EQ(range.fps(), 24.0);
    EXPECT_EQ(range.durationFrames(), 24);
    EXPECT_TRUE(range.containsFrame(25));
    EXPECT_FALSE(range.containsFrame(49));
}

TEST(TimeCodeContractTest, ParsingDropFrameSeparatorAppliesDropFrameNumbering)
{
    TimeCode timecode(0, 29.97);

    timecode.setFromQString(QStringLiteral("00:01:00;02"));

    EXPECT_TRUE(timecode.isDropFrame());
    EXPECT_EQ(timecode.frame(), 1800);
    EXPECT_EQ(timecode.toString(), QStringLiteral("00:01:00;02"));
}

TEST(TimeCodeRangeContractTest, UsesHalfOpenFrameMembershipAndPreservesRate)
{
    TimeCodeRange range;
    range.setByFrameRange(10, 20, 24.0);

    EXPECT_TRUE(range.isValid());
    EXPECT_EQ(range.startTimeCode().frame(), 10);
    EXPECT_EQ(range.stopTimeCode().frame(), 20);
    EXPECT_DOUBLE_EQ(range.fps(), 24.0);
    EXPECT_EQ(range.durationFrames(), 10);
    EXPECT_DOUBLE_EQ(range.durationSeconds(), 10.0 / 24.0);
    EXPECT_EQ(range.durationRational().value(), 10);
    EXPECT_EQ(range.durationRational().scale(), 24);
    EXPECT_TRUE(range.containsFrame(10));
    EXPECT_TRUE(range.containsFrame(19));
    EXPECT_FALSE(range.containsFrame(20));
    EXPECT_TRUE(range.contains(TimeCode(19, 24.0)));
    EXPECT_FALSE(range.contains(TimeCode(20, 24.0)));
}

TEST(TimeCodeRangeContractTest, IntersectionAndUnionHandleOverlapAdjacencyAndDisjointRanges)
{
    TimeCodeRange left;
    left.setByFrameRange(10, 20, 30.0);
    TimeCodeRange overlap;
    overlap.setByFrameRange(15, 25, 30.0);
    TimeCodeRange adjacent;
    adjacent.setByFrameRange(20, 30, 30.0);
    TimeCodeRange disjoint;
    disjoint.setByFrameRange(40, 50, 30.0);

    const auto intersection = left.intersection(overlap);
    EXPECT_EQ(intersection.startTimeCode().frame(), 15);
    EXPECT_EQ(intersection.stopTimeCode().frame(), 20);
    EXPECT_EQ(intersection.durationFrames(), 5);

    const auto adjacentIntersection = left.intersection(adjacent);
    EXPECT_EQ(adjacentIntersection.startTimeCode().frame(), 0);
    EXPECT_EQ(adjacentIntersection.stopTimeCode().frame(), 0);
    EXPECT_EQ(adjacentIntersection.durationFrames(), 0);

    const auto disjointIntersection = left.intersection(disjoint);
    EXPECT_EQ(disjointIntersection.durationFrames(), 0);

    const auto unionRange = left.united(disjoint);
    EXPECT_EQ(unionRange.startTimeCode().frame(), 10);
    EXPECT_EQ(unionRange.stopTimeCode().frame(), 50);
    EXPECT_EQ(unionRange.fps(), 30.0);
}

TEST(TimeCodeRangeContractTest, EmptyAndInvalidRangesDoNotOverlapValidRanges)
{
    TimeCodeRange valid;
    valid.setByFrameRange(0, 30, 30.0);
    TimeCodeRange empty;
    empty.setByFrameRange(12, 12, 30.0);
    TimeCodeRange reversed;
    reversed.setByFrameRange(20, 10, 30.0);

    EXPECT_TRUE(empty.isValid());
    EXPECT_EQ(empty.durationFrames(), 0);
    EXPECT_FALSE(empty.overlaps(valid));
    EXPECT_FALSE(valid.overlaps(empty));

    EXPECT_FALSE(reversed.isValid());
    EXPECT_FALSE(reversed.overlaps(valid));
    EXPECT_FALSE(valid.overlaps(reversed));
}

TEST(TimeCodeRangeContractTest, NonPositiveFrameRateFallsBackToThirtyFramesPerSecond)
{
    TimeCodeRange zeroRate;
    zeroRate.setByFrameRange(0, 15, 0.0);
    EXPECT_TRUE(zeroRate.isValid());
    EXPECT_DOUBLE_EQ(zeroRate.fps(), 30.0);
    EXPECT_DOUBLE_EQ(zeroRate.durationSeconds(), 0.5);

    TimeCodeRange negativeRate;
    negativeRate.setByFrameRange(0, 30, -24.0);
    EXPECT_DOUBLE_EQ(negativeRate.fps(), 30.0);
    EXPECT_DOUBLE_EQ(negativeRate.durationSeconds(), 1.0);
}

TEST(TimeCodeRangeContractTest, NonFiniteFrameRateFallsBackToThirtyFramesPerSecond)
{
    const double nonFiniteRates[] = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };

    for (const double rate : nonFiniteRates) {
        TimeCodeRange range;
        range.setByFrameRange(0, 60, rate);

        EXPECT_DOUBLE_EQ(range.fps(), 30.0);
        EXPECT_TRUE(range.isValid());
        EXPECT_DOUBLE_EQ(range.durationSeconds(), 2.0);
        const auto rationalDuration = range.durationRational();
        EXPECT_EQ(rationalDuration.value(), 60);
        EXPECT_EQ(rationalDuration.scale(), 30);
    }
}

TEST(TimeCodeRangeContractTest, TrimAndOffsetClampWithoutReversingEndpoints)
{
    TimeCodeRange range;
    range.setByFrameRange(10, 20, 25.0);
    range.offset(-5);
    EXPECT_EQ(range.startTimeCode().frame(), 5);
    EXPECT_EQ(range.stopTimeCode().frame(), 15);

    range.trimStart(20);
    EXPECT_EQ(range.startTimeCode().frame(), 15);
    EXPECT_EQ(range.stopTimeCode().frame(), 15);
    EXPECT_TRUE(range.isValid());

    range.setByFrameRange(10, 20, 25.0);
    range.trimEnd(-20);
    EXPECT_EQ(range.startTimeCode().frame(), 10);
    EXPECT_EQ(range.stopTimeCode().frame(), 10);
    EXPECT_TRUE(range.isValid());
}

TEST(TimeCodeRangeContractTest, CopyAndMoveOperationsKeepIndependentValues)
{
    TimeCodeRange original;
    original.setByFrameRange(-12, 48, 24.0);

    TimeCodeRange copied(original);
    copied.offset(3);
    EXPECT_EQ(original.startTimeCode().frame(), -12);
    EXPECT_EQ(original.stopTimeCode().frame(), 48);
    EXPECT_EQ(copied.startTimeCode().frame(), -9);
    EXPECT_EQ(copied.stopTimeCode().frame(), 51);

    TimeCodeRange assigned;
    assigned = original;
    assigned.trimEnd(-10);
    EXPECT_EQ(original.stopTimeCode().frame(), 48);
    EXPECT_EQ(assigned.stopTimeCode().frame(), 38);

    TimeCodeRange moved(std::move(copied));
    EXPECT_EQ(moved.startTimeCode().frame(), -9);
    EXPECT_EQ(moved.stopTimeCode().frame(), 51);

    TimeCodeRange moveAssigned;
    moveAssigned = std::move(assigned);
    EXPECT_EQ(moveAssigned.startTimeCode().frame(), -12);
    EXPECT_EQ(moveAssigned.stopTimeCode().frame(), -12);
}
