# Milestone: リアクティブイベントシステム (2026-03-28)

**Status:** Phase 1 完了 (トリガーイベント定義 Core 実装済み)
**Goal:** オブジェクト間の接触/条件を検出し、リアクティブなアニメーションを発生させる。

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

### 統合ポイント

| 用途 | 既存 API | 場所 |
|------|---------|------|
| BB 取得 | `layer->transformedBoundingBox()` → `QRectF` | `ArtifactAbstractLayer:128` |
| 全レイヤー列挙 | `composition->allLayer()` | `ArtifactAbstractComposition:106` |
| プロパティ変更 | `layer->setLayerPropertyValue("opacity", 0.0)` | 全レイヤータイプ |
| 位置取得/設定 | `layer->position3D()` / `setPosition3D()` | `ArtifactAbstractLayer:135-136` |
| アクティブ判定 | `layer->isActiveAt(frame)` | `ArtifactAbstractLayer:159` |
| 更新ループ | `composition->goToFrame()` → `changed()` | コンポジション更新 |
| 永続化 | `toJson()` / `fromJson()` | プロジェクト保存 |

### 評価タイミング

```
Composition::update(float dt):
  goToFrame(currentFrame)           // キーフレーム評価
  reactiveEngine->evaluate(frame, dt) // ← ここでリアクティブ評価
  Q_EMIT changed()                  // UI 更新
```

### 状態管理 (OnEnterRange/OnExitRange/OnSeparation)

これらは**前フレームの状態を記憶**する必要がある:

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
evaluate(frame, deltaTime):
  // 1. 状態更新
  updateLayerStates(frame)

  // 2. Lifecycle トリガー評価
  for layer in composition->allLayer():
    active = layer->isActiveAt(frame)
    if active && !prevActive: fire(OnEnterRange, layer)
    if !active && prevActive: fire(OnExitRange, layer)
    if active && frame == layer->inPoint(): fire(OnStart, layer)
    if active && frame == layer->outPoint(): fire(OnEnd, layer)

  // 3. Spatial トリガー評価 (全ペア)
  for each rule with Spatial trigger:
    bbA = layerA->transformedBoundingBox()
    bbB = layerB->transformedBoundingBox()
    contacting = bbA.intersects(bbB)
    if rule.type == OnContact and contacting and !wasContacting: fire()
    if rule.type == OnSeparation and !contacting and wasContacting: fire()
    if rule.type == OnProximity and distance(bbA, bbB) < threshold: fire()

  // 4. Value トリガー評価
  for each rule with Value trigger:
    value = layer->getLayerPropertyValue(propertyPath)
    if rule.type == OnValueExceed and value > threshold: fire()
    if rule.type == OnValueDrop and value < threshold: fire()

  // 5. Frame トリガー評価
  for each rule with Frame trigger:
    if frame == rule.trigger.frameNumber: fire()

  // 6. 遅延リアクション処理
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
      case SetProperty:
        if reaction.duration > 0:
          // アニメーション付き補間
          startValue = target->getLayerPropertyValue(reaction.propertyPath)
          animatedValue = interpolate(startValue, reaction.value,
                                      elapsed/duration, reaction.easing)
          target->setLayerPropertyValue(reaction.propertyPath, animatedValue)
        else:
          target->setLayerPropertyValue(reaction.propertyPath, reaction.value)

      case AnimateProperty:
        // Linearly interpolate from current to target over duration
        // Uses easing function from Interpolate.ixx

      case RandomizeProperty:
        randomValue = lerp(reaction.value, reaction.valueMax, rand())
        target->setLayerPropertyValue(reaction.propertyPath, randomValue)

      case ApplyForce/ApplyImpulse/Attract/Repel:
        // パーティクルレイヤーへの力場追加
        if auto particle = dynamic_cast<ArtifactParticleLayer*>(target):
          particle->addForceField(reaction.strength, direction)

      case PlayAnimation/PauseAnimation/GoToFrame:
        target->goToFrame(reaction.targetFrame)
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
