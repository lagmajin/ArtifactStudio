#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

#include <QString>

import Event.Bus;
import Frame.Position;
import Frame.Rate;
import Playback.Clock;
import Playback.State;

namespace {

using ArtifactCore::FramePosition;
using ArtifactCore::FrameRate;
using ArtifactCore::PlaybackClock;
using ArtifactCore::PlaybackClockSnapshot;
using ArtifactCore::PlaybackClockStateChangedEvent;
using ArtifactCore::PlaybackState;

struct RecordedEvent {
    PlaybackClockSnapshot previous;
    PlaybackClockSnapshot current;
    QString reason;
};

class EventRecorder {
public:
    EventRecorder()
        : subscription_(ArtifactCore::globalEventBus()
                            .subscribe<PlaybackClockStateChangedEvent>(
                                [this](const PlaybackClockStateChangedEvent& event) {
                                    events_.push_back(RecordedEvent{
                                        event.previous, event.current, event.reason});
                                }))
    {
    }

    bool connected() const { return subscription_.connected(); }
    const std::vector<RecordedEvent>& events() const { return events_; }
    void clear() { events_.clear(); }

private:
    std::vector<RecordedEvent> events_;
    ArtifactCore::EventBus::Subscription subscription_;
};

} // namespace

TEST(PlaybackClockContractTest, DefaultConstructionReportsStoppedBaseline)
{
    PlaybackClock clock;

    EXPECT_TRUE(clock.isStopped());
    EXPECT_FALSE(clock.isPlaying());
    EXPECT_FALSE(clock.isPaused());
    EXPECT_EQ(clock.state(), PlaybackState::Stopped);

    EXPECT_EQ(clock.currentFrame(), 0);
    EXPECT_EQ(clock.currentPosition().framePosition(), 0);
    EXPECT_DOUBLE_EQ(clock.framesPerSecond(), 30.0);
    EXPECT_DOUBLE_EQ(clock.playbackSpeed(), 1.0);
    EXPECT_FALSE(clock.isReversePlaying());

    EXPECT_FALSE(clock.isLooping());
    EXPECT_EQ(clock.loopStartFrame(), 0);
    EXPECT_EQ(clock.loopEndFrame(), 0);

    EXPECT_FALSE(clock.isAudioSyncEnabled());
    EXPECT_EQ(clock.audioOffset().count(), 0);
    EXPECT_FALSE(clock.isDropFrameDetectionEnabled());
    EXPECT_EQ(clock.droppedFrameCount(), 0);

    EXPECT_EQ(clock.elapsedTime().count(), 0);
    EXPECT_EQ(clock.elapsedTimeMs().count(), 0);
    EXPECT_DOUBLE_EQ(clock.elapsedSeconds(), 0.0);

    const PlaybackClockSnapshot snapshot = clock.snapshot();
    EXPECT_EQ(snapshot.state, PlaybackState::Stopped);
    EXPECT_EQ(snapshot.currentFrame, 0);
    EXPECT_DOUBLE_EQ(snapshot.frameRate, 30.0);
    EXPECT_DOUBLE_EQ(snapshot.playbackSpeed, 1.0);
    EXPECT_FALSE(snapshot.looping);
    EXPECT_EQ(snapshot.loopStart, 0);
    EXPECT_EQ(snapshot.loopEnd, 0);
    EXPECT_FALSE(snapshot.audioSyncEnabled);
    EXPECT_EQ(snapshot.audioOffset.count(), 0);
    EXPECT_EQ(snapshot.droppedFrameCount, 0);
}

TEST(PlaybackClockContractTest, FrameRateConstructorSetsNominalRate)
{
    PlaybackClock clock(FrameRate(24.0f));

    EXPECT_DOUBLE_EQ(clock.framesPerSecond(), 24.0);
    EXPECT_DOUBLE_EQ(clock.frameRate().framerate(), 24.0);
    EXPECT_TRUE(clock.isStopped());
    EXPECT_EQ(clock.currentFrame(), 0);
}

