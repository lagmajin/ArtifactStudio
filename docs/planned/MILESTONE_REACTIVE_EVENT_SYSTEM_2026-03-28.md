# Milestone: リアクティブイベントシステム (2026-03-28)

**Status:** Phase 1 完了 (トリガーイベント定義 Core 実装済み)
**Goal:** オブジェクト間の接触/条件を検出し、リアクティブなアニメーションを発生させる。

---

## 仕様概要

このマイルストーンの実行契約は次を基準にする。

- イベントは即時発火せず、**フレーム末に一括処理**するキュー方式とする
- **レイヤーのキーフレームは一切触らない**
- イベントは `PropertyOverlay` として値に乗算・加算するだけに留める
- エフェクトは `EventBus` を直接知らず、受け取ったイベントを契機に内部状態を進める
- 登録単位はレイヤー単体ではなく `ContactSubscription { triggerLayer, reactorLayer }` とする
- 1 つのトリガーに対して複数の `Reaction` を登録できる

旧来の `OnStart / OnContact / OnFrame` という名前は、既存の設計メモや UI ラベルに残る場合があるが、ランタイム契約としては下のイベント一覧を優先する。

---

## コンセプト

```
条件: LayerA の bounding box が LayerB の bounding box と接触
リアクション: LayerB の opacity を 0 にする
     OR: LayerB の色を変える
     OR: LayerB に力を加える (パーティクル方向)
     OR: LayerB のアニメーションをトリガー
```

---

## 実装済み

| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/Reactive/ReactiveEvents.ixx` | 全型定義 (TriggerEventType 13種, ReactionType 14種, TriggerCondition, Reaction, ReactiveRule, TriggerEvent) |
| `ArtifactCore/src/Reactive/ReactiveEvents.cppm` | JSON シリアライズ (toJson/fromJson), 文字列変換 |

---

## 詳細設計

### Core vs App 層の分離

| 層 | クラス | 状態 | 理由 |
|---|--------|------|------|
| **ArtifactCore** | `ReactiveEvents.ixx` (型定義) | ✅ 完了 | データ定義 + 永続化。レイヤー非依存 |
| **ArtifactCore** | `CollisionDetector` | ⬜ 未実装 | BB 計算は `QRectF` のみ。レイヤー非依存 |
| **Artifact** | `ReactiveEventEngine` | ⬜ 未実装 | `layer->setLayerPropertyValue()` を呼ぶため App 層 |
| **Artifact** | `ReactionExecutor` | ⬜ 未実装 | 各リアクションの実行ロジック |

### アーキテクチャ方針

- `EventBus` は登録済みの `TriggerPair` のみ処理する
- `ContactSubscription` は `triggerLayer` と `reactorLayer` の 2 要素で構成する
- Contact 系は `layerA / layerB / point / normal` を持つ
- Layer 系は `layerId` 単位で扱う
- Effect 系は `effectId` 単位で完結する
- `Reaction` は値の上書きではなく、`PropertyOverlay` を介した合成を基本とする
- `PropertyOverlay` は keyframed value に対する加算・乗算の入力として評価する
- `TimelineReaction` は再生ヘッド制御を行うが、タイムラインのキーフレーム定義自体は変更しない
- `TriggerReaction` は別レイヤーの開始・停止チェーンを制御する
- パーティクル系は `ContactBegin` を受けてバーストし、`ParticleSystemFinished` だけを終端イベントとして扱う

### イベント一覧

#### 接触系

| イベント | 内容 |
|---|---|
| `ContactBegin { layerA, layerB, point, normal }` | 接触開始 |
| `ContactEnd { layerA, layerB }` | 接触終了 |

#### レイヤー系

| イベント | 内容 |
|---|---|
| `LayerAnimationFinished { layerId }` | レイヤーアニメーション完了 |
| `LayerKeyframeReached { layerId, keyframeIndex, label }` | 特定キーフレーム到達 |
| `LayerOutPointReached { layerId }` | out point 到達 |

#### エフェクト系

| イベント | 内容 |
|---|---|
| `ParticleSystemFinished { effectId }` | パーティクルシステム終了 |

### トリガーイベント体系 (13種)

| カテゴリ | トリガー | 用途 |
|---------|---------|------|
| **Lifecycle** | `OnStart` | レイヤーの再生開始 (inPoint 到達) |
| | `OnEnd` | レイヤーの再生終了 (outPoint 到達) |
| | `OnEnterRange` | アクティブ範囲に入った |
| | `OnExitRange` | アクティブ範囲から出た |
| | `OnLoop` | ループ再生でサイクル繰り返し |
| **Spatial** | `OnContact` | BB が接触した |
| | `OnSeparation` | 接触 → 非接触 |
| | `OnProximity` | 距離が閾値以内 |
| **Value** | `OnValueExceed` | プロパティ値が閾値超過 |
| | `OnValueDrop` | プロパティ値が閾値以下 |
| | `OnValueCross` | プロパティ値が閾値を横切った |
| **Frame** | `OnFrame` | 特定フレーム到達 |

### リアクション体系 (14種)

| カテゴリ | リアクション |
|---------|------------|
| Property | `SetProperty` / `AnimateProperty` / `RandomizeProperty` |
| Force | `ApplyImpulse` / `ApplyForce` / `Attract` / `Repel` |
| Playback | `PlayAnimation` / `PauseAnimation` / `GoToFrame` |
| Spawn | `SpawnLayer` / `DestroyLayer` |
| Audio | `PlaySound` |

### Reaction 種別

| 種別 | 実装用途 | 振る舞い |
|---|---|---|
| `PropertyReaction` | `PropertyOverlay` を生成して Property に渡す | `opacity` / `color` などの値変化 |
| `PhysicsReaction` | `tick()` で継続更新 | 力・速度・加速度の継続制御 |
| `TimelineReaction` | 再生ヘッドを制御 | アニメ開始・シーク・停止 |
| `TriggerReaction` | 別レイヤーの開始・停止チェーン | 連鎖トリガーを構成 |

### PropertyOverlay

```cpp
struct PropertyOverlay {
    float    startTime;    // 接触した絶対時刻
    float    duration;     // 変化時間
    QVariant targetValue;  // 目標値
    EasingFn easing;       // カーブ
};
```

`evaluate()` 内では、次のように合成する。

```cpp
QColor = keyframedValue + overlay.evaluate(currentTime);
```

### エフェクトの扱い

| 種別 | イベント発火 | イベント受信 |
|---|---|---|
| ビジュアル系（ブラー・グロー・色収差） | なし | なし |
| パーティクル系 | `ParticleSystemFinished` のみ | `ContactBegin` を受信してバースト |

### 統合ポイント

| 用途 | 既存 API | 場所 |
|------|---------|------|
| BB 取得 | `layer->transformedBoundingBox()` → `QRectF` | `ArtifactAbstractLayer:128` |
| 全レイヤー列挙 | `composition->allLayer()` | `ArtifactAbstractComposition:106` |
| プロパティ変更 | `layer->setLayerPropertyValue("opacity", 0.0)` | 全レイヤータイプ |
| 位置取得/設定 | `layer->position3D()` / `setPosition3D()` | `ArtifactAbstractLayer:135-136` |
| アクティブ判定 | `layer->isActiveAt(frame)` | `ArtifactAbstractLayer:159` |
| 更新ループ | `composition->goToFrame()` → `changed()` | コンポジション更新 |
| リアクティブ評価 | フレーム末キューで `evaluate()` | App 層 |
| 永続化 | `toJson()` / `fromJson()` | プロジェクト保存 |

### 評価タイミング

```
Composition::update(float dt):
  goToFrame(currentFrame)           // キーフレーム評価
  reactiveEngine->queue(frame, dt)    // 即時発火しない
  reactiveEngine->evaluateQueued()     // フレーム末にまとめて評価
  Q_EMIT changed()                  // UI 更新
