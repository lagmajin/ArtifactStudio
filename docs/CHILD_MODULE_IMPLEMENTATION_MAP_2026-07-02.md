# ArtifactStudio 子モジュール実装マップ

**作成**: 2026-07-02  
**対象**: `ArtifactCore` / `Artifact` / `ArtifactWidgets`  
**目的**: 全実装の俯瞰マップ。今後の開発で「どこが完了済みか」「どこに手を付けるべきか」を即座に判断するためのインデックス。



---

## 2. ArtifactCore 実装マップ

**役割**: 非GUIの基盤モジュール群。全レイヤーで共有される低〜中レベル機能。

### 2.1 ドメイン別実装一覧

凡例: ✅ 完了 / 🟡 部分的 / 🔴 未着手 / 🆕 新規

| # | ドメイン | ファイル数 | 状態 | 備考 |
|---|---------|:---------:|:----:|------|
| 1 | **Acoustic** | 5 cppm + 2 ixx | 🟡 | 摩擦/共鳴/風/雨/空間音響モデル |
| 2 | **AI** | 16 cppm | 🟡 | TieredAIManager, LlamaLocal, OnnxDml, Description群 |
| 3 | **Analyze** | 3 cppm + 1(OCV) | 🟡 | Histgram, ImageAnalyzer, SmartPalette |
| 4 | **Animation** | 5 cppm + 1 ixx | 🟡 | AnimatableTransform2D/3D, Dynamics, Easing |
| 5 | **Application** | 1 cppm | ✅ | ArtifactAppSettings |
| 6 | **Asset** | 4 cppm | 🟡 | AssetDatabase, AssetManager, Importer |
| 7 | **Audio** | 24 cppm | 🟡 | Mixer, Bus, 各種Effect, ASIO, WASAPI |
| 8 | **Channel** | - | 🔴 | 未確認 |
| 9 | **Color** | 10+ cppm | 🟡 | FloatColor, ACES, LUT, ColorWheels |
| 10 | **Container** | 6 cppm | ✅ | NamedVector, List, SmallVector, NameMap, IdMap |
| 11 | **Event** | - | ✅ | EventBus, Debugger |
| 12 | **Graphics** | 多数 | 🟡 | Particle, RayTracing, MotionBlur, MaskRasterizer |
| 13 | **Image** | 11 cppm + 1 cpp | 🟡 | F32x4_RGBA, PSD, YUV420, OpenCV連携 |
| 14 | **ImageProcessing** | 多数+3 ixx | 🟡 | CreativeEffects14種✅, OpenCV, Halide |
| 15 | **Math** | 多数 | 🟡 | Bezier, Interpolate, Quaternion, Noise |
| 16 | **NLE** | - | 🟡 | Sequence, Track, Clip, Transition, Marker |
| 17 | **Shape** | 6 cppm | 🟡 | ShapePath, Group, AeOperators, Repeater |
| 18 | **Text** | 多数+1 ixx | 🟡 | TextEngine, GlyphLayout, HarfBuzz, Japanese |
| 19 | **Time** | 5 cppm | ✅ | RationalTime, TimeCode, TimeRemap |
| 20 | **Transform** | 10 cppm | 🟡 | Camera, Light, Rotate, Scale, Zoom |
| 21 | **UI** | 6 cppm | 🟡 | InputOperator, Selection, Shortcut, Viewport |
| 22 | **Utils** | 多数 | ✅ | UniString, Path, Tag, Id, Hash, Profiler |
| 23 | **Video** | 20+ cppm + 13 ixx | 🟡 | FFmpeg, GStreamer, 14種Transition |
| 24 | **VST** | 2 cppm | 🟡 | VSTEffect, VSTHost |
| 25 | **XR** | 1 cppm | 🟡 | OpenXR (最小限) |

---

## 3. Artifact 実装マップ

**役割**: アプリケーション層。UI Widget、Composition、Layer、Effect、Render、Service、Project管理。

### 3.1 include ディレクトリ別ドメイン一覧

