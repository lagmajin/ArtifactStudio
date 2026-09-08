# Media / Video / Codec / IO 詳細監査

**日付**: 2026-08-02
**最終更新:** 2026-09-07
**調査範囲**: ソースコード直接読み込み（~50ヘッダ）

---

## 1. MediaReader — メディア読み込み 🟢 85%

`Media/include/MediaReader.ixx`

| 機能 | 状態 |
|------|------|
| FFmpeg libavformat 使用 | ✅ 生 AVPacket をキューイング |
| ビデオ/オーディオ分離 | ✅ Video/Audio/Unknown ストリーム |
| ワーカースレッド | ✅ `std::thread` + `QMutex` + `QWaitCondition` |
| 開始/停止/一時停止 | ✅ |
| パケット単位アクセス | ✅ `getNextPacket(StreamType)` |

コードはスタンダードな FFmpeg demuxer。Pause/Stop 制御が揃っている。

---

## 2. FFmpegVideoDecoder — ビデオデコード 🟡 60%

`Video/include/FFMpegVideoDecoder.ixx`

| 機能 | 状態 |
|------|------|
| libavcodec デコード | ✅ |
| フレーム取得 | ✅ |
| シーク | ✅ |

**2026-09-07対応印**: 現行コードでは`receive_frame`のEOF／エラー処理と`send_packet`の戻り値処理があり、致命エラー時に無条件`continue`する旧記述は現行コードと一致しない。AudioDecoderもEOF時のdrainと送信エラー処理を実装済み。実ファイルでのruntime受入れは未確認。

---

## 3. FFmpegEncoder — ビデオエンコード 🟢 80%

`Video/include/FFMpegEncoder.ixx`

| 機能 | 状態 |
|------|------|
| コーデック | ✅ H.264/H.265/ProRes |
| ハードウェアエンコード | ✅ `preferHardware` フラグ。NVENC/AMF/QSV |
| HDR | ✅ `HDRColorSpace` (SDR_BT709/HDR10_PQ/HLG_BT2020)。`hdrNominalPeak` |
| 品質制御 | ✅ CRF + preset + profile |
| フレームレート | ✅ `fps` |
| コンテナ | ✅ `container` (mp4/mov/mkv) |
| sws_scale | ✅ 品質レベル指定あり（Lanczos/Spline/Bilinear） |

---

## 4. ImageExporter — 画像出力 🟢 85%

`IO/Image/include/ImageExporter.ixx`

| 機能 | 状態 |
|------|------|
| OpenImageIO 使用 | ✅ `OIIO::ImageBuf` 対応 |
| QImage 対応 | ✅ |
| 非同期書き出し | ✅ `std::future<ImageExportResult> writeAsync()` |
| マルチチャンネル(AOV) | ✅ `writeMultiChannel(MultiChannelImage)` |
| エラー報告 | ✅ `ImageExportResult { success, errorStage, errorMessage }` |

---

## 5. MediaPlaybackController — 再生制御 🟡 60%

`Media/include/MediaPlaybackController.ixx`

**既知の問題**: 成熟度分析 — UI スレッドで `std::this_thread::sleep_for` / `yield` を実行（ビデオパス残存）

---

## 6. Video/Transition システム 🟢 85%

| トランジション | 状態 |
|--------------|------|
| BlockDissolve | ✅ |
| Crossfade | ✅ |
| Cube | ✅ |
| Dissolve | ✅ |
| Doors | ✅ |
| Flip | ✅ |
| GlitchDisplace | ✅ |
| GradientWipe | ✅ |
| IrisWipe | ✅ |
| LightLeak | ✅ |
| LinearWipe | ✅ |
| RadialWipe | ✅ |
| Slide | ✅ |
| Spin | ✅ |
| Wipe | ✅ |
| Zoom | ✅ |
| **TransitionFactory** | ✅ |

15種のトランジション。重複やデッドコードはなく、各トランジションが独立して定義されている。

---

## 7. Stabilizer — 🟡 部分実装（2026-09-07現行コード照合）

`Video/include/Stabilizer.ixx`

旧成熟度分析での報告:
- `BatchStabilizer` が I/O 完全スキップして `return true`（スタブ）
- Harris corner response の公式が誤り

現行コードでは、`BatchStabilizer::process()` が入力検証、画像読込、`VideoStabilizer` 実行、出力保存、進捗通知を行い、Harris response も `det - 0.04 * trace * trace` の公式になっている。したがって「完全に機能しないスタブ」は現行コードと一致せず、単一画像処理の実装済み、動画シーケンスI/Oとruntime受入れは未確認として扱う。

---

## 8. 問題点まとめ

| コンポーネント | スコア | 所見 |
|---------------|--------|------|
| MediaReader | 🟢 85% | FFmpeg demuxer。安定 |
| FFmpegVideoDecoder | 🟡 部分対応（2026-09-07） | EOF／送信・受信エラー処理、stride範囲検証、FPS fallbackを確認。runtime未確認 |
| FFmpegAudioDecoder | 🟡 部分対応（2026-09-07） | EOF drainとfatal error処理を確認。runtime未確認 |
| FFmpegEncoder | 🟢 80% | H.264/H.265/ProRes + HDR。完成度高い |
| ImageExporter | 🟢 85% | OIIO + QImage + 非同期 + MultiChannel |
| MediaPlaybackController | 🟡 60% | UI スレッド sleep 問題 |
| Transition システム | 🟢 85% | 15種。クリーン |
| Stabilizer | 🟡 部分対応（2026-09-07） | BatchStabilizerの単一画像I/O・安定化処理は実装済み。動画シーケンス処理とruntime未確認 |
| GStreamer | 🟡 30% | インターフェースはあるが使用実績不明 |
| MFEncoder/MFFrameExtractor | 🟡 30% | MediaFoundation バックエンド。状態不明 |

**総合**: 🟡 65%

---

## 9. 次モジュール予告

依存順で次は **Audio層**（~30モジュール）→ **Text/Font層** → **Animation層** と続く。
