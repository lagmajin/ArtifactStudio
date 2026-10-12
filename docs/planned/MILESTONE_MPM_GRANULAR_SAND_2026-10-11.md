# MPM 2D Granular Sand Simulation

**最終更新:** 2026-10-12
- 2026-10-12: CFL初期条件を既定値で再計算。640×480 source・96×48 seedではcellSize=min(384/96,168/48)=3.5px、E=12,000/ν=0.2/ρ=1.6でlongitudinal speed≈91.3px/s、CFL dt≈1.92ms、15fps frame当たり約35 substepsとなり1024上限内。UIの個別hard rangeは全組合せを保証しないため、最大E・最小density・高Poisson比・高particle countの組合せはfactory側CFL guardで拒否される。Layer setterは設定とproperty cacheをrollbackし旧solverを維持する。組合せ上限のUI説明・runtime feedback、変形後CFL backlogは未検証。
- 2026-10-12: 640×480 default sourceのsampling geometryを確認。128×96 fieldは5×5px/texel。96×48 seedは初期area/particle=14px²・nominal support≈3.74px、最大500×200 seedは≈0.645px²・nominal support≈0.80pxで、どちらもraster floorがtexel対角≈7.07pxまで引き上げる。kernelは粒子ごとにcurrent volumeを離散kernel sumとpixel areaで正規化して加算するのでparticle countで総rest volumeを直接減らさない。edge/camera crop・alpha thresholdを含む実画像結果と最大数時CPU timeは未確認。
- 2026-10-12: render sampling負荷を上限算出。authoring最大100,000粒子、Granular raster stencilは各軸18 sample以下なので最大324 samples/particle（約32.4M sample/field、さらに最大同数のaccumulate）で上限がある。既定128×96 fieldは640×480 sourceで5px/texelとなり、seed由来supportがtexelより小さい場合にraster側support floorをtexel対角相当に上げ、mass-normalized kernelをfieldへ加算する。これはboundedである根拠で、性能が実用的な証明ではない。最大seed・bounds拡張時のCPU時間と画素品質は未計測。
- 2026-10-12: backlog値はComposition clockでまだsolverへ渡していない秒数とsolver-local accumulatorでまだsubstep化していない秒数の和で、処理段階が異なるため重複しない。両accumulatorの102.4秒上限を超える入力時間は破棄され得るため、Property Editor警告を「上限時にsimulation timeが省略される場合がある」と明記し、PhysicsSystem/MpmSolverコメントも上限超過分の破棄を説明する。実際の飽和・時間欠落・UI警告状態はruntime未確認。
- 2026-10-12: solver-local `accumulatedTime_`とComposition granular clockのpending秒数をLayer API経由で読み、単一MPM Layerの既存Property Editor status行へ0.25秒以上のsimulation backlog警告を追加。grid expansion limitが同時に出た場合は領域制限を優先。再生中は既存FrameChangedEventを8 frameごとにsampleし、status種別が変わる時だけ翻訳文字列を構築する。property update timerや新規event配線・frameログは増やさない。警告閾値・表示更新間隔・catch-up/drop条件のruntime確認は未実施。
- 2026-10-12: 物性propertyの使用箇所を再確認。`density`はseed時の`mass=rho*volume0`に使われ、密度field出力は`volume0*det(F)`と支持kernelから作られるため密度調整は見た目の色/alphaではなく質量・慣性・応力へ作用する。摩擦角は粒子hardening stateを通るDrucker–Prager yield係数へ作用。現在は変更時にsolver/baselineを再構築するので履歴は初期seedへ戻る。物理パラメータ適用先は一貫するが、密度の2D unit-depth解釈と調整時resetのUXはruntime/product review未確認。
- 2026-10-12: Save/clone lifecycleとsnapshot validationを再確認。Composition JSONはcurrentFrameとLayer authored settings（粒子配列は含まない）を保存し、load/Render Queue cloneは全Layer attach後に保存frameへ`goToFrame()`するため、粒子はseed baselineから決定論的replayされる。`canRestoreSnapshot()`はgrid geometry/capacity、world/local accumulator、active particle位置・F/Fp・C・q・seed不変mass/volumeとfloor/open-grid safe marginを全solver分検証してからworld restoreする。source上の復元順と検査に欠落は見つからないが、load/clone/長尺frameでの数値再現性・処理時間はruntime未確認。
- 2026-10-12: MPM描画のframe/bounds順を静的追跡。Render Queue CPU/GPUは`Composition::goToFrame(target)`後にsurface寸法を求め、Composition Viewは対象Layerの`goToFrame(target)`後に`particleRenderBounds()`を使う。直接GPU drawとCPU raster fallbackはsource矩形と粒子AABBのunionをdensity-field sampling bounds/local placementへ渡し、fallback出力寸法はsource-size由来のrequested sizeを保持しつつ4096px/4M pixelへ制限する。フレーム順・空間範囲に静的な食い違いは見つからない。各出力経路の画素一致、tile/camera/crop時のruntime表示、bounds極値は未確認。
- 2026-10-12: `projectMpmGranularHencky2D()`と`applyGranularPlasticity()`を計画書の2D式と再照合。非extension係数は`((lambda+mu)/mu)*alpha*trace`、偏差ノルムは2D Frobenius、extensionはtrace>=0でtipへ投影し、投影後`Fp=Fe_new^-1*F`で`F=Fe*Fp`を保つ実装。式の符号・次元係数・積順序に静的な不一致は見つからない。公開論文の補足PDFは取得先が403を返したため、独立解析fixture、数値試験、離散solverでの物理妥当性は未確認。
- 2026-10-12: 再生入力経路を静的追跡。`PlaybackService`のframe syncは`Composition::goToFrame()`を呼び、連続フレームは各サブステップ直前にcollision proxyと`mpmSand.gravityY`を評価してから`advancePhysicsFrame()`内のMPM clockを進める。seek/replayも各frameで同じLayer評価・衝突評価を行う。gravity keyframeを含む入力順序に静的な差は見つからないが、再生とseekの粒子状態一致はruntime未確認。

**ステータス:** In Progress
- 2026-10-12: source resize経路はbase setter後にsolver再構築し、失敗時は旧source sizeとproperty valueへ戻す。`configureSolver()`はseedをsourceの`(20%,10%)`から`(80%,45%)`までの矩形に置き、domainとfloorを同じ`(0,0)..(width,height)`に渡す。PhysicsSystem側のsafe-grid検査はseed全体をdomain内かつstencil安全範囲へ制約する。したがって寸法変更後の初期seed/floor座標は同じsource-local契約で構成される。実寸法変更後の表示、旧粒子状態のresetが意図どおりか、runtime未確認。
- 2026-10-12: MPM Sand layerの通常生成は`ArtifactProjectService`がAddLayerを含む既存macroへ包む。UndoはCompositionからremoveして`setComposition(nullptr)`経由でsolver registrationを解除し、Redoは同じLayer objectをappendしてattachment readinessを検証、`setComposition(owner)`が新solver/baselineを再作成する。material property変更も新solverとbaseline生成が成功した後に登録を置換し、失敗時はlayer authoring値をrollbackして旧solverを維持する。複製はJSON propertyを復元してからCompositionへattachする。これらはコード経路の静的確認であり、Undo/Redo・duplicate・selection/current-frame・再生後の状態とdirty/cache通知を含むruntime確認は未実施。
- 2026-10-12: CPU `stepOnce()`の粒子境界penetration補正で、Granular modeは接触方向への法線速度だけを除去し、境界から離れる速度を保持するよう修正。粒子位置補正とAPIC `C`の法線成分projectionも外向き接触時だけ適用する。legacy elastic modeの従来の速度停止挙動は維持する。floor rebound/離脱時の数値応答はruntime未確認。
- 2026-10-12: `MpmSolver2D::update()`のCPU frame後処理は、床だけを設定したGranular solverでも旧boundary rectangleの左右・上端へparticleをclampしていた。GPU側のpost-step処理およびCPU `stepOnce()`と同様、左右・上側は`hasBoundary_`時だけ、下側は通常boundaryまたはGranular floor時だけclampするよう修正。CPU frame更新での開放境界とCPU/GPU一致はruntime未確認。
- 2026-10-12: Granular solverは作成時に現在gridの最大4倍node数（Compositionの既存4,194,304 node budget内）をvector sizeとしてcold-path確保し、その範囲内でactive grid geometryを変更する。frame中のgrid拡張は再確保せず、粒子状態を保ってgrid origin/寸法を更新する。snapshot restoreは予約capacity内のgeometry差を許容する。MPM Sandの境界はsource下端floorだけにし、左右・上側をopenにした。拡張上限時はsolver status flagを立て、従来のsafe-grid clampへfallbackする。grid growth、floor contact、snapshot seek、capacity到達時のruntimeと数値挙動は未検証。GPU/CPU surfaceはsource boundsとparticle boundsのunionを使い、Render Queueのoutputは4M pixel以内にdownsampleする。
- 2026-10-12: `MpmBounds2D`と`MpmSolver2D::particleBounds(padding)`を追加し、active material pointのfiniteなsolver-space AABBをallocation-freeに照会できるようにした。`ArtifactMpmSand2DLayer`はこれを使い、GPU packetとCPU Render Queue surfaceの描画範囲/originをsource rectangleと粒子範囲の和集合にする。Render Queue出力は各辺4096pxかつ4M pixel以内にbounded downsampleする。これでgrid外移動は可能にならず、matte placement・広域surfaceの見た目と性能は未検証。ビルド未実行。
- 2026-10-12: Composition Viewの`drawLayerForCompositionView()`を追跡。`localBounds()`は早期の有効サイズ確認に使われ、MPM型は専用source-rect raster branchに入らず最後の`layer->draw(renderer)`へ到達する。関数内にauthored boundsとの交差による per-layer culling は見つからず、MPM `draw()`は粒子範囲から作ったlocal-space meshをglobal transform付きで送る。Composition viewport/render target自体の外側は通常どおりclipされる。実機での画面端・camera・crop表示、selection overlayの範囲は未確認。
- 2026-10-12: 初期Composition View経路はMPM Layerをgeneric direct-draw fallbackへ送っていたが、その後Composition Render ControllerとRender Queue helperにMPM専用分岐を追加した。対応rasterizer effectはGPU intermediateへ適用し、layer maskまたは未対応rasterizer work時はparticle-expanded bounded software surfaceへfallbackする。matteはComposition passの既存処理へ渡す。modifier適用範囲と通常Composition Viewでのruntime差分は未確認。
- 2026-10-12: `gridExpansionLimitReached`は一度容量上限へ到達した後、次substepの粒子clampで条件が消えてもstickyに保持する。値を`MpmSnapshot2D`に含め、seek時は過去時点の状態へ戻す。clear/reconfigureで解除する。solver getterと単一MPM選択時のProperty Editor警告は実装済み。警告更新タイミングと診断用surfaceの必要性はruntime未確認。
- 2026-10-12: 選択中の単一MPM Sand layerのProperty Editorに、grid拡張上限到達時だけ警告文を表示する。既存のframe/updateValues経路から状態を読み、独自event配線や常時表示chromeは追加しない。警告文をja/en/zh/zh-TW/ko/fr/de/es/pt/ru/arへ追加。表示更新タイミングと各localeの実画面は未確認。
- 2026-10-12: Composition Render ControllerのGPU layer intermediateへMPM Sandを直接描く専用分岐を追加。GPU rasterizer effect planが有効でlayer maskがない場合は中間texture上のMPM geometryへeffectを適用する。GPU未対応rasterizer work/layer mask時はparticle-expanded boundsを保持し、4096px/4M pixel上限でsoftware surfaceへfallbackする。ArtifactCompositionViewDrawingのRender Queue GPU helperにも同じMPM分岐を追加し、rasterizer work時はCPU effect/mask処理へsoftware surfaceを渡す。粒子boundsの変化でsoftware surface寸法が毎frame変わらないようrequested-size保持APIを追加し、authored layer sizeとLOD基準・4096px/4M pixel内の固定寸法で出力する。density fieldのsampling rectangleは拡張particle boundsを保ち、別QImageへのLOD downsample copyを避ける。CPU surface二面がどちらも保持中の場合のfallbackも、拡張particle boundsでbounded QImageを生成する。draw opacity overrideを追加してintermediateでのlayer opacity二重適用を避ける。runtimeでGPU effect/mask/matte、bounds、opacity、画素一致、surface再確保頻度は未確認。
- 2026-10-12: `MpmSolver2D`へactive particle AABB cacheを追加。粒子境界はsolverの既存final finite-state passで集計して更新し、seed追加・snapshot restore後は初回readで再構築、capacity fallback等でstep前にpositionを補正した時はcacheをinvalidateする。`particleBounds()`とgrid expansionからrender boundsまで通常時の追加全粒子bounds scanを除いた。density writer自体は粒子走査を維持する。最大10万粒子でのbounds/density CPU time計測とreplay/restore後のbounds一致確認は未実施。
- 2026-10-12: `MpmSolver2D::snapshot(MpmSnapshot2D&)`を追加し、宛先vector capacityを保って粒子・fracture配列をcaptureする。Granular checkpointは最古の非anchor共通snapshotをmap内で移し、solver配列capacityが十分な場合にcapture storageを再利用する。再利用時の大配列確保を避け、storageは引き続きsnapshot map所有であるため`trimGranularWorldSnapshots()`のbyte budget内に含まれる。共通再利用storageがない初回capture等は従来どおりbudgetを予約して新規確保する。100,000粒子のcopy時間、allocator lock、frame hitch、eviction/reuse後のseek数値一致、capture failure時の動作は未計測。
- 2026-10-12: 既定Layer 640×480、seed 60%×35%、96×48点、E=12000、ν=0.2、density=1.6の静的見積りはseed 384×168、spacing 4×3.5 px、dx=3.5 px、grid 187×142=26,554 nodes。参照縦波速は約91.29 px/s、CFL internal dtは約1.917 ms、24 fps frameあたり約22 internal step、生成時1/15秒budget判定は35/1024 step。これは式と既定値の照合であり、実フレーム時間・落下/堆積挙動の検証ではない。
- 2026-10-12: Klár et al. 2016一次資料の式(25)–(31)を静的照合。Hencky energy/stress、return-map係数`(d*lambda+2*mu)/(2*mu)`、trace保持、extension tip、hardening incrementは実装と整合し、`d=2`で現在の`1+lambda/mu`になる。論文の摩擦角変換式(31)は3D用であり、Layer実装は別途定義した2D in-plane変換`alpha=sin(phi)/sqrt(2)`を使うため、論文全体の完全移植とは扱わない。実装fixture/runtimeは未検証。
- 2026-10-12: CFL波速だけで最小刻みの予算を超える場合、以前の大きなfixedDtを残さず`1e-6`へ落とすfail-safeを追加。生成APIの材料範囲では独立したlog-stretch走査でrest縦波比最大約164.6、初期受理目安768 speed/dxに対して最小刻み限界50000だった。float実装・低レベルAPI端点・runtime挙動は未検証。ビルド未実行。
- 2026-10-12: ArtifactMpmSand2DLayer.cppmがpacket定数色生成でFloatColorを直接使うようになったため、Color.Floatをimplementation moduleへ直接importした。transitive importに依存しないモジュール境界に修正。build未実行。
- 2026-10-12: MPM Sandの作成導線を既存Particleサブメニューから2D Layerメニューへ移し、専用の砂山シルエットiconを追加。MPM material layerとして既存emitter particle項目と視覚・分類を分ける。メニュー操作とicon表示のruntime確認は未実施。
- 2026-10-12: GPU surfaceはCPU software pathと同じdensity-grid三角形分割・頂点alphaを使い、3頂点すべてがalpha 0の三角形だけを省く。最大値は`130*98*6 = 76,440`頂点。位置+alphaの3 float vertexを2本reserveするworst-case scratchはLayerごとに約1.75 MiB。GPU shader ABIは既存`RectVertex`変換後のまま。数式上の補間は揃えたが、実画素parityとpacket lifetimeはruntime計測が残る。
- 2026-10-12: MPM seek snapshotは粒子（F/Fp/C・塑性硬化を含む）、破断index、grid origin、solver accumulator、粒子grid寸法を保存し、固定のsolver grid geometryはrestore先との一致を検証する。gravity/colliderはsnapshot stateではなく、両経路で各step直前にframe評価から再適用される。現行保存項目に静的な欠落は見つからないが、snapshot round-tripと再生・seek数値一致はruntime未検証。

