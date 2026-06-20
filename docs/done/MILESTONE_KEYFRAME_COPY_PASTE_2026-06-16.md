# M-CLIP-1 Keyframe Copy & Paste

**Date**: 2026-06-16
**Status**: Completed
**Source**: [`../planned/MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md`](../planned/MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md)

## Summary

Timeline keyframe copy / paste is already wired through the clipboard manager and timeline widgets.

## Current Ground Truth

- `ArtifactCore::ClipboardManager` has keyframe clipboard support
- `ArtifactTimelineWidget` copies selected keyframes into the clipboard and pastes them at the playhead
- `ArtifactTimelineTrackPainterView` exposes keyframe copy / cut / paste actions in the timeline context menu
- `ArtifactAnimationMenu` exposes the copy / paste entry points through the edit menu

## Done Criteria

- Timeline 上で keyframe をコピーして別の位置へペーストできる
- clipboard に keyframe データが載る
- timeline / menu から同じ導線で呼べる

## Related Files

- `ArtifactCore/include/Clipboard/ClipboardManager.ixx`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`
- `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`

