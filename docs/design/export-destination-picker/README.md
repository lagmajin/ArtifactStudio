# Export Destination Picker 採用モック

**最終更新:** 2026-09-12

## 採用画像

- `export-destination-picker-dcc-concept-2026-09-12.png`

## 採用範囲

- Places、destination browser、output previewの3ペイン構成
- base name、version、frame token、subfolder pattern、insert token
- format、frame range、推定容量、空き容量、最終filenameの事前表示
- non-overwriteを既定とするconflict policyと具体的な衝突説明

## 責務境界

- 保存先と最終output pathの構築・検証を担当する。
- render開始、encoder設定、frame生成、queue mutationは既存Render Queue経路へ渡す。
- 衝突時に暗黙上書きせず、Create next versionを安全な既定とする。
- 推定容量を確定値として表示せず、estimateであることを明記する。

## 実装状況

- Render Output Settings の Browse 操作を `RenderDestinationPickerDialog` に置き換えた。
- folder、base name、version、image-sequence token、extensionと最終pathを一画面で確認できる。
- 既存ファイルがある場合は、確定pathを次の空きversionへ進める。暗黙の上書きは行わない。
- format、codec、frame range、容量見積り、render queue mutationは既存の Render Output Settings／Render Queue の責務として残す。

## 生成プロンプト要約

render destination browserにversion／frame token、subfolder、conflict policy、filename result、推定容量／空き容量を統合し、確定前に出力結果を読めるDCC専用画面を指定した。
