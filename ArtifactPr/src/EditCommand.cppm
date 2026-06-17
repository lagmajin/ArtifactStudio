module;

#include <QObject>
#include <QVariant>
#include <QVector>

module ArtifactPr.EditCommand;

import ArtifactPr.EditCommand;
import ArtifactPr.EditorEngine;

namespace ArtifactPr {

// =====================================================================
// SlipClipCommand
// =====================================================================
void SlipClipCommand::doSlip(FramePosition sourceIn, FramePosition sourceOut) {
    auto* engine = EditorEngine::instance();
    auto* clip = engine->findClip(clipId_);
    if (clip) {
        clip->sourceIn = sourceIn;
        clip->sourceOut = sourceOut;
        Q_EMIT engine->clipChanged(clipId_);
    }
}

// =====================================================================
// SlideClipCommand
// =====================================================================
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
}

// =====================================================================
// RippleDeleteCommand
// =====================================================================
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
    Q_EMIT engine->projectModified();
}

// =====================================================================
// InsertEditCommand
// =====================================================================
void InsertEditCommand::doInsert(bool undo) {
    auto* engine = EditorEngine::instance();
    auto* track = engine->findTrack(trackId_);
    if (!track) return;

    if (undo) {
        // 挿入した clip を除去
        for (int i = 0; i < track->clips.size(); ++i) {
            if (track->clips[i].id == sourceClip_.id && track->clips[i].startFrame == insertAt_) {
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
        newClip.id = QStringLiteral("insert_%1").arg(QObject::sender() ? QString() : QString());
        // 同じ startFrame 位置に挿入 (sort 維持)
        int insertIndex = 0;
        for (int i = 0; i < track->clips.size(); ++i) {
            if (track->clips[i].startFrame < insertAt_) {
                insertIndex = i + 1;
            }
        }
        track->clips.insert(insertIndex, newClip);
    }
    Q_EMIT engine->projectModified();
}

// =====================================================================
// OverwriteEditCommand
// =====================================================================
void OverwriteEditCommand::doOverwrite(bool undo) {
    auto* engine = EditorEngine::instance();
    auto* track = engine->findTrack(trackId_);
    if (!track) return;

    if (undo) {
        // 元の clip を除去 + removedClips を復元
        for (int i = 0; i < track->clips.size(); ++i) {
            if (track->clips[i].id == sourceClip_.id) {
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
        track->clips.append(newClip);
    }
    Q_EMIT engine->projectModified();
}

// =====================================================================
// LiftEditCommand
// =====================================================================
void LiftEditCommand::doLift(bool undo) {
    auto* engine = EditorEngine::instance();
    auto* track = engine->findTrack(trackId_);
    if (!track) return;

    if (undo) {
        // originalClips を復元 + removedClips を除去
        for (int i = 0; i < track->clips.size(); ++i) {
            // 同じ startFrame 範囲を持つ clip を除去 (lift で作られた gap)
        }
        for (const auto& c : originalClips_) {
            track->clips.append(c);
        }
        for (const auto& c : removedClips_) {
            // 衝突部分を削除
            track->clips.removeIf([&](const DemoClip& c) {
                return c.id == c.id;  // 同一性で除去
            });
        }
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
    Q_EMIT engine->projectModified();
}

// =====================================================================
// ClipPropertyCommand
// =====================================================================
QString ClipPropertyCommand::description() const {
    switch (kind_) {
    case Kind::Volume: return QStringLiteral("Change Clip Volume");
    case Kind::Speed:  return QStringLiteral("Change Clip Speed");
    case Kind::Reverse: return QStringLiteral("Reverse Clip");
    case Kind::Name:   return QStringLiteral("Rename Clip");
    }
    return QStringLiteral("Change Clip Property");
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
    }
}

} // namespace ArtifactPr