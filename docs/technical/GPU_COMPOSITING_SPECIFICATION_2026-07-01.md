# GPU Compositing Specification

Date: 2026-07-01
Status: Normative draft for the Composition Viewer GPU layer path
Applies to: `Artifact`, `ArtifactCore`, preview, RAM preview, render/export paths

## 1. Purpose

This document is the implementation contract for GPU layer composition.
Its purpose is to prevent each shader, renderer, and presentation path from
making a different assumption about color space, alpha representation, numeric
range, resource format, or resource lifetime.

The keywords **MUST**, **MUST NOT**, **SHOULD**, and **MAY** are normative.
When an older milestone or bug note conflicts with this document, this document
is the intended contract. A conflicting implementation is not evidence that
the contract changed.

Related background:

- `docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md`
- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md`
- `docs/planned/MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md`
- `docs/bugs/COMPUTE_BLEND_FAILURE_HYPOTHESES_2026-03-23.md`

## 2. Canonical Internal Representation

The composition accumulator has one canonical representation:

| Property | Required value |
|---|---|
| Channel order | RGBA |
| Color space | scene-linear |
| Alpha model | premultiplied alpha |
| Accumulator format | `RGBA32F` |
| Valid alpha range | `[0, 1]` |
| Valid finite values | NaN and infinity are forbidden |
| Coordinate origin | identical for source, destination, and output |

RGB values MAY exceed `1.0` only while the composition is explicitly operating
in an HDR scene-linear domain. Alpha MUST never exceed `1.0`.

The current Composition Viewer uses:

- layer graphics target: `RGBA8_UNORM_SRGB`
- converted layer compute surface: `RGBA32F`
- accumulator and temporary surface: `RGBA32F`

This format mismatch is an explicit conversion boundary. Code MUST NOT sample
the graphics layer target as if it were already the canonical compute format.

## 3. Alpha Contract

Three representations exist and MUST be named distinctly:

1. `layerEncoded`: graphics output in the layer render-target format.
2. `srcStraight`: scene-linear RGB with straight alpha, used by blend formulas.
3. `accumPremul`: scene-linear RGB premultiplied by alpha, used between layers.

The required conversion is:

```text
layerEncoded
  -> decode transfer function
  -> recover straight RGB when the graphics target contains premultiplied RGB
  -> srcStraight
  -> blend formula
  -> Porter-Duff source-over composition
  -> accumPremul
```

For alpha `a`:

```text
premultiply(rgb, a)   = rgb * a
unpremultiply(rgb, a) = a > epsilon ? rgb / a : 0
```

Unpremultiplication MUST use an epsilon guard. RGB MUST be set to zero when
alpha is effectively zero; hidden RGB under zero alpha MUST NOT enter later
blend calculations.

Opacity modifies source alpha exactly once:

```text
As = saturate(sourceAlpha * layerOpacity)
```

Opacity MUST NOT be applied once during layer drawing and again during compute
blend unless that double application is an explicitly documented effect.

## 4. Color-Space Contract

Blend equations MUST run in scene-linear space. They MUST NOT run directly on
sRGB-encoded RGB values.

- Reading an sRGB layer texture MUST perform a defined sRGB-to-linear decode.
- Accumulator and temporary textures MUST remain linear.
- Linear-to-display conversion MUST occur only at presentation or encoded-file
  output.
- UI colors, composition background colors, imported images, video frames, and
  generated layers MUST declare the conversion boundary by which they enter
  scene-linear composition.

The final presentation conversion MUST be separate from layer-to-compute
conversion. A generic function named `convertLayerToFloat` currently serves
more than one semantic purpose; future cleanup SHOULD split it into explicit
operations such as:

```text
decodeLayerForBlend(...)
unpremultiplyForPresentation(...)
encodeForDisplay(...)
```

## 5. Standard Blend Equation

Let:

- `Cs` be straight source RGB.
- `Cb` be straight destination RGB.
- `As` be source alpha after opacity.
- `Ab` be destination alpha.
- `B(Cb, Cs)` be the blend-mode RGB function.
- `Co` and `Ao` be premultiplied output RGB and output alpha.

All standard blend modes MUST use:

```text
Ao = As + Ab * (1 - As)

Co = CbPremul * (1 - As)
   + (B(Cb, Cs) * Ab + Cs * (1 - Ab)) * As
