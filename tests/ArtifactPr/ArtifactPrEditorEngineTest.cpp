// ArtifactPr EditorEngine contract test.
//
// EditorEngine is the single document/undo/render owner shared by the GUI, the
// CLI and the exporter. These cases pin the parts of that contract that are
// observable without a GPU device or a real media file: the new-project
// invariants, in/out point clamping, render plan construction (quality preset,
// range clamp, audio mute/solo/fx filter), snapping, the NLE snapshot and the
// project save/load round trip.
#include <gtest/gtest.h>

#include <QFile>
#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>
#include <QVector>

#include <cstring>

#include "ArtifactPrTestApp.h"

import ArtifactPr.EditorEngine;

namespace {

[[maybe_unused]] const bool artifactPrApplicationRegistered =
    ArtifactPrTest::registerApplicationEnvironment();

ArtifactPr::EditorEngine* freshEngine()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->newProject();
    return engine;
}

ArtifactPr::DemoClip makeClip(const QString& id, ArtifactPr::FramePosition start,
                              ArtifactPr::FramePosition duration)
{
    ArtifactPr::DemoClip clip;
    clip.id = id;
    clip.name = id;
    clip.sourceFile = id + QStringLiteral(".mov");
    clip.startFrame = start;
    clip.duration = duration;
    clip.sourceIn = 0;
    clip.sourceOut = duration;
    return clip;
}

} // namespace

TEST(ArtifactPrEditorEngineTest, NewProjectProvidesOneVideoAndOneAudioTrack)
{
    auto* engine = freshEngine();
    const ArtifactPr::DemoSequence sequence = engine->currentSequence();
    const ArtifactPr::DemoProject project = engine->currentProject();

    EXPECT_FALSE(project.id.isEmpty());
    EXPECT_FALSE(project.name.isEmpty());
    ASSERT_EQ(project.sequences.size(), 1);
    EXPECT_EQ(project.activeSequenceId, sequence.id);
    EXPECT_TRUE(project.mediaPool.isEmpty());

    EXPECT_FALSE(sequence.id.isEmpty());
    EXPECT_EQ(sequence.resolution, QStringLiteral("1920x1080"));
    EXPECT_EQ(sequence.frameRate, QStringLiteral("30 fps"));
    EXPECT_GT(sequence.duration, 0);

    ASSERT_EQ(sequence.videoTracks.size(), 1);
    ASSERT_EQ(sequence.audioTracks.size(), 1);
    EXPECT_FALSE(sequence.videoTracks[0].id.isEmpty());
    EXPECT_EQ(sequence.videoTracks[0].kind, QStringLiteral("video"));
    EXPECT_EQ(sequence.audioTracks[0].kind, QStringLiteral("audio"));
    EXPECT_TRUE(sequence.videoTracks[0].clips.isEmpty());

    EXPECT_EQ(engine->currentFrame(), 0);
    EXPECT_EQ(engine->inPoint(), 0);
    EXPECT_EQ(engine->outPoint(), sequence.duration);
    EXPECT_FALSE(engine->isPlaying());
}

TEST(ArtifactPrEditorEngineTest, InOutPointsClampToSequenceBoundsAndKeepOrder)
{
    auto* engine = freshEngine();
    const ArtifactPr::FramePosition duration = engine->currentSequence().duration;

    engine->setInPoint(-10);
    EXPECT_EQ(engine->inPoint(), 0);

    engine->setInPoint(100);
    EXPECT_EQ(engine->inPoint(), 100);
    EXPECT_EQ(engine->outPoint(), duration);

    // Out may not fall below In.
    engine->setOutPoint(50);
    EXPECT_EQ(engine->inPoint(), 100);
    EXPECT_EQ(engine->outPoint(), 100);

    engine->setOutPoint(200);
    EXPECT_EQ(engine->outPoint(), 200);

    // In may not rise above Out.
    engine->setInPoint(500);
    EXPECT_EQ(engine->inPoint(), 200);
    EXPECT_EQ(engine->outPoint(), 200);

    // Out may not exceed the sequence duration.
    engine->setOutPoint(9999);
    EXPECT_EQ(engine->outPoint(), duration);
    EXPECT_LE(engine->inPoint(), engine->outPoint());
}

