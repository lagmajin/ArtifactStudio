# Image-to-Image Color Matching R&D: Skin and White Preservation

**最終更新:** 2026-11-17
**ステータス:** In Progress

## Objective and scope

Determine how the existing `ArtifactCore::AutoColorMatcher` methods (Reinhard,
MeanStddev, Histogram) transfer color from a reference image while preserving
skin appearance and neutral whites. This is an evaluation of the existing Core
API and bounded offline baselines; it does not authorize a new algorithm,
product UI, default change, or runtime integration.

The existing `experiments/ImageCorrectionSuite` color-match chart and its
channel-wise mean/std reference are retained as a synthetic control. The suite
currently does not execute Core `AutoColorMatcher`, and its single affine chart
does not represent skin, white balance, mixed illumination, alpha, or natural
image texture. Existing Core contract tests cover BGRA interpretation,
alpha preservation, intensity blending, and basic method dispatch. An authorized
image-overload contract case now also compares a licensed full-frame ColorChecker
pair against a float32-faithful equation mirror; neither check establishes
natural skin/white visual quality or product acceptance.

Static source inspection found that Reinhard converts logical RGB values to
Lab before computing whole-image channel statistics, MeanStddev computes
per-channel statistics on logical RGB values, and Histogram builds independent
8-bit-per-channel CDFs. All three currently include every pixel regardless of
alpha when deriving statistics; output alpha is left unchanged. The API
exposes no skin/white protection mask or ROI weighting. These are source
observations, not runtime measurements.

## Static call-site audit (2026-11-17)

A repository-wide search for `AutoColorMatcher` and `import Color.AutoMatch`
found the Core declaration/implementation, its contract-test fixture and R&D
documentation, but no application/runtime call site. The exposed API mutates
an `ImageF32x4_RGBA` source in place or accepts raw BGRA float buffers; it is
not currently evidenced as an editor preview/export operation. This search is
bounded by checked-in text sources and does not prove runtime reachability via
reflection or dynamically loaded code. It does mean preview/export parity and
in-application performance cannot currently be assessed as existing behavior.

## Questions

1. Which method, if any, moves source appearance toward a reference without
   unacceptable changes to protected skin and neutral-white regions?
2. How do intensity and the source/reference pairing affect that tradeoff?
3. Do alpha, hidden RGB, clipping, or transfer-space assumptions create visible
   regressions?

## Evaluation materials and conditions

The deterministic chart in `experiments/ColorMatchLab/run_rd0.py` is generated
at 512×320 with seed 20261116. It contains a smooth field, fixed skin-like,
near-white and saturated patches, texture, and ROI masks. Pixel values are
known synthetic controls, not human skin ground truth. The float source,
reference and offline mean/std output, preview images, ROI mask files, and
SHA-256 manifest are in `experiments/ColorMatchLab/captures/rd0/`.

The per-channel mean/std control is a simple independent Python equation. It
is not a mirror of `AutoColorMatcher`: Core MeanStddev currently uses BGRA
buffer values but the rd0 control uses linear RGB, and Core Reinhard and
Histogram use different equations. Metrics are computed on unclipped linear
float values; PNG previews clip to [0,1] and encode sRGB. ROI chroma is the
mean absolute deviation from each pixel's RGB channel mean, not a perceptual
color difference. No ICC conversion or alpha behavior is represented.

The lab references existing CC0 working plates in
`../CinematicLooksLab/assets/portrait-2024-1536.jpg` and
`../CinematicLooksLab/assets/forest-wild-1536.jpg`; provenance and hashes are
in `../CinematicLooksLab/assets/SOURCES.md`. They are not paired references
and have no reviewed face/skin or neutral-white ROI annotations. The natural-
image comparison is not ready to begin until paired, licensed references and
reviewed ROI masks are available.

## Measurements and gate protocol

The rd0 run records RGB distributions, luminance shift, source/reference RGB
MAE, neutral-chroma proxy, clipping and non-finite counts by ROI. These values
only check that fixture and measurement plumbing work; they do not set quality
thresholds, establish visual fidelity, or select a winner. Edge/texture
retention, perceptual-space skin difference, manual ROI review, blinded visual
review, and time/memory measures remain unimplemented.

Before Core candidate outputs are compared, add provenance-backed portrait and
non-portrait source/reference pairs, annotate skin/white and relevant control
ROIs, inspect the unchanged-source baseline, document reviewer-approved
thresholds, and freeze gates in the manifest. Compare Core Reinhard,
MeanStddev, and Histogram at intensities 0.25, 0.5, and 1.0. Review randomized
full-frame and named crops under fixed display conditions. Record failures and
reviewer disagreement; if suitable references or agreed gates are missing,
keep the result inconclusive.

## Required unverified lanes

Core execution and C++ differential, paired natural photographs, reviewed
ROIs, human review and acceptance thresholds, edge/detail preservation,
alpha/hidden RGB, ICC, HDR, GPU/backend parity, product preview/export paths,
and runtime performance remain unverified. No build, CMake configure, or test
execution is part of this R&D.

## RD15: transfer encoding sensitivity (2026-11-16)

Added a predeclared 256×192 synthetic sensitivity probe comparing sRGB-encoded
float and linear-light float representations of the same synthetic scene.
Three inspected equation mirrors × three intensities were measured, with
linear results re-encoded to sRGB for comparison. At intensity 1.0, cross-
encoding RGB MAE was 0.00778 / 0.000712 / 0.00247 whole-frame for Reinhard /
MeanStddev / Histogram; skin-proxy ROI values were 0.00203 / 0.000449 /
0.00247. This shows equation sensitivity in this fixture, not a method ranking
or product image-quality result.

Static inspection confirms `ImageF32x4_RGBA::setColorDescriptor` retains
transfer and primaries while canonicalizing storage/order, but the matcher
overload forwards only raw float pixels and dimensions. Thus the current
overload does not act on the descriptor. Full metrics, fixed conditions,
source hashes and output hashes are in
`experiments/ColorMatchLab/captures/rd15_encoding_sensitivity/manifest.json`.
The probe uses source-mirror equations, not compiled Core output. Runtime
differential, caller metadata, color-primary conversion, HDR, alpha, visual
review, human portrait pairs, product route, and performance remain open.

## RD16: finite float range sensitivity (2026-11-16)

Added a fixed 39-pixel probe covering finite negative values, SDR values, and
highlights through 4.0. The reference is related to source by an unclipped
per-channel affine transform. At intensity 1.0 the source-mirror whole-fixture
RGB MAE was 0.2023 / approximately 1.35e-16 / 0.2001 for Reinhard /
MeanStddev / Histogram. MeanStddev nearly exactly inverts the deliberately
affine construction; that is a positive control, not evidence of general HDR
quality or method superiority. Reinhard and Histogram return only 0–1 values
at full intensity; MeanStddev retains values outside that range. The full
underflow / SDR / highlight and intensity breakdown is recorded in
`experiments/ColorMatchLab/captures/rd16_float_range_sensitivity/manifest.json`.

This is finite synthetic equation-mirror evidence only. Compiled Core output,
actual linear-HDR display behavior, non-sRGB primaries, alpha, NaN/Inf or
extreme values, visual acceptance, application route, and performance remain
unverified.

## R0 progress (2026-11-17)

- Added `experiments/ColorMatchLab/` with a deterministic 512×320 synthetic
  source/reference chart, fixed skin-like / near-white / saturated patches,
  smooth and texture ROIs, an offline mean/std diagnostic, per-ROI descriptive
  metrics, and SHA-256 manifest. The chart is synthetic and does not establish
  human skin or visual quality.
- Added `alpha_statistics_counterfactual.py`: two fixtures with identical
  visible pixels and alpha but different RGB under fully transparent pixels
  yield different whole-frame statistics; their opaque-only statistics match.
  This supports the potential risk from source-observed all-pixel statistics,
  but is not an execution or output measurement of Core.
- Reused corpus candidates are a single CC0 portrait and a CC0 forest plate.
  Later RD6/RD7 added licensed registered ColorChecker and natural-scene
  RAW/JPEG pairs, but there is still no captured portrait WB pair or
  independently reviewed skin/real-white ROI.
- A hand-drawn face-skin ROI candidate and overlay are saved under
  `experiments/ColorMatchLab/captures/rd0/portrait_skin_roi_candidate.*`.
  Visual inspection shows it is useful for annotation review but it is not
  independently approved; the source has strong mixed warm light/shadow and
  no neutral-white ROI or paired reference. It is excluded from acceptance
  metrics until reviewed and supplemented with other portraits.
- Numeric thresholds, manual visual review, Core outputs, and candidate ranking
  remain open. The R&D is in progress, not passed.
- `run_rd1_source_mirror.py` generated a pixel-aligned portrait pair using a
  known encoded-RGB cast and compared independent Python transcriptions of the
  three inspected equations at intensities 0.25 / 0.5 / 1.0. Nine outputs and
  per-region measurements are hash-recorded in
  `experiments/ColorMatchLab/captures/rd1_source_mirror/`. Across this
  synthetic cast, stronger settings generally reduce whole-frame distance to
  the generated reference; this is expected under the generated target and
  does not establish natural look quality or skin preservation. The source
  file hash is pinned, but no C++ differential or Core execution was performed.
- A bright window rectangle is included only as an unreviewed candidate ROI;
  its neutrality is not established. The annotated face ROI also remains
  unapproved. These regions are diagnostic only and no acceptance gate or
  method ranking is set.
- R1 controlled cast recovery was run on the existing CC0 portrait. A known
  same-image color cast served as the pixel-aligned synthetic reference;
  three independently transcribed Core equations × three intensities were
  measured and hash-recorded. Stronger settings move toward this constructed
  reference (e.g. Histogram 1.0 whole-image MAE 0.01299 → 0.00184; candidate
  face ROI 0.00892 → 0.00213). This is an algorithmic diagnostic only, not
  Core execution or evidence of natural-photo quality / skin protection.
  Reviewed natural photo pairs, approved ROI, white ROI, visual gate, and
  C++ differential remain incomplete.
- R2 added four neutral linear-light target levels and a known warm cast to a
  synthetic chart, then measured the independent source mirrors at three
  intensities. At intensity 1.0 the neutral ROI mean ΔE76 proxy was 5.72 /
  9.49 / 7.20 for Reinhard / MeanStddev / Histogram, with mean linear
  Rec.709 luminance shifts +0.0280 / +0.0260 / +0.0217 and RGB MAE 0.0258 /
  0.0282 / 0.0282. These results show that neutral chroma recovery and
  luminance shift trade off under this constructed cast; they are not
  perceptual acceptance or a winner. Natural white objects, skin interaction,
  C++ execution, alpha, ICC/HDR, and runtime remain unverified.
