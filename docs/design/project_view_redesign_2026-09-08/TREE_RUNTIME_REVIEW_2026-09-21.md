# Project View Tree runtime review

**最終更新:** 2026-09-21

## Evidence

- [Runtime screenshot](project-view-tree-runtime-2026-09-21.png)
- [Primary mock](project-view-focused-workbench.png)
- Owner: `ArtifactProjectManagerWidget`

## Findings

1. **No-project state is duplicated.** Browse pane and detail pane both show `No project open`, while the adopted direction calls for one clear empty-state message. Keep the browse pane as the primary empty state and make the detail pane quiet or collapse it until a project exists.
2. **The browse controls do not read as one compact header.** Search, type, view mode, and unused filter are technically on one row, but wide gaps and three different control treatments fragment the row. Use a fixed-width type filter, a compact Tree／Tile segmented control, and a normal toggle for Unused.
3. **The context bar repeats state instead of showing hierarchy.** `Tree · All · All items` plus `0 items · 0 selected` repeats information already visible in the controls and status bar. Prefer a project breadcrumb／scope on the left and one result count on the right.
4. **The Tree columns are too sparse for structure-and-state reading.** The runtime screenshot exposes `Name / Size / Duration`; the mock gives type, status, modified time, and size enough room to distinguish project structure from a generic file list. Add or reprioritize `Type`, `Status`, and `Modified`; keep media-specific values secondary.
5. **Empty content is visually over-weighted.** The large cyan document glyph and centered copy dominate the full browse area, while the next action is only described in text. Reduce the illustration and place one existing action near the message, without turning Project View into a file browser.
6. **Action icons are visually louder than the information they operate on.** The colored square plates in the runtime header compete with the result count. The approved `project_*.svg` direction is monochrome, 16px-readable, and should sit on quieter 28px controls.
7. **Table header separators and surface bands are stronger than row structure.** The header and top chrome form several horizontal blocks before any content appears. Reduce band contrast and let row hover, selection, type, and status carry the hierarchy.

## Accessibility / verification limits

The screenshot supports contrast, density, hierarchy, and duplicate-state findings only. Focus order, keyboard operation, accessible names, hit targets, splitter behavior, Tree／Tile selection retention, and high-DPI rendering require runtime interaction. No build or runtime test was performed in this review.

## Smallest next move

First remove the duplicated no-project message and simplify the context bar. Then refine the header controls and columns without changing the underlying item model, selection synchronization, or Asset Browser responsibility boundary.

## Resolution

2026-09-21に findings 1–5 を実装へ反映した。empty stateの一本化、compactなTree／Tile switch、breadcrumb化、`Name / Type / Status / Size / Modified`列、empty iconとaction iconの抑制を行った。あわせてsurface bandとseparatorのcontrastも低減した。ビルドおよびruntime再撮影は未実施。