TEST(ArtifactPrEditorEngineTest, SeekToFrameClampsToSequenceDuration)
{
    auto* engine = freshEngine();
    const ArtifactPr::FramePosition duration = engine->currentSequence().duration;

    engine->seekToFrame(120);
    EXPECT_EQ(engine->currentFrame(), 120);

    engine->seekToFrame(duration + 500);
    EXPECT_EQ(engine->currentFrame(), duration);

    engine->seekToFrame(-50);
    EXPECT_EQ(engine->currentFrame(), 0);
}

TEST(ArtifactPrEditorEngineTest, RenderQualityPresetSelectsScaleAndProxy)
{
    auto* engine = freshEngine();

    const auto full = engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full);
    EXPECT_DOUBLE_EQ(full.qualityScale, 1.0);
    EXPECT_FALSE(full.useProxyMedia);

    const auto preview = engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Preview);
    EXPECT_DOUBLE_EQ(preview.qualityScale, 0.5);
    EXPECT_TRUE(preview.useProxyMedia);

    const auto draft = engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Draft);
    EXPECT_DOUBLE_EQ(draft.qualityScale, 0.25);
    EXPECT_TRUE(draft.useProxyMedia);
}

TEST(ArtifactPrEditorEngineTest, RenderPlanClampsTheRequestedRange)
{
    auto* engine = freshEngine();
    const ArtifactPr::FramePosition duration = engine->currentSequence().duration;

    const auto defaults = engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full);
    EXPECT_EQ(defaults.startFrame, engine->inPoint());
    EXPECT_EQ(defaults.endFrame, engine->outPoint());
    EXPECT_TRUE(defaults.isValid());

    const auto explicitRange =
        engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full, 100, 200);
    EXPECT_EQ(explicitRange.startFrame, 100);
    EXPECT_EQ(explicitRange.endFrame, 200);

    const auto clamped =
        engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full, -5, 9999);
    EXPECT_EQ(clamped.startFrame, 0);
    EXPECT_EQ(clamped.endFrame, duration);

    // An inverted request collapses to the start frame instead of producing an
    // invalid range.
    const auto inverted =
        engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full, 200, 100);
    EXPECT_EQ(inverted.startFrame, 200);
    EXPECT_EQ(inverted.endFrame, 200);
    EXPECT_TRUE(inverted.isValid());
}

TEST(ArtifactPrEditorEngineTest,
     RenderPlanSnapshotStaysFrozenWhileTheLiveProjectContinuesEditing)
{
    auto* engine = freshEngine();
    const QString trackId = engine->currentSequence().videoTracks[0].id;
    ArtifactPr::DemoClip source = makeClip(QStringLiteral("frozen-plan-source"), 0, 40);
    source.sourceIn = 7;
    source.sourceOut = 47;
    ASSERT_TRUE(engine->editSourceRange(trackId, source, false, false));
    const QString clipId = engine->currentSequence().videoTracks[0].clips[0].id;
    engine->addMarker(11, QStringLiteral("Before plan"));

    const ArtifactPr::RenderPlan frozenPlan = engine->createRenderPlan(
        ArtifactPr::RenderQualityPreset::Full, 5, 25);
    ASSERT_TRUE(frozenPlan.isValid());
    const QJsonObject snapshotAtPlanCreation = engine->nleSnapshot();
    EXPECT_EQ(frozenPlan.nleSnapshot, snapshotAtPlanCreation);
    EXPECT_EQ(frozenPlan.startFrame, 5);
    EXPECT_EQ(frozenPlan.endFrame, 25);

    engine->setClipReversed(clipId, true);
    engine->addMarker(19, QStringLiteral("After plan"));
    const QJsonObject liveSnapshotAfterEdits = engine->nleSnapshot();
    EXPECT_NE(liveSnapshotAfterEdits, snapshotAtPlanCreation);

    // The exporter receives a value snapshot and must not observe edits made
    // after plan creation; a subsequently created plan sees the new state.
    EXPECT_EQ(frozenPlan.nleSnapshot, snapshotAtPlanCreation);
    const ArtifactPr::RenderPlan refreshedPlan = engine->createRenderPlan(
        ArtifactPr::RenderQualityPreset::Full, 5, 25);
    EXPECT_EQ(refreshedPlan.nleSnapshot, liveSnapshotAfterEdits);
    EXPECT_NE(refreshedPlan.nleSnapshot, frozenPlan.nleSnapshot);
    EXPECT_EQ(frozenPlan.startFrame, 5);
    EXPECT_EQ(frozenPlan.endFrame, 25);
}

