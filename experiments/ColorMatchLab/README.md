# Color Match Lab

**最終更新:** 2026-11-17

画像間色合わせのオフライン評価ラボです。Python runnerは式ミラーや集計を担当し、Core実測は `tests/ArtifactCore/AutoColorMatcherContractTest.cpp` の専用ケースで行います。synthetic fixture、ROI、source hash付きmanifest、評価制約は `EVALUATION_PLAN.md` を参照してください。

```powershell
python experiments/ColorMatchLab/run_rd0.py
python experiments/ColorMatchLab/alpha_statistics_counterfactual.py
```

出力は `captures/rd0/` です。RD22–RD27では既存 `AutoColorMatcher` のCore比較を合成portrait、ColorChecker WB pair、synthetic alpha条件、Middlebury自然シーンのlamp ROIで記録しています。自然肌・測色済み白を含む権利確認済みpaired reference、独立した人手評価、事前登録gate、製品経路の受入は未完了です。記述統計から候補の優劣を判断しません。

## Reused research sources

Provenance is recorded in `../CinematicLooksLab/assets/SOURCES.md`:

- `portrait-2024-1536.jpg` is a CC0 1.0 working copy (SHA-256 `9c14e3c0206ad6fe080c349368bf29219071892002fc01f1b6fc947af3bd40e2`) and a candidate for reviewed face/skin ROI annotation. It is not a reference target.
- `forest-wild-1536.jpg` is a CC0 1.0 working copy (SHA-256 `b417d6ffe3b494ee80906d6b433544ee7a2afa51c1557dcdf3b8feae9536ce6c`) and a non-portrait control for foliage, shadow, branch detail, and highlights.

`captures/rd0/portrait_skin_roi_candidate.json` and `portrait_skin_roi_review.png` record a manually drawn face-skin ROI candidate on the CC0 portrait. It excludes approximate eye/brow and mouth regions, but the boundary has not been independently approved and must not yet be used as a ground-truth skin mask. The portrait has strong mixed warm illumination and does not provide a neutral-white ROI or paired reference. It is excluded from acceptance metrics until reviewed and supplemented with other portraits.

No rights-cleared natural-photo source/reference pair that combines human skin with measured neutral/white regions or independently reviewed ROI masks is part of this lab yet. The synthetic rd0 chart cannot stand in for it. `alpha_statistics_counterfactual.json` is an equation-mirror mechanism only; the later RD25 capture is an actual Core hidden-RGB sensitivity measurement.

### Recent actual-Core diagnostics

The Core target now has twelve GoogleTest cases. RD22/RD23 record portrait
synthetic-cast responses, RD24 records a Middlebury ColorChecker WB-pair chart
diagnostic, RD25 measures how alpha-zero hidden RGB affects visible output, and
RD26 measures numeric premultiplied-alpha association, RD27 compares the
actual Core output with its equation mirror on a natural-scene lamp-shade ROI,
RD28 records Debug CRT allocation requests, and RD29 measures exploratory
gradient/endpoint response on RD27's saved Core PNGs.
Every Core case and saved-image diagnostic is exploratory; none establishes
natural-skin or neutral-white acceptance.

```powershell
python experiments/ColorMatchLab/validate_rd22_core_sweep.py
python experiments/ColorMatchLab/validate_rd23_core_capture.py
python experiments/ColorMatchLab/validate_rd24_core_colorchecker.py
python experiments/ColorMatchLab/validate_rd25_core_alpha.py
python experiments/ColorMatchLab/validate_rd26_core_premultiplied.py
python experiments/ColorMatchLab/record_rd27_core_natural_scene.py
python experiments/ColorMatchLab/record_rd28_core_allocations.py
python experiments/ColorMatchLab/record_rd29_core_detail.py
python experiments/ColorMatchLab/record_rd30_portrait_skin_detail.py
python experiments/ColorMatchLab/record_rd31_portrait_skin_color_response.py
python experiments/ColorMatchLab/record_rd32_core_portrait_skin_color_response.py
python experiments/ColorMatchLab/record_rd33_portrait_skin_lightness_response.py
python experiments/ColorMatchLab/record_rd34_second_portrait_core_skin_response.py
python experiments/ColorMatchLab/summarize_rd35_two_portrait_core_skin_response.py
```

