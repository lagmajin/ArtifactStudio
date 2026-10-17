**最終更新:** 2026-10-18
**ステータス:** In Progress — active packet-order paths, Particle / retained Billboard / transferred Gizmo / solid and textured 3D Card / bounded 3D shape triangle / warm-atlas ordinary and transformed GlyphText deferred parity with distinct-glyph and post-seed atlas-update image assertions, projector fallback, offscreen target continuity, the bounded asynchronous base-to-overlay recording window, Composition point-light frame images, the Cloner async-dispatch → immediate-clear → CPU-fallback image contract, and a renderer-owned viewport display-transform pass. `ArtifactIRenderer` can retain one base batch while the owner records later packets into a second pre-reserved buffer; immediate-render barriers and present drain the earlier batch first. Opt-in profiling separates worker record / FinishCommandList, owner ExecuteCommandLists submission, worker FinishFrame, and Composition render-to-present timing. Composition's full frame body, immediate-context ownership, and `present` remain on the caller thread, so GUI rendering is not yet generally non-blocking. Debug Ninja/MSVC `Artifact` and the Composition point-light integration test build and pass; the app post-build deployment still reports missing Qt translation-catalog / debug-DLL paths.

# Render Thread Separation — Phase 4 Feasibility & Design

親計画: [`MILESTONE_EFFECT_PARAM_UI_PERFORMANCE_2026-10-16.md`](MILESTONE_EFFECT_PARAM_UI_PERFORMANCE_2026-10-16.md)
の Phase 4（描画の GUI スレッド分離 / レンダラ・最大工事）。

本ドキュメントはフィージビリティ評価・設計提案と、段階導入した変更の状態を記録する。
Diligent/DX12 の severe 領域のため、AGENTS の指示どおり設計レビューを前提とし、本フェーズは
Phase 0/1 の完了後に独立マイルストーンとして扱う。

**設計判断:** `deferredContext` 化は一気に行わず、描画パス単位で段階的に進める価値がある。
ただし deferred context は immediate context の置き換えではない。現在も
`DiligentImmediateSubmitter` の一部経路で使われており、同じ呼び出しスレッド内で記録してから
immediate context で実行している。そのため、まず記録可能なパスの境界と安全性を整え、必要な箇所だけ
worker へ移す。GUI のブロックをなくす最終段階では、immediate context と present の所有スレッドも
別途移す必要がある。

**Worker recording の初段（2026-10-17、検証済み）:** `DiligentImmediateSubmitter` に
Deferred command list の記録と `FinishFrame()` を分離する API を追加し、最初の対象を
submitter 所有リソースだけで描ける 16 個以上の `SolidRectPkt` / `SolidRectXformPkt` /
`LinePkt` / `QuadPkt` / `SolidTriPkt` buffer に限定した。
別スレッドで record、immediate owner で execute、その後 recorder thread で FinishFrame
する headless contract test を追加した。worker lane は独立した `ShaderManager` /
`DiligentImmediateSubmitter` と deferred context を所有し、mutable な SRB を immediate lane と共有しない。
`FinishFrame()` は immediate owner が command list を実行し終えた後に worker lane で呼ぶ。
さらに同じ buffer を production `submit()` に通し、既存の
16-packet threshold による deferred selection も同じ immediate baseline と比較する。record 中は Immediate Context を参照せず、execute 完了まで
packet buffer を保持し、3 経路の readback image を比較する契約である。`ArtifactGpuFoundation` と
`ArtifactRender`、テスト target の Debug ビルドに成功し、Vulkan は CTest suite 6/6、D3D12 は個別プロセス suite 6/6
で pass した。新しい worker recording ケースも両 backend で pass し、coverage が一致し、最大 channel 差は
`1.0e-4` 以下だった。immediate reference、worker-recorded frame、production `submit()` deferred frame は backend ごとの PNG
(`build/tests/Artifact/Debug/cloner_gpu_render_captures/deferred_{worker,submit}_recorded_{d3d12,vulkan}.png` と
`deferred_worker_immediate_{d3d12,vulkan}.png`) として保存し、出力画像を目視確認した。Windows の test binary には Qt offscreen integration plugin が必要なため、Cloner test target の
隣へ plugin を配置し `QT_QPA_PLATFORM_PLUGIN_PATH` を設定した。確認対象は独立した headless renderer と
この限定 packet mix であり、Composition snapshot、scene/resource ownership、frame queue、present の worker 移行や
GUI 非ブロッキングは証明しない。production `submit()` の deferred selection は依然として同じ呼び出し thread で記録・実行する。
既存の巨大な Visual Studio / CMake module build のため、以後の再 configure は長時間を要する。

**Recorder resource lifetime の追加検証（2026-10-17）:** worker 専用 submitter の buffer / PSO を worker 起動前に準備し、
worker 内で deferred context を設定・record・`FinishFrame()` する形でも D3D12 / Vulkan の画像 contract が pass した。
`setDeferredContext(empty)` を worker 終了時に呼ぶ変形は両 backend で access violation (`0xc0000005`) になった。
この変形では worker-local `RefCntAutoPtr` が worker thread 上で context の最後の参照を解放するため、最後の参照を
worker 外へ保持した場合との違いは context の解放 thread である。現段階では worker を join し、worker submitter が保持する
deferred context の最終 release を worker 終了後に行うこと。worker 上での明示的な context reset は行わない。
再確認で得た 4 枚の immediate / worker-recorded PNG はすべて SHA-256
`AF60E824696AFA7C599F551A644CE6C0D285C108BCD947497C00F1A18416470A` で一致した。

**初期進捗（2026-10-16〜2026-10-17、後続訂正あり）:** `BillboardPkt` は `PrimitiveRenderer3D` が保持する immediate context に直描画し、
周囲の packet と順序が分かれる可能性があった。`PrimitiveRenderer3D::drawBillboardQuadWithContext()` を追加し、
この packet は submitter の `recordCtx` と packet の RTV を明示して記録するようにしたため、deferred command list 内の順序を維持する。
submitter から `PrimitiveRenderer3D` の RTV override を一時変更して `nullptr` に戻す操作も除き、active offscreen target を後続の即時 3D draw が失わないようにした。
QImage overload も packet 作成時に texture を解決して retained SRV 付き `BillboardPkt` を append する。
これにより通常経路の QImage 変換／resource 作成は記録 worker に移らず、context 記録だけを後段へ渡せる。
この初期段階では手動生成などで残る `BillboardImagePkt` が submit 中に texture を作り、含まれる buffer
全体を immediate fallback に残していた（2026-10-17 の後続修正で解消、下記参照）。さらに `LinePkt` /
`QuadPkt` / `SolidTriPkt` / `AtlasSpritePkt` の保留バッチは、種類が
切り替わる時に前のバッチを flush する。これにより末尾の固定 flush 順が interleaved packet の順序を
入れ替える経路を塞いだ。Particle は `ParticleRenderer::updateBuffer(recordCtx, data)` に変え、upload / prepare / draw を同じ
recording context に通すことで Particle packet を deferred 対象にした。Diligent の D3D12 / Vulkan 実装は
`UpdateBuffer` の CPU 入力を context の upload allocation へ呼び出し中にコピーし、その context の command stream
へ copy を記録する。Cloner placement も `ClonerGridDispatchPkt` 化した。producer は GPU parameter 値と output range を
順序どおり予約して dispatch packet を draw packet より前に append し、submitter が parameter upload と compute dispatch を
`recordCtx` に記録する。dispatch packet は PSO / SRB / parameter buffer / output buffer を保持し、後続の
`ClonerSpriteXformPkt` は対応する packet index の記録結果を参照する。Map に失敗した場合は既存 CPU fallback を使う。
そのため Cloner を含む buffer も packet 数などの通常条件を満たせば deferred recording に進める。この進捗記録時点で残る全体 fallback は、
submit 中に texture 作成と保持 immediate context への描画を行う legacy `BillboardImagePkt`（後続修正で解消、下記参照）。
3D gizmo は line と triangle を別 vector に溜め、末尾に line → triangle の固定順で描いていたため、面と輪郭線が交互に積まれると呼び出し順が崩れていた。頂点を一つの順序付き stream に保持し、同じ topology の連続区間だけを run としてまとめて、run 順に記録する形へ変更した。`DiligentImmediateSubmitter::submit()` は 2D packet の後にこの gizmo geometry を同じ `recordCtx` へ記録する。packet 数と gizmo 頂点数の合計が 16 以上で legacy `BillboardImagePkt` がなければ、既存 deferred command list に入る。これにより line / triangle の順を保ちながら、gizmo overlay の upload と draw も deferred 対象へ進めた。この進捗記録時点では小規模な仕事と `BillboardImagePkt` 含有 buffer が immediate context を使っていたが、後者は下記修正で deferred recording に移した。
Bindless の Sprite 専用経路は pending gizmo の有無を見ず、Sprite packet だけを先に実行していた。gizmo が保留中なら通常 submitter に渡し、Sprite と gizmo を同じ記録経路で順に処理するようにした。これで bindless 有効時にも gizmo overlay が後続 present へ取り残されない。
これらはソース確認までで、ビルド／実行での描画順と画像 parity は未確認。
worker-owned deferred recorder へ進めるには、ParticleRenderer の可変 GPU resource state も recorder ごとに所有させる
必要がある。加えて submitter は可変 PSO cache と batch 配列
（`DiligentImmediateSubmitter.ixx:136,156-188`）、PrimitiveRenderer3D は可変 SRB / constant buffer / 定数値 / RTV
 override（`PrimitiveRenderer3D.cppm:318,326-368`）を持つ。現在は同一スレッド内で直列記録するが、別 worker
から context pointer だけを使い回すことはできず、mutable state と recording resource の worker ownership が要る。
Bindless submitter は Sprite の形式・screen size・texture 数・batch 容量が変わると途中 flush するが、後続 packet
の失敗時に buffer 全体を fallback へ再送して先行 draw を重ねる可能性があった。fallback 有効時は一回の batch に
収まる条件を先に検査し、最終 flush の失敗は draw 完了前に fallback するようにした。制約を超える buffer は
bindless draw を始める前に通常 submitter へ渡す。この単一 batch 成功経路は 16 packet 以上なら同じ deferred
context で記録し、command list を即時 context で実行してから buffer を reset する。記録 worker はまだ導入せず、
GUI / 呼び出し thread 上で同期実行する。

**追加進捗（2026-10-17）:** Diligent の deferred context は immediate context の viewport / scissor を継承しない。
`SetRenderTargets()` 後に D3D12 backend が viewport を target 全体へ設定するため、部分 viewport を使う Composition
描画は packet 数が deferred threshold を超えた場合に領域外へ描く可能性があった。`RenderViewportState` を値として保持し、
submitter と `PrimitiveRenderer3D` が target bind 後に viewport / scissor を再適用するようにした。viewport が変わる場合は
その前の queued packet と gizmo を古い状態で submit してから次の状態へ移る。これで既存 deferred 経路の部分 viewport
境界を維持する形になったが、D3D12 / Vulkan の実機 parity は未確認である。
現行の `ArtifactIRenderer::Impl` が保持して submit に使うのは `DiligentImmediateSubmitter` であり、
`DiligentBindlessSubmitter` は Artifact の描画経路に生成・呼び出し箇所が見つからなかった。そのため bindless
submitter 側の safety 修正は現行 renderer の実行経路にはまだ反映されない。

**追加進捗（2026-10-17、legacy image billboard）:** 直前まで唯一の全体 immediate fallback だった
`BillboardImagePkt` が、PrimitiveRenderer3D の保持 immediate context へ描画していた。submitter の渡す
`recordCtx` で描画する API を追加し、submit 時に解決した texture を packet に保持して command list 実行まで寿命を
維持するようにした。これでこの packet も同じ順序付き recording context に入り、variant を含む buffer 全体を
immediate に切り替える条件を除いた。QImage texture の生成自体は呼び出し thread 上に残し、worker へは移していない。
ソース検索では現在の production caller は見つからなかったが、公開 packet variant としての順序経路は保った。

**追加監査（2026-10-17、packet order）:** `DiligentImmediateSubmitter::submit()` はカテゴリ切替前に該当する保留 batch を
flush し、同時に異なる primitive / sprite batch を複数 pending にしない。`RenderCommandBuffer::moveAppendBuffer()` は
source 内 packet 順で append し、`RenderCommandBufferPool::tryMergeInto()` は worker slot 順に結合する。並列
polyline / styled polyline / polygon は入力の連続範囲を worker index 順に割り当て、同期 `Parallel::For` の完了後に merge
する。mesh、3D card、textured card、3D shape の即時 draw 前と render target override の切替前には queued packet を
submit する経路がある。これらはソース上の順序根拠で、実機の描画順・画像 parity の証拠ではない。現在の
`ArtifactIRenderer::Impl` は `DiligentImmediateSubmitter` を使い、`DiligentBindlessSubmitter` は現行経路から到達しない。
この範囲（submitter batch / worker merge / 既知の直接 draw barrier）では追加逆転を確認しなかったが、後続監査で
immediate clear / compute / copy 境界の抜けを確認した（次項）。deferred command list の実行・present は引き続き
呼び出し thread 上で同期され、worker-owned recording と D3D12 / Vulkan runtime parity は未確認である。

