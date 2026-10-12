# Layer mask visual contract test

**最終更新:** 2026-10-12

This independent test target exercises `Artifact::LayerMask::compositeAlphaMask`
and `MaskPath::rasterizeToAlpha` using generated polygon masks. Each case is
checked against numeric expectations and written as an 8-bit grayscale PNG. A
Python wrapper assembles the planes into a labeled PNG atlas for quick visual
review.

## Build and run

Enable `ARTIFACT_BUILD_TESTS` to register `ArtifactMaskVisualTest` (it is part
of the broad test option because the `Artifact` application library is needed).
The target
links the existing `Artifact` library and uses its production mask modules; it
does not require window or GPU initialization. The implementation path is CPU
rasterization through OpenCV. `ArtifactMaskVisualTest` is separately selectable
from the rest of the tests.

Run the executable with an output directory argument to get ten individual PNG
alpha planes:

```powershell
MaskVisualTest.exe artifacts/mask-visual
```

Create a labeled contact sheet from those PNG files (requires Pillow):

```powershell
python tests/mask_visual/run_mask_visual_test.py path/to/MaskVisualTest.exe --output artifacts/mask-visual
```

The ten cases cover Add, Subtract, Intersect, Difference, opacity, positive and
negative expansion, feathering, inversion, and a four-path parameter matrix.
The executable returns nonzero on a failed contract assertion. The wrapper
preserves the individual PNG files and writes `mask-visual-atlas.png` beside
them.
