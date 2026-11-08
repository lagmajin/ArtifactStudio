#include <gtest/gtest.h>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

import NLE.Core;
import NLE.OTIO;

using namespace ArtifactCore;
using namespace ArtifactCore::NLE;

namespace {

ClipDraft makeDraft(const qint64 timelineStart, const qint64 duration,
                    const qint64 sourceStart = 0)
{
    ClipDraft draft;
    draft.sourceRange = FrameRange::fromDuration(sourceStart, duration);
    draft.timelineRange = FrameRange::fromDuration(timelineStart, duration);
    draft.trimRange = draft.sourceRange;
    return draft;
}

} // namespace

TEST(NLECoreTest, TrimClipRollMovesBoundaryAcrossAdjacentClips)
{
    NLEProjectStore store;
    const auto sequenceId = store.createSequence(QStringLiteral("Roll"));
    const auto trackId = store.createTrack(sequenceId);
    const auto leftId = store.addClip(sequenceId, trackId, makeDraft(0, 20));
    const auto rightId = store.addClip(sequenceId, trackId, makeDraft(20, 20));
    ASSERT_TRUE(leftId.isValid());
    ASSERT_TRUE(rightId.isValid());

    // Extend the left clip to 30 frames; roll consumes 10 frames of the right.
    EXPECT_TRUE(store.trimClip(
        leftId, FrameRange::fromDuration(0, 30), TrimMode::Roll));

    const Clip* left = store.clip(leftId);
    const Clip* right = store.clip(rightId);
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->timelineRange.start(), 0);
    EXPECT_EQ(left->timelineRange.end(), 30);
    EXPECT_EQ(right->timelineRange.start(), 30);
    EXPECT_EQ(right->timelineRange.end(), 40);
    EXPECT_EQ(right->sourceRange.start(), 10);

    // Rolling without an adjacent clip must fail.
    EXPECT_FALSE(store.trimClip(rightId, FrameRange::fromDuration(30, 20),
                                TrimMode::Roll));
}

TEST(NLECoreTest, SlideClipKeepsSourceRangeAndAdjustsNeighbours)
{
    NLEProjectStore store;
    const auto sequenceId = store.createSequence(QStringLiteral("Slide"));
    const auto trackId = store.createTrack(sequenceId);
    const auto firstId = store.addClip(sequenceId, trackId, makeDraft(0, 20));
    const auto movingId = store.addClip(sequenceId, trackId, makeDraft(20, 20, 100));
    const auto lastId = store.addClip(sequenceId, trackId, makeDraft(40, 20));
    ASSERT_TRUE(firstId.isValid());
    ASSERT_TRUE(movingId.isValid());
    ASSERT_TRUE(lastId.isValid());

    // Slide the middle clip from [20,40) to [35,55).
    // The left neighbour extends by 15 (its source out-point advances), the
    // right neighbour loses 15 head frames (its source in-point advances).
    EXPECT_TRUE(store.slideClip(movingId, FramePosition(35)));

    const Clip* first = store.clip(firstId);
    const Clip* moving = store.clip(movingId);
    const Clip* last = store.clip(lastId);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(moving, nullptr);
    ASSERT_NE(last, nullptr);

    EXPECT_EQ(first->timelineRange.end(), 35);
    EXPECT_EQ(first->sourceRange.end(), 35);
    EXPECT_EQ(moving->timelineRange.start(), 35);
    EXPECT_EQ(moving->timelineRange.duration(), 20);
    EXPECT_EQ(moving->sourceRange.start(), 100);
    EXPECT_EQ(last->timelineRange.start(), 55);
    EXPECT_EQ(last->sourceRange.start(), 15);
}

TEST(NLECoreTest, EditHistoryUndoRedoRestoresState)
{
    NLEProjectStore store;
    NLEEditHistory history(&store);
    const auto sequenceId = store.createSequence(QStringLiteral("History"));
    const auto trackId = store.createTrack(sequenceId);

    history.capture(QStringLiteral("Add clip"));
    const auto clipId = store.addClip(sequenceId, trackId, makeDraft(0, 10));
    ASSERT_TRUE(clipId.isValid());

    ASSERT_TRUE(history.canUndo());
    EXPECT_EQ(history.undoLabel().toStdString(), "Add clip");
    ASSERT_TRUE(history.undo());
    EXPECT_EQ(store.clipIds(trackId).size(), 0);

    ASSERT_TRUE(history.canRedo());
    ASSERT_TRUE(history.redo());
    ASSERT_NE(store.clip(clipId), nullptr);
}

