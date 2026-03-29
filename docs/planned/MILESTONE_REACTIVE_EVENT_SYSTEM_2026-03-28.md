# Milestone: リアクティブイベントシステム (2026-03-28)

**Status:** Not Started
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

## Architecture

```
ReactiveEventEngine (Core)
  ├── CollisionDetector             ← BB 衝突検出
  │     ├── checkContact(A, B) → bool
  │     ├── checkOverlap(A, B) → float
  │     └── checkProximity(A, B, distance) → bool
  └── ReactiveRule[]
        ├── trigger: TriggerCondition
        │     ├── ContactTrigger      (A が B に接触)
        │     ├── ProximityTrigger    (A と B が距離以内)
        │     ├── ValueTrigger        (プロパティが閾値を超えた)
        │     └── FrameTrigger        (特定フレームに到達)
        ├── reactions: Reaction[]
        │     ├── SetPropertyReaction (プロパティ変更)
        │     ├── ApplyForceReaction  (パーティクルに力)
        │     ├── TriggerAnimation    (アニメーション開始)
        │     └── SpawnReaction       (オブジェクト生成)
        └── options: {once, repeating, delay, cooldown}
```

---

## Milestone 1: 衝突検出エンジン

### Implementation
- `CollisionDetector` クラス作成
- `layer->getGlobalTransform()` で BB 計算
- 接触/重なり/距離判定
- 空間ハッシュグリッド (Broad phase)

### 見積: 10h

---

## Milestone 2: リアクティブイベントエンジン

### Implementation
- `ReactiveEventEngine` クラス
- TriggerCondition 評価 (Contact/Proximity/Value/Frame)
- Reaction 実行 (SetProperty/ApplyForce)
- オプション (once/repeating/delay/cooldown)

### 見積: 13h

---

## Milestone 3: SetProperty リアクション

### Implementation
- 値アニメーション (補間付き)
- プロパティマッピング (opacity/position/scale/rotation/fillColor)
- 複合リアクション
- イージング

### 見積: 8h

---

## Milestone 4: ApplyForce リアクション

### Implementation
- Impulse / Continuous / Attract / Repel
- パーティクルへの力場追加
- 方向 (接触法線 or ベクトル)

### 見積: 6h

---

## Milestone 5: Inspector UI

### Implementation
- ルールエディタ (条件/リアクション設定)
- ビジュアルフィードバック
- プリセット

### 見積: 8h

---

## 使用例

### 例1: 接触でフェードアウト
```
Trigger: LayerA が LayerB に接触
Reaction: LayerB.opacity = 0 (duration: 0.5s, easeOut)
Options: once = true
```

### 例2: 近づいたら色が変わる
```
Trigger: LayerA と LayerB の距離 < 100px
Reaction: LayerB.fillColor = #FF0000 (duration: 0.2s)
Options: repeating = true
```

### 例3: 衝突でパーティクルを跳ね返す
```
Trigger: ParticleLayer が LayerB に接触
Reaction: ApplyForce(Repel, strength: 50, direction: contactNormal)
```

### 例4: フレーム到達でスケールアニメーション開始
```
Trigger: Frame 60 に到達
Reaction: LayerA.scale = 1.5 (duration: 1.0s, easeInOut)
```

---

## Deliverables

| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/Reactive/CollisionDetector.ixx` | 衝突検出エンジン |
| `ArtifactCore/src/Reactive/CollisionDetector.cppm` | 実装 |
| `ArtifactCore/include/Reactive/ReactiveEventEngine.ixx` | イベントエンジン |
| `ArtifactCore/src/Reactive/ReactiveEventEngine.cppm` | 実装 |
| `ArtifactCore/include/Reactive/ReactiveRule.ixx` | ルール/トリガー/リアクション構造体 |
| `Artifact/src/Widgets/ReactiveRuleEditor.cppm` | UI |

**総見積: ~45h**
