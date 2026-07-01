# M-RE-2.A Network RPC Client Milestone

作成日: 2026-07-02
位置づけ: `ArtifactCore/NetworkRPCServer.ixx` (TCP/JSON-RPC 2.0 サーバ) の **対となるクライアント** を 1 モジュールに切り出す。
`docs/planned/MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` (Phase 1〜5) の **Phase 4 (out-of-process worker)**
を成立させるための前提部品。

参照:
- `ArtifactCore/NetworkRPCServer.ixx` — 既存サーバ。`register / heartbeat / assignJob / status / frameCompleted / frameFailed` を扱う
- `ArtifactCore/src/Network/NetworkRPCServer.cppm` — 既存サーバ実装
- `ArtifactCore/include/Render/RenderFarmMaster.ixx` — `startRpcServer()` でサーバ起動側
- `ArtifactCore/src/Render/RenderFarmMaster.cppm` — 既存 master 実装
- `ArtifactCore/include/Render/RenderFarmTypes.ixx` — `RenderFrameRange` 等の契約
- `ArtifactCore/include/Render/RenderFarmWorker.ixx` / `RenderFarmWorker.cppm` — in-process worker (別系統)
- `ArtifactRenderer/src/main.cpp` — 既存 CLI (single `--job` モード)
- `docs/planned/MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md` — 親/子プロセス分離の契約
- `docs/planned/MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` — 上位 milestone
- `docs/done/MILESTONE_EXTERNAL_RENDERER_DESIGN_PHASE1_2026-06-25.md` — 親/子分離 Phase 1 done

---

## 1. 目的

- `NetworkRPCServer` のメソッド語彙 (`register / heartbeat / assignJob / status / frameCompleted / frameFailed`) に対応する **Qt クライアント** を 1 モジュールに閉じる
- `ArtifactRenderer` の **farm-client モード** (`--farm-client <host:port> --worker-id <id>`) を成立させる
- `ImmediateContext` / `IDeviceContext` 等の低レベルハンドルを **wire で渡さない** 契約 (既存 `M-IR-8` / `RENDER_BOUNDARY_SAFETY_GATE`) を厳守
- `ArtifactAbstractLayer / ArtifactAbstractEffect` の live object を **wire に乗せない** (snapshot 経由のみ)

## 2. 現状整理 (2026-07-02 基準)

### 2.1 既存資産

| 要素 | 場所 | 状態 |
|------|------|------|
| TCP/JSON-RPC 2.0 サーバ | `ArtifactCore/NetworkRPCServer.ixx` + `src/Network/NetworkRPCServer.cppm` | 100% 実装。`register / heartbeat / assignJob / status` |
| Render Farm master | `ArtifactCore/include/Render/RenderFarmMaster.ixx` + `.cppm` | 100%。`startRpcServer()` で in-process farm の上に RPC サーバ起動 |
| サーバ RPC ハンドラ | `RenderFarmMaster.cppm` の `setOnRequest` | `frameCompleted / frameFailed` 2 種を処理 |
| 既存 `ArtifactRenderer` | `ArtifactRenderer/src/main.cpp` | `--job <file>` 単発モード。farm-client モードなし |
| Checkpoint / Retry / Log | `ArtifactCore/include/Render/CheckpointStore.ixx`, `LogCollector.ixx`, `ProgressAggregator.ixx` | 100% (master 側) |

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| `NetworkRPCClient` モジュール | **不在** (CMake にも名前だけ) | farm-client が実装できない |
| `ArtifactRenderer` の farm-client 入口 | **不在** | 別プロセス worker として master に繋げない |
| worker capability 検出 (GPU 数 / VRAM / disk) | **不在** | スケジューラに渡せる情報が空 |
| master からの `cancel` 通知 | **不在** | 別 process 停止の経路がない |

### 2.3 設計方針 (AGENTS.md 整合)

