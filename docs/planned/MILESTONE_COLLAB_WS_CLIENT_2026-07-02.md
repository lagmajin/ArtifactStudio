# M-COLLAB-1 Collaboration WebSocket Client Milestone

作成日: 2026-07-02
位置づけ: 既存 `tools/collaboration-server` (Node + `ws`) のプロトコルに **Qt クライアント側** から接続する foundation。
`MILESTONE_TEAM_PROJECT_REALTIME_SYNC_2026-04-12.md` (Phase 1〜4) と
`plans/MILESTONE_COLLABORATIVE_REACTIVE_RULES_2026-06-07.md` の Phase 1〜2 を、
**Qt 側 I/O レイヤ 1 段** に集約する位置づけ。

参照:
- `tools/collaboration-server/server.js` — 既存プロトタイプ。`join / operation / lock_request / unlock_request / presence` 5 種を確定
- `tools/collaboration-server/README.md` — プロトコル語彙
- `docs/planned/MILESTONE_COLLABORATION_FEATURES_2026-03-28.md` — 機能要件 (60-80h 全体)
- `docs/planned/MILESTONE_TEAM_PROJECT_REALTIME_SYNC_2026-04-12.md` — 既存 Phase 設計
- `plans/MILESTONE_COLLABORATIVE_REACTIVE_RULES_2026-06-07.md` — ReactiveRule 限定版
- `Artifact/include/Widgets/CollabPresenceWidget.ixx` / `Artifact/src/Widgets/CollabPresenceWidget.cppm` — 既存 UI 部品
- `ArtifactCore/include/Command/EditSession.ixx` / `EditSessionManager.ixx` — 編集セッション側
- `ArtifactCore/src/Collaborate/CollaborationProtocol.cppm` — ReactiveRule 用の軽量プロトコル

---

## 1. 目的

- Qt 6.5 標準の `QWebSocket` ベースで **接続 / 切断 / 再接続 / ハートビート / サーバメッセージ** を 1 モジュールに閉じる
- 既存 `tools/collaboration-server` の 5 種メッセージを **型付き API** で送受信可能にする
- `EditSession` への配線は本 milestone のスコープ外 (M-COLLAB-2)。**I/O 境界のみ提供**
- 既存 `CollabPresenceWidget` の更新経路として **そのまま使える** シグナル集合を出す

## 2. 現状整理 (2026-07-02 基準)

### 2.1 既存資産

| 要素 | 場所 | 状態 |
|------|------|------|
| Node WebSocket サーバ | `tools/collaboration-server/server.js` | 動作可能。`join / operation / lock_request / unlock_request / presence` 確定 |
| プレゼンス widget | `Artifact/include/Widgets/CollabPresenceWidget.ixx` + `.cppm` | 100% 実装済。UI 統合なし |
| ReactiveRule 用軽量 protocol | `ArtifactCore/src/Collaborate/CollaborationProtocol.cppm` | JSON ラッパのみ。transport なし |
| EditSession | `ArtifactCore/include/Command/EditSession.ixx` | `pushCommand()` まで。broadcast なし |
| EditSessionManager | `ArtifactCore/include/Command/EditSessionManager.ixx` | UI との bridge 役 |

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Qt WebSocket クライアント | **不在** | サーバに繋げない |
| 再接続 / ハートビート | **不在** | 切断で手動再接続必須 |
| メッセージスキーマ検証 | **不在** | サーバ破壊でクライアントが落ちる |
| Q_DECLARE_METATYPE | **不在** | 既存 rules に倣って登録必要 |

### 2.3 設計方針 (AGENTS.md 整合)

- **global signal を新規追加しない**。本モジュールは `QObject` 派生で `Q_SIGNAL` 経由 (既存 `W_OBJECT` 規約) で配信
- **`.ixx` は宣言成立に必要な最小限の `import`** のみ。`QWebSocket` を実装側に閉じ、`.ixx` では前方宣言を避ける
- **`module X;` 以降に `#include` を追加しない**。`QWebSocket` / `QJsonObject` 等は **global module fragment** (`module;` 行と `module X;` の間) に置く
- **`QImage` / `setStyleSheet` / `QColorDialog` は新規採用しない**。UI 描画が必要になっても `QPainter` 直書き + 既存 theme token
- 親-子リポ bump: 親 `ArtifactStudio` の gitlink 更新を `.github/GIT_WORKFLOW_PARENT_CHILD.md` に従う
- `ArtifactCore` 内 `CMakeLists.txt` の force list に **`CollaborationWebSocket.cppm`** を追記
- `ArtifactWidgets` サブモジュールは **触らない**

