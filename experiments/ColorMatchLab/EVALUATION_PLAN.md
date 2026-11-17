# Color Match Lab Evaluation Plan

**最終更新:** 2026-11-17
**ステータス:** In Progress — RD15/RD16 synthetic range probes and RD18 single-scene white-object source-mirror scoring are recorded; RD17's contaminated ROI was rejected before scoring. The existing Debug Core contract target passes 12 GoogleTest cases, including fixed vectors, a full-frame RD6 float32 mirror differential, RD7 image/raw route equivalence, RD22 actual Core portrait ROI strength/tail measurements (165 rows), RD23 second-portrait synthetic-cast stress measurements (9 rows), RD24 actual-Core ColorChecker WB-pair chart measurements (27 rows), RD25 Core alpha/hidden-RGB sensitivity (6 conditions), and RD26 premultiplied-alpha numeric association (6 conditions). RD30–RD35 are retrospective descriptive detail/color/lightness analyses on saved Core previews and equation mirrors. RD22/RD23 are synthetic-cast diagnostics; RD24 is a camera auto-WB chart-target diagnostic. None is product-quality acceptance. A source-reviewed search found no dataset presently cleared for the required natural portrait source/reference pair with skin and measured neutral/white regions. Natural paired portraits, measured neutral-white materials, independent ROI review, product-quality acceptance gates, and in-application use remain open. Repository-wide text search finds no application call site for `AutoColorMatcher`. An ISSA spectral-data candidate is documented below as skin-colorimetric context only; dataset-level license terms have not been confirmed and no data was obtained.

RD27 adds an actual Core natural-scene lamp ROI diagnostic and full-frame float comparison to the existing equation mirror; the latest focused CTest rerun passed 1/1 (80.19 s). RD27 is still exploratory and does not close the natural-skin/neutral-white acceptance gap.

### Refreshed public-dataset rights and fit audit (2026-11-17)

