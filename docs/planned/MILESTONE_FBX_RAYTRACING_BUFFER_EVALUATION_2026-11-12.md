# Milestone: FBX Raster / Ray Tracing Buffer Evaluation

**最終更新:** 2026-11-12
**ステータス:** In Progress

## 目的と対象

FBXから読み込んだ実メッシュ、UV、複数材質、画像テクスチャを使い、Raster PBRとRay Tracing PBRの入力・中間値・最終出力を同じ条件で取得して、ImporterからGPU shaderまでの接続と画像応答を判定する。製品CompositionへのRT frame dispatchと合成も最終対象に含む。単独shader式や合成fixtureの成功だけで製品統合済みとは判定しない。

対象はopaque/masked FBX、single/multi-material、UV0/UV1、base color、metallic-roughness、normal、occlusion、emission、clearcoat、sheen、anisotropy、およびlinear HDR environment。Blended transparency、animation deformation parity、DX12/Vulkan両方の完了判定は、後続条件が整うまで個別に未検証として扱う。

## 現在の証拠と実装境界

- `ArtifactCore::RayTracingManager` はRT outputのRGBA32F textureを公開し、`traceCamera()`、PBR material table、texture samplingを持つ。既存のheadless Vulkan acceptanceはtexture/material応答の画素assertionとPNG captureを行う。
- RT camera requestにcapture向け出力モードを追加し、Beauty、Base Color、World Normal、Base UV、Metallic/Roughness/AO、Emission、Hit Distance、Primary Visibilityを同じRGBA32F出力へ個別dispatchできるようにした。既定値はBeauty。
- headless acceptanceへAOVごとのfinite/coverageと期待値確認、PNG表示capture、float RGB PFM保存を追加した。FBX Composition integrationにも、active cameraをRT cameraへ写して各AOVを保存する経路を追加した。さらにImporter由来FBX geometry/material/mapsをCore RTへ接続し、texture有効/無効の差をAOVで比較する試験を追加・実行した。
- `ArtifactIRenderer::drawMesh()` は対応可能なopaque/masked・single-instance geometryとmaterialをRT managerへ登録し、BLAS/TLASを更新する。
- `Model3DLightCompositionIntegrationTest` はFBXのRaster Composition表示とRT geometry登録を確認する。ただしdispatchは現状1x1 warmupのみ。
- `ArtifactCompositionRenderController` からactive cameraをRTへ渡すframe dispatchとRT output mergeは見つかっていない。現時点で製品CompositionのRT beauty captureは取得できず、実装前の未達項目である。
- 通常のRT outputは引き続きbeauty radiance一枚。AOV selectorは`RTRayCamera` per-dispatch診断であり、albedo/base color、world normal、UV、roughness/metallic、emission、primary visibility、hit distanceを製品Composition出力やexportへつなぐ契約は未実装。

## 比較条件

- **入力:** tracked CC0または自作の小型FBX fixtureを優先し、FBX本体・texture一式・バージョン・SHA-256・利用条件をmanifestへ記録する。配布不可モデルのcaptureやassetはリポジトリへ追加しない。
- **シーン:** 同一FBX、transform、scene light、HDR environment、camera、解像度、frame、material値、UV transformを固定する。最初は静止opaque/maskedモデルで実施し、animationとtransparencyを別評価にする。
- **表示:** 生のlinear float bufferを測定正本として保存し、表示用PNGは同じ固定表示変換から生成する。HDR値を8-bitへ丸めたPNGだけで合否を判断しない。
- **参照:** Raster出力を実装比較のbaselineとするが、物理的ground truthとはみなさない。Raster/RT間の差は既知のshaderモデル差、環境lighting、tone mappingを切り分けて説明する。
- **実行:** 順序はVulkan acceptance、Production Composition/Vulkan、Production Composition/D3D12。非対応backendはskipとして明記し、成功扱いしない。

## Capture matrix

|段階|capture|目的|評価|
|---|---|---|---|
|Importer|頂点/法線/UV0/UV1/triangle material index、slot別material factorsとtexture path|FBXからGPUへ渡すデータの完全性|finite値、index/range整合、fixture manifestとの対応|
|Raster inputs|material constants、各textureのlinear/sRGB view、depth|raster shader binding確認|slotごとのbinding、UV set/transform、texture decode、depth coverage|
|Raster outputs|beauty float、world normal、base color、metallic/roughness、emission、depth|基準のshader応答|finite値、coverage、channel statistics、局所cropの目視|
|RT inputs|BLAS/TLAS instance count、triangle material index、RT material table、texture descriptor/path mapping|AS/material packetの連携確認|active geometry、slot mapping、texture index、revision/transform整合。FBX multi-slot fixtureではRT AOV上のmaterial切替を確認済み|
|RT outputs|beauty RGBA32F、base color、world normal、UV, metallic/roughness、emission、primary hit/miss、hit distance|closest-hitとray traversal確認|finite値、coverage、AOV別期待応答、Rasterとの差分と原因分類。single-material全8 AOVとmulti-material BaseColor/MetalRough captureをVulkanで取得|
|Composition|RT beauty merge前後、depth/alpha、final presented frame|製品経路の統合確認|同一camera・resolutionでframe dispatch、merge、fallbackを検査|

