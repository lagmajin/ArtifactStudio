# Timeline Clip Bar Diligent Playground

`ArtifactIRenderer` の既存Diligent描画APIで、クリップバーの3つの表現を同一画面に描く独立Playground。タイムラインWidgetやプロジェクトデータは使用しない。

## Build and run

Configure the ArtifactStudio root project, then explicitly build this opt-in target:

```powershell
cmake --build <build-directory> --config Debug --target ArtifactTimelineClipBarPlayground
<build-directory>\bin\Debug\ArtifactTimelineClipBarPlayground.exe
```

## Controls

- Click a comparison row to select its design
- **1–3**: select a design
- **D**: toggle dense row spacing

## Scope and evaluation

This prototype uses Diligent GPU draw calls for panels, rectangles, lines, and labels. Bars and waveform marks are deterministic sample geometry. It does not exercise the production Timeline hit testing, trimming, selection synchronization, media thumbnails, or editing path. GPU initialization and runtime appearance have not been verified. Compare its captures with the browser lab for design only; do not treat this target as production timeline integration evidence.
