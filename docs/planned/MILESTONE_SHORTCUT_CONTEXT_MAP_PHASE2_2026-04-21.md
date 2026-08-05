> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md](MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md)

# M-SC-2 Phase 2 Widget / Region Registration
**作成日:** 2026-04-21

## Goal

主要 widget を region 単位で keymap 登録できるようにする。

## Targets

- `ArtifactCompositionRenderWidget`
- `ArtifactTimelineWidget`
- `ArtifactLayerPanelWidget`
- `ArtifactAssetBrowser`
- `ArtifactInspectorWidget`
- `ArtifactProjectManagerWidget`

## Tasks

- 各 widget に context を対応付ける
- `Composition` 系の region を `Viewport` と `Overlay` に分ける
- `Timeline` 系の region を `Left` / `Right` に分ける
- `LayerPanel` と `AssetBrowser` の独立 keymap を登録する
