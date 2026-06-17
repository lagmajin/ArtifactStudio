# ArtifactPr 実装可能性レポート — 2026-06-16

作成日: 2026-06-16
目的: `ArtifactPr` サブモジュール (Premiere-like α プロトタイプ) に対して、ソースコード読み込み + 違反事項スキャンで **何ができて何ができないか** を整理し、**着手しやすい実装候補** を提示する。
調査範囲: `ArtifactPr/src/`, `ArtifactPr/include/` の `.cppm` / `.ixx`。

---

## 1. TL;DR (要約)

`ArtifactPr` は **α プロトタイプ** として骨格は整っているが、以下のような問題がある:

- ⚠️ **AGENTS.md / taste 違反コードが混入**: `setStyleSheet` 25 hit / 新規 signal-slot 多用
- ⚠️ **コアロジックが欠如**: thumbnail / GPU / audio engine / effect on clip / render queue がすべて 0
- ✅ **骨格は揃っている**: Editor Engine / MainWindow / 5 panel / Widget / Demo data
- ✅ **実装できるものは多い**: Marker (83 hit) / Transition (16) / 既存 Foundation を活用

**着手しやすい実装順 (推奨)**:
1. **renderer を QMediaPlayer から QAbstractVideoSurface ベースに置換** (動画 preview の品質向上)
2. **thumbnail 生成** (Media panel の体験向上)
3. **audio engine 自作** (loudness / gain / routing)
4. **M-PR-EDIT-1 Slip/Slide/Ripple/Trim/Insert/Overwrite** (NLE 編集の中核)
5. **M-PR-MULTICAM-1 Multi-cam 編集** (インタビュー / ライブ配信)
6. **M-PR-CAPTION-1 Caption / Subtitle** (Foundation `M-CAPTION-1` から転用)
7. **M-PR-RENDER-1 Render Queue** (`MILESTONE_RENDER_QUEUE_2026-03-22.md` から転用)

---

## 2. 既存資産 (再走査)

### 2.1 全体規模

| ファイル | 行数 |
|---|---:|
| `ArtifactPr/src/ArtifactPrEditorEngine.cppm` | **1460** |
| `ArtifactPr/src/ArtifactPrMainWindow.cppm` | **2108** |
| `ArtifactPr/src/ArtifactPrDemoData.cppm` | 45 |
| `ArtifactPr/src/ExportDialog.cppm` | 92 |
| `ArtifactPr/src/MediaPanel.cppm` | 95 |
| `ArtifactPr/src/ProjectPanel.cppm` | 55 |
| `ArtifactPr/src/TransportBarWidget.cppm` | 166 |
| `ArtifactPr/src/VideoPlayerWidget.cppm` | 50 |
| **合計** | **~4071 行** |

### 2.2 パターン

| パターン | 件数 | 評価 |
|---|---:|---|
| `connect()` | 90 | ✅ 多用 (中央集権 hub 複数) |
| `Q_EMIT` | 88 | ✅ signal 多用 |
| `W_OBJECT_IMPL` / `W_OBJECT(` / `W_SIGNAL` | 39 | ✅ class ベース |
| `setStyleSheet` | **25** | ⚠️ **AGENTS.md / taste 違反** |
| `QMessageBox` | 3 | OK (dialog 用途) |
| `QInputDialog` | 3 | OK (dialog 用途) |
| `QFileDialog` | 8 | OK (IO 用途) |
| `QFont` | 17 | OK (UI text 用途) |
| `QPainter` | 10 | OK (custom draw) |
| `update()` | 12 | OK (再描画要求) |
| `QColorDialog` | 0 | ✅ taste 整合 |
| `QFontDialog` | 0 | ✅ taste 整合 |
| `QImage` | 0 | ✅ taste 整合 |
| `repaint()` in paintEvent | 0 | ✅ perf 整合 |

### 2.3 ロジック関連

| 機能 | 件数 | 場所 |
|---|---:|---|
| `Marker` (Timeline 構造体) | **83** | Editor Engine |
| `Transition` (Crossfade / DipToBlack / Wipe) | 16 | Editor Engine |
| `DemoClip` / `DemoTrack` | 10 / 12 | Demo Data |
| `audio` / `video` 言及 | 10 / 8 | TransportBar / Video |
| `QMediaPlayer` | 3 | VideoPlayerWidget |
| `DockManager` / `DockWidget` (KDDockWidgets) | 52 | MainWindow |
| `JSON save` / `JSON load` | 1 / 1 | EditorEngine (project 保存) |

