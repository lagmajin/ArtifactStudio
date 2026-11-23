# Cinematic Looks Lab — image sources

**最終更新:** 2026-11-16

Photographic plates used by the lab are kept with this project. Source pages identify each work as CC0 1.0. The original downloads and normalized working copies are retained so the working images can be regenerated; their SHA-256 values are also emitted in each run manifest.

## Portrait 2024

- Source page: [Wikimedia Commons — Portrait 2024.jpg](https://commons.wikimedia.org/wiki/File:Portrait_2024.jpg)
- Description page lists CC0 1.0 and uploader/copyright holder `MarieClaudeGendron2`; embedded camera metadata credits Louis Rivest-Henault as photographer.
- Original: `portrait-2024-source.jpg`, 5184 × 3456, SHA-256 `50b3f431675b55d53fefa13451d734af13988c43585a5236c39b2644a47a4f07`.
- Working copy: `portrait-2024-1536.jpg`, 1536 × 1024, SHA-256 `9c14e3c0206ad6fe080c349368bf29219071892002fc01f1b6fc947af3bd40e2`. Created by proportional Pillow LANCZOS resize, JPEG quality 95, 4:4:4.
- Evaluation role: face/skin, hair, window highlights, and dark foreground.

## Landscape of a wild forest

- Source page: [Wikimedia Commons — Landscape of a wild forest.jpg](https://commons.wikimedia.org/wiki/File:Landscape_of_a_wild_forest.jpg)
- Photographer/uploader: SpessiG; the description page lists CC0 1.0.
- Original: `forest-wild-source.jpg`, 3136 × 2091, SHA-256 `2f9f01326230bf9166eec430eb6d14cac69412d6cf6e2ecc7a25970683e4ddea`.
- Working copy: `forest-wild-1536.jpg`, 1536 × 1024, SHA-256 `b417d6ffe3b494ee80906d6b433544ee7a2afa51c1557dcdf3b8feae9536ce6c`. Created by proportional Pillow LANCZOS resize, JPEG quality 95, 4:4:4.
- Evaluation role: green foliage, deep shadows, fine branches, and sunlit highlights.

The output look is not a ground-truth reference for either photograph. CC0 licensing describes the image copyright license stated on the source page; the lab does not infer photographic or colorimetric ground truth from it.

## Synthetic alpha-association chart

- `alpha-association-chart.png` is generated locally by `make_alpha_association_fixture.py`; it has no external source or license dependency.
- The 1280 × 768 RGBA PNG stores straight/unassociated RGB and contains eight constant-color rows with six fixed alpha patches plus a horizontal 0–255 alpha ramp. Color values, byte levels, and SHA-256 are in `alpha-association-chart.json`.
- This is a diagnostic input for future Composition / Render Queue captures, not a reference output or ground truth. No Artifact runtime capture has been made from it yet.
