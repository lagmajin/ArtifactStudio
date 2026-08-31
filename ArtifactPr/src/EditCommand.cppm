module;

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QtGlobal>
#include <QVariant>
#include <QVector>

module ArtifactPr.EditCommand;

import ArtifactPr.EditCommand;
import ArtifactPr.EditorEngine;

namespace ArtifactPr {

namespace {

int insertedClipCounter = 0;

// serialize/deserialize 用のローカル DemoClip 変換 (Engine のプロジェクト保存
// 形式と同じキー名)。コマンドは必要フィールドのみを扱う。
QJsonObject commandClipToJson(const DemoClip& clip)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = clip.id;
    obj[QStringLiteral("name")] = clip.name;
    obj[QStringLiteral("sourceFile")] = clip.sourceFile;
    obj[QStringLiteral("startFrame")] = static_cast<qint64>(clip.startFrame);
    obj[QStringLiteral("duration")] = static_cast<qint64>(clip.duration);
    obj[QStringLiteral("sourceIn")] = static_cast<qint64>(clip.sourceIn);
    obj[QStringLiteral("sourceOut")] = static_cast<qint64>(clip.sourceOut);
    obj[QStringLiteral("color")] = clip.color;
    obj[QStringLiteral("linked")] = clip.linked;
    obj[QStringLiteral("selected")] = clip.selected;
    obj[QStringLiteral("reversed")] = clip.reversed;
    obj[QStringLiteral("speed")] = clip.speed;
    obj[QStringLiteral("volume")] = clip.volume;
    obj[QStringLiteral("enabled")] = clip.enabled;
    obj[QStringLiteral("opacity")] = clip.opacity;
    return obj;
}

DemoClip commandClipFromJson(const QJsonObject& obj)
{
    DemoClip clip;
    clip.id = obj[QStringLiteral("id")].toString();
    clip.name = obj[QStringLiteral("name")].toString();
    clip.sourceFile = obj[QStringLiteral("sourceFile")].toString();
    clip.startFrame = obj[QStringLiteral("startFrame")].toInteger<qint64>();
    clip.duration = obj[QStringLiteral("duration")].toInteger<qint64>();
    clip.sourceIn = obj[QStringLiteral("sourceIn")].toInteger<qint64>();
    clip.sourceOut = obj[QStringLiteral("sourceOut")].toInteger<qint64>();
    clip.color = obj[QStringLiteral("color")].toString(QStringLiteral("#4a9eff"));
    clip.linked = obj[QStringLiteral("linked")].toBool();
    clip.selected = obj[QStringLiteral("selected")].toBool();
    clip.reversed = obj[QStringLiteral("reversed")].toBool();
    clip.speed = obj[QStringLiteral("speed")].toDouble(1.0);
    clip.volume = obj[QStringLiteral("volume")].toDouble(1.0);
    clip.enabled = obj[QStringLiteral("enabled")].toBool(true);
    clip.opacity = obj[QStringLiteral("opacity")].toDouble(1.0);
    return clip;
}

} // namespace

// =====================================================================
// SlipClipCommand
// =====================================================================
QJsonObject SlipClipCommand::serialize() const {
    return QJsonObject{
        {QStringLiteral("clipId"), clipId_},
        {QStringLiteral("oldSourceIn"), static_cast<qint64>(oldSourceIn_)},
        {QStringLiteral("oldSourceOut"), static_cast<qint64>(oldSourceOut_)},
        {QStringLiteral("newSourceIn"), static_cast<qint64>(newSourceIn_)},
        {QStringLiteral("newSourceOut"), static_cast<qint64>(newSourceOut_)},
    };
}

bool SlipClipCommand::deserialize(const QJsonObject& data) {
    clipId_ = data[QStringLiteral("clipId")].toString();
    oldSourceIn_ = data[QStringLiteral("oldSourceIn")].toInteger<qint64>();
    oldSourceOut_ = data[QStringLiteral("oldSourceOut")].toInteger<qint64>();
    newSourceIn_ = data[QStringLiteral("newSourceIn")].toInteger<qint64>();
    newSourceOut_ = data[QStringLiteral("newSourceOut")].toInteger<qint64>();
    setText(QStringLiteral("Slip Clip"));
    return !clipId_.isEmpty();
}