TEST(NLECoreTest, EditHistoryBoundsDepthAndDropsRedoBranchAfterNewCapture)
{
    NLEProjectStore store;
    NLEEditHistory history(&store);
    history.setMaxDepth(2);
    const auto sequenceId = store.createSequence(QStringLiteral("History branch"));
    const auto trackId = store.createTrack(sequenceId);

    history.capture(QStringLiteral("Add first"));
    const auto firstId = store.addClip(sequenceId, trackId, makeDraft(0, 10));
    history.capture(QStringLiteral("Add second"));
    const auto secondId = store.addClip(sequenceId, trackId, makeDraft(10, 10));
    history.capture(QStringLiteral("Add third"));
    const auto thirdId = store.addClip(sequenceId, trackId, makeDraft(20, 10));
    ASSERT_TRUE(firstId.isValid());
    ASSERT_TRUE(secondId.isValid());
    ASSERT_TRUE(thirdId.isValid());
    ASSERT_EQ(history.depth(), 2u);
    ASSERT_EQ(store.clipIds(trackId).size(), 3);

    EXPECT_EQ(history.undoLabel(), QStringLiteral("Add third"));
    ASSERT_TRUE(history.undo());
    EXPECT_EQ(store.clipIds(trackId).size(), 2);
    EXPECT_EQ(history.undoLabel(), QStringLiteral("Add second"));
    ASSERT_TRUE(history.undo());
    EXPECT_EQ(store.clipIds(trackId).size(), 1);
    EXPECT_FALSE(history.canUndo())
        << "the oldest captured state must have been discarded at max depth";

    EXPECT_EQ(history.redoLabel(), QStringLiteral("Add second"));
    ASSERT_TRUE(history.redo());
    EXPECT_EQ(store.clipIds(trackId).size(), 2);
    ASSERT_TRUE(history.canRedo());

    history.capture(QStringLiteral("Branch edit"));
    const auto branchId = store.addClip(sequenceId, trackId, makeDraft(20, 5));
    ASSERT_TRUE(branchId.isValid());
    EXPECT_FALSE(history.canRedo())
        << "capturing a new edit after undo must invalidate the old redo branch";
    EXPECT_EQ(store.clipIds(trackId).size(), 3);

    ASSERT_TRUE(history.undo());
    EXPECT_EQ(store.clipIds(trackId).size(), 2);
    ASSERT_TRUE(history.canRedo());
    ASSERT_TRUE(history.redo());
    EXPECT_EQ(store.clipIds(trackId).size(), 3);
    ASSERT_NE(store.clip(branchId), nullptr);
}

TEST(NLECoreTest, SwitchingProjectStoresClearsBothHistoryBranches)
{
    NLEProjectStore firstStore;
    NLEEditHistory history(&firstStore);
    const auto firstSequence = firstStore.createSequence(QStringLiteral("First project"));
    const auto firstTrack = firstStore.createTrack(firstSequence);

    history.capture(QStringLiteral("Before first clip"));
    const auto firstClip = firstStore.addClip(
        firstSequence, firstTrack, makeDraft(0, 10));
    history.capture(QStringLiteral("Before second clip"));
    const auto secondClip = firstStore.addClip(
        firstSequence, firstTrack, makeDraft(10, 10));
    ASSERT_TRUE(firstClip.isValid());
    ASSERT_TRUE(secondClip.isValid());

    ASSERT_TRUE(history.undo());
    EXPECT_EQ(firstStore.clipIds(firstTrack).size(), 1);
    ASSERT_TRUE(history.canUndo());
    ASSERT_TRUE(history.canRedo());

    NLEProjectStore secondStore;
    const auto secondSequence = secondStore.createSequence(QStringLiteral("Second project"));
    const auto secondTrack = secondStore.createTrack(secondSequence);
    history.setStore(&secondStore);
    EXPECT_FALSE(history.canUndo());
    EXPECT_FALSE(history.canRedo());
    EXPECT_EQ(firstStore.clipIds(firstTrack).size(), 1);
    EXPECT_EQ(secondStore.clipIds(secondTrack).size(), 0);

    history.capture(QStringLiteral("Second project edit"));
    const auto secondProjectClip = secondStore.addClip(
        secondSequence, secondTrack, makeDraft(20, 5));
    ASSERT_TRUE(secondProjectClip.isValid());
    ASSERT_TRUE(history.undo());
    EXPECT_EQ(secondStore.clipIds(secondTrack).size(), 0);
    ASSERT_TRUE(history.canRedo());

    history.setStore(&secondStore);
    EXPECT_TRUE(history.canRedo())
        << "setting the same store must preserve its current history branch";
    ASSERT_TRUE(history.redo());
    ASSERT_NE(secondStore.clip(secondProjectClip), nullptr);
    EXPECT_EQ(firstStore.clipIds(firstTrack).size(), 1);
}

