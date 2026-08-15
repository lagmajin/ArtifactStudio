# MILESTONE_COLLAB_BACKEND_FOUNDATION_2026-07-21

**Status:** ✅ Complete (4/4)
**最終更新:** 2026-08-15
**Goal:** コラボレーション機能のバックエンド（Qt クライアント側 I/O レイヤ、ロック管理、EditSession 連携）を実装し、既存 `tools/collaboration-server` に接続可能な状態にする。

## 背景

- **サーバー**: `tools/collaboration-server/server.js` (Node.js + ws) は 5 種メッセージ (join/operation/lock_request/unlock_request/presence) 対応済みで動作可能
- **孤立コンポーネント**: `CollabPresenceWidget` (UI 完備)、`CollaborationProtocol` (JSON ラッパー)、`EditSession` (履歴) はあるが、**サーバーと Qt を繋ぐ transport 層が 0 行**
- 全体進捗: 約 15〜20%

## Scope

| # | 項目 | ファイル | 内容 |
|---|------|----------|------|
| 1 | WebSocket クライアント `.ixx` | `ArtifactCore/include/Network/CollaborationWebSocket.ixx` | 公開 API: 接続/切断/再接続/送信/シグナル |
| 2 | WebSocket クライアント `.cppm` | `ArtifactCore/src/Network/CollaborationWebSocket.cppm` | QWebSocket 実装、ハートビート、指数バックオフ再接続、メッセージ型 |
| 3 | LayerLockManager `.ixx` + `.cppm` | `ArtifactCore/include/Command/LayerLockManager.ixx` + `ArtifactCore/src/Command/LayerLockManager.cppm` | レイヤーロック管理: acquire/release/purge/timeout |
| 4 | EditSession broadcast フック | `ArtifactCore/include/Command/EditSession.ixx` 拡張 | pushCommand にコールバック機構追加 |

## Non-goals

- `CollabPresenceWidget` の MainWindow 統合 (UI)
- タイムラインロック表示 (UI)
- OT/CRDT 競合解決アルゴリズム (M-COLLAB-6)
- 認証/TLS
- CMake/vcpkg 設定変更（別途指示）
- `ArtifactWidgets` サブモジュール変更

## 依存関係

```
CollaborationWebSocket (本 milestone)
├── QWebSocket (Qt6::WebSockets - vcpkg 要追加)
├── tools/collaboration-server (既存)
└── CollaborationProtocol (既存 - メッセージ型のベース)

LayerLockManager (本 milestone)
├── CollaborationWebSocket (lock_request/unlock_request 送信)
└── EditSession (ロック状態の反映)
```

## 実施順序

1. `CollaborationWebSocket.ixx` + `.cppm` (M-COLLAB-1 相当)
2. `LayerLockManager.ixx` 
3. `EditSession` broadcast フック

## 2026-08-15 現行コード監査

`CollaborationWebSocket` は QWebSocket 接続、join／operation／lock／presence／rule sync の送受信、heartbeat、再接続、受信キューを実装している。`LayerLockManager` は acquire／release、リモート状態反映、期限切れ purge を持ち、`EditSession::pushCommand()` は JSON 履歴と登録済み broadcast callback を扱う。したがって、この文書の4項目は現行コード上も完了扱いでよい。

ただし、これはバックエンド基盤の完了であり、UI統合、OT／CRDT競合解決、認証／TLS、サーバーとの実接続受入、切断・再接続時の編集整合性は別マイルストーン／runtime検証の範囲に残る。

## 2026-07-25 実装監査（履歴）

- `ArtifactCore/include/Network/CollaborationWebSocket.ixx` と `ArtifactCore/src/Network/CollaborationWebSocket.cppm` に接続状態、送受信、ハートビート、指数バックオフ再接続の API／実装が存在する。
- `ArtifactCore/include/Command/LayerLockManager.ixx` と `ArtifactCore/src/Command/LayerLockManager.cppm` に acquire/release、リモート状態反映、期限切れ purge が存在する。
- `ArtifactCore/include/Command/EditSession.ixx` の `pushCommand()` は JSON 履歴を作成し、登録済み `BroadcastCallback` を呼び出す。
- UI 統合、競合解決、認証/TLS、依存設定変更は Non-goals のため、この監査では未実施でも完了判定を変更しない。

## 関連

- `docs/planned/MILESTONE_COLLAB_WS_CLIENT_2026-07-02.md` — 本実装の設計ベース
- `docs/planned/MILESTONE_TEAM_PROJECT_REALTIME_SYNC_2026-04-12.md` — Phase 1〜4 全体計画
- `docs/planned/MILESTONE_COLLABORATION_FEATURES_2026-03-28.md` — 機能要件 (60-80h)
- `plans/MILESTONE_COLLABORATIVE_REACTIVE_RULES_2026-06-07.md` — ReactiveRule 限定版
- `tools/collaboration-server/server.js` — 接続先サーバー
