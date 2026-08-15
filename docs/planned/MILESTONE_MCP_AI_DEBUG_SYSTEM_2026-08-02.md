# MCP AI デバッグシステム 統合マイルストーン

**ステータス:** Protocol and tool surface substantial / live integration acceptance pending

**最終更新:** 2026-08-15
**日付**: 2026-08-02
**統合元**: `MILESTONE_MCP_LIVE_DEBUG`, `MILESTONE_MCP_DEBUG_BREAKPOINTS`, `MILESTONE_AI_DEBUGGER_EXTENSIONS`
**先行成果**: `DEBUG_MCP_PSEUDO_BREAKPOINT_PLAN_2026-06-04`（初期構想）, `DEBUG_AGENT_RESIDENT_VERTICAL_SLICE_2026-07-26`（常駐 Debug Agent 実装済み）

## 現行コード監査 (2026-08-15)

`McpBridge` は JSON-RPC framing、tools/list・tools/call、ログ／state／composition／layer／render queue／GPU memory／property／step／pause／trace／breakpoint／watchpoint／patch／query／performance 系の分岐を持つ。`McpTransport` は QProcess ベースのセッションを提供し、`Trace`／`FrameDebug` には render snapshot の記録経路がある。したがって初期計画の「21種ツール未実装」という前提は古い。一方、実行中アプリへの接続、実際の `debug-bridge.json`／state file 更新、safe write の権限・rollback、各ツールの UI／renderer 状態との完全な一致は今回の静的確認では受入できないため、統合は未完として扱う。

## 既存インフラ（すべて実装済み）

| コンポーネント | 機能 |
|---------------|------|
| `McpBridge.ixx` | JSON-RPC 2.0 フレーミング、`initialize`/`tools/list`/`tools/call`/`ping` |
| `McpTransport.ixx` | `QProcess` ベース MCP トランスポート |
| `ToolBridge.ixx` | ツール実行ブリッジ、`executeToolCall()`、`toolSchemaJson()` |
| `DiagnosticRecorder` | スレッドセーフなイベントレコーダー |
| `DiagnosticScope` | RAII スコープ計測 |
| `DiagnosticSnapshot` | 全イベントの JSON シリアライズ |
| `TraceRecorder` | スレッドセーフトレース |
| `SessionLedger` | リカバリーポイント付きセッション台帳 |
| `DebugIdentity` | `std::source_location` ベースのオブジェクト識別 |
| `ExpressionEvaluator` | 任意の式を評価 |
| `CrashHandler` | ネイティブクラッシュのスタックトレース記録 |
| `ArtifactDebugAgent` | 常駐 Debug Agent（playback frame 監視、`debug-bridge.json` 出力、`session.paused` 連携）|
| ログソース | **80ファイル以上**で `qDebug`/`qWarning`/`qInfo`/`qCritical` 使用済み |

---

## Phase 1: ライブデバッグ基盤

### 1.1 DebugLogBuffer

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

    static constexpr int kMaxEntries = 10000;
};
```

### 1.2 基本デバッグツール（21種）

`ToolExecutor` に登録：

```json
{
  "tools": [
    {"name": "debug.log",                   "description": "直近のログエントリを取得"},
    {"name": "debug.logCategory",           "description": "カテゴリでフィルタしたログを取得"},
    {"name": "debug.compositionState",      "description": "コンポジションの内部状態を取得"},
    {"name": "debug.layerState",            "description": "指定レイヤーの詳細状態を取得"},
    {"name": "debug.renderGraph",           "description": "RenderGraph の現在のパス一覧"},
    {"name": "debug.renderQueue",           "description": "レンダーキューの状態"},
    {"name": "debug.gpuMemory",             "description": "GPU メモリ使用量とテクスチャキャッシュ状態"},
    {"name": "debug.performance",           "description": "フレーム時間、FPS、CPU/GPU 使用率"},
    {"name": "debug.evaluate",              "description": "任意の式を評価（ExpressionEvaluator 経由）"},
    {"name": "debug.getProperty",           "description": "レイヤーのプロパティ値を取得（完全パス指定）"},
    {"name": "debug.setProperty",           "description": "レイヤーのプロパティ値を設定（完全パス指定）"},
    {"name": "debug.listLayers",            "description": "全レイヤー一覧"},
    {"name": "debug.listCompositions",      "description": "全コンポジション一覧"},
    {"name": "debug.getSelection",          "description": "現在の選択状態"},
    {"name": "debug.getTools",              "description": "現在のアクティブツール"},
    {"name": "debug.getViewport",           "description": "VP状態（位置/サイズ/ズーム/パン）"},
    {"name": "debug.getTimeline",           "description": "タイムライン状態"},
    {"name": "debug.getFarm",               "description": "Farm worker 状態一覧"},
    {"name": "debug.getRig",                "description": "Rig2D の全ボーン/コントロール/制約状態"},
    {"name": "debug.getColorPipeline",      "description": "カラーパイプライン設定"},
    {"name": "debug.getMaskPaths",          "description": "選択レイヤーの全マスクパス頂点データ"}
  ]
}
```

### 1.3 ArtifactDebugServer

```cpp
class ArtifactDebugServer : public QObject {
public:
    static ArtifactDebugServer* instance();

