# AIデバッガ拡張 実装マイルストーン

**日付**: 2026-08-02
**インフラ前提**: `DiagnosticRecorder`、`DiagnosticSnapshot`、`TraceRecorder`、`ExpressionEvaluator`、`MCP`、`Commandパターン` 完備

---

## 1. 自動根本原因解析（Root Cause Tracing）

### 1.1 概念

エラー発生時に `traceId` を辿って因果連鎖を自動トレースし、AIに読める説明を生成する。

```
エラー: "3D Layer not rendering" (sequence=482, traceId=82)
  ← source: "skip:mesh-not-loaded" (sequence=478, traceId=82)
     ← source: "meshLoaded_=false; sourcePath=""" (sequence=452, traceId=82)
        ← source: "Artifact3DLayer created with FixedGeometry::Cube" (sequence=400, traceId=82)
           ← ROOT CAUSE: Cube は createFixedGeometryMesh を使うが、その内部で setSourcePath を呼んでいない
```

### 1.2 実装

```cpp
class RootCauseAnalyzer {
public:
    struct CauseChain {
        std::vector<DiagnosticEvent> chain;   // 新しい順
        std::string rootCauseSummary;
        float confidence;
        std::vector<std::string> suggestedFixes;
    };

    /// 指定 traceId の全イベントを取得し、因果連鎖を構築
    CauseChain analyze(uint64_t traceId);

    /// 最新のエラーイベントから自動解析
    CauseChain analyzeLatestError();

    /// MCP ツールとして公開
    /// debug.rootCause {traceId: 82} → 因果連鎖JSON
};
```

### 1.3 アルゴリズム

```
1. DiagnosticRecorder::instance() から traceId で全イベントを取得
2. sequence 順にソート
3. イベントを component→component の遷移としてパース
   - "Artifact3DLayer::draw" → "skip:mesh-not-loaded"
   - "Artifact3DLayer::loadFromFile" → "sourcePath=empty"
   - "ArtifactLayerFactory::createLayer" → "Model3D with FixedGeometry::Cube"
4. 遷移グラフから最長パス = 因果連鎖
5. 最深ノードを ROOT CAUSE としてマーク
6. 既知のバグパターンと照合して suggestedFix を提案
```

### 1.4 MCP レスポンス例

```json
{
  "rootCause": {
    "traceId": 82,
    "chain": [
      {"seq": 482, "component": "Artifact3DLayer", "operation": "draw", "message": "skip:mesh-not-loaded"},
      {"seq": 452, "component": "Artifact3DLayer", "operation": "loadFromFile", "message": "sourcePath empty"},
      {"seq": 400, "component": "ArtifactLayerFactory", "operation": "createLayer", "message": "Model3D FixedGeometry::Cube"}
    ],
    "rootCauseSummary": "Cube primitive created without sourcePath. Mesh remains unloaded because createFixedGeometryMesh calls createCubeMesh which does not set sourcePath.",
    "confidence": 0.94,
    "suggestedFixes": [
      "Set sourcePath to asset path before calling loadFromFile",
      "Or call createFixedGeometryMesh which auto-loads internal mesh"
    ]
  }
}
```

---

## 2. 差分デバッグ（Diff Debugging）

### 2.1 概念

「正常な状態」と「異常な状態」の間で変化した全プロパティを自動列挙する。バイナリサーチで変化点を特定する。

### 2.2 実装

```cpp
class DiffDebugger {
public:
    struct StateDiff {
        QString targetPath;       // "layer.xxx.meshLoaded"
        QString description;
        QVariant beforeValue;
        QVariant afterValue;
    };

    /// 2つのフレーム間で変化した全プロパティを列挙
    std::vector<StateDiff> diffFrames(
        int64_t frameA, int64_t frameB,
        const QStringList& targets  // 監視対象（空=全レイヤーの全プロパティ）
    );

    /// バイナリサーチで変化が発生した正確なフレームを特定
    int64_t bisectChangePoint(
        int64_t goodFrame, int64_t badFrame,
        const QString& targetExpression  // "layer('Cube').meshLoaded"
    );

    /// 全レイヤーの完全状態スナップショットを取得
    QJsonObject captureFullState(int64_t frame);
};
```

### 2.3 使用シナリオ（AI エージェントからの自動調査）