- 2026-10-12: `applyGranularPlasticity()`はHencky return-map後に`FpNew=FeNew^-1 F`、`Fnew=FeNew FpNew`で`F=Fe Fp`を保ち、plastic multiplierをhardening stateへ蓄積する。replay失敗はrestore/baselineの事前検証で返り、replay進行開始後のfailure returnはない。構成則のstress/return-map受入fixtureと実solver replayは未検証。
- 2026-10-12: CPU MPM surfaceのsoftware resampleをGPU meshと同じ対角線の二triangle piecewise-linear alpha補間に揃える。CPUだけbilinear補間し、さらにfield頂点alphaを事前8-bit量子化していた差を除く。GPU pixel parity/輪郭改善は実画素runtime未確認。
- 2026-10-12: 公開Timeline seekは`goToFrame()`からGranular replayを呼ぶが、その戻り値を無視していた。solverが存在するCompositionでreplay失敗なら、呼び出し前のframeへLayer/positionを戻し、Soft Bodyとrigid authored state、collision/joint評価も復旧する。seek failure fixtureは未検証。
- 2026-10-12: 大seek/巻戻し時にGranular replayが失敗しても`requiresReplay`中でないとComposition frame rollbackしない経路を修正。対象worldにsolverが存在する場合はreplay失敗を常にseek失敗として扱い、previous frameとSoft Body snapshotへ戻す。checkpoint/baseline破損時の実動作は未検証。
- 2026-10-12: frame-rate変更処理が全Layerへ同じCompositionをsetCompositionしても、MPM Layerの同Composition bindで有効なsolverをdetach/reseedしないようにする。baseのsetCompositionはTransform3D base/variantのtime scaleのみを更新し、live particle stateとforced anchorを保持する。レジストリにsolverがない場合だけrebind recoveryとして再構築する。frame-rate変更UI/runtimeは未検証。
- 2026-10-12: replay target frameでLayer評価後にcollision proxyとgravityを同期して次の通常stepへ渡し、pinned rate-change anchorより前へseekした場合はanchor pinを失効させる。これでアンカー後のstep入力と別branchへ戻った後のcache保持を整理する。runtime seek検証は未実施。
- 2026-10-12: frame-rate変更後のlive solver状態を保持するため、強制captureした現在frameをseek replay anchorとしてpinする。通常checkpoint数の上限とは別枠で1 anchorを保持し、byte budget evictionでもanchorを飛ばす。budget内にanchorと追加snapshotを保持できない場合はcaptureを失敗させ、既存のbaseline replay fallbackを使う。runtime予算圧迫・frame-rate変更後seekは未検証。
- 2026-10-12: `mpmSand.gravityY` のanimatable flag/expression/keyframes/envelopesをMPM layer JSONへ個別保存・復元する。通常のLayer property serializer対象外であるMPM固有プロパティの保存経路を補完。scalar `mpmSandGravityY`との併存、project duplicate、Undo/Redo、expression評価のround-tripはruntime未検証。
- 2026-10-12: Projectのlayer duplicate helperは既存property cacheへserialized値を直接書くため、MPM Gravity private authoring scalarが複製先で既定値のまま残り得た。Gravity propertyを復元後、その値をMPM layer setterにも適用し、solver authoring state・scalar JSON・property cacheを揃える。duplicate後のsolver予算失敗時と再読込はruntime未検証。
- 2026-10-12: `SetLayerPropertyKeyframesCommand`は共通`applyLayerPropertyKeyframeSnapshot()`で同一Layer property cacheのkeyframesとbefore/after animatable flagを更新する。Gravity cacheはconstructorで常時生成されるため、Undo/Redoでproperty identityが変わる静的経路はない。再生・seek・Undo/Redo後のsolver評価一致はruntime未確認。
- 2026-10-12: `SetLayerPropertyValueCommand`はundo/redo時にMPM setterを呼ぶが、以前はsetter成功後にGravity以外のscalar cacheをcanonical内部値へ同期していなかった。MPM全parameter pathについてsetterの同値成功とsolver再構成成功後にcacheを同期し、Undo/Redoおよび直接setter経路の表示値を揃える。実UI・Undo runtime確認は未実施。
- 2026-10-12: Material設定変更で`PhysicsSystem::setGranularMaterialSolver()`がworld単位snapshot mapを消去し、shared clock remainderを0へ戻し、登録済みsolver全体のcache nodeを作り直した後で対象solverとbaselineを差し替える。異なるmaterial設定のcheckpoint混在を防ぐ静的経路を確認。変更直後の複数MPM Layer再生・seek再現はruntime未確認。
- 2026-10-12: ただし短い通常再生はlive frameから直接stepし、設定変更後の新baselineをcurrent composition frameまでreplayしないため、seek replayと状態が分岐し得た。Granular world clockにreplay-required flagを追加し、次のincremental composition advance前に前frameまでbaseline replayする。失敗時はComposition frameを前frameへ戻してstepを止める。replay-required中に大seek/巻き戻しreplayが失敗した場合も前frameへrollbackする。step-count比較・長frame・複数solverのruntime一致は未確認。
- 2026-10-12: 1つのGranular solverをunregisterするとworld checkpoint全体を消す一方、残solverのlive状態とclockは保持されていた。seek cache missはbaselineから再生するため通常再生と分岐し得る。残solverがあるunregisterでもreplay-requiredを立て、次の通常進行前にbaseline replayする。残Solverの削除後play/seek runtimeは未確認。frame-rate変更のlive-state保持契約は別経路のため維持。

- 2026-10-12: Granular CPU grid wall frictionを接触中だけのCoulomb速度制約へ変更。法線進入速度を除去し、接線速度変化を`μ|Δv_n|`以内に制限する。離反・非接触では接線速度を保持し、Corotated式は維持。4壁、corner、CFL step数変化でのruntime検証は未完了。
- 2026-10-12: gravity座標契約をTransform経路と照合。P0はComposition画面座標の+Y重力だが、粒子/境界はLayer local、solver gravityは生成時にlocal `(0, gravityY)`固定で、frameごとに更新するGranular bridge APIはない。よって非identity transformでgravityの方向/画面上の大きさがP0と一致しない可能性がある。GravityをComposition-worldとするかLayer-localへ契約変更するか、scale時の物理単位・粒径の扱いを判断する。変換fixtureとruntime確認は未実施。
- 2026-10-12: Composition-world gravity契約を実装。`evaluateLayerCollisionPairs()`で各Granular solverのframe時刻に評価したgravity `(0, gravityY)`を取得し、当該frameの評価済みLayer global transformのinverse線形部でsolver localへ写してからsolverを進める。Gravity値はLayer frame rateでキーフレーム補間し、expression/envelope/external overrideも評価する。gravity property cacheはLayer constructorでcold path初期化し、JSON復元後とGravity setter成功後にprivate authoring値へ同期するため、Inspector未表示や直接setter経由でもCompositionが古い値を読まない。translationは相殺し、静止した任意の可逆affine transformではCompositionへ戻した加速度が`(0, gravityY)`となる。非affine／特異／非finite transform、またはlocal結果がfloat範囲外ならsolver gravityを0にする。particle spacing、domain寸法、stress/densityのsolver-local意味論は引き続きTransformで画面変化するため、物理的な長さ・材料応答の不変性は保証しない。重力animation、再生/seek実fixture、animated transformの非慣性項、Transform変更境界、ゼロgravity fallbackは未検証。
- 初版Transform受入契約: 静止Transformでは `A * (A^-1 * (0, gravityY)) = (0, gravityY)` をtranslation、rotation、反転、一様scale、非一様scaleで数値確認する。Transformをアニメーションした場合はsolver localの粒子状態を維持したまま描画・collision proxyを各frameのaffine Transformで写し、gravityだけを同frameの逆線形変換で設定する。Gravity自身のkeyframe/expression/envelopeもLayer frame timeで評価し、通常再生・seek replayが同じ値を使うことを検証する。Layerの並進／回転加速度に対する慣性力、moving collider速度伝達、非一様scale下の等方的な粒径・応力は初版保証外とする。通常再生とseek replayで同じframe列の粒子状態が許容誤差内で一致すること、非可逆／非affine transformでgravityが0へfallbackすることをruntime gateとする。
- Transform同期実装: Compositionは各frameのLayer評価済み`QTransform`とinverseを得てcollision proxyをsolver localへ変換する。同じinverse線形変換を`setGranularMaterialGravity()`経由のsolver `setGravity(gx, gy)`へ適用する。translationはベクトル変換に含めない。非一様scale下の粒径/material responseはsolver local契約のままで、gravityだけの逆変換は完全なworld-space物理を意味しない。通常再生／seek replay両経路へ共通評価関数を使う。
- Gravity同期の時間経路: 通常再生は`setFramePosition()`の各stepでcollision pair評価後に`advancePhysicsFrame()`を呼ぶ。seek replayはsnapshot restoreまたはbaseline reset後、各frameのlayer `goToFrame()`→collision pair評価→Granular advance→snapshot captureを行う。snapshotにgravity値は含まれないが、両経路とも各Composition frameのstep直前にposeとgravityを再評価し、そのframeに属するMPM内部substep中は同一gravityを使う。フレーム番号ごとの適用順は静的に一致する。animated rotation、transform modifier、seek/replay数値一致、frame境界の力積差はruntime未確認。
- 型境界: `docs/planned/MILESTONE_TYPED_COORDINATE_SPACES_2026-09-27.md`はComposition/Layer vectorを型分離する方針だが、MpmSolver2D gravity APIと現在のbridgeはraw floatで、物理加速度（px/s²）の専用型は未導入。CompositionではQTransformの点差からベクトル変換を明示している。今後の座標強型化ではこのbridge/solver境界を候補に含める。
- 2026-10-12: Granular Plane/Circle/Polygon/Box collider摩擦を法線接近時のみ適用し、反発を含む法線速度補正量`Δv_n=-(1+e)v_n`から接線変化のCoulomb上限`μΔv_n`を計算する。離反粒子の接線速度・APIC行列を摩擦で縮小しない。Corotated応答は維持。形状ごとの接触runtime検証は未完了。
- 2026-10-12: Polygon頂点と粒子位置が一致する場合、単位外向き隣接法線とsigned turnから凸／凹を判定する。凸頂点は両面進入時に二等分法線、片面進入時はその面法線を使い、凹頂点は速度に対して最も進入成分が強い面法線を使う。Granular接触解決ではtwice-areaが1e-12以下の輪郭を除外し、projectionと同じ長さ1e-6以下の辺を法線候補にしない。CW/CCW・凹頂点・退化形状のruntime fixtureは未完了。


- 2026-10-12: Granular `MpmCollider2D::Plane` をComposition画面座標（+Y下向き、gravityY=+980）に合わせ、horizontal floorとして `particle.y > plane.y` を検出し`-Y`法線で位置/速度補正する。`vY>0`の接近は`vn<0`となり反発後`vY'=-e*vY`、離反`vn>0`は再反射しないことを式で静的照合。従来Corotated MPM Planeのy-up条件は維持。Plane colliderのGranular接触fixtureは未実施。

- 2026-10-12: 動的collision sourceはComposition frameのposeを各stepで追従するが、`MpmCollider2D`とGranular接触応答はcollider velocity/previous poseを持たない。したがってpenetration位置補正はできても、移動床・壁のlinear velocityや回転体の接触点速度を粒子へ伝えないposition-only kinematic contactである。平行移動/回転colliderの接触速度をfixture化し、初版でmoving-platform運動量伝達を要求するか判断する。runtime未確認。

- 2026-10-12: Composition collision pair評価は通常再生stepで `Layer::goToFrame()` が走らないため、layer-current-frameをcache keyにする `getGlobalTransform()` ではtransform animation後も古いglobal transformを返す可能性があった。collider候補のtransformを各 `position_.framePosition()` に対する `getGlobalTransformAt()` で評価するよう変更し、target inverse/source outline mappingも同じtransformに揃えた。通常再生vsseekのanimated collider接触と追加CPU costは未検証。

