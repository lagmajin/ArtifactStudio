# Phase 4 実行メモ: Dependency Declaration

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md) の Phase 4 を、実評価経路を壊さずに進めるための実行メモ。

この段階から「何を描くか」だけでなく、「何が必要か」を host に先に伝える。

---

## 方針

1. dependency declaration は先に型で固定する
2. 実レンダリングの順序変更は最小にする
3. layer / effect / precomp の依存を同じ表現に寄せる
4. UI 側の見え方は後から追従させる

---

## 現状の土台

- [`Artifact/include/Effetcs/ArtifactAbstractEffect.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Effetcs/ArtifactAbstractEffect.ixx)
- [`Artifact/include/Effetcs/EffectContext.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Effetcs/EffectContext.ixx)
- [`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactCompositionViewDrawing.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`Artifact/src/Layer/ArtifactAbstractLayer.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactAbstractLayer.cppm)

---

## 実装タスク

### 1. 宣言型の追加

追加候補:

- `EffectDependencyDescriptor`
- `DependencyGraphNode`
- `RequestedInputSurface`
- `RequestedChannelSet`

責務:

- effect が必要入力を先に述べる
- host が upstream / mask / matte / precomp を先に把握する
- render 前に依存関係を安定させる

### 2. layer / effect / precomp の依存を共通化する

やること:

- layer の upstream 依存を宣言する
- effect の入力 surface を宣言する
- precomp の外部依存を宣言する

### 3. host が評価順を組めるようにする

やること:

- dependency 宣言から簡易 graph を作る
- render queue と preview の両方で同じ graph を参照する
- cycle / missing dependency の診断を読めるようにする

### 4. 既存経路は adapter 経由で残す

やること:

- 旧来の direct call path をすぐには消さない
- declaration が無いものは legacy として動かす
- 新規 effect だけ先に宣言型へ寄せられるようにする

---

## 実装順

1. `EffectDependencyDescriptor` / `DependencyGraphNode`
2. layer / effect / precomp の宣言型
3. host 側 graph 組み立て
4. legacy path の adapter 維持

---

## 完了条件

- render 前に必要入力が分かる
- cycle / missing dependency を診断できる
- legacy effect が壊れない

---

## 変更しないこと

- 既存 effect の一括書き換え
- レンダリングアルゴリズムの全面変更
- UI の大改修

---

## リスク

- 宣言が実評価より先に走りすぎると、今の挙動との差が見えにくくなる
- dependency graph を一気に厳格化すると、既存 effect の互換が切れる

---

## 次の Phase への橋渡し

Phase 4 が終わると、Phase 5 で ROI metadata / partial invalidation を依存グラフに自然につなげやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE3_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE3_EXECUTION_2026-04-20.md)