    enum class ServerMode { Stdio, Tcp };

    bool start(ServerMode mode = ServerMode::Stdio, int tcpPort = 9877);
    void stop();
    bool isRunning() const;
    void enableLogCapture(bool enable = true);
};
```

起動：`ArtifactStudio.exe --mcp-debug --mcp-port 9877`

| ファイル | 新規/変更 |
|---------|----------|
| `Artifact/include/Service/DebugLogBuffer.ixx` | 新規 |
| `Artifact/src/Service/DebugLogBuffer.cppm` | 新規 |
| `Artifact/include/Service/ArtifactDebugServer.ixx` | 新規 |
| `Artifact/src/Service/ArtifactDebugServer.cppm` | 新規 |
| `Artifact/src/Tool/AIToolDSL/` (ToolExecutor) | 変更 |
| `Artifact/src/AppMain.cppm` | 変更 |

---

## Phase 2: ブレークポイント・ステッピング

### 2.1 DataBreakpoint

「このプロパティが閾値を超えたら DiagnosticEvent を発行」

```cpp
struct DataBreakpoint {
    QString targetPath;
    float previousValue;
    float threshold;
    float changeTolerance;
    int frameInterval = 1;
    std::uint64_t lastTriggeredSequence = 0;
    bool enabled = true;
};
```

MCP: `debug.addDataBreakpoint` / `debug.removeDataBreakpoint` / `debug.listDataBreakpoints`

### 2.2 Conditional Watchpoint

「特定の条件が満たされたときだけ状態をキャプチャ」

```cpp
struct Watchpoint {
    QString expression;         // "layer('Cube').opacity < 0.5"
    QString captureTarget;
    QString snapshotName;
    bool oneShot = true;
    bool enabled = true;
};
```

MCP: `debug.addWatchpoint` / `debug.removeWatchpoint` / `debug.listWatchpoints`

### 2.3 MCPDebugStepper

アプリ全体を止めずに、MCP 経由でステップ実行

```cpp
class MCPDebugStepper {
public:
    void stepForward(int frames = 1);
    void stepToFrame(int64_t target);
    void pause();
    void resume();
    void setAutoCapture(const QStringList& targets);
    bool isPaused() const;
    int64_t currentStepFrame() const;
};
```

MCP: `debug.stepForward` / `debug.stepToFrame` / `debug.pause` / `debug.resume`

### 2.4 MemoryInspector

`DebugIdentity` ベースのオブジェクトグラフ探索

```cpp
class MemoryInspector {
public:
    struct ObjectInfo {
        uint64_t debugId;
        QString typeName;
        QString debugName;
        QString creationFile;
        uint32_t creationLine;
        uint64_t ownerId;
        size_t estimatedSize;
        int refCount;
    };

