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

---

## Static audit follow-up (2026-07-25)

現行の `FrameResourceInspectorWidget`、`FrameDebugViewWidget`、resource snapshot を確認した。ビルド・実機 resource 操作は未実施。

| Ticket | 現状 | 判定 |
|---|---|---|
| P2-T1 Resource Model | resource/attachment/preview/readback の snapshot と selection 対応が存在する。MIP/array/slice/channel/pixel inspect の完全な型・経路は未確認。 | 部分実装 |
| P2-T2 Resource Inspector UI | 任意 resource の要約、attachment、preview、format/readback 情報を read-only 表示する UI がある。live resource 切替の実機確認は未実施。 | 部分実装／確認待ち |
| Render Bridge | renderer/frame snapshot から resource を表示できる。ROI/partial-eval/transient の網羅的関連付けは未確認。 | 部分実装 |

### Phase 2 判定

Resource Inspector の read-only first cut は存在するが、任意 live resource の完全な選択・pixel inspect・slice/channel 操作は未完了。Phase 2 は「部分実装／実行確認待ち」とする。
