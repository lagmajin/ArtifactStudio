# Rename dialog design reference

**最終更新:** 2026-09-21

Composition と Layer の名称変更を、同じ操作規則を持つ小型モーダルとして整理するための設計参照。

## モック

- [Composition Rename](composition-rename-dialog-concept-2026-09-21.png): 通常状態。対象のコンポジション情報を補助表示し、名称入力へ初期フォーカスを置く。
- [Layer Rename](layer-rename-dialog-concept-2026-09-21.png): 通常状態。レイヤーカラー、種別、所属コンポジションを対象識別に使う。
- [Validation](rename-dialog-validation-concept-2026-09-21.png): 重複名の例。予測可能な入力エラーは別の警告ダイアログを開かず、入力欄の直下に表示する。

画像は構造、情報量、余白、フォーカス、検証状態の参照であり、背景に描かれた Project、Timeline、Properties の構成や機能は採用対象外とする。画像内の色値も固定仕様ではなく、実装時点のテーマ token を使用する。

## 共通仕様

- ダイアログを開いた時点で現在名を全選択し、名称入力へフォーカスを置く。
- フッター右端は `Cancel`、`Rename` の順とし、主操作を右端へ置く。
- 空文字、空白のみ、使用不能文字、重複名などの既知エラーは入力欄の直下に表示し、解消まで `Rename` を無効化する。
- `Enter` は有効な場合のみ確定、`Escape` とタイトルバーの閉じる操作は変更を適用せず閉じる。
- Composition は解像度、fps、尺を読取専用の対象確認情報として表示できる。これらは名称変更では編集しない。
- Layer は所属 Composition、レイヤーカラー、レイヤー種別を読取専用の対象確認情報として表示できる。source、effect、property は名称変更では編集しない。
- 名称が変化していない場合は mutation を発行せず閉じる。

## 実装境界

現状の各入口にある `QInputDialog::getText()` を専用の共通 Rename dialog へ置き換える場合も、名称変更の実処理は既存 service と Undo command を正規経路とする。

- Composition: `ArtifactProjectService::renameComposition()` / `RenameCompositionCommand`
- Layer: `ArtifactProjectService::renameLayerInCurrentComposition()` / `RenameLayerCommand`

ダイアログは入力値と accept / reject の結果だけを返し、composition や layer を直接変更しない。Project View、Timeline、Composition menu、Layer menu、Composition Graph の各入口は同じダイアログ規則と既存 mutation 経路を共有する。新規 signal / slot は追加しない。

共通の視覚・入力・実装規則は [Dialog design and implementation rules](../dialog-guidelines/README.md) を参照する。

## 2026-09-21 実装

- `ArtifactRenameDialog` を `Artifact.Widgets.AppDialogs` に追加し、Composition／Layer／一般 Project Item の共通入力面とした。
- Project View、Timeline、Composition menu、Layer menu、Composition Graph の既存 Rename 入口を共通ダイアログへ置換した。
- 現在名の全選択、空文字のインライン検証、無変更時の確定抑止、Cancel／Rename の順序を実装した。
- ダイアログは入力値だけを返し、既存 service／Undo 経路による mutation は各呼出側に維持した。
- QtCSS と新規 signal／slot 接続は追加していない。

ビルド、テスト、DPI／IME／キーボード／スクリーンリーダーでの実機確認は未実施。重複名リストを渡す API は用意したが、各 domain の重複許可方針が未確定のため、現時点の各入口では空文字検証のみ有効化している。
