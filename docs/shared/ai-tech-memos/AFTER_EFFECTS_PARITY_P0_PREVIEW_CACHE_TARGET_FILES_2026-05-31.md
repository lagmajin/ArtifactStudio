# After Effects Parity P0 Preview Cache Target Files

> 2026-05-31

## Purpose

`preview / cache / playback` の P0 を追うときに、まず見る対象ファイルを固定するためのメモ。

## Primary Files

- [Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
  - preview state の見え方
  - cache / playback / scrub の接点
  - `requested / ready / failed` の UI 反映
- [Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm)
  - viewport 更新
  - `Fill` / `100%` の初期化
  - resize / initial fit のタイミング
- [ArtifactCore/src/Transform/ViewportTransformer.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Transform/ViewportTransformer.cppm)
  - `fit` と `cover`
  - `FillCanvasToViewport()` / `FitCanvasToViewport()`
  - zoom / pan の基礎計算

## Related Files

- [X:/Dev/ArtifactStudio/docs/bugs/BUG_COMPOSITION_EDITOR_ZOOM_FILL_100_PERCENT_MISPOSITION_2026-05-30.md](X:/Dev/ArtifactStudio/docs/bugs/BUG_COMPOSITION_EDITOR_ZOOM_FILL_100_PERCENT_MISPOSITION_2026-05-30.md)
  - `Fill / 100%` のズレの調査メモ
- `Artifact/src/Widgets/Render/ArtifactPlaybackService.cppm`
  - preview / playback の状態管理があるなら最初に確認する候補
- `Artifact/src/Widgets/Render/ArtifactTimelineWidget.cppm`
  - scrub / cache / playback の UI 側の接点候補

## What To Verify In Each File

### Render Controller

- preview がどの状態を `ready` とみなすか
- cache hit と render complete の関係
- playback / diagnostics の truth source

### Composition Editor

- viewport がいつ再計算されるか
- `Fill` / `100%` がどのタイミングで適用されるか
- resize debounce と initial fit が競合しないか

### Viewport Transformer

- `Fill` が cover であることが仕様として明示されているか
- `100%` が physical pixel 基準になっていないか
- pan のオフセットが logical / physical の期待と一致するか

## Suggested Read Order

1. [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_INVESTIGATION_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_INVESTIGATION_2026-05-31.md)
2. [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_QUICKCHECK_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_QUICKCHECK_2026-05-31.md)
3. [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md)
4. This file
5. [AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md)

## Notes

- ここでは修正方針はまだ決めない
- まず `何が truth source か` を固定する
- そのあとで state contract と UI contract を分けて直す

