# Professional Media Materials Support (2026-07-16)

**Status:** Phase 1 implemented / Phase 2 UI foundation implemented  
**ID:** M-PRO-MEDIA-1

## Goal

Make professional still-image and image-sequence sources (EXR/HDR, wide-gamut,
log-encoded and high-bit-depth files) survive import with enough information for
explicit interpretation and later display conversion.

## Scope

- OIIO-based ingest for EXR/HDR and high-bit-depth images
- preservation of source color space, transfer function, primaries and bit depth
- explicit log/HDR classification at the ingest boundary
- linear working-space conversion before effects and compositing
- source interpretation UI and per-source override
- OCIO/ACES input and display transforms
- validation with EXR, HDR, 10/12/16-bit and representative log sources

## Current implementation

- `RawImage` now carries professional-source metadata without changing its raw
  pixel contract.
- `ImageImporter` records OIIO color-space and transfer metadata, preserves EXR
  float and common TIFF/PNG 16-bit reads, and classifies likely log/HDR sources.
  This is diagnostic/interpretive metadata only; it does not silently transform
  pixels.

## Phases

1. **Ingest metadata (implemented):** preserve source metadata and classify log/HDR.
2. **Source interpretation:** add explicit input-space selection and an
   `Auto / Linear / sRGB / ACEScc / ACEScct / Rec.2020` override. The existing
   `SourceInterpretOverride` now carries explicit input color-space and transfer
   function choices without changing importer pixels, and the existing
   `Interpret Footage` dialog plus `FootageItem` / `FootageInterpretService`
   now retain/apply those choices.
3. **Working-space conversion:** convert once to the project linear working space
   before effects, masks and compositing.
4. **Display/output:** apply OCIO/ACES display transforms independently from the
   working image; preserve HDR values and alpha semantics.
5. **Validation/export:** verify round trips, metadata retention and export of
   EXR/HDR/16-bit outputs.

## Non-goals

- guessing a camera log curve when the source metadata is absent
- applying a display transform inside the importer
- replacing the existing QImage compatibility path in one change

## Acceptance criteria

- importing a professional source exposes its detected metadata;
- raw values are unchanged until an explicit interpretation is selected;
- log/HDR sources do not get clamped to 8-bit during ingest;
- working-space and display-space transforms remain separate.