**追加監査（2026-10-17、容量不足時の順序）:** MPM surface packet は owner queue が満杯なら、先行 packet を
`submitQueuedDraws()` で送ってから retry する。並列 polyline / styled polyline / polygon は worker lease または merge が
失敗した場合、worker buffer を reset して owner queue へ通常 packet を順に記録するため、失敗した部分 buffer が後から
混ざらない。GPU Cloner placement packet の追加失敗は当該 placement だけ通常 sprite packet にフォールバックする。
確認範囲では capacity failure による即時描画や先行 packet の追い越しは見つからなかった。source 上の制御フロー確認であり、
queue 容量上限時の runtime 表示確認は未実施。

**追加修正（2026-10-17、reusable video texture 上書き順序）:** `textureForVideoFrame()` は layer ごとの cache slot を再利用し、
異なる frame の `UpdateTexture()` を即時 context に出す。一方、同じ SRV を使う packet は owner queue に未 submit のまま残り得るため、
次 frame の upload が前 frame の draw より先に実行される経路があった。`PrimitiveRenderer2D` は Sprite / Cloner Sprite / Atlas Sprite /
textured triangle / local sprite / masked sprite の各 packet 追加時に、現在解決済みの video slot と一致すれば queue epoch を記録し、
同じ slot の上書き前にだけ owner queue を submit する。packet 全件の走査や追加 resource 確保は行わない。
Composition 側は texture を Cloner enumeration / dispatch より前に一度だけ解決し、overwrite barrier が現在の dispatch と sprite packet を
分離しないようにした。後続の runtime test で async Deferred handoff 時に epoch を進めると in-flight command list の参照先を上書きする競合を再現したため、epoch 更新を command list 実行完了時へ移し、overwrite callback が worker-only pending batch も drain するよう修正した。red video frame を同一 slot に blue frame で上書きする test は Debug / D3D12 と Debug / Vulkan で pass し、Deferred frame は同期 red baseline と全画素一致、次の frame は blue update を表示した。`artifact_renderer_deferred_video_reuse_first_{d3d12,vulkan}.png` は SHA-256 `2C79384A22C014CC16AA2BE55C417108D63A879FE23DD4940FC370CB88B5ABE8`、`artifact_renderer_deferred_video_reuse_second_{d3d12,vulkan}.png` は `0C5D42EEBCA087A4D52A6318E551534CF4648170F1C07935A819B4C253FE1F94` で各 backend 間一致し、画像を目視確認した。

**追加修正（2026-10-17、GlyphAtlas full upload 順序）:** `GlyphAtlas` が容量上限に達すると atlas を clear して glyph を再配置し、
`PrimitiveRenderer2D::uploadGlyphAtlasIfNeeded()` が同じ GPU texture 全体を即時更新する。旧 UV を持つ Atlas Sprite packet が queue に
残っている場合に限り、texture 上書き前に owner queue を submit する callback を `ArtifactIRenderer` と独立 Timeline renderer の両方へ接続した。
通常の新規 glyph は空き shelf への部分 upload のままであり、queue drain や texture 再生成を行わない。全消去時の順序境界は source で確認したが、
atlas 飽和を含む runtime 表示は未確認である。

**追加修正（2026-10-17、Map failure）:** `DiligentImmediateSubmitter` の packet draw は `mapWriteDiscard()` が失敗しても
draw を発行する経路があり、直前 packet の内容が dynamic buffer に残っていると、その stale geometry を現在 packet の位置で
描画する可能性があった。Diligent の共通 `MapBuffer()` は backend 確保前に map 状態を登録するため、失敗時も `UnmapBuffer()` を呼んで
状態を戻す。batch と個別 packet の vertex / constant buffer Map 成功を draw 前に確認し、失敗した batch は
保留 CPU data を破棄、個別 packet はその draw をスキップする。これで古い buffer 内容を再生する経路を防ぐ。複数 glyph の
packet は途中で Map が失敗すると既に記録された glyph draw は残り、その packet の残りを中断する。failure injection と runtime
表示確認は未実施である。

**追加修正（2026-10-17、immediate operation の順序境界）:** `clearRenderTarget()` / `clearDepthRenderTarget()`、
`drawEnvironmentSkybox()`、GPU `blendLayers()` / `clearTextureRegion()` / `convertLayerToFloat()` / `applyTrackMatte()` /
`copyTextureOutsideRegion()` / viewport exposure / MSAA resolve、compute 用 color target unbind は、即時 context へ操作を出す前に
queued packet を submit するようにした。先行 packet が同じ resource を更新する場合に、clear / draw / compute / copy が先に実行される
順序逆転を防ぐ。swapchain resize も旧 backbuffer を参照する packet を復帰 bind なしで drain してから解除・再生成する。
これらは現行 renderer の shared immediate context と既存 deferred recording の境界を守る変更で、worker-owned recorder への移行ではない。
確認はソース上の呼び出し順までで、実機の clear/blend/readback/resize parity は未確認である。

**追加修正（2026-10-17、shadow map と queued packet の順序）:** Composition controller は layer packet を積んだ状態で `renderShadowMapFrame()` を呼び、その後の `flush()` で packet を materialize していた。shadow pass は immediate context の target を shadow map に切り替えて clear / draw するため、先行 packet より先に GPU command が発行される。`renderShadowMapFrame()` の shadow target bind 前に queued packet と gizmo を submit し、呼び出し順を保つようにした。Composition controller からの呼び出し順と renderer 内の command 順を source で確認した。runtime の画像影響と D3D12 / Vulkan parity は未確認である。

**追加修正（2026-10-17、async readback order）:** `readbackTextureViewToImageAsync()` は active render target を含む任意の texture view を source にできるが、staging への `CopyTexture()` 前に queued packet を submit していなかった。同期 readback には同じ境界があり、同 frame の active target を読み戻す場合に copy が packet draw を追い越す可能性があるため、copy 直前に `submitQueuedDraws(ctx)` を追加した。fence wait と CPU map は引き続き非同期のまま。確認はソース上の API 契約と呼び出し順までで、runtime readback parity は未確認である。

**追加修正（2026-10-17、async readback context ownership）:** fence 後の worker が captured immediate context を map/unmap していたため、非同期 readback 専用の `QThreadPool` を最大1 thread・無期限維持にし、その thread だけが使う deferred context を初回 readback 時に作るようにした。Diligent `IDeviceContext` API は graphics context での staging map をサポートし、D3D12 / Vulkan の staging map 実装は resource の host mapping / cache invalidation を行い、deferred command recording を必要としない。device が再初期化された場合は worker 上で古い context を置き換える。pool は renderer の最終 data member とし、destruction 時に worker を join してから他の renderer state と deferred context を破棄する。Qt `QThreadPool` の destructor は実行中の runnable 完了まで block し、Diligent `IFence::Wait()` は timeout 引数を持たないため、renderer teardown は readback fence の完了を待つ可能性がある。通常完了時の待ち時間と device loss / hung GPU 時の挙動は runtime 未確認である。

**追加監査（2026-10-17、Composition 側の raw immediate 呼び出し）:** 最終表示の BGRA accumulator 変換と channel display は、renderer packet を経由せず immediate context へ直接 dispatch する。ただし先行する accumulator 更新も immediate context 上で完了し、変換先の scratch texture は先行する viewport background packet の render target と別 resource である。後続の presentation sprite は変換後の SRV を読む。Composition-space cache の色変換も build path では cache 描画の `renderer_->flush()` 後に行い、cache-hit は既存 cache を読む。今回確認した呼び出し順では追加の queue drain を必要とする resource 依存は見つからなかった。raw `immediateContext()` 呼び出しを一律 drain するのではなく、packet が書く resource と即時操作が読む／書く resource の依存で境界を決める。runtime の resource transition / frame ordering は未確認である。

---

## 1. ゴール / 非ゴール

### ゴール
- ビューポートのコンポジション描画（`renderOneFrameImpl`）を GUI スレッドから
  専用スレッドへ移し、GUI（Inspector / Timeline / 入力）が描画でブロックされないようにする。
- swapchain / QWidget 依存を明示的な境界として切り分ける。
- シーン状態を snapshot として受け渡し、スレッド間のデータ競合を排除する。

### 非ゴール（本ドキュメント）
- render worker への全面移行。ソフトレンダラの変更。動画デコード経路の変更。
- 最終レンダリング（RenderFarm / export）経路の並列化。

---

## 2. 現状の実行モデル（根拠付き）

| 要素 | 現状 | 根拠 |
|------|------|------|
| tick スレッド | `PreciseTicker` が worker thread を持つ | `ArtifactCore/src/Thread/PreciseTicker.cppm:21,113` |
| tick → GUI | callback が `QMetaObject::invokeMethod(this, …, Qt::QueuedConnection)` で GUI へ marshal | `ArtifactCompositionRenderController.cppm:19486,19518,19737` |
| フレーム本体 | `impl_->renderOneFrameImpl(this)` が GUI スレッドで全実行 | 呼び出し `:19723` / 定義 `:42329`–`:49391`（約 7,000 行） |
| present | `renderer_->present()` をフレーム内で直接呼ぶ | `:42694`, `:14844` |
| controller 型 | `CompositionRenderController` は `QObject`、host は `QPointer<QWidget>` | `:18785`, `:16772` |
| swapchain | `QWidget*` から `DiligentDeviceManager` が生成/再生成 | `DiligentDeviceManager.ixx:144-150`; 実装 `DiligentDeviceManager.cppm:1612,1651`; `ArtifactIRenderer.ixx:189-190,228` |
| 再入/多重防止 | `renderTickPosted_` / `renderDirty_` / `renderInProgress_` | `:19508`, `:19598`, `:19621` |
| 非同期 readback | GPU copy + fence wait は worker 実行（既存） | `:48591`（コメントに「render thread」表記あり） |

要点: **tick は既に別スレッドだが、フレーム全体は GUI スレッドで実行されている。**

---

## 3. なぜ「present 単独の非同期化」では不十分か

- `present()` は `renderer_->present()`（`:14844`, `:42694`）に過ぎず、GUI をブロックしているのは
  present 前の **コマンド記録と各パスの実行**（`renderOneFrameImpl` 全体）。
- Diligent の契約上、**immediate context は単一スレッド専用**
  （`DiligentDeviceManager.ixx:48-49`）。
- deferred context は別スレッドでコマンドを記録し、`FinishCommandList()` で得た command list を
  immediate context から実行するためのもの。これにより記録処理は段階的に分離できるが、即時実行と
  present が GUI 上に残ればブロックも一部残る。
- 「描画コマンド記録の分散」と「immediate context / present の所有スレッド移動」は別の移行軸として
  計画する。前者は小さく始められ、後者が GUI ブロックを解消する最終境界になる。

---

## 4. 既存の土台（追い風）

- **スレッド契約の明文化**: `// Contract: one immediate context is driven from exactly one thread;
  extra threads must use device->CreateDeferredContext().`
  （`Artifact/include/Render/DiligentDeviceManager.ixx:48-49`）。
- **deferred context は既に限定利用中**: `DiligentDeviceManager` は deferred context を生成して
  `deferredContext()` を公開する（`DiligentDeviceManager.cppm:1426,1511,1581,1604,1952`）。
  `ArtifactIRenderer.cppm:2852,2947,4027` から `DiligentImmediateSubmitter` へ渡され、
  packet 数など通常条件を満たす command buffer は deferred context で記録後、同じ呼び出し内で immediate
  context へ実行される（`DiligentImmediateSubmitter.cppm:1008-1470`）。全 packet type は同じ記録 context を使う。
  Cloner placement compute とその sprite draw も一つの順序付き packet stream に載せられるようにした。
  これは既存の段階導入例だが、呼び出し元スレッドで同期実行するため GUI 非ブロック化ではない。
- **共有 device**: `acquireSharedRenderDeviceForCurrentBackend(...)` / `SharedRenderDeviceLease`
  （`DiligentDeviceManager.ixx:19-39`）。
- **device 機能フラグ**: `MultithreadedResourceCreation = DEVICE_FEATURE_STATE_DISABLED`
  （`:745,963,1104`）、`NumAsyncShaderCompilationThreads = 2`（`:201`）。
- **worker の置き場**: `ArtifactCore::RenderWorker` は存在するが **空スタブ**
  （`ArtifactCore/src/Render/RenderWorker.cppm:39-42` `run(){}`）。
- **async readback API**: `readbackToImageAsync` / `readbackTextureViewToBufferAsync`
  （`Artifact/include/Render/ArtifactIRenderer.ixx:270-280`）。

---

## 5. 主要ブロッカー / 依存

1. **共有 immediate context の利用者が多い**
   `acquireSharedRenderDeviceForCurrentBackend` は複数の GPU Effect
   （`Artifact/src/Effects/**`, `Artifact/src/Effect/ArtifactCreativeEffects.cppm`）と
   `ArtifactDiligentEngineRenderWindow` から呼ばれ、同じ shared immediate context を返す。
   一方 `ArtifactDiligentTimelineRenderWindow` は `createIndependentRenderDevice()` を使うため、
   この shared context の利用者として数えない。`ArtifactRenderQueueService` など renderer 経由の
   利用者は呼び出し経路ごとに実 context とスレッドを追跡する必要がある。
   → コンポジションだけを別スレッドに移す場合、shared immediate context を使う既存経路との所有権・
   実行順を明確にしなければ競合しうる。**最大の制約。**
2. **QWidget 依存**: host 可視判定 `hostWidget_->isVisible()`（`:42381`）、
   `owner->recreateSwapChain(host)`（`:42548`）、`recreateSwapChain(QWidget*)`（`:20080`）。
3. **シーン直読み**: `previewPipeline_.composition()`、layer / property、`ArtifactPlaybackService`、
   `ArtifactEffectFrameSampler` をフレーム内で都度参照（`:42404`, `:19647`, `:19673`）。
