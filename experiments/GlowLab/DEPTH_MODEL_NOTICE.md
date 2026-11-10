# Depth Model Notice

**最終更新:** 2026-11-10

GlowLab's optional single-image depth estimator uses the int8 ONNX export of **Depth Anything V2 Small** from `onnx-community/depth-anything-v2-small`, pinned to revision `4472b7362082ad9968fee890ca0f1e5aca36b93d`. The model card labels this checkpoint Apache-2.0. The downloaded 27,258,801-byte weight is verified against SHA-256 `01aa7a23de3f4a0ee1a2bb9997e6918104c85a9f95dea46d27b9b3fb0c6b9001`. GlowLab does not redistribute the weights; the CLI downloads them to the user's local model cache after an explicit `--download-model` option.

Model card and files: <https://huggingface.co/onnx-community/depth-anything-v2-small/tree/4472b7362082ad9968fee890ca0f1e5aca36b93d>

The model estimates relative monocular depth. It does not produce calibrated distances, scene normals, visibility, or shadows. Inspect the generated grayscale map and confirm its near/far orientation before using it for flash-front timing. Depth may be wrong around hair, glass, reflective surfaces, dark regions, and occlusion boundaries.