TEST(ArtifactPrEditorEngineTest, RenderPlanAudioClipsRespectMuteSoloEnabledAndGain)
{
    auto* engine = freshEngine();
    ArtifactPr::DemoSequence sequence = engine->currentSequence();

    ArtifactPr::DemoTrack second;
    second.id = QStringLiteral("contract_audio_track_2");
    second.name = QStringLiteral("A2");
    second.kind = QStringLiteral("audio");
    sequence.audioTracks.append(second);

    sequence.audioTracks[0].clips.append(makeClip(QStringLiteral("a1_clip"), 0, 90));
    sequence.audioTracks[1].clips.append(makeClip(QStringLiteral("a2_clip"), 0, 90));
    engine->setCurrentSequence(sequence);

    auto plan = engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full);
    ASSERT_EQ(plan.audioClips.size(), 2);
    EXPECT_DOUBLE_EQ(plan.audioClips[0].volume, 1.0);

    // An audio gain effect folds into the frozen plan volume.
    sequence = engine->currentSequence();
    sequence.audioTracks[0].clips[0].effects.insert(QStringLiteral("fx.audioGain.enabled"),
                                                    QStringLiteral("1"));
    sequence.audioTracks[0].clips[0].effects.insert(QStringLiteral("fx.audioGain.gainDb"),
                                                    QStringLiteral("6.020599913279624"));
    engine->setCurrentSequence(sequence);

    plan = engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full);
    ASSERT_EQ(plan.audioClips.size(), 2);
    EXPECT_NEAR(plan.audioClips[0].volume, 2.0, 1e-9);

    // A disabled clip is dropped.
    sequence = engine->currentSequence();
    sequence.audioTracks[0].clips[0].enabled = false;
    engine->setCurrentSequence(sequence);
    plan = engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full);
    ASSERT_EQ(plan.audioClips.size(), 1);
    EXPECT_EQ(plan.audioClips[0].clipId, QStringLiteral("a2_clip"));

    // A muted track is dropped.
    sequence = engine->currentSequence();
    sequence.audioTracks[0].clips[0].enabled = true;
    sequence.audioTracks[0].muted = true;
    engine->setCurrentSequence(sequence);
    plan = engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full);
    ASSERT_EQ(plan.audioClips.size(), 1);
    EXPECT_EQ(plan.audioClips[0].clipId, QStringLiteral("a2_clip"));

    // Solo on one track excludes the other.
    sequence = engine->currentSequence();
    sequence.audioTracks[0].muted = false;
    sequence.audioTracks[1].solo = true;
    engine->setCurrentSequence(sequence);
    plan = engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full);
    ASSERT_EQ(plan.audioClips.size(), 1);
    EXPECT_EQ(plan.audioClips[0].clipId, QStringLiteral("a2_clip"));

    // The video clip effect map is frozen per clip id.
    sequence = engine->currentSequence();
    ArtifactPr::DemoClip videoClip = makeClip(QStringLiteral("v1_clip"), 0, 40);
    videoClip.effects.insert(QStringLiteral("fx.glow.enabled"), QStringLiteral("1"));
    sequence.videoTracks[0].clips.append(videoClip);
    engine->setCurrentSequence(sequence);
    plan = engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full);
    ASSERT_TRUE(plan.clipEffects.contains(QStringLiteral("v1_clip")));
    EXPECT_EQ(plan.clipEffects.value(QStringLiteral("v1_clip"))
                  .value(QStringLiteral("fx.glow.enabled"))
                  .toString(),
              QStringLiteral("1"));
}