4. **遅延リソース生成**: `gpuTextureCacheManager_`、各 pass / pipeline、shader cache がフレーム内で
   遅延生成される。生成スレッドを統一しないと affinity が崩れる。
5. **RAM preview / readback が playback service と密結合**（`:48570-48630`）。
6. **multi-viewport**: `CompositionRenderController` は複数インスタンス化されうる
   （multi-viewport マイルストーン）。単一 render thread 前提を置かない。

---

## 6. 提案スレッド所有権モデル

- **device**: 共有のまま維持（`acquireSharedRenderDeviceForCurrentBackend`）。
- **immediate context + swapchain**: 単一の **render owner thread** が所有し、そこで
  `renderOneFrameImpl` と `present()` を実行する。
- **GUI thread**: GPU 資源・swapchain に一切触れない。swapchain の create / recreate と
  resize 要求は「要求だけ」を GUI → render owner へ渡す。
- **他サブシステム**: shared immediate context を使う処理は最初は render owner thread 上で直列化する。
  deferred context へ移すのは、対象パスが worker 側で安全に記録でき、必要な resource を command list
  実行まで保持できると確認できたものに限る。Effect の直接呼び出しには `Map/Update/Dispatch` に加えて
  flush / wait / GPU readback を含むものがあり、context の差し替えだけでは移行できない。
- **deferred recorder**: context と mutable な submitter は worker ごとに専有させる。完成した command list
  を immediate owner に順序付きで渡し、実行まで参照 resource の寿命を保つ。bounded queue と frame
  generation により古い結果を破棄できるようにする。
- **scene**: GUI が snapshot を publish し、render owner が consume する（所有権は一方向）。

### スレッド境界（要求 API のイメージ）
- GUI → render: `requestResize(w,h)`, `requestSwapChainRecreate(hwnd)`, `requestFrame(snapshot)`, `requestStop()`。
- render → GUI: 完了通知は既存の event bus / queued 通知を再利用（**新規 signal/slot は禁止**）。

---

## 7. 段階的実装案

| Step | 内容 | リスク | 前提 |
|------|------|--------|------|
| S0 | Phase 0 の計測を流用し、GUI CPU prep / context record / execute / present を分けて測る。shared-context 利用者と各 pass の direct call を分類 | 低 | Phase 0 完了 |
| S1 | M-IR-8 の renderer façade に沿って controller の direct context access を個別に減らす。thread ownership は変えない | 低〜中 | S0 |
| S2 | 既存 `DiligentImmediateSubmitter` deferred 経路の packet eligibility・描画順・resource lifetime を監査し、安全な条件を固定 | 中 | S1 |
| S3 | GUI で immutable な frame/command snapshot を作り、単一の packet-only pass を worker-owned deferred context で記録。bounded queue から immediate owner へ順序付きで渡す | 高 | S2 |
| S4 | Effect / compute は候補を個別選定し、direct context・同期 readback を renderer 経由の記録可能な API に変えられるものだけ移行 | 高 | S0〜S3 の測定で効果が確認できること |
| S5 | immediate context と present を render owner thread へ移す。GUI scene snapshot と HWND / swapchain lifecycle の境界を分離 | 最高 | S0〜S3 で所有権と受け渡し契約が確立 |

各 Step は段階ごとの rollback 経路を維持し、Phase 0 計測で GUI block time と frame time を比較する。
`MultithreadedResourceCreation` は deferred command recording と別の機能なので、必要性を計測・契約確認
せずに有効化しない。`ArtifactCore::RenderWorker` は現在空スタブだが、実装時に子リポジトリ変更が必要なら
その作業範囲を別途明示する。

---

## 8. scene snapshot 契約（提案 / 未確定）

render thread がフレームに必要とする最小状態:
- `compositionId`, frame position, temporal 情報（`:19673`）
- viewport size（`hostWidth_`/`hostHeight_`）、zoom / pan（`:42400-42416`）
- ROI / resolution scale / `RenderMode`（`:42421-42435`）
- 描画対象 layer / effect / property の値スナップショット
- playback 状態（playing / current frame / speed / loop）
- RAM preview 要求とその generation

契約方針: snapshot は GUI 側で生成し、**読み取り専用**で render thread へ move する。
render thread から scene を直接書き換えない。

---

## 9. リスク / トレードオフ

- Diligent/DX12 は severe 領域（AGENTS）。広く触らず、変更範囲を最小化する。
- `docs/technical/HOT_PATH_RULES.md` に従い、ホットパスで大容量確保・readback・暗黙変換を増やさない。
- GUI フォールバックを常に保持し、段階ごとにロールバック可能にする。
- 入力の連続性（viewport interaction / gizmo / timeline scrub）と render 順序の整合を要検証。
- device lost / resize 競合時の再生成を render thread 所有下で正しく直列化する必要がある。

---

## 10. 検証方法

- Phase 0 の計測（effect param だけでなくフレーム全体の GUI ブロック時間）。
- 既存 profiling フラグの流用（例: `ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP` 系、`interactionPerf`）。
- pre/post で「GUI ブロック最大/平均 ms」「present 回数/ドロップ」「frame time」を比較。
- build / GPU validation is recorded below. The narrow record/execute/finish seam can now be exercised from a worker thread; moving Composition frame ownership, queueing, and present off the GUI thread remains a separate implementation phase.
- `ArtifactClonerGpuRenderContractTest` に 16 packet 以上の headless 描画ケースを追加し、Triangle → Quad の順が
  Deferred submit 後の重なりピクセルに保たれることを確認する。テストは後描画 Quad の色を検査する。
  Cloner compute + sprite 経路も、12 個の不可視 packet・dispatch・3 sprite packet で同じ閾値を越えて確認する。
  `DeferredCommandListCanBeRecordedAndFinishedOnWorker` は worker thread で record と `FinishFrame()` を行い、
  12 rectangle + line + quad + triangle + overlapping rectangle の即時経路との readback image を比較して描画順も検査する。
  Debug / Vulkan は CTest suite 6/6、Debug / D3D12 は個別プロセス suite 6/6 pass。
  Windows CTest target の隣へ Qt offscreen plugin を配置し plugin path を指定する。
- `ClonerGpuRenderContractTest.DeferredSubmitPreservesPartialViewportAndScissor` は、部分 viewport 内の solid rect を即時経路と 15 個の不可視 packet + 1 draw の Deferred 経路で描画し、128×128 全画面の alpha coverage mask と viewport 外 scissor を比較する。Debug / D3D12 と Debug / Vulkan の双方で個別プロセス pass。
- `ClonerGpuRenderContractTest.DeferredSubmitDrainsPacketsBeforeViewportSwitch` は、異なる部分 viewport の間で queue を切り替え、即時経路と Deferred 経路の全画面 alpha coverage を比較する。両領域の色と領域外 scissor も確認し、Debug / D3D12 と Debug / Vulkan で個別プロセス pass。
- 2026-10-17: 対象 test target の依存ビルドで、既存 ArtifactCore 作業ツリーにあった bool 戻り値の型不一致と
  Ninja 向け IFC 参照漏れを局所修正し、Artifact 側では const submit 経路から command-buffer submit 通知を
  更新できるよう renderer subobject を mutable にした。Vulkan の Cloner compute shader が拒否されたため、
  C++ / HLSL parameter block を 16-byte 境界に揃え、Cloner sprite の local origin を geometry scale から分離した。
  また Cloner test は clear を dispatch より前に置き、dispatch + dependent sprites が同一 command list に入る
  16-packet ケースへ拡張した。`ArtifactClonerGpuRenderContractTest` の 3 ケースは Debug / Vulkan と
  Debug / D3D12 の両方で pass。GPU placement の画像契約と Deferred packet 順序をこの headless test 範囲で確認した。
  さらに Cloner placement range に batch generation を持たせ、submit または renderer / placement GPU resource の
  再初期化後の古い range は GPU draw 経路で拒否して caller の CPU fallback へ戻す。dispatch submit と sprite draw を分ける回帰ケースも追加し、Vulkan / D3D12 で
  3 sprite の CPU fallback 画像を確認した。3 ケースは backend ごとに個別プロセスで実行し、Vulkan 物理 device と
  D3D12 adapter のログも確認して両 backend で pass。同一プロセスで複数 headless Vulkan device を作ると Diligent
  global function pointer 制約エラーが出るため、このテスト群は個別プロセス実行を検証条件とする。
- 2026-10-17: `Particle3DRenderContractTest` に、不可視の solid-rect packet 15 個と Particle packet 1 個で Deferred 閾値を越え、即時経路と画素数・重心・coverage mask を比較するケースを追加した。実行時に `readbackTextureViewToFloatBuffer()` が RGBA texture の raw 順序を返す一方、`readbackToImageF32()` が BGRA 入力として扱っていたため赤粒子が青として解釈され、既存 Particle image contract も全て画素数 0 となる不整合を確認した。readback 境界を `setFromRGBA32F()` に修正した。Windows の実行 target には Qt offscreen integration plugin が配置されず `QGuiApplication` が初期化できなかったため、CTest target の隣へ plugin を copy し `QT_QPA_PLATFORM_PLUGIN_PATH` を設定するようにした。Debug の Particle contract suite 全体が CTest で pass、新しい Deferred ケースは D3D12 と Vulkan の個別プロセスで pass。depthTest / depthWrite がともに無効なら Particle packet に DSV を保持しないようにし、Particle PSO の DSVFormat と attachment を一致させた。CTest の suite と 7 ケース全体を Debug / D3D12、Debug / Vulkan で実行し、両 backend で pass。depth format mismatch warning も再実行で解消した。これは同期 Deferred submit の確認であり、worker thread 化の証明ではない。

---

**追加検証（2026-10-17、Ninja/MSVC app build と worker image contract）:** ArtifactCore の分割実装は
CXX_SCAN_FOR_MODULES=OFF の source が多く、MSVC/Ninja では CMake scanner が補う IFC reference を
手動 table で渡す必要がある。split target の provider ownership と source table が一致しない場合、C2230
（IFC 不足）または C7684（同名 IFC の重複解決）が起点となり、多数の後続型エラーに見える。今回の
SceneNode.cppm は aggregate / split の二重参照、FFmpegThumbnailExtractor.cppm は scanner と手動表の二重参照、
VectorExport.cppm は分割先と再公開 import の不足だった。provider / consumer target directory を分離し、scanner
有効 source は手動 import table から除き、SceneNode と VectorExport の所有先・import 閉包を合わせた。
その後 Artifact target は Debug / Ninja/MSVC で完了し、ArtifactCompositionRenderController.cppm を含む
ArtifactAppRuntime もコンパイルした。テスト target 再ビルドでは、Windows SDK の rc.exe が PATH にないため
ArtifactEffectContract.dll の resource compile が失敗したが、SDK x64 bin を PATH に加えると通過した。
ArtifactClonerGpuRenderContractTest を最新ソースでビルドし、
ClonerGpuRenderContractTest.DeferredCommandListCanBeRecordedAndFinishedOnWorker を D3D12 と Vulkan の別プロセスで
実行して両方 pass。各 backend で immediate reference / worker-recorded / production deferred submit の6 PNG を
out/build/x64-Debug-Agent/tests/Artifact/cloner_gpu_render_captures/ に出力し、worker image を目視比較した。
Composition frame の worker 化、resource snapshot、frame queue、present の分離は未実施であり、この contract は
限定 packet set だけを証明する。

**追加検証（2026-10-17、ArtifactIRenderer 公開 API の画像契約）:** `ArtifactIRenderer` の公開描画 API から
2 viewport を描き、viewport 切替前の submit barrier と 16 packet 以上の deferred 選択を通す headless ケースを拡張した。
即時経路と deferred 経路を全画素・全 RGBA channel で比較し、Debug / D3D12 と Debug / Vulkan で個別プロセス pass。
両経路の実描画 PNG を `out/build/x64-Debug-Agent/tests/Artifact/cloner_gpu_render_captures/` に保存し、backend ごとに
immediate / deferred の SHA-256 が一致することを確認した。これは Artifact アプリが使用する renderer の同期 deferred
経路を検証するが、worker-owned recorder や Composition frame の非同期化を証明するものではない。

**追加修正・検証（2026-10-17、worker submitter の teardown 所有権）:** production の
`DeferredPacketRecordWorker` を組み込んだ後、worker test は描画 assertion を通過した後の
`ArtifactIRenderer::destroy()` で SEH access violation になった。VS debugger の triage で
`DiligentImmediateSubmitter::destroy()` → `PSOAndSRB::operator=(PSOAndSRB&&)` を特定した。
glyph pipeline clone helper が借用 raw pointer を所有型 `PSOAndSRB` の一時値へ格納し、
その destructor が `ShaderManager` 所有の参照を余分に `Release()` していた。raw pointer を借用する
clone overload に置き換え、worker shutdown 後に保持 device reference も解放するよう修正した。
`DeferredSubmitDrainsPacketsBeforeViewportSwitch` と
`DeferredCommandListCanBeRecordedAndFinishedOnWorker` は Debug / D3D12 と Debug / Vulkan で pass。
production renderer の immediate / worker-deferred PNG と worker contract の immediate / worker / production-submit
PNG をそれぞれ出力し、目視で描画内容を確認した。production renderer の 128×128 immediate / deferred PNG は
`artifact_renderer_{immediate,deferred}_{d3d12,vulkan}.png` として保存され、4 枚すべての SHA-256 は
`2A1BAB4EAC02427FDAEB1557BB920D55DA26B14B12D2D6549994043C132C1431` で一致した。worker contract の
`deferred_worker_immediate_*` / `deferred_worker_recorded_*` / `deferred_submit_recorded_*` も 6 枚すべての
SHA-256 が `AF60E824696AFA7C599F551A644CE6C0D285C108BCD947497C00F1A18416470A` で一致した。これは限定 packet set の recording + execute + teardown を検証するもので、
Composition frame の worker 化、scene snapshot、GUI 非ブロッキングはまだ未検証である。

