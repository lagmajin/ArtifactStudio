# Timeline Curve Editor Mode

## Goal

Add a curve editor mode to `ArtifactTimelineWidget` so the same surface can switch between:

- the normal timeline / layer orchestration view
- a curve-focused editor for animated properties and keyframes

The intended trigger is a keyboard toggle on `U` or `Tab` while the timeline has focus.

## UX Intent

- keep the playhead, selection, and zoom context when switching modes
- preserve the user's current time position
- make the transition feel like a mode change, not a new window
- keep timeline editing available as the default path
- show curve editing only when the current selection can actually expose animated properties

## Scope

- mode state and toggle routing inside `ArtifactTimelineWidget`
- a curve editor surface that reuses the current time context
- selection-aware property curve presentation
- keyboard focus and shortcut handling for `U` / `Tab`
- undo-friendly curve edits for keyframe navigation and value changes
- documentation alignment with `docs/WIDGET_MAP.md`

## Phases

### Phase 1

- define the mode state and toggle entry points
- wire keyboard handling for `U` and `Tab`
- preserve playhead and selection when switching modes

### Phase 2

- introduce the curve editor surface and basic curve rendering
- connect selected animated properties to visible curves
- add curve navigation, zoom, and pan behavior

### Phase 3

- integrate curve edits with the existing keyframe / property system
- add regression tests for shortcut routing and state preservation
- expose the mode to AI-assisted workflows if useful

