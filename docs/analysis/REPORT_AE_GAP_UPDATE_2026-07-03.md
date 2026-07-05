# AE ギャップ最新実装状況レポート — 2026-07-03

**作成日:** 2026-07-03  
**目的:** 2026-05-28〜06-16 時点の各 AE ギャップ分析文書に対し、その後の実装進捗を反映した最新スナップショットを提供する。  
**参照元:**
- `docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md`
- `docs/analysis/REPORT_AE_GAP_AND_SIGNAL_HOTSPOT_2026-06-16.md`
- `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md`
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md`
- `docs/analysis/WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md`
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md`
- `docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md`
- `docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md`
- `docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md`

---

## 1. エグゼクティブサマリ

2026-05-28〜06-16 時点で指摘された主要ギャップのうち、**約 30% が完了または大幅に前進した**。特に Layer Styles（InnerShadow/Stroke/Satin 完了）、OFX Plugin System（実装開始）、Keyframe Copy & Paste（完了）、Interpret Footage（完了）が大きな進展。

一方、**P0 の Preview/Cache 安定性、Track Matte UX、Graph Editor、Text Animator UX** は依然として未完了だが、周辺基盤（Viewport 性能、Multi-frame preview、Playback stability）は継続的に改善されている。

---

## 2. 実装進捗サマリ (対 2026-05-28 Gap)

凡例: ✅ 完了 / 🔄 進行中 / 🟡 部分的 / ❌ 未着手 / 🆕 新規着手

| # | 領域 | 2026-05-28 状態 | 2026-07-03 状態 | 主な進展 |
|---|------|-----------------|-----------------|---------|
| P0-1 | Preview/Cache/Playback 安定性 | 🟡 土台あり・状態契約未整備 | 🔄 継続改善中 | Multi-frame preview, Viewport 性能改善, VideoLayer 再生安定性 ✅ |
| P0-2 | Track Matte / Alpha / Blend | 🟡 評価経路あり・正確性不足 | 🔄 継続中 | Blend Mode Catalog 追加 ✅, color blend modes 拡充 ✅, Track Matte drag UX は ❌ |
| P1-1 | Keyframe/Graph Editor | 🟡 選択可視化済・Speed Graph 未接続 | 🔄 一部完了 | Keyframe Copy & Paste ✅, sampleSpeedGraph() 実装済み ✅ |
| P1-2 | Text Animator UX | 🟡 Engine 完成・UI 未接続 | 🔄 一部完了 | Inline Edit Phase 1 ✅ |
| P1-3 | Motion Blur | 🟡 実装あり・UI 配線弱い | 🟡 未変化 | 変化なし |
| P1-4 | Adjustment Layer | 🟡 スタブ段階 | 🟡 未変化 | 変化なし |
| P1-5 | Parent/Transform 伝播 | 🟡 データ構造あり・不完全 | 🔄 進行中 | Parent Pick-Whip 設計完了 🆕 |
| P2-1 | Marker System | ❌ 未着手 | ❌ 未着手 | 変化なし |
| P2-2 | Shape Operators | 🟡 一部実装 | 🔄 進行中 | Shape modeling edit mode ✅, path vertex insertion ✅ |
| P2-3 | Precompose Workflow | 🟡 呼出経路あり・unprecompose 未完成 | 🟡 未変化 | 変化なし |
| P2-4 | Layer Styles | 🟡 DropShadow のみ | ✅ 大幅進展 | InnerShadow/Stroke/Satin ✅ (2026-07-02) |
| P2-5 | Time Remap / Frame Blend | 🟡 基礎あり | 🟡 未変化 | 変化なし |
| P2-6 | Expression 互換 | 🟡 Parser/Evaluator あり・stdlib 不足 | 🔄 進行中 | Script runtime support ✅ |
| P3-1 | Effects 数拡充 | 🟡 基本セットあり | 🔄 進行中 | Auto color grading ✅, Color grading compute ✅, Effect level mask 設計 🆕 |
| P3-2 | OCIO / ACES | ❌ 未着手 | ❌ 未着手 | 変化なし |
| P3-3 | 3D Camera Tracker | ❌ 未着手 | 🟡 着手 | Point Tracker (initial core) ✅ (ArtifactCore) |
| P3-4 | Plugin SDK / AEX | 🟡 OFX スタブ | 🆕 実装進行中 | PluginLoader/Sandbox/Registry ✅, OFX render pipeline ✅ |
| P3-5 | Mogrt-like Templates | ❌ 未着手 | ❌ 未着手 | 変化なし |
| P3-6 | Python API | 🟡 部分的 | 🔄 進行中 | Script runtime 拡張 ✅ |
| - | Numeric Field Quick Calc | ✅ 完了 | ✅ 完了 | 完了確認済 |
| - | Interpret Footage | ❌ 未着手 | ✅ 完了 | Dialog + context menu + service ✅ |
| - | Roving Keyframes | ❌ | ❌ 未着手 | 変化なし |
| - | Motion Sketch | ❌ | ❌ 未着手 | 変化なし |
| - | Auto-Orient | ❌ | ❌ 未着手 | 変化なし |
| - | Audio Scrubbing | ❌ | ❌ 未着手 | 変化なし |
| - | Source Text Keyframe | ❌ | ❌ 未着手 | 変化なし |
| - | Roto/Paint | ❌ | ❌ 未着手 | 変化なし |
| - | 2D Point Tracker | ❌ | 🟡 着手 | Initial core in ArtifactCore ✅ |
| - | ICC Profile Embed | ❌ | ❌ 未着手 | 変化なし |
| - | LUT Browser | 🟡 部分的 | 🟡 未変化 | 変化なし |
| - | Scopes (Waveform/Vector/Parade) | 🟡 部分的 | 🟡 未変化 | 変化なし |
| - | Render Farm | 🟡 RPC 低レイヤーのみ | 🔄 進行中 | Coordination/checkpointing ✅ |
| - | A/B Wipe viewer | ❌ | ❌ 未着手 | 変化なし |
| - | Asset Instance Sharing | ❌ | ❌ 未着手 | 変化なし |