TEST(PlaybackClockContractTest, TransportTransitionsPublishOrderedReasons)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.start();
    EXPECT_TRUE(clock.isPlaying());
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("start"));
    EXPECT_EQ(recorder.events()[0].previous.state, PlaybackState::Stopped);
    EXPECT_EQ(recorder.events()[0].current.state, PlaybackState::Playing);

    clock.pause();
    EXPECT_TRUE(clock.isPaused());
    ASSERT_EQ(recorder.events().size(), 2u);
    EXPECT_EQ(recorder.events()[1].reason, QStringLiteral("pause"));
    EXPECT_EQ(recorder.events()[1].previous.state, PlaybackState::Playing);
    EXPECT_EQ(recorder.events()[1].current.state, PlaybackState::Paused);

    clock.resume();
    EXPECT_TRUE(clock.isPlaying());
    ASSERT_EQ(recorder.events().size(), 3u);
    EXPECT_EQ(recorder.events()[2].reason, QStringLiteral("resume"));
    EXPECT_EQ(recorder.events()[2].previous.state, PlaybackState::Paused);
    EXPECT_EQ(recorder.events()[2].current.state, PlaybackState::Playing);

    clock.stop();
    EXPECT_TRUE(clock.isStopped());
    EXPECT_EQ(clock.currentFrame(), 0);
    ASSERT_EQ(recorder.events().size(), 4u);
    EXPECT_EQ(recorder.events()[3].reason, QStringLiteral("stop"));
    EXPECT_EQ(recorder.events()[3].previous.state, PlaybackState::Playing);
    EXPECT_EQ(recorder.events()[3].current.state, PlaybackState::Stopped);
}

TEST(PlaybackClockContractTest, RedundantTransportCallsDoNotPublishOrChangeState)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;

    // pause/resume outside their valid source state are no-ops.
    clock.pause();
    clock.resume();
    EXPECT_TRUE(clock.isStopped());
    EXPECT_TRUE(recorder.events().empty());

    clock.start();
    ASSERT_EQ(recorder.events().size(), 1u);
    recorder.clear();

    // start() while already playing only refreshes timing internals.
    clock.start();
    EXPECT_TRUE(clock.isPlaying());
    EXPECT_TRUE(recorder.events().empty());

    // resume() while playing is a no-op.
    clock.resume();
    EXPECT_TRUE(clock.isPlaying());
    EXPECT_TRUE(recorder.events().empty());

    clock.pause();
    ASSERT_EQ(recorder.events().size(), 1u);
    recorder.clear();

    // pause() while already paused is a no-op.
    clock.pause();
    EXPECT_TRUE(clock.isPaused());
    EXPECT_TRUE(recorder.events().empty());

    // stop() while already stopped with an unchanged frame is a no-op.
    clock.stop();
    ASSERT_EQ(recorder.events().size(), 1u);
    recorder.clear();
    clock.stop();
    EXPECT_TRUE(recorder.events().empty());
}

TEST(PlaybackClockContractTest, SetFrameBecomesStartFrameAndStopReturnsToIt)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.setFrame(120);
    EXPECT_EQ(clock.currentFrame(), 120);
    EXPECT_EQ(clock.currentPosition().framePosition(), 120);
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("setFrame"));
    EXPECT_EQ(recorder.events()[0].current.currentFrame, 120);
    recorder.clear();

    clock.start();
    clock.setFrame(50);
    EXPECT_EQ(clock.currentFrame(), 50);
    clock.stop();
    EXPECT_EQ(clock.currentFrame(), 50)
        << "stop() must return to the most recent setFrame() start frame";

    clock.setPosition(FramePosition(75));
    EXPECT_EQ(clock.currentFrame(), 75);
    EXPECT_EQ(clock.currentPosition().framePosition(), 75);

    // Re-setting the same frame does not publish a duplicate change.
    recorder.clear();
    clock.setFrame(75);
    EXPECT_TRUE(recorder.events().empty());
}

TEST(PlaybackClockContractTest, TimecodeUsesTheConfiguredFrameRate)
{
    PlaybackClock clock;
    clock.setFrame(90);
    EXPECT_EQ(clock.timecode(), QStringLiteral("00:00:03:00"));
    EXPECT_EQ(clock.timecodeWithSubframe(), QStringLiteral("00:00:03:00.00"));

    clock.setFrameRate(FrameRate(24.0f));
    clock.setFrame(90);
    EXPECT_EQ(clock.timecode(), QStringLiteral("00:00:03:18"));
    EXPECT_DOUBLE_EQ(clock.framesPerSecond(), 24.0);

    clock.setFrame(0);
    EXPECT_EQ(clock.timecode(), QStringLiteral("00:00:00:00"));
}