### 2.4 完全未実装 (MISS) — 重要

| 機能 | 件数 | 評価 |
|---|---:|---|
| **Thumbnail 生成** | 0 | ❌ Media panel が file 名のみ表示 |
| **GPU / OpenGL** | 0 | ❌ 動画 preview は `QMediaPlayer` 任せ |
| **Audio engine 自作** | 0 | ❌ audio sync / gain / loudness 不可能 |
| **Effect on clip** | 0 | ❌ NLE の中核機能 |
| **Render queue** | 0 | ❌ export は dialog で 1 ファイルのみ |
| **Multi-cam** | 0 | ❌ |
| **Caption / Subtitle** | 0 | ❌ |
| **Loudness meter** | 0 | ❌ |
| **Image sequence export** | 0 | ❌ |
| **Color scope (Vector / Waveform)** | 0 | ❌ |
| **Audio scrub** | 0 | ❌ |
| **OCIO** | 0 | ❌ |

---

## 3. AGENTS.md / taste 違反事項

### 3.1 違反一覧 (要修正)

| 違反 | 件数 | 場所 | 重要度 |
|---|---:|---|---|
| `setStyleSheet` 新規使用 | **25 hit** | VideoPlayerWidget, MainWindow, Demo Data | ⚠️ **中** — 25 件は単発警告ベースで集約が必要 |
| 新規 signal/slot 接続 (中央集権 hub) | **90 hit** | 全部 | ⚠️ **低〜中** — 既存実装の範疇。新規追加時に要注意 |
| `QMessageBox` / `QInputDialog` | 3 / 3 | MainWindow | ⚠️ **低** — dialog 用途なので OK 寄り |

### 3.2 整合 (taste 遵守)

| 項目 | 状況 |
|---|---|
| `QImage` | ✅ 0 hit (原則禁止を遵守) |
| `QColorDialog` | ✅ 0 hit (禁止遵守) |
| `QFontDialog` | ✅ 0 hit (禁止遵守) |
| `repaint()` in paintEvent | ✅ 0 hit (perf OK) |

### 3.3 違反コード例

```cpp
// VideoPlayerWidget.cppm:21
placeholderLabel_->setStyleSheet(
    QStringLiteral("background-color: #1a1a1a; color: #555;"));
// → QPalette / owner-draw / 既存 theme token に置換すべき

// ExportDialog.cppm:40, 81
connect(browseBtn, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);
connect(exportBtn, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
// → 既存 dialog signal の流用、新規 global signal ではない (OK 寄り)
```

---

## 4. アーキテクチャ分析

### 4.1 構造

```
ArtifactPr/
├── EditorEngine (1460 lines) ← コアロジック。Project / Sequence / Track / Clip / Marker / Transition / Undo / Redo
├── MainWindow (2108 lines)  ← KDDockWidgets ベースの UI shell
├── DemoData (45 lines)       ← 起動時のサンプルデータ
├── MediaPanel (95)           ← ファイルリスト
├── ProjectPanel (55)         ← project tree
├── TransportBarWidget (166)  ← 再生コントロール + timecode + speed
├── VideoPlayerWidget (50)    ← QMediaPlayer ベースの video preview
├── ExportDialog (92)         ← 1 ファイル export (H.264 / H.265 / ProRes / DNxHD)
└── main.cpp                  ← エントリ
```

→ **KDDockWidgets ベース** の **ドッキング可能 UI**。これは `Artifact` アプリ (QMainWindow 直) とは異なる選択。**意図的** とのこと。

### 4.2 強み

1. **QMediaPlayer** による video playback が動作 (再生 / 一時停止 / シーク / 音量)
2. **Undo / Redo stack** (`qDeleteAll(undoStack_)`) が **実装済み**
3. **JSON project save / load** 実装済み (`EditorEngine`)
4. **Transport bar の timecode** 表示が **30 fps 固定で動作**
5. **Speed playback** (1x / 2x / -1x / -2x) 対応
6. **Marker** の構造体定義あり (In / Out / Standard)
7. **Transition** (Crossfade / DipToBlack / Wipe) の enum + 構造体定義あり
8. **DockManager** ベースで柔軟

### 4.3 弱み