```

### 状態管理

接触系とレイヤー系は**前フレームの状態を記憶**する必要がある。

```cpp
// ReactiveEventEngine 内部状態
struct LayerState {
    bool wasActive = false;          // 前フレームでアクティブだったか
    bool wasContacting = false;      // 前フレームで接触していたか
};

std::map<QString, LayerState> layerStates_;

evaluate(frame, dt):
  for each layer:
    currentActive = layer->isActiveAt(frame)
    if currentActive && !layerStates_[id].wasActive:
      // OnEnterRange 発火
    if !currentActive && layerStates_[id].wasActive:
      // OnExitRange 発火
    layerStates_[id].wasActive = currentActive
```

### ReactiveEventEngine の評価フロー

```
queue(frame, deltaTime):
  pendingFrames.push_back({frame, deltaTime})

evaluateQueued():
  for queued in pendingFrames:
    evaluateFrame(queued.frame, queued.deltaTime)
  pendingFrames.clear()

evaluateFrame(frame, deltaTime):
  // 1. 状態更新
  updateLayerStates(frame)

  // 2. Layer イベント評価
  for layer in composition->allLayer():
    if frame == layer->outPoint(): fire(LayerOutPointReached, layer)

  // 3. Contact イベント評価
  for each registered ContactSubscription:
    bbA = triggerLayer->transformedBoundingBox()
    bbB = reactorLayer->transformedBoundingBox()
    if contactBegin(bbA, bbB): fire(ContactBegin, triggerLayer, reactorLayer)
    if contactEnd(bbA, bbB): fire(ContactEnd, triggerLayer, reactorLayer)

  // 4. Reaction キュー処理
  for pending in scheduledReactions:
    if pending.fireTime <= now: execute(pending)
```

### Reaction 実行ロジック

```
executeReactions(rule, frame, deltaTime):
  for reaction in rule.reactions:
    target = composition->layerById(reaction.targetLayerId)
    if !target: continue

    switch(reaction.type):
      case PropertyReaction:
        target->pushPropertyOverlay(reaction.propertyPath, overlay)

      case PhysicsReaction:
        target->tickReaction(reaction, deltaTime)

      case TimelineReaction:
        target->goToFrame(reaction.targetFrame)

      case TriggerReaction:
        target->trigger(reaction.targetLayerId)
```

### データ構造 (Core 層)

```cpp
// Trigger types (13種 — ReactiveEvents.ixx で定義済み)
enum class TriggerEventType {
    None, OnStart, OnEnd, OnEnterRange, OnExitRange, OnLoop,
    OnContact, OnSeparation, OnProximity,
    OnValueExceed, OnValueDrop, OnValueCross, OnFrame
};

// Reaction types (14種 — ReactiveEvents.ixx で定義済み)
enum class ReactionType {
    None, SetProperty, AnimateProperty, RandomizeProperty,
    ApplyImpulse, ApplyForce, Attract, Repel,
    PlayAnimation, PauseAnimation, GoToFrame,
    SpawnLayer, DestroyLayer, PlaySound
};

// Trigger condition
struct TriggerCondition {
    TriggerEventType type = TriggerEventType::None;
    QString sourceLayerId;         // ソースレイヤー ID
    QString targetLayerId;         // ターゲットレイヤー ID
    float proximityThreshold = 50.0f;
    QString propertyPath;
    float valueThreshold = 0.0f;
    int64_t frameNumber = 0;
};

