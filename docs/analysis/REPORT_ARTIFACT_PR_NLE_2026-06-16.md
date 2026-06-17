# ArtifactPr (Premiere-like NLE) 機能ギャップレポート — 2026-06-16

作成日: 2026-06-16
目的: `ArtifactPr` サブモジュールのソースコードを読み、Premiere Pro / Final Cut / DaVinci Resolve の NLE 機能に対する **実装済み / 未実装** を整理する。
調査範囲: `ArtifactPr/` 配下のみ。`ArtifactCore` 共有は除く。

---

## 1. ArtifactPr 概要

`ArtifactPr/README.md`:
> Pr-like editor prototype that reuses `ArtifactCore` as the shared foundation.
> This project is intentionally separate from `Artifact` so the two apps can grow in different directions without forcing the same UI or timeline shape.

→ **Premiere 風の独立プロトタイプ**。`ArtifactCore` 共有。`Artifact` コンポジット系とは UI / timeline 形状が異なる。

### 1.1 ファイル構成 (26 件)

| 種類 | ファイル |
|---|---|
| Core | `ArtifactPrEditorEngine.{cppm,ixx,hpp}` |
| Window | `ArtifactPrMainWindow.{cppm,ixx,hpp}` |
| Panel | `MediaPanel.{cppm,ixx,hpp}` / `ProjectPanel.{cppm,ixx,hpp}` |
| Widget | `TransportBarWidget.{cppm,ixx,hpp}` / `VideoPlayerWidget.{cppm,ixx,hpp}` |
| Dialog | `ExportDialog.{cppm,ixx,hpp}` |
| Demo | `ArtifactPrDemoData.{cppm,ixx,hpp}` |
| Entry | `main.cpp` / `CMakeLists.txt` |

→ 7 widget 系。最小限の NLE 構造はあり。

---

## 2. 実装済み (OK)

| 機能 | 件数 | 評価 |
|---|---:|---|
| **Transition (basic)** | 78 | 良好。`Crossfade / DipToBlack / WipeLeft / WipeRight` enum + 構造体 |
| **Multi-track timeline** | 53 | 良好。`videoTracks / audioTracks` + `DemoClip / DemoTrack` 構造体 |
| **Cut / Copy / Paste clip** | 6 | 部分的。メニューなし、クリップ ID ベース |
| **In / Out (sequence)** | 4 | 部分的。`Marker::Type::In / Out` enum |
| **Magnetic timeline (snap)** | 3 | 部分的。`TRIM_HANDLE_WIDTH / FRAME_WIDTH` 定数のみ |
| **Audio waveform on timeline** | 2 | 部分的 |
| **Source patching (program monitor)** | 2 | 部分的 |
| **ProRes / HAP / AV1 export** | 1 | `ExportDialog::codecCombo_` に ProRes 文字列のみ |

→ **Transition / Multi-track / 基本構造は揃っている**。実用 NLE としては最低限の骨格あり。

---

## 3. 完全未実装 (MISS) — 58 項目

### 3.1 NLE 編集機能 (24 項目)

| 機能 | Premiere / FCP での位置付け |
|---|---|
| **Nesting (sequence in sequence)** | Premiere: nested sequence。NLE 中核 |
| **Multi-cam editing** | Premiere: Multi-Cam 編集 |
| **Linked selection (sync audio+video)** | Premiere: linked selection |
| **Adjustment layer (broadcast safe)** | Premiere: Adjustment Layer |
| **Timecode burn-in overlay** | 納品時の焼き込み |
| **Captions / Subtitles (CEA-708)** | 放送字幕 |
| **Loudness meter (BS.1770)** | 配信基準 (Resolve / Premiere Pro) |
| **SRT / WebVTT import-export** | web 字幕 |
| **Slip / Slide / Ripple** | 3 mode 編集 |
| **Trim edit** | clip 端編集 |
| **Sync lock** | A/V sync 維持 |
| **Insert edit** | insert 編集 |
| **Overwrite edit** | overwrite 編集 |
| **Lift / Extract** | 非破壊編集 |
| **Three-point edit** | 3 point 編集 |
| **Four-point edit** | 4 point 編集 |

### 3.2 Audio 編集 (5 項目)

| 機能 |
|---|
| **Audio scrub** |
| **Audio gain keyframe** |
| **VST plugin host** |
| **Audio routing matrix** |
| **ADSR envelope** |

