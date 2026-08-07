# Async Asset Streaming / Diligent Upload Milestone

**最終更新:** 2026-08-07
**ステータス:** Implemented / Build Verified

## 実装状況（2026-08-07）

- Phase 0の初期監査を実施し、現行DirectStorage実装が存在しないこと、DX12 native device取得経路、同期的な連番prefetch、既存GPU texture cache/upload箇所を確認した。
- Phase 1の最初のsliceとして、`ImageSequenceSource::prefetchFrame()`をboundedなbackground decodeへ変更した。
- prefetch結果にはgeneration検証、重複抑制、最大in-flight数、bounded completed queueを適用した。
- decoded frame cache accessをmutexで保護し、source size/mtime検証後にprefetch結果を取り込む。
- compile-optionalな`DirectStorageReader`を追加し、DirectStorage 1.3 file-to-memory queue、status完了確認、QFile fallback、統計を実装した。
- `ARTIFACT_DIRECTSTORAGE_SDK_DIR`から公式SDK headerを追加し、`dstorage.dll`を配備するCMake経路を追加した。SDK無しではportable fallbackのみをコンパイルする。
- 静止画prefetchはreaderで一度だけfile bytesを取得し、OIIO `IOMemReader`から`QImage`互換結果と`ImageF32x4_RGBA`を一回のdecodeで生成するようにした。
- bounded worker pool、priority、generation、cancellation、request coalescing、read/completed byte budgetを持つ`AsyncAssetReadScheduler`を実装した。
- `GPUTextureCacheManager`のCPU画像uploadをbounded `DiligentUploadCoordinator`へ移し、owner render laneで件数・byte上限付きbatchとしてDiligent `ITextureUploader`のstaging resourceからcopyする。GPU完了後のstaging recycleもDiligentへ委譲し、DX12/Vulkanとも同じ経路を使う。
- pending uploadはowner/source version変更、device reset、clearでcancelし、世代不一致の結果をpublishしない。pending bytes/jobsを既存cache statsへ追加した。
- 静止画の`ImageF32x4_RGBA`派生キャッシュ（version、source revision、subimage policyをkey/headerへ保持、atomic write）を追加した。cache hitは同じasync readerを通り、DirectStorage利用可能時はfile-to-memory経路の対象になる。
- DX12 native resourceへのDirectStorage GPU destinationとGDeflateは採用しなかった。現行のloose画像はCPU decodeが必要で、Diligent所有外のnative resource/fenceを持ち込むより、共通Diligent upload経路でDX12/Vulkan parityを維持する方が安全なためである。
- portable fallback構成で`ArtifactRender`と`Artifact`全体をDebugビルドし、リンクまで成功した。
- DirectStorage 1.3 SDK有効構成でも`DirectStorageReader`を実コンパイルし、`Artifact`全体の最終リンクと`dstorage.dll`の実行ディレクトリ配備まで成功した。
- `check_module_hygiene`は実行したが、今回の変更外にある既存の`Artifact.Effect.Rasterizer.RadialBlur`重複exportを検出して失敗した。今回追加したmoduleに対する違反は検出されていない。
- runtimeのDX12/Vulkan実機比較とI/O・decode・upload各stageの性能計測は未実施。採用判定の次段として代表アセットで計測する。

## 目的

静止画・連番画像・3Dアセットのファイル読み込み、デコード、GPU転送を非同期パイプライン化し、UI thread と render submit lane をディスクI/O待ちで停止させない。

DX12ではDirectStorageを選択可能な高速経路として利用し、Vulkanでは非同期CPU I/OとDiligentのGPU転送経路を利用する。上位のasset/layerコードはbackend固有APIを知らず、同じrequest、priority、cancellation、generation contractを使う。

本設計でいう「Diligent経由」は、最終的なGPUリソースの所有・公開・描画時同期をDiligent resourceとして扱うことを意味する。DirectStorageのDX12 API呼び出しだけは、Artifact側の閉じたbackend adapterからnative D3D12 handleへ降りる。

## 現状

