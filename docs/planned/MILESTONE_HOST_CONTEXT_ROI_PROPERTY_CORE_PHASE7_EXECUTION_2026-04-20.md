# Phase 7 実行メモ: Network / Script / Headless Integration

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md) の Phase 7 を、UI 外の入口へ同じ contract を流すための実行メモ。

この段階で初めて、property / context / host contract を NetworkRPCServer や script / headless 側から使えるようにする。

---

## 方針

1. UI 先行の contract をそのまま外へ開く
2. CLI / network / script で同じ型を使う
3. headless render でも context を共通化する
4. 外部入口は read-only から始める

---

## 現状の土台

- [`ArtifactCore/NetworkRPCServer.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/NetworkRPCServer.ixx)
- [`ArtifactCore/src/Network/NetworkRPCServer.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Network/NetworkRPCServer.cppm)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE2_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE2_EXECUTION_2026-04-20.md)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE1_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE1_EXECUTION_2026-04-20.md)

---

## 実装タスク

### 1. property registry を外へ出す

やること:

- RPC から property を path / ID で参照できるようにする
- UI なしで current value / keyframe / expression を読めるようにする
- read-only query を先に公開する

### 2. render context を headless で共通化する

やること:

- CLI render でも同じ `RenderContext` / snapshot を使う
- preview / export / proxy と headless の差を契約で埋める
- interactive でない用途を明示する

### 3. script entry を contract ベースにする

やること:

- script から layer / composition / property / render job を触れるようにする
- UI 依存の state を直接触らない
- command / query を分ける

### 4. plugin / remote evaluation への橋にする

やること:

- future plugin sandbox が同じ contract を使えるようにする
- remote evaluation でも host contract を再利用できるようにする
- まずは local / read-only から始める

---

## 実装順

1. property registry の外部参照
2. headless render context の共通化
3. script entry の contract 化
4. plugin / remote evaluation の橋渡し

---

## 完了条件

- UI なしで property 操作と render kickoff が可能
- remote / local / interactive で同じ contract を使える
- 既存の UI 経路が壊れない

---

## 変更しないこと

- UI の使い勝手
- 既存 script / network の動作を急に壊すこと
- contract の read-only 以外の権限拡張

---

## リスク

- 外部公開を急ぐと、内部 contract の未整理が露呈する
- read-only で始めないと、UI 由来の副作用が外にも漏れる
- headless / network / script を同時に変えると切り分けが難しい

---

## 関連文書

- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE6_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE6_EXECUTION_2026-04-20.md)
