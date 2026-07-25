# Milestone: NRD Ray-Tracing Denoiser Integration

**Status:** Dependency staged; render-pass integration pending

## Scope

Integrate NVIDIA Real-time Denoisers (NRD) as an optional GPU post-pass for the planned Diligent DX12/Vulkan ray-tracing path. NRD is not applied to the existing 2D compositor output.

## Current repository state

- Diligent ray tracing is already feature-gated by `ARTIFACT_ENABLE_RAY_TRACING`.
- `ArtifactCore::IRayTracingManager` already owns the TLAS, SBT, and floating-point RT output.
- `ArtifactIRenderer` exposes the RT output view.
- NRD is pinned as the `libs/NRD` git submodule.

## Integration boundary

The denoiser must remain separate from `IRayTracingManager`:

```text
ray generation -> noisy radiance
              -> normal / view-Z / motion-vector auxiliary targets
              -> NRD compute dispatches
              -> denoised radiance
```

The first implementation target is `REBLUR_DIFFUSE_SPECULAR` in a temporal path. `RELAX` is a later option for cases where the ray-traced signal and material classification require it.

## Required next code slice

1. Add RT-owned normal, view-Z, motion-vector, and denoised-output textures.
2. Add a backend-neutral `IRayTracingDenoiser` contract in `ArtifactCore`.
3. Add D3D12 and Vulkan NRD resource/state adapters in the renderer layer.
4. Compile NRD HLSL/GLSL shaders through the existing Diligent shader path.
5. Dispatch NRD only when RT is enabled and all auxiliary inputs are valid; otherwise return the raw RT output.
6. Reset NRD history on camera cuts, resize, composition changes, and RT disable/enable transitions.

## Constraints

- Do not put NRD into the 2D composition pipeline.
- Do not share the immediate context across worker threads.
- Do not add Qt stylesheet, Qt composition, or `QImage` conversion to the RT/NRD path.
- Do not claim denoised output until normal, depth, and motion-vector inputs are produced by the actual RT shaders.

## 2026-07-25 実装監査

- `ARTIFACT_ENABLE_RAY_TRACING`、`IRayTracingManager`、RT output view の既存基盤は計画書の記載どおり確認できる。
- NRD 専用の `IRayTracingDenoiser` contract、normal／view-Z／motion-vector／denoised output texture、DX12／Vulkan resource adapter、REBLUR dispatch は現行コードで確認できない。
- `TemporalDenoiseEffect` は既存 2D rasterizer effect であり、NRD の RT path の実装証拠には含めない。
- したがって本マイルストーンは `Dependency staged; render-pass integration pending` のままとする。NRD output、history reset、backend parity は未実装・未検証である。
