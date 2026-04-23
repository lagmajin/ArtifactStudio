# Hue/Saturation Effect Implementation Plan

## Overview

This document outlines the implementation plan for the Hue/Saturation color grading effect, following ArtifactStudio's technical guidelines for color processing, numerical representation, alpha handling, UI design, and quality assurance.

## Color Space Specification

### Input Color Space
- **Input**: sRGB (gamma-encoded) or linear RGB depending on source material
- **Detection**: Automatically detect based on image metadata or assume sRGB for QImage inputs

### Working Color Space
- **Primary Working Space**: Linear RGB (scene-referred)
- **Conversion**: sRGB inputs are converted to linear RGB using sRGB EOTF (2.4 gamma)
- **Operations**: All color adjustments performed in linear RGB space
- **Perceptual Operations**: Hue adjustments remain in HSL space (derived from linear RGB)

### Display Color Space
- **Output**: sRGB (gamma-encoded) for display/rendering
- **Conversion**: Linear RGB results converted back to sRGB using sRGB OETF

## Numerical Representation

### Precision Requirements
- **Internal Calculations**: float32 (32-bit floating point)
- **Range**: No 0..1 clamping during intermediate calculations
- **HDR Support**: Allow values > 1.0 for highlight preservation
- **Soft Clipping**: Applied only at final output stage if needed (reason: prevent display overflow)

### Processing Pipeline
```
Input (sRGB) → Linear RGB → HSL Conversion → Adjustments → RGB Conversion → Output (sRGB)
```

## Alpha Handling

### Premultiplied Alpha Support
- **Detection**: Check if input has premultiplied alpha
- **Processing Order**:
  1. Unpremultiply: `color = color / alpha` (avoid division by zero)
  2. Color Correction: Apply HSL adjustments to RGB
  3. Premultiply: `color = color * alpha`
- **Alpha Zero Handling**: For alpha ≈ 0, skip color correction to prevent NaN/inf

### Alpha Channel Behavior
- **Hue/Saturation Adjustments**: Applied only to RGB, alpha unchanged
- **Lightness Adjustments**: May affect perceived alpha, but alpha channel preserved

## UI Parameter Mapping

### Parameter Ranges and Units
- **Master Hue**: -180° to +180° (degrees)
- **Master Saturation**: -100% to +100% (percentage)
- **Master Lightness**: -100% to +100% (percentage)
- **Channel Adjustments**: Same ranges as master

### Non-Linear Mapping
- **Low Range Sensitivity**: Use exponential mapping for fine control at low values
- **Formula**: `internal_value = sign(ui_value) * pow(abs(ui_value) / max_value, gamma)`
- **Gamma**: 2.2 for perceptual response

### Special Parameters
- **Exposure**: Convert to stops (2^x scale)
- **Contrast**: Implement with pivot point (midtone preservation)

## Quality Assurance

### CPU/GPU Consistency
- **Formula Alignment**: Identical mathematical operations
- **Constants**: Same floating-point constants
- **Processing Order**: Same sequence of operations
- **Precision**: float32 throughout

### Numerical Stability
- **Fast Math**: Disabled (affects precision)
- **Half Precision**: Avoided for intermediate calculations
- **Hue Protection**: In low saturation areas (< 0.01), maintain original hue to prevent artifacts

### Test Viewpoints
- **Color Accuracy**: Verify against known color transformation matrices
- **Edge Cases**: Pure colors, grayscale, high contrast images
- **Alpha Blending**: Premultiplied vs straight alpha results
- **HDR Content**: Values > 1.0 preservation
- **Performance**: CPU vs GPU timing comparison

## Processing Order Detail

### Per-Pixel Processing
1. **Input Validation**: Check alpha, detect color space
2. **Alpha Unpremultiply** (if premultiplied): `rgb = rgb / max(alpha, epsilon)`
3. **Color Space Conversion**: sRGB → Linear RGB (if needed)
4. **HSL Conversion**: Linear RGB → HSL
5. **Master Adjustments**:
   - Hue rotation
   - Saturation scaling
   - Lightness adjustment
6. **Channel Adjustments** (based on hue ranges):
   - Red: 330°-30°
   - Yellow: 30°-90°
   - Green: 90°-150°
   - Cyan: 150°-210°
   - Blue: 210°-270°
   - Magenta: 270°-330°
7. **HSL → RGB Conversion**: HSL → Linear RGB
8. **Color Space Conversion**: Linear RGB → sRGB (for display)
9. **Alpha Premultiply** (if originally premultiplied): `rgb = rgb * alpha`
10. **Output Clamping**: Soft clamp to 0..1 range only if required for display

### Clamp Positions
- **No Clamps**: During HSL adjustments and RGB conversions
- **Soft Clamp**: Final output only (reason: preserve HDR information until display)

## Implementation Artifacts

### CPU Implementation
- **Location**: `ArtifactCore/src/ImageProcessing/ColorTransform/HueSaturation.cppm`
- **Class**: `HueSaturationEffect`
- **Methods**: `apply()`, `applyPixel()`

### GPU Implementation (Planned)
- **Location**: DiligentEngine shader pipeline
- **Shader**: HLSL compute shader
- **Integration**: Via DiligentFX post-processing

### UI Integration
- **Location**: Inspector effect panel
- **Controls**: Sliders with non-linear mapping
- **Preview**: Real-time GPU preview

## Validation Checklist

- [ ] Color space conversions accurate (±0.001)
- [ ] HSL roundtrip conversion lossless
- [ ] Alpha handling preserves premultiplied state
- [ ] CPU/GPU results identical (float32 precision)
- [ ] Low saturation hue stability
- [ ] HDR value preservation (>1.0)
- [ ] Performance acceptable (<5ms for 4K frame)
- [ ] UI parameter mapping smooth and intuitive