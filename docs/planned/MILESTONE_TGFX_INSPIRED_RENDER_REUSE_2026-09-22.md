# TGFX-inspired Render Reuse / Damage Scheduling

**最終更新:** 2026-09-26
**ステータス:** In Progress

## 目的

Tencent TGFX を依存ライブラリとして導入せず、TGFX 2.1 系で確認できる
DisplayList、dirty region、tiled rendering、frame-based resource expiration の設計を、
ArtifactStudio の既存 Diligent / RenderGraph / GPU texture cache 境界へ移植する。

Donor reference: Tencent TGFX (`https://github.com/Tencent/tgfx`), BSD-3-Clause.
TGFX 2.1.1 の [release notes](https://github.com/Tencent/tgfx/releases) を確認し、2.1.0で導入された
partial refresh、tiled rendering、dirty marking、image downscaling cache、
resource expirationを設計上の参考とする。2.1.1で追加されたvisible-area外のlayer draw cullingと
tile優先順位の改善も候補として扱うが、ArtifactではDiligent共通経路と既存quality契約を保ち、
実測なしに採用しない。donor codeはコピーしない。

## 守る境界

- `ArtifactIRenderer -> Diligent -> D3D12 / Vulkan` を維持する。
- TGFX の Canvas / Surface / Context / backend 実装を持ち込まない。
- `QImage`、CPU readback、GPU upload をホットパスへ新規に暗黙追加しない。
- resource expiration と tile refinement は bounded とし、枯渇時に one-shot resource を生成しない。
- D3D12 / Vulkan で同じ公開契約を使い、backend 固有処理を上位へ漏らさない。

## 現状

- `RenderCommandBuffer` は描画 packet をフレーム単位で記録する。
- `GPUTextureCacheManager` は owner / generation / byte budget / entry budget / LRU を持つ。
- `ArtifactRenderROI` に `DirtyRegionAccumulator`、`RenderDamageTracker`、`TileGrid` が存在する。
- Composition View は ROI による画面外 layer skip と composition-space GPU cache の基礎を持つ。
- damage tracker は layer invalidation を受け取り、成功したpresent後にslot単位で消費される。
  TGFX-RR2b/RR3のopt-in経路では、固定上限8 tileの同一row planを既存retained textureのROI再合成に使う。
  独立tile targetや近傍LOD保持は未実装で、pixel parity・性能・backend parityも未計測。

## 実装段階

### TGFX-RR1: Frame-based GPU resource expiration

- cache entry に最終使用 frame を保持する。
- `beginFrame(frameIndex)` で cache のフレーム境界を明示する。
- 未使用期間が閾値を超えた entry を1フレーム当たり固定数だけ破棄する。
- expiration、budget eviction、明示 invalidation を診断上区別する。

完了条件:

- cache hit で最終使用 frame が更新される。
- expiration scan は1フレーム当たりの最大破棄数を超えない。
- 既定値0でexpirationを無効化できる。
- device reset後に古いframe ageを引き継がない。

### TGFX-RR2: Damage lifecycle

- layer変更時に旧boundsと新boundsの和をdamageとして記録する。
- full-frame effect、composition構造変更、camera/3D変更はfull redrawへ昇格する。
- 描画成功後にだけdamageを消費する。
- dirty rect、full-redraw理由、dirty tile数を遅延評価の診断へ出す。

完了条件:

- layer単位invalidatonが直後のbase invalidationで消失しない。
- transform変更で旧位置に残像が残らない契約を持つ。
- 失敗・早期returnしたframeでdamageを誤消費しない。

### TGFX-RR3: Bounded tile refinement

- 大型2D compositionに限定して`TileGrid`から可視dirty tileを列挙する。
- current visible frameを最優先し、1フレーム当たりのrefine数に上限を持たせる。
- zoom変更時は利用可能な近傍LODを表示し、stale generationはpublishしない。
- mask / matte / adjustment / 3D / full-frame effectは初期対象外とする。

完了条件:

- pan / zoom中に画面外tileを生成しない。
- queue depth、refined、deferred、dropped、stale rejectionを計測できる。
- queue上限超過時はcoalesceまたはdeferし、無制限確保しない。

### TGFX-RR4: Retained layer draw records

- layer content revision、draw bounds、resource generationを持つ再利用可能な描画記録を定義する。
- content不変時はpacket再構築を避け、transform / opacityだけを再評価する。
- raw GPU pointerの寿命をcache handle / generationで検証する。

完了条件:

- content revision不変の静止layerでpacket再構築数が減る。
- device reset、cache eviction、layer削除後にstale resourceをsubmitしない。

## 実装順

RR1、RR2a/RR2bの静的実装とRR3のbounded ROI接続は済んでいる。次はRR0の比較基準を固定し、
RR2b/RR3の静的な境界・失敗経路を詰めてからRR4へ進む。RR1〜RR3の実機受入はRR5で確認する。

### Bounds culling follow-up

- `effectExpandedLayerBounds()`は`enabledRasterizerOverscanPixels()`を呼び、従来は呼び出しごとにlayer effect listを走査していた。
- 非アニメーションeffectのoverscan値を`effectRevision()`単位でlayer内キャッシュし、keyframe/expression/envelope/modulationを持つeffectは毎回評価する。
- `setDirty(LayerDirtyFlag::Effect)`がrevisionを更新する経路を保つ。enabled-state、undo/redo、effect property変更後に値を再計算することを静的に追跡し、runtime parityとCPU計測はRR5で受け入れる。

## 対象ファイル

Artifact:

- `Artifact/include/Render/GPUTextureCacheManager.ixx`
- `Artifact/src/Render/GPUTextureCacheManager.cppm`
- `Artifact/include/Render/ArtifactRenderROI.ixx`
- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/include/Render/RenderCommandBuffer.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

RR2bで必要性を確認するArtifactCore境界:

- `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx`
- `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`
- `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`

## 検証

静的確認:

- module purview内に`#include`を追加しない。
- texture cache APIがDiligent backend型以外のD3D12/Vulkan詳細を公開しない。
- expiration処理がboundedであり、通常frameに同期waitやresource生成を追加しない。
- 変更文書の日付監査を通す。

実行確認（ユーザー許可後）:

- D3D12とVulkanで静止画／連番画像／シェイプの表示が一致する。
- cache hit / expired eviction / budget evictionが期待通り増える。
- pan、zoom、scrub、長時間idle後の再描画でstale textureや欠落がない。
- dirty regionとtile refinement導入後、局所編集でfull composite回数が減る。

## 現在の状態（2026-09-26 ソース静的照合）

- TGFX-RR1: 静的実装済み。`beginFrame()`、frame age、最大8件のbounded expiration、upload処理を同じframe indexで1回に制限するgate、累積とframe単位のeviction診断値を追加した。expirationはentryを1回走査して最古の期限切れを固定容量配列へ最大8件選び、その後に削除する。stats APIとDebugger resource noteにexplicit、owner change、budget eviction、device reset、clear-allの累積数を表示し、expirationは既存の累積／frame単位カウンタで区別できる。pending upload数／bytesはcache側の未完了ticket集合から同時に算出し、GPU転送中もowner別と全体で会計範囲を一致させる。owner statsはQSetをコピーせず参照走査する。importしたVulkan frameは外部decoder所有でbyte予算に含めず、全体／owner statsに件数を別表示する。`invalidateOwner()`はentry／pending ticketの一時コピーを作らずerase-return iterator traversalし、古いgenerationのhandleをinvalidateしても実際の削除がない限り統計を増やさない。upload coordinatorのcancelはqueue中ticketを即時除去し、完了済みticketはresultを回収、処理中ticketは完了時にresultを公開せず破棄する。generationが古くなったin-flight uploadも同じ経路で抑止し、owner invalidation後にorphan resultが蓄積しないようにした。pending uploadの完了確認と、asset version更新時の古いpending upload取消も`QHash::keys()`／一時`QList`を作らないiterator traversalにした。`findExisting()`はowner + canonical cacheKey indexで候補entryへ直接到達し、期待formatも確認する。F32画像のcanonical keyは登録／検索で同じcolor descriptor helperを通す。現行のF32 image／sequence／SVG lookupは`RGBA32_FLOAT`を指定する。Vulkan frameを含む全entryのindex keyもformat別にし、同一source keyでもpixel formatの異なるframeを混同しない。連番画像の別frame entryは走査しない。`findExisting()`経由のcache hitも統計へ反映する。Assetのstale purgeはsource versionとcolor-space/transfer variantを分け、同一sourceの複数解釈を共存させる。同一owner・versionのpurge走査はversion遷移時だけ実行し、asset entry/pendingが消えたownerのversion記録を掃除する。ビルド／D3D12・Vulkan実機確認待ち。
- `tryAcquireExistingLocked()` cache-hit follow-up: 既存のowner/cache-key indexからformat一致entryを直接探し、hit時のcomposite `QString` key構築を回避する。pending判定とmiss処理は従来のfull-key indexを使う。variant別hit、QString allocation、lock時間のruntime計測待ち。
- F32 color-key follow-up: `colorAwareImageCacheKey()` now reserves one output QString and appends the seven signed numeric descriptor fields with a stack digit buffer instead of chaining `QString::arg()` intermediates. Signed append preserves invalid/negative enum values as the prior formatter did. Generated-key parity across all descriptor values and allocations/frame remain unverified.
- RR1 scan-bound check: the app configures `GPUTextureCacheManager::setMaxEntries(256)`, and `pruneExpiredLocked()` visits the entry map once while retaining at most eight eviction candidates in fixed arrays. Thus the configured app path bounds expiration scan to 256 entries and evictions to eight per processed frame; the public setter itself accepts larger values, so standalone users can choose a higher scan bound.
- Upload miss contention note: `acquireOrCreateFromRgbaBytes()` copies the full payload and enqueues under the cache mutex. Viewport cache access runs from the controller QObject thread and Render Queue keeps cache managers worker-local, but synchronous EventBus publication can call `LayerChangedEvent` invalidation from another publisher thread. The mutex therefore serializes owner invalidation with cache lookup/upload; moving bytes outside it requires an invalidation-safe bounded reservation or per-owner generation, and must prevent duplicate payload copies. No change until caller overlap and lock time are measured.
- TGFX-RR2: lifecycle foundation実装済み。旧／新boundsの和、未知／非finite boundsのfull-frame昇格、debug snapshotに加え、2つのpreview RenderPipeline slotごとにdamageとretained composite有効状態を保持する。slotの保持結果にはcomposition/frame、target寸法、accumulator texture、downsample、canvas寸法、zoom/panを記録し、不一致時はslotをfull redrawへ戻す。成功present後は対象slotのdamageだけを消費し、全slotが追いつくまで集約damageを残す。RenderGraph全passと`ok` present statusも消費条件とする。pass/present失敗時は全slotの保持状態を無効化し、render keyをresetしてfull redrawを再要求する。idle中の失敗でも1回だけ自動retryし、連続失敗でretry loopにならないようguardする。ビルド／実機確認待ち。
- TGFX-RR2b: `ArtifactCore::ComputeRegion`と、Normal blend、layer-to-float、transparent clearのROI dispatch APIを追加し、Artifact renderer wrapperから呼べるようにした。さらにDiligent composition経路へ実験的partial recomposeを接続した。`Render/Experimental/TGFXPartialRecompose`設定（既定false）を有効にした場合のみ、front orthographicの単純なNormal blend静止画／solid／shape構成で、slotごとのretained結果と局所damageが有効なときROI clear、交差layerのz順再合成、blend/convert ROI dispatchを行う。ROIは1px拡張し、そのrender-target範囲をcanvas座標へ戻してlayer cullingにも使う。targetの75%以上ならfull redrawへ戻す。初回、frame/viewport/resource不一致、無効bounds、3D、effect、mask、matte、modifier、overlay、channel表示等は対象外。partial処理中の失敗はpresentを止め、次回full redrawへ戻す。Debug logging有効時、candidate/start/failure/commit、ROI pixel数、skip layer数を記録する。非交差layerのraster描画はskipするが、交差layerのoffscreen rasterization自体はfull targetのまま。描画速度やpixel parityは未計測。ビルド／shader compile／D3D12・Vulkan確認待ち。
- TGFX-RR3: allocation-freeなcoverage countと、可視dirty tileを最大8件選ぶ固定容量planを実装済み。planは同一dirty rectangleの連続する1 rowを選び、global grid上のcursorから始める。preview pipeline slotごとにcursorを保持し、成功したpartial passだけが選択範囲をconsumeして次batchへ進む。失敗・full redrawではcursorをresetする。Debuggerの`planRemainder`は選択中のdamage rectangle内でのdeferred count、`tiles`はdamage全体の外接範囲を表す。描画対象は独立tile targetではなく、既存retained composition textureのROIを再合成する。unbounded vectorを返すtile enumeration APIに加え、未使用の`SparseTileSurface`（tileごとのheap確保）と`TileRenderScheduler`（無制限queue・callback保持）を撤去し、tile列挙を固定容量plan経由に限定した。ROIとtileの交差判定は半開区間で行い、タイル境界ぴったりの右端／下端で隣tileを誤計上しない。無効な画像／tile寸法ではgridをinvalidのままにし、ゼロ除算とceil除算の加算overflowを避ける。coverage countは大きなgridでもint上限に飽和させる。空boundsのinvalidationsは`RenderDamageTracker::markDirty()`で無視し、damageなしのlayer entryが診断に現れないようにした。`DirtyRegionAccumulator`は固定容量16個の非重複矩形を保持し、描画済みrectのsubtract APIを追加した。上限時は外接rectへcoalesceしてdamageを保守的に残す。通常のlayer traversalには`roiRect`によるvisible-area cullingがあり、opt-in partial recomposeでは`gpuDamageCanvasRect`との交差も検査するため、このculling自体を重複実装しない。描画・presentの失敗経路はfull redrawへ戻る。継続batchのtick予約、render-key早期returnの可視damage例外、可視damageがあるslotの全体確認を静的に接続した。pan／zoomでretained stateが不一致なら選択slotのdamageをfull redrawへ昇格する。pixel parity、frame time、8 tile超の完了挙動は未実測。
- TGFX-RR4: `RenderCommandBuffer`はframe単位のpacket配列で、`reset()`時に消去する。retained draw recordは未実装。
- RR4/packet storage note: `reset()` destroys packet payloads and texture pins but preserves `std::vector<DrawPacket>` capacity. This avoids steady-frame growth after the high-water packet count, but a one-off large frame retains that capacity until renderer destruction. Do not shrink per frame; first measure packet count/capacity and verify the submit/reset boundary before considering hysteretic cold-boundary trimming.
- Diligent timeline glyph draw: `drawGlyphText()`の`UniString::toStdU32String()`を、implicitly-shared `QString`の直接UTF-16 decodeと既存renderer-owned scratchで置き換えた。constructorで予約済みのscratch capacityを維持し、入力文字列長に応じた毎call reserveは行わない。scratchのcold growthやglyph cache missを含むallocation/per-frame CPU効果は未計測。
- F32 cache hit: `CompositionViewDrawing`の静止画／連番／SVG描画は`findExisting()`と`acquireOrCreate()`の二重lookupをやめ、canonical keyとformatを内包する`acquireOrCreate()`を1回だけ呼ぶ。Debuggerの候補確認はdescriptor一致を検査する`findExisting()` overloadを使う。
- 静止画／連番画像／SVG／shapeのrasterizer effect/mask経路は、更新済みcrop状態、source寸法、入力経路に対応したsurface寸法、color pipeline、mask／effect revision、scene-light lift、連番のresolved frame index/content keyから既存surface cache keyを作り、処理済みbuffer/surface/valid textureを`toQImage()`前に再利用する。静止画／連番画像／shapeは既存経路どおりLOD縮小後の寸法、SVGは既存経路どおりsource寸法を使う。連番寸法はsequence loaderが全frameで一致を検証済みの`sourceSize()`を使い、resolved frame indexが無効なら従来経路へ戻る。SVGはloaded sourceのversionと`sourceSize()`で照合する。Shape は `contentRevision()` を cache key に含め、既存 `markDirty()` と path-keyframe 編集時に更新する。effect stack/property、enable、Undo／Redo時のmodulationとeffect-mask changes、および layer effect JSON restore は`effectRevision()`を通じてkeyを更新する。 Effect presetは呼び出し側で先に適用してから記録するため、snapshot commandの初回redoでも所有layerのeffect revisionを更新する。animated effect propertyのframe-scope判定はrevision-keyed atomic cacheに保持し、steady frameではeffect/property snapshotを走査しない。static layer cacheの全entry trimはhit pathから除き、insert後だけ実行する。byte totalはentryの追加／置換／evictionで差分会計する（上限128 entry／512 MiBは維持）。`AbstractProperty::hasKeyFrames()`と`keyFrameCount()`はcache key・opacity・Render Controller・property評価・Undo検証等の有無／件数照会でkeyframe vectorのコピーを避ける。実行時効果は未計測。 Solid／crop／source-timeの数値cache identityはfloat 9桁、double 17桁の有効数字で構成し、丸めによる異なる値の誤aliasを避ける。legacy shapeはparametric width/height、shape contents stackは`localBounds()`から求めた寸法と既存の64Mpx上限縮小を使う。静的Textもanimator／source-text keyframeとscene lightがない場合に限りF32処理結果をcacheし、後続frameで再利用する。 surface cache miss時は既存move constructorで処理済みF32 bufferをcacheへ移し、全画像コピーを避ける。無効bounds、enabled external matte、cache miss、source buffer不在時は従来経路を使う。effect/mask key invalidation、temporary source override、device resetの動的parityとRender Queue上の効果は未確認。

静的根拠: `GPUTextureCacheManager::beginFrame()`、`RenderDamageTracker::makeBoundedDirtyTilePlan()`、
`CompositionRenderController::Impl::recordLayerDamage()`、slotごとのpresent後damage消費、
`RenderCommandBuffer::reset()`。性能向上、pixel parity、backend parityは未計測。

## 次の実装単位と依存順

| 段階 | 実装内容 | その段階の完了条件 |
| --- | --- | --- |
| RR0 基準固定 | 既存のfull frame経路で静止画、連番、シェイプ、pan/zoom、scrubの描画回数・GPU時間・upload/readback量を記録。damageとIRRの所有範囲を固定し、viewport culling前後のeffect-expanded bounds評価数とeffect-list走査数、`RenderCommandBuffer`のpacket count/capacityも記録 | 同じsceneで後続段階と比較できる診断項目、full redraw理由、packet配列のsteady/high-water容量、正解frameの取得手順が決まる |
| RR2a damage保持 | 結果targetごとに未処理damageとcomposition/frame/device generationを保持。描画成功した領域だけを消費し、未処理領域を維持 | 8 tileを超えるdamage、present失敗、途中return、旧位置と新位置で欠落や残像がない |
| RR2b GPU局所再合成 | Diligent経路のcomposition-space結果targetを再利用し、dirty領域をclearして、その領域と交差する全有効layerを既存z順で合成。GPU blend/convertも対象矩形へ制限 | 実験経路は静的接続済み。ビルド後、非変更領域の保持、画像一致、処理pixel数、full composite回数を確認 |
| RR3 実tile描画 | 可視dirty tileを固定上限で描画し、残りを次frameへ公平に持ち越す。poolとgenerationでtargetを管理 | 画面外tileを描かず、遅延tileが最終的に完成し、古いframe/device/compositionの結果を表示しない |
| RR4 描画記録再利用 | layer content revisionとresource handle/generationを持つ論理的なdraw recordを導入。frame単位の`RenderCommandBuffer`へ有効なpacketだけ再投入 | 静止layerのpacket再構築が減り、eviction・device reset・layer削除でstale pointerをsubmitしない |
| RR5 受入 | D3D12/Vulkanで代表sceneを比較し、保存・再読込後のpreviewとRender Queueを照合 | 各段階の正しさと効果を数値・画像で確認してから段階を完了扱いにする |

### RR2a: damageの消費単位

- `RenderDamageTracker`の現在の和集合とfull redraw昇格を維持する。
  transform変更は旧boundsと新bounds、blur/glow等はeffect拡張後のboundsを含める。
- 現在の「present処理後に全damageを`clearAll()`」は、計画だけを出す段階では診断境界だが、
  partial描画を有効にした後は描画完了領域だけを消費する契約へ変更する。
  deferred tileは再試行対象として残し、毎frame先頭の8件だけを繰り返さない。
- 初回frame、frame変更、composition構造変更、viewport/canvas寸法またはdownsample変更、
  device reset、cache eviction、camera/3D、
  全画面effectや影響範囲不明のeffectはfull redrawとする。
  連番はframe変更時にsourceが変わるため、時間依存の安全な範囲判定ができるまでfull redrawとする。
- mask、matte、adjustment、非Normal blendは各経路のROIとalpha境界が確認できるまで
  full redrawへ昇格し、近似描画には切り替えない。
- `DESIGN_INTERACTIVE_RENDER_REGION_2026-09-22.md`の手動IRRは別の表示要求である。
  IRR内を描いても外側のpending damageを消費しない。両者を併用する時のclip、
  結果targetの所有者、frame完了判定をRR2aで同文書と整合させる。

### RR2b: 主GPU経路への接続

- 最初の実行対象は、front viewでの単純な2D構成（Solid、静止画、Shape、Normal blend）。Shapeはmodifier・mask・effectなしに限定する。
  既存`compositionSpaceGpuCache`はopt-inで`!pipelineEnabled`に限られ、Shapeと連番も対象外。
  このfallback側だけを完成として扱わず、Diligentの通常GPU合成経路を最終対象にする。
- 実装境界の静的確認: `ArtifactIRenderer::setViewportRect(x,y,w,h,targetW,targetH)`は
  Diligent viewportとscissorを設定するが、compute dispatchには作用しない。
  `ArtifactCore::LayerBlendPipeline::applyPointwise()`は全texture寸法でdispatchし、
  Layer Blend shaderはdispatch idをtexture座標として直接読む。dispatchのorigin/extentと
  dispatch外pixel保持の契約がないため、scissor追加だけでは局所再合成にならない。
- Diligent raster scissorはPSOの`RasterizerDesc.ScissorEnable`も必要。
  Diligentの`RasterizerStateDesc`既定値は`False`。現在のArtifact側でこのflagを設定しているのは
  `ArtifactDiligentEngineRenderWindow.cppm`のPSOだけ。通常のSolid/Gradientや
  `DiligentBindlessSubmitter`のSprite PSOは既定値を使っているため、2D layer rasterで
  context scissorが有効とは確認できない。`ArtifactRenderLayerPipeline::renderComposition()`のscissor設定も、
  clear後に実際のlayer drawを行わないstubである。RR2bの交差layer rasterをROIへ
  クリップするには、使用する全PSOを特定しscissor有効化を個別確認する必要がある。
- したがってcompute側にはdestination座標上のROIを渡し、bounds check、dispatch group数、
  shaderの読み書き座標を揃える。dirty部分を透明へ戻す処理も必要で、全体clearをROI scissorで
  置き換えたと仮定しない。既存`clearRenderTarget()`はtexture viewに対する全体clearを呼ぶ。
  blend、layer-to-float、matte、pointwise effectの各passが
  ROI内だけを書き、外側を保持すると確認できるまではRR2bを有効化しない。
- `ArtifactIRenderer::copyTextureOutsideRegion()`はROIを除く上下左右の帯を固定回数の
  `CopyTexture`コマンドで複製する。これはping-pong先の既存pixel保持用primitiveであり、
  `LayerBlendPipeline::clearRegion()`は透明clear用computeである。両者とも呼び出し側の
  dirty ROI再合成と全layerのz順再合成へ実験設定下で接続した。GPU結果検証は未実施。
- `LayerBlendPipeline::blend()`はsrc/dst/outputを別textureにする契約で、通常GPU pathは
  accum/tempをping-pongする。dispatchをROIへ狭めるだけだと、出力先のROI外に前frameの
  有効結果が残る保証がない。RR2bでROI外を保つcopy/seed境界を定義し、tileごとの独立targetを
  選ぶ場合はRR3のpool予算・寿命とまとめて設計する。
- 最終結果をcomposition座標で保持し、pan/zoomのみの変更ではその結果を表示変換する。
  content、frame、color interpretation、output size、device generationの変更は
  対応するcache keyまたはfull redrawへ反映する。
- dirty矩形では最終色の上書きだけをしない。透明に戻して、交差する全layerを既存の
  z順・opacity・blend契約で再合成する。2D drawのscissorとcompute passのdispatch範囲を
  ともに制限し、dispatch外のpixelを保持する。
- resize、composition切替、失敗frame、resource不足では既存のfull frame GPU経路へ
  明示的に戻す。resource不足を理由にframe中の臨時textureを生成しない。

### RR3: 上限付きtile実行

- 既存`TileGrid::makeBoundedPlan()`の固定容量8件を起点にする。
  schedulerはvisible ROIとpending damageの積集合から選び、同じ先頭tileへ偏らない
  cursorまたは同等の公平性を持つ。非可視tileは描画せずpendingとして残す。
- Diligent partial recomposeでは既存retained composition textureのROIを再利用し、
  独立tile targetを増やさない。scratchとdescriptorは既存pipelineの寿命に従い、
  描画中の容量不足ではdefer/coalesceする。`SparseTileSurface::ensureTile()`の
  `new std::vector`はホットパスで使用しない。
- zoom中は有効な既存結果を表示し、要求世代とframeが一致するtileだけをpublishする。
  tilesの境界、fractional zoom、画像alpha端、Shape strokeのはみ出しは比較画像で確認する。

### RR4: 描画記録の寿命

- `RenderCommandBuffer`はframeごとのsubmit bufferとして使い続ける。
  retained recordにはlayer ID、content revision、draw bounds、安定したcache handle、
  resource generationと必要なパラメータを保持する。raw `ITextureView*`を寿命を超えて所有しない。
- transform/opacityだけの変更で再利用できるpacketと、Shape頂点・画像source・effect変更で
  再構築するpacketを分ける。layer削除、Undo/Redo、cache eviction、device resetで失効させる。
- 現行の`DrawPacket`はsubmit用であり、layer ID/revision/generationを含まず、変換済み行列・borrowed texture pointerとframe内pin・QString/QFont・Particle data等を同じvariantへ格納する。そのままretained化しない。先にRR0で静止frameのpacket再構築数とCPU時間を測り、layer draw境界でimmutable content recordとframeごとのtransform/material bindingを分離できるか確認する。最初の対象は安定identityとgenerationを与えられる少数の静止画／単純Shapeに限定し、Text、Particle、Billboard、temporary source/masked textureは別途寿命契約ができるまで除外する。
- `GPUTextureCacheHandle`はIDとgenerationを持ち、`isValid()`と`textureView()`は世代不一致を拒否する。ただし`textureView()`はmutex解放後にraw pointerを返す。retained recordからframe packetへ戻す境界では、generation確認と`RefCntAutoPtr` pin取得を同一寿命操作として提供できるAPIが必要。現行のlayer draw loopは個別layerのROI／opacity判定後に`drawLayerForCompositionView()`を呼ぶが、その内部は複数primitive packetをemitし得るため、record単位と描画順の一致を確認してから切り出す。
- QImageの`PrimitiveRenderer2D` cache keyは`QImage::cacheKey()`へ変更した。Qtの内容IDは編集で変わり、暗黙共有コピーでは保持される。ImageF32ではsource UUID/version、連番frame content key、寸法、color descriptorから作るtexture key overloadを追加し、ImageLayer、Composition draw、SVG、Puppetの安定source経路で利用する。Textの非加工raster fallbackはlayer UUID、content revision、layer current frameを使う。`setCurrentFrame()`の実装はcomposition frameをinPoint/startTimeで一定オフセットするため、テキスト評価のcomposition frameと1対1で進む。Videoは`cachedFrameImageBuffer()`／`isFrameCached()`がsource versionのrefresh前にcacheを照会し得るため、Asset UUID/version/frameだけではstale frameを区別できず、今回はsampled keyを維持する。F32の一時source fallbackは従来の先頭4 KiB＋末尾byteによる32-bit keyから、最大4096 evenly spaced bytesを使う64-bit bounded fingerprintへ変更した。これはaliasを減らすがexact identityではない。UV矩形付きtexture-view描画を追加し、既存のUVなし描画packetは維持する。temporary source override、加工済みの一時ImageF32、masked texture pathは引き続きsampled fingerprintを使う。ビルドとGPU実機でkey collision、crop parity、再読込後の差し替え、texture cache共有を確認する。

## 受入シーンと観測値

| シーン・操作 | 正しさ | 効果・診断 |
| --- | --- | --- |
| 静止画・Solid・単純Shapeの局所移動、Undo/Redo | 旧位置に残像なし。full frame基準と同じ色・alpha | dirty矩形、合成pixel数、GPU pass数、cache hit |
| 連番のframe advance、scrub、ループ | 各frameのsourceが正しく、旧frame tileを表示しない | full redraw昇格理由、stale拒否数 |
| pan/zoomとviewport resize | 座標・境界・strokeが一致し、resize後に古いtargetを使わない | 再合成回数、tile生成数、GPU時間 |
| 8 tile超のdamage、長時間連続編集 | deferred分が完成し、キューが無制限に伸びない | scheduled/deferred/drop、queue depth、allocation数 |
| mask/matte/adjustment/effect/3D、device reset | 対象外sceneは正しいfull redrawへ戻る | 昇格理由、resource generation、fallback回数 |

受入時は同一scene・解像度・backendで既存full frame結果を基準画像とし、
previewとRender Queueの結果、各channelの差分画像を残す。性能は同条件の
GPU描画時間と処理pixel数で判定し、present/vsync待ちを描画時間と混同しない。
ビルド、テスト、実機確認はAGENTS.mdの指示どおりユーザーの明示指示後に実施する。

## 実装上の境界と関連文書

- 新規`QImage`、Qt合成、新規signal/slot、ソフトレンダラー拡張はこのマイルストーンに含めない。
- ホットパスの容量、同期待機、診断は`docs/technical/HOT_PATH_RULES.md`を優先する。
- 手動IRRとの重複と優先順位は`docs/planned/DESIGN_INTERACTIVE_RENDER_REGION_2026-09-22.md`と照合する。
- full frame baselineとボトルネックは`docs/analysis/COMPOSITION_VIEWPORT_PERFORMANCE_AUDIT_2026-09-09.md`を参照する。
- 作業ファイルは上記の対象ファイルを優先し、`ArtifactCore`やDiligent forkへの変更が必要になった場合は親側の代替を先に確認する。


## 2026-09-26 — Effect envelope invalidation and frame identity

- **確認できた事実:** `LayerEffectEnvelope` is sampled while building rasterizer effect context. `setEffectEnvelope()` previously marked only `Property`, so `effectRevision()` did not advance. Both Composition View and Render Controller surface keys included a frame only for animated effect properties; an enabled layer envelope with effects could therefore reuse a surface rendered at another frame. Render Controller also rebuilt an effect/property snapshot and scanned it on each key construction despite the existing revision-keyed animation query.
- **対応:** Envelope edits now dirty both `Property` and `Effect` and record both dirty reasons. Both surface-key builders include the requested frame, layer-relative frame, and active composition frame when animated effect properties exist or when an enabled envelope and at least one effect are present. Render Controller now uses `hasAnimatedEffectProperties()` like Composition View, reusing the revision-keyed result.
- **価値または懸念（未検証）:** This closes stale-surface routes when envelope-driven or animated effect evaluation sees different requested, layer-relative, or composition frames, and removes repeated effect-list/property snapshots from the Render Controller key path. The conservative effect-count condition may reduce reuse for disabled or non-rasterizer effects while an envelope is enabled. Pixel parity and cache behavior have not been runtime-tested.
- **次に確認すること:** After build/runtime authorization, scrub an enabled envelope with rasterizer effects, edit/undo the envelope, and compare both preview cache paths against a forced rebuild. Confirm opacity and effect dirty consumers respond correctly.


## 2026-09-26 — Render Controller effect revision in surface identity

- **確認できた事実:** The Composition View surface key includes both mask and effect revisions. The Render Controller key included mask revision and surface generation but omitted effect revision. Static effect edits therefore did not change that key unless another identity field happened to change.
- **対応:** Added `layer->effectRevision()` to the Render Controller surface identity, matching Composition View.
- **価値または懸念（未検証）:** Static effect property, enable, stage, preset, and envelope mutations that advance effect revision now invalidate the processed surface key. Runtime cache invalidation remains unverified.
- **次に確認すること:** After build/runtime authorization, render a cached static effect, edit its value and enabled state, then verify each next render misses/replaces the old surface and matches a forced rebuild; repeat through undo/redo.


## 2026-09-26 — Preserve precision in Render Controller keys

- **確認できた事実:** The Render Controller has an independent surface-key builder. It still serialized source time, blend amount, and stop-motion rate to three decimal places, Solid/SolidImage RGBA to four fixed decimal places, and their bounds to two places, allowing distinct values to alias in that cache.
- **対応:** Source time and stop-motion rate now use 17 significant digits; float blend amounts and colors use 9 significant digits; Solid/SolidImage bounds use 17 significant digits.
- **価値または懸念（未検証）:** Removes decimal-rounding collisions from these controller key fields. Longer strings can increase key formatting/comparison cost; output parity and performance are not runtime-measured.
- **次に確認すること:** After build/runtime authorization, test sub-precision changes in Solid colors/bounds and source-time mapping against forced rebuilds, then profile key construction.


## 2026-09-26 — Shape and SVG revisions in Render Controller keys

- **確認できた事実:** Composition View includes Shape `contentRevision()` and SVG `sourceVersion()` in its surface identity. The Render Controller already frame-scoped animated Shape properties but its Shape key omitted the content revision, and its SVG key omitted loaded-source version. Shape edits that did not alter dimensions/type and same-path SVG reloads could therefore retain an old surface key.
- **対応:** Added Shape content revision and SVG source version to the Render Controller key, matching Composition View's revision coverage.
- **価値または懸念（未検証）:** Invalidates cached surfaces after Shape content edits and SVG source reloads even when path/dimensions are unchanged. Runtime replacement and visual parity remain unverified.
- **次に確認すること:** After build/runtime authorization, edit Shape fill/path without changing bounds and reload an SVG at the same path; ensure each updates the controller cache and matches a forced rebuild.


## 2026-09-26 — Sequence and animated crop identity in Render Controller

- **確認できた事実:** Composition View keys include resolved sequence frame index/content identity and scope animated source-crop properties by frame. The Render Controller key omitted both. Its rasterizer/mask image path also called `toQImage()` without first resolving animated crop properties, unlike the direct draw path.
- **対応:** Added sequence index/content key and animated-crop frame to the controller surface identity. The image rasterizer/mask path now calls `refreshAnimatedSourceCrop()` before `toQImage()`.
- **価値または懸念（未検証）:** Prevents cross-frame reuse for sequence/crop changes and feeds the current crop state to effect processing. Frame resolution and image parity have not been runtime-tested.
- **次に確認すること:** After build/runtime authorization, scrub an image sequence with a rasterizer effect, animate crop/rotation, and compare cache hits/misses and output against Composition View and a forced rebuild.


## 2026-09-26 — Frame identity follows evaluated clocks

- **確認できた事実:** Effect keys now distinguish requested, layer-relative, and composition frames, but animated crop, Shape, Video, and Text keys still used only the requested frame. Their render paths can evaluate against layer-relative or active composition time, so explicit-frame/offline draws with a different active layer clock could alias.
- **対応:** Both surface-key builders now serialize requested frame, layer current frame, and active composition frame for animated crop, animated Shape, Video, and animated/source-keyed Text entries.
- **価値または懸念（未検証）:** Keeps cache identity aligned with the clocks available to these render paths. Entries may be less reusable when caller and active clocks differ even if final pixels happen to match; runtime behavior has not been measured.
- **次に確認すること:** After build/runtime authorization, render explicit frames while varying active layer/composition frame independently for crop, Shape, Video, and Text, and compare against forced rebuild output.


## 2026-09-26 — Solid gradient inputs in Render Controller keys

- **確認できた事実:** Solid2D and SolidImage controller keys recorded only base color and bounds. The processed source surface also depends on fill type, gradient endpoint colors, angle, reverse, center, scale, and offset. Keyframed gradients additionally vary by frame. Composition View already includes those values and animation state.
- **対応:** Added all gradient inputs at float/double round-trip precision and frame-scoped animated gradient entries to both controller solid keys.
- **価値または懸念（未検証）:** Prevents stale cached surfaces after gradient edits or scrubbing animated gradients. Extra key formatting/property checks occur only in these two solid branches; runtime cost and parity are not measured.
- **次に確認すること:** After build/runtime authorization, edit each gradient control and scrub animated gradient keys on Solid2D/SolidImage with effects or masks; compare cache output against forced rebuilds.


## 2026-09-26 — Unsupported surface sources bypass controller reuse

- **確認できた事実:** The Render Controller surface key builder returned a generic layer/effect identity for types without a dedicated source key. Precomposition output depends on child composition revision, sampled child frame, and instance overrides, none of which were represented. `applySurfaceAndDraw()` also appended render-mode suffixes unconditionally, so an empty/unsupported identity could not disable reuse.
- **対応:** Unsupported layer types now return an empty base identity. Render-mode, effect-scale, GPU-mode, overscan, and matte suffixes are appended only when a supported source identity exists; unsupported sources still render and apply effects/mattes, but skip retained surface caching.
- **価値または懸念（未検証）:** Prevents stale reuse for precomposition or future surface sources until their full source/instance identity is defined. Precomp effect/mask surfaces may recompute each frame, with additional render cost.
- **次に確認すること:** After build/runtime authorization, verify precomp output changes after child edits, child-frame advance, and exposed-property changes while its effect/mask output remains correct. Add a dedicated revision key before re-enabling reuse if the cost is material.


## 2026-09-26 — Gradient animation clocks aligned across cache paths

- **確認できた事実:** The Render Controller solid keys used requested, layer-relative, and composition frames for animated gradient parameters after the frame-identity update. Composition View still used only the requested frame in both Solid2D and SolidImage gradient branches.
- **対応:** Composition View gradient frame identity now includes all three clocks, matching Render Controller.
- **価値または懸念（未検証）:** Avoids a stale hit when explicit render frame and active layer/composition clocks differ. No extra work occurs for static gradients. Runtime rendering remains unverified.
- **次に確認すること:** With build/runtime authorization, render animated Solid2D/SolidImage gradients with intentionally mismatched requested/layer/composition frames and compare both cache paths to forced rebuilds.


## 2026-09-26 — Static Solid pointwise cache hits avoid stack reconstruction

- **確認できた事実:** `SolidPointwisePreviewCache::resolve()` copied the layer effect vector, validated it, built a pointwise stack, and serialized every Exposure parameter before checking whether the renderer-owned output texture was already reusable. For static effects, layer effect revision and opaque source RGBA fully identify the supported stack state. Layer effect envelopes are a separate dynamic input and must remain in the signature.
- **対応:** After validating the active Diligent device/context, static stacks now probe the bounded per-layer cache by source RGBA and effect revision before taking the effect snapshot or rebuilding the stack. Envelope-driven stacks bypass that shortcut and include sampled effect strength in their exact signature. Exact signature hits refresh the stored revision.
- **価値または懸念（未検証）:** Removes effect-vector snapshot, stack construction, and parameter formatting on steady static hits while preserving animated properties and effect-envelope identity. Runtime GPU ordering, cache-hit count, and output parity have not been measured.
- **次に確認すること:** After build/runtime authorization, compare static Exposure hit output to full stack dispatch, edit source color/effect properties, scrub animated Exposure and layer effect envelopes, and verify device/context replacement invalidates the bounded cache.


## 2026-09-26 — Effect modulation participates in animated cache state

- **確認できた事実:** `ArtifactAbstractEffect::setContext()` evaluates modulation assignments per composition frame, but `hasAnimatedEffectProperties()` only checked keyframes, expressions, and property envelopes. A modulated Exposure could therefore be classified static and pass the new pointwise-cache shortcut. The service path that applies modulation snapshots without Undo also did not advance the owning layer's effect revision.
- **対応:** Animated-effect detection now checks each editable property's modulation target and includes active modulation in the revision-cached result. Direct no-Undo layer modulation snapshot application now marks `Effect` dirty after verified restore. Undo-backed mutation already advances owner revisions through its command.
- **価値または懸念（未検証）:** Modulated effect values now use the dynamic signature path, and adding/removing a modulation target invalidates the cached classification. Runtime modulation output and fast-hit behavior remain unverified.
- **次に確認すること:** After build/runtime authorization, modulate Exposure with an LFO and a Macro, scrub frames, change/undo the assignment, and compare both pointwise and surface-cache output against full evaluation.


## 2026-09-26 — Text surface identity includes animated style properties

- **確認できた事実:** `ArtifactTextLayer::draw()` applies keyframes for 33 text style, layout, color, stroke, and shadow properties. The Render Controller surface key was frame-scoped only for source-text keyframes and animator stacks, so a text layer animated only through a style property could reuse a surface across frames.
- **対応:** Text drawing uses one static path list for its 33 evaluated style properties. Cache identity and Composition View's static raster cache use `ArtifactTextLayer::hasAnimatedTextProperties()`, which scans registered `text.*` properties under a single cache lock instead of acquiring the lock once per path. Render Controller and Composition View include requested, layer-relative, and active composition frame clocks when Text properties are keyed; the separate static raster cache bypasses reuse for them.
- **価値または懸念（未検証）:** Avoids stale glyph surfaces for font, layout, color, stroke, and shadow animation without adding per-frame collection allocation. The property list must stay aligned with `ArtifactTextLayer::draw()`; render parity is not runtime-verified.
- **次に確認すること:** After build/runtime authorization, scrub one Text layer animated only by font size, color, or shadow and compare cache output to a forced rebuild; verify static Text still reuses its surface.


## 2026-09-26 — Shape cache animation scan skips display sorting

- **確認できた事実:** Both Shape cache-key paths only test whether any property has keyframes, but called `PropertyGroup::sortedProperties()`. Its implementation copies the property vector and performs `std::stable_sort`; display order is irrelevant to this boolean scan.
- **対応:** Both scans now use `allProperties()`, which preserves insertion order and avoids the stable sort and its sorting workspace. The property pointer vector and `getLayerPropertyGroups()` result are still copied/constructed.
- **価値または懸念（未検証）:** Removes unnecessary display-order work from a repeated cache-key path while preserving the keyframe predicate. This does not eliminate collection allocation; doing so needs a Shape-owned animation summary or non-copying iteration API.
- **次に確認すること:** With profiling access, measure cache-key CPU and allocations on Shape-heavy compositions; then decide whether an Artifact-only Shape animation summary can cover dynamic content/operator properties without scanning property groups.


## 2026-09-26 — Shape cache animation scan avoids property-group construction

- **確認できた事実:** Shape properties are registered in `ArtifactAbstractLayer::Impl::propertyCache_` through `persistentLayerProperty()`. Both cache-key paths rebuilt all Shape property groups each time only to test for keyed `shape.*` properties. Dynamic content/operator paths are registered in the same cache when their editable property objects are created.
- **対応:** Added `hasCachedAnimatedPropertiesWithPrefix()` to scan the existing property cache under its mutex, then changed both Shape keys to use it with the `shape.` prefix while retaining the explicit path-keyframe check. It does not create property groups, vectors, or sort work.
- **価値または懸念（未検証）:** Removes repeated Shape group/property collection construction from these cache-key paths and includes dynamic Shape paths already registered by editing/loading. It scans the cached property map on each key build; profiling must confirm that this bounded map traversal is cheaper under representative Shape workloads.
- **次に確認すること:** After build/runtime authorization, verify keyed width, fill, dynamic content, operator, and path animation against forced rebuilds; profile CPU time and allocations for static and animated Shapes.


## 2026-09-26 — Image source crop uses mutation revision in cache identity

- **確認できた事実:** Both surface cache keys formatted 12 crop values into a new `QString` per lookup. The ImageLayer cropped-QImage cache also rebuilt the same formatted signature to test for crop changes. All crop mutations flow through `SourceCrop` setters, `fromJson()`, or `clampToSource()`.
- **対応:** Added a monotonic `SourceCrop` revision advanced by those mutation paths. Composition View and Render Controller keys now store that revision, and the cropped-QImage cache compares it directly instead of formatting the crop signature. Both key builders detect animated crop properties through one `sourceCrop.*` property-cache scan rather than a repeated list of individual path lookups.
- **価値または懸念（未検証）:** Removes repeated multi-field string formatting from both render cache identity and the cropped-image cache check, while preserving explicit frame clocks for animated crop. The revision advances only when normalized crop state changes, allowing repeated evaluation of a held keyframe to reuse the same entry; runtime crop/keyframe parity is not verified.
- **次に確認すること:** After build/runtime authorization, edit and undo every Source Crop field, scrub animated crop, resize/relink the source, and verify both surface paths and cropped-image reuse against uncached rendering.


## 2026-09-26 — Video surface identity includes asset source version

- **確認できた事実:** The Video surface keys included path, frame clocks, proxy quality, and dimensions, but omitted the AssetManager source version. Replacing/relinking content behind the same asset identity could therefore leave a matching cached frame key.
- **対応:** Exposed `ArtifactVideoLayer::sourceVersion()` through its asset identity and included it in both Composition View and Render Controller Video surface keys.
- **価値または懸念（未検証）:** Prevents a stale surface hit after the video asset revision changes while preserving frame-aware identity. Asset replacement behavior and decode/cache invalidation remain runtime-unverified.
- **次に確認すること:** After build/runtime authorization, replace or relink the same video asset and verify both cache paths render the updated frame; compare with cache disabled.


## 2026-09-26 — Solid gradient animation detection uses one property-cache scan

- **確認できた事実:** Both cache-key builders checked eight gradient property paths separately to decide whether to include frame clocks, causing repeated property-cache locking for each Solid layer key.
- **対応:** Replaced the per-path loops in Composition View and Render Controller with one prefix query for registered `solid.gradient*` properties. Gradient parameter values and three-clock frame identity are unchanged.
- **価値または懸念（未検証）:** Reduces lock acquisitions for gradient animation detection and uses the same predicate in both paths. Map traversal cost and frame-key correctness remain unprofiled at runtime.
- **次に確認すること:** After build/runtime authorization, verify Solid2D/SolidImage static reuse and animated gradient scrubbing against forced rebuild output; profile property-cache scan cost.


## 2026-09-26 — Matte eligibility check avoids reference-vector copy

- **確認できた事実:** Composition View's rasterizer cache eligibility only needed to know whether a layer had an enabled external matte, but `matteReferences()` copied the whole reference vector before the boolean scan.
- **対応:** Added `ArtifactAbstractLayer::hasEnabledExternalMatteReference()` to scan the owned `NamedVector` directly and routed cache eligibility, cached-surface lookup, and `applySurfaceAndDraw` through the boolean query. Image, Shape, SVG, and static Text paths no longer copy references before a cache hit; `applySurfaceAndDraw` copies references only when an external matte is enabled, source images are available, and the surface must actually be processed.
- **価値または懸念（未検証）:** Avoids reference-vector copies on no-matte draws, cache hits, and draws without matte source images while preserving self/disabled filtering. Runtime allocation impact is not measured.
- **次に確認すること:** After build/runtime authorization, compare matte-enabled cache eligibility and rendering against the existing vector-based behavior for enabled, disabled, missing-source, and self-reference cases.


## 2026-09-26 — Partial recompose resource failure preserves pending damage

- **確認できた事実:** In the opt-in partial GPU recompose layer loop, missing render target/SRV resources at the draw, float-conversion, or blend stages previously took `continue`. A frame could then reach successful presentation and consume the scheduled damage region even though an intersecting layer was skipped.
- **対応:** Those three resource validation branches now abort the active partial pass and set `partialGpuRecomposeFailed`. The existing failed-pass path rejects presentation, invalidates retained slot state, marks all preview slots for full redraw, and retains a retryable full-redraw request. The existing full-render behavior still continues on these branches.
- **価値または懸念（未検証）:** Prevents partial damage consumption after a required layer stage was skipped. The resource-missing path is difficult to trigger through ordinary eligible scenes and runtime retry behavior remains unverified.
- **次に確認すること:** After build/runtime authorization, inject or simulate missing intermediate, float, and blend resources independently; confirm presentation is rejected, damage is not consumed, and the next frame performs a full redraw on D3D12 and Vulkan.


## 2026-09-26 — Cache eligibility avoids effect-list snapshots

- **確認できた事実:** Composition-space cache eligibility, partial-recompose eligibility, Composition View frame-sync, image GPU texture sharing, and Render Queue output-format choice used `getEffects()` snapshots for count/emptiness. The Composition View and Render Controller rasterizer-work helpers also copied `getEffects()` to inspect enabled pipeline stages. The rasterized-surface cache probe repeated the rasterizer-work check even though every caller had already established that condition.
- **対応:** Replaced count/emptiness snapshots with `effectCount()`. Added `ArtifactAbstractLayer::hasEnabledRasterizerEffect()` to scan the owned `NamedVector` directly and routed both rasterizer-work helpers through it. Removed the redundant rasterizer-work scan inside the cache probe lambda.
- **価値または懸念（未検証）:** Avoids temporary effect-list allocations and duplicate effect scans during cache/recompose eligibility, frame-sync, image texture-sharing checks, Render Queue format selection, and rasterized-surface probes while preserving existing effect-presence and enabled-stage semantics. Frame-time impact is not measured.
- **次に確認すること:** After build/runtime authorization, verify scene eligibility is unchanged for zero and nonzero effect stacks, then profile CPU allocations with the opt-in paths enabled.


## 2026-09-26 — Matte predicates avoid copied reference lists

- **確認できた事実:** Render Controller's boolean matte predicate and diagnostic count helper called `matteReferences()`, which materializes a `std::vector` copy although both callers only need presence or count. The pre-render matte-source scan also copied references for every active layer before checking for an empty list. The layer already owns the references in `NamedVector`.
- **対応:** Added `enabledExternalMatteReferenceCount()` to scan the owned references and implemented the existing boolean query in terms of that count. Render Controller predicates use those queries, and the matte-source prepass now skips layers with no enabled external reference before taking the vector snapshot needed to enumerate actual sources.
- **価値または懸念（未検証）:** Removes matte-vector copies from repeated render eligibility, damage dependency, diagnostic count, and matte-free prepass paths while retaining enabled, non-nil, non-self semantics. Concurrent mutation and render threading behavior remain as in the existing layer-state contract; runtime allocation impact is unmeasured.
- **次に確認すること:** Compare presence/count results against the vector-based predicate for disabled, nil, self, and multiple external references; profile allocation counts on matte-free layer traversals.


## 2026-09-26 — Matte application skips snapshots when no external matte is active

- **確認できた事実:** Two CPU matte application helpers and the GPU layer-matte preparation path copied each layer's complete matte-reference vector before filtering for enabled, non-nil, non-self references. When no such reference existed, the copied vector was only used to return without modifying the surface or GPU view.
- **対応:** Each path now checks `hasEnabledExternalMatteReference()` before taking the snapshot required to enumerate active references. The QImage helper also returns safely for a null layer.
- **価値または懸念（未検証）:** Avoids temporary vector construction for matte-free, disabled-only, nil-only, and self-only cases while retaining the reference enumeration needed for real matte work. Runtime output and allocation impact are unmeasured.
- **次に確認すること:** Compare CPU/GPU matte output for empty, disabled-only, self-only, missing-source, and valid-reference cases; profile the matte-free layer path.


## 2026-09-26 — Matte UI summaries use owned-entry queries

- **確認できた事実:** Layer Editor's surface summary iterated a copied reference list to count enabled external mattes. Timeline's layer-tone summary copied the list only to determine whether an active external matte existed.
- **対応:** Replaced those traversals with `enabledExternalMatteReferenceCount()` and `hasEnabledExternalMatteReference()`.
- **価値または懸念（未検証）:** Avoids temporary reference-vector allocations in UI summaries and preserves the enabled, non-nil, non-self predicate. UI runtime behavior is unverified.
- **次に確認すること:** Compare summary counts and Timeline tones for empty, disabled-only, self-only, and valid matte references.


## 2026-09-26 — Rasterizer surface builders defer effect snapshots

- **確認できた事実:** Composition View and Render Controller surface builders copied the full effect list and traversed it to detect an enabled rasterizer effect. The list was then needed only if rasterizer effects were actually applied; mask-only/matte-only processing did not consume that snapshot.
- **対応:** Both builders now use `hasEnabledRasterizerEffect()` for eligibility and take the effect snapshot only inside the rasterizer application branch. The controller also avoids querying/copying rasterizer effects when CPU effects are explicitly deferred to GPU.
- **価値または懸念（未検証）:** Removes effect-vector construction and redundant traversal from common mask-only and matte-only surfaces while preserving the effect enumeration needed for actual stack application. Runtime parity and allocation impact are unmeasured.
- **次に確認すること:** Compare effect-free, mask-only, matte-only, CPU rasterizer, and GPU-deferred surfaces against forced rebuild output; profile allocations on effect-free layers.


## 2026-09-26 — Composition final-effect path skips empty stacks

- **確認できた事実:** Composition View's final-effect helper copied the composition effect list before discovering whether the stack was empty. The helper can run after rendering the composition surface.
- **対応:** Added an `effectCount()` early return before retrieving the list.
- **価値または懸念（未検証）:** Avoids an empty vector snapshot on compositions without effects while leaving non-empty stack evaluation unchanged. Frame-time impact is unmeasured.
- **次に確認すること:** Verify empty-stack final-effect calls leave the rendered buffer unchanged and profile effect-free compositions.


## 2026-09-26 — Overscan bounds skip inactive rasterizer stacks

- **確認できた事実:** `layerOverscanPixels()` copied each layer's effect list before summing enabled rasterizer ROI expansion. Empty, disabled-only, and non-rasterizer-only stacks necessarily returned zero.
- **対応:** Replaced the empty-stack count check with `hasEnabledRasterizerEffect()` before the list snapshot.
- **価値または懸念（未検証）:** Avoids effect-vector construction for empty, disabled-only, and non-rasterizer-only layers while preserving zero expansion for those cases. Runtime impact is unmeasured.
- **次に確認すること:** Compare expanded bounds for empty, disabled-only, non-rasterizer, rasterizer-without-overscan, and overscan stacks; profile damage-bound calculation.


## 2026-09-26 — Composition effect eligibility scans owned entries

- **確認できた事実:** Final composition effects copied the composition-owned effect vector to find any enabled rasterizer effect, then traversed that copy again to apply the stack. Unlike layers, Composition had no direct predicate over its owned entries.
- **対応:** Added `ArtifactAbstractComposition::hasEnabledRasterizerEffect()` and used it before retrieving the list for actual final-effect application.
- **価値または懸念（未検証）:** Disabled-only and non-rasterizer-only composition stacks now avoid a temporary vector and duplicate traversal. Actual rasterizer stacks still take one snapshot for ordered application. Runtime output and ABI/module integration are unverified.
- **次に確認すること:** Verify the query matches vector-based detection for null, disabled, non-rasterizer, and enabled rasterizer entries; build and compare final composition effect output after build authorization.


## 2026-09-26 — Final rasterizer stages reuse already ordered snapshots

- **確認できた事実:** Layer and composition final-effect paths retrieved one effect snapshot and unconditionally called `sortedByStage()`, which copied that snapshot again before applying it. `isStageOrderValid()` already checks the relative stage order of enabled effects.
- **対応:** Both paths now retain and iterate the first snapshot when enabled effects are already stage-ordered, and call `sortedByStage()` only when the order is invalid.
- **価値または懸念（未検証）:** Removes a second effect-vector copy for already ordered stacks while retaining the previous stable sort for out-of-order stacks. Since disabled and null effects are skipped during execution, their placement does not affect rendered output. Runtime parity is unverified.
- **次に確認すること:** Compare ordered and out-of-order stacks containing disabled/null entries against the previous stable-sort behavior; profile stack allocations and effect output.


## 2026-09-26 — GPU raster plan rejects inactive stacks before copying

- **確認できた事実:** `buildGpuRasterEffectPlan()` copied all layer effects before discovering that the plan had no enabled rasterizer work. This plan builder is called while preparing the GPU layer intermediate.
- **対応:** Added `hasEnabledRasterizerEffect()` to the builder's initial eligibility guard.
- **価値または懸念（未検証）:** Skips temporary effect-vector construction and plan initialization for empty, disabled-only, and non-rasterizer-only stacks. Eligibility remains false for those cases, as before.
- **次に確認すること:** Compare plan eligibility for empty, disabled-only, non-rasterizer, supported, and unsupported enabled rasterizer stacks; profile layer preparation.


## 2026-09-26 — Damage invalidation queries full-frame effects directly

- **確認できた事実:** Layer damage invalidation copied the complete effect list to test whether any enabled effect's ROI hint required a full redraw. The code needed only a boolean and performed this check when recording damage.
- **対応:** Added `ArtifactAbstractLayer::hasEnabledFullFrameEffect()` to scan owned effect entries and replaced the snapshot loop with this query.
- **価値または懸念（未検証）:** Removes a temporary vector from property-damage invalidation while retaining the enabled/full-frame hint predicate. Threading and runtime invalidation behavior remain unverified.
- **次に確認すること:** Compare the query to the old loop for empty, disabled, full-frame, and bounded-ROI effects; verify full redraw promotion after property changes.


## 2026-09-26 — Overscan bounds scan owned effects without a snapshot

- **確認できた事実:** `layerOverscanPixels()` used an effect-vector snapshot to sum nonnegative ROI expansion from enabled, overscan-enabled rasterizer effects, excluding full-frame effects. The required result is a scalar and does not require effect enumeration outside the layer.
- **対応:** Added `enabledRasterizerOverscanPixels()` to scan the layer-owned effect entries and replaced the controller's vector-based summation.
- **価値または懸念（未検証）:** Removes effect-vector allocation and repeated `SharedPtr` copies from damage-bound and GPU layer preparation paths while preserving the existing sum/filter rules. Runtime bounds and timing are unverified.
- **次に確認すること:** Compare sums for disabled, non-rasterizer, overscan-disabled, full-frame, and multiple bounded-ROI effects; verify expanded damage bounds and profile traversal.


## 2026-09-26 — Mask path copies identified for a later cache boundary

- **確認できた事実:** The two principal render mask loops call `layer->mask(index)`, which copies a stored `LayerMask`; animated property resolution then copies each `MaskPath` before applying overrides.
- **対応:** Recorded the copy chain and its frame-time relevance in `Insight.md`; no cache or API change was made because the current source scan did not establish a safe property/frame revision key.
- **価値または懸念（未検証）:** This is a potentially larger masked-preview cost than the boolean snapshots addressed above, but a stale resolved-mask cache would change rendered output.
- **次に確認すること:** Trace mask/property revision and evaluation-frame ownership, then profile static and animated masks before selecting a cache/view design.


## 2026-09-26 — Animated mask properties participate in surface identity

- **確認できた事実:** `applyMaskPropertyState()` evaluates mask properties at `currentTimelineTime()`, which follows the active composition frame. The Composition View and Render Controller surface keys included `maskRevision` but no frame component for animated mask properties. `hasCachedAnimatedPropertiesWithPrefix()` detected keyframes but not expressions.
- **対応:** The property-prefix query now treats keyframes and expressions as time-varying. Both surface keys include requested, layer, and composition frame identity when cached `mask.*` properties are time-varying.
- **価値または懸念（未検証）:** Prevents a surface cached under one evaluated mask-property time from being reused at another frame, while static masks keep their existing reuse. Expression-driven masks may conservatively miss cache across frames even when an expression is time-invariant.
- **次に確認すること:** Scrub keyframed and expression-driven mask enabled/feather/opacity properties in both cache paths; compare each frame against forced rebuild and verify static masks still hit.


## 2026-09-26 — Static mask resolution skips property lookups

- **確認できた事実:** `applyMaskPropertyState()` built every `mask.*` property path and copied each `MaskPath` even when no property for that mask had keyframes or an expression. Static mask property edits route through `setLayerPropertyValue()`, which updates the base mask in `LayerMaskMatteState` and advances `maskRevision`.
- **対応:** Added a per-mask time-varying property prefix check before timeline lookup, path-string construction, and path-copy resolution.
- **価値または懸念（未検証）:** Static masks now use the already-current base mask without redundant animated-property resolution; keyframed and expression-backed masks retain the previous evaluation path. Render parity is unverified.
- **次に確認すること:** Compare static mask property edits against animated overrides; test adjacent mask indices (for example 1 and 10) so prefix matching does not cross masks; profile static and animated path work.


## 2026-09-26 — Static mask rasterization borrows owned mask data

- **確認できた事実:** Both primary CPU mask rasterization loops copied each stored `LayerMask` through `mask(index)`. For static masks, no animated-property override is needed; the base mask stays layer-owned until a mask mutation.
- **対応:** Added a read-only `maskView()` pointer API with invalid-index handling and an explicit mutation lifetime note. The two CPU rasterization loops borrow that view for static masks and create the prior resolved value copy only when that specific mask has keyframes or an expression.
- **価値または懸念（未検証）:** Avoids deep path copies for static masks during surface processing while preserving animated property evaluation. Borrowed views are safe only for immediate reads before any mask mutation; render-thread synchronization remains an assumption of the existing path. Per-mask dynamic-property detection now avoids constructing a prefix string and parses mask indices directly from cached property paths, with a dot boundary so mask 1 does not match mask 10. It still locks and scans the property cache for each query, so total frame cost is unverified; consider revisioned precomputed dynamic-mask metadata if profiling shows the scan is material.
- **次に確認すること:** Compare static and animated mask output, invalid-index behavior, and mutation/read lifetime; profile mask-heavy rasterization with many path vertices.


## 2026-09-26 — Composition image finalization skips effect-free conversion

- **確認できた事実:** `applyCompositionFinalEffectsToImage()` downsampled the source QImage and converted it through OpenCV and F32 before the buffer helper discovered that no enabled rasterizer effect existed.
- **対応:** Added the composition's direct rasterizer-effect query to the image helper's entry guard.
- **価値または懸念（未検証）:** Avoids image resize, CV/F32 conversion, and temporary image allocations for effect-free compositions. The helper still returns false in this case, preserving its prior result.
- **次に確認すること:** Verify the false/no-mutation contract for empty, disabled-only, and non-rasterizer-only composition stacks; profile effect-free composition finalization.


## 2026-09-26 — Composition image finalization reuses its effect snapshot

- **確認できた事実:** After the image path confirmed an enabled rasterizer effect and converted the image, it called the public buffer helper, which repeated the predicate scan and copied the composition effect list again.
- **対応:** Extracted an internal buffer application helper that accepts the already retrieved effect vector. The image path now queries and snapshots once before conversion, then passes that snapshot through; the public buffer path still validates and snapshots independently.
- **価値または懸念（未検証）:** Removes a second composition effect scan and vector copy from image finalization without changing external API or effect ordering. Runtime output remains unverified.
- **次に確認すること:** Compare image and buffer final-effect output for ordered and out-of-order stacks, then measure effect-list scans and allocations.


## 2026-09-26 — Kept LOD surface resolution consistent across cache fallbacks

- **確認できた事実:** Image, Shape, and Particle surfaces were already passed through `downsampleForLOD()` before `applySurfaceAndDraw()`. When an enabled external matte disabled surface caching, the lambda's uncached `allowSurfaceCache` branch downsampled those surfaces again. Separately, SVG cache lookup explicitly used source resolution (`downsampleForPreview=false`), while a cache-free fallback downsampled it.
- **対応:** Added a `skipLodDownsample` argument to the local draw helper. Image, Shape, and Particle paths mark their supplied surfaces as already downsampled; SVG paths preserve source resolution, matching their cache key/lookup policy.
- **価値または懸念（未検証）:** Aligns cache-free rasterizer fallback dimensions with the cache path and removes redundant CPU resizing when mattes bypass caches. Runtime pixel parity and timing remain unverified.
- **次に確認すること:** After build/runtime authorization, compare matte-enabled Image, Shape, and Particle output at each LOD against the cache-enabled path; compare SVG rasterizer output with/without cache and verify matte/effect dimensions.


## 2026-09-26 — RR3 deferred tile batches schedule the next render tick

- **確認できた事実:** The RR3 planner bounds each partial recompose to eight tiles and consumes only the presented canvas region. The render tick consumes `renderDirty_` before entering `renderOneFrameImpl()`. A successful batch retains remaining damage and advances its cursor. Rechecking only `hasDirtyRegions()` would also keep scheduling frames for damage outside the current visible ROI. Separately, the unchanged render-key early return ran before the planner and would discard the follow-up tick.
- **対応:** A successful frame schedules another tick only when replanning any preview slot's remaining damage against the current visible ROI produces a non-empty plan, including when the selected slot performed a full redraw. A dedicated atomic continuation flag bypasses the render-key early return for that follow-up and is consumed after slot acquisition, avoiding a second damage-map scan before the draw plan. Offscreen-only damage remains pending until a later viewport change; failure continues through the existing full-redraw recovery path.
- **受け入れ条件:** In runtime verification, create more than eight visible dirty tiles while idle and confirm successive batches advance until the visible plan is empty and the ticker stops, even if offscreen damage remains. Pan to the deferred region and confirm it renders; also check ROI movement and failure in a later batch. This remains unverified because build/runtime execution is not authorized.


## 2026-09-26 — Failed frame retries once before waiting for new activity

- **確認できた事実:** Frame failure invalidated retained slots and requested a full redraw, but the render tick had already consumed `renderDirty_`; resetting the render key alone did not start another frame while idle.
- **対応:** The failure path now schedules one retry on the existing render ticker. An atomic guard stops a persistent failure from retrying forever; a successful frame or a new `markRenderDirty()` request re-arms the single retry.
- **受け入れ条件:** Verify recovery from one transient failure, bounded behavior under repeated failure, and retry re-arming after a later user-driven dirty event. Runtime verification remains outstanding.

- **2026-09-26:** `PrimitiveRenderer3D::textureCache_` now has a hard 50-entry bound and 512 MiB estimated RGBA8 byte budget. Misses evict least-recently-accessed entries until both limits hold; a single image larger than the budget is drawn but not cached. Removed the former periodic age sweep after confirming its counter advanced per billboard draw call rather than per presented frame; bounded LRU retention now controls stale entries without a full-cache scan. Byte estimates exclude backend allocation overhead; GPU residency and pixel behavior remain unverified.
- **2026-09-26:** `RenderCommandBuffer` now pins `ITextureView` references on textured packets until `reset()` after submission; this lets PrimitiveRenderer2D enforce a per-map 50-entry LRU cap and shared 512 MiB estimated RGBA8 cache budget without invalidating queued sprite or mask packets. Both maps reserve 50 buckets during `createBuffers()`. A single image larger than the budget is retained alone to preserve existing raw-view return behavior. Sprite, transformed sprite, atlas, textured triangle, masked sprite, and billboard packet paths participate. Pending packet pins can keep evicted resources alive past the cache budget until submission; in-flight retention follows the command-buffer packet count, which currently has no explicit cap. Per-packet AddRef cost, cache churn, and D3D12/Vulkan runtime behavior remain unverified; estimates exclude backend allocation overhead.
- **2026-09-26:** `RenderCommandBuffer::append()` is now forwarding-templated and emplaces typed packet values directly into the packet vector before applying resource pins. This avoids the intermediate `DrawPacket` value move on typed callers. The reduction is source-level only; packet throughput and frame timing remain unmeasured.
- **2026-09-26:** `drawGlyphText()` now deduplicates first-appearance-ordered code points with a renderer-owned 4096-slot open-address index and clears only slots occupied by the preceding call. This replaces the prior per-codepoint scan of the growing unique-codepoint vector for up to 4096 unique values; after index saturation it falls back to the original linear membership check, preserving support for arbitrary input length. It adds 24 KiB fixed metadata per renderer; glyph ordering, saturation, and performance remain unverified.
- **2026-09-26:** Diligent's simple and transformed glyph submit paths now share a 2048-entry resolved-font/GlyphKey cache keyed by input QFont, code point, and render mode, allowing multiple font settings to coexist. A fixed 4096-slot index hashes common QFont properties once per text packet and confirms full QFont equality on hits; empty sentinels initialize on first use, capacity rollover clears both structures, and `ArtifactArray` capacity is reserved during buffer setup. This removes repeated per-glyph QFont fallback resolution and family UTF-8 conversion for cached text; variable cache memory, hash distribution, fallback parity, and frame-time impact remain unverified. `FontManager::loadFontFromFile()` exists, with no in-repository caller found; if runtime font registration is adopted, include a font-database revision in invalidation.
- **2026-09-26:** Diligent glyph scratch entries now copy only base/offset positions, offset rotation/scale/opacity, and `GlyphRect`. The fill and outline passes do not consume the rest of `GlyphItem`, which includes multiple QString values and shaped-index vectors. The scratch reserves 2048 compact entries during buffer setup, reducing first-use growth for large text runs; runs longer than that may still grow. Pixel parity and copy/allocation impact remain unverified.
- **2026-09-26:** Kept the second `GlyphAtlas::acquire()` in `drawGlyphText()`: prewarming all unique glyphs can fill/reset the atlas, which clears prior entries and overwrites their rectangles. Reusing rects across prewarm and packet emission without an atlas generation would risk sampling stale locations. If profiling justifies this optimization, add a read-only generation at the GlyphAtlas ownership boundary before using per-call rect caches.
- **2026-09-26:** `DiligentImmediateSubmitter::endFrameDebugCapture()` now swaps the current and last-frame pass vectors and returns void. Its sole frame-loop caller discarded the returned vector, while the old implementation copied the current records into the last-frame vector and then returned another by-value copy. The swap preserves `frameDebugPasses()` publication and reuses both capacities; snapshot freshness and allocation reduction remain runtime-unverified.
- **2026-09-26:** The five submit paths now move their completed local `FrameDebugPassRecord` into the current vector through the private recorder's rvalue-reference API. The records are not used after publication, so this avoids copying implicitly shared QString/binding members; recorded contents and performance remain runtime-unverified.
- **2026-09-26:** Particle renderer success and per-frame matrix logs now use the `artifact.render.particles` debug category; initialization is categorized info, while failure warnings remain unconditional. This avoids success-stream formatting, including submitter `debugState()` construction, when the category is disabled. Log behavior and performance remain runtime-unverified.
- **2026-09-26:** Particle acceptance is now a dedicated `ArtifactIRenderer::particleDrawQueued()` result consumed by ArtifactParticleLayer; it no longer parses debug text during rendering. Successful particle diagnostic text is stored as scalar metadata and formatted only when `particleDebugState()` is requested for a frame snapshot. Empty/failure states and frame start clear the queued flag. Diagnostic string parity and runtime fallback behavior remain unverified.
- **2026-09-26:** `PrimitiveRenderer2D::resolvedGlyphFont()` now uses a fixed 4096-slot open-address index for its existing 2048-entry resolved-font cache. The vector remains the owner of entries, and style changes or capacity rollover clear both structures. This replaces per-codepoint linear cache scans with bounded probing and adds 8 KiB of fixed index storage; text rendering parity and timing remain unverified.
- **2026-09-26:** Glyph atlas uploads already use `GlyphAtlasDirtyRegion`: new glyphs update a single dirty union box, and atlas reset requests a full upload. No additional Artifact uploader was added. Disjoint glyph updates can inflate the union area; a bounded multi-rectangle API would belong with GlyphAtlas ownership and needs transfer-area profiling first.
