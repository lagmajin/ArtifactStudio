# MILESTONE: Detached Task / Detached Session

**最終更新:** 2026-09-20
**ステータス:** Not Started
**識別子:** M-DETACH-1

## 1. 目的

現在のビュー・選択・フォーカスを奪わずに作業を実行できる **Detached Task** の基盤を作る。

利用者がコンポジションで作業している最中に、「新規コンポジションを」のような操作を画面切り替えなしで実行できるようにする。第1段の入力はテキストとし、音声入力（ASR）を後段で同じ入口へ差し込む。

対象とする作業は次の3種。

| 種別 | 内容 | 第1段 |
|---|---|---|
| ImmediateCommand | 新規コンポジション、レイヤー追加、プロパティ設定など短時間の操作 | 対象 |
| AiJob | AI が複数コマンドを連続実行する作業 | 対象 |
| LongJob | レンダー、プロキシ生成、書き出し、取込 | render queue 1 本のみ |

### 非目標（この milestone では扱わない）

- 音声入力そのもの（マイク取得・ASR・TTS）。別 milestone とする
- 複数コンポジションの真の同時編集（`ArtifactActiveContextService` は単一コンテキスト）
- Out-of-process worker 化（`MILESTONE_BACKGROUND_UTILITY_WORKER_PROCESS_2026-04-22` の領域）
- ステータスバーへの通知 badge 追加（`docs/design/status-bar/README.md:22-24` が禁止）

## 2. 決定事項

| 項目 | 決定 |
|---|---|
| 第1段の主眼 | 浮動トレイを先に作る。音声は後段 |
| 「画面切り替えなし」の意味 | 現在のビュー・選択・プレイヘッドを維持したまま実行する |
| 第1段の投入口 | テキスト入力（コマンド / AI の2モード）。ASR は後から同じ入口へ |
| トレイの表示 | タスク投入で自動展開（フォーカス非奪取）→ 全完了で畳む |
| `switch_composition` / `set_playback_state` | Detached では実行せず拒否する |
| 実行順序 | まず全部直列（書き込みレーンを 1 本に固定） |
| トレイのトグル既定キー | 未割当て（空） |
| 承認が必要なとき | トレイに滞留させる（非モーダル）。モーダルは出さない |
| `AskEveryTime` の ReadOnly | 常に自動実行 |
| 承認ポリシーの定義 | 共有モジュールへ抽出し AI Cloud と Detached で同じものを使う |
| Undo | コマンドごとに 1 ステップ（タスク単位のまとめは行わない） |
| AI モード | 第1段では配線しない（トレイに無効ボタンとして置く） |

## 2.1 実装状況（2026-09-20）

第1段のうちコマンド実行とトレイまでを実装した。ビルド・実機確認は未実施（利用者指示待ち）。

| Phase | 状態 | 内容 |
|---|---|---|
| 0 | 完了 | 本マイルストーン文書 |
| 1 | 完了 | `BackgroundTaskWorkerPool::SetSnapshotSink`（未使用の `SetEventBus` を削除）、`CommandIR::isReadOnlyType`、`Artifact.AI.AgentApprovalPolicy` の新設と `ArtifactAICloudWidget` の共有定義化、`CompositionRenderController::isInteractionBusy` |
| 2 | 一部完了 | `ArtifactDetachedTaskService`（コマンド実行のみ）。AI モードと長時間ジョブのアダプタは次段 |
| 3 | 一部完了 | `ShortcutId::ViewDetachedTasks`（既定は未割当て）、浮動トレイ、View メニューのトグル。**ゲートのプローブ登録が未了** |
| 4 | 継続 | ViewCoupled の拒否と削除系の承認までは実装済み。LongJob の render queue 連携は次段 |
| 5 | 未着手 | 音声入力 |

実装中に判明した計画の前提違い（詳細は `Insight.md` の 2026-09-20 項目）。

