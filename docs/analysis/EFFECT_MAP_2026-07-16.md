# ArtifactStudio Effect Map

**Status:** Living analysis

## Purpose

This map separates four facts that are currently easy to confuse:

1. an effect implementation exists somewhere in the repository;
2. the Composition effect service can create it;
3. the Add Effect menu exposes it;
4. its GPU path executes HLSL rather than falling back to CPU.

The map is the source for effect-catalog cleanup and GPU parity prioritization.
The current source audit finds 47 public `supportsGPU() == true` contracts and
49 effect implementation files containing an HLSL literal; these are different
counts because one implementation file can serve multiple effect modes and
some declared contracts still use CPU fallback paths.
The CPU audit currently finds `ArtifactCore::Parallel::For` in 72 effect
implementation files; this is an adoption signal, not a claim that every
effect has a fully parallel pipeline.
It does not replace the CPU/HLSL dual-backend or GPU-parity milestones.

Performance caveat: the newly added fallback-compatible GPU paths currently
accept `ImageF32x4RGBA` input, upload it to a temporary texture, dispatch the
shader, wait for completion, and read the result back. They prove an HLSL
execution path, but they are not yet GPU-native compositor paths and should
not be treated as a performance win until texture ownership and synchronization
are moved into the render pipeline.

## Legend

| Field | Meaning |
|---|---|
| Service | `ArtifactEffectService` can create the effect from a stable ID. |
| Menu | `availableEffects()` currently exposes it to Add Effect. |
| CPU | A usable `ArtifactAbstractEffect` CPU/reference implementation exists. |
| GPU | `HLSL` means an actual compute dispatch is present; `Declared` means the class advertises GPU support but needs runtime audit; `CPU` means CPU-only. |
| Temporal | The result requires a previous frame, history, or a frame sampler. |

## Current User-Facing Catalog

| Category | Representative effects | Service / Menu | GPU status | Temporal | Notes |
|---|---|---|---|---|---|
| Color correction | Brightness, Exposure, Hue/Saturation, Curves, Levels, Color Balance, Channel Mixer, Tritone, Fill | Yes / Yes | Mostly Declared; audit required | No | Strongest catalog coverage. LUT and explicit color-space transform are not represented as first-class catalog entries. |
| Blur / sharpen | Gaussian Blur, Blur, Sharpen, Radial Blur, Anisotropic Flow Blur, Aperture Shape Blur | Yes / Yes | Mixed Declared / CPU | Some | Parameter parity and real dispatch need an effect-by-effect audit. |
| Glow / styling | Glow, Edge Bloom, Chromatic Glow, Reactive Glow, Halation, Bevel, Satin, Stroke | Yes / Yes | Mixed Declared / CPU | Some | Good breadth, but grouping and GPU evidence are inconsistent. |
| Distort | Wave, Lens Distortion, Liquify, Turbulent Displace, Spherize, Twist, Bend, Kaleidoscope | Yes / Yes | Mixed Declared / CPU | No | Geometry and raster effects share the menu without a stage indicator. |
| Keying / matte | Chroma Key, Displacement Map | Yes / Yes | Mixed | No | Luma Key and Difference Matte are not exposed as normal catalog effects. |
| Temporal | Screen Shake, Time Displacement, Posterize Time, Echo, Frame Blend, Feedback, Trails | Partial / Partial | Predominantly CPU | Yes | Requires explicit history/reset/cache metadata in the catalog. |
| Generate | Radio Waves, Simple Rain, procedural generators | Partial / Partial | Mixed | No | Generator vs filter distinction is not visible to users. |
| Creative | Glitch, Halftone, Old TV, Dithering, Kuwahara, Film Damage, Chromatic Relief | Partial / Partial | Glitch/Halftone/Old TV: HLSL; others mixed | Some | Two independent creative-effect families currently coexist. |

## Creative Effect Bridge Audit

`ArtifactCore::CreativeEffectFactory` has a broader creative pack than the
Composition `ArtifactEffectService` catalog. The bridge is incomplete.