**追加実装（2026-10-17、Composition host preflight の GUI 境界化）:**
`CompositionRenderController::Impl::HostFrameSnapshot` を追加し、host の可視性・論理サイズ・DPR と診断用 HWND を
GUI 側で値として取得するようにした。line debug overlay の4種（DebugProbe / MaskPath / MaskHandle / Bounds）の可視性も
GUI 側で値にし、frame body が `owner` の visibility API を呼ばないようにした。可視性判定、widget サイズと swapchain の同期、resize / swapchain 再生成は
`prepareHostFrame()` にまとめ、`renderOneFrameImpl()` は snapshot を `const&` で受け取る。これにより frame 本体内の
`QWidget` 直接参照を取り除いた。Artifact Debug アプリは現行 source で再コンパイル・リンクした。CMake は MSVC
14.51 を検出した一方で v143 を期待する警告を継続して出し、windeployqt は Qt 翻訳カタログと
`Qt6Widgetsd.dll` の解決に失敗したが、CMake の build target は成功した。これは worker 移行ではない。
`renderOneFrameImpl()` は依然 GUI thread 上で、composition / layer / playback を直接読み、owner の UI API を呼ぶ。

**追加修正・検証（2026-10-17、resize 同期フレーム）:** host preflight で widget サイズの変化を検出した際、
viewport と swapchain を同期的に更新した後、常に frame をスキップしていた。D3D12 では最初の ticker frame 後に
host サイズが確定し、直後の同期 `renderOneFrame()` がこの経路へ入るため baseline serial が進まず、Composition
integration test が失敗した。再生成後に swapchain が有効なら同じ呼び出しで frame 本体へ進むよう修正した。
ArtifactModel3DLightCompositionIntegrationTest target の再ビルドが成功し、PointLight Composition integration test は
Debug / D3D12 と Debug / Vulkan の両方で pass。両 backend が実際の `composition_controller_point_light.png` を出力し、
SHA-256 は `ABCBCE0B0225AA7BF5BFE985BF3157165416BBB342B9AAFC4F1A8B8FE383C3DB` で一致した。
確認画像は `out/build/x64-Debug-Agent/model3d_light_capture/d3d12/Debug/` と
`out/build/x64-Debug-Agent/model3d_light_capture/Debug/` にある。これはサイズ変更を含む Composition の同期描画経路の
検証であり、Composition frame の worker 化はまだ証明しない。

**追加検証（2026-10-17、Composition frame image の readback 待機）:**
`Model3DLightCompositionIntegrationTest` は frame serial の進行直後に
`readbackPresentedFrameImage()` を一度だけ呼んでいた。frame completion と GPU pipeline の readback source 準備は同時とは限らず、
GPU pipeline 初期化中の D3D12 point-light case で source がまだ null のためテスト画像を得られないことがあった。
テスト側で Qt screen capture を使わず、renderer の GPU readback が非 null になるまで最大 5 秒イベントを処理して待つようにした。
Debug / D3D12 と Debug / Vulkan の point-light Composition integration test が pass し、両方で実際の
`composition_controller_point_light.png` を出力した。画像は SHA-256
`ABCBCE0B0225AA7BF5BFE985BF3157165416BBB342B9AAFC4F1A8B8FE383C3DB` で一致し、描画結果を目視確認した。
これは Composition controller の GPU image 出力確認であり、deferred worker recording / GUI 非ブロッキングを証明しない。

**再検証（2026-10-17、worker 変更後の Composition point-light image）:** 前回の追加 worker 実装後の実行は CMake 登録値と異なる
`QT_QPA_PLATFORM=offscreen` を使っていたため swapchain が作成されず、ticker の frame serial が進まない結果になっていた。
さらに実行ファイル隣の `platforms` に offscreen plugin がない状態で起動した試行は、Qt debug runtime の platform plugin 初期化ダイアログで停止した。
Windows Composition test の登録契約に合わせ `QT_QPA_PLATFORM=windows` と実行 target 隣の `qwindowsd.dll` を使い、最新 test object をリンクした実行ファイルで
Debug / Vulkan と Debug / D3D12 を個別に再実行したところ、point-light integration test は双方 pass した。
両 backend は実際の 400×300 `composition_controller_point_light.png` を出力し、SHA-256 は
`ABCBCE0B0225AA7BF5BFE985BF3157165416BBB342B9AAFC4F1A8B8FE383C3DB` で一致した。
これにより今回のComposition画像 parity は確認できたが、frame body は引き続きGUI thread上で同期実行される。

**既存 snapshot 経路の再利用調査（2026-10-17）:** `ArtifactRenderQueueService::cloneCompositionSnapshot()` は
`ArtifactAbstractComposition::toJson()` / `fromJson()` で composition を複製し、component simulation bake を複写する。
`ArtifactAbstractComposition::changed()` には worker で作る復元 snapshot から GUI 向け global event を同期発行しない契約もある。
これは worker 側 composition clone の既存例だが、interactive frame ごとの全 JSON serialize / deserialize や非同期 video open を
そのまま採用できる証拠ではない。候補は GUI 側で revision 単位に snapshot payload を作り、worker 側で所有 clone を更新する構成だが、
再利用可否と費用は別途計測が必要である。

**追加検証（2026-10-17、pending gizmo overlay の worker fallback）:** production `ArtifactIRenderer` は 2D packet 専用の worker recorder を持つため、
16 packet 以上の条件を満たしても 3D gizmo geometry が同時に pending している batch は main submitter に残す eligibility gate を追加した。
回帰テストは不可視 15 rect + 可視 1 rect + 緑の gizmo line を積み、worker submit count が増えないこと、rect と gizmo が最終画像に含まれることを検証する。
Debug / D3D12 と Debug / Vulkan の両方で pass し、実際の 128×128 PNG を `artifact_renderer_pending_gizmo_{d3d12,vulkan}.png` として出力した。
両画像の SHA-256 は `16D865E960A5C5FDD3E096D9FA71AB5F16D141E7A3EB26D2A18E1A2FFB12D3B5` で一致し、緑線の画素範囲は両 backend とも x=16..111 / y=63 だった。
これは worker recorder が pending gizmo を消費しない同期 fallback の確認であり、Composition frame の非同期化や GUI 非ブロッキングを証明しない。

**再実行（2026-10-17、deferred contract matrix）:** `ArtifactClonerGpuRenderContractTest` の `*Deferred*` 6 ケースを
Debug / D3D12 と Debug / Vulkan で再実行し、両方 6/6 pass。interleaved quad/triangle 順序、partial viewport/scissor、viewport 切替時の drain、
worker record/execute/finish、pending gizmo overlay fallback、production `ArtifactIRenderer` のSprite worker selectionを含む。これは headless renderer の deferred packet path が両 backend で動作する証拠で、
Composition frame の呼び出し元を非同期化する証拠ではない。

**Sprite / Atlas packetのworker記録（2026-10-17）:** `SpritePkt` / `SpriteXformPkt` に加えて
`AtlasSpritePkt` / `AtlasSpriteXformPkt` を worker recorder の eligibility に加え、1×1 immutable texture を保持する4 packetを worker image contract に含めた。
packet append 時に SRV を retain し、submitterごとのpipeline/SRBと事前確保済みsprite vertex bufferを使ってrecordする。通常・変換・atlas・変換atlas spriteの可視領域と
即時描画との全画素一致を検証し、Debug / D3D12 と Debug / Vulkan で `*Deferred*` 6/6 pass。immediate / worker recorded の4枚
(`deferred_worker_{immediate,recorded}_{d3d12,vulkan}.png`) は SHA-256 `0C6D37072EC6034B8AF36F1A748117F74934775A166CBEE4590571B641CA8C28` で一致した。
対象は独立 headless renderer の bounded packet mix であり、Composition がsprite packetを非同期に処理することや GUI 非ブロッキングを示すものではない。

**production renderer経路（2026-10-17）:** `ArtifactRendererWorkerDeferredRecordsClonerAndSpritePackets` は実際の `ArtifactIRenderer` に不可視12 packet、通常Sprite / 変換Sprite、
Cloner grid dispatch 1 packetと配置sprite 3 packetを渡し、`flushAndWait()` 後に `deferredWorkerSubmitCount()` が1増えること、textureの元参照を解放した後も
全spriteが最終画像に残ることを確認する。packet数は18で、dispatchとdependent drawsをworkerが同一command listに記録する。
Debug / D3D12 と Debug / Vulkan で pass し、`artifact_renderer_deferred_cloner_sprites_{d3d12,vulkan}.png` は SHA-256
`D5BEFAA7AF859FAAEAF4918CEF1245C846A945BDEA268861F9B52E10412691B5` で一致した。これはproduction `ArtifactIRenderer` がCloner/Sprite packet bufferをworkerへ選ぶ証拠だが、
`flushAndWait()` は同期呼び出しであり、Composition GUI thread の非ブロッキングや present 所有権の移行を示すものではない。

**production Particle packet のworker記録（2026-10-17）:** 上記production image contractにAlpha / ScreenAligned設定の可視Particle packetを加え、同一batch内のCloner dispatch / Spriteの後ろに記録した。`DiligentImmediateSubmitter::canRecordDeferred()` はParticle packetをAdditiveまたはAlphaの既定設定に限定して受け入れ、それ以外のParticle設定・別packetは同期immediate fallbackへ送る。worker専用のParticleRendererを追加するとGPU bufferとPSOを重複所有し初期化時の同期pipeline構築も必要になるため採用せず、renderer所有の既存ParticleRendererをsubmit handoff中だけworker submitterへ渡す。現状のworker `submit()` は記録・immediate execute・`FinishFrame()` 完了まで呼び出し元を同期ブロックし、その間に同じParticleRendererへ並行アクセスする呼び出しはない。この直列handoff条件のもとでParticleのbuffer update / prepare / drawもworkerのdeferred contextへ記録される。Debug / Vulkan と Debug / D3D12 でproduction worker testがpassし、両backendの実画像 `artifact_renderer_deferred_cloner_sprites_particle_{vulkan,d3d12}.png` は SHA-256 `9424E3615BB64CBCAB5655E1058DE2BEF0CDB77433CDBF6E64F7B562A18CA018` で一致した。これは限定2設定のParticle packetに対する同期worker record / execute画像契約である。submitを呼び出し元非ブロッキングにする前にParticleRendererのmutable buffer / PSO stateをworker専用化するか、immutable per-frame resource leaseへ移す必要がある。

**再分割余地（確認値と評価）:** ArtifactCore には既に 51 個の domain static library がある一方、aggregate
ArtifactCore target には今回の単独 build log 上で 396 action が残り、Artifact app build は依存込みで数千の
module scan / compile action を持つ。ArtifactCompositionRenderController.cppm は約54,000行で、抽出候補として大きい。ただし
同ファイルの Impl 状態と frame-local resource が結合しているため、機械的な source/target 分割は安全な単位を
示さない。推論としては、Core では独立した provider ownership を持つ低結合 domain から split target 候補を測り、
Composition では CPU snapshot / packet preparation / GPU pass の境界を先に作るのがよい。新しい target は
public module import と transitive IFC 閉包、link dependency を増やすため、次の切り出しは依存 DAG と warm
incremental build action の両方で利得を確認してから行う。

**コンパイル診断の分類と再検証（2026-10-17）:** `temp/core-text-build2.log` では最初に `Property.Types` の IFC が二つ候補として
解決エラー (`C7684`) になり、その後 `Shape.Types` と `BezierSegment` の未解決エラーが連続している。ArtifactCore / Artifact の
split-target 再構成中に実行した最初の Composition integration target build は 1,780 個の error 診断で失敗し、そのうち 82 件が
`C7684`。例として `ObjectDetector.cppm` では同じ module に対して `CMakeFiles/ArtifactCoreAI.dir/*.ifc` と
`ArtifactCoreAI.dir/Debug/*.ixx.ifc` が同時に候補となった。さらに `AssetMenuModel.cppm` のような `module X;` 実装単位が
対応 interface の宣言を解決できず、後続の型・メンバー診断が連鎖した。これは当時の未コミット split-target CMake と module-source
登録が生成した build graph の問題で、Composition controller のソース欠陥とは別だった。

重複 IFC と実装単位の CompileAs 分類を修正した後、最後に残った `C7684` は `ArtifactBasicVideoPreviewWidget.ixx` の `import std;` で、
同じ `std` に project-owned `artifact_std.ifc` と MSVC の `std.ixx.ifc` が候補となっていた。この interface は標準ライブラリ型を公開宣言に
使っていなかったため不要な `import std;` を削除した。Debug の `ArtifactWidgets` と `ArtifactModel3DLightCompositionIntegrationTest`
target がともに build 成功し、`ArtifactCompositionRenderController.cppm` の現行 snapshot / gizmo-validation 変更もコンパイルされた。
続けて `Artifact` Debug target も `Artifact.exe` まで link 成功した。post-build の `windeployqt` は vcpkg Debug 側の
`translations/catalogs.json` と `Qt6Widgetsd.dll` が見つからない警告を出したが、build 自体は成功した。
GoogleTest の Point-light Composition case は Windows Qt platform plugin を使って Debug / D3D12 と Debug / Vulkan の両方で pass。
それぞれ実際の 400×300 `composition_controller_point_light.png` を出力し、SHA-256 は
`ABCBCE0B0225AA7BF5BFE985BF3157165416BBB342B9AAFC4F1A8B8FE383C3DB` で一致した。画像は
`build/model3d_light_capture/deferred-separation/{d3d12,vulkan}/` に保存した。これは production Composition controller の GPU frame 出力と backend parity の確認であり、
frame body の worker 化や GUI 非ブロッキングは証明しない。`LNK1104` / `LNK1168` は別のリンク段階問題として扱う。