RD30 reuses RD9's CC0 portrait equation-mirror outputs and hand-reviewed face/arm
mask candidates to describe local luminance-gradient and 3×3 RMS-contrast
response. It excludes the synthetic target from detail scoring and is not Core
output, independent skin annotation, or an acceptance evaluation. Results are
in `captures/rd30_portrait_skin_detail/`.

RD31 records per-pixel Lab, chroma, and eligible hue-change distributions from
the same equation mirrors relative to unchanged source. The source is only a
numeric baseline; these statistics do not define acceptable skin appearance.
See `captures/rd31_portrait_skin_color_response/manifest.json`.

RD32 compares those descriptive statistics for already saved RD22 Core PNG
previews and RD9 float mirrors against one common Core-saved source baseline.
It is a retrospective 8-bit preview analysis, not float parity or acceptance.
Results and test/source/output hashes are in
`captures/rd32_core_portrait_skin_color_response/manifest.json`; the recorder
validates each source/mask/output hash against its upstream capture manifest.

RD33 records signed/absolute Lab L* response distributions for the same Core
previews and mirror outputs. Source lightness is only a numeric baseline, not a
desired exposure. See `captures/rd33_portrait_skin_lightness_response/`.
Its recorder verifies upstream Core, prep, mirror, mask, and RD32 manifest hashes
before measuring.

RD34 adds descriptive face-only color/lightness distributions for the already
saved actual-Core previews of RD23's second, shadowed CC0 portrait. Its
single-review mask and constructed cast do not provide skin-color truth.
RD35 joins matched strength-1 face rows from the two preview captures and
checks upstream CSV, manifest, and selected PNG hashes before summarizing.
Its cross-portrait differences remain confounded by image, lighting, and mask.

RD25’s six output-pair rows are in `captures/rd25_core_alpha/manifest.json`;
RD26’s six numeric-range rows are in
`captures/rd26_core_premultiplied/manifest.json`. The user authorized Core test
execution for a pair they plan to provide, but that pair is not staged yet and
the intake template remains incomplete. Pair intake v2 records an explicit
matcher route: differing image dimensions cannot select the same-size image
overload, and require a dimension-aware route plus registration evidence. Run
`python experiments/ColorMatchLab/validate_pair_intake.py <intake.json>` before
candidate scoring; `READY FOR HUMAN REVIEW` still requires a human check of
rights, ROI and pre-output acceptance gates.

`run_rd1_source_mirror.py` creates a known warm cast of the CC0 portrait and independently transcribes the inspected Core equations for all three methods at three intensities. Results and a comparison sheet are in `captures/rd1_source_mirror/`. This is exploratory equation-mirror evidence, not compiled Core output, a natural paired-photo study, or quality acceptance. The face ROI and bright-window region remain unreviewed candidates; the latter is not established as neutral white. No gate or winner is recorded.

The same rd1 runner now also applies each method in reverse, from the known cast toward the pixel-aligned original. The inverse-cast section separates pixels recoverable under the known affine transform from pixels clipped during cast generation, and reports candidate skin/window regions separately. The SDR input and reference are explicitly constrained to [0,1]. In the invertible area, the intensity-1.0 RGB MAE was 0.009190 / 0.000328 / 0.000078 for the Reinhard / MeanStddev / Histogram source mirrors. This provides a paired synthetic recovery control only: it cannot establish natural-photo matching quality, and the ROI labels remain unreviewed. The asymmetry is consistent with Lab-space statistics versus a channel-wise affine inverse, but remains unverified until compiled Core differential.