- **`ImmediateContext` / `IDeviceContext` を wire に乗せない**。`RenderJobRequest::renderFrame` のような `std::function` メンバは送らない
- ペイロードは **job snapshot JSON** のみ。`ArtifactCore/src/Render/ArtifactJobContract*` のスキーマに従う
- **global signal を新規追加しない**。`QObject` 派生 + `Q_SIGNAL` 経由
- `.ixx` 側に **不必要な `import` を入れない**。`QTcpSocket` は実装側のみ
- **`module X;` 以降に `#include` を追加しない**。`QTcpSocket` / `QJsonObject` は global module fragment に
- **`QImage` / `setStyleSheet` / `QColorDialog` は新規採用しない**
- 親-子リポ bump: 親 `ArtifactStudio` の gitlink 更新を `.github/GIT_WORKFLOW_PARENT_CHILD.md` に従う
- `ArtifactCore/CMakeLists.txt` の force list に **`NetworkRPCClient.cppm`** を追記 (CMake には既に `NetworkRPCClient.ixx` のエントリがあり、実体未作成状態)
- `ArtifactWidgets` サブモジュールは **触らない**
- `ArtifactRenderer` は既存 `tools/collaboration-server` とは別系統。`M-COLLAB-1` とは独立

## 3. スコープ

### 3.1 含む

- `ArtifactCore/include/Network/NetworkRPCClient.ixx` — `Network.RPCClient` 公開 API
- `ArtifactCore/src/Network/NetworkRPCClient.cppm` — 実装本体
- 既存 `NetworkRPCServer` との 1:1 対応メソッド:

## 4. 公開 API 概要 (ixx 側)

```cpp
export module Network.RPCClient;

export namespace ArtifactCore {

enum class RPCClientState {
    Disconnected,
    Connecting,
    Connected,
    Registering,
    Idle,         // 接続済・job 待ち
    Rendering,    // assignJob 受信・レンダ中
    Cancelling,
    Error
};

struct WorkerCapability {
    int gpus = 1;
    qint64 gpusMemoryMb = 0;
    qint64 diskFreeMb = 0;
    QString osVersion;
};

class NetworkRPCClient : public QObject {
    W_OBJECT(NetworkRPCClient)
public:
    explicit NetworkRPCClient(QObject* parent = nullptr);
    ~NetworkRPCClient();

    // 接続
    bool connectToMaster(const QString& host, unsigned short port,
                          const QString& workerId, const WorkerCapability& cap);
    void disconnect();
    bool isConnected() const;
    RPCClientState state() const;

    // 接続後ハンドラ設定 (master 側 `assignJob` / `cancel` 受信)
    void setOnJobAssigned(std::function<void(const QJsonObject& jobJson)> handler);
    void setOnCancelRequested(std::function<void()> handler);

    // 送信 (master へ報告)
    void reportFrameCompleted(int frame, const QString& outputPath);
    void reportFrameFailed(int frame, const QString& errorMessage);
    void sendHeartbeat();

    // 状態シグナル
    void stateChanged(RPCClientState s)            W_SIGNAL(stateChanged, s);
    void jobAssigned(const QJsonObject& jobJson)    W_SIGNAL(jobAssigned, jobJson);
    void cancelRequested()                          W_SIGNAL(cancelRequested);
    void errorOccurred(const QString& message)      W_SIGNAL(errorOccurred, message);

private:
    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
```

## 5. 実装ファイル (.cppm) の責務

- `QTcpSocket` 実体保持とライフサイクル
- 内部 queue: 接続中以外の report は **最大 512 件** のリングバッファで保持
- ハートビート: `QTimer` 10 秒間隔 (サーバ側タイムアウト 30s 未満)
- JSON 組み立て: `{ "jsonrpc": "2.0", "method": "frameCompleted", "params": {...}, "id": <n> }`
- 受信: `assignJob` 受信時 `jobAssigned` シグナル発火。`status` 受信時は `reportFrameCompleted(0, "")` で alive 応答
- ログ: `qDebug()` ベース

## 6. テスト方針

- `ArtifactCore/tests/` 配下に軽量ユニットテスト
- 検証内容:
  - JSON スキーマ整合 (6 種メソッドの serialize/deserialize)
  - 切断 → 再接続のシーケンス
  - queue 溢れ挙動 (テスト用 1 件 queue に切替)
  - 不正 JSON 受信で `errorOccurred` が出ること
- 結合テスト: `ArtifactRenderer` (CLI) を farm-client モードで起動し、master (`RenderFarmMaster::startRpcServer()`) に register され、`assignJob` 受信までを確認