1. **AGENTS.md 違反コードが混入** (`setStyleSheet` 25 件)
2. **Thumbnail 生成なし** → Media panel が file 名のみ
3. **QMediaPlayer 任せ** → GPU preview 不可 / audio sync 不可
4. **Audio engine 自作なし** → loudness / gain / routing / scrubbing 不可
5. **Effect on clip** なし → NLE として肝心の機能が空
6. **Render queue なし** → 1 ファイル export のみ
7. **Color 系 (Scope / OCIO / LUT)** なし
8. **Caption / Subtitle** なし
9. **Auto-save / Crash recovery** なし
10. **Undo/Redo はあるが depth が不明** (EditorEngine に `qDeleteAll` のみ)

---

## 5. 実装できそうな候補 (Phase 別)

### 5.1 Phase A: 即着手 (1〜2 sprint)

#### A-1. **M-PR-RENDERER-1 Video Preview 強化** (3〜5 day)

- 現状: `QMediaPlayer` ベース (system decoder 任せ)
- 改善: `QAbstractVideoSurface` ベースでフレーム取得 → custom paint
- 利点: GPU preview / scrub frame accuracy / color space control / GPU sync
- 触る: `ArtifactPr/src/VideoPlayerWidget.cppm` (再設計)

#### A-2. **M-PR-THUMB-1 Thumbnail 生成** (1〜2 day)

- 現状: Media panel に file 名のみ
- 改善: ffmpeg / Qt AVF 経由で 1 frame 抽出 → 縮小 → cache
- 触る: `ArtifactPr/src/MediaPanel.cppm` + 新規 `MediaThumbnailer.{ixx,cppm}`

#### A-3. **M-PR-EDIT-1 NLE 編集 (Slip / Slide / Ripple / Trim / Insert / Overwrite)** (1〜3 week)

- 現状: Cut / Copy / Paste の ID ベースのみ
- 改善: clip 端編集 / 3 mode 編集 / sync lock / linked selection
- 触る: `ArtifactPr/src/ArtifactPrEditorEngine.cppm` (新規 `EditCommand` 追加) + `ArtifactPr/src/ArtifactPrMainWindow.cppm` (Timeline UI 拡張)
- 重要度: **NLE の中核機能**

### 5.2 Phase B: 中期 (1 sprint)

#### B-1. **M-PR-AUDIO-1 Audio engine 自作** (1〜2 week)

- 現状: QMediaPlayer 任せ (audio 制御不可)
- 改善: 音声ファイルを decode して PCM ストリーム取得 → audio scrub / gain / routing / loudness meter
- 触る: 新規 `ArtifactPr/include/Audio/{AudioEngine,AudioBuffer,AudioRouting,AudioLoudness}.ixx` + `.cppm`
- 重要度: audio scrub / loudness / VST host の前提

#### B-2. **M-PR-MULTICAM-1 Multi-cam 編集** (2〜4 week)

- 現状: 未着手
- 改善: Multi-cam source group / camera switcher UI / 多 camera 同期
- 触る: `EditorEngine` + `MainWindow`

#### B-3. **M-PR-CAPTION-1 Caption / Subtitle (SRT / WebVTT)** (1 week)

- 既存: `M-CAPTION-1` (ArtifactCore 側) の foundation 転用
- 改善: `ArtifactPr` 側に caption track を追加
- 触る: 新規 `ArtifactPr/src/CaptionPanel.cppm` + `EditorEngine` 拡張

### 5.3 Phase C: 中長期

#### C-1. **M-PR-RENDER-1 Render Queue** (1〜2 week)

- 既存: `MILESTONE_RENDER_QUEUE_2026-03-22.md` から foundation 転用
- 改善: batch export / queue management / progress
- 触る: 新規 `ArtifactPr/src/RenderQueuePanel.cppm` + `EditorEngine` 拡張

#### C-2. **M-PR-COLOR-1 Color panel (Lumetri + Scopes)** (2〜3 week)

- 既存: `M-SCOPES-1` / `M-OCIO-1` の foundation 転用
- 改善: `ArtifactPr` 側に color panel を追加
- 触る: 新規 `ArtifactPr/src/ColorPanel.cppm` + Scopes widget

#### C-3. **M-PR-EFFECT-1 Effect on clip (NLE style)** (3〜5 week)

- 現状: 未着手
- 改善: clip 単位の effect chain / preset browser
- 触る: `EditorEngine` + `MainWindow` (Timeline effect row)