TEST(NLECoreTest, RemoveClipCleansTransitionsMarkersAndLinkGroups)
{
    NLEProjectStore store;
    const auto sequenceId = store.createSequence(QStringLiteral("Cascade"));
    const auto trackId = store.createTrack(sequenceId);
    const auto leftId = store.addClip(sequenceId, trackId, makeDraft(0, 10));
    const auto rightId = store.addClip(sequenceId, trackId, makeDraft(10, 10));
    ASSERT_TRUE(leftId.isValid());
    ASSERT_TRUE(rightId.isValid());
    const auto transitionId =
        store.createTransition(trackId, leftId, rightId,
                               FrameRange::fromDuration(5, 10));
    ASSERT_TRUE(transitionId.isValid());
    Clip* left = store.clip(leftId);
    ASSERT_NE(left, nullptr);
    const auto clipMarkerId = store.createMarker(
        sequenceId, FramePosition(2), QStringLiteral("clip marker"));
    left->markers.push_back(clipMarkerId);
    const auto groupId = store.createLinkGroup();
    ASSERT_TRUE(store.addClipToLinkGroup(leftId, groupId));

    EXPECT_TRUE(store.removeClip(leftId));

    EXPECT_FALSE(store.transition(transitionId));
    EXPECT_FALSE(store.marker(clipMarkerId));
    const ClipLinkGroup* group = store.linkGroup(groupId);
    ASSERT_NE(group, nullptr);
    EXPECT_TRUE(group->members.isEmpty());
    EXPECT_EQ(store.transitions(trackId).size(), 0);
}

TEST(NLECoreTest, TimecodeRoundTripHandlesDropFrame)
{
    TimeBase plain;
    plain.numerator = 1;
    plain.denominator = 30;
    EXPECT_EQ(plain.timecodeString(3723), QStringLiteral("00:02:04:03"));
    bool ok = false;
    EXPECT_EQ(plain.frameFromTimecode(QStringLiteral("00:02:04:03"), &ok), 3723);
    EXPECT_TRUE(ok);

    TimeBase drop;
    drop.numerator = 1001;
    drop.denominator = 30000;
    drop.dropFrame = true;
    // The classic 29.97DF hour boundary: 01:00:00;00 == frame 107892.
    EXPECT_EQ(drop.timecodeString(107892), QStringLiteral("01:00:00;00"));
    EXPECT_EQ(drop.frameFromTimecode(QStringLiteral("01:00:00;00"), &ok), 107892);
    EXPECT_TRUE(ok);

    // Round-trip across several arbitrary frames.
    for (const std::int64_t frame : {std::int64_t{0}, std::int64_t{1799},
                                     std::int64_t{1800}, std::int64_t{53999},
                                     std::int64_t{107892}, std::int64_t{215784}}) {
        const bool roundTripOk =
            drop.frameFromTimecode(drop.timecodeString(frame)) == frame;
        EXPECT_TRUE(roundTripOk);
    }
}

TEST(NLECoreTest, ClipTimeMappingHonorsSpeedAndReversal)
{
    Clip clip;
    clip.timelineRange = FrameRange::fromDuration(0, 20);
    clip.sourceRange = FrameRange::fromDuration(100, 20);

    clip.speed = 2.0;
    EXPECT_EQ(timelineFrameToSourceFrame(clip, 0), 100);
    EXPECT_EQ(timelineFrameToSourceFrame(clip, 5), 110);
    EXPECT_EQ(timelineFrameToSourceFrame(clip, 10), 120 - 1);
    EXPECT_EQ(sourceFrameToTimelineFrame(clip, 110), 5);

    clip.reversed = true;
    EXPECT_EQ(timelineFrameToSourceFrame(clip, 0), 119);
    EXPECT_EQ(timelineFrameToSourceFrame(clip, 5), 109);
    EXPECT_EQ(sourceFrameToTimelineFrame(clip, 119), 0);
    EXPECT_EQ(sourceFrameToTimelineFrame(clip, 109), 5);
}