## 7. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` | 本 milestone はその **Phase 4 (out-of-process worker)** の前提部品 |
| `MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md` | 親/子分離の契約。本 milestone は RPC 層の client 側 |
| `MILESTONE_EXTERNAL_RENDERER_DESIGN_PHASE1_2026-06-25.md` | Phase 1 done。本 milestone はその上位 |
| `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` | `ImmediateContext` を wire で共有しない方針。本 milestone で厳守 |
| `MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md` | `M-IR-8` を侵さない |

## 8. リスクと未解決論点

1. **サーバ側 protocol の JSON-RPC 2.0 互換性**。既存 `NetworkRPCServer.cppm` は `jsonrpc: "2.0"` を使うが、サーバハンドラは `method` ベースで非標準の拡張 (例: `params` を直接 object として扱う) を含む。クライアント側は厳密に 2.0 準拠し、サーバ拡張に依存しない
2. **TCP 切断検出の遅延**。`QTcpSocket::disconnected` を信頼。ハーフクローズ (送信だけ成功・受信だけ失敗) は OS 依存。本 milestone では明示 heartbeat 抜けを dead 判定基準にする
3. **認証なし**。LAN 前提。トークン認証は Phase 5
4. **Capability の手動指定**。v0 は `--gpus <N>` 引数。OS 依存の自動検出は別 milestone
5. **cancel 通知**。サーバ側ハンドラ未実装。本 milestone では受信シグナル定義のみ

## 9. Done Criteria (本 milestone)

- `ArtifactCore/CMakeLists.txt` の `_artifact_force_module` リストに `NetworkRPCClient.cppm` が追記されている
- 既存 `NetworkRPCClient.ixx` (CMake の GLOB 対象として既に列挙) の実体 `.cppm` が作成され、ビルド対象として登録される
- `NetworkRPCClient` 単体で `connectToMaster` → `register` → 受信 `assignJob` → `reportFrameCompleted` までがソケット越しに動作
- `ImmediateContext` / `IDeviceContext` / `ArtifactAbstractLayer*` / `ArtifactAbstractEffect*` の **生ポインタが wire を跨がない** ことを `grep` ベースで確認
- 既存 `NetworkRPCServer` の API と 1:1 対応 (6 メソッド)
- `.ixx` 側に **不要な `import` がない** (`QTcpSocket` は実装側のみ)
- `module X;` 以降に `#include` がない
- 新規 `QImage` / `setStyleSheet` / 新規 global signal が増えていない
- `ArtifactWidgets` を触っていない
- 親-子リポ bump 手順が `.github/GIT_WORKFLOW_PARENT_CHILD.md` に整合

## 10. 想定工数

8-12 時間 (M-RE-2.A 単独)。`MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` の Phase 4 準備に相当。

## 11. 更新履歴

- 2026-07-02: 初版作成。M-RE-2.A として `MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` の Phase 4 (out-of-process worker) の前提部品を 1 段の milestone として切り出し。

  - `register` (workerId, capability)
  - `heartbeat` (workerId, timestamp)
  - `assignJob` (jobJson) — サーバから受け
  - `status` (workerId) — サーバから要求
  - `frameCompleted` (workerId, frame, outputPath)
  - `frameFailed` (workerId, frame, errorMessage)
- 再接続: TCP 切断検出 → 3 秒待機 → 再 connect を 3 回まで。打ち切り後 `errorOccurred` を発
- capability 検出 (Windows 限定): GPU 数は `IDXGIAdapter` 列挙 (将来 Phase)。v0 は **引数 `--gpus <N>` で手動指定**
- cancel 通知: master 側未実装のため本 milestone では **メソッドスタブのみ**

### 3.2 含まない (別 milestone)

- `ArtifactRenderer` の farm-client CLI 統合 (M-RE-2.B)
- `ArtifactRenderQueueService` からの farm 投入導線 (M-RE-2.C)
- Affinity / 認証 / TLS (Phase 4-5)
- GPU capability 自動検出 (別 topic、OS 依存 path 必要)
- ログ集約 (`<project_root>/.artifact/farm/logs/`)

