module;
#include <wobjectdefs.h>
#include <QObject>
#include <QPointer>
#include <QVector>
#include <QString>
#include <QColor>
#include <QMap>
#include <QtGlobal>
#include <QVariant>
#include <QJsonObject>
#include <memory>

export module ArtifactPr.EditorEngine;

import NLE.Core;

export namespace ArtifactPr {

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

/// Shared input contract for preview and export.
/// The plan freezes the editable NLE state so playback and export do not read
/// the live editor model while it is being changed.
enum class RenderQualityPreset {
    Draft,
    Preview,
    Full,
};

struct RenderPlan {
    QJsonObject nleSnapshot;
    QString resolution;
    QString frameRate;
    FramePosition startFrame = 0;
    FramePosition endFrame = 0;
    double qualityScale = 1.0;
    bool useProxyMedia = false;

    bool isValid() const { return !nleSnapshot.isEmpty() && endFrame >= startFrame; }
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

class NLEStateCommand : public UndoCommand
{
public:
    NLEStateCommand(const QJsonObject& before, const QJsonObject& after)
        : before_(before), after_(after) {}

    void undo() override;
    void redo() override;
    QString description() const override { return QStringLiteral("NLE Edit"); }

private:
    QJsonObject before_;
    QJsonObject after_;
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
    void setCurrentSequence(const DemoSequence& seq)
    {
        currentSequence_ = seq;
        for (auto& projectSequence : currentProject_.sequences) {
            if (projectSequence.id == seq.id) {
                projectSequence = seq;
                break;
            }
        }
        const FramePosition maxFrame = qMax<FramePosition>(0, currentSequence_.duration);
        const FramePosition clampedFrame = qMax<FramePosition>(0, qMin(currentFrame_, maxFrame));
        if (clampedFrame != currentFrame_) {
            currentFrame_ = clampedFrame;
            Q_EMIT currentFrameChanged(currentFrame_);
        }
        inPoint_ = qMax<FramePosition>(0, qMin(inPoint_, maxFrame));
        outPoint_ = qMax<FramePosition>(0, qMin(outPoint_, maxFrame));
        Q_EMIT sequenceChanged(currentSequence_);
        Q_EMIT projectModified();
    }

    QJsonObject nleSnapshot() const;
    bool restoreNLESnapshot(const QJsonObject& snapshot);
    RenderPlan createRenderPlan(RenderQualityPreset preset,
                                FramePosition startFrame = -1,
                                FramePosition endFrame = -1) const;

    FramePosition currentFrame() const { return currentFrame_; }
    void setCurrentFrame(FramePosition frame);

    FramePosition inPoint() const { return inPoint_; }
    FramePosition outPoint() const { return outPoint_; }
    void setInPoint(FramePosition p)
    {
        const FramePosition maxFrame = qMax<FramePosition>(0, currentSequence_.duration);
        inPoint_ = qMax<FramePosition>(0, qMin(p, qMin(outPoint_, maxFrame)));
        Q_EMIT inOutPointChanged(inPoint_, outPoint_);
    }
    void setOutPoint(FramePosition p)
    {
        const FramePosition maxFrame = qMax<FramePosition>(0, currentSequence_.duration);
        outPoint_ = qMax<FramePosition>(inPoint_, qMin(p, maxFrame));
        Q_EMIT inOutPointChanged(inPoint_, outPoint_);
    }

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
    bool setVideoTransitionDuration(const QString& transitionId, FramePosition duration);

    void addMediaToPool(const QString& filePath, const QString& name, const QString& type);
    void removeMediaFromPool(const QString& mediaId);

    void cutClip(const QString& clipId);
    void copyClip(const QString& clipId);

    // Auto-save (M-PR-AUTOSAVE)
    bool isAutoSaveEnabled() const { return autoSaveEnabled_; }
    void setAutoSaveEnabled(bool enabled) { autoSaveEnabled_ = enabled; }

    int autoSaveIntervalSec() const { return autoSaveIntervalSec_; }
    void setAutoSaveIntervalSec(int sec) { autoSaveIntervalSec_ = sec; }

    QString autoSaveFilePath() const { return autoSaveFilePath_; }
    void setAutoSaveFilePath(const QString& path) { autoSaveFilePath_ = path; }

    /// 手動で auto-save をトリガ。
    bool runAutoSave();
    void pasteClip(FramePosition targetFrame);

    // 6 種類の NLE 編集操作 (UndoCommand 経由)
    void slipClip(const QString& clipId, FramePosition delta);
    void slideClip(const QString& clipId, FramePosition delta);
    void moveClip(const QString& clipId, FramePosition newStart);
    void trimClip(const QString& clipId,
                  FramePosition newStart,
                  FramePosition newDuration,
                  FramePosition newSourceIn,
                  FramePosition newSourceOut);
    void rippleDeleteClipAt(const QString& clipId);
    void insertClipFromSource(const QString& trackId, const DemoClip& sourceClip, FramePosition insertAt);
    void overwriteClipFromSource(const QString& trackId, const DemoClip& sourceClip, FramePosition overwriteAt);
    void liftRange(const QString& trackId, FramePosition from, FramePosition to);