- `ArtifactImageLayer` は `QtConcurrent` でOIIO読み込みを先行実行するが、同じ要求から `QImage` と `ImageF32x4_RGBA` の両方を生成する。
- `ImageSequenceSource` は `QImageReader` による同期デコードと8-entry LRU cacheを持つ。
- `ImageSequenceSource::prefetchFrame()` は `frameAt()` を同期呼び出しするため、独立した非同期先読みqueueではない。
- GPU readbackにはring/fenceの土台があるが、asset upload専用のbounded ringと完了contractはない。
- DiligentのDX12 backendからnative `ID3D12Device` を取得する既存経路がある。
- DirectStorage SDK、`IDStorage*` wrapper、capability判定、queue、diagnosticsは現行コードに存在しない。

## 設計原則

1. I/O、decode/transcode、GPU uploadを別stageにする。
2. requestはimmutableな識別情報を持ち、完了時にgenerationとsource revisionを再検証する。
3. queue、staging memory、in-flight upload数は必ずboundedにする。
4. interactive current frameを最優先し、遠方prefetchはpressure時に破棄できるようにする。
5. render threadはファイルI/O、decode、fence waitを行わない。
6. Diligent immediate contextまたはbackend queueの操作はowner laneに限定する。
7. `ImageF32x4_RGBA` またはGPU-ready payloadを本流とし、新しい`QImage`中心経路を追加しない。
8. DirectStorage非対応、Vulkan、HDD、通常圧縮画像では安全なportable pathへ自動fallbackする。
9. DirectStorageをPNG/JPEG/EXR decoderとして扱わない。通常形式のdecodeはOIIO等でCPU実行する。
10. DiligentEngine fork、global signal/slot、暗黙のCPU download/GPU uploadを導入しない。

## 全体構成

```text
Asset / Layer request
        |
        v
AsyncAssetScheduler
  priority / cancellation / generation / budget
        |
        +---------------- File I/O backend ----------------+
        |                                                  |
        |  PortableAsyncReader                DirectStorageReader (DX12)
        |  loose PNG/EXR/mesh                 packed/GDeflate/GPU-ready
        |                                                  |
        +-----------------------+--------------------------+
                                v
                     Decode / Transcode workers
                     OIIO -> ImageF32x4_RGBA
                     packed cache -> GPU-ready payload
                                |
                                v
                         bounded upload ring
                                |
                                v
                    DiligentGpuUploadCoordinator
                    single owner submit lane
                       /                  \
              D3D12 adapter          Vulkan adapter
              DS GPU fast path       staging + transfer
                       \                  /
                                v
                    Diligent texture/buffer handle
                                |
                                v
                     safe publish at frame boundary
```

## 共通contract

以下は概念設計であり、公開module APIへそのまま追加する前に既存source/task contractとの統合を監査する。

```cpp
enum class AssetLoadPriority {
    Interactive,
    PlaybackNext,
    PreviewBuild,
    BackgroundWarm
};

enum class AssetPayloadIntent {
    CpuLinearImage,
    GpuTexture,
    GpuBuffer,
    MetadataOnly
};

struct AssetLoadRequest {
    AssetId assetId;
    Utf8Path sourcePath;
    uint64_t sourceRevision = 0;
    uint64_t generation = 0;
    AssetLoadPriority priority = AssetLoadPriority::BackgroundWarm;
    AssetPayloadIntent intent = AssetPayloadIntent::CpuLinearImage;
    FrameIndex frame = 0;
    SubresourceRange subresources;
    CancellationToken cancellation;
};
```

結果は少なくとも次の状態を区別する。

```text
Queued -> Reading -> Decoding -> ReadyToUpload
       -> UploadSubmitted -> Ready
       -> Canceled / Stale / Failed / Fallback
```

`Ready` publish条件:

- requestのasset identityが現在値と一致する
- source revisionが一致する
- generationが一致する
- GPU fence/timeline valueが完了している
- Diligent resource stateがsampling可能な状態にある

## I/O backend

### PortableAsyncReader

最初に実装する標準経路。DX12/Vulkanの両方で利用する。

- bounded worker poolでファイルrangeを読み込む
- small fileはまとめ、large fileはrange requestとする
- cancellation後の結果はpublishしない
- loose PNG/JPEG/TIFF/EXR等はここからOIIO decodeへ渡す
- Windows固有のoverlapped I/O採用は実測後に判断し、上位contractへ露出させない
- OS cacheを前提とした通常経路と、DirectStorage bypass pathの統計を混ぜない

