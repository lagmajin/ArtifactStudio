# MCP ブレークポイント / メモリウォッチ 実装マイルストーン

**日付**: 2026-08-02
**現状**: `DiagnosticRecorder`、`DiagnosticSnapshot`、`TraceRecorder`、`SessionLedger` が完全実装済み。式評価エンジン完備。MCP基盤完備。
**発想**: VSデバッガのようにアプリを止めてメモリを見るのではなく、**自然な境界点で状態をキャプチャし、MCP経由で観測する**方式。アプリを止めないので VPも動いたままデバッグできる。

---

## 既存のデバッグインフラ（すべて実装済み）

| コンポーネント | 機能 |
|---------------|------|
| `DiagnosticRecorder` | スレッドセーフなイベントレコーダー。シーケンス番号/トレースID/タイムスタンプ/フレームインデックス自動付与 |
| `DiagnosticScope` | RAIIスコープ計測。関数の入口-出口を自動記録。例外安全 |
| `DiagnosticSnapshot` | 全イベントのJSONシリアライズ。エラー/Warningカウント、最新障害情報 |
| `TraceRecorder` | スレッドセーフトレース。クラッシュレコード、ロック記録、フレームタイムライン |
| `SessionLedger` | リカバリーポイント付きセッション台帳 |
| `DebugIdentity` | `std::source_location` ベースのオブジェクト識別。全オブジェクトが作成場所を記憶 |
| `ExpressionEvaluator` | 任意の式を評価可能 |
| `CrashHandler` | ネイティブクラッシュのスタックトレース記録 |

---

## Phase 1: データブレークポイント（Data Breakpoints）

### 1.1 概念

VS のデータブレークポイントは「この変数が変化したら停止」。Artifactでは：
「このレイヤーのこのプロパティが閾値を超えたら DiagnosticEvent を発行」

```cpp
struct DataBreakpoint {
    QString targetPath;           // "layer.xxx.transform.position.x"
    float previousValue;
    float threshold;
    float changeTolerance;        // これ以上変化したら発火
    int frameInterval = 1;        // 何フレームおきにチェック
    std::uint64_t lastTriggeredSequence = 0;
    bool enabled = true;
};

class DataBreakpointEngine {
public:
    void add(const DataBreakpoint& bp);
    void remove(int id);

    /// 毎フレーム呼び出す。発火したら DiagnosticRecorder に記録
    void evaluateAll(int64_t currentFrame);
    
    /// MCP 経由で全アクティブブレークポイントの状態を返す
    QJsonObject snapshot() const;
};
```

### 1.2 登録例（MCP tools/call）

```json
{
  "method": "tools/call",
  "params": {
    "tool": {
      "class": "debug",
      "method": "addDataBreakpoint",
      "arguments": [
        {
          "target": "layer.3d_model.transform.position.z",
          "threshold": 10.0,
          "changeTolerance": 1.0,
          "frameInterval": 5
        }
      ]
    }
  }
}
```

→ `position.z` が 5フレームの間に 1.0 以上変動したら、`DiagnosticRecorder` に `"DataBreakpoint: layer.3d_model.transform.position.z changed by Δ2.3 (threshold: 1.0)"` というイベントが記録される。

---

## Phase 2: 条件付きウォッチポイント（Conditional Watchpoints）

### 2.1 概念

「特定の条件が満たされたときだけ状態をキャプチャ」

```cpp
struct Watchpoint {
    QString expression;           // 評価式: "layer('Cube').opacity < 0.5"
    QString captureTarget;        // キャプチャ対象: "layer('Cube')" → 全プロパティ
    QString snapshotName;
    bool oneShot = true;          // true = 1回発火で自動解除
    bool enabled = true;
};

class WatchpointEngine {
public:
    void add(const Watchpoint& wp);
    void evaluateAll(int64_t currentFrame);

    /// 発火時に DiagnosticSnapshot を取得し MCP 経由でクライアントに通知
    std::vector<DiagnosticSnapshot> firedSnapshots() const;
};
```

### 2.2 使用例

```json
{
  "tool": {
    "class": "debug",
    "method": "addWatchpoint",
    "arguments": [{
      "expression": "layer('Background').meshLoaded == false && frame > 10",
      "capture": ["layer('Background')", "composition.renderState"],
      "oneShot": false
    }]
  }
}
```

→ フレーム10以降で `meshLoaded` が false になるたびに、Background レイヤーとコンポジション状態の完全なスナップショットがキャプチャされる。

---

## Phase 3: フレームステップ実行

### 3.1 概念

アプリ全体を止めるのではなく、MCP 経由で「1フレーム進める→状態を返す→待つ→次の指示を待つ」のステップ実行。

```cpp
class MCPDebugStepper {
public:
    // MCP ツール
    void stepForward(int frames = 1);    // Nフレーム進める
    void stepToFrame(int64_t target);    // 特定フレームまで進める
    void pause();                         // デバッグポーズ（PlaybackService連動）
    void resume();                        // 再開
    
    // 各ステップ後に自動でこのスナップショットを返す設定
    void setAutoCapture(const QStringList& targets);
    
    bool isPaused() const;
    int64_t currentStepFrame() const;
};
```

