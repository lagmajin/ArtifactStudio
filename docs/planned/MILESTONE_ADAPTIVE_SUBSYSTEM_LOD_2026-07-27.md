> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_RENDERING_PERFORMANCE_2026-03-28.md](MILESTONE_RENDERING_PERFORMANCE_2026-03-28.md)

# Milestone: Adaptive Subsystem LOD

**ステータス:** Partial foundation / implementation and runtime verification pending

**作成日:** 2026-07-27
**対象:** Composition Preview、Playback、RAM Preview、3D Render、Particle/VFX、Animation、Rig、Physics
**目的:** 編集結果の即時性と最終出力の正確性を守りながら、サブシステムごとの品質・更新頻度・処理量を負荷に応じて調整する

## 1. Goal

- Texture、Material/Shader、Reflection、GI/Lighting、Shadow、Post Effect、Animation、Skeleton、Physics、Particle/VFX、更新頻度を独立したLOD軸として扱う。
- zoomだけでなく、画面占有率、選択状態、操作状態、再生状態、可視性、計測負荷をLOD判定へ使う。
- selected / manipulated objectは常に応答性を優先し、古い低品質結果で上書きしない。
- Previewでのみ近似を許可し、操作終了後または停止後にFull品質へ収束させる。
- Offline RenderではLODによるframe skip、simulation省略、近似評価を禁止する。
- LOD切替時にPSO、texture、bufferを毎回作り直さず、事前生成済みresourceを再利用する。
- サブシステム別の効果を自動ログから比較できるようにする。

## 2. Current Baseline

2026-07-27時点で次の基盤が存在する。

- `LODManager`はzoomから`Low / Medium / High`を選択できる。
- `ArtifactIRenderer`はcurrent detail levelを保持する。
- Composition Viewerはzoomから`DetailLevel`を算出してrendererへ渡す。
- `ArtifactCompositionViewDrawing`は一部surfaceをLOD別にdownsampleする。
- `ArtifactProcedural3DLayer`はDraft / Preview / Full生成へLODを接続している。
- `Artifact3DLayer::drawLOD()`は通常drawと同じで、mesh / material LODは未接続である。
- `PreviewQuality`にscale、render mode、texture quality、shadow enable、最大frame rateがある。
- Mesh textureは通常1 mipで生成され、TextureQualityはtexture residencyへ一貫して接続されていない。
- Particleは最大数、simulation、GPU renderer、GPU visibility cullingの接続点を持つ。
- Rig2Dはframe移動時にbone / constraintを全評価する。
- Physics2Dはfixed delta相当のstepとsubstep countを受け取れる。
- 3D lightはshadow設定を持つが、共通shadow map LOD policyは未整備である。

## 3. Non-Goals

- すべてのサブシステムを同じLow / Medium / High設定へ固定すること。
- Offline Renderの正確性を犠牲にすること。
- selected objectの操作反映を間引くこと。
- LOD切替のたびにresourceを破棄・再生成すること。
- Skeleton LODでboneを単純削除し、weightやconstraintを壊すこと。
- Physics LODで経過時間を捨て、simulation結果を非決定的にすること。
- `libs/DiligentEngine` forkを最初から変更すること。

## 4. LOD Context Contract

既存`DetailLevel`を直接すべてへ伝播させず、frameごとのimmutableな`LODContext`を導入する。

### Inputs

- render mode:
  - `Interactive`
  - `Playback`
  - `RamPreviewBuild`
  - `OfflineRender`
- viewport zoom
- projected screen area / projected radius
- viewport内外とocclusion state
- selected / hovered / manipulated
- motion magnitude
- temporal dependencyの有無
- layer / system priority
- current CPU frame time
- current GPU frame time
- frame budget
- recent missed-frame count
- memory pressure

### Outputs

- `textureLevel`
- `materialLevel`
- `reflectionLevel`
- `globalIlluminationLevel`
- `shadowLevel`
- `postEffectLevel`
- `animationLevel`
- `skeletonLevel`
- `physicsLevel`
- `particleLevel`
- `updateRateDivisor`
- `forceFullQuality`
- `reasonFlags`
- `policyGeneration`

### Mandatory rules

