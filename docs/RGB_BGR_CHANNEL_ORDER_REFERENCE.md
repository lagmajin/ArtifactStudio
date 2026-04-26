# RGB/BGR Channel Order Conventions - ArtifactStudio Reference

**Purpose**: Centralized reference for channel order conventions across ArtifactStudio codebase to prevent AI-induced color channel swaps.

**Last Updated**: 2026-04-27

## Quick Reference

| System         | Format         | Channel Order | Notes |
|----------------|----------------|---------------|-------|
| **Qt/QImage**  | ARGB32/RGBA    | RGBA (native) | Endian-dependent on platform; typically stored as 0xAARRGGBB in memory |
| **OpenCV 8-bit** | CV_8UC3/CV_8UC4 | BGR(A) | OpenCV default; color values in BGR order |
| **OpenCV float** | CV_32FC4       | Generic | No built-in assumption; depends on creation path |
| **Direct3D/Diligent** | DXGI formats | RGBA | GPU convention; little-endian on x86 = 0xAABBGGRR memory |
| **FFmpeg**     | AVFrame        | Depends on format | Typically RGB for most codecs; check `AVFrame::format` |

## Channel Order by Component

### ArtifactCore (C++)

#### Text/Font Rendering
- **GlyphAtlas**: Stores glyph bitmaps as single-channel grayscale (L8)
- **TextRenderer**: Output format matches composition format (typically RGBA)
- **No BGR usage**: Font system is format-agnostic

#### Audio/Video Codec
- **FFmpegDecoder**: Outputs RGB frames (converted from codec native)
- **FFmpegEncoder**: Accepts RGB input, converts to codec format on encode
- **Note**: FFmpeg C headers use RGB order for color spaces

#### Image Processing
- **ColorSpace conversions** (LabColor, XYZColor, etc.): Use RGB order internally
- **OpenCV operations**: Use BGR order (OpenCV convention)
- **Conversion points**: Explicit `cv::cvtColor(src, dst, cv::COLOR_BGR2RGB)` required

#### Mask Rendering
- **Mask generation**: CV_32FC1 single-channel alpha (order N/A)
- **Mask application**: Multiplies into alpha channel of target image
- **Target image format**: CV_32FC4 with memory layout RGBA
- **See**: `Artifact/src/Mask/LayerMask.cppm:153-173`

### Artifact (UI/Rendering)

#### Composition Rendering
- **Intermediate buffers**: Typically RGBA float format (DiligentEngine convention)
- **QImage output**: Converts from RGBA to platform-native ARGB32
- **No implicit BGR**: All GPU→QImage conversions are explicit RGBA→ARGB

#### Layer Rendering
- **Video layers**: Decode as RGB, store in RGBA buffers
- **Image layers**: Load with OpenCV (BGR), convert to RGB on GPU upload
- **Text layers**: Render directly to RGBA framebuffer

#### Screenshot/Export
- **PNG export**: Uses QImage native format (RGBA on little-endian)
- **JPEG export**: Uses libjpeg convention (RGB)
- **Conversion**: Explicit in export pipeline

## Conversion Points (CRITICAL)

### ❌ DO NOT DO

```cpp
// WRONG: Assuming channel order without verification
cv::Mat from_opencv = /* BGR image */;
upload_to_gpu(from_opencv.data);  // Will show red/blue swapped on GPU

// WRONG: Mixing QImage and OpenCV without conversion
QImage qi = /* RGBA image */;
cv::Mat mat(qi.height(), qi.width(), CV_8UC4, qi.bits());
// mat will be interpreted as BGR, colors will be wrong
```

### ✅ DO THIS

```cpp
// CORRECT: OpenCV BGR → GPU RGBA
cv::Mat bgr_image = cv::imread("photo.jpg");  // BGR
cv::Mat rgb_image;
cv::cvtColor(bgr_image, rgb_image, cv::COLOR_BGR2RGB);
upload_to_gpu_rgba(rgb_image.data);

// CORRECT: QImage → GPU RGBA
QImage qi = QImage("photo.png");  // RGBA
cv::Mat mat;
if (qi.format() == QImage::Format_RGBA8888) {
    mat = cv::Mat(qi.height(), qi.width(), CV_8UC4, qi.bits());
} else {
    // Convert to RGBA if needed
    qi = qi.convertToFormat(QImage::Format_RGBA8888);
    mat = cv::Mat(qi.height(), qi.width(), CV_8UC4, qi.bits());
}
upload_to_gpu_rgba(mat.data);
```

