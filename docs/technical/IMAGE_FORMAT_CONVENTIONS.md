# Image Format & Channel Order Conventions

**最終更新:** 2026-10-17

**Critical Reference Document** - Used by multiple clients (ArtifactStudio, preview engines, external tools)

This document mainly explains the **current / legacy conversion reality**.
For the canonical direction the renderer should converge toward, see:

- [RENDER_FORMAT_CONTRACT_2026-05-16.md](RENDER_FORMAT_CONTRACT_2026-05-16.md)
- [IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md](IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md)

## TL;DR: Channel Order Quick Reference

| System | Format | Memory Layout | Alpha Position | Notes |
|--------|--------|---------------|-----------------|-------|
| **Qt (QImage)** | RGB888 | **RGB** (left-to-right) | N/A (separate) | Qt default; used for UI and display |
| **Qt (Format_ARGB32)** | ARGB32 | **BGRA** on Little Endian | channels[3] | Confusing name; actually stores BGRA on Windows/x86 |
| **OpenCV** | CV_8UC3 | **BGR** (BGR, not RGB) | N/A | OpenCV's default convention; most functions assume BGR |
| **OpenCV** | CV_8UC4 / CV_32FC4 | **Producer-defined** | commonly channels[3]=A | OpenCV 4-channel matrices do not encode RGBA/BGRA semantics; the producer or descriptor defines RGB order |
| **DiligentEngine** | RGBA16_FLOAT / RGBA32_FLOAT | **Artifact image values: BGRA** | Last component | Resource format is RGBA; Artifact shader convention determines image value order |
| **Artifact CPU float image** | `ImageF32x4_RGBA` / `ImageF16x4` | **BGRA** (`B, G, R, A`) | last component | Canonical CPU backing order for Float32 and Float16; descriptor must report BGRA |
| **OpenCV CV_8UC4 / CV_32FC4** | 4-channel | **Producer-defined** | commonly channels[3]=A | Matrix type alone does not encode channel semantics; Artifact-owned image processing uses BGRA |

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

### BGRA image value order in Diligent float textures

Artifact image-pipeline float textures use Diligent `RGBA16_FLOAT` / `RGBA32_FLOAT` resources, but their four values are interpreted as `[B, G, R, A]`. This lets BGRA CPU image backing upload without a full-image channel reorder. It is an Artifact shader convention; it does not claim Diligent provides a BGRA float resource format. External shaders that require RGBA must use an explicit boundary adapter.

Shaders in this image pipeline must treat `.x/.y/.z/.w` as `B/G/R/A`. Convert only when an external RGBA shader/API or presentation boundary requires RGBA semantics. Do not infer channel order from the `RGBA16_FLOAT` / `RGBA32_FLOAT` resource format name.

## ImageF32x4_RGBA Compatibility Behavior

**Class Name**: `ImageF32x4_RGBA`  
**Internal Storage**: CV_32FC4 in fixed BGRA order. `setColorDescriptor()` preserves the BGRA/Float32 storage fields while updating color metadata.
RGBA-named setters accept logical RGBA and normalize to BGRA backing. The no-descriptor OpenCV setter follows OpenCV BGR/BGRA convention. A 4-channel OpenCV matrix alone does not identify its color order; explicit descriptors define the source when it is not BGRA.

The canonical CPU float-image backing order is BGRA for both Float32 and Float16. `ImageF32x4_RGBA` remains a logical/API compatibility name; raw `rgba32fData()` storage is BGRA. Artifact image-pipeline GPU float textures use the same BGRA value order over RGBA16F/32F resources, so their upload/readback does not reorder channels.

### When Working with ImageF32x4_RGBA

```cpp
ImageF32x4_RGBA img;
cv::Mat backing = img.toCVMat();              // physical [B, G, R, A], canonical CPU order
cv::Mat bgra = img.toCanonicalBGRA32FC4();    // physical [B, G, R, A], canonical CPU order
cv::Mat rgba = img.toCanonicalRGBA32FC4();    // explicit external RGBA API boundary [R, G, B, A]
```

Do not infer order from `CV_32FC4`, the image class name, or channel count. Artifact-owned float images are BGRA; arbitrary OpenCV matrices remain producer-defined. Use the descriptor or an explicitly named conversion.

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
ImageF32x4_RGBA + SurfaceColorDescriptor
        ↓
GPU upload boundary (validate BGRA descriptor; preserve BGRA value order)
        ↓
DiligentEngine RGBA16F/32F texture (Artifact values [B, G, R, A])
        ↓
Artifact image shaders (expect BGRA value order)
```

**Key Point**: CPU Float32/Float16 backing and Artifact image-pipeline GPU float values are BGRA. The Diligent resource format remains RGBA16F/32F because no float BGRA format is available; unknown or unsupported layouts must be rejected rather than relabeled.

## Common Mistakes

| ❌ Mistake | ✅ Correct | Impact |
|-----------|-----------|--------|
| Assuming `rgba32fData()` is true [R,G,B,A] | Treat CPU image backing as [B,G,R,A], or use descriptor-aware logical RGBA views | Red/blue swaps in effects |
| Forgetting RGB→BGR conversion in OpenCV | Use cv::COLOR_RGB2BGR | Wrong colors in rendering |
| Inferring shader value order from an RGBA float resource name | Follow the declared Artifact BGRA image convention | Red/blue swap in effects |
| Assuming cv::split() preserves input format | It does (BGRA in = BGRA out) | Alpha applied to wrong channel |
| Using channels[3] assuming RGB order | channels[3] is Alpha in BGRA ✅ | Works correctly, but confusing to read |

## Checklist: Adding New Image Processing Code

- [ ] Identify input format (Qt RGB888? OpenCV BGR? GPU RGBA?)
- [ ] Identify output format (Qt RGB888? OpenCV BGR? GPU RGBA?)
- [ ] If input is Qt RGB888 → OpenCV: use `cv::COLOR_RGB2BGR`
- [ ] If input is OpenCV → Qt RGB888: use `cv::COLOR_BGR2RGB`
- [ ] Artifact CPU Float32/Float16 image storage is BGRA; preserve BGRA descriptor
- [ ] If using arbitrary OpenCV input: inspect its producer contract or pass an explicit descriptor
- [ ] Artifact image-pipeline GPU float textures preserve BGRA values; convert only at explicitly RGBA external/presentation boundaries
- [ ] Document the conversion in comments (copy-paste from this doc if needed)
- [ ] Add test case: RGB test image → verify colors correct in output

## References

- **OpenCV Documentation**: https://docs.opencv.org/master/d3/d63/classcv_1_1Mat.html
- **Qt Documentation**: https://doc.qt.io/qt-6/qimage-formats.html
- **DiligentEngine DXGI Formats**: https://github.com/DiligentGraphics/DiligentEngine/blob/master/Common/interface/Platforms/Win32/Win32Window.hpp
- **BGRA vs RGBA**: Common confusion in computer graphics; always verify with docs

---

**Last Updated**: 2026-10-16
**Author**: Development Team  
**Applies To**: ArtifactStudio core, ArtifactCore, Artifact module, External clients using libArtifact