- `OfflineRender`は全軸Full、`updateRateDivisor = 1`。
- selectedまたはmanipulatedはAnimation / Skeleton / TransformをFull更新する。
- property / gizmo操作終了後は指定時間内にFull-quality frameへ収束する。
- discrete event、visibility key、layer order、matte、audio同期eventは間引かない。
- temporal effectは勝手にframeを飛ばさず、対応policyを明示する。
- LOD変更はhysteresisとminimum hold durationを持つ。
- policy generationが古い非同期結果をpublishしない。

## 5. Quality Levels

共通名は`Full / Balanced / Draft / Minimal`とするが、各軸の具体的意味は独立させる。

| Level | 用途 | 原則 |
|---|---|---|
| Full | Offline、停止後refine、選択対象 | 正確性優先 |
| Balanced | 通常Playback | 見た目を保ちながら主要負荷を削減 |
| Draft | Interaction、負荷超過 | 更新頻度・sample数・effect数を削減 |
| Minimal | 画面占有率が極小、非選択、負荷緊急時 | continuityを保つ最小表現 |

`Minimal`でもobjectの存在、transform、visibility、主要色、timeline timingは維持する。

## 6. Work Packages

## LOD-0: Baseline, Diagnostics, and Policy Safety

### Scope

- `LODManager`のold/new通知を修正し、getterとstate transitionの責務を分ける。
- frame単位の`LODContext`とreason flagsを定義する。
- subsystem別の選択level、処理時間、skip / reuse数をJSONLへ記録する。
- LOD強制無効、軸別無効、Full固定のdebug overrideを用意する。
- Full結果とLOD結果を比較できるcapture contractを定義する。

### Metrics

- subsystem CPU / GPU time
- selected levelとreason
- level transition count
- hysteresis rejection count
- full-quality refine latency
- stale result rejection count
- cache hit / miss
- visual difference

### Exit criteria

- 1 frameで各サブシステムがなぜそのlevelになったか説明できる。
- Offline Renderで全軸Fullになる。
- LOD有効時の問題を軸単位で無効化して切り分けられる。

## LOD-1: Update Frequency LOD

最初に導入する共通scheduler。品質そのものより、評価頻度を制御する。

### Scope

- update cohortを`1 / 2 / 4 / 8 frame`に分ける。
- object IDから安定したphase offsetを作り、同じframeへの集中を避ける。
- skipped update間は前回結果保持または補間を選択する。
- selected / manipulated / newly visibleは即時更新する。
- dirty generation変更時は次回cohortを待たず更新する。
- animation event、decode request、physics accumulated timeを捨てない。

### Initial targets

1. 非選択Particle simulation preparation
2. 非選択Rig evaluation
3. procedural source regeneration
4. shadow refresh request
5. expensive diagnostic refresh

### Exit criteria

- update spikeが複数frameへ分散される。
- selected objectのinput-to-visible latencyを悪化させない。
- skipped updateによってtimeline timeが失われない。
- objectごとの更新phaseが再生ごとにランダム変動しない。

## LOD-2: Particle / VFX LOD

### Independent controls

- emission rate
- maximum active particle count
- simulation update divisor
- force field / collision frequency
- sort mode
- trail sample count
- sprite / mesh shading level
- VFX effect pass count
- render resolution

### Level proposal

- Full:
  - authoring値どおり
- Balanced:
  - particle budget 75%
  - expensive field / collisionは隔frame可能
- Draft:
  - particle budget 35%
  - update divisor 2
  - trail / sort / secondary effectを簡略化
- Minimal:
  - particle budget 10%
  - update divisor 4
  -主要emitterと主要色だけ維持

### Rules

- random seedとstable particle identityを維持する。
- LOD変更で既存particle poolを全消去しない。
- Additive particleはGPU compaction順序変更を許可できる。
- Alpha / Multiply等の順序依存blendはsort contractを維持する。
- Offline RenderはFull simulation / Full rendering。

### Exit criteria

- active count、simulation time、draw timeがlevelに応じて低下する。
- LOD切替でparticleが全消失・再発生しない。
- selected emitter編集はFullまたはBalanced以上で即時反映する。

## LOD-3: Material / Shader LOD