TEST(PlaybackClockContractTest, PlaybackSpeedTracksDirectionAndSuppressesDuplicates)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.setPlaybackSpeed(2.0);
    EXPECT_DOUBLE_EQ(clock.playbackSpeed(), 2.0);
    EXPECT_FALSE(clock.isReversePlaying());
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("setPlaybackSpeed"));
    recorder.clear();

    clock.setPlaybackSpeed(-0.5);
    EXPECT_DOUBLE_EQ(clock.playbackSpeed(), -0.5);
    EXPECT_TRUE(clock.isReversePlaying());
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_DOUBLE_EQ(recorder.events()[0].current.playbackSpeed, -0.5);
    recorder.clear();

    clock.setPlaybackSpeed(-0.5);
    EXPECT_TRUE(recorder.events().empty());

    clock.setPlaybackSpeed(0.0);
    EXPECT_FALSE(clock.isReversePlaying());
    EXPECT_DOUBLE_EQ(clock.snapshot().playbackSpeed, 0.0);
    ASSERT_EQ(recorder.events().size(), 1u);

    clock.setPlaybackSpeed(-1.0);
    EXPECT_TRUE(clock.isReversePlaying());
}

TEST(PlaybackClockContractTest, ChangingSpeedWhilePausedKeepsTheCurrentFrame)
{
    PlaybackClock clock;
    clock.setFrameRate(FrameRate(24.0f));
    clock.setFrame(0);
    clock.start();

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(2);
    std::int64_t frameBeforePause = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        frameBeforePause = clock.currentFrame();
        if (frameBeforePause >= 6) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_GE(frameBeforePause, 6)
        << "the clock should advance enough to detect a re-anchor jump";

    clock.pause();
    const std::int64_t pausedFrame = clock.currentFrame();
    ASSERT_GE(pausedFrame, 6);

    clock.setPlaybackSpeed(2.0);

    EXPECT_EQ(clock.currentFrame(), pausedFrame)
        << "changing speed should alter subsequent time progression, not reinterpret elapsed time";
    EXPECT_DOUBLE_EQ(clock.playbackSpeed(), 2.0);
}

TEST(PlaybackClockContractTest,
     ChangingSpeedAndDirectionWhilePlayingKeepsFramePositionContinuous)
{
    PlaybackClock clock;
    clock.setFrameRate(FrameRate(60.0f));
    clock.setFrame(100);
    clock.start();

    const auto waitForFrame = [&clock](const auto& predicate) {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate(clock.currentFrame())) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return predicate(clock.currentFrame());
    };

    ASSERT_TRUE(waitForFrame([](std::int64_t frame) { return frame >= 106; }));
    const std::int64_t beforeSpeedChange = clock.currentFrame();
    clock.setPlaybackSpeed(2.0);
    const std::int64_t afterSpeedChange = clock.currentFrame();
    EXPECT_GE(afterSpeedChange, beforeSpeedChange - 1);
    EXPECT_LE(afterSpeedChange, beforeSpeedChange + 1)
        << "a speed change must re-anchor elapsed time at the observed frame";

    ASSERT_TRUE(waitForFrame([afterSpeedChange](std::int64_t frame) {
        return frame >= afterSpeedChange + 4;
    }));
    const std::int64_t beforeDirectionChange = clock.currentFrame();
    clock.setPlaybackSpeed(-1.0);
    const std::int64_t afterDirectionChange = clock.currentFrame();
    EXPECT_GE(afterDirectionChange, beforeDirectionChange - 1);
    EXPECT_LE(afterDirectionChange, beforeDirectionChange + 1)
        << "switching to reverse must not reinterpret elapsed time as negative progress";

    EXPECT_TRUE(waitForFrame([afterDirectionChange](std::int64_t frame) {
        return frame <= afterDirectionChange - 4;
    })) << "the clock should continue in reverse after the continuous direction change";
}

TEST(PlaybackClockContractTest, LoopRangeEnablesLoopingAndClearRetainsEndpoints)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.setLoopRange(10, 20);

    EXPECT_TRUE(clock.isLooping());
    EXPECT_EQ(clock.loopStartFrame(), 10);
    EXPECT_EQ(clock.loopEndFrame(), 20);
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("setLoopRange"));
    EXPECT_TRUE(recorder.events()[0].current.looping);
    recorder.clear();

    clock.clearLoopRange();
    EXPECT_FALSE(clock.isLooping());
    EXPECT_EQ(clock.loopStartFrame(), 10);
    EXPECT_EQ(clock.loopEndFrame(), 20)
        << "clearLoopRange() disables looping but keeps the stored endpoints";
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("clearLoopRange"));

    recorder.clear();
    clock.clearLoopRange();
    EXPECT_TRUE(recorder.events().empty());
}