void SlipClipCommand::doSlip(FramePosition sourceIn, FramePosition sourceOut) {
    auto* engine = EditorEngine::instance();
    auto* clip = engine->findClip(clipId_);
    if (clip) {
        clip->sourceIn = sourceIn;
        clip->sourceOut = sourceOut;
        Q_EMIT engine->clipChanged(clipId_);
        for (const auto& track : engine->currentSequence().videoTracks) {
            for (const auto& videoClip : track.clips) {
                if (videoClip.id == clipId_) {
                    engine->setCurrentSequence(engine->currentSequence());
                    return;
                }
            }
        }
    }
}

// =====================================================================
// SlideClipCommand
// =====================================================================
QJsonObject SlideClipCommand::serialize() const {
    return QJsonObject{
        {QStringLiteral("clipId"), clipId_},
        {QStringLiteral("oldStart"), static_cast<qint64>(oldStart_)},
        {QStringLiteral("newStart"), static_cast<qint64>(newStart_)},
        {QStringLiteral("leftClipId"), leftClipId_},
        {QStringLiteral("oldLeftEnd"), static_cast<qint64>(oldLeftEnd_)},
        {QStringLiteral("newLeftEnd"), static_cast<qint64>(newLeftEnd_)},
        {QStringLiteral("rightClipId"), rightClipId_},
        {QStringLiteral("oldRightStart"), static_cast<qint64>(oldRightStart_)},
        {QStringLiteral("newRightStart"), static_cast<qint64>(newRightStart_)},
    };
}

bool SlideClipCommand::deserialize(const QJsonObject& data) {
    clipId_ = data[QStringLiteral("clipId")].toString();
    oldStart_ = data[QStringLiteral("oldStart")].toInteger<qint64>();
    newStart_ = data[QStringLiteral("newStart")].toInteger<qint64>();
    leftClipId_ = data[QStringLiteral("leftClipId")].toString();
    oldLeftEnd_ = data[QStringLiteral("oldLeftEnd")].toInteger<qint64>();
    newLeftEnd_ = data[QStringLiteral("newLeftEnd")].toInteger<qint64>();
    rightClipId_ = data[QStringLiteral("rightClipId")].toString();
    oldRightStart_ = data[QStringLiteral("oldRightStart")].toInteger<qint64>();
    newRightStart_ = data[QStringLiteral("newRightStart")].toInteger<qint64>();
    setText(QStringLiteral("Slide Clip"));
    return !clipId_.isEmpty();
}

void SlideClipCommand::computeNewEdges() {
    // newStart - oldStart = delta
    // newLeftEnd = oldLeftEnd + delta
    // newRightStart = oldRightStart + delta
    const FramePosition delta = newStart_ - oldStart_;
    newLeftEnd_ = oldLeftEnd_ + delta;
    newRightStart_ = oldRightStart_ + delta;
}

void SlideClipCommand::doSlide(FramePosition start, FramePosition leftEnd, FramePosition rightStart) {
    auto* engine = EditorEngine::instance();
    auto* clip = engine->findClip(clipId_);
    if (clip) {
        clip->startFrame = start;
    }
    if (!leftClipId_.isEmpty()) {
        auto* leftClip = engine->findClip(leftClipId_);
        if (leftClip) {
            // leftClip の end = leftClip->startFrame + leftClip->duration
            // leftEnd_ は「leftClip の end (startFrame + duration)」
            leftClip->duration = leftEnd - leftClip->startFrame;
            Q_EMIT engine->clipChanged(leftClipId_);
        }
    }
    if (!rightClipId_.isEmpty()) {
        auto* rightClip = engine->findClip(rightClipId_);
        if (rightClip) {
            rightClip->startFrame = rightStart;
            Q_EMIT engine->clipChanged(rightClipId_);
        }
    }
    if (clip) {
        Q_EMIT engine->clipChanged(clipId_);
    }
    engine->setCurrentSequence(engine->currentSequence());
}

