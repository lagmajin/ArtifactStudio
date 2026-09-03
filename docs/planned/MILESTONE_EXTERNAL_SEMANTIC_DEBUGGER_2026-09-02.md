# MILESTONE: 外部 Semantic Debugger（ArtifactDebugger.exe）

**最終更新:** 2026-09-02
**ステータス:** Not Started
**識別子:** M-DIAG-7

## 1. 目的

Visual Studio のように C++ の変数やアドレスを表示するだけではなく、ArtifactStudio の編集・アニメーション・合成・レンダリングの意味を説明できる、外部プロセス型のデバッガを提供する。

利用者が知りたい問いを、専用の一級操作として扱う。

- `Why is this invisible?`
- `Who changed opacity to 0?`
- `Why is Position X 483.23?`
- `Which render pass produced this pixel?`
- `What changed between frame 1841 and 1842?`

本 milestone の正規プロダクトは `ArtifactDebugger.exe` とする。ArtifactStudio 本体には、状態の意味を記録・公開する最小限の `ArtifactDebugRuntime` だけを置き、デバッガの表示・検索・解析・操作UIは別プロセスに分離する。

## 2. 設計判断

### 2.1 外部プロセスを正規のUI境界とする

```text
ArtifactStudio.exe
  └─ ArtifactDebugRuntime
       ├─ DebugIdentity / semantic registry
       ├─ mutation provenance
       ├─ frame trace / snapshots
       └─ debugger transport endpoint

ArtifactDebugger.exe
  ├─ attach / session management
  ├─ Semantic Inspector
  ├─ Why? cause-chain viewer
  ├─ Timeline / frame diff
  ├─ semantic breakpoint / watchpoint
  └─ RenderGraph / resource inspector
```

外部化する理由は、デバッガのUIや障害が本体のQt・レンダリング状態を汚染しないこと、実行中のプロセスへAttachできること、VSと併用できること、Debugger側を独立して改良・配布できることである。

### 2.2 既存のMCP/診断基盤を再利用する

既存の `MILESTONE_MCP_AI_DEBUG_SYSTEM_2026-08-02` はログ、state、trace、property、breakpoint、watchpoint、step、render graphなどの語彙をすでに持つ。これを別系統に作り直さず、ArtifactDebugger.exeの通信契約・操作契約の基礎として再利用する。

既存のMCPはAI専用に閉じず、次の3クライアントが同じ意味モデルを共有できる構造へ整理する。

```text
ArtifactDebugRuntime
       │
       ├─ ArtifactDebugger.exe
       ├─ MCP / AI client
       └─ headless diagnostic harness
```

### 2.3 IPCは制御と大量データを分ける

- 制御・要求・応答: 既存のローカルRPC／JSON-RPC系契約を再利用する
- イベント・フレーム履歴・大きなsnapshot: `MILESTONE_SHARED_MEMORY_IPC.md` の共有メモリ基盤を候補とする
- 接続初期段階はストリーム経路でも成立させ、共有メモリを必須条件にしない
- GPUの生ハンドル、`IDeviceContext`、live layer pointerをwireへ渡さない
- wireには安定したdebug ID、semantic path、値、revision、frame、sourceだけを載せる

## 3. スコープ

### 含む

- `ArtifactDebugger.exe` の独立起動・Attach・Detach・再接続
- Layer / Composition / Property / Asset / RenderPass / Resource のsemantic identity
- プロパティ変更の provenance（誰が、いつ、なぜ、どの値へ変更したか）
- Base / Animation / Parent / Constraint などの寄与内訳
- `Why?` による可視性・値・描画結果の原因チェーン
- フレーム単位のイベント、状態差分、直近snapshotの閲覧
- object/property単位のsemantic breakpointとwatchpoint
- RenderGraphのpass、resource lifetime、read/write関係の表示
- VSのnative debuggerとの併用

### 含まない

- Visual StudioのC++ source-level debuggerの置き換え
- 任意メモリへの書き込みやポインタ操作
- 初期段階での完全なreverse debugger
- GPU resourceを別プロセスから直接所有・操作すること
- 既存のレンダラーや合成経路をデバッガ都合で大規模変更すること
- デバッガUIをArtifactStudio本体へ常設すること

