#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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

TEST(SequenceGroupContractTest, EveryOrderIsABijectionForElementCountsOneThroughThirtyTwo)
{
    constexpr std::array<SequenceOrder, 4> orders = {
        SequenceOrder::Forward,
        SequenceOrder::Reverse,
        SequenceOrder::CenterOut,
        SequenceOrder::OutsideIn,
    };
    constexpr double startDelay = 1.25;
    constexpr double elementInterval = 0.125;

    for (const SequenceOrder order : orders) {
        SequenceDefinition definition;
        definition.order = order;
        definition.timing.startDelay = startDelay;
        definition.timing.elementInterval = elementInterval;

        for (std::int32_t count = 1; count <= 32; ++count) {
            std::array<bool, 32> rankSeen{};
            for (std::int32_t element = 0; element < count; ++element) {
                const double start = definition.elementStartTime(element, count);
                const double rankValue = (start - startDelay) / elementInterval;
                const auto rank = static_cast<std::int32_t>(std::llround(rankValue));

                EXPECT_NEAR(rankValue, static_cast<double>(rank), 1.0e-12)
                    << "count=" << count << " element=" << element;
                ASSERT_GE(rank, 0) << "count=" << count << " element=" << element;
                ASSERT_LT(rank, count) << "count=" << count << " element=" << element;
                EXPECT_FALSE(rankSeen[static_cast<std::size_t>(rank)])
                    << "count=" << count << " duplicate rank=" << rank;
                rankSeen[static_cast<std::size_t>(rank)] = true;
            }

            for (std::int32_t rank = 0; rank < count; ++rank) {
                EXPECT_TRUE(rankSeen[static_cast<std::size_t>(rank)])
                    << "count=" << count << " missing rank=" << rank;
            }

            EXPECT_DOUBLE_EQ(definition.elementStartTime(-1, count),
                             definition.elementStartTime(0, count));
            EXPECT_DOUBLE_EQ(definition.elementStartTime(count, count),
                             definition.elementStartTime(count - 1, count));
        }
    }
}

