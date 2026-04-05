# カラーピッカーダイアログ リデザイン マイルストーン

## 概要

`FloatColorPicker` ダイアログ（`ArtifactWidgets/src/Dialog/FloatColorPicker.cppm`）を
デザイン仕様画像（`docs/image/ColorPickerDialog.jpeg`）に近づける。

## 現状

- カラーホイール（HSV）＋ QDial（輝度）を左右に配置
- RGBA/HSV スライダーを QGroupBox で縦並び
- プリセットカラーパレット（16色）
- 値は 0–255 の整数表示
- Previous/Current の2色プレビュー
- ボタン: OK / Cancel（英語）

## 目標デザイン（仕様画像より）

| 要素 | 内容 |
|------|------|
| タブバー | **HSB** \| RGB \| HSL（上部に配置） |
| 左パネル | カラーホイール（約 280×280 px、楕円形） |
| 輝度スライダー | ホイール右横に縦配置（QSlider Qt::Vertical） |
| 右パネル上部 | 横長カラープレビューバー（高さ約 45px） |
| チャンネルスライダー | H: 0–360°（int）/ S,B,A: 0.000–1.000（float） |
| HEX 入力 | 8桁 RRGGBBAA 形式 + 「8桁でアルファ含む」ラベル |
| ボタン | リセット（左端）/ キャンセル / OK |

## 実装タスク

1. `Impl` 構造体の刷新
   - `QDial` → `QSlider(Qt::Vertical)` (brightnessSlider)
   - `QSpinBox`（S/B/A）→ `QDoubleSpinBox` (0.000–1.000, 3桁)
   - `QTabBar` + `QStackedWidget` 追加
   - `ColorViewLabel* colorPreviewBar` 追加（Previous/Current を廃止）
   - プリセットボタン配列 削除
2. HSL 変換関数追加（`rgbToHSL`, `hslToRGB`）
3. `updateAllFromColor()` を全パネル更新に対応
4. HEX: 常に 8桁 RRGGBBAA 表示
5. コンストラクタのレイアウト全面再構成

## 変更ファイル

| ファイル | 変更種別 |
|---------|---------|
| `ArtifactWidgets/src/Dialog/FloatColorPicker.cppm` | 大規模改修 |

## 優先度

高（カラー操作 UX の根幹）

## 参照

- デザイン画像: `docs/image/ColorPickerDialog.jpeg`
- ヘッダー: `ArtifactWidgets/include/Dialog/FloatColorPicker.ixx`（API 変更なし）
