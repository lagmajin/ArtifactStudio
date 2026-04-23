# Phase 2 実行メモ: Always-on Resource Inspector - File Tickets

> 2026-04-21 作成

## 目的

`MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE2_2026-04-21.md` を、実装にそのまま切れる作業票へ落とす。

## チケット 1: `FrameResourceView`

対象候補:
- `ArtifactCore/include/Render/FrameResourceView.ixx`
- `ArtifactCore/include/Render/FramePixelInspect.ixx`

やること:
- resource selection の共通モデルを定義する
- pixel inspect 用の座標 / slice / format を持たせる

完了条件:
- 任意 resource を共通表現で読める

## チケット 2: resource inspector UI

対象:
- `Artifact/src/Widgets/Diagnostics/FrameResourceInspectorWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

やること:
- ライブプレビューの read-only 表示を作る
- MIP / array / slice の切り替えを足す

完了条件:
- resource を常時見られる

