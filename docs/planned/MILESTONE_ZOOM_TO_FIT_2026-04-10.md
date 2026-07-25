# ズームtoフィット機能の拡張
**マイルストーン**: M-CO-3 Zoom to Fit Enhancements
**作成日**: 2026-04-10
**見積もり**: 5-7h
**優先度**: Low (細かいUX改善)

## 概要

After Effects の「Fit」コマンドをより賢くし、コンポジションの効率的な閲覧を支援。
ワンクリックで最適なズームレベルに調整。

## 機能仕様

### スマートフィット
**状況に応じたズーム:**
- `Fit to Composition`: コンポジション全体を表示
- `Fit to Selection`: 選択レイヤーのバウンディングボックスにフィット
- `Fit to Visible`: 表示中の全レイヤーにフィット
- `Fit to Work Area`: 作業領域範囲にフィット

### アスペクト比考慮
**ビューポート最適化:**
- コンポジションのアスペクト比を維持
- 余白を最小限に (5%マージン)
- 高解像度ディスプレイ対応

### キーボードショートカット
**クイックアクセス:**
- `Ctrl + 0`: Fit to Composition
- `Ctrl + Alt + 0`: Fit to Selection
- `Shift + 0`: Fit to Visible
- `Ctrl + Shift + 0`: Fit to Work Area

### 実装要件
- 既存ズームシステム拡張
- アニメーション効果付きズーム遷移
- 設定保存 (デフォルト動作)
- 複数コンポジション対応

### 実装場所
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` (拡張)
- メニュー: `View > Zoom > Fit`

## 技術的考慮
- ズーム計算の正確性
- アニメーションのパフォーマンス
- 複数モニター対応

## AEとの差別化
- より詳細なフィットオプション
- アニメーション効果
- キーボードショートカットの充実

## テストケース
- 各種フィットモードの正確性
- アニメーションの滑らかさ
- キーボードショートカットの動作

---

## 2026-07-25 現状確認

`ArtifactCompositionRenderController` / `ArtifactCompositionRenderWidget` から `fitToViewport()` を呼ぶ単一の Zoom Fit は実装済みで、Composition Editor のコマンド／コンテキスト導線と View メニューの「画面に合わせる」も存在する。ショートカットも `ViewFitToScreen` として登録され、既定値は `Shift+/` になっている。

一方、仕様にある Selection / Visible / Work Area ごとのフィット計算、5%マージンを明示した専用モード、フィット遷移アニメーション、複数コンポジション単位の設定保存、`Ctrl+0` 等の指定ショートカットは確認できない。したがって本マイルストーンは「Composition 全体を viewport に合わせる基礎機能は実装済み、拡張モードは未実装」と判定する。

確認範囲: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`、`ArtifactCore/src/UI/ShortcutBindings.cppm`。ビルド・実機操作による動作確認は未実施。
