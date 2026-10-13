**最終更新:** 2026-10-14

# Insight Register

## 2026-10-14: RT shading geometryへvertex colorを統合

- **関連:** `ArtifactCore::RayTracingManager`、`ArtifactIRenderer`、Raster／RTのPrincipled shading。
- **確認済み事実:** Raster `MeshRenderer` は頂点カラーRGBをsRGB→linear変換してbase colorへ乗算し、頂点alphaもmaterial alphaへ乗算する。RT shading geometryは位置・法線・UV・primitive material indexを保持していたが、vertex color入力はなく、RT closest-hitとMasked any-hitで未使用だった。
- **変更:** 頂点カラーをRT geometry bufferへ追加し、closest-hit base colorとany-hit alpha clipへ渡す。vertex colorがないgeometryは白として扱う。
- **価値／懸念:** 頂点カラー付きmeshのRaster／RT差を減らし、頂点alphaを使ったMasked材質をRTでも表現する。各importerが出す頂点RGBのtransfer encoding契約は一元化されておらず、Raster既存処理へ合わせたためglTF等のlinear vertex colorの扱いは未解決。compile／画像parity未確認。
- **次に確認:** source importerごとの色空間契約を整理し、頂点色fixtureでRaster／RTとalpha clipを比較する。

### 2026-10-13 — Render Queueのテキスト統合テストはshaping parityまで段階化する

- **関連:** `tests/Artifact/RenderQueueLayerImageIntegrationTest.cpp`、`tests/Artifact/CMakeLists.txt`、`tests/ArtifactCore/TextShapingTest.cpp`、`tests/Artifact/TextGlyphRenderContractTest.cpp`。
- **確認できた事実:** Render Manager本番queue経由の通常Text Layer画像テストは既存だった。Source Textは`setSourceTextAtFrame()`でproperty keyframe化される。Core shaping suiteにはアラビア語、デーヴァナーガリー、ZWJ family emoji、肌色modifier等の低レベルcaseがある一方、Render Queueからそれらを含む文を描画するcaseはなかった。
- **変更:** queue integrationへ2-frame Source Text、アラビア語+結合記号/デーヴァナーガリー数字の混在文、ZWJ family/肌色modifier/国旗/keycap emojiの描画caseを追加し、個別CTest entryを登録。各出力の明るいglyph pixel存在を検査する。
- **価値／懸念:** low-level shapingやGPU glyph submitterの検証と、本番queue/合成/出力の結合を区別して回帰検知できる。現在のpixel-count assertionは文字が何らかの形で描画されたことを確認するだけで、glyph順序・cluster単位・色付きemojiの完全な見た目を保証しない。フォントfallbackは環境依存。
- **実行結果 (2026-10-13):** Windows Debug / D3D12 headlessで3件の個別CTestがすべて成功した。Source Textケースでは `text.value` property生成のため `getLayerPropertyGroups()` を先に呼ぶ必要があり、Composition duration 3・Custom range `[1,3)` で2枚を出力する。最初はdurationを誤って2に上書きしていたため2枚目が範囲外だった。
- **次に確認:** 現在の画素数assertionはglyph順序・cluster単位・色付きemojiの完全な見た目を保証しない。必要なら固定font fixtureと承認baselineでshaping/emoji visual parityを追加する。

### 2026-10-12 — Partial module rebuilds can retain stale allocation templates

- **関連:** `Artifact/include/Render/GPUTextureCacheManager.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** 通常ログに出ない first-chance UUID 解析例外と、終了時の `GPUTextureCacheManager` ヒープ破損を native Windows debugger で捕捉した。空の bulk Removed ID を nil として扱い、追加の生成所有者 `ArtifactRenderQueueService.cppm` を再コンパイルして canonical standalone bench を再リンクした後、両方の障害は再発しなくなった。D3D12VA output texture に simultaneous-access flag を指定した後、COPY queue の barrier layout ERROR も消えた。H.264 1/2 層・3 秒ずつの実行で fault exception / CRT heap assertion / D3D12 ERROR はすべて 0。性能条件は引き続き未達。
- **設計上の推論:** stale な `make_unique<GPUTextureCacheManager>` の COMDAT が別の object に残ると、生成側の古い class size と新しい実装が混在し得る。個々の constructor 命令サイズは未比較だが、生成所有者の再コンパイルでヒープ破損が解消した。
- **価値／懸念:** exported class のメンバー追加後は生成／破棄所有者も同じ module layout にそろえる必要がある。通常ログだけで例外なしと判断しない。simultaneous-access は fence の代替ではなく、texture compression / bandwidth への影響は未測定。
- **次に確認:** partial build を続ける場合の importer／allocation-owner freshness を機械的に照合する方法を検討する。性能検証再開前に canonical exe と DLL を固定し、cache enabled / ProRes と長時間の runtime fault gate を確認する。

未着手の設計判断、現在の優先方針に直結する実装候補、実機検証待ちだけを記録する。実装済みの詳細履歴と過去の調査は [Insight Archive (through 2026-09-01)](docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md) を参照。

### 2026-10-12 — Animation Dynamics spring/damperの独立テスト対象
- **関連:** `ArtifactCore/include/Animation/AnimationDynamics.ixx`、`tests/ArtifactCore/AnimationDynamicsStandaloneTest.cpp`、`tests/ArtifactCore/AnimationDynamicsStandalone/CMakeLists.txt`。
- **確認できた事実:** `Animation.Dynamics`には1D/2D/3D spring-damper、Lag Follower、DynamicsChannelがあるが、既存testsには同module専用suiteがなかった。既存module interfaceは`Core.ArtifactString`だけをimportするため、standalone module interface projectで依存moduleを追加せずに登録できる形。
- **変更:** Artifact.exeやArtifactCore全体へリンクしないstandalone GTest targetを追加し、初期seed、更新方向、非有限target保持、dt上限clamp、2D軸独立、channel resetを契約化した。
- **価値／懸念:** 小さな決定的なsuiteでバネ挙動の回帰を分離できる。今回の更新方向caseは振動・収束率や解析解との誤差を主張するものではなく、resetは次回updateのtargetへ通常seedする契約を確認する。
- **次に確認:** ユーザー許可後にstandalone CMakeをconfigureし、MSVC module interface / GTest targetのbuildとCTestを実行する。数値安定性、preset/質量境界、3D、Lag/Off、overshoot limiterは追加候補。

### 2026-10-12 — Granular MPM Plane colliderを画面座標の床へ合わせる
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`resolveColliders()` Plane branch)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid()` gravity contract)。
- **確認できた事実:** MPM SandはCompositionの画面座標で重力`+Y`（既定`980 px/s²`）を使う。一方、Plane colliderは従来 `particle.y < plane.y` を `+Y` 法線で押し戻すため、`+Y`方向へ落下する粒子はplaneを越えても接触判定されず、既存の向きはy-up floorだった。Composition collider生成はBox/Circle/Polygonを使いPlaneを生成していない。
- **変更:** Granular Drucker–PragerのPlane branchだけをy-down座標向けhorizontal floorに変更し、`particle.y > plane.y` で位置をplaneへ戻して法線`-Y`のGranular速度応答を適用する。従来Corotated MPMのPlane挙動は維持。
- **価値／懸念:** 公開Granular solver APIでPlaneを床として使う場合、落下・法線反発・接線摩擦が重力座標と一致する。Granularの天井/任意向き片側planeを表すAPIはなく、このplaneは床として定義する。砂のplane接触はruntime未検証。
- **静的符号照合:** 落下速度`vY>0`と床法線`n=(0,-1)`では`vn=-vY<0`となり、共通応答式から反発後`vY'=-e*vY`。上向き離反時は`vn>0`なので法線速度を再反射しない。接線速度とGranular APIC `C`には既存摩擦projectionが適用される。
- **次に確認:** `y<plane`, `y=plane`, `y>plane`で静止/接近/離反を検査し、`+Y`落下速度への反発と接線friction、Corotated legacy応答をfixtureで確認する。

### 2026-10-12 — MPM moving colliderは幾何位置だけで速度を伝えない
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`evaluateLayerCollisionPairs()`)、`ArtifactCore/include/Physics/MpmSolver2D.ixx` (`MpmCollider2D`)、`ArtifactCore/src/Physics/MpmSolver2D.cppm` (`resolveColliders()`、`applyGranularColliderVelocityResponse()`)。
- **確認できた事実:** Compositionは各frameのsource geometryをsolver local空間へ再配置するが、`MpmCollider2D`はposition/size/restitution/frictionだけでlinear/angular velocityやprevious poseを持たない。Granular接触応答は粒子速度の法線/接線成分だけを補正し、接触時にsource velocityを差し引いたり加えたりしない。粒子位置のpenetration correctionは実行する。
- **推測／未検証:** アニメーション床・壁は砂を幾何的に押し出せても運動量を砂へ与えず、sourceが離れた後の粒子速度が静止のままになる可能性がある。step幅・貫通量・連続するframeの接触履歴による見た目はruntime未確認。
- **価値／懸念:** 動的colliderの transform追従はposition-only kinematic contactであり、運動学的接触ではない。速度を追加する場合は前frame pose、Compositionからsolver localへの相対速度変換、polygon/circleの接触点に応じた角速度、seek replayの再構築を揃える必要があり、単純にtransform差分を一様速度として足すのは回転体で誤る。
- **次に確認:** 静止粒子へ平行移動床・回転polygonを一定速度で当てる定量fixtureを作り、現行のposition-only応答をbaseline計測した上で、動的接触速度を初版要件に含めるか決める。

### 2026-10-12 — 通常再生のmaterial colliderはComposition frame transformを評価
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`setFramePosition()`、`evaluateLayerCollisionPairs()`)、`Artifact/src/Layer/ArtifactAbstractLayerTransform.cppm` (`getGlobalTransform()` / `getGlobalTransformAt()`)。
- **確認できた事実:** PlaybackEngineは通常再生でComposition `setFramePosition()` を呼ぶ。`ArtifactAbstractLayer::goToFrame()` は明示seek/replayでのみ呼ばれ、layer `currentFrame_` は通常stepでは更新されない。`getGlobalTransform()` のcache keyはlayer `currentFrame_`である一方、transform cache値にはComposition時刻で変化するkeyframeが含まれるため、通常再生中に従来collision evaluationが同APIを使うとtransform cacheが旧layer frameのままになる。seek replayは各layerを先に`goToFrame(currentFrame)`するため症状が表面化しにくい。
- **変更:** `evaluateLayerCollisionPairs()` のcollision source/target transform取得を `getGlobalTransformAt(position_.framePosition())` に変更した。親layerも同じ明示frameで再帰評価する。各simulation frameで再構築するcollider proxyとinverse transformがComposition frameに追従する。
- **価値／懸念:** 通常再生時に動くcollision source、移動するMPM/soft material target、親transform chainを現在frameで評価できる。明示transform評価のCPUコストは増える可能性があり未計測。ほかの `getGlobalTransform()` consumerが持つ同種問題は別途調査対象。
- **次に確認:** 通常playbackとseek replayでanimated box/circle/polygon colliderのlayer-local proxy座標を同frame比較し、静止sourceとanimated target、親子transformを含む受入確認を行う。

### 2026-10-12 — MPM Sand parameter previewをcommit-onlyにする
- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` (`previewLayerValue()`)、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm` (`applyPreviewValue()` / `applyCommitValue()`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** Shared property rowはpreview時に先に`AbstractProperty` cacheを候補値へ更新し、その後void preview handlerを呼ぶ。MPM Sand setterはsolver再生成失敗時にauthoring値/cacheをrollbackするが、PropertyWidget preview handlerはsetterのboolを無視し、失敗後のcache状態をeditorへ再同期する契約がなかった。MPM parameter editはsolverを再生成し粒子をseedし直す。
- **変更:** `ArtifactPropertyWidget::previewLayerValue()` は `ArtifactMpmSand2DLayer` の `mpmSand.*` propertyをpreview applyせずreturnし、編集をcommit boundaryに限定した。既存commit pathはsetter後にproperty cacheと候補値の差を検出してeditorをcanonical valueへ戻す。実装TUへ `Artifact.Layer.MpmSand2D` importを追加。
- **価値／懸念:** slider scrub中の頻繁なsolver再生成・砂状態破棄を止め、solver生成拒否時もcommit callbackのcanonical resyncを使える。砂の連続live previewはなくなるが、実際の編集ではcommit時に適用される。UI runtimeは未確認。
- **次に確認:** MPM parameterのspinbox text edit、slider scrub+release、solver予算超過によるcommit拒否、Undo/Redo、multi-selectionで表示値・solver state・command履歴が同期することをruntime確認する。

### 2026-10-12 — MPM Sand multi-selection commitは失敗時に部分適用しうる
- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` (`commitLayerValue()`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm` (`setLayerPropertyValue()`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid()`)。
- **確認できた事実:** PropertyWidgetは選択対象を順に更新し、各 `setLayerPropertyValue()` の戻り値を無視する。MPM setterはsolver生成失敗時に当該Layerだけauthoring値とproperty cacheをrollbackするが、既に成功した別Layerのsolverをまとめて戻すmacro transactionはない。各Compositionの粒子・grid/checkpoint予算により、同じ編集でもtargetごとに生成可否が異なり得る。
- **推測／未検証:** 複数Compositionまたはworld予算の限界付近で選択MPM Layerへ一括編集すると、一部だけ新設定/再seedされ、代表Layer editorの表示値は代表Layerの成否だけを示す可能性がある。具体的な予算配分再現は未実施。
- **価値／懸念:** 一括編集の原子性・表示値整合に関わる。正しくrollbackするには事前に全target solver payload/grid予算をreserveするか、旧solver state・world clock・checkpointを保持するtransaction APIが必要。後者は大きなparticle payloadを二重保持しうるため、安易な逐次rollbackは旧simulation historyを復元しない。
- **次に確認:** MPM multi-selectionを許す製品仕様と同一Composition/worldの有無を整理し、world単位preflight/commit APIまたは失敗対象別の結果表示を設計する。現行依頼の範囲では未修正。

### 2026-10-12 — MPM frame-rate変更時はworld clock remainderもreset
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`setFrameRate()`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`invalidateGranularMaterialSnapshots()`、`setPhysicsTimelineSettings()`)。
- **確認できた事実:** Granular checkpointはframe位置とComposition world-clock remainderを対で保存する。frame rate変更時、Compositionは旧frame→秒マッピングのcheckpointを捨てるが、従来はlive world clock remainderを保持していた。再生replayはcache miss時にsolver baseline/current stateからframe 0扱いで進むため、旧時間軸のremainderが残ると新しいframe delta列と二重計上される。Composition単位のcache invalidationへ任意のclock resetを追加し、`setFrameRate()`だけ明示的にresetするよう修正した。全体Physics timeline設定の変更では他Compositionへ副作用を出さないようworld clockを維持し、granular checkpointだけを消去する。
- **価値／懸念:** Frame-rate変更後の新しいframe-time mappingと粒子clockの開始境界を一致させる。solver粒子状態はresetせず保持するため、速度・変形履歴の時間解釈を新rateへつなぐ意味論は製品上の選択として残る。変更APIはビルド/runtime未検証。
- **次に確認:** frame rate変更直前に非ゼロclock remainderを作り、rate変更後frame 0/1 replayと連続再生の粒子stateが新時間軸で一致するfixtureを実行する。

### 2026-10-12 — MPM Hencky-Drucker-Prager基準例を独立算術照合
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`projectMpmGranularHencky2D()`、`applyGranularPlasticity()`、`firstPiolaKirchhoff()`)、`docs/planned/MILESTONE_MPM_GRANULAR_SAND_2026-10-11.md`。
- **確認できた事実:** 文書の`μ=1, λ=1.5, φ=35°`, `ε=(-0.5,0.3)`例をPythonのdouble算術で再計算し、`α=0.4055797877`, `δγ=0.3628955311`, `H=(-0.2433941091,0.0433941091)`, `tr(H)=-0.2`を得た。実装は`||dev ε||F + (1+λ/μ)tr(ε)α`と同じ式を使う。Hencky Kirchhoff主応力から各特異値で割り、`U`/`V`基底へ戻す処理は`P=τ Fe^{-T}`と一致し、投影点のyield residualは約`-5.6e-17`。
- **価値／懸念:** 代表的な2D return mapping係数・符号・主応力変換は文書例と整合する。これは独立算術の確認で、C++ float実装・特異ベクトル再構成・shear/rotation・連成MPMの安定性を検証したものではない。
- **次に確認:** 許可された数値fixtureでhydrostatic compression、extension tip、pure shearのtrace保持、回転付き非対角Fe、`F=Fe Fp`再構成誤差、非有限入力拒否を評価する。ビルド／テストはユーザー指示待ち。

### 2026-10-12 — MPM friction hardening曲線の最大値は原典どおり

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::hardenedMpmFrictionAlpha2D()`
- **確認できた事実:** 実装式はKlár et al. 2016式(30)の`phi= h0 + (h1*q-h3) exp(-h2*q)`に対し、`h0=初期摩擦角+h3`としてq=0でLayerの設定角になるよう調整している。`h1=9°`, `h2=0.2`, `h3=10°`は同論文Table 3に記載された係数で、原典はこの曲線にmaximumとasymptoteがあると明記する。実装の89°上限は数値上の角度境界を守る追加clamp。
- **価値／懸念:** q≈6.1で増加から減少へ転じる形はこの式の特性であり、単調hardeningが必要と決めつけて変更すべきではない。論文は3D手法を説明しており、現行2D yield conversionとの物理的一致や、本UI係数解釈のruntime妥当性までは保証しない。
- **次に確認:** q=0、ピーク近傍、q=100の摩擦角・alphaを数値fixtureで固定し、hardening state上限と初期角80°近辺の89°clamp連続性を確認する。

### 2026-10-12 — MPM Sandの物性編集はsolverを再seedする
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm` (`setLayerPropertyValue()`、`configureSolver()`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid()`、`setGranularMaterialSolver()`)。
- **確認できた事実:** MPM Sandの物性・重力・seed列数/行数のproperty変更は、編集中Compositionがあれば`configureSolver()`を呼ぶ。PhysicsSystemは同じComposition world IDとLayerIDの既存solverを新solverへ置き換え、world snapshot群とworld clock accumulatorを消去/ゼロ化する。新solverは初期seed粒子から生成されるため、編集時点の粒子変形・速度・堆積状態は引き継がれない。solver生成が失敗した場合はLayer側authoring値とproperty cacheをrollbackする。
- **価値／懸念:** 物性変更後に古い物性の履歴を混ぜない明瞭な意味論だが、連続previewでは値が変わるたびに再seedされ、途中でシミュレーションした状態を破棄する。UI操作として意図的なresetなのか、物性だけを現状態へ反映するべきなのかは仕様未確定。単純に状態を引き継ぐとconstitutive history・particle mass/reference volume・checkpoint互換性の整合が必要になる。
- **次に確認:** Property Editorでのpreview/commit、複数選択、Undo/Redo時にsolver再seedが何回起きるか、またユーザーにreset意味論が明示されるかをruntime受入れで確認し、必要なら物性変更とseed解像度変更を別扱いにする仕様を決める。


### 2026-10-12 — Granular CFL floorより速い状態を緊急scaleする
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`updateGranularTimeStepForState`、`setTimeStep`)。
- **確認できた事実:** Granular CFL推定は`C`由来affine velocityも含む一方、共通`setTimeStep()`は`1e-6 s`未満を受け付けない。要求刻みがさらに小さい場合、従来はCFL要件を満たさない最小値へclampされていた。
- **変更:** 波速を差し引いた最小刻み互換の移流速度上限を求め、それを超える粒子だけ`vel`と`C`を同一比率で縮小して0.1%の余裕を作る。通常上限内の粒子状態には触れない。
- **価値／懸念:** timestep floor後もaffine stencil速度boundに対するCFLを維持し、過大な有限速度勾配が一歩で格子を飛ぶのを抑える。発火時は物理速度・affine stateを失う非常用の挙動で、発生頻度と材料品質への影響はruntime未検証。
- **次に確認:** 最小刻みの直前・直後でCFL bound、particle momentum、affine fieldの縮小連続性を測り、異常なパラメータ・入力だけが発火することをfixtureで確認する。


### 2026-10-12 — MPM Sand CFL boundにAPIC affine速度を含める
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`updateGranularTimeStepForState`、P2G)。
- **確認できた事実:** P2G node速度は`p.vel + C*dpos`を使うが、従来のGranular CFL速度上限はparticle centerの`|p.vel|`だけを集計していた。純せん断・回転ではcenter speedが小さくてもstencil内node速度は`C*dpos`分だけ大きくなる。
- **変更:** 粒子ごとに`||C||F`を計算し、4×4 cubic stencilのsupport `|dpos| < 2√2 dx`から得るaffine速度上界を中心速度へ加算してCFL characteristic speedへ渡す。
- **価値／懸念:** 大きな局所velocity gradientを含む状態でtimestepを過大に保つ危険を抑える。Frobenius boundはdirectional boundより保守的になり、step数増加のruntime影響は未計測。
- **次に確認:** pure shear、rigid rotation、併進+shearで`max_i |v + C dpos|`を実サポート上走査し、bound超過がないこと、同一seed/frame結果とCFL step予算を確認する。


### 2026-10-12 — Granular接触でAPIC affine fieldも投影する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`applyGranularColliderVelocityResponse`、`stepOnce`、P2G)。
- **確認できた事実:** P2Gは粒子速度に加えて`C * (x_i - x_p)`を格子運動量へ足す。G2P後のcollider/domain補正は粒子`vel`と位置を変更するが、従来は`C`をそのまま残していた。
- **変更:** Granular colliderでは`(I - nnᵀ)C`で法線方向のaffine出力を除き、接線成分へfriction retentionを適用する。粒子domain clampでも同じ法線投影を軸ごとに行う。Corotated経路は変更しない。
- **価値／懸念:** 接触後に残る法線affine velocityが次stepのP2Gで壁向き運動量を再注入する経路を抑える。接線摩擦と角・複数接触の相互作用は数値fixture/runtime未確認。
- **次に確認:** restitution/frictionと壁接触の接近・離反・接線/回転場で1step momentumを測り、affine projection前後の有限性と固定step収束を確認する。


### 2026-10-12 — MPM砂surfaceに固定density閾値がある
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm` (`densityAlpha`、`draw`)。
- **確認できた事実:** `densityAlpha()`はdensity `<=0.08`を完全透明にし、triangle生成もquad内の4 sampleがすべて`<=0.08`なら省略する。solverのdensity fieldには0..0.08の正値が保持されても表示されない。
- **推測／未検証:** 塑性流動後に粒子量が広い領域へ薄く伸び、sample densityが閾値を下回ると、物理状態が残っていても砂面が途切れるか消える可能性がある。実際の伸長分布・アンチエイリアス・重なりと閾値の関係はruntime未確認。
- **価値／懸念:** 薄い砂層の可視下限が固定値0.08で決まる。閾値を下げると遠方・疎な粒子のhaloや面積膨張を招くため、単純変更ではなくmass-preserving surface再構成との比較が必要。
- **次に確認:** 圧密 pile・長い薄層・field端を含む粒子配置でfield積分値とrender alpha footprintを測り、mass保持と輪郭の両方から閾値を調整する。


### 2026-10-12 — 弾性MPMのGPU初期化失敗時に時間を二重加算する経路
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`updateGPUSubsteps`、`update`)。
- **確認できた事実:** GPU substepは初期化前に`accumulatedTime_`へframe deltaを加算する。Compute初期化失敗時は`false`を返し、呼出元はCPU経路へ進んで同じ`elapsedSeconds`を再加算する。GranularモデルはGPU backendを拒否するため、この経路には入らない。
- **推測／未検証:** GPU初期化に失敗してCPU fallbackした最初のupdateで、上限に達していなければ同じ時間を2回分stepし、弾性MPMのsimulation timeが1frameだけ先行する可能性がある。実GPU failure/runtime再現は未確認。
- **価値／懸念:** GPU unavailable/初期化失敗後にCPUへfallbackする弾性MPMの時間軸とseek決定性へ影響しうる。Granular MPMの変更範囲外なので今回は未修正。
- **次に確認:** 初期化失敗時にGPU attempt前のaccumulatorを保存・復元する案を検討し、失敗前後でGPU fallbackとCPU-onlyのparticle stateが一致するfixtureを作る。実装修正はelastic GPU fallback責務として別途扱う。


### 2026-10-12 — Granular snapshot塑性硬化履歴の範囲を検証
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`MpmSolver2D::canRestoreSnapshot`)。
- **確認できた事実:** Granular return mappingはhardening historyを0..100へclampして使い、更新も上限100で保存する。一方snapshot validationは有限性しか見ず、negativeまたは100超の有限値をrestoreできた。
- **変更:** Granular snapshotだけplasticStrainが `[0,100]` 外ならrestoreを拒否する。既存Corotated snapshot条件は変更しない。
- **価値／懸念:** malformed checkpointが摩擦硬化係数を意図せず変え、seek結果を分岐させるのを防ぐ。保存済みcheckpointがすべて現在のsolver生成由来なら互換性影響はない見込みだが、runtime未確認。
- **次に確認:** 初期snapshot・塑性化後snapshot・値域境界のrestoreと、破損値拒否をfixture化し、restore後stepの粒子状態が連続することを確認する。


### 2026-10-12 — MPM collider geometryを登録時に検証
- **関連:** `ArtifactCore/include/Physics/MpmSolver2D.ixx` (`MpmSolver2D::addCollider`)。
- **確認できた事実:** collider geometryはsolver内部の接触計算で直接使われ、登録APIは検証なしに保存していた。Plane/Circle/Boxの必須座標や寸法がNaN/Infの場合、またPolygon配列が不完全または非有限の場合、粒子位置判定・補正へ不正値が伝播する。
- **変更:** collider typeごとに必要geometryの有限性を登録時に検査し、未知typeと不正Polygonを拒否する。friction/restitutionの方針は係数別のGranular fallbackに委ねる。
- **価値／懸念:** フレームごとの接触計算へ不正形状が入る前に一度だけ遮断し、Polygonの頂点scanもcold registrationに閉じる。既存呼出元が不正な負寸法・radiusを意図していないか、reject時に診断が必要かは未検証。
- **次に確認:** 正常な各collider typeの既存形状が従来どおり登録されることと、不正値が登録されないことをAPI fixtureで確認する。


### 2026-10-12 — MPM砂colliderの非有限摩擦係数を無反応へ退避
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`resolveColliders`)。
- **確認できた事実:** `std::clamp(NaN, 0, 1)`は有限値へ正規化しない。Composition側のproperty読取はfloat値を取得するが、すべての入口で有限性を保証するとは限らず、NaN係数がGranular contact responseまで届く可能性がある。
- **変更:** Granular経路に限り非有限friction/restitutionを0へ置換してからclampする。Corotated legacy経路は維持する。
- **価値／懸念:** 不正係数1つでparticle velocityへNaNが伝播しsolver状態を壊すのを防ぐ。通常のUI/API入力でNaNが到達するか、退避時の診断が必要かは未検証。
- **次に確認:** 不正係数を持つPlane/Circle/Box/Polygonのcontact fixtureで粒子状態が有限のままか確認し、API境界での一貫した係数validationの要否を判断する。


### 2026-10-12 — Granular Plane/Box contactは内向き速度だけを反射する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`resolveColliders`、`applyGranularColliderVelocityResponse`)。
- **確認できた事実:** Circle/PolygonはGranular向け共通法線応答を使う一方、PlaneとBoxは旧式`abs(vNormal)*restitution`を無条件に使っていた。貫通補正で位置を接触面へ戻す際、接触面から離れる粒子速度も反転し、法線速度の符号が変わっていた。
- **変更:** Granular Plane/Boxで接触面の外向き法線を共通応答へ渡し、内向き法線速度のみ反発し、接線成分だけ摩擦する。既存Corotatedの応答は維持する。
- **価値／懸念:** collider貫通補正が接触面から離れる運動を逆流させる問題を防ぐ。Box cornerのtie breakや複数colliderの逐次応答は runtime 未検証。
- **次に確認:** Plane/Boxそれぞれで内向き・外向き法線速度、接線速度、restitution=0/1、摩擦=0/1をfixture化し、既存elastic MPM結果が不変であることを確認する。

### 2026-10-12 — 細長いレイヤーでGranular density kernelを楕円化する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`writeDensityField`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm` (128×96 density field)。
- **確認できた事実:** Granular density kernelは等方円形だが支持半径は`8*min(pixelWorldWidth,pixelWorldHeight)`で制限される。ピクセル寸法比が約8を超えると長辺pixel方向の半径が1pixel未満となり、粒子phaseによって局所kernel sampleが欠けたり、総weightが0となり粒子をfieldへ出せない可能性がある。
- **変更:** Granularのraster-only kernelをaxis-aligned ellipseにし、各軸ごとにpixel diagonal由来のcoverage floorと8pixel ceilingを設定する。粒子ごとの離散weight normalizationと最大18×18 stencilを維持し、既存Corotated density pathは円形のままにする。
- **価値／懸念:** 極端なlayer aspect ratioで砂が密度fieldから脱落する経路を塞ぐ。ワールド空間では異方的な可視supportとなるため、fieldは物理shapeではなく表示用surfaceの再構成だと明確に扱う。絵作り・alpha parityはruntime未確認。
- **次に確認:** aspect ratio 1:1、8:1、16:1超で複数subpixel particle phaseを走査し、field積分・surface連続性・最大18×18 stencil boundを検証する。

### 2026-10-12 — MPM grid seed APIで導出値をreserve前に検証する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`MpmSolver2D::addParticlesGrid`)。
- **確認できた事実:** APIは入力引数が有限であることを確認していたが、`w*h`、粒子volume/mass、導出spacingや端点のfloat演算結果は検証せずvector reserveしてから粒子を追加していた。直接呼び出しで積がoverflowすると不正粒子や一部初期化状態を作る余地があった。
- **変更:** seed位置・spacing・volume・massを計算して有限かつ正を確認し、成功後にのみvector reserveとparticle-grid metadata更新を行う。
- **価値／懸念:** Granular factory以外からの直接利用も壊れたseedを登録しにくくする。allocation例外は既存API契約に委ね、runtimeでは未確認。
- **次に確認:** 極端値と通常値でseed追加のall-or-none性、Corotated既存呼び出しの互換性、Granular seedの有限性を確認する。

### 2026-10-12 — Granular seedはparticle volumeと一致するcell-centered配置にする
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`addParticlesGrid`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid`)。
- **確認できた事実:** Granular particle volumeは`w*h/(columns*rows)`だが、初期位置は`w/(columns-1)`間隔で両端を含んでいた。点群のVoronoi代表面積は特に低い列/行数で割当volumeとずれ、PhysicsSystemのCFL用spacingも同じ端点間隔を使っていた。
- **変更:** Granularだけ`w/columns`、`h/rows`間隔のcell centerに粒子を配置し、CFL cell sizeもその小さい間隔（1px floor）から求める。既存CorotatedVonMisesの粒子配置はそのまま維持する。
- **価値／懸念:** 初期質量が表す矩形面積とseed配置の代表領域を揃え、粗い粒子設定でも端の配置に起因する系統的な密度差を抑える。離散support・pile安定性・従来presetへの無影響はruntime未確認。
- **次に確認:** 低解像度と標準解像度で粒子群の重心・総質量・初期密度fieldを比較し、shape material fixtureに変化がないことを確認する。

### 2026-10-12 — MPM SandのE/Poisson比はplane-strain係数を使う
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`setMaterial`、Hencky stress)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid`)、MPM Sandの材料property UI。
- **確認できた事実:** 現行Lamé係数は`lambda=E*nu/((1+nu)(1-2nu))`で、plane-stressで使う`E*nu/(1-nu^2)`ではない。一方、降伏面は一次資料の2D in-plane Drucker–Prager定義を使い、out-of-plane stressを含めない。
- **推測／未検証:** 現行構成則はplane-strain stiffnessと2D yieldを組み合わせた準2D制作モデルとして意図されているように見えるが、実砂の校正やUIに示すE/Poisson比の解釈はruntime/設計レビューで確認されていない。
- **価値／懸念:** E/nuが暗黙の次元変換として解釈される誤解を避けるため、初版の仕様に「2D in-plane deformation + plane-strain Lamé stiffness + 2D Drucker–Prager yield」と記録する。plane-stressへ変更すると圧縮剛性・安息角・体積応答も変わりうる。
- **次に確認:** material panelのtooltip/docでこの制作向け意味論を伝えるか設計レビューし、plane-stress/plane-strain解析fixtureとpile compressionを比較する。今回はUI挙動を変更していない。

### 2026-10-12 — Granular solver登録前にfloat seed計算と粒子数を確認する
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid`)、`ArtifactCore/src/Physics/MpmSolver2D.cppm` (`addParticlesGrid`)。
- **確認できた事実:** PhysicsSystemは総体積・粒子体積・質量をdoubleで検証するが、`addParticlesGrid()`はfloatで`width*height`、粒子volume、massを再計算する。float丸めやoverflowで粒子生成がno-opでも、以前は空solverをbaseline登録できた。
- **変更:** 実際のfloat式とseed centerを登録前に検証し、solverが要求粒子数を生成し、position/mass/volumeが有限かつ正の場合だけregistryへ公開する。
- **価値／懸念:** Compositionへ有効な粒子なしsolverが登録され、Layerだけが初期化成功扱いになる不整合を防ぐ。極端値と実allocator容量での動作はruntime未確認。
- **次に確認:** 有効範囲の端・float underflow/overflow近傍・grid予算超過を生成APIで確認し、失敗時に既存solverとMaterial設定が維持されることを検証する。

### 2026-10-12 — MPMの共有fixed-step時計で非有限値を既定値へ戻す
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm` (`setPhysicsTimelineSettings`、`advanceGranularMaterialSolvers`)。
- **確認できた事実:** `std::clamp(NaN, min, max)`はNaNを既定範囲へ戻さない。Granular world clockは`accumulator >= fixedDt`で進むためfixedDtがNaNだとstepが一度も実行されない。
- **変更:** 非有限fixed stepだけ1/60秒へ置き換えてから従来の範囲へclampする。
- **価値／懸念:** 不正な設定値で物理時間が静止する状態を防ぐ。呼び出し側が意図してNaNを渡す設計は見つかっておらず、fallback経路はruntime未確認。
- **次に確認:** PhysicsTimelineSettingsの保存・供給経路で非有限値が作られる可能性を確認し、CPU MPM playback/seekでは一定のfixed-step sequenceが維持されるか検証する。

### 2026-10-12 — Granular MPMのpositive SVDはスケーリングして小特異値を復元する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`decomposePositiveMpm2D`)。
- **確認できた事実:** 2×2 Gram行列をfloatで直接二乗すると大きな有限変形勾配でoverflowし、`mean - radius`で小さい固有値を求めるとほぼ等方な伸長で精度を失う。さらに元スケールの`det(F)`を先に評価すると、Gram行列より前にoverflowしうる。
- **変更:** 行列をscaled Frobenius normで正規化してprincipal eigenvalueを求め、scaled determinantと最大特異値から最小特異値を復元する。正値性判定も元スケールdeterminantを使わない。
- **価値／懸念:** 有限値を保てる大伸長とほぼ等方状態でSVDが不必要に失敗する経路を減らす。float determinant自体がoverflowする状態や極端に悪条件な圧縮は依然として無効判定となりうる。
- **次に確認:** 最大・最小特異値、回転・せん断、近等方伸長の解析fixtureを作り、`Fe`再構成とHencky projectionへの誤差伝播を検証する。build/test/runtimeは未実施。

### 2026-10-12 — Granular collider摩擦は接線へ分離しpolygon法線を外向きにする
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`resolveColliders`、`resolvePolygonCollider`)。
- **確認できた事実:** Circle/polygon colliderは反発処理後に速度ベクトル全体を`1-friction`倍しており、摩擦が法線反発にも作用していた。またpolygon内点から最寄り境界へのベクトルは内向きなので、そのままでは粒子をsolid collider外へ出す応答法線として逆向きになる。
- **変更:** Granularモデルでは法線外向き速度をrestitutionで応答させ、接線成分だけ摩擦減衰する。polygonの応答法線はGranular時に外向きへ反転した。既存CorotatedVonMises経路は元の式を維持する。
- **価値／懸念:** Circle/Polygon colliderで砂の貫通修正と反発を法線方向に分離し、接線摩擦を独立させる。polygon角・粒子中心が境界上にある場合の法線安定性、複数collider接触順序は未検証。
- **次に確認:** circle/polygonに対するinside/outsideからの粒子移動、法線／接線速度と反発係数の解析fixtureを追加し、弾性MPM既存fixtureに差がないことを確かめる。

### 2026-10-12 — Granular MPMのwall frictionは法線速度を変えず接線だけに効かせる
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`applyBoundaryConditions`)。
- **確認できた事実:** boundary条件は各壁で`boundaryFriction_`を法線速度にも適用していた。granular砂では壁から内側へ動く速度が一部の壁で増幅／減衰され、wall frictionの値が法線運動まで変えていた。
- **変更:** Granular Drucker–Prager時は壁へ向かう法線速度だけ0にし、壁から離れる法線速度は保持する。`boundaryFriction_`は接線成分にだけ適用する。既存CorotatedVonMises経路は従来式を維持する。
- **価値／懸念:** 床・側壁からの反発／粘着ではなく、非貫通と接線摩擦として扱える。4壁それぞれの inward/outward velocityと摩擦の符号確認、砂pileの安息角への影響は未検証。
- **次に確認:** 4壁の正負法線速度と接線速度を固定条件で検査し、既存CorotatedVonMisesの境界fixtureに差がないことを確認する。

### 2026-10-12 — Render Queue MPM worker cloneは並列描画開始前に生成される
- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` (`cloneCompositionSnapshot`、CPU MFR worker setup、GPU worker setup)、`ArtifactCore/src/Physics/PhysicsSystem.cppm`。
- **確認できた事実:** CPU MFR composition cloneとmulti-GPU/XPU fallback cloneはworker threadを起動する前のsetup loopで逐次生成される。clone時のMPM solver登録・初期checkpoint生成は、このsetup phaseで完了する。`ArtifactAbstractComposition::fromJson()`はLayer attach後に保存current frameへ`goToFrame()`し、Render Queueも各target frameで`goToFrame()`してGranular replayへ入る。worker実行時は各worker固有のComposition/runtime physics worldを使い、同一worldを複数workerへ共有しない。**追加変更:** solver registrationでworld clockとper-layer snapshot map nodeを先行作成し、worker snapshot captureは共有外側registryを`find()`するだけにした。1 Layerのdetachでworld共通cacheを消す場合も、残るsolverのlayer map nodeをdetach側で再作成する。worker advance/restoreでclock entry missing時にcontainerを構造変更せず、失敗を返す。
- **確認できた寿命:** CPU MFR clonesはRender Queue所有の`workerCompositions`に、GPU clonesはGPU worker ownerに保持され、teardownは全render workerを`join()`した後に進むため、通常dispatch中に終了workerがComposition destructorからworld registryを消す経路はない。
- **価値／懸念:** clone生成時のregistry構造変更と、worker終了時のworld teardownが並行する経路は静的には排除できた。workerは異なるworldごとのsnapshot mapとclockを同時更新するため、custom registryと異worldの並行アクセスの全スレッド安全性は静的追跡だけでは証明できない。
- **未検証:** UI側Compositionの同時solver登録／破棄やworker間のThreadSanitizer挙動は未確認。**次に確認:** 許可されたThreadSanitizer/runtime stressで複数MPM workerのcache capture/trimとUI編集並行を検証する。

### 2026-10-12 — MPM seekがbaseline復元できない場合に診断を残す
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`replayGranularMaterialToFrame`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`resetGranularMaterialToBaseline`)。
- **確認できた事実:** checkpointがない場合のbaseline replayは正常なcache miss経路だが、baseline snapshot自体が欠落・不整合ならreplayは`false`を返す。`goToFrame()`と`setFramePosition()`はこの戻り値を無視するため、砂状態が更新されずにframeだけ移動しても記録がなかった。
- **変更:** baseline復元失敗時だけworld IDとtarget frameを`[Composition][MPM]` warningへ出す。通常のcache missではログを出さない。
- **価値／懸念:** solver未登録・baseline破損を静止した砂と区別できる。利用者向けのUI error surfaceやrecover操作は未実装。
- **次に確認:** solver登録失敗とbaseline欠落を故意に作った開発fixtureで、通常cache missが無警告、復元失敗が一度のwarningになることを確認する。

### 2026-10-12 — MPM密度meshは標本中心を保持し外周cellで全域を覆う
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`writeDensityField`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** `writeDensityField()` は128×96個のpixel center標本を出力する。単純な127×95 quad化ではLayer外周の半pixelと最右・最下cellを描けず、中心標本から境界への4点平均は孤立した低密度peakのalphaをしきい値未満に落とし得る。
- **変更:** 130×98頂点の格子で129×97 cellを作り、各field標本を内部頂点として保持し、その外側へ半pixel幅のcellを追加した。外周値は最端標本をclampして使う。最大mesh容量は75,078頂点。
- **価値／懸念:** GPU surfaceがLayer全域を覆い、fieldの中心標本値を失わない。外周を複製する境界条件による輪郭品質、GPU/CPU parityは未検証。
- **次に確認:** field端・単一粒子・Layer全域の密度分布を固定fixtureで可視化し、render outputでmesh外周とCPU thumbnailの境界を比較する。

### 2026-10-12 — CPU Render QueueのMPM thumbnail経路
- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` (`renderLayerSurface`、`renderSingleFrameComposition`)、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`drawLayerForCompositionView`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** CPU Render Queueの `renderLayerSurface()` にMPM固有分岐はないが、Layerの `getThumbnail()` へfallbackする。後続実装で `ArtifactMpmSand2DLayer::getThumbnail()` がsolver密度fieldからbounded `ImageF32x4_RGBA` surfaceを作り、既存のQImage境界で返すため、CPU出力もMPM simulation像を含む。
- **残る未検証:** Render Queueの該当frame solver state、mask/effect適用、GPUとの画素・alpha parity、frameごとの変換コストはruntime未確認。
- **次に確認:** CPU/GPU Render Queueで同一scene/frameを比較し、mask/blend、透明edge、MFR cloneでの状態一致を検証する。

### 2026-10-12 — MPM密度描画は現在粒子体積を反映する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`writeDensityField`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** 密度場writerは各粒子の初期体積 `volume0` と固定支持半径だけを使っていたため、MPMの変形勾配 `F` による現在体積の変化がsurface kernelへ反映されなかった。
- **変更:** `volume0 * det(F)` を描画体積に使い、支持半径を `sqrt(det(F))` に比例させた。出力fieldの最小pixel幅を基準に半径を各軸8 pixel以内へclampし、既存のbounded field契約を保つ。
- **価値／懸念:** 圧縮・膨張で粒子面積が追従し、変形後も密度fieldがmaterial stateから作られる。見た目のしきい値、体積変化時の輪郭品質、field resolutionによる性能は未検証。
- **次に確認:** 許可されたruntime検証時に、単粒子の体積変化と圧縮pileでdensity maskの面積・重なりが期待どおりに変わることをcapture比較する。

### 2026-10-12 — MPM surface migration must remove stale particle-buffer references
- **関連:** `Artifact/include/Layer/ArtifactMpmSand2DLayer.ixx`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** Layer headerに `renderParticles_` は存在しない一方、property更新とJSON復元に古い `renderParticles_.reserve(...)` 呼び出しが残っていた。新しいsurface mesh経路の移行後に残った未定義メンバー参照だった。
- **変更:** 2箇所の未使用reserve呼び出しを削除し、同名参照が残っていないことを検索で確認した。
- **価値／懸念:** コンパイルを阻む古いParticle payload残骸を除去した。ビルドによる確認は未実施。
- **次に確認:** 次の許可済みbuildでMpmSand2D module pairのcompile/linkを確認する。

### 2026-10-12 — MPM surface buffer acquisition must preserve undersized recycled storage
- **関連:** `Artifact/include/Render/RenderCommandBuffer.ixx`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **確認できた事実:** surface buffer poolはsubmit/reset後のpacket vertex配列を再利用する。容量不足のbufferをacquire時に単純に破棄すると、将来要求が増えたときに確保済み容量を失う。容量が揃っている現行Layerでは通常発生しないが、取得API単体の失敗経路として存在した。
- **変更:** 取得できないbufferを同じbounded poolへ戻す処理を追加した。
- **価値／懸念:** 一時的な容量不一致で再確保を誘発せず、bounded reuse契約を維持する。差分は静的に確認したが、packet resetを含む実行時の再利用やallocator計測は未検証。
- **次に確認:** build/runtime確認を許可された段階で、複数surface packetのsubmit後に容量が保持されることとpool hit率を計測する。

### 2026-10-11 — Paired module implementation must not redeclare export module
- **関連:** `Artifact/CMakeLists.txt`、`Artifact/cmake/ArtifactSources.cmake`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** Artifact registers public `.ixx` providers in `APP_MODULES` and paired `.cppm` implementations in `APP_IMPL`. The MpmSand2D implementation redeclared `export module` and Ninja dyndep rejected it as an export provider outside a `CXX_MODULES` file set. Changing the implementation declaration to `module Artifact.Layer.MpmSand2D;` let dyndep proceed. The subsequent build stopped earlier in `ArtifactCore/src/Physics/MpmSolver2D.cppm:384` with MSVC C2397, before compiler verification of the MpmSand2D implementation.
- **仮説／未検証:** The implementation declaration now matches sibling paired `.cppm` files, but compile/link correctness remains unverified until the independent ArtifactCore narrowing error is resolved.
- **価値／懸念:** This separates module registration failure from the later Core compile failure; avoid changing the Core child repository without an explicit request.
- **次に確認:** After the ArtifactCore build blocker is resolved, rebuild `ArtifactUiTest` and confirm both the MpmSand2D module pair and Cloner fixture compile.

### 2026-10-11 — MPM砂は既存の弾性材質MPMとは構成則を分ける
- **関連:** `ArtifactCore/include/Physics/MpmSolver2D.ixx`、`ArtifactCore/src/Physics/MpmSolver2D.cppm`、`ArtifactCore/src/Physics/PhysicsSystem.cppm`、`Artifact/src/Layer/ArtifactSandSim2DLayer.cppm`。
- **確認できた事実:** 現行MPMはfixed-corotated応力と正規化von Mises型の塑性投影を持ち、presetはFlesh/Foam/HardRubber/Wood。既存SandSim2Dはセルオートマトンで、別Layerが描画・保存する。PhysicsSystemはLayerID単位のMPM solverとsnapshotを管理し、Shapeの材質シミュレーションから使われている。
- **仮説／未検証:** 既存MPMへ材料モデル選択を追加し、砂だけ圧力依存摩擦降伏・dilatancy・引張制限を使えば、solverの重複を抑えて粒状流動を追加できる可能性がある。Mohr–Coulomb系の2D応力条件と粒子体積更新を固定した実装は未検証。
- **価値／懸念:** 顆粒塑性を既存von Mises projectionのパラメータ調整だけで近似すると、安息角や圧密を制御できず「砂色の弾性体」になる懸念がある。GPU経路はCPU塑性tailを持つため、同モデルのGPU対応とperformanceは別途立証が必要。
- **次に確認:** `docs/planned/MILESTONE_MPM_GRANULAR_SAND_2026-10-11.md` に記録した式、単位、受入シナリオをレビューし、採用する顆粒降伏投影とLayer設定所有者を確定してから実装へ進む。

### 2026-10-11 — MPM砂モデルは弾性応力・塑性投影・GPU ABIを一体で設計
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm`、`ArtifactCore/include/Physics/MpmSolver2D.ixx`、`ArtifactCore/include/Graphics/MpmCompute.ixx`、`ArtifactCore/src/Graphics/MpmCompute.cppm`、`ArtifactCore/src/Physics/PhysicsSystem.cppm`。
- **確認できた事実:** CPU `firstPiolaKirchhoff()` はfixed-corotated応力、`applyPlasticity()` はvon Mises型の対数特異値投影。GPU particle ABIはF/Fp/C/塑性履歴を転送する。`MpmSnapshot2D` はparticle stateとaccumulator等を保存するが、材料係数・モデル選択・grid設定は含まない。
- **仮説／未検証:** 砂用Drucker–Pragerへ切り替えるなら、yield projectionだけでなく弾性応力と応力測度・2D応力条件を同時に揃える必要がある。Hencky弾性が候補だが、現在の2D Fp更新との組み合わせは未検証。
- **価値／懸念:** CPU用の砂モデル追加時にGPU solverが旧構成則のまま計算する誤ルートを防ぎ、project設定とper-frame snapshotの所有境界を分けられる。モデル判別漏れはGPU/CPU state divergenceを起こす。
- **次に確認:** 材料モデルごとの弾性応力・return mapping・particle historyを定義し、Layer JSON、MpmSnapshot2D、GPU upload/readback ABIのどこが各値を所有するか設計レビューする。

### 2026-10-11 — MPM砂のComposition表示はShape格子変形と分離する
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm` (`buildMpmParticleRenderData`)、`Artifact/src/Layer/ArtifactShapeLayer.cppm` (`drawMaterialGrid`)、`Artifact/src/Layer/ArtifactSandSim2DLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **確認できた事実:** MPM render-data bridgeは手動呼び出し向けでComposition自動描画に接続されていない。ShapeのMPM描画はparticleGridColumns/Rowsで隣接粒子をquad化する。SandSim2DLayerはdraw呼出しのたびCPU/GPU simulationをstepし、GPU側readbackを行う。
- **仮説／未検証:** 砂は任意大変形で初期格子順のquadが折り重なり、描画の隙間・セル反転を起こし得る。独立Sand Layerと粒子sprite描画なら、既存Particle Layerのemitter意味論やShape gridを使わずMPM点配置を表現できる。描画境界を専用render packetにするコストは未計測。
- **価値／懸念:** Simulation tickをLayer drawから切り離し、Composition playback/snapshot seek契約に接続できる。点spriteだけでは粒子間の空隙が目立つ場合、bounded density surface化が追加で必要。
- **次に確認:** Composition render収集におけるlayer raster sourceと直接primitive pathを調べ、MpmSandLayerの粒子drawをCPU/GPU/Render Queueで共有する最小APIを決める。bounds expansionとopacity/mask/exportの統合も確認する。

### 2026-10-11 — MPM playbackは接続済みだがRender Queue状態隔離は未確認
- **関連:** `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`setFramePosition`/`goToFrame`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **確認できた事実:** PlaybackEngineのforward renderはComposition `setFramePosition()` を呼び、内部1〜8 frame進行の既定経路で `advancePhysicsFrame()` からMPMを更新する。従来Shape用`materialSolvers_`はglobal registryである。一方、砂solver用registryにComposition instance固有runtime ID、worldごとのfixed-step accumulator、snapshot cache/capture/restoreを追加し、CompositionからそのIDを渡す経路が入った。JSON cloneも新しいComposition Implから別runtime IDを得る。MPM cache miss後のbaseline replayは未実装。Component Simulation bakeは別にexport/importされる。
- **仮説／未検証:** Render Queue cloneのlayer IDsがglobal PhysicsSystem registryへ同じsolverを引くのか、solverを再生成するのか、各workerのMPM状態をどう隔離するかは未特定。offline renderでMPMが正しい状態になるとは現証拠から判定できない。
- **価値／懸念:** 砂solverの複数Composition間step/snapshot共有はruntime world単位で避ける基盤が入った。ただし通常Shape用registryは従来のglobal scopeであり、砂専用Layer・clone lifecycle・seek miss時の再simulationは未接続。
- **次に確認:** 専用Layerのattach/detachでruntime world付きsolverを生成・破棄し、baseline snapshotとcollider/inputを用いるcache miss replayをComposition frame evaluationへ接続する。worker cloneごとの再現性と同時Composition playbackを検証する。

### 2026-10-11 — Layer JSON復元がPhysicsSystem MPMを仮IDへ登録する疑い
- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` (`fromJson`)、`Artifact/src/Layer/ArtifactAbstractLayerPersistence.cppm` (`fromJsonProperties`)、`Artifact/src/Layer/ArtifactLayerFactory.cppm` (`restoreSerializedId`)、`Artifact/src/Layer/ArtifactAbstractLayer.cppm` (`setId`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm`。
- **確認できた事実:** Shape `fromJson()` はfactoryへ返す前にbase `fromJsonProperties()` を呼ぶ。base復元は `materialPhysicsEnabled` がtrueなら `enableMaterialPhysics()` を呼び、layerの現IDでPhysicsSystem material solverを登録する。Layer factoryが返却直前にserialized IDを `setId()` するが、このsetterはPhysicsSystem registryを移行しない。`getMaterialSolver()` はLayerIDでregistryを検索する。
- **仮説／未検証:** MPM enabled Shapeの通常project loadやJSON clone後、solverが復元Layerのserialized IDではなくconstructor生成IDに登録され、solverを取得できないか孤児registry entryが残る可能性がある。Artifact runtimeでのproject load結果は未確認。
- **価値／懸念:** MPM砂のLayer/Render Queue integrationより前にLayer ID restoration contractが必要。MFR cloneは同じserialized IDを再利用するため、仮IDsolverが残るとsim state lost/stale registryを招く懸念がある。
- **次に確認:** Artifactサブモジュールの変更許可後、serialized IDを確定してからphysics registrationする順序へ直す。通常load、Render Queue clone、Undo/Redoでregistry keyとsolver lifetimeを検証する。

### 2026-10-11 — Debug executable の PDB 無効化フラグ
- **関連:** `Artifact/CMakeLists.txt` の Debug executable linker option。
- **確認できた事実:** `/PDB:NONE` は PDB 出力名 `NONE` と解釈され、build tree に約6 GBのファイルを作り、再リンクで `LNK1201` になった。MSVC の `/DEBUG:NONE` が PDB を生成しない指定であることを公式 linker docs で確認し、置換した。
- **仮説／未検証:** Visual Studio 2026 と全ての supported Debug configurations での再リンク確認は未実施。今回の `ArtifactUiTest` link は通過した。
- **価値／懸念:** Debug executable link が意図どおり PDB を省略し、`NONE` という巨大な出力ファイルを作らない。
- **次に確認:** clean Debug build 後に PDB が作られず、link 成功することを確認する。

### 2026-10-11 — Cloner GPU draw parity は通常 Viewer の raster source で成立

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tools/capture_ui_test.py`、`docs/planned/MILESTONE_CLONER_UNIFICATION_2026-08-23.md`。
- **確認できた事実:** Composition Viewer の raster fixture は、GPU有効時に3×3で9個のplacement-backed sprite drawを記録し、CPU fallback 0を報告した。D3D12上のCPU/GPU renderer readbackは `ja` / `en` / `zh` / `zh-TW` 全ロケールで完全一致し、各946×1085画像に39200個のfixture色画素があった。キャプチャスクリプトは `--cloner-backend cpu|gpu` と診断route assertionを提供する。
- **追加確認 (2026-10-11):** GPU適格streamは `drawWithGpuEligibleClonerGridIndexed` 内で候補ごとの `QMatrix4x4` を生成し、Composition View callback側の `gpuGridPlacementMatchesCpuInstance` が同じdescriptorから期待行列を再構成して全16要素を比較する。加えてsubmitter失敗用fallback行列も各clone packetに保存する。これは挙動の事実であり、比較を消す場合はcomposition fieldによる行列／weight変更とsubmit失敗時のCPU fallbackを保つ別の証明経路が必要。
- **追加確認 (2026-10-11):** `capture_ui_test.py` は `--cloner-rotation-step` を受けても fixture がその値を適用するのはRadialだけだった。誤った Linear + rotation-step 組合せが成功したように見えないよう、fixture起動前に Radial 以外の指定を拒否する検証を追加した。実行時のCPU fallback確認にはまだ非対応fixture条件の入力経路が必要。
- **追加確認 (2026-10-11):** Linear `opacityDecay=0.2` はGPU適格性から除外される既存fixture条件だったため、capture helperに `--cloner-expect-cpu-fallback` を追加して実行確認した。GPU準備済みだがplacement未処理・GPU draw 0件となり、`ja` / `en` / `zh` / `zh-TW` のCPU/GPU renderer readbackは同一frame 2で全画素一致、weight traceも一致した。arena overflow・GPU resource failureは別条件で未検証。
- **追加確認 (2026-10-11):** Radial `opacityDecay=1.0` を既存exeで試すと、CPU weight trace `1,1,0,0,0` に対してGPU stream traceが `1,1,0,-1,-2` となり、GPU 3 draw＋CPU fallback 2件へ分岐した。`drawWithGpuEligibleClonerGridIndexed` の共通candidate callbackでweightを `std::clamp(...,0,1)` し、CPU `appendCloneInstance` と意味論をそろえた。対象 `.ixx` module objectのコンパイルは通過したが、既存exeは修正前のためruntime再確認は未実施。
- **仮説／未検証:** GPU placementを読むdrawでもcloneごとのCPU順序走査とfallback用CPU transform生成を残すため、CPU準備時間が支配的な大規模Gridでは速度向上が限定的な可能性がある。GPU runtime性能、Vulkan runtime parity、容量上限／resource failure時の実機fallbackは未検証。
- **価値／懸念:** 描画段階は実画像を使ったCPU parityまで到達した。透明合成順を保持しながら複数cloneを1 drawへまとめられるかは、順序・blend semanticsを測定してから別段階で判断する。
- **次に確認:** GPU timeとCPU準備時間を分離計測し、固定容量上限近傍とGPU resource failureのfallback挙動を確認してからGPU-3の対象拡張を選ぶ。

### 2026-10-11 — Linear Cloner の平行移動配置をGPU経路へ拡張

- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/include/Render/ArtifactIRenderer.ixx`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** Linear generatorはclone index 0から `offsetXYZ * (index + 1)` で配置する。GPU placement recordに同じ順序のtranslationを生成し、HLSL vertex shaderがslotを読み取る経路をGrid/Matrixと共用した。対応対象は単一generator、transform stackなし、回転なし、不透明度減衰なし、Random legacy mode以外。D3D12上のCPU/GPU出力は4ロケールで全画素一致し、Linear fixture色17,396画素とGPU draw 4件、fallback 0件を確認した。Gridも同じ再ビルド後、4ロケール全画素一致・fixture色39,200画素・GPU draw 9件を再確認した。
- **仮説／未検証:** singleton drawを保つため透明合成順とCPU fallbackを維持できるが、大きいcountでCPU準備時間が支配的かは未測定。回転／減衰／Random modeのruntime fallback、容量超過・resource failure fallback、Vulkan runtime parityは未確認。
- **価値／懸念:** LinearとGrid/Matrixが同じGPU output arena、record ABI、sprite vertex pathを利用する段階になった。rows/depthのゼロ組合せをLinear modeとしてHLSLへ伝えるため、そのsentinel契約を別distribution追加時に曖昧にしないこと。
- **次に確認:** rows/depth sentinelを明示的なdistribution enumへ置き換える設計を検討し、Linearの不対応設定とarena容量超過をruntime fixtureでCPU parity確認してからRadialを追加する。

### 2026-10-11 — Radial Cloner GPU placement は回転とローカル矩形原点を保持
- **関連:** `Artifact/src/Layer/ArtifactCloneLayer.cppm` のRadial評価、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx` のgenerator列挙、`Artifact/include/Render/ArtifactIRenderer.ixx` のplacement record、`Artifact/src/Render/ShaderManager.cppm` のCloner sprite vertex shader。
- **確認できた事実:** `layerGenerators()` component Radialの配置は原点周りに半径分移動し、cloneを `angle` 度回転する。GPU placement recordの予約領域をrotation degreesに使い、HLSL sprite pathへlocal rect originを回転順序に含めた。旧式Cloner内部のradialCount/radius/anglesが正規generator descriptorへ反映されていなかったため、descriptor変換時にcanonical内部設定をコピーした。80×48矩形・4 clone fixtureでD3D12の `ja` / `en` / `zh` / `zh-TW` 全てCPU/GPU完全一致（946×1085、fixture色16,599画素）、GPU draw 5件、fallback 0件。GridとLinearもjaで完全一致。旧式ClonerGenerator effectのbounds-center契約は同経路とは別である。
- **価値／懸念:** RadialをCPU fallbackからGPUへ段階的に広げ、設定値もdescriptorへ正しく渡るようになった。Radial非ゼロrotationStepもD3D12 parityを確認済み。opacityDecay／modifier、Vulkan runtime、性能は未検証。
- **次に確認:** opacity decay／modifier等はそれぞれCPU parity fixtureを追加してからGPU対象を選び、Vulkan runtime parityも確認する。

### 2026-10-11 — Grid/Matrix Cloner 減衰weightをclone indexと共有
- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、Cloner UI fixture。
- **確認できた事実:** 旧Cloner Gridはflattened index `z*(rows*columns)+y*columns+x` を `1-opacityDecay*index` に適用していたが、canonical component generatorはweightを常に1としていた。Grid/Matrix descriptor CPU instance生成とGPU stream callbackに同じ0..1 clamp式を加え、GPU照合側でもweight式を確認するようにした。D3D12 Grid decay=0.2は `en` / `ja` / `zh` / `zh-TW` 全てCPU/GPU readback全画素一致（946×1085、GPU 9 draw／fallback 0）。trace列は `1,1,0.8,0.6,0.4,0,0,0,0`。enのdecay=0比で26,293画素が変化した。
- **確認できた事実:** Matrix専用descriptor fixtureはdecay=0.2で `ja` / `en` / `zh` / `zh-TW` 全て全画素一致、GPU 9 draw／fallback 0。D3D12とVulkanの両方でMatrix CPU/GPU readbackがframe 2、全画素一致した。さらにVulkan Grid decay=0.2、Radial rotationStep=15／decay=0.2も同じ4ロケールでframe 2のCPU/GPU readbackが全画素一致し、GridはGPU 9 draw、Radialは5 draw、fallback 0。Matrix weight traceは `1,1,0.8,0.6,0.4,0,0,0,0`。decay=0と比べCPU/GPU双方で26,293画素変化し、decay=0同士も全画素一致した。decay=1の英語fixtureはweight `1,1,0,0,0,0,0,0,0` でCPU/GPU全画素一致した。
- **仮説／未検証:** arena容量近傍、性能は未検証。
- **価値／懸念:** legacy Grid weight意味論を正規generator CPU/GPU描画へそろえ、opacity shader修正を使ってGridも段階拡張した。
- **次に確認:** 出力arena容量近傍と性能を確認する。

### 2026-10-11 — Cloner GPU arenaの上限はbase slot込み
- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm` (`dispatchClonerGridPlacement`)、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`（GPU placement observer／sprite fallback）。
- **確認できた事実:** arenaは65,536 recordsで、各dispatchは候補数+1（base slot）を予約する。batch cursorが0の場合に受け入れ可能な候補数は最大65,535。残容量不足はparameter mapping／compute dispatch前にfalseで返り、range未取得時は通常のCPU行列sprite drawへ進む。cursorはsubmitter batch送信後に0へ戻る。
- **仮説／未検証:** overflow時のCPU fallbackはコード経路上boundedだが、65,536候補を実際に超えるfixtureでfallback count・画像 parity・フレーム時間を確認していない。
- **価値／懸念:** 65,536を「clone数上限」と誤読せず、base recordとbatch内の複数dispatchが共有するcursorを含めて上限を説明できる。大量clone fallbackはCPU transform loopとsprite recordingが続くため、GPU上限拒否だけでは入力 workload を安価にできない。
- **次に確認:** 安全なoverflow fixtureで1 batch超過時のCPU fallbackと画像を検証し、同一clone数でCPU準備時間／GPU時間を計測する。

### 2026-10-11 — transformed-sprite vertex shader がCPU opacityを落としていた
- **関連:** `Artifact/src/Render/ShaderManager.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`、Cloner UI fixture。
- **確認できた事実:** fixture CPU/GPU両経路はclone weight `1.000, 1.000, 0.800, 0.600, 0.400` を作りdraw callbackまで渡していた。通常 transformed-sprite VSは入力頂点カラーattributeを宣言せず、出力を常に白・alpha=1に固定していた。GPU Cloner VSはOpacity cbufferを使うため減衰し、CPU標準sprite経路だけが減衰を失っていた。Artifact側の標準VSを頂点カラー入力へ対応させ、入力layoutへcolor attributeを追加し、Cloner専用PSOは旧2 attribute layoutを明示した。変更後、D3D12 radial `rotationStep=15` でdecay=0.2と0の両CPU/GPU readbackが全画素一致し、decay画像間では11,609画素が変化。GPUは各5 draw／fallback 0。
- **仮説／未検証:** SpriteTransformを使う他の描画操作もopacityが正常化するはずだが、専用UI / matte / Puppet deformationの全変種は未検証。
- **価値／懸念:** GPU Cloner固有のopacity差ではなく、CPU fallback共有vertex shaderの頂点color欠落を修正した。Diligent backend-neutral shader/PSO境界内で、ArtifactCore child repoは変更していない。
- **次に確認:** transformed-spriteの通常opacity、mask、textured triangleで回帰を確認し、Radial decayのlocale / Vulkan runtime coverageを広げる。

### 2026-10-11 — GPU Radial rotationStep parity
- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** CPU Radialの配置角は `startAngle + angleStep * index`、描画回転は `placementAngle + rotationStep * index`。GPU placement computeへstepを追加した後、GPU向けCPU stream生成側が回転増分を変換行列に加えていなかったため、matrix照合がclone 1–3をCPU fallbackへ送っていた。GPU stream変換もCPU Component生成と一致させると、D3D12 fixtureでRadial `rotationStep=15` が5 GPU draw／0 fallbackとなり、CPU/GPU readbackが946×1085の全画素で一致した。Grid（9 draw）、Linear（4 draw）、Radial step=0（5 draw）も同buildで全画素一致。Radial step=15、Grid、Linearは `ja` / `en` / `zh` / `zh-TW` すべてで比較した。初回の15度stepキャプチャはframe 1の読み戻しで黒だったが、frame 2で再取得したCPU/GPU画像にはクローンが描画され完全一致した。
- **価値／懸念:** radial offset angle と sprite rotation を別々に運び、GPU配列生成とCPU照合を同じ意味論にそろえた。parameter blockは固定長64 bytesに拡張し、readbackなしのdispatchを保つ。Vulkan runtime、非常に大きいstep値の数値境界、性能は未検証。
- **次に確認:** Vulkan backendでもRadial step parityを確認し、opacity decayやmodifierは個別のCPU/GPU parity fixtureを作ってから段階的に追加する。

### 2026-10-11 — Cloner fallback診断はCPU描画完了後に確定する
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tools/capture_ui_test.py`、Cloner GPU-2 fixture。
- **確認できた事実:** GPU placement eligibilityがfalseの場合、通常の `drawWithClonerEffect()` がcloneをCPU描画するが、先行して出力するroute診断の `cpuFallbackDraws` はGPU callback内の失敗だけを数えていた。そのため全体CPU fallbackを0件と記録し得た。通常CPU描画callbackで件数を加算し、描画後にroute診断を最終値で保存するよう変更した。対象controller objectはビルド済み。
- **確認できた事実:** 既存の古いexeでfallback fixtureを走らせると `cpuFallbackDraws=0` が出て、新しいassertionはこれを不合格にした。同frame 2の946×1085 renderer readbackはCPU backend画像と全画素一致し、weight traceは `1.000,1.000,0.800,0.600` だった。これはfallback描画の既存動作確認であり、新しいcounter修正自体のruntime確認ではない。
- **仮説／未検証:** 更新済みexeではweight traceと同数の4 callbackを数える見込みだが、MPMソースを変更せずリンク済みexeを更新する経路は未確認。
- **価値／懸念:** GPU placement未対応・不適格時のCPU経路をGPU部分fallbackと区別して観測できる。diagnostic有効時のファイル書込はfixture専用で、通常frameには適用されない。
- **次に確認:** 更新済み `ArtifactUiTest.exe` でRandom legacy fallback fixtureを実行し、GPU draw 0、CPU fallback draw >0、weight traceとreadback parityを確認する。現在の実行ファイルは古い。

### 2026-10-11 — Linear Cloner rotationとopacity decayは既存placement shaderで表現できる
- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`、Cloner UI fixture。
- **確認できた事実:** CPU Linear generatorは各cloneへ `translate(offset * (index + 1))` の後に `rotate(rotationStep * (index + 1))` を適用する。GPU placement recordは回転角を保持でき、shared vertex shaderがlocal verticesを回転してoffsetへ足す。Linearのopacity decayはGPU placementから返すclone indexに基づき、既存 per-draw opacityへ `clamp(1 - opacityDecay * index, 0, 1)` を渡す。GPU eligibility、CPU transform/weight照合、range rotation-aware判定、fixture入力とweight assertionを拡張した。既存exeのCPU fixtureではLinear count 3／decay 0.2のtrace `1.000,1.000,0.800,0.600` とdecay 1.0のtrace `1.000,1.000,0.000,0.000` がassertionを通った。静的再読でLinear branchのweight変数宣言漏れとRadial branchの重複宣言を検出し修正した。
- **2026-10-12 の確認:** 現行 `ArtifactIRenderer.cppm` から配置compute HLSLを再抽出し、Vulkan SDK `glslangValidator -D -V -S comp -e main` と `spirv-val` が両方成功した。既存 `.modmap` を用いた対象5オブジェクト（Cloner support、Composition drawing、IRenderer、AppMain、CompositionRenderController）のMSVC単独コンパイルも成功した。既存 `ArtifactUiTest.exe` の Linear CPU fixture は回転step 15／decay 0.2で `ja` / `en` / `zh` / `zh-TW` 全てcapture成功し、946×1085 readbackとweight trace `1.000,1.000,0.800,0.600` を得た。GPU fixtureはexeが `gpuReady=1` に対して `placementHandled=0, gpuDraws=0` を返してcapture assertionに失敗した。exeは2026-10-11 22:50のままなので、この失敗は最新GPU C++統合を実行した結果ではなく、最新GPU parityも検証していない。最新HLSLのDXIL compileも未実施。
- **既存GPU基礎経路の対照確認:** 同じ古いexeのGrid fixtureは D3D12 で `gpuReady=1, placementHandled=1, gpuDraws=9, cpuFallbackDraws=0`。英語CPU/GPU renderer readback（946×1085）の比較は差分画素0だった。よって前項のLinear GPU fixture失敗はGPU Grid/Matrixの既存基礎経路が全般的に動かないことを示すものではなく、exeに含まれていないLinear拡張について結論できない。
- **Radial再確認:** 同じexeで英語Radial fixture（rotationStep 15／opacityDecay 0.2）をCPU/GPU両方実行。GPU routeは5 draw／CPU fallback 0、両renderer readback 946×1085、差分画素0、両方のweight traceは `1.000,1.000,0.800,0.600,0.400`。
- **検証バイナリ更新の制約:** Ninjaの既存link graphを読むとArtifactUiTestは未生成の `ArtifactMpmSand2DLayer.cppm.obj` を入力に含み、現在は別名exeへの再linkも既存objectだけでは完了しない。Cloner関連5 objectは直接compile済み。MPMソースに触れない指定を保つ限り、Linear GPUの最新runtime parityは既存exeだけでは検証できない。
- **2026-10-12 placement compute 実機試験:** 現行HLSLを `glslangValidator` でSPIR-V化し、Vulkan descriptor bindingのみ明示した一時shaderと外部一時C++ harnessをNVIDIA RTX 4070 Tiでdispatchした。base recordを含むLinear 3候補、Radial 4候補、Grid 6候補がCPU参照式と一致し、nonzero outputOffset (2/7/15) でもrecord・rotation・slot順が一致した。readbackは検証harnessだけで行い、Artifact hot pathにはreadbackを追加していない。これは配置computeの実機検証で、Diligent統合・D3D12・sprite描画・Linear全画面 parityの証拠ではない。
- **2026-10-12 描画shader compile:** 現行 `ShaderManager.cppm` のCloner sprite vertex HLSLを抽出してVulkan SPIR-V compileと `spirv-val` に通した。一時検証用binding整理後、TransformCB / ClonerSpriteCB / placement StructuredBufferはbinding 0/1/2に分離され、32-byte placement ABIとshader interfaceの構文検証が成立した。Diligent runtime PSO/vertex layoutの実行時再生成確認ではない。
- **2026-10-12 DXIL compile:** 配置 compute と現行Cloner sprite vertex shaderの未加工HLSLを、アプリ配下の `dxcompiler.dll` に含まれるDXC APIでそれぞれ `cs_6_0` / `vs_6_0` compileした。両方DXIL object生成に成功（4,840 / 6,252 bytes）。実GPU D3D12 dispatch・PSO creation・drawの証拠ではない。
- **2026-10-12 D3D12 placement 実機試験:** DXC生成の現行compute DXILをNVIDIA RTX 4070 TiのD3D12 compute queueでdispatchし、CPU式と照合した。Linear 3、Radial 4、Grid 6に加えて、count=1 Radialとidentity Gridを検証。5ケース全てbase slot、record index、offset、rotationが一致し、nonzero outputOffset (2/7/15/22/25) も合格した。これは配置compute shader自体のD3D12実行証拠で、Diligent統合やsprite rasterizationのend-to-end parityではない。
- **2026-10-12 Linear placement-to-draw 実機試験:** 配置compute DXILをD3D12 command listでdispatchし、UAV barrierとUAV→NON_PIXEL_SHADER_RESOURCE transition後、同じbufferを現行Cloner VSのStructuredBufferとして読み、80×48 unit quadのbase+3 clonesを描画した。nonzero `outputOffset=5` のbase/clone slotsを同じ `FirstPlacement` で選択し、rotation 0/15/30/45度、offset 150/120 pixelずつ、実weight `1.0/1.0/0.8/0.6` を検証。4 quadの出力bbox・pixel数・alphaがCPU参照期待値内でPASS（alpha 255/255/204/153）。CPU reference bitmapとの差は1画素で、30度cloneの下端付近のraster boundaryに限られた。テストPSはVS Colorを返す最小PSのためtexture samplingとDiligent packet自体は未検証だが、配置compute出力からvertex shader描画まで同一GPU command listで検証した。
- **仮説／未検証:** 既存Radial rotationとGrid/Matrix opacity parityに準じる設計。修正後のC++ module compileとLinear回転・decay併用時のCPU/GPU画像一致は未確認。
- **価値／懸念:** shader/packetの新規ABIを増やさずLinear rotationとweightを拡張できる。GPU enabled時のruntime parityを確認するまでは対応完了と扱わない。
- **次に確認:** Linear `rotationStep=15` と `opacityDecay=0.2` を併用し、CPU版とD3D12/Vulkan renderer readbackを `ja` / `en` / `zh` / `zh-TW` で比較する。Random legacy modeをfallback fixtureに使う。

### 2026-10-11 — TrackerManager の project load 置換と Controller binding

- **関連:** `Artifact/src/Project/ArtifactProjectImporter.cppm`（TrackerManager::fromJson / clearTrackers）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（controller session binding）。
- **確認できた事実:** project importer は非同期ロード時にバックグラウンド worker で実行され、従来はその中から `TrackerManager::fromJson()` によりグローバル Manager を置換していた。トラッカー配列を `ArtifactProjectImporterResult` に保持させ、ProjectManager の同期成功後／非同期 UI-thread commit 時に Manager を復元するよう変更した。さらにフレーム収集開始前に元 tracker を job ごとの `MotionTracker` copy へ複製し、フレーム収集と solve は copy 上で進める。成功時、同じ ID／同じ登録 pointer が残る場合だけ結果を元へ反映し、失敗・キャンセル・切替時は copy を破棄する。
- **仮説／未検証:** TrackerManager の置換をまたぐ解析では古い job の結果を破棄する設計だが、実機で project load と解析完了が競合した際の UI・undo・進捗表示は未確認。MotionTracker copy の frame buffer と解析結果の独立性も runtime 未検証。
- **価値／懸念:** importer worker とグローバル TrackerManager の同時更新を避け、Manager 置換後も solve worker が削除済み tracker pointer を使わない。キャンセル時に既存結果・設定を解析準備のクリア処理で失わない。1解析jobにつき開始時に tracker 状態を複製するコールドパス確保が発生する。
- **次に確認:** 許可後に非同期 project load と長時間解析を重ね、旧 job 結果が破棄され、新プロジェクト session／UI が維持されることを確認する。

### 2026-10-11 — Tracker source/session の project 保存境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（source selection）、`Artifact/src/Project/ArtifactProject.cppm`（extension data）、`Artifact/src/Project/ArtifactProjectExporter.cppm`／`ArtifactProjectImporter.cppm`（保存復元）。
- **確認できた事実:** Tracker ドックは追跡元と結果適用先を分離し、composition default source と tracker session 別 source override を project `extension_data` に保存する。session manifest は composition ID で索引し、active tracker ID と tracker ごとの編集 geometry も保存する。解析開始時には入力元を固定する。Controller の Composition 切替／破棄では Manager session を削除せず detach し、再選択時に bind する。
- **確認できた事実:** session create/delete 用の Undo command を追加し、tracker JSON、同じ tracker ID、geometry と source override を保持して復元する。既存 `UndoManagerChangedEvent` 経由で active manifest と controller binding を再同期する。
- **確認できた事実:** `LayerChangedEvent::Removed` は layer undo command の redo でも発行される。source ID をその場で消すと、layer undo で同じ ID の layer を戻しても source 設定が復元されず Undo/Redo の意味を損なうため、イベント時の即時 cleanup は採用しない。source 解決は現在の Composition 内で layer ID の存在を検証し、stale ID は使用せず選択 layer へ fallback する。
- **仮説／未検証:** source metadata の整理は `RemoveLayerCommand` のスナップショットと同じ undoable operation に統合する必要がある。保存後の Manager session 復元、複数 Composition 間での session ID 分離、Undo/Redo の実機動作も未検証。旧データの Manager 内 orphan tracker は安全に所有 Composition を推定できない。
- **価値／懸念:** Core／サブモジュールを変更せず既存 project extension lifecycle を使って複数 session の UI 選択と geometry/source の保存を実現できる。manager session の寿命と project serialization の接続が期待どおりかは受入れ確認が必要。
- **次に確認:** tracker source metadata の remove/restore を `RemoveLayerCommand` の Undo/Redo にどう含めるか設計し、許可後に保存／再読込、session 切替、layer Undo/Redo、tracker create/delete Undo/Redo、複数 Composition 切替を実行確認する。


### 2026-10-11 — Tracker の作業形状とセッション設定の保存境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（TrackerBoxState 初期化とキャプチャ）、`Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm`（viewport geometry）、`ArtifactCore/src/Tracking/MotionTracker.cppm`（settings / track regions）。
- **確認できた事実:** Feature/Search の幅と高さは `LayeredConfigStore` の `Viewport/TrackPoint/*` キーから読み書きされ、作業環境単位の設定になっている。Tracker の Core session には Point の位置・検索領域、Planar の track points / region と解析結果が保存される。ビューポート上の `TrackerBoxState.innerCenter` 自体はその Core session JSON に含まれず、各解析開始時に Gizmo state から Core へ点／領域を作っている。
- **仮説／未検証:** プロジェクトを切り替えた後も workspace の特徴領域サイズは共有される一方、中心座標や編集中の形状は session と一緒に復元されないため、保存済み結果と画面上の編集形状が異なる可能性がある。実際の再読込後レイアウトは未検証。
- **価値／懸念:** session chooser や再開 UI を足す前に、解析済みトラックと編集可能な source geometry のどちらを project/session が所有するか決める必要がある。
- **次に確認:** source layer／composition 単位の geometry 保存、workspace default との優先順位、プロジェクト再読込時の復元先を設計し、複数 session と source 切替で検証する。

### 2026-10-11 — 実写トラスの面追跡は背景除外と幾何誤差の確認が必要

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm:1002`、`ArtifactCore/include/Tracking/MotionTracker.ixx:151`、`tests/ArtifactCore/MotionTrackerVideoInputTest.cpp`。
- **確認できた事実:** planar solveのECC／特徴点検出マスクは矩形ROI全体を有効にする。公開入力は`addTrackRegion(QRectF)`であり、任意の背景除外マスクを渡すAPIは見つからない。CC0橋映像の選択領域にはトラスの穴越しの川と奥側の梁が入る。正解座標のある急角度の独立テストでは、24フレームの貼り付け四隅の最大誤差2px以下という検査を通過した（2026-10-11）。実写の信頼度0.5以上という検査だけでは幾何精度の証明にならない。
- **仮説／未検証:** 実写終盤のズレに、別奥行きの背景や梁の相対運動と、投影ROIを軸平行bounding boxへ戻す処理が寄与している可能性がある。急角度自体の一般的な失敗や、背景除外での改善は未証明。
- **価値／懸念:** 相関の信頼度が高くても、貼り付け面以外の画素に追従すれば合成の位置がずれる。背景除外マスクの保持と実写の独立した特徴点注釈が精度検証に必要。
- **次に確認:** ArtifactCore変更の明示許可後、参照フレームの除外マスクをホモグラフィで伝搬する最小設計と、実写終盤の注釈点に対する再投影誤差を検証する。

### 2026-10-11 — ArtifactHashMap iteratorの衝突chain走査疑い

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** iteratorは現在Nodeとbucket位置を保持する。`operator++()`はbucket位置を先に1つ進め、現在nodeの`next`を参照していない。hash衝突した複数entryが同一bucketに入る構造は`emplaceImpl()`で確認した。また`emplaceImpl()`は既存keyを見つけたときmapped valueを代入する一方、既存の`TryEmplaceDuplicateKeepsOriginalMappedValue`は元値維持を契約している。
- **仮説／未検証:** 同一bucketの連鎖要素をrange iterationで飛ばし、重複tryEmplaceで値を置換する可能性がある。runtime testはAGENTS.mdの実行制約により未実施。
- **価値／懸念:** range traversalでmap entryを取りこぼしたり、insert-only APIが既存valueを書き換えたりすると、反復ベースのシリアライズ／集計や呼び出し側の状態前提が壊れる恐れがある。
- **次に確認:** `ArtifactHashMapContractTest.IterationVisitsEveryEntryInCollisionChain` とmodel testを実行し、再現するならiterator increment／duplicate emplacementの契約を修正・検証する。

### 2026-10-11 — SmallVector debug snapshotのsamples型不一致の疑い

- **関連:** `ArtifactCore/include/Container/SmallVector.ixx`、`ArtifactCore/include/Container/ContainerDebug.ixx`、`ArtifactCore/include/Container/NamedVector.ixx`。
- **確認できた事実:** `SmallVector::debugSnapshot()` は `debugSample(4)` の戻り値 `NamedVector<ContainerElementSample>` を、`ContainerDebugSnapshot::samples` の `std::vector<ContainerElementSample>` aggregate fieldへ直接渡している。`NamedVector` には `toStdVector()` はあるが、確認した宣言範囲では暗黙の `std::vector` 変換演算子は見つからない。
- **仮説／未検証:** debugSnapshot／watch系のtemplate memberをinstantiateすると型変換エラーになる可能性がある。ビルドは禁止中のためコンパイラでは未確認。
- **価値／懸念:** 独立suiteが当該moduleのtemplate実体化で阻害される可能性があり、debug snapshot APIの利用可能性にも影響する。
- **次に確認:** ArtifactCore変更が許可された作業で、`samples.toStdVector()` 相当の明示変換が正しい境界か確認し、module compileとdebugSnapshot利用テストを実行する。

### 2026-10-11 — CppLinq lazy views borrow the input range

- **関連:** `ArtifactCore/include/Utils/CppLinq.ixx`、`tests/ArtifactCore/CppLinqContractTest.cpp`。
- **確認できた事実:** `Where` と `Select` は `const R&` を受け取り、入力rangeへ `views::filter`／`views::transform` をpipeしたviewを返す。既存suiteは元rangeの変更反映と遅延評価を検証している。
- **仮説／未検証:** 一時rangeを渡すと関数内では名前付きconst lvalueとしてview化されるため、呼び出し式の終わりでrangeが破棄された後のview走査がdanglingになる可能性が高い。現在の呼び出し元が一時rangeを使うかは未確認。
- **価値／懸念:** borrowed view契約を明示するか、所有viewと借用viewを分ければ、一時range由来の寿命事故を防ぎやすい。
- **次に確認:** 本番呼び出し箇所を検索し、一時rangeを渡す箇所の有無と許容するownership semanticsを確認してからAPI設計と寿命テストを決める。

### 2026-10-11 — Crash report parser の認識条件が header substring のみ

- **関連:** `ArtifactCore/include/Diagnostics/CoreCrashReportParser.ixx`、`tests/ArtifactCore/CrashReportParserStandaloneTest.cpp`。
- **確認できた事実:** `parseCrashReport()` は `=== Artifact Crash Report ===` が入力内のどこかにあれば `parsed=true` を返し、`parseCrashReportFile()` はそのフラグだけで成功扱いにする。例外情報やstack sectionがないheader単独の入力も成功する。
- **仮説／未検証:** report producer が出力する正式形式で必須sectionが定義されている場合、途中で切れた／壊れたreportを成功として空に近いsnapshotへ変換する可能性がある。headerのみを有効とする仕様かは未検証。
- **価値／懸念:** 最低限の構造検証を行えば、壊れたcrash reportと正常な最小reportを区別しやすい。
- **次に確認:** crash report producerの出力形式と既存保存データを照合し、成功判定に必要なsectionを決めたうえで、header-only／欠落sectionの契約テストを確定する。

### 2026-10-11 — NoiseGenerator legacy seed state is process-global

- **関連:** `ArtifactCore/src/Math/NoiseGenerator.cppm`、`ArtifactCore/include/Math/NoiseGenerator.ixx`、`tests/ArtifactCore/NoiseGeneratorStandaloneTest.cpp`。
- **確認できた事実:** Legacy `NoiseGenerator::setSeed()` rewrites shared static permutation array `p` and `isInitialized`; `perlin()` may initialize this state lazily. `NoiseField` instead stores its permutation table per instance and is documented as concurrently evaluable.
- **仮説／未検証:** Concurrent legacy noise evaluation and reseeding may race. Actual callers and whether they reseed during rendering are not yet checked.
- **価値／懸念:** Per-instance `NoiseField` can isolate deterministic procedural fields from process-wide seed changes.
- **次に確認:** Search production call sites for concurrent `perlin()` evaluation and runtime `setSeed()` use; if needed, add an explicit concurrency contract and migrate only approved call paths.

### 2026-10-11 — LayerBoundsCalculator2D の退化矩形 hit test 境界

- **関連:** `ArtifactCore/src/Geometry/LayerBoundsCalculator.cppm`、`tests/ArtifactCore/LayerBoundsCalculatorStandaloneTest.cpp`。
- **確認できた事実:** `contains()` は四辺の外積符号を判定し、全外積が0の場合も内包として返す。幅・高さがともに0の矩形では、任意の問い合わせ点への各外積が0になる。
- **仮説／未検証:** 退化矩形が hit-test 対象になると、本来の一点以外も含む判定になり得る。UI側が0サイズ矩形を問い合わせる頻度と期待仕様は未検証。
- **価値／懸念:** 0サイズまたは極小レイヤーの選択領域が広がる可能性がある。
- **次に確認:** zero-width／zero-height／zero-area各ケースの境界意味論を決め、必要なら `contains()` の退化形状処理と独立テストを追加する。

### 2026-10-11 — FormantExtractorは末尾の不完全解析windowを出力しない

- **関連:** `ArtifactCore/src/Audio/FormantExtractor.cppm`、`tests/ArtifactCore/FormantExtractorStandaloneTest.cpp`。
- **確認できた事実:** `analyzeTrack()`は`framesPerAnalysis <= totalFrames - offset`の間だけイベントを生成するため、最後にwindow長未満のsamplesが残る場合は解析しない。
- **仮説／未検証:** frame-rateと音声長が割り切れない素材では最後の短区間がmouth-shapeイベントに反映されない可能性がある。切り捨てが意図された仕様かは未検証。
- **価値／懸念:** 音声末尾の短い発音・閉口イベントがtrack出力から欠落する可能性がある。
- **次に確認:** tail paddingまたは短windowの分析を仕様化し、末尾にイベントを生成するかどうかの期待値を決める。

### 2026-10-11 — AudioVolumeのdeserializeが入力を読まない

- **関連:** `ArtifactCore/include/Audio/AudioVolume.ixx`、`ArtifactCore/src/Audio/AudioVolume.cppm`、`tests/ArtifactCore/AudioVolumeStandaloneTest.cpp`。
- **確認できた事実:** `serialize()`はvolume/mutedをJSON文字列へ出力する一方、`deserialize(const UniString&)`の実装は引数を無視し、既定volume 1.0を返す。
- **仮説／未検証:** 保存したvolume値を読み戻す経路ではレベル設定が失われる可能性がある。利用箇所は未確認。
- **価値／懸念:** API名から期待される保存復元が実現していない。
- **次に確認:** 呼び出し元とデータ互換性を調査し、serialize/deserializeのJSON形式と不正入力処理を定義した後、独立roundtripケースを追加する。

### 2026-10-11 — AudioDecibelsのdeserializeが入力を読まない

- **関連:** `ArtifactCore/include/Audio/AudioDecibels.ixx`、`ArtifactCore/src/Audio/AudioDecibels.cppm`、`tests/ArtifactCore/AudioDecibelsStandaloneTest.cpp`。
- **確認できた事実:** `serialize()`は`{"dB":...}`形式のJSON文字列を作る一方、`deserialize(const UniString&)`の実装は引数を無視して0 dBを返す。
- **仮説／未検証:** 保存した非ゼロ値を読み戻す経路で設定が失われる可能性がある。呼び出し元の利用状況は未検証。
- **価値／懸念:** `AudioDecibels` の永続化／複製でレベル値を復元できない。
- **次に確認:** 利用箇所と既存データ互換性を確認し、修正許可範囲でserialize/deserialize roundtripおよび不正JSON入力の契約を独立suiteへ加える。

### 2026-10-11 — AudioWaveformのbinningが端数frameを捨てる可能性

- **関連:** `ArtifactCore/src/Audio/AudioWaveform.cppm`、`ArtifactCore/include/Audio/AudioWaveform.ixx`、`tests/ArtifactCore/AudioWaveformStandaloneTest.cpp`。
- **確認できた事実:** bin幅は整数除算`frames / resolution`で決まり、全binを同じstep幅で処理する。
- **仮説／未検証:** `frames`が`resolution`で割り切れない場合、末尾frameがどのbinにも入らない可能性がある。現suiteは割り切れる寸法を使っており、この境界を実行確認していない。
- **価値／懸念:** preview waveformの末尾にあるtransientが欠落する可能性がある。
- **次に確認:** 端数のあるframe数と最終sampleだけに信号を置く独立caseで、全frameがbinningされるか確認する。

### 2026-10-11 — AudioSpectrumのDFT binはcosine投影の絶対値合計

- **関連:** `ArtifactCore/src/Audio/AudioSpectrum.cppm`、`tests/ArtifactCore/AudioSpectrumStandaloneTest.cpp`。
- **確認できた事実:** `computeFFT()`は各binで`abs(input[i] * cos(angle))`を加算しており、複素DFTのreal/imaginary成分やmagnitude normは計算しない。
- **仮説／未検証:** spectrum binsは正弦波の周波数振幅を一般的なスペクトラム解析の期待どおりには表さない可能性がある。どの表示側がこの結果を利用するか、また現状が意図的な簡易指標かは未検証。
- **価値／懸念:** このAPIをスペクトラム表示や周波数比較へ使う場合、bin値から正しい周波数強度を判断できない可能性がある。
- **次に確認:** 利用側の正規化・表示契約を調査し、DC・単一正弦波・Nyquist近傍の期待スペクトルを定めてから独立信号ケースを追加する。

### 2026-10-11 — AudioBassTrebleの係数atomicが未使用

- **関連:** `ArtifactCore/include/Audio/AudioBassTreble.ixx`、`ArtifactCore/src/Audio/AudioBassTreble.cppm`、`tests/ArtifactCore/AudioBassTrebleStandaloneTest.cpp`。
- **確認できた事実:** `bassCoeff_`と`trebleCoeff_`はメンバー初期化とconstructorで1.0をstoreされるが、`process()`は読み出さず、都度dB値からgainを計算する。
- **仮説／未検証:** atomic係数は旧実装の残存物、または将来の制御用である可能性がある。現時点の処理結果には使われない。
- **価値／懸念:** setter更新とaudio processing間のthread-safe受け渡しを目的にした設計なら、その同期契約が実際には機能していない可能性がある。
- **次に確認:** setter/processの並行利用契約を確認し、不要な残存メンバーか、atomic値を使うべき仕様かを判断する。

### 2026-10-11 — AudioChorusの一部公開設定がprocessへ未接続

- **関連:** `ArtifactCore/include/Audio/AudioChorus.ixx`、`ArtifactCore/src/Audio/AudioChorus.cppm`、`tests/ArtifactCore/AudioChorusStandaloneTest.cpp`。
- **確認できた事実:** `process()`は`rate_`、`depth_`、`baseDelayMs_`を使うが、`feedback_`と`mode_`を参照しない。出力は固定係数でdry 0.7／delayed 0.3を混合する。
- **仮説／未検証:** Feedback parameterやFlanger modeを変更しても音声出力へ作用しない可能性が高い。現状が仮実装か意図的仕様かは未検証。
- **価値／懸念:** UIから調整できる公開parameterが期待どおりの音響変化を生まない可能性がある。
- **次に確認:** Chorus／Flangerの仕様を定め、feedback loopとmode別delay/mixの期待応答を決めてからsignal-level独立ケースを設計する。

### 2026-10-11 — AudioReverbのsize parameterが処理に未接続

- **関連:** `ArtifactCore/include/Audio/AudioReverb.ixx`、`ArtifactCore/src/Audio/AudioReverb.cppm`、`tests/ArtifactCore/AudioReverbStandaloneTest.cpp`。
- **確認できた事実:** `size_`はsetter・parameter metadata・JSON保存復元に使われるが、`process()`は参照しない。また`allpassBuffer_`は確保されるが処理で使われない。
- **仮説／未検証:** sizeを変更しても残響の時間・密度は変わらない。これらが暫定実装か意図的な簡略化かは未検証。
- **価値／懸念:** parameter UIで調整できても、sizeが音響結果へ反映されない可能性がある。
- **次に確認:** DSP仕様を確定し、sizeが残響長・遅延・密度のどれを制御するか決めたうえで、異なるsize設定間の出力差を期待値として追加する。

### 2026-10-11 — AudioStereoMixer center／delay settersがprocessに未接続

- **関連:** `ArtifactCore/include/Audio/AudioStereoMixer.ixx`、`ArtifactCore/src/Audio/AudioStereoMixer.cppm`、`tests/ArtifactCore/AudioStereoMixerStandaloneTest.cpp`。
- **確認できた事実:** `setCenter`、`setLeftDelay`、`setRightDelay`はprivate stateを更新するが、`process()`は`leftRight_`だけを参照して左右gainを適用する。
- **仮説／未検証:** 現行APIの利用者はcenter extractionや左右delayが機能すると期待し得るが、実処理へ反映されない可能性が高い。各項目が将来用placeholderかは未検証。
- **価値／懸念:** UIやautomationに設定導線がある場合、値を変えても音声結果が変化しない。
- **次に確認:** setterの利用箇所とAudioStereoMixerの正式仕様を確認し、実装範囲が許可された際にそれぞれのDSP応答を決めて独立期待値を追加する。

### 2026-10-11 — AudioHighLowPassのhigh-pass recurrenceがDCを残す可能性

- **関連:** `ArtifactCore/src/Audio/AudioHighLowPass.cppm`、`ArtifactCore/include/Audio/AudioHighLowPass.ixx`、`tests/ArtifactCore/AudioHighLowPassStandaloneTest.cpp`。
- **確認できた事実:** high-pass pathは`hpInput = input - hpState`、次に`hpState = hpCoef * hpState + hpInput`を計算して`hpInput`を出力する。low-passも有効な場合、後段で`data[i]`をlow-pass stateへ再代入する。
- **仮説／未検証:** high-pass recurrenceは一定DC入力に対して0へ収束しない可能性が高く、両filter有効時はhigh-pass成分がlow-pass出力に置換される。追加DC rejection caseは意図した期待を表現するが、実行していない。
- **価値／懸念:** UI上high-passとlow-passの双方を設定できる場合、high-pass単独やcascade filterとしての期待と異なる応答になる可能性がある。
- **次に確認:** DSP仕様（one-pole cutoff、係数、cascade順）を決め、impulse／step responseと両filter同時有効時の期待値を定義してから修正範囲と回帰確認を行う。

### 2026-10-11 — AudioDelayが書き込み位置より先のbuffer indexを読む

- **関連:** `ArtifactCore/src/Audio/AudioDelay.cppm`、`ArtifactCore/include/Audio/AudioDelay.ixx`、`tests/ArtifactCore/AudioDelayStandaloneTest.cpp`。
- **確認できた事実:** 各sampleでread indexを`writePos + delaySamples`として計算し、同じ`writePos`へ現在sampleを保存する。delay bufferは初期状態で48000要素のゼロ値を持つ。
- **仮説／未検証:** 通常の遅延範囲では過去に書いたsampleではなく未書込の先行領域を読み続け、Impulseの遅延音が返らない可能性が高い。新しい1-frame impulse regression caseがこの期待を確認するが、ビルド・実行はしていない。
- **価値／懸念:** delay timeとwet mixを設定してもechoが出ず、wet signalが無音になる可能性がある。
- **次に確認:** 許可された作業範囲で独立suiteを実行し、SPSC状態・block境界を含む読出し／書込み位置を正す修正後に、単一blockおよび複数blockのimpulse応答を検証する。

### 2026-10-11 — AudioCompressor parameter metadataとsetter範囲が異なる

- **関連:** `ArtifactCore/include/Audio/AudioCompressor.ixx`、`ArtifactCore/src/Audio/AudioCompressor.cppm`、`tests/ArtifactCore/AudioCompressorStandaloneTest.cpp`。
- **確認できた事実:** `getParameters()`のthresholdは-60〜0 dB、attackは0.1〜100 ms、releaseは1〜1000 ms。一方setterとprocess内clampはthreshold -120〜20 dB、attack 0.1〜1000 ms、release 1〜5000 msを許容する。
- **仮説／未検証:** UIやautomationがmetadata範囲を制約として使用する場合、直接setter／JSON経由で設定できる値域と表示上の値域が異なる可能性がある。
- **価値／懸念:** serialized settingの表示／再編集時に値がmetadata外となり、UIでの再現性が不明瞭になる。
- **次に確認:** `EffectParameter`利用側とcompressor UIのclamp処理を調べ、各境界の正規契約を決めてから値域を横断する独立ケースを追加する。

### 2026-10-11 — AudioTone parameter metadataとsetter／process範囲が異なる

- **関連:** `ArtifactCore/include/Audio/AudioTone.ixx`、`ArtifactCore/src/Audio/AudioTone.cppm`、`tests/ArtifactCore/AudioToneStandaloneTest.cpp`。
- **確認できた事実:** `getParameters()`はfrequencyを20–20000 Hz、amplitudeを0–1として公開する。一方、直接setterはfinite値ならこの範囲外も保持し、`process()`ではfrequencyを0–96000 Hzにclampし、amplitudeはclampしない。
- **仮説／未検証:** parameter metadataをUIやautomationが編集制約として使用し、直接setterやJSONが別経路から範囲外値を渡す場合、表示値とprocess実効値が異なる可能性がある。
- **価値／懸念:** parameter range、serialization、DSP clampの契約が未統一だと、再現性・automation表示で混乱を生む。
- **次に確認:** `EffectParameter`の全利用箇所と既存tone UIのclamp責務を調べ、どの境界が正式な制約か決めてから範囲テストを加える。

### 2026-10-11 — AudioPannerの複数modeが同一gainを返す

- **関連:** `ArtifactCore/include/Audio/AudioPanner.ixx`、`ArtifactCore/src/Audio/AudioPanner.cppm`、`tests/ArtifactCore/AudioPannerStandaloneTest.cpp`。
- **確認できた事実:** `calculateGain()`は`StereoBalance`以外の`EqualPower`、`VBAP`、`Ambisonics`、`Binaural`で、azimuth/elevationに関係なく`{1.0f, 1.0f}`を返す。定電力ゲインの独立static関数は別に存在する。
- **仮説／未検証:** 複数modeは未実装または暫定placeholderである可能性がある。呼び出し側がmode名どおりの立体音響結果を期待するか、modeがUI設定だけで未使用かは未検証。
- **価値／懸念:** mode選択が処理結果へ反映されない場合、設定上の期待と実際のpanが一致しない。
- **次に確認:** 利用箇所とmodeごとのspeaker/HRTF/encoder設計を調べ、各modeの契約と独立期待値を定めてからgain動作テストを追加する。

### 2026-10-11 — AudioRingBufferの公開12chレイアウトと保存上限に差

- **関連:** `ArtifactCore/include/Audio/AudioSegment.ixx`、`ArtifactCore/include/Audio/AudioRingBuffer.ixx`、`ArtifactCore/src/Audio/AudioRingBuffer.cppm`、`tests/ArtifactCore/AudioRingBufferStandaloneTest.cpp`。
- **確認できた事実:** `AudioChannelLayout::Surround714`は12chと定義されている一方、`AudioRingBuffer::Impl`は`kMaxChannels = 10`で、`write()`は10超の入力チャンネルを拒否する。
- **仮説／未検証:** RingBufferが12ch layoutを扱う必要があるなら、enum上の対応と実装の最大チャンネル数が一致していない可能性がある。全consumerが12chを使わない構成かは未検証。
- **価値／懸念:** 7.1.4 audioをこのring bufferへ流すとwrite失敗となり、上流が失敗を適切に扱わなければ音声欠落につながる可能性がある。
- **次に確認:** 利用箇所から実際の最大channel layoutとwrite失敗時の扱いを追い、仕様を確定してから最大ch境界のstandalone契約を追加する。

### 2026-10-11 — AudioBufferQueueのチャンネル長チェックがframeCountと整合しない

- **関連:** `ArtifactCore/include/Audio/AudioBufferQueue.ixx`、`ArtifactCore/include/Audio/AudioSegment.ixx`、`tests/ArtifactCore/AudioBufferQueueStandaloneTest.cpp`。
- **確認できた事実:** `AudioSegment::frameCount()`は全チャンネルの最小長を返す。`AudioBufferQueue::push()`はその値より短いチャンネルを探すが、同じチャンネル群から計算した最小長より短いものは存在しないため、この長さ検証条件は成立しない。
- **仮説／未検証:** channelごとにサンプル数が異なる不正segmentもqueueへ入り得る。意図した契約が可変長チャンネルを許さない場合は、不揃いデータを拒否すべき。
- **価値／懸念:** consumerがフレーム数と各channelの有効サンプル数を異なる前提で処理すると、欠落サンプルの無音化や範囲外アクセスにつながる可能性がある。
- **次に確認:** AudioBufferQueue利用側のchannel length前提を追跡し、許可された範囲で均一長要件またはframeCount基準のvalidationを定義して不揃いchannelの契約テストを追加する。

### 2026-10-11 — ArtifactOptional swap writes across inactive union member

- **関連:** `ArtifactCore/src/Core/ArtifactOptional.cppm`、`tests/ArtifactCore/ArtifactOptionalContractTest.cpp`。
- **確認できた事実:** owning `ArtifactOptional<T>::swap` のpresent／empty分岐で、empty側の `contained_` にplacement constructionではなくmove assignmentを行ってから `hasValue_` をtrueにする。
- **仮説／未検証:** 非trivialな `T` ではinactive union memberのobject lifetime開始条件を満たさない可能性がある。新しいmove-only payloadケースは通常の値状態を検査するが、lifetime違反を直接証明するものではない。
- **価値／懸念:** ASan等で検出可能な未定義動作の疑いがあり、`unique_ptr` や他の非trivial payloadに対するswap安全性へ影響する可能性がある。
- **次に確認:** 子repo編集の指示範囲を確認したうえで、標準union lifetime規則を照合し、適切なsanitizer付きstandalone実行またはplacement constructionへの修正後検証を行う。

### 2026-10-11 — Audio ring-buffer capacity unit is implicit

- **関連:** `ArtifactCore/include/Core/Defaults.ixx`、`tests/ArtifactCore/DefaultsContractTest.cpp`。
- **確認できた事実:** `kAudioRingBufferCapacity` は `kDefaultSampleRate * 8` で定義され、コメントは8秒と説明している。チャンネル数は別定数 `kDefaultAudioChannels` で2となっている。
- **仮説／未検証:** 容量がsample frames単位かinterleaved scalar samples単位かは、この定数とコメントだけでは確定できない。チャンネル数を掛けるべきかの断定は未検証。
- **価値／懸念:** 単位を明示しないまま利用側がchannel countを乗除すると、実際の保持時間が意図とずれる可能性がある。
- **次に確認:** `kAudioRingBufferCapacity` の全利用箇所とbuffer indexing conventionを追い、unitと保持時間を定義してから追加契約を設ける。

### 2026-10-11 — Tag StringLike setter implementation is empty

- **関連:** `ArtifactCore/include/Utils/Tag.ixx`、`ArtifactCore/src/Utils/Tag.cppm`、`tests/ArtifactCore/TagContractTest.cpp`。
- **確認できた事実:** `Tag::setName(const S&)` のテンプレート定義は空で、非テンプレート `setName(const UniString&)` だけが名前を更新する。既存契約テストには `std::string` と `QString` を渡して更新を期待する2ケースがある。
- **仮説／未検証:** 空のテンプレート定義ではその2ケースの期待値を満たさない可能性が高い。C++20 module の実際の呼出し解決／ビルドは未確認。
- **価値／懸念:** Tag用standalone suiteの基本API契約に不整合がある可能性があり、ケース追加だけでは回帰品質を高められない。
- **次に確認:** 子repo編集の指示範囲を確認したうえで、許可された作業時にStringLike overloadの意図と実際のsuite結果を確認する。

### 2026-10-11 — RationalTime LCM can overflow before fallback

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** 異なるscaleの加算・減算は`lcm(s1, s2)`を呼び、`lcm`は`abs(a) / gcd(a,b) * abs(b)`をoverflow検査なしで計算する。その後にあるfallbackはLCM計算の後でしか到達しない。
- **仮説／未検証:** scaleのLCMがint64範囲を超える場合は既存の`fromSeconds` fallbackでwrapせず結果化するべきと考え、ほぼint64最大の互いに異なるscaleを使う加算・減算の独立caseを追加した。未実行。
- **価値／懸念:** 適切な値のRationalTime同士でも、異なる大きなtimebaseを混ぜた際に未定義signed overflowや誤結果を検出する。
- **次に確認:** LCMをchecked multiplyで構築し、表現不可時にfallbackへ進むよう修正した後、standalone suiteを実行して確認する。

### 2026-10-11 — HashValue unknown algorithm falls through to MD5

- **関連:** `ArtifactCore/src/Utils/HashValue.cppm`、`tests/ArtifactCore/HashValueContractTest.cpp`。
- **確認できた事実:** Qt algorithm変換は`Unknown`を含むdefaultでMD5を返し、`fromData`はそのdigest bytesを`Algorithm::Unknown`として保持する。結果の`isValid()`はfalseだが、raw bytesとhex digestは存在する。
- **仮説／未検証:** 未知のアルゴリズムをMD5へ代替すると呼び出し側が誤ったdigestを参照し得るため、factoryは空のinvalid値を返すべきと考え、独立caseを追加した。テスト未実行。
- **価値／懸念:** `Unknown`を指定した際のalgorithm downgradeを可視化し、誤ったハッシュ比較を防ぐ。
- **次に確認:** `fromData`と`fromFile`双方で未知enum値をdigest計算前に拒否する契約に統一する。

### 2026-10-11 — artifactAbs preserves negative floating zero

- **関連:** `ArtifactCore/include/Core/ArtifactMath.ixx`、`tests/ArtifactCore/ArtifactMathContractTest.cpp`。
- **確認できた事実:** 浮動小数点にも使える`artifactAbs`は`value < T{}`のときだけ符号反転するため、`-0.0 < 0.0`がfalseとなり符号bitが残る。
- **仮説／未検証:** absolute valueはnegative zeroをpositive zeroへ正規化する期待が妥当と考え、`std::signbit`で検査する独立caseを追加した。テスト未実行。
- **価値／懸念:** zeroの比較では見えない符号差を、後続の符号依存演算やserializationへ持ち越さないための検出。
- **次に確認:** floating overloadを`std::abs`等へ分けるか、negative zero保持を意図した仕様とするか決める。

### 2026-10-11 — RawImage accepts unsupported pixel type as valid

- **関連:** `ArtifactCore/src/Image/RawImage.cppm`、`ArtifactCore/include/Image/RawImage.ixx`、`tests/ArtifactCore/RawImageContractTest.cpp`。
- **確認できた事実:** `getPixelTypeSizeInBytes()`は既知scalar typeのみbyte幅を返し、未知typeでは0を返す。一方`isValid()`はshapeの正値、data非empty、pixelType非emptyだけを確認し、byte幅とdata長の整合を見ない。
- **仮説／未検証:** 有効なraw imageは既知pixel typeを持ち、width × height × channels × byte widthのpayloadを最低限含むべきと考え、未知type拒否と不足buffer拒否の独立caseを追加した。実装はArtifactCore子repoのため変更していない。テスト未実行。
- **価値／懸念:** 下流処理でbyte幅0や途中で切れたpixel bufferを有効画像として扱う危険を検出する。
- **次に確認:** oversized bufferの許容、strideや平面別layoutの有無、サイズ計算overflowを確認して必要byte数の厳密な契約を定める。

### 2026-10-11 — Id parser can ignore suffix after embedded NUL

- **関連:** `ArtifactCore/src/Utils/Id.cppm`、`tests/ArtifactCore/IdContractTest.cpp`。
- **確認できた事実:** QStringからIdを構築するとUTF-8化後の`QByteArray::constData()`をBoost UUID parserへ渡す。NUL以降のbyteはC文字列として見えない。
- **仮説／未検証:** UUID入力全体を検証し、末尾の埋め込みNULとsuffixを含む入力は拒否すべきと考え、valid UUID + NUL + suffixのtestを追加した。テスト未実行。
- **価値／懸念:** 見かけ上不正な入力が有効UUID prefixへ切り詰められるのを検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** parserへ明示byte長を渡すか、Boost parser後に消費長と元QString長が一致することを検証し、NUL境界を独立suiteで確認する。

### 2026-10-11 — Result can be successful without a value

- **関連:** `ArtifactCore/include/Utils/Result.ixx`、`tests/ArtifactCore/ResultContractTest.cpp`。
- **確認できた事実:** `Result<T>(Status)`はstatusをそのまま保持し、`Result<T>::success()`はvalue optionalでなくstatusのsuccess flagを返す。そのため`Result<int>(Status::ok())`は`success()==true`かつ`hasValue()==false`になる。
- **仮説／未検証:** valueを持たないResult<T>はfailure状態として正規化すべきと考え、食い違いを防ぐcaseを追加した。テスト未実行。
- **価値／懸念:** callerが成功を確認後にvalueを使う通常の契約で、未定義の空optional参照へ進む可能性を検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** `Status::ok()`からの変換方針を定め、Result<T>の成功状態とvalue presenceを常に整合させる。

### 2026-10-11 — ArtifactHashMap tryEmplace eagerly constructs and replaces duplicate value

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** variadic `tryEmplace(key, args...)`は`emplaceImpl(key, V(args...))`を呼ぶため、重複確認より前にmapped値を構築する。共通`emplaceImpl`は重複時に既存mapped値へ転送代入している。
- **仮説／未検証:** try-emplace系操作では重複時にmapped値を構築せず、既存値も保持するべきと考え、構築回数と値を検査するcaseを追加した。テスト未実行。
- **価値／懸念:** 重複keyに対する不要な副作用と、既存値の意図しない上書きを検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** `tryEmplace`と`emplace`の重複時契約を分け、構築遅延と値保持を実装後に独立suiteで検証する。

### 2026-10-11 — ArtifactArray appendAll self-alias can fail to terminate

- **関連:** `ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactArrayContractTest.cpp`。
- **確認できた事実:** `appendAll(const Array& other)`は`other`のrange-for中に`append()`する。`other`が*thisと同じobjectの場合、反復中にsizeが増え続け、end iteratorも動くため処理が終了しない。
- **仮説／未検証:** self-appendは元の要素列を一度だけ複製する契約が使いやすいと考えるが、ハングを起こすテストは追加していない。
- **価値／懸念:** 明示的なaliasingで無限に配列を伸ばし続ける危険がある。ArtifactCore子repoの実装は変更していない。
- **次に確認:** alias-safeなappendAll実装方針を決め、固定された元sizeでのself-appendを独立suiteで検証する。

### 2026-10-11 — Buffer afterLast drops input byte when delimiter is absent

- **関連:** `ArtifactCore/include/Utils/Buffer.ixx`、`tests/ArtifactCore/BufferContractTest.cpp`。
- **確認できた事実:** `splitLast()`はdelimiter不在時に`{empty, value}`を返すが、`afterLast()`は右partがemptyかどうかだけを調べ、非emptyなら先頭byteを常に除去する。
- **仮説／未検証:** delimiter不在時のafter-last helperは入力全体を保つべきと考え、主要aliasを同じcaseで検査するテストを追加した。テスト未実行。
- **価値／懸念:** separatorなしの文字列から誤って先頭byteが失われる境界を検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** delimiterが見つかったかを明示するAPIに分けるか、split結果の区別を使ってbefore/after helperの不在契約を揃える。

### 2026-10-11 — ArtifactRegex invalid recompile retains capture count

- **関連:** `ArtifactCore/include/Core/ArtifactRegex.ixx`、`tests/ArtifactCore/ArtifactRegexContractTest.cpp`。
- **確認できた事実:** `compile()`は開始時にprogramを消去するが、`groupCount_`は正常compile完了時にしか更新しない。そのためcapture付きregexの後にcompile失敗すると、invalid objectから以前のcapture数が返る。
- **仮説／未検証:** invalid状態のcapture数は0へ戻すべきと考え、2 captureから構文エラーへの再compileを検査するcaseを追加した。テスト未実行。
- **価値／懸念:** stale parser metadataを検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** invalid patternおよびemitter失敗後に、公開metadataが常に初期状態へ戻る契約を確認する。

### 2026-10-11 — AspectRatio decimal approximation skips positive minimum

- **関連:** `ArtifactCore/src/Core/AspectRatio.cppm`、`tests/ArtifactCore/AspectRatioContractTest.cpp`。
- **確認できた事実:** decimal近似ループは分母を1〜100で探索する一方、`round(ratio * denom) <= 0`の候補をすべて飛ばす。`0.004`では最大分母でも丸めた分子が0となり、初期値の`1:1`が残る。
- **仮説／未検証:** 正の入力は許容分母内で最も近い正の分数へ近似するのが望ましいと考え、`0.004`に対する`1:100`を契約テストに追加した。テスト未実行。
- **価値／懸念:** 1/200より小さい正のratioが1:1へ化ける境界を見つける。ArtifactCore子repoの実装は変更していない。
- **次に確認:** decimal parserの近似仕様を決め、極小正数と最大許容分母周辺を独立suiteで検証する。

### 2026-10-11 — Audio profiler reset leaves last callback frame metadata stale

- **関連:** `ArtifactCore/include/Utils/PerformanceProfiler.ixx`、`tests/ArtifactCore/PerformanceProfilerContractTest.cpp`。
- **確認できた事実:** `AudioEngineProfiler::resetStats()`はcallback/fill counters、sums、maxima、underflow count、buffer levelを0にするが、`lastCallbackFrames_`と`lastRequestedFrames_`をリセットしない。
- **仮説／未検証:** reset後snapshotはframe metadataも初期値0へ戻すと考え、caseを追加した。実行していない。
- **価値／懸念:** reset後のsnapshotが古いcallback状態を示し、集計値と矛盾する境界を検出する。
- **次に確認:** reset APIの期待契約を確認し、必要なら2つのatomicをreset処理へ含めて独立suiteで検証する。

### 2026-10-11 — ArtifactRandom real range width can overflow

- **関連:** `ArtifactCore/include/Core/ArtifactRandom.ixx`、`tests/ArtifactCore/ArtifactRandomContractTest.cpp`。
- **確認できた事実:** `between(double min, double max)`は`max - min`を先に計算するため、両端が有限でも`lowest()`から`max()`までの幅は正のinfinityになる。
- **仮説／未検証:** 有限な順序付き端点からは有限で半開区間内の値を返すべきと考え、全域端点のcaseを追加した。テスト未実行。
- **価値／懸念:** 広いrangeでinfinity / NaNが返る数値境界を検出する。ArtifactCore子repoの実装修正は行っていない。
- **次に確認:** overflowしない補間式または生成器を選び、端点近傍・最大範囲の分布を独立suiteで確認する。

### 2026-10-11 — Safe relative path predicate accepts embedded NUL

- **関連:** `ArtifactCore/include/Utils/Text/Path.ixx`、`tests/ArtifactCore/PathContractTest.cpp`。
- **確認できた事実:** `normalizePathSeparators()`はembedded NULを拒否するが、`isSafeRelativePath()`は空・absolute・`..` segmentだけを確認し、`"safe\\0name"`のようなNUL含有文字列をtrueにする。
- **仮説／未検証:** `isSafeRelativePath()`の「safe」判定はNUL含有入力も拒否すべきと考え、単独caseを追加した。テスト未実行。
- **価値／懸念:** C系APIへ渡した場合に文字列が途中で切れる可能性のあるpathを安全と誤判定する境界を検出する。ArtifactCore child repoの修正は行っていない。
- **次に確認:** path predicate群でNULの方針を統一し、修正後に独立suiteで確認する。

### 2026-10-11 — NameGenerator reinterprets placeholder text inside base names

- **関連:** `ArtifactCore/src/Utils/NameGenerator.cppm`、`tests/ArtifactCore/NameGeneratorContractTest.cpp`。
- **確認できた事実:** `makeCandidateZero()`はテンプレートの`(\\name)`をbase文字列で置換した後、完成途中の文字列全体から最初の`***`を検索する。baseに`***`が含まれると、番号スロットより先にbase内の文字が置換される。
- **仮説／未検証:** baseは任意の利用者名として不透明に保持されるべきと考え、`take***`を含む入力のcaseを追加した。実行していない。
- **価値／懸念:** パターン構文と値の文字列が衝突し、生成名が破損する境界を検出する。ArtifactCore側の修正はサブモジュール編集禁止のため行っていない。
- **次に確認:** テンプレートを値挿入前に分解する方法でslot位置を保持し、独立suiteで実行確認する。

### 2026-10-11 — Numeric min/max NaN behavior depends on argument order

- **関連:** `ArtifactCore/include/Utils/Numeric.ixx`、`tests/ArtifactCore/NumericContractTest.cpp`。
- **確認できた事実:** `min_same` / `max_same` はcommon typeへ変換後、`std::min` / `std::max` に委譲する。NaNとの `<` 比較がfalseとなるため、第1引数がNaNならNaNを返し、第2引数がNaNなら有限値を返す。
- **仮説／未検証:** 既存の標準アルゴリズム委譲を契約として記録するcaseを追加した。テストは未実行。
- **価値／懸念:** NaNの順序依存性を可視化し、将来の置換・最適化で無意識に意味が変わるのを検知できる。呼び出し側がNaN伝播を期待する場合は別途明示処理が必要。
- **次に確認:** 数値ユーティリティの公開契約としてNaN伝播を保証すべきか、利用側の要件を確認する。

### 2026-10-11 — NameGenerator padding is capped by local buffer

- **関連:** `ArtifactCore/src/Utils/NameGenerator.cppm`、`tests/ArtifactCore/NameGeneratorContractTest.cpp`。
- **確認できた事実:** `formatPaddedNumber()`は32 byteのstack bufferへ`snprintf`し、幅32以上では終端nullを含む固定領域によりrequested widthを満たせない。
- **仮説／未検証:** `zeroPad`は最小桁数として機能すべきとし、width 40の回帰caseを追加。実行していない。
- **価値／懸念:** 大きいpadding設定で生成名が切れる/幅不足となる問題を検出する。
- **次に確認:** `zeroPad`の上限契約を決め、可変長ではなくbounded安全な生成形式を選んでstandalone testを実行する。

### 2026-10-11 — ArtifactFunctionRef accepts a null function pointer as valid

- **関連:** `ArtifactCore/include/Core/ArtifactFunctionRef.ixx`、`tests/ArtifactCore/ArtifactFunctionRefContractTest.cpp`。
- **確認できた事実:** function pointer overloadは入力pointerのnull検査なしでstorageに保存し、常にinvoke thunkを設定するため`isValid()`がtrueとなる。
- **仮説／未検証:** null function pointerはdefault/nullptr wrapper同様invalidであるべきと考え、trait/runtime caseを追加した。未実行。
- **価値／懸念:** wrapperがnull targetをcallableと誤認してinvoke時に落ちる境界を検出する。
- **次に確認:** constructorでnull関数ポインタを空状態として扱い、standalone caseを実行確認する。

### 2026-10-11 — UniformScale implicitly copies owning Impl pointer

- **関連:** `ArtifactCore/include/Core/UniformScale.ixx`、`ArtifactCore/src/Core/UniformScale.cppm`、`tests/ArtifactCore/UniformScaleContractTest.cpp`。
- **確認できた事実:** `UniformScale`はowning raw `Impl*`とuser-declared destructorを持つ一方、copy/move operationsを宣言していない。C++の暗黙copyはポインタ値を複製する。
- **仮説／未検証:** 公開型は少なくともshallow copy不可であるべきとしてtrait assertionをruntime testにした。現実装ではcopy traitがtrueとなり失敗見込み。未実行。
- **価値／懸念:** 二重deleteを招くcopy pathの回帰を検出する。実装修正はArtifactCore child repo変更にあたるため、今回は行わない。
- **次に確認:** copy/move禁止または安全な所有権移譲を明示し、standalone suiteでtrait契約を実行する。

### 2026-10-11 — RationalTime rounding bias can overflow independently

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** `rescaledTo()`が積のoverflowを回避した後、正負の両経路で`denominator / 2`をnumeratorへ足す。`INT64_MAX / 2`相当の端数roundingでbias加算自体が符号付き範囲を超える。
- **仮説／未検証:** 結果はhalf-away-from-zeroの正負対称値になるべきとし、新しい端値caseを追加した。実行していない。
- **価値／懸念:** 積overflow fallbackだけでは保護されないsigned UBを独立に検出する。
- **次に確認:** 端値を安全に丸める除算・剰余ベース実装を行い、両符号の結果をstandalone suiteで検証する。

### 2026-10-11 — RationalTime rescaling negates INT64_MIN

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** `rescaledTo()`の負数丸め式は`-numerator`を評価し、`numerator == INT64_MIN`では符号付き範囲を超える。
- **仮説／未検証:** `INT64_MIN`を同scaleまたはscale 2から1へ変換する数学的結果を保つべきとしてcaseを追加した。未実行。
- **価値／懸念:** 丸めbias overflowとは別に、最小値だけで起こるnegation UBを検出する。
- **次に確認:** magnitudeベースの符号付き丸め処理を用意し、INT64_MINの恒等・縮小変換を実行検証する。

### 2026-10-11 — FileTypeDetector may accept invalid UTF-8 bytes as text

- **関連:** `ArtifactCore/src/File/FileTypeDetector.cppm`、`tests/ArtifactCore/FileTypeDetectorContractTest.cpp`。
- **確認できた事実:** magic判定のtext fallbackはheader bytesのcontrol rangeだけを検査し、UTF-8 continuationやscalar validityを確認している処理が見当たらない。既存suiteにはinvalid lead/continuationがBinaryという期待がある。
- **仮説／未検証:** invalid leadとcontinuation以外のoverlong, surrogate, out-of-range, truncated sequenceもBinaryになるべきと考え、table caseを追加した。コード読解上は現実装のtext fallbackで通る可能性があるが、テストは未実行。
- **価値／懸念:** UTF-8妥当性の穴をより完全に検出する。既存期待と実装が不一致の可能性があり、ArtifactCore child repoには変更を加えていない。
- **次に確認:** standalone suiteで上記4 encoding classを実行し、必要ならUTF-8 scalar validatorを実装・別依頼で扱う。

### 2026-10-11 — AssetMetaFile setters preserve extension JSON

- **関連:** `ArtifactCore/src/Asset/AssetMetaFile.cppm`、`tests/ArtifactCore/AssetMetaFileContractTest.cpp`。
- **確認できた事実:** `setUuid()` / `setType()` は既存`asset` objectを取得して対象keyだけを更新し、`data_`内の他のroot fieldも残す。standalone caseはsave/load後の未知field保持を検査する。
- **仮説／未検証:** 将来追加されたmetadata fieldを古いreaderが更新しても壊さず保存できることが互換性上望ましい。caseは追加したが未実行。
- **価値／懸念:** forward-compatible metadata round tripを回帰契約化する。未知fieldを意図して削除するAPIが将来必要な場合は明示操作との区別が要る。
- **次に確認:** `AssetMetaFileStandalone`でroot/asset extension fieldのsave/load往復を実行する。

### 2026-10-11 — SteadyClock tick subtraction can overflow

- **関連:** `ArtifactCore/include/Core/ArtifactChrono.ixx`、`tests/ArtifactCore/ArtifactChronoContractTest.cpp`。
- **確認できた事実:** `SteadyClock::between()`は両OS経路でtick差を符号付き`int64_t`上で減算する。POSIXは`to > from`判定後だが、Windowsは差分を計算してから符号を検査する。全域の正差と逆順差の絶対値は`int64_t`で表せない。
- **仮説／未検証:** 正方向の最大Duration飽和と逆方向の0 clampが期待動作と考え、extreme tick差の両向きcaseを追加した。未実行。
- **価値／懸念:** 長時間や不正tick入力での符号付き整数overflowを検出する。Windows分岐では逆順入力も減算自体がoverflowする。
- **次に確認:** 安全な差分計算（符号付き境界判定）を実装し、WindowsとPOSIX双方で境界を実行検証する。

### 2026-10-11 — FrameRate storage scale can overflow on finite input

- **関連:** `ArtifactCore/include/Frame/FrameRate.ixx`、`tests/ArtifactCore/FrameRateContractTest.cpp`。
- **確認できた事実:** `storageScaleForFps()`はNaN/Inf/nonpositive値をfallbackへ送るが、有限正値は上限確認なしで`std::llround`後`int64_t`へ変換する。
- **仮説／未検証:** `double`の極大有限値は表現可能範囲を大幅に超えるため、整数上限への飽和が安全な契約と考え、新しい回帰caseを追加した。未実行。
- **価値／懸念:** legacy frame scale変換で範囲外の浮動小数点整数変換を検出できる。期待値は上限飽和のAPI契約を明示するため、実装修正が必要になる見込み。
- **次に確認:** `fps > int64_t max`判定を`llround`より先に行う実装へ直し、独立FrameRate suiteで極大値と境界近傍を実行確認する。

# Insight Register

### 2026-10-11 — FastSettingsStore failed reload clears existing cache

- **関連:** `ArtifactCore/src/Core/FastSettingsStore.cppm`、`tests/ArtifactCore/FastSettingsStoreContractTest.cpp`。
- **確認できた事実:** `loadFromDiskUnlocked()`はCBORの読み込み・型検査より前に`cache_`をclearし、invalid payloadではfalseを返す。`reload(true)`はこの関数を直接呼ぶ。
- **仮説／未検証:** reload失敗時は最後に読み込み済みの値を保持し、エラー復旧可能にする方が望ましい。新しいcaseはその期待だが、実行していない。
- **価値／懸念:** ディスク破損や途中書き込み後のreloadでメモリ上の有効設定が消える可能性を検出する。ArtifactCore child repoは変更していない。
- **次に確認:** invalid CBOR・I/O失敗時のreload保証を定義し、必要ならtemporary cacheへのparse後にcommitする方式へ直して独立suiteで確認する。

### 2026-10-11 — FastSettingsStore sync failure retains retry state

- **関連:** `ArtifactCore/src/Core/FastSettingsStore.cppm`、`tests/ArtifactCore/FastSettingsStoreContractTest.cpp`。
- **確認できた事実:** `syncUnlocked()`は未open、親directory作成、`QSaveFile::open/write/commit`の失敗時にfalseを返し、成功時だけdirty/pendingをclearする。
- **仮説／未検証:** 各I/O失敗経路でdirty値とpending countが保たれ、後のretryが可能なことを回帰契約にした。新caseはtargetを既存directoryにして保存open失敗を狙うが、実行していない。
- **価値／懸念:** 保存エラー後に変更が誤ってclean扱いされる回帰を検知する。ディレクトリをファイルとして開けないOS挙動に依存する。
- **次に確認:** 実行環境で`QSaveFile`のopen failureとして安定するかを確認し、必要なら明示的に書込不能な親パスを使う。

### 2026-10-11 — RationalTime rescaledTo accepts negative target scales

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** `toFrameCount()`はfpsが0以下なら0を返す一方、`rescaledTo()`はtarget scaleが0の場合だけを無効扱いし、負値では負の分子として再スケールする。RationalTimeのconstructorは非正scaleを1に正規化する。
- **仮説／未検証:** `rescaledTo()`のtarget scaleも正値に限り、負値は0または安全な既定値を返すべきと考える。追加caseは0を期待するが実行していない。
- **価値／懸念:** 不正なtarget scaleで時間方向が反転する結果を防ぐ契約候補。ArtifactCore child repoは変更していない。
- **次に確認:** 呼び出し元で負target scaleが意図的に利用されていないことと、無効入力の期待値を確認してから実装修正する。

### 2026-10-11 — ArtifactHashMap moved-from destruction may dereference null buckets

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** move constructorとmove assignmentは移動元の`buckets_`を`nullptr`にし、`bucketCount_`を初期容量に戻す。destructorは常に`clear()`を呼び、`clear()`は`bucketCount_`回`buckets_[i]`を参照する。
- **仮説／未検証:** 別mapへのmove後に移動元が破棄されるとnull pointer dereferenceになる可能性が高い。実行していない。
- **価値／懸念:** move APIの基本的な破棄安全性に関わる。再現テストはstandalone suite全体をクラッシュさせる可能性があるため今回追加しなかった。ArtifactCore child repoは変更していない。
- **次に確認:** move後のsourceへ空bucketを再確保する等、valid destructible invariantを実装者と確認し、安全に失敗を報告できるテスト形態で検証する。

### 2026-10-11 — ShapeVertex mirrorHandles length mode is indistinguishable

- **関連:** `ArtifactCore/include/Core/ShapePoint.ixx`、`tests/ArtifactCore/ShapeVertexContractTest.cpp`。
- **確認できた事実:** `mirrorHandles(bool keepLength)`はtrue/false両分岐で`outHandle_`を`-inHandle_`に置き換えており、既存outgoing handleの長さを読み取らない。リポジトリ内の他の呼び出し・説明は検索で見つからなかった。
- **仮説／未検証:** `keepLength=false`は反対側ハンドルの既存長を維持しながらincoming方向と反対向きにそろえる意図かもしれない。新しい契約caseはこの解釈を期待するが、実装者の意図を示す仕様は見つけていないためテストは未実行。
- **価値／懸念:** UIから選べる長さモードが無効になっている可能性を切り分ける。ArtifactCore child repoは今回変更していない。
- **次に確認:** 呼び出し元または設計仕様で`keepLength`の意味を確定し、期待値を承認してから実装修正と独立suite実行を行う。

### 2026-10-11 — ArtifactTimer can expire before it starts for negative timeouts

- **関連:** `ArtifactCore/include/Core/ArtifactTimer.ixx`、`tests/ArtifactCore/ArtifactTimerContractTest.cpp`。
- **確認できた事実:** `hasExpired(timeoutMs)` はtimer validityを確認せず、`timer_.elapsed() >= timeoutMs` を返す。Qtの無効な`QElapsedTimer`はelapsedが負値となり、既存テストは未startの0 timeoutを未expired、開始済みの負timeoutをexpiredとしている。
- **仮説:** 未start timerではtimeout値にかかわらず未expiredを返すのがAPIとして一貫する。新しい負timeout未startケースは現実装でexpiredになる見込みだが、実行していない。
- **価値／懸念:** start前に待機完了と誤判定する呼び出しを検出できる。実装修正はArtifactCore child repoにあるため今回は行わない。
- **次に確認:** 意図する無効timerの契約を確認し、必要なら`isValid()` guardを加えて独立suiteを実行する。

### 2026-10-11 — FileTypeDetector treats malformed UTF-8 headers as text

- **関連:** `ArtifactCore/src/File/FileTypeDetector.cppm`、`tests/ArtifactCore/FileTypeDetectorContractTest.cpp`。
- **確認できた事実:** The text fallback rejects ASCII control bytes other than tab/LF/CR, but accepts every byte >= 32 without validating UTF-8. A regression case now expects malformed UTF-8 lead/continuation sequences to classify as Binary; it is not executed.
- **仮説:** Since valid UTF-8 Japanese text is already explicitly supported by a test, malformed multibyte sequences should not be classified as text. Current fallback will classify both new fixtures as Text.
- **価値／懸念:** This distinguishes valid Unicode text from arbitrary high-bit binary data. The implementation is in the ArtifactCore child repository, which is not modified here.
- **次に確認:** Confirm the intended accepted encodings (UTF-8 only or UTF-16/legacy as well), implement matching validation in the child repository if authorized, then run the standalone target.

### 2026-10-11 — AssetMetaFile tag removal does not normalize input

- **関連:** `ArtifactCore/src/Asset/AssetMetaFile.cppm`、`tests/ArtifactCore/AssetMetaFileContractTest.cpp`。
- **確認できた事実:** `addTag()` stores `tag.trimmed()`, while `removeTag()` compares each stored tag directly to the untrimmed argument. Added a standalone contract case that adds and removes the same whitespace-padded tag; based on the implementation it is expected to fail, but it was not run.
- **仮説:** Callers that pass the same user-entered tag string to add and remove can leave a tag behind because only the insertion path normalizes whitespace.
- **価値／懸念:** This test catches asymmetric tag matching without editing the child repository. If the intended contract is exact matching for removal, document that explicitly instead of normalizing.
- **次に確認:** Confirm tag matching policy, then fix/remove the regression case with the user-authorized ArtifactCore change and execute the standalone target when allowed.

### 2026-10-11 — ArtifactArray squeeze currently cannot reduce capacity

- **関連:** `ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactArrayContractTest.cpp`。
- **確認できた事実:** `squeeze()` calls `reserve(size_)`, while `reserve(n)` immediately returns whenever `n <= capacity_`. Thus a non-empty array with spare capacity retains its old allocation. Added a standalone contract case expecting capacity to shrink to size while preserving elements; it was not executed.
- **仮説:** The API name and QVector-like capacity interface imply `squeeze()` should release excess storage; current implementation does not.
- **価値／懸念:** This regression case detects an ineffective public capacity operation. Fixing it requires editing the ArtifactCore child repository and remains outside this test-only scope.
- **次に確認:** With child-repository change authorization, implement allocate/move/release or equivalent safe shrink logic, then run the standalone suite and non-trivial lifetime case.

### 2026-10-11 — ArtifactRandom inclusive maximum overflows its upper bound

- **関連:** `ArtifactCore/include/Core/ArtifactRandom.ixx`、`tests/ArtifactCore/ArtifactRandomContractTest.cpp`。
- **確認できた事実:** integer `between(min, max)` computes `max + 1` in signed `int` before passing it to Qt's bounded generator. Existing boundary tests stop at `INT_MAX - 1`.
- **仮説:** `between(INT_MAX, INT_MAX)` overflows before generation and cannot reliably satisfy the documented-by-tests inclusive range behavior. A singleton-at-maximum regression case was added; it was not executed.
- **価値／懸念:** Highest-end integer boundary is now represented in the independent suite, but the case may fail or invoke undefined behavior until the child implementation handles that endpoint.
- **次に確認:** ArtifactCore-side remediation should generate over a wider unsigned interval or special-case the upper endpoint; then execute this standalone target after explicit authorization.

### 2026-10-11 — ArtifactVariant empty-state destruction dispatch

- **関連:** `ArtifactCore/include/Core/ArtifactVariant.ixx`、`tests/ArtifactCore/ArtifactVariantContractTest.cpp`。
- **確認できた事実:** default construction sets `typeIndex_` to zero and exposes `isSet()==false`, but `destroy()` unconditionally dispatches `vtableForCurrent()` using the first alternative before resetting the index. Destructor and assignment call `destroy()` even for that unset state.
- **仮説:** an unset variant may invoke the first alternative's destructor on storage where no object lifetime began; behavior is especially hazardous when the first alternative is non-trivial. Added tests exercise default/copy/move unset state and assignment clearing an active alternative. They are unexecuted; sanitizers/runtime validation is still needed.
- **価値／懸念:** Empty-state lifecycle coverage can expose undefined destruction behavior hidden by the current first-alternative-is-int fixture. Fixing this requires editing the ArtifactCore child repository, which is outside the authorized scope.
- **次に確認:** With explicit permission for child-repository changes and test execution, guard `destroy()` on `typeIndex_ != 0`, then run the standalone suite under an available sanitizer.

### 2026-10-11 — Tag StringLike setter has an empty implementation

- **関連:** `ArtifactCore/include/Utils/Tag.ixx`、`ArtifactCore/src/Utils/Tag.cppm`、`tests/ArtifactCore/TagContractTest.cpp`。
- **確認できた事実:** `Tag::setName(const S&)` の `StringLike` template body は空である。実装側にある非-template setterは `const UniString&` overloadのみ。`StringLike` は `QString` と `std::string` を含む型を受け入れる。
- **仮説:** `QString` / `std::string` を直接渡す通常の呼び出しは空のtemplate overloadを選び、名前を変更しない可能性が高い。追加した2つのstandalone contract caseはこのAPI意図を固定し、実行時に失敗する見込みだが未実行。
- **価値／懸念:** generic setterを使う呼び出しが黙って無視される不具合を検出できる。修正が必要なら子repository編集禁止ルールに従い、ArtifactCore側の実装修正は別途明示依頼を待つ。
- **次に確認:** `Tag` のStringLike setterの意図と所有repoでの修正可否を確認し、許可後にこのstandalone suiteを実行する。

### 2026-10-11 — Id contract test references undeclared isValid

- **関連:** `ArtifactCore/include/Utils/Id.ixx`、`tests/ArtifactCore/IdContractTest.cpp`。
- **確認できた事実:** 現在の `Id` module interfaceには`isValid()`宣言がなく、テストの`ValidUuidStringRoundTripsInCanonicalForm`は`value.isValid()`を呼び出している。`Id.cppm`にもその定義は見当たらない。
- **価値／懸念:** 現行ソースのままでは独立suiteのコンパイルを妨げる可能性が高い。ビルド／テストは指示により実施していないため、実コンパイル診断は未確認。
- **次に確認:** `isValid()`を追加する設計意図があるか確認し、なければテストを`!isNil()`等の実在する契約に修正する。変更後にユーザー許可を得て独立suiteを実行する。

### 2026-10-11 — Tag StringLike setter template is empty

- **関連:** `ArtifactCore/include/Utils/Tag.ixx`、`ArtifactCore/src/Utils/Tag.cppm`、`tests/ArtifactCore/TagContractTest.cpp`。
- **確認できた事実:** `Tag::setName(const UniString&)` は `impl_->name` を更新するが、`StringLike` を受けるheader template overloadの本体は空。
- **価値／懸念:** `setName(QString/std::string/const char*)` など、`StringLike` conceptを満たす型で呼び出しても名前は変わらない。今回の独立suiteは動作する`UniString` overloadだけを検査し、この未実装挙動は通過テストとして隠さない。
- **次に確認:** template setterで`toQString`して`UniString` overloadへ委譲するか、template APIを削除するか判断した後、各対応文字列型のsetNameを回帰テストする。

### 2026-10-11 — ArtifactDict set calls absent ArtifactHashMap insert

- **関連:** `ArtifactCore/include/Core/ArtifactDict.ixx`、`ArtifactCore/src/Core/ArtifactHashMap.cppm`。
- **確認できた事実:** `ArtifactDict::set` は`map_.insert(key, value)`を呼ぶが、基底`ArtifactHashMap`が公開する挿入APIは`tryEmplace` / `emplace`で、`insert`は定義されていない。
- **価値／懸念:** `ArtifactDict::set`をインスタンス化するコードはコンパイルできない。既存の`ArtifactDict`テストはこの経路を使っており、直接module構成で検証する独立suiteもまだない。
- **次に確認:** ArtifactDictがinsert-or-assignかinsert-onlyか意味を決め、ArtifactHashMapの既存APIへ接続してから独立standaloneでset/get/removeを検証する。

### 2026-10-11 — Callback documentation references missing ifValid API

- **関連:** `ArtifactCore/include/Core/ArtifactCallback.ixx`、`tests/ArtifactCore/ArtifactCallbackContractTest.cpp`。
- **確認できた事実:** Callbackのclass commentは`isValid()`確認または`ifValid()`利用を案内するが、公開メソッドに`ifValid`はなく、`isValid` / `operator bool` / `invoke` / `clear` / `reset`だけがある。
- **価値／懸念:** 利用者が文書の例外安全な呼び出しAPIを探しても利用できず、empty時に直接`invoke`するとempty `std::function`の呼び出しになる。
- **次に確認:** null-safe invocation APIを追加するか、commentを実在APIに合わせる判断後、empty経路の仕様をテストする。

### 2026-10-11 — ArtifactFunction unsafe empty invocation and heap move

- **関連:** `ArtifactCore/include/Core/ArtifactFunction.ixx`、`tests/ArtifactCore/ArtifactFunctionContractTest.cpp`。
- **確認できた事実:** `operator()` は`invoker_`未設定時のguardなしに関数pointerを呼ぶ。heap callableのmoveでは`onHeap_`を先にtrueにした状態で、未初期化の移動先storageを`ptr()`経由で参照する。またmove後のsource callableはdestroyerを呼ばずmetadataだけnull化される。
- **価値／懸念:** 空function invocationは無効な関数pointer呼び出しとなる。heap callable moveは無効なstorage参照となり得て、SBO/heap双方のmoveはmoved-from callableのdestructorを欠落させる。
- **次に確認:** 空invokeを明示的な契約（assert/exception/no-op）にし、heap移動時のpointer transferとsource destructionを正しく実装してから、heap callable move・move assignment・destructor balance・empty invocationのテストを追加する。

### 2026-10-11 — ArtifactSpan end on null empty range

- **関連:** `ArtifactCore/include/Core/ArtifactSpan.ixx`、`tests/ArtifactCore/ArtifactSpanContractTest.cpp`。
- **確認できた事実:** default `ArtifactSpan` は`data_ == nullptr`、`size_ == 0`。`end()` は一律 `data_ + size_` を返す。
- **価値／懸念:** C++のポインタ加算対象となる配列がないnull pointerに対する`+ 0`は、標準上安全な配列内／one-past演算として保証されない。空spanをrange-forなどで走査する経路の扱いを確認したい。
- **次に確認:** empty span用にnullをそのまま返す分岐を設けるか、null pointer rangeをiteratorとして使わない契約にするか決め、default/empty spanのbegin/endとrange iterationを回帰テストする。

### 2026-10-11 — Position2D methods on moved-from objects

- **関連:** `ArtifactCore/include/Core/Position2D.ixx`、`ArtifactCore/src/Core/Position2D.cppm`、`tests/ArtifactCore/Position2DContractTest.cpp`。
- **確認できた事実:** move constructor/assignmentは`impl_`の所有権を移してsourceを`nullptr`にする。`Set` / `Get` はnull確認せず`impl_`を参照する一方、destructorはnull-safeな`delete`となる。
- **価値／懸念:** moved-from objectは破棄可能だが、再設定や読み出しはクラッシュする。通常のmoved-from型に期待される再代入・再利用可能性は満たさない。
- **次に確認:** moved-from状態を空として扱うか、有効なゼロ値へ再初期化するかを決め、move後のGet/Setと再代入の契約を追加する。

### 2026-10-11 — ArtifactPtr WeakPtr copy assignment ownership count

- **関連:** `ArtifactCore/include/Core/ArtifactPtr.ixx`、`tests/ArtifactCore/ArtifactPtrContractTest.cpp`。
- **確認できた事実:** `WeakPtr` はcopy constructorでweak countを増やすが、copy assignmentを定義していない。コンパイラ生成assignmentはcontrol-block pointerをmemberwise copyするだけなので、既存weak ownerへの代入でcountを適切に保持・解放しない。
- **価値／懸念:** 一時`WeakPtr`からの代入では一時objectのdestructorがcountを減らし、代入先が解放済みcontrol blockを参照する可能性がある。テストは未定義寿命状態を踏まないよう代入を使用しない。
- **次に確認:** copy/move assignmentの所有権契約を定め、weak countを保持する実装と同一control blockへのself-assignmentを回帰テストする。

### 2026-10-11 — ArtifactPtr UniquePtr reset with its current pointer

- **関連:** `ArtifactCore/include/Core/ArtifactPtr.ixx`、`tests/ArtifactCore/ArtifactPtrContractTest.cpp`。
- **確認できた事実:** `UniquePtr::reset(raw)` は先に `ptr_ = raw` を設定した後、元の `old` を削除する。そのため現在所有中の同じポインタを渡すと、所有値を解放した後にdangling pointerを保持する。
- **価値／懸念:** 同一アドレスへのresetはuse-after-free/double-deleteにつながり得る。
- **次に確認:** self-resetをno-opとする契約を定めて実装を修正し、同一pointerを渡した後の値アクセスと破棄回数を回帰テストする。

### 2026-10-11 — ArtifactThread condition notification synchronization

- **関連:** `ArtifactCore/include/Core/ArtifactThread.ixx`、`tests/ArtifactCore/ArtifactThreadContractTest.cpp`。
- **確認できた事実:** `Cond::wait(Lock&)` は渡された`Lock`のmutexを解放せず、別の`cv_lock_`でcondition variableを待つ。一方、`wakeOne/wakeAll`は`notified_`をmutex lockなしで書き換える。
- **価値／懸念:** wait前後のpredicate保護が呼び出し側mutexと同期せず、通知のlost wakeや`notified_`のdata raceを起こし得る。
- **次に確認:** `Lock`から待機中にmutexを解放／再取得できる設計にし、predicateを同一mutex下で確認するAPI契約を確定してrace・lost-wake回帰ケースを加える。

### 2026-10-11 — ArtifactOptional empty/present swap and empty initializer list

- **関連:** `ArtifactCore/src/Core/ArtifactOptional.cppm`、`tests/ArtifactCore/ArtifactOptionalContractTest.cpp`。
- **確認できた事実:** `ArtifactOptional<T>::swap` の片側だけpresentの分岐はempty側のunion memberへplacement constructionせずmove assignmentを行う。initializer-list constructorは空listでも`hasValue_`をtrueにし、contained `T`を構築しない。
- **価値／懸念:** empty/present swap後のdestructorは未構築`T`を破棄し、empty initializer-list optionalも同じ不正状態となる。
- **次に確認:** 片側presence swapはplacement move constructionを行う状態遷移に修正し、empty initializer-listはemptyとする契約を定めてそれぞれの回帰テストを追加する。

### 2026-10-11 — ArtifactMath signed absolute minimum overflow

- **関連:** `ArtifactCore/include/Core/ArtifactMath.ixx`、`tests/ArtifactCore/ArtifactMathContractTest.cpp`。
- **確認できた事実:** `artifactAbs` は負値を unary `-value` で正値化する。符号付き整数の最小値は対応する正値を同じ型で表せず、評価は符号付きoverflowになる。
- **価値／懸念:** `artifactAbs(INT_MIN)`等は安全な絶対値を返さない。
- **次に確認:** APIの戻り型を広げるか飽和させるか仕様を定め、整数最小値の回帰ケースを決める。

### 2026-10-11 — ArtifactRandom integer upper-bound overflow

- **関連:** `ArtifactCore/include/Core/ArtifactRandom.ixx`、`tests/ArtifactCore/ArtifactRandomContractTest.cpp`。
- **確認できた事実:** 整数版`between(min, max)`はinclusive化のため`max + 1`を`int`で計算してQt APIへ渡す。
- **価値／懸念:** `max == INT_MAX`の場合に符号付き整数overflowとなり、定義済み動作ではない。
- **次に確認:** 上限inclusive要件を保てる範囲幅APIまたは64-bit中間値による実装へ変更し、`INT_MAX`近辺を含む境界回帰ケースを追加する。

### 2026-10-11 — ArtifactVariant destroys storage while empty

- **関連:** `ArtifactCore/include/Core/ArtifactVariant.ixx`、`tests/ArtifactCore/ArtifactVariantContractTest.cpp`。
- **確認できた事実:** `destroy()` は `typeIndex_ == 0` の空状態でも index 0 の vtable を取得して destroyer を呼ぶ。index 0 は第1 alternativeであり、既定構築 variant の storageにはその型のオブジェクトが構築されていない。
- **価値／懸念:** 第1 alternativeが非自明デストラクタを持つ場合、空variantの破棄や値を持つvariantから空variantへの代入で未構築オブジェクトに対して破棄処理を行う。
- **次に確認:** `destroy()` は `isSet()` の時だけ破棄関数を呼ぶようにし、空状態の生成・破棄と値から空へのcopy assignmentを回帰テストする。

### 2026-10-11 — ArtifactAlgorithms erase API does not match ArtifactArray

- **関連:** `ArtifactCore/include/Core/ArtifactAlgorithms.ixx`、`ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactAlgorithmsContractTest.cpp`。
- **確認できた事実:** `artifactRemoveIf` は削除時に`container.eraseAt(index)`を呼ぶ。一方、`ArtifactArray`が提供する削除APIは`removeAt(index)`で、`eraseAt`は定義されていない。
- **価値／懸念:** `artifactRemoveIf(Array<T>&, ...)` は要求メンバーが一致せず直接利用できない。suiteではAPIどおり`eraseAt`を持つ小fixtureを介してアルゴリズム単体を検査する。
- **次に確認:** `artifactRemoveIf` を`removeAt`コンテナへ一般化するか、`ArtifactArray`へaliasを設けるか決めて、実際のArray統合テストを追加する。

### 2026-10-11 — ArtifactHashMap collision iteration and moved-from lifetime

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** iteratorの`operator++()`はbucket indexを進め、現在Nodeの`next`を辿らないため、同一bucket内の複数Nodeをiterationで全件訪問しない。move constructor/assignmentは移動元のbucket pointerをnullにするが、destructorの`clear()`はbucket配列をdereferenceする。また、`const_iterator`はmutable `iterator` のaliasでconst mapの`find()`もmutable referenceを返す。`tryEmplace`重複時は既存mapped valueへ新valueを代入してfalseを返す。
- **価値／懸念:** collision iterationの取りこぼし、move元destruction時のnull dereference、const map経由の変更、try-emplace名称から期待される既存値非変更との不一致がある。
- **次に確認:** iteratorがchainとbucket遷移の両方を扱うよう修正し、const iteratorを読み取り専用にし、move元を空の有効状態にする。重複時のtryEmplaceを非変更契約にするか決める。回帰テストは修正後にcollision iteration、const性、重複値保持、move元スコープ終了を含める。

### 2026-10-11 — ArtifactArray squeeze currently cannot reduce capacity

- **関連:** `ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactArrayContractTest.cpp`。
- **確認できた事実:** `squeeze()` は `size_ < capacity_` の場合に `reserve(size_)` を呼ぶが、`reserve()` は要求容量が現在容量以下なら即returnする。従ってstorage容量は減らない。
- **価値／懸念:** 名前と一般的なcontainer契約から期待される容量縮小が起きず、過剰確保メモリを保持する。
- **次に確認:** 容量を縮める再配置処理が必要か判断し、必要なら要素move/destructionとallocation failure時の保証を定めた後でcapacity assertionを追加する。

### 2026-10-11 — AudioEngineProfiler reset retains last frame counts

- **関連:** `ArtifactCore/include/Utils/PerformanceProfiler.ixx`、`tests/ArtifactCore/PerformanceProfilerContractTest.cpp`。
- **確認できた事実:** `resetStats()` はcallback/fill counters、durations、underflow、buffer levelを0にするが、`lastCallbackFrames_` と `lastRequestedFrames_` はresetしない。
- **価値／懸念:** reset後snapshotが過去のframe数と新しい累積統計を混在させる。これが意図的なlast-known metadataか、reset漏れかは未検証。
- **次に確認:** reset後もlast-known値を保持する契約か決め、仕様がzero resetなら両atomicもresetして期待値を追加する。

### 2026-10-11 — PatternNameGenerator without a number slot can loop forever

- **関連:** `ArtifactCore/src/Utils/NameGenerator.cppm`、`tests/ArtifactCore/NameGeneratorContractTest.cpp`。
- **確認できた事実:** `Generate` はcandidateが `usedNames` に存在する限り番号を増やして再試行するが、`makeCandidateZero` はpatternに `***` がなければ候補文字列を変化させない。同じ固定文字列が使用済みになった後の再生成は終了しない。
- **価値／懸念:** 固定patternやname slotだけのpatternを複数回使うと呼び出しが停止する可能性がある。再試行上限なしのループであり、通常のテストではプロセスをハングさせうる。
- **次に確認:** number slotがないpatternの重複方針（エラー、同名返却、pattern妥当性検証）を決め、boundedな失敗契約を実装した後に回帰テストを追加する。

### 2026-10-11 — MultipleTag moved-from state is not reusable

- **関連:** `ArtifactCore/src/Utils/MulitpleTag.cppm`、`ArtifactCore/include/Utils/MultipleTag.ixx`。
- **確認できた事実:** move constructor と move assignment は移動元の `impl_` を `nullptr` にする。destructor は許容するが、`count()`、`clear()` などの通常メソッドは `impl_` を直接参照する。
- **価値／懸念:** 移動元を破棄以外に使うと null dereference になる。クラスの moved-from 契約が未定義であり、利用側が再利用可能と期待する場合は問題になる。
- **次に確認:** moved-from 状態を空の有効状態へ戻すか、型の契約として明示するかを決めてから、move 後再利用テストを追加する。

### 2026-10-11 — Buffer view lifetime and missing-delimiter edge

- **関連:** `ArtifactCore/include/Utils/Buffer.ixx`、`tests/ArtifactCore/BufferContractTest.cpp`。
- **確認できた事実:** `toBufferView(const QString&)` はローカル `QByteArray` のdata pointerを `BufferView` として返すため、戻り値はローカル所有物の破棄後を参照する。`afterLast` は区切りがないと `splitLast` が `{empty, value}` を返し、非empty側の先頭1byteを除いたviewを返す。
- **価値／懸念:** QString変換のviewはuse-after-lifetimeとなる。delimiter未検出時の `afterLast` は命名上期待される意味とずれる可能性がある（期待仕様は未確認）。独立suiteではこれらの不安定な期待値を固定せず、テスト対象から外した。
- **次に確認:** Buffer view APIで所有権が期待されるか、または呼び出し側所有のUTF-8作業領域を受け取る形にするかを確認する。`afterLast`／`takeAfter`／`suffixAfter` の delimiter未検出仕様を定めて境界テストを追加する。

未着手の設計判断、現在の優先方針に直結する実装候補、実機検証待ちだけを記録する。実装済みの詳細履歴と過去の調査は [Insight Archive (through 2026-09-01)](docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md) を参照。

### 2026-10-10 — 2D/3D layer transform composition order

- **関連:** `ArtifactCore/src/Transform/TransformHelper.cppm`、`tests/ArtifactCore/CompositionTransformTimeContractTest.cpp`。
- **確認できた事実:** `combineLayerTransform2D(local, parent)` は `local * parent` を返す一方、3D 版は `parent * local` を返す。2D 実装コメントは親変換を子空間で適用すると記述し、3D 実装は `QMatrix4x4` の標準的な親×子順を使っている。
- **価値／懸念:** 2D と 3D の規約差が意図的か、ネストしたレイヤーの描画・座標変換で整合しているかは未検証。単体テストでは各 API の現在の契約を固定できるが、親子レイヤーの実描画結果までは保証しない。
- **次に確認:** 親子レイヤーを使う描画・bounds・入力座標の各経路で変換積の順序を追い、同じ階層構成の 2D/3D テストを追加すべきか判断する。

### 2026-10-10 — XPU CPU lanes share oneTBB through Core.Parallel

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/src/Core/Parallel.cppm`、`ArtifactCore/include/Common/ThreadPool.ixx`。
- **確認できた事実:** XPU mixed の CPU frame lanes はソフトウェア renderer を通り、複数の画像処理が `ArtifactCore::Parallel::For` を呼ぶ。同 API は `tbb::parallel_for` に委譲される。既存 `ThreadPool` は `tbb::global_control(max_allowed_parallelism, concurrency_)` を作り、TBB の global control は scheduler 内で同時実行する worker thread 数を制限する。TBB の implicit arena は parallel algorithm を呼ぶ user thread ごとに関連付く ([task arena](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/latest/elements/onetbb/source/task_scheduler/task_arena/task_arena_cls)、[global control](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/latest/elements/onetbb/source/task_scheduler/scheduling_controls/global_control_cls))。
- **今回の対応:** `Core.Parallel` に共有 TBB arena API を置き、全 `Parallel::For` と XPU CPU frame lanes を同じ arena に通す。XPU は初回 arena 初期化へ process-available thread count を渡し、job host limit を実 concurrency 以下へ上限化する。MSVC module compile で `ThreadPool.ixx` の TBB inline実装を Artifact から使う形が失敗したため撤回し、TBBの実装詳細を既存 `Core.Parallel.cppm` 内へ閉じた。XPU summary と統合テスト source に scheduler と concurrency の確認を加えた。
- **価値／懸念:** XPU CPU frame lanes と内側の TBB kernels が共有 concurrency cap を使う。Taskflow、QThreadPool、GPU submission、writer/encoderとの完全な process-wide budget は未統合。Debug構成でCore/Artifact/test targetがビルド成功し、RTX 4070 Ti Vulkan + CPUのmixed-text、2D/3D particle、10-frame all-adapters/backpressureテストがそれぞれ3回連続成功。共有TBB arena/concurrencyのdiagnosticsも確認済み。複数物理GPU/iGPUでの受入は未確認。
- **次に確認:** ArtifactCoreとArtifactの対象ターゲットをビルドし、XPU mixed runtimeでCPU frame lanesとnested `Parallel::For` が同一arenaに参加すること、summaryの実 concurrency、TBB worker数とprocess CPU utilizationを検証する。

### 2026-10-10 — XPU D3D12 requires matching Agility runtime exports

- **関連:** root `CMakeLists.txt` Windows SDK `try_compile` paths、`Artifact/src/Render/DiligentDeviceManager.cppm`、`Artifact/tests/RenderQueueLayerImageIntegrationTest.cpp`。
- **確認できた事実:** CMake `D3D12_SUPPORTED` と `VULKAN_SUPPORTED` は両方TRUE。ArtifactはAgility SDK 619 のversion/pathを無条件exportしていたが、SDK `D3D12Core.dll` が配置されない構成では `D3D12_ERROR_INVALID_REDIST` (`0x887E0003`) でdevice creationが拒否される。テスト機は Intel Core i7-13700KF と NVIDIA GeForce RTX 4070 Ti の1 GPU構成で、iGPUは列挙されない。
- **今回の対応:** Agility SDK runtimeがCMakeで設定された場合だけversion/pathをexportする。SDKなし構成ではOS D3D12 runtimeへ戻す。新しいDebug treeでRender Queue統合テストを実行し、CPU＋D3D12 primaryの混在10-frame dispatch、D3D12 backend選択、画像内容を確認。自動backend選択のCPU＋Vulkan混在画像テストも成功。
- **価値／懸念:** D3D12 primaryを利用できる構成が復旧した。Agility SDKを設定した構成は未検証。現ホストではiGPUおよび複数物理GPUを使った同時レンダリングを受け入れられない。
- **次に確認:** Agility SDK 619を明示設定したruntimeと、iGPU＋dGPUまたは複数dGPUの実機で、各GPU laneの参加・フレーム数・画像出力を検証する。

### 2026-10-10 — Particle XPU parity uses different rasterizers

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`、`tests/Artifact/RenderQueueLayerImageIntegrationTest.cpp`。
- **確認できた事実:** software particle rendering paints a QPainter radial-gradient circle sprite. GPU particle rendering draws an instanced quad and computes a soft circular alpha edge with a shader `smoothstep`; the rasterizers differ.
- **今回の対応:** isolated XPU particle Render Queue test verifies deterministic CPU reference output, XPU frame outputs, plan diagnostics, and both 2D/3D layer setup. The GPU-only production particle test separately verifies visible pixels. CPU/GPU lane participation is asserted in the separate multi-frame text case.
- **価値／懸念:** exact CPU/GPU particle parity remains unverified; the XPU case records mismatch count/bounds without guessing a tolerance. Both XPU particle and GPU-only visibility cases passed three consecutive runs.
- **次に確認:** capture and compare XPU particle pixels across different GPU vendors/backends; do not call parity complete from manager output alone.

### 2026-10-10 — XPU pull dispatch adapts frame share without static weights

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、XPU `nextFrameCounter` worker loop。
- **確認できた事実:** mixed/multi-GPU lanes receive one distinct startup frame, then claim the next frame only after `renderOneFrame` completes. There is no fixed per-device frame quota after seeding.
- **今回の対応:** documented the completion-driven pull behavior next to the shared counter and in P4. Per-device timing weights remain diagnostic rather than being fed back into assignment.
- **価値／懸念:** fast lanes naturally claim more remaining frames, so a stale startup weight is unnecessary and could bias work after scene/device conditions change. This does not prove there is no starvation under long-tail frames or output-buffer backpressure.
- **次に確認:** use runtime bench frame counts and lane render durations on dissimilar CPU/iGPU/dGPU hardware. Add a weighted policy only if the pull scheduler shows a repeatable imbalance that lowers throughput.

### 2026-10-10 — Render Queue の HW encoder slot は現行の単一job実行では飽和しない

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`。
- **確認できた事実:** `ArtifactRenderQueueService::startAllJobs()` は `isRendering_` で同一serviceの二重開始を拒否し、単一 `workerThread_` 内で `jobOrder` を順番に処理する。各動画jobのencoder backendはそのjob処理中に作成される。
- **今回の対応:** `ARTIFACT_XPU_MAX_HW_ENCODERS` の共有slotをNative HW / ffmpeg HW pipe / Vulkan pipeへ適用し、session lifetime中の上限をコード化した。
- **価値／懸念:** 将来job並列化または独立encoder利用が入った際のプロセス共通上限を先に確立できる。ただし現行Render Queue単体では複数jobが同時にencoderを開かないため、設定値の差によるthroughput/backoffのruntime効果はまだない。今回の変更だけでXPU並列性能が改善したとは扱わない。
- **次に確認:** encoder backendを呼び出す他の同一process経路とservice構築可視性を確認し、競合可能なconsumerがなければ、job並列化導入時に同時encoder上限の受入試験を追加する。

### 2026-10-10 — XPU consumer worker と RenderQueue writer 上限の統合

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/src/IO/Image/AsyncImageWriterManager.cppm`。
- **確認できた事実:** XPU混在時はCPU/GPU frame worker上限とは別に画像書き込みfutureが生成されていた。`AsyncImageWriterManager` は既定hardware_concurrencyサイズのthread poolを作り、enqueue件数を呼び出し側へ通知・制限するAPIがない。
- **今回の対応:** RenderQueue側のasync writer futureを1〜2件に固定し、XPU mixed jobではその同時writer数をhardware thread数から予約してframe lane上限を決める。6 logical threads未満ではasync writerを止めて同期書き込みへ戻す。job summaryへ共有枠を出し、mixed画像統合テストに枠の包含関係を追加。manager指定時はbounded future経路へfallbackし、理由を警告する。ArtifactCoreは変更していない。
- **価値／懸念:** mixed job内のCPU render laneとsequence writer threadが別々に上限を消費しない。これはjob-localな保守的予算であり、UI・TBB・他poolを含むprocess-wide schedulerではない。CPU budgetの効果、writer失敗時の挙動は実行未確認。
- **次に確認:** ユーザー許可後に混在レンダー＋async sequenceの低論理CPU数／通常CPU数条件で、summaryのthread枠、キャンセル、書込失敗伝播、長時間メモリ上限を確認し、render/consumer総CPU予算をbenchで確定する。

### 2026-10-10 — XPU mixed video encoder もframe laneのhost budgetを使う

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`、`RenderQueue/XpuEncoderThreads`。
- **確認できた事実:** native FFmpeg settingsは`threadCount=0`のままencoderへ渡しており、external FFmpeg pipeにも`-threads`指定がなかった。video consumerはframe workerと並行して動く。
- **今回の対応:** mixed動画ではencoder thread数の既定を1にし、正数指定もusable host thread数へclampする。service側は同数をframe laneの予算から控除し、nativeおよびpipe backend双方へthread countを渡す。
- **価値／懸念:** frame renderingとencodeが同じCPU資源を超過予約しにくくなる。混在jobのthread枠であり、アプリ全体のpoolを横断してはいない。FFmpegが実際にそのthread上限を守ること、画質・出力と速度への影響は未検証。
- **次に確認:** software/hardware native backendとexternal pipeで、混在動画のsummary予約数・encoder起動ログ・実プロセスthread数を確認し、thread数別benchを行う。

### 2026-10-10 — XPU mixed eligibility must include the selected render backend

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`xpuMixedEligibleForJob`。
- **確認できた事実:** CPU backendを選んだjobでは`cpu-render-backend-selected`をmixed unavailable reasonとして記録していた一方、eligible predicate自体にはGPU backend条件がなく、CPU/GPU lane計画と予算予約へ進む余地があった。
- **今回の対応:** `useGpuBackend`をeligibility条件に加え、実際にGPU rendererが使われるjobだけをXPU mixedとして扱う。Render Queue画像統合テストにCPU backend baselineのunavailable reason、未起動mixed lane、未予約budgetを検査するassertionを追加した。
- **価値／懸念:** unavailable reasonとplan/CPU予算の分類が一致する。統合テストは未実行で、CPU backendを明示した mixed 要求のruntime summaryは未確認。
- **次に確認:** GPU backend有効／CPU backend明示の両ケースで、active mixed plan、frame lane数、writer/encoder予約が期待通り切り替わることを確認する。

### 2026-10-10 — XPU thread budget should use process-available CPUs

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`。
- **確認できた事実:** XPU lane/encoder budgetは`std::thread::hardware_concurrency()`でmachine-wide logical thread数を参照していた。Qtの`QThread::idealThreadCount()`は、OSが対応する場合にprocessが利用可能なlogical processor数を返し、affinityやCPU hotplugで変化し得る。
- **今回の対応:** serviceとencoderの予算計算を`QThread::idealThreadCount()`へ統一し、processに適用されたCPU制限を反映する。
- **価値／懸念:** affinity/container/job objectなどの制約が公開される環境で、実際に使えないCPU数を予算へ数えない。OSごとの検出範囲とjob開始後の変化はruntime未確認。
- **次に確認:** affinityを制限した環境でsummaryのhardwareThreadsが利用可能processor数に一致すること、同一jobのencoder/service両方が同じ数を使うことを確認する。

### 2026-10-10 — 既存Schedulerはdevice-affineなXPU frame laneを表現しない

- **関連:** `Artifact/src/Render/ArtifactRenderScheduler.cppm`、`ArtifactCore/include/Render/MFR/MFRDispatcher.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **確認できた事実:** `ArtifactRenderScheduler` は優先度・重複排除付きの汎用RenderTaskとTaskSystemを持ち、device/renderer affinityをタスクに渡すAPIが見当たらない。`MFRDispatcher` のFrameTaskもframe番号だけを受け取り、実行worker identityをcallbackへ返さない。現在のGPU laneは各immediate context・cache・composition snapshotを単独所有する。
- **設計仮説（未検証）:** 汎用Schedulerへ現状のframe callbackをそのまま移すと、どのGPU laneが実行中かを外部状態で割り当てる必要が生じ、context所有権やin-flight上限が壊れる可能性がある。XPU dispatcher APIはnode/worker identity付きのclaim/completeを提供する必要がある。
- **価値／懸念:** 既存Schedulerの名目上の再利用でGPU資源所有を弱めるリスクを避けられる。一方、現行のローカルatomic採番とraw thread管理はRenderQueueに残る。
- **次に確認:** P2 runtime受入後、device-affine worker descriptorを持つAPIが現在の枠内で追加できるか、シグナル／スロットを増やさず段階的に統合できるかを設計レビューする。

### 2026-10-10 — XPU assist と consumer pipeline は実際の重複実行が必要

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、XPU P3/P4。
- **確認できた事実:** `igpu=assist` のpreview縮小と動画 `pipeline=on` は、CPU処理を `std::async` で開始して同じフレーム内ですぐ `get()` しており、iGPU処理でもencodeとの重複でもなかった。GPU assist plan node は現時点で `maxInFlight=0`。
- **今回の対応:** 誤解を招く即時待機のasync処理を撤去し、動画pipeline要求は非対応警告を出して同期経路を使うようにした。マイルストーン文書も未実装に訂正した。さらに、専用D3D12 workerに偏っていたGPU frame数の観測へprimary rendererのadapter IDとframe数を加えた。
- **価値／懸念:** 「async」指定だけでXPU利用を誤認せず、hot pathで余計なfuture/threadを生成しない。iGPU assistそのものと動画のbounded lookahead pipelineは未実装。
- **次に確認:** GPU資源で実行可能な既存convert/scale/encode経路とrenderer/device ownershipを特定し、assistの一作業を実GPUへ割り当てる。動画pipelineはbounded lookaheadで次フレーム変換と現フレームencodeを実際に重ねる。primaryとdedicated GPU laneを同時に使うfallback caseのbench counter整合は未検証。

### 2026-10-10 — XPU 開発スイッチを ArtifactStartup.json から解決する

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`、`docs/technical/STARTUP_FLAGS_CONTRACT_2026-09-29.md`。
- **確認できた事実:** Startup Flags 契約は XPU 系環境変数を `RenderQueue/Xpu*` へ段階移行する計画だが、Render Queue と encoder は環境変数だけを直接読んでいた。また GUI 起動では `ArtifactAppSettings::instance()` が `QApplication` と Startup JSON 読込より先に呼ばれていた。
- **今回の対応:** XPU spec、worker上限、iGPU参加、preview、sequence、parity、pipeline、bench、clone診断、encoder thread/preset/HW上限の値を `LayeredConfigStore` 起点へ移した。JSON があれば対応 env より優先し、各 setting accessor は解決値を起動後初回にキャッシュする。`AppMain` の GUI 起動順を変更し、Startup JSON を最初の `ArtifactAppSettings` 取得前に読み込む。テンプレートに任意keyの一覧を記載。
- **価値／懸念:** IDEやExplorer起動でもXPUを設定でき、解決層の読み込みをrender frame hot pathへ持ち込まない。StartupFlags競合時のログと全keyの型・優先順位はruntime未検証。
- **次に確認:** XPU各keyについて環境変数のみ／JSONのみ／JSONとenv競合／未設定の4条件で解決値を確認する。ビルド・テストはユーザー明示指示待ち。

### 2026-10-10 — XPU GPU worker の初期化失敗を adapter 単位で隔離

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`GpuFinalWorker` 初期化。
- **確認できた事実:** worker 構築ループは composition snapshot または adapter renderer の初期化に失敗したとき、既に作成済みの全 GPU worker も `workersReady` gate で無効化していた。
- **今回の対応:** 失敗した adapter の worker だけを skip し、他の GPU worker は保持する。CPU fallback snapshot が利用できなくても GPU renderer 自体は登録する。混在時の最低1 worker条件と legacy multi-GPU時の最低2 worker条件は維持する。
- **価値／懸念:** 1台の不調 adapter が他の正常な GPU lane を無効にしない。fallback snapshot がない GPU lane は GPU render 失敗時にCPUへ切り替えられない。
- **次に確認:** adapter 初期化失敗・snapshot clone失敗を個別に注入し、残存 worker のフレーム出力、fallback、summary計数を確認する。ビルド・テストはユーザー明示指示待ち。

### 2026-10-10 — XPU CPU lane の snapshot clone 部分失敗を縮退扱いにする

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、XPU mixed CPU worker 初期化。
- **確認できた事実:** CPU composition snapshot の作成ループは一つでも clone に失敗すると `mixedCpuReady=false` にし、すでに正常作成した snapshot がある場合も mixed dispatch を有効にしなかった。
- **今回の対応:** 失敗を警告して clone ループを止め、作成済み snapshot が一つ以上あればその数だけ CPU lane を登録する。CPU lane が0ならGPU側の既存dispatchへ縮退する。
- **価値／懸念:** 一時的なclone失敗で既に確保したCPU資源を全て失わず、実際に初期化できたlane数を超えて起動しない。
- **次に確認:** clone failure を注入し、要求／実稼働 lane 数、順序復元、フレーム成功数が縮退後の構成と一致することをruntimeで確認する。ビルド・テストはユーザー明示指示待ち。

### 2026-10-10 — Kuwahara の近傍統計を一走査で集計する

- **関連:** `Artifact/src/Effects/Kuwahara/KuwaharaEffect.cppm`。
- **確認できた事実:** CPU と HLSL の両方で、各 quadrant の平均用と分散用に同じ近傍画素を別々に走査・読込していた。GPU shader は1画素につき4 quadrantを処理する。
- **今回の対応:** RGBの二乗和も平均と同時に集計し、`E[x²] - E[x]²` から分散を得る形へ変更。CPU と HLSL を揃えた。負の丸め誤差は0へクランプする。CPU 側のquadrant loopは0／負のoffsetで終端へ進まず走査が継続する向きだったため、GPU と同じ画素範囲を進むよう修正。
- **価値／懸念:** 近傍テクスチャ読込・CPU画素読込を理論上およそ半減できる。二乗和方式は従来の差分二乗方式と浮動小数点丸めが異なり、HDR値やほぼ一様な領域では分散選択に差が出る可能性がある。速度と画像差は未検証。
- **次に確認:** ビルド後、CPU/GPU pixel parity と半径別の実行時間を比較する。ビルド・テストは明示指示待ち。

## 現在の優先検証

### 2026-10-10 — ArtifactHashMap::operator[] が既存値を黙って消していた

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`ArtifactCore/src/Composition/CompositionRegistry.cppm`、`tests/ArtifactCore/CompositionTransformTimeContractTest.cpp`。
- **確認できた事実:** `operator[]` が `tryEmplace(key)`（空パックの可変長 overload）に転送され、キー存在時に `curr->data.second = V()` でデフォルト上書きして返していた。`NameMap::operator[]` 経由の読み取りは全て破壊読みだった。実害：`CompositionRegistry::findComposition()` は `contains()` が真でも `entries[key]` で nullptr に潰して返していた（テストで再現・確定）。script host 用の名前解決が全滅していたことになる。
- **今回の対応:** 存在確認→既存参照返却、不存在時のみ `tryEmplace(key, V())` で挿入に修正。単引数 `tryEmplace` の他利用者はなし。回帰用に NameMap 直接試験を残した。
- **価値／懸念:** 全 NameMap/HashMap 利用者の読み取りが正しくなる。逆に今まで「読むと消える」挙動に依存していたコードがあれば顕在化する。既存ビルド済み 14 件は全パス。
- **次に確認:** フルスイート（ArtifactApp 含む）のビルド・実行は未実施。`ArtifactProjectRoundTripTest`（tests/Artifact、ArtifactAppRuntime 要）は未ビルド。

### 2026-10-10 — RenderGraph に executionLevels と executeParallel を追加した

- **関連:** `ArtifactCore/include/Graphics/RenderGraph.ixx`、`tests/ArtifactCore/RenderGraphTest.cpp`。
- **確認できた事実:** `compile()` の Kahn 順序はレベル非減少なので、伝播した level 配列でグルーピングすると並列実行可能なバッチになる。既存 `execute()` の逐次語義は不変。`allocationSlot` / `lifetimes` は算出のみで backend の消費者がいない（未検証ではなく grep で確認済み）。
- **今回の対応:** `CompiledRenderGraph::executionLevels` を追加し、ダックタイピングの launcher（`async(F)->future<bool>`、`Core.TaskSystem` が適合、RenderGraph 側に新規依存なし）でレベル同期実行する `executeParallel` を実装。単一パスレベルはインライン実行で launcher に触らない。単体テスト 15 件（順序・cycle 到達不能ではなく除外・alias 共有/分離・失敗伝播・診断）を追加。cycle は宣言順 append の API では到達不能なためテスト対象外とした。
- **価値／懸念:** 現行グラフは全て線形チェーンのため、フレームパスへの配線は効果ゼロ＋リスクのみ。配線は分岐グラフが出てから。真の並列レンダリングにはパケット構築自体のスレッド分割が別途必要。
- **次に確認:** ユーザーの明示指示後、テストのビルド・実行で検証する。ビルド・テストは明示指示待ち。

### 2026-10-10 — 診断グラフへ層 raster/mask/blend チェーンを載せた

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`frameDebugSnapshot` の diagnosticGraph）。
- **確認できた事実:** 診断スナップショットのグラフは層ごとの raster が単一 `layerResource` への書き込み羅列＋単一 blend で、マスク段が存在しなかった。実行は変えず記述だけ変える余地がある（診断専用のため）。
- **今回の対応:** 層ごとに raster→mask→blend のチェーンを登録。raster は共有 scratch への書き込み（WAW 辺で直列化され実実行と一致）、mask は enabled matte 参照がある層のみ、`blend` は層ごとに accum 共有トークンへ。`FunctionalRenderPass` 実体は作らず記述子のみで、per-frame の `std::function` 確保は増やさない。空 comp 時は accum 書込維持の Empty blend を残し、下流 effect パスの read-before-write 失敗を避けた。
- **価値／懸念:** FramePipelineViewWidget で層・マスクの有無が可視化され、カリング漏れが目視できる。実行順序・性能は不変。
- **次に確認:** ユーザーの明示指示後、診断スナップショットの表示確認とテストのビルド・実行。ビルド・テストは明示指示待ち。

### 2026-10-10 — Layer2D は未定義メソッドのためテストから除外した

- **関連:** `ArtifactCore/include/Transform/StaticTransform2D.ixx`、`ArtifactCore/src/Layer/Layer2D.cppm`、`tests/ArtifactCore/CompositionTransformTimeContractTest.cpp`。
- **確認できた事実:** `StaticTransform2D::toQTransform()` と `setTransform2D()` は宣言のみで定義が codebase に存在しない（未検証ではなく grep で確認済み）。`Layer2D::transformedLayer()` は `toQTransform()` を呼ぶ。function-level linking（`/Gy`）は未設定のため、Layer2D の any symbol 参照は static lib の object 全体を引き込み LNK2019 になる。既存テストも Layer2D を参照していない。
- **今回の対応:** 新規 Composition/Transform/Time テストでは Layer2D 本体を外し、StaticTransform2D の定義済みメソッド・Registry・RationalTime・TimeRemap・MatteMode 契約のみにした。除外理由はテスト内コメントに記載。
- **価値／懸念:** Layer2D 所有権テストは定義追加後にしか書けない。本番側も Layer2D.cppm の symbol を参照した瞬間に同じリンク破損が出る。
- **次に確認:** ユーザー承認後に `toQTransform()`（回転・拡縮・アンカー・平行移動の合成順序）と `setTransform2D()`（リセット語義）の定義を追加し、Layer2D 所有権テストを復活させる。ビルド・テストは明示指示待ち。

### 2026-10-10 — Render Manager の visual fixture と queue restore 契約

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm::fromJson`、`Artifact/src/AppMain.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** `fromJson()` は保存済み queue の `Completed` / `Failed` 状態を読み込んだ後、復元した全 job を `Pending`・progress 0 に戻す。失敗の `errorMessage` は保持される。これは実行中状態を安全に復旧しないための既存動作である。
- **今回の対応:** Render Manager の visual fixture は実プロジェクトと composition を作り、待機ジョブ2件と attention 表示用 error 付きジョブ1件を使用する。`fromJson()` の復元安全性を変えずに選択・詳細・status 表示を撮影できる。
- **価値／懸念:** 画像比較が実際の composition/preflight 表示を通り、AppData queue の残存値にも依存しない。Completed / active progress の行表示はこのfixtureでカバーしない。
- **次に確認:** interaction testで完了・進行中の表示状態が必要になった時、永続化の復旧契約を弱めず、テスト専用の状態注入境界で表現できるか検討する。

### 2026-10-10 — Render Manager UI のロケール別翻訳カバレッジ

- **関連:** `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactRenderQueuePresentation.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** `ArtifactUiTest --lang ja/en/zh/zh-TW` で撮影した同一 fixture の PNG は4言語とも同一 SHA-256 だった。現時点では Render Manager の表示がロケール変更を反映していない。
- **価値／懸念:** 言語別 UI テストを入れることで、ロケール起動の回帰と翻訳の未接続を区別して把握できる。モックは英語のみのため、他言語は英語モックとの差分で評価できない。
- **次に確認:** ユーザーが翻訳対応を依頼した段階で、Render Manager の静的ラベル・状態・ボタン文字列の翻訳経路を調査し、翻訳済み UI に同一言語の基準画像を用意する。

### 2026-10-10 — 3D粒子contractをoffline-render optionから単独登録する

- **関連:** `tests/Artifact/CMakeLists.txt`、`ARTIFACT_ENABLE_OFFLINE_RENDER_TEST`、`ArtifactOfflineRendererContractTest`。
- **確認できた事実:** Artifactのoffline renderer contract targetは `ARTIFACT_BUILD_TESTS` または `ARTIFACT_ENABLE_OFFLINE_RENDER_TEST` で登録される。初期の3D粒子contract登録は `ARTIFACT_BUILD_TESTS` だけに結び付いていた。
- **今回の対応:** 3D particle contractとproduction-layer captureを、既存offline-render optionからも有効にし、READMEに必要なGTest/GPU条件を記載する。
- **価値／懸念:** 全unit suiteを有効化せずに3D headless rendering gateを選択できる。CMake再構成は禁止中のため、登録条件はソース確認のみ。
- **次に確認:** ユーザーの明示指示後、offline-render option単独のconfigureと対象CTest登録を確認する。

### 2026-10-10 — headless particle drawはPSOを同期準備する

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm::drawParticles`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm::ensureGraphicsPipeline/prepare`、`tests/Artifact/Particle3DRenderContractTest.cpp`。
- **確認できた事実:** offline rendererは `m_offlineWidth > 0` の間、particle draw時に `ensureGraphicsPipeline(data.options)` を呼ぶ。未キャッシュPSOはこのheadless経路で同期作成される一方、interactive経路は非同期準備を利用できる。
- **価値／懸念:** 独立headless testで最初のparticle frameが非同期PSO compile raceに負ける懸念を抑えられる。この前提はoffline draw分岐に依存する。
- **次に確認:** offline rendererの初期化・draw経路を変更した場合、同期PSO準備が保たれるか再確認し、外れるならtest fixtureで明示prewarmまたは完了待ちを行う。

### 2026-10-10 — 共有 renderer の粒子テストではカメラ状態を明示的に戻す

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ArtifactIRenderer::reset3DCameraMatrices()`。
- **確認できた事実:** 粒子 contract tests は同じ headless renderer を共有する。3D camera/model test と depth test は `set3DCameraMatrices()` を呼ぶ一方、後続の2D粒子ケースはカメラを戻しておらず、3D状態を残したまま描画する可能性があった。
- **今回の対応:** 2Dケースの描画前に3D camera matricesをresetし、描画後に `cameraMode=2d` と `depthTest=0` / `depthWrite=0` をassertし、3D depth casesでは送信完了後に期待するdepth flagsをassertする。3D camera/model移動、透視footprint、VelocityAligned、depth-order各caseでも `cameraMode=3d` を明示assertする。depthTest/depthWrite診断はpacket送信完了後に読む。readback画像の寸法が96×96でない場合はpixel走査へ進まず失敗させる。
- **価値／懸念:** 2D粒子の no-depth regression check が、テスト実行順やrenderer状態の偶然に依存しにくくなる。今回のAGENTS.md制約により、この差分のビルド・実行は未確認。
- **次に確認:** ユーザーがビルド・テスト実行を許可した後、3D contract suiteを複数回実行し、全ケースが実行され2D診断が成立することを確認する。

### 2026-10-10 — 3D粒子のheadless runnerはQt platformもoffscreenに固定する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`tools/Particle3DPlayground/main.cpp`。
- **確認できた事実:** 3D contract testとproduction-layer captureはいずれも `QGuiApplication` を生成する。既存のheadless renderer contract testでは同じGPU作業前に `offscreen` を設定している。
- **今回の対応:** GTest global environmentとcapture executableの両方で、`QGuiApplication` 構築前にQt offscreen platformを指定する。
- **価値／懸念:** GPU backendが利用できてもdisplay serverがない環境でQt初期化に失敗する可能性を下げる。Qt offscreen pluginがないホストでの実行は未確認。
- **次に確認:** Linux/Vulkanのdisplayなし環境とWindows/Vulkanでcapture executableを実行し、Qt初期化とDiligent device初期化の失敗を区別して確認する。

### 2026-10-10 — image-only particle runnerはQGuiApplicationに留める

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`tools/Particle3DPlayground/main.cpp`。
- **確認できた事実:** 3D contractとcapture runnerは画像・GPU描画のみを使い、QWidget APIを参照しない。既存の `ArtifactOfflineRendererContractTest` はheadless readbackに `QGuiApplication` を使っている。
- **今回の対応:** 3D test environmentとcapture mainを `QGuiApplication` に変更し、Widgets applicationの初期化を持ち込まない。
- **価値／懸念:** GUI-less GPU render testの初期化責務が実利用APIと揃う。Qt offscreen pluginがない環境については未確認。
- **次に確認:** headless test executableをdisplay serverなしで起動し、Qt GUI platformとDiligent device初期化の両方を確認する。

### 2026-10-10 — 3D particle seek回帰にもフレーム再訪を含める

- **関連:** `tools/Particle3DPlayground/main.cpp`、2D particle firework capture。
- **確認できた事実:** 2D firework captureは連続再生、直接seek、60→10→45再訪の画像を比較している。3D production captureは連続再生と直接seekだけを比較し、同一layerの非単調なframe移動は未検査だった。`ArtifactParticleLayer::draw()` は `simulationReusable` を保持し、`ParticleSystem::goToFrame()` は同一fps・未来フレーム・120Hz完全step境界でのみsimulationを再利用する。60→10はresetし、その後10→45は条件が合えば再利用経路を通る。
- **今回の対応:** production `ArtifactParticle3DLayer` の別instanceで60→10→45を描画し、frame45 PNGとpixel/channel差をレポート・許容値判定へ加える。
- **価値／懸念:** 履歴依存の状態更新が非単調seek後に再現性を壊す回帰を検出できる。GPU runtimeでの再現性は未確認。
- **次に確認:** VulkanとD3D12で再訪画像が連続再生frame45と決定論的許容差内に収まるか実行する。
- **追補:** 再訪のframe 60と10それぞれで、生存粒子数・`state=queued`・`cameraMode=3d`も確認するようにし、途中のGPU draw不成立を識別する。

### 2026-10-10 — Particle depthWrite=falseを後続geometryで検査する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticlePkt` 深度attachment、3D card PSO。
- **確認できた事実:** 既存の前後関係テストは粒子のdepthTestを確認し、production captureはdepthWrite=falseの設定で手前カードによる遮蔽を見る。ただし、粒子がdepth bufferへ書き込まず、後続のgeometryが粒子より奥でも描画できることは独立に確認していなかった。
- **今回の対応:** `depthTest=true, depthWrite=false` の前景particleを描画・readbackした後、depthWrite=trueの後景cardを描画する。最初の画像で粒子色、最終画像でcard色を別々にassertする。
- **価値／懸念:** 粒子PSOのDepthWriteEnable契約と後続3D geometryとの相互作用を画像で検査する。実GPUでのpixel結果は未確認。
- **次に確認:** Vulkan/D3D12で前景particleが先に出て、後景cardが最終pixelを覆うことを実行確認する。
- **追補:** 深度helperは各readback画像の寸法を確認してから中心pixelを読む。GPU初期化後のreadback失敗はzero pixelのassertion failureとして扱い、invalid imageへ直接アクセスしない。

### 2026-10-10 — VelocityAlignedの速度方向を画像bboxで検査する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticleRenderer` vertex shader、3D particle milestone P4-4。
- **確認できた事実:** shaderは速度をview spaceへ変換し、VelocityAligned policyのとき画面上速度方向から回転角を加える。円形particleでは回転差が見えないため、stretch付き粒子を使えば姿勢を画像bboxで区別できる。
- **今回の対応:** 正面カメラでX速度/Y速度を、傾斜カメラでワールドX速度をそれぞれGPU readbackし、cameraMode=3dを診断確認したうえで、正面Xは縦長・正面Yは横長・傾斜Xは投影された画面速度に沿う横長bboxとなることをcontract化する。深度test/writeを明示的に無効化し、全caseのGPU position/velocity xyzも明示初期化する。
- **価値／懸念:** マイルストーンに残るVelocityAlignedのruntime gateを自動画像テストへ取り込む。Vulkan/D3D12上の実bboxは未確認。
- **次に確認:** 正面と傾斜カメラの全3画像で粒子pixelが存在し、縦横bboxが期待どおりになることを各backendで実行確認する。

### 2026-10-10 — 3D粒子のモデルZ移動は透視投影サイズで検査する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticleRenderer.cppm` のvertex shader。
- **確認できた事実:** Particle vertex shaderはrow-majorのModelRowを使って粒子位置をmodel→viewへ変換し、screen-aligned billboard offsetをview-spaceへ加えてからprojectionする。テストのmodelMatrix[3]/[11]はX/Zの行translationに対応する。従って、同じ小粒子をカメラ方向へZ移動すると画面上のfootprintが増えることを、GPU readbackで契約化できる。
- **今回の対応:** 独立3D contractに、model Z=0とZ=1の画素数比較を追加。既存のX移動テストを保ち、Z移動と粒径をhelper引数で指定できるようにする。
- **価値／懸念:** 3Dテストが画面上の平行移動だけでなく奥行きと透視スケールも検査する。実GPU上での差分安定性は未確認。
- **次に確認:** VulkanとD3D12でnear/far pixel countの両方が非ゼロで、nearがfarを上回ることを実行確認する。

### 2026-10-10 — 3D particle contractもGPU位置xyzを明示初期化する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticleVertex::px/py/pz`。
- **確認できた事実:** `ParticleVertex` のposition成分はdefault member initializerを持たない。2D particle GPU画像テストの調査で、未設定の `pz` がGPU射影後の描画欠落を起こした実例がある。
- **今回の対応:** 3D移動helperとdepth-order helperで `px/py/pz` をそれぞれゼロに明示設定し、Z変化はmodel matrixだけから与える。
- **価値／懸念:** vector element constructionの値初期化規則や将来の生成方法変更に依存せず、テストの意図する座標が固定される。
- **次に確認:** GPU実行時に3D移動・透視footprint・深度ケースが引き続き粒子を描画することを確認する。

### 2026-10-09 — ParticleLayerのGPU画像出力とflipbook経路

- **関連:** `tools/Particle2DPlayground/main.cpp`、`Artifact/src/Layer/ArtifactParticleLayer.cppm`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`。
- **確認できた事実:** NVIDIA RTX 4070 Ti / Vulkan の headless readbackで矩形8836画素、明示初期化済みの粒子44324画素を確認した。先行テストの粒子は `ParticleVertex` の `pz` を設定しておらず、射影後Zが不定だった。テスト側で値初期化し `pz=0` にするとproduction shaderのまま表示されたため、ParticleRendererの射影修正は不要だった。Emitter `texturePath` は連番ディレクトリとsprite sheetの両方を既存software rendererが読めるが、通常GPU shaderには画像SRVがない。
- **今回の対応:** ArtifactParticleLayerは画像ソース付き2D emitterを既存のcached-image経路へ送る。通常ビューはQImage spriteをGPU rendererへ渡し、GPU offscreen surfaceは既存呼出側のsoftware fallbackへ戻す。連番と4×4 sheetをGPU render target経由で自動キャプチャし、各5フレーム、双方の動き、および画素差8以下／channel差2以下を検査。確認結果は5時刻中4時刻が完全一致、残りは1画素・最大channel差1。
- **価値／懸念:** 画像粒子は通常Viewportでも表示可能になった。texture sampling自体はGPU ParticleRendererに追加していないため、画像付き大量粒子ではsoftware rasterと動的sprite uploadのコストがかかる。GPU native texture samplingへの置換は未検証。
- **次に確認:** 実際のViewport上で連番／sheetの見え方と多数粒子時のframe costを確認し、必要なら既存texture cacheを使うGPU sprite pathの設計を比較する。

### 2026-10-09 — 風の動きは葉プリセットとEmitter物理値で独立確認できる

- **関連:** `tools/Particle2DPlayground/main.cpp`、`ParticlePresets::leaves()`、`ArtifactParticleLayer::setLayerPropertyValue()`。
- **確認できた事実:** 葉プリセットは連続発生・回転をすでに持ち、公開 property path から wind direction／strength、turbulence amplitude／frequency／evolution、drag を設定できる。Playground の自動キャプチャで30〜150フレームの5枚を保存し、先頭と末尾で7386画素が異なることを確認した。
- **価値／懸念:** 花びらのスプライトアニメとは異なる、Emitter物理による軌道変化を小さなオフライン例で確認できる。PNGは現状、葉テクスチャではなく既存の円形particle描画である。GPU経路での同じ動きとスプライト表示は未検証。
- **次に確認:** GPU粒子画素の欠落原因を特定した後、連番画像を載せた葉のWind/Turbulence表示をGPU／software両経路で比較する。

### 2026-10-09 — ACEScc zero encode/decode の逆変換不一致

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToACEScc` / `acesccToLinear`、`tests/ArtifactCore/ColorBridgeTest.cpp`。
- **確認できた事実:** `encode(0, ACEScc)` は定数 `-0.3584474886f` を返すが、その値を `decode` へ渡すと約 `131072` になる。非ゼロの密なscene/HDR格子ではこの zero sentinel を除外すると往復を検査できる。ArtifactCore は子リポジトリなので本作業では実装を変更していない。
- **価値／懸念:** ACEScc の黒コードが公開decode式と逆変換になっておらず、黒を含む encode→decode 処理は大きな正値を生成する可能性がある。定数の丸め誤差が指数で増幅されている可能性はあるが、原因と規格上の期待値は未検証。
- **次に確認:** ACES ST 2065-4 の負値／black code 規約と定数精度を一次資料で照合し、ユーザーが ArtifactCore 修正を依頼した場合に専用の黒境界回帰テストと最小修正を行う。

### 2026-10-09 — Rec.709 OETF/EOTF 折れ点の非整合

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToRec709` / `rec709ToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 独立した倍精度参照と全1,024個の正規化コード値を照合すると曲線本体は約4e-7以内だった。一方、OETF の線形折れ点 `0.018` を高側式へ入れた値は約 `0.08124793` だが EOTF は `0.081` を境界としており、境界ちょうどで `0.017945` 付近へ戻る。両端の折れ点が一致していない。Rec.2020 の同じ全コード比較は約4e-7以内で通る。
- **価値／懸念:** Rec.709 のブレークポイント近傍に明確な段差・往復誤差がある。原因は実装にあるしきい値 `0.081` と OETF の係数から生じる `0.0812479...` の差と見られるが、採用する規格定数と許容誤差は未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** 参照する BT.709 版と規格上の breakpoint 定義を一次資料で確認し、ユーザーが ArtifactCore 修正を明示した場合に OETF/EOTF 境界の最小修正と回帰テストを行う。

### 2026-10-09 — sRGB OETF breakpoint直後の微小な下向き段差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToSRGB`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** `0.0031308f` とその次の表現可能floatを別々の枝へ通すと、OETF出力が約 `0.040449936` から `0.040449910` へ約 `2.7e-8` 下がる。倍精度の区分式参照との比較でもこの値を再現し、隣接float専用テストに現在の段差をcharacterizationした。
- **価値／懸念:** 数値としてはごく小さいが、OETFの厳密な単調性を破る。丸められたbreakpointと係数による既知の標準近似として許容するか、分岐点／係数を調整すべきかは未検証。ArtifactCore は子リポジトリなので本作業では実装を変更していない。
- **次に確認:** IEC 61966-2-1 の採用定数と出力精度要求を一次資料で確認し、ユーザーがArtifactCore修正を明示した場合に最小の境界修正と回帰テストを行う。

### 2026-10-09 — PQ float 実装の全10bitコード精度

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToPQ` / `pqToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** SMPTE ST 2084 の独立倍精度式に対し、正規化された全1,024コード値の OETF/EOTF を比較した。現在の float 実装は最大絶対差が OETF 約 `9.94e-6`（code 274）、EOTF 約 `3.85e-5`（code 1022）。テストではこの観測差を少し上回る `1.1e-5` / `4.0e-5` を回帰上限としている。
- **価値／懸念:** PQ EOTF の高コード側で線形値の差が OETF より増幅される。これが許容可能な float 実装誤差か、要求精度に対して不足かは未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** 用途別のPQ精度要求と参照規格の試験点を確認し、ユーザーが ArtifactCore 修正を明示した場合に係数・演算精度の最小変更を検討する。

### 2026-10-09 — Canon Log 3 low toe 境界の段差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`linearToCanonLog3` / `canonLog3ToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** low encoded threshold `0.04076162` から計算した線形 low toe 境界の直下では OETF が約 `0.04076`、境界ちょうどでは middle affine branch に入り約 `0.04700` を返す。EOTF も encoded threshold の直下で約 `-0.0113`、threshold ちょうどで middle branch の約 `-0.0140` に切り替わる。独立テストに全10bitコードの照合と、この low toe の枝切替を固定する characterization を追加した。
- **価値／懸念:** Canon Log 3 low toe の encode/decode に目立つ不連続がある。式の係数・境界の取り違えか、負値域の設計仕様かは未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** Canon Log 3 の公開仕様にある low-end toe と負値範囲を一次資料で照合し、ユーザーが ArtifactCore 修正を明示した場合に最小修正と回帰テストを行う。

### 2026-10-09 — Canon Log 2 負側 toe 境界の OETF 段差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`linearToCanonLog2`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 負側 toe 境界 `-(10^(toe/slope)-1)/scale` の直下は負値用式で 0 付近を返し、境界ちょうどは通常式 `slope*log10(linear*scale+1)+toe` に切り替わって約 `-0.0146` になる。回帰テストでは両枝の値と段差を characterise した。Cineon の black code は式上 `95/1023` と確認した。
- **価値／懸念:** Canon Log 2 の負値 toe に不連続がある。正値域の通常コードでは目立たない一方、負値・scene-linear入力を通すと境界に段差を生む。負値処理が規格要件かは未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** Canon Log 2 の公開仕様で負の scene-linear 値と toe 境界の規約を確認し、ユーザーが ArtifactCore 修正を明示した場合に最小修正する。

### 2026-10-09 — ACESlog enum の汎用変換 dispatch 未接続

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`TransferFunction::ACESlog`、`ColorTransferFunction::encode/decode`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** `TransferFunction` enum に `ACESlog` があるが、汎用 encode/decode の switch に専用 case はなく default の linear identity を返す。独立テストに現在の identity fallback を固定した。ACESlog の専用 OETF/EOTF を提供する実装はこのインターフェースで確認できなかった。
- **価値／懸念:** UI や呼び出し側が enum を選んでも ACESlog 変換は行われず、入力値がそのまま返る。これは未実装か意図的 fallback か未検証であり、テストは現状の dispatch を characterization している。
- **次に確認:** ACESlog の対象規格・必要な用途と UI 露出有無を確認し、ユーザーが ArtifactCore 機能追加を明示した場合に専用変換と独立参照テストを追加する。

### 2026-10-09 — XYZ_D60 gamut enum が Bradford dispatch に未接続

- **関連:** `ArtifactCore/include/Color/ColorGamutConversion.ixx`、`Gamut::XYZ_D60` / `getConversionMatrix()`、`tests/ArtifactCore/ColorGamutConversionTest.cpp`。
- **確認できた事実:** `XYZ_D60` と `XYZ_D65` の両方向変換は `getConversionMatrix()` で identity となる。Bradford 判定が `ACES_AP0` / `ACES_AP1` のみを D60 source/target として扱い、`XYZ_D60` は含めていない。D60白 `(.9526461, 1, 1.0088252)` は D65へ変換しても不変で、D65白から逆方向も変化しない。独立テストに現行 fallback を characterization として追加した。
- **価値／懸念:** 色域 enum の名前と実際の white-point routing が一致せず、D60↔D65 XYZ 変換ではBradford adaptationが省略される。ArtifactCore は子リポジトリのためこの作業では実装を変更していない。
- **次に確認:** `XYZ_D60` を単なるXYZ座標系タグとして扱う設計か、D60 white-point adaptation 対象として扱うべきかを仕様確認し、ユーザーが ArtifactCore 修正を明示した場合に最小 dispatch 修正を行う。

### 2026-10-09 — ArtifactUiTest の QSettings 分離境界

- **関連:** `Artifact/src/AppMain.cppm`、Artifact 内の `QSettings(organization, application)` 呼び出し。
- **確認済み:** Qt の公式 API 仕様では `setDefaultFormat()` は既定コンストラクタにのみ効き、組織名・アプリ名を明示したコンストラクタは `NativeFormat` を使う。Windows では `NativeFormat` の `setPath()` も効かない。UI Test 起動の `QStandardPaths::setTestModeEnabled(true)` は AppData／AppConfig 等の標準書込先を test 領域へ変えるが、明示的な QSettings コンストラクタの Windows Registry 保存先は分離しない。
- **価値／懸念:** 現状の UI Test exe は実画面を独立起動できる一方、これらの明示設定を読む／書く画面は通常版と同じ Registry 設定を共有する。完全な設定分離を行うなら、QSettings 呼出側の共通 factory 化などが必要で、起動処理だけの変更では実現できない。
- **次に確認:** UI テストで通常版の既存設定を読む・変更することが許容されるかを決め、完全分離が要件なら明示コンストラクタ呼出箇所を一括で扱う小さな設定アダプタの設計を検討する。

### 2026-10-09 — Filmstrip 未取得フレーム生成の実行経路

- **関連:** `Artifact/src/Widgets/ArtifactFilmstripWidget.cppm`、`Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm`、`Artifact/src/Service/ArtifactPlaybackService.cppm`。
- **確認済み:** Filmstrip は RAM キャッシュの縮小表示とパネル内の onion skin／比較を提供する。既存の offscreen renderer の `renderFrame(position, composition)` は position をレイヤーの active 判定に使うが、各レイヤーを指定フレームへ評価していない。共有レイヤーを描画するため、そのままワーカーへ渡す根拠もない。Playback Service の build queue は単一で、Filmstrip が置き換えると他のプレビュー要求に干渉する。
- **未検証の案:** 未取得分の独立生成には、既存 GPU submission の所有経路で固定上限の要求を処理し、composition revision と表示世代で古い結果を破棄する仕組みを検討する。
- **次の確認:** 共有レイヤー評価状態と immediate context の所有者を確認し、表示中のフレームを変更せずに生成できる経路を選ぶ。

### 粒子 — GPU第一経路化・非同期PSO・ディスクPSOキャッシュの実機確認

- **関連:** `ArtifactCore/src/Graphics/ParticleRenderer.cppm`（非同期ワーカー・PSOキャッシュ・ディスクregistry）、`Artifact/src/Layer/ArtifactParticleLayer.cppm`（trail送出・GPU面）、`Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **確認すること:** 層追加の固まり解消、初回CPU→GPU切替の継ぎ目、trail／stretch／エフェクト付き表示、2回目起動での待機診断消失とキャッシュファイル生成、Vulkanでの96バイト stride。

### 2026-10-09 — 粒子キャプチャはレイヤーのフレーム同期経路を通す

- **関連:** `tools/Particle2DPlayground/main.cpp`、`Artifact/src/Layer/ArtifactParticleLayer.cppm`、`Artifact/src/Generator/ArtifactParticleGenerator.cppm`。
- **確認済み:** `renderFrame()` に大きな時間差を直接渡しただけでは burst が出ず、空PNGになった。`ArtifactParticleLayer::draw()` は `goToFrame()` で決定論的な粒子状態を同期してからソフト描画へ進む。Playground の自動キャプチャもこの既存経路を通すと粒子があるPNGを出力できた。
- **価値／懸念:** 自動画像テストが空画像を成功扱いしないよう、粒子数・保存成否を検証する必要がある。現在の `--capture-firework` はソフト描画経路のみで、GPU readback と連続再生／seek間の画素一致は未検証。
- **次に確認:** 同じ burst を GPU と software で保存し、連続再生・直接seek・巻き戻し後の同一フレーム画像を比較する。

### 静止画・連番画像 — GPU cache と実素材の再生／出力確認

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm`、`Artifact/src/Render/GPUTextureCacheManager.cppm`。
- **状態:** 実装済み、runtime未検証。
- **確認すること:** 4K連番、欠番、Time Remap、再リンク、Preview／Render Queueでフレーム・GPUメモリ・出力が一致すること。

### Shape — Path keyframe／Merge Paths／SVG出力の実機確認

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/include/Layer/ArtifactShapeLayer.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **状態:** GPU／互換フォールバック／boundsの同期は実装済み。SVGのグラデーション、stroke taper／alignは未対応。
- **確認すること:** 複数subpathと各Merge Paths mode、Path keyframe、GPU／出力SVGの一致。

### Shape — 複数コンテンツ／GPUベクター描画の実機確認（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`ShapeContent`、`paintGpuPaintItems`、`resolveContentVisPaths`、`renderContentsToImage`）、`Artifact/include/Layer/ArtifactShapeLayer.ixx`。
- **状態:** 実装済み、ビルド・runtime未検証（ビルドは指示待ちのため未実行）。
- **内容:** 1レイヤー複数パス（形状＋塗り＋線＋表示＋結合モード、空＝従来動作）。グラデーションは三角形重心サンプリング、Inside／Outsideはハーフオフセット＋中央線、テーパー／勾配線はセグメント分割でGPU描画し、viewportのQImageスプライト分岐を撤去。結合はCPU側QPainterPath真偽値演算で解決し、スタイルは保持。物理グリッド・3Dカード高速パス・オペレータキー評価は従来のまま。
- **確認すること:** 既存単一シェイプの見た目不変（solid高速パス・operator分岐は温存）、グラデーション／align／taperのGPU描画、Subtract／Intersect／Differenceの穴・境界線、連番・サムネイル・SVG出力、Repeater大量複製時の負荷。
- **既知の近似（未検証の仮説ではない仕様）:** テーパー線の結合部は Butt 重ね、Roundキャップは矩形延長近似、dash＋taper併用はdash優先、コンテンツのパス頂点アニメは未対応（静的）。

### Shape — 沿路グラデーション線／ダッシュオフセット（2026-09-03）

- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`（`PolylineStyle`）、`Artifact/src/Render/ArtifactIRenderer.cppm`（`drawStyledPolyline`）、`Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** `PolylineStyle`に`gradientEnabled/gradientStart/gradientEnd/dashOffset`を追加。`drawStyledPolyline`は累積長パラメータでセグメント・ダッシュ・結合・キャップを沿路補間色で描画し、dash位相は`dashOffset`の剰余で解決。レイヤー側は`shape.dashOffset`（アニメ可）＋コンテンツ別`dashOffset`、勾配のみの線は taper 分割器ではなく`drawStyledPolyline`経由に変更（結合・キャップ・dashと合成可）。QImage互換・SVG出力（`dashOffset`のみ）・保存も配線。
- **確認すること:** 既存実線の見た目不変（新フィールド既定で旧経路と同一）、勾配＋dash＋round結合の合成、負offset・巨大offset、マーチングアンツのキーフレーム補間。
- **既知の近似:** taper＋dash併用はdash優先でtaper無効、勾配サンプリングは線形補間。

### Shape — SVG相互運用（取込・書出）（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`SvgImport`、`shapeContentsToSvg`、`parseShapeContentsFromSvg`、`addShapeContentsFromSvg`、`importSvgFileContents`）。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** 書出は結合解決済みパス＋塗り／線／dash／fill-ruleを`<path>`＋`linear/radialGradient` defsで出力（taper線→通常線、conical→単色、勾配線→中間色に縮退）。取込は`path(d全命令・Aはベジェ化)`・rect（角丸可）・circle・ellipse・polygon・polyline・line＋線形／円形グラデーション（前方参照可）＋transform bake＋継承スタイルを編集可能コンテンツ化（座標はbounds正規化、256件cap、64MB cap）。`ClipboardManager`は未変更で、受渡し自体は素のSVGテキストを呼出側に委譲。
- **確認すること:** Illustrator／Figma出力SVGの往復、userSpace勾配・奇数dash・相対命令・指数表記の数値、空・不正SVG（0件／-1）、既存JSON互換。
- **既知の近似:** 複数subpathは1要素に統合（線描画で連結線が出る）、3 stops以上は両端のみ、gradientTransform・非等方scale下の線幅・group fill-opacity継承は近似、strokeのurl()は勾配線として解決（fillのみ前方参照対応だった点をstore側で統一）。

### Shape — コンテンツ編集サポート（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`activeContentIndex_`、`ShapeContentProxy`、`duplicateShapeContent`、`moveShapeContent`、`insertShapeContent`、`swapShapeContents`）、`Artifact/include/Layer/ArtifactShapeLayer.ixx`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** `activeContentIndex_`（-1 = レガシーモード）と`ShapeContentProxy`（`ArtifactShapeLayer*` + index）を導入。Proxyは`name`/`visible`/`opacity`/`merge`/`fill`/`stroke`/`geometry`/`duplicate`を`setShapeContentAt`経由で直接編集し、PropertyEditorは`shape.activeContentIndex`で操作対象を切り替える。複製（挿入位置にコピー）、挿入、`move`、`swap` APIを追加。`shape.content.<i>.type/width/height/cornerRadius/starPoints/starInnerRadius/polygonSides/fillRule` を `setLayerPropertyValue` で直接編集可能に拡張。JSONシリアライズに`activeContentIndex`を含む。
- **確認すること:** Proxyのスワイプ（他のインデックス参照）、move/swap後のbounds・visPaths再構築、JSON往復、PropertyEditorでのアクティブコンテンツ切り替え時の描画反映、contentジオメトリ編集時の再構築・保存。

### 2.5D — 局所DOF／motion blurの品質と負荷

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Layer/Artifact{Image,Shape,Text}Layer.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **状態:** 実装済み、runtime未検証。
- **確認すること:** Image／Shape／Textで深度・focus・shutterを変え、alpha順、極端なblur、再生時のGPU負荷、書き出し結果を確認する。

### Gobo runtime texture — 将来接続の安定ID境界

- **関連:** `ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`。
- **状態:** ファイルGoboを置換できるruntime SRV入力は実装済み。Image Layerとの接続・UI・保存は未実装。
- **判断待ち:** 接続を始める時点で、packed scene-light slotではなく安定したLight IDとresource revisionを対応付ける。

## 現在の設計判断

### Semantic Debugger — 外部 `ArtifactDebugger.exe` を正規UI境界とする

- **関連:** `docs/planned/MILESTONE_EXTERNAL_SEMANTIC_DEBUGGER_2026-09-02.md`、既存のMCP／Trace／Shared Memory IPC診断基盤。
- **状態:** 未着手。設計判断をマイルストーン化。
- **判断:** ArtifactStudio本体には低コストの `ArtifactDebugRuntime`（semantic identity、mutation provenance、frame snapshot、safe-point制御）だけを置き、意味表示・原因解析・timeline・semantic breakpointのUIは外部プロセスへ分離する。既存MCPはAI専用に作り直さず、Debuggerとheadless harnessが共有するread／control protocolの基盤として再利用する。
- **価値／懸念:** 本体のQt／レンダリング状態をデバッガUIから隔離し、VS native debuggerとの併用、実行中Attach、Debugger単独更新を可能にする。一方、protocol version、履歴欠落の「未観測」表示、frame boundaryでのpause、snapshot復元とdeterministic replayの境界が必要。
- **次に確認すること:** Phase 0で既存MCP TCP／QLocalSocket／Named Pipeの接続候補、共有されるsemantic schema、diagnostic buildと通常buildのruntime有効化方針を確定する。

### Layer modulation は opacity以外へ拡張しない

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`ArtifactCore/include/Audio/Modulation/Router.ixx`。
- **状態:** opacityの評価、保存、Undo基盤は実装済み。Inspector導線とruntime確認は未完。
- **判断:** Transformはvariant／physics／layoutとの評価順を定義するまで追加しない。

### 物理 — rigid joint／polygon colliderの動作確認を先行する

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`ArtifactCore/src/Physics/Physics2D.cppm`。
- **状態:** jointとpolygon colliderの導線は実装済み、runtime未検証。
- **確認すること:** 重力の画面座標符号、scrub復元の制約、凹形状の凸近似、SoftBody／MPMとの接触。

### 3D rendering — AOVと実GPU契約は別スライスに保つ

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`。
- **状態:** World Position AOVのtarget基盤はあるが、書き込みPSO／runtime検証は未実装。
- **判断:** 2D／2.5Dの完成度を優先し、AOV、GPU skinning、ray tracingは個別の受入条件を定義してから進める。

### プロキシ生成 — Out-of-Process 専用ワーカー (ArtifactProxyWorker) 方式の採用

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`、`Artifact/include/Proxy/ProxyService.ixx`。
- **状態:** `ArtifactProxyWorker.exe` の native / Media Foundation / ffmpeg 経路と JSON Lines 通知を実装。native は実エンコーダー名と検出候補も返す。host は jobId/outputPath/outputBytes を照合し、Project View から queue キャンセルも可能。Eighth、音声再エンコード、hardware encoder、auto fallback、staged package の worker 実機確認済み。Media Foundation は H.264 入力で成功するが、ProRes 入力は非互換で、極小出力は 64px/axis にクランプする。成功・失敗・キャンセル時の partial cleanup と、検証ツールの worker timeout 回収も確認済み。host UI 統合の実機確認は未完。
- **判断:** 本体プロセスの安定性（クラッシュ・OOM 巻き添え防止）と FFmpeg C API 直接利用（進捗通知・GPU HW エンコード）を両立するため、専用の子プロセスワーカー（`ArtifactProxyWorker`）を設けて非同期 IPC で連携する構成を正規方針とする。

### Proxy worker — 成果物の原子性と timeout 回収を同じ受入条件にする

- **関連:** `Artifact/src/Worker/ArtifactProxyWorker.cpp`、`tools/proxy_worker_smoke_test.py`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- **状態:** 実装済み、host UI実機確認待ち。
- **判断:** worker の exit code だけでは成功とみなさず、completed message、final output、partial cleanup、timeout／強制終了後の状態を一組で検証する。これにより、ハングや中断を有効な proxy と誤認する経路を受入段階で検出できる。

## 保留中の設計判断

### シェイプレイヤー VP 操作の増強範囲（2026-09-02 ユーザー質問）

- **質問:** 「シェイプレイヤーのVP操作機能増強いけそうか」
- **解釈:** `VP = メインコンポジション Viewport`（`taste.md` の communication-integrity 規範に従い grep で `Viewport`/`TextViewport` へ展開、コード上に独立した `VisualProgramming` 系は無いため）。
- **現状（コード読みで確認済み、未検証含む）:**
  - `Artifact/src/Tool/ArtifactToolManager.cppm:44-46` に `ToolType::Shape / Rectangle / Ellipse` が定義され、`ToolType::Shape` は `ArtifactToolService` で `Shape modeling` 入口と紐付け済み（`docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:258`）。
  - `Artifact/src/Layer/ArtifactShapeLayer.cppm` は `ShapeType`（Rect/Square/Ellipse/Star/Polygon/Triangle/Line）+ `customPolygonPoints_` + `CustomPathVertex { pos, inTangent, outTangent, smooth }` + `customPathClosed_` を保持し、`evaluatePathAt(frame)` でパス頂点キーフレーム評価を実装。
  - `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のシェイプ系 VP ハンドラ:
    - `mousePress` L22853-22923 で Rectangle/Ellipse/Shape ツールのドラッグ → 矩形/楕円/スター/ポリゴン/三角のシェイプレイヤー新規作成（選択レイヤー有りは mask 経路、無しなら `RectangleToolMode::Shape/EllipseShape/StarShape/PolygonShape/TriangleShape` で `ArtifactShapeLayer` を生成、L27803-27873）。
    - `mousePress` L22940-22991 で Pen ツールが Shape レイヤー選択時のみマスクではなくカスタムパスを `pendingShapePathVertices_` へ追加（開始点クリック or Enter で確定、Backspace で取消、Escape 取消）。
    - `mousePress` L23527 `beginShapePathVertexDrag`、`updateShapePathVertexDrag` L25095、`endShapePathVertexDrag` L27345 経由でカスタムパスの頂点/タンジェントドラッグ編集。`ShapePathVertexEditCommand` で Undo。
    - `mousePress` L23506-23527 で Line シェイプの端点ドラッグ (`isDraggingLineEndpoint_` / `draggingLineLayer_`) を実装。
    - `isDraggingShapePathVertex_` / `shapePathEditPending_` / `shapePathEditDirty_` / `hoveredShapePathVertex_` / `hoveredShapePathTangent_` / `draggingShapePathTangent_` (0=vertex / 1=in / 2=out) を state に保持 (L12488-12505)。
  - `ArtifactCompositionRenderOverlay.cppm` のシェイプレイヤー描画: L1028-1105 でカスタム Polygon の頂点ストローク描画、Line の 2 端点描画、customPathVertices のベジェ描画を実装。ただし **頂点ハンドル/タンジェントハンドル/選択ハイライト/セグメント挿入マーカーのオーバーレイ描画パスは未確認**（grep 上このファイルには vertex overlay / hit-area / ハンドルサイズ定数が Shape 用に出てこない）。
  - `ArtifactRenderLayerWidgetv2`（LayerEditorPanel 内、`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:255-279`）に vertex / segment / tangent のコンテキストメニュー・Ctrl-click 選択追加・Shift toggle・numbering・hover/select 表示・path vertex duplication・polygon vertex duplication・segment insert 経路がある。`MILESTONE_LAYER_EDIT_2026-04-25.md:209-219` で `customPolygonPoints` を `CustomPathVertex` へ拡張済み。
- **増強候補（メイン VP 視点で未着手／不足）:**
  - (1) シェイプ専用 vertex/tangent/segment overlay 描画の RenderOverlay 統合（`LayerEditorPanel` の機能をメイン VP へ移植した残骸: `INSIGHT_ARCHIVE_2026-09-01.md:4549-4551` の懸念「ビルド未実施。タンジェント smooth 反射の長さ保存比、パスキーフレームの UI は次段階」）。
  - (2) Rect の `cornerRadius` ハンドル（`hitTestCornerRadiusHandle()` は `ArtifactRenderLayerWidgetv2` にあり、メイン VP 側は `setSize()` を width/height ドラッグで更新する経路のみ、`MILESTONE_LAYER_EDIT_2026-04-25.md:60` に「ShapeEditCommand 同様」とあるが RenderController 側 grep で未確認）。
  - (3) Star の `starInnerRadius_` ハンドル、Polygon の頂点ドラッグ挿入。
  - (4) `ToolType::Shape` のプリセット図形選択 UI（Rect/Ellipse/Star/Polygon/Line/Triangle のアクティブ切替。現状シェイプ作成は `Shape` 単独か `Rectangle/Ellipse` ツールの `rectangleToolMode_` 切替のみで、Panel 上にプリセット導線なし）。
  - (5) シェイプ operator stack（TrimPaths/Merge Paths/Offset/Pucker/Rounded/Wiggle/ZigZag/Twist/HandDrawnWobble/Stroke taper、9種実装済）のシェイプ VP 上インスペクタ／数値ハンドルドラッグ編集。
  - (6) パスの open/closed トグル、smooth toggle、corner ↔ bezier 切替を VP ハンドルで（`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:243-245` の selection grammar 整備と並ぶ）。
  - (7) シェイプレイヤー選択時の頂点/セグメント/タンジェントの選択 grammar をメイン VP 上で完成させ、`MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md:11-12,29` で計画中の `Artifact.Widgets.LayerEditor.ShapeOverlay` / `ShapeEditSession` / `ShapeHoverController` へ接続する。
- **価値／懸念:** AE 互換のシェイプ編集（特に頂点ドラッグ・tangent smooth・polygon segment insert・operator ハンドル）は既存の描画・データ層を破壊せずに機能を乗せられる層が既に厚く、メイン VP 側の実装ギャップはおおむね UI と routing 追加で済む。一方で (1) RenderOverlay への新規シェイプ専用 HUD 描画と `(7) ShapeEditSession` 抽出は `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` と相互作用し、`ArtifactCompositionRenderController.cppm` 28214 行・`ArtifactShapeLayer.cppm` 3380 行・`ArtifactCompositionRenderOverlay.cppm` 1827 行という巨大ファイル状態では変更影響範囲の見積もりが難しい。`MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md` の "incremental / stable" 方針に従い、まず最小スライスで 1 機能ずつ上げるのが安全。
- **次に必要なユーザー判断:**
  1. スコープ: 既存の `MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md` Phase D（キャンバス頂点編集）と Phase E（複数シェイプ）をそれぞれ独立に進めるか、または一括で (1)〜(7) をフェーズ計画に起こすか。
  2. 編集ホスト: メイン VP で直接編集（既存 `ArtifactCompositionRenderController` を拡張）か、`ArtifactRenderLayerWidgetv2`（LayerEditorPanel）側に集約して「ソロビュー」相当の編集ペインにするか。
  3. データモデル: 現状の単一 primitive を維持して `ShapeType` をツールプリセットにマップするか、コア `ShapeGroup` ベース（Phase E）へ移行してから VP 操作を実装するか。

## 検証運用

- **2026-09-04 — AI セグメンテーションの Core 契約:** `ArtifactCore/include/AI/ImageSegmenter.ixx` と `ArtifactCore/include/Image/DepthMap.ixx`。**事実:** 既存の `applySegmentationMask()` は空実装で、推論結果を書き込む `DepthMap` API がなかった。**対応:** 推論を `IImageSegmenter`（正規化された1ch前景マスク出力）へ限定し、Core 側で bilinear resample、閾値・softness・反転、alpha乗算／置換を適用する共有契約を実装した。`refineSegmentationMask()`で閾値／softness、foreground expand／contract、featherの共通後処理を追加し、`segmentBatch()`で複数の静止画／フレームから非破壊マスクを一括生成できるようにした。`analyzeSegmentationMask()`は foreground coverage／平均信頼度／bounds を返し、空マスクや過大マスクを App 側で警告できる。連番では `stabilizeSegmentationMask()` が前フレームマスクを控えめに混ぜ、推論のちらつきを抑える（動き追従は行わない）。モデル未配置時は、非AI・低品質であることを明示した `LuminanceImageSegmenter` を高コントラスト素材用のフォールバックとして追加した。**価値／懸念:** ONNX／DirectML、CPU fallback、将来のGPU推論はいずれも同一結果型に接続できるが、実モデル・モデル資産契約・GPU経路／実機品質は未検証。**次に確認:** 人物セグメンテーションモデルを1つ選定し、静止画の alpha 結果と既存 GPU mask 合成の preview／export parity を確認する。

- **2026-09-04 — ONNX/DirectML セグメンテーションアダプタ:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**事実:** 既存のONNX DirectML実装はテキスト生成専用で、画像モデルの入力・出力を `IImageSegmenter` に正規化する実装がなかった。またONNX compile definition/link は `ArtifactCoreAI` ではなく親 target にのみ付与されていた。**対応:** NCHW float 入力、最終2次元をマスクとするfloat出力のONNXモデルを、DirectML優先で読み込み、`DepthMap`へ戻すアダプタを追加。入力RGBのscale／mean／stddevと出力のNone／Sigmoid／Softmax、複数出力モデルの `outputIndex` を設定可能にし、出力マスクは bilinear で元解像度へ戻してsoft matteの連続値を保つ。AI target 自身へONNX link/defineを付与した。**価値／懸念:** 背景除去モデルをCoreだけで動かせるが、複数入力・動的shapeなどは設定契約を拡張してから対応する。**次に確認:** 実モデル（例: U²-Net系）を配置し、人物／髪のマット品質、DirectML利用、失敗時メッセージを実機確認する。

- **2026-09-04 — ONNX image module の明示BMI参照:** `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`。**事実:** `ArtifactCore` は実装 `.cppm` の primary interface attachment を自動dependency scanへ任せず、同ファイルで明示的なBMI参照を管理する。**対応:** `OnnxImageSegmenter.cppm` に primary interface と `Core.AI.ImageSegmenter` の参照を追加した。**価値／懸念:** Ninja/MSVCのdyndep不安定化を避けられるが、今後の新規 `import` 追加時にも同ファイルを同期する必要がある。**次に確認:** ユーザー許可後のCMake生成／ビルドで、OnnxImageSegmenterのIFC参照とONNXヘッダ解決を確認する。

- **2026-09-04 — ONNX image model diagnostics:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `modelInfo()` に ready、DirectML有効状態、入力サイズ・チャンネル、入力／出力テンソル名をまとめた read-only snapshot を追加。**価値:** App/UIを変更せずに、モデル契約と実行バックエンドの診断を接続できる。**次に確認:** 実モデル読み込み時にsnapshotとONNX Runtimeのsession情報が一致すること。

- **2026-09-04 — ONNX segmentation JSON configuration:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `loadOptionsFromJson()` を追加し、入力サイズ、前処理、letterbox、出力選択／activation、DirectML優先度を外部JSONから読み込む。既存sessionは設定変更時にresetする。**価値:** モデル資産を後で導入する際、コード変更なしにモデル固有契約を再現できる。**次に確認:** 実モデルの配布設定JSONを1つ作成し、モデル入力仕様と照合する。

- **2026-09-04 — ONNX segmentation letterbox pre-process:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `preserveAspectRatio` と padding value を追加し、固定サイズモデルへletterboxで渡し、出力マスクを同じ座標変換で元解像度へ戻す処理を追加。既定はstretchで後方互換を維持。**価値:** 縦長素材や正方形モデルで人物形状を歪めずに推論できる。**次に確認:** 16:9／9:16／1:1の実モデル結果でpadding境界とmask座標を確認する。

- **2026-09-04 — セグメンテーション失敗診断の統一:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `IImageSegmenter::lastError()` を共通契約へ加え、`segmentBatch()` が未ready・各item失敗・不正itemの最後の理由を返すようにした。**価値:** App側の一括処理UIが推論失敗を空マスクと誤認せず、ユーザーへ具体的に表示できる。**次に確認:** 実ONNXモデル不在・不正モデル・正常モデルでエラーが期待どおり更新されること。

- **2026-09-04 — 複数セグメンテーションマスクのCore合成:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `combineSegmentationMasks()` に Replace／Union／Intersect／Subtract を追加。入力解像度が異なっても `DepthMap` のbilinear samplingで target座標へ合わせる。**価値:** 人物＋髪、AIマスク＋手動補正、複数推論モデルの結果をQImage経由なしに共通マットへ統合できる。**次に確認:** 異解像度マスクでの境界品質、連続アルファのSubtract意味論、GPU cutoutとのpixel parity。

- **2026-09-04 — セグメンテーション自動適用の受入れガード:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `acceptsSegmentationMask()` と coverage／平均信頼度のしきい値設定を追加。**価値:** 空、または誤って画面全体を前景と判定したマスクを、Appが非破壊プレビューのまま停止・確認できる。**次に確認:** 実モデル別に人物／物体／背景なし素材の適正な閾値を決める。

- **2026-09-04 — OpenCV RotoBrush の IImageSegmenter adapter:** `ArtifactCore/include/AI/RotoBrushImageSegmenter.ixx`、`ArtifactCore/src/AI/RotoBrushImageSegmenter.cppm`。**事実:** 既存 `OpenCVRotoBrushEngine` はGrabCut初期マスク、前景／背景ストローク、Optical Flow伝播を実装済みだが、画像AIの共通契約へ未接続だった。**対応:** canonical BGRA float bufferを既存engineへ明示的に渡し、出力 `CV_8UC1` を `DepthMap` へ変換するadapterを追加。`propagateToNextFrame()` で、初期マスクを作成後の既存 Farneback flow 伝播をCore APIとして公開した。**価値:** モデルが無い環境でも、手動補正付きマットをONNX経路と同じbatch／refine／apply経路に渡せ、連番ではRotoBrushの追従を利用できる。**次に確認:** 現行engineのストローク座標・GrabCutマット・OpenCV例外時・大きなオクルージョンでの実機結果を確認する。

- **2026-09-04 — 軽量タスク facade の配置:** `ArtifactCore/include/Thread/LightweightTask.ixx` に、共有 `QThreadPool` を使う `executeLightweightTask` / `dispatchLightweightTasks` と、完了・キャンセル・失敗状態だけを持つ `LightweightTaskContext` を追加した。**事実:** 既存の `ThreadPool`、`Parallel`、`BackgroundTaskWorkerPool` は粒度や責務が異なる。**価値／懸念:** 短い非同期処理の入口を統一できる一方、context はタスク完了前に破棄できず、タスク内から `wait()` するとデッドロックする。**次に確認:** 実利用箇所を1つ選び、キャンセル・例外・pool飽和時の挙動をビルド／runtimeで検証する。

- **2026-09-04 — 2D Transform Gizmo の視覚ノイズ削減（Scale 中央 Y+ 軸線・Rotate 楕円重ね・軸 sweep 縮小）:** `Artifact/src/Widgets/Render/TransformGizmo.cppm` の `drawScaleCenterHandle` から Y+ 軸線 + tip ハンドルを撤去し、Rotate 描画ブロックから `drawEllipse` 2 本（X 軸赤 / Y 軸緑）と 68° sweep の X/Y 色分け弧 4 本のうち範囲を 36° に縮小。**事実:** 旧 `GIZMO_IMPLEMENTATION_STATUS_2026-04-10.md` の「Scale の中心→四隅 X 線」記述は既に解消済みで、現コードの X 線正体は中央ハンドルの Y+ 軸線だった。Aspect Lock は `isCornerScaleHandle()` 側にあり、Center ハンドルの Y 軸線とは無関係。Rotate リングは `hitThickness = ringThickness * rotateRingHitBoost` で既に hit area と visual thickness が分離済み。**価値／懸念:** X 線ノイズ・4 軸 rainbow 効果・楕円重ねがそれぞれ薄れ、平面/画像レイヤーの Scale と Rotate 操作の視認性が上がるはず。`drawEllipse` ローカル関数（816 行）は未使用になるが残置、hit test・Undo・ショートカットには触れていない。**次に確認:** ユーザー許可後に `Artifact` のモジュールビルドを実行し、`ArtifactTransformGizmo` の IFC が正常に再生成され、Scale 4 隅ハンドル・Center ハンドル・Rotate リング・Leader・Drag arc の描画が既存と一致することを確認。

- **2026-09-04 — M-VP-9 Navigation Contract 現状マップの固定:** `docs/technical/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_STATUS_2026-09-04.md` を新規作成し、既存実装の静的マップ（Alt+LMB orbit / MMB pan / Wheel zoom / `PreviewOrbitSnapshot` による orientation・pan・zoom 保存復元 / Frame Selected・All・View Undo・Redo の QAction + QShortcut 経路 / `activeViewport()` 系）と未着手項目（navigation cross 表示 / active viewport 細い枠 / preview-only と camera layer の厳密分離 / pivot・orbit source selector / surface snap）を表形式で明文化した。**事実:** `ArtifactCompositionEditor.cppm:9220-9272` の `setPreviewOrbitMode` は camera state のみを snapshot 化し、navigation session フラグ（`isAltOrbiting_` / `isPanning_` / `isAltZooming_`）は含まれない。`maskNavigationLocked` 経路は ON 時の抑制のみ。Work Cursor は配置・中央化・消去・overlay 表示まで既存、Pivot source 切替と surface snap は未着手。**価値／懸念:** AGENTS.md の「RenderScheduler / DX12 パスはシビア扱い」「既存挙動を不用意に変えない」「新規 signal/slot 接続禁止」「QPainter / QImage / QtCSS 禁止」に従うと、navigation cross 追加は Editor → Overlay への状態渡し経路が必要で pane manager (M-VP-2) 移行と密結合のため、Phase 3 では**コード改変ではなく状態マップの固定**で止めた。**次に確認:** ユーザー許可後に (1) preview-orbit snapshot に `isAltOrbiting_` / `isPanning_` / `isAltZooming_` フラグを含めた場合の復元整合、(2) navigation cross を `previewOrbitMode_` ON 時のみ theme token のみで描画する場合の最小実装可否、(3) active viewport 細い枠を pane manager 移行なしで 1 段重ね描画できるかどうか、を順に判断する。

- **2026-09-04 — FFmpeg C API のモジュール境界:** `ArtifactCore/src/Codec/FFmpegThumbnailExtractor.cppm` では、vcpkg の FFmpeg ヘッダが C リンケージを自動付与しない構成だったため、`extern "C"` でグローバルモジュールフラグメント内のヘッダ群を包む必要がある。**事実:** 未解決シンボルが `?av...` と C++ 修飾されていたが、修正後は通常リンクまで進み、`/WHOLEARCHIVE` は複数定義を起こした。**価値／懸念:** C++20 module の import／リンク問題に見えても、まず ABI のリンケージ名を確認する。**次に確認:** FFmpeg を参照する他の module 実装でも同じヘッダ配置を維持する。

- **2026-09-02 — C++ module split target の IFC 参照は分岐順に注意:** `ArtifactCore/CMakeLists.txt` では `src/Mask/` の包括分岐が個別の `RotoMask.cppm` 分岐より先に評価されるため、後置した個別参照設定だけでは実際のコンパイルコマンドに反映されない。**関連:** `ArtifactCore/CMakeLists.txt`, `RotoMask.cppm`, `ConfigSchema.cppm`, `Artifact/CMakeLists.txt`。**価値／懸念:** split target 化では「設定が存在する」だけでなく、最終的な source property の適用順と生成コマンドへの反映を確認する必要がある。**次に確認:** ユーザー許可後の再生成・ビルドで、対象コマンドに `/reference` が現れ、C1199 が解消することを確認する。

- ビルド・テスト・CMakeはユーザーの明示許可後に実行する。
- runtime検証済みになった項目は、このファイルからアーカイブへ移す。
- 実装済みの細かな履歴や重複した検証候補は、新規Insightとして追加せずアーカイブを更新する。

- **2026-10-11 — MPMのcache miss seekは旧runtime stateを残し得る:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::restoreSoftBodySnapshots()` はSoftBody/Cloth/MPMの全snapshotを先行検証し、一対象でも欠落すると何もrestoreせずfalseを返す。`Artifact/src/Composition/ArtifactAbstractComposition.cppm` の `setFramePosition()` / `goToFrame()` はこの結果を使ったMPM reset/replayを行わず、逆行時の `resetSoftBodiesToRestState()` もsoft bodiesだけをresetする。**事実:** MPM baseline snapshotからの再simulationはこの経路に存在しない。**価値／懸念:** cache evictionや直接seek後に前のframeのMPM状態が残り、表示frameとsolver stateがずれる可能性がある（runtime未検証）。**次に確認:** 砂用baseline保持、nearest-prior restore、fixed-step replayをcomposition/workerごとに持つ設計とし、順再生・direct seek・逆行往復・eviction後を同一state比較する。

- **2026-10-11 — Hencky/DP principal projectionをCPU MPMへ接続:** `ArtifactCore/include/Physics/MpmSolver2D.ixx` と `ArtifactCore/src/Physics/MpmSolver2D.cppm` に `MpmConstitutiveModel::GranularDruckerPrager` を追加し、principal Hencky projection、Kirchhoff→PK1 stress、`Fp_new=Fe_new⁻¹F` plastic updateをCPU経路へ接続した。Granular modeではsolid fractureをskipしGPU backend selectionを拒否、model/friction設定は粒子生成後に変更不可とした。**価値／懸念:** existing corotated/von-Mises経路はdispatch前後で維持し、粒子寿命とCPU-only制約を局所化する。Solver modeはLayer/save/load/snapshot metadataへ未接続で、fixture/build/runtime確認も未実施。**次に確認:** 2×2 SVD再構成（特に回転・重複特異値）、圧縮・extension・せん断return map、`F=FeFp`、旧preset回帰を許可後に検証し、その後専用Layerへbindする。

- **2026-10-11 — 2×2 SVDは重複特異値で固有ベクトル角が不定:** `MpmSolver2D.cppm` の既存analytic angle式は `q≈0` または `r≈0` のときU/V角を個別に0へ置く。重複特異値ではその自由度選択で剛体回転を再構成できない場合がある。**変更:** granular constitutive pathだけ `FeᵀFe` の右固有vectorと `U=Fe V Σ⁻¹` でprincipal frameを作り、`σ1=det(Fe)/σ0` で小さい特異値の差し引きcancelも避ける。legacy solid pathは未変更。**未検証:** 回転・等方伸長・近重複特異値fixtureでSVD再構成誤差を評価していない。**次に確認:** constitutive unit testで正detのランダム2×2行列について `UΣVᵀ≈Fe` と `det(U),det(V)>0` を測定し、しきい値近傍を確認する。

### 2026-10-12 — MPM density surface packet failure remains silent at queue capacity

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`、`Artifact/include/Render/RenderCommandBuffer.ixx`。
- **確認できた事実:** MPM Layerは粒子packetではなく、128x96 density fieldからbounded triangle meshを作って単一`MpmSurfacePkt`として送る。`PrimitiveRenderer2D::drawMpmSurface()`はcommand buffer満杯時にfalseを返しpacketを追加しない。Layerは戻り値を無視するため、そのframeの砂面は欠落する。CPU Render Queue側にはsolver densityから生成する専用`getThumbnail()`があるが、GPU Composition Viewの同一frame drawで代替描画はされない。
- **価値／懸念:** 粒子ごとのpacket overflowは避けている一方、command queue全体が満杯ならMPM面の可視性保証はない。Layer側のmesh容量は全cell三角形数を上限として確保済みで、問題はmesh vertex capacityではなくcommand packet slotの拒否。
- **未検証／次に確認:** packet capacity到達をruntimeで再現していない。優先slot確保、packet admission計画、または明示的な描画失敗診断のいずれかを設計し、他Layerの順序・bounded allocation・GPU submit失敗との責務を比較する。QPainter/QImageや粒子ごとのprimitive fallbackは追加しない。

## Weekly insight files

- **2026-10-11 — MPM砂は応力・塑性・破断・GPU tailを一体で切替える:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` の静的読解では、CPU応力はfixed-corotated PK1 (`firstPiolaKirchhoff`)、塑性投影は別関数のvon Mises型、塑性投影後に固体向けfracture判定が実行される。GPU substepもGPU弾性更新のあとCPUで塑性・collider・fracture tailを実行する。一次資料 Klár et al. 2016はHencky対数主ひずみのreturn mapping、extension時のtip投影、yield gradientの偏差方向を使う体積保存型non-associated flowを説明する。**訂正:** 同entryの「2D係数`α=sin(phi)`」は標準Frobenius normの`||dev(τ)||F`には誤り。現在のproject契約の`y(τ)=||dev(τ)||F + α tr(τ)`と2D Mohr-Coulomb関係`|τ1-τ2|/2 = -(tr(τ)/2)sin(phi)`から、`α=sin(phi)/sqrt(2)`を独立に導く。Klár et al.本文の3D係数変換はこの2D係数の根拠として直接流用しない。独立dilatancy角を必須とする先の案は採用した2D体積保存型projectionと異なるため、設計書を訂正した。**価値／懸念:** 塑性モデルだけの追加は応力との不整合を生み、GPUを許すと既存shaderとfracture副作用を招く。砂対応は応力生成・return mapping・粒子寿命・backend policyを一つのconstitutive strategyとして切替え、既存solid経路を固定する必要がある。**未検証:** 現行solverの積順序・平面応力近似にこの投影を組み込む数値安定性。**次に確認:** 圧縮・せん断・extensionの解析fixtureを作り、論文式、Kirchhoff応力符号、`F=Fe Fp`更新を照合する。

### 2026

- [ISO Week 41 (2026-10-05 – 2026-10-11)](insights/2026/ISO-W41.md) — 129 entries
- [ISO Week 40 (2026-09-28 – 2026-10-04)](insights/2026/ISO-W40.md) — 94 entries
- [ISO Week 39 (2026-09-21 – 2026-09-27)](insights/2026/ISO-W39.md) — 164 entries
- [ISO Week 38 (2026-09-14 – 2026-09-20)](insights/2026/ISO-W38.md) — 31 entries
- [ISO Week 37 (2026-09-07 – 2026-09-13)](insights/2026/ISO-W37.md) — 98 entries
- [ISO Week 36 (2026-08-31 – 2026-09-06)](insights/2026/ISO-W36.md) — 79 entries
- **2026-10-11 — 粒子描画packetの容量をreset後にbounded再利用する:** `RenderCommandBuffer::reset()`はsubmit後に呼ばれ、`ParticlePkt`はGPU submit完了後に破棄されるため、その境界で最大4本のparticle vectorを回収し、次のaccepted rvalue draw時に呼出元へ返すpoolを追加した。**価値:** MPM Layerは通常フレームで確保済みvectorを使い回せる。**懸念／未検証:** 初回warm-up、4本を超える同時packet、別のworker command bufferでの予約量、vector返却後の再使用をビルド・runtime・allocator計測で確認していない。**次に確認:** 安定フレームのallocator traceと粒子数別のpool hit率を取得し、pool上限と最大粒子数を決める。
- **2026-10-11 — Granular seek snapshotには外側fixed-step clockも必要:** `MpmSnapshot2D`はsolver内部accumulatorを持つ一方、`PhysicsSystem`はComposition単位の別accumulatorでgranular solverを進めていた。solver snapshotだけを戻すと、特にfpsとfixed timestepが整数比でないCompositionで再生位相がずれる。**変更:** snapshotにGranular用world accumulatorを付け、12フレーム間隔の共通checkpointとbaseline replayを実装した。現在のrestoreコードは全Granular solverのframe snapshotとclock remainderを先に検証し、最寄りの共通checkpointを一括復元する。checkpointがない場合は全baselineを復元してworld clockを0へ戻す。Material/solver再登録はworld cacheを消去しclockをresetして新baselineを作るため、frame snapshotへ静的なmodel/config IDは含めない。公開`MpmSolver2D::restoreSnapshot()`の別instance/config復元は同一設定を呼出側前提とし、この互換性をAPI内では照合しない。**未検証:** 異なるfpsでの連続再生とseek後の状態一致、cache trim後のnearest checkpoint動作。**次に確認:** 25/30/60fpsの前進再生とrandom seek後を同じframe stateで比較し、maxCachedFrames端で再現する。

- **2026-10-12 — MPM solverの更新をLayer active範囲へ同期:** `ArtifactAbstractComposition::evaluateLayerCollisionPairs()` は `isActiveAt(position_)` によって非active Layerをcollision対象から除外する一方、`PhysicsSystem::advanceGranularMaterialSolvers()` はworld内の全Granular solverを更新していた。**変更:** Compositionの既存frame評価でMPM solverのactiveフラグを同期し、active時だけfixed-step updateする。**価値／懸念:** playhead配置やin-pointより前の再生・seekで、表示されない期間に砂だけ進む不整合を防ぐ。**補足事実:** outer world clockはComposition内で共有され、inactive solverへの`update(fixedDt)`だけがskipされる。world clockはその間もfixed stepを消費し、solver固有accumulatorへinactive時間は加算されない。snapshotには共通clockのfractional remainderが保存される。**未検証:** active境界を跨ぐ通常再生・逆再生・checkpoint replayでの数値一致。**次に確認:** active前後のsolver snapshotを比較し、非active期間で粒子状態が不変、再active後にinactive期間分をcatch-upしないこと、seek往復で同じstateへ戻ることを確認する。

- **2026-10-12 — MPM surface recycle poolは同一frameのpacket上限に揃える:** `RenderCommandBuffer::reset()` はsurface packetから頂点vectorを回収するが、pool予約は2本固定だった。frame packet capacity内に3件以上のMPM surfaceがあると後続packetのmesh storageが破棄され、Layerが次frameに容量付きbufferを得られなくなる可能性があった。**変更:** pool metadataをcommand packet capacity分だけ初期reserveし、accepted surface packetを全てreset時に再利用可能にした。**価値／懸念:** packet数でboundedのまま複数Layerを継続描画できる。最大capacity時のmetadata overhead、初回buffer warm-up、submit/reset後の実再利用は未計測。**次に確認:** 3件以上のMPM Layerを複数frame記録し、mesh容量が維持されることと同時に、packet overflow時の明示挙動を検査する。

- **2026-10-12 — CPU Render QueueのMPM thumbnailはsolver密度から生成する:** CPU Render Queueの`renderLayerSurface()`はMPM固有分岐を持たず、Layerのthumbnail APIへfallbackしていた。`ArtifactMpmSand2DLayer`に`getThumbnail()`を追加し、現在frameのMPM solverからbounded density fieldを埋め、固定サイズの`ImageF32x4_RGBA`面を作ってQImage API境界へ変換する。GPU描画と同じ支持半径、sand色、alpha曲線を共有し、その式と色・alpha閾値はLayer内の共通定数/helperへ集約した。**確認できた事実:** CPU MFRはworkerごとに`cloneCompositionSnapshot()`を作成し、GPU workerとXPU CPU laneも独立composition snapshotを持つため、このLayerの再利用surface bufferは各worker clone内に分かれる。Diligent `MainRTVFormat`はsRGBで、`ImageF32x4_RGBA::toQImage()`はlinear surfaceをsRGBへ変換する。**価値／懸念:** Software Render Queueで汎用thumbnailを出す不整合を除き、既存QImage compositorへCPU surfaceを供給する。mask/blend適用後の視覚結果、GPUとのtransfer/alpha一致、画質・frame時間は未検証。**次に確認:** GPU/CPUの同一scene/frameで透明edge、mask/blend、色差と出力寸法を比較し、MFR cloneでのruntime再現を検証する。
- **2026-10-11 — MPM粒子seed領域と計算domainは別の寸法契約が必要:** `createGranularMaterialGrid`が初期粒子配置のwidth/heightから格子Nx/Nyも作っていた。sandboxの初期粒子はLayer内の一部分に置くため、そのままでは粒子がseed領域より下へ移動する格子が足りない。**変更:** 初期seed boundsとsimulation domain boundsをAPIで分離し、計算domainをLayer全体にしてparticle boundaryを設定した。**未検証:** 格子境界上の粒子安定性と堆積見た目。**次に確認:** 全source boundsへの落下、左右壁、下端堆積、collider併用時をruntime captureで確認する。


- **2026-10-12 — MPM砂の円collider中心点に決定的な退避方向を与える:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::resolveColliders()` は円の中心とparticleが一致すると距離ゼロでcollider解決をskipしていた。**変更:** GranularDruckerPragerだけ+x法線を退避方向にして円周へ位置補正し、既存CorotatedVonMisesは従来どおりskipする。**価値／懸念:** 初期配置や大きなstepで円の内部中心に入った砂粒子が永久に中心へ残る経路をなくす。中心には物理的に一意な法線がなく、+xは決定的な退避規約であり、接触の見た目・複数collider順序は未検証。**次に確認:** 中心静止／中心へ入射／小半径／ゼロ半径で有限値、位置補正、反発・接線摩擦を解析fixtureで確認する。


- **2026-10-12 — MPM砂のpolygon境界で最近接辺から法線を復元:** `MpmSolver2D.cppm::resolvePolygonCollider()` は内部particleの位置差で法線を得るため、境界上ではゼロ長になり、ray-crossingの内外判定が境界点を除外する場合は応答自体がなかった。**変更:** GranularDruckerPragerで最近接辺の向きとpolygon signed areaから外向き単位法線を復元し、距離二乗`1e-10`以下の境界点も接触応答する。既存CorotatedVonMisesの内外判定・法線処理は維持。**価値／懸念:** 辺上での入射速度に応答できるが、頂点の複数辺同距離、退化polygon、非有限頂点は別途安定性確認が必要。**次に確認:** CW/CCW polygon、辺中央・頂点・境界上の内向き／外向き速度、退化形状を解析fixtureとruntimeで確認する。


- **2026-10-12 — MPM砂Layerのdetachはworld-scoped solverを明示解除する:** `ArtifactAbstractComposition::Impl::removeLayer()` / `removeAllLayers()` はLayerをvirtual `setComposition(nullptr)`でdetachし、`ArtifactMpmSand2DLayer::setComposition()` はbase更新前に旧Composition runtime world IDとLayerIDでGranular solverを削除する。**価値／懸念:** MPM砂についてはLayer destructor頼みではなくComposition detach時に登録を閉じる静的経路がある。Undo後のsolver再生成と実world registry状態はruntime未確認。通常Shape material solverのglobal registry寿命とは別件。**次に確認:** 削除・Undo・Composition破棄後にworld-scoped granular registryが重複せず、再追加で初期baselineが一つだけ生成されることをruntime確認する。

- **2026-10-12 — Cloner Plain modifier の2D affine集約:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx` と `Artifact/src/Render/ShaderManager.cppm`。**事実:** GPU placement recordはoffset・Z rotation・local scale XYを保持し、単一PlainのD3D12 scale＋rotation raster probeはCPU参照と画素一致した。modifier列の合成ではtranslationを前段の累積rotation／scaleで写し、累積transformがrotation＋diagonal scaleとして表せる範囲（例: uniform scale＋rotationの後へnonuniform scale）をGPU対象とし、nonuniform scaleの後にZ rotationが来るshear生成順序はfallbackする。**未検証の仮説:** placement recordに2×2 linear basis (4 floats) とtranslationを保持すれば、shearを含む複数Plain modifierもGPU上で忠実に表せる可能性がある。**価値／懸念:** より広いmodifier chainを扱える一方、record stride増加、VS/compute ABI更新、3D Z semantics、親stack合成を確認する必要がある。**次に確認:** 新しい `--cloner-plain-chain` fixtureで実descriptorとCPU/GPU matrixを照合し、shearを生む順序をfallbackさせることもruntime確認する。


### 2026-10-12 — MPM density raster work is spatially bounded per particle

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::writeDensityField()`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** field dimensions/pixel count are capped, support radius is limited to 8 pixels per axis, non-finite/out-of-field particles are skipped, and each particle loops only over its clipped floor/ceil bounding box. With an exact 8-pixel radius, inclusive floor/ceil endpoints yield at most 18x18 tested samples per particle; work scales with active particle count rather than scanning the entire field per particle.
- **価値／懸念:** The density calculation has a finite per-particle stencil and fixed field storage, but the authored maximum 500x200 population can still imply high total CPU work. Floating-point edge behavior and per-frame cost at 100k particles have not been measured.
- **次に確認:** Profile `writeDensityField()` at default and maximum UI particle counts, including large deformation determinants and field-edge particles; verify the support clamp and density coverage in rendered output.

### 2026-10-12 — MPM砂の壁clampとdensity fieldの支持上限を共有

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::mpmSupportRadius()`、`PhysicsSystem::createGranularMaterialGrid()`。
- **確認できた事実:** density writerは出力sample間隔から最大支持半径を決める一方、壁clampはsolver側で独立に粒子体積から半径を算出していた。変形膨張時、両者のclamp上限が異なり得る。
- **変更:** Layerがfield寸法から計算した基準支持半径をGranular solver生成引数で渡し、particle populationおよびworld baseline登録前に静的solver設定として確定する。CPU `stepOnce()`、GPU readback tail、density writerはすべて`granularSupportRadiusLimit_`を参照し、各自のcell/field最大値ともminを取る。snapshotには含めず、solver再生成時にLayerから再設定する。
- **価値／懸念:** solver粒子の壁距離とfield kernelが同じ基準半径上限を使い、writer入力が8px超でもbounded clampで処理できる。GPU sandboxは現状非対応のためGPU tailは将来の整合用。構成の実コンパイル、edge fieldの描画、再生成後の値維持は未検証。
- **次に確認:** helperの返却値が128x96 field上限以内であること、solver登録前に値が確定すること、四辺・異方格子・膨張変形時のcoverageを数値／runtimeで確認する。

### 2026-10-12 — MPM support radiusの粒子数積をdoubleで計算

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::mpmSupportRadius()`。
- **確認できた事実:** helperは`columns * rows`をint乗算してfloatへ変換していた。当時のLayer UI上限500×200では積は安全だが、当時のPhysicsSystem granular factoryは最大262,144 particleまで受け入れ、helperはLayer用の寸法・resolution契約を自身では検証していなかった。2026-10-12にfactoryをLayer contract同等の100,000粒子へ変更済み。
- **変更:** helperで列数2..500、行数2..200と正の有限Layer寸法を検証する。seed寸法、particle count、particle volume、nominal radius、field capまでdoubleで計算し、最終radiusが正・有限・float範囲内であることを検査してから変換する。
- **価値／懸念:** int積overflowを避け、無効resolutionを黙って1へ補正しない。実際の最大寸法・粒子数での数値比較は未実施。
- **次に確認:** UI上限とfactory上限で旧float式との値、8px field cap、density writerの有限性を数値fixtureで比較する。

### 2026-10-12 — MPM粒子格子幅を密なseed軸に合わせる

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`。
- **確認できた事実:** grid cell sizeはX/Y粒子間隔の大きい方から決められ、縦横比が異なるseedでは間隔の小さい軸の複数列が同じ少数セルへ写り得た。Layer既定seedは384×168px、96×48配置。
- **変更:** 両軸間隔をdoubleで求め、小さい方を1px下限でcell sizeに採用した。格子ノード予算とCFL step上限は既存の制約を維持する。
- **価値／懸念:** 異方配置でも密な方向の粒子密度に見合う格子解像度を保てる。格子・粒子の数値品質とCFL棄却境界は未検証。
- **次に確認:** 既定および極端な列行比でcell mappingと格子予算を数値確認し、許容パラメータ範囲を実機で受け入れる。

### 2026-10-12 — MPM density rasterをfield sampleへ離散正規化

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::writeDensityField()`。
- **確認できた事実:** 640×480 Layerの128×96 fieldはworld sample間隔が5×5px。100,000粒子では粒子体積0.64512px²、既存nominal support radius約0.801pxでsample missし得た。半pixel対角長下限だけでは最悪位相でカーネル重みがほぼ0となり、peak clampにより離散体積寄与を保てないことも数値確認した。
- **変更:** Granularの支持半径下限をfield pixel diagonal（tiny epsilon付き）にし、各particleのfield内離散kernel weight合計をdoubleで求めて `currentVolume / (pixelArea * weightSum)` で正規化。field edgeで切られた領域は残ったsampleへ再分配される。各軸8px以内という既存上限から、18×18固定長の局所weight配列へcandidate kernelを1回計算・保存してから加算する。追加heap確保も二重の距離計算もない。個別particle peakはfloat表現上限だけを適用し、field加算後のdensityのみ1にcap。非Granular writer式は維持。
- **価値／懸念:** field sample位相とfield edge clippingに対する粒子ごとの離散体積寄与のぶれを抑え、単独kernelの輪郭まで個別peak capで縮めない。overlap時のpixel density capは局所寄与を減らし得る。固定局所配列は1,296 bytesでbounded。コード上のcandidate評価上界は18×18×100,000で約3,240万/field生成（field初期化12,288要素は別）だが、これは算術上界でありframe時間の実測ではない。
- **次に確認:** 100k粒子をsubpixel phase、layer edge、変形determinant違いでラスタ化し、pixelAreaを掛けたfield積分、overlap saturation、rendered qualityとframe timeを確認する。

### 2026-10-12 — MPM砂のG2Pで旧粒子位置の速度勾配を使う

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::gridToParticle()`、`updateDeformationGradient()`。
- **確認できた事実:** CPU stepはG2Pで`p.pos += v*dt`した後、変形勾配計算が移動後の`p.pos`から格子速度勾配を再標本化していた。G2P時の速度格子は移動前の粒子位置に対応する。したがって砂の`F`更新が1step先の位置を使う不整合だった。
- **変更:** Granular CPU経路のみG2Pの同じ旧位置・同じ格子 stencilで`gradV`を計算し、`p.pos`更新前に`F=(I+dt*gradV)F`を適用。Corotated経路の既存更新順は変更しない。
- **価値／懸念:** Hencky応力・体積・支持半径を決める`F`と、APIC速度転送の評価位置を一致させる。追加のstencil走査をなくすが、G2P内の勾配演算は増える。affine velocity fieldの一致と堆積挙動はruntime未検証。
- **次に確認:** 一様並進で`gradV=0`、affine fieldで所定勾配となること、1particle変形とshear boxをCPU solver fixtureで確認する。

### 2026-10-12 — APIC P2Gのaffine momentumへ粒子質量を掛ける

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::particleToGrid()`、`ArtifactCore/src/Graphics/MpmCompute.cppm::g_P2GBody`。
- **確認できた事実:** P2Gは格子運動量に `w * (m*v + C*diff)` を加算していた。`C`はG2Pで`velocity * diff * 4/dx²`として更新されるため速度/長さ、`C*diff`は速度単位であり、運動量項へ足すには粒子質量が不足していた。CPUとGPU shaderの両方に同じ式があった。
- **変更:** CPU/GPU両経路を `w*m*(v + C*diff)` に修正した。GPU shaderのfixed-point accumulationも同じ値式を使う。
- **価値／懸念:** APIC affine transferの次元を揃え、各粒子の格子運動量寄与が質量に比例するようにする。既存Corotated MPMの軌跡も修正前から変化する。運動量・角運動量保存とCPU/GPU parityは未検証。
- **次に確認:** constant translationとrigid rotationでP2G→G2Pの線形／角運動量を比較し、CPU/GPUの量子化誤差を測る。

### 2026-10-12 — Granular CPU格子境界の接線摩擦を一度だけ適用

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::stepOnce()`。
- **確認できた事実:** CPU stepはP2G直後とforce・grid velocity更新後の2回`applyBoundaryConditions()`を呼ぶ。Granular境界条件は壁法線速度を止め、毎回接線速度へ`1-boundaryFriction`を掛けるため、同じstep中に摩擦が二重適用されていた。GPU P2G/substep shaderの境界応答は速度更新後に一度行う。
- **変更:** Granular CPUではP2G直後の境界応答をskipし、stress forceとgrid velocityを反映した後の一度だけにした。Corotated legacyの呼出順は維持。
- **価値／懸念:** 壁際の砂の接線速度を二重に減衰させない。粒子境界clamp/collider responseも別に存在するため、衝突での全体摩擦量はruntime未検証。
- **次に確認:** 床上の水平運動量減衰を1step/固定時間で測り、CPUとGPUの境界応答回数・摩擦係数を比較する。

### 2026-10-12 — MPM初期粒子格子を一括reserve

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::addParticlesGrid()`。
- **確認できた事実:** 当時のGranular factoryは最大262,144のfactory上限内（Layer UI上限100,000）でseed粒子を作っていたが、2026-10-12にfactoryをLayer contract同等へ変更済み。汎用`addParticlesGrid()`のparticle volume denominatorは`nc*nr`をsigned intで計算していた。任意の大きなAPI入力ではoverflowし得る。またcapacityを逐次拡張していた。
- **変更:** columns/rowsを`size_t`化し、`max_size()/rows`で積overflowを先に拒否。チェック済み積をreserve数とvolume denominatorで共有し、`size()-max_size()`も検証してから必要容量を一度reserveする。
- **価値／懸念:** 初期投入中の段階的再確保を避け、particle volumeを実粒子数で除算する。1回の大容量cold-path allocationは残り、allocation時間は未計測。
- **次に確認:** 既定・最大粒子数で生成時間とピークmemoryを計測し、Composition snapshot baseline複製を含めた起動コストを確認する。

### 2026-10-12 — MPM砂の格子node予算をComposition単位へ集約

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`。
- **確認できた事実:** factoryは1 solverあたりgrid nodeを4,194,304以下に制限していたが、同じCompositionに複数MPM Sand Layerを登録すると各Layerが個別上限を使え、solver grid memory総量に同じ上限がなかった。solverには公開`gridNx()/gridNy()` accessorがある。
- **変更:** world ID内の既存solver node数をchecked `size_t`積で加算し、新規gridを加えたComposition総数が同じ4,194,304以下か登録前に検証する。差し替え対象Layerの旧gridはsteady-state計算から除外する。
- **価値／懸念:** 多数のMPM Layerでgrid memoryがLayer数に比例して無制限に積み上がることを避け、既存単solver最大値を維持する。solver replacement中は旧gridと新gridが一時共存し、他Composition/Render Queue cloneをまたぐprocess-wide総量はこのbudget対象外。
- **次に確認:** 複数Layer・複数world・Render Queue cloneでgrid allocation bytesとピークを計測し、world-local capがmemory budgetとして十分か確認する。

### 2026-10-12 — MPM砂のparticle payload予算にlive solverを含める

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`、`trimGranularWorldSnapshots()`。
- **確認できた事実:** 既存64 MiB budgetはbaselineとcheckpoint粒子payloadだけを数え、稼働solverの粒子配列を含めていなかった。snapshot trimmingもvector sizeを数えてcapacityの確保分を含めていなかった。
- **変更:** solver登録前のworld payload判定は既存live particle capacity + baseline capacityに加え、新solverと作成済みbaseline snapshotの実capacityを確認する。baseline snapshotは確認した同じ値を登録へ移す。checkpoint capture前にincoming particle payload分を予約し、古いcheckpointを先にevictしてからsnapshotをコピーする。枠を確保できない場合はそのcheckpointを作らない。trim計算はlive、baseline、checkpointのparticle/fragment-index vector capacityを合算する。
- **価値／懸念:** seek履歴だけでなくsimulation stateのresident particle payloadもComposition単位で制限でき、checkpointコピー中の粒子payload二重保持も抑えられる。allocator node/map overhead、vector copy capacityの実装差、solver replacement中の旧新共存は含まず、全process/worldの総量制限ではない。
- **次に確認:** capacityベースの見積もりと実allocator使用量、checkpoint capture/replacement peakを計測し、payload budgetとしての差を定量化する。

### 2026-10-12 — MPM粒子境界余白を支持半径へ制限

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::stepOnce()` とGPU readback tail。
- **確認できた事実:** 粒子境界clampは砂でもセル半幅を基礎余白としていた。格子幅をseed密度に合わせて細分化すると、この余白も増える場合がある。Granular solverには既にfield由来の共通支持半径上限があり、現在体積由来半径もclampに使っている。
- **変更:** Granularの基礎余白を `min(cellSize/2, granularSupportRadiusLimit_)` にし、CPU/GPU両経路で共有した。Corotated solidは従来のセル半幅のまま。
- **価値／懸念:** 解像度変更による粒子中心の境界距離増大を支持kernel基準で抑える。wall/node boundaryとの連成や細格子時の堆積形状は未検証。
- **次に確認:** 4面の壁衝突と床堆積で、node boundary response・particle clamp・density surfaceの間隔を数値/runtime確認する。

### 2026-10-12 — GPU Cloner fallback行列へstack scaleを反映

- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、Cloner GPU placement。
- **確認できた事実:** GPU eligible用streaming列挙はCPU clone transformを生成し、GPU matrix照合失敗時のsprite fallbackとcomposition field中心計算に同じ行列を使う。Plain modifierのXY scaleがplacement dispatchへ渡る一方、このstream行列には反映されず、generator transformがidentityの場合はmodifier適用前にcloneを除外していた。transform stackもshear-freeならrotation＋diagonal XY scaleとして既存placement recordへ集約できる。
- **変更:** CPU参照行列へstackとmodifier translation／Z rotation／XY scaleを適用してからidentity判定するようにした。stack XY scaleはshearを生まない順序に限定してplacement recordへ合成する。
- **価値／懸念:** GPU fallback時の表示・field queryでmodifier scaleが欠落する経路を塞ぎ、identity配置でもmodifierが変形するcloneを保持する。10,000件のshear-free affine合成を数値比較したが、Viewer runtimeの画素一致は未確認。
- **次に確認:** GPU dispatch失敗fixtureでtranslation／rotation／scale stackとPlain chainのCPU fallback画像をGPU placement結果と比較する。

### 2026-10-12 — GPU Cloner eligibility の毎レイヤー構築コスト

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm::drawWithPlacementObserverIndexed`、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx::drawWithGpuEligibleClonerGridIndexed`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm::layerGenerators()` / `layerCloneModifiers()`。
- **確認できた事実:** Composition Viewのindexed draw wrapperはGPU placement observerが有効な場合、描画対象レイヤーごとにGPU eligibilityを試す。helperは候補判定の前に値返しAPIの`layerGenerators()`と`layerCloneModifiers()`を呼ぶ。前者はNamedVectorを新規構築してbuiltin descriptorを同期し、後者もNamedVectorを構築して互換time-offset/sequence descriptorをpushする。
- **推測／未検証:** Clonerを持たない多数のレイヤーでもフレームごとにこのコールド寄りdescriptor生成が走り、非対象レイヤーでは後続CPU列挙時に同種の取得が重複する可能性がある。実際のheap allocation回数・frame time寄与は未計測。
- **価値／懸念:** GPU Cloner導入で描画対象全体へコピーやアロケーションを波及させると、ClonerなしCompositionのCPUコストまで増える。GPU可否判定より前にcheapな候補metadataを確認できれば、descriptor合成をCloner対象だけへ絞れる。ただし明示的generator descriptorだけを持つLayerを取りこぼさない検索方法が必要。
- **次に確認:** `ArtifactAbstractLayer`の内部generator stateを走査せずに候補有無を判定できるAPI／generation cacheの有無を調査し、allocation計測付きで非Cloner多数ケースを比較する。ReactiveEvents、MPMを変更せず、Clone descriptorだけを対象にする。

### 2026-10-12 — MPM surface packet rejection時の表示欠落

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`、`Artifact/src/Render/PrimitiveRenderer2D.cppm::drawMpmSurface()`、`Artifact/include/Render/RenderCommandBuffer.ixx::tryAppend()`。
- **確認できた事実:** MPM Layerは`drawMpmSurface()`のbool結果を無視する。surface packetはqueue満杯時に拒否され、verticesはLayer所有bufferへ戻るため次回再利用できるが、そのframeでは砂surfaceが描画されない。owner queueは最大16,384 packetを事前予約し、一般packetも同じ満杯条件でappend失敗する。
- **変更:** `ArtifactIRenderer::Impl::drawMpmSurface()`で、入力packetが有効かつowner queue満杯の時だけ既存`submitQueuedDraws()`を実行し、active render target復元後に一度再試行するようにした。
- **価値／懸念:** MPM surface packetは先行packetの後へ再投入され、queue容量を超えたフレームでもsurfaceを記録できる。満杯時は途中submitでbatchingが分かれる。GPU runtimeでtarget/order parityとoverflow頻度は未確認。
- **次に確認:** queue満杯fixtureで既存packetとMPM surfaceの画面順序・target維持をruntime確認し、途中submitの頻度とframe costを計測する。

### 2026-10-12 — MPM APIC CFL支持半径を二次カーネルに合わせる

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`、`weight()`。
- **確認できた事実:** 二次B-spline重みは正規化距離`|r|<1.5`でのみ非ゼロ。したがって4×4 stencilで`C*dpos`のFrobenius上限を取る距離は各軸1.5 cell、2Dユークリッド距離で`1.5*sqrt(2)*dx`。既存CFL定数`2*sqrt(2)*dx`は支持距離より33%大きかった。
- **変更:** APIC affine速度上限を`1.5*sqrt(2)*dx*||C||F`へ修正し、計画書のCFL説明も更新した。
- **価値／懸念:** kernel支持範囲を保守的に覆いながら、不要に小さな内部時間刻みを避ける。実際のparticle-grid位相での最大寄与、安定刻み、堆積挙動は未検証。
- **次に確認:** stencil内の全粒子位置位相と一般`C`行列に対し、各`C*dpos`がこの上限以下であることを数値fixtureで確認し、限界CFLケースで安定性とstep数を比較する。

### 2026-10-12 — MPM CFL波速は最も低密度の粒子で制約する

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`、`addParticlesGrid()`。
- **確認できた事実:** solver APIは複数回の粒子grid追加を許し、それぞれ異なる密度から粒子mass/volume0が決まる。CFL更新は粒子ごとに密度を走査していたが、ループ後に最後の粒子の密度だけで縦波速を評価していたため、追加順序で刻みが変わり得た。
- **変更:** 各active粒子の`(lambda+2*mu)/(mass/volume0)`を計算し、最大波速をCFLに使用する。
- **価値／懸念:** 複数密度の混在時にも、低密度粒子が要求する刻みを満たす。粒子密度差が大きい条件で実際の安定上限と保守性は未検証。
- **次に確認:** 同じ粒子集合を逆順追加して状態・刻みが一致すること、低密度粒子の追加で刻みが同じか小さくなることをsolver fixtureで確認する。

### 2026-10-12 — MPM CFL走査でNaNを最大値から漏らさない

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`。
- **確認できた事実:** `std::max(finiteValue, NaN)`は通常finite側を返すため、速度またはAPIC `C`にNaNがあると粒子速度上限へ反映されず、そのまま同stepのP2Gへ進み得た。
- **変更:** CFL上限の算出前に非有限の粒子速度をzero、非有限の`C`をzero matrixへ戻す。
- **価値／懸念:** 壊れたAPIC状態を同じstepのgrid転送へ伝播させない。位置・変形行列など他の非有限状態は既存のpost-step repairに残り、完全な異常状態回復を保証するものではない。
- **次に確認:** NaN/Infを速度・各`C`成分へ個別注入するfixtureで、P2G後の全粒子・格子状態が有限でsnapshot復元可能なことを確認する。

### 2026-10-12 — MPM粒子異常状態を格子index化より前に復旧

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`、`particleToGrid()`。
- **確認できた事実:** Granular状態の既存復旧はstep後に実行される。NaN positionは`particleToGrid()`の`floor`とint変換を通過して格子index計算に入るため、後段guardでは早すぎる復旧にならない。有限でもgridから極端に離れた位置はfloat-to-intの範囲外変換になり得る。`F`/`Fp`の特異・非有限値も同stepの応力計算へ入る。
- **変更:** CFL/step前処理でactive粒子のposition、velocity、`F`/`Fp` determinant、`C`、hardening stateを検査し、既存の復旧方針に沿って非有限positionをgrid domain中心へ置き、有限だがgrid外へ飛んだpositionを二次カーネルの完全支持範囲へclampし、invalid deformationはidentity、速度/`C`はzero、hardening stateは許容範囲へ正規化する。domain center自体が非有限の場合だけzeroへfallbackする。`setGridOrigin()`も非有限座標を拒否して最後の有効原点を保つ。
- **価値／懸念:** P2Gの座標変換とHencky stress前に不正数値を除く。異常状態の物理的意味を復元するものではなく、壊れた粒子をzero/identityへ落とす保護動作である。
- **次に確認:** 各粒子成分へNaN/Inf、反転determinant、過大hardeningを個別に注入し、P2G前正規化・有限な格子/粒子状態・snapshot検証を確認する。

### 2026-10-12 — MPM格子寸法とworld extentを更新前に検証

- **関連:** `ArtifactCore/include/Physics/MpmSolver2D.ixx::setGridOrigin()`、`ArtifactCore/src/Physics/MpmSolver2D.cppm::setGrid()`、grid node indexing。
- **確認できた事実:** `setGrid()`は`nx_*ny_`をsigned intで使用し、非有限cell sizeや巨大なdimensionをそのままgrid allocationへ渡せた。`setGridOrigin()`も有限座標のみ見ていたため、origin+grid extentがfloat範囲を超える設定を受け入れ得た。
- **変更:** `setGrid()`をbool APIにし、cell size、int積、vector max size、float world extentを事前検証して無効入力を拒否する。`setGridOrigin()`もbool APIとしてgrid far edgeがfloat範囲に収まる場合だけ更新し、両setterとも粒子生成後のgrid再解釈を拒否する。Material Grid/Granular Grid factoryは両設定の失敗を確認し、Material solverは粒子seedまで生成できた後にregistryへ登録する。grid幅へのdouble→int変換も範囲確認後に行う。signed積を使っていた単用途`resizeGrid()`は除去し、検査済みsize_t node countでgridをresizeする。
- **価値／懸念:** grid allocation、coordinate normalization、node index計算へoverflow/Infが混入する経路と、粒子/snapshotに対してgrid geometryだけが後変更される不整合を防ぐ。極端だが有限な大きいgridはallocator制約に達し得る。
- **次に確認:** NaN/Inf cell size、`INT_MAX`付近の各dimension、積overflow、origin+extent overflowを与え、無効入力後に直前のgrid構成が維持されることをAPI fixtureで確認する。

### 2026-10-12 — MPM material grid生成失敗をLayer authoring状態へ伝播

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createMaterialGrid()`、`Artifact/src/Layer/ArtifactLayerPhysicsBridge.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm::enableMaterialPhysics()`。
- **確認できた事実:** PhysicsSystemはinvalid grid/seed入力でnullptrを返すようになったが、Layer bridgeは戻り値を捨て、`enableMaterialPhysics()`はsolverがない場合もenabled/solverKindを先に更新していた。preset再生成が失敗したときには古いsolverだけが残り、新しいauthoring presetと実solverも食い違い得た。
- **変更:** bridgeをbool戻り値にし、PhysicsSystemのsolver生成成功後にMaterial Gridをregistryへ登録する。Layerは生成成功後にenabled/solverKind/presetをcommitし、preset変更時もenabled状態なら成功確認後にpresetを更新する。
- **価値／懸念:** 生成失敗時にUI/serialized authoring stateが存在しないsolverを有効と示さず、既存solverを保ったまま失敗を止められる。caller UIが失敗をユーザーへ表示するかは別途検討余地がある。
- **次に確認:** NaN bounds等でfactoryを失敗させ、初回enableのauthoring/registryが未変更、preset変更失敗時は旧solverと旧presetが一致することをfixtureで確認する。

### 2026-10-12 — MPM clear時にsolverの残り時間も破棄

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::clear()`、`update()`。
- **確認できた事実:** `clear()`は粒子・破断・colliderとseed grid metadataを消去する一方、solver accumulatorを残していた。clear後に粒子を追加すると、旧simulationのfractional step/backlogが新状態へ適用され得る。
- **変更:** `clear()`で`accumulatedTime_`も0へ戻す。
- **価値／懸念:** solver再利用時に旧時間が新しい粒子状態へ混入しない。PhysicsSystem側Composition clockとsnapshotの同時resetはこの直接APIの責務外。
- **次に確認:** 未完了substepを残した状態のclear→粒子再追加→updateで、clear前のaccumulatorが再生されないことをsolver fixtureで確認する。

### 2026-10-12 — MPM snapshotはgrid geometryをsolver間で持ち替えない

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::canRestoreSnapshot()`、`restoreSnapshot()`、`setGridOrigin()`。
- **確認できた事実:** Snapshotには粒子world positionとgrid originは入るが、grid dimensions/cell sizeは保存されず受け側solverの現在値を使っていた。clear後に同じorigin/seed metadataのままsolver gridだけ変更した場合、従来validatorでは旧particle positionsを異なる格子で使うsnapshotを受け入れ得た。
- **変更:** Snapshotへcell sizeとsolver node dimensionsを追加し、restore validatorでorigin・solver grid geometry・particle-grid columns/rowsが受け側の固定設定と完全一致することを要求する。Granular snapshotでは全粒子activeと各positionのquadratic-kernel-safe grid bounds内を追加条件にする。
- **価値／懸念:** restoreでparticle coordinatesを別grid geometryの下へ再解釈しない。異solverへのsnapshot移植は設計上未対応のまま明確に拒否する。
- **次に確認:** 同一solverのround tripは受理し、origin/column/rowを個別変更したsnapshotは拒否するfixtureを追加・実行する。
### 2026-10-12 — Granular MPM snapshotでseed不変の質量・体積を維持
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`MpmSolver2D::canRestoreSnapshot`)。
- **確認できた事実:** Granular snapshotの従来検証はmass/volume0の有限性と正値を確認するが、受け側solverの初期seed値との一致は確認していなかった。CFL波速は粒子密度 `mass/volume0` から計算される。
- **変更:** Granular snapshotの粒子数、mass、volume0、active状態を受け側solverの粒子列と照合してからrestoreする。通常のstepでこれらは更新されない固定属性。
- **価値／懸念:** snapshotの有限な改変で材料密度だけを別値にし、CFL条件・力学応答を変える経路を拒否する。異なるseed状態のsolver間snapshot移植も拒否される。
- **次に確認:** 同一solverの往復restoreが通り、mass/volume0/activeの個別改変および粒子数不一致が拒否されるfixtureを追加・実行する。

### 2026-10-12 — Granular seekは最新の共通checkpointから検証する
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm` (`restoreGranularMaterialSnapshots`)。
- **確認できた事実:** checkpointはframe key順に保持される。seek復元は従来、各solverのcheckpointを古い順に走査し、候補ごとに粒子全件を`canRestoreSnapshot()`で検証してから最新frameを選んでいた。通常の有効checkpointでも最大cache候補数だけ全粒子検査が繰り返される。
- **変更:** anchor Layerのcheckpointを新しい順に見て、全solverの粒子状態・共通frame・world clock remainderが一致する最初の候補を採用する。
- **価値／懸念:** 通常のseek時に全粒子検証を最新checkpoint分へ抑える。古い候補fallbackの回数、seek時間、異常snapshot時の動作は未計測。
- **次に確認:** 1 Layer/複数Layerで最新checkpoint採用と、最新checkpointを破損させた場合の直前共通checkpointへのfallbackをfixtureで確認し、粒子数とcheckpoint数を増やして計測する。
### 2026-10-12 — MPM Sand拒否編集でProperty cacheと欄を復元
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`。
- **確認できた事実:** Property Editorの共通preview/commitはlayer setterを呼ぶ前に`AbstractProperty`の値を更新する。MPM Sand setterは範囲外値やsolver再生成失敗時にauthoring fieldを維持／ロールバックするが、cached property値は従来戻していなかった。commit経路もsetter成否ではなくcached propertyの前後値でUndo commandを組み立てる。
- **変更:** MPM Sandの拒否時に現在のauthoring field値を該当cached propertyへ戻す。Property Editor共通rowはpreview/commit handler後にcached propertyが入力値と異なる場合、editor表示と補助表示をcanonical property値へ同期する。
- **価値／懸念:** solverを更新できなかった値がUI/Undo上で受理されたように見える不整合を防ぐ。一般row canonical syncの他setterへの影響、multi-selection、Undo再生成失敗ケースは実機未確認。
- **次に確認:** MPM propertyの有効編集・範囲外入力・solver予算超過時にauthoring値、property cache、editor値、Undo stackが一致するか確認する。

### 2026-10-12 — MPM SandのUndo適用で拒否setterをcache fallbackしない
- **関連:** `Artifact/src/Undo/UndoManager.cppm::applyLayerPropertyValue`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::setLayerPropertyValue`。
- **確認できた事実:** Undo/Redoの共通適用関数はlayer setterが`false`を返した後、一般propertyの互換性のため`AbstractProperty` cacheだけを直接更新するfallbackを持つ。MPM Sandはsolver再生成失敗時にauthoring値とcacheを旧値へ戻すため、このfallbackが走るとcacheだけが新値になる。`Artifact.Layer.Factory`はすでにMPM moduleをimportしている。
- **変更:** MPM Sand型でsetterが拒否した場合は共通fallbackへ進まず、command適用を失敗として返す。
- **価値／懸念:** Undo/Redo時の予算超過などでsolver設定とproperty cacheが分離するのを防ぐ。command stackが失敗をどう扱うか、拒否後のUndo/Redo挙動はruntime未確認。
- **次に確認:** solver再構成が成功するUndo/Redoと、意図的に再構成が拒否される場合のauthoring値・cache・solver・Undo stackの一致をruntime確認する。

### 2026-10-12 — MPM CPU Render Queueのsurfaceをbounded QImage poolへ再利用
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::getSoftwareRenderSurface`、`Artifact/src/Render/ArtifactRenderQueueService.cppm::renderLayerSurface`。
- **確認できた事実:** 汎用fallbackは`getThumbnail()`で128×96 float surfaceのQImage変換、Layer寸法scaled image、ARGB32 premultiplied変換をframeごとに作っていた。640×480 Layerで画像payload約2.4 MiB/frame。後続rasterizerはsurface dimensionsをlayer座標系として扱う。
- **変更:** MPM Layerが2枚の再利用QImage bufferを持ち、密度fieldを最終Layer寸法へresampleしてlinear sand RGBをsRGB化、premultiplied ARGBへ直接書く専用CPU surface APIを追加。当初はbilinear samplingだったが、後続変更でGPU meshと同じ固定対角線の二triangle piecewise-linear alpha補間へ置換した。最終pixelのalpha byteとRGB byteをpremultiply前に量子化する。Render Queueは型別分岐からこのAPIを使い、thumbnail変換とgeneric format conversionを避ける。detached bufferを優先し、2枚とも利用中なら従来thumbnail経路へfallbackする。通常thumbnail APIとGPU surface pathは維持。
- **価値／懸念:** 通常の1回またはmatte prepass＋final passではframeごとの画像buffer確保を再利用へ置き換え、surface dimensionsとQImage境界を保つ。linear-to-sRGB/alpha parityの8-bit量子化順序は寄せたが、Qt smooth scalingとのpixel差、別Render Queue呼出し組合せで2枚を超える生存bufferがないこと、複数worker寿命は未検証。density field生成とoutput sample走査は毎回必要。
- **次に確認:** build/link後、128×96→source dimensions基準画像比較、premultiplied alphaとopacity/matte/effect合成、matte source二重参照、frame連続処理でQImage allocationが寸法変更時に限られること、複数worker間のbuffer隔離を確認・計測する。

### 2026-10-12 — MPM Sandの同値commitでsolverを再seedしない
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::setLayerPropertyValue`。
- **確認できた事実:** Property Editorは編集previewをlayer setterへ渡し、その後commitも同じ値で呼び出す。MPM Sand setterは各呼出しでsolverを再生成し、baselineとComposition checkpointを作り直すため、preview済みの値がcommit時に再seedされていた。
- **変更:** parse/range検証後にMPM material/gravity/seed-resolution値を既存authoring値と比較し、同値ならsolver登録へ進まず成功を返す。
- **価値／懸念:** preview→commitで同じ最終値のsolver再生成・seed resetを避け、同値property通知の不要作業も避ける。異なるpreview値ごとの再生成負荷、Undo/Redo、粒子継続性はruntime未確認。
- **次に確認:** preview連続入力後に同値commitでsolver identity/baselineが維持され、異値commit・Undo/Redoでは意図した再生成が一度だけ起きるか計測する。

### 2026-10-12 — Granular world clock step予算をqueued timeへ連動
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::advanceGranularMaterialSolvers`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::replayGranularMaterialToFrame`。
- **確認できた事実:** Physics timeline fixed stepは1msまで設定できる。一方、Composition再生／replayは`max(1,fps)`のframe deltaを渡すため、1fps frameは1秒。Granular外側時計の固定64 step上限では1frameにつき64msしか消化できず、少なくとも936msずつbacklogが増える。外側backlogは102.4秒でcapされ、それを超えるstall時間は捨てる設計。
- **変更:** queued elapsed/fixed stepから必要step数を算出し、1 call上限1024で処理する。1秒/1msのケースは1callで全て消化できる。より大きいstallはbounded backlogとして残る。
- **価値／懸念:** 低fps Compositionのseek/replayでsimulation clockが恒常的に遅れる条件を解消しつつ、1 call workを1024 stepへboundedに保つ。低fps/高粒子数でのUI latencyは未計測。
- **次に確認:** fixedDt=1msで1fps/15fps/60fpsのframe replayがworld accumulatorを増やさず進み、1.024秒超stallでは最大1024 step後に正しいremainderが残ることをfixtureとruntime計測で確認する。

### 2026-10-12 — MPM checkpoint budget must include concurrent replacement peak

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::captureGranularMaterialSnapshots()`、`trimGranularWorldSnapshots()`。
- **確認できた事実:** captureは全solver分のlive particle capacityを事前見積もりとして予約し、古いcheckpointをevictしてからsnapshotを複製する。各snapshotの実particle/fracture capacity判明後にも、その1件をreservedとして予算を調整してからregistryへ公開する。複数solver captureの途中でregistry nodeが欠落するか実容量予約が失敗した場合は、そのframeのpartial entriesを全layerから除去する。最大UI設定500×200の単一solverは初期live/baselineに加え、最低1 checkpoint分がこのpayload予算へ入る。
- **未検証の懸念:** partial frameはscope guardで除去するが、allocation例外そのものはcapture callerへ伝播する。標準vector copyの実capacity上限も標準上はsource capacity以下と保証されないが、copy後にcapacityを計上してからpublishする。
- **価値／懸念:** 64 MiB payload予算内へlive+baseline+既存checkpoint+incoming snapshotを収め、実capacityが事前見積もりを上回る場合もpublish前に古いcacheを調整する。通常restoreは全solver共通frameだけを採用し、単solverだけの部分frameは受理しない。
- **次に確認:** allocation failure injectionを使い、例外境界でpartial frameが残らないこと、例外伝播がアプリのcapture callerでどう処理されるか、旧共通checkpointが必要以上に失われないかを確認する。

### 2026-10-12 — MPM Sand solver初期化失敗をLayer登録前に返す

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::setComposition()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::appendLayerTop()`、`Artifact/src/Project/ArtifactProject.cppm::createLayerAndAddToComposition()`。
- **確認できた事実:** 当初MPM Layer attach時のsolver生成失敗はwarningのみで、Composition appendとProject作成は成功扱いだった。`setComposition()`はvoidで、Composition moduleからMPM具体型を参照すると依存循環の懸念がある。
- **変更:** 基底Layerへ既定trueの`isCompositionAttachmentReady()`確認点を追加し、MPM SandはComposition runtime worldにsolverが登録された場合だけtrueを返す。Top/Bottom/Indexの登録口はattach後・registry変更前に確認し、失敗時はLayerをdetachして登録を止める。JSON復元は失敗Layerをparent resolution対象へ加えない。Project作成もappend失敗時にdirty化しない。
- **価値／懸念:** MPM具体型のimportをCompositionへ追加せず、通常Layerの既存attach動作を維持して、MPM solverのないblank LayerがCompositionへ登録される経路を閉じる。公開virtual追加に伴う全module rebuildと実動作は未確認。
- **次に確認:** 予算超過・無効ID/geometryでのTop/Bottom/Index追加、JSON復元とclone/undo attach、通常Layer append、失敗後world registry cleanupを確認し、成功/失敗結果とComposition treeが一致することをruntimeで検証する。

### 2026-10-12 — MPM attach readinessは新規Composition接続だけで判定

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` のTop/Bottom/Index追加経路。
- **確認できた事実:** readiness判定を毎回行うと、同じCompositionへ登録済みのLayerを並べ替えるケースでも失敗時のcleanupが `setComposition(nullptr)` を呼ぶ。registryにはLayerが残ったままComposition参照だけが切れる不整合になり得る。MPM Sandの`setComposition()`は同一Composition時にsolver再生成を行わないため、再挿入は新規attachではない。
- **変更:** 追加前の`compositionObject()`を比較し、別Composition/nullからの新規接続だけreadinessを検証する。既存Composition内での再挿入ではattach失敗cleanupを行わない。
- **価値／懸念:** 既存のLayer reorder/group経路でComposition所有状態を壊さない。Index挿入の戻り値は呼び出し側へ公開済みで、生成した新Layerは既存のmembership確認も行う。
- **次に確認:** MPM層を含むreorder/ungroup、solver登録失敗後の新規attach拒否、JSON/Undo再接続でComposition membership・QObject参照・solver registryが一貫することをruntimeで確認する。

### 2026-10-12 — Granular solver再設定時の一時メモリもworld予算へ含める

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`、`trimGranularWorldSnapshots()`。
- **確認できた事実:** 再設定は新solverとbaselineを作ってから既存entryを置換する。従来の事前計算は同じLayerの旧solverを除外し、world checkpointもsolver構築前にevictしなかったため、置換中のlive＋baseline＋cache＋incoming payloadが64 MiBを超え得た。grid-node上限も同Layer旧gridを除外していた。
- **変更:** world byte/grid予算の事前計算へ置換対象の現行solverを含める。solver確保前に要求particle count分を予約し、solver live capacityが判明した後は実live capacity＋要求粒子数分をbaseline複製前に再予約する。baseline生成後もlive/baseline capacity合計で再度checkpoint予算を調整してからcommitする。
- **価値／懸念:** 設定変更時もComposition単位の粒子・grid上限を越えず、新solver生成に失敗した場合は旧solverを保持できる。snapshot eviction後に割当が失敗した場合、solver stateは維持されるがseek cacheは削減済みとなる。vector capacityの実測は未実施。
- **次に確認:** 上限近傍の同Layer再設定で旧solver＋新solverの実capacityを含む時に拒否され、失敗時は旧solverの状態・設定が維持されること、余裕がある時は再設定が成功することをAPI/runtime検証する。

### 2026-10-12 — Granular snapshotのfracture配列を空に固定

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::canRestoreSnapshot()`、Granular checkpoint restore。
- **確認できた事実:** Granular modelでは`checkFracture()`が即returnし、factoryもfractureを無効化するため`fracturedIndices_`は空のまま。一方validatorは各indexがparticle範囲内なら任意個数を許可し、restoreのvector assignmentでsolver側の配列容量を増やし得た。
- **変更:** Granular snapshotはseed particle数の一致に加え、`fracturedIndices`が空でなければ拒否する。Corotated modelのfracture snapshotは従来どおり許容する。
- **価値／懸念:** Granular restoreが破断状態を持ち込まず、solver payload budget外の配列増加も防ぐ。壊れた内部snapshot fixtureと通常seekは未実行。
- **次に確認:** empty fracture listの通常Granular snapshotはrestoreでき、粒子数内の重複indexを持つmalformed snapshotはrestore拒否されることをAPI fixtureで確認する。

### 2026-10-12 — MPM GPU dispatch境界でGranularモデルを拒否

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGPUSubsteps()`、`setBackend()`、`setConstitutiveModel()`。
- **確認できた事実:** 公開setterはGranularモデルとGPU backendの併用を拒否するが、GPU substep入口自体は構成則を確認せずelastic shaderへ進める。現行のlayer factoryからGPU backendを設定する経路は見当たらないものの、将来callerが増えた場合や内部状態不整合では砂状態をelastic shaderで更新し得る。
- **変更:** GPU substep開始時にもGranular Drucker–Pragerを拒否し、callerの既存CPU fallbackへ戻す。
- **価値／懸念:** CPU-onlyの砂構成則契約をdispatch境界で守り、弾性GPU shaderによる誤更新を防ぐ。変更はGPU dispatch入口の防御条件のみで、Corotated経路は変わらない。
- **次に確認:** Granular+GPU backendの不正状態を作るfixtureでGPU dispatchされずCPU fallbackすること、通常Corotated GPU dispatchが維持されることを確認する。

### 2026-10-12 — MPM GPU density surfaceで頂点alphaをblend
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`、`Artifact/src/Render/ShaderManager.cppm::createPSOs()`。
- **確認できた事実:** MPMのsurface triangleは `PrimitiveRenderer2D::drawMpmSurface()` から共通solid-triangle batchへ流れる。ShaderManagerのsolid triangle PSOはsolid rectangle pipelineを複製し、blend設定も引き継ぐため、vertex alpha blendingはすでに有効。最初のPSO診断は誤りで、変更を残さなかった。
- **未検証の仮説:** 固定128×96 density fieldの見た目を支配する要素は、最大100,000粒子設定時に1粒子あたり0.64512px²となる低い粒子体積と、粒子がfield sample中心からずれた時の離散kernel weightである可能性がある。支持半径のsample coverage floorは既にfield pixel diagonal（640×480 Layerなら7.07px）で、密度寄与をその局所stencil内の離散積分で正規化しているため、単に支持半径を大きくするのは解決でなく体積も広がる。
- **変更:** solid-triangle renderer/PSOは変更せず、密度fieldと解像度の関係を追加調査する。
- **次に確認:** 1粒子をsample中心と最悪位相へ平行移動する解析fixtureで、離散field積分・threshold後の連結領域・積算体積を比較し、見た目の改善が支持半径ではなくfield resolution/thresholdから得られるか調べる。

### 2026-10-12 — MPM solver APIはauthoring粒子数上限を尊重
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`。
- **確認できた事実:** MPM Sand Layer propertyとJSON復元はcolumnsを2..500、rowsを2..200に制限する（最大100,000粒子）。Composition solver factoryは入力値を独立に512×512へclampし、aggregate capも262,144としていたため、直接API呼出しで上限外値を黙って増やし、またLayerの既定上限より最大2.62倍の粒子を生成できた。
- **変更:** API factoryはcolumns 2..500、rows 2..200、合計100,000以下だけを受け入れ、範囲外値をrejectするよう変更。保存値が作成後に変質せず、particle payload/grid予算を不要に消費しない。
- **次に確認:** 範囲の境界・直外値、solver予算超過をAPI fixtureで確認し、既定96×48と最大500×200で生成数がrequestと一致することを検証する。


### 2026-10-12 — MPM GPU surfaceはdensity thresholdで三角形をclip（後続変更で廃止）
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`、MPM surface vertex buffer。
- **確認できた事実:** 従来はdensity field sample四隅の一つでも0.08を超えるとquad全域を描き、低密度側の格子cellまで砂面色が広がっていた。smoothstep alphaは頂点にあるが、isodensity contourそのものはcell edgeへスナップしていた。
- **変更:** 各quadを元の2三角形に分け、各三角形をdensity 0.08の半平面でSutherland–Hodgman clipする。交差位置の座標を密度から線形補間し、残った三角形／四角形をfanで出力。最大4三角形（12頂点）/quadの固定buffer容量をcold pathでreserveする。frame中heap allocationは追加しない。
- **価値／懸念:** 粒子pointを描かず連続density surfaceとして表示する契約を保ったまま、密度閾値輪郭をsample cellの内部へ置き、薄い境界を厳密にfield閾値から切り出す。密度自体のfield解像度は128×96のまま。PSO／GPU画素、最大packet量、alpha continuityはruntime未確認。
- **次に確認:** 1sample幅の孤立blob、斜め境界、場の外端、全密度の一様fieldについて面積と輪郭pixelを比較し、triangle windingとCPU thumbnail境界の差をruntime captureで確認する。
- **後続更新:** clip方式は同日後続変更で廃止。現行方式は「GPU砂輪郭のclipを廃止しCPUと同じ三角形補間へ統一」を参照。


### 2026-10-12 — MPM clip交点でCPUのfield alpha補間を保つ（後続変更で廃止）
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`、`getSoftwareRenderSurface()`。
- **確認できた事実:** CPU Render Queueはsmoothstep後のdensity alpha fieldを拡大補間して出力する。GPU surfaceをdensity thresholdでclipした交点へ `densityAlpha(threshold)=0` を直接設定すると、field node間でalphaを線形補間するCPU画像に比べて、急勾配の輪郭でGPU側が痩せる。
- **変更:** clip vertexはdensity値とは別にalpha値を持つ。交点座標とalphaの両方を同じedge interpolation factorで補間し、幾何はdensity threshold、色alphaはfield nodeのsmoothstep alphaを継続する。
- **価値／懸念:** 連続面の境界位置を密度から決めつつ、従来CPU surfaceのedge interpolationに近いalpha profileを維持する。CPUはbilinear/8-bit、GPUはtriangle-linear/floatなのでpixel parityは未保証。
- **次に確認:** （当時の案）高勾配の単粒子とsubpixel移動でCPU/GPUの輪郭幅・最大alpha・積算coverageを同じzoomで比較する。現行方式の比較条件は後続Insight項目を参照。
- **後続更新:** clip交点方式は同日後続変更で廃止。現行方式は「GPU砂輪郭のclipを廃止しCPUと同じ三角形補間へ統一」を参照。


### 2026-10-12 — MPM surfaceはopacity zero時にfield rasterを省略
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`。
- **確認できた事実:** GPU MPM drawはdensity fieldを書き出し、最大12,513 quadを走査してからopacityをvertex alphaへ設定していた。opacityが0でもsurfaceは最終出力へ寄与しない。
- **変更:** finite/clampしたsafe opacityが0ならdensity rasterとmesh traversalを始める前にreturnする。OpacityがNaN/非有限の場合は既存どおり0相当として描画しない。
- **価値／懸念:** 完全透明のMPM Layerでframe-time CPU workを避ける。opacity変化時のsolver stateは変更せず、状態復元やsimulation clockには影響しない。性能差は未計測。
- **次に確認:** Composition Viewでopacity=0および0近傍のLayer表示、GPU draw packetが生成されないことを確認する。


### 2026-10-12 — MPM fixed-step collision候補配列をComposition scratchへ保持
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`。
- **確認できた事実:** MPM fixed-step/playback/replayが進むたびにcollision pair更新が呼ばれる。関数は各呼出しでlocal `std::vector<ArtifactAbstractLayerPtr>`を作って現在Layer数分reserveし、active solver/collider候補を再構築していた。
- **変更:** Composition Impl所有の`ArtifactArray<ArtifactAbstractLayerPtr>`をscratchにし、各呼出しで要素だけremove、current Layer countまでcapacityをreserveしてappendする。reserveは既存capacity以下ではno-opなので、過去最大Layer数を超えないstepではcandidate storageが再確保されない。Layer個別remove／remove-all時もscratch要素をreleaseし、capacityのみ保持する。collision pair setと選別ロジックは変更しない。
- **価値／懸念:** Granular fixed-stepのcollision更新で毎step起きるローカル配列のallocate/freeをなくし、独自コンテナ優先ルールへ合わせる。first useまたはCompositionの過去最大Layer数更新時はcold/hot境界でcapacity growthが残る。remove/clear時はSharedPtr要素を直ちに破棄する。CPU計測とreplay parityは未確認。
- **次に確認:** Compositionの最初のMPM playback以降でallocation profilerを取り、collision candidate bufferの容量がLayer追加時以外に変化しないこと、collider移動/active境界/seek結果が維持されることを確認する。


### 2026-10-12 — Collision evaluatorの未使用pair lifetime scanを除去
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`、`collisionPairKey()`。
- **確認できた事実:** pair scanはactive collision/material candidate同士のboundsを毎step組み合わせ比較し、QString keyを組み立てQSetへinsertしていた。既存setとのcontains後の分岐は空で、`activeCollisionPairs_`とkey helperはこのscan外で一度も読まれない。collider solver登録は直前の独立target/source loopで実行されている。
- **変更:** `collisionPairKey()`、pair boundsの二重loop、local `nextPairs`と未使用`activeCollisionPairs_`を削除。Granular/legacy material solver向けcollider clear・変換・registerと、acoustic impactの`compositionCollisionBounds()`使用は保持。
- **価値／懸念:** collision candidate数nに対する毎step O(n²) bounds/QString/QSet仕事をなくし、衝突形状登録の実処理だけを維持する。もし将来pair lifetimeからイベントを起こす場合は、実際のconsumer/契約を追加してから再導入する。物理結果・イベント回帰はruntime未確認。
- **次に確認:** 複数collision source/material targetの組合せで各frame colliderが再登録されること、contact/audio/fracture component eventがpair scan削除で変わらないことをruntime比較する。


### 2026-10-12 — MPM polygon collider輪郭バッファを固定step間で再利用

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`、`ArtifactCore/include/Physics/MpmSolver2D.ixx::addCollider()`。
- **確認できた事実:** 毎fixed-stepのcollision更新でPolygon source用`MpmCollider2D`をローカル生成し、輪郭をreserve/pushしていた。Solverも`addCollider()`でpolygon vectorを深コピーし、直後の`clearColliders()`はその容量を破棄していた。`colliders()`はモジュール外から使われていない。
- **変更:** Composition Implに再利用collider scratchを保持。Solverは既存`MpmCollider2D::polygonPoints`と同じ`std::vector<float>`型のpolygon point buffer poolを持ち、clear時にvector ownershipをpoolへ戻し、add時は再利用して内容だけassignする。公開`colliders()`はactive分だけを返す既存契約のまま。
- **価値／懸念:** 安定したsource/targetとoutlineサイズではfixed-stepごとのpolygon-point heap churnを避ける。新規最大polygon数／頂点数の初回増加時は容量成長し、Composition scratchとsolver poolに同じ輪郭データが保持されるため常駐メモリは増える。runtime計測・物理 parityは未確認。
- **次に確認:** 複数polygon target/sourceでoutline数の増減・target transform更新・clear/replay往復を検証し、steady-stateのallocation profilerでpolygon-point allocationが0になることを測る。


### 2026-10-12 — MPM polygon source outlineをcaller-owned scratchへappend

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm::appendCollisionOutlineLocalPoints()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`。
- **確認できた事実:** Shapeのcollision outline APIは`NamedVector<QPointF>`を値返却し、MPM fixed-stepごとにoutlineを構築してComposition側で一時オブジェクトとして受け取っていた。authoring済みpath/polygonは既存shapeデータから直接列挙できる。
- **変更:** 既存の値返却APIを維持しつつcaller-owned output APIを追加。Compositionは`NamedVector<QPointF>`をImpl scratchとして保持し、authoring済みpath/polygonは再利用capacityへ直接appendする。後続でRect、Square、Triangle、Line、Ellipse、Star、Polygon、Arrow、Heart、Diamond、Gear、Crossもcollision専用の直接サンプラーへ変更し、一時vectorを避ける。
- **価値／懸念:** 安定した輪郭サイズではMPM fixed-step中のoutline一時vector生成を避ける。primitive collision samplerは描画用`buildRenderablePoints()`とは別の式実装になったため、全ShapeTypeの点順・数値一致をruntime/fixtureで確認する必要がある。Layer interface/module integrationはbuild未検証。
- **次に確認:** 各ShapeType、corner radius、最小寸法、custom polygon/path、multi-contentについて、collision専用出力と既存`buildRenderablePoints()`を比較する。fixed-step allocation profilerでComposition scratch capacity安定後の確保数を測る。


### 2026-10-12 — MPM polygon colliderはparticle単位の辺走査前にAABB cull

- **関連:** `ArtifactCore/include/Physics/MpmSolver2D.ixx::addCollider()`、`ArtifactCore/src/Physics/MpmSolver2D.cppm::resolveColliders()`。
- **確認できた事実:** Polygon colliderは各粒子ごとに`mpmClosestPolygonPoint()`で全辺を走査し、続けて`mpmPointInPolygon()`でも全頂点を走査していた。Shape path／polygonは最大10万点まで保持でき、粒子数も最大10万であるため、bounding rectangle外の粒子にも高コストな全辺処理を行っていた。
- **変更:** `addCollider()`で有限なpolygon verticesを検証する既存loopにmin/maxを集約し、solver-owned colliderへAABBをキャッシュする。`resolveColliders()`はinclusive AABB外の粒子をpolygon traversal前にskipし、Granularの既存`normalLengthSq <= 1e-10`境界接触許容を保持するためそのAABBを1e-5拡張する。polygon座標や公開collider authored x/y/width/heightは変更しない。
- **価値／懸念:** AABB外かつ境界許容域外の粒子はinsideにもcontactにもならないため、接触意味を保った広域cullとなり、点ごとのO(vertices)を省ける。AABB内では既存の全辺走査が残り、AABBはcollider再登録時に全頂点を一度走査する。実機高速化・境界parityは未計測。
- **次に確認:** 凹凸polygon、縮退点列、AABB境界ちょうどのparticleで旧判定との結果を比較し、粒子がAABB外/内に分布するケースでstep timeと辺訪問数をprofileする。


### 2026-10-12 — MPM polygon queryの内外判定と最近傍辺探索を一周へ統合

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::mpmQueryPolygon()`、`ArtifactCore/include/Physics/MpmSolver2D.ixx::addCollider()`。
- **確認できた事実:** AABB内の各粒子に対し、旧実装はpoint-in-polygonとclosest-edgeを別々に全頂点走査していた。境界一致時のGranular法線選択ではpolygon signed areaもさらに全頂点走査していた。
- **変更:** closest-edgeループで同じedgeを使ってray-crossing toggleも更新する単一queryを導入。signed doubled areaはaddColliderの頂点validation/AABB loop中に算出・保持し、境界法線応答で再利用する。境界 epsilonと辺index順は維持。
- **価値／懸念:** AABB内粒子のpolygon vertex readsを通常2周から1周へ、境界接触時も追加周回なしへ減らす。branch/interleaveによるCPU時間差とray crossing/closest contactの数値・イベント同値は未計測。
- **次に確認:** CW/CCW、凹polygon、水平/垂直辺、頂点一致、重複点、縮退辺の結果を旧式fixtureで比較し、粒子分布別のpolygon辺訪問数とfixed-step timeをprofileする。


### 2026-10-12 — Polygon内部粒子も最近傍境界が必要

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::resolvePolygonCollider()`。
- **確認できた事実:** polygon colliderへの接触後、solverは粒子をcontainment判定だけでなく最近傍境界点へ投影する。粒子がpolygon内部にあっても境界距離と法線が必要であり、内外判定がtrueになった時点で走査を打ち切ると深くめり込んだ粒子の位置復元が変わる。
- **価値／懸念:** 直近のAABB cullとsingle-pass queryを維持したうえで、AABB内全粒子の最寄り辺探索を無条件に省く単純shortcutを避ける。大量頂点polygon内の粒子密度が高い場合は依然O(particles×vertices)となる。
- **次に確認:** 実使用sceneでpolygon頂点数とAABB内particle数、edge traversal時間を計測する。必要ならcollision semanticsを保つsegment BVHなどを評価し、単純なinside-only early-outは採用しない。


### 2026-10-12 — Collider係数sanitizeをparticle loop外へ移動

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::MpmSolver2D::resolveColliders()`。
- **確認できた事実:** friction/restitutionのfinite checkとclampは粒子ごと・colliderごとに同じcollider authored値から再計算されていた。応答関数内で他particleは参照・変更しない。
- **変更:** 走査順の変更案は取り消し、各particleのcollider orderを維持。solver-owned colliderへlegacy clamp値とGranular finite/clamp値を登録時にキャッシュして、particle×collider毎のclamp/finite検査を除去した。
- **価値／懸念:** nested particle→collider traversalと粒子アクセス局所性を保ち、係数をcollider再登録時だけ計算する。キャッシュ値がcurrent API semantics（legacy NaN保持、Granular nonfinite=0）と一致するかコード上は式を対応させたが、state parityと実CPU時間は未計測。
- **次に確認:** legacy solidとGranularのbox/circle/polygon/planeでNaN/±Inf/範囲外係数の旧loopとの粒子state parityを比較し、particle数・collider数別のstep timeをprofileする。


### 2026-10-12 — Composition collision source情報を1評価内で再利用

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`。
- **確認できた事実:** 各material targetのsource走査で、同じcollision sourceについてenabled/shape/offset/restitution property、global transform、world collision boundsを再取得・再計算していた。Outlineもsource-target pairごとに生成していた。
- **変更:** Composition-owned `ArtifactArray<CollisionLayerScratch>`へsource/target判定、collision properties、global transform、world bounds、target inverseを一度評価して保持。collider生成をsource outer / target innerへ変更し、各targetへ入るsource順は旧処理と同じに保つ。polygon outlineは最初の有効targetでlazy生成し、そのsourceの他targetで再利用する。頂点のtarget-local変換はtargetごとに残す。
- **価値／懸念:** 重複property accessor・bounds transform・inverse transformとmulti-target時のoutline生成を削減する。scratch要素はLayer SharedPtrを保持するため、従来通りremove/remove-all時の全scratch clearを維持した。source-target順の物理結果とruntime効果は未確認。
- **次に確認:** 1 source/複数target、複数source/複数target、同一source-target除外、inactive/非invertible target、empty polygon outline fallbackを比較し、transform評価回数・outline生成回数・solver collider順が一致することを確認する。


### 2026-10-12 — Polygon頂点のQTransform mapを合成行列1回へまとめる

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`。
- **確認できた事実:** Polygon vertexごとに`sourceOffset`加算、source global map、target inverse mapを実行していた。Qt公式QTransform文書は、積に書いた順に各transformを適用すると説明している。
- **変更:** source単位で`translate(offset) * sourceGlobal`を作り、target単位でさらに`* inverseTarget`を合成。各vertexはcombined transformのmapを1回受ける。
- **価値／懸念:** per-vertex QPointF加算と2回のmapを1回のprojective mapにまとめる。行列積先行によりfloat roundingが従来の逐次mapと異なる可能性があり、境界粒子の数値差は未検証。
- **次に確認:** translate/scale/rotation/shear/projective transformsで旧nested mapとの頂点差を許容差測定し、接触境界と大polygon頂点数でstate parityとstep timeを確認する。

# 2026-10-12 — Bound MPM polygon collider coordinate arithmetic

- **関連:** `ArtifactCore/include/Physics/MpmSolver2D.ixx`, `MpmSolver2D::addCollider()`
- **確認できた事実:** Polygon contact code calculates edge length squared in `float`, while collider registration accumulated signed doubled area in `float` around the world origin. Finite but enormous vertices could overflow intermediates, and large absolute coordinates could cause cancellation for small polygons.
- **変更:** Plane, Box, Circle, and Polygon geometry scalars are now limited to ±1e18; this bounds two-coordinate deltas, squared distances, and center±radius/half-extent operations within float range. Polygon signed doubled area accumulates in `double` relative to the first vertex and saturates to finite float range when cached.
- **仮説 / 未検証:** This bounds contact arithmetic and reduces translation-driven cancellation for accepted polygon geometry. The coordinate limit is many orders above ordinary Composition units, but extreme-coordinate handling, translated winding, and high-vertex behavior are not runtime-tested.
- **価値 / 懸念:** Invalid extreme colliders are rejected at registration, before broad-phase/contact math can propagate infinities or NaNs.
- **次に確認:** Runtime-check polygons near the coordinate limit, opposite-sign endpoint deltas, high vertex counts, both winding directions, small polygons translated far from origin, and ordinary geometry parity.

# 2026-10-12 — Reject unrepresentable MPM boundary spans

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm`, `MpmSolver2D::setBoundary()`
- **確認できた事実:** Boundary endpoints were checked individually for finiteness, then subtracted in `float` to detect degenerate spans. Opposite-sign large finite endpoints can overflow that subtraction to infinity, which bypasses the minimum-width rejection and marks the boundary active.
- **変更:** Normalize endpoints locally, compute width and height in `double`, and reject either span if it exceeds finite `float` range or is below the existing minimum before assigning boundary state.
- **仮説 / 未検証:** This prevents representational overflow from entering per-particle boundary clamps without changing ordinary boundary values or order-independent endpoint handling. Runtime large-span checks remain unrun.
- **価値 / 懸念:** Invalid oversized domains no longer enable a boundary whose float extent cannot be represented.
- **次に確認:** Exercise normal, reversed, degenerate, and opposite-sign near-limit endpoints through the solver API and confirm invalid calls leave `hasBoundary()` false.

# 2026-10-12 — Require a valid domain boundary for granular solver creation

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`, `createGranularMaterialGrid()`
- **確認できた事実:** `MpmSolver2D::setBoundary()` returns `void` and can leave `hasBoundary()` false for collapsed or unrepresentable spans. Granular creation previously continued configuring, seeding, and registering that solver without checking the boundary state. Checkpoint trimming also mutates retained replay history, so invalid inputs must be rejected before that step.
- **変更:** Granular solver creation now validates the float-converted endpoints and actual spans before checkpoint trimming, uses those validated endpoints, and defensively requires `hasBoundary()` after applying them.
- **仮説 / 未検証:** This prevents an attached MPM Sand layer from succeeding with its domain clamp silently disabled and avoids evicting snapshots for a deterministic invalid-boundary request. Tiny-domain and endpoint-collapse cases have not been exercised at runtime.
- **価値 / 懸念:** Creation upholds the authored closed-domain invariant without mutating seek history for invalid geometry.
- **次に確認:** Verify normal integer-sized domains still create, while sub-epsilon spans and collapsed float endpoints fail before any solver/snapshot registry mutation.

# 2026-10-12 — Use rounded boundary geometry for the granular grid

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`, `createGranularMaterialGrid()`
- **確認できた事実:** Solver domain endpoints are stored as `float`, but granular grid dimensions and seed containment were derived from the original input widths/endpoints before float rounding. Near representational boundaries these geometries can differ.
- **変更:** The grid extent now uses the validated float-rounded boundary spans, and seed bounds are checked against those same effective endpoints before checkpoint mutation.
- **仮説 / 未検証:** This keeps grid, seed, and particle clamp geometry aligned after endpoint rounding. Ordinary and near-ULP input cases are not runtime-tested.
- **価値 / 懸念:** A successfully created world uses one consistent domain geometry across grid allocation, initial particles, and boundary enforcement.
- **次に確認:** Exercise endpoint sums around float ULP transitions and verify all seed positions remain inside the effective domain and grid support margin.

# 2026-10-12 — Validate the rounded MPM grid stencil margin

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`, `createGranularMaterialGrid()`; `MpmSolver2D::updateGranularTimeStepForState()`
- **確認できた事実:** Granular recovery clamps particles to `gridOrigin + 2*cellSize` through `gridOrigin + (gridSize-2)*cellSize` so the quadratic MPM stencil remains in bounds. Grid origin is stored as `float`, so subtracting two cells can round away at large world offsets.
- **変更:** Creation now calculates the same float origin and safe position range used by the solver, rejects a domain not fully covered by that range before checkpoint trimming, and passes the validated origin to `setGridOrigin()`.
- **仮説 / 未検証:** This prevents a solver from attaching with reduced/missing stencil margin due to float origin rounding. Large-offset/small-cell cases and ordinary-domain acceptance have not been runtime-tested.
- **価値 / 懸念:** Domain clamps, seed placement, and the solver's quadratic-kernel-safe range now share representable geometry.
- **次に確認:** Compare generated seed extrema with the computed safe interval at ordinary values and around float ULP transitions; verify invalid geometry fails before changing snapshots or solver registration.

# 2026-10-12 — Share one implementation for granular seed geometry

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`, `createGranularMaterialGrid()` / `MpmSolver2D::addParticlesGrid()`
- **確認できた事実:** `addParticlesGrid()` computes float spacing, start/last positions, volume, and mass; granular PhysicsSystem creation must know the same seed extrema before it trims frame snapshots.
- **変更:** Added `calculateMpmParticleGridSeedGeometry2D()` to the existing `Physics.Mpm2D` module and made both seed generation and preflight consume its result. The helper preserves the solver's float operation order and reports count, extrema, and mass/volume values.
- **仮説 / 未検証:** Sharing one computation removes formula drift between validation and particle creation. Module integration and numeric parity have not been built or runtime-tested.
- **価値 / 懸念:** Invalid seed geometry can be rejected before resource-budget mutation without maintaining a second independent formula in PhysicsSystem.
- **次に確認:** Build the existing module graph when authorized; compare helper extrema against seeded particles for regular and float-ULP-edge requests, and verify failures leave snapshots/registry unchanged.

# 2026-10-12 — Validate target inverse transforms before MPM bounds mapping

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `evaluateLayerCollisionPairs()`
- **確認できた事実:** Material-target transforms are inverted once per collision evaluation. The previous code accepted the inverse solely from `QTransform::inverted()`'s invertible flag and then passed it to `mapRect()`.
- **変更:** The inverse is now usable only when inversion reports success and all nine matrix coefficients are finite. Target-local bounds also retain their coordinate/size finite checks.
- **仮説 / 未検証:** This blocks malformed inverse coefficients from entering MPM broad-phase bounds. No near-singular or corrupted transform has been exercised at runtime.
- **価値 / 懸念:** The broad-phase map now checks both the API inversion result and the actual matrix values before collider construction.
- **次に確認:** When runtime checks are authorized, test ordinary, near-singular, singular, and non-finite transforms against collider registration and finite solver state.

### 2026-10-12 — Reject non-finite Composition collision bounds

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `evaluateLayerCollisionPairs()`
- **確認できた事実:** Collision source bounds are transformed to world coordinates and then mapped through each material target's inverse transform before solver collider construction. Non-finite transform coefficients or mapped rectangle coordinates can otherwise enter `QRectF`/solver bounds checks.
- **変更:** A non-finite global transform invalidates the source world bounds, and target-local bounds are skipped if any coordinate or size is non-finite.
- **仮説 / 未検証:** This keeps malformed transform values out of MPM AABB and contact calculations. No runtime corrupted-transform case has been exercised.
- **価値 / 懸念:** Invalid source geometry cannot poison MPM collider broad-phase values. Such a collider is omitted until its transform/bounds become finite.
- **次に確認:** With runtime validation authorized, inject NaN/Inf into source and target transforms and verify the solver state stays finite and normal transforms continue to register identical colliders.

### 2026-10-12 — Affine-only composition for MPM collision outlines

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `MpmSolver2D::addCollider()`
- **確認できた事実:** Collision outline vertices previously passed through nested source-global and target-inverse `QTransform::map()` calls. The current code composes these transforms for affine source and target transforms, while retaining the nested mapping for projective transforms. It rejects the collider if any mapped coordinate or its float conversion is non-finite, consistent with the solver rejecting non-finite polygon coordinates.
- **仮説 / 未検証:** Affine composition reduces per-vertex transform work, but matrix composition can change floating-point rounding near collision boundaries. No runtime parity or performance measurement has been run.
- **価値 / 懸念:** This limits the fast path to affine transforms and avoids introducing a different projective mapping path, while preventing invalid coordinates from reaching collider registration.
- **次に確認:** When builds and runtime checks are authorized, compare affine and projective collider vertex coordinates and boundary contacts against the nested-map reference, then measure evaluation cost.
### 2026-10-12 — MPM通常再生後のGranular checkpoint capture

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::setFramePosition()`、`PhysicsSystem::captureGranularMaterialSnapshots()`
- **確認できた事実:** Granular solverは通常再生時に`advancePhysicsFrame()`から前進する一方、checkpoint captureの呼び出しはseek replay経路にだけ存在した。capture関数自体は12 frame間隔、共通状態、Composition byte budgetを適用する。
- **変更:** 通常再生stepが完了したframeでもcapture関数を呼ぶよう接続した。
- **価値 / 懸念:** 通常再生で到達したcheckpointからseek replayを再開でき、baselineからの全再計算を避けられる。capture frameの一致とseek数値再現性は未検証。
- **次に確認:** 通常再生で12 frame境界のsnapshotが保存されること、そこからのseekがbaseline replayと同一状態になることをbuild/runtime確認する。

### 2026-10-12 — MPM外部collider摩擦は固定値

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`
- **確認できた事実:** CompositionがMaterial/Granular solverへ登録する外部colliderはrestitutionをLayerの`physics.restitution`から読む一方、frictionは常に`0.2f`を設定する。MpmSolverはその値をGranular contact responseへ渡す。ArtifactのLayer property routingには`physics.friction`がなく、SoftBody colliderもfrictionを`0.15f`固定にしている。
- **価値 / 懸念:** 同じ砂Layerでも外部collider表面ごとの滑りやすさを調整できず、斜面堆積や壁沿いの流動が全source共通の摩擦へ依存する。既存UIから再利用できるpropertyは見つからず、摩擦設定を公開するには共有Layer/Collision契約の設計が必要。
- **次に確認:** MPM受入前に、固定摩擦係数で十分かをpile/colliderシナリオで評価する。足りなければCollision componentの摩擦設定を共通propertyとして導入する影響範囲・保存互換性・他solver統合を別途設計する。

### 2026-10-12 — MPM gravityとLayer transformの座標契約

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::configureSolver()`、`ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`
- **確認できた事実:** 粒子と境界はLayer local座標で生成し、draw時にglobal transformを適用する。外部colliderはComposition座標からsolver localへ逆変換される。GravityもComposition座標からinverse linear transformでsolver localへ写す処理を`evaluateLayerCollisionPairs()`に実装した。Gravity値はLayer frame rateで時刻評価し、keyframe補間・expression・envelope・external overrideを読んでから変換する。property cacheはLayer constructorでgravity pathだけcold path初期化し、JSON復元後とsetter成功後にprivate値を同期する。
- **確認できた事実:** 普通のnon-roving keyframeは`keyFrameInterpolationSegment()`からallocation-free scalar補間を行う。範囲外は追加した`keyFrameBoundaryValue()`で端点をallocation-free取得し、既存segment APIの意味とShape Gradientキャッシュ動作を変えない。
- **価値 / 懸念:** explicit expression/envelopeとroving trackでは既存property evaluatorへfallbackするため、評価frameごとに内部vector copy等のallocationが発生する可能性がある。これは該当機能をGravityへ設定したLayerに限るが、hot-path profileは未実施。完全なallocation-free評価APIは現状見つかっていない。
- **仮説 / 未検証:** 静止した可逆affine Layer transformでは、local gravityをglobal linear transformでCompositionへ戻すと指定gravity `(0, gravityY)`になる。動的Transformのmoving-frame慣性項は実装しておらず、runtimeのgravity向き・倍率は未確認。
- **価値 / 懸念:** particle spacing、domain寸法、stress/densityのsolver-local意味論はTransformで画面変化するため、物理的な粒径や材料応答までworld-space不変ではない。特異／非affine／非finite transform、または変換gravityがfloat範囲外の場合はsolver gravityを0へfallbackする。
- **既存経路の追加確認:** Layer transformはposition/rotation/scaleを含むframe評価可能な `QTransform`。Compositionはframeごとにこのtransformとそのinverseを取得し、colliderをsolver localへ変換する。Layer physics bridgeにはGranular solver取得口があり、solver自身に`setGravity(gx, gy)`があるが、Granular gravity更新関数はbridgeにない。したがってAPI境界上、Compositionが評価済みtransformの線形部でworld gravityをlocalへ写してsolverへ設定する経路を足すことは可能。translationはベクトル変換に含めない。
- **step/snapshot経路の追加確認:** 通常再生は`setFramePosition()`の各stepでcollision pair評価後に`advancePhysicsFrame()`を呼ぶ。seek replayは`replayGranularMaterialToFrame()`でsnapshot restore/baseline reset後、各frameのlayer transform更新→collision pair評価→Granular advance→snapshot captureを行う。Granular snapshotは粒子/clock stateを持つが重力設定は持たない。world gravityを導入する場合、どのframe poseを次のintegrated intervalへ適用するかを両step経路で一致させ、restore直後にも再設定する必要がある。
- **座標型の追加確認:** `docs/planned/MILESTONE_TYPED_COORDINATE_SPACES_2026-09-27.md`はComposition/Layer vectorの型分離を進めているが、MPM gravity APIはraw floatでpx/s²の専用型は見当たらない。Composition→solver-local gravity mappingを実装する際は、明示的なnamed conversionを使うかcoordinate-space milestoneへ接続し、暗黙の`QTransform`/float混在を増やさない。
- **価値 / 懸念:** API追加とComposition frame評価の拡張で重力方向契約は合わせられる可能性がある。ただしsolverのparticle/domain寸法もLayer local pxのままなので、非一様scale時の物理的な長さ・material responseはworld-space不変ではない。gravityだけを逆変換するならLayer localをsolver単位とする局所シミュレーションであり、pixel物理の完全なworld-space実現とは区別して受入条件を記す必要がある。通常再生とseek replayの両経路でtransform/gravity同期が要る。
- **次に確認:** 逆変換の数値fixtureとruntimeでtranslation、rotation、反転、uniform/nonuniform scale後のComposition gravity、collider接触、重力keyframe、再生/seek一致を検証する。animated transformの非慣性項を含めるか、singular/non-affine fallbackをどう扱うかは受入仕様として評価する。

### 2026-10-12 — MPM density fieldは描画用の正規化surface

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::writeDensityField()`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::densityAlpha()`
- **確認できた事実:** 各粒子は離散kernel weightの合計で`currentVolume`を正規化してfieldへ足すが、各pixel値は1.0で飽和し、field外へはみ出す粒子のweightもfield内sampleだけで再正規化される。
- **仮説 / 未検証:** 重なり・高密度状態ではfieldの面積積分は物理体積を保存しない。したがって現在のfield/alphaは描画surface用で、密度計測や質量保存の判定値には使えない。
- **価値 / 懸念:** 表示上のsurface再構成とsolverの質量保存を混同しない。今後field積分を体積検査へ転用する場合はsaturationと境界再正規化を除いた専用診断が必要。
- **次に確認:** 単粒子・重なり・field境界の解析配置で積分誤差を測り、描画閾値やsupport変更がsolver粒子状態へ影響しないことを確認する。
### 2026-10-12 — Granular GPU境界経路は現行契約では到達不能

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::setBackend()`、`setConstitutiveModel()`、`updateGPUSubsteps()`
- **確認できた事実:** Granularモデル選択中のGPU backend設定は拒否され、GPU backend中のGranularモデル選択も拒否される。したがってGPU substep後処理内のGranular分岐は現行APIから到達不能である。
- **訂正:** 以前GPU後処理にGranular APIC壁投影が欠落すると記録し、到達不能分岐へ変更を加えたが、現行製品経路の不具合ではなかった。無効な変更を戻した。
- **価値 / 懸念:** GPU砂対応を追加する際は、単に後処理へ分岐を足すだけでは不十分であり、GPU構成則・塑性・境界を含むCPU parityを実装・検証する必要がある。
- **次に確認:** GPU砂を明示的に対象化する設計時に、backend capability契約と対応fixtureをまとめて見直す。

### 2026-10-12 — Granular grid wall frictionを接触時のCoulomb制約へ変更

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::applyBoundaryConditions()`
- **確認できた事実:** Granular CPU solverは内部CFL timestepごとにgrid境界上の接線速度を`1-boundaryFriction`倍していた。接近していない粒子速度にも適用され、同じ実時間で内部step数が増えるほど摩擦が累積する。一方、外部collider応答は接触面の接線速度に別の一定割合を適用する。
- **変更:** Granular grid壁では外向き法線速度だけを取り除き、その速度補正量`|Δv_n|`を法線impulse相当として最大接線速度変化を`μ|Δv_n|`に制限し、接線速度を0で止めるCoulomb型の応答へ変更。離反・接触していない状態では接線速度を変えない。Corotated legacy境界式は維持。
- **価値 / 懸念:** 壁に触れていない粒子へ摩擦を適用する誤りを避け、一定割合の毎substep減衰によるCFL step数依存を減らす。grid boundaryの離散接触は投影法なので、corner接触と外部collider摩擦との数値的一致は未検証。
- **次に確認:** 解析fixtureで左右上下壁の法線非貫通、`|Δv_t| <= μ|Δv_n|`、接触なし／離反時の接線速度不変、Corotated軌跡不変、CFL timestep変更時の同一接触結果を確認する。

### 2026-10-12 — Granular外部collider摩擦を接触impulse上限へ変更

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::applyGranularColliderVelocityResponse()`、`resolveColliders()`
- **確認できた事実:** Plane/Circle/Polygon/Boxで形状接触と位置補正が成立した際、Granularの接線速度と`C`行列を摩擦係数に基づいて縮小していたが、接近法線速度が正（離反）でも同じ減衰を実行していた。
- **変更:** Granularは法線接近速度`v_n < 0`の場合のみ反発を適用し、`Δv_n=-(1+e)v_n`を上限算出に使う。接線速度は`μΔv_n`以下の変化で0まで止め、これを超える成分だけ残す。離反状態では速度・`C`を摩擦で変更しない。Corotated応答式は維持。
- **静的照合:** `v_n=-2, e=0.25, μ=0.4`なら法線補正量は2.5、接線変化上限は1.0。接線速さ0.5は0まで減り、速さ2は1残るため、接線変化量は上限を超えない。
- **追加確認:** Plane/Boxは軸方向の単位法線、Circleは半径差分を距離で除した単位法線、Polygonは内側押し戻し差分または辺方向を長さで除した単位法線を応答関数へ渡す。
- **価値 / 懸念:** 接触中の摩擦を法線運動量交換に制限し、Collider内へ位置補正された離反粒子への無条件な摩擦を除く。形状境界からの法線推定・cornerや移動collider速度を含めた挙動はruntime未確認。
- **次に確認:** 各shapeで接近・離反・接線速度の解析fixtureを行い、反発後法線速度とCoulomb上限、CPU replay経路での決定性を確認する。

### 2026-10-12 — Polygon頂点接触法線を凸凹と接近速度に応じて選択

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::mpmQueryPolygon()`、`resolvePolygonCollider()`
- **確認できた事実:** Polygon境界上で粒子が頂点と一致すると最短点差分がゼロとなり、以前はループ内で最初に見つかった辺の法線だけをfallbackに使っていた。隣接法線の単純平均だけでは、反射するturnを持つ凹頂点で和が内部を向く場合がある。
- **変更:** 同じ頂点に接する二辺の単位外向き法線を保持し、windingを掛けた辺turnで凸／凹を判定する。凸頂点では両法線へ同時進入する場合に限り和を使い、片面のみへの進入ならその面を使う。凹頂点は接近速度との内積がより負の面法線を使う。法線の和は再正規化し、面積がほぼ0のGranular polygonは接触解決対象から除外する。法線算出も最短辺候補に限定し、投影不能な長さ1e-6以下の辺は候補法線にしない。
- **価値 / 懸念:** winding反転や頂点の辺走査順に依存する法線選択を抑え、凹頂点で内向きの二等分法線を使う問題を避ける。自己交差・重複・極端に短い辺は未検証。
- **次に確認:** 同じ凸／凹頂点をCW/CCWで表して接近速度、選択法線、反発・摩擦が一致すること、重複頂点・退化形状で有限な応答となることを解析fixtureで確認する。

### 2026-10-12 — GPU砂輪郭のclipを廃止しCPUと同じ三角形補間へ統一

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`、`emitAlphaTriangle()`
- **確認できた事実:** CPU Render Queueはsmoothstep後のdensity alpha fieldを、GPU surfaceと同じ固定対角線の二triangle piecewise-linear補間で出力する。
- **変更:** 従来のdensity threshold geometry clippingを廃止し、元の三角形と各頂点alphaを保持する。全頂点alphaが0の三角形のみ省略する。これでCPU/GPUの補間関数と対角線が一致し、最大頂点数は1セル6（従来12）、二本のposition+alpha scratch bufferは約3.5 MiBから約1.75 MiBへ減る。
- **価値 / 懸念:** ソフトウェア面とGPU面のalpha field定義を揃え、境界交点の特別処理も不要にする。GPU rasterization、target format、blend経路を含む実画素parityは未検証。
- **次に確認:** 同じdensity fieldをCPU/GPUへ与え、alpha境界・細い支持領域・sRGB targetでpremultiplied pixel差を比較する。

### 2026-10-12 — MPM砂のJSON復元からsolver bindまでの順序を確認

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm::createArtifactLayerFromJson()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::appendLayerTop()` / `fromJson()`、`ArtifactMpmSand2DLayer::setComposition()`
- **確認できた事実:** FactoryはLayer JSON propertiesを復元してserialized Layer IDを戻した後、Compositionへ渡す。Composition appendが`setComposition()`でworld/Layer ID付きGranular solverを構築し、`isCompositionAttachmentReady()`で登録有無を検査する。Composition `fromJson()`はLayer attach後に保存frameへ移動してGranular replayへ入る。
- **価値 / 懸念:** 静的な保存・load順序ではsolverが未復元IDのまま登録される問題は見つからなかった。solver state自体はJSON保存せず、authoring settingsとseedから再構築してframe replayする。
- **次に確認:** project save/load、Render Queue clone、Undo/Redoのruntimeで、solver registry bind、保存frameの粒子状態、seek往復が再現することを確認する。

### 2026-10-12 — Granular dampingと壁摩擦のCFL刻み依存を静的照合

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGridVelocities()`、`applyBoundaryConditions()`
- **確認できた事実:** Granular damping factorは`(1-d)^(120*dt)`であり、自由減衰の係数を複数stepで積算すると`(1-d)^(120*Σdt)`になる。壁摩擦は外向き法線速度補正量ごとに`μ|Δv_n|`以内で接線速度を減らす。
- **静的照合:** 同一総時間かつ他の力がない場合、step数を分割してもdamping係数の積は一致する。同一の総法線impulseを壁で受けた場合、接線速度の最大減少量の合計は`μΣ|Δv_n|`でstep数に依存しない。
- **価値 / 懸念:** 両項目は一定割合の毎step減衰と異なり、CFL step数だけで係数が変化しない設計。ただし力積分、接触発生時刻、粒子-G2P移送は刻み幅で変わり得るため、完全な軌跡一致は保証しない。
- **次に確認:** 同じ総物理時間でCFL substep数を変え、自由減衰解析解と壁の法線impulse・接線速度差をruntime比較する。
### 2026-10-12 — MPM Gravityのkeyframe状態はレイヤー固有JSONで永続化する

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::toJson()` / `fromJsonProperties()`、`ArtifactCore/include/Property/PropertySerializationBridge.ixx`
- **確認できた事実:** `ArtifactAbstractLayerPersistence` の一般プロパティ保存対象にはTransformなどの限定経路があり、`mpmSand.gravityY` のようなLayer固有property cacheは自動で保存されない。CompositionはMPM Gravityをframeごとにproperty evaluatorから読むため、keyframe等を保存しないと再読込後の挙動が変わり得る。
- **変更:** scalar gravityは従来キーを維持したまま、animatable flag/expression/keyframes/envelopesを専用JSONキーで追加保存・復元する。Project duplicate helperはproperty cacheへ直接deserializeしてprivate authoring scalarを更新しないため、Gravity復元後にanimatable flagをコピーしてMPM setterも呼び同期する。
- **追加修正:** `SetLayerPropertyValueCommand`のUndo/RedoではMPM setter成功後に一般property cacheへ値を書き戻すfallbackが禁止されており、Gravity以外のMPM scalar setterもcacheを同期していなかった。MPM parameter setterの同値成功・構成更新成功後に内部canonical値をcacheへ反映する。
- **価値／懸念:** 再読込・複製後もアニメーションしたGravity値とsolver authoring stateを揃える。複製時のsolver予算不足、Undo/Redo、expression評価はruntime未検証。
- **次に確認:** MPM layerのsave/load・duplicate・Undo/Redoでscalar値とanimationが揃い、Composition replay結果が一致することをruntime確認する。

\n
### 2026-10-12 — MPM material reset後の通常再生は現在frameへ追いつかせる

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::setGranularMaterialSolver()` / `GranularMaterialWorldClock`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::setFramePosition()` / `replayGranularMaterialToFrame()`。
- **確認できた事実:** material再登録はworld snapshotを消去してbaselineを差し替える。従来のseek pathは新baselineから目標frameまでreplayする一方、1〜8 frameの通常再生pathは現在のlive solverをそのまま一歩進めるため、設定変更時のComposition frameが0より大きいと通常再生とseekが異なる状態を作り得た。
- **変更:** world clockに`requiresReplay`を持たせ、solver登録後最初のincremental Composition advance前に前frameまで共通replayを行う。1つのsolver解除で残solverのworld checkpointが消えた場合も同じflagを立てる。短い進行と大seek/巻き戻しの両方でrequired replayが失敗したら前frameへrollbackする。replay完了時だけflagを消す。
- **追加修正:** Composition frame rate変更はlive granular stateを保持する既存契約のため、world checkpointを破棄後にframe intervalを無視して変更時frameを強制captureする。capture成功ならそのframeをseek anchorとしてflagを消し、budget/capture失敗ならflagを残して次incremental advanceでbaseline replayへfallbackする。
- **価値／懸念:** 設定変更後の通常再生をseek時と同じbaseline/frame経路へ揃える。高いframe位置での初回再生はbaselineからのreplay時間が増える。runtime決定性・失敗回復・大frame番号の負荷は未検証。
- **次に確認:** frame Nでmaterial設定変更後の通常play N→N+1とseek N+1のparticle snapshotを比較し、world内複数MPM Layer、baseline replay失敗、最初のframe/大frame catch-upを確認する。

### 2026-10-12 — MPM frame-rate変更のseek anchor保持

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::captureGranularMaterialSnapshots()` / `trimGranularWorldSnapshots()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::setFrameRate()`
- **確認できた事実:** frame-rate変更後のlive solver stateを保つため現在frameを強制checkpointにするが、通常checkpoint上限で古いanchorを消すと、その後のseekがframe 0 baselineから再計算され、変更直後のlive stateと分岐し得る。
- **変更:** 強制checkpoint frameをworld clockのreplay anchorとして記録し、通常frame-count trimmingとbyte-budget evictionの双方で保持する。byte budgetがanchorと新規checkpointを収容できない場合は新規captureをrollbackし、既存のbaseline replay fallbackへ戻す。evictionはworld共通の同一frameを全solverから取り除く。
- **価値／懸念:** frame-rate変更時のlive-state保持と後続seekの共通起点を両立する。pinned anchorのため利用可能なrolling checkpoint数が1つ減り、予算不足ではcapture失敗が起こりやすくなる。runtime未検証。
- **次に確認:** 複数MPM Layer、低いsnapshot budget、古いanchorと通常checkpointのeviction、frame-rate変更後の現在frame以降seekで状態一致を確認する。
- **追加修正:** seekがpinned anchorより前の共通checkpointへ戻るとき、そのpinを失効させる。またreplayの到達frameでもLayerを評価してcollision proxyとgravityを同期し、復元particle stateを次stepへ正しく接続する。
- **追加修正:** Composition frame-rate setterは同一Compositionを全Layerへ再bindする。MPM Layerの同Composition bindでは有効solverを保持し、baseの`setComposition()`でTransform3D base/variantのtime scaleだけを更新する。registry missing時だけrebind recoveryでsolverを再構築する。
- **追加修正:** CPU MPM software surfaceは頂点alphaをfield段階で8-bit化後bilinear補間していたが、GPU surfaceは同じfield gridを固定diagonalのtriangle meshでpiecewise-linear補間する。CPU resampleをGPUと同じtriangle splitへ合わせ、出力pixelでalphaを量子化する。

### 2026-10-12 — MPM seek replay失敗時にframe移動を拒否

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::setFramePosition()`、`ArtifactCore/src/Physics/PhysicsSystem.cppm::hasGranularMaterialSolvers()`
- **確認できた事実:** 大seek/巻き戻しはGranular replayが失敗しても、solver再登録由来の`requiresReplay`が立っている場合だけComposition positionを戻していた。通常のbaseline/checkpoint restore failureでは古い粒子状態のままframeだけ移動し得る。
- **変更:** PhysicsSystemで対象worldのactive solver有無を判定し、solverがある状態でreplayが失敗したseekをprevious frameへrollbackする。
- **価値／懸念:** simulation frameとComposition frameのずれを防ぐ。追加の静的確認では、replayのfalse returnはcheckpoint/baselineの全solver事前検証で失敗する経路であり、replay進行ループにはfalse returnがない。checkpoint capture failureはcache missとして無視されるため、それ自体でsolver advanceが中断されるわけではない。bool契約どおりの実装を確認したが、実行時の破損状態fixtureは未検証。
- **次に確認:** baseline restore不可・共通checkpoint破損のfixtureでComposition frameが変わらず、solver stateも元に戻ることをruntime確認する。
- **追加修正:** UI/Timestamp seekが通る`goToFrame()`もGranular replayの戻り値を確認し、solverが存在するCompositionで失敗したら呼出し前frameへposition/Layerを戻し、Soft Bodyとrigid authored stateを復元してcollision/joint評価を更新する。

### 2026-10-12 — MPM surface meshのper-layer scratch memory

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::kMpmMaximumSurfaceVertices` / constructor、`Artifact/include/Render/RenderCommandBuffer.ixx::MpmSurfaceVertex`
- **確認できた事実:** 128×96 density fieldから作る130×98セルは、旧threshold-clip方式で最大12頂点/cellだった。旧6-float vertexで二本の全容量scratchは約7 MiB。RGBをpacket constant colorへ移してposition+alphaの3-float vertexとし、その後のCPU/GPU三角形補間統一で上限を6頂点/cellへ変更した。**現行:** 76,440頂点、二本の全容量scratchは1,834,560 bytes（約1.75 MiB）。GPU shader ABIは既存`RectVertex`変換後のまま。
- **未検証の仮説:** 多数の低粒子数MPM Layerでは粒子状態よりsurface scratchが先に総メモリを支配し得る。頂点payloadの半減後も、実sceneのLayer数・実頂点数・GPU packet lifetimeの計測が必要。
- **価値／懸念:** full-capacity reserveはframe-time再確保を避けてsurface生成をboundedにする。vertex縮小とtriangle cullingで旧設計よりscratchは小さいが、renderer packetを含む総memoryは未計測。
- **次に確認:** 1/10/50 MPM Layerでidle時と同時描画時のresident/capacity bytesを測り、default設定でのsurface vertex high-water markとpacket retentionを記録する。その結果からshared bounded poolへ進むか判断する。

### 2026-10-12 — Granular CFLをsolver内部stepごとに更新

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::update()` / `updateGranularTimeStepForState()`
- **確認できた事実:** 変更前はGranular CFL刻みを`update()`入口で一度決め、その後の最大1024 internal stepすべてへ使っていた。応力加速や移流が同じbatch内で速度を変えた後も、次stepの刻みは更新されなかった。
- **変更:** 初回CFL選択後、各step実行後の粒子状態から次のGranular刻みを再計算する。remaining elapsed timeが新刻みを満たさなければstepを行わず、余りを既存accumulatorへ保持する。Corotated経路とComposition outer clockは変更しない。
- **価値／懸念:** 1つの`update()`内で粒子速度が上がるケースにもstate-dependent CFLを適用する。粒子数が多い場合に毎step粒子scanが増えるためCPU負荷が増加し、CFL制約・再生一致ともruntime未検証。
- **次に確認:** 高剛性・大重力・衝突で1つのouter frameに複数solver substepが発生するsceneを用い、各stepで`dt * (waveSpeed + advectionBound) <= 0.05 * dx`を記録する。加えて同一elapsedを異なるouter frame分割で与え、粒子snapshotとaccumulatorを比較する。

### 2026-10-12 — MPM砂の弾性応力を全変形勾配へ連鎖

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::computeGridForces()`、`firstPiolaKirchhoff()`、`MpmParticle2D::{F,Fp}`。
- **確認できた事実:** Granularの`firstPiolaKirchhoff(Fe)`は`Fe=F Fp⁻¹`に対する弾性PK1 `P_e=∂ψ/∂Fe`を返す。粒子grid forceの形状関数勾配は全変形`F`に対応し、変形エネルギーを全Fで微分するには`P=P_e Fp⁻ᵀ`が必要。従来の`computeGridForces()`は非単位`Fp`でも`P_e`をそのまま使用していた。
- **変更:** Granularモデルだけ`P_e Fp⁻ᵀ`へ変換してからgrid forceへ渡す。Corotated modelの既存stress経路は変更しない。2×2行列値演算のみで追加allocationなし。
- **一次資料:** Klár et al., [Supplementary Technical Document](https://www.cs.ucr.edu/~craigs/papers/2016-sand/tech-doc.pdf), §5.2 defines `PE=∫Ω0 ψ(Fe)dX` and derives total PK1 `P=(∂ψ/∂Fe)Fp⁻ᵀ`. This confirms there is no extra `det(Fp)` multiplier under this energy convention.
- **価値／懸念:** 塑性履歴後の弾性応力力を、`Fe=F Fp⁻¹`の定義と全変形勾配に対して整合させる。独立数値例でanalytic gradientと中央差分の最大誤差は`5.1e-11`だった。solver/C++ float経路での力・運動量・塑性流動への影響は未検証。
- **次に確認:** `Fp=I`で旧Granular力と一致、Corotated経路で回帰差分なしを静的に確認し、テスト実行が明示許可された後にC++ fixtureとruntime比較を行う。

### 2026-10-12 — MPM有限ひずみCFL波速の保守性は未確認

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`、Granular Hencky弾性応力。
- **確認できた事実:** 旧CFLは各粒子の参照密度`mass/volume0`とrest-state縦波係数`lambda+2*mu`のみを使っていた。Granular応力はHencky energy `ψ=μΣ(log σi)^2 + λ/2(Σlog σi)^2`に基づくため、弾性状態のPK1接線は特異値に依存する。
- **解析:** `Fe=diag(s,1)`では`P11=(2μ+λ)log(s)/s`、主方向PK1 tangentは`(2μ+λ)(1-log(s))/s²`。PK1微分固有値に配置変換の特異値因子`s²`を掛けると、`μ=1, λ=1.5, s=0.5`で基準`λ+2μ`比は速度約1.301倍。`Fe=sI`では主特異値だけを摂動するtangentは`((2μ+λ)-(2μ+2λ)log(s))/s²`で、速度比候補は約1.411倍。以前の2.60倍値はPK1微分固有値をそのまま速度へ対応させた誤りとして撤回。
- **変更:** `granularHenckyAcousticSpeedSquaredUpperBound()`を追加。Hencky energyの2D主基底接線から方向別の2×2 acoustic tensorを組み、各Gershgorin行和を全角度に対して解析最大化する。上界とrest-state係数の大きい方をGranular CFLへ使うため、角度サンプリングによる見逃しを避ける。反復特異値は完全一致時だけ解析極限を使い、近接値では`log1p`と因数分解した平方差で評価する。CFL用singular value extractionは主方向の三角関数を計算しない。固定`Fp`で`Fe=F Fp⁻¹`を微分すると、弾性側入力方向・出力応力の双方にある`Fp`変換はcurrent-space acoustic contractionで相殺される。
- **独立照合:** 主基底の解析接線行列はPK1 rank-one中央差分（`h=1e-6`）と複数伸長・方向で最大絶対誤差約`1.5e-9`。`σ1,σ2∈exp([-3,3])`の21×21状態格子と各状態721方向を走査し、Gershgorin上界の下回りは0件。加えて`ν=-0.9,-0.5,0,0.3,0.45`ごとに`σ1,σ2∈exp([-2.5,2.5])`の13×13状態・361方向を走査し、下回り0件だった。正のE・密度は全接線と質量密度を同率scaleするため速度比走査に影響しない。対角stateでの全方向最大は一部で主軸から外れ、軸候補だけでは不足することも確認した。C++ float parity、plastic active-set/return-map後の離散安定性、runtime性能は未検証。
- **価値／懸念:** CFLは有限ひずみ弾性剛性の明示的な全方向上界を使う。Gershgorin評価は実際の最大固有値より保守的で刻みを小さくする場合がある。特異値/上界計算が失敗したstateではrest-state縦波係数へfallbackし、前stepの伸長由来の大きなdtを残さない。粒子ごとの特異値抽出と音響上界評価は毎Granular substepで走るため、実sceneのCPU費用は未計測。またこのwave boundはそのstep開始時の弾性predictorを線形化したもの。plastic projection・衝突・MPM transfer由来の追加安定制限までは包含しない。
- **出典:** Sun, Shinar & Schroeder, *Time step restrictions for MPM* (2020), [paper](https://cs.ucr.edu/~shinar/papers/2020_explicit_mpm_dt.pdf) は、rest-configurationの線形音速は有限変形で過小または過大になり得ること、一般等方超弾性で特性音速を導出することを述べる。また音速は`J`だけに依存しない例を示す。これは本件の状態依存CFL必要性の根拠であり、本Layerの2D Drucker–Prager塑性接線や積分安定性を直接証明しない。
- **次に確認:** C++ float実装値と独立差分を比較し、active Granular solverで実際に選ばれたdtが`dt*(waveUpperBound+advectionBound)<=0.05*dx`を満たすことを許可後に確認する。plastic projection・grid transfer・collisionの固有安定性は別gateとして扱う。

### 2026-10-12 — Granular外部Collider補正後の変形勾配

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::stepOnce()` / `resolveColliders()`
- **確認できた事実:** Granular stepはG2Pで速度勾配から`F`を更新し、塑性return mapを適用した後に`resolveColliders()`が粒子位置・速度・APIC `C`を補正する。外部Colliderによるpenetration correctionはそのstepの`F`/`Fp`/hardening historyを巻き戻さない。既存の最終state guardはfinite/正determinantなどの破損回避を行うが、strain量の妥当性は評価していない。
- **未検証の仮説:** 粒子が外部Colliderへ一時貫入したstepで、衝突前速度勾配由来の圧縮・塑性硬化が残り、単なる接触位置補正より強い局所圧密を作る可能性がある。一方、通常の接触応力として妥当な場合もあり、コードだけでは不具合と断定できない。
- **価値／懸念:** 粒子非貫通・速度反発だけでなく、砂の体積変化と塑性履歴が接触応答として自然か確認する必要を示す。Fを接触後に単純rollbackすると接触の応力経路まで消すため、測定なしに補正式を導入しない。
- **次に確認:** 固定seedで外部collider衝突とdomain wall衝突を比較し、接触前後の`det(F)`、`det(Fp)`、hardening、反発、積算体積を記録する。colliderなし同等stepと比べ、penetration projectionだけに起因する過剰変形が再現した場合は、grid-side contactを含む責務を設計する。

### 2026-10-12 — CFL下限到達時に以前の大きな刻みを残さない

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`。
- **確認できた事実:** wave speedだけで`0.05*dx/1e-6`を超えると、従来は関数がreturnし、前回の`fixedDt_`が残った。Sand layer生成APIのE/ν/密度制約と初期step budgetの範囲では、独立した101×101 log-stretch走査（ν=-0.9,-0.5,0,0.3,0.45）でrest縦波係数に対する最大上界比は約164.6、受理可能な初期speed/dx目安768に対し最小刻み限界は50000だった。この走査はfloat演算や塑性更新を含まず、C++ runtime検証ではない。
- **変更:** wave speedだけで最小刻みのCFL予算を超えた場合、前回値を残すより最小刻みへ落とす。CFL条件そのものは満たせないので、これは過大な既存刻みを避けるfail-safeであり、安定性保証ではない。
- **価値／懸念:** 異常または低レベルAPI経由の極端な状態で前step由来の大きなdtを保持しない。最小刻みでも波動CFL違反が残り得る。低レベルAPI材料設定を含む到達性とruntime結果は未確認。
- **次に確認:** C++ float実装で極端な伸長・圧縮と直接setMaterial設定を走査し、最小刻みfallback、エラー診断、回復挙動を確認する。

### 2026-10-12 — MPM Sandの表示fieldは塑性せん断形状を直接描かない

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::writeDensityField()`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`。
- **確認できた事実:** density fieldの各寄与は現在体積`volume0*det(F)`を使うが、supportはscalar radiusから作るaxis-aligned kernelで、`F`の主方向・せん断成分を描画kernelへ反映しない。Layer描画はそのfieldをalpha surfaceへ変換する。
- **未検証の仮説:** 強いせん断・伸長した領域でも個々のquadrature particle supportは円形のままなので、粗いfield解像度では局所輪郭の引き延ばしが弱く見える可能性がある。ただしMPM material pointは物理的な粒子形状ではなく積分点であり、変形楕円を描く方が材料連続体の見た目に正しいとは限らない。
- **価値／懸念:** simulation stateの`F`とrender surfaceの再構成を混同しない。粒子kernelを変えるとGPU/CPU parity、field normalization、支持範囲・hot-path費用にも影響するため、表示改善は受入sceneでの比較が先。
- **次に確認:** 単純せん断・一軸伸長・圧縮の固定stateで、現行scalar kernel、変形計量を使うellipse kernel、より高解像度のfieldを同一scene上で可視比較し、輪郭の連続性とCPU費用を測る。

### 2026-10-12 — MPM Sandの境界摩擦は内部摩擦角と独立の固定値

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`、`ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`、`ArtifactCore/src/Physics/MpmSolver2D.cppm::applyBoundaryConditions()`。
- **確認できた事実:** 外部collision sourceからMPM colliderを組む際に`friction=0.2`を設定し、Granular Layerのsolver domain wallには`boundaryFriction=0.25`を設定する。Layerの`frictionAngleDegrees`は別物性としてDrucker–Prager内部yieldへ使われる。現在のMPM Sand property groupに外部collider／domain wall摩擦の編集項目はない。
- **未検証の仮説:** 外部接触面や砂箱壁の滑りを内部安息角とは別に調整したい制作sceneでは、固定値がチューニングを制限する可能性がある。既存Collision componentの摩擦編集契約が見当たらないため、いきなり共通UIまたはLayer専用設定を増やす根拠はまだない。
- **価値／懸念:** 材料内部摩擦と境界接線応答を混同しない。外部shapeの既定値は既存Mpm colliderの固定係数とも関係するため、変更する場合は他のmaterial solverへの影響を避ける設計が必要。
- **次に確認:** 受入sceneでdomain wallと外部colliderごとの滑りを評価し、必要なら独立設定のowner・保存先・既存Collision UIとの関係を決める。

### 2026-10-12 — MPM Sandのsource boundsは現在simulation enclosureでもある

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::configureSolver()` / `draw()` / `getSoftwareRenderSurface()`、`ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`、`ArtifactCore/src/Physics/MpmSolver2D.cppm::stepOnce()`。
- **確認できた事実:** seedはsource sizeの60%×35%領域へ置かれる一方、granular solver domain/boundaryはsource rectangle全体`[0,width]×[0,height]`。CPU/GPU描画fieldも同じsource rectangleに限定され、solverはparticle-level wall clampでdomain外の粒子中心を止める。
- **解釈:** seed rectangleの外へ下方に広がる砂は現domain内で描画できる。source rectangleそのものを越える砂は物理境界で停止するため、現在のコードにおいてrender culling/clipによる粒子消失とは異なる。
- **未検証の仕様差:** milestoneの「初期Layer rectangle外へ広がってもclip/cullされない」がseed boundsを指すなら現構造と整合する。source boundsを越えるopen-domain sandを指すなら、有限grid/domain拡張、snapshot geometry契約、CPU/GPU/Render Queueの描画origin・boundsを一体で設計する必要があり、現実装では未達。どちらを受入契約とするかはコードだけから確定できない。
- **次に確認:** 受入sceneでLayer rectangleを砂箱の壁として使うか、描画/transform boundsの外へ伸びるopen-domainとして使うかを決める。その判断まではsolver wallやLayer geometryの意味を推測で変更しない。

### 2026-10-12 — 非アクティブMPMレイヤーは再アクティブ化まで時間を止める

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`、`ArtifactCore/src/Physics/PhysicsSystem.cppm::advanceGranularMaterialSolvers()`
- **確認できた事実:** Composition上でLayerがinactiveのframeでは、そのMPM solverだけworld clockのstep対象から外れる。world clockは他のactive solverとともに進み、Layerが再びactiveになるとsolverは保存されていた粒子状態から進行を再開する。seek replayでは各frameのLayer activityを再評価する。
- **未検証の仮説:** visibility/activityをシミュレーションpauseとして扱う意図なら一貫している。非表示でも物理時間を進める期待なら、復帰時のcatch-up仕様が必要になる。
- **価値／懸念:** 一つのComposition world内でLayerごとに時間経過が異なるため、衝突相手がactive期間中だけ相互作用する。pauseとcatch-upのどちらを製品挙動にするかはまだ確認されていない。
- **次に確認:** Layerのin/out、enabled/visibility、通常再生とseekで「inactive中の砂」を止める仕様か、時間を進める仕様かを決め、必要なら受入fixtureへ追加する。
### 2026-10-12 — Video previewはfloat変換後のupload再量子化が重なる

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::cpuVideoFrameToImageF32x4_RGBA()`、`ArtifactCore/include/Image/ImageF32x4_RGBA.ixx::setFromCVMatOwned()`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx::convertSurfacePixels()`、`Artifact/src/Render/PrimitiveRenderer2D.cppm::drawSpriteTransformed()`。
- **確認できた事実:** 独立Composition Preview BenchのH.264 1080p30では、RGB24からfloat化するCPU変換が所有権移譲前約19–23ms、移譲後約6.5msとなった。一方、そのfloat画像をRGBA8 sRGB textureへ変換するupload conversionに約10–12ms、texture作成に約2–4msを要した。最適化後もH.264 1080p30/720p30とProRes 422 HQの1・2 layer strict benchはいずれも不合格だった。H.264 decoderはFFmpeg software backendで動作した。
- **未検証の仮説:** RGB24をfloat bufferに展開してからRGBA8 textureへ戻す二段変換を避ける動画専用byte-surface経路、またはGPU側での色変換を導入すれば帯域とCPU時間を削減できる可能性がある。ただし`ImageF32x4_RGBA`は他の合成・プレビューAPIからfloat surfaceとしても利用されるため、単純な内部storage形式変更は互換性を壊す恐れがある。
- **価値／懸念:** CPU変換とtexture uploadの双方が実測でフレーム予算を使っていると切り分けられた。RAM previewの追試は準備中に約1.1 GiBへ達して停止しており、キャッシュ利用での救済は未確認。
- **次に確認:** 動画Layer専用の8-bit frame payloadを導入する場合、color transform、shared payload、frame cache、software fallback、Composition preview、export/renderの各consumerを列挙し、float APIへ変換する箇所を境界で明示する。Diligentのdynamic texture update／staging reuseとGPU decodeの現状も、renderer/backendの契約を確認してから比較する。

### 2026-10-12 — 動画preview texture slot再利用とbounded async変換候補

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm::textureForVideoFrame()`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** 独立benchのH.264 1080p30で、動画Layer IDと寸法ごとにGPU textureを保持し、同一frame再描画時は転送を省き、新frameでは既存slotを`UpdateTexture`するbounded poolを追加した。既存texture cache上限は50件／512 MiB。profileは初回slot作成約1ms、更新約0–1ms、毎新frameのfloat→RGBA8 CPU conversion約11msを示す。追試2回の1/2 layer dropped/late/repeatedは1/1/10・3/7/8、1/2/13・3/6/4でいずれもstrict不合格。実行差があるため安定したend-to-end改善は未確認。
- **未検証の仮説:** CPU pixel conversionをdecode後のworkerへ移し、3個程度のfixed upload bufferとgeneration tagで結果を捨てればrender laneの11msを隠せる可能性がある。ただしdecode worker競合や追加memoryがあるため性能改善は未証明。Diligent context/texture操作をworkerへ移す設計は所有権・同期上の危険がある。
- **価値／懸念:** texture作成再利用は入ったが、ユーザーの無drop/late/repeat条件を満たしていない。1080p RGBA8は約8 MiB/frameなので、全RAM frameにsidecarを持つとpreview memoryが大きく増える。async化するなら固定数buffer、bounded backpressure、stale generation rejection、queue saturation counterを含め、GPU更新をrender ownerに留める必要がある。
- **次に確認:** bounded async conversionを加える前にdecoder frame lifetime、既存task poolの低遅延task経路、frame generation/cancel境界、buffer lease lifetimeを実コード上で確認する。導入後は同一独立benchを複数回走らせ、drop/late/repeated=0を達成するまで採用扱いにしない。

### 2026-10-12 — FFmpeg D3D11VAはComposition direct decodeで動作、preview条件は未達

- **関連:** `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`tools/VideoPreviewBench/main.cpp`。
- **確認できた事実:** 同梱FFmpeg CLIでH.264 1080p30を5回ずつdecode。各run 90/90 frames。median wall timeはsoftware 0.18 s、D3D11VA 0.16 s、CUDA 0.16 s（GPU frameをNV12でCPUへdownloadする処理込み）。verbose logのhardware output formatはD3D11=`d3d11`、CUDA=`cuda`。ProRes HQはsoftwareのみ90/90 frames、median 0.14 sでD3D11VA hardware frameなし。
- **今回の試行:** `MediaImageFrameDecoder`にopt-in D3D11VA device、format選択、D3D11 frame download経路を実装し、benchに`--ffmpeg-hwaccel d3d11va`を追加した。`VideoLayer` direct decode requestからcontroller/direct decoderまでstderr traceし、D3D11VA device init=0、H.264 `AV_PIX_FMT_D3D11` frame receipt、CPU presentationへのdownloadを実証。GPU指定はdirect decoderだけへ渡し、通常video decoderやproxy controllerの不要なD3D device作成を避ける。ビルド成功。
- **確認できた事実:** 同一bench A/Bのraw decode profile medianはsoftware 22.37ms、D3D11VA 10.63ms。CPU image conversionは6.62ms／6.73ms。drop/late/repeatはsoftware 1 layer=4/1/13、2 layers=1/6/5、D3D11VA 1 layer=2/1/9、2 layers=2/13/4。D3D11VAはdecode時間をおよそ半減したが、厳格な無drop条件は未達で全counterの改善は混在。GPU framesはCPUへdownload後にfloat化し、rendererでさらにRGBA8 upload conversionする。
- **価値／懸念:** アプリのFFmpeg direct decodeでGPU accelerationは動作するがzero-copyではない。download、CPU conversion、renderer upload conversionが残る。GPU decodeの適用だけではproduction playback acceptanceを満たさない。
- **次に確認:** worker側のbounded async upload conversionを設計・実装し、複数回A/Bでdrop/late/repeat=0を狙う。別案としてD3D12VAとDiligentのresource/device共有も調査するが、異なるdeviceの受け渡し・同期を確認するまでdirect bindingしない。

### 2026-10-12 — Video upload scratch buffer再利用

- **関連:** `ArtifactCore/include/Image/SurfacePixelConversion.ixx`、`ArtifactCore/include/Image/ImageUploadConversion.ixx`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** 動画フレームのfloat→RGBA8同期変換は、描画スレッド上で毎回新しいvectorを生成していた。変換先bufferを書き換えるAPIを追加し、render thread-localの64 MiB上限slotを再利用するようにした。超過サイズはone-shot bufferにし、slotを拡大しない。Debug bench targetはビルド成功。H.264 1080p30 D3D11VAで2回測定し、1/2 layerのdropped/late/repeatedは2/1/6・0/13/3、および2/1/6・3/7/3。新frameの同期変換時間は約9–11msで、strict playback条件は未達。
- **未検証の仮説:** bounded allocation reuseによりheap contentionは減り得るが、今回のbenchはallocation回数を計測しておらず、end-to-endの明確な改善は確認できない。変換自体はまだ同期である。
- **価値／懸念:** 1080p/4Kで一時vectorの反復割当を避け、巨大フレームを長期保持しない。一方、ゼロドロップ改善にはデコード先読み結果を用いたworker側変換が必要で、準備完了状態、stale generation破棄、固定容量backpressureを一緒に設計する必要がある。
- **進捗／未検証:** decode workerでdecoded/prefetched frameを変換し、video layerごと3 frame / 64 MiBに制限するupload cache、別個のframe-ready判定、およびprepared uploadをrendererへ渡す経路を実装した。Diligent resource操作はrenderer owner threadに残した。動画Layer・Composition render controller・renderer・bench sourceのコンパイルは通過したが、ArtifactAppRuntime link時にlinkerが約10 GiBを使用し、システム空き物理memoryが1 GiB未満となったためbuildを中断した。新コードのruntime/A-B benchは未実施。
- **次に確認:** メモリ負荷を抑えてArtifactAppRuntimeをリンクできる構成を確認し、ビルド完了後にshared sourceの複数layer、seek/source-version切り替え、4Kのcache budget、drop/late/repeated=0を独立benchで検証する。

### 2026-10-12 — VideoLayer upload conversion buffer pool

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::Impl::cacheUploadFrame()`。
- **確認できた事実:** upload sidecarはlayerごと4 frame / 64 MiB以内で保持され、呼び出しごとに新規`ImageUploadBuffer`を確保していた。`convertImageForUploadInto()`は既存bufferへ容量を保って書き込める。`SharedPtr::useCount()`でrenderer等がまだ参照中のbufferを識別できる。
- **変更:** upload cacheから退避され、他の利用者が保持していないbufferを最大4個・64 MiBのbounded poolへ戻し、次のsidecar変換で再利用する。変換cacheもlayerごと最大64 MiB。Diligent texture操作は変更せずrender owner threadに残す。
- **価値／懸念:** sidecar入替時のheap/vector再確保を抑える狙い。これはバッファ再利用であり、CPU pixel conversion自体は依然decode worker上で同期実行されるため、decode throughputやno-drop acceptanceへの効果は未計測。
- **次に確認:** 対象moduleのコンパイル後、pool再利用回数・保持容量をprofileし、同一H.264 1/2 layerでD3D11VA/software両方を反復ベンチする。次段階の独立変換workerはbounded queue、generation cancel、layer destruction joinを揃えてから測定する。
- **追試結果:** 対象module単体コンパイルと手動差分linkは成功。H.264 1080p30のsingle-run D3D11VAは1 layer=2 drop/1 late/11 repeated/30 final、2 layers=0/7/1/13。softwareは1 layer=3/1/13/28、2 layers=1/8/2/11。各約3秒でstrict gate不合格。従ってpool再利用・GPU decodeともend-to-end改善は証明されず、preview no-dropは未達。

### 2026-10-12 — Video upload conversion bounded worker trial

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::Impl::enqueueUploadFrame()`、`cacheUploadFrame()`、`cancelPendingUploadConversions()`。
- **変更:** decode workerからsidecar conversionを外し、専用single-thread `QThreadPool`へ送る。layerごと最大4 requests、current frameをprefetchより優先、世代不一致結果を破棄。source/destruction cancelはworker join後に戻る。decoder prefetchを3から1へ減らす実験も含む。
- **確認できた事実:** 対象C++ module compile、bench incremental link、standalone process実行は成功。H.264 1080p30のD3D11VA 1/2 layerは2/1/5/12、1/4/0/6、softwareは2/2/5/13、1/1/2/5 (drop/late/repeated/final frame)。ProRes 422 HQ softwareは5/2/7/14、0/5/0/6。全て3秒測定でstrict gate不合格。D3D11VA device init/frame receiveは続けて確認した。
- **価値／懸念:** 変換とGPU texture操作のthread ownershipを分けたが、sidecar未準備frameがrenderに到達する場合があり、fallback samplesは1x1 placeholderだった。decode進行も5–14 frame程度に止まり、single-thread化・prefetch縮小・GPU decodeのどれも安定したend-to-end改善を示していない。結果のばらつきとearly-frame seek/startup costも残る。
- **次に確認:** playback tick、render CPU duration、decode request/coalescing、upload readinessを同一timestampで記録し、実際に支配するstallを特定する。必要ならGPU texture conversionまたはFFmpeg/Diligent間の共有可能なresource経路を別途評価するが、backend/device synchronizationを確認するまでDiligent resourceをdecode workerから触らない。

### 2026-10-12 — Video preview shared RGBA32F copy stall

- **2026-10-13追記（先読み）:** `decodeCurrentFrame()` の先読み分岐には値cacheへのcloneが残っていた。upload向け共有payload確定後に `putShared()` を使う最小変更を実施。frame／byte上限は同じ。通常要求側と同じ所有権を再利用するが、変更後の固有H.264 1layer10秒は提示147、late108で、性能改善は確認できず。変更前は提示145、late106。

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::Impl::prepareDecodeRequest()`、`FrameCache`、`ArtifactCompositionRenderController`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** 1080p RGBA32F shared-frame lookupは約0.03msだが、`currentFrameBuffer_`へ複製する処理は約11.2ms、直後の`FrameCache::put()`で再度複製する処理は約10.7–11.2msだった。composition simulation eval内で周期的に合計約22–23msを占め、decode/upload準備待ちの再評価でも大きいコピーが起きていた。
- **変更・検証:** shared frameを`FrameCache::putShared()`で再利用し、upload sidecar準備済み・入力色変換なしならCPU frame bufferへのコピーを遅延する。CPU getterはshared frameをmaterializeできる。upload sidecar未準備時に同じdecoded frameを再要求する経路もskipし、upload readiness確認より前にRAM cacheをdeep copyしないよう順序を変更した。手動module compile/link後の独立benchでD3D11VA H.264の最大component simulation時間は約22–23msから6.5msに低下。
- **価値／懸念:** 大きなUI-thread stallの一つを約15ms以上削ったが、1/2 layerともstrict gate不合格。H.264の平均render frameは約31.6–46.0msで、decode新規frame 9–15msとCPU pixel conversion 6.5–7.3msも残る。uploadが間に合わず1x1 placeholderを使ったrender sampleがあり、GPU timestampも未取得。
- **FFmpeg GPU decode確認:** H.264 D3D11VAはdevice initと`AV_PIX_FMT_D3D11` frame receiptを確認したが、現presentation pathはCPU downloadを行う。ProResではD3D11VA device initは通るがhardware frame receiptなし。どちらも無drop/late/repeatの acceptance は満たさない。
- **未検証の仮説／次に確認:** GPU upload sidecarが遅れた時に現在の1x1 upload fallbackがなぜ選択されるか、GPU timestampまたはCPU present-wait計測で`Layer Surface Pass` / presentの実行時間を分離する。FFmpeg D3D11VAからDiligent D3D12へのshared-resource連携はdevice違いと同期方式の確認が必要で、未検証のまま結合しない。

### 2026-10-12 — Render-loop project diagnostics caused periodic preview stalls

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`ArtifactProjectService::currentProjectDiagnostics()`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** Composition render loopから毎秒（composition変更時も）`currentProjectDiagnostics()`を呼び、project health / app / audio診断を集約していた。結果は advisory で、エラーでも描画は続行し、render path summaryとwarningにだけ使われていた。profileした呼び出しは12.8–100.7msで、周期的な`preSceneMs` spikeと対応した。
- **変更・検証:** 診断呼び出しと未使用キャッシュをrender loopから削除し、診断dashboard/APIは維持した。render controller module compileとbench relinkは成功。修正後のH.264 1080p30 / D3D11VA / 2 layersでは`preSceneMs`約0.00–0.01msとなり、周期的なproject diagnostic stallは消えた。
- **価値／懸念:** UI/render threadを最大約100ms止めていたadvisory処理を除去できた。一方3.259秒の独立benchはdrop=2、late=2、repeat=3、final frame=7/89でstrict gate不合格。render frame平均は約22–34ms、個別76–81ms、GPU timingは引き続き未取得。非同期化やpool増量は主問題の解決に足りず、simulation/render/presentとdecode/upload pacingの同時計測が次の切り分け対象。
- **次に確認:** component simulationの`goToFrame`内、Layer Surface Pass、present待ち、decode request/ready時刻を同一frame IDで揃えてprofileし、最大76–81ms sampleの寄与を分解する。diagnostic workerへの移動はproject snapshot/thread safetyが確認できるまで行わない。

### 2026-10-12 — FFmpeg GPU decode A/B and CPU download boundary

- **関連:** `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`Artifact/src/Layer/ArtifactVideoLayer.cppm`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** H.264 1080p30独立Composition benchで、software / D3D11VA 1 layerはdrop/late/repeat=0/1/6・1/0/6、2 layersは0/4/1・0/5/0。ProRes HQは1 layer=3/2/10・3/2/9、2 layers=1/2/3・0/1/5。すべて3秒前後のrunでstrict gate不合格。H.264 D3D11VAはdevice initと1920x1080 hardware frame受信を確認したが、`MediaImageFrameDecoder`がCPU frameへdownloadしておりzero-copyではない。ProResはdevice initのみでhardware frame受信なし。
- **FFmpeg CLI:** 90-frame H.264 warm run medianはsoftware=231.3ms、D3D11VA surface=348.4ms、CUDA surface=282.6ms。これは当該clipにおいてGPU decodeがCLI decode wall-timeも短縮しない結果。正しい`-hwaccel_output_format d3d11`でProRes 2 framesをdecodeした時はsoftware pixel formatを出力し、app pathでもhardware frameを受け取らなかったためsoftware fallbackと判断。ただしdecoderがhardware pathを選ばない具体理由は未特定。過去runと値の変動があり、恒常的な速度比は未確定。
- **価値／懸念:** GPU pathが実アプリで動作するcodecと、CPUへdownloadする境界を確認できた。GPU decode有効化だけではpreview gateに近づく安定した改善を実証できていない。共有payload cache hit時はdirect decodeを避けるため、layer count間のGPU/CPU A/Bには経路混在があり得る。
- **次に確認:** D3D11 decode surfaceのpixel formatとDiligent D3D12/Vulkan texture upload契約を調べ、CPU download＋float conversionを避ける共有可能なresource/interoperability経路があるか小さく実証する。併せて同一timestampでdecode ready、upload ready、render submit、presentを結ぶtraceを追加し、strict gateが失敗する主因をframe単位で特定する。

### 2026-10-12 — Vulkan FFmpeg decoder probe

- **関連:** `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`tools/VideoPreviewBench/main.cpp`、`tools/VideoPreviewBench/README.md`。
- **変更:** standalone benchに`--ffmpeg-hwaccel vulkan`を追加し、FFmpeg Vulkan hwdeviceを作成するdecoder pathを試験可能にした。hardware AVVkFrameのdirect presentationは既存のsemaphore同期制約により有効化していない。
- **確認できた事実:** Artifact同梱FFmpegのvcpkg Debug/Release configは`CONFIG_VULKAN=0`、`CONFIG_H264_VULKAN_HWACCEL=0`、一方`CONFIG_D3D11VA=1`、`CONFIG_D3D12VA=1`。このため`av_hwdevice_ctx_alloc(VULKAN)`が利用不可となり、`av_hwdevice_ctx_create`は`AVERROR(ENOMEM)`を返していた。Vulkan runtime列挙による事前判定を追加した。1 layer=3/1/6、2 layers=0/3/1 drop/late/repeatの旧runはsoftware fallbackでありVulkan性能結果ではない。別インストールのFFmpeg CLIはH.264のVulkan output (`pix_fmt=vulkan`)で90/90 framesを5回decodeし、median 460.1ms。software CLI medianは265.5ms。同梱と外部CLIは別build/runtimeである。
- **価値／懸念:** FFmpeg Vulkan decode capabilityは別CLI build側にあるが、Artifact同梱packageにはVulkan backendが組み込まれていない。外部CLIのGPU surface出力もsoftware比で遅く、app playback改善の根拠にはならない。
- **次に確認:** package featureを変更する場合はFFmpeg Vulkan/H.264 Vulkan hwaccelを有効にした専用buildを用意し、Diligentの同一device/queueをFFmpegへ渡す経路とFFmpeg timeline semaphoreのwait/signal ownershipを解決してbenchで検証する。

### 2026-10-12 — FFmpeg D3D12VA decode is operational but does not improve preview

- **関連:** `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`tools/VideoPreviewBench/main.cpp`、`libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h`。
- **変更:** standalone benchからD3D12VAを要求できるようdecoder初期化、pixel-format選択、およびD3D12 hardware frame downloadを追加した。
- **確認できた事実:** bundled FFmpeg configで`CONFIG_D3D12VA=1`、`CONFIG_H264_D3D12VA_HWACCEL=1`。app direct pathはD3D12VA device init、decoder attach、1920x1080 `AV_PIX_FMT_D3D12` framesを受信し、CPU download後にpreview bufferを作った。同一build no-RAM-cache runのsoftware/D3D11VA/D3D12VA結果は1 layer drop/late/repeat=2/3/7・0/1/7・2/2/6、2 layers=1/3/2・0/3/2・0/4/1。render averageは1 layer=37.8/41.5/39.8ms、2 layers=24.6/37.2/42.5ms。全run strict gate不合格。External FFmpeg CLIの90-frame medianはsoftware 229.7ms、D3D12VA 432.3ms。
- **価値／懸念:** D3D12VAは実アプリで使えるが、今回のCPU-download経路はsoftware比較より遅く、安定したpreview改善を示さない。`AVD3D12VAFrame`には`ID3D12Resource`とfence/valueがありnative wrapper候補に見えるが、今回のFFmpeg decoderは独自deviceを作るためDiligent device resourceとして直接使える証拠はない。
- **次に確認:** Diligentの`CreateTextureFromD3DResource`要件、rendererから取得するdevice/queue、FFmpeg D3D12VA shared-device context、D3D12 video fence wait/signal順序を突き合わせる。同一deviceを安全に使える構成が確認できたら、CPU download無しのbounded experimentを作り、GPU resource lifetimeを保ったまま独立benchで比較する。

### 2026-10-12 — Video upload conversion poolのworker数を増やしてもstrict previewは改善しない

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::videoUploadConversionThreadPool()`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** H.264 1080p30の独立benchで、1 worker版と2 worker版をsoftware / D3D12VA、1 / 2 layerで各1 run比較した。2 worker版のdrop/late/repeatはsoftware 1/2 layer=1/2/8・0/7/2、D3D12VA=3/2/5・0/5/1で全case strict不合格。平均描画時間も2 layerで約35.9ms / 51.3msだった。worker上限は1へ戻した。
- **未検証の仮説:** 専用poolのタスクがレイヤー単位の連続queue drainになっているため、workerを増やしてもdecode、renderer submit、presentとのCPU競合が勝る可能性がある。今回単発runだけでは原因を確定できない。
- **価値／懸念:** worker数とpool容量を増やすだけでは、遅延frameや再生range未到達を解消できないと分かった。非同期化の成否は「worker実行中」ではなく、対象frameがdeadline前にupload-readyとなった比率で測る必要がある。
- **次に確認:** decode request・decode ready・upload enqueue/start/ready・render queued・presentを同じframe IDとmonotonic timestampで記録し、queue待ち、CPU処理、present同期のどこで約数百msの遅れが生じるか切り分ける。

### 2026-10-12 — D3D12VA readback timing is measurable but not the whole stall

- **関連:** `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm::downloadHwFrameToCpuVideoFrame()`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** 1080p30 H.264 / 1 layer standalone runで、D3D12VAのwarm `av_hwframe_transfer_data()` medianは約1.41ms（最初は8.76ms）、その後のCPU RGB変換medianは約5.11ms。renderer RGBA upload conversion profileは多くが1ms未満、texture updateは0–2ms。D3D12 frame descriptorからresource pointerとfence valueも読めた。
- **価値／懸念:** GPU→CPU readbackは短く無視できないが、再生遅延を単独で説明しない。decode worker全体やsoftware decodeも高い/ばらつきがあり、短い1回ずつの比較ではbackend優位を断定できない。現アプリ経路はGPU→CPU→CPU色変換→GPU uploadで、GPU間直接転送は未計測。
- **次に確認:** requested/presented frame IDをキーに、packet/decode完了、hardware readback、CPU conversion、upload-ready、render queue、presentを同一monotonic clockで測る。別個にFFmpeg deviceとDiligent deviceのidentityを比較し、同じ場合のみD3D12 resource state・decode fence・frame lifetimeを含むdirect GPU pathの小さな実現性probeを設計する。

### 2026-10-12 — FFmpeg can decode on the Composition renderer's D3D12 device

- **関連:** `tools/VideoPreviewBench/main.cpp::runSharedD3D12DecodeProbe()`、`DiligentDeviceManager::acquireSharedRenderDeviceForCurrentBackend()`、FFmpeg `AVD3D12VADeviceContext`。
- **確認できた事実:** standalone benchでComposition rendererのnative `ID3D12Device`をFFmpegのD3D12VA contextへ設定し、H.264の1920×1080 `AV_PIX_FMT_D3D12` frameをdecodeできた。frame textureの`GetDevice()` pointerはrenderer deviceと完全一致 (`sameDevice=1`)。native resource formatはNV12。plane SRV compute shaderでRGBA8へ変換、出力をDiligentでwrapできた。固定3枠ringで各slotのoutput/descriptors/allocator/listを再利用し、source `AVFrame` cloneをcompletion fence retireまで保持する。さらにDiligent immediate queue (`LockCommandQueue`) の `WaitFence` / `Submit` / `EnqueueSignal` を使い、source wait・conversion command list・完了signalを同じDiligent queue上で順序づけた。独立runを3回行い、毎回8/8 submit・complete、PTS単調、sample pixel検証成功、`maxInFlightSlots=3`。elapsedは143–199ms。1 runのframe 3 slot waitは1.36ms、frame 4–7は0.005–0.011ms、enqueue wallは2.69–3.65ms。初回setup/sample validationは約80ms。FFmpeg decoder fenceはsubmit時にすでにcompleteだった。
- **価値／懸念:** device跨ぎ共有のためのexternal heap/handle copyは不要で、Diligent所有queueへのbounded async GPU conversion submissionが成立することを確認した。ただしprobeは固定ringの変換だけで、Diligent state trackingを伴う実draw、decodeとGPU workの同時進行、vsync-paced再生、drop/late/repeatは未検証。コード上、`GPUTextureCacheManager::acquireOrCreate(GpuVideoFrame)` は現在Vulkan image以外を拒否し、D3D12 NV12はRGBA sprite pathに入らない。
- **次に確認:** D3D12 completion fenceを非blockingにpollし、slot枯渇時はdefer可能にする bounded conversion cacheを、既存draw packetのowned queue/state境界に接続する設計を確定する。resource state ownershipとcache invalidationを明示し、その後standalone app playbackで1/2+ layer、H.264/ProRes、cache on/offのstrict frame gateを測り、ゼロdrop/late/repeatを確認する。

### 2026-10-12 — Production media controller shared D3D12 device hook

- **関連:** `ArtifactCore/include/Media/MediaPlaybackController.ixx`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`、`tools/VideoPreviewBench/main.cpp`。
- **変更:** `MediaPlaybackController`にnative D3D12 device setterを追加し、COM参照を保持してdirect FFmpeg D3D12VA decoderへ渡す経路を追加した。GPU frame保持は `getVideoFrameAtFrameDirectRaw(frame, true)` の呼び出し単位で指定し、従来の1引数APIはCPU readback動作のまま維持した。
- **確認できた事実:** 変更した `MediaImageFrameDecoder.ixx`、`MediaPlaybackController.ixx`、両実装module、bench mainを手動コンパイル・再リンクし、standalone probeを実行した。H.264 1920x1080のcontroller frameは `storage=3` (D3D12Texture) でrenderer deviceと一致 (`sameDevice=1`)。続く固定3枠 ringのprobeは8/8 frameを変換・完了し、PTS monotonic、sample pixel検証成功、elapsed 132msだった。1回のcapability probe結果であり、安定した性能測定ではない。`NOMINMAX` でD3D12 Windows macroと `std::max/min` の衝突を除去した。
- **未検証:** ArtifactVideoLayerからshared deviceを供給する製品経路、D3D12 NV12を `GPUTextureCacheManager` とsprite drawへ渡す経路、実draw、vsync playback、再生性能、strict frame gateは未確認。通常の既存raw decoder APIは引き続きCPU readbackする。
- **価値／懸念:** controller境界でshared-device hardware frameのnative identityとlifetime保持を実証した。COM lifetimeはcontroller保持中とdecoder hwdeviceの保持中の両方で必要。次にlayer/render-device供給境界をつなぎ、Diligent state ownershipとNV12 conversion cacheを経て製品drawで検証する。

### 2026-10-12 — D3D12 video conversion slot needs post-draw retirement

- **関連:** `Artifact/src/Render/GPUTextureCacheManager.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm::presentAndUpdateVideoLayers()`、`tools/VideoPreviewBench/main.cpp::enqueueNv12ComputeConversionProbe()`。
- **確認できた事実:** 現在のGPUTextureCacheManagerは`GpuVideoFrame`入力でVulkan imageだけを受け付ける。standalone probeはDiligent immediate queueにNV12→RGBA computeを投入できるが、probeにはsprite drawがない。Composition rendererはdraw packetをsubmitした後にPresentし、Diligent contextの`FinishFrame()`を呼ぶ。
- **未検証の設計仮説:** conversion fence完了は出力textureが後続sprite drawから解放された証拠にならない。同じ固定出力slotを再利用する場合、当該slotを読んだ最後のdrawより後にDiligent queueへretire signalを出し、そのsignalが完了するまで再利用をdeferする必要がある。Present後signalとslot数でbounded poolを保つ案が有力だが、render graph複数経路とcache/device resetのretirement契約は未検証。
- **価値／懸念:** conversion-only fenceだけでoutput ringを実装すると、描画がまだ読んでいるtextureを次のframeで上書きする競合が起こり得る。最終draw後のretirementとpool枯渇時のdeferを先に設計し、同期waitやone-shot resource生成へ逃がさない。
- **次に確認:** 全composition present経路でcache end-frame signalを置ける位置を特定し、Diligent immediate queueのsignalがPresent後に順序化されること、frame slotの同一submission内でconversion→draw→retireの順になること、device loss時のsource AVFrame lifetimeを確認する。差分の後は1/2層でconverter slot busy率とfence retire latencyを計測する。

### 2026-10-12 — Product D3D12 video path requires slot-key invalidation and shutdown retirement

- **関連:** `Artifact/src/Render/GPUTextureCacheManager.cppm::D3D12VideoFrameConversionPool`、`Artifact/src/Layer/ArtifactVideoLayer.cppm::decodeFrameToGpuFrame()`。
- **確認できた事実:** 実装接続中、再利用可能slotを上書きする前にそのslotを指すcache entryを外さないと、古いframe keyが更新後の内容を返す。slot選択時に既存entry IDを消してから変換する経路を追加した。またCompositionの2つのPresent経路からdraw後retire signalを要求し、pool teardownではimmediate contextの`WaitForIdle()`を呼んでからslot resourceと保持AVFrameを解放するようにした。
- **未検証の懸念:** teardownでの`WaitForIdle()`は低頻度のdevice/cache破棄時だけだが、デバイスロスト時の挙動と描画stateへの影響は未確認。現在の接続差分はまだ対象moduleのコンパイル・実描画で検証できていない。
- **価値:** fixed ringのcache identityとGPU lifetimeは別々に守る必要がある。slotの上書き前にcache keyを無効化し、destructionもqueue completionを考慮する。
- **次に確認:** `setDevice` / `clearDevice` / device loss時にboundedなretirementができるか調べ、targeted compile後、H.264 / ProRes、1 / 2 video layerでslot再利用とdrop/late/repeatを計測する。strict条件はそれら3値がすべて0であること。

### 2026-10-12 — Standalone preview debugger: clean exceptions, unrelated runtime warnings

- **関連:** `tools/VideoPreviewBench`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::replayGranularMaterialToFrame()`、`Artifact/src/Project/ArtifactRevisionService.cppm::loadLedger()`。
- **確認できた事実:** 最新のcanonical `ArtifactVideoPreviewBench.exe`をネイティブデバッガ下でH.264 720p、1/2層、D3D12 renderer + D3D12VAで起動し、source replacement・playback・window/process teardownまで実行。first-chance exceptionイベント0、Debug CRT/heap/assertion出力0、D3D12 ERROR/CORRUPTION出力0、exit codeは厳格preview gateの失敗を示す1だった。一方、Compositionの空またはgranular world未登録状態でMPM baseline reset失敗warningが25回、ArtifactVCSの`ledger.jsonl`を`WriteOnly device`としてreadしたというQt warningが2回観測された。これらは今回の捕捉ではC++例外やD3D12エラーではない。
- **未検証の仮説:** MPM warningはgranular materialが存在しないCompositionでもreplay resetを試みている可能性がある。QFile warningはVCS ledgerのread/write lifecycle上の別経路の可能性があり、現ログだけでは呼び出し元を特定できない。
- **価値／懸念:** 例外再現の前提はこのstandaloneシナリオでは解消したと確認できるが、診断ログ上の2種類のruntime warningは検証ログをノイズ化する。Product app全体・ProRes・RAM cache-on・長時間実行の例外不在までは証明していない。
- **次に確認:** 必要に応じてQt message handler / call stackを使い`ledger.jsonl` read warningの呼び出し元を特定し、granular worldの登録有無を調べてreset警告が無操作のCompositionにも妥当か確認する。動画のstrict frame gateは例外とは独立の未解決課題として続ける。

- **追記 2026-10-12:** `ArtifactRevisionService::appendRevisions()`がledgerを`WriteOnly | Append`で開いた後、末尾確認に`read(1)`を呼んでいたことを特定。`ReadWrite | Append`へ変更し、ArtifactRevisionServiceとComposition実装をtargeted compile、standalone benchを再リンクして再実行。ネイティブデバッガの同一1/2層シナリオで`WriteOnly device`警告は2件から0件、granular solver reset失敗warningは25件から0件になった。first-chance exception、Debug CRT/heap/assertion、D3D12 ERROR/CORRUPTIONも引き続き0件。厳格preview gateは依然失敗し、例外解消とは別に扱う。

### 2026-10-12 — 動画準備完了通知のキャッシュ無効化によるGUI停滞
- 関連: `Artifact/src/Layer/ArtifactVideoLayer.cppm`, `Artifact/src/Service/ArtifactPlaybackService.cppm`, `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`, `tools/VideoPreviewBench/main.cpp`。
- 確認済み: 独立H.264/D3D12VA/720pテストで、動画準備完了のLayerChanged通知が約280〜340ms。既存の複数レンダリング所有者から1,800フレーム範囲のディスク削除が繰り返され、各範囲約70〜100ms。GUIへのblocking dispatchが300〜530ms待ち、定常audio供給は約0.02〜0.03ms、診断サンプリングは約1msだった。
- 修正: 保存画像のない再生tickはディスク名前空間/シリアライズを計算しない。範囲無効化は名前空間を一度解決し、空のディレクトリでフレームごとの削除を省略する。存在する未hydrationディスクキャッシュの削除は維持。計測では通知が多くの場合約4〜16msへ低下。診断は既存PROFILE_SETUPフラグ、固定回数、追加flushなし。
- 懸念/未解決: 実行時フレーム準備完了と作者によるレイヤー編集が同じLayerChanged::Modifiedとして扱われるため、RAM構築キャンセルと全キャッシュ無効化は残る。既存イベント責務を整理し、停止中の非同期完成の再描画を保ったまま、編集・ソース変更のみが永続プレビューを無効化することを確認する必要がある。現段階では新たなイベント型/公開signalやslotは追加していない。
- 検証: 修正後のネイティブデバッガはfirst-chance実行時例外0、終了コード1（厳格性能条件の不合格）。D3D12 live-object警告は別途残る。全アプリ/ProRes/長時間安定性は未確認。
- 最終の非デバッガ/非profile測定（5秒、720p H.264、1/2レイヤー）: 1レイヤーは74フレーム進行、dropped=2/late=39/repeated=0。2レイヤーは58フレーム進行、dropped=0/late=46/repeated=0。3秒profileの進行改善だけを性能合格の証拠とはしない。ゼロコマ落ち・リアルタイム条件は未達。

### 2026-10-12 — 動画FrameReadyを既存通知経路で編集変更から区別
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/src/Layer/ArtifactVideoLayer.cppm`, `Artifact/src/Service/ArtifactPlaybackService.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` と既存LayerChanged購読者。
- 実装: 既存LayerChangedのChangeTypeへFrameReadyを追加（フィールド・レイアウト変更なし）。デコード/アップロード完了だけが送信する。レンダラーは該当レイヤーの一時サーフェスを無効化し、既存render tickを起こす。RAM構築・永続キャッシュの無効化は行わない。他の編集購読者はFrameReadyを無視し、自動保存/編集hook/階層再構築/編集UI更新を抑制する。ソース読み込みと遅れて判明したソース寸法変更はModifiedのまま。
- 境界: 新規公開signal/slot、バス購読、Transport状態所有者は追加していない。新規コンテナやフレームごとの確保は追加していない。既存の一時サーフェス無効化・再描画経路を再利用する。
- 要確認: 停止中の非同期完成、RAM構築完了、キャッシュなし/あり再生、例外/終了時安全性を独立benchで確認する。全アプリの編集UIおよびソフト/別backendの実行確認は別範囲。
- 検証結果: event interfaceと既存全購読者・動画producer・render controllerを対象に23ファイルを直接コンパイルし、独立benchをリンク/配置。非debugger・非profileの720p H.264/D3D12VA/5秒: 1レイヤーfinal=80/dropped=3/late=45/repeated=0、2レイヤーfinal=56/dropped=0/late=51/repeated=0。性能条件は不合格。前回比較で速度向上は断定しない。Native debuggerの1/2レイヤー再生・source replacement・終了チェックはexceptions=0、D3D12 ERROR/CORRUPTION=0、終了code=1（性能判定）。
- RAM構築結果: 40秒待ってready=0/requested=89。確認済みのコードではrequestRamPreviewBuildがキューを作り、レンダラーが現在フレームとbuildQueueNextFrameが一致するときだけcaptureする。nextRamPreviewBuildFrameの呼び出しはservice内部・診断だけで、停止中に構築対象へ進める実行経路を検索で確認できなかった。未検証: 全アプリの別経路が存在する可能性と、frame0のcaptureが発生しない他の条件。これはFrameReady種別修正とは別の構築ドライバー課題として調査を続ける。

### 2026-10-12 — RAM構築を止めていた通知の順序と初回描画条件の確定
- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`, `tools/VideoPreviewBench/main.cpp`, `Artifact/src/Layer/ArtifactVideoLayer.cppm`。
- 確認済み: FrameReady区別後もready=0/90。PROFILE_SETUPでqueueReason=layer-changed、active=0を確認。GUI上の編集イベントでも無効化をさらにqueued hopしていたため、イベント後に開始したbuildが消され得た。GUI同一threadはinline、他threadはqueuedかつComposition再確認へ修正。
- 確認済み: この修正後ready=1/90、queueReason=preview-render-contract-changed。初回readbackでrender contractを確定すると残りbuildも消されていた。古いcache/世代を無効化しつつ、activeだったrange/reasonは新contractで再要求する。次の検証で52フレームまで進行。
- 確認済み: 構築用seekのnear-playhead prewarmが前方へ進むほど既構築frameをevictし、90枚が揃わなかった。独立benchは構築range以上の既存prewarm radiusを設定し、既存PlaybackService API経由でqueue先頭を表示・render tickへ再試行を依頼する。画像合成・Transportの別実装は追加しない。headをvideo/readback待ちで回す既存deferは、非同期完成とcapture対象がずれるため、この2理由のみheadを保持する。
- 検証済みの到達点: 720p H.264/1 layerでRAM=90/90、3秒でfinalFrame=91。しかしstrict drop/late条件は不合格。2 layersはまだ0/90。測定範囲と準備phaseの分離前の値であり、性能合格として扱わない。
- 測定修正: FrameRange終端はexclusive。benchはframeCountをendへ渡し、Composition durationも測定秒数に合わせる。case間はstop/wait、seek0/pump後にlayerを置換。準備phaseのlateを測定へ混ぜないため、既存VideoLayer診断にresetFrameTimingDiagnosticsを追加し、測定前のみ呼ぶ。新規signal/slot・コンテナ・Transport所有者は追加していない。cold contract再構築は既存queue再確保を使う（毎frameではなく描画条件の変更時のみ）。

### 2026-10-12 — キャッシュ構築のケース再利用・終了所有権・表示確認の待ち合わせ
- 関連: tools/VideoPreviewBench/main.cpp、Artifact/src/Layer/ArtifactVideoLayer.cppm、Artifact/src/Playback/ArtifactPlaybackEngine.cppm、Artifact/src/Service/ArtifactPlaybackService.cppm。
- 確認済み: case間のviewport.stopがrenderer tickも停止していた。停止中構築前に既存controller.startを呼び、1/2レイヤーとも30/30および90/90を構築できた。Transportは引き続きPlaybackServiceが所有する。
- 診断訂正: shared-cache経路ではCPU bufferが1x1でもcurrentSharedFrameは正しい画像。debugStateを実効shared寸法に修正。画像cache破損は確認していない。一方setSourceSizeはgeometry revisionを進めず、同一frameのboundsが1x1に残るため動画metadata/source寸法更新時に既存Source dirtyを設定した。
- 終了ハング: native attachでmainがshared background pool destructorを待ち、decode workerがvideo upload poolのstart mutexを待つstackを確認。独立benchはpoolが生存しているうちにremoveAllLayersでdecode/uploadをjoinして終了する。修正後H.264/ProResの複数caseプロセスは正常終了。製品全体の終了順序は未検証。
- 表示抜け: composition同期だけではrender tick前に次frameが適用され得る。既存recordPresentedFrameをworkerへ返し、既存mutex/conditionで最大100msのbounded waitを追加。GUI/GPUを待たせず、pause/stopで解除し、observer不在なら既存dispatchへ戻す。signal/slot・購読・コンテナの追加なし。
- 非debugger結果: H.264 720p30/cacheあり3秒、1 layer final89/drop0/late0/repeat0、2 layers final54/drop0/late25/repeat0。ProRes422HQ1080p30/cacheあり1秒、1 layer final29/drop0/late0/repeat0、2 layers final19/drop0/late4/repeat0。短時間の1 layer合格であり、持続realtime/多layer条件は未達。
- 未検証の仮説: tryGetRamPreviewFrameImageの毎frame DeepCopyとCPU uploadが残る。cache hit=GPU zero-copyとは限らず、このコストの計測と所有権を保った再利用を次に検討する。新規標準コンテナは追加していない。

### 2026-10-12 — RAM合成画像の同期描画に限定したborrow
- 関連: Artifact/include/Service/ArtifactPlaybackService.ixx、Artifact/src/Service/ArtifactPlaybackService.cppm、Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 確認済み: tryGetRamPreviewFrameImageはDeepCopyを返し、viewportは描画ごとに大容量CPU cloneを行っていた。新しいborrowRamPreviewFrameImageはowner thread限定でcache-owned const imageを返す。既存copy APIは他の呼び出し用に維持。rendererは取得からdrawまでcache変更/API呼び出し/event pumpを挟まず、PrimitiveRenderer2Dはその呼び出し中にCPU uploadを完了し、コマンドにはGPU textureだけを保持する。
- 境界: 返却pointerはcache変更まで、同期描画のみ。保存・background渡しは禁止。新規signal/slot、コンテナ、allocationは追加せず、既存一時Imageの1x1 allocationとDeepCopyを減らす。CPU upload自体は既存経路のまま。速度への寄与とnative例外は独立benchで検証中。
- 直前native結果: cacheあり3秒/H264のデバッガ実行はfault exceptions0、D3D12 ERROR/CORRUPTION0、正常exit1。ただしready71/90で35秒timeoutとなり1caseだけ。debugger検証は速度や2case切り替えの証拠には使わない。

- borrow変更後の非debugger結果: H.264720p30/cacheあり3秒、1 layer final89/drop0/late0/repeat0、2 layers final59/drop0/late18/repeat0。両case ready90/90、プロセス正常終了。前回final54/late25との一度の比較だけではspeedupを断定しない。全体条件は未達。temp/video-borrowed-preview-3s.json。

- borrow後native検証: 1/2 layers両caseのRAM30/30、再生/source replacement/終了までfault exceptions0、D3D12 ERROR/CORRUPTION0、CRT heap/assertion0、terminal exit1（性能gate）。temp/video-native-borrowed-debug.log。非fatal live-object/shader warningは残る。全アプリは起動・検証していない。
- bench判定修正: 250ms周期sample後の終盤で発生したlate/repeatを取りこぼさないよう、測定終了時も診断を取得してから合否を決める。

### 2026-10-12 — RAM再生のディスクprobeとFrameReadyのviewport状態初期化
- 関連: Artifact/src/Service/ArtifactPlaybackService.cppm、Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 確認済み: valid RAM imageがあるtickでもhasPreviewFrameOnDiskを呼び、QImageReader.canReadとmanifest/file検証を同期実行していた。PROFILE_SETUPのpublication後半は10〜15ms、sync自体は約0.2ms。valid RAM時はwriter/eviction/invalidationが維持するdisk stateを保ち、取得が必要なdisk-load時の検証は残す。
- 結果: H264720p30/cacheあり2layers/3秒でfinal74/drop0/late28。以前のfinal59より進んだが単独runでspeedupを断定しない。
- 確認済み: FrameReadyはinvalidateBaseComposite(true)を呼び、lastRenderKeyState全体がゼロ化される。viewport変化判定がこのzoom/panを使うため、コンテンツ完成をviewport-transform-changingと判定しRAMfallbackを外し得る。FrameReadyの呼び出しだけ3個のfloatを保存復元する。base invalidation serial/dirty/render keyの無効化は維持。他の編集操作は変更しない。
- 結果: cached2layers/3秒final81/drop0/late0だが、90frameのrealtime進行条件は未達。後述の計測修正前の値であり、latenessの合格証拠にしない。

### 2026-10-12 — キャッシュ再生の表示時間診断を描画前から計測
- 関連: Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm、Artifact/src/Layer/ArtifactVideoLayer.cppm。
- 確認済み: RAM表示のmarkFrameCompositionCacheReady/markFrameRenderQueuedがrenderer.present後に呼ばれ、request timestampまでresetしたためpresentedMsほぼ0になり、キャッシュ描画の遅延を隠していた。
- 修正: RAM選択確定後の描画前にcache ticket/queuedを設定し、同一frameのcomposition request timestampを維持。既存present後のmarkPresentedが実時間を記録する。RAM ticketを非表示のdecode completionで書き換えない。キャッシュがdiskにも保存されているという理由だけで実際のRAM表示をcomposition-diskと記録しない。
- 検証: H264720p30/cacheあり1秒、1layer final29/drop0/late0/repeat0、2layers final29/drop0/late6/repeat0。両case30/30、正常exit1。1layerの平均presentedMs20.19（最終19.98）、2layers24.23。以前のcached PASSは新計測で再確認が必要。gateは緩和していない。

### 2026-10-12 — デコード済みpayloadとupload表現の寿命を分けて再利用
- 関連: Artifact/src/Layer/ArtifactVideoLayer.cppm。
- 確認済み: upload cacheは4frames/64MiBのbounded保持。prepareDecodeRequestのshared-payload再利用がhasCachedUploadFrameを必須にしていたため、CPU payloadが有効でもupload evictionだけでdecode workerへ進んでいた。
- 修正: source version/dimensionsが合うshared CPU payloadは再利用し、不足uploadだけを既存priority upload conversion poolへenqueueする。cache容量は増やさない。新規signal/slot・container・pool・起動flagなし。既存workerのbounded4 requestを再利用し、再デコードとCPU image cloneを避ける。既存のbackground upload allocationは残る。live cache-miss/共有payload/source変更/終了と複数layerを再検証する。

- upload再利用後の検証: H264/cacheあり1秒の1,2,4 layersはfinal29/drop0/repeat0。1/4はlate0、2はlate1。続く3秒の2,4,8 layersは全case RAM90/90、final89/drop0/late0/repeat0、terminal exit0（temp/video-shared-upload-scale.json）。修正後timingを使うが短いsingle runで、持続安定性は未達。
- 測定範囲: bench Compositionは固定1280x720/30fps。1080p ProResは入力寸法でありnative1080p Compositionの性能とは異なる。現在同一入力の複数layerであり、独立した異なる動画streamの同時性能は未検証。
- 未検証の次の確認: PrimitiveRenderer2Dのsprite texture cacheは50 entries/512MiB。RAM frame cache128とentry数が違い、90-frameのGPU事前保持はentry限界で全体が残らない可能性がある。GPU warmupを実装するならentry/byte両budget・冷/温phaseの分離・正確なowner/version identityを先に確認する。現在のscopeを勝手に拡げてはいない。

- ProRes再検証: 1080p ProRes422HQ入力/720p Composition/cacheあり3秒/2,4,8 layers。全case90/90、final89/drop0/repeat0、lateは2/1/10。正常exit1。strict条件は未達。temp/video-prores-shared-upload-scale.json。
- 最新native再検証: 1/2 H264 layers両case30/30、再生・source replacement・終了。fault exceptions0、D3D12 ERROR/CORRUPTION0、CRT heap/assertion0、正常exit1（性能gate）。temp/video-native-cache-reuse-debug.log。ProRes/native1080p/長時間/異なるsource同時/他backendはまだ検証範囲外。

### 2026-10-12 — 再生時計と実際に表示した最終frameを分けるgate
- 関連: Artifact/include/Service/ArtifactPlaybackService.ixx、Artifact/src/Service/ArtifactPlaybackService.cppm、tools/VideoPreviewBench/main.cpp。
- 確認済み: benchのfinalFrameはengine currentFrameで、recordPresentedFrameの最後の実表示を読んでいなかった。特に測定終了直前は未表示targetが残り得る。
- 修正: serviceの実表示通知だけで更新するlastObservedPresentedFrameを追加。playで-1にresetし、仮のlastPresentedFrame seedとは区別する。benchはfinalPresentedFrameもframeCount-2以上を必須とする。新規signal/slot/container/フレームallocationなし。setterは既存renderer routeのみ。表示完了はCPU-side present成功通知で、GPU scanout時刻ではない。
- 未検証: 新gateで2/8 layersの3秒cached H264を再測定中。以前のrealtime PASSは新gateで再確認が必要。
### 2026-10-12 — 独立benchのCompositionとviewport解像度を明示
- 関連: tools/VideoPreviewBench/main.cpp。
- 修正予定の現状: composition-width/height CLIを追加し1..8192整数をvalidate、既定1280x720。Composition/viewport/windowを同じ要求サイズで作り、reportにComposition寸法/FPSと実viewport寸法を記録。入力1080pだけをnative1080pの証拠にしない。source編集済み、実行中bench終了後にbuild/runする。巨大frameのメモリ実行は別のbounded budget確認が必要で、上限入力validateだけで性能を保証しない。

- 実表示gate再検証: H264720p/cacheあり3秒/2,8 layers。両方transport89/actual89/RAM90/90/drop0/repeat0。2layers late1でfail、8layers late0でpass（temp/video-actual-presented-gate.json）。複数runで安定合格とはまだ言えない。
- native1080p実行: ProRes入力/Composition1920x1080/viewport実寸1920x1080/cacheあり3秒。1layer transport79/actual79/late65、2layers transport65/actual64/late58。両方90/90/drop0/repeat0、正常exit1（temp/video-native1080-prores.json）。720p結果を1080pの証拠に使わない。約4.5GiB working setを観測、長時間の保持/budget調査が必要。
- bench取得負荷: frameDebugSnapshot(false)でも診断resource/RenderGraph/stringをGUI threadで構築していた。通常は既存frameDebugCountersにし、PROFILE_SETUP時のみ詳細snapshot。合否用actual/video timingは維持。変更後1080p1layer/cacheあり1秒はtransport29/actual28/late21/RAM30/30/drop0/repeat0でfail。診断負荷だけでnative1080pの遅延は説明できない（temp/video-native1080-counters.json）。
- 次の計測: 既存PROFILE_UPLOADの384sample上限で準備後の再生まで捕捉し、bench境界measure markerで区切る。GPU/CPU間copy自体ではなくRGBA conversion/texture creationも既存timerで測る。profiling実行をrealtime acceptanceに使わない。明示flushを1つ削除。新規flag/pool/containerなし。

### 2026-10-12 — 1080pキャッシュ表示のCPU変換と新規texture生成の実測
- 関連: Artifact/src/Render/PrimitiveRenderer2D.cppm、tools/VideoPreviewBench/main.cpp。
- 計測事実: PROFILE_UPLOAD/1920x1080 Composition/ProRes/cacheあり1秒のmeasure marker後、27 upload samples。convertMs中央値18/max20、createTextureMs中央値7/max16、keyMs中央値0。cacheHit中央値0。再生区間にvideo decode samplesはなく、cached composite表示の変換・texture作成が約25msを占めている。temp/video-native1080-upload-profile.log。profiling runの遅延19はacceptanceには使わない。
- 実寸訂正: CPU upload imageは2400x1195。CompositionとQt widgetの論理寸法1920x1080だけをphysical cached image寸法と見なしてはいけない。benchへcachedImageWidth/Height、viewportDevicePixelRatioを追加。画像がcanvas-onlyかviewport背景も含むか、読出しcontractを次に確認する。
- 次の実装候補（未実装・未検証）: RAM entryへ準備済みRGBA8 upload representationを持たせ、cold build時に既存convertImageForUploadIntoで作る。表示時は既存textureForVideoFrameのowner/content-key付き再利用texture slotへ渡し、毎frame変換/texture生成を避ける。CPU imageと同じeviction/generation/source変更で無効化し、追加upload byte budgetを明示する必要がある。新しいstd mapは作らず既存entry value/独自containerを検討。単に全frameGPU常駐を無制限に増やさない。変換をGUI cold boundaryで行うか既存bounded workerへ送るか、終了/ownership/未完成表示の経路を先に読む。
- 修正しない推測: ImageF32x4RGBAWithCacheはDiligent GPU APIを持つがimage() constが可変参照を返してcpu-dirtyを設定し、float textureのcolor contractもsprite RGBA8経路と違う。これをそのままGPU借用して速度を断定しない。

### 2026-10-12 — RAM entryのbounded表示用RGBA8準備と既存texture slot再利用
- 関連: Artifact/include/Service/ArtifactPlaybackService.ixx、Artifact/src/Service/ArtifactPlaybackService.cppm、Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 実装: 既存RAM mapのentry valueにCPU storage/既存ImageUploadBuffer/revisionをまとめる。停止中cache storeのみ既存convertImageForUploadIntoを実行し、毎frame live captureには追加変換しない。uploadは1frame64MiB、全体1GiB上限。容量超過ではuploadなしで既存画像表示へfallback。置換/evict/range無効化/clearでuploadも破棄しbyte計数を更新する。
- 表示: 同一owner thread/同期描画のborrowUpload API。controllerごとにcold生成UUID、entryごとの単調revisionで既存textureForVideoFrameへprepared uploadを渡し、既存reusable texture slotを使用する。色変換target/texture formatは従来のRGBA8 sRGB straightと同じ。準備済みGPU texture不在/失敗では従来sprite経路を使う。Transport、新規signal/slot/subscription、独自GPUqueueは追加しない。
- allocation例外: 停止中cache preparationでImageUploadBufferの既存std::vector byte storageを確保する。新規std containerは作らず既存RAM mapのvalueのみ変更。既存独自ImageUploadBuffer API/ABIを再利用するため、その既存std::vectorを置換しない。追加メモリ最大1GiB。ライブでは既存GPU texture update allocation/バックエンド内部確保の実測が必要。
- 現状: source編集/直接build中、runtime/color/eviction/source replacement/終了はまだ未検証。新方式の速度は未証明。目標を達成したとは扱わない。
- 直前native検証: actual-present gate/DPI reportまでの版本は1/2layers30/30でexceptions0、正常exit1。prepared upload変更は含まない。

- prepared upload初回検証: ProRes/cacheあり/1920x1080 Composition/viewport DPR1.25/実cache2400x1195/1秒。1layer transport29/actual29/drop0/late0/repeat0でpass。2layers transport29/actual29/drop0/late1/repeat0でfail。RAM各30/30、プロセス正常exit1。temp/video-prepared-upload-native1080.json。単発1秒で安定性は断定しないが、以前1layer late21だった条件で進展した。新GPU slotのnative例外検証中。

- prepared upload native検証: 1/2 H264/cache各30/30、replacement/playback/teardownをnative debuggerで確認。fault exceptions0、D3D12 ERROR/CORRUPTION0、CRT heap/assertion0、正常exit1。temp/video-native-prepared-upload-debug.log。GPU image visual parity、ProRes native debugging、追加byte budget exhaustion、他backendはまだ未確認。

- prepared upload 3秒検証: ProRes/1920x1080論理viewport/DPR1.25/cache実寸2400x1195/2,4layers各90/90。2layers actual89/drop0/late4/repeat0でfail、4layers actual89/drop0/late0/repeat0でpass、正常exit1。temp/video-prepared-upload-prores-scale.json。cold-caseと後続caseの差、disk writer負荷、dynamic GPU page warmupの寄与は未検証。workingset約8GiBを観測（PC約32GiB、空き約4GiB）。単発4layer passで完全達成とはしない。
- bench比較モード: --no-disk-cacheは既存setDiskPreviewCacheEnabledを起動設定時に使い、RAM policyを維持したままdisk persistenceの寄与を分ける。defaultはdisk有効のまま。reportにdiskCacheEnabledを記録。製品のdefault変更ではない。build/run確認中。

### 2026-10-12 — 表示済みcached frame再描画の診断二重計上を抑制
- 関連: Artifact/src/Layer/ArtifactVideoLayer.cppm。
- 確認済み: 同じtimeline frameのcache redrawもmarkFrameCompositionCacheReadyでticketを作り直し、同一request timestampを保ってmarkPresentedするため、すでに正常表示済みのフレームが後の再描画でlateとして追加され得た。新しいtransport frameへの表示と無関係なredrawを区別する必要がある。
- 修正: FrameTicketにpresentationRecorded boolを追加。最初のpresent成功/lateの結果を記録した後は、同一ticketのqueue/present/cacheReadyで結果を作り直さない。beginFrameTicket/resetが新timeline frameを評価するため、実際の初回遅延・repeatedLastGood・actual presented-range gateは維持する。フレームごとの確保・公開signal/slotは追加しない。現在のmatrix実行には未反映、終了後にbuild/retestする。
- 確認すること: 並行decodeのbeginFrameTicketで同一timeline ticketが書き換わる経路、source/seek/loop変更時のreset、およびrecorded outcome count。短時間PASSを目標達成へ読み替えない。

- 二重計上の追加証拠: prepared upload/RAM-only/ProRes1080p論理3秒/1..8matrixは全actual89/drop0/repeat0。1,7layersのみlate0でpass、他late10/6/2/3/2/3でfail。1layer ticket window89だが複数layer全てwindow120（上限）となり90-frame範囲より多いoutcomeを確認。temp/video-prepared-upload-prores-ram-matrix.json。二重計上が全lateの原因だとはまだ断定せず、修正後first-present遅延とcountを再測定する。

- ticket bool単独の再検証: ProRes/1080p論理/RAM-only3秒/2,8layersはactual89/late0/drop0/repeat0でpassしたがwindow120/117と二重件数が残った（temp/video-unique-presentation-prores.json）。prepareDecodeRequestのbeginFrameTicketが同一frameのticketを作り直す経路を踏まえ、ImplでlastRecordedOutcomeFrame/Generationを保持し、queue/presentでも連続する同一frame+generationを拒否する。reset診断でこのledgerもclearする。全lateを隠す変更ではなくfirst present結果を維持する。現在build済み・再測定中。
- ticket bool版本ProRes native検証: 1920x1080論理/1,2layers各30/30/RAM-only/終了までexceptions0、D3D12 ERROR/CORRUPTION0、CRT heap/assertion0、正常exit0。temp/video-native-unique-prores-debug.log。ledger追加はそのrunに含まない。

- ledger修正後ProRes1080p論理/DPR1.25/RAM-only3秒/2,8layers: actual89/drop0/late0/repeat0、両first-video window89へ整合。avgPresentedMs16.60/18.85。RAM各90/90、正常exit0。temp/video-outcome-dedup-prores.json。短期/同一素材の複数layer条件であり、長時間・異なる素材混在・disk有効・visual parityは未達。ledger版本native検証中。

- 最新ledger版本ProRes native検証: 1080p論理/RAM-only/1,2layers各30/30、actual29/late0/drop0/repeat0。fault exceptions0、D3D12 ERROR/CORRUPTION0、CRT heap/assertion0、正常exit0。temp/video-native-outcome-dedup-debug.log、temp/video-native-outcome-dedup-repro.json。visual parity/他backend/長時間はこの証拠に含まない。

### 2026-10-12 — 独立プレビューの混在素材検証と長時間診断の範囲
- 関連: tools/VideoPreviewBench/main.cpp、Artifact/src/Layer/ArtifactVideoLayer.cppm。
- 確認済み: canonical exe、H.264 720p + ProRes HQ 1080p、論理1920×1080/DPR1.25、RAM-only、3秒の2/8-layer検証は共にactual89/drop0/repeat0。2-layer late0/pass、8-layer late3/fail。temp/video-mixed-outcome-dedup.json。混在条件の安定合格は未達。
- 確認済み: canonical exeを明示指定したnative debuggerの混在2/8-layer・1秒検証は例外0、重大D3D12/CRTエラー0、terminal exit1。両case actual29、late1/0。temp/video-native-mixed-dedup-debug.log。旧helper既定exeの初回起動はcase report未生成で検証証拠から除外。
- 懸念（未検証）: 現在のlate/repeated検出は各レイヤーの直近outcome窓を周期的に読むため、長時間の累積件数とは異なる。長時間検証では窓を超える計測、ループのframe/generation ledgerリセット、128-frame RAM上限を含め、合格条件を取りこぼさない測定方式が必要。
- 次に確認: 通常実行で8-layer混在の遅延発生箇所を計測し、表示用転送・GUI scheduling・メモリ圧迫を切り分ける。長時間試験はboundedなキャッシュ運用と累積present測定を設計してから行う。

### 2026-10-12 — 混在動画プレビューの転送とPresent計測
- 関連: Artifact/src/Render/PrimitiveRenderer2D.cppm、ArtifactCompositionRenderController.cppm、tools/VideoPreviewBench。
- 確認済み: 既存opt-in uploader診断を未準備24回／準備済み2048回に分離し、prepared表示と強制flush除去を追加。preparedはdecode workerも利用するのでRAM Compositeの識別子ではない。診断なしの描画処理は同じ。既存object/static library/benchを局所ビルドし正常リンク。
- 確認済み: 混在8-layer/3秒測定区間の128 uploadは全て2400×1195、pool hit、texture新規生成0。CPU側conversion/key median0/max2ms、UpdateTexture median1/max8ms。actual89/drop0/repeat0/late8、terminal exit1。temp/video-mixed-upload-bounded-summary.json。診断付きなので性能合格の証拠ではない。
- 確認済み: 既存Composition ViewフェーズログはQT_FORCE_STDERR_LOGGING=1を併用して採取。混在8-layer/1秒はactual29/drop0/late0/repeat0、terminal exit0。16ms以上のslow26回に限りframe median19/max24ms、setup1/1ms、layer2/13ms、submit2/11ms、present14/20ms。temp/video-mixed-phase-stderr-profile.log。CPU wall phaseの一部であり全frame平均やGPU時刻ではない。
- 仮説（未検証）: 残る散発lateはCPU変換やtexture新規生成だけでなく、Present待ちとGUI再描画タイミングの競合が関係する可能性。vsyncやpresent同期を根拠なしに解除すると表示順序・画面品質を悪化させる恐れがある。
- 次に確認: 同一frameの再描画要求、transport ACK、Present待ちの時系列とswapchain既存契約を調べ、boundedな再描画抑制で改善可能か確認する。

### 2026-10-12 — 同一frame再描画のtemporal不連続判定
- 関連: Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm のrender tick。
- 確認済み: temporalFrame != last+1という判定は同一frame再描画でもtrueとなり、履歴無効化とN-1のpre-roll描画が走る。条件にtemporalFrame != lastを追加。同一frameを履歴の不連続として扱わず、seek/reverseの従来経路を維持。描画抑制・同期解除・追加allocationはない。
- 確認済み: controller objectのみ再コンパイルしてbenchをリンク。通常mixed2/8-layer・RAM-only・論理1920×1080・3秒はactual89/drop0/repeat0、late0/1、exit1。temp/video-same-frame-history-mixed.json。旧8-layer late3から単発で減ったが、ばらつきがあり性能改善は未確定。
- 確認済み: 最新canonical exeのnative mixed2/8-layer・1秒検証は両actual29/drop0/repeat0、late1/0、fault exception0、重大D3D12/CRTエラー0、正常terminal exit1。temp/video-native-same-frame-history-debug.log。実行中benchは残っていない。
- 価値／懸念: 同一frame再描画の不要な履歴破棄とpre-rollを回避できる。temporal effectの見た目、seek/reverse、scene変更時の履歴更新はこの動画のみの試験では未検証。
- 次に確認: 同一条件で再描画／Present回数を比較し、8-layerの残る遅延と、seek/reverseの既存履歴契約を確認する。

### 2026-10-12 — RAM表示済みフレームへのFrameReady追加描画
- 関連: Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 確認済み: 同一frame temporal修正後でも8-layer混在3秒の測定は138 pooled texture参照。全2400×1195/pool hit、UpdateTexture CPU median0/max3ms、actual89/drop0/late0/repeat0。temp/video-history-upload-summary.json。Present回数減少の証拠ではない。
- 実装: RAM表示成功時のframe/CompositionIDを値保持。既存FrameReady subscriberはlive surface/damage無効化を維持し、再生中・同一Composition/frameのRAM表示成功済み・interactionなし・RAM画像残存時だけmarkRenderDirty追加を省く。既存dirtyは消さず、FrameChangedやauthoring要求も抑制しない。追加allocation/queue/signal/subscriptionなし。
- 確認済み: 局所controller build/bench link後、通常混在2/8-layer・RAM-only・論理1920×1080・3秒は両actual89/drop0/late0/repeat0でterminal exit0。temp/video-ram-ready-coalesce-mixed.json。
- 確認済み: native混在2/8-layer・1秒はactual29/drop0/repeat0、late1/0、fault exception0、重大D3D12/CRTエラー0、正常terminal exit1。temp/video-native-ram-ready-coalesce-debug.log。bench processは残っていない。
- 未検証: 単発短時間の通常合格であり長時間・反復・Composition切替・viewport操作・cacheなし・diskありを証明しない。追加描画回数の再比較も必要。
- 次に確認: 通常反復と測定中Present回数を比較し、長時間試験のbounded cache/累積診断を整備する。

### 2026-10-12 — Disk有効試験と長時間向け累積診断
- 関連: Artifact/src/Layer/ArtifactVideoLayer.cppm、tools/VideoPreviewBench/main.cpp。
- 確認済み: coalesce修正のdisk有効・混在2/8-layer・論理1920×1080・3秒は両RAM90/disk90/ready90、actual89/drop0/late0/repeat0、terminal exit0。temp/video-ram-ready-coalesce-mixed-disk.json。保存処理を伴う単発短時間の合格。
- 実装: 直近120-outcome窓とは別にlate/repeatedの固定quint64累積値を保持。first presentationのみ加算し既存診断resetでゼロにする。benchの合否は累積値のレイヤー間最大を採用（INT32_MAXで飽和）、policyをJSONに記載。累積diagnostic欠落は有効sampleに数えない。新規hot allocation/public ABI変更なし。
- 確認済み: 変更2objectを局所compile/link。H.264 1-layer・論理1280×720・RAM-only・4.2秒は126/126、actual125、window120、累積late/repeated0、drop0、terminal exit0。temp/video-cumulative-window-boundary.json。
- 確認済み: 最新exeのnative mixed2/8-layer・1秒は両actual29/drop0/repeat0、late1/0。fault exception0、重大D3D12/CRTエラー0、正常terminal exit1。temp/video-native-cumulative-outcomes-debug.log。benchは残っていない。
- 未検証: 早期の非ゼロerrorを窓からevictした際の累積保持は注入試験未実施。長時間ループ、bounded rolling cache、disk保存の長時間安定、全layer matrixを今後確認する。
- 次に確認: 128-frame cache上限を維持した長時間ループ試験と、wrapをまたぐactual present/drop診断の契約を整備する。

### 2026-10-12 — bounded windowの30秒ループ検証
- 関連: ArtifactPlaybackService.ixx/.cppm、tools/VideoPreviewBench/main.cpp。
- 実装: サービス既存present通知で同じframeのredrawを除く表示成功数を値保持しplay時reset。benchにloop-window-seconds追加、windowを128frames以内に制限、source/Composition/cache期間をwindowへ分離。測定時間のfps相当actual countと累積late/repeated/dropで判定。新規hot allocation/queue/signalなし。
- 実装: Loop中skip Noneの境界gapをplayable inclusive端点から数える。明示seekは既存baseline reset、ping-pongは除外。wrap欠落注入、reverse/ping-pong/seek回帰は未検証。
- 確認済み: service interface/implementation/bench objectを局所compile/link。混在2/8-layer・論理1920×1080・RAM-only・3秒窓を30秒ループは両900 actual presentation notifications、RAM90、final0、drop0/累積late0/repeat0、terminal exit0。temp/video-loop-window-mixed-30s.json。
- 確認済み: native mixed2/8-layer・1秒窓を3秒ループは両90 notifications/final0/drop0/repeat0/late1、fault exception0、重大D3D12/CRTエラー0、正常terminal exit1。temp/video-native-loop-window-debug.log。残存bench processなし。
- 制限: 成功通知はCPU present()の観測で物理scanoutではない。cached短区間の持続反復の証拠であり、長い固有区間やrolling decode、disk有効長時間、全layer matrix、反復試験、visual parityを証明しない。
- 次に確認: 同じ30秒条件の反復とdisk有効、全1..8 matrix、および長い固有区間をboundedに準備・再生する方式。

### 2026-10-13 — 全1..8-layer持続matrixと比較成果物
- 関連: tools/VideoPreviewBench/export_report.py、ArtifactVideoLayer::goToFrame、ArtifactPlaybackService::syncCurrentCompositionFrame。
- 確認済み: 最新ordinary混在1..8-layer（1はH.264のみ）・論理1920×1080・RAM-only・3秒窓/各30秒は全900 observed presentations/drop0/repeat0、late0/0/0/1/2/0/0/1。4/5/8はfail、正常exit1。temp/video-loop-window-mixed-matrix-30s.json。先行2/8単発passの安定再現は未達。
- 確認済み: 8-layer最後のwindow late0でも累積late1でfail。5-layerもwindow late1/cumulative2。過去の実遅延を窓のeviction後も保持した実行証拠。
- 実装／確認: offline exporterでCSVとmanifestを出力。全8case status/present/late件数を元JSONと照合一致。post-run静的CPU/GPU/driver/OS inventory、report/exe SHA256を記録。exe snapshotはlaunch identityではなく、利用率/peak memory/physical scanout等の不足を明記。測定中にOS inventory/hashを実行せず終了後に収集。残存benchなし。
- 確認済みのcode事実: PlaybackServiceは各frameでComposition goToFrameを呼び、VideoLayer goToFrameはsource frameが変わるとdecodeCurrentFrameを先行開始する。RAM final composite表示中もこの入口が残る。
- 仮説（未検証）: RAM表示中の不要なlive decode/prefetchやframe-ready処理が散発latenessに関与する可能性。LayerからPlaybackServiceへ新規importすると責務やmodule循環を広げるため、cache-ready時のdecode抑制はfallback/viewport契約を確認してから最小境界で検討する。
- 次に確認: 失敗caseのフレーム単位時系列とdecode/GUI待ちを計測し、反復条件で比較。長い固有区間とdisk有効matrix、残るmetricsを継続する。

### 2026-10-13 — First-late保持とComposition反映前の描画
- 関連: ArtifactVideoLayer.ixx/.cppm、ArtifactCompositionRenderController.cppm、VideoPreviewBench main/exporter。
- 確認済み: 旧版4/5/8-layer・各30秒repeatは全900 presentations/drop0/repeat0、late0/1/0、exit1。temp/video-loop-failed-cases-repeat.json。散発性と5-layer再発を確認。
- 実装: 最初のlate frame/source/elapsed/decode/queued/present/renderToPresentをPODで保持し既存resetで消去。詳細文字列は明示decodeState(true)でcold取得、通常decodeState()は従来summary。benchは計測終了後に全layer ID/source/diagnosticsをJSONへ、exporterはmanifestに保存。新規hot allocationなし。
- 確認済み: 初回詳細format実験5-layer60秒は約41.475秒のframe/source74でlate1、各layer decode0、queued16.09–17.00ms、renderToPresent17.20–17.48ms。temp/video-first-late-mixed-5layer-60s.json。この版は毎frame詳細整形がありperformance比較の限界。cold取得へ修正した5-layer60秒は1800/drop0/late0/repeat0/exit0。
- code事実／実装: rendererはPlaybackService clockを描画frameに採用する一方、Composition更新はGUI dispatchされる。render tickでmatching Composition frame != active service frameの場合、描画せず既存同期FrameChanged要求を待つguard追加。pause/different Composition/render queueは除外。新規transport/queue/signal/waitなし。不一致発生率とlateへの因果寄与は未測定。
- 確認済み: 最新guardのmixed5/8-layer・論理1920×1080・RAM-only・3秒窓/各60秒は両1800/drop0/累積late0/repeat0/exit0。temp/video-composition-sync-gate-60s.jsonとCSV。
- 確認済み: native mixed2/8-layer・1秒窓/3秒は両90/drop0/repeat0、late1/0。fault exception0、重大D3D12/CRTエラー0、正常exit1。temp/video-native-composition-sync-gate-debug.log。bench残存なし。
- 次に確認: 最新全1..8 matrixと反復、guard deferral時系列、viewport/no-cache/disk有効経路。cached短区間であり長い固有sourceとvisual parityは未達。

## 2026-10-13 — 完成描画数と表示通知数の比較

- 関連: tools/VideoPreviewBench/main.cpp、export_report.py、ArtifactCompositionRenderController。
- 確認事実: 既存currentFrameSerialの測定区間差分を追加。全1..8-layer各60秒試験では4-layerにlate3、他はlate0。4-layer追試は完成描画1800／表示通知1800、drop/late/repeat0。余分な完成描画はこの追試では確認されない。
- 価値・懸念: 単発合格だけでは散発遅延を解決したと判断できない。queued19.96msとrender-to-present16.46msの先行first-late証拠はCPU側時間であり、背景decodeやGPU負荷を除外できない。
- 次の確認: 同一条件反復と遅延発生時の描画数比較。physical scanoutは未検証。Microsoft GetFrameStatistics仕様ではflip/fullscreenの条件と複数monitor等の信頼性制約があるため、新規hot-path DwmFlushを安易に入れない。参照: https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getframestatistics

### 2026-10-13 — 同数描画でも散発遅延が再現

- 関連: temp/video-render-cycles-repeat-1.json、repeat-2.json、Composition render ticker。
- 確認事実: 4-layer各60秒の独立2試行は両完成描画1800／表示通知1800、drop/repeat0、late0/1。失敗試行はframe68、queued18.49ms、render-to-present15.19ms、total33.68ms。再生開始の既存PlaybackStateChangedでticker intervalが1msへ更新されるコードも確認。
- 価値: この失敗を余分な完成描画だけで説明する仮説は支持されない。
- 未検証: OS scheduling、背景decode/prefetch、swapchain待ち、ベンチの250ms診断sampleの影響。次にこれらの時間を低負荷で分離し、合否閾値を緩めず調査する。

### 2026-10-13 — 定期診断sampleの観測負荷を数値化

- 関連: tools/VideoPreviewBench/main.cpp、export_report.py。
- 確認事実: 既存QElapsedTimerを使いvideo＋renderer診断の最大時間と開始時刻を固定scalarで保持。4-layer60秒試験は表示/描画1800、drop/late/repeat0。最大sample2.3833ms、video部分2.2934ms。新規ログ・container・product変更なし。
- 価値・懸念: 診断自体のGUI占有を無視しない。現試行はlateなしのため因果は未検証。既存QString正規表現/QJson sample確保は残り、今回の追加scalar計測自体に確保はない。
- 次の確認: 同一診断版の失敗試行と最大sample時刻の比較、msleep/GUI dispatchの時間分離。renderer平均時間には準備区間が混ざるため、区間内平均として使う前に集計範囲も確認する。

### 2026-10-13 — 動画upload cache上限と共有cache fallbackの区別

- 関連: Artifact/src/Layer/ArtifactVideoLayer.cppm goToFrame、prepareDecodeRequest、hasCachedUploadFrame、buildUploadFrame。
- 確認事実: goToFrameはComposition RAM再生でもsource更新とdecodeCurrentFrame準備へ進む。ローカルuploadFramesは4-frame/64MiB上限だが、hasCachedUploadFrameは共有Asset decodedPayloadも確認し、共有uploadがあればenqueueを避ける。worker側でも共有uploadを再利用して変換を省略する。
- 価値・懸念: ローカル4-frame上限だけではsteady-state変換の原因と断定できない。上限拡張やdecode停止の根拠はまだない。
- 次の確認（未検証）: Composition RAM再生中の共有lookup hit/missと準備時間。全layer frame評価を止めると動的property/audio/simulationへの影響があり、単純なgoToFrame省略は採用しない。

### 2026-10-13 — disk併用時の失敗と準備完了条件

- 関連: VideoPreviewBench waitForRamPreview、PlaybackService disk writer、export_report.py。
- 確認事実: disk有効4/8-layer各60秒でdrop1/0、late6/3、repeat0。最終disk90であるが測定開始時のdisk完了数はレポートに残っていない。既存準備はRAM-readyのみでreturnする。
- 実施: 任意wait-disk-cacheでRAM＋disk全range完了を待つ比較条件を追加。製品コード変更なし、ビルド成功、無効option組合せexit2。実再生比較は未検証。
- 価値・懸念: 保存ワーカーの負荷を原因と断定しない。RAM併用なのでdisk-only read性能の証拠でもない。外部OS peak観測はwhole-process Working Set約10.57GiBで、case別集計や最後の未sample peakではない。
- 次の確認: 準備時disk数を別fieldに保持し、waitあり/なしを同条件で反復。

### 2026-10-13 — ベンチmeasurement event pumpの切り分け候補

- 関連: tools/VideoPreviewBench/main.cpp measured playback loop。
- 確認事実: measurementはprocessEvents(AllEvents,10)とQThread::msleep(2)を繰り返す。既存250ms診断は最大約2–3msだが、sleep自身の実待機時間・GUI dispatch遅延は未取得。
- 仮説（未検証）: polling/sleepが自然なQt event loopと異なるGUI応答を作り、散発lateに寄与する可能性。製品rendererだけの問題と断定しない。
- 次の確認: QBasicTimer＋既存QEventLoopで測定deadline/sampleを処理する独立harness比較。新signal/slotやtimerごとのheap allocationを追加せず、合否を緩めない。既存条件の結果も保持する。

### 2026-10-13 — disk保存完了後にもlateが残る

- 関連: VideoPreviewBench prepared cache counts、wait-disk-cache。
- 確認事実: mixed4/8-layer各60秒の開始時はRAM90/disk90。両表示/描画1800、drop/repeat0、late1/0。4-layer first-lateはframe17、queued19.74msとrender-to-present14.50msでtotal34.24ms。
- 価値: disk未完了のみを原因とする説明は支持されない。待機なしcaseとの件数差は試行変動があり因果未確定。
- 次の確認: 同じprepared countを記録した反復と、GUI measurement event loop比較。基準や合否を緩和しない。

### 2026-10-13 — measurement dispatchの比較実装

- 関連: VideoPreviewBench PreviewMeasurementLoop、measurement-loop option。
- 確認事実: polling既定値を維持してqt modeを追加。stack QEventLoop/QBasicTimer2個と参照callbackで、250ms診断と残り測定時間deadlineを処理する。製品transport/render変更なし、新signal接続・標準container・callback heap所有なし。object/link成功。
- 価値・懸念: ベンチ自身の2ms polling sleepを分離する比較条件。timer登録は測定開始のcold boundaryで行うが、Qt内部のallocation全体を測定したとは言えない。
- 次の確認: 同一保存完了条件でqt/polling反復、native例外検証。単発合格を因果proofにしない。

### 2026-10-13 — Presentation ACKの成功判定を厳格化

- 関連: ArtifactCompositionRenderController::presentAndUpdateVideoLayers、ArtifactIRenderer::present、VideoLayer::markPresented。
- 確認事実: controllerはstatusにfailが含まれなければPlaybackServiceに表示成功を通知していた。rendererにはskipped-no-swapchain、device-recovery-deferred-shared、device-recovered、およびfailを含まない例外messageが存在する。VideoLayer側はok以外をbottleneck=presentとするが、stage/outcomeは時間と重複だけで判定する。
- 修正: controller ACKもokのみへ1行で統一。新API/待ち/確保なし。現在のdispatch比較は旧binary固定のまま完了させ、その後にobject/linkとnative再検証する。
- 懸念・次の確認: skip/recoveryを実表示と数えない正しさの修正であり、現在の通常lateの原因を説明したとは言えない。故障注入による各statusのruntime確認は未検証。

### 2026-10-13 — 同binary dispatch反復比較の結果

- 関連: compare_dispatch_reports.py、temp/video-dispatch-paired-comparison.json。
- 確認事実: polling/qt/qt/polling各60秒の報告条件と起動直前SHA256は全一致。polling合格1/2（drop1/0、late1/0）、Qt合格2/2（drop/late/repeat0、表示1800）。全試行準備RAM90/disk90。
- 価値・懸念: dispatch寄与の可能性はあるが、少数逐次試行から因果や安定合格は証明できない。比較binaryに後続ACK修正は含まない。
- 次の確認: 最新ACK修正版でQt mode全layer matrixと反復、長い固有source区間、physical表示。ACK修正後nativeは例外・重大GPU/CRTエラー0、2/8-layer各90表示、late各1、正常exit1。故障注入は未検証。

### 2026-10-13 — Qt/ACK全matrix未達とWindows短待機候補

- 関連: video-qt-ack-full-matrix-60s.json、probe_windows_waits.py、ArtifactCore/src/Thread/PreciseTicker.cppm。
- 確認事実: 全1..8-layer各60秒は全表示/描画1800、drop/repeat0、late0/0/0/0/1/1/9/27。開始cache RAM90/disk90。実行直前/終了後exe hash一致、外部memory観測終了、Working Set peak約10.48GiB。Qt dispatch単独では全条件未達。
- 独立probe: 1ms要求各256回、Sleep/通常waitable/high-resolution waitableのp50は15.4294/15.5977/1.5371ms。設定・priorityは変更せず自timer handleのみ作成/close。非GUI Pythonであり、実Qt/C++ CV/previewの証拠ではない。
- 仮説（未検証）: PreciseTickerのCV wait_untilが要求1msより粗い待機になり、queued15–22ms＋Present12–19msのlateに寄与する可能性。
- 次の確認: 実tickerと同じC++/GUI環境の待機測定を先に行い、global timer resolutionやpriority変更ではなく再利用するhigh-resolution waitable timerを検討。stop/reschedule、fallback、handle lifetime、他ticker利用者への影響を読んでから最小修正する。
- 参照: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw 、https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod

### 2026-10-13 — 実レンダーtickerの短待機計測

- 関連: ArtifactCompositionRenderController.cppm、temp/video-real-render-dispatch-profile.json。
- 確認事実: 8動画レイヤー、Qt測定ループ、disk-readyの診断実行で、要求1msの実ticker間隔63件は中央値1.555ms、最小1.028ms、最大28.272ms。独立Python待機probeの約15msを実tickerへ当てはめる説明は支持されない。診断有効の試行はlate1であり性能合格の根拠ではない。
- 次の確認: 既存workerからGUIへの投稿とcallback開始の時間差をboundedで記録する。共通PreciseTickerの待機方式はこの結果だけでは変更しない。

- 続くGUI受け渡し診断: temp/video-gui-dispatch-profile.json/.log。64件の投稿からGUI callback開始まで中央値3.910ms、最大32.710ms、10ms超20件。ticker63件は中央値1.556ms、最大28.623ms。8レイヤー3秒試行はdrop0/repeat0/late2で未達。診断ログ自体の影響があるため受け渡しの遅延存在を示すに留め、原因や改善量は断定しない。次はGUI側の処理占有時間を特定する。

### 2026-10-13 — 提示後処理と診断文字列の切り分け

- 関連: ArtifactCompositionRenderController.cppmのpresentAndUpdateVideoLayers、ArtifactStatusBar.cppm。
- 確認事実: Present成功通知の後に各動画のmarkFramePresented、最初の可視動画のdecodeState文字列生成があり、描画側は診断eventを発行する。StatusBarは文字列の大文字化とelide、tooltip更新を行う。
- 仮説（未検証）: この経路がGUI占有に寄与する可能性があるが、存在だけでは遅延原因の証拠にならない。
- 次の確認: 既存profileフラグでPresent呼び出し／提示後bookkeepingを64件まで別々に測る。通常実行ではclock読み取りと整形を行わない。診断eventの削除やUI挙動変更は未実施。

- 提示区間の診断結果（2026-10-13）: `temp/video-present-stage-profile.json/.log`。8混合レイヤー、Qt、disk-ready、cache window1秒／測定3秒。64件のPresent中央値3.3755ms／最大7.414ms、提示後bookkeeping中央値0.240ms／最大1.714ms、GUI待ち中央値4.3595ms／最大23.064ms。drop／late／repeatは0でexit0。ただし3秒windowの前回とは条件が異なる短診断であり安定性合格や改善量を示さない。提示後更新だけを主因とする説明はこの結果では支持されず、GUIの他の処理占有／描画開始前を引き続き調べる。

### 2026-10-13 — 描画tickerのidle停止とworker再生成候補

- 関連: ArtifactCompositionRenderController.cppmのrenderDirty空判定とmarkRenderDirty、ArtifactCore/src/Thread/PreciseTicker.cppm。
- 確認事実: 描画要求が空のcallbackはviewport操作中でなければtickerをstopする。markRenderDirtyは停止中ならstartする。stopはworkerをjoinし、startはstd::threadを生成する。
- 仮説（未検証）: 再生フレーム間でもこのidle判定が走るとworker生成とjoinが毎フレーム発生し、GUI待機の揺れに寄与する可能性。停止時playing状態とframeSerialを64件まで診断して確認する。共通tickerの実装変更は未実施。

- 実測追記: temp/video-idle-stop-profile.json/.logの8混合レイヤー、cache window1秒／測定3秒で停止診断64件中61件がplaying=1、frameSerial58–118の連続フレームだった。drop／late／repeat0の短試行でも毎フレーム停止がある。再生中のidle判定でworkerを維持する最小修正を実施し比較準備中。通常の停止中idle／明示停止経路は維持。idle時も既存のcoalesced Qt投稿が続くため、その小さなイベント確保・CPU負荷と遅延を次の実行で確認する。

- 2026-10-13 実行結果: `temp/video-playback-worker-retain-60s.json`。修正後、診断無効、8混合レイヤー、Qt、disk-ready、3秒window／測定60秒でpreparedRAM90／disk90、描画1800／提示1800、drop0／late0／repeat0、exit0。最大diagnostic sample7.5894ms。旧全matrixの8レイヤーlate27との条件は主要項目が一致するが、別試行かつmatrix内順序・熱状態差があるため因果や安定性はまだ断定しない。全matrix反復、長い固有フレーム、物理scanoutは未完了。

- 2026-10-13 native確認: `temp/video-native-playback-worker-retain-debug.log` はexceptions0、D3D12 ERROR／CORRUPTION等の重大エラー0、正常な合否終了exit1。2／8レイヤー各3秒、window1秒、disk-ready、Qtで提示／描画90ずつ、drop／repeat0、late1／0。debugger下の2レイヤーはタイミング未達。Live Object警告は残る。通常実行の性能合格として扱わない。1–8レイヤー各60秒、window3秒の通常全matrixを開始し、`temp/video-worker-retain-full-matrix-60s-launch.json` に実行前hashとargumentsを保存した。終了時hash／exitは終了後に保存する。

### 2026-10-13 — 長い固有区間のcacheゲートと復元境界

- 関連: tools/VideoPreviewBench/main.cppのprepareRamPreview、ArtifactPlaybackService.cppmのeviction／hydrateFrameFromDisk／storeRamPreviewImageData。
- 確認事実: 全RAM-readyゲートと128-frame evictionは128超区間で両立しない。同期disk復元と停止中限定RGBA8 sidecar生成も長いrolling経路の制約。
- 価値: 3秒cache反復の成功を長いsource安定性へ拡張して解釈しない。
- 次の確認: 全disk準備と先読みRAMを別ゲートにし、既存所有権境界を保つbounded非同期復元の設計・実装・検証。詳細を既存milestoneの次段階へ記録した。未実装。

### 2026-10-13 — 非同期disk復元の共有状態境界

- 関連: ArtifactPlaybackService.cppmのdisk writer、loadPreviewDiskManifestSnapshot、isPreviewDiskManifestFrameValid。
- 確認事実: writerはpreviewDiskWriteMutexを保持してPNG保存、容量eviction、manifest書込を行う。GUIからのwrite enqueueとcache削除も同じmutexを使う。manifest読込memoはサービスのmemberへ書き込むため、既存hydrate／validation関数をそのままworkerで呼ぶ方式は共有状態の競合を招く。
- 懸念（未検証）: writerとの共用キューは保存の重い処理の後ろへ先読みを滞留させる可能性。現在のwarm-cache試行に影響する原因だとは断定しない。
- 次の確認: worker専用manifest snapshotとimmutable request identityを使い、GUI所有LRU／frame statesへ触るのは結果反映のみとする。frame、namespace、composition、generation、期待サイズ／形式を結果へ保持し、世代不一致を捨てる。保存・復元のbounded schedulingと終了処理を設計してから実装する。

### 2026-10-13 — rolling復元の画像APIと色契約

- 関連: ArtifactCore/src/Image/ImageF32x4_RGBA.cppmのload、ImageUploadConversion.ixx、ImageF32x4_With_Cache。
- 確認事実: ImageF32x4_RGBA::loadはOIIO FLOAT／reorient／RGBA channels経路でunknownRgba32Floatを設定し、失敗時はQImageのpremultiplied BGRA互換経路へ進む。既存disk hydrateはQImageからRGBA8へ明示変換する経路なので、loadへ単純置換すると色・alpha契約が一致するとは証明できない。upload変換には既存convertImageForUploadIntoがある。cache wrapperはswap APIを持ち、固定slotからGUIcacheへの所有移譲の候補となる。
- 次の確認: PNG cacheのsRGB／straight alphaを明示し、半透明・黒・白・色パッチの比較でdisk/RAM一致を確認してから採用する。固定slot／byte budget／stale世代廃棄とともに設計し、GUI側の深いcopy・大容量確保を持ち込まない。未実装・未検証。

### 2026-10-13 — CPU RAM画像とuploadの合算budget

- 関連: ArtifactPlaybackService.cppmのevictRamPreviewImageCacheIfNeeded、storeRamPreviewImageData。
- 確認事実: CPU image evictionは128-frame上限、upload sidecarは別1GiB byte上限。全CPU画像のbyte上限は当該経路では未確認。全matrix中PID16700の単一working set観測は10857377792bytesで、peak／cache単独値ではない。temp/video-worker-retain-mid-matrix-memory-note.jsonに観測とnominal payload計算を保存。2400x1195のRGBA32F＋RGBA8は90枚で5162400000bytes、128枚で7342080000bytes（複製・GPU・queue等を除く）。
- 懸念: rolling poolを128枚だけで制限してもbyte量は大きい。新たな容量制御はRAM ready契約と先読み数を同時に設計し、既存試験を単純なeviction変更で壊さない。
- 次の確認: CPU＋upload＋in-flight＋retired slotを合算した固定byte budgetと、サイズ検証／allocation前reservation／高解像度でのbounded backpressureを実装する。現在のrenderer性能原因や単一観測をpeakと解釈しない。

### 2026-10-13 — 全matrixのメモリ継続観測

- 関連: temp/video-worker-retain-memory-observer.py、temp/video-worker-retain-full-matrix-memory.json。
- 確認事実: 実在するbench PID16700へquery/read/synchronize handleを1回だけ取得し、1秒間隔最大3600件でworking set、OS lifetime peak working set、private bytes、peak pagefileを記録するread-only観測を開始。case4途中からで、先行caseの時系列はない。初回保存を確認し、working set10935255040／peak11099897856／private10746658816bytes。process exit待機はhandleを用い、PID再利用された別プロセスへ再接続しない。終了または上限でhandleを閉じる。停止／kill／priority変更は行わない。
- 価値: 単発観測だけでpeak・リーク・増加傾向を判断しない。記録中はprocessExited=falseが未終了を示し、末尾区間が未観測の可能性も残る。CPU画像・GPU資源・allocator保持を区別できる値ではない。
- 次の確認: matrixとobserverの終了を別々に確認し、完成reportとlaunch hash、whole-process memoryをoffline exportへ添付する。1–3レイヤー各60秒の途中case結果はdrop／late／repeat0、最終JSONはまだ未完成。

### 2026-10-13 — 試行実行記録の再利用

- 関連: tools/VideoPreviewBench/run_recorded.py。
- 実装: 新規prefixに限って独立benchを実行し、exe／startup fileの前後hash、arguments、selected environment、exitを記録するrunner。既存report上書きを事前拒否し、runner例外時も終了記録を残す。
- 確認: help正常、既存prefix拒否exit2で新たなbenchを起動しなかった。実際の新規試行は未検証。
- 次の確認: 現行matrix終了後の反復試行で採用し、exportのlaunch-recordと一致することを確認する。loaded DLLと実効startup flagsの証明ではない。

- worker維持の全matrix途中結果追記（2026-10-13）: 1–6レイヤー各60秒はdrop／late／repeat0、7レイヤーはdrop0／late5／repeat0で条件未達。temp/video-worker-retain-full-matrix-interim.jsonへログ集計を保存。8レイヤーは進行中で、最終per-layer firstLate診断は終了reportで確認する。単独8レイヤー成功を全matrix安定性の証明へ拡張しない。worker再生成の解消だけで十分とは言えない。

### worker維持・全matrix終了（2026-10-13）

`temp/video-worker-retain-full-matrix-60s.json` は1–8レイヤー各60秒、Qt、disk-ready、window3秒。全case提示1800／描画1800、preparedRAM90／disk90、drop／repeat0。lateは0／0／0／0／0／0／5／1でexit1、未達。launch前後exe hash一致、observerは終了確認済み642件、OS peak working set11413118976bytes（全process／途中開始、cache単独ではない）。CSVとmanifestへlaunchとmemoryのfingerprintを添付した。

7レイヤー最初のlateはframe54・開始1806.25ms、decode0、queued6.45ms、render-to-present29.37ms、全体35.83ms。8レイヤーの最初のlateはProRes layer2・frame59・開始55969.30ms、decode0、queued1.60ms、render-to-present31.79ms、全体33.39ms。単独8レイヤーの成功で安定性は証明できず、次はrender区間全体とPresent／uploadをbounded診断する。

### 2026-10-13 — 再生区間の描画profile追加

- 実測: temp/video-worker-retain-render-duration-7.json/.log。measure marker後の7レイヤー診断で描画64件中央値16.2385ms／最大24.040ms、Present中央値3.3445ms／最大9.623ms、bookkeeping中央値0.2485ms／最大1.671ms。prepared upload90件update中央値0ms／最大3ms（整数ms量子化）、conversion／creation最大各1ms。GUI待ち64件中央値0.059ms／最大22.005ms。drop0／late1／repeat0で未達。診断有効の短試行であり改善の証明ではない。
- 確認事実: 既存SetupProfileはrenderFrameCounter<32のみなのでcache準備でサンプルを使い切り、measure区間を記録しない。再生中用の独立64件bounded counterを追加し、既存phase marksを再利用して残りの描画区間を計測する。通常実行のclock／整形を増やさない。
- 次の確認: setup phaseを再計測し、render全体とupload／Present以外の時間を確認する。新recorded runnerで実試行が終了し、exit1と実行前後exe hash一致を確認済み。

- 再生SetupProfile結果（2026-10-13）: temp/video-worker-retain-playback-setup-7-summary.json、7レイヤーのmeasure marker後64件。preScene中央値0.01ms、sceneLights0.235ms、envShadow0.01ms、cameraRange0.02ms、playbackState0.01ms、damage0.04ms、pipeline0.41ms、graphSetup0.02ms。renderPresent区間中央値15.645ms／最大26.24ms。短診断はdrop／late／repeat0でexit0だが通常全matrix未達を覆さない。初期scene／damage設定よりrenderPresent内部が支配的。次にRAM base／composite／overlay／submit／presentの既存pass境界を切り分ける。

### 2026-10-13 — RAM描画pass内の区間計測

- 関連: ArtifactCompositionRenderController.cppmのRAM Base／Composite、Overlay、SetupProfile。
- 実装: 既存setup marksを固定14要素へ拡張し、RAM pass前後・Overlay前後を記録。profile有効かつ既存bounded枠内だけRAM pass／between／overlay／tailPresentを出力する。描画順序／内容／event経路は未変更。
- 次の確認: 7レイヤーの再生中64件でrenderPresent中央値約15.65msの内訳を調べる。固定stack領域の増分32bytes、heap collectionや毎フレームログの無制限追加はない。通常実行では計測時計や整形を行わない。ビルド・実測は確認中。

- RAM pass実測追記（2026-10-13）: temp/video-worker-retain-ram-pass-7-summary.jsonのmeasure後64件。RAM pass中央値1.857ms／最大13.411ms、between中央値8.3795ms／最大15.313ms、overlay中央値0.0755ms／最大1.071ms、tailPresent中央値5.9855ms／最大17.883ms。短診断drop／late／repeat0。between内のshadow finalize／flush／selection overlayを追加分割する。ArtifactIRenderer::flushはsubmitQueuedDrawsの後にctx->Flushし、既存flushContextTimeUsはctx->Flushだけを測るため、submit全体時間と同一視しない。barrierの削除は未実施。

- finalize分割結果（2026-10-13）: temp/video-worker-retain-ram-finalize-7-summary.jsonのmeasure後64件。beforeFinalize中央値0.002ms／最大0.004ms、finalize中央値2.316ms／最大16.578ms、selection中央値0.0235ms／最大0.050ms、afterSelection0ms。先のbetween中央値8.38msとの差は別試行変動で、改善量とは解釈しない。中間区間はshadow／submitQueuedDraws／context Flush／RTV復帰に集中する。DiligentImmediateSubmitterはdeferred contextがある場合Begin／FinishCommandList／ExecuteCommandLists／FinishFrameを使い、DeviceManagerはdeferred contextを作成している。これが主因かは未検証。次はsubmitとcontext Flushを分ける。barrierやdeferred経路の変更は未実施。

### 2026-10-13 — context Flushとsubmitを既存counterで分離

- 関連: controllerのRAM finalize、ArtifactIRenderer::flushContextTimeUs。
- 実装: 既存64件setup profile枠内でrenderer.flush全体の高精度時間と既存context Flush累積counter差分を同時に出力する。共通renderer／Diligent依存は未変更。全体からcontext時間を引いた残差にはsubmit以外の少量処理も含み、純粋なsubmit時間と断定しない。
- 確認: controllerのみcompile／standalone relink成功、diff check成功、7混合レイヤー診断実行中（temp/video-worker-retain-ram-flush-7）。直前のfinalize診断は提示90、drop0／late1／repeat0、正常合否終了exit1、exe前後hash一致。
- 次の確認: Flush自身が長いか、その前のcommand recording／executionが長いかを実測する。通常実行では追加clock／counter取得／整形を行わない。

- Flush内訳の実測（2026-10-13）: temp/video-worker-retain-ram-flush-7-summary.jsonのmeasure後64件。flush全体中央値8.2035ms／最大17.222ms、context Flush中央値0.004ms／最大0.021ms、残差中央値8.1985ms／最大17.219ms。短診断drop／late／repeat0でexit0。残差はsubmitQueuedDraws（2D submitと3D gizmo flush）と微小処理を含む。context Flush自身を主因とする説明は支持されない。checkerboardは既存CheckerboardPkt1件でGPU描画するため、多数のCPU矩形記録を原因として扱わない。次に2D submitのrecord／FinishCommandList／ExecuteCommandLists／FinishFrameと3D gizmo flushを計測する。汎用Profilerの有効化はframe／scopeログに動的確保を伴うため、bounded診断の代替として無条件に有効化しない。

### 2026-10-13 — 2D submitと3D gizmo送信の分離

- 関連: Artifact/src/Render/ArtifactIRenderer.cppmのsubmitQueuedDraws。
- 実装: 既存PROFILE_SETUPで2D submit／3D gizmo flush時間とpacket数を測る。診断時のみ非空submit最大4096件／threadの固定counter、通常時clock／packet size取得／整形は行わない。準備区間のsubmitが多いためcontrollerの64再生枠とは別枠にし、measure marker後のみ集計する。公開API・signal・新規container・Diligent依存変更はなし。固定counter以外の保持容量は増やさず、stdioは明示flushしない。
- 次の確認: renderer object／static library／standalone exeの順に更新し、7レイヤーの再生中サンプルを比較する。profileによる追加ログの影響があるので性能合格に使わない。

- 2D／3D送信実測（2026-10-13）: temp/video-worker-retain-submit-7-summary.json、measure後180非空submit。2D中央値2.1645ms／最大14.129ms、3D gizmo中央値0.001ms／最大0.027ms。短診断drop／late／repeat0でexit0。2D側の揺れに絞り、DiligentImmediateSubmitter.cppmへrecord、FinishCommandList、ExecuteCommandLists、FinishFrameのbounded stage診断を追加。4096件/thread、既存PROFILE_SETUP、公開APIと描画動作未変更。headerはglobal module fragmentへ直接include。compile／library／standalone link確認中。

### 2026-10-13 — 小さなserial submitのcommand-list handoff

- 実測: temp/video-worker-retain-submit-stages-7-summary.json、measure後180件すべてdeferred、packet数3と9が各90件。record中央値0.465ms／最大8.306ms、FinishCommandList中央値0.0305ms／最大0.051ms、ExecuteCommandLists中央値1.712ms／最大11.808ms、FinishFrame中央値0.011ms／最大0.029ms。短診断drop0／late1／repeat0、exit1。
- 実装: 16 packet未満は既存immediate submit経路を使い、より大きなbatchはdeferredを維持。既存compute consumer例外は維持。変更はsubmitter cppmの選択条件のみで、描画順序／PSO／resource transition／GPU buffer仕様は変えない。16という境界は初期実装判断であり最適値は未検証。
- 次の確認: 同じ7レイヤー診断でrecord／executeと全体時間、通常7／8長時間、native例外とD3D12エラー、表示一致を確認。性能改善・正しさはまだ未証明。新規hot allocation／containerなし、依存Diligent変更なし。

- 小submit immediate診断（2026-10-13）: temp/video-small-submit-immediate-profile-7-summary.json、measure後180件、deferred0件。record中央値0.3815ms／最大1.897ms、FinishCommandList／ExecuteCommandLists／FinishFrameは使わず0。短7レイヤー診断はdrop／late／repeat0でexit0。単一診断・ログ有効なので通常長時間安定性と表示一致はまだ未証明。次はcanonical exeのnative2／8レイヤーと通常7／8長時間、cold明示readbackを使う表示確認を進める。

### 小submitのimmediate経路・検証中（2026-10-13）

再生中の2D送信は3／9packetで、旧deferred経路のExecuteCommandListsは最大11.808msだった。DiligentImmediateSubmitter.cppmの選択条件を16packet未満は既存immediate経路へ変更した。大きなbatchとcompute consumer例外、描画順序／PSO／resource transitionを維持。境界16の最適性、他描画内容の表示一致は未確認。共通submitterの変更なので動画以外の小batchにも作用し、表示確認が必要。Diligent依存や公開APIは未変更。

修正後7レイヤー診断は180非空submitすべてimmediate、record中央値0.3815ms／最大1.897ms、drop／late／repeat0。native2／8レイヤー各3秒はexceptions0、重大D3D12エラー0、late1／0で正常合否終了exit1。Live Object警告は残る。7／8レイヤー各120秒、window3秒、Qt、disk-ready、診断無効の通常試行を開始（temp/video-small-submit-immediate-7-8-120s）。長い固有source／他レイヤー表示／physical scanoutの証明ではない。

### 2026-10-13 — 性能区間外のGPU画像保存

- 関連: tools/VideoPreviewBench/main.cpp。
- 実装準備: 任意capture-final-frameを追加。report必須、測定とvideo診断の完了・停止後に既存renderer.readbackToImageを明示呼出ししPNGへ保存、caseのpostMeasurementCaptureへpath／size／saved／取得元を記録する。QImageは既存Qt入出力境界のcold診断のみで、描画や合成に採用しない。既存captureCurrentFrameImageのscreen fallbackへは委譲しない。既存PNGは上書きせず保存失敗時overallExit1。性能カウンターの合否と画像保存結果は別記録。
- 制限: GPU render targetの終了時読み戻しであり、physical scanout／全再生フレームの表示／frame identity一致の証明ではない。capture未指定時はreadbackしない。
- 次の確認: 現在の120秒試行終了後、bench mainだけcompile／relinkし、保存画像の目視とcolor／alpha／black検査を進める。現時点では未ビルド・未実行。

- 小submit immediate通常試行の途中結果（2026-10-13）: temp/video-small-submit-immediate-7-8-120s.logで7レイヤー／window3秒／診断無効／120秒のcase終了を確認、drop／late／repeat0。8レイヤーは準備中。最終JSONが出るまで提示数などの全項目は未確認。画像保存オプションは未ビルドのままなので、この試行条件には影響しない。

### 2026-10-13: 停止後swapchain readbackのframe対応

- 関連: Artifact/src/Render/ArtifactIRenderer.cppm、tools/VideoPreviewBench/main.cpp。
- 確認済み: activeColorViewはcurrent backbufferを優先する。
- 未検証: Present後readback画像と直前の提示frameとの対応。buffer rotationにより一致しない可能性がある。
- 価値／懸念: 単一GPU画像を最終提示frameや物理scanoutの証拠と誤認しない。
- 次の確認: 明示的な対象texture／frame identityを保持できる既存経路を調べる。現在の画像保存は診断用途に限定する。

### 2026-10-13: disk hydration上限は走査件数を制限しない

- 関連: Artifact/src/Service/ArtifactPlaybackService.cppm、hydrateFramesFromDiskNear。
- 確認済み: touchedFramesはhydrateFrameFromDiskがtrueのときだけ増加する。cache欠損時にはmaxFramesToTouch=9でもrange全体を探索し、各候補で同期file／manifest確認を行い得る。
- 懸念: 画像読込件数の上限と、GUIでの探索／I/O件数の上限は別。長いrangeの欠損や壊れたcacheで入力応答へ影響する可能性（時間は未計測）。
- 次の確認: 既存range readiness契約を保ち、候補走査の固定上限と継続cursor／非同期readerを設計する。現在の短い全RAM試験ではこの経路の改善効果を判断できない。

- 上記候補探索上限の進捗（2026-10-13）: 成功ではなく試行件数を数える最小修正を実装。未ビルド／runtime未確認。同期デコードとrange全体disk readinessの課題は残る。

### 2026-10-13: 非同期disk復元後のGUI DeepCopy境界

- 関連: ArtifactPlaybackService.cppm::storeRamPreviewImageData、ArtifactCore/src/Image/ImageF32x4_With_Cache.cppm::SetCpuImage。
- 確認済み: 保存先のSetCpuImageはDeepCopyする。既存swapはimpl pointerを交換する。Image／wrapperはQObject。
- 懸念: decodeだけのworker化では、結果反映時の大容量コピー／確保がGUIへ残る。worker生成QObjectの移譲をswapだけで安全と断定できない。
- 次に確認: GUIコールド生成slotのraw作業域をworkerへ貸す設計、GPU resource lifetime、GUI cacheへの交換、全slot合計bytesの上限。未実装／未検証。

### 2026-10-13: disk書込queueの画像所有と容量上限

- 関連: ArtifactPlaybackService.cppm::PreviewDiskWriteTask、3箇所のpreviewDiskWriteQueue_.push_back。
- 確認済み: taskはImageF32x4_RGBAを値で所有し、enqueue前にqueue件数／合計bytesの上限を確認していない。通常captureと2つのstore overloadでenqueueする。
- 懸念: writerが遅いと画像snapshotが累積し得る。今回whole-process memory peakへの寄与量は未計測で、主因とは断定しない。
- 次に確認: snapshot bytes／queue high-waterをbounded診断し、固定slotとdefer契約を設計する。満杯時のframe破棄、RAM画像の無所有borrow、GUI同期待機への置換は避ける。既存persistToDisk／generation／invalidation契約を維持する。未実装。

### 2026-10-13: 中断で消えるcase診断の保存境界

- 関連: tools/VideoPreviewBench/main.cpp、run_recorded.py。
- 確認済み: 従来reportは全case終了後にだけ保存され、途中終了時にはfirst-late等のcold snapshotを失う。hydration-bound全matrixはprocess／session消失、reportなし、launch exit nullで終了原因未確認。
- 実装: case境界でQSaveFileに専用schema／complete=falseのcheckpointを保存。runnerは既存partialを起動前拒否。測定中の新規I/Oはなし。未ビルド／runtime未検証。
- 次の確認: case結果とcheckpoint一致、exporterの未完了schema拒否、保存失敗時exit、同exeの周期／終了時のみ診断比較。

### 2026-10-13: RAM合成textureの単一更新slot

- 関連: Artifact/src/Render/PrimitiveRenderer2D.cppm::textureForVideoFrame、CompositionRenderControllerのRAM Preview Composite。
- 確認済み: texture cache keyはowner IDと画像寸法。frame内容は同一textureへUpdateTextureし、前frameと同じtextureを再利用する。RAM合成でもこの経路を使う。
- 仮説（未検証）: GPUが前frameを参照中の更新がsubmission／resource transitionの待ちや揺れに関係する可能性。CPU upload profileだけではGPU依存待ちを断定できない。
- 次の確認: Diligentの所有laneでUpdateTextureとdrawの順序、GPU completionとslot寿命を測定する。固定複数slotは明示completion／総bytes上限／cache invalidationを含めて検討し、単に輪番上書きして安全と見なさない。現在のABBA中はrendererを変更しない。

### 2026-10-13: firstLateQueuedMsはGUI queue単独ではない

- 関連: ArtifactVideoLayer.cppm::markRenderQueued／markFrameCompositionCacheReady、CompositionRenderController.cppmのRAM経路。
- 確認済み: requestedAtからmarkRenderQueuedまでを計測し、cacheReadyはtimestampを保持する。GUI callback開始より前後の処理を含み得る。
- 懸念: 155msをGUI queue単独の停滞と断定すると誤った最適化へ進む。
- 次の確認: frame identity付きdispatch待ちとpre-draw各stageを照合する。現時点では要求→描画開始の合計のみ確認。

### 2026-10-13: 再生tickerのidle GUI投稿

- 関連: CompositionRenderController.cppm::compositionRenderDispatchIntervalMs／PreciseTicker callback／markRenderDirty。
- 確認済み: 再生中interval1ms、workerはpendingをcoalesceするがdirtyを確認せずGUIへ投稿し、GUI側でdirty=falseならreturnする。worker維持修正により毎frameの再生成は避けたが、idle GUI投稿は継続する。
- 懸念（未計測）: callback処理とQt queued event確保がフレーム間でも発生する。今回の遅延原因とは断定しない。
- 次の確認: resize ghostの継続描画、interaction timeout、停止／hidden／同期gateの責務。workerからGUI専用状態を読まず、idle投稿だけ減らせるatomic demand契約を検討する。単純dirty gateはこれらの機能を壊し得るため未実装。

### 2026-10-13 — 再生中のRAM captureに生成キュー先頭制約

- **関連:** `ArtifactCompositionRenderController::PreviewFrameRequest`／`publishPreviewFrameRequestResult()`、部分cache bench。
- **事実:** 描画frameと生成キュー先頭の一致を投入時と完了時に要求していた。部分準備30frame／10秒試行で提示141、合成cacheは30frameのまま。
- **修正:** 再生中だけ描画frameをtargetとし、先頭との一致を要求しない。世代・composition・target identity・pending membershipは保持。paused warmupは同じ制約を保持。
- **価値／懸念:** 先頭の待ちが後続captureを止める条件を除去する。生成増加と性能への影響は未検証。新たに進むreadback／保存量は既存の容量制限との関係を確認する必要がある。
- **次に確認:** 同条件prefix試行でcache増加を確認し、例外・GPU debug layer・lateを別々に判定する。
- **追加確認:** 修正後もRAM30のまま。nativeでframe30／120のpendingBuild=falseを確認。bench prefix設定がradius30へ縮め、play()のprewarmで生成範囲まで30に戻ることが実試行の直接原因。prefix準備数とradiusを分離。先頭制約の修正だけではこの試行の停止を解決しなかった。

### 2026-10-13 — live captureのQt境界変換重複

- **関連:** `ArtifactPlaybackService::Impl::storeRamPreviewFrameImage(QImage)`、`storeFrameImageInRam()`。
- **事実:** RAM取り込みでimageToCpuPreviewFrameを呼び、disk task構築でも同じQImageを再変換していた。diskなしprefix試行も提示94／late64で性能不合格。
- **変更:** Qt境界の変換を1回にしCPU版store overloadでRAMとwriterの所有を扱う。既存copy契約は維持。修正後prefix試行は提示48／late19／drop1で、提示47から速度改善は確認できず。
- **懸念／次に確認:** GUI上のRAM画像cloneとwriter queue mutexのPNG処理待ちが残る。単独効果を実測し、容量と世代を守った非同期引き渡しを検討する。

### 2026-10-13 — native readback poolのleaseはGUI受け取りまで必要

- **関連:** ArtifactIRenderer async readback、Composition controllerのcache publication。
- **事実:** staging fence完了だけでslotを再利用すると、GUI待ちのnative byte payloadが上書きされ得る。旧QImage経路は独立pixel storageの所有でこれを回避していた。
- **実装:** 既存ImageUploadBufferを3slotに冷間準備し、SharedPtr useCountでGUI受け取りまで再利用を拒否。worker／GUI／renderer破棄をまたぐbyte storageの寿命を共有所有で維持する。1slot64MiB上限。
- **価値／懸念:** readback→RAM cache間のQt画像境界を除去するが、GPU→CPUとCPU cacheのコピーは残る。通常prefixは提示47／drop17で不合格。最新版native試行はexception0だが提示47／drop1で不合格。色alpha parityは未検証。
- **次に確認:** 通常prefix試行、native exception、payload色／alphaの旧経路との一致、GPU resident cacheのidentityとbyte budget。


## 2026-10-13 — RAM replay の GPU residency と cache budget

- 確認済み事実: PrimitiveRenderer2D の video upload は owner と寸法ごとの単一 texture を上書きする。既存 sprite/mask texture LRU は 50 entry / 合計512 MiB。controller の RAM replay は CPU cache があってもこの upload を使っていた。
- 関連: Artifact/src/Render/PrimitiveRenderer2D.cppm、Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 今回の範囲: cold RAM publication 後の32 owner warmupと、内容一致のみを返す resident lookup。cache eviction 時は既存経路。総 budget は増やさない。
- 未検証の仮説: 大きい preview と90 frame以上のループは budget不足により residency改善が限定的になる。GPU-only capture と memory policy は別途寿命・編集無効化を確認する必要がある。
- 次に確認: 実際の resident hit / upload 回避、例外、同条件の late/drop、RGBA parity。短いloopの成功を長尺生成の成功と扱わない。

### 2026-10-14: Principled material controls and maps (continuation record)

- **関連:** `ArtifactCore::Material`、`Artifact3DModelLayer`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** 既存作業でAO／base color／metallic-roughness／normal／emission map、ray-cone mip選択、Raster multi-material slot map、Masked shadow alpha、異方性、Burley diffuse、coat／sheen層を順次接続している。
- **変更:** ここまでの作業記録が失われたため、セッション要約から確認できる直近機能を集約して復旧した。独立Coat Normal、Diffuse Roughness、float Base Color transfer encoding、alpha-aware odd-size mip filterを含む。
- **価値／懸念:** 実装経緯の要点を残す。詳細な過去の検証条件は元記録から完全復元できず、GPU compile／runtime parityも未確認。
- **次に確認:** 主要map・material controlsをRaster／RTでcompileし、texture LOD・alpha mode・normal variance・energy consistencyを代表fixtureで評価する。

### 2026-10-14: ufbxのPrincipled拡張scalar import

- **関連:** `ArtifactCore::MeshImporter::detectTexturesFromUfbx()` と `Artifact3DModelLayer::setModelSource()`。
- **確認済み事実:** ufbxの `ufbx_material_pbr_maps` は coat factor／roughness、transmission factor、specular IOR、sheen factor／roughnessを保持する。既存importはmetallic／roughness scalarと基本texture mapのみLayerへ渡していた。
- **変更:** ufbxの最初に明示されたscalar値をImporter getterへ保持し、single-material 3D layer importで対応値が既定値のままならMaterialへ反映する。
- **価値／懸念:** extension scalarを手作業で再設定せず、既存Principled shadingへ取り込める。multi-material meshへのper-slot配布、sheen color、extension texture mapは未対応で、glTF fixture/runtimeは未確認。
- **次に確認:** `KHR_materials_clearcoat`／`transmission`／`ior`／`sheen` fixtureで値の取得、Layer Materialへの適用、再保存後の値を確認する。必要ならper-slot UFBX material mappingを別段階で設計する。

### 2026-10-14: UFBX / glTF multi-material partsをmesh slotへ接続

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`、`Artifact/src/Layer/Artifact3DModelLayer.cppm`、既存Raster／RT multi-material paths。
- **確認済み事実:** ufbxは`material_part_usage_order`と各partの`face_indices`、`num_triangles`を提供し、face単位のmaterial割当てを構築している。`Mesh::generateRenderData()`はpolygonを挿入順にfan triangulationする。
- **変更:** 全UFBX materialのPrincipled factors、tints、texture pathsを取得し、material別face順とindex rangesを生成。Layer slot Materialsを既存Raster range／RT triangle slotへ渡す。texture-only base color mapでは無指定color factorを誤適用しない。
- **価値／懸念:** glTF/FBXで複数素材の色と質感が一つのflatten mesh内でも保持される。実build／fixtureでufbx material table index整合、UV、alpha blending、Raster/RT描画は未確認。node instance overrideやalpha mask semanticsも未対応。
- **次に確認:** multi-part glTFとFBX fixtureでslot range、base color／extension factor／texture、RT primitive mappingを照合し、build後にRaster／RT画像を確認する。

### 2026-10-14: PBR base-color mipの透明縁と奇数寸法を保持

- **関連:** Raster `buildRgba8MipChain` とRT `loadRgbaPbrTexture` のtexture mip構築。
- **確認済み事実:** 固定2x2または整数binの縮小では奇数幅／高さの端texelが落ちるか重みが偏る。透明base-color texelのRGBをalphaに関係なく平均すると、縮小時に色bleedし得る。
- **変更:** source／destination texel矩形の重なり面積で平均し、base-colorはalpha重み付きlinear RGB、alphaは面積平均にする。normal vectorの平均長alphaも維持する。
- **価値／懸念:** 透明縁と不均等縮小の問題を抑える。alphaをopacityに使わない素材では色平均へ影響する可能性があり、実画素未確認。
- **次に確認:** 奇数寸法と透明色edgeのmip readbackをRaster／RTで比較する。

### 2026-10-14: Principled sheen IBLへAOを適用

- **関連:** Raster `MeshRenderer.cppm` とRT `RayTracingManager.cppm` のsheen environment。
- **確認済み事実:** AO mapはdiffuse IBLとbase specularの遮蔽に使う一方、sheen environmentには使われていなかった。
- **変更:** sheen roughnessとview cosineに既存roughness-aware specular-occlusion近似を適用する。AO=1なら従来応答を保つ。
- **価値／懸念:** 凹部でsheenだけが残る不整合を抑える。Charlie sheenにGGX由来scalar近似を使うため、物理積分ではなく画素検証も未実施。
- **次に確認:** AO／roughness sweepとRaster／RT parityを確認する。

### 2026-10-14: Normal Mapへ独立した追加UV回転を追加

- **関連:** `ArtifactCore::Material`、`Artifact3DModelLayer`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** Materialの既存offset／scale／rotationは全texture共通だった。normalとcoat-normalは別texture pathを持つが、どちらも同じ共通UV座標でsampleされ、Rasterのderivative tangent basisとRT triangle tangent basisもそのUVに一致していた。
- **変更:** 共通UV transform後にNormal Map専用の追加rotationをMaterial／JSON／Property／material signature／single・multi-material Raster／RTへ追加した。normal sample座標を回し、RTはtangent／bitangent basisにも同角度を適用する。Coat Normalは同じ追加回転を使う。
- **価値／懸念:** albedo／roughness textureの位置・方向を変えずnormal detailだけを向け直せる。後続変更でcoat-normal専用rotation／scaleとmap別UV set選択も追加した。shader compileと画像parityは未確認。
- **次に確認:** 0度で旧応答を保つこと、90度で+U normal detailが+Vへ回ること、mirrored UV／non-uniform scale、normal／coat-normal併用をRaster／RTで比較する。

### 2026-10-14: Normal Map専用UV scaleとray-cone mip連動

- **関連:** `ArtifactCore::Material`、`Artifact3DModelLayer`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** Normal Mapには共通UV変換後の独立rotationを追加済みだが、texture密度はbase color等の共通scaleを共有していた。Raster LODはnormal UVのimplicit derivative、RT LODはtriangle UV edgeとray coneから計算する。
- **変更:** 0.01〜10のuniform Normal UV ScaleをMaterial／Property／JSON／signature／single・multi Raster／RTへ接続した。normalとcoat-normalのUVを中心基準で拡縮し、RT ray-cone LOD計算にもscaleを反映する。
- **価値／懸念:** base colorやmetallic-roughnessのtile密度を保ちながらnormal detailの大きさを調整できる。後続変更でcoat-normal専用scale／rotationとUV set選択を追加した。shader compileとmip image parityは未確認。
- **次に確認:** scale 0.5／1／2で模様密度が対応して変化し、RT LODもtexture footprintに応じて上がること、rotationとの合成とRaster／RT方向一致を確認する。

### 2026-10-14: Coat Normal専用UV rotation／scale

- **関連:** `ArtifactCore::Material`、`Artifact3DModelLayer`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** 独立Coat Normal textureは別pathでbindされていたが、sampling UV、tangent basis、mip scaleはbase Normal Mapと同じmappingを共有していた。
- **変更:** Coat Normal専用のrotationとuniform scaleをMaterial／JSON／Property／material signature／single・multi Raster／RTへ追加。Rasterは専用UV derivativeからnormalを再構成し、RTはcoat tangent frameを回転しray-cone LODへscaleを適用する。default 0度／1倍では従来と同じmapping。
- **価値／懸念:** 塗膜のnormal detailを基材のnormal textureと独立に向き・密度・UV set調整できる。shader compilationとRaster／RT画像parityは未確認。
- **次に確認:** base／coat normalの角度・密度を別々に変更し、coat highlightとray-cone mip応答がRaster／RTで一致することを確認する。

### 2026-10-14: Coat Normal専用UV rotation／scale

- **関連:** `ArtifactCore::Material`、`Artifact3DModelLayer`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** Coat Normalは別texture pathでbindされていたが、sample UV、tangent basis、ray-cone LODはNormal Mapの共通mappingを共有していた。
- **変更:** Coat Normal専用rotationとuniform scaleをMaterial／JSON／Property／signature／single・multi Raster／RTへ接続。Rasterはcoat UV derivativeからbasisを再構成し、RTはcoat tangent basisとray-cone LODへ専用変換を適用する。
- **価値／懸念:** 塗膜のnormal detailを基材と独立して向き・密度調整できる。offsetとUV setの個別選択は未対応。shader compileとRaster／RT parityは未確認。
- **次に確認:** base／coat mapへ異なる方向・密度のfixtureを使い、coat highlightの変化とRaster／RT一致を比較する。

### 2026-10-14: Raster異方性接線へNormal Map UV rotationを反映

- **関連:** `ArtifactCore/src/Graphics/MeshRenderer.cppm` のRaster mesh PBR shader、RT `RayTracingManager.cppm` のtangent frame。
- **確認済み事実:** RTはNormal Map専用rotationをnormal tangent／bitangentへ反映するが、Raster異方性軸の微分は通常 `meshUv` から求めていた。Raster normal map自体は専用 `normalMapUv` を使っていた。
- **変更:** Raster異方性接線のUV微分を `normalMapUv` から求め、Normal Map専用rotation／scaleを接線方向にも反映した。
- **価値／懸念:** normal map rotationと異方性の組合せでRaster／RTのハイライト方向が食い違う可能性を減らす。画像によるparityは未確認。
- **次に確認:** mirrored UVを含むメッシュでNormal Map rotationとAnisotropy Rotationを個別・複合変更し、Raster／RT方向を比較する。

### 2026-10-14: Raster normal mappingの接線基底をRTと直交化

- **関連:** Raster `computeNormalMapped()` とRT `RTWarmup_ClosestHit` の微分／triangle接線基底。
- **確認済み事実:** Rasterは画面微分からTとBを独立に正規化し、補間法線への投影や直交化を行っていなかった。RTはTを補間法線へ投影し、UV handednessを保ってBをcross積から再構成していた。
- **変更:** RasterのNormal／Coat Normal用基底をRTと同じ投影・符号付き再構成にそろえ、退化Tでは元の法線へfallbackする。
- **価値／懸念:** 滑らかな法線でnormal detailが法線方向へ混ざる誤差と、Raster／RT間の異なる基底を抑える。画像parityは未確認。
- **次に確認:** 球面のsmooth normal、mirrored UV、normal strength 0／1、coat normal併用で各経路の法線／ハイライトを比較する。

### 2026-10-14: Clearcoatをsheen・基材反射・透過の上層として適用

- **関連:** Raster `MeshRenderer.cppm` とRT `RayTracingManager.cppm` のdirect／IBL material layering。
- **確認済み事実:** マイルストーン本文はcoatがsheenの上層と記録している一方、direct／IBL sheenと透過環境光はcoat energyで減衰していなかった。base diffuse／specularとEmissionは既にcoatによる減衰を受けていた。またRaster／RTのback-light transmissionは、`NdotL <= 0`でループを抜ける前にしか評価されず到達不能だった。
- **変更:** direct／IBL sheen、IBL transmissionへclearcoat energyを適用し、Raster／RTのback-light transmissionを裏面光分岐へ移してview-side coat energyを適用した。RTはPrincipled IOR／thin-film Fresnel、view-ray TIR、および裏面向きshadow-ray originを使う。status記述も本文と一致させた。
- **価値／懸念:** clearcoat有効時にsheenや環境透過が最上層coatを突き抜ける見え方を抑える。Raster／RTの背面直接光transmissionが意図した条件で寄与する。RTのsoft／area shadowとsurface parityは未確認で、視覚検証とshader compilationも未実施。
- **次に確認:** coat weight／IOR／tintを変え、sheen・透過の減衰とRaster／RT一致を確認する。

### 2026-10-14: ufbx UV1をMesh RenderDataまで保持

- **関連:** `MeshImporter.cppm`、`Mesh::RenderData`、`Mesh::generateRenderData()`、`generateMeshletLODData()`。
- **確認済み事実:** ufbx meshは`uv_sets`配列で複数の`vertex_uv`属性を持つが、importerは`vertex_uv`（UV0）だけを読み、RenderDataとmeshlet remapにもUV1の格納先がなかった。
- **変更:** 2番目のufbx UV setを`uv1` vertex attributeへ読み、RenderDataの`uvs1`にtriangle flattenする。mesh内の一部だけUV1がない場合はUV0を複製する。meshlet remapにもUV1を含めた。Base Color／Metallic-Roughness／Normal／Coat Normal／Occlusion／Emission／OpacityごとにUV Set（0／1）をMaterial／Layer JSON／Propertyへ追加し、Raster／RT samplingへ伝搬した。Masked shadow alpha clipとRT Any HitにもBase Color／Opacityの選択UVを通す。RTは各選択UVの微分でray-cone mip密度を計算し、Normal／Coat Normalは別々の接線基底を使う。
- **価値／懸念:** すべての現在のPBR texture mapでUV1をRaster／RT共通に選択できる。shader compilation／画像 parityは未確認。
- **次に確認:** 複数UVを含むFBX／glTF fixtureをRaster／RTで描画し、各map選択、masked shadow境界、normal detail、mip応答、material serialization、parityを確認する。
