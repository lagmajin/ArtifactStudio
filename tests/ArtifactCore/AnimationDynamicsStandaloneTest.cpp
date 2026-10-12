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
