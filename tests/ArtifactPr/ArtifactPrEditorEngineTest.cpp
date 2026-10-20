// ArtifactPr EditorEngine contract test.
//
// EditorEngine is the single document/undo/render owner shared by the GUI, the
// CLI and the exporter. These cases pin the parts of that contract that are
// observable without a GPU device or a real media file: the new-project
// invariants, in/out point clamping, render plan construction (quality preset,
// range clamp, audio mute/solo/fx filter), snapping, the NLE snapshot and the
// project save/load round trip.
#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>
#include <QVector>

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
