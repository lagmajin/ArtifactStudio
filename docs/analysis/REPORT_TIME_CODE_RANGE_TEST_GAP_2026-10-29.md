# TimeCodeRange Test Coverage Gap

**最終更新:** 2026-10-29

## 調査

`ArtifactCore/include/Time/TimeCodeRange.ixx` と `ArtifactCore/src/Time/TimeCodeRange.cpp` は、フレーム範囲、時間換算、半開区間の包含判定、交差・結合、trim、コピー／ムーブを提供する。既存 `FrameTimeTest.cpp` は `TimeCode` と `FrameRange` を検証しているが、`TimeCodeRange` を直接検証する独立テストは見つからなかった。

## 追加したテスト

`tests/ArtifactCore/TimeCodeRangeContractTest.cpp` を追加し、`ArtifactCore` にリンクする独立ターゲットとして登録した。時間単位変換と終端除外、重なり・隣接・離隔範囲の演算、trim の clamp、コピーの独立性、PImpl 所有権のムーブを確認する。ソース調査で実装契約が読める範囲に期待値を限定し、テストのビルド・実行は行っていない。
