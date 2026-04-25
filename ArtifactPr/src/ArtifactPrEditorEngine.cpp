#include "ArtifactPrEditorEngine.hpp"

#include <QColor>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>

using namespace Qt::StringLiterals;

namespace ArtifactPr {

static QString generateLegacyId()
{
    static int counter = 0;
    return QStringLiteral("clip_%1").arg(++counter);
}

static QString generateMarkerId()
{
    static int counter = 0;
    return QStringLiteral("marker_%1").arg(++counter);
}

static QString generateTransitionId()
{
    static int counter = 0;
    return QStringLiteral("trans_%1").arg(++counter);
}

static QString generateMediaId()
{
    static int counter = 0;
    return QStringLiteral("media_%1").arg(++counter);
}

static QString generateSequenceId()
{
    static int counter = 0;
    return QStringLiteral("seq_%1").arg(++counter);
}

static QString transitionTypeToStringLocal(TransitionType type)
{
    switch (type) {
    case TransitionType::Crossfade: return QStringLiteral("crossfade");
    case TransitionType::DipToBlack: return QStringLiteral("dip_to_black");
    case TransitionType::WipeLeft: return QStringLiteral("wipe_left");
    case TransitionType::WipeRight: return QStringLiteral("wipe_right");
    default: return QStringLiteral("crossfade");
    }
}

static TransitionType stringToTransitionTypeLocal(const QString& str)
{
    if (str == QStringLiteral("dip_to_black")) return TransitionType::DipToBlack;
    if (str == QStringLiteral("wipe_left")) return TransitionType::WipeLeft;
    if (str == QStringLiteral("wipe_right")) return TransitionType::WipeRight;
    return TransitionType::Crossfade;
}

EditorEngine* EditorEngine::s_instance = nullptr;

EditorEngine* EditorEngine::instance()
{
    if (!s_instance) {
        s_instance = new EditorEngine();
    }
    return s_instance;
}

EditorEngine::EditorEngine()
{
    s_instance = this;
    newProject();
}

EditorEngine::~EditorEngine()
{
    qDeleteAll(undoStack_);
    qDeleteAll(redoStack_);
}

void EditorEngine::newProject()
{
    DemoProject project;
    project.id = generateLegacyId();
    project.name = QStringLiteral("Untitled Project");
    project.version = QStringLiteral("1.0");
    project.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    project.modifiedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    DemoSequence seq;
    seq.id = generateSequenceId();
    seq.name = QStringLiteral("Sequence 1");
    seq.resolution = QStringLiteral("1920x1080");
    seq.frameRate = QStringLiteral("30 fps");

    DemoTrack v1;
    v1.id = QStringLiteral("track_v1");
    v1.name = QStringLiteral("V1");
    v1.kind = QStringLiteral("video");
    v1.height = 28;
    seq.videoTracks.push_back(v1);

    DemoTrack a1;
    a1.id = QStringLiteral("track_a1");
    a1.name = QStringLiteral("A1");
    a1.kind = QStringLiteral("audio");
    a1.height = 28;
    seq.audioTracks.push_back(a1);

    project.sequences.push_back(seq);
    project.activeSequenceId = seq.id;

    currentProject_ = project;
    currentSequence_ = seq;
    currentFrame_ = 0;
    inPoint_ = 0;
    outPoint_ = 350;
    playbackSpeed_ = PlaybackSpeed::Stop;
    selectedClipId_.clear();

    qDeleteAll(undoStack_);
    undoStack_.clear();
    qDeleteAll(redoStack_);
    redoStack_.clear();

    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectLoaded(true, QStringLiteral("New project created"));
}

void EditorEngine::loadDemoProject()
{
    newProject();

    MediaItem media1;
    media1.id = generateMediaId();
    media1.name = QStringLiteral("Sample Video 1");
    media1.filePath = QStringLiteral(":/demo/video1.mp4");
    media1.type = QStringLiteral("video");
    media1.resolution = QStringLiteral("1920x1080");
    media1.duration = QStringLiteral("00:10");
    currentProject_.mediaPool.push_back(media1);

    MediaItem media2;
    media2.id = generateMediaId();
    media2.name = QStringLiteral("Sample Video 2");
    media2.filePath = QStringLiteral(":/demo/video2.mp4");
    media2.type = QStringLiteral("video");
    media2.resolution = QStringLiteral("1920x1080");
    media2.duration = QStringLiteral("00:05");
    currentProject_.mediaPool.push_back(media2);

    if (currentSequence_.videoTracks.size() > 0) {
        DemoClip clip1;
        clip1.id = generateLegacyId();
        clip1.name = QStringLiteral("Clip 1");
        clip1.sourceFile = media1.filePath;
        clip1.startFrame = 0;
        clip1.duration = 200;
        clip1.sourceIn = 0;
        clip1.sourceOut = 200;
        clip1.color = QStringLiteral("#4a9eff");
        clip1.speed = 1.0;
        clip1.volume = 1.0;
        currentSequence_.videoTracks[0].clips.push_back(clip1);

        DemoClip clip2;
        clip2.id = generateLegacyId();
        clip2.name = QStringLiteral("Clip 2");
        clip2.sourceFile = media2.filePath;
        clip2.startFrame = 200;
        clip2.duration = 150;
        clip2.sourceIn = 0;
        clip2.sourceOut = 150;
        clip2.color = QStringLiteral("#4a9eff");
        clip2.speed = 1.0;
        clip2.volume = 1.0;
        currentSequence_.videoTracks[0].clips.push_back(clip2);
    }

    if (currentSequence_.audioTracks.size() > 0) {
        MediaItem audioMedia;
        audioMedia.id = generateMediaId();
        audioMedia.name = QStringLiteral("Sample Audio");
        audioMedia.filePath = QStringLiteral(":/demo/audio1.mp3");
        audioMedia.type = QStringLiteral("audio");
        audioMedia.duration = QStringLiteral("00:12");
        currentProject_.mediaPool.push_back(audioMedia);

        DemoClip audio1;
        audio1.id = generateLegacyId();
        audio1.name = QStringLiteral("Audio 1");
        audio1.sourceFile = audioMedia.filePath;
        audio1.startFrame = 0;
        audio1.duration = 350;
        audio1.sourceIn = 0;
        audio1.sourceOut = 350;
        audio1.color = QStringLiteral("#4eff4a");
        audio1.speed = 1.0;
        audio1.volume = 1.0;
        currentSequence_.audioTracks[0].clips.push_back(audio1);
    }

    currentProject_.modifiedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    Q_EMIT projectLoaded(true, QStringLiteral("Demo project loaded"));
}

void EditorEngine::setCurrentFrame(FramePosition frame)
{
    currentFrame_ = frame;
    Q_EMIT currentFrameChanged(frame);
}

void EditorEngine::selectClip(const QString& clipId)
{
    for (auto& track : currentSequence_.videoTracks) {
        for (auto& clip : track.clips) {
            clip.selected = (clip.id == clipId);
        }
    }
    for (auto& track : currentSequence_.audioTracks) {
        for (auto& clip : track.clips) {
            clip.selected = (clip.id == clipId);
        }
    }

    selectedClipId_ = clipId;
    Q_EMIT clipSelectionChanged(clipId);
}

void EditorEngine::clearSelection()
{
    for (auto& track : currentSequence_.videoTracks) {
        for (auto& clip : track.clips) {
            clip.selected = false;
        }
    }
    for (auto& track : currentSequence_.audioTracks) {
        for (auto& clip : track.clips) {
            clip.selected = false;
        }
    }
    selectedClipId_.clear();
    Q_EMIT clipSelectionChanged(QString());
}

DemoClip* EditorEngine::findClip(const QString& clipId)
{
    for (auto& track : currentSequence_.videoTracks) {
        for (auto& clip : track.clips) {
            if (clip.id == clipId) return &clip;
        }
    }
    for (auto& track : currentSequence_.audioTracks) {
        for (auto& clip : track.clips) {
            if (clip.id == clipId) return &clip;
        }
    }
    return nullptr;
}

DemoTrack* EditorEngine::findTrack(const QString& trackId)
{
    for (auto& track : currentSequence_.videoTracks) {
        if (track.id == trackId) return &track;
    }
    for (auto& track : currentSequence_.audioTracks) {
        if (track.id == trackId) return &track;
    }
    return nullptr;
}

void EditorEngine::stepForward()
{
    if (currentFrame_ < currentSequence_.duration) {
        setCurrentFrame(currentFrame_ + 1);
    }
}

void EditorEngine::stepBackward()
{
    if (currentFrame_ > 0) {
        setCurrentFrame(currentFrame_ - 1);
    }
}

void EditorEngine::togglePlayPause()
{
    if (isPlaying()) {
        pause();
    } else {
        play();
    }
}

void EditorEngine::seekToFrame(FramePosition frame)
{
    FramePosition pos = qMax(0, qMin(frame, currentSequence_.duration));
    setCurrentFrame(pos);
}

void EditorEngine::shuttleForward()
{
    switch (playbackSpeed_) {
    case PlaybackSpeed::Stop:
    case PlaybackSpeed::Pause:
        playbackSpeed_ = PlaybackSpeed::Forward1x;
        break;
    case PlaybackSpeed::Forward1x:
        playbackSpeed_ = PlaybackSpeed::Forward2x;
        break;
    case PlaybackSpeed::Forward2x:
        playbackSpeed_ = PlaybackSpeed::Forward4x;
        break;
    case PlaybackSpeed::Forward4x:
        playbackSpeed_ = PlaybackSpeed::Forward8x;
        break;
    case PlaybackSpeed::Forward8x:
        playbackSpeed_ = PlaybackSpeed::Forward8x;
        break;
    default:
        playbackSpeed_ = PlaybackSpeed::Forward1x;
        break;
    }
    Q_EMIT playbackSpeedChanged(playbackSpeed_);
    Q_EMIT playbackStateChanged(isPlaying());
}

void EditorEngine::shuttleReverse()
{
    switch (playbackSpeed_) {
    case PlaybackSpeed::Stop:
    case PlaybackSpeed::Pause:
        playbackSpeed_ = PlaybackSpeed::Reverse1x;
        break;
    case PlaybackSpeed::Reverse1x:
        playbackSpeed_ = PlaybackSpeed::Reverse2x;
        break;
    case PlaybackSpeed::Reverse2x:
        playbackSpeed_ = PlaybackSpeed::Reverse4x;
        break;
    case PlaybackSpeed::Reverse4x:
        playbackSpeed_ = PlaybackSpeed::Reverse8x;
        break;
    case PlaybackSpeed::Reverse8x:
        playbackSpeed_ = PlaybackSpeed::Reverse8x;
        break;
    default:
        playbackSpeed_ = PlaybackSpeed::Reverse1x;
        break;
    }
    Q_EMIT playbackSpeedChanged(playbackSpeed_);
    Q_EMIT playbackStateChanged(isPlaying());
}

void EditorEngine::setPlaybackSpeed(PlaybackSpeed speed)
{
    playbackSpeed_ = speed;
    Q_EMIT playbackSpeedChanged(speed);
    Q_EMIT playbackStateChanged(isPlaying());
}

void EditorEngine::deleteSelectedClip()
{
    if (selectedClipId_.isEmpty()) return;

    auto* clip = findClip(selectedClipId_);
    if (!clip) return;

    QString trackId;
    int index = -1;

    for (auto& track : currentSequence_.videoTracks) {
        for (int i = 0; i < track.clips.size(); ++i) {
            if (track.clips[i].id == selectedClipId_) {
                trackId = track.id;
                index = i;
                auto cmd = new DeleteClipCommand(track.id, track.clips[i], i);
                pushUndo(cmd);
                track.clips.removeAt(i);
                break;
            }
        }
        if (!trackId.isEmpty()) break;
    }

    if (trackId.isEmpty()) {
        for (auto& track : currentSequence_.audioTracks) {
            for (int i = 0; i < track.clips.size(); ++i) {
                if (track.clips[i].id == selectedClipId_) {
                    trackId = track.id;
                    index = i;
                    auto cmd = new DeleteClipCommand(track.id, track.clips[i], i);
                    pushUndo(cmd);
                    track.clips.removeAt(i);
                    break;
                }
            }
            if (!trackId.isEmpty()) break;
        }
    }

    selectedClipId_.clear();
    Q_EMIT clipSelectionChanged(QString());
    Q_EMIT projectModified();
    Q_EMIT sequenceChanged(currentSequence_);
}

void EditorEngine::rippleDeleteSelectedClip()
{
    if (selectedClipId_.isEmpty()) return;

    auto* clip = findClip(selectedClipId_);
    if (!clip) return;

    QString trackId;
    int index = -1;
    DemoTrack* track = nullptr;

    for (auto& t : currentSequence_.videoTracks) {
        for (int i = 0; i < t.clips.size(); ++i) {
            if (t.clips[i].id == selectedClipId_) {
                trackId = t.id;
                index = i;
                track = &t;
                break;
            }
        }
        if (track) break;
    }

    if (!track) {
        for (auto& t : currentSequence_.audioTracks) {
            for (int i = 0; i < t.clips.size(); ++i) {
                if (t.clips[i].id == selectedClipId_) {
                    trackId = t.id;
                    index = i;
                    track = &t;
                    break;
                }
            }
            if (track) break;
        }
    }

    if (!track) return;

    FramePosition deletedStart = clip->startFrame;
    FramePosition deletedDuration = clip->duration;

    auto cmd = new DeleteClipCommand(track->id, *clip, index);
    pushUndo(cmd);
    track->clips.removeAt(index);

    for (int i = index; i < track->clips.size(); ++i) {
        track->clips[i].startFrame -= deletedDuration;
    }

    selectedClipId_.clear();
    Q_EMIT clipSelectionChanged(QString());
    Q_EMIT projectModified();
    Q_EMIT sequenceChanged(currentSequence_);
}

void EditorEngine::splitClipAtPlayhead()
{
    if (selectedClipId_.isEmpty()) return;

    auto* clip = findClip(selectedClipId_);
    if (!clip) return;

    if (currentFrame_ <= clip->startFrame || currentFrame_ >= clip->startFrame + clip->duration) {
        return;
    }

    FramePosition splitPoint = currentFrame_ - clip->startFrame;
    FramePosition newDuration = splitPoint;

    DemoClip newClip = *clip;
    newClip.id = generateLegacyId();
    newClip.startFrame = currentFrame_;
    newClip.duration = clip->duration - splitPoint;
    newClip.sourceIn = clip->sourceIn + splitPoint;
    newClip.selected = false;

    clip->duration = newDuration;

    DemoTrack* track = nullptr;
    for (auto& t : currentSequence_.videoTracks) {
        for (auto& c : t.clips) {
            if (c.id == clip->id) {
                track = &t;
                break;
            }
        }
        if (track) break;
    }

    if (!track) {
        for (auto& t : currentSequence_.audioTracks) {
            for (auto& c : t.clips) {
                if (c.id == clip->id) {
                    track = &t;
                    break;
                }
            }
            if (track) break;
        }
    }

    if (track) {
        for (int i = 0; i < track->clips.size(); ++i) {
            if (track->clips[i].id == clip->id) {
                track->clips.insert(i + 1, newClip);
                break;
            }
        }
    }

    Q_EMIT projectModified();
    Q_EMIT sequenceChanged(currentSequence_);
}

void EditorEngine::duplicateSelectedClip()
{
    if (selectedClipId_.isEmpty()) return;

    auto* clip = findClip(selectedClipId_);
    if (!clip) return;

    DemoClip newClip = *clip;
    newClip.id = generateLegacyId();
    newClip.startFrame = clip->startFrame + clip->duration;
    newClip.selected = false;

    DemoTrack* track = nullptr;
    for (auto& t : currentSequence_.videoTracks) {
        for (auto& c : t.clips) {
            if (c.id == clip->id) {
                track = &t;
                break;
            }
        }
        if (track) break;
    }

    if (!track) {
        for (auto& t : currentSequence_.audioTracks) {
            for (auto& c : t.clips) {
                if (c.id == clip->id) {
                    track = &t;
                    break;
                }
            }
            if (track) break;
        }
    }

    if (track) {
        bool inserted = false;
        for (int i = 0; i < track->clips.size(); ++i) {
            if (track->clips[i].startFrame > newClip.startFrame) {
                track->clips.insert(i, newClip);
                inserted = true;
                break;
            }
        }
        if (!inserted) {
            track->clips.push_back(newClip);
        }
        selectClip(newClip.id);
    }

    Q_EMIT projectModified();
    Q_EMIT sequenceChanged(currentSequence_);
}

void EditorEngine::setClipSpeed(const QString& clipId, double speed)
{
    auto* clip = findClip(clipId);
    if (!clip) return;

    clip->speed = speed;
    Q_EMIT clipChanged(clipId);
    Q_EMIT projectModified();
}

void EditorEngine::setClipReversed(const QString& clipId, bool reversed)
{
    auto* clip = findClip(clipId);
    if (!clip) return;

    clip->reversed = reversed;
    Q_EMIT clipChanged(clipId);
    Q_EMIT projectModified();
}

void EditorEngine::setClipVolume(const QString& clipId, double volume)
{
    auto* clip = findClip(clipId);
    if (!clip) return;

    clip->volume = qMax(0.0, qMin(2.0, volume));
    Q_EMIT clipChanged(clipId);
    Q_EMIT projectModified();
}

void EditorEngine::setClipName(const QString& clipId, const QString& name)
{
    auto* clip = findClip(clipId);
    if (!clip) return;

    clip->name = name;
    Q_EMIT clipChanged(clipId);
    Q_EMIT projectModified();
}

void EditorEngine::cutClip(const QString& clipId)
{
    auto* clip = findClip(clipId);
    if (!clip) return;

    clipboard_ = *clip;
    deleteSelectedClip();
}

void EditorEngine::copyClip(const QString& clipId)
{
    auto* clip = findClip(clipId);
    if (!clip) return;

    clipboard_ = *clip;
}

void EditorEngine::pasteClip(FramePosition targetFrame)
{
    if (clipboard_.id.isEmpty()) return;

    DemoClip newClip = clipboard_;
    newClip.id = generateId(QStringLiteral("clip"));
    newClip.startFrame = targetFrame;
    newClip.selected = false;

    DemoTrack* targetTrack = nullptr;
    if (targetFrame < 0) targetFrame = currentFrame_;

    for (auto& track : currentSequence_.videoTracks) {
        targetTrack = &track;
        break;
    }

    if (!targetTrack) {
        for (auto& track : currentSequence_.audioTracks) {
            targetTrack = &track;
            break;
        }
    }

    if (targetTrack) {
        bool inserted = false;
        for (int i = 0; i < targetTrack->clips.size(); ++i) {
            if (targetTrack->clips[i].startFrame > newClip.startFrame) {
                targetTrack->clips.insert(i, newClip);
                inserted = true;
                break;
            }
        }
        if (!inserted) {
            targetTrack->clips.push_back(newClip);
        }
        selectClip(newClip.id);
        Q_EMIT projectModified();
        Q_EMIT sequenceChanged(currentSequence_);
    }
}

FramePosition EditorEngine::snapToNearest(FramePosition frame, bool forLeftEdge)
{
    if (!snapEnabled_) return frame;

    const int SNAP_THRESHOLD = 5;

    if (qAbs(frame - currentFrame_) <= SNAP_THRESHOLD) {
        return currentFrame_;
    }

    for (const auto& track : currentSequence_.videoTracks) {
        for (const auto& clip : track.clips) {
            if (forLeftEdge) {
                if (qAbs(frame - clip.startFrame) <= SNAP_THRESHOLD) {
                    return clip.startFrame;
                }
                if (qAbs(frame - (clip.startFrame + clip.duration)) <= SNAP_THRESHOLD) {
                    return clip.startFrame + clip.duration;
                }
            } else {
                if (qAbs(frame - clip.startFrame) <= SNAP_THRESHOLD) {
                    return clip.startFrame;
                }
            }
        }
    }

    for (const auto& track : currentSequence_.audioTracks) {
        for (const auto& clip : track.clips) {
            if (forLeftEdge) {
                if (qAbs(frame - clip.startFrame) <= SNAP_THRESHOLD) {
                    return clip.startFrame;
                }
                if (qAbs(frame - (clip.startFrame + clip.duration)) <= SNAP_THRESHOLD) {
                    return clip.startFrame + clip.duration;
                }
            } else {
                if (qAbs(frame - clip.startFrame) <= SNAP_THRESHOLD) {
                    return clip.startFrame;
                }
            }
        }
    }

    for (const auto& marker : currentSequence_.markers) {
        if (qAbs(frame - marker.position) <= SNAP_THRESHOLD) {
            return marker.position;
        }
    }

    return frame;
}

void EditorEngine::pushUndo(UndoCommand* cmd)
{
    undoStack_.push_back(cmd);
    qDeleteAll(redoStack_);
    redoStack_.clear();
}

void EditorEngine::undo()
{
    if (undoStack_.isEmpty()) return;
    auto* cmd = undoStack_.takeLast();
    cmd->undo();
    redoStack_.push_back(cmd);
    Q_EMIT projectModified();
    Q_EMIT sequenceChanged(currentSequence_);
}

void EditorEngine::redo()
{
    if (redoStack_.isEmpty()) return;
    auto* cmd = redoStack_.takeLast();
    cmd->redo();
    undoStack_.push_back(cmd);
    Q_EMIT projectModified();
    Q_EMIT sequenceChanged(currentSequence_);
}

QString EditorEngine::generateId(const QString& prefix)
{
    static int counter = 0;
    return QStringLiteral("%1_%2").arg(prefix).arg(++counter);
}

void MoveClipCommand::doMove(FramePosition pos)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    auto* clip = engine->findClip(clipId_);
    if (clip) {
        clip->startFrame = pos;
    }
}