```
1. Frame 30: 正常（Cube が描画されている）
   Frame 35: 異常（Cube が消えた）
2. diffFrames(30, 35) →
   "layer.Cube.meshLoaded: true → false"
   "layer.Cube.sourcePath: 'cube.obj' → ''"
   "layer.Cube.vertexCount: 512 → 0"
3. bisectChangePoint(30, 35, "layer('Cube').meshLoaded") → 32
4. Frame 32 の前後を詳しく調査
```

### 2.4 効率化

10,000プロパティの全比較は重いため、最初は主要プロパティ（meshLoaded, sourcePath, vertexCount, opacity, position, scale）だけを比較し、差分が見つかったらそのレイヤーの全プロパティを展開する。

---

## 3. Semantic Object Query

### 3.1 概念

AI が自然言語に近い形で「特定の条件を満たすオブジェクト」を問い合わせられる。

```
「visible=false で、かつ GPU テクスチャを保持しているレイヤーを列挙」
「参照カウント > 1 だが、どの composition からも到達不能な SharedPtr を検出」
「今アクティブなエフェクトのうち、有効でないものを列挙」
```

### 3.2 実装

```cpp
class SemanticQueryEngine {
public:
    using FilterFn = std::function<bool(const QJsonObject&)>;

    /// 全レイヤーから条件にマッチするものを返す
    QJsonArray queryLayers(const QString& condition) const;

    /// 全オブジェクトから条件にマッチするものを返す
    QJsonArray queryObjects(const QString& condition) const;

    /// 到達不能な SharedPtr を検出（ガベージコレクション相当）
    QJsonArray detectOrphans() const;
};
```

### 3.3 クエリ言語（ミニマル）

```json
// MCP ツール呼び出し例:
{
  "class": "debug",
  "method": "query",
  "arguments": [{
    "type": "layers",
    "where": "visible == false && hasGpuTexture == true && type == 'ImageLayer'",
    "select": ["id", "name", "textureSize", "gpuMemoryBytes"],
    "orderBy": "gpuMemoryBytes desc",
    "limit": 10
  }]
}
```

これは `ExpressionEvaluator` の構文を流用して実装できる。

---

## 4. 再現レシピ生成（Reproduction Recipe）

### 4.1 概念

バグ発見後、編集操作履歴から最小再現手順を自動生成する。

### 4.2 前提

既存の `Command` パターン（`EditSession`、`LambdaCommand`、`SerializableCommand`）で全操作が記録されていることを前提とする。

```cpp
class RecipeGenerator {
public:
    struct RecipeStep {
        QString action;       // "createLayer", "setProperty", "moveToFrame"
        QJsonObject params;   // {"type": "Model3D", "fixedGeometry": "Cube"}
        int64_t frameAt;
    };

    struct Recipe {
        std::vector<RecipeStep> steps;
        QString title;         // "Reproduce: 3D Cube not rendering"
        QString expectedOutcome;
        QString actualBug;
        size_t costOfSteps;    // 重み（ステップ数×操作コスト）
    };

    /// 編集セッション履歴から、特定の障害イベントを引き起こした最小操作セットを抽出
    Recipe extractMinimalRecipe(
        uint64_t failureTraceId,
        int maxSteps = 20
    );
};
```

### 4.3 アルゴリズム

```
1. 障害 traceId に関連する全 DiagnosticEvent を取得
2. frameAt が最も早いイベント = バグが顕在化したフレーム
3. そのフレームより前の全編集操作を EditSession から取得
4. 操作を1つずつ取り除いて再シミュレーションし、バグが再現するか確認
5. 不要な操作を削った最小セット = 再現レシピ
```

### 4.4 出力例

```
Recipe: "3D Layer fails to render when FixedGeometry is Cube"
Steps:
  1. New Composition (1920x1080, 30fps)
  2. Layer → New → 3D Model
  3. Select layer, set FixedGeometry = Cube (instead of loading from file)
  4. Go to Frame 1
Expected: Wireframe/Solid cube visible in viewport
Actual:   Layer invisible. meshLoaded=false, mesh vertexCount=0
Fix:      Set geometry from file, or ensure createFixedGeometryMesh sets meshLoaded_
```

---

## 5. AI Predictive Watchpoint

### 5.1 概念

AI が「この領域は変更頻度が高く複雑で、バグが潜んでいる可能性が高い」と予測し、自動的にウォッチポイントを設定する。

