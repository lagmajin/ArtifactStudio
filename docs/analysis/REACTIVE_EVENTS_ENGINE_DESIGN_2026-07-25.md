# ReactiveEvents Engine Design

**作成日:** 2026-07-25

## 現状

`ReactiveEvents` モジュールには13種トリガー + 14種リアクション + ルール制御の完全なデータモデルとJSONシリアライズが揃っている。評価エンジンとリアクション実行部分が欠落。

次に切るなら、この欠落部分を最小経路で閉じるのが自然。まず `evaluate()` でトリガーを出し、その結果を `ArtifactReactionExecutor` へ渡すところまでを第一段とする。

## アーキテクチャ

```
毎フレーム goToFrame() 内:
  ReactiveEngine::evaluate(frame, dt, ctx)
    └─ edge detection + 条件判定 → delay/cooldown制御
    └─ TriggerEvent 出力 → EventBus::publish<TriggerEvent>
          └─ ArtifactReactionExecutor (subscribe)
                └─ execute(rule, event)
```

## ReactiveEngine (ArtifactCore)

`ReactiveEvaluationContext` — layer 型に依存しない callback 群:

```cpp
struct ReactiveEvaluationContext {
    int64_t currentFrame;
    float deltaTime;
    std::function<QVariant(QString,QString)> layerPropertyValue; // (layerId, path) → value
    std::function<bool(QString)> layerIsActive;                   // layerId → active?
    std::function<int64_t(QString)> layerInPoint;                // layerId → inPoint フレーム
    std::function<int64_t(QString)> layerOutPoint;               // layerId → outPoint フレーム
};
```

`ReactiveEngine` — ルール管理 + 毎フレーム evaluate:

- addRule / removeRule / clearRules / rules
- evaluate(frame, dt, ctx) → vector<TriggerEvent>
- 内部で前フレーム状態保存 (prevActive, prevValues)
- 各ルールの実行状態管理 (fired, fireAccumulator, cooldownRemaining)

## ファイル変更

| File | 変更 |
|------|------|
| `ArtifactCore/include/Reactive/ReactiveEvents.ixx` | ReactiveEvaluationContext + ReactiveEngine 追加 |
| `ArtifactCore/src/Reactive/ReactiveEvents.cppm` | engine 実装 |
| `Artifact/include/Service/ArtifactReactionExecutor.ixx` | 新規 |
| `Artifact/src/Service/ArtifactReactionExecutor.cppm` | 新規 |
