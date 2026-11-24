# Image-to-Vector R&D — Pre-registered Image Evaluation Plan

**最終更新:** 2026-11-20
**Status:** Registered before any candidate image was produced (rd0). Provisional gates below are fixed from the rd0 baseline **before** rd1 candidate comparison.

This plan follows `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`. The experiment is an offline, deterministic CPU study in Python/NumPy/OpenCV. It does not call Artifact binaries, does not establish product parity, and does not approve an effect, UI, or default.

## 1. Purpose and target

**Goal:** Determine whether raster logos/illustrations can be decomposed into color regions and editable contours with acceptable geometry, hole, thin-feature, color, and alpha-edge fidelity, and which parameterization is the best starting point for a future Auto-Trace implementation.

**In scope**

- Synthetic vector-defined fixtures at 512×320 (primary) and 256×160 / 1024×640 (resolution cases): flat-color badge with a hole, overlapping color regions, nested holes/ring, thin strokes and small dots, semi-transparent region with antialiased edges.
- Offline methods: posterize + color clustering + connected components → contours → polygons; luminance/alpha threshold contour trace (a mirror of the existing `MaskPath::fromAlphaMask` behavior); Bezier-refinement variants in later rounds.
- Region decomposition, contour geometry, hole preservation, thin-feature recall, color accuracy, alpha compositing, vertex counts, and wall time.

**Out of scope (recorded as unverified, not as a pass)**

- Real photographic material, aesthetic preference, and human visual review. rd2 may add CC0 real logos as *external fixtures*, never as ground truth.
- Artifact GPU / Diligent paths, Composition Viewer, Render Queue, preview vs final routing, backend parity.
- Product UI, Undo, shortcut bindings, properties, defaults, effect catalog, and any change to ArtifactCore / Artifact / ArtifactWidgets.
- Video / sequential-frame temporal stability (no time dimension in this study).
- Hot-path allocation behavior of a product implementation. Only offline wall time is measured.

## 2. Comparison targets and their role

| Item | Role | May be called ground truth? |
|---|---|---|
| Vector definition JSON + per-region coverage maps | Authoritative source geometry, color, holes, alpha, and thin-feature labels | **Yes for the synthetic fixtures only** — generated in this repository from known polygons |
| Deterministic antialiased raster `source.png` / `source.npz` | Rasterized vector, the input to every method | Yes as "input identity" reference only |
| `alpha_threshold_trace` | Control: mirror of current `fromAlphaMask` behavior (single-channel threshold → contour → polygon). Demonstrates the color-decomposition gap. | No |
| `posterize_components` | Baseline: color quantization + per-cluster connected components + `findContours(RETR_CCOMP)` + `approxPolyDP` | No |
| `kmeans_color_contours` (rd1) | First candidate: palette clustering over RGB + per-cluster contour hierarchy + Bezier smoothing | No |

No method output is a physical or perceptual reference. Differences are reported against the vector ground truth and against the control, not as a universal ranking.

## 3. Evaluation material

All fixtures are generated deterministically by `generate_fixtures.py` with `seed=20261120`. Provenance: "generated in this repository"; no external asset is used in rd0/rd1. Each file hash is recorded in `captures/rd0/manifest.json`.

| Fixture | Primary content | Fixed ROIs |
|---|---|---|
| `logo_badge` | 4 flat colors, rounded outer shape, inner ring with a hole, sharp corners | `full`, `flat_interior`, `edge_band`, `hole`, `corner` |
| `flat_illustration` | overlapping color regions, near-touching shapes, 3 regions | `full`, `flat_interior`, `edge_band` |
| `nested_holes` | ring with an inner hole hierarchy (outer + hole) | `full`, `hole`, `edge_band` |
| `thin_strokes` | 1–2 px strokes, small dots, sharp corners | `full`, `thin_features`, `edge_band` |
| `alpha_logo` | one region at alpha 0.5, antialiased edges, hidden RGB zeroed | `full`, `edge_band`, `semi_transparent` |
| `logo_badge_256` / `logo_badge_1024` | resolution scaling of `logo_badge` | same as `logo_badge` |

