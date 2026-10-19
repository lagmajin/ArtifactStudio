#include <QApplication>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QString>
#include <QWidget>

#include <cstdio>

import ArtifactPr.EditorEngine;
import ArtifactPr.TimelinePanel;

namespace {

int fail(const char* message)
{
    std::fprintf(stderr, "ArtifactPr TimelinePanel standalone contract failed: %s\n", message);
    return 1;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ArtifactPrTimelinePanelStandaloneTest"));

    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->newProject();
    auto sequence = engine->currentSequence();
    if (sequence.videoTracks.isEmpty() || sequence.audioTracks.isEmpty())
        return fail("new project must provide video and audio tracks");

    ArtifactPr::DemoClip clip;
    clip.id = QStringLiteral("standalone_clip_1");
    clip.name = QStringLiteral("Fixture Clip");
    clip.sourceFile = QStringLiteral("fixture.mov");
    clip.startFrame = 0;
    clip.duration = 90;
    sequence.videoTracks[0].clips.append(clip);
    engine->setCurrentSequence(sequence);

    ArtifactPrTimelinePanel panel;
    if (panel.objectName() != QStringLiteral("artifactpr.timeline.panel"))
        return fail("timeline root must expose its stable object name");

    const auto trackName = QStringLiteral("artifactpr.timeline.track.%1")
                               .arg(sequence.videoTracks[0].id);
    const auto clipName = QStringLiteral("artifactpr.timeline.clip.%1").arg(clip.id);
    if (!panel.findChild<QWidget*>(QStringLiteral("artifactpr.timeline.ruler")))
        return fail("timeline ruler must be present in the standalone panel");
    if (!panel.findChild<QScrollArea*>())
        return fail("timeline track scroll surface must be present");
    if (!panel.findChild<QWidget*>(trackName))
        return fail("sequence video track must produce a timeline row");
    if (!panel.findChild<QWidget*>(clipName))
        return fail("sequence clip must produce a timeline clip widget");
    if (!panel.findChild<QSlider*>())
        return fail("timeline zoom control must be present");

    auto* zoom = panel.findChild<QSlider*>(QStringLiteral("zoomSlider_"));
    auto* zoomLabel = panel.findChild<QLabel*>(QStringLiteral("zoomLevelLabel_"));
    auto* sequenceInfo = panel.findChild<QLabel*>(QStringLiteral("sequenceInfo_"));
    if (!zoom || !zoomLabel || !sequenceInfo)
        return fail("timeline controls must expose their stable contract names");
    if (!sequenceInfo->text().startsWith(sequence.name))
        return fail("timeline sequence summary must describe the active sequence");
    zoom->setValue(10);
    if (zoomLabel->text() != QStringLiteral("2.0x"))
        return fail("zoom control changes must update the displayed scale");

    std::puts("ArtifactPr TimelinePanel standalone contract passed.");
    return 0;
}
