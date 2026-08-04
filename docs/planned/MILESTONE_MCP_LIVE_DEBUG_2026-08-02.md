# MCP ライブデバッグサーバー 実装マイルストーン

**日付**: 2026-08-02
**現状**: MCP 基盤は**完全に実装済み**。残るはデバッグツールの登録のみ。
**既存インフラ**:
- `McpBridge.ixx` — JSON-RPC 2.0 フレーミング（Content-Length ヘッダ）、`initialize`/`tools/list`/`tools/call`/`ping`
- `McpTransport.ixx` — `QProcess` ベースの MCP トランスポート。タイムアウト、エラーハンドリング完備
- `ToolBridge.ixx` — ツール実行ブリッジ。`executeToolCall()`、`toolSchemaJson()`、コンポーネント登録
- **80ファイル以上で `qDebug`/`qWarning`/`qInfo`/`qCritical` が使用済み** — ログソースは豊富

**やるべきこと**: デバッグ用 MCP ツールを `ToolExecutor` に登録するだけ。

---

## Phase 1: デバッグツールの登録

### Step 1.1 — DebugLogBuffer

既存の全ログ出力 (`qDebug`/`qWarning`/`qInfo`/`qCritical`) をインターセプトしてリングバッファに蓄積:

```cpp
class DebugLogBuffer : public QObject {
public:
    static DebugLogBuffer* instance();

    void install();  // qInstallMessageHandler

    struct LogEntry {
        QDateTime timestamp;
        QtMsgType type;
        QString category;
        QString message;
    };

    std::vector<LogEntry> recent(int count = 200) const;
    std::vector<LogEntry> filterByCategory(const QString& category, int count = 100) const;
    void clear();
    
    // リングバッファ（最新10,000行を保持）
    static constexpr int kMaxEntries = 10000;

private:
    void append(LogEntry&& entry);
    std::deque<LogEntry> buffer_;
    std::mutex mutex_;
};
```

### Step 1.2 — デバッグツール群

`ToolExecutor` に登録するツール一覧:

```json
{
  "tools": [
    {"name": "debug.log", "description": "直近のログエントリを取得"},
    {"name": "debug.logCategory", "description": "カテゴリでフィルタしたログを取得"},
    {"name": "debug.compositionState", "description": "コンポジションの内部状態を取得（レイヤー数、フレーム、キャッシュ状態等）"},
    {"name": "debug.layerState", "description": "指定レイヤーの詳細状態を取得"},
    {"name": "debug.renderGraph", "description": "RenderGraph の現在のパス一覧を取得"},
    {"name": "debug.renderQueue", "description": "レンダーキューの状態を取得"},
    {"name": "debug.gpuMemory", "description": "GPU メモリ使用量とテクスチャキャッシュ状態を取得"},
    {"name": "debug.performance", "description": "フレーム時間、FPS、CPU/GPU 使用率を取得"},
    {"name": "debug.evaluate", "description": "任意の式を評価（ExpressionEvaluator 経由）"},
    {"name": "debug.getProperty", "description": "レイヤーのプロパティ値を取得（完全パス指定）"},
    {"name": "debug.setProperty", "description": "レイヤーのプロパティ値を設定（完全パス指定）"},
    {"name": "debug.listLayers", "description": "全レイヤー一覧（名前/ID/可視性/ロック/種類）"},
    {"name": "debug.listCompositions", "description": "全コンポジション一覧（名前/サイズ/フレームレート/フレーム数）"},
    {"name": "debug.getSelection", "description": "現在の選択状態を取得"},
    {"name": "debug.getTools", "description": "現在のアクティブツールを取得"},
    {"name": "debug.getViewport", "description": "VP状態（位置/サイズ/ズーム/パン）を取得"},
    {"name": "debug.getTimeline", "description": "タイムライン状態（現在フレーム/範囲/ワークエリア）を取得"},
    {"name": "debug.getFarm", "description": "Farm worker 状態一覧を取得"},
    {"name": "debug.getRig", "description": "Rig2D の全ボーン/コントロール/制約状態を取得"},
    {"name": "debug.getColorPipeline", "description": "現在のカラーパイプライン設定（OCIO/LUT/workingSpace）を取得"},
    {"name": "debug.getMaskPaths", "description": "選択レイヤーの全マスクパス頂点データを取得"}
  ]
}
```

### Step 1.3 — 登録コード

各ツールは既存のシングルトン/インスタンスからデータを取得するだけ:

```cpp
// debug.log → DebugLogBuffer::instance()->recent(200)
// debug.layerState → CompositionRenderController から layer->debugState()
// debug.getProperty → layer->getProperty(path)->getValue()
// debug.evaluate → ExpressionEvaluator::evaluate(expression)
// debug.getRig → Rig2D::debugState() または bone/control をイテレート
```

---

## Phase 2: DebugServer 起動

### Step 2.1 — ArtifactDebugServer

**新規**: `Artifact/include/Service/ArtifactDebugServer.ixx` + `.cppm`

```cpp
class ArtifactDebugServer : public QObject {
public:
    static ArtifactDebugServer* instance();

    // MCP サーバーモード
    enum class ServerMode { Stdio, Tcp };
    
    bool start(ServerMode mode = ServerMode::Stdio, int tcpPort = 9877);
    void stop();
    bool isRunning() const;
    
    // 自動的に DebugLogBuffer をインストールする
    void enableLogCapture(bool enable = true);
};
```

