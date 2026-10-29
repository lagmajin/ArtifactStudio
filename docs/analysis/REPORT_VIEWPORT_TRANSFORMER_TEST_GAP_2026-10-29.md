# ViewportTransformer Test Coverage Gap

**最終更新:** 2026-10-29

## 調査

`Core.Transform.Viewport` はキャンバス／ビューポート変換、回転、zoom-around-anchor、fit/fill、NDC 変換、定数バッファ値を提供する。一方、テスト群には `ViewportTransformer` を直接利用するケースがなく、変換の可逆性やズームアンカーの維持を独立確認するテストが見当たらなかった。

## 追加したテスト

`tests/ArtifactCore/ViewportTransformerContractTest.cpp` を独立 GoogleTest ターゲットに登録した。回転時の順逆変換、アンカーを固定するズーム、fit/fill の計算、定数バッファ値、zoom/rotation clamp、ResetView、NDC の上下方向を検証する。製品モジュールと同じ `ArtifactCore` をリンクし、GPU デバイスや Qt UI を起動せず CPU 上の契約を対象にする。ビルド・テスト実行は行っていない。

調査中、`ZoomAroundViewportPoint()` の pan 補正が回転前座標を使い、回転中に指定キャンバス点がアンカー位置からずれる実装不整合を確認した。補正を通常の `CanvasToViewport()` と同じくキャンバス中心回転後の座標に基づく計算へ修正し、回転付きのアンカー保持テストを追加した。
