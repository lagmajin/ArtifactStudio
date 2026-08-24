module;
#include <QColor>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QMetaObject>
#include <QHash>
#include <QSet>
#include <cmath>
#include <memory>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

module ArtifactPr.EditorEngine;

import ArtifactPr.EditCommand;
import ArtifactPr.SequenceExporter;

using namespace Qt::StringLiterals;

namespace ArtifactPr {

// コマンド serialize 用の前方向宣言 (定義はファイル後半・namespace ArtifactPr 直下)。
QJsonObject clipToJson(const DemoClip& clip);
DemoClip jsonToClip(const QJsonObject& obj);
QJsonObject markerToJson(const Marker& marker);
Marker jsonToMarker(const QJsonObject& obj);
QJsonObject transitionToJson(const Transition& trans);
Transition jsonToTransition(const QJsonObject& obj);
QJsonObject trackToJson(const DemoTrack& track);
DemoTrack jsonToTrack(const QJsonObject& obj);

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

void EditorEngine::rebuildLegacySnapshotFromNLE()
{
    if (!nleStore_ || currentProject_.activeSequenceId.isEmpty()) {
        return;
    }

    const auto sequenceId = ArtifactCore::NLE::SequenceId::fromString(currentProject_.activeSequenceId);
    const auto* sequence = nleStore_->sequence(sequenceId);
    if (!sequence) {
        return;
    }

    DemoSequence snapshot;
    snapshot.id = sequence->id.toString();
    snapshot.name = sequence->name;
    snapshot.resolution = QStringLiteral("1920x1080");
    snapshot.frameRate = QStringLiteral("%1 fps").arg(sequence->timeBase.fps(), 0, 'f', 3);
    snapshot.duration = sequence->duration.isValid() ? sequence->duration.end() : 0;

    for (const auto& trackId : nleStore_->trackIds(sequence->id)) {
        const auto* coreTrack = nleStore_->track(trackId);
        if (!coreTrack) {
            continue;
        }

        DemoTrack track;
        track.id = coreTrack->id.toString();
        track.name = coreTrack->name;
        track.kind = coreTrack->kind == ArtifactCore::NLE::TrackKind::Audio
            ? QStringLiteral("audio")
            : QStringLiteral("video");
        track.muted = coreTrack->mute;
        track.solo = coreTrack->solo;

        for (const auto& clipId : coreTrack->clipOrder) {
            const auto* coreClip = nleStore_->clip(clipId);
            if (!coreClip || !coreClip->timelineRange.isValid()) {
                continue;
            }

            DemoClip clip;
            clip.id = coreClip->id.toString();
            clip.name = coreClip->name;
            clip.startFrame = coreClip->timelineRange.start();
            clip.duration = coreClip->timelineRange.duration();
            clip.sourceIn = coreClip->sourceRange.isValid() ? coreClip->sourceRange.start() : 0;
            clip.sourceOut = coreClip->sourceRange.isValid() ? coreClip->sourceRange.end() : clip.sourceIn + clip.duration;
            clip.speed = coreClip->speed;
            clip.reversed = coreClip->reversed;
            clip.opacity = coreClip->opacity;
            clip.enabled = coreClip->enabled;
            clip.linked = coreClip->linkedGroupId != 0;
            clip.selected = coreClip->selected;
            clip.color = track.kind == QStringLiteral("audio")
                ? QStringLiteral("#4eff4a")
                : QStringLiteral("#4a9eff");

            if (const auto* source = nleStore_->source(coreClip->sourceId)) {
                clip.sourceFile = source->useProxy && !source->proxyUri.isEmpty()
                    ? source->proxyUri
                    : source->uri;
            }
            track.clips.push_back(clip);
        }

        if (coreTrack->kind == ArtifactCore::NLE::TrackKind::Audio) {
            snapshot.audioTracks.push_back(track);
        } else {
            snapshot.videoTracks.push_back(track);
        }
    }

    // markers / transitions を NLE → legacy へ復元
    for (const auto& coreMarkerId : sequence->markers) {
        const auto* coreMarker = nleStore_->marker(coreMarkerId);
        if (!coreMarker) continue;
        Marker marker;
        marker.id = coreMarker->id.toString();
        marker.position = coreMarker->position;
        marker.name = coreMarker->name;
        marker.comment = coreMarker->note;
        marker.color = coreMarker->color;
        marker.type = Marker::Type::Comment;
        snapshot.markers.append(marker);
    }

    for (const auto& trackId : sequence->trackOrder) {
        for (const auto* coreTransition : nleStore_->transitions(trackId)) {
            if (!coreTransition) continue;
            Transition trans;
            trans.id = coreTransition->id.toString();
            trans.trackId = coreTransition->trackId.toString();
            trans.leftClipId = coreTransition->leftClipId.toString();
            trans.rightClipId = coreTransition->rightClipId.toString();
            trans.startFrame = coreTransition->range.start();
            trans.duration = static_cast<FramePosition>(coreTransition->duration + 0.5);
            switch (coreTransition->kind) {
            case ArtifactCore::NLE::TransitionKind::Dissolve:
                trans.type = TransitionType::DipToBlack;
                break;
            case ArtifactCore::NLE::TransitionKind::Wipe:
                trans.type = coreTransition->direction
                        == ArtifactCore::NLE::Transition::Direction::RightToLeft
                    ? TransitionType::WipeLeft
                    : TransitionType::WipeRight;
                break;
            default:
                trans.type = TransitionType::Crossfade;
                break;
            }
            snapshot.transitions.append(trans);
        }
    }

    currentSequence_ = snapshot;
    for (auto& projectSequence : currentProject_.sequences) {
        if (projectSequence.id == snapshot.id) {
            projectSequence = snapshot;
            break;
        }
    }
    Q_EMIT sequenceChanged(currentSequence_);
}

bool EditorEngine::resolveSourceForClip(const QString& sourceFile,
                                        ArtifactCore::NLE::SourceRef& outRef)
{
    if (sourceFile.isEmpty() || !nleStore_) return false;

    // 既存の uri と一致するソースがあればそれを使う
    for (const auto& seqId : nleStore_->sequenceIds()) {
        for (const auto& tid : nleStore_->trackIds(seqId)) {
            for (const auto& cid : nleStore_->clipIds(tid)) {
                const auto* c = nleStore_->clip(cid);
                if (!c) continue;
                const auto* existing = nleStore_->source(c->sourceId);
                if (existing && existing->uri == sourceFile) {
                    outRef = *existing;
                    return true;
                }
            }
        }
    }

    // 無ければプローブして新規登録
    ArtifactCore::NLE::SourceRef probe;
    probe.uri = sourceFile;
    probe.displayName = QFileInfo(sourceFile).fileName();

    const QString suffix = QFileInfo(sourceFile).suffix().toLower();
    static const QSet<QString> imageExts = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("bmp"), QStringLiteral("tif"), QStringLiteral("tiff"),
        QStringLiteral("webp")
    };
    const bool isImage = imageExts.contains(suffix);

    bool probed = false;
    cv::Mat imageOrFrame;
    if (isImage) {
        imageOrFrame = cv::imread(sourceFile.toStdString(), cv::IMREAD_COLOR);
        probed = !imageOrFrame.empty();
        if (probed) {
            probe.mimeType = QStringLiteral("image");
            probe.timeBase = ArtifactCore::NLE::TimeBase{1, 30, false};
            probe.availableRange = ArtifactCore::FrameRange::fromFrameCount(0, 1);
            probe.frameSize = QSize(imageOrFrame.cols, imageOrFrame.rows);
        }
    } else {
        cv::VideoCapture capture(sourceFile.toStdString(), cv::CAP_FFMPEG);
        if (capture.isOpened()) {
            const double fps = capture.get(cv::CAP_PROP_FPS);
            const double frameCount = capture.get(cv::CAP_PROP_FRAME_COUNT);
            const int w = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
            const int h = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
            if (w > 0 && h > 0 && fps > 0.0) {
                // fps を有理数へ (numerator=1000, denominator=round(1000/fps))
                const int den = qMax(1, static_cast<int>(std::lround(1000.0 / fps)));
                probe.timeBase = ArtifactCore::NLE::TimeBase{1000, den, false};
                const qint64 frames = frameCount > 0
                    ? static_cast<qint64>(frameCount) : 0;
                probe.availableRange = frames > 0
                    ? ArtifactCore::FrameRange::fromFrameCount(0, frames)
                    : ArtifactCore::FrameRange::infinite();
                probe.frameSize = QSize(w, h);
                probe.mimeType = QStringLiteral("video");
                probed = true;
            }
        }
    }

    if (!probed) {
        // プローブ失敗でもインポートは止めない。既定値で継続。
        probe.timeBase = ArtifactCore::NLE::TimeBase{1, 30, false};
        probe.availableRange = ArtifactCore::FrameRange::fromFrameCount(0, 100000);
    }

    outRef = probe;
    return true;
}