- 2026-10-12: Multi-selectionの設定commitは未原子的。PropertyWidgetはtarget layerを順に更新してsetterのfalseを無視するため、粒子/grid/checkpoint予算によってMPM solver再生成がtargetごとに異なる結果となる場合、一部だけ更新される可能性がある。代表layerのcache/editorはその代表layerの値を表示する。安全に全体rollbackするには旧粒子状態/checkpoint/clockの保持またはworld単位の事前予算reservationが必要で、単純rollbackはsimulation historyを復元しない。runtime未確認・未修正。

- 2026-10-12: Property EditorのMPM Sand `mpmSand.*` parameter previewをcommit-onlyに変更。shared rowはpreview候補を先にproperty cacheへ書き込み、void handlerでは拒否結果を返せないため、solver再生成失敗時にeditorだけ候補値を表示し続ける恐れがあった。preview中はsetterを呼ばず状態を保持し、commit時に一度だけsolverを再seedする。拒否時は既存canonical cache比較でeditorを戻す。連続preview、拒否commit、Undo/Redo、multi-selectionのruntime確認は未実施。

- 2026-10-12: MPM gravity transformのテスト責務を既存fixture配置と照合。`tests/ArtifactCore/PhysicsDeterminismTest.cpp`にはSoftBody・Fluid・SandSim2D fixtureがあるが、MpmSolver2D/Granular MPMのfixtureは存在しない。逆線形変換の数値検証だけでなく、Compositionの通常再生・seek replayが同じframe transformをsolverへ適用する結合fixtureが必要。テスト追加と実行はリポジトリ指示によりユーザーが明示するまで行わない。
- Gravity propertyのallocation経路: 普通の非roving keyframeはallocation-free `keyFrameInterpolationSegment()`の固定segmentからscalar補間する。キー範囲外は追加した`keyFrameBoundaryValue()`が端点scalarをallocation-freeで返す。既存segment APIの戻り意味とShape Gradientのキャッシュ判定は変更しない。明示的なexpression/envelopeでは既存`AbstractProperty::evaluateValue()`、roving keyframeでは既存`interpolateValue()`へfallbackし、いずれもproperty内部vector copy/ExpressionEvaluator由来のframe-time allocationがあり得る。発生は当該featureをGravityへ設定したLayerごと・評価frameごとに限られるが、代替のallocation-free完全評価APIは現状ない。profile/runtime検証が必要。
- 2026-10-12: Gravity scalarのBezier、Catmull-Rom、Hermite、および標準easing補間式を既存`KeyframeInterpolator::evaluate()`と静的照合。segmentの`alpha`、隣接値、before/after spanの定義が各式に対応し、範囲外は端点値、Constantは区間始値を使う。C++ compile、 roving/expression/envelope評価、実フレームの数値一致は未検証。
- 2026-10-12: 構成則のfixture不足を受け入れ条件へ具体化。既存 `CoreDiagnostic.Test.cppm` にMpm専用fixtureは見つからず、CMake/test追加・実行はAGENTSの明示指示待ち。許可後のfixtureでは、(1)静水圧圧縮は無投影、(2)extension tipは`H=0`、(3)対角塑性例は`tr(H)=tr(ε)`かつyield residual≈0、(4)回転付き非対角`Fe`で特異値・主応力・`P=τFe^{-T}`をbasis変換参照と比較、(5)`Fp_new=Fe_new^{-1}F`後の`Fe_new Fp_new≈F`、(6)determinantと全要素のfiniteを確認する。代表基準数値の独立double算術照合は完了したが、float実装とC++ module経路の検証は未実施。

- 2026-10-12: frame-rate変更ではGranular world checkpointを破棄するだけでなく、対象Compositionのouter world-clock remainderも0へ戻す。frame位置→秒対応が変わった後に旧時刻余りを新frame deltaへ持ち込まない。通常のsnapshot invalidationおよび全体Physics timeline設定変更ではworld間の副作用を避けてlive granular clockを保持する。rate変更前に非ゼロremainderがあるreplay/runtime一致は未検証。
- 2026-10-12: frame-rate変更時はlive solver stateを保持し、現在frameのGranular checkpointをintervalに関係なく強制captureする。snapshot予算などでcapture不能ならreplay-requiredを残し、次incremental advanceでbaselineからcurrent frameまで再計算してseekと揃える。frame-rate変更後のseek先が現在frameより前の場合はbaseline under new rateで再計算する。予算圧迫・eviction・fps変更後の往復runtimeは未確認。

- 2026-10-12: Hencky-Drucker-Prager基準例`μ=1, λ=1.5, φ=35°`, `ε=(-0.5,0.3)`を独立double算術で照合。実装式と一致し、`α=0.4055797877`, `δγ=0.3628955311`, `H=(-0.2433941091,0.0433941091)`, `tr(H)=-0.2`、yield residualは約`-5.6e-17`。`firstPiolaKirchhoff()`の主応力/特異値変換も`P=τ Fe^{-T}`と一致する静的式確認。C++ float、回転・非対角Fe、`F=Fe Fp`誤差、MPM runtime安定性は未検証。
- 2026-10-12: Granular force assembly now applies the multiplicative-decomposition chain rule `P_total=P_e Fp^{-T}` after `P_e=firstPiolaKirchhoff(Fe)`, because `Fe=F Fp^{-1}` while grid force gradients differentiate total `F`. The change is scoped to Granular; Corotated behavior is unchanged. Analytic-vs-finite-difference force, non-identity `Fp`, and runtime effects remain unverified.
- 2026-10-12: Granular CFL wave speed now uses an all-direction 2D Hencky acoustic-tensor Gershgorin upper bound computed from each particle's elastic singular values, with the rest-state longitudinal speed as a floor and fallback if the state singular-value/upper-bound calculation is invalid. Independent double probes found no underbound across 21×21 stretches in `exp([-3,3])` at 721 directions and, across the Layer's five Poisson-ratio probe values `-0.9,-0.5,0,0.3,0.45`, 13×13 stretches in `exp([-2.5,2.5])` at 361 directions each. `Fp` cancels from the current-space acoustic contraction for fixed plastic state. The CFL path uses scaled-Gram singular values without principal-direction trigonometry or heap allocation; plastic update, transfer, collision stability, C++ float parity, and per-substep CPU cost remain unverified.

- 2026-10-12: 設定変更の静的経路を確認。MPM Sandの物性・重力・seed grid設定を編集中に変えると`configureSolver()`が新solverを作り、PhysicsSystemは同じworld/Layer entryを置換してgranular snapshotとworld clockをresetする。結果として既存粒子の変形・速度・堆積履歴は初期seedへ戻る。作成失敗時はLayer値/property cacheをrollbackする。これは旧物性の履歴を混ぜない一方、property preview中の再seedと操作意図が未確認。preview/commit、multi-selection、Undo/Redoのruntime受入れ、および物性変更時に現粒子状態を保持すべきかの製品仕様判断が残る。