### 5.2 ヒューリスティック

| シグナル | 重み | 説明 |
|---------|------|------|
| 最近の変更（git diff） | 0.35 | 直近変更されたファイルほどリスク高 |
| 過去のバグ発生頻度 | 0.30 | DiagnosticRecorder の履歴から |
| コード複雑度（行数/分岐数） | 0.20 | 300行超の関数は要注意 |
| 依存関係の深さ | 0.10 | 多くのモジュールから import されている |
| 型安全性の低さ | 0.05 | void*/dynamic_cast の多用 |

### 5.3 実装

```cpp
class PredictiveWatchpointEngine {
public:
    struct RiskScore {
        QString fileOrComponent;
        float score;             // 0-1
        QString reason;
        std::vector<QString> suggestedWatchTargets;
    };

    /// リスク評価を計算（AI が呼ぶ）
    std::vector<RiskScore> evaluateRisks();

    /// 高リスク領域に自動ウォッチポイントを設定
    void autoWatch(float threshold = 0.5f);
};
```

---

## ファイル一覧

| 機能 | ヘッダ | 実装 | 新規/変更 |
|------|--------|------|----------|
| RootCauseAnalyzer | `Diagnostics/RootCauseAnalyzer.ixx` | `.cppm` | 新規 |
| DiffDebugger | `Diagnostics/DiffDebugger.ixx` | `.cppm` | 新規 |
| SemanticQueryEngine | `Diagnostics/SemanticQueryEngine.ixx` | `.cppm` | 新規 |
| RecipeGenerator | `Diagnostics/RecipeGenerator.ixx` | `.cppm` | 新規 |
| PredictiveWatchpoint | `Diagnostics/PredictiveWatchpoint.ixx` | `.cppm` | 新規 |
| ExecutionFlowVisualizer | `Diagnostics/ExecutionFlowVisualizer.ixx` | `.cppm` | 新規 |
| StressTestRunner | `Diagnostics/StressTestRunner.ixx` | `.cppm` | 新規 |
| RegressionDetector | `Diagnostics/RegressionDetector.ixx` | `.cppm` | 新規 |
| SmartLogFilter | `Diagnostics/SmartLogFilter.ixx` | `.cppm` | 新規 |
| ToolExecutor | — | 変更 | ツール登録追加 |

---

## 実装規模と優先順位

| 優先度 | 機能 | 工数 | 理由 |
|--------|------|------|------|
| **P0** | RootCauseAnalyzer | **極小** | traceId でイベントを集めてソートするだけ。既存インフラ完璧 |
| **P0** | DiffDebugger | **小** | ExpressionEvaluator でプロパティ値を比較。バイナリサーチも単純 |
| **P0** | ExecutionFlowVisualizer | **極小** | DiagnosticScopeイベントをMermaidに変換するだけ |
| **P0** | SmartLogFilter | **極小** | traceIdでフィルタ＋重要度ランク付け |
| **P1** | LivePatching | **極小** | debug.setPropertyで即反映。既に設計済み |
| **P1** | SemanticQueryEngine | **中** | ミニマルなクエリ言語 + ObjectGraph 探索 |
| **P1** | RegressionDetector | **小** | 既知正常スナップショットと現在状態のdiff |
| **P2** | RecipeGenerator | **中** | EditSession 履歴が十分なら実装可能 |
| **P2** | StressTestRunner | **中** | 操作スクリプト実行＋メモリ/エラー監視 |
| **P3** | PredictiveWatchpoint | **中** | ヒューリスティックの調整が必要 |

---

## 6. Live Patching

### 6.1 概念

バグ発見 → MCP 経由でプロパティを即変更 → VPにリアルタイム反映。「原因はこれだと思う、試してみる」をAIが即実行できる。

### 6.2 すでに設計済み

`MILESTONE_MCP_LIVE_DEBUG` の `debug.setProperty` と `debug.evaluate` がそのまま Live Patching として機能する。

```
1. AI: "sourcePathが空だから meshLoaded=false だと思う"
2. debug.evaluate("layer('Cube').sourcePath") → ""
3. debug.setProperty("layer.Cube", "model.sourcePath", "sphere.obj")
4. debug.stepForward(1)
5. debug.evaluate("layer('Cube').meshLoaded") → true   ← 仮説確認！
```

