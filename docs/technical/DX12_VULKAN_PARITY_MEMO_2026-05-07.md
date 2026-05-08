# DX12 / Vulkan Parity Memo - 2026-05-07

This memo records the current parity work for the Diligent-based render path.

## What was tightened

- `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`
  - fixed `blendDirect()` so the destination texture is passed explicitly as `DstTex`
  - kept the legacy overload only for compatibility, with a warning
- `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx`
  - added the new `blendDirect(src, dst, out, ...)` contract
- `ArtifactCore/include/Graphics/GPUcomputeContext.ixx`
  - added backend-neutral aliases:
    - `RenderDevice()`
    - `DeviceContext()`
- `ArtifactCore/src/Graphics/GPUcomputeContext.cppm`
  - implemented the backend-neutral aliases and kept the old `D3D12*` names for compatibility
- `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`
  - switched internal device lookup to the backend-neutral `RenderDevice()`
- several Core compute/render helpers were also moved from `D3D12RenderDevice()` / `D3D12DeviceContext()` to the neutral aliases

## Why this matters

- The compute blend path now has a clearer resource contract:
  - `SrcTex`
  - `DstTex`
  - `OutTex`
- The code no longer relies on a DX12-shaped API name where the implementation is actually backend-neutral.
- This reduces the risk of backend-specific mistakes when validating D3D12 and Vulkan parity.

## Related parity fixes already in place

- `LayerBlendComputeShader` now uses a storage-image-compatible linear format for `OutTex`
- render pipeline textures already use the linear pipeline format for accum/temp
- debug reporting now surfaces:
  - `videoState`
  - `particleState`
  - `textState`
  - `glyphAtlasState`

## Next checks to continue parity work

- verify all `blend()` call sites still use the correct source/destination ordering
- keep texture format / storage-image usage consistent across Vulkan and DX12
- prefer backend-neutral naming in new shared GPU helpers
- avoid silent backend fallback where a hard failure would make the bug easier to diagnose
