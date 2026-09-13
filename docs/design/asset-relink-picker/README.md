# Asset Relink Picker 採用モック

**最終更新:** 2026-09-12

## 採用画像

- `asset-relink-picker-dcc-concept-2026-09-12.png`

## 採用範囲

- missing asset、relink候補、比較詳細の3ペイン構成
- candidate confidenceとMatch Reasonsの併記
- folder mapping、unresolved skip、適用件数の明示
- missingは赤、検証済みmatchは緑、主操作はcool blue

## 責務境界

- 候補の探索、比較、採用判断までを担当する。
- logical Asset ID、media type、frame range、filesystem情報を候補理由として明示する。
- relink適用、参照更新、cache invalidation、Undoは既存Project Service／command経路に限定する。
- filename一致だけでBest matchにしない。

## 実装状況

- Asset Browser の `Find Relink Candidates...` は `RelinkCandidatePickerDialog` を使い、候補のscore、path、Match Reasons、連番frame一致数を比較してから選ぶ。
- 候補探索とranking、logical Asset ID照合、適用、cache invalidation、Undoは既存の Project Service／command経路を使用する。
- Project Viewのbulk relinkと、単一ファイルを直接指定するRelink Source導線は既存のまま。候補判断を伴うAsset Browser経路を先に専用化した。

## 生成プロンプト要約

欠落元と候補を並べ、logical Asset IDを最優先の一致理由として表示する復旧専用DCC画面を指定した。複数relink、folder mapping、未解決項目の扱いも確定前に読める構成とした。
