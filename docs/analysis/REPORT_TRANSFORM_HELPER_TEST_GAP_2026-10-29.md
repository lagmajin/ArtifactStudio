# Transform Helper Test Coverage Gap

**最終更新:** 2026-10-29

## 調査

既存 `CompositionTransformTimeContractTest.cpp` は Qt の 2D / 3D helper と Qt 行列の親子結合を既に検証していた。未カバーだったのは独自 `Matrix4x4` 経路とその親子結合である。

## 追加したテスト

`tests/ArtifactCore/TransformHelperContractTest.cpp` を独立ターゲットとして登録した。独自 `Matrix4x4` の point transform を Qt 3D helper と同じ anchor / probe 座標で照合し、親子結合後の点を期待座標と比べる。ソース照合で `combineLayerMatrix4x4()` が local × parent とし、Qt 3D 経路の親 × local と演算順が逆であることを確認したため、独自行列経路も親 × local に修正した。ビルド・テスト実行は行っていない。