- 2026-10-12: Compositionの通常再生step後にもGranular snapshot captureを呼び、既存の12 frame間隔・共通checkpoint条件・world byte budgetを通して再生中checkpointを保持する。従来はseek replay側だけがcaptureしており、通常再生後のseekがbaselineから再計算していた。再生→seekの数値一致とcheckpoint hitは未実行。
- 2026-10-12: CPU surface直描きの密度alphaを既存`toQImage()`境界と同じ8-bit sRGB RGBA精度へ先に量子化してから拡大補間し、出力前にalphaとsand RGBのbyte値をpremultiplyするよう整合。Qt `SmoothTransformation`との差分と半透明pixel parityは実画像比較未実施。
- 2026-10-12: MPM GPU surfaceを各quadのalpha頂点meshからdensity 0.08でclipしたtriangle meshへ変更。元の2 trianglesを三角形半平面でclipし、交差点のfield座標とalphaを同じedge interpolation factorで求めて残りpolygonをfan triangulationする。1quad最大12 vertexをcold path reserveし、frame-time heap allocationを追加しない。GPU輪郭品質、CPU parity、最大buffer容量は未確認。
- 2026-10-12: GPU surfaceのdensity threshold clippingを廃止し、元の各三角形と頂点alphaを維持する方式へ変更。CPU software pathと同じpiecewise-linear alpha fieldになり、全頂点alphaが0の三角形だけをcullする。実画素輪郭・GPU/CPU pixel parityは未確認。
- 2026-10-12: GPU Composition ViewのMPM `draw()` はsafe layer opacityが0の場合、density field rasterとsurface mesh走査の前にreturnする。描画不能なsurfaceで最大12,513 quadのframe-time traversalを省く。透明状態のGPU captureと処理時間は未確認。
- 2026-10-12: Composition collision evaluation now reuses a Composition-owned `ArtifactArray<ArtifactAbstractLayerPtr>` scratch for active collision/material candidates instead of constructing and reserving a local `std::vector` on every fixed step. Capacity grows only when layer count exceeds the historical maximum, and remove/remove-all clears retained layer pointers while preserving scratch capacity; pair selection and evaluation order are unchanged. Allocation profiling and replay/collision runtime parity remain unverified.
- 2026-10-12: Removed the unused per-step collision-pair bounds scan, QString key creation, and QSet tracking from `evaluateLayerCollisionPairs()`. The pair set was only read by an empty branch and had no consumer outside this method; active material collider clear/transform/register work remains intact. Physics/event regression is not runtime-verified.
- 2026-10-12: Composition now reuses one polygon collider scratch, and `MpmSolver2D` recycles polygon-point vector capacities across `clearColliders()` / `addCollider()` while preserving the active `colliders()` vector contract. First-use and new-peak collider counts/outline sizes can still grow buffers; allocator profiling and collision/replay parity remain unverified.
- 2026-10-12: Added caller-owned `appendCollisionOutlineLocalPoints()` and a Composition `NamedVector<QPointF>` scratch for collision-source outlines. Authored path/polygon and multi-content path/polygon points append directly into retained storage; generated primitive outlines still use the existing temporary `buildRenderablePoints()` vector. Runtime behavior and allocation profiling remain unverified.
- 2026-10-12: Shape collision outlines for built-in primitives now append directly to caller-owned `NamedVector` storage, including rounded shapes and complex presets; collision-only sampling duplicates the established point formulas while normal rendering keeps `buildRenderablePoints()`. This removes per-step sampled-outline temporary vectors after scratch capacity stabilizes, but point-for-point parity and runtime allocations are unverified.
- 2026-10-12: `MpmSolver2D::addCollider()` now caches polygon vertex AABBs, and collision response skips point-in-polygon plus closest-edge traversal for particles outside those bounds. Granular culling expands the AABB by the existing 1e-5 boundary-contact tolerance. The vertex walk remains once per collider registration; high particle/vertex-count speedup and boundary parity are unmeasured.
- 2026-10-12: Polygon collision queries now combine ray-crossing containment and closest-point/edge selection in one vertex pass. `addCollider()` also caches signed doubled area, removing the extra per-contact winding scan for exact-boundary granular particles. AABB-interior particles still require one full polygon pass; numerical/contact parity and timings are unverified.
- 2026-10-12: Code review confirmed that collision resolution projects every inside particle to its nearest polygon boundary point, so point-in-polygon early-out alone would change deep-penetration response. Exact nearest-edge work remains O(vertices) per particle inside the collider AABB; acceleration beyond the new cull/fused query needs measured demand and a spatial edge structure.
- 2026-10-12: Collider response keeps its particle→collider traversal. `addCollider()` precomputes legacy clamp values and Granular finite/clamp values once at registration, removing repeated per-particle coefficient sanitization without changing each particle's collider order. State parity and step-time impact remain unmeasured.
- 2026-10-12: Composition collision candidates now snapshot enabled/shape/offset/restitution, material-target flags, global transforms, and source world bounds once per evaluation. Target inverses are also computed once, and polygon outline generation is lazy and once per source while collider registration still arrives in the same source order for each target. Per-target outline mapping remains necessary; runtime parity and timing are unverified.
- 2026-10-12: Polygon outline mapping now composes source offset→global→target inverse transforms once per source-target pair for affine transforms, then maps each vertex once instead of invoking nested `map()` twice. Projective transforms retain the previous nested mapping path. Non-finite mapped or float-converted vertices reject that collider before registration, matching the solver's finite-polygon boundary. Floating-point boundary-contact parity remains runtime-unverified.
- 2026-10-12: Composition collision candidates now invalidate source bounds when any global-transform matrix coefficient is non-finite, and skip target-space AABBs with non-finite coordinates or dimensions before collider construction. This prevents malformed transforms from reaching solver AABB/contact math; behavior under corrupted transforms is statically reviewed but runtime-unverified.
- 2026-10-12: Material-target inverse transforms are now accepted only when `QTransform::inverted()` reports success and all nine inverse coefficients are finite. This prevents an invalid inverse matrix from being used in bounds mapping even when the API's invertibility flag alone is insufficient; malformed/extreme transform behavior is runtime-unverified.
- 2026-10-12: MPM collider registration now rejects non-finite and ±1e18-exceeding Plane/Box/Circle/Polygon coordinates and extents, bounding float edge-length, squared-distance, and center±radius/half-extent operations. Polygon signed doubled area is accumulated in double relative to the first vertex, reducing cancellation for polygons far from the origin, then saturated to finite float range for the cached winding test. Extreme-coordinate rejection and translated-area winding behavior are statically reviewed but runtime-unverified.
- 2026-10-12: `MpmSolver2D::setBoundary()` now computes normalized boundary width/height in double and rejects spans larger than `float` can represent before enabling particle clamping. This prevents finite endpoints with an overflowing float difference from creating an active boundary; oversized-boundary behavior is runtime-unverified.
- 2026-10-12: `PhysicsSystem::createGranularMaterialGrid()` now validates float-converted domain endpoints and their actual spans before any checkpoint eviction, then checks `hasBoundary()` after applying that same validated domain. This prevents a successful solver without its authored clamp and avoids mutating retained seek history for an invalid tiny/collapsed domain. API failure behavior is runtime-unverified.
- 2026-10-12: Granular grid dimensions and seed containment now use the same float-rounded boundary spans/endpoints that `setBoundary()` receives. This prevents a rounding-inward domain edge from leaving the seed outside its effective boundary or sizing the grid from a subtly different extent; endpoint-rounding cases remain runtime-unverified.
- 2026-10-12: Granular solver creation now computes the actual float `gridOrigin` and the solver's float safe stencil range (`origin + 2 cells` through `origin + (gridSize - 2) cells`) before checkpoint trimming. It rejects domains the rounded grid cannot cover with its full quadratic-kernel margin, and passes the same origin values into `setGridOrigin()`; large-offset/small-cell behavior remains runtime-unverified.
- 2026-10-12: Added `calculateMpmParticleGridSeedGeometry2D()` in the existing `Physics.Mpm2D` module and routed both `MpmSolver2D::addParticlesGrid()` and PhysicsSystem's pre-trim validation through it. Seed spacing, endpoints, total/particle volume, and particle mass now share one float-operation-order implementation; PhysicsSystem still checks the returned extrema against its rounded domain/stencil limits. Build and runtime parity remain unverified.
- 2026-10-12: CPU Render QueueにMPM専用surface経路を追加。初期実装は128×96密度fieldを最終Layer寸法へbilinear samplingしたが、後続変更でGPU meshと同じ固定対角線の二triangle piecewise-linear alpha補間へ置き換えた。linear sand colorをsRGB化して再利用可能なpremultiplied `QImage` bufferへ直接出力する。2枚のdetached bufferをmatte prepass/final pass向けに保持し、通常thumbnail経路とgeneric format conversionを迂回する。色・alpha parity、画素寸法、buffer再利用、複数worker計測は未検証。
`createGranularMaterialGrid()` は Layer authoring contract と同じ columns 2..500 / rows 2..200 / 合計100,000粒子を受理し、範囲外をrejectする。旧factoryの512×512 clampはUI上限より多い粒子を黙って作るため除去した。API境界・最大数fixtureは未実施。
- 2026-10-12: Undo/Redoの共通property適用でMPM setterがsolver再生成を拒否した場合、汎用キャッシュ値fallbackを適用せず失敗として返すよう修正。通常Property Editor preview/commitと合わせ、拒否値がsolver authoring state/cache/UI/Undoへ混入しない静的経路を確保した。Undo/Redo runtime確認は未実施。
- 2026-10-12: MPM Sand layer name, creation tooltip, and ten material-property labels/tooltips are now present in Arabic, German, Spanish, French, Korean, Portuguese, and Russian locale JSON, in addition to the existing English, Japanese, and Chinese entries. The layer-menu translation generator/mapping now lists the same keys. JSON parse/key-presence checks passed; locale UI captures and clipping checks remain unrun.
- 2026-10-12: Composition attachment now checks a virtual Layer readiness hook after `setComposition()` and before Top/Bottom/Index registration. MPM Sand reports ready only when its solver exists in the target runtime world; failed attachment detaches and returns `LayerInitializationFailed`, and JSON restoration omits failed Layers from parent resolution. Project creation no longer marks the project dirty when append fails. Build/module integration and failure-path runtime behavior remain unverified.
- 2026-10-12: Granular solver reconfiguration now includes the existing same-Layer solver/grid in transient Composition budgets. It reserves requested live+baseline payload before allocation, then rechecks actual live capacity plus requested baseline size before baseline copy and actual live+baseline capacities before commit, evicting old checkpoints as needed. This bounds replacement payload while preserving the old solver on rejection; allocator overhead and runtime behavior remain unverified.
- 2026-10-12: Granular checkpoint capture now reserves live particle capacity before copying, then reserves each snapshot's actual particle/fracture capacities before publishing it. A scope guard removes partial entries for the frame from every solver cache on early return or exception. Exception propagation handling and allocator-capacity behavior remain unverified.
- 2026-10-12: Granular snapshot validation now rejects any fracture-index payload; Granular mode disables fracture and `checkFracture()` returns without adding indices. This keeps restore from allocating an unbudgeted fracture array and leaves Corotated fracture snapshots unchanged. Snapshot mutation fixtures remain unrun.
- 2026-10-12: MPM GPU substep dispatch now rejects the Granular Drucker–Prager model at the dispatch boundary as a CPU fallback safeguard, in addition to the existing backend/model setters refusing that combination. GPU/CPU runtime routing remains unverified.
- 2026-10-12: `MpmSnapshot2D` now records solver grid cell size and node dimensions; at this stage `MpmSolver2D::canRestoreSnapshot()` required the receiving solver's exact grid geometry. Granular snapshots also require active particles positioned inside the quadratic-kernel-safe grid range. The exact-geometry condition was later relaxed only for Granular solver snapshots within pre-reserved node capacity (see the 2026-10-12 grid-expansion entry). Snapshot mutation/restore fixtures remain unrun.
- 2026-10-12: Granular snapshot validation also requires particle count, mass, reference volume, and active state to match the receiving solver's seeded particles. These properties are fixed for the lifetime of a granular solver; rejecting altered values prevents a restored snapshot from changing density and therefore the material wave speed used by the CFL bound. Snapshot mutation/restore fixtures remain unrun.
- 2026-10-12: Granular seek now scans the anchor Layer's frame-ordered checkpoint registry from newest to oldest and stops at the first checkpoint valid for every solver in the world. This preserves common-frame/world-clock validation while avoiding repeated full-particle validation of older checkpoints on the normal seek path. Seek cost and invalid-checkpoint fallback remain unmeasured.
- 2026-10-12: Rejected MPM Sand parameter edits now restore the cached property value along with the authoring field when parsing, range validation, or solver recreation fails. The Property Editor resynchronizes its preview/commit editor from the property value after handlers run, preventing a rejected edit from appearing accepted or recording a value change that the solver did not apply. Multi-selection and Undo behavior remain runtime-unverified.
- 2026-10-12: MPM Sand parameter setters now no-op when the parsed value equals the current authoring value. A slider preview that already rebuilt the solver is therefore not followed by a second identical solver recreation at commit; repeated property notifications also avoid needless reseeding. Preview cost during continuous value changes is still unmeasured.
- 2026-10-12: Granular outer-clock step budget now scales with queued elapsed time up to 1024 steps per call. The previous fixed 64-step cap could not process a 1-second composition frame at the allowed 1 ms physics step, accumulating 936 ms per frame; 1024 covers the full 1-second frame while preserving bounded catch-up for larger stalls. Low-FPS performance and oversized-stall recovery remain unverified.
- 2026-10-12: Granular constitutive pathの2×2 positive SVDをscaled Gram行列で計算し、largest singular valueはscaled eigenvalue、smallest singular valueはscaled determinantから復元する。元スケールの`det(F)`を先に計算せず、有限成分の大きなほぼ等方伸長での二乗・determinant overflowと固有値差し引きによる小特異値の消失を避ける。基準3状態のprojection式を独立算術で照合しyield residualは丸め誤差内だったが、実装fixture、条件数の高い圧縮、構成則への誤差伝播は未確認。
- 2026-10-12: PhysicsSystemのtimeline fixed step入力がNaNの場合、`std::clamp`だけではNaNが残りGranular world clockの比較が止まるため、1/60秒へfallbackしてから既存範囲へclampする。通常設定・不正設定のruntime経路は未検証。
- 2026-10-12: Granular solver生成時に、API本体がparticle volume/massを計算するfloat式とseed centerを事前検証し、粒子数と各seedのposition/mass/volumeが有効なことを登録前に確認する。doubleで範囲内でもfloat演算で初期粒子が作れない場合に空solverを登録する経路を閉じる。極端なサイズ境界とallocator挙動は未検証。
- 2026-10-12: Granular seed粒子は各セル代表点としてセル中心へ置き、spacingを`width/columns` × `height/rows`に合わせる。particleVolume=`width*height/(columns*rows)`と一粒子当たりの初期代表面積が揃い、低解像度でも端点重複による代表体積ずれを抑える。CFL cell sizeはこのspacingの小さい方（1px floor）とする。既存Corotated MPMの端点配置は維持する。粒子数別の質量分布・pile挙動は未検証。
- 2026-10-12: `MpmSolver2D::addParticlesGrid()`も導出したspacing、seed端点、総volume、particle volume/massの有限性・正値をvector reserveとgrid metadata更新より前に検査する。直接API呼び出しでfloat overflow由来の不正粒子や部分初期化を残す経路を防ぐ。割当失敗・極端値のruntime挙動は未検証。
- 2026-10-12: Granular density raster uses an axis-aligned elliptical kernel when density pixels have different world-space width/height. Each axis uses the same `hypot(pixelWorldWidth, pixelWorldHeight)` support floor with a tiny strict-coverage epsilon, then applies its own 8-pixel ceiling, so extreme aspect ratios do not shrink the long-pixel support below sample coverage. Per-particle peak density is normalized against discrete kernel weights sampled inside the field, including clipping at field edges, using a fixed 18×18 local stencil and one distance evaluation per candidate sample. Before accumulated-density saturation, each isolated particle's discrete contribution integrates to its current volume. Only accumulated per-pixel density is capped at one. This prevents high-count particle supports from missing sample centers and reduces phase/edge variation in discrete volume contribution. Anisotropic appearance, rendered quality, and overlap saturation remain unverified.
- 2026-10-12: Granular Plane and Box collider contact now route through the same outward-normal response as circle/polygon: only inward normal velocity receives restitution and friction scales the tangent component. The previous axis response reflected `abs(vNormal)` even for particles already moving away from a collider surface. Corotated response remains unchanged. Contact velocity fixtures and multi-collider order remain unverified.
- 2026-10-12: Granular collider friction/restitution now map non-finite coefficients to zero before contact response, preventing NaN values from surviving `std::clamp` and contaminating particle velocity. The Corotated coefficient path is unchanged; malformed-coefficient runtime behavior is unverified.
- 2026-10-12: `MpmSolver2D::addCollider()` rejects non-finite required geometry, invalid collider enum values, and Polygon outlines with fewer than three vertices, odd coordinate count, or non-finite coordinates before storing them. Friction/restitution remain separate so Granular can apply its zero fallback. Invalid collider rejection and valid collider compatibility are not runtime-tested.
- 2026-10-12: Granular snapshot validation now rejects plastic hardening history outside the solver invariant `[0, 100]`, alongside invalid deformation determinants, before copying snapshot state into the live solver. This prevents a malformed snapshot from introducing negative or out-of-range hardening into a restored run; valid replay parity is unverified.
- 2026-10-12: Granular collider response now projects APIC affine velocity matrix `C` onto the contact tangent plane and applies the same tangent friction retention as particle velocity. Particle-level domain clamps also remove the clamped normal output row from `C`; Corotated response is unchanged. This prevents a post-G2P position/velocity correction from leaving a normal affine term that P2G could re-inject next step. Wall/collider momentum and friction response remain runtime-unverified.
- 2026-10-12: Granular CFL characteristic speed now includes a conservative APIC affine contribution `1.5*sqrt(2)*dx*||C||F`, bounding `|C*dpos|` across the active 4×4 stencil in addition to particle-center speed. This removes excess conservatism while still bounding shear/rotation fields whose center velocity is small. CFL preflight repairs non-finite position/velocity, clamps positions to the complete quadratic-kernel support range before integer node indexing, repairs invalid `F`/`Fp`, non-finite APIC `C`, and out-of-range hardening state before transfer. Support-bound and stable-step acceptance are unverified.
- 2026-10-12: If the required Granular CFL timestep would fall below the solver API floor of 1e-6 s, the solver now proportionally scales only particles whose center-plus-affine advection bound exceeds the floor-compatible speed, with a 0.1% margin. This prevents `setTimeStep()` from silently clamping to a timestep that violates the computed CFL bound; the emergency velocity/affine-state loss path is runtime-unverified.
- 2026-10-12: Surface extraction retains each of the two original field-quad triangles and its smoothstep vertex alpha; it omits only triangles whose vertex alphas are all zero. Edge samples clamp to the nearest density pixel. CPU/GPU use the same piecewise-linear field and diagonal, but GPU pixel parity and contour quality remain runtime-unverified.
- 2026-10-12: GPU MPM surface packets use the shared solid-triangle pass-through color shader with back-face culling disabled and straight-alpha blending; the default render target is `RGBA8_UNORM_SRGB`. CPU thumbnail converts its `linearStraightRgba32Float` surface through the shared pixel conversion to sRGB straight RGBA8, then premultiplies for CPU queue composition. These paths have compatible transfer/alpha intent at the default target, but alternate target formats, blend accumulation, and pixel parity remain unverified.
- 2026-10-12: CPU MPM thumbnail sizing now fits against the Layer source aspect ratio, then resamples the density field once to that fitted output size. Previously `KeepAspectRatio` used the 128×96 field ratio; Render Queue stretched that result into the Layer bounds, causing a second nonuniform interpolation for non-4:3 Layers. The sizing contract is corrected statically; image parity/quality is not runtime-verified.

