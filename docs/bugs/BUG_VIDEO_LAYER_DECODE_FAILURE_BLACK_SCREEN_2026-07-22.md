# Bug Report: VideoLayer デコード失敗・黒画面の調査 (2026-07-22)

> 状態: ソース修正済み（ビルド・実機確認は未実施）
> 対象症状: VideoLayer（FFmpeg 間接利用）がデコードに失敗し、プレビューが黒い画面になる
> 関連: `docs/bugs/BUG_VIDEO_LAYER_BLACK_AND_PLAYBACK_STOP_INVESTIGATION_2026-07-08.md`

## 1. 結論サマリ

### 1-1. ArtifactCore HEAD がコンパイル不能（修正済み）

`f0bb855 fix_video_frame_conversion_reconfiguration` (2026-07-21) で
`makeCpuVideoFrameFromFrame` のシグネチャが 5 引数 → 3 引数に変更されたが、
呼び出し側 1 箇所が旧形式のまま残っていた。

- **場所**: `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm:110`
  （`makeCpuVideoFrameFromDownloadedFrame` 内）
- **修正内容**:
  ```cpp
  // 修正前（5引数・コンパイルエラー）
  out = makeCpuVideoFrameFromFrame(
      frame, swsCtx, frame->width, frame->height, pts);
  // 修正後（定義 :61 に一致）
  out = makeCpuVideoFrameFromFrame(frame, swsCtx, pts);
  ```
- 残りの呼び出し箇所（:504, :601）は既に 3 引数形式で一致していた。
- この関数は Vulkan HW ダウンロード経路（`downloadHwFrameToCpuVideoFrame`）専用のため、
  SW デコード時の実行挙動には影響しないが、**再ビルドが必ず失敗する状態だった**。

### 1-2. 黒画面のランタイム発生経路（現行コード）

現行の描画チェーン:

```
ArtifactVideoLayer::draw() (:2436)
  → cachedFrameImageBuffer() / currentFrameImageBuffer() (:1891)
    → 非同期ワーカー (decodeCurrentFrame :1559)
      → MediaPlaybackController::getVideoFrameAtFrameDirectRaw() (:1066)
        → decodeVideoFrameDirectAtFrameRaw() (:260)
```

`decodeVideoFrameDirectAtFrameRaw()` が `std::monostate` を返すと
`decodedVideoFrameToImageF32x4_RGBA()` が空バッファを返し、
`currentFrameBuffer_` が一度も更新されないまま `draw()` が早期 return する。
**有効フレームがゼロの状態では真っ黒のまま**になる。

なお 7/8 調査時点の主要原因は現行コードで構造的に解消済み:

- 旧候補A（デコーダ並行アクセス競合）→ `directMediaSource_` / `directVideoDecoder_` /
  `directDecodeMutex_` による分離で解消。
- 旧候補B（Vulkan HW デコードと CPU パスの不整合）→ `draw()` からの
  `setVulkanDevice` 呼び出しが撤去され休眠中（:2439-2441 のコメント参照）。
- 旧候補C（draw のブロッキング待機）→ `currentFrameImageBuffer()` は
  ブロックせず非同期要求 + 現バッファ返却に変更済み。

## 2. 実行時の失敗ポイントとログ対応表

| # | ログ出力 | 原因 | 場所 |
|---|---------|------|------|
| 1 | `[MediaPlayback] direct decode skipped: invalid state` | 直接デコーダの open/init 失敗、または **fps_ == 0**（avg_frame_rate → av_guess_frame_rate → r_frame_rate → フレーム数÷duration の補完が全滅する素材: 一部 MKV/WebM・VFR 等） | MediaPlaybackController.cppm:272-284 |
| 2 | `[MediaPlayback] direct decode failed for frame ... packetsRead=... eof=...` | シーク後、targetPts に届く前にパケット読み切り（長 GOP・破損ストリーム）。maxPackets = max(512, waitAttempts×8) | :403-423 |
| 3 | `[MediaPlayback] sendPacket failed` | デコーダ内部エラー（AVCodecContext 破損） | :386-390 |
| 4 | `[MediaImageFrameDecoder] initialize failed: codec not found / could not open codec` | 未対応コーデック・open2 失敗 | MediaImageFrameDecoder.cppm:354-386 |
| 5 | `[MediaImageFrameDecoder] Vulkan frame download failed` | Vulkan HW フレームの CPU ダウンロード失敗（現行は `setVulkanDevice` 呼出し元なし＝休眠。誰かが呼ぶと顕在化） | :129-135 |
| 6 | `[VideoLayer] DECODE FAILED ... backendLastError=...` | レイヤ側最終失敗。`backendLastError` と `controller->getDebugState()` に詳細が出る | ArtifactVideoLayer.cppm:1732-1746 |

補足:

- レイヤ側は失敗した同一 requestId を **250ms 再試行抑制**する
  （`prepareDecodeRequest` :929-940、"decode-failed-retained"）。
  正常フレームを一度も持たない場合、この抑制中も黒のまま。
- `sws_getCachedContext` が変換不能なピクセルフォーマットの場合も
  無効 CpuVideoFrame → 失敗扱い（通常の yuv420p / 10bit 系は変換可能）。
- `cpuVideoFrameToImageF32x4_RGBA()` の RGB24 分岐は `cv::COLOR_RGB2BGRA`。
  黒ではなく「赤青反転」が出る場合はここを疑う（ArtifactVideoLayer.cppm:266-274）。

## 3. 切り分け手順

1. 再現時のコンソールログを上表と照合する。
   まず `[MediaPlayback] direct decode skipped: invalid state`（fps=0 か）
   と `[MediaPlayback] direct decode failed for frame`（シーク/読み切りか）を確認。
2. 実行バイナリがどのコミット時点のビルドか確認する。
   2026-07-21 の `f0bb855` 以降は本レポート 1-1 の修正がないとビルド不可。
3. 問題の素材で `ffprobe` の `avg_frame_rate` / `r_frame_rate` / `nb_frames` を確認し、
   fps 補完チェーン（MediaPlaybackController.cppm:464-476）が効くか検証する。

## 4. 関連ファイル

- `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`（今回の修正箇所 :110 / デコーダ本体）
- `ArtifactCore/src/Media/MediaPlaybackController.cppm`（直接デコードパス :260-435 / fps 補完 :464-476）
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`（draw :2436 / decodeCurrentFrame :1559 / currentFrameImageBuffer :1891）
- 関連コミット: `f0bb855`（シグネチャ変更元）、`8316238`、`f1bca2b`、`71521cc`

## 5. 未確認事項

- ビルド・実機確認は未実施（AGENTS.md ルールによりユーザー指示まで禁止）。
- 実行バイナリのビルド時点コミットが不明のため、実行時原因が上表のどれかは
  ログ確認が必要。
