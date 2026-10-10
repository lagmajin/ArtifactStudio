**最終更新:** 2026-10-10

# Artifact 3D Particle Capture Test

An independent image-based test for the production `ArtifactParticle3DLayer`.
It opens no ArtifactStudio project. The executable renders the GPU particle
layer through `ArtifactIRenderer`, saves PNG captures, and checks that camera
orbit changes the image while a direct seek reproduces the same frame as a
frame-by-frame playback path. Qt runs with its offscreen platform plugin, so
the capture command does not require a visible desktop session.
The companion GTest contract also checks model/camera translation, perspective
size change under model-space Z movement, velocity-aligned billboard orientation,
depth occlusion, disabled depth writes, and the 2D no-depth fallback. The
production capture compares both a direct frame-45 seek and a
60→10→45 frame revisit against continuous playback. CTest registers the
production-layer image capture alongside the renderer contract.

## Build and capture

Build the opt-in target from an already configured ArtifactStudio build tree:

```powershell
cmake --build <build-directory> --config Debug --target ArtifactParticle3DPlayground
<build-directory>\bin\Debug\ArtifactParticle3DPlayground.exe --capture <output-directory>
```

## Registered GPU tests

Configure the build tree with `ARTIFACT_BUILD_TESTS=ON` or
`ARTIFACT_ENABLE_OFFLINE_RENDER_TEST=ON`, with GTest and a supported GPU
backend. Build the renderer contract target, then run just the raw-renderer
contract and production-layer capture tests:

```powershell
cmake --build <build-directory> --config Debug --target ArtifactParticle3DRenderContractTest
ctest --test-dir <build-directory> -C Debug -R "^ArtifactParticle3D(RenderContract|LayerImageCapture)Test$" --output-on-failure
```

The build target also builds the `ArtifactParticle3DPlayground` capture runner.
CTest writes the generated PNGs and `particle_3d_report.txt` under
`<build-directory>\particle_3d_capture_test\Debug`.

The capture directory receives `front_view_frame_45.png`,
`depth_occluded_frame_45.png`,
`depth_disabled_control_frame_45.png`,
`orbit_view_frame_45.png`, `layer_transform_frame_45.png`, `later_frame_60.png`,
`direct_seek_frame_45.png`, `revisited_frame_45.png`, and
`particle_3d_report.txt`. The command exits
nonzero if the GPU path is unavailable, the production 3D layer does not queue
a draw with a 3D camera, a capture contains no visible particles, the orbit or
later-frame image does not change, or direct-seek/revisit determinism exceeds
the reported pixel tolerance. The depth capture turns on the production layer's
depth test and verifies that a nearer opaque 3D card hides all warm-colored
explosion particles in the image. A paired depth-disabled control verifies
that the same layer and card ordering leaves those particles visible when the
depth test is off.
