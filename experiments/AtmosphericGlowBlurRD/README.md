# Atmospheric Glow Blur R&D

Offline CPU exploration for the planned Atmospheric Glow Blur effect. It does
not call Artifact source or binaries and does not establish product parity,
visual acceptance, or runtime performance.

## Reproduce rd1

From the repository root:

```powershell
python experiments/AtmosphericGlowBlurRD/atmospheric_glow_blur_lab.py
```

This captures three resolutions (480×270, 960×540, 1920×1080), the deterministic
HDR chart, two CC0 photographic plates, an alpha edge fixture, and a seven-frame
moving source sequence. `--only-960` limits the image cases to the primary
resolution while retaining alpha and temporal diagnostics.

At 960×540 the generator also runs 66 one-factor model comparisons across
threshold, broad fraction, broad radius, veil fraction, and amount on all three
sources. Contact sheets and the raw region metrics are in
`rd1_parameter_sweeps.json` and `sweep-*.review.png`. These are exploratory
parameter responses, not a final preset search or acceptance run.

## Evidence

- `captures/rd1/rd1_manifest.json` records inputs, settings, fixed ROIs,
  source/code/output hashes, runtime versions, region metrics, alpha checks,
  and sequence residuals. Verify its checksum against `rd1_manifest.sha256`.
- `*.rgba.npy` / `*.npy` files hold float32 RGBA/RGB source, model output, and
  effect components. Values used for metrics remain linear and unclamped.
- `*.preview.png` files are review-only: Reinhard `x/(1+x)` then sRGB OETF.
  They are not Artifact's view transform and are not suitable for HDR parity.
- CC0 asset provenance and source hashes are in `experiments/GlowLab/assets/SOURCES.md`.
  Portrait assets are excluded because their individual license record was not
  located. JPEG input is decoded using an sRGB transfer assumption; embedded
  ICC profile application is not part of this study.

## Model definitions

`baseline_model` is a standalone fixed-threshold, three-Gaussian equation model
with weights 0.50/0.30/0.20 and sigmas 5/9.5/14 native pixels. It is not a
capture of an installed Artifact Glow effect. `candidate_model` uses a higher
soft threshold, 65/35 local/broad Gaussian source scatter, and an independently
recorded 0.08 broad veil component. All are exploratory parameters, not product
defaults or approved look values.

Spatial sigmas scale proportionally with image width relative to the 960px
primary capture, so the resolution comparison keeps the same framed look.

The alpha pair is unassociated before processing; the measured agreement only
checks this offline equation when both inputs represent the same straight RGB.
It does not validate Artifact's alpha contract. Fully-transparent hidden RGB is
currently zero in the canonical fixture; invalid hidden-RGB invariance remains
unmeasured. Temporal residuals are screen-space diagnostics without motion
compensation and have no acceptance threshold.

The rd1 comparisons intentionally do not set final quality thresholds: there is
no source-matched current Artifact runtime baseline, no approved target image,
and no GPU/preview/final capture. Current-source product capture, visual review
by a separate reviewer, backend/resolution parity, and performance remain open.

`compare_installed_glow_sdr.py` reads a pre-existing `ArtifactUiTest` installed
CPU Glow capture from `experiments/GlowLab/captures/` without launching the app.
It verifies the saved image hashes and compares twenty combinations of that
historical Glow gain sweep against offline candidate amounts on the exact
captured input. See `captures/product-glow-sdr-rd1/comparison_manifest.json`.
This provides an SDR look baseline only: the recorded executable hash matches
the local binary, but the capture's source revision is absent, output is 8-bit,
and the numeric Glow gain and candidate amount are not equivalent controls.

The same report also includes an offline reconstruction of the current
`GlowEffectCPUImpl::applyCPU` equation. On this opaque input, the raw-sRGB-code
equation mirror differs from all four historical outputs by about
0.0014–0.0018 8-bit code RMSE (maximum 1 code); a linearized-source mirror is
farther away. This supports the equation interpretation for this saved capture
but cannot recover the missing source revision. A separate fixed-mask transfer
ablation shows that running the candidate's convolution/composite in encoded
sRGB values moves it numerically closer to the legacy Glow PNG than scene-linear
calculation does. This is a transfer-domain sensitivity result, not a quality
recommendation; the final effect color contract remains undecided.

The fixed-mask ablation's whole-image encoded RGB MAE (gain .10/.25/.50/1.00)
was scene-linear `1.529/2.651/4.308/6.915` codes and encoded-sRGB
`0.664/1.604/2.998/5.190` codes. The central intersection ROI had the same
direction (`3.932/6.984/11.076/16.680` versus `1.901/4.359/7.718/12.243`).
Because the mask was held fixed, this isolates convolution/composite transfer
sensitivity; it does not show that encoded-space processing is perceptually
correct. The mirror and ablation artifacts, including per-pixel float arrays,
are in `captures/product-glow-sdr-rd1/`.