void MoveClipCommand::undo()
{
    doMove(oldStart_);
}

void TrimClipCommand::doTrim(FramePosition start, FramePosition duration)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    auto* clip = engine->findClip(clipId_);
    if (clip) {
        clip->startFrame = start;
        clip->duration = duration;
    }
}

void TrimClipCommand::undo()
{
    doTrim(oldStart_, oldDuration_);
}

void DeleteClipCommand::undo()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    auto* track = engine->findTrack(trackId_);
    if (track && index_ >= 0 && index_ <= track->clips.size()) {
        track->clips.insert(index_, clip_);
    }
}

void DeleteClipCommand::redo()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    auto* track = engine->findTrack(trackId_);
    if (track) {
        for (int i = 0; i < track->clips.size(); ++i) {
            if (track->clips[i].id == clip_.id) {
                track->clips.removeAt(i);
                break;
            }
        }
    }
}

void EditorEngine::addMarker(FramePosition position, const QString& name, const QString& comment)
{
    Marker marker;
    marker.id = generateMarkerId();
    marker.position = position;
    marker.name = name.isEmpty() ? QStringLiteral("Marker %1").arg(currentSequence_.markers.size() + 1) : name;
    marker.comment = comment;
    marker.color = QColor(Qt::yellow);
    marker.type = Marker::Type::Comment;

    currentSequence_.markers.append(marker);

    pushUndo(new AddMarkerCommand(marker));
    Q_EMIT markerChanged();
    Q_EMIT projectModified();
}

