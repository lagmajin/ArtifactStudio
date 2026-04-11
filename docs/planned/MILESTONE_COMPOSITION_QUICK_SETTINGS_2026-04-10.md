# コンポジションクイック設定メニューの実装
**マイルストーン**: M-CO-1 Composition Quick Settings Menu
**作成日**: 2026-04-10
**見積もり**: 4-6h
**優先度**: Low (細かいUX改善)

## 概要

After Effects ではコンポジションパネル上で右クリックすると、解像度やフレームレートなどの設定を素早く変更できるメニューが表示される。
これを実装することで、設定変更のためのダイアログ開閉を減らし、ワークフローを効率化する。

## 機能仕様

### 右クリックコンテキストメニュー
**Composition パネル右クリック時:**
- `Resolution >` サブメニュー
  - `HD (1920x1080)`
  - `Full HD (1920x1080)` ✓現在選択中
  - `4K (3840x2160)`
  - `8K (7680x4320)`
  - `Custom...` (ダイアログを開く)

- `Frame Rate >` サブメニュー
  - `23.976 fps`
  - `24 fps`
  - `25 fps`
  - `29.97 fps` ✓現在選択中
  - `30 fps`
  - `50 fps`
  - `59.94 fps`
  - `60 fps`
  - `Custom...`

- `Duration >` サブメニュー
  - `5 seconds`
  - `10 seconds`
  - `30 seconds`
  - `1 minute`
  - `Custom...`

- `Background Color...` (カラーピッカーダイアログ)
- `---`
- `Composition Settings...` (フルダイアログを開く)

### 実装要件
- `ArtifactCompositionEditor` の contextMenuEvent でメニュー生成
- 現在の設定値にチェックマーク表示
- 変更時は即座に反映 (undo対応)
- キーボードショートカット対応可能

### 実装場所
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 既存のコンテキストメニューに追加

## 技術的考慮
- プリセット値はハードコードではなく設定ファイルから読み込み
- undo/redo スタック対応
- 複数コンポジション選択時の動作

## AEとの差別化
- より多くの解像度プリセット (16:9, 1:1, 9:16 など)
- 最近使用した設定の履歴表示
- カスタムプリセットの保存機能

## テストケース
- 各プリセットの正確な適用
- チェックマークの正しい表示
- undo/redo の動作確認
- 複数コンポジションでの動作