Fixture details (W/H, supersample factor, region polygon coordinates, colors, alpha, ROI rectangles, per-file SHA-256) are stored in `gt_regions.json`, `rois.json`, and the manifest. Processing domain: **sRGB encoded 8-bit values scaled to 0..1** (display-referred). Fixtures store straight (unassociated) alpha; hidden RGB is zeroed where alpha == 0. This is an offline convention and does not imply a product color/alpha contract.

## 4. Fixed image conditions

- Supersample factor `SS=4`; coverage maps downsampled with `INTER_AREA` to fractional per-pixel coverage.
- Primary resolution 512×320; scale cases 256×160 and 1024×640.
- Quantization: `posterize_components` uses k-means with `k` fixed to the fixture's ground-truth color count (this uses the known synthetic palette only to set a deterministic control setting, not to match region identity). `alpha_threshold_trace` uses threshold 0.5.
- Polygon simplification: `cv::approxPolyDP` with epsilon `1.5 px` at 512 width, scaled with resolution (×0.5 at 256, ×2 at 1024), matching the existing `simplificationTolerance=1.5` default at primary scale.
- Minimum region area `16 px` at 512 width, scaled by area with resolution.
- Alpha: methods never modify alpha values; traced polygons are re-rasterized with `SS=4` antialiasing for compositing. Composite tests use `out.rgb·α + bg·(1−α)` over black (0,0,0), mid gray (0.5,0.5,0.5), and white (1,1,1).
- Review PNGs are 8-bit display images and are **not** measurement data. Measurements use float32 coverage/label arrays saved as `.npz`.
- No random seed inside methods. The fixture generator seed is recorded once; identity check regenerates and compares byte-for-byte.

## 5. Required evaluations, regions, and pre-registered gates

Each row is measured on the 512×320 fixtures unless stated. Gates are review gates for **this fixture set**, not universal laws. Values were finalized from the rd0 baseline recorded below, **before** any rd1 candidate comparison.

| Item | Region / procedure | Final gate | Status |
|---|---|---|---|
| Fixture identity | regenerate all fixtures with same seed; compare `source.npz` | max abs diff = 0 in all channels | final (rd0: 0.0 for all 7 fixtures) |
| Effect correctness | `alpha_threshold_trace` on `logo_badge` must produce exactly 1 outer contour with a hole recovered from hierarchy | region count = 1, hole present | final |
| Region decomposition | `region IoU` per GT visible region vs best matched output region (match by max IoU) | mean IoU ≥ 0.90, min IoU ≥ 0.70 | final |
| Region count | output regions with area ≥ min_area vs GT region count | absolute error ≤ 1 | final |
| Boundary geometry | symmetric chamfer distance between GT and output region boundaries, p95, in pixels; only IoU ≥ 0.5 matches | p95 ≤ 3.0 px at 512 width | final |
| Hole preservation | for each GT hole, an output hole covering ≥ 50% of its pixels | 100% of GT holes recovered on `nested_holes` and `logo_badge` | final |
| Thin features | `thin_strokes` / `thin_features` ROI; recall of GT thin pixels | recall ≥ 0.80 | final |
| Color | mean ΔE76 (CIE Lab, sRGB 8-bit) of matched region mean color vs GT color | mean ΔE ≤ 3.0 | final |
| Alpha / edges | composite over black / gray / white; p95 abs error in `edge_band` after ±1 px alignment tolerance | ≤ 60/255 encoded codes | final |
| Resolution | `logo_badge_256` / `_1024`; chamfer normalized by width/512 vs the 512 value | within ±25% | final |
| Performance | median wall time of 3 runs at 512×320 per method | no gate; descriptive only | final |

