# Phase 2 実行メモ: Pass / Resource Inspector

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md) の Phase 2 を、`1 フレーム` の中身を pass / resource / attachment 単位で読めるようにするための実行メモ。

この段階では compare や export よりも先に、何が描画されたのかを追える検査面を作る。

---

## 方針

1. まずは read-only
2. pass / resource / attachment を分けて表示する
3. frame cache があるなら補助情報として使う
4. 失敗時に「どこまで見えていたか」が分かるようにする

---

## 現状の土台

- [`Artifact/include/Render/ArtifactRenderQueueService.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderQueueService.ixx)
- [`Artifact/include/Render/ArtifactFrameCache.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactFrameCache.ixx)
- [`Artifact/include/Render/ArtifactIRenderer.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactIRenderer.ixx)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm)

---

## 実装タスク

### 1. Pass Record を固める

追加候補:

- `FrameDebugPassRecord`
- `FrameDebugPassStatus`
- `FrameDebugPassKind`

責務:

- pass の順序を保持する
- clear / draw / resolve / readback を分ける
- pass ごとの成功 / 失敗 / skipped を読めるようにする

### 2. Resource Record を固める

追加候補:

- `FrameDebugResourceRecord`
- `FrameDebugAttachmentRecord`
- `FrameDebugTextureRef`
- `FrameDebugBufferRef`

責務:

- texture / buffer / surface を参照として保持する
- source / intermediate / output を分ける
- cache hit / miss / stale を表せるようにする

### 3. Frame Cache を inspector に接続する

候補ファイル:

- [`Artifact/include/Render/ArtifactFrameCache.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactFrameCache.ixx)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)

やること:

- cached frame range を pass inspector の補助情報にする
- cache miss / stale を frame summary に反映する
- cache がない場合は空表示に自然フォールバックする

### 4. 既存 UI に read-only inspector を載せる

候補ファイル:

- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)

やること:

- pass 名と resource 要約を text で表示する
- 最初は summary だけでよい
- 将来の専用 widget へ流用しやすいデータ形にする

---

## 実装順

1. `Artifact/include/Render/ArtifactRenderQueueService.ixx`
2. `Artifact/include/Render/ArtifactIRenderer.ixx`
3. `Artifact/include/Render/ArtifactFrameCache.ixx`
4. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
5. `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
6. `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

---

## Work Tickets

### P2-T1 Pass Record Model

対象:

- `ArtifactCore/include/Frame/FrameDebug.ixx` 新規

内容:

- `FrameDebugPassRecord` / `FrameDebugPassStatus` / `FrameDebugPassKind` を追加する
- pass の順序と結果を保持する
- clear / draw / resolve / readback を区別する

完了条件:

- pass 単位で状態を表せる

### P2-T2 Resource Record Model

対象:

- `ArtifactCore/include/Frame/FrameDebug.ixx` 新規

内容:

- `FrameDebugResourceRecord` / `FrameDebugAttachmentRecord` / `FrameDebugTextureRef` / `FrameDebugBufferRef` を追加する
- source / intermediate / output を分ける
- cache hit / miss / stale を表せるようにする

完了条件:

- resource 単位で参照を読める

### P2-T3 Frame Cache Bridge

対象:

- `Artifact/include/Render/ArtifactFrameCache.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

内容:

- cached frame range を inspector に渡す
- cache miss / stale を frame summary に反映する
- cache なしの場合の空表示を定義する

完了条件:

- frame cache が inspector の補助情報として見える

### P2-T4 Read-only Inspector Surface

対象:

- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

内容:

- pass 名 / resource 要約 / attachment 要約を text で出す
- summary の first cut を作る
- 将来の専用 widget へ移植しやすくする

完了条件:

- 既存 UI から pass / resource summary を読める

---

## 完了条件

- pass sequence が読める
- resource / attachment の関係が読める
- cache hit / miss / stale が分かる
- failed frame の要点を inspector で追える
- 既存の render 結果や playback 挙動は変わらない

---

## 変更しないこと

- 既存の render path の順序
- 既存の readback や cache アルゴリズム
- 既存 UI の操作感

---

## リスク

- resource を全部出すと inspector が長くなりすぎる
- pass の粒度を間違えると原因追跡しにくい
- frame cache と inspector の責務を混ぜると後で切りにくい

---

## 次の Phase への橋渡し

Phase 2 が終わると、Phase 3 で compare / scrub / step を入れたときに「何を比べるか」が明確になる。

---

## 関連文書

- [`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md)
- [`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE1_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE1_EXECUTION_2026-04-20.md)
- [`docs/planned/MILESTONES_BACKLOG.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)