// Reaction action
struct Reaction {
    ReactionType type = ReactionType::None;
    QString targetLayerId;
    QString propertyPath;
    QVariant value;
    QVariant valueMax;             // Randomize 用
    float duration = 0.0f;
    QString easing;                // "easeIn", "easeOut", "easeInOut", "bounce", "elastic"
    float strength = 1.0f;
    float directionX = 0.0f;
    float directionY = 0.0f;
    int64_t targetFrame = 0;
    QString spawnLayerType;
};

// Rule
struct ReactiveRule {
    QString id;
    QString name;
    bool enabled = true;
    TriggerCondition trigger;
    std::vector<Reaction> reactions;
    bool once = false;
    bool repeating = false;
    float delay = 0.0f;
    float cooldown = 0.0f;
    // 実行状態 (永続化しない)
    bool fired = false;
    int64_t lastFiredFrame = -1;
    float fireAccumulator = 0.0f;
};
```

### 永続化 (JSON)

```json
{
  "reactiveEvents": [
    {
      "id": "rule-001",
      "name": "接触でフェードアウト",
      "trigger": {
        "type": "OnContact",
        "sourceLayerId": "layer-id-a",
        "targetLayerId": "layer-id-b"
      },
      "reactions": [
        {
          "type": "AnimateProperty",
          "targetLayerId": "layer-id-b",
          "propertyPath": "opacity",
          "value": 0.0,
          "duration": 0.5,
          "easing": "easeOut"
        }
      ],
      "once": true,
      "enabled": true
    },
    {
      "id": "rule-002",
      "name": "再生開始でスケールアップ",
      "trigger": {
        "type": "OnStart",
        "sourceLayerId": "layer-id-c"
      },
      "reactions": [
        {
          "type": "AnimateProperty",
          "targetLayerId": "layer-id-c",
          "propertyPath": "scaleX",
          "value": 1.5,
          "duration": 1.0,
          "easing": "easeInOut"
        }
      ]
    },
    {
      "id": "rule-003",
      "name": "Exit Range でフェードアウト",
      "trigger": {
        "type": "OnExitRange",
        "sourceLayerId": "layer-id-d"
      },
      "reactions": [
        {
          "type": "SetProperty",
          "targetLayerId": "layer-id-d",
          "propertyPath": "opacity",
          "value": 0.0
        }
      ]
    }
  ]
}
```

---

## Architecture

```
ReactiveEventEngine (Artifact)
  ├── LayerStateTracker              ← 前フレーム状態記憶
  │     ├── wasActive (per layer)
  │     ├── wasContacting (per pair)
  │     └── prevPropertyValue (per layer/property)
  ├── LifecycleEvaluator             ← OnStart/OnEnd/OnEnterRange/OnExitRange/OnLoop
  ├── CollisionDetector (Core)       ← BB 衝突検出
  │     ├── checkContact(A, B) → bool
  │     ├── checkOverlap(A, B) → float
  │     └── checkProximity(A, B, distance) → bool
  ├── SpatialEvaluator               ← OnContact/OnSeparation/OnProximity
  ├── ValueEvaluator                 ← OnValueExceed/OnValueDrop/OnValueCross
  ├── FrameEvaluator                 ← OnFrame
  ├── DelayScheduler                 ← 遅延リアクション管理
  └── ReactionExecutor               ← リアクション実行
        ├── SetPropertyExecutor
        ├── AnimatePropertyExecutor
        ├── RandomizePropertyExecutor
        ├── ForceExecutor
        └── PlaybackExecutor
