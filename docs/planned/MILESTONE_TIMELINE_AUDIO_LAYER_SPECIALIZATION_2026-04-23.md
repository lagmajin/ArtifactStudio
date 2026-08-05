> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md](MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md)

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
- Phase 1 execution memo is absorbed into the parent milestone

## 2026-07-25 現状確認

共通タイムライン上で Audio Layer の専用化は進んでいる。`TrackClipVisual::Kind::Audio`、Audio Layer／Video Layer の波形生成、署名付きキャッシュ、peak／RMS 描画、音声状態の補助表示、Scrub Preview 導線が実装済み。Audio Layer の mute／solo／volume／pan は既存 Layer／Mixer 経路と連携する。

未完了または未検証なのは、トラックヘッダ専用バッジの一貫した表示、クリップ端のフェードハンドル、音量ラインの直接ドラッグ編集、波形クリックからの専用 seek、再生ヘッドとの実時間追従、ズームアウト時の段階的間引きである。判定は「波形・状態表示・スクラブ基盤は実装済み、Audio専用編集ハンドルは未完了」とする。
