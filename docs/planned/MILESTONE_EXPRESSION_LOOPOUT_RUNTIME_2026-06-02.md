# M-MOTION-3 Expression loopOut / loopIn Runtime (2026-06-02)

日付：2026-06-02
目標：Expression エディタの Copilot で提案される `loopOut("cycle")` などループ関数を、ExpressionEvaluator で実際に実行可能にする。

---

## Goal

- `loopOut(type)`, `loopIn(type)`, `loopOutDuration(type, duration)`, `loopInDuration(type, duration)` の4関数を Expression ランタイムで実装
- ループタイプ: `cycle`, `pingpong`, `continue`, `offset` をサポート
- キーフレームが2つ以上ある場合にループが正しく動作する

---

## Definition of Done

- [ ] **loopOut("cycle")** - 最後のキーフレームから最初のキーフレームに戻って繰り返す
- [ ] **loopOut("pingpong")** - 最後→最初→最後の往復
- [ ] **loopOut("continue")** - 最後の速度を維持して延長（定速外挿）
- [ ] **loopOut("offset")** - 最初と最後の差分を加算しながら繰り返す
- [ ] **loopIn** 系 - 上記4タイプを開始側に適用
- [ ] **loopOutDuration / loopInDuration** - 時間幅指定版
- [ ] キーフレームが1つだけの場合 → その値を維持（エラーにしない）
- [ ] キーフレームが0の場合 → `value` プロパティ値をそのまま返す
- [ ] タイムリマップと併用した場合の正しい動作

---

## Analysis: 現状確認

- `ArtifactExpressionCopilotWidget` の UI で `loopOut("cycle")` を提案するボタンは存在する
- しかし `ExpressionEvaluator` に `loopOut` の実装がない（ソースコード検索で未確認）
- ユーザーが式を書いてもループ関数が未定義エラーになる、または何も返さない

---

## Implementation Phases

### Phase 1: ループ関数コア

**ファイル**: `ArtifactCore/src/Script/Expression/ExpressionLoopFunctions.cppm` (修正/追加)

**完了条件**:
- [ ] Expression ランタイム組み込み関数として `loopOut` を登録
- [ ] プロパティのキーフレームリストにアクセスするための `KeyframeAccess` ユーティリティ
- [ ] 4つのループモード（cycle / pingpong / continue / offset）の計算ロジック

```cpp
namespace ArtifactCore::Expression {

enum class LoopMode { Cycle, PingPong, Continue, Offset };

QVariant computeLoopOut(
    const std::vector<KeyFrame>& keyframes,
    int64_t currentFrame, int64_t totalFrames, LoopMode mode);

void registerLoopFunctions(ExpressionEvaluator& evaluator);
}
```

### Phase 2: ExpressionEvaluator 統合

**ファイル**: `ArtifactCore/src/Script/Expression/ExpressionEvaluator.cppm`

**完了条件**:
- [ ] `ExpressionEvaluator::registerBuiltins()` でループ関数を登録
- [ ] ループ関数から現在のプロパティのキーフレームリストにアクセスできるよう `PropertyContext` を拡張
- [ ] `loopOut` / `loopIn` が Expression 内で `double loopOut(string mode)` として呼び出し可能に

### Phase 3: テスト

**完了条件**:
- [ ] `loopOut("cycle")`: 3キーフレーム (0, 50, 100) で frames 150 の値が frame 50 と同じになる
- [ ] `loopOut("pingpong")`: frames 150 の値が frame 50 と同じになる（往復）
- [ ] `loopOut("continue")`: 最後の速度ベクトルで外挿
- [ ] `loopOut("offset")`: 1周期ごとに差分が累積
- [ ] キーフレーム1つのエッジケース

---

## Dependencies

- ExpressionEvaluator (Script.Expression.Evaluator)
- AbstractProperty (キーフレームリスト取得)
- KeyFrame / AnimatableValue

---

## Total Estimate

| Phase | 時間 |
|---|---|
| Phase 1: ループ関数コア | 4-6h |
| Phase 2: ExpressionEvaluator 統合 | 2-4h |
| Phase 3: テスト | 2-3h |
| **合計** | **8-13h** |