TEST(PlaybackClockContractTest,
     PlaybackWrapsWithinStoredLoopEndpointsInBothDirections)
{
    PlaybackClock clock(FrameRate(30.0f));
    clock.setFrame(15);
    clock.setLoopRange(10, 20);
    clock.start();

    const auto observeWrap = [&clock](bool reverse) {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(2);
        std::int64_t previous = clock.currentFrame();
        bool observedWrap = false;
        while (std::chrono::steady_clock::now() < deadline) {
            const std::int64_t current = clock.currentFrame();
            EXPECT_GE(current, 10) << "loop frame escaped below its start";
            EXPECT_LE(current, 20) << "loop frame escaped above its end";
            if (reverse) {
                observedWrap = observedWrap ||
                    (previous <= 12 && current >= 18);
            } else {
                observedWrap = observedWrap ||
                    (previous >= 18 && current <= 12);
            }
            previous = current;
            if (observedWrap) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return observedWrap;
    };

    EXPECT_TRUE(observeWrap(false))
        << "forward playback should wrap from the high endpoint to the low endpoint";
    clock.setPlaybackSpeed(-1.0);
    EXPECT_TRUE(observeWrap(true))
        << "reverse playback should wrap from the low endpoint to the high endpoint";
    clock.stop();
}

TEST(PlaybackClockContractTest,
     PausingNearLoopEndFreezesFrameAndResumeContinuesThroughWrap)
{
    PlaybackClock clock(FrameRate(30.0f));
    clock.setFrame(18);
    clock.setLoopRange(10, 20);
    clock.setPlaybackSpeed(0.5);
    clock.start();

    const auto waitForFrame = [&clock](const auto& predicate) {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate(clock.currentFrame())) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return predicate(clock.currentFrame());
    };

    ASSERT_TRUE(waitForFrame([](std::int64_t frame) { return frame >= 19; }));
    clock.pause();
    const std::int64_t pausedFrame = clock.currentFrame();
    ASSERT_TRUE(clock.isPaused());
    ASSERT_GE(pausedFrame, 19);
    ASSERT_LT(pausedFrame, 20);

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    EXPECT_EQ(clock.currentFrame(), pausedFrame)
        << "loop evaluation must not advance a paused clock";

    clock.resume();
    ASSERT_TRUE(waitForFrame([pausedFrame](std::int64_t frame) {
        return frame < pausedFrame;
    })) << "resumed playback should cross the loop end and continue at its start";
    EXPECT_GE(clock.currentFrame(), 10);
    EXPECT_LT(clock.currentFrame(), 20);
    clock.stop();
}

TEST(PlaybackClockContractTest, AudioSyncIgnoresCallsWhileDisabled)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.start();
    recorder.clear();

    clock.syncToAudioClock(std::chrono::seconds(1));
    EXPECT_EQ(clock.audioOffset().count(), 0);
    EXPECT_TRUE(recorder.events().empty());

    clock.setAudioSyncEnabled(true);
    EXPECT_TRUE(clock.isAudioSyncEnabled());
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("setAudioSyncEnabled"));
    EXPECT_TRUE(recorder.events()[0].current.audioSyncEnabled);
}

TEST(PlaybackClockContractTest, AudioSyncRecordsOffsetAndPublishesOnlyOnVisibleChange)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock(FrameRate(24.0f));
    clock.setAudioSyncEnabled(true);
    clock.start();
    clock.pause();

    // While paused the video clock is frozen, so the exact offset is stable.
    const auto videoTime = clock.elapsedTime();
    recorder.clear();

    clock.syncToAudioClock(videoTime);
    EXPECT_EQ(clock.audioOffset().count(), 0)
        << "a zero offset leaves the observable snapshot unchanged";
    EXPECT_TRUE(recorder.events().empty());

    clock.syncToAudioClock(videoTime + std::chrono::seconds(1));
    EXPECT_EQ(clock.audioOffset().count(), 1000000);
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("syncToAudioClock"));
    EXPECT_EQ(recorder.events()[0].current.audioOffset.count(), 1000000);
}

TEST(PlaybackClockContractTest, DropFrameDetectionCountsFrameGapsAndResets)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.setDropFrameDetectionEnabled(true);
    EXPECT_TRUE(clock.isDropFrameDetectionEnabled());
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason,
              QStringLiteral("setDropFrameDetectionEnabled"));
    recorder.clear();

    // Stopped clocks anchor updateCurrentFrame() to startFrame_, so the
    // frame sequence is deterministic: -1 -> 100 -> 200.
    clock.setFrame(100);
    EXPECT_EQ(clock.currentFrame(), 100);
    EXPECT_EQ(clock.droppedFrameCount(), 0);

    clock.setFrame(200);
    EXPECT_EQ(clock.currentFrame(), 200);
    EXPECT_EQ(clock.droppedFrameCount(), 99)
        << "expected frames 101..199 to be reported as dropped";

    clock.resetDroppedFrameCount();
    EXPECT_EQ(clock.droppedFrameCount(), 0);

    recorder.clear();
    clock.resetDroppedFrameCount();
    EXPECT_TRUE(recorder.events().empty())
        << "resetting an already-zero counter is not a visible change";
}

