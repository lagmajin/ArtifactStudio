# 2D Rig Core Contract Note - 2026-04-29

## 判断

現在の路線は大きく正しい。

- `ArtifactCore::Rig2D` をリグデータ、階層、評価、IK、永続化の責務境界にする
- `ArtifactAbstract2DLayer` は Core リグを保持し、アプリ側 UI へ薄い API を出す
- `TransformGizmo` や Timeline へリグ固有状態を直接混ぜない

## 今回進めたこと

- `Rig2D` に ID ベースの骨操作 API を追加
- `Rig2D` / `Bone2D` に JSON 保存復元を追加
- `Rig2D::evaluate(time)` を追加し、将来のキー/制約/IK 評価の入口を Core に固定
- `ArtifactAbstract2DLayer` に ID ベースの薄いリグ操作 API と rig2D 保存復元を追加
- 既存の 2D 派生レイヤーが `ArtifactAbstract2DLayer` 経由で保存/復元/プロパティ取得するように調整

## まだやらないこと

- リグ編集 UI の新規 signal/slot 配線
- `TransformGizmo` へのリグ責務の混入
- hot path への `QImage` 追加
- IK/制約/ウェイトの本格実装

## 次の安全な順番

1. `Rig2D` の JSON ラウンドトリップ確認
2. `ArtifactAbstract2DLayer` の派生レイヤー保存復元確認
3. Bone 選択/編集状態を app 側の軽い state として設計
4. IK/constraint は Core 側モデルを決めてから UI へ出す