// =====================================================================
// RippleDeleteCommand
// =====================================================================
QJsonObject RippleDeleteCommand::serialize() const {
    return QJsonObject{
        {QStringLiteral("trackId"), trackId_},
        {QStringLiteral("clip"), commandClipToJson(clip_)},
        {QStringLiteral("index"), index_},
        {QStringLiteral("oldDuration"), static_cast<qint64>(oldDuration_)},
        {QStringLiteral("newDuration"), static_cast<qint64>(newDuration_)},
    };
}

bool RippleDeleteCommand::deserialize(const QJsonObject& data) {
    trackId_ = data[QStringLiteral("trackId")].toString();
    clip_ = commandClipFromJson(data[QStringLiteral("clip")].toObject());
    index_ = data[QStringLiteral("index")].toInt(-1);
    oldDuration_ = data[QStringLiteral("oldDuration")].toInteger<qint64>();
    newDuration_ = data[QStringLiteral("newDuration")].toInteger<qint64>();
    setText(QStringLiteral("Ripple Delete Clip"));
    return !trackId_.isEmpty();
}

void RippleDeleteCommand::doRipple(bool undo) {
    auto* engine = EditorEngine::instance();
    auto* track = engine->findTrack(trackId_);
    if (!track) return;

    if (undo) {
        // 再挿入
        if (index_ >= 0 && index_ <= track->clips.size()) {
            track->clips.insert(index_, clip_);
        }
        // duration 復元
        // シーケンス duration は DemoSequence.duration で表現
        // EditorEngine に setSequenceDuration() がないので
        // projectModified だけ emit
    } else {
        // 削除 + 後続を前に詰める
        for (int i = 0; i < track->clips.size(); ++i) {
            if (track->clips[i].id == clip_.id) {
                track->clips.removeAt(i);
                // 後続 clip を clip.duration 分前にシフト
                for (int j = i; j < track->clips.size(); ++j) {
                    track->clips[j].startFrame -= clip_.duration;
                }
                break;
            }
        }
    }
    auto sequence = engine->currentSequence();
    sequence.duration = qMax<FramePosition>(0, undo ? oldDuration_ : newDuration_);
    engine->setCurrentSequence(sequence);
    Q_EMIT engine->projectModified();
}

// =====================================================================
// InsertEditCommand
// =====================================================================
QJsonObject InsertEditCommand::serialize() const {
    return QJsonObject{
        {QStringLiteral("trackId"), trackId_},
        {QStringLiteral("sourceClip"), commandClipToJson(sourceClip_)},
        {QStringLiteral("insertAt"), static_cast<qint64>(insertAt_)},
        {QStringLiteral("oldDuration"), static_cast<qint64>(oldDuration_)},
        {QStringLiteral("newDuration"), static_cast<qint64>(newDuration_)},
        {QStringLiteral("insertedClipId"), insertedClipId_},
    };
}

bool InsertEditCommand::deserialize(const QJsonObject& data) {
    trackId_ = data[QStringLiteral("trackId")].toString();
    sourceClip_ = commandClipFromJson(data[QStringLiteral("sourceClip")].toObject());
    insertAt_ = data[QStringLiteral("insertAt")].toInteger<qint64>();
    oldDuration_ = data[QStringLiteral("oldDuration")].toInteger<qint64>();
    newDuration_ = data[QStringLiteral("newDuration")].toInteger<qint64>();
    insertedClipId_ = data[QStringLiteral("insertedClipId")].toString();
    setText(QStringLiteral("Insert Edit"));
    return !trackId_.isEmpty();
}

