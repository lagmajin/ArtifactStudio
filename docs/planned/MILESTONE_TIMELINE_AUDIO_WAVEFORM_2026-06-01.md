# Milestone: Timeline Audio Waveform Display

Status: Archived reference

作成日: 2026-06-01
親: `MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md`
関連: `MILESTONE_TIMELINE_AUDIO_LAYER_SPECIALIZATION_2026-04-23.md`, `MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md` (Item 9)

---

## 目的

タイムライン上で Audio Layer の音声波形を clip 領域内へ連続的に表示し、
オーディオ編集（フェード、trim、gain）を視覚的に行えるようにする。
`MILESTONE_TIMELINE_AUDIO_LAYER_SPECIALIZATION_2026-04-23.md` で設計された
Audio Layer descriptor + TrackPainterView 拡張を採用し、残る未着手となっている
実装面を完了する。

---

## 既存の土台

- `Artifact/src/Audio/FFmpegAudioDecoder.cppm` — PCM デコード済み
- `ArtifactCore/src/Audio/AudioBuffer.cppm` — AudioBus / buffer 管理
- `Artifact/src/Audio/AudioWaveform` — 波形 widget
- `ArtifactTimelineTrackPainterView::TrackClipVisual` — clip 内描画構造
- `TimelineRowDescriptor` / `LayerPresentationDescriptor` — 行カテゴライズ API
- `ArtifactLayerPanelWidget::describeLayerPresentation()` — 行の descriptor 返却

---

## 役割

この文書は完成済みの実装メモとして残す。実作業の参照先は `docs/done/MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md` に移した。

---

## 実装メモ

- `ArtifactTimelineTrackPainterView` の audio waveform 描画は正規経路として扱う
- `ArtifactTimelineWidget` 側で waveform データの準備とキャッシュが行われる
- 関連するフェードやドラッグ編集は別の整理タスクへ切り出して扱う

---

## 検証条件

- Audio Layer を Composition に追加すると Timeline の対応行に波形が見える
- ズーム変更に応じて waveform の見え方が追従する
- waveform ready / preview 状態の debug 表示が破綻しない

---

## 関連ファイル（影響）

- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`
- `Artifact/include/Layer/ArtifactAudioLayer.ixx` / `.cpp`
- `Artifact/src/Audio/AudioWaveform`
