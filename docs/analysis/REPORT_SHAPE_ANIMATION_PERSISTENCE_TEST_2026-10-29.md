# Shape Layer Animation Persistence Test

**最終更新:** 2026-10-29

## 調査結果

- `ArtifactShapeLayer` にカスタムパスのキーフレーム設定、フレーム評価、JSON 保存・復元 API がある。
- 既存 `Artifact.Test.ShapePath` は補間とレイヤー JSON 往復を個別に確認する。別の GPU 視覚テストは主にフレーム 0 の描画を確認する。
- そのため、新しい契約テストは保存・復元後の同一オブジェクトについて、開始・中間・終了フレームの評価値を同時に検証する。

## 実装

`tests/Artifact/ShapeLayerAnimationPersistenceContractTest.cpp` は専用 GoogleTest ケースで、異なる頂点位置・ハンドル・smooth 状態を持つパスキーをフレーム 0 と 10 に設定する。`toJson()` / `fromJson()` 往復後に、キー前・キー・補間点（フレーム 2 と 5）・キー後を検証し、両頂点の位置、両ハンドル、smooth の切り替え、および範囲外 hold を照合する。描画品質や GPU backend を検証するテストではなく、パスアニメーションの永続化と評価契約に範囲を限定する。

CTest は既存 `ARTIFACT_BUILD_TESTS` 設定下で `ArtifactShapeLayerAnimationPersistenceContractTest` として登録する。ビルド・テスト実行は行っていない。