void EditorEngine::importLegacyProjectToNLE()
{
    if (!nleStore_ || currentProject_.activeSequenceId.isEmpty()) return;

    nleStore_->clear();

    // アクティブシーケンスとトラックを再構築
    DemoSequence* legacySequence = nullptr;
    for (auto& seq : currentProject_.sequences) {
        if (seq.id == currentProject_.activeSequenceId) {
            legacySequence = &seq;
            break;
        }
    }
    if (!legacySequence) return;

    const auto coreSequenceId = nleStore_->createSequence(
        legacySequence->name, ArtifactCore::NLE::TimeBase{1, 30, false});
    currentProject_.activeSequenceId = coreSequenceId.toString();

    auto importTrack = [&](const DemoTrack& legacyTrack) {
        const auto trackKind = legacyTrack.kind == QStringLiteral("audio")
            ? ArtifactCore::NLE::TrackKind::Audio
            : ArtifactCore::NLE::TrackKind::Video;
        const auto coreTrackId = nleStore_->createTrack(coreSequenceId, trackKind, legacyTrack.name);
        for (const auto& legacyClip : legacyTrack.clips) {
            if (legacyClip.sourceFile.isEmpty()) continue;
            ArtifactCore::NLE::SourceRef probe;
            if (!resolveSourceForClip(legacyClip.sourceFile, probe)) continue;
            const auto sourceId = nleStore_->registerSource(probe);

            ArtifactCore::NLE::ClipDraft draft;
            draft.sourceId = sourceId;
            const FramePosition sourceLen = qMax<FramePosition>(
                1, legacyClip.sourceOut - legacyClip.sourceIn);
            draft.sourceRange = ArtifactCore::FrameRange::fromFrameCount(
                legacyClip.sourceIn, sourceLen);
            draft.timelineRange = ArtifactCore::FrameRange::fromFrameCount(
                legacyClip.startFrame, qMax<FramePosition>(1, legacyClip.duration));
            draft.trimRange = draft.sourceRange;
            draft.name = legacyClip.name;
            draft.speed = legacyClip.speed;
            draft.opacity = legacyClip.opacity;
            draft.enabled = legacyClip.enabled;
            draft.reversed = legacyClip.reversed;
            nleStore_->addClip(coreSequenceId, coreTrackId, draft);
        }
    };
    for (const auto& legacyTrack : legacySequence->videoTracks) importTrack(legacyTrack);
    for (const auto& legacyTrack : legacySequence->audioTracks) importTrack(legacyTrack);

    // マーカーを再構築
    for (const auto& legacyMarker : legacySequence->markers) {
        nleStore_->createMarker(coreSequenceId,
                                ArtifactCore::FramePosition(legacyMarker.position),
                                legacyMarker.name, legacyMarker.comment,
                                legacyMarker.color);
    }

    rebuildLegacySnapshotFromNLE();
    currentSequence_ = *legacySequence;
    for (auto& projectSequence : currentProject_.sequences) {
        if (projectSequence.id == legacySequence->id) {
            projectSequence = currentSequence_;
            break;
        }
    }
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
    nleStore_ = std::make_unique<ArtifactCore::NLE::NLEProjectStore>();
    newProject();
}

EditorEngine::~EditorEngine()
{
    cancelExport();
    if (exportThread_.joinable()) {
        exportThread_.join();
    }
    editSession_.undoStack()->clear();
}

void EditorEngine::startExport(const ArtifactPr::RenderPlan& plan,
                               const ArtifactPr::ExportSettings& settings)
{
    bool expected = false;
    if (!isExporting_.compare_exchange_strong(expected, true)) {
        return;
    }

    cancelRequested_.store(false);
    Q_EMIT exportStarted();

    if (exportThread_.joinable()) {
        exportThread_.join();
    }
    exportThread_ = std::thread([this, plan, settings]() {
        auto progress = [this](int percent) {
            QMetaObject::invokeMethod(this, [this, percent]() {
                Q_EMIT exportProgress(percent);
            }, Qt::QueuedConnection);
        };

        const ExportResult result =
            exportSequence(plan, settings, cancelRequested_, progress);

        isExporting_.store(false);
        QMetaObject::invokeMethod(this, [this, result]() {
            Q_EMIT exportFinished(result.success, result.error);
        }, Qt::QueuedConnection);
    });
}

void EditorEngine::cancelExport()
{
    cancelRequested_.store(true);
}

QJsonObject EditorEngine::nleSnapshot() const
{
    return nleStore_ ? nleStore_->toJson() : QJsonObject{};
}

RenderPlan EditorEngine::createRenderPlan(RenderQualityPreset preset,
                                          FramePosition startFrame,
                                          FramePosition endFrame) const
{
    RenderPlan plan;
    plan.nleSnapshot = nleSnapshot();
    plan.resolution = currentSequence_.resolution;
    plan.frameRate = currentSequence_.frameRate;
    const FramePosition maxFrame = qMax<FramePosition>(0, currentSequence_.duration);
    const FramePosition requestedStart = startFrame >= 0 ? startFrame : inPoint_;
    const FramePosition requestedEnd = endFrame >= 0 ? endFrame : outPoint_;
    plan.startFrame = qMax<FramePosition>(0, qMin(requestedStart, maxFrame));
    plan.endFrame = qMax<FramePosition>(plan.startFrame,
                                        qMin(requestedEnd, maxFrame));

    switch (preset) {
    case RenderQualityPreset::Draft:
        plan.qualityScale = 0.25;
        plan.useProxyMedia = true;
        break;
    case RenderQualityPreset::Preview:
        plan.qualityScale = 0.5;
        plan.useProxyMedia = true;
        break;
    case RenderQualityPreset::Full:
        plan.qualityScale = 1.0;
        plan.useProxyMedia = false;
        break;
    }

    return plan;
}

bool EditorEngine::restoreNLESnapshot(const QJsonObject& snapshot)
{
    if (!nleStore_ || !nleStore_->loadFromJson(snapshot)) {
        return false;
    }
    rebuildLegacySnapshotFromNLE();
    Q_EMIT projectModified();
    return true;
}

void NLEStateCommand::undo()
{
    apply(beforeJson_);
}

void NLEStateCommand::redo()
{
    apply(afterJson_);
}

void NLEStateCommand::apply(const QJsonObject& snapshot)
{
    if (snapshot.isEmpty()) {
        return;
    }
    EditorEngine::instance()->restoreNLESnapshot(snapshot);
}

void EditorEngine::newProject()
{
    projectFilePath_.clear();
    if (!nleStore_) {
        nleStore_ = std::make_unique<ArtifactCore::NLE::NLEProjectStore>();
    }
    nleStore_->clear();

    DemoProject project;
    project.id = generateLegacyId();
    project.name = QStringLiteral("Untitled Project");
    project.version = QStringLiteral("1.0");
    project.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    project.modifiedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    DemoSequence seq;
    const auto coreSequenceId = nleStore_->createSequence(
        QStringLiteral("Sequence 1"), ArtifactCore::NLE::TimeBase{1, 30, false});
    seq.id = coreSequenceId.toString();
    seq.name = QStringLiteral("Sequence 1");
    seq.resolution = QStringLiteral("1920x1080");
    seq.frameRate = QStringLiteral("30 fps");
    seq.duration = 350;
    if (auto* coreSequence = nleStore_->sequence(coreSequenceId)) {
        coreSequence->duration = ArtifactCore::FrameRange::fromFrameCount(0, seq.duration);
    }

    DemoTrack v1;
    const auto coreVideoTrackId = nleStore_->createTrack(
        coreSequenceId, ArtifactCore::NLE::TrackKind::Video, QStringLiteral("V1"));
    v1.id = coreVideoTrackId.toString();
    v1.name = QStringLiteral("V1");
    v1.kind = QStringLiteral("video");
    v1.height = 28;
    seq.videoTracks.push_back(v1);

    DemoTrack a1;
    const auto coreAudioTrackId = nleStore_->createTrack(
        coreSequenceId, ArtifactCore::NLE::TrackKind::Audio, QStringLiteral("A1"));
    a1.id = coreAudioTrackId.toString();
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

    editSession_.undoStack()->clear();

    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectLoaded(true, QStringLiteral("New project created"));
}

void EditorEngine::newSequence()
{
    if (currentProject_.id.isEmpty()) {
        newProject();
    }
    if (!nleStore_) {
        nleStore_ = std::make_unique<ArtifactCore::NLE::NLEProjectStore>();
    }

    DemoSequence sequence;
    const auto coreSequenceId = nleStore_->createSequence(
        QStringLiteral("Sequence %1").arg(currentProject_.sequences.size() + 1),
        ArtifactCore::NLE::TimeBase{1, 30, false});
    sequence.id = coreSequenceId.toString();
    sequence.name = QStringLiteral("Sequence %1").arg(currentProject_.sequences.size() + 1);
    sequence.resolution = QStringLiteral("1920x1080");
    sequence.frameRate = QStringLiteral("30 fps");
    sequence.duration = 350;
    if (auto* coreSequence = nleStore_->sequence(coreSequenceId)) {
        coreSequence->duration = ArtifactCore::FrameRange::fromFrameCount(0, sequence.duration);
    }

    DemoTrack video;
    const auto coreVideoTrackId = nleStore_->createTrack(
        coreSequenceId, ArtifactCore::NLE::TrackKind::Video, QStringLiteral("V1"));
    video.id = coreVideoTrackId.toString();
    video.name = QStringLiteral("V1");
    video.kind = QStringLiteral("video");
    video.height = 28;
    sequence.videoTracks.push_back(video);

    DemoTrack audio;
    const auto coreAudioTrackId = nleStore_->createTrack(
        coreSequenceId, ArtifactCore::NLE::TrackKind::Audio, QStringLiteral("A1"));
    audio.id = coreAudioTrackId.toString();
    audio.name = QStringLiteral("A1");
    audio.kind = QStringLiteral("audio");
    audio.height = 28;
    sequence.audioTracks.push_back(audio);

    currentProject_.sequences.push_back(sequence);
    currentProject_.activeSequenceId = sequence.id;
    currentProject_.modifiedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    currentSequence_ = sequence;
    currentFrame_ = 0;
    inPoint_ = 0;
    outPoint_ = sequence.duration;
    playbackSpeed_ = PlaybackSpeed::Stop;
    selectedClipId_.clear();

    editSession_.undoStack()->clear();

    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectModified();
}

bool EditorEngine::selectSequence(const QString& sequenceId)
{
    for (const auto& sequence : currentProject_.sequences) {
        if (sequence.id != sequenceId) continue;

        currentProject_.activeSequenceId = sequence.id;
        currentSequence_ = sequence;
        currentFrame_ = 0;
        inPoint_ = 0;
        outPoint_ = qMax<FramePosition>(0, sequence.duration);
        playbackSpeed_ = PlaybackSpeed::Stop;
        selectedClipId_.clear();
        Q_EMIT sequenceChanged(currentSequence_);
        Q_EMIT projectModified();
        return true;
    }
    return false;
}

