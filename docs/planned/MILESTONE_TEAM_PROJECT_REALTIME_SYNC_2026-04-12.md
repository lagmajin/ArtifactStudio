# MILESTONE_TEAM_PROJECT_REALTIME_SYNC_2026-04-12

## チームプロジェクト リアルタイム同期基盤

### 概要

複数ユーザーが同一プロジェクトを**同時に編集**できるリアルタイムコラボレーション機能を実装する。
Google Docs のような同時編集体験を After Effects コンポジティングワークフローに提供する。

---

## 前提条件

### 既存で利用可能な基盤

| 要素 | 場所 | 内容 |
|------|------|------|
| コマンドシリアライズ | `ArtifactCore/include/Command/SerializableCommand.ixx` | `serialize()` / `deserialize()` 定義済み |
| 編集セッション履歴 | `ArtifactCore/include/Command/EditSession.ixx` | `pushCommand()` / `getHistoryAsJson()` |
| プロジェクトJSON化 | 全主要クラス `toJson()` / `fromJson()` | 94箇所に実装済み |
| Undo/Redo | `QUndoStack` + コマンドパターン | 既存のUndoシステム |
| HTTPクライアント | `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm` | `QNetworkAccessManager` 使用経験あり |
| 設計ドキュメント | `docs/planned/MILESTONE_COLLABORATION_FEATURES_2026-03-28.md` | Phase設計とコード例あり |

### 新規に必要なもの

| 要素 | 技術選定 |
|------|---------|
| WebSocket通信 | **QWebSocket** (Qt NetworkAuth モジュール) |
| サーバー | **Node.js + Socket.IO** (プロトタイピング) → **C++ Crow/CppRestSDK** (本番) |
| 競合解決 | **OT (Operational Transformation)** — Google Docs方式 |
| 操作ブロードキャスト | `EditSession` 拡張 + WebSocket送信 |
| プレゼンス表示 | カスタムウィジェット `ArtifactPresenceWidget` |

---

## Phase 構成

### Phase 1: WebSocket 通信基盤 (8-12h)

**目標:** クライアントとサーバー間の双方向通信を確立する。

#### 1.1 QWebSocket クライアント実装

```cpp
// ArtifactCore/include/Network/CollaborationWebSocket.ixx
export module Network.CollaborationWebSocket;

export namespace ArtifactCore {

class CollaborationWebSocket : public QObject {
    W_OBJECT(CollaborationWebSocket)
public:
    void connectToServer(const QString& serverUrl, const QString& projectId);
    void disconnect();
    bool isConnected() const;

    // メッセージ送信
    void sendOperation(const QJsonObject& operation);
    void sendPresence(const QJsonObject& presence);
    void sendLockRequest(const QString& layerId);
    void sendUnlockRequest(const QString& layerId);

signals:
    void operationReceived(const QJsonObject& operation)
        W_SIGNAL(operationReceived, operation);
    void presenceUpdated(const QJsonObject& presence)
        W_SIGNAL(presenceUpdated, presence);
    void lockGranted(const QString& layerId)
        W_SIGNAL(lockGranted, layerId);
    void lockDenied(const QString& layerId, const QString& reason)
        W_SIGNAL(lockDenied, layerId, reason);
    void userJoined(const QJsonObject& user)
        W_SIGNAL(userJoined, user);
    void userLeft(const QJsonObject& user)
        W_SIGNAL(userLeft, user);
    void connectionStateChanged(ConnectionState state)
        W_SIGNAL(connectionStateChanged, state);

private:
    QWebSocket* ws_;
    QString projectId_;
    ConnectionState state_;
};

} // namespace ArtifactCore
```

#### 1.2 メッセージプロトコル定義

```typescript
// 操作ブロードキャスト
{
    "type": "operation",
    "clientId": "uuid-1234",
    "projectId": "proj-5678",
    "operation": {
        "type": "setProperty",
        "target": "layer.layer-abc.transform.position",
        "value": [100, 200],
        "frame": 0
    },
    "version": 42,
    "timestamp": "2026-04-12T10:30:00Z"
}

// プレゼンス更新
{
    "type": "presence",
    "clientId": "uuid-1234",
    "userId": "user-001",
    "userName": "山田太郎",
    "userColor": "#FF5722",
    "cursorPosition": { "widget": "timeline", "frame": 150 },
    "selectedLayers": ["layer-abc", "layer-def"],
    "timestamp": "2026-04-12T10:30:00Z"
}

// ロック要求
{
    "type": "lock_request",
    "clientId": "uuid-1234",
    "layerId": "layer-abc",
    "timestamp": "2026-04-12T10:30:00Z"
}
```

#### 1.3 サーバー (Node.js プロトタイプ)

