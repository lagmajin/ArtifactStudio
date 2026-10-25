#include <gtest/gtest.h>

#include <cmath>
#include <limits>

import Animation.Dynamics;

using namespace ArtifactCore;

TEST(AnimationDynamicsStandaloneTest, SpringSeedsAtFirstTargetWithoutVelocity)
{
    SpringDamper1DState state;

    const float result = springDamper1D(state, 12.5f, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(result, 12.5f);
    EXPECT_FLOAT_EQ(state.position, 12.5f);
    EXPECT_FLOAT_EQ(state.velocity, 0.0f);
    EXPECT_TRUE(state.seeded);
}

TEST(AnimationDynamicsStandaloneTest, SpringMovesTowardTargetAndDampingReducesVelocity)
{
    SpringDamper1DState state;
    springDamper1D(state, 0.0f, 1.0f / 60.0f);
    state.position = 1.0f;
    state.velocity = 0.0f;

    const float result = springDamper1D(state, 0.0f, 1.0f / 60.0f,
                                        DynamicsPreset::Smooth());

    EXPECT_LT(result, 1.0f);
    EXPECT_LT(state.velocity, 0.0f);
    EXPECT_TRUE(std::isfinite(result));
}

TEST(AnimationDynamicsStandaloneTest, NonFiniteTargetLeavesSpringStateUnchanged)
{
    SpringDamper1DState state{3.0f, -2.0f, true};

    const float result = springDamper1D(
        state, std::numeric_limits<float>::quiet_NaN(), 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(result, 3.0f);
    EXPECT_FLOAT_EQ(state.position, 3.0f);
    EXPECT_FLOAT_EQ(state.velocity, -2.0f);
    EXPECT_TRUE(state.seeded);
}

TEST(AnimationDynamicsStandaloneTest, SpringClampsTimeStepToDocumentedMaximum)
{
    SpringDamper1DState clamped{1.0f, 0.0f, true};
    SpringDamper1DState reference{1.0f, 0.0f, true};

    const float actual = springDamper1D(clamped, 0.0f, 1.0f,
                                        DynamicsPreset::Smooth());
    const float expected = springDamper1D(reference, 0.0f, 0.1f,
                                          DynamicsPreset::Smooth());

    EXPECT_FLOAT_EQ(actual, expected);
    EXPECT_FLOAT_EQ(clamped.velocity, reference.velocity);
}

TEST(AnimationDynamicsStandaloneTest, TwoDimensionalSpringUpdatesAxesIndependently)
{
    SpringDamper2DState state;

    const DynVec2 seeded = springDamper2D(state, {4.0f, -7.0f}, 1.0f / 60.0f);
    ASSERT_FLOAT_EQ(seeded.x, 4.0f);
    ASSERT_FLOAT_EQ(seeded.y, -7.0f);

    const DynVec2 updated = springDamper2D(state, {0.0f, -7.0f}, 1.0f / 60.0f);

    EXPECT_LT(updated.x, 4.0f);
    EXPECT_FLOAT_EQ(updated.y, -7.0f);
}

TEST(AnimationDynamicsStandaloneTest, DynamicsChannelResetClearsSpringHistory)
{
    DynamicsChannel1D channel;
    channel.mode = DynamicsMode::Spring;
    channel.preset = DynamicsPreset::Bouncy();
    ASSERT_FLOAT_EQ(channel.update(5.0f, 1.0f / 60.0f), 5.0f);
    channel.update(0.0f, 1.0f / 60.0f);
    ASSERT_TRUE(channel.springState.seeded);

    channel.reset(9.0f);
    const float afterReset = channel.update(9.0f, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(afterReset, 9.0f);
    EXPECT_FLOAT_EQ(channel.springState.velocity, 0.0f);
    EXPECT_TRUE(channel.springState.seeded);
}

TEST(AnimationDynamicsStandaloneTest, VelocityAccumulatorTracksSamplesAndResetBaseline)
{
    VelocityAccumulator1D accumulator;

    EXPECT_FLOAT_EQ(accumulator.update(3.0f, 0.5f), 6.0f);
    EXPECT_FLOAT_EQ(accumulator.update(2.0f, 0.25f), -4.0f);

    accumulator.reset(10.0f);
    EXPECT_FLOAT_EQ(accumulator.velocity, 0.0f);
    EXPECT_FLOAT_EQ(accumulator.prevPos, 10.0f);
    EXPECT_FLOAT_EQ(accumulator.update(11.0f, 0.5f), 2.0f);
}

TEST(AnimationDynamicsStandaloneTest, VelocityAccumulatorClampsNonPositiveTimeStep)
{
    VelocityAccumulator1D zeroDt;
    VelocityAccumulator1D negativeDt;

    EXPECT_FLOAT_EQ(zeroDt.update(1.0f, 0.0f), 10000.0f);
    EXPECT_FLOAT_EQ(negativeDt.update(1.0f, -1.0f), 10000.0f);
}

TEST(AnimationDynamicsStandaloneTest, LagFollowerSeedsThenUsesClampedTimeStepAndMinimumTau)
{
    LagFollower1DState state;

    EXPECT_FLOAT_EQ(lagFollower1D(state, 4.0f, 0.0f, 0.0f), 4.0f);
    EXPECT_TRUE(state.seeded);

    const float afterZeroDt = lagFollower1D(state, 8.0f, 0.0f, 0.0f);
    const float expectedAlpha = 0.0001f / (0.001f + 0.0001f);
    EXPECT_NEAR(afterZeroDt, 4.0f + expectedAlpha * 4.0f, 1e-6f);
}

TEST(AnimationDynamicsStandaloneTest, LagFollowerClampsLargeTimeStep)
{
    LagFollower1DState largeStep{0.0f, true};
    LagFollower1DState clampedStep{0.0f, true};

    const float actual = lagFollower1D(largeStep, 10.0f, 1.0f, 0.1f);
    const float expected = lagFollower1D(clampedStep, 10.0f, 0.5f, 0.1f);

    EXPECT_FLOAT_EQ(actual, expected);
}

TEST(AnimationDynamicsStandaloneTest, TwoDimensionalVelocityAccumulatorTracksAxesAndReset)
{
    VelocityAccumulator2D accumulator;

    const DynVec2 first = accumulator.update({2.0f, -3.0f}, 0.5f);
    EXPECT_FLOAT_EQ(first.x, 4.0f);
    EXPECT_FLOAT_EQ(first.y, -6.0f);

    accumulator.reset({10.0f, 20.0f});
    const DynVec2 afterReset = accumulator.update({11.0f, 18.0f}, 0.25f);
    EXPECT_FLOAT_EQ(afterReset.x, 4.0f);
    EXPECT_FLOAT_EQ(afterReset.y, -8.0f);
}

TEST(AnimationDynamicsStandaloneTest, DynamicsChannelOffAndLagFollowUseTheirOwnState)
{
    DynamicsChannel1D channel;
    channel.mode = DynamicsMode::Off;
    EXPECT_FLOAT_EQ(channel.update(5.0f, 0.1f), 5.0f);
    EXPECT_FALSE(channel.springState.seeded);
    EXPECT_FALSE(channel.lagState.seeded);

    channel.mode = DynamicsMode::LagFollow;
    channel.lagTau = 0.1f;
    EXPECT_FLOAT_EQ(channel.update(2.0f, 0.1f), 2.0f);
    EXPECT_FLOAT_EQ(channel.update(6.0f, 0.1f), 4.0f);
    EXPECT_TRUE(channel.lagState.seeded);
    EXPECT_FALSE(channel.springState.seeded);
}

TEST(AnimationDynamicsStandaloneTest, OvershootClampBoundsBothTravelDirections)
{
    EXPECT_FLOAT_EQ(clampOvershoot(20.0f, 10.0f, 0.0f, 0.25f), 12.5f);
    EXPECT_FLOAT_EQ(clampOvershoot(-20.0f, -10.0f, 0.0f, 0.25f), -12.5f);
    EXPECT_FLOAT_EQ(clampOvershoot(20.0f, 10.0f, 0.0f, 0.0f), 10.0f);
}

TEST(AnimationDynamicsStandaloneTest, SpringChannelAppliesOvershootClampToReturnedValue)
{
    DynamicsChannel1D channel;
    channel.mode = DynamicsMode::Spring;
    channel.preset = DynamicsPreset{0.0f, 0.0f, 1.0f};
    channel.restPos = 0.0f;
    channel.overshootLimit = 0.25f;
    channel.clampOvershootEnabled = true;
    channel.springState = SpringDamper1DState{2.0f, 0.0f, true};

    const float returned = channel.update(1.0f, 0.01f);

    EXPECT_FLOAT_EQ(returned, 1.25f);
    EXPECT_FLOAT_EQ(channel.springState.position, 2.0f);
}

TEST(AnimationDynamicsStandaloneTest, ThreeDimensionalSpringUpdatesAndResetsAxesIndependently)
{
    SpringDamper3DState state;

    const DynVec3 seeded = springDamper3D(state, {2.0f, -3.0f, 4.0f}, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(seeded.x, 2.0f);
    EXPECT_FLOAT_EQ(seeded.y, -3.0f);
    EXPECT_FLOAT_EQ(seeded.z, 4.0f);

    const DynVec3 updated = springDamper3D(state, {0.0f, -3.0f, 4.0f}, 1.0f / 60.0f);
    EXPECT_LT(updated.x, 2.0f);
    EXPECT_FLOAT_EQ(updated.y, -3.0f);
    EXPECT_FLOAT_EQ(updated.z, 4.0f);

    resetSpringDamper3D(state);
    EXPECT_FALSE(state.x.seeded);
    EXPECT_FALSE(state.y.seeded);
    EXPECT_FALSE(state.z.seeded);
    EXPECT_FLOAT_EQ(state.x.velocity, 0.0f);
    EXPECT_FLOAT_EQ(state.y.velocity, 0.0f);
    EXPECT_FLOAT_EQ(state.z.velocity, 0.0f);
}

TEST(AnimationDynamicsStandaloneTest, ThreeDimensionalLagFollowerUpdatesAndResetsAxesIndependently)
{
    LagFollower3DState state;

    const DynVec3 seeded = lagFollower3D(state, {1.0f, 2.0f, 3.0f}, 0.1f);
    EXPECT_FLOAT_EQ(seeded.x, 1.0f);
    EXPECT_FLOAT_EQ(seeded.y, 2.0f);
    EXPECT_FLOAT_EQ(seeded.z, 3.0f);

    const DynVec3 updated = lagFollower3D(state, {5.0f, 2.0f, 3.0f}, 0.1f);
    EXPECT_FLOAT_EQ(updated.x, 3.0f);
    EXPECT_FLOAT_EQ(updated.y, 2.0f);
    EXPECT_FLOAT_EQ(updated.z, 3.0f);

    resetLagFollower3D(state);
    EXPECT_FALSE(state.x.seeded);
    EXPECT_FALSE(state.y.seeded);
    EXPECT_FALSE(state.z.seeded);
    EXPECT_FLOAT_EQ(state.x.value, 0.0f);
    EXPECT_FLOAT_EQ(state.y.value, 0.0f);
    EXPECT_FLOAT_EQ(state.z.value, 0.0f);
}

TEST(AnimationDynamicsStandaloneTest, TwoDimensionalDynamicsChannelDispatchesAndResetsAxes)
{
    DynamicsChannel2D channel;
    channel.x.mode = DynamicsMode::LagFollow;
    channel.y.mode = DynamicsMode::Off;

    const DynVec2 seeded = channel.update({1.0f, 7.0f}, 0.1f);
    EXPECT_FLOAT_EQ(seeded.x, 1.0f);
    EXPECT_FLOAT_EQ(seeded.y, 7.0f);

    const DynVec2 updated = channel.update({5.0f, -2.0f}, 0.1f);
    EXPECT_FLOAT_EQ(updated.x, 3.0f);
    EXPECT_FLOAT_EQ(updated.y, -2.0f);
    EXPECT_TRUE(channel.x.lagState.seeded);
    EXPECT_FALSE(channel.y.lagState.seeded);

    channel.reset({9.0f, -4.0f});
    EXPECT_FALSE(channel.x.lagState.seeded);
    EXPECT_FLOAT_EQ(channel.x.restPos, 9.0f);
    EXPECT_FLOAT_EQ(channel.y.restPos, -4.0f);
}

TEST(AnimationDynamicsStandaloneTest, TwoDimensionalLagFollowerUpdatesAndResetsAxesIndependently)
{
    LagFollower2DState state;

    const DynVec2 seeded = lagFollower2D(state, {2.0f, -4.0f}, 0.1f);
    EXPECT_FLOAT_EQ(seeded.x, 2.0f);
    EXPECT_FLOAT_EQ(seeded.y, -4.0f);

    const DynVec2 updated = lagFollower2D(state, {6.0f, -4.0f}, 0.1f);
    EXPECT_FLOAT_EQ(updated.x, 4.0f);
    EXPECT_FLOAT_EQ(updated.y, -4.0f);

    resetLagFollower2D(state);
    EXPECT_FALSE(state.x.seeded);
    EXPECT_FALSE(state.y.seeded);
    EXPECT_FLOAT_EQ(state.x.value, 0.0f);
    EXPECT_FLOAT_EQ(state.y.value, 0.0f);
}

TEST(AnimationDynamicsStandaloneTest, ThreeDimensionalDynamicsChannelDispatchesAndResetsAxes)
{
    DynamicsChannel3D channel;
    channel.x.mode = DynamicsMode::LagFollow;
    channel.y.mode = DynamicsMode::Off;
    channel.z.mode = DynamicsMode::Spring;

    const DynVec3 seeded = channel.update({1.0f, 7.0f, -2.0f}, 0.1f);
    EXPECT_FLOAT_EQ(seeded.x, 1.0f);
    EXPECT_FLOAT_EQ(seeded.y, 7.0f);
    EXPECT_FLOAT_EQ(seeded.z, -2.0f);
    EXPECT_TRUE(channel.x.lagState.seeded);
    EXPECT_FALSE(channel.y.lagState.seeded);
    EXPECT_TRUE(channel.z.springState.seeded);

    channel.reset({9.0f, -4.0f, 3.0f});
    EXPECT_FALSE(channel.x.lagState.seeded);
    EXPECT_FALSE(channel.y.springState.seeded);
    EXPECT_FALSE(channel.z.springState.seeded);
    EXPECT_FLOAT_EQ(channel.x.restPos, 9.0f);
    EXPECT_FLOAT_EQ(channel.y.restPos, -4.0f);
    EXPECT_FLOAT_EQ(channel.z.restPos, 3.0f);
}
