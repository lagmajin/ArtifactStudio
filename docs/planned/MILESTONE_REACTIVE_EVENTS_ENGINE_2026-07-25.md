# MILESTONE_REACTIVE_EVENTS_ENGINE_2026-07-25

**ステータス:** ✅ Complete (4/4)
**対象:** `ArtifactCore/include/Reactive/ReactiveEvents.ixx`, `ArtifactCore/src/Reactive/ReactiveEvents.cppm`, `Artifact/include/Service/ArtifactReactionExecutor.ixx`, `Artifact/src/Service/ArtifactReactionExecutor.cppm`
**位置づけ:** After Effects のイベント駆動システムに相当する ReactiveEvents の評価エンジンを実装。
**作成日:** 2026-07-25

## 1. 目的

ReactiveEvents モジュールは13種トリガー + 14種リアクションのデータモデルとJSONシリアライズは完備しているが、**評価エンジンが丸ごと欠落**していた。フレームごとにルールを評価し、条件成立時にリアクションを実行するパイプラインを構築する。

## 2. 現状 (2026-07-25)

| 要素 | 状態 |
|------|------|
| TriggerCondition / Reaction / ReactiveRule データ構造 | ✅ 完備 |
| JSON シリアライズ | ✅ 完備 |
| 13トリガー種別の評価ロジック | ❌ 不在 |
| edge detection (前フレーム比較) | ❌ 不在 |
| once/delay/cooldown 制御 | ❌ ルールのデータはあるがエンジンが未実装 |
| リアクション実行 (SetProperty, GoToFrame, SpawnLayer等) | ❌ 不在 |
| レイヤーシステムとの結合 | ❌ 不在 |

## 3. 実装内容

### 3.1 ReactiveEvaluationContext

レイヤーシステムの型に依存しない callback 群を定義:

- `layerPropertyValue(layerId, propertyPath)` → 現在のプロパティ値
- `layerIsActive(layerId)` → レイヤーがアクティブ範囲内か
- `layerInPoint(layerId)` → inPoint フレーム
- `layerOutPoint(layerId)` → outPoint フレーム

### 3.2 ReactiveEngine (ArtifactCore)

評価エンジン — ルール管理 + 毎フレーム evaluate:

- `addRule` / `removeRule` / `clearRules` / `rules` / `ruleCount`
- `evaluate(ctx)` → 全ルール評価、条件成立時は `TriggerEvent` リストを返す
- 内部状態: 前フレーム比較用の `prevActive_`, `prevValues_`, `prevFrame_`
- ルール制御: once (fired フラグ), delay (fireAccumulator), cooldown (cooldownRemaining)

評価する13トリガー種別:
| トリガー | ロジック |
|----------|----------|
| OnStart | frame >= inPoint && 前フレームは inactive |
| OnEnd | frame >= outPoint && 前フレームは active |
| OnEnterRange | now active && 前フレームは inactive |
| OnExitRange | now inactive && 前フレームは active |
| OnLoop | frame < prevFrame |
| OnContact/Separation/Proximity | TBD (layerBounds callback 追加時) |
| OnValueExceed/Drop | 値と閾値の比較 + edge detection |
| OnValueCross | 符号反転検出 |
| OnFrame | frame == frameNumber |

### 3.3 ArtifactReactionExecutor (Artifact)

EventBus 非依存 — 直接 executeReaction() を呼ぶ形:

- `setComposition()`, `setPlaybackController()` で依存を注入
- 7種リアクション実装:
  - SetProperty → `layer->setLayerPropertyValue(path, value)`
  - AnimateProperty → `property->addKeyFrame(frame, value)`
  - RandomizeProperty → random(min, max) して setLayerPropertyValue
  - PlayAnimation → `controller->play()`
  - PauseAnimation → `controller->pause()`
  - GoToFrame → `controller->goToFrame(FramePosition(frame))`
  - SpawnLayer → factory + `composition->appendLayerTop(layer)`
  - DestroyLayer → `composition->removeLayerById(layerId)`
  - ApplyImpulse/ApplyForce/Attract/Repel/PlaySound → TBD

### 3.4 レイヤー統合ポイント

`ArtifactAbstractLayer::goToFrame()` の末尾で `reactiveEngine().evaluate(context)` を呼び、
返った TriggerEvent を `ArtifactReactionExecutor` に渡す想定。統合は次フェーズ。

## 4. 変更ファイル

| ファイル | 変更 |
|----------|------|
| `ArtifactCore/include/Reactive/ReactiveEvents.ixx` | `ReactiveEvaluationContext` struct + `ReactiveEngine` class 追加 (~50行) |
| `ArtifactCore/src/Reactive/ReactiveEvents.cppm` | engine 実装 — 管理API + evaluate 全トリガー (~150行) |
| `Artifact/include/Service/ArtifactReactionExecutor.ixx` | 新規 (~60行) |
| `Artifact/src/Service/ArtifactReactionExecutor.cppm` | 新規 (~150行) |

## 5. 残タスク

- [ ] OnContact/OnSeparation/OnProximity の layerBounds callback
- [ ] ApplyImpulse/ApplyForce/Attract/Repel の物理リアクション
- [ ] PlaySound リアクション (AudioLayer 作成)
- [ ] layer 統合: goToFrame() → ReactiveEngine::evaluate() 呼び出し
- [ ] ApplyProperty / Attract / Repel の継続的評価 (evaluate 内で毎フレーム適用)
- [ ] easing 文字列 → InterpolationType 変換 (AnimateProperty 用)
