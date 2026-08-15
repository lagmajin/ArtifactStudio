# マイルストーン: Composition Editor Figma-like Overlay / Snap / HUD

**最終更新:** 2026-08-15
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

## 2026-08-15 現行コード監査

- Smart Guides / Snap は `TransformGizmo.cppm` に実装され、composition edge、center、layer edge、spacing の候補線と回転 snap を扱う。
- Selection Overlay は `ArtifactCompositionRenderOverlay.cppm` に集約され、選択矩形、回転ハンドル、anchor/center、shape/mask/roto 系の表示、3D bounds/wireframe 経路が存在する。
- Composition Render Widget には Rotation / Anchor Point tool、ドラッグ中の rotation info overlay、selection sync、既存 event bus 経路がある。
- Pixel Probe は `showColorSamplerOverlay_`、`updateColorSamplerOverlay()`、`drawColorSamplerOverlay()` として実装され、現在フレームの RGBA／画像ピクセル／canvas座標に加えて、既存 hit-test による最上位 layer ID を hover 位置から表示する。Region単位の mask/matte追跡、ビデオ decode 状態の常時 context overlay、probe結果の FrameDebug diagnostics 統合は未完了または未検証。
- したがって snap と selection overlay は実装済み、Useful HUD と layer probe は部分実装、mask/matte context diagnostics は未完了として扱う。runtime 視認性は未検証。