1. `AIClient` のツールループは承認を一切通さない。承認を尊重する非モーダルな AI ループは現存しないため、Detached の AI モードは `AIClient` を流用できない。AI モードは承認設計と合わせて次段へ回した。
2. コマンド実行は Qt ウィジェット・Undo・各サービスを触るため **UI スレッド必須**。`BackgroundTaskWorkerPool` の worker thread では実行できないため、サービスは自前の FIFO と QTimer で UI スレッド実行する。`SetSnapshotSink` はスレッド安全なジョブを載せる次段のために用意した状態で、第1段では未使用。
3. サービスから描画コントローラへ直接到達すると widget 層依存になるため、操作中ゲートは**注入プローブ**方式にした。AppMain からの登録は未了で、現状のゲートはモーダルダイアログのみを見る。
4. `delete_layer` は `removeLayerFromCurrentComposition` を通り `SafeWriteRemovalGate` を通らない。gate があるのは `*Confirmed` 系 6 メソッドのみ。第1段の承認は Detached 側のポリシーとして `delete_layer` / `remove_effect` / `delete_keyframe` を対象にした。

第1段で満たしていない受入条件。

- 選択とプレイヘッドの退避は未実装（復元するのはアクティブコンポジションのみ）。検証 7 の「選択・プレイヘッドが切り替わらない」は部分的にしか満たさない。
- `producedCompositionId` を設定していないため、トレイの「表示」は現状つねに無効。
- コマンドモードの入力は CommandIR の JSON オブジェクトのみ。自然言語の簡易構文は未対応。

## 3. 現状

### 3.1 再利用できる土台

コマンド実行は既に「フォーカス不要・同期・エラーコード付き」の形で存在する。

- `Artifact::WorkspaceAutomation::validateCommand` / `executeCommand` — `Artifact/include/AI/WorkspaceAutomation.ixx:1968, 1984`
- `Artifact::CommandIRExecutor`（`ArtifactCore::CommandExecutor` 実装）— `Artifact/src/AI/CommandIRExecutor.cppm:25`、入口は `commandExecutor()` `:1128`
- `create_composition` は既に対応済み — `CommandIRExecutor.cppm:103, 740-757`
- コマンド語彙は `CommandIR::supportedCommands()` に 34 種ぶん集約 — `ArtifactCore/include/AI/CommandIR.ixx:383-557`。Undo ラベルは `undoLabelForType()` `:597-703`

新規コンポジション作成は `ArtifactCompositionManager::createNewComposition()` から `CompositionCreatedEvent` を `globalEventBus()` へ publish し、多数のウィジェットが購読して自動更新する（`Artifact/src/Project/ArtifactProject.cppm:68-72, 1324`）。つまり **画面操作なしで作る経路は既にある**。

`WorkspaceAutomation::createComposition`（`WorkspaceAutomation.ixx:2955`）は `projectManager().createComposition(params)` を呼ぶだけで、active composition を切り替えない。

AI 経路も既存である。`AIClient::postMessage` は別スレッドを起動し（`Artifact/src/AI/AIClient.cppm:787`）、EventBus と Qt queued で UI へマーシャリングする。ツールループは `ToolBridge` → `AIToolExecutor`。

バックグラウンドタスク基盤は型まで揃っている。

- `BackgroundTaskWorkerPool` — `ArtifactCore/include/Thread/BackgroundTaskWorkerPool.ixx:67`
- `IBackgroundTask` / `TaskSnapshot` / `TaskState` / `TaskPriority` / `TaskCategory` / `CancelToken` — `ArtifactCore/include/Thread/BackgroundTaskRuntime.ixx`
- ビルドには既に含まれている（`ArtifactCore/cmake/ArtifactCoreSources.cmake:690-692` に明示列挙）

浮動ウィンドウの既存例が 2 つある。

- 非アクティベート表示の作り: `ArtifactPieMenuWidget` — `Qt::FramelessWindowHint | Qt::Tool | Qt::NoDropShadowWindowHint` / `WA_TranslucentBackground` / `WA_ShowWithoutActivating`（`Artifact/src/Widgets/Render/ArtifactPieMenuWidget.cppm:62-65`）
- トップレベル窓の生成と位置保存: `ArtifactSecondaryPreviewWindow` — `QSettings` に `normalGeometry()` を保存（`Artifact/src/Widgets/ArtifactSecondaryPreviewWindow.cppm:58-82`）、`QPointer` 遅延生成と `WA_DeleteOnClose, false`（`Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm:936, 3129-3134`）

テーマは `DccStyleTheme`（`ArtifactCore/include/Utils/WindowStyleCSS.ixx:36-40`）。

