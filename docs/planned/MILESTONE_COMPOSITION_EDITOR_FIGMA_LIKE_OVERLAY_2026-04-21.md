# マイルストーン: Composition Editor Figma-like Overlay / Snap / HUD

> 2026-04-21 作成

## 目的

コンポジットエディタに、Figma 風の「見てすぐ分かる」補助レイヤーを足す。

狙いは見た目の派手さではなく、操作中に必要な情報を最小コストで返すこと。

## 何を足すか

- Smart Guides / Snap
  - 辺合わせ
  - 中心合わせ
  - 同幅 / 同高
  - 間隔一致
  - 近接候補の可視化
- Selection Overlay
  - 選択矩形
  - 中心点
  - 回転ハンドル
  - サイズ / 位置 / 回転角の簡易表示
- Useful HUD
  - レイヤー名
  - ブレンドモード
  - マスク数 / マット有無
  - ROI / frame / timecode
- Context Overlay
  - 近いレイヤーとの距離
  - ビデオの current frame / decode state
  - マスク / マットの要約
- Pixel / Region Probe
  - マウス位置の RGBA
  - どのレイヤーに属するか
  - どのマスク / matte で影響したか

## Non-Goals

- 新しい signal/slot の配線増加
- QtCSS 追加
- 常時重いプローブ
- Figma の完全再現

## Design Principles

- 操作中にだけ効く
- overlay は描画本体に寄せる
- スナップは補助であって強制ではない
- 情報は selection context に限定する
- diagnostics は既存の `FrameDebug` / `Trace` と共有する

## Suggested Execution Order

1. snap / smart guides
2. selection overlay / handles / bounds
3. useful HUD / context overlay / probe

## Target Files

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

## Related Docs

- `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`
- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`
- `docs/planned/MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md`
