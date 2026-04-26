#pragma once

#include <QObject>
#include <QPointer>
#include <QVector>
#include <QString>
#include <QColor>
#include <QMap>

namespace ArtifactPr {

using FramePosition = int64_t;

struct DemoClip {
    QString id;
    QString name;
    QString sourceFile;
    FramePosition startFrame = 0;
    FramePosition duration = 100;
    FramePosition sourceIn = 0;
    FramePosition sourceOut = 100;
    QString color = QStringLiteral("#4a9eff");
    bool selected = false;
    bool linked = false;
    bool reversed = false;
    double speed = 1.0;
    double volume = 1.0;
    QMap<QString, QVariant> effects;
};

struct DemoTrack {
    QString id;
    QString name;
    QString kind;
    QVector<DemoClip> clips;
    bool muted = false;
    bool solo = false;
    int height = 28;
};

struct Marker {
    QString id;
    FramePosition position;
    QString name;
    QString comment;
    QColor color;
    enum class Type { Comment, Chapter, In, Out } type = Type::Comment;
};

enum class TransitionType {
    Crossfade,
    DipToBlack,
    WipeLeft,
    WipeRight
};

struct Transition {
    QString id;
    QString trackId;
    QString leftClipId;
    QString rightClipId;
    FramePosition startFrame;
    FramePosition duration;
    TransitionType type = TransitionType::Crossfade;
};

struct DemoSequence {
    QString id;
    QString name;
    QString resolution;
    QString frameRate;
    QVector<DemoTrack> videoTracks;
    QVector<DemoTrack> audioTracks;
    QVector<Marker> markers;
    QVector<Transition> transitions;
    FramePosition duration = 350;
};

struct MediaItem {
    QString id;
    QString name;
    QString filePath;
    QString type;
    QString resolution;
    QString duration;
    bool hasProxy = false;
    QString proxyPath;
};

struct DemoProject {
    QString id;
    QString name;
    QString version;
    QString createdAt;
    QString modifiedAt;
    QVector<MediaItem> mediaPool;
    QVector<DemoSequence> sequences;
    QString activeSequenceId;
};

enum class PlaybackSpeed {
    Stop = 0,
    Reverse1x = -1,
    Reverse2x = -2,
    Reverse4x = -4,
    Reverse8x = -8,
    Pause = 1,
    Forward1x = 2,
    Forward2x = 4,
    Forward4x = 8,
    Forward8x = 16
};

class UndoCommand
{
public:
    virtual ~UndoCommand() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual QString description() const = 0;
};

class EditorEngine : public QObject
{
    Q_OBJECT
public:
    static EditorEngine* instance();

    EditorEngine();
    ~EditorEngine();

    void loadDemoProject();

    DemoProject currentProject() const { return currentProject_; }
    void setCurrentProject(const DemoProject& project) { currentProject_ = project; }

    DemoSequence currentSequence() const { return currentSequence_; }
    void setCurrentSequence(const DemoSequence& seq) { currentSequence_ = seq; }

    FramePosition currentFrame() const { return currentFrame_; }
    void setCurrentFrame(FramePosition frame);

    FramePosition inPoint() const { return inPoint_; }
    FramePosition outPoint() const { return outPoint_; }
    void setInPoint(FramePosition p) { inPoint_ = p; Q_EMIT inOutPointChanged(inPoint_, outPoint_); }
    void setOutPoint(FramePosition p) { outPoint_ = p; Q_EMIT inOutPointChanged(inPoint_, outPoint_); }

    PlaybackSpeed playbackSpeed() const { return playbackSpeed_; }
    bool isPlaying() const { return playbackSpeed_ != PlaybackSpeed::Stop && playbackSpeed_ != PlaybackSpeed::Pause; }

    QString selectedClipId() const { return selectedClipId_; }
    void selectClip(const QString& clipId);
    void clearSelection();
    DemoClip* findClip(const QString& clipId);
    DemoTrack* findTrack(const QString& trackId);
    const QVector<Marker>& markers() const { return currentSequence_.markers; }
    const QVector<Transition>& transitions() const { return currentSequence_.transitions; }

    const QVector<MediaItem>& mediaPool() const { return currentProject_.mediaPool; }

public Q_SLOTS:
    void play() { setPlaybackSpeed(PlaybackSpeed::Forward1x); }
    void pause() { setPlaybackSpeed(PlaybackSpeed::Pause); }
    void stop() { setPlaybackSpeed(PlaybackSpeed::Stop); }
    void stepForward();
    void stepBackward();
    void togglePlayPause();
    void seekToFrame(FramePosition frame);

    void shuttleForward();
    void shuttleReverse();

    void setPlaybackSpeed(PlaybackSpeed speed);

    void deleteSelectedClip();
    void rippleDeleteSelectedClip();
    void splitClipAtPlayhead();
    void duplicateSelectedClip();

    void setClipSpeed(const QString& clipId, double speed);
    void setClipReversed(const QString& clipId, bool reversed);
    void setClipVolume(const QString& clipId, double volume);
    void setClipName(const QString& clipId, const QString& name);

    void addTransitionAtPlayhead(TransitionType type, FramePosition duration = 12);

    void addMarker(FramePosition position, const QString& name = QString(), const QString& comment = QString());
    void deleteMarker(const QString& markerId);
    void moveMarker(const QString& markerId, FramePosition newPosition);
    void setMarkerName(const QString& markerId, const QString& name);
    void setMarkerComment(const QString& markerId, const QString& comment);
    void clearMarkers();