TEST(NLECoreTest, ClipTimeMappingMatchesFractionalSpeedRoundingGrid)
{
    struct MappingCase {
        double speed;
        std::array<std::int64_t, 6> sourceOffsets;
        std::array<std::int64_t, 10> timelineOffsets;
    };
    const std::array<MappingCase, 4> cases = {{
        {0.5, {0, 1, 1, 2, 2, 3}, {0, 2, 4, 5, 5, 5, 5, 5, 5, 5}},
        {1.0, {0, 1, 2, 3, 4, 5}, {0, 1, 2, 3, 4, 5, 5, 5, 5, 5}},
        {1.5, {0, 2, 3, 5, 6, 8}, {0, 1, 1, 2, 3, 3, 4, 5, 5, 5}},
        {2.5, {0, 3, 5, 8, 9, 9}, {0, 0, 1, 1, 2, 2, 2, 3, 3, 4}},
    }};

    constexpr std::int64_t timelineStart = 50;
    constexpr std::int64_t timelineDuration = 6;
    constexpr std::int64_t sourceStart = 100;
    constexpr std::int64_t sourceDuration = 10;

    for (const MappingCase& mapping : cases) {
        for (const bool reversed : {false, true}) {
            Clip clip;
            clip.timelineRange =
                FrameRange::fromDuration(timelineStart, timelineDuration);
            clip.sourceRange = FrameRange::fromDuration(sourceStart, sourceDuration);
            clip.speed = mapping.speed;
            clip.reversed = reversed;

            for (std::int64_t timelineOffset = 0;
                 timelineOffset < timelineDuration; ++timelineOffset) {
                const std::int64_t sourceOffset =
                    mapping.sourceOffsets[static_cast<std::size_t>(timelineOffset)];
                const std::int64_t expectedSource = reversed
                    ? sourceStart + sourceDuration - 1 - sourceOffset
                    : sourceStart + sourceOffset;
                EXPECT_EQ(timelineFrameToSourceFrame(
                              clip, timelineStart + timelineOffset),
                          expectedSource)
                    << "speed=" << mapping.speed << " reversed=" << reversed
                    << " timelineOffset=" << timelineOffset;
            }

            for (std::int64_t sourceOffset = 0;
                 sourceOffset < sourceDuration; ++sourceOffset) {
                const std::int64_t timelineOffset =
                    mapping.timelineOffsets[static_cast<std::size_t>(sourceOffset)];
                const std::int64_t sourceFrame = reversed
                    ? sourceStart + sourceDuration - 1 - sourceOffset
                    : sourceStart + sourceOffset;
                EXPECT_EQ(sourceFrameToTimelineFrame(clip, sourceFrame),
                          timelineStart + timelineOffset)
                    << "speed=" << mapping.speed << " reversed=" << reversed
                    << " sourceOffset=" << sourceOffset;
            }

            const std::int64_t firstMappedOffset = mapping.sourceOffsets.front();
            const std::int64_t lastMappedOffset = mapping.sourceOffsets.back();
            EXPECT_EQ(timelineFrameToSourceFrame(clip, timelineStart - 100),
                      reversed ? sourceStart + sourceDuration - 1 - firstMappedOffset
                               : sourceStart + firstMappedOffset);
            EXPECT_EQ(timelineFrameToSourceFrame(
                          clip, timelineStart + timelineDuration + 100),
                      reversed ? sourceStart + sourceDuration - 1 - lastMappedOffset
                               : sourceStart + lastMappedOffset);
        }
    }
}

TEST(NLECoreTest, InvalidClipSpeedsFallBackToUnitRateInBothMappingDirections)
{
    Clip clip;
    clip.timelineRange = FrameRange::fromDuration(50, 6);
    clip.sourceRange = FrameRange::fromDuration(100, 10);
    const double invalidSpeeds[] = {
        0.0,
        -2.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };

    for (const double speed : invalidSpeeds) {
        clip.speed = speed;
        EXPECT_EQ(timelineFrameToSourceFrame(clip, 53), 103)
            << "speed=" << speed;
        EXPECT_EQ(sourceFrameToTimelineFrame(clip, 104), 54)
            << "speed=" << speed;
    }
}

TEST(NLECoreTest, ConformServiceAppliesAvailableRange)
{
    NLEProjectStore store;
    const auto sequenceId = store.createSequence(QStringLiteral("Conform"));
    const auto trackId = store.createTrack(sequenceId);

    SourceRef source;
    source.uri = QStringLiteral("Z:/media/clipA.mov");
    source.displayName = QStringLiteral("clipA");
    source.availableRange = FrameRange::fromDuration(0, 10);
    const auto sourceId = store.registerSource(source);
    ASSERT_TRUE(sourceId.isValid());

    ClipDraft draft = makeDraft(0, 20);
    draft.sourceId = sourceId;
    const auto clipId = store.addClip(sequenceId, trackId, draft);
    ASSERT_TRUE(clipId.isValid());

    ConformService conform(&store);
    const ConformReport report = conform.conformSequence(sequenceId);
    EXPECT_TRUE(report.success);
    ASSERT_EQ(report.updatedClips.size(), 1);
    const Clip* conformed = store.clip(clipId);
    ASSERT_NE(conformed, nullptr);
    EXPECT_EQ(conformed->sourceRange.start(), 0);
    EXPECT_EQ(conformed->sourceRange.end(), 10);

    // Trims that exceed availability are rejected.
    EXPECT_FALSE(store.trimClip(clipId, FrameRange::fromDuration(0, 20),
                                TrimMode::Source));
    EXPECT_TRUE(store.trimClip(clipId, FrameRange::fromDuration(0, 8),
                               TrimMode::Source));

    // Offline media surfaces as unresolved and flips the report.
    ASSERT_TRUE(store.setSourceAvailability(sourceId, false));
    const ConformReport offlineReport = conform.conformSequence(sequenceId);
    EXPECT_FALSE(offlineReport.success);
    EXPECT_EQ(offlineReport.unresolvedClips.size(), 1);
}

