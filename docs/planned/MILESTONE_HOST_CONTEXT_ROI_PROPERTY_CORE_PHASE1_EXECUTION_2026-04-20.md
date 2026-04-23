# Phase 1 実行メモ: Render Context Registry

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md) の Phase 1 を、既存動作を変えずに進めるための実行メモ。

この段階では `RenderContext` を新規設計し直さず、既存の型を「共通の文脈」として正式化する。

---

## 方針

1. 既存の `RenderContext` は残す
2. 新しい registry / snapshot / factory を追加する
3. preview / export / proxy の context 生成を 1 か所に寄せる
4. 既存の render path は当面 adapter 経由で読むだけにする

---

## 現状の土台

- [`Artifact/include/Render/ArtifactRenderContext.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderContext.ixx)
- [`Artifact/include/Render/ArtifactRenderROI.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderROI.ixx)
- [`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactCompositionViewDrawing.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)

---

## 実装タスク

### 1. Registry 型を追加する

追加候補:

- `RenderPurpose`
- `RenderContextSnapshot`
- `RenderContextRegistry`
- `RenderContextKey`

責務:

- context の「用途」を明示する
- interactive / preview / export / proxy を同じ入口で作る
- layer / effect / queue / thumbnail が同じ snapshot を参照できるようにする

### 2. 既存 `RenderContext` を snapshot に変換できるようにする

やること:

- `RenderContext` から `RenderContextSnapshot` を作る
- `RenderContextSnapshot` から `RenderContext` を復元する
- `resolutionScale / roi / interactive / colorDepth / colorSpace` を必須項目として固定する

### 3. context factory を集約する

候補ファイル:

- [`Artifact/include/Render/ArtifactRenderContext.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderContext.ixx)
- [`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactCompositionViewDrawing.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)

やること:

- preview 用 context 生成
- export 用 context 生成
- proxy / thumbnail 用 context 生成
- interactive かどうかの判定を共通化

### 4. read-only access API を足す

やること:

- layer / effect / queue 側が `RenderContextSnapshot` を受け取れるようにする
- まずは getter のみ
- mutating API は次の段階まで置かない

---

## 実装順

1. `Artifact/include/Render/ArtifactRenderContext.ixx` に snapshot / key / purpose を追加
2. `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` で preview 側の生成を factory 化
3. `Artifact/src/Render/ArtifactRenderQueueService.cppm` で export 側の生成を factory 化
4. 必要なら `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` を preview/editor 用の入口に合わせる

---

## 完了条件

- preview / export / proxy が同じ context 構造を使う
- `interactive` / `resolutionScale` / `roi` / `colorDepth` の意味が 1 つに揃う
- 既存の描画結果が変わらない

---

## 変更しないこと

- 既存の描画アルゴリズム
- 既存の layer / effect 実装
- 既存 UI の見た目と操作感
- ROI の意味そのもの

---

## リスク

- `RenderContext` の意味を変えると、preview と export の差が出る可能性がある
- 生成箇所が分散したままだと registry が形骸化する
- `interactive` の定義が曖昧なままだと用途分けが崩れる

---

## 次の Phase への橋渡し

Phase 1 が終わると、次の Phase 2 で property registry を同じく read-only で追加しやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md)
- [`docs/planned/MILESTONES_BACKLOG.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)