void EditorEngine::deleteMarker(const QString& markerId)
{
    for (int i = 0; i < currentSequence_.markers.size(); ++i) {
        if (currentSequence_.markers[i].id == markerId) {
            auto marker = currentSequence_.markers[i];
            pushUndo(new DeleteMarkerCommand(marker, i));
            currentSequence_.markers.removeAt(i);
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }
}

void EditorEngine::moveMarker(const QString& markerId, FramePosition newPosition)
{
    for (auto& marker : currentSequence_.markers) {
        if (marker.id == markerId) {
            marker.position = newPosition;
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }
}

void EditorEngine::setMarkerName(const QString& markerId, const QString& name)
{
    for (auto& marker : currentSequence_.markers) {
        if (marker.id == markerId) {
            marker.name = name;
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }
}

void EditorEngine::setMarkerComment(const QString& markerId, const QString& comment)
{
    for (auto& marker : currentSequence_.markers) {
        if (marker.id == markerId) {
            marker.comment = comment;
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }
}

void EditorEngine::clearMarkers()
{
    currentSequence_.markers.clear();
    Q_EMIT markerChanged();
    Q_EMIT projectModified();
}

void EditorEngine::addTransitionAtPlayhead(TransitionType type, FramePosition duration)
{
    if (selectedClipId_.isEmpty()) return;

    auto* selectedClip = findClip(selectedClipId_);
    if (!selectedClip) return;

    FramePosition playhead = currentFrame_;

    for (auto& track : currentSequence_.videoTracks) {
        for (int i = 0; i < track.clips.size(); ++i) {
            if (track.clips[i].id == selectedClipId_) {
                if (i + 1 < track.clips.size()) {
                    auto& leftClip = track.clips[i];
                    auto& rightClip = track.clips[i + 1];

                    if (playhead >= leftClip.startFrame && playhead <= leftClip.startFrame + leftClip.duration) {
                        FramePosition transStart = playhead;
                        addTransition(track.id, leftClip.id, rightClip.id, transStart, type, duration);
                        return;
                    }
                }
                if (i > 0) {
                    auto& leftClip = track.clips[i - 1];
                    auto& rightClip = track.clips[i];

                    if (playhead >= rightClip.startFrame && playhead <= rightClip.startFrame + rightClip.duration) {
                        FramePosition transStart = rightClip.startFrame;
                        addTransition(track.id, leftClip.id, rightClip.id, transStart, type, duration);
                        return;
                    }
                }
            }
        }
    }

    for (auto& track : currentSequence_.audioTracks) {
        for (int i = 0; i < track.clips.size(); ++i) {
            if (track.clips[i].id == selectedClipId_) {
                if (i + 1 < track.clips.size()) {
                    auto& leftClip = track.clips[i];
                    auto& rightClip = track.clips[i + 1];

                    if (playhead >= leftClip.startFrame && playhead <= leftClip.startFrame + leftClip.duration) {
                        FramePosition transStart = playhead;
                        addTransition(track.id, leftClip.id, rightClip.id, transStart, type, duration);
                        return;
                    }
                }
            }
        }
    }
}

void EditorEngine::addTransition(const QString& trackId, const QString& leftClipId, const QString& rightClipId,
                                 FramePosition startFrame, TransitionType type, FramePosition duration)
{
    Transition trans;
    trans.id = generateTransitionId();
    trans.trackId = trackId;
    trans.leftClipId = leftClipId;
    trans.rightClipId = rightClipId;
    trans.startFrame = startFrame;
    trans.duration = duration;
    trans.type = type;

    currentSequence_.transitions.append(trans);
    Q_EMIT transitionChanged();
    Q_EMIT projectModified();
}

void EditorEngine::deleteTransition(const QString& transitionId)
{
    for (int i = 0; i < currentSequence_.transitions.size(); ++i) {
        if (currentSequence_.transitions[i].id == transitionId) {
            currentSequence_.transitions.removeAt(i);
            Q_EMIT transitionChanged();
            Q_EMIT projectModified();
            return;
        }
    }
}

QString EditorEngine::transitionTypeToString(TransitionType type)
{
    return transitionTypeToStringLocal(type);
}

TransitionType EditorEngine::stringToTransitionType(const QString& str)
{
    return stringToTransitionTypeLocal(str);
}

void EditorEngine::addMediaToPool(const QString& filePath, const QString& name, const QString& type)
{
    MediaItem media;
    media.id = generateMediaId();
    media.name = name.isEmpty() ? QFileInfo(filePath).fileName() : name;
    media.filePath = filePath;
    media.type = type;
    media.hasProxy = false;

    currentProject_.mediaPool.push_back(media);
    currentProject_.modifiedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    Q_EMIT projectModified();
}

void EditorEngine::removeMediaFromPool(const QString& mediaId)
{
    for (int i = 0; i < currentProject_.mediaPool.size(); ++i) {
        if (currentProject_.mediaPool[i].id == mediaId) {
            currentProject_.mediaPool.removeAt(i);
            currentProject_.modifiedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
            Q_EMIT projectModified();
            return;
        }
    }
}

QJsonObject markerToJson(const Marker& marker)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = marker.id;
    obj[QStringLiteral("position")] = marker.position;
    obj[QStringLiteral("name")] = marker.name;
    obj[QStringLiteral("comment")] = marker.comment;
    obj[QStringLiteral("color")] = marker.color.name();
    obj[QStringLiteral("type")] = static_cast<int>(marker.type);
    return obj;
}

Marker jsonToMarker(const QJsonObject& obj)
{
    Marker marker;
    marker.id = obj[QStringLiteral("id")].toString();
    marker.position = obj[QStringLiteral("position")].toInteger();
    marker.name = obj[QStringLiteral("name")].toString();
    marker.comment = obj[QStringLiteral("comment")].toString();
    marker.color = QColor(obj[QStringLiteral("color")].toString(QStringLiteral("#ffff00")));
    marker.type = static_cast<Marker::Type>(obj[QStringLiteral("type")].toInt(0));
    return marker;
}

QJsonObject transitionToJson(const Transition& trans)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = trans.id;
    obj[QStringLiteral("trackId")] = trans.trackId;
    obj[QStringLiteral("leftClipId")] = trans.leftClipId;
    obj[QStringLiteral("rightClipId")] = trans.rightClipId;
    obj[QStringLiteral("startFrame")] = trans.startFrame;
    obj[QStringLiteral("duration")] = trans.duration;
    obj[QStringLiteral("type")] = transitionTypeToStringLocal(trans.type);
    return obj;
}

Transition jsonToTransition(const QJsonObject& obj)
{
    Transition trans;
    trans.id = obj[QStringLiteral("id")].toString();
    trans.trackId = obj[QStringLiteral("trackId")].toString();
    trans.leftClipId = obj[QStringLiteral("leftClipId")].toString();
    trans.rightClipId = obj[QStringLiteral("rightClipId")].toString();
    trans.startFrame = obj[QStringLiteral("startFrame")].toInteger();
    trans.duration = obj[QStringLiteral("duration")].toInteger();
    trans.type = stringToTransitionTypeLocal(obj[QStringLiteral("type")].toString());
    return trans;
}

QJsonObject clipToJson(const DemoClip& clip)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = clip.id;
    obj[QStringLiteral("name")] = clip.name;
    obj[QStringLiteral("sourceFile")] = clip.sourceFile;
    obj[QStringLiteral("startFrame")] = clip.startFrame;
    obj[QStringLiteral("duration")] = clip.duration;
    obj[QStringLiteral("sourceIn")] = clip.sourceIn;
    obj[QStringLiteral("sourceOut")] = clip.sourceOut;
    obj[QStringLiteral("color")] = clip.color;
    obj[QStringLiteral("linked")] = clip.linked;
    obj[QStringLiteral("reversed")] = clip.reversed;
    obj[QStringLiteral("speed")] = clip.speed;
    obj[QStringLiteral("volume")] = clip.volume;

    QJsonObject effects;
    for (auto it = clip.effects.constKeyValueBegin(); it != clip.effects.constKeyValueEnd(); ++it) {
        effects[it->first] = it->second.toString();
    }
    obj[QStringLiteral("effects")] = effects;

    return obj;
}

DemoClip jsonToClip(const QJsonObject& obj)
{
    DemoClip clip;
    clip.id = obj[QStringLiteral("id")].toString();
    clip.name = obj[QStringLiteral("name")].toString();
    clip.sourceFile = obj[QStringLiteral("sourceFile")].toString();
    clip.startFrame = obj[QStringLiteral("startFrame")].toInteger();
    clip.duration = obj[QStringLiteral("duration")].toInteger();
    clip.sourceIn = obj[QStringLiteral("sourceIn")].toInteger();
    clip.sourceOut = obj[QStringLiteral("sourceOut")].toInteger();
    clip.color = obj[QStringLiteral("color")].toString(QStringLiteral("#4a9eff"));
    clip.linked = obj[QStringLiteral("linked")].toBool();
    clip.reversed = obj[QStringLiteral("reversed")].toBool();
    clip.speed = obj[QStringLiteral("speed")].toDouble(1.0);
    clip.volume = obj[QStringLiteral("volume")].toDouble(1.0);

    QJsonObject effects = obj[QStringLiteral("effects")].toObject();
    for (const auto& key : effects.keys()) {
        clip.effects[key] = effects.value(key).toString();
    }

    return clip;
}

QJsonObject trackToJson(const DemoTrack& track)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = track.id;
    obj[QStringLiteral("name")] = track.name;
    obj[QStringLiteral("kind")] = track.kind;
    obj[QStringLiteral("muted")] = track.muted;
    obj[QStringLiteral("solo")] = track.solo;
    obj[QStringLiteral("height")] = track.height;

    QJsonArray clips;
    for (const auto& clip : track.clips) {
        clips.append(clipToJson(clip));
    }
    obj[QStringLiteral("clips")] = clips;

    return obj;
}