**限定非同期record window（2026-10-17、実装・画像確認）:** `ArtifactIRenderer::flushDeferredPacketsAsync()` は、許可済みpacket mixだけを1-slot workerへ渡し、`RenderCommandBuffer` の事前確保済み第2bufferへ切り替える。Composition controller はベース描画のflush地点でこのAPIを呼び、続くoverlay packetをowner thread上で準備する。先行batchは、target変更以外の即時描画barrier、texture overwrite barrier、present、destroyではcommand listの記録とExecuteCommandListsまでを完了させる。成功時のdeferred FinishFrameはworker側で後続処理と重ね、次のworker job再利用またはdestroy前に完了を回収する。target overrideだけは、worker用bufferがtarget/resource参照を保持し、owner queueが空のときに限りjoinせず状態を更新し、新bufferのtargetを再armする。Particle packet は既定blend設定の範囲で非同期workerへ渡せるようになった。pending 3D gizmo、capture mode、worker非対応packetは同期fallbackに残る。workerのrecord失敗時も、保持中のbufferをowner submitterへ同期fallbackし、teardown時はrecord/FinishFrame待ちを解消してからresourceを解放する。

`ArtifactRendererAsyncDeferredRecordingPreservesOverlayOrder` は16個の不可視solid packetと赤いbase rectをworkerへ渡し、続いて青いoverlayをowner queueへ追加する。同期基準と全画素 / RGBA channel を `1.0e-4` tolerance で比較し、重なり画素と実PNGを確認した。Debug / D3D12 と Debug / Vulkan の両方でpassし、出力PNG `build/tests/Artifact/Debug/cloner_gpu_render_captures/artifact_renderer_async_recorded_overlay_{d3d12,vulkan}.png` は SHA-256 `03D6E07A4F50A81E154D89C547A6814650680A1AE77272400EF54B5F1C3A629F` で一致した。さらに Composition point-light integration test を両backendで実行し、各400×300 PNG `build/model3d_light_capture/deferred-async/{d3d12,vulkan}/composition_controller_point_light.png` は SHA-256 `ABCBCE0B0225AA7BF5BFE985BF3157165416BBB342B9AAFC4F1A8B8FE383C3DB` で一致した。これはCompositionの最終画像 parityを示すが、当該シーンでasync preflightが必ず選択されたことや、フレーム全体のGUI非ブロッキングを単独では証明しない。

**Composition production worker selection（2026-10-17、追加画像検証）:** 点光源だけの従来シーンでは async submit counter が 0 のままになることを新しい統合 assertion で検出した。これは worker API の不通ではなく、production recorder が小さな batch を immediate path に残す 16-packet 閾値によるものだった。対象ケースにモデル背面へ小さな黒い Solid packet を16個加え、production `CompositionRenderController` の最初の描画フレームで `deferredWorkerSubmitCount()` が増えることを要求するようにした。Debug / D3D12 と Debug / Vulkan でテストが passし、各400×300の `composition_controller_point_light.png` は SHA-256 `AC7C475B62E440744DFF4D9F97D69ED63C38BC71C0020E1A67BC11254AA18754` で一致した。これはComposition controllerから実際に非同期記録workerが選択され、present画像が両backendで揃った証拠である。点光源シーンでの worker selection は閾値以上の負荷を与えた場合に限られ、小さい batch と一般のGUI非ブロッキングを示すものではない。

ビルド切り分けでは、Debug / MSBuild 18 の `Artifact` target が `Artifact.exe` まで完了し、`ArtifactIRenderer.cppm` と約54,000行の `ArtifactCompositionRenderController.cppm` を含むruntime compilationも通過した。ArtifactCoreAnalyze の失敗箇所を追い、scanner-disabled の `ImageAnalyzer.cppm` に `Image.ImageSurfaceView` / `Graphics.SurfaceColorContract` / 再公開される `Color.TransferFunction` の IFC `/reference` と provider object の build dependency が欠けていたため CMake table に追加した。その後 Debug / MSBuild の全 Artifact target が完了した。さらに Debug / Ninja/MSVC の `ArtifactCoreAnalyze` target も 1,520 action 後に `ArtifactCore.lib` と `ArtifactCoreAnalyze.lib` の link まで成功し、同じ module source map 修正を確認した。post-build `windeployqt` は翻訳catalogとDebug `Qt6Widgetsd.dll` の配置警告を出した。full Artifact Ninja app build は依存再構築が大きく中断しており、アプリ全体の Ninja 検証と warm incremental action 数は未確認である。

**再分割への示唆:** Ninja の `ArtifactCoreAnalyze` 単独 build でも Core を含む約1,500 actionが走った。これは split target が存在しても aggregate target / IFC provider 閉包が大きいと再ビルド範囲が限定されない実例である。新しい分割を加える前に warm build の action数を基準化し、implementation-only edit と interface edit の影響範囲を比較する。

**Deferred worker `FinishFrame()` の待ち合わせを分離（2026-10-17）:** 成功した非同期 recording は、owner が `ExecuteCommandLists()` を呼び packet buffer を解放した時点で immediate submit barrier を解除し、worker は同じ deferred context 上で `FinishFrame()` を完了させる。これにより後続 overlay submit / present と deferred frame cleanup が重なる余地を作った。worker の再利用は `FinishFrame()` 完了を確認してからにし、失敗時 fallback と shutdown は引き続き完了待ちする。同期 `submit()` 経路は従来どおり完了待ちする。Debug / MSBuild の `ArtifactModel3DLightCompositionIntegrationTest` と `ArtifactClonerGpuRenderContractTest` を再ビルドし、point-light Composition worker-selection case と async overlay-order image contract を D3D12 / Vulkan で個別実行して pass。Composition の 400×300 PNG は両backendで SHA-256 `AC7C475B62E440744DFF4D9F97D69ED63C38BC71C0020E1A67BC11254AA18754`、overlay contract PNG は `03D6E07A4F50A81E154D89C547A6814650680A1AE77272400EF54B5F1C3A629F` で一致した。画像と teardown の契約は確認したが、`FinishFrame()` 重複時間や GUI latency の改善量は計測していない。

**Deferred packet eligibility 拡張（2026-10-17）:** `DiligentImmediateSubmitter::canRecordDeferred()` に `GradientRectPkt`、`DotLinePkt`、`CheckerboardPkt`、`GridPkt`、`RectOutlinePkt`、`MpmSurfacePkt` を追加した。`SolidCirclePkt` は32 segment / 96頂点生成を固定長 `std::array` にしてdrawごとのヒープ確保をなくした。MPM producer は最大 `(129 × 97 × 6) = 75,078` 頂点を再利用bufferへ事前確保する。submitterは三角形を最大4,096頂点の事前確保stagingに積み、満杯前にflushするため、このMPM packetもworker recording中にCPU stagingを再確保しない。worker image contract は最大サイズのMPM三角形packetを含む27 packetを即時経路・worker command list・production submitterで描画し、画素/RGBAを `1.0e-4` tolerance で比較する。最初・staging境界後・最終三角形の画素を検査し、最大MPM packetが複数回のbounded flushを通過することも確認した。さらにproduction `ArtifactIRenderer` testはMPM packetを含む状態でworker submit count増加と可視画素を検査する。Debug / D3D12 と Debug / Vulkan で該当2ケースを各backend・個別プロセスでpassした。worker-recorded contract PNGは両backendでSHA-256 `434F17F02CF1217BD055D545E927CD63902678572D097F04C38902642A34BECB`、production renderer PNGは `07EC0D7409014D88E71866905F9327C6FD00D2177987BACA32F14EB2B2F468BB` で一致し、実画像を目視確認した。これはbounded MPM surface packetのrecord/execute parityを検証する。MPM simulation・density-field生成・surface tessellation自体は引き続きComposition caller thread上で行い、Composition内のpacket頻度やlatency改善量は未計測。

**Deferred textured-triangle packet（2026-10-17）:** `TexturedTriangleXformPkt` は固定長の頂点・UV・変換値と texture view 参照だけで記録でき、`RenderCommandBuffer::append()` がSRVをpacket内にpinするため `canRecordDeferred()` の適格packetへ加えた。worker command-list比較とproduction `ArtifactIRenderer::drawTexturedTriangleTransformed()` の画像contractに可視三角形を加え、Debug / D3D12 と Debug / Vulkan で各ケースを個別プロセス実行してpassした。worker-recorded PNG は両backendで SHA-256 `85F7F30470FFE6968AE8BBC3276B54549556408EF11C2354CA3FC721710A9602`、production renderer PNG は `FAA7BBAF984A3574D95C8F912DAF169D5A2B9A148C7F56F6178A0CCC05AD3FC8` で一致し、production画像上の三角形も目視確認した。texture生成・cache lookupはproducer側に残し、この変更は描画packetの記録先だけをworkerへ広げる。Composition内のpacket頻度やlatency改善量は未計測。

`MaskedSpritePkt` も同じ worker eligibility に追加した。scene / mask SRV は command buffer append 時にpacketへpinされ、QImage mask のtexture化は呼び出し側で済ませてからpacketを記録する。初回の描画検証で masked PSO が4×4 transform vertex shaderを使いながら offset/scale/screenSize constantを渡しており、既存の同期経路でもspriteが描画されない不整合を検出した。masked PSO を通常の2D sprite vertex shaderへ揃えて修正した。worker image contract と production `ArtifactIRenderer::drawMaskedTextureLocal()` の両ケースは Debug / D3D12 と Debug / Vulkan で個別プロセスpass。worker-recorded PNG は両backendで SHA-256 `4E6208D2EEC3312DF6699111B0D04267C3984DC18B57852BE423A5611CE0F465`、production renderer PNG は `F98B30B22C5A0C6655B3399FE8C99A563677AEB7AB5F8A982AD2A4F30B6D7E2F` で一致し、masked spriteの可視画素を確認した。texture生成・cache lookupは呼び出し側に残る。Composition内のpacket頻度やlatency改善量は未計測。

**module IFC 参照表の静的監査（2026-10-17）:** `scripts/check_module_hygiene.py` を拡張し、ArtifactCore の明示 IFC table にある source / interface / provider path、module 宣言名、同一 consumer の provider 重複を検査するようにした。`python scripts/check_module_hygiene.py` は pass。`python scripts/check_source_manifests.py` は現行作業ツリーで Artifact / ArtifactCore の未登録・stale source と focused pack 所有関係の不一致を報告しており、全件の build target ownership は未整理。これは本 renderer 変更が起こしたエラーとは断定せず、追加の target 分割前に各 source の正規 owner を揃える必要がある。

**Deferred worker stage profiling（2026-10-17、D3D12 / Vulkan 実測）:** 既存の opt-in `ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP` が有効な場合に限り、worker の `record` / `FinishCommandList()`、owner thread の `ExecuteCommandLists()` 呼び出し時間、worker の `FinishFrame()` を別々に出力するようにした。Composition 側には従来の setup / pass / render-to-present 計測があり、無効時には追加 timer とログ整形を行わない。Point-light production integration を Debug / D3D12 と Debug / Vulkan で実行し、worker 選択を確認した。先頭の16 packet batch は D3D12 で record 1.549 ms / FinishCommandList 0.049 ms / ExecuteCommandLists の CPU 所要 3.576 ms / FinishFrame 0.013 ms、Vulkan で 0.575 / 0.066 / 0.731 / 0.027 ms だった。Composition の同じ先頭フレーム `renderPresentMs` は D3D12 1,514.84 ms、Vulkan 3,484.03 ms であり、初回初期化・全描画・present を含むため worker 作業時間と単純比較できない。出力した400×300 PNGは両backendで SHA-256 `AC7C475B62E440744DFF4D9F97D69ED63C38BC71C0020E1A67BC11254AA18754` で一致した。今回の計測はCPU command recording / submission と `FinishFrame()` の所要時間であり、GPU実行時間やGUI latency の改善量を示さない。ExecuteCommandLists と present は引き続き owner/caller thread に残る。

**Glyph Text worker eligibility guard（2026-10-17）:** 16個の packet 条件を満たす batch に Glyph Text packet を混ぜた場合も `flushDeferredPacketsAsync()` が同期 owner 経路へ戻り、deferred worker submit count を増やさず、緑色の glyph を含む実画像を保存する contract test を追加した。実装を追うと `submitGlyphText()` は submitter-local HarfBuzz/Qt shaping、フォント fallback 解決、可変長 glyph 配列、GlyphAtlas の QImage / map 更新を行い、初回は同じ関数内から Diligent texture を作成する。worker 用 submitter は別 atlas を所有し、現在は GPU atlas texture を事前作成していない。このまま packet allow-list だけを拡張すると hot-path allocation と worker 側 resource creation の境界を越えるため、今回の段階では owner fallback を画像付きで固定した。producer 側の同じ batch を使った同期描画と fallback 後のフレームを `1.0e-4` tolerance で比較し、Debug / D3D12 と Debug / Vulkan で pass。両 backend の `artifact_renderer_glyph_text_owner_fallback_{d3d12,vulkan}.png` は SHA-256 `0E32C158B9A2D83D1CD4948D44744B110215147C455A130E423675BAF0B054F6` で一致し、緑色の glyph pixel を検査した。将来の worker 化には producer 側で bounded glyph draw data と atlas texture/SRV の寿命を確定し、worker が QFont・atlas cache・resource creation に触れずに記録できる packet contract が必要。

