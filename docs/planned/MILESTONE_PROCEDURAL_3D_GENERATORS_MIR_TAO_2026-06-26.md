# Procedural 3D Generators Milestone

Date: 2026-06-26

Last updated: 2026-08-15

Status: Implementation present through the authoring/render/input/preset slices;
static audit completed 2026-06-28. Build, runtime visual checks, and render-queue
sequence verification remain pending under the repository execution policy.

Current implementation:

- deterministic Terrain and Path Tube CPU geometry generation
- normals, UVs, quality limits, tube/ribbon profiles, taper, twist, and noise
- project-serializable `ArtifactProcedural3DLayer`
- existing mesh renderer bridge with solid/wire shading
- Terrain image/audio inputs and Path Tube mask/shape source sampling
- creation menu, Inspector properties, cache diagnostics, and creative presets

Static audit follow-up (2026-07-25):

- `ArtifactCore::Procedural3DGenerators` provides deterministic Terrain and Path Tube mesh generation, bounded quality presets, normals/UVs, path profiles, taper/twist/noise, and result metadata.
- `ArtifactProcedural3DLayer` owns both generator variants, resolves image-luminance/audio inputs and mask/shape path data, generates the mesh at resolved frame/quality, exposes property groups, serializes/deserializes settings, and participates in the layer factory.
- The layer reaches the existing renderer through `ArtifactIRenderer::drawMesh`/wire handling; the render queue explicitly recognizes the layer type.
- The layer menu exposes `Terrain (Mir)` and `Path Tube (Tao)` creation actions with presets.
- P0-P6 are represented in static code. P7-P8 remain runtime/render-queue sequence verification items; no build or runtime execution was performed under the repository policy.

Update 2026-08-15:

- Current code still provides deterministic Terrain and Path Tube generation with bounded quality presets, normals/UVs, path profile, taper/twist/noise, and result metadata in `Procedural3DGenerators`.
- `ArtifactProcedural3DLayer` resolves image/audio and mask/shape inputs, serializes settings, exposes properties, participates in the layer factory, and reaches the existing mesh renderer including wire shading. Creation actions and presets remain present in the layer menu.
- No new evidence establishes render-queue sequence acceptance, runtime visual quality, cache invalidation under animated inputs, or long-running performance behavior. P7-P8 therefore remain pending.
- Status remains **authoring/render/input/preset slices implemented; runtime/build verification pending**. Build, tests, and runtime checks were not executed.

## Goal

Artifactのcomposition layerとして扱えるprocedural 3D sourceを実装する。

初期名称:

- `Terrain`: grid surface / height field generator
- `Path Tube`: spline path / tube / ribbon generator

この計画は互換クローンではなく、Artifact向けのprocedural 3D generator familyとして実装する。

## Non-Goals

- Trapcode plugin format互換
- After Effects plugin host互換
- DiligentEngine fork / gitlink更新
- 新規QtCSS、`QColorDialog`、グローバルsignal/slot配線
- hot pathでの新規`QImage`依存
- 最初からPBR / shadow / depth of fieldまで含めること

## Existing Anchors

- Mesh container: `ArtifactCore/include/Mesh/Mesh.ixx`
- Diligent mesh renderer: `ArtifactCore/include/Graphics/MeshRenderer.ixx`
- Particle/source layer precedent: `Artifact/include/Layer/ArtifactParticleLayer.ixx`
- Generator precedent: `Artifact/include/Generator/ArtifactParticleGenerator.ixx`
- Composition render boundary: `docs/COMPOSITION_EDITOR_CONTRACT.md`
- Software/GPU render direction: `Artifact/docs/MILESTONE_M11_SOFTWARE_RENDER_PIPELINE_2026-03-11.md`

## Architecture

### Core Layering

Keep the feature split into three layers:

1. `ArtifactCore` procedural geometry
   - owns deterministic mesh generation
   - has no editor UI responsibility
   - outputs `Mesh::RenderData` or a small renderer-ready geometry struct

2. `Artifact` composition source layer
   - owns timeline-facing state, serialization, properties, and preview invalidation
   - exposes generator properties through existing property group paths

3. Renderer bridge
   - converts generator output to `MeshRenderer` geometry buffers
   - receives resolved camera/viewport/layer state from composition controller
   - does not infer editor tool state

### Proposed Modules

Prefer implementation files over public module churn where possible.

Core:

- `ArtifactCore/include/Geometry/ProceduralSurface.ixx`
- `ArtifactCore/src/Geometry/ProceduralSurface.cppm`
- `ArtifactCore/include/Geometry/ProceduralPathMesh.ixx`
- `ArtifactCore/src/Geometry/ProceduralPathMesh.cppm`

Artifact:

