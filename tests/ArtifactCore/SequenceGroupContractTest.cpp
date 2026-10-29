#include <gtest/gtest.h>

#include <string>

import Sequence.SequenceGroup;

using namespace ArtifactCore;

namespace {
SequenceDefinition staggeredDefinition()
{
    SequenceDefinition definition;
    definition.distribution = SequenceDistribution::Stagger;
    definition.timing.startDelay = 0.5;
    definition.timing.elementInterval = 1.0;
    definition.timing.playbackRate = 2.0;
    definition.phases = {
        SequencePhase{"intro", 1.0, 0.25, SequencePhaseStart::AfterPreviousPhase, "linear"},
        SequencePhase{"outro", 2.0, 0.5, SequencePhaseStart::AfterPreviousPhase, "linear"},
    };
    return definition;
}
}

TEST(SequenceGroupContractTest, ElementStartTimeHonorsForwardReverseAndOutsideInOrder)
{
    SequenceDefinition definition;
    definition.timing.startDelay = 2.0;
    definition.timing.elementInterval = 0.5;

    definition.order = SequenceOrder::Forward;
    EXPECT_DOUBLE_EQ(definition.elementStartTime(0, 5), 2.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(2, 5), 3.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(4, 5), 4.0);

    definition.order = SequenceOrder::Reverse;
    EXPECT_DOUBLE_EQ(definition.elementStartTime(0, 5), 4.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(2, 5), 3.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(4, 5), 2.0);

    definition.order = SequenceOrder::OutsideIn;
    EXPECT_DOUBLE_EQ(definition.elementStartTime(0, 5), 2.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(1, 5), 3.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(2, 5), 4.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(3, 5), 3.5);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(4, 5), 2.5);
}

TEST(SequenceGroupContractTest, SimultaneousAndSingleElementStartAtTheConfiguredDelay)
{
    SequenceDefinition definition;
    definition.timing.startDelay = 3.25;
    definition.timing.elementInterval = 2.0;

    EXPECT_DOUBLE_EQ(definition.elementStartTime(0, 1), 3.25);
    definition.distribution = SequenceDistribution::Simultaneous;
    EXPECT_DOUBLE_EQ(definition.elementStartTime(0, 8), 3.25);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(7, 8), 3.25);
}

TEST(SequenceGroupContractTest, CenterOutOrderAssignsUniqueRanksForOddAndEvenCounts)
{
    SequenceDefinition definition;
    definition.order = SequenceOrder::CenterOut;
    definition.timing.startDelay = 2.0;
    definition.timing.elementInterval = 0.5;

    EXPECT_DOUBLE_EQ(definition.elementStartTime(0, 5), 3.5);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(1, 5), 2.5);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(2, 5), 2.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(3, 5), 3.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(4, 5), 4.0);

    definition.timing.elementInterval = 1.0;
    EXPECT_DOUBLE_EQ(definition.elementStartTime(0, 4), 4.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(1, 4), 2.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(2, 4), 3.0);
    EXPECT_DOUBLE_EQ(definition.elementStartTime(3, 4), 5.0);
}

TEST(SequenceGroupContractTest, DurationIncludesNonNegativePhasesAndLastStaggeredElement)
{
    auto definition = staggeredDefinition();
    definition.phases.push_back(
        SequencePhase{"negative", -4.0, 0.0, SequencePhaseStart::AfterPreviousPhase, "linear"});

    EXPECT_DOUBLE_EQ(definition.duration(), 3.5);
    EXPECT_DOUBLE_EQ(definition.durationForElementCount(0), 0.0);
    EXPECT_DOUBLE_EQ(definition.durationForElementCount(3), 5.5);
}

TEST(SequenceGroupContractTest, PhaseStartUsesElementOffsetAndPrecedingDurations)
{
    const auto definition = staggeredDefinition();

    EXPECT_DOUBLE_EQ(definition.phaseStartTime(1, 3, 0), 1.75);
    EXPECT_DOUBLE_EQ(definition.phaseStartTime(1, 3, 1), 3.0);

    auto absolute = definition;
    absolute.phases[1].start = SequencePhaseStart::Absolute;
    absolute.phases[1].startOffset = 12.0;
    EXPECT_DOUBLE_EQ(absolute.phaseStartTime(1, 3, 1), 12.0);
    EXPECT_DOUBLE_EQ(definition.phaseStartTime(1, 3, -1), 0.0);
    EXPECT_DOUBLE_EQ(definition.phaseStartTime(1, 3, 8), 0.0);
}