```javascript
// collaboration-server/index.js
const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 8080 });

const sessions = new Map(); // projectId -> { clients, operations, locks }

wss.on('connection', (ws, req) => {
    ws.on('message', (data) => {
        const msg = JSON.parse(data);
        handleClientMessage(ws, msg);
    });
});

function handleClientMessage(ws, msg) {
    const session = sessions.get(msg.projectId);
    if (!session) return;

    switch (msg.type) {
        case 'join':
            session.clients.add(ws);
            broadcast(session, { type: 'user_joined', clientId: msg.clientId });
            // 現在の操作履歴を新規参加者に送信
            msg.clientId && ws.send(JSON.stringify({
                type: 'history',
                operations: session.operations
            }));
            break;

        case 'operation':
            // OT変換を適用
            const transformed = applyOT(msg.operation, session.operations);
            session.operations.push({ ...transformed, version: session.operations.length });
            // 送信者以外にブロードキャスト
            broadcast(session, { ...msg, operation: transformed }, ws);
            break;

        case 'lock_request':
            if (!session.locks.has(msg.layerId)) {
                session.locks.set(msg.layerId, msg.clientId);
                ws.send(JSON.stringify({ type: 'lock_granted', layerId: msg.layerId }));
                broadcast(session, { type: 'lock_updated', layerId: msg.layerId, clientId: msg.clientId }, ws);
            } else {
                ws.send(JSON.stringify({
                    type: 'lock_denied',
                    layerId: msg.layerId,
                    reason: `Locked by ${session.locks.get(msg.layerId)}`
                }));
            }
            break;

        case 'presence':
            broadcast(session, msg, ws);
            break;
    }
}

function broadcast(session, msg, excludeWs = null) {
    const data = JSON.stringify(msg);
    for (const client of session.clients) {
        if (client !== excludeWs && client.readyState === WebSocket.OPEN) {
            client.send(data);
        }
    }
}
```

---

### Phase 2: 操作ブロードキャスト統合 (12-16h)

**目標:** 既存の `EditSession` と `SerializableCommand` を WebSocket に接続する。

#### 2.1 EditSession の拡張

```cpp
// ArtifactCore/include/Command/EditSession.ixx (拡張)
class EditSession {
public:
    // 既存
    void pushCommand(std::shared_ptr<SerializableCommand> cmd);
    QJsonObject getHistoryAsJson() const;

    // 新規: コラボレーション用
    void setCollaborationClient(std::shared_ptr<CollaborationWebSocket> client);
    void broadcastCommand(std::shared_ptr<SerializableCommand> cmd);
    void applyRemoteOperation(const QJsonObject& operation);

private:
    std::shared_ptr<CollaborationWebSocket> collabClient_;
    bool isRemoteOperation_ = false; // リモート操作中はローカルUndoStackに積まない
};
```

#### 2.2 コマンドのOT変換

```cpp
// ArtifactCore/include/Command/OperationTransformer.ixx
export module Command.OperationTransformer;

export namespace ArtifactCore {

class OperationTransformer {
public:
    // 2つの操作を競合解決（OT1: 後発操作を先行操作に対して変換）
    static QJsonObject transform(const QJsonObject& op, const QJsonObject& againstOp);

    // 操作の互換性チェック
    static bool areCompatible(const QJsonObject& a, const QJsonObject& b);

    // 操作のターゲット抽出
    static QString getTargetPath(const QJsonObject& op);

private:
    // プロパティ設定操作の競合解決
    static QJsonObject transformSetProperty(const QJsonObject& op, const QJsonObject& againstOp);

    // レイヤー操作の競合解決
    static QJsonObject transformLayerOperation(const QJsonObject& op, const QJsonObject& againstOp);
};

} // namespace ArtifactCore
```

---

### Phase 3: 競合検出とロック機構 (12-16h)

**目標:** 同一レイヤーの同時編集をロックで排他制御する。

#### 3.1 LayerLockManager

```cpp
// ArtifactCore/include/Command/LayerLockManager.ixx
export module Command.LayerLockManager;

export namespace ArtifactCore {

struct LayerLockInfo {
    QString layerId;
    QString clientId;
    QString userName;
    std::chrono::steady_clock::time_point acquiredAt;
    std::chrono::seconds timeout;
};

class LayerLockManager : public QObject {
    W_OBJECT(LayerLockManager)
public:
    bool requestLock(const QString& layerId, const QString& clientId, const QString& userName);
    void releaseLock(const QString& layerId, const QString& clientId);
    bool hasLock(const QString& layerId, const QString& clientId) const;
    LayerLockInfo getLockInfo(const QString& layerId) const;

    // タイムアウトしたロックを自動解放
    void checkTimeouts();

signals:
    void lockAcquired(const QString& layerId, const QString& clientId)
        W_SIGNAL(lockAcquired, layerId, clientId);
    void lockReleased(const QString& layerId, const QString& clientId)
        W_SIGNAL(lockReleased, layerId, clientId);
    void lockRequestDenied(const QString& layerId, const QString& reason)
        W_SIGNAL(lockRequestDenied, layerId, reason);

private:
    QHash<QString, LayerLockInfo> activeLocks_;
    std::chrono::seconds defaultTimeout_{300}; // 5分
};

} // namespace ArtifactCore
```

