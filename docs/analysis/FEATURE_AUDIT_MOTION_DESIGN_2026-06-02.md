# Motion Design Feature Audit — 2026-06-02

対象: `Artifact/`, `ArtifactCore/` （third_party, libs 含まず）
手法: grep + 該当ファイル読み込み
プロンプト元: モーションデザイナー視点 30 项目的な仕分け

---

## サマリ

- **IMPLEMENTED: 5件**
- **PARTIAL: 14件**
- **NOT FOUND: 11件**

---

## 完全実装済み (IMPLEMENTED)

| # | 機能 | エビデンス |
|---|---|---|
| 7 | Pre-compose | `Artifact/src/Service/ArtifactProjectService.cpp:1298` `precomposeLayersInCurrentComposition`, `Artifact/src/Widgets/Dialog/PrecomposeDialog.cppm`, `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm:877` |
| 8 | Adjustment Layer | `Artifact/include/Layer/ArtifactAdjustableLayer.ixx`, `Artifact/src/Layer/ArtifactAdjustableLayer.cppm`, `Artifact/src/Test/ArtifactTestAdjustmentLayer.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:6287` |
| 13 | Time Remap | `ArtifactCore/src/Time/TimeRemap.cppm`, `Artifact/include/Layer/ArtifactAbstractLayer.ixx:216-219`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm:1054-1106`, menu `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm:404` |
| 19 | Audio Waveform on Timeline | `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp:1528-1569` `waveformPeaks`/`waveformRms` drawing, `Artifact/src/Widgets/ArtifactTimelineWidget.cpp:270` `buildAudioWaveformForLayer` |
| 21 | VU Meter / Spectrum Analyzer | `Artifact/include/Widgets/SpectrumAnalyzerWidget.ixx`, `Artifact/src/Widgets/SpectrumAnalyzerWidget.cppm`, `Artifact/src/Widgets/AudioMixerWidget.cppm:122` |
| 25 | Proxy/Draft/Full Toggle in Preview UI | `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm:81,193-240` (Draft toggle), `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:3889` (Fast Draft), `Artifact/src/Layer/ArtifactVideoLayer.ixx:74-75` (`ProxyQuality` enum) |

**備考:** 以上の 5 件は UI + データモデル + レンダリング導線の全てが繋がっている。

---

## 部分実装 (PARTIAL) — 14件

| # | 機能 | 現状 | 欠落 |
|---|---|---|---|
| 1 | Easy Ease (F9) | `Artifact/src/Widgets/ArtifactTimelineWidget.cpp:2433-2438` F9 で `InterpolationType::EaseInOut` 等を設定するショートカットあり | 現在は静的なイージング投入のみ。AE 式の「速度ベース自動イージング」(slow-in/slow-out を選択キーフレームの速度から自動算定) は未実装 |
| 2 | Speed Graph / Value Graph toggle | `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx:66` `toggleValueGraphRequested` declaration あり | curve editor (`Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm`) での速度グラフ描画は宣言コメントのみ `:815-817`。UI 切替が存在するか未確認 |
| 5 | Motion Path display | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:960-1310` build/hittest, `:2246` `showMotionPathOverlay_` flag あり | render 本体は `:6650-6674` でコメントアウト / stub。overlay toggle のみ |
| 6 | Parent Pick Whip | parenting データモデルあり (`Artifact/src/Layer/ArtifactAbstractLayer.cppm:720-1041`), menu は "Select Parent" (`Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm:303`) | drag-to-parent の Inspector/Timeline 操作クラス・ツール未発見 |
| 10 | Layer Styles | `Artifact/include/Effects/Rasterizer/DropShadowEffect.ixx` のみ単体存在 | Bevel / InnerShadow / Stroke / Satin 等のレイヤースタイル一式は未検出。シェイプの stroke とは別。Inspector 着地点未確認 |
| 11 | Puppet Tool | toolbar アイコン (`Artifact/src/Widgets/ArtifactToolBar.cppm:45`) のみ | mesh deformer / pin / warp 実装はなし |
| 12 | Auto-Orient | — | 一致するコードなし。time-remap / motion path とも分離未設計 |
| 14 | Motion Blur | `Artifact/include/Effect/ArtifactMotionBlur.ixx` (`MotionBlurEffect`, `setShutterAngle`), `Artifact/src/Effect/ArtifactMotionBlur.cppm:323` global switch は `Artifact/src/Widgets/ArtifactTimelineGlobalSwitches.cppm:92` | shutter angle/samples を Inspect from timeline する高レベル UX 未確認 |
| 15 | Echo/Afterimage | — | 一致する effect class なし。単発 frame history の概念は render cache に存在するが effect としては未実装 |
| 16 | Text Animator | `ArtifactCore/include/Text/TextAnimator.ixx` + `TextAnimator.cppm` でエンジンあり | timeline トラック UI と Inspector 入力導線が部分的または未接続 (`docs/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md` が ongoing) |
| 17 | Range Selector + Wiggly | `ArtifactCore/include/Text/TextAnimator.ixx:54-65` (`RangeSelector`, `WigglySelector`) 構造体あり | UI（Inspector/Timeline）から操作する導線は未検出 |
| 20 | Audio Scrubbing | — | scrub 時のリアルタイム音声 preview は見当たらず、playbackEngine への導線のみ存在 |
| 22 | Color Profile Embed | `ArtifactCore/src/AI/DataAssetDescriptions.cppm:132` に `colorProfile` field が1つある | ICC 読み書き / 埋め込み export ロジックは見当たらない |
| 23 | LUT Browser/Picker | `Artifact/src/Color/ArtifactColorScienceManager.cppm:144-150` でディレクトリ走査、`Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm:34,159-212` で LUT load/clear あり | ブラウザ UI (.cube/.3dl 一覧＋プレビュー) としては部分 |
| 24 | Scopes | ParadeScope (`Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp:968`), Vectorscope (`Artifact/src/Render/ArtifactHDRMonitor.cppm:153-190`), Waveform は未確認 | 全体の Scope パネルは部分的 |
| 26 | ROI | `Artifact/include/Render/ArtifactRenderContext.ixx:104-154`, `Artifact/include/Render/ArtifactRenderROI.ixx` struct あり | debug draw コメントアウト。UI からの ROI 設定導線は未確認 |
| 27 | RAM Preview queue | `Artifact/src/Service/ArtifactPlaybackService.cppm:488` reason ログ存在、milestone doc `MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md` あり | スタンドアロン queue UI は見当たらない |
| 28 | Render Farm | `ArtifactCore/CMakeLists.txt:16`, `ArtifactCore/src/Network/NetworkRPCServer.cppm` で low-level network 要素あり | render farm orchestration (master/slave スケジューラ) 未実装 |
| 29 | Render Queue auto-restart | `ArtifactCore/src/Render/RendererQueueManager.cppm` あり | checkpoint / retry ロジック未発見 |
| 30 | A/B / Wipe | `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp:1701` A/B chip あり、wipe はドキュメント記載のみ | wipe ビューア未実装 |