弾性パラメータの現行解釈を記録する。`setMaterial()` は `mu=E/(2(1+nu))`, `lambda=E*nu/((1+nu)(1-2nu))` を使い、2D変形のHencky stress `tau=2mu*epsilon+lambda*tr(epsilon)I` に適用する。これは3D isotropic materialのplane-strain reductionに相当するLamé係数であり、plane-stressの有効lambda `E*nu/(1-nu^2)` とは異なる。一方、降伏面はKlár et al.の2D Drucker–Prager条件で、out-of-plane stressをyield normへ含めない。よって本初版の契約は「2D in-plane deformation + plane-strain Lamé stiffness + 2D Drucker–Prager yield」と明記し、厳密な準2D実材料再現やplane-stress切替は別設計とする。E/nu意味論のUI表示と安息角・圧密への影響は未検証。
- 2026-10-12: Granular grid cell size is derived from the smaller X/Y seed-particle spacing (with a 1px floor), so anisotropic particle layouts do not collapse their denser axis onto an oversized grid. Granular particle boundary margins are capped by the shared support-radius limit in both CPU and GPU readback paths. Existing grid-node and CFL/substep budgets remain the creation guards; resolution/performance acceptance is still unverified.
- 2026-10-12: Granular density raster support radius has a minimum of one density pixel diagonal (with a tiny strict-coverage epsilon), capped by the existing 8px-per-axis limit. Granular per-particle peak density is normalized against discrete kernel weights sampled inside the field, including clipping at field edges, using a fixed 18×18 local stencil and one distance evaluation per candidate sample. Particle contributions are not individually capped; only accumulated per-pixel density is capped at one, preserving each isolated particle's normalized kernel shape. This prevents high-count particle supports from missing sample centers and reduces phase/edge variation in discrete volume contribution. Rendered quality and overlap saturation remain unverified.
- 2026-10-12: Granular CPU G2P now samples the velocity gradient at the old particle position and updates `F` before advancing particle position, reusing the G2P stencil. The legacy Corotated path retains its existing post-G2P deformation update. Solver behavior and affine-field reproduction are unverified at runtime.
- 2026-10-12: APIC P2G now multiplies the affine `C * (x_i - x_p)` velocity correction by particle mass before accumulating grid momentum in both CPU and GPU transfer code. Dimensional consistency is corrected; momentum/angular-momentum conservation and backend parity remain unverified.
- 2026-10-12: Granular CPU `stepOnce()` applies grid boundary response only after force and velocity update, matching the GPU transfer cadence; it no longer applies wall friction twice to P2G momentum in one step. Granular wall contact behavior remains runtime-unverified.
- 2026-10-12: `addParticlesGrid()` checks the column×row product against vector capacity in `size_t`, reuses that checked count for both reserve and particle-volume division, and reserves the population once before filling the seed grid. This avoids signed-int overflow in the denominator and repeated cold-path vector growth/copies. Allocation latency remains unmeasured.
- 2026-10-12: Granular grid nodes now share a 4,194,304-node budget per Composition across registered MPM Sand solvers, in addition to the per-solver cap. The 64 MiB per-Composition particle payload budget now includes live solver vector capacity, baseline capacity, and cached checkpoint capacities; registration checks the actual live and baseline vector capacities before publishing the solver, and checkpoint capture reserves room for incoming particle payload by evicting older checkpoints before copying. If room cannot be made, that checkpoint is skipped. Reconfiguration counts the old same-Layer solver/grid plus incoming solver payload at the transient peak, performs requested-size and actual-capacity cache reservation before commit, and preserves the old solver on rejection; allocator overhead and runtime behavior remain unmeasured.
- 2026-10-12: If an MPM surface packet is rejected because the bounded owner command queue is full, the renderer submits queued packets in order, restores the active target, and retries the surface once. This avoids losing the sand surface solely due to packet-queue saturation; runtime ordering and overflow frequency are unverified.
- Layerは128×96 density fieldの8px上限と粒子seed寸法から基準支持半径を計算し、Granular solver生成時に`granularSupportRadiusLimit_`として固定する。CPU `stepOnce()`、GPU readback tail、density writerはこの値とcell/field local capの小さい方を使う。CorotatedVonMisesには適用しない。solver静的設定でありparticle snapshotへは含めず、Layer rebind時に再設定する。buildとruntime coverageは未確認。

**ステータス:** In Progress — CPU構成則・粒子摩擦硬化、CFL依存減衰、Layer統合、seek checkpoint/replay、packet再利用とqueue overflow時のdrain/retry、Composition単位のparticle payload予算を実装。ビルド・数値・runtime検証は未完了

## 目的

既存のセルオートマトン `SandSim2D` とは別に、Material Point Method (MPM) による連続体ベースの2D砂シミュレーションを制作レイヤーとして提供する。砂粒の集合を変形可能な粒状体として扱い、重力下の堆積、斜面の安息、押し込み・せん断時の塑性流動を表現する。

既存 `MpmSolver2D`、`PhysicsSystem` の layer 単位所有、固定時間刻み／snapshot、Composition の collider proxy 経路を活用する。現在Shapeに使われている弾性材質の意味論は変更しない。

## 現状と境界

**最終更新:** 2026-10-12

- `MpmSolver2D` は APIC P2G/G2P、fixed-corotated応力、簡易von Mises塑性、破断、CPU/GPU backend、snapshotを持つ。
- `PhysicsSystem` はLayerIDごとの材質solverとフレームsnapshotを保持し、Shapeレイヤーの材質シミュレーションに利用している。
- 現MPM presetはFlesh/Foam/HardRubber/Wood。圧力依存の摩擦降伏・顆粒圧密を扱う砂モデルではない。
- `SandSim2D` はセルオートマトンであり、別の制作レイヤーとCPU/GPU実装を持つ。このモデルと操作・保存データは変更しない。
- GPU MPMの塑性／破断末尾処理はCPU実行を含む。砂モデルのGPU parityとreadbackコストは別途計測が必要。

## 採用する初版の構成則

### 先行研究からの選択

初版はKlár et al.の**Henckyひずみ弾性 + Drucker–Prager塑性投影**を候補として固定する。同論文の非関連流れ則は、摩擦角と独立なdilatancy角をパラメータ化する形ではなく、yield gradientの偏差成分を塑性流れ方向に使って塑性体積変化を抑える。したがって初版に `dilatancyAngle` を必須パラメータとして持ち込まない。別の地盤工学型dilatancyモデルを加える場合は独立した後続モデルとして設計する。既存fixed-corotated応力をそのまま組み合わせず、降伏面と一貫するHencky弾性エネルギー／応力を実装する。

参考にするのは、Klár et al., *Drucker-Prager Elastoplasticity for Sand Animation* (SIGGRAPH 2016, DOI: 10.1145/2897824.2925906) のDrucker–Prager応力投影、非関連流れ則、Henckyひずみ弾性、APIC MPMである。これは見た目の砂挙動向けのアルゴリズム選択根拠であり、論文コードの移植・複製を許可するものではない。既存実装と式・テストから独自に導出する。

数値実装ではPradhana, *Multiphase Simulation Using Material Point Method* (UCLA dissertation, 2017) が報告する従来Drucker–Prager return mappingの体積増加アーティファクトを受入リスクに含め、塑性投影後のdet(Fp)、体積変化、有限値を検査する。単純に特異値を同率縮小する現在のvon Mises投影を流用しない。

### 記号・符号・次元

材料更新はまず一次資料の符号規約（圧縮応力が負）に合わせる。`d=2`、Kirchhoff応力 `τ=Jσ`、`dev(A)=A-tr(A)I/d` とする。Coulomb摩擦係数 `μf=tan(phi)` をDrucker–Prager係数へ `α=μf/sqrt(2(1+μf²))=sin(phi)/sqrt(2)` と写像し、yield functionを

`y(τ) = α tr(τ) + ||dev(τ)||F <= 0`

とする。これは論文の2D Drucker–Prager円錐であり、`sqrt(3/2 s:s)` の3D von-Mises相当量へ置換しない。圧縮正のAPIへ変換する場合は `p=-tr(τ)/d` を明示し、同じ条件を `||dev(τ)||F - d α p <= 0` と書く。基礎論文モデルは非凝集性なので初版の凝集力は0固定とし、cohesionやtensile cutoffを加える場合は基礎モデルからの拡張として別途根拠・式・テストを定義する。

`F=Fe Fp` を保ち、trial `Fe` のSVD `UΣVᵀ` から `ε=log(Σ)` を作る。`tr(ε)>=0` のextension caseは `H=0` のcone tipへ投影する。その他は `dev(ε)=ε-tr(ε)I/d`、

`δγ = ||dev(ε)||F + ((d λ + 2 μ)/(2 μ)) tr(ε) α`

とし、`δγ<=0` なら弾性状態を維持する。正なら `H=ε-δγ dev(ε)/||dev(ε)||F`、`Fe_new=U exp(H) Vᵀ` とする。これはHencky座標でのreturn mappingで、非関連流れ方向は `dev(τ)/||dev(τ)||F` となる。`Fp_new=Fe_new^{-1}F` として `F=Fe_new Fp_new` を再構成する。この投影は実装済みだが、論文の2D係数・符号と積順序の解析fixture照合は未実施。

2D対角主Henckyひずみ `ε=(e1,e2)` では `tr(ε)=e1+e2`、`||dev(ε)||F=|e1-e2|/sqrt(2)`。したがってnon-extension時のprojection係数は

`δγ = |e1-e2|/sqrt(2) + ((λ+μ)/μ) α (e1+e2)`

となる。`tr(ε)>=0` は `H=(0,0)` のtipへ投影し、`tr(ε)<0` では `δγ<=0` ならtrialを維持、それ以外は `H=ε-δγ dev(ε)/||dev(ε)||F` とする。したがって静水圧圧縮（偏差ゼロ・負のtrace）は弾性trialのまま、extension tipは別分岐で扱い、ゼロ偏差を塑性方向の除算に使わない。解析fixtureはこの3分岐を直接検査し、純せん断で `tr(H)=tr(ε)`、yield面上で `y(τ(H))=0` を確認する。

粒子のhardening stateを `q` とし、Klár et al.の摩擦硬化曲線 `φF=h0+(h1q-h3)exp(-h2q)` を使う。Layerの初期摩擦角を `φ0` として、既定定数 `h0=φ0+10°`, `h1=9°`, `h2=0.2`, `h3=10°` を適用し、`q=0`で `φF=φ0`、塑性履歴で粒子ごとの摩擦角を更新する。2D Coulomb摩擦角からDrucker–Prager係数へは `μ=tan(φF)`, `α=μ/sqrt(2(1+μ²))=sin(φF)/sqrt(2)` と変換する。この2D導出を使い、論文疑似コードにある別次元向け係数較正式は流用しない。実装では`q`を100で数値飽和し、摩擦角を89°未満に制限する。`φ0=0°`はhardeningパラメータの境界ケースとして扱う。hardening係数はKlár et al.の標準値を基に初期角をLayer入力へ写像した制作向け設定で、実測砂の校正値ではない。

内部応力APIの契約も明示する。Hencky energy `ψ(ε)=μ tr(ε²)+(λ/2)tr(ε)²` からKirchhoff応力は `τ=2με+λ tr(ε)I`。現行 `computeGridForces()` はfirst Piola–Kirchhoff応力を使うため、constitutive helperは `P=τ Fe^{-T}` を返す。GPU shaderの既存fixed-corotated応力は砂strategyでは呼び出さない。

解析fixtureの基準値は `μ=1`, `λ=1.5`, `phi=35°` (`α≈0.405580`) とする。`ε=(-0.1,-0.1)` は静水圧圧縮で `δγ≈-0.2028`、trial維持。`ε=(0.05,0.02)` はextensionで `H=(0,0)`。`ε=(-0.5,0.3)` は `δγ≈0.3629`、`H≈(-0.2434,0.0434)`、`tr(H)=-0.2` となり塑性体積変化を保ったまま `y(τ(H))≈0` へ戻る。これは式の算術確認用で、離散MPMの安定性を証明するものではない。

リポジトリには `gravityY=980` と `gravityY=9.8` の両方があるため、砂Layerは独自の単位契約を使う。Composition座標をpx、時間を秒、基準質量を任意単位、粒子厚みを1pxとし、既定重力を `980 px/s²`（100px=1mとした場合の9.8m/s²相当）とする。Eと密度もこの契約に合わせた制作向けパラメータであり、実測材料値を示さない。

粒子初期質量は2D unit thicknessの `mass = rho * volume0` とする。塑性投影では `tr(H)=tr(ε)` を維持し、体積を変えるのはtrialの塑性体積変化とextension-tip投影に限定する。基礎モデルにない任意の`Jp` clampやtensile cutoffは追加せず、`F`, `Fp`の正determinantと有限値を保つ。体積範囲・圧密や引張破壊の拡張は基礎モデルの別設計として受入試験と根拠を先に定義する。砂は凝集力0を既定とするが、数値安定化用の小さい弾性応答を持つ。

### 必須の物理検査

- `y(τ)<=tolerance` を塑性投影後に満たす。静水圧状態と純せん断状態を個別に検査する。
- 純せん断塑性投影で `tr(H)` とplastic volumeが理論上の体積保存条件を満たす。extension tip caseも別に検査する。
- 圧縮return mappingが材料点の体積を不意に増やさない。体積補正がある場合、質量保存・`Jp`・弾性体積 `Je` の関係を明記する。
- 除荷で塑性状態が戻らないこと、`Fp` の更新後も `F = Fe Fp` が数値許容差内で成立すること。
- 極端な圧縮・せん断、`J` が小さい場合、ゼロ応力、ゼロ凝集力でdeterminant/inverse/logが有限かつ定義域内に保たれること。
- yield projectionに必要な粒子履歴はsnapshotに含める。Material設定はLayerが所有し、PhysicsSystem snapshotがMaterial値と初期状態を混同しない。

### 参考資料

