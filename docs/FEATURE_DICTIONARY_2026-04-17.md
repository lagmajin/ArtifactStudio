# ArtifactStudio 機能辞典

## 📋 目次

1. [プロジェクト概要](#プロジェクト概要)
2. [📦 モジュール一覧](#モジュール一覧)
3. [🔧 主要コンポーネント詳細](#主要コンポーネント詳細)
4. [🎨 エフェクトシステム](#エフェクトシステム)
5. [🎬 レイヤーシステム](#レイヤーシステム)
6. [🎞️ タイムラインシステム](#タイムラインシステム)
7. [🖥️ UI/ウィジェット](#uiウィジェット)
8. [🔗 サードパーティ統合](#サードパーティ統合)
9. [📚 依存関係図](#依存関係図)

---

## プロジェクト概要

**ArtifactStudio** はクロスプラットフォーム対応のプロフェッショナル動画編集・VFX制作アプリケーションです。After Effects の機能を拡張し、3Dグラフィックス統合やAIアシスタント機能を備えるモダンなコンポジターとして設計されています。

**開発アーキテクチャ**：
- **モジュール構成**：C++20 モジュールシステム（`.ixx`）を採用
- **UIフレームワーク**：Qt6 / QWidgetベース
- **レンダリング**：DiligentEngine経由でDirect3D12/Vulkan/Metal/OpenGL対応
- **スクリプト**：Pythonエディタ統合、C#スクリプトサポート（将来的）
- **AI統合**：llama.cppを活用したローカルAI機能

**サブモジュール**（git submodule構造）：
- `ArtifactCore/` - コア機能（形状、レンダリング、メディア、カラー、テキスト等）
- `Artifact/` - アプリケーション層（プロジェクト管理、エフェクト、レイヤー、UI）
- `ArtifactWidgets/` - 共通ウィジェットライブラリ
- `libs/DiligentEngine/` - クロスプラットフォームグラフィックスエンジン
- `libs/llama.cpp/` - LLM推論エンジン

---

## モジュール一覧

### ArtifactCore

**概要**：アプリケーションの基盤となる非GUIモジュール群。数学、幾何学、画像処理、メディアコーデックなど、すべてのレイヤーで共有される低レベル・ミドルレベル機能を提供。

| モジュール名 | パス（相対） | 主要クラス | 説明 |
|---|---|---|---|
| **AI** | `include/AI/` | `LlamaLocalAgent`, `LocalAIAgent`, `McpBridge`, `ToolBridge`, `APIKeyManager` | ローカルAIエージェント、MCPブリッジ、ツール実行 |
| **Analyze** | `include/Analyze/` | `Histgram`, `SmartPalette` | 画像分析、ヒストグラム、スマートパレット |
| **Animation** | `include/Animation/` | `Animatable`, `Dynamics`, `Transform2D`, `Value` | アニメーションシステム、キーフレーム、イーズィング |
| **Asset** | `include/Asset/` | `AssetID`, `AssetDatabase`, `AssetImporter`, `AssetManager` | アセット管理、DB、インポート |
| **Audio** | `include/Audio/` | `AudioMixer`, `AudioEffect`, `AudioAnalyzer`, `AudioVolume`, `SoundTrack` | オーディオ処理、エフェクト、ミキサー |
| **Channel** | `include/Channel/` | - | オーディオ/ビデオチャンネル抽象化 |
| **Color** | `include/Color/` | `FloatColor`, `ColorManager`, `ColorSwatch`, `ColorLUT`, `ACESColorManager`, `ColorWheelsProcessor` | カラーマネジメント、LUT、カラースペース変換 |
| **Composition** | `include/Composition/` | `Composition`, `PreComposeManager`, `PreComposeCommand` | コンポジション管理、ネスト |
| **Container** | `include/Container/` | `MultiIndex` | マルチインデックスコンテナ |
| **Core** | `include/Core/` | `EditSessionManager`, `DiagnosticEngine`, `ProjectDiagnostic`, `Mask.RotoMask`, `UI.RotoMaskEditor`, `TestHelper` | コアユーティリティ、診断、マスク編集 |
| **Event** | `include/Event/` | `EventBus` | イベントバス、パブリッシュ/サブスクライブ |
| **File** | `include/File/` | `FileTypeDetector` | ファイルタイプ検出 |
| **Font** | `include/Font/` | `FontDescriptor`, `FreeFont` | フォント管理 |
| **Frame** | `include/Frame/` | - | フレーム管理 |
| **Graphics** | `include/Graphics/` | `ParticleCompute`, `ParticleRenderer`, `IRayTracingManager`, `Texture`, `MotionBlurProcessor` | グラフィックス計算、パーティクル、レイトレーシング |
| **Image** | `include/Image/` | `ImageF32x4RGBAWithCache`, `PsdDocument`, `ImageYUV420`, `Bitmap` | 画像データ構造、PSD対応 |
| **ImageProcessing** | `include/ImageProcessing/` | `FluidVisualizer`, `SharpenDirectionalBlur`, `Halation`, `VolumetricShine`, OpenCV統合 | 画像処理エフェクト |
| **Knob** | `include/Knob/` | `Knob`, `KnobList` | プロパティ制御ノブ |
| **Layer** | `include/Layer/` | `Layer`, `LayerStrip` | レイヤー抽象化 |
| **Math** | `include/Math/` | `Bezier`, `BezierSampler`, `Interpolate`, `Quaternion`, `Rotation`, `Noise` | 数学関数、補完、ノイズ生成 |
| **Media** | `include/Media/` | `ISource`, `MediaFrame`, `MediaProbe`, `TimeStamp` | メディアソース抽象化 |
| **Platform** | `include/Platform/` | `ShellUtils`, `Windows.Process` | プラットフォーム固有ユーティリティ |
| **Playback** | `include/Playback/` | `PlaybackManager` | 再生制御 |
| **Preview** | `include/Preview/` | - | プレビュー管理 |
| **Property** | `include/Property/` | `Property`, `Group`, `LinkManager` | プロパティシステム |
| **Script** | `include/Script/` | `ScriptRuntime`, `Script`, `ScriptExpression.*` | スクリプト実行環境 |
| **Shape** | `include/Shape/` | `ShapePath`, `ShapeLayer`, `ShapeGroup`, `ShapeOperator`, `TrimPaths`, `ShapeTypes` | ベクターシェイプ、パス操作 |
| **Source** | `include/Source/` | `ISource` | ソース抽象化 |
| **Text** | `include/Text/` | `TextStyle`, `GlyphAtlas`, `GlyphLayout`, `TextAnimator` | テキストレンダリング、グリフ管理 |
| **Time** | `include/Time/` | `Duration`, `RationalTime`, `RealTime`, `TimeCode`, `TimeRange`, `TimeRemap` | 時間表現、タイムコード |
| **Track** | `include/Track/` | `LayerTrack` | トラック管理 |
| **Transform** | `include/Transform/` | `StaticTransform3D`, `StaticTransform2D`, `Rotate`, `Rotate2D`, `Scale2D`, `ZoomScale`, `AnchorPoint2D`, `Camera`, `Light` | 座標変換、ビューポート |
| **UI** | `include/UI/` | `InputOperator`, `InputOperatorManager`, `SelectionManager`, `KeyMap`, `InteractiveActions`, `RotoMaskEditor`, `DragOperator`, `TransformControls`, `GizmoMode` | UI操作、入力処理 |
| **Utils** | `include/Utils/` | `Path`, `String`, `UniString`, `HashValue`, `Id`, `AssetManager`, `JsonLike`, `ScopedTimer`, `PerformanceProfiler`, `Tag`, `MultipleTag` | ユーティリティ、文字列、パス、プロファイラ |
| **Video** | `include/Video/` | `Video`, `FFmpegVideoDecoder`, `FFMpegEncoder`, `GStreamerDecoder`, `GStreamerEncoder`, `PlaybackManager`, `EncoderSetting`, `Stabilizer` | 動画デコード/エンコード、安定化 |
| **VST** | `include/VST/` | `VSTHost`, `VSTEffect` | VSTプラグインホスト |
| **ShaderNode** | `include/ShaderNode/` | `ArtifactShaderNode` | ノードベースシェーダー編集 |

---

### Artifact

**概要**：アプリケーション層のモジュール群。プロジェクト管理、コンポジション編集、エフェクト適用、レンダリング、UIウィジェットを統合。

| モジュール名 | パス（相対） | 主要クラス | 説明 |
|---|---|---|---|
| **AI** | `include/AI/` | `AIClient`, `FileAutomation`, `MaterialAutomation`, `RenderAutomation`, `WorkspaceAutomation` | AI自動化クライアント |
| **Application** | `include/Application/` | - | アプリケーション基盤 |
| **Asset** | `include/Asset/` | `ArtifactAssetResult` | アセット操作結果 |
| **Audio** | `include/Audio/` | `AudioClockProvider` | オーディオクロック |
| **AVSync** | `include/AVSync/` | `AVSyncBridge` | A/V同期ブリッジ |
| **Color** | `include/Color/` | `ColorSettings`, `ColorManager`, `ColorNodeGraph`, `LUTData`, `ColorWheelsProcessor`, `ColorGrader`, `ColorPaletteManager` | カラーグレーディングシステム |
| **Composition** | `include/Composition/` | `ArtifactComposition2D`, `ArtifactComposition3D`, `CompositionSettings`, `Abstract`, `InitParams`, `PlaybackController`, `FindQuery`, `InOutPoints` | コンポジション管理、再生制御 |
| **Controller** | `include/Controller/` | `TimelineViewProvider` | タイムラインビュー提供 |
| **Core** | `include/Core/` | `ArtifactMainWindow`, `WindowManager` | メインウィンドウ管理 |
| **Diagnostics** | `include/Diagnostics/` | `ArtifactDebugConsoleWidget`, `ArtifactMissingFileRule`, `ArtifactPerformanceRule` | デバッグコンソール、診断ルール |
| **Effetcs** | `include/Effetcs/` | 多数のエフェクト実装（下記参照） | エフェクト実装（実体はEffetcsサブフォルダ） |
| **Effect** | `include/Effect/` | `ArtifactAbstractEffect`, `ArtifactEffectImplBase`, `EffectContext` | エフェクト基底クラス |
| **Export** | `include/Export/` | `Python.ArtifactPythonAPI` | Python APIエクスポート |
| **Generator** | `include/Generator/` | `AbstractGenerator`, `Manager`, `Particle` | コンテンツ生成 |
| **GUI** | `include/GUI/` | `ArtifactGuiManager`, `ArtifactToolOptionsBar` | GUI管理 |
| **Layer** | `include/Layer/` | `ArtifactAbstractLayer`, `ArtifactLayerSettings`, `ArtifactVideoLayer`, `ArtifactAudioLayer`, `ArtifactShapeLayer`, `ArtifactTextLayer`, `ArtifactImageLayer`, `ArtifactGroupLayer`, `ArtifactSolidImageLayer`, `ArtifactSvgLayer`, `ArtifactNullLayer`, `ArtifactCameraLayer`, `ArtifactLightLayer`, `ArtifactParticleLayer`, `ArtifactSDFLayer`, `ArtifactCloneLayer`, `ArtifactCompositionLayer` | レイヤー実装、レイヤー設定 |
| **Library** | `include/Library/` | `Link` | ライブラリリンク |
| **Localization** | `include/Localization/` | `TranslationManager` | ローカライゼーション |
| **LOD** | `include/LOD/` | `LODManager` | Level of Detail管理 |
| **Mask** | `include/Mask/` | `LayerMask`, `MaskPath` | マスクシステム |
| **Playback** | `include/Playback/` | `ArtifactPlaybackEngine` | 再生エンジン |
| **Preview** | `include/Preview/` | `ArtifactPreviewController`, `ArtifactLayerPreviewPipeline` | プレビュー制御 |
| **Project** | `include/Project/` | `ArtifactProjectManager`, `ProjectItems`, `ProjectModel`, `ProjectSettings`, `ProjectHealth`, `ProjectExporter`, `ProjectImporter`, `ProjectAutoSaveManager`, `ProjectPresetManager` | プロジェクト管理、インポート/エクスポート |
| **Render** | `include/Render/` | `ArtifactRenderManager`, `ArtifactRenderController`, `ArtifactIRenderer`, `ArtifactRenderer`, `ArtifactFrameCache`, `ArtifactPerformanceMonitor`, `ArtifactRenderContext`, `ArtifactRenderROI`, `ArtifactRenderScheduler`, `ArtifactGPUTextureCacheManager`, `PrimitiveRenderer2D`, `PrimitiveRenderer3D`, `ArtifactOffscreenRenderer2D` | レンダリングシステム |
| **Script** | `include/Script/` | `ScriptHooks` | スクリプトフック |
| **Service** | `include/Service/` | `ArtifactProjectService`, `ArtifactEffectService`, `ArtifactAudioService`, `ArtifactPlaybackService`, `ArtifactClipboardService`, `ArtifactApplicationService`, `ActiveContext`, `Proxy.Service` | サービス層 |
| **Tool** | `include/Tool/` | `ArtifactToolManager`, `ToolMode`, `Tool.Service`, `Tool.AIDSL` | ツール管理 |
| **VST** | `include/VST/` | `ArtifactVSTHost`, `ArtifactVSTEffect` | VSTホスト統合 |
| **Widgets** | `include/Widgets/` | 多数のウィジェット（下記参照） | UIコンポーネント群 |

---

### ArtifactWidgets

**概要**：再利用可能なQtウィジェット群。タイムライン、レンダーキュー、カラーピッカー、3Dビューアなどを提供。

| モジュール名 | パス（相対） | 主要クラス | 説明 |
|---|---|---|---|
| **Audio** | `include/Audio/` | `AudioBusWidget`, `AudioPreviewWidget`, `VolumeSlider`, `SpectrumAnalyzerWidget` | オーディオ関連ウィジェット |
| **Button** | `include/Button/` | `FloatColorPickerButton`, `ColorPickerButton`, `SubToolButton` | カスタムボタン |
| **Code** | `include/Code/` | `CodeEditor`, `SyntaxHighlighter` | コードエディタ |
| **Color** | `include/Color/` | `ColorWheel`, `ColorViewLabel`, `HistgramWidget`, `ParadeScopeWidget`, `WaveformScopeWidget`, `VectorScopeWidget` | カラーツール |
| **Common** | `include/Common/` | `AbstractWidget`, `CommonImports`, `CommandAction`, `CollapsibleSection`, `DragSpinBox`, `EditableLabel`, `HeadPanel`, `ToggleGlyphButton`, `ClickableLabel`, `AlignmentCombobox` | 基本ウィジェット |
| **Console** | `include/Console/` | `ConsoleWidget` | コンソール出力 |
| **Dialog** | `include/Dialog/` | `Dialog`, `FloatColorPicker`, `FloatColorPickerDialog`, `ColorSwatchDialog`, `PrecomposeDialog`, `CreatePlaneLayerDialog`, `CreateCameraLayerDialog`, `ApplicationSettingDialog`, `ArtifactRenderOutputSettingDialog`, `ArtifactCreateCompositionDialog`, `EditCompositionSettingDialog`, `KeyboardOverlayDialog` | ダイアログ |
| **Dock** | `include/Dock/` | `Pane`, `HeadPanel`, `DockGlowStyle`, `DockStyleManager` | ドックシステム |
| **Effect** | `include/Effect/` | `WidgetGlowFrame` | エフェクトUI |
| **Graphics** | `include/Graphics/` | `GraphicsItemFloatImageItem`, `NodeWireGraphicItem` | グラフィックスアイテム |
| **Image** | `include/Image/` | `BasicImageViewWidget` | 画像表示 |
| **Interface** | `include/Interface/` | `IViewer` | ビューアインターフェース |
| **Knob** | `include/Knob/` | `KnobSlider`, `RotaryKnob`, `KnobCheckBox`, `KnobEditorWidget`, `KnobColorPicker`, `AbstractKnobEditor` | パラメータ制御ノブ |
| **Panel** | `include/Panel/` | `DraggableSplitter` | パネル分割 |
| **Preview** | `include/Preview/` | `PreviewControlWidget` | プレビュー制御 |
| **Render** | `include/Render/` | `RenderQueueManagerWidget`, `BackendSettingWidget`, `GridRenderer` | レンダリングUI |
| **Search** | `include/Search/` | `SearchWidget` | 検索 |
| **Style** | `include/Style/` | `AccentDelegate` | スタイル装飾 |
| **Time** | `include/Time/` | `TimeCodeLabel`, `TimeCodeEditor` | タイムコード表示・編集 |
| **Video** | `include/Video/` | `ArtifactBasicVideoPreviewWidget` | 動画プレビュー |
| **Viewer** | `include/Viewer/` | `ContentViewer`, `ModelViewer` | コンテンツビューア |
| **Web** | `include/Web/` | `WebBrowser` | Webブラウザ統合 |

**Widgetsモジュール主要クラス**（トップレベル）：
- `ArtifactTimelineWidget` - タイムライン全体
- `ArtifactLayerPanelWidget` - レイヤーパネル
- `ArtifactCompositionEditor` - コンポジションエディタ
- `ArtifactCompositionRenderController` - レンダリング制御
- `ArtifactRenderManagerWidget` - レンダーキュー管理
- `ArtifactPropertyWidget` - プロパティインスペクタ
- `ArtifactProjectManagerWidget` - プロジェクトマネージャ
- `ArtifactAssetBrowser` - アセットブラウザ
- `ArtifactStatusBar` - ステータスバー
- `ArtifactSecondaryPreviewWindow` - セカンダリプレビュー
- `ArtifactSnapshotCompareWidget` - スナップショット比較
- `ArtifactColorSciencePanel` - カラーサイエンスパネル
- `ArtifactAudioMixerWidget` - オーディオミキサー
- `ArtifactQuickToolBox` - クイックツールボックス
- `ArtifactUndoHistoryWidget` - アンドゥ履歴
- `ReactiveEventEditorWindow` - リアクティブイベントエディタ
- `ArtifactProblemViewWidget` - 問題表示パネル

---

## 主要コンポーネント詳細

### 1. レンダリングシステム

#### ArtifactIRenderer (`Artifact.Render.IRenderer`)
**役割**：DiligentEngine をラップするレンダリングエンジン実体。D3D12/Vulkan/Metal/OpenGLを抽象化。

```
file: Artifact/include/Render/ArtifactIRenderer.ixx
主要メソッド:
- initialize(QWidget* widget)
- createSwapChain(QWidget* widget)
- readbackToImage() -> QImage
- renderOneFrame()
- setClearColor(FloatColor)
```

**内部構造**：
- PImpl イディオムにより、D3D12固有ロジックを隠蔽
- Diligent::RefCntAutoPtr<IRenderDevice> と IDeviceContext を保持
- ソフトウェアレンダリング診断ウィジェット（ArtifactSoftwareRenderTestWidget）と連携

---

#### ArtifactRenderController (`Artifact.Render.Controller`)
**役割**：レンダリング制御の高level API。CompositionRenderController から呼び出され、IRenderer を操作。

---

#### ArtifactCompositionRenderController (`Artifact.Widgets.CompositionRenderController`)
**役割**：コンポジションエディタビューの中心コントローラ。マウス操作、ズーム、パン、Gizmo操作、LOD管理を統合。

```cpp
主要メンバ:
- Impl* impl_ (pImpl)
- LODManager* lodManager()
- TransformGizmo* gizmo()
- Artifact3DGizmo* gizmo3D()
主要機能:
- コンポジション表示・操作
- レイヤー選択・ハイライト
- レンダリングキュー連携
- HUDオーバレイ表示
- カメラ制御
```

---

### 2. コンポジションシステム

#### ArtifactComposition (`Artifact.Composition._2D` / `Composition3D`)
**役割**：2D/3Dコンポジションのowlホルダー。レイヤー階層、タイムライン、出力設定を管理。

```cpp
class ArtifactComposition {
private:
    class Impl;
    Impl* impl_;
};
- コンポジションID、名前、解像度、フレームレート
- レイヤー階層管理
- タイムライン・In/Outポイント
- プロジェクト内の一意性保証
```

---

#### CompositionSettings
**パラメータ**：
- 解像度（width, height）
- FPS（RationalTime）
- ドロップフレーム対応
- ピクセルアスペクト比
- カラースペース

---

### 3. レイヤーシステム

#### ArtifactAbstractLayer（基底クラス）
**実装クラス一覧**：
- `ArtifactVideoLayer` - 動画レイヤー
- `ArtifactAudioLayer` - オーディオレイヤー
- `ArtifactImageLayer` - 画像レイヤー
- `ArtifactShapeLayer` - シェイプレイヤー（ベジェパス）
- `ArtifactTextLayer` - テキストレイヤー
- `ArtifactSolidImageLayer` - ソリッドカラーレイヤー
- `ArtifactSvgLayer` - SVGレイヤー
- `ArtifactGroupLayer` - グループレイヤー
- `ArtifactNullLayer` - ヌルレイヤー（親子付け用）
- `ArtifactCameraLayer` - カメラレイヤー
- `ArtifactLightLayer` - ライトレイヤー
- `ArtifactParticleLayer` - パーティクルレイヤー
- `ArtifactSDFLayer` - SDF（ Signed Distance Field）レイヤー
- `ArtifactCloneLayer` - クローンレイヤー
- `ArtifactCompositionLayer` - コンポジションレイヤー（ネスト）

**共通プロパティ**：
- `visible`, `locked`, `solo`
- トラックマット（マスク）
- ブレンドモード
- 不透明度
- 位置・回転・スケール（トランジフォーム）
- エフェクトスタック

---

#### ArtifactLayerSetting
**レイヤー基本設定**：
- ID、名前
- 表示/非表示
- ロック状態
- Solo状態

---

#### ArtifactLayerEditorWidgetV2 (`Artifact.Widgets.RenderLayerWidgetv2`)
**役割**：レイヤー編集用ビューア。ソフトウェアレンダリング・Diligentレンダラーの切替え可能。

---

### 4. マスクシステム

#### MaskPath (`Artifact.Mask.Path`)
**構造**：
```cpp
struct MaskVertex {
    QPointF position;     // アンカーポイント
    QPointF inTangent;    // 入力タンジェント（相対）
    QPointF outTangent;   // 出力タンジェント（相対）
};
class MaskPath {
- 複数のMaskVertexを保持
- 閉じたパスかどうか
- マスクモード（Add, Subtract, Intersect, Difference）
```

---

#### LayerMask (`Artifact.Mask.LayerMask`)
**機能**：
- 複数のMaskPathを統合
- OpenCVベースのラスタライゼーション
- `compositeAlphaMask()` - アルファマスク生成
- `applyToImage()` - 画像に適用

---

#### RotoMaskEditor (`Core.UI.RotoMaskEditor`)
**編集モード**：
- `Select` - 選択・移動
- `Draw` - 描画（頂点追加）
- `Edit` - 制御点編集
- `Delete` - 頂点削除

---

### 5. シェイプシステム

#### ShapePath (`Shape.Path`)
**概要**：ベジェパスデータ構造。アニメーション対応。

---

#### ShapeLayer (`Shape.Layer`)
**機能**：After Effectsのシェイプレイヤーに相当。複数のShapePath、塗り、ストローク、変形を保持。

---

#### ShapeGroup (`Shape.Group`)
**機能**：シェイプのグループ化。

---

### 6. エフェクトシステム

#### アーキテクチャ概要

エフェクトは **5段階パイプライン** に従って適用されます：

```
Generator → Geometry Transform → Material Render → Rasterizer → LayerTransform
```

参照：`docs/EFFECT_SYSTEM_SPECIFICATION.md`

---

#### 基底クラス

- `ArtifactAbstractEffect` (`Artifact.Effect.Abstract`)
- `ArtifactEffectImplBase` (`Artifact.Effect.ImplBase`)
- `ArtifactCreativeEffects` (`Artifact.Effect.Creative`) - クリエイティブエフェクト集
- `EffectContext` - エフェクト適用コンテキスト

---

#### エフェクト一覧

**カテゴリ：Blur（ブラー）**
| エフェクト | モジュール | 主な機能 |
|---|---|---|
| `BlurEffect` | `Artifact.Effect.Rasterizer.Blur` | ガウシアン、エッジ保持 |
| `GauusianBlur` | `Artifact.Effect.GauusianBlur` | ガウスブラー |
| `DirectionBlur` | `Artifact.Effect.Blur.DirectionBlur` | 方向ブラー |
| `RadialBlur` | `Artifact.Effect.RadialBlur` | 放射状ブラー |
| `MotionBlurEffect` | `Artifact.Effect.MotionBlur` | モーションブラー、モーションベクトル |

**カテゴリ：Color Correction（カラー補正）**
| エフェクト | モジュール | 主な機能 |
|---|---|---|
| `HueAndSaturation` | `Artifact.Effect.ColorCorrection` | 色相・彩度調整 |
| `ExposureEffect` | `Artifact.Effect.ColorCorrection` | 露出調整 |
| `BrightnessEffect` | `Artifact.Effect.ColorCorrection` | 明るさ調整 |
| `LiftGammaGainEffect` | `Artifact.Effect.LiftGammaGain` | レベル補正（LGG） |
| `WhiteBalanceEffect` | `Artifact.Effect.WhiteBalance` | ホワイトバランス |
| `ColorLUTEffect` | `Artifact.Color.LUT` | LUT適用 |

**カテゴリ：Glow（グロー）**
| エフェクト | モジュール | 主な機能 |
|---|---|---|
| `GlowEffect` | `Artifact.Effect.Glow` | マルチレイヤーグロー |

**カテゴリ：Stabilization（安定化）**
| エフェクト | モジュール | 主な機能 |
|---|---|---|
| `StabilizerEffect` | `Artifact.Effect.Stabilizer` | 手ブレ補正 |
| `LiveStabilizer` | `Artifact.Effect.Stabilizer` | ライブ安定化 |
| `BatchStabilizer` | `Artifact.Effect.Stabilizer` | バッチ安定化 |

**カテゴリ：Generative（生成）**
| エフェクト | モジュール | 主な機能 |
|---|---|---|
| `NoiseEffect` | `Artifact.Effect.Noise` | ノイズ生成 |
| `WaveEffect` | `Artifact.Effect.Wave` | 波形歪み |
| `SpherizeEffect` | `Artifact.Effect.Spherize` | 球面化 |

**カテゴリ：Keying（キーイング）**
| エフェクト | モジュール | 主な機能 |
|---|---|---|
| `ChromaKeyEffect` | `Artifact.Effect.Keying` | クロマキー |

**カテゴリ：Rasterizer（ラスタライザ）**
| エフェクト | モジュール | 主な機能 |
|---|---|---|
| `DropShadowEffect` | `Artifact.Effect.Rasterizer` | ドロップシャドウ |
| `PBRMaterialEffect` | `Artifact.Effect.Render` | PBRマテリアル |

**カテゴリ：Distortion（歪み）**
| エフェクト | モジュール | 主な機能 |
|---|---|---|
| `TwistTransform` | `Artifact.Effect.Transform` | ねじれ変形 |
| `BendTransform` | `Artifact.Effect.Transform` | 曲げ変形 |

**カテゴリ：Transform（変換）**
| エフェクト | モジュール | 主な機能 |
|---|---|---|
| `LayerTransform2D` | `Artifact.Effect.LayerTransform` | 2Dレイヤー変換 |

**カテゴリ：Field（フィールド）**
| エフェクト | モジュール | 主な機能 |
|---|---|---|
| `LinearField` | `Artifact.Effect.Field` | 線形力場 |
| `RadialField` | `Artifact.Effect.Field` | 放射状力場 |
| `BoxField` | `Artifact.Effect.Field` | ボックス力場 |
| `SphericalField` | `Artifact.Effect.Field` | 球面力場 |

**エフェクト管理**：
- `ArtifactGlobalEffectManager` (`Artifact.Effects.Manager`)
- `EffectFactory`, `Creator` 関数オブジェクト

---

### 7. タイムラインシステム

#### ArtifactTimelineWidget (`Artifact.Widgets.Timeline`)
**構成要素**：
- 左ペイン：`ArtifactLayerPanelWidget`
- 右ペイン：`TimelineTrackView`（トラック表示本体）
- 右上：`ArtifactTimelineNavigatorWidget`（タイムナビゲーター）
- タイムナビゲーター下：`ArtifactTimelineScrubBar`（RAMキャッシュバー）
- ワークエリア：`ArtifactWorkAreaControlWidget`

**データモデル**：
- `ArtifactTimelineIconModel` - アイコンモデル
- `ArtifactTimelineKeyframeModel` - キーフレームモデル
- `TimelineScaleWidget` - タイムスケール

**マウス/キーボード操作**：
- ドラッグ&ドロップでレイヤー移動・トリム
- ホイールでズーム、スペースキーで再生
- 右クリックでコンテキストメニュー

---

### 8. ビューア・プレビュー

#### ArtifactContentsViewer (`Artifact.Widgets.Viewer`)
**役割**：画像/動画/3Dモデルなどのコンテンツ閲覧。比較モード、履歴表示、インスペクション機能を統合。

---

#### ArtifactCompositionEditor (`Artifact.Widgets.Render`)
**役割**：コンポジションの本編編集ビューア。再生、ズーム、フィット、Gizmo操作。

---

#### ArtifactSecondaryPreviewWindow (`Artifact.Widgets`)
**役割**：独立したセカンダリプレビューウィンドウ。

---

### 9. プロジェクト管理

#### ArtifactProjectManager (`Artifact.Project.Manager`)
**機能**：
- プロジェクト作成・読込・保存
- 自動保存管理（AutoSaveManager）
- プロジェクトヘルス診断（ProjectHealth）
- インポート/エクスポート
- プリセット管理

```cpp
主なメソッド:
- createProject(name, force)
- loadFromFile(path)
- saveToFile(path)
- saveIncremental(path)
```

---

### 10. シェーダーノードシステム

#### ArtifactShaderNode (`Artifact.ShaderNode.Core`)
**構造**：
- `ShaderNodeBase` - ノード基底
- `Pin` - 入力/出力ピン
- `Link` - ノード間リンク
- データ型：Float, Vector2, Vector3, Vector4, Texture2D

**用途**：カラーグレーディング、エフェクトのノードベース編集。

---

### 11. スクリプトシステム

#### ScriptRuntime (`Script.Runtime`)
**エンジン**：
- Python統合（pybind11経由）
- C#エンジン（将来実装）
- 式評価エンジン（Script.Expression）

```cpp
struct ScriptHostSnapshot {
    appName, appVersion, projectName
    activeCompositionName, workingDirectory
    selection, hasProject, hasComposition
};
class ScriptRuntime {
    LogCallback ログコールバック
    evaluate(script) -> ScriptExecutionResult
}
```

**Python API**：`Artifact.PythonAPI`経由で以下を公開：
- プロジェクトAPI
- レイヤーAPI
- エフェクトAPI
- レンダーAPI
- ユーティリティAPI

---

### 12. VST統合

#### ArtifactVSTHost (`Artifact.VST.Host`)
**機能**：VST2/VST3プラグインをホストし、オーディオエフェクトとして適用。

---

### 13. ツールシステム

#### ArtifactToolManager (`Artifact.Tool.Manager`)
```cpp
enum class ToolType {
    Selection, Hand, Zoom, Rotation,
    AnchorPoint, Pen, Text, Shape,
    Rectangle, Ellipse, Move, Scale,
    Ripple, Rolling, Slip, Slide
};
```

---

### 14. カラーサイエンス

#### ArtifactColorScienceManager (`Artifact.Color.ScienceManager`)
**機能**：
- ACESカラーマネジメント
- LUT適用・管理
- カラーホイール、カーブ
- ノードベースカラーグレーディング

#### ColorNodeGraph (`Artifact.Color.NodeGraph`)
**ノード種類**：
- `ColorInputNode` - 入力
- `ColorOutputNode` - 出力
- `LiftGammaGainNode` - LGG
- `ContrastNode` - コントラスト
- `HueSaturationNode` - 色相・彩度
- `CurvesNode` - カーブ
- `ExposureNode` - 露出
- `InvertNode` - 反転
- `ClampNode` - クランプ
- `QualifierNode` - クオリファイア
- `BlurNode` - ブラー
- `ColorSpaceNode` - カラースペース変換
- `MergeNode` - マージ

---

## エフェクトシステム

### エフェクトパイプライン（5段階）

1. **Generator** - コンテンツ生成
   - 例：テキスト、シェイプ、パーティクル、フラクタルノイズ

2. **Geometry Transform** - ジオメトリ変換
   - 例：ワープ、ディストーション、メッシュ変形、ツイスト、ベンド

3. **Material Render** - マテリアル適用
   - 例：PBRマテリアル、テクスチャマッピング、ライティング

4. **Rasterizer** - 最終画像エフェクト
   - 例：ブラー、グロー、ドロップシャドウ、シャープ、ノイズ、グレイン

5. **LayerTransform** - レイヤー変換
   - 例：位置、回転、スケール、不透明度、ブレンドモード

**実行順序**：Generator → Geometry → Material → Rasterizer → LayerTransform

**ComputeMode**：CPU / GPU / AUTO 選択可能

---

## レイヤーシステム

### レイヤー階層モデル

```
ArtifactAbstractLayer (QObject派生)
├── ArtifactVideoLayer        (動画ファイル、イメージシーケンス)
├── ArtifactAudioLayer        (オーディオクリップ)
├── ArtifactImageLayer        (静止画)
├── ArtifactShapeLayer        (ベクトルシェイプ)
├── ArtifactTextLayer         (テキスト)
├── ArtifactSolidImageLayer   (ソリッドカラー)
├── ArtifactSvgLayer          (SVGベクター)
├── ArtifactGroupLayer        (グループ)
├── ArtifactNullLayer         (ヌルオブジェクト)
├── ArtifactCameraLayer       (3Dカメラ)
├── ArtifactLightLayer        (ライト)
├── ArtifactParticleLayer     (パーティクル)
├── ArtifactSDFLayer          (SDFテキスト/シェイプ)
├── ArtifactCloneLayer        (クローン)
├── ArtifactCompositionLayer  (ネストしたコンポジション)
```

---

### レイヤー設定：ArtifactLayerSetting
- `visible()` / `setVisible()`
- `locked()` / `setLocked()`
- `solo()` / `setSolo()`
- `id()`, `setId()`

---

## タイムラインシステム

### 主要ウィジェット構成

```
ArtifactTimelineWidget
├── ArtifactLayerPanelWidget         (左ペイン)
│   ├── レイヤー名表示
│   ├── 表示/非表示トグル
│   ├── ロック状態
│   ├── Soloトグル
│   └── ブレンドモード
├── ArtifactTimelineNavigatorWidget   (右上タイムナビゲーター)
│   └── 表示範囲調整
├── ArtifactTimelineScrubBar          (RAMキャッシュバー)
│   └── 緑色：キャッシュ済み
├── ArtifactWorkAreaControlWidget     (イン/アウトバー)
└── TimelineTrackView                 (右ペイン本体)
    ├── トラック行
    ├── キーフレームアイテム
    ├── 赤いシークバー（playhead）
    └── 背景グリッド
```

**用語对应**：
- `playhead` → UI: 赤い縦棒（シークバー）
- `inPoint/outPoint` → レイヤー有効範囲、ワークエリアバー編集対象
- `TimelineTrackView` → タイムライン本体（レイヤー編集領域）

---

## UI/ウィジェット

### 共通ウィジェット基盤
- `AbstractWidget` (`ArtifactWidgets.Common.AbstractWidget`) - 全ウィジェットの基底
- `CommonStyle` - スタイル管理

### カスタムコントロール
- `KnobSlider`, `RotaryKnob`, `KnobCheckBox`, `ColorPickerButton` - パラメータ制御
- `TimeCodeLabel`, `TimeCodeEditor` - タイムコード表示
- `ColorWheel`, `HistgramWidget`, `WaveformScopeWidget`, `VectorScopeWidget` - カラーツール
- `DraggableSplitter` - パネル分割

### ドックシステム
- `Pane` - ドックペイン
- `DockStyleManager` - スタイル・glow管理

---

## サードパーティ統合

### DiligentEngine (`libs/DiligentEngine/`)

**役割**：クロスプラットフォーム低レベルグラフィックス抽象化レイヤー。

**サポートGPU API**：
| API | プラットフォーム |
|---|---|
| Direct3D12 | Windows |
| Direct3D11 | Windows, UWP |
| Vulkan | Windows, Linux, Android, macOS（MoltenVK） |
| Metal | macOS, iOS, tvOS |
| OpenGL/GLES | Linux, Android, macOS |
| WebGPU | Web（Emscripten） |

**Artifact での利用方法**：
- `ArtifactIRenderer` が DiligentCore の `IRenderDevice`, `IDeviceContext` を直接保持
- シェーダーは HLSL で記述、各種バックエンドに自動変換
- PSO（Pipeline State Object）管理、リソースバインディングは Diligent 任せ
- Render State Notation（JSON）でパイプライン描述可能
- 重要な制約：D3D12固有コードに触れる場合は設計レビュー必須

**ディレクトリ構成**：
```
libs/DiligentEngine/
├── DiligentCore/      ← グラフィックスエンジン本体（submodule）
├── DiligentTools/     ← テクスチャローダ、imgui統合など
├── DiligentFX/        ← PBR、ポストプロセス、シャドウなど高levelコンポーネント
└── DiligentSamples/   ← チュートリアル・サンプル
```

---

### llama.cpp (`libs/llama.cpp/`)

**役割**：ローカルLLM（Large Language Model）推論エンジン。AIアシスタント機能を実装。

**主な機能**：
- モデルサポート：LLaMA, LLaMA2, LLaMA3, Mistral, Mixtral, Gemma など
- 量子化：1.5bit〜8bit整数、CPU+GPUハイブリッド推論
- バックエンド：CUDA, Vulkan, Metal, SYCL, CPU（x86, ARM, RISC-V）
- WebUI サーバー（llama-server）も含む
- マルチモーダル（画像+テキスト）対応

**Artifact での統合**：
- `Core.AI.LlamaLocalAgent`, `LocalAIAgent` 経由で推論実行
- `Core.AI.McpBridge` で MCP（Model Context Protocol）連携
- `Core.AI.ToolBridge` でツール実行（プロジェクト操作、レンダリングなど）
- AI機能の一部は sandbox 環境（`Core.AI.CommandSandbox`）で実行

---

### OpenCV

**用途**：
- 画像処理：`ImageProcessing/OpenCV/` - モノクロ、ポスタリゼーション、ネガティブ
- ラスタライゼーション：`Composition/OpenCVCompositionBuffer2D` - ソフトウェア合成
- ロトスコープ：`ImageProcessing/OpenCV/OpenCVPuppetEngine`, `OpenCVRotoBrushEngine`

**連携クラス**：`ImageAnalyzer`, `AudioAnalyzer`

---

### FFmpeg

**用途**：
- 動画デコード：`Video/FFmpegVideoDecoder`
- 動画エンコード：`Video/FFmpegEncoder`
- 音声エンコード：`Video/FFmpegAudioEncoder`
- サムネイル抽出：`Codec/FFmpegThumbnailExtractor`

**ライセンス**: GPL/LGPL、プロジェクト全体のライセンスに影響

---

### GStreamer

**代替実装**：
- `GStreamerDecoder` - マルチメディアパイプライン
- `GStreamerEncoder` - エンコード

---

## 依存関係図（テキスト表現）

```
[UI Layer] Qt6
    ↑
[Widget Layer] ArtifactWidgets
    ├── Common widgets
    ├── Timeline widgets
    ├── Render widgets
    └── Color widgets
    ↑
[Application Layer] Artifact
    ├── Project.Manager
    ├── Composition.*
    ├── Layer.*
    ├── Mask.*
    ├── Render.*
    │   └── ArtifactIRenderer ───┐
    ├── Service.*                 │
    ├── Effetcs.*                 │
    └── Widgets.*                 │
                                 ↓
[Core Layer] ArtifactCore         [Third-party]
    ├── AI.*                       ├── DiligentEngine (GPU)
    ├── Color.*                    ├── llama.cpp (LLM)
    ├── Graphics.*                 ├── OpenCV (Image Proc)
    ├── Image.*                    ├── FFmpeg (Codec)
    ├── Math.*                     ├── GStreamer (Media)
    ├── Script.*                   ├── Qt6 (UI)
    ├── Shape.*                    ├── TBB (Threading)
    ├── Text.*                     ├── EnTT (ECS)
    ├── Time.*                     └── Boost (Utilities)
    ├── Transform.*
    ├── UI.*
    ├── Utils.*
    ├── Video.*
    └── Audio.*

フロー:
User → Qt UI → Artifact Services → ArtifactCore → DiligentEngine → GPU
                                    ↓
                              OpenCV / FFmpeg / llama.cpp
```

---

## その他重要情報

### エントリーポイント
- `Artifact/src/AppMain.cppm` - アプリケーション初期化、主要widgetの生成と接続

### テスト・診断
- `TestEncoder` (`ArtifactCore/include/TestEncoder.ixx`)
- `ArtifactSoftwareRenderTestWidget`
- `ArtifactSoftwareCompositionTestWidget`
- `ArtifactSoftwareLayerTestWidget`
- `ArtifactLayerCompositeTestWidget`
- `ArtifactTimelineLayerTestWidget`

### マイルストーン・ドキュメント
主要な設計・実装計画は `docs/planned/` に多数存在：
- `MILESTONE_*` 各機能の実装マイルストーン
- `WIDGET_MAP.md` - ウィジェット名称・役割一覧（必読）
- `EFFECT_SYSTEM_SPECIFICATION.md` - エフェクトシステム仕様
- `group-layer-render-plan.md`, `LAYER_COMPOSITE_TEST_WIDGET.md` など

---

**最終更新**: 2026-04-17  
**調査対象コミット**: ArtifactStudio 親リポジトリ + 3サブモジュール  
**注意**: サブモジュール（`libs/DiligentEngine`, `libs/llama.cpp` 等）は変更せず、レビュー必須