DemoTrack jsonToTrack(const QJsonObject& obj)
{
    DemoTrack track;
    track.id = obj[QStringLiteral("id")].toString();
    track.name = obj[QStringLiteral("name")].toString();
    track.kind = obj[QStringLiteral("kind")].toString();
    track.muted = obj[QStringLiteral("muted")].toBool();
    track.solo = obj[QStringLiteral("solo")].toBool();
    track.height = obj[QStringLiteral("height")].toInt(28);

    QJsonArray clips = obj[QStringLiteral("clips")].toArray();
    for (const auto& clipVal : clips) {
        track.clips.append(jsonToClip(clipVal.toObject()));
    }

    return track;
}

QJsonObject sequenceToJson(const DemoSequence& seq)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = seq.id;
    obj[QStringLiteral("name")] = seq.name;
    obj[QStringLiteral("resolution")] = seq.resolution;
    obj[QStringLiteral("frameRate")] = seq.frameRate;
    obj[QStringLiteral("duration")] = seq.duration;

    QJsonArray videoTracks;
    for (const auto& track : seq.videoTracks) {
        videoTracks.append(trackToJson(track));
    }
    obj[QStringLiteral("videoTracks")] = videoTracks;

    QJsonArray audioTracks;
    for (const auto& track : seq.audioTracks) {
        audioTracks.append(trackToJson(track));
    }
    obj[QStringLiteral("audioTracks")] = audioTracks;

    QJsonArray markers;
    for (const auto& marker : seq.markers) {
        markers.append(markerToJson(marker));
    }
    obj[QStringLiteral("markers")] = markers;

    QJsonArray transitions;
    for (const auto& trans : seq.transitions) {
        transitions.append(transitionToJson(trans));
    }
    obj[QStringLiteral("transitions")] = transitions;

    return obj;
}