**MPM packet の bounded-work 選択（2026-10-17）:** `canRecordDeferred()` は packet 数に加えて MPM surface の vertex count を仕事量へ換算する。最大75,078頂点の単一 MPM packet は、4,096頂点 staging chunk を基準とする threshold を満たし、`flushDeferredPacketsAsync()` で worker recording に入る。3頂点の小 packet は owner path に残る。両ケースのテストは Debug / D3D12 と Debug / Vulkan で pass し、worker submit count、緑色の可視 pixel、PNG を確認した。大 packet の worker PNG は両 backend で SHA-256 `9B22B70F4E8B8E9452B157145BD68F811620921447FB02F334FA23A483ABA5C3`、小 packet owner PNG は `09A9E08A97489B45E1F5105BE3021BF6C37639356CCA9B03F7401F16953579AA` で一致した。opt-in stage profile の大 packet は D3D12 で worker record 3.966 ms / FinishCommandList 0.032 ms / owner ExecuteCommandLists CPU 所要0.513 ms / worker FinishFrame 0.011 ms、Vulkan で3.697 / 0.039 / 0.210 / 0.017 ms だった。これは worker へ移った command recording の観測値で、同じ workload の owner record 時間との比較や GUI latency 改善を測ったものではない。MPM mesh の生成・simulation は caller 側に残る。

**Cloner stale range の split-submit fallback image contract（2026-10-18）:** `SubmittedPlacementRangeFallsBackAfterQueueDrain` は Cloner dispatch を submit した後、旧 batch generation の placement range が `drawSpriteTransformedClonerGrid()` に拒否されることを確認する。Composition の実 callsite と同じく、呼び出し元が通常の transformed sprite draw へ fallback した結果、3つの可視 clone が残る。Debug / D3D12 と Debug / Vulkan で pass し、実フレーム PNG `artifact_renderer_cloner_stale_range_fallback_{d3d12,vulkan}.png` を保存した。SHA-256 は両 backend で `5CFCE0311CB2DB4A2DC9B42080F7676BE635AB9A328611817D80853544E0CCB5` で一致した。これは submit barrier をまたいだ range rejection と画像 fallback の契約を示す。production observer と indexed draw callback の間に queue drain がある Composition 経路は、引き続き source 調査上見つかっていない。

**Deferred barrier の owner wait 計測（2026-10-18）:** `ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP=1` のときだけ `DeferredPacketRecordWorker::executeAndRelease()` が `recordReady_` condition wait を `recordReadyWait` として出力する。Production Composition point-light integration を実 device 上で Debug / D3D12 と Debug / Vulkan で実行し、16 packet batch の wait は両 backend とも `0.000 ms`（表示精度 0.001 ms）、記録は D3D12 0.798 / 0.206 ms、Vulkan 0.224 ms、ExecuteCommandLists 呼び出しのCPU所要はそれぞれ 3.796 / 0.177 ms、0.547 ms だった。worker `FinishFrame()` は 0.009 / 0.013 ms、0.017 ms。point-light の 400×300 frame PNG は両 backend で SHA-256 `AC7C475B62E440744DFF4D9F97D69ED63C38BC71C0020E1A67BC11254AA18754` で一致した。この workload では barrier 到達前に worker recording が済んでいるが、単一 scene の測定で GUI latency 改善量や他の workload の待ち時間は示さない。profile 無効時は計測 timer を開始せず、出力もしない。

**Composition point-light の再計測（2026-10-18、同一テスト・両 backend）:** `CompositionControllerAppliesPointLightLayer` を `ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP=1` で再実行し、同テストが要求する production `deferredWorkerSubmitCount()` 増加と400×300画像の出力を確認した。各backendのPNG (`temp/deferred-frame-profile/{d3d12,vulkan}/composition_controller_point_light.png`) は SHA-256 `AC7C475B62E440744DFF4D9F97D69ED63C38BC71C0020E1A67BC11254AA18754` で一致した。16-packet batch の worker record / FinishCommandList は D3D12 cold `1.113 / 0.338 ms`、次フレーム `0.196 / 0.035 ms`、Vulkan `0.193 / 0.258 ms`。owner の record-ready wait は全て `0.000 ms`（表示精度0.001 ms）。ExecuteCommandLists 呼び出しCPU時間は D3D12 cold `3.384 ms` / 次フレーム `0.190 ms`、Vulkan `0.471 ms`、worker FinishFrame はそれぞれ `0.014 / 0.011 ms` と `0.152 ms` だった。Composition の render-to-present 区間は D3D12 初回 `1240.25 ms` / 次フレーム `16.77 ms` / 後続 dirty frame `56.21 ms`、Vulkan 初回 `2502.12 ms` / 次フレーム `395.65 ms`。したがって packet recording の worker 移行と順序は実フレームで確認できたが、Composition 全体の GUI-thread block は残り、特にこの Vulkan workload では worker record 時間より render-to-present 区間が大幅に長い。これらは profile 有効の単一 integration workload であり、GPU実行時間・present待ち・一般的な latency 改善量の分離計測ではない。

**Composition render pass subphase の実測（2026-10-18、両 backend）:** 上記 opt-in profile に既存 phase timer の出力を加え、`CompositionControllerAppliesPointLightLayer` を Debug / D3D12 と Debug / Vulkan で再実行した。両実行で deferred worker submit count 増加を確認し、出力400×300 PNG は両 backend とも SHA-256 `AC7C475B62E440744DFF4D9F97D69ED63C38BC71C0020E1A67BC11254AA18754`。整数ms表示の pass profile は D3D12 初回 frame で `base/surface/mask/composite/post=0/0/0/0/0`, `overlay=11`, `flush=0`, `present=2`、次 frame で `0/0/0/0/0`, `2/0/1`、後続 dirty frame で `17/0/0/0/0`, `1/3/0`。Vulkan 初回 frame は `0/0/0/0/0`, `3/0/1`、次 frame は `base=308`, `surface/mask/composite/post=0`, `overlay=1`, `flush=1`, `present=0`。同 Vulkan frame の `renderPresentMs` は `388.52 ms`、16-packet worker record / FinishCommandList は `0.193 / 0.036 ms`、record-ready wait `0.000 ms` だった。従ってこのフレームでは base pass の caller-side elapsed time が render-to-present の大半を占める候補だが、phase 計測は整数msに丸められ、GPU実行・同期待ちの内訳を分離しないため、base pass内のどのAPIが時間を使ったかは未特定。これは次に `beginGpuBasePass()` / `seedGpuBasePassBackground()` の呼び出しと同期点を絞って調べる根拠であり、base passをworkerへ移せることの証明ではない。実画像は D3D12 / Vulkan とも同じ pink 3D surface を出力し、テストは両 backend で pass した。

**Composition base setup の clear 区間計測（2026-10-18、両 backend）:** profile有効時だけ `beginGpuBasePass()` 全体を viewport/depth setup と accumulation clear 区間に分けた。Point-light integration は D3D12 / Vulkan とも passし、実フレームPNGは各400×300、両 backend の SHA-256 `AC7C475B62E440744DFF4D9F97D69ED63C38BC71C0020E1A67BC11254AA18754`。Vulkan の2枚目の frame は `renderPresentMs=388.61`、`baseMs=312`、そのうち `baseSetupMs=311`、`backgroundSeedMs=1`、`accumulationClearMs=154`、`depthClearMs=0`（各値は整数ms切り捨て）。D3D12 後続 dirty frame は `renderPresentMs=45.70`、`baseMs=16`、`baseSetupMs=15`、`backgroundSeedMs=1`、`accumulationClearMs=7`。この区間timerは clear 呼び出し周辺のsubmit/state処理を含むため、154msをGPU clear単体の所要時間とは解釈できない。profileが示すのはVulkanこのframeの遅延が背景seedではなくbase setup側にあり、その中で accumulation clear 区間も大きいことまでで、即時contextの待ち・driver work・resource transitionの分離は未確認。次は該当区間内の `submitQueuedDraws()` と Diligent clear / target binding を切り分け、同期context操作をworkerへ移す前に順序・resource lifetime条件を確認する。

**Accumulation clear の不要な 3D PSO 切替を除去（2026-10-18、実装・実フレーム確認）:** depth clear 前の `pushRenderTarget()` は `setRenderTargetOverrides()` 経由で `PrimitiveRenderer3D::setRenderTargetFormat()` を呼び、color format が変わると `createPSO()` を同期実行していた。clearしか行わない setup で描画用PSOを作り直す必要がないため、明示 RTV/DSV を指定して深度をclearするAPIを追加し、accumulation clearでも汎用target override stackを使わないようにした。前回のVulkan dirty frameは`renderPresentMs=388.61`（`baseSetupMs=311`、`accumulationClearMs=154`）、今回の同じProduction Composition point-light testは`renderPresentMs=77.89`（整数ms、通常フレーム）。D3D12は前回`45.70ms`、今回`16.48ms`。profileのclear別内訳は今回整数msで0と表示され、clear API単体の精密な改善値とはみなせない。Vulkanは初回frameにPSO等の初期化を含む長い起動コストも残る（今回`frame=0 renderPresentMs=2460.59`）。両backendのテストはpassし、400×300の出力PNGはSHA-256 `AC7C475B62E440744DFF4D9F7D69ED63C38BC71C0020E1A67BC11254AA18754` で一致し、目視でもピンク色の3D面を確認した。これは不要なclear-time format切替の回避と当該シーンのフレーム出力を示す。GUI thread全体の非同期化や他シーンでのlatency改善量は未確認。

## 11. 未確認事項

1. Cloner placement range の batch generation は packet submit と renderer / placement GPU resource の再初期化で進む。
   `dispatchClonerGridPlacement` の production callsite は共通 placement observer 1 箇所に集約され、observer の設定後は
   indexed draw callback へ同期的に進む。Composition surface path は texture binding を observer dispatch より先に解決し、
   image sprite path も source texture を先に解決する。Puppet 分岐は同じ stable image key の texture cache hit 後に
   triangle packet を積む。確認した production 経路に dispatch と dependent draw の間の queue drain はない。
   headless split-submit test は明示 flush 後の stale range 拒否と CPU fallback を検証する。残る未確認は generation counter の
   64-bit wraparound と renderer device loss 中の再初期化である。
2. GPU Effect の shared immediate context 利用が Composition の frame 内だけか、frame 外の編集・初期化からも
   起きるか。renderer queue / cache 経由の context と thread も呼び出し単位で追う。
3. deferred path の packet の context 経路と順序。`BillboardPkt`、Particle upload / prepare / draw、通常の QImage
   overload、Cloner dispatch / draw は `recordCtx` を使う。`LinePkt` / `QuadPkt` / `SolidTriPkt` / `AtlasSpritePkt` は
   種類の切替時に前の保留バッチを flush する。Particle の `UpdateBuffer` は D3D12 / Vulkan backend で context 内 upload
   allocation への即時コピー後、同 context の command stream に記録される。Cloner は producer が output cursor の range を
   順序どおり予約して `ClonerGridDispatchPkt` を draw packet より前に追加し、submitter が `recordCtx` で params を upload して
   compute を dispatch する。後続 sprite は dispatch packet index を参照し、Map 失敗時は CPU fallback する。packet は PSO / SRB /
   params buffer / output buffer を保持する。legacy `BillboardImagePkt` は submit 時に解決した texture を packet 内に保持し、
   `recordCtx` で描画するようにしたため、現在の packet variant に buffer-wide immediate fallback は残らない。
   QImage texture の生成は呼び出し thread 上に残り、worker へは移していない。Cloner は同じ deferred recorder の中で
   compute → draw と buffer transition を記録できる形になったが、
   16-packet Cloner placement + sprite case は Debug / Vulkan と Debug / D3D12 の両方で build/runtime pass。
   追加した `DeferredClonerRangeFallsBackAcrossImmediateClearBarrier` は、15 filler + dispatch を async worker に渡した後、
   即時 clear barrier で worker submit を完了し、stale placement range を拒否して CPU transform fallback へ移ることを確認する。
   3つの可視 clone の frame image を両 backend で保存し、SHA-256 `5CFCE0311CB2DB4A2DC9B42080F7676BE635AB9A328611817D80853544E0CCB5` で一致した。
   これは明示的に clear を挟む renderer contract の実機証拠であり、Composition の通常 observer-to-draw path に barrier が挿入される頻度は示さない。
   range cursor reservation と command buffer append は producer 側の直列操作のままで、
   まだ worker から renderer を呼べない。worker 移行では submitter の mutable batch/PSO 状態、ParticleRenderer の GPU resource state と
   PrimitiveRenderer3D の SRB / constant buffer 状態も recorder ごとに所有させる必要がある。Bindless submitter は
   fallback 有効時に単一 batch 条件を事前検査し、
   partial draw 後に buffer 全体を再送しない。条件を満たす 16 packet 以上は既存の deferred context へ同期記録し、
   immediate context で実行する。`RenderCommandBuffer::moveAppendBuffer()` は source packet を順に append し、
   `RenderCommandBufferPool::tryMergeInto()` は worker slot の添字順に buffer を結合する。並列 polyline / polygon
   生成は worker index ごとに入力の連続範囲を担当させるため、worker の完了順による packet 順序の逆転はない。
   Bindless Sprite 経路は pending gizmo がある batch を通常 submitter に渡し、overlay の submit が次回へ遅れないようにした。
   `ArtifactIRenderer::submitQueuedDraws()` が呼ぶ submitter は queued 2D packet の後に ordered gizmo topology runs を
   同じ recording context へ記録する。対象 batch は通常 packet 数だけでなく gizmo vertex 数も threshold に含めるため、
   gizmo-only の大きい batch も deferred recording に入る。line / triangle の run 順、単一 upload、SRB/PSO 切替は source で
   確認した。明示的な 3D card / shape barrier も queued 2D と gizmo を先に submit する。Gizmo overlay の実画像 parity は
   swapchain target で Debug / D3D12 と Debug / Vulkan の両方で確認した。Map/Unmap failure injection と resource-state の
   独立計測は未実施。offscreen RTV/DSV 上で Billboard worker batch の後に深度付き immediate 3D card を描く barrier も
   production renderer image contract で両backendの全画素 parity を確認した。deferred context は immediate viewport / scissor を
   継承しないため、`RenderViewportState` の保持と target bind 後の再適用、viewport 切替前の queued packet submit を追加した。
   単一 solid rect の部分 viewport / scissor parity と queued packet 中の viewport 切替前 drain も Debug / D3D12 と Debug / Vulkan で確認した。
   Artifact の描画経路で使用される submitter は `DiligentImmediateSubmitter` であり、`DiligentBindlessSubmitter` の
   safety 修正は現行 renderer の経路からは到達しないことも source search で確認した。
