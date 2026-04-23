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
