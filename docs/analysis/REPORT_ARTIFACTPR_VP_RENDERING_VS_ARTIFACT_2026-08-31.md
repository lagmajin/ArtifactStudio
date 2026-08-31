# ArtifactPr VP / レンダリング — Artifact との比較レポート 2026-08-31

**最終更新:** 2026-08-31

作成日: 2026-08-31
目的: `ArtifactPr` のビューポート(VP)とレンダリング経路を `Artifact` と比較し、**差分・劣っている点・棲み分け**を整理する。
調査範囲: `ArtifactPr/` 配下の実装ファイルと `Artifact/include/Widgets/Render/`, `Artifact/include/Widgets/` の VP / レンダリング経路。
未確認事項は明示する。

---

## 1. 全体像

`ArtifactPr/README.md`:
> Pr-like editor prototype that reuses `ArtifactCore` as the shared foundation.
> This project is intentionally separate from `Artifact` so the two apps can grow in different directions without forcing the same UI or timeline shape.

`ArtifactPr/CMakeLists.txt:72` の依存:
```
ArtifactCore, ArtifactCoreCommand, ArtifactCoreAudio, ArtifactCoreVideo, ArtifactCoreNLE, ArtifactWidgets```
加えて `qtadvanceddocking-qt6` を link。

**`Artifact` 本体 / `ArtifactRenderer` / `DiligentEngine` には一切依存していない**。grep `Diligent|IRenderer|RenderQueue|RenderPass` を `ArtifactPr/` 配下に実施 → 結果 0 件(2026-08-31 時点)。

→ **ArtifactPr は NLE(動画編集)プロトタイプ**で、Premiere 風のレイアウトを再現することが目的。Artifact 側の DCC / レイヤーベースレンダリングとは目的が異なる。

### 1.1 ファイル構成

行数の多い順(`ArtifactPr/` 配下、`powershell Get-ChildItem` で集計):

| ファイル | 行数 |
|---|---|
| `src/ArtifactPrEditorEngine.cppm` | 3070 |
| `src/ArtifactPrMainWindow.cppm` | 2770 |
| `src/EditCommand.cppm` | 531 |
| `src/SequenceExporter.cppm` | 470 |
| `src/ClipEffects.cppm` | 464 |
| `src/ShortcutHelpDialog.cppm` | 407 |
| `src/AudioPreviewMixer.cppm` | 318 |
| `src/ExportDialog.cppm` | 310 |
| `src/MediaPanel.cppm` | 267 |
| `src/MediaThumbnailer.cppm` | 211 |
| `src/EditCommand.ixx` | 204 |
| `src/SequenceAudioRenderer.cppm` | 191 |
| `src/MediaFrameDecoder.cppm` | 179 |
| `src/PrShortcut.cppm` | 172 |
| `src/TransportBarWidget.cppm` | 160 |
| `src/AppTheme.cppm` | 157 |
| `src/VideoPlayerWidget.cppm` | 110 |
| `src/VideoSurface.cppm` | 93 |
| `src/MarkerEditDialog.cppm` | 79 |
| `src/PrColorPickerDialog.cppm` | 76 |
| `src/ClipEffects.ixx` | 71 |
| `src/SequenceCompositor.cppm` | 65 |
| `src/TimecodeOverlayWidget.cppm` | 58 |

---

## 2. ビューポート比較

| 観点 | ArtifactPr | Artifact |
|---|---|---|
| VP クラス | `VideoPlayerWidget` + `SequenceCanvasWidget`(`ArtifactPrMainWindow.cppm:634` 内にインライン定義) | `ArtifactCompositionEditor`(`Artifact/include/Widgets/Render/ArtifactCompositionEditor.ixx:16`) + `ArtifactDiligentEngineRenderWindow`(`Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx:39`) + `ArtifactViewportCamera` |
| 描画バックエンド | **QMediaPlayer + QAbstractVideoSurface + QPainter**(CPU) | **DiligentEngine(D3D12/Vulkan) GPU パス** + `CompositionRenderController` |
| 表示解像度 | ソース解像度のまま / canvas サイズに fit | コンポジション解像度、Transform / Camera 適用 |
| 操作系 | タイムライン ruler のフレームクリックでシーク、ズームスライダ、トランジション / クリップのドラッグ | パン、回転、ズーム、Fit/Fill/100%、Toolbox トグル、Shortcut フル装備 |
| フレームソース | `MediaFrameDecoder`(FFmpeg ベース、別 worker thread)で順次 decode → `ArtifactCore::ImageF32x4_RGBA` → `composeSequenceLayers` でアルファ合成 → `QPainter::drawImage` 表示 | レイヤーコンポーネント評価 → `RenderQueueService` → `IRenderDevice` → SwapChain |
| 信号 | `currentFrameChanged` / `sequenceChanged` / `playbackStateChanged` | `videoDebugMessage` 等(独自)、RenderController 経由のフレーム更新 |

