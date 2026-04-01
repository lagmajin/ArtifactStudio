# マイルストーン: EventBus Widget Migration Matrix

> 2026-04-01 作成

## 目的

`ArtifactCore::EventBus` を使って、複数 widget にまたがる状態変化の配信を段階的に切り替える。

ここでいう「切り替え」は Qt signal を全面廃止することではない。

- 高頻度入力は Qt signal のまま残す
- 状態変更の fan-out が大きい経路だけ EventBus に寄せる
- UI thread 固有の処理は Qt bridge で受け直す

---

## 切り替え判断

### EventBus に寄せる

- 1 回の変更で 2 つ以上の widget が再描画 / 再集計される
- background job の完了を複数の UI が見る
- project / composition / layer の状態変化を広域に配りたい
- debounce / coalesce したい
- 将来 headless や non-Qt 経路でも再利用したい

### Qt signal のまま残す

- mouse / drag / hover / scrub の生入力
- widget 内部だけで完結する UI state
- 即時の描画プレビュー
- context menu / shortcut / focus のローカル挙動

---

## 主要イベント群

### Core 状態

- `ProjectChangedEvent`
- `CompositionChangedEvent`
- `LayerChangedEvent`
- `SelectionChangedEvent`
- `FrameChangedEvent`
- `PlaybackStateChangedEvent`
- `PropertyChangedEvent`
- `WorkAreaChangedEvent`

### 補助イベント

- `ThumbnailUpdatedEvent`
- `RenderQueueChangedEvent`
- `HistoryEntryAppendedEvent`
- `BackgroundTaskCompletedEvent`
- `SearchResultChangedEvent`
- `KeyframeChangedEvent`
- `DiagnosticsStateChangedEvent`

---

## Widget 別マトリクス

### 1. `ArtifactProjectManagerWidget`

#### 受けるイベント

- `CompositionChangedEvent`
- `LayerChangedEvent`
- `CurrentCompositionChangedEvent`
- `SelectionChangedEvent`
- `ThumbnailUpdatedEvent`
- `ProjectChangedEvent`

#### 何を更新するか

- project tree の再集計
- thumbnail debounce の起動
- selection summary の更新
- missing asset / relink 状態の再表示

#### Qt signal のまま残すもの

- view 内の選択 UI
- search box の入力
- toolbar button の click

#### 切り替えメモ

- まずは `compositionCreated` / `layerCreated` / `layerRemoved` 相当を EventBus へ移す
- その後 `ThumbnailUpdatedEvent` で debounce を統一する

---

### 2. `ArtifactTimelineWidget`

#### 受けるイベント

- `CompositionChangedEvent`
- `LayerChangedEvent`
- `SelectionChangedEvent`
- `FrameChangedEvent`
- `PlaybackStateChangedEvent`
- `WorkAreaChangedEvent`
- `KeyframeChangedEvent`
- `SearchResultChangedEvent`
- `HistoryEntryAppendedEvent`

#### 何を更新するか

- right track view の clip / keyframe 再構築
- selection ハイライト
- work area の同期
- playhead の位置更新
- search result / keyframe navigation state の更新
- timeline history / log の反映

#### Qt signal のまま残すもの

- clip drag 中の位置変化
- scrub 中の即時追従
- wheel zoom / pan のローカル挙動

#### 切り替えメモ

- `refreshTracks()` を EventBus の購読先で叩けるようにする
- `frameChanged` は表示同期だけに限定し、操作起点は Qt のまま維持する

---

### 3. `ArtifactCompositionEditor`

#### 受けるイベント

- `SelectionChangedEvent`
- `LayerChangedEvent`
- `KeyframeChangedEvent`
- `WorkAreaChangedEvent`
- `CompositionChangedEvent`

#### 何を更新するか

- viewport overlay
- smart guide / snap state
- pivot / gizmo space 表示
- inline edit の状態復元
- layer inspector 的な選択要約

#### Qt signal のまま残すもの

- gizmo 直接操作
- mouse drag の途中状態
- context menu の command 実行

#### 切り替えメモ

- UI 更新は `SelectionChangedEvent` と `LayerChangedEvent` で coalesce する
- overlay 描画は EventBus の結果を読むだけに寄せる