#### 3.2 タイムライン上のロック表示

```cpp
// Artifact/src/Widgets/Timeline/LayerLockIndicator.cppm
// タイムラインレイヤー行にロックアイコンを表示
// ロック中は該当レイヤーの編集をグレーアウト
```

---

### Phase 4: リアルタイムプレゼンス表示 (8-12h)

**目標:** 他ユーザーの位置・選択状態を可視化する。

#### 4.1 PresenceWidget

```cpp
// Artifact/include/Widgets/ArtifactPresenceWidget.ixx
export class ArtifactPresenceWidget : public QWidget {
    W_OBJECT(ArtifactPresenceWidget)
public:
    void addPresence(const QString& userId, const QString& userName, const QColor& color);
    void updatePresence(const QString& userId, const QJsonObject& data);
    void removePresence(const QString& userId);

private:
    struct UserPresence {
        QString name;
        QColor color;
        QString cursorLocation;  // "timeline:150", "inspector:transform.position"
        QStringList selectedLayers;
    };
    QHash<QString, UserPresence> presenceMap_;
};
```

#### 4.2 タイムライン上の他ユーザーカーソル

```cpp
// タイムラインのフレーム位置に他ユーザーのカーソルを表示
// 色付きの縦線で「誰がどこを見ているか」を可視化
```

---

## 依存関係

```
Phase 1: WebSocket通信基盤
    └── QWebSocket (Qt NetworkAuth)
    └── Node.js サーバー (プロトタイピング)

Phase 2: 操作ブロードキャスト統合
    └── EditSession 拡張
    └── SerializableCommand 実装拡充
    └── OperationTransformer (OT変換)

Phase 3: 競合検出とロック機構
    └── LayerLockManager
    └── タイムラインロック表示

Phase 4: リアルタイムプレゼンス表示
    └── PresenceWidget
    └── タイムライン他ユーザーカーソル
```

---

## 技術的課題と対策

### 1. OT変換の複雑さ

**課題:** 異なるユーザーの操作が競合した際、正しく変換する必要がある。
**対策:**
- 最初は単純な「後勝ち」方式でスタート
- プロパティ設定操作のみOT変換を実装
- 段階的に複雑な操作（レイヤー追加/削除等）に対応

### 2. ネットワーク遅延・切断

**課題:** 操作中に切断された場合の復旧。
**対策:**
- オフライン中は操作をローカルキューに蓄積
- 再接続時に未送信操作を一括送信
- サーバー側でバージョン番号を管理し、欠落を検出

### 3. パフォーマンス

**課題:** 多数のユーザーが同時操作するとブロードキャスト負荷が増大。
**対策:**
- 操作の間引き（throttle）: 同一プロパティの連続更新は100ms以内に1回のみ送信
- プレゼンス更新も間引き: 500ms以内に1回
- WebSocketメッセージは最小限のJSONにシリアライズ

---

## 工数サマリー

| Phase | 内容 | 工数 |
|-------|------|------|
| Phase 1 | WebSocket通信基盤 | 8-12h |
| Phase 2 | 操作ブロードキャスト統合 | 12-16h |
| Phase 3 | 競合検出とロック機構 | 12-16h |
| Phase 4 | リアルタイムプレゼンス表示 | 8-12h |
| **合計** | | **40-56h** |

---

## 将来の拡張 (Phase 5+)

- **変更履歴ビューアー:** 誰がいつ何を変更したかを時系列表示
- **コメント/レビュー機能:** レイヤー単位でのコメント付与
- **Git連携:** プロジェクトのバージョン管理をGitと同期
- **オフライン編集:** 切断中でも編集可能、再接続時にマージ
- **権限管理:** 閲覧のみ/編集可能/管理者 のロール設定

---

## 参考文献

- [MILESTONE_COLLABORATION_FEATURES_2026-03-28.md](../../docs/planned/MILESTONE_COLLABORATION_FEATURES_2026-03-28.md)
- [MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md](MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md)
- [MILESTONE_SESSION_LEDGER_RECOVERY_WORKSPACE_2026-04-09.md](MILESTONE_SESSION_LEDGER_RECOVERY_WORKSPACE_2026-04-09.md)
