# 独立機能テスト拡充マイルストーン

**最終更新:** 2026-10-25
**ステータス:** In Progress

## 目的

優先度高として挙げた機能テスト候補を、既存suiteとの重複を避けながら順番に監査・拡充する。製品アプリの起動、ユーザーデータ変更、GPU依存を避けられるものはCoreの独立契約テストに置く。サービス統合、レンダー、GUIが不可避な項目は別suiteへ分け、対象範囲を明示する。

各項目は「既存テストで十分」「既存suiteへケース追加」「独立suite新設」「仕様確認待ち」のいずれかを調査して決める。テストが追加されたことと、ビルド・実行で合格したことは区別する。

## 実装順・追跡

| # | テスト領域 | 主な独立境界 | 状況 |
|---:|---|---|---|
| 1 | キーフレーム補間 | Core `KeyframeSplineTest` | 負時刻・Constant終端・負の線形速度とトラック外速度0のケースを追加。`KeyframeStandalone` projectにも`KeyframeSplineTest.cpp`を登録し、最小`Math.Interpolate`依存でbuild可能にした。未configure・未実行 |
| 2 | フレームレート変換 | Core `FrameRateContractTest` / `FrameTimeTest` / `RationalTimeContractTest` | exact/nominal rate・正負half-frame丸め等の既存coverageを確認。FrameRate独立suite 34/34 pass、RationalTime独立suite 26件中24 pass・2 fail（int64境界のrescaleで符号反転）。`FrameTimeTest`はArtifactCore全体依存のため未実行。 |
| 3 | PropertyPath | Core `PropertyPathTest` | 空・区切り文字のみ、owner/property片側空ケースを含む7テストを独立targetで実行しpass。 |
| 4 | ID・参照整合性 | Core ID契約、Composition Node Store | CompositionNodeStore独立suiteの7ケース（duplicate/unknown parent/self・ancestor cycle atomicity/removal detach/reparent/order+JSON roundtrip/order update/invalid JSON import）を実行しpass。 |
| 5 | 選択状態遷移 | Selection model/service | PrecomposeViewerの別process `--check` harnessにselection保持・切替・Undo/Redoケースあり。ArtifactAppRuntime依存でmodule scanが広域化し、2回目は777/6806段階で中断。GUI runtime未確認。 |
| 6 | レイヤー順序操作 | 順序モデル/service | #4と同じ独立CompositionNodeStore suiteでsetOrder・親子順序・不正値拒否ケースを実行しpass。 |
| 7 | レイヤー複製契約 | Layer model/service | Project RoundTrip別process suiteでID独立・opacity複製・Composition登録・無効ID拒否を確認するケースあり。QTemporaryDir、ProjectCleanup、RUN_SERIALで隔離。AppRuntime target依存のため未ビルド・未実行 |
| 8 | Undo/Redoトランザクション | Undo command/model | Manager contractに通常遷移・Redo分岐・失敗補償・single-entry budget拒否・entry countおよび総memory budget上限時の最古Undo退避と新entryのundo/redo・macro部分巻戻し（redo/undo双方）を追加。AppRuntime依存、未ビルド・未実行 |
| 9 | 保存データ移行 | Serialization/schema migration | 独立SchemaMigration suiteへcallback例外時の入力原子性testを追加し実行。既存4件pass、新規`FailedCallbackLeavesInputUnchanged` fail: 後段callback例外後もJSONが途中migration済みの状態へ変更される。期待値を維持、ArtifactCore childは未変更。 |
| 10 | 部分失敗時の原子性 | Service/API contract | PrecomposeViewerの独立`--check`回帰経路にinvalid request・empty child・cycle拒否などがあり、失敗時の構造・選択・undo/redo履歴を比較。Project RoundTripの不正Project置換load失敗ケースへsingleton UndoManagerのundo/redo両枝保持と履歴の適用順確認を追加。suite結線は静的確認済み。未ビルド・未実行 |
| 11 | Effect Stack編集往復 | Effect sequence | 既存CPU contractへstage sorter経由で同stage安定順が非可換Effectのpixel出力へ反映されるケースを追加。stage順・同stage安定順・mix境界・複数pixel・大surface等を含むCPU suiteと、Project RoundTripの削除/再挿入後の型順・設定復元を確認。両suiteとも実行未確認。`ArtifactEffectSequenceContractTest`はアプリプロセスを起動しないが、`ArtifactEffectsColor`がArtifactCore/Render/EffectContractを推移依存に持ち、独立module suiteには未分離。Undo連携は別項目。 |
| 12 | Mask/Effect順序 | CPU小画像統合 | `RasterizerMaskEffectIntegrationTest`の固定32×16 fixtureへmask-only比較を追加。mask外の同一pixelがmask-onlyではalpha 0、mask後のBlur適用時にalpha拡張することと内部/境界/遠方外部の勾配を確認。専用processだが`ArtifactAppRuntime`へリンクし、実行未確認。 |
| 13 | Track Matte参照 | Matte model/render | Project RoundTrip caseを2 source参照fixtureへ拡張。1 source削除でdangling参照のみ除去し、別sourceへの参照ID/opacity/invertを保持、さらにProject保存・再読込後もtarget/source/参照設定を維持することを確認。`QTemporaryDir`で隔離するがsuiteはsingleton manager/AppRuntime依存。未実行。 |
| 14 | 静止画レイヤー置換 | Layer/asset integration | `LoadingAnotherStillImageReplacesPixelsAndKeepsLayerEdits`を拡張し、寸法が異なるpathと同じ寸法の別pathの双方で画素更新・sourcePath更新、opacity/position保持を確認。`QTemporaryDir`で隔離、専用別processだがAppRuntime依存。実行未確認。 |
| 15 | 連番画像欠番・再接続 | Asset sequence/import | 欠番caseを連続2frame欠番へ拡張。先行欠番を修復して有効pixelをcacheした直後、次の欠番frameで修復pixelがstale表示されずplaceholderへ戻ることを確認。既存の壊れたpathでのstale拒否と有効内容再読込caseもあり。suiteはAppRuntime依存、専用process/RUN_SERIAL。実行未確認。 |
| 16 | 再読込後の参照修復 | Project/asset integration | `StillImageLayerRoundTripTest.MissingSourceCanBeRelinkedAfterProjectReload`で欠損path/placeholderのProject往復保持と、同path復旧後の画素再読込を確認。欠損化前のopacity/positionも再読込後および再接続後に保持するassertを追加。fixture隔離済み、AppRuntime依存で実行未確認。 |
| 17 | Undo/Redo後の保存 | Project/Undo integration | `ProjectSavePersistsUndoAndRedoResults`がUndo後・Redo後を別artifact fileへ保存し、再読込後のopacityを検査。Undo履歴の永続化ではなく結果状態契約。Project RoundTrip/AppRuntime依存、実行未確認。 |
| 18 | 編集・保存・再読込 | Project round-trip | `ArtifactProjectRoundTripTest.ResaveAfterLoadIsStable`で同path再保存、reload後のcomposition編集、別名保存・再load後のname/noteを確認。`CreateSaveLoadPreservesProject`は複数Composition/Layer/notes/extension dataを往復。専用suite/AppRuntime依存、実行未確認。 |
| 19 | 複製・親子変更・削除 | Layer/project integration | Node Store独立suiteに中間node削除時の直接子root化・下位subtree保持を追加。有効なreparent/root detach/childrenOf反映も同suiteにあり、standalone suite 8/8 pass。Project側複製caseに、複製opacityの独立編集が元レイヤーへ波及しないassertを追加。Project suiteはAppRuntime依存、未実行。 |
| 20 | プリコンポーズ・子編集・解除 | Project service integration | PrecomposeViewer独立回帰ケースで子編集値の解除・Undo/Redo保持、nested unwrap、保存再読込後の構成とManager階層復元、拒否時原子性を確認。Windows専用viewerの`--check`経路（CTest登録ではない）を静的確認。suite改変なし、未ビルド・未実行 |
| 21 | Playback範囲・マーカー | Playback model/service | 独立CPU `ArtifactInOutPointsContractTest` に未設定・両端設定・片側クリア・逆順・Composition外点、マーカー/章の厳密な前後移動と順序、同一位置更新、型/タグ/テキスト検索、範囲端点を含む列挙、所有権安全な削除と章索引更新、JSON/サポート対象XML属性の往復、invalid JSON拒否時の状態保持、marker配列内の不正項目スキップと未知typeの既定値を追加。malformed XML拒否時に既存range/markerを維持する回帰期待も追加したが、現行Artifact importerは解析前に既存状態を消去するため不一致がソース上明らか。未ビルド・未実行でruntime状態は未確認。別processの`ArtifactPlaybackRangeServiceContractTest`でAll/WorkAreaのEngine range適用とIn/Out clippingを追加。サービスはQThread/AudioRendererを構築するが再生は開始せず、Core event loopの専用targetに隔離 |
| 22 | Transport状態機械 | Playback Service | 独立Playback Service processでPlay→Pause→Resume→Stopに加え、再生開始時のframe進行・Pause中のframe停止・Resume後の進行・Stop完了後のframe停止を確認。RAM/disk preview無効化、Composition解除とstop/wait cleanup。AppRuntime依存、未ビルド・未実行 |
| 23 | Render Cache key | Render controller/model | Core `RenderPipelineContractTest`で各identity field変更時のkey inequalityに加え、各fieldのstable hash変化も検査。`cacheKey()`のfeature flag転送とhash混入は静的に整合。targetはArtifactCore依存、runtime未確認 |
| 24 | Cache invalidation範囲 | Layer Surface Cache | Windows GUI harnessで未変更rerender pixel一致、対象Effect編集後のtarget invalidation/rebuildとpresented pixel変化、無関係Layer編集の画面反映とtarget cache build/invalidation非干渉を検査。ArtifactAppRuntime/ArtifactCore依存、ビルド/runtime未確認 |
| 25 | Dirty範囲計算 | Bounds/renderer contract | 既存Standalone suiteに退化点が自身以外を拒否するassertと、退化線分が端点外・軸外を拒否するケースを追加。現`contains()`の外積符号判定では全外積0となる範囲外入力を含むため、追加assertは未実行で失敗見込み。ArtifactCore childは未変更。 |
| 26 | 画像境界・形式変換 | Core image contract | `ImageSurfaceViewTest`でpointer/dimension/stride/alignment/overflowとpadded row、`SurfacePixelConversionTest`でpremultiplied alpha・byte/float・量子化境界、`ImageIoContractTest`でfile decode境界を既存網羅。`ImageIoContractTest`は既存binaryで9/9 pass。`RawImageContractTest.RejectsPixelBufferShorterThanDescribedImage`もあり、`RawImage::isValid()`はdata非空しか確認しないため必要byte数との不一致を検出する期待。x64 build treeのRawImage targetはArtifactCore全体を再buildし、animation module compile errorsで対象test前に停止。 |
| 27 | GPU resource世代管理 | GPU resource contract | Core `RenderPipelineContractTest`にGI resource全6種のdescriptor kind/寸法/channel数のtable-driven検査を追加。既存のresize世代・History descriptor・temporal history連続/frame欠落/camera cut検査、独立CPU `ArtifactFrameCacheContractTest`のgeneration bump後の旧entry非可視化とstale cleanupも確認。実GPU device世代復旧は未カバー。個別target buildを試行したが、ArtifactCore dependency再ビルド中に無関係な`MFEncoder.cppm`と`AnimatableTransform3D.cppm`のmodule compile errorsで185/603段階停止。テスト未起動。 |
| 28 | Hot path容量上限 | 診断・負荷suite | 独立CPU `ArtifactFrameCacheContractTest` にframe count 0時のstore拒否・状態不変、明示的`trimToSize()`のFIFO eviction到達値を追加。既存suiteのフレーム数/メモリ上限・LRU eviction・予算超過entry拒否・Composition/frame/layer/effect指定invalidation、Image sequence cache上限、Playback stress sample上限・RAM preview 128件上限も確認。未ビルド・未実行 |
| 29 | 再生中Composition切替 | Playback integration | 既存Playback stress harnessの単発切替でactive index変更・Service current Composition一致・切替前再生中なら再開・Transport widget一致を明示判定。2つ以上のCompositionで実行が必要。stress未実行 |
| 30 | Playback ServiceとUI同期 | Playback/UI integration | 独立Playback stress harnessに実際の`ArtifactPlaybackControlWidget`を配置し、ServiceとUIのPlaying/Paused/Stopped query一致をstate出力、Composition切替、resize、連続巡回、run完了時に検査。GUI runtime未実行 |
| 31 | Cache有効/無効画像比較 | Renderer integration | 別processのheadless GPU contract targetで、公開`drawLayerForCompositionView`へ明示cache map／`nullptr`を渡し、Rasterizer effectつきImageLayerのcache build・entry identity維持によるreuse・bypass画像をRGBA byte完全一致比較。実関数がArtifact側のため`ArtifactAppRuntime`へリンクするが、Artifact.exeは起動しない。別系統static GPU cacheはLayerCachePolicyで無効化。controller全Composition pipelineとの比較ではない。未ビルド・未実行 |
| 32 | アニメーションTransform GPU描画 | Headless GPU integration | RenderQueue GPU integrationで一般Solid layerのPosition X keyframeをGPU指定で2 frame出力し、赤画素の重心がframe間で80px超移動することを比較する独立CTestケースを追加。GPU依存。未ビルド・未実行 |
| 33 | 大画像＋Effect | Render stress/integration | 独立headless GPU contract targetで1280×720 ImageLayer＋Rasterizer Glowをcache/bypass描画し、両画像の完全一致とeffectなしbaselineとの差を確認。CPU Effect contract suiteには大surface Brightness処理・端/中央画素・入力保持もあり。RenderQueue負荷比較は未カバー。GPU依存・未ビルド・未実行 |
| 34 | 非表示レイヤー負荷 | Render stress | Playback stress harnessに`stress.compareVisibilityProfiles`を追加。同じComposition・再生run内で全表示と奇数index半数非表示を同じ時間計測し、可視/非表示数、100ms samplingのframe進行率、stall sample、提示frame delta、drop deltaを比較可能。元の可視状態は計測後に復元する。runtime比較は未実施 |
| 35 | リサイズ中の描画安定性 | Viewer/GPU GUI | 独立Playback stress harnessの再生中resize検査に、Viewportが正寸法で要求window寸法内に収まるassertを追加。Qt event processing後のframe進行・再生継続・Transport状態一致も判定。GPU/GUI未実行 |
| 36 | 複数Composition連続再生 | Playback stress | 独立Playback stress harnessに`stress.cycleCompositions`を追加。再生中に全Compositionを順番に切り替え、各切替後のComposition一致・再生継続・フレーム進行を判定。巡回後に開始Compositionへ戻ったことも結果出力と合否に含めた。終了時stop/wait/参照解放処理も既存実装で確認。GPU/GUI実行は未確認 |
| 37 | GPU device喪失後の復旧 | GPU integration | 独立GPU processに`DiligentDeviceManagerRecoveryContractTest`を追加。headless device作成後に公開`markDeviceLost()`→destroy→再初期化し、新device/contextを確認する。backend device removalやPresent失敗を注入しないため、ArtifactIRendererの自動recovery経路・実GPU喪失は別途未カバー。GPU依存・未ビルド/未実行 |
| 38 | Export中のUI応答 | Export/UI integration | 既存EXR RenderQueue統合ケースを `[0,48)` の48-frame出力に拡張。描画中のGUI timer event処理に加え、全48ファイルをOpenEXRで読み込み、寸法・RGBA要素数・有限値・可視画素を検査する。GPU依存。未ビルド・未実行 |