Run `python experiments/ColorMatchLab/validate_rd1_manifest.py` to verify the rd1 capture hashes, pinned Core and runner sources, input range, condition/region coverage, and finite outputs. This is an artifact audit, not a C++ parity test.

`histogram_response_sweep.py` probes the rd2 high-neutral-level discontinuity with 141 levels and records the per-channel 8-bit source bins, source CDF, and chosen reference bin. `python experiments/ColorMatchLab/validate_rd3_manifest.py` checks the reproduced boundary and artifact hashes. The result isolates behavior for this synthetic global distribution; it does not establish a natural-image defect or acceptance result.

`white_axis_study.py` generates a second deterministic capture with four
known neutral target patches (linear levels 0.58 / 0.70 / 0.82 / 0.92) and a
warm cast in the source. It reports per-method, per-intensity neutral-axis
spread, Lab D65 ΔE76 proxy, linear Rec.709 luminance shift, RGB distance, and
clipping in `captures/rd2_white_axis/`. It is a synthetic source-mirror study,
not C++ output or natural-white object evaluation.


RD1 includes an inserted 0.82 sRGB neutral card patch at [64,640,192,736]. It
provides a known local neutral ROI under the synthetic cast; it is an artificial
patch over the portrait and does not represent a natural white object.


`histogram_patch_area_control.py` contrasts ROI size changes with unchanged
global histograms against patch-area growth that replaces background pixels.
Run `python experiments/ColorMatchLab/validate_rd4_manifest.py` to audit the
fixed-histogram and area-dependent CDF invariants. This remains synthetic
source-mirror evidence only.


`make_rd1_visual_review.py` creates face-candidate and neutral-card crops;
`python experiments/ColorMatchLab/validate_rd1_visual_review.py` checks crop and
source-preview provenance. The sheets support exploratory inspection only and
do not replace an independent/blinded visual review or Core capture.


`contract_vector_probe.py` compares the MeanStddev and Histogram equation
mirrors against fixed numeric vectors documented in the existing Core contract
test source. This is a source-level consistency probe, not a test run or C++
differential.


`run_rd6_colorchecker_pair.py` evaluates a registered Canon EOS 20D WB1-to-WB2
ColorChecker JPEG pair from Middlebury with per-patch, skin-tone proxy, white,
and gray ROIs. Review licensing and subset hashes in
`assets/MiddleburyColorChecker24/SOURCES.md`. Run
`python experiments/ColorMatchLab/validate_rd6_manifest.py` to audit the
capture. WB2 is a camera rendition, not calibrated ground truth; chart patches
do not substitute for portraits or natural white objects, and the runner is
not compiled Core output.


`run_rd7_natural_scene_pair.py` evaluates a registered Middlebury Canon EOS 5D
kitchen RAW-render/JPEG pair with an unreviewed light-wall candidate and
refrigerator-detail ROI. The source is converted from dataset linear RGB to
sRGB; the camera JPEG is not ground truth. Run
`python experiments/ColorMatchLab/validate_rd7_manifest.py` to audit provenance
and outputs. It contains no human subject and is not skin evidence.


`run_rd8_skin_white_chroma.py` decomposes the saved RD6 ColorChecker pair into
skin-proxy hue/chroma and white-patch neutrality metrics without changing RD6
outputs. Run `python experiments/ColorMatchLab/validate_rd8_manifest.py` to
check provenance and coverage. These chart/camera-rendering metrics do not
establish real-skin preservation or real-white neutrality.


`prepare_rd9_real_skin_white_scene.py` pins a CC0 portrait source, a known
synthetic cast reference and manually reviewed face/arm/patterned-blouse masks.
Review the overlay before scoring; the blouse is not a calibrated neutral.
`run_rd9_real_skin_white_scene.py` measures the three equation mirrors and
`make_rd9_visual_sheets.py` stores full-frame and ROI comparison sheets.
`run_rd10_roi_strength_tradeoff.py` probes independent ROI blend factors as a
post-match oracle-mask control, not a product implementation. Run
`python experiments/ColorMatchLab/validate_rd9_rd10_manifests.py` to audit
disjoint masks, provenance, conditions and endpoint invariants.


