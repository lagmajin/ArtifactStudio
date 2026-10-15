# Blur and Glow Variation Explorer Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Add a visual 3×3 parameter-exploration workflow for Blur and Glow, allowing users to compare candidates, select one, and iteratively refine around it without mutating the project until confirmation.

**Architecture:** Build one shared candidate-generation/session/UI flow, with effect-specific adapters for parameter discovery, bounds, and temporary render overrides. Render candidates sequentially at reduced preview resolution through a non-mutating preview path; commit only the selected parameter through the existing effect service/Undo path. Start with one scalar parameter per effect.

**Tech Stack:** Existing C++20 modules, Qt Widgets, Artifact effect/property services, existing preview renderer and test infrastructure.

---

## Context and constraints

- Repository: `C:/dev/ArtifactStudio`.
- Existing Glow implementation: `Artifact/src/Effects/Glow/GlowEffect.cppm`; `baseSigma` is presented as **Radius**. `GlowEffect::getProperties()` and `setPropertyValue()` are the relevant property methods.
- Existing commit path: `Artifact/src/Service/ArtifactEffectService.cppm`, `ArtifactEffectService::setEffectProperty()`; this path participates in Undo, dirty state, and notifications. Use only when the user confirms a candidate.
- Property UI: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` (`ArtifactPropertyWidget::Impl` / `editableProperties()`).
- Preview controller: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`; viewport widget files are `Artifact/include/Widgets/Render/ArtifactCompositionRenderWidget.ixx` and `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`.
- Glow GPU work is in part under `ArtifactCore/src/ImageProcessing/DirectCompute/GlowCS.cppm`; existing analysis notes flag readback/synchronization constraints for neighborhood effects.
- Do not overwrite or revert pre-existing working-tree changes. Before implementation, re-run `git status --short` and keep changes scoped to feature files.
- Exact Blur property name/range and a suitable non-mutating preview seam must be confirmed in discovery; do not assume Blur and Glow expose identical parameter APIs.

## Product scope (MVP)

- Support one scalar parameter each: Blur radius/size (confirm actual property and units in source) and Glow `baseSigma` / Radius.
- Generate 9 ordered candidates around the current value. Initial exploration uses a broad, valid range; refinement uses a smaller step around the selected value.
- Show candidates in a dedicated dialog/panel with value labels, render progress, cancel/retry, and a clear selected state.
- Candidate rendering does not mutate live effect properties, create Undo commands, or change the project dirty state.
- Confirming a candidate changes exactly one property through the established effect service and creates one undoable change.
- MVP renders the stopped/current frame, sequentially, at reduced preview resolution. No playback, multi-parameter grids, persistent candidate cache, or simultaneous GPU dispatch in first release.

## Design decisions to validate before coding

1. Trace Blur's concrete property implementation and identify its scalar parameter, legal range, unit/scale, and whether it is GPU or CPU rendered.
2. Find an existing renderer entry point that can render a composition/frame with a per-render property override without changing the model. Prefer that seam. If none exists, implement a narrowly-scoped preview override context; do not temporarily call `setEffectProperty()` nine times.
3. Verify whether thumbnails can stay GPU-resident and be composed into the candidate panel. If CPU images are required, use explicit, bounded readbacks and measure cost; never trigger hidden readback in a loop.
4. Ensure candidate requests are tagged with a generation/session ID so stale results are discarded after cancel, effect/layer/frame changes, or a newer refinement request.
5. Define step/range behavior for linear and logarithmic-looking controls. Clamp to effect limits, avoid duplicate candidates at bounds, and always include the current/selected value.

## Implementation tasks

### Task 1: Map Blur/Glow parameters and render seams

**Objective:** Confirm exact parameter definitions and a non-mutating way to render one effect variant.

**Files:** Read `Artifact/src/Effects/Blur/` and `Artifact/src/Effects/Glow/`; inspect effect interfaces, preview/render controller, `ArtifactEffectService`, and relevant tests. No edits until the chosen seam is documented.

**Steps:**
1. Identify concrete Blur effect class(es), scalar radius property, limits, and setter/getter semantics.
2. Trace the preview render call from `ArtifactCompositionRenderController.cppm` into the effect execution path.
3. Determine whether a transient property override can be passed to rendering without model mutation.
4. Inspect existing test targets in `tests/Artifact` and `tests/ArtifactCore`, plus root CMake presets/build instructions.
5. Record the selected seam and fallback behavior in code comments/design notes before implementing.

**Acceptance:** Both effects have confirmed property IDs, ranges, and preview execution domains; no assumption that an ordinary property setter is safe for candidate rendering.

### Task 2: Define deterministic 3×3 candidate generation

**Objective:** Add a small testable value-generation component for coarse and fine exploration.

**Likely files:** New focused component under `Artifact/include/Effect/` and `Artifact/src/Effect/` (final location follows existing module conventions); new unit test under `tests/Artifact` or `tests/ArtifactCore`; update the corresponding CMake test list.

**Steps:**
1. Write tests for nine ordered values around a center, positive/non-negative bounds, clamping, duplicate removal at bounds, finite numeric inputs, and refinement-step reduction.
2. Run the focused test and verify it fails for the missing generator.
3. Implement a deterministic generator parameterized by center, step/range, and legal min/max; keep effect-specific units outside the generic algorithm.
4. Test boundary cases and ensure a bounded domain still returns unique candidates where possible (otherwise report fewer than nine rather than fabricating duplicate choices).

