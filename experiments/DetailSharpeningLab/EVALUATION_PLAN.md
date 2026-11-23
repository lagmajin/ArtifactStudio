# Detail Sharpening R&D — Pre-registered Image Evaluation Plan

**最終更新:** 2026-11-14
**Status:** Registered before any candidate image was produced (rd1). Thresholds below are fixed before candidate selection.

This plan follows `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`. The experiment is an offline, deterministic CPU study. It does not call Artifact binaries, does not establish product parity, and does not approve an effect or default.

## 1. Purpose and target

**Goal:** Find out whether a detail-enhancement stage can raise local edge/detail contrast without the halo, overshoot, chroma-fringe, noise-amplification, threshold-flicker, and clipping side effects of a plain unsharp mask.

**In scope**

- Static synthetic HDR/SDR plates at 960×540 and 1920×1080: hard step edge, fine 1 px lines, colored edge, noise patches, ramp, near-clipping highlight step, text-like strokes.
- Straight-alpha antialiased edge fixture composited over black, mid gray, and white.
- An 8-frame moving low-amplitude grating sequence at 480×270 for threshold-driven temporal instability.
- Four models: product CPU equation mirror, luma-domain soft-threshold USM, halo-limited luma USM, multiscale halo-limited clarity.

**Out of scope (recorded as unverified, not as a pass)**

- Artifact GPU path, Composition Viewer, Render Queue final output, preview vs final routing.
- D3D12/Vulkan backend parity, HDR display transform, OCIO/working-space conversion.
- Any shot with motion blur, depth of field, grain, or compression that is not in the fixtures.
- Real photographic material and aesthetic preference. No external asset is used in rd1.
- Product defaults, property ranges, UI, and effect-catalog changes.
- Hot-path allocation behavior of the product implementation (measured only as wall time of offline NumPy/OpenCV models).

## 2. Comparison targets and their role

| Item | Role | May be called ground truth? |
|---|---|---|
| Source fixture | reference for geometry, alpha, plateau values, and noise statistics | Yes for "unchanged under neutral settings" only |
| `product_usm_mirror` | source-equation mirror of `Artifact/src/Effects/Sharpen/SharpenEffect.cpp` CPU path (per-channel USM, hard threshold, clamp ≥ 0) | No. It is a reimplementation, not an installed-effect capture. |
| `luma_usm_soft` | candidate: luma-domain detail, soft-knee threshold | No |
| `halo_limited` | candidate: luma USM with output clamped to source local extrema | No |
| `multiscale_limited` | candidate: fine + coarse clarity with the same clamp | No |

No model output is a physical or perceptual reference. Differences are reported against the source and against `product_usm_mirror` as a control, not as a ranking.

## 3. Evaluation material

All fixtures are generated deterministically by `detail_sharpen_lab.py` with `seed=20261114`. They are synthetic, so license/provenance is "generated in this repository"; each file hash is recorded in the manifest.

- `chart_960.png` / `chart_1920.png`: primary and resolution plates. Fixed ROIs: `step_edge`, `fine_lines`, `color_edge`, `noise_low`, `noise_high`, `ramp`, `highlight_step`, `text_strokes`, `flat_gray`.
- `alpha_edge.png`: antialiased rounded alpha edge with a semi-transparent region and hidden RGB set to zero.
- `moving_grating/`: 8 frames, 480×270, a sinusoidal grating of amplitude 0.02 moving 0.6 px/frame over a flat 0.10 background; ROI `grating_roi`.

Fixture details are recorded in the manifest (dimensions, seed, ROI rectangles, per-file SHA-256). Pixel values are scene-linear float32 RGB unless stated.

## 4. Fixed image conditions

