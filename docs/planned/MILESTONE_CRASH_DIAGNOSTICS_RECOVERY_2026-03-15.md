# M-DEV-1 Crash Diagnostics & Recovery (2026-03-15)

**最終更新:** 2026-08-15

**現状:** 診断収集・クラッシュ記録・safe mode 起動入口は実装済み。完全なダンプ／スタック解決、復旧 UI、実クラッシュ QA は未完了。

## 2026-08-15 現行コード照合

- ✅ `CrashHandler::captureStackTrace()` は Windows の `CaptureStackBackTrace` を使ったアドレス列収集を実装済み。symbol 解決や minidump は未実装のまま。

- ✅ `CrashHandler` は Windows の unhandled exception filter を install し、`crash_reports` へ timestamp 付き report を保存する。起動時の pending report ingest と `TraceRecorder`／`SessionLedger` への記録も `AppMain` から接続されている。
- ✅ `Core.Diagnostics.CrashReportParser` は保存 report を `DiagnosticSnapshot` へ変換し、`CoreDiagnostic.Test.cppm` に parser／ingest の contract test がある。
- ✅ `Trace` は crash／thread／event の履歴を JSON 化でき、App Debugger／Trace Timeline が直近 crash と履歴を表示・出力する。render path には任意の crash trace logging もある。
- ✅ `--safe-mode` の起動フラグ入口は存在する。
- ⚠️ `CrashHandler::captureStackTrace()` は現状プレースホルダー相当で、完全な symbolized stack／minidump、主要オブジェクトの安全な snapshot、既知クラッシュの再現 QA は未達。
- ⏳ safe mode のユーザー向け recovery guidance、診断パッケージ送信 UI、実クラッシュからの end-to-end 検証は未完了。

目的
- アプリケーションで発生するクラッシュの根本原因特定を容易にし、ユーザに安全な回復手順を提供するための診断基盤とワークフローを整備する。

期待される成果物
- クラッシュ発生時に自動で回収される診断パッケージ（スタックトレース、スレッド一覧、主要オブジェクトの軽量スナップショット、環境情報）。
- 例外／シグナルハンドラによる最小限のダンプ生成とログの整理。
- セーフモード起動フラグとユーザ向け回復ガイダンス画面。

主要タスク
1. 既存のログ出力・クラッシュダンプ収集経路を調査してドキュメント化する。
2. シグナル（Windows: structured exception / SEH）と std::terminate フックでのハンドリングを統一し、スタックトレース取得を実装する（軽量）。
3. Project / Container / Asset 等の重要オブジェクトについて、診断用に安全に取得できる要約スナップショット関数を追加する。
4. 診断パッケージの保存先と命名規則を決め、ユーザ向けの「ログを送信」UIを用意する。
5. セーフモード起動（設定や一部機能を無効化）と、そのフローを実装する。
6. QA 用の再現手順と回帰テストを追加する（手動／自動）。

受け入れ基準
- クラッシュ発生時に診断ファイルが所定の場所へ保存されること。
- 保存された診断パッケージを使って少なくとも 1 件の既知クラッシュが再現可能であること。
- セーフモードでアプリが起動し、最低限の編集操作が行えること。

見積り
- 4～12時間（初期段階で基本的な診断パッケージとセーフモードを実装する想定）。

備考
- Windows/MSVC 環境固有の SEH との連携や、外部クラッシュレポートサービス統合（任意）を今後検討する。