void EditorEngine::updateSequenceSettings(const QString& name,
                                          const QString& resolution,
                                          const QString& frameRate)
{
    if (currentSequence_.id.isEmpty()) return;

    const QString trimmedName = name.trimmed();
    const QString trimmedResolution = resolution.trimmed();
    const QString trimmedFrameRate = frameRate.trimmed();
    if (!trimmedName.isEmpty()) currentSequence_.name = trimmedName;
    if (!trimmedResolution.isEmpty()) currentSequence_.resolution = trimmedResolution;
    if (!trimmedFrameRate.isEmpty()) currentSequence_.frameRate = trimmedFrameRate;

    for (auto& sequence : currentProject_.sequences) {
        if (sequence.id == currentSequence_.id) {
            sequence = currentSequence_;
            break;
        }
    }
    if (nleStore_) {
        const auto coreSequenceId = ArtifactCore::NLE::SequenceId::fromString(currentSequence_.id);
        if (auto* coreSequence = nleStore_->sequence(coreSequenceId)) {
            coreSequence->name = currentSequence_.name;
            if (!trimmedFrameRate.isEmpty()) {
                QString rateText = trimmedFrameRate;
                rateText.remove(QStringLiteral("fps"), Qt::CaseInsensitive);
                bool ok = false;
                const double fps = rateText.trimmed().toDouble(&ok);
                if (ok && fps > 0.0) {
                    ArtifactCore::NLE::TimeBase timeBase;
                    if (rateText.trimmed().startsWith(QStringLiteral("23.976"))) {
                        timeBase = ArtifactCore::NLE::TimeBase{1001, 24000, false};
                    } else if (rateText.trimmed().startsWith(QStringLiteral("29.97"))) {
                        timeBase = ArtifactCore::NLE::TimeBase{1001, 30000, false};
                    } else if (rateText.trimmed().startsWith(QStringLiteral("59.94"))) {
                        timeBase = ArtifactCore::NLE::TimeBase{1001, 60000, false};
                    } else {
                        timeBase = ArtifactCore::NLE::TimeBase{1, static_cast<int32_t>(fps + 0.5), false};
                    }
                    if (timeBase.isValid()) coreSequence->timeBase = timeBase;
                }
            }
        }
    }
    currentProject_.modifiedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectModified();
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

    QHash<QString, ArtifactCore::NLE::SourceId> sourceIdsByPath;
    for (const auto& media : currentProject_.mediaPool) {
        ArtifactCore::NLE::SourceRef source;
        source.uri = media.filePath;
        source.displayName = media.name;
        source.mimeType = media.type;
        source.timeBase = ArtifactCore::NLE::TimeBase{1, 30, false};
        source.availableRange = ArtifactCore::FrameRange::fromDuration(0, 100000);
        sourceIdsByPath.insert(media.filePath, nleStore_->registerSource(source));
    }

    const auto importTrack = [this, &sourceIdsByPath](const DemoTrack& legacyTrack) {
        const auto trackId = ArtifactCore::NLE::TrackId::fromString(legacyTrack.id);
        for (const auto& legacyClip : legacyTrack.clips) {
            const auto sourceId = sourceIdsByPath.value(legacyClip.sourceFile);
            if (!sourceId.isValid()) {
                continue;
            }

            ArtifactCore::NLE::ClipDraft draft;
            draft.sourceId = sourceId;
            draft.sourceRange = ArtifactCore::FrameRange(legacyClip.sourceIn, legacyClip.sourceOut);
            draft.timelineRange = ArtifactCore::FrameRange::fromDuration(
                legacyClip.startFrame, legacyClip.duration);
            draft.trimRange = draft.sourceRange;
            draft.name = legacyClip.name;
            draft.speed = legacyClip.speed;
            draft.reversed = legacyClip.reversed;
            nleStore_->addClip(
                ArtifactCore::NLE::SequenceId::fromString(currentProject_.activeSequenceId),
                trackId,
                draft);
        }
    };

    for (const auto& track : currentSequence_.videoTracks) {
        importTrack(track);
    }
    for (const auto& track : currentSequence_.audioTracks) {
        importTrack(track);
    }
    rebuildLegacySnapshotFromNLE();

    currentProject_.modifiedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    Q_EMIT projectLoaded(true, QStringLiteral("Demo project loaded"));
}

void EditorEngine::setCurrentFrame(FramePosition frame)
{
    const FramePosition maxFrame = qMax<FramePosition>(0, currentSequence_.duration);
    currentFrame_ = qMax<FramePosition>(0, qMin(frame, maxFrame));
    Q_EMIT currentFrameChanged(currentFrame_);
}

