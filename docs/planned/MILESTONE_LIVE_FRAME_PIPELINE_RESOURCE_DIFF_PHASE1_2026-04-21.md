# Phase 1: Frame Graph / Pipeline View

> 2026-04-21 作成

## 目的

[`docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md) の Phase 1 を、実装にそのまま切れる形で進める。

この段階では「常時見える resource inspector」より先に、フレーム構造そのものを読めるようにする。

---

## 方針

1. まず Pass DAG を定義する
2. read / write / dependency を最小限で持つ
3. RT / Texture / Buffer の lifetime を追えるようにする
4. UAV / RTV / barrier hazard は簡易フラグから始める

---

## 実装タスク

### 1. Frame Graph model を定義する

追加候補:

- `FramePipelineGraph`
- `FramePipelineNode`
- `FramePipelineEdge`
- `FrameResourceLifetime`
- `FrameHazardRecord`

やること:

- pass を node として表現する
- read / write の edge を持たせる
- lifetime band を付ける

### 2. 既存 render path から DAG を組み立てる

候補ファイル:

- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`Artifact/src/Render/ArtifactIRenderer.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactIRenderer.cppm)

やること:

- pass 順序を読めるようにする
- attachment / readback / output を node に紐づける
- backend / fallback / partial eval の要約を付ける

### 3. hazard の簡易検出を追加する

やること:

- UAV / RTV の衝突候補を立てる
- barrier が必要そうな箇所をフラグ化する
- read-after-write / write-after-read を簡易に読む

---

## 実装順

1. `ArtifactCore/include/Render/FramePipelineGraph.ixx`
2. `ArtifactCore/include/Render/FrameResourceLifetime.ixx`
3. `ArtifactCore/include/Render/FrameHazardRecord.ixx`
4. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
5. `Artifact/src/Render/ArtifactRenderQueueService.cppm`
6. `Artifact/src/Render/ArtifactIRenderer.cppm`

---

## File Tickets

### P1-T1 Core Graph Model

対象:

- `ArtifactCore/include/Render/FramePipelineGraph.ixx`
- `ArtifactCore/include/Render/FrameResourceLifetime.ixx`
- `ArtifactCore/include/Render/FrameHazardRecord.ixx`

完了条件:

- node / edge / lifetime / hazard の型が読める

### P1-T2 Render Summary Bridge

対象:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`

完了条件:

- 既存 render path から DAG 要約を組み立てられる

---

## Static audit follow-up (2026-07-25)

現行の FramePipelineView、FrameDebug snapshot、resource/pass records を確認した。ビルド・GPU hazard の実測は未実施。

| Ticket | 現状 | 判定 |
|---|---|---|
| P1-T1 Core Graph Model | pipeline/pass/resource/attachment と hazard の要約型、graph summary の表示が存在する。明示的な read/write DAG と lifetime band の全型・全経路は未確認。 | 部分実装 |
| P1-T2 Render Summary Bridge | Composition/renderer の frame snapshot、backend/fallback、pass/resource summary がある。既存 render path 全てからの DAG 収集は未確認。 | 部分実装 |
| Hazard detection | failed pass、resource warning、density/hazard summary の表示がある。UAV/RTV、barrier、RAW/WAR の実 GPU 判定は未確認。 | 部分実装／確認待ち |

### Phase 1 判定

表示用の pipeline/resource summary は実装済みだが、完全な DAG/lifetime/hazard 収集は未完了。Phase 1 は「部分実装／実行確認待ち」とする。