### 2026-10-16 追加確認

- `CompositionNodeStore::fromJson()` は配列内の各要素を順次 `addNode()` に通す。重複IDは最初の項目が残り、存在しない親・空ID・オブジェクト以外は捨てられる。独立CPU契約テストでこの復元挙動を確認するケースを追加。
- `ArtifactInOutPoints::effectiveRange()` はIn/Outが逆順なら端点を並べ替えるが、Composition境界外の明示点は clamp しない。この契約の追加ケースを登録。
- `ArtifactInOutPoints` のマーカー位置検索・章ナビゲーションがstrict boundaryを使い、昇順で列挙すること、および型不正JSONを拒否した際に既存範囲とマーカーを保持する契約を追加。
- マーカーの同一位置再追加は既存objectを更新する契約、comment/link/tag横断のcase-insensitive検索と、全メタデータ・In/Out点のJSON往復を追加。
- マーカー範囲抽出は両端を含むこと、別ownerの同位置marker pointerを削除要求に渡しても既存markerを保持すること、chapter削除後にchapter indexが更新されることを追加。
- JSON内の非object/position欠落markerをスキップし、未知typeをCommentとして復元し、その既定色と文字列tagの正規化を確認するケースを追加。
- In/Out pointsのXML export/importで、frame range、chapter/flash type、XML escapingされたcomment、tagsが往復する契約を追加。XML形式で保存しないmarker色/WebLinkは比較対象外。
- #2既存suiteを再監査。FrameTimeのFramePosition/TimeCode変換と正負境界、RationalTimeのoverflow/rounding/reference grid、FrameRate storage scaleの境界ケースが既にあり、追加候補の重複を避けた。runtimeでFrameRate standalone 34/34 pass。RationalTime standaloneは26件中24件passで、`RoundingBiasDoesNotOverflowAtInt64NumeratorLimit` と `RescalingMinimumNumeratorDoesNotNegateOverflow` が失敗し、int64境界での符号反転を再現。out-of-rangeの`double`最大値は公開契約が確認できず飽和期待を削除し、int64範囲内最大の丸め可能入力へ変更。`FrameTimeTest`はArtifactCore全体をlinkするため、EnvironmentVariable module IFCのビルド失敗により未実行。
- #2のMSVC standalone targetは、別targetへ置いたscanner-disabled module implementationの明示`/reference`とQt runtime DLL PATHが不足していたため、test-only CMakeに補完。FrameRate test sourceにも直接QString includeとMSVCで不正なinitializer-list型推論を修正。製品ターゲット・子repoは変更していない。
- #3 `PropertyPathTest` は単独module interface targetで7ケースすべてpass。`QString(), QString()`のdirect-initializationがmost-vexing-parseとなり `.isValid()` を呼べないため、test fixtureをbrace initializationへ変更。
- #4/#6 CompositionNodeStore suiteはArtifact child module interfaceと実装だけを使うstandalone CPU processで7ケースすべてpass。通常のArtifact CMake suiteはArtifactAppRuntimeへリンクしているが、この契約範囲では製品アプリやAppRuntimeを起動・リンクせず検査できることを確認。
- #5の`ArtifactPrecomposeViewer` target buildは`ArtifactAppRuntime`を経由してArtifactCore全moduleのdependency scanを開始し、実コンパイル前に最初の試行は526/7326、二度目は777/6806 scanで中断。再確認時点でも`ArtifactCore/CMakeLists.txt`の`EnvironmentVariable.cppm`はinterface objectへの`OBJECT_DEPENDS`だけで、MSVCに必要なIFC `/reference`を持たない構成だった。過去に同じtarget graphで当該module-not-foundを確認済み。製品app processは起動しておらず、GUIケースはruntime未実行。
- #7 DuplicateLayerケースはProjectManager serviceを通し、別ID・コピーしたopacity/name・composition内の双方存在・nil ID拒否を検査する。`QTemporaryDir`と`ProjectCleanup`でproject状態を隔離し、CTestは`RUN_SERIAL`。
- #9 SchemaMigration header-only moduleを専用standalone projectから検査し、chain migration・unknown field保持・invalid registration・path選択/不変性の4 testを実行してpass。callback例外時の部分更新は仕様が未確定のためassertしていない。
- #10 Project RoundTripのfailed replacement loadケースは、既存project pointer/name/composition/layer保持を検査。PrecomposeViewer `test.invalidRequestsAtomic` はinvalid/stale/empty selection、invalid unprecompose対象、selection保持、undo redo履歴保持と既存redo実行まで比較する。両方のruntimeはAppRuntime依存。
- #11 `EffectSequenceContractTest.cpp`の6ケースはstage sort/stable order、順序変更後のCPU pixels、mix境界値を含む。Project RoundTripは削除・再挿入後の順序と設定値を保存再読込する。標準targetは`ArtifactEffectsColor`にリンクし、そのtargetはArtifactCore/Audio/ColorCollection/Render/EffectContractを公開依存として持つ。テスト実行ファイルは現生成buildに存在せず、広い推移依存を避ける独立suiteは未整備。
- #12 該当ケースは`RasterizerMaskEffectIntegrationTest.RasterizerBlurRunsAfterLayerMask`。32×16 sourceを使いalpha勾配とpositive boundaryを確認する。CTestは別process・RUN_SERIALだがtarget link先は`ArtifactAppRuntime`で、独立module suiteではない。実行fileは現生成buildにない。
- #13 `ArtifactProjectRoundTripTest.RemovingMatteSourceClearsOnlyDanglingReference`はtemporary project rootとcleanupを使い、source削除後にtarget/Compositionを維持してdangling matteだけを除去する。suiteはAppRuntime依存でRUN_SERIAL。実行fileなし。
- #14 `StillImageLayerRoundTripTest.LoadingAnotherStillImageReplacesPixelsAndKeepsLayerEdits`はtemporary PNGを使い、2x1から3x2へのpath/pixel置換とopacity/position保持を確認する。専用別process/RUN_SERIALだがAppRuntime依存。実行fileなし。
- #15 同じStillImage suiteにmissing frameからplaceholderへの切替とファイル復旧、破損pathでstale frameを出さないケース、同一pathの有効内容差替え後に新画素を再読込するケースがある。各fixtureは`QTemporaryDir`で隔離。suiteはAppRuntime依存で実行fileなし。
- #16 `StillImageLayerRoundTripTest.MissingSourceCanBeRelinkedAfterProjectReload`はmissing pathを持つimage layerを保存・再読込し、pathとplaceholderを確認後、同じpathへPNGを書いて再接続・画素を確認する。temporary project root、AppRuntime依存の専用suite。
- #17 `ArtifactProjectRoundTripTest.ProjectSavePersistsUndoAndRedoResults`はUndo後とRedo後のopacity値を個別保存し、各fileをloadして値を再検査する。Undo履歴そのものはserializable対象にせず、suiteはRUN_SERIAL/AppRuntime依存。実行未確認。
- #18 `ArtifactProjectRoundTripTest.ResaveAfterLoadIsStable`はsame-path backup branchを通した後にreloadし、composition name/noteを編集して別pathへ保存・再loadする。`CreateSaveLoadPreservesProject`はproject treeとnotes/extensionsを含む。AppRuntime依存の専用suiteで実行未確認。
- #8 Undo contractに`maxSingleEntryBytes=0`での実行前拒否と`maxEntryCount=1`の古い履歴退避を追加。既存の失敗テストが`maxEntryCount=0`を指定していたため、setBudgetがentry countを最低1へclampする実装と矛盾していた。拒否条件を単一entry byte budgetへ合わせた。
- #19 CompositionNodeStoreで4階層fixtureの中間nodeを削除し、削除node不在・直下childのroot化・それより下の親子関係維持を独立suiteへ追加。root一覧は同orderの順序契約に依存させず件数で検査する。VS 18/Ninja standalone suiteは8/8 pass。既存configureのMSVC `/showIncludes` prefixが英語と不一致で日本語include一覧が出力された。CMake再configureはしていない。
- #22 Playback ServiceのPlay/Pause/Resume/Stop testにframe clockの進行・停止・再開を追加。空Composition、RAM/disk preview無効、bounded waitを使い、Pause観測は100msに限定する。ArtifactAppRuntime依存のためruntime未実行。
- #23 Render cache identityの各field差分helperで、`RenderCacheKey` equalityだけでなく`stableHash()`の変化も確認するよう強化。これでhash functorの入力漏れも同じ変異ケース群で検出する。runtime未確認。
- #24 Surface Cache harnessにEffect編集後と無関係Layer編集後のpresented frame readback比較を追加。前者は未変更frameとpixel差があること、後者も直前frameから変化することを確認しつつ、対象layerのcache invalidation/build counterは変わらないことを検査する。
- #25 `LayerBoundsCalculator2D::contains()`の外積判定では全頂点一致の退化点は任意pointでcross=0となり、ゼロ幅線分上のcollinearな延長点も内側判定になる。standalone testに退化点の近傍拒否と退化線分の端点外/軸外拒否を追加した。実行targetなし、結果未確認。
- #26 画像境界・形式変換は既存suiteの分割が十分で追加重複なし。`RawImageContractTest`には短いdataを拒否するテストが既にあり、`RawImage::isValid()`の現実装（非空だけ確認）と不一致。静的に失敗見込み、実行未確認、ArtifactCore childは変更なし。
- #27 GI frame resource descriptorを全6種で確認するtable-driven testをCore単体suiteへ追加。各resourceのkind・640×360寸法・channel数（Depth=1、他=4）を検査。実装はheader定義の仕様を反映、未実行。
- #28 `FrameCache`へframe count 0時のstore拒否と統計不変、および明示`trimToSize(16)`で3件からFIFO oldest 2件を除去してbyte目標へ到達する契約を追加。未実行。
- #8 Undo contractへ総memory budgetを単一command推定サイズに制限した2-entry push/eviction/undo/redoを追加。古いentry退避後もmemoryPressureが1以下で最新entryが可逆なことを確認する。未実行。
- #9 migrationの2段階経路で後段callbackが例外を投げるケースを追加。例外の伝播と、呼び出し元JSONが開始時のまま保たれる原子性を期待する。現実装はcallbackごとにdataへ逐次反映するので失敗見込み、ArtifactCore childは未変更。
- #9 standalone targetは一時的なMSVC/Windows SDK `INCLUDE` と`LIB`設定で再build・linkできた。後段callback例外の新規caseが入力変更を検出し、4/5 pass。CMake再構成なし。初回compileはNinja `msvc_deps_prefix`と日本語`/showIncludes`出力が不一致となりinclude一覧が漏れたため、そこで作業を止めた。2回目は既存objectからlinkのみ実行。
- #10 不正Project置換load失敗時に既存undo/redo履歴も保持することをProject RoundTrip contractへ追加。int probeに2履歴を積んで一度Undoした状態（両stack非空）を作り、load失敗後にstack countとUndo→Redo順の値遷移を確認する。
- #11 EffectSequence CPU contractに、stable same-stage sort後にBrightness→InvertとInvert→Brightnessの出力画素がそれぞれ期待値になるtestを追加。sort安定性のpointer順だけでなく、その順序が実Effect処理へ伝播することを検査する。
- #12 Rasterizer mask/blur integrationにblur無効時のmask-only再描画を追加し、mask外pixelのalphaがmask-onlyでは0、Blur適用経路では増加することを比較する。
- #13 Track Matte source削除caseに第2のsourceと参照を追加。1 source削除後に生存sourceへの参照が残ることと、target/参照ID/opacity/invertがProject save/load後も保持されることを検査する。
- #14 still-image replacement caseに同寸法・別pathの第3画像を追加。寸法変化のない置換でもpixel/sourcePathが更新され、opacity/positionは維持されることを確認する。
- #15 欠番sequence fixtureへ第2の連続欠番frameを追加。先の欠番復旧後に次frameが依然欠番の場合、復旧pixelがstale表示されずplaceholderになる契約を確認する。
- #27 個別target buildは既存`x64-Debug-Agent`構成で実行したが、603段階のArtifactCore依存build中、`ArtifactCore/src/Codec/MFEncoder.cppm`と`ArtifactCore/src/Animation/AnimatableTransform3D.cppm`のmodule compile errorsにより185/603で停止。対象gtestは起動していない。CMake再構成なし。
- #1に負の線形傾きとトラック両端外で速度が0へクランプされる契約を追加。既存の正の傾き・定数区間ケースと重複しない。
- #23 の前回記録「feature flagをcache keyへ代入していない」は現状態と不一致だったため撤回。現在の`ArtifactCore/include/Graphics/RenderPipelineFoundation.ixx`では3 flagのkeyへの代入、`RenderCacheKey::stableHash()`での混入、既存契約テストの差分assertを静的に確認した。実行未確認。
- Playback stress harness にComposition連続巡回操作を追加。各遷移でPlayback Serviceの対象・再生状態とフレーム進行をまとめて返す。GPU/GUI環境での実行は未実施。
- Renderライブラリ内のCPU `FrameCache` を単独processのcontract targetで検査するケースを追加。フレーム数・バイト数上限、LRU選択、oversized entry rejectionを対象とし、Graphics Deviceは生成しない。
- `FrameCache` の世代bump/古いentry cleanup、およびComposition・frame range・layer ID・effect IDによる対象invalidatonの組み合わせを追加。
- Playback stress harnessに製品の`ArtifactPlaybackControlWidget`を追加し、Playback Serviceの状態とウィジェットの状態queryを比較する。既存UIのテストプロセス内利用のみで、製品のイベント配線は変更しない。
- 独立Playback Service processにTransport Play→Pause→Resume→Stop状態遷移ケースを追加。失敗経路を含めPlayback停止・Composition参照解除を行う。runtime未確認。
- Playback stress harnessに同一Composition・同一再生run中の可視/交互非表示profile比較を追加し、各条件の実測時間、フレーム進行sample率、stall、提示/drop frame差、可視レイヤー数を出力。比較後は開始時の各layer visibilityへ戻す。GUI/GPUでの実測は未実施。
- Composition Layer Surface Cacheの既存Windows GUI harnessに、controllerの`readbackPresentedFrameImage()`を使った未変更rerender前後のComposition frame pixel完全一致判定を追加。window chromeやdesktop captureに依存せず、cache hit診断だけでなく出力不変性も見る。ArtifactCore IFC blockerによりビルド/runtime未実施。
- RAM preview容量テストでは`storeRamPreviewFrameImage()`に1×1 CPU画像を渡し、disk persistenceを無効にして128件上限とLRU evictionを独立process内で検査する。Serviceのprivate上限値はテストに露出させず、公開統計・bitmap・frame stateから振る舞いを確認する。
- #31はArtifact child repoの公開`drawLayerForCompositionView`がsurface-cache mapを引数に取るため、root test targetからmap有無を切り替えるheadless比較を追加した。Rasterizer effect経路を使い、独立static GPU layer cacheはlayer policyで無効化する。controller統合経路全体のcache bypass検証は対象外であり、実行未確認。
- #33に1280×720 ImageLayerのRasterizer Glow headless描画ケースを追加。cache/bypass pixel equalityとeffectなしbaselineとの差を確認する。GPU runtime未実施。
- `ArtifactCompositionViewDrawing`のsurface cache keyはImageLayerのpath/source version/crop等を参照するが、`setFromQImage()`後はsource path/asset IDが空になりsource versionが0になる。同サイズのin-memory画像差し替えをcache entry identityへ反映できるか、静的には保証を確認できなかったため、#31 parity testでは未検証の差し替えassertを除き、build/reuse/bypass比較に限定した。別途製品側cache identityの調査が必要。
- #37のrenderer recoveryはPresent失敗文言のdevice-lost判定に接続される。追加した独立GPU testはmanagerのlost teardown後にheadless再初期化できることだけを確認し、Present由来の自動recoveryやbackendの実device removalは検査しない。
- 失敗Projectのload時、未ロード状態の維持だけでなく既存の有効Project・Composition・Layerを保持することをProject round-trip suiteで確認するケースを追加。Project importerが成功後にだけcurrent projectを差し替える既存実装と対応付けた。
- 独立targetの再実行ではDLL探索pathを`raytracing-acceptance`のvcpkg runtimeとVS CRTへ一時的に通す必要があった。CompositionNodeStore targetは`cmake --build ... --target ArtifactCompositionNodeStoreContractTest`で`ninja: no work to do`を確認後、8/8 pass。FrameRate 34/34、PropertyPath 7/7、ImageIo 9/9 pass。RationalTimeは24/26 passで、int64境界rescaleの2ケースが符号反転でfail。SchemaMigrationは新規testを含む状態で4/5 pass、callback exceptionの原子性期待がfailし途中JSON mutationを確認した。CMake configureは実行していない。`cmake --build ... --target help`でRawImage/RenderPipeline/FrameCache/SurfaceCacheParity/PlaybackRange/StillImage targetsの登録を確認したが、x64 build treeのRawImage target buildは425-step ArtifactCore依存を引き、animation module compile errorsにより対象test前に停止。他の広域target buildは行わない。