各GPU buffer captureはcold diagnostic/test pathだけで読み戻し、production hot pathにreadbackを導入しない。Readbackはstaging resourceを再利用し、試験captureを行うフレームを計測対象性能値から除外する。

## 合否方法

1. baseline captureを取得した後、候補比較前に許容差を確定し、単位・色空間・alpha association・集計法をmanifestへ保存する。
2. 全画面に加え、材質境界、UV seam、normal detail、alpha cutout、高輝度emission、明暗遷移を含む固定cropを保存する。
3. 各bufferで寸法/format、min/max/mean/分位点、非有限値数、coverageを記録する。Beautyはchannel別MAE/RMSE/max/p99/閾値超過率と差分画像を取り、AOVは意味ごとの期待範囲とslot境界応答を調べる。
4. Opaque beautyのRaster/RT差は数値と目視の両方で判定する。AOVまたはshader fixtureの一致を最終画像の合格と取り違えない。
5. Time stabilityは静止fixtureでは対象外（理由: frame間でscene/materialを変えない）。animation/temporal outputの評価を始める段階で連続frameへ追加する。
6. Performanceはcapture readbackを外したwarm steady-stateで、backend、GPU、resolution、pass、GPU時間、resource bytesを記録する。現状の予算値は未定で、baseline取得後に固定する。

## 実装スライス

1. 既存Vulkan acceptanceのFBX fixtureをtracked・再現可能な小型fixtureへ固定し、Importer由来のslot/UV/texture情報をmanifest化する。
2. **完了（Vulkanでshader/runtime確認済み）:** per-dispatch RT diagnostic output selectorを追加した。通常callerの既定値はbeautyで、永続設定やUI toggleは追加していない。Visibilityはprimary camera rayのhit/missを示し、shadow visibilityは含まない。
3. **一部実証済み:** Core acceptanceにAOV別float readback、PNG/PFM/統計JSON、finite/coverage/代表値assertionを追加した。8つのsynthetic triangle AOVとFBX separate-PBR AOVがNVIDIA GeForce RTX 4070 Ti / Vulkanで実行できた。FBX geometryのBeauty/BaseColor/Normal/UV/MetalRough/Emission/HitDistance/Visibilityを全画面で保存した。65,536/65,536 pixelが全AOVでhit、全RGBA32F sample finite。代表値: BaseColor mean `(0.60003, 0.20012, 0.05003)`、WorldNormal `(0.5, 0.5, 0)`、UV range `0.1906–0.8094`、Metal/Rough/AO `(0.20012, 0.80049, 1)`、HitDistance range `1.50–1.74`、Visibility `1`。Metal/Rough texture有効時と同じFBX・geometryのscalar-onlyを比較し、AOVがtexture result`(0.2,0.8)`からfactor`(1,1)`へ移ることを確認した。実行captureは`out/build/x64-Debug-Agent/artifacts/ray-tracing-frames/`にPNG/PFM/統計JSONとして生成される。single-material fixtureが単色mapかつ画面を満たす形状のため、そのcaptureだけでは境界・背景品質を目視評価できない。multi-material fixtureのfixed boundary cropは別途capture済み。
   - **追加確認（2026-11-12）:** 自作`multi-material-pbr.fbx`で2 slot／12 triangleをImporterからRenderDataへ渡し、RT `Base Color`と`Metallic/Roughness` AOVをRTX 4070 Ti / Vulkanで取得。対角カメラから両slotを可視化し、各全画面captureでslot 0 = 5,721 pixels、slot 1 = 11,443 pixels。fixed cropは全画面座標`(64,64)`、`128×128`で切り出し、各AOVともslot 0 = 4,836 pixels、slot 1 = 10,126 pixels。両群のfinite値とslot別期待値をassertし、全画面/cropのPFM/PNGとpixel-count JSONを保存した。PNG目視ではcubeの可視2面に沿って青/橙と白/cyanが分かれている。slot存在のassertionは画質parity閾値ではない。
   - **Raster multi-slot確認（2026-11-12）:** 同じImporter geometry・slot index rangeを`MeshRenderer::setMaterialRanges` / `drawMaterialSlots`へ渡し、Vulkan RGBA32Fをcapture。slot画素数は5,721 / 11,443、fixed crop `(64,64,128,128)`では4,836 / 10,126 pixel。RTとRasterは水平スクリーン基底が反転しているためRaster Xを反転して位置合わせした。Raster material rangeのbaseColorをblackにし、同じsource baseColorをemissionへ渡すdiagnostic bufferを作成。RT `Base Color` AOVとのcoverageは両方17,164 pixelで一致し、coverage内51,492 RGB sampleのMAE/RMSE/最大差がすべて0だった。PNG/PFM、差分PFM、統計JSONを保存しcropを目視確認。これはImporter→RenderData slot identityとRaster material range / RT material tableが同じbaseColor値を返す契約の証拠で、Beauty parityやproduction `ArtifactIRenderer` runtime確認ではない。scalar AOV比較はsourceへ追加したが、実行・結果確認はまだ。
