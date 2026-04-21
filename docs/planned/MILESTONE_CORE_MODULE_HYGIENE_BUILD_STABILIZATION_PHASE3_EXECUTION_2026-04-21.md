# Phase 3 実行メモ: API Compatibility Pass - File Tickets

> 2026-04-21 作成

## 目的

`MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE3_2026-04-21.md` を、実装にそのまま切れるファイル別チケットへ落とす。

## チケット 1: `Property.ixx`

対象:
- [`ArtifactCore/include/Property/Property.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/Property.ixx)

やること:
- `getTypeName()` 参照を消す
- `PropertyType` -> string の helper を通す
- read-only snapshot の type 表示を安定化する

完了条件:
- compile error が解消される

## チケット 2: `ArtifactRenderQueueService.ixx`

対象:
- [`Artifact/include/Render/ArtifactRenderQueueService.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderQueueService.ixx)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)

やること:
- `SessionLedger` の import を明示する
- read-only accessor がヘッダ側で見えるようにする
- session / failed frame / snapshot 系の API を壊さない

完了条件:
- `SessionLedger` の未解決参照が消える
- API の可視性が宣言と一致する

## チケット 3: `ArtifactRenderROI.ixx`

対象:
- [`Artifact/include/Render/ArtifactRenderROI.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderROI.ixx)

やること:
- `TileKey` / `TileGrid` / `DirtyRegionAccumulator` の順序を安定化する
- numeric helper 導入後の型崩れを吸収する

完了条件:
- 追加した helper と API が矛盾しない

## チケット 4: `FrameDebug.ixx`

対象:
- [`ArtifactCore/include/Frame/FrameDebug.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Frame/FrameDebug.ixx)

やること:
- `toJson()` / `fromJson()` の read-only surface を維持する
- `FrameDebug*` 型の利用側が include / import で迷わないようにする

完了条件:
- snapshot 系の API が安定して見える

## 実装順

1. `Property.ixx`
2. `ArtifactRenderQueueService.ixx`
3. `ArtifactRenderROI.ixx`
4. `FrameDebug.ixx`

