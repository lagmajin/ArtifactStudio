# Repository Audit: Performance Hotspots

## Purpose

Identify expensive paths in text, render, preview, and cache invalidation before optimizing anything.

## Scope

- `Artifact/src/Layer/ArtifactTextLayer.cppm`
- `Artifact/src/Render/*`
- `Artifact/src/Widgets/Render/*`
- cache invalidation and redraw triggers

## Questions to Answer

- Which code paths are re-running work more often than necessary?
- Where are cache keys incomplete or too broad?
- Which hot paths still do full recompute on small property changes?

## Deliverable

- A ranked hotspot list with likely wins.

