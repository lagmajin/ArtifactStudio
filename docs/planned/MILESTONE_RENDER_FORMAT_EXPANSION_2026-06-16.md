# M-EXPORT-1 Render Format Expansion Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `ArtifactCore/src/Video/AbstractEncoder.cppm`,
      `ArtifactCore/src/Video/FFMpegEncoder*`,
      `ArtifactCore/src/Video/EncoderSetting*`,
      `ArtifactCore/src/Video/FFMpegAudioEncoder*`,
      `Artifact/src/Service/ArtifactRenderQueueService.cppm`,
      `Artifact/src/Widgets/Render/RenderQueueManagerWidget.cppm`,
      `Artifact/src/Widgets/Render/ArtifactRenderOutputSettingDialog.cppm`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`
位置づけ: 既存の H.264 / MP4 中心の export を **Image sequence / ProRes / HAP / WebM / AV1 / OGG / Opus / FLAC / Audio-only** に拡張する foundation。
参照:
- `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.11
- `docs/analysis/DESIRED_IMPORT_FORMATS_2026-04-19.md`
- `docs/planned/RENDER_QUEUE_MANAGER_GAP_ANALYSIS_2026-04-13.md` (画像シーケンス / 音声 / テンプレ export)
- `docs/planned/MILESTONE_RENDER_QUEUE_2026-03-22.md`
- `docs/planned/MILESTONE_RENDER_QUEUE_ENCODING_2026-04-01.md`
- `docs/planned/MILESTONE_RENDER_QUEUE_GPU_BACKEND_2026-04-03.md`
- `docs/planned/MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` (Phase 1 で out-of-process encoder)
- `docs/planned/MILESTONE_OCIO_INTEGRATION_2026-06-16.md` (export display role)

---

## 1. 目的

