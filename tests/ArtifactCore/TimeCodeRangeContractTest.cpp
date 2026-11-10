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

TEST(TimeCodeRangeContractTest,
     ConstructorKeepsFrameEndpointsAndUsesStartRateWhenEndpointRatesDiffer)
{
    const TimeCodeRange range(TimeCode(100, 24.0), TimeCode(150, 30.0));

    ASSERT_TRUE(range.isValid());
    EXPECT_EQ(range.startTimeCode().frame(), 100);
    EXPECT_EQ(range.stopTimeCode().frame(), 150);
    EXPECT_DOUBLE_EQ(range.fps(), 24.0);
    EXPECT_EQ(range.durationFrames(), 50);
    EXPECT_DOUBLE_EQ(range.durationSeconds(), 50.0 / 24.0);
    EXPECT_EQ(range.startTimeCode().fps(), 24.0);
    EXPECT_EQ(range.stopTimeCode().fps(), 24.0)
        << "the range exposes both endpoints in its single selected frame rate";
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

TEST(TimeCodeRangeContractTest,
     ContainsTimeCodeComparesElapsedTimeAcrossDifferentFrameRates)
{
    TimeCodeRange oneToTwoSeconds;
    oneToTwoSeconds.setByFrameRange(24, 48, 24.0);

    EXPECT_FALSE(oneToTwoSeconds.contains(TimeCode(27, 30.0)))
        << "frame 27 at 30 fps is 0.9 seconds, before the range";
    EXPECT_TRUE(oneToTwoSeconds.contains(TimeCode(45, 30.0)))
        << "frame 45 at 30 fps is 1.5 seconds, inside the range";
    EXPECT_TRUE(oneToTwoSeconds.contains(TimeCode(50, 30.0)))
        << "frame 50 at 30 fps is about 1.67 seconds, inside despite its raw frame index";
    EXPECT_FALSE(oneToTwoSeconds.contains(TimeCode(63, 30.0)))
        << "frame 63 at 30 fps is 2.1 seconds, after the range";
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

TEST(TimeCodeRangeContractTest,
     IntersectionAndUnionCompareIntervalsAcrossDifferentFrameRates)
{
    TimeCodeRange at24Fps;
    at24Fps.setByFrameRange(24, 60, 24.0); // [1.0s, 2.5s)
    TimeCodeRange at30Fps;
    at30Fps.setByFrameRange(45, 90, 30.0); // [1.5s, 3.0s)

    const auto intersection = at24Fps.intersection(at30Fps);
    ASSERT_TRUE(intersection.isValid());
    EXPECT_DOUBLE_EQ(intersection.durationSeconds(), 1.0);
    EXPECT_TRUE(intersection.contains(TimeCode(45, 30.0)));
    EXPECT_TRUE(intersection.contains(TimeCode(74, 30.0)));
    EXPECT_FALSE(intersection.contains(TimeCode(75, 30.0)));

    const auto reverseIntersection = at30Fps.intersection(at24Fps);
    ASSERT_TRUE(reverseIntersection.isValid());
    EXPECT_DOUBLE_EQ(reverseIntersection.durationSeconds(),
                     intersection.durationSeconds());
    EXPECT_TRUE(reverseIntersection.contains(TimeCode(45, 30.0)));
    EXPECT_FALSE(reverseIntersection.contains(TimeCode(75, 30.0)));

    const auto united = at24Fps.united(at30Fps);
    ASSERT_TRUE(united.isValid());
    EXPECT_DOUBLE_EQ(united.durationSeconds(), 2.0);
    EXPECT_TRUE(united.contains(TimeCode(24, 24.0)));
    EXPECT_TRUE(united.contains(TimeCode(89, 30.0)));
    EXPECT_FALSE(united.contains(TimeCode(90, 30.0)));

    const auto reverseUnion = at30Fps.united(at24Fps);
    ASSERT_TRUE(reverseUnion.isValid());
    EXPECT_DOUBLE_EQ(reverseUnion.durationSeconds(), united.durationSeconds());
    EXPECT_TRUE(reverseUnion.contains(TimeCode(24, 24.0)));
    EXPECT_FALSE(reverseUnion.contains(TimeCode(90, 30.0)));
}

TEST(TimeCodeRangeContractTest,
     IntersectionAndUnionCompareIntervalsAcrossDifferentFrameRates)
{
    TimeCodeRange at24Fps;
    at24Fps.setByFrameRange(24, 60, 24.0); // [1.0s, 2.5s)
    TimeCodeRange at30Fps;
    at30Fps.setByFrameRange(45, 90, 30.0); // [1.5s, 3.0s)

    const auto intersection = at24Fps.intersection(at30Fps);
    ASSERT_TRUE(intersection.isValid());
    EXPECT_DOUBLE_EQ(intersection.durationSeconds(), 1.0);
    EXPECT_TRUE(intersection.contains(TimeCode(45, 30.0)));
    EXPECT_TRUE(intersection.contains(TimeCode(74, 30.0)));
    EXPECT_FALSE(intersection.contains(TimeCode(75, 30.0)));

    const auto reverseIntersection = at30Fps.intersection(at24Fps);
    ASSERT_TRUE(reverseIntersection.isValid());
    EXPECT_DOUBLE_EQ(reverseIntersection.durationSeconds(),
                     intersection.durationSeconds());
    EXPECT_TRUE(reverseIntersection.contains(TimeCode(45, 30.0)));
    EXPECT_FALSE(reverseIntersection.contains(TimeCode(75, 30.0)));

    const auto united = at24Fps.united(at30Fps);
    ASSERT_TRUE(united.isValid());
    EXPECT_DOUBLE_EQ(united.durationSeconds(), 2.0);
    EXPECT_TRUE(united.contains(TimeCode(24, 24.0)));
    EXPECT_TRUE(united.contains(TimeCode(89, 30.0)));
    EXPECT_FALSE(united.contains(TimeCode(90, 30.0)));

    const auto reverseUnion = at30Fps.united(at24Fps);
    ASSERT_TRUE(reverseUnion.isValid());
    EXPECT_DOUBLE_EQ(reverseUnion.durationSeconds(), united.durationSeconds());
    EXPECT_TRUE(reverseUnion.contains(TimeCode(24, 24.0)));
    EXPECT_FALSE(reverseUnion.contains(TimeCode(90, 30.0)));
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

TEST(TimeCodeRangeContractTest,
     RationalDurationPreservesFractionalFrameRateSemantics)
{
    TimeCodeRange range;
    range.setByFrameRange(0, 300, 29.97);

    const auto rationalDuration = range.durationRational();
    EXPECT_NEAR(rationalDuration.toSeconds(), range.durationSeconds(), 1e-6)
        << "rational duration must not truncate a fractional frame rate";
}

TEST(TimeCodeRangeContractTest, IntervalOperationsMatchAnIndependentFrameSetOracle)
{
    // Exhaustively compare every small valid half-open interval pair against
    // frame membership. This catches endpoint and adjacency errors that a few
    // hand-picked overlaps can miss.
    for (int leftStart = -3; leftStart <= 3; ++leftStart) {
        for (int leftStop = leftStart; leftStop <= 4; ++leftStop) {
            TimeCodeRange left;
            left.setByFrameRange(leftStart, leftStop, 24.0);

            for (int rightStart = -3; rightStart <= 3; ++rightStart) {
                for (int rightStop = rightStart; rightStop <= 4; ++rightStop) {
                    TimeCodeRange right;
                    right.setByFrameRange(rightStart, rightStop, 24.0);

                    bool oracleOverlaps = false;
                    for (int frame = -4; frame <= 5; ++frame) {
                        oracleOverlaps = oracleOverlaps ||
                            (left.containsFrame(frame) && right.containsFrame(frame));
                    }
                    EXPECT_EQ(left.overlaps(right), oracleOverlaps)
                        << "left=[" << leftStart << "," << leftStop << ") right=["
                        << rightStart << "," << rightStop << ")";

                    const auto intersection = left.intersection(right);
                    if (oracleOverlaps) {
                        const int expectedStart = (leftStart > rightStart)
                            ? leftStart : rightStart;
                        const int expectedStop = (leftStop < rightStop)
                            ? leftStop : rightStop;
                        EXPECT_EQ(intersection.startTimeCode().frame(), expectedStart);
                        EXPECT_EQ(intersection.stopTimeCode().frame(), expectedStop);
                    } else {
                        EXPECT_EQ(intersection.durationFrames(), 0);
                    }

                    const auto hull = left.united(right);
                    const int expectedHullStart = (leftStart < rightStart)
                        ? leftStart : rightStart;
                    const int expectedHullStop = (leftStop > rightStop)
                        ? leftStop : rightStop;
                    EXPECT_EQ(hull.startTimeCode().frame(), expectedHullStart);
                    EXPECT_EQ(hull.stopTimeCode().frame(), expectedHullStop);
                    EXPECT_DOUBLE_EQ(hull.fps(), 24.0);
                }
            }
        }
    }
}

TEST(TimeCodeRangeContractTest, SmallIntervalsRemainExactNearIntegerFrameLimits)
{
    const int minFrame = std::numeric_limits<int>::min();
    const int maxFrame = std::numeric_limits<int>::max();

    TimeCodeRange nearMinimum;
    nearMinimum.setByFrameRange(minFrame + 1, minFrame + 10, 24.0);
    TimeCodeRange minimumOverlap;
    minimumOverlap.setByFrameRange(minFrame + 5, minFrame + 15, 24.0);
    const auto minimumIntersection = nearMinimum.intersection(minimumOverlap);
    const auto minimumUnion = nearMinimum.united(minimumOverlap);
    EXPECT_EQ(minimumIntersection.startTimeCode().frame(), minFrame + 5);
    EXPECT_EQ(minimumIntersection.stopTimeCode().frame(), minFrame + 10);
    EXPECT_EQ(minimumIntersection.durationFrames(), 5);
    EXPECT_EQ(minimumUnion.startTimeCode().frame(), minFrame + 1);
    EXPECT_EQ(minimumUnion.stopTimeCode().frame(), minFrame + 15);

    TimeCodeRange nearMaximum;
    nearMaximum.setByFrameRange(maxFrame - 10, maxFrame - 1, 24.0);
    TimeCodeRange maximumOverlap;
    maximumOverlap.setByFrameRange(maxFrame - 5, maxFrame, 24.0);
    const auto maximumIntersection = nearMaximum.intersection(maximumOverlap);
    const auto maximumUnion = nearMaximum.united(maximumOverlap);
    EXPECT_EQ(maximumIntersection.startTimeCode().frame(), maxFrame - 5);
    EXPECT_EQ(maximumIntersection.stopTimeCode().frame(), maxFrame - 1);
    EXPECT_EQ(maximumIntersection.durationFrames(), 4);
    EXPECT_EQ(maximumUnion.startTimeCode().frame(), maxFrame - 10);
    EXPECT_EQ(maximumUnion.stopTimeCode().frame(), maxFrame);
    EXPECT_EQ(maximumUnion.durationFrames(), 10);
    EXPECT_TRUE(maximumUnion.containsFrame(maxFrame - 1));
    EXPECT_FALSE(maximumUnion.containsFrame(maxFrame));
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
