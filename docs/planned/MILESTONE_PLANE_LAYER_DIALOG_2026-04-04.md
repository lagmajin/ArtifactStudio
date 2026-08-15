# 平面レイヤー設定ダイアログ リデザイン マイルストーン

**最終更新:** 2026-08-15

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

## 2026-07-25 実装監査

`CreatePlaneLayerDialog.cppm` に日本語タイトル／セクション、名前入力、単位コンボ、幅・高さ、縦横比ロック、ピクセル縦横比、カラー・HEX 双方向同期、コンポジションサイズ適用、Create／Edit 両ダイアログの共通導線を確認した。設計タスクの主要項目は静的には実装済みと判定する。一方、仕様画像との実際のレイアウト一致、各入力値の保存・再読込、lock／fit／HEX の runtime 挙動、既存 layer creation からの実機導線は未実行のため未検証とする。

## Update 2026-08-15

- 現行コードを再確認し、平面レイヤー作成時の `ArtifactLayerFactory` が単色だけでなく gradient start/end color、angle、reverse、center、scale、offset を初期化へ渡すことを確認した。
- ダイアログ側の名前、サイズ、単位、縦横比ロック、ピクセル縦横比、HEX 色、コンポジションサイズ適用の主要導線は既存監査どおり実装済み相当である。
- ただし、仕様画像とのピクセル単位のレイアウト一致、編集後の値の保存／再読込、lock／fit／HEX の相互更新、既存作成導線の実機動作は静的確認だけでは証明できない。判定は **主要UI実装済み／runtime・受入れ未確認** を維持する。
- ビルド・テスト・runtime 確認は未実施。