### 3.2 操作中を知る既存状態

| 対象 | 既存 API / 状態 | 場所 |
|---|---|---|
| ビューポート操作全般 | `viewportInteracting_`（idle **120 ms** で自動解除 — `viewportInteractionIdleMs_ = 120`） | `ArtifactCompositionRenderController.cppm:14035`、値 `:14074`、set `:17068`、clear `:17094` |
| Blender 風モーダルギズモ | `isModalGizmoInteractionActive()` | `.ixx:393` / `.cppm:24266` |
| タイムライン操作 | `ArtifactTimelineWidget::isInteracting()` | `ArtifactTimelineWidget.cppm:7937` |
| 「操作中」の集約式 | `forceContinuousRedraw = viewportInteracting_ \|\| isRubberBandSelecting_ \|\| isShapeVertexMarqueeSelecting_ \|\| dropGhostVisible_ \|\| (gizmo_ && gizmo_->isDragging()) \|\| (textGizmo_ && textGizmo_->isDragging())` | `ArtifactCompositionRenderController.cppm:37210-37217` |

`viewportInteracting_` の idle は 120 ms しかないため、単体では「押しっぱなしで静止したドラッグ」でゲートが開いてしまう。集約式に含まれるドラッグ系フラグがそれを防ぐ。**集約式を再利用するのが正しい。**

### 3.3 承認の既存実装

- `ToolApprovalMode { AskEveryTime=0, AutoApprove=1, YOLO=2 }` — `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm:750-777`（**匿名 namespace**）。`QSettings("ArtifactStudio","AICloud")` の `toolApprovalMode` に永続化（`:1007-1018`）
- `isReadOnlyToolCall(toolCall)` — メソッド名の**接頭辞ヒューリスティック**（`:779-801`）
- `requestToolExecutionApproval(parent, toolCall, mode)` — YOLO か「AutoApprove かつ read-only」以外は**モーダル `QMessageBox`**（`:826-856`）。呼び出しは `:999`
- `SafeWriteRemovalGate::authorize(dryRun, confirmation, userConfirmed, ...)` — `ArtifactCore/include/AI/CommandIR.ixx:312-335`。高リスク削除に `userConfirmed` を要求し、`WorkspaceAutomation.ixx:3728, 3843, 3947, 7315, 8284, 8728` の 6 か所で使われる

### 3.4 Undo の実態

`Artifact/src/Undo/UndoManager.cppm` を確認した結果、次のとおり。

- `push()` は常に `undoStack` に積む（`:4799`）。**記録中でも同じ**（1 コマンド = 1 履歴）
- `beginActionRecording(label)`（`:4822`）/ `endActionRecording()`（`:4831`）は、**シリアライズ可能なコマンドを 5〜10 件集めてリプレイ用 JSON を作る観測機構**であり、Undo 履歴をまとめる機構ではない
  - 記録中の `push()` は、非シリアライズ可 / offload 済 / 11 件目で `actionRecordingFailed_ = true`（`:4802-4804`）
  - `endActionRecording()` は `failed || size < 5 || size > 10` なら空を返す（`:4840`）。macro を push しない
- `cancelActionRecording()` は状態を消すだけで、**適用済み変更を戻さない**（`:4845-4850`）
- まとめる正規手段は `MacroUndoCommand`（`Artifact/include/Undo/UndoManager.ixx:1367`）を `push()` すること

したがって「Detached 1 タスク = 1 Undo ステップ」は既存機構では実現できない。第1段はコマンドごとに 1 ステップとする。

### 3.5 欠けているもの

