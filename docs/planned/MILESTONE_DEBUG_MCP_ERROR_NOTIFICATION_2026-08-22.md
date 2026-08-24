# Debug MCP Error Notification

**最終更新:** 2026-08-22
**ステータス:** In Progress

## 現在の実装状況

実装済みの主な診断発生源:

- RAM Preview frame failure: `preview.frame_failed`
- FFmpeg video decode receive/send failure: `scope.failed`
- MediaSource open／seek／probe failure: `decode.open_failed` / `decode.seek_failed`
- MediaAudioDecoder 初期化 failure: `scope.failed`
- Render Farm failure: `render.farm_failed`
- local video encoder open／finalize failure: `encode.open_failed` / `encode.finalize_failed`

実装済みの外部取得・制御:

- bridge snapshot への bounded Error/Fatal event 公開
- `get_latest_failure`
- `get_diagnostic_events`（ack／`sinceSequence` 対応）
- `get_failure_context`
- `acknowledge_diagnostic_sequence`
- `set_diagnostic_breakpoint`
- severity／code／component による協調 pause

未完了:

- MCP host の push notification／resource update 対応
- worker 個別の失敗を重複なく集約するポリシー
- 複数 MCP client 間の ack 分離
- 実ファイルを使った decode／preview／render failure の runtime 受入

## 目的

プレビュー、画像／動画デコード、レンダー、エンコードなどで深刻なエラーが発生したとき、既存の Debug MCP 経由で外部 AI が次の情報を取得できるようにする。

- エラーの発生を検知する
- 現在のフレーム、選択レイヤー、コンポジションを取得する
- エラー直前の trace と診断イベントを取得する
- 必要に応じて再生を協調的に pause する
- 同一エラーの重複通知を抑制する

## 現状確認

既存の `DebugBridgeFileWriter` は `debug-bridge.json` を定期更新し、再生状態、Project Health、TraceRecorder、property snapshot を公開している。

既存の `tools/debug-mcp-server` には snapshot、watch、break condition、resume、step、break history の MCP tool がある。アプリ側にも状態ファイルを読み、条件一致時に playback を pause する処理がある。

一方、`ArtifactCore::DiagnosticRecorder` が保持する構造化された `Error` / `Fatal` イベントは、現在の bridge snapshot に直接含まれていない。デコーダや RAM Preview の `lastError_` も共通の MCP 診断イベント列にはまだ統合されていない。

## スコープ

### Phase 1 — 診断イベントの bridge 公開（完了）

- `DiagnosticRecorder` の最新 sequence を bridge に含める
- `Error` / `Fatal` を最大32件の bounded array で出力する
- code、message、component、operation、objectId、frameIndex、traceId を保持する
- bridge の JSON 書き込み失敗や診断列の欠落を静かに無視せず、既存ログへ記録する

実装済み:

- `get_latest_failure` / `get_diagnostic_events` を MCP server に追加
- `preview.frame_failed` を RAM Preview から記録
- FFmpeg の receive/send packet failure を `DiagnosticScope` の failure として記録

完了条件:

- AI が `debug-bridge.json` から最新の構造化エラーを読める
- 通常の preview tick に大きな追加走査を発生させない
- 古いイベントを無限に蓄積しない

### Phase 2 — MCP の診断取得・ack（部分完了）

- `get_latest_failure`
- `get_diagnostic_events`
- `acknowledge_diagnostic_sequence`
- `get_failure_context`

診断取得、ack、failure context の tool は実装済み。`get_diagnostic_events` は ack sequence または明示された `sinceSequence` 以降に限定でき、bounded history の `eventsTruncated`／`firstPublishedSequence` も返す。アプリ側の永続的な ack 連携と複数 AI セッション間の ack 分離は未完了。

`get_failure_context` は、診断イベント、現在 snapshot、直前 trace、直前数フレームの bounded context をまとめて返す。

### Phase 3 — 診断ブレークポイント（部分完了）

- severity、component、code、objectId を条件にできる診断 watch を追加する
- 条件一致時は既存の協調 pause を再利用する
- break hit に診断イベントと snapshot を保存する
- 同一 `code + objectId + frameIndex` の連続重複を抑制する