### 5.4 Phase D: 長期 / 戦略

#### D-1. **M-PR-LIVE-1 WebSocket Live Preview** (数日)

- 既存: `ArtifactWebUIHost` 活用
- 改善: WebSocket 経由で編集中をブラウザで確認
- 触る: 新規 `ArtifactPr/src/WebPreviewServer.cppm` + Web UI

#### D-2. **M-PR-AI-1 Auto Caption / ASR** (4〜8 week)

- 既存: `MILESTONE_AI_ASSISTED_FEATURES_2026-03-29.md` の foundation 転用
- 改善: ASR (Whisper) 統合 → SRT 自動生成

---

## 6. 着手優先度マトリクス

| 候補 | 価値 | 工数 | 既存依存 | 優先度 |
|---|---|---|---|---|
| M-PR-EDIT-1 NLE 編集 | ★★★★★ | 中 | EditorEngine | **最優先** |
| M-PR-RENDERER-1 Preview 強化 | ★★★★ | 小 | VideoPlayerWidget | 高 |
| M-PR-THUMB-1 Thumbnail | ★★★ | 小 | MediaPanel | 高 |
| M-PR-AUDIO-1 Audio engine | ★★★★ | 大 | 新規 | 中 |
| M-PR-CAPTION-1 Caption | ★★★ | 中 | M-CAPTION-1 | 中 |
| M-PR-RENDER-1 Render queue | ★★★ | 中 | MILESTONE_RENDER_QUEUE | 中 |
| M-PR-COLOR-1 Color | ★★★ | 中 | M-SCOPES-1 / M-OCIO-1 | 中 |
| M-PR-MULTICAM-1 Multi-cam | ★★★★ | 大 | 新規 | 低 |
| M-PR-EFFECT-1 Effect on clip | ★★★★ | 大 | 新規 | 低 |
| M-PR-LIVE-1 WebSocket | ★★ | 小 | ArtifactWebUIHost | 低 |
| M-PR-AI-1 Auto caption | ★★★ | 大 | M-AI | 低 |

---

## 7. 既存 milestone との接続

### 7.1 既に Artifact 側で作成した milestone (ArtifactPr で転用可能)

| milestone | 概要 | ArtifactPr 側での活用 |
|---|---|---|
| **M-WEBEXPORT-1** | SVG + CSS keyframes + HTML player | Sequence → Web export |
| **M-LIVE-1** | WebSocket Live Preview | 編集中 Web プレビュー |
| **M-LOTTIE-1** | Lottie JSON | Adobe AE / Figma 互換 |
| **M-WEBGPUEXPORT-1** | Three.js / WebGPU | 3D / 立体ロゴ |
| **M-EXPORT-1** | Render Format Expansion | PNG / ProRes / HAP |
| **M-CAPTION-1** | Caption / Subtitle | SRT / WebVTT |
| **M-OCIO-1** | OpenColorIO | Color pipeline |
| **M-SCOPES-1** | Scopes (Vector / Waveform) | Color panel |
| **M-CRASH-1** | Crash-safe Save | project save 安全性 |
| **M-LOUDNESS-1** | Loudness Meter | audio panel |

### 7.2 まだ作成していない ArtifactPr 専用 milestone

- **M-PR-EDIT-1 NLE 編集** (Slip/Slide/Ripple/Trim/Insert/Overwrite)
- **M-PR-RENDERER-1 Video Preview 強化**
- **M-PR-THUMB-1 Thumbnail**
- **M-PR-AUDIO-1 Audio engine 自作**
- **M-PR-MULTICAM-1 Multi-cam**
- **M-PR-CAPTION-1 Caption / Subtitle**
- **M-PR-RENDER-1 Render Queue**
- **M-PR-COLOR-1 Color panel**
- **M-PR-EFFECT-1 Effect on clip**
- **M-PR-LIVE-1 WebSocket Live Preview**
- **M-PR-AI-1 Auto Caption / ASR**

---

## 8. リスクと軽減