```

The result stored in `OutTex` is `(Co, Ao)` and is therefore premultiplied.
The next layer MUST read it as `accumPremul`.

`Normal` is the source-over specialization:

```text
Co = Cs * As + CbPremul * (1 - As)
Ao = As + Ab * (1 - As)
```

## 6. Numeric-Range Policy

Every blend shader MUST produce finite output.

The following rules apply:

- Alpha MUST be saturated to `[0, 1]`.
- Divisors MUST use an epsilon guard.
- Square roots and powers MUST receive valid domains.
- HSL conversion helpers MUST handle achromatic and zero-delta colors.
- NaN and infinity MUST be treated as a blend failure in debug validation.

For the current SDR Composition Viewer, `Add` MUST clamp its composed RGB to
`[0, 1]` before presentation. This prevents an unbounded additive result from
poisoning the downstream preview path.

```text
BAdd(Cb, Cs) = saturate(Cb + Cs)
```

When a true HDR output path is introduced, it MAY preserve RGB above `1.0`,
but only if all of the following are explicit:

1. the composition working range,
2. the output texture format,
3. the tone-mapping operator,
4. the display transfer function,
5. finite-value and maximum-luminance guards.

Removing the SDR clamp without those five pieces is forbidden.

## 7. Resource and Dispatch Sequence

For each visible layer, the GPU path MUST perform this sequence:

1. Clear `layerRTV` to transparent black.
2. Draw exactly one layer into `layerRTV`.
3. Resolve layer-local effects, masks, and mattes.
4. Flush pending graphics commands.
5. Unbind the layer resource from RTV usage.
6. Decode/convert `layerSRV` into the straight, linear compute source.
7. Bind `srcSRV`, `accumSRV`, and `tempUAV`.
8. Dispatch the selected blend compute shader.
9. Validate success in debug/harness builds.
10. Swap accumulator and temporary resources only after a successful dispatch.

The same texture subresource MUST NOT be bound simultaneously for conflicting
read/write roles. In particular:

- `DstTex` and `OutTex` MUST be different resources for a dispatch.
- an RTV MUST be unbound before the same resource is read as SRV or written as
  UAV.
- accumulator swapping MUST NOT occur after a failed dispatch.

Resource transitions MUST be performed through Diligent state transitions or
an equivalent explicit barrier mechanism. Reliance on incidental prior state
is forbidden.

## 8. Failure and Fallback Contract

A dispatch that returned from the API is not automatically a valid blend.
Failures include:

- missing or unready pipeline/executor,
- null SRV/UAV/constant buffer,
- failed resource binding,
- incompatible texture dimensions or formats,
- non-finite output,
- output that is unexpectedly all zero or a single saturated channel.

The Composition Viewer GPU path is fail-closed:

1. dispatch the requested blend mode only,
2. swap the accumulator only after a successful dispatch,
3. leave the accumulator unchanged when conversion, validation, binding, or
   dispatch fails.

Pipeline capability failures MUST be isolated. Standard layer blend readiness
depends on the blend constant buffer, layer-to-float conversion, and standard
blend executors. Optional track-matte, stencil, or stochastic capabilities MUST
NOT make `Add`, `Multiply`, or other standard blend modes globally unavailable.
When a layer actually requests an unavailable optional capability, that layer
MUST be skipped without swapping the accumulator; the capability MUST NOT be
silently ignored.

Viewer initialization MUST be retryable when the render device is not ready at
the first deferred attempt. A failed shader initialization MAY use a bounded
retry policy; it MUST NOT leave the viewer permanently on the direct path merely
because a one-shot startup callback ran before device creation completed.

The viewer MUST NOT silently retry with `Normal` or draw a direct sprite into
the float accumulator. Those paths have different blend, format, and alpha
semantics and can make a broken requested mode appear successful.

Export SHOULD fail with an actionable error or use a contract-equivalent CPU
compositor when the requested GPU mode is unavailable.

## 9. Masks, Mattes, and Special Modes

Masks and track mattes MUST be resolved before standard blend composition
consumes the layer source.

`StencilAlpha`, `StencilLuma`, `SilhouetteAlpha`, and `SilhouetteLuma` modify
destination coverage and are not ordinary RGB blend functions. They SHOULD use
an explicitly classified stencil/silhouette path instead of pretending to be a
standard `B(Cb, Cs)` mode.

`Dissolve` and `DancingDissolve` require deterministic stochastic input.
Preview and export MUST use a documented seed and frame rule. A random sequence
whose output changes between identical renders is forbidden.

## 10. Background and Presentation

Viewport chrome is not composition content.

- Checkerboard and viewport gradients MUST NOT enter `accumPremul`.
- A composition background layer MAY enter the accumulator if it is part of the
  composition model.
- Transparent composition output MUST NOT disable the GPU blend path. It starts
  from a transparent-black accumulator and follows the same layer loop.
- Transparent composition output MUST preserve alpha through the final
  accumulator.
- The final premultiplied accumulator MUST be converted exactly once to the
  representation expected by the presentation sprite or output encoder.

Presentation MUST NOT alter the stored accumulator or feed display-encoded
pixels back into subsequent layer blending.

## 11. CPU/GPU Parity

The CPU compositor and GPU compositor MUST implement the same:

- blend RGB function,
- opacity rule,
- Porter-Duff equation,
- alpha representation,
- clamp/HDR policy,
- exceptional-value handling.

Approximate parity is acceptable only for explicitly documented stochastic or
precision-sensitive modes. Mode names MUST NOT map to materially different
equations between CPU preview, GPU preview, and export.

## 12. Diagnostics

Debug capture SHOULD report at least:

- selected blend mode,
- source/destination/output formats,
- source and destination alpha representation,
- conversion success,
- dispatch success,
- fallback path,
- RGB and alpha min/max,
- NaN/Inf count,
- saturated-pixel count,
- layer name and ID.

The existing `FrameDebugSnapshot` Blend/Mask resource is the preferred reporting
surface. Permanent debug overlays in the viewport are not required.

For a blue-screen or single-channel failure, inspect in this order:

1. output contains NaN/Inf,
2. channel order or texture format mismatch,
3. sRGB decode/encode applied twice or omitted,
4. straight/premultiplied mismatch,
5. SRV/UAV aliasing or stale resource state,
6. unbounded blend output,
7. fallback presenting the wrong resource.

## 13. Minimum Verification Matrix

Every blend implementation change SHOULD cover:

| Case | Required coverage |
|---|---|
| Transparent source | destination unchanged |
| Transparent destination | source appears with correct alpha |
| Opaque source/destination | expected blend RGB |
| Source opacity 0, 0.5, 1 | monotonic result |
| Black/white primaries | channel-order sanity |
| Alpha edge near zero | no NaN/Inf |
| Repeated 32-layer accumulation | finite and stable |
| SDR Add | no channel-wide saturation failure |
| CPU vs GPU | bounded pixel error |
| Preview vs export | same mode and alpha semantics |

At minimum, fixed fixtures SHOULD include `Normal`, `Add`, `Multiply`, `Screen`,
`Overlay`, one HSL mode, one stencil mode, and one dissolve mode.

## 14. Ownership

| Responsibility | Owner |
|---|---|
| Blend-mode enum and names | `ArtifactCore/include/Layer/LayerBlend.ixx` |
| Compute equations | `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx` |
| Pipeline creation/binding/dispatch | `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm` |
| Viewer layer loop and fallback | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` |
| Renderer bridge | `Artifact/src/Render/ArtifactIRenderer.cppm` |
| CPU reference compositor | `Artifact/src/Render/Software/ArtifactSoftwareImageCompositor.cppm` and `ArtifactCore/src/Color/ColorBlendMode.cppm` |

Low-level Diligent backend code is not the default owner of application-level
format, alpha, or blend inconsistencies. Before changing the backend, confirm
that the application resource declarations, module wiring, and binding sequence
already obey this contract.

## 15. Definition of Done

The GPU compositing contract is considered implemented when:

1. all composition paths declare their color and alpha representations,
2. layer decode and final presentation conversion are separate operations,
3. all standard modes use the canonical Porter-Duff equation,
4. Add cannot corrupt SDR preview,
5. special modes are classified outside the standard path where necessary,
6. fallback is visible and export-safe,
7. CPU/GPU fixtures cover the minimum verification matrix,
8. debug capture can distinguish dispatch failure from valid but empty output.