---

## 未実装 (NOT FOUND) — 11件

| # | 機能 | 所見 |
|---|---|---|
| 3 | Roving Keyframes | 一致コードなし。keyframe の time-addressable な continuous 化は現在の model でも未導入 |
| 4 | Motion Sketch | 一致コードなし。mouse path → keyframe batch 生成がない |
| 9 | Track Matte Drag UX | ドラッグUI/データモデルコード未発見。`Artifact/include/Mask/LayerMask.ixx` の mask はあくまでマスク。track matte 参照自体も未確認 |
| 10b | Puppet Tool | toolbar に placeholder のみ。mesh/pin/bone コア無し |
| 12 | Auto-Orient | 0 hit。path に沿う向き自動補正がない |
| 15 | Echo/Afterimage | effect クラス無し |
| 18 | Source Text keyframe | `setSourceText` 等無し。テキスト編集はインライン編集のみ |
| 20 | Audio Scrubbing | 実装無し |
| 22 | ICC embed | embed ロジック無し |
| 28b | Render Farm orchestration | RPC 低レイヤーのみ |
| 29 | Queue checkpoint retry | 未実装 |

**注意:** #9 Track Matte は `LayerMask` はあっても track matte 参照 (`LayerMatteReference`) 本体が未発見だったため、データモデル面も含めて NOT FOUND とした。

---

## 補足

- F9 ショートカットはあるが速度ベースイージングではない → PARTIAL
- Value Graph toggle 宣言はあるが curve editor 本体未接続 → PARTIAL  
- Text Animator engine は完成に近いが timeline/property UI が未整備 → PARTIAL
- Audio Waveform は track painter に描画コードが存在する点が最も進んでいる

---

## 推奨優先度（モーションデザイナー視点）

P0: #3 Roving Keyframes / #4 Motion Sketch / #12 Auto-Orient / #18 Source Text / #9 Track Matte Drag / #20 Audio Scrubbing
P1: #1 Easy Ease 深化 / #2 Speed Graph UI / #10 Layer Styles / #14 Motion Blur UI / #15 Echo / #26 ROI UX
P2: #22 ICC / #23 LUT Browser / #24 Scopes / #27 RAM Preview Queue / #28/29 Render Queue / #30 Wipe