| Creative effect | Core factory | Composition service / menu | Raster effect | GPU status | Action |
|---|---:|---:|---:|---|---|
| Glitch | Yes | Yes / Yes (`builtin.glitch`) | Yes, duplicate family | HLSL in `ArtifactCreativeEffects` | Give it one stable service ID and retire duplicate presentation. |
| Halftone | Yes | Yes / Yes | Yes, duplicate family | HLSL in `ArtifactCreativeEffects` | Make one parameter contract the canonical one. |
| Old TV | Yes | Yes / Yes (`builtin.old_tv`) | No user-facing service entry | HLSL in `ArtifactCreativeEffects` | Runtime-validate before treating its GPU path as production-ready. |
| Pixelate | Yes | Yes (`builtin.pixelate`) / catalog | Mosaic implementation reused | CPU | Bridged through the existing Mosaic effect contract. |
| Posterize | Yes | Yes (`builtin.posterize`) / catalog | Curves Posterize preset reused | CPU | Bridged through the existing Curves preset contract. |
| Mirror | Yes | No / No | No direct catalog item | CPU | Low-cost bridge candidate. |
| Fisheye | Yes | No / No | Lens Distortion is related | CPU | Decide whether Lens Distortion supersedes it. |
| Edge Echo | Yes | No / No | No direct catalog item | Core has GPU computer | Best candidate for an explicit bridge after its host contract is verified. |
| Temporal Fossil / Surface Memory | Yes | No / No | No | CPU/history | Require temporal cache/reset contract before exposure. |
| Pigment Separation / Depth Melt / Light Pressure | Yes | No / No | No | CPU | Creative-pack bridge candidates after property schemas are defined. |

## Confirmed HLSL Paths in This Pass

| Effect | Location | Fallback |
|---|---|---|
| Glitch | `Artifact/src/Effect/ArtifactCreativeEffects.cppm` | CPU reference implementation |
| Halftone | `Artifact/src/Effect/ArtifactCreativeEffects.cppm` | CPU reference implementation |
| Old TV | `Artifact/src/Effect/ArtifactCreativeEffects.cppm` | CPU reference implementation |
| Vignette | `Artifact/src/Effects/Rasterizer/VignetteEffect.cppm` | CPU reference implementation |
| Stripes | `Artifact/src/Effects/Rasterizer/StripesEffect.cppm` | CPU reference implementation |
| Add Noise | `Artifact/src/Effects/AddNoise/AddNoiseEffect.cppm` | CPU reference implementation |
| Grayscale | `Artifact/src/Effects/ColorCorrection/GrayscaleEffect.cppm` | CPU reference implementation |
| Linear Wipe | `Artifact/src/Effects/LinearWipe/LinearWipeEffect.cppm` | CPU reference implementation |
| Find Edges | `Artifact/src/Effects/FindEdges/FindEdgesEffect.cppm` | CPU reference implementation |
| White Balance | `Artifact/src/Effects/WhiteBalanceEffect.cppm` | CPU reference implementation |
| Gradient Ramp | `Artifact/src/Effects/ColorCorrection/GradientRampEffect.cppm` | CPU reference implementation |
| Color Wheels (LGG / OGG) | `Artifact/src/Effects/ColorCorrection/ColorWheelsEffect.cppm` | CPU fallback for Three-way / Levels |
| Lift / Gamma / Gain | `Artifact/src/Effects/LiftGammaGainEffect.cppm` | CPU reference implementation |
| Radial Shadow | `Artifact/src/Effects/RadialShadow/RadialShadowEffect.cppm` | CPU reference implementation |
| Directional Glow (built-in patterns) | `Artifact/src/Effects/DirectionalGlowEffect.cppm` | CPU fallback for Custom angles |

`supportsGPU()` alone is not proof of a real shader path. The map must record
the implementation and later runtime evidence separately.

Directional Glow and Radial Shadow currently have source-level Compute paths,
but their OpenCV CPU compositing and GPU shader compositing are not yet proven
pixel-identical. They remain `HLSL implemented / parity pending` until a
runtime CPU-vs-GPU comparison is performed.

The current source audit found 47 effects advertising `supportsGPU() == true`.
Several legacy declarations still need classification because their current
`applyGPU()` body delegates directly to `applyCPU()`. Lens Distortion, Drop
Shadow, Wave, and Spherize now have concrete HLSL candidates/paths. Bevel,
Chromatic Glow, and Radial Blur now have real HLSL paths; Turbulent Displace
remains CPU-only despite its legacy declaration.

## CPU MT Progress

The following spatial CPU reference implementations now use
`ArtifactCore::Parallel::For` over independent scanlines:

- Creative: Glitch, Old TV, Halftone
- Rasterizer: Vignette, Stripes, Chromatic Aberration, Hex Grid, Bricks,
  Voronoi, Strobe, Rasterizer Glow, Edge, Difference Matte, Rasterizer
  Kaleidoscope, Trail Fade, Radial Blur, Add Noise, White Balance, Color Wheels,
  and Color Curves
