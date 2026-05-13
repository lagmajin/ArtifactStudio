# Composition Mask Rendering Investigation - 2026-05-10

## Symptom 1: Blank Screen (Dark Gray / Clear Color)
When a mask was created and completed, the entire composition editor turned blank (displaying only the default clear color).

### Root Cause
LOD downsampling resulted in a smaller target buffer (`mat`), but the CPU rasterizer for the mask (`MaskPath::rasterizeToAlpha`) used unscaled local coordinates. As a result, the mask path was drawn completely out of bounds (rendering as fully transparent black), which caused the entire layer to become transparent.

### Fix
Added `scaleX` and `scaleY` parameters down the mask rasterization pipeline. The layer's `localBounds` size compared to the rendering buffer size is used to compute the correct scale factors. `MaskPath` now scales all vertices and expansion/feathering offsets correctly before passing them to OpenCV's `fillPoly`.

## Symptom 2: Area Outside Mask Not Hidden
After fixing Symptom 1, the mask path itself displayed correctly, but the area *outside* the mask was not hidden (it remained fully visible instead of becoming transparent).

### Root Cause
When masking layers loaded from standard image formats (such as JPEGs), `QImage` formats like `Format_RGB32` or `Format_RGB888` were parsed by `CvUtils::qImageToCvMat` as 3-channel `CV_8UC3` (BGR) matrices. 
The rasterization pipeline then called `mat.convertTo(mat, CV_32FC4, 1.0/255.0)`, which OpenCV interpreted as "convert depth to 32F, but keep 3 channels" (resulting in `CV_32FC3`).
Later, `LayerMask::applyToImage` executed `cv::split(img, channels)`. Finding only 3 channels, it bypassed the alpha channel multiplication block entirely (`if (channels.size() >= 4)`). Thus, the layer's image was completely unaffected by the generated mask.

### Fix
Modified `buildRasterizedSurfaceBuffer` in `ArtifactCompositionRenderController.cppm` to explicitly guarantee a 4-channel matrix before calling `convertTo`. If `mat.channels() == 3`, it calls `cv::cvtColor(mat, mat, cv::COLOR_BGR2BGRA)` (and handles grayscale formats similarly) to ensure an alpha channel exists. This forces the matrix to `CV_32FC4` and allows `applyToImage` to successfully premultiply the RGB and Alpha channels by the mask.


## Symptom 3: Color Corruption & Incorrect Alpha When Using GPU Texture Cache Path

After fixing Symptoms 1 and 2, the mask works correctly through the CPU `QImage` path. However, when the GPU texture cache path is active (i.e., `layerUsesGpuTextureCacheForCompositionView` returns true), masked layers appeared with **severely corrupted colors and incorrect alpha** (often fully opaque or garbage color output).

### Root Cause — Two interrelated bugs

**Bug A — Wrong stride for float upload (`GPUTextureCacheManager.cppm`)**  
`makeUploadImageData` took the `float* rgba32fData()` pointer and set:
```cpp
upload.stride = width * 4 * sizeof(float);  // = width * 16 bytes
```
Then passed this raw float data to `CreateTexture` with `textureFormat_ = TEX_FORMAT_RGBA8_UNORM_SRGB` (4 bytes/pixel).  
Result: the GPU driver interpreted 16-byte-per-pixel float data as 4-byte-per-pixel RGBA8, causing every pixel to read garbage values.

**Bug B — BGRA channel order mismatch (both `GPUTextureCacheManager.cppm` and `PrimitiveRenderer2D.cppm`)**  
`ImageF32x4_RGBA` stores data internally in **BGRA order** (inherited from `qImageToCvMat` which yields `CV_8UC4/BGRA` from `Format_ARGB32`). Both upload sites read the float channels as if they were in RGBA order, resulting in red and blue channels being swapped in the rendered output.

### Fix — `GPUTextureCacheManager.cppm` (`makeUploadImageData`)
Replaced the raw `memcpy` of float data with a pixel-by-pixel conversion loop that:
1. Reads channels in **BGRA** order from the source float array.
2. Writes them in **RGBA** order as `uint8_t` values.
3. Sets `stride = width * 4` (bytes, not floats) to match `RGBA8_UNORM` format.

### Fix — `PrimitiveRenderer2D.cppm` (`drawSpriteTransformed(ImageF32x4_RGBA)`)
Applied the same BGRA→RGBA channel reorder in the inline upload loop. Variable names renamed from `r,g,b,a` to `srcR,srcG,srcB,srcA` (with `srcB` at index 0 and `srcR` at index 2) to make the BGRA source layout explicit.

### Impact
These bugs only manifested when `layerUsesGpuTextureCacheForCompositionView` returned `true` for the layer. CPU-path rendering (through `toQImage()` → `drawSpriteTransformed(QImage)`) was unaffected because Qt's `convertToFormat(RGBA8888)` correctly handled the BGRA↔RGBA swap internally.

## Ongoing Considerations
- **Mask Performance**: Mask rasterization heavily relies on CPU-side `cv::Mat` conversions and polygon filling. If drag/drop latency occurs during mask manipulation, a GPU-accelerated mask rasterization strategy might be required in the future.
- **Multiple Masks**: Currently, multiple masks on a layer are correctly composed using max (Add), multiply (Intersect), etc., inside `compositeAlphaMask`. Ensure mask mode combinations remain stable under different blending conditions.
- **Channel Order Invariant**: All code paths that consume `ImageF32x4_RGBA::rgba32fData()` must be aware that the internal `cv::Mat` is in **BGRA** channel order (not RGBA), because the mat is sourced from `qImageToCvMat` on `Format_ARGB32` images. A future refactor to store as true RGBA internally (using `cv::COLOR_BGRA2RGBA` in `setFromCVMat` for the `CV_32FC4` case) would eliminate this footgun.
