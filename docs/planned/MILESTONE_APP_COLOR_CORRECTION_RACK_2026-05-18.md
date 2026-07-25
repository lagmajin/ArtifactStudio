# MILESTONE_APP_COLOR_CORRECTION_RACK_2026-05-18

## Goal
Add a first-class color correction rack that feels native to the Inspector and makes the app immediately better for day-to-day compositing work.

## Why this matters
The current surface is already getting better at selection, navigation, and empty-state guidance.
The next big gap is tonal control: users need a place to adjust exposure, contrast, saturation, hue, and related corrections without leaving the Inspector.

This milestone is about making those controls easy to discover, easy to stack, and easy to revisit.

## Scope
- Add a dedicated color correction effect group or rack that is visible from the Inspector workflow.
- Expose a small set of core adjustments first:
  - Exposure
  - Contrast
  - Brightness
  - Saturation
  - Hue
- Add the next tier of classic corrections if the architecture already supports them cleanly:
  - Levels
  - Curves
  - White balance / temperature
  - Tint
- Keep the controls consistent with the existing effect/property editor patterns.
- Make the rack work naturally with selection changes and effect focus.
- Keep the UI readable when no project, no composition, or no layer is selected.

## Non-goals
- Do not redesign the entire Inspector.
- Do not add a new global event system.
- Do not introduce QtCSS or QColorDialog.
- Do not expand into a full color management system unless the current architecture already supports it.

## Deliverables
- A usable color correction entry point in the Inspector.
- A stable set of default controls with sensible labels and ordering.
- Empty-state guidance that tells the user how to get to the rack.
- Enough visual polish that the new controls feel like part of the app rather than a bolt-on panel.

## Current progress
- Inspector guidance now points users toward color controls instead of generic effect parameters.
- The empty-state copy is aligned around opening a composition, selecting a layer, and then editing color controls.
- The next implementation step is to wire an actual color-correction preset or property set into the effect editor path.

## 2026-07-25 実装監査

Exposure／Hue and Saturation／Photo Filter／Auto Exposure などの個別 ColorCorrection effect、Color Grading Engine、Looks preset browser の基盤は実装を確認した。一方、Inspector 内で Exposure／Contrast／Brightness／Saturation／Hue を一体の rack として表示する導線、選択変更との安定した連動、未選択時の rack への具体的な到達案内は確認できない。したがって色補正機能の部品は実装済みだが、本マイルストーンの Inspector rack UX と受け入れ条件は未完了・runtime未検証とする。

## Implementation notes
- Prefer reusing the existing property editor and effect rack conventions.
- Keep the first pass small and composable.
- If a control cannot be expressed cleanly with the current widgets, defer it rather than forcing a brittle special case.
- Favor direct manipulation inside the existing selection and effect workflow.

## Success criteria
- A user can open a composition, select a layer, and immediately find color correction controls.
- The most common tonal adjustments are available without leaving the Inspector.
- The rack reads as part of the same UI language as the rest of the app.
- Empty states still point the user toward the next action.

## Suggested follow-ups
- Add presets for common looks.
- Add a curves editor if the base rack proves stable.
- Add a preview-friendly before/after toggle if it fits the current render path.
