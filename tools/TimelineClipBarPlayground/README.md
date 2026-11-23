# Timeline Clip Bar Diligent Playground

`ArtifactIRenderer` の既存Diligent描画APIで、クリップバーの3つの表現を同一フレームへ描くヘッドレスGPUキャプチャ用Playground。タイムラインWidgetやプロジェクトデータは使用しない。

## Build and run

Configure the ArtifactStudio root project, then explicitly build this opt-in target:

```powershell
cmake --build <build-directory> --config Debug --target ArtifactTimelineClipBarPlayground
<build-directory>\bin\Debug\ArtifactTimelineClipBarPlayground.exe --capture <output.png>
```

## Controls

The captured frame uses dense spacing and highlights design B as the selected candidate.

## Scope and evaluation

This prototype uses Diligent GPU draw calls for panels, rectangles, lines, and fixed bitmap labels. Bars and waveform marks are deterministic sample geometry. A Debug build and D3D12 capture were verified on an NVIDIA GeForce RTX 4070 Ti. It does not exercise the production Timeline hit testing, trimming, selection synchronization, media thumbnails, or editing path. Compare its captures with the browser lab for design only; do not treat this target as production timeline integration evidence.