### 6.3 拡張: トランザクションロールバック

```cpp
// LivePatching 専用: 変更前の状態を保存し、デバッグ後に戻せる
class LivePatchSession {
public:
    void begin();    // 現在の全状態をキャプチャ
    void apply(const QString& targetPath, const QVariant& newValue);
    void rollback(); // begin() 時点の状態に戻す
    void commit();   // 変更を確定
};
```

---

## 7. Execution Flow Visualization

### 7.1 概念

1フレーム分の `DiagnosticScope` イベントを **Mermaid シーケンス図**として生成し、AI が視覚的に理解できる形で返す。

```
sequenceDiagram
    CompositionRenderController->>Artifact3DLayer: draw()
    Artifact3DLayer->>Artifact3DLayer: transform3D.snapshotAt()
    Artifact3DLayer->>Artifact3DLayer: mesh_.vertexCount() → 0
    Artifact3DLayer-->>CompositionRenderController: skip:mesh-not-loaded (52μs)
    Note over Artifact3DLayer: meshLoaded_=false, sourcePath=""
```

### 7.2 実装

```cpp
class ExecutionFlowVisualizer {
public:
    /// 指定フレームの全 DiagnosticScope イベントから Mermaid 図を生成
    QString generateMermaid(int64_t frame);

    /// traceId のイベントに限定した Mermaid 図
    QString generateMermaidForTrace(uint64_t traceId);

    /// 特定の component に限定（例: "Artifact3DLayer" だけ）
    QString generateMermaidForComponent(int64_t frame, const QString& component);

    /// パフォーマンスホットスポット可視化（durationNs が大きい順に強調）
    QString generateFlameGraph(int64_t frame);
};
```

### 7.3 生成アルゴリズム

```
1. DiagnosticRecorder から frameIndex=target の全イベントを取得
2. sequence 順にソート
3. component名を actor に、operation名を message に変換
4. durationNs が 1ms 超のものには Note で所要時間を付記
5. Mermaid 構文に組み立て
6. コードブロックとして MCP レスポンスに埋め込み
```

### 7.4 MCP ツール

```json
{ "method": "tools/call", "params": { "tool": {
  "class": "debug", "method": "flow", "arguments": [{"frame": 35, "format": "mermaid"}]
}}}
```

→ 応答に Mermaid シーケンス図が含まれ、AI が即座に実行フローを把握できる。

---

## 8. Stress Test Automation

### 8.1 概念

MCP 経由で「この操作を N 回繰り返して壊れないか」を自動検証する。

### 8.2 実装

```cpp
struct StressTestScript {
    QString name;
    std::vector<StressTestStep> steps;
    int repeatCount = 100;
    int frameDelay = 0;    // 操作間の待機フレーム数
};

struct StressTestStep {
    QString action;        // "createLayer", "deleteLayer", "setProperty", "seekFrame"
    QJsonObject params;
};

struct StressTestResult {
    bool passed;
    int totalIterations;
    int failedAtIteration;
    QString failureReason;

    // 時系列メトリクス
    std::vector<float> frameTimeHistory;     // 各フレームのレンダリング時間
    std::vector<size_t> memoryUsageHistory;  // メモリ使用量の推移
    std::vector<int> diagnosticEventCountHistory; // イベント数の推移
    int crashCount;
    size_t peakMemoryBytes;
    double averageFrameTimeMs;
};
```

### 8.3 スクリプト例

```json
{
  "name": "Layer add/remove stress test",
  "repeatCount": 100,
  "steps": [
    {"action": "createLayer", "params": {"type": "Solid2D", "color": [1,0,0]}},
    {"action": "seekFrame", "params": {"frame": "next"}},
    {"action": "setProperty", "params": {"target": "layer.last", "path": "opacity", "value": 0.5}},
    {"action": "seekFrame", "params": {"frame": "next"}},
    {"action": "deleteLayer", "params": {"target": "layer.last"}},
    {"action": "seekFrame", "params": {"frame": "next"}}
  ]
}
```

### 8.4 メトリクス可視化

```
Iteration: 42/100
Frame Time: ████████░░ 8.2ms (avg: 7.8ms, max: 14.3ms at iter 23)
Memory:     ██████░░░░ 1.2GB (peak: 1.4GB at iter 23)
Diagnostic Events: 3 (1 warn, 0 error)
Status: ✅ Running
```

