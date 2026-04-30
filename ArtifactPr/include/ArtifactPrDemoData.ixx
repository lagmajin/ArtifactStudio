module;
#include <QString>
#include <QStringList>
#include <QVector>

export module ArtifactPr.DemoData;

export namespace ArtifactPr {

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

}