    std::vector<ObjectInfo> listAll() const;
    QString dumpObject(uint64_t debugId) const;
    QJsonObject objectGraph(uint64_t rootDebugId) const;
};
```

MCP: `debug.memory.list` / `debug.memory.dump` / `debug.memory.graph` / `debug.memory.leaks`

### 2.5 GPU Debug Inspector

```cpp
class GPUDebugInspector {
public:
    QJsonObject sampleTexture(const QString& textureName, int x, int y, int w, int h);
    QJsonObject readBuffer(const QString& bufferName, int offset, int count);
    QJsonObject gpuMemoryStats();
    QJsonObject boundResources();
};
```

| ファイル | 新規/変更 |
|---------|----------|
| `ArtifactCore/include/Diagnostics/DataBreakpoint.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/DataBreakpoint.cppm` | 新規 |
| `ArtifactCore/include/Diagnostics/Watchpoint.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/Watchpoint.cppm` | 新規 |
| `Artifact/include/Service/MCPDebugStepper.ixx` | 新規 |
| `Artifact/src/Service/MCPDebugStepper.cppm` | 新規 |
| `ArtifactCore/include/Diagnostics/MemoryInspector.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/MemoryInspector.cppm` | 新規 |
| `Artifact/include/Service/GPUDebugInspector.ixx` | 新規 |
| `Artifact/src/Service/GPUDebugInspector.cppm` | 新規 |
| `Artifact/src/Tool/AIToolDSL/` (ToolExecutor) | 変更 |

---

## Phase 3: AI 分析・自動化

### 3.1 RootCauseAnalyzer（P0・極小）

エラー発生時に `traceId` を辿って因果連鎖を自動トレース

```cpp
class RootCauseAnalyzer {
public:
    struct CauseChain {
        std::vector<DiagnosticEvent> chain;
        std::string rootCauseSummary;
        float confidence;
        std::vector<std::string> suggestedFixes;
    };

    CauseChain analyze(uint64_t traceId);
    CauseChain analyzeLatestError();
};
```

MCP: `debug.rootCause`

### 3.2 DiffDebugger（P0・小）

2フレーム間の全プロパティ変化を自動列挙＋バイナリサーチで変化点特定

```cpp
class DiffDebugger {
public:
    struct StateDiff {
        QString targetPath;
        QString description;
        QVariant beforeValue;
        QVariant afterValue;
    };

    std::vector<StateDiff> diffFrames(int64_t frameA, int64_t frameB, const QStringList& targets);
    int64_t bisectChangePoint(int64_t goodFrame, int64_t badFrame, const QString& targetExpression);
    QJsonObject captureFullState(int64_t frame);
};
```

MCP: `debug.diff` / `debug.bisect`

### 3.3 ExecutionFlowVisualizer（P0・極小）

1フレーム分の `DiagnosticScope` イベントを Mermaid シーケンス図として生成

```cpp
class ExecutionFlowVisualizer {
public:
    QString generateMermaid(int64_t frame);
    QString generateMermaidForTrace(uint64_t traceId);
    QString generateMermaidForComponent(int64_t frame, const QString& component);
    QString generateFlameGraph(int64_t frame);
};
```

MCP: `debug.flow`

### 3.4 SmartLogFilter（P0・極小）

エラーの `traceId` に関連するイベントのみを関連度順に抽出

```cpp
class SmartLogFilter {
public:
    struct RelevanceScore {
        DiagnosticEvent event;
        float score;
        QString reason;
    };

    std::vector<RelevanceScore> filterByTrace(uint64_t traceId, int topN = 20);
    std::vector<RelevanceScore> filterImportant(int64_t fromFrame, int64_t toFrame, CoreDiagnosticSeverity minSeverity);
    QString summarize(int64_t frame);
};
```

MCP: `debug.filterLog`

### 3.5 LivePatching（P1・極小）

Phase 1 の `debug.setProperty` / `debug.evaluate` をそのまま活用。トランザクションロールバックを追加：

```cpp
class LivePatchSession {
public:
    void begin();
    void apply(const QString& targetPath, const QVariant& newValue);
    void rollback();
    void commit();
};
```

MCP: `debug.patch.begin` / `debug.patch.apply` / `debug.patch.rollback` / `debug.patch.commit`

### 3.6 RegressionDetector（P1・小）

修正前の状態スナップショットをベースラインとして保存し、修正後に自動比較

```cpp
class RegressionDetector {
public:
    void captureBaseline(const QString& name, const QStringList& targets);
    std::vector<StateDiff> compare(const QString& baselineName);

