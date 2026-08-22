#include <gtest/gtest.h>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

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

TEST(NLEOtioRoundTripTest, NestedSequencesSurviveExportImport)
{
    NLEProjectStore store;
    const auto nestedId = store.createSequence(QStringLiteral("Nested"));
    const auto nestedTrackId = store.createTrack(nestedId);
    ASSERT_TRUE(
        store.addClip(nestedId, nestedTrackId, makeDraft(0, 12)).isValid());

    const auto parentId = store.createSequence(QStringLiteral("Parent"));
    const auto parentTrackId = store.createTrack(parentId);
    ClipDraft nestedDraft = makeDraft(0, 12);
    nestedDraft.nestedSequenceId = nestedId;
    const auto nestClipId = store.addClip(parentId, parentTrackId, nestedDraft);
    ASSERT_TRUE(nestClipId.isValid());

    const QJsonObject exported = OtioAdapter::exportTimeline(store, parentId);
    ASSERT_FALSE(exported.isEmpty());

    NLEProjectStore importedStore;
    SequenceId importedParentId;
    QVector<QString> warnings;
    ASSERT_TRUE(OtioAdapter::importTimeline(importedStore, exported,
                                            &importedParentId, &warnings));
    ASSERT_TRUE(importedParentId.isValid());

    const auto tracks = importedStore.trackIds(importedParentId);
    ASSERT_EQ(tracks.size(), 1);
    const auto clips = importedStore.clipIds(tracks.front());
    ASSERT_EQ(clips.size(), 1);
    const Clip* importedNestedClip = importedStore.clip(clips.front());
    ASSERT_NE(importedNestedClip, nullptr);
    EXPECT_TRUE(importedNestedClip->nestedSequenceId.isValid());

    const Sequence* importedNested =
        importedStore.sequence(importedNestedClip->nestedSequenceId);
    ASSERT_NE(importedNested, nullptr);
    EXPECT_EQ(importedNested->name.toStdString(), "Nested");
    const auto nestedTracks = importedStore.trackIds(importedNestedClip->nestedSequenceId);
    ASSERT_EQ(nestedTracks.size(), 1);
    EXPECT_EQ(importedStore.clipIds(nestedTracks.front()).size(), 1);
}
