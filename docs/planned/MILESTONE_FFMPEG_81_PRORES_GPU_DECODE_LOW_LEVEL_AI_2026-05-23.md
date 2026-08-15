# FFmpeg 8.1+ ProRes GPU Decode - Low Level AI Implementation Milestone

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

Vulkan `AVHWDeviceContext`／`AVHWFramesContext` の初期化、`AV_PIX_FMT_VULKAN` 優先選択、HW frame の受信・CPU download、`GpuVideoFrame` → `GPUTextureCacheManager` → Diligent texture の橋渡しは実装済みです。direct decoder と通常 playback の分離、色メタデータ伝搬、CPU fallback も確認できます。

ただし `directVulkanVideoFramesEnabled()` は false 固定で、Qt preview backend は GPU frame を提示できず、実際の再生は CPU frame 経路へ戻ります。ProRes の capability probe／診断、timeline semaphore ownership bridge、汎用 HW frame handle、VFR/HFR seek と stride／音声の低レベル課題は未完了・未検証です。現状は **GPU decode foundation は存在するが native GPU decode は無効**です。

**Date**: 2026-05-23  
**Last Updated**: 2026-07-21 (コード実態調査に基づき更新)  
**Status**: In Progress (Vulkan HW decode path partially built, intentionally disabled)

**Related**:

- `docs/planned/MILESTONE_FFMPEG_GPU_DECODE_BACKEND_2026-03-28.md`
- `docs/shared/ai-tech-memos/FFMPEG_8_1_PRORES_AND_VCPKG_2026-04-21.md`
- `Artifact/docs/MILESTONE_VIDEO_QIMAGE_RETIREMENT_2026-04-15.md`

---

## Goal

Add a real FFmpeg hardware decode path for Apple ProRes when FFmpeg 8.1+ and
the platform backend support it, while keeping the current CPU decode path as
the stable fallback.

For this milestone, "GPU decode" means FFmpeg hardware frames through
`AVHWDeviceContext` / `AVHWFramesContext`, not merely uploading CPU-decoded
frames to a GPU texture afterward.

---

## Current State

Observed in the current codebase:

1. Local vcpkg install contains `ffmpeg_8.1_x64-windows.list`.
2. Public playback backend selection currently has only:
   - `DecoderBackend::FFmpeg`
   - `DecoderBackend::MediaFoundation`
3. `GpuVideoFrame` exists as a type, but it only contains a Vulkan handle variant
   today.
4. `FFmpegPlaybackBackend` currently rejects GPU frames in the Qt preview path.
5. FFmpeg video decode still uses `avcodec_find_decoder(codec_id)` and CPU
   `sws_scale` conversion paths.

This means the dependency may be new enough, but the application-level GPU
decode path is not implemented.


---

## 2026-07-21 Implementation Status (コード実態調査)

### 実装済みの GPU Decode 基盤

#### D1 (Capability Probe) — 部分的に実装

- ✅ `MediaImageFrameDecoder::setVulkanDevice()` (`MediaImageFrameDecoder.cppm:296-341`) — Vulkan device context 作成 (`AV_HWDEVICE_TYPE_VULKAN`)
- ✅ `chooseBestDecoderPixelFormat()` (`MediaImageFrameDecoder.cppm:162-176`) — `hw_device_ctx` あり時は `AV_PIX_FMT_VULKAN` 優先
- ✅ `initialize()` (`MediaImageFrameDecoder.cppm:343-406`) — `AVCodecContext::hw_device_ctx` 設定済み
- ❌ `avcodec_get_hw_config()` による ProRes ハードウェア対応の自動検出は未実装
- ❌ 診断ログに「GPU decode 試行の有無・失敗理由」は未出力

#### D2 (Hardware Decoder Context) — 実装済みだが無効化