`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.11:

> - OpenColorIO (OCIO) config: 0 hit
> - OpenEXR multilayer: 0 hit
> - FBX / Alembic / USD import: 0 hit
> - WebM / VP9 export: 0 hit
> - AV1 export: 0 hit
> - ProRes export: 0 hit
> - HAP export (game): 0 hit
> - OGG / Opus / FLAC import: 0 hit
> - 3D LUT (.cube / .3dl) write: 0 hit
> - Audio export (only): 0 hit
> - Image sequence export: 0 hit

`RENDER_QUEUE_MANAGER_GAP_ANALYSIS_2026-04-13.md` の 16 件の gap のうち、**画像シーケンス出力 (1)** と **オーディオレンダリング分離 (2)** が P0 に挙げられている。

プロダクションでは H.264 / MP4 だけでは不十分:

- **Image sequence (PNG / TIFF / EXR)**: コンポジット再加工、VFX ワークフローの中間ファイル
- **ProRes 422 / 4444**: 放送・映画納品の高品質 codec
- **HAP / HAP-Q**: ゲーム・インスタレーションの再生負荷低減
- **WebM / VP9 / AV1**: Web 配信・オープン codec
- **Audio-only (WAV / MP3 / OGG / FLAC)**: ポッドキャスト / プレビュー用
- **OGG / Opus / FLAC import**: `DESIRED_IMPORT_FORMATS` の「殆どのフォーマットで失敗」解消

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/Render/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `ArtifactCore/src/Video/AbstractEncoder.cppm` — encoder 抽象基底
- `ArtifactCore/src/Video/FFMpegEncoder*` — FFmpeg 経由の encoder (H.264 / MP4 中心)
- `ArtifactCore/src/Video/EncoderSetting*` — encoder 設定
- `ArtifactCore/src/Video/FFMpegAudioEncoder*` — 音声 encoder
- `Artifact/src/Service/ArtifactRenderQueueService.cppm` — render queue service
- `Artifact/src/Widgets/Render/RenderQueueManagerWidget.cppm` — queue UI
- `Artifact/src/Widgets/Render/ArtifactRenderOutputSettingDialog.cppm` — output setting dialog
- `MILESTONE_RENDER_QUEUE_2026-03-22.md` — queue 全体
- `MILESTONE_RENDER_QUEUE_ENCODING_2026-04-01.md` — encoding
- `MILESTONE_RENDER_QUEUE_GPU_BACKEND_2026-04-03.md` — GPU backend

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Image sequence export | 0 hit | VFX / 再加工の素材渡し不可 |
| Audio-only export | 0 hit | ポッドキャスト / プレビュー不可 |
| ProRes export | 0 hit | 放送納品不可 |
| HAP export | 0 hit | ゲーム / インスタレーション不可 |
| WebM / VP9 / AV1 export | 0 hit | Web 配信不可 |
| OGG / Opus / FLAC import | 0 hit | 殆どの音声 import が失敗 |
| LUT write | 0 hit | 他ツールへ LUT 渡せない (M-OCIO-1 経由で対応予定) |
| Encoder の選択 UI | `EncoderSetting` enum のみ | 拡張余地 |
| Output preset | なし | 繰り返し設定の手間 |
| Project 保存 | codec 設定 | 拡張余地 |

### 2.3 既存 milestone との関係

- `MILESTONE_RENDER_QUEUE_2026-03-22.md` — 既存 queue。本 milestone は **encoder 拡張** で補完
- `MILESTONE_RENDER_QUEUE_ENCODING_2026-04-01.md` — 既存 encoding。本 milestone は新 codec の追加
- `MILESTONE_RENDER_QUEUE_GPU_BACKEND_2026-04-03.md` — GPU backend。本 milestone は CPU/GPU 両方で動作
- `RENDER_QUEUE_MANAGER_GAP_ANALYSIS_2026-04-13.md` — 16 gap のうち画像 / 音声を本 milestone がカバー
- `MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` — out-of-process worker。本 milestone の encoder を `RenderJobRequest` 経由で farm に渡せる
- `MILESTONE_OCIO_INTEGRATION_2026-06-16.md` — output display role。本 milestone は output format と並走

---

## 3. 設計の柱

### 3.1 Encoder Kind 拡張

`ArtifactCore/src/Video/AbstractEncoder.cppm` を拡張:

```cpp
enum class EncoderKind {
    H264_MP4,           // 既存
    HEVC_MP4,           // H.265
    VP9_WebM,           // 新規
    AV1_MP4,            // 新規
    ProRes_422,         // 新規
    ProRes_4444,        // 新規
    HAP,                // 新規 (game)
    HAP_Q,              // 新規 (game high quality)
    PNG_Sequence,       // 新規
    TIFF_Sequence,      // 新規
    EXR_Sequence,       // 新規
    WAV_Audio,          // 新規
    MP3_Audio,          // 新規
    OGG_Audio,          // 新規
    FLAC_Audio,         // 新規
    Opus_Audio,         // 新規
    // 将来
    Custom,
};
```

- `EncoderKind` を video / audio / image の 3 種に分類
- 各 kind の `Encoder` 実装は `AbstractEncoder` 派生

### 3.2 Encoder 実装

| Kind | 実装 | 既存 / 新規 |
|---|---|---|
| H264_MP4 | `FFMpegEncoder` | 既存 |
| HEVC_MP4 | `FFMpegEncoder` | 既存 (kind 追加のみ) |
| VP9_WebM | `FFMpegEncoder` 派生 | 新規 |
| AV1_MP4 | `FFMpegEncoder` 派生 | 新規 |
| ProRes_422 / 4444 | `FFMpegEncoder` 派生 | 新規 |
| HAP / HAP_Q | `HAPEncoder` | 新規 |
| PNG_Sequence | `ImageSequenceEncoder` | 新規 |
| TIFF_Sequence | `ImageSequenceEncoder` 派生 | 新規 |
| EXR_Sequence | `ImageSequenceEncoder` 派生 | 新規 (multi-layer は将来) |
| WAV / MP3 / OGG / FLAC / Opus Audio | `FFMpegAudioEncoder` 派生 | 新規 |

### 3.3 Image Sequence Encoder

`ArtifactCore/src/Video/ImageSequenceEncoder.cppm`:

```cpp
class ImageSequenceEncoder : public AbstractEncoder {
public:
    ImageSequenceEncoder(EncoderKind kind, QString pathTemplate);  // "frame_%05d.png"

    void open() override;
    void writeFrame(const ImageF32x4RGBAWithCache& frame, int64_t frameNumber) override;
    void close() override;

    // 設定
    void setFormat(ImageSequenceFormat fmt);   // PNG / TIFF / EXR
    void setBitDepth(int bits);                // 8 / 16 / 32
    void setColorSpace(OCIOColorSpace cs);     // linear / sRGB / ACEScg
};
```

- 1 frame = 1 file
- `QImage` を使わず `ImageF32x4RGBAWithCache` 経由で書き出し
- ファイル名は `QString::arg` で 5 桁ゼロ埋め

### 3.4 Audio-only Encoder

`ArtifactCore/src/Video/AudioOnlyEncoder.cppm`:

```cpp
class AudioOnlyEncoder : public AbstractEncoder {
public:
    AudioOnlyEncoder(EncoderKind kind, QString outputPath);

