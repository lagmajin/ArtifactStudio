# EventBus 流量調査レポート（2026-04-12）

**日付**: 2026-04-12  
**対象**: Qt シグナルスロットから内部イベントバス形式に移行した後のパフォーマンス調査  
**結論**: EventBus 自体のオーバーヘッドより大きい問題は「同じ操作で複数イベントが発火し、購読者ごとに重たい処理が何度も走る」構造

---

## 1. EventBus アーキテクチャの問題点

### 発見事項 A: 二重発火パターンが残存

以前 `docs/bugs/EVENTBUS_CASCADE_REDRAW_PERF_2026-04-05.md` で修正が報告されたが、以下のパターンが残っている可能性：

```cpp
// 問題のパターン - EventBus publish と Qt signal が両方走る
connect(&impl_->projectManager(), &ArtifactProjectManager::projectChanged, this, [this]() {
    globalEventBus().publish<ProjectChangedEvent>(...);  // EventBus
    projectChanged();                                     // Qt signal も emit
});
```

**影響**: 通知が2経路で配送され購読者ごとに重複処理が発生

---

### 発見事項 B: post() vs publish() の混在

コードベース内で2つの配信方式が混在：

| 方式 | メソッド | 動作 |
|------|---------|------|
| 同期即時 | [`publish()`](ArtifactCore/include/Event/EventBus.ixx:86) |  registry から購読者リストをコピーして全コールバックを同期実行 |
| キューイング | [`post()`](ArtifactCore/include/Event/EventBus.ixx:91) | 優先度順ソートでキューに入れ、[`drain()`](ArtifactCore/src/Event/EventBus.cppm:234) で取り出す |

**問題**: `publish()` はロック 획득量大、`post()` はキュー管理コスト発生

---

### 発見事項 C: publishRaw() の実装

[`ArtifactCore/src/Event/EventBus.cppm:156-188`](ArtifactCore/src/Event/EventBus.cppm:156)

```cpp
std::size_t EventBus::publishRaw(std::type_index type, const void* payload) const {
    // registryMutex ロック
    // 購読者リストをコピー（snapshot）
    // 各購読者に対して callback を同期実行
    // → ロック竞争中、N 回コールバック実行
}
```

**ボトルネック**:
- registryMutex のロック時間が長い
- 購読者数に比例して処理時間増加
- callback 実行中に別の publish がブロック

---

### 発見事項 D: enqueueRaw() + drain() の実装

[`ArtifactCore/src/Event/EventBus.cppm:191-205`](ArtifactCore/src/Event/EventBus.cppm:191)

```cpp
void EventBus::enqueueRaw(...) {
    std::lock_guard<std::mutex> lock(impl_->queueMutex);
    // 優先度順で挿入位置決定（O(n)）
    impl_->queue.insert(pos, QueuedEvent{...});
}
```

[`ArtifactCore/src/Event/EventBus.cppm:234-266`](ArtifactCore/src/Event/EventBus.cppm:234)

```cpp
std::size_t EventBus::drain(std::size_t maxEvents) {
    // キューから batch 取り出し
    // 各 QueuedEvent の dispatch() を実行
    // → dispatch が publishRaw を呼ぶため再帰的可能性
}
```

---

## 2. イベント流量のボトルネック

### 高頻度イベント種

| イベント | 発火頻度 | 購読者数 | 問題度 |
|----------|---------|---------|-------|
| `LayerChangedEvent` | レイヤー操作常に発火 | 5+ | ★★★ |
| `ProjectChangedEvent` | プロジェクト変更時 | 10+ | ★★★ |
| `LayerSelectionChangedEvent` | 選択変更時 | 3+ | ★★ |

---

### 問題となる操作のイベント発火例

**レイヤー追加時の多重発火**:
```
addLayer()
  └─ globalEventBus().publish(LayerChangedEvent{Created})  // ArtifactAbstractComposition.cppm:251
       └─ ArtifactTimelineWidget::refreshTracks()
       └─ ArtifactPropertyWidget::update()
       └─ ArtifactCompositionRenderController::invalidateCache()
  └─ notifyProjectMutation() → ProjectChangedEvent
       └─ ArtifactTimelineWidget::refreshTracks()  ← 重複2回目
       └─ ArtifactLayerPanelWidget::updateLayout()
  └─ selectLayer() → LayerSelectionChangedEvent
       └─ ArtifactTimelineWidget::updateSelectionState()
```