- Color correction: Brightness, Channel Mixer, Hue/Saturation, Color Balance,
  Gradient Ramp, Tritone, Levels, Selective Color, Photo Filter, Colorama,
  and Fill
- Generators: Simple Rain composite and Radio Waves pixel evaluation
- Procedural Texture Generator: existing CPU preset evaluation already uses
  deterministic horizontal stripes with joined worker threads when enabled
- Additional spatial CPU paths: Exposure and Chromatic Relief
- Additional independent-row paths: Auto Mosaic feathering and Displacement Map
- Geometry/rasterizer CPU paths: Lens Distortion, Wave, Spherize, and Liquify
  now use the shared `ArtifactCore::Parallel::For` row scheduler rather than
  effect-local QtConcurrent mapping
- Additional post-filter path: Bevel highlight/shadow field construction
- Shadow paths: Drop Shadow and Inner Shadow extraction, matte construction,
  and final composite; both row paths now derive source pointers per row to
  remain race-free under the shared scheduler
- Glow paths: Liquid Glow flow-map construction and Chromatic Glow composite
- Liquid Glow final flow remap now uses a row-parallel bilinear sampler with
  explicit `BORDER_REFLECT_101` handling instead of serial `cv::remap`
- Radial Blur sample remapping is also row-parallel with explicit bilinear
  `BORDER_REPLICATE` handling
- Additional glow/shadow paths: Luminescence Caustics and Radial Shadow field generation
- Directional Glow: bright extraction, directional blur rows, and final streak composite
- Screen Shake: clamp/mirror displacement sampling path
- Stroke: alpha extraction, stroke matte construction, layer generation, and
  final OVER composite are row-parallel; morphology remains owned by OpenCV
- Satin: alpha extraction, color matte generation, and final blend are
  row-parallel; offset copy and GaussianBlur remain OpenCV operations
- Vector Blur: block-wise motion-vector estimation and final sample rows are
  parallel after the previous-frame snapshot is acquired
- Optical Flow Blur: per-pixel Lucas–Kanade flow rows and final smear rows are
  parallel after the previous-frame snapshot is acquired
- Temporal Smear: block-wise motion estimation and final jittered smear rows
  are parallel after the previous-frame snapshot is acquired
- Mosaic: independent rectangular/diamond cells are processed in parallel;
  each cell reads and writes only its own region
- Kuwahara: CPU reference now reads from an immutable source snapshot and
  writes independent output rows in parallel; this removes pixel-order
  dependence before the expensive quadrant statistics
- Chroma Key: per-pixel key-distance, alpha, and spill-reduction processing
  is row-parallel after the source clone
- Chromatic Aberration: CPU row path retained and a Compute/HLSL radial
  channel-shift path added; CPU/GPU interpolation parity remains pending
- Blur: premultiplied color-space conversion rows are parallel; OpenCV
  GaussianBlur iterations remain delegated to OpenCV
- Deflicker: history and median target calculation remain serial, while the
  post-history luminance correction is row-parallel
- Ghost: each sampled historical frame is acquired serially, then its alpha
  accumulation is processed in parallel rows
- Motion Trail: previous-frame acquisition stays serial, while block-wise
  motion estimation and final trail sampling rows are parallel
- Light Trails: previous-frame acquisition stays serial, while block-wise
  motion estimation and luminance-gated trail rows are parallel
- Pixel Sort: previous-frame acquisition stays serial, while block-wise motion
  estimation and per-pixel gather/sort/blend processing are parallel
- Slit Scan: persistence decay, slit-row insertion, and output clamping are
  parallelized after the frame-state update remains ordered
- Time Warp remains serial: each pixel may request a different historical
  frame, and the current frame-sampler contract does not make those accesses
  safe for concurrent calls
- Frame Blend: previous-frame acquisition remains serial, while the
  two-frame pixel blend is row-parallel
- Echo: historical frame sampling and weight accumulation remain ordered,
  while the final multi-echo pixel blend is row-parallel
- Feedback: previous-frame acquisition remains serial, while transformed
  previous-frame sampling and compositing are row-parallel
- Frame Average: historical frame collection and weight totals remain ordered,
  while the final temporal average is row-parallel
- Frame Accumulation: previous accumulation import and phase ordering remain
  serial, while decay, current-frame contribution, and final blend phases are
  row-parallel
