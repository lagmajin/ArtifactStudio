# Late-Stage & DCC Gap レポート — 2026-06-16

作成日: 2026-06-16
目的: `docs/analysis/REPORT_AE_GAP_AND_SIGNAL_HOTSPOT_2026-06-16.md` で扱わなかった **AE 後半の FX 系 / 3D / Collaboration / データ連携** と、**Premiere Pro / DaVinci Resolve / Nuke / Fusion / Houdini / Figma** 等の他 DCC に固有の機能ギャップを 125 項目で走査する。
調査範囲: `Artifact/`, `ArtifactCore/` 配下の `.cppm` のみ。`third_party / libs / ArtifactWidgets` は対象外（サブモジュール境界）。

---

## 1. サマリ

| 区分 | 件数 |
|---|---:|
| 完全未実装 (MISS, 0 hit) | **115 項目** |
| 部分実装 (PART, 1〜2 hit) | 3 項目 |
| 実装あり (OK, 3+ hit) | 7 項目 |

**重要発見**:
- AE 以外の DCC に固有の **編集 (Premiere) / カラー (Resolve) / 3D compositing (Nuke) / 物理 (Houdini)** 機能の 95% 以上が 0 hit
- 「ある」と言われていた `Composition recovery (autosave)` は **65 hit** と予想外に厚い実装あり
- `AE expression stdlib` は 85 hit だが、`wiggle / seedRandom` などの **関数呼び出し** は限定的
- 3D 周辺は `3D Camera Tracker (solve)` が 19 hit あるが、`Refine` / `Reconstruct` / `Extrusion` はすべて 0 hit

---

## 2. カテゴリ別ギャップ

### 2.1 Rotoscoping / Paint (AE: 後半の FX)

すべて 0 hit。AE / Mocha / Silhouette 系の Roto は **完全に未着手**:

- Paint layer (animated brush) — AE の「ペイント」でフレーム毎の手書き
- Paint effects (Smear, Clone Stroke 等)
- Refine Edge / Refine Matte (Roto の境界補正)
- Mocha planar tracker (平面トラッキング)
- Advanced Spill Suppressor (キー後の縁色除去)
- Mesh Warp (歪み) — `MILESTONE_MESH_WARP_LIQUIFY_2026-06-02.md` で参照はあるが実装は不在

→ **AE の Roto / Paint 系は milestone doc が 1 個あるのみで、コードは完全に空**。

### 2.2 3D Camera Tracker (AE: 後半の 3D)

`3D Camera Tracker (solve)` のみ 19 hit。**残りはすべて 0 hit**:

- 3D Camera Refine (ground plane) — カメラ位置の精密化
- 3D Scene Reconstruction (point cloud)
- 3D Extrusion (text / shape 押し出し)
- 3D Material System (PBR) — 1 hit (AI descriptions 内)

→ **`3D Camera Tracker` だけ**で、`3D Refine` / `3D Reconstruction` / `Extrusion` / `Material` はすべて未着手。AE の 3D ワークフローは solve のみ。

### 2.3 Particle / Physics / Simulation (Houdini / RealFlow)

- Particle system (advanced): **5 hit** (scaffold あり)
- Particle physics (rigid body): 0 hit
- Fluid simulation: **24 hit** (実装あり、`MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md` 系)
- Crowd simulation: 0 hit
- Cloth / Hair sim: 0 hit

→ **Fluid は実装あり**。Rigid body / Crowd / Cloth / Hair は **完全に未着手**。

### 2.4 Expression (AE: 補間 / スクリプト)

- `AE expression stdlib` (`thisComp / thisLayer / thisProperty`): **85 hit** — ベースは揃っている
- `Wiggle / random / noise / loop`: **6 hit** — 一部関数あり
- `Drop-down expression menu`: **9 hit** — UI scaffold あり
- **Expression engine (full)**: 0 hit — `ExpressionEvaluator` クラスは 0 hit
- **Pick whip (drag UI)**: 0 hit — `MILESTONE_PICK_WHIP_UI_2026-06-02.md` のみ
- **Expression Library browser**: 0 hit