DemoSequence jsonToSequence(const QJsonObject& obj)
{
    DemoSequence seq;
    seq.id = obj[QStringLiteral("id")].toString();
    seq.name = obj[QStringLiteral("name")].toString();
    seq.resolution = obj[QStringLiteral("resolution")].toString(QStringLiteral("1920x1080"));
    seq.frameRate = obj[QStringLiteral("frameRate")].toString(QStringLiteral("30 fps"));
    seq.duration = obj[QStringLiteral("duration")].toInteger(350);

    QJsonArray videoTracks = obj[QStringLiteral("videoTracks")].toArray();
    for (const auto& trackVal : videoTracks) {
        seq.videoTracks.append(jsonToTrack(trackVal.toObject()));
    }

    QJsonArray audioTracks = obj[QStringLiteral("audioTracks")].toArray();
    for (const auto& trackVal : audioTracks) {
        seq.audioTracks.append(jsonToTrack(trackVal.toObject()));
    }

    QJsonArray markers = obj[QStringLiteral("markers")].toArray();
    for (const auto& markerVal : markers) {
        seq.markers.append(jsonToMarker(markerVal.toObject()));
    }

    QJsonArray transitions = obj[QStringLiteral("transitions")].toArray();
    for (const auto& transVal : transitions) {
        seq.transitions.append(jsonToTransition(transVal.toObject()));
    }

    return seq;
}