TEST(NLECoreTest, ClipResolverUsesOnlineProxyAndClipsBothSourceRangeEnds)
{
    NLEProjectStore store;
    const auto sequenceId = store.createSequence(QStringLiteral("Proxy Resolve"));
    const auto trackId = store.createTrack(sequenceId);
    SourceRef source;
    source.uri = QStringLiteral("Z:/media/original.mov");
    source.online = false;
    source.proxyAvailable = true;
    source.proxyOnline = true;
    source.useProxy = true;
    source.proxyUri = QStringLiteral("Z:/proxy/original-proxy.mov");
    source.availableRange = FrameRange::fromDuration(90, 20);
    const auto sourceId = store.registerSource(source);
    ASSERT_TRUE(sourceId.isValid());

    ClipDraft draft = makeDraft(0, 40, 80);
    draft.sourceId = sourceId;
    draft.speed = 1.5;
    draft.reversed = true;
    const auto clipId = store.addClip(sequenceId, trackId, draft);
    ASSERT_TRUE(clipId.isValid());

    const ClipResolution resolution = ClipResolver(&store).resolveClip(clipId);

    EXPECT_TRUE(resolution.online);
    EXPECT_TRUE(resolution.useProxy);
    EXPECT_EQ(resolution.source.uri, QStringLiteral("Z:/media/original.mov"));
    EXPECT_EQ(resolution.source.proxyUri,
              QStringLiteral("Z:/proxy/original-proxy.mov"));
    EXPECT_EQ(resolution.sourceRange, FrameRange::fromDuration(80, 40));
    EXPECT_EQ(resolution.effectiveRange, FrameRange::fromDuration(90, 20));
    EXPECT_DOUBLE_EQ(resolution.speed, 1.5);
    EXPECT_TRUE(resolution.reversed);
    EXPECT_EQ(resolution.diagnostic,
              QStringLiteral("Source range clipped to available range"));
}

TEST(NLECoreTest, MissingNestedSequenceIsConsistentAcrossResolutionAndValidation)
{
    NLEProjectStore store;
    const auto sequenceId = store.createSequence(QStringLiteral("Parent"));
    const auto trackId = store.createTrack(sequenceId);
    SourceRef source;
    source.uri = QStringLiteral("Z:/media/placeholder.mov");
    const auto sourceId = store.registerSource(source);
    ASSERT_TRUE(sourceId.isValid());

    ClipDraft draft = makeDraft(0, 10);
    draft.sourceId = sourceId;
    draft.nestedSequenceId = SequenceId{999999};
    const auto clipId = store.addClip(sequenceId, trackId, draft);
    ASSERT_TRUE(clipId.isValid());

    const ClipResolution resolution = ClipResolver(&store).resolveClip(clipId);
    EXPECT_FALSE(resolution.online);
    EXPECT_FALSE(resolution.useProxy);
    EXPECT_EQ(resolution.diagnostic,
              QStringLiteral("Nested sequence not found"));

    const NLEValidationReport report = store.validate();
    EXPECT_FALSE(report.success);
    EXPECT_EQ(report.invalidClipCount, 1);
    const bool reportedMissingNestedSequence = std::any_of(
        report.issues.cbegin(), report.issues.cend(), [](const auto& issue) {
            return issue.code == QStringLiteral("missing_nested_sequence");
        });
    EXPECT_TRUE(reportedMissingNestedSequence);
}

TEST(NLECoreTest, CreateTransitionRejectsCrossTrackPairs)
{
    NLEProjectStore store;
    const auto sequenceId = store.createSequence(QStringLiteral("XTrack"));
    const auto videoTrackId = store.createTrack(sequenceId);
    const auto audioTrackId = store.createTrack(sequenceId, TrackKind::Audio);
    const auto videoClipId =
        store.addClip(sequenceId, videoTrackId, makeDraft(0, 10));
    const auto audioClipId =
        store.addClip(sequenceId, audioTrackId, makeDraft(0, 10));
    ASSERT_TRUE(videoClipId.isValid());
    ASSERT_TRUE(audioClipId.isValid());

    EXPECT_FALSE(store
                     .createTransition(videoTrackId, videoClipId, audioClipId,
                                       FrameRange::fromDuration(5, 5))
                     .isValid());
    EXPECT_FALSE(store
                     .createTransition(videoTrackId, videoClipId, videoClipId,
                                       FrameRange::fromDuration(5, 0))
                     .isValid());
}

