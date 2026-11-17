# Shape Path Morph Acceptance Project

**最終更新:** 2026-11-17

## 用途

頂点数の異なるパスキーフレームを UI で作成し、補間表示・Undo/Redo・保存／再読込を確認する手動受入プロジェクト。

## 作成するプロジェクト

- Composition: 256×256、24 fps、frame 0–12
- Shape layer: 塗りを1つ、ストロークを1つ設定し、レイヤーを中央に配置
- frame 0: 閉じた三角形パス。点は `(-10,-8)`, `(10,-8)`, `(0,12)`
- frame 12: 閉じた五角形パス。点は `(0,-14)`, `(13,-4)`, `(8,11)`, `(-8,11)`, `(-13,-4)`
- 保存名: `ShapePathMorphAcceptance.artifact`

座標はレイヤー中心基準の作成値。UIで同じ形を作れる場合は画面上で作成し、実際の座標系が異なる場合は形状を優先する。

## 確認手順

1. frame 0、3、6、9、12を記録し、途中フレームで輪郭が連続して変化するか確認する。
2. frame 6の時点で保存し、閉じて再度開く。frame 0、6、12の輪郭を比較する。
3. frame 12の形を一度変更して Undo、Redo し、各段階で輪郭が期待どおり戻るか確認する。
4. 開始・中間・終了フレームを Preview と Render Queue で出力し、輪郭を比較する。

## 関連する既存契約テスト

`ShapeLayerAnimationPersistenceContractTest.TopologyChangesMorphBetweenKeysAndSurviveJsonRoundTrip` は三角形から五角形への補間とJSON往復を検査する。ここでは同じ形の推移をUIおよびレンダー経路から確認する。

## 受入メモ

パスキーフレームのUI操作が利用できない場合は、UI受入を未達として記録する。テストコードが通ることだけでUI操作可能とは判定しない。
