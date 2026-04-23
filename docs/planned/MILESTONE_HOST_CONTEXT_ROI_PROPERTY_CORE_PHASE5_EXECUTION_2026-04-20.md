# Phase 5 実行メモ: ROI Metadata and Partial Invalidation

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md) の Phase 5 を、full-frame 前提のままでも入れやすい形で進めるための実行メモ。

この段階では tile renderer に飛ばず、まず ROI と dirty region の metadata を正確に持つ。

---

## 方針

1. 先に metadata を足す
2. 挙動変更は最小にする
3. dirty region は accumulator として扱う
4. ROI hint を effect / layer / precomp で共通化する

---

## 現状の土台

- [`Artifact/include/Render/ArtifactRenderROI.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderROI.ixx)
- [`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactCompositionViewDrawing.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`Artifact/src/Layer/ArtifactAbstractLayer.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactAbstractLayer.cppm)

---

## 実装タスク

### 1. ROI metadata を追加する

追加候補:

- `EffectROIHint`
- `LayerInvalidationRegion`
- `DirtyRegionAccumulator`
- `RenderDamageTracker`

責務:

- どこが変わったかを領域で記録する
- effect が必要とする拡張 ROI を表す
- full-frame 再描画と部分再描画の差を metadata で表現する

### 2. invalidation を集約する

やること:

- layer 変更時の dirty rect を積む
- composition / precomp / effect の変更を同じ tracker に通す
- UI repaint と render invalidation を分けて記録する

### 3. ROI hint を effect / layer / precomp に通す

やること:

- blur / glow / matte / mask などの ROI 拡張を宣言する
- upstream から downstream への必要範囲を伝える
- render queue / preview で同じ metadata を参照する

### 4. まだ tile にはしない

やること:

- full-frame path を残す
- tile surface は次の Phase で扱う
- この Phase は metadata の整備に留める

---

## 実装順

1. `EffectROIHint` / `LayerInvalidationRegion`
2. `DirtyRegionAccumulator` / `RenderDamageTracker`
3. ROI hint の宣言と接続
4. full-frame path のままメタデータ利用

---

## 完了条件

- 変更範囲を metadata として追跡できる
- effect / layer / precomp の ROI を共通化できる
- 既存の full-frame 描画結果が変わらない

---

## 変更しないこと

- tile-backed surface の全面導入
- render アルゴリズムの全面変更
- UI の見た目変更

---

## リスク

- invalidation を雑に集約すると、再描画漏れか過剰再描画が起きる
- ROI hint を曖昧にすると、次の tile phase で効果が薄くなる

---

## 次の Phase への橋渡し

Phase 5 が終わると、Phase 6 で tile-backed surface を導入しやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE4_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE4_EXECUTION_2026-04-20.md)
