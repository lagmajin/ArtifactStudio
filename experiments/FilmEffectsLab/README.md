# Film Effects Lab

**最終更新:** 2026-11-13

This folder contains exploratory studies of image effects associated with film looks, including grain, scratches, dust, halation and related image response. Each study has its own preregistered evaluation plan and records whether it models source equations or captures the running application.

Current damage morphology study:

- Plan: `plans/damage-morphology-resolution-r1.json`
- Runner: `film_damage_morphology_resolution_study.py`
- Results: `captures/damage-morphology-resolution-r1/metrics.json` and diagnostic PNGs
- Notes: `RESEARCH_NOTES.md`

Current Film Damage Gate Weave alpha-edge model:

- Plan: `plans/film-damage-gate-weave-alpha-response-r1.json`
- Runner: `film_damage_gate_weave_alpha_response_study.py`
- Results: `captures/film-damage-gate-weave-alpha-response-r1/metrics.json`, checksum, and 21 previews/contact assets
- Limitation: Fixed OpenCV displacement equation model; it does not reproduce Film Damage RNG or capture runtime output.

Current Film Damage Film Burn ring / alpha-response model:

- Plan: `plans/film-damage-film-burn-alpha-response-r1.json`
- Runner: `film_damage_film_burn_alpha_response_study.py`
- Results: `captures/film-damage-film-burn-alpha-response-r1/metrics.json`, checksums, square/wide contact sheets and 24 previews
- Limitation: Fixed diagnostic ring centers; NumPy equation model only, with no Film Damage RNG replay or app capture.

Current Film Damage shared RNG call-schedule audit:

- Plan: `plans/film-damage-control-rng-coupling-r1.json`
- Runner: `film_damage_control_rng_coupling_study.py`
- Results: `captures/film-damage-control-rng-coupling-r1/metrics.json` and checksums
- Limitation: Static high-level distribution-call counts only; no standard-library engine draws, placements, pixels, or app execution.

Current Film Grunge / Film Damage stack-order model:

- Plan: `plans/film-suite-grunge-damage-stack-order-r1.json`
- Runner: `film_suite_grunge_damage_stack_order_study.py`
- Results: `captures/film-suite-grunge-damage-stack-order-r1/metrics.json`, recursive checksums, full contact sheets, enlarged mark/alpha-edge crops
- Limitation: Fixed diagnostic fields and isolated source equations only; no C++/app output, randomized placement parity, or preferred-order decision.

Current Film Damage Flicker tone / alpha-response model:

- Plan: `plans/film-damage-flicker-tone-alpha-response-r1.json`
- Runner: `film_damage_flicker_tone_alpha_response_study.py`
- Results: `captures/film-damage-flicker-tone-alpha-response-r1/metrics.json`, checksums and straight/premultiplied contact sheets
- Limitation: Fixed normal-variate probes and isolated Float32 equation; no RNG sequence, temporal acceptance or app output.

Current Physical Halation alpha-edge equation model:

- Plan: `plans/physical-halation-alpha-edge-response-r1.json`
- Runner: `physical_halation_alpha_edge_response_study.py`
- Results: `captures/physical-halation-alpha-edge-response-r1/metrics.json`, checksums, contact sheets and detail crops
- Limitation: Python source-equation model only; no C++/GPU parity, alpha-association confirmation on real sources, or app/display evaluation.

Current Physical Halation spread / intensity / HDR response model:

- Plan: `plans/physical-halation-hdr-spread-intensity-response-r1.json`
- Runner: `physical_halation_hdr_spread_intensity_response_study.py`
- Results: `captures/physical-halation-hdr-spread-intensity-response-r1/metrics.json`, checksums, point-source arrays and contact sheets
- Limitation: Source-equation model with synthetic numeric linear-like plates; no runtime, transfer-function, display, or performance validation.

Current Film Damage Flicker analytic gain-probability model:

- Plan: `plans/film-damage-flicker-gain-probability-r1.json`
- Runner: `film_damage_flicker_gain_probability_study.py`
- Results: `captures/film-damage-flicker-gain-probability-r1/metrics.json`, probability matrix, curve and checksums
- Limitation: Standard-normal marginal probabilities only; no platform RNG sequence, temporal correlation, clamp, display or runtime evidence.

Current Film Grunge / Film Damage dust and scratch count quantization model:

- Plan: `plans/film-damage-grunge-amount-quantization-r1.json`
- Runner: `film_damage_grunge_amount_quantization_study.py`
- Results: `captures/film-damage-grunge-amount-quantization-r1/metrics.json`, response charts and checksums
- Limitation: Static count formulas only; no RNG placement, rasterized marks, slider precision confirmation, or visual-strength judgment.