- Both source-mirror runner paths pin the inspected `AutoColorMatch.cpp`
  SHA-256 and refuse to regenerate captures after that source changes until
  the equation mirror is explicitly re-audited. This protects against stale
  source-derived diagnostics; it does not prove parity.

### R2 response-shape finding

The regenerated manifest now compares whole frame, neutral-axis ROI, its
non-neutral complement, and each of the four patch levels. All three equation
mirrors reduced ΔE76 monotonically as intensity rose across every patch; the
absolute luminance shift increased at the same time. At intensity 1.0, per-
level ΔE76 (targets 0.58→0.92) was Reinhard [4.37, 5.29, 6.22, 7.00],
MeanStddev [6.24, 7.99, 10.62, 13.12], Histogram [5.70, 8.67, 13.81, 0.62].
Histogram's high-level discontinuity may reflect its 256-bin CDF behavior, but
the current single synthetic distribution does not establish cause or defect.
Whole-frame RGB MAE also understated neutral-ROI error for all three methods.
This is a reason to retain ROI-specific measurements; it does not set a gate.


### RD1 inverse-cast paired control

The portrait source-mirror now also applies the methods in reverse, from a
known synthetic cast toward the pixel-aligned original. The affine encoded-RGB
cast clipped 16.62% of pixels; exact recovery is impossible in those pixels,
so metrics separate the remaining invertible pixels. At intensity 1.0, RGB
MAE to the original over invertible pixels was 0.009190 / 0.000328 / 0.000078
for Reinhard / MeanStddev / Histogram. On the candidate skin ROI's invertible
pixels it was 0.010661 / 0.000428 / 0.000076. This is a synthetic asymmetric
recovery diagnostic, not a natural-photo method ranking: the transform is
global, the skin and bright-window masks remain unreviewed, and all outputs
are independent Python equation mirrors. Inputs are explicitly in the SDR [0,1] range. The discrepancy is consistent
with using Lab-space statistics to approximate a channel-wise affine inverse,
but this explanation remains unverified. Compiled Core differential remains
necessary before drawing conclusions about the inspected implementation.


### RD1 inserted neutral-card ROI

A synthetic encoded-sRGB gray 0.82 patch (128×96) was placed at [64,640,192,736]
outside the candidate face ROI and passed through the known cast. It is an
artificial card patch over a photo, not a natural white object. At inverse
intensity 1.0, RGB MAE to its known source was 0.022246 / 0.000139 / 0.000392
for Reinhard / MeanStddev / Histogram, versus 0.010642 / 0.000430 / 0.000076
on the skin candidate's invertible pixels. The method ordering changes by ROI,
which supports keeping local region measurements. The sharp inserted patch
also changes global statistics, so this diagnostic is not a prediction of
real-object behavior or a quality ranking. The RD1 manifest auditor now checks
the inserted patch's encoded-to-linear values, location, and region coverage.


### RD3 Histogram boundary diagnostic

A 141-step sweep of the highest synthetic neutral patch (linear 0.84–0.98)
located the rd2 Histogram mirror discontinuity between 0.891 and 0.892. Mean
patch ΔE76 changed 7.5812 → 0.3666 as the encoded source blue bin changed
227 → 228; the source CDF at that bin changed 0.918628 → 1.0, and the nearest
reference bin changed 235 → 242. This supports a quantized upper-tail CDF
boundary as the mechanism in this fixture's global histogram. It does not
prove a compiled Core output or a product defect. Full per-step results and
CDF details are hash-recorded in `experiments/ColorMatchLab/captures/rd3_histogram_sweep/`.


### RD4 patch-area / global-histogram control

Five square ROI sizes (0.62%–10% of frame) were compared at linear target
0.891 and 0.892. Rearranging the exact same source/reference pixel multisets
kept global histograms fixed; under that control, output ROI response was
invariant across sizes. When the patch instead replaced background pixels,
the global distribution changed: at 0.891, the blue reference lookup bin
moved 224 → 241 and mean ΔE76 fell 18.825 → 1.147 as area grew; at 0.892,
the source CDF was 1.0 and the chosen bin and ΔE76 (242 / 0.367) stayed fixed.
This supports global CDF composition, rather than ROI area by itself, as the
mechanism in these synthetic cases. It is not a natural-image quality result
or Core execution evidence. Results and histogram signatures are in
`experiments/ColorMatchLab/captures/rd4_histogram_area_control/`.


### RD1 exploratory visual crops

Added reproducible face-candidate and inserted-neutral-card crop sheets for the
source, synthetic cast, and all three equation mirrors at intensity 0.5 / 1.0.
An informal unblinded inspection of preview PNGs showed a more yellow/red
appearance for Reinhard at intensity 1.0 in the face crop; differences on the
card crop were subtle at the displayed scale. This is not independent human
review, a blinded comparison, an acceptance gate, or Core output. Artifact
provenance and source image hashes pass `validate_rd1_visual_review.py`.


### RD5 existing contract-vector mirror check

The Python MeanStddev mirror reproduced the existing unequal-size-reference
contract vector with zero RGB error, including values above 1.0. The Histogram
mirror reproduced the two-level quantile vector within 0.002942 (the existing
contract tolerance is 1/255 ≈ 0.003922). This cross-checks two equation
transcriptions against numeric expectations already recorded in the C++ test
source. It does not execute the C++ tests, confirm C++/Python differential,
cover Reinhard, or exercise Core alpha behavior. Evidence is stored in
`experiments/ColorMatchLab/captures/rd5_contract_vector_probe/`.


## RD6 ColorChecker paired WB probe plan (2026-11-17)

Use a licensed Middlebury registered ColorChecker 24 pair from one camera and
exposure: Canon EOS 20D, daylight illuminant i2, exposure e3 (nominal 0 EV),
comparing camera WB1 (fixed tungsten) input JPEG to WB2 (auto) reference JPEG.
This is same-camera, same-chart, registered capture data with different
white-balance settings; WB2 is a camera rendition target, not colorimetric
ground truth. The dataset page grants permission to use and publish the images
and requests citation; this subset is retained with source-page attribution
and hashes under `experiments/ColorMatchLab/assets/MiddleburyColorChecker24/`.

Predefined masks use the 6×4 registered chart grid and central 60×60 pixels of
each 101×101 patch. Skin-tone proxy ROI is row 0, columns 0–1; the white patch
is row 3, column 0; neutral-gray ROI is row 3, columns 1–5. These are physical
ColorChecker patches, not human skin or real white objects. Compare unchanged
source baseline and all three equation mirrors at intensity 0.25 / 0.5 / 1.0.
Record CIE Lab D65 ΔE76 and RGB error against WB2 per patch and in aggregated
skin-proxy / white / gray ROIs, plus source/reference/mirror sheet. No pass
threshold or winner will be assigned; the study remains a paired camera-WB
probe, not Core execution or natural portrait acceptance.


### RD6 registered ColorChecker white-balance pair

Added a Middlebury-licensed Canon EOS 20D ColorChecker24 JPEG pair: fixed
tungsten WB1 vs auto WB2, daylight illuminant i2, exposure e3. Central 60×60
ROIs cover two standardized skin-tone chart colors, one white, five neutral
grays, and all 24 chart patches. At intensity 1.0, mean ΔE76 to WB2 was
16.434 → 6.877 / 2.999 / 1.667 for baseline → Reinhard / MeanStddev /
Histogram on the skin-tone proxy; white was 17.362 → 3.017 / 3.097 / 1.261;
neutral grays were 12.980 → 4.485 / 2.409 / 2.240. No clipping/non-finite
values were observed. WB2 is only the camera auto-WB rendition; these values
are not colorimetric ground truth, natural skin/white behavior, or product Core
output. Asset licensing, archive and image hashes are recorded in
`experiments/ColorMatchLab/assets/MiddleburyColorChecker24/SOURCES.md`; the 28
output/mask hashes and 10 scored conditions pass
`experiments/ColorMatchLab/validate_rd6_manifest.py`.


### RD7 registered natural-scene RAW/JPEG control plan

Use Middlebury's registered Canon EOS 5D kitchen scene (`scaled-im00`), with
linear-RGB RAW-rendered PNG converted by the standard sRGB transfer curve as
source and the paired camera JPEG as reference. Record this as a natural-scene
texture/camera-rendering comparison; the JPEG is not colorimetric ground truth
and processing differences may include camera tone/color response. An
unreviewed near-neutral wall rectangle is a candidate local ROI; no reflectance
claim will be made. Compare source baseline and three methods at intensities
0.25 / 0.5 / 1.0 using full-frame and candidate-wall Lab/RGB errors, clipping,
and luminance-gradient statistics for detail. This pair has no human subject
and cannot answer skin preservation. Archive and image provenance are in
`experiments/ColorMatchLab/assets/MiddleburyColorChecker24/SOURCES.md`.


### RD7 registered natural-scene rendering pair

Added a Middlebury Canon EOS 5D warm-lit kitchen scene: standard-linear-RGB
RAW render converted to sRGB, paired with its registered camera JPEG. At
intensity 1.0, mean ΔE76 to the JPEG was 14.15 baseline and 4.61 / 4.44 /
3.12 for Reinhard / MeanStddev / Histogram. The unreviewed light-wall box was
19.20 → 3.48 / 4.06 / 1.46. Refrigerator-detail mean luminance-gradient
magnitude was 0.02475 (source), 0.05560 (JPEG), and 0.06507 / 0.06339 /
0.05467 (candidates). This is a registered natural-texture/camera-rendering
control, not a calibrated target, a verified white ROI, skin evidence, or an
acceptance result. Provenance is in the asset SOURCES record; the 10 conditions
and 25 capture hashes pass `experiments/ColorMatchLab/validate_rd7_manifest.py`.


### RD8 skin-proxy hue/chroma and white-neutrality decomposition