TEST(NLECoreTest, TransitionEditsPreserveClipGeometryAndReportAffectedObjects)
{
    NLEProjectStore store;
    const auto sequenceId = store.createSequence(QStringLiteral("TransitionEdit"));
    const auto trackId = store.createTrack(sequenceId);
    const auto leftId = store.addClip(sequenceId, trackId, makeDraft(0, 24));
    const auto rightId = store.addClip(sequenceId, trackId, makeDraft(24, 24));
    ASSERT_TRUE(leftId.isValid());
    ASSERT_TRUE(rightId.isValid());

    const FrameRange transitionRange = FrameRange::fromDuration(18, 12);
    const auto transitionId = store.createTransition(
        trackId, leftId, rightId, transitionRange, TransitionKind::Crossfade,
        12.0);
    ASSERT_TRUE(transitionId.isValid());
    SequenceEditor editor(&store);

    const EditResult durationResult =
        editor.setTransitionDuration(transitionId, 8.0);
    ASSERT_TRUE(durationResult.success);
    EXPECT_EQ(durationResult.touchedClips.size(), 2);
    EXPECT_EQ(durationResult.touchedClips[0], leftId);
    EXPECT_EQ(durationResult.touchedClips[1], rightId);
    ASSERT_EQ(durationResult.touchedTracks.size(), 1);
    EXPECT_EQ(durationResult.touchedTracks[0], trackId);
    ASSERT_EQ(durationResult.touchedSequences.size(), 1);
    EXPECT_EQ(durationResult.touchedSequences[0], sequenceId);

    const Transition* transition = store.transition(transitionId);
    ASSERT_NE(transition, nullptr);
    EXPECT_DOUBLE_EQ(transition->duration, 8.0);
    EXPECT_EQ(transition->range.start(), transitionRange.start());
    EXPECT_EQ(transition->range.end(), transitionRange.end());

    const EditResult kindResult =
        editor.setTransitionKind(transitionId, TransitionKind::IrisWipe);
    ASSERT_TRUE(kindResult.success);
    transition = store.transition(transitionId);
    ASSERT_NE(transition, nullptr);
    EXPECT_EQ(transition->kind, TransitionKind::IrisWipe);
    EXPECT_DOUBLE_EQ(transition->duration, 8.0);
    EXPECT_EQ(transition->range.start(), transitionRange.start());
    EXPECT_EQ(transition->range.end(), transitionRange.end());

    const Clip* left = store.clip(leftId);
    const Clip* right = store.clip(rightId);
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->timelineRange.start(), 0);
    EXPECT_EQ(left->timelineRange.end(), 24);
    EXPECT_EQ(right->timelineRange.start(), 24);
    EXPECT_EQ(right->timelineRange.end(), 48);
}

TEST(NLEOtioRoundTripTest, TransitionsSpeedMarkersSubtitlesSurvive)
{
    NLEProjectStore store;
    TimeBase timeBase;
    timeBase.numerator = 1;
    timeBase.denominator = 24;
    const auto sequenceId = store.createSequence(QStringLiteral("RoundTrip"), timeBase);
    const auto trackId = store.createTrack(sequenceId);
    const auto leftId = store.addClip(sequenceId, trackId, makeDraft(0, 24));
    ClipDraft fastDraft = makeDraft(24, 24);
    fastDraft.speed = 2.0;
    fastDraft.reversed = true;
    const auto rightId = store.addClip(sequenceId, trackId, fastDraft);
    ASSERT_TRUE(leftId.isValid());
    ASSERT_TRUE(rightId.isValid());

    const TransitionId transitionId = store.createTransition(
        trackId, leftId, rightId,
        FrameRange::fromDuration(18, 12),
        TransitionKind::IrisWipe, 12.0);
    ASSERT_TRUE(transitionId.isValid());
    store.createMarker(sequenceId, FramePosition(6), QStringLiteral("Chapter"),
                       QStringLiteral("intro"), QColor(1, 2, 3, 4));

    Sequence* sequence = store.sequence(sequenceId);
    ASSERT_NE(sequence, nullptr);
    SubtitleCue cue;
    cue.range = FrameRange::fromDuration(0, 12);
    cue.text = QStringLiteral("Hello");
    sequence->subtitles.push_back(cue);

    const QJsonObject exported = OtioAdapter::exportTimeline(store, sequenceId);
    ASSERT_FALSE(exported.isEmpty());

    // Subtitles live under metadata for standards-safe interchange.
    EXPECT_TRUE(exported.value(QStringLiteral("metadata")).toObject()
                    .contains(QStringLiteral("artifactSubtitles")));

    NLEProjectStore importedStore;
    QVector<QString> warnings;
    SequenceId importedSequenceId;
    ASSERT_TRUE(OtioAdapter::importTimeline(importedStore, exported,
                                            &importedSequenceId, &warnings));
    ASSERT_TRUE(importedSequenceId.isValid());

    const Sequence* importedSequence = importedStore.sequence(importedSequenceId);
    ASSERT_NE(importedSequence, nullptr);
    EXPECT_EQ(importedSequence->timeBase.denominator, 24);
    ASSERT_EQ(importedSequence->subtitles.size(), 1);
    EXPECT_EQ(importedSequence->subtitles.front().text.toStdString(), "Hello");

    const auto importedTracks = importedStore.trackIds(importedSequenceId);
    ASSERT_EQ(importedTracks.size(), 1);
    const auto importedClips = importedStore.clipIds(importedTracks.front());
    ASSERT_EQ(importedClips.size(), 2);

    const Clip* importedLeft = importedStore.clip(importedClips[0]);
    const Clip* importedRight = importedStore.clip(importedClips[1]);
    ASSERT_NE(importedLeft, nullptr);
    ASSERT_NE(importedRight, nullptr);
    EXPECT_DOUBLE_EQ(importedRight->speed, 2.0);
    EXPECT_TRUE(importedRight->reversed);

    const auto importedTransitions =
        importedStore.transitionsForClip(importedClips[0]);
    ASSERT_EQ(importedTransitions.size(), 1);
    EXPECT_EQ(importedTransitions.front()->kind, TransitionKind::IrisWipe);

    bool markerFound = false;
    for (const MarkerId& markerId : importedSequence->markers) {
        const Marker* marker = importedStore.marker(markerId);
        if (marker && marker->name == QStringLiteral("Chapter")) {
            markerFound = true;
            EXPECT_EQ(marker->color.red(), 1);
            EXPECT_EQ(marker->color.green(), 2);
            EXPECT_EQ(marker->color.blue(), 3);
            EXPECT_EQ(marker->color.alpha(), 4);
        }
    }
    EXPECT_TRUE(markerFound);
}

