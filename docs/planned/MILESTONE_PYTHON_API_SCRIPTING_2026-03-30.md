# Milestone: Python API & Scripting Console (M-PY-1)

**最終更新:** 2026-08-15

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

## Static Audit (2026-07-25)

計画時点から実装が進み、`ArtifactCore::PythonEngine` に Python 初期化／終了、文字列・ファイル実行、式評価、global 値、C++ function 登録、stdout／stderr callback、interactive console line、search path 管理がある。`CorePythonAPI` は math／color／DSP／system／composition API を登録し、`ArtifactPythonAPI` は選択 layer の rename／cleanup／trim 等を公開する。App 起動時の script scan、Python Hook Manager、Script Menu、WorkspaceAutomation／MCP 経由の自動化経路も確認できる。

一方、計画が想定した pybind11 による `ArtifactCore`／`ArtifactStudio` の型付き Python object export、独立した `ScriptingConsoleWidget`（QPlainTextEdit REPL、syntax highlight、completion）、`app`／`project`／`selected_layers` の直接オブジェクト公開、render queue の完全な Python API、headless CLI 実行、sandbox／権限・実行時間制限、runtime の Python interpreter／output redirect 検証は未確認である。現状は登録関数を中心とした bridge で、型安全な object model と専用 console UI は別途必要である。

判定: **Python 実行基盤と限定 API は実装済み。** Phase 1 の full bridge、Phase 2 の専用 console、Phase 3 の headless／plugin workflow は未完了または未検証である。

## 現行コード監査 (2026-08-15)

`PythonEngine` は pybind11／外部 Python fallback、execute／evaluate、登録関数・global、stdout callback、複数行 console line を持ち、`CorePythonAPI`／`ArtifactPythonHookManager` から限定 API を登録できる。専用 `ScriptingConsoleWidget`、typed object model の全面 export、sandbox／権限・時間制限、履歴・補完、headless CLI の一貫した受入れは現行コードでは確認できない。したがって Python bridge の基盤は進展済みだが、Phase 1 の full bridge と Phase 2〜3 は未完了・runtime 検証待ちとする。