TEST(PlaybackClockContractTest, CopyAndMovePreserveIndependentClockState)
{
    PlaybackClock original(FrameRate(24.0f));
    original.setFrame(40);
    original.setLoopRange(10, 20);
    original.setPlaybackSpeed(2.0);
    original.setAudioSyncEnabled(true);

    PlaybackClock copied(original);
    EXPECT_EQ(copied.currentFrame(), 40);
    EXPECT_DOUBLE_EQ(copied.framesPerSecond(), 24.0);
    EXPECT_DOUBLE_EQ(copied.playbackSpeed(), 2.0);
    EXPECT_TRUE(copied.isLooping());
    EXPECT_EQ(copied.loopStartFrame(), 10);
    EXPECT_EQ(copied.loopEndFrame(), 20);
    EXPECT_TRUE(copied.isAudioSyncEnabled());

    copied.setFrame(5);
    copied.setPlaybackSpeed(-1.0);
    copied.clearLoopRange();
    EXPECT_EQ(original.currentFrame(), 40) << "copies must not share storage";
    EXPECT_DOUBLE_EQ(original.playbackSpeed(), 2.0);
    EXPECT_TRUE(original.isLooping());

    PlaybackClock assigned;
    assigned = original;
    assigned.setFrame(7);
    EXPECT_EQ(original.currentFrame(), 40);
    EXPECT_EQ(assigned.currentFrame(), 7);

    PlaybackClock moved(std::move(assigned));
    EXPECT_EQ(moved.currentFrame(), 7);
    EXPECT_DOUBLE_EQ(moved.framesPerSecond(), 24.0);
    EXPECT_TRUE(moved.isLooping());
    moved.setFrame(8);
    EXPECT_EQ(moved.currentFrame(), 8);

    PlaybackClock moveAssigned;
    moveAssigned = std::move(moved);
    moveAssigned.setFrame(9);
    EXPECT_EQ(moveAssigned.currentFrame(), 9);
    EXPECT_DOUBLE_EQ(moveAssigned.framesPerSecond(), 24.0);
}

TEST(PlaybackClockContractTest, SnapshotAndStatisticsReflectConfiguration)
{
    PlaybackClock clock(FrameRate(24.0f));
    clock.setFrame(90);
    clock.setPlaybackSpeed(0.5);
    clock.setLoopRange(10, 20);
    clock.setAudioSyncEnabled(true);
    clock.setDropFrameDetectionEnabled(true);

    const PlaybackClockSnapshot snapshot = clock.snapshot();
    EXPECT_EQ(snapshot.state, PlaybackState::Stopped);
    EXPECT_EQ(snapshot.currentFrame, 90);
    EXPECT_DOUBLE_EQ(snapshot.frameRate, 24.0);
    EXPECT_DOUBLE_EQ(snapshot.playbackSpeed, 0.5);
    EXPECT_TRUE(snapshot.looping);
    EXPECT_EQ(snapshot.loopStart, 10);
    EXPECT_EQ(snapshot.loopEnd, 20);
    EXPECT_TRUE(snapshot.audioSyncEnabled);
    EXPECT_EQ(snapshot.audioOffset.count(), 0);
    EXPECT_EQ(snapshot.droppedFrameCount, 0);

    const QString stats = clock.statistics();
    EXPECT_TRUE(stats.contains(QStringLiteral("State: Stopped")));
    EXPECT_TRUE(stats.contains(QStringLiteral("Current Frame: 90")));
    EXPECT_TRUE(stats.contains(QStringLiteral("Frame Rate: 24 fps")));
    EXPECT_TRUE(stats.contains(QStringLiteral("Playback Speed: 0.5x")));
    EXPECT_TRUE(stats.contains(QStringLiteral("Audio Sync: Enabled")));
    EXPECT_TRUE(stats.contains(QStringLiteral("Dropped Frames: 0")));
    EXPECT_TRUE(stats.contains(QStringLiteral("Looping: Yes")));
    EXPECT_TRUE(stats.contains(QStringLiteral("Loop Range: 10 - 20")));
}

