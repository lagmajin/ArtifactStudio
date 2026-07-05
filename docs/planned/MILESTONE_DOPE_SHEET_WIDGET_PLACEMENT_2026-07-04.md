# Milestone: Dope Sheet Widget Placement (2026-07-04)

## Goal

既存実装の責務と dock 構造を壊さずに、Dope Sheet をどこへ置くのが最も自然かを決める。

結論としては、`ArtifactTimelineWidget` に全面統合せず、`timeline-adjacent` な別 widget / 別 dock とする。
ただし keyframe source of truth は分離せず、既存 timeline と同じ `AbstractProperty` / `ArtifactTimelineKeyframeModel` を共有する。

---

## Existing Implementation Findings

### 1. `ArtifactTimelineWidget` はすでに重い orchestration shell

- `docs/analysis/WIDGET_GAP_ANALYSIS_2026-06-03.md` では `ArtifactTimelineWidget` は 5413 行級の中枢 widget と整理されている
- 実装上も `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` は
  - left pane
  - search/header
  - navigator
  - scrub bar
  - work area
  - right painter view
  - curve editor
  - selection / playback / sync
  を一括で束ねている
- `docs/analysis/REPORT_AE_GAP_AND_SIGNAL_HOTSPOT_2026-06-16.md` でも接続 hub として扱われている

つまり、ここへ Dope Sheet の表示語彙まで直接足すと、責務がさらに膨らみやすい。

### 2. 右ペインは `ArtifactTimelineTrackPainterView` が正規の layer track 編集面

- `docs/WIDGET_MAP.md` では `ArtifactTimelineTrackPainterView` は
  `right-side editing surface / track content / keyframe lane / waveform`
  の担当と定義されている
- 既存の planning docs でも、右ペインは clip / marker / playhead を扱う owner-draw surface として整理されている
- 現在の timeline は
  - layer rows
  - clip range
  - waveform
  - playhead
  - selected property keyframes
  を「layer track 上で読む」前提が強い

Dope Sheet の「全 property を時系列で密に読む面」とは、読み順と密度がかなり違う。

### 3. dock 構造は composition ごとに timeline surface を増やせる

- `Artifact/src/AppMain.cppm` では `CompositionCreatedEvent` ごとに
  `ArtifactTimelineWidget` を new して `ads::BottomDockWidgetArea` に dock 化している
- dock id も `timeline::<compositionId>` と composition 単位で整理されている
- `ArtifactMainWindow` は tabbed dock を扱える

このため、Dope Sheet も composition ごとの dock として増やす余地がある。
既存の main window / dock system と相性が良い。

### 4. 「timeline-adjacent の専用 surface」を置く前例がすでにある

- `docs/planned/MILESTONE_TIMING_EVENT_VIEW_2026-04-10.md` は
  `Build a lightweight timeline-adjacent widget` と明記している
- これは main timeline を肥大化させず、時間編集専用面を別 surface として置く方針の前例になる

Dope Sheet も同じ枠組みで考えるのが自然。

---

## Recommendation

### Recommended UI Shape

- `ArtifactTimelineWidget`
  - layer / clip / playhead / work area の既存責務を維持
- `ArtifactDopeSheetWidget`
  - keyframe 一覧と複数 property の時間編集に専念
- 両者は別 widget
- 両者は同じ composition context を共有
- 両者は同じ keyframe model / property source を共有

### Recommended Placement

- 既定は `ads::BottomDockWidgetArea`
- timeline と同じ composition 単位の dock
- 初期表示は timeline dock と tab group 化するのが第一候補

理由:

- 同じ「時間編集」文脈なので、left/right inspector より bottom area が自然
- timeline と切り替えながら使いやすい
- 既存の dock orchestration を流用しやすい
- timeline 本体の layout を壊さない

---

## Why Not Full Integration Into `ArtifactTimelineWidget`

- `ArtifactTimelineWidget` はすでに orchestration が重い
- 既存 right pane は layer-track 読解に最適化されている
- Dope Sheet は
  - property を跨いだ一覧性
  - 高密度 key 編集
  - 一括 offset / scale
  を主目的にするため、clip/track view とは操作語彙が異なる
- mode 切替を timeline 本体へ埋め込むと
  - header state
  - selection routing
  - keyboard focus
  - painter interaction
  の分岐が増えやすい

つまり、統合コストに対して得られる価値が小さい。

---

## Shared Contract

UI は分けるが、データは分けない。

- keyframe source of truth:
  - `ArtifactCore::AbstractProperty`
- timeline-facing model:
  - `ArtifactTimelineKeyframeModel`
- current turn で追加した Dope Sheet core:
  - keyframe collect
  - multi-key offset
  - multi-key scale

同期対象:

- current composition
- playhead frame
- visible frame range
- active layer / property context
- keyframe selection state

---

## Minimal Execution Plan

### Phase 1

- `ArtifactDopeSheetWidget` を新設
- composition を受け取って keyframe 一覧を描く
- playhead 表示のみ同期
- offset / scale を model API 経由で実行

### Phase 2

- `AppMain` で composition ごとの Dope Sheet dock を生成
- timeline dock と tab 化
- activate / focus / reopen policy を timeline と揃える

### Phase 3

- selection sync
- visible range sync
- jump between timeline and dope sheet

---

## Decision

この repo では、Dope Sheet は `ArtifactTimelineWidget` へ完全統合せず、
`timeline-adjacent` な別 widget / 別 dock として導入するのが最も安全で拡張しやすい。