void InsertEditCommand::doInsert(bool undo) {
    auto* engine = EditorEngine::instance();
    auto* track = engine->findTrack(trackId_);
    if (!track) return;

    if (undo) {
        // 挿入した clip を除去
        for (int i = 0; i < track->clips.size(); ++i) {
            if (track->clips[i].id == insertedClipId_ && track->clips[i].startFrame == insertAt_) {
                track->clips.removeAt(i);
                // 後続 clip を元位置に戻す
                for (int j = i; j < track->clips.size(); ++j) {
                    track->clips[j].startFrame += sourceClip_.duration;
                }
                break;
            }
        }
    } else {
        // insert 位置より後の clip を後ろにずらす
        for (auto& c : track->clips) {
            if (c.startFrame >= insertAt_) {
                c.startFrame += sourceClip_.duration;
            }
        }
        // 新しい clip を insert
        DemoClip newClip = sourceClip_;
        newClip.startFrame = insertAt_;
        if (insertedClipId_.isEmpty()) {
            insertedClipId_ = QStringLiteral("insert_%1").arg(++insertedClipCounter);
        }
        newClip.id = insertedClipId_;
        // 同じ startFrame 位置に挿入 (sort 維持)
        int insertIndex = 0;
        for (int i = 0; i < track->clips.size(); ++i) {
            if (track->clips[i].startFrame < insertAt_) {
                insertIndex = i + 1;
            }
        }
        track->clips.insert(insertIndex, newClip);
    }
    auto sequence = engine->currentSequence();
    sequence.duration = qMax<FramePosition>(0, undo ? oldDuration_ : newDuration_);
    engine->setCurrentSequence(sequence);
    Q_EMIT engine->projectModified();
}

// =====================================================================
// OverwriteEditCommand
// =====================================================================
QJsonObject OverwriteEditCommand::serialize() const {
    QJsonArray removed;
    for (const auto& c : removedClips_) {
        removed.append(commandClipToJson(c));
    }
    return QJsonObject{
        {QStringLiteral("trackId"), trackId_},
        {QStringLiteral("sourceClip"), commandClipToJson(sourceClip_)},
        {QStringLiteral("overwriteAt"), static_cast<qint64>(overwriteAt_)},
        {QStringLiteral("insertedClipId"), insertedClipId_},
        {QStringLiteral("removedClips"), removed},
    };
}

bool OverwriteEditCommand::deserialize(const QJsonObject& data) {
    trackId_ = data[QStringLiteral("trackId")].toString();
    sourceClip_ = commandClipFromJson(data[QStringLiteral("sourceClip")].toObject());
    overwriteAt_ = data[QStringLiteral("overwriteAt")].toInteger<qint64>();
    insertedClipId_ = data[QStringLiteral("insertedClipId")].toString();
    removedClips_.clear();
    const auto removed = data[QStringLiteral("removedClips")].toArray();
    for (const auto& v : removed) {
        removedClips_.append(commandClipFromJson(v.toObject()));
    }
    setText(QStringLiteral("Overwrite Edit"));
    return !trackId_.isEmpty();
}

void OverwriteEditCommand::doOverwrite(bool undo) {
    auto* engine = EditorEngine::instance();
    auto* track = engine->findTrack(trackId_);
    if (!track) return;
    const bool isVideoTrack = track->kind == QStringLiteral("video");

    if (undo) {
        // 元の clip を除去 + removedClips を復元
        for (int i = 0; i < track->clips.size(); ++i) {
            if (track->clips[i].id == insertedClipId_) {
                track->clips.removeAt(i);
                break;
            }
        }
        for (const auto& c : removedClips_) {
            track->clips.append(c);
        }
    } else {
        // 衝突範囲の clip を saved + 削除
        removedClips_.clear();
        const FramePosition overlapStart = overwriteAt_;
        const FramePosition overlapEnd = overwriteAt_ + sourceClip_.duration;

        QVector<DemoClip> remaining;
        for (auto& c : track->clips) {
            const FramePosition cEnd = c.startFrame + c.duration;
            if (cEnd <= overlapStart || c.startFrame >= overlapEnd) {
                // 衝突なし
                remaining.append(c);
            } else {
                // 衝突あり -> 削除して保存
                removedClips_.append(c);
            }
        }
        track->clips = remaining;

        // sourceClip を挿入
        DemoClip newClip = sourceClip_;
        newClip.startFrame = overwriteAt_;
        if (insertedClipId_.isEmpty()) {
            insertedClipId_ = QStringLiteral("overwrite_%1").arg(++insertedClipCounter);
        }
        newClip.id = insertedClipId_;
        track->clips.append(newClip);
    }
    if (isVideoTrack) {
        engine->setCurrentSequence(engine->currentSequence());
    }
    Q_EMIT engine->projectModified();
}

