#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <utility>

import Math.Interpolate;

using namespace ArtifactCore;

namespace {

using Track = KeyframeInterpolator<float>;

Track makeLinearTrack()
{
    Track track;
    for (int index = 0; index != 4; ++index) {
        Track::KeyframeEntry key;
        key.time = static_cast<double>(index);
        key.value = static_cast<float>(index * 10);
        track.addKeyframe(key);
    }
    return track;
}

}

TEST(KeyframeContractTest, EmptyAndSingleKeyTracksHaveStableBoundaryBehavior)
{
    Track empty;
    EXPECT_TRUE(empty.isEmpty());
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_FLOAT_EQ(empty.evaluate(-100.0), 0.0f);
    EXPECT_FLOAT_EQ(empty.speedAt(0.0), 0.0f);

    Track single;
    Track::KeyframeEntry key;
    key.time = 4.0;
    key.value = 12.5f;
    single.addKeyframe(key);
    EXPECT_FLOAT_EQ(single.evaluate(-100.0), 12.5f);
    EXPECT_FLOAT_EQ(single.evaluate(4.0), 12.5f);
    EXPECT_FLOAT_EQ(single.evaluate(100.0), 12.5f);
    EXPECT_FLOAT_EQ(single.speedAt(4.0), 0.0f);
}

TEST(KeyframeContractTest, InsertionOrderIsSortedAndOutsideTimesClampToEndpoints)
{
    Track track;
    for (const auto [time, value] : std::array<std::pair<double, float>, 3>{
             {{8.0, 80.0f}, {2.0, 20.0f}, {5.0, 50.0f}}}) {
        Track::KeyframeEntry key;
        key.time = time;
        key.value = value;
        track.addKeyframe(key);
    }

    const auto keys = track.keyframes();
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_DOUBLE_EQ(keys[0].time, 2.0);
    EXPECT_DOUBLE_EQ(keys[1].time, 5.0);
    EXPECT_DOUBLE_EQ(keys[2].time, 8.0);
    EXPECT_FLOAT_EQ(track.evaluate(-1.0), 20.0f);
    EXPECT_FLOAT_EQ(track.evaluate(9.0), 80.0f);
}

TEST(KeyframeContractTest, ExactOuterKeyTimesReturnTheirStoredValues)
{
    const Track track = makeLinearTrack();

    EXPECT_FLOAT_EQ(track.evaluate(0.0), 0.0f);
    EXPECT_FLOAT_EQ(track.evaluate(3.0), 30.0f);
}

TEST(KeyframeContractTest, LinearTrackInterpolatesAcrossUnequalSegments)
{
    Track track;
    const std::array times{0.0, 2.0, 7.0};
    const std::array values{0.0f, 10.0f, 35.0f};
    for (std::size_t i = 0; i < times.size(); ++i) {
        Track::KeyframeEntry key;
        key.time = times[i];
        key.value = values[i];
        track.addKeyframe(key);
    }

    EXPECT_FLOAT_EQ(track.evaluate(1.0), 5.0f);
    EXPECT_FLOAT_EQ(track.evaluate(4.5), 22.5f);
    EXPECT_FLOAT_EQ(track.evaluate(2.0), 10.0f);
    EXPECT_NEAR(track.speedAt(1.0), 5.0f, 0.02f);
    EXPECT_NEAR(track.speedAt(4.0), 5.0f, 0.02f);
}

TEST(KeyframeContractTest, LinearSpeedUsesRightHandSlopeAndClampsOutsideTrack)
{
    Track track;
    Track::KeyframeEntry first;
    first.time = 2.0;
    first.value = 4.0f;
    Track::KeyframeEntry last;
    last.time = 5.0;
    last.value = 10.0f;
    track.addKeyframe(first);
    track.addKeyframe(last);

    EXPECT_FLOAT_EQ(track.speedAt(1.0), 0.0f);
    EXPECT_NEAR(track.speedAt(2.0), 2.0f, 0.02f);
    EXPECT_FLOAT_EQ(track.speedAt(5.0), 0.0f);
    EXPECT_FLOAT_EQ(track.speedAt(6.0), 0.0f);
}

TEST(KeyframeContractTest, ExactKeyBelongsToNewSegmentAfterConstantHold)
{
    Track track;
    Track::KeyframeEntry first;
    first.time = 0.0;
    first.value = 3.0f;
    first.type = InterpolationType::Constant;
    Track::KeyframeEntry second;
    second.time = 4.0;
    second.value = 9.0f;
    second.type = InterpolationType::Linear;
    Track::KeyframeEntry third;
    third.time = 8.0;
    third.value = 17.0f;
    track.addKeyframe(first);
    track.addKeyframe(second);
    track.addKeyframe(third);

    EXPECT_FLOAT_EQ(track.evaluate(3.999), 3.0f);
    EXPECT_FLOAT_EQ(track.evaluate(4.0), 9.0f);
    EXPECT_FLOAT_EQ(track.evaluate(6.0), 13.0f);
}

TEST(KeyframeContractTest, ConstantHoldHasZeroSpeedInsideSegment)
{
    Track track;
    Track::KeyframeEntry first;
    first.time = 0.0;
    first.value = 3.0f;
    first.type = InterpolationType::Constant;
    Track::KeyframeEntry second;
    second.time = 4.0;
    second.value = 9.0f;
    track.addKeyframe(first);
    track.addKeyframe(second);

    EXPECT_FLOAT_EQ(track.speedAt(2.0), 0.0f);
}