### 3.3 Color (5 項目)

| 機能 |
|---|
| **Lumetri / color panel** |
| **Vector scope** |
| **Waveform scope** |
| **Parade scope** |
| **OCIO config** |
| **LUT (export / import)** |

### 3.4 Render / Export (7 項目)

| 機能 |
|---|
| **Render queue** |
| **Background render** |
| **Image sequence export** |
| **Audio-only export** |
| **Checkpoint / retry** |

### 3.5 Effects (5 項目)

| 機能 |
|---|
| **Effect on clip (NLE style)** |
| **Effect preset / browse** |
| **Lumetri preset** |
| **Audio effect** |
| **Adjustment layer effect** |
| **Mask (NLE clip)** |

### 3.6 Project / Metadata (7 項目)

| 機能 |
|---|
| **Project metadata** |
| **Project share (team)** |
| **Version / revision** |
| **Auto-save** |
| **Crash recovery** |
| **Atomic save** |

### 3.7 Collaboration / Cloud (3 項目)

| 機能 |
|---|
| **Team project (real-time sync)** |
| **Cloud asset** |
| **Loudness normalization (streaming)** |

### 3.8 Performance (4 項目)

| 機能 |
|---|
| **Smart render (region only / ROI)** |
| **LOD preview** |
| **Background decode** |
| **Hardware decode (NVDEC / QSV)** |

### 3.9 UI / Project 拡張 (5 項目)

| 機能 |
|---|
| **Project inspector** |
| **Marker (timeline)** |
| **Sequence preset** |
| **Smart conform / reframe** |
| **Speech to text / auto caption** |

→ **Premiere Pro / DaVinci Resolve の主要機能の 95% が未着手**。ArtifactPr は **プロトタイプ** という位置付けで、α 版として最低限の骨格のみ。

---

## 4. 既存 App との比較

| 軸 | `Artifact` (コンポジット) | `ArtifactPr` (NLE) |
|---|---|---|
| Composition | ✅ 完全 | - |
| Layer / Effect | ✅ 完全 | - |
| Render Engine | ✅ 完全 (Diligent GPU) | - |
| Timeline (Multi-track) | ✅ 部分 | ✅ 部分 |
| Transition | ✅ 部分的 | ✅ 部分的 |
| Multi-cam / Magnetic | - | ❌ |
| Audio scrub / VST | - | ❌ |
| Color (Lumetri / Scopes) | - | ❌ |
| Render queue | ✅ 部分 | ❌ |
| Effect on clip | ✅ 部分的 | ❌ |
| Caption / Subtitle | - | ❌ |
| Auto-save | ✅ 部分的 | ❌ |
| Crash recovery | ✅ 部分的 | ❌ |

→ `Artifact` も `ArtifactPr` も **NLE 機能の大半を欠いている**。ただし `Artifact` の方がエフェクト / レイヤー / color 系は圧倒的に進んでいる。

---

## 5. 新規 milestone 候補 (ArtifactPr 専用)

### 5.1 P0

| テーマ | 価値 |
|---|---|
| **M-PR-EDIT-1 Edit 操作 (Slip / Slide / Ripple / Trim / Insert / Overwrite)** | NLE の中核編集機能。3 mode 編集は Premiere の基礎 |
| **M-PR-AUDIO-1 Audio 編集 (scrub / gain / VST / routing)** | 動画制作の audio 80% をカバー |
| **M-PR-CAPTION-1 Caption / Subtitle (SRT / WebVTT / CEA-708)** | web 配信 + 放送 |
| **M-PR-LOUDNESS-1 Loudness Meter (BS.1770) + Normalize** | 配信基準 (YouTube / Netflix / ATSC) |
| **M-PR-COLOR-1 Lumetri + Scopes (Vector / Waveform / Parade)** | DaVinci 的な Color page |
| **M-PR-RENDER-1 Render Queue + Background + Image Sequence** | NLE の納品基盤 |

### 5.2 P1

| テーマ | 価値 |
|---|---|
| **M-PR-MULTICAM-1 Multi-cam 編集** | インタビュー / ライブ配信 |
| **M-PR-NESTED-1 Sequence Nesting** | 中核 NLE |
| **M-PR-LINK-1 Linked Selection + Sync Lock** | A/V sync |
| **M-PR-ADJ-1 Adjustment Layer + Timecode Burn-in** | 放送基準 |
| **M-PR-EFFECT-1 Clip Effect chain + Preset browser** | Lumetri preset |
| **M-PR-AUTO-1 Auto-save + Crash Recovery (atomic)** | 安全性 |
| **M-PR-VERSION-1 Version / Revision 履歴** | 共同制作 |