    struct RegressionReport {
        std::vector<StateDiff> expectedChanges;
        std::vector<StateDiff> unexpectedChanges;
        bool hasRegression;
    };

    RegressionReport detect(const QString& baselineName, const QStringList& expectChanges);
};
```

MCP: `debug.regression.capture` / `debug.regression.compare` / `debug.regression.detect`

### 3.7 SemanticQueryEngine（P1・中）

自然言語に近い形でオブジェクトを検索

```cpp
class SemanticQueryEngine {
public:
    QJsonArray queryLayers(const QString& condition) const;
    QJsonArray queryObjects(const QString& condition) const;
    QJsonArray detectOrphans() const;
};
```

MCP: `debug.query`（ミニマルクエリ言語：`where`, `select`, `orderBy`, `limit`）

### 3.8 RecipeGenerator（P2・中）

編集操作履歴から最小再現手順を自動生成

```cpp
class RecipeGenerator {
public:
    struct Recipe {
        std::vector<RecipeStep> steps;
        QString title;
        QString expectedOutcome;
        QString actualBug;
        size_t costOfSteps;
    };

    Recipe extractMinimalRecipe(uint64_t failureTraceId, int maxSteps = 20);
};
```

MCP: `debug.recipe`

### 3.9 StressTestRunner（P2・中）

スクリプト化された操作を N 回繰り返し、メモリリークやフレームタイム劣化を監視

```cpp
struct StressTestScript {
    QString name;
    std::vector<StressTestStep> steps;
    int repeatCount = 100;
    int frameDelay = 0;
};

struct StressTestResult {
    bool passed;
    int totalIterations;
    int failedAtIteration;
    QString failureReason;
    std::vector<float> frameTimeHistory;
    std::vector<size_t> memoryUsageHistory;
    int crashCount;
    size_t peakMemoryBytes;
};
```

MCP: `debug.stress.run` / `debug.stress.result`

### 3.10 PredictiveWatchpoint（P3・中）

コード変更頻度・過去のバグ履歴・複雑度からリスク評価し、自動ウォッチポイント設定

```cpp
class PredictiveWatchpointEngine {
public:
    struct RiskScore {
        QString fileOrComponent;
        float score;
        QString reason;
        std::vector<QString> suggestedWatchTargets;
    };

