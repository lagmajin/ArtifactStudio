# マイルストーン: Background Utility Worker Process

> 2026-04-22 作成

## 目的

サムネイル生成、waveform 生成、proxy 生成、素材メタデータ抽出などの「雑用」を、将来的に UI / render 本体から分離できる専用ワーカープロセスへ整理する。

このマイルストーンの狙いは次の 4 点。

- UI スレッドや editor 操作と無関係な重い雑用を隔離する
- render queue 本体と雑用系前処理を責務分離する
- 後から job を増やしても破綻しない共通 job 契約を作る
- まずは in-process 互換のまま job surface を揃え、最後に out-of-process へ移す

---

## 対象にする雑用

初期ターゲットは次の通り。

- サムネイル生成
- waveform 生成
- proxy 生成
- 素材メタデータ抽出
- フォント一覧キャッシュ
- 画像シーケンス走査
- メディアの事前検証
- render queue 前のプリフライト
- 自動保存の圧縮 / スナップショット整理
- ログ収集
- クラッシュレポート整形
- AI 補助の前処理
- キャッシュ掃除
- 連番欠落チェック

---

## 先に決める原則

1. いきなり別 EXE へ飛ばさない
2. まずは `job request -> worker runtime -> result` の共通契約を作る
3. 既存呼び出し点は adapter で包む
4. UI は「直接処理」ではなく「job を投げる側」に寄せる
5. 重い I/O と CPU 前処理は render 本体とは別キューで扱う
6. 将来のプロセス分離を前提に、request/result は serialize 可能な構造にする

---

## このマイルストーンで作る中核

### 1. Utility Job Contract

- `UtilityJobKind`
- `UtilityJobId`
- `UtilityJobPriority`
- `UtilityJobRequest`
- `UtilityJobResult`
- `UtilityJobProgress`
- `UtilityJobError`
- `UtilityJobCancellationToken`

### 2. Worker Runtime

- `UtilityWorkerRuntime`
- `UtilityWorkerScheduler`
- `UtilityJobQueue`
- `UtilityWorkerMetrics`
- `UtilityWorkerTaskRegistry`

### 3. Process Boundary 準備

- `UtilityWorkerClient`
- `UtilityWorkerProtocolVersion`
- `UtilityWorkerEnvelope`
- `UtilityWorkerSpawnPolicy`
- `UtilityWorkerHeartbeat`

### 4. Host Integration

- asset browser / timeline / project import / render queue / autosave から job を投げる薄い facade
- diagnostics から pending / running / failed を見える化するビュー

---

## 対象外

この文書では次はまだやらない。

- render 本体の完全別プロセス化
- GPU render worker の切り出し
- 分散レンダリング
- sandbox / 権限制御の完全実装
- crash recovery の完全自動再実行

---

## 想定アーキテクチャ

### Phase 1-2

同一プロセス内だが、API は将来の外部ワーカー前提で固定する。

`UI / Service -> UtilityJobFacade -> UtilityWorkerScheduler -> Worker Task`

### Phase 3 以降

`UI / Service -> UtilityWorkerClient -> UtilityWorkerProcess -> Task Registry`

---

## Job 分類

| 分類 | 代表タスク | 性質 | 優先度 | 備考 |
|---|---|---|---|---|
| Media Preview | サムネイル、waveform、画像シーケンス走査 | I/O + 軽中 CPU | 中 | editor 体感に効く |
| Media Preparation | proxy、メタデータ抽出、事前検証 | I/O + CPU | 中 | import 後 / idle 時向き |
| Render Preparation | プリフライト、連番欠落チェック | 軽中 CPU | 高 | render queue 前に必要 |
| Maintenance | autosave 整理、キャッシュ掃除、ログ収集 | I/O 中心 | 低 | idle / shutdown 前向き |
| Diagnostics | クラッシュレポート整形、ログ収集 | I/O + text processing | 中 | supportability 向上 |
| AI Preprocess | 素材索引化、特徴抽出、下処理 | CPU + I/O | 低〜中 | 後で増えやすい |

---

## フェーズ構成

## Phase 1: Job Contract 固定

### 目的

雑用を共通の job surface へ載せる。

### やること

- `UtilityJobRequest/Result/Progress/Error` を定義
- job kind と priority を定義
- cancel / retry / timeout / dedupe key の概念を入れる
- result を serialize 可能な DTO に寄せる

### 完了条件

- サムネイル / waveform / proxy / metadata の 4 系統を同一 API で表現できる
- UI / service が job 実装詳細を直接知らない

