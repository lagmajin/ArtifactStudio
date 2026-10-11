#include <gtest/gtest.h>

#include <QString>

import Frame.SkipTracker;

using namespace ArtifactCore;

class FrameSkipTrackerStandaloneTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        tracker = FrameSkipTracker::instance();
        tracker->setTrackingEnabled(true);
        tracker->reset();
    }

    FrameSkipTracker* tracker = nullptr;
};

TEST_F(FrameSkipTrackerStandaloneTest, ResetClearsCountersAndHistory)
{
    tracker->recordSkip(10, 9, FrameSkipReason::CacheMiss, QStringLiteral("cache"));
    tracker->reset();
    EXPECT_EQ(tracker->totalSkips(), 0);
    EXPECT_EQ(tracker->consecutiveSkips(), 0);
    EXPECT_TRUE(tracker->recentEvents().isEmpty());
    EXPECT_TRUE(tracker->skippedFrames().isEmpty());
}

TEST_F(FrameSkipTrackerStandaloneTest, SuccessfulDispatchStoresRequestCommitAndDisplay)
{
    tracker->beginDispatch(120);
    tracker->commitFrame(120);
    tracker->displayFrame(120);
    const FrameDispatchEvent event = tracker->lastEvent();
    EXPECT_EQ(event.requestedFrame, 120);
    EXPECT_EQ(event.committedFrame, 120);
    EXPECT_EQ(event.displayedFrame, 120);
    EXPECT_FALSE(event.skipped);
    EXPECT_EQ(event.skipReason, FrameSkipReason::None);
    EXPECT_EQ(tracker->totalSkips(), 0);
    EXPECT_EQ(tracker->consecutiveSkips(), 0);
}

TEST_F(FrameSkipTrackerStandaloneTest, DisplayingDifferentFrameRecordsUnknownSkip)
{
    tracker->beginDispatch(120);
    tracker->commitFrame(118);
    tracker->displayFrame(118);
    const FrameDispatchEvent event = tracker->lastEvent();
    EXPECT_EQ(event.requestedFrame, 120);
    EXPECT_EQ(event.committedFrame, 118);
    EXPECT_EQ(event.displayedFrame, 118);
    EXPECT_TRUE(event.skipped);
    EXPECT_EQ(event.skipReason, FrameSkipReason::Unknown);
    EXPECT_EQ(tracker->totalSkips(), 1);
    EXPECT_EQ(tracker->consecutiveSkips(), 1);
}

TEST_F(FrameSkipTrackerStandaloneTest, ExplicitSkipPreservesReasonDetailAndFallback)
{
    tracker->recordSkip(50, 47, FrameSkipReason::DecodePending,
                        QStringLiteral("decoder busy"));
    const auto event = tracker->lastEvent();
    EXPECT_EQ(event.requestedFrame, 50);
    EXPECT_EQ(event.committedFrame, 47);
    EXPECT_EQ(event.displayedFrame, 47);
    EXPECT_TRUE(event.skipped);
    EXPECT_EQ(event.skipReason, FrameSkipReason::DecodePending);
    EXPECT_EQ(event.skipDetail, QStringLiteral("decoder busy"));
    EXPECT_TRUE(event.timestamp.isValid());
}

TEST_F(FrameSkipTrackerStandaloneTest, SuccessfulDisplayResetsConsecutiveSkipCount)
{
    tracker->recordSkip(10, 9, FrameSkipReason::TooHeavy);
    tracker->recordSkip(11, 9, FrameSkipReason::TooHeavy);
    EXPECT_EQ(tracker->consecutiveSkips(), 2);
    tracker->beginDispatch(12);
    tracker->displayFrame(12);
    EXPECT_EQ(tracker->totalSkips(), 2);
    EXPECT_EQ(tracker->consecutiveSkips(), 0);
}

TEST_F(FrameSkipTrackerStandaloneTest, TotalSkipsPersistsAcrossSuccessfulDisplays)
{
    tracker->recordSkip(10, 9, FrameSkipReason::SeekConflict);
    tracker->beginDispatch(11);
    tracker->displayFrame(11);
    tracker->beginDispatch(12);
    tracker->displayFrame(12);
    EXPECT_EQ(tracker->totalSkips(), 1);
    EXPECT_EQ(tracker->consecutiveSkips(), 0);
}