---

## 3. 重要進展ハイライト

### 3.1 ✅ Layer Styles: InnerShadow / Stroke / Satin 完了 (2026-07-02)

全て Rasterizer pipeline stage に CPU/GPU 両パスで実装。DropShadow (既存) と合わせて AE 標準 Layer Style のコアセット完成。

**残:** Color Overlay, Gradient Overlay, Pattern Overlay, Contour curve editor UI

### 3.2 🆕 OFX Plugin System 実装開始

ArtifactCore 側で以下が完了:
- PluginRegistry / PluginLoader / PluginSandbox / PluginCommon
- ILayerPlugin interface
- OFX render pipeline: clipGetImage, clipGetRod, instance lifecycle, apply()
- CLI: --help, --version, --plugin-list, --plugin-info

**残:** Plugin manager UI, サードパーティ plugin 実証, AEX compatibility

### 3.3 ✅ Interpret Footage 完了

- SourceInterpret 型 (InterpretImpactReport, FrameRatePreserveMode, SourceInterpretOverride)
- Interpret Footage dialog + context menu entry + service

### 3.4 ✅ Keyframe Copy & Paste 完了

ClipboardManager keyframe support + timeline context menu + animation menu entry 全て完備。

### 3.5 🆕 Parent Pick-Whip 設計完了 (2026-07-03)

- Draft milestone 作成済み
- 4 Phase で実装計画: Layer panel affordance -> Drag preview -> Undo/validation -> Inspector entry

### 3.6 🆕 Point Tracker (initial core) 追加

ArtifactCore 側に motiontracker_ncc_tracking として初期コア追加。UI は未着手。

### 3.7 ✅ Viewport / Preview 性能改善 (複数コミット)

- Interactive preview / surface cache
- Fast draft skip expensive passes
- Cached viewport project preflight
- Sampled viewport render diagnostics
- Composition multipass execution
- Multi-frame preview rendering (ArtifactCore)

---

## 4. 未着手の重要ギャップ (優先順位付き)

### P0 (最優先 / 制作破綻リスク)

| # | 機能 | 現状 | 備考 |
|---|------|------|------|
| 1 | **RAM Preview / Cache 安定性** | 🟡 状態契約未整備 | requested/ready/failed 統一が最大課題 |
| 2 | **Track Matte Drag UX** | ❌ | データモデルあり (16 hit) + drag link 0 hit |
| 3 | **Graph Editor / Speed Graph wiring** | 🟡 sampleSpeedGraph() 実装済み・UI 未接続 | B-1 優先度低だが着手可能 |
| 4 | **Text Animator UX (timeline/Inspector)** | 🟡 Engine 完成 | Range Selector/Wiggly UI 不在 |
| 5 | **Audio Scrubbing** | ❌ | Audio engine 基盤あり・実装なし |
| 6 | **Source Text Keyframe** | ❌ | Text layer inline edit Phase 1 完了が土台 |

### P1 (高優先度 / 制作体験)

| # | 機能 | 現状 | 備考 |
|---|------|------|------|
| 7 | **Precompose 完成 (unprecompose)** | 🟡 呼出あり・内部未完成 | Core 側未完成 |
| 8 | **Parent Pick-Whip 実装** | 🆕 設計完了 | Phase 1 着手待ち |
| 9 | **Auto-Orient** | ❌ | Motion path 依存 |
| 10 | **Motion Blur UI** | 🟡 実装あり・UX 弱い | Shutter angle/samples UI |
| 11 | **Effect Level Mask** | 🆕 設計完了 | AE の「エフェクト個別マスク」 |
| 12 | **Asset Instance Sharing** | ❌ | メモリ効率の根本問題 |
| 13 | **Shape Operators (Trim Path, Repeater, Boolean)** | 🟡 一部実装 | Shape editing mode ✅ は完了 |

### P2 (標準プロ機能)

