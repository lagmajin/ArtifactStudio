# Harness Engineering Principles

This note defines the working rules for harness engineering in Artifact.

The goal is not to add more debug UI. The goal is to make implementation loops shorter, safer, and easier to compare.

---

## What Harness Engineering Means

Harness engineering is the practice of building a small, repeatable working surface around a feature so we can:

- fix the input
- observe the output
- classify failures
- compare changes
- repeat the same check without re-learning the app

It is a development tool, a diagnosis tool, and a regression tool at the same time.

---

## Core Principles

### 1. Fixed Input

Every harness must define a small set of named scenarios or presets.

- do not rely on hidden editor state
- do not depend on the previous run
- keep fixtures deterministic where possible

### 2. Fixed Observation

Every harness must emit the same vocabulary for what happened.

- use text-first reports
- record the active goal, preset, and result
- prefer `FrameDebugSnapshot` or equivalent structured state over ad hoc overlays

### 3. Fixed Failure Taxonomy

Failures must be named, not implied.

- `ok`
- `skipped`
- `failed`
- `degraded`
- `pending`

The report should explain why the state landed in that bucket.

### 4. Short Feedback Loop

The harness should answer the next implementation question quickly.

- what changed
- what stayed broken
- what to try next

If the harness does not reduce the next decision, it is too vague.

### 5. No New Global Wiring

The harness must reuse existing paths where possible.

- no central event bus just for harnesses
- no extra app-wide signal/slot graph
- no duplicate backend path just for inspection

### 6. Comparison Over Impression

The harness should help compare states, not just display them.

- report identity
- timestamp
- preset
- frame number
- scene / layer / backend context

### 7. Separate Concern Boundaries

The harness must not absorb product UI responsibilities.

- diagnostics are not editing
- presets are not policies
- overlays are not the same as scene content

If a feature needs real editor behavior, it belongs in the feature contract, not the harness.

---

## Recommended Report Shape

Use the same top-level questions in every harness report:

- What was the goal?
- What preset or scenario ran?
- What happened?
- Was it expected?
- What should I do next?

A good harness report is actionable even if the UI is closed later.

---

## Anti-Patterns

- one-off overlays that cannot be copied
- silent skips
- state that survives preset switches
- reports that only say `something is wrong`
- debug-only branches that drift away from the production path
- hiding a failure behind a prettier visualization

---

## First Use Cases

The first places to apply these rules in Artifact are:

- `DebugRenderHarnessWidget`
- `FrameDebugSnapshot`
- `AppDebuggerWidget`
- `DebugRenderHarness` report text
- `Blend / Mask Contract` diagnostics
- future property / timeline smoke surfaces

---

## Relation To Existing Docs

- [`../planned/MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`](../planned/MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md)
- [`DEBUG_RENDER_HARNESS_REPORT_TEMPLATE_2026-04-30.md`](./DEBUG_RENDER_HARNESS_REPORT_TEMPLATE_2026-04-30.md)
- [`DEBUG_RENDER_HARNESS_SCENE_PRESET_CONTRACT_2026-04-30.md`](./DEBUG_RENDER_HARNESS_SCENE_PRESET_CONTRACT_2026-04-30.md)
- [`BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md`](./BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md)

---

## Next Step

Add a `goal / expected / actual / next action` block to the report contract and keep it aligned with the existing summary fields.