    std::vector<RiskScore> evaluateRisks();
    void autoWatch(float threshold = 0.5f);
};
```

MCP: `debug.predict.risks` / `debug.predict.autoWatch`

| ファイル | 新規/変更 |
|---------|----------|
| `ArtifactCore/include/Diagnostics/RootCauseAnalyzer.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/RootCauseAnalyzer.cppm` | 新規 |
| `ArtifactCore/include/Diagnostics/DiffDebugger.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/DiffDebugger.cppm` | 新規 |
| `ArtifactCore/include/Diagnostics/ExecutionFlowVisualizer.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/ExecutionFlowVisualizer.cppm` | 新規 |
| `ArtifactCore/include/Diagnostics/SmartLogFilter.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/SmartLogFilter.cppm` | 新規 |
| `ArtifactCore/include/Diagnostics/RegressionDetector.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/RegressionDetector.cppm` | 新規 |
| `ArtifactCore/include/Diagnostics/SemanticQueryEngine.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/SemanticQueryEngine.cppm` | 新規 |
| `ArtifactCore/include/Diagnostics/RecipeGenerator.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/RecipeGenerator.cppm` | 新規 |
| `ArtifactCore/include/Diagnostics/StressTestRunner.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/StressTestRunner.cppm` | 新規 |
| `ArtifactCore/include/Diagnostics/PredictiveWatchpoint.ixx` | 新規 |
| `ArtifactCore/src/Diagnostics/PredictiveWatchpoint.cppm` | 新規 |
| `Artifact/src/Tool/AIToolDSL/` (ToolExecutor) | 変更 |

---

## 実装規模・優先順位サマリ

| 優先度 | 機能 | 工数 | インパクト |
|--------|------|------|-----------|
| **P0** | Phase 1 基本ツール + DebugServer | 小 | 🔥🔥🔥🔥🔥 |
| **P0** | RootCauseAnalyzer | 極小 | 🔥🔥🔥🔥🔥 |
| **P0** | ExecutionFlowVisualizer | 極小 | 🔥🔥🔥🔥🔥 |
| **P0** | DiffDebugger | 小 | 🔥🔥🔥🔥 |
| **P0** | SmartLogFilter | 極小 | 🔥🔥🔥🔥 |
| **P0** | LivePatching | 極小 | 🔥🔥🔥🔥 |
| **P1** | DataBreakpoint | 小 | 🔥🔥🔥 |
| **P1** | Watchpoint | 小 | 🔥🔥🔥 |
| **P1** | Stepper | 小 | 🔥🔥🔥🔥 |
| **P1** | RegressionDetector | 小 | 🔥🔥🔥🔥 |
| **P1** | SemanticQueryEngine | 中 | 🔥🔥🔥 |
| **P2** | MemoryInspector | 中 | 🔥🔥🔥 |
| **P2** | GPUInspector | 中 | 🔥🔥🔥 |
| **P2** | RecipeGenerator | 中 | 🔥🔥🔥 |
| **P2** | StressTestRunner | 中 | 🔥🔥🔥 |
| **P3** | PredictiveWatchpoint | 中 | 🔥🔥 |

**合計**: 新規約22ファイル + ToolExecutor/AppMain 変更。既存の MCP/DiagnosticRecorder/ExpressionEvaluator/DebugAgent インフラが完備しているため、大半が thin wrapper。

---

## 検証チェックリスト

- [x] Phase 1: `debug.log` が直近の qDebug 出力を返す（静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.logCategory` / `debug.getTools` をMCP tools/list・tools/callへ公開（静的実装確認済み、runtime未確認）
- [x] Phase 3: `debug.filterLog` でログ本文/contextを検索（静的実装確認済み、runtime未確認）
- [x] Phase 1: `--mcp-debug` をstdio MCPサーバー入口として受け付ける
- [x] Phase 1: `--mcp-debug --mcp-port <port>` でlocalhost TCP MCPサーバーを起動（静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.state` / `debug.pause` / `debug.resume` を既存の `debug-mcp-state.json` 契約へ接続（静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.trace` で `TraceRecorder` の直近スナップショットを取得（静的実装確認済み、runtime未確認）
- [x] Phase 3: `debug.flow` で直近traceからMermaid sequence diagramを生成（静的実装確認済み、runtime未確認）
- [x] Phase 3: `debug.rootCause` で最新crash/trace eventと関連イベントを返却（ヒューリスティック、runtime未確認）
- [x] Phase 3: `debug.diff` で2フレーム間のtrace scope差分を返却（静的実装確認済み、runtime未確認）
- [x] Phase 3: `debug.bisect` で観測済みtraceからscope出現frame候補を推定（ヒューリスティック、runtime未確認）
- [x] Phase 3: `debug.recipe` でTraceから再現手順候補を生成（ヒューリスティック、runtime未確認）
- [x] Phase 1: `debug.listProperties` / `debug.getProperty` で登録済みプロパティを読み取り（静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.setProperty` でread-only ownerを除くプロパティを更新（静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.evaluate` を `ExpressionEvaluator` に接続（静的実装確認済み、runtime未確認）
- [x] Phase 3: `debug.query` でプロパティpath/typeを条件検索（where/select/orderBy/limit、静的実装確認済み、runtime未確認）
- [x] Phase 3: `debug.patch.begin` / `debug.patch.apply` / `debug.patch.rollback` / `debug.patch.commit` を追加（プロセス内保持、静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.performance` でTrace frame timingからFPS・平均/直近フレーム時間を返却（静的実装確認済み、runtime未確認）
- [x] Phase 3: `debug.regression.capture` / `debug.regression.compare` でtrace frameベースラインを比較（プロセス内保持、静的実装確認済み、runtime未確認）
- [x] Phase 3: `debug.regression.detect` で期待差分を除外した予期しないframe差分を検出（静的実装確認済み、runtime未確認）
- [x] Phase 2: `debug.addDataBreakpoint` / `debug.removeDataBreakpoint` / `debug.listDataBreakpoints` を既存state契約へ接続（静的実装確認済み、runtime未確認）
- [x] Phase 2: `debug.addWatchpoint` / `debug.removeWatchpoint` / `debug.listWatchpoints` を既存state契約へ接続（静的実装確認済み、runtime未確認）
- [x] Phase 2: `debug.stepForward` / `debug.stepToFrame` をstate pollerとPlaybackServiceへ接続（静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.getSelection` / `debug.getViewport` をstate snapshotへ接続（静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.compositionState` / `debug.layerState` をstate snapshotへ接続（静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.listCompositions` / `debug.listLayers` をworkspace snapshotへ接続（未観測時は明示、静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.renderQueue` / `debug.getFarm` をsnapshotへ接続（未観測時は明示、静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.getColorPipeline` / `debug.getMaskPaths` をsnapshotへ接続（未観測時は明示、静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.renderGraph` でTraceから観測済みレンダーパスを列挙（静的実装確認済み、runtime未確認）
- [x] Phase 1: `debug.getTimeline` でTrace frame時系列とdurationを返却（静的実装確認済み、runtime未確認）
- [ ] Phase 1: CommandCode から `tools/call` で全基本ツールが実行できる
- [ ] Phase 1: Stdio / TCP 両モードで動作
- [x] Phase 2: DataBreakpoint が条件一致時に発火し、履歴とafter snapshotを保存（静的実装確認済み、runtime未確認）
- [x] Phase 2: Watchpoint が登録パスの値を定期取得し `lastWatchSnapshot` に保存（静的実装確認済み、runtime未確認）
- [x] Phase 2: Stepper がMCP状態経由でPlayback frameを更新（静的実装確認済み、runtime未確認）
- [x] Phase 2: MemoryInspector が `DebugIdentity` のライブ登録からオブジェクト／所有関係グラフを返す（静的実装確認済み、runtime未確認）
- [x] Phase 2: `debug.gpuMemory` / `debug.getRig` のsnapshot観測経路を追加（静的実装確認済み、runtime未確認）
- [x] Phase 2: `debug.stress.run` が ExpressionEvaluator の steps を反復実行し、失敗位置を返す（静的実装確認済み、runtime未確認）
- [x] Phase 3: `debug.predict.risks` / `debug.predict.autoWatch` をTraceとWatchpoint状態へ接続（ヒューリスティック、runtime未確認）
- [x] Phase 3: RootCauseAnalyzer相当の `debug.rootCause` ヒューリスティックをTraceへ接続（runtime未確認）
- [x] Phase 3: DiffDebugger相当の `debug.diff` をフレーム間差分へ接続（runtime未確認）
- [x] Phase 3: ExecutionFlowVisualizer相当の `debug.flow` Mermaid生成を追加（runtime未確認）
- [x] Phase 3: RecipeGenerator相当の `debug.recipe` 候補抽出を追加（runtime未確認）

### 実行確認メモ（2026-08-04）

- Node版stdioハーネスで `initialize` / `tools/list` / `tools/call(get_session_summary)` を実測成功。
- Node版stdioハーネスで `set_break_condition` / `list_break_conditions` / `set_debug_watch` / `step_one_tick` / `get_session_summary` / `get_break_history` を実測成功。
- Node版stdioハーネスで `set_mock_snapshot` → Breakpoint → `step_one_tick` → `get_last_break_hit` / `get_break_history` のモック状態経路を実測成功。
- Artifact統合版のビルドは、今回のMCP変更とは無関係な `ArtifactCore/src/Image/OpenEXR.cppm` の既存コンパイルエラーで停止。
- したがって、CommandCode経由の全ツール実行とArtifact統合版のstdio/TCP実動作は未確認のまま保持する。