### 2.1 `VideoPlayerWidget`(ソースモニター、`ArtifactPr/src/VideoPlayerWidget.cppm:62-135`)

- `QMediaPlayer` + `QAbstractVideoSurface`(`ArtifactPr::PrVideoSurface`、`ArtifactPr/src/VideoSurface.cppm:16-88`)
- `PrVideoSurface::present()` で `QVideoFrame::toImage()` してシグナル送出
- `VideoCanvas::paintEvent`(`VideoPlayerWidget.cppm:43-58`)で `QPixmap::scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)` 描画
- AGENTS.md「QImage は表示境界のみで Pixmap 化」「QPainter での新規合成禁止」は整合。`PrVideoSurface` 内で `toImage()` して paintEvent 内で `QPixmap::fromImage`、decoder → compositor → canvas の流れは `ImageF32x4_RGBA` を維持

### 2.2 `SequenceCanvasWidget`(プログラムモニター、`ArtifactPr/src/ArtifactPrMainWindow.cppm:634-691` インライン定義)

- 受信は `ArtifactCore::ImageF32x4_RGBA` のみ、内部 `cachedImage_` は表示境界で `frame_.toQImage()` して `QPainter::drawImage`(`MainWindow.cppm:651-686`)
- レターボックス(黒背景)中央配置スケーリングのみ
- Transform / Pan / Rotate 操作は無し

### 2.3 `ArtifactCompositionEditor`(`Artifact/include/Widgets/Render/ArtifactCompositionEditor.ixx:16-53`)

- `play / pause / togglePlayPause / stop / resetView / zoomIn / zoomOut / zoomFit / zoomFill / zoom100 / toggleViewportToolboxes` のスロット完備
- `handleImportPlacementKeyPress(QKeyEvent*)` でインポート配置キーハンドラ
- 内部に `class Impl; Impl* impl_` の PImpl、所有は `Impl*` の明示所有(AGENTS.md 整合)
- `CompositionRenderController` を内包

### 2.4 `ArtifactDiligentEngineRenderWindow`(`Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx:39`)

- `Diligent::IRenderDevice` / `IDeviceContext` / `ISwapChain` を RefCntAutoPtr で保持
- `Solid / Wireframe / SolidWithWire` の 3 シェーディングモード
- `useSoftwareFallback_` / `usingSharedDevice_` のフォールバック分岐
- PImpl + AGENTS.md 整合

---

## 3. レンダリング経路比較