## Format Detection Guide

### Determining Actual Channel Order

**In C++:**
```cpp
// For QImage
if (image.format() == QImage::Format_ARGB32) {
    // Stored as 0xAARRGGBB in memory (little-endian on x86)
    // Sequential bytes: B, G, R, A
}

// For OpenCV
if (image.channels() == 3 && image.type() == CV_8UC3) {
    // OpenCV default: BGR
    // For compatibility, convert: cv::cvtColor(image, out, cv::COLOR_BGR2RGB)
}

// For GPU texture
if (format == DXGI_FORMAT_R8G8B8A8_UNORM) {
    // Direct3D: RGBA
    // Sequential bytes: R, G, B, A
}
```

## Testing Channel Order

### Verification Steps

1. **Create test pattern**: Red square in top-left
   - Red = (255, 0, 0) in RGB
   - Red = (0, 0, 255) in BGR

2. **Save/display** using each backend:
   - Verify red appears as red (not cyan)
   - If wrong: channel swap occurred

3. **Log channel values** at conversion:
   ```cpp
   auto [r, g, b, a] = get_pixel_at(0, 0);
   qDebug() << "Pixel[0,0]: R=" << r << "G=" << g << "B=" << b;
   ```

## Common Issues & Fixes

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Colors appear swapped (R↔B) | Channel order mismatch at format conversion | Add explicit cv::cvtColor or cv::merge in correct order |
| Image appears monochrome (grayscale) | Wrong channel split/merge or single channel treated as RGB | Verify cv::split() result count; check channel merge order |
| Specific color channel missing | Wrong channel index in split/merge | Add logging for channel indices; verify against format spec |
| Alpha channel wrong | Confusion between premultiplied/straight alpha | Check blend mode; verify alpha range [0,1] or [0,255] |

## FFmpeg Integration

**FFmpeg color space conventions:**
- Most codecs output in **RGB** (not BGR)
- YUV/YCbCr codecs are explicitly converted to RGB on decode
- Conversion: Use `libswscale` or `av_frame_get_buffer()` with RGB format

**ArtifactCore FFmpeg usage:**
```cpp
// Correct: FFmpeg → RGB → GPU
AVFrame* decoded = /* from FFmpeg decoder */;
// decoded->format is typically AV_PIX_FMT_RGB24 or AV_PIX_FMT_RGBA
// Upload directly (or convert if different format):
if (decoded->format == AV_PIX_FMT_RGB24) {
    upload_rgb_to_gpu(decoded->data[0], decoded->linesize[0]);
}
```

## Per-Module Summary

### ArtifactCore
- **Color theory**: RGB order (internal)
- **OpenCV integration**: BGR order (explicit cv::cvtColor)
- **FFmpeg integration**: RGB order
- **GPU output**: RGBA order (Direct3D)

### Artifact
- **GPU rendering**: RGBA order
- **QImage I/O**: Platform native (typically ARGB on Windows)
- **Export**: Format-specific (PNG=RGBA, JPEG=RGB)

### ArtifactWidgets
- **Qt convention**: ARGB/RGBA (platform native)
- **No OpenCV usage**: No BGR concerns

## References

- **OpenCV Channel Order**: https://docs.opencv.org/master/d3/d63/classcv_1_1Mat.html
- **Direct3D Format Specs**: https://docs.microsoft.com/en-us/windows/win32/direct3d11/texture1d-subresource-addressing
- **Qt QImage Formats**: https://doc.qt.io/qt-6/qimage.html#Format-enum
- **FFmpeg Color Spaces**: https://ffmpeg.org/doxygen/trunk/pixfmt_8h.html

---

**Purpose**: Prevent AI color channel confusion. Always reference this when working with cross-backend image formats.  
**Maintainer**: Copilot  
**Last Verified**: 2026-04-27