## 実装・受入ルール

- 既存suiteのケース追加で責務が保てる場合、新しいtargetや製品側入口を増やさない。
- 新targetは小さな依存境界を保ち、Artifact.exeやユーザー設定に依存させない。GUI/GPUが必要なものだけ明示的に分離する。
- テストfixtureは決定的・一時領域限定とし、テスト後に状態を戻す。
- GPU backendごとの未実行・fallback・capture不能は合格に数えず、結果へ記録する。
- 診断やinstrumentationはopt-inとし、無効時のhot pathに計測負荷を持ち込まない。
- 新規シグナル／スロット、QtCSS、QColorDialog、QImage合成、Qt CompositionModeを導入しない。
- CMake、ビルド、テスト実行はユーザーの明示指示があるまで行わない。実装済み・静的確認済み・runtime合格を別々に記録する。

## 完了条件

追跡中の全項目について、既存カバレッジまたは追加したケースを具体的に確認し、重複のないテスト配置を決める。実装対象はテストコードと必要最小限のtest-only fixtureに限る。ユーザー方針でビルド・実行できない間は、runtime検証を未完了として明示し、完了扱いにしない。

- #470 Tuple<T&>のcopy assignmentが参照先を再bindせず、destinationが元から参照している値へsourceの値を書き込む契約を追加。既存テストは参照要素の構築・アクセスのみを確認。