---

## 9. Regression Detection

### 9.1 概念

「修正前に正常だった状態」のスナップショットを保存し、修正後に自動比較。修正の副作用を瞬時に検出する。

### 9.2 実装

```cpp
class RegressionDetector {
public:
    /// 現在の状態スナップショットをベースラインとして保存
    void captureBaseline(const QString& name, const QStringList& targets);

    /// 現在の状態をベースラインと比較し、差分を報告
    std::vector<StateDiff> compare(const QString& baselineName);

    /// 自動: 変更された全プロパティのうち、予期しないものを検出
    /// （expectChanges に含まれない変更があれば回帰候補）
    struct RegressionReport {
        std::vector<StateDiff> expectedChanges;
        std::vector<StateDiff> unexpectedChanges;  // ← これが回帰
        bool hasRegression;
    };

    RegressionReport detect(const QString& baselineName,
                             const QStringList& expectChanges);
};
```

### 9.3 使用シナリオ

```
1. AI: "3Dレイヤーの修正をする。まずベースラインを取る"
2. debug.regression.capture "pre-3d-fix"
3. debug.setProperty → mesh sourcePath を修正
4. debug.regression.detect "pre-3d-fix" ["layer.Cube.sourcePath", "layer.Cube.meshLoaded"]
   → expectedChanges: 2件
   → unexpectedChanges: 0件 ← 回帰なし！修正成功
```

---

## 10. Smart Log Filtering

### 10.1 概念

エラーの traceId に関連するイベントだけを抽出し、無関係なログノイズを AI が自動除去する。

### 10.2 実装

```cpp
class SmartLogFilter {
public:
    struct RelevanceScore {
        DiagnosticEvent event;
        float score;            // 0-1: traceId との関連度
        QString reason;         // なぜ関連ありと判定したか
    };

    /// 指定 traceId に関連するイベントのみを関連度順に返す
    std::vector<RelevanceScore> filterByTrace(uint64_t traceId, int topN = 20);

    /// 時系列ウィンドウ内の重要イベントのみ抽出
    std::vector<RelevanceScore> filterImportant(
        int64_t fromFrame, int64_t toFrame,
        CoreDiagnosticSeverity minSeverity = CoreDiagnosticSeverity::Warning);

    /// 自動要約: 「Frame 35 で Warning 2件、Error 1件。全3イベントが traceId=82 に関連」
    QString summarize(int64_t frame);
};
```

### 10.3 フィルタリングロジック

```
関連度スコア = 
  same traceId:             +1.00（同一トレース）
  same component:           +0.40（同じコンポーネント）
  same threadId:            +0.15（同じスレッド）
  time proximity (<1ms):    +0.20（時間的近接）
  severity is Error/Fatal:  +0.30（重要なイベント）
  frameIndex nearby (±3):   +0.10（前後のフレーム）
```

---

## 全体まとめ: AI デバッガ機能一覧

| # | 機能 | 工数 | インパクト |
|---|------|------|-----------|
| 1 | RootCauseAnalyzer | 極小 | 🔥🔥🔥🔥🔥 |
| 2 | DiffDebugger | 小 | 🔥🔥🔥🔥 |
| 3 | ExecutionFlowVisualizer | 極小 | 🔥🔥🔥🔥🔥 |
| 4 | SmartLogFilter | 極小 | 🔥🔥🔥🔥 |
| 5 | LivePatching | 極小 | 🔥🔥🔥🔥 |
| 6 | RegressionDetector | 小 | 🔥🔥🔥🔥 |
| 7 | SemanticQueryEngine | 中 | 🔥🔥🔥 |
| 8 | StressTestRunner | 中 | 🔥🔥🔥 |
| 9 | RecipeGenerator | 中 | 🔥🔥🔥 |
| 10 | PredictiveWatchpoint | 中 | 🔥🔥 |
| — | DataBreakpoint | 小 | 🔥🔥🔥 |
| — | Watchpoint | 小 | 🔥🔥🔥 |
| — | Stepper | 小 | 🔥🔥🔥🔥 |
| — | MemoryInspector | 中 | 🔥🔥🔥 |
| — | GPUInspector | 中 | 🔥🔥🔥 |

**全15機能。P0 6つはほぼ配線だけで動く。トータル 10 新規ファイル。**
