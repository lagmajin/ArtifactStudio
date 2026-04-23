# Milestone: Timeline Audio Layer Specialization (2026-04-23)

**Status:** Planning
**Parent:** `MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23`
**Goal:** タイムラインの Audio layer を最初の専用化対象として、波形・状態・編集ハンドルを共通タイムラインに自然に足す。

---

## まずやること

Audio layer は、他のレイヤーより「タイムライン上で見たい情報」が明確。

- 波形
- ミュート / ソロ / ロック
- 音量オートメーション
- フェードイン / フェードアウト
- 再生中の現在位置

そのため、最初の専用化対象に向いている。

---

## 既存の土台

- `ArtifactTimelineTrackPainterView::TrackClipVisual`
- `ArtifactTimelineTrackPainterView::KeyframeMarkerVisual`
- `TimelineRowDescriptor`
- `LayerPresentationDescriptor`
- `ArtifactLayerPanelWidget::describeLayerPresentation()`

これらがあるので、Audio layer を別ビューに分けずとも、
「共通トラック + Audio 専用の補助描画」で始められる。

---

## Phase 1: Audio track descriptor

Audio layer 向けに以下を返す descriptor を用意する。

- 表示ラベル
- 色
- 専用バッジ
- 補助テキスト
- 編集可能な補助ハンドルの有無

候補:
- `Audio`
- `Waveform`
- `Mute`
- `Solo`
- `Lock`

---

## Phase 2: 波形の常時表示

- Audio clip の内部に波形を薄く表示する
- 非表示時は簡易バー表示に落とす
- ズームアウト時はピークラインだけに間引く

---

## Phase 3: Audio 操作の見える化

- ミュート / ソロ / ロックをトラックヘッダに表示
- クリップ端のフェードハンドル
- 音量キーフレームの可視化
- 再生中の波形追従

---

## Phase 4: Audio 専用の編集導線

- クリックで波形の位置に seek
- クリップ端のドラッグで trim
- 音量ラインのドラッグで gain 調整
- 既存の keyframe 編集と衝突しないようにする

---

## 実装メモ

- `ArtifactTimelineTrackPainterView::setClips()` に Audio 用の補助情報を足す
- `visibleTimelineRowDescriptors()` から row 種別を判定し、Audio だけ別描画へ分岐する
- まずは表示だけで、編集ハンドルは後から足す

---

## 優先順位

1. 波形表示
2. Audio 状態のバッジ
3. フェードハンドル
4. 音量オートメーション

---

## 連携先

- [`MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md)
- [`MILESTONE_TIMELINE_LAYER_SPECIALIZATION_EXECUTION_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_LAYER_SPECIALIZATION_EXECUTION_2026-04-23.md)