## 3. スコープ

### 3.1 含む

- `ArtifactCore/include/Network/CollaborationWebSocket.ixx` — `Network.CollaborationWebSocket` 公開 API
- `ArtifactCore/src/Network/CollaborationWebSocket.cppm` — 実装本体
- `Q_DECLARE_METATYPE` / `W_REGISTER_ARGTYPE` 登録 (compile unit 末尾)
- 既存サーバプロトコルと 1:1 対応する型:
  - `JoinMessage` / `JoinAckMessage`
  - `OperationMessage` (operation ブロードキャスト)

## 4. 公開 API 概要 (ixx 側)

```cpp
export module Network.CollaborationWebSocket;

export namespace ArtifactCore {

// サーバ接続状態
enum class CollabConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

// 5 種メッセージのタグ + ペイロード (JSON object そのまま)
struct CollabMessage {
    QString type;        // "join" / "operation" / "lock_request" / "unlock_request" / "presence"
    QJsonObject payload; // サーバプロトコル準拠
    qint64 timestamp = 0;
};

class CollaborationWebSocket : public QObject {
    W_OBJECT(CollaborationWebSocket)
public:
    explicit CollaborationWebSocket(QObject* parent = nullptr);
    ~CollaborationWebSocket();

    // 接続
    void connectToServer(const QString& url, const QString& projectId,
                          const QString& userId, const QString& userName,
                          const QString& userColor);
    void disconnect();
    bool isConnected() const;
    CollabConnectionState state() const;

    // 送信 (5 種)
    void sendOperation(const QJsonObject& operation);
    void sendLockRequest(const QString& layerId);
    void sendUnlockRequest(const QString& layerId);
    void sendPresence(const QJsonObject& presence);

    // 受信シグナル (global 化せず、QObject 経由で配信)
    void messageReceived(const CollabMessage& msg)        W_SIGNAL(messageReceived, msg);
    void connectionStateChanged(CollabConnectionState s)  W_SIGNAL(connectionStateChanged, s);
    void userJoined(const QJsonObject& user)              W_SIGNAL(userJoined, user);
    void userLeft(const QJsonObject& user)                W_SIGNAL(userLeft, user);
    void historyReceived(const QJsonArray& operations)    W_SIGNAL(historyReceived, operations);
    void lockGranted(const QString& layerId)              W_SIGNAL(lockGranted, layerId);
    void lockDenied(const QString& layerId, const QString& reason)
                                                         W_SIGNAL(lockDenied, layerId, reason);
    void lockReleased(const QString& layerId)             W_SIGNAL(lockReleased, layerId);
    void remoteOperation(const QJsonObject& operation)    W_SIGNAL(remoteOperation, operation);
    void remotePresence(const QString& userId, const QJsonObject& presence)
                                                         W_SIGNAL(remotePresence, userId, presence);
    void protocolError(const QString& message)            W_SIGNAL(protocolError, message);

private:
    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
```

## 5. 実装ファイル (.cppm) の責務

- `QWebSocket` 実体保持とライフサイクル
- 内部 queue: 接続中以外の発呼は **最大 256 件** のリングバッファで保持し、Connected 後に送出
- JSON パース失敗時: `protocolError` を発し、当該フレームを破棄
- ハートビート: `QTimer` 25s 毎に `{ "type": "ping", "timestamp": <ms> }` を送出
- 自動再接続: `CollabConnectionState::Reconnecting` に遷移し、指数バックオフで `connectToServer` を再呼出
- ログ: `qDebug()` ベース (App Debugger 側は別 milestone)

## 6. テスト方針

- `ArtifactCore/tests/` 配下に軽量ユニットテストを追加 (`ArtifactCore` テストターゲットがある場合)
- 検証内容:
  - JSON スキーマ整合 (5 種メッセージの serialize/deserialize)
  - 切断 → 指数バックオフ → 再接続のシーケンス
  - 接続中以外の発呼が queue に積まれ、Connected 後に送信されること
  - 不正 JSON 受信で `protocolError` が出ること