QJsonObject mediaItemToJson(const MediaItem& media)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = media.id;
    obj[QStringLiteral("name")] = media.name;
    obj[QStringLiteral("filePath")] = media.filePath;
    obj[QStringLiteral("type")] = media.type;
    obj[QStringLiteral("resolution")] = media.resolution;
    obj[QStringLiteral("duration")] = media.duration;
    obj[QStringLiteral("hasProxy")] = media.hasProxy;
    obj[QStringLiteral("proxyPath")] = media.proxyPath;
    return obj;
}

MediaItem jsonToMediaItem(const QJsonObject& obj)
{
    MediaItem media;
    media.id = obj[QStringLiteral("id")].toString();
    media.name = obj[QStringLiteral("name")].toString();
    media.filePath = obj[QStringLiteral("filePath")].toString();
    media.type = obj[QStringLiteral("type")].toString();
    media.resolution = obj[QStringLiteral("resolution")].toString();
    media.duration = obj[QStringLiteral("duration")].toString();
    media.hasProxy = obj[QStringLiteral("hasProxy")].toBool();
    media.proxyPath = obj[QStringLiteral("proxyPath")].toString();
    return media;
}

QJsonObject projectToJson(const DemoProject& project)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = project.id;
    obj[QStringLiteral("name")] = project.name;
    obj[QStringLiteral("version")] = project.version;
    obj[QStringLiteral("createdAt")] = project.createdAt;
    obj[QStringLiteral("modifiedAt")] = project.modifiedAt;
    obj[QStringLiteral("activeSequenceId")] = project.activeSequenceId;

    QJsonArray mediaPool;
    for (const auto& media : project.mediaPool) {
        mediaPool.append(mediaItemToJson(media));
    }
    obj[QStringLiteral("mediaPool")] = mediaPool;

    QJsonArray sequences;
    for (const auto& seq : project.sequences) {
        sequences.append(sequenceToJson(seq));
    }
    obj[QStringLiteral("sequences")] = sequences;

    return obj;
}

