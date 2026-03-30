# Milestone: Motion Blur (2026-03-29)

**Status:** Not Started
**Goal:** レイヤーにモーションブラーを適用。高速移動オブジェクトの残像効果。

---

## 現状

| 機能 | 状態 |
|------|------|
| Transform の前後フレーム差分 | ❌ 未実装 |
| モーションブラー計算 | ❌ 未実装 |
| ブラー合成シェーダー | ⚠️ Gaussian はあるがモーション方向なし |
| タイムサンプリング | ❌ 未実装 |

---

## Architecture

```
MotionBlurPass
  ├── 1. 前フレームと現在フレームの Transform 差分を計算
  │     └── velocity = (currentPos - prevPos) / deltaTime
  ├── 2. 速度ベクトルからブラー方向・強度を決定
  │     ├── angle = atan2(velocity.y, velocity.x)
  │     └── magnitude = length(velocity) * shutterAngle
  ├── 3. 方向性ブラーを適用
  │     ├── Multi-pass directional blur (directional kernel)
  │     └── サンプル数 = shutterSamples (3, 8, 16, 32)
  └── 4. 結果をコンポジットに合成
```

---

## Implementation

### パラメータ:
- `shutterAngle` (0.0 ~ 1.0) — シャッター角度。0.5 = 180度
- `shutterPhase` (-0.5 ~ 0.5) — シャッターフェーズ
- `shutterSamples` (1 ~ 32) — サンプル数
- `adaptiveSampleLimit` — アダプティブサンプリング上限

### 描画フロー:
```
for each layer with motionBlur enabled:
  1. prevTransform = layer->globalTransformAt(frame - 1)
  2. currTransform = layer->globalTransformAt(frame)
  3. velocity = computeVelocity(prevTransform, currTransform)
  4. for s in 0..shutterSamples:
       t = (s / shutterSamples) * shutterAngle + shutterPhase
       sampleTransform = lerp(prevTransform, currTransform, t)
       render layer at sampleTransform
       accumulate into blur buffer
  5. blurBuffer /= shutterSamples
```

### GPU 実装:
- 方向性ブラー Compute シェーダー
- サンプリングは平均化（box blur）またはガウシアン重み

---

## 見積

| タスク | 見積 |
|--------|------|
| Transform 差分計算 | 2h |
| 方向性ブラー Compute シェーダー | 3h |
| マルチサンプル描画ループ | 3h |
| パラメータ UI | 1h |

**総見積: ~9h**

---

## Deliverables

| ファイル | 内容 |
|---------|------|
| `Artifact/shaders/MotionBlurCS.hlsl` | 方向性ブラー CS |
| `ArtifactCore/include/Graphics/MotionBlurPass.ixx` | モーションブラー処理 |
| `ArtifactCore/src/Graphics/MotionBlurPass.cppm` | 実装 |

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `ArtifactCore/include/Transform/AnimatableTransform3D.ixx` | - | Transform データ |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 1207 | drawSpriteTransformed |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 1362 | レンダーループ |
