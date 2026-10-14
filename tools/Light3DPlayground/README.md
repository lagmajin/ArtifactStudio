**最終更新:** 2026-10-14

# 3D Object and Light Layer Image Test

This opt-in capture runner starts no ArtifactStudio project or main application
executable. It renders the production `Artifact3DLayer` sphere with each
production `ArtifactLightLayer` type through the headless Diligent renderer,
saves PNG frames, and reports visible/lit pixel counts and mean RGB/luma.
The light-to-renderer conversion mirrors the composition render bridge; this
keeps the capture isolated while exercising the real 3D object, light layer
properties, renderer, shaders, and GPU readback.

The checks are image-based sanity checks, not a golden-image similarity test:
they reject missing/empty frames, non-finite channel values, or a mostly dark
object. Compare the five lit PNGs with `no_light_frame_0001.png` when reviewing
the captured output.

## Build and capture

Build from an already configured ArtifactStudio build tree:

```powershell
cmake --build <build-directory> --config Debug --target ArtifactLight3DPlayground
<build-directory>\bin\Debug\ArtifactLight3DPlayground.exe --capture <output-directory>
```

The output directory contains `point_frame_0001.png`, `spot_frame_0001.png`,
`parallel_frame_0001.png`, `ambient_frame_0001.png`, `area_frame_0001.png`,
`no_light_frame_0001.png`, and `light_3d_report.txt`.
