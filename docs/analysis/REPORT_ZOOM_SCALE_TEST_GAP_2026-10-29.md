# ZoomScale2D Test Coverage Gap

**最終更新:** 2026-10-29

## 調査

`Core.Scale.Zoom` は scale の clamp、増減、四則演算、境界設定、reset、比較、copy/move を提供する。テスト群を検索したが、このクラスを直接使うテストは見当たらなかった。

## 追加したテスト

`tests/ArtifactCore/ZoomScaleContractTest.cpp` を独立ターゲットとして登録し、既定境界、カスタム境界、操作後の clamp、値演算の非変更性、コピーと代入の独立性、reset と等価比較を検証する。テストはソース上の既存 API に限定し、ビルド・実行はしていない。