| # | 機能 | 現状 | 備考 |
|---|------|------|------|
| 14 | **Marker System** | ❌ | コンポ/レイヤーとも未着手 |
| 15 | **Roving Keyframes** | ❌ | Keyframe model 変更要 |
| 16 | **Motion Sketch** | ❌ | 45 hit 名前言及のみ |
| 17 | **Expression stdlib (wiggle, loopIn, thisComp...)** | 🟡 Script runtime 拡張中 | まだ AE 互換不足 |
| 18 | **Pick Whip (expression linking)** | ❌ | 設計文書のみ |
| 19 | **OCIO / ACES** | ❌ | 基盤あり・production pipeline なし |

### P3 (長期課題)

| # | 機能 | 現状 |
|---|------|------|
| 20 | **Roto / Paint (animated brush)** | ❌ 完全未着手 |
| 21 | **3D Material System** | ❌ 未着手 |
| 22 | **Mocha 風 planar tracker** | ❌ (Point tracker initial core ✅) |
| 23 | **LUT Browser** | 🟡 部分的 |
| 24 | **Scopes Panel** | 🟡 部分的 |
| 25 | **ICC Profile Embed** | ❌ 未着手 |
| 26 | **A/B Wipe viewer** | ❌ 未着手 |
| 27 | **Render Farm orchestration** | 🟡 RPC / diagnostics / checkpoint 進行中 |
| 28 | **.mogrt template system** | ❌ 未着手 |
| 29 | **AEP / PSD-layers / Lottie import** | ❌ 0 hit |
| 30 | **Multi-cam editing** | ❌ 未着手 |
| 31 | **Subtitle (SRT/WebVTT)** | ❌ 未着手 |

---

## 5. 最近完了したマイルストーン一覧 (2026-05-28〜2026-07-03)

| 完了日 | マイルストーン | 参照 |
|--------|--------------|------|
| 2026-06-02 | Render Preflight ✅ | docs/done/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md |
| 2026-06-16 | Keyframe Copy & Paste ✅ | docs/done/MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md |
| 2026-06-22 | Timeline Selection Sets ✅ | docs/done/MILESTONE_TIMELINE_SELECTION_SETS_2026-06-22.md |
| 2026-06-25 | Text Layer Inline Edit Phase 1 ✅ | docs/done/MILESTONE_TEXT_LAYER_INLINE_EDIT_PHASE1_2026-06-25.md |
| 2026-06-25 | VideoLayer Playback Stability ✅ | docs/done/MILESTONE_VIDEO_LAYER_PLAYBACK_STABILITY_2026-06-25.md |
| 2026-06-25 | External Renderer Design Phase 1 ✅ | docs/done/MILESTONE_EXTERNAL_RENDERER_DESIGN_PHASE1_2026-06-25.md |
| 2026-06-25 | Project Memo Panel ✅ | docs/done/MILESTONE_PROJECT_MEMO_PANEL_2026-06-25.md |
| 2026-06-27 | Solid Color Effects ✅ | docs/done/MILESTONE_SOLID_COLOR_EFFECTS_2026-06-27.md |
| 2026-07-02 | Layer Styles (InnerShadow/Stroke/Satin) ✅ | docs/done/MILESTONE_LAYER_STYLES_INNERSHADOW_STROKE_SATIN_2026-07-02.md |
| 2026-07-03 | Interpret Footage ✅ | Artifact + ArtifactCore commits |
| 2026-07-03 | OFX Plugin System (core) ✅ | ArtifactCore commits |
| 2026-07-03 | Parent Pick-Whip Draft 🆕 | docs/planned/MILESTONE_PARENT_PICK_WHIP_2026-07-03.md |
| 2026-07-03 | Effect Level Mask Draft 🆕 | docs/planned/FEATURE_EFFECT_LEVEL_MASK_2026-07-02.md |

---

## 6. 推奨着手順 (2026-07-03 版)

### 短期 (0-2 週間)

1. **Parent Pick-Whip Phase 1** -- 設計完了・layer panel affordance から着手可能
2. **Effect Level Mask Phase 1** -- 設計完了・AbstractEffect へのマスク参照追加
3. **Keyframe Copy & Paste wiring 確認** -- 完了確認・残りの UI 導線精査
4. **Precompose unprecompose() 完成** -- Core 側の未完成を完了

### 中期 (2-4 週間)

5. **Track Matte Drag UX** -- データモデルあり、drag link UI のみ
6. **Graph Editor / Speed Graph wiring** -- sampleSpeedGraph() 実装済・UI 接続のみ
7. **Asset Instance Sharing Phase 1** -- Core foundation
8. **Marker System Phase 1** -- コンポ/レイヤーマーカー

### 長期 (4-8 週間)

9. **Expression stdlib AE 互換拡張** -- wiggle/loopIn/thisComp
10. **Audio Scrubbing** -- Audio engine 基盤上に実装
11. **Auto-Orient** -- Motion path 依存解決後
12. **OCIO / ACES production pipeline**
13. **Shape Operators 拡充 (Trim Path, Repeater, Boolean)**

---

## 7. 更新履歴

- 2026-07-03: 初版作成。2026-05-28〜06-16 時点の全ギャップ文書に対し、その後の Artifact/ArtifactCore コミットと完了マイルストーンを反映。