TEST(NLEOtioRoundTripTest, SpeedWarpKeepsTimelineDurationAndFollowingClipPosition)
{
    NLEProjectStore store;
    TimeBase timeBase;
    timeBase.numerator = 1;
    timeBase.denominator = 24;
    const auto sequenceId = store.createSequence(QStringLiteral("Time Warp"), timeBase);
    const auto trackId = store.createTrack(sequenceId);
    ASSERT_TRUE(store.addClip(sequenceId, trackId, makeDraft(0, 24)).isValid());

    ClipDraft fastDraft;
    fastDraft.sourceRange = FrameRange::fromDuration(100, 48);
    fastDraft.timelineRange = FrameRange::fromDuration(24, 24);
    fastDraft.trimRange = fastDraft.sourceRange;
    fastDraft.speed = 2.0;
    fastDraft.reversed = true;
    ASSERT_TRUE(store.addClip(sequenceId, trackId, fastDraft).isValid());
    ASSERT_TRUE(store.addClip(sequenceId, trackId, makeDraft(48, 24)).isValid());

    const QJsonObject exported = OtioAdapter::exportTimeline(store, sequenceId);
    ASSERT_FALSE(exported.isEmpty());

    NLEProjectStore importedStore;
    SequenceId importedSequenceId;
    QVector<QString> warnings;
    ASSERT_TRUE(OtioAdapter::importTimeline(importedStore, exported,
                                            &importedSequenceId, &warnings));
    EXPECT_TRUE(warnings.isEmpty());
    const auto importedTracks = importedStore.trackIds(importedSequenceId);
    ASSERT_EQ(importedTracks.size(), 1);
    const auto importedClips = importedStore.clipIds(importedTracks.front());
    ASSERT_EQ(importedClips.size(), 3);

    const Clip* importedFast = importedStore.clip(importedClips[1]);
    const Clip* importedFollowing = importedStore.clip(importedClips[2]);
    ASSERT_NE(importedFast, nullptr);
    ASSERT_NE(importedFollowing, nullptr);
    EXPECT_EQ(importedFast->sourceRange, FrameRange::fromDuration(100, 48));
    EXPECT_EQ(importedFast->timelineRange, FrameRange::fromDuration(24, 24));
    EXPECT_DOUBLE_EQ(importedFast->speed, 2.0);
    EXPECT_TRUE(importedFast->reversed);
    EXPECT_EQ(importedFollowing->timelineRange, FrameRange::fromDuration(48, 24));
}