### DirectStorageReader

Windows/DX12限定のoptional adapterとしてArtifact側に置く。

用途を次の2段階に限定する。

1. file -> memory
   - packed cacheをCPU-visible destinationへ読み込む
   - 通常画像形式は読み込み後にCPU decodeする
   - 導入初期の比較・fallback確認に使う
2. file -> D3D12 resource
   - GPU-ready texture/buffer payloadのみを対象とする
   - GDeflateは専用cache format導入後に有効化する
   - native resource、queue、fenceとDiligent resource stateの所有境界をPhase 0で確認する

DirectStorage request失敗時は、同じrequest identityのままportable pathへ最大1回fallbackする。無限retryは禁止する。

### Vulkanに対するDirectStorageの扱い

DirectStorageのGPU destinationはD3D12 resourceであり、Vulkan imageへ直接書き込む共通経路にはしない。

Vulkanでは次を標準とする。

```text
async file read -> decode/transcode -> mapped staging ring
-> Diligent copy/upload submission -> fence/timeline completion
-> Diligent texture publish
```

専用transfer queueがDiligentの現在のdevice/context構成から安全に利用できるかを先に監査する。利用できない場合も、single Diligent submit lane上でcopyをbatch化し、`WaitForIdle`を使わずfenceで完了管理する。native Vulkan queueへ直接降りる拡張は初期scope外とする。

## Decode / transcode方針

### Loose source assets

- PNG/JPEG/TIFF/EXR/PSD等はOIIOをcanonical decode境界とする。
- 同一requestで`QImage`とfloat imageを無条件に二重生成しない。
- renderer向けは`ImageF32x4_RGBA`または用途に合う明示formatを生成する。
- thumbnail/UIが`QImage`を必要とする場合だけ、別intentまたは明示adapterを使用する。
- color space、orientation、subimage、source revisionをdecode resultに保持する。

### GPU-ready derived cache

DirectStorage効果を最大化する本命経路。

- importまたはidle時にderived cacheを生成する。
- initial formatsはRGBA8/16F raw tileまたはBCn/DDS相当を候補とする。
- mip/subresource単位でrange read可能なindexを持つ。
- cache keyにsource hash/revision、decoder version、working color space、pixel format、mip policyを含める。
- GDeflateはcache format versionを分け、CPU fallback decoderを必須とする。
- cache不整合時はsource assetから再生成し、壊れたcacheを正解として扱わない。

## GPU upload coordinator

### Ownership

`DiligentGpuUploadCoordinator`が次を所有する。

- staging buffer ring
- upload job queue
- backend upload adapter
- per-submit fence/timeline value
- in-flight byte budget
- completed resource publish queue

layerやdecoderがDiligent contextを直接操作しない。

### Batching

- frameごと、またはbyte thresholdごとにcopyをbatch submitする。
- interactive requestは次batchへ即時参加できる。
- small uploadを1件ずつflushしない。
- ring枯渇時にrender threadを待たせず、background producerへbackpressureを返す。
- texture descriptorの差し替えは既存frame-in-flightが安全な境界で行う。

### Memory policy

初期値は設定値ではなく安全な固定上限から開始する。

- CPU read/decode queue: bounded job count
- decoded payload: bounded byte budget
- staging ring: 2～3 slots
- in-flight GPU upload: bounded bytes and requests
- decoded sequence cache: frame countではなくbyte budgetを主基準にする

UMAでは不要なstaging copyを省ける可能性があるが、adapter memory capabilityを確認してから別policyにする。

## Scheduling

優先順位:

1. 現在表示に必要なasset/frame
2. playback方向の次frame
3. RAM Preview build対象
4. 近傍の連番prefetch
5. thumbnail/metadata/background cache warm

同じasset/frame/intent/request revisionはcoalesceする。interactive要求が既存background jobと一致した場合は新規I/Oを増やさずpriorityを昇格する。