// =====================================================================
// LiftEditCommand
// =====================================================================
QJsonObject LiftEditCommand::serialize() const {
    QJsonArray original;
    for (const auto& c : originalClips_) {
        original.append(commandClipToJson(c));
    }
    QJsonArray removed;
    for (const auto& c : removedClips_) {
        removed.append(commandClipToJson(c));
    }
    return QJsonObject{
        {QStringLiteral("trackId"), trackId_},
        {QStringLiteral("from"), static_cast<qint64>(from_)},
        {QStringLiteral("to"), static_cast<qint64>(to_)},
        {QStringLiteral("originalClips"), original},
        {QStringLiteral("removedClips"), removed},
    };
}

bool LiftEditCommand::deserialize(const QJsonObject& data) {
    trackId_ = data[QStringLiteral("trackId")].toString();
    from_ = data[QStringLiteral("from")].toInteger<qint64>();
    to_ = data[QStringLiteral("to")].toInteger<qint64>();
    originalClips_.clear();
    for (const auto& v : data[QStringLiteral("originalClips")].toArray()) {
        originalClips_.append(commandClipFromJson(v.toObject()));
    }
    removedClips_.clear();
    for (const auto& v : data[QStringLiteral("removedClips")].toArray()) {
        removedClips_.append(commandClipFromJson(v.toObject()));
    }
    setText(QStringLiteral("Lift Edit"));
    return !trackId_.isEmpty();
}

void LiftEditCommand::doLift(bool undo) {
    auto* engine = EditorEngine::instance();
    auto* track = engine->findTrack(trackId_);
    if (!track) return;
    const bool isVideoTrack = track->kind == QStringLiteral("video");

    if (undo) {
        // 部分 trim と削除を含む元の clip 配列をそのまま復元する。
        track->clips = originalClips_;
    } else {
        // from ~ to 区間にある clip 部分を抜き取る
        originalClips_ = track->clips;
        removedClips_.clear();

        QVector<DemoClip> remaining;
        for (auto& c : track->clips) {
            const FramePosition cEnd = c.startFrame + c.duration;
            if (cEnd <= from_ || c.startFrame >= to_) {
                // 区間外 -> そのまま
                remaining.append(c);
            } else if (c.startFrame >= from_ && cEnd <= to_) {
                // 全体が区間内 -> 削除して保存
                removedClips_.append(c);
            } else {
                // 部分的に区間内 -> 切り詰める
                DemoClip trimmed = c;
                if (c.startFrame < from_) {
                    trimmed.duration = from_ - c.startFrame;
                    remaining.append(trimmed);
                }
                if (cEnd > to_) {
                    DemoClip right = c;
                    right.startFrame = to_;
                    right.duration = cEnd - to_;
                    remaining.append(right);
                    removedClips_.append(right);  // 元の clip 範囲を記録
                }
            }
        }
        track->clips = remaining;
    }
    if (isVideoTrack) {
        engine->setCurrentSequence(engine->currentSequence());
    }
    Q_EMIT engine->projectModified();
}

// =====================================================================
// ClipPropertyCommand
// =====================================================================
QString ClipPropertyCommand::descriptionText() const {
    switch (kind_) {
    case Kind::Volume: return QStringLiteral("Change Clip Volume");
    case Kind::Speed:  return QStringLiteral("Change Clip Speed");
    case Kind::Reverse: return QStringLiteral("Reverse Clip");
    case Kind::Name:   return QStringLiteral("Rename Clip");
    case Kind::Opacity: return QStringLiteral("Change Clip Opacity");
    }
    return QStringLiteral("Change Clip Property");
}