| 経路 | ArtifactPr | Artifact |
|---|---|---|
| **合成** | `composeSequenceLayers(canvasSize, layers, bg)`(`ArtifactPr/src/SequenceCompositor.cppm:60-85`)、`ImageF32x4_RGBA::alphaBlend` + OpenCV `cv::resize` | `RenderQueueService` + `LayerEvaluationContext` → `IRenderDevice` → SwapChain(GPU) |
| **フレーム合成単位** | NLE store(`ArtifactCore::NLE::NLEProjectStore`)からアクティブクリップ収集 → 各 clip を FFmpeg で decode → `applyClipEffects`(`ArtifactPr/src/ClipEffects.cppm`)→ `ImageF32x4_RGBA` | レイヤー単位(`AbstractLayer` の描画メソッド)で各コンポーネント評価 → `applyClonerComponentTransform` 等の evaluator 群 |
| **エフェクト** | クリップ単位 11 種類(brightnessContrast / hueSaturation / colorWheels / gaussianBlur / boxBlur / unsharpMask / glow / posterize / scale / rotate / audioGain / audioEqualizer)、`ClipEffects.cppm` 464 行、CPU で OpenCV 適用 | レイヤー単位コンポーネント 9 種類(Box2D 物理 / Crowd / Fluid / Particle / Fracture / Joint / Layout / Cloner / Motion) |
| **エクスポート** | `SequenceExporter::exportSequence`(`SequenceExporter.cppm:281-535`)、`SequenceTimelineRenderer` で plan を凍結 → 全フレーム逐次 `renderFrame` → `ArtifactCore::FFmpegEncoder` で h264/hevc/prores/dnxhd mp4mov または連番 PNG/JPEG 出力。音声は別途 `renderSequenceAudio` → ffmpeg で mux | `Artifact` 側にも export 経路はあるが、`ArtifactPr` ほどの FFmpeg ベース外部 encoder は使わず、GPU 経由の offline render + `ArtifactPr` 的な CPU fallback もある(未確認) |
| **動画再生 / パフォーマンス** | OpenCV `cv::resize` + `ImageF32x4_RGBA` 合成(CPU)、全フレーム逐次。`ProgramMonitorPanel::onFrameDecoded` で z-order 合成、`requestGeneration_` で古い結果を破棄 | GPU パスでリアルタイム。CPU fallback は `useMfr = false` 経路(`ArtifactRenderQueueService.cppm:6660` 周辺) |
| **RenderPlan 凍結** | あり。`ArtifactPr/include/ArtifactPrEditorEngine.ixx:140-157` の `RenderPlan` で nleSnapshot / clipEffects / transitions / audioClips を凍結。preview と export で同じ evaluator(`SequenceTimelineRenderer` / `ProgramMonitorPanel::onFrameDecoded`)を使う | `LayerEvaluationState` 機構(`ArtifactLayerComponentSystem.ixx:725-866`) があるが、Composition 側 evaluator 本体は未確認 |

### 3.1 ArtifactPr の合成パイプライン詳細

```
NLE Store → collectActiveClips(frame) → ActiveClip[]
   ↓
for each ActiveClip:
   decodeSourceFrame(filePath, sourceFrame, stillImage, ImageF32x4_RGBA&)
   ├─ still: stillCache_.find(filePath) → image.load(filePath)
   └─ video: FFmpegVideoDecoder.decodeNextVideoFrameRaw() or decodeFrameAtRaw()
              → CpuVideoFrame (RGB24) → cv::cvtColor(CV_8UC3 → CV_8UC4)
              → ImageF32x4_RGBA::setFromRGBA8
   applyClipEffects(decoded, clip.effects)
   CompositeLayer { frame=std::move(decoded), opacity=clip.opacity × transitionFactor }
   ↓
composeSequenceLayers(canvasSize, layers, background)
   for each CompositeLayer:
       fitFrameToCanvas(layer.frame) → cv::resize → 中央配置
       canvas.alphaBlend(overlay, layer.opacity)
   ↓
ImageF32x4_RGBA
```

- 全行程 CPU。OpenCV の `cv::INTER_LINEAR` リサイズ + `ImageF32x4_RGBA::alphaBlend`
- エクスポートは 1 フレームずつ逐次 `encoder.addImage(frame)`(`SequenceExporter.cppm:471`)

### 3.2 Artifact 側の合成(参考)

```
Layer / Composition
   ↓
ArtifactCompositionEditor::play() / pause()
   ↓
CompositionRenderController → RenderQueueService
   ├─ GPU パス: DiligentEngine::IRenderDevice → SwapChain
   └─ CPU fallback: useMfr = false (`ArtifactRenderQueueService.cppm:6660`)
   ↓
ComponentEvaluators
   ├─ Box2D Physics / Joint / Collision
   ├─ FluidSolver2D / LiquidSolver2D
   ├─ Particle Emitter / Crowd / Layout / Cloner
   └─ Fracture / Motion Dynamics
```

---

## 4. Artifact 視点で見た ArtifactPr の劣っている点 / 差分

### 4.1 GPU レンダリングが完全に欠落(最大)

- Artifact: DiligentEngine / D3D12 / Vulkan / シェーダ PSO / SwapChain 完備。物理 / 流体 / パーティクルが GPU パスで動く
- ArtifactPr: 全部 CPU。`ImageF32x4_RGBA` + OpenCV `cv::resize`。動画素材も静止画も全部 CPU アルファ合成
- 結果: フル HD 30fps リアルタイム再生が前提の NLE で CPU 合成はパフォーマンス的に苦しい。エクスポートも逐次 1 フレームずつ `cv::resize` + `alphaBlend`