TEST(ArtifactPrEditorEngineTest,
     RenderPlanDerivedMediaInputsStayFrozenAcrossLiveSequenceEdits)
{
    auto* engine = freshEngine();
    ArtifactPr::DemoSequence sequence = engine->currentSequence();
    const QString videoTrackId = sequence.videoTracks[0].id;

    ArtifactPr::DemoClip left = makeClip(QStringLiteral("frozen-left"), 0, 30);
    left.effects.insert(QStringLiteral("fx.glow.enabled"), QStringLiteral("1"));
    left.effects.insert(QStringLiteral("fx.glow.intensity"), QStringLiteral("0.75"));
    ArtifactPr::DemoClip right = makeClip(QStringLiteral("frozen-right"), 30, 30);
    sequence.videoTracks[0].clips.append(left);
    sequence.videoTracks[0].clips.append(right);

    ArtifactPr::Transition transition;
    transition.id = QStringLiteral("frozen-transition");
    transition.trackId = videoTrackId;
    transition.leftClipId = left.id;
    transition.rightClipId = right.id;
    transition.startFrame = 25;
    transition.duration = 10;
    sequence.transitions.append(transition);

    ArtifactPr::DemoClip audio = makeClip(QStringLiteral("frozen-audio"), 4, 40);
    audio.volume = 0.5;
    audio.effects.insert(QStringLiteral("fx.audioGain.enabled"), QStringLiteral("1"));
    audio.effects.insert(QStringLiteral("fx.audioGain.gainDb"),
                         QStringLiteral("6.020599913279624"));
    audio.effects.insert(QStringLiteral("fx.audioEqualizer.enabled"), QStringLiteral("1"));
    audio.effects.insert(QStringLiteral("fx.audioEqualizer.lowDb"), QStringLiteral("3.5"));
    sequence.audioTracks[0].clips.append(audio);
    engine->setCurrentSequence(sequence);

    const ArtifactPr::RenderPlan frozen =
        engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full);
    ASSERT_EQ(frozen.transitions.size(), 1);
    ASSERT_TRUE(frozen.clipEffects.contains(left.id));
    ASSERT_EQ(frozen.audioClips.size(), 1);
    EXPECT_NEAR(frozen.audioClips[0].volume, 1.0, 1e-9);
    EXPECT_DOUBLE_EQ(frozen.audioClips[0].eqLowDb, 3.5);

    sequence = engine->currentSequence();
    sequence.transitions.clear();
    sequence.videoTracks[0].clips[0].effects.insert(
        QStringLiteral("fx.glow.intensity"), QStringLiteral("1.5"));
    sequence.audioTracks[0].muted = true;
    sequence.audioTracks[0].clips[0].volume = 1.7;
    sequence.audioTracks[0].clips[0].effects.insert(
        QStringLiteral("fx.audioEqualizer.lowDb"), QStringLiteral("-8"));
    engine->setCurrentSequence(sequence);

    // Mutating the live project must not alter any derived inputs already
    // handed to an export job. A new plan observes the complete new state.
    ASSERT_EQ(frozen.transitions.size(), 1);
    EXPECT_EQ(frozen.transitions[0].id, QStringLiteral("frozen-transition"));
    EXPECT_EQ(frozen.clipEffects.value(left.id)
                  .value(QStringLiteral("fx.glow.intensity")).toString(),
              QStringLiteral("0.75"));
    ASSERT_EQ(frozen.audioClips.size(), 1);
    EXPECT_NEAR(frozen.audioClips[0].volume, 1.0, 1e-9);
    EXPECT_DOUBLE_EQ(frozen.audioClips[0].eqLowDb, 3.5);

    const ArtifactPr::RenderPlan refreshed =
        engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full);
    EXPECT_TRUE(refreshed.transitions.isEmpty());
    EXPECT_EQ(refreshed.clipEffects.value(left.id)
                  .value(QStringLiteral("fx.glow.intensity")).toString(),
              QStringLiteral("1.5"));
    EXPECT_TRUE(refreshed.audioClips.isEmpty());
}

TEST(ArtifactPrEditorEngineTest, SnapToNearestUsesPlayheadAndClipEdges)
{
    auto* engine = freshEngine();
    ArtifactPr::DemoSequence sequence = engine->currentSequence();
    sequence.videoTracks[0].clips.append(makeClip(QStringLiteral("snap_clip"), 200, 50));
    engine->setCurrentSequence(sequence);

    engine->seekToFrame(0);

    // The playhead wins inside the threshold.
    EXPECT_EQ(engine->snapToNearest(3, true), 0);

    // A left edge snaps for both edge kinds; the right edge only for left.
    EXPECT_EQ(engine->snapToNearest(202, true), 200);
    EXPECT_EQ(engine->snapToNearest(202, false), 200);
    EXPECT_EQ(engine->snapToNearest(248, true), 250);
    EXPECT_EQ(engine->snapToNearest(248, false), 248);

    // Outside the threshold nothing moves.
    EXPECT_EQ(engine->snapToNearest(150, true), 150);
    EXPECT_EQ(engine->snapToNearest(180, false), 180);
}