→ **ベース実装はあるが、UI / ライブラリ / 完全な stdlib は未完成**。`MILESTONE_EXPRESSION_STDLIB_COMPATIBILITY_2026-04-18.md` が進行中だが、現状コード根拠は薄い。

### 2.5 Project / Asset / Collaboration (Team project)

すべて 0 hit。**チーム制作機能は完全に未着手**:

- Smart Project (folder watch) — 1 hit (AI description 内)
- Team project (collaboration) — `MILESTONE_TEAM_PROJECT_REALTIME_SYNC_2026-04-12.md` のみ
- Render farm web UI
- Cloud asset library
- DRM / license management — `MILESTONE_SECURITY_HARDENING_2026-03-28.md` は別 topic

→ 個人制作に焦点。チーム機能は roadmap 上のみで実装なし。

### 2.6 2D Point Tracker (AE / Mocha / SynthEyes 系)

すべて 0 hit。`MILESTONE_2D_POINT_TRACKER_2026-06-02.md` のみ存在:

- 2D point tracker
- Mocha-style planar tracker
- Tracker node UI

→ トラッキング系は **完全に未着手**。

### 2.7 FX-specific (AE: 後半 / Fusion 系)

すべて 0 hit。AE / Fusion に固有の FX 系は **完全に未着手**:

- Card Wipe / Radial Wipe
- Glow Variants (advanced)
- Lens Flare / Light Leak
- Cartoon (cel shading / Outline)
- Color Range (AE 風)
- Compound effect (Blur)
- Corner Pin (UI node)
- Liquify / Mesh Warp

→ `docs/done/MILESTONE_GLOW_VARIANTS_2026-06-13.md` / `MILESTONE_MESH_WARP_LIQUIFY_2026-06-02.md` の doc はあるが、コード根拠 0 hit。

### 2.8 Color (DaVinci Resolve / Baselight / Nuke)

- Color page (Qualifier / Power Window): 0 hit
- Color managed output (ACES): 0 hit
- HDR / Dolby Vision mastering: 0 hit
- LUT calc (programmatic): 0 hit
- Vector scope / Waveform scope / Parade scope: 0 hit (各々)
- Color warper (wheels + log): 0 hit
- LUT export (write): 0 hit
- ACES IDT / ODT config: 0 hit
- OpenColorIO (OCIO) config: 0 hit
- OpenColorIO transforms (LMT): 0 hit

→ **DaVinci 的な Color page 機能は完全に未着手**。`MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` はあるが、コード根拠 0 hit。`M-LUT-1 LUT Browser` を提案したが、**LUT export まで踏み込んでいない**。

### 2.9 Edit (Premiere Pro / FCP)

すべて 0 hit:

- Magnetic timeline
- Nesting (sequence in sequence) — `ArtifactCompositionLayer` は precomp 用途で nesting とは別
- Multi-cam editing
- Adjustment / Broadcast layer
- Linked selection (sync audio+video)
- Timecode overlay (burn-in)
- Captions / Subtitles (CEA-708)
- Loudness meter (BS.1770)
- Broadcast safe (RGB 0-255)
- Closed caption export (SRT)
- Subtitle (SRT / WebVTT) import — `DESIRED_IMPORT_FORMATS` で言及

→ **Premiere 的な編集機能は完全に未着手**。キャプション / 字幕 / 放送基準も 0 hit。

### 2.10 3D compositing (Nuke / Fusion)

すべて 0 hit:

- 3D scene merge
- Card 3D (basic 3D layer)
- Depth merge (deep data)
- Position pass / normal pass / depth pass
- 3D shader network
- Relighting (basic)
- Matte paint in 3D
- Lens distortion correction