DemoProject jsonToProject(const QJsonObject& obj)
{
    DemoProject project;
    project.id = obj[QStringLiteral("id")].toString();
    project.name = obj[QStringLiteral("name")].toString();
    project.version = obj[QStringLiteral("version")].toString(QStringLiteral("1.0"));
    project.createdAt = obj[QStringLiteral("createdAt")].toString();
    project.modifiedAt = obj[QStringLiteral("modifiedAt")].toString();
    project.activeSequenceId = obj[QStringLiteral("activeSequenceId")].toString();

    QJsonArray mediaPool = obj[QStringLiteral("mediaPool")].toArray();
    for (const auto& mediaVal : mediaPool) {
        project.mediaPool.append(jsonToMediaItem(mediaVal.toObject()));
    }

    QJsonArray sequences = obj[QStringLiteral("sequences")].toArray();
    for (const auto& seqVal : sequences) {
        project.sequences.append(jsonToSequence(seqVal.toObject()));
    }

    return project;
}

bool EditorEngine::saveProject(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        Q_EMIT projectSaved(false, QStringLiteral("Failed to open file for writing"));
        return false;
    }

    currentProject_.modifiedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject json = projectToJson(currentProject_);
    QJsonDocument doc(json);

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    Q_EMIT projectSaved(true, QStringLiteral("Project saved successfully"));
    return true;
}