- 音声入力は皆無。マイクキャプチャすら無く、音声スタックは出力専用（`Artifact/src/Audio/WASAPIDevice.cppm:57` は `eRender`/`IAudioRenderClient`、`Artifact/include/Audio/IAudioDevice.ixx` に `read()` 無し）。ASR / TTS も未実装
- 通知トーストが無い（tray 通知・ステータスバー・モーダル `QProgressDialog` のみ）
- `BackgroundTaskWorkerPool` は実質デッドコード。EventBus publish が全箇所コメントアウトされ、`SetEventBus` は no-op（`:164-166, 224-226, 380-382, 388-390, 414-416, 441-443, 452-454`）。コメント理由の `Core.Event.EventBus` は誤りで、実際は `export module Event.Bus;`（`ArtifactCore/include/Event/EventBus.ixx:14`）
- `ArtifactActiveContextService` は `activeComp_` を 1 つしか持たない（`Artifact/src/Application/ActiveContextService.cppm:81`）
- active composition を切り替えてしまう既存自動化経路が実在（`Artifact/src/Service/ArtifactProjectService.cppm:4317, 5736, 5984, 6038`）
- 「操作中」を 1 つの boolean で問い合わせる公開 API が無い
- `CommandIR` に read-only 判定が無い
- 承認ポリシーが UI ウィジェットの匿名 namespace に閉じており再利用できない
- read-only 判定の基準が二重（接頭辞ヒューリスティックと語彙完全一致）
- 複数コマンドを 1 Undo ステップにまとめる push 保留機構が無い
- `QApplication::activeModalWidget()` は Artifact 内で未使用

## 4. 設計

### 4.1 Core 側（依存を増やさない）

`BackgroundTaskWorkerPool` に依存フリーなシンクを足す。

```cpp
void SetSnapshotSink(std::function<void(const TaskSnapshot&)> sink);
```

既存のコメントアウト箇所（Submit / Running 遷移 / reportProgress / Completed / Failed / 最終状態）を sink 呼び出しへ置換する。EventBus を Core の `.ixx` に `import` しない理由は、`.ixx` への新規 `import` がモジュール循環とビルド全体再スキャンを招くため（AGENTS.md の C++20 modules 循環参照ルール）。`std::function` なら `Event.Bus` への依存が App 層に閉じる。ヘッダオンリー `.ixx` の編集のみで **CMake 変更は不要**。

`CommandIR` に読み取り専用メタデータを足す。

```cpp
static bool isReadOnlyType(const QString& type);
```

対象は `get_scene_info` / `get_layer_info` / `get_keyframes` / `get_render_status` / `list_available_effects` / `list_compositions` / `list_project_items` の 7 種で、**語彙の完全一致**で判定する。接頭辞ヒューリスティックには依存しない。

### 4.2 実行ポリシー表

`CommandIR::supportedCommands()` の 34 種を 4 分類する。

| 分類 | 対象 | 挙動 |
|---|---|---|
| ReadOnly | `get_scene_info` / `get_layer_info` / `get_keyframes` / `get_render_status` / `list_available_effects` / `list_compositions` / `list_project_items` | gate を通さず即時実行。承認不要。ビュー不変 |
| PreserveView（既定） | `create_composition` / `create_layer` / `delete_layer` / `duplicate_layer` / `group_layers` / `split_layer` / `rename_layer` / `move_layer` / `set_layer_*` / `add_effect` / `remove_effect` / `set_effect_*` / `set_property` / `set_keyframes` / `batch_set_keyframes` / `delete_keyframe` / `set_work_area` / `add_marker` / `set_layer_parent` / `import_asset` | gate 通過後に実行し、実行前後のビュー退避と復元を適用 |
| LongJob | `export_composition` / `start_render_queue` | gate 通過後、render queue へ投入して即座に完了扱い。進捗は既存サービスが持ち、トレイは状態を写すだけ |
| ViewCoupled（拒否） | `switch_composition` / `set_playback_state` | 実行しない。トレイに拒否理由を出す。「表示」導線で代替する |

### 4.3 ビュー維持の不変条件

1. 実行前に `ArtifactProjectService::currentCompositionId()` / `ArtifactActiveContextService::activeComposition()` / 選択 / プレイヘッド frame を退避する
2. `WorkspaceAutomation::executeCommand` または `Artifact::commandExecutor()` で実行する
3. 実行後に active composition / 選択が動いていたら `ArtifactProjectService::changeCurrentComposition`（`ArtifactProjectService.cppm:5907`）の正規経路で復元する
4. 動いた事実を結果メッセージに残す

復元を `changeCurrentComposition` 経由にするのは ProjectService / ActiveContext / Playback の 3 owner を揃えるためである。直に触ると `workspaceDiagnostics().operationState` に `COMPOSITION_STATE_MISMATCH` が出る（`docs/analysis/OPERATION_STATE_MAP_2026-08-30.md:13-14, 47-52`）。

