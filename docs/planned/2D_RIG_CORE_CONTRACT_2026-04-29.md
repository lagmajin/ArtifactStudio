# 2D Rig Core Contract Note - 2026-04-29

## 判断

現在の路線は大きく正しい。

- `ArtifactCore::Rig2D` をリグデータ、階層、評価、IK、永続化の責務境界にする
- `ArtifactAbstract2DLayer` は Core リグを保持し、アプリ側 UI へ薄い API を出す
- `TransformGizmo` や Timeline へリグ固有状態を直接混ぜない

今の repo にはすでに `Rig2D` / `Bone2D` / `ArtifactAbstract2DLayer::rig2D()` があり、完全新規の `RigGraph` を作るより既存を拡張する方が安全。

## 今回進めたこと

- `Rig2D` に ID ベースの骨操作 API を追加
- `Rig2D` / `Bone2D` に JSON 保存復元を追加
- `Rig2D::evaluate(time)` を追加し、将来のキー/制約/IK 評価の入口を Core に固定
- `ArtifactAbstract2DLayer` に ID ベースの薄いリグ操作 API と rig2D 保存復元を追加
- 既存の 2D 派生レイヤーが `ArtifactAbstract2DLayer` 経由で保存/復元/プロパティ取得するように調整
- `Rig2D` に control / constraint の初期実装を追加
- `ArtifactAbstract2DLayer` から control / constraint を作れる薄い API を追加
- `RigEvaluationContext2D` を追加して constraint 評価の索引を分離

実際の実装地点:

- `ArtifactCore/include/Rig/Rig2D.ixx`
- `ArtifactCore/src/Rig/Rig2D.cppm`
- `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`

## まだやらないこと

- リグ編集 UI の新規 signal/slot 配線
- `TransformGizmo` へのリグ責務の混入
- hot path への `QImage` 追加
- 汎用 `RigGraph` の全面置き換え
- まだ UI 上で control を直接編集する導線の本格配線

---

## 既存クラスの見直し方針

### `Rig2D`

今の中核として残す。

- ボーン階層
- 評価入口
- IK
- JSON 永続化
- 将来の constraint 評価
- control / constraint の保存と評価

### `Bone2D`

単なる静的ノードではなく、評価結果を保持できる実行単位に寄せる。

- base pose
- keyed pose
- resolved pose
- local/global transform cache

### `ArtifactAbstract2DLayer`

UI ではなくレイヤー所有のリグホストに徹する。

- `rig2D()` で Core を公開
- `rig.*` の薄い property 群を出す
- 編集 UI からは薄く触る

### 追加したいクラス

- `RigConstraint2D`
- `RigEvaluationContext2D`
- `RigControlSet2D`
- `RigPropertyBinding2D`
- `ParentConstraint2D`
- `MapRangeConstraint2D`
- `AimConstraint2D`
- `TwoBoneIKConstraint2D`
- `ArtifactRigControllerLayer`

### いま実装済みの最小形

- `RigControl2D`
- `RigEvaluationContext2D`
- `RigConstraint2D`
- `ParentConstraint2D`
- `MapRangeConstraint2D`
- `AimConstraint2D`
- `TwoBoneIKConstraint2D`
- `ArtifactAbstract2DLayer::addRigSlider`
- `ArtifactAbstract2DLayer::addRigPoint`
- `ArtifactAbstract2DLayer::addRigAngle`
- `ArtifactAbstract2DLayer::addRigParentConstraint`
- `ArtifactAbstract2DLayer::addRigMapRangeConstraint`
- `ArtifactAbstract2DLayer::addRigAimConstraint`
- `ArtifactAbstract2DLayer::addRigTwoBoneIKConstraint`

### 置き換えない方がいいもの

- `Rig2D` を捨てて新しい `RigGraph` に全面置換すること
- Layer ごとの特殊分岐で constraint を増やすこと
- gizmo 側に rig の評価ロジックを入れること

## 次の安全な順番

1. `Rig2D` の JSON ラウンドトリップ確認
2. `ArtifactAbstract2DLayer` の派生レイヤー保存復元確認
3. `RigConstraint2D` と `RigEvaluationContext2D` の Core モデルを固める
   - `RigEvaluationContext2D` は実装済み
4. `ArtifactRigControllerLayer` を UI 表示として設計する
5. Bone 選択/編集状態を app 側の軽い state として設計する
6. IK/constraint は Core 側モデルを決めてから UI へ出す
