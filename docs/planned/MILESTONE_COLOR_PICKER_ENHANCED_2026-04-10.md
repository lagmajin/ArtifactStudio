# カラーピッカーの拡張機能
**マイルストーン**: M-CP-1 Enhanced Color Picker
**作成日**: 2026-04-10
**見積もり**: 8-10h
**優先度**: Low (細かいUX改善)

## 概要

After Effects のカラーピッカーをより使いやすく拡張。
カラーホイール、最近使った色、システムカラーピッカー統合など。

## 機能仕様

### カラーピッカーモード
**新規タブ追加:**
- `Wheel`: カラーホイール + 明度バー
- `Spectrum`: スペクトルバー + 明度バー
- `Sliders`: RGB/HSV/HSL 数値スライダー
- `System`: OS標準カラーピッカー呼び出し

### スマート機能
**利便性向上:**
- 最近使った色のグリッド表示 (24色)
- カラーパレット保存/読み込み
- 透明度の視覚的プレビュー
- カラーブラインド対応表示

### 統合機能
**ワークフロー連携:**
- 選択色をクリップボードにコピー
- 16進数/HTML色コード表示
- カラーマッチング (画像から色抽出)
- カラーモード変換 (RGB↔HSV↔HSL)

### 実装要件
- 既存カラーピッカー拡張
- 設定保存 (お気に入り色など)
- キーボードショートカット対応
- 高DPIディスプレイ対応

### 実装場所
- `Artifact/src/Widgets/Dialogs/ArtifactColorPickerDialog.cppm` (拡張)
- 設定項目: `Preferences > Colors > Picker settings`

## 技術的考慮
- カラー計算の精度
- メモリ使用量 (最近使った色保存)
- アクセシビリティ対応

## AEとの差別化
- より多くのカラーモード
- 最近使った色の充実
- システム統合

## テストケース
- 各種カラーモードの正確な変換
- 最近使った色の保存/読み込み
- システム統合の動作確認

## 2026-07-25 実装監査

FloatColorPicker の既存導線、スライダーのクリックジャンプ補助、ColorPaletteWidget の調和色生成・Smart Extract・palette の JSON 保存／読込は実装を確認した。一方、Wheel／Spectrum／Sliders／System のタブ構成、24色の recent grid、透明度 preview、color-blind 表示、clipboard／hex 表示、picker 設定の永続化、shortcut／高DPI受け入れ確認は揃っていない。なお、新規 QColorDialog／OS picker 導入はプロジェクト方針上追加しない。したがって既存 picker 改善は部分実装、本マイルストーン全体は未完了・runtime未検証とする。
