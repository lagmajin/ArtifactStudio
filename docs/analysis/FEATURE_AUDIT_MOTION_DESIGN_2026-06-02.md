# Motion Design Feature Audit — 2026-06-02

対象: `Artifact/`, `ArtifactCore/` （third_party, libs 含まず）
手法: grep + 該当ファイル読み込み
プロンプト元: モーションデザイナー視点 30 项目的な仕分け

---

## サマリ

- **IMPLEMENTED: 8件**
- **PARTIAL: 16件**
- **NOT FOUND: 6件**

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
| 1 | Easy Ease (F9) | F9 の補間設定で前後キーフレームの velocity から Bezier ハンドルを自動算出 | 実機受入れ確認が残る |
| 2 | Speed Graph / Value Graph toggle | Curve Editor の Value/Speed 切替、メニュー、ショートカット、ハンドル編集を確認 | 実機受入れ確認が残る |
| 5 | Motion Path display | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:960-1310` build/hittest, `:2246` `showMotionPathOverlay_` flag あり | render 本体は `:6650-6674` でコメントアウト / stub。overlay toggle のみ |
| 4 | Motion Sketch | `ArtifactMotionSketchTool` のツール選択、キャンバス収集、平滑化、位置キー生成、Undo を確認 | 実機受入れ確認が残る |
| 12 | Auto-Orient | `AnimatableTransform3D` の AlongPath / AlongPathAtFrameStart 評価、Property UI、JSON 保存復元を確認 | 実機受入れ確認が残る |
| 6 | Parent Pick Whip | parenting データモデルあり (`Artifact/src/Layer/ArtifactAbstractLayer.cppm:720-1041`), menu は "Select Parent" (`Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm:303`) | drag-to-parent の Inspector/Timeline 操作クラス・ツール未発見 |
| 10 | Layer Styles | Drop Shadow / Bevel / Inner Shadow / Stroke / Satin の各 rasterizer effect と EffectService 登録を確認 | 実機受入れと一括 Layer Styles プリセット UX が残る |
| 11 | Puppet Tool | toolbar アイコン (`Artifact/src/Widgets/ArtifactToolBar.cppm:45`) のみ | mesh deformer / pin / warp 実装はなし |
| 14 | Motion Blur | `Artifact/include/Effect/ArtifactMotionBlur.ixx` (`MotionBlurEffect`, `setShutterAngle`), `Artifact/src/Effect/ArtifactMotionBlur.cppm:323` global switch は `Artifact/src/Widgets/ArtifactTimelineGlobalSwitches.cppm:92` | shutter angle/samples を Inspect from timeline する高レベル UX 未確認 |
| 15 | Echo/Afterimage | `EchoEffect` が実装済み | GPU 経路と実機受入れ確認が残る |
| 16 | Text Animator | `ArtifactCore/include/Text/TextAnimator.ixx` + `TextAnimator.cppm` でエンジンあり | timeline トラック UI と Inspector 入力導線が部分的または未接続 (`docs/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md` が ongoing) |
| 17 | Range Selector + Wiggly | `ArtifactCore/include/Text/TextAnimator.ixx:54-65` (`RangeSelector`, `WigglySelector`) 構造体あり | UI（Inspector/Timeline）から操作する導線は未検出 |
| 20 | Audio Scrubbing | `Artifact/include/Audio/ArtifactAudioScrubController.ixx`, `Artifact/src/Audio/ArtifactAudioScrubController.cppm` | 設定・低遅延再生・音量制御・出力デバイス診断まで実装済み。実機受入れのみ未確認 |
| 22 | Color Profile Embed | `ArtifactCore/src/AI/DataAssetDescriptions.cppm:132` に `colorProfile` field が1つある | ICC 読み書き / 埋め込み export ロジックは見当たらない |
| 23 | LUT Browser/Picker | `Artifact/src/Color/ArtifactColorScienceManager.cppm:144-150` でディレクトリ走査、`Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm:34,159-212` で LUT load/clear あり | ブラウザ UI (.cube/.3dl 一覧＋プレビュー) としては部分 |
| 24 | Scopes | ParadeScope (`Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp:968`), Vectorscope (`Artifact/src/Render/ArtifactHDRMonitor.cppm:153-190`), Waveform は未確認 | 全体の Scope パネルは部分的 |
| 26 | ROI | `Artifact/include/Render/ArtifactRenderContext.ixx:104-154`, `Artifact/include/Render/ArtifactRenderROI.ixx` struct あり | debug draw コメントアウト。UI からの ROI 設定導線は未確認 |
| 27 | RAM Preview queue | `Artifact/src/Service/ArtifactPlaybackService.cppm:488` reason ログ存在、milestone doc `MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md` あり | スタンドアロン queue UI は見当たらない |
| 28 | Render Farm | `FarmWorkerMain.cppm` の外部レンダラー実行、リトライ、出力検証、`RenderFarmMaster.cppm` のジョブ管理が実装済み | 実機の分散受入れ確認が残る |
| 29 | Render Queue auto-restart | `detectFailedFrames()`、`rerenderFailedFrames()`、`rerenderAllDetectedFailedFrames()`、`resetJobForRerun()`、キュー JSON 永続化が実装済み | UI からの自動再投入ボタンと実機受入れ確認が残る |
| 30 | A/B / Wipe | `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm` に A/B source routing、Wipe/Split/Difference、ドラッグ式 wipe handle、slider、状態保存を実装済み | 実機受入れ確認が残る |

---

## 未実装 (NOT FOUND) — 11件

| # | 機能 | 所見 |
|---|---|---|
| 3 | Roving Keyframes | `KeyFrame::roving`、Timeline の Roving menu、Undo、保存/復元が実装済み。連続時間の自動再配置は追加検証課題 |
| 9 | Track Matte Drag UX | `LayerMatteReference`、描画適用、Undo、タイムライン Alt-drag pick-whip、循環参照拒否まで実装済み。残りは操作発見性・編集体験の改善 |
| 10b | Puppet Tool | `ArtifactPuppetTool` と `OpenCVPuppetEngine`、キャンバス上のピン操作、Undo、削除・回転・weight/depth 調整まで実装済み |
| 15 | Echo/Afterimage | `EchoEffect` と temporal sampler 経路が実装済み。実機受入れ確認が残る |
| 18 | Source Text keyframe | `ArtifactTextLayer::setSourceTextAtFrame()`、保存/復元、Timeline/Property UI 導線が実装済み |
| 20 | Audio Scrubbing | 実装済み。`ArtifactAudioScrubController` の実機受入れ確認が残る |
| 22 | ICC embed | `ImageExportOptions` の ICC バイナリ／ファイル指定と、OIIO の通常・ImageBuf・マルチチャンネル出力への `ICCProfile` 属性埋め込みを実装済み |
| 28b | Render Farm orchestration | `RenderFarmMaster` / `FarmWorkerMain` に実装済み。実機受入れ確認が残る |
| 29 | Queue checkpoint retry | 失敗フレーム検出・指定／全件再レンダー・ジョブ再実行・キュー永続化が実装済み。UI 導線と実機受入れ確認が残る |

**注意:** #9 Track Matte は旧監査時点では未発見としたが、現行ソース再確認で `ArtifactLayerMatte.ixx` のデータモデルとタイムラインの Alt-drag link 実装を確認した。したがって NOT FOUND ではなく、実装済み（UX改善余地あり）へ訂正する。

---

## 補足

- F9 ショートカットはあるが速度ベースイージングではない → PARTIAL
- Value Graph toggle 宣言はあるが curve editor 本体未接続 → PARTIAL  
- Text Animator engine は完成に近いが timeline/property UI が未整備 → PARTIAL
- Audio Waveform は track painter に描画コードが存在する点が最も進んでいる

---

## 推奨優先度（モーションデザイナー視点）

P0: #4 Motion Sketch UI / #12 Auto-Orient UI
P1: #1 Easy Ease 深化 / #2 Speed Graph UI / #10 Layer Styles / #14 Motion Blur UI / #15 Echo / #26 ROI UX
P2: #22 ICC / #23 LUT Browser / #24 Scopes / #27 RAM Preview Queue / #28/29 Render Queue / #30 Wipe