- Klár et al. 2016, [Drucker-Prager Elastoplasticity for Sand Animation](https://doi.org/10.1145/2897824.2925906) — 非関連流れ則、Hencky弾性、APIC MPMによる砂アニメーション。
- Klár et al. 2016, [Supplementary Technical Document](https://www.cs.ucr.edu/~craigs/papers/2016-sand/tech-doc.pdf) — 2D Drucker–Prager係数、Henckyエネルギー導関数、volume-preserving non-associated projection、extension tip caseの一次導出。
- 同補足資料 §5.2 は`PE=∫Ω0 ψ(Fe)dX`から全変形勾配に共役な`P=(∂ψ/∂Fe)Fp^{-T}`を導く。現行Granular force assemblyのchain-rule修正はこのエネルギー規約に基づき、追加の`det(Fp)`係数を用いない。
- Sun, Shinar & Schroeder 2020, [Time step restrictions for MPM](https://cs.ucr.edu/~shinar/papers/2020_explicit_mpm_dt.pdf) — 非線形等方超弾性の有限変形音速と明示MPMの複数timestep restriction。本Layerの塑性algorithmic tangentへ直接適用できる根拠とはしない。
- Pradhana 2017, [Multiphase Simulation Using Material Point Method](https://escholarship.org/uc/item/52b8b82q) — Drucker–Prager砂、return mapping、体積増加artifactの議論。
- Zhang et al. 2021, [Dam Breach Simulation with the Material Point Method](https://www.mdpi.com/2079-3197/9/2/8) — MPM塑性投影と体積補正の数値処理例。別の土砂シナリオであり、そのままの材料パラメータ根拠にはしない。

## 非交渉の契約

### 実装進捗 (2026-10-11)

- `LayerType::MpmSand2D` と独立Layerを追加し、既存のCA `SandSim2D` は変更せず共存させた。
- Layer attach時にComposition runtime world IDとLayer IDでGranular Drucker–Prager solverを登録し、detach時に解除する。初期粒子はLayer内の下寄り領域へ格子状に配置する。
- seed粒子領域と計算domainを分離し、計算domainはLayerのsource bounds全体に取る。これで初期配置領域より下へ落下・堆積でき、Layer boundsに粒子境界も置く。
- Granular density writerはLayerから渡される粒子基準半径をsolver作成時の`granularSupportRadiusLimit_`で統一し、実field寸法の8px上限をさらに適用する。GPU/CPU wall clampも同じ基準上限と粒子`det(F)`を使う。非Granular solverでは従来の`supportRadius`引数を維持する。
- MPM密度surfaceをbounded triangle packetとしてGPU solid-triangle batchへ送り、Layer transformとopacityを反映する。`RenderCommandBuffer`はsubmit/reset後にsurface頂点vectorを回収し、次frameのdrawへ返す。recycle容量はframe packet capacityに合わせ、同一frame内の複数MPM Layerのaccepted surface bufferをすべて保持できる。owner queueが満杯なら既存packetを順序どおりsubmitしてからsurface packetを再試行する。CPU Render Queue向けにはLayerの`getThumbnail()`で同じ密度field・色・alpha曲線から`ImageF32x4_RGBA`を作り、既存QImage surface境界へ変換する。pool再利用、overflow時のGPU順序・target復元、GPU/CPU視覚parity、CPU surface/effect/matte出力は未検証。
- E、Poisson比、内部摩擦角（UIはdegree、JSON/APIはradian）、密度、無次元減衰（0..1）、重力（px/s²）、粒子列/行数をLayer propertyとして公開し、値域を検証してMaterial設定をJSON保存する。Granular modeの減衰値は1/120秒あたりの速度減衰率として解釈し、CFL内部dtに応じて指数スケールする（legacy solidの従来のstepごとの値は維持）。solver内部刻みはPhysicsSystemのComposition共通fixed stepに従い、初期粒子速度0で粒子格子幅と全粒子の縦波速度上限からCFL係数0.05を使って設定し、1/120秒を上限とする。Compositionのworld clockはsolverへ進める経過時間を渡し、solver内部dtがその時間をCFL基準で分割する。各Granular internal stepの直前に粒子中心速度に加え、APIC `C`のFrobenius normから4×4 stencil内の速度寄与上限 `1.5*sqrt(2)*dx*||C||F` を評価し、`dt <= 0.05 * dx / (sqrt((lambda + 2*mu) / density) + maxParticleAdvectionSpeed)`を用いる。これにより同一`update()`内で応力・移流速度が増えた後のstepにも新しいCFL刻みを適用する。`maxParticleAdvectionSpeed`は粒子中心速度と各粒子のAPIC affine上限の和の最大値である。波速は各粒子の`mass/volume0`から算出し、その最大値をCFLへ使う。必要刻みがAPI下限の1e-6秒を下回る場合は、下限内でCFL上限を守れるよう上限超過粒子の`vel`と`C`を比例縮小する。この緊急経路は塑性履歴や`F`を直接変更しないが、運動量・変形への影響はruntime未検証。最大1024 solver substepsを超える内部CFL backlogは102.4秒まで保持する。Granular outer clockはqueued elapsed timeに応じて1回のstep budgetを決め、最大1024 fixed stepまで処理する。queued backlogをsnapshot対象clockに保持し、outer clock backlogは102.4秒を上限として、それを超えたstall時間は切り捨てる。極端なframe deltaでの処理量、solver側CFL上限、時間上限の受入れは未検証。Property編集はsolverを再初期化するため、既存の砂状態はリセットされる。新solverの生成に失敗した場合は旧設定・旧solverを維持する。
- CFL式の訂正（直前のtransport契約を更新）: `sqrt((lambda+2*mu)/density)`というrest-state近似を使う説明は旧仕様。現行Granular CFLは`acousticSpeedSquaredUpperBound`を各particleの`Fe=F Fp⁻¹`から計算し、`sqrt(acousticSpeedSquaredUpperBound/(mass/volume0))`と粒子中心/APIC移流上界の和を使う。upper boundは2D全伝播方向をGershgorin行和で包絡し、rest-state縦波速を下限とする。これは固定`Fp`の弾性predictorに対する音響上界であり、return-map・衝突・grid transfer stabilityを証明しない。
- Granular step終了時に`F`/`Fp` determinantとAPIC `C`の有限性を検査し、非物理状態をidentity/zero状態へ戻して速度勾配の伝播を止める。これは破綻からの数値復旧であり、受入試験で異常状態を起こさないことの代替ではない。
- `plasticStrain`をsnapshot対象のhardening state `q`として再利用し、Klár摩擦硬化曲線で現在の粒子摩擦角を求める。初期摩擦角はLayer設定、硬化係数は既定値、q=100で曲線を飽和させる。hardeningにより摩擦角が増えること、seek後に同じ摩擦角へ復元することは数値試験で未確認。
- Layer menuとfactory、明示module manifestへ登録した。CMake/ビルド/テストは未実行。
- seekは12フレーム間隔でGranular solver群の共通snapshotを保持し、cache miss時に最寄りの前方checkpointからtargetまで再生する。snapshotには粒子状態・solverの秒単位accumulatorとCompositionのclock remainderを保存する。Granular CFL刻みは各internal stepの前に現在粒子速度・APIC `C`から再評価し、restore後も次step前に同じ評価を行うため、snapshotにdt自体は含めない。restore時に現solver dtでsnapshot accumulatorを切り詰めない。step判定の許容誤差で発生する負の丸め残量は0へ正規化し、world clock snapshotの非負条件を保つ。solverの最大許容dt×substep上限を越える不正accumulatorは拒否する。baseline粒子payloadとsnapshot粒子payloadの合計を64 MiB/Compositionに制限する。予算超過時は新規solver登録を拒否し、snapshotは最古の共通frameからevictする。Compositionのcollision pair評価時にMPM Layerのactive状態を同期し、in/out範囲外のLayerは固定時間ステップのsolver更新対象から外す。動的colliderはsnapshotには含めず、各replay frameでcollision pair評価が対象solverの旧colliderをclearし、そのframeのactive source transformをsolver local空間へ変換して再登録してからadvanceする。初回replay、複数layerの共通checkpoint選択、active範囲と動的colliderを含むseek経路はコード実装済みだが、数値一致とUI/Render Queue runtime確認は未実施。
- 未完了: 動的collider replayの一致確認、配置・単位系の調整、構成則解析fixtureと視覚確認、粒子packet poolの計測と上限評価。

1. 砂の構成則を既存弾性モデルへ暗黙適用せず、`MpmMaterialModel` 等で明示選択する。従来のFlesh/Foam/Rubber/Woodの数値・動作・保存互換性を保つ。
2. 砂は圧力依存の摩擦降伏を持つ顆粒塑性とし、初期モデルは2D向け Drucker–Prager近似（Mohr–Coulomb内接円錐）を基礎候補とする。圧縮時の塑性体積変化・粒子体積／密度の許容範囲を定義し、引張で砂が粘着して膜状に残らないよう引張強度を制限する。
3. 材料パラメータは内部摩擦角、弾性率、密度、減衰を明示する。基礎論文モデルでは凝集力は0固定、独立dilatancy角は持たない。値の単位・2D応力条件・安定範囲を設計文書とAPIで固定する。
4. シミュレーション粒子は表示用Particle Layerのエミッタ粒子として扱わない。MPM状態、砂専用初期配置・パラメータ、描画 packet、保存復元を明確に分離する。
5. Simulationは固定step、決定可能な初期seed、既存PhysicsSystemのframe snapshot/seek契約に従う。Preview/seek/renderで異なる状態進行を起こさない。
6. GPU対応はCPU基準結果との一致基準を定めた後に進める。CPU fallback、容量上限、readback/同期境界を明示し、GPU未対応材質を暗黙に対応済みと表示しない。
7. ホットパスで粒子・gridの再確保、可変長ログ、GPU resource再生成を起こさない。容量は初期化／解像度変更時に確保し、上限時の挙動を定義する。

## フェーズ

| Phase | 内容 | 完了条件 |
|---|---|---|
| P0 | 現MPM応力・塑性、Layer/PhysicsSystem、render/seek経路を仕様化し、単位と採用構成則を固定 | 式、圧力符号、yield条件、体積保存の流れ方向、extension tip処理、物理単位、snapshot責務が明文化される |
| P1 | MPM材料モデルを選択可能にし、既存弾性モデルを保持したまま顆粒塑性を実装 | 単軸圧縮・せん断・除荷で降伏面、体積変化、有限値、決定性を検査できる |
| P2 | 独立したMPM砂Layer／作成導線と初期条件・砂パラメータを追加 | Shapeの材質設定や既存SandSim2Dとデータ／UI責務が混線しない。保存後に同じ設定を復元できる |
| P3 | 砂粒子の描画、外部collider、fixed-step再生、snapshot/seekを統合 | Composition preview、frame seek、offline frame評価が同一solver stateを表示し、Undo/Redo・save/load後も復元できる |
| P4 | 安定性・品質・性能をCPU基準で評価し、GPU対象範囲を決定 | 安息・堆積・せん断・圧密・境界・決定性・上限近傍の受入結果が記録され、CPU/GPU対応範囲が明示される |
| P5 | 必要な場合にGPU granular MPM経路を実装 | P4のCPU基準と許容誤差・失敗fallback・同期/readback予算を満たし、D3D12/Vulkan対象backendで確認される |

## 受け入れシナリオ

- 塊を水平床へ落とすと、発散・振動せず停止し、接触面から有限な層厚で堆積する。
- 塊を斜面へ積み、静止後の斜面角が設定摩擦角と一貫する（粒子解像度ごとの許容誤差を定義する）。
- 砂塊を押し／せん断した後、除荷しても弾性固体のように元形状へ戻らず、塑性変形が残る。
- 引張・回転・高圧縮・境界接触でNaN/Inf、粒子の格子飛び、エネルギー発散、意図しない破断が起こらない。
- 同一seed・設定・fixed-step列で同一結果を再現し、別表示fpsから同じframeをseekしても状態が一致する。
- 順方向 `0→N`、直接seek `0→N`、逆方向 `N→0→N`、cache eviction後の `M→N` を比較し、位置・速度・`F`・`Fp`・plastic historyが許容差内で一致する。Layer追加／設定編集後は旧generation snapshotを使わない。
- project save/load、layer duplicate、Undo/Redoで初期条件・材質値・シミュレーション有効状態を保持する。
- Serialized ID復元後、`PhysicsSystem::getMaterialSolver(layer->id())` が非nullで、仮生成ID側に孤児solverが残らない。
- レイヤー削除で対応するMPM solverとsnapshotを確実に解除し、Undoで再追加した場合は保存状態から同じlayer IDへ一度だけ再bindする。重複IDの修復を行うComposition読込でも登録IDと最終Layer IDを一致させる。
- Render Queue JSON cloneと各MFR workerでframeごとに隔離されたMPM stateを取得でき、worker間でsolver stateが上書きされない。
- CPU/GPUが両方利用可能な場合、明示された対応モデルで粒子状態と画像の許容差を評価し、未対応モデルは明示的にCPUへfallbackする。

## 実装上の注意

- 初版の式はsolver全体へif分岐を散在させず、材料モデル選択点と粒子状態更新に閉じる。構成則の追加でShapeの既存FEM/材質挙動を変えない。
- `MpmParticle2D` の現在snapshotは全粒子状態を持つ一方、材料設定の保存所有者はLayer/PhysicsSystem境界で確認してから設計する。新しい状態フィールドを追加する場合はsnapshot妥当性とGPU ABIも同時に確認する。
- 現行 `PhysicsTimelineSettings::maxCachedFrames` は既定480で、各solverのsnapshot mapをフレーム数でtrimする。MPM snapshotは全粒子配列を各frame分コピーするため、この件数上限だけでは粒子数・MPM砂レイヤー数に比例した総メモリ量を抑えない。砂Layerのbaseline particle bytesを含むtotal-byte budgetを設け、cache miss時は直前snapshot/baselineから決定的再実行する。silentな欠落snapshotや無制限な全frame蓄積を許さない。
- `ArtifactAbstractComposition::Impl::removeLayer()` と `removeAllLayers()` はLayerをvirtual `setComposition(nullptr)` でdetachする。`ArtifactMpmSand2DLayer::setComposition()` は旧Compositionのworld IDとLayerIDでGranular solverを明示解除してからbaseへdetachするため、MPM砂solverの登録寿命はコード上で閉じている。一般のShape material solverは `ArtifactAbstractLayer::disableMaterialPhysics()` が別途 `removeMaterialSolver(id())` を呼ぶため、detach/UndoだけでこのAPIが実行されるかは別の既存課題としてruntime確認が必要。MPM砂をCompositionから削除・全削除・再Undoした時の実world挙動は未検証。
- （当初の調査時点の記録）`MpmSnapshot2D` は粒子履歴、境界、時間状態を保存しない設計だった。その後、粒子・grid origin/dimensions・cell size・accumulator等の再現に必要な動的状態はsnapshotへ追加済み。材料/モデル、重力・境界・collider、fixedDt/maxSubstepsなどの静的設定はLayer/solver registrationが所有する。GPU device/session pointerやresource handleはsnapshotへ入れない。
- 現行 `firstPiolaKirchhoff()` はfixed-corotated応力、`applyPlasticity()` はvon Mises型の特異値投影である。砂のDrucker–Prager yield projectionだけを追加し、応力・流れ方向を既存式から流用する実装は構成則として整合しない。砂用弾性応力と非関連return mappingの両方を一体で定義し、既存モデル経路へ回帰を波及させない。
- GPU粒子ABIは位置・速度・F/Fp・C・volume0・塑性履歴・色を持つ。モデル選択／砂固有履歴をGPU化する際は `MpmParticleGPU` layout、HLSL `MpmParticle`、upload/readback変換、shader permutationを同時に更新・検査する。CPU-only砂を先行する場合もGPUモード選択時に誤って既存fixed-corotated shaderへ流さない明示fallbackを設ける。
- GPU pathは現状P2Gの共有加算や塑性CPU tailを含むため、材料モデルだけをshader化してGPU parity済みとは判定しない。
- 新規ソルバーを作る前に既存MpmSolver2Dへの追加が適切かを評価する。既存ソルバーを共用できない具体的な式／状態／backend要件が示された場合のみ分離を選ぶ。
- MPM砂は本マイルストーン対象。セルオートマトン砂の操作ツール、火・水など他素材、全般的な粒子機能拡張は対象外。

## P0の実装境界案（コード調査に基づく）

### constitutive実装の必須順序

初期調査後に `LayerType::MpmSand2D`、専用Layer、JSON設定、layer menu/factory、粒子render packet、Composition単位solver lifecycleを接続した。Composition frame seekは共通checkpointを探し、cache miss時はbaselineまたは最寄りcheckpointからframeごとのcollider評価とfixed-stepを再実行する。Granular snapshotは12 frameごとに保持する。GPU simulationは未対応でCPUのみ。build/link、構成則数値安定性・受入シナリオ、通常再生／seek／offline Render Queueでの表示一致は未確認。

`MpmSolver2D::computeGridForces()` は `firstPiolaKirchhoff(Fe)` を使う。legacy modeはfixed-corotated PK1応力を維持し、granular modeではHencky PK1を返す。`applyPlasticity()` もmodel別に分岐する。`stepOnce()` は応力計算→P2G→G2P/F更新→塑性投影→collider→`checkFracture()` の順で動くが、granular modeはfracture判定をskipする。GPU substepはshader側がfixed-corotated弾性なのでGranularDruckerPrager選択時にGPU backendを拒否する。

- CPU granular constitutive分岐を実装済み。受入前に解析fixtureで式・SVD・stress/return-map state transitionを検証し、legacy strategyは粒子軌跡とsnapshot fixtureで回帰確認する。
- 砂 strategyは初版CPU only。`setBackend(GPU)` は砂選択状態でfalseを返す。MpmSand2DはCPU backendを使う。GPU simulation対応とfallback UIは本実装に含めない。
- Granular modeは `checkFracture()` をskipする。`maxPlasticStrain` / `fractureEnergy` のsolid fracture semanticsはlegacy modelに限定する。
- Solver-level constitutive APIはCPU stress/return mappingへ接続済み。LayerType/factory/Property UIは追加済みだが、単軸圧縮／せん断／除荷の数値検証が未完了のため完成扱いにはしない。

### solver API / 構成則境界

- `MpmMaterialPreset`（Shape向けの見た目preset）と構成則選択を混同しない。solver APIには `MpmConstitutiveModel`（`CorotatedVonMises`, `GranularDruckerPrager`）を追加済み。Layer側の設定は「MPM砂LayerならGranularDruckerPrager」と明示し、generic Shapeでuser選択されない限り既存モデルを維持する。
- CPUの粒子状態更新は `applyPlasticity()` の大きな分岐へ砂式を埋め込むのではなく、材料モデルごとの内部 helper に分ける。応力生成も `firstPiolaKirchhoff()` から構成則単位のstress helperへ分離し、`stepOnce()` の順序と既存モデルの計算式を保つ。
- 砂用粒子履歴には `Fp` に加えて硬化履歴 `q` が必要で、既存の `plasticStrain` を利用し `MpmParticle2D` と `MpmSnapshot2D` に保持する実装とした。モデル・摩擦角・弾性係数など静的material設定はLayer/projectデータが所有する。基礎モデルの凝集力は0、独立dilatancyはなし。履歴の単位・時間刻み依存性と数値安定性は受入検証待ち。
- GPUは `MpmConstitutiveModel::GranularDruckerPrager` 未対応の間、`setBackend(GPU)` が成功したように見せてはならない。モデル選択APIがGPU unsupportedを返す、または明示CPU fallback statusを返し、呼出側がbackend/理由を表示可能にする。GPU shaderへ旧fixed-corotated処理を流し続ける状態は禁止。

### Layerと描画境界

既存 `ArtifactAbstractLayer::enableMaterialPhysics()` は弾性MPMを保持し、砂Layerは別registry/APIで構成則を固定する。`ArtifactMpmSand2DLayer` は専用particle render packetで描画し、Shapeの変形quad描画やCA SandSim2Dを変更しない。初期粒子配置はLayer幅60%・高さ35%。砂Layerの初期床はsource下端で、左右・上側はopenとし、計算gridは初期source boundsから粒子移動に応じて予約capacity内で拡張する。設定はproject JSONへ保存する。Granular Drucker–Pragerでは床の法線速度を外向き時だけ除去し、`boundaryFriction`を接線速度へ適用する。既存弾性MPMの境界挙動は維持する。

- 専用 `MpmSand2D` Layer、factory、JSON save-load、layer creation menuは実装済み。duplicate/undo lifecycleはruntime未確認。
- MPM Layerは `ParticleRenderData` を使わず、solverの粒子位置から128×96のbounded密度fieldを作り、半透明頂点色付き三角形meshとして1 packetでrendererへ送る。密度寄与は現在体積 `volume0 * det(F)` と変形に応じた支持半径を使い、半径は出力fieldの各軸8 pixel以内に制限する。field writerは最大4M pixelのcaller-owned bufferを使い、meshはfield標本中心を内部頂点として保持し、その外側に半pixel幅のcellを追加して全Layer領域を覆う。格子は130×98頂点、129×97 cellで、最大75,078 verticesの事前確保bufferを2本で再利用する。標本中心のdensity値は平均化せず頂点alphaへ渡す。DiligentImmediateSubmitterは既存の4096頂点solid-triangle batch VB/PSOへ分割して描き、新GPU resourceは作らない。
- 静的コード上、GPU Composition ViewとGPU Render Queueは `drawLayerForCompositionView()` のfallbackから `ArtifactMpmSand2DLayer::draw()` へ到達する。CPU Render Queueの `renderLayerSurface()` はMPM専用 `getSoftwareRenderSurface()` を呼び、その後共通のrasterizer effects/masks、matte、opacity、transform、blend処理へ進む。GPU owner queue満杯時は `ArtifactIRenderer::Impl::drawMpmSurface()` が既存packetをsubmitしてactive render targetを復元し、surfaceを一度再試行する。途中submit時のlayer順序・target維持、Render Queue経由のmask/effect実画像確認、GPUとの画素・alpha parity、frameごとの変換コストはruntime未確認。field resolutionによる輪郭品質・性能も実機未確認。
- Shapeの変形quad描画や点spriteでMPM砂を表現しない。任意の大変形で粒子格子順序が折り重なるため、格子quadはセル反転・交差・破断穴を表せず、点spriteは連続体表面の密度を表せない。現行bounded density surfaceはGPU Layer drawへ接続済み。CPU Render Queueは専用`getThumbnail()`がsolver密度fieldをQImage境界経由で返す。Render Queueでのmask/effect適用とGPU parityは未確認。QImage/QPainterを描画・合成の本流へ持ち込まない。
- Density fieldは描画surface再構成専用で、物理質量・体積の診断値ではない。粒子ごとには離散kernelをfield内sampleで正規化してcurrent volumeを配賦するが、重なったpixelは1.0で飽和し、field境界でkernel supportが切れた場合は残ったfield内sampleへ再正規化されるため、field積分は一般に物理体積を保存しない。
- 描画粒子のGPU処理とsimulation backendは別の責務で扱う。現行MPM砂はCPU simulationであり、既存Diligent triangle batchによる描画をsimulation GPU化と誤認させない。
- 連続再生中だけsimulationを進め、停止・scrub・offline frame要求はPhysicsSystemの該当frame snapshotから評価する。Layer `draw()`をsimulation tickとして利用しない。現在SandSim2Dの `draw()` は呼出ごとにsimulationを進めるため、その時間所有パターンをMPM砂へ複製しない。
- Layer boundsとparticle field boundsを区別する。動いて広がる砂が初期Layer rectangle外へ出てもclip/cullで消えず、transform/collider/render boundsを同じローカル座標契約で更新する。

#### 初期Layer矩形外への移動に関する実装ギャップ（2026-10-12）

この受入条件は初期配置seed矩形を越えて粒子が移動・表示されることを求める。Granular solverはsource下端floorを残し、左右・上側のLayer境界wallを外す。solver gridは作成時のlive grid node数の最大4倍までcold pathで確保し、現在の粒子範囲が安全marginを越える前にactive geometryだけを拡大する。拡張ではgrid buffer再確保を行わず、粒子position/F/Fp/Cとworld座標を保持する。`MpmSnapshot2D`のorigin/dimensionsはsnapshotごとに保存され、restoreは同一cell size・seed metadataと予約capacityの範囲を検証してgeometryを復元する。CPU Render Queueはsurfaceとそのlocal originを別々に受け取り、GPU/CPU density fieldはsource boundsとparticle support boundsのunionを使う。Composition Viewコードにはauthored local boundsによるMPM per-layer cullは見つからない。GPU Composition Render Controllerは対応するrasterizer effectをintermediateへ適用し、layer maskまたはGPU未対応rasterizer work時は上限付きsoftware surfaceへfallbackする。matteはComposition passの既存経路で処理する。残る実装・受入項目は、modifier適用範囲の確認、容量上限時にClampへfallbackする状態のUI runtime確認である。実機・数値の未検証項目は、viewport端/camera/crop表示、selection overlayの範囲、粒子拡張とseek/replay、floor/collider応答、最大粒子数時のbounds/density描画CPU時間、checkpoint capture時のallocation hitchとstorage reuse、CPU/GPU画質・性能、実scene受入である。

このギャップを閉じる実装では少なくとも次を一体で満たすこと。

1. **実装済み、runtime未確認:** seed矩形、solver計算領域、Layer/source bounds、表示field boundsを分ける。solverではsource下端floorを維持し、左右・上側のsource境界をwallにしない。
2. **部分実装:** particle boundsをCPU/GPU surfaceへ含め、CPU Render Queueのsurface placementへlocal originを渡す。Composition View draw pathにauthored-bounds per-layer cullがないことは静的確認済み。GPU Composition Render Controllerは対応rasterizer effectをGPU intermediateへ適用し、maskまたは未対応effect時はbounded software surfaceへfallbackする。matteはComposition passの既存処理へ渡す。modifierの適用範囲とtransform・viewport edge・offline renderはruntime受入確認が残る。
3. **実装済み、runtime未確認:** Granular solver gridは予約capacity内でactive geometryを拡張し、粒子/APIC/plastic stateとworld coordinateを保つ。snapshotはgeometryを保存し、capacity内の復元を許可する。step中のgrid node vector再確保は行わない。
4. **部分実装:** Composition worldのnode budgetに予約capacityを含め、capacity超過時はsticky solver status flagをsnapshotに記録しsafe-grid clampへfallbackする。選択中Property Editorの警告表示は実装済み。capacity boundaryのruntime受入と警告の表示更新タイミングは未検証。

静的照合では、`MpmSolver2D::setBoundary()`と`updateGranularTimeStepForState()`のgrid端clamp、`setGrid()`の粒子配列非空時resize拒否、snapshot geometry一致、source-size field範囲が元の制約だった。現在は予約node capacity内の粒子保持grid拡張、可変grid snapshot restore、MPM専用floor、CPU/GPU particle-bound field extentまで実装した。開放領域受入scene・数値再現・viewport edge/camera/crop・CPU/GPU画素比較は未検証。Composition Viewコードにはauthored-bound per-layer cullが見つからない。有限capacity超過時はsticky solver status flagをsnapshotへ保存し、単一MPM layer選択時はProperty Editorへ警告を表示する。専用診断surfaceとruntime表示確認は未実施。MPMはComposition GPU intermediateで対応可能なrasterizer effectを適用し、layer maskまたは未対応effectはbounded software surfaceへfallbackする。matteは各composition passの既存処理を使う。modifier pathの適用範囲とruntime parityは未確認。

設計上の分離を維持すること。`ArtifactAbstractLayer::localBounds()`はTransform/anchor・選択・衝突等にも使われるため、MPM particle AABBをそのまま返すとauthored geometryとcollider/pivotの意味を動的に変えてしまう。現行コードでは`safeLayerSize()`と`getSoftwareRenderSurface()`がMPM専用render boundsを使い、CPU surface placementにも同じlocal boundsを渡す。GPU packetも同じparticle/source unionから生成する。Composition Viewの`drawLayerForCompositionView()`はauthored `localBounds()`を有効性確認と通常のLayer座標契約に利用するが、MPMはfallbackで直接drawされ per-layer cullはしない。viewport/render target外の通常clip、selection/overlayの範囲、camera/crop表示は実sceneで確認する。対応rasterizer effectsはGPU intermediate、maskと未対応effectsはbounded software surface、matteはComposition passの既存経路へ渡る。modifierの適用範囲と全経路のparityはruntime未確認。

拡張時のメモリ契約はattach/configure時にCompositionの4,194,304-node budget内で予約capacityを確保する方式に決定した。初期grid node数の最大4倍を予約するが、残budgetが少ない場合はその範囲へ縮小する。simulation stepではactive dimensions/originのみを更新し、予約済みvectorのsize/capacityを超えない。粒子拡張・resetの実時間、予約nodeの実メモリ、複数Layerの予算境界、capacity超過時のsafe clampとstatus flagはruntime未検証。

### Simulation clock / frame evaluation調査

PlaybackEngineの小刻み前進ではCompositionが砂solverをworld ID付きでadvanceし、12 frameごとにcheckpointをcaptureする。solver registryとclockはComposition runtime world IDごとに分離する。Render Queue cloneは新runtime world IDを得てLayerのsetComposition時にsolverを構築する（workerごとのruntime parityは未確認）。

`goToFrame()` と大seek/逆再生はGranular専用replay経路を通る。最寄りの全砂Layer共通checkpointを復元し、各frameでLayer frameとcollision pairを評価してfixed-stepを進める。checkpointがない場合はsolver baselineへ戻る。replay結果の数値一致とframe 0からの実動作は未検証。

従来のSoftBody/Cloth/Material snapshot restoreはGranular replayとは別の既存経路であり、本変更でその契約を置き換えない。Granular replayが失敗した場合、Composition呼び出し側は戻り値を表示・伝播していないため、solver未bind/registry欠損時に砂が静止しても判別しにくい。diagnostic表示が残課題。

実装済み契約: Baselineはsolver登録時に作り、設定変更時にsolver再生成・cache invalidationする。共通checkpointを全solverで先にvalidateし、frame replayで各frameのcollision pairを評価する。snapshot payloadとbaseline粒子配列の合計はComposition worldごとに64 MiB以内とし、予算超過時は最古の共通frameをevictする。cache keyにはworld IDとLayerIDを使い、solver再登録でworld cacheを全消去するためgeneration/model versionは明示key化していない。

Render Queue cloneはJSON復元後にCompositionへLayerを追加するため、MpmSand2D solverはclone側の新runtime world IDへbindされる設計。`fromJson()`はLayer attach後に保存current frameへ`goToFrame()`し、render側も対象frameで`goToFrame()`を呼んでGranular replayへ入るため、静的経路ではseed baselineのまま出力しない。worker開始前に逐次実行されるsolver登録でworld clockと各Layerのsnapshot map nodeを事前作成し、snapshot captureは共有外側registryを`find()`するだけに変更した。worker側のclock/snapshot container構造追加を避ける。CPU MFR cloneはRender Queue所有のvector、GPU cloneはworker ownerが保持し、dispatch teardownは全render workerをjoinした後に進むためworker中にclone Compositionが破棄されない。worker単位のframe再現、複数worker間の状態隔離、UI編集との同時実行、描画一致はruntime未確認。

既存Shapeのmaterial physicsではJSON property restoration前後のID順によるregistry不整合懸念が残る。専用MpmSand2D factory経路は`fromJsonProperties()`でauthoring設定を読み、serialized IDを復元してからCompositionのappendへ渡す。appendは`setComposition()`でsolverを作成し、`isCompositionAttachmentReady()`でregistry bind成功を確認する。Composition `fromJson()`は全Layerをattachした後に保存frameへ`goToFrame()`し、Granular replayへ入る。したがってsource上の順序は整合しているが、JSON load、Clone、Undo/Redo後のsolver bindと粒子状態のruntime一致は未確認。

MpmSand2Dはserialized ID復元後、Compositionが `setComposition()` してsolverを作る順序を利用する。既存Shape registry問題は別件として本マイルストーンでは修正しない。

残作業は通常playback、seek miss、Composition clone、Render Queue workerでの状態隔離をruntime確認すること。baseline snapshot復元不能時のworld ID/frame warningは実装済みだが、その発火・非発火条件をruntime確認する。MPM専用timerやLayer draw side effectは追加しない。

### 保存・シーク責務

コード上 `MpmSnapshot2D` は粒子配列、破断index、grid origin、solver accumulator、初期粒子格子寸法、Granular world clock remainderを含む。model/material、grid cell size/dimension、重力、boundary、fixedDt、substep limitなどの静的solver設定はLayer/solver registrationが所有し、frame snapshotには複製しない。外部colliderはCompositionが各replay frameで再評価してsolverへ適用する。したがってframe cacheは同一runtime world内の同じsolver registrationへだけ復元する。

- project保存値: Layerが所有する構成則enum、砂material parameters、初期粒子seed/配置、grid/resolution、collision/simulation flags。
- frame cache value: position/velocity/F/Fp/C/mass/volume/塑性履歴/active、accumulator、その他将来追加される時間発展状態。
- solver runtime value: GPU device/context/resource handle、scratch grid、render cacheなど。これはproject JSONやframe snapshotへ保存しない。
- solver registrationの再生成・Material設定変更ではworld単位のframe cacheを無効化し、clockをresetして全solver baselineを作り直す。Granular checkpointは同じruntime world内で全Layerの共通frameと共通clock remainderを検証してからrestoreする。snapshot value自体にはmodel/configuration identifierがないため、異なるsolver instance/configuration間の直接restoreは同一設定であることを呼出側の前提とする。
- Layer削除／Undoでsolverを再構築する場合、Layer JSONに保持するseed/configから同じ初期状態を再生成し、PhysicsSystemのframe cacheを初期状態と混同しない。

### 2D応力と単位のP0決定項目

既存のMpm solverの重力既定値は980だが、`PhysicsSystem::update()` の別既定値は9.8。Granular Layerは solver既定値に依存せず、Layerの保存設定から重力を明示設定する。

- 初期契約: Composition上の位置単位=px、時間=s、質量=任意基準mass、unit thickness=1px。Gravityは画面座標の+Y方向を正として `px/s²` で保存し、既定値は980。Eと密度は制作向けの同一基準値であり、物理実測単位を入力する機能は本初版に含めない。
- Compositionのworld clockがGranular solverへ渡すのは進める経過時間であり、solver内部dtはその経過時間を複数substepへ分割する。内部dtは粒子中心速度とAPIC affine速度寄与上限`1.5*sqrt(2)*dx*||C||F`を含むCFL速度をupdateごとに再評価し、1/120秒を上限とする。1024内部stepを超えて残った秒数は102.4秒までsnapshot対象accumulatorに保持する。Granular outer clockはqueued elapsed timeに応じて1回のstep budgetを決め、最大1024固定stepまで処理する。超過残量を同じworld snapshotに保持し、outer clock残量は102.4秒を上限として、それを超えるstall分は切り捨てる。各上限を跨ぐCFL更新、seek/replay、数値安定性は受入試験に残す。
- 密度を `mass/(px² * unitThickness)`、Young率／凝集力／応力を同じ `mass/(px*s²)` 系の力/面積として扱う。数値の基準質量単位をUI/仕様に表示するか、無次元化したmaterial presetとして入力値を限定する。
- 2Dは一次資料の2D面内応力条件を採用する。`||dev(τ)||F` を使い、3D von-Mises相当応力へ置換しない。Coulomb摩擦角からの係数は2D導出の `α=sin(phi)/sqrt(2)` に合わせる。
- 摩擦角 `phi` の範囲をdegree単位で定義し、`0 <= phi < 90°` を初版の妥当条件とする。2D係数は `alpha=sin(phi)/sqrt(2)` とする。基礎モデルは `c=0`、独立dilatancy角なし。extension時のcone-tip投影、塑性hardening履歴の再現、正determinantとE/ρ比に基づく最小fixed dtを試験で定める。

### P1の独立テスト契約

新しい契約テストは既存 `tests/ArtifactCore/PhysicsDeterminismTest.cpp` の責務・命名・CMake登録規約を確認して追加し、実装時に明示許可を得た後で実行する（リポジトリの現行指示ではAIによるビルド／テストはユーザー明示指示まで行わない）。テスト群を少なくとも以下へ分ける。

1. **構成則unit test:** elastic trial stress、pure shear、hydrostatic compression、extension tip caseについてyield sign・return map後 `y(τ)`・plastic volumeを確認。
   - Multiplicative chain rule: finite-difference `W(F)=ψ(F Fp⁻¹)` against `P_total=P_e Fp⁻ᵀ` for non-identity, rotated, and non-diagonal `Fp`; verify `Fp=I` equivalence and no change to the Corotated force path. For acoustic CFL probes, keep `Fp` fixed during the elastic perturbation, apply `dFe=dF Fp⁻¹`, and compare both elastic and total-F tangents.
2. **履歴contract test:** `F=Fe Fp`、`det(Fp)`境界、除荷後残留、snapshot→restore後の次step一致を確認。
3. **solver integration test:** 1〜複数particleのdrop/settling、床摩擦、pile slope、shear boxを固定step・固定seedで実行し、有限値と定量的な角度/体積/運動量範囲を確認する。壁4方向について法線非貫通と接線速度変化`|Δv_t| <= μ|Δv_n|`を確認し、非接触／離反状態では接線速度が変わらないことを検査する。異なるCFL timestepで同じ法線入射・接線初期速度を使い、壁摩擦のstep-count依存がないことを確認する。Plane/Circle/Polygon/Box colliderでも法線長が1であること、接近時の反発後法線速度と`|Δv_t| <= μ(1+e)|v_n|`、離反時の速度・APIC行列不変を確認する。接触stepでは`F`/`Fp`の有限性・正determinant・塑性硬化量を記録し、外部colliderへの一時貫入が非接触の同等stepに比べて過剰な体積／塑性変化を作らないか評価する。Circleは中心一致からの退避、polygonはCW/CCWの辺上・頂点接触を行い、凸頂点の両面進入／片面進入と凹頂点で選ばれる法線・応答がwinding反転で一致すること、重複頂点・退化形状で有限応答となることを確認する。Granular dampingは同じ総時間に対して異なるCFL substep数でも同じ減衰率になることを確認する。Granular solverとouter clockのstep上限・snapshot backlog復元、102.4秒境界を比較し、時間損失とseek/replay一致を確認する。Layer active/inactive境界ではinactive中のparticle snapshot不変、再active後の前進、境界を跨ぐseek往復の一致を確認する。density field writerは既定と最大設定粒子数でframe時間を計測し、各particleの範囲が支持半径clamp（各axis最大8pixel、整数境界を含むbboxは最大18×18sample）によりboundedであることを確認する。さらに弾性圧縮・伸長および塑性履歴後の複数`Fe`状態・伝播方向について、PK1の摂動差分から構成則接線とacoustic tensorの最大特性波速を評価し、現在の参照密度`mass/volume0`と未変形Lamé係数を使うCFL波速の保守性を確認する。初期の弾性軸probeは`Fe=diag(s,1)`で`P11=(2μ+λ)log(s)/s`、`∂P11/∂s=(2μ+λ)(1-log(s))/s²`となる。`μ=1, λ=1.5`では現配置acoustic tensorを4,001方向で走査した対角probe群の最大が主軸にあり、`s=0.5`で基準速度の約1.30倍だった。Sun et al. 2020式(3)は比較根拠とするが、plastic algorithmic tangentへ直接外挿しない。一般の非対角・塑性状態も含めて数値差分と配置変換を評価し、必要ならCFL式を根拠付きで改訂する。
   - CFL acoustic upper-bound contract: compare the analytic principal-basis tangent with PK1 finite differences for repeated and separated singular values; scan the current-space acoustic tensor over propagation directions and verify no eigenvalue exceeds the Gershgorin bound; verify `Fp` cancellation for fixed plastic state and `dt*(waveBound+advectionBound) <= 0.05*dx`. Repeat after plastic return-map updates as a sequence of frozen-state elastic probes. This does not substitute for the independent solver transfer/collision stability fixtures above.
4. **backward compatibility test:** 既存CorotatedVonMises presetについて粒子軌跡/snapshotまたは既存fixture結果を固定し、モデル追加によるShape側差分を検知する。
5. **保存・再生test:** Layer JSONの材料設定とseed、複数Granular Layerでの共通checkpoint選択、clock remainderを含むall-or-nothing restore、checkpoint miss時の全solver baseline復元、Undo/Redo後のsolver再生成を分離して検査する。
6. **GPU capability test:** GPU未対応モデルがCPUへ明示fallbackすることを確認。GPU実装後に限りCPU/GPU state parityを別gateとする。

テストは内部式をそのまま期待値に再実装するだけでなく、解析可能なstress states、保存したgolden fixture、物理量の上限／下限を使う。決定性のbit-exact保証は同一backend・同一実行環境内に限定し、異backend比較は明記した数値許容差とする。

## 関連

- `docs/planned/MILESTONE_PHYSICS_PRODUCTION_HARDENING.md`
- `docs/planned/MILESTONE_PROFESSIONAL_SOFT_BODY_2026-07-11.md`
- `docs/planned/MILESTONE_PHYSICS_TESTBENCH_SOFT_BODY_2026-10-08.md`
- `ArtifactCore/include/Physics/MpmSolver2D.ixx`
- `ArtifactCore/src/Physics/MpmSolver2D.cppm`
- `ArtifactCore/src/Physics/PhysicsSystem.cppm`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`

## 初回調査判定（2026-10-11）

**CPU MPM砂の専用Layer・生成導線・設定保存・描画packetとqueue overflow recovery・Composition単位replayは実装済み。CPU数値安定性、build/link、undo/clone lifecycle、seek/render/offline exportのruntime一致は未検証。GPU simulationは初版対象外。**


- 2026-10-12: CFL hard rangeをseed grid/物性に応じて動的に狭める案は不採用。Property Editorの数値欄は生成時にmetadata rangeをspinboxへ設定し、既存欄をrange変更に追従させる公開refresh経路が見つからなかった。さらに物性個別の範囲だけでは、列/行・source寸法との組み合わせをUI上で保証しない。既存factoryのCFL guardとrollback、および実行中backlog statusを正とし、range metadataを動的改変しない。UI runtime未検証。

- 2026-10-12: Initial solver failure cannot be surfaced through the current Property Editor status row: Composition append/insert paths call `setComposition`, then reject and detach a new layer when `isCompositionAttachmentReady()` is false, before it enters the layer list. Property edits on an already attached layer rollback failed solver reconfiguration and preserve its previous solver. A useful degraded-layer recovery path would require an explicit Composition attachment policy change; no UI warning was added because there is no reachable selected layer state to display it.

- 2026-10-12: MPM Sand menuの作成操作はvoidのProjectService APIを呼び、失敗時も結果を表示していなかった。同期追加の前後で対象CompositionのLayer数を比較し、追加されなければ既存の dialog.layer.add_failed 翻訳によるwarningを出す。新しいevent/API/localeは増やさない。作成失敗経路のUI runtime確認は未実施。