### 4.2 VP のインタラクション貧弱

- Artifact: `ArtifactViewportCamera`(パン / 回転 / ズーム)、`zoomIn / zoomOut / zoomFit / zoomFill / zoom100`、Toolbox、Shortcut、`resetView`
- ArtifactPr: タイムライン ruler のフレームクリック + ズームスライダのみ。ビューポート自体のパン / 回転 / ズームはなし(素材の解像度で固定表示)

### 4.3 タイムライン UI が全部手書き paintEvent

- `TimelinePanel::refreshTimeline`(`ArtifactPrMainWindow.cppm:2210-2236`)で毎回全 widget を `delete` して再構築 → ガベージと再レイアウトコスト
- `TimelineClipWidget` の `paintEvent`(`ArtifactPrMainWindow.cppm:176-213`)で都度描画。Qt 標準の `QGraphicsView` を使っていない
- `TimelineRulerWidget` も全 paintEvent で目盛・マーカー・トランジションを再描画
- Artifact 側は `ArtifactTimelineWidget.cppm` でフレーム単位の構造化表示とキャッシュ

### 4.4 エフェクトが「クリップの静止変換」止まり

- ArtifactPr: 11 種類全部 1 フレーム内で完結する OpenCV / ImageF32x4_RGBA の CPU 処理(色補正 / ぼかし / グロー / ポスタリゼーション / スケール / 回転 / ゲイン / EQ)
- 動画ならではの **時間軸エフェクト(時間リマップ / モーションブラー / 速度補間 / ピクセルモーション / ネスト合成)** が無い
- トランジションも Crossfade / DipToBlack / WipeLeft / WipeRight の 4 種のみで opacity カーブが線形 + DipToBlack のみ(`SequenceExporter.cppm:71-91` の `transitionSpansFor`)

### 4.5 評価 / 編集モデルが NLE ベースのみで AE 式は不在

- ArtifactPr は「タイムライン上のクリップ」が編集の最小単位、レイヤーやコンポジションという概念は無し
- Artifact 側は `AbstractLayer` を時間軸に複数並べる AE 風モデル。`ArtifactCompositionEditor` で複数レイヤーを一つのコンポジションとして評価
- 結果: マスク / エクスプレッション / ネストコンポジション / 3D レイヤー / 親子の transform / 自動オリゴンメーション等は Artifact のみ

### 4.6 RenderPlan 凍結アーキテクチャは存在するが、Artifact ほどの render path 階層は無い

- ArtifactPr は `RenderPlan`(`ArtifactPr/include/ArtifactPrEditorEngine.ixx:140-157`)で nleSnapshot / clipEffects / transitions / audioClips を凍結して preview と export で同じ evaluator(`SequenceTimelineRenderer` / `ProgramMonitorPanel::onFrameDecoded`)を使う
- 一方 `ProgramMonitorPanel` のソースは `requestPreviewFrame` でフレーム都度 `MediaFrameDecoder` に逐次要求、`onFrameDecoded` で z-order 合成 → 編集状態と preview の同期は generation 番号で担保、export 側は別 thread で同じレンダラーを使う
- これは **NLE 用には十分合理的** だが、Artifact 側の `LayerEvaluationState`(9 コンポーネントの intent/drive/dynamics ディスパッチ)ほどの拡張性はない

### 4.7 複数ウィンドウ / DCC 的なドッキング不足

- ArtifactPr は `qtadvanceddocking-qt6` を link している(`ArtifactPr/CMakeLists.txt:97-98`)が、`ArtifactPrMainWindow` 内で固定パネル構成を組んでいる
- ユーザー側でパレットを動かせるドッキングフル活用はしていない

### 4.8 依存する外部 plugin / shared 機能の少なさ

- Artifact: `ArtifactRenderer` / `ArtifactWidgets` を大量に活用、独自 plugin 機構
- ArtifactPr: `ArtifactWidgets` を link しているが、実質的に自前 widget 群で完結

---

## 5. ArtifactPr が優れている点 / Artifact と異なる強み