## 4. 意味モデル

### 4.1 安定した対象識別

各対象を名前だけでなく、プロジェクト・コンポジション・レイヤー・プロパティの階層IDで表す。

```text
project/<project-id>/composition/<composition-id>/layer/<layer-id>/property/opacity
```

表示名は変更可能なラベルとして扱い、履歴の結合キーには使用しない。再読込やrename後も同じ対象を追跡できるrevisionとgenerationを定義する。

### 4.2 Mutation provenance

```text
PropertyMutation
  target: layer/<id>/opacity
  previous: 0.69
  current: 0.72
  frame: 1842
  source: AnimationSystem::Evaluate
  reason: FadeIn / keyframe 4 -> 5
  trace: <trace-id>
```

通常のプロパティ値へ履歴を直接埋め込まず、debug-onlyのbounded recorderへ記録する。履歴が無効、満杯、または観測対象外の場合は、デバッガ上で「未観測」と明示し、推測結果を確定情報として表示しない。

### 4.3 値の寄与グラフ

最終値だけでなく、評価に参加した要素を説明する。

```text
Position X = 483.23
  Base Transform  400.00
  Animation         72.00
  Parent            16.23
  Constraint        -5.00
  Final            483.23
```

各寄与は、値・順序・source・入力revisionを持つ。評価順が記録されていない場合は、単なる値の一覧とし「因果関係」と表示しない。

### 4.4 Cause chain

例: 非表示原因を次のようなグラフとして返す。

```text
Layer "PlayerShadow"
  Visible = true
  Opacity = 1.0
  Transform = valid
  └─ Parent "WorldFX"
       Visible = false
       └─ changed at frame 2281
            source: CutsceneSystem::HideWorldFX()
            trigger: Event "BossIntroStart"
```

原因チェーンは直接観測されたリンクと、条件から導いた候補を区別する。候補にはconfidenceを付け、確定原因として扱わない。

## 5. フェーズ

### Phase 0: 契約・責務固定

- `ArtifactDebugRuntime` と `ArtifactDebugger.exe` の責務を確定する
- session、protocol version、engine build、capability、endianness、最大payloadを定義する
- debug ID、semantic path、frame、revision、trace IDの共通型を定義する
- attach／detach／heartbeat／shutdown／stale sessionの状態機械を定義する
- MCP、headless harness、外部Debuggerが同じread vocabularyを使うことを確認する

完了条件: wire contractと責務分担が文書化され、live objectやGPU native handleを送らないことが静的に確認できる。

### Phase 1: 外部Debuggerの読み取り専用縦切り

- `ArtifactDebugger.exe` を単独起動できる
- ArtifactStudioへlocalhost経由でAttach／Detachできる
- session overview、active composition、selected layer、current frameを表示する
- connection loss、protocol mismatch、未観測状態をUIで説明する
- VS debuggerと同時接続できる

完了条件: ArtifactStudioを再起動せずにAttach／Detach／再接続でき、選択中のLayerとcurrent frameが本体の状態と一致する。

### Phase 2: Property provenance / Why?

- Transform、Opacity、Visibleを最初の対象にする
- mutation historyをframe、source、previous/current value付きで表示する
- Base／Animation／Parent／Constraintの寄与内訳を表示する
- `Why is this invisible?` と `Who changed this?` の問い合わせを提供する
- `last modified`から該当trace／frameへジャンプする

完了条件: 意図的にOpacityまたはVisibleを変更したケースで、変更元・変更時刻・変更前後値・原因chainを外部Debuggerから追跡できる。

### Phase 3: Semantic breakpoint / Timeline

- object created、destroyed、reparented、hidden、selected、modifiedを監視する
- `layer("Title").opacity > 1` のような条件を登録する
- frame boundaryで安全にpauseし、continue、step forwardを提供する
- 直近300フレーム程度のbounded snapshot ringを実装する
- frame diffとchange-point候補を表示する

完了条件: 条件一致時に対象、frame、直前値、変更元、after snapshotを取得でき、breakpoint解除後に本体を通常再開できる。

### Phase 4: Snapshot閲覧と限定的な巻き戻し