- Data Mosh remains serial: RNG/stateful block aging and overlapping block
  blend order are part of the output contract
- Film Damage: deterministic per-row grain/noise and composite path
- Dithering: Bayer 2x2/4x4/8x8/16x16 modes; error-diffusion modes remain serial
- Turbulent Displace: deterministic noise-field generation and the final
  bilinear remap are both row-parallel; GPU remains intentionally undeclared

The shared `ArtifactAbstractEffect::applyConfigured()` mask-composite pass is
also row-parallelized, covering mask-enabled effects across both CPU reference
and GPU-fallback execution.

Temporal effects remain intentionally separate until their history ownership,
frame reset, and source/destination aliasing rules are explicit.

The remaining not-fully-MT scanline users are intentionally kept out of the
simple MT pass when their core path depends on temporal history, error
diffusion, or multi-scale convolution. Current examples are Time Warp,
Time Displacement, Reaction Diffusion Blur, Dithering error-diffusion modes,
Aperture Shape Blur's whole FFT pipeline, and the multi-layer Glow
implementation. Echo, Feedback, Frame Blend, Mosaic, and Kuwahara now have
safe parallel sections documented below.

Aperture Shape Blur is an exception at the channel level: its three independent
FFT convolution channels are dispatched in parallel, while each channel's
FFT remains owned by OpenCV.

`Bevel` now has a real Compute/HLSL path using a 3x3 local edge estimate;
because the CPU reference uses OpenCV GaussianBlur and the shader uses a
different neighborhood approximation, it is HLSL implemented but parity
pending. `Radial Blur` now has a Compute/HLSL sampling path matching its
current displacement contract; interpolation and edge behavior still require
runtime parity verification. `Chromatic Glow` now has a Compute/HLSL bright
sample path; its blur kernel differs from the CPU GaussianBlur reference, so
parity remains pending. `Turbulent Displace` is CPU-only despite having a
private fallback implementation. `Satin` now has a Compute/HLSL offset-and-
sample path; its sample kernel differs from the CPU GaussianBlur reference,
so parity remains pending.

`Stroke` now has a Compute/HLSL circular-neighborhood expansion path. Its CPU
reference remains row-parallel, while the shader uses a fixed maximum search
radius and therefore needs runtime parity verification against the OpenCV
elliptical morphology. A fallback call inside an otherwise real GPU path is
not treated as a stub without inspecting the full implementation body.

`Hex Grid` now has a Compute/HLSL generator path. It is output-only because the
effect is procedural; CPU remains the parity reference and GPU/CPU edge-shape
matching is still pending.

`Lens Distortion` now has a Compute/HLSL remapping path with explicit input and
output textures. The shader now uses four-tap bilinear sampling; edge clamping
and coordinate rounding still require runtime parity verification.

`Drop Shadow` now has a Compute/HLSL path using a bounded 9x9 Gaussian-like
alpha kernel and explicit shadow-over-foreground compositing. Its kernel and
edge behavior intentionally remain parity-pending against the OpenCV CPU
reference.

`Wave` and `Spherize` now have input-texture Compute/HLSL remapping paths. Both
retain the existing CPU implementation as fallback; edge rounding and the
CPU sampling convention still require runtime parity comparison.

`Liquify` now has a Compute/HLSL circular-brush path. Advanced brush types
continue to use the CPU implementation deliberately, so this is a partial GPU
contract rather than a claim of full brush-mode parity.

The remaining direct GPU-to-CPU delegates are limited to non-public/legacy
paths and Turbulent Displace; they are not counted as completed HLSL paths.

Static audit result: among conventional effect headers that explicitly return
`true` from the inline `supportsGPU()` contract, no corresponding `.cppm`
implementation is currently missing a shader source or Compute pipeline
marker. Two special cases remain separately classified: the Creative aggregate
module and the header-defined `ProceduralTextureGeneratorEffect`. They require
their owning module contracts to be audited rather than being matched by a
same-basename `.cppm` lookup. This is only a source-level result; it does not
replace backend shader compilation.

`ProceduralTextureGeneratorEffect` is explicitly CPU-only until its
preset-specific GPU contract exists; this prevents `supportsGPU()` from
advertising a mode with no GPU implementation.

Temporal GPU caveat: `Vector Blur` and `Temporal Smear` contain Compute
shaders, but their current shader inputs do not include the previous-frame
image or a motion-vector texture. They are structural GPU paths, not
CPU/GPU-equivalent temporal implementations. The CPU MT work in these effects
is valid only after the previous-frame snapshot has been acquired.

