# Depth of Field Lab

**Capture date:** 2026-11-10

This lab contains two distinct references: an offline photo/depth focus sweep,
and an independent headless Vulkan capture of ArtifactStudio's GPU
`DepthOfFieldPass`. The GPU fixture uses known metric-depth bands and a
resolution chart; it does not require the Artifact application window.

## Recreate the R&D capture

From the repository root, with Python, NumPy, and OpenCV installed:

```powershell
python experiments/DepthOfFieldLab/capture_depth_sweep.py
```

Output: `experiments/DepthOfFieldLab/captures/depth-sweep-rd1/`

- `dof-focus-sweep.png`: source and far / subject / near focus comparison.
- `far-focus.png`, `mid-focus.png`, `near-focus.png`: full-resolution variants.
- `capture-metadata.json`: input hashes, relative-depth normalization, focus
  values, and limitations.

The focus values are normalized against the 2nd–98th percentile of the
monocular depth estimate (`higher=near`). Blur radius is an illustrative
relative CoC mapped to an eight-level Gaussian pyramid. The image has no metric
depth, aperture-shaped bokeh, physically calibrated lens, temporal behavior,
or proper depth-layer compositing. Visible cross-depth color bleed is useful
as a diagnostic target, not evidence of production quality or a renderer
regression.

## Capture the independent GPU test

Build the existing test target in an already configured build tree, then run
the capture checker:

```powershell
cmake --build build --config Debug --target ArtifactPostProcessPassContractTest
python tests/depth_of_field_visual/check_depth_of_field_image.py `
  --test-exe build/tests/Artifact/Debug/ArtifactPostProcessPassContractTest.exe `
  --output-dir experiments/DepthOfFieldLab/captures/gpu-dof-r1
```

The GPU test sends a synthetic three-plane chart through the real pass, reads
back its output, saves input/output PPM images, and verifies that the known
focus plane retains more chart contrast than the near and far planes. The
checker creates `dof-gpu-comparison.png` (input left, GPU output right) and
`dof-gpu-metrics.json`. This is GPU-pass evidence, not a full 3D composition
or production-render acceptance capture; bokeh shape, transparency, HDR,
temporal stability, backend parity, and performance remain separate checks.

## Next capture stage

R1 still needs an Artifact composition capture using a controlled 3D scene
with a point-light bokeh target, silhouette edges, HDR, and transparent output.
Capture GPU backend, resolution, camera settings, and frame timing with each
run. The independent chart fixture verifies the GPU pass response, but does
not replace that production integration capture or timing.