    void addTransition(const QString& trackId, const QString& leftClipId, const QString& rightClipId,
                      FramePosition startFrame, TransitionType type, FramePosition duration = 12);
    void deleteTransition(const QString& transitionId);

    void addMediaToPool(const QString& filePath, const QString& name, const QString& type);
    void removeMediaFromPool(const QString& mediaId);

    void cutClip(const QString& clipId);
    void copyClip(const QString& clipId);
    void pasteClip(FramePosition targetFrame);

    bool saveProject(const QString& filePath);
    bool loadProject(const QString& filePath);
    void newProject();

    bool isSnapEnabled() const { return snapEnabled_; }
    void setSnapEnabled(bool enabled) { snapEnabled_ = enabled; }
    FramePosition snapToNearest(FramePosition frame, bool forLeftEdge);

    void undo();
    void redo();

    bool hasClipboard() const { return !clipboard_.id.isEmpty(); }
    const DemoClip& clipboard() const { return clipboard_; }

Q_SIGNALS:
    void currentFrameChanged(ArtifactPr::FramePosition frame) W_SIGNAL(currentFrameChanged, frame);
    void playbackStateChanged(bool isPlaying) W_SIGNAL(playbackStateChanged, isPlaying);
    void playbackSpeedChanged(ArtifactPr::PlaybackSpeed speed) W_SIGNAL(playbackSpeedChanged, speed);
    void inOutPointChanged(ArtifactPr::FramePosition inPoint, ArtifactPr::FramePosition outPoint) W_SIGNAL(inOutPointChanged, inPoint, outPoint);
    void sequenceChanged(const DemoSequence& sequence) W_SIGNAL(sequenceChanged, sequence);
    void clipSelectionChanged(const QString& clipId) W_SIGNAL(clipSelectionChanged, clipId);
    void clipChanged(const QString& clipId) W_SIGNAL(clipChanged, clipId);
    void projectModified() W_SIGNAL(projectModified);
    void projectSaved(bool success, const QString& message) W_SIGNAL(projectSaved, success, message);
    void projectLoaded(bool success, const QString& message) W_SIGNAL(projectLoaded, success, message);
    void markerChanged() W_SIGNAL(markerChanged);
    void transitionChanged() W_SIGNAL(transitionChanged);
    void exportStarted() W_SIGNAL(exportStarted);
    void exportProgress(int percent) W_SIGNAL(exportProgress, percent);
    void exportFinished(bool success, const QString& message) W_SIGNAL(exportFinished, success, message);

private:
    void pushUndo(UndoCommand* cmd);
    QString generateId(const QString& prefix);
    static QString transitionTypeToString(TransitionType type);
    static TransitionType stringToTransitionType(const QString& str);

    static EditorEngine* s_instance;

    DemoProject currentProject_;
    DemoSequence currentSequence_;
    FramePosition currentFrame_ = 0;
    FramePosition inPoint_ = 0;
    FramePosition outPoint_ = 350;
    PlaybackSpeed playbackSpeed_ = PlaybackSpeed::Stop;

    QString selectedClipId_;

    DemoClip clipboard_;

    bool snapEnabled_ = true;

    QVector<UndoCommand*> undoStack_;
    QVector<UndoCommand*> redoStack_;
};

class MoveClipCommand : public UndoCommand
{
public:
    MoveClipCommand(const QString& clipId, FramePosition oldStart, FramePosition newStart)
        : clipId_(clipId), oldStart_(oldStart), newStart_(newStart) {}

    void undo() override;
    void redo() override { doMove(newStart_); }
    QString description() const override { return QStringLiteral("Move Clip"); }

private:
    void doMove(FramePosition pos);

    QString clipId_;
    FramePosition oldStart_;
    FramePosition newStart_;
};

class TrimClipCommand : public UndoCommand
{
public:
    TrimClipCommand(const QString& clipId, FramePosition oldStart, FramePosition oldDuration,
                   FramePosition newStart, FramePosition newDuration)
        : clipId_(clipId), oldStart_(oldStart), oldDuration_(oldDuration),
          newStart_(newStart), newDuration_(newDuration) {}

    void undo() override;
    void redo() override { doTrim(newStart_, newDuration_); }
    QString description() const override { return QStringLiteral("Trim Clip"); }

private:
    void doTrim(FramePosition start, FramePosition duration);

    QString clipId_;
    FramePosition oldStart_;
    FramePosition oldDuration_;
    FramePosition newStart_;
    FramePosition newDuration_;
};

class DeleteClipCommand : public UndoCommand
{
public:
    DeleteClipCommand(const QString& trackId, const DemoClip& clip, int index)
        : trackId_(trackId), clip_(clip), index_(index) {}

    void undo() override;
    void redo() override;
    QString description() const override { return QStringLiteral("Delete Clip"); }

private:
    QString trackId_;
    DemoClip clip_;
    int index_;
};

class AddMarkerCommand : public UndoCommand
{
public:
    AddMarkerCommand(const Marker& marker)
        : marker_(marker) {}

    void undo() override;
    void redo() override;
    QString description() const override { return QStringLiteral("Add Marker"); }

private:
    Marker marker_;
};

class DeleteMarkerCommand : public UndoCommand
{
public:
    DeleteMarkerCommand(const Marker& marker, int index)
        : marker_(marker), index_(index) {}

    void undo() override;
    void redo() override;
    QString description() const override { return QStringLiteral("Delete Marker"); }

private:
    Marker marker_;
    int index_;
};

} // namespace ArtifactPr