- #471 ArtifactVariant未設定時のindex()==0は先頭alternativeを意味せず、全holdsAlternative<T>()がfalseとなる契約を追加。既存テストは未設定状態のみ確認。

- #472 ArtifactTimer::restart()が経過時間の起点をリセットする契約を追加。既存testはrestart後の有効性と非負値のみ確認。

- #473 UniquePtr::swap()がempty/non-empty両方向で所有権を移し、値とempty状態を保つ契約を追加。既存swap testは両側が所有中のケースのみ。

- #474 artifactIsFinite(long double)の有限値・最大有限値・正の無限大に対する契約を追加。既存有限性テストはfloat/doubleと整数のみを確認。

- #475 Durationの負のscalar乗算がdurationの符号を反転する契約を追加。既存testは正のscalarのみを確認。

- #476 ArtifactArrayのzero-length sliceと、left/rightへの全範囲以上の要求が空配列または全体コピーを返す境界を追加。既存slice testは一部区間・midの末尾・短いleft/rightのみ。

- #477 ArtifactSpan::first(size)とlast(size)が元のdata pointerおよび全要素範囲を保つ契約を追加。既存境界testはzero countとfull subspanのみ。

- #478 ArtifactArray::squeeze()が空配列の余剰storageを解放してcapacityを0にする契約を追加。現行実装はreserve(0)がcapacity以下で早期returnするため、静的には期待fail見込み。ArtifactCore childは未変更。

- #479 ArtifactHashMapがraw bucket storageの所有権を持つため、copy construction/assignment不可であることをcompile-time contractとして追加。既存suiteはmove ownershipのみを確認。

- #480 ArtifactHashMapのconst begin()がconst value_type referenceを公開するcompile-time contractを追加。現行const_iteratorはmutable iterator aliasのため、静的には期待fail見込み。ArtifactCore childは未変更。

- #481 ArtifactHashMap iteratorのprefix incrementが同じiteratorへのreferenceを返し、次entryへ進む契約を追加。既存iterator testはpost-incrementのみを確認。

- #482 HashSetへの重複追加がfalseを返すだけでなく、既存要素のinsertion-order snapshotを変えない契約を追加。既存testは重複後のsizeのみと、通常挿入順を別々に確認。

- #483 HashSet::swap()がstateful hasherを対応bucket storageと一緒に交換し、swap後lookupを保つ契約を追加。現行swapはfunctorを交換しないため静的には期待fail見込み。ArtifactCore childは未変更。

- #484 Callbackのmove constructionでsourceがemptyになり、そのsourceへ再bindできる契約を追加。既存testは移動先captureの保持とsource.clear()後の独立性のみ確認。

- #485 rtifactSortがcopy不能なunique_ptr要素をcustom comparatorで並べ替え、全所有値を保持する契約を追加。既存sort testはcopy可能なscalar要素のみ。

- #486 HashSet::swap()がstateful KeyEqualの同値性規則をmembershipと一緒に交換する契約を追加。exact/tens equalityを使い、現行swapでは規則が入れ替わらないことを検出する。ArtifactCore childは未変更。

- #487 String::toInt/toDoubleがstrtol/strtodのrange errorを変換失敗として報告する契約を追加。現行実装はerrnoを参照せずdigit prefixだけでokを立てるため、静的には期待fail見込み。ArtifactCore childは未変更。

- #488 ASCII trimが端のUTF-8 non-breaking space bytes (C2 A0) をASCII空白扱いせず保持する契約を追加。既存testはASCII whitespaceとhigh-bit case変換を個別確認。

- #489 String numeric conversionが先頭ASCII空白・明示的plus符号・数値prefix後のsuffixを受理する契約を追加。既存positive parseは負符号または空白なしの値のみ。

- #490 ArtifactExpected::transform callbackのreference returnをdecayして元参照先から独立したowned valueにする契約を追加。既存callbackはvalue returnのみ。

- #491 ArtifactExpected::transformError callbackのreference returnをdecayして元参照先から独立したowned errorにする契約を追加。既存callbackはvalue returnのみ。

- #492 ArtifactOptionalのlvalue value_orがpresent時にはfallback変換を行わず、empty時だけ一度変換する契約を追加。既存testは通常値の選択のみ。

- #493 ArtifactOptional<T&>のmove construction後にsource/destination双方が元参照先をaliasし続ける契約を追加。既存testはmove assignment後の両aliasのみ確認。

- #494 ArtifactVariant::get<T>()がmutable variantからT&、const variantからconst T&を返す型契約を追加。既存suiteはvalueの読み書きとconst getIf pointerのみ確認。

- #495 ArtifactSpanのbegin/endがT*またはconst T*を返すconstness契約を追加。既存static_assertはoperator[]とdata()のみ確認。

- #496 null C-stringから構築したempty StringのtoInt/toDoubleが0とok=falseを返す契約を追加。既存testはnull constructionと非数値入力を別々に確認。

- #497 ArtifactExpected::toOptional()がsuccess payloadの独立copyを返し、元Expectedの後続変更に影響されない契約を追加。既存testはintのvalue存在とerror dropのみ確認。

- #498 ArtifactArray::replace()が有効indexのvalue置換時にdata pointer、element address、capacityを保持する契約を追加。既存testは値とsizeのみ確認。

- #499 ArtifactArray::replace()がreplacement argumentとして同じindexの既存要素を受けても値とsizeを保つself-alias契約を追加。現行実装はdestroy後に参照からcopyするため静的には期待fail見込み。ArtifactCore childは未変更。

- #500 ArtifactArray::insert()がshift対象要素をreplacement argumentとして参照していても正しい値を挿入する契約を追加。capacityを事前確保して再配置を除外。現行実装はshift後に参照を読むため静的には期待fail見込み。ArtifactCore childは未変更。

- #501 ArtifactArray::append()がcapacity growth時にsourceとして同じArray内要素を参照しても値を保つ契約を追加。現行実装はensureCapacity後に参照を読むため静的には期待fail見込み。ArtifactCore childは未変更。

- #502 ArtifactArray::appendAll()がArray自身をsourceとしcapacity growthを伴っても元の要素列を一度だけ追加する契約を追加。現行range-forはgrowthでsource iteratorを無効化するため静的には期待fail見込み。ArtifactCore childは未変更。

- #503 SmallVector::add()がgrowth時に内部要素をsource参照として渡されても値を保持する独立Standalone回帰testを追加。現行実装はgrowth後に参照を読むため静的には期待fail見込み。ArtifactCore childは未変更。

- #504 SmallVector::insert()がshift対象要素をsource参照として渡されても元の値を挿入する独立Standalone回帰testを追加。現行実装はshift後に参照を読むため静的には期待fail見込み。ArtifactCore childは未変更。

- #505 SmallVectorのmove-only unique_ptr要素がinline capacityを超えるgrowth、middle insert、takeAtを通して全所有値を保持する独立Standalone testを追加。既存testsはcopyable value typeのみ。ArtifactCore childは未変更。

- #506 ArtifactMinMaxの整数artifactMin/artifactMax/clamp/artifactLerpがconstexpr contextから利用可能である契約をstatic_assertで追加。既存testはruntime結果のみ確認。

- #507 artifactFindInがconst containerからconst element referenceを指すiteratorを返し、未検出時にend()を返す契約を追加。既存testはmutable containerだけ確認。

- #508 RandomStream::shuffleが同一seedと同一入力に対して同じ並びと最終stateを生成する再現性契約を追加。既存testはshuffle後の要素保持と0/1要素時のstate維持を確認。

- #509 Matrix4x4::operator*=が右辺に自身を渡す自己別名時も正しい積を保持する契約を追加。既存testは独立した右辺matrixだけを確認。

- #510 Size_2Dのwidthが0かつheightが非0のとき、emptyでaspectRatioが0となる契約を追加。既存aspect ratio境界はheight==0のみ確認。

- #511 XYZColor::operator==の差分が許容値1e-6fと等しい場合は不一致になる厳密境界契約を追加。既存testは許容差内外のみ確認。

- #512 SmallVectorのself move assignmentがinline/heap-backed双方で要素列とcapacityを保持する契約を追加。既存suiteは別instance間のmoveのみ確認。

- #513 safeNormalizeRangeが逆順の無限端点を[-infinity,+infinity]へ正規化し、有限範囲へ縮退させない契約を追加。既存testはNaNと有限区間のみ。

- #514 RandomStream::pick(vector/array)が選択要素をconst T&で返し、呼出側から変更できない型契約を追加。既存move-only testは元要素へのaliasを検査するが戻り値のconstnessは明示確認しない。

- #515 ArtifactSpan::front/backがconst span wrapperでも要素型に応じてT&またはconst T&を返す型契約をstatic_assertで追加。既存constness検査はoperator[]/data()/iteratorのみ。

- #516 owning ArtifactOptionalのoperator*がmutable/const lvalueとmutable/const rvalueでT&/const T&/T&&/const T&&を返す型契約を追加。既存static_assertはvalue()のみ確認。

- #517 ArtifactVariant::getIf<T>()がmutable variantからT*、const variantからconst T*を返す型契約を追加。既存runtime testはポインタ値を確認するが戻り値型を明示しない。

- #518 ColorTransferFunctionのHLG encode/decodeを独立参照式で比較し、線形1/12とencoded 0.5の分岐点および両側を検査するケースを追加。既存ColorPipeline suiteはsRGB/Rec.2020/PQを対象としHLGなし。

- #519 ColorTransferFunction::encode/decodeのgeneric switchを16対応modeそれぞれの公開curve functionと照合し、未知enumのidentity fallbackも確認するdispatch contractを追加。直接curveの一部は個別数式test未整備のためdispatch coverageとして記録。

- #520 ColorGamutConversionのACES_AP1↔sRGB D60/D65 adaptationがneutral whiteを維持する契約と、同一gamut matrixがidentityとなる契約を追加。既存独立参照比較はsRGB/Rec.2020間のみ。

- #521 Rec.709 encode/decodeを独立piecewise参照式と比較し、linear 0.018/encoded 0.081の閾値とその前後、負入力clampを確認するtestを追加。既存suiteはgeneric dispatchのみを対象。

- #522 Quaternion SLERPがidentityから90度回転までのarcのt=0.25地点で22.5度進み、単位長を保つ契約を追加。既存testは端点とπ回転の中点のみ確認。

- #523 NamedListのwatchSinceが読み取りでは発火せず、failed accessと以後のmutationでcheckpoint通知を行う契約、およびdebugSampleのlimit/element address対応を追加。既存NamedList suiteは監視とサンプリングを確認していない。

- #524 ImageF32x4_RGBA::setFromGpuRGBA32Fのpadding付き2行RGBA readbackがstrideを尊重してBGRA backingと論理RGBA pixelを復元し、不足stride時は既存画像を維持する契約を追加。既存pixel channel order testは1×1転送bufferのみでreadback APIを未確認。

- #525 ImageF32x4_RGBA::setFromGpuBGRA32Fがpadding付き複数行のBGRA入力をstrideどおりに読み、BGRA backingを保ちつつ論理RGBA pixel APIへ正しく反映する契約を追加。既存testに同readback経路なし。

