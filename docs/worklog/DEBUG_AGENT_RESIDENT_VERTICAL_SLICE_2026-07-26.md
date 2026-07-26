# Resident Debug Agent Vertical Slice - 2026-07-26

## Status

Artifact側に、UIを開かなくても動作する最小の常駐Debug Agentを追加した。

## Implemented

- `AppMain` の `QApplication` 起動直後に `ArtifactDebugAgent` を生成
- 100msごとのQObject timer eventをsafe checkpointとして使用
- Playbackの現在frame・前回frame・再生状態・tick番号を `debug-bridge.json` へ出力
- MCP stateの `session.paused` を検知した場合、Playbackを協調停止
- 新規signal/slot、QtCSS、低レベルD3D12変更は追加していない

## Current Boundary

この縦切りでは、監視対象はまずPlayback frameと再生状態に限定している。任意のproperty、render resource、bufferの値監視と、直前Nサンプルの永続化は次段階。

## Verification

ビルド・テストは未実施。ソース変更と既存API名の静的確認のみ。