- 結合テスト: `tools/collaboration-server` をローカル起動し、2 クライアントで `operation` 送受信が往復することを確認

## 7. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_TEAM_PROJECT_REALTIME_SYNC_2026-04-12.md` | 本 milestone はその **Phase 1 実装** に相当。Phase 2-4 は本 milestone 後 |
| `MILESTONE_COLLABORATION_FEATURES_2026-03-28.md` | 機能要件の大本。本 milestone はその "リアルタイム同期" の I/O 境界 |
| `plans/MILESTONE_COLLABORATIVE_REACTIVE_RULES_2026-06-07.md` | ReactiveRule 専用。本 milestone の I/O を再利用 |
| `MILESTONE_SECURITY_HARDENING_2026-03-28.md` | 認証 / TLS は別 topic |

## 8. リスクと未解決論点

1. **`QWebSocket` の QtNetworkAuth 依存**。vcpkg / ビルド構成に Qt6 NetworkAuth があるか確認必要
2. **JSON-RPC 風 vs プレーン JSON**。既存サーバは後者。サーバ側スキーマ変更があれば追従必要
3. **リングバッファあふれ**。長時間オフラインで 256 件超。UI 側で警告する経路は M-COLLAB-2 側
4. **複数 project 同時接続**。本 milestone は 1 socket = 1 project 前提。複数 project 同時編集は別 milestone
5. **サーバ側の PING 受信 (RFC 6455)**。本 milestone は client → server PING のみ。双方向は将来

## 9. Done Criteria (本 milestone)

- `tools/collaboration-server` に対して `join` → `operation` 送信 → 他クライアントで `remoteOperation` シグナル受信、までが動く
- 切断 → 自動再接続 → 再送 (queue 経由) が動く
- 5 種メッセージ型すべてが `W_REGISTER_ARGTYPE` され、QMetaType 経由で運べる
- 既存 `CollabPresenceWidget` が `userJoined` / `userLeft` / `remotePresence` を購読して表示更新できる (統合は M-COLLAB-3)
- `ArtifactCore/CMakeLists.txt` の `_artifact_force_module` リストに `CollaborationWebSocket.cppm` が追記されている
- `.ixx` 側に **不要な `import` がない** (QWebSocket は実装側のみ)
- `module X;` 以降に `#include` がない
- 新規 `QImage` / `setStyleSheet` / 新規 global signal が増えていない
- `ArtifactWidgets` を触っていない
- 親-子リポ bump 手順が `.github/GIT_WORKFLOW_PARENT_CHILD.md` に整合

## 10. 想定工数

8-12 時間 (M-COLLAB-1 単独)。Phase 1 of `MILESTONE_TEAM_PROJECT_REALTIME_SYNC` と同等。

## 11. 更新履歴

- 2026-07-02: 初版作成。M-COLLAB-1 として `MILESTONE_TEAM_PROJECT_REALTIME_SYNC_2026-04-12.md` の Phase 1 実装を 1 段の milestone として切り出し。

  - `LockRequestMessage` / `LockGrantedMessage` / `LockDeniedMessage` / `LockReleasedMessage` / `LockUpdateMessage`
  - `PresenceMessage`
  - `UserJoinedMessage` / `UserLeftMessage`
  - `HistoryMessage` (新規参加時の過去ログ)
  - `ErrorMessage` (サーバ側 `{ "type": "error", "message": "..." }`)
- ハートビート (PING/PONG) は **クライアント主導で 25 秒間隔**。サーバ側 PING 受信枠も将来追加可能だが本 milestone では未対応
- 自動再接続: **指数バックオフ (1s, 2s, 4s, 8s, 16s, 30s 上限)**。最大 6 回で打ち切り

### 3.2 含まない (別 milestone)

- `EditSession` への broadcast フック (M-COLLAB-2)
- OT / CRDT による変換 (M-COLLAB-6)
- タイムライン側のロックインジケータ (M-COLLAB-3)
- `CollabPresenceWidget` の MainWindow 統合 (M-COLLAB-3)
- 認証 / TLS (P2 領域)
- 変更履歴ビューア (M-COLLAB-7)
- `ReactiveRule` 専用 bridge (M-COLLAB-5)