Current Film Grunge / Add Noise grain Size-boundary comparison:

- Plan: `plans/grain-family-size-boundary-response-r1.json`
- Runner: `grain_family_size_boundary_response_study.py`
- Results: `captures/grain-family-size-boundary-response-r1/metrics.json`, 4x residual contact sheet and checksums
- Limitation: 28x28 safe source-equation domain only; no C++/GPU output, high-resolution Grunge hash parity, or perceptual judgment.

Current Match Grain reference-resolution / output-scale model:

- Plan: `plans/match-grain-reference-resolution-scale-r1.json`
- Runner: `match_grain_reference_resolution_scale_study.py`
- Results: `captures/match-grain-reference-resolution-scale-r1/metrics.json`, native-reference and residual contact sheets, raw reference assets and checksums
- Limitation: Synthetic source-equation model only; no runtime named-input, color metadata, GPU, alpha or visual acceptance evidence.

Current Film Grunge grain temporal-field model:

- Plan: `plans/grunge-grain-temporal-field-correlation-r1.json`
- Runner: `grunge_grain_temporal_field_correlation_study.py`
- Results: `captures/grunge-grain-temporal-field-correlation-r1/metrics.json`, checksums and selected-frame contact sheets
- Limitation: Safe 28x28 source-equation domain only; no compiled output, full-resolution parity or perceptual temporal acceptance.

Current temporal source audit:

- Plan: `plans/temporal-seed-key-audit-r1.json`
- Runner: `temporal_seed_key_audit.py`
- Result: `captures/temporal-seed-key-audit-r1/metrics.json`

Current Film Grunge grain scale study:

- Plan: `plans/grunge-grain-spatial-scale-r1.json`
- Runner: `grunge_grain_spatial_scale_study.py`
- Results: `captures/grunge-grain-spatial-scale-r1/metrics.json` and diagnostic plates

Current Film Grunge alpha-association study:

- Plan: `plans/grunge-grain-alpha-association-r1.json`
- Runner: `grunge_grain_alpha_association_study.py`
- Results: `captures/grunge-grain-alpha-association-r1/metrics.json` and background contact sheets

Current cross-effect alpha-response study:

- Plan: `plans/film_suite_alpha_response-r1.json`
- Runner: `film_suite_alpha_response_study.py`
- Results: `captures/film-suite-alpha-response-r1/metrics.json` and contact sheets

Current Add Noise route equation audit (synthetic and offline):

- Plan: `plans/add-noise-route-equation-audit-r1.json`
- Runner: `add_noise_route_equation_audit.py`
- Results: `captures/add-noise-route-equation-audit-r1/metrics.json` and 18 diagnostic PNGs
- Findings: CPU and resident source equations agree in the model; dedicated wrapper seed conversion differs for nonzero seeds. No compiled/runtime parity claim.

Current Add Noise grain-field characterization (offline synthetic model):

- Plan: `plans/add-noise-grain-field-characterization-r1.json`
- Runner: `add_noise_grain_field_characterization_study.py`
- Results: `captures/add-noise-grain-field-characterization-r1/metrics.json`, checksum, and 15 preview/contact assets
- Findings: sizes below 1px both yield per-pixel independent fields; larger sizes increase block correlation. Amount .4 strongly clips shadow and highlight panels. Metrics are source-equation descriptions, not aesthetic acceptance.

Current Add Noise chroma/mode response (offline synthetic model):

- Plan: `plans/add-noise-chroma-response-r1.json`
- Runner: `add_noise_chroma_response_study.py`
- Results: `captures/add-noise-chroma-response-r1/metrics.json`, checksum, and 15 preview/contact assets
- Findings: color mode shifts hue on saturated patches; mono/luma modes apply a shared channel delta. Luma mode does not preserve the existing luminance. No display-color or aesthetic claim.

Current Add Noise alpha compositing model (conditional, synthetic):

- Plan: `plans/add-noise-alpha-compositing-r1.json`
- Runner: `add_noise_alpha_compositing_study.py`
- Results: `captures/add-noise-alpha-compositing-r1/metrics.json` and 20 previews, including a source-path-predicted CPU/GPU route difference
- Follow-up static trace: the eligible Composition GPU route converts the layer to straight RGB before applying resident raster effects, then the blend pass premultiplies by alpha; the CPU rasterizer QImage route can preserve a Premultiplied descriptor. Actual dispatch/output remains unverified.

Current Add Noise Composition ingress / HDR-alpha route model:

- Plan: `plans/add-noise-composition-ingress-hdr-alpha-r1.json`
- Runner: `add_noise_composition_ingress_dynamic_range_study.py`
- Results: `captures/add-noise-composition-ingress-hdr-alpha-r1/metrics.json`, checksums, and black/white CPU-versus-resident contact sheets
- Limitation: Conditional NumPy source-equation model; does not establish which Composition route executes or its compiled/runtime pixels.

