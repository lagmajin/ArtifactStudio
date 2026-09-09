# Object Picker dialog design reference

**最終更新:** 2026-09-09

`object-picker-dialog-v1-2026-09-09.png` は、現行 `ArtifactObjectPickerDialog` の検索と階層選択を読みやすくする実装参照である。

- Search、Name／ID／Type のツリー、単一選択、Cancel／Select の既存機能だけを扱う。
- Composition と Layer の親子関係、選択行、補助情報のコントラストを参照する。
- 画像内のアイコンは種類識別の表現例であり、新しいアイコン資産や型の追加根拠にはしない。
- サムネイル、Inspector、追加フィルター、プレビューは導入しない。
- QtCSS、新規 signal / slot 接続、`QImage` の追加根拠にはしない。

## 実装状況

Version 1 のタイトル、補助説明、検索欄、Name／ID／Type の列配分、主要操作の視覚階層を `ArtifactObjectPickerDialog` に反映済み。既存の検索、単一選択、ダブルクリック確定、型制限は維持している。
