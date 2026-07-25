# RAM Preview Cache Milestone

AE 風の RAM preview を、`render -> frame cache -> playback` の流れで安定して動かすための milestone.

## Goal

- 直近のフレーム列を RAM に積んで、再生時に即座に再生できるようにする
- render quality が高いフレームを先に cache し、再生は cache hit を優先する
- timeline scrub / range playback / loop playback と cache を連動させる
- cache 状態を UI から見えるようにする

## Scope

- `Artifact/src/Render/ArtifactFrameCache.cppm`
- `Artifact/include/Render/ArtifactFrameCache.ixx`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`

## Non-Goals

- 永続キャッシュをディスクに書くこと
- GPU texture cache の全面再設計
- 単独の video decoder 最適化だけで完結させること

## Background

現状のプロジェクトには preview / frame cache / playback の土台はあるが、
AE の RAM preview のように「一度貯めたフレームを連続再生し、キャッシュされた範囲を見ながら編集する」導線はまだ明示されていない。

この milestone では、`frame cache` を単なる最適化ではなく、
`playback の主要経路` として扱う。

## Proposed Model

- `CacheRange`
  - 現在の RAM preview で保持している frame range
- `CachePolicy`
  - `Ahead`, `Behind`, `AroundPlayhead`, `FullRange`
- `CacheQuality`
  - `Draft`, `Preview`, `Full`
- `CacheState`
  - `Empty`, `Preloading`, `Ready`, `Stale`, `Overfull`

## Phases

### Phase 1: Cache Range Model

- current playback range と cache range を分ける
- seek / scrub / play で cache の有効範囲を更新する
- cache miss 時の fallback を決める

### Phase 2: Prewarm / Fill

- play 開始時に先読みして cache を埋める
- `Ahead` と `AroundPlayhead` を優先する
- quality preset に応じて cache 生成コストを調整する

### Phase 3: Playback Integration

- cache hit のフレームを優先して再生する
- cache miss のときのみ再レンダリングする
- loop playback / in-out playback と連動する

### Phase 4: UI and Diagnostics

- cache range / hit rate / warm status を表示する
- RAM preview の開始 / 停止 / クリアを UI から操作できるようにする
- dropped frame / cache stall / stale cache を診断できるようにする

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4

## Current Status

- `ArtifactFrameCache` は既に存在する
- `PlaybackEngine` もある
- ただし RAM preview としての cache range / prewarm / playback priority はまだ独立した仕様になっていない

---

## Static audit follow-up (2026-07-25)

現行の `ArtifactPlaybackService`、`ArtifactRamPreviewController`、`ArtifactFrameCache`、Playback/Timeline 導線を照合した。ビルド・実機再生は未実施。

| Phase | 現状 | 判定 |
|---|---|---|
| 1. Cache Range Model | preview range、frame state、requested/ready/failed、work-area/loop 周辺の状態と frame bitmap がある。 | 実装済み／実行確認待ち |
| 2. Prewarm / Fill | priority build queue、playhead/near/directional/work-area の順序、キャンセル、generation invalidation がある。 | 実装済み／実測待ち |
| 3. Playback Integration | final composition frame の ready 判定、RAM LRU、disk hydrate、playback fallback の基盤がある。 | 部分実装／ready range の安定再生確認待ち |
| 4. UI / Diagnostics | cache bar、hit rate、cached count、failure reason、clear/build/stop のサービス導線がある。 | 部分実装／全 widget の同一状態表示確認待ち |

### 現在の判定

当初の「range/prewarm/priority が独立仕様になっていない」という記載は現行コードでは更新が必要。Phase 1〜2 はコード上進展しており、残る中心課題は Phase 3〜4 の実行経路統合・再生保証・診断表示である。

### 追加確認

`ArtifactFrameCache` 自体には LRU、メモリ／フレーム上限、generation invalidation、prefetch、hit/miss 統計が実装されている。一方、`ArtifactPlaybackService.cppm` では `std::unique_ptr<FrameCache> frameCache_` がコメントアウトされ、「FrameCache module is disabled」と明記されている。現行の RAM preview はサービス内の frame-state／priority／fallback 管理が中心で、汎用 `FrameCache` の hit 優先再生経路へ接続済みとは断定できない。この点を踏まえ、Phase 3 は「部分実装」よりも、FrameCache 接続と実再生の検証を必須残課題として扱う。