TEST(PlaybackClockContractTest, ResetReturnsToStartFrameAndClearsSyncCountersOnly)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock(FrameRate(24.0f));
    clock.setFrame(30);
    clock.setPlaybackSpeed(-1.0);
    clock.setLoopRange(0, 60);
    clock.setAudioSyncEnabled(true);

    clock.start();
    clock.pause();
    clock.syncToAudioClock(std::chrono::seconds(2));
    EXPECT_NE(clock.audioOffset().count(), 0);

    // Produce a deterministic dropped-frame count.
    clock.setDropFrameDetectionEnabled(true);
    clock.setFrame(100);
    EXPECT_EQ(clock.currentFrame(), 100);
    clock.setFrame(200);
    EXPECT_EQ(clock.droppedFrameCount(), 99);

    recorder.clear();
    clock.reset();

    EXPECT_TRUE(clock.isStopped());
    EXPECT_EQ(clock.currentFrame(), 200) << "reset() returns to the start frame";
    EXPECT_EQ(clock.audioOffset().count(), 0);
    EXPECT_EQ(clock.droppedFrameCount(), 0);
    EXPECT_DOUBLE_EQ(clock.framesPerSecond(), 24.0);
    EXPECT_DOUBLE_EQ(clock.playbackSpeed(), -1.0)
        << "reset() does not change playback speed";
    EXPECT_TRUE(clock.isLooping()) << "reset() does not clear the loop range";
    EXPECT_TRUE(clock.isAudioSyncEnabled());
    EXPECT_TRUE(clock.isDropFrameDetectionEnabled());

    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("reset"));
    EXPECT_EQ(recorder.events()[0].current.state, PlaybackState::Stopped);
    EXPECT_EQ(recorder.events()[0].current.currentFrame, 200);

    recorder.clear();
    clock.reset();
    EXPECT_TRUE(recorder.events().empty());
}

TEST(PlaybackClockContractTest, SetFrameRatePublishesReasonWithoutMovingTheCurrentFrame)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.setFrame(90);
    recorder.clear();

    clock.setFrameRate(FrameRate(24.0f));

    EXPECT_DOUBLE_EQ(clock.framesPerSecond(), 24.0);
    EXPECT_EQ(clock.currentFrame(), 90);
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("setFrameRate"));
    EXPECT_DOUBLE_EQ(recorder.events()[0].current.frameRate, 24.0);
}

TEST(PlaybackClockContractTest, SetPositionPublishesTheSetFrameReason)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.setPosition(FramePosition(42));

    EXPECT_EQ(clock.currentFrame(), 42);
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("setFrame"));
    EXPECT_EQ(recorder.events()[0].current.currentFrame, 42);
}

TEST(PlaybackClockContractTest, PublishedSnapshotMatchesTheObservedSnapshot)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.setFrame(30);
    clock.setLoopRange(10, 40);
    clock.setPlaybackSpeed(1.5);
    clock.start();

    ASSERT_EQ(recorder.events().size(), 4u);
    const PlaybackClockSnapshot published = recorder.events().back().current;
    const PlaybackClockSnapshot observed = clock.snapshot();

    EXPECT_EQ(published.state, observed.state);
    EXPECT_EQ(published.currentFrame, observed.currentFrame);
    EXPECT_DOUBLE_EQ(published.frameRate, observed.frameRate);
    EXPECT_DOUBLE_EQ(published.playbackSpeed, observed.playbackSpeed);
    EXPECT_EQ(published.looping, observed.looping);
    EXPECT_EQ(published.loopStart, observed.loopStart);
    EXPECT_EQ(published.loopEnd, observed.loopEnd);
    EXPECT_EQ(published.audioSyncEnabled, observed.audioSyncEnabled);
    EXPECT_EQ(published.audioOffset.count(), observed.audioOffset.count());
    EXPECT_EQ(published.droppedFrameCount, observed.droppedFrameCount);
}

TEST(PlaybackClockContractTest,
     StateChangeObserverCanReenterClockAndPublishNestedChange)
{
    PlaybackClock clock;
    std::vector<PlaybackClockStateChangedEvent> observed;
    bool nestedChangeRequested = false;
    auto subscription = ArtifactCore::globalEventBus()
        .subscribe<PlaybackClockStateChangedEvent>(
            [&](const PlaybackClockStateChangedEvent& event) {
                observed.push_back(event);
                if (!nestedChangeRequested && event.current.currentFrame == 12) {
                    nestedChangeRequested = true;
                    clock.setFrame(24);
                }
            });
    ASSERT_TRUE(subscription.connected());

    clock.setFrame(12);

    ASSERT_EQ(observed.size(), 2u);
    EXPECT_EQ(observed[0].reason, QStringLiteral("setFrame"));
    EXPECT_EQ(observed[0].current.currentFrame, 12);
    EXPECT_EQ(observed[1].reason, QStringLiteral("setFrame"));
    EXPECT_EQ(observed[1].previous.currentFrame, 12);
    EXPECT_EQ(observed[1].current.currentFrame, 24);
    EXPECT_EQ(clock.currentFrame(), 24);
}

