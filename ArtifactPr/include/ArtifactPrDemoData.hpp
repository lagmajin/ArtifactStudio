#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct ArtifactPrClipSpec
{
    QString name;
    QString meta;
};

struct ArtifactPrTrackSpec
{
    QString name;
    QVector<ArtifactPrClipSpec> clips;
};

struct ArtifactPrSequenceSpec
{
    QString name;
    QString resolution;
    QString frameRate;
    QVector<ArtifactPrTrackSpec> videoTracks;
    QVector<ArtifactPrTrackSpec> audioTracks;
};

struct ArtifactPrProjectSpec
{
    QString name;
    QStringList mediaItems;
    QVector<ArtifactPrSequenceSpec> sequences;
};

ArtifactPrProjectSpec makeArtifactPrDemoProject();
