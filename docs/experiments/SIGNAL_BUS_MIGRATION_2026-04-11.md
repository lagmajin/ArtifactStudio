# Qt シグナル → EventBus 移行レポート
**日付:** 2026-04-11

## 目的

`ArtifactStudio` 内のパフォーマンスボトルネックとなっていた Qt シグナル／スロット接続を
`ArtifactCore::EventBus` 経由の通知に置き換え、描画ループ中の余分な Qt ディスパッチコストを削減する。

---

## 移行した箇所

### 1. `notifyLayerMutation()` — `ArtifactAbstractLayer.cppm`

**変更前:**
```cpp
Q_EMIT layer->changed();
```

**変更後:**
```cpp
ArtifactCore::globalEventBus().publish(LayerChangedEvent{
    comp ? comp->id().toString() : QString{},
    layer->id().toString(),
    LayerChangedEvent::ChangeType::Modified});
Q_EMIT layer->changed();  // 未移行コンシューマ向けに残存
```

**効果:**  
全プロパティ変更（opacity, position, mask 編集など）が EventBus 経由で配信されるようになった。
レンダーコントローラの既存 `LayerChangedEvent` 購読が機能し、**プロパティ変更時のレンダリングが正しくトリガーされる**。

---

### 2. `ArtifactAbstractComposition.cppm` — 構造変更イベント

以下の操作に `LayerChangedEvent::Created` / `Removed` を追加:

| メソッド | イベント型 |
|---|---|
| `appendLayerTop` | `Created` |
| `appendLayerBottom` | `Created` |
| `removeLayer` | `Removed` |
| `removeAllLayers` | `Removed`（compositionId のみ、layerId は空文字） |
| `moveLayerToIndex` | `Modified` |
| `bringToFront` | `Modified` |
| `sendToBack` | `Modified` |

`Q_EMIT owner_->changed()` は非レイヤー系コンシューマ向けに維持。

---

### 3. `ArtifactCompositionRenderController.cppm` — 購読の強化

#### 3-a. `LayerChangedEvent` 購読アップグレード

**変更前:** `Modified` のみ処理（`Created`/`Removed` は未対応）

**変更後:**
- `Created` / `Removed` → `surfaceCache_.clear()` + `gpuTextureCacheManager_->clear()` + `applyCompositionState()` + `renderOneFrame()`
- `Modified` → 対象レイヤーの surface キャッシュ無効化 + `changeDetector_.markLayerChanged()` + `renderOneFrame()`
- `renderQueueActive_` 中はスキップ（キュー実行中の二重レンダー防止）

これにより `bindCompositionChanged` の Qt 接続と役割が重複しなくなった（Qt 接続は引き続き機能するが、
今後は `bindCompositionChanged` を段階的に削除できる）。

#### 3-b. 直接 `changed()` 呼び出しを EventBus publish に置き換え

| 行 | 操作 |
|---|---|
| 2321 | PenTool: パスクローズ時 |
| 2377 | PenTool: 頂点追加時 |
| 2601 | PenTool: 頂点ドラッグ時 |
| 2688 | Gizmo3D: トランスフォーム更新時 |

**変更前:** `selectedLayer->changed()` / `layer->changed()`  
**変更後:** `ArtifactCore::globalEventBus().publish(LayerChangedEvent{...})`

---

### 4. `ArtifactLayerPanelWidget.cpp` — 毎レイヤー Qt 接続を廃止

**変更前:**  
`updateLayout()` のたびに全レイヤーをループして `QObject::connect(layer->changed, update)` を登録・管理。
追加・削除のたびに接続を動的に追加／切断するオーバーヘッドがあった。

**変更後:**  
既存の `LayerChangedEvent` EventBus 購読に `Modified` 分岐を追加:
```cpp
} else if (event.changeType == LayerChangedEvent::ChangeType::Modified) {
    if (event.compositionId == impl_->compositionId.toString()) {
        update();  // 軽量再描画のみ（レイアウト再構築なし）
    }
}
```

`layerChangedConnections` ハッシュ、`clearLayerChangedSubscriptions()`、`refreshLayerChangedSubscriptions()` を削除。
`updateLayout()` から毎回呼んでいた O(N) の接続管理ループが不要になった。

---

## パフォーマンス改善の見込み

| 操作 | 変更前 | 変更後 |
|---|---|---|
| レイヤープロパティ変更（ドラッグ中） | 全レイヤー Qt 接続 → タイムラインパネル全体 `update()` (毎フレーム) | EventBus → 単一の `update()` |
| レイヤー追加 | Qt `changed()` → `updateLayout()` + 接続管理ループ | EventBus `Created` → `updateLayout()` のみ |
| マスク頂点ドラッグ | `Qt changed()` → タイムライン repaint + 空の Qt スロットループ | EventBus → レンダーコントローラの targeted invalidate |
| Gizmo3D ドラッグ | `Qt changed()` → 接続なしで空振り + タイムライン repaint | EventBus → レンダーコントローラが直接処理 |

---

## 残存する Qt 接続（意図的）

- `notifyLayerMutation()` 内の `Q_EMIT layer->changed()` → `ArtifactPropertyWidget`（プロパティパネル更新）と `ArtifactCompositionEditor`（テキストオーバーレイフィルタ同期）向け
- `ArtifactAbstractComposition` 内の `Q_EMIT owner_->changed()` → 非レイヤー系コンシューマ向け
- `bindCompositionChanged` Qt 接続 → 引き続き機能するが、EventBus 購読が上位を担うため今後削除候補

---

## 今後の課題

1. `bindCompositionChanged` の Qt 接続を削除し EventBus 購読に完全移行
2. `ArtifactPropertyWidget` の `layer->changed()` 購読を EventBus 化
3. `ArtifactCompositionEditor` のテキストオーバーレイ同期を EventBus 化
4. EventBus の `post()` (非同期キュー) を活用して UI スレッドのブロッキングをさらに削減