    bool saveProject(const QString& filePath);
    bool loadProject(const QString& filePath);
    void newProject();

    bool isSnapEnabled() const { return snapEnabled_; }
    void setSnapEnabled(bool enabled) { snapEnabled_ = enabled; }
    FramePosition snapToNearest(FramePosition frame, bool forLeftEdge);

    /// 拡張 snap (5 種類)。
    enum class SnapKind {
        Clip,           // 既存
        Marker,         // 既存
        Playhead,       // 既存
        Transition,     // 新規
        Frame,          // 新規: 1 frame 単位
        Second,         // 新規: fps 単位 (30 fps なら 30 frame 単位)
    };
    FramePosition snapToNearestEx(FramePosition frame, bool forLeftEdge, int threshold = 5);

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

    /// UndoCommand を push する (UI レイヤから直接利用可)。
    /// 内部実装は stack_ に積む + redoStack_ をクリア。
    void pushUndo(UndoCommand* cmd);

private:
    QString generateId(const QString& prefix);
    static QString transitionTypeToString(TransitionType type);
    static TransitionType stringToTransitionType(const QString& str);
    void rebuildLegacySnapshotFromNLE();

    static EditorEngine* s_instance;

    std::unique_ptr<ArtifactCore::NLE::NLEProjectStore> nleStore_;

    DemoProject currentProject_;
    DemoSequence currentSequence_;
    FramePosition currentFrame_ = 0;
    FramePosition inPoint_ = 0;
    FramePosition outPoint_ = 350;
    PlaybackSpeed playbackSpeed_ = PlaybackSpeed::Stop;

    // Auto-save (M-PR-AUTOSAVE)
    bool autoSaveEnabled_ = false;
    int autoSaveIntervalSec_ = 60;
    QString autoSaveFilePath_;

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
                   FramePosition oldSourceIn, FramePosition oldSourceOut,
                   FramePosition newStart, FramePosition newDuration,
                   FramePosition newSourceIn, FramePosition newSourceOut)
        : clipId_(clipId), oldStart_(oldStart), oldDuration_(oldDuration),
          oldSourceIn_(oldSourceIn), oldSourceOut_(oldSourceOut),
          newStart_(newStart), newDuration_(newDuration),
          newSourceIn_(newSourceIn), newSourceOut_(newSourceOut) {}

    void undo() override;
    void redo() override { doTrim(newStart_, newDuration_, newSourceIn_, newSourceOut_); }
    QString description() const override { return QStringLiteral("Trim Clip"); }

private:
    void doTrim(FramePosition start, FramePosition duration,
                FramePosition sourceIn, FramePosition sourceOut);

    QString clipId_;
    FramePosition oldStart_;
    FramePosition oldDuration_;
    FramePosition oldSourceIn_;
    FramePosition oldSourceOut_;
    FramePosition newStart_;
    FramePosition newDuration_;
    FramePosition newSourceIn_;
    FramePosition newSourceOut_;
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

class VideoTracksStateCommand : public UndoCommand
{
public:
    VideoTracksStateCommand(QVector<DemoTrack> before, QVector<DemoTrack> after,
                            FramePosition beforeDuration, FramePosition afterDuration,
                            QString beforeSelection, QString afterSelection)
        : before_(std::move(before)), after_(std::move(after)),
          beforeDuration_(beforeDuration), afterDuration_(afterDuration),
          beforeSelection_(std::move(beforeSelection)), afterSelection_(std::move(afterSelection)) {}

    void undo() override;
    void redo() override;
    QString description() const override { return QStringLiteral("Change Video Clips"); }

private:
    void apply(const QVector<DemoTrack>& tracks, FramePosition duration,
               const QString& selection);

    QVector<DemoTrack> before_;
    QVector<DemoTrack> after_;
    FramePosition beforeDuration_;
    FramePosition afterDuration_;
    QString beforeSelection_;
    QString afterSelection_;
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

class MarkerStateCommand : public UndoCommand
{
public:
    MarkerStateCommand(QVector<Marker> before, QVector<Marker> after)
        : before_(std::move(before)), after_(std::move(after)) {}

    void undo() override;
    void redo() override;
    QString description() const override { return QStringLiteral("Change Markers"); }

private:
    void apply(const QVector<Marker>& markers);

    QVector<Marker> before_;
    QVector<Marker> after_;
};

class TransitionStateCommand : public UndoCommand
{
public:
    TransitionStateCommand(QVector<Transition> before, QVector<Transition> after)
        : before_(std::move(before)), after_(std::move(after)) {}

    void undo() override;
    void redo() override;
    QString description() const override { return QStringLiteral("Change Video Transition"); }

private:
    void apply(const QVector<Transition>& transitions);

    QVector<Transition> before_;
    QVector<Transition> after_;
};

}
