**最終更新:** 2026-10-09

# Artifact 2D ParticleLayer Playground

A small, independent executable that creates and renders the production
`ArtifactParticleLayer` through `ArtifactIRenderer` and its Diligent GPU path.
It does not open or modify an ArtifactStudio project. Its source is separate,
while its CMake target links the same `ArtifactAppRuntime` used by the editor
so the test exercises the real layer, simulation, and renderer.

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
- **1–5**: select fountain, fire, smoke, rain, or snow
- **Up / Down**: raise or lower the selected emitter's rate

The window advances the layer's composition frame at 30 fps. Rendering goes
through `ArtifactParticleLayer::draw()` and `ArtifactIRenderer`; it does not
read back GPU frames to the CPU for display.