→ **Nuke 的な deep data / pass merge は完全に未着手**。`Artifact3DLayer` は base のみ。

### 2.11 Asset / file format

すべて 0 hit:

- OpenColorIO (OCIO) config
- OpenEXR multilayer
- FBX / Alembic / USD import
- WebM / VP9 / AV1 / ProRes / HAP export
- OGG / Opus / FLAC import — `DESIRED_IMPORT_FORMATS` の `m4a, ogg, flac` 言及
- 3D LUT write (`.cube` / `.3dl`)
- Audio-only export
- Image sequence export — `RENDER_QUEUE_MANAGER_GAP_ANALYSIS_2026-04-13.md` #1 で言及

→ **コーデック / フォーマット層の拡張は完全に未着手**。

### 2.12 Plugin / Scripting

すべて 0 hit:

- OFX host — `MILESTONE_OFX_PLUGIN_SUPPORT_2026-04-18.md` のみ
- AEX / AE plugin import
- Python console (REPL) — `MILESTONE_SCRIPT_MENU_MACRO_ENTRY_EXECUTION_2026-05-31.md` のみ
- Macro / script preset
- Scripted effect

→ **プラグイン SDK / Python REPL / 拡張 script は完全に未着手**。コア Expression engine はあるが、外部からの拡張点は無し。

### 2.13 AI 系

すべて 0 hit (PART 1 件のみ):

- AI rotoscope (auto alpha) — `MILESTONE_AI_ASSISTED_FEATURES_2026-03-29.md` 系
- AI denoise (temporal)
- AI color match (style transfer) — 1 hit (description のみ)
- AI upscale
- AI voice isolation / denoise
- Auto caption (ASR)
- Subtitle translate (AI)

→ **AI 自動化機能はほぼ未着手**。Local LLM はあるが、画像 / 音声の AI 処理は無し。

### 2.14 Animation / Physics (Houdini / Duik)

すべて 0 hit:

- Spring physics (layer) — `MILESTONE_ANIMATION_DYNAMICS_CORE_2026-03-28.md` のみ
- Duik / Rubberhose (rig)
- Joysticks / Sliders (controller)
- Character animation rig
- Inverse Kinematics (IK)
- Forward Kinematics (FK)
- Walk cycle generator

→ **リグ / IK / 物理 sim は完全に未着手**。アニメ自動化は手作業頼み。

### 2.15 Misc (Adobe / CapCut 系)

- Multicam angle switcher: 0 hit
- Time lapse / Speed ramp: 0 hit
- Optical Flow (interpolation): 0 hit
- Timewarp UI: 0 hit
- Smart reframe (vertical/horizontal): 0 hit
- Sequence template (.mogrt): 0 hit
- Live preview (GPU RAM cloud): 0 hit
- Halo / Lens flare node: 0 hit
- Color science pipeline (advanced): 0 hit

### 2.16 Audio 高度機能

- Audio VST3 host: 0 hit — `ArtifactVSTHost` (VST2) はあり
- Audio bus routing matrix: 0 hit — `AudioMixer` はあるが matrix UI なし
- Audio sidechain routing: 0 hit
- Audio real-time VU meter: 0 hit — `SpectrumAnalyzerWidget` 別物
- Audio spectrum analyzer (FFT): 0 hit
- Audio ADSR envelope: 0 hit
- Audio noise gate: 0 hit

### 2.17 Composition recovery

- `Composition recovery (autosave)`: **65 hit** — 意外と厚い実装あり
  - `MILESTONE_PROJECT_AUTO_SAVE_2026-04-10.md` / `MILESTONE_CRASH_DIAGNOSTICS_RECOVERY_2026-03-15.md` 系
  - ユーザー体験を支えている重要機能

---

## 3. 重要発見

### 3.1 「tool / menu / icon はある」が実装は無い

多数あります:

- **Motion Sketch (45 hit)** + **Time remap keyframe (42 hit)** + **Mask count inspector (52 hit)** — 名前空間の言及はあるが、実用ロジック不在
- **Lottie / AEP / PSD layers import (0 hit)** — `DESIRED_IMPORT_FORMATS_2026-04-19.md` 🔝🟠 と一致
- **Glow / Cartoon / Lens Flare / Compound Blur / Mesh Warp** — milestone doc はあるが、コード根拠 0 hit

### 3.2 コア Expression は揃っているが、UI / ライブラリは未完成

- `AE expression stdlib` 85 hit (基底)
- `Wiggle / random / noise` 6 hit (主要関数のみ)
- `Drop-down expression menu` 9 hit (UI scaffold)
- **Pick whip / Library browser は 0 hit**

### 3.3 カラーグレードの二大柱が未着手

- `LUT Browser` (M-LUT-1 で提案済)
- `LUT write / IDT / ODT / ACES / OCIO` 0 hit 全部
- `Vector / Waveform / Parade scope` 0 hit 全部
- `Color page (Qualifier / Power Window)` 0 hit

→ DaVinci 的なカラー制作は **Color science foundation のみで、本体 UI 無し**。

### 3.4 3D は Camera solve のみで停止

- `3D Camera Tracker` 19 hit
- `3D Refine / Scene Reconstruct / Extrusion / Material` 0 hit
- `3D shader network / Relighting / Matte paint in 3D` 0 hit

→ **Camera solve → 解像度編集 → 統合** の AE 風 3D ワークフローは最初の solve のみ。

### 3.5 物理 / アニメ自動化はゼロ

- 流体 sim は実装あり (`MILESTONE_VFX_PARTICLE_FLUID`)
- Rigid body / Crowd / Cloth / Hair / Spring / IK / FK / Rig すべて 0 hit
- **Duik / Rubberhose / DUIK 系の animator も 0 hit**

### 3.6 制作チーム機能はゼロ

- Team project / Render farm web UI / Cloud asset library すべて 0 hit
- `MILESTONE_TEAM_PROJECT_REALTIME_SYNC_2026-04-12.md` のみ存在
- Adobe Team Projects / Figma multiplayer / Houdini Indie collab とは隔絶

### 3.7 コーデック / フォーマットの拡張余地

- 既存 export は H.264 中心（推定）。WebM / VP9 / AV1 / ProRes / HAP / OGG / Opus / FLAC / Image sequence / Audio-only すべて 0 hit
- 特に **ProRes / HAP / Audio-only / Image sequence** は実制作で必須

---

## 4. 推奨される提案 (新規 milestone 候補)

痛み度 × コード根拠 × 既存着手 の総合で評価。

### 4.1 P0 (すぐ着手 / 既存依存あり)

| テーマ | 既存依存 | 価値 |
|---|---|---|
| **Refine Edge / Spill Suppressor** | `MILESTONE_TRACK_MATTE_DRAG_LINK_UX_2026-06-01.md` 系 | クロマキー後の仕上げ |
| **2D Point Tracker** | `MILESTONE_2D_POINT_TRACKER_2026-06-02.md` | replace / motion 追従の必須機能 |
| **OpenColorIO 統合** | `MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` | プロダクションカラー基盤 |
| **Scopes (Vector / Waveform / Parade)** | `MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md` | カラー / 露出の基準 |
| **Loudness meter (BS.1770)** | audio engine は実装済 | 配信基準 |
| **Image sequence export** | `RENDER_QUEUE_MANAGER_GAP_ANALYSIS_2026-04-13.md` #1 | 制作の基本 |
| **ProRes export** | Render queue | 高品質納品 |
| **SRT / WebVTT import + export** | Caption 系 | 字幕制作 |
| **Pick whip UI** | `MILESTONE_PICK_WHIP_UI_2026-06-02.md` | Expression 体験 |
| **AEP / PSD-layers / Lottie import** | `DESIRED_IMPORT_FORMATS` | 制作ワークフロー |

