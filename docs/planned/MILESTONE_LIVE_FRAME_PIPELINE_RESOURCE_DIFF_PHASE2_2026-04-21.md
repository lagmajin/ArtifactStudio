# Phase 2: Always-on Resource Inspector

> 2026-04-21 作成

## 目的

[`docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md) の Phase 2 を、常時見える resource inspector として切り出す。

---

## 方針

1. 任意 resource をライブで選べるようにする
2. MIP / array / slice / channel を切り替えられるようにする
3. pixel inspect は read-only から始める
4. mask / ROI / transient resource の関係を見える化する

---

## 実装タスク

### 1. Resource View model を定義する

追加候補:

- `FrameResourceView`
- `FrameResourceSelection`
- `FramePixelInspect`

やること:

- texture / buffer / RT を共通表現で扱う
- selection state を保持する
- pixel inspect に必要な座標と slice を持つ

### 2. 常時表示の inspector を作る

候補ファイル:

- `Artifact/src/Widgets/Diagnostics/FrameResourceInspectorWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

やること:

- resource 切り替え UI を作る
- read-only preview を出す
- pixel value / format / color space を読めるようにする

### 3. render path から resource を集める

候補ファイル:

- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

やること:

- attachment / readback / cache を収集する
- ROI / partial eval の対象を紐づける
- live selection に必要な識別子を固定する

---

## File Tickets

### P2-T1 Resource Model

対象:

- `ArtifactCore/include/Render/FrameResourceView.ixx`
- `ArtifactCore/include/Render/FramePixelInspect.ixx`

完了条件:

- resource selection と pixel inspect の型が読める

### P2-T2 Resource Inspector UI

対象:

- `Artifact/src/Widgets/Diagnostics/FrameResourceInspectorWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

完了条件:

- 任意 resource を読める UI がある

