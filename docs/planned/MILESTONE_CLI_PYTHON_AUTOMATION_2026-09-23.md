# M-CLI-1: CLI・Python 対話実行・自動化の統合

**最終更新:** 2026-09-23
**ステータス:** In Progress（単発 `--command --json`、versioned `--request` / persistent command JSONL、Command IR catalog / validate / execute と stdin JSONL CLI、Python `run` / `eval` / 人向け・JSONL REPL を実装。Windows console 入口保証と実行検証は未完了）

## 目的と完了像

PowerShell などの端末から ArtifactStudio の操作を単発コマンド、コマンドファイル、Python ファイル、対話型 Python で実行できるようにする。呼び出し側が処理結果、診断、終了コードを安定して受け取り、GUI と同じプロジェクト操作を安全に自動化できる状態を完成とする。

重要な利用者として、ArtifactStudio 自身を一つのツールとして呼ぶ AI エージェントを含める。AI が生成した PowerShell / Python バッチから Artifact を起動し、構造化された機能一覧・検証結果・実行結果を受け取り、結果に基づいて次の処理を選べることを目指す。AI のために内部 API を無制限に公開するのではなく、人とエージェントが同じ安定した CLI 契約を使う。

提案する利用形（コマンド名は実装時に `--help` と整合させて確定する）:

```powershell
$r = & .\ArtifactCli.exe project validate .\sample.artifact --json | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw $r.error.message }

& .\ArtifactCli.exe python run .\tools\make_scene.py --project .\sample.artifact
& .\ArtifactCli.exe python repl --project .\sample.artifact
```

`ArtifactCli.exe` は提案名。既存の `Artifact.exe --interactive` / `--script` は互換入口として扱う。CLI から使う対象はまずプロジェクト検証・情報取得・保存、次に Command IR で安全に扱える編集操作と render queue の状態取得とする。動画デコードや新しいレンダリング経路の実装は、このマイルストーンの前提にしない。

## 現状の根拠（2026-09-23、静的確認）

| 区分 | 観測された事実 | 残る課題 |
|---|---|---|
| Artifact CLI | `ArtifactCommandLine.cppm` が `--interactive` / `--script` / `--command` / `--request` / `command-ir` を選択し、コマンドシェルは stderr に診断を出した command を失敗として記録する。単発 command と stdin JSONL stream は共通 envelope を返し、JSON request は schema version 1 と requestId を扱う。`help --json` は usage・説明・副作用区分付きの登録 command catalog を返す。Command IR の JSON request は catalog / validate / execute と構造化結果を扱い、`command-ir -` はプロジェクト session を維持して複数要求を処理する。 | 引数型 schema / project・GPU capability 発見、script の共通 result stream、console subsystem の PowerShell 動作保証は未完了。 |
| Python | `PythonEngine` に initialize、execute、executeFile、evaluate、`pushConsoleLine`、出力 callback、最終エラーがある。CLI に Python `run` / `eval` / 人向け `repl` と app API 登録を接続し、`repl --jsonl` は JSON request / response と session state を扱う。WorkspaceAutomation bridge は引数を JSON 値で受け渡し、戻り値も JSON から Python 値へ復元する。Python から command vocabulary、validate、execute を呼べる。 | Python CLI は未ビルド・未実行。外部 Python fallback では Artifact C++ API と永続 REPL を提供しない。対話 API は bool と `hasError()` の組合せで、公開結果型への整理が残る。 |
| 自動化 | `CommandResult` は success、valid、executed、errorCode、diagnostics 等を持ち、App 側に `CommandIRExecutor` がある。 | CLI シェルは別の JSON 直接操作を含み、Command IR・GUI の結果と Undo 境界の統一は未完了。 |
| 起動・出力 | `Artifact/CMakeLists.txt` は GUI 本体を `add_executable(Artifact WIN32)` で作る。`AppMain.cppm` に CLI モード時の親 console 接続と CRT 標準 handle の再結合を追加した。 | Windows PowerShell / ConPTY からのパイプ・入力・待機・`$LASTEXITCODE` は未実行で保証未確認。 |
| アプリ内端末 | `PowerShellWidget` は外部 shell を QProcess で起動する UI。 | Artifact の CLI と Python REPL の実行・結果契約とは別の責務。専用 Script Console 計画とも混同しない。 |

