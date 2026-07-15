# Bug Investigation: ビデオレイヤーが黒く潰れる / 再生できなくなる (2026-07-08)

> 状態: 長期修正を実装（2026-07-16、ビルド・実機確認は未実施）
> 対象症状: ビデオレイヤーの表示が黒く潰れる（または黒点滅）、再生が止まる
> 関連: `docs/done/MILESTONE_VIDEO_LAYER_PLAYBACK_STABILITY_2026-06-25.md`

## 0. 2026-07-16 更新

7月8日時点の調査後、通常再生用と直接フレーム取得用のデマルチプレクサ／デコーダー分離、CPU表示パスでのVulkan decode無効化、描画スレッドのFuture待機撤去は既に反映されていた。今回、残っていた停止・要求寿命・メタデータ依存を次の構造へ更新した。

- `ArtifactVideoLayer` の直接取得を単一ワーカーへ集約し、未処理要求は最新1件だけ保持する。
- 世代IDに加えて要求IDを導入し、停止・シーク後に古いデコード結果が表示を上書きしないようにする。
- Stopはワーカー完了を待たず旧世代を無効化し、Compositionの開始フレームだけを最終要求にする。
- 同一フレームの失敗を描画ごとに無限再試行せず、別フレーム移動または世代更新まで最後の正常フレームを保持する。
- `MediaPlaybackController` は `avg_frame_rate` が欠落した素材で `av_guess_frame_rate`、`r_frame_rate`、フレーム数÷durationの順にFPSを補完する。
- 直接デコード用のstream index/time baseを独立保持し、非ゼロ`start_time`をseekとPTS比較の双方へ反映する。
- `ArtifactPlaybackService` とCompositionのplay/pause/stop状態を同期し、Composition stopから動画レイヤーのデコード世代を無効化する。

以下の節は、修正前の原因追跡記録として残す。

---

## 1. デコード／描画アーキテクチャ（現状）

- 描画は **CPU パスのみ** を消費する:
  - `ArtifactVideoLayer::draw()` — `Artifact/src/Layer/ArtifactVideoLayer.cppm:1918`
  - 取得経路: `cachedFrameImageBuffer()` (:1965) → `currentFrameImageBuffer()` (:1967) → `impl_->currentFrameBuffer_`（`ImageF32x4_RGBA`、CPU）
  - `GpuVideoFrame` ペイロードは `decodedVideoFrameHasGpuPayload()` で「あるか」だけ判定され、**draw は一切消費しない**。
- デコード基盤は `MediaPlaybackController`（= `impl_->playbackController_`）が持ち、内部で `MediaImageFrameDecoder videoDecoder_` を使う。
- 最近のリファクタ `a5e5dc4 feat_video_raw_decode_and_qimage_retirement`（ArtifactCore）で `MediaPlaybackController` / `FFMpegVideoDecoder` / `MediaImageFrameDecoder` が大幅書き換えされている（QImage 廃止、direct-decode パス新設）。

## 2. 根本原因候補（優先順）

### A. 並行デコードの競合（最有力：黒潰れ＋再生停止の両方を説明）

単一の `MediaImageFrameDecoder videoDecoder_` を **2 つのデコードパスが共有**し、排他が不十分。

- 再生パス: `MediaPlaybackController::getNextVideoFrameRaw()` — `ArtifactCore/src/Media/MediaPlaybackController.cppm:880`
  - `mediaReader_` + `videoDecoder_->sendPacket()` / `receiveFrameRaw()` を呼ぶ。**排他ロックなし**。
- 直接/シークパス: `MediaPlaybackController::getVideoFrameAtFrameDirectRaw()` — `:962`
  - `mediaSource_`（生 AVFormatContext）を自前でシーク/読み進め、`videoDecoder_->sendPacket()` / `receiveFrameRaw()` を呼ぶ。ここだけ `directDecodeMutex_`（:988）で守られている。
- 呼び出し側: `ArtifactVideoLayer::decodeCurrentFrame()` は **バックグラウンドスレッド**（QtConcurrent、`:1341`）から直接パスを呼ぶ。さらに `goToFrame()` のルックアヘッドプリフェッチ（`:2024`）も背景スレッドから直接パスを呼ぶ。

→ 再生中に描画/シーク/プリフェッチが背景スレッドで直接パスを起動すると、**同じ `AVCodecContext` へ `send_packet` / `receive_frame` が並行して入り、デコーダが破損/AVERROR を返す**。結果:
  - フレーム欠落・破損 → 黒点滅
  - デコーダがエラー状態に → "DECODE FAILED"（`:1385` の警告） → **再生停止**
- 副次的: 直接パスは `mediaSource_` のフォーマットコンテキストをシークするが、再生パスは `mediaReader_` から読む。両者が同一ファイルのデマルチプレクサ状態を取り合い → 再生位置が破壊され停止/デュプリ。

### B. Vulkan HW デコード有効化が CPU 描画パスと噛み合わない（潜伏的土地雷）

