# Transform Helper Test Coverage Gap

**最終更新:** 2026-11-01

## 調査

既存 `CompositionTransformTimeContractTest.cpp` は Qt の 2D / 3D helper と Qt 行列の親子結合を既に検証していた。未カバーだったのは独自 `Matrix4x4` 経路とその親子結合である。

## 追加したテスト

`tests/ArtifactCore/TransformHelperContractTest.cpp` を独立ターゲットとして登録した。独自 `Matrix4x4` の point transform を Qt 3D helper と同じ anchor / probe 座標で照合し、親子結合後の点を期待座標と比べる。ソース照合で `combineLayerMatrix4x4()` が local × parent とし、Qt 3D 経路の親 × local と演算順が逆であることを確認したため、独自行列経路も親 × local に修正した。ビルド・テスト実行は行っていない。

2026-10-31、3階層の回転親＋拡大祖先＋平行移動子を合成し、各祖先が子の外側へ順に適用される座標を追加検査。ビルド・テスト未実行。

## アンカーポイント契約（2026-11-01）

`tests/ArtifactCore/TransformAnchorContractTest.cpp`（`ArtifactCoreTransformAnchorContractTest`）を追加した。内容は (1) アンカーが position に落ちる不動点性、(2) 非アンカー点を独立式 `pos + R·S·(p − anchor)` と照合、(3) 負スケール・非等方スケール、(4) アンカー 0 と素の T·R·S の一致、(5) 子アンカーが親変換を通ることと 3 階層の逐次適用一致、(6) 3D の anchorZ、(7) 独自 `Matrix4x4` と Qt 経路の 288 組グリッド一致、(8) 2D と 3D の Z 平面一致、(9) `AnimatableTransform3D` の `getAllMatrix`/`getMatrix`。

画像系は 4 ケース（回転 30°／非等方スケール／負スケール／親子合成）で、アンカー上のドットとオフセット矩形を CPU 側 `warpAffine`（`Layer2D::transformedLayer()` と同じアフィン抽出）で変換し、アンカー画素の不透明性と重心位置を画素で検証したうえで `tests/ArtifactCore/baselines/transform-anchor/` の PNG と比較する。基準画像の更新は `ARTIFACT_TRANSFORM_ANCHOR_UPDATE_BASELINES=1`、比較はチャネル差 2・差分画素 0.05%・平均差 0.2。Debug で 15/15 pass、基準画像の負例（別ケース画像を差し替え）で `differingFraction=0.0202` の FAIL を確認。

このテストで `AnimatableTransform3D::getMatrix()/getAllMatrix()` の合成順が Diligent の行ベクトル規約と逆であることが判明したため、ArtifactCore 側を修正した（`makeEulerRotation` の転置解消、`anchor * scale * rotation * translation` への並べ替え、`anchorPosition()` をアンカー写像へ変更）。詳細は `Insight.md` の 2026-11-01 項を参照。
