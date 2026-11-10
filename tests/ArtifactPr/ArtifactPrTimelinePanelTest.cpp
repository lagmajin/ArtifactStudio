// ArtifactPr timeline panel widget contract.
//
// This is the gtest port of the former
// tests/ArtifactPr/TimelinePanelStandaloneTest.cpp. The panel is constructed
// without ArtifactPrMainWindow and is driven purely through the EditorEngine
// singleton, so the stable object names below are the panel's public contract.
#include <gtest/gtest.h>

#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QString>
#include <QUndoStack>
#include <QWidget>

#include "ArtifactPrTestApp.h"

import ArtifactPr.EditorEngine;
import ArtifactPr.TimelinePanel;

namespace {

[[maybe_unused]] const bool artifactPrApplicationRegistered =
    ArtifactPrTest::registerApplicationEnvironment();

ArtifactPr::DemoClip fixtureClip(const QString& id)
{
    ArtifactPr::DemoClip clip;
    clip.id = id;
    clip.name = QStringLiteral("Fixture Clip");
    clip.sourceFile = QStringLiteral("fixture.mov");
    clip.startFrame = 0;
    clip.duration = 90;
    clip.sourceIn = 0;
    clip.sourceOut = 90;
    return clip;
}

} // namespace

TEST(ArtifactPrTimelinePanelTest, PanelExposesRulerTracksClipsAndZoomControls)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->newProject();

    ArtifactPrTimelinePanel panel;
    EXPECT_EQ(panel.objectName(), QStringLiteral("artifactpr.timeline.panel"));

    ArtifactPr::DemoSequence sequence = engine->currentSequence();
    ASSERT_FALSE(sequence.videoTracks.isEmpty());
    ASSERT_FALSE(sequence.audioTracks.isEmpty());

    const ArtifactPr::DemoClip clip = fixtureClip(QStringLiteral("standalone_clip_1"));
    sequence.videoTracks[0].clips.append(clip);
    engine->setCurrentSequence(sequence);

    EXPECT_NE(panel.findChild<QWidget*>(QStringLiteral("artifactpr.timeline.ruler")), nullptr);
    EXPECT_NE(panel.findChild<QScrollArea*>(), nullptr);

    const QString trackName =
        QStringLiteral("artifactpr.timeline.track.%1").arg(sequence.videoTracks[0].id);
    const QString audioTrackName =
        QStringLiteral("artifactpr.timeline.track.%1").arg(sequence.audioTracks[0].id);
    const QString clipName = QStringLiteral("artifactpr.timeline.clip.%1").arg(clip.id);
    EXPECT_NE(panel.findChild<QWidget*>(trackName), nullptr);
    EXPECT_NE(panel.findChild<QWidget*>(audioTrackName), nullptr);
    EXPECT_NE(panel.findChild<QWidget*>(clipName), nullptr);
    EXPECT_NE(panel.findChild<QSlider*>(), nullptr);
}

TEST(ArtifactPrTimelinePanelTest, PanelExposesItsStableControlNames)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->newProject();

    ArtifactPrTimelinePanel panel;

    auto* zoom = panel.findChild<QSlider*>(QStringLiteral("zoomSlider_"));
    auto* zoomLabel = panel.findChild<QLabel*>(QStringLiteral("zoomLevelLabel_"));
    auto* sequenceInfo = panel.findChild<QLabel*>(QStringLiteral("sequenceInfo_"));
    ASSERT_NE(zoom, nullptr);
    ASSERT_NE(zoomLabel, nullptr);
    ASSERT_NE(sequenceInfo, nullptr);

    EXPECT_EQ(zoom->minimum(), 1);
    EXPECT_EQ(zoom->maximum(), 10);
    EXPECT_EQ(zoom->value(), 5);
}

TEST(ArtifactPrTimelinePanelTest, ZoomControlUpdatesTheDisplayedScale)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->newProject();

    ArtifactPrTimelinePanel panel;
    auto* zoom = panel.findChild<QSlider*>(QStringLiteral("zoomSlider_"));
    auto* zoomLabel = panel.findChild<QLabel*>(QStringLiteral("zoomLevelLabel_"));
    ASSERT_NE(zoom, nullptr);
    ASSERT_NE(zoomLabel, nullptr);

    zoom->setValue(10);
    EXPECT_EQ(zoomLabel->text(), QStringLiteral("2.0x"));

    zoom->setValue(1);
    EXPECT_EQ(zoomLabel->text(), QStringLiteral("0.2x"));

    zoom->setValue(5);
    EXPECT_EQ(zoomLabel->text(), QStringLiteral("1.0x"));
}

TEST(ArtifactPrTimelinePanelTest, SequenceSummaryDescribesTheActiveSequence)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->newProject();

    ArtifactPrTimelinePanel panel;
    auto* sequenceInfo = panel.findChild<QLabel*>(QStringLiteral("sequenceInfo_"));
    ASSERT_NE(sequenceInfo, nullptr);

    engine->setCurrentSequence(engine->currentSequence());

    const ArtifactPr::DemoSequence sequence = engine->currentSequence();
    EXPECT_FALSE(sequence.name.isEmpty());
    EXPECT_TRUE(sequenceInfo->text().startsWith(sequence.name));
    EXPECT_TRUE(sequenceInfo->text().contains(sequence.frameRate));
}