The official [Color Constancy with Faces project page](https://htchen.github.io/awb/) and [paper](https://htchen.github.io/awb/awb_single.pdf) describe 472 people with paired card-present/card-absent photographs and manually marked face/card regions. This is the closest discovered structural match for a skin + neutral-reference pair, but the project page does not state a dataset license or public image download terms. Do not acquire or use these images until rights are explicitly confirmed. The paper's “ground-truth” is a scene illuminant estimated from a neutral card, not a measured skin color target or pixel-aligned corrected image; differences between the pair also include card presence and capture/rendering conditions.

[Zenodo's skin-colour characterisation record](https://zenodo.org/records/5532176) describes a downloadable workbook pairing spectrophotometer-measured CIE XYZ skin samples with RGB values extracted from RAW/JPG captures. The record does not expose a dataset license in its rights section and contains a spreadsheet rather than the image pair/ROI set needed here. It may inform a future colorimetric protocol only after the underlying image and data terms are confirmed; it is not a substitute evaluation fixture.

DAST remains request-only: its [README](https://github.com/dasec/DAST-SkinTone-database/blob/main/README.md) describes colorimeter-measured skin sites and exposure-series portraits, while its [data/participant agreement](https://dasec.h-da.de/wp-content/uploads/2025/06/DAST-HDA-consent-form-250401.pdf) requires a dataset license agreement. Its distributed face crops do not provide a visible measured white patch or a paired white-balance target image. No provider was contacted and no external images or tables were downloaded. These candidates therefore do not change the product-quality gate: wait for a rights-cleared user pair or explicit license evidence, then predeclare ROI and target-relative gates before scoring.

### Interface boundary audit (2026-11-17; source inspection only)

The current public convenience dispatcher does not accept separate source/reference dimensions. `AutoColorMatcher::match(float*, const float*, width, height, ...)` forwards the same width/height for both buffers, and `match(ImageF32x4_RGBA&, const ImageF32x4_RGBA&, ...)` forwards only the source dimensions. The three named algorithm methods do accept separate dimensions; the existing unequal-size contract case exercises only `meanStddevMatch`, not either dispatcher. `computeMatch(..., Method)` currently ignores its method argument and always calculates per-channel mean/stddev scale and offset, even though the declaration defaults the method to Reinhard. Its confidence is `clamp(1 - average(abs(scale - 1)), 0, 1)`, so offsets do not contribute. By direct algebra from that formula, equal source/reference channel standard deviations with nonzero mean offsets produce scale 1 and confidence 1; this is an illustrative inference, not an executed result. No application call site was found in the repository source search. These are separate from the three image matching methods and their captured results. For evaluation setup, the convenience image overload is only evidenced for equal-sized buffers and must not be used on a differently sized pair until the caller/API behavior is resolved. A future evaluator using `computeMatch` must record its fixed mean/stddev semantics and confidence interpretation rather than treating it as the selected method. No runtime out-of-bounds experiment was performed. Pair intake template v2 now requires an explicit matcher route and evidence; its preflight rejects a differently sized pair assigned to the same-size image overload, and retains the registration-mapping evidence requirement for differing dimensions.

## Image-effect evaluation rule gate audit (2026-11-17)

This is a disposition of available R&D evidence against `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`; it is not a product-quality pass.

| Required area | Existing evidence | Current disposition / remaining product gate |
|---|---|---|
| Effect response and neutral setting | Core contract vectors, intensity 0 handling, synthetic intensity sweeps (RD22), and Core-vs-equation comparisons (RD27). | Method equations and selected fixtures are described. User-pair response continuity and preferred strength remain unaccepted. |
| Overall quality and details | Full-frame Core PNGs/crops and single-review unblinded sheets for synthetic portraits, ColorChecker, and one Middlebury natural scene; RD29 measures gradient/edge response on the saved RD27 previews. Source inspection shows each method derives global statistics then transforms each pixel independently, without neighborhood sampling or a spatial filter. | No independent/blind quality review or agreed skin hue/chroma/detail limits. RD29 is 8-bit, retrospective and limited to whole-frame/lamp regions. Spatial blur from filtering is not expected from this implementation path; float clipping, quantization/banding, edge contrast changes and detail visibility still lack product acceptance gates. |
| Color and alpha | ColorChecker proxy/white/gray ROIs, descriptor checks, hidden-RGB and premultiplied-alpha synthetic Core probes. | No measured human-skin ROI paired with measured neutral/white material in a rights-cleared capture; no multi-background composite review; ICC/transfer interpretation for product inputs remains unresolved. |
| Temporal stability | `AutoColorMatcher` exposes a single-image operation and has no temporal state in the inspected API. | Not applicable to the isolated still-image API. Any sequence/product caller would require a separate frame-stability gate; no such caller was found. |
| Route, resolution, backend | Core image/raw equivalence and Core captures at 1000×667, chart resolution, 1280×851, and 1536×1024. | No production caller; no GPU/backend parity or final-render route; unequal-dimension convenience overload contract remains unresolved. |
| Performance and memory | Single Debug CPU timing series (RD23) and two Debug CRT allocation observations (RD28), plus source-derived workspace estimates. | No Release/GPU timing, peak live/process memory, target budget, invocation frequency, or app-route profile. No performance acceptance conclusion. |
| Reference statistics and acceptance | Multi-metric full-frame/ROI records, manifests and fixed masks across synthetic and permitted diagnostic pairs. | Available references are constructed casts, chart renditions, or camera JPEGs, not product ground truth. Region-specific numeric tolerances, pre-output independent review, and final adopt/reject decision remain open. |

This audit keeps the prior Core tests and diagnostic captures as supporting evidence only. It does not replace the missing rights-cleared source/reference pair, measured neutral/white evidence, independent ROI review, pre-agreed acceptance gates, or product caller needed to finish this R&D.

## RD30 CC0 portrait skin-region detail response (predeclared 2026-11-17)

Use the existing 1280×851 Wikimedia CC0 portrait in `captures/rd9_real_skin_white_scene/` and its already source-reviewed face and arm masks (30,913 / 32,661 pixels). Do not score the blouse, combine face/arm, or treat the synthetic reference as captured ground truth. Before inspecting new crops or metrics, fix the nine existing equation-mirror outputs (three methods × strengths 0.25/0.5/1.0), unchanged source, face and arm masks, and full frame. Masks were reviewed before prior RD9 scoring but not independently accepted; this retrospective use measures only this candidate mask's statistics. Decode clipped sRGB values to linear sRGB, calculate Rec.709 luminance and centered one-pixel finite-difference gradient magnitude, plus local RMS contrast (3×3 box window, population standard deviation; only centers whose full window lies inside the region). Report p50/p90/p95 gradient, RMS contrast, ratios to source, and nonfinite/out-of-range channel fractions per region. Include source/output hashes and mask hashes. No candidate target distance, ranking, pass/fail, or threshold is applied. The reference is deliberately excluded from detail metrics.

This is an exploratory diagnostic over one portrait and equation mirrors, not current Core output; input JPEG is encoded-sRGB assumed, not measured-profile evidence. Because the transform changes tones/channels, luminance-gradient response does not isolate visibility of semantic skin texture. The source-only hand mask has no independent annotation; only one face/arm and one warm/mixed-light scene are covered. Do not infer acceptable skin color/detail, natural white preservation, paired-capture correction, app/GPU output, alpha, performance, or cross-person generalization. Any manual inspection after metrics is unblinded and cannot set acceptance gates. Missing independent skin annotation and a rights-cleared paired reference remain explicit evaluation gaps.

## RD31 CC0 portrait candidate-skin color-response distribution (predeclared 2026-11-17)

Use the same RD9 CC0 portrait, unchanged source and the already fixed face/arm mask candidates; the R&D reviewer has inspected the source overlays, but masks are not independently annotated. Exclude the blouse and all synthetic references. Measure each RGB output and source in encoded sRGB as stored by the mirror, then transform clipped RGB to D65 Lab using the existing pinned `rgb_to_lab` mirror. Per mask and each of the nine equation-mirror outputs, report per-pixel ΔE76 to unchanged source (median/p90/p95), per-pixel chroma difference ΔC*ab (median/p90/p95), signed median/p90/p95 shifts in a* and b*, and hue-angle change for source/output pixels whose chroma is at least 5 in both (median/p90/p95 absolute angle and sample count). Also report raw per-channel out-of-range fraction before Lab/display clipping and non-finite counts. Unchanged source is the numeric baseline only; do not call it desired skin color. Do not use a color threshold as a skin classifier, infer skin-tone category, compare to synthetic target, rank methods, or set pass/fail limits. Save method, strength, mask/source/output/script hashes, conversion assumptions, sample counts and metric definitions.

This is a retrospective response description for one CC0 portrait and equation mirrors, not compiled Core output, an independently identified skin dataset, measured colorimetry, a paired photographic reference, or perceptual/ethical skin-tone validation. Lab values assume the RD9 encoded-sRGB input and the pinned mirror's D65 transform. Hue is unstable near neutral, so the fixed C*ab≥5 eligibility is only a numerical stability convention, not a skin-tone gate. Channel-wise distributions do not prove acceptable hue or lightness preservation. No acceptance threshold, visual ranking or product conclusion is allowed. Independent annotation, additional diverse portraits and blind reviewer criteria remain open.

## RD32 actual-Core saved-preview portrait skin color response (predeclared 2026-11-17)

Reuse the already saved RD22 actual-Core 8-bit PNG outputs for this same RD9 CC0 portrait and existing face/arm source-reviewed candidate masks. They are strength 0.5/1.0 for each of three methods. Also read RD9's nine equation-mirror `.npy` outputs at those strengths. Use the RD22 Core-saved `source.png` as the common baseline for both sets, not RD9's PIL-saved source, so numeric difference distributions share the exact same reference values. This is retrospective because candidate previews were already viewed. Keep face and arm separate; exclude blouse and synthetic reference. Decode baseline and Core previews as encoded sRGB; transform mirror float output after clipping to [0,1] to the same display-referred sRGB values; apply the pinned D65/2° Lab conversion. Report per-pixel ΔE76, signed a*/b*/C*ab shifts, absolute circular hue change for pixels where both baseline and output C*ab≥5, eligible sample count, and endpoint occupancy (any encoded channel exactly 0 or 255) for the Core PNG only. Endpoint occupancy is not proof of clipping. Save all source/output/mask hashes and the RD22 test/core source hashes from its capture manifest. Report Core-versus-mirror metric differences descriptively; this is not float parity because Core results are quantized saved previews and the equation-mirror images were generated from the separately prepared RD9 source. No threshold, method ranking, target-color judgement, or acceptance gate is allowed.

This compares saved 8-bit previews, not retained Core float output. PNG endpoint values cannot reveal whether clipping happened before save. It covers one already viewed, unpaired portrait, hand-reviewed but independently unverified masks, and only strengths 0.5/1.0. It does not validate measured skin color, neutral white, a real photographic pair, cross-demographic behavior, product route, or visual acceptance. Do not infer that Core-vs-mirror agreement would prove quality, or disagreement alone would prove a user-visible defect.

## RD33 actual-Core portrait candidate-skin lightness response (predeclared 2026-11-17)

Reuse RD32's same source baseline, face/arm masks, actual-Core strength 0.5/1.0 PNG previews and matched equation-mirror float outputs. No new image or crop review is needed; outputs were already viewed. For each route, method, strength and region, transform clipped encoded-sRGB pixels through the pinned D65/2° Lab conversion. Report signed per-pixel ΔL* from the common Core-saved source at p50/p90/p95, absolute ΔL* at p50/p90/p95, and output L* at p05/p50/p95. Preserve the `actual_core_preview` versus `equation_mirror` labels and source/output hashes; the common-source distinction and route-generation limitations in RD32 apply. Source remains a numeric baseline, not a desired exposure/skin target. No threshold, method ranking, target judgment, or acceptance gate is allowed.

This is retrospective descriptive lightness response on one unpaired CC0 portrait. Lab assumes encoded sRGB; Core values are quantized preview PNGs and mirror outputs originate from the separately PIL-prepared source. It cannot establish perceived brightness preference, exposure correctness, skin-tone acceptance, white preservation, or float parity. Independent masks and paired measured-neutral/white photography remain unverified.

## RD34 second-portrait actual-Core face color/lightness response (predeclared 2026-11-17)

Reuse RD23's already saved actual-Core 8-bit PNGs for the 1536×1024 shadowed CC0 portrait and its existing face-only source-reviewed candidate mask. Use `core_outputs/Reinhard_0.0.png` as the common Core-saved unchanged source baseline; RD23's manifest gives this exact hash as the shared output for all three zero-strength methods. The previews were already viewed, so this is retrospective. Measure unchanged Core source as a baseline row plus each method at strengths 0.5 and 1.0. Decode preview RGB as encoded sRGB and use the pinned D65/2° Lab conversion. Report per-pixel ΔE76 to the unchanged source (p50/p90/p95), signed Δa*/Δb*/ΔC*ab (same percentiles), absolute circular hue change (p50/p90/p95) only where both source/output C*ab≥5 with sample count, signed/absolute ΔL* (p50/p90/p95), output L* (p05/p50/p95), and any-channel 0/255 endpoint occupancy. Verify source, mask and output hashes against the RD23 manifest before scoring. Keep the source as numeric baseline only and the synthetic cast reference excluded.

This is a second portrait, not a second paired capture: reference is a constructed cast, the face mask has single-review provenance, and results are 8-bit Core previews. The shadow/highlight split also means regional distributions are scene-specific. No target-color error, method ranking, threshold, or acceptance gate is applied. It does not establish population/demographic fairness, neutral-white preservation, actual WB correction, independent ROI accuracy, or product acceptance.

## RD35 two-portrait saved-Core face-response summary (predeclared 2026-11-17)

Join only existing RD32/RD33 first-portrait actual-Core preview rows and RD34 second-portrait actual-Core preview rows, each at strength 1 in the face-only candidate ROI. Report method-wise median/p95 ΔE76, median/p95 absolute hue change, median signed ΔL*, p95 absolute ΔL*, and endpoint occupancy. Include pixel counts and source/mask/manifest hashes for each portrait. Compare the two portraits within each method as independent descriptive samples, without pooling pixels, averaging method outcomes, ranking methods, or declaring the smaller response preferable. Verify that all selected rows have the planned route/region/strength and that the RD32/RD33/RD34 capture hashes match their manifests before joining. No target-reference metric is used.

This is a six-row retrospective cross-fixture summary of two unpaired CC0 portraits with single-review candidate masks and already-viewed 8-bit Core previews. Portraits differ in lighting, source rendition, mask and preparation pipeline; between-portrait numeric differences are confounded and cannot be attributed to method behavior or skin-tone population. This does not establish natural WB correction, desired skin color, fairness, neutral-white preservation, or acceptance.

## RD29 actual-Core detail response on the RD27 scene (predeclared 2026-11-17)

This retrospective descriptive diagnostic uses only the already saved 8-bit PNGs from the RD27 actual-Core run (source and each method at strength 1.0); candidate images were already viewed during RD27, so this is not a blind review or acceptance gate. It asks whether the pointwise color transforms materially change spatial luminance-gradient structure on the permitted Middlebury scene. Read each PNG as sRGB, decode to linear sRGB, and compute Rec.709 Y and centered finite-difference gradients without resizing, filtering, or registration. Report for source and each Core result the mean/median/p95 gradient magnitude, candidate/source mean and p95 ratios, and source-edge orientation agreement (mean cosine and median angular difference over pixels above the source-only regional p90 gradient threshold). Compute that threshold over positive source gradient magnitudes only and exclude zero-gradient pixels from the orientation set. Also record saved-preview endpoint occupancy: fraction of pixels with any encoded RGB channel exactly 0 or 255. This is not itself proof of clipping. Use the full frame, existing source-reviewed lamp masks, and fixed 18-pixel context boxes mechanically derived from those masks. Save exact mask/PNG hashes, method, metric definitions, sample counts and the p90 threshold per region. Do not use the camera JPEG as ground truth for detail.

This uses quantized preview PNGs rather than retained Core float buffers, assumes sRGB encoding for display values, and uses lampshades that are smooth materials rather than skin or fine-texture targets. The gradient comparison mixes expected tone/color remapping with detail visibility; it can describe edge/contrast response but cannot establish no perceived loss. No acceptance tolerance or method ranking is applied. Natural-skin detail, measured-white texture, banding under other output encodings, independent review, and app/GPU paths remain unverified.

During initial RD29 calculation, p90 over all ROI gradients was zero for the nearly flat lamp masks and incorrectly selected zero-gradient pixels as edge samples. The edge set was corrected to the p90 of positive source gradients only, zero-gradient pixels excluded, and the full 15-row matrix recomputed. The initial CSV was overwritten; no method choice or acceptance threshold was based on it. A later label audit changed `clipped_pixel_fraction` to `endpoint_pixel_fraction`: the PNG alone cannot distinguish natural endpoint values from values clipped before save, so results report endpoint occupancy only and were regenerated under that corrected name.

### RD29 results (2026-11-17)

The 15-row capture used the RD27 1000×667 source and strength-1 Core PNGs. Over the full frame, output/source p95 linear-luminance gradient ratios were 2.478 / 2.440 / 2.528 for Reinhard / MeanStddev / Histogram. Mean source-edge orientation cosines were 0.9912 / 0.9924 / 0.9995, with median angular differences 0.41° / 0.40° / 0.54°. This indicates strong spatial edge-direction agreement over this frame's top-decile positive source gradients, while edge contrast changed substantially. Saved-preview endpoint occupancy for any RGB channel equal to 0 or 255 rose from 2.263% in source to 10.653% / 10.023% / 0.027% (R / M / H); endpoint occupancy does not prove that each such pixel was clipped.

Inside the left/right lamp masks, source-edge mean orientation cosine was 0.481/0.522 for Reinhard, 0.504/0.621 for MeanStddev, and 0.9965/0.9981 for Histogram. Median angle was 90.00°/33.94°, 30.00°/12.59°, and 2.07°/2.14°, respectively. Output/source p95 gradient ratios were 1.80/1.13, 1.77/1.10, and 1.13/0.91. Endpoint occupancy in the same masks was source 0% and output 26.76%/27.87%, 22.67%/23.23%, and 0%/0%. The two masks contain only 1,222/1,141 pixels and mark smooth, not measured-neutral, lampshades; low orientation agreement and endpoint occupancy are descriptive values, not proof of perceptual detail loss or a method ranking. The three images are 8-bit `toQImage()` previews from Core float outputs, so export quantization is included.

After correcting the zero-threshold edge-selection bug and renaming the endpoint metric, all rows were regenerated. Metrics, exact mask/source/output/script hashes and definitions are in `captures/rd29_core_detail/manifest.json` and `detail_metrics.csv`; rerun `python experiments/ColorMatchLab/record_rd29_core_detail.py` to reproduce. No candidate gate or threshold was set after viewing these results.

## RD27 actual-Core natural-scene lamp ROI diagnostic (predeclared 2026-11-17)

目的は、既存のRD18でsource-only確認した2つのlamp shade ROIに対し、式ミラーではなく現在のCore `ImageF32x4_RGBA` overloadが実際に出す値を記録し、同じ入力のPythonミラーとの差とcamera-JPEG referenceへの距離を記述すること。素材はMiddlebury registered EOS 5D natural-scene RAW/JPEG pair（RD7 capture、1000×667）。提供元は画像の使用・公開を許可しcitationを求めるが、これは中立度測定済みの白物体でも撮影時WB差のground truthでもない。ROIは既存`roi_review.json`のsource-only analyst reviewに承認済みとある2 polygon mask（1,222/1,141 px）をそのまま使い、candidate出力を見た後に変更しない。

未変更source、camera JPEG reference、およびCore Reinhard/MeanStddev/Histogramをstrength 0.25/0.5/1.0で比較する。全画面と各shade ROIで、Core float出力からD65/2° Lab ΔE76 mean/p95/max source-relative・reference-relative、encoded RGB MAE、chromaを測り、strength 1.0のCore PNGも保存する。既存RD18 source-mirror metricsとの差は同一領域・同一式/色空間の比較限界を明示して記録する。閾値、合否、方式順位は設定しない。結果・入力・mask・test/Core source hashをcapture manifestに保存する。

これは一sceneの実Core実行確認とROI挙動記述であり、自然肌、測色済みneutral-white、被写体一般への受入、独立レビュー、製品経路、GPU parity、性能/メモリgateを示さない。Core test変更後に既存Debug targetを実行して9条件を取得し、CSV・画像hashと既存contract targetの結果を検証する。候補比較と出力作成後にROI・指標・条件・gateを変えない。

## RD28 Debug CRT allocation diagnostic (predeclared 2026-11-17)

目的はRD23と同じCore raw-buffer `match()`入口で、各方式×1280×851/1536×1024の一回のwarm-up後に、一回の呼び出しが発生させるDebug CRTのallocation/reallocation回数と要求byte数を観測し、先の明示vector payload見積もりと照合すること。測定区間は入力restore・ログ・集計・ファイルI/Oの外側でhookを有効にし、呼び出し完了直後に解除する。各条件は一回のinstrumented sampleとし、同一Debug target内で記録する。ThreadPoolが区間中に行うDebug CRT heap操作も測定値に含め、手法のvector確保だけに分離しない。

これはMSVC Debug CRTが観測するCPU heap request count/bytesであり、peak live memory、release allocator、process working set、GPU、内部TLS/OS/runtimeが別allocatorで確保する全領域を測らない。hook導入による実行時間は性能値として扱わない。allocation count/bytesに合否閾値を置かず、ソース由来vector growth request予想との差も診断情報として残す。実アプリ経路と頻度・budgetが不明なため、hot-path適否や採用の結論は出さない。

### RD28 allocation-count repeatability follow-up (predeclared 2026-11-17)

初回のCTest実行と同じテストbinaryの直接実行を比較したところ、他の5条件は同値だったが、1280×851 HistogramでCRT event countが25から24、要求byte数が27,778,179から27,777,780へ変わった。ThreadPool/Debug CRT event数が実行形態で揺れる可能性があるため、同じsource/binary/条件をCTestで一度再実行し、直接実行結果と両方のCSV/logを保存する。対象条件・warm-up・hook区間・metricは変えず、run間のcount/byte差のみを記述する。製品品質、performance、memory合否のthresholdは加えない。初回CTestのCSVは直接実行で上書き済みなので、再現可能なraw rowとして扱わない。

### RD28結果 (2026-11-17)

Direct GoogleTest実行は12/12成功（78.85 s）、2回目のfocused CTestは1/1成功（80.19 s）。保存された2runでは、1280×851のReinhard/MeanStddevは双方36 events・26,168,826 requested bytes、Histogramは双方24 events・27,777,780 bytes。1536×1024ではReinhard/MeanStddevが双方36・37,786,170 bytes。Histogramはdirectが25・40,109,187 bytes、CTest repeatが24・40,108,788 bytesだった（1 event / 399 bytes差）。初回CTestは1280 Histogramが25・27,778,179 bytesだったが、その時のCSVはdirect runで上書き済みのため初回値をmanifestの再現可能なsampleとしては採用していない。

明示vector request byte予測との差はReinhard/MeanStddevで570 bytes、Histogramは1280条件で756 bytes、1536条件で756/1,155 bytesだった。明示vector growth予測は18/12 eventsだが、hookには追加eventが見られ、Histogram 1536のevent countがrun間で変動した。ThreadPool等の未帰属要求を含むため、countの揺れから具体的な割当元は断定しない。Histogramの要求byte合計が他方式より大きいのは3チャンネルを順に処理し、各回source/referenceフルフレームvectorを確保する累積allocation trafficを含むためである。同時liveな明示workspace概算（8.31+0.26 MiB / 12+0.375 MiB）と比べ、要求byte合計をpeak memoryと解釈しない。Direct/CTest各CSV・logのhashは`captures/rd28_core_allocations/manifest.json`に固定した。このDebug CRT/raw-buffer観測はアプリ予算、Release/GPU、working set、全allocatorのpeak live bytesを示さない。

## Core temporary-memory source estimate (2026-11-17)

This is a source-derived payload estimate, not an allocator measurement, process peak, GPU memory result, or application budget. It counts explicit `std::vector` element payloads in `ArtifactCore/src/Color/AutoColorMatch.cpp`, assumes equal source/reference pixel counts, and excludes vector capacity overhead, thread-pool/runtime allocations, input/output images, stack arrays, allocator metadata, and unrelated process memory.

Reinhard and MeanStddev hold six full-frame float channel arrays together (three source and three reference): `6 * N * sizeof(float)`, or 24.93 MiB at 1280×851 and 36.00 MiB at 1536×1024. `computeChannelStats` additionally holds two double partial arrays for one channel at a time; with 4096-pixel chunks this adds about 4.2 KiB / 6.0 KiB. Their explicit vectors also produce 18 nonempty `resize` growth requests per invocation at these sizes (six channel vectors plus two partial arrays for each of six stats calls). Histogram holds two full-frame float channel arrays at once (`2 * N * sizeof(float)`: 8.31 MiB / 12.00 MiB), plus one `std::array<int,256>` per 4096-pixel chunk while building each CDF (about 0.26 MiB / 0.38 MiB); its explicit vectors make 12 nonempty growth requests (two channel vectors and two CDF partial arrays for each of three channels). Counts are source-derived vector growth requests, not measured allocator calls; capacity overhead and allocations inside `Parallel::ForSize` are excluded. These payloads are lower-bound explicit vector storage, not measured peaks.

RD23 Debug raw-buffer timing and RD28 Debug CRT allocation traffic are documented in the milestone. Runtime peak live temporary memory, Release behavior, GPU/application route, and invocation frequency remain unmeasured. These estimates are not an acceptance budget; measure actual process memory under a real caller and target resolution after that route is identified.

## RD15 色符号化・SurfaceColorDescriptor感度（出力測定前に固定）

目的は、同じSDR色をsRGB符号化floatとlinear-light floatで表したとき、現行3方式の式が同じ見た目の結果を返すかを比較すること。入力には同一の決定的な人工色票（肌色相当、白/中立、飽和色、勾配、細かい変化）を使い、値だけをsRGB符号化またはsRGB EOTFでlinear化した2表現にする。各表現内ではsource/reference関係を同一に保ち、固定した合成チャンネルcastを逆方向に適用する。実肌・実白・撮影対の代替とはしない。

baselineは未変更source、候補は独立Python式ミラーによるReinhard/MeanStddev/Histogram、強度0.25/0.5/1.0。linear表現の出力はsRGBへ再符号化してから、対応するsRGB表現の出力と比較する。全画面、肌色相当、中立、飽和、勾配領域について、各表現の参照差、表現間の差、出力RGB分布、クリップ/範囲外/非有限値を記録する。RGB値が色の意味を表す規約はSurfaceColorDescriptorのtransfer/primariesで明示し、評価時はsRGB Rec.709 D65に揃える。閾値・合否・方式順位は設定しない。

これはソース式の感度プローブのみであり、Core実行、記述子を使った変換、C++一致の証拠ではない。現在のImageF32x4_RGBAオーバーロードは記述子を matcher に渡していないという静的所見を併記する。色票の小ささと単純さ、実装ミラー、目視未実施、ICC/HDR/alpha/性能/製品経路を限界として記録する。実測後に素材・条件・指標・ゲートを変更しない。

### RD15 結果（2026-11-16）

256×192の合成色票で、同じsRGB色を符号化floatとlinear floatに表現し、3方式×3強度を独立式ミラーで比較した。linear出力はsRGBへ戻して比較した。強度1.0時の表現間RGB MAE（全画面）はReinhard 0.00778、Histogram 0.00247、MeanStddev 0.000712。肌色相当ROIではそれぞれ0.00203、0.00247、0.000449、中立ROIでは0.00128、0.00163、0.000330。値は今回の固定合成cast・小規模色票に限った記述値であり、閾値・優劣・実画像品質を意味しない。条件・全ROI・全強度・出力hashは `captures/rd15_encoding_sensitivity/manifest.json` に記録した。

ソース確認では `setColorDescriptor` はtransfer/primariesを保持してstorage/channel orderをcanonicalizeし、ImageF32x4_RGBA版matcherはdescriptorを渡さずfloat bufferと寸法だけを呼び出す。よって本プローブは入力値の符号化を変えた時の式の感度を示すに留まり、メタデータ経由の色管理動作は評価していない。C++ differential、実際のdescriptor設定、RGBプライマリ変換、HDR、実写肌/白、目視・性能・製品経路は未検証のまま。

## RD16 負値・HDR float 値域感度（出力測定前に固定）

目的は、0〜1を超える/下回る有限float入力に対する既存3方式の値域応答を調べること。seed不要の固定人工パッチに、各チャンネル順を変えた負値（-0.25、-0.05）、SDR端と中間値（0、0.01、0.04、0.18、0.5、0.9、1.0）、高輝度（1.05、1.2、2、4）を同じ画素数ずつ含める。referenceはsource前に定めるチャンネル別scale/offsetを適用した値とし、clipしない。これはScene-Referred SDR/HDRの実写・露出変換や正解HDR画像ではない。

未変更source baselineと、Reinhard/MeanStddev/Histogramの独立式ミラーを強度0.25/0.5/1.0で比較する。固定レベル群（underflow、SDR、highlight）と全体について、referenceへのRGB MAE/p95/max、出力の負値/0〜1内/1超比率、有限値を記録する。ΔE/Labは適用しない（Reinhard式自体が0〜1へclampするうえ、HDR評価色空間/表示変換を定めていない）。合否閾値・方式順位は設定せず、各強度で入力値域がどう変わるかだけを報告する。

実行前にCoreソースの変換式と範囲を監査し、結果取得後は条件を変えない。ミラーはC++出力でない。巨大値、NaN/Inf、実線形HDRの表示変換、descriptor経由の挙動、alpha、Core differential、GPU/製品経路、性能、画質の目視は本RD16の対象外または未検証として記録する。

### RD16 結果（2026-11-16）

固定39画素の全fixtureで、sourceはreferenceにclipなしのチャンネル別scale/offsetを適用して作り、全13レベルへ各3チャンネル順を割り当てた。強度1.0の全体RGB MAEはReinhard 0.2023、MeanStddevは約1.35e-16、Histogram 0.2001だった。MeanStddevはこの設計どおりアフィン関係をほぼ完全復元するための陽性対照であり、一般的なHDR品質比較や方式優位の根拠ではない。

同じ強度1.0でReinhardは負値/高輝度値を含む入力を全て0〜1出力に収め、underflow/highlight ROI MAEは0.1056/0.5915。Histogramもunderflow/highlight ROI MAE 0.1076/0.5879、出力は0〜1内に収まった。MeanStddevは範囲を維持し、負値ROIの全チャンネルが負、高輝度ROIの58.3%が1超だった。強度0.25/0.5では元sourceを混ぜるため、MeanStddevとHistogramの出力に一部の負値/1超が残るが、Reinhard出力は範囲内だった。これは選んだ小さな合成値列への応答で、実Core differential、線形HDRの表示品質、descriptor運用、実写での望ましさは未検証。全レベル・ROI・強度・hashは `captures/rd16_float_range_sensitivity/manifest.json` にある。

## RD17 自然室内ペア上の白物体ROI候補（出力再採点前に固定）

既存Middlebury `Canon EOS 5D` の登録済みRAW/JPEG室内ペア（RD7と同じ1000×667画像）を再利用する。RAW由来linear RGBをsRGB OETFへ変換した画像をsource、カメラJPEGをreferenceとする。JPEGはカメラ現像ターゲットであり測色真値ではない。新しい素材は取得しない。

出力を作る前に、source画像の冷蔵庫右扉内 `[384,228,496,435)` を手動で候補領域として固定し、source値のみで `R,G,B > [0.36,0.33,0.30]` かつ `max(R,G,B)-min(R,G,B) < 0.16` を満たす画素を「白色面候補」maskとする。閾値と座標は候補出力を見てから調整しない。source+mask overlayでROIが冷蔵庫面に限定されているか採点前に確認し、周囲の異物・柄・境界の混入が支配的ならmaskを不採用として理由を記録し、ROI数値は品質根拠に使わない。1つの環境・白物体候補であり、白色基準板ではない。

比較は未変更source baselineとRD7の既存3方式×強度0.25/0.5/1.0ミラー出力。ROI/全画面のJPEG referenceへのencoded-sRGB MAE/p95とLab D65/2° ΔE76、source相対のRGB/Lab差、ROIの平均Lab chroma/色相、有限値とclipを記録する。referenceへの近さと入力白の色保持は別の尺度として並記し、閾値・合否・方式順位は定めない。candidateの視覚確認は固定source/reference/output cropを同じ表示条件で行うが、独立盲検レビューではない。

この評価は既存のsource-mirror出力を使うため、C++/Core differential、自然な別撮影WB参照、測色済み白、独立ROI annotation、人手盲検、性能、製品経路を証明しない。既存出力はこの再評価前に生成済みだが、この白色ROIによる候補採点・表示レビューはまだ行っていない。条件、mask hash、結果hashをRD17記録に残す。

### RD17 ROI事前確認結果（2026-11-16）

固定source-only maskは15,613画素（全体の2.34%）を選んだが、source overlay上で冷蔵庫表面以外のマグネット、写真/ステッカー、花/茎、扉境界を明確に含んだ。事前に定めた拒否条件に該当するため、ROI-specific candidate scoringは実行せず、ROI指標を品質根拠に用いない。mask/overlayと拒否理由は `captures/rd17_white_object_roi/roi_review.json` に記録した。candidate出力は見ず、threshold tuningもしていない。評価ルール上、別ROIを試すなら新条件としてcandidateを見る前に登録する。

## RD18 自然室内ペンダントシェードの手動ROI（出力採点前に固定）

同じMiddlebury Canon EOS 5D登録RAW/JPEG室内ペアを再利用し、source-only画像上の2つのペンダント照明シェードを手動で固定polygon注釈する。左シェードpolygon: `[(384,124),(426,124),(437,130),(437,140),(427,146),(388,146),(379,138),(379,131)]`。右シェードpolygon: `[(700,125),(741,125),(755,131),(757,139),(749,146),(708,146),(700,139)]`。閾値選択は追加しない。各maskは別ROIとして扱い、source overlayでシェードだけに限定されているかcandidate出力を開く前に目視確認する。ケーブル、電球/照明中心、背景または境界の混入が大きいROIは棄却し、結果に使わない。シェードは白色物体候補であって測色済み中立面ではない。

overlay採用後に限り、RD7既存の未変更sourceと3方式×0.25/0.5/1.0ミラー出力を、全画面と左右シェードごとに比較する。camera JPEGへのencoded-sRGB MAE/p95とLab D65/2° ΔE76、source相対RGB/Lab差、平均Lab C*ab/色相、clip/有限値を記録する。reference closenessとsource neutrality保持は区別して報告する。candidate cropはsource/reference/出力で同一条件の固定sheetにするが、評価者は独立・盲検ではない。閾値・合否・方式順位は置かない。

この条件も単一室内・単一カメラであり、物理中立度や照明の一様性を保証しない。Core differential、独立ROI確認、自然なWB変更ペア、複数肌色/白物体、性能、製品経路は未検証。ROI採択可否とcandidate出力を開いたタイミングをcapture記録に残す。

### RD18 結果（2026-11-16）

source overlayの確認後にROIを採用し、既存RD7ミラー9条件を採点した。左右ROIは1,222/1,141画素。未変更sourceからcamera JPEGへの平均ΔE76は全画面14.15、左右シェード22.60/22.92だった。JPEG参照の平均L*は左右84.25/85.13に対してsourceは61.76/62.37であり、ペアには大きな明度・現像差がある。強度1.0のHistogramミラーはJPEGとの差を左右1.62/1.31まで下げ、source相対ΔE76は23.12/23.27。左右のmean-vector C*abはsource 0.67/2.72、JPEG 0.63/0.99、出力1.93/0.45となった。従ってJPEGへの近さは「source白の保持」と同義ではなく、片側ではsourceよりC*abを上げた。MeanStddev強度1はROI画素の16.0〜18.2%のRGB値が1超。全画面の強度1平均ΔE76はReinhard 4.61、MeanStddev 4.44、Histogram 3.12（source 14.15）。

source/reference/候補crop sheetをunblindedに確認した。強度に応じた明度と見た目の変化が見えるが、これは一組のRAW-render/JPEG camera pairと同一シーンの登録ペアに限る。シェード表面は物理中立度を測定しておらず、JPEGも正解色ではない。Lab色相は低chromaのため解釈を限定する。全画面・ROI・各強度の結果、レビュー履歴、sheetと入力・出力hashは `captures/rd18_lamp_white_roi/manifest.json` に記録した。独立盲検、実Core、別WB撮影対、性能、製品経路は未検証。

## 外部データ候補監査: ISSA（2026-11-16）

ISSA論文とFigshareレコードの説明では、2,113人由来の15,256件の皮膚スペクトル/測色レコードを、coding schemeとdatasheetの2つの表計算ファイルで配布し、任意の照明・観測者で測色値を計算できる構成としている。これは肌色レンジや参照測色値の背景資料になり得る。一方、顔画像、照明違いの画像ペア、画像上のROIは含まれると確認できず、画像間色合わせの品質評価素材にはならない。

論文はCC BY 4.0だが、データ本体のFigshareアイテムに付されたライセンスはこの調査では直接確認できなかった。論文ライセンスをデータへ流用せず、ファイルの取得・利用・変換は保留する。ライセンス確認後も校正背景として適切な範囲だけを利用候補とし、実写画像ペアの未解決ゲートは維持する。出典: [ISSA論文とデータ構成](https://pmc.ncbi.nlm.nih.gov/articles/PMC11930939/)、[Figshare DOI](https://doi.org/10.6084/m9.figshare.28228571.v4)。

## 外部データ候補監査: 実写肌を含むWB参照ペア（2026-11-17）

画像ペアの必要条件を、同一sceneのsource/reference rendition、肌を含むscene、neutral/whiteのROI根拠、商用製品R&Dでの利用権、pixel対応/色管理の確認可能性とする。次の公開候補を一次README/配布説明で確認した。権利やpair semanticsが不明な素材は取得・採点しない。

| 候補 | 確認できた適合点 | 未達点と扱い |
|---|---|---|
| SamsungLabs S24 Raw-sRGB | 3,224 scene、in-camera sRGBと複数Lightroomレンディション、neutral/user-preference WB annotation、二次光源mask、privacy blur maskを説明。顔を含み得る自然sceneに有望。 | dataset READMEに独立したデータライセンス表示が見つからない。関連GitHub forkの`LICENSE.md`はCC BY-NC-SA 4.0でコードrepoの文書であり、画像データへの適用は確認できない。RAWとsRGBにはISP cropによるpixel shiftも明記。利用許諾とpair geometryを確認するまで素材を取得・採点しない。|
| DAST Skin Tone Database | 80人/960顔画像、CST skin tone 1–9、5部位のCortex DSM-4 colorimeter measurement、white-card calibrated sRGB TIFFを説明。full datasetはacademic/commercial use向けに申請可能。 | 12枚/subjectはnominal/under/over exposure各4枚で、WB違いのpixel-paired renditionではない。dataset license agreementと派生物の扱いを確認するまで取得しない。肌計測背景または補助候補とし、image-match参照ペアとは扱わない。|
| Color Constancy Using Faces (2011) | 論文は472人の顔+neutral card撮影対、RAW/sRGB化、face/card manual masksを説明。 | 公開論文からデータの明確な商用利用ライセンス/取得条件を確認できない。使用しない。|

以上から、このR&Dの主評価条件を満たし、かつ製品R&D利用が明示されたpairはまだない。Middleburyは「use and publish with citation」の許諾でチャート/自然室内のdiagnosticには利用できるが、skinを含むWB参照ペアではない。次の実行段階では、ユーザー提供の権利確認済み同一scene pair、またはproviderからの明示許諾を受けたペアを必要とする。**この外部候補監査は、データ配布ページへアクセスしただけで、配布物/画像をダウンロードまたは採点していない。**

受領素材は `PAIR_INTAKE_TEMPLATE.json` に原本hash、利用権の根拠と許可範囲、referenceの意味、scene/撮影差、profile/alpha、ROI由来と候補出力前レビューを記録する。`python experiments/ColorMatchLab/validate_pair_intake.py <intake.json>` は先に利用権・画像処理・派生crop/mask/metrics保管許可を検査し、不足時は画像を開かず `HOLD` を返す。許可が記録されている場合はtemplate/status/provider/pair ID、原本hash/寸法/format、ROI前レビュー、レビュー/合否ゲートを検査し、同じformat・同寸法か、または異寸法の登録写像証拠とdimension-safe matcher routeがあるかを確認する。検証スクリプトは権利文書やROI内容の正しさを自動認証しない。`READY FOR HUMAN REVIEW` 後も、人が許諾証拠・ROI・登録写像を確認し、候補出力前ゲートを確定してから採点する。権利欄やpair semanticsが不明な場合は隔離して採点しない。neutral-WB referenceとuser-preference renderingは別評価armとして扱い、同じ正解として混ぜない。baselineとROIを先に固定し、候補比較より前に担当者が色差の空間/許容範囲、肌色相/彩度と細部保持、中立白の限界、重大欠陥、判定者と意見不一致の解決方法を合意する。RD24のチャートPNGにはICC profileが埋め込まれておらず、Lab用sRGB解釈は仮定として記録した。今後profileがない素材は撮像/現像根拠を別途記し、根拠が足りなければ色差の絶対評価を保留する。

出典: [S24 dataset README](https://github.com/SamsungLabs/time-aware-awb/blob/main/s24-raw-srgb-dataset/README.md)、[関連repo license](https://github.com/mfzhang/ISP-time-aware-awb/blob/main/LICENSE.md)、[DAST dataset README](https://github.com/dasec/DAST-SkinTone-database/blob/main/README.md)、[Color Constancy Using Faces paper](https://htchen.github.io/awb/awb_single.pdf)、[Middlebury data terms](https://vision.middlebury.edu/color/data/)。

## DAST肌露出ペア候補（出力採点前の計画、2026-11-16）

公式[DAST README](https://github.com/dasec/DAST-SkinTone-database/blob/main/README.md)は80人・960顔画像・sRGB/TIFF、5つの測色部位、複数CST階級、2カメラ、個人ごと12撮影を記載する。うちWBカード校正済み条件での露出違いがあり、全件は申請制で学術・商用利用可能と説明されるが、DAST-EXはHochschule Darmstadtとのdataset license agreementで提供される。公開[参加者同意書](https://dasec.h-da.de/wp-content/uploads/2025/06/DAST-HDA-consent-form-250401.pdf)も外部研究者への共有に同契約を要求し、ライセンシーの利用をHDAの範囲外にしない条件を記載する。したがってREADMEだけで製品R&Dでの派生画像・crop・ROI・metricsの保存を許諾済みとは扱わない。example archiveも取得条件/再配布条件が直接確認できず、現時点で取得しない。

これは肌の露出耐性/肌ROIでのglobal-statistics副作用を調べるストレス素材候補で、WB色温度変換のペアやMatcherの色合わせ正解ではない。candidateを実行する前に、配布者からexample dataを含む評価用途および派生物の保存/閲覧/社内共有に関する書面条件を確認し、試料subject・camera・露出label・測色ファイル・取得物hashを固定する。素材が利用可能になったら、公開条件どおりsRGB/TIFFとして読む。個人をまたいで参照sourceとしない。同一subject・同一cameraのWB校正済みnominal exposure (EL3) をROI/基準に使い、EL1/2/4/5からEL3へ3 matcher×0.25/0.5/1.0を計測する。ただしEL3は露出条件付きの撮影画像であり、画素単位GTではない。肌は測色部位に対応する頬/額ROI、背景・髪・目・唇は別ROIとしてsource画像だけで注釈する。測色器は約50 mm²を測定するが画像ROIとの精密な座標対応は公開説明から確定できないため、ΔEを面ROI全体のpixel GTとして扱わず、測色点のsite-level比較と位置対応の不確かさを記録する。[提供側資料](https://www.christoph-busch.de/files/Busch-DAST-260423.pdf)は撮影が単一セッションである制限を挙げるため、異なる撮影日や長期的肌色変化への一般化は主張しない。

事前固定する記述指標: 画素値の有限数・0/1境界clip率・RGB分位点、EL3と各candidateの肌ROI ΔE76 / ΔL* / ΔC* / circular hue差、source-relative肌ΔE76、EL3目標との差分改善量、ROI外の平均変化、局所エッジ/細部保持。意図的な露出差のため全画面画素誤差は合否根拠にしない。カメラ個体/露出設定の完全統制、局所 tone curve/flash差は残余交絡として層別表示する。method順位、閾値、合格判定は出力を見る前に独立レビュー合意がない限り設定しない。source-only ROI確認、未変更baseline、候補確認時刻、レビュー担当、全条件manifestを残す。

この候補は「肌ROIと測色値がある同一人の露出ブラケット」であり、自然なWB色温度ペアの不足を埋めない。白保持についてはニュートラルカードが各評価フレームに残り、位置/物性/面積を独立確認できる場合だけROI候補にする。カードが含まれない、または同一条件で比較できない場合は白評価を未検証とする。C++ Core differential、独立盲検レビュー、別撮影WBペア、アプリ経路、性能は引き続き未検証。

## 素材適合性マトリクスと次ゲート（2026-11-16）

| 素材 | 強み | 本R&Dでの使い方 | 埋まらない点/制限 |
|---|---|---|---|
| Middlebury ColorChecker 24, RD6 | 登録済み同露出のWB1/WB2、既知チャートパッチ、サイト利用公開許可と引用指示 | 測色色票・肌色proxy・ニュートラル軸の定量対照 | 人の肌・自然物・編集後見た目ではない。既にRD6/RD8で調査済み |
| Middlebury natural scene, RD7/RD18 | 登録RAW/JPEG、多様な自然テクスチャ、サイト利用公開許可と引用指示 | 自然場面グローバル変化と白物体候補ROIの補助分析 | JPEGはカメラ現像結果。単一シーン結果、露出/現像差、ROIは未測色 |
| CC0 Pexels portrait, RD9–RD14 | 実際の肌・顔/腕・布ディテール、CC0 1.0の作業コピー | source-only肌ROIと制御したcast/alpha機序プローブ | 参照castは合成。肌/白の物理GTや自然WBペアでない |
| DAST | WBカード校正済み肌画像、同一人の露出ブラケット、皮膚測色値、複数skin tone | 配布許可確認後のskin/露出 stress-test候補 | 未取得・例ファイル条件未確認。露出ブラケットでありWB pair/画素GTでない。中立カードの可視性不明 |
| ISSA | 多人数の皮膚反射スペクトルと測色記録 | item-level terms確認後にskin colorimetric contextを検討 | 顔画像・paired image・ROIなし。データitem license未確認 |
| Color Constancy with Faces | 論文上は472顔/カード有無ペアと顔ROI | 配布権利が確認できた場合のWB/face研究素材 | 公開配布場所と利用条件を確認できず、未取得 |
| PolyU Face Color Constancy (CIC 2025) | 1,299枚の顔中心画像、8人、実/実験室のdual-illuminant条件。ColorCheckerから前景/背景の光源条件を取得 | 利用許諾後、同一シーン/背景照明でforeground照明が異なる群を組み、global matchの混合照明リスクを見る探索用stress set | 論文は公開配布先/利用条件を示さず、画像単位のWB reference・pixel illuminant map・ROI maskは提供内容に記載なし。明示的なペア対応と権利、ROI手作業が必要 |

この候補探索では論文/READMEの利用可能性説明だけを取得権利として扱わない。追加データ採用の次段階は、素材提供元の利用条件（評価目的、社内/商用研究、派生画像・crop・ROI・統計の保存/共有、再配布、保存期限）を権利者に確認し、回答と適用範囲を記録すること。現在は未照会で、外部連絡を行っていない。個別許諾が取れない場合は自社同意付き撮影/チャート撮影を用意する。

PolyU論文はmulti-illuminant顔写真だが、照明GTはforeground/background条件の組でありpixel mapではない。色票撮影と顔写真の対応が同じ設定単位で確認でき、顔/背景のsource-only ROIを確定できた場合だけ混合照明stress analysisへ進む。単一global `AutoColorMatcher` の出力を局所WB ground truthとして採点せず、顔ROIと非顔/background ROIの同時変化を記述し、単一照明のreferenceとして不適なら品質差分評価を見送る。公開論文にはdataset download URLまたはdata licenseが見つからなかったため、提供者から明示許諾されるまでは取得しない。出典: [CIC 2025論文](https://library.imaging.org/admin/apis/public/api/ist/website/downloadArticle/cic/33/1/15)、[PolyU研究者記録](https://research.polyu.edu.hk/en/publications/facial-centric-color-constancy-dataset-to-improve-scenario-specif/)。

受入判断は、(1) 権利文書で対象用途と結果保存が明確、(2) source/referenceの意味と撮影条件が固定され、(3) source-only ROIが候補出力を開く前に独立確認済み、(4) 複数人物・skin-tone range・複数camera/illumination条件がある、(5) 測色面と画像座標の対応が説明可能、(6) Core実行が既存式ミラーと照合済み、(7) 全体/ROIの色・clipping・detail指標と盲検レビューがある、(8) 利用経路が決まった後のtime/temporary-memory budgetが計測済み、を個別ゲートとして記録する。権利または素材がないゲートは「未達/未検証」であり、synthetic/ColorChecker/Middlebury単独の結果で代替しない。

## 必須画像評価項目の現況と完了ゲート（2026-11-17 更新）

| 画像評価ルール項目 | 現在の適用判断と証拠 | 状態/完了に必要な証拠 |
|---|---|---|
| 効果の正しさ/中立強度 | 固定小fixtureとtagged descriptor indifference、ColorChecker full-frame mirror比較、image/raw route一致、およびRD22/RD23の実Core synthetic-cast強度出力をDebug targetで実行。RD24は実Core WB chart-pairの27 ROI条件、RD25はCore alpha hidden-RGB influence、RD26はpremultiplied-alpha rangeを各6条件で記録。Core targetは12 GoogleTest cases pass | 一部確認済み: 固定fixture、2 portraits上の合成transform応答、1 chart WB rendition pair。未検証: 自然WB referenceに対する製品品質、別/異寸法実写pair、全strengthのimage quality gate |
| 画質/細部 | RD9/RD18/RD22/RD23に固定crop/sheetと単独unblinded目視記録。RD23はfull-frame/face cropを表示。布は柄/影、shadeは未測色、ROI注釈は独立レビューでない | 未検証: 権利済み複数portraitとneutral実物、独立/盲検review、ROI別detail/edge/textureと事前確定した欠陥受入値 |
| 色/alpha | RD13–16はhidden RGB、値域、transferのsource-mirror probe。raw契約testでalpha不変を確認し、RD25でbinary-alpha/hidden-RGB influence、RD26でpremultiplied-alpha numeric rangeをCore測定し、RD22/RD23はopaque portraitのCore出力を測定。RD24のsource/referenceは両方ともruntime descriptorでtransfer/primaries Unknown、file ICCもなし。別tagの同一numeric Core bufferは一致するがdescriptor変換は行われない | 部分確認済み: opaque SDR synthetic cast、Core binary-alphaでalpha値不変・hidden RGBに依存する可視出力差、premultiplied-alpha descriptorでfinite/alpha preservation。ただしRGB<=alpha constraintをviolatesするchannelあり。未検証: semitransparent edge上の背景別visual composite、profile-known transfer/primariesの実写比較、HDR display transform |
| 時間安定性 | 入力/出力1枚のstill-image API評価に限定するため本R&Dでは対象外。動画対応はプロジェクトの優先外 | 動画/連番フレーム適用は別タスク: frame-varying global statsによるflickerを同期ROI/固定露出scene sequenceで検査するまでfeature suitabilityを主張しない |
| 経路/解像度 | application callerが見つからず、全 captureは特定の固定解像度。preview/final、downsampled/full-resolution差、GPU backendは実行していない | 未検証: 実 caller/target pathの特定後、同一画像のnative/fullとpreview/downsampledをCoreで比較。GPU parityはGPU実装が対象経路に存在する場合に限る |
| 性能/メモリ | RD23でCore raw-buffer `match()`本体をVisual Studio Debug・24 hardware threads表示環境にて、1280×851/1536×1024、各方式7 timed samplesで計測。medianは直近のrunで小解像度Reinhard/MeanStddev/Histogram 44.02/14.72/220.33 ms、大解像度57.74/21.98/313.47 ms（いずれも7 samples） | 部分測定: Debug CPUの4 runのみ。app caller、Release/GPU、preview/final経路、allocation/peak memory、実行頻度・予算との比較は未検証。hot-path適否や製品性能を結論しない |
| 合否/閾値 | 現在の数値は記述比較。人工GT・camera JPEG・source-relative距離は役割が異なる | 未設定: 実素材とbaselineを固定後、候補最終比較前に担当者/独立レビューが色・白・肌・欠陥の許容値を合意する |

この監査でRD9/18/22/23/24/25/26の領域別metricを、品質合格や自然なWB成功の証拠に昇格させない。画像評価ルール上の未検証レーンが残るため、本R&Dは継続中であり、現段階で方式の採用/棄却は判定しない。

## Core differential実行経路の監査（2026-11-16）

2026-11-16時点では、`tests/ArtifactCore/CMakeLists.txt`に `ArtifactCoreAutoColorMatcherContractTest` が登録され、`ArtifactCore` libraryへリンクしていたが、test sourceには `float*` raw BGRA buffer APIだけを呼ぶ5ケースがあった。この記録は当日のsnapshotであり、現在のtest sourceには後続ケースが追加されている。

独立 `tests/ArtifactCore/ColorSpaceStandalone` は色変換/画像面契約を登録するが、`Color.AutoMatch` moduleと `AutoColorMatch.cpp` をその module/implementation targetsに含めていない。したがってこのstandalone suiteは現状、matcherの既存差分実行経路ではない。full ArtifactCore suiteはCore parityの最初の実行場所だが、契約テストがpassしても肌/白の画像品質証拠にはならない。

### RD22/RD23/RD24/RD25/RD26 Core実測の現在地（2026-11-17）

許可済みの `ArtifactCoreAutoColorMatcherContractTest` はVisual Studio 18 Debugでbuildし、CTest target 1/1・GoogleTest 12/12 pass。小さい固定fixtureのCore契約、ColorChecker Core/float32 mirror比較、複数のimage/raw API一致に加え、RD22のCC0 portrait合成cast 165 ROI測定、RD23の別CC0 portrait合成cast 9 ROI測定、RD24のMiddlebury WB pair chart測定27行とRD25/RD26のCore alpha 6+6条件を実Core `ImageF32x4_RGBA` overloadで実行した。RD24はskin-tone proxy 2色票・白色票1枚・neutral gray5枚の中央60×60 ROIを3方式×3強度で測定し、source/WB2参照へのLab D65/2° ΔE76、出力平均Lab/C*ab、Core画像とレビューシートを保存した。WB2はカメラauto-WB JPEGで測色的ground truthではなく、各色票も実人肌・自然白ではない。RD22/RD23/RD24/RD25/RD26の結果は探索診断に限定し、方式合格を示さない。ユーザーは権利確認済みsource/reference pairを提供し、ArtifactCore test実行を許可すると回答済み。現在の作業ツリーにはそのpairがまだ無く、intake templateは未記入のため候補品質の採点は保留中。

Core tagged-descriptor check: `ImageOverloadMatchesRawBufferAndEquationMirrorForTaggedDescriptors` feeds the same six BGRA pixels with sRGB-tagged and linear-tagged descriptors into the actual image overload. Across all three methods and strengths 0.25/0.5/1.0, outputs are exactly equal (`EXPECT_FLOAT_EQ`), matching the source path at `AutoColorMatch.cpp` 417–420 that forwards raw float buffers and dimensions without descriptors. This confirms metadata is ignored for identical numeric buffers. It does not test semantically equivalent colors stored in different transfer encodings; RD15 remains an equation-mirror-only sensitivity probe for that case.

これはCoreの既存経路に対する限定測定であり、RD24のWB2 camera JPEGを色差正解とは扱えず、独立/盲検visual acceptance、自然白保持や肌色許容値を評価していない。2026-11-16の「未実行」記録は当日の状態を示す歴史snapshotとして読む。現在も `ColorSpaceStandalone` にmatcher登録を追加しておらず、製品caller、GPU/preview/final path、Release時性能、allocation/peak memory、予算対比は未確認。RD23の単回Debug CPU timingは初期参考値のみ。R&D完了にはCore出力/必要なmirror差分に加えて、権利・pair semantics・独立ROI・color/alpha、実経路・性能などの未達ゲートを個別に閉じる必要がある。

### RD25 Core alpha / hidden-RGB influence (predeclared; 2026-11-17)

Reuse the RD13 CC0 portrait fixture and its fixed binary-alpha strip (x < 256,
alpha=0; remaining 80% alpha=1). The two source arrays have identical visible
RGB and alpha; transparent RGB is black in one and magenta in the other. Reuse
the same constructed RGB reference. Before opening any new Core result, measure
the actual `ImageF32x4_RGBA` overload for Reinhard, MeanStddev, and Histogram at
intensity 0.5 and 1.0. On the fixed alpha>0 pixels, record paired Core-output
RGB absolute-difference mean/p95/max and Lab D65/2° ΔE76 mean/p95/max; verify
finite output values and exact alpha preservation. No threshold or quality
gate is set. This isolates whether fully hidden RGB changes global-match
statistics and thereby alters visible pixels. Actual Core execution passed all
12 GoogleTest cases and measured all six conditions. For Reinhard, paired
visible-output mean ΔE76 / p95 is 7.42 / 10.91 at 0.5 and 14.82 / 21.81 at
1.0. MeanStddev is 13.61 / 21.09 and 26.82 / 42.30; Histogram is 12.65 /
35.14 and 24.83 / 68.81. The six max ΔE76 values range from 24.23 to 89.45.
Per-channel RGB MAE ranges 0.0437–0.1035; all outputs are finite and alpha is
preserved exactly. This confirms hidden transparent RGB changes global source
statistics and measurably changes otherwise identical visible pixels in this
fixture. It does not establish real-project prevalence or a product defect
threshold. Metrics and provenance hashes are in
`captures/rd25_core_alpha/manifest.json`; `validate_rd25_core_alpha.py` checks
coverage and hashes. It is a synthetic binary-alpha
mechanism probe, not a typical project prevalence estimate, a product alpha
contract for premultiplied images, or photographic acceptance. Input masks and
constructed target are pinned to RD13; the current user-provided natural image
pair is still required for the primary product-quality question.

### RD26 Core premultiplied-alpha numeric contract (predeclared; 2026-11-17)

Use a fixed 64x64 synthetic RGB gradient with a horizontal alpha ramp from 0
to 1. Construct source and reference buffers as premultiplied BGRA32F and tag
both `SurfaceColorDescriptor` values as linear-premultiplied BGRA. Before
opening results, measure the actual Core image overload for all three methods
at strengths 0.5 and 1.0. Record finite output status, exact alpha preservation,
RGB channel counts below zero or greater than alpha (the premultiplied-range
constraint), maximum excess over alpha, and nonzero RGB in fully transparent
pixels. Set no acceptance threshold. This isolates whether the current Core
path preserves numeric premultiplied association after matching; it does not
measure visual quality or represent a production image distribution.

### RD24 source-change / WB2-distance tradeoff（2026-11-17）

27個の既存Core ROI測定を、WB1 sourceからのmean Lab D65/2° ΔE76とWB2 camera-JPEG renditionまでのmean ΔE76の2軸で整理した。strength 0.25→1.0では全体にsource距離が大きくなりWB2距離が小さくなる。例えばskin proxyではReinhard 3.36→13.45 source ΔEと13.41→6.88 WB2距離、MeanStddev 3.86→15.76と12.69→3.00、Histogram 3.98→16.33と12.47→1.67。white patchでもsource変化14.53/19.87/17.44とWB2距離3.02/3.10/1.26（strength 1.0）となる。tradeoffのPareto frontierと全候補値は `captures/rd24_colorchecker_core/tradeoff_summary.json` に保存した。これはチャート上で目標レンディションへ近づける代わりに入力の色を変える量を示す記述図であり、肌/白の保持合格や望ましい目標を決めない。sRGBのLab計算仮定、WB2が測色的正解でないこと、チャート色票と自然物の差も継続して適用する。

平均a*/b*/C*abも `captures/rd24_colorchecker_core/chroma_summary.json` にまとめた。source・WB2 reference・outputすべての平均Labを同じCore float測定ループから得ており、保存PNGの8-bit量子化差は集計に含まれない。strength 1のskin-tone proxy C*abはWB1 source 15.23からReinhard/MeanStddev/Histogramの25.83/29.11/29.81へ増加。white patchはsource 7.85から6.93/12.24/9.77、neutral grayはsource 5.70から8.08/7.97/7.96へ変化した。これは同じ未知transfer/primariesをsRGBと仮定したchart ROIの平均彩度proxyであり、実人肌の彩度保持や材質中立度を表さない。特にチャートの「white」はsource時点で無彩色ではなく、測色済みneutral whiteとは扱わない。

## ソース経路監査の補足（2026-11-16）

`ArtifactCore/src/Color/AutoColorMatch.cpp` の現行画像オーバーロードは、`rgba32fData()`と寸法のみをraw-buffer関数へ渡し、`ImageF32x4_RGBA::colorDescriptor()`を参照しない。matcherソース内では方式別の全画像統計用 `std::vector` が複数確保される。よってdescriptor対応とフレーム/ホットパス適性は未評価であり、製品呼び出し元が見つからない現状で性能適合とは判断しない。実際の利用文脈を定めずに製品統合や時間予算の合否を推定しない。

製品品質の完了判定には、権利確認済みの実撮影ペアと肌/測色中立領域、Core出力比較、色記述子・alpha契約、独立ROIレビュー、対象レンダリング経路と性能計測が引き続き必要。

## RD9 実写ポートレート上の既知色かぶり診断（出力採点前に固定）

素材はWikimedia Commonsの`Pexels-photo-707448.jpg`（CC0 1.0）1280×851 JPEG。ファイルの色プロファイルは取得サムネイルに含まれないため、符号化RGBをsRGBと仮定する。参照は同じ画素に固定変換 `clip(src * [0.94, 1.015, 1.075] + [0.012, 0.006, -0.008], 0, 1)` を適用して作り、実撮影の異なるWBや物理的な正解色とは扱わない。

出力を作る前に、同一座標の顔肌、上腕肌、両者の結合、柄物の白/アイボリー布ROIを手動確認した。布は目視で選んだ衣服ポリゴンに入力画像由来の固定しきい値（R>0.48、G>0.45、B>0.38、max(R,G,B)-min(R,G,B)<0.20）を適用し、濃色柄・深い影を除く。ROIは専門の第三者によるセグメンテーションではなく、一枚・一人の目視選定であり、白布も測色済み中立面ではない。入力、構築参照、マスクは全候補で同一とし、source recordにハッシュと取得根拠を残す。

基準は未変更入力、候補はReinhard/MeanStddev/Histogramの各強度0.25/0.5/1.0。全画面と4 ROIについて参照とのLab D65/2° ΔE76、encoded-sRGB MAE/p95、クリップ率、有限値、肌・布のLab平均/C*ab/色相、輝度勾配を記録する。参照は既知合成変換なので、これへの一致はこの合成課題上の数値誤差であり、自然なWB補正の正解や肌/布品質の証明ではない。強度ごとの結果を記述比較し、現時点では恣意的な合否閾値や勝者を設定しない。候補を見る前のROI確認記録を保存する。独立・盲検の人手評価、実撮影WB対、Core出力比較、色/alpha契約、解像度/バックエンド、性能は未検証のまま残す。

## RD10 手動ROI別のマッチ量トレードオフ（出力採点前に固定）

RD9の既存強度1.0出力をベースに、顔+腕の手動肌ROIと白ブラウスROIだけで、元入力とグローバル出力の符号化sRGB線形補間 `src + λ*(global_match-src)` を行う。肌λ・布λは各0/0.5/1、3方式それぞれで全27組合せを評価し、λ=1が未変更のグローバル候補、λ=0がそのROIの入力保持、λ=0.5が中間である。これは既存AutoColorMatcherの機能やCore強度と同じ挙動ではなく、ROI保護の理想上限と両ROIの独立効果を見る後処理プローブである。

同一ROI/全画面について、入力と合成参照それぞれへのLab D65/2° ΔE76・encoded-sRGB MAE、C*ab/色相、輝度勾配、クリップを測る。入力からの距離と参照への距離を併記し、色合わせと肌/布保持のトレードオフを示す。画素マスクは明示した固定ROIであり、実装可能な自動セグメンテーションを意味しない。閾値や合格宣言は設けず、方式順位も決めない。

## RD8 追加評価: 肌色相当パッチと白の色成分

RD6 の固定 ColorChecker ROI を再利用し、追加の素材選定や出力再計算なしで解析する。肌色相当パッチは暗部・明部を個別および結合で扱い、D65/2° Lab の平均 L*a*b*、平均ベクトル由来 C*ab、画素ごとの C*ab 平均/p95、角度の円平均と円集中度を記録する。白パッチは同じ統計を記録し、WB1 入力・WB2 カメラ JPEG 参照・候補の差を示す。候補は既存3方式×強度0.25/0.5/1.0、基準は未変更入力。

色相保持の解釈では入力からの角度変化と参照への角度差を併記し、色差は平均 Lab ベクトル間の ΔE76 として扱う。WB2 は物理的正解ではなく、ColorChecker skin-tone patch も人肌ではないため、どの数値にも合否閾値や「肌保持」の合格を割り当てない。白パッチは中立材の代替ではない。R&D 完了前に必要な追加評価は実人肌・実白 ROI の事前レビュー済み素材、盲検目視レビュー、Core/C++経路比較、画像効果ルールに定める色・alpha・細部・性能項目であり、本 RD8 はそれらを代替しない。

## Purpose and comparison arms

Evaluate reference-directed color transfer and skin/neutral-white preservation
for existing `ArtifactCore::AutoColorMatcher`. The lab harness does not call or
mirror the Core implementation. Baseline is unchanged source; planned Core arms
are Reinhard, MeanStddev, and Histogram at intensity 0.25, 0.5, and 1.0. The
synthetic offline mean/std equation is diagnostic only. Existing
ImageCorrectionSuite results remain separate; none are photographic or
perceptual ground truth.

## Materials and fixed conditions

The 512×320 chart from `run_rd0.py` is generated with seed 20261116. It has a
smooth field, fixed skin-like / near-white / saturated patches, texture, and
ROI masks. It is an artificial numeric control, not a skin-appearance target.
The generated float arrays, previews, masks, and hashes are recorded in
`captures/rd0/manifest.json`.

CC0 portrait and forest working copies are available in
`../CinematicLooksLab/assets/`; source, license, and hashes are documented in
that lab's `assets/SOURCES.md`. They are candidate evaluation plates, not
paired references, and have no reviewed ROI masks yet. Do not start natural-
image candidate scoring until licensed pairs and manual ROI annotations are
available.

Synthetic metrics use unclipped linear RGB float32. Preview PNGs clip to
[0,1] and use sRGB encoding. The offline control is not the Core MeanStddev
implementation. ROI neutral-chroma is a simple RGB-channel deviation proxy,
not perceptual color difference. Alpha, ICC, and HDR are not represented by
rd0.

## Measurements, review, and gates

rd0 records per-ROI RGB distribution, luminance shift, RGB source/reference
MAE, chroma proxy, clipping, non-finite count, artifact hashes, and generator
hash. These measurements validate fixture plumbing only. No visual-quality
gates or winner are claimed.

Before the Core sweep, add paired, licensed natural photographs and reviewed
skin/white/background/detail masks. Inspect unchanged-source baseline outputs,
then agree and freeze task-specific gates before viewing candidate results.
Review randomized full-frame and ROI sheets at fixed display conditions. Record
color difference in a declared perceptual space, neutral-white deviation,
skin hue/chroma, local contrast/detail, alpha compositing where applicable,
and CPU time/memory. Missing pairs, ROI annotations, or agreed thresholds keep
the outcome inconclusive.

## Unverified

Core execution/differential, natural-photo pairs, manual ROI review, human
visual review, thresholds, detail retention, alpha/hidden RGB, ICC, HDR,
GPU/backend parity, product preview/export, and performance remain open. No
build, configure, or tests are part of this evaluation step.
The separate `alpha_statistics_counterfactual.py` records transparent-pixel
counts and compares whole-image versus opaque-only RGB statistics for two
inputs whose visible pixels and alpha match but hidden RGB differs. It
characterizes a potential transform-sensitivity mechanism. It does not run or
mirror `AutoColorMatcher`, and it cannot quantify actual output differences.

`run_rd1_source_mirror.py` adds a pixel-aligned cast-recovery diagnostic on one CC0 portrait: the reference is generated by a fixed per-channel cast from the source, and the three inspected equations are transcribed into Python. The script pins the Core source hash and records every image and regional metric. This measures independent mirrors against a known synthetic transform; it is not C++ differential evidence. The face ROI and bright-window ROI are unreviewed candidates; no neutral-white or natural-reference conclusion is permitted from them.

## R1 synthetic cast recovery (2026-11-16)

A same-scene reference was generated from the CC0 portrait with the fixed
encoded-sRGB transform recorded in the rd1 manifest. The independent Python
transcriptions were evaluated at intensity 0.25 / 0.5 / 1.0. Stronger settings
reduced mean RGB error against that constructed reference; Histogram at 1.0
reported whole-frame MAE 0.00184 and the unreviewed face-ROI candidate MAE
0.00213 (baseline values 0.01299 and 0.00892). This is expected for a global
synthetic channel cast and is not evidence of natural-image color quality,
skin protection, or a preferred method. Results, all output hashes, the
transcription script hash, Core-source hash, and candidate ROI hash are stored
in `captures/rd1_source_mirror/manifest.json`. No C++ execution or differential
was performed. The human visual gate, natural paired references, reviewed
masks, neutral-white ROI, performance, and product paths remain open.

`white_axis_study.py` adds four controlled linear neutral targets (0.58, 0.70,
0.82, 0.92) and a synthetic warm cast to the rd0 chart. It records neutral-axis
spread, Lab D65 ΔE76 proxy, linear Rec.709 luminance shift, reference RGB error,
and clipping for the independent source mirrors. This isolates a measurable
white-axis response under known conditions; the target is synthetic, the
mirror is not C++ execution, and the measurements do not represent real white
objects or define acceptance limits.

## R2 neutral-axis synthetic response (2026-11-16)

A four-level (0.58 / 0.70 / 0.82 / 0.92 linear) neutral target was added to
an opaque synthetic chart, with the source patches given a known warm cast.
Independent mirrors of all three equations were evaluated at intensities
0.25 / 0.5 / 1.0. At intensity 1.0, mean ΔE76 proxy across the neutral ROI was
5.72 (Reinhard), 9.49 (MeanStddev), and 7.20 (Histogram); corresponding mean
linear Rec.709 luminance shifts were +0.0280, +0.0260, and +0.0217. The RGB MAE
to the known neutral target was 0.0258, 0.0282, and 0.0282. These are
synthetic diagnostic results, not quality gates or a winner: each formula can
move the neutral patch toward this constructed target while altering
luminance, and no human review has judged the tradeoff. Full per-level data,
ROI counts, source hashes, and outputs are in
`experiments/ColorMatchLab/captures/rd2_white_axis/manifest.json`.

The white-axis fixture has no skin material and does not answer whether the
same transform preserves skin. It is opaque, SDR, and equation-mirror only;
real neutral objects, HDR/ICC, Core execution, alpha behavior, performance, and
product paths remain unverified.

Both source-mirror runners now stop if `AutoColorMatch.cpp` no longer matches
the inspected SHA-256. A changed Core source therefore requires re-auditing
the translated equations before the captures can be regenerated; updating the
pin alone is not evidence of parity.

## R2 response-shape follow-up (2026-11-16)

The regenerated rd2 manifest now reports whole-frame, neutral-axis, non-neutral
complement, and four per-level metrics from the same output, with out-of-range
ratios measured before display clipping. Across all four target levels, each
method's mirrored ΔE76 proxy decreased monotonically as intensity rose from
0.25 to 1.0; absolute luminance shift also increased monotonically. That
quantifies a consistent synthetic chroma/luminance tradeoff, not a quality
preference.

At intensity 1.0, neutral-level ΔE76 values from 0.58 to 0.92 were Reinhard
[4.37, 5.29, 6.22, 7.00], MeanStddev [6.24, 7.99, 10.62, 13.12], and Histogram
[5.70, 8.67, 13.81, 0.62]. Histogram is non-monotonic across target levels;
the abrupt high-level change is consistent with a possible 8-bit CDF/binning
sensitivity in the inspected equation, but this synthetic single distribution
does not isolate the cause. It is a follow-up diagnostic question, not proof
of a defect or recommendation. At the same intensity the whole-frame RGB MAE
was lower than the neutral-ROI MAE for all three mirrors, showing that the
aggregate can understate error in the region of interest.


## RD1 inverse-cast paired control (2026-11-16)

The portrait source-mirror runner now evaluates both directions of a known
synthetic transform: original → cast reference and cast reference → original.
The original is exact pixel-aligned ground truth for this constructed reverse
task. The cast is affine in encoded RGB before clipping; 83.38% of pixels were
not clipped and are mathematically invertible, while clipped pixels are
reported separately because exact recovery is impossible there. For intensity
1.0, the independent mirrors' RGB MAE to the original on the invertible area
was 0.009190 (Reinhard), 0.000328 (MeanStddev), and 0.000078 (Histogram).
The source and cast inputs are explicitly kept in the SDR [0,1] range.
On the candidate skin ROI's invertible pixels it was 0.010661, 0.000428, and
0.000076 respectively. These are not natural-image quality results: the
forward cast is global, the equations operate on encoded RGB except Reinhard's
Lab path, the masks are unreviewed, and no compiled C++ differential or visual
gate has been run. The discrepancy is consistent with the distinction between
Lab-space statistics and a channel-wise affine inverse, but does not prove that
explanation. The result motivates a compiled Core differential before drawing
conclusions from the mirror ranking.


## RD1 inserted neutral-card ROI (2026-11-16)

A 128×96 patch with known encoded-sRGB gray 0.82 was inserted into the
portrait outside the candidate face ROI before the same global cast was
applied. It is an artificial card patch over photographic content, not a
natural white object; its rect and synthetic status are in the manifest. At
inverse intensity 1.0, its RGB MAE to the known neutral source was 0.022246 /
0.000139 / 0.000392 for Reinhard / MeanStddev / Histogram, compared with
0.010642 / 0.000430 / 0.000076 on the unreviewed skin ROI's invertible pixels.
The change in method ordering between ROIs shows that a whole-frame recovery
score cannot stand in for local skin/neutral behavior. The inserted patch
contains a hard-edged region and changes global image statistics, so this is a
controlled mechanism study only; it cannot predict how a natural white object
behaves in a real paired-photo task. The manifest auditor checks patch values,
ROI coordinates, output hashes, and coverage, but does not establish Core
parity or image-quality acceptance.


## RD3 Histogram neutral-level boundary sweep (2026-11-16)

To investigate the rd2 high-level response, the highest neutral patch was
swept from linear 0.84 to 0.98 in 141 fixed increments while retaining the
rest of the same synthetic chart. The inspected Histogram equation mirror was
run at intensity 1.0. The largest adjacent change occurred from 0.891 to 0.892:
mean patch ΔE76 fell from 7.5812 to 0.3666. In the semantic blue channel, the
encoded source patch moved from bin 227 to 228; its source CDF moved from
0.918628 to 1.0, and the nearest reference bin moved from 235 to 242. This
indicates that a quantized upper-tail CDF boundary in this particular global
distribution explains the discontinuity in the mirror output. The 8-bit bins
and first-minimum tie behavior are present in the inspected Core source, but
this remains equation-mirror evidence, not compiled C++ differential or proof
of a product defect. The curve and per-step CDF diagnostics are in
`captures/rd3_histogram_sweep/response.csv` and `manifest.json`.

A channel-order audit confirmed that the Python arrays are semantic RGB while
the Core raw-buffer API is documented/tested as BGRA. The equations are
transcribed per semantic RGB channel (and Reinhard explicitly converts RGB to
Lab); mean/std and histogram processing are channel-separable. The stored
RGB-only mirrors therefore preserve those semantic equations but do not model
the raw BGRA memory layout or alpha path. No evidence of channel permutation
causing the reported metric discontinuity was found.


## RD4 patch-area and fixed-histogram control (2026-11-16)

Two controls compared square neutral patches of 32, 48, 64, 90, and 128 pixels
per side at target linear levels 0.891 and 0.892. In the fixed-global-
histogram arm, the same paired source/reference pixel multisets were spatially
rearranged so the contiguous probe ROI changed size while global histograms
remained identical. At each level, output ΔE76 and the chosen reference bin
were invariant across ROI sizes (RD4 audit confirms one histogram signature
and stable ROI response for all five sizes).

In the background-replacement arm, growing the patch also changed the global
distribution. At 0.891, area growth from 0.62% to 10% moved the blue reference
lookup bin 224 → 241 and ROI mean ΔE76 18.825 → 1.147. At 0.892, the source
CDF was 1.0 and the lookup stayed at bin 242, with ΔE76 0.367 at all sizes.
This separates ROI measurement area from global CDF effects: area alone does
not alter the result if the global histograms are fixed; replacing background
pixels changes the global reference CDF and therefore changes the transfer.
All cases remain synthetic equation-mirror controls with spatially rearranged
content or hard-edged patches, not natural-photo evidence or product behavior.
`captures/rd4_histogram_area_control/results.json` stores per-case histogram
hashes and mapping data; `validate_rd4_manifest.py` audits those invariants.


## RD1 visual crop preparation (2026-11-16)

`make_rd1_visual_review.py` produces face-candidate and inserted-neutral-card
crop sheets across source, cast reference, and all three equation mirrors at
intensity 0.5 / 1.0. Hashes for each sheet and all eight source previews are
validated by `validate_rd1_visual_review.py`. An exploratory unblinded visual
inspection of the face crop showed a more yellow/red appearance in the
Reinhard mirror at intensity 1.0 than in the MeanStddev and Histogram mirrors.
At the reduced card crop display size, differences were subtle and not
reliably rankable by sight. This is an informal observation of preview PNGs,
not an independent human review, blinded comparison, quality gate, or Core
output. The ROI remains unreviewed and the source/reference relation remains
synthetic.


## RD5 existing contract-vector mirror check (2026-11-16)

`contract_vector_probe.py` transcribes the fixed BGRA test vectors in
`tests/ArtifactCore/AutoColorMatcherContractTest.cpp` into semantic RGB arrays,
runs the independent equation mirrors, then converts results back for
comparison. MeanStddev matched the existing unequal-size-reference vector
exactly (max RGB error 0 with a 1e-6 tolerance, including values above 1).
Histogram matched the two-level quantile vector within 0.002942, below the
source test's 1/255 tolerance. These checks support consistency of the offline
translations with two existing numeric contracts. They do not run C++, validate
Reinhard, compare output buffers, exercise alpha in Core, or establish image
quality. Source hashes and probe results are in
`captures/rd5_contract_vector_probe/result.json`.


## RD6 registered ColorChecker WB pair (2026-11-16)

A small licensed Middlebury ColorChecker24 subset was added with source-page
provenance and hashes in `assets/MiddleburyColorChecker24/SOURCES.md`. The
selected Canon EOS 20D pair is WB1 fixed-tungsten vs WB2 auto, under daylight
illumination i2 and exposure e3 (nominal 0 EV), both 606×404 registered chart
images. WB2 is the camera auto-WB rendition, not calibrated ground truth. The
dataset page permits use and publication with citation; it is not marked CC0.
The two RAW-rendered PNGs are retained as transfer-space controls but were not
used for JPEG-pair scoring.

A fixed 6×4 grid uses the central 60×60 pixels of every 101×101 patch. Rows 0,
columns 0–1 are standardized skin-tone proxy patches; row 3, column 0 is white;
row 3, columns 1–5 are neutral gray. At intensity 1.0, mean Lab D65 ΔE76 to
WB2 on the skin-tone proxy was 16.434 for unchanged WB1, then 6.877 / 2.999 /
1.667 for Reinhard / MeanStddev / Histogram. White-patch ΔE76 was 17.362, then
3.017 / 3.097 / 1.261. Neutral-gray-group ΔE76 was 12.980, then 4.485 / 2.409
/ 2.240. No candidate clipping or non-finite output was measured. These
figures describe closeness to this camera's WB2 rendition, not physical color
accuracy, skin preservation in portraits, or a method winner. Full per-patch
results, masks, outputs, and a comparison sheet are in
`captures/rd6_colorchecker_pair/`; `validate_rd6_manifest.py` checks coverage,
provenance hashes, finite outputs, and artifact hashes. No Core execution or
visual acceptance has been performed.


## RD7 registered natural-scene RAW/JPEG pair (2026-11-16)

Added the Middlebury Canon EOS 5D `scaled-im00` warm-lit kitchen pair. The
source is the site's standard-linear-RGB RAW rendering converted to sRGB; the
registered camera JPEG is the reference. The camera rendition is not
colorimetric ground truth and may include camera tone/color processing. A
light-wall box and refrigerator-detail box are unreviewed candidate ROIs. At
intensity 1.0, full-frame mean ΔE76 to the JPEG was 14.15 baseline, 4.61
(Reinhard), 4.44 (MeanStddev), and 3.12 (Histogram). The wall candidate was
19.20 → 3.48 / 4.06 / 1.46. In the refrigerator-detail ROI, mean luminance
gradient magnitude was 0.02475 in the sRGB-encoded source, 0.05560 in the JPEG
reference, and 0.06507 / 0.06339 / 0.05467 for the three intensity-1 outputs.
These statistics characterize this camera-rendering pair and the chosen boxes;
they do not establish edge-quality acceptance, a natural white reference, or
method superiority. The pair has no person/skin ROI. Provenance, 10 condition
results, masks, outputs, and 25 artifact hashes are in
`captures/rd7_natural_scene_pair/`; `validate_rd7_manifest.py` checks them.


## RD9 natural portrait-content cast diagnostic

The selected CC0 Pexels/Wikimedia file includes visible human skin and a light
patterned blouse. It is a single input; the reference is a fixed synthetic
RGB cast, not a captured WB pair. The manually reviewed face, arm and blouse
ROIs are stored with an overlay and hashes. The blouse mask uses the fixed
input-space color threshold written above and excludes all skin-mask pixels.
The 10-condition source-mirror run measures whole frame, each skin region and
the blouse region. At intensity 1.0, Histogram mean per-pixel ΔE76 to the
synthetic reference is 2.944 → 0.007 on combined skin and 5.521 → 0.057 on
blouse. Skin mean-vector C*ab changes 14.23 → 11.77 and face hue 42.33° →
48.50°. MeanStddev intensity 1.0 exceeds 1.0 in 0.732% of full-frame RGB
channel samples (1.481% of pixels) and 6.281% / 11.346% in the blouse ROI,
with maximum 1.059. These numeric results and unblinded visual preview inspection are
exploratory; neither establishes product quality or independent human review.
The full-frame comparison and face/arm/blouse crops are stored in the RD9
scored capture. No quality gate or method winner was assigned.


## RD10 fixed-mask regional blend tradeoff

The intensity-1 output from each source mirror was blended back toward the
input independently in the reviewed skin and blouse masks with factors 0, 0.5
and 1. This is an oracle-mask post-effect diagnostic rather than a proposed
AutoColorMatcher implementation. The 27 combinations record both source
preservation and synthetic-reference error. For Histogram, skin factor 0.5
versus 1 changes source ΔE76 1.480 → 2.946 and target ΔE76 1.465 → 0.007;
C*ab is 12.98 vs 11.77 (input 14.23). Blouse factor 0.5 versus 1 changes
source/target ΔE76 from 2.782/2.740 to 5.502/0.057. The error tradeoff is
ROI-dependent and visible in the stored measurements; no factor is endorsed.
All region masks are disjoint. The validator checks pinned inputs, complete
27-way coverage, unchanged/global endpoints, finite metrics and all RD9 file
hashes. Real capture pairs, independent mask review, blinded assessment,
Core output and performance gates remain unverified.


## RD11 cast-direction sensitivity on the same portrait

Reuse only the RD9 source and fixed masks, with three constructed target casts
chosen before scoring: cool (RD9 scale/offset `[0.94,1.015,1.075]` /
`[0.012,0.006,-0.008]`), warm (`[1.06,1.0,0.94]` /
`[-0.008,0.006,0.012]`), and green (`[0.98,1.05,0.98]` /
`[0.006,-0.006,0.006]`). Each target is clipped to [0,1] in encoded sRGB.
Measure unchanged source plus each of three source-mirror equations at strength
0.5 and 1.0, on the full frame and fixed face/arm/skin/blouse ROIs. Record
reference error, source-relative skin C*ab/hue shift, blouse chroma/hue,
clipping and non-finite values. This directional sweep tests sensitivity of
the observed RD9 tradeoff to cast direction, not real illuminants, colorimetric
accuracy, or a quality gate. No method ranking or pass threshold will be
assigned.

RD11 ran the predeclared 21 conditions. The source ROI mean-vector C*ab is
14.23 for skin and 7.70 for blouse. Constructed targets drive those values to
11.77 / 5.41 (cool), 15.87 / 12.65 (warm), and 13.69 / 12.08 (green). At
intensity 1.0, Histogram gives skin C*ab 11.77 / 15.92 / 13.77 and blouse
C*ab 5.43 / 12.65 / 12.08 for cool / warm / green. Combined-skin hue moves
from 44.06° in source toward target 49.95° / 46.47° / 49.48°; Histogram
outputs are 50.49° / 46.40° / 49.87°. This direction sweep confirms the
sign and magnitude of chroma change depend on the target cast; it does not say
which target is perceptually preferable. The target metrics, outputs, and
42 measurement artifacts plus four fixed comparison sheets are in
`captures/rd11_cast_direction_sensitivity/` and pass
`validate_rd11_manifest.py`. Unblinded display-sheet inspection shows a clear
cast-direction change in the blouse ROI and modest differences in the face
crop at montage scale; this is exploratory visual context only, not independent
human review or an acceptance gate.


## RD12 synthetic cast-strength sensitivity (predeclared)

Use the same CC0 portrait, pinned ROI masks, equation mirrors, D65 Lab metrics,
and encoded-sRGB transform family as RD9/RD11. Keep each RD11 cast direction
and scale vector fixed, but multiply its offset vector by 0.5 and 1.5, giving
three strengths for each direction. This yields nine constructed targets plus
unchanged-source and three mirror methods at 0.5/1.0 for every target (63
conditions). Each cast formula is `clip(source * scale + offset * strength,
0, 1)` in encoded sRGB. The predeclared outputs are whole-frame, separate face
and arm, combined skin, and light-blouse ROI Lab ΔE76 to source/target, RGB
MAE, C*ab, circular hue, luminance gradient, clipping, finite count and
percentiles. The target is synthetic; no strength is treated as a physical
illuminant or expected product correction. Do not tune ROI thresholds,
choose a preferred method, or assign a quality gate after observing results.

RD12 completed all 63 conditions. For Histogram at intensity 1.0, combined-skin
target C*ab is 10.85 / 11.77 / 12.74 for cool offset multipliers 0.5 / 1.0 /
1.5, 16.97 / 15.87 / 14.78 for warm, and 13.63 / 13.69 / 13.84 for green.
Corresponding light-blouse target C*ab is 4.90 / 5.41 / 6.02, 13.28 / 12.65 /
12.03, and 12.93 / 12.08 / 11.24. At intensity 1.0, output ROI means remain
within ΔE76 0.323–0.414 of each constructed target for combined skin and
0.271–0.354 for blouse, while skin source-distance ranges 1.673–3.458 and
blouse source-distance 4.655–6.603. These are expected sensitivities of
reference-directed transfer to the chosen synthetic target, not evidence about
physical illumination or perceptual preference. Four fixed full-frame and ROI
comparison sheets were generated and inspected unblinded; the target-strength
changes are visible in both face and blouse crops, while this montage review
is exploratory and not independent or a gate. The 126 image/array artifacts,
four sheets, target metrics, conditions and pins are checked by
`validate_rd12_manifest.py`.

## RD13 transparent hidden-RGB output sensitivity (predeclared)

Use the same pinned RD9 CC0 portrait and synthetic reference. Make two RGBA
source variants with identical alpha, visible RGB and target, but replace RGB
only where alpha equals zero: variant A uses black hidden RGB, variant B uses
magenta hidden RGB. The alpha mask is a fixed 20%-width full-height side strip
chosen for a controlled mechanism probe, not a real cutout or segmentation
example. Run the three inspected equation mirrors at intensity 0.5 and 1.0 on
both variants. Before scoring, record the six-pair method/intensity grid,
exact hidden fills, RGBA hashes, and source/Core equation pins. Measure output
RGB differences only on alpha>0 pixels, alpha preservation, non-finite/range
statistics, and Lab ΔE76 between paired visible outputs; render fixed black,
mid-gray and white composites for exploratory visual comparison. Because current
Core source derives global statistics from RGB without reading alpha and leaves
alpha unchanged, this test isolates downstream visible influence of hidden RGB.
The Python mirrors are not C++ execution; no quality threshold or production
behavior decision is assigned.

RD13 will also establish whether the earlier mechanism-only counterfactual
produces a practically visible difference in the chosen synthetic portrait
fixture. The 20% mask is deliberately prominent to measure sensitivity, not an
estimate of how frequently real projects use such transparency.

RD13 completed the six method/intensity pairs. The two input RGBA arrays have
exactly identical alpha and alpha>0 RGB; 217,856 of 1,089,280 pixels (20%) are
fully transparent and differ only by hidden black vs magenta RGB. In the
source-equation mirrors at intensity 1.0, changing only those hidden values
changes visible combined-skin mean ΔE76 by 17.73 / 25.34 / 26.06 and blouse
mean ΔE76 by 8.28 / 37.61 / 54.33 for Reinhard / MeanStddev / Histogram.
For Histogram, skin mean ΔE76 to the same synthetic reference is 7.45 with
black hidden RGB and 24.60 with magenta hidden RGB; corresponding source
distances are 9.35 and 26.52. The exploratory composites visibly shift the
opaque scene color. This demonstrates substantial sensitivity in the chosen
20%-transparent fixture, not a real-project frequency or compiled Core result.
All six pairings, visible/source/target metrics, six comparison sheets and 36
hashed artifacts pass `validate_rd13_manifest.py`.

## RD14 transparent-area dose response (predeclared)

Reuse the pinned RD9 portrait/reference and RD13 hidden-RGB pair construction.
Set the fully transparent strip width to 0%, 1%, 5%, 10%, and 20% of frame
width, always in the same left-side position; keep all other alpha=1 and alter
RGB only below alpha=0 (black vs magenta). Apply each equation mirror once at
intensity 1.0 to each pair, for 15 pairwise method/fraction conditions. The
0% condition is an identical-input/identical-output control. Record the same
visible full-frame and skin/blouse ΔE76/RGB pair differences and each variant's
source/target error, chroma/hue and clipping; create fixed black-composite
comparison sheets. This quantifies the dose response in one deliberately
controlled spatial pattern, not real-asset prevalence. No threshold, likely
usage rate, or recommended alpha policy will be inferred.

RD14 completed all 15 predeclared method/fraction pairs. At 0% transparency,
black and magenta controls produce identical output for all three mirrors. At
1%, paired visible-output mean ΔE76 is 1.59–2.79 full-frame, 1.69–6.21 on
combined skin, and 1.48–4.96 on blouse. At 10%, those ROI differences rise to
13.45–15.91 on skin and 5.29–36.19 on blouse; at 20% they are 17.73–26.06 and
8.28–54.33. The five-row black-composite sheets show subtle differences at
1–5% and clear green/cast shifts for magenta hidden RGB at 10–20% in the
Histogram mirror. This is a monotonic area-dose trend in this single spatial
fixture, but method/ROI response is nonlinear and does not predict real asset
frequency. All 15 pairs, zero-area identity controls, 68 hashed artifacts and
input pins pass `validate_rd14_manifest.py`.

## External paired-portrait data access audit

The 2011 [`Color Constancy with Faces` report](https://htchen.github.io/awb/awb_single.pdf) describes 472 pairs of people
photographed with/without neutral reference cards and manually marked face
regions, which is a strong scientific fit for skin and neutral-card analysis.
The accessible project page links the paper but does not expose a dataset
download or usage license; no images were obtained. The official [PPR10K repository](https://github.com/csjliang/PPR10K) has full-resolution
human-region masks and expert edits but its repository restricts images and
derived data to non-commercial research, so it is excluded from product R&D.
The [MIT-Adobe FiveK page](https://data.csail.mit.edu/graphics/fivek/) links useful paired expert edits and scene metadata, but the
official image license is research-only and disallows use directed toward
commercial advantage; it is excluded for the same reason. The project must
use only explicit commercially compatible assets or user-supplied consented
captures for this product-facing research. These findings leave a real
captured portrait/gray-card pair as an open data gap; the current RD9/11 CC0
photo plus synthetic casts cannot close it.

The official [York/Adobe Rendered WB page](https://yorkucvil.github.io/projects/public_html/sRGB_WB_correction/dataset.html)
describes 62,535 DSLR input/target pairs plus additional camera sets and offers
chart-masked versions. The upstream [repository terms](https://github.com/mahmoudnafifi/WB_sRGB)
say the dataset is research-only and commercial use requires a separate
license, so it is not acquired for this product-facing study. The 2025
[PolyU face-centric paper](https://library.imaging.org/admin/apis/public/api/ist/website/downloadArticle/cic/33/1/15)
describes 1,299 portrait images and dual illuminants, but no public dataset
download or data-use terms were found in the publisher record; no data was
obtained. [Middlebury's natural scene set](https://vision.middlebury.edu/color/data/)
permits use and publication with citation but describes natural scenes rather
than a portrait/skin benchmark. None of these sources closes the product-
compatible real portrait-pair gap.

## York/Adobe Rendered WB scope and rights audit (2026-11-16)

The current [official dataset page](https://yorkucvil.github.io/projects/public_html/sRGB_WB_correction/dataset.html)
describes 62,535 DSLR rendered inputs paired with chart-derived target images,
plus 2,881 cross-camera test pairs and 10,242 rendered Cube+ pairs. It also
states that the set is provided for purposes other than evaluating post-capture
white-balance correction, and that the images are available for reasonable
academic fair use. The page identifies manually selected chart grays as the
target-white basis and supplies chart masks/coordinates, making this a strong
technical match for a WB benchmark but not a rights-cleared product-R&D asset
under the stated scope. This controls the earlier characterization of it only
as research-only: even a research license would not resolve the page's stated
purpose restriction. No files were downloaded or evaluated; do not use the
pairs, chart masks, derived crops, outputs or metrics for this R&D without
written permission that explicitly covers the intended evaluation and retained
derivatives. The exact image-level licenses and downstream derivative terms
remain unverified.

The remaining permitted work with current lab assets is exploratory synthetic
cast response and already-recorded chart/natural-scene controls. RD7/RD9 Core
checks establish image-overload/raw-buffer equivalence only. These lanes cannot
close paired portrait, measured skin, neutral-white, independent review, visual
acceptance, production route or performance gates. The source audit therefore narrows the available-data
route but does not establish a method ranking or quality acceptance.

## RD19 Core/Equation-mirror fixed-vector and full-frame differential (2026-11-17)

The authorized Debug contract target includes a deterministic 3×2
BGRA float fixture. For all three methods and strengths 0.25/0.5/1.0, it checks
that `ImageF32x4_RGBA` output equals the raw-buffer output exactly, alpha is
unchanged, and results are identical when the descriptor tags are sRGB-encoded
straight BGRA32F versus linear straight BGRA32F. At strength 1.0, all 18 RGB
samples per method are also compared against expected values produced by the
independent `apply_reinhard`, `apply_mean_std`, and `apply_histogram` functions
in `run_rd1_source_mirror.py`. Tolerances are 2e-4, 2e-6, and 1/255
respectively. All nine GoogleTest cases pass. The tolerance checks prove only
this small in-memory numeric fixture; they are not image-quality thresholds.

The descriptor cross-check confirms current behavior that the image overload
does not interpret transfer metadata. It does not justify that behavior as a
color-management contract. The fixture has six synthetic pixels, no annotated
skin/white ROI, no alpha-composited preview, and no full-resolution capture.
The target now has nine passing GoogleTest cases (CTest 1/1) in Visual Studio 18
Debug. Seventh case: the 606×404 RD6 ColorChecker image overload agrees with the
float32-faithful Python mirror previews for three methods × three strengths
within 1.5/255 after decode. Eighth: the 1000×667 RD7 natural-scene image
overload agrees with its raw-buffer Core overload within 1e-6 for the same nine
conditions. Ninth: the 1280×851 RD9 CC0 portrait with constructed cast reference
has the same image/raw contract check. RD7/RD9 are route-equivalence checks, not
Python differential, natural WB, or visual quality evidence. Test and Core source
hashes are pinned in the milestone record.

## Consent-based in-house capture fallback (predeclared; no capture performed)

If an externally licensed portrait pair is unavailable, acquire a controlled
pilot only after the participants' written consent explicitly covers product
R&D, retention of raw/source/reference images, ROI masks, crops, metrics and
rendered derivatives, internal reviewer access, storage location, retention
period and any intended sharing. Keep consent records separate from image
identifiers and restrict access to the authorized reviewers. Do not infer
permission to retain derivatives from permission to photograph.

For each capture condition, use one fixed camera/lens and tripod, fixed focus,
aperture, shutter, ISO and lighting, and one RAW exposure containing a
consenting subject, a certified neutral target at the face plane, and a
measured matte neutral-white material. Record camera, lens, RAW metadata,
illuminant measurement, target identity/certificate, exposure and capture
hashes. From that same RAW, render aligned source/reference images with the
same demosaic, camera profile, exposure, tone curve and output encoding; vary
only the predeclared source white-balance multiplier versus the target
multiplier derived from the neutral target. Preserve high-bit-depth files and
the exact renderer/version/settings. The target is a controlled WB rendition,
not a claim of perceptual skin-color ground truth. Do not include the
calibration chart in the evaluated frame unless its effect on global statistics
is explicitly a separate condition.

Before candidate outputs are opened, a reviewer who did not make the matcher
comparison approves source-only disjoint masks for skin, measured neutral card,
measured white material, hair/eyes/lips, background and edge bands. Record ROI
coordinates, mask pixel counts, reviewer, source hash, rejected regions and
colorimeter-to-pixel location uncertainty. Include several consenting
subjects spanning measured skin-color range, more than one illuminant and
camera/profile condition for the adoption gate; set sample count from desired
uncertainty after a pilot rather than inventing a universal minimum. Capture a
separate mixed-illumination condition only as a stress test, since a single
global transform has no single local WB truth there.

Freeze capture pairs, masks, metrics, visual-review protocol and region-specific
acceptance tolerances after source-only baseline review and before randomized
candidate review. A failed neutral-target check, clipping, pose/registration
change, light drift, consent gap, or unsupported color-profile conversion
invalidates the pair for the affected gate. No in-house capture, participant
contact, or candidate scoring has been performed as part of this plan.

## RD27 results — actual-Core natural-scene lamp ROI diagnostic (2026-11-17)

The predeclared RD27 run used the registered Middlebury EOS 5D warm-lit kitchen RAW/JPEG pair and the unchanged RD18 source-reviewed lamp masks (1,222 and 1,141 pixels). The dataset provider permits use and publication with citation; this permission is not CC0. The latest focused Visual Studio Debug CTest rerun passed 1/1 target (80.19 seconds); RD27 provenance was refreshed to this log. The Core test recorded 27 region rows (three methods × three strengths × whole frame/left/right ROI), saved the source/reference and three strength-1 Core PNGs, and checked 9 full-frame float32 comparisons against the RD7 equation-mirror arrays (2,001,000 RGB channel values each). Histogram matched the mirror exactly; maximum absolute difference was 2.23e-6 for Reinhard and 1.20e-7 for MeanStddev. Thus the mirror agrees closely with this current Core implementation on this specific pair and arithmetic path.

At strength 1, mean reference ΔE76 for left/right lamp shades was 4.81/5.18 for Reinhard, 5.50/5.50 for MeanStddev, and 1.62/1.31 for Histogram. Mean source-relative ΔE76 there was 25.94/26.69, 25.66/26.26, and 23.12/23.27 respectively. The fixed comparison sheet was inspected once, unblinded: all outputs visibly changed the shade appearance toward the camera JPEG; Histogram appeared closest in these two crops. This does not establish which appearance is correct. The camera JPEG is processed and not a measured WB target, and the shades are not colorimeter-confirmed neutral. No ranking, threshold, acceptance or product conclusion is drawn. Full metrics, Core/mirror float differences, conditions, source/mask/test hashes, PNG hashes and review limitations are recorded in `captures/rd27_core_natural_scene/manifest.json`; rerun the capture validation with `python experiments/ColorMatchLab/record_rd27_core_natural_scene.py` after the existing target test.


Histogram CDF precision follow-up: the first full-image test against the prior
RD6 preview failed with maximum RGB differences 0.012745 / 0.025490 / 0.047059
at intensities 0.25 / 0.5 / 1.0. Source inspection traced this to Python float64
CDF accumulation versus Core float32 CDF accumulation and nearest-bin selection.
`run_rd1_source_mirror.py` now follows float32 binning/CDF/blend arithmetic. After
regenerating RD6 and RD8, the full-image Core test passes all nine method/intensity
conditions within 1.5/255 against 8-bit previews. `validate_rd6_manifest.py` and
`validate_rd8_manifest.py` both pass. The RD19 precision record preserves the
previously measured maxima and current corrected outputs; original mirror output
binaries were untracked and unrecoverable, so aggregate mean/p99 comparisons are
not claimed. This is a mirror-fidelity correction, not evidence of a Core bug.
The RD7 and RD9 image-overload checks compare each Core image overload with its own raw-buffer Core path, not with the Python equation mirrors or a visual reference. User-supplied rights-cleared paired portrait/white samples, reviewed natural ROIs,
blind visual acceptance, performance, and production-path validation remain open.


## RD22 実Core強度・高分位差分 sweep（2026-11-17）

RD20と同じCC0 portrait、固定合成cast reference、事前レビュー済み5 ROIを使い、実Core `ImageF32x4_RGBA` overloadの強度を0.0〜1.0の0.1刻みで測定する。3方式×11強度×5領域=165行。固定する指標はD65/2° Labのsource/referenceへの画素平均・nearest-rank p95・最大ΔE76、ΔE76>2の画素率（記述用cutoffのみ）、mean-vector C*ab、pixel-mean C*ab、円環平均色相。強度0はsource baselineとして同一実行で取得する。閾値は置かず、品質合否や方式順位を作らず、変化曲線の記述に限定する。

combined-skin ROIのsource mean-vector C*abは14.23、合成referenceは11.77。強度1ではMeanStddev / Histogram / Reinhardのoutput C*abは11.75 / 11.78 / 12.71。対応するsourceへの平均ΔE76は2.94 / 3.06 / 1.76、合成referenceへの平均ΔE76は0.19 / 0.36 / 2.05だった。0〜1の各点でsourceからの色差は増え、合成targetへの色差は下がる。これは作った変換を同じ画像に戻す構成でのCore挙動で、実WB補正の望ましさ、許容される肌色変化、第三者・別画像への一般化を示さない。出力強度を選ぶgateも設定しない。combined-skinの強度1ではsource-relative p95/max/ΔE>2画素率がMeanStddev 6.19/8.25/72.45%、Histogram 6.53/8.88/84.47%、Reinhard 1.96/2.64/3.67%。平均だけでは全分布の差が隠れるため高分位も保存した。2 ΔE cutoffは合否基準でない。165行・capture log・metric-only曲線図・入力mask/reference・Core/test source hashを `captures/rd20_core_skin_white/manifest.json` と `metrics.json` に固定した（RD capture id: rd22）。再検証は `python experiments/ColorMatchLab/validate_rd22_core_sweep.py`。


### RD22 visual capture and unblinded review

The contract case saves the source, constructed reference and actual Core full-frame PNG outputs at strengths 0.0, 0.5 and 1.0 using the existing `ImageF32x4_RGBA::save()` image-I/O path. A fixed-coordinate comparison sheet shows source, target and all three strength-1 methods at full-frame, face and arm crops. One unblinded R&D reviewer inspected the sheet: method differences at this view scale are subtle but visible in face/arm tone, and no obvious structural loss was seen. This is not a blind or independent review, establishes no acceptance tolerance, and cannot judge neutral-white preservation because the patterned blouse ROI is not measured white. PNG hashes and the review limitation are recorded in `captures/rd20_core_skin_white/manifest.json`; image files are under `captures/rd22_core_outputs/`. This did not connect the Core API to a product route.


## RD23 追加CC0ポートレート候補（2026-11-17、未採点）

`experiments/CinematicLooksLab/assets/portrait-2024-1536.jpg`（CC0 1.0、1536×1024）に、RD20と同じ固定の合成castを適用したsource/referenceと顔ROI候補を `captures/rd23_shadowed_portrait/` に作成した。最初のoverlayでは目・口の除外がずれていたため、拡大source cropで座標を見直し、除外楕円を再配置した。更新後は目と口がoverview上でROI外、顔外周は概ね顔内に収まることを単独目視で確認した。許可済みCore Debugテストは、実 `ImageF32x4_RGBA` overloadで3方式×強度0/0.5/1の9条件を測定し、face ROI 25,913 pixelについてsource相対ΔE76 mean/p95/max/ΔE>2率と、合成reference相対meanをCSV化する。強度1のsource相対 mean/p95/max/ΔE>2率は、Reinhard 4.65/6.40/7.43/94.17%、MeanStddev 3.54/7.05/7.80/74.50%、Histogram 3.51/6.80/8.05/71.05%。合成reference相対meanは順に5.83/0.15/0.33。これは一枚のCC0写真に合成castを当てた結果で、指標cutoffは記述用、方式順位・許容肌色変化・実WBの正解性を示さない。固定full-frame/face crop sheetの単独目視では顔の方式差は見えるが、盲検/独立の受容判定ではない。

同じDebug case内でCore raw-buffer `match()`のみを2解像度で計時した。1 warm-up + 7 samplesのmedian/min–max msは1280×851でReinhard 43.93 (43.04–44.68)、MeanStddev 14.28 (14.04–14.83)、Histogram 217.06 (212.99–227.45)、1536×1024で58.08 (56.66–62.70)、21.53 (21.24–22.76)、326.27 (313.48–335.32)。24 hardware threads表示の単一Debug runで、source restore・I/O・metric処理を外した値である。製品予算に対する判定ではなく、メモリ/allocationsも測っていない。全数値とsource/test/log/output hashesは `captures/rd23_shadowed_portrait/manifest.json`、`core_outputs/face_metrics.csv`、`core_outputs/performance.csv` に記録し、`python experiments/ColorMatchLab/validate_rd23_core_capture.py` で再検証できる。

この素材は一枚の権利確認済み写真であり、実際の照明違いWBペア、測色済み白、撮影同意記録ではない。合成referenceは物理的正解ではなく、今回のmask reviewも単独・非盲検である。追加の探索的Core stress測定には利用可能な候補だが、実写色合わせの正解性を評価できない。ユーザー提供の権利確認済みsource/referenceペアを得るまで製品品質・肌保持・白保持の受入証拠には数えない。

## 公開ペアデータ候補の権利・適合性確認（2026-11-17）

| 候補 | タスク適合性 | 現時点の扱い |
|---|---|---|
| [Color Constancy Using Faces](https://htchen.github.io/awb/awb_single.pdf) | 472人の顔について、WhiBalカードあり/なしの同一照明写真ペア、顔/眼/カードの手動ROI、RAW由来sRGBが論文に記載され、肌＋中立参照に最も近い | 論文は研究データの構成を説明するが、取得データの利用ライセンスを確認できていない。入手・利用しない |
| [York/Adobe Rendered WB](https://yorkucvil.github.io/projects/public_html/sRGB_WB_correction/dataset.html) | 7 DSLR等のWB違い入力と、チャート中間灰色から得たtarget画像・パッチ座標を提供。ポストキャプチャWBには直接適合 | ページは「academic fair use」と表示し、データをpost-capture WB評価以外の目的向けと記述する箇所もある。製品R&Dの利用権が明瞭でないため取得・使用しない |
| [Samsung S24 Raw-sRGB](https://github.com/SamsungLabs/time-aware-awb/tree/main/s24-raw-srgb-dataset) | 3,224 RAW、複数sRGB rendition、neutral/user-preference WB ground truth、混合光マスクを記載 | 公式README上で利用ライセンスを確認できない。人物同意・商用利用条件も未確認のため取得・使用しない |
| [Middlebury Color](https://vision.middlebury.edu/color/data/) | 色票の照明/WB違い、RAW/JPEGペア、自然シーンRAW/JPGペアがあり、色・白軸の技術確認に使える | 肌を含む同一場面の顔評価ではなく、このページで製品R&D用途のライセンスも確認できない。商用品質の採用素材にはしない |

上記は公式説明ページでペア構成・制約を確認した候補調査であり、データ本体の取得・処理はしていない。利用許諾が明示された画像ペアが届くまでは、既存の合成cast結果を実写受入に転用しない。


RD26 actual Core execution passed as part of the same 12/12 target. The six
outputs were finite and preserved alpha exactly. On the 64x64 premultiplied
linear-BGRA fixture, channels above alpha / negative channels / RGB nonzero at
alpha=0 were: Reinhard 155/0/128 at strength 0.5 and 229/0/128 at 1.0;
MeanStddev 128/64/192 and 136/64/192; Histogram 64/0/64 at both strengths.
Maximum excess over alpha ranged 0.00196–0.01564. These values document numeric
association violations on this fixture; they do not alone establish how any
application caller stores or composites the image. Metrics and hashes are in
`captures/rd26_core_premultiplied/manifest.json`, checked by
`validate_rd26_core_premultiplied.py`.
