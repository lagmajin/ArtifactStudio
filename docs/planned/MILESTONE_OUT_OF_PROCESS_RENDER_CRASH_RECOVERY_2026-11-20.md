# オフラインレンダーのプロセス分離とクラッシュ復帰マイルストーン

**最終更新:** 2026-11-20
**ステータス:** Not Started
**作成日:** 2026-11-20
**目的:** Render Queue のオフラインジョブを子プロセスで実行し、GPU/ドライバFault・ハング・自前コード／外部コードのクラッシュからジョブを復帰可能にする。スループット改善は本マイルストーンの目的に含まない。
**現状:** 本節は計画のみ。実装・ビルド・実行・計測は未実施（2026-11-20 時点）。

---

## 1. なぜプロセス分離か

スレッド内では次の故障を止められず、すべてアプリプロセス全体を死なせる。

- device lost / TDR / ドライバFault / VRAM 枯渇
- ハング（`frameTimeoutMs` は「成功したが遅かった」を失敗にするだけで、固まったスレッドは停止できない）
- OFX など外部コードのアクセス違反
- 自前のレンダリングコードのアクセス違反・競合

プロセスなら kill と再起動が可能で、親（UI・ジョブ管理・エンコード）は生存する。**守る対象は Render Queue のオフラインジョブに限定**する。Composition View のプレビュー／編集は従来どおり同一プロセスとし、毎フレームのプロセス間画像転送は行わない（コスト対効果が合わない）。

同一 GPU 上でプロセスを増やしても GPU スループットは増えず、VRAM は重複する。per-adapter 分割は「Vulkan の process-global function pointer 制約を避ける」ための別案件とし、本マイルストーンでは扱わない。

## 2. 現状確認（2026-11-20・コード読解）

| 事実 | 箇所 | 影響 |
|---|---|---|
| farm 経路は `const bool useFarm = false;` で無効 | `Artifact/src/Render/ArtifactRenderQueueService.cpp` | 既存 farm をそのまま有効化してもジョブは流れない |
| ローカル farm worker は `splitRange` の互いに素な subrange を `std::thread` で実行し `request.renderFrame` を呼ぶ | `ArtifactCore/src/Render/RenderFarmMaster.cpp` | 同一プロセス内のスレッドクラッシュでアプリ全体が死ぬ |
| `saveCheckpoint()` は `baseFrame + totalProgress_.completed`（全 worker 共有の完了数）を `completedUpToFrame` として保存 | 同上 | 並列 shard が非順序に完了すると、未完了フレームまで完了扱いになり得る（連続前提のバグ） |
| 再開判定と `outputManifest()` は既存フレームファイルの `size() > 0` のみを見る | 同上 | 書き込み途中で落ちた truncated 画像が完了扱いになる |
| リモート worker の未報告フレームは timeout 時に failed 印付けするだけで再配分しない | `collectRemoteResults()` 同ファイル | 子プロセス死後の自動復帰経路が無い |
| heartbeat / disconnect / frame timeout の検出は実装済み | `ArtifactCore/src/Network/NetworkRPCServer.cpp` | 監視の土台は利用可能 |
| retry / held（`FailureManifest::setHeld`）は実装済み | `ArtifactCore/src/Render/RenderFarmTypes.ixx`、`RenderFarmMaster.cpp` | poison frame の表現はあるが、再起動との連携は無い |
| `ArtifactWorker` は `rendererExecutable` + `renderPayload` を受けて子を起動するだけで描画しない | `Artifact/src/Worker/FarmWorkerMain.cpp` | 本番合成を描く worker は別途必要 |
| `artifact-renderer.exe` は Qt Svg/QPainter の診断レンダラで `unsupportedLayerKinds` を返す | `ArtifactRenderer/`（親 CMake ツリー外） | 本番 composition の代替にならない |
| `OffscreenCompositionRenderer` は production に存在し `renderToQImage()` を公開 | `Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cpp` | headless worker の土台になる |
| `render` CLI は「headless executor is not implemented」で終了 | `Artifact/src/AppMain.cpp` | CLI 単独のレンダー実行体は未実装 |
| `ArtifactAutoSaveManager` の recovery point と起動時復元プロンプトは実装済み | `Artifact/src/AppMain.cpp` | プロジェクトデータの保全は既存機構でカバー |