    void open() override;
    void writeFrame(const ImageF32x4RGBAWithCache& /*unused*/, int64_t frameNumber) override;
    void writeAudioFrame(const AudioSegment& audio, int64_t frameNumber) override;
    void close() override;
};
```

- video frame は **破棄**、audio のみ書き出し
- 既存の `AudioMixer` から audio segment を取得

### 3.5 Audio Import 拡張

`ArtifactCore/src/Audio/AudioImporter.ixx` を新規追加:

```cpp
class AudioImporter {
public:
    // OGG / Opus / FLAC / WAV / MP3
    static AudioSegment import(const QString& filePath);
    static bool isSupported(const QString& extension);
};
```

- FFmpeg 経由でデコード
- `AudioSegment` 経由で既存 audio engine に乗る

### 3.6 Output Preset

`ArtifactCore/src/Video/OutputPreset.ixx`:

```cpp
struct OutputPreset {
    QString name;                    // "ProRes 422 HQ for Broadcast"
    EncoderKind kind;
    QMap<QString, QVariant> params;  // codec-specific
    QString colorSpace;              // sRGB / Rec.709 / Rec.2020
    int width;
    int height;
    double frameRate;
};

class OutputPresetLibrary {
public:
    static OutputPresetLibrary& instance();
    QList<OutputPreset> builtins() const;
    void saveCustom(const OutputPreset& preset);
    QList<OutputPreset> customs() const;
};
```

- built-in preset:
  - `H.264 High Quality (Web)`
  - `H.264 Blu-ray`
  - `ProRes 422 HQ (Broadcast)`
  - `ProRes 4444 (Mastering)`
  - `HAP (Game)`
  - `PNG Sequence (8-bit Linear)`
  - `PNG Sequence (16-bit sRGB)`
  - `EXR Sequence (32-bit Linear)`
  - `WAV 48kHz 24-bit (Audio)`
  - `MP3 192kbps (Podcast)`

### 3.7 UI 露出

`ArtifactRenderOutputSettingDialog` の **Codec** dropdown を `EncoderKind` で拡張:

- 一覧に `ProRes / HAP / Image Sequence / Audio Only / WebM / AV1` を追加
- preset dropdown で built-in を選択
- `Custom` 選択時は従来通り手動設定

`RenderQueueManagerWidget` に **preset column** を追加し、job 単位で preset を表示。

### 3.8 Project 保存

- `ArtifactProjectManager` の project JSON に `renderQueue.jobs[].encoderKind` と `outputPreset` 追加
- 旧プロジェクトは `H264_MP4` として開く
- preset は built-in 名で保存 (custom は `outputPresets.customs` セクション)

### 3.9 Render Farm 接続

`MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` の `RenderJobRequest::RenderSettings` に `EncoderKind` と `OutputPreset` を含める:

```cpp
struct RenderSettings {
    EncoderKind encoderKind;
    OutputPreset outputPreset;
    QString outputPath;
    int width;
    int height;
    double frameRate;
    QString colorSpace;
    // ... 既存
};
```

- out-of-process worker はこの `RenderSettings` を受け取り、適切な encoder で出力
- 同じ `EncoderKind` を farm worker 側で実装

### 3.10 不変条件 (Guardrails)

- 既存 `FFMpegEncoder` / `AbstractEncoder` の API は **温存**
- 既存 `EncoderSetting` enum は温存。新規 `EncoderKind` で並走
- `QImage` を **新規 hot path に入れない**。`ImageF32x4RGBAWithCache` 経由
- 既存 project は新 codec を **未選択** で開く
- 新規 signal-slot 接続は `encoderProgress / encoderDone` 2 個に限定
- 音声書き出し時の sample rate / bit depth を明示
- image sequence の **連番桁数** を 4/5/6 桁から選択可能

### 3.11 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `encoder.missing-codec` (severity=error, encoder 実装なし)
- `encoder.ffmpeg-failed` (severity=error, FFmpeg 失敗)
- `encoder.audio-missing` (severity=warning, audio-only 時に audio 不在)
- `encoder.image-seq-gap` (severity=warning, 連番欠落検出)
- `encoder.preset-incompatible` (severity=info, preset のパラメータ矛盾)

---

## 4. フェーズ計画

### Phase 1: EncoderKind + AbstractEncoder 拡張 (P0, 1 セッション)

- `EncoderKind` enum 拡張
- `AbstractEncoder` に `EncoderKind kind() const` 追加
- 既存 encoder の kind 設定

**Done criteria:**
- 既存 encoder が kind を持つ
- 新規 kind を **未実装** として列挙 (将来 ready 判定用)

### Phase 2: Image Sequence encoder (P0, 1〜2 セッション)

- `ImageSequenceEncoder` 実装
- PNG / TIFF / EXR (multi-layer なし) 対応
- bit depth 8/16/32

**Done criteria:**
- `PNG_Sequence` で 1 frame 書き出し
- 連番桁数 4/5/6 選択
- 既存 test project で output 確認

### Phase 3: Audio-only encoder (P0, 1 セッション)

- `AudioOnlyEncoder` 実装
- WAV / MP3 / OGG / FLAC / Opus
- 既存 `AudioMixer` から segment 取得

**Done criteria:**
- `WAV_Audio` で書き出し
- 動画が伴わない audio-only job が queue で動作
- sample rate / bit depth が指定値と一致

### Phase 4: ProRes / HAP / WebM / AV1 encoder (P0, 2 セッション)

- `FFMpegEncoder` 派生で 4 codec 実装
- `HAPEncoder` を別クラスで実装
- それぞれ kind 経由で dispatch

**Done criteria:**
- 4 codec すべてで `setEncoder(kind)` して書き出し可能
- `HAP` で QT プレイヤが読める
- `ProRes 422` で ffmpeg が読める

### Phase 5: Audio import 拡張 (P0, 1 セッション)

- `AudioImporter::import()` 実装
- OGG / Opus / FLAC / WAV / MP3 すべてに対応
- 既存 audio engine に乗る

**Done criteria:**
- 5 形式すべて import 成功
- 既存 `ArtifactAudioLayer` が自動的に利用

### Phase 6: Output preset + UI (P0, 1〜2 セッション)

- `OutputPreset` + `OutputPresetLibrary`
- `ArtifactRenderOutputSettingDialog` の dropdown 拡張
- `RenderQueueManagerWidget` に preset column
- built-in 9 種

**Done criteria:**
- preset dropdown から 9 種選択
- 選択後 dialog に codec 設定が反映
- 旧 dialog の挙動は温存

### Phase 7: Project 保存 + Diagnostics (P1, 1 セッション)

- project JSON に encoder kind と preset 追加
- 旧プロジェクトの default 補完
- Problem View への `encoder.*` 健全性 contribution

**Done criteria:**
- project 保存 → 再読込で復元
- 旧プロジェクトは H.264 / MP4 として開く
- `encoder.ffmpeg-failed` 等が Problem View に表示

### Phase 8: Render Farm 接続 (P2, 別 milestone 推奨)

- `MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` の `RenderSettings` に `EncoderKind / OutputPreset` 追加
- out-of-process worker で encoder 起動
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_RENDER_FARM_ENCODER_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_RENDER_QUEUE_2026-03-22.md` | 既存 queue。本 milestone は encoder 拡張。 |
| `MILESTONE_RENDER_QUEUE_ENCODING_2026-04-01.md` | encoding。本 milestone は codec 追加。 |
| `MILESTONE_RENDER_QUEUE_GPU_BACKEND_2026-04-03.md` | GPU backend。本 milestone は CPU/GPU 両対応。 |
| `MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` | out-of-process。本 milestone は farm 接続の **型** だけ提供。 |
| `MILESTONE_OCIO_INTEGRATION_2026-06-16.md` | output color space。本 milestone は並走。 |
| `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` | 別 topic。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **FFmpeg codec サポート**。`HEVC / VP9 / AV1 / ProRes` の FFmpeg ビルドフラグ。`vcpkg` 依存
2. **HAP codec**。HAP は FFmpeg 非標準。外部 library `libhap` 必要
3. **EXR multi-layer**。`OpenEXR` 依存。multi-layer は Phase 1 では single layer のみ
4. **Image sequence 連番桁数**。100,000 frame 越えのプロジェクトで 5 桁不足。桁数の default を 5 にする
5. **Audio-only job の video frame**。`writeFrame` を呼んでも `QImage` 経由なので破棄可能