TEST(SequenceGroupContractTest, DurationUsesTheLatestScheduledElementForEveryOrder)
{
    constexpr std::array<SequenceOrder, 4> orders = {
        SequenceOrder::Forward,
        SequenceOrder::Reverse,
        SequenceOrder::CenterOut,
        SequenceOrder::OutsideIn,
    };
    constexpr double startDelay = 0.375;
    constexpr double interval = 0.625;
    constexpr double phaseDuration = 2.25;

    for (const auto order : orders) {
        SequenceDefinition definition;
        definition.order = order;
        definition.distribution = SequenceDistribution::Stagger;
        definition.timing.startDelay = startDelay;
        definition.timing.elementInterval = interval;
        definition.phases = {SequencePhase{"main", phaseDuration}};

        for (std::int32_t count = 1; count <= 33; ++count) {
            // Each supported order is a permutation of [0, count), so the
            // latest scheduled rank is count - 1 regardless of its ordering.
            const std::int32_t latestOrderRank = count - 1;
            const double expectedDuration = startDelay +
                static_cast<double>(latestOrderRank) * interval + phaseDuration;
            EXPECT_DOUBLE_EQ(definition.durationForElementCount(count), expectedDuration)
                << "order=" << static_cast<int>(order) << " count=" << count;
        }

        definition.distribution = SequenceDistribution::Simultaneous;
        EXPECT_DOUBLE_EQ(definition.durationForElementCount(33), startDelay + phaseDuration);
        EXPECT_DOUBLE_EQ(definition.durationForElementCount(0), 0.0);
    }
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

TEST(SequenceGroupContractTest,
     ValidityRejectsNonFiniteTimingDurationsAndPhaseOffsets)
{
    const std::array<double, 3> nonFiniteValues{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };

    for (const double value : nonFiniteValues) {
        auto startDelay = staggeredDefinition();
        startDelay.timing.startDelay = value;
        EXPECT_FALSE(startDelay.isValid()) << "startDelay=" << value;

        auto elementInterval = staggeredDefinition();
        elementInterval.timing.elementInterval = value;
        EXPECT_FALSE(elementInterval.isValid()) << "elementInterval=" << value;

        auto playbackRate = staggeredDefinition();
        playbackRate.timing.playbackRate = value;
        EXPECT_FALSE(playbackRate.isValid()) << "playbackRate=" << value;

        auto phaseDuration = staggeredDefinition();
        phaseDuration.phases.front().duration = value;
        EXPECT_FALSE(phaseDuration.isValid()) << "phaseDuration=" << value;

        auto phaseStartOffset = staggeredDefinition();
        phaseStartOffset.phases.front().startOffset = value;
        EXPECT_FALSE(phaseStartOffset.isValid()) << "phaseStartOffset=" << value;
    }
}

TEST(SequenceGroupContractTest,
     ValidityRejectsFiniteInputsWhoseDerivedDurationOverflows)
{
    const double largestFinite = std::numeric_limits<double>::max();

    auto phaseAccumulation = staggeredDefinition();
    phaseAccumulation.phases = {
        SequencePhase{"first", largestFinite},
        SequencePhase{"second", largestFinite},
    };
    EXPECT_FALSE(std::isfinite(phaseAccumulation.duration()));
    EXPECT_FALSE(phaseAccumulation.isValid());

    auto staggerMultiplication = staggeredDefinition();
    staggerMultiplication.timing.elementInterval = largestFinite;
    staggerMultiplication.phases = {SequencePhase{"single", 1.0}};
    EXPECT_FALSE(std::isfinite(
        staggerMultiplication.durationForElementCount(3)));
    EXPECT_FALSE(staggerMultiplication.isValid());
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

TEST(SequenceGroupContractTest, ChangingElementCountClampsActivePlaybackBeforeEitherBoundary)
{
    const auto definition = staggeredDefinition();
    const double threeElementDuration = definition.durationForElementCount(3);
    const double oneElementDuration = definition.durationForElementCount(1);
    ASSERT_GT(threeElementDuration, oneElementDuration);

    SequencePlayer forward(&definition);
    forward.setElementCount(3);
    forward.play();
    forward.advance(2.5);
    ASSERT_DOUBLE_EQ(forward.time(), 5.0);
    ASSERT_EQ(forward.playback().state, SequencePlaybackState::Playing);

    forward.setElementCount(1);
    EXPECT_DOUBLE_EQ(forward.time(), oneElementDuration);
    EXPECT_EQ(forward.playback().state, SequencePlaybackState::Playing);
    forward.advance(0.25);
    EXPECT_DOUBLE_EQ(forward.time(), oneElementDuration);
    EXPECT_EQ(forward.playback().state, SequencePlaybackState::Completed);

    SequencePlayer reverse(&definition);
    reverse.setElementCount(3);
    reverse.play(true);
    ASSERT_DOUBLE_EQ(reverse.time(), threeElementDuration);
    reverse.setElementCount(1);
    EXPECT_DOUBLE_EQ(reverse.time(), oneElementDuration);
    EXPECT_TRUE(reverse.playback().reverse);
    EXPECT_EQ(reverse.playback().state, SequencePlaybackState::Playing);
    reverse.advance(2.0);
    EXPECT_DOUBLE_EQ(reverse.time(), 0.0);
    EXPECT_EQ(reverse.playback().state, SequencePlaybackState::Completed);
}

TEST(SequenceGroupContractTest, NonFiniteAdvanceDoesNotCorruptPlaybackProgress)
{
    const auto definition = staggeredDefinition();
    const std::array<double, 3> invalidDeltas = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };

    for (const double invalidDelta : invalidDeltas) {
        SequencePlayer player(&definition);
        player.setElementCount(2);
        player.play();
        player.advance(0.25);
        ASSERT_DOUBLE_EQ(player.time(), 0.5);
        ASSERT_EQ(player.playback().state, SequencePlaybackState::Playing);

        player.advance(invalidDelta);

        EXPECT_DOUBLE_EQ(player.time(), 0.5) << "delta=" << invalidDelta;
        EXPECT_EQ(player.playback().state, SequencePlaybackState::Playing)
            << "delta=" << invalidDelta;
    }
}

TEST(SequenceGroupContractTest,
     FiniteAdvanceWhosePlaybackScaledDeltaOverflowsAtBothPlaybackBoundaries)
{
    const auto definition = staggeredDefinition();
    const double duration = definition.durationForElementCount(2);

    SequencePlayer forward(&definition);
    forward.setElementCount(2);
    forward.play();
    forward.advance(std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(forward.time(), duration);
    EXPECT_TRUE(std::isfinite(forward.time()));
    EXPECT_EQ(forward.playback().state, SequencePlaybackState::Completed);

    SequencePlayer reverse(&definition);
    reverse.setElementCount(2);
    reverse.play(true);
    reverse.advance(std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(reverse.time(), 0.0);
    EXPECT_TRUE(std::isfinite(reverse.time()));
    EXPECT_EQ(reverse.playback().state, SequencePlaybackState::Completed);
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

TEST(SequenceGroupContractTest, PauseAndResumePreserveForwardAndReverseProgressToBoundaries)
{
    const auto definition = staggeredDefinition();
    const double duration = definition.durationForElementCount(2);

    SequencePlayer forward(&definition);
    forward.setElementCount(2);
    forward.play();
    forward.advance(0.75);
    const double forwardPausedTime = forward.time();
    forward.pause();
    forward.advance(2.0);
    EXPECT_DOUBLE_EQ(forward.time(), forwardPausedTime);
    EXPECT_EQ(forward.playback().state, SequencePlaybackState::Paused);
    forward.play();
    forward.advance(0.25);
    EXPECT_DOUBLE_EQ(forward.time(), 2.0);
    EXPECT_EQ(forward.playback().state, SequencePlaybackState::Playing);
    forward.pause();
    forward.play();
    forward.advance(duration);
    EXPECT_DOUBLE_EQ(forward.time(), duration);
    EXPECT_EQ(forward.playback().state, SequencePlaybackState::Completed);

    SequencePlayer reverse(&definition);
    reverse.setElementCount(2);
    reverse.play(true);
    reverse.advance(0.75);
    const double reversePausedTime = reverse.time();
    reverse.pause();
    reverse.advance(2.0);
    EXPECT_DOUBLE_EQ(reverse.time(), reversePausedTime);
    EXPECT_EQ(reverse.playback().state, SequencePlaybackState::Paused);
    reverse.play(true);
    reverse.advance(0.25);
    EXPECT_DOUBLE_EQ(reverse.time(), duration - 2.0);
    EXPECT_EQ(reverse.playback().state, SequencePlaybackState::Playing);
    reverse.pause();
    reverse.play(true);
    reverse.advance(duration);
    EXPECT_DOUBLE_EQ(reverse.time(), 0.0);
    EXPECT_EQ(reverse.playback().state, SequencePlaybackState::Completed);
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

TEST(SequenceGroupContractTest,
     ElementIntervalOverridePreservesCenterOutAndOutsideInOrder)
{
    constexpr std::array<SequenceOrder, 2> orders = {
        SequenceOrder::CenterOut, SequenceOrder::OutsideIn};
    constexpr std::array<std::array<std::int32_t, 4>, 2> ranks = {{
        {{2, 0, 1, 3}}, // center-out: indices 1, 2, 0, 3
        {{0, 2, 3, 1}}, // outside-in: indices 0, 3, 1, 2
    }};

    for (std::size_t orderIndex = 0; orderIndex < orders.size(); ++orderIndex) {
        auto definition = staggeredDefinition();
        definition.order = orders[orderIndex];
        SequencePlayer player(&definition);
        player.setElementCount(4);
        SequenceOverrides overrides;
        overrides.hasElementInterval = true;
        overrides.elementInterval = 2.5;
        player.setOverrides(overrides);

        for (std::int32_t elementIndex = 0; elementIndex < 4; ++elementIndex) {
            const double expected = definition.timing.startDelay +
                static_cast<double>(ranks[orderIndex][elementIndex]) *
                    overrides.elementInterval +
                definition.phases.front().startOffset;
            EXPECT_DOUBLE_EQ(player.phaseStartTime(elementIndex, 0), expected)
                << "order=" << orderIndex << " element=" << elementIndex;
        }
    }
}

TEST(SequenceGroupContractTest,
     ReverseOverrideControlsPlaybackDirectionAndInitialBoundary)
{
    const auto definition = staggeredDefinition();
    const double duration = definition.durationForElementCount(2);

    SequencePlayer forcedReverse(&definition);
    forcedReverse.setElementCount(2);
    SequenceOverrides reverseOverride;
    reverseOverride.hasReverse = true;
    reverseOverride.reverse = true;
    forcedReverse.setOverrides(reverseOverride);
    forcedReverse.play(false);

    EXPECT_TRUE(forcedReverse.playback().reverse);
    EXPECT_DOUBLE_EQ(forcedReverse.time(), duration);
    forcedReverse.advance(0.25);
    EXPECT_DOUBLE_EQ(forcedReverse.time(), duration - 0.25);
    EXPECT_EQ(forcedReverse.playback().state, SequencePlaybackState::Playing);

    SequencePlayer forcedForward(&definition);
    forcedForward.setElementCount(2);
    SequenceOverrides forwardOverride;
    forwardOverride.hasReverse = true;
    forwardOverride.reverse = false;
    forcedForward.setOverrides(forwardOverride);
    forcedForward.play(true);

    EXPECT_FALSE(forcedForward.playback().reverse);
    EXPECT_DOUBLE_EQ(forcedForward.time(), 0.0);
    forcedForward.advance(0.25);
    EXPECT_DOUBLE_EQ(forcedForward.time(), 0.25);
    EXPECT_EQ(forcedForward.playback().state, SequencePlaybackState::Playing);
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
