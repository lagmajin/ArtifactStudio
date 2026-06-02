# MILESTONE: Visual Density Monitor

**Date**: 2026-06-03  
**Status**: Proposed  
**Priority**: Medium-High  
**Related**: `docs/WIDGET_MAP.md`, `docs/planned/MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`, `docs/planned/MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md`

---

## 概要

養殖・水槽管理の発想を AE 風アプリへ輸入し、画面が「情報で詰まりすぎていないか」を定量化する。

この機能は派手な演出ではなく、作業者の視線と理解を守るための診断メーターとする。

狙いは次の 4 つ。

- どこが詰まりすぎているかを見える化する
- どこを少し空けると読みやすくなるかを示す
- 明るさ・文字・線・動きの密度を同じ語彙で扱う
- 画面全体の「うるささ」を主観ではなく補助指標で読む

---

## 1. Phase 1 Indicators

最初の段階で入れる指標は、実装コストと効きのバランスがよい順に固定する。

### 1-1. Visual Density

画面そのものの詰まり具合。

候補:

- layer count
- visible object count
- text / label count
- edge / outline count
- overlapping bounds count

### 1-2. Information Density

情報量の詰まり具合。

候補:

- UI control count
- annotation count
- warning badge count
- per-region label count
- repeated semantic hints count

### 1-3. Luminance Density

明るさの集中度。

候補:

- local brightness concentration
- highlight saturation
- dark region clustering
- high contrast edge load

### 1-4. Motion Density

動きの詰まり具合。

候補:

- selected region motion delta
- frame-to-frame delta accumulation
- rapid keyframe cluster count
- animation activity hotspot

---

## 2. Surface Placement

新しい巨大 widget は増やさず、既存 surface に分散させる。

### 2-1. Overlay.Composition

主な役割:

- selected region の density heatmap
- crowding warning の軽いオーバーレイ
- 空ける余白の候補表示

### 2-2. FrameDebugViewWidget

主な役割:

- frame 単位の density summary
- warning / next action
- compare mode との関係

### 2-3. AppDebuggerWidget

主な役割:

- density monitor の全体 summary
- problem view / frame debug への導線
- heavy panel ではなく観測窓としての提示

### 2-4. TimelineTrackView

将来の役割:

- timeline 上の密度バー
- change volume の可視化
- crowded region の warning marker

---

## 3. Measurement Contract

Phase 1 では、矩形グリッドで十分に価値が出る。

```text
VisualDensitySample
├─ regionId
├─ rect
├─ visualDensity
├─ informationDensity
├─ luminanceDensity
├─ motionDensity
├─ severity
└─ note
```

### 3-1. Suggested Inputs

- visible layer bounds
- selected layer bounds
- text layer count
- mask / matte / overlay annotations
- local brightness estimate
- frame delta estimate

### 3-2. Suggested Output

- `Low / Medium / High`
- density score
- warning reason
- next action hint

---

## 4. Phase 1 Behavior

Phase 1 は「診断」だけを先に作る。

- density score を出す
- heatmap を出す
- warning を出す
- どこを休ませるべきかの候補を 1 つ返す

まだやらないこと:

- 完全自動のレイアウト最適化
- 画面構成の強制変更
- 高コストなピクセル全域解析
- permanent warning の大量固定

---

## 5. Success Criteria

- 画面が詰まりすぎているかを短時間で読める
- visual / info / luminance / motion の 4 軸が混ざらない
- overlay と debugger の語彙が一致する
- 「ここを少し空ける」が提案として返る

---

## 6. Related Docs

- [`MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`](./MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md)
- [`MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md`](./MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md)

