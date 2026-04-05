# カラースウォッチダイアログ 新規作成マイルストーン

## 概要

`ColorSwatchDialog` を新規作成する。
デザイン仕様画像（`docs/image/colowswatch.jpeg`）に基づくカテゴリー別スウォッチ管理ダイアログ。

既存の `ArtifactColorSwatchWidget`（埋め込みウィジェット）は別用途として存続させ、
本ダイアログはスタンドアロン QDialog として実装する。

## 現状

- `Artifact/src/Widgets/Color/ArtifactColorSwatchWidget.cppm` に QWidget 版が存在
- Import/Export .gpl のみで、カテゴリー管理・情報表示パネルなし
- QDialog 版は存在しない

## 目標デザイン（仕様画像より）

| 要素 | 内容 |
|------|------|
| ツールバー | ⠿ / • / + / × / ↑ / ↓ ボタン群、右端に "AE Default Library" ラベル |
| スウォッチセクション | 3カテゴリー（基本カラー / 映像制作 / 透明・グロー）、各折りたたみ可能 |
| 基本カラー | White 〜 Yellow 12 色 + 追加ボタン |
| 映像制作 | 映像制作用 10 色プリセット + 追加ボタン |
| 透明・グロー | 6 種のチェッカーパターンスウォッチ + 追加ボタン |
| 情報パネル（下部） | 選択色の正方形プレビュー（48×48）/ カラー名 / HEX + RGBA値 |
| ボタン | 閉じる / 適用 ↗ |

## 実装タスク

1. `Artifact/include/Widgets/Dialog/ColorSwatchDialog.ixx` 新規作成
   - `ColorSwatchDialog : QDialog`
   - `colorApplied(FloatColor)` シグナル
   - `selectedColor()`, `setCurrentColor()` メソッド
2. `Artifact/src/Widgets/Dialog/ColorSwatchDialog.cppm` 新規作成
   - `SwatchEntry` 構造体（isChecker フラグ付き）
   - 3カテゴリーデータのハードコード定義
   - 折りたたみセクションウィジェット
   - スウォッチセル（クリックで選択、ダブルクリックで適用）
   - 情報パネルの動的更新
   - `showEvent` でウィンドウ中央表示
3. CMakeLists.txt は `GLOB_RECURSE` により自動収集（変更不要）

## 変更ファイル

| ファイル | 変更種別 |
|---------|---------|
| `Artifact/include/Widgets/Dialog/ColorSwatchDialog.ixx` | 新規作成 |
| `Artifact/src/Widgets/Dialog/ColorSwatchDialog.cppm` | 新規作成 |

## 優先度

中（カラー選択ワークフロー拡張）

## 参照

- デザイン画像: `docs/image/colowswatch.jpeg`
- 既存ウィジェット: `Artifact/src/Widgets/Color/ArtifactColorSwatchWidget.cppm`
