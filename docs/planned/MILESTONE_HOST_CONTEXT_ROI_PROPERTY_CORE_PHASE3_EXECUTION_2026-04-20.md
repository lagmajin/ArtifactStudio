# Phase 3 実行メモ: Host-Plugin Contract Adapter

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md) の Phase 3 を、既存 effect 実装を壊さずに進めるための実行メモ。

この段階では direct buffer path を即廃止せず、まず host との契約だけを明文化する。

---

## 方針

1. 既存 effect は legacy path として残す
2. host-adapted path は adapter で並走させる
3. dependency declaration は型だけ先に足す
4. 実評価経路の全面切替は後段に回す

---

## 現状の土台

- [`Artifact/include/Effetcs/ArtifactAbstractEffect.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Effetcs/ArtifactAbstractEffect.ixx)
- [`Artifact/include/Effetcs/EffectContext.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Effetcs/EffectContext.ixx)
- [`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactCompositionViewDrawing.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)

---

## 実装タスク

### 1. Host contract の型を追加する

追加候補:

- `EffectHostContext`
- `EffectInputRequest`
- `EffectOutputSurface`
- `EffectCapabilityDescriptor`
- `EffectDependencyDescriptor`
- `IEffectHostAdapter`

責務:

- effect が何を要求するかを表現する
- host が何を渡したかを表現する
- direct buffer 以外の契約を置けるようにする

### 2. legacy / adapted の二層を作る

やること:

- `apply(src, dst)` の既存 effect を legacy path として包む
- 新規の adapter 経路を host-adapted path として追加する
- どちらも同じ render entry から呼べるようにする

### 3. dependency の宣言型だけ足す

やること:

- `DescribeDependencies(Time, ROI, Channels)` の型を先に作る
- 実装は空でもよい
- host 側が先に読むための契約だけを固定する

### 4. ROI / purpose を effect context に通す

やること:

- editor preview / export / proxy で effect に渡す context を共通化する
- effect が ROI を read-only で参照できるようにする
- 既存 effect 実装は当面そのまま動かす

---

## 実装順

1. Host contract の型を追加する
2. legacy / adapted の二層を作る
3. dependency の宣言型を足す
4. ROI / purpose を effect context に通す

---

## 完了条件

- 新規 effect が host contract 経由でも表現できる
- 旧 effect が adapter 経由でそのまま動く
- dependency declaration の型が安定する
- 実描画結果が変わらない

---

## 変更しないこと

- 既存 effect の実装を一気に書き換えること
- direct buffer path の即廃止
- 既存 preview / export の見た目
- UI から effect を触る導線

---

## リスク

- host contract を急ぎすぎると、effect 実装の互換性が崩れる
- dependency declaration を実評価に先行適用しすぎると、render order が変わる
- ROI の意味が曖昧だと、部分評価の結果が不安定になる

---

## 次の Phase への橋渡し

Phase 3 が終わると、Phase 4 で dependency declaration を実評価へ接続しやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE1_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE1_EXECUTION_2026-04-20.md)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE2_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE2_EXECUTION_2026-04-20.md)
