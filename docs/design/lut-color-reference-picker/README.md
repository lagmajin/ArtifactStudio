# LUT & Color Reference Picker 採用モック

**最終更新:** 2026-09-12

## 採用画像

- `lut-color-reference-picker-dcc-concept-2026-09-12.png`

## 採用範囲

- library／folder、同一reference thumbnail grid、before/after詳細の3ペイン構成
- Technical／Film／Creative／Camera／Favorites filter
- format、cube size、input／output color space、domain、compatibility表示
- preview strengthとworking-preview-onlyの明確な区別

## 責務境界

- LUT探索、比較preview、互換性確認、選択を担当する。
- 全thumbnailは同じreference sourceで生成し、look比較の条件を揃える。
- color transformの適用、project添付、effect property更新、Undoは既存Color／Effect経路へ渡す。
- input／output color spaceが不明なLUTを暗黙にACES互換として扱わない。

## 実装状況

- `ArtifactLutColorReferencePickerDialog` を `Artifact.Widgets.ColorSciencePanel` module に追加した。
- Color Science Panelの既存 `Load LUT…` は専門pickerを開く導線へ変更した。
- All LUTs／Built-in／Files のlibrary、検索、アイコングリッド、選択詳細、System file import、Use LUTを実装した。
- 選択後は既存 `ArtifactColorScienceManager::loadBuiltinLUT()` または `loadLUT()` へ委譲する。LUT parser、GPU適用、ColorLUTEffect、Undo経路は変更していない。
- 現行Color managerがpreview-only routing、LUT input/output domain metadata、favorites永続化を公開していないため、それらを機能するように見せるコントロールは無効表示または未実装のままにした。
- グリッドはファイル／built-inのアイコン表示を先行し、同一referenceへのLUT適用サムネイルとbefore/after split previewは既存GPU preview APIが整ってから追加する。

## 生成プロンプト要約

同一portrait／color chartを全LUTへ適用したgrid、before/after split、technical metadata、favorites、preview strengthを備えるDCC向けLUT pickerを指定した。