### 4.4 操作中ガード

`forceContinuousRedraw` の集約式（`ArtifactCompositionRenderController.cppm:37210-37217`）を private ヘルパへ抽出し、公開 `bool isInteractionBusy() const;` を追加する。`isModalGizmoInteractionActive()`（`.ixx:393` / `.cppm:24266`）と同じ流儀にする。

`canExecuteNow()` は次を AND する。

1. `isInteractionBusy() == false`
2. `ArtifactTimelineWidget::isInteracting() == false`
3. `QApplication::activeModalWidget() == nullptr`

実行できないときは **Deferred** としてキューに残し、待機理由をトレイに出す。QTimer（約 100 ms）でポーリングし、新規 signal/slot は作らない。

### 4.5 順序保証

`BackgroundTaskWorkerPool::WorkerLoop` は毎回 `std::sort` で優先度順に取り出すため（`BackgroundTaskWorkerPool.ixx:290-296`）、同一優先度内の到着順は保証されない。

プールを `Config{ maxWorkers = 1, enableDependencyResolution = true }` の書き込みレーン専用にし、サービスが自前の FIFO から先頭 1 件ずつ submit する。第1段は読み取りも同じ直列レーンに載せる。長時間ジョブは既存 render queue サービスが自前でスレッドを持つため、第二のプールは作らない。

### 4.6 承認ポリシー

Detached ではモーダルを出さず、承認待ちをトレイ内で解決する。

| 設定 | Detached での挙動 |
|---|---|
| YOLO | 従来どおり承認なしで実行 |
| AutoApprove | ReadOnly は自動。書き込みは AwaitingApproval で滞留 |
| AskEveryTime | ReadOnly は常に自動実行。それ以外は AwaitingApproval |

AwaitingApproval は実行前の待機状態とし、行に操作内容のサマリを出してトレイ上で許可 / 拒否を選ぶ。高リスク削除は「許可」を `userConfirmed=true` として `SafeWriteRemovalGate::authorize` へ渡す。拒否した場合は実行せず Denied とする。

`ToolApprovalMode` と read-only 判定は小さな共有モジュールへ抽出し、`ArtifactAICloudWidget.cppm` と Detached サービスの両方が同じ定義を使う。設定値は既存 `QSettings("ArtifactStudio","AICloud")` の `toolApprovalMode` を同じキーとして読む。

### 4.7 Undo

コマンドごとに 1 ステップとする。`CommandIRExecutor` が供給する `undoLabel`（例 `Create Composition` — `CommandIRExecutor.cppm:742`）がそのまま履歴ラベルになる。Detached 用の追加実装は不要。`beginActionRecording` 系はリプレイ用の観測機構であり、Detached では使わない。

### 4.8 イベント

`Artifact/include/Event/ArtifactEventTypes.ixx`（`export module Artifact.Event.Types;`）に `DetachedTaskAddedEvent` / `DetachedTaskChangedEvent` / `DetachedTaskFinishedEvent` を追加する。publish は `ArtifactCore::globalEventBus()`。購読は `ArtifactStatusBar.cppm:192-215` のパターン（`QThread::currentThread() == thread()` 判定と `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`、`std::vector<EventBus::Subscription>` 保持）に従う。

### 4.9 トレイ UI

第1段はドックではなく浮動トレイとする。

- ウィンドウ設定は `ArtifactPieMenuWidget.cppm:62-65` の流儀（`Qt::FramelessWindowHint | Qt::Tool | Qt::NoDropShadowWindowHint` / `WA_TranslucentBackground` / `WA_ShowWithoutActivating`）
- 生成とライフサイクルは `ArtifactSecondaryPreviewWindow` の流儀（`QPointer` 遅延生成、`WA_DeleteOnClose, false`、`QSettings` に位置と展開状態を保存）
- 行の構成は `RenderQueueJobWidget`（`Artifact/src/Widgets/Render/ArtifactRenderQueueJobPanel.cppm:127-148` の「折りたたみ式 1 行＋詳細」）を手本にする

表示ポリシー:

| 状態 | 挙動 |
|---|---|
| 起動直後 | 非表示（`QSettings` に展開状態があれば復元） |
| タスク投入 | 自動で展開表示。フォーカスは奪わない |
| 全タスクが終端 | 数秒の猶予後に畳んで小さなバッジへ |
| AwaitingApproval / Deferred / Running / Pending がある | 畳まない |
| トレイがフォーカス中、または入力欄にテキストがある | 畳まない |

トグルは View メニューのアクションとし、新規 `ShortcutId::ViewDetachedTasks` を割り当てる。既定キーは未割当て（空）とする。`ApplicationSettingDialog.cppm` の `shortcutContext()` は ShortcutId のレンジ判定で context を決め、最後に `"Workspace.Timeline"` へフォールバックするため（`:1816`）、新 ID には明示マッピングを追加する。

### 4.10 制約の遵守

- 配色は `DccStyleTheme` から `QPalette` へ写す（`ArtifactStatusBar.cppm:115-134` のパターン）。`setStyleSheet` を新規追加しない
- 枠・角丸の描画は `ArtifactPieMenuWidget::paintEvent` と同程度の HUD 描画に留め、レイヤー合成・ブレンドを `QPainter` へ逃がさない
- ステータスバーに通知 badge を足さない（`docs/design/status-bar/README.md:22-24`）
- 新規のシグナル＆スロット配線をしない。状態伝搬は EventBus、ローカルのボタン click のみ `connect` を使う
- `ArtifactWidgets` / `libs` / `third_party` は変更しない

## 5. 実装フェーズ

| Phase | 内容 | 完了条件 |
|---|---|---|
| 0 | 本マイルストーン文書の作成 | 本ファイルが存在し、`**最終更新:** 2026-09-20` が入る |
| 1 | Core: `SetSnapshotSink` / `CommandIR::isReadOnlyType` / 承認ポリシー抽出 / `isInteractionBusy()` | タスクの状態変化がシンクへ届き、承認判定が 1 か所に集まる |
| 2 | `ArtifactDetachedTaskService` と Task アダプタ、イベント | テキスト投入でコマンドが実行され、ビュー・選択・プレイヘッドが切り替わらない |
| 3 | 浮動トレイとショートカット、View メニュー | 検証 10〜14, 17〜21 が通る |
| 4 | ポリシー確定（ViewCoupled 拒否、LongJob の render queue 連携） | 検証 5, 6 が通る |
| 5 | 音声入力（別 milestone） | — |

## 6. 変更対象ファイル

| ファイル | 変更 |
|---|---|
| `ArtifactCore/include/Thread/BackgroundTaskWorkerPool.ixx` | `SetSnapshotSink` 追加、コメントアウトされた publish をシンク呼び出しへ置換 |
| `ArtifactCore/include/AI/CommandIR.ixx` | `isReadOnlyType()` を追加 |
| `ArtifactCore/include/UI/ShortcutBindings.ixx` | `ShortcutId::ViewDetachedTasks` を追加し `Count` を +1 |
| `ArtifactCore/src/UI/ShortcutBindings.cppm` | 表示名 2 か所と `allShortcutIds()`。既定キーは空のまま |
| `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm` | `shortcutContext()` に明示マッピングを追加 |
| `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx` | `isInteractionBusy()` を宣言 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 集約式を `computeInteractionBusy()` へ抽出し `isInteractionBusy()` を実装 |
| `Artifact/include/AI/AgentApprovalPolicy.ixx` | 新規（`ToolApprovalMode` と read-only 判定の共有定義） |
| `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm` | 匿名 namespace の定義を共有定義参照へ置換 |
| `Artifact/include/Service/ArtifactDetachedTaskService.ixx` | 新規 |
| `Artifact/src/Service/ArtifactDetachedTaskService.cppm` | 新規 |
| `Artifact/src/AI/DetachedCommandTask.cppm` ほか Task アダプタ | 新規（3 種） |
| `Artifact/include/Widgets/Tasks/ArtifactDetachedTaskTray.ixx` | 新規 |
| `Artifact/src/Widgets/Tasks/ArtifactDetachedTaskTray.cppm` | 新規 |
| `Artifact/include/Event/ArtifactEventTypes.ixx` | Detached Task イベント 3 種を追加 |
| `Artifact/cmake/ArtifactSources.cmake` | 新規モジュールを 2 リストへ追加 |
| `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm` | トグルアクションと遅延生成を追加 |
| `Insight.md` | 承認ポリシーの閉塞、read-only 判定の二重基準、`beginActionRecording` の実体、push 保留機構の不在を記録 |
| `docs/WIDGET_MAP.md` | 「Tasks / Detached」を追加 |