→ レイヤー1つ追加で **4-5 イベント発火**、購読者ごとに **refreshTracks() 等が複数回実行**

---

## 3. キャッシュ戦略の欠如

[`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm) では：

```cpp
// LayerChangedEvent 購読時
globalEventBus().subscribe<LayerChangedEvent>([this](const LayerChangedEvent& ev) {
    invalidateBaseComposite();  // 全部破棄
    requestRender();
});
```

- 変更されたレイヤーだけが対象なのに全キャッシュ破棄
- フレーム範囲の部分的無効化がない

---

## 4. drain() 呼び出しの散在

[`drain()`](ArtifactCore/src/Event/EventBus.cppm:234) の呼び出し箇所：

| ファイル | 箇所 | 問題 |
|---------|------|------|
| `ArtifactProjectManagerWidget.cppm` | 3389, 3437, 3447, 3453, 3461, 3469, 3478, 3603 | -widget 内で毎操作 `drain()` |
| `ArtifactCompositionGraphWidget.cppm` | 275, 279, 283, 287, 291 | 同上 |
| `ReactiveEventEditorWindow.cppm` | 816, 822, 828, 834, 849 | 同上 |

**問題**: UI スレッドで同期処理されており、`drain()` 中に大量の callback が実行される

---

## 5. 推奨される改善案

### 即座に実施可能な対策（★★★）

| 対策 | 対象 | 期待効果 |
|------|------|---------|
| `publish()` を `post()` に統一 | `LayerChangedEvent` 発火箇所 | キューイングによるバースト制御 |
| `drain()` の呼び出し箇所統制 | `AppMain.cppm` | 非UIスレッドでの `drain()` 除去 |
| 類似イベント統合（Debounce） | `refreshTracks()` | 1フレーム内の重複処理解消 |
| Qt signal と EventBus の二重発火 제거 | `ArtifactProjectService.cpp` | 重複通知の消除 |

### 中期的に実施すべき対策（★★）

| 対策 | 対象 | 期待効果 |
|------|------|---------|
| イベントカテゴリ分级 | `EventBus.ixx` | High/Normal/Low 優先度で処理制御 |
| 購読者数の削減 | 全体 | 不要な購読を disconnect |
| 差分更新の導入 | `LayerChangedEvent` | 変更プロパティのみ通知 |
| 部分的キャッシュ無効化 | `CompositionRenderController` | 変更レイヤーだけが無効化 |

---

## 6. 検証方法

流量を確認するには `ArtifactCore/src/Event/EventBus.cppm` に以下を追加：

```cpp
// publishRaw() 冒頭にログ追加
std::size_t EventBus::publishRaw(std::type_index type, const void* payload) const {
    qDebug() << "[EventBus.publish]" << type.name() << "subscribers:" << subscriberCountRaw(type);
    // ... existing code
}

// enqueueRaw() 冒단에ログ追加  
void EventBus::enqueueRaw(std::type_index type, std::shared_ptr<const void> payload, 
                          void (*dispatch)(EventBus&, const void*), EventPriority priority) {
    qDebug() << "[EventBus.enqueue]" << type.name() << "priority:" << (int)priority 
             << "queue:" << impl_->queue.size();
    // ... existing code
}
```

これにより実際の流量が可視化されます。

---

## 7. 関連ドキュメント

- `docs/bugs/EVENT_BUS_MIGRATION_STATUS_2026-04-05.md` - EventBus 移行状況
- `docs/bugs/EVENTBUS_CASCADE_REDRAW_PERF_2026-04-05.md` - 過剰再描画問題
- `ArtifactCore/include/Event/EventBus.ixx` - EventBus 定義
- `ArtifactCore/src/Event/EventBus.cppm` - EventBus 実装

---

## 8. 結論

**EventBus 自体のオーバーヘッドより大きい問題は「同じ操作で複数イベントが発火し、購読者ごとに重たい処理が何度も走る」構造。**

流量抑制よりも：
1. イベント発火数の削減（多重発火の除去）
2. 購読者側の処理最適化（debounce、部分更新）
3. `post()` 統一によるキューイング制御

の3点が効果大きいです。