---

### 4. `ArtifactInspectorWidget`

#### 受けるイベント

- `ProjectChangedEvent`
- `SelectionChangedEvent`
- `LayerSelectionChangedEvent`
- `PropertyChangedEvent`
- `LayerChangedEvent`
- `CompositionChangedEvent`

#### 何を更新するか

- property editor の再読込
- multi-selection summary
- editable state
- keyframe / expression / effect 情報の再表示

#### Qt signal のまま残すもの

- spinbox / slider / text edit の input
- local undo stack の push

#### 切り替えメモ

- まずは selection change だけ購読に変える
- 次に property commit を EventBus 経由で再読込する

---

### 5. `ArtifactRenderQueueManagerWidget`

#### 受けるイベント

- `RenderQueueChangedEvent`
- `RenderQueueLogEvent`
- `BackgroundTaskCompletedEvent`
- `CompositionChangedEvent`

#### 何を更新するか

- queue list
- progress / estimate / retry state
- history / log panel
- completed job の post action

#### Qt signal のまま残すもの

- job item の再順序付け
- manual cancel / retry の操作

#### 切り替えメモ

- queue service の内部状態変更を EventBus に流す
- UI は bus から履歴と状態を読むだけにする

---

### 6. `ArtifactPlaybackControlWidget`

#### 受けるイベント

- `FrameChangedEvent`
- `PlaybackStateChangedEvent`
- `CompositionChangedEvent`

#### 何を更新するか

- playhead 表示
- transport buttons の enable / disable
- current frame / fps / timecode

#### Qt signal のまま残すもの

- 直接の再生ボタン操作
- shuttle / jog / shortcut の入力

---

### 7. `ArtifactAssetBrowser`

#### 受けるイベント

- `ThumbnailUpdatedEvent`
- `ProjectChangedEvent`
- `CompositionChangedEvent`
- `LayerChangedEvent`

#### 何を更新するか

- thumbnail refresh
- missing / unused / dependency 表示
- proxy / cache status

#### Qt signal のまま残すもの

- tree view のローカル選択
- drag and drop の開始 / 終了

---

### 8. `ArtifactProjectHealthDashboard`

#### 受けるイベント

- `DiagnosticsStateChangedEvent`
- `ProjectChangedEvent`
- `CompositionChangedEvent`
- `HistoryEntryAppendedEvent`

#### 何を更新するか

- validation summary
- broken reference / missing asset alert
- recovery suggestion
- recent issue log

---

## 移行順

### Phase A: 配信型の基盤固定

1. `ArtifactCore` の `EventBus` を土台にする
2. `ArtifactEventTypes` を project / composition / layer 用に整理する
3. Qt bridge は service 層に閉じる

### Phase B: 広域更新の置換

1. `ArtifactProjectManagerWidget`  [done]
2. `ArtifactInspectorWidget`  [done]
3. `ArtifactRenderQueueManagerWidget`
4. `ArtifactTimelineWidget`

### Phase C: 表示系の共通化

1. `ArtifactPlaybackControlWidget`
2. `ArtifactAssetBrowser`
3. `ArtifactProjectHealthDashboard`
4. `ArtifactCompositionEditor`

### Phase D: 高頻度経路の絞り込み

1. `FrameChangedEvent` を表示同期に限定
2. `SelectionChangedEvent` を coalesce
3. `PropertyChangedEvent` を debounce

---

## 成功条件

- `projectChanged` のような広域通知が EventBus に乗る
- timeline / inspector / project manager が同じ変化を別々に購読できる
- 高頻度入力は Qt signal のままでも破綻しない
- UI の再描画が 1 箇所の変更で何度も走らない
- background job の完了が history / queue / notification に同時反映できる

---

## 実装メモ

- payload は可能なら `QString` / `LayerID` / `CompositionID` / 小さな enum に寄せる
- UI 専用オブジェクトや `QWidget*` を event payload に入れない
- queued delivery は `drain()` を明示的に呼ぶ側を決める
- まずは `ProjectManagerWidget` のパターンを踏襲する
- `EventBus::Subscription` は必ず `Impl` 側で保持する。破棄すると即 disconnect される