- ✅ `downloadHwFrameToCpuVideoFrame()` (`MediaImageFrameDecoder.cppm:123-146`) — Vulkan HW frame → CPU frame 転送
- ✅ `receiveFrameRaw()` — HW frame 受信 → `downloadHwFrameToCpuVideoFrame()` 経由で CPU に落とす
- ⚠️ **`directVulkanVideoFramesEnabled()` が `false` をハードコード** (`MediaImageFrameDecoder.cppm:269-277`)
  - 理由: AVVkFrame timeline semaphore の ownership/synchronization bridge が未実装
  - 結果: HW decode 経路はコード上存在するが、常に CPU パスが使用される

#### D3 (GpuVideoFrame Handle) — 部分的に実装

- ✅ `GpuVideoFrame` + `VulkanVideoFrameHandle` (`VideoFrame.ixx`) — Vulkan image + layout + format
- ✅ `GPUTextureCacheManager::acquireOrCreate(..., GpuVideoFrame)` (`GPUTextureCacheManager.ixx:73-75`) — Vulkan ネイティブイメージのラップ（`CreateTextureFromVulkanImage`）
- ❌ `FFmpegHardwareFrameHandle` のような汎用 HW frame ハンドル型は未実装（Vulkan 固有）

#### D4 (Renderer Bridge) — 実装済み

- ✅ `GPUTextureCacheManager` が `GpuVideoFrame` → Diligent texture のブリッジ (`GPUTextureCacheManager.cppm:271-373`)
- ✅ Composition view GPU ブランチは `toQImage()` を通らず直接 SRV バインド
- ✅ CPU fallback パスは維持（`draw()` は `ImageF32x4_RGBA` を消費）

### 全体アーキテクチャ上の改善点 (2026-07-21 時点)

| 項目 | 状態 |
|---|---|
| direct decode パス分離 | ✅ `directVideoDecoder_` + `directMediaSource_` が再生パスと独立 |
| FPS 補完チェーン | ✅ `avg_frame_rate` → `av_guess_frame_rate` → `r_frame_rate` → `フレーム数÷duration` |
| カラーメタデータ伝搬 | ✅ AVFrame から `colorspace/range/primaries/trc` を伝搬 |
| Vulkan HW decode 安全化 | ✅ 明示的無効化 + Vulkan フレームスキップ |
| サムネイル抽出のリソース管理 | ✅ cleanup で全リソース解放（リーク修正済み） |
| 並行デコード競合 | ✅ 分離デコーダ + `directDecodeMutex_` |

### 残存する低レベル課題

| 課題 | ファイル:行 | 影響 |
|---|---|---|
| stride 計算 int overflow | `FFMpegVideoDecoder.cppm:69` | 4K+ で buffer overrun |
| VFR/HFR シーク | `FFMpegVideoDecoder.cppm:311-314` | 高 FPS 素材でシーク不能 |
| Audio decoder 致命エラー後 flush なし | `FFMpegAudioDecoder.cppm:305-307` | 音声デコード継続破損 |
| UI スレッド sleep (video パス) | `MediaPlaybackController.cppm:1016-1020` | 再生中 UI ラグ |
| `av_rescale_rnd` int64→int truncation | `FFMpegAudioDecoder.cppm:379` | 長時間音声でバッファ誤り |

### 次の一手

- **P0**: Vulkan timeline semaphore bridge 実装 → `directVulkanVideoFramesEnabled()` を有効化可能に
- **P0**: stride overflow 修正 (`width * 3` → `static_cast<int64_t>(width) * 3`)
- **P1**: VFR/HFR シーク修正（`avg_frame_rate` ベース timebase を使用）
- **P1**: HW decode 対応コーデックの自動検出（`avcodec_get_hw_config` + 診断ログ）

---

## First Files

Read these before editing:

