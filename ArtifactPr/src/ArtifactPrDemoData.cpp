#include "ArtifactPrDemoData.hpp"

ArtifactPrProjectSpec makeArtifactPrDemoProject()
{
    ArtifactPrProjectSpec project;
    project.name = QStringLiteral("ArtifactPr Demo Project");
    project.mediaItems = {
        QStringLiteral("intro.mov"),
        QStringLiteral("interview.aac"),
        QStringLiteral("broll_city.mp4"),
        QStringLiteral("logo.png"),
    };

    ArtifactPrSequenceSpec sequence;
    sequence.name = QStringLiteral("Main Cut");
    sequence.resolution = QStringLiteral("1920x1080");
    sequence.frameRate = QStringLiteral("30 fps");
    sequence.videoTracks = {
        {QStringLiteral("V1"), {
            {QStringLiteral("Intro"), QStringLiteral("00:00 - 00:06")},
            {QStringLiteral("Interview"), QStringLiteral("00:06 - 00:18")},
            {QStringLiteral("Outro"), QStringLiteral("00:18 - 00:24")},
        }},
        {QStringLiteral("V2"), {
            {QStringLiteral("B-Roll"), QStringLiteral("overlay layer")},
            {QStringLiteral("Title"), QStringLiteral("animated text")},
        }},
    };
    sequence.audioTracks = {
        {QStringLiteral("A1"), {
            {QStringLiteral("VO"), QStringLiteral("clean narration")},
            {QStringLiteral("Music"), QStringLiteral("bed track")},
        }},
    };

    project.sequences = {sequence};
    return project;
}