The historical capture's executable hash matches the local `ArtifactUiTest`
binary, and the current `Artifact` child HEAD is
`d211c85ef0ce0e6a5a1fb14608c97b6e0e12a3fe` (2026-11-12). The capture has no
source revision, so neither the binary match nor equation match establishes
that it was built from this exact source tree.

## Blind visual review packet

`build_blind_review_packet.py` reads only the saved SDR product capture and
offline candidate arrays. It creates four A/B full-frame comparisons plus
fixed ROI crops, a response CSV template, and a hash manifest in
`captures/blind-review-sdr-rd1/`. The A/B method key is kept in the sibling
`captures/blind-review-sdr-rd1-key/` directory; do not share that directory
with a reviewer until their responses are frozen. A/B order is shuffled from a
recorded seed. The reviewer must record method-blind responses before opening
the key. This packet tests visibility and perceived differences only: the
historical Glow gain and candidate amount are not equivalent controls, and the
material is an 8-bit SDR capture. No independent review has yet been performed.

Rebuild it from the repository root with:

```powershell
python experiments/AtmosphericGlowBlurRD/build_blind_review_packet.py
```

## Matched-energy scatter profile control

`compare_scatter_profiles.py` applies one fixed smooth highlight mask to a
deterministic HDR chart, then compares a single Gaussian at the Glow-like RMS
radius, a three-scale Glow-like profile, a single Gaussian at the candidate
RMS radius, and the local/broad/veil profile. It normalizes all scattered RGB
so each output adds the same total scene-linear Rec.709 luma on this chart.
This isolates PSF shape from source selection and integrated energy; it is not
a perceptual brightness match or product comparison.

Results and raw arrays are stored in `captures/scatter-profile-rd1/`. The
Glow-like multiscale profile has impulse PSF r95 25.02 px; the local/broad/veil
profile has r95 74.41 px and retains a more concentrated center than a single
Gaussian with the same RMS radius (r50 9.22 vs 28.02 px). The candidate's weak
veil contributes 0.237 scene-linear luma units inside the synthetic fine-detail
ROI, while the ROI luma standard deviation changes by only `-1.04e-6`. These
are equation responses on one synthetic chart, not evidence of perceived
improvement. See the manifest's full regions, unbounded float arrays, and
impulse PSF measurements.

Run from the repository root with:

```powershell
python experiments/AtmosphericGlowBlurRD/compare_scatter_profiles.py
```

`build_scatter_blind_review_packet.py` turns three equal-energy model pairings
into A/B full-frame and fixed-ROI review images, with a CSV response sheet.
The method key is isolated in the sibling `captures/blind-review-scatter-profile-rd1-key/`
directory. The packet uses one synthetic chart and the study's Reinhard+sRGB
preview; it cannot establish product appearance. No independent responses are
recorded yet.

## CC0 photographic scatter controls

`compare_scatter_profiles_photographs.py` repeats the same mask, PSFs, and
per-image total-energy normalization on the provenance-checked CC0 neon and
streetlight plates. Region masks reuse the predeclared practical-light and
detail ROIs from rd1. In the streetlight plate, the sky-field ring receives
about `0.0026` added luma units from the multiscale profile and `1.9381` from
local/broad/veil; the left/right lamp-core luma standard deviation rises by
`0.0703/0.0531` and `0.0722/0.0554`, respectively. On neon, the fine-detail
ROI luma standard deviation rises by `0.01629` for multiscale and `0.01370` for
local/broad/veil. These are region responses on two example photographs, not
quality scores. Output arrays, masks, manifests, and contacts are in
`captures/scatter-photo-rd1/`.

`build_photo_scatter_blind_review.py` creates six anonymized A/B comparisons
(three model pairings on each plate), full frames, practical-light/detail ROI
crops, and a response CSV. The method key is separated at
`captures/blind-review-photo-scatter-rd1-key/`. It remains a method-blind
review packet only; no independent reviewer responses are recorded.
The response sheet asks perceived brightness separately from atmospheric
character because equal scene-linear added energy does not equalize the
Reinhard+sRGB preview's displayed brightness. For multiscale Glow vs
local/broad/veil, mean decoded preview-luma deltas are `+0.000306` over the
whole neon image and `+0.000011` over the whole streetlight image, while local
ROIs differ more; see the per-region display-luma fields in the photo manifest.