現状はコード読取による観測であり、ビルド・実行による検証済み判定ではない。作業ツリーには本件以外の既存変更があるため、この文書は既存ファイルの完了判定を書き換えない。

## 共通契約

1. **入口:** GUI 起動、CLI 単発、CLI コマンドファイル、Python `run` / `eval` / `repl` のモードを明示的に分離する。`--help` とオプション検証は GUI 初期化なしで動く。PowerShell の引用符・空白・日本語パスを扱えるよう argv の文字コードと path 解決を統一する。
2. **結果:** 各操作は成功、失敗、継続待ち、取消を区別する内部結果型を返す。CLI ハンドラの stderr 書込みだけで成功扱いしない。Python 例外、登録 API のエラー、保存失敗、子プロセス異常終了も結果へ反映する。
3. **機械出力:** `--json` の単発実行では stdout に UTF-8 の JSON オブジェクトを一つだけ出す。最低限 `schemaVersion`、`ok`、`command`、`result`、`error`（`code` / `message` / `details`）、`warnings` を持たせる。進捗・ログ・診断は stderr に分け、JSON を混ぜない。対話・複数要求の機械モードは一行一結果の JSON Lines とし、人向けプロンプトを混入させない。
4. **終了コード:** 現行の `0` 成功、`1` 操作・検証失敗、`2` 引数・スクリプトファイル入力エラーを維持する。`3` は既存 `render` 未実装を含む機能利用不能の意味として整理する。中断は `130` を候補とし、Windows 停止手段との整合を受入時に決める。JSON の `ok` と終了コードは必ず一致する。
5. **プロジェクト状態:** 読取と変更を分ける。変更は既存 service / Command IR の検証・Undo・保存規約を通し、GUI の直接ウィジェット呼び出しや CLI 独自の project JSON 書換えへ機能を増やさない。headless で必要な service と GUI 必須 service を明示する。
6. **Python の信頼境界:** 最初はユーザーが指定したローカルスクリプトを実行する trusted mode として説明する。現行 PythonEngine を「sandbox 済み」と表示しない。制限付きスクリプトを必要とする場合は別プロセス・権限・タイムアウト・資源制限の設計を別途行う。
7. **AI / agent caller:** コマンド一覧・引数 schema・機能利用可否を CLI から発見できる。呼出し側が shell quoting に依存しない JSON request file または stdin request を使える。AI が作ったバッチはまず構文・引数・対象 project を検証でき、実行結果を次の判断に使える。dry-run は副作用を発生させない操作にだけ提供し、非対応操作は明示する。

## 実装フェーズと受入条件

### P0: 実行可能な console 入口と結果契約

- 既存 `Artifact.exe` の CLI モードで親 console へ接続する方式を実装した。PowerShell / ConPTY で失敗する場合に限り、独立 `ArtifactCli.exe` 方式を評価する。GUI 本体との共通 parser / dispatcher は再利用する。
- 単発 `project validate` / `project stats` を提供し、実際の project path、存在しない path、不正 JSON、空白・日本語 path を扱う。
- 既存 `--script` の各コマンドを共通結果型へ移し、失敗した行と終了コードを一致させる。複数行では最後の成功で前の失敗を消さない。
- `--json` の stdout は単一 JSON、stderr は診断のみ。PowerShell `ConvertFrom-Json` と `$LASTEXITCODE` で結果を判定できる。
- AI agent が使うための機械向け help / command description と副作用区分を整備する。shell quoting を避ける `--request <file>` / `--request -` の version 1 は `schemaVersion`、任意の `requestId`、単一 `command` string を受け、呼出結果へ `requestId` を戻す。stdin 形式では1プロセス内でコマンド状態を保つ。
- PowerShell などから Command IR を直接呼べる `command-ir <file|-> [--project <file>]` を追加する。version 1 request は operation `catalog` / `validate` / `execute` を受け、validate / execute は command object を要求し、execute は project を一度ロードしてWorkspaceAutomation executorを使う。`saveProject:true` を指定した場合は成功後に既存 project exporter で保存し、保存成否も戻り値へ含める。標準入力の JSON Lines は request ごとの失敗を返しつつ stream を継続し、最後の終了コードへ集約する。