bool EditorEngine::loadProject(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT projectLoaded(false, QStringLiteral("Failed to open file for reading"));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        Q_EMIT projectLoaded(false, QStringLiteral("JSON parse error: %1").arg(error.errorString()));
        return false;
    }

    currentProject_ = jsonToProject(doc.object());

    for (const auto& seq : currentProject_.sequences) {
        if (seq.id == currentProject_.activeSequenceId) {
            currentSequence_ = seq;
            break;
        }
    }

    currentFrame_ = 0;
    inPoint_ = 0;
    outPoint_ = currentSequence_.duration;
    playbackSpeed_ = PlaybackSpeed::Stop;
    selectedClipId_.clear();

    qDeleteAll(undoStack_);
    undoStack_.clear();
    qDeleteAll(redoStack_);
    redoStack_.clear();

    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectLoaded(true, QStringLiteral("Project loaded successfully"));
    return true;
}

void AddMarkerCommand::undo()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    for (int i = 0; i < engine->currentSequence().markers.size(); ++i) {
        if (engine->currentSequence().markers[i].id == marker_.id) {
            engine->currentSequence().markers.removeAt(i);
            break;
        }
    }
    Q_EMIT engine->markerChanged();
}

void AddMarkerCommand::redo()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->currentSequence().markers.append(marker_);
    Q_EMIT engine->markerChanged();
}

void DeleteMarkerCommand::undo()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->currentSequence().markers.insert(index_, marker_);
    Q_EMIT engine->markerChanged();
}

void DeleteMarkerCommand::redo()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    for (int i = 0; i < engine->currentSequence().markers.size(); ++i) {
        if (engine->currentSequence().markers[i].id == marker_.id) {
            engine->currentSequence().markers.removeAt(i);
            break;
        }
    }
    Q_EMIT engine->markerChanged();
}

} // namespace ArtifactPr

