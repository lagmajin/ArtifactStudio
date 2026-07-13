# Content Viewer Design Mockups

Date: 2026-07-12

## Selected Direction

`content-viewer-compare-selected.png` is the selected baseline.

The target is a Nuke-inspired, comparison-first Content Viewer integrated with
ArtifactStudio's existing Asset Browser, playback service, RAM Preview state,
and dock-based desktop layout.

## Images

- `content-viewer-focus.png`: single-source inspection direction
- `content-viewer-compare-selected.png`: selected A/B compare and wipe direction
- `content-viewer-review-strip.png`: continuous asset browsing direction

## Responsibility Boundary

- Asset Browser owns discovery, selection, filtering, and assignment of inputs A/B.
- Content Viewer owns media inspection, playback, channel/display transforms,
  zoom, A/B comparison, wipe/split/difference modes, and scopes.
- Composition Editor remains the editing surface. Opening an asset there is an
  explicit action rather than the default preview behavior.
- Playback and RAM Preview state remain service-owned rather than widget-owned.

## First Implementation Shape

1. Add a dockable Content Viewer shell using existing theme tokens and palette.
2. Support one source first, then add A/B source slots and swap.
3. Reuse the existing playback service for frame, timecode, range, and cache state.
4. Add wipe first; split and difference can follow without changing ownership.
5. Keep scopes optional/collapsible so the image remains the primary surface.

## Interaction Baseline

- Single click in Asset Browser selects an asset.
- Space previews the selected asset.
- Double click pins/opens it in Content Viewer.
- `1` and `2` assign Viewer A and B; swap exchanges them.
- `J`, `K`, `L` control reverse, pause, and forward playback.
- Arrow keys step frames; Shift+arrow jumps by a larger interval.
- `F` fits the image; `1:1` shows actual pixels.
- Opening in Composition is an explicit secondary action.
