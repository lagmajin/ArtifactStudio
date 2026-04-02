# MILESTONE: UI Theme System Rollout

> 2026-04-02 Draft

## Goal

Turn the existing theme work into a visible studio skin rollout.
The focus is not "more colors", but a consistent surface language across the main editor, property panes, dock chrome, and render-adjacent widgets.

## Why

The app already has a theme direction, but the surface language is still mixed:
- some widgets are theme-aware
- some still rely on fixed `QSS`
- some use palette only in part
- some important panes still feel like separate products

This rollout is meant to make the whole editor feel like one UI system.

## Scope

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/PropertyEditor/*`
- `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Timeline/*`
- `Artifact/src/Widgets/Dock/*`

## Non-Goals

- A full visual redesign of every widget
- Replacing every last custom paint path in one pass
- Changing the overall editor workflow
- Breaking the current dark-first look

## Phases

### Phase 1: Theme Tokens and Surface Definitions

- define app, surface, elevated, border, borderStrong, textPrimary, textSecondary, accent, accentSoft
- make sure the same tokens are reused in editor chrome and content panes
- keep palette mapping explicit so `QSS` does not become the source of truth again

### Phase 2: Core Studio Surfaces

- align main window, dock frames, toolbars, status bars, and headers
- make timeline, inspector, property editor, and render queue feel like one family
- remove obvious style mismatches first

### Phase 3: Property and Inspector Focus

- finish the property / inspector style ownership cleanup
- remove fixed color islands
- make hover, focus, selection, and disabled states consistent

### Phase 4: Studio Skin Polish

- refine spacing, separators, tab shapes, badges, and row density
- make the editor feel intentional rather than "themed by accident"
- keep render and timeline surfaces readable at a glance

## First Targets

1. `Artifact/src/AppMain.cppm`
2. `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
3. `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
4. `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
5. `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
6. `Artifact/src/Widgets/Dock/DockStyleManager.cppm`

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
