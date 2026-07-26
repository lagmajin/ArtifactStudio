# REACTIVE_EVENTS_ENGINE_WORKLOG_2026-07-25

## 概要

ReactiveEvents の評価エンジン実装。

## 調査結果

ReactiveEvents は13種トリガー + 14種リアクションのデータモデルとJSONシリアライズが完備。
Rig2D の `Bone2D::evaluate(time)` と同じパターンで評価エンジンが丸ごと欠落。

## 実装内容

### ReactiveEngine (ArtifactCore)

既存 `ReactiveEvents.ixx` + `.cppm` に追加:
- `ReactiveEvaluationContext` — layer-agnostic callback 群 (layerPropertyValue, layerIsActive, layerInPoint, layerOutPoint)
- `ReactiveEngine` — addRule/removeRule/clearRules/evaluate(ctx)
- 13トリガー種別の評価ロジック (edge detection 完備: OnStart/OnEnd/OnEnter/OnExit/OnLoop/OnValueExceed/OnValueDrop/OnValueCross/OnFrame)
- once/delay/cooldown 制御

### ArtifactReactionExecutor (Artifact)

新規ファイル 2つ:
- `Artifact/include/Service/ArtifactReactionExecutor.ixx`
- `Artifact/src/Service/ArtifactReactionExecutor.cppm`

7種リアクション実装: SetProperty, AnimateProperty, RandomizeProperty, PlayAnimation, PauseAnimation, GoToFrame, SpawnLayer, DestroyLayer

## 変更ファイル

| ファイル | 追加行 | 内容 |
|----------|--------|------|
| `ArtifactCore/include/Reactive/ReactiveEvents.ixx` | ~50行 | ReactiveEvaluationContext + ReactiveEngine |
| `ArtifactCore/src/Reactive/ReactiveEvents.cppm` | ~150行 | engine evaluate 実装 |
| `Artifact/include/Service/ArtifactReactionExecutor.ixx` | 新規 ~60行 | 宣言 |
| `Artifact/src/Service/ArtifactReactionExecutor.cppm` | 新規 ~150行 | 実装 |