**完了判定:** 新規 PowerShell プロセスから成功・不正入力・実行失敗を区別でき、CLI 起動で GUI window が出ず、既存 `--interactive` / `--script` の代表操作が継続利用できる。

### P1: Python ファイル・式・対話実行

- `python run <file>`、`python eval <expression>`、`python repl` を用意する。Python 初期化と `CorePythonAPI` / App 側 API 登録の可否をモードごとに明示する。
- PythonEngine と Artifact API を CLI で初期化し、project load・UTF-8 script 読込・評価結果を接続した。外部 Python fallback では in-process Artifact API 不在を警告し、永続 REPL を unavailable として返す。
- WorkspaceAutomation Python bridge の引数は JSON 値で型を保ち、戻り値は JSON を Python dict / list / scalar に復元する。非 JSON 値と NaN / Infinity は境界で拒否する。
- `artifact.core.automation.command_vocabulary()` / `validate_command(dict)` / `execute_command(dict)` を追加し、AI生成Pythonスクリプトから既存Command IR executorを発見・検証・実行できるようにした。
- REPL は同一プロセス内の変数・import・選択 context を保持し、複数行ブロック、空行確定、EOF、例外後の続行、`exit()` を扱う。継続待ちと失敗を異なる状態として表す。
- stdout / stderr / 式の返り値 / Python 例外を分ける。人向け REPL ではプロンプトとトレースバックを表示し、機械向け `--jsonl` REPL は状態を保ちながら request ごとの結果と EOF 時の不完全入力を返す。
- Python 利用不可、初期化失敗、ファイル不存在、構文エラー、実行例外の各終了理由を共通契約へ変換する。Python script が失敗した場合、`$LASTEXITCODE` が 0 にならない。

**完了判定:** PowerShell で `.py` の成功結果と例外を取得できる。対話入力の複数行・状態保持・例外からの復帰が成立し、プロジェクトを開いたモードでは許可された API が使える。

### P2: アプリ操作の統一とバッチ運用

- CLI と Python からの変更操作を既存 Command IR / service に接続し、同じ入力なら GUI と同じ結果・Undo 単位・dirty state を返す。
- 読取、変更、保存、render queue 投入／状態取得を段階的に公開する。GPU を要する処理は既存の GPU/Diligent 経路を優先し、headless 条件を明示する。
- バッチの部分成功、失敗時の継続／停止、出力ファイルの確定、取消後の後片付けを契約化する。非同期 job は投入成功と処理完了を別の結果にする。
- `--help`、終了コード一覧、PowerShell サンプル、JSON schema version と互換方針を文書化する。
- AI が生成したバッチのために機能 discovery、事前検証、対応操作の dry-run、request / result の相関 ID を提供する。未対応機能や副作用の有無を推測させず、機械出力に capability として返す。
- 1 回の CLI 起動で完結する呼出しに加え、必要な場合は JSON Lines で複数要求を続けて送り、要求ごとの結果境界を保持する。対話 Python の REPL 状態を使う場合も、接続終了時に状態を持ち越さない。

**完了判定:** 保存を含む代表的な編集ワークフローを PowerShell と Python から再現でき、失敗時に project・Undo・出力ファイルが規定どおりの状態に戻る。

## 受入マトリクス