上記のうち checkpoint とフレーム妥当性の問題はコード読解に基づく推論であり、実行による再現は未実施。

## 3. 設計

### 3.1 構成

```
親 Artifact.exe
 ├── UI / Queue Manager / SessionLedger（既存）
 ├── supervisor: 子の起動・監視・再起動・未完了フレームの再割当
 └── encoder: native FFmpeg（CPU、既存の連番→動画経路を再利用）

子 artifact-render-worker.exe（1 本、opt-in 時のみ）
 ├── 凍結スナップショット（project コピー + composition id + range + seed/bake）
 ├── OffscreenCompositionRenderer でフレーム描画
 └── フレームを temp → rename で原子的に書き、完了を RPC で報告
```

- 有効化は opt-in。`ARTIFACT_RENDER_OUT_OF_PROCESS=on` または `RenderQueue/OutOfProcessRender`。既定は従来どおり同一プロセス実行。
- 子へ渡す設定は明示的な CLI 引数とスナップショット JSON とし、ユーザー設定（`settings.cbor`）への書き込みは行わない。`ARTIFACT_*` の起動フラグは起動時 1 回読みなので、子には必要な値を明示継承する。
- 子が親のメモリ内 composition ポインタを直接受け取る経路は作らない。部分編集中の状態を描く必要はなく、凍結コピーを描く。
- 動画は子が連番を書き、親が最後にまとめてエンコードする。子が落ちても既存フレームはディスクに残り、エンコードはやり直し可能。
- 新しい signal/slot 接続や中央集権イベント配線は追加しない。既存の farm RPC コールバック（`setOnProgress` / `setOnCompleted`）と command/service 経路を再利用する。

### 3.2 契約

- **出力不変**: 同一プロセス経路と連番 pixel / MP4 hash が一致する。フレーム番号・finalize 順序は変更しない。
- **進捗**: 子のフレーム完了 ack を親が集約して既存 Queue UI へ流す。子ローカルの再生状態は持たない。
- **スレッド予算**: 親が子の枠を差し引いて worker/encoder 数を決める。子には独自の上限を渡す（`QThread::idealThreadCount()` を親子で二重に使わない）。
- **決定性**: particle / fluid / MPM などフレーム間状態を持つレイヤーは seed 固定または `componentSimulationBake` 相当の bake を明示する。再開時に state が飛ぶと出力が変わるため、ここを暗黙に解決しない。
- **フレーム妥当性**: 再開時はファイルサイズではなく、画像ヘッダの可読性と写出し時の checksum で完全性を判定する。
- **失敗の表現**: 失敗フレームは `FailureManifest` の held と再試行回数で表現し、診断イベントと SessionLedger に残す。

## 4. フェーズと DoD

### P0: checkpoint / resume の正しさ（他フェーズの前提）

- `saveCheckpoint()` を「連続完了位置」から「完了フレームの明示集合」へ変更する。
- `executeLocalRange()` の再開スキップを `size() > 0` から完全性検証へ変更する。
- 子が死んだときの未完了 range を親が再割当できる形にする（`collectRemoteResults()` の failed 印付けだけでは不十分）。

**DoD:** 4 shard を意図的に非順序で完了させても再開時に欠番・重複が出ないこと。truncated な画像ファイルが完了扱いされないこと。いずれもテスト追加（ビルド・実行は許可後に実施）。

### P1: headless render worker EXE

- 新 target（例 `ArtifactRenderWorker`）を追加し、`OffscreenCompositionRenderer` で project コピー + composition + frame range を描画する。
- 既存 `NetworkRPCClient` / `NetworkRPCServer` の job 割当・進捗・cancel 契約を再利用する。新規プロトコルを広く導入しない。
- 診断用 `artifact-renderer.exe` は流用せず、本番合成描画経路をそのまま使う。新規 QPainter 合成や QImage のホットパス採用は禁止。