seek、asset relink、source更新、composition切替、quality/working-space変更ではgenerationを進める。I/O自体を停止できない場合も、古い結果をdecode/upload/publishへ進めない。

## Backend capability

起動時にtyped capability snapshotを作り、backend選択理由を診断可能にする。

```cpp
struct AssetStreamingCapabilities {
    bool portableAsyncRead = true;
    bool diligentGpuUpload = false;
    bool dedicatedTransferQueue = false;
    bool directStorageAvailable = false;
    bool directStorageGpuDestination = false;
    bool gpuDecompressionOptimized = false;
    bool gpuDecompressionFallback = false;
    bool cpuDecompressionFallback = false;
    bool isUma = false;
};
```

DirectStorageが利用可能でも、loose compressed imageに対して自動的にGPU fast pathを選ばない。payload formatとdestination compatibilityが確認できた場合だけ選択する。

## 診断項目

- request count / coalesced count / canceled / stale
- queue depth and queued bytes per stage
- read latency / throughput
- decode time / transcode time
- staging wait time
- upload batch size / submit count / uploaded bytes
- GPU completion latency
- cache hit / miss / invalidation reason
- selected backend and fallback reason
- DirectStorage compression support
- frame deadline miss count
- render-thread synchronous I/O count（目標0）

診断名はrequestに固定長labelを持たせ、asset path全体やユーザーデータを常時ログへ出さない。

## 導入フェーズ

### Phase 0: 計測と所有境界監査

- 静止画と連番の`read/decode/convert/upload/wait`を分離計測する。
- DiligentのDX12 textureからnative `ID3D12Resource`を安全に取得できる範囲を確認する。
- DirectStorage queueとDiligent context間のresource state/fence contractを確認する。
- Vulkan/DX12の現行upload経路、submit thread、flush/wait箇所を列挙する。
- Diligentから専用transfer queueを利用できるか確認する。

完了条件:

- bottleneckがI/O、decode、CPU conversion、GPU uploadのどこか実測で判別できる
- backendごとのresource ownership図が確定する
- native APIへ降りる最小境界が確定する

### Phase 1: Portable schedulerと連番非同期化

- common request、priority、generation、cancellationを実装する。
- `ImageSequenceSource`の同期prefetchをschedulerへ置換する。
- CPU decode cacheをbyte-budgetedにする。
- GPU uploadは既存経路を維持し、I/O/decodeだけを非同期化する。

完了条件:

- scrub/playback中にUI threadが連番ファイル読み込みを待たない
- stale frameがcacheへ入らない
- DX12/Vulkanで同じ結果になる

### Phase 2: Diligent upload ring

- bounded staging ringとupload coordinatorを追加する。
- copyをbatch化し、fence完了後にresourceをpublishする。
- `WaitForIdle`なしで複数uploadをin-flightにする。
- backend parityとfallback reasonを診断する。

完了条件:

- file/decode N+1とGPU upload Nがoverlapする
- render submit laneがdisk/decode/fence waitで停止しない
- resize/device loss/shutdown時にin-flight resourceを安全にretireできる

### Phase 3: DirectStorage file-to-memory pilot

- Windows/DX12限定adapterを追加する。
- packed test payloadをmemory destinationへ読み込む。
- portable readerとのthroughput、CPU使用率、latencyを同一fixtureで比較する。
- capability不足・HDD・API失敗時のfallbackを確認する。

完了条件:

- DirectStorageを使用したか診断で確認できる
- 出力byte parityがある
- portable pathより悪い環境では自動選択しない根拠が得られる

### Phase 4: Derived image-sequence cache

- indexed、versioned、subresource/range-readableなcacheを追加する。
- source revisionとcolor/decode policyをcache keyへ含める。
- current development priorityに合わせ、静止画・連番画像から採用する。
- corruption/rebuild/fallbackを実装する。

完了条件:

- loose image decodeを毎frame繰り返さない
- cache miss時も正しいsource fallbackがある
- seek時に必要frameを優先してrange loadできる

### Phase 5: DirectStorage GPU destination / GDeflate

- GPU-ready payloadだけをD3D12 resourceへ直接配送する。
- GPU/CPU decompression supportを実行時判定する。
- Diligent resource stateとdescriptor publishをsafe boundaryで統合する。
- Vulkanは同一cacheをportable read + staging uploadで利用する。

