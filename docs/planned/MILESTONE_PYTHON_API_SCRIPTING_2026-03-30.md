# Milestone: Python API & Scripting Console (M-PY-1)

## 🎯 目的
`ArtifactStudio` の操作を外部および内部からプログラム可能にし、大量の処理（バッチレンダリング、アセット整理、コンポジション生成）を自動化するための基盤を構築する。また、ユーザーが独自のツール（プラグイン）を作成できるエコシステムを提供する。

## 🏗️ アーキテクチャ構成
1. **`ArtifactPythonBridge`**: 
   - `pybind11` を活用し、C++ モジュール (`ArtifactCore`, `ArtifactStudio`) の各クラスを Python オブジェクトとしてエクスポートする。
2. **`ScriptingConsoleWidget`**:
   - `QPlainTextEdit` ベースの組み込みレピュル (REPL)。
   - コードのシンタックスハイライト、オートコンプリート。
3. **`PluginManager`**:
   - 特定のフォルダにある `.py` ファイルを読み込み、アプリのメニューやツールバーにボタンを追加する機能。

## 📅 実装フェーズ

### Phase 1: Python Bridge 基盤 (2026-04-10 - 2026-04-20)
- [ ] `pybind11` のプロジェクト導入と CMake 構成の整理。
- [ ] `ArtifactCore::Project`, `ArtifactCore::Composition`, `Artifact::AbstractLayer` の基本エクスポート。
- [ ] シンプルな Python スクリプトからの「プロジェクト作成、レイヤー追加」の動作確認。

### Phase 2: 組み込みコンソール UI (2026-04-21 - 2026-04-30)
- [ ] `ScriptingConsoleWidget` の実装。
- [ ] 標準入出力 (`sys.stdout`, `sys.stderr`) のウィジェットへのリダイレクト。
- [ ] カレントオブジェクト ( `app`, `project`, `selected_layers` ) への直接アクセス変数の定義。

### Phase 3: 自動化ワークフロー & プラグイン (2026-05-01 - 2026-05-15)
- [ ] `M-FE-6 Script hook` との統合。
- [ ] Python からレンダリングキューへのジョブ追加 API の拡充。
- [ ] シェルからのヘッドレス実行モード（コマンドライン引数でのスクリプト実行）の検討。

## 🚀 期待される成果
- After Effects の ExtendScript や Blender の Python API に匹敵する強力な自動化環境が手に入る。
- 非 GUI 環境（サーバー等）でのバッチレンダリングが可能になる。

## 🔗 関連マイルストーン
- [M-FE-6 Batch / Macro / Script Entry](MILESTONE_FEATURE_EXPANSION_2026-03-25.md)
- [M-APP-1 Application Cross-Cutting Improvement](MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md)