### Scope

- shader feature maskとquality tierを分離する。
- PSO variantを事前生成またはlazy cacheし、frame中に再compileしない。
- PBR texture / lighting featureを段階的に省略する。
- pointwise effect fusion、effect bypass、sample count低減をLODへ接続する。
- ID / normal / velocity等のauxiliary passは必要時だけ生成する。

### Level proposal

- Full:
  - authoring shader、全material texture、full lighting
- Balanced:
  - PBR維持、secondary texture / expensive branchを選択的に削減
- Draft:
  - simplified lit、normal / occlusion / clearcoat等を省略
- Minimal:
  - unlit base color / opacity

### Rules

- transparency、alpha test、matte、object IDは省略しない。
- color space、premultiplied alpha、blend contractはlevel間で一致させる。
- selected materialはFull previewへ昇格できる。

### Exit criteria

- steady stateでLOD切替によるPSO compileが0。
- Draft / Minimalでshader GPU timeが低下する。
- silhouette、opacity、matte結果がFullと一致する。

## LOD-4: Texture LOD

### Scope

- texture import / upload時にmip chainを生成または保持する。
- texture cache keyへformat、color space、mip policy、quality generationを含める。
- projected texel densityからrequired mip rangeを求める。
- sampler LOD bias / min LOD / max LODをquality policyへ接続する。
- VRAM pressure時は高mipからevictし、base resource identityを維持する。
- selected textureとpixel inspectionはFullへ昇格する。

### Level proposal

- Full: mip 0を含む全chain
- Balanced: projected sizeに必要なmipまでresident
- Draft: +1～2 mip bias
- Minimal: 小さいresident mipのみ

### Rules

- sRGB / linear / normal mapのmip生成方法を区別する。
- normal mapは単純color averageにしない。
- alpha coverageを必要とするtextureはcoverage-preserving mipを検討する。
- LOD切替時にCPU decodeやGPU uploadを同期実行しない。

### Exit criteria

- 遠景textureのsampling costとVRAM residencyが低下する。
- mip未準備時に黒textureを出さず、last-good mipへfallbackする。
- pan / zoom時のmip thrashingがhysteresis内に収まる。

## LOD-5: Reflection LOD

reflection techniqueを距離だけで固定せず、projected area、roughness、面の重要度、更新頻度、backend capabilityから選択する。

### Technique hierarchy

- Full / near:
  - ray-traced reflection
- Balanced / middle:
  - SSR
- Draft / far:
  - reflection probe / baked cubemap
- Minimal / tiny:
  - reflection off、またはambient specularのみ

### Importance rules

- mirror / water / polished hero surfaceは最低quality floorを持つ。
- rough surfaceは高品質reflectionの優先度を下げる。
- projected areaが閾値未満の面はreflectionを無効化できる。
- selected material / reflection probe / mirrorは即時Fullへ昇格する。
- low-importance surfaceは内部解像度と更新頻度を独立して下げる。

### Fallback order

`RT -> SSR -> Probe -> Ambient Specular -> Off`

fallback先が未準備の場合に黒を出さず、last-good reflectionまたはprobeへ戻る。

### Transition rules

- technique間を一定frame cross-fadeする。
- SSR confidenceが低いpixelはprobeへblendする。
- RT / SSR historyはcamera cut、resolution変更、material generation変更でinvalidateする。
- reflection result cacheはcamera、surface transform、roughness、source scene generationをkeyに含める。

### Controls

- RT ray / sample count
- max recursion
- reflection resolution scale
- SSR step / thickness / history weight
- probe density / probe resolution
- update divisor
- contribution threshold

### Exit criteria

- near mirror / waterはquality floorを維持する。
- middle / far surfaceでreflection GPU timeが低下する。
- technique切替時にblack frame、強いpop、古いcamera historyが出ない。
- Offline Renderはauthoring reflection policyまたはFull固定となる。

## LOD-6: GI and Lighting LOD

既存のSSGI / DDGI / ray-tracing capabilityとGI quality設定を、距離・重要度・負荷に応じた段階へ接続する。

### Technique hierarchy

- Full:
  - RT GI / high-quality DDGI / authoring bounce設定