TEST(NLEOtioRoundTripTest, NestedSequencesSurviveExportImport)
{
    NLEProjectStore store;
    const auto leafId = store.createSequence(QStringLiteral("Leaf"));
    const auto leafVideoTrackId =
        store.createTrack(leafId, TrackKind::Video, QStringLiteral("Leaf Video"));
    const auto leafAudioTrackId =
        store.createTrack(leafId, TrackKind::Audio, QStringLiteral("Leaf Audio"));
    ASSERT_TRUE(store.addClip(leafId, leafVideoTrackId, makeDraft(0, 12)).isValid());
    ASSERT_TRUE(store.addClip(leafId, leafAudioTrackId, makeDraft(0, 8)).isValid());

    const auto nestedId = store.createSequence(QStringLiteral("Nested"));
    const auto nestedVideoTrackId = store.createTrack(
        nestedId, TrackKind::Video, QStringLiteral("Nested Video"));
    const auto nestedAudioTrackId = store.createTrack(
        nestedId, TrackKind::Audio, QStringLiteral("Nested Audio"));
    ClipDraft leafDraft = makeDraft(5, 12);
    leafDraft.nestedSequenceId = leafId;
    ASSERT_TRUE(store.addClip(nestedId, nestedVideoTrackId, leafDraft).isValid());
    ASSERT_TRUE(store.addClip(nestedId, nestedAudioTrackId, makeDraft(0, 6)).isValid());

    const auto parentId = store.createSequence(QStringLiteral("Parent"));
    const auto parentAudioTrackId = store.createTrack(
        parentId, TrackKind::Audio, QStringLiteral("Parent Audio"));
    const auto parentVideoTrackId = store.createTrack(
        parentId, TrackKind::Video, QStringLiteral("Parent Video"));
    ASSERT_TRUE(store.addClip(parentId, parentAudioTrackId,
                              makeDraft(0, 4)).isValid());
    ClipDraft nestedDraft = makeDraft(3, 21);
    nestedDraft.nestedSequenceId = nestedId;
    const auto nestClipId = store.addClip(parentId, parentVideoTrackId, nestedDraft);
    ASSERT_TRUE(nestClipId.isValid());

    const QJsonObject exported = OtioAdapter::exportTimeline(store, parentId);
    ASSERT_FALSE(exported.isEmpty());

    NLEProjectStore importedStore;
    SequenceId importedParentId;
    QVector<QString> warnings;
    ASSERT_TRUE(OtioAdapter::importTimeline(importedStore, exported,
                                            &importedParentId, &warnings));
    ASSERT_TRUE(importedParentId.isValid());
    EXPECT_TRUE(warnings.isEmpty());

    const auto tracks = importedStore.trackIds(importedParentId);
    ASSERT_EQ(tracks.size(), 2);
    ASSERT_NE(importedStore.track(tracks[0]), nullptr);
    ASSERT_NE(importedStore.track(tracks[1]), nullptr);
    EXPECT_EQ(importedStore.track(tracks[0])->name, QStringLiteral("Parent Audio"));
    EXPECT_EQ(importedStore.track(tracks[1])->name, QStringLiteral("Parent Video"));
    EXPECT_EQ(importedStore.track(tracks[0])->kind, TrackKind::Audio);
    EXPECT_EQ(importedStore.track(tracks[1])->kind, TrackKind::Video);
    ASSERT_EQ(importedStore.clipIds(tracks[0]).size(), 1);
    const auto parentVideoClips = importedStore.clipIds(tracks[1]);
    ASSERT_EQ(parentVideoClips.size(), 1);
    const Clip* importedNestedClip = importedStore.clip(parentVideoClips.front());
    ASSERT_NE(importedNestedClip, nullptr);
    EXPECT_TRUE(importedNestedClip->nestedSequenceId.isValid());
    EXPECT_EQ(importedNestedClip->timelineRange,
              FrameRange::fromDuration(3, 21));

    const Sequence* importedNested =
        importedStore.sequence(importedNestedClip->nestedSequenceId);
    ASSERT_NE(importedNested, nullptr);
    EXPECT_EQ(importedNested->name.toStdString(), "Nested");
    const auto nestedTracks = importedStore.trackIds(importedNestedClip->nestedSequenceId);
    ASSERT_EQ(nestedTracks.size(), 2);
    ASSERT_NE(importedStore.track(nestedTracks[0]), nullptr);
    ASSERT_NE(importedStore.track(nestedTracks[1]), nullptr);
    EXPECT_EQ(importedStore.track(nestedTracks[0])->name,
              QStringLiteral("Nested Video"));
    EXPECT_EQ(importedStore.track(nestedTracks[1])->name,
              QStringLiteral("Nested Audio"));
    const auto nestedVideoClips = importedStore.clipIds(nestedTracks[0]);
    ASSERT_EQ(nestedVideoClips.size(), 1);
    const Clip* importedLeafClip = importedStore.clip(nestedVideoClips.front());
    ASSERT_NE(importedLeafClip, nullptr);
    EXPECT_EQ(importedLeafClip->timelineRange, FrameRange::fromDuration(5, 12));
    ASSERT_TRUE(importedLeafClip->nestedSequenceId.isValid());
    const Sequence* importedLeaf =
        importedStore.sequence(importedLeafClip->nestedSequenceId);
    ASSERT_NE(importedLeaf, nullptr);
    EXPECT_EQ(importedLeaf->name.toStdString(), "Leaf");
    const auto leafTracks = importedStore.trackIds(importedLeafClip->nestedSequenceId);
    ASSERT_EQ(leafTracks.size(), 2);
    ASSERT_NE(importedStore.track(leafTracks[0]), nullptr);
    ASSERT_NE(importedStore.track(leafTracks[1]), nullptr);
    EXPECT_EQ(importedStore.track(leafTracks[0])->name, QStringLiteral("Leaf Video"));
    EXPECT_EQ(importedStore.track(leafTracks[1])->name, QStringLiteral("Leaf Audio"));
    EXPECT_EQ(importedStore.clipIds(leafTracks[0]).size(), 1);
    EXPECT_EQ(importedStore.clipIds(leafTracks[1]).size(), 1);
}
