# Motion Blur Image Quality Check

**最終更新:** 2026-11-05

This independent headless GPU check runs two motion-blur fixtures, writes the GPU
input and output as PPM images, and applies deterministic image recognition
checks to the output. The multi-layer fixture checks that the static green mark
survives, the red object leaves a horizontal trail, the yellow object leaves a
vertical trail, and the unblurred input does not already contain the red trail.
The rotation fixture uses a tangential velocity field: the centre marker must
stay sharp, a left-edge marker must smear vertically above and below, and no
horizontal trail may appear.

The rotation fixture writes `motion-blur-rotation-input.ppm` and
`motion-blur-rotation-output.ppm` next to the multi-layer pair.

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