- Balanced:
  - SSGIまたはreduced DDGI
- Draft:
  - sparse probe / baked lighting
- Minimal:
  - baked ambient / direct lightingのみ

### Controls

- indirect bounce count
- RT ray / sample count
- SSGI step countとresolution scale
- probe spacing / density
- probes updated per frame
- denoiser resolution
- denoiser update divisor
- emissive contribution threshold
- low-importance light count

### Rules

- 小さい発光物はprojected areaとemission energyの両方が閾値未満の場合だけGI寄与から外す。
- hero emissive、selected light、selected emissive materialは寄与を維持する。
- distant static areaはbaked lighting / probe cacheを優先する。
- camera cut、light generation、geometry generation変更時に必要範囲だけhistory / probeをinvalidateする。
- GI technique切替は直接光と合計energyが急変しないようtemporal blendする。
- denoiserを間引く場合も、古いhistoryの最大保持時間を制限する。

### Exit criteria

- ray / sample / probe update数とGI GPU timeがlevel別に低下する。
- 小さいemissive除外で主要照明の見た目が変わらない。
- camera移動後にstale GIが規定時間を越えて残らない。
- Offline RenderはFull GIと決定的なsample policyを使用する。

## LOD-7: Post Effect LOD

post effectは内部解像度、sample / iteration数、temporal更新頻度を独立制御する。

### Depth of Field

- Full: authoring resolution / sample count
- Balanced: half resolution
- Draft: quarter resolution、reduced sample count
- Minimal: off、または近似blur

focus target、selected object、focus distance編集中はquality floorを上げる。

### Blur

- spatial blurはhalf / quarter resolutionへ落とし、radiusを解像度に合わせて補正する。
- Dual Kawase等のdownsample chainを優先し、CPU readbackを挟まない。
- matte / alpha edgeを壊す単純downsampleは使用しない。

### Ambient Occlusion

- resolution scale
- direction / sample count
- radius step count
- temporal accumulation frequency
- distant / tiny object contribution

CACAO / SSAO等の実装ごとに同じquality contractへmappingする。

### Bloom

- downsample段数
- blur iteration
- threshold source resolution
- lens artifact / secondary streak

小さい発光物をGIから除外しても、Bloomの画面上寄与は別判定とする。

### Motion Blur

- sample count
- tile size
- velocity dilation quality
- camera-only approximation
- object contribution threshold

selected objectのtransform編集時はmotion blurを一時停止できる。

### Fog

- distant geometryのdetail削減を隠す補助として使用できる。
- LOD都合だけでauthoring fog densityやcolorを変更しない。
- far detail fadeとfog contributionを連動させ、突然objectを消さない。
- Offline Renderに自動追加fogを混入させない。

### Exit criteria

- effect別のinternal resolution / sample / iterationがdiagnosticsへ出る。
- Draftでpost GPU timeが低下する。
- focus edge、alpha edge、velocity edgeの破綻が許容差内である。
- Full refine時にhistory ghostや解像度境界が残らない。

## LOD-8: Animation LOD

### Scope

- property evaluation cacheをframe / generation / property dependencyで管理する。
- continuous animationはupdate divisorに応じてsampleし、間を補間する。
- offscreen / non-selected objectのconstraint / expression評価頻度を制御する。
- animation resultとrender resultのgenerationを一致させる。

### Never skip

- discrete / hold key
- visibility change
- event / marker
- layer in / out
- source frame change
- audio同期
- selected / manipulated property

### Exit criteria

- non-selected animation evaluation timeが低下する。
- skipped interval内のdiscrete eventを失わない。
- scrub、停止、Offline Renderではexact frame評価になる。
- refine後の値がFull評価と一致する。

## LOD-9: Shadow LOD

shadow map本流とresource lifetimeを先に確定してから既定有効にする。

### Controls

- shadow-casting light count
- map resolution
- cascade count
- filter tap count
- contact shadow
- update divisor
- static shadow cache

### Level proposal

- Full: authoring設定
- Balanced:主要light、medium map、reduced taps
- Draft:主要light 1、low map、cached update
- Minimal: shadow offまたはblob / contact substitute

### Rules