3. command list 実行までに保持すべき resource と command buffer の寿命、順序、frame generation の契約。
4. swapchain `Present` のスレッド要件（D3D12 / Vulkan 別）と vsync block の所在。
5. multi-viewport 時の controller / swapchain / deferred recorder の多重化と owner thread。
6. `MultithreadedResourceCreation` を必要とする箇所があるか。deferred context 導入とは別に確認する。
7. `readbackTextureViewToImageAsync()` は copy 前に packet を drain し、fence 後の `MapTextureSubresource()` / `UnmapTextureSubresource()` を readback worker 専用の deferred context へ移した。Diligent API と D3D12 / Vulkan staging map 実装のソース根拠に加え、16 packet Deferred frame の async readback を開始して renderer を破棄し、callback 完了後の実画像を同期 owner frame と全画素比較する contract を Debug / D3D12 と Debug / Vulkan で確認した。両 PNG は SHA-256 `9F90B34416CF9A860F0CAEA56B54FCD361D363340593AF3BF6EDB5BA8286FACB` で一致した。これは正常な GPU 完了時に teardown が callback を待つ経路を示す。device loss / hung GPU で `IFence::Wait()` が返らない場合の終了挙動と待ち時間は未確認。
8. Particle の Deferred 経路は 15 個の不可視 rectangle + 1 Particle の image contract で即時経路と画素数・重心・coverage mask が一致し、Debug / D3D12 と Debug / Vulkan で個別プロセス pass。`readbackToImageF32()` は RGBA texture から読み出した raw float を RGBA 入力として canonical BGRA image へ変換する。Windows CTest は実行 target 隣の Qt offscreen plugin を使う。Particle depth 無効時の DSV bind warning は解消済み。worker ownership は未解決。
9. Solid `CardPkt` は active projector がない時だけ owner queue に追加し、packet が呼び出し時の view / projection、active DSV を command-list 実行まで保持する。worker 専用 `PrimitiveRenderer3D` の PSO / SRB / constant buffer と record context で描画する。offscreen Billboard barrier 後の Card batch を 15 個の不可視 filler と合わせて worker eligible にし、同期 owner image と比較するケースが Debug / D3D12・Vulkan の個別プロセスで pass。各パス開始時に同じ camera を設定し、deferred packet flush 前には camera scope が reset される production 順も通す。両 PNG は SHA-256 `C189DB4BA742C183948DE757721FDBCC44B28C8248975D31ECBF69A3195FFC1B` で一致し、center pixel は `(63, 108, 249, 255)`。追加した projector-active fallback ケースも両 backend で pass し、先行する 16-packet batch だけが worker submit count を増やし、projected Card は owner で描画されることを offscreen pixel で確認した。`artifact_renderer_projector_card_fallback_{d3d12,vulkan}.png` は SHA-256 `082D6FD62D1A33852FA30B129D763720CB03E2BB7AD71C50464BB226B26C0556` で一致。
10. `TexturedCardPkt` は SRV、active DSV、呼び出し時の view / projection を保持し、worker-local card SRB / constant buffer で depth-writing pass → transparent pass の順に記録する。projector が有効な場合は owner immediate fallback を使う。enqueue 後に producer の texture reference を解放し、camera reset 後に worker で 2 pass 描画した offscreen frame は同期 owner image と全画素一致、Debug / D3D12 と Debug / Vulkan の個別プロセスで pass。`artifact_renderer_deferred_textured_card_{d3d12,vulkan}.png` は SHA-256 `BA4053903EC5A064ECB798C6EAE081527DEB505B104E884059EBB24F0BFE45CC` で一致し、center pixel は `(246, 131, 86, 255)`。これは単一 textured card と 15 個の不可視 filler の contract で、Composition の通常頻度や GUI latency 改善量は示さない。
11. `draw3DShape()` は既存の triangulated vertex list を `ShapeTrianglesPkt` に move し、active DSV と呼び出し時 camera を保持して ordered worker queue に渡す。worker は初期化時確保した 256 頂点の Shape VB 容量内だけを受理し、mapped buffer に直接頂点を変換して以前の staging `std::vector<ShapeCardVertex>` を除いた。容量超過と queue failure は owner immediate path に戻る。camera reset 後に triangle shape を描いた offscreen frame は同期 owner image と全画素一致、Debug / D3D12 と Debug / Vulkan の個別プロセスで pass。両 PNG の SHA-256 は `278F02F61C42D3198EB08CD6B3872D59F43019459A387EBB60C1C9335D2DD304` で一致。polygon triangulation と triangle-list 展開の producer-side vector allocation、容量超過 fallback の頻度、Composition 実 workload の latency は未計測。
12. 通常 `GlyphTextPkt` と `GlyphTextXformPkt` は owner 側で shaping / fallback font 解決 / atlas acquire を済ませ、事前確保した deferred double-buffer の各 `RenderCommandBuffer` に 4,096 glyph slot の snapshot を置き、atlas SRV を buffer が retain してから worker に渡す。一般の `RenderCommandBufferPool` worker slot には追加領域を予約しない。初回 atlas texture 作成、full-atlas upload / eviction、buffer 容量超過、frame debug / cost capture は owner fallback。変換文字の matrix / device-pixel-ratio 描画は従来の処理を維持し、glyph rect の供給だけ prepared snapshot に切り替える。headless Qt では未登録の Arial が複数文字を同じ replacement glyph に解決したため、この test は repository 同梱 Roboto を明示登録し、`QRawFont` の distinct glyph ID / mask と実 frame の複数 glyph silhouette を検査する。冷たい atlas の owner fallback で `Seed` だけを登録した後、追加 glyph を含む通常文字・変換文字を worker batch で描画して atlas の owner-side update 順序も通し、warm 後の同期 owner image と比較する offscreen frame を同期 owner image と全画素比較する contract は Debug / D3D12 と Debug / Vulkan で pass し、両 PNG `artifact_renderer_glyph_text_deferred_{d3d12,vulkan}.png` の SHA-256 は `8A4D37BA98CA877D9569BF1277E6333AAFBC0E0F29528E7EBC166E2A5C58EB79` で一致した。さらに別の bounded ASCII fixture は raw glyph bitmap と2px paddingの総面積が2048×2048 atlasを超えることを実行前に確認し、先に同じ大サイズ font の atlas texture を作成・upload して clean にしてから容量超過バッチを準備する。これにより cold first-use とは独立して atlas eviction を踏み、worker submit count を増やさず owner fallback で可視 glyph frame を出力することを Debug / D3D12 と Debug / Vulkan の実デバイスで確認した。`artifact_renderer_glyph_atlas_eviction_fallback_{d3d12,vulkan}.png` は両 backend で SHA-256 `EAB84E5FF0CA9D0A6B7104BB44B8984EFE0AD59DC9DD55E1FF2FFDE4F16151E5` で一致した。4,097 個の prepared glyph を作る warm-atlas case では 4,096 slot 上限で worker recording を拒否し、owner fallback の可視文字 frame を D3D12 / Vulkan で出力した。`artifact_renderer_glyph_snapshot_capacity_fallback_{d3d12,vulkan}.png` は SHA-256 `28B2220508B23A29D4551B4B1D73E7D503BF719B66929576414E1CD0EE7F791E` で一致した。cold first-use fallback も同じ test 内で確認した。これは可視 glyph 形状を含む packet recording parity と容量超過時の owner fallback を示すが、owner 側の HarfBuzz / atlas preparation work、通常 Composition での glyph batch 頻度、GUI latency 改善量は示さない。

13. warm atlas に `X` だけを先に upload し、新規 glyph を含む `Deferred` の15 filler + GlyphText batch を worker に記録させた直後、owner が次 buffer に blue overlay を積む。flush 後の frame を同じ workload の同期 owner image と全画素比較し、overlay が先頭 glyph より上、残り glyph が見えることを確認した。Debug / D3D12 と Debug / Vulkan の実 device で pass し、両 PNG `artifact_renderer_deferred_glyph_overlay_{d3d12,vulkan}.png` の SHA-256 は `CC001DBF4E2B7684FCD0E8FD166D283A1FF9997A59642C8B4BCA2384B0B0B6FB` で一致した。これは glyph atlas update と base-worker → owner-overlay の限定順序を検証し、Composition frame の一般的な GUI 非ブロッキングを証明するものではない。

**Deferred frame async readback と renderer teardown（2026-10-18）:** renderer queue に15個の不可視 filler と可視 rect を積み、明示 flush 前に `readbackToImageAsync()` を呼ぶ。readback は packet drain 後に staging copy を発行する。callback から実 QImage を受け取った後の image を同期 owner rendering の frame と全画素比較し、renderer scope を callback の完了待ちより先に抜ける test を追加した。Debug / D3D12 と Debug / Vulkan で実 device を使って pass し、各 backend の PNG は SHA-256 `9F90B34416CF9A860F0CAEA56B54FCD361D363340593AF3BF6EDB5BA8286FACB` で一致した。通常完了の teardown が async pool の仕事を待ち、readback context / staging image の寿命を保つことを示す。GPU hang / device loss の無期限 fence wait は未検証。

**Reusable video texture の Cloner / masked packet 順序（2026-10-18）:** 同一 video layer の reusable 4×4 texture slot に red frame を解決してから、15 packet 相当の Deferred workload を async handoff し、owner が同じ slot に blue frame を upload する。Cloner ケースは GPU dispatch と3つの可視 sprite、masked ケースは白 mask 付き texture draw を含む。どちらも red Deferred frame を同期 owner baseline と全画素比較（RGBA float の最大誤差 `1.0e-4`）し、slot 更新後の次 frame が blue に変わることを probe pixel で確認した。Debug / D3D12 と Debug / Vulkan の実 device で各 test を個別実行して pass。4組の first / second frame PNG は `build/tests/Artifact/Debug/cloner_gpu_render_captures/` に保存され、同一ケースの backend 間 SHA-256 が一致した。Cloner first / second は `74BA55BB60526FA817E87DE8FC837B3B93BD93D1FA158DA5BEB9A0F9F9F03982` / `BBE7EE1C5B71549A2366493074D35025A20142529577B0169F8789F223077398`、masked first / second は `FF0FEF7C917DBA84D1554283B8E2285978F2C1E71D3F27B7293195567C796572` / `1B6EAEF93115F273DB138C6E18FA52583FBD62BFCE392FCA6F9AAA56C343B12A` で一致した。実 PNG を目視し、Cloner の red sprites と masked blue frame を確認した。Composition controller の実 frame sequence と通常 queue occupancy 分布は未検証。

**Video texture queue saturation と owner shape fallback（2026-10-18）:** renderer 起動時に確保する 16,384 packet queue を実際に満たし、可視 red sprite を含む async Deferred batch を記録した後、同じ reusable video slot を blue に更新した。最初の出力は red sprite のまま保たれ、次 frame の blue sprite と三角 shape を含む第二 batch でも queue を満たしてから `draw3DShape()` を呼ぶ。容量不足時には同期 drain が起こり、worker submit count が呼び出し中に1増え、shape は owner immediate fallback で描かれた。両 frame の色 probe と shape center pixel を検査し、Debug / D3D12 と Debug / Vulkan の個別 process で pass。PNG `artifact_renderer_video_queue_capacity_first_{d3d12,vulkan}.png` / `artifact_renderer_video_queue_capacity_fallback_{d3d12,vulkan}.png` は `build/tests/Artifact/Debug/cloner_gpu_render_captures/` に保存し、backend 間 SHA-256 は first `EF2F20156DCC758374D60DB13C7231322289100D258274AFF202037770C719F9`、fallback `4B1E3308A4B3E680F81E2BABF8E9012F102C788B37CA22A77C498D0E2B4BA44E` で一致した。初回の赤 frame assertion で `PrimitiveRenderer2D::drawTextureLocal()` が video reader epoch を記録しないことを検出し、packet append 後に同 epoch tracking を加えた。production Composition の直接経路では transformed sprite が使われるが、一般の texture-view sprite API も reusable slot の寿命契約を守るようにした。これは renderer-level full-capacity fallback と texture overwrite ordering の image contract であり、Composition controller の実フレーム sequence や通常 queue occupancy 分布までは示さない。