### Step 2.2 — Stdio モード

メインプロセス内で動作。`stdin`/`stdout` で MCP フレームをやり取り:

```cpp
// stdin から MCP リクエストを読み取り、stdout にレスポンスを書き出す
void ArtifactDebugServer::processStdio() {
    QByteArray requestBuffer;
    while (running_) {
        // stdin から読み取り
        QByteArray chunk = readStdin(4096);
        requestBuffer.append(chunk);
        
        // 完全な MCP フレームを処理
        QJsonObject request;
        while (McpBridge::tryPopFrame(&requestBuffer, &request)) {
            QJsonObject response = McpBridge::handleRequest(request);
            QByteArray frame = McpBridge::encodeFrame(response);
            writeStdout(frame);
        }
    }
}
```

### Step 2.3 — TCP モード

リモートデバッグ用。`QTcpServer` で接続を受け付け:

```cpp
bool ArtifactDebugServer::startTcp(int port) {
    tcpServer_ = std::make_unique<QTcpServer>();
    connect(tcpServer_.get(), &QTcpServer::newConnection, [this]() {
        // クライアントごとに処理スレッド
    });
    return tcpServer_->listen(QHostAddress::LocalHost, port);
}
```

---

## Phase 3: 使用シナリオ

### 3.1 起動

```bash
# Artifact 起動時に MCP デバッグを有効化
ArtifactStudio.exe --mcp-debug --mcp-port 9877
```

### 3.2 外部ツールからの接続例

CommandCode や他の MCP クライアントから:

```json
// → tools/call
{"jsonrpc":"2.0","method":"tools/call","params":{"tool":{"class":"debug","method":"log"}},"id":1}

// ← response
{"jsonrpc":"2.0","id":1,"result":{"content":[
  "[14:32:01.234 WARN] Artifact3DLayer: mesh-not-loaded id={uuid}",
  "[14:32:01.456 INFO] CompositionRenderController: drawLayerForCompositionView skip bounds id={uuid}",
  "[14:32:01.789 WARN] OCIOConfig: Unknown display view for 'sRGB'"
]}}
```

### 3.3 リアルタイムログ監視

```json
// 5秒おきにポーリング → 新しいログエントリだけ取得
{"method":"tools/call","params":{"tool":{"class":"debug","method":"log","arguments":[{"since":"2026-08-02T14:30:00Z"}]}}}
```

### 3.4 3Dレイヤー描画バグ調査シナリオ

```
1. debug.listLayers → "Cube" レイヤーの id を特定
2. debug.layerState {layerId: "xxx"} → meshLoaded_=false を発見
3. debug.log {category: "Artifact3DLayer"} → "skip:mesh-not-loaded" のログを確認
4. debug.evaluate {expression: "layer('xxx').meshLoaded"} → false
5. debug.setProperty {path: "model.sourcePath", value: "cube.obj"} → meshLoaded_=true
6. debug.layerState → meshLoaded_=true, 描画成功
```

---

## ファイル一覧

| フェーズ | ファイル | 新規/変更 | 内容 |
|---------|----------|----------|------|
| P1 | `Artifact/include/Service/DebugLogBuffer.ixx` | 新規 | ログインターセプトリングバッファ |
| P1 | `Artifact/src/Service/DebugLogBuffer.cppm` | 新規 | 実装 |
| P1 | `Artifact/src/Tool/AIToolDSL/` (ToolExecutor) | 変更 | デバッグツール登録 |
| P2 | `Artifact/include/Service/ArtifactDebugServer.ixx` | 新規 | MCP デバッグサーバー |
| P2 | `Artifact/src/Service/ArtifactDebugServer.cppm` | 新規 | 実装 |
| P2 | `Artifact/src/AppMain.cppm` | 変更 | 起動時デバッグサーバー開始 |

---

## 検証チェックリスト

- [ ] `debug.log` が直近の qDebug 出力を返す
- [ ] `debug.layerState` が meshLoaded_/renderMode_/sourcePath_ を正しく返す
- [ ] `debug.evaluate` で式が評価できる
- [ ] `debug.getProperty` / `debug.setProperty` でプロパティ読み書きができる
- [ ] `debug.getViewport` がズーム/パン/解像度を返す
- [ ] CommandCode から `tools/call` で全ツールが実行できる
- [ ] Stdio モードと TCP モードの両方で動作
- [ ] 複数クライアントの同時接続が可能

---

## 実装規模見積もり

| 項目 | 工数 | 理由 |
|------|------|------|
| DebugLogBuffer | **極小** | リングバッファ + qInstallMessageHandler |
| ツール登録（20種） | **小〜中** | 各ツールは既存シングルトンの getter 呼び出しのみ。新規ロジック不要 |
| DebugServer | **小** | Stdio/TCP リードループは 100 行程度 |
| AppMain 統合 | **極小** | コマンドライン解析 + 起動時サーバー開始 |

**合計**: 小（新規3ファイル + 既存2ファイル変更）。既存の MCP/ToolBridge インフラがあるため、ほとんどがボイラープレート。
