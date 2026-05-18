# Milestone: DX12/Vulkan Ray Tracing Integration

Date: 2026-05-16

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
