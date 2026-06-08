# Milestone: Real-time Collaboration for Reactive Rules
**Created:** 2026-06-07
**Status:** Planned
**Priority:** Medium

## 概要
`ReactiveRule`をWebSocketで同期し、複数ユーザーによるレイヤー操作のリアルタイム共同編集を可能にする。

## 背景
- AEにはない「物理ベースリアクション」を外部化
- 現在の`ReactiveEvents`はローカルのみ
- `ArtifactWidgets/src/Collaborate/`が空

## 実装スコープ

### Phase 1 (Week 1) - プロトコル層
```
ArtifactCore/src/Collaborate/CollaborationProtocol.cppm
```
- JSONメッセージフォーマット
- `TriggerCondition`/`Reaction`の直列化

### Phase 2 (Week 2) - ネットワーク層
```
ArtifactCore/src/Collaborate/WebSocketSync.cppm
```
- Qt WebSocketサーバー/クライアント
- タイムスタンプベース競合解決

### Phase 3 (Week 3) - UI層
```
Artifact/src/Widgets/Collaborate/CollaborationPanel.cppm
```
- 参加者リスト
- ルール編集同期表示

## 技術スタック
- Qt 6.5 `QWebSocket`
- `TrackResult`のJSON直列化（既存）
- `ArtifactRenderQueueManagerWidget`のイベントバスパターン

## 成果指標
- 2人以上で`ApplyForce`ルールを同時編集
- 100ms未満の遅延
- 競合時の自動マージ

## 関連
- `ArtifactCore/src/Reactive/ReactiveEvents.cppm`
- `ArtifactCore/src/Script/Expression/ExpressionEvaluator.cppm`