# Professional Media Materials Support (2026-07-16)

**Status:** Phase 1〜2 implemented / Phase 3〜5 partial and runtime validation pending  
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

## 2026-07-25 実装監査

- `RawImage`／`ImageImporter`／`SourceInterpretOverride`／`FootageInterpretService` に、source color space、transfer、primaries、bit depth の保持と Auto／Linear／sRGB／ACES 系の明示的解釈設定が存在する。
- importer が入力画素を暗黙に display transform せず、EXR float／16-bit 読み込みを保持する境界は確認できる。
- `OCIOConfig`、ACES／transfer／gamut の基盤は存在するが、working-space conversion が effects／mask／compositing 前に一度だけ適用される統合経路、および display/output transform との完全な分離はコード監査だけでは確認できない。
- EXR／HDR／10／12／16-bit／log source の実ファイル round-trip、metadata retention、export 検証は未実施である。
- よってステータスは `Phase 1〜2 implemented / Phase 3〜5 partial and runtime validation pending` のままとする。

## 2026-07-29 実装更新

- Asset Browser に加えて Project View の `Interpret Footage` 導線にも、既存の `FootageInterpretService` を使った Input color space / Input transfer の明示選択を追加した。
- `Auto` は空の override として保存し、Linear / sRGB / ACEScc / ACEScct / Rec.2020 と Linear / sRGB / LogC / S-Log3 / PQ / HLG を明示値として保存する。
- importer の画素変換は変更せず、選択値は既存の FootageItem 解釈メタデータへ保持する。
- working-space conversion、display/output transform、実ファイル round-trip は引き続き未完了・未検証である。
