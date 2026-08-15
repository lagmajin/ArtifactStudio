# Milestone: DX12/Vulkan Ray Tracing Integration

Date: 2026-05-16
**最終更新:** 2026-08-15

## Goal

Bring ray tracing into Artifact through Diligent's cross-backend API without forking
the renderer per backend. DX12 and Vulkan should share the Core contract, the
same acceleration-structure ownership model, and the same app-level diagnostics.

## Current State

- `DiligentDeviceManager` already requests `Features.RayTracing = ENABLED` for
  DX12 and Vulkan, then falls back to a non-RT device if creation fails.
- `ArtifactCore::RayTracingManager` owns the future Core boundary for BLAS/TLAS.
- `ArtifactIRenderer` exposes the Core manager to the app layer.
- The first integration slice records RT capability, backend, Diligent limits,
  unit-quad BLAS creation, and TLAS creation in render diagnostics.

## Implementation Slices

1. Capability and diagnostics contract
   - Core reports device type, feature state, supported flag, BLAS/TLAS creation,
     recursion depth, raygen/thread limits, TLAS/BLAS limits, and AS alignments.
   - App render path summary includes the RT debug state.

2. Real BLAS/TLAS build path
   - First slice builds a shared unit-quad BLAS and one-instance TLAS through
     Diligent using `BIND_RAY_TRACING` geometry, scratch, and instance buffers.
   - Replace the unit-quad placeholder with explicit geometry upload/build data.
   - Keep geometry input in Core buffers, not `QImage` or Qt-owned image memory.
   - Add dirty flags so layer transforms update TLAS without rebuilding BLAS.

3. Ray tracing pipeline state
   - First slice creates a minimal HLSL raygen/miss/closest-hit set, RT PSO,
     SBT, and a 1x1 warmup `TraceRays` dispatch when RT is supported.
   - The warmup shader binds the Core TLAS plus an `RGBA32_FLOAT` UAV output
     texture so it exercises the same resource path that a composition render
     pass will later use.
   - Core exposes the RT output SRV/UAV and `ArtifactIRenderer` exposes the SRV
     as `rayTracingOutputTextureView()` for the future composition merge path.
   - Add a minimal ray generation, miss, and closest-hit shader set.
   - Create SBT records through Diligent, not backend-specific native calls.
   - Output to a float render texture compatible with the composition pipeline.

4. App integration and fallback
   - Add debug/experimental toggles in the app layer.
   - Fall back to raster/composition paths when RT is unsupported, disabled, or
     missing AS resources.
   - Keep diagnostics visible enough to distinguish unsupported GPU/driver,
     disabled backend, AS creation failure, and dispatch failure.

5. Composition use cases
   - Start with shadows/visibility or intersection picking before full GI.
   - Later connect scene lights and material data for reflections/GI.

## Constraints

- Do not split DX12 and Vulkan logic unless Diligent cannot represent a feature.
- Do not add hot-path `QImage` conversions.
- Do not change DiligentEngine itself for this milestone.
- Keep each low-level slice small enough to build and bisect independently.

## Static Audit (2026-07-25)

Capability／diagnosticsの基盤は実装されている。`DiligentDeviceManager` は backend の RayTracing feature を確認し、設定に応じて DX12／Vulkan の device creation を試行する。`RayTracingManager` は Diligent の共通 API で capability、recursion／raygen／AS limits、alignment、unit-quad BLAS、TLAS、warmup RT PSO／SBT／出力 texture を管理し、`ArtifactIRenderer` は初期化・TLAS build・backend／AS状態を診断文字列へ出している。非対応時に manager 初期化をスキップする経路もある。

しかし、現状のBLASは unit quad のplaceholderで、`createOrUpdateBLAS` は layer geometry を利用せず、`buildTLAS` も layer transform の収集・更新を将来課題としている。Composition の実描画やshadow／picking／GIへの接続、debug toggle、dispatch failure と AS creation failure の粒度、DX12／Vulkan両backendでの実機動作は未確認。別系統の `GPURayTracer` も存在するが、今回の `IRayTracingManager`／composition 経路へ統合済みとは断定できない。よって Slice 1 は静的実装済み、Slice 2〜5 はwarmup／placeholder段階と実行検証待ちとして記録する。

確認対象:

- `ArtifactCore/src/Graphics/RayTracingManager.cppm`
- `ArtifactCore/include/Graphics/RayTracingManager.ixx`
- `ArtifactCore/src/Render/GPURayTracer.cppm`
- `Artifact/src/Render/DiligentDeviceManager.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/src/Render/Shaders/RayTrace.hlsl`

## 現行コード監査 (2026-08-15)

`RayTracingManager` は Diligent 共通 API で device capability、BLAS／TLAS、scratch／instance buffer、RT PSO／SBT、output texture、`TraceRays` dispatch を管理している。現行実装では `RTGeometryData` の vertex/index buffer を受ける BLAS 登録・build 経路と active instance の TLAS build も確認でき、旧監査の「unit-quad placeholder のみ」という判定は更新が必要である。

ただし、layer geometry／transform の composition 収集、実描画への shadow／picking／GI 接続、DX12／Vulkan 両 backend の実機確認、dispatch／AS failure の app diagnostics 粒度、raster fallback の受入れは未確認である。判定は **capability／AS／warmup の基盤実装済み、composition integration と runtime acceptance は pending** とする。

## Update 2026-08-15

- `RayTracingManager` の Diligent 共通 capability、`RTGeometryData` による BLAS 更新、active instance の TLAS、scratch／instance buffer、warmup RT PSO／SBT／`TraceRays` を再確認。
- `ArtifactIRenderer` は geometry change 時の BLAS／TLAS 更新と RT diagnostics を呼び出す経路まで持つため、旧「unit-quad placeholder のみ」という判定は更新。
- ただし layer geometry／transform の composition 収集、shadow／picking／GI の実利用、DX12／Vulkan 実機差、AS／dispatch failure の詳細 diagnostics、raster fallback の runtime 受入は未完了。