---

## 12. 参考ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  （`:16772`, `:18785`, `:19486`, `:19518`, `:19598`, `:19621`, `:19723`, `:20080`,
  `:42329-49391`, `:42400`, `:42548`, `:48570-48630`）
- `ArtifactCore/src/Thread/PreciseTicker.cppm:21,113`
- `ArtifactCore/src/Render/RenderWorker.cppm:39-42`（空スタブ）
- `Artifact/include/Render/DiligentDeviceManager.ixx:19-64,144-155`
- `Artifact/src/Render/DiligentDeviceManager.cppm:201,745,1426,1511,1581,1612,1651,1952`
- `Artifact/include/Render/DiligentImmediateSubmitter.ixx` / `Artifact/src/Render/DiligentImmediateSubmitter.cppm:1008-1520`
- `Artifact/include/Render/PrimitiveRenderer3D.ixx:138-145`
- `Artifact/src/Render/PrimitiveRenderer3D.cppm:914-1162,1400-1450,1988-2010`
- `Artifact/include/Render/RenderCommandBuffer.ixx:293-319,359-370,455-490`
- `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm:181-218`（independent device）
- `Artifact/include/Render/ArtifactIRenderer.ixx:189-190,228,270-280,753`
- `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`
- Diligent Engine: [D3D12 architecture](https://diligentgraphics.com/diligent-engine/architecture/d3d12/) — deferred recording and immediate execution.
- Diligent Engine: [Managing Resource Lifetimes](https://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-resource-lifetimes/) — keep resources alive until deferred command lists execute.
- `docs/planned/MILESTONE_EFFECT_PARAM_UI_PERFORMANCE_2026-10-16.md`
- `docs/technical/HOT_PATH_RULES.md`
- `AGENTS.md`（Diligent severe 領域 / 最小差分 / 子リポジトリ境界 / flag 契約）

**Deferred Particle handoff（2026-10-17、production frame画像確認）:** `flushDeferredPacketsAsync()` が Particle を含むbatchを拒否していた原因は、worker submitter に `ParticleRenderer` を設定する前に `canSubmit()` を呼んでいた順序だった。前のasync job完了後、eligibility preflightより先に同rendererをworkerへ設定し、mutable renderer state の診断読み取りはworker job active中に避ける。production `ArtifactIRenderer` test では cloner、sprite、particle、textured/masked sprite、最大MPM surface をworkerへhandoffし、その後owner queueへ追加したoverlayを同じフレームで描画する。Debug / D3D12 と Debug / Vulkan で個別プロセスpassし、出力PNG `artifact_renderer_async_deferred_cloner_sprites_particle_{d3d12,vulkan}.png` はSHA-256 `29618E7D046572363EEC2D81D38EF3F6A83F65657C02FC132CD79B7EF16F0002` で一致した。これは当該packet mixとoverlay順の描画結果を確認するが、すべてのParticle render optionやGUI latency改善量は示さない。

**Deferred Particle upload workload threshold（2026-10-17、連続frame画像確認）:** `ParticlePkt` は通常1 packetのままでも `ParticleRenderer::updateBuffer()` が最大100,000個の `ParticleVertex` を一度に `UpdateBuffer()` する。eligibility work estimate に4,096 vertexごとのbounded upload workを加え、65,536 particle（6 MiB upload）の単独batchをworkerへ選択するようにした。小さい1 particle packetはowner pathに残る。同一renderer上でworker large packetを連続2 frame記録した後、小packet owner frameも描画するcontractを追加し、可視Particleと保存PNGを検査した。Debug / D3D12 と Debug / Vulkan でpass。先頭worker PNG `artifact_renderer_particle_payload_worker_{d3d12,vulkan}.png` はSHA-256 `C498C904D5DB70965A3EA52480328C07D9756D27F9CC0B76AE4EA980390A50AF`、2枚目のworker PNG `artifact_renderer_particle_payload_worker_repeat_{d3d12,vulkan}.png` と小packet owner PNG `artifact_renderer_particle_payload_owner_{d3d12,vulkan}.png` は `7FD61C1B0AF584E17EAA66CF6B8B1EEA2AA15CD509AD24D22ADD9DB7900A52C5` で各backend一致。両frame後のworker heap high-water markはD3D12 6 MiB / 8 MiB、Vulkan 6 MiB / 7 MiBのままで、D3D12は8 MiB dynamic pageを1回生成した。これは同一renderer内の容量再利用を示唆するが、per-frame allocator call数は直接計測していない。容量上限はrendererの100,000 particle bufferでbounded。GUI latency改善量も未確認。

**Deferred textured Billboard packet（2026-10-17、production frame画像確認）:** `ArtifactIRenderer` に camera-facing billboard packet API を追加し、worker submitter は owner と独立した `PrimitiveRenderer3D` の PSO / SRB / constant buffer を使う。worker recording 開始時に camera matrices と viewport snapshot を渡し、`BillboardPkt` のSRVは既存 `RenderCommandBuffer::append()` が保持する。QImageを含む `BillboardImagePkt` はworker eligibilityから除外し、画像変換・texture cache lookupをowner側に残す。depth無効のBillboard PSOはDSV formatを要求しない設定とした。即時描画とworker描画を全画素比較し、packet後に元texture参照を解放してもworker frameが描けることを確認した。新規ケースと既存 `*Deferred*` 8ケースは Debug / D3D12、Debug / Vulkan の個別プロセスでpass。PNG `artifact_renderer_deferred_billboard_{d3d12,vulkan}.png` は両backendでSHA-256 `4B1EEE989188A3B4204E950BFBC6133CA0839E3E3AFAA6D4274E57BB881C9428` で一致し、実画像を目視確認した。worker用 `PrimitiveRenderer3D` は初期化時にshader / bufferを別途作成するため起動時間への影響は未計測。offscreen / swapchain以外のDSV構成、他の3D packet種、Composition latency改善量は未検証。

**Deferred Gizmo geometry transfer（2026-10-17、async frame画像確認）:** handoff前にowner `PrimitiveRenderer3D` の bounded vertex / topology-run arrays をworker rendererへswapし、worker固有SRBと事前確保済みGPU vertex bufferでGizmo overlayを記録する。buffer capacityが足りない場合のGPU buffer拡張はworker dispatch前のcaller threadで行い、worker recording中のresource creationを避ける。workerの `submitImpl` は2D packetの後にGizmo runを順番どおりdrawし、ownerは次のoverlay geometryを別のqueueへ蓄積できる。小さいGizmo batchは既定work threshold未満のためowner pathに残る。15個の不可視filler + base rectangle + Gizmo lineをasync handoffし、その後overlay rectangleをownerでsubmitするケースを即時描画基準と全画素比較した。既存小batch fallback caseも含め Debug / D3D12 と Debug / Vulkan でpass。PNG `artifact_renderer_async_deferred_gizmo_{d3d12,vulkan}.png` は両backendでSHA-256 `9D946DE6035DD5555037A0A0BCFFDD8F178E7C8AB53F22879764554FDA310E6F` で一致し、実画像を目視確認した。depth無効のGizmo pipelineはDSVをbindしない。これは2D batch後に描くGizmo overlayのworker recording parityを検証するもので、full GUI-thread non-blockingやComposition latency改善量は未測定。

**Composition render owner 移行前の所有権監査（2026-10-18）:** 現行controllerでは `renderOneFrame()` とrender tickerのqueued callbackがGUI thread上で `prepareHostFrame()` / `renderOneFrameImpl()` を呼ぶ。`renderOneFrameImpl()` 内の直接 `owner->` 呼び出しはvideo debug通知2箇所とdirty通知1箇所に限られるが、layer traversal、effect、overlay helperへmutableなcomposition/layer参照とcontroller `Impl` stateを渡すため、関数をそのまま別threadへ投げてもthread-safeにはならない。また `CompositionRenderController::initialize(QWidget*)` は `ArtifactIRenderer::initialize(QWidget*)` をGUI thread上で呼び、rendererは `DiligentDeviceManager::initialize(QWidget*)` の共有device/immediate contextを取得する。`present()` も同contextでqueueをdrainしてからswapchainをPresentする。従ってrender owner移行には、(1) UI編集と同時に読めるcomposition参照を渡さず、変更をcoalesceしたrevision付きimmutable frame snapshot / worker-owned scene cloneを用意すること、(2) renderer・共有immediate context・swapchainの単一thread所有権または全shared-context利用者の直列化契約、(3) resize / teardown / HWND寿命とworker完了を同期する境界が必要。`ArtifactAbstractComposition::revision()` は `changed()` ごとに増えるため静的scene payloadのcache key候補になる一方、`goToFrame()` はrevisionを増やさずにlayerを更新し、global `PhysicsSystem` のsoft-body restore / granular replay / collision / joint evaluationを実行する。さらに `PhysicsSystem::instance()` はprocess-wide singletonで、`PhysicsSystem.cppm` のworld registry / snapshot操作にmutexはない。worker cloneのframe評価を単に別threadで実行すると、GUI側compositionのsimulation操作と同時に同じregistryへ触れる可能性がある。従ってframe番号を別途渡すだけのworker cloneでもsimulation stateの所有権を分離できない。既存 `ArtifactRenderQueueService::cloneCompositionSnapshot()` はJSON round-tripであり、毎frame snapshotには使わない。今回はこの監査により直接frame dispatchを次の実装手段とせず、scene snapshot・simulation state・GPU owner boundaryを別契約として先に成立させる必要があると確認した。実際のworker-owned Composition frameは未実装。

**Deferred offscreen Billboard → immediate 3D barrier（2026-10-17、target image確認）:** RGBA8 sRGB offscreen RTV と D32 DSV を `pushRenderTarget()` で有効にし、15個の filler + retained `BillboardPkt` をworkerにrecordした後、`draw3DCard()` の深度付きimmediate drawを続けるproduction renderer testを追加した。同期owner描画とworker描画のoffscreen imageを全画素比較（RGBA各channel 1/255以内）し、centerのblue card pixelも確認した。Deferred 10ケースすべてを独立プロセスでD3D12 / Vulkan各レーン実行し、各起動時に実デバイスtypeを確認してpassした。Vulkanは1プロセス内で複数headless deviceを作ろうとするとDiligentのglobal Vulkan function pointer制限に当たり、`initializeHeadless()` がD3D12 fallbackを許すため、一括実行はVulkan証拠として扱わない。`artifact_renderer_deferred_billboard_offscreen_{d3d12,vulkan}.png` はSHA-256 `C189DB4BA742C183948DE757721FDBCC44B28C8248975D31ECBF69A3195FFC1B` で一致した。worker submit countはasync recording開始時ではなく、barrierでcommand listをexecute/releaseした後に増えるため、テストでは後続3D draw後に確認する。これはoffscreen color/depth target保持とpacket順序の実画像検証であり、一般のGUI latency改善やComposition内offscreen workload頻度を示さない。

**Composition async worker の再検証（2026-10-18、Debug / D3D12・Vulkan）:** Ninja/MSVC `Artifact` target は639/642 taskまで進み、`Artifact.exe` link が成功した。CMakeのglob再確認で全module依存scanが再生成された後の実行であり、旧10/13ログの `RenderGraph::validateCompiledPassOrder` unresolved externalは現在の `RenderGraph.execute()` に同じ呼び出しがなく再現しなかった。Composition point-light integration testをCTest既定のVulkan設定と、`windows` QPA plugin / D3D12明示設定の個別プロセスで実行し、両方passした。テストはproduction frame中の `deferredWorkerSubmitCount` 増加をassertし、両 backend の400×300 PNGを出力した。今回の両PNG SHA-256は `AC7C475B62E440744DFF4D9F7D69ED63C38BC71C0020E1A67BC11254AA18754` で一致し、D3D12画像も目視確認した。Ninja app link後の `windeployqt` はQt translation catalogとDebug DLL探索パスのwarningを出したが、build exit codeは0。コンパイル時には複数の `C5202`（global module fragment内の非preprocessor行）と `C4715`（`ArtifactMainWindow::closeDock` の戻り値欠落）warningが残る。実行時テストの手動起動では誤った `offscreen` plugin pathを指定して停止したが、CTestのWindows向け `windows` plugin設定と同じ条件では成功した。これはこのCompositionシーンでworker submitが選ばれ実フレームが生成された証拠で、Composition全体のGUI thread非ブロッキングを示すものではない。

**Composition Renderer façade の小段階（2026-10-18、Debug / Vulkan）:** Viewport OCIO LUTのbake/cache責務を `ViewportColorPipeline` に残し、GPU LUT uploadとdisplay-transform dispatchを `ArtifactIRenderer` のAPIへ移した。Composition controllerはこの処理で `immediateContext()` を取得・保持せず、rendererがdevice managerのcontextを内部使用する。CPU LUTはrendererが所有し、renderer device再初期化時にGPU LUTを遅延再uploadする。`Artifact` targetのビルドと `ArtifactModel3DLightCompositionPointIntegrationTest` が成功し、CTest実行でDeferred worker submit assertionを通過、新しい400×300 PNGフレームを生成した（SHA-256 `AC7C475B62E440744DFF4D9F7D69ED63C38BC71C0020E1A67BC11254AA18754`）。画像は目視確認済み。テストシーンではOCIO configが有効でないため、active LUTでの色変換画質・device reset後の再uploadは未検証。これはcontrollerからGPU contextアクセスを減らす小段階であり、共有immediate contextの所有threadやComposition frame全体はまだ移行していない。
