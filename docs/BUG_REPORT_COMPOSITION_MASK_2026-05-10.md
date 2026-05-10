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

## Ongoing Considerations
- **Mask Performance**: Mask rasterization heavily relies on CPU-side `cv::Mat` conversions and polygon filling. If drag/drop latency occurs during mask manipulation, a GPU-accelerated mask rasterization strategy might be required in the future.
- **Multiple Masks**: Currently, multiple masks on a layer are correctly composed using max (Add), multiply (Intersect), etc., inside `compositeAlphaMask`. Ensure mask mode combinations remain stable under different blending conditions.