### 5.3 P2

| テーマ | 価値 |
|---|---|
| **M-PR-ROI-1 Smart Render (Region)** | 巨大 project の効率 |
| **M-PR-TEAM-1 Team project / Cloud asset** | 将来 |
| **M-PR-CAPTION-AI-1 Auto Caption / ASR** | AI |
| **M-PR-CONFORM-1 Smart Conform / Reframe** | SNS 用 |

---

## 6. 既存 milestone との接続

`Artifact` 側で作成済みの milestone は **`ArtifactPr` からも流用可能**:

- `M-CAPTION-1 Caption / Subtitle` — SRT/WebVTT parser は `ArtifactCore` 共有
- `M-EXPORT-1 Render Format Expansion` — codec 拡張
- `M-OCIO-1 OpenColorIO` — color pipeline
- `M-SCOPES-1 Scopes` — Vector / Waveform / Parade
- `M-CRASH-1 Crash-safe Save` — atomic write
- `M-TEMPLATE-1 Project Template Gallery`

→ `Artifact` で実装 → `ArtifactPr` で利用 という流れが可能。

---

## 7. 既存 App (Artifact) と並走した場合の推奨分担

| 機能 | 担当 |
|---|---|
| Composition / Layer / Effect | `Artifact` |
| **Multi-track timeline / Transition / Markers** | **`ArtifactPr`** |
| **Audio edit / VST / Loudness** | **`ArtifactPr`** |
| **Multi-cam / Linked selection / Sync lock** | **`ArtifactPr`** |
| **Caption / Subtitle / Timecode burn-in** | **両方** (SRT parser は共通) |
| **Color pipeline (OCIO / Scopes)** | **両方** (Core 共通) |
| **Render queue / Image sequence export** | **両方** (encoder 共通) |
| **Project save / Atomic write / Auto-save** | **両方** (Core 共通) |

---

## 8. リスクと未解決論点

### 8.1 構造的リスク

1. **`ArtifactPr` は α プロトタイプ**。Premiere Pro 相当にするには莫大な工数
2. **`Artifact` との UI 統一**。Dock / Window / Tool 構造が 2 つ存在する
3. **共有 foundation の extension コスト**。新機能は `ArtifactCore` 側に置く必要

### 8.2 設計未解決

- `ArtifactPr` を **production 化** するかどうか。README に「intentionally separate」とあり、方針未確定
- `Artifact` の timeline / 編集機能を `ArtifactPr` 側に移植するかどうか
- Project 形式の統一 (`.apr` vs `.artifact`)

### 8.3 サブモジュール境界

- `ArtifactPr` 配下のみを書く
- `ArtifactCore` 共有は Core 側を変更
- `Artifact` / `ArtifactWidgets` は触らない (明示依頼時のみ)
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 9. まとめ

- **`ArtifactPr` は α プロトタイプ**。最低限の NLE 骨格 (multi-track / transition / cut/copy/paste / in-out) のみ
- **Premiere Pro の主要機能の 95% が未着手** (58 項目 MISS)
- **既存 App (`Artifact`) の方がエフェクト / color / render は圧倒的に進んでいる**
- 共有 foundation として `ArtifactCore` 経由で **SRT / OCIO / Scopes / Render queue / Crash-safe save** などは **両方** で利用可能

**推奨される次の一手**:

1. **`Artifact` 側の foundation milestone を先に完成** (M-CAPTION-1 / M-OCIO-1 / M-SCOPES-1 / M-EXPORT-1 / M-CRASH-1)
2. **`ArtifactPr` 側から foundation を import**
3. **`ArtifactPr` 専用 NLE 機能を追加** (M-PR-EDIT-1 / M-PR-AUDIO-1 / M-PR-CAPTION-1)

`Artifact` 優先なら、現状の milestone backlog 通り。M-OCIO-1 / M-SCOPES-1 / M-EXPORT-1 / M-CAPTION-1 / M-CRASH-1 を起こすと **`ArtifactPr` 側にも転用できる** ため、双方にとって価値が高い。

---

## 10. 更新履歴

- 2026-06-16: 初版作成。`ArtifactPr/` 配下を NLE 機能の 64 項目で走査。