`run_rd11_cast_direction_sensitivity.py` reuses the RD9 portrait and masks to
measure cool, warm and green constructed-cast targets. It stores target/source
and candidate ROI Lab/hue/chroma metrics plus output artifacts under
`captures/rd11_cast_direction_sensitivity/`. Run
`python experiments/ColorMatchLab/make_rd11_visual_sheets.py` to regenerate its
fixed full-frame and ROI sheets, then
`python experiments/ColorMatchLab/validate_rd11_manifest.py` to verify the
three direction groups and hashes. All references remain synthetic.

`run_rd12_cast_strength_sensitivity.py` tests nine fixed direction/offset-
strength targets on the same portrait and masks (63 method conditions), storing
ROI-specific color, clipping, and detail metrics plus float arrays and previews.
`make_rd12_visual_sheets.py` creates fixed full-frame and ROI montages. Run
`python experiments/ColorMatchLab/validate_rd12_manifest.py` to verify all
source pins, condition coverage, finite values and 130 output hashes. This is
still synthetic reference evidence, not a real camera-WB pair or Core result.

`run_rd13_transparent_hidden_rgb.py` compares identical RGBA portrait inputs
whose RGB differs only beneath a fixed alpha-zero 20% strip (black vs magenta),
then measures visible-output changes for all three source mirrors at strengths
0.5/1.0. It stores black/mid-gray/white composite previews. Run
`python experiments/ColorMatchLab/validate_rd13_manifest.py` to audit the
identical visible source/alpha, six method pairs, ROI metrics, source pins and
36 hashes. The deliberately prominent transparency is a mechanism probe, not a
typical-project estimate or compiled Core differential.

`run_rd14_transparent_area_dose.py` varies the fully transparent strip from
0% through 20% in fixed ratios and measures all three mirrors at intensity 1.0.
The zero-area case is an exact paired-output control. Run
`python experiments/ColorMatchLab/validate_rd14_manifest.py` to audit all 15
method/fraction pairs, ROI coverage, zero-area identity and 68 output hashes.

`run_rd15_encoding_sensitivity.py` compares the source equation mirrors on
equivalent synthetic sRGB-encoded and linear-float scenes. It isolates numeric
transfer-encoding sensitivity; it does not exercise `SurfaceColorDescriptor`
or compiled Core. Outputs and per-ROI metrics are in
`captures/rd15_encoding_sensitivity/`.

`run_rd16_float_range_sensitivity.py` probes finite negative, SDR, and HDR-like
float values against an unclipped synthetic affine reference. It records
underflow/SDR/highlight errors and output range by method and intensity in
`captures/rd16_float_range_sensitivity/`. The affine construction is a
MeanStddev positive control, not a general HDR quality benchmark.

`prepare_rd17_fridge_roi.py` records a source-only fixed threshold mask and
overlay for a white refrigerator panel candidate in the licensed Middlebury
natural-scene pair. Visual mask review found magnets, pictures, flowers, and
panel edges inside the selected region, so `captures/rd17_white_object_roi/`
records rejection before candidate-specific scoring. The ROI must not be used
as evidence of white preservation.

`prepare_rd18_lamp_roi.py` creates two source-only manual polygon masks for
pendant shade candidates in the same Middlebury pair. After source overlay
review accepts the masks, `score_rd18_lamp_roi.py` measures those ROIs and the
whole frame against the camera JPEG, keeping target error separate from
source-relative change. Results are in `captures/rd18_lamp_white_roi/` and
remain one-scene source-mirror evidence, not Core output or neutral-white
acceptance.