| シナリオ | 期待結果 | 確認方法（実装段階で実施） |
|---|---|---|
| PowerShell から単発成功 | JSON が一件、終了コード 0、余計な stdout なし | 新規 console プロセスで pipeline / `ConvertFrom-Json` |
| 入力・操作・Python 例外 | JSON と非 0 終了コードが一致、診断は stderr | 各失敗 fixture を個別プロセスで実行 |
| Python REPL の複数行と例外 | 継続待ち表示、変数維持、例外後に次の入力が可能 | 端末でブロック入力・例外・再入力 |
| Command IR JSONL の複数要求 | request ごとのJSON応答、同一プロセス内のproject state保持、失敗集約と opt-in save 結果 | PowerShell pipelineでcatalog / validate / executeを送り、requestId・最終終了コード・再読込後の保存内容を照合 |
| 空白・日本語 path、UTF-8 | argv、出力、script 読込で文字化けしない | PowerShell から実 path で実行 |
| 変更操作と保存 | 同じ Command IR 結果・Undo・dirty state | GUI 操作と CLI 操作の project 再読込比較 |
| 非同期処理・中断 | job ID、進捗、完了と取消を区別 | queue 状態取得と停止後の出力確認 |
| AI が生成したバッチ | capability を読んで事前検証し、実行結果から分岐できる | エージェント相当の呼出し元が request を送り、schema・request ID・JSON 結果を照合 |

AGENTS.md に従い、この計画作成時はビルド、CMake、テストを実行していない。実装後の実機受入にはユーザーの明示指示が必要。

## 既存計画との関係・衝突

- [Python API & Scripting Console](MILESTONE_PYTHON_API_SCRIPTING_2026-03-30.md): Python bridge と API 拡張の計画。本書は端末からの実行と戻り値を所有する。
- [Script Console (REPL)](MILESTONE_SCRIPT_CONSOLE_2026-06-16.md): アプリ内 widget の計画。本書の端末 REPL と Python 実行部分を共有する。旧文書の新規 signal / slot 許容記述は現行 AGENTS.md の全面禁止と矛盾するため採用しない。旧文書の単純な Python module 禁止による sandbox 案も実装済み安全性とはみなさない。
- [Command IR / Automation Foundation](MILESTONE_COMMAND_IR_AUTOMATION_FOUNDATION_2026-06-28.md): 変更操作の検証・Undo・結果型の正本。CLI 独自の編集意味論を追加しない。
- [Terminal Shell](MILESTONE_TERMINAL_SHELL_2026-04-06.md): アプリ内の OS shell UI。CLI 実行ファイルの契約とは別に進める。
- [CLI interactive shell](../../Artifact/docs/CLI_INTERACTIVE_SHELL.md): 現行の利用法。P0 で互換性を確認し、完成時に実際の引数・結果へ更新する。

## AI からツールとして使う運用例

1. エージェントは `--describe` 相当の機械向け情報から CLI version、schema version、利用可能なコマンド、読み書き範囲、headless / GPU 条件を取得する。
2. 生成したバッチは `validate` または dry-run 対応操作で project と引数を検査する。dry-run 非対応の変更を dry-run 可能と誤認しない。
3. 実行時は PowerShell の文字列連結で複雑な引数を組み立てず、JSON request file / stdin を渡す。Artifact は request ID と共通 result envelope を返す。
4. エージェントは終了コードと JSON の `ok` / `error.code` を照合し、失敗時は診断を利用して修正・再試行の判断を行う。非同期 job は受付結果と完了結果を別 request として追跡する。

このフローは特定の AI 製品への直接統合を要件にしない。CLI / JSON 契約を、PowerShell、Python、将来の MCP / agent adapter が共有できる境界として設計する。

## 実装上の注意

`PythonEngine::pushConsoleLine()` は bool で継続待ちを返す設計だが、fallback 側では完了したコードの `execute()` 失敗も bool に重ねている。P1 では呼出側が `hasError()` に頼るだけでなく、実行結果型を明確にしてこの曖昧さを解消する。`Artifact.exe` の `WIN32` 指定と CLI stdout の実動作も P0 最初の確認対象とする。

本マイルストーンは計画文書であり、上記のコマンド例は現時点で実行可能な機能一覧を示すものではない。