| リスク | 影響 | 軽減策 |
|---|---|---|
| `ArtifactPr` を **production 化する方針が未確定** | 工数見積もりが立てにくい | README の「intentionally separate」方針の確認をユーザに依頼 |
| AGENTS.md 違反コード (`setStyleSheet` 25 件) | taste 違反 | Phase A の最初で違反箇所を一括修正 (QPalette / owner-draw / theme token に置換) |
| 新規 signal-slot 90 件 | 中央集権化の温床 | 既存実装の範疇。新規追加は **必ず設計レビュー** |
| QMediaPlayer 依存 | GPU preview / audio sync / scrubbing 不可 | Phase A-1 で `QAbstractVideoSurface` ベースに置換 |
| Audio engine 自作コスト | audio 機能の大部分が後回し | M-PR-AUDIO-1 は **Phase B-1** で着手。QMediaPlayer 維持で Phase A を完走 |
| Render queue なし | 大量 export 不可 | M-PR-RENDER-1 で batch export を追加 (Phase B-3) |
| Thumbnail なし | Media panel の UX が悪い | M-PR-THUMB-1 で即着手 (Phase A-2) |
| Effect on clip なし | NLE として肝心の機能が空 | Phase C-3 で着手 (低優先) |
| Color 系 なし | グレーディング不可 | M-PR-COLOR-1 で Phase C-2 で着手 (中優先) |

---

## 9. 不変条件 (AGENTS.md / taste 整合)

### 9.1 禁止事項 (新規実装で守る)

- **`QtCSS` / `setStyleSheet` 新規追加禁止**。既存 25 箇所は修正対象
- **`QImage` 新規採用禁止** (export 用途は除く)
- **`QColorDialog` / `QFontDialog` 新規使用禁止**
- **新規 signal-slot 接続は設計レビュー必須** (中央集権 hub 化を避ける)

### 9.2 推奨事項

- **Core 側にロジックを置く**。UI 層は thin に
- **既存 API の後方互換を維持**
- **`ArtifactWidgets` サブモジュール不可触**
- **submodule bump 手順**: `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 10. Done Criteria (Phase A 完走のため)

- [ ] AGENTS.md 違反 (`setStyleSheet` 25 件) を 0 件に削減
- [ ] M-PR-RENDERER-1: `QAbstractVideoSurface` ベースに置換
- [ ] M-PR-THUMB-1: Thumbnail 生成 + Media panel 表示
- [ ] M-PR-EDIT-1: 6 種類の NLE 編集 (Slip / Slide / Ripple / Trim / Insert / Overwrite)
- [ ] Unit test (GoogleTest / Qt Test) の coverage 80% 以上
- [ ] 既存 milestone (M-CAPTION-1 / M-SCOPES-1 / M-OCIO-1) との接続ドキュメントを更新
- [ ] `RENDER_FORMAT_CONTRACT_2026-05-16.md` の linear canonical と整合

---

## 11. 関連ファイル

### 11.1 触る (新規 / 変更)

| ファイル | 種類 |
|---|---|
| `ArtifactPr/src/ArtifactPrEditorEngine.cppm` | 変更 (EditCommand / Multi-cam / RenderQueue 追加) |
| `ArtifactPr/src/ArtifactPrMainWindow.cppm` | 変更 (Timeline UI 拡張、setStyleSheet 修正) |
| `ArtifactPr/src/VideoPlayerWidget.cppm` | 変更 (QAbstractVideoSurface 化、setStyleSheet 修正) |
| `ArtifactPr/src/MediaPanel.cppm` | 変更 (Thumbnail 表示) |
| `ArtifactPr/src/ExportDialog.cppm` | 変更 (Render Queue 起動) |
| `ArtifactPr/include/Audio/*.ixx` | 新規 (Audio engine 群) |
| `ArtifactPr/include/Render/*.ixx` | 新規 (Render queue / batch export) |
| `ArtifactPr/include/MultiCam/*.ixx` | 新規 (Multi-cam source group) |
| `ArtifactPr/src/CaptionPanel.cppm` | 新規 (Caption UI) |
| `ArtifactPr/src/RenderQueuePanel.cppm` | 新規 (Render queue UI) |
| `ArtifactPr/src/ColorPanel.cppm` | 新規 (Color UI) |
| `ArtifactPr/src/WebPreviewServer.cppm` | 新規 (WebSocket) |

### 11.2 触らない

- `ArtifactWidgets/` サブモジュール
- `Artifact/` サブモジュール (明示依頼時のみ)
- `ArtifactCore/` サブモジュール (明示依頼時のみ)
- `libs/`, `third_party/*`

---

## 12. 更新履歴

- 2026-06-16: 初版作成。`ArtifactPr/` 配下を 50 項目で走査。AGENTS.md 違反 25 件発見。