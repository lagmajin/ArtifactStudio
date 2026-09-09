# Light Layer Dialog design

**最終更新:** 2026-09-09

## Version 1

`light-layer-dialog-v1-2026-09-09.png` は、現在は既定値で即時作成される Light Layer に、最小限の初期設定を与えるための提案モックである。

- 基本項目は Name / Type / Color / Intensity とする。
- Type は既存の Point / Spot / Parallel / Ambient / Area を使用する。
- 右側の Type Settings は選択種別に必要な既存項目だけを表示する。Point は Range / Cast Shadows、Spot は Range / Cone Angle / Cone Feather / Cast Shadows、Area は Area Shape / Width / Height / Cast Shadows を想定する。
- Parallel / Ambient では不要な距離・形状項目を表示しない。
- GOBO / Glow / Light Linking と詳細な影設定は既存 Inspector の責務として、作成ダイアログには追加しない。
- Color は `FloatColorPicker` 系の承認済み経路を使い、`QColorDialog` を使用しない。
- ライティング結果の画像プレビュー、GPU readback、texture / staging resource、`QImage` を追加しない。
- 新規の signal / slot 接続、QtCSS、レンダリング経路変更を行わない。

## Version 2 — Rich

`light-layer-dialog-v2-rich-2026-09-09.png` は Version 1 の機能範囲を維持し、作成時の判断材料と視覚階層を強化した採用案である。Version 2 は `CreateLightLayerDialog` として実装済み。

- Type のドロップダウンを Point / Spot / Parallel / Ambient / Area の選択タイルに置き換え、種類を比較しやすくする。
- Type Settings 冒頭に選択中タイプの名称と短い説明を表示する。
- 下部の要約は Type / Color / Intensity の既存入力から導出し、新たな状態や設定値を保持しない。
- Type は5つの選択タイルとして実装する。専用アイコンは既存資産で5種を正しく表現できないため、Version 2 初期実装では誤認を避けてラベル表示とする。
- Version 1 と同様に、レンダープレビュー、高度設定、GPU readback、画像変換は追加しない。
- Layer メニューと Composition Editor の既存ライト作成導線は同じダイアログを使用する。