| # | ドメイン | ファイル数(.ixx) | 状態 | 説明 |
|---|---------|:--------------:|:----:|------|
| 1 | **AI** | 5 | 🟡 | AIClient, File/Material/Render/Workspace Automation |
| 2 | **Application** | 3 | 🟡 | ActiveContextService, AppManager, CommandDispatcher |
| 3 | **Artifact** | 1 | ✅ | AppMain (エントリポイント) |
| 4 | **Asset** | 3 | 🟡 | AssetResult, DirectoryModel, MenuModel |
| 5 | **Audio** | 8 | 🟡 | AudioMixer, Scrub, Waveform, ClockProvider, Devices |
| 6 | **Color** | 9 | 🟡 | ColorGrading, NodeGraph, Wheels, OCIO |
| 7 | **Composition** | 9 | 🟡 | Abstract, 2D/3D, Manager, PlaybackController, Parametric |
| 8 | **Core** | 3 | 🟡 | WorkspaceManager, SystemStats, SmartGuides |
| 9 | **Diagnostics** | 2 | 🟡 | Notification, ValidationRules |
| 10 | **Effect** | 8 | 🟡 | CornerPin, Creative, Film, MotionBlur, Stabilizer, Transition |
| 11 | **Effects** | 多数 | 🟡 | AbstractEffect, Field, 各種VisualEffect |
| 12 | **Export** | 4 | 🟡 | PremierePro, FFmpeg, ImageSequence, PythonAPI |
| 13 | **Layer** | 20 | 🟡 | LayerFactory, Property, 各種Layer型 |
| 14 | **Mask** | 4 | 🟡 | MaskPath, LayerMask, Settings, MaskTool |
| 15 | **Project** | 7 | 🟡 | Document, ProjectManager, Serializer, TreeModel, TemplateManager |
| 16 | **Render** | 7 | 🟡 | ArtifactIRenderer, LayerRenderNode, RenderPassSpec, RenderQueueJob |
| 17 | **Timeline** | 2 | 🟡 | TimelineKeyBinding, TimelineService |
| 18 | **Undo** | 2 | 🟡 | UndoManager, UndoableCommand |
| 19 | **Widgets** | 50+ | 🟡 | 下記 3.2 で詳細 |

### 3.2 Artifact Widgets 詳細（Artifact 管理下）

| カテゴリ | キーWidget | ファイル数 | 状態 |
|---------|-----------|:---------:|:----:|
| **MainWindow** | ArtifactMainWindow, DockStyleManager | 3 | 🟡 |
| **Project** | ArtifactProjectManagerWidget, ArtifactInspectorWidget | 6 | 🟡 |
| **Asset** | ArtifactAssetBrowser | 1 | 🟡 |
| **AI** | ArtifactAICloudWidget | 2 | 🟡 |
| **Render** | CompositionEditor/Overlay, Gizmo群, PieMenu, LayerEditor | 30+ | 🟡 |
| **Timeline** | TrackPainterView, ScrubBar, Navigator, Keyframe, EasingLab | 20+ | 🟡 |
| **Property** | ArtifactPropertyEditor | 1 | 🟡 |
| **Viewer** | ArtifactContentsViewer | 1 | 🟡 |
| **Diagnostics** | ProfilerPanel, EventBusDebugger | 5 | ✅/🟡 |
| **Menu** | File/Edit/View/Composition/Layer/Effect etc. | 20+ | 🟡 |
| **WebUI** | ArtifactWebBridge, ArtifactWebUIHost | 2 | 🟡 |

---

## 4. ArtifactWidgets 実装マップ

**役割**: Qt6 ベースの共通Widgetライブラリ。Artifact から利用される独立DLL。

