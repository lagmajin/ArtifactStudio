# 不具合調査記録: Diligent ビューア — レイヤー非表示＆不透明度が連動しない

## 報告

メインウィンドウ中央のレイヤービューパネル（Diligent バックエンド `CompositionViewport`）の描画が、プロパティパネルで変更したレイヤーの **非表示 (visible)** と **不透明度 (opacity)** に連動しない。

---

## 調査で追跡したシグナルチェーン

```
PropertyWidget / LayerPanel
  → layer->setLayerPropertyValue("layer.visible"|"layer.opacity", value)
    → setVisible() / setOpacity()
      → notifyLayerMutation()
        → Q_EMIT changed()
          → CompositionRenderController::renderOneFrame()  [シグナル接続]
            → layer->isVisible() チェック → layer->draw(renderer)
              → renderer->drawSolidRect / drawSprite  (ここで opacity を使う必要がある)
                → renderer->present()
```

---

## 特定した根本原因（5 件）

### 原因 1: `ArtifactSolidImageLayer::draw` — opacity 未使用 ★主因

**ファイル:** `Artifact/src/Layer/ArtifactSolidImageLayer.cppm` (line 79–86)

```cpp
// 修正前
renderer->drawSolidRect(pos, size, impl_->color_);
// → opacity パラメータ（第4引数、デフォルト 1.0f）を渡していない
```

**修正:** `this->opacity()` を第 4 引数として渡す。

### 原因 2: `ArtifactTextLayer::draw` — opacity 未使用 ★主因

**ファイル:** `Artifact/src/Layer/ArtifactTextLayer.cppm` (line 306)

```cpp
// 修正前
renderer->drawSprite(0, 0, w, h, impl_->renderedImage_);
// → opacity パラメータ（第6引数、デフォルト 1.0f）を渡していない
```

**修正:** `this->opacity()` を第 6 引数として渡す。

### 原因 3: `ArtifactVideoLayer::draw` — opacity 未使用 ★主因

**ファイル:** `Artifact/src/Layer/ArtifactVideoLayer.cppm` (line 603)

```cpp
// 修正前
renderer->drawSprite(0, 0, w, h, impl_->currentQImage_);
```

**補足:** `ArtifactVideoLayer_draw.patch` に修正済みの内容が存在していたが未適用だった。

**修正:** パッチ相当の `this->opacity()` 追加を直接適用。

### 原因 4: `setOpacity` — `changed()` の二重発火

**ファイル:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` (line 1041–1049)

```cpp
// 修正前
notifyLayerMutation(this, ...);  // ← 内部で Q_EMIT changed() を呼ぶ
Q_EMIT changed();                // ← 2回目の発火（冗長）
```

`notifyLayerMutation()` が既に `Q_EMIT layer->changed()` を行っているため、直後の `Q_EMIT changed()` は冗長で、`renderOneFrame()` が 1 回の opacity 変更で 2 回呼ばれていた。

**修正:** 冗長な `Q_EMIT changed()` を削除。

### 原因 5: 新規レイヤーのシグナル未接続

**ファイル:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

`setComposition()` は呼び出し時点で存在するレイヤーにのみ `changed → renderOneFrame` を接続する。`setComposition()` 後に追加されたレイヤーは `changed()` シグナルが未接続のため、プロパティ変更が描画に反映されない。

**修正:** コンストラクタで `ArtifactProjectService::layerCreated` シグナルを監視し、新規レイヤーにも自動接続する。

---

## 参考: 正しく opacity を渡していたレイヤー

| レイヤー | opacity 使用 | 備考 |
|---|---|---|
| `ArtifactImageLayer` | ✅ `this->opacity()` | `drawSprite` の第6引数 |
| `ArtifactSolid2DLayer` | ✅ `this->opacity()` | `drawSolidRect` の第4引数 |
| `ArtifactSolidImageLayer` | ❌ → ✅ 修正済み | |
| `ArtifactTextLayer` | ❌ → ✅ 修正済み | |
| `ArtifactVideoLayer` | ❌ → ✅ 修正済み | `.patch` 存在するも未適用だった |

---

## 変更ファイル一覧

| ファイル | 変更内容 |
|---|---|
| `Artifact/src/Layer/ArtifactSolidImageLayer.cppm` | `draw()` に `this->opacity()` を追加 |
| `Artifact/src/Layer/ArtifactTextLayer.cppm` | `draw()` に `this->opacity()` を追加 |
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | `draw()` に `this->opacity()` を追加 |
| `Artifact/src/Layer/ArtifactAbstractLayer.cppm` | `setOpacity` の二重 `changed()` 発火を修正 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 新規レイヤーの `changed` シグナル自動接続を追加 |

---

## 補足: visibility（非表示）について

`renderOneFrame()` は `layer->isVisible()` を正しくチェックしており、`setVisible()` → `notifyLayerMutation()` → `changed()` のシグナルチェーンも正常。既存レイヤーの visibility トグルは正しく動作する。問題は **原因 5**（新規レイヤー未接続）の場合のみ発生する。

## 残留リスク

- `ArtifactCompositionLayer` は `draw()` をオーバーライドしていない（pure virtual のまま）。プリコンポジションの描画が未実装の可能性。
- `ArtifactPreviewCompositionPipeline::render()` は `CompositionRenderController::renderOneFrame()` から呼ばれておらず、ロジックが重複している。将来的に統一推奨。