- #526 AudioMixer::deserializeが`buses`の型不正を拒否した場合、先にロード済みのbus構成とeffect rackを変更しない原子性契約を追加。既存persistence suiteは成功documentの再読込/idempotencyを確認するが、不正入力時の既存state保持は未確認。

- #527 CameraGuideのbody depthが0の入力で最小厚みにclampされた後、生成meshの全triangleが非退化（cross-product area > 0）である契約を追加。既存geometry suiteはindex範囲とdepth座標を確認するがtriangle面積は未確認。

- #528 AudioEffect派生fixtureがparameter setterをoverrideしない場合、base `setParameterValue()`は既知/未知IDどちらでも派生parameter状態を変更しない契約を追加。既存base suiteはgetter既定値とderived getterのみ確認。

- #529 DataSourceRegistryがディレクトリ名とbasenameに複数のdotを含むURIでも最終suffixのみでfactoryを選び、compound suffixでは誤dispatchしない契約を追加。既存suiteは単一dotのURI/pathだけを確認。

- #530 DataCacheの上限超過時、途中でcache hitした古いentryを保持し、未アクセスのentryをLRU evictionする契約を追加。既存testはmax entry上限と新entry挿入時の単純evictionのみ確認。

- #531 AudioMixerの同一bus effect rackを異種2段でserialize/deserializeした後もeffect chain順と各段parameterが保持される契約を追加。既存roundtripはbusごとに単一effectのみ。

- #532 AudioMixerがeffect chain途中の未対応type entryをskipしても、その前後の認識済みeffect順を保持する契約を追加。既存unknown type testは末尾entryのみをskip。

- #533 CsvParserがEOFまで閉じないquoted fieldを`MalformedRow`として拒否するケースを追加。enumは存在するが未閉じquote testがなく、現実装はEOF時の`inQuotes`を検査しないため静的には期待fail見込み。ArtifactCore childは未変更。

- #534 DataTableの整数/浮動小数getterが数値prefix後に余分な文字を含む値を拒否し、caller fallbackを返す契約を追加。既存fallback testは全体が非数値の文字列のみ。現行`std::stoll`/`std::stod`呼び出しはparse endを確認しないため静的には期待fail見込み。ArtifactCore childは未変更。

- #535 FormantExtractor::analyzeTrackがchannelごとに異なるサンプル長を持つstereo segmentでも最短channelまでの完全windowだけを解析し、短いchannel末尾を超えてeventを生成しない契約を追加。既存track window testはmonoのみ。

- #536 AudioSpectrumの5.1 layout loudnessがLFE channel (index 3)を除外し、残る5 channelのmean-squareからLUFSを算出する契約を追加。既存loudness testはmono/stereoのみ。

- #537 DataCacheの既存keyを再putするとsource/timestampだけでなくloadedAtも更新され、新source起点でTTL期間が始まる契約を追加。既存TTL suiteは初回put期限とhit非延長のみ確認。

- #538 AudioVibratoの公開parameter introspectionが4 controlsのID/range/default値を返し、ID setter/getterが全controlへ反映し未知IDを変更しない契約を追加。既存suiteは直接setterのみ確認。

- #539 AudioTremoloの`width=1` stereo auto-panで同一sampleの左右が逆相LFOになる独立audio contractを追加。現行processはchannel outer loop内でphaseを全block進めた後にrightを処理するため、静的にはright LFOがblock内で固定され期待fail見込み。ArtifactCore childは未変更。

- #540 AudioWaveformのframe数がresolutionで割り切れない場合もすべての入力sampleをbucket RMSへ含める契約を追加。現行`step = frames / resolution`固定分割は余りsampleを落とすため、静的には期待fail見込み。ArtifactCore childは未変更。

- #541 AudioCache::clearExpiredが古いentryを削除し、直近getでlastAccess更新されたentryを保持する契約を追加。既存cache suiteはclearとLRU容量cleanupのみでage cleanup未確認。

- #542 AudioTapeSaturationのparameter introspectionが4 controlsのID/range/default値を公開し、ID setter/getterが反映され未知IDでは状態を変えない契約を追加。既存suiteは直接setterとJSON roundtripのみ確認。

- #543 AudioParametricEQ::setParameterValueが不正なband ID（数値indexの後ろに文字が続く形式、index欠落、未知property）を無視し既存bandを変更しない契約を追加。既存testは有効ID、未知band index、非有限値を確認するがmalformed ID形式を未確認。現行std::stoiはparse endを検証せず、`band1x.frequency`はband1として誤受理する静的fail見込み。ArtifactCore childは未変更。

- #544 FractionalDelayLine::readSecondsが有限だがsample-rate乗算でoverflowする大きな正の秒数を、read(sample)同様に利用可能な最大履歴へclampする契約を追加。既存testは大きなdelay sample数のclampと非有限秒数のzero-delay扱いを別々に確認。現行readSecondsは乗算overflow後のinfinityをread()へ渡しzero delayに変換するため静的fail見込み。ArtifactCore childは未変更。

- #545 AllPassFilter::clearがdelay historyだけを消去し、設定済みdelay tapとfeedbackを保持する契約を追加。既存clear testは履歴消去のみ確認し、clear後の動作に設定が保持されるかは未確認。

- #546 AudioChorusのstereo各channelが独立したdelay historyを保持し、片側のimpulseが反対側出力へ漏れない契約を追加。実装コメントはL/R別履歴を明記するが、既存standalone testはmono segmentのみ。

- #547 AudioExpanderのparameter introspectionが4 controlsのID/range/defaultを公開し、ID setter/getterが反映され未知IDでは既存値を変更しない契約を追加。既存suiteは直接setterとJSON roundtripのみ確認。

- #548 AudioBufferQueueがchannel vectorは存在するが最短channelのframe数が0のsegmentを拒否し、既存queued segmentを変更しない契約を追加。既存invalid push suiteはchannel無しとinvalid sample rateを確認。

- #549 AudioDownMixerの5.1→7.1 promotionが既存6-channel bedの順序・内容を保持し、新しいback pairをsilenceで初期化してsample rate/startFrameも保つ契約を追加。既存suiteはstereo/mono→7.1級layout、7.1→5.1を確認するが5.1→7.1を未確認。

- #550 AudioPhaserのstereo内部filter/feedback stateがchannel間で独立し、left-only impulseがright outputへ漏れない契約を追加。既存suiteのprocessing testsはmonoのみ。

- #551 AudioCompressorが圧縮されたloud blockの次にquiet blockを処理した際、gain-reduction meterをそのblockの非圧縮値へ更新する契約を追加。既存suiteは圧縮時の値とinvalid/empty block後のresetを確認するが、通常のquiet blockでの回復を未確認。

- #552 AudioBassTrebleがsample rate変更時にlow/high shelf filter historyを初期化し、新rateでの出力が新規instanceと一致する契約を追加。既存suiteはblock continuityを確認するがrate切替後の状態を未確認。

- #553 AudioPlateReverbの稼働中size変更でdelay buffersが再構築され旧sizeのtail stateが消去され、新設定の動作がfresh instanceと一致する契約を追加。既存suiteは異なるsizeの別instance impulse response差のみ確認。

- #554 AudioDCBlockerがSurround51 segmentでも先頭2 channelだけ処理し、center/LFE/surroundの残り4 channelを保持する契約を追加。既存suiteはmono/stereoのchannel state分離を確認するが多chの非対象channelは未確認。

- #555 LfoSource::setPhaseが負の有限phaseを[0,1)へ正規化し、phaseOffset・current phase・sample出力で3/4周期として扱う契約を追加。既存suiteは正のfractional phase、phase advance wrap、非有限phase無視を確認するが負値wrapを未確認。

- #556 AudioSpectrumが連続する隣接segmentを処理した時のintegrated/short-term loudnessが、同じサンプルを1 segmentで処理した結果と一致する契約を追加。既存suiteはdiscontinuous segmentによるintegrated window resetを確認するが連続block等価性は未確認。

- #557 AudioDownMixerの7.1.4→7.1変換が標準7.1 bedの前8 channelを順序・内容・タイミングmetadataごと保持し、height channelを出力しない契約を追加。既存suiteは7.1.4→stereoと7.1→5.1を確認するが7.1.4→7.1を未確認。

- #558 AudioEffect::fromJsonがbypass値の型不正（文字列/数値）をfalseへfallbackする契約を追加。既存suiteは欠損値のfalse fallbackと正しいboolean復元を確認するが型不一致を未確認。

- #559 AudioRasterizer::rasterizeInterleavedが3-channel入力をuneven 2-binへ分割する際、各frameの全channelからmin/maxを集約し全frameを覆う契約を追加。既存interleaved suiteはstereoのみ確認。

- #560 ModulationRouter::removeAssignment後の次processでruntime target indexが再構築され、削除済みtarget modulationが0になりtargetValueがbaseへ戻る契約を追加。既存testはassignment削除後のempty状態だけ確認。

- #561 AudioCache::prefetchのproviderがfalseを返したframeをcacheせず、後続frame要求を継続する契約を追加。既存suiteは全成功・範囲無効・provider例外時継続を確認するがfalse応答を未確認。

- #562 AudioExpanderが同じ絶対値の正負入力へ同一gainを適用し、出力が符号反転対称になる契約を追加。既存suiteは極性を揃えた信号のattenuation/continuityを確認するが正負対称性を未確認。

- #563 AudioExpanderがpeak後の長いrelease中は小信号をthreshold以上のenvelopeとして通し、fresh instanceでは同じ小信号をattenuateする契約を追加。既存suiteはblock continuityとstereo envelope分離を確認するがpeak後のrelease保持を比較していない。

- #564 AudioExpanderがpeak後に十分なrelease時間を経ると小信号を再びthreshold未満としてattenuateする契約を追加。#563のrelease中の保持確認に対し、release完了側の遷移は既存suiteで未確認。

- #565 AudioSpringReverbの処理中にsample rateを変更するとdelay/filter stateが再構築され、新rateで開始したfresh instanceと出力が一致する契約を追加。既存suiteは単一rateのimpulse tailのみ確認し、再構築境界を確認していない。

- #566 AudioSpringReverb::tailSamplesがdecay=0で0を返し、tension増加・decay延長に従って推定tail長を増加させる契約を追加。既存suiteはtailが正であることだけ確認し、設定値への単調応答を未確認。

- #567 AudioSpringReverbの処理中にtensionを変更するとdelay/filter stateが再構築され、変更後tensionで開始したfresh instanceと出力が一致する契約を追加。既存suiteはtension設定値のみ確認し、処理中の再構成境界を未確認。

- #568 AudioSpringReverbのbypass中に入力を処理してもreverb履歴が進まず、解除後の出力がbypass区間を挟まなかった同等instanceと一致する契約を追加。既存suiteはbypass時の入力保持のみ確認し、state不変を未確認。

- #569 AllPassFilterの固定整数遅延・feedback=0.5のimpulse応答がunit energyとなるall-pass特性を追加。既存suiteはdelay tap等を確認するが周波数非着色特性を未確認。未実行。静的に現行出力式へ代入すると出力エネルギーは概算7/3となり期待値と不一致の見込み。現行式がAllPassの契約を満たすか、実行による確認が必要。

- #570 AllPassFilter::processへNaN/±Inf modulationSamplesを渡しても有限入力に対する出力が有限となる契約を追加。既存suiteはsetParameters経由の非有限delay/feedbackを確認するが、sample単位modulationの非有限値を未確認。

