# MILESTONE_RIG2D_BONE_KEYFRAME_ANIMATION_2026-07-25

**ステータス:** Partial（時間評価・キーフレーム補間を実装済み。FPS 設定、独立補間、AnimationLayerStack、RigControl、編集 UI、runtime 検証は未完了）
**対象:** `ArtifactCore/include/Rig/Rig2D.ixx`, `ArtifactCore/src/Rig/Rig2D.cppm`
**位置づけ:** Maya / MotionBuilder のボーンキーフレームに相当する Rig2D の時間ベース評価を実装する。
**作成日:** 2026-07-25

## 1. 目的

Rig2D システムの `Bone2D::evaluate(RationalTime)` が常に静的な `localTransform_` を返すスタブ状態から脱却し、キーフレームベースの時間補間アニメーションを可能にする。

## 2. 現状 (2026-07-25)

| 要素 | 状態 | 詳細 |
|------|------|------|
| Bone2D::evaluate(time) | ❌ スタブ | `Q_UNUSED(time); return localTransform_;` のみ。時間パラメータを完全無視 |
| AnimatableValueT\<BoneTransform\> | ❌ 利用不可 | `Animation.Value` に完成されたキーフレームエンジンがあるが Rig2D が使っていない |
| BoneTransform | ❌ 演算子不足 | `AnimatableValueT<T>` が要求する `+`, `-`, `*float` 演算子が未定義 |
| TwoBoneIKConstraint2D poleAngle | ⚠️ 直列化のみ | `poleAngle_` は `toJson()`/`fromJson()` で保存されるが `evaluate()` で未使用 |
| キーフレームシリアライズ | ❌ 不在 | ボーンの JSON にキーフレームデータを含まない |

### 既存資産

- `Animation.Value` module: `AnimatableValueT<T>`, `KeyFrameT<T>`, 各種補間 (Linear, Quadratic, Sine, Bounce, Elastic, Back 等)、Spring 物理、スレッドセーフ、JSON シリアライズ完備。`mix<T>()` / `interpolationAlpha()` / `interpolateValue<T>()` でテンプレートベースの型安全な補間が可能。

## 3. 実装内容 (2026-07-25)

### 3.1 BoneTransform 演算子追加

`BoneTransform` に以下を追加:
- `operator+(const BoneTransform&)` — 成分ごとの加算
- `operator-(const BoneTransform&)` — 成分ごとの減算
- `operator*(float)` — スカラー乗算

これにより `AnimatableValueT<BoneTransform>` が `mix()`, `interpolateValue()`, `AnimationLayerStackT` で動作可能になる。

### 3.2 Bone2D キーフレーム管理

`Bone2D` に以下を追加:
- `AnimatableValueT<BoneTransform> keyframes_` メンバー
- `addKeyFrame(FramePosition, BoneTransform)` 
- `removeKeyFrameAt(FramePosition)`
- `hasKeyFrameAt(FramePosition)`
- `keyFrameCount()`
- `clearKeyFrames()`
- `Bone2D::evaluate(time)` 実装 — キーフレームがあれば `keyframes_.at(pos)` で時間補間、なければ `localTransform_` を返す
- JSON シリアライズ対応 (`toJson()` / `fromJson()` にキーフレーム配列追加)

### 3.3 TwoBoneIKConstraint2D poleAngle 実装

`TwoBoneIKConstraint2D::evaluate()` で `poleAngle_` の符号を肘の向き決定に使用。
- `poleAngle_ >= 0` → 反時計回り (左肘)
- `poleAngle_ < 0` → 時計回り (右肘)

## 4. 変更ファイル

| ファイル | 変更 |
|----------|------|
| `ArtifactCore/include/Rig/Rig2D.ixx` | `import Animation.Value`, `import Frame.Position` 追加。`BoneTransform` に演算子追加。`Bone2D` にキーフレームAPI追加。`AnimatableValueT<BoneTransform>` メンバー追加 |
| `ArtifactCore/src/Rig/Rig2D.cppm` | `import Animation.Value`, `import Frame.Position` 追加。`BoneTransform` 演算子実装。`Bone2D::evaluate(time)` 実装。キーフレーム管理実装。JSON シリアライズ更新。`TwoBoneIKConstraint2D::evaluate()` で poleAngle 利用 |

## 5. 残タスク / 将来展望

- [ ] Rig2D にデフォルト FPS を持たせる（現在 `evaluate()` 内でハードコード 30fps）
- [ ] ボーンごとに独立した補間タイプ設定（現在はキーフレームごと）
- [ ] AnimationLayerStackT を利用したボーンアニメーションレイヤー（非破壊アニメーションブレンディング）
- [ ] RigControl の時間ベースキーフレーミング
- [ ] キーフレームエディタ UI