void EditorEngine::selectClip(const QString& clipId)
{
    bool found = false;
    for (auto& track : currentSequence_.videoTracks) {
        for (auto& clip : track.clips) {
            clip.selected = (clip.id == clipId);
            found = found || clip.id == clipId;
        }
    }
    for (auto& track : currentSequence_.audioTracks) {
        for (auto& clip : track.clips) {
            clip.selected = (clip.id == clipId);
            found = found || clip.id == clipId;
        }
    }

    if (!found) {
        selectedClipId_.clear();
        Q_EMIT clipSelectionChanged(QString());
        return;
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

    if (nleStore_) {
        const auto clipId = ArtifactCore::NLE::ClipId::fromString(selectedClipId_);
        if (nleStore_->hasClip(clipId)) {
            const QJsonObject before = nleSnapshot();
            if (!nleStore_->removeClip(clipId)) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            selectedClipId_.clear();
            Q_EMIT clipSelectionChanged(QString());
            rebuildLegacySnapshotFromNLE();
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(selectedClipId_);
    if (!clip) return;

    QString trackId;
    int index = -1;
    bool removedFromVideo = false;
    const auto beforeVideoTracks = currentSequence_.videoTracks;
    const FramePosition beforeDuration = currentSequence_.duration;
    const QString beforeSelection = selectedClipId_;

    for (auto& track : currentSequence_.videoTracks) {
        for (int i = 0; i < track.clips.size(); ++i) {
            if (track.clips[i].id == selectedClipId_) {
                trackId = track.id;
                index = i;
                track.clips.removeAt(i);
                removedFromVideo = true;
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
                    // QUndoStack::push が redo() を即座に実行して clip を削除する
                    pushUndo(std::make_unique<DeleteClipCommand>(track.id, track.clips[i], i));
                    break;
                }
            }
            if (!trackId.isEmpty()) break;
        }
    }

    selectedClipId_.clear();
    Q_EMIT clipSelectionChanged(QString());
    if (removedFromVideo) {
        setCurrentSequence(currentSequence_);
        pushUndo(std::make_unique<VideoTracksStateCommand>(beforeVideoTracks,
                                             currentSequence_.videoTracks,
                                             beforeDuration,
                                             currentSequence_.duration,
                                             beforeSelection,
                                             QString()));
    }
    Q_EMIT projectModified();
    Q_EMIT sequenceChanged(currentSequence_);
}

void EditorEngine::rippleDeleteSelectedClip()
{
    if (selectedClipId_.isEmpty()) return;

    if (nleStore_) {
        const auto clipId = ArtifactCore::NLE::ClipId::fromString(selectedClipId_);
        if (nleStore_->hasClip(clipId)) {
            const QJsonObject before = nleSnapshot();
            if (!nleStore_->rippleDelete(clipId)) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            selectedClipId_.clear();
            Q_EMIT clipSelectionChanged(QString());
            rebuildLegacySnapshotFromNLE();
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(selectedClipId_);
    if (!clip) return;

    QString trackId;
    int index = -1;
    DemoTrack* track = nullptr;
    bool isVideoTrack = false;

    for (auto& t : currentSequence_.videoTracks) {
        for (int i = 0; i < t.clips.size(); ++i) {
            if (t.clips[i].id == selectedClipId_) {
                trackId = t.id;
                index = i;
                track = &t;
                isVideoTrack = true;
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

    FramePosition deletedDuration = clip->duration;

    if (isVideoTrack) {
        const FramePosition oldDuration = currentSequence_.duration;
        const FramePosition newDuration = oldDuration - deletedDuration;
        // QUndoStack::push が redo() を即座に実行する
        pushUndo(std::make_unique<RippleDeleteCommand>(track->id, *clip, index,
                                                       oldDuration, newDuration));
    } else {
        // QUndoStack::push が redo() を即座に実行して clip を削除する
        pushUndo(std::make_unique<DeleteClipCommand>(track->id, *clip, index));

        for (int i = index; i < track->clips.size(); ++i) {
            track->clips[i].startFrame -= deletedDuration;
        }
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

    // NLE ストア経由: trimClip(左半分) + addClip(右半分) を 1 コマンドに。
    // Core には split API が無いためこの組合せで実装する。
    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clip->id);
        if (auto* coreClip = nleStore_->clip(coreClipId)) {
            const FramePosition splitPoint = currentFrame_ - coreClip->timelineRange.start();
            const auto sequenceId = ArtifactCore::NLE::SequenceId::fromString(currentProject_.activeSequenceId);
            const auto trackId = coreClip->trackId;
            const auto sourceRange = coreClip->sourceRange;

            ArtifactCore::NLE::ClipDraft rightDraft;
            rightDraft.sourceId = coreClip->sourceId;
            rightDraft.sourceRange = ArtifactCore::FrameRange::fromFrameCount(
                sourceRange.start() + splitPoint,
                qMax<qint64>(1, sourceRange.duration() - splitPoint));
            rightDraft.timelineRange = ArtifactCore::FrameRange::fromFrameCount(
                currentFrame_,
                qMax<qint64>(1, coreClip->timelineRange.duration() - splitPoint));
            rightDraft.trimRange = rightDraft.sourceRange;
            rightDraft.name = coreClip->name;
            rightDraft.speed = coreClip->speed;
            rightDraft.opacity = coreClip->opacity;
            rightDraft.enabled = coreClip->enabled;
            rightDraft.reversed = coreClip->reversed;
            rightDraft.linkedGroupId = coreClip->linkedGroupId;

            const QJsonObject before = nleSnapshot();

            // 左側を縮める (Source モード: sourceRange/timelineRange を同時調整)
            const auto leftSourceRange = ArtifactCore::FrameRange::fromFrameCount(
                sourceRange.start(), qMin(splitPoint, sourceRange.duration()));
            if (!nleStore_->trimClip(coreClipId, leftSourceRange, ArtifactCore::NLE::TrimMode::Source)) {
                return;
            }

            const auto newRightId = nleStore_->addClip(sequenceId, trackId, rightDraft);
            if (!newRightId.isValid()) {
                // ロールバックして legacy フォールバックへ
                nleStore_->loadFromJson(before);
                return;
            }

            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT projectModified();
            Q_EMIT sequenceChanged(currentSequence_);
            return;
        }
    }

    const auto beforeVideoTracks = currentSequence_.videoTracks;

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

    if (track && selectedVideoClip) {
        setCurrentSequence(currentSequence_);
        pushUndo(std::make_unique<VideoTracksStateCommand>(beforeVideoTracks,
                                             currentSequence_.videoTracks,
                                             beforeDuration,
                                             currentSequence_.duration,
                                             beforeSelection,
                                             selectedClipId_));
    }

    Q_EMIT projectModified();
    Q_EMIT sequenceChanged(currentSequence_);
}

void EditorEngine::duplicateSelectedClip()
{
    if (selectedClipId_.isEmpty()) return;

    auto* clip = findClip(selectedClipId_);
    if (!clip) return;

    // NLE ストア経由: 元クリップの複製を後ろへ overwrite 配置。
    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clip->id);
        if (auto* coreClip = nleStore_->clip(coreClipId)) {
            const auto sequenceId = ArtifactCore::NLE::SequenceId::fromString(currentProject_.activeSequenceId);

            ArtifactCore::NLE::ClipDraft draft;
            draft.sourceId = coreClip->sourceId;
            draft.sourceRange = coreClip->sourceRange;
            const FramePosition newStart = coreClip->timelineRange.start()
                + coreClip->timelineRange.duration();
            draft.timelineRange = ArtifactCore::FrameRange::fromFrameCount(
                newStart, coreClip->timelineRange.duration());
            draft.trimRange = coreClip->trimRange;
            draft.name = coreClip->name;
            draft.speed = coreClip->speed;
            draft.opacity = coreClip->opacity;
            draft.enabled = coreClip->enabled;
            draft.reversed = coreClip->reversed;
            draft.linkedGroupId = coreClip->linkedGroupId;

            const QJsonObject before = nleSnapshot();
            const auto newClipId = nleStore_->overwriteClip(sequenceId, coreClip->trackId, draft);
            if (!newClipId.isValid()) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            selectClip(newClipId.toString());
            Q_EMIT projectModified();
            Q_EMIT sequenceChanged(currentSequence_);
            return;
        }
    }

    const auto beforeVideoTracks = currentSequence_.videoTracks;
    const FramePosition beforeDuration = currentSequence_.duration;
    const QString beforeSelection = selectedClipId_;
    bool selectedVideoClip = false;
    for (const auto& track : currentSequence_.videoTracks) {
        for (const auto& videoClip : track.clips) {
            if (videoClip.id == selectedClipId_) {
                selectedVideoClip = true;
                break;
            }
        }
        if (selectedVideoClip) break;
    }

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
        if (selectedVideoClip) {
            currentSequence_.duration = qMax<FramePosition>(
                currentSequence_.duration, newClip.startFrame + newClip.duration);
            setCurrentSequence(currentSequence_);
            pushUndo(std::make_unique<VideoTracksStateCommand>(beforeVideoTracks,
                                                 currentSequence_.videoTracks,
                                                 beforeDuration,
                                                 currentSequence_.duration,
                                                 beforeSelection,
                                                 selectedClipId_));
        }
    }

    Q_EMIT projectModified();
    Q_EMIT sequenceChanged(currentSequence_);
}

// =====================================================================
// 6 種類の NLE 編集操作
// ---------------------------------------------------------------------
// すべて UndoCommand 経由で undo / redo 対応。
// doXxx() helper の中で engine の clipChanged / projectModified を emit。
// =====================================================================

namespace {

DemoTrack* findTrackById(DemoSequence& seq, const QString& trackId) {
    for (auto& t : seq.videoTracks) {
        if (t.id == trackId) return &t;
    }
    for (auto& t : seq.audioTracks) {
        if (t.id == trackId) return &t;
    }
    return nullptr;
}

} // namespace

void EditorEngine::slipClip(const QString& clipId, FramePosition delta)
{
    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clipId);
        if (nleStore_->hasClip(coreClipId)) {
            const auto* coreClip = nleStore_->clip(coreClipId);
            if (!coreClip || !coreClip->sourceRange.isValid()) {
                return;
            }
            const QJsonObject before = nleSnapshot();
            if (!nleStore_->slipClip(coreClipId,
                                     ArtifactCore::FramePosition(coreClip->sourceRange.start() + delta))) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT clipChanged(clipId);
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(clipId);
    if (!clip) return;

    const FramePosition oldIn = clip->sourceIn;
    const FramePosition oldOut = clip->sourceOut;
    const FramePosition newIn = oldIn + delta;
    const FramePosition newOut = oldOut + delta;
    bool isVideoClip = false;
    for (const auto& track : currentSequence_.videoTracks) {
        for (const auto& videoClip : track.clips) {
            if (videoClip.id == clipId) {
                isVideoClip = true;
                break;
            }
        }
        if (isVideoClip) break;
    }

    // QUndoStack::push が redo() を即座に実行する
    pushUndo(std::make_unique<SlipClipCommand>(clipId, oldIn, oldOut, newIn, newOut));
    if (isVideoClip) {
        setCurrentSequence(currentSequence_);
    }
    Q_EMIT clipChanged(clipId);
    Q_EMIT projectModified();
}

void EditorEngine::slideClip(const QString& clipId, FramePosition delta)
{
    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clipId);
        if (nleStore_->hasClip(coreClipId)) {
            const auto* coreClip = nleStore_->clip(coreClipId);
            if (!coreClip || !coreClip->timelineRange.isValid()) {
                return;
            }
            const QJsonObject before = nleSnapshot();
            if (!nleStore_->slideClip(
                    coreClipId,
                    ArtifactCore::FramePosition(coreClip->timelineRange.start() + delta))) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT clipChanged(clipId);
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(clipId);
    if (!clip) return;

    DemoTrack* track = nullptr;
    int clipIndex = -1;
    for (auto& t : currentSequence_.videoTracks) {
        for (int i = 0; i < t.clips.size(); ++i) {
            if (t.clips[i].id == clipId) { track = &t; clipIndex = i; break; }
        }
        if (track) break;
    }
    if (!track) return;

    const QString leftId = clipIndex > 0 ? track->clips[clipIndex - 1].id : QString();
    const QString rightId = clipIndex < track->clips.size() - 1 ? track->clips[clipIndex + 1].id : QString();
    const FramePosition oldLeftEnd = !leftId.isEmpty()
        ? (track->clips[clipIndex - 1].startFrame + track->clips[clipIndex - 1].duration)
        : 0;
    const FramePosition oldRightStart = !rightId.isEmpty()
        ? track->clips[clipIndex + 1].startFrame
        : 0;

    // QUndoStack::push が redo() を即座に実行する (computeNewEdges は ctor 後に実施)
    auto cmd = std::make_unique<SlideClipCommand>(clipId,
                                                  clip->startFrame, clip->startFrame + delta,
                                                  leftId, oldLeftEnd,
                                                  rightId, oldRightStart);
    cmd->computeNewEdges();
    pushUndo(std::move(cmd));
    setCurrentSequence(currentSequence_);
    Q_EMIT clipChanged(clipId);
    Q_EMIT projectModified();
}

void EditorEngine::moveClip(const QString& clipId, FramePosition newStart)
{
    newStart = qMax<FramePosition>(0, newStart);

    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clipId);
        if (nleStore_->hasClip(coreClipId)) {
            const QJsonObject before = nleSnapshot();
            if (!nleStore_->moveClip(
                    coreClipId, ArtifactCore::FramePosition(newStart))) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT clipChanged(clipId);
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(clipId);
    if (!clip) {
        return;
    }
    bool isVideoClip = false;
    for (const auto& track : currentSequence_.videoTracks) {
        for (const auto& videoClip : track.clips) {
            if (videoClip.id == clipId) {
                isVideoClip = true;
                break;
            }
        }
        if (isVideoClip) break;
    }
    const FramePosition oldStart = clip->startFrame;
    clip->startFrame = newStart;
    pushUndo(std::make_unique<MoveClipCommand>(clipId, oldStart, newStart));
    if (isVideoClip) {
        setCurrentSequence(currentSequence_);
    }
    Q_EMIT clipChanged(clipId);
    Q_EMIT projectModified();
}

void EditorEngine::trimClip(const QString& clipId,
                            FramePosition newStart,
                            FramePosition newDuration,
                            FramePosition newSourceIn,
                            FramePosition newSourceOut)
{
    newStart = qMax<FramePosition>(0, newStart);
    if (newDuration <= 0 || newSourceOut <= newSourceIn) {
        return;
    }

    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clipId);
        if (nleStore_->hasClip(coreClipId)) {
            const QJsonObject before = nleSnapshot();
            const ArtifactCore::FrameRange sourceRange(newSourceIn, newSourceOut);
            if (!nleStore_->trimClip(coreClipId, sourceRange,
                                     ArtifactCore::NLE::TrimMode::Source)
                || !nleStore_->moveClip(
                    coreClipId, ArtifactCore::FramePosition(newStart))) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT clipChanged(clipId);
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(clipId);
    if (!clip) {
        return;
    }
    bool isVideoClip = false;
    for (const auto& track : currentSequence_.videoTracks) {
        for (const auto& videoClip : track.clips) {
            if (videoClip.id == clipId) {
                isVideoClip = true;
                break;
            }
        }
        if (isVideoClip) break;
    }
    const FramePosition oldStart = clip->startFrame;
    const FramePosition oldDuration = clip->duration;
    const FramePosition oldSourceIn = clip->sourceIn;
    const FramePosition oldSourceOut = clip->sourceOut;
    // QUndoStack::push が redo() を即座に実行する
    pushUndo(std::make_unique<TrimClipCommand>(clipId,
                                    oldStart, oldDuration,
                                    oldSourceIn, oldSourceOut,
                                    newStart, newDuration,
                                    newSourceIn, newSourceOut));
    if (isVideoClip) {
        setCurrentSequence(currentSequence_);
    }
    Q_EMIT clipChanged(clipId);
    Q_EMIT projectModified();
}

void EditorEngine::rippleDeleteClipAt(const QString& clipId)
{
    // NLE ストア経由 (rippleDeleteSelectedClip と同じ経路に統一)。
    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clipId);
        if (nleStore_->hasClip(coreClipId)) {
            const QJsonObject before = nleSnapshot();
            if (!nleStore_->rippleDelete(coreClipId)) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            if (selectedClipId_ == clipId) {
                selectedClipId_.clear();
                Q_EMIT clipSelectionChanged(QString());
            }
            Q_EMIT sequenceChanged(currentSequence_);
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(clipId);
    if (!clip) return;

    DemoTrack* track = nullptr;
    int index = -1;
    for (auto& t : currentSequence_.videoTracks) {
        for (int i = 0; i < t.clips.size(); ++i) {
            if (t.clips[i].id == clipId) { track = &t; index = i; break; }
        }
        if (track) break;
    }
    if (!track) return;

    const FramePosition oldDuration = currentSequence_.duration;
    const FramePosition newDuration = oldDuration - clip->duration;

    // QUndoStack::push が redo() を即座に実行する
    pushUndo(std::make_unique<RippleDeleteCommand>(track->id, *clip, index,
                                                   oldDuration, newDuration));
    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectModified();
}

void EditorEngine::insertClipFromSource(const QString& trackId,
                                        const DemoClip& sourceClip,
                                        FramePosition insertAt)
{
    auto* track = findTrackById(currentSequence_, trackId);
    if (!track) return;

    // NLE ストア経由: addClip が後続クリップを押し出す (挿入 semantics)。
    if (nleStore_) {
        const auto sequenceId = ArtifactCore::NLE::SequenceId::fromString(currentProject_.activeSequenceId);
        const auto coreTrackId = ArtifactCore::NLE::TrackId::fromString(trackId);
        if (nleStore_->hasTrack(coreTrackId)) {
            ArtifactCore::NLE::SourceRef sourceProbe;
            ArtifactCore::NLE::SourceId sourceId;
            if (resolveSourceForClip(sourceClip.sourceFile, sourceProbe)) {
                sourceId = nleStore_->registerSource(sourceProbe);
            }

            ArtifactCore::NLE::ClipDraft draft;
            draft.sourceId = sourceId;
            const FramePosition sourceLen = qMax<FramePosition>(
                1, sourceClip.sourceOut - sourceClip.sourceIn);
            draft.sourceRange = ArtifactCore::FrameRange::fromFrameCount(
                sourceClip.sourceIn, sourceLen);
            draft.timelineRange = ArtifactCore::FrameRange::fromFrameCount(
                insertAt, sourceClip.duration);
            draft.trimRange = draft.sourceRange;
            draft.name = sourceClip.name;
            draft.speed = sourceClip.speed;
            draft.opacity = sourceClip.opacity;
            draft.enabled = true;
            draft.reversed = sourceClip.reversed;

            const QJsonObject before = nleSnapshot();
            const auto newClipId = nleStore_->addClip(sequenceId, coreTrackId, draft);
            if (!newClipId.isValid()) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT sequenceChanged(currentSequence_);
            Q_EMIT projectModified();
            return;
        }
    }

    const FramePosition oldDuration = currentSequence_.duration;
    const FramePosition newDuration = oldDuration + sourceClip.duration;

    // QUndoStack::push が redo() を即座に実行する
    pushUndo(std::make_unique<InsertEditCommand>(trackId, sourceClip, insertAt,
                                                 oldDuration, newDuration));
    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectModified();
}

void EditorEngine::overwriteClipFromSource(const QString& trackId,
                                           const DemoClip& sourceClip,
                                           FramePosition overwriteAt)
{
    auto* track = findTrackById(currentSequence_, trackId);
    if (!track) return;

    // NLE ストア経由: overwriteClip が重なりを削除してから配置する。
    if (nleStore_) {
        const auto sequenceId = ArtifactCore::NLE::SequenceId::fromString(currentProject_.activeSequenceId);
        const auto coreTrackId = ArtifactCore::NLE::TrackId::fromString(trackId);
        if (nleStore_->hasTrack(coreTrackId)) {
            ArtifactCore::NLE::SourceRef sourceProbe;
            ArtifactCore::NLE::SourceId sourceId;
            if (resolveSourceForClip(sourceClip.sourceFile, sourceProbe)) {
                sourceId = nleStore_->registerSource(sourceProbe);
            }

            ArtifactCore::NLE::ClipDraft draft;
            draft.sourceId = sourceId;
            const FramePosition sourceLen = qMax<FramePosition>(
                1, sourceClip.sourceOut - sourceClip.sourceIn);
            draft.sourceRange = ArtifactCore::FrameRange::fromFrameCount(
                sourceClip.sourceIn, sourceLen);
            draft.timelineRange = ArtifactCore::FrameRange::fromFrameCount(
                overwriteAt, sourceClip.duration);
            draft.trimRange = draft.sourceRange;
            draft.name = sourceClip.name;
            draft.speed = sourceClip.speed;
            draft.opacity = sourceClip.opacity;
            draft.enabled = true;
            draft.reversed = sourceClip.reversed;

            const QJsonObject before = nleSnapshot();
            const auto newClipId = nleStore_->overwriteClip(sequenceId, coreTrackId, draft);
            if (!newClipId.isValid()) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT sequenceChanged(currentSequence_);
            Q_EMIT projectModified();
            return;
        }
    }

    // QUndoStack::push が redo() を即座に実行する
    pushUndo(std::make_unique<OverwriteEditCommand>(trackId, sourceClip, overwriteAt));
    if (track->kind == QStringLiteral("video")) {
        setCurrentSequence(currentSequence_);
    }
    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectModified();
}

void EditorEngine::liftRange(const QString& trackId,
                             FramePosition from, FramePosition to)
{
    auto* track = findTrackById(currentSequence_, trackId);
    if (!track) return;

    // NLE ストア経由: 範囲に重なるクリップを removeClip。
    if (nleStore_) {
        const auto coreTrackId = ArtifactCore::NLE::TrackId::fromString(trackId);
        if (nleStore_->hasTrack(coreTrackId)) {
            QVector<ArtifactCore::NLE::ClipId> overlapping;
            for (const auto& cid : nleStore_->clipIds(coreTrackId)) {
                const auto* c = nleStore_->clip(cid);
                if (!c) continue;
                const auto range = c->timelineRange;
                if (range.end() > from && range.start() < to) {
                    overlapping.append(cid);
                }
            }
            if (overlapping.isEmpty()) return;

            const QJsonObject before = nleSnapshot();
            for (const auto& cid : overlapping) {
                nleStore_->removeClip(cid);
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT sequenceChanged(currentSequence_);
            Q_EMIT projectModified();
            return;
        }
    }

    // QUndoStack::push が redo() を即座に実行する
    pushUndo(std::make_unique<LiftEditCommand>(trackId, from, to, track->clips));
    if (track->kind == QStringLiteral("video")) {
        setCurrentSequence(currentSequence_);
    }
    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectModified();
}

void EditorEngine::setTrackMuted(const QString& trackId, bool muted)
{
    // NLE ストア経由
    if (nleStore_) {
        const auto coreTrackId = ArtifactCore::NLE::TrackId::fromString(trackId);
        if (auto* coreTrack = nleStore_->track(coreTrackId)) {
            const QJsonObject before = nleSnapshot();
            coreTrack->mute = muted;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT projectModified();
            return;
        }
    }

    for (auto& track : currentSequence_.videoTracks) {
        if (track.id == trackId) { track.muted = muted; break; }
    }
    for (auto& track : currentSequence_.audioTracks) {
        if (track.id == trackId) { track.muted = muted; break; }
    }
    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectModified();
}

void EditorEngine::setTrackSolo(const QString& trackId, bool solo)
{
    // NLE ストア経由
    if (nleStore_) {
        const auto coreTrackId = ArtifactCore::NLE::TrackId::fromString(trackId);
        if (auto* coreTrack = nleStore_->track(coreTrackId)) {
            const QJsonObject before = nleSnapshot();
            coreTrack->solo = solo;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT projectModified();
            return;
        }
    }

    for (auto& track : currentSequence_.videoTracks) {
        if (track.id == trackId) { track.solo = solo; break; }
    }
    for (auto& track : currentSequence_.audioTracks) {
        if (track.id == trackId) { track.solo = solo; break; }
    }
    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectModified();
}

void EditorEngine::setClipSpeed(const QString& clipId, double speed)
{
    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clipId);
        if (auto* coreClip = nleStore_->clip(coreClipId)) {
            const QJsonObject before = nleSnapshot();
            coreClip->speed = speed;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT clipChanged(clipId);
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(clipId);
    if (!clip) return;

    speed = qMax(0.01, speed);
    pushUndo(std::make_unique<ClipPropertyCommand>(clipId, ClipPropertyCommand::Kind::Speed,
                                                   clip->speed, speed));
}

void EditorEngine::setClipReversed(const QString& clipId, bool reversed)
{
    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clipId);
        if (auto* coreClip = nleStore_->clip(coreClipId)) {
            const QJsonObject before = nleSnapshot();
            coreClip->reversed = reversed;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT clipChanged(clipId);
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(clipId);
    if (!clip) return;

    pushUndo(std::make_unique<ClipPropertyCommand>(clipId, ClipPropertyCommand::Kind::Reverse,
                                                   clip->reversed, reversed));
}

void EditorEngine::setClipVolume(const QString& clipId, double volume)
{
    auto* clip = findClip(clipId);
    if (!clip) return;

    volume = qMax(0.0, qMin(2.0, volume));
    pushUndo(std::make_unique<ClipPropertyCommand>(clipId, ClipPropertyCommand::Kind::Volume,
                                                   clip->volume, volume));
}

void EditorEngine::setClipOpacity(const QString& clipId, double opacity)
{
    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clipId);
        if (auto* coreClip = nleStore_->clip(coreClipId)) {
            const QJsonObject before = nleSnapshot();
            coreClip->opacity = qBound(0.0, opacity, 1.0);
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT clipChanged(clipId);
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(clipId);
    if (!clip) return;

    opacity = qBound(0.0, opacity, 1.0);
    pushUndo(std::make_unique<ClipPropertyCommand>(clipId, ClipPropertyCommand::Kind::Opacity,
                                                   clip->opacity, opacity));
}

void EditorEngine::setClipName(const QString& clipId, const QString& name)
{
    if (nleStore_) {
        const auto coreClipId = ArtifactCore::NLE::ClipId::fromString(clipId);
        if (auto* coreClip = nleStore_->clip(coreClipId)) {
            const QJsonObject before = nleSnapshot();
            coreClip->name = name;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT clipChanged(clipId);
            Q_EMIT projectModified();
            return;
        }
    }

    auto* clip = findClip(clipId);
    if (!clip) return;

    pushUndo(std::make_unique<ClipPropertyCommand>(clipId, ClipPropertyCommand::Kind::Name,
                                                   clip->name, name));
}

void EditorEngine::cutClip(const QString& clipId)
{
    auto* clip = findClip(clipId);
    if (!clip) return;

    clipboard_ = *clip;
    bool isVideoClip = false;
    for (const auto& track : currentSequence_.videoTracks) {
        for (const auto& videoClip : track.clips) {
            if (videoClip.id == clipId) {
                isVideoClip = true;
                break;
            }
        }
        if (isVideoClip) break;
    }
    if (isVideoClip && selectedClipId_ != clipId) {
        selectClip(clipId);
    }
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

    if (targetFrame < 0) targetFrame = currentFrame_;

    // NLE ストア経由: クリップボードを overwrite 配置。
    if (nleStore_ && !clipboard_.sourceFile.isEmpty()) {
        // クリップボードの所属種別 (video/audio) に合う最初のトラックを探す
        QString targetTrackId;
        for (const auto& trackId : nleStore_->trackIds(
                 ArtifactCore::NLE::SequenceId::fromString(currentProject_.activeSequenceId))) {
            const auto* coreTrack = nleStore_->track(trackId);
            if (!coreTrack) continue;
            const bool isAudioTrack = coreTrack->kind == ArtifactCore::NLE::TrackKind::Audio;
            const bool clipboardIsAudio = clipboard_.color == QStringLiteral("#4eff4a");
            if (isAudioTrack == clipboardIsAudio) {
                targetTrackId = trackId.toString();
                break;
            }
        }

        const auto coreTrackId = ArtifactCore::NLE::TrackId::fromString(targetTrackId);
        if (!targetTrackId.isEmpty() && nleStore_->hasTrack(coreTrackId)) {
            ArtifactCore::NLE::SourceRef sourceProbe;
            ArtifactCore::NLE::SourceId sourceId;
            if (resolveSourceForClip(clipboard_.sourceFile, sourceProbe)) {
                sourceId = nleStore_->registerSource(sourceProbe);
            }

            ArtifactCore::NLE::ClipDraft draft;
            draft.sourceId = sourceId;
            const FramePosition sourceLen = qMax<FramePosition>(
                1, clipboard_.sourceOut - clipboard_.sourceIn);
            draft.sourceRange = ArtifactCore::FrameRange::fromFrameCount(
                clipboard_.sourceIn, sourceLen);
            draft.timelineRange = ArtifactCore::FrameRange::fromFrameCount(
                targetFrame, qMax<FramePosition>(1, clipboard_.duration));
            draft.trimRange = draft.sourceRange;
            draft.name = clipboard_.name;
            draft.speed = clipboard_.speed;
            draft.opacity = clipboard_.opacity;
            draft.enabled = clipboard_.enabled;
            draft.reversed = clipboard_.reversed;

            const auto sequenceId = ArtifactCore::NLE::SequenceId::fromString(currentProject_.activeSequenceId);
            const QJsonObject before = nleSnapshot();
            const auto newClipId = nleStore_->overwriteClip(sequenceId, coreTrackId, draft);
            if (!newClipId.isValid()) {
                return;
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            selectClip(newClipId.toString());
            Q_EMIT projectModified();
            Q_EMIT sequenceChanged(currentSequence_);
            return;
        }
    }

    const auto beforeVideoTracks = currentSequence_.videoTracks;
    const FramePosition beforeDuration = currentSequence_.duration;
    const QString beforeSelection = selectedClipId_;

    DemoClip newClip = clipboard_;
    newClip.id = generateId(QStringLiteral("clip"));
    newClip.startFrame = targetFrame;
    newClip.selected = false;

    DemoTrack* targetTrack = nullptr;
    bool targetIsVideo = false;
    for (auto& track : currentSequence_.videoTracks) {
        targetTrack = &track;
        targetIsVideo = true;
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
        if (targetIsVideo) {
            currentSequence_.duration = qMax<FramePosition>(
                currentSequence_.duration, newClip.startFrame + newClip.duration);
            setCurrentSequence(currentSequence_);
            pushUndo(std::make_unique<VideoTracksStateCommand>(beforeVideoTracks,
                                                 currentSequence_.videoTracks,
                                                 beforeDuration,
                                                 currentSequence_.duration,
                                                 beforeSelection,
                                                 selectedClipId_));
        }
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

// =====================================================================
// snapToNearestEx: 拡張 snap。
// 既存の snapToNearest + transition + frame + second を統合。
// frame / second は round して返す (clamp ではなく round なので
// ドラッグ中に即時 snap される)。
// =====================================================================
FramePosition EditorEngine::snapToNearestEx(FramePosition frame, bool forLeftEdge, int threshold)
{
    if (!snapEnabled_) return frame;

    // 既存ロジック (clip / marker / playhead) を再利用
    const FramePosition baseSnap = snapToNearest(frame, forLeftEdge);
    if (baseSnap != frame) return baseSnap;

    // transition 端
    for (const auto& trans : currentSequence_.transitions) {
        if (qAbs(frame - trans.startFrame) <= threshold) {
            return trans.startFrame;
        }
        if (qAbs(frame - (trans.startFrame + trans.duration)) <= threshold) {
            return trans.startFrame + trans.duration;
        }
    }

    // second 単位 (sequence の frame rate に合わせる)
    bool fpsOk = false;
    const double parsedFps = currentSequence_.frameRate.section(QChar(' '), 0, 0).toDouble(&fpsOk);
    const int kFps = !fpsOk || parsedFps <= 0.0
        ? 30
        : qMax(1, static_cast<int>(parsedFps + 0.5));
    const FramePosition secondFrame = (frame / kFps) * kFps;
    if (qAbs(frame - secondFrame) <= threshold) {
        return secondFrame;
    }
    const FramePosition nextSecond = secondFrame + kFps;
    if (qAbs(frame - nextSecond) <= threshold) {
        return nextSecond;
    }

    // 1 frame 単位は threshold == 0 相当。返さない。
    return frame;
}

void EditorEngine::pushUndo(std::unique_ptr<ArtifactCore::SerializableCommand> cmd)
{
    if (!cmd) {
        return;
    }
    // EditSession.pushCommand が履歴ログ記録 + QUndoStack.push (redo 即時実行) を行う。
    editSession_.pushCommand(std::move(cmd));
}

void EditorEngine::undo()
{
    if (!editSession_.undoStack()->canUndo()) {
        return;
    }
    editSession_.undoStack()->undo();
    Q_EMIT projectModified();
    Q_EMIT sequenceChanged(currentSequence_);
}

void EditorEngine::redo()
{
    if (!editSession_.undoStack()->canRedo()) {
        return;
    }
    editSession_.undoStack()->redo();
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
        bool isVideoClip = false;
        for (const auto& track : engine->currentSequence().videoTracks) {
            for (const auto& videoClip : track.clips) {
                if (videoClip.id == clipId_) {
                    isVideoClip = true;
                    break;
                }
            }
            if (isVideoClip) break;
        }
        if (isVideoClip) {
            engine->setCurrentSequence(engine->currentSequence());
        }
    }
}

void MoveClipCommand::undo()
{
    doMove(oldStart_);
}

QJsonObject MoveClipCommand::serialize() const
{
    return QJsonObject{
        {QStringLiteral("clipId"), clipId_},
        {QStringLiteral("oldStart"), static_cast<qint64>(oldStart_)},
        {QStringLiteral("newStart"), static_cast<qint64>(newStart_)},
    };
}

bool MoveClipCommand::deserialize(const QJsonObject& data)
{
    clipId_ = data[QStringLiteral("clipId")].toString();
    oldStart_ = data[QStringLiteral("oldStart")].toInteger<qint64>();
    newStart_ = data[QStringLiteral("newStart")].toInteger<qint64>();
    setText(QStringLiteral("Move Clip"));
    return !clipId_.isEmpty();
}

void TrimClipCommand::doTrim(FramePosition start, FramePosition duration,
                             FramePosition sourceIn, FramePosition sourceOut)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    auto* clip = engine->findClip(clipId_);
    if (clip) {
        clip->startFrame = start;
        clip->duration = duration;
        clip->sourceIn = sourceIn;
        clip->sourceOut = sourceOut;
        bool isVideoClip = false;
        for (const auto& track : engine->currentSequence().videoTracks) {
            for (const auto& videoClip : track.clips) {
                if (videoClip.id == clipId_) {
                    isVideoClip = true;
                    break;
                }
            }
            if (isVideoClip) break;
        }
        if (isVideoClip) {
            engine->setCurrentSequence(engine->currentSequence());
        }
    }
}

void TrimClipCommand::undo()
{
    doTrim(oldStart_, oldDuration_, oldSourceIn_, oldSourceOut_);
}

QJsonObject TrimClipCommand::serialize() const
{
    return QJsonObject{
        {QStringLiteral("clipId"), clipId_},
        {QStringLiteral("oldStart"), static_cast<qint64>(oldStart_)},
        {QStringLiteral("oldDuration"), static_cast<qint64>(oldDuration_)},
        {QStringLiteral("oldSourceIn"), static_cast<qint64>(oldSourceIn_)},
        {QStringLiteral("oldSourceOut"), static_cast<qint64>(oldSourceOut_)},
        {QStringLiteral("newStart"), static_cast<qint64>(newStart_)},
        {QStringLiteral("newDuration"), static_cast<qint64>(newDuration_)},
        {QStringLiteral("newSourceIn"), static_cast<qint64>(newSourceIn_)},
        {QStringLiteral("newSourceOut"), static_cast<qint64>(newSourceOut_)},
    };
}

bool TrimClipCommand::deserialize(const QJsonObject& data)
{
    clipId_ = data[QStringLiteral("clipId")].toString();
    oldStart_ = data[QStringLiteral("oldStart")].toInteger<qint64>();
    oldDuration_ = data[QStringLiteral("oldDuration")].toInteger<qint64>();
    oldSourceIn_ = data[QStringLiteral("oldSourceIn")].toInteger<qint64>();
    oldSourceOut_ = data[QStringLiteral("oldSourceOut")].toInteger<qint64>();
    newStart_ = data[QStringLiteral("newStart")].toInteger<qint64>();
    newDuration_ = data[QStringLiteral("newDuration")].toInteger<qint64>();
    newSourceIn_ = data[QStringLiteral("newSourceIn")].toInteger<qint64>();
    newSourceOut_ = data[QStringLiteral("newSourceOut")].toInteger<qint64>();
    setText(QStringLiteral("Trim Clip"));
    return !clipId_.isEmpty();
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

QJsonObject DeleteClipCommand::serialize() const
{
    return QJsonObject{
        {QStringLiteral("trackId"), trackId_},
        {QStringLiteral("clip"), clipToJson(clip_)},
        {QStringLiteral("index"), index_},
    };
}

bool DeleteClipCommand::deserialize(const QJsonObject& data)
{
    trackId_ = data[QStringLiteral("trackId")].toString();
    clip_ = jsonToClip(data[QStringLiteral("clip")].toObject());
    index_ = data[QStringLiteral("index")].toInt(-1);
    setText(QStringLiteral("Delete Clip"));
    return !trackId_.isEmpty();
}

bool EditorEngine::runAutoSave()
{
    if (!autoSaveEnabled_) return false;
    if (autoSaveFilePath_.isEmpty()) return false;

    const bool result = saveProject(autoSaveFilePath_);
    return result;
}

void EditorEngine::addMarker(FramePosition position, const QString& name, const QString& comment)
{
    position = qMax<FramePosition>(0, qMin(position, currentSequence_.duration));

    // NLE ストア経由 (ID は MarkerId::toString() を legacy にも使う)。
    if (nleStore_) {
        const auto sequenceId = ArtifactCore::NLE::SequenceId::fromString(currentProject_.activeSequenceId);
        if (nleStore_->hasSequence(sequenceId)) {
            const QString markerName = name.isEmpty()
                ? QStringLiteral("Marker %1").arg(currentSequence_.markers.size() + 1)
                : name;
            const QJsonObject before = nleSnapshot();
            const auto coreMarkerId = nleStore_->createMarker(
                sequenceId, ArtifactCore::FramePosition(position),
                markerName, comment, QColor(Qt::yellow));
            if (!coreMarkerId.isValid()) return;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }

    const auto before = currentSequence_.markers;
    Marker marker;
    marker.id = generateMarkerId();
    marker.position = position;
    marker.name = name.isEmpty() ? QStringLiteral("Marker %1").arg(currentSequence_.markers.size() + 1) : name;
    marker.comment = comment;
    marker.color = QColor(Qt::yellow);
    marker.type = Marker::Type::Comment;

    currentSequence_.markers.append(marker);

    pushUndo(std::make_unique<MarkerStateCommand>(before, currentSequence_.markers));
    Q_EMIT markerChanged();
    Q_EMIT projectModified();
}

void EditorEngine::deleteMarker(const QString& markerId)
{
    // NLE ストア経由
    if (nleStore_) {
        const auto coreMarkerId = ArtifactCore::NLE::MarkerId::fromString(markerId);
        if (nleStore_->marker(coreMarkerId)) {
            const QJsonObject before = nleSnapshot();
            if (!nleStore_->removeMarker(coreMarkerId)) return;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }

    for (int i = 0; i < currentSequence_.markers.size(); ++i) {
        if (currentSequence_.markers[i].id == markerId) {
            const auto before = currentSequence_.markers;
            currentSequence_.markers.removeAt(i);
            pushUndo(std::make_unique<MarkerStateCommand>(before, currentSequence_.markers));
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }
}

void EditorEngine::moveMarker(const QString& markerId, FramePosition newPosition)
{
    // NLE ストア経由
    if (nleStore_) {
        const auto coreMarkerId = ArtifactCore::NLE::MarkerId::fromString(markerId);
        if (auto* coreMarker = nleStore_->marker(coreMarkerId)) {
            const QJsonObject before = nleSnapshot();
            coreMarker->position = ArtifactCore::FramePosition(qMax<FramePosition>(0, newPosition));
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }

    const auto before = currentSequence_.markers;
    for (auto& marker : currentSequence_.markers) {
        if (marker.id == markerId) {
            marker.position = qMax<FramePosition>(0,
                qMin(newPosition, currentSequence_.duration));
            pushUndo(std::make_unique<MarkerStateCommand>(before, currentSequence_.markers));
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }
}

void EditorEngine::setMarkerName(const QString& markerId, const QString& name)
{
    // NLE ストア経由
    if (nleStore_) {
        const auto coreMarkerId = ArtifactCore::NLE::MarkerId::fromString(markerId);
        if (auto* coreMarker = nleStore_->marker(coreMarkerId)) {
            const QJsonObject before = nleSnapshot();
            coreMarker->name = name;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }

    const auto before = currentSequence_.markers;
    for (auto& marker : currentSequence_.markers) {
        if (marker.id == markerId) {
            marker.name = name;
            pushUndo(std::make_unique<MarkerStateCommand>(before, currentSequence_.markers));
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }
}

void EditorEngine::setMarkerComment(const QString& markerId, const QString& comment)
{
    // NLE ストア経由
    if (nleStore_) {
        const auto coreMarkerId = ArtifactCore::NLE::MarkerId::fromString(markerId);
        if (auto* coreMarker = nleStore_->marker(coreMarkerId)) {
            const QJsonObject before = nleSnapshot();
            coreMarker->note = comment;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }

    const auto before = currentSequence_.markers;
    for (auto& marker : currentSequence_.markers) {
        if (marker.id == markerId) {
            marker.comment = comment;
            pushUndo(std::make_unique<MarkerStateCommand>(before, currentSequence_.markers));
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }
}

void EditorEngine::clearMarkers()
{
    if (currentSequence_.markers.isEmpty()) return;

    // NLE ストア経由
    if (nleStore_) {
        const auto sequenceId = ArtifactCore::NLE::SequenceId::fromString(currentProject_.activeSequenceId);
        QVector<ArtifactCore::NLE::MarkerId> toRemove;
        for (const auto& mid : nleStore_->sequence(sequenceId) ? nleStore_->sequence(sequenceId)->markers
                                                               : QVector<ArtifactCore::NLE::MarkerId>{}) {
            toRemove.append(mid);
        }
        if (!toRemove.isEmpty()) {
            const QJsonObject before = nleSnapshot();
            for (const auto& mid : toRemove) {
                nleStore_->removeMarker(mid);
            }
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT markerChanged();
            Q_EMIT projectModified();
            return;
        }
    }

    const auto before = currentSequence_.markers;
    currentSequence_.markers.clear();
    pushUndo(std::make_unique<MarkerStateCommand>(before, currentSequence_.markers));
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

namespace {

// legacy TransitionType(4種) → Core TransitionKind の対応。
// Core 側に DipToBlack/WipeLeft/WipeRight の列挙は無いため
// DipToBlack→Dissolve、Wipe→Wipe (+Direction) に写像する。
ArtifactCore::NLE::TransitionKind transitionKindForType(
    TransitionType type, ArtifactCore::NLE::Transition::Direction& direction)
{
    direction = ArtifactCore::NLE::Transition::Direction::LeftToRight;
    switch (type) {
    case TransitionType::Crossfade: return ArtifactCore::NLE::TransitionKind::Crossfade;
    case TransitionType::DipToBlack: return ArtifactCore::NLE::TransitionKind::Dissolve;
    case TransitionType::WipeLeft:
        direction = ArtifactCore::NLE::Transition::Direction::RightToLeft;
        return ArtifactCore::NLE::TransitionKind::Wipe;
    case TransitionType::WipeRight: return ArtifactCore::NLE::TransitionKind::Wipe;
    }
    return ArtifactCore::NLE::TransitionKind::Crossfade;
}

} // namespace

void EditorEngine::addTransition(const QString& trackId, const QString& leftClipId, const QString& rightClipId,
                                 FramePosition startFrame, TransitionType type, FramePosition duration)
{
    if (duration <= 0 || startFrame < 0) return;

    // NLE ストア経由 (ID は TransitionId::toString() を legacy にも使う)。
    if (nleStore_) {
        const auto coreTrackId = ArtifactCore::NLE::TrackId::fromString(trackId);
        const auto leftId = ArtifactCore::NLE::ClipId::fromString(leftClipId);
        const auto rightId = ArtifactCore::NLE::ClipId::fromString(rightClipId);
        if (nleStore_->hasTrack(coreTrackId)
            && nleStore_->hasClip(leftId) && nleStore_->hasClip(rightId)) {
            const auto range = ArtifactCore::FrameRange::fromDuration(
                startFrame, duration);
            ArtifactCore::NLE::Transition::Direction direction;
            const auto kind = transitionKindForType(type, direction);
            const QJsonObject before = nleSnapshot();
            const auto coreTransId = nleStore_->createTransition(
                coreTrackId, leftId, rightId, range,
                kind, static_cast<double>(duration), direction);
            if (!coreTransId.isValid()) return;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT transitionChanged();
            Q_EMIT projectModified();
            return;
        }
    }

    bool isVideoTrack = false;
    for (const auto& track : currentSequence_.videoTracks) {
        if (track.id == trackId) {
            isVideoTrack = true;
            break;
        }
    }

    Transition trans;
    trans.id = generateTransitionId();
    trans.trackId = trackId;
    trans.leftClipId = leftClipId;
    trans.rightClipId = rightClipId;
    trans.startFrame = startFrame;
    trans.duration = duration;
    trans.type = type;

    const auto before = currentSequence_.transitions;
    currentSequence_.transitions.append(trans);
    if (isVideoTrack) {
        setCurrentSequence(currentSequence_);
        pushUndo(std::make_unique<TransitionStateCommand>(before, currentSequence_.transitions));
    }
    Q_EMIT transitionChanged();
    Q_EMIT projectModified();
}

void EditorEngine::deleteTransition(const QString& transitionId)
{
    // NLE ストア経由
    if (nleStore_) {
        const auto coreTransId = ArtifactCore::NLE::TransitionId::fromString(transitionId);
        if (nleStore_->transition(coreTransId)) {
            const QJsonObject before = nleSnapshot();
            if (!nleStore_->removeTransition(coreTransId)) return;
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT transitionChanged();
            Q_EMIT projectModified();
            return;
        }
    }

    for (int i = 0; i < currentSequence_.transitions.size(); ++i) {
        if (currentSequence_.transitions[i].id == transitionId) {
            const auto before = currentSequence_.transitions;
            const bool isVideoTrack = [&]() {
                for (const auto& track : currentSequence_.videoTracks) {
                    if (track.id == currentSequence_.transitions[i].trackId) return true;
                }
                return false;
            }();
            currentSequence_.transitions.removeAt(i);
            if (isVideoTrack) {
                setCurrentSequence(currentSequence_);
                pushUndo(std::make_unique<TransitionStateCommand>(before, currentSequence_.transitions));
            }
            Q_EMIT transitionChanged();
            Q_EMIT projectModified();
            return;
        }
    }
}

bool EditorEngine::setVideoTransitionDuration(const QString& transitionId, FramePosition duration)
{
    if (duration <= 0) return false;

    // NLE ストア経由
    if (nleStore_) {
        const auto coreTransId = ArtifactCore::NLE::TransitionId::fromString(transitionId);
        if (auto* coreTransition = nleStore_->transition(coreTransId)) {
            const QJsonObject before = nleSnapshot();
            coreTransition->duration = static_cast<double>(duration);
            const auto range = coreTransition->range;
            coreTransition->range = ArtifactCore::FrameRange::fromDuration(
                range.start(), duration);
            const QJsonObject after = nleSnapshot();
            pushUndo(std::make_unique<NLEStateCommand>(before, after));
            rebuildLegacySnapshotFromNLE();
            Q_EMIT transitionChanged();
            Q_EMIT projectModified();
            return true;
        }
    }

    bool isVideoTransition = false;
    for (const auto& transition : currentSequence_.transitions) {
        if (transition.id != transitionId) continue;
        for (const auto& track : currentSequence_.videoTracks) {
            if (track.id == transition.trackId) {
                isVideoTransition = true;
                break;
            }
        }
        break;
    }
    if (!isVideoTransition) return false;

    const auto before = currentSequence_.transitions;
    for (auto& transition : currentSequence_.transitions) {
        if (transition.id == transitionId) {
            transition.duration = duration;
            break;
        }
    }
    setCurrentSequence(currentSequence_);
    pushUndo(std::make_unique<TransitionStateCommand>(before, currentSequence_.transitions));
    return true;
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

    // プローブして NLE ストアへ SourceRef 登録 (frameSize/timeBase/availableRange 取得)。
    // 失敗してもインポートは止めない (resolveSourceForClip が既定値を用意する)。
    ArtifactCore::NLE::SourceRef probe;
    if (resolveSourceForClip(filePath, probe)) {
        if (!probe.frameSize.isEmpty()) {
            media.resolution = QStringLiteral("%1x%2")
                .arg(probe.frameSize.width())
                .arg(probe.frameSize.height());
        }
        nleStore_->registerSource(probe);
    }

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
    obj[QStringLiteral("selected")] = clip.selected;
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
    clip.selected = obj[QStringLiteral("selected")].toBool();
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
    // Pre-save validation: check project health
    if (!validateProjectHealth(currentProject_)) {
        QMessageBox::warning(nullptr, "Project Validation Failed",
            "Project has critical errors that must be fixed before saving.\n"
            "Please check the Problem View for details.");
        Q_EMIT projectSaved(false, QStringLiteral("Validation failed: project has critical errors"));
        return false;
    }

    // QSaveFile が既存ファイルを先に削除せず、commit 時に置換する。
    // crash や disk full で中途半端な状態にならない。
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT projectSaved(false, QStringLiteral("Failed to open project file for writing: %1").arg(filePath));
        return false;
    }

    const QString previousModifiedAt = currentProject_.modifiedAt;
    currentProject_.modifiedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject json = projectToJson(currentProject_);
    // NLE ストアの完全スナップショットを併せて永続化
    // (sources/markers/transitions/linkGroups を含む。nleSnapshot() = toJson())
    json.insert(QStringLiteral("nle"), nleSnapshot());
    QJsonDocument doc(json);
    const QByteArray payload = doc.toJson(QJsonDocument::Indented);

    if (file.write(payload) != payload.size() || !file.commit()) {
        currentProject_.modifiedAt = previousModifiedAt;
        Q_EMIT projectSaved(false, QStringLiteral("Failed to commit project file: %1").arg(filePath));
        return false;
    }

    projectFilePath_ = QFileInfo(filePath).absoluteFilePath();
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
    if (!doc.isObject()) {
        Q_EMIT projectLoaded(false, QStringLiteral("Project JSON root must be an object"));
        return false;
    }

    projectFilePath_ = QFileInfo(filePath).absoluteFilePath();

    currentProject_ = jsonToProject(doc.object());

    // Post-load validation: check project health
    if (!validateProjectHealth(currentProject_)) {
        qWarning() << "[loadProject] Project loaded but has critical health issues";
        // Don't block load, but warn user
    }

    currentSequence_ = DemoSequence{};
    bool activeSequenceFound = false;
    for (const auto& seq : currentProject_.sequences) {
        if (seq.id == currentProject_.activeSequenceId) {
            currentSequence_ = seq;
            activeSequenceFound = true;
            break;
        }
    }
    if (!activeSequenceFound && !currentProject_.sequences.isEmpty()) {
        currentSequence_ = currentProject_.sequences.front();
        currentProject_.activeSequenceId = currentSequence_.id;
    }

    currentFrame_ = 0;
    inPoint_ = 0;
    outPoint_ = qMax<FramePosition>(0, currentSequence_.duration);
    playbackSpeed_ = PlaybackSpeed::Stop;
    selectedClipId_.clear();

    // NLE スナップショットがあればストアを復元し legacy をそこから再構築。
    // 無い旧形式ファイルは legacy → NLE 再構築して以後の編集を同期可能にする。
    const QJsonObject nleJson = doc.object()[QStringLiteral("nle")].toObject();
    if (nleStore_) {
        if (!nleJson.isEmpty() && nleStore_->loadFromJson(nleJson)) {
            rebuildLegacySnapshotFromNLE();
            // rebuild 後に activeSequenceId が変わる可能性があるため再取得
            for (const auto& seq : currentProject_.sequences) {
                if (seq.id == currentProject_.activeSequenceId) {
                    currentSequence_ = seq;
                    break;
                }
            }
        } else {
            importLegacyProjectToNLE();
        }
    }

    editSession_.undoStack()->clear();

    Q_EMIT sequenceChanged(currentSequence_);
    Q_EMIT projectLoaded(true, QStringLiteral("Project loaded successfully"));
    return true;
}

static bool validateProjectHealth(const DemoProject& project)
{
    bool hasErrors = false;
    
    // Check for missing media files
    for (const auto& media : project.mediaPool) {
        QFileInfo fi(media.filePath);
        if (!fi.exists() || !fi.isFile()) {
            qWarning() << "[validateProjectHealth] Missing media file:" << media.filePath;
            hasErrors = true;
        }
    }
    
    // Check for empty sequences
    if (project.sequences.isEmpty()) {
        qWarning() << "[validateProjectHealth] Project has no sequences";
        hasErrors = true;
    }
    
    // Check for invalid active sequence
    bool activeSeqFound = false;
    for (const auto& seq : project.sequences) {
        if (seq.id == project.activeSequenceId) {
            activeSeqFound = true;
            break;
        }
    }
    if (!project.activeSequenceId.isEmpty() && !activeSeqFound) {
        qWarning() << "[validateProjectHealth] Active sequence not found:" << project.activeSequenceId;
        hasErrors = true;
    }
    
    // Check for zero duration
    if (project.activeSequenceId.isEmpty() || project.sequences.isEmpty()) {
        // No sequences is already checked
    } else {
        for (const auto& seq : project.sequences) {
            if (seq.duration <= 0) {
                qWarning() << "[validateProjectHealth] Sequence has zero/negative duration:" << seq.id;
                hasErrors = true;
            }
        }
    }
    
    return !hasErrors;
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

void VideoTracksStateCommand::apply(const QVector<DemoTrack>& tracks,
                                    FramePosition duration,
                                    const QString& selection)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    auto sequence = engine->currentSequence();
    sequence.videoTracks = tracks;
    sequence.duration = duration;
    engine->setCurrentSequence(sequence);
    if (selection.isEmpty()) {
        engine->clearSelection();
    } else {
        engine->selectClip(selection);
    }
}

void VideoTracksStateCommand::undo()
{
    apply(before_, beforeDuration_, beforeSelection_);
}

void VideoTracksStateCommand::redo()
{
    apply(after_, afterDuration_, afterSelection_);
}

QJsonObject VideoTracksStateCommand::serialize() const
{
    QJsonArray tracks;
    for (const auto& t : after_) {
        tracks.append(trackToJson(t));
    }
    return QJsonObject{
        {QStringLiteral("tracks"), tracks},
        {QStringLiteral("duration"), static_cast<qint64>(afterDuration_)},
        {QStringLiteral("selection"), afterSelection_},
    };
}

bool VideoTracksStateCommand::deserialize(const QJsonObject& data)
{
    after_.clear();
    for (const auto& v : data[QStringLiteral("tracks")].toArray()) {
        after_.append(jsonToTrack(v.toObject()));
    }
    afterDuration_ = data[QStringLiteral("duration")].toInteger<qint64>();
    afterSelection_ = data[QStringLiteral("selection")].toString();
    setText(QStringLiteral("Change Video Clips"));
    return true;
}

void MarkerStateCommand::apply(const QVector<Marker>& markers)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    auto sequence = engine->currentSequence();
    sequence.markers = markers;
    engine->setCurrentSequence(sequence);
    Q_EMIT engine->markerChanged();
}

void TransitionStateCommand::apply(const QVector<Transition>& transitions)
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    auto sequence = engine->currentSequence();
    sequence.transitions = transitions;
    engine->setCurrentSequence(sequence);
    Q_EMIT engine->transitionChanged();
}

void TransitionStateCommand::undo()
{
    apply(before_);
}

void TransitionStateCommand::redo()
{
    apply(after_);
}

void MarkerStateCommand::undo()
{
    apply(before_);
}

void MarkerStateCommand::redo()
{
    apply(after_);
}

QJsonObject MarkerStateCommand::serialize() const
{
    QJsonArray markers;
    for (const auto& m : after_) {
        markers.append(markerToJson(m));
    }
    return QJsonObject{{QStringLiteral("markers"), markers}};
}

bool MarkerStateCommand::deserialize(const QJsonObject& data)
{
    after_.clear();
    for (const auto& v : data[QStringLiteral("markers")].toArray()) {
        after_.append(jsonToMarker(v.toObject()));
    }
    setText(QStringLiteral("Change Markers"));
    return true;
}

QJsonObject TransitionStateCommand::serialize() const
{
    QJsonArray transitions;
    for (const auto& t : after_) {
        transitions.append(transitionToJson(t));
    }
    return QJsonObject{{QStringLiteral("transitions"), transitions}};
}

bool TransitionStateCommand::deserialize(const QJsonObject& data)
{
    after_.clear();
    for (const auto& v : data[QStringLiteral("transitions")].toArray()) {
        after_.append(jsonToTransition(v.toObject()));
    }
    setText(QStringLiteral("Change Video Transition"));
    return true;
}

} // namespace ArtifactPr