| # | ドメイン | .ixx | .cppm | .cpp | 状態 | 説明 |
|---|---------|:---:|:----:|:----:|:----:|------|
| 1 | **3D** | 1 | 0 | 0 | 🟡 | ModelViewer |
| 2 | **Audio** | 3 | 0 | 1 | 🟡 | AudioBusWidget, VolumeSlider |
| 3 | **Button** | 3 | 0 | 1 | 🟡 | ColorPickerButton, FloatColorPickerButton, SubToolButton |
| 4 | **Code** | 2 | 2 | 0 | 🟡 | CodeEditor, SyntaxHighlighter |
| 5 | **Color** | 5 | 1 | 4 | 🟡 | VectorScope, ColorWheel, Histgram, Parade/WaveformScope |
| 6 | **Common** | 14 | 5 | 7 | 🟡 | AbstractWidget/Dialog, DragSpinBox, EditableLabel, EnhancedSlider, ClickableLabel, CollapsibleSection, ToggleGlyphButton, WidgetCounter, Win関連 |
| 7 | **Console** | 1 | 0 | 1 | 🟡 | ConsoleWidget |
| 8 | **Dialog** | 3 | 2 | 1 | 🟡 | FloatColorPicker, InterpretFootageDialog, KeyboardOverlay |
| 9 | **Dock** | 2 | 0 | 1 | 🟡 | HeadPanel, Pane |
| 10 | **Effect** | 1 | 0 | 1 | 🟡 | WidgetGlowFrame |
| 11 | **Graphics** | 1 | 0 | 2 | 🟡 | FloatImageItem, NodeWireGraphicItem |
| 12 | **Image** | 1 | 0 | 1 | 🟡 | BasicImageViewWidget |
| 13 | **Knob** | 7 | 3 | 2 | 🟡 | RotaryKnob, KnobSlider, KnobCheckBox, AbstractKnobEditor, KnobEditorWidget, KnobColorPicker |
| 14 | **Platform** | 2 | 0 | 0 | 🟡 | QuickLook, MacTouchBar |
| 15 | **Preview** | 1 | 0 | 1 | 🟡 | PreviewControlWidget |
| 16 | **Render** | 3 | 1 | 2 | 🟡 | BackendSettingWidget, RayTracerWidget, RenderQueueManager |
| 17 | **Time** | 2 | 0 | 2 | 🟡 | TimeCodeEditor, TimeCodeLabel |
| 18 | **Video** | 1 | 0 | 1 | 🟡 | ArtifactBasicVideoPreviewWidget |
| 19 | **Viewer** | 2 | 0 | 2 | 🟡 | ContentViewer, ModelViewer |

---

## 5. ドメイン横断マップ

各機能が 3 モジュール間でどう分散しているかを示す。

### 5.1 Render Pipeline

```
ArtifactWidgets → Artifact → ArtifactCore
BackendSetting   ArtifactIRenderer       RenderWorker
RayTracerWidget  CompositionEditor       Graphics/*
                 CompositionRenderWidget MaskCutoutPipeline
                 DiligentEngineWindow
                 Gizmo群 (Transform/3D/Text)
```

### 5.2 Color Pipeline

```
ArtifactWidgets → Artifact → ArtifactCore
FloatColorPicker   ColorGradingEngine     Color::FloatColor
ColorWheel         ColorManagement        Color::ColorManager
VectorScopeWidget  ColorNodeGraph         Color::ACESManager
HistgramWidget     ColorWheels            Color::ColorLUT
ParadeScopeWidget  OCIOManager            Analyze::Histgram
```

### 5.3 Audio Pipeline

```
ArtifactWidgets → Artifact → ArtifactCore
AudioBusWidget     ArtifactAudioMixer     Audio::AudioMixer
VolumeSlider       ArtifactAudioWaveform  Audio::AudioBus
TimeCodeEditor     AudioScrubController   Audio::AudioAnalyzer
                   AudioClockProvider     Audio::各種Effect
                   PortAudio/WASAPI       Audio::AudioRenderer
```

### 5.4 AI Pipeline

```
ArtifactWidgets → Artifact → ArtifactCore
(none)             AICloudWidget           AI::TieredAIManager
                   AI::AIClient            AI::LlamaLocalAgent
                   Automation群            AI::OnnxDmlLocalAgent
                   (File/Material/Render/  AI::APIKeyManager
                    Workspace)             AI::Description群15+
```

### 5.5 Timeline