- 過去snapshotをread-onlyで閲覧する
- frame間のproperty／object／event差分を表示する
- snapshot復元可能な状態と、表示専用の状態を明確に区別する
- 復元は安全なframe boundaryでのみ実行する
- deterministic replayが成立しない場合は、`replay unavailable`を表示する

完了条件: 過去フレームを現在実行中の状態と混同せず閲覧でき、復元失敗や復元対象外を明示できる。

### Phase 5: RenderGraph / Resource semantics

- RenderPassの順序、入力、出力、read/write関係を表示する
- texture／bufferのformat、size、lifetime、memory estimate、last barrierを表示する
- pixel inspect時に、対象pixelのpass／layer contributorを追跡する
- GPU objectのアドレスではなく、engine resource IDとsemantic ownerを表示する
- GPU経路と互換フォールバック経路の差分を表示する

完了条件: 選択したresourceについて、作成元・書き込みpass・読み取りpass・現在state・未観測箇所を外部Debuggerで確認できる。

### Phase 6: 運用・配布・回帰

- Debugger単体の配布とversion compatibilityを整備する
- session／trace／snapshotを診断bundleとして保存・再読込する
- sanitized trace exportを提供する
- Debug build／diagnostic build／通常buildの機能差を定義する
- 外部Debuggerを用いた静止画、連番画像、Shape、合成、3Dの回帰手順を整備する

完了条件: 別マシンまたは別セッションで診断bundleを開き、元プロセスなしでも観測結果を再確認できる。

## 6. 予定コンポーネント

実装時の候補であり、既存のmodule境界とコード監査後に確定する。

| コンポーネント | 役割 | 配置候補 |
|---|---|---|
| `ArtifactDebugRuntime` | 本体側の観測・公開・安全なpause | Artifact / ArtifactCore |
| `SemanticDebugTypes` | ID、path、frame、revision、source契約 | ArtifactCore |
| `ProvenanceRecorder` | mutationと寄与のbounded記録 | ArtifactCore / Diagnostics |
| `DebugSessionEndpoint` | attach、request、response、heartbeat | Artifact |
| `ArtifactDebugger.exe` | 外部UI、query、timeline、breakpoint | 新規debugger target |
| `ArtifactDebuggerProtocol` | wire schemaとversioning | ArtifactCoreまたは共有protocol |
| `SnapshotStore` | frame snapshot、diff、bundle export | ArtifactCore / Diagnostics |

`ArtifactDebugger.exe` はArtifactStudioのwidget実装を直接importしない。UIの共有が必要な場合も、まずprotocolとsemantic modelを共有し、live Qt objectの共有は避ける。

## 7. IPC・停止・安全性

### 接続

- v0は同一マシンのlocalhost接続に限定する
- session tokenとprotocol versionを必須にする
- `ArtifactStudio.exe` が明示的にdebug endpointを有効化した場合だけ接続を受け付ける
- unexpected disconnectは本体の実行継続を既定とする

### Pause

Debuggerからのpauseは任意命令箇所での強制停止ではなく、ArtifactStudioが安全なframe boundaryで受け付ける。レンダー中、GPU submit中、resource lifetime更新中の停止は「pending」と表示する。

### Write操作

Phase 1〜2はread-onlyとする。将来のproperty patchは、既存のCommand／Undo／Safe Write契約を通し、直接メモリ書き込みを禁止する。patchにはpreview、confirmation、rollback、auditを必須とする。

## 8. 既存マイルストーンとの関係

| 既存マイルストーン | 関係 |
|---|---|
| `MILESTONE_MCP_AI_DEBUG_SYSTEM_2026-08-02.md` | 既存のdebug tool vocabulary、trace、breakpoint、watchpointを再利用する。MCP専用のUIを新設する計画ではない。 |
| `MILESTONE_SHARED_MEMORY_IPC.md` | 大量event／snapshot転送の候補。Phase 1の接続成立を共有メモリ実装の完了に依存させない。 |
| `MILESTONE_LOGGING_SYSTEM_2026-07-26.md` | 構造化ログ、bounded buffer、JSONLを再利用する。ログ本文だけでなくsemantic sourceを追加する。 |
| `MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md` | 外部Debuggerの回帰用に既存の最小再現surfaceとreport vocabularyを利用する。 |
| `MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md` | RenderGraph、resource lifetime、frame diffの既存計画を外部表示へ接続する。 |
| `MILESTONE_OUT_OF_PROCESS_PROXY_WORKER_2026-09-02.md` | 子プロセス分離の実装知見は参照するが、proxy生成workerとDebuggerのjob責務・protocolは共有しない。 |