**Acceptance:** Candidate values are deterministic, bounded, unique where the domain permits, and refinement narrows around the chosen value.

### Task 3: Add non-mutating candidate render request/session

**Objective:** Render one candidate at a time without changing serialized properties, undo history, or dirty state.

**Likely files:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` plus a small request/result type near the rendering interfaces; add focused render-controller/service tests.

**Steps:**
1. Add a test/probe that captures the target effect property and dirty/undo state, requests a candidate render, and asserts they remain unchanged.
2. Add an explicit candidate render request carrying composition/frame, effect identity, property ID/value override, preview dimensions/quality, and session generation ID.
3. Queue requests sequentially; expose progress and cancellation; discard results whose generation ID is stale.
4. Guarantee cleanup of temporary render resources on success, failure, cancellation, and project/effect changes.
5. Keep unsupported effect/render paths on the existing renderer; fail visibly rather than silently showing a render without the override.

**Acceptance:** Rendering all nine candidates never changes the live property or creates Undo/dirty notifications, and cancel/stale requests cannot replace current results.

### Task 4: Add shared variation dialog and effect-specific entry points

**Objective:** Let users launch the same exploration UI from Blur and Glow properties.

**Likely files:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`; add dialog/widget `.ixx` and `.cppm` in the established Widgets organization; translation catalogs under `Artifact/translations/`.

**Steps:**
1. Write UI/model tests for candidate loading, selected candidate state, cancel, confirm, and refine transitions.
2. Create a dedicated 3×3 thumbnail panel with parameter values, loading/error states, keyboard-accessible selection, and clear Apply/Refine/Cancel controls.
3. Add effect-specific adapters for Blur radius and Glow `baseSigma`; keep candidate UI and scheduling shared.
4. Make refine generate a narrower neighborhood centered on the selected candidate without committing it.
5. Add translated labels for at least all supported locales using the project's established translation workflow.

**Acceptance:** Both supported effects use the same UI; switching/canceling does not mutate the document; refinement visibly centers on the selected candidate.

### Task 5: Commit selection through existing Undo path

**Objective:** Apply only the confirmed candidate as one normal property edit.

**Files:** `Artifact/src/Service/ArtifactEffectService.cppm` and focused service/Undo tests; dialog confirmation handler.

**Steps:**
1. Test confirmation yields one property change and one Undo step; cancel/refine yield none.
2. Route confirmation through `ArtifactEffectService::setEffectProperty()` using the selected effect/property/value.
3. Verify undo restores the original value and redo restores the selected candidate.
4. Close or refresh the variation UI after commit and trigger the normal preview invalidation path.

**Acceptance:** Exactly one selected value is committed and is fully undoable/redoable; exploratory candidates never enter history.

### Task 6: Performance and end-to-end verification

**Objective:** Confirm the feature is responsive and visually consistent for both effects.

**Files:** Existing render/UI integration tests; add or extend only targeted tests and diagnostics.

**Steps:**
1. Run focused candidate-generation and service tests.
2. Build the affected targets using the repository's documented Windows/CMake preset and verify actual build output.
3. Run an interactive scenario for Blur and Glow separately: open explorer, wait for nine thumbnails, refine, confirm, undo/redo, cancel.
4. Measure total candidate latency, per-candidate latency, peak memory/VRAM where available, and readback count. Compare sequential reduced-resolution rendering against normal preview.
5. Check that chosen thumbnail and committed full-quality preview correspond within expected preview-quality differences.
6. Run relevant existing regression tests and confirm unrelated pre-existing working-tree changes remain untouched.

**Acceptance:** No property/Undo/dirty mutation before confirmation; no stale thumbnails after cancellation/refinement; both effects render correct overrides; acceptable measured latency/memory; build and focused tests pass. Document hardware and measured timings rather than claiming a generic GPU speedup.

## Likely files to change

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Service/ArtifactEffectService.cppm` (only if an explicit commit helper is needed; prefer existing API)
- `Artifact/src/Effects/Glow/GlowEffect.cppm` and the confirmed Blur implementation only if adapter metadata is needed; avoid changing core effect semantics.
- New shared candidate/session component and dedicated widget files, placed according to existing module/CMake conventions.
- `Artifact/translations/*.json`
- Focused tests and relevant `CMakeLists.txt` test registration.

## Risks and mitigations

- **No safe override render seam:** add a narrow transient override context at the render boundary; never simulate candidates by repeated live mutations.
- **GPU readback or ninefold cost:** sequential scheduling, reduced-resolution thumbnails, bounded in-flight resources, cancellation; measure before adding concurrency.
- **Preview differs from final rendering:** reuse the same effect implementation and parameter semantics; only resolution/quality may differ; verify selected candidate against final preview.
- **Boundary candidates duplicate:** deduplicate and present fewer options if the legal domain cannot supply nine unique values.
- **Glow/Blur parameter scales differ:** effect-specific step policies; shared code handles only candidate mechanics and lifecycle.
- **Stale asynchronous work:** generation IDs plus cancellation and resource-safe completion callbacks.
- **Large UI scope:** dedicated dialog first; defer persistent docking/customizable grids.

## Explicitly out of scope for MVP

- Simultaneous multi-parameter exploration.
- All-effect generic support.
- Concurrent GPU candidate rendering.
- Playback-time exploration or rendering multiple timeline frames.
- Saving/exporting contact sheets or candidate history.
- Any change to the effect's visual algorithm.