TEST(ArtifactPrEditorEngineTest, SnapToNearestUsesMarkers)
{
    auto* engine = freshEngine();
    engine->addMarker(300, QStringLiteral("Chapter 1"), QStringLiteral("contract"));

    ASSERT_FALSE(engine->markers().isEmpty());
    EXPECT_EQ(engine->markers().first().position, 300);

    engine->seekToFrame(0);
    EXPECT_EQ(engine->snapToNearest(302, false), 300);
    EXPECT_EQ(engine->snapToNearest(296, false), 300);
    EXPECT_EQ(engine->snapToNearest(320, false), 320);
}

TEST(ArtifactPrEditorEngineTest, SnappingCanBeDisabled)
{
    auto* engine = freshEngine();
    engine->setSnapEnabled(false);
    engine->seekToFrame(0);
    EXPECT_EQ(engine->snapToNearest(3, true), 3);
    engine->setSnapEnabled(true);
}

TEST(ArtifactPrEditorEngineTest,
     ExtendedSnappingPrioritizesTransitionsThenHonorsSecondThreshold)
{
    auto* engine = freshEngine();
    ArtifactPr::DemoSequence sequence = engine->currentSequence();

    ArtifactPr::Transition transition;
    transition.id = QStringLiteral("snap_transition");
    transition.trackId = sequence.videoTracks[0].id;
    transition.leftClipId = QStringLiteral("left");
    transition.rightClipId = QStringLiteral("right");
    transition.startFrame = 64;
    transition.duration = 20;
    sequence.transitions.append(transition);
    engine->setCurrentSequence(sequence);

    // The transition start (64) and previous one-second boundary (60) are
    // equally close; transition edges have priority in the extended snap.
    EXPECT_EQ(engine->snapToNearestEx(62, false, 4), 64);
    EXPECT_EQ(engine->snapToNearestEx(82, true, 2), 84);

    // With no nearer transition, both neighboring second boundaries are
    // eligible, and the caller's threshold is inclusive.
    EXPECT_EQ(engine->snapToNearestEx(91, false, 1), 90);
    EXPECT_EQ(engine->snapToNearestEx(119, false, 1), 120);
    EXPECT_EQ(engine->snapToNearestEx(94, false, 4), 90);
    EXPECT_EQ(engine->snapToNearestEx(94, false, 3), 94);

    // Legacy targets keep their fixed-priority behavior, and disabling snap
    // bypasses both the legacy and extended candidates.
    EXPECT_EQ(engine->snapToNearestEx(4, false, 100), 0);
    engine->setSnapEnabled(false);
    EXPECT_EQ(engine->snapToNearestEx(62, false, 100), 62);
}

TEST(ArtifactPrEditorEngineTest, NleSnapshotRoundTripsThroughRestore)
{
    auto* engine = freshEngine();
    const QJsonObject snapshot = engine->nleSnapshot();
    ASSERT_FALSE(snapshot.isEmpty());

    // Editing moves the live snapshot away from the frozen one...
    engine->addMarker(10, QStringLiteral("marker"), QString());
    EXPECT_NE(engine->nleSnapshot(), snapshot);

    // ...and restoring the frozen snapshot puts it back exactly.
    EXPECT_TRUE(engine->restoreNLESnapshot(snapshot));
    EXPECT_EQ(engine->nleSnapshot(), snapshot);
}

TEST(ArtifactPrEditorEngineTest, ProjectSaveLoadRoundTripRestoresTheNleStore)
{
    auto* engine = freshEngine();
    ArtifactPr::DemoSequence sequence = engine->currentSequence();
    sequence.videoTracks[0].clips.append(makeClip(QStringLiteral("roundtrip_clip"), 10, 30));
    engine->setCurrentSequence(sequence);

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("contract.artifactpr"));

    const QString sequenceId = engine->currentSequence().id;
    const QJsonObject snapshot = engine->nleSnapshot();
    const QString projectName = engine->currentProject().name;
    ASSERT_TRUE(engine->saveProject(path));

    engine->newProject();
    ASSERT_TRUE(engine->loadProject(path));

    EXPECT_EQ(engine->currentProject().name, projectName);
    EXPECT_EQ(engine->currentSequence().id, sequenceId);
    EXPECT_EQ(engine->nleSnapshot(), snapshot);
    EXPECT_FALSE(engine->projectFilePath().isEmpty());
}