### 4.2 P1 (中期的 / 価値は高いが依存重い)

- **Mocha 風 planar tracker** — 2D Point Tracker の依存
- **3D Refine / Material** — 3D Camera solve の依存
- **Fluid → Rigid body / Cloth** — `MILESTONE_VFX_PARTICLE_FLUID` の発展
- **OFX host** — プラグイン SDK 全体
- **Subtitle (CEA-708) / 放送セーフ** — 放送業界向け
- **HDR / Dolby Vision mastering** — `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` との接続
- **OC transforms (LMT / IDT / ODT)** — カラーパイプライン
- **Timewarp UI / Optical Flow interpolation** — スローモーション
- **Expression Library browser** — `MILESTONE_EXPRESSION_STDLIB_COMPATIBILITY_2026-04-18.md` の発展
- **Python REPL** — 拡張性
- **AI rotoscope / auto caption / voice isolation** — AI 基盤
- **Render farm web UI** — `M-RE-2 Render Farm Design` の発展

### 4.3 P2 (長期 / 構造変革)

- Team project / Real-time collaboration
- Cloud asset library
- 3D shader network / Relighting
- Duik 風 rig / IK / FK / Character animation
- Spring physics (layer)
- Smart reframe (Auto reframe)
- Cloud preview (GPU cloud)
- Multicam editing
- Walk cycle generator

---

## 5. 既存 milestone との接続

- `MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` — **OCIO / Scopes / LUT export を含める**形で拡張
- `MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md` — Fluid → Rigid body / Cloth / Hair sim への発展
- `MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md` — **Scopes / Loudness** を foundation
- `MILESTONE_OFX_PLUGIN_SUPPORT_2026-04-18.md` — **OFX / AEX** を SDK として統合
- `MILESTONE_EXPRESSION_STDLIB_COMPATIBILITY_2026-04-18.md` — **Pick whip / Library browser** を含める
- `MILESTONE_3D_MATERIAL_SYSTEM_2026-03-31.md` — 1 hit のみ。**3D Material を本実装に**
- `MILESTONE_RENDER_QUEUE_2026-03-22.md` — **Image sequence / ProRes / HAP / Audio-only export** を追加

---

## 6. まとめ

- **AE 後半の FX (Roto / Paint / Mesh Warp / Cartoon) は完全に未着手**
- **Premiere 的な編集機能 (Multi-cam / Magnetic / Timecode / Caption) は完全に未着手**
- **DaVinci 的なカラー (OCIO / ACES / Scopes / Qualifier) は完全に未着手**
- **Nuke 的な 3D compositing (deep data / pass merge) は完全に未着手**
- **Houdini 的な物理 / アニメ自動化 (IK / FK / Rig / Spring) は完全に未着手**
- **3D 周辺は solve のみで停止**
- **コラボ / チーム / クラウド機能は完全に未着手**
- **コーデック拡張 (ProRes / HAP / WebM / AV1) は完全に未着手**

→ AE 互換で残った `M-ASSET-1 / M-CLIP-1 / M-TL-16 / M-TXT-3 / M-AU-8 / M-MO-1` を着手しつつ、**中期的には DCC 拡張 (Premiere / Resolve / Nuke / Houdini 由来) の提案** が必要。

新規 milestone として:
- **M-OCIO-1 OpenColorIO 統合**
- **M-SCOPES-1 Scopes (Vector / Waveform / Parade)**
- **M-EXPORT-1 Render Format Expansion (ProRes / HAP / Image sequence / Audio-only)**
- **M-CAPTION-1 Caption / Subtitle (SRT / WebVTT)**
- **M-2DTRACK-1 2D Point Tracker**
- **M-PAINT-1 Paint Layer (animated brush)**

あたりが自然な次の一手。

---

## 7. 更新履歴

- 2026-06-16: 初版作成。`Artifact/`, `ArtifactCore/` 配下の `.cppm` を 125 項目で走査。