RD8 reuses the RD6 saved arrays without rerunning or changing the RD6 capture.
It records per-patch and combined skin-proxy mean Lab, C*ab, circular hue,
per-pixel chroma distribution, source/reference distance, and white-patch
neutrality across all ten conditions. The metrics show that the WB2 target is
substantially more chromatic than WB1 on both chart skin proxies: combined
mean-vector C*ab is 15.23 (source) vs 29.74 (WB2). At intensity 1.0, the
combined value is 25.83 / 29.11 / 29.81 for Reinhard / MeanStddev / Histogram;
mean-Lab ΔE76 to WB2 is 4.898 / 1.217 / 0.077. For the lighter proxy patch,
source hue is 11.73° vs 43.71° in WB2; intensity-1 outputs are 40.16° / 40.88°
/ 42.37°. For the white patch, source and WB2 chroma are 7.85 and 9.61, but
their mean-Lab hue directions differ by 166°. Intensity-1 chroma is 6.93 /
12.24 / 9.77 and ΔE76 to WB2 is 2.890 / 2.898 / 0.508. Thus target closeness
can drive a marked saturation increase on standardized skin colors, while a
white patch may retain or increase chroma depending on method. This is a
camera-WB matching probe, not a skin-preservation or white-neutrality pass:
WB2 is not calibrated truth, and the patches are not human skin or real white
objects. Results are in
`experiments/ColorMatchLab/captures/rd8_skin_white_chroma/metrics.json`;
`validate_rd8_manifest.py` audits the input pins and metric coverage. No Core
execution, independent visual review, threshold, or acceptance gate was used.


## RD9 real portrait content with constructed cast

Added a CC0 Wikimedia Commons/Pexels image of a woman wearing a light patterned
blouse, with source and derivative hash at
`experiments/ColorMatchLab/assets/WikimediaCC0/SOURCES.md`. The source is
1280×851 and treated as sRGB because the downloaded thumbnail has no embedded
ICC profile. A fixed synthetic RGB cast creates a pixel-aligned reference;
this is not another capture or physical WB ground truth. Before running
candidate equations, the face, arm, and low-chroma light blouse ROIs were
manually inspected. The clothing threshold removes dark pattern/shadows, and
overlap between the skin and blouse masks is zero. ROIs remain single-image
analyst annotations, not independent ground truth.

In the RD9 source mirrors, intensity 1.0 Histogram lowered mean per-pixel
ΔE76 to the constructed reference from 2.944 to 0.007 on combined face/arm
skin and from 5.521 to 0.057 on the blouse ROI. On the same skin ROI mean
vector C*ab changed 14.23 → 11.77; face hue changed 42.33° → 48.50°. These
results demonstrate a concrete target-match/skin-color tradeoff in this
constructed cast: high matching accuracy does not preserve input skin
chroma/hue. They do not show that this cast is natural or that the blouse is
neutral white. Reinhard and MeanStddev show the same directional skin chroma
decrease at intensity 1.0, while also retaining larger reference errors. Full
condition and region data, plus full-frame/ROI comparison sheets, are in
`experiments/ColorMatchLab/captures/rd9_real_skin_white_scored/manifest.json`.
MeanStddev also emitted out-of-range values in this mirror: 0.732% of RGB
channel samples (1.481% of pixels) exceeded 1.0 over the full frame; the
blouse candidate ROI had 6.281% channel samples (11.346% of pixels) above 1.0,
with a maximum value of 1.059. Reinhard and Histogram had no above-1 samples
in this run. These are pre-display-clamp mirror measurements, not compiled
Core output evidence.


## RD10 oracle-mask strength tradeoff probe

For each intensity-1 global mirror, an offline post-match encoded-sRGB blend
was applied independently inside the fixed skin and blouse masks, using
factor 0 / 0.5 / 1 (source retention / midpoint / global output). The 27
combinations are an oracle-mask diagnostic, not an AutoColorMatcher API,
implementation, or performance result. For Histogram, changing skin factor
from 1 to 0.5 moved combined-skin C*ab 11.77 → 12.98 (input 14.23) and
increased reference ΔE76 0.007 → 1.465; the full-frame mean ΔE76 changed only
0.013 → 0.098. On the blouse, factor 0.5 yielded source/target ΔE76 2.782 /
2.740 and C*ab 6.10; factor 1 yielded 5.502 / 0.057 and C*ab 5.42 (input
C*ab 7.70). The metrics expose independent tradeoffs and show how a small ROI
can be obscured in full-frame averages. They do not identify a preferred
factor: the source blouse is patterned/shaded and the reference is synthetic.
All 27 masks/conditions and endpoint invariants pass
`experiments/ColorMatchLab/validate_rd9_rd10_manifests.py`.


## RD11 cast-direction sensitivity

Three predeclared synthetic encoded-sRGB casts (cool / warm / green) were
applied to the same portrait and fixed masks. The source combined-skin C*ab
was 14.23 and the light-blouse C*ab was 7.70. Their constructed targets were
11.77 / 5.41, 15.87 / 12.65, and 13.69 / 12.08 respectively. At intensity
1.0, Histogram mirror outputs had skin C*ab 11.77 / 15.92 / 13.77 and blouse
C*ab 5.43 / 12.65 / 12.08. Thus this synthetic transfer lowers skin chroma
for the cool case, raises it for warm, and changes hue/chroma differently for
green. It is a direction-sensitivity result, not a real illuminant or image
quality comparison. Four hash-pinned full-frame/face/arm/blouse comparison
sheets are available; the unblinded exploratory preview shows clearer
cast-direction changes in the blouse crop than in the face crop at montage
scale, and is not an independent human review. All 21 conditions, target ROI
metrics, 46 hashed output artifacts, and input pins pass
`experiments/ColorMatchLab/validate_rd11_manifest.py`.


## RD12 synthetic cast strength sweep

For each of the cool / warm / green cast directions, the fixed RGB offset was
scaled to 0.5 / 1.0 / 1.5 while keeping its channel scale fixed. This creates
nine synthetic references and 63 predeclared conditions (unchanged source plus
three equation mirrors at strengths 0.5/1.0 per reference). For the Histogram
mirror at intensity 1.0, combined-skin target C*ab is 10.85 / 11.77 / 12.74
for cool, 16.97 / 15.87 / 14.78 for warm, and 13.63 / 13.69 / 13.84 for
green. Blouse target C*ab is 4.90 / 5.41 / 6.02, 13.28 / 12.65 / 12.03,
and 12.93 / 12.08 / 11.24. Output mean ΔE76 to each constructed target is
0.323–0.414 on combined skin and 0.271–0.354 on blouse, while distance from
the original image remains 1.673–3.458 and 4.655–6.603 respectively. This
shows target-strength sensitivity and a clear match-versus-source-change
tradeoff; it does not determine what a real white balance should do. All 63
conditions and 130 hashed image/array/visual-sheet artifacts pass
`experiments/ColorMatchLab/validate_rd12_manifest.py`. The fixed full-frame,
face, arm and blouse sheets were inspected unblinded; strength differences are
visible in face and blouse crops, but that exploratory montage is not an
independent reviewer or quality gate.

The York/Adobe Rendered WB source describes a large real-image input/target
collection, but its official repository restricts the data to research use
unless a separate commercial license is obtained; it was not acquired. The
2025 PolyU face-centric data is scientifically relevant, but no public download
or usage terms were found in its publisher record. No new portrait data was
downloaded. This leaves actual portrait pairs and data-use permission open.

## RD13 hidden RGB under transparency

The pinned portrait was given an artificial binary alpha mask with a 20%-width
transparent strip. Two source buffers share exactly the same alpha and all
visible RGB; only alpha-zero RGB is black in one and magenta in the other. The
three equation mirrors were evaluated at strengths 0.5 and 1.0 against the
same synthetic cast reference. At strength 1.0, the visible combined-skin
outputs differ by mean ΔE76 17.73 / 25.34 / 26.06 and the blouse by 8.28 /
37.61 / 54.33 for Reinhard / MeanStddev / Histogram. For Histogram, skin
distance to the synthetic reference is 7.45 vs 24.60 and source-relative
distance is 9.35 vs 26.52 for black vs magenta hidden RGB. Six fixed comparison
sheets show the resulting global color shift when composited over black,
mid-gray, and white. All six pairs and 36 artifacts pass
`experiments/ColorMatchLab/validate_rd13_manifest.py`.

The 20% fully transparent strip is an intentionally strong synthetic
mechanism-probe, not an estimate of typical assets. This is Python equation
mirror evidence, not compiled Core output, and the straight-alpha composite
preview does not decide ArtifactStudio's alpha association contract. Hidden-RGB
handling, actual project prevalence, Core runtime, and any desired behavior
change remain open.

## RD14 transparent-area dose response

With the same fixed portrait and black/magenta hidden RGB variants, the alpha-0
strip was varied from 0%, 1%, 5%, 10%, to 20% of frame width. Each of the three
mirrors was run at intensity 1.0. The 0% condition gives identical paired
outputs. At 1%, paired visible-output mean ΔE76 is 1.59–2.79 full frame,
1.69–6.21 on skin, and 1.48–4.96 on blouse. At 10%, it is 13.45–15.91 on
skin and 5.29–36.19 on blouse; at 20% it is 17.73–26.06 and 8.28–54.33.
Fixed black-composite sheets show visible color changes increasing across the
larger synthetic strips. The full 15-condition dose grid and 68 hashed
artifacts pass `experiments/ColorMatchLab/validate_rd14_manifest.py`.

This is one synthetic portrait and a simplified vertical mask, not evidence of
how often transparent hidden RGB occurs in projects. It remains an equation
mirror, with no actual Core or alpha-association runtime comparison.

## Paired-portrait dataset search

The `Color Constancy with Faces` paper reports 472 people, manually marked
face regions, and paired images with/without a neutral card, but the accessible
project page does not expose dataset download terms; the images were not
acquired. PPR10K provides portrait masks and expert targets but restricts the
image/derived data to non-commercial research. MIT-Adobe FiveK also grants
research use under a license that excludes commercial-advantage use. The
latter two are excluded from this product-facing R&D. The remaining evidence
gap is an actual camera-paired human portrait with explicit compatible usage
rights; RD9 and RD11 use a CC0 portrait with constructed casts only.

### Additional dataset candidate check (2026-11-16)