TEST(SequenceGroupContractTest, ValidityRejectsMissingPhasesAndInvalidTiming)
{
    SequenceDefinition definition;
    EXPECT_FALSE(definition.isValid());

    definition.phases.push_back(SequencePhase{"phase", 1.0});
    EXPECT_TRUE(definition.isValid());
    definition.timing.elementInterval = -0.1;
    EXPECT_FALSE(definition.isValid());
    definition.timing.elementInterval = 0.0;
    definition.timing.playbackRate = 0.0;
    EXPECT_FALSE(definition.isValid());
    definition.timing.playbackRate = 1.0;
    definition.phases.front().duration = -1.0;
    EXPECT_FALSE(definition.isValid());
}

TEST(SequenceGroupContractTest, PlayerAdvancesByPlaybackRateAndCompletesAtDuration)
{
    const auto definition = staggeredDefinition();
    SequencePlayer player(&definition);
    player.setElementCount(2);

    EXPECT_EQ(player.playback().state, SequencePlaybackState::Stopped);
    player.play();
    player.advance(0.5);
    EXPECT_DOUBLE_EQ(player.time(), 1.0);
    EXPECT_EQ(player.playback().state, SequencePlaybackState::Playing);
    player.advance(10.0);
    EXPECT_DOUBLE_EQ(player.time(), definition.durationForElementCount(2));
    EXPECT_EQ(player.playback().state, SequencePlaybackState::Completed);
}

TEST(SequenceGroupContractTest, PlayerPauseStopAndResetControlTransportState)
{
    const auto definition = staggeredDefinition();
    SequencePlayer player(&definition);
    player.setElementCount(2);
    player.play();
    player.advance(0.25);
    player.pause();
    const double pausedTime = player.time();
    player.advance(1.0);
    EXPECT_DOUBLE_EQ(player.time(), pausedTime);
    EXPECT_EQ(player.playback().state, SequencePlaybackState::Paused);

    player.stop();
    EXPECT_DOUBLE_EQ(player.time(), 0.0);
    EXPECT_EQ(player.playback().state, SequencePlaybackState::Stopped);
    player.play();
    player.reset();
    EXPECT_EQ(player.playback().state, SequencePlaybackState::Stopped);
    EXPECT_EQ(player.elementCount(), 0);
}

TEST(SequenceGroupContractTest, ReversePlaybackStartsAtEndAndMovesTowardZero)
{
    const auto definition = staggeredDefinition();
    SequencePlayer player(&definition);
    player.setElementCount(2);
    player.play(true);
    const double duration = definition.durationForElementCount(2);
    EXPECT_DOUBLE_EQ(player.time(), duration);
    EXPECT_TRUE(player.playback().reverse);

    player.advance(0.5);
    EXPECT_DOUBLE_EQ(player.time(), duration - 1.0);
    player.advance(10.0);
    EXPECT_DOUBLE_EQ(player.time(), 0.0);
    EXPECT_EQ(player.playback().state, SequencePlaybackState::Completed);
}

TEST(SequenceGroupContractTest, OverridesReplacePlaybackRateAndStaggerInterval)
{
    auto definition = staggeredDefinition();
    SequencePlayer player(&definition);
    player.setElementCount(3);
    SequenceOverrides overrides;
    overrides.hasPlaybackRate = true;
    overrides.playbackRate = 0.5;
    overrides.hasElementInterval = true;
    overrides.elementInterval = 3.0;
    player.setOverrides(overrides);

    EXPECT_DOUBLE_EQ(player.phaseStartTime(2, 0), 6.75);
    player.play();
    player.advance(2.0);
    EXPECT_DOUBLE_EQ(player.time(), 1.0);
}

TEST(SequenceGroupContractTest, ChangingDefinitionResetsPlaybackAndClearsPosition)
{
    const auto first = staggeredDefinition();
    auto second = staggeredDefinition();
    SequencePlayer player(&first);
    player.setElementCount(2);
    player.play();
    player.advance(0.5);
    ASSERT_GT(player.time(), 0.0);

    player.setDefinition(&second);
    EXPECT_EQ(player.definition(), &second);
    EXPECT_EQ(player.elementCount(), 0);
    EXPECT_DOUBLE_EQ(player.time(), 0.0);
    EXPECT_EQ(player.playback().state, SequencePlaybackState::Stopped);
}
