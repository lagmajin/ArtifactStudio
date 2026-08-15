# マイルストーン: Session Ledger / Recovery Workspace

> 2026-04-09 作成

## Update 2026-08-15

- `ArtifactCore::SessionLedger` と `ArtifactRenderQueueService::sessionLedger()` は存在し、project opened／render job／failed task／crash 等の typed entry と crash 記録の入口を持つ。`ArtifactAutoSaveManager` の recovery point、revision ledger、render queue checkpoint／job 状態も個別に存在する。
- ただし、専用の Recovery Workspace UI、recent／failed／recoverable の切替、project・render・recovery の相互参照、台帳の永続化／再起動復元は確認できない。現状は **台帳モデルと記録入口の部分実装、統合 UI／運用導線は未完了** と判定する。

## Update 2026-08-15 — Phase 1/2 implementation

- `SessionLedger` に schema version 付き JSON serialize／deserialize と `saveToFile()`／`loadFromFile()` を追加した。既存の typed entry、recovery point、session metadata を同一ファイルへ保存できる。
- `AppMain` で起動時ロード、終了時保存を接続した。保存先は AppData 配下の `Session/session-ledger.json` とし、破損・未存在時は空の現行セッションを継続する。
- `ArtifactRecoveryWorkspaceWidget` を追加し、台帳の recent／failed／recoverable 切替、件数表示、recovery point 件数表示、手動 refresh を実装した。右側 dock に遅延生成で登録し、初期表示は非表示とした。
- 選択項目から recovery snapshot の open、failed render job の resume を既存の Project Manager／Render Queue 経路へ接続した。project・render・recovery の詳細な相互参照と runtime 受入れは未検証。

## 目的

project / render job / failed task / recovery point を 1 つの作業台帳にまとめ、アプリ全体の復旧導線を整理する。

この milestone は、単なる autosave ではなく「今何をしていて、何が失敗し、どこから戻れるか」を App 側で見える化することを狙う。

---

## 背景

現在のアプリでは、保存・レンダー・クラッシュ復帰・最近の作業が別々の導線に分かれている。

その結果、

- 長時間 render の途中状態が追いにくい
- 失敗した job の再開が断片的
- crash 後にどこまで戻せるか分かりにくい
- recent project / job / recovery point が散らばる

という問題がある。

---

## 方針

### 原則

1. autosave とは別に session の履歴を持つ
2. project / render / recovery を同じ台帳に並べる
3. 失敗理由と復帰可能点を明示する
4. UI は「履歴を見る場所」と「再開する場所」を分ける
5. 既存の project save flow を壊さない

---

## Phase 1: Session Ledger Model

### 目的

作業の履歴を typed に記録する。

### 作業項目

- session id
- open project list
- render job history
- failed task list
- recovery point list
- dirty / saved / exporting state

### 完了条件

- 1 セッションの履歴を追える
- App 側から台帳を読める

---

## Phase 2: Recovery Points

### 目的

復旧可能な地点を明示する。

### 作業項目

- checkpoint 作成
- autosave と recovery point の分離
- crash 後の resume candidate 表示
- failed render の再実行導線

### 完了条件

- 少なくとも 1 つの復旧候補を提示できる
- crash 後に戻る地点が分かる

---

## Phase 3: Workspace UI

### 目的

台帳を App の画面として扱う。

### 作業項目

- session ledger view
- recent / active / failed / recoverable の切り替え
- quick reopen
- render / project / recovery の相互参照

### 完了条件

- 1 画面で現在の作業状況が把握できる
- 失敗からの復帰が迷いにくい

---

## Non-Goals

- 完全な version control 代替
- 共同編集の同期基盤
- 全プロジェクトの自動バックアップクラウド化

---

## Related

- `docs/planned/MILESTONE_RENDERING_PERFORMANCE_2026-03-28.md`
- `docs/planned/MILESTONE_MULTI_FRAME_RENDERING_2026-04-09.md`
- `docs/planned/MILESTONE_EXPORT_REVIEW_SHARE_2026-03-27.md`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`

## Current Status

2026-08-15 現行コードでは、Core の SessionLedger と autosave／revision／render queue の個別基盤に加え、App 起動時の ledger 読込・終了時保存、recovery directory の表示、Recovery Workspace の lazy dock 表示まで実装済み。App 側の一元表示と永続化は前進したが、復旧候補からの quick reopen／render resume、project／render／recovery の相互参照、実クラッシュ後の runtime 受入れは未検証。
App 側の作業履歴と復旧導線をまとめる基盤 milestone として扱う。

## Update 2026-08-15

M-QA-3 の現行コード照合に合わせ、Session Ledger／Recovery Workspace の実装済み範囲を反映した。今回の確認では追加コード変更は不要だった。残課題は recovery action の実接続とクラッシュ後の end-to-end 検証である。
