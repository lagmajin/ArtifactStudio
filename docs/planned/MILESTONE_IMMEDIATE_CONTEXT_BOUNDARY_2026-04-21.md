# M-IR-8 ImmediateContext Boundary / De-direct

`DiligentEngine` の `IDeviceContext` / `ImmediateContext` を、layer / widget / controller 側から直接触らない構造へ寄せるための更新段階。

## Goal

- `ImmediateContext` の使用を backend 内部へ閉じ込める
- `ArtifactIRenderer` / `RenderCommandBuffer` / `DiligentImmediateSubmitter` を正式な描画境界にする
- low-level submit / present / readback は残しつつ、上位レイヤーからの直叩きを段階的に減らす

## Scope

- `ArtifactIRenderer`
- `DiligentImmediateSubmitter`
- `PrimitiveRenderer2D` / `PrimitiveRenderer3D`
- `CompositionRenderController`
- preview / gizmo / overlay / diagnostics の描画入口

## Non-Goals

- `DiligentEngine` サブモジュール本体の改変
- backend を Qt-only 実装へ置き換えること
- `ImmediateContext` を repo から完全削除すること

## Why Now

- 現状の設計は、描画 packet 化の本流はかなり整っている一方で、`IDeviceContext` 取得口や低レベル API の露出がまだ残っている
- 今後の `Frame Debug View` / `Pipeline View` / GPU effect parity / ROI / render queue 改善を進める前に、renderer boundary を揃えた方が事故が減る
- DX12 / Vulkan backend の読み違いを防ぐためにも、上位コードが low-level context に触れない形へ寄せたい

## Current Understanding

- 主流の 2D 描画はすでに `RenderCommandBuffer` → `DiligentImmediateSubmitter` へ寄っている
- 2026-04-28 時点で、`ParticleRenderer` と `PrimitiveRenderer3D` billboard も packet 化され、`RenderCommandBuffer` 順序内で submit される
- 一方で `ArtifactIRenderer` は `IDeviceContext` を内部保持し、`present` / `flush` / `readback` / render target 制御は low-level path のまま
- したがって目標は「全廃」ではなく「外側からの直叩きを止めて内部へ隔離する」こと

## Phases

### Phase 1: Surface Audit / Boundary Freeze

- `IDeviceContext` / `ImmediateContext` へ触れている call site を棚卸しする
- 上位層が直接使ってよい API と、backend 内部へ押し込む API を分ける
- `ArtifactIRenderer` の maintenance rule に沿って、D3D12 path の保守境界を明文化する
- 実行メモ: `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_PHASE1_EXECUTION_2026-04-21.md`

### Phase 2: Render Entry Consolidation

- layer / widget / controller 側の low-level draw を `ArtifactIRenderer` API へ寄せる
- overlay / gizmo / preview / diagnostics の描画入口を共通化する
- `PrimitiveRenderer2D / 3D` と `RenderCommandBuffer` 経路を優先する
- 実行メモ: `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_PHASE2_EXECUTION_2026-04-21.md`

### Phase 3: Context Access Narrowing

- `immediateContext()` のような low-level accessor を read-only diagnostics 向けと backend 用に分離する
- render target / readback / submit の責務を `ArtifactIRenderer::Impl` 側へ寄せる
- 上位コードから `IDeviceContext` 型が見えなくても進められる形へ寄せる
- 実行メモ: `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_PHASE3_EXECUTION_2026-04-21.md`

### Phase 4: Diagnostics / Debug Compatibility

- `Frame Debug View` / `Pipeline View` / `Trace` が必要とする情報を renderer summary API で取れるようにする
- 低レベル context を直接読むのではなく、attachment / resource / submit summary で読めるようにする
- 実行メモ: `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_PHASE4_EXECUTION_2026-04-21.md`

## Completion Criteria

- layer / widget / controller からの `ImmediateContext` 直叩きが実質なくなる
- `ArtifactIRenderer` が描画境界として一貫して使われる
- submit / present / readback / swapchain は backend 内部責務として残る
- debug / diagnostics も renderer summary API 経由で必要情報を取れる

## Target Files

- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/include/Render/DiligentImmediateSubmitter.ixx`
- `Artifact/src/Render/DiligentImmediateSubmitter.cppm`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Render/PrimitiveRenderer3D.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`

## Related

- `Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md`
- `docs/planned/ARTIFACT_IRENDERER_ANALYSIS_2026-04-17.md`
- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`
