# 平面レイヤー設定ダイアログ リデザイン マイルストーン

## 概要

`CreateSolidLayerSettingDialog` / `EditPlaneLayerSettingDialog` を
デザイン仕様画像（`docs/image/PlaneLayerSettingDialog.jpeg`）に近づける。

## 現状

- タイトル: "Plane Layer Settings"（英語）
- プリセットコンボで解像度選択
- 幅・高さ DragSpinBox のみ
- カラーボタン 1 個（プレビューアイコン）
- セクション区切りなし
- ボタン: OK / Cancel（英語）

## 目標デザイン（仕様画像より）

| 要素 | 内容 |
|------|------|
| ダイアログタイトル | 「平面設定」 |
| セクション: 名前 | 「名前」ラベル + QLineEdit（プレースホルダー: "ホワイト 平面 1"） |
| セクション: サイズ | 幅/高さ + "px" ラベル + ロックボタン（縦横比固定トグル） |
| | 単位コンボ（ピクセル / ポイント / パーセント / ミリメートル） |
| | 「コンポジションサイズを使用」ボタン |
| | ピクセル縦横比コンボ（正方形ピクセル / D1/DV 各種） |
| セクション: カラー | カラースウォッチボタン（40×24）+ HEX テキストボックス |
| | チェックボックス「平面をコンポジションサイズに合わせる」 |
| ボタン | キャンセル / OK（日本語）|

## 実装タスク

1. `PlaneLayerSettingPage` コンストラクタ全面改修
   - `resolutionCombobox_` → `unitCombo`（単位選択）に置き換え
   - `lockButton`（縦横比ロック）追加
   - `pixelAspectCombo`（ピクセル縦横比）追加
   - `hexColorEdit`（HEX テキスト入力）追加
   - `fitToCompCheck`（コンポジションサイズに合わせるチェック）追加
   - セクションヘッダー（薄い区切り線 + 灰色ラベル）追加
2. `CreateSolidLayerSettingDialog` 改修
   - タイトル「平面設定」
   - × 閉じるボタン（ヘッダー右端、ホバー赤）
   - 「名前」セクションヘッダー追加
   - ボタンラベルを「キャンセル」/ "OK" に統一
   - サイズ 520×500 に調整
3. `EditPlaneLayerSettingDialog` 同様に改修
4. カラー↔HEX の双方向同期実装

## 変更ファイル

| ファイル | 変更種別 |
|---------|---------|
| `Artifact/src/Widgets/Dialog/CreatePlaneLayerDialog.cppm` | 大規模改修 |
| `Artifact/include/Widgets/Dialog/CreatePlaneLayerDialog.ixx` | 変更なし（API 維持） |

## 優先度

高（レイヤー作成の主要ダイアログ）

## 参照

- デザイン画像: `docs/image/PlaneLayerSettingDialog.jpeg`