TEST(KeyframeContractTest, HermiteMatchesSmoothstepForZeroTangents)
{
    for (const float t : {0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f}) {
        const float expected = 3.0f * t * t - 2.0f * t * t * t;
        EXPECT_NEAR(hermiteInterpolate(0.0f, 0.0f, 1.0f, 0.0f, t), expected, 1e-6f);
    }
}

TEST(KeyframeContractTest, HermiteSupportsExtrapolationAndExactEndpoints)
{
    EXPECT_FLOAT_EQ(hermiteInterpolate(2.0f, 5.0f, 7.0f, -3.0f, 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(hermiteInterpolate(2.0f, 5.0f, 7.0f, -3.0f, 1.0f), 7.0f);
    EXPECT_NE(hermiteInterpolate(0.0f, 0.0f, 1.0f, 0.0f, -0.25f), 0.0f);
}

TEST(KeyframeContractTest, CatmullRomIsLinearForCollinearControlsAndPassesThrough)
{
    for (int step = 0; step <= 20; ++step) {
        const float t = static_cast<float>(step) / 20.0f;
        EXPECT_NEAR(catmullRomInterpolate(0.0f, 10.0f, 20.0f, 30.0f, t),
                    10.0f + 10.0f * t, 1e-5f);
    }
    EXPECT_FLOAT_EQ(catmullRomInterpolate(-1.0f, 4.0f, 9.0f, 12.0f, 0.0f), 4.0f);
    EXPECT_FLOAT_EQ(catmullRomInterpolate(-1.0f, 4.0f, 9.0f, 12.0f, 1.0f), 9.0f);
}

TEST(KeyframeContractTest, InterpolatorCatmullRomUsesDuplicatedEdgeNeighbors)
{
    Track track;
    for (const auto [time, value] : std::array<std::pair<double, float>, 3>{
             {{0.0, 0.0f}, {1.0, 10.0f}, {2.0, 20.0f}}}) {
        Track::KeyframeEntry key;
        key.time = time;
        key.value = value;
        key.type = InterpolationType::CatmullRom;
        track.addKeyframe(key);
    }

    EXPECT_NEAR(track.evaluate(0.5), 4.375f, 1e-5f);
    EXPECT_NEAR(track.evaluate(1.5), 15.0f, 1e-5f);
    EXPECT_NEAR(track.evaluate(1.0), 10.0f, 1e-6f);
}

TEST(KeyframeContractTest, HermiteInterpolatorPreservesLinearMotionAcrossUnequalSpans)
{
    Track track;
    for (const auto [time, value] : std::array<std::pair<double, float>, 3>{
             {{0.0, 0.0f}, {1.0, 1.0f}, {3.0, 3.0f}}}) {
        Track::KeyframeEntry key;
        key.time = time;
        key.value = value;
        key.type = InterpolationType::Hermite;
        track.addKeyframe(key);
    }

    for (int step = 0; step <= 30; ++step) {
        const double time = static_cast<double>(step) / 10.0;
        EXPECT_NEAR(track.evaluate(time), static_cast<float>(time), 1e-5f);
    }
}

TEST(KeyframeContractTest, NonUniformHermiteScalesFiniteDifferenceTangentsBySegmentDuration)
{
    Track track;
    for (const auto [time, value] : std::array<std::pair<double, float>, 3>{
             {{0.0, 0.0f}, {1.0, 2.0f}, {3.0, 2.0f}}}) {
        Track::KeyframeEntry key;
        key.time = time;
        key.value = value;
        key.type = InterpolationType::Hermite;
        track.addKeyframe(key);
    }

    EXPECT_NEAR(track.evaluate(2.0), 13.0f / 6.0f, 1e-5f);
}

TEST(KeyframeContractTest, BezierDefaultHandleTrackIsSymmetricAndFinite)
{
    Track track;
    for (const auto [time, value] : std::array<std::pair<double, float>, 2>{
             {{0.0, 0.0f}, {1.0, 1.0f}}}) {
        Track::KeyframeEntry key;
        key.time = time;
        key.value = value;
        key.type = InterpolationType::Bezier;
        track.addKeyframe(key);
    }

    EXPECT_NEAR(track.evaluate(0.5), 0.5f, 1e-4f);
    for (int step = 0; step <= 100; ++step) {
        const double t = static_cast<double>(step) / 100.0;
        EXPECT_TRUE(std::isfinite(track.evaluate(t)));
    }
}

TEST(KeyframeContractTest, BezierInterpolatorUsesOutgoingKeyCustomHandles)
{
    Track track;
    Track::KeyframeEntry first;
    first.time = 0.0;
    first.value = 0.0f;
    first.type = InterpolationType::Bezier;
    first.cp1_x = 0.2f;
    first.cp1_y = 0.8f;
    first.cp2_x = 0.8f;
    first.cp2_y = 0.9f;
    Track::KeyframeEntry last;
    last.time = 1.0;
    last.value = 1.0f;
    track.addKeyframe(first);
    track.addKeyframe(last);

    EXPECT_NEAR(track.evaluate(0.5), 0.7625f, 1e-4f);
}

TEST(KeyframeContractTest, ClearResetsContentsAndSpeed)
{
    auto track = makeLinearTrack();
    ASSERT_EQ(track.size(), 4u);
    track.clear();

    EXPECT_TRUE(track.isEmpty());
    EXPECT_EQ(track.size(), 0u);
    EXPECT_FLOAT_EQ(track.evaluate(1.5), 0.0f);
    EXPECT_FLOAT_EQ(track.speedAt(1.5), 0.0f);
}