### 3.2 AI エージェントからのシナリオ

```
1. debug.stepToFrame(35)       → "Frame 35 reached. 13 diagnostic events since last step."
2. debug.layerState("Cube")    → "meshLoaded: false, renderMode: wireframe, vertices: 0"
3. debug.evaluate("layer('Cube').sourcePath")  → ""
4. debug.setProperty("Cube", "model.sourcePath", "sphere.obj")
5. debug.stepForward(1)        → "Frame 36: meshLoaded=true, vertices=512, polygons=256"
6. debug.evaluate("layer('Cube').mesh.polygonCount") → 256
```

---

## Phase 4: メモリインスペクション（全レイヤー/オブジェクトの完全ダンプ）

### 4.1 メモリマップ

既存の `DebugIdentity` を全主要オブジェクトに適用し、オブジェクトグラフを構築:

```cpp
class MemoryInspector {
public:
    struct ObjectInfo {
        uint64_t debugId;
        QString typeName;
        QString debugName;
        QString creationFile;
        uint32_t creationLine;
        uint64_t ownerId;          // 所有オブジェクトの debugId
        size_t estimatedSize;      // 推定メモリサイズ
        int refCount;              // 参照カウント（SharedPtrなら）
    };

    /// 全登録オブジェクトを列挙
    std::vector<ObjectInfo> listAll() const;

    /// 特定オブジェクトの詳細ダンプ
    QString dumpObject(uint64_t debugId) const;

    /// オブジェクトグラフをJSONで返す
    QJsonObject objectGraph(uint64_t rootDebugId) const;
};
```

### 4.2 MCP ツール

```json
// debug.memory.list → 全ライブオブジェクトの一覧（型/名前/作成場所/サイズ）
// debug.memory.dump {id: 12345} → オブジェクトの全プロパティをダンプ
// debug.memory.graph {root: "composition-id"} → コンポジション→レイヤー→エフェクト の所有グラフ
// debug.memory.leaks → SharedPtrで参照が残っているが到達不能なオブジェクトを検出
```

---

## Phase 5: GPU バッファインスペクション

### 5.1 テクスチャ/バッファの内容を MCP 経由で読み取り

```cpp
class GPUDebugInspector {
public:
    /// GPUテクスチャの指定領域をCPUにダウンロードし、RGBA値を返す
    QJsonObject sampleTexture(const QString& textureName, int x, int y, int w, int h);

    /// GPUバッファの内容を取得
    QJsonObject readBuffer(const QString& bufferName, int offset, int count);

    /// GPUメモリプールの使用状況
    QJsonObject gpuMemoryStats();

    /// 現在バインドされている全リソース一覧
    QJsonObject boundResources();
};
```

---

## ファイル一覧

| フェーズ | ファイル | 新規/変更 |
|---------|----------|----------|
| P1 | `ArtifactCore/include/Diagnostics/DataBreakpoint.ixx` | 新規 |
| P1 | `ArtifactCore/src/Diagnostics/DataBreakpoint.cppm` | 新規 |
| P2 | `ArtifactCore/include/Diagnostics/Watchpoint.ixx` | 新規 |
| P2 | `ArtifactCore/src/Diagnostics/Watchpoint.cppm` | 新規 |
| P3 | `Artifact/include/Service/MCPDebugStepper.ixx` | 新規 |
| P3 | `Artifact/src/Service/MCPDebugStepper.cppm` | 新規 |
| P4 | `ArtifactCore/include/Diagnostics/MemoryInspector.ixx` | 新規 |
| P4 | `ArtifactCore/src/Diagnostics/MemoryInspector.cppm` | 新規 |
| P5 | `Artifact/include/Service/GPUDebugInspector.ixx` | 新規 |
| P5 | `Artifact/src/Service/GPUDebugInspector.cppm` | 新規 |
| — | `Artifact/src/Tool/AIToolDSL/` (ToolExecutor) | 変更（ツール登録） |

---

## 実装規模

| フェーズ | 工数 | 理由 |
|---------|------|------|
| P1 DataBreakpoint | **小** | 毎フレーム `getProperty->getValue()` をポーリングするだけ |
| P2 Watchpoint | **小** | `ExpressionEvaluator` で評価 → Threshold 判定 |
| P3 Stepper | **小** | `PlaybackService::goToFrame()` + `DiagnosticRecorder::since(seq)` |
| P4 MemoryInspector | **中** | DebugIdentity 登録 + 再帰的ダンプ。既存インフラあり |
| P5 GPUInspector | **中** | `readbackChannelToImage` をラップ + GPUメモリクエリ |

**合計**: 中。新規10ファイル。既存の DiagnosticRecorder/DebugIdentity/MCP/ExpressionEvaluator がすべて揃っているので、ほとんどが薄いラッパー。