- `Artifact/include/Layer/ArtifactProcedural3DLayer.ixx`
- `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- `Artifact/include/Generator/ArtifactProcedural3DGenerator.ixx`
- `Artifact/src/Generator/ArtifactProcedural3DGenerator.cppm`

Renderer integration:

- prefer extending existing 3D render path in `Artifact/src/Render/PrimitiveRenderer3D.cppm`
- only touch `ArtifactCore/src/Graphics/MeshRenderer.cppm` if an existing API cannot upload generated mesh data correctly
- do not modify low-level D3D12 / Diligent backend setup for MVP

Build registration:

- Any new `.ixx` / `.cppm` must be explicitly registered in the owning `CMakeLists.txt`
- Do not rely on `GLOB_RECURSE` discovery

## Data Model

### Generator Kind

```cpp
enum class Procedural3DKind {
    Terrain,
    PathTube
};
```

### Shared Settings

- `seed`
- `resolutionQuality` (`Draft`, `Preview`, `Full`)
- `shadingMode` (`Wire`, `Solid`, `Normal`, `Lit`)
- `baseColor`
- `opacity`
- `worldTransform`
- `cameraMode` (`CompositionCamera`, `LayerLocalCamera`)

### Terrain Settings

- `columns`, `rows`
- `sizeX`, `sizeY`
- `height`
- `noiseScale`
- `noiseAmplitude`
- `noiseOctaves`
- `noiseEvolution`
- `heightSource` (`Noise`, `ImageLuminance`, `AudioAmplitude` later)
- `uvMode` (`Grid`, `Planar`, `Polar`)
- `wireThickness` for overlay/preview

### Path Tube Settings

- `pathSource` (`Parametric`, `MaskPath`, `ShapePath` later)
- `profile` (`Tube`, `Ribbon`, `Beads`)
- `segments`
- `sides`
- `radius`
- `taperStart`, `taperEnd`
- `twist`
- `pathOffset`
- `noiseAmplitude`
- `noiseScale`
- `repeatCount`

## Milestones

### P0 - Design Lock and Module Contract

Goal:

Define the minimal public API before touching render integration.

Tasks:

- Add a short design note for the generator source family.
- Decide final UI names: `Terrain` / `Path Tube` unless product naming changes.
- Define `Procedural3DKind`, settings structs, and generated geometry output type.
- Confirm whether `Mesh::RenderData` is sufficient or a lean `GeneratedMeshData` is needed.
- Identify serialization shape for project save/load.

Definition of Done:

- Header/module declarations are self-contained.
- No import cycle is introduced.
- No editor or renderer behavior changes yet.

Risk:

- `.ixx` changes fan out. Keep public declarations minimal and put generation details in `.cppm`.

### P1 - Terrain CPU Geometry MVP

Goal:

Generate a deterministic animated height-field mesh without UI integration.

Tasks:

- Implement grid topology generation.
- Implement normal generation.
- Implement UV generation.
- Implement deterministic value/simplex-style noise helper in implementation scope.
- Add draft/preview/full resolution presets.
- Expose `generateTerrain(settings, timeSeconds)`.

Definition of Done:

- Given the same settings/time, output positions/normals/uvs/indices are stable.
- `columns * rows` bounds are clamped.
- Full resolution cannot allocate unbounded geometry.
- No `QImage` conversion path is required.

Candidate files:

- `ArtifactCore/include/Geometry/ProceduralSurface.ixx`
- `ArtifactCore/src/Geometry/ProceduralSurface.cppm`

### P2 - Path Tube CPU Geometry MVP

Goal:

Generate a tube/ribbon mesh from a parametric path.

Tasks:

- Implement basic parametric path sampling.
- Build tangent/normal/binormal frame with stable fallback at inflection points.
- Implement tube profile.
- Implement ribbon profile.
- Implement taper and twist.
- Expose `generatePathTube(settings, timeSeconds)`.

Definition of Done:

- Tube and ribbon render data share one output contract.
- Degenerate path lengths fail gracefully with empty mesh.
- Segment/sides limits are clamped.

Candidate files:

- `ArtifactCore/include/Geometry/ProceduralPathMesh.ixx`
- `ArtifactCore/src/Geometry/ProceduralPathMesh.cppm`

### P3 - Composition Source Layer

Goal:

Add a project-serializable layer that can represent either Terrain or Path Tube.

Tasks:

- Add `ArtifactProcedural3DLayer`.
- Store `Procedural3DKind` and variant settings.
- Implement `toJson()` / `fromJson()`.
- Implement `localBounds()` as conservative 2D bounds for editor selection.
- Implement property groups for core parameters.
- Keep generated mesh cache keyed by settings + frame/time + quality.

Definition of Done:

- Layer can be created, serialized, restored, and inspected.
- No new public signal/slot route is introduced.
- Properties use existing property group mechanism.

Candidate files:

- `Artifact/include/Layer/ArtifactProcedural3DLayer.ixx`
- `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`

### P4 - Renderer Bridge MVP

Goal:

Show generated mesh in the composition viewport through the existing render boundary.

Tasks:

- Add an `ArtifactIRenderer`/3D renderer entry point only if no suitable draw call exists.
- Convert generated mesh data to `MeshRenderer::updateMeshGeometry`.
- Reuse existing view/projection handling where possible.
- Add simple material constants: color, opacity, wire/solid mode.
- Use composition frame time for animated evolution.

Definition of Done:

- A Terrain layer appears in preview.
- A Path Tube layer appears in preview.
- Draft quality updates fast enough for parameter changes.
- Render controller passes explicit resolved state; renderer does not inspect global UI.

Candidate files:

- `Artifact/src/Render/PrimitiveRenderer3D.cppm`
- `Artifact/include/Render/PrimitiveRenderer3D.ixx`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

### P5 - Creation UI and Property Editing

Goal:

Expose the feature in normal authoring workflows.

Tasks:

- Add menu actions for `Layer > New > Terrain` and `Layer > New > Path Tube`.
- Add Project/Timeline display names.
- Add property editor sections:
  - Geometry
  - Surface / Profile
  - Displacement
  - Material
  - Render Quality
- Use existing theme tokens and existing approved color picker patterns.
- Add original Studio SVG icons only if the menu needs icons.

Definition of Done:

- User can create both layer types from UI.
- Parameters update preview without app restart.
- No QtCSS is added.
- No `QColorDialog` is added.

Candidate files:

- existing layer menu / composition creation service files after confirming current ownership
- `docs/WIDGET_MAP.md` should be checked before naming UI widgets

### P6 - Input Sources

Goal:

Make the generators useful in motion design rather than just demos.

Tasks:

- Terrain image luminance height source.
- Terrain audio amplitude height/evolution source.
- Path Tube from shape/mask path.
- Path Tube path offset and repeat animation.
- Optional texture path for base color/opacity using existing asset system.

Definition of Done:

- Missing assets degrade to procedural fallback.
- Asset paths serialize and relink consistently with Project View rules.
- `QImage` use, if required for file boundary decoding, stays explicit and outside hot render path.

### P7 - Render Queue and Cache Integrity

Goal:

Make output deterministic for still/sequence render.

Tasks:

- Ensure frame-number based evaluation is deterministic.
- Ensure preview quality and render quality are separate.
- Add cache invalidation on property/time/source changes.
- Confirm work-area sequence output uses the same frame evaluation.

Definition of Done:

- Same project/settings/frame yields same mesh.
- Draft preview does not leak into final render.
- Cache keys include generator kind, settings version, source asset revision, frame/time, and quality.

### P8 - Polish and Creative Controls

Goal:

Reach the first artist-friendly version.

Tasks:

- Presets:
  - low poly terrain
  - wire landscape
  - soft cloth wave
  - neon path tube
  - ribbon trail
- Viewport overlay for bounds and path handles.
- Better normals and optional faceted shading.
- Fog/depth fade if existing renderer path supports it cheaply.
- Audio reactive controls if audio sampling path is stable.

Definition of Done:

- Presets are project-safe and editable.
- Overlay is drawn after content through the composition render contract.
- Controls remain compact and scan-friendly.

## First Implementation Slice

Recommended first slice:

1. `ArtifactCore` terrain generator only.
2. One `ArtifactProcedural3DLayer` with `kind = Terrain`.
3. Renderer bridge for solid shaded mesh.
4. Menu action to create Terrain.
5. Save/load round trip.

Explicitly defer:

- Path Tube
- image/audio sources
- advanced material
- final render queue polish

Why:

Terrain proves the entire source-layer-renderer-property loop with the least editor complexity.

## Verification Plan

Do not run build/test/CMake unless explicitly approved.

When approved, verify in this order:

1. Targeted module hygiene check, if available.
2. Build only the smallest affected target.
3. Launch app and create a composition.
4. Add Terrain layer.
5. Change height, resolution, evolution, shading mode.
6. Save and reopen project.
7. Render a short image sequence after render queue path is wired.

Manual visual checks:

- Mesh is non-empty.
- Normals are not inverted.
- Wire/solid modes match UI.
- Preview parameter changes do not resize unrelated UI.
- Draft/Full quality switch changes mesh density without changing layer identity.

## Implementation Constraints

- Child repos must only be edited when explicitly requested.
- Commit child repos before parent gitlink updates if commits are requested.
- Keep Diligent/DX12 edits minimal and localized.
- Put `#include` only in global module fragments before `module X;`.
- Avoid new imports in `.ixx` unless required for declaration validity.
- Prefer forward declarations for pointer/reference-only types.
- Preserve CRLF on existing source edits.
- New source files require explicit CMake registration.

## Open Questions

- Should the initial layer type be a dedicated `ArtifactProcedural3DLayer` or reuse a broader generator layer concept?
- Should `Path Tube` consume existing mask/shape paths first, or start with parametric paths only?
- Should final render use the same Diligent mesh path immediately, or have a temporary software fallback for thumbnails?
- What should the user-facing Japanese names be in menus: `Terrain`, `Path Tube`, or more product-like names?
