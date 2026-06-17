module;

#include <QString>
#include <QVector>

export module ArtifactPr.EditCommand;

import ArtifactPr.EditorEngine;

export namespace ArtifactPr {

// =====================================================================
// 6 種類の NLE 編集コマンド (UndoCommand 派生)。
// ---------------------------------------------------------------------
// すべて既存の UndoCommand パターンを踏襲 (MoveClipCommand / TrimClipCommand と同じ)。
// doXxx() private helper + undo/redo の 2 つだけ実装する。
// =====================================================================

/// Slip: clip の sourceIn / sourceOut を相対シフト。
/// クリップ位置 (startFrame) は変わらず、source 範囲だけ動く。
/// length 不変。隣のクリップに影響なし。
class SlipClipCommand : public UndoCommand {
public:
    SlipClipCommand(const QString& clipId,
                    FramePosition oldSourceIn, FramePosition oldSourceOut,
                    FramePosition newSourceIn, FramePosition newSourceOut)
        : clipId_(clipId),
          oldSourceIn_(oldSourceIn), oldSourceOut_(oldSourceOut),
          newSourceIn_(newSourceIn), newSourceOut_(newSourceOut) {}

    void undo() override { doSlip(oldSourceIn_, oldSourceOut_); }
    void redo() override { doSlip(newSourceIn_, newSourceOut_); }
    QString description() const override { return QStringLiteral("Slip Clip"); }

private:
    void doSlip(FramePosition sourceIn, FramePosition sourceOut);

    QString clipId_;
    FramePosition oldSourceIn_, oldSourceOut_;
    FramePosition newSourceIn_, newSourceOut_;
};

/// Slide: clip の startFrame を delta シフトし、
/// 隣の clip の end / start を吸収。
/// 長さは不変。隣接 clip の位置だけ調整。
class SlideClipCommand : public UndoCommand {
public:
    SlideClipCommand(const QString& clipId,
                     FramePosition oldStart, FramePosition newStart,
                     const QString& leftClipId, FramePosition oldLeftEnd,
                     const QString& rightClipId, FramePosition oldRightStart)
        : clipId_(clipId),
          oldStart_(oldStart), newStart_(newStart),
          leftClipId_(leftClipId), oldLeftEnd_(oldLeftEnd),
          rightClipId_(rightClipId), oldRightStart_(oldRightStart) {}

    void undo() override { doSlide(oldStart_, oldLeftEnd_, oldRightStart_); }
    void redo() override { doSlide(newStart_, newLeftEnd_, newRightStart_); }
    QString description() const override { return QStringLiteral("Slide Clip"); }

    void computeNewEdges();

private:
    void doSlide(FramePosition start, FramePosition leftEnd, FramePosition rightStart);

    QString clipId_;
    FramePosition oldStart_, newStart_;
    QString leftClipId_, rightClipId_;
    FramePosition oldLeftEnd_, oldRightStart_;
    FramePosition newLeftEnd_ = 0, newRightStart_ = 0;
};

/// Ripple Delete: 選択 clip を削除し、後続 clip を前に詰める。
/// シーケンス全体の duration が clip の長さ分縮む。
class RippleDeleteCommand : public UndoCommand {
public:
    RippleDeleteCommand(const QString& trackId, const DemoClip& clip, int index,
                        FramePosition oldDuration, FramePosition newDuration)
        : trackId_(trackId), clip_(clip), index_(index),
          oldDuration_(oldDuration), newDuration_(newDuration) {}

    void undo() override { doRipple(true); }
    void redo() override { doRipple(false); }
    QString description() const override { return QStringLiteral("Ripple Delete Clip"); }

private:
    void doRipple(bool undo);

    QString trackId_;
    DemoClip clip_;
    int index_;
    FramePosition oldDuration_, newDuration_;
};

/// Insert Edit: source clip を record timeline の playhead に挿入。
/// playhead 以降のクリップを後ろにずらす (ripple insert)。
class InsertEditCommand : public UndoCommand {
public:
    InsertEditCommand(const QString& trackId, const DemoClip& sourceClip,
                      FramePosition insertAt, FramePosition oldDuration, FramePosition newDuration)
        : trackId_(trackId), sourceClip_(sourceClip),
          insertAt_(insertAt),
          oldDuration_(oldDuration), newDuration_(newDuration) {}

    void undo() override { doInsert(true); }
    void redo() override { doInsert(false); }
    QString description() const override { return QStringLiteral("Insert Edit"); }

private:
    void doInsert(bool undo);

    QString trackId_;
    DemoClip sourceClip_;
    FramePosition insertAt_;
    FramePosition oldDuration_, newDuration_;
};

/// Overwrite Edit: source clip を playhead 位置で上書き。
/// playhead 以降のクリップと衝突した場合、衝突部分を切り詰める。
class OverwriteEditCommand : public UndoCommand {
public:
    OverwriteEditCommand(const QString& trackId, const DemoClip& sourceClip,
                         FramePosition overwriteAt)
        : trackId_(trackId), sourceClip_(sourceClip),
          overwriteAt_(overwriteAt) {}

    void undo() override { doOverwrite(true); }
    void redo() override { doOverwrite(false); }
    QString description() const override { return QStringLiteral("Overwrite Edit"); }

private:
    void doOverwrite(bool undo);

    QString trackId_;
    DemoClip sourceClip_;
    FramePosition overwriteAt_;
    QVector<DemoClip> removedClips_;  // 上書きで消えた clip (redo 時に保持)
};

/// Lift Edit: playhead 区間にある clip 部分を削除 (gap を作る)。
/// シーケンス duration は変えない。
class LiftEditCommand : public UndoCommand {
public:
    LiftEditCommand(const QString& trackId, FramePosition from, FramePosition to,
                     QVector<DemoClip> originalClips)
        : trackId_(trackId), from_(from), to_(to), originalClips_(std::move(originalClips)) {}

    void undo() override { doLift(true); }
    void redo() override { doLift(false); }
    QString description() const override { return QStringLiteral("Lift Edit"); }

private:
    void doLift(bool undo);

    QString trackId_;
    FramePosition from_, to_;
    QVector<DemoClip> originalClips_;
    QVector<DemoClip> removedClips_;  // redo 時に保持
};

/// Clip property (Volume / Speed / Reverse / Name) を UndoCommand 化。
/// variant で任意の property を扱う。
/// value は QVariant。QMetaType で型を保持。
class ClipPropertyCommand : public UndoCommand {
public:
    enum class Kind { Volume, Speed, Reverse, Name };

    ClipPropertyCommand(const QString& clipId, Kind kind,
                        QVariant oldValue, QVariant newValue)
        : clipId_(clipId), kind_(kind),
          oldValue_(std::move(oldValue)), newValue_(std::move(newValue)) {}

    void undo() override { doApply(oldValue_); }
    void redo() override { doApply(newValue_); }
    QString description() const override;

private:
    void doApply(const QVariant& v);

    QString clipId_;
    Kind kind_;
    QVariant oldValue_;
    QVariant newValue_;
};

} // namespace ArtifactPr