1. `ArtifactCore/include/Media/MediaPlaybackController.ixx`
2. `ArtifactCore/src/Media/MediaPlaybackController.cppm`
3. `ArtifactCore/src/Codec/FFMpegVideoDecoder.cppm`
4. `ArtifactCore/include/Video/VideoFrame.ixx`
5. `Artifact/src/Layer/ArtifactVideoLayer.cppm`
6. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
7. `Artifact/include/Render/GPUTextureCacheManager.ixx`
8. `Artifact/src/Render/GPUTextureCacheManager.cppm`

---

## Non-Goals

- Do not remove CPU decode.
- Do not make GPU decode the default until capability detection and fallback are
  visible in diagnostics.
- Do not pretend all ProRes variants are hardware-decodable on every platform.
- Do not add a renderer bridge that only works for one graphics backend without
  naming that limitation.
- Do not silently convert hardware frames through CPU and call it GPU native.

---

## Backend Contract

Add an explicit backend policy before implementing decode internals.

Suggested shape:

```cpp
enum class DecoderBackend {
  FFmpeg,
  MediaFoundation,
  FFmpegGpu
};

enum class DecodeBackendPolicy {
  Auto,
  Cpu,
  Gpu
};
```

If keeping the enum small is preferred, add the policy elsewhere and leave
`DecoderBackend` as the selected concrete backend. The important contract is:

- `Auto`: try GPU only when capability probe says it is supported; otherwise CPU.
- `Cpu`: never create a hardware device.
- `Gpu`: request hardware decode and report a clear failure if unavailable.

---

## Platform Notes

Expected FFmpeg hwdevice targets to probe:

- macOS: `AV_HWDEVICE_TYPE_VIDEOTOOLBOX`
- Windows: `AV_HWDEVICE_TYPE_D3D11VA` or `AV_HWDEVICE_TYPE_D3D12VA` if available
- Linux: `AV_HWDEVICE_TYPE_VAAPI` or `AV_HWDEVICE_TYPE_VULKAN`

Apple ProRes GPU-native decode is platform and FFmpeg-build dependent. The AI
must probe runtime capability instead of assuming support from FFmpeg version
alone.

---

## Implementation Phases

### Phase D1 - Capability Probe And Diagnostics

Target files:

- `ArtifactCore/src/Media/MediaPlaybackController.cppm`
- `ArtifactCore/include/Media/MediaPlaybackController.ixx`
- Possibly a new small helper in `ArtifactCore/src/Media/`

Tasks:

1. Add a capability probe function that checks:
   - FFmpeg runtime version
   - codec id `AV_CODEC_ID_PRORES`
   - available hardware device types
   - decoder hardware configs via `avcodec_get_hw_config`
2. Log selected backend, codec, hwdevice, pixel format, and fallback reason.
3. Surface the selected backend in playback metadata / debug state if a local
   field already exists.

Done criteria:

- Diagnostics can answer: "Did we attempt GPU ProRes decode? If not, why?"
- CPU playback behavior remains unchanged.

### Phase D2 - Hardware Decoder Context

Target files:

- `ArtifactCore/src/Codec/FFMpegVideoDecoder.cppm`
- Or a new `ArtifactCore/src/Media/FFmpegGpuVideoDecoder.cppm` if isolation is
  cleaner.

Tasks:

1. Create `AVBufferRef* hw_device_ctx` with the selected hwdevice type.
2. Attach it to `AVCodecContext::hw_device_ctx`.
3. Provide a `get_format` callback that selects the hardware pixel format
   reported by FFmpeg.
4. Keep CPU fallback open by returning software format when policy allows it.
5. Preserve seek / flush behavior.

Done criteria:

- Hardware decode path can open a ProRes stream when the FFmpeg build supports
  it.
- Failure falls back or reports clearly according to policy.

### Phase D3 - `GpuVideoFrame` Handle Expansion

Target file:

- `ArtifactCore/include/Video/VideoFrame.ixx`

Tasks:

1. Extend `GpuVideoFrame` handle variants beyond Vulkan-only.
2. Add enough metadata to describe:
   - storage kind
   - native handle pointer
   - pixel format
   - color info
   - synchronization requirement if known