TEST(ArtifactPrEditorEngineTest,
     ProjectSaveLoadRoundTripPreservesMultipleTrimmedClipsAndMarkers)
{
    auto* engine = freshEngine();
    const QString trackId = engine->currentSequence().videoTracks[0].id;

    ArtifactPr::DemoClip first = makeClip(QStringLiteral("first-source"), 0, 30);
    first.sourceIn = 12;
    first.sourceOut = 42;
    first.speed = 1.5;
    first.reversed = true;
    ASSERT_TRUE(engine->editSourceRange(trackId, first, false, false));

    const auto firstClip = engine->currentSequence().videoTracks[0].clips[0];
    EXPECT_EQ(firstClip.startFrame, 0);
    EXPECT_EQ(firstClip.duration, 20);
    EXPECT_EQ(firstClip.sourceIn, 12);
    EXPECT_EQ(firstClip.sourceOut, 42);
    EXPECT_DOUBLE_EQ(firstClip.speed, 1.5);
    EXPECT_TRUE(firstClip.reversed);

    engine->seekToFrame(firstClip.startFrame + firstClip.duration);
    ArtifactPr::DemoClip second = makeClip(QStringLiteral("second-source"), 0, 24);
    second.sourceIn = 5;
    second.sourceOut = 29;
    second.speed = 2.0;
    ASSERT_TRUE(engine->editSourceRange(trackId, second, false, false));

    engine->addMarker(19, QStringLiteral("Cue 🎬"), QStringLiteral("before cut"));
    const ArtifactPr::DemoSequence beforeSave = engine->currentSequence();
    ASSERT_EQ(beforeSave.videoTracks[0].clips.size(), 2);
    ASSERT_EQ(beforeSave.markers.size(), 1);
    const QJsonObject nleSnapshot = engine->nleSnapshot();

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("complex-roundtrip.artifactpr"));
    ASSERT_TRUE(engine->saveProject(path));

    engine->newProject();
    ASSERT_TRUE(engine->loadProject(path));

    EXPECT_EQ(engine->nleSnapshot(), nleSnapshot);
    const ArtifactPr::DemoSequence restored = engine->currentSequence();
    ASSERT_EQ(restored.videoTracks.size(), 1);
    ASSERT_EQ(restored.videoTracks[0].clips.size(), 2);
    EXPECT_EQ(restored.videoTracks[0].clips[0].sourceFile,
              beforeSave.videoTracks[0].clips[0].sourceFile);
    EXPECT_EQ(restored.videoTracks[0].clips[0].sourceIn, 12);
    EXPECT_EQ(restored.videoTracks[0].clips[0].sourceOut, 42);
    EXPECT_EQ(restored.videoTracks[0].clips[0].duration, 20);
    EXPECT_DOUBLE_EQ(restored.videoTracks[0].clips[0].speed, 1.5);
    EXPECT_TRUE(restored.videoTracks[0].clips[0].reversed);
    EXPECT_EQ(restored.videoTracks[0].clips[1].sourceIn, 5);
    EXPECT_EQ(restored.videoTracks[0].clips[1].sourceOut, 29);
    EXPECT_EQ(restored.videoTracks[0].clips[1].duration, 12);
    EXPECT_DOUBLE_EQ(restored.videoTracks[0].clips[1].speed, 2.0);
    ASSERT_EQ(restored.markers.size(), 1);
    EXPECT_EQ(restored.markers[0].position, 19);
    EXPECT_EQ(restored.markers[0].name, QStringLiteral("Cue 🎬"));
    EXPECT_EQ(restored.markers[0].comment, QStringLiteral("before cut"));
}

TEST(ArtifactPrEditorEngineTest,
     RejectedProjectFilesLeaveTheOpenProjectAndPlayheadUntouched)
{
    auto* engine = freshEngine();
    engine->addMarker(47, QStringLiteral("Keep this state"), QStringLiteral("baseline"));
    engine->seekToFrame(83);

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString savedPath = directory.filePath(QStringLiteral("open-project.artifactpr"));
    ASSERT_TRUE(engine->saveProject(savedPath));

    const QJsonObject snapshotBefore = engine->nleSnapshot();
    const QString projectIdBefore = engine->currentProject().id;
    const QString sequenceIdBefore = engine->currentSequence().id;
    const QString projectPathBefore = engine->projectFilePath();
    const auto frameBefore = engine->currentFrame();

    const struct InvalidProjectCase {
        const char* filename;
        const char* contents;
    } cases[] = {
        {"malformed.artifactpr", "{invalid"},
        {"non-object-root.artifactpr", "[]"},
    };

    for (const auto& invalid : cases) {
        SCOPED_TRACE(invalid.filename);
        const QString invalidPath = directory.filePath(QString::fromLatin1(invalid.filename));
        QFile file(invalidPath);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(file.write(invalid.contents),
                  static_cast<qint64>(std::strlen(invalid.contents)));
        file.close();

        EXPECT_FALSE(engine->loadProject(invalidPath)) << invalid.filename;
        EXPECT_EQ(engine->nleSnapshot(), snapshotBefore) << invalid.filename;
        EXPECT_EQ(engine->currentProject().id, projectIdBefore) << invalid.filename;
        EXPECT_EQ(engine->currentSequence().id, sequenceIdBefore) << invalid.filename;
        EXPECT_EQ(engine->currentFrame(), frameBefore) << invalid.filename;
        EXPECT_EQ(engine->projectFilePath(), projectPathBefore) << invalid.filename;
        ASSERT_EQ(engine->currentSequence().markers.size(), 1);
        EXPECT_EQ(engine->currentSequence().markers[0].name,
                  QStringLiteral("Keep this state")) << invalid.filename;
    }
}