TEST(PlaybackClockContractTest, ResetWhilePlayingReturnsToTheStartFrame)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.setFrame(45);
    clock.start();
    recorder.clear();

    clock.reset();

    EXPECT_TRUE(clock.isStopped());
    EXPECT_EQ(clock.currentFrame(), 45);
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("reset"));
    EXPECT_EQ(recorder.events()[0].previous.state, PlaybackState::Playing);
    EXPECT_EQ(recorder.events()[0].current.state, PlaybackState::Stopped);
    EXPECT_EQ(recorder.events()[0].current.currentFrame, 45);
}

TEST(PlaybackClockContractTest, SetFrameWhilePlayingUpdatesTheSnapshotImmediately)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.start();
    recorder.clear();

    clock.setFrame(50);

    // The snapshot is captured before the next elapsed-time derived frame
    // update, so it reports the frame that was just requested.
    EXPECT_EQ(clock.snapshot().currentFrame, 50);
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("setFrame"));
    EXPECT_EQ(recorder.events()[0].current.currentFrame, 50);
}

TEST(PlaybackClockContractTest, SelfAssignmentKeepsTheClockUsable)
{
    PlaybackClock clock(FrameRate(24.0f));
    clock.setFrame(12);
    clock.setPlaybackSpeed(2.0);

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#pragma clang diagnostic ignored "-Wself-move"
#endif
    // Self assignment must be a no-op instead of releasing the owned state.
    const PlaybackClock& alias = clock;
    clock = alias;

    EXPECT_TRUE(clock.isStopped());
    EXPECT_EQ(clock.currentFrame(), 12);
    EXPECT_DOUBLE_EQ(clock.framesPerSecond(), 24.0);
    EXPECT_DOUBLE_EQ(clock.playbackSpeed(), 2.0);

    PlaybackClock&& selfReference = std::move(clock);
    clock = std::move(selfReference);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    EXPECT_TRUE(clock.isStopped());
    EXPECT_EQ(clock.currentFrame(), 12);
    EXPECT_DOUBLE_EQ(clock.playbackSpeed(), 2.0);
}

TEST(PlaybackClockContractTest, CopyingAMovedFromClockYieldsAFreshStoppedClock)
{
    PlaybackClock source;
    source.setFrame(70);

    PlaybackClock moved(std::move(source));

    // The source is moved-from and no longer owns an implementation, but
    // copying it stays well defined and yields a default constructed clock.
    PlaybackClock copyOfMoved(source);
    EXPECT_TRUE(copyOfMoved.isStopped());
    EXPECT_EQ(copyOfMoved.currentFrame(), 0);
    EXPECT_DOUBLE_EQ(copyOfMoved.framesPerSecond(), 30.0);

    // The moved-to clock keeps the original state.
    EXPECT_EQ(moved.currentFrame(), 70);
}

TEST(PlaybackClockContractTest, TimecodeFormatsHoursMinutesAndSeconds)
{
    PlaybackClock clock;

    clock.setFrame(30 * 90);           // 1 minute 30 seconds at 30 fps
    EXPECT_EQ(clock.timecode(), QStringLiteral("00:01:30:00"));

    clock.setFrame(30 * (3600 + 125)); // 1 hour 2 minutes 5 seconds at 30 fps
    EXPECT_EQ(clock.timecode(), QStringLiteral("01:02:05:00"));
    EXPECT_EQ(clock.timecodeWithSubframe(), QStringLiteral("01:02:05:00.00"));
}

TEST(PlaybackClockContractTest, DeltaTimeIsMeasuredFromTheLastUpdate)
{
    PlaybackClock clock;
    clock.start();

    const auto first = clock.deltaTime();
    EXPECT_GE(first.count(), 0);

    clock.setPlaybackSpeed(2.0);
    const auto second = clock.deltaTime();
    EXPECT_GE(second.count(), 0);
}

TEST(PlaybackClockContractTest, DeltaTimeAppliesForwardReverseAndStoppedSpeeds)
{
    PlaybackClock clock;

    // deltaTime() reports a duration scaled by playbackSpeed(), regardless
    // of whether transport has been started. Zero speed therefore freezes
    // the returned delta; reverse speed returns a non-positive delta.
    clock.setPlaybackSpeed(0.0);
    EXPECT_EQ(clock.deltaTime().count(), 0);

    clock.setPlaybackSpeed(-1.0);
    EXPECT_LE(clock.deltaTime().count(), 0);

    clock.setPlaybackSpeed(2.0);
    EXPECT_GE(clock.deltaTime().count(), 0);
}