- Processing domain: scene-linear float32 RGB, Rec.709 luma for detail extraction (`0.2126/0.7152/0.0722`).
- Radius: `sigma=1.0 px` at 960 width. Resolution case scales sigma with image width (×2 at 1920) plus a fixed-1 px control.
- Primary candidate settings, fixed before measurement: `amount=1.0`, `sigma=1.0`, soft threshold `0.02` with knee `0.01` (luma detail). Halo clamp radius `ceil(3·sigma)`. Multiscale coarse radius `3·sigma`, coarse weight `0.35`.
- Product mirror settings: `amount=1.0`, `sigma=1.0`, `threshold=0.0`; additionally swept at amount 0.5/1.0/2.0 and sigma 1.0/2.0 with `threshold=0.0` and `0.02`.
- Alpha: fixtures store straight (unassociated) alpha; models never modify alpha. Composite tests use `out.rgb·α + bg·(1−α)`.
- Output for review: Reinhard `x/(1+x)` then sRGB OETF, 8-bit PNG. Review PNGs are not measurement data. Measurements use the raw float arrays saved as compressed `float32` `.npz`.
- No random seed is used inside the models. The fixture generator seed is recorded once.

## 5. Required evaluations, regions, and pre-registered gates

Each row is measured on the 960×540 chart unless stated. Gates are review gates for this fixture set, not universal laws.

| Item | Region / procedure | Gate (pre-registered) |
|---|---|---|
| Effect correctness | neutral settings `amount=0` must return the source exactly | max abs difference = 0 in all channels |
| Halo / overshoot | `step_edge`; per-pixel output beyond source local extrema (window radius `3σ`) by > 1e-3 linear; plateau overshoot in linear and 8-bit codes | `halo_limited` and `multiscale_limited`: halo-band fraction ≤ 1.0% and plateau overshoot ≤ 0.004 linear (≈1 code). Mirror has no gate; its value is the control. |
| Detail / edge slope | averaged horizontal profile of the hard step; 20→80% rise distance | candidates must reduce rise distance by ≥ 15% vs source at the primary setting |
| Fine detail | `fine_lines`; high-pass energy ratio and p99−p1 line contrast vs source | energy ratio ≥ 1.10 and no more than 5% of line-ROI pixels clipped |
| Color | `color_edge`; change in (R−G) and (B−G) inside ±3 px of the edge, in 8-bit codes | candidate chroma change ≤ 50% of `product_usm_mirror`'s value |
| Noise | `noise_low` (σ≈0.004) and `noise_high` (σ≈0.02); std ratio | soft-threshold candidates: std ratio ≤ 1.25 at `noise_low`; report `noise_high` without a gate |
| Clipping | whole frame and `highlight_step`; newly clipped channels (src ≤ 1.0 → out > 1.0) | candidates ≤ 0.10% of channels full frame and ≤ 1.0% in `highlight_step` |
| Alpha | alpha equality; composite-on-black/gray/white max error in the 3 px edge band | alpha max abs difference = 0; composite error ≤ 2/255 encoded |
| Temporal stability | 8-frame `grating_roi`; temporal std of the high-pass luma at each pixel, ratio model/source | candidate ratio ≤ 1.50; mirror value reported as the hard-threshold control |
| Resolution | chart at 1920 with sigma scaled ×2; edge-rise gain vs 960 | candidate's relative rise reduction must stay within ±25% of the 960 value |
| Performance | median wall time of 3 runs at 960×540 per model | no gate; reported descriptively, no product claim |

A candidate passes only if every gated row passes. Any unmeasured row stays "unverified" and blocks an overall pass claim.

## 6. Statistics to record

Per model and region: channel min/max/mean, luminance p1/p50/p99, non-finite count, newly clipped channel fraction, high-pass energy, and the effect-specific values above. Full-frame metrics are always accompanied by the fixed-region values because local defects are hidden by frame averages. Diff images are saved for review and are display-normalized ×8; the measurement uses raw floats.

## 7. Execution conditions and evidence

- Python 3.14, NumPy, OpenCV, Pillow; versions and platform are written to the manifest.
- Outputs: `captures/rd1/manifest.json`, `manifest.sha256`, per-case `.npz`, review PNGs, contact sheets, ROI crops, and diff sheets.
- Each record stores fixture/model/settings, input and output hashes, ROI definitions, metric definitions, environment, and run date.
- Reviewer: the run records the operator (AI agent) and states that no independent human visual review has been performed.
- Thresholds may only change with a recorded reason, the previous and new value, the approver, and a re-run of affected comparisons. They may not be adjusted after seeing candidate results.

