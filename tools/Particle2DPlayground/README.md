**最終更新:** 2026-10-09

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

## GPU particle capture

Render a deterministic particle through the production Diligent GPU path and
read the target back to a PNG. The report includes particle/reference pixel
counts and the renderer submission state; the command exits nonzero if either
shape is missing:

```powershell
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --capture-gpu-particle <output-directory>
```

## PNG sequence and sprite-sheet capture

Render the same 16-frame petal animation from a numbered PNG directory and a
4×4 sprite sheet. Five PNGs per source and alpha-coverage counts are written to
separate subdirectories; the command also checks that both sources animate
and that the first and last sequence/sheet renders match pixel-for-pixel:

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