- selected light / caster編集時は即時refreshする。
- static caster / lightはshadow mapを再利用する。
- camera / caster / light generationでinvalidationする。
- shadow disabled時もlighting energyが不自然に倍増しない補正を検討する。

### Exit criteria

- shadow refresh数、map pixel数、GPU timeがlevel別に低下する。
- stale shadowが指定hold durationを越えて残らない。
- Offline RenderはFull shadow。

## LOD-10: Physics LOD

### Scope

- fixed-step accumulatorをauthoritativeにする。
- Previewでsubstep、solver iteration、collision detail、field frequencyを調整する。
- sleep / island / offscreen policyを導入する。
- skipped render frameでもsimulation elapsed timeを捨てない。
- snapshot / seek / resetとLOD stateを整合させる。

### Rules

- LODで`deltaTime`を捨てない。
- 大きなdeltaを1 stepへ渡さず、bounded catch-upする。
- selected body、active collision、constraint chainは品質を昇格する。
- Offline RenderとbakeはFull固定かつ決定論的にする。
- LOD変更でbody stateを再生成しない。

### Exit criteria

- Previewのphysics CPU timeがlevel別に低下する。
- 同じseed / Full policyのbake結果が再現可能である。
- LODからFullへ戻した後に爆発、tunneling、時間飛びがない。

## LOD-11: Skeleton LOD

構造削減より先に評価頻度とconstraint簡略化を導入する。

### Phase A: Evaluation LOD

- non-selected rigのevaluation divisor
- cached pose interpolation
- offscreen constraint frequency
- selected chain / IK targetの強制Full

### Phase B: Constraint LOD

- secondary constraintの頻度低下
- expensive IK iteration数の削減
- distant secondary motionの停止

### Phase C: Structural Skeleton LOD

- bone importance / protected bone contract
- reduced bone palette
- removed bone weightをancestorへremap
- animation clip / constraint target remap
- Full / reduced paletteのGPU skinning variant

### Rules

- root、deformation-critical、constraint target、selected boneは削除しない。
- weight総和を維持する。
- topology / binding generation変更時にreduced paletteを再構築する。
- authoring中はFull skeleton。

### Exit criteria

- Phase Aでbone / constraint evaluation timeが低下する。
- Structural LODでskin matrix uploadとvertex skinning costが低下する。
- silhouette / joint collapse / weight discontinuityが許容差内である。

## 7. Delivery Order

1. `LOD-0 Baseline, Diagnostics, and Policy Safety`
2. `LOD-1 Update Frequency LOD`
3. `LOD-2 Particle / VFX LOD`
4. `LOD-3 Material / Shader LOD`
5. `LOD-4 Texture LOD`
6. `LOD-5 Reflection LOD`
7. `LOD-6 GI and Lighting LOD`
8. `LOD-7 Post Effect LOD`
9. `LOD-8 Animation LOD`
10. `LOD-9 Shadow LOD`
11. `LOD-10 Physics LOD`
12. `LOD-11 Skeleton LOD`

`LOD-1`は後続すべての共通schedulerとなる。Physics / Skeletonは結果差と決定性のリスクが高いため最後にする。

## 8. Adaptation Policy

品質を1 frameごとに上下させない。

### Downgrade

- CPUまたはGPU p95がframe budgetを連続N frame超過した場合に1段階下げる。
- subsystem costがbudget超過の主要因である場合だけ、その軸を優先して下げる。
- emergency downgradeでもselected transform / animationはFullを維持する。

### Upgrade

- budget余裕が一定期間継続した場合に1段階上げる。
- 操作終了、再生停止、frame cache hit時はrefineを優先する。
- 複数軸を同時に上げず、負荷スパイクを避ける。

### Hysteresis

- downgrade thresholdとupgrade thresholdを分ける。
- minimum hold frame数を持つ。
- objectごとにstable phase offsetを持つ。

## 9. Cache and Generation Contract

LOD result cache keyには少なくとも次を含める。

- object / layer ID
- subsystem type
- source generation
- property generation
- time / frame
- LOD level
- quality-relevant settings
- backend / format / color space
- policy generation

低LOD結果はFull結果としてpublishしない。Full cache entryが存在する場合、低LOD生成によって上書きしない。