```
ArtifactWidgets → Artifact → ArtifactCore
TimeCodeEditor    TimelineWidget         Time::RationalTime
TimeCodeLabel     LayerPanelWidget        Time::TimeCodeRange
                  TrackPainterView        Animation::KeyframeEditingTools
                  KeyframeModel           Animation::EasingCurveUtil
                  ScrubBar/Navigator
                  WorkAreaControl
                  TimeCodeWidget
```

### 5.6 Video / Media

```
ArtifactWidgets → Artifact → ArtifactCore
BasicVideoPreview Layer::VideoLayer      Video::FFmpegVideoDecoder
ContentViewer     Layer::AudioLayer       Video::FFmpegEncoder
                  Layer::MediaLayer       Video::GStreamerDecoder/Encoder
                  ArtifactContentsViewer  Video::Transition 14種
                                          Media::ISource/Frame/Probe
```

### 5.7 Text System

```
ArtifactWidgets → Artifact → ArtifactCore
(none)            Layer::TextLayer       Text::TextEngine
                                         Text::TextStyle
                                         Text::GlyphLayout
                                         Text::FontDescriptor
                                         Text::HarfBuzz (Shaping)
                                         Font::FreeFont
```

### 5.8 Effect System

```
ArtifactWidgets → Artifact → ArtifactCore
WidgetGlowFrame   AbstractEffect         ImageProcessing::*
                  GlobalEffectManager     14 Creative Effects ✅
                  FilmEffects/MotionBlur  OpenCV系エフェクト
                  Stabilizer/Transition   Halide系エフェクト
```


---

## 6. 未着手／部分実装のホットスポット

### 6.1 🔴 未着手（実装ファイルが1つも存在しない）

**凡例**: 「宣言のみ」= include/.ixx はあるが src/ に .cppm/.cpp がゼロ

| 優先度 | 領域 | モジュール | 詳細 |
|:-----:|------|-----------|------|
| Mid | **Crowd Simulation** | ArtifactCore | BoidsSwarmSystem.ixx宣言のみ、src/ゼロ |
| Mid | **Rule System** | ArtifactCore | NamingRule.hppのみ、src/ゼロ |
| Low | **FSR (AMD FidelityFX)** | ArtifactCore | AMDヘッダのみプロジェクトコードゼロ |
| Mid | **Components/Field** | Artifact | FieldComponent.ixx宣言のみ、src/ゼロ |
| Low | **OFX Plugin (連携)** | Artifact | 上流SDKヘッダのみ、自前コードゼロ |
| Low | **SearchWidget** | ArtifactWidgets | SearchWidget.ixx宣言のみ、src/ゼロ |

### 6.2 🟡 以下は「最小限あるがある程度足りない」領域

| 領域 | モジュール | ファイル数 | 現状 |
|------|-----------|:---------:|------|
| Command IR | ArtifactCore+Artifact | 1 cppm + 7 ixx | ✅ Phase1完了: 6コマンド種のExecutor実装・WorkspaceAutomation経由で実働。Phase2以降は語彙拡張・Resolver・Macro |
| Physics | ArtifactCore | 7 cppm | Fluid/Fracture/MPM/Sand/SoftBody 各1ファイル、統合不足 |
| Plugin | ArtifactCore | 2 cppm | PluginCommon/Registry 最小限 |
| Rig / 2D Rig | ArtifactCore | 1 cppm + 1 ixx | Rig2D 基本構造のみ |
| Simulation | ArtifactCore | 1 cppm | PyroSimulation 1ファイル |
| Mesh | ArtifactCore | 1 cppm | Mesh 最小限 |
| ShaderNode | ArtifactCore | 1 cppm + 1 ixx | 基本構造のみ |
| Scene | ArtifactCore | 1 cppm | SceneNode 最小限 |
| Generate | ArtifactCore | 1 cppm | GenerateTestImageのみ |

### 6.3 🟡 部分実装（要確認・要拡充）

