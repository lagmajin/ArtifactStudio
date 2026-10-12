# ArtifactVideoPreviewBench

**最終更新:** 2026-10-13

Standalone, visible GPU Composition Viewport benchmark for video-only layer stacks. It opens the production `ArtifactCompositionEditor`, `ArtifactVideoLayer`, and `ArtifactPlaybackService`; it is not an FFmpeg-only decode speed test. It runs in its own process and sets an isolated temporary project-data root before application services initialize.

## Build

After configuring the main project, build the opt-in target:

```powershell
cmake --build <build-dir> --config Release --target ArtifactVideoPreviewBench
```

The executable is placed in `bin/Release` (or the selected configuration).

## Run

```powershell
.\bin\Release\ArtifactVideoPreviewBench.exe `
  --input D:\media\h264-long-gop.mp4 `
  --input D:\media\prores-422-hq.mov `
  --layer-counts 1,2,3,4,5,6,7,8 `
  --seconds 3 `
  --report D:\bench-results\video-preview.json
```

The input list is cycled when more layers are requested than input files. Use repeated paths to measure multiple layers sharing one source; provide distinct paths to compare independent-source decoding. Each requested layer count starts with a fresh layer stack and preview cache.

By default, each case builds the full requested RAM preview range first, then measures real-time playback with skip mode set to `None`. The default 3-second range stays below PlaybackService's current 128-frame RAM image budget. It passes only when PlaybackService records zero dropped frames, video timing diagnostics observe zero late/repeated frames, and playback advances through the requested range. `--no-ram-cache` disables RAM preview for a decode-sensitive comparison and permits longer runs. A cache build timeout or incomplete requested range fails the case instead of silently measuring a partial cache.

Use 30 fps source material for the default composition timing. Reports include the discovered source codec and source FPS. Generate or provide licensed test footage outside the repository; the program writes project state only under a temporary test data directory and writes a report only to the explicit `--report` path.

For an opt-in device-sharing and queue-interoperability probe, run one H.264 input with `--probe-shared-d3d12-device`. The benchmark initializes the Composition renderer, gives its native D3D12 device to FFmpeg, decodes eight frames, verifies each decoder texture belongs to that device, converts the NV12 planes to RGBA with compute work submitted through Diligent's immediate D3D12 queue, and validates one output pixel. This mode exits after the probe and does not run playback cases.

The production `MediaPlaybackController` direct decoder can also receive a retained native D3D12 device through `setD3D12Device()`. GPU-frame retention is an explicit per-call option on `getVideoFrameAtFrameDirectRaw(frame, true)`; the existing one-argument call still downloads to CPU. The standalone probe decodes frame 0 through this controller using the Composition renderer device, then runs the bounded queue conversion probe. On 2026-10-12 the controller returned a 1920x1080 D3D12 frame on the same device (`sameDevice=1`); the following ring converted and completed 8/8 frames with monotonic PTS and sample-pixel validation in 132 ms. This verifies device identity and queue interop only: the application layer does not yet pass its renderer device into its controller, the D3D12 NV12 texture is not connected to the product draw/cache path, and preview playback remains synchronous at the direct decode boundary. It does not establish a playback improvement or pass the strict zero-drop/late/repeat condition.

This benchmark requires an interactive desktop session and an available supported GPU backend. It was built and run on Windows 11, Qt 6.10.2, with an NVIDIA GeForce RTX 4070 Ti. Debug Qt runtime DLLs and platform/image-format plugins were present beside the executable. CMake's `windeployqt` step warned that it could not find debug Qt DLLs in the vcpkg `bin` directory, but the target build completed and the executable launched. H.264 hardware-decoder initialization fell back to FFmpeg software decode.

## Initial measured run (2026-10-12)

The generated 1080p30 H.264 (about 26 Mb/s) and ProRes 422 HQ (about 231 Mb/s) clips were three seconds long. These initial strict runs did **not** meet the zero-drop/late/repeated-frame condition:

| Codec | Layers | RAM preview | Dropped | Late | Repeated | Result |
|---|---:|---|---:|---:|---:|---|
| H.264 | 1 | Off | 1 | 6 | 6 | Failed |
| H.264 | 2 | Off | 0 | 6 | 4 | Failed |
| ProRes 422 HQ | 1 | Off | 1 | 5 | 1 | Failed |
| ProRes 422 HQ | 2 | Off | 3 | 3 | 4 | Failed |
| H.264 | 1 | On | — | — | — | RAM preview did not become ready (0/89 frames in 180 s) |

The report JSON files and generated clips are kept under the current user's temporary `ArtifactVideoPreviewBenchInputs-*` directory. These measurements are a baseline for this machine and harness, not a general performance guarantee.

## Follow-up diagnosis and optimization (2026-10-12)

The isolated benchmark was rerun after reducing frame-copy and pixel-conversion work. The sRGB8 upload conversion uses parallel row processing; H.264 RGB24-to-float conversion writes directly into a CV_32FC4 buffer and transfers ownership into `ImageF32x4_RGBA` instead of cloning that full buffer. Shared decoded frames are also reused by the frame cache and presentation buffer where available. The gated diagnostic switch `ARTIFACT_VIDEO_PREVIEW_PROFILE_UPLOAD=1` reports decode, CPU conversion, render submission, upload conversion, and texture creation timings to stderr.

On this machine, H.264 1080p30 without RAM preview still failed strict playback after these changes: 1 layer had 2 dropped, 7 late, and 7 repeated frames, advancing to frame 19/89 in 3.208 seconds. Profiling showed about 14 ms subsequent raw decode, 6.5 ms CPU conversion after ownership transfer, and about 10–12 ms upload conversion plus 2–4 ms texture creation per new frame. The initial frame decode was about 155 ms. H.264 720p30 also failed: 1 layer had 3/1/13 and 2 layers had 1/22/4 dropped/late/repeated frames. ProRes 422 HQ 1080p30 without RAM preview failed too: 1 layer had 2/10/16 and 2 layers had 1/8/7 dropped/late/repeated frames.

A RAM-preview-enabled follow-up did not produce a report before its process used about 1.1 GiB while preparing the range, so that run was stopped. This is not a passing result. The zero-drop/late/repeated-frame target remains unmet; current measurements point to decode plus per-frame upload/conversion costs, with substantial machine-load variance.

A repeat H.264 1080p30 run measured 1 layer at 1/1/13 and 2 layers at 0/10/5 dropped/late/repeated frames. Counts vary between runs, but both still fail the strict condition.

## Video texture reuse pool follow-up (2026-10-12)

The render path now keeps a bounded GPU texture slot per video layer and dimensions, updates the slot with `UpdateTexture`, and skips both upload conversion and transfer when the same frame is drawn again. It remains on the renderer-owned thread; no Diligent context calls were moved to a decoder worker. The existing texture cache limits the pool to 50 entries / 512 MiB.

With the pool enabled, H.264 1080p30 still failed strict playback. Two profiled 1/2-layer runs measured dropped/late/repeated as 1/1/10 and 3/7/8, then 1/2/13 and 3/6/4. The first slot creation was about 1 ms and subsequent texture updates about 0–1 ms, while each new frame still spent about 11 ms in CPU float-to-RGBA8 conversion. Render submission was about 12–15 ms. These runs suggest texture recreation was reduced, but do not establish a stable end-to-end improvement or satisfy the no-drop requirement.

The next async candidate is CPU pixel conversion only, with a small fixed number of owned upload buffers and stale-frame generation checks. Decoder/render GPU resource operations must stay on their existing owner thread. A per-frame RGBA8 sidecar for the entire RAM cache is not acceptable without a separate memory budget: it would add roughly 8 MiB per 1080p frame. The benchmark currently does not measure this async design.

## Bounded upload scratch buffer follow-up (2026-10-12)

The synchronous video upload conversion now reuses one render-thread scratch buffer, retaining at most 64 MiB of RGBA8 capacity. Larger frames use a one-shot temporary buffer and do not expand the retained slot. This reduces repeated heap allocation for common 1080p/4K playback, but conversion still runs synchronously on the renderer thread; profile time remained about 9–11 ms per new 1080p frame.

Two D3D11VA H.264 1080p30 A/B runs after this change still failed the benchmark's zero-drop/late/repeated condition. Run 1 measured 1 layer at 2/1/6 and 2 layers at 0/13/3; run 2 measured 1 layer at 2/1/6 and 2 layers at 3/7/3 dropped/late/repeated frames. The fixed-size scratch reuse did not demonstrate end-to-end playback improvement.

An asynchronous-conversion prototype converts decoded and prefetched frames on the existing decode worker, keeps up to four RGBA8 frames within a 64 MiB per-layer cache, and exposes readiness separately from the decoded float-frame cache. The render path can pass a prepared buffer to the renderer so Diligent texture creation/update stays on the renderer owner thread. Source-version changes clear the converted-frame cache. The affected modules compiled. A reduced-incremental manual link completed after the ordinary link attempt approached 10 GiB; the resulting executable was run. D3D11VA H.264 1080p30 with four upload slots measured 1 layer at 1/1/10 and 2 layers at 2/6/2 dropped/late/repeated frames, advancing only to frames 24 and 14 respectively in about three seconds. Strict playback remains unverified as passing and in fact failed these runs.

The cache now also has a bounded pool of up to four evicted upload buffers and 64 MiB total. Only buffers whose shared ownership has returned to the layer are recycled; retained buffers are never mutated while the renderer may still read them. This avoids some repeated buffer allocation, but conversion still executes synchronously on the serialized decode worker. No runtime result for the pool revision has been collected yet.

The buffer-pool revision compiled and was run with the same H.264 1080p30 clip. Both decoder modes still failed the strict condition:

| Decode | Layers | Dropped | Late | Repeated | Final frame | Measured |
|---|---:|---:|---:|---:|---:|---:|
| D3D11VA | 1 | 2 | 1 | 11 | 30/89 | 3.430 s |
| D3D11VA | 2 | 0 | 7 | 1 | 13/89 | 3.099 s |
| Software | 1 | 3 | 1 | 13 | 28/89 | 3.029 s |
| Software | 2 | 1 | 8 | 2 | 11/89 | 3.228 s |

The JSON field is `repeatedFramesObserved`. D3D11VA device init and hardware-frame receipt were logged in the app path, with frames downloaded to CPU for presentation. These single runs do not establish a stable improvement from the pool or GPU decode. The no-drop/no-late goal remains unmet, and async conversion is still the next substantial change.

## Bounded asynchronous upload conversion follow-up (2026-10-12)

Upload sidecar generation now runs on a dedicated single-thread conversion pool, independent from the decoder pool. Each layer owns a four-request bounded queue, prioritizes the current frame over prefetch, rejects stale generation results, and joins the worker during layer/source teardown. Converted sidecars use the fixed four-buffer / 64 MiB pool described above. GPU texture operations remain on the renderer thread. Decoder lookahead was reduced from three frames to one for this experiment so sequential prefetch cannot hold the decode worker far beyond a current-frame request.

The affected module compiled and the standalone benchmark linked and ran. One 3-second no-RAM-cache run per case produced:

| Codec / decode | Layers | Dropped | Late | Repeated | Final frame | Result |
|---|---:|---:|---:|---:|---:|---|
| H.264 / D3D11VA | 1 | 2 | 1 | 5 | 12/89 | Failed |
| H.264 / D3D11VA | 2 | 1 | 4 | 0 | 6/89 | Failed |
| H.264 / software | 1 | 2 | 2 | 5 | 13/89 | Failed |
| H.264 / software | 2 | 1 | 1 | 2 | 5/89 | Failed |
| ProRes 422 HQ / software | 1 | 5 | 2 | 7 | 14/89 | Failed |
| ProRes 422 HQ / software | 2 | 0 | 5 | 0 | 6/89 | Failed |

The values are `repeatedFramesObserved` from the benchmark report. Profiles showed occasional frames reaching the render path without their upload sidecar ready; the fallback retained the 1x1 placeholder in those samples. D3D11VA still initialized and received H.264 hardware frames, but this build did not show a performance win over software in these single runs. The conversion worker and fixed pool are therefore experimental and do not satisfy the no-drop/no-late or realtime-range requirement. Next investigation should focus on why decode progress stalls around frames 5–14, including renderer/viewport CPU time and decoder seek startup, before increasing queue depth or worker count.

## FFmpeg hardware decode probe (2026-10-12)

The bundled FFmpeg CLI was checked independently from the application. Its `-hwaccels` list includes `d3d11va`, `cuda`, and `d3d12va`. On the same 90-frame H.264 1080p30 clip, five warm CLI runs decoded all 90 frames in each case. Median wall times including GPU-to-CPU download to NV12 were 0.18 s for software, 0.16 s for D3D11VA, and 0.16 s for CUDA. FFmpeg verbose output confirmed `pix_fmt: yuv420p` for software, `d3d11` for D3D11VA, and `cuda` for CUDA. This establishes hardware decoding works on this machine, but the roughly 11% difference is a decode-only CLI measurement, not Composition playback performance.

ProRes 422 HQ 1080p30 decoded all 90 frames in a median 0.14 s using software. The FFmpeg ProRes decoder did not expose a usable D3D11VA hardware frame path in this probe.

An opt-in D3D11VA path was added to the app's direct `MediaImageFrameDecoder`, with hardware-frame download to the existing CPU presentation path, and the standalone benchmark gained `--ffmpeg-hwaccel d3d11va`. An initial app run was later instrumented with direct `fprintf` diagnostics, confirming D3D11VA initialization and D3D11 hardware frames on the actual VideoLayer direct-decode path. The D3D11VA option is passed only to that direct decoder, avoiding device creation in the separate normal playback decoder.

A same-build H.264 1080p30 A/B run still failed strict playback in every case:

| Decode | Layers | Dropped | Late | Repeated | Final frame | Raw decode median* | CPU image conversion median* |
|---|---:|---:|---:|---:|---:|---:|---:|
| Software | 1 | 4 | 1 | 13 | 29/89 | 22.37 ms | 6.62 ms |
| D3D11VA | 1 | 2 | 1 | 9 | 29/89 | 10.63 ms | 6.73 ms |
| Software | 2 | 1 | 6 | 5 | 15/89 | 22.37 ms | 6.62 ms |
| D3D11VA | 2 | 2 | 13 | 4 | 19/89 | 10.63 ms | 6.73 ms |

*Profile medians combine the decoded samples captured in both layer-count cases. D3D11VA roughly halved raw decode time on this run, while conversion cost stayed unchanged and playback counters were mixed. Renderer upload conversion remains about 11 ms on new frames. Direct GPU frame presentation is not connected, so frames still download to CPU. The zero-drop/late/repeated-frame requirement remains unmet.

## Shared-frame copy removal follow-up (2026-10-12)

An opt-in setup profile localized recurring 21–27 ms UI-thread spikes to `ArtifactVideoLayer::goToFrame()` during composition simulation evaluation. A second profile split the shared decoded-payload path: looking up the shared frame took about 0.03 ms, copying it into the current frame buffer took about 11.2 ms, and copying it again into the RAM frame cache took another 10.7–11.2 ms for a 1920x1080 RGBA32F frame.

The shared-payload path now stores the existing shared frame in `FrameCache` rather than deep-copying it. When an RGBA8 upload sidecar is already ready, it defers the CPU presentation-buffer copy unless an input color transform requires a transformed buffer. CPU callers can still materialize the shared frame through the existing current-frame accessors. During upload conversion, repeated requests for an already-presented frame are skipped, and cache lookup is ordered after the upload-cache readiness check to avoid copying a large frame while its sidecar is pending.

After this change, the highest measured `componentSimulationMs` in the profiled 1/2-layer H.264 runs fell from about 22–23 ms to 6.5 ms with D3D11VA. The same runs still failed strict playback:

| Decode | Layers | Dropped | Late | Repeated | Final frame | Average render frame |
|---|---:|---:|---:|---:|---:|---:|
| D3D11VA | 1 | 1 | 2 | 7 | 14/89 | 45.5 ms |
| D3D11VA | 2 | 0 | 2 | 2 | 8/89 | 31.6 ms |
| Software | 1 | 1 | 2 | 7 | 14/89 | 46.0 ms |
| Software | 2 | 0 | 6 | 2 | 9/89 | 31.4 ms |

The profiles then showed the remaining work is not explained by this copy path alone. In one D3D11VA run, direct decode took 9–15 ms on newly decoded H.264 frames and CPU conversion took 6.5–7.3 ms; the same run still observed repeated frames and render averages above the 33.3 ms budget. Some render samples reached the upload fallback with a 1x1 buffer while the full-resolution sidecar was not ready. GPU timing is unavailable in this benchmark, so GPU execution versus render-thread submission/present wait remains unresolved.

## Render-loop project diagnostic removal (2026-10-12)

The composition render loop refreshed `ArtifactProjectService::currentProjectDiagnostics()` every second and on composition changes. This work performed project health checks and merged app, audio, and project diagnostics on the render thread. The result was advisory only: rendering continued even when errors were found; it only updated a render-path summary and emitted a warning. Setup profiling measured individual calls at 12.8–100.7 ms, matching periodic `preSceneMs` spikes.

Removed that advisory refresh and its cached fields from the render loop. The project diagnostics API and dashboard remain unchanged. The affected render-controller module compiled and the standalone benchmark relinked. A 3-second H.264 1080p30 D3D11VA run with two layers showed `preSceneMs` around 0.00–0.01 ms and no `projectDiagnosticsMs` samples, so the periodic pre-scene stall is removed. The benchmark still failed strict playback: 2 dropped, 2 late, 3 repeated frames, final frame 7/89, 3.259 seconds. Render-frame averages ranged roughly 22–34 ms, with individual samples at 76–81 ms; GPU timing remains unavailable. The next bottleneck is now in simulation/render/present or decode/upload pacing, not the removed project preflight call.

ProRes 422 HQ was also tested through the app with `--ffmpeg-hwaccel d3d11va`. FFmpeg created a D3D11VA device for codec id 147, but the app received no D3D11 hardware frames for ProRes; its playback results remained comparable to software decoding and failed the strict gate. H.264 produced D3D11 hardware frames, which are downloaded to CPU before the current presentation path. Neither codec meets the zero-drop/late/repeated requirement yet.

## FFmpeg hardware decode A/B after render-loop diagnostic removal (2026-10-12)

Ran the standalone Composition benchmark with RAM cache disabled, D3D11VA requested, and software selected as the control. The 1080p30 H.264 cases used the same clip with 1 and 2 layers. All runs failed the strict no-drop/no-late/no-repeat and realtime-progress gate:

| Codec | Decode | Layers | Dropped | Late | Repeated | Final frame | Render average |
|---|---|---:|---:|---:|---:|---:|---:|
| H.264 | Software | 1 | 0 | 1 | 6 | 11/89 | 37.0 ms |
| H.264 | D3D11VA | 1 | 1 | 0 | 6 | 13/89 | 31.5 ms |
| H.264 | Software | 2 | 0 | 4 | 1 | 8/89 | 30.2 ms |
| H.264 | D3D11VA | 2 | 0 | 5 | 0 | 7/89 | 28.7 ms |
| ProRes 422 HQ | Software | 1 | 3 | 2 | 10 | 18/89 | 42.6 ms |
| ProRes 422 HQ | D3D11VA | 1 | 3 | 2 | 9 | 18/89 | 35.6 ms |
| ProRes 422 HQ | Software | 2 | 1 | 2 | 3 | 6/89 | 25.2 ms |
| ProRes 422 HQ | D3D11VA | 2 | 0 | 1 | 5 | 8/89 | 22.1 ms |

The D3D11VA H.264 application path initialized and received 1920x1080 `AV_PIX_FMT_D3D11` frames, but each was downloaded into a CPU frame (`cpu=1`, `gpu=0`) before existing conversion/presentation. ProRes initialized a D3D11VA device but produced no D3D11 hardware frames; direct results were CPU frames. FFmpeg CLI checks confirmed H.264 can remain in D3D11 output format. With the correct `-hwaccel_output_format d3d11` option, ProRes decoded 2 frames without error but emitted no D3D11 pixel-format/selection log; the app's direct path likewise received only CPU frames. This supports software fallback, though it does not independently prove why the ProRes decoder declined the hardware path.

Five warm CLI runs decoding all 90 H.264 frames with GPU output surfaces measured median wall times of 231.3 ms (software), 348.4 ms (D3D11VA), and 282.6 ms (CUDA). This GPU-output CLI path did not improve decode-only wall time for this sample and is not an application presentation benchmark. Results vary from earlier measurements, so the current A/B does not establish a stable GPU-decoding win. The strict preview acceptance remains unmet.

## Vulkan decode probe (2026-10-12)

Added `--ffmpeg-hwaccel vulkan` to probe whether the bundled FFmpeg decoder can create its Vulkan device. The decoder module and interface, playback controller module, benchmark executable, and `ArtifactCoreMedia.lib` were compiled/relinked. The first run returned `AVERROR(ENOMEM)` (`Cannot allocate memory`), then software fallback. Inspection of the exact vcpkg build configuration identified the cause: `CONFIG_VULKAN=0` and `CONFIG_H264_VULKAN_HWACCEL=0`, while D3D11VA and D3D12VA are enabled. The runtime backend enumeration reports Vulkan unavailable, so the probe now prints that fact and skips the unsupported device creation. The two earlier Vulkan-requested bench cases were software fallback, not Vulkan measurements: 1 layer reported 3 dropped / 1 late / 6 repeated, and 2 layers 0 / 3 / 1; both failed the gate.

The separately installed FFmpeg CLI did create Vulkan output frames for H.264 (`pix_fmt: vulkan`) and decoded all 90 frames in each of five runs. Median wall time was 460.1 ms with Vulkan output surfaces versus 265.5 ms software on this run. The CLI result is decode/output timing only; it does not represent the app's presentation path. Vulkan direct presentation remains disabled because its AVVkFrame semaphore handoff is not synchronized with Diligent's queue. Evaluating Vulkan in Artifact requires an FFmpeg package build with Vulkan and H.264 Vulkan hwaccel enabled, followed by device/queue synchronization work; the current package cannot exercise that path.

## D3D12VA decode probe (2026-10-12)

The bundled FFmpeg build has `CONFIG_D3D12VA=1` and `CONFIG_H264_D3D12VA_HWACCEL=1`, so the bench now supports `--ffmpeg-hwaccel d3d12va`. The decoder module/interface, playback controller, `ArtifactCoreMedia.lib`, and benchmark were rebuilt. The actual application initialized D3D12VA, attached its H.264 decoder, received 1920x1080 `AV_PIX_FMT_D3D12` frames, and successfully downloaded them into the existing CPU presentation path.

Same-build, 3-second H.264 1080p30 no-RAM-cache runs all failed strict playback:

| Decode | Layers | Dropped | Late | Repeated | Final frame | Render average |
|---|---:|---:|---:|---:|---:|---:|
| Software | 1 | 2 | 3 | 7 | 14/89 | 37.8 ms |
| D3D11VA | 1 | 0 | 1 | 7 | 11/89 | 41.5 ms |
| D3D12VA | 1 | 2 | 2 | 6 | 14/89 | 39.8 ms |
| Software | 2 | 1 | 3 | 2 | 5/89 | 24.6 ms |
| D3D11VA | 2 | 0 | 3 | 2 | 8/89 | 37.2 ms |
| D3D12VA | 2 | 0 | 4 | 1 | 8/89 | 42.5 ms |

Five warm external FFmpeg CLI runs decoded all 90 H.264 frames with `-hwaccel d3d12va -hwaccel_output_format d3d12`; median wall time was 432.3 ms versus 229.7 ms software. D3D12VA is therefore operational in Artifact but did not improve this sample's measured preview or decode wall time. Unlike D3D11VA, an `AVD3D12VAFrame` exposes an `ID3D12Resource` and D3D12 fence/value. This is a possible same-device interop path to investigate, but this experiment creates FFmpeg's own device and still downloads to CPU; direct wrapping is not implemented or proven safe. Strict no-drop/no-late/no-repeat playback remains unmet.

## Two-worker conversion pool and current FFmpeg A/B (2026-10-12)

The conversion pool was temporarily raised from one to two workers on this 16-core / 24-logical-processor machine. This was an isolated experiment; the source setting was restored to one worker after no consistent end-to-end gain appeared. Using the same standalone executable, clip, 3-second duration, and RAM-cache-disabled condition, the one-worker software run measured 1 layer at 1/2/5 dropped/late/repeated (15/89 final frame, 22.36 ms reported render average) and 2 layers at 0/4/4 (8/89, 31.57 ms). D3D12VA measured 1 layer at 2/2/4 (14/89, 35.62 ms) and 2 layers at 0/5/0 (8/89, 46.80 ms). All cases failed.

With the temporary two-worker pool, software measured 1 layer at 1/2/8 (17/89, 21.38 ms) and 2 layers at 0/7/2 (9/89, 35.92 ms). D3D12VA measured 1 layer at 3/2/5 (13/89, 31.83 ms) and 2 layers at 0/5/1 (7/89, 51.33 ms). That trial also failed every case and did not show a repeatable improvement, so the pool remains bounded at one conversion worker. These single runs are noisy and should not be interpreted as a stable performance ranking.

The current app-path probe again confirmed D3D12VA initialization on the RTX 4070 Ti and receipt of 1920×1080 `AV_PIX_FMT_D3D12` frames. The presentation path still downloads them to CPU; hardware decode did not improve the strict preview result in this comparison. Source inspection also found that this Diligent revision does not expose an NV12 texture format / video sampling path for the existing RGBA sprite shader. Direct presentation therefore needs a deliberate NV12 plane sampling or GPU conversion path, plus same-device frame lifetime and fence synchronization. `CreateTextureFromD3DResource` alone is insufficient. No direct GPU-frame presentation was enabled.

## D3D12VA CPU-download and conversion timing (2026-10-12)

Added opt-in `ARTIFACT_VIDEO_PREVIEW_PROFILE_HWFRAMES=1` instrumentation around the existing hardware-frame transfer boundary. It reports at most 40 samples per process with the D3D12 resource pointer and decoder fence value, `av_hwframe_transfer_data()` wall time, and subsequent CPU RGB conversion time. The measurements used the standalone 1080p30 H.264 bench, 1 layer, 3 seconds, RAM preview disabled. On this run, D3D12VA warm GPU-to-CPU download median was about 1.41 ms (first frame 8.76 ms), while conversion after download had a median of 5.11 ms. Playback measured 0 dropped, 1 late, 3 repeated, and final frame 15/89; strict acceptance still failed. CPU-side renderer RGBA upload conversion was generally below the 1 ms timer resolution in observed samples, while texture update was about 0–2 ms.

The software control had no hardware-download samples; raw decode samples included a first-frame 166.75 ms outlier and later new-frame samples around 13.68–16.90 ms. Its separate CPU frame conversion samples were about 6.9–9.6 ms. The single-case software result ended at frame 9/89 with 1 dropped and 1 late frame. These short asynchronous playback samples are noisy and are not a stable backend ranking. They show the D3D12VA readback is measurable but does not alone account for the observed frame-range stall: decoder queue readiness and frame arrival remain broader bottlenecks.

This profile measures GPU-to-CPU FFmpeg frame transfer, **not** a decoder-to-Diligent GPU-to-GPU copy. The CPU-download timing run used FFmpeg's independently created D3D12 device; the separate shared-device probe below establishes same-device decoding but does not test GPU presentation. The profile is disabled by default. Rebuild was performed by invoking the exact affected module command and relinking the already configured standalone target; no CMake regeneration was run.

## Shared D3D12 device decode feasibility (2026-10-12)

The opt-in `--probe-shared-d3d12-device` mode initializes the real Composition renderer, obtains its shared native D3D12 device, initializes FFmpeg's D3D12VA context with that device, and decodes H.264 frames sequentially. On the RTX 4070 Ti, eight 1920×1080 `AV_PIX_FMT_D3D12` frames were received. Every frame texture's device matched the renderer device. The frame resource format was DXGI `NV12` (103). Each frame was sampled with D3D12 plane SRVs, converted to RGBA8 using a compute shader, and successfully wrapped with Diligent; the per-frame center samples changed as the source frames changed.

PTS advanced monotonically as `0, 512, 1024, 1536, 2048, 2560, 3072, 3584`. Fence values are surface-local rather than globally increasing: FFmpeg reused several fence objects, with values advancing on each object's later use. The probe records each fence pointer and value and queues a GPU-side wait for that pair. All eight decoder fences had already completed by conversion submission.

The probe uses the Diligent immediate context's own D3D12 command queue, shader/PSO, and completion fence with a fixed three-slot ring. It acquires the queue using `LockCommandQueue()`, queues the FFmpeg source-fence wait through `ICommandQueueD3D12::WaitFence`, submits the compute command list with `Submit`, signals conversion completion with `EnqueueSignal`, then unlocks the queue. Each slot has its own output texture, descriptors, allocator/list, and (for sample 0 only) readback buffer. It retains the source `AVFrame` until that slot's completion fence is retired. CPU waits occur only when the producer reaches a still-busy ring slot and during final drain. Three standalone runs each submitted and retired 8/8 frames with monotonic PTS and reached `maxInFlightSlots=3`; elapsed sequence times were 143–199 ms. One run's frames 3–7 enqueue wall was 2.69–3.65 ms; frame 3 waited 1.36 ms for slot 0, while later slot reuse waits were under 0.011 ms. This demonstrates bounded asynchronous submission on Diligent's queue in the isolated probe, not drop-free playback: decoding had already completed each source fence, the probe has no timed display loop, and it does not draw the converted textures in the viewport. The first sample still includes roughly 80 ms one-time shader/resource setup and validation readback.

At the time of this probe, the GPU texture cache accepted Vulkan decoder images only. The subsequent D3D12 integration adds a fixed three-slot NV12 conversion pool and sprite presentation with post-draw retirement. The probe itself still waits on CPU at slot reuse/final drain and does not draw the resulting texture, so its timing does not establish real-time playback performance.

## Runtime exception repair (2026-10-12)

Performance investigation is paused until runtime faults are resolved. Windows Application events recorded an `0x80000003` Debug CRT crash in the earlier `ArtifactVideoPreviewBench-device-diag.exe`, plus an AppHang in an earlier product probe. Ordinary stdout/stderr logs did not capture these faults; the earlier conclusion that there were no exceptions was incomplete.

A native Windows debugger captured all first-chance exceptions and `OutputDebugString` output through process exit. It found repeated Boost UUID parsing exceptions when the viewport constructed a `LayerID` from the empty ID in the bulk `Removed` event. The viewport now treats that sentinel as a nil ID and clears selection on bulk removal. It also found heap corruption while deleting `GPUTextureCacheManager`. Recompiling the additional allocation owner, `ArtifactRenderQueueService.cppm`, along with the render controller and relinking the standalone bench removed that corruption. The old allocation template instantiation had survived the earlier partial rebuild; changes to an exported class layout require rebuilding every allocation owner, including owners linked as otherwise unused object files.

The debug stream additionally exposed D3D12 `INCOMPATIBLE_BARRIER_LAYOUT` errors from FFmpeg's COPY-queue CPU frame download. D3D12VA device contexts now set `D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS` before frame allocation, preserving a COMMON texture layout for the COPY queue and the existing fences for decoder/conversion ordering. This uses the installed FFmpeg 8.1 device-context API. Microsoft's [resource flags](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_flags) and [resource-state rules](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states) describe the multi-queue contract; this flag can affect texture compression/bandwidth and has not been performance-rated.

After targeted compilation and the configured standalone link/deployment command, the canonical executable is `out/build/x64-Debug/bin/Debug/ArtifactVideoPreviewBench.exe`, with its matching DLLs and Qt plugins. The old `temp/ArtifactVideoPreviewBench-*.exe` files are obsolete diagnostic builds. Native-debugger checks with H.264 1280×720 at 30 fps, 1 then 2 layers, RAM cache disabled, exercised source replacement and full window/process teardown: the final 3-second-per-case run recorded zero fault exceptions, zero Debug CRT heap/assertion errors, and zero D3D12 ERROR/CORRUPTION messages. First-chance `0x406d1388` notifications originated in NVIDIA/D3D debug-layer thread naming and are recorded separately. Nonfatal D3D12 resource-creation/live-object and shader warnings remain. Exit code 1 is the existing strict playback acceptance failure, not an exception; debugger timings are not performance evidence. Logs: `temp/video-native-debug.log` and `temp/video-native-before-copy-fix.log`. Longer runs, cache-enabled cases, ProRes, Vulkan, and the full application have not been covered by this runtime repair validation.

## Runtime frame readiness and cache invalidation (2026-10-12)

Video decode/upload completion now uses `LayerChangedEvent::ChangeType::FrameReady` on the existing event route. Composition render controllers invalidate the current layer surface and request redraw; authoring subscribers ignore the hint. It does not cancel RAM preview construction, remove persistent preview ranges, refresh authoring panels, or invoke edit hooks. Actual source loads, source geometry changes and property edits retain `Modified` notifications. No public signal/slot or new bus subscription is introduced.

The existing `ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP=1` diagnostic includes bounded frame-publication, transport-dispatch, audio-feed, readiness-notification and invalidation timings. It does not flush per sample. Build and runtime evidence for this change is recorded in `Insight.md` and `temp/video-frame-ready-*.json`/logs. A passing exception check is separate from realtime/no-drop acceptance.

## RAM preparation phase and measured phase

The standalone harness advances the build head through `ArtifactPlaybackService`, preserving its full requested range and using the existing render tick. Its prewarm radius retains that range. Composition duration equals the measurement duration; frame ranges use an exclusive end. Each case stops and waits for transport, seeks to frame zero, and settles pending GUI work before replacing layers. Once preparation is complete, frame-zero presentation settles and video timing outcomes reset before the measured playback phase. Preparation lateness is not counted as measured playback lateness; the strict zero-drop/late/repeat and realtime-progress gates remain in force.

In PlaybackService, GUI authoring invalidation is applied inline, while a queued cross-thread invalidation rechecks composition identity. Initial render-contract establishment rebuilds an active RAM request after discarding old-contract caches. Pending decode/readback keeps its capture head stable until completion. This does not yet supply a general detached background-build driver for the full product application.

The harness restarts the existing render controller before each stopped preparation phase; stopping the previous case also stops its render tick. Both layer-count cases now construct the full range. Before process exit, it removes its video layers while background pools are still alive. Native attach stacks identified a decode worker trying to submit upload conversion during static pool destruction; explicit layer teardown fixes the observed exit hang.

## Presentation backpressure results (2026-10-12)

The sequential playback worker uses the renderer's existing `recordPresentedFrame` route to acknowledge each target before dispatching the next frame. The wait uses the existing condition variable, is bounded to 100 ms, and wakes on pause/stop. It adds no GUI/GPU wait, signal/slot, subscription, or per-frame container allocation. Missing observers fall back to existing dispatch after the timeout; explicit frame-skip modes retain asynchronous dispatch.

On the current Debug/D3D12 bench, cached H.264 720p30 for 3 seconds: 1 layer reached frame 89 with zero dropped/late/repeated frames; 2 layers reached frame 54 with zero dropped/repeated but 25 late frames. Cached ProRes 422 HQ 1080p30 for 1 second: 1 layer reached frame 29 with zero dropped/late/repeated; 2 layers reached frame 19 with zero dropped/repeated but 4 late frames. Both processes exited normally. These short runs establish progress, not sustained realtime or general scaling acceptance. Reports: `temp/video-presentation-ack-3s.json` and `temp/video-prores-presentation-ack.json`.

Remaining investigation: the RAM image retrieval path calls `DeepCopy()` per rendered frame and uploads a CPU image. Its contribution to the remaining multi-layer lateness is unverified. A cache hit does not itself establish a zero-copy GPU presentation path.

The Composition render controller now borrows the cache-owned const CPU image for its synchronous draw instead of `DeepCopy()`. The service rejects cross-thread borrowing; callers must not retain the pointer or pump events before drawing. The draw uploads synchronously and retains only the GPU texture. Existing copy APIs remain available. A subsequent H.264 720p30 cached 3-second run again passed 1 layer (frame 89); 2 layers reached frame 59 with 18 late frames and zero dropped/repeated frames. Both cases constructed 90/90 and the process exited normally. One short run does not establish the speedup or sustained acceptance. Report: `temp/video-borrowed-preview-3s.json`.

Native debugger validation after borrowing covered both 1/2-layer cases with 30/30 cached frames, playback, source replacement and process teardown: zero fault exceptions and zero D3D12 ERROR/CORRUPTION or CRT heap/assertion errors, terminal exit 1 from the performance gate. Log: `temp/video-native-borrowed-debug.log`. Nonfatal live-object/shader warnings remain. This is runtime safety evidence on the standalone target, not full-application validation. The harness also samples video outcomes once at measurement end so the final interval after its periodic sample cannot conceal lateness.

## Cached playback refinements and timing correction (2026-10-12)

Valid RAM playback ticks no longer reopen the disk PNG and manifest just to refresh disk status. Writes, evictions and authoring invalidation continue to maintain that status; actual disk loading still validates the files. FrameReady content invalidation preserves the previous zoom/pan values so it cannot falsely disable RAM fallback as a changing viewport. Its serial/dirty invalidation remains active. A valid shared decoded CPU payload can now regenerate only its evicted upload representation through the existing bounded conversion worker, rather than re-decoding the source. The upload cache remains limited to four frames/64 MiB.

The previous cached video timing reset happened **after** `present()` and hid cached draw latency. Cache tickets now start before drawing and preserve the frame request timestamp. Decode completion cannot replace a composition-cache ticket. RAM display is identified as RAM even when a disk copy also exists. Earlier cached-pass results require reconfirmation with this correction; acceptance thresholds were not relaxed.

With the corrected timing, one Debug/D3D12 cached H.264 720p30 run for 3 seconds passed **2, 4 and 8 layers**: each built 90/90, reached frame 89, and recorded zero dropped, late or repeated frames, with normal terminal exit 0. Report: `temp/video-shared-upload-scale.json`. A preceding 1-second run had one late frame for 2 layers, so sustained stability is still unproven. That run used a **1280×720, 30 fps** Composition, including tests whose source footage is 1080p. It does not establish native 1080p Composition performance, no-cache performance, distinct simultaneous source streams or other backends.

The corresponding cached 1080p ProRes 422 HQ source run (same 720p Composition, 3 seconds) reached frame 89 for 2/4/8 layers, with zero dropped/repeated frames but respectively 2/1/10 late frames. All cases built 90/90, process exit 1; strict acceptance remains failed. Report: `temp/video-prores-shared-upload-scale.json`.

Latest native debugger validation of these source changes covered 1/2 H.264 layers, 30/30 cached frames per case, playback, replacement and teardown: exceptions 0, D3D12 ERROR/CORRUPTION and CRT heap/assertion errors 0, normal terminal exit 1 for the performance gate. Log: `temp/video-native-cache-reuse-debug.log`. Debugger measurements do not prove playback performance.

## Actual presentation and resolution evidence (2026-10-12)

Reports now separate the transport's `finalFrame` from renderer-confirmed `finalPresentedFrame`. Both must reach the required range. The service records actual presentation independently of its initial dropped-frame baseline and resets it on play. This confirms a successful CPU-side `present()` notification, not physical scanout time. A repeated 720p H.264 cached 3-second run reached actual frame 89 for both 2/8 layers; 2 layers had one late frame (failed), while 8 layers had zero (passed). Report: `temp/video-actual-presented-gate.json`.

`--composition-width` and `--composition-height` now select the Composition and window size (defaults 1280/720, valid integers 1..8192). Reports include requested Composition dimensions/FPS and actual viewport dimensions. A cached ProRes native 1920×1080 Composition/viewport run for 3 seconds built 90/90 in both 1/2-layer cases but failed: 1 layer actual frame 79, late 65; 2 layers transport frame 65/actual 64, late 58. Both had zero dropped/repeated frames. Report: `temp/video-native1080-prores.json`. Native 1080p acceptance remains unachieved.

Normal timing samples now read existing published renderer counters; detailed resource/graph/string snapshots are taken only with `ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP=1`. This reduces benchmark-induced GUI work without disabling actual presentation or video timing gates. Case-boundary progress messages run outside measured playback.

The subsequent counters-only 1080p ProRes cached 1-second run still failed (transport 29, actual 28, 21 late frames). Upload profiling separated after the measurement marker found 27 cached-image uploads: CPU conversion median 18 ms/max 20, texture creation median 7 ms/max 16, and content-key time below the 1 ms resolution. No video decode samples occurred in that measured interval. Log: `temp/video-native1080-upload-profile.log`. This identifies remaining presentation work; profiling is not acceptance evidence.

The upload image was **2400×1195**, so logical Composition/window dimensions alone do not describe actual pixel cost under Windows display scaling. New report fields `cachedImageWidth`, `cachedImageHeight` and `viewportDevicePixelRatio` expose this difference. Cached-image capture geometry and color contract must be checked before changing the capture path.

## Prepared RAM presentation upload (2026-10-12)

The existing RAM cache entry now retains its CPU image, a prepared RGBA8 sRGB straight upload buffer, and a monotonically assigned content revision. Upload conversion occurs only when storing frames while transport is stopped. Prepared data is bounded to 64 MiB/frame and 1 GiB total and follows the entry's replacement, eviction, range invalidation and clear. Frames exceeding the additional budget use the existing image path. This adds cold preparation time and memory; it does not make live capture allocate an additional converted image. The existing map is retained with a changed value type; the upload uses the existing `ImageUploadBuffer` byte vector.

Synchronous owner-thread rendering borrows that representation and passes it to the existing `textureForVideoFrame` reusable texture slot, then draws the returned SRV. Each controller has a cold-created UUID and each cache replacement gets a new revision. It retains the existing conversion target/texture format; missing or failed prepared textures fall back to the old sprite path. No new transport, signal/slot, subscription, GPU queue, or unlimited frame texture cache is introduced.

First ProRes result at Composition/viewport 1920×1080, DPR 1.25, cached image 2400×1195, cached playback 1 second: 1 layer displayed frame 29 with zero dropped/late/repeated (pass); 2 layers displayed frame 29 with zero dropped/repeated but one late (fail). Both built 30/30 and exited normally. Report: `temp/video-prepared-upload-native1080.json`. Longer/multiple-run stability and visual parity are still unverified.

Native debugger validation of the new route covered 1/2 cached H.264 cases, 30/30 frames each, replacement, playback and teardown: exceptions 0, D3D12 ERROR/CORRUPTION and CRT heap/assertion errors 0, terminal exit 1 from the performance gate. Log: `temp/video-native-prepared-upload-debug.log`. Native ProRes debugging, budget exhaustion and other backends still require coverage.

`--no-disk-cache` selects RAM-only preparation using the existing PlaybackService API; the default still persists to disk. Reports record `diskCacheEnabled`. A single RAM-only native-size ProRes 2-layer/3-second run passed, but the subsequent 1..8 matrix had late-frame failures in 2/3/4/5/6/8 cases (all actually displayed frame 89 with zero dropped/repeated). Report: `temp/video-prepared-upload-prores-ram-matrix.json`. It is not a stable scaling pass.

Timing review found that same-frame redraws could append new outcomes after a frame was already presented. A ticket-local flag alone was insufficient because a repeated decode request could replace the ticket. The diagnostic now preserves the first result using a frame/generation ledger and resets that ledger with measurement diagnostics. Genuine first-presentation latency and repeated-last-good failures remain counted. The corrected RAM-only ProRes 2/8-layer/3-second run recorded **89 outcomes per first video**, actual frame 89, and zero dropped/late/repeated frames in both cases; average first-presentation times were 16.60/18.85 ms. Report: `temp/video-outcome-dedup-prores.json`. This still does not prove long-duration stability, distinct simultaneous source streams, disk-enabled stability or visual parity.

The preceding ticket-local version also passed native debugging with ProRes at logical 1920×1080, 1/2 layers, 30/30 frames per case, replacement/playback/teardown: exceptions 0, D3D12 ERROR/CORRUPTION and CRT heap/assertion errors 0, terminal exit 0. Log: `temp/video-native-unique-prores-debug.log`. Native validation of the additional frame/generation ledger is tracked separately.

Latest ledger native validation also completed both ProRes 1/2-layer cases with 30/30 cached frames, actual frame 29 and zero dropped/late/repeated frames: exceptions 0, D3D12 ERROR/CORRUPTION and CRT heap/assertion errors 0, terminal exit 0. Logs/reports: `temp/video-native-outcome-dedup-debug.log`, `temp/video-native-outcome-dedup-repro.json`. No benchmark process remains running after validation.

## Mixed-source validation (2026-10-12)

The canonical executable was tested with independent H.264 1280×720 and ProRes HQ 1920×1080 inputs, cycled across layers, at logical Composition/viewport 1920×1080, DPR 1.25, RAM-only, 3 seconds. Both 2/8-layer cases actually presented frame 89 with zero dropped/repeated frames. Two layers had zero late frames and passed; eight layers had 3 late frames and failed strict acceptance. Process terminated normally with exit 1. Report: `temp/video-mixed-outcome-dedup.json`. Mixed-source stability is not established.

Native debugging of the canonical executable covered both mixed-source 2/8-layer cases for 1 second, replacement and teardown: exceptions 0, D3D12 ERROR/CORRUPTION and CRT heap/assertion errors 0, normal terminal exit 1. Actual frame 29 was reached in both cases; late counts were respectively 1/0. Logs/report: `temp/video-native-mixed-dedup-debug.log`, `temp/video-native-mixed-dedup-repro.json`. Debugger timings are not performance acceptance evidence. The helper script's obsolete default executable was bypassed with an explicit canonical executable argument; its preliminary invocation produced no case report and is excluded. No benchmark process remained running afterward.

## Mixed-source presentation profiling (2026-10-12)

The existing opt-in upload profiler now records at most 24 unprepared and 2048 prepared calls separately, identifies prepared calls, and avoids an explicit flush per pooled upload. Prepared buffers also come from video decode workers, so their presence alone does not identify RAM Composite calls. Ordinary rendering is unchanged when profiling is disabled. Initial 384/256-call diagnostic attempts exhausted their limits during preparation and provide no measured upload evidence.

The bounded 8-layer mixed-source 3-second profile captured 128 measured calls, all at 2400×1195, all pool hits. CPU-side conversion/key phase median/max was 0/2 ms, texture creation 0/0 ms, UpdateTexture 1/8 ms. Actual frame 89 was reached, dropped/repeated 0, late 8; terminal exit 1. Logs/report/summary: `temp/video-mixed-upload-bounded-profile.log`, `temp/video-mixed-upload-bounded-profile.json`, `temp/video-mixed-upload-bounded-summary.json`. Instrumented timings are not acceptance evidence or GPU execution times.

Existing Composition View phase logs were captured using `QT_LOGGING_RULES=artifact.compositionview.debug=true` and `QT_FORCE_STDERR_LOGGING=1`; without the latter, the first attempt did not capture phase logs. The 8-layer mixed-source RAM-only 1-second run reached actual frame 29 with zero dropped/late/repeated and exit 0. Among 26 logged slow frames (at least 16 ms), frame median/max was 19/24 ms, setup 1/1 ms, layer phase 2/13 ms, submission 2/11 ms, present phase 14/20 ms. Log/report: `temp/video-mixed-phase-stderr-profile.log`, `temp/video-mixed-phase-stderr-profile.json`. These are CPU wall phases and a selected slow-frame subset, not all-frame statistics or physical scanout proof. Present scheduling/wait remains the next investigation; mixed-source sustained acceptance is still unproven.

## Same-frame temporal history correction (2026-10-12)

The render tick treated any frame other than previous+1 as a temporal discontinuity, including redraws of the same frame. This invalidated temporal history and rendered frame N-1 before drawing N again. The predicate now excludes same-frame redraws; noncontiguous seeks and reverse steps retain their existing invalidation/pre-roll behavior. No redraw is suppressed, no synchronization is removed, and the change adds no allocation.

After compiling only the affected controller object and linking the standalone benchmark, the ordinary mixed-source RAM-only 2/8-layer, logical 1920×1080, 3-second test reached actual frame 89 in both cases with dropped/repeated 0. Late counts were 0/1, so two layers passed and eight layers still failed; terminal exit 1. Report: `temp/video-same-frame-history-mixed.json`. The earlier eight-layer late count was 3, but run variability prevents claiming a stable performance improvement. Temporal-effect visual parity, seek/reverse behavior, longer duration and disk-enabled acceptance remain unverified.

Native validation of the canonical updated executable covered mixed-source 2/8-layer, 1-second cases, replacement/playback/teardown: fault exceptions 0, D3D12 ERROR/CORRUPTION and CRT heap/assertion errors 0, normal terminal exit 1 for the performance gate. Both reached actual frame 29 with dropped/repeated 0; late counts were 1/0. Logs/report: `temp/video-native-same-frame-history-debug.log`, `temp/video-native-same-frame-history-repro.json`. No benchmark process remained running after validation.

## RAM-presented frame readiness coalescing (2026-10-12)

After the temporal correction, an instrumented mixed eight-layer 3-second run reached actual frame 89 with zero dropped/late/repeated, but still made 138 measured pooled texture calls. All were 2400×1195 pool hits; UpdateTexture CPU median/max was 0/3 ms, creation 0 ms. Report/summary: `temp/video-history-upload-profile.json`, `temp/video-history-upload-summary.json`. This does not establish reduced presentation count or stable acceptance.

The existing FrameReady subscriber invalidates live surfaces and marks layer damage, but now avoids requesting an additional draw if RAM presentation already succeeded for the same Composition/frame, playback is active, no viewport interaction is busy, and that RAM image remains available. A Composition ID accompanies the presented frame to avoid cross-Composition aliasing. Authoring invalidation, frame changes, live rendering and pending dirty requests retain their existing paths. The change adds fixed-size state and no new queue, signal, subscription or allocation.

Ordinary mixed H.264/ProRes RAM-only 2/8-layer, logical 1920×1080, 3-second validation passed both cases: actual frame 89, dropped/late/repeated 0, normal terminal exit 0. Report: `temp/video-ram-ready-coalesce-mixed.json`. Native mixed 2/8-layer 1-second validation reached actual frame 29 in both cases with dropped/repeated 0 and late 1/0: fault exceptions 0, D3D12 ERROR/CORRUPTION and CRT heap/assertion errors 0, normal terminal exit 1 for the performance gate. Logs/report: `temp/video-native-ram-ready-coalesce-debug.log`, `temp/video-native-ram-ready-coalesce-repro.json`. No benchmark process remained running. Sustained/repeated acceptance, Composition switching, viewport interaction, no-cache behavior and disk-enabled coverage remain incomplete.

## Disk-enabled validation and cumulative timing (2026-10-12)

Mixed 2/8-layer, logical 1920×1080, 3-second validation with disk persistence enabled passed both cases: 90/90 prepared frames, 90 frames in RAM and on disk, actual frame 89, dropped/late/repeated 0, terminal exit 0. Report: `temp/video-ram-ready-coalesce-mixed-disk.json`. This is a short single run, not proof of sustained persistence performance.

Video timing now retains fixed-size cumulative late/repeated counters alongside the existing 120-outcome window. They increment only for the first recorded presentation and reset in `resetFrameTimingDiagnostics()`. Benchmark acceptance reads cumulative counters rather than the rolling window. Report `videoTimingCountPolicy` states that values are the maximum per-layer cumulative count since measurement reset, saturated at INT32_MAX; they are not a sum across layers. Missing cumulative diagnostics cannot count as valid timing samples. The change adds no hot-path allocation or public module layout dependency.

The updated executable's H.264 1-layer, logical 1280×720, RAM-only 4.2-second run prepared 126/126 frames, actually presented frame 125, reached `windowFrames=120`, and retained cumulative late/repeated 0, dropped 0, terminal exit 0. Report: `temp/video-cumulative-window-boundary.json`. This verifies integration beyond the rolling-window length, but does not demonstrate retention of an injected nonzero early error or establish long-duration stability. Native mixed 2/8-layer, 1-second validation reached actual frame 29 in both cases, dropped/repeated 0, late 1/0: fault exceptions 0, D3D12 ERROR/CORRUPTION and CRT heap/assertion errors 0, normal terminal exit 1 for the timing gate. Logs/report: `temp/video-native-cumulative-outcomes-debug.log`, `temp/video-native-cumulative-outcomes-repro.json`. No benchmark process remained running.

## Bounded-window loop measurement (2026-10-12)

`--loop-window-seconds <seconds>` optionally loops a source/Composition/cache window while `--seconds` specifies the full measured time. The window must be at least 1 second, no longer than the measurement, and at most 128 frames at 30fps. Source-duration checks and preparation use the window duration. Default non-loop measurement is unchanged. Report fields `loopMeasurement` and `sourceWindowSeconds` identify the mode.

The existing PlaybackService presentation notification now maintains `observedPresentedFrameCount()` since play, counting frame changes rather than same-frame redraws. This is CPU-side successful presentation evidence, not physical scanout. Loop acceptance requires playback to remain active and the count to reach floor(measured seconds × 30) minus two frames, in addition to cumulative late/repeated and drop counts of zero. The report retains final frame numbers and adds `observedPresentedFrames`. Forward/reverse loop-wrap gaps are counted using the playable inclusive endpoints when looping with skip mode None; ping-pong and explicit seek baseline reset retain existing behavior.

Mixed H.264/ProRes, logical 1920×1080, RAM-only, 2/8 layers, 3-second window repeated for 30 seconds: both cases kept 90 RAM frames, recorded **900 observed presentations**, final frame 0, dropped/cumulative late/repeated 0, terminal exit 0. Report: `temp/video-loop-window-mixed-30s.json`. This validates sustained repetition of a cached short segment; it does not prove a long unique source segment, rolling-cache decoding, all layer counts, repeated-run stability, disk-enabled long-duration performance or visual parity.

Native validation of the updated service/interface/benchmark covered mixed 2/8-layer 1-second windows looped for 3 seconds: both recorded 90 presentations, final frame 0, drop/repeat 0, cumulative late 1 each. Fault exceptions 0, D3D12 ERROR/CORRUPTION and CRT heap/assertion errors 0, normal terminal exit 1 for the timing gate. Logs/report: `temp/video-native-loop-window-debug.log`, `temp/video-native-loop-window-repro.json`. No benchmark process remained running. Deliberately skipped wrap frames and seek/reverse/ping-pong regression cases remain unverified.

## Full layer matrix and offline export (2026-10-13)

The latest ordinary 1..8-layer matrix repeated the 3-second window for 30 seconds per case, logical 1920×1080, RAM-only, H.264/ProRes inputs cycled across layers (one layer is H.264 only). Every case recorded 900 successful presentation notifications with zero drops/repeats. Cumulative late counts for layers 1..8 were **0,0,0,1,2,0,0,1**; 4/5/8 failed strict acceptance. The process terminated normally with exit 1. Report: `temp/video-loop-window-mixed-matrix-30s.json`. The preceding 2/8 pass is not stable all-layer acceptance.

At the eight-layer case's end, the rolling diagnostic showed lateFrames=0 while cumulativeLateFrames=1 and the case failed. This directly confirms that an early real error survives rolling-window eviction. Five layers similarly showed rolling late=1 versus cumulative=2. It is not a fault-injection test.

`export_report.py` converts an existing completed report into CSV and a manifest without rerunning or changing the benchmark. It preserves reported conditions, per-layer source metadata, statuses and counts, identifies unavailable metrics, and hashes the source report. Optional `--executable` captures a **post-run** file fingerprint, not automatic launch identity; optional `--environment` embeds independently captured JSON. Input files cannot be overwritten by export destinations. Example:

```powershell
python tools/VideoPreviewBench/export_report.py temp/video-loop-window-mixed-matrix-30s.json --output-prefix temp/video-loop-window-mixed-matrix-30s-export --executable out/build/x64-Debug/bin/Debug/ArtifactVideoPreviewBench.exe --environment temp/video-loop-matrix-postrun-environment.json
```

Exports: `temp/video-loop-window-mixed-matrix-30s-export.csv`, `temp/video-loop-window-mixed-matrix-30s-export.manifest.json`. The environment snapshot records post-run CPU/GPU/driver/OS inventory and total system RAM, not utilization or peak process memory. All eight CSV statuses and presentation/late counts were checked against the source JSON and matched. No benchmark process remains running. Sparse lateness, long unique-source playback, repeated-run stability, disk-enabled sustained matrix, visual parity and the remaining planned metrics are still incomplete.

## First-late evidence and composition synchronization (2026-10-13)

The subsequent full 1..8-layer matrix ran each case for 60 seconds with the same mixed inputs, logical 1920×1080, RAM-only and 3-second window. All cases recorded 1800 successful presentation notifications and zero drops/repeats. Cumulative late counts were **0,0,0,3,0,0,0,0**; four layers failed, and the process terminated with exit 1. Report/export: `temp/video-sync-gate-full-matrix-60s.json`, `temp/video-sync-gate-full-matrix-60s-export.csv`. The first four-layer incident was frame/source 16, about 18.541 seconds after diagnostic reset, decode 0 ms, queued 19.96 ms, render-to-present 16.46 ms, total 36.42 ms. Background decode and GPU execution are not excluded by these CPU-side durations.

The benchmark now records `renderCycles` as the difference of the existing controller frame serial around measurement. This counts completed render cycles, including redundant redraws, rather than physical scanout. The exporter preserves this field when available and extracts first-late phase evidence into CSV/manifest; older reports leave unavailable fields blank. A real five-layer diagnostic report was checked against the export: frame 74, five layers with evidence, queued 17.00 ms and render-to-present 17.20 ms for the earliest recorded incident.

The four-layer 60-second repeat with this counter recorded 1800 render cycles and 1800 presentation notifications, drop/late/repeat 0, terminal exit 0 (`temp/video-render-cycles-4layer-60s.json`). No surplus completed rendering was observed in this run. This does not explain the preceding three late outcomes or establish repeatable acceptance.

Two further independent four-layer 60-second runs both recorded 1800 render cycles and 1800 presentations, drop/repeat 0. Trial 1 passed with late 0; trial 2 failed with late 1 (normal exit 0/1 respectively). Reports: `temp/video-render-cycles-repeat-1.json`, `temp/video-render-cycles-repeat-2.json`. Trial 2's first-video incident was frame/source 68 at 26.272 seconds after reset: queued 18.49 ms, render-to-present 15.19 ms, total 33.68 ms, decode 0 ms. Surplus completed rendering does not explain this observed failure. OS scheduling, background work and swapchain waiting remain unseparated.

Periodic benchmark sampling now retains fixed numeric maxima: `maximumDiagnosticSampleMs` covers video diagnostics plus renderer sampling (including existing JSON sample construction), `maximumVideoDiagnosticSampleMs` covers the video portion, and `maximumDiagnosticSampleAtMs` identifies the start of the largest combined sample relative to measurement. The final post-measurement sample and optional verbose profile logging are outside these durations. This adds no sample log or new collection. CSV preserves optional fields; older reports remain blank. A four-layer 60-second run recorded 1800 render cycles/presentations, drop/late/repeat 0, exit 0, combined maximum 2.3833 ms at 59515 ms and video maximum 2.2934 ms (`temp/video-sampler-duration-4layer-60s.json`). No late event occurred in that run, so sampling's contribution to a failing run remains unproven.

The latest native debugger run covered mixed 2/8 layers, one-second windows looped for three seconds: both 90 render cycles/presentations, drop/repeat 0, late 1/0. Fault exceptions and matched D3D12 ERROR/CORRUPTION or CRT heap/assertion errors were zero; process exited normally with code 1 for the timing gate (`temp/video-native-sampler-duration-debug.log`, `temp/video-native-sampler-duration-repro.json`). Debugger timing is not ordinary performance acceptance. No benchmark process remained running.

The same ordinary four-layer setup was extended to 120 seconds: 3600 completed render cycles and successful presentation notifications, drop/cumulative late/repeat 0, normal exit 0. Combined diagnostic maximum was 3.2351 ms at 79344 ms; video portion maximum 3.1644 ms (`temp/video-sampler-duration-4layer-120s.json`, corresponding export CSV/manifest). This remains repetition of a three-second cached window, not 120 seconds of unique source content. No late event occurred, so this does not explain prior failing runs.

## Sustained disk-enabled coverage and external memory (2026-10-13)

### Measurement dispatch comparison

`compare_dispatch_reports.py` reads completed launch records and reports offline. It compares reported cache/viewport/source/duration conditions and pre-launch executable hashes, then lists per-mode case pass/drop/late/repeat counts without a causal or statistical claim. Preparation counts are part of the comparison. Example:

```powershell
python tools/VideoPreviewBench/compare_dispatch_reports.py temp/video-dispatch-paired-launches.json --output temp/video-dispatch-paired-comparison.json
```

`--measurement-loop polling` retains the original default: `processEvents(AllEvents, 10)` followed by a 2 ms sleep. `--measurement-loop qt` runs an ordinary `QEventLoop` with two stack-owned `QBasicTimer`s: a precise 250 ms diagnostic timer and a precise measurement deadline. The existing sample callback is referenced on the stack without a new signal connection, callback heap allocation or timer-per-frame creation. Deadline setup subtracts elapsed playback startup/sample time from the requested duration; post-measurement cumulative diagnostics and acceptance thresholds stay the same. Qt mode rejects durations beyond its integer millisecond timer range. `measurementLoop` is recorded in JSON/CSV/manifest; historical reports leave it blank. Product transport and renderer are unchanged. This is an experimental dispatch comparison, not yet a demonstrated latency fix.

The first ordinary Qt-mode mixed four-layer 60-second run with disk-ready preparation recorded RAM 90/disk 90 before playback, 1800 render cycles/presentations, drop/cumulative late/repeat 0, measured 60.000 seconds and normal exit 0. Maximum diagnostic sample was 2.4608 ms (`temp/video-qt-loop-disk-ready-4layer-60s.json`, matching export CSV/manifest). Earlier polling-mode runs also sometimes passed; this isolated Qt result does not establish a causal improvement or repeatable acceptance. Same-binary paired repetitions remain pending.

Qt-mode native validation covered disk-ready mixed 2/8 layers, one-second windows looped for three seconds: both 90 cycles/presentations, drop/repeat 0, cumulative late 1 each, disk 30 prepared. Fault exceptions and matched major D3D12/CRT heap/assertion errors were zero, normal exit 1 (`temp/video-native-qt-loop-debug.log`, `temp/video-native-qt-loop-repro.json`). No benchmark process remained running. Instrumented debugger results are separate from ordinary performance acceptance.

An ordered polling/Qt/Qt/polling comparison used the same binary for four independent four-layer 60-second disk-ready runs. The offline comparison confirmed equal reported source/cache/viewport/duration conditions and identical pre-launch SHA256 values. Polling passed 1/2 cases: drop counts 1/0, cumulative late 1/0. Qt passed 2/2: drop/late/repeat 0 in both, 1800 cycles/presentations each. First polling case recorded 1799 cycles/presentations; the other three recorded 1800. Diagnostic maxima were 2.3214, 2.3294, 2.0586 and 4.6499 ms in trial order. Results and launch records: `temp/video-dispatch-paired-comparison.json`, `temp/video-dispatch-paired-launches.json`. This small sequential comparison suggests a dispatch contribution but does not establish causality, a full layer matrix or stable acceptance. The following controller ACK repair was not in this fixed binary.

### Successful-presentation ACK contract

The controller previously notified PlaybackService whenever the renderer status lacked the substring `fail`. Renderer statuses include `skipped-no-swapchain`, recovery statuses and exception messages that need not contain `fail`; these are not successful presentations. The ACK guard now requires exactly `ok`, using the same renderer status contract as the cached-frame coalescing guard. This one-line change adds no queue, allocation or API. It repairs success accounting rather than claiming a latency improvement. Controller object/link succeeded; explicit skipped/recovery fault injection remains unverified.

Native validation after this repair covered disk-ready Qt-mode mixed 2/8 layers with one-second windows looped for three seconds: both 90 cycles/presentations, drop/repeat 0, late 1 each, fault exceptions 0 and matched major D3D12/CRT heap/assertion errors 0, normal exit 1 (`temp/video-native-present-success-ack-debug.log`, `temp/video-native-present-success-ack-repro.json`). Existing live-object warnings remained; no benchmark process remained running. This verifies normal runtime integration, not the untested skip/recovery statuses or ordinary timing acceptance.

The offline exporter accepts `--launch-record` for a completed single-run record identifying the exact source report and containing a pre-launch SHA256. It attaches the raw launch record, its fingerprint and an explicit caveat that this is not loaded-module verification. Input/output collision protection includes this file. Attachment was checked against the real first polling trial's launch record (`temp/video-dispatch-paired-1-launch-export.manifest.json`); pairing the same record with the second Qt report returned exit 2 before producing a CSV. The completed full Qt/ACK matrix below uses this capture route.

## Full Qt/ACK matrix and independent wait probe (2026-10-13)

The completed latest 1..8-layer, Qt-mode, disk-ready matrix repeated the three-second window for 60 seconds per case. Every case started with RAM 90/disk 90 and recorded 1800 render cycles/presentations, drop/repeat 0. Cumulative late counts were **0,0,0,0,1,1,9,27**; layers 5..8 failed and the process terminated normally with exit 1. Report/export: `temp/video-qt-ack-full-matrix-60s.json`, `temp/video-qt-ack-full-matrix-60s-export.csv`. All eight CSV presentation/late counts matched JSON, the pre-launch SHA256 matched the post-run executable fingerprint, and memory observation confirmed process exit. Whole-process peak Working Set observed was 11256479744 bytes (about 10.48 GiB); this includes preparation/all cases and is not per-case memory or a guarantee of the final unsampled peak. No benchmark process remained running.

First late snapshots: five layers at 25.537 s after reset, queued 21.65 ms + render-to-present 11.73 ms; six layers at 56.072 s, 19.98 + 14.34 ms; seven layers at 0.972 s, 15.93 + 18.09 ms; eight layers at 1.605 s, 15.20 + 18.91 ms. Decoder time was 0 ms in these snapshots, which does not exclude background work. Periodic diagnostic maxima for layers 1..8 were 0.4547, 0.6429, 0.9041, 2.0229, 2.0715, 2.8022, 2.8146 and 3.4592 ms. Qt dispatch and strict successful-present ACK alone do not achieve the full acceptance gate.

`probe_windows_waits.py` measures short Win32 waits in an independent non-GUI process without timer-resolution or priority changes. It compares Sleep, ordinary waitable timer and explicitly high-resolution waitable timer, closes its own handles, uses bounded five-second waits, and reports nearest-rank percentiles of call-plus-wait wall time. On this machine, 256 requested 1 ms waits per method gave p50 **15.4294 / 15.5977 / 1.5371 ms**, p99 **16.6303 / 16.6137 / 1.7078 ms** (`temp/video-windows-wait-probe.json`). This is not Qt, C++ condition-variable or preview timing evidence. It motivates measuring the actual render ticker's wait behavior before changing production scheduling. API contract: [CreateWaitableTimerExW](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw), [SetWaitableTimer](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer).

Mixed 4/8-layer cases, each 60 seconds, logical 1920×1080 and a three-second cached loop with RAM and disk enabled, both failed: four layers recorded 1799 render cycles/presentations, drop 1, cumulative late 6, repeat 0; eight layers recorded 1800 cycles/presentations, drop 0, late 3, repeat 0. Normal process exit 1. Both final snapshots had RAM 90/disk 90; these final counts do not prove disk persistence had completed at measurement start. Report: `temp/video-disk-cache-mixed-60s.json`.

An external observer sampled that exact process's OS lifetime peak counters once per second, without modifying it. Whole-process peak Working Set was 11346812928 bytes (about 10.57 GiB), peak paged-memory counter 11220717568 bytes, 233 samples, process exit confirmed. Preparation and both cases are included; monitoring started after launch and does not establish per-case memory or guarantee capture of the final unsampled peak. Observation: `temp/video-disk-cache-mixed-60s-memory.json`. Exporter's optional `--process-memory` attaches this JSON, its fingerprint and scope caveat to the manifest; benchmark-reported missing metrics remain explicitly separate. The attached values were checked against the source.

`--wait-disk-cache` explicitly requires every requested disk frame as well as RAM readiness before measurement. Default RAM-ready preparation remains unchanged, and the report records `waitForDiskCache`. It rejects combinations with `--no-disk-cache` or `--no-ram-cache` (latest executable returned exit 2 for the former). The standalone object/link build succeeded. Background persistence as the cause of the preceding failures remains unverified. This is RAM/disk coexistence coverage, not disk-only playback.

The subsequent disk-ready mixed 4/8-layer 60-second comparison records `preparedRamPreviewFrames` and `preparedDiskPreviewFrames` immediately before measurement, separately from the final counts. Both cases started with RAM 90/disk 90 and recorded 1800 render cycles/presentations, drop/repeat 0. Four layers still failed with cumulative late 1; eight layers passed with late 0, normal terminal exit 1 overall (`temp/video-disk-ready-mixed-60s.json`). First four-layer incident: frame/source 17 at 24.584 seconds after reset, queued 19.74 ms plus render-to-present 14.50 ms, total 34.24 ms. Diagnostic sample maxima were 4.1281/6.6529 ms. Incomplete persistence alone does not explain the observed failure, and run variability prevents attributing the lower counts to waiting. CSV preparation counts were checked against the source report. The earlier RAM-ready run's preparation counts remain unknown.

Native validation of this executable and disk-ready preparation covered mixed 2/8 layers, one-second windows looped for three seconds. Both started with RAM 30/disk 30 and recorded 90 presentations, drop/repeat 0, late 1/0. Fault exceptions and matched major D3D12/CRT heap/assertion errors were zero, normal exit 1 (`temp/video-native-disk-ready-debug.log`, `temp/video-native-disk-ready-repro.json`). Debugger timing is not ordinary acceptance. No benchmark process remained running.

The unchanged executable's 4/5/8-layer 30-second repeat recorded 900 presentations each, drop/repeat 0 and cumulative late 0/1/0, exit 1. Report: `temp/video-loop-failed-cases-repeat.json`. Sparse lateness remains reproducible but varies across runs.

Video diagnostics now retain the first late presentation's frame/source, elapsed time since diagnostic reset, decode, queued, presented and render-to-present durations in fixed-size values. Reset clears this evidence. The existing `decodeState()` keeps its usual summary; the new explicit `decodeState(true)` formats the extended snapshot only at the benchmark's completed-measurement boundary. `videoTimingByLayer` includes every layer's ID/source/diagnostics, and the offline manifest preserves those records. This avoids adding detailed string formatting to normal frame updates.

The initial extended-format experiment (before restricting formatting to cold snapshots) recorded one late frame in a 5-layer 60-second run: at about 41.475 seconds, frame/source 74, decode 0 ms, queued 16.09–17.00 ms and render-to-present 17.20–17.48 ms across the five layers. Report: `temp/video-first-late-mixed-5layer-60s.json`. It is instrumented diagnostic evidence, not proof that background decoding or GPU execution contributes nothing. With cold formatting, the subsequent five-layer 60-second run recorded 1800 presentations, drop/late/repeat 0, exit 0 (`temp/video-first-late-cold-5layer-60s.json`); run variability prevents a causal performance claim.

The render tick now defers drawing while the matching Composition frame differs from the active PlaybackService clock. The service's existing synchronized FrameChanged notification requests the draw afterward. This prevents drawing/acknowledging a cached new frame with old layer state; no new clock, signal, queue, blocking wait or allocation is introduced. Paused, different-Composition and render-queue paths keep their previous behavior. The mismatch's occurrence rate and exact contribution to lateness are not yet measured.

After the controller build/link, mixed 5/8-layer, logical 1920×1080, RAM-only, 3-second windows looped for **60 seconds** both recorded **1800 presentations**, drop/cumulative late/repeat 0, exit 0. Report/CSV: `temp/video-composition-sync-gate-60s.json`, `temp/video-composition-sync-gate-60s-export.csv`. Native validation covered mixed 2/8-layer 1-second windows looped for 3 seconds: both 90 presentations, drop/repeat 0, late 1/0, fault exceptions 0, D3D12 ERROR/CORRUPTION and CRT heap/assertion errors 0, normal exit 1 for the timing gate. Logs/report: `temp/video-native-composition-sync-gate-debug.log`, `temp/video-native-composition-sync-gate-repro.json`. No benchmark process remained running. The full 1..8 matrix, repeated stability, no-cache behavior, viewport interaction and sustained disk-enabled coverage still need revalidation on this revision.

### 実tickerとGUI受け渡し診断（2026-10-13）

`temp/video-gui-dispatch-profile.json/.log` は8混合レイヤー、Qt測定ループ、disk-ready、3秒cache window／3秒測定の診断試行。ticker63件は中央値1.556ms、最大28.623ms、GUI投稿からcallback開始64件は中央値3.910ms、最大32.710ms、10ms超20件。drop0／repeat0／late2で未達。診断有効の値は通常実行の性能合格には使わない。独立Python待機probeの約15msを実tickerへ当てはめる説明は支持されず、共通PreciseTickerは変更していない。次にPresent呼び出しと提示後bookkeepingを個別にbounded計測する。

- 提示区間の診断結果（2026-10-13）: `temp/video-present-stage-profile.json/.log`。8混合レイヤー、Qt、disk-ready、cache window1秒／測定3秒。64件のPresent中央値3.3755ms／最大7.414ms、提示後bookkeeping中央値0.240ms／最大1.714ms、GUI待ち中央値4.3595ms／最大23.064ms。drop／late／repeatは0でexit0。ただし3秒windowの前回とは条件が異なる短診断であり安定性合格や改善量を示さない。提示後更新だけを主因とする説明はこの結果では支持されず、GUIの他の処理占有／描画開始前を引き続き調べる。

### 再生中の描画worker維持（2026-10-13・検証中）

`temp/video-idle-stop-profile.json/.log` の8混合レイヤー、cache window1秒／測定3秒で、idle停止64件のうち61件が再生中で、連続frameSerial58–118に対応した。既存tickerは停止時join／再開時thread生成を行うため、再生中は描画要求が空でもworkerを維持する最小変更をcontroller側へ入れた。停止中idle、明示停止、dirtyなしの描画省略、既存Qt投稿のcoalescingは維持する。既存1ms周期の小さなQtイベント確保が再生中idleにも続く代償がある。共通PreciseTickerには変更を入れていない。診断無効の8レイヤー60秒／3秒window／disk-ready試行を実行中で、改善は未確認。

- 2026-10-13 実行結果: `temp/video-playback-worker-retain-60s.json`。修正後、診断無効、8混合レイヤー、Qt、disk-ready、3秒window／測定60秒でpreparedRAM90／disk90、描画1800／提示1800、drop0／late0／repeat0、exit0。最大diagnostic sample7.5894ms。旧全matrixの8レイヤーlate27との条件は主要項目が一致するが、別試行かつmatrix内順序・熱状態差があるため因果や安定性はまだ断定しない。全matrix反復、長い固有フレーム、物理scanoutは未完了。

- 2026-10-13 native確認: `temp/video-native-playback-worker-retain-debug.log` はexceptions0、D3D12 ERROR／CORRUPTION等の重大エラー0、正常な合否終了exit1。2／8レイヤー各3秒、window1秒、disk-ready、Qtで提示／描画90ずつ、drop／repeat0、late1／0。debugger下の2レイヤーはタイミング未達。Live Object警告は残る。通常実行の性能合格として扱わない。1–8レイヤー各60秒、window3秒の通常全matrixを開始し、`temp/video-worker-retain-full-matrix-60s-launch.json` に実行前hashとargumentsを保存した。終了時hash／exitは終了後に保存する。

### 実行記録ランナー（2026-10-13）

`python tools/VideoPreviewBench/run_recorded.py --executable <bench.exe> --output-prefix <new-prefix> -- <bench arguments>` で、新規prefixのJSON report／log／launch記録を作る。`--report` はrunnerが指定し、環境は呼び出し元から継承する。exeとArtifactStartup.jsonの実行前後hash、選択した診断環境値、引数、mode、exitCodeを保存する。既存出力があれば実行前exit2で拒否する。hashは読み込まれたDLLや実効起動設定の証明ではない。現在のmatrixは従来temp runnerで開始済みで、このrunnerに切り替えていない。helpと既存prefix拒否だけを確認済みで、新runnerからの実試行はまだ未検証。

### worker維持・全matrix終了（2026-10-13）

`temp/video-worker-retain-full-matrix-60s.json` は1–8レイヤー各60秒、Qt、disk-ready、window3秒。全case提示1800／描画1800、preparedRAM90／disk90、drop／repeat0。lateは0／0／0／0／0／0／5／1でexit1、未達。launch前後exe hash一致、observerは終了確認済み642件、OS peak working set11413118976bytes（全process／途中開始、cache単独ではない）。CSVとmanifestへlaunchとmemoryのfingerprintを添付した。

7レイヤー最初のlateはframe54・開始1806.25ms、decode0、queued6.45ms、render-to-present29.37ms、全体35.83ms。8レイヤーの最初のlateはProRes layer2・frame59・開始55969.30ms、decode0、queued1.60ms、render-to-present31.79ms、全体33.39ms。単独8レイヤーの成功で安定性は証明できず、次はrender区間全体とPresent／uploadをbounded診断する。

### 小submitのimmediate経路・検証中（2026-10-13）

再生中の2D送信は3／9packetで、旧deferred経路のExecuteCommandListsは最大11.808msだった。DiligentImmediateSubmitter.cppmの選択条件を16packet未満は既存immediate経路へ変更した。大きなbatchとcompute consumer例外、描画順序／PSO／resource transitionを維持。境界16の最適性、他描画内容の表示一致は未確認。共通submitterの変更なので動画以外の小batchにも作用し、表示確認が必要。Diligent依存や公開APIは未変更。

修正後7レイヤー診断は180非空submitすべてimmediate、record中央値0.3815ms／最大1.897ms、drop／late／repeat0。native2／8レイヤー各3秒はexceptions0、重大D3D12エラー0、late1／0で正常合否終了exit1。Live Object警告は残る。7／8レイヤー各120秒、window3秒、Qt、disk-ready、診断無効の通常試行を開始（temp/video-small-submit-immediate-7-8-120s）。長い固有source／他レイヤー表示／physical scanoutの証明ではない。

### 停止後GPU画像保存オプション（2026-10-13）

`--capture-final-frame` は `--report` 必須。測定・診断集計・停止後に既存renderer.readbackToImageを明示呼出しし、reportと同じdirectoryへ `<report stem>-layers-N-gpu.png` を保存する。既存PNGは上書きせず、保存失敗時は全体exit1。caseのpostMeasurementCaptureに取得元／path／size／savedを記録し、physicalScanoutVerifiedはfalse。性能のpassed値とは別結果で、画像保存自体は性能gateへ混ぜない。通常測定では指定しない。現在の7／8レイヤー120秒試行はこの追加前のexeで進行中で、保存オプションは終了後にビルド・実行する。停止後GPU targetの単一画像であり、再生中全frame／frame identity／物理scanoutの証明ではない。

### 小submit経路・120秒試験終了（2026-10-13）

`temp/video-small-submit-immediate-7-8-120s.json` は7／8混合レイヤー各120秒、Qt、disk-ready、3秒cache window、診断無効。提示／描画は3600／3601、両caseのdrop／late／repeatはすべて0、preparedRAM／diskは90、exit0。launch前後exe SHA256一致。一回の短いcache反復であり、長い固有source、全matrix反復、他描画内容とphysical scanoutの検証は未完了。

画像取得元の調査では、rendererのactiveColorViewはswapchainのcurrent backbufferを優先する。Present後にはbackbufferが切り替わり得るため、停止後readbackを最後に提示したframeとして扱わない。保存画像はGPU targetの診断資料に限る。

保存オプションのmain object／bench linkは成功。report未指定はexit2。`temp/video-gpu-capture-2-8.json` は2／8レイヤー各3秒、window1秒、Qt、disk-ready、提示90、drop／late／repeat0、exit0。両画像を2400×1195で保存し、目視で動画内容と非黒画像を確認した。last presented frameとの一致、全frameの鮮度、色の数値一致は未検証。1–8各60秒の通常matrixを `temp/video-small-submit-full-matrix-60s` で開始した。

次ビルドのcapture metadataには `presentedFrameIdentityVerified=false` と `mayReadRotatedBackBuffer=true` を追加した（未ビルド）。`--capture-final-frame` はcurrent GPU targetを読み、最終提示frameの証拠には使わない。

### disk候補数のbounded診断（2026-10-13・未ビルド）

探索上限修正に、既存PROFILE_SETUP有効時のみ最大32回の `VideoPreviewDiskHydration` 診断を追加。実際の候補数／limit／成功数／所要時間を出す。通常実行は追加clock／文字列整形を行わない。1–8レイヤー通常matrix終了後にServiceとbench mainの既存objectだけをcompileし、benchをlinkする。新しいCMake生成／全体ビルドは行わない。

### 小submit経路の全matrix終了（2026-10-13）

`temp/video-small-submit-full-matrix-60s.json` は1–8混合レイヤー各60秒、Qt、disk-ready、3秒cache window、診断無効。全8caseの提示／描画1800、drop／late／repeat0、preparedRAM／disk90、exit0。launch前後exe SHA256一致。外部memory observerはcase3途中から781 sample、終了確認、OS lifetime peak working set11296538624bytes。cache単独やcase別の値ではない。CSV／manifestにlaunchとmemory記録を添付した。長い固有source／同条件反復／physical scanoutは未確認。

探索候補上限・bounded診断・capture frame identity metadataを含むService／bench main既存objectのcompileとbench linkは成功。新exeでnative exception／capture確認を開始した。通常性能matrixはこの新exeでは未確認。

新exeのnative試行 `temp/video-native-hydration-capture-debug.log` はexceptions0、D3D12 ERROR／CORRUPTIONとCRT heap／assertionの重大行0、正常合否終了exit1。2／8レイヤー各3秒は両提示90、drop／repeat0、late1／0で、2は性能未達。デバッガ有効の性能結果を通常試験の合格には使わない。両画像保存成功、frame identity／scanout未確認のmetadataも反映。Live Object警告は残る。次にPROFILE_SETUP有効のcandidate診断を2レイヤーで開始した。

### 探索上限の実行確認（2026-10-13）

`temp/video-hydration-candidate-profile-summary.json` は32診断sample、最大候補9、成功0のsample2件でも上限以内。中央値0.407ms／最大6.955ms。2レイヤーの診断試行は提示90、drop／late／repeat0、正常exit0。これは診断有効の短試行で、通常性能の合格証明にはしない。

新exeで診断無効の1–8各60秒、Qt／disk-ready／3秒windowの通常matrixを `temp/video-hydration-bound-full-matrix-60s` で開始した。既存7／8各120秒と全matrix成功は別exeの証拠として区別する。

### 測定診断負荷の分離（2026-10-13）

hydration-bound全matrixは中断後にprocess／session消失を確認。reportなし、launch exit nullのため正常終了と扱わない。ログでは1／3／4layerのlate1／1／2、2layerはlate0。終了原因は未確認。

`--final-only-diagnostics` を追加。周期callbackは診断取得を省略し、終了時の累積late／repeat／dropと提示回数による同じgateを維持。renderer時間sampleと周期sample最大時間はこのmodeでは取得しない。reportにdiagnosticSamplingPolicyを保存し、export／比較条件にも追加。既定modeは変更しない。過去reportのpolicy欠落は不明として扱う。ビルド／実行確認中。

### ケース境界checkpoint（2026-10-13・未ビルド）

report指定時は各case終了／cache準備失敗後、`<report>.partial.json` をQSaveFileで原子的に保存する処理を追加。専用schema、complete=false、expectedCaseCountを記録し、最終reportとは区別する。measurement外でのみJSON化／I/Oし、case途中の故障は直前の完了caseまでを保持する。通常exporterはcheckpoint schemaを受け付けない。runnerは残存partialも既存出力として起動前拒否する。現在のfinal-only試行には未反映。compile／runtime確認は試行終了後。

checkpoint補助検証（2026-10-13）: `temp/video-checkpoint-validation/result.json`。残存partialのみのprefixでrunnerはexit2、bench起動／launch作成なし。専用checkpoint schemaを通常exporterへ渡すとexit2、CSVなし。C++保存処理自体は未ビルド／未実行で、これらはPython境界の検証だけ。

### 終了時のみ診断の60秒試験（2026-10-13）

`temp/video-final-only-1-8-60s.json`、Qt／disk-ready／3秒window、diagnosticSamplingPolicy=final-only。1／8layer両提示／描画1800、drop／repeat0、late0／18、正常exit1、launch前後hash一致。8layer最初のlateはframe1、開始140ms付近、decode0、queued約25ms、render-to-present約38ms、全体約63ms。周期診断を省いても未達。従来成功trialとはexeと実行条件が異なるため因果比較ではない。CSV／manifestにlaunch記録添付済み。checkpoint追加のbench main compile成功、link進行中。

### checkpoint実行確認と同exe比較（2026-10-13）

`temp/video-checkpoint-runtime-1-2.json` は1／2layer各3秒、Qt、disk-ready、window1秒、final-only診断。両drop／late／repeat0、正常終了。case2準備中にpartialがcomplete=false／予定2／保存済み1で存在することを確認。終了後はpartialの2caseとfinal reportのcasesが完全一致。通常exporterのschema拒否とrunner残存partial拒否も確認済み。

同exe、8混合layer各60秒、window3秒、Qt、disk-ready、周期→final-only→final-only→周期の4trial比較を `temp/video-sampling-abba-run.py` で開始。診断無効。性能gateは同じで、diagnosticSamplingPolicyを別条件として保存する。比較中はビルド／exe更新を行わない。小さい逐次標本であり統計的因果証明にはしない。

診断方式比較のoffline集計は `compare_sampling_reports.py <launch-array> --output <new-json>`。診断方式をgroupにし、dispatch mode／source／cache／pixel寸法／durationを共通条件として照合する。前後exe hash不一致、policy欠落、診断profile有効、異常終了reportを拒否する。help確認済み、実4trial集計は終了待ち。比較器の出力は新規pathを指定する。

比較器のoffline境界検証（2026-10-13）: `temp/video-sampling-comparator-validation/result.json`。post-run hash不一致／profile有効／異常exitをexit2で拒否し出力なし。保存済み1／8layer reportはhash一致でもsameReportedConditions=falseとして正しく区別。既存出力pathも拒否する。これらは比較器の検証で、進行中4trialの性能結果ではない。

ABBA trial1（2026-10-13）: 周期診断、8layer、60秒、提示／描画1800、drop／repeat0、late7、正常exit1。最初のlateはframe6／8、decode0、queued約0.6–1.7ms、render-to-present約32–39ms。診断sample最大4.7738msは開始13786ms。CSV／launch添付済み。trial2進行中で全比較の結論は未確定。

比較出力の各trialへ既存exporterのfirst_late_eventsを再利用して全layerの最初の遅延snapshotを追加。保存済みfinal-only1／8layer reportで0／8event一致を確認（2026-10-13）。集計中にbenchを起動・変更しない。ABBA trial2は測定開始を確認。

### ABBA途中2trialの照合（2026-10-13）

`temp/video-sampling-abba-first-two-comparison.json` は入力／cache／寸法／duration／dispatch共通条件とexe前後hash一致を確認。周期late7、final-only late1、両提示／描画1800・drop／repeat0で未達。trial2最初のlateはframe62、開始50223ms、decode0、queued155.08ms、render-to-present18.31ms、全体173.39ms。trial1のrender-to-present約32–39msと異なり、今回の単発lateはGUI受け渡し区間が大きい。OS scheduling、GUI callback、cache競合などの原因は未検証。診断方式の因果効果は4trial終了まで結論を出さない。trial3進行中。

計測解釈の訂正（2026-10-13）: firstLateQueuedMsはVideoLayerのrequestedAt→markRenderQueuedの区間であり、純粋なGUI投稿→callback開始ではない。RAM経路はrequestedAtを保持し、controllerの描画前にmarkRenderQueuedする。155msをGUIキュー待ちだけと断定する説明は不正確で、フレーム要求から描画開始までの合計として扱う。GUI dispatch timestamp診断と描画前stage計測を同一frameで照合する必要がある。

ABBA trial3（2026-10-13）: final-only診断、8layer60秒、提示／描画1800、drop／late／repeat0、正常exit0。3trialの共通条件とexe前後hash一致をoffline照合済み。周期late7、final-only late1／0。trial4の周期診断が進行中で因果結論は未確定。

既存GUI dispatch診断へframeAtCallbackとframeSerialBeforeを追加（未ビルド）。既存profile有効／64sample上限の中でのみcomposition位置を読む。workerからcompositionを触らず、callback時点のframeと投稿要求frameが同じとは断定しない。ABBA終了までexeへ反映しない。

dispatch frame診断の補強（2026-10-13・未ビルド）: callbackログのframeAtCallback／frameSerialBeforeに加え、render durationログへframeAtRenderStartを追加。両値はGUIで取得し、既存profile有効／sample上限内だけで読む。renderFrameCounterはrenderOneFrameImpl末尾で増えるがskipもあるため、before+1の機械的joinを正しいと断定しない。frame値／serial／実描画記録を照合する。guard付きexact controller object＋bench link helperを準備。ABBA trial4測定中のため未実行。

### ABBA比較終了（2026-10-13）

`temp/video-sampling-abba-comparison.json`。同exe前後hash／報告された共通条件一致、8layer各60秒／Qt／disk-ready／3秒window。周期診断late7／0、final-only late1／0。両方式2回中1回合格、全trial drop／repeat0。診断方式のみで安定性は説明できず、既定方式を切り替える根拠にはしない。フレーム対応付きdispatch／render診断のcontroller object compileを開始。比較exeの結果と次exeを区別する。

### frame付きdispatch診断実行（2026-10-13）

controller compile／bench link成功。`temp/video-dispatch-frame-identity-profile.json` は8layer、3秒測定／1秒window、Qt、disk-ready、PROFILE_SETUP有効、final-only診断。提示90、drop／late／repeat0、正常exit0。`summarize_dispatch_profile.py` により64callback中央値0.084ms／最大14.628ms、64render中央値10.7835ms／最大21.992ms。callback frameは1–6のみで上限を使い切り、renderは0–29を覆う。自動frame joinは未確認として出力。診断有効短試行なので通常性能合格には使わない。次はidle callbackでsample上限を消費しない位置へ診断を移し、frame要求→callback→pre-drawの範囲を追う。

実描画callbackのsample選択（2026-10-13）: queuedMsとframeAtCallbackはGUI callback入口で確定し、ログ／sample countだけをdirty／in-progress／composition同期gateを通過した描画直前へ移動。idle／skipは64件を消費しない。入口から描画直前までをqueuedMsへ加算しない。診断無効では追加時計／composition参照／整形なし。exact controller object compile進行中。性能経路の描画／同期gateは変更なし。

実描画dispatch診断のbuild／link成功（2026-10-13）。temp/video-rendered-dispatch-profileは8layer3秒／window1秒、診断有効、提示90、drop／late／repeat0で正常終了。idle callbackを除いた実描画sampleの詳細はsummary JSONに保存。診断有効短試行は通常性能合格に使わない。

### 長時間dispatchのbounded slow診断（2026-10-13）

最初の64実描画callbackに加えて、callback待ち20ms超を最大32件の別counterで記録する修正を追加。上限はcontroller寿命ごと、合計最大96ログ。profile有効時だけ待ち時間を検査し、frame位置の参照と整形はsample対象のみ。baseline終了後もslow上限到達まで時計読取は続くため、診断試行は通常性能合格に使わない。idle／skipのログは出さない。render duration側は従来64件のままなので、長時間slowの描画区間が常に併記されるとは限らない。compile／link確認中。

slow dispatch診断のcontroller object compile／bench link成功（2026-10-13）。8layer各60秒、3秒window、Qt／disk-ready、PROFILE_SETUP有効／final-onlyで `temp/video-slow-dispatch-60s-profile` を開始。解析器は20ms超のcallback待ちを別配列に保存する。baselineとslow追加sampleを合わせた全体medianは通常callback分布の推定ではない。正常性能の合格判定には使わない。

### slow dispatch60秒診断終了（2026-10-13）

`temp/video-slow-dispatch-60s-profile.json` は8layer／Qt／disk-ready／3秒window、PROFILE_SETUP有効、final-only。提示／描画1800、drop／late／repeat0、正常exit0、launch前後hash一致。64callback中央値0.598ms／最大4.814ms、64render中央値11.229ms／最大19.127ms、20ms超slow callback0件。問題未再現であり、過去の単発lateが解決した証拠ではない。

同exeの診断無効8layer300秒、同3秒windowを `temp/video-eight-layer-300s-normal` で開始。長い固有source再生とは別で、短いcache反復の長時間安定性を確認する。

### 8レイヤー5分試験終了（2026-10-13）

`temp/video-eight-layer-300s-normal.json` は診断無効、final-only、Qt／disk-ready、H.264とProRes混在8layer、3秒cache window。300秒で提示／描画9000、drop／late／repeatすべて0、正常exit0。case checkpointと最終caseは一致。CSVとlaunch記録付きmanifestを `temp/video-eight-layer-300s-normal-export` に保存。実cache画像2400×1195、90frame。短いcache反復の成功であり、長い固有source・rolling cache・物理scanoutの検証とは区別する。過去の単発lateが消滅した保証ではない。

最新controllerのネイティブ例外確認（2026-10-13）: `temp/video-native-final-controller-debug.log`／`temp/video-native-final-controller-repro.json`。2／8layer各3秒、window1秒、Qt／disk-ready、final-only、Win32 debugger下で終了までexception0。提示各90、drop／repeat0。2layer late1、8layer late0、exit1は性能判定の不合格。debugger実行の遅延は通常実行の合格根拠にしない。今回の範囲で例外は再現せず、全条件で解決済みとは断定しない。

### 固有H.264連続再生の基準（2026-10-13）

`temp/video-unique-h264-no-ram-1-10s.json` はH.264 1layer、10秒、loopなし、合成RAM cacheなし、Qt、final-only、診断無効。提示145、描画161、drop0／repeat0だがlate106で不合格（30fpsなら約300提示必要）。最初のlateはframe32、request-to-render30.35ms、render-to-present10.41ms、total40.76ms。decode時間0のsnapshotだけでデコーダ全体のコストを断定しない。shared frame cache内の先読みは継続しており、全cacheを無効にした試験ではない。

先読みcacheの値コピーを通常要求と同じputSharedへ変更し、upload workerへ渡すimmutable payloadを保持する。frame／byte上限・世代確認・upload queue経路は既存を使う。性能改善と例外安全性は再検証待ち。

### 部分準備で長い範囲を計測する設定（2026-10-13）

`--ram-warmup-frames N`（8..128、source range内、RAM cache必須）を追加。先頭N frameだけのRAM／任意disk準備を待ち、測定前にsource全範囲へRAM build rangeを戻す。既存serviceのcache／decode fallback／buildを計測し、非同期rolling実装の完成とは扱わない。`--seconds` の提示数条件、late／drop／repeatの0条件は変更しない。`--wait-disk-cache` は準備prefixだけに適用される。report／checkpointのramWarmupFramesとcachePolicyでfull準備と区別し、比較器にも条件を追加。旧reportに値がない場合は比較条件不明とする。

先読みputShared修正のobject compile／bench link成功。cacheなし同条件10秒は提示147、late108、drop／repeat0で不合格。変更前145／106から有意な改善は確認できず、1回のclone削減のみ。`temp/video-streaming-prefix-h264-1-10s` で新設定（prefix30、H.264 1layer10秒）の実行確認中。

部分cache試験終了（2026-10-13）: `temp/video-streaming-prefix-h264-1-10s.json` はprefix30のRAM／disk準備成功後、300frame範囲を10.046秒計測。提示141、late100、drop／repeat0、最終提示141、正常exit1（性能不合格）。終了時合成RAM／diskは30frameのまま。CSV／launch manifest保存済み。初lateframe30、request-to-render137.09ms、render-to-present11.38ms、total148.47ms。prefix境界からの供給待ちと再生中cache生成条件を次に調査する。実装は既存経路を測る設定であり、rolling cacheの成功を主張しない。

### 再生中のcapture対象修正（2026-10-13）

部分cache試験で合成cacheが30frameから増えない条件を調査。controllerは生成待ちの描画frameでもbuildQueueNextFrameとの一致を要求し、callbackでもキュー先頭を再検査していた。再生中に生成待ち先頭を通り過ぎると後続のexact frame captureが遮断される。再生中のrequestは描画frame自体をtargetとし、callbackの先頭一致だけを省く。composition ID／build generation／request targetとframeの一致／pending membershipを維持し、paused warmupは従来の先頭一致を維持。既存のbounded async readback経路を使用する。compile／連続再生結果は確認中。

### capture停止条件の追加確認とprefix試験修正（2026-10-13）

先頭一致を外した単独試行 `temp/video-playback-capture-prefix-h264-1-10s.json` でもRAM30のまま、提示145／late105／drop1で不合格。native `temp/video-native-prefix-capture-debug.log` はexception0、通常終了exit1。frame30／120でpendingBuild=falseを確認した。`play()` 最後のprewarmRamPreviewAroundは生成範囲をradiusへ戻すため、prefix設定がradius30まで縮めたことが、この試行で生成待ちが空になる直接原因だった。先頭一致条件の修正だけでは説明できなかった。

prefix準備数とradiusを分離し、radiusは測定source frameCountとする。先頭Nだけ準備し、開始時のprewarmで測定範囲のpendingを維持する。メモリLRU128上限は変更しない。controllerの準備有無判定2箇所も、画像全体をcloneするcachedFrameImageBufferから既存のcachedFrameImageBufferShared参照へ変更。exact sourceframeと互換性を確認する既存APIを利用。共有化が必要な古い値cache entryは既存getSharedで一度だけ小さな共有payload生成があり得るが、大容量cloneを判定のたびに作らない。exact compile／link成功、`temp/video-prefix-radius-h264-1-10s` で再計測中。

範囲修正後のprefix試行（2026-10-13）: `temp/video-prefix-radius-h264-1-10s.json` はRAM30→48／disk30→47で生成増加を確認。一方提示47、late16、drop11、repeat0で不合格。diskなしの `temp/video-prefix-ram-only-h264-1-10s.json` は提示94、late64、drop1、RAM95で不合格。disk保存を含む負荷の影響はあるが、RAM取り込みだけでも要求性能を満たさない。queue mutexがPNG encode／manifest／budget処理を通して保持される事実と、GUI側の変換／cloneを引き続き確認する。

QImage版storeRamPreviewFrameImageはRAM用とtask用に同じ画像を2回RGBA32Fへ変換していた。変換を1回にして既存CPU版overloadへ渡す変更を実施。RAM／task所有のcopy、世代判定、既存writer経路は維持。compile／link確認中、効果は未検証。

単回Qt境界変換の試行終了（2026-10-13）: `temp/video-single-convert-prefix-h264-1-10s.json` は提示48、late19、drop1、repeat0、RAM49／disk48、正常exit1。提示47から有意な速度改善は確認できない。世代・pending確認を保ったcapture生成は進むが、live生成と保存を含む経路は依然不合格。最新exeのnative prefix試行 `temp/video-native-single-convert-debug.py` を開始し、例外／GPU errorを別に確認する。

### QImageを経由しないnative readback（2026-10-13）

`ArtifactIRenderer::readbackTextureViewToBufferAsync` を追加し、Composition controllerのcache captureで使用する。既存3-slot staging／fence ringにslotごとのImageUploadBufferを保持。RGBA8 byte列への変換はworkerが直接行い、QImage pixel allocation／QImage→CPU cache変換を挟まない。現在のRGBA16／32F→sRGB8 quantizationは旧経路と同じ計算を維持する。GUI publisherは共有payloadを保持して既存CPU buffer storeへ明示変換する。

slotのbusyに加えpayload useCount>1で再利用を拒否するため、GUI待ちにもleaseが及ぶ。3 slots×各64MiB以下。buffer／staging確保は停止時のcold captureだけ許可し、再生中は容量不足／寸法変更時にfalseでdeferする。既存Qt readback APIは互換caller用に残す。ImageUploadBufferの既存byte containerを再利用し、新規std collectionは追加しない。

GPU→CPU readback自体、GUI上のRGBA32F store、disk writer内のQt PNG encodingはまだ残る。GPU resident preview cache／非Qt disk encodingの完了を主張しない。停止時の小さなSharedPtr／callback確保と既存GUI eventを使い、full-frame byte領域は3slotの冷間準備だけ。interface／implementation compile成功、controller／bench／archive／link確認中。

### native buffer経路の実行確認（2026-10-13）

interface／implementation／controller／bench compile、ArtifactRender archive、bench link成功。`temp/video-native-buffer-prefix-h264-1-10s.json` はprefix30／disk-ready、提示47、late8、drop17、repeat0、RAM48／disk47で正常exit1（不合格）。QImage境界除去だけの性能改善は確認できず。cold pool確保失敗は部分初期化bufferを公開せずfalseを返す修正もcompile／link成功。

最新版の `temp/video-native-buffer-debug.log`／`temp/video-native-buffer-repro.json` はdebugger下で正常exit1、native exception0。D3D12 ERROR／CORRUPTION、Diligent ERROR／Error、assertを検索した範囲で該当0。提示47、late18、drop1、RAM48／disk47で不合格。30frame準備と追加captureが3slotを超えて完了し、lease再利用が進むことを確認。ただし全失敗条件・色alpha parity・Vulkanは未検証。native判定と通常性能判定は混同しない。

既存disk writerはRGBA32F→RGBA8専用変換の後QImageを包んでPNG保存している。OpenCV imgcodecsは親／独立benchの既存include／link依存に含まれるため、非Qt PNG codecを使う親側代替を検討できる。GPU resident cacheの寿命／content identity／総byte budgetを先に固定し、生成queueのgenerationだけを恒久cache identityに使わない。GPU cache自体はまだ未実装。

### GPU resident RAM replay の初期実装（2026-10-13）

停止時の authoritative RAM publication 後に、既存 renderer texture LRU 内でフレーム位置 modulo 32 の固定 owner を温める。再生は RAM upload revision と既存 image content key が一致する resident texture を参照する。residentOnly overload は miss 時に変換・GPU upload・resource 確保をせず nullptr を返す。元の4引数APIを維持し、miss時の既存単一 streaming slotを維持する。

これは CPU キャッシュから準備済み GPU texture への replay 改善であり、GPU 合成結果だけで cache capture を完結する経路ではない。cold warmup に追加 upload がある。既存 50-entry / 512 MiB 総 LRU を拡大せず、32 owner の texture は他の sprite と予算を共有する。大きい画像や eviction、長いループでは resident miss が起こる。新規 std container は追加せず既存 Core.ArtifactArray の ArtifactStaticArray を使う。native exception、性能、色・alpha parity、Vulkan の確認は実行結果ごとに記録する。


resident replay の短いloop診断: `temp/video-resident-replay-loop-h264-profile.json` はH264 1 layer / 1秒30frame loop / 10秒で提示300、late/drop/repeat 0、正常exit0。測定中の texture profile 300 samples はcreate/updateがすべて0ms。ただし整数ms診断だけで転送byte数0を証明しない。profiling有効なので通常条件の性能試行とは区別する。長尺prefix試行は不合格のまま。


最新版native prefix試行: `temp/video-resident-replay-debug.log` は正常terminal exit1 / native exception0。D3D12 ERROR/CORRUPTION、Diligent Engine ERROR、assertion failedの検索該当0。`temp/video-resident-replay-repro.json` は提示46、late18、drop1、repeat0。native debugでの例外判定と通常性能判定を区別する。native環境の長尺生成は引き続き不合格。


resident replay の通常混在試行: `temp/video-resident-replay-mixed-2-8-10s.json` は H264 / ProRes HQ を交互に配置した2 / 8 layer、1秒30frame cache loop、各10秒、diagnostic profiling無効。両case提示300、late/drop/repeat 0、正常exit0。これは長尺生成の合格・実際のscanout画像の検証を意味しない。その後、32 ownerの固定配列をimport済みCore.ArtifactArrayのArtifactStaticArrayへ変更し、controllerと独立benchの再linkを実施する。


ArtifactStaticArray版のcontroller compile / standalone link成功。最新版再試行 `temp/video-resident-static-array-mixed-2-8-3s.json` は同じ混在2 / 8 layer、1秒30frame loop、各3秒で両case提示90、late/drop/repeat 0、正常exit0。通常性能の短いloop確認。long unique source、RGBA parity、Vulkan、最新版固定配列版のnative debugger再試行は未確認。直前版native exception0の証拠とはexe fingerprintを分ける。