Current Match Grain reference-transfer source model:

- Plan: `plans/match-grain-reference-transfer-r1.json`
- Runner: `match_grain_reference_transfer_study.py`
- Results: `captures/match-grain-reference-transfer-r1/metrics.json`, 16-bit plates, contact sheets, and fixed ROI crops

Current Match Grain temporal jitter source model:

- Plan: `plans/match-grain-temporal-jitter-response-r1.json`
- Runner: `match_grain_temporal_jitter_response_study.py`
- Results: `captures/match-grain-temporal-jitter-response-r1/metrics.json`, `checksums.json`, and `match-grain-temporal-jitter-contact.png`
- Method: Fixed synthetic reference residual, 240 integer frames, six `temporalJitter` settings, and two output resolutions; measures hashed offsets, cyclic wrap coverage, and adjacent-frame residual correlation/delta.
- Limitation: Static source-equation model only; no product playback, named-input, compiled output, perceived-flicker or performance conclusion.

Current cross-effect grain temporal-response model:

- Plan: `plans/grain-suite-temporal-response-r1.json`
- Runner: `grain_suite_temporal_response_study.py`
- Results: `captures/grain-suite-temporal-response-r1/metrics.json`, `checksums.json`, and `grain-suite-temporal-contact.png`
- Method: 128 frames on a 28x28 neutral plate; Film Grunge, Add Noise CPU color, and Match Grain are calibrated to the same initial green residual standard deviation, then temporal correlation, delta, channel correlation, and static-seed identity are measured.
- Limitation: Isolated source-equation models only; no runtime, GPU, playback, perceptual temporal acceptance, or recommended control values.

Current Physical Halation frame-edge response model:

- Plan: `plans/physical-halation-frame-edge-response-r1.json`
- Runner: `physical_halation_frame_edge_response_study.py`
- Results: `captures/physical-halation-frame-edge-response-r1/metrics.json`, checksums, and spread-specific contact sheets
- Limitation: Float32 source-equation model with opaque neutral point sources; it characterizes edge-clamped box diffusion and is not C++/app output or a quality verdict.

Current Physical Halation frame-size / Spread scaling model:

- Plan: `plans/physical-halation-frame-size-scaling-r1.json`
- Runner: `physical_halation_frame_size_scaling_study.py`
- Results: `captures/physical-halation-frame-size-scaling-r1/metrics.json`, checksums, and edge/corner energy heatmaps
- Limitation: Opaque neutral point source equation model only; results characterize the radius-to-frame response and boundary clamp, not a visual quality verdict.

Current Physical Halation finite highlight-cluster edge model:

- Plan: `plans/physical-halation-highlight-cluster-edge-response-r1.json`
- Runner: `physical_halation_highlight_cluster_edge_response_study.py`
- Results: `captures/physical-halation-highlight-cluster-edge-response-r1/metrics.json`, checksums, and the fixed-global-curve contact sheet
- Limitation: Synthetic opaque neutral Gaussian spots only; distinguishes finite-cluster response from point-impulse amplification without establishing app parity or visual acceptance.

Prepared Composition runtime capture (user-authorized, offline-only; capture remains pending because the Effects panel did not bind to the active temporary composition):

- Plan: `plans/film-suite-composition-runtime-capture-r1.json`
- Fixture generator: `prepare_film_runtime_capture.py`
- Analyzer for exported PNGs: `analyze_film_runtime_capture.py`
- Inputs/ROIs: `fixtures/film-suite-runtime-r1/`
- Capture manifest template: `captures/film-suite-composition-runtime-capture-r1/capture-manifest.template.json`

Python source-equation models are exploratory. They do not establish compiled effect parity, product image acceptance, temporal behavior, runtime performance, or a matched perceptual strength between controls.

Current Film Grunge / Add Noise matched-strength spectrum model:

- Plan: `plans/grain-family-matched-strength-spectrum-r1.json`
- Runner: `grain_family_matched_strength_spectrum_study.py`
- Results: `captures/grain-family-matched-strength-spectrum-r1/metrics.json`, `checksums.json`, and `matched-grain-strength-spectrum-contact.png`
- Method: 28x28, 128-seed source-equation ensembles; Add Noise mono/color Amount is calibrated per Size to Film Grunge's default green residual standard deviation, then spatial spectrum, lag correlation, channel correlation, and tone clipping are compared.
- Limitation: Equation models only; Core FilmGrain and legacy FilmEffectProcessor are excluded because current production callers were not found. No runtime/app, GPU, perceptual, alpha, temporal, or performance conclusion.