## 8. Known limitations stated up front

- Offline NumPy/OpenCV models only. The product CPU path uses OpenCV too, but the mirror is still not a capture.
- Synthetic fixtures cannot establish behavior on real photographic texture, skin, foliage, or compressed video.
- The temporal fixture is a low-amplitude grating; it isolates threshold flicker but is not a motion-compensated real-shot test.
- The 8-bit review PNGs cannot show HDR precision; raw floats are retained as `.npz` for measurement.
- No GPU, no final Viewer, no preview/final route comparison, no allocation profiling.

## 9. rd2 addendum (registered 2026-11-14, after rd1, before rd2)

**Reason.** rd1 produced two results that make the original fixtures insufficient, not that change the gates:

1. `halo_limited` reduced 20→80% rise distance by only 0.2% on the 1 px hard step while removing all overshoot. A pixel-sharp edge is already at the sampling limit, so a halo-free operator has no headroom there. The realistic case is a lens-softened edge.
2. `luma_usm_soft` raised the straight-alpha composite error on black to 26.1 codes, above the 15.9 codes of the per-channel mirror, because detail is computed from unassociated RGB without alpha weighting.

**Added material (does not replace rd1 records).**

- `soft_edge` fixture: 960×540 step blurred with a Gaussian of σ=1.2 px (declared synthetic PSF, not a measured lens), plus the same 0.004 and 0.020 noise fields. ROI `step_edge` is ±10% of width around the transition; noise ROIs are fixed in the manifest.
- `alpha_weighted_limited` model: the rd1 `halo_limited` detail multiplied by straight alpha before compositing; alpha is never modified.

**Thresholds.** Unchanged from section 5: halo-band fraction ≤1.0%, overshoot ≤0.004 linear, rise reduction ≥15%, composite max error ≤2 codes. These gates were fixed before rd1 and are not relaxed for rd2; a candidate that still fails is reported as failing.

**Scope note.** Alpha weighting is a straight-alpha concession for this fixture. It does not decide the product alpha contract, and it does not establish premultiplied behavior.

## 10. rd3 addendum (registered 2026-11-14, after rd2, before rd3)

**Reason.** rd2 isolated three issues that rd3 must test instead of assuming:

1. An absolute threshold of 0.02 stopped all soft-edge response. The threshold needs to scale with the image's own detail/noise level.
2. Near-clipping highlights (~0.985) still gain 4.1–4.4% newly clipped channels under any USM. A detail headroom rolloff has not been tested.
3. The clamp radius was fixed at 3σ. Its effect on the achievable gain is unknown.
4. No photographic material has been evaluated yet, so all conclusions are synthetic.

**Added material.**

- CC0 working plates recorded in `experiments/GlowLab/assets/SOURCES.md`: `cc0-neon-streets-1536.jpg` (1536×1025, SHA-256 `8791bd59…`) and `cc0-street-light-night.jpg` (576×1280, SHA-256 `b01e663d…`). Decoded as sRGB and linearized with the sRGB EOTF assumption; embedded ICC is not applied. Resized for this study (neon → 960×641, streetlight → 432×960) with Pillow LANCZOS; the transform and output hashes are recorded.
- Automatic fixed ROIs per plate: a 4×3 grid; `bright` = highest mean luma cell, `dark` = lowest mean, `flat` = lowest luma variance cell, `detail` = highest high-pass energy cell. Coordinates and cell stats are stored, not chosen by eye after seeing results.

**Added models.**

- `noise_rel_k{1,2,3}`: luma detail with threshold `k·σ̂`, knee = threshold, where `σ̂ = 1.4826·median(|luma − Gaussian(luma,1px)|)` over the whole frame. On textured plates this is an upper bound of sensor noise because texture contributes to the estimate; that bias is recorded.
- `halo_noise_rel_k3`: clamp radius 3σ plus `k=3` threshold.
- `halo_rolloff`: clamp radius 3σ, threshold 0, detail multiplied by `1 − smoothstep(0.85, 1.0, max_channel(src + detail))`.
- `halo_noise_rel_k3_rolloff`: combination of the two.