3. Keep CPU frames unchanged.

Suggested direction:

```cpp
struct FFmpegHardwareFrameHandle {
  void* avFrame = nullptr;
  void* hwFramesContext = nullptr;
  int hwPixelFormat = 0;
};
```

Use ownership carefully. If storing `AVFrame*`, the wrapper must define clone /
free semantics or hold a ref-counted frame wrapper.

Done criteria:

- The decode layer can return a valid `GpuVideoFrame` without losing ownership
  of the underlying FFmpeg frame.

### Phase D4 - Renderer Bridge

Target files:

- `Artifact/src/Render/GPUTextureCacheManager.cppm`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

Tasks:

1. Add a bridge from `GpuVideoFrame` to the renderer texture cache.
2. If zero-copy is unavailable, implement a named low-copy path and expose that
   as a diagnostic.
3. Do not route GPU frames through `QImage` for the main preview/render path.
4. Keep Qt preview widgets using CPU fallback until they can present GPU frames.

Done criteria:

- Composition view can consume a GPU decoded frame through renderer texture
  infrastructure.
- Existing CPU video layers still render.

### Phase D5 - Playback And Seek Semantics

Target files:

- `ArtifactCore/src/Media/MediaPlaybackController.cppm`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`

Tasks:

1. Define reset behavior on `seek`, `pause`, `play`, and `stop`.
2. Ensure decoder flush also releases or invalidates hardware frames.
3. Ensure timeline frame number and decoded frame pts stay aligned.
4. Log when fallback switches from GPU to CPU during a media session.

Done criteria:

- Scrubbing a ProRes clip does not display stale hardware frames.
- Fallback does not leave the video layer in a mixed backend state.

---

## Low-Level AI Guardrails

- Search before editing:
  - `GpuVideoFrame`
  - `DecoderBackend`
  - `decodeFrameRaw`
  - `receiveFrameRaw`
  - `AV_CODEC_ID_PRORES`
  - `avcodec_find_decoder`
- Keep CPU software decode as the reference path.
- Treat FFmpeg version as insufficient proof of GPU ProRes support; always probe.
- Keep ownership explicit for `AVFrame`, `AVBufferRef`, and renderer texture
  handles.
- Do not add `QImage` to the GPU-native hot path.

---

## Verification

Only run these when build/test is explicitly allowed:

1. Build with current vcpkg FFmpeg.
2. Run capability probe on a machine with known FFmpeg 8.1+ runtime.
3. Open ProRes 422 and ProRes 4444 clips.
4. Confirm selected backend and fallback reason in diagnostics.
5. Scrub forward/backward and verify pts/frame alignment.
6. Confirm CPU fallback still works when GPU policy is disabled.
7. Confirm Qt-only preview path does not crash when GPU frames are produced.

---

## Completion Criteria

- The app can explicitly choose or auto-select a GPU FFmpeg decode backend.
- ProRes capability is detected at runtime and reported.
- GPU frames have a real ownership-safe representation.
- Composition render path can consume GPU decoded frames without `QImage`.
- CPU decode remains the fallback and regression baseline.


---

## Static audit follow-up (2026-07-25)

関連 backend の現状を再照合した結果、GpuVideoFrame は Vulkan handle variant と metadata を持ち、FFmpeg の Vulkan hwdevice／hardware frame 生成と CPU download fallback が存在する。ただし、ProRes 専用の runtime capability probe、FFmpegGpu／DecodeBackendPolicy の明示契約、D3D11VA／VideoToolbox／VAAPI の選択、Qt preview／renderer の GPU frame 消費は未完了である。

したがって本文書の D1 は部分実装、D2 は Vulkan 基盤のみ、D3〜D6 は未完了または検証待ちとする。FFmpeg 8.1 の存在だけでは対応証明にならないため、実機 capability probe と ProRes 422／4444 の runtime 検証が必要である。
