# Transform Helper Test Coverage Gap

**最終更新:** 2026-10-29

## 調査

`Transform.Hlper` はレイヤーの 2D / 3D anchor・position・rotation・scale から Qt と独自行列の変換を構築し、親子変換を結合する。テスト群にはこの helper を直接呼び出すケースが見つからなかった。

## 追加したテスト

`tests/ArtifactCore/TransformHelperContractTest.cpp` を独立ターゲットとして登録した。2D / 3D とも anchor が指定 position に写ること、変換後座標が有限であること、2D 結合結果が local の後に parent を適用することを確認する。数値は Qt の行列適用規則に基づく固定点で照合する。ビルド・テスト実行は行っていない。