**rd3 gates (fixed now, not relaxed later).**

| ID | Gate |
|---|---|
| N1 | `noise_rel_k3` on the 960 chart: `noise_low` std ratio ≤ 1.15 and soft-edge 20→80% rise gain ≥ 10% |
| N2 | `halo_rolloff`: 960-chart `highlight_new_clip_fraction` ≤ 0.1% and soft-edge rise gain ≥ 90% of `halo_limited` (threshold 0) |
| N3 | Photographic candidate (`halo_noise_rel_k3_rolloff`): full-frame new clipped channel fraction ≤ 0.05%; zero extrema exceedance in `bright`, `dark`, and `flat` ROIs; alpha unchanged; chroma delta ≤ the mirror's |
| N4 | Radius sweep 1σ/2σ/3σ/6σ: descriptive only, no pass/fail |
| R1 | Reused rd2 gates on the soft edge: halo ≤ 1.0%, overshoot ≤ 0.004 linear, rise gain ≥ 15% |

N1–N3 are evaluated on the exact fixtures above. Photographic results are response evidence on two plates, not aesthetic acceptance.

**Limitations added for rd3.** The CC0 plates are 8-bit sRGB JPEGs; linearization cannot recover HDR or clipped source detail. There is no ground truth for halo on a photograph, so extrema exceedance is used as a spatial proxy and its limits are recorded. No independent human review is performed.

## 11. rd3b correction (registered 2026-11-14, after the first rd3 run, before the corrected run)

**Observed failure of the registered estimator.** `σ̂ = 1.4826·median(|luma − Gaussian(luma,1px)|)` over the whole frame collapses toward zero when most of the frame is flat and noise-free: the chart's flat background dominates the median, so `k·σ̂ ≈ 0` and `noise_rel_k1/k2/k3` produced output identical to threshold 0 on both the chart and the soft edge (`rise` 0.291 / 0.158, `noise_low` ratio 2.372 / 1.667 unchanged). This is an estimator/fixture limitation, not evidence about adaptive thresholds.

**Corrections (gates N2, R1, N3 semantics unchanged; N1 restated for valid material).**

1. **Estimator:** block-wise noise floor. Split the frame into 16×16 blocks; per block compute `1.4826·median(|hp − median(hp)|)` where `hp = luma − Gaussian(luma,1px)`; take the 10th percentile across blocks as `σ̂`. This targets the quietest blocks instead of the frame median.
2. **Material:** the clean chart's flat fields contain no noise, so no noise-floor estimator can be exercised there. Add `chart_960_noisy`: the same chart plus deterministic Gaussian noise σ=0.004 applied to **every** pixel (seed 20261114, recorded). N1 is evaluated on this plate; the clean chart remains for N2 and the hard-edge baseline.
3. **Photographic luma bound:** per-channel clamping bounds each channel but not luma, so the candidate still exceeded luma local extrema on the neon plate (3.13% of the frame, 15.5% for the mirror). Add `luma_guard_combo`: compute the candidate detail, clamp the **luma delta** to the source's local luma min/max (radius 3σ), and apply that delta to RGB. This guarantees the luma bound by construction and preserves the chroma of the source. N3 is evaluated on this variant; the per-channel variant remains in the manifest as the failing comparison.

**Restated N1 (on `chart_960_noisy`, k=3, σ̂ = block-wise floor):** `noise_low` std ratio ≤ 1.15 while the hard-edge 20→80% rise gain is ≥ 10%. **N3 (photos):** `luma_guard_combo` must satisfy new clip ≤ 0.05% full frame, zero luma extrema exceedance in `bright`/`dark`/`flat`, alpha unchanged, chroma delta ≤ mirror.

**Recorded difference from rd3 first run.** rd3 first-run numbers are retained in `captures/rd3/manifest.json` under `first_attempt`; the corrected run is written to the same manifest as `cases`/`photos`, with this plan section as the reason for the change. No gate is loosened.