TEST(ArtifactPrEditorEngineTest,
     NleEditsUndoRedoAcrossSnapshotsAndDiscardTheAbandonedRedoBranch)
{
    auto* engine = freshEngine();
    const QJsonObject initialSnapshot = engine->nleSnapshot();
    const QString trackId = engine->currentSequence().videoTracks[0].id;

    ArtifactPr::DemoClip source = makeClip(QStringLiteral("undo-source"), 0, 24);
    source.sourceIn = 8;
    source.sourceOut = 32;
    ASSERT_TRUE(engine->editSourceRange(trackId, source, false, false));
    const QJsonObject afterInsert = engine->nleSnapshot();
    const QString clipId = engine->currentSequence().videoTracks[0].clips[0].id;

    engine->addMarker(12, QStringLiteral("Undo marker"));
    const QJsonObject afterMarker = engine->nleSnapshot();
    engine->setClipReversed(clipId, true);
    const QJsonObject afterReverse = engine->nleSnapshot();
    EXPECT_TRUE(engine->currentSequence().videoTracks[0].clips[0].reversed);

    engine->undo();
    EXPECT_EQ(engine->nleSnapshot(), afterMarker);
    EXPECT_FALSE(engine->currentSequence().videoTracks[0].clips[0].reversed);
    engine->undo();
    EXPECT_EQ(engine->nleSnapshot(), afterInsert);
    EXPECT_TRUE(engine->currentSequence().markers.isEmpty());
    engine->undo();
    EXPECT_EQ(engine->nleSnapshot(), initialSnapshot);
    EXPECT_TRUE(engine->currentSequence().videoTracks[0].clips.isEmpty());

    engine->redo();
    EXPECT_EQ(engine->nleSnapshot(), afterInsert);
    engine->redo();
    EXPECT_EQ(engine->nleSnapshot(), afterMarker);
    engine->redo();
    EXPECT_EQ(engine->nleSnapshot(), afterReverse);
    EXPECT_TRUE(engine->currentSequence().videoTracks[0].clips[0].reversed);

    engine->undo();
    EXPECT_EQ(engine->nleSnapshot(), afterMarker);
    engine->addMarker(20, QStringLiteral("New branch"));
    const QJsonObject branchedSnapshot = engine->nleSnapshot();
    ASSERT_NE(branchedSnapshot, afterReverse);

    // A new command after undo must clear the old redo branch.
    engine->redo();
    EXPECT_EQ(engine->nleSnapshot(), branchedSnapshot);
    ASSERT_EQ(engine->currentSequence().markers.size(), 2);
    EXPECT_EQ(engine->currentSequence().markers[1].name,
              QStringLiteral("New branch"));
}

