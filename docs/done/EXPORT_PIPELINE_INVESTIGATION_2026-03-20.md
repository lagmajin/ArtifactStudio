# 動画出力パイプライン調査レポート

> 2026-03-20 調査

## 概要

動画出力パイプラインは**大枠が完成済み**。FFmpeg エンコーダー、レンダーキュー UI、フォーマットプリセットが揃っており、残差は主に統合・配線レベル。

---

## ファイル一覧と状態

### エンコーダーコア (ArtifactCore)

| ファイル | 行数 | 状態 |
|---|---|---|
| `ArtifactCore/include/Video/FFMpegEncoder.ixx` | 829 | ✅ 完成 — H.264/H.265/ProRes/VP9/MJPEG |
| `ArtifactCore/src/Image/FFmpegEncoder.cppm` | 829 | ✅ 実装済み |
| `ArtifactCore/src/Image/FFmpegEncoder.Helpers.cppm` | 177 | ✅ コーデック/コンテナ検証ヘルパー |
| `ArtifactCore/src/Image/FFmpegEncoder.Test.cppm` | 577 | ✅ テストスイート |
| `ArtifactCore/include/Video/EncoderSetting.ixx` | 197 | ✅ プリセット (YouTube, ProRes, Web) |
| `ArtifactCore/include/Video/AbstractEncoder.ixx` | 30 | ❌ スタブ |
| `ArtifactCore/include/Video/FFmpegAudioEncoder.ixx` | 60 | ❌ スタブ |
| `ArtifactCore/include/Video/GStreamerEncoder.ixx` | 58 | ❌ スタブ |
| `ArtifactCore/include/Codec/MFEncoder.ixx` | 80 | ⚠️ 部分実装 |

### レンダーキューサービス (Artifact)

| ファイル | 行数 | 状態 |
|---|---|---|
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | **1739** | ✅ **メインエンジン** — ジョブ管理、フレームレンダリング、エンコーダーバックエンド |
| `Artifact/include/Render/ArtifactRenderQueueService.ixx` | 145 | ✅ ジョブライフサイクル API |
| `Artifact/src/Render/ArtifactRenderQueuePresets.cppm` | 281 | ✅ **14プリセット** |
| `Artifact/src/Render/ArtifactRenderScheduler.cppm` | 488 | ✅ スレッドプール、タスク優先度、パラレル戦略 |

### レンダーキューUI (Artifact)

| ファイル | 行数 | 状態 |
|---|---|---|
| `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp` | **1797** | ✅ **完全実装** — ジョブ一覧、検索/フィルタ、進捗、開始/停止 |
| `Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm` | 420 | ✅ 出力パス、フォーマット、コーデック、解像度、フレームレート |
| `Artifact/src/Widgets/Render/ArtifactRenderQueuePresetSelector.cppm` | 265 | ✅ プリセット選択ダイアログ |

---

## 対応フォーマット

### 動画コーデック

| コーデック | CRF | プリセット | プロファイル |
|---|---|---|---|
| H.264 | ✅ | ✅ | ✅ |
| H.265 | ✅ | ✅ | ✅ |
| ProRes | — | — | ✅ (proxy/lt/422/hq/4444) |
| VP9 | ✅ | ✅ | — |
| MJPEG | — | — | — |

### コンテナ

| フォーマット | 状態 |
|---|---|
| MP4 | ✅ |
| MOV | ✅ |
| MKV | ✅ |
| AVI | ✅ |
| WebM | ✅ |

### 画像シーケンス

| フォーマット | 状態 |
|---|---|
| PNG (8bit/16bit) | ✅ |
| JPEG | ✅ (エンコーダー側) |
| TIFF | ✅ (エンコーダー側) |
| EXR | ⚠️ TIFF フォールバック |

### ビルトインプリセット (14種)

YouTube 1080p/4K、ProRes 422/4444、H.264/H.265 Web、DNxHD、WMV、AVI無圧縮、WebM VP9、PNG/JPEG/TIFF/BMP/EXR シーケンス

---

## エクスポートワークフロー

```
ユーザー [START RENDER] クリック
  ↓
RenderQueueManagerWidget (UI)
  ↓ signal
ArtifactRenderQueueService::startAllJobs()
  ↓ Worker Thread
各ジョブを順次処理:
  ├── createVideoEncodeBackend(job)
  │    ├── NativeFFmpegBackend (FFmpeg C API) ← 優先
  │    └── PipeFFmpegExeBackend (QProcess) ← フォールバック
  │
  ├── renderSingleFrameComposition(job, frameNumber)
  │    └── QPainter ベースのソフトウェアコンポジット
  │        (背景 + レイヤー + 変換 + ブレンド + 不透明度)
  │
  ├── videoBackend->addFrame(qimg, frameIndex)
  │    └── FFmpegEncoder::addImage() → YUV変換 → エンコード
  │
  └── 進捗報告 → QMetaObject::invokeMethod
      ↓
  allJobsCompleted signal
```

---

## 主要な差分

### Critical (出力が機能しない)

| # | 問題 | 優先度 |
|---|---|---|
| 1 | **コンポジションレンダリングが QPainter ベース** — ビューポートの Diligent GPU レンダリングと出力が一致しない | **高** |
| 2 | **オーディオエンコーダーがスタブ** — `FFmpegAudioEncoder` の `openEncoder()` / `closeEncoder()` が空。音声多重化なし | 中 |
| 3 | **画像シーケンスが PNG のみ** — JPEG/TIFF/EXR シーケンス出力がエンコーダー側にあるがレンダーループに未接続 | 中 |

### Important (品質/使い勝手)

| # | 問題 | 優先度 |
|---|---|---|
| 4 | **パイプバックエンドがコーデック設定を無視** — `libx264` 固定 | 低 |
| 5 | **EXR 出力が TIFF フォールバック** | 低 |
| 6 | **ハードウェアアクセラレーションなし** (NVENC/AMF/QSV) | 低 |
| 7 | **ジョブキューセーブなし** — プロジェクト保存時にキューが永続化されない | 低 |

### コード品質

| # | 問題 |
|---|---|
| 8 | 古いコメント (`// Temporarily void* to bypass build error`, `// import Encoder.FFmpegEncoder; // Temporarily disabled`) が残存 |
| 9 | `AbstractEncoder.ixx` が空スタブ |

---

## 結論

動画出力の**インフラはほぼ完成**。FFmpeg C API エンコーダー、14プリセット、完全なレンダーキュー UI が揃っている。

残差は:
1. **QPainter ベースのレンダリング**を Diligent GPU パイプラインに統合 (または QPainter 出力の品質を保証)
2. **オーディオエンコーダーの実装**
3. **画像シーケンス形式の拡充**

の3点。アーキテクチャ自体は揃っているため、統合・配線作業が主。
