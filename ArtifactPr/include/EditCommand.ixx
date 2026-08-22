module;

#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QVector>

export module ArtifactPr.EditCommand;

import ArtifactPr.EditorEngine;
import Command.Serializable;

export namespace ArtifactPr {

using FramePosition = int64_t;

// =====================================================================
// NLE 編集コマンド群 (Core SerializableCommand 派生)。
// ---------------------------------------------------------------------
// QUndoStack::push が redo() を即座に実行するため、全 redo は絶対値セット
// (冪等) として動作する。doXxx() private helper + undo/redo の2つだけ実装。
// =====================================================================

/// Slip: clip の sourceIn / sourceOut を絶対値へセット。
/// クリップ位置 (startFrame) は変わらず、source 範囲だけ動く。length 不変。
class SlipClipCommand : public ArtifactCore::SerializableCommand {
public:
    static constexpr auto kType = QStringLiteral("nle.slipClip");

    SlipClipCommand(const QString& clipId,
                    FramePosition oldSourceIn, FramePosition oldSourceOut,
                    FramePosition newSourceIn, FramePosition newSourceOut)
        : clipId_(clipId),
          oldSourceIn_(oldSourceIn), oldSourceOut_(oldSourceOut),
          newSourceIn_(newSourceIn), newSourceOut_(newSourceOut)
    {
        setText(QStringLiteral("Slip Clip"));
    }

    void undo() override { doSlip(oldSourceIn_, oldSourceOut_); }
    void redo() override { doSlip(newSourceIn_, newSourceOut_); }

    QString commandType() const override { return kType; }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;

private:
    void doSlip(FramePosition sourceIn, FramePosition sourceOut);

    QString clipId_;
    FramePosition oldSourceIn_, oldSourceOut_;
    FramePosition newSourceIn_, newSourceOut_;
};

/// Slide: clip の startFrame と隣接 clip の端点を絶対値でセットする。長さ不変。
class SlideClipCommand : public ArtifactCore::SerializableCommand {
public:
    static constexpr auto kType = QStringLiteral("nle.slideClip");

    SlideClipCommand(const QString& clipId,
                     FramePosition oldStart, FramePosition newStart,
                     const QString& leftClipId, FramePosition oldLeftEnd,
                     const QString& rightClipId, FramePosition oldRightStart)
        : clipId_(clipId),
          oldStart_(oldStart), newStart_(newStart),
          leftClipId_(leftClipId), oldLeftEnd_(oldLeftEnd),
          rightClipId_(rightClipId), oldRightStart_(oldRightStart)
    {
        setText(QStringLiteral("Slide Clip"));
    }

    void undo() override { doSlide(oldStart_, oldLeftEnd_, oldRightStart_); }
    void redo() override { doSlide(newStart_, newLeftEnd_, newRightStart_); }

    QString commandType() const override { return kType; }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;

    /// redo/undo が使う隣接 clip の新しい端点を計算して確定させる。
    /// push 前に必ず 1 回呼ぶこと (QUndoStack::push が redo を即座に実行するため)。
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
class RippleDeleteCommand : public ArtifactCore::SerializableCommand {
public:
    static constexpr auto kType = QStringLiteral("nle.rippleDelete");

    RippleDeleteCommand(const QString& trackId, const DemoClip& clip, int index,
                        FramePosition oldDuration, FramePosition newDuration)
        : trackId_(trackId), clip_(clip), index_(index),
          oldDuration_(oldDuration), newDuration_(newDuration)
    {
        setText(QStringLiteral("Ripple Delete Clip"));
    }

    void undo() override { doRipple(true); }
    void redo() override { doRipple(false); }

    QString commandType() const override { return kType; }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;

private:
    void doRipple(bool undo);

    QString trackId_;
    DemoClip clip_;
    int index_;
    FramePosition oldDuration_, newDuration_;
};

/// Insert Edit: source clip を record timeline の playhead に挿入。
/// playhead 以降のクリップを後ろにずらす (ripple insert)。
class InsertEditCommand : public ArtifactCore::SerializableCommand {
public:
    static constexpr auto kType = QStringLiteral("nle.insertEdit");

