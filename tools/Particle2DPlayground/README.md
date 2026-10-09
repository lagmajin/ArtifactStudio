**最終更新:** 2026-10-09

# Artifact 2D ParticleLayer Playground

A small, independent executable for the production `ArtifactParticleLayer`.
It does not open or modify an ArtifactStudio project. Its source is separate,
while its CMake target links the same `ArtifactAppRuntime` used by the editor.
The regular mode exercises the layer's Diligent GPU draw path; flipbook modes
exercise the layer's image render path with a PNG sequence or sprite sheet.

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
- **N / F1**: return to regular GPU particles
- **1–5**: regular presets (fountain, fire, smoke, rain, snow)
- **F2 / F3 / F4**: petal, spark, or autumn-leaf PNG sequence
- **Shift+F2 / Shift+F3 / Shift+F4**: the matching packed sprite sheet
- **Up / Down**: raise or lower the selected emitter's rate

The sample image folders and atlases are found in `temp/particle_flipbook_test`
when launched from the repository or its build output. If they are elsewhere,
the app opens a folder or file picker. Numbered PNGs are sorted numerically;
the samples contain 16 frames and play at 12 fps. The test calls the layer's
existing `renderFrame()` image path for both sequence and atlas sprites; regular
particles call `ArtifactParticleLayer::draw()` and render through Diligent.

## Firework PNG capture

Capture the production explosion preset at five points from 1.1 to 1.9 seconds. The command drives deterministic frame sync and the software image-render path, saves transparent PNGs, and checks direct seek and revisiting frame 45 against continuous playback. It writes `determinism_report.txt` and exits nonzero for empty output, out-of-tolerance frames, or save failures:

```powershell
<build-directory>\bin\Debug\ArtifactParticle2DPlayground.exe --capture-firework <output-directory>
```