完了条件:

- DX12でCPU-side full payload copyを削減できる
- DX12/Vulkanの表示parityがある
- GPU decompression非対応でもCPU fallbackで同じ結果になる

### Phase 6: 静止画・3Dへの展開

- large still、mip/tile、mesh bufferへ同じschedulerを適用する。
- thumbnail、metadata、final exportへ無条件に展開しない。
- workload別測定で効果が確認できる対象だけ採用する。

## 初期対象ファイル

- `ArtifactCore/src/Media/ImageSequenceSource.cppm`
- `ArtifactCore/include/Media/ImageSequenceSource.ixx`
- `Artifact/src/Layer/ArtifactImageLayer.cppm`
- `Artifact/src/Render/DiligentDeviceManager.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 新規adapterが必要な場合も公開`.ixx`変更を最小化し、実装は既存または新規`.cppm`へ閉じる

## 非対象

- 動画decode/loadの新規展開
- DiligentEngine forkまたはgitlink変更
- Vulkan native APIへの直接的な独自upload経路
- 全画像の事前変換強制
- DirectStorageを必須runtimeにすること
- UI設定画面の先行追加
- render contextのmulti-thread同時操作
- 新しいglobal signal/slot
- `QImage`、`QPainter`、Qt compositionを使う新しい本流

## リスク

### DirectStorageを入れても速くならない

loose PNG/JPEG/EXRではdecodeやfloat変換が支配的な可能性がある。Phase 0計測とfile-to-memory pilotを先行し、I/Oだけの改善を全体改善と誤認しない。

### DX12だけ別のresource ownershipになる

DirectStorageがnative resourceへ書いた後、Diligent側がresource stateと完了を認識できなければ破損や同期stallにつながる。native fast pathはadapter内部に閉じ、Diligent resource公開前に明示的なhandoffを行う。

### Vulkan transfer queueの過剰設計

専用queueは常に速いとは限らず、queue family ownership transferも増える。まずDiligentの既存queueでbatched uploadを実装し、実測で専用queueの価値がある場合だけ拡張する。

### memory pressure

read buffer、decoded float image、staging、device textureが同時に存在するとpeak memoryが増える。全stageにbyte budgetを持たせ、consumerが遅い場合はproducerへbackpressureを返す。

### edit/seek後の古い結果

キャンセルだけに依存せず、source revisionとgenerationをupload直前・publish直前に再検証する。

## 成功基準

- render/UI threadで同期file readを行わない。
- 連番再生時にread、decode、uploadがoverlapする。
- current frame要求がbackground warmより優先される。
- DX12/Vulkanでpixel outputとcolor interpretationが一致する。
- fallbackを含むbackend選択理由が診断できる。
- queueとmemory使用量がboundedである。
- DirectStorage採用対象ではCPU使用率またはframe deadline missがportable pathより改善する。
- 効果がないworkloadではDirectStorageを使用しない。

## 依存・関連文書

- `Artifact/docs/MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md`
- `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`
- `Artifact/docs/planned/MILESTONE_MULTI_FRAME_PREVIEW_RENDERING_2026-06-29.md`
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md` の `C-ARC-6 Typed Background Task Runtime`
- Microsoft DirectStorage request destination type: <https://learn.microsoft.com/en-us/windows/win32/dstorage/dstorage/ne-dstorage-dstorage_request_destination_type>
- Microsoft DirectStorage samples: <https://github.com/microsoft/DirectStorage>
- Khronos Vulkan queue guidance: <https://docs.vulkan.org/guide/latest/queues.html>
- Khronos Vulkan staging buffer guidance: <https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/02_Staging_buffer.html>

## 推奨判断

採用する。ただし最初からDirectStorage中心にはしない。

1. portable schedulerと連番非同期化
2. Diligent upload ring
3. DirectStorage file-to-memory pilot
4. derived cache
5. DX12 GPU destination/GDeflate

この順序なら、DirectStorageの効果が小さい環境でもPhase 1～2の改善がDX12/Vulkan双方へ残り、DX12 fast pathだけを安全に追加できる。