- `ArtifactVideoLayer::draw()` は `:1923` で `playbackController_->setVulkanDevice(...)` を呼び、以降 `vulkanDeviceConfigured_ = true`（:1938）。
- `MediaImageFrameDecoder::setVulkanDevice()` — `MediaImageFrameDecoder.cppm:291` が `hwDeviceCtx_` を保存。
- `initialize()`（`:338`）は `hwDeviceCtx_` があれば `codecContext_->hw_device_ctx` をセットし（:375-377）、`get_format = chooseBestDecoderPixelFormat`（:373）が `hw_device_ctx` あり時は `AV_PIX_FMT_VULKAN` を優先（:162-176）。
- その後デコードは Vulkan HW フレームを返し、`downloadHwFrameToCpuVideoFrame()`（:117）で CPU に落とす必要がある。**この `av_hwframe_transfer_data` が失敗すると（警告 :131）`std::monostate` を返し、CPU バッファが空 → 黒表示・表示不可**。

顕在化条件: `setVulkanDevice` は最初の `draw()` で呼ばれるが、その時点では codec context は既に開かれているため即時には発動しない。**ただしメディア再オープン / Interpret Footage / プロキシ差し替え等で `initialize()` が再走すると Vulkan HW デコードに切り替わり、上記ダウンロード失敗経路で黒化する。** 「再生できなくなった」はこの再オープン後症状と一致しやすい。

### C. draw がバックグラウンドデコード完了をブロック取得（黒点滅・フリーズ）

- `currentFrameImageBuffer()` は `:1424` で `impl_->decodeFuture_.result()` を呼び、**描画（レンダー）スレッドをブロック**してバックグラウンドデコード完了を待つ。
- 高速シークや生成(世代)不一致で `currentFrameBuffer_` が空のままになると、そのフレームは空 → 黒点滅。またレンダースレッドのブロックは UI フリーズにも直結。

### D. 色チャネル反転（黒潰れではなく別症状だが要注意）

- `cpuVideoFrameToImageF32x4_RGBA()` の RGB24 分岐は `cv::cvtColor(RGB2BGRA)` を使う（`:227-233`）。
- FFmpeg の `AV_PIX_FMT_RGB24` はメモリ上 R,G,B 順。cvtColor(RGB2BGRA) は R/B を入れ替えて BGRA を作る。レンダラが RGBA 順を期待する場合、**赤/青が反転**する（黒潰れではなく色ずれ）。症状が「暗い」ではなく「色がおかしい」場合はこちらを疑う。

## 3. 推奨される追加調査・診断手順

1. 再現時に以下ログを確認:
   - `[MediaImageFrameDecoder] Vulkan frame download failed`（:131）→ 候補 B の顕在化。
   - `[VideoLayer] DECODE FAILED`（ArtifactVideoLayer.cppm:1385）→ 候補 A のデコーダエラー。
   - `[VideoLayer] async decode null frame`（:1449）→ 世代不一致 / 空フレーム。
2. 環境変数 `ARTIFACT_ENABLE_VULKAN_VIDEO_DIRECT` を unset / set の両方で比較（directVulkanVideoFramesEnabled() は :263）。
3. シングルスレッド再生（プリフェッチ・背景デコードを無効）で症状が消えるか確認 → 候補 A の確定。

## 4. 修正案の方向性（実装は別タスク）

- **A の解決**: `videoDecoder_` への全アクセス（再生パス + 直接パス）を単一ミューテックスで直列化、または直接パス専用の別デコーダインスタンスを持たせる。デマルチプレクサも再生リーダと直接パスで共有しない（直接パスは再生リーダのシークを使うか、再生一時停止中のみ許可）。
- **B の解決**: `draw()` は実際に `GpuVideoFrame` を描画するパスがない限り Vulkan HW デコードを有効化しない（CPU 専用パスなら `setVulkanDevice` を呼ばない）。再オープン時の `hw_device_ctx` 設定を CPU 互換に留める。
- **C の解決**: `draw()` から `decodeFuture_.result()` のブロッキング取得を除去し、前フレームの最終良好バッファを維持して空フレーム時は黒点滅ではなくホールドする。
- **D の解決**: RGB24→BGRA の cvtColor 方向を見直し、レンダラの期待チャネル順と一致させる。

## 5. 関連ファイル

- `Artifact/src/Layer/ArtifactVideoLayer.cppm`（draw / decodeCurrentFrame / currentFrameImageBuffer / goToFrame）
- `ArtifactCore/src/Media/MediaPlaybackController.cppm`（getNextVideoFrameRaw / getVideoFrameAtFrameDirectRaw）
- `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`（setVulkanDevice / receiveFrameRaw / downloadHwFrameToCpuVideoFrame / chooseBestDecoderPixelFormat）
- `ArtifactCore/src/Codec/FFMpegVideoDecoder.cppm`（decodeNextVideoFrameRaw / makeCpuVideoFrameFromFrame）
- `ArtifactCore/include/Video/VideoFrame.ixx`（DecodedVideoFrame variant / GpuVideoFrame / CpuVideoFrame）
- 関連コミット: `a5e5dc4 feat_video_raw_decode_and_qimage_retirement`