## 9. リスクと判断待ち

| リスク | 方針 |
|---|---|
| semantic情報の記録コスト | debug buildではbounded recorder、通常buildではoffまたは低コストstubにする |
| 履歴が欠落したときの誤診 | 未観測・候補・確定を別表示し、推測を確定原因に昇格させない |
| 本体とDebuggerのversionずれ | protocol version、engine build、capability negotiation、unknown field許容を持つ |
| pause中のGPU／worker状態 | safe pointとpending状態を契約化し、任意thread suspendを行わない |
| reverse debuggingの過大化 | まずsnapshot閲覧、次に限定復元、最後にdeterministic replayの順にする |
| IPCの大量データ転送 | control planeとdata planeを分離し、必要なときだけshared memoryを使う |
| デバッガが本体を不安定化 | 外部UIを分離し、endpoint切断時は本体を通常実行へ戻す |

### ユーザー判断が必要な項目

1. `ArtifactDebugger.exe` を同一リポジトリ内の独立targetとして持つか、将来別リポジトリへ切り出せる境界を先に作るか
2. Phase 1の接続方式を既存MCP TCP、QLocalSocket、Named Pipeのどれから始めるか
3. snapshot復元を本milestoneに含めるか、Phase 4をread-only閲覧までで区切るか
4. 通常buildにも低コストのdebug endpointを残すか、diagnostic build限定にするか

## 10. Done Criteria

- [ ] `ArtifactDebugger.exe` がArtifactStudioと別プロセスで起動する
- [ ] Attach／Detach／再接続／protocol mismatchが安全に処理される
- [ ] active composition、selected layer、current frameをread-onlyで表示できる
- [ ] LayerのOpacity／Visible／Transformについて、値・履歴・source・frameを表示できる
- [ ] `Why?` で親可視性、opacity、animation、maskなどの観測済み原因chainを辿れる
- [ ] semantic breakpointがframe boundaryで発火し、after snapshotを保持できる
- [ ] frame diffと直近snapshot ringを外部Debuggerから閲覧できる
- [ ] RenderGraph resourceについてpass、lifetime、owner、stateを表示できる
- [ ] VS native debuggerとの同時利用を妨げない
- [ ] 本体へlive pointer、GPU native handle、任意メモリ書き込みを渡さない
- [ ] Debugger切断・クラッシュがArtifactStudioの通常実行を停止させない
- [ ] 診断bundleを保存し、プロセスなしでread-only再表示できる
- [ ] 静止画、連番画像、Shape、合成、3Dの代表ケースで意味表示の整合を確認できる

## 11. 検証方針

ビルド・テスト・CMake生成は、ユーザーの明示許可後に行う。実装時は次の順で確認する。

1. protocol schemaの単体検証
2. mock host／mock debugger間のAttach・再接続・version negotiation
3. ArtifactStudio実プロセスとのread-only接続
4. 意図的なproperty mutationによるprovenance検証
5. semantic breakpoint、frame snapshot、diff検証
6. RenderGraph／resourceの実機確認
7. Debugger切断、host crash、partial snapshot、古いbundleの復旧確認

## 12. 見積もり

- Phase 0〜1: 16〜24時間
- Phase 2〜3: 24〜40時間
- Phase 4〜5: 32〜56時間
- Phase 6・回帰受入: 16〜24時間
- 合計: 88〜144時間（既存MCP／Trace／IPC基盤を再利用できる場合）

この見積もりは専用GUIの縦切りから始める前提であり、完全なreverse debugger、GPU外部共有、任意コード評価の安全化は含めない。

## 13. 更新履歴

- 2026-09-02: 初版作成。既存のMCP AI debug計画を外部 `ArtifactDebugger.exe` のsemantic debugging milestoneとして再整理。