`diagnostic_severity_is`、`diagnostic_code_is`、`diagnostic_matches` による latest failure 条件、および既存の協調 pause は実装済み。MCP 専用の `set_diagnostic_breakpoint` も追加し、severity／code／component／objectId を組み合わせられる。break hit の詳細 payload、重複抑制ポリシーは未完了。

### Phase 4 — 実エラー源の統合

優先順:

1. RAM Preview の frame failure
2. Media / FFmpeg decoder の open、seek、decode failure
3. composition render failure
4. render queue / encoder failure

Render Farm の確定失敗境界には `render.farm_failed` を追加済み。local encoder の open／finalize 失敗には `encode.open_failed`／`encode.finalize_failed` を追加済み。`MediaSource` の open／seek／probe failure には `decode.open_failed`／`decode.seek_failed` を追加済み。`MediaAudioDecoder` の初期化失敗は `scope.failed` として記録する。通常の worker の個別失敗統合は未完了。

既存の `lastError_` を一括置換せず、失敗が確定する境界で `DiagnosticRecorder` に記録する。通常の成功経路、Qt signal/slot、Diligent/DX12 低レベル経路は変更しない。

## 非スコープ

- MCP server からの任意スレッド停止
- Diligent / DX12 内部への広範なフック
- 新しいグローバル signal/slot 網
- 全ての warning の AI 通知
- エラー発生時の自動修復

## 通知モデル

MCP stdio server から AI へ任意の push 通知を前提にしない。初期実装は、未確認 sequence を保持し、AI が `get_latest_failure` または `get_diagnostic_events` を呼んだときに返す pull モデルとする。

MCP host が resource update / notification を正式に扱える場合のみ、後段で push 通知を追加検討する。

## 主要ファイル

- `Artifact/src/AppMain.cppm`
- `ArtifactCore/include/Diagnostics/CoreDiagnosticRecorder.ixx`
- `ArtifactCore/include/Diagnostics/CoreDiagnosticSnapshot.ixx`
- `Artifact/src/Render/ArtifactRamPreviewController.cppm`
- `ArtifactCore/src/Media/MediaPlaybackController.cppm`
- `tools/debug-mcp-server/server.js`
- `docs/DEBUG_MCP_PSEUDO_BREAKPOINT_PLAN_2026-06-04.md`

## リスクと確認事項

- `DiagnosticRecorder` の sequence を polling 側で取りこぼさないこと
- bridge snapshot の JSON サイズを bounded に保つこと
- decoder の worker thread から診断を記録しても既存の thread-safe 契約を破らないこと
- 同一エラーの再通知で AI 側がループしないこと
- 実ファイルによる decode／preview runtime 検証は、ビルド・テスト実行の許可後に行う

## Runtime 受入手順（未実行）

### MCP mock smoke

1. `set_mock_snapshot` で `diagnostics.latestSequence`、`diagnostics.latestFailure`、`diagnostics.events`、`trace` を含む snapshot を設定する。
2. `get_latest_failure` で `hasUnacknowledgedFailure=true` と failure payload を確認する。
3. `get_failure_context` で diagnostic event と trace tail が返ることを確認する。
4. `acknowledge_diagnostic_sequence` を呼び、同じ sequence が未確認として再表示されないことを確認する。
5. `set_diagnostic_breakpoint` に `code=preview.frame_failed` を指定し、次の mock snapshot 更新で break condition が検出されることを確認する。

### Live app smoke

1. アプリ起動後に `debug-bridge.json` と `debug-mcp-state.json` のパスを確認する。
2. MCP から `set_diagnostic_breakpoint` を設定する。
3. 壊れた media または RAM Preview failure を発生させる。
4. `get_failure_context` で component、operation、frame／object、trace が揃うことを確認する。
5. playback が協調 pause し、`resume_debug_session` 後に再開できることを確認する。

## 推奨する最初の垂直スライス

1. `DiagnosticRecorder` の最新 failure を bridge snapshot に追加
2. MCP の `get_latest_failure` を追加
3. `preview.frame_failed` を1箇所だけ構造化記録
4. MCP から failure context を取得できることを確認

この順なら、診断イベントの共通契約を先に固定でき、decoder／renderer の個別統合を後から安全に増やせる。