**DoD:** CLI 単独で 3 フレームの連番と summary を出力できること。同一入力の同一プロセス経路と全 pixel 一致すること（実行は許可後）。

### P2: Render Queue の supervisor 接続

- opt-in フラグで子を起動し、完了 ack で進捗を更新する。フレーム出力は一時ディレクトリの連番とし、正常終了後に親が既存の連番→動画経路で encode する。
- 子の終了コード・heartbeat timeout・`frameTimeoutMs` を検知して kill → backoff → 再起動し、未完了フレームのみ再割当する。
- 親のスレッド予算から子の枠を控除する。

**DoD:** 子を実行中に kill しても親が生存し、再起動後に全フレームが揃うこと。連番と MP4 が baseline と一致すること。フラグ off で従来動作が変わらないこと。

### P3: poison frame / 監視と診断

- 同一フレームで N 回失敗したら held にしてジョブを続行する（クラッシュループの打ち切り）。
- 再起動回数とジョブ時間の上限、子のログ収集、失敗理由の SessionLedger / 診断イベントへの記録を追加する。

**DoD:** 特定フレームで必ず落ちる fixture が held 扱いで完了すること。上限超過時に原因付きで失敗すること。

### P4: スコープ外（将来）

- per-adapter のプロセス分割（Vulkan の device 制約回避）
- 共有メモリリング（`RenderFarmSharedBuffer`）によるフレーム転送のゼロコピー化
- プレビュー／Composition View のプロセス隔離
- decode / iGPU assist のプロセス化

## 5. 検証計画（実行はユーザーの明示許可後）

1. 単体: checkpoint の非順序完了、truncated 画像の再開判定、held の優先順位。
2. 統合: 3／30 フレームのジョブで子を (a) kill、(b) ハング、(c) 既知の AV で落とし、親が復帰できること。
3. parity: 同一プロセス経路と連番全 pixel、MP4 hash を比較する。
4. stateful layers: particle / fluid / MPM を seed 固定と bake ありで再開し、継ぎ目がないこと。
5. 起動コスト: Qt init、shader compile、asset decode の child 起動時間と再起動オーバヘッドを計測する。
6. 実機: 現状の RTX 4070 Ti 単一 GPU で実施。複数物理 GPU / iGPU は未検証のまま記録する。

## 6. リスクと未検証事項

- 本計画の前提となる checkpoint バグ・`size() > 0` 判定は静的読解のみで、実測していない。
- 子プロセスの VRAM 重複と起動コスト（shader compile / asset cache）は未計測。
- フレーム間状態を持つレイヤーの決定性が揃わない場合、再開後の出力が単一プロセス経路と一致しない。`componentSimulationBake` の既存範囲を超える作業が発生する可能性がある。
- farm 経路は `useFarm = false` のままで、既存 farm が動く前提での見積もりはできない。
- 本マイルストーンではビルド・テスト・ベンチマークを実行しない。

## 7. 関連文書

- `MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md` — 外部レンダラの job snapshot / CLI / diagnostics 設計
- `MILESTONE_FARM_WORKER_REMOTE_RENDER_2026-08-01.md` — farm worker に本番描画を接続する試み（未完了）
- `MILESTONE_HETERO_COMPUTE_FOUNDATION_2026-09-14.md` — XPU のスレッド内 mixed dispatch（本件はその外側のプロセス分離）
- `MILESTONE_RENDER_THREAD_SEPARATION_FEASIBILITY_2026-10-16.md` — Diligent の Vulkan process-global function pointer 制約
- `../analysis/DCC_THIN_AREA_SURVEY_2026-11-18.md` — ローカルレンダーファームの現状整理
- `../../Insight.md` — 2026-11-20 の調査記録（checkpoint 連続前提、external renderer は診断のみ）