| 領域 | ArtifactPr の強み |
|---|---|
| **動画素材の取り回し** | FFmpeg ベース `MediaFrameDecoder` / `MediaThumbnailer`(worker thread)で non-blocking decode。Artifact は動画凍結方針で読み込み自体を後回し |
| **NLE 専用 UX** | ソース / プログラムモニターの分離、In / Out マーカー、Insert / Overwrite 編集、ripple delete / slip / slide / lift、`SnapKind`(Clip/Marker/Playhead/Transition/Frame/Second の 6 種)、Undo/Redo(QUndoStack + SerializableCommand)、J / K / L 系ショートカット、shuttle(1x/2x/4x/8x)、markers / transitions / effects の編集、`TransportBar` |
| **エクスポートの完成度** | H264 / HEVC / ProRes / DNxHD の MP4 / MOV、PNG / JPEG 連番、WAV / MP3、音声 mux、`tempVideoPath` 経由の中間ファイル運用、`cancel.load()` での cancel、`onProgress` コールバック |
| **Undo/Redo の command 体系** | `NLEStateCommand` / `MoveClipCommand` / `TrimClipCommand` / `DeleteClipCommand` / `VideoTracksStateCommand` / `MarkerStateCommand` / `TransitionStateCommand` / `ClipPropertyCommand`(`EditCommand.cppm` 531 行)。`ArtifactCore::SerializableCommand` を継承 |
| **i18n** | `uiText(en, ja)` / `trUi(en, ja)` ヘルパで日本語 / 英語の切替(システムロケール判定、`ArtifactPrMainWindow.cppm:84-110`)。Artifact 側はここまで統一されていない |
| **AudioMeterPanel** | peak hold / decay / dB ラベル付きメーター、`AudioPreviewMixer` の level callback 連動(`MainWindow.cppm:1109-1204`) |

---

## 6. まとめ / 棲み分け

両者は「タイムラインは共有するが目的が違う」独立アプリで、**Artifact が AE 寄り、ArtifactPr が Pr 寄り**という棲み分け。

| 観点 | Artifact(本流) | ArtifactPr(プロトタイプ) |
|---|---|---|
| ゴール | AE 風のコンポジション / レイヤー / コンポーネント編集 | Premiere 風の NLE 動画編集 |
| 描画バックエンド | GPU(DiligentEngine)+ CPU fallback | CPU のみ(QPainter / OpenCV) |
| 編集モデル | レイヤー + コンポーネント 9 種 | クリップ + トラック + トランジション |
| 強み | 9 種のコンポーネント(Box2D 物理、FluidSolver2D 等)、GPU パス、AE 式ワークフロー | NLE UX の作り込み、FFmpeg エクスポート、Undo/Redo command 体系、i18n |
| 弱み | 動画読み込み・エクスポート機能は薄い(動画凍結方針) | GPU パス無し、VP インタラクション貧弱、コンポーネント不在 |

ArtifactPr に Artifact の VP/レンダリング機能を移植する計画があるかは未確認。

---

## 7. 未確認事項

- `ArtifactPr` のビルド状態(最新ビルドが通っているか、warning / error の有無)
- Artifact 側 `ArtifactRenderer` との接続プラン(`ArtifactPr` 側で GPU パスを使う将来計画の有無)
- Artifact 側にも動画エクスポート経路があるか(エクスポートの代替が GPU offline render 経由で同等か)
- `qtadvanceddocking` の活用度(`ArtifactPrMainWindow` 内で `DockManager.h` を include しているが、レイアウトをユーザーが動かせるかは未確認)
- `LayerEvaluationState` の Composition 側 evaluator 本体(`ArtifactCore` 側 `NLE` モジュールに類似の evaluator がいるか)
- `ArtifactPr` 側のソースモニター / プログラムモニターのフレームレート実測値(FPS がターゲットに達しているか)

---

## 8. 関連ファイル