- #571 FrameSkipTrackerがrequested/committed/displayed frameを独立して保持し、commit frameがrequestと異なってもrequested frameをdisplayした場合はskip扱いしない契約を追加。既存suiteは3値が一致する成功dispatchとdisplay/request不一致によるskipを確認するが、commit値のみ異なるケースを未確認。

- #572 FrameSkipTrackerのtrackingを無効化しても既存history・skip counterは保持し、その後のdispatch/explicit skipを記録しない契約を追加。既存suiteは空stateから無効化して新規記録を抑制するが、蓄積済みhistory保持を未確認。

- #573 AudioTransientShaperのstereo各channelがenvelope stateを独立保持し、同じ入力を処理するmono instanceの出力とそれぞれ一致する契約を追加。既存suiteはmonoのattack応答のみ確認し、stereo channel間の状態干渉を未確認。

- #574 AudioVibratoを連続blockまたは隣接segmentへ分けて処理した時に、delay履歴とLFO phaseを保持して全出力sampleが一致する契約を追加。既存suiteは単一segment内のdelay応答を確認するが、process呼び出し境界を跨ぐ状態継続を未確認。

- #575 AudioTremoloのmix=0.5出力がdry信号とmix=1変調信号の中点となる線形mix契約を追加。既存suiteはmix default/JSONとmix=0 dryのみ確認するが、中間mixのblendを未確認。

- #576 StereoBalance modeのAudioPanner::calculateGainがelevation値に影響されず、同じazimuthで同一gainを返す契約を追加。既存suiteはazimuthの端点・center・clampのみ確認し、elevationの非関与を未確認。

- #577 AudioDeEsserがcrossoverより十分低いtoneをほぼ変化させず保持する契約を追加。既存suiteは高域toneのattenuationを確認するが、帯域分割後のlow-band preservationを未確認。

- #578 AudioPingPongDelayのstereo cross-feed echoがsegment分割後も履歴を保ち、連続処理と全出力が一致する契約を追加。既存suiteは単一block内の左右交互echoを確認するが、block境界を跨ぐfeedback echo継続を未確認。

- #579 AudioTapeSaturationのwarmthを最大にすると高域toneの定常出力energyがwarmth=0より低下するtone-shaping契約を追加。既存suiteはwarmth=0のdrive効果と設定値を確認するが、warmthの処理効果を未確認。

- #580 AudioBassTrebleで12dB bass boostを適用した時、50Hz toneの定常energyが12kHz toneより大きくなる低域選択性の契約を追加。既存suiteはconstant signalへのbass gainを確認するが、周波数依存の処理効果を未確認。

- #581 AudioPlateReverbのmodDepth=1と0で同じwet impulseを処理した応答が異なる、modulation depthがtank delayへ反映される契約を追加。既存suiteはdefault modulation下のstereo tailを確認するが、modDepth controlの処理効果を未確認。

- #582 AudioChorusのfeedback=0と0.9でwet impulse responseが異なるfeedback control契約を追加。既存suiteはfeedbackのsetter/metadata/JSONのみ確認。未実行。静的確認では`AudioChorus::process`内に`feedback_`参照がなく、現行sourceでは同じresponseとなる見込み。feedback controlの処理経路が必要か実行・設計確認が必要。

- #583 AudioReverbのmix=0でdry impulseを処理してもwet comb historyを蓄積し、次blockでmix=1へ戻した時のwet出力が最初からmix=1だったinstanceと一致する契約を追加。既存suiteはmix=0時のdry出力のみ確認し、wet state蓄積を未確認。

- #584 AudioPhaserのfeedback=0と0.9で同じwet signalを処理した出力が異なる、feedback loopがparameter値を反映する契約を追加。既存suiteはparameter routingとdefault settingsでのsignal変化を確認するが、feedback controlの処理効果は未確認。

- #585 AudioChorusとFlanger modeが同一入力に対して異なるresponseを生成する契約を追加。既存suiteはmode選択とJSON往復のみ確認。未実行。静的確認では`AudioChorus::process`に`mode_`参照がなく、現行sourceは両modeで同じ処理となる見込み。mode別動作仕様または実装経路の確認が必要。

- #586 DiskSizeのsuccessful queryがavailable ≤ free ≤ totalを満たし、usedBytesをtotal-freeとして返す契約を追加。既存suiteは有効queryの成功状態のみ確認し、容量値の相互関係を未確認。

- #587 ArtifactCore::String::reserveが要求容量以上を確保し、既存内容と終端文字を維持し、小さい要求ではcapacityを縮めない契約を追加。既存suiteは初期inline capacityとappend時の成長を確認するが、reserve API自体の保持・非縮小性を未確認。

- #588 ArtifactCore::String::replaceで削除数0を指定した場合、先頭・中間・末尾へreplacementを挿入し、lengthと終端文字を正しく更新する契約を追加。既存suiteは非ゼロ削除の置換と範囲外positionを確認するが、挿入動作を未確認。

- #589 artifactRemoveIfがpredicateを元の各要素にちょうど1回評価し、stableな残存順序と返却要素数を保つ契約を追加。既存suiteは除去結果・相対順を確認するが、predicate呼出し回数を未確認。

- #590 artifactAccumulateがcustom operationを初期値から要素順に左畳み込みし、range内の各要素に1回適用する契約を、非結合な減算と呼出し時の累積値で追加。既存suiteの加算・乗算は結合的で、走査順を区別できない。

- #591 artifactRound(-0.25)が数値上0でありながら負のzero sign bitを保持するIEEE浮動小数点契約を追加。既存suiteは負数の丸め方向を確認するが、負のゼロ符号を未確認。

- #592 ArtifactCore::StringViewのbegin/end反復範囲が明示長の全byteを含み、埋め込みNULを保持する契約を追加。既存suiteはat()/length()/equalityでNUL保持を確認するが、iterator境界を未確認。

- #593 artifactAtan2が負のX軸上でY=+0を+π、Y=-0を-πへ写像するIEEE signed-zero branch契約を追加。既存suiteは非軸上の四象限のみ確認。

- #594 safeNormalizeRangeが有限の±double最大値を端点に受け取っても、差分オーバーフローで退化レンジにせず順序付けて保持する契約を追加。既存suiteは無限端点を確認するが、有限極値の差分オーバーフロー境界は未確認。

- #595 Duration::fromMillis/fromSecondsが±0.75ns相当の浮動小数入力を±1nsへ丸める契約を追加。既存half-away-from-zero testはfromMicrosのみを確認。

- #596 artifactReverseがmove-onlyなunique_ptr要素を反転し、全所有値を失わず保持する契約を追加。既存reverse suiteは整数配列だけを確認。

- #597 SmallVector::removeIfでpredicateに一致する要素がない場合、内容だけでなくmutation count/historyとdebug checkpointのversionも変えない契約を追加。既存suiteは実際に削除するケースのみ確認。

- #598 SmallVectorのmutation historyが最新8 recordだけを保持し、先頭・末尾のversionと件数情報が履歴打ち切り後も正しい契約を追加。既存suiteにbounded historyの検査なし。

- #599 Asset.Sequenceがフレーム番号風の数字で終わっても拡張子のないfilenameをsequence化せずsingleとして返す契約を追加。既存suiteは非数値名・過大桁トークンを確認するが、拡張子欠落境界を未確認。

- #600 SmallVector::watchSinceがreadCountのみの変化では通知せず、checkpoint後の失敗アクセスをsnapshot付きで通知する契約を追加。既存suiteにcheckpoint watcherの検査なし。

- #601 ArtifactRegexのzero-width `^` matchが空入力で1件だけ返り、findAll/replacementが無限進行せず終了する契約を追加。既存suiteは非空入力のzero-width進行のみ確認。

- #602 ArtifactRegex::replaceAllが埋め込みNULより後ろのmatchを置換しても、NUL byteを含むmatch外の入力範囲を出力に保持する契約を追加。既存suiteはregex searchでの明示長NULを確認するが、replacement outputでは未確認。

- #603 ColorSwatch::sortがRGB平均輝度で順序付けし、透明度alphaに影響されない契約を追加。既存sort testは全entryのalphaが1.0でalpha非関与を未確認。

- #604 artifactHypotが一方に∞、他方にNaNを受けた時は∞を返し、NaNと有限値の組み合わせはNaNを伝播する契約を追加。既存suiteは有限値と大入力のoverflow回避のみ確認。

- #605 artifactSqrt(-0.0)が数値上0を返しつつ負のzero sign bitを保つIEEE契約を追加。既存suiteは正数の平方根とartifactAbsの負zero処理を確認するが、sqrt単体は未確認。

- #606 artifactPowが0^0およびNaN^0に対し1を返す指数0の標準浮動小数点特殊ケースを追加。既存suiteは正負整数指数と型昇格を確認するが、zero exponentは未確認。

- #607 CsvDataSourceの`.tsv` registry factoryがtab区切りファイルを2列のtableとして開き、文字列/整数値を返す契約を追加。既存registry integration testはuppercase `.CSV`のみ確認。

- #608 CsvParserがCRLF (`\r\n`) を1つのrecord separatorとして扱い、余分な空行を作らず3レコードを返す契約を追加。既存suiteはLF、CR単独、末尾LFを確認するがCRLFの組み合わせは未確認。

- #609 CsvParserの自動delimiter検出でcommaがない場合の同数候補はsemicolonを優先し、tab/pipeを値内に保持する契約を追加。既存suiteはcommaを含む同数候補のみ確認。

- #610 FloatColorの4チャンネル版`setColor`がRGBに加えてalphaも置き換える契約を追加。既存suiteは個別setterとalphaを維持する3チャンネル版`setColor`を確認するが、4チャンネル一括setterを直接確認していない。

- #611 DiskSizeが無効pathによるquery失敗後に有効pathで再queryするとvalid状態へ復帰し、容量値の整合性を取り戻す契約を追加。既存suiteはvalid→invalidと初回valid queryを別々に確認するが、invalid→valid遷移は未確認。

- #612 AudioCache::prefetchが`int64_t`最大frameを跨がず、開始frameが上限直前の場合は到達可能な2 frameだけproviderへ要求する契約を追加。既存suiteは負値・zero countと通常連番を確認するが、整数上限のoverflow回避は未確認。

- #613 AudioCacheで既存frame keyをサンプル数の異なるsegmentに置換すると、memoryUsageが新segment分だけを数え、旧容量を二重計上しない契約を追加。既存replace testは格納sample更新とentry数のみ確認。

- #614 Matrix4x4::rotationZが負の90度で正X方向を負Y方向へ回す符号規約を確認するケースを追加。既存suiteは正90度と0度のみ確認。

- #615 AudioCache::prefetchのproviderがfalseを返したframeに既存cache entryがある場合、古いsegmentを維持しentryを削除・空置換しない契約を追加。既存suiteは未cache frameのfalse応答をskipする動作のみ確認。

- #616 SpatialGrid::queryの半径がcellSizeを超えて複数cell幅に及ぶ場合も、離れたcellにある範囲内候補を列挙する契約を追加。既存suiteは隣接cell境界の小半径queryのみ確認。

- #617 Size_2Dの汎用`x/y`座標変換が小数の正負値をintへ0方向に切り詰める契約を追加。既存suiteの浮動小数座標は整数値相当のみ。