    InsertEditCommand(const QString& trackId, const DemoClip& sourceClip,
                      FramePosition insertAt, FramePosition oldDuration, FramePosition newDuration)
        : trackId_(trackId), sourceClip_(sourceClip),
          insertAt_(insertAt),
          oldDuration_(oldDuration), newDuration_(newDuration)
    {
        setText(QStringLiteral("Insert Edit"));
    }

    void undo() override { doInsert(true); }
    void redo() override { doInsert(false); }

    QString commandType() const override { return kType; }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;

private:
    void doInsert(bool undo);

    QString trackId_;
    DemoClip sourceClip_;
    FramePosition insertAt_;
    FramePosition oldDuration_, newDuration_;
    // 冪等 redo のため、初回 redo 時に確定した id を保持する
    QString insertedClipId_;
};

/// Overwrite Edit: source clip を playhead 位置で上書き。
/// playhead 以降のクリップと衝突した場合、衝突部分を切り詰める。
class OverwriteEditCommand : public ArtifactCore::SerializableCommand {
public:
    static constexpr auto kType = QStringLiteral("nle.overwriteEdit");

    OverwriteEditCommand(const QString& trackId, const DemoClip& sourceClip,
                         FramePosition overwriteAt)
        : trackId_(trackId), sourceClip_(sourceClip),
          overwriteAt_(overwriteAt)
    {
        setText(QStringLiteral("Overwrite Edit"));
    }

    void undo() override { doOverwrite(true); }
    void redo() override { doOverwrite(false); }

    QString commandType() const override { return kType; }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;

private:
    void doOverwrite(bool undo);

    QString trackId_;
    DemoClip sourceClip_;
    FramePosition overwriteAt_;
    QString insertedClipId_;
    QVector<DemoClip> removedClips_;  // 上書きで消えた clip (redo 冪等化のため保持)
};

/// Lift Edit: playhead 区間にある clip 部分を削除 (gap を作る)。
/// シーケンス duration は変えない。
class LiftEditCommand : public ArtifactCore::SerializableCommand {
public:
    static constexpr auto kType = QStringLiteral("nle.liftEdit");

    LiftEditCommand(const QString& trackId, FramePosition from, FramePosition to,
                     QVector<DemoClip> originalClips)
        : trackId_(trackId), from_(from), to_(to), originalClips_(std::move(originalClips))
    {
        setText(QStringLiteral("Lift Edit"));
    }

    void undo() override { doLift(true); }
    void redo() override { doLift(false); }

    QString commandType() const override { return kType; }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;

private:
    void doLift(bool undo);

    QString trackId_;
    FramePosition from_, to_;
    QVector<DemoClip> originalClips_;  // undo 用スナップショット (初回 redo で保存される)
    QVector<DemoClip> removedClips_;   // redo 時に保持
};

/// Clip property (Volume / Speed / Reverse / Name / Opacity) の変更。
/// value は QVariant。QMetaType で型を保持。
class ClipPropertyCommand : public ArtifactCore::SerializableCommand {
public:
    enum class Kind { Volume, Speed, Reverse, Name, Opacity };

    static constexpr auto kType = QStringLiteral("nle.clipProperty");

    ClipPropertyCommand(const QString& clipId, Kind kind,
                        QVariant oldValue, QVariant newValue)
        : clipId_(clipId), kind_(kind),
          oldValue_(std::move(oldValue)), newValue_(std::move(newValue))
    {
        setText(descriptionText());
    }

    void undo() override { doApply(oldValue_); }
    void redo() override { doApply(newValue_); }

    QString commandType() const override { return kType; }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;

private:
    QString descriptionText() const;
    void doApply(const QVariant& v);

    QString clipId_;
    Kind kind_;
    QVariant oldValue_;
    QVariant newValue_;
};

} // namespace ArtifactPr
