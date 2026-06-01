# Milestone: Timeline Audio Waveform Display

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

## 未着手要素（設計漏れ）

- TimelineTrackPainterView への Audio 専用描画分岐
- 時間縮尺ごとの waveform 粗密制御（ズームアウトで peak ピンへ置換）
- Audio 行のフェードハンドル描画
- 再生ヘッド追従の時点ハイライト
- タイムラインからのドラッグで trim / fade / gain 編集

---

## フェーズ

### Phase 1: Audio Track Visual Description

- `LayerPresentationDescriptor` 拡張で Audio-only 行を返却
  - `isAudio_ = true` / `audioPeakData_` / `audioDuration_`
- `ArtifactLayerPanelWidget::describeLayerPresentation()` が AudioLayer で
  true を返す branch を実装
- これにより `ArtifactTimelineTrackPainterView` 側で row ごとに判定できる状態を作る

### Phase 2: Waveform Rendering in TrackPainterView

- `TrackClipVisual` に `audioWaveformPeaks_` を追加
- Audio Layer の PCM を 256 bin peak（min/max ペア）へ事前サンプリングし
  `ArtifactAudioBuffer` から取得
- Paint 時に clip の time-to-x 変換を使い、対応する peak bin を縦線で描画
- 縮小時は n-frame 単位で max を1つに寄せ、頻度を下げる

### Phase 3: Fade / Gain Handles

- クリップ左右端にフェードイン/アウト三角ハンドルを表示
- 既存 `ArtifactLayer` の `audioFadeIn_ / audioFadeOut_` を inspected し、
  painter で三角グラデで表示（フェード無し=ハンドル非表示）

### Phase 4: Playhead Highlight and Scrubbing

- 再生ヘッド位置で waveform を強調（明るい縦線 + 周囲ハイライト）
- 波形クリック → playhead seek を `ArtifactTimelineWidget` -> EventBus で伝達
- 音量オートメーション keyframe のタイムライントラック描画と waveform が
  重ならないようレイヤー順序を制御

### Phase 5: Editing Handles (Trim / Fade Drag / Gain)

- クリップ端ドラッグで trim — 既存の keyframe drag 経路を再利用
- フェードハンドルドラッグで `audioFadeIn_ / audioFadeOut_` 更新
  - Undo は `SetPropertyCommand` で積む
- clip 枠内クリック → 音量 lane の keyframe が無ければ 1 つ追加
- ドラッグで gain 値更新。keyframe モデルへ書き込み、Playback にも反映

---

## 検証条件

- Audio Layer を Composition に追加 → Timeline の対応行に波形が描画される
- ズームアウト / ズームインで波形の詳細レベルが適切に変わる
- フェードハンドルをドラッグ → オーディオにフェードが掛かり、Inspector の値が更新
- Undo/Redo で trim / fade / gain 変更が巻き戻る
- 波形クリックで再生ヘッドが移動し、再生するとその位置から音が聞こえる

---

## 関連ファイル（影響）

- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`
- `Artifact/include/Layer/ArtifactAudioLayer.ixx` / `.cpp`
- `Artifact/src/Audio/AudioWaveform` (参照: PCM -> peak 抽出 API を追加)

---

## 見積

- Phase 1: 4–6h
- Phase 2: 6–10h
- Phase 3: 4–6h
- Phase 4: 4–6h
- Phase 5: 8–12h

合計: 26–40h
