# Motion Blur Image Quality Check

**最終更新:** 2026-11-04

This independent headless GPU check runs only the motion-blur multi-layer fixture,
writes the GPU input and output as PPM images, and applies deterministic image
recognition checks to the output. It checks that the static green mark survives,
the red object leaves a horizontal trail, the yellow object leaves a vertical
trail, and the unblurred input does not already contain the red trail.

Build `ArtifactPostProcessPassContractTest` using the repository's configured
build tree, then run the checker directly:

```powershell
python tests/motion_blur_visual/check_motion_blur_image.py `
  --test-exe build/tests/Artifact/Debug/ArtifactPostProcessPassContractTest.exe `
  --output-dir artifacts/motion-blur-quality
```

The executable argument is intentionally explicit so the check can use an
existing build without configuring CMake or building the application. On a host
without a headless Vulkan device, the underlying GoogleTest case is skipped and
the image check cannot produce a pass. The output directory contains
`motion-blur-input.ppm` and `motion-blur-output.ppm`; PPM is used to keep image
export dependency-free. The checks use the rendered pixels directly and do not
compare against a hand-authored golden image.
