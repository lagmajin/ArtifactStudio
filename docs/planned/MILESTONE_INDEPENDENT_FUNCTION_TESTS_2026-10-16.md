# 独立機能テスト拡充マイルストーン

**最終更新:** 2026-10-16
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

38項目すべてについて、既存カバレッジまたは追加したケースを具体的に確認し、重複のないテスト配置を決める。実装対象はテストコードと必要最小限のtest-only fixtureに限る。ユーザー方針でビルド・実行できない間は、runtime検証を未完了として明示し、完了扱いにしない。
