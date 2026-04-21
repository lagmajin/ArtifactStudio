# M-IR-8 Safe Sequence

`ImmediateContext` 境界整理を実装するとき、描画を壊しにくい順序で進めるための実施順メモ。

## Recommended Order

### Step 1: Trace / Summary First

先に `Trace` / `FrameDebugSnapshot` / renderer summary を足す。

理由:
- 描画を変える前に、壊れたときの観測点を増やせる
- startup thread churn も同時に追える

対象:
- `ArtifactCore/include/Diagnostics/Trace.ixx`
- `ArtifactCore/include/Frame/FrameDebug.ixx`
- `Artifact/src/Widgets/Diagnostics/*`

### Step 2: Façade API Additions

`ArtifactIRenderer` に low-level 置き換え用の façade を足す。

例:
- `unbindColorTargetsForCompute()`
- `setViewportRect(...)`
- `restoreHostViewport(...)`
- renderer summary API

理由:
- call site をいきなり書き換える前に置換先を作れる

### Step 3: Controller-side Replacement

`CompositionRenderController` の direct context access を 1 本ずつ façade へ寄せる。

理由:
- ここが今の最優先 call site
- ただし一気にやると壊れやすいので、bind / viewport / compute 前後を分ける

### Step 4: Particle Path Cleanup

particle を `ParticlePassContext` 相当へ閉じ込める。

理由:
- particle は壊れやすいが、controller より局所化しやすい
- 先に summary / trace があると故障点を見失いにくい

### Step 5: Access Narrowing

最後に `immediateContext()` 依存を縮小する。

理由:
- façade と call site 置換が済んでから閉じる方が安全

## What To Avoid First

- 最初から `PrimitiveRenderer3D` を大改修する
- diagnostics のために新しい public low-level accessor を増やす
- `CompositionRenderController` と particle path を同時に大きく書き換える

## Practical Start

最初の実装着手点としては、次の順が安全。

1. `M-DIAG-5 Phase 1`
2. `M-IR-8` の renderer summary / façade の最小追加
3. `CompositionRenderController` の 1 か所置換
4. particle draw path の helper 化

## Related

- `docs/planned/RENDER_BOUNDARY_CHANGE_SAFETY_CHECKLIST_2026-04-21.md`
- `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`
- `docs/planned/MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md`