| 優先度 | 領域 | 現状 | 次の一手 |
|:-----:|------|------|---------|
| High | GPU Mask Compute Phase 3 | Phase1/2/4 ✅、Phase3未接続 | CompositionRenderControllerへの配線 |
| High | GPU Text Rendering WP2-6 | WP1(GlyphAtlas) 設計済み未実装 | 本格GPUテキスト描画パス |
| High | Critical Render/Media Stability | 不具合あり、診断導線強化中 | smoke test, regression surface |
| Mid | Comp Editor Mask/Roto | 基礎実装済み、編集UX不足 | Mask/Roto Editingワークフロー強化 |
| Mid | Text Animator Next Gen | TextAnimatorEngine存在、UI/Timeline未統合 | Selector/Preset/Timeline連携 |
| Mid | NLE Core UI統合 | Sequence/Track/Clip実装済み、UI未統合 | NLE編集モードのUI結合 |
| Mid | Particle Layer 3D Migration | 別パスで実装中 | 3Dパーティクルレンダリング安定化 |
| Mid | Parametric Composition | Slot model 実装中 | instance override, cache key |

### 6.4 既知の問題領域

| 問題 | モジュール | 文書 |
|------|-----------|------|
| Focus問題 (Composition Editor) | Artifact | BUG_REPORT_COMPOSITION_EDITOR_GHOST_OVERLAY |
| RGB/BGR混在リスク | ArtifactCore | RGB_BGR_CHANNEL_ORDER_REFERENCE |
| マスク消失 | Artifact | BUG_REPORT_COMPOSITION_MASK |
| 3D Viewport描画不具合 | ArtifactCore | BUG_REPORT_3D_VIEWPORT |
| Property Editor ラグ | Artifact | PROPERTY_EDITOR_LAG_HYPOTHESIS |
| Video Layer フリッカー | ArtifactCore/Video | REPORT_VIDEO_LAYER_FLICKER |
| Shape Layer Gizmo ラグ | Artifact | SHAPE_LAYER_GIZMO_LAG_FIX_PLAN |

---

## 7. 優先開発候補一覧

### Tier 1: 今すぐ着手すべき

| # | 候補 | モジュール | 工数 | 理由 |
|---|------|-----------|:----:|------|
| 1 | Critical Render/Media Stability | Artifact+Core | 15-20h | 描画信頼性が全開発の前提 |
| 2 | GPU Mask Compute Phase 3 | Artifact(Comp) | 5-8h | Phase1/2/4完了、残るは配線のみ |
| 3 | Container Migration 継続 | ArtifactCore | 10-15h | Phase1-2完了、Phase3へ |

### Tier 2: 短期〜中期的に着手

| # | 候補 | モジュール | 工数 | 理由 |
|---|------|-----------|:----:|------|
| 4 | ~~Command IR / Automation~~ | ArtifactCore+Artifact | ✅Phase1-2完了 | 14→34コマンド種に拡張、AI向けread/inspect/management完備 |
| 5 | Text Animator Next Gen | Artifact+ArtifactCore | 12-20h | UE/AE互換テキストアニメーション |
| 6 | Comp Editor Mask/Roto | Artifact | 10-15h | 編集ワークフローの完成 |
| 7 | GPU Text Draw WP-1 | ArtifactCore | 8h | GlyphAtlas、後続の前提 |
| 8 | ~~ShapeOperator 実装~~ | ArtifactCore/Shape | ✅完了 | MergePaths に Difference/Merge モード追加 |
| 9 | NLE Core UI統合 | Artifact | 15-25h | NLE編集モードの本格化 |

### Tier 3: 長期的・依存解決後

| # | 候補 | 依存 |
|---|------|------|
| 10 | OIIO Image Pipeline Migration | Color Management 基盤完了後 |
| 11 | Video QImage Retirement | Color/Image 基盤完了後 |
| 12 | Puppet Tool | Rigシステム完成後 |
| 13 | Physics Engine | 基盤整備後 |
| 14 | Plugin System | アーキテクチャ安定後 |

---

**最終更新**: 2026-07-02 (ShapeOperator Difference/Merge 追加, Command IR Phase1 Executor実装)  
**総ファイル数**: ~760 (ArtifactCore ~244 + Artifact ~420 + ArtifactWidgets ~96)  
**次回更新**: 新モジュール追加時 / マイルストーン完了時 / 大規模リファクタ後