### ArtifactPr 側
- `ArtifactPr/README.md`(設計方針)
- `ArtifactPr/CMakeLists.txt:72`(依存関係)
- `ArtifactPr/src/main.cpp`(エントリ)
- `ArtifactPr/include/ArtifactPrMainWindow.ixx`
- `ArtifactPr/src/ArtifactPrMainWindow.cppm`(2770 行の本体)
- `ArtifactPr/include/ArtifactPrEditorEngine.ixx:140-157`(`RenderPlan` 凍結)
- `ArtifactPr/src/ArtifactPrEditorEngine.cppm`(3070 行)
- `ArtifactPr/include/VideoPlayerWidget.ixx` / `ArtifactPr/src/VideoPlayerWidget.cppm`
- `ArtifactPr/src/VideoSurface.cppm`(`PrVideoSurface` 実装)
- `ArtifactPr/include/SequenceCompositor.ixx` / `ArtifactPr/src/SequenceCompositor.cppm`
- `ArtifactPr/src/SequenceExporter.cppm`(470 行、エクスポート本体)
- `ArtifactPr/src/ClipEffects.cppm`(464 行、クリップエフェクト 11 種)
- `ArtifactPr/src/AudioPreviewMixer.cppm`(音声プレビュー)
- `ArtifactPr/src/SequenceAudioRenderer.cppm`(音声エクスポート)
- `ArtifactPr/src/MediaFrameDecoder.cppm` / `ArtifactPr/src/MediaThumbnailer.cppm`
- `ArtifactPr/src/EditCommand.cppm`(Undo/Redo command 体系)

### Artifact 側(比較対象)
- `Artifact/include/Widgets/Render/ArtifactCompositionEditor.ixx:16-53`
- `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx:39`
- `Artifact/include/Widgets/Render/ArtifactViewportCamera.ixx`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`
- `Artifact/src/Widgets/Render/ViewportColorPipeline.cppm`
- `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`
- `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx:725-866`(9 種の component descriptor)
- `ArtifactCore/src/Physics/PhysicsSystem.cppm` / `ArtifactCore/src/Physics/SoftBodySolver.cppm` / `ArtifactCore/src/Physics/FractureEngine.cppm` / `ArtifactCore/include/Physics/FluidSolver2D.ixx`
- `ArtifactCore/src/NLE/NLEProjectStore.{ixx,cppm}`(共有 NLE ストア)

### 既存 ArtifactPr 関連レポート
- `docs/analysis/REPORT_ARTIFACT_PR_IMPLEMENTABILITY_2026-06-16.md`
- `docs/analysis/REPORT_ARTIFACT_PR_NLE_2026-06-16.md`(Premiere 機能ギャップ)

---

## 9. 補遺: AGENTS.md 整合性チェック

| ルール | ArtifactPr の対応 |
|---|---|
| `QImage` の新規採用は原則禁止(IO 境界・Qt API との境界のみ) | ✅ `PrVideoSurface::present` で `QVideoFrame::toImage()`(Qt API 境界)、`SequenceCanvasWidget::cachedImage_` は `frame_.toQImage()`(表示境界のみ) |
| `QPainter` / Qt の `CompositionMode` を使った新規の描画・合成実装は禁止 | ⚠️ `VideoCanvas::paintEvent` / `TimelineClipWidget::paintEvent` / `TimelineRulerWidget::paintEvent` / `AudioMeterWidget::paintEvent` 等で QPainter を使っているが、CompositionMode は使っていない(ただの drawText / drawRect / drawLine / drawPath)。合成本体は `ImageF32x4_RGBA::alphaBlend` で行っているため整合 |
| `QtCSS` / `setStyleSheet()` の新規追加は禁止 | ✅ `AppTheme`(`ArtifactPr/src/AppTheme.cppm` 157 行)で対応。`PrProxyStyle` + `QPalette` で賄う |
| `QColorDialog` の新規使用は禁止 | ✅ 独自 `PrColorPickerDialog`(`ArtifactPr/src/PrColorPickerDialog.cppm` 76 行)を使用 |
| 新規のシグナル&スロット接続は絶対禁止 | ✅ 既存経路のみで組まれている(グローバルな新規 signal なし) |
| module purview に `#include` を追加しない | ✅ `ArtifactPr/src/main.cpp:1-12` は import のみで構成、`PrStatusNotifier.cppm:8` 等の cppm も `module;` の GMF にのみ `#include` |
| PImpl の `Impl` 所有は `Impl*` を明示所有 | ✅ `ArtifactCompositionEditor` / `ArtifactDiligentEngineRenderWindow` ともに `Impl*` 所有 |
| ラインエンディング LF | ✅ 全ファイル LF 統一(2026-08-31 確認) |

→ AGENTS.md の主要ルールは概ね整合。一部の `paintEvent` での QPainter 利用は「描画(widget 自前描画)」であって「合成」ではないので、ルール違反ではない。CompositionMode 不使用が条件。