### リスク

低い。まずは型定義と facade だけで進められる。

---

## Phase 2: In-Process Utility Worker Runtime

### 目的

まだ別プロセス化せず、専用スケジューラで雑用を隔離する。

### やること

- `UtilityWorkerScheduler` を追加
- `interactive`, `background`, `maintenance` の 3 queue を分離
- 並列数制限、coalesce、dedupe、cancel を実装
- metrics と簡易 diagnostics を追加

### 完了条件

- thumbnail / waveform / metadata / sequence scan が新 scheduler を通る
- render queue 本体と別 queue で動く

### リスク

低〜中。スレッド競合とキャンセル整備が必要。

---

## Phase 3: Host Facade 置換

### 目的

既存の各所バラバラ実装を facade 経由に寄せる。

### やること

- `ThumbnailService` 相当の入口を job facade 化
- waveform, proxy, metadata, preflight も同様に置換
- autosave maintenance / cache cleanup も同型で統一

### 完了条件

- 主要雑用の開始点が `UtilityJobFacade` 経由に揃う
- 呼び出し元が thread 実装や future 実装を知らない

### リスク

中。既存の暗黙依存が出やすい。

---

## Phase 4: External Worker Protocol

### 目的

別プロセス化のための protocol と envelope を作る。

### やること

- `UtilityWorkerEnvelope` 定義
- request/result/progress の versioning
- stdio / named pipe / local socket のいずれかを一次採用
- heartbeat / shutdown / restart を定義

### 完了条件

- in-process runtime と同じ request/result で out-of-process transport を差し替え可能
- protocol version mismatch を検知できる

### リスク

中。serialize 境界の設計が重要。

---

## Phase 5: Dedicated Utility Worker Process

### 目的

雑用専用プロセスを導入する。

### やること

- `ArtifactUtilityWorker(.exe)` を追加
- task registry を worker 側へ移す
- parent 側は client / monitor / restart policy を持つ
- idle shutdown / startup warmup を追加

### 完了条件

- thumbnail / waveform / metadata / preflight が worker process 上で動く
- host 側から見る API は変わらない

### リスク

中〜高。パス / 依存 DLL / 環境差分の扱いが必要。

---

## Phase 6: Maintenance / Diagnostics / AI 前処理拡張

### 目的

雑用ワーカーを恒久的な「裏方基盤」に育てる。

### やること

- autosave 整理
- cache cleanup
- log collection
- crash report formatting
- AI preprocess
- sequence gap check
- render queue preflight

### 完了条件

- 雑用カテゴリの大半が worker process に載る
- diagnostics から成功率 / 失敗率 / 平均時間を見える

### リスク

中。job 増加による肥大化を registry で抑える必要がある。

---

## 実装順の推奨

| 優先 | タスク | 理由 |
|---|---|---|
| 1 | サムネイル生成 | 最も UI 体感に効き、独立しやすい |
| 2 | waveform 生成 | I/O と CPU の分離検証に向く |
| 3 | 素材メタデータ抽出 | import 時の雑用代表 |
| 4 | 画像シーケンス走査 | 事前検証系の核になる |
| 5 | render queue 前プリフライト | render 本体分離の前段として重要 |
| 6 | proxy 生成 | 長時間 job の制御検証に向く |
| 7 | キャッシュ掃除 / autosave 整理 | maintenance 系を乗せる |
| 8 | ログ収集 / crash report 整形 | diagnostics 連携 |
| 9 | AI 補助の前処理 | 最後に追加しやすい |

---

## 実装表

## 実装表 A: 基盤

| ID | 項目 | 内容 | 依存 | 完了条件 |
|---|---|---|---|---|
| WKR-001 | Job kind 定義 | utility job 種別列挙 | なし | enum と文字列表現が安定 |
| WKR-002 | Job request/result DTO | serialize 可能 DTO | WKR-001 | 主要 4 job を表現可能 |
| WKR-003 | Progress / error 契約 | 進捗、失敗、retryable | WKR-002 | UI で共通表示可能 |
| WKR-004 | Scheduler | queue / priority / cancel | WKR-002 | in-process 実行可能 |
| WKR-005 | Metrics | pending, running, avg time | WKR-004 | diagnostics 表示可能 |
| WKR-006 | Facade | host 側の統一入口 | WKR-004 | 呼び出し元が統一 API 利用 |
| WKR-007 | Protocol envelope | request/result versioning | WKR-002 | process 分離準備完了 |
| WKR-008 | Worker client | 起動 / 送受信 / heartbeat | WKR-007 | external mode へ差替可能 |
| WKR-009 | Worker process | 専用 EXE / task registry | WKR-008 | 主要 job が別プロセスで動作 |

