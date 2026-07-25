# M-IR-9 Render Boundary Safety Gate

`ImmediateContext` / render target / particle / startup worker の境界変更を、描画崩れを起こしにくい順序で進めるための安全ゲート。

## Goal

- いまの描画を壊しやすい変更を先送りできる状態にする
- 次に戻るときの判断基準を 1 枚にまとめる
- `ImmediateContext` 境界整理、particle stabilizing、startup thread churn trace を「今は置いておく」判断に耐える形で残す

## What This Milestone Means

- 今すぐ大きく手を入れない
- 代わりに、追跡しやすい順序と危険点を固定する
- 描画を触る前に `Trace` / `FrameDebug` / summary API を先に増やす

## Safety Rules

- `CompositionRenderController` に新しい direct `ctx->...` を足さない
- widget 側で `IDeviceContext` を増やさない
- particle の draw entry と render target の戻し処理を同時にいじらない
- startup の worker burst と render boundary の変更を同じ commit に入れない

## Deferred Workstreams

- `M-IR-8 ImmediateContext Boundary / De-direct`
- `M-DIAG-5 Startup Thread Churn / Worker Burst Trace`
- `Particle Render Path Stabilization`
- `M-IR-10 ArtifactIRenderer 2D Primitive Expansion`

## Return Order

1. `Trace` / `FrameDebug` の summary を先に整える
2. `ArtifactIRenderer` の façade API を足す
3. `CompositionRenderController` の call site を 1 本ずつ置換する
4. particle draw path を helper 化する
5. 最後に `immediateContext()` 依存を縮める

## Done Criteria

- 変更を再開するときの順番が決まっている
- 壊れやすいポイントが checklist 化されている
- いまは置いておく判断をしても、戻り先が失われない

## 2026-05-31 Note

- `Arc / Rounded Rect / Styled Polyline` の追加は renderer façade の拡張としては有望だが、`CompositionRenderController` から low-level call site を増やす形では入れない
- まず `ArtifactIRenderer` / `PrimitiveRenderer2D` に閉じた実装面として用意し、その後 shape workflow から採用する順を守る

## Static Audit (2026-07-25)

安全ゲートの記録基盤はかなり進んでいる。`ArtifactIRenderer` は `PrimitiveRenderer2D`、particle renderer、FrameDebug pass/resource summary、RT diagnostics を所有し、粒子描画には empty／device-null／invalid-viewport／no-RTV の skip と診断ログがある。`CompositionRenderController` には render crash trace、FrameDebug snapshot、not-ready／hidden／in-progress の skip 記録があり、renderer façade 側に描画責務を集約する方向は確認できる。

一方、`ArtifactIRenderer` には依然として `immediateContext()` の公開APIと複数の low-level context操作が残り、`CompositionRenderController` にも immediate context 取得箇所が存在する。従って M-IR-8 の de-direct、全call siteの一括移行、particle draw helper化、startup worker churn trace、各backend／render-target復帰の実行確認は未完了。これは実装完了ではなく、危険な変更を再開する順序と観測点が整った「安全ゲート準備済み」と判定する。

確認対象:

- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `ArtifactCore/include/Frame/FrameDebug.ixx`
- `ArtifactCore/include/Diagnostics/Trace.ixx`