4. **captureコード追加・Composition未実行:** Production Composition integrationでFBXを実Raster表示し、active cameraから`RTRayCamera`を作ってRT AOVをdispatch、RGBA32F PFM、表示PNG、JSON channel statistics（finite count、min/mean/p50/p95/p99/max、coverage）を保存する。single-material fixtureを使うこのComposition test自体はまだ未実行。Core acceptanceでは同じmulti-slot FBXのRaster material-only Base Color diagnosticとRT Base Color AOVを別dispatchで比較し、horizontal alignment後に0差を確認した。Beauty parity、Metallic/Roughness/AO diagnostic、およびComposition統合比較は未実施で、captureだけでは全体の画質差を合格と判定できない。
5. Compositionへの自動frame dispatch/output mergeは依然pending。capture integration testで得るRT AOVは、現在の1x1 warmupとは独立に明示dispatchする診断結果であり、製品Composition RT表示の証拠ではない。
6. Vulkan結果が安定してからD3D12の同じcapture matrixを実行し、backend parityとfallbackを判定する。

## 完了条件

- ImporterからRaster/RTまでのmaterial/texture/UV/slot対応をfixtureとcaptureで追跡できる。
- 指定AOVを両経路で保存し、Beauty差分、統計、fixed crop、目視判定、性能を含む再現可能なreportがある。
- Compositionのactive camera、frame dispatch、RT output merge、unsupported fallbackが実runtime captureで確認される。
- 画像評価の必須項目すべてに結果、対象外理由または未検証が記載され、採用/不採用判断と既知制限がある。

未取得の画像、未実行のbackend、未接続のComposition pathは成功扱いしない。本マイルストーンは製品RTを常時有効化する承認ではない。

## 2026-11-12 実装状態

