# Internal EventBus Migration Status

**Date**: 2026-04-05  
**Status**: Partial Implementation – Migration Incomplete  
**Owner**: Core Architecture Team

---

## Overview

ArtifactStudio 内部のイベント通知システムは、従来の **Qt シグナル/スロット** から、型安全な **EventBus** への移行が計画・部分的に進行中である。しかし、**レイヤーシステムおよびコンポジションエディタ関連コンポーネントは依然として Qt シグナルに依存**しており、完全移行は尚未来である。

---

## Current Event Notification Mechanisms

### 1. Qt Signals/Slots (Legacy)

- **Where used**: `ArtifactAbstractLayer::changed` を中心としたレイヤー変更通知
- **Implementation**: `notifyLayerMutation()` 内で `Q_EMIT changed()` を発行
- **Consumers**:
  - `CompositionRenderController` – レイヤーごとに `layer->changed` を購読し、キャッシュ無効化・再レンダリングを実行
  - `ArtifactPropertyWidget` – レイヤー選択時のプロパティ値更新に `changed` を購読
  - その他、タイムライン、再生制御など

### 2. EventBus (New System)

- **Module**: `ArtifactCore/include/Event/EventBus.ixx`
- **API**: `globalEventBus().subscribe<EventType>(callback)`, `post(event)`
- **Consumers identified** (File grep results):

| File | Usage |
|------|-------|
| `ArtifactTimelineLayerTestWidget.cppm` | `EventBus eventBus_ = globalEventBus()` |
| `ArtifactRenderQueueManagerWidget.cpp` | EventBus ローカルインスタンス |
| `ReactiveEventEditorWindow.cppm` | EventBus を使用 |
| `ArtifactProjectManagerWidget.cppm` | EventBus サブスクライブ/ポスト |
| `ArtifactInspectorWidget.cppm` | EventBus を使用 |
| `ArtifactCompositionGraphWidget.cppm` | EventBus を使用 |
| `ArtifactCompositionAudioMixerWidget.cppm` | EventBus を使用 |

**Note**: 上記の多くは **UI ウィジェット間のカスタムイベント**（例: プロジェクト変更、設定変更）に限定されており、**レイヤー自体の変更通知は EventBus を経由していない**。

---

## Gap Analysis

### Not Migrated to EventBus

| Component | Event Emission | Subscribers | Status |
|-----------|----------------|-------------|--------|
| `ArtifactAbstractLayer` | `changed()` (Qt) | `CompositionRenderController`, `ArtifactPropertyWidget` | ❌ Qt 信号のみ |
| `ArtifactAbstractComposition` | `changed()` (Qt) | Various | ❌ Qt 信号のみ |
| `ArtifactPlaybackService` | `frameChanged`, `playbackStateChanged` (Qt) | UI widgets | ❌ Qt 信号のみ |
| `ArtifactProjectService` | `projectChanged` (Qt) | PropertyWidget, etc. | ❌ Qt 信号のみ |

**Finding**: レイヤーシステム全体が **EventBus 未対応**。`notifyLayerMutation()` は依然として `Q_EMIT changed()` のみを発行しており、`EventBus::post<LayerChangedEvent>(...)` などのラッパーは存在しない。

### Why Migration Stalled

1. **No Layer-Specific Event Types**  
   `ArtifactCore::EventBus` は汎用的だが、`LayerChanged`, `LayerOpacityChanged`, `LayerTransformChanged` などの型定義が存在しない。

2. **Existing Code Works**  
   Qt シグナルスロットは既に十分に機能しており、変える必然性が低かった。

3. **Partial Adoption Creates Fragmentation**  
   ウィジェット間のカスタムイベントのみ EventBus を使うが、コアエンティティ (Layer, Composition) は旧方式のまま。これは混乱を招く。

4. **Migration Cost**  
   全レイヤー setter で `notifyLayerMutation()` を `EventBus::post()` にreplaceし、`CompositionRenderController` などのサブスクライバーを全て EventBus 購読に書き換える大規模作業が必要。

---

## Implications for Opacity Update Issue

Opacity 変更がコンポジットエディタに反映しない原因と、EventBus 移行状況の関連:

- **Signal path is intact**: `setOpacity()` → `notifyLayerMutation()` → `changed()` → `CompositionRenderController::renderOneFrame()` は接続されている
- **Thus, the problem is NOT missing signal**, 而是 **信号は出ているが描画更新が適用されない** という別の原因
- **However**, 一部コンポーネントが EventBus に移行し始めているため、もし `ArtifactAbstractLayer` 側だけが EventBus 化され、`CompositionRenderController` が Qt シグナル購読だけ的情况下では 通知が失われる可能性がある。
  - 現在は両方とも Qt シグナルなので問題なし
  - 将来的に Layer 側だけ EventBus 化すると、`CompositionRenderController` が見失う

---

## Recommended Migration Path

### Phase 1: Define Layer Event Types

```cpp
// ArtifactCore/include/Layer/LayerEvents.ixx
export module Layer.Events;

export namespace ArtifactCore {

struct LayerChanged {
    LayerID layerId;
    LayerDirtyFlag flags;
    LayerDirtyReason reason;
    FramePosition frame;  // optional
};

struct LayerPropertyChanged : LayerChanged {
    QString propertyName;
    QVariant newValue;
};

// etc.

}
```

### Phase 2: Enhance EventBus with Type Erasure for Variants

EventBus は既に `publishRaw/`subscribeRaw` を持つが、typed インターフェースが不十分。`Event<T>` ラッパーを追加するか、既存の `publish`/`subscribe` テンプレートを拡張する。

### Phase 3: Dual-Emission During Transition

`notifyLayerMutation()` を以下のように拡張:

```cpp
void notifyLayerMutation(ArtifactAbstractLayer *layer, LayerDirtyFlag flag, LayerDirtyReason reason) {
    layer->setDirty(flag);
    layer->addDirtyReason(reason);
    Q_EMIT layer->changed();  // legacy

    // New: also post to EventBus (with copied data to avoid lifetime issues)
    LayerChangedEvent ev{layer->id(), flag, reason, layer->currentFrame()};
    ArtifactCore::globalEventBus().post(ev);
}
```

こうすることで、サブスクライバーは Qt シグナルまたは EventBus のどちらでも購読可能になる。

### Phase 4: Migrate Major Subscribers

- `CompositionRenderController`: `layerChangedConnections_` を `EventBus::subscribe<LayerChanged>` に置き換え
- `ArtifactPropertyWidget`: `layer->changed` を `EventBus::subscribe<LayerPropertyChanged>` に (ただし property ごとのフィルタは EventBus で行う)
- `TimelineWidget`: キーフレーム更新通知を EventBus に移行

### Phase 5: Deprecate Qt Signals in Core

`ArtifactAbstractLayer::changed` を `[[deprecated]]` とし、新規コードでは EventBus のみを使用するようガイド。

---

## Risks of Partial Migration

1. **Dual-path bugs**: 同じ通知が 2 通りの方法で配送され、順序不同や重複处理后の一貫性問題が発生する可能性
2. **Performance**: EventBus が型情報とヒープ割り当てを追加する可能性、高频レイヤー更新 (drag, animation) で負荷増大
3. **Debugging difficulty**: 通知経路が 2 種類あると、どの経路が使われてるか追跡困難
4. **Missing subscribers**: もし Qt シグナル側だけ migrate して EventBus 購読者がいないと、通知が見落とされる

---

## Action Items

- [ ] Create `LayerEvents.ixx` with well-defined event structs
- [ ] Benchmark EventBus overhead vs Qt signals for per-frame layer updates
- [ ] Implement dual-emission in `notifyLayerMutation()` (behind feature flag if needed)
- [ ] Migrate `CompositionRenderController` to EventBus in a separate branch
- [ ] Update documentation: "All new component-to-component communication must use EventBus"
- [ ] Add deprecation warnings for direct `layer->changed` connections outside core

---

## Related Documents

- `ArtifactCore/docs/MILESTONE_INTERNAL_EVENT_SYSTEM_2026-03-24.md` (internal event system plan)
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md` (EventBus backlog)
- `ArtifactCore/include/Event/EventBus.ixx` (source)

---

**Conclusion**: The opacity update issue is unrelated to EventBus migration status, but the fragmented adoption (some widgets use EventBus, core layers still use Qt signals) indicates an incomplete architectural transition. Full migration requires significant effort and must be done carefully to avoid dropping notifications like the one suspected in the opacity bug.
