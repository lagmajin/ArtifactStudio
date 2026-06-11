# Repository Audit: Selection and Context

## Purpose

Review selection state, active context, gizmo routing, and shortcut context usage as one system.

## Scope

- `Artifact/src/Layer/Selection*`
- `Artifact/src/Widgets/Render/*`
- `Artifact/src/Application/*`
- shortcut and tool routing code

## Questions to Answer

- Which selection state is global, composition-local, or widget-local?
- Where do gizmos rely on implicit active-layer assumptions?
- Which context names are already stable and which still drift?

## Deliverable

- A responsibility map and a list of context edge cases.

