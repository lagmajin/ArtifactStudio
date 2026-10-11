#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <limits>

import Audio.DSP.AllPassFilter;

using ArtifactCore::Audio::DSP::AllPassFilter;

TEST(AllPassFilterStandaloneTest, UninitializedFilterUsesDefaultFeedbackEquation)
{
    AllPassFilter filter;
    EXPECT_FLOAT_EQ(filter.process(0.25f), -0.125f);
}

TEST(AllPassFilterStandaloneTest, ZeroFeedbackProducesOneSampleDelayedImpulse)
{
    AllPassFilter filter;
    filter.initialize(1.0f, 4.0f);
    filter.setParametersSamples(1.0f, 0.0f);

    EXPECT_FLOAT_EQ(filter.process(1.0f), 0.0f);
    EXPECT_FLOAT_EQ(filter.process(0.0f), 1.0f);
    EXPECT_FLOAT_EQ(filter.process(0.0f), 0.0f);
}

TEST(AllPassFilterStandaloneTest, FeedbackIsClampedToStableRange)
{
    AllPassFilter positive;
    positive.initialize(1.0f, 4.0f);
    positive.setParametersSamples(1.0f, 4.0f);
    EXPECT_NEAR(positive.process(1.0f), -0.999f, 1e-6f);

    AllPassFilter negative;
    negative.initialize(1.0f, 4.0f);
    negative.setParametersSamples(1.0f, -4.0f);
    EXPECT_NEAR(negative.process(1.0f), 0.999f, 1e-6f);
}

TEST(AllPassFilterStandaloneTest, NonFiniteFeedbackDefaultsToZero)
{
    AllPassFilter filter;
    filter.initialize(1.0f, 4.0f);
    filter.setParametersSamples(1.0f, std::numeric_limits<float>::quiet_NaN());

    EXPECT_FLOAT_EQ(filter.process(1.0f), 0.0f);
}

TEST(AllPassFilterStandaloneTest, NonFiniteDelayDefaultsToZero)
{
    AllPassFilter filter;
    filter.initialize(1.0f, 4.0f);
    filter.setParametersSamples(std::numeric_limits<float>::infinity(), 0.0f);

    EXPECT_FLOAT_EQ(filter.process(1.0f), 0.0f);
}

TEST(AllPassFilterStandaloneTest, SecondsAndSampleParameterSettersAgree)
{
    AllPassFilter seconds;
    AllPassFilter samples;
    seconds.initialize(1.0f, 4.0f);
    samples.initialize(1.0f, 4.0f);
    seconds.setParameters(0.25f, 0.4f, 4.0f);
    samples.setParametersSamples(1.0f, 0.4f);

    for (const float input : {0.5f, -0.25f, 0.75f, 0.0f})
        EXPECT_FLOAT_EQ(seconds.process(input), samples.process(input));
}

TEST(AllPassFilterStandaloneTest, ModulationChangesTheDelayTap)
{
    AllPassFilter baseDelay;
    AllPassFilter modulatedDelay;
    baseDelay.initialize(1.0f, 4.0f);
    modulatedDelay.initialize(1.0f, 4.0f);
    baseDelay.setParametersSamples(1.0f, 0.0f);
    modulatedDelay.setParametersSamples(1.0f, 0.0f);
    baseDelay.process(1.0f);
    modulatedDelay.process(1.0f);

    EXPECT_FLOAT_EQ(baseDelay.process(0.0f), 1.0f);
    EXPECT_FLOAT_EQ(modulatedDelay.process(0.0f, 1.0f), 0.0f);
}

TEST(AllPassFilterStandaloneTest, ClearRemovesTheStoredDelayHistory)
{
    AllPassFilter filter;
    filter.initialize(1.0f, 4.0f);
    filter.setParametersSamples(1.0f, 0.0f);
    filter.process(1.0f);
    filter.clear();

    EXPECT_FLOAT_EQ(filter.process(0.0f), 0.0f);
}

TEST(AllPassFilterStandaloneTest, InvalidSecondsParametersSelectZeroDelay)
{
    AllPassFilter filter;
    filter.initialize(1.0f, 4.0f);
    filter.setParameters(-0.25f, 0.0f, 4.0f);

    EXPECT_FLOAT_EQ(filter.process(1.0f), 0.0f);
}

