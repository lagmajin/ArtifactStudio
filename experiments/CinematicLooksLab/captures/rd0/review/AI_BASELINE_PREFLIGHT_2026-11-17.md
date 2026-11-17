# AI baseline preflight — rd0

**最終更新:** 2026-11-17  
**状態:** AIによる未盲検のbaseline記述のみ。人手レビュー、gate、画質判定ではない。

## Scope and method

Only the five `baseline_i0.00` full images and selected baseline ROI crops were viewed. No look candidate image, blind contact sheet, or `blind_review_key.json` was opened. Images were displayed with the local image viewer at fit-to-window; display calibration, ICC handling, and physical display conditions are unknown. This review records visible input characteristics to help a human reviewer separate them from changes introduced by a look.

## Baseline observations

| Case | Observed baseline characteristics | Candidate review should watch for |
|---|---|---|
| Chart | Smooth grayscale and color gradients; clean flat patches, hard step edge, and thin horizontal lines. Black and white endpoints are intentionally at the range limits. | New hue in the neutral ramp, broken/merged fine lines, gradient banding, edge halos, or unintended expansion of endpoint clipping. Compare with the actual same-position baseline ROI. |
| Neon streets | Dense city detail with deep shadows, bright signs/headlights, and saturated cyan/green/red light. The source already has strong local contrast and bright highlight cores. | Loss of shadow separation, broadened light cores/halos, colored edge fringes, or reduced road/building detail beyond the baseline. JPEG source/output limits small-detail judgments. |
| Street lights | Dark blue-purple sky, bright point lamps with visible star/ray glare, and colored decorative lights. Bright lamp cores approach display white in the viewed crop. | Added glare spread, clipped lamp structure, hue changes around bright cores, or amplified dark noise. Existing glare is source content, not an effect defect. |
| Portrait | Strong warm directional illumination across the face, a pronounced shadow over the lower face, red/orange hair light, and a cool bright window background. The face has uneven illumination. | Detail loss, halos at hair/window boundaries, new clipping, or large unintended hue shifts. This single image is not a skin-tone ground truth and must not define acceptable skin color by itself. |
| Forest | Dense fine branches and foliage, a bright central sun/sky region, dark green lower foliage, and high local contrast. | Branch merging, foliage chroma noise, highlight clipping/spread, or loss of dark-foliage separation beyond baseline. JPEG detail is a source/output limitation. |

These are visual observations at the displayed scale, not measurements or defects established by comparison. The source photographs are not ground-truth images for cinematic color.

## Gate implications to discuss in the human baseline review

- Compare each candidate with its same-case baseline under the same display transform; do not use another case as a reference.
- Set technical thresholds from explicit contracts and baseline measurements where possible. For visual defects, record ROI, display scale, severity definition, and reviewer before opening candidate sheets.
- Keep look preference separate from technical quality. Do not infer a winning preset from this preflight.
- Keep HDR, alpha/background compositing, CPU/GPU/runtime parity, and 1080p/4K performance as separate gates; these baseline pictures cannot answer them.

## Evidence identities

### Full baseline images

| File | SHA-256 |
|---|---|
| `images/chart__baseline_i0.00.png` | `d3795a62af95e7ed81cc0c21b03e555cc9c87c2aee9c479c668b199fb5732afd` |
| `images/neon_streets__baseline_i0.00.jpg` | `3189ccebcb6d04f6499ca9e569390e793297ad5710df28f3522a7060ad19a556` |
| `images/street_lights__baseline_i0.00.jpg` | `037f3be44c99553aed51a6d3a53daa35e950b88752c484fb443dcd952d82e725` |
| `images/portrait__baseline_i0.00.jpg` | `3c4146be825b326630bfe5de0fef3609fbfdb5c065185c58e43ecb2d5e675ea6` |
| `images/forest__baseline_i0.00.jpg` | `7e848bba496d7c862bddfbdf3a46d3606096958d490523673a488338a96e41c1` |

### Viewed baseline crops

| File | SHA-256 |
|---|---|
| `images/chart__baseline_i0.00__roi_gray_ramp.png` | `9260a1bd16ab82788546307d78b694bb7501937c73a4cd76d9070a65da0104c4` |
| `images/chart__baseline_i0.00__roi_fine_lines.png` | `dc54602d3e2f164e7a209b36afbf4f89143435d1732a23f06a764da5765de2aa` |
| `images/chart__baseline_i0.00__roi_dark_highlight.png` | `681a0af6e02877a1f1405d52d6ae58ad45241e3e852b3bae73c8c9805855fe13` |
| `images/chart__baseline_i0.00__roi_smooth_gradient.png` | `20c02beee6a55ff920c4a198294a59fc75a165732b4b7c1bf93c50302c879298` |
| `images/street_lights__baseline_i0.00__roi_lamp_and_glare.png` | `a45e1bc543af2bceb3a691725cc9a051e207d77540acc1f1efae0b42fe2740f4` |
| `images/portrait__baseline_i0.00__roi_face_skin.png` | `adb681d304ef39f900b68c1ce1d428b805efe52bd6861090f5a76660e2d03c79` |
| `images/portrait__baseline_i0.00__roi_bright_window.png` | `70534d4595b67e7ed263c2803aeedb60b299106a452f84b0a0e19a2f847f84b3` |
| `images/forest__baseline_i0.00__roi_fine_branches.png` | `b10389dfc3f35519a3079c9c4d605fba23ada00223df05fe86c65d9c159106fa` |
| `images/neon_streets__baseline_i0.00__roi_intersection_lights.png` | `b3990d8c4c6d6f95adadafc688661d184fcbf1b6ae1a97735fab78f3c2efb18a` |