QJsonObject ClipPropertyCommand::serialize() const {
    QJsonObject obj;
    obj[QStringLiteral("clipId")] = clipId_;
    obj[QStringLiteral("kind")] = static_cast<int>(kind_);
    obj[QStringLiteral("oldValue")] = oldValue_.toString();
    obj[QStringLiteral("newValue")] = newValue_.toString();
    return obj;
}

bool ClipPropertyCommand::deserialize(const QJsonObject& data) {
    clipId_ = data[QStringLiteral("clipId")].toString();
    kind_ = static_cast<Kind>(data[QStringLiteral("kind")].toInt(0));
    oldValue_ = data[QStringLiteral("oldValue")].toString();
    newValue_ = data[QStringLiteral("newValue")].toString();
    setText(descriptionText());
    return !clipId_.isEmpty();
}

void ClipPropertyCommand::doApply(const QVariant& v) {
    auto* engine = EditorEngine::instance();
    auto* clip = engine->findClip(clipId_);
    if (!clip) return;

    switch (kind_) {
    case Kind::Volume:
        clip->volume = v.toDouble();
        Q_EMIT engine->clipChanged(clipId_);
        break;
    case Kind::Speed:
        clip->speed = v.toDouble();
        Q_EMIT engine->clipChanged(clipId_);
        break;
    case Kind::Reverse:
        clip->reversed = v.toBool();
        Q_EMIT engine->clipChanged(clipId_);
        break;
    case Kind::Name:
        clip->name = v.toString();
        Q_EMIT engine->clipChanged(clipId_);
        break;
    case Kind::Opacity:
        clip->opacity = v.toDouble();
        Q_EMIT engine->clipChanged(clipId_);
        break;
    }
    for (const auto& track : engine->currentSequence().videoTracks) {
        for (const auto& videoClip : track.clips) {
            if (videoClip.id == clipId_) {
                engine->setCurrentSequence(engine->currentSequence());
                Q_EMIT engine->projectModified();
                return;
            }
        }
    }
    Q_EMIT engine->projectModified();
}

// =====================================================================
// ClipEffectsCommand
// =====================================================================
QJsonObject clipEffectsToJson(const QMap<QString, QVariant>& effects)
{
    QJsonObject obj;
    for (auto it = effects.constKeyValueBegin(); it != effects.constKeyValueEnd(); ++it) {
        obj[it->first] = it->second.toString();
    }
    return obj;
}

QMap<QString, QVariant> clipEffectsFromJson(const QJsonObject& obj)
{
    QMap<QString, QVariant> effects;
    for (const auto& key : obj.keys()) {
        effects.insert(key, obj.value(key).toString());
    }
    return effects;
}

QJsonObject ClipEffectsCommand::serialize() const {
    return QJsonObject{
        {QStringLiteral("clipId"), clipId_},
        {QStringLiteral("oldEffects"), clipEffectsToJson(oldEffects_)},
        {QStringLiteral("newEffects"), clipEffectsToJson(newEffects_)},
    };
}

bool ClipEffectsCommand::deserialize(const QJsonObject& data) {
    clipId_ = data[QStringLiteral("clipId")].toString();
    oldEffects_ = clipEffectsFromJson(data[QStringLiteral("oldEffects")].toObject());
    newEffects_ = clipEffectsFromJson(data[QStringLiteral("newEffects")].toObject());
    setText(QStringLiteral("Change Clip Effects"));
    return !clipId_.isEmpty();
}

void ClipEffectsCommand::doApply(const QMap<QString, QVariant>& effects) {
    auto* engine = EditorEngine::instance();
    auto* clip = engine->findClip(clipId_);
    if (!clip) return;

    clip->effects = effects;
    Q_EMIT engine->clipChanged(clipId_);
    engine->setCurrentSequence(engine->currentSequence());
    Q_EMIT engine->projectModified();
}

} // namespace ArtifactPr
