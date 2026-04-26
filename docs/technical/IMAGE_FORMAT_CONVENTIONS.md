# Image Format & Channel Order Conventions

**Critical Reference Document** - Used by multiple clients (ArtifactStudio, preview engines, external tools)

## TL;DR: Channel Order Quick Reference

| System | Format | Memory Layout | Alpha Position | Notes |
|--------|--------|---------------|-----------------|-------|
| **Qt (QImage)** | RGB888 | **RGB** (left-to-right) | N/A (separate) | Qt default; used for UI and display |
| **Qt (Format_ARGB32)** | ARGB32 | **BGRA** on Little Endian | channels[3] | Confusing name; actually stores BGRA on Windows/x86 |
| **OpenCV** | CV_8UC3 | **BGR** (BGR, not RGB) | N/A | OpenCV's default convention; most functions assume BGR |
| **OpenCV** | CV_8UC4 | **BGRA** | channels[3]=A | 4-channel variant; alpha always at index 3 |
| **OpenCV** | CV_32FC4 | **BGRA** (float) | channels[3]=A | Used in rendering pipeline; alpha at index 3 |
| **DiligentEngine** | DXGI_FORMAT_R8G8B8A8_UNORM | **RGBA** (GPU texture) | Last byte | GPU textures expect RGBA order in VRAM |
| **ImageF32x4_RGBA** | Internal: CV_32FC4 | **BGRA** ⚠️ | channels[3]=A | Name misleading; uses OpenCV BGR internally |

## Detailed Conversions

### Qt RGB888 → OpenCV BGR

```cpp
// QImage Format_RGB888 is Qt's native RGB order
// OpenCV expects BGR by default
cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);  // Convert RGB → BGR
```

**Files implementing this**: `ArtifactCore/include/ImageProcessing/OpenCV/CvUtils.ixx:32-35`

### OpenCV BGR → Qt RGB888

```cpp
cv::Mat bgr;  // From OpenCV
cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
QImage qi(rgb.data, rgb.cols, rgb.rows, QImage::Format_RGB888);
```

**Files implementing this**: `ArtifactCore/include/ImageProcessing/OpenCV/CvUtils.ixx:62-66`

### OpenCV BGRA → DiligentEngine RGBA

When uploading to GPU texture:

```cpp
// OpenCV uses BGRA internally
// DiligentEngine textures (DXGI_FORMAT_R8G8B8A8_UNORM) expect RGBA in VRAM
// Swap B and R channels before GPU upload

// Existing implementation (DO THIS):
std::vector<cv::Mat> channels(4);
cv::split(bgra_mat, channels);                    // [B, G, R, A]
std::swap(channels[0], channels[2]);              // [R, G, B, A]
cv::merge(channels, rgba_mat);                    // → RGBA order
```

**Files implementing this**: 
- `Artifact/src/Render/GPUTextureCacheManager.cppm:67-70`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm:1136` (channel swap on upload)

## ImageF32x4_RGBA Quirk ⚠️

**Class Name**: `ImageF32x4_RGBA`  
**Internal Storage**: CV_32FC4 in **BGRA order**  
**Why**: Inherits OpenCV's BGR convention for performance (zero-copy from OpenCV mats)

### When Working with ImageF32x4_RGBA

```cpp
// This is MISLEADING but correct:
ImageF32x4_RGBA img;
cv::Mat mat = img.toCVMat();        // mat is CV_32FC4 in BGRA layout
std::vector<cv::Mat> channels(4);
cv::split(mat, channels);            // [B, G, R, A]
// ❌ DON'T:  channels[0] = Red      // WRONG! channels[0] is Blue
// ✅ DO:     channels[2] = Red      // Correct! channels[2] is Red
```

**Problem Example**: Mask application  
- Assuming channels[3] = Alpha ✅ (correct)
- If you assumed channels[0] = Red from class name = Bug (channels[0] is Blue in BGR)

**Files affected**:
- `Artifact/src/Mask/LayerMask.cppm:153-170` (mask application)
- `ArtifactCore/src/Image/ImageF32x4_RGBA.cppm:297-343` (CV_32FC4 preservation)

## GPU Texture Upload Pipeline

```
Qt/Composition Surface
        ↓
QImage (RGB888)
        ↓
cv::Mat (BGR via COLOR_RGB2BGR)
        ↓
ImageF32x4_RGBA (internal BGRA CV_32FC4)
        ↓
GPU Texture Upload (SWAP B↔R for RGBA)
        ↓
DiligentEngine Texture (RGBA in VRAM)
        ↓
Shaders (expect RGBA)
```

**Key Point**: BGRA→RGBA swap must happen **at upload boundary**, not before or after.

## Common Mistakes

| ❌ Mistake | ✅ Correct | Impact |
|-----------|-----------|--------|
| Assuming ImageF32x4_RGBA channels are [R,G,B,A] | Channels are [B,G,R,A] | Color inversion in masks, effects |
| Forgetting RGB→BGR conversion in OpenCV | Use cv::COLOR_RGB2BGR | Wrong colors in rendering |
| Not swapping BGRA→RGBA at GPU upload | Swap in GPUTextureCacheManager | Color corruption on texture |
| Assuming cv::split() preserves input format | It does (BGRA in = BGRA out) | Alpha applied to wrong channel |
| Using channels[3] assuming RGB order | channels[3] is Alpha in BGRA ✅ | Works correctly, but confusing to read |

## Checklist: Adding New Image Processing Code

- [ ] Identify input format (Qt RGB888? OpenCV BGR? GPU RGBA?)
- [ ] Identify output format (Qt RGB888? OpenCV BGR? GPU RGBA?)
- [ ] If input is Qt RGB888 → OpenCV: use `cv::COLOR_RGB2BGR`
- [ ] If input is OpenCV → Qt RGB888: use `cv::COLOR_BGR2RGB`
- [ ] If using ImageF32x4_RGBA: **remember channels are BGRA, not RGBA**
- [ ] If uploading to GPU: swap B↔R channels before passing to DiligentEngine
- [ ] Document the conversion in comments (copy-paste from this doc if needed)
- [ ] Add test case: RGB test image → verify colors correct in output

## References

- **OpenCV Documentation**: https://docs.opencv.org/master/d3/d63/classcv_1_1Mat.html
- **Qt Documentation**: https://doc.qt.io/qt-6/qimage-formats.html
- **DiligentEngine DXGI Formats**: https://github.com/DiligentGraphics/DiligentEngine/blob/master/Common/interface/Platforms/Win32/Win32Window.hpp
- **BGRA vs RGBA**: Common confusion in computer graphics; always verify with docs

---

**Last Updated**: 2026-04-26  
**Author**: Development Team  
**Applies To**: ArtifactStudio core, ArtifactCore, Artifact module, External clients using libArtifact