```

---

## Milestone 1: Lifecycle トリガー (最もシンプル)

### Implementation
- `OnStart` / `OnEnd` / `OnEnterRange` / `OnExitRange` / `OnLoop` の評価
- `LayerStateTracker` で前フレーム状態を記憶
- `layer->isActiveAt()` / `layer->inPoint()` / `layer->outPoint()` を使用

### 見積: 6h

---

## Milestone 2: リアクティブイベントエンジン基盤

### Implementation
- `ReactiveEventEngine` クラス (App 層)
- 全トリガー評価ループ
- Delay / cooldown / once / repeating 管理
- `ReactionExecutor` 基盤

### 見積: 8h

---

## Milestone 3: 衝突検出エンジン + Spatial トリガー

### Implementation
- `CollisionDetector` クラス (Core 層)
- OnContact / OnSeparation / OnProximity 評価
- 空間ハッシュグリッド (Broad phase)

### 見積: 10h

---

## Milestone 4: SetProperty/AnimateProperty リアクション

### Implementation
- 値アニメーション (補間付き)
- プロパティマッピング (opacity/position/scale/rotation/fillColor)
- イージング (Interpolate.ixx を使用)

### 見積: 8h

---

## Milestone 5: ApplyForce リアクション

### Implementation
- Impulse / Continuous / Attract / Repel
- パーティクルへの力場追加

### 見積: 6h

---

## Milestone 6: Inspector UI

### Implementation
- ルールエディタ (条件/リアクション設定)
- ビジュアルフィードバック
- プリセット

### 見積: 8h

---

## 使用例

### 例1: OnStart でスケールアップ
```
Trigger: OnStart (source: LayerA)
Reaction: LayerA.scaleX = 1.5 (duration: 1.0s, easeInOut)
```

### 例2: OnContact でフェードアウト
```
Trigger: OnContact (source: LayerA, target: LayerB)
Reaction: AnimateProperty(LayerB.opacity = 0, duration: 0.5s, easeOut)
Options: once = true
```

### 例3: OnProximity で色が変わる
```
Trigger: OnProximity (source: LayerA, target: LayerB, threshold: 100px)
Reaction: SetProperty(LayerB.fillColor = #FF0000)
Options: repeating = true
```

### 例4: OnExitRange で非表示
```
Trigger: OnExitRange (source: LayerA)
Reaction: SetProperty(LayerA.opacity = 0.0)
```

### 例5: OnFrame でパーティクルに力
```
Trigger: OnFrame (frame: 60)
Reaction: ApplyForce(target: ParticleLayer, strength: 50, direction: (0, -1))
```

---

## Deliverables

| ファイル | 状態 | 内容 |
|---------|------|------|
| `ArtifactCore/include/Reactive/ReactiveEvents.ixx` | ✅ 完了 | 全型定義 |
| `ArtifactCore/src/Reactive/ReactiveEvents.cppm` | ✅ 完了 | JSON シリアライズ |
| `ArtifactCore/include/Reactive/CollisionDetector.ixx` | ⬜ 未実装 | 衝突検出エンジン |
| `ArtifactCore/src/Reactive/CollisionDetector.cppm` | ⬜ 未実装 | 実装 |
| `Artifact/src/Reactive/ReactiveEventEngine.ixx` | ⬜ 未実装 | イベントエンジン |
| `Artifact/src/Reactive/ReactiveEventEngine.cppm` | ⬜ 未実装 | 実装 |
| `Artifact/src/Widgets/ReactiveRuleEditor.cppm` | ⬜ 未実装 | UI |
| `Artifact/src/Widgets/ReactiveEventEditorWindow.cppm` | ⬜ 未実装 | 独立ウィンドウ UI |

**総見積: ~46h** (うち完了済み ~4h)

---

## UI Direction Note

- 最初は独立ウィンドウで始めるのが自然
- `Target Tree / Event Rules / Inspector / Event Log` を 1 画面にまとめる
- 後から dockable / inspector integration に拡張できる構造を前提にする
- 詳細なウィンドウ案は `docs/planned/MILESTONE_REACTIVE_EVENT_EDITOR_WINDOW_2026-03-29.md`

---

## 未決定・将来検討

- 接触判定のタイミング
  - 毎フレーム全ペア
  - dirty flag
  - QuadTree
- `TimelineReaction` の内部
  - クリップ再生
  - tween 動的生成
- `Sequence / Stagger DSL`
  - カード連鎖などの規則的なタイミング制御
