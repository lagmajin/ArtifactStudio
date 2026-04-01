# UI EventBus Adoption Milestone

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