TEST(ArtifactPrTimelinePanelTest, PanelRefreshesAfterTheSequenceChanges)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->newProject();

    // The panel is built first, so the row has to appear through the engine's
    // sequenceChanged notification rather than the constructor.
    ArtifactPrTimelinePanel panel;

    ArtifactPr::DemoSequence sequence = engine->currentSequence();
    const ArtifactPr::DemoClip clip = fixtureClip(QStringLiteral("late_clip"));
    sequence.videoTracks[0].clips.append(clip);
    engine->setCurrentSequence(sequence);

    const QString clipName = QStringLiteral("artifactpr.timeline.clip.%1").arg(clip.id);
    EXPECT_NE(panel.findChild<QWidget*>(clipName), nullptr);

    // Removing the clip must drop the widget again.
    ArtifactPr::DemoSequence emptied = engine->currentSequence();
    emptied.videoTracks[0].clips.clear();
    engine->setCurrentSequence(emptied);
    EXPECT_EQ(panel.findChild<QWidget*>(clipName), nullptr);
}

TEST(ArtifactPrTimelinePanelTest,
     ProgrammaticMuteSoloRefreshDoesNotWriteBackIntoNleHistory)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->newProject();
    ArtifactPrTimelinePanel panel;

    const QJsonObject snapshotBefore = engine->nleSnapshot();
    const int undoCountBefore = engine->undoStack()->count();
    ArtifactPr::DemoSequence sequence = engine->currentSequence();
    ASSERT_EQ(sequence.audioTracks.size(), 1);
    const QString trackId = sequence.audioTracks[0].id;
    sequence.audioTracks[0].muted = true;
    sequence.audioTracks[0].solo = true;
    engine->setCurrentSequence(sequence);

    const QString trackName =
        QStringLiteral("artifactpr.timeline.track.%1").arg(trackId);
    QWidget* trackWidget = panel.findChild<QWidget*>(trackName);
    ASSERT_NE(trackWidget, nullptr);
    const auto buttons = trackWidget->findChildren<QPushButton*>();
    ASSERT_EQ(buttons.size(), 2);
    EXPECT_EQ(buttons[0]->text(), QStringLiteral("M"));
    EXPECT_EQ(buttons[1]->text(), QStringLiteral("S"));
    EXPECT_TRUE(buttons[0]->isChecked());
    EXPECT_TRUE(buttons[1]->isChecked());

    // Refreshing from model state must only update the controls. It must not
    // feed toggled events back into EditorEngine and create NLE edits.
    EXPECT_EQ(engine->nleSnapshot(), snapshotBefore);
    EXPECT_EQ(engine->undoStack()->count(), undoCountBefore);
}

TEST(ArtifactPrTimelinePanelTest,
     ZoomRefreshKeepsClipWidgetIdentityAndUpdatesItsWidth)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->newProject();
    ArtifactPrTimelinePanel panel;

    auto* zoom = panel.findChild<QSlider*>(QStringLiteral("zoomSlider_"));
    ASSERT_NE(zoom, nullptr);
    // Set the shared frame-width state deterministically before adding clips.
    zoom->setValue(10);
    zoom->setValue(5);

    ArtifactPr::DemoSequence sequence = engine->currentSequence();
    const ArtifactPr::DemoClip clip = fixtureClip(QStringLiteral("zoom_stable_clip"));
    sequence.videoTracks[0].clips.append(clip);
    engine->setCurrentSequence(sequence);

    const QString clipName = QStringLiteral("artifactpr.timeline.clip.%1").arg(clip.id);
    QWidget* originalWidget = panel.findChild<QWidget*>(clipName);
    ASSERT_NE(originalWidget, nullptr);
    EXPECT_EQ(originalWidget->minimumWidth(), 180);
    engine->selectClip(clip.id);

    // A same-structure zoom refresh should adjust geometry in place, keeping
    // the selected clip widget alive instead of rebuilding the timeline row.
    zoom->setValue(10);
    QWidget* zoomedWidget = panel.findChild<QWidget*>(clipName);
    ASSERT_NE(zoomedWidget, nullptr);
    EXPECT_EQ(zoomedWidget, originalWidget);
    EXPECT_EQ(zoomedWidget->minimumWidth(), 360);
    EXPECT_EQ(engine->selectedClipId(), clip.id);

    zoom->setValue(5);
    QWidget* restoredWidget = panel.findChild<QWidget*>(clipName);
    EXPECT_EQ(restoredWidget, originalWidget);
    ASSERT_NE(restoredWidget, nullptr);
    EXPECT_EQ(restoredWidget->minimumWidth(), 180);
    EXPECT_EQ(engine->selectedClipId(), clip.id);
}
