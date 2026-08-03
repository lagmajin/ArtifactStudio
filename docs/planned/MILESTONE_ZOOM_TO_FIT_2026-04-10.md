# ズームtoフィット機能の拡張
**ステータス:** 実装完了（runtime検証待ち）
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

## Static Audit Update (2026-07-29)

- Composition設定時と既存の `zoomFit()` 経路で、viewport短辺の5%を `fitToViewport()` の余白として渡すようにした。
- アスペクト比維持と既存のFit導線は保持する。
- `zoomFitSelection()` と Command Palette／context menu の `Zoom Fit Selection` を追加し、選択レイヤーの transformed bounds を5%余白でviewport中央へ収めるようにした。
- `zoomFitVisible()` と Command Palette／context menu の `Zoom Fit Visible` を追加し、可視レイヤーの transformed bounds を統合して5%余白でviewport中央へ収めるようにした。
- Selection / Visible / Work Area、遷移アニメーション、指定ショートカット、runtime操作確認は未実施。

判定: **Composition Fit、Selection Fit、Visible Fit、Work Area Fit を実装。遷移アニメーション、指定ショートカット、runtime検証は pending。**

確認範囲: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`、`ArtifactCore/src/UI/ShortcutBindings.cppm`。ビルド・実機操作による動作確認は未実施。

### Shortcut audit (2026-07-29)

- 既存の `ShortcutBindings` は `Ctrl+/` / `Shift+/` を標準のズーム操作に割り当てている。
- `Ctrl+0` / `Ctrl+Alt+0` は Timeline / Contents Viewer の既存操作と衝突するため、Selection / Visible / Work Area 用の0系ショートカットはこの段階で新規登録しない。
- キー割り当てを追加する場合は、アプリ全体の shortcut scope と競合解決を先に設計する。