A candidate passes only if every gated row passes. Any unmeasured row stays "unverified" and blocks an overall pass claim. A method that fails a gate is not excluded from the report; it is reported with its failure evidence.

### rd0 baseline result (measured 2026-11-20)

`run_rd0.py` generated 7 deterministic fixtures (identity max abs diff = 0.0 for all) and measured two methods. Full numbers: `captures/rd0/metrics.json`; gate values: `captures/rd0/gates.json`.

| Fixture | alpha_threshold_trace IoU / chamfer / holes | posterize_components IoU / chamfer / holes |
|---|---|---|
| logo_badge | 0.249 / 101.2 px / 0-1 | 0.819 / 16.5 px / 1-1 |
| flat_illustration | 0.473 / 1.4 px / – | 0.931 / 22.6 px / – |
| nested_holes | 0.332 / 92.6 px / 0-1 | 0.928 / 22.9 px / 1-1 |
| thin_strokes | 0.245 / n/a / – (thin recall 0.654) | 0.278 / n/a / – (thin recall 0.898) |
| alpha_logo | 0.332 / 73.5 px / – | 0.968 / 1.0 px / – (color ΔE 21.8) |
| logo_badge_256 | 0.248 / 50.6 px / 0-1 | 0.793 / 8.4 px / 1-1 |
| logo_badge_1024 | 0.249 / 204.4 px / 0-1 | 0.830 / 32.3 px / 1-1 |

Shift-tolerant edge p95 for `posterize_components`: 35.2–70.2 codes. Wall time (median, 3 runs): `alpha_threshold_trace` ~3 ms, `posterize_components` ~16 ms at 512×320.

Gate rationale from this baseline:

- IoU/chamfer gates are set above the control and at/above the baseline for decomposition so that rd1 must show a real improvement; the naive methods fail on `logo_badge`, `nested_holes`, and `thin_strokes`.
- Hole recovery already passes at baseline for the two target fixtures and is kept at 100% as a no-regression gate.
- Thin recall already passes at baseline (0.898) and is kept as a no-regression gate.
- Color gate of 3.0 fails on `alpha_logo` (ΔE 21.8) because k-means returns the *blended* color of a semi-transparent region rather than its intrinsic color. This is a documented limitation to attack in rd1, not a threshold artifact.
- Edge gate of 60 codes is set just below the worst primary baseline (59.8 for `thin_strokes`); the 1024 case (70.2) remains an open resolution issue.
- Resolution gate uses width-normalized chamfer (8.4/0.5=16.8 at 256, 32.3/2=16.1 at 1024 vs 16.5 at 512), so the baseline passes and rd1 must not regress.

No human visual review has been performed. Numeric results are not a quality pass.

## 6. Statistics to record

Per method and fixture: region count, per-region area and area error ratio, matched IoU (mean/min), symmetric chamfer boundary distance (p50/p95/max), hole recovery count, thin-feature recall, mean color and ΔE76, alpha/edge composite max error per background, vertex counts (raw / simplified), non-finite count, and median wall time. Full-frame values are always accompanied by fixed-ROI values because local defects are hidden by frame averages. Diff images are display-normalized ×8; measurements use raw float arrays.

## 7. Execution conditions and evidence

- Python 3.14, NumPy 2.4, OpenCV 4.13, Pillow 12; versions and platform written to the manifest.
- Outputs: `captures/rd0/manifest.json`, `manifest.sha256`, `metrics.json`, `gates.json`, `fixtures/`, review PNGs, ROI crops, and diff sheets.
- Each record stores fixture/model/settings, input and output hashes, ROI definitions, metric definitions, environment, and run date.
- Reviewer: the run records the operator (AI agent) and states that no independent human visual review has been performed.
- Generated images/arrays under `captures/` are git-ignored. Only manually selected final evidence with conditions, date, provenance, and hash is force-added individually. Blind-review keys (if any) are not committed before review completes.
- Thresholds may only change with a recorded reason, previous and new value, approver, and a re-run of affected comparisons.
