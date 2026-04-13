# チームプロジェクト 使用例

## Phase 1-4 統合後の基本的な使用フロー

### 1. サーバーの起動

```bash
cd tools/collaboration-server
npm install
npm start
# => Collaboration server started on port 8080
```

### 2. クライアント（ArtifactStudio）からの接続

```cpp
#include <Command/CollaborationManager.ixx>
#include <Command/EditSession.ixx>
#include <Command/LayerLockManager.ixx>

using namespace ArtifactCore;

// マネージャーの作成
auto collabManager = std::make_unique<CollaborationManager>();
auto lockManager = std::make_unique<LayerLockManager>();

// ユーザー情報の設定
collabManager->setUserInfo("user-001", "山田太郎", "#FF5722");

// EditSessionとの統合
EditSession* session = ...; // 既存のセッション
collabManager->setEditSession(session);

// サーバーに接続
collabManager->connectToServer("ws://localhost:8080", "proj-composition-001");

// コールバックの設定
collabManager->setRemoteOperationCallback([](const QJsonObject& op) {
    qDebug() << "Remote operation received:" << op;
    // UIの更新、レイヤーロックのチェック等
});

collabManager->setUserJoinedCallback([](const CollaborationManager::UserInfo& user) {
    qDebug() << "User joined:" << user.userName;
});

collabManager->setLockStateChangedCallback([](const QString& layerId, bool granted) {
    if (granted) {
        qDebug() << "Lock acquired:" << layerId;
    } else {
        qDebug() << "Lock denied:" << layerId;
    }
});
```

### 3. レイヤーのロック

```cpp
// レイヤー編集開始時にロックを要求
collabManager->requestLayerLock("layer-abc");

// ロック成功後、編集を開始
if (collabManager->hasLayerLock("layer-abc")) {
    // レイヤーの編集処理...
    // EditSession::pushCommand() で操作が自動的にブロードキャストされる
}

// 編集完了後、ロックを解放
collabManager->releaseLayerLock("layer-abc");
```

### 4. プレゼンスの更新

```cpp
// タイムラインでフレーム移動時
QJsonObject presence;
presence[QStringLiteral("cursorLocation")] = QStringLiteral("timeline:150");
presence[QStringLiteral("selectedLayers")] = QJsonArray{"layer-abc", "layer-def"};
collabManager->updatePresence(presence);
```

### 5. UI統合

```cpp
// プレゼンスウィジェットの配置
auto* presenceWidget = new CollabPresenceWidget(parent);
presenceWidget->setLocalUser("user-001", "山田太郎", QColor("#FF5722"));

// ユーザー参加時に追加
collabManager->setUserJoinedCallback([presenceWidget](const auto& user) {
    presenceWidget->addUser(user.userId, user.userName, QColor(user.userColor));
});

// ユーザー離脱時に削除
collabManager->setUserLeftCallback([presenceWidget](const auto& user) {
    presenceWidget->removeUser(user.userId);
});

// タイムラインのレイヤー行にロックインジケーター配置
auto* lockIndicator = new LayerLockIndicator(layerRowWidget);

// ロック状態の更新
lockManager->requestLock("layer-abc", "user-001", "山田太郎");
lockIndicator->setLocked(true, "山田太郎", "#FF5722");
```

---

## 接続状態の監視

```cpp
collabManager->setConnectionStateChangedCallback([](CollabConnectionState state) {
    switch (state) {
    case CollabConnectionState::Connected:
        qDebug() << "Connected to collaboration server";
        break;
    case CollabConnectionState::Disconnected:
        qDebug() << "Disconnected from collaboration server";
        break;
    case CollabConnectionState::Reconnecting:
        qDebug() << "Reconnecting...";
        break;
    case CollabConnectionState::Error:
        qDebug() << "Connection error";
        break;
    }
});
```

---

## オフラインからの再接続

```cpp
// 切断検知時に再接続を試みる
collabManager->setConnectionStateChangedCallback([collabManager](CollabConnectionState state) {
    if (state == CollabConnectionState::Disconnected) {
        // 3秒後に再接続を試みる
        QTimer::singleShot(3000, [collabManager]() {
            if (!collabManager->isConnected()) {
                // EditSessionは接続時に自動的に設定される
                collabManager->connectToServer(serverUrl, projectId);
            }
        });
    }
});

// 再接続後、未送信の操作があれば送信
// EditSessionの履歴とサーバーのバージョンを比較して欠落を検出
```
