# HiDPI Fixed-Size UI Audit

**Date**: 2026-06-15
**Scope**: ArtifactStudio parent repo only. Static scan of fixed-size and fixed-dimension GUI code paths.

This is a lightweight inventory of GUI areas that are most likely to feel tight or brittle on 4K / HiDPI displays. It is not a full runtime verification.

## High-risk spots

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
  - Fixed chrome strip and bottom bar heights.
  - Main editor size hint is still a single hard-coded desktop size.
- `Artifact/src/Widgets/Dialog/ArtifactCreateCompositionDialog.cppm`
  - Entire dialog is fixed-size.
  - Several controls use fixed widths/heights.
- `Artifact/src/Widgets/Dialog/CreatePlaneLayerDialog.cppm`
  - Entire dialog is fixed-size.
  - Multiple labels and buttons rely on fixed pixel dimensions.
- `Artifact/src/Widgets/Dialog/CreateCameraLayerDialog.cppm`
  - Entire dialog is fixed-size.
  - Several rows use hard-coded widths.
- `Artifact/src/Widgets/Dialog/PrecomposeDialog.cppm`
  - Entire dialog is fixed-size.
  - Layer list and header spacing are fixed.
- `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm`
  - Play/control widgets are mostly fixed-size.
  - Several button widths look tuned for one density.
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
  - Many action buttons use fixed sizes.
  - This is less risky than full dialogs, but still worth watching on dense displays.

## Medium-risk spots

- `Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`
  - Fixed heights on the timecode chrome.
- `Artifact/src/Widgets/Timeline/ArtifactTimelineLabel.cppm`
  - Fixed height label strip.
- `Artifact/src/Widgets/Render/ArtifactCompositionViewerFooter.cppm`
  - Fixed-width selection label.
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
  - Fixed-size shell driven by a custom calc routine.
- `Artifact/src/Widgets/Diagnostics/ProfilerOverlayWidget.cppm`
  - Fixed-size overlay based on calculated width/height.

## Lower-risk / probably acceptable

- Icon buttons with explicit square sizes.
- Thumbnail cells and compact tool buttons where density consistency matters more than fluid layout.
- Small fixed dimensions that are clearly meant as tap targets rather than layout containers.

## Takeaway

The parent app is not uniformly rigid, but there are still several dialog- and chrome-level fixed-size containers that can feel cramped or disproportionate on HiDPI displays.
The biggest practical risk is not the renderer anymore; it is these higher-level layout shells and a few density-sensitive controls.