TEST(PlaybackClockContractTest, LoopRangeEndpointsAreStoredVerbatim)
{
    EventRecorder recorder;
    ASSERT_TRUE(recorder.connected());

    PlaybackClock clock;
    clock.setLoopRange(20, 10); // reversed endpoints are accepted verbatim

    EXPECT_EQ(clock.loopStartFrame(), 20);
    EXPECT_EQ(clock.loopEndFrame(), 10);
    EXPECT_TRUE(clock.isLooping());
    ASSERT_EQ(recorder.events().size(), 1u);
    EXPECT_EQ(recorder.events()[0].reason, QStringLiteral("setLoopRange"));
}

TEST(PlaybackClockContractTest,
     ForwardAndReversePlaybackWrapLargeAudioSyncOvershootsIntoLoopRange)
{
    const auto verifyWrappedOvershoot = [](double speed) {
        PlaybackClock clock;
        clock.setFrameRate(FrameRate(1.0f));
        clock.setFrame(0);
        clock.setLoopRange(10, 20);
        clock.setPlaybackSpeed(speed);
        clock.setAudioSyncEnabled(true);
        clock.start();
        clock.syncToAudioClock(std::chrono::seconds(25));

        const auto wrappedFrame = clock.currentFrame();
        EXPECT_GE(wrappedFrame, 10);
        EXPECT_LT(wrappedFrame, 20);
    };

    verifyWrappedOvershoot(1.0);
    verifyWrappedOvershoot(-1.0);
}


TEST(PlaybackClockContractTest,
     ForwardLoopWrapsFrameDistanceAcrossSignedIntegerLimits)
{
    PlaybackClock clock(FrameRate(1.0f));
    const std::int64_t loopStart = std::numeric_limits<std::int64_t>::min();
    const std::int64_t loopEnd = loopStart + 10;

    clock.setLoopRange(loopStart, loopEnd);
    clock.setFrame(std::numeric_limits<std::int64_t>::max());

    // Keep the clock stopped: updateCurrentFrame() then uses zero elapsed time,
    // making the integer boundary case deterministic.
    const std::int64_t wrappedFrame = clock.currentFrame();

    // Mathematically, (INT64_MAX - (INT64_MIN + 10)) mod 10 == 5.
    // The loop is half-open, so the wrapped frame is start + 5.
    EXPECT_EQ(wrappedFrame, loopStart + 5);
    EXPECT_GE(wrappedFrame, loopStart);
    EXPECT_LT(wrappedFrame, loopEnd);
}

TEST(PlaybackClockContractTest,
     ReverseLoopWrapsFrameDistanceAcrossSignedIntegerLimits)
{
    PlaybackClock clock(FrameRate(1.0f));
    const std::int64_t loopEnd = std::numeric_limits<std::int64_t>::max();
    const std::int64_t loopStart = loopEnd - 10;

    clock.setLoopRange(loopStart, loopEnd);
    clock.setPlaybackSpeed(-1.0);
    clock.setFrame(std::numeric_limits<std::int64_t>::min());

    // Mathematically, (loopStart - INT64_MIN) mod 10 == 5.
    // Reverse wrap maps that distance to end - 5.
    const std::int64_t wrappedFrame = clock.currentFrame();
    EXPECT_EQ(wrappedFrame, loopEnd - 5);
    EXPECT_GE(wrappedFrame, loopStart);
    EXPECT_LT(wrappedFrame, loopEnd);
}


TEST(PlaybackClockContractTest,
     ReversePlaybackAtLoopStartWrapsToLastIncludedFrame)
{
    PlaybackClock clock(FrameRate(1.0f));
    clock.setLoopRange(10, 20);
    clock.setPlaybackSpeed(-1.0);
    clock.setFrame(10);

    // The loop is [10, 20): reverse playback at its first frame wraps to 19,
    // the last included frame, rather than the excluded end frame 20.
    EXPECT_EQ(clock.currentFrame(), 19);
}

TEST(PlaybackClockContractTest,
     ForwardPlaybackAtLoopEndWrapsToFirstIncludedFrame)
{
    PlaybackClock clock(FrameRate(1.0f));
    clock.setLoopRange(10, 20);
    clock.setFrame(20);

    // The loop interval is half-open [10, 20), so the exact excluded end
    // must wrap to the first included frame even while transport is stopped.
    EXPECT_EQ(clock.currentFrame(), 10);
    EXPECT_GE(clock.currentFrame(), clock.loopStartFrame());
    EXPECT_LT(clock.currentFrame(), clock.loopEndFrame());
}