The official [SamsungLabs S24 Raw-sRGB dataset README](https://github.com/SamsungLabs/time-aware-awb/blob/main/s24-raw-srgb-dataset/README.md)
describes 3,224 scenes with raw, in-camera sRGB, expert Lightroom sRGB, neutral
and preference WB labels, and secondary-illuminant masks. However, the
repository's published [LICENSE.md](https://github.com/SamsungLabs/time-aware-awb/blob/main/LICENSE.md)
is CC BY-NC-SA 4.0, and the dataset README does not state separate dataset terms.
Its README also says faces and other personal information are blurred. It is
therefore not adopted for this product-facing R&D or for skin ROI scoring; a
separately confirmed commercial-use grant and unblurred/consented skin material
would be required before reconsideration. Even if rights were clarified, its
raw-to-render pairs are a WB/ISP study resource, not a direct pair of differently
white-balanced sRGB source/reference images. This candidate does not close the
portrait-pair gate.

The [Color Constancy Using Faces paper](https://htchen.github.io/awb/awb_single.pdf)
continues to be scientifically relevant: it describes 472 paired RAW captures,
neutral cards, and manually labeled faces. The linked paper and accessible
project material still do not establish dataset download terms or a license,
so no images were acquired or scored.

## RD17: existing natural-scene white ROI candidate rejected (2026-11-16)

On the existing licensed Middlebury EOS 5D registered RAW/JPEG kitchen pair,
a source-only ROI was predeclared on the refrigerator door, with fixed sRGB
minimum-channel and channel-spread thresholds. The mask selected 15,613 pixels
(2.34% of the frame), but source-overlay review showed it also included
fridge magnets, photos/stickers, flowers/stems, and panel edges. Per the frozen
plan, the ROI was rejected before candidate-specific scoring; no outputs were
viewed or scored under this ROI and its numeric results must not be used as
white-preservation evidence. The mask, overlay, and rejection rationale are in
`experiments/ColorMatchLab/captures/rd17_white_object_roi/roi_review.json`.
This rejects one ROI construction, not the algorithm or dataset, and leaves
real-white ROI evaluation open.

## RD18: pendant-shade ROI on a natural-scene pair (2026-11-16)

Two manually polygoned pendant shades passed source-only overlay review
(1,222 / 1,141 pixels). On the existing Middlebury EOS 5D registered
RAW-render/JPEG pair, unchanged-source mean ΔE76 to the camera JPEG was 14.15
whole-frame and 22.60 / 22.92 on the two shade ROIs. The source L* values were
61.76 / 62.37, while the JPEG values were 84.25 / 85.13, showing that this
pair contains a large exposure/rendering difference. At intensity 1.0,
Histogram mirror reduced the ROI JPEG difference to 1.62 / 1.31, while
source-relative ΔE76 remained 23.12 / 23.27. Mean-vector C*ab moved from
0.67 / 2.72 in source and 0.63 / 0.99 in JPEG to 1.93 / 0.45 in output;
therefore target closeness is not a white-preservation score. MeanStddev also
had 16.0–18.2% of ROI RGB samples above 1.0 at intensity 1.0. Whole-frame
mean ΔE76 at intensity 1 was 4.61 / 4.44 / 3.12 for Reinhard / MeanStddev /
Histogram (source 14.15).

Fixed crop sheets were inspected unblinded. This remains source-mirror
evidence on one camera-render pair; neither the shade material's neutrality
nor the JPEG's status as color truth is established. Low-chroma hue is
unstable, and actual Core output, independent review, paired WB captures,
performance, and product route remain open. Full region metrics, conditions,
review status, and hashes are in
`experiments/ColorMatchLab/captures/rd18_lamp_white_roi/manifest.json`.

## ISSA skin spectra dataset audit (2026-11-16)

The [ISSA article and data description](https://pmc.ncbi.nlm.nih.gov/articles/PMC11930939/)
reports 15,256 skin spectral/colorimetric records from 2,113 people, arranged
in a coding spreadsheet and a datasheet, with spectral data usable to calculate
colorimetry under selected illuminants and observers. This may provide
skin-colorimetric context, but it is not a set of face images, paired
illumination captures, or spatial skin annotations and cannot close the
image-pair quality gate.

The article is CC BY 4.0, but the license attached to the dataset item at
[Figshare DOI](https://doi.org/10.6084/m9.figshare.28228571.v4) could not be
directly verified during this audit. No files were downloaded or used; do not
infer the data license from the article license. Reconsider as calibration
context only after confirming item-level terms. Even then, actual paired
portrait/reference images remain required.

## DAST controlled skin-exposure candidate (2026-11-16)

The official [DAST README](https://github.com/dasec/DAST-SkinTone-database/blob/main/README.md)
describes 80 people, 960 sRGB TIFF face images, five skin colorimeter sites,
multiple skin-tone classes, two cameras, and twelve captures per person. The
capture series includes white-card-calibrated exposure variants. The full
dataset is request-only; the README says academic and commercial use
is available by request. However, the [participant consent form](https://dasec.h-da.de/wp-content/uploads/2025/06/DAST-HDA-consent-form-250401.pdf)
states DAST-EX is provided to researchers entering a dataset license agreement
with Hochschule Darmstadt, with use constrained by that agreement. Therefore
the README alone does not establish permission to store product-R&D crops,
ROI masks, rendered candidates or metrics. Example-data terms also need direct
confirmation. No data was downloaded.

This is a promising stress set for skin-region response to exposure brackets,
not a different-white-balance pair or a pixel-ground-truth color-match target.
Before any candidate output is generated, obtain explicit terms for the
example/full files and evaluation use. If authorized material is available,
freeze same-person/same-camera pairs using nominal exposure EL3 as a conditional
reference and EL1/EL2/EL4/EL5 as sources. Use source-only face/cheek/forehead
and non-skin masks; associate measurement rows to subjects and record their
spatial uncertainty. Report source and target skin ΔE76 / ΔL* / ΔC* / hue,
measured-skin-point differences as secondary evidence, clipping/value
distributions, non-skin change and edge/detail retention. The instrument
measures about 50 mm², but public materials do not establish exact image-
coordinate mapping for each spot; report site-level comparisons with spatial
uncertainty, not pixel ground truth over a full skin ROI. The [source
presentation](https://www.christoph-busch.de/files/Busch-DAST-260423.pdf)
lists single-session capture as a limitation, so do not claim
across-session or long-term skin-color generalization. EL3 is not pixel ground
truth; camera tone curves and exposure controls remain confounds, and no
ranking/acceptance threshold will be inferred from unreviewed outputs.

This can strengthen skin-specific stress coverage only. It closes neither the
natural WB pair gap nor the neutral-white preservation gap unless the same
capture set has a visually verified neutral card in comparable conditions.
Core differential, independent blind review, product route, and performance
remain open.

## Candidate-source matrix and adoption gate (2026-11-16)

| Candidate source | Relevant evidence | Appropriate role | Remaining gap |
|---|---|---|---|
| Middlebury ColorChecker 24 (RD6) | Registered WB1/WB2 at equal exposure, known chart patches, permission to use/publish with citation | Numeric chart/neutral-axis and skin-tone-proxy control | Not human skin or natural-object appearance; already studied in RD6/RD8 |
| Middlebury natural scene (RD7/RD18) | Registered RAW/JPEG and natural texture, use/publish permission with citation | Supplemental global response and object-ROI exploration | JPEG is camera rendering; one-scene evidence with exposure/render differences and unmeasured object neutrality |
| CC0 Pexels portrait (RD9–RD14) | Actual face/arm skin and garment detail, CC0 1.0 working copy | Source-only skin ROI and controlled cast/alpha mechanism probes | Reference is synthetic; no physical skin/white ground truth or natural WB pair |
| DAST | White-card calibrated skin photography, same-person exposure brackets, measured skin sites and diverse skin-tone classes | Skin/exposure stress test if terms are confirmed | Not obtained; example terms need confirmation; exposure bracket is not a WB pair or pixel GT; neutral card visibility is unknown |
| ISSA | Skin reflectance spectra and colorimetric records across many subjects | Possible skin-colorimetric context after item-level terms check | No face images, image pairs or spatial ROIs; item license not confirmed |
| Color Constancy Using Faces | Paper reports 472 face/card pairs and face ROI labels | WB/face research resource if redistribution/use terms are established | No verified public download or license; not obtained |
| PolyU Face Color Constancy (CIC 2025) | Paper reports 1,299 face-centric images of eight people under foreground/background dual-illuminant conditions; color charts capture illuminant conditions | Exploratory mixed-illumination stress set for a global transform if legitimate pair mapping and use terms are obtained | No download/license is identified in the paper; no pixel-wise illuminant map, correction target image, or face/background mask is described |

Do not treat a paper's or repository's access statement alone as permission to
retain source material, crops, masks, derived images or metrics. The next data
adoption gate is written confirmation of permitted evaluation purpose and
storage/sharing of derivatives, plus retention and redistribution limits.
No data provider has been contacted. If explicit terms cannot be secured, use
consented in-house captures with a neutral target.

The PolyU paper describes illumination ground truth as foreground/background
conditions captured with a ColorChecker, not a pixel-wise map or a corrected
reference image. If use is authorized, first verify the mapping between each
portrait and its ColorChecker setup, then annotate face and non-face regions
from source images only. Because `AutoColorMatcher` applies a single global
transform, treat these images as a mixed-illumination stress set. Record
simultaneous face/background changes; do not score a global output as local
WB ground truth. If no single intended reference is justified, omit target
error and report only bounded source-relative response. The paper provides no
dataset URL or item-level license; do not acquire until explicit terms are
confirmed. Sources: [CIC 2025 paper](https://library.imaging.org/admin/apis/public/api/ist/website/downloadArticle/cic/33/1/15),
[PolyU research record](https://research.polyu.edu.hk/en/publications/facial-centric-color-constancy-dataset-to-improve-scenario-specif/).

## Image-effect evaluation rule audit (2026-11-16)

| Required lane | Current status | Evidence required before a product-quality conclusion |
|---|---|---|
| Effect correctness / neutral setting | Python equation mirrors and existing contract-test source; no Core execution in this R&D | Authorized Core-versus-mirror differential, method/intensity endpoints, neutral intensity and equal/unequal source/reference dimensions |
| Image quality / detail | Fixed unblinded crops and limited luminance-gradient statistics exist; natural skin/white ground truth is absent | Rights-cleared multiple portraits and measured/verified neutral material, blind 1:1 and zoom review, ROI detail/edge/texture metrics and defect log |
| Color / alpha | Hidden RGB, range and transfer probes are source mirrors only | Core tests for straight/premultiplied, transparent edges over backgrounds, known transfer/primaries and HDR display transform |
| Temporal stability | Out of scope for a single still-image operation | Any sequence/video application requires a separate synchronized frame test for flicker from frame-varying global statistics; no sequence suitability claim before then |
| Route / resolution | No application caller found; captures use fixed resolutions only | Identify real caller, compare native/full and preview/downsampled Core outputs; evaluate GPU parity only if that route exists |
| Performance / memory | Source inspection shows multiple full-frame vector allocations; runtime time, peak temporary memory and invocation frequency are unknown | Define the actual route and budget, then measure elapsed time and peak temporary memory/allocation behavior at representative resolutions |
| Acceptance thresholds | Existing metrics are descriptive; synthetic target, camera JPEG and source-relative distance have different roles | Fix reviewed materials/baselines, reviewer and region-specific tolerances before final candidate comparison |

RD9/RD18 regional statistics therefore remain exploratory evidence and do not
establish natural WB success, skin/white preservation, or method acceptance.
Because several required lanes and thresholds remain open, this R&D stays
In Progress and does not select or reject a method.

## Core test-route audit (2026-11-17)

`tests/ArtifactCore/CMakeLists.txt` registers
`ArtifactCoreAutoColorMatcherContractTest` linked against `ArtifactCore`.
Its five existing source cases call only the raw `float*` BGRA API; they cover
logical-RGB affine parameters, unequal reference dimensions for MeanStddev,
Histogram quantization, invalid/zero-intensity guards, and alpha preservation.
They do not cover `ImageF32x4_RGBA` overloads, descriptor handling, full-image
ROI output, or visual quality. The independent
`tests/ArtifactCore/ColorSpaceStandalone` module and implementation targets do
not currently include `Color.AutoMatch` / `AutoColorMatch.cpp`.

The existing full-Core contract target has now been built and run with explicit
user authorization; four cases were added. They cover image/raw path equality
and fixed mirror vectors on a small fixture; full-frame Core output versus the
float32-faithful RD6 mirror; and image-overload/raw-buffer equivalence on RD7
natural-scene and RD9 CC0 portrait/synthetic-cast inputs. RD7 is a Core
API route check, not Python differential. RD9 additionally measures actual Core
output over five regions and eleven strengths per method (165 rows); this is
exploratory evidence, not image-quality acceptance. The
contract test cannot substitute for natural paired portraits, measured white,
independent review, product route/resolution or performance gates.

## Authorized Core contract execution (2026-11-17)

Built `ArtifactCoreAutoColorMatcherContractTest` in the existing Visual Studio
18 Debug configuration with:

```powershell
$env:VSLANG = '1033'
cmake --build build --config Debug --target ArtifactCoreAutoColorMatcherContractTest -- /m:2
ctest --test-dir build -C Debug -R "^ArtifactCoreAutoColorMatcherContractTest$" --output-on-failure
```

The build exited 0 and CTest reported 1/1 test passed; verbose output shows all
nine GoogleTest cases passed. The original five cases cover logical RGB
transforms from BGRA buffers, unequal reference dimensions in MeanStddev with
alpha preservation, Histogram quantile mapping/alpha preservation,
zero-intensity and invalid-dimension guards, and intensity blending/alpha
preservation. The sixth case compares the `ImageF32x4_RGBA` overload against
the raw-buffer path for all three methods at strengths 0.25, 0.5 and 1.0, with
sRGB-encoded and linear straight descriptors. The two overloads match exactly,
alpha stays unchanged, and output is identical across the two descriptors.
For the six-pixel fixture at strength 1.0, logical RGB values also match the
independent `run_rd1_source_mirror.py` equation outputs within 2e-4 (Reinhard),
2e-6 (MeanStddev), and 1/255 (Histogram quantization). Those are numeric
fixture tolerances, not visual acceptance thresholds. This demonstrates a
small deterministic C++/mirror comparison, not natural-image quality or
descriptor-aware color management. The seventh case compares full-resolution
Core output with the float32-faithful RD6 Python mirror after PNG decode; all
nine method/intensity conditions pass within 1.5/255. RD6 and derived RD8
metrics were regenerated and their manifests validate. The eighth case compares
image-overload and raw-buffer Core outputs on full-frame RD7 natural-scene arrays;
the ninth does the same for a CC0 portrait with a deterministic synthetic cast.
Both pass all nine method/intensity combinations at 1e-6 RGB tolerance. Those
two are overload contracts, not Python differential or quality acceptance.

Source pins: test `4c1e25e5dc319fa265b61af12e24876238c9b23a739cf1e9b4c46f051ab3aa73`,
Core implementation `62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab`,
and test CMake registration `a82e80e8a8294b44835a917cdb6e9dc7582bdc99119801936e066d90071e455c`.
The user-authorized portrait/neutral-white pair has not yet appeared in the
workspace, so image R&D scoring remains pending.

Each gate is tracked separately: (1) documented rights for intended use and
retained evidence; (2) fixed source/reference roles and capture conditions;
(3) independent source-only ROI approval before candidate outputs are opened;
(4) multiple subjects, skin-tone range and capture conditions; (5) explained
mapping from skin measurements to image ROIs; (6) Core output checked against
the equation mirror; (7) whole-frame/ROI color, clipping and detail measures
plus blind review; and (8) measured time/temporary-memory budget after the
actual route is known. A missing rights or material gate stays open and is not
substituted by synthetic, chart or single-scene evidence.

## Source-path and performance context (2026-11-16)

The checked-in `ImageF32x4_RGBA` overloads in
`ArtifactCore/src/Color/AutoColorMatch.cpp` forward raw float buffers and image
dimensions without reading `colorDescriptor()`. The algorithms allocate
multiple full-frame `std::vector` buffers for global statistics and channel
processing. Repository search found no application caller, so no product
preview/export route or performance budget is currently defined. The later
limited contract execution does not measure that route or performance. Product
suitability remains unassessed.

## York/Adobe Rendered WB candidate audit (2026-11-16)

The [official dataset page](https://yorkucvil.github.io/projects/public_html/sRGB_WB_correction/dataset.html)
describes 62,535 DSLR input/target pairs, 2,881 additional cross-camera pairs,
and 10,242 rendered Cube+ pairs. Although the chart-derived targets and
chart-masked variants fit WB evaluation technically, the page says the data is
provided for purposes other than evaluating post-capture white-balance
correction and describes access as reasonable academic fair use. This is not
adequate authorization for the current product-facing R&D use. No files were
downloaded or evaluated. Require written permission that explicitly covers
this evaluation and retained crops, masks, outputs and metrics; image-level
licenses and derivative terms are not established by the page. This adds a
strong technical candidate to the exclusion record, not to the adopted corpus.

The existing CC0 synthetic-cast probes and cited Middlebury controls remain
exploratory. The real paired-portrait and verified neutral-white gates, Core
execution, independent visual review, application route and performance remain
open.

## Consent-based in-house capture fallback (planned only)

If external data rights cannot be confirmed, the evaluation plan now specifies
a consented capture path: render aligned source/reference files from one RAW
exposure under identical demosaic, camera profile, tone curve and encoding,
varying only predeclared WB multipliers derived from a measured neutral target.
Include a consenting face, a certified neutral card and measured matte white
material under the same illuminant; independently annotate source-only skin,
neutral, white, non-skin and boundary ROIs before candidate review. Require
explicit consent for the source, reference, crops, masks, outputs, metrics,
review access, storage and retention. The protocol calls for a measured skin
range across multiple subjects and more than one camera/illuminant, with pilot-
based sample sizing. This is a planned fallback only: no person was contacted,
no image was captured, and no image-rights or quality gate is closed.


## RD19 Histogram CDF precision reconciliation (2026-11-17)

The first full-image differential against RD6 previews failed only for Histogram.
Inspection of `AutoColorMatch.cpp` showed that the implementation bins float
channels after multiplying by `255.0f`, stores CDF values as `float`, accumulates
the CDF in `float`, and selects the first closest reference bin. The prior Python
mirror used float64 cumulative sums, which can move nearest-quantile bins on a
large image. The mirror was updated to reproduce the inspected float32 arithmetic
and the intensity blend rounding. RD6 was regenerated and its 10 conditions,
24 patch masks, and 28 output hashes validate; derived RD8 also regenerated and
its input pins and all 10 × 4 metric regions validate.

With the corrected mirror, the full-Core image overload matches saved previews
for all three methods and strengths 0.25 / 0.5 / 1.0 within 1.5/255 after
8-bit preview decoding. This narrows the previous mismatch to mirror precision;
it is not a discovered Core defect. The old preview comparison had maximum RGB
errors 0.012745 / 0.025490 / 0.047059 for Histogram strengths 0.25 / 0.5 / 1.0
(about 3.25 / 6.5 / 12 8-bit levels). Old output binaries were untracked and
could not be recovered after regeneration, so those maxima are retained from the
recorded differential and no mean/p99 is claimed. Current corrected outputs
and source pins are recorded in `experiments/ColorMatchLab/captures/rd19_histogram_precision/`.
RD6 chart skin tones remain proxies, not people; all visual acceptance gates
remain open.

## RD22 Actual Core portrait strength and tail sweep (2026-11-17)

The ninth contract case now measures the actual Core `ImageF32x4_RGBA`
overload on the same CC0 portrait and constructed synthetic cast for strengths
0.0 through 1.0 in 0.1 increments. The five existing reviewed regions yield
165 rows (three methods × eleven strengths × five regions), now including nearest-rank p95, maximum ΔE76, and descriptive percentage above 2 ΔE76, retained in
`experiments/ColorMatchLab/captures/rd20_core_skin_white/manifest.json` and
`metrics.json`. The metric set is unchanged: source/reference mean per-pixel
Lab D65/2° ΔE76, mean-vector and per-pixel mean C*ab, and circular mean hue.

For combined skin, source mean-vector C*ab is 14.23 and constructed-reference
C*ab is 11.77. At strength 1.0, output C*ab is 11.75 / 11.78 / 12.71 for
MeanStddev / Histogram / Reinhard. Source-relative mean ΔE76 is 2.94 / 3.06 /
1.76, while synthetic-reference mean ΔE76 is 0.19 / 0.36 / 2.05. Through the
sampled strengths, source change rises as distance to this constructed target
falls. This characterizes the Core response to this known synthetic transform;
it does not establish that the target is desirable, define tolerable skin
change, or select an operating strength. For combined skin at strength 1, source-relative p95/max/percentage above 2 are 6.19/8.25/72.45% for MeanStddev, 6.53/8.88/84.47% for Histogram, and 1.96/2.64/3.67% for Reinhard. The 2 ΔE cutoff is descriptive only, not a quality limit. Face and arm differ: at full strength,
MeanStddev source-relative ΔE76 is 3.09 on face and 2.80 on arm; target-relative
means are 0.21 and 0.16. The accompanying `strength_sweep_skin_curves.png` plots
aggregated Core metrics only, not image pixels/previews. The blouse candidate is
not measured white. Product-quality gates remain open.


The same authorized case saves source, constructed reference and actual Core PNG
outputs for each method at strengths 0.0, 0.5 and 1.0 through the existing
`ImageF32x4_RGBA::save()` I/O boundary. The sheet
`experiments/ColorMatchLab/captures/rd20_core_skin_white/core_visual_review.png`
uses fixed full-frame, face and arm crops. A single unblinded reviewer found
subtle but visible face/arm tone differences at this display scale and no
obvious structural loss. This is not an independent or blinded acceptance
review, does not set a tolerance, and says nothing conclusive about white
preservation: the blouse is patterned and unmeasured. All source/reference and
strength-1 output PNG hashes are pinned in the RD22 manifest; the test source
hash is `b2af3f825f8422b298c03b360270140d0e7a4b7d8ab38e95086609f166977866`.

## RD23 second-portrait Core stress diagnostic (2026-11-17)

A second CC0 portrait (`portrait-2024-1536.jpg`, 1536×1024) was prepared with
the same constructed per-channel cast. Its single-review face mask was revised
against an enlarged source crop so visible eyes and mouth are excluded at
overview scale. This is an analyst candidate mask, not independent annotation.
The authorized Core Debug contract test measures the actual image overload at
strengths 0 / 0.5 / 1 for all three methods, producing nine rows for 25,913
masked pixels. At strength 1, source-relative mean / p95 / maximum ΔE76 and
percentage above 2 are Reinhard 4.65 / 6.40 / 7.43 / 94.17%, MeanStddev 3.54 /
7.05 / 7.80 / 74.50%, and Histogram 3.51 / 6.80 / 8.05 / 71.05%. Mean ΔE76 to
the constructed target is 5.83 / 0.15 / 0.33, respectively. These are
descriptive results for one synthetic transform; they do not select a method,
define acceptable skin change, or establish natural WB quality. The full-frame
and face-crop sheet received one unblinded inspection. The face differences
are visible, but the inspection is not independent acceptance review. No
neutral-white ROI exists in this scene.

The full `ArtifactCoreAutoColorMatcherContractTest` target was rebuilt with
`VSLANG=1033`; CTest passed 1/1 target and GoogleTest passed 12/12 cases. RD23
metrics, test source/log hashes and output PNG hashes are in
`experiments/ColorMatchLab/captures/rd23_shadowed_portrait/manifest.json`.
Natural paired WB images, measured neutral-white evaluation, independent ROI
review, blind acceptance and application-route validation remain open.

## RD24 actual-Core ColorChecker WB-pair chart diagnostic (2026-11-17)

The registered Middlebury Canon EOS 20D pair (`wb1i2e3` fixed tungsten and
`wb2i2e3` camera auto-WB, daylight/exposure e3) was run through the actual Core
`ImageF32x4_RGBA` overload for all three methods at strengths 0.25/0.5/1.0.
The existing contract case also checks Core output against the float32 mirror;
the full 12/12 target passed. A new chart ROI report records 27 rows across
the skin-tone proxy (two patches, 7,200 pixels), white patch (3,600 pixels),
and neutral gray (five patches, 18,000 pixels), using central 60x60 pixels per
patch. It reports Lab D65/2° ΔE76 distributions relative to WB1 source and WB2,
plus output mean Lab/C*ab. At strength 1, mean ΔE76 to WB2 is 6.88/3.00/1.67
for the skin proxy, 3.02/3.10/1.26 for the white patch, and 4.49/2.41/2.24
for neutral gray (Reinhard/MeanStddev/Histogram). These are target-distance
descriptions, not preservation scores: WB2 is an in-camera auto-WB JPEG, not
calibrated truth; chart skin tones are not people and chart white is not a
natural white object. Source-relative means, full rows, Core PNGs, a visual
sheet, hashes, and limits are captured under
`experiments/ColorMatchLab/captures/rd24_colorchecker_core/`; the validator
checks the 27-row matrix, 12/12 log and hashes. The dataset permits use and
publication with citation but is not labeled CC0; its citation and source
terms are in the manifest. This does not close the required natural paired
portrait/neutral-material, independent review, caller, or product acceptance
gates.

The test also records the runtime `SurfaceColorDescriptor` for both loaded
images. Both report unknown transfer (`transfer_known=0`) and unknown primaries
(`primaries=0`); Pillow inspection found no embedded ICC profile in either PNG.
The Lab calculation therefore uses an explicit sRGB assumption, not metadata
confirmed by the Core image object. The manifest pins both descriptor lines
from the full-suite log and the validator checks the unknown states. The
current test-source hash after this instrumentation is
`5fff4be0e1900c772cbb9367dd3a9b00e65be444ea2380cf914b7f2b3654906a`.

### RD24 source-change versus WB2-distance summary

The 27 existing ROI rows were also organized as a two-axis descriptive
tradeoff: mean per-pixel ΔE76 from the WB1 source versus mean per-pixel ΔE76 to
the WB2 camera rendition. From strength 0.25 to 1.0, the measured source change
generally increases while distance to WB2 decreases. For the skin-tone proxy,
Reinhard moves from 3.36 to 13.45 source ΔE while WB2 distance goes 13.41 to
6.88; MeanStddev 3.86 to 15.76 while 12.69 to 3.00; Histogram 3.98 to 16.33
while 12.47 to 1.67. At strength 1.0, white-patch source changes are 14.53 /
19.87 / 17.44 and WB2 distances are 3.02 / 3.10 / 1.26 (Reinhard /
MeanStddev / Histogram). All candidates and region-wise nondominated points
are recorded in `experiments/ColorMatchLab/captures/rd24_colorchecker_core/
tradeoff_summary.json` by `analyze_rd24_tradeoff.py`. The frontier only
describes this diagnostic geometry; it is not a method preference, skin/white
preservation result, or quality gate. The WB2 target and sRGB Lab assumptions
retain the limits above.

The same chart ROIs were summarized for mean Lab a*/b*/C*ab in
`experiments/ColorMatchLab/captures/rd24_colorchecker_core/chroma_summary.json`.
All source, WB2 reference, and output mean-Lab values come from the same Core float measurement loop; saved 8-bit PNG quantization is excluded from the statistics.
For the skin-tone proxy, WB1 source mean C*ab is 15.23; strength-1 outputs are
25.83 / 29.11 / 29.81 (Reinhard / MeanStddev / Histogram), an increase of
10.60 / 13.88 / 14.59. For the white patch, source C*ab is 7.85 and outputs
are 6.93 / 12.24 / 9.77; for neutral gray, source is 5.70 and outputs are
8.08 / 7.97 / 7.96. These are average chroma proxies on chart patches under
the same unverified sRGB interpretation. The chart white is already chromatic
at source, so neither lower C*ab nor closeness to WB2 establishes physical
neutrality. These values are not human-skin saturation-preservation evidence.

The same RD23 case also records a limited Core raw-buffer `match()` timing sample:
one warm-up and seven calls per method, with source restore and I/O excluded.
In the latest of four Debug runs, median/min–max milliseconds for Reinhard /
MeanStddev / Histogram are 44.02 (43.14–45.18) / 14.72 (14.30–15.19) /
220.33 (213.70–225.89) at 1280×851, and 57.74 (55.33–59.38) / 21.98
(21.61–23.94) / 313.47 (312.40–318.51) at 1536×1024. The first three run
medians were 43.93 / 14.28 / 217.06 and 58.08 / 21.53 / 326.27; 44.66 /
14.24 / 219.45 and 55.51 / 21.46 / 319.13; then 44.62 / 14.33 / 218.08
and 58.07 / 22.35 / 327.08 ms. Four runs are not enough to characterize
tail latency or establish reproducibility. All reported 24 hardware threads
and used Visual Studio Debug. These values are
not an app budget or Release/GPU performance result. Peak live memory remains
unmeasured; RD28 below records a separate Debug CRT allocation-request sample.
The six timing rows are in
`core_outputs/performance.csv`; the validator
`experiments/ColorMatchLab/validate_rd23_core_capture.py` checks the RD23 rows,
logs and artifact hashes.

### RD25 actual-Core alpha / hidden-RGB influence (2026-11-17)

The RD13 CC0 portrait fixture uses an alpha-zero strip on x<256 and visible
alpha=1 elsewhere. Black-hidden and magenta-hidden source arrays have identical
visible RGB and alpha, with a shared synthetic cast reference. The actual Core
`ImageF32x4_RGBA` overload was run for all three methods at strengths 0.5 and
1.0. For paired visible outputs (871,424 pixels), RGB channel MAE / p95 and
Lab D65/2° ΔE76 mean / p95 were: Reinhard 0.0514 / 0.0877 and 7.42 / 10.91
at 0.5; 0.1035 / 0.1781 and 14.82 / 21.81 at 1.0. MeanStddev was 0.0466 /
0.1148 and 13.61 / 21.09; 0.0932 / 0.2296 and 26.82 / 42.30. Histogram
was 0.0437 / 0.1765 and 12.65 / 35.14; 0.0875 / 0.3529 and 24.83 / 68.81.
All outputs were finite and alpha was preserved exactly. This demonstrates that
transparent hidden RGB contributes to global matching statistics and changes
otherwise identical visible output in this synthetic fixture. It is not a
prevalence estimate or a product quality gate; semitransparent edges,
premultiplied inputs, background composites, and human visual acceptance remain
open. The six rows and hashes are in
`experiments/ColorMatchLab/captures/rd25_core_alpha/manifest.json`, checked by
`experiments/ColorMatchLab/validate_rd25_core_alpha.py`.

### RD26 actual-Core premultiplied-alpha numeric association (2026-11-17)

A fixed 64x64 RGB gradient and horizontal 0-to-1 alpha ramp were stored as
linear premultiplied BGRA32F and tagged with
`canonicalLinearPremultipliedBgra`. The Core image overload was measured for all
three methods at strengths 0.5 and 1.0. Outputs were finite and alpha remained
exact. Of the RGB channels, counts below zero / above alpha / nonzero at alpha
zero were Reinhard 0/155/128 and 0/229/128; MeanStddev 64/128/192 and
64/136/192; Histogram 0/64/64 at both strengths. Maximum excess over alpha was
0.00196–0.01564. The matcher therefore does not preserve the premultiplied
range invariant on this fixture even though alpha is unchanged. This is a
numeric mechanism result only: no background composites or application caller
were evaluated, and it does not establish caller expectations. The 6 rows and
hashes are in
`experiments/ColorMatchLab/captures/rd26_core_premultiplied/manifest.json`,
validated by `experiments/ColorMatchLab/validate_rd26_core_premultiplied.py`.

### Core tagged-descriptor behavior (actual contract test; 2026-11-17)

### Source-derived temporary-memory estimate (2026-11-17)

This estimate counts explicit vector element payloads in
`ArtifactCore/src/Color/AutoColorMatch.cpp`; it is not an allocator measurement,
process peak, GPU memory result, or application budget. With equal-sized source
and reference images, Reinhard and MeanStddev keep six full-frame float channel
arrays live: 24.93 MiB at 1280×851 and 36.00 MiB at 1536×1024. Their active
`computeChannelStats` call adds two double partial arrays (about 4.2 KiB / 6.0
KiB at 4096-pixel chunks). Their explicit vectors make 18 nonempty resize
growth requests per invocation (six channel vectors plus two partial arrays
for each of six stats calls). Histogram holds two full-frame float channel
arrays (8.31 MiB / 12.00 MiB) plus one partial histogram array while building
a CDF (about 0.26 MiB / 0.38 MiB); the explicit vectors make 12 nonempty
growth requests per invocation. These are source-derived vector growth
requests, not measured allocator calls; allocations inside `Parallel::ForSize`,
capacity overhead, allocator metadata, thread runtime, input/output images and
unrelated allocations are excluded. Runtime peak temporary memory remains
unmeasured; RD28 below records allocator request traffic separately. RD23
timing data does not imply a memory budget.

`ImageOverloadMatchesRawBufferAndEquationMirrorForTaggedDescriptors` supplies
the same six-pixel BGRA float arrays with sRGB and linear descriptor tags. For
all three methods at strengths 0.25/0.5/1.0, actual image-overload outputs are
exactly equal (`EXPECT_FLOAT_EQ`). The image overload forwards `rgba32fData()`
and dimensions directly to the raw-buffer overload (`AutoColorMatch.cpp`
417–420), so it does not decode/encode according to transfer metadata. This
confirms the metadata behavior for identical numeric values. It does not prove
that semantically equivalent sRGB-encoded and linear-light images match
equivalently; RD15 measured that only with independent equation mirrors.

## RD27 actual-Core natural-scene lamp ROI differential (2026-11-17)

The existing Middlebury EOS 5D kitchen RAW/JPEG pair and the RD18 source-only
reviewed lamp-shade masks were used for an actual Core diagnostic. The masks
contain 1,222 and 1,141 pixels. The dataset provider permits image use and
publication with citation; this is not a CC0 designation. The Core image
overload was run at three strengths for all three methods. The test checks
image-overload/raw-buffer parity, measures source-relative and camera-JPEG-
relative Lab D65/2° ΔE76 mean/p95/max, encoded RGB MAE and mean Lab-vector
chroma, and saves strength-1 previews. It also compares all 2,001,000 RGB
float values per method/strength against the existing RD7 equation-mirror
arrays. CTest passed the focused Debug target (1/1, 77.55 s). Histogram was
bitwise equal to the mirror; maximum absolute float difference was 2.23e-6 for
Reinhard and 1.20e-7 for MeanStddev. Across the nine condition/region strength-1
comparisons, the maximum difference in mean reference ΔE76 versus the RD18
mirror summary was 1.50e-6.

At strength 1, camera-JPEG mean ΔE76 for left/right shade ROIs was 4.81/5.18
(Reinhard), 5.50/5.50 (MeanStddev), and 1.62/1.31 (Histogram); source-relative
mean ΔE76 was 25.94/26.69, 25.66/26.26, and 23.12/23.27. One unblinded
inspection of the fixed comparison sheet found visible changes toward the
camera JPEG for all methods and judged Histogram visually closest in those two
crops. This is a descriptive single-scene observation only: the JPEG is a
processed camera rendition, and the lamp shades have not been measured as
neutral. No winner, quality gate, or product acceptance is claimed. The
Core/mirror differential and ROI values are consistent with the inspected
equation mirror on this pair, but do not generalize to other scenes or establish
the mirror as a colorimetric reference. Full evidence and hashes are in
`experiments/ColorMatchLab/captures/rd27_core_natural_scene/manifest.json`;
`record_rd27_core_natural_scene.py` checks the output matrix, ROI counts, hashes
and CTest record.

The rights-cleared user-provided portrait/WB pair, measured neutral-white
regions, independent mask review, pre-agreed product-quality thresholds, and
application caller remain open. RD27 does not close these gates.

## RD28 Debug CRT allocation-request diagnostic (2026-11-17)

The existing RD23 raw-buffer benchmark was extended with one allocation
capture per condition after warm-up. The measurement interval covers only
`AutoColorMatcher::match()`; source restore and reporting are outside the hook.
MSVC Debug `_CrtSetAllocHook` counts `_HOOK_ALLOC` and `_HOOK_REALLOC` events
and their requested bytes. The direct GoogleTest run passed 12/12 (78.85 s),
and the repeated focused CTest passed 1/1 (80.19 s).

Across the retained direct and CTest runs, 1280×851 Reinhard/MeanStddev both
produced 36 events / 26,168,826 bytes, and Histogram both produced 24 /
27,777,780 bytes. At 1536×1024, Reinhard/MeanStddev both produced 36 /
37,786,170 bytes. Histogram direct run produced 25 / 40,109,187 bytes; the
CTest repeat produced 24 / 40,108,788 (one event / 399 bytes apart). An earlier
CTest sample showed 25 / 27,778,179 for 1280 Histogram, but its CSV was
overwritten before the repeatability follow-up and is not retained as a
reproducible sample.

Source-derived explicit vector growth requests are 18 for Reinhard/MeanStddev
and 12 for Histogram. Predicted cumulative vector payload request bytes differ
from the saved observations by 570 bytes for Reinhard/MeanStddev, 756 bytes for
1280 Histogram, and 756 / 1,155 bytes for 1536 Histogram. Extra hook events
are not attributed to a specific runtime component. Histogram totals count
three sequential channel-vector pairs, so they describe allocation traffic,
not peak live workspace. Its source-derived live workspace remains about
8.57 / 12.38 MiB including the active partial histogram.

The hook can see ThreadPool CRT work but not all OS/runtime or non-CRT
allocators. It measures request traffic, not peak live bytes, working set,
Release/GPU behavior, an application call path, or an acceptance budget. Both
saved allocation CSVs and run hashes are in
`experiments/ColorMatchLab/captures/rd28_core_allocations/manifest.json` and
can be checked with `python experiments/ColorMatchLab/record_rd28_core_allocations.py`.

## RD29 actual-Core spatial-detail response diagnostic (2026-11-17)

RD29 reuses the already saved 8-bit PNGs from the RD27 Core run (Middlebury
EOS 5D scene, 1000×667, strength 1.0). Candidate outputs had been viewed
already, so this is retrospective, descriptive analysis rather than blind
acceptance. The reference camera JPEG was not treated as a detail ground truth.
Each saved PNG was interpreted as sRGB, decoded to linear sRGB, and reduced to
Rec.709 Y. Centered finite-difference gradients were measured on the full frame,
the existing source-reviewed left/right lamp masks, and fixed 18-pixel context
boxes around those masks. The edge-orientation set used only positive source
gradients at or above that region's source-only p90. PNG endpoint occupancy is
the fraction of pixels with any encoded channel exactly 0 or 255; it does not
prove that a pixel was clipped.

Across the full frame, output/source p95 gradient ratios were 2.478 / 2.440 /
2.528 for Reinhard / MeanStddev / Histogram. Mean edge-orientation cosine was
0.9912 / 0.9924 / 0.9995, and median angle was 0.41° / 0.40° / 0.54°.
Full-frame endpoint occupancy was 2.263% for source and 10.653% / 10.023% /
0.027% for the three strength-1 outputs. The transform increased gradient
contrast in this display-referred image, while strongest-edge directions mostly
remained similar over the full frame; endpoint occupancy alone cannot establish
loss of detail.

Within the small lamp masks (1,222 / 1,141 pixels), mean edge-orientation cosine
was 0.481 / 0.522 for Reinhard, 0.504 / 0.621 for MeanStddev, and 0.9965 / 0.9981
for Histogram. Median angle was 90.00° / 33.94°, 30.00° / 12.59°, and 2.07° /
2.14°. Output/source p95 gradient ratios were 1.80 / 1.13, 1.77 / 1.10, and
1.13 / 0.91. Endpoint occupancy inside these masks was 0% for source and
26.76% / 27.87%, 22.67% / 23.23%, and 0% / 0% for the three methods. These
values show substantial local edge/endpoint changes for two unmeasured smooth
lampshades in one scene; they are not a product-quality ranking, not measured
white preservation, and not evidence of human-skin detail behavior.

The first calculation incorrectly used p90 over all ROI gradients, which
included zero-gradient pixels in the near-flat lamp masks. RD29 was recomputed
with p90 over positive source gradients and zero-gradient pixels excluded. A
label audit also changed "clipped pixels" to "endpoint occupancy", since the
saved 8-bit PNG alone cannot tell natural endpoint values from clipping. The
initial CSV was overwritten before any gate or method choice used it. This
analysis uses `ImageF32x4_RGBA::save()` output converted to 8-bit sRGB, not the
retained Core float buffers, and remains one unblinded diagnostic. Full rows,
hashes, definitions and limitations are in
`experiments/ColorMatchLab/captures/rd29_core_detail/manifest.json` and
`detail_metrics.csv`; rerun with
`python experiments/ColorMatchLab/record_rd29_core_detail.py`.

## RD30 CC0 portrait skin-region detail-response diagnostic (2026-11-17)

RD30 reuses the RD9 1280×851 Wikimedia CC0 portrait, its existing source-reviewed
face (30,913 px) and arm (32,661 px) masks, unchanged source, and the nine
strength/method equation-mirror outputs. It reports full-frame, face-mask, and
arm-mask linear-sRGB Rec.709 luminance-gradient p50/p90/p95 and 3×3 population
RMS local-contrast p50/p90/p95. A local-contrast center is counted only when
its full 3×3 window lies inside the mask. Out-of-range and non-finite channels
are recorded before display clipping. The synthetic reference is excluded.

At strength 1, face-mask p95 gradient relative to source was 1.020 / 1.011 /
1.013 and p95 local RMS contrast was 1.017 / 1.010 / 1.012 for Reinhard /
MeanStddev / Histogram. Arm-mask p95 gradient ratios were 1.020 / 1.010 /
1.014 and local RMS contrast ratios were 1.020 / 1.013 / 1.017. These
descriptive statistics show small increases in the selected high-percentile
gradient and local-contrast measures in these equation mirrors. They do not
show perceptual texture visibility, accepted skin preservation, or a preferred
method. Tone remapping itself changes luminance gradients.

The masks were hand/source reviewed in RD9 but have not been independently
annotated; this retrospective diagnostic is not compiled Core output and does
not establish a real paired-capture target. No threshold, ranking, or acceptance
gate was applied. The 30-row CSV, source/mask/script hashes, metric definitions
and limitations are in
`experiments/ColorMatchLab/captures/rd30_portrait_skin_detail/manifest.json`
and `detail_metrics.csv`. Reproduce with
`python experiments/ColorMatchLab/record_rd30_portrait_skin_detail.py`.

## RD31 CC0 portrait candidate-skin color-response diagnostic (2026-11-17)

RD31 records per-pixel color-change distributions for RD9's existing equation
mirrors relative to unchanged source, separately for the candidate face and
arm masks. It uses the pinned D65/2° Lab equation mirror on clipped encoded-sRGB
values and reports ΔE76, signed a*/b*/C*ab changes, and absolute circular hue
change for pixels with source and output C*ab≥5. This eligibility cutoff only
avoids unstable hue values near neutral; it does not classify skin. The
synthetic target, blouse ROI, and any desired-skin judgement are excluded.

At strength 1, median ΔE76 / median absolute hue change was 1.68 / 0.90° for
Reinhard, 2.50 / 4.92° for MeanStddev, and 2.53 / 5.32° for Histogram in the
face mask. In the arm mask, the corresponding values were 1.63 / 0.32°,
2.38 / 4.31°, and 2.17 / 4.67°. These are descriptive differences from this
portrait's own source values, not distances to correct skin color or a method
ranking. They do not support preferred treatment because this is one
unpaired, warm/mixed-light portrait and equation mirrors rather than compiled
Core output.

The hand-reviewed candidate masks have not been independently annotated; Lab
assumes encoded sRGB with the pinned mirror. No tolerance, target hue, or
acceptance gate was set. Per-strength rows and provenance are in
`experiments/ColorMatchLab/captures/rd31_portrait_skin_color_response/manifest.json`
and `color_response.csv`; reproduce with
`python experiments/ColorMatchLab/record_rd31_portrait_skin_color_response.py`.

## RD32 actual-Core saved-preview skin color response (2026-11-17)

RD32 reuses RD22's already saved actual-Core PNG previews (strength 0.5/1.0),
the RD22 Core-saved source PNG, and RD9's existing face/arm candidate masks.
It also evaluates the RD9 float equation-mirror outputs at the same strengths
against that exact same Core-saved source baseline. This yields 24 ROI-condition
rows across actual-Core previews and equation mirrors. The summaries include
per-pixel ΔE76, signed Lab/chroma shifts, circular hue-change percentiles where
both source and output C*ab≥5, and Core preview endpoint occupancy. The output
preview PNGs were already viewed; the result is retrospective, not blind.

At strength 1 in the face ROI, Core-preview versus mirror median ΔE76 was
1.91/1.68, 2.55/2.50, and 2.59/2.53; median absolute hue change was
2.35°/0.90°, 5.18°/4.92°, and 6.37°/5.32° (Reinhard / MeanStddev /
Histogram). In the arm ROI, median ΔE76 was 1.94/1.63, 2.17/2.38, and
2.57/2.17; median hue change was 1.96°/0.32°, 4.48°/4.31°, and
5.71°/4.67°. These describe differences between saved routes on one fixture.
They are not ground-truth color errors, proof of exact Core/mirror parity, or a
method ranking. Differences combine the saved 8-bit Core output, Core-vs-mirror
implementation, and the distinct RD9 prepared versus RD22 Core-saved source
generation used for the equation mirrors.

The preview images are 8-bit; endpoint occupancy does not prove clipping. The
source is not desired skin appearance, and candidate masks are not independent
annotations. No measured white, paired photographic reference, blind visual
gate, or product acceptance is present. Full provenance, source/mask/test/Core
hashes and 24 rows are in
`experiments/ColorMatchLab/captures/rd32_core_portrait_skin_color_response/manifest.json`
and `color_response.csv`; rerun with
`python experiments/ColorMatchLab/record_rd32_core_portrait_skin_color_response.py`.

## RD33 actual-Core portrait candidate-skin lightness response (2026-11-17)

RD33 reuses the same RD22 Core-saved source baseline, face/arm candidate masks,
actual-Core PNG previews, and RD9 equation-mirror outputs as RD32. The 24 rows
report signed per-pixel ΔL*, absolute ΔL*, and output L* percentiles separately
by route, method, strength and region. All strength-1 median signed ΔL* values
were positive. Face ROI Core-preview values were +0.88 / +0.46 / +0.31 L* for
Reinhard / MeanStddev / Histogram, versus +0.79 / +0.44 / +0.47 for the
equation mirrors. Arm ROI values were +0.89 / +0.45 / +0.29 for Core previews
and +0.79 / +0.39 / +0.45 for mirrors. Absolute ΔL* p95 ranged 0.54–1.01 L*
for the six Core-preview ROI/method combinations and 0.63–0.80 for the six
mirror combinations. These are source-relative shifts in the saved display
pipeline, not an exposure target or visual-quality preference.

This one retrospective, unpaired portrait cannot establish desired face/arm
brightness, correct skin tone, measured-white preservation, float parity, or
product acceptance. Full 24-row statistics and hashes are in
`experiments/ColorMatchLab/captures/rd33_portrait_skin_lightness_response/manifest.json`
and `lightness_response.csv`; rerun with
`python experiments/ColorMatchLab/record_rd33_portrait_skin_lightness_response.py`.

### RD32/RD33 provenance preflight audit (2026-11-17)

The recorders now compare each Core source/output PNG hash with RD20's capture
manifest, the RD9 masks with the preparation manifest, and equation-mirror
outputs with RD9's scored manifest before calculating rows. RD32 records hashes
of the RD9 prep/scored manifests and its analysis/Lab conversion scripts. RD33
also records RD20, RD9, and RD32 manifest hashes, Core/test/asset hashes, and
analysis/Lab conversion script hashes. Both were rerun after this audit; row
counts and CSV hashes remained unchanged (RD32 24 rows,
`8db782c3065e4f608107945b465cdda485590c3c523b0e3a7e4d2a5a376c55f2`; RD33 24
rows, `6b6bca3c8cb4ac8f7bd81a502a4cf8dd8df05c9c84907c8f11465f1ed05331f9`).
This verifies capture provenance consistency, not quality acceptance.

## RD34 second-portrait actual-Core face response (2026-11-17)

RD34 reuses the already saved RD23 actual-Core PNG outputs and its shared
zero-strength `Reinhard_0.0.png` baseline on the 1536×1024 shadowed CC0 portrait.
The RD23 source-reviewed face-only candidate mask contains 25,913 pixels. The
seven rows report source-relative ΔE76, a*/b*/C*ab, eligible hue change, L*
shift/output distributions, and 8-bit preview endpoint occupancy. All source,
mask and output hashes are checked against RD23's manifest before analysis.

At strength 1, median ΔE76 to the Core-saved source was 5.18 / 2.96 / 3.16 for
Reinhard / MeanStddev / Histogram; median absolute eligible hue change was
5.00° / 1.90° / 2.02°. Median signed ΔL* was +0.69 / −0.42 / −0.59. Core
preview endpoint occupancy was 16.73% / 2.01% / 3.37%. These differ from RD34's
first-portrait context because the scene/skin ROI and lighting differ; this is
one additional sample, not a method comparison. Endpoint occupancy alone does
not prove clipping, and none of these source-relative values establishes
desired skin color, target correction, or acceptance.

The candidate outputs had been viewed before analysis; the mask is single-review
only and synthetic cast is not WB ground truth. This remains retrospective
8-bit preview evidence without measured white or paired capture. Detailed rows,
hashes and limitations are in
`experiments/ColorMatchLab/captures/rd34_second_portrait_core_skin_response/manifest.json`
and `skin_response.csv`; rerun with
`python experiments/ColorMatchLab/record_rd34_second_portrait_core_skin_response.py`.

## RD35 two-portrait saved-Core face-response comparison (2026-11-17)

RD35 selects strength-1 actual-Core preview face rows from RD32/RD33 (RD9
1280×851 portrait) and RD34 (RD23 1536×1024 shadowed portrait). It reports six
rows: one per method and portrait, including median/p95 ΔE76, median/p95 hue
change, median signed/p95 absolute ΔL*, and endpoint occupancy. The summarizer
verifies upstream capture CSV hashes, the selected image hash against its
upstream manifest, and consistency of the first portrait's RD32/RD33 source and
face-mask hashes.

Median ΔE76 differed across these two examples: RD9→RD23 was Reinhard
1.91→5.18, MeanStddev 2.55→2.96, Histogram 2.59→3.16. Median signed ΔL*
also changed sign for MeanStddev (+0.46→−0.42) and Histogram (+0.31→−0.59).
This indicates that the same descriptive measures vary across the two selected
images and masks. Lighting, source rendition, scene and masks all differ, so
these deltas cannot be attributed to a method or interpreted as population
behavior. Rows are not pooled or ranked.

The pair consists of unpaired CC0 portraits with synthetic cast references and
single-review candidate masks; RD22/RD23 Core outputs are saved 8-bit PNGs.
No measured white, blind review, natural WB target, fairness or product
acceptance is covered. Full provenance and hashes are in
`experiments/ColorMatchLab/captures/rd35_two_portrait_core_skin_response/manifest.json`
and `two_portrait_summary.csv`; rerun with
`python experiments/ColorMatchLab/summarize_rd35_two_portrait_core_skin_response.py`.

## Public dataset rights recheck (2026-11-17)

The official [Color Constancy with Faces project page](https://htchen.github.io/awb/) and [paper](https://htchen.github.io/awb/awb_single.pdf) describe 472 people with card-present/card-absent photographs and manual face/card regions. This is the closest structural match for skin plus a neutral reference, but the public page does not provide verified dataset download or usage terms. The paper's card-derived scene illuminant is not measured skin color or a pixel-aligned corrected image. Do not acquire/use the images pending explicit rights confirmation.

[Zenodo's skin-colour characterisation record](https://zenodo.org/records/5532176) describes a workbook pairing CM-2600d-measured skin XYZ with RGB extracted from RAW/JPG captures. A dataset license is not exposed in the record's rights section, and the workbook is not a spatial image pair or ROI set. Treat as possible colorimetric protocol context only pending item and underlying-image terms.

DAST remains a request-only candidate: the official README describes measured skin sites and exposure-series face crops, while its data/participant agreement requires a dataset license agreement. The available face crops do not establish visible measured white or a paired WB target. No provider was contacted and no external images/tables were downloaded. This audit did not score new images and does not close the predeclared natural-skin/measured-neutral acceptance gate.
