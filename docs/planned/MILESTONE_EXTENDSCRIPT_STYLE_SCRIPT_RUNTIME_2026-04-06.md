# Milestone: ExtendScript-Style Script Runtime (M-PY-3)

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

`ScriptRuntime` は軽量 Expression parser/evaluator、JavaScript／ExtendScript style 切替、host snapshot（app／project／composition／selection／thisComp／thisLayer）、`log`／`warn`／`error`、複数 statement の実行、ファイル実行、構文・runtime error の位置情報を実装している。`ArtifactScript` の parser／VM と `ArtifactScriptMenu` の script file 導線も別系統の基盤として存在する。

一方、目標にある `app.project` の実操作、renderQueue／timeline／layer mutation、REPL／console履歴、非同期ジョブ・キャンセル・タイムアウト、実行権限境界、UIの統一結果／stack表示は確認できない。従って host runtime skeleton は部分実装済みだが、ExtendScript-style automation 全体は未完了とする。

## Goal

`ArtifactStudio` に、After Effects の ExtendScript に近いアプリ内自動化スクリプト実行基盤を追加する。

ここで目指すのは純粋な Node.js 互換ではなく、次の用途に強い実用ランタイムです。

- プロジェクト操作
- レイヤー操作
- レンダーやバッチ処理の自動化
- UI からのスクリプト実行
- コンソール実行とファイル実行
- ホスト API を通じた `app / project / selection` へのアクセス

## Positioning

既存の基盤を土台にする。

- `Script.Expression/*` は式評価と診断に使う
- `Script.Engine/*` は軽量 VM / AST キャッシュ / コンテキスト管理に使う
- `Script.Python.Engine` は別系統の高機能自動化として残す
- 本マイルストーンは、UI 統合しやすい ExtendScript 風のホストランタイムを作る

## What This Is Not

- 完全な Node.js 互換ではない
- npm の全エコシステムを再現するものではない
- ブラウザ互換の JavaScript VM をフル実装するものではない

## Target API Shape

最初に欲しいのは、以下のようなホスト API です。

- `app.version`
- `app.project`
- `app.activeDocument` または `app.activeComposition`
- `selection`
- `log()`
- `warn()`
- `error()`
- `openProject()`
- `saveProject()`
- `renderQueue.addJob()`
- `timeline.currentFrame`
- `layer.add()`

## Execution Model

- 1 回限りのスクリプト実行
- ファイルからの実行
- REPL / console 実行
- 非同期ジョブの開始と完了通知
- キャンセル可能な長時間処理
- タイムアウト付き実行

## Phases

### Phase 1: Host Runtime Skeleton

- [ ] スクリプト実行エンジンの責務を定義する
- [ ] `app` オブジェクト相当のホスト公開 API を決める
- [ ] `project / composition / layer / selection` の最小読み取り API を用意する
- [ ] ログ出力とエラー出力を標準化する
- [ ] 単発スクリプトとファイル実行を通す

Phase 1 の実行メモは親文書へ統合済み:

- 親文書へ統合済み

### Phase 2: Interactive Console / Script Surface

- [ ] `Script` メニューから開ける専用コンソールを用意する
- [ ] 入力履歴、複数行入力、再実行を扱う
- [ ] 実行結果を UI に返す
- [ ] エラー位置とスタック情報を表示する

### Phase 3: Automation API Expansion

- [ ] プロジェクト作成 / 読み込み / 保存 / 書き出し API
- [ ] レイヤー追加 / 削除 / 並べ替え / グループ化 API
- [ ] レンダーキュー投入 API
- [ ] 選択中アイテムへの操作 API
- [ ] バッチ処理やマクロからの呼び出し導線

### Phase 4: Integration and Events

- [ ] UI イベントや内部イベントと接続する
- [ ] スクリプトからイベント購読できるようにする
- [ ] 実行中キャンセルと安全な中断を整える
- [ ] スクリプト実行の権限境界を整理する

### Phase 5: Optional JavaScript/TypeScript Frontend

- [ ] 必要なら JS/TS 風フロントエンドを検討する
- [ ] もし導入するなら、ホスト API を共通化したまま差し替え可能にする
- [ ] TypeScript は transpile 前提で扱う

## Risks

- 安易に Node.js 互換を目指すとスコープが膨らみすぎる
- スクリプトから何でも触れると安全性と保守性が落ちる
- UI と Core の責務分離を崩すと、後で修正コストが増える

## Success Criteria

- スクリプトからプロジェクトとレイヤーを操作できる
- 実行結果とエラーが UI で見える
- 小さな自動化タスクを、アプリ内だけで完結できる
- 将来 Python / JS / 独自 DSL のどれを採るかを後から選べる

## Related Milestones

- [docs/planned/MILESTONE_PYTHON_API_SCRIPTING_2026-03-30.md](./MILESTONE_PYTHON_API_SCRIPTING_2026-03-30.md)
- [docs/planned/MILESTONE_SCRIPT_MENU_PY_LOADER_2026-04-02.md](./MILESTONE_SCRIPT_MENU_PY_LOADER_2026-04-02.md)
- [docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md](./MILESTONE_FEATURE_EXPANSION_2026-03-25.md)
- [ArtifactCore/include/Script/Script.ixx](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/ArtifactCore/include/Script/Script.ixx)
- [ArtifactCore/include/Script/Engine/Context/ScriptContext.ixx](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/ArtifactCore/include/Script/Engine/Context/ScriptContext.ixx)
- [ArtifactCore/include/Script/Engine/BuiltinScriptVM.ixx](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/ArtifactCore/include/Script/Engine/BuiltinScriptVM.ixx)
- [ArtifactCore/include/Script/Python/PythonEngine.ixx](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/ArtifactCore/include/Script/Python/PythonEngine.ixx)
- 親文書へ統合済み


---

## Static audit follow-up (2026-07-25)

ScriptRuntime、ScriptContext、BuiltinScriptVM と xecute／xecuteFile、host snapshot、logger、timeout 付き VM 経路を確認した。さらに ArtifactPythonHookManager と WorkspaceAutomation により project／layer／effect／keyframe／playback／render queue 操作の API 基盤が存在する。

一方、ExtendScript 風の専用 Script console、REPL 履歴／複数行再実行、UI への統一結果・stack 表示、スクリプトからのイベント購読、実行権限境界、キャンセルの一貫した UI 導線は確認できない。Python API の一部は placeholder 応答も含むため、Phase 1 は基盤部分実装、Phase 2〜4 は未完了または検証待ち、Phase 5 は未着手として扱う。