- #618 AudioWriterのstereo 24-bit WAVヘッダーでbyteRateがsampleRate×channels×3、blockAlignがchannels×3になるassertを追加。既存round-trip reopen caseはchannels/sampleRate/bitDepth/data sizeを確認するが、この2 header fieldは未確認。

- #619 AudioWriterの再オープン後stereo WAVでRIFF chunk sizeがfile sizeから8 byteを引いた値に更新されるassertを追加。既存suiteは16-bit monoのRIFF sizeと2ファイル目のbyte sizeを別々に確認するが、再オープン時のRIFF fieldは未確認。

- #620 AudioWriterが16-bit PCMの正負0.5 sampleをそれぞれ+16384/-16384へ量子化する独立payloadケースを追加。既存suiteは0と±1およびclamp後の±2を確認するが、符号付き中間振幅は未確認。

- #621 AudioMixer FX persistenceで再読込時にmaster `effects: []`を明示すると、以前復元済みrackを空に置き換える状態遷移testを追加。既存idempotence testは同じ非空rackの重複防止のみ確認。

- #622 AudioMixer FX persistenceで通常busの再読込時に`effects: []`を指定し、以前復元済みのrackが空へ置き換わる分岐testを追加。#621は別実装経路のmaster rackのみ対象。

- #623 AudioSegment::frameCountが複数channel中1つでも空なら0を返しつつ、channelCountは保持する契約を追加。既存suiteは正の異なるchannel長の最小値のみ確認。

- #624 CsvDataSourceのreload後に`info().fileSize`が更新済みCSVの実ファイルbyte数へ追随するassertを追加。既存reload testはtable内容のみ確認し、file metadataを再検査しない。

- #625 AudioTransientShaperのfast/slow envelope状態が同一sample rateの複数process blockを跨いで連続し、単一block処理と各sampleが一致するケースを追加。既存suiteはsingle-block processingとchannel別stateを確認。

- #626 AudioTransientShaperがSurround 5.1 segmentでも先頭2chのみを処理し、center/LFE/surroundの4chを未変更で保つ契約を追加。既存suiteはmono/stereoのみ確認。

- #627 AudioRasterizer::rasterizeInterleavedでbinCountがcomplete frame数を上回る場合、1 frameごとのbinへ縮退し、各frameの全channel min/maxを保つ契約を追加。既存suiteはinterleaved uneven binsと不足frame境界を確認するが過剰bin数は未確認。

- #628 RandomStream::gaussianへ同じseedと正負の標準偏差を渡した場合、同じ標準正規サンプルを平均値の両側へ反転して返す契約を追加。既存suiteはseed再現性と標準偏差0のみ確認し、負の標準偏差が符号反転として扱われることは未確認。

- #629 ColorSwatch::saveの`.gpl`拡張子dispatchがexport経路を通し、保存した色entryをloadで復元できる契約を追加。既存suiteはexportGPL直呼びとloadによるround-trip、およびunsupported extension時のsave失敗のみ確認。

- #630 AudioWriter::closeFileを確定済みWAVに対して再度呼んでも、保存済みヘッダー・payloadを含むファイル全体が変化しないidempotent close契約を追加。既存suiteはcloseとdestructorによるfinalizeを確認するが、close後の再呼び出しによる内容保持は未確認。

- #631 AudioCache::getCachedで取得したsegmentのサンプル変更・拡張がcache内の保存segmentへ影響しないcopy-isolation契約を追加。既存suiteは取得時の値保持を確認するが、取得後の出力mutationからの独立性は未確認。

- #632 AudioCacheのcapacity超過cleanupでgetCachedによるcache hitがlastAccessを更新し、未アクセスの古いframeが先にLRU evictionされる契約を追加。既存suiteはsize上限のみ確認し、eviction victim選択は未確認。

- #633 AudioDownMixerのsame-layout sanitizeで非有限sampleをoutput側だけ置き換え、const source segment内のNaN/Infを変更しない入力独立契約を追加。既存suiteは出力値のsanitizeを確認するが、処理後のsource保持は未確認。

- #634 AudioSpectrum::normalizeToTargetLufsが正規化対象segment内のNaN/Inf sampleをsilenceへ置き換え、残りの出力値をfiniteに保つ契約を追加。既存suiteは有限入力のgain適用と非有限target時のunity保持を確認するが、非有限入力sampleのsanitizationは未確認。

- #635 AudioWaveformのRMSが最大有限float sampleでoverflowしてinfinityにならず、float表現可能上限の有限値へ収まる契約を追加。既存suiteは通常振幅とNaN/Inf除外を確認するが、最大有限値境界は未確認。

- #636 AudioSpectrumのintegrated loudness frame endが`int64_t`最大値で飽和した後、次blockのstartFrameが最大値なら連続windowとして両blockのenergyを統合する契約を追加。既存suiteは通常frame位置での連続／不連続のみ確認。

- #637 AudioAnalyzerへ最大有限float sampleを入力した場合もRMS/peakが最大有限値を保ち、FFT spectrum全binが有限値となるoverflow境界契約を追加。既存suiteは通常振幅と非有限入力のsanitizationを確認するが、最大有限入力は未確認。

- #638 AudioDelayがNaN/Inf入力をsilenceとして遅延bufferへ格納し、後続のfinite sampleにも非有限値をfeedback経由で伝播させない契約を追加。既存suiteはdelay timing、channel分離、bypass等を確認するが、非有限入力とbuffer状態の健全性は未確認。

- #639 AudioReverbがSurround 5.1入力の先頭2chだけを処理し、center/LFE/surroundの残り4chを入力値のまま保持するchannel-scope契約を追加。既存suiteはstereo間のhistory分離のみ確認し、stereo超過chは未確認。

- #640 AudioStereoWidenerがmono segmentをpass-throughする際、stereo bass-mono filter stateを進めず、次のstereo blockの出力をmono blockなしの場合と一致させるstate-isolation契約を追加。既存suiteはmono pass-throughと隣接stereo block continuityを個別に確認。

- #641 AudioBitCrusherでmix=0のblockを処理してもsample-hold downsample stateを消費せず、その後mixを有効化したblock出力が未処理時と一致する契約を追加。既存suiteはmix補間とdownsample state continuityを確認するが、zero-mix中のstate扱いは未確認。

- #642 AudioTremoloのLFO phaseが複数segmentを跨いで保持され、block分割処理の全sample出力が連続処理と一致する契約を追加。既存suiteは長block modulationとstereo phase offsetを確認するが、segment境界連続性は未確認。

- #643 AudioExpanderをratio=1で処理したblockがsignalとenvelope stateの双方を変更せず、後続の再有効化出力がそのblockを処理しなかった個体と一致する契約を追加。既存suiteはratio=1中のsignal保持のみ確認。

- #644 AudioDelayがmix=0の間もdelay bufferへ入力履歴を蓄積し、後からwet mixを有効化すると同じtailが現れる契約を追加。既存suiteはzero mixのdry出力保持のみ確認。

- #645 AudioPingPongDelay::tailSamplesがfeedback=0では0を返し、既知のdelay sample数とfeedback decay式に一致し、feedback増加で推定tailが長くなる契約を追加。既存suiteはdefault時の正値のみ確認。

- #646 AudioCache::prefetchの大きな要求を1回4096 frameへ制限し、provider呼び出し数と最終要求frameが上限に一致するbounded-work契約を追加。既存suiteは逐次要求とint64終端の安全停止を確認するが、通常時の上限値は未確認。

- #647 AudioCache::clearExpiredへ負のmaxAgeMsを渡した場合も0msへsanitizationされ、前回access時刻より後の時点で古いentryが期限切れとなりmemory usageも0へ戻る契約を追加。既存suiteは正のage閾値のみ確認。

- #648 `float3<float>::normalized()`が各成分は有限でも内積和がfloat上限を超える大きさのvectorを単位vectorへ正規化する境界契約を追加。現行のfloat dot/length先行計算では静的にoverflowが見込まれ、期待fail候補。ArtifactCore childは変更せず。

- #649 XYZColorのtoString/fromString往復でQtが科学表記する微小・大成分も維持する契約を追加。現行parser regexは指数部を受け付けないため静的には期待fail見込み。ArtifactCore childは変更せず。

- #650 QuaternionのEuler変換でY軸角が±90度のジンバルロック位置でも、toEuler/fromEuler往復後の回転が元と等価になる契約を追加。通常角度のround-tripは既存suiteで確認済みだが、asinの端点と角度の縮退ケースは未確認。

- #651 FrameSkipTracker::reset()が履歴・skip集計を初期化してもtracking無効設定を保持し、reset後もイベント記録を抑止する契約を追加。既存suiteはreset後にtrackingを有効化するが、disabled設定の保持は未確認。

- #652 RotationTurnsのmove constructor後、null implになった元オブジェクトが再度move assignmentを受けて通常利用可能になる契約を追加。既存suiteはmove元の破棄とlive destinationへのmove assignmentを確認するが、moved-from objectの再利用は未確認。

- #653 CsvParser::parseFile()が一時CSVファイルを開き、parseStringと同様にheader/data rowsを返す公開ファイル経路の契約を追加。既存suiteはparseStringのみを確認しており、parseFileの正常読み込みを一度も呼び出していなかった。

- #654 Audio Spatial distanceAttenuationの各モデルが距離の負／正の無限大を近／遠距離境界として扱い、linearは0、inverse/exponentialはmaxDistanceでの係数に収束する契約を追加。既存suiteは有限値での境界・補間のみ確認。

- #655 CsvParser::parseFile()が存在しないパスに対してFileNotFoundを返し、header/rows/countを空のままにするエラー契約を追加。parseFileの成功経路は#653で確認済み、欠損パス経路は未確認。

- #656 CsvParser::parseFile()が開ける空ファイルをEmptyFileとして返し、parse resultにheader/rowsを作らない契約を追加。parseStringの空入力とparseFileの欠損パスは既存／追加suiteで確認するが、実ファイルのempty-content分岐は未確認。

- #657 Audio Spatial distanceAttenuationへNaN距離を渡しても各モデルのattenuationが有限値に保たれる契約を追加。現行のlinear/inverse/exponential計算はいずれもNaNを伝播するため静的な期待fail候補。ArtifactCore childは変更せず。

- #658 CsvParser::parseFile()がUTF-8日本語fieldを読み込み時に保持する契約を追加。parseFile成功経路はASCII値のみ、parseString suiteにも日本語fieldのcoverageがなかった。

- #659 ResolutionRemap::calculateImpact()のmask/keyframe/anchor各入力が単独でimpactを立て、それ以外の個別flag/countを汚さない契約を追加。既存testは3要因を同時に有効化しており、単一要因の寄与を分離していなかった。

- #660 AudioPanner::applyPanning()がstereo gainで指定されないSurround 5.1後続channelを変更・短縮せず、non-finite値を含む元sampleも保持する契約を追加。既存suiteは有限値の未対応2ch目のみ確認。

- #661 AspectRatio::setFromString()がint範囲を超えるwidthまたはheightのpairを拒否し、事前設定値を保つ契約を追加。既存suiteは不正形式・非正値・int最大値のconstructorを確認するが、parserの数値overflowは未確認。

- #662 ResolutionRemap::remapScale()がold/newのどちらかがempty sizeの場合に元scaleを維持する契約を追加。positionとtransformはempty-size identityを既存suiteで確認する一方、scaleの同等境界は未確認。現実装はsize検査がなくscaleを0へ落とすため静的期待fail候補。ArtifactCore childは変更せず。