`UndoManager` の変更は不要である。

## 7. 検証

ビルド・テストは利用者の明示指示があるまで実行しない（AGENTS.md）。

手動チェック:

1. ビューポートでレイヤーをドラッグ中にコマンド投入 → Deferred になり理由が出る。ドラッグ終了後に実行される
2. 押しっぱなしで静止したドラッグ中に投入 → 120 ms の idle を超えても Deferred のまま
3. タイムラインでキーフレームをドラッグ中に投入 → 同様に Deferred
4. モーダルダイアログ表示中に投入 → ダイアログが閉じるまで Deferred
5. `switch_composition` / `set_playback_state` を投入 → 拒否理由が出て、ビューは変わらない
6. コマンドを 3 件連続投入 → 投入順に適用される
7. ロゴ用コンポジションを編集中に「新規コンポジション」を投入 → 作成されるがビュー・選択・プレイヘッドは切り替わらない
8. 実行後 `Ctrl+Z` → 1 ステップで戻り、ラベルが `Create Composition`
9. `workspaceDiagnostics().operationState` に `COMPOSITION_STATE_MISMATCH` が出ない
10. タスク投入でトレイが自動展開するがフォーカスは奪われない
11. 全タスク完了 → 数秒後に畳まれる。Deferred / AwaitingApproval が残っている間は畳まれない
12. トレイを閉じて再表示 → 展開状態・位置・サイズが復元される
13. ショートカット設定画面に `View Detached Tasks` が意図したカテゴリで出る。既定は未割当て。割当て→保存→再読込が効き、空に戻せる
14. テーマ切替でトレイ配色が追随する
15. `setStyleSheet` が新規追加されていない
16. `git status -s` で `ArtifactWidgets` / `libs` / `third_party` が無変更
17. `AskEveryTime` で書き込みコマンドを投入 → モーダルが出ず、トレイに承認待ちが出る。許可で実行、拒否で Denied
18. `YOLO` で投入 → 承認なしで実行される
19. `AskEveryTime` でも ReadOnly は承認待ちにならず即時実行される
20. 高リスク削除はトレイの「許可」を経ないと実行されない
21. AI モードで複数コマンドを実行 → 各コマンドが個別の Undo ステップになる
22. AI Cloud ウィジェットの承認が抽出後も従来どおり動く

## 8. 未確認・リスク

- ビューポートのパン／ズーム中に `notifyViewportInteractionActivity()` がマウス移動ごとに呼ばれるかは未確認。呼ばれない場合、ゆっくりのパンでは gate が開く。ただしパンはプロジェクトを変更しないため実害は低い
- コマンド単位の Undo は AI ジョブが多数コマンドを出すと履歴を圧迫する。タスク単位にまとめたくなったら `UndoManager::push` に保留機構を足す設計が必要
- `Qt::Tool` 窓はアプリが非アクティブになると隠れる。OS 通知で拾う設計は第2段へ回す
- 単一 worker の直列化により即時コマンドが 1 件ずつしか走らない。体感が悪ければ ReadOnly を別レーンへ分離する
- `ShortcutId` を増やすと `Count` 依存の配列サイズに触れる。`toJson`/`loadFromJson`（`ShortcutBindings.ixx:181-182`）の後方互換を実装時に確認する
- 空バインドの既定で `defaultShortcut()` が空 `QKeySequence` を返す経路が UI 上で問題なく扱われるか、実装時に確認する
- `ToolApprovalMode` / `isReadOnlyToolCall` の抽出は AI Cloud ウィジェットに触るため回帰リスクがある（検証 22 で担保）
- ローカルのボタン click `connect` は Artifact 内で既に一般化している（`Artifact/src` で 91 箇所）。AGENTS.md の「新規のシグナル＆スロット接続禁止」は全体配線の禁止と解釈して進めるが、例外解釈の是非は設計レビューで確認したい
- 第1段では AI マルチステップを既存 `AIClient` のツールループに載せるため、進捗粒度は AI 応答単位より細かくできない可能性がある
