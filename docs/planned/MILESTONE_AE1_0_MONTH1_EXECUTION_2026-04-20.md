# MILESTONE: AE 1.0 Month 1 Execution

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md) の Month 1 を、実装順がぶれない粒度に落とす。

Month 1 のゴールは「AE 1.0 の骨格を壊さずに固めること」であり、見た目の派手さよりも契約と同期の安定化を優先する。

---

## Month 1 の範囲

### 1-1 Host / Context / ROI / Property Core

目的:

- render context / ROI / property registry をホスト共通の契約に寄せる
- effect / layer / queue が同じ context 構造を読めるようにする

対象:

- [`Artifact/include/Render/ArtifactRenderContext.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderContext.ixx)
- [`Artifact/include/Render/ArtifactRenderROI.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderROI.ixx)
- [`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactCompositionViewDrawing.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`ArtifactCore/include/Property/AbstractProperty.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/AbstractProperty.ixx)
- [`ArtifactCore/include/Property/PropertyGroup.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/PropertyGroup.ixx)

完了条件:

- preview / export / proxy の context 生成が共通化される
- property が UI なしでもたどれる状態になる
- 既存の描画結果は変わらない

---

### 1-2 Timeline / Selection / Service Boundary Hardening

目的:

- selection の二重経路を減らす
- `NoLayer` になった理由を追えるようにする
- `*Service` 以外からの直参照を減らす

対象:

- [`Artifact/src/AppMain.cppm`](X:/Dev/ArtifactStudio/Artifact/src/AppMain.cppm)
- [`Artifact/src/Widgets/ArtifactInspectorWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactInspectorWidget.cppm)
- [`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactPropertyWidget.cppm)
- [`Artifact/src/Service/ArtifactProjectService.cpp`](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactProjectService.cpp)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)

完了条件:

- layer selection の理由がログで読める
- property 編集中に current layer が落ちにくい
- service boundary の責務が分かる

---

### 1-3 Render Queue End-to-End

目的:

- キュー登録から完了/失敗表示まで一続きにする
- export / dummy output / recovery を安定化する

対象:

- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`Artifact/src/Widgets/Render/ArtifactRenderQueueWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactRenderQueueWidget.cppm)
- [`Artifact/src/Widgets/ArtifactMainWindow.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactMainWindow.cppm)
- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)

完了条件:

- job の登録名が揃う
- 失敗理由が UI とログの両方で読める
- render queue が v1.0 の運用導線として成立する

---

## 実装順

1. Host / Context / ROI / Property Core
2. Timeline / Selection / Service Boundary Hardening
3. Render Queue End-to-End

---

## 変更しないこと

- 既存の描画アルゴリズム
- 既存のレイヤー実装
- 既存の UI 配置
- 既存の render queue の job model の意味

---

## リスク

- context / property の意味を先に変えると、preview/export の差が出る可能性がある
- selection の同期を雑にまとめると、Inspector と PropertyWidget が再びずれる
- render queue の job 表示だけ先に変えると、失敗診断が曖昧になる

---

## 次の Month への橋渡し

Month 1 が終わると、Month 2 で track matte / mask / shape core を入れやすくなる。
また、Month 3 の easing / motion blur に必要な property / context の基盤も揃いやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md)
- [`Artifact/docs/MILESTONE_V1_0_PRODUCTION_READINESS_2026-03-11.md`](X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_V1_0_PRODUCTION_READINESS_2026-03-11.md)
