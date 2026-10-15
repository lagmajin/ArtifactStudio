# Blur and Glow 3×3 Visual Exploration Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Add an opt-in 3×3 candidate explorer for the Blur and Glow effects, let a user apply one candidate, then generate/refine another 3×3 grid centered on the selected values, without changing normal effect editing or creating nine undo entries.

**Architecture:** Keep the existing effect/property/service path authoritative for committed edits. Build candidates from a captured baseline and bounded parameter axes; render each through isolated preview state rather than repeatedly mutating the live effect. Display nine labeled thumbnails, then commit the chosen property set through a single undoable service command. Subsequent refinement re-centers axes on the selected values and commits on Apply only.

**Tech Stack:** C++23 modules, Qt Widgets, existing Artifact effect/property/service/render pipeline, CMake/CTest, GoogleTest; use existing CPU image-effect contract tests for algorithm correctness and add UI/controller contract coverage at the narrowest supported seam.

---

## Findings and implementation constraints

- Blur’s public effect is declared in `Artifact/include/Effects/Blur/BlurEffect.ixx`; its implementation is `Artifact/src/Effects/Blur/BlurEffect.cppm`. Tunable user-facing properties include `Radius` (0.1–2048 px, soft range 0.1–128), `Strength` (0–1), `Iterations` (1–16), `Mode` (Gaussian/Edge Preserving), `Edge Threshold` (0–1), and `Premultiplied Alpha`. Radius is the best first axis; optionally pair Radius with Strength. Preserve the effect’s exact property names and use typed property metadata/ranges.
- Blur has a legacy GPU implementation with `Flush()`/`WaitForIdle()` readback code and explicit comments warning against widening its supported resident-GPU path. GPU routing is restricted to premultiplied Gaussian blur, full strength, sigma < 3, and capacity-limited passes. **Do not implement candidate previews by calling the legacy GPU effect per tile.** Use bounded CPU preview evaluation initially or a validated existing resident render route; keep expensive renders off the GUI thread and cancel stale work.
- Glow’s property contract is in `Artifact/src/Effects/Glow/GlowEffect.cppm`. `baseSigma` is displayed as “Radius” (0.1–128 px); other useful axes are `glowGain`/“Intensity” (0–10), `layerCount` (1–16), `sigmaGrowth` (0.1–4), `baseAlpha`, and `alphaFalloff`. Glow CPU preview computation is OpenCV Gaussian blur over each layer; its legacy GPU path also performs blocking readback. Keep candidates bounded and use CPU preview rendering for initial support.
- `ArtifactEffectService::setEffectProperty` in `Artifact/src/Service/ArtifactEffectService.cppm` validates properties, pushes individual undo commands, marks the layer Effect-dirty, and publishes change events. Calling it nine times while generating candidates would mutate the project and pollute undo history. Add a batch apply service operation that validates every field first, stores all old values, applies atomically/rolls back on error, and records **one** undo command for the full chosen candidate. Reuse existing dirty/event semantics once after successful apply.
- Property UI lives in `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` (declaration `Artifact/include/Widgets/ArtifactPropertyWidget.ixx`). The composition preview/render controller is `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`markRenderDirty()` and render tick). Prefer a dedicated dialog/widget launched from the focused effect property group; do not embed the 3×3 lifecycle in the monolithic composition render controller.
- Existing tests include `tests/Artifact/SpatialImageEffectContractTest.cpp` and CMake registration in `Artifact/CMakeLists.txt`/`tests/Artifact/CMakeLists.txt`; the focused image-effect harness is `tests/ImageEffects/CMakeLists.txt`, but it currently registers a fixed set and no direct Blur/Glow contract test. Standard test setup is `cmake --build <build> --target <test>` and `ctest --test-dir <build> -R <test> --output-on-failure`. `out/build/x64-Debug-Agent` exists but cache has `ARTIFACT_BUILD_TESTS=OFF` and `ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS=OFF`; inspect/configure an isolated test build before relying on it.
- Repository already contains `plans/` with milestone documents; use `plans/` rather than the skill-default `.hermes/plans/` to match project convention. The repository had pre-existing local modifications at inspection time; implementation must preserve them and limit edits to the feature/test files below.

## Product behavior and MVP decisions

1. Open **Explore** from the selected Blur or Glow effect’s property group.
2. Choose one primary parameter (default Radius: Blur `Radius`, Glow `baseSigma`) and optional secondary parameter (default Strength for Blur; Intensity/`glowGain` for Glow). If no secondary parameter is selected, use a safe default orthogonal parameter appropriate to the effect rather than varying unrelated controls.
3. Generate exactly 9 deterministic candidates in a 3×3 grid, centered on current values. Scale the primary and secondary ranges to the property’s hard range; avoid duplicate/clamped values near endpoints. Candidate tiles show parameter values and identify the center/current-value candidate.
4. Selecting a tile only updates selection/highlight; it does not change the live effect. **Apply** commits the selected candidate atomically as one undo step. Cancel/close discards uncommitted state.
5. **Refine** creates a new 3×3 around the currently selected candidate, narrowing each axis range by a documented fixed factor (initial proposal: 0.5), bounded by the effect property ranges. The selected candidate remains the new center. Applying after refinement is still one atomic undo step from the pre-exploration baseline (if Apply happened earlier, the refinement session starts a new baseline).
6. If the layer/effect/composition/frame changes or the user edits the effect externally while rendering, invalidate/cancel stale candidate jobs and prevent applying results to a different effect identity. If the source frame changes, regenerate against the new frame or require explicit refresh; never silently present stale thumbnails.
7. No candidate is automatically accepted. This avoids surprising project mutation and gives predictable undo/dirty behavior.

## Proposed files

### Likely new files
- `Artifact/include/Widgets/EffectVariationExplorer.ixx` — public dialog/widget declaration.
- `Artifact/src/Widgets/EffectVariationExplorer.cppm` — grid, selection, refine/apply/cancel, accessibility and lifecycle.
- `Artifact/include/Service/ArtifactEffectVariationPreviewService.ixx` and `Artifact/src/Service/ArtifactEffectVariationPreviewService.cppm` — candidate validation, isolated render request/result, cancellation/revision tokens; alternatively place a small service API in existing `ArtifactEffectService` if that avoids unnecessary subsystem growth.
- `Artifact/include/Service/ArtifactEffectBatchCommand.ixx` / implementation only if existing undo command types cannot represent one atomic batch; prefer a focused internal command next to the existing SetPropertyCommand implementation.
- `tests/Artifact/EffectVariationExplorerContractTest.cpp` — candidate generation, boundaries, selection/refine and stale-result/atomic-apply contracts.
- `tests/Artifact/BlurEffectContractTest.cpp` and `tests/Artifact/GlowEffectContractTest.cpp` — CPU effect behavior/property contracts if coverage is not added to existing spatial test.

### Likely modifications
- `Artifact/include/Widgets/ArtifactPropertyWidget.ixx`, `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` — expose Explore action only for Blur/Glow effect groups; pass stable layer/effect identity and editor context.
- `Artifact/include/Service/ArtifactEffectService.ixx`, `Artifact/src/Service/ArtifactEffectService.cppm` — atomic batch validation/apply API and undo behavior, or keep service API in a new focused service.
- `Artifact/CMakeLists.txt` — add module sources to the correct source manifest/target ownership and ensure Qt Widgets links are present.
- `tests/Artifact/CMakeLists.txt` — register the new contract target(s), test labels.
- `tests/ImageEffects/CMakeLists.txt` — register standalone Blur/Glow CPU contract tests in the focused harness if they can link without app startup.
- Translation catalogs/resources for labels and accessibility strings, if the corresponding strings are localized in this app.

## Implementation steps

### Task 1: Lock the candidate and property contracts

**Objective:** Add tests/spec helpers defining candidate values, range behavior, refinement, and exact parameter identity before UI work.

**Files:**
- Create or modify: `tests/Artifact/EffectVariationExplorerContractTest.cpp`
- Modify: `Artifact/CMakeLists.txt` and `tests/Artifact/CMakeLists.txt` only as needed for test registration.

**Steps:**
1. Write tests for deterministic 3×3 ordering, center candidate matching baseline, bounds/clamping without duplicates, missing/invalid property rejection, one-axis mode, and refine narrowing around selected values.
2. Verify failures using the project’s existing CMake/Ninja test build after enabling the focused test option in an isolated build directory.
3. Implement a pure candidate-generation helper (no effect mutation, rendering, or Qt widget dependency) and make tests pass.
4. Include Blur Radius/Strength and Glow baseSigma/glowGain metadata/range fixtures matching the declarations above; reject non-numeric or non-editable properties.

### Task 2: Define an isolated preview render contract

**Objective:** Render parameter overrides without modifying the live layer/effect or producing readback stalls on the UI thread.

**Files:**
- Create/modify: focused preview service files described above.
- Test: `tests/Artifact/EffectVariationExplorerContractTest.cpp` and/or a new renderer contract test.

**Steps:**
1. Add a request structure containing immutable source image/frame snapshot, effect identity, candidate index, property override map, preview dimensions/quality, and monotonically increasing session revision.
2. Clone/construct an isolated effect instance using the same effect type and copy current properties, then apply only the candidate override to that detached instance. Do not use `ArtifactEffectService::setEffectProperty` for previews.
3. Render at a bounded thumbnail resolution using CPU path for Blur/Glow initially. Never call the Blur/Glow legacy GPU apply/readback path for nine candidates.
4. Run work on a worker/existing async execution facility; return results to GUI thread. Cancel queued/outstanding work on dialog close or revision change; ignore stale results by identity + revision + candidate index.
5. Add tests proving requests do not mutate source effect values, all nine candidate IDs map correctly, and stale/cancelled results cannot replace current thumbnails.

### Task 3: Add atomic one-undo batch commit

**Objective:** Apply the selected set of candidate values as one validated undoable edit.

**Files:**
- Modify: `Artifact/include/Service/ArtifactEffectService.ixx`, `Artifact/src/Service/ArtifactEffectService.cppm`, or add a focused batch command implementation.
- Test: service/undo contract test under `tests/Artifact/`.

**Steps:**
1. Add an API that takes stable layer ID, effect ID, and property/value map. Resolve all editable properties and validate conversion/ranges before mutation.
2. Implement one undo command storing all old/new values; ensure undo restores the complete prior set and redo reapplies the complete candidate. If any setter fails, roll back all already-applied properties and emit no dirty/event notifications.
3. On success, call `layer->setDirty(LayerDirtyFlag::Effect)` and publish the same layer/project/effect change events as the current single-property path exactly once.
4. Test multi-property apply, undo, redo, invalid-property atomic rejection, setter rollback, and exactly-one history entry/change notification where the existing test seam allows.

### Task 4: Build the 3×3 explorer UI

**Objective:** Present candidate thumbnails and the select/refine/apply/cancel flow.

**Files:**
- Create: `Artifact/include/Widgets/EffectVariationExplorer.ixx`, `Artifact/src/Widgets/EffectVariationExplorer.cppm`.
- Modify: `Artifact/include/Widgets/ArtifactPropertyWidget.ixx`, `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`, `Artifact/CMakeLists.txt`.

**Steps:**
1. Add a 3×3 accessible grid, candidate value labels, loading/error placeholders, selected-state indication, selected parameter/range controls, and buttons for Refine, Apply, Cancel.
2. Initialize from the selected effect’s current editable properties; explicitly support Blur and Glow only. Keep unsupported effects without the action.
3. Trigger preview service requests for each candidate; update tiles only for matching session revision and candidate index. Disable Apply until a candidate is selected and its preview request completed (unless policy explicitly permits apply-before-thumbnail, which is not recommended).
4. Refine from selected values using the narrowing factor and new session revision. Preserve the same effect/layer token and prevent duplicate launches.
5. Apply once via the batch service, close only on success; report validation/target-change errors without silently committing. Cancel/close cancels requests and leaves the effect untouched.
6. Add keyboard navigation, screen-reader names containing parameter values, and translated labels.

### Task 5: Verify effect fidelity and interactive cost

**Objective:** Confirm candidate previews are representative enough and performance remains bounded.

**Files:**
- Create/modify: `tests/Artifact/BlurEffectContractTest.cpp`, `tests/Artifact/GlowEffectContractTest.cpp`, `tests/Artifact/EffectVariationExplorerContractTest.cpp`.
- Modify: CMake test manifests.

**Steps:**
1. Use synthetic RGBA fixtures to test Blur radius spread/strength, input immutability, finite output, dimensions/color descriptor/alpha expectations; test Glow radius and intensity produce distinct outputs and preserve alpha/input.
2. Compare preview-size candidate output to the corresponding full-size CPU effect at the preview scale using a documented tolerance; avoid asserting CPU/GPU bit equality.
3. Add a deterministic request-count/cancellation test (exactly nine requests for grid generation; stale requests ignored). Add a reasonable timing smoke test only if CI timing is stable; otherwise record manual profiling procedure.
4. Validate very wide Blur/Glow candidate limits and cap total CPU work (thumbnail dimensions, max active jobs, cancellation, and no full-resolution 9-way render).

### Task 6: Integrate and run verification

**Objective:** Ensure production target manifests, test registration, and UI lifecycle are correct.

**Files:**
- Final touched files from above; no unrelated cleanup.

**Steps:**
1. Run source manifest validation: `cmake --build out/build/x64-Debug-Agent --target check_source_manifests` (or the repository’s configured equivalent).
2. Configure a dedicated build directory for the image-effect/UI contract tests; do not reconfigure or overwrite the user’s active build. Confirm GTest, Qt, OpenCV, and required modules are available before configuring.
3. Build focused targets, then run `ctest --test-dir <focused-build> -R 'EffectVariationExplorer|BlurEffect|GlowEffect|SpatialImageEffect' --output-on-failure`.
4. Build the affected production target on supported Windows/MSVC configuration. If UI runtime integration tests require GPU/display, run them only where that environment is available and report the constraint explicitly.
5. Manually verify: open Blur explorer, select/apply; undo/redo; refine/apply; cancel leaves values unchanged; repeat for Glow; switch frame/effect during render and ensure stale tiles are ignored.

## Risks and mitigations

- **Nine renders can be expensive:** thumbnail-only CPU previews, bounded worker pool/queue, cancel superseded sessions, and do not eagerly render full-resolution candidates.
- **Blocking GPU readbacks:** Blur and Glow legacy implementations contain `Flush`/`WaitForIdle` readback. Route candidate previews through isolated CPU evaluation unless an already-safe resident GPU route is specifically verified; do not “optimize” by dispatching nine GPU jobs through these paths.
- **Preview/result mismatch:** preserve effect identity, parameter units/ranges, color/alpha descriptor and scale; test visual contracts. Document that thumbnails are interactive previews, not final-resolution proof.
- **Undo pollution/partial mutation:** previews never touch the live effect; commit is one atomic batch command with validate-first and rollback.
- **Stale results / target changes:** session revision + stable layer/effect IDs; cancel on close/context change and reject apply if target identity is no longer valid.
- **Large or unusual ranges:** compute candidates in floating point, clamp to hard ranges, de-duplicate after quantization/step rounding, and surface unavailable candidate slots rather than silently duplicating.
- **Existing working tree changes:** preserve all pre-existing modifications; add only planned feature/test files and review `git status`/diff before completion.

## Open questions to resolve before implementation

1. Should the secondary axis default to Blur Strength and Glow Intensity, or should first release use one parameter at a time for a more interpretable grid? Proposed MVP: two axes, with explicit axis selectors and user-visible values.
2. Should Apply close the explorer or leave it open for another refine round? Proposed behavior: Apply commits and closes; opening Explore again begins a new session. Refine is available before Apply, not after.
3. Confirm the detached effect-cloning/serialization factory available for all effect instances; if cloning is not reliable, add explicit pure parameter evaluation hooks for these two effects rather than temporarily mutating/restoring live state.
4. Confirm project localization catalog convention and the exact supported UI test target after checking `AGENTS.md` instructions in the implementation session.