- 出力selector、Core AOV capture/value-checkケース、FBX Composition AOV captureケースをsourceへ追加した。さらにFBX multi-material fixture、Mesh RenderDataのtriangle material index伝搬、Artifact production RT登録での伝搬、slot別RT AOV acceptance captureを追加した。
- Core AOV captureはVulkan runtimeで実行済み。synthetic AOV全8種とFBX separate metallic/roughness mapのfloat output assertionが通過した。FBX texture mapとscalar-only結果を同じgeometry/cameraで比較する条件が通り、Importer texture pathsからRT shader samplingまでの単一材質接続を実証した。出力はRGBA32F readbackからPFMと表示PNGへ保存される。
- FBX Composition capture sourceにRGBA32F staging readback、PFM、チャンネル統計JSON、PNGとのcoverage整合チェックを追加。診断test内のみのGPU readbackで、製品frame hot pathへは入れていない。C++/runtime未確認。
- 静的レビューでAOV testのcapture helperがRGBA32F readback結果を256x256固定で扱う不整合を発見し、readback pixel vectorからPNG/PFMを作るよう修正。従来のcapture test helper呼び出しはそのまま維持し、新規AOV captureの同一バッファ保存を可能にした。
- FBX integration captureはRT出力のみでComposition Raster AOVとの比較ではない。Core acceptanceでmulti-slot Raster material-only Base Color diagnosticとRT Base Color AOVを対にして比較し、horizontal alignment後に0差を確認した。これはBeautyやProduction Compositionの一致を示さない。Metallic/Roughness/AO diagnostic sourceは追加したが、現行build treeで外部ArtifactUiTest buildとの競合があり未実行。production Composition比較も未達。
- RT shaderとCore acceptance C++はDebug構成でコンパイル済み。FBX AOV filtered testはRTX 4070 Ti / Vulkanでpass。再構成時はCMakeがvcpkg/toolchain discoveryを実行し、Windows SDK `rc.exe`探索に一時的なPATH補正が必要だった。
- 既存の非FBX `TracesPlaneAndThreeDimensionalMeshGeometry` baseline testにはspot coneの3 assertion失敗が残る（line 13959, 13969, 13971）。FBX追加test自体はpassしており、この既存失敗は本sliceの結論と混ぜない。
- Composition targetのC++ compileは既存Module/IFC依存エラー（`Container.NameMap` / `Core.ArtifactSet`、および先行時点の`ScopeComputer` declaration不整合）に阻まれ、runtime captureは未取得。製品Compositionのframe dispatch/output mergeも未実装。
- `separate-pbr.fbx`はsingle-materialかつ単色のmetallic/roughness maps。Raster Beautyも同じimported geometry・mapsでRGBA32F captureし、map-on/map-off差はcoverage一致の196,608 RGB samplesに対してMAE=0.226304、RMSE=0.274845、最大差=0.445396。したがってRaster map responseは確認できた。Raster/RT Beautyのcoverageも両方65,536 pixelだが、同条件pixel差はMAE=0.222653、RMSE=0.277033、最大差=0.451101。絶対差PFM/PNGを保存した。この差をただちに不具合とも合格とも判定しない。両系統のlighting/shaderモデル差に分解できておらず、比較用の承認基準値も未決定。UV seam/多材質境界 parityは未評価。Vulkanだけの結果であり、D3D12、性能、alpha/transparency、animation temporal stabilityも未検証。
- `multi-material-pbr.fbx`（自作fixture）を使う新規RT acceptanceはVulkanでpassした。Importerが2 material・2 slotを返し、RenderDataが12 triangleへmaterial indexを渡し、RT AOV上で両slotが見える。Base Color / Metallic-Roughness双方のcaptureでslot別画素数は5,721 / 11,443。fixed crop `(64,64,128,128)`内では4,836 / 10,126 pixel。両slotの有限値と期待範囲を確認し、対角視点の2つの可視面で色およびmetal/roughness差を目視確認した。結果は`53-fbx-multi-material-base-color`と`54-fbx-multi-material-metallic-roughness`の全画面/crop PNG/PFM/JSON。slot存在assertionはparity許容差や画質合格基準を定義しない。
- 同じRT acceptanceでRaster `MeshRenderer`のmaterial range drawをRGBA32Fでcapture。slot画素数は5,721 / 11,443、crop内は4,836 / 10,126。emission-only Base Color diagnosticをRT AOVと比較し、Raster X反転後の51,492 RGB sampleでMAE/RMSE/最大差=0、coverageは両方17,164 pixel。`55-fbx-multi-material-raster.png/.pfm`, `55-fbx-multi-material-raster-boundary-crop.png`, `55-fbx-multi-material-raster.json`, `56-fbx-multi-material-base-color-difference.pfm`を保存。Base Color cropで境界を目視した。これはmaterial factor契約の確認で、lit Beautyやproduction Composition parityではない。
- 追加された本番Artifact renderer側triangle material index伝搬は`ArtifactIRenderer.cpp` objectとしてコンパイル済みだが、runtimeでは未確認。Artifact全体target buildは既存`PluginRegistry.cpp`の`Container.NameMap` / `Core.ArtifactSet` IFC不足で停止した。個別`MeshImporterFbxTest`契約テストもFBX import test optionが無効なため今回未実行。新規RT acceptanceはImporter→Mesh RenderData→RT managerまでを直接検証している。
- Raster Beauty map-on/map-off差はcoverage一致の196,608 RGB samplesに対してMAE=0.226304、RMSE=0.274845、最大差=0.445396。Raster/RT Beautyのcoverageも両方65,536 pixelだが、同条件pixel差はMAE=0.222653、RMSE=0.277033、最大差=0.451101。絶対差PFM/PNGを保存した。この差をただちに不具合とも合格とも判定しない。両系統のlighting/shaderモデル差に分解できておらず、比較用の承認基準値も未決定。Base Color material diagnosticだけはRaster/RT比較を通過。Metallic/Roughness/AO parity、UV seam、normal detail cropは未評価。Vulkanだけの結果であり、D3D12、性能、alpha/transparency、animation temporal stabilityも未検証。
- したがって現段階は**single- / multi-material FBXからRTへのslot/material値接続は合格、Raster/RT画像品質および製品Composition統合は未達**。R&D全体を完了とはしない。