TEST_F(FrameSkipTrackerStandaloneTest, RecentEventsReturnsNewestRequestedCount)
{
    for (int frame = 1; frame <= 4; ++frame) {
        tracker->beginDispatch(frame);
        tracker->displayFrame(frame);
    }
    const auto recent = tracker->recentEvents(2);
    ASSERT_EQ(recent.size(), 2);
    EXPECT_EQ(recent[0].requestedFrame, 3);
    EXPECT_EQ(recent[1].requestedFrame, 4);
}

TEST_F(FrameSkipTrackerStandaloneTest, RecentEventsClampsNonPositiveCountsToEmpty)
{
    tracker->beginDispatch(1);
    tracker->displayFrame(1);
    EXPECT_TRUE(tracker->recentEvents(0).isEmpty());
    EXPECT_TRUE(tracker->recentEvents(-3).isEmpty());
}

TEST_F(FrameSkipTrackerStandaloneTest, RecentEventsReturnsAllWhenCountExceedsHistory)
{
    for (int frame = 1; frame <= 3; ++frame) {
        tracker->beginDispatch(frame);
        tracker->displayFrame(frame);
    }
    EXPECT_EQ(tracker->recentEvents(10).size(), 3);
}

TEST_F(FrameSkipTrackerStandaloneTest, SkippedFramesFiltersSuccessfulEvents)
{
    tracker->recordSkip(10, 9, FrameSkipReason::CacheMiss);
    tracker->beginDispatch(11);
    tracker->displayFrame(11);
    tracker->recordSkip(12, 11, FrameSkipReason::DroppedByPolicy);
    const auto skipped = tracker->skippedFrames();
    ASSERT_EQ(skipped.size(), 2);
    EXPECT_EQ(skipped[0].requestedFrame, 10);
    EXPECT_EQ(skipped[1].requestedFrame, 12);
}

TEST_F(FrameSkipTrackerStandaloneTest, TrackingDisabledSuppressesDispatchAndExplicitSkips)
{
    tracker->setTrackingEnabled(false);
    tracker->beginDispatch(20);
    tracker->commitFrame(19);
    tracker->displayFrame(19);
    tracker->recordSkip(21, 19, FrameSkipReason::Unknown);
    EXPECT_EQ(tracker->totalSkips(), 0);
    EXPECT_EQ(tracker->consecutiveSkips(), 0);
    EXPECT_TRUE(tracker->recentEvents().isEmpty());
}

TEST_F(FrameSkipTrackerStandaloneTest, ExplicitSkipReasonDoesNotGetReplacedByDisplayLogic)
{
    tracker->recordSkip(30, 29, FrameSkipReason::CacheMiss);
    EXPECT_EQ(tracker->lastEvent().skipReason, FrameSkipReason::CacheMiss);
}

TEST_F(FrameSkipTrackerStandaloneTest, HistoryRetainsOnlyMostRecentTwoHundredEvents)
{
    for (int frame = 0; frame < 205; ++frame) {
        tracker->beginDispatch(frame);
        tracker->displayFrame(frame);
    }
    const auto history = tracker->recentEvents(500);
    ASSERT_EQ(history.size(), 200);
    EXPECT_EQ(history.front().requestedFrame, 5);
    EXPECT_EQ(history.back().requestedFrame, 204);
}

TEST_F(FrameSkipTrackerStandaloneTest, SkipTotalsRemainUnboundedByHistoryCapacity)
{
    for (int frame = 0; frame < 205; ++frame) {
        tracker->recordSkip(frame + 1, frame, FrameSkipReason::TooHeavy);
    }
    EXPECT_EQ(tracker->totalSkips(), 205);
    EXPECT_EQ(tracker->consecutiveSkips(), 205);
    EXPECT_EQ(tracker->skippedFrames().size(), 200);
}

TEST_F(FrameSkipTrackerStandaloneTest, SummaryReportsTotalAndConsecutiveCounts)
{
    tracker->recordSkip(5, 4, FrameSkipReason::TooHeavy);
    tracker->recordSkip(6, 4, FrameSkipReason::TooHeavy);
    EXPECT_EQ(tracker->summary(), QStringLiteral("FrameSkipTracker: 2 skips (2 consecutive)"));
}

TEST_F(FrameSkipTrackerStandaloneTest, EmptyLastEventIsDefaultInitialized)
{
    const auto event = tracker->lastEvent();
    EXPECT_EQ(event.requestedFrame, 0);
    EXPECT_EQ(event.committedFrame, 0);
    EXPECT_EQ(event.displayedFrame, 0);
    EXPECT_FALSE(event.skipped);
}