TEST(ArtifactPrEditorEngineTest,
     NleSplitPartitionsNonzeroSourceRangeAndUndoRestoresTheWholeClip)
{
    auto* engine = freshEngine();
    const QString trackId = engine->currentSequence().videoTracks[0].id;
    ArtifactPr::DemoClip source = makeClip(QStringLiteral("split-source"), 0, 40);
    source.sourceIn = 17;
    source.sourceOut = 57;
    ASSERT_TRUE(engine->editSourceRange(trackId, source, false, false));

    const QString clipId = engine->currentSequence().videoTracks[0].clips[0].id;
    engine->selectClip(clipId);
    const QJsonObject beforeSplit = engine->nleSnapshot();
    engine->seekToFrame(17); // 17 frames into the 40-frame timeline range.
    engine->splitClipAtPlayhead();

    const QJsonObject afterSplit = engine->nleSnapshot();
    const ArtifactPr::DemoSequence splitSequence = engine->currentSequence();
    ASSERT_EQ(splitSequence.videoTracks[0].clips.size(), 2);
    const auto left = splitSequence.videoTracks[0].clips[0];
    const auto right = splitSequence.videoTracks[0].clips[1];
    EXPECT_EQ(left.id, clipId);
    EXPECT_EQ(left.startFrame, 0);
    EXPECT_EQ(left.duration, 17);
    EXPECT_EQ(left.sourceIn, 17);
    EXPECT_EQ(left.sourceOut, 34);
    EXPECT_EQ(right.startFrame, 17);
    EXPECT_EQ(right.duration, 23);
    EXPECT_EQ(right.sourceIn, 34);
    EXPECT_EQ(right.sourceOut, 57);

    engine->undo();
    EXPECT_EQ(engine->nleSnapshot(), beforeSplit);
    ASSERT_EQ(engine->currentSequence().videoTracks[0].clips.size(), 1);
    EXPECT_EQ(engine->currentSequence().videoTracks[0].clips[0].duration, 40);
    EXPECT_EQ(engine->currentSequence().videoTracks[0].clips[0].sourceIn, 17);
    EXPECT_EQ(engine->currentSequence().videoTracks[0].clips[0].sourceOut, 57);

    engine->redo();
    EXPECT_EQ(engine->nleSnapshot(), afterSplit);
    ASSERT_EQ(engine->currentSequence().videoTracks[0].clips.size(), 2);
    EXPECT_EQ(engine->currentSequence().videoTracks[0].clips[1].sourceIn, 34);
    EXPECT_EQ(engine->currentSequence().videoTracks[0].clips[1].sourceOut, 57);
}

TEST(ArtifactPrEditorEngineTest,
     TransitionOverlapGuardIsAtomicAndTheAcceptedEditIsUndoable)
{
    auto* engine = freshEngine();
    const QString trackId = engine->currentSequence().videoTracks[0].id;

    ArtifactPr::DemoClip leftSource = makeClip(QStringLiteral("transition-left"), 0, 30);
    ASSERT_TRUE(engine->editSourceRange(trackId, leftSource, false, false));
    const QString leftClipId = engine->currentSequence().videoTracks[0].clips[0].id;

    engine->seekToFrame(30);
    ArtifactPr::DemoClip rightSource = makeClip(QStringLiteral("transition-right"), 0, 30);
    ASSERT_TRUE(engine->editSourceRange(trackId, rightSource, false, false));
    const QString rightClipId = engine->currentSequence().videoTracks[0].clips[1].id;
    const QJsonObject beforeTransition = engine->nleSnapshot();

    engine->addTransition(trackId, leftClipId, rightClipId, 25,
                          ArtifactPr::TransitionType::Crossfade, 10);
    const QJsonObject acceptedTransition = engine->nleSnapshot();
    ASSERT_NE(acceptedTransition, beforeTransition);
    ASSERT_EQ(engine->currentSequence().transitions.size(), 1);
    EXPECT_EQ(engine->currentSequence().transitions[0].startFrame, 25);
    EXPECT_EQ(engine->currentSequence().transitions[0].duration, 10);

    // Both an exact duplicate and a partially overlapping interval for the
    // same pair must be rejected without changing the stored edit.
    engine->addTransition(trackId, leftClipId, rightClipId, 25,
                          ArtifactPr::TransitionType::WipeLeft, 10);
    engine->addTransition(trackId, leftClipId, rightClipId, 30,
                          ArtifactPr::TransitionType::DipToBlack, 8);
    EXPECT_EQ(engine->nleSnapshot(), acceptedTransition);
    ASSERT_EQ(engine->currentSequence().transitions.size(), 1);
    EXPECT_EQ(engine->currentSequence().transitions[0].type,
              ArtifactPr::TransitionType::Crossfade);

    engine->undo();
    EXPECT_EQ(engine->nleSnapshot(), beforeTransition);
    EXPECT_TRUE(engine->currentSequence().transitions.isEmpty());
    engine->redo();
    EXPECT_EQ(engine->nleSnapshot(), acceptedTransition);
    ASSERT_EQ(engine->currentSequence().transitions.size(), 1);
    EXPECT_EQ(engine->currentSequence().transitions[0].type,
              ArtifactPr::TransitionType::Crossfade);
}
