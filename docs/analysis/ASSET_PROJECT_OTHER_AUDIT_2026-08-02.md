# Asset / Project / Plugin / Other 監査

**日付**: 2026-08-02

---

## Asset 層 — 🟡 60%

| コンポーネント | スコア | 所見 |
|---------------|--------|------|
| AssetDatabase | 🟡 60% | データベースはあるが複雑 |
| AssetImporter | 🟡 60% | 各種フォーマット対応 |
| AssetConverter | 🟡 50% | 変換パイプライン |
| AssetSequence | 🟢 80% | 連番画像管理 |
| AssetMetaFile | 🟡 60% | メタデータファイル |
| ImageAsset | 🟢 80% | 画像アセット型 |
| VectorImport | 🟡 50% | ベクターインポート |

---

## Project 層 — 🟡 65%

| コンポーネント | スコア | 所見 |
|---------------|--------|------|
| ArtifactProjectManager | 🟡 65% | プロジェクト管理。メモリリークあり |
| ArtifactProject | 🟡 65% | プロジェクトデータモデル |
| ArtifactProjectService | 🟢 80% | プロジェクトサービス |
| ArtifactPresetManager | 🟢 75% | プリセット管理 |
| ArtifactProjectExporter | 🟡 60% | プロジェクト書き出し |
| ArtifactProjectImporter | 🟡 60% | プロジェクト読み込み |
| ArtifactAutoSaveManager | 🟢 80% | 自動保存 |
| ArtifactRevisionService | 🟡 50% | バージョン管理 |

---

## Plugin 層 — 🟡 45%

| コンポーネント | スコア | 所見 |
|---------------|--------|------|
| ILayerPlugin | 🟡 50% | レイヤープラグインインターフェース |
| PluginCommon | 🟡 50% | 共通プラグイン基盤 |
| PluginRegistry | 🟡 50% | プラグイン登録 |
| PluginSandbox | 🟡 40% | サンドボックス実行 |
| PluginLoader | 🟡 50% | 動的ロード |
| VST3Interfaces | 🟡 50% | VST3 ホストインターフェース |

---

## Script 層 — 🟡 60%

| コンポーネント | スコア | 所見 |
|---------------|--------|------|
| ExpressionEvaluator | 🟢 85% | 式評価エンジン |
| ExpressionParser | 🟢 80% | 式パーサー |
| BuiltinScriptVM | 🟢 80% | 組み込みスクリプトVM |
| PythonEngine | 🟡 60% | Python インターフェース |
| AngelScript | 🟡 50% | AngelScript バインディング |
| CSharpScriptEngine | 🟡 40% | C# スクリプト |

---

## AI 層 — 🟡 55%

| コンポーネント | スコア | 所見 |
|---------------|--------|------|
| LlamaLocalAgent | 🟢 80% | ローカルLLM推論 |
| OnnxDmlLocalAgent | 🟢 80% | ONNX + DirectML |
| McpBridge | 🟢 85% | MCPプロトコル。全アプリ中唯一 |
| TieredAIManager | 🟢 75% | 段階的AI管理 |
| CommandIR/CommandSandbox | 🟡 60% | コマンド実行 |
| ObjectDetector | 🟡 50% | 物体検出 |

---

## Physics / Simulation 層 — 🟡 45%

| コンポーネント | スコア | 所見 |
|---------------|--------|------|
| Physics2D | 🟡 60% | 2D物理エンジン |
| FluidSolver2D | 🟡 50% | 2D流体 |
| MpmSolver2D | 🟡 40% | 質点法2D |
| SandSim2D | 🟡 50% | 砂シミュレーション |
| PyroSimulation | 🟡 40% | 火炎シミュレーション |
| FractureEngine | 🟡 40% | 破壊シミュレーション |
| SoftBodySolver | 🟡 40% | 軟体ソルバー |
| BoidsSwarmSystem | 🟡 50% | 群衆シミュレーション |

---

## その他小モジュール

| モジュール | スコア | 所見 |
|-----------|--------|------|
| **NLE/OTIO** | 🟡 40% | OpenTimelineIO。インターフェースあり |
| **Shape システム** | 🟡 50% | ShapePath/AeOperators/TrimPaths/Repeater。ShapeLayer あり |
| **Channel** | 🟢 80% | チャンネル抽象。Codec/Generator/OS 型 |
| **Container** | 🟢 85% | MultiIndexContainer/NamedVector/SmallVector などのデータ構造 |
| **Data** | 🟢 80% | DataTable/ColumnType/TypedColumn/DataCache |
| **Diagnostics** | 🟢 80% | CrashHandler/Trace/ProjectDiagnostic/Logger |
| **Event** | 🟢 85% | EventBus |
| **Property** | 🟢 85% | AbstractProperty/Property/PropertyGroup/Serialization |
| **UI基盤** | 🟡 60% | InputOperator/DragOperator/GizmoMode/ShortcutBindings |
| **Command** | 🟢 80% | CommandModule/EditSession/LambdaCommand |
| **Platform** | 🟢 80% | ProcessHelper/ShellUtils/SystemUsage |
| **Thread** | 🟢 85% | BackgroundTask/WorkerPool/PreciseTicker |
| **Memory** | 🟢 80% | ArtifactAllocators |

---

## 全体サマリー（監査16カテゴリ完）

| # | カテゴリ | スコア | 主な強み | 主な弱み |
|---|---------|--------|----------|----------|
| 1 | Color Pipeline | 🟡 70% | 伝達関数16種/DaVinci同等 | OCIO実libなし/RRT近似 |
| 2 | Image/ImageProcessing | 🟡 75% | SurfaceColorDescriptor/140種エフェクト | EXR空/コード重複3系統 |
| 3 | Graphics/GPU | 🟡 70% | Diligent統合/MeshRenderer | RenderGraphトポロソートなし |
| 4 | Media/Video/IO | 🟡 65% | FFmpeg+HDR/15種Transition/ImageExporter | Stabilizer完全死亡 |
| 5 | Audio | 🟢 80% | LipSync/スペクトル/10種エフェクト | ASIO/VST3不完全 |
| 6 | Text/Font | 🟡 50% | GlyphAtlas/HarfBuzz/SDF | TextAnimator未実装 |
| 7 | Animation | 🟡 70% | 式エンジン/Rig2D/Easing | VP操作未配線 |
| 8 | Transform/Frame/Time | 🟢 85% | 完成度高い |
| 9 | Asset | 🟡 60% | 基本機能 |
| 10 | Project | 🟡 65% | プロジェクト管理+自動保存 | メモリリークあり |
| 11 | Plugin | 🟡 45% | インターフェースあり | SDK未完成 |
| 12 | Script | 🟡 60% | ExpressionEvaluator/Python | 一部不完全 |
| 13 | AI | 🟡 55% | LLM+ONNX+MCP | コマンド実行スタブ |
| 14 | Physics/Simulation | 🟡 45% | 多様なソルバー | すべてプロトタイプ段階 |
| 15 | Shape | 🟡 50% | 基本システム |
| 16 | Foundation (Container/Data/Event/Property/Thread/Platform) | 🟢 82% | 完成度高い |

**プロジェクト全体平均**: 🟡 ~65%
