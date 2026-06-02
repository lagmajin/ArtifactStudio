# MILESTONE: Cross-Industry Inspection Tools

**Date**: 2026-06-02  
**Status**: Proposed  
**Priority**: High  
**Related**: `docs/WIDGET_MAP.md`, `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`, `docs/planned/MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`, `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`, `docs/planned/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`

---

## 概要

AE 風アプリの差別化として、他業界ソフトの「観察」「比較」「危険可視化」の思想を輸入する。

目的は機能数を増やすことではなく、

- なぜこのフレームだけ変なのか
- どこまで effect が効いているのか
- 前の案と今の案のどちらが良いのか
- どこで motion が急変したのか

を短時間で読めるようにすること。

この milestone は、`Timeline`、`Overlay.Composition`、`FrameDebugViewWidget`、`AppDebuggerWidget` にまたがる inspection 系の上位整理として扱う。

---

## 1. Motion Ghost / Version Ghost

### 着想元

- ゲームの replay / ghost
- editor の before / after compare

### 何をしたいか

- 前回保存時の motion を半透明で重ねる
- A 案 / B 案の動きを同じ timeline 文脈で比較する
- キーフレーム修正前の姿勢を薄く残す

### 最小実装

- 選択 layer の transform snapshot を保持
- current frame の前後数点を ghost overlay で描く
- 1 つ前の snapshot だけ比較できるようにする

### 向いている surface

- `Overlay.Composition`
- `TimelineTrackView`
- `ArtifactContentsViewer` compare mode

### 実現難度

- 中

### メモ

- full undo history を直接見せるより、比較用 snapshot を明示的に持つほうが安全
- まずは transform / bounds / anchor のみでよい

---

## 2. Effect Hitbox View

### 着想元

- ゲームの hitbox / hurtbox / trigger volume

### 何をしたいか

- visible bounds
- mask bounds
- matte 範囲
- glow / blur の影響範囲
- motion blur の到達範囲
- click / interaction 可能域

を色分けして重ねる。

### 最小実装

- layer bounds
- alpha bounds
- mask bounds
- blur radius expanded bounds

を overlay で表示する。

### 向いている surface

- `Overlay.Composition`
- `FrameDebugViewWidget`

### 実現難度

- 低〜中

### メモ

- この案は最も即効性が高い
- effect ごとの ROI / expanded bounds の文法整理にもつながる

---

## 3. Frame Slice Inspector

### 着想元

- 医療画像ビューア
- CT / MRI の slice / probe

### 何をしたいか

- 特定 pixel の時間方向履歴を見る
- RGBA / alpha / luminance の変化を見る
- 前後 frame の縦断面 / 横断面を比較する
- 「この 1 ピクセルだけなぜ壊れたか」を追う

### 最小実装

- pixel probe
- frame-to-frame RGBA history
- single-point chart

### 向いている surface

- `FrameResourceInspectorWidget`
- `FrameDebugViewWidget`
- `AppDebuggerWidget`

### 実現難度

- 中

### メモ

- debug 用途の価値が高い
- UI は重くしすぎず、まずは 1 点 probe に限定する

---

## 4. Timeline Volatility View

### 着想元

- 株チャート
- トレードツールの出来高 / ボラティリティ表示

### 何をしたいか

- キーフレーム変化量の強さを下段バーで見る
- 急変ポイントを検出する
- 平滑化候補を見つける
- speed graph とは別に「荒れ」を読む

### 最小実装

- position / scale / rotation / opacity の変化量を集計
- selected property の変化量バーを timeline 下段へ表示
- threshold 超えを warning marker 化

### 向いている surface

- `TimelineTrackView`
- `ArtifactTimelineWidget`

### 実現難度

- 中

### メモ

- graph editor の代替ではなく補助レイヤーとして扱う
- `変化量` と `意図した演出` を混同しない UI が必要

---

## 5. Difference / Leakage View

### 着想元

- 電子顕微鏡 / 科学可視化
- A/B 差分観察

### 何をしたいか

- A/B frame difference
- alpha leak
- compression artifact visibility
- color difference
- noise-only 抽出

### 最小実装

- absolute diff
- alpha-only diff
- threshold diff
- false-color difference heatmap

### 向いている surface

- `ArtifactContentsViewer`
- `FrameDebugViewWidget`
- `FrameStateDiffWidget`

### 実現難度

- 低〜中

### メモ

- `alpha leak view` は特に優先度が高い
- `DeltaE` は色管理前提が固まってから第 2 段階でよい

---

## 推奨優先順位

1. `Effect Hitbox View`
2. `Difference / Leakage View`
3. `Motion Ghost / Version Ghost`
4. `Timeline Volatility View`
5. `Frame Slice Inspector`

---

## なぜこの順か

- `Effect Hitbox View` は overlay 追加で価値が出やすい
- `Difference / Leakage View` は既存 debug surface と自然につながる
- `Motion Ghost` は魅力が大きいが、snapshot 契約が必要
- `Timeline Volatility` は timeline 読みやすさ改善として有望
- `Frame Slice Inspector` は強いが、UI と readback の設計がやや重い

---

## 実装マップ

### Overlay.Composition

- `Effect Hitbox View`
- `Motion Ghost`

### Timeline

- `Timeline Volatility View`
- `Version Ghost` の marker / compare導線

### Frame Debug / App Debugger

- `Frame Slice Inspector`
- `Difference / Leakage View`
- hitbox / bounds の理由説明

### Contents Viewer

- A/B compare
- false-color diff
- alpha-only compare

---

## Phase 1 候補

Phase 1 は、最小の差分で価値が出る次の 2 本に絞る。

- `Effect Hitbox View`
- `Difference / Leakage View`

この 2 本は、inspection 機能としての体感価値が高く、既存 surface への接続も比較的軽い。

---

## Related Execution Memo

- [`MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_PHASE1_EXECUTION_2026-06-02.md`](./MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_PHASE1_EXECUTION_2026-06-02.md)
