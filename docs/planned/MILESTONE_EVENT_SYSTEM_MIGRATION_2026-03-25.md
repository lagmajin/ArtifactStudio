# マイルストーン: Qt Signal/Slot から Core Event System への段階移行

> 2026-03-25 作成

## 目的

Qt の `signal/slot` を完全に捨てるのではなく、アプリの中核状態更新を `ArtifactCore` 側のイベントシステムへ寄せる。

狙いは次の 3 つ。

1. 依存関係を薄くする
2. UI と Core の責務を分ける
3. 状態変化の fan-out を制御しやすくする

---

## 現状の問題

今は多くの変化が Qt signal を起点に広がっている。

- `frameChanged`
- `projectChanged`
- `layerCreated`
- `selectionChanged`
- `currentCompositionChanged`
- `propertyChanged`

この構造は分かりやすい一方で、次の問題がある。

- UI widget が service に強く依存する
- 1 回の操作で複数 widget が同時に更新される
- redraw / relayout / selection sync の責務が混ざる
- Core の変更と UI の反映が密結合になる

---

## 方針

### 原則

Core は Qt を知らない。

イベントの正は `ArtifactCore` に置き、Qt は UI 境界でのアダプタに限定する。

### 推奨レイヤー

1. `ArtifactCore Event Bus`
2. `Application Service` のイベント購読
3. `Qt Widget` はサービスや bus を読むだけ

Qt の `signal/slot` は当面次の用途だけ残す。

- widget 内部の UI 状態伝播
- Qt のライフサイクルと結び付くイベント
- 既存コードの互換アダプタ

---

## 移行対象

### Phase 1: Core イベントの土台

追加したいもの:

- `EventBus` または `EventDispatcher`
- `EventType`
- `EventPayload`
- `Subscription` / `ConnectionToken`

最低限のイベント例:

- `ProjectChanged`
- `CompositionChanged`
- `LayerAdded`
- `LayerRemoved`
- `LayerSelected`
- `FrameChanged`
- `PlaybackStateChanged`
- `PropertyChanged`
- `AudioStateChanged`

### Phase 2: Service 層の購読化

`ArtifactProjectService` / `ArtifactPlaybackService` / `ArtifactApplicationManager` は、
Qt signal の発火元ではなく、Core event を受けて必要な Qt signal を出すアダプタになる。

### Phase 3: UI 参照の縮小

widget は service の `signal` を直接多数つなぐのではなく、
必要な event だけを購読する。

これにより、次の更新コストが下がる。

- playback scrub
- timeline selection sync
- composition viewer redraw
- inspector refresh

### Phase 4: 重要経路の直結

まずは以下の経路を event 化する。

- frame seek / playback tick
- layer add/remove/move
- property commit
- audio state change

その後に view 専用イベントを追加する。

---

## 置き換え優先順位

### 高優先

1. `frameChanged`
2. `projectChanged`
3. `layerCreated` / `layerRemoved`
4. `selectionChanged`
5. `propertyChanged`

### 中優先

1. `playbackStateChanged`
2. `currentCompositionChanged`
3. `audio` 系イベント

### 低優先

1. widget 内部の細かな UI 通知
2. 単発のダイアログ通知

---

## 設計上の注意

- event payload は Qt 型に依存しすぎない
- event は軽量にする
- UI 更新は coalesce できるようにする
- 既存の Qt signal を一気に消さず、段階的にアダプタ化する
- playback/timeline のような高頻度イベントは throttle / debounce 前提にする

---

## 期待効果

- Core ロジックの再利用性が上がる
- UI の大きな再描画を抑えやすくなる
- undo/redo や property keyframe と整合しやすくなる
- 将来の非 Qt UI や headless 実行にも繋ぎやすい

---

## 今回の方針

現段階ではまだ接続しない。

先にやることは以下。

1. EventBus の最小 API を設計する
2. `frameChanged` だけを実験的に event 化する
3. 既存 Qt signal はアダプタとして残す
4. 影響範囲が小さい widget から順に切り替える

---

## Next Step

最初の実装候補は `FrameChanged` と `SelectionChanged`。
この 2 つは更新頻度が高く、Qt signal の fan-out 問題が見えやすい。
