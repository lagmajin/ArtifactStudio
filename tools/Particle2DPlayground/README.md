**最終更新:** 2026-11-19

# Artifact 2D ParticleLayer Playground

A small, independent executable for the production `ArtifactParticleLayer`.
It does not open or modify an ArtifactStudio project. Its source is separate,
while its CMake target links the same `ArtifactAppRuntime` used by the editor.
The interactive window exercises the layer's Diligent GPU draw path. The
firework capture command exercises the deterministic frame-sync and software
image-render path.

## Build and run

Configure the ArtifactStudio root project with its normal dependencies, then
build and run the additional target:

```powershell
cmake --build <build-directory> --config Debug --target ArtifactParticle2DPlayground
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe
```

The target is excluded from the default build and is added by the root CMake
project after `ArtifactAppRuntime` is defined.

## Controls

- **Space**: pause or resume
- **R**: reset the deterministic emitter
- **1–5**: regular presets (fountain, fire, smoke, rain, snow)
- **6**: explosion burst (firework)
- **7**: leaves carried by steady wind and turbulence
- **Up / Down**: raise or lower the selected emitter's rate

## Life curve persistence check

Check independent size, color, and opacity curve evaluation and GPU particle
falloff, ring radius, color intensity, density LOD, and trail settings through the layer's
property API and JSON round-trip, including
defaults for older JSON files:

```powershell
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --check-life-curves
```

This checks numerical curve evaluation and persistence, not the visual output
or GPU rendering.

## GPU shape capture

Capture production-layer Soft Circle, Gaussian, Hard Circle, Ring,
stretched Ring, Diamond, and Cross through the headless Diligent renderer:

```powershell
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --capture-gpu-shapes <output-directory>
```

The command writes seven PNGs and `gpu_shapes_report.txt`, failing if a draw is
empty. The images still need visual comparison; this is not an HDR or blend
equation test.

## GPU particle capture

Render both a deterministic core particle and a production `ArtifactParticleLayer`
through the Diligent GPU path, then read each target back to PNG. The report
includes particle/reference pixel counts and renderer submission state; the
command exits nonzero if either render is missing:

```powershell
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --capture-gpu-particle <output-directory>
```

## 2D Shape/Text target evaluation

Capture the production `ArtifactParticleLayer` targeting an ordered Shape
Stack ring (outer and inner ellipse paths, Merge Paths/Subtract, then fill and
stroke) and the stroked two glyph text `A8`. Fixed fixture LayerIDs keep target
sampling seeds comparable across separate backend runs. The report includes rendered
alpha at the hole center and ring edge. The headless Diligent backend is selected with
`ARTIFACT_RENDER_BACKEND=d3d12` or `vulkan`. Captures include frames 30/60/90/180/240,
a frame-60 direct-seek replay, disabled-default parity, alpha-preserving PNGs,
and particle-center counts for the shape, its hole, and the text bounds:

```powershell
$env:ARTIFACT_RENDER_BACKEND = "d3d12"
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --capture-shape-target output\particle-shape-target\d3d12
$env:ARTIFACT_RENDER_BACKEND = "vulkan"
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --capture-shape-target output\particle-shape-target\vulkan
```

Each output directory contains `shape_target_report.json` and the PNG sequence.
Compare both backend reports and inspect full frames plus 100%/400% crops. The
`output/particle-shape-target/backend_comparison.json` records RGBA/alpha
differences and particle-center parity between the latest Vulkan/D3D12 runs.
The capture covers this controlled static Shape/Text fixture; it does not cover
animated targets, stroke-only Shape targets, export color management, or all
font backends. Each baseline/Shape/Text case also reports five warmed
`ParticleSystem::update()` timing batches at 360 live particles and sampled process
working-set growth. That timer excludes rendering, GPU readback, and PNG work;
the working-set sample is CPU process memory, not GPU memory, and can miss
short peaks.

Check the option's default, JSON round trip, and legacy JSON defaults with:

```powershell
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --check-shape-target-persistence
```

## PNG sequence and sprite-sheet capture

Render the same 16-frame petal animation from a numbered PNG directory and a
4×4 sprite sheet. Five PNGs per source and alpha-coverage counts are written to
separate subdirectories; the command also checks that both sources animate
and that the corresponding sequence/sheet renders match within a small
pixel tolerance:

```powershell
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --capture-flipbook <sequence-directory> <sprite-sheet.png> <output-directory>
```

## Firework PNG capture

Capture the production explosion preset at five points from 1.1 to 1.9 seconds.
The command drives the layer through its frame synchronization and software
fallback draw path, then saves the resulting transparent PNGs. It also checks
that direct seek and revisiting frame 45 match continuous playback within a
small pixel tolerance, and writes `determinism_report.txt`. It exits nonzero if
the images are empty, the frame comparison exceeds tolerance, or saving fails:

```powershell
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --capture-firework <output-directory>
```

## Wind-blown leaves PNG capture

Save five frames from the production leaves preset with rightward wind,
turbulence, and drag. The command reports live-particle counts and checks that
the first and last captures differ, so it covers changing trajectories as well
as PNG output:

```powershell
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --capture-wind-leaves <output-directory>
```