## GPU Implementation Queue

The next HLSL candidates should be selected from effects that are both visible
in the Composition catalog and currently CPU-only or only declared as GPU
capable. The simple candidate pass is complete; remaining work is parity and
GPU-native integration:

1. Color correction with simple per-pixel contracts: Lift/Gamma/Gain is now
   implemented for the two wheel modes; remaining candidate is the full
   Three-way / Levels contract.
2. Spatial stylize: validate Chromatic Aberration, Hex Grid, and Voronoi.
3. Expensive neighborhood effects: Bevel, Chromatic Glow, Glow, Radial Blur,
   and Find Edges, after their parameter and color-space parity is
   documented. Replace CPU-delegating GPU stubs rather than only adding a
   `supportsGPU()` declaration.
4. Temporal effects only after the temporal host/cache contract is available.

## Verification Checklist

For every `HLSL implemented` entry, record these separately:

1. shader source compiles for the active backend;
2. resource signature variables bind without warnings;
3. a CPU/GPU comparison uses the same input, dimensions, parameters, and
   color representation;
4. fallback behavior is exercised for device loss, unsupported mode, and
   readback failure;
5. upload/readback timing is measured separately from shader dispatch time.

Until all five have evidence, the entry remains `parity pending` and must not
be counted as a GPU performance improvement.

## Gaps to Prioritize

1. **Catalog bridge:** expose the Core creative pack through stable
   `ArtifactEffectService` IDs, starting with Glitch, Old TV, Pixelate,
   Posterize, and Mirror.
2. **Keying / matte:** add or bridge Luma Key and Difference Matte, then state
   whether an effect requires a secondary layer/source.
3. **Color pipeline:** add first-class LUT and color-space transform entries
   only after the working/input/output color contract is fixed.
4. **Temporal contract:** add `requiresHistory`, reset behavior, and cache cost
   to all temporal effect entries before expanding their menu exposure.
5. **GPU evidence:** distinguish `Declared`, `HLSL implemented`,
   `runtime verified`, and `CPU fallback` in a generated catalog.

## Required Implementation Policy

The following is the active implementation order for every catalog effect.

1. **GPU path:** when an effect has no actual HLSL/compute implementation, add
   one. A `supportsGPU()` declaration or a GPU implementation that simply calls
   `applyCPU()` does not satisfy this requirement.
2. **CPU reference path:** retain the CPU implementation for test, comparison,
   and fallback use, but parallelize it by safe independent work units. Use rows
   or tiles for spatial effects; temporal/history effects must partition only
   after their read/write history contract is explicit.
3. **Fallback:** GPU allocation, PSO creation, dispatch, and readback failures
   must return to the CPU reference implementation without changing the effect
   result contract.
4. **Evidence:** do not label a backend `runtime verified` until it has been
   exercised on the supported backend with the CPU/GPU output checked.

This policy applies first to effects visible from `ArtifactEffectService`, then
to the wider Core creative-effect pack as it is bridged into the Composition
catalog.

## Proposed Catalog Record

Every effect should eventually emit a single descriptor:

```text
id, displayName, category, pipelineStage,
serviceCreatable, menuVisible,
cpuReference, gpuMode, gpuRuntimeVerified,
supportedLayerKinds, maskSupport,
temporalRequirement, resetPolicy,
propertySchemaVersion, deprecation/replacement
```

The descriptor should be consumed by Add Effect, Inspector, render diagnostics,
and an eventual CPU/GPU comparison view instead of maintaining separate lists.

## Next Implementation Slice

1. Decide canonical ownership for Halftone and remove its duplicate catalog
   route.
2. Add the descriptor fields above incrementally, starting with CPU/GPU state
   and temporal requirements.

## Static Contract Audit (2026-07-16)

The effect headers advertising `supportsGPU() == true` were matched against
their corresponding `src/Effects` or `src/Effect` implementation files using
the repository's absolute child-root paths. The audit found no advertised GPU
effect without a `ComputeExecutor`, compute dispatch, or pipeline-creation
marker. This is source-level evidence only; it does not prove shader
compilation, resource binding, dispatch success, or CPU/GPU pixel parity.

## Related Documents

- `docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`
- `docs/planned/MILESTONE_CREATIVE_EFFECT_CPU_HLSL_DUAL_BACKEND_2026-03-25.md`
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md`
- `ArtifactCore/docs/CREATIVE_EFFECTS_MEMO.md`