- #663 SimpleWavのUniString版loadFromFile/saveToFileがUnicodeパスをQString版へ正しく委譲し、音声内容を維持する契約を追加。既存suiteはQString版のみ使用し、UniString overloadは未呼び出し。

- #664 SimpleWav::loadFromFile()が明示的な負のmaxFramesを無制限 sentinelとして扱い、全interleaved frameを読み込む契約を追加。既存suiteはdefault引数と0を確認するが、負数引数は未確認。

- #665 DataTable::clear()後も同一instanceが新しいschemaとrowを受け入れ、旧column lookupを復活させない再利用契約を追加。既存suiteはclear直後の空状態のみ確認。

- #666 AudioBufferQueue::clear()が満杯時の全100 slotを解放し、empty/not-full状態へ戻して新規segmentを受け入れる契約を追加。既存suiteは満杯拒否と少数segmentのclearを個別に確認するが、満杯からのclearは未確認。

- #667 RandomStream::nextU32()がnextU64()と同じ1 draw分だけstateを進め、64bit drawの上位32bitを返す幅間整合契約を追加。既存suiteは同じ幅同士の再現性のみ確認。

- #668 TimeCode::setFromQString()が通常のcolon-separated `HH:MM:SS:FF` を現在fpsでフレームへ変換し、non-drop `:` 表記のままround-tripする契約を追加。既存suiteはdrop-frame parseと末尾dotのみ直接確認。

- #669 AudioRingBuffer::write()が内部上限の10chを超える入力を拒否し、拒否時に既存のbuffered frameを保持する契約を追加。既存suiteは10chまでの正常系を確認するが、11ch rejectionとfailure後のdata preservationは未確認。

- #670 calculateSpeakerGains()が有限だが二乗和でfloat overflowする極大方向入力を受けても、全gainを有限・非負に保ち単位energyへ正規化する契約を追加。既存suiteはNaN/Infと通常サイズの方向を確認するが、有限値の演算overflow境界は未確認。

- #671 Size_2D::area()が幅または高さ0のdegenerate sizeで0を返す契約を追加。既存suiteは通常のareaと高さ0のaspect ratioを確認するが、両軸のゼロ寸法に対するareaは未確認。

- #672 makeNukeStyleCameraGuidePrimitive()が負のbody depthを最小厚み0.01へclampする契約を追加。既存suiteはゼロdepth clampを確認するが、負値入力の下限動作は未確認。

- #673 ColorSwatch::importGPL()がCRLF改行のpalette nameとentry nameを末尾CRなしで読み込む契約を追加。既存suiteはLFデータのみ確認。現状はgetline後のCRを除去しないためPOSIX環境で失敗する見込み。

- #674 AudioBassTreble::process()がチャンネルごとに長さの異なるsegmentで共通frame数のみ処理し、長いチャンネルの余剰末尾sampleを保持する契約を追加。既存suiteは各channelが同じframe数の入力のみ確認。

- #675 AudioBitCrusher::process()がStereoを超えるSurround 5.1入力で先頭2 channelだけを処理し、center/LFE/surround channelのPCM値を保持する契約を追加。既存suiteはstereo channel間のhold state分離を確認するが、追加channelの保持は未確認。

- #676 AudioParametricEQ::getParameters()がsetBandCount()後のband数に合わせて3項目ずつmetadataを返し、最後のbandのID/valueを反映する契約を追加。既存suiteは初期4 bandのmetadataのみ確認。

- #677 Matrix4x4のtranslation×scale合成順序がz座標にも適用され、translation後にz scaleされず期待位置を保つ契約を追加。既存suiteは合成順序のx/yのみ確認し、z合成は未確認。

- #678 AudioStereoWidener::process()がSurround 5.1入力を処理する際、先頭L/R channelだけを変更しcenter/LFE/surroundの追加channelを保持する契約を追加。既存suiteはmono passthroughとstereo widener処理を確認するが、多channel入力は未確認。

- #679 float3::normalized()が各成分float最大有限値の有限非ゼロvectorをoverflowさせずunit vectorへ正規化する契約を追加。既存suiteは二乗しても有限の大きな成分まで確認。現在のdot-based lengthは二乗和でoverflowするため静的期待fail候補。実行未確認。

- #680 SmallVector::front()/back()がempty時にnullptrを返し、要素がある場合に先頭／末尾要素へのpointerを返す契約を追加。既存suiteはprefix/suffix値検索を確認するが、pointer accessorは未確認。

- #681 slerp(a,b,t)とslerp(b,a,1-t)が同一orientation（quaternion符号を除く）を返す対称性契約を追加。既存suiteはendpoint・中間arc・shortest pathを確認するが、endpoint/progress reversal invariantは未確認。

- #682 ColorSwatchのmove構築とmove代入がpalette nameおよびentriesを移し、代入先の旧paletteを置換する契約を追加。既存suiteはcopy構築・代入のみ確認。

- #683 DataCacheがmaxEntries満杯時に既存keyを置換しても無関係なkeyをevictしない契約を追加。既存suiteは容量未満での同一key置換と、新規key追加時のLRU evictionを個別に確認。現実装はput前に無条件evictするため静的期待fail候補。実行未確認。

- #684 TypeInferenceがint64範囲外の数値文字列をIntに分類せずFloatとして扱う契約を追加。既存suiteは通常のsigned integerのみ確認。TypedColumnのInt変換先はint64_tだがTypeInference::isInt()は数字列かのみ見るため静的期待fail候補。実行未確認。

- #685 DataTable::getInt()がint64最大／最小文字列を正確に読み、各方向の範囲外入力ではcaller fallbackを返す契約を追加。既存suiteは小さなintegerとmalformed suffixのみ確認。


- #686 AudioAnalyzer::analyze()が低／中／高帯域強度をスペクトルの対応bin平均として算出し、ナイキスト上限を越えるhigh帯域終端を最後のbinへclampする契約を追加。既存suiteは解析指標・スペクトル形状は確認するが、帯域平均のbin対応は未確認。

- #687 AudioWaveform::process()がフレーム数10・resolution4の端数区間で末尾フレームを落とさず最終binへ反映する契約を追加。既存の端数テストは5/2だけだが、現実装のstep=frames/resolutionと固定長区間では一般の余りを処理できず、本ケースは静的期待fail候補。実行未確認。

- #688 AudioDecibelsの除算演算子と複合除算が負の除数でdB値の符号を反転する契約を追加。既存suiteは正の除数、ゼロ近傍、非有限除数を確認するが、負の有限除数は未確認。

- #689 AudioBitCrusher::fromJson()がパラメーターキー欠落時にbits=8、downsample=1、mix=1の既定値を適用する契約を追加。既存suiteは全キーを含むtoJson()/fromJson()往復のみ確認。

- #690 AudioBitCrusher::process()がfloat downsample値を最近整数へ丸め、2.49では2 sample、2.5では3 sampleのhold周期を使う契約を追加。既存suiteは整数downsample倍率のみ確認。

- #691 Asset Sequenceの18桁frame-number境界でもSequenceGroup::pathPattern()がpadding幅を保持して`%018lld`を生成する契約を追加。既存の18桁検出テストはframe値・filenameのみ確認し、pattern幅は未確認。

- #692 DataTable::getBool()がdouble格納値をboolへ暗黙変換せず、caller fallbackを返す契約を追加。既存suiteはstring token、int64、monostateの変換／fallbackのみ確認。

- #693 DataTable::getFloat()がdouble格納値を列番号版・列名版の双方で精度を保って返す契約を追加。既存suiteは数値文字列とint64格納値からの変換のみ確認し、double variant直接取得は未確認。

- #694 DiskSize::formatBytes()が1 MiBの1 byte手前を小数1桁丸め後の単位境界に従って`1.0 MB`と正規化する契約を追加。現実装はunit選択後に丸めるため`1024.0 KB`となる静的期待fail候補。実行未確認。

- #695 FrameSkipTrackerのdispatch eventがrequested/committed/displayed frame番号のint64最小値・最大値を保持し、不一致をUnknown skipとして記録する契約を追加。既存suiteは通常のframe indexのみ確認。

- #696 AspectRatio::setFromString()が探索上限denominator=100より小さい正のdecimal比を最小の正の有界近似`1:100`へ写す契約を追加。現実装は全候補numeratorが0となり初期候補`1:1`を残す静的期待fail候補。実行未確認。

- #697 DataTable::getBool()がDataValueに直接格納されたtrue/falseをそれぞれ正しく返す契約を追加。既存suiteはstring token、int64変換、double/monostate fallbackを確認するがbool variant分岐は未確認。

- #698 DataTable::getFloat()がdouble範囲外となる指数形式文字列を変換失敗として扱いcaller fallbackを返す契約を追加。既存suiteは非数値文字列とnumeric-prefix後続文字を確認するが、`std::stod`のrange errorは未確認。

- #699 artifactSort()が空・単一要素rangeを安全に処理し、単一要素でcomparatorを呼ばない境界契約を追加。既存suiteは複数要素のduplicate/sort方向を確認するが、要素数0/1は未確認。

- #700 RandomStream::chance()が±infinity確率をfalse/trueへ決定し、0/1外の確定結果と同様に乱数stateを進めない契約を追加。既存suiteは有限の範囲外確率とNaNを確認するが、無限大は未確認。

- #701 ImageAnalyzer::percentile()のp=0がhistogram内の最小占有binを返す契約を追加。現実装はtarget=0で空のbin 0を最初に選ぶため静的期待fail候補。実行未確認。


- #702 CsvParserが引用フィールド内のCRLFをレコード区切りとして分割せず、CRとLFの両方をfield contentとして保持する契約を追加。既存suiteは引用フィールド内LFと引用外CR/CRLFレコード区切りを個別確認するが、引用内CRLFは未確認。

- #703 mulSat()が符号付き乗算の`lowest * -1`と正負両方向のoverflowを飽和し、負数同士の通常積を保持する契約を追加。既存suiteは正のoverflow例のみ確認。

- #704 saturateCast()がint64/uint64間の符号と上限境界を正しく飽和する契約を追加。既存suiteはsigned negativeからunsignedへのunderflowのみ確認。

- #705 addSat/subSat/mulSatのunsigned型で加算overflow・減算underflow・乗算overflowを飽和し、範囲内計算を保持する契約を追加。既存suiteはsigned演算のみ確認。

- #706 saturateCast()が狭いsigned/unsigned targetへの上限・下限変換をclampし、範囲内値を保持する契約を追加。既存suiteはunsigned charへの負値変換とint64/uint64境界のみ確認。

- #707 saturateCast()のsame-sign narrowing（signed→signed、unsigned→unsigned）がtarget最小/最大を越える値をclampし、端点を保持する契約を追加。既存suiteは符号を跨ぐcastのみ確認。

- #708 ArtifactFunctionRefが`int`戻りcallableを`double()` signatureでborrowし、呼び出し時に戻り値を変換する契約を追加。既存suiteは同型戻り値とreference returnを確認するが、戻り型変換は未確認。

- #709 AbstractPropertyのInteger modulation結果が小数部分を最近接整数へ丸める契約を追加。既存のmodulation testはFloatの範囲clampを確認するが、Integer専用のrounding分岐は未確認。