### 6.2 契約上の未解決

- **OCIO color space と encoder の整合**。`MILESTONE_OCIO_INTEGRATION_2026-06-16.md` 完了後、`OutputPreset::colorSpace` を OCIO role で自動補完
- **HDR / Dolby Vision 出力**。本 milestone のスコープ外
- **Render Farm out-of-process**。Phase 8 で `M-RE-2` と統合
- **Custom encoder plugin**。`MILESTONE_OFX_PLUGIN_SUPPORT_2026-04-18.md` と並走
- **Audio normalize (loudness)**。`MILESTONE_OCIO_INTEGRATION_2026-06-16.md` 後に別 milestone 推奨

### 6.3 サブモジュール境界

- `ArtifactCore/src/Video/*` に新規ファイル追加
- 外部 library (FFmpeg / libhap / OpenEXR) は `vcpkg` または `find_package` 経由
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- `EncoderKind` 15 種すべてに encoder 実装
- `Image Sequence` で PNG / TIFF / EXR 書き出し
- `Audio-only` で WAV / MP3 / OGG / FLAC / Opus 書き出し
- `ProRes / HAP / WebM / AV1` の 4 codec 動作
- 5 形式の audio import 成功
- 9 種の built-in output preset
- preset dropdown で codec 設定が反映
- project 保存 → 再読込で復元
- 旧プロジェクトは H.264 / MP4 として開く
- Problem View に `encoder.*` 健全性表示
- 既存 `FFMpegEncoder` / `AbstractEncoder` API が温存
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.11 / §4 を正式 milestone に起こした。Render format expansion foundation。
