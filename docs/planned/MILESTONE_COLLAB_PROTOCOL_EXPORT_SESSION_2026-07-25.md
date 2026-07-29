# MILESTONE_COLLAB_PROTOCOL_EXPORT_SESSION_2026-07-25

**ステータス:** Partial（protocol export、session ID、rule sync API を実装済み。server bridge、UI 状態表示、競合解決、認証、runtime 検証は未完了）
**対象:** `ArtifactCore/src/Collaborate/CollaborationProtocol.cppm`, `ArtifactCore/include/Network/CollaborationWebSocket.ixx`, `ArtifactCore/src/Network/CollaborationWebSocket.cppm`
**位置づけ:** M-COLLAB-1 の後続。孤立していた `CollaborationProtocol` を export 化し、`CollaborationWebSocket` と接続。セッション管理を追加する。
**作成日:** 2026-07-25

## 1. 目的

- `CollaborationProtocol` (`src/Collaborate/CollaborationProtocol.cppm`, module `Collaborate.Protocol`) が誰からも `import` されていない孤立コードから脱却する
- `CollaborationWebSocket` にセッション管理と rule sync メッセージ機能を追加する

## 2. 現状 (2026-07-25)

| 要素 | 状態 | 詳細 |
|------|------|------|
| Collaborate.Protocol module | ❌ 孤立 | `export module` はあるが型に `export` が無い。コードが存在するだけで誰も使えない |
| CollaborationWebSocket | ✅ 動作可 | 5メッセージプロトコル (join/operation/lock/unlock/presence)、ハートビート25s、指数バックオフ再接続 (6回/最大30s)、メッセージキュー (max 256) 完備 |
| セッションID | ❌ 不在 | sessionId を誰も生成・保持していない |
| Rule sync メッセージ | ❌ 不在 | WebSocket は "rule_added/removed/updated/executed" を認識しない |
| LayerLockManager | ✅ 動作可 | acquire/release/purge/タイムアウト 完備。5分デフォルト |

### 既存資産

- `CollaborationProtocol` (`module Collaborate.Protocol`): `CollaborationMessage` (8種), `wrapRule()`, `unwrapRule()`, `serialize()`, `deserialize()`, `generateSessionId()` 完備
- `Reactive.Events` module: `ReactiveRule` (TriggerCondition + vector<Reaction>), JSON シリアライズ完備。`CollaborationProtocol` が内部で利用
- `tools/collaboration-server/server.js`: Node.js WebSocket サーバ。5種メッセージ対応済み。rule sync メッセージ種別は現状サーバ側に実装されていない（クライアント側定義のみ）
- `docs/planned/MILESTONE_COLLAB_BACKEND_FOUNDATION_2026-07-21.md`: 前段階のマイルストーン。WebSocket クライアントと LayerLockManager を実装済み

## 3. 実装内容 (2026-07-25)

### 3.1 CollaborationProtocol export 化

`src/Collaborate/CollaborationProtocol.cppm` の型に `export` を追加:
- `export struct CollaborationMessage`
- `export class CollaborationProtocol`

モジュール名 `Collaborate.Protocol` は既存のまま。CMakeLists.txt の登録も既にあるため、追加設定不要。

### 3.2 CollaborationWebSocket セッション管理

- コンストラクタで `QUuid::createUuid()` による sessionId 生成
- `sessionId()` アクセサ追加
- Impl に `QString sessionId` メンバー追加

### 3.3 Rule sync シグナル追加

CollaborationWebSocket に以下を追加:
- `sendRuleSync(type, ruleId, payload)` — rule 同期メッセージを JSON で送信。payload は JSON オブジェクトに変換
- シグナル: `ruleAdded(ruleId, payload)`, `ruleRemoved(ruleId)`, `ruleUpdated(ruleId, payload)`, `ruleExecuted(ruleId)`
- `textMessageReceived` ハンドラに rule_* メッセージのディスパッチ追加

## 4. 変更ファイル

| ファイル | 変更 |
|----------|------|
| `ArtifactCore/src/Collaborate/CollaborationProtocol.cppm` | `struct CollaborationMessage` → `export struct`, `class CollaborationProtocol` → `export class`. `.ixx` は分離せず `.cppm` 内完結 |
| `ArtifactCore/include/Network/CollaborationWebSocket.ixx` | `sessionId()` / `sendRuleSync()` API 追加。`ruleAdded`/`ruleRemoved`/`ruleUpdated`/`ruleExecuted` シグナル追加 |
| `ArtifactCore/src/Network/CollaborationWebSocket.cppm` | セッションID生成 (`QUuid`)。`sendRuleSync()` 実装。`textMessageReceived` に rule sync ディスパッチ追加 |

## 5. 残タスク / 将来展望

- [ ] `tools/collaboration-server/server.js` に rule sync メッセージ種別のハンドリング追加
- [ ] `CollaborationProtocol.wrapRule()` と `CollaborationWebSocket.sendRuleSync()` を接続するブリッジコード
- [ ] UI での rule 共有状態表示
- [ ] 競合解決 (OT/CRDT)
- [ ] 認証/TLS
- [ ] ユーザーセッション管理クラス（現在は Impl 内の sessionId のみ）

## 6. 依存関係

```
CollaborationWebSocket (本 milestone)
├── QWebSocket (Qt6::WebSockets)
├── tools/collaboration-server (外部サーバ、5種メッセージ対応)
├── Collaborate.Protocol (本 milestone で export 化)
│   └── Reactive.Events (ReactiveRule データモデル)
└── cmd.exe → QUuid (Qt6::Core)
```

## 7. 関連

- `docs/planned/MILESTONE_COLLAB_BACKEND_FOUNDATION_2026-07-21.md` — 前段階。WebSocket クライアント基盤
- `docs/planned/MILESTONE_COLLAB_WS_CLIENT_2026-07-02.md` — 設計ベース
- `docs/planned/MILESTONE_COLLABORATION_FEATURES_2026-03-28.md` — 機能要件
- `tools/collaboration-server/server.js` — 接続先サーバ