## 10. Test Matrix

### Scenes

1. 4K textureを使用する静止3D model
2. PBR materialを複数使用するinstanced mesh
3. 複数shadow lightとmoving caster
4. 100以上のanimated property
5. IK / constraintを含むRig2D
6. 多bone skinning mesh
7. Box2D rigid body / joint scene
8. 10万particleと複数force field
9. mirror / water / rough reflection混在scene
10. RT GI / SSGI / probe / emissive混在scene
11. DOF / AO / Bloom / Motion Blur / Fog混在scene
12. Particle、reflection、GI、shadow、post、animation、physics混在scene

### Modes

- idle
- property / gizmo interaction
- playback
- scrub
- RAM Preview build
- Offline Render

### Required verification

- CPU / GPU frame time median / p95 / p99
- input-to-visible latency
- subsystem update count
- selected level / transition count
- cache hit / miss
- VRAM residency
- Full refine latency
- visual difference
- deterministic Offline output

## 11. Rollback Conditions

- selected objectの操作反映が遅くなる。
- stale low-quality resultが新しいFull resultを上書きする。
- discrete animation eventを失う。
- Physicsでelapsed timeを失う、またはFullへ戻した際に不安定化する。
- Skeletonのweight / constraint remapが不正になる。
- Texture LODでblack / missing textureが出る。
- Reflection fallbackでblack frameまたは強いpopが出る。
- GI / denoiser historyがcamera cut後も残る。
- Post Effectの低解像度境界でalpha / focus / velocity edgeが破綻する。
- LOD transitionが頻繁に往復する。
- Offline RenderへPreview近似が混入する。

問題が出た軸だけを無効化し、他のLOD軸は維持できる構造を必須とする。

## 12. Related Documents

- `docs/technical/LOD_SYSTEM_FEASIBILITY_ANALYSIS_2026-03-28.md`
- `docs/technical/LOD_SYSTEM_PHASE1_IMPLEMENTATION_2026-03-28.md`
- `docs/planned/MILESTONE_INTERACTIVE_RENDER_PERFORMANCE_2026-07-27.md`
- `docs/planned/MILESTONE_GPU_DRIVEN_MDI_RENDER_2026-04-02.md`
- `Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md`
- `Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`
- `ArtifactCore/docs/MILESTONE_2D_RIG_SYSTEM_2026-04-15.md`

## 13. First Implementation Cut

最初の実装は`LOD-0`と`LOD-1`の限定範囲とする。

- `LODContext`とaxis別quality level
- render mode別の強制Full規則
- old/new transitionの正しい記録
- hysteresis / minimum hold
- stable update cohort
- selected / manipulated / dirtyの即時昇格
- subsystem別JSONL summary
- debug override

この段階ではTexture、Material、Physics、Skeletonの見た目やsimulation品質を変更しない。schedulerと証拠ログが安定した後、Particle/VFXを最初の実利用対象とする。

## Static audit follow-up (2026-07-29)

現行コードと関連ドキュメントを静的に確認した。ビルド・実機操作・性能計測は未実施。

| 項目 | 現状 | 判定 |
|---|---|---|
| 既存 LOD 基盤 | `LODManager`、renderer の `DetailLevel`、Viewer の zoom 接続、PreviewQuality、Procedural3D/Particle の接続点が存在する。 | 実装済み／統合確認待ち |
| LOD-0 Policy Safety | render mode と quality の基盤はあるが、axis 別 `LODContext`、Full 強制、debug override、JSONL evidence の共通契約は未確認。 | 未完了 |
| LOD-1 Update Frequency | Rig/Physics/Particle に個別の更新制御点はあるが、stable cohort、hysteresis、selected/manipulated の共通昇格 scheduler は未確認。 | 未完了 |
| 後続 subsystem LOD | texture/material/reflection/GI/shadow/post/animation などは接続点または個別設定に留まり、共通 policy と correctness gate は未完了。 | 未完了 |

### 判定

文書の `Not Started` は現状を過小評価していたため、基盤ありの部分実装へ更新した。ただし最初の実装カットで定義した共通 scheduler と証拠ログが未完成なので、マイルストーン完了とは扱わない。
