# UI EventBus Adoption Milestone

**最終更新:** 2026-08-15

## Update 2026-08-15

- `ArtifactCore::EventBus` は global bus、typed subscribe／publish／post／drain、subscription lifetime、debugger の fire／frequency／subscriber 統計を実装済み。
- Project Manager、Inspector、MainWindow、Project Model、Hierarchy、Asset Browser、Property Widget、Health／Problem／Graph／Audio Mixer などが Project／Composition／Layer／Selection／Frame 系イベントを購読している。Timeline も LayerChanged 等の publish と一部 EventBus subscription を持つため、前回の「次: Timeline」より移行が進んでいる。
- ただし Render Queue Manager、Playback Control、Composition Editor 全体の更新契約、Qt signal の fan-out 棚卸し、診断／recovery の共通通知化は未完了または未確認。Timeline の Project／Layer／Selection／Frame／WorkArea 系購読は queued dispatch と既存の selection refresh coalesce を持ち、CompositionChanged も widget 内の保留フラグで 1 tick に coalesce するようにした。他 widget の debounce policy は未統一。現状は Phase 1〜2 と Phase 4 の部分実装で、全面置換ではない。

> 2026-04-01 作成

`ArtifactCore::EventBus` を UI 層の広域更新に適用し、`Qt signal` と役割分担するためのマイルストーン。

このマイルストーンは「UI を EventBus に全面置換する」ものではない。

- 高頻度入力は Qt のまま残す
- project / composition / selection / queue / diagnostics の状態変化だけ bus に寄せる
- widget 内部の即時反応は従来どおり Qt signal を使う

## Current Progress

- Implemented: `ArtifactProjectManagerWidget`
- Implemented: `ArtifactInspectorWidget`
- In progress: `ArtifactRenderQueueManagerWidget`
- Next: `ArtifactTimelineWidget`

## Goal

- UI 更新の fan-out を見える化する
- project / timeline / inspector / render queue の再集約を bus で扱う
- debounce / coalesce を widget 単位で統一する
- UI thread の処理と state change を分ける

## First Targets

### P0

- `ArtifactProjectManagerWidget`
- `ArtifactInspectorWidget`
- `ArtifactTimelineWidget`

### P1

- `ArtifactRenderQueueManagerWidget`
- `ArtifactPlaybackControlWidget`
- `ArtifactAssetBrowser`

### P2

- `ArtifactCompositionEditor`
- `ArtifactProjectHealthDashboard`

## Events

- `ProjectChangedEvent`
- `CompositionChangedEvent`
- `CurrentCompositionChangedEvent`
- `LayerChangedEvent`
- `LayerSelectionChangedEvent`
- `SelectionChangedEvent`
- `FrameChangedEvent`
- `WorkAreaChangedEvent`
- `RenderQueueChangedEvent`
- `RenderQueueLogEvent`
- `ThumbnailUpdatedEvent`

## Phases

### Phase 1: Inventory

- 既存 `signal/slot` のうち fan-out が大きいものを洗い出す
- widget ごとに「即時反応」と「広域反映」を分ける

### Phase 2: Bridge

- service 層で Qt signal -> EventBus post を行う
- widget は bus subscription から再描画・再集計する

### Phase 3: Coalesce

- selection / project / thumbnail / queue を debounce する
- 連続更新を 1 tick にまとめる

### Phase 4: Expand

- timeline / inspector / composition editor の表示同期を bus 化する
- diagnostics / health / recovery を UI の共通通知へ寄せる

## Done Criteria

- 主要 widget が EventBus を購読している
- Qt signal は局所入力だけに減る
- 同じ更新が複数 widget に散るときの責務が明確になる
- UI 側の更新をログとマイルストーンで追える