---

## 実装表 B: ジョブ別

| Job | Phase 1 | Phase 2 | Phase 3 | Phase 4+ |
|---|---|---|---|---|
| Thumbnail | request/result 定義 | scheduler 化 | asset browser / project view 置換 | process 分離 |
| Waveform | request/result 定義 | scheduler 化 | timeline / audio panel 置換 | process 分離 |
| Proxy | request/result 定義 | scheduler 化 | media pipeline 置換 | long job 制御 |
| Metadata Extract | request/result 定義 | scheduler 化 | import / inspector 置換 | process 分離 |
| Font Cache | DTO 定義 | maintenance queue | app startup / text tools 統合 | background refresh |
| Sequence Scan | DTO 定義 | scheduler 化 | import / validation 統合 | gap check 連携 |
| Media Preflight | DTO 定義 | scheduler 化 | import 前 validation | render preflight 連携 |
| Render Preflight | DTO 定義 | high priority queue | render queue 統合 | worker process 化 |
| Autosave Maintenance | DTO 定義 | maintenance queue | autosave service 統合 | idle policy |
| Log Collect | DTO 定義 | maintenance queue | diagnostics 統合 | crash pack 添付 |
| Crash Report Format | DTO 定義 | maintenance queue | crash path 統合 | upload 前整形 |
| AI Preprocess | DTO 定義 | background queue | AI tooling 統合 | feature cache |
| Cache Cleanup | DTO 定義 | maintenance queue | cache manager 統合 | policy 化 |
| Sequence Gap Check | DTO 定義 | scheduler 化 | import / render preflight 統合 | worker process 化 |

---

## 実装表 C: 優先度とキュー方針

| Queue | 主用途 | 例 | 並列度の目安 | 備考 |
|---|---|---|---|---|
| Interactive | 体感に直結 | thumbnail, waveform 先頭可視範囲 | 小 | latency 優先 |
| Background | 後ろで進める | metadata, sequence scan, AI preprocess | 中 | throughput 優先 |
| Maintenance | idle / low priority | cache cleanup, autosave cleanup, log pack | 小 | 中断しやすくする |
| Long Running | 長時間専用 | proxy build, heavy preflight | 1〜小 | starvation 回避 |

---

## 実装表 D: 依存境界

| 境界 | 置く場所 | 理由 |
|---|---|---|
| DTO / job contract | `ArtifactCore` 寄り | UI 非依存・再利用性重視 |
| scheduler / runtime | `ArtifactCore` または app service 層 | host 非依存な基盤にしやすい |
| facade / orchestration | `Artifact` service 層 | project / asset / render queue と接続 |
| diagnostics UI | `Artifact` / `ArtifactWidgets` | 可視化責務 |
| external worker executable | app 配下の別 target | 将来の完全分離の核 |

---

## 受け入れ条件

- UI から見て雑用が「直接関数呼び出し」ではなく job として扱われる
- 最低 4 job（thumbnail / waveform / metadata / sequence scan）が共通 runtime へ載る
- queue 別に metrics が見える
- cancel / retry / dedupe の最小セットがある
- external worker へ差し替える protocol の骨格が定義済み

---

## 最初の実装スコープ提案

最初の 2 週間相当はこれで十分。

1. `UtilityJobKind/Request/Result/Progress`
2. `UtilityWorkerScheduler`
3. thumbnail job 移行
4. waveform job 移行
5. metadata extract job 移行
6. sequence scan job 移行
7. diagnostics に pending/running/failed を追加

これで「雑用を雑用ワーカーへ集める入口」ができる。

---

## 将来の拡張

- utility worker を複数プロセスへ分割
  - media worker
  - maintenance worker
  - AI preprocess worker
- remote worker / farm 連携
- sandbox / capability 制御
- per-job resource budget
- cache warmup / predictive scheduling

---

## まとめ

このマイルストーンは、render 本体をいきなり外へ出す前に、雑用だけを先に整理して分離可能にするための基盤整備である。

順番としては次が安全。

- contract を作る
- in-process scheduler に寄せる
- facade で各所を統一する
- protocol を作る
- 専用 worker process に出す

この順なら、今の実装を大きく壊さずに「雑用専用ワーカー」へ進められる。