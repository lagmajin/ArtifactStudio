# TimeCodeRange Test Coverage Gap

**最終更新:** 2026-10-31

## 調査

`ArtifactCore/include/Time/TimeCodeRange.ixx` と `ArtifactCore/src/Time/TimeCodeRange.cpp` は、フレーム範囲、時間換算、半開区間の包含判定、交差・結合、trim、コピー／ムーブを提供する。既存 `FrameTimeTest.cpp` は `TimeCode` と `FrameRange` を検証しているが、`TimeCodeRange` を直接検証する独立テストは見つからなかった。

## 追加したテスト

`tests/ArtifactCore/TimeCodeRangeContractTest.cpp` を追加し、`ArtifactCore` にリンクする独立ターゲットとして登録した。時間単位変換と終端除外、重なり・隣接・離隔範囲の演算、trim の clamp、コピーの独立性、PImpl 所有権のムーブを確認する。ソース調査で実装契約が読める範囲に期待値を限定し、テストのビルド・実行は行っていない。

## 追加拡充（2026-10-31）

空範囲・逆転範囲と有効範囲の `overlaps()` を追加検査する。静的確認で逆転範囲が有効範囲と重なると誤判定する不整合を確認したため、無効範囲と長さ0範囲を overlap 判定から除外するよう `TimeCodeRange.cpp` を修正した。fps が0または負の場合に30 fpsへ戻す既存契約も追加テスト化した。ビルド・テスト未実施。
