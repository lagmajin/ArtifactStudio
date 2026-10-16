# 独立機能テスト拡充マイルストーン

**最終更新:** 2026-10-16
**ステータス:** In Progress

## 目的

優先度高として挙げた機能テスト候補を、既存suiteとの重複を避けながら順番に監査・拡充する。製品アプリの起動、ユーザーデータ変更、GPU依存を避けられるものはCoreの独立契約テストに置く。サービス統合、レンダー、GUIが不可避な項目は別suiteへ分け、対象範囲を明示する。

各項目は「既存テストで十分」「既存suiteへケース追加」「独立suite新設」「仕様確認待ち」のいずれかを調査して決める。テストが追加されたことと、ビルド・実行で合格したことは区別する。

## 実装順・追跡

| # | テスト領域 | 主な独立境界 | 状況 |
|---:|---|---|---|
| 1 | キーフレーム補間 | Core `KeyframeSplineTest` | 負時刻・Constant終端・負の線形速度とトラック外速度0のケースを追加。未実行 |
| 2 | フレームレート変換 | Core `FrameRateContractTest` / `FrameTimeTest` / `RationalTimeContractTest` | 既存suiteでexact/nominal rateのFramePosition↔RationalTime変換、正負half-frame丸め、TimeCodeのdrop-frame 10分境界、invalid rate fallback、storageScaleのhalf boundary sweep・飽和・reference gridまで確認。重複追加なし。runtime未実行 |
| 3 | PropertyPath | Core `PropertyPathTest` | 空・区切り文字のみ、owner/property片側空ケースを追加。未実行 |
| 4 | ID・参照整合性 | Core ID契約、Composition Node Store | Node Store用の独立CPU contract targetに6ケース（duplicate/unknown parent/self・ancestor cycle atomicity/removal detach/order+JSON roundtrip/order update/invalid JSON import）を追加。未ビルド・未実行 |
| 5 | 選択状態遷移 | Selection model/service | 独立PrecomposeViewerに選択遷移ケースあり。重複追加せず実行状態を追跡 |
| 6 | レイヤー順序操作 | 順序モデル/service | Composition Node StoreにsetOrder・親子順序反映・不正値拒否を追加。未ビルド・未実行 |
| 7 | レイヤー複製契約 | Layer model/service | Project round-trip独立suiteへID独立・opacity複製・無効ID拒否を追加。未ビルド・未実行 |
| 8 | Undo/Redoトランザクション | Undo command/model | 独立Manager contract targetにpush・undo/redo・Redo分岐・push時失敗補償・Undo/Redo中の部分失敗補償・budget拒否・macroの通常動作とmacro内子コマンド失敗時の部分巻戻し（redo/undo双方）を追加。未ビルド・未実行 |
| 9 | 保存データ移行 | Serialization/schema migration | ArtifactCore独立suiteに複数段移行・未知field保持・不正登録・経路不在時の不変性・複数経路時の最短前方経路選択を追加。静的監査でmigration関数例外時は入力JSONへ段階反映され部分更新となる可能性を発見（ArtifactCore child repo、本作業では未変更）。未ビルド・未実行 |
| 10 | 部分失敗時の原子性 | Service/API contract | PrecomposeViewerの独立`--check`回帰経路にinvalid request・empty child・cycle拒否などがあり、失敗時の構造・選択・undo/redo履歴を比較。Project RoundTrip suiteにもnil layer/Composition拒否、不正Project置換load失敗時に既存Project/Composition/Layer保持を確認する独立ケースあり。suite結線は静的確認済み。未ビルド・未実行 |
| 11 | Effect Stack編集往復 | Effect sequence | Project RoundTripへEffect削除・再挿入後の型順・個別設定値の保存再読込ケースあり。CPU contractでstage順と同stageの安定順、および画素出力も確認。Undo連携は別項目。未ビルド・未実行 |
| 12 | Mask/Effect順序 | CPU小画像統合 | 独立CPU integration targetの固定32×16 fixtureでmask内部・境界・外部のalpha勾配を確認。target登録を静的確認済み。未ビルド・未実行 |
| 13 | Track Matte参照 | Matte model/render | Project RoundTrip独立suiteでsource削除後にtargetを保持し、dangling参照のみ除去されることを確認。未ビルド・未実行 |
| 14 | 静止画レイヤー置換 | Layer/asset integration | 別パスへの置換後、source path・新寸法・画素と、opacity・positionの編集状態保持を確認。未ビルド・未実行 |
| 15 | 連番画像欠番・再接続 | Asset sequence/import | 欠番で古い画素をplaceholderへ置換し、欠番解消後の再接続、同一パス上の壊れた内容でstale画素を出さないこと、および有効内容へ更新後の再読込を既存suiteで確認。未ビルド・未実行 |
| 16 | 再読込後の参照修復 | Project/asset integration | 欠損画像パスとplaceholderをProject保存・再読込後も保持し、同じパスから画素を再読込できるケースあり。未ビルド・未実行 |
| 17 | Undo/Redo後の保存 | Project/Undo integration | Project RoundTripでUndo結果とRedo結果をそれぞれ別ファイルへ保存・再読込して値を確認。Undo履歴自体の永続化ではなく結果状態の保存契約。未ビルド・未実行 |
| 18 | 編集・保存・再読込 | Project round-trip | 既存suiteで複数Composition/Layer/notes/extension dataの保存再読込と、再読込後の編集・別名保存・再読込を確認。追加なし。未実行 |
| 19 | 複製・親子変更・削除 | Layer/project integration | Node Store独立suiteに有効なreparentとroot detach後のchildrenOf反映を追加。既存の親削除時direct child detach、Project suiteの複製・削除と無効操作拒否も静的確認。未ビルド・未実行 |
| 20 | プリコンポーズ・子編集・解除 | Project service integration | PrecomposeViewer独立回帰ケースで子編集値の解除・Undo/Redo保持、nested unwrap、保存再読込後の構成とManager階層復元、拒否時原子性を確認。Windows専用viewerの`--check`経路（CTest登録ではない）を静的確認。suite改変なし、未ビルド・未実行 |
| 21 | Playback範囲 | Playback model/service | 独立CPU `ArtifactInOutPointsContractTest` に未設定・両端設定・片側クリア・逆順・Composition外点を追加。別processの`ArtifactPlaybackRangeServiceContractTest`でAll/WorkAreaのEngine range適用とIn/Out clippingを追加。サービスはQThread/AudioRendererを構築するが再生は開始せず、Core event loopの専用targetに隔離。未ビルド・未実行 |
| 22 | Transport状態機械 | Playback Service | 既存の独立Playback Service processにPlay→Pause→Play(Resume)→Stopの状態遷移ケースを追加。RAM/disk preview無効化、Composition解除とstop/wait cleanupを行う。Service生成・実再生はQThread/AudioRendererを伴うため通常CPU unitより負荷はあるが、Artifactアプリ本体とは別processに隔離。未ビルド・未実行 |
| 23 | Render Cache key | Render controller/model | 独立Core `RenderPipelineContractTest` がframe/backend/scene/settings/dimensions/color/feature flagを検査。現ArtifactCoreソースの`RenderInputSnapshot::cacheKey()`は`requiresCompute`/`requiresHDR`/`usesTemporalHistory`を`RenderCacheKey`へ代入し、stable hashにも含めることを再確認。`CacheKeyTracksRemainingIdentityColorAndFeatureFields`の期待と静的には整合。runtime未確認 |
| 24 | Cache invalidation範囲 | Layer Surface Cache | 独立Windows GUI harnessでcache hit・対象Effect更新後のinvalidaton/rebuild・無関係Layer非干渉を検査し、未変更rerender前後のpresented Composition frameをpixel完全一致比較するよう拡張。ArtifactCore IFC問題でビルド/runtime未確認 |
| 25 | Dirty範囲計算 | Bounds/renderer contract | 独立Standalone suiteで変換四隅/AABB/回転/scale/負scale/包含・端点/退化矩形/角度等価性まで確認。追加なし、未実行 |
| 26 | 画像境界・形式変換 | Core image contract | 独立suite群でinvalid pointer/dimension/stride、padded row stride、premultiplied alpha、byte/float変換、file decode境界を分担して検査。今回追加なし、未実行 |
| 27 | GPU resource世代管理 | GPU resource contract | Core `RenderPipelineContractTest`でGI frame資源のresize世代・descriptor寸法とtemporal historyの連続・frame欠落・camera cut無効化を確認。独立CPU `ArtifactFrameCacheContractTest`にもgeneration bump後の旧entry非可視化とstale cleanupを確認。実GPU device世代復旧は未カバー。未ビルド・未実行 |
| 28 | Hot path容量上限 | 診断・負荷suite | 独立CPU `ArtifactFrameCacheContractTest` にフレーム数上限・メモリ上限・LRU eviction・予算超過entry拒否・Composition/frame/layer/effect指定invalidatonを追加。Image sequence cacheのentry/byte上限とPlayback stress harnessのsample上限も既存テストでカバー。Playback Service suiteへ公開API経由で129枚を投入し、128件上限・最古entry退避・既存frame再store後のLRU退避順を確認するケースを追加。未ビルド・未実行 |
| 29 | 再生中Composition切替 | Playback integration | 既存Playback stress harnessの単発切替でactive index変更・Service current Composition一致・切替前再生中なら再開・Transport widget一致を明示判定。2つ以上のCompositionで実行が必要。stress未実行 |
| 30 | Playback ServiceとUI同期 | Playback/UI integration | 独立Playback stress harnessに実際の`ArtifactPlaybackControlWidget`を配置し、ServiceとUIのPlaying/Paused/Stopped query一致をstate出力、Composition切替、resize、連続巡回、run完了時に検査。GUI runtime未実行 |
| 31 | Cache有効/無効画像比較 | Renderer integration | Composition Layer Surface Cache harnessはcache hit/miss/build/invalidation診断のみで、cache mapを無効化する公開/test-only経路はない。cache bypassにはArtifact child repoのcontroller API・描画呼出し変更が必要。AGENTS.mdの「子repoは明示依頼なしに編集禁止」により今回は範囲外。画像の同一条件比較は未追加 |
| 32 | アニメーションTransform GPU描画 | Headless GPU integration | RenderQueue GPU integrationで一般Solid layerのPosition X keyframeをGPU指定で2 frame出力し、赤画素の重心がframe間で80px超移動することを比較する独立CTestケースを追加。GPU依存。未ビルド・未実行 |
| 33 | 大画像＋Effect | Render stress/integration | 既存CPU Effect contract suiteへ1280×720 surfaceのBrightness処理・端/中央画素・入力保持を追加。GPU/render queue負荷比較は未カバー。未実行 |
| 34 | 非表示レイヤー負荷 | Render stress | Playback stress harnessに`stress.compareVisibilityProfiles`を追加。同じComposition・再生run内で全表示と奇数index半数非表示を同じ時間計測し、可視/非表示数、100ms samplingのframe進行率、stall sample、提示frame delta、drop deltaを比較可能。元の可視状態は計測後に復元する。runtime比較は未実施 |
| 35 | リサイズ中の描画安定性 | Viewer/GPU GUI | 独立Playback stress harnessへ再生中リサイズ操作を追加。Qt event processing後のframe進行・再生継続・Viewport正寸法・Transport状態一致を判定。GPU/GUI未実行 |
| 36 | 複数Composition連続再生 | Playback stress | 独立Playback stress harnessに`stress.cycleCompositions`を追加。再生中に全Compositionを順番に切り替え、各切替後のComposition一致・再生継続・フレーム進行を判定し、最後に開始Compositionへ戻る。終了時stop/wait/参照解放処理も既存実装で確認。GPU/GUI実行は未確認 |
| 37 | GPU device喪失後の復旧 | GPU integration | 独立GPU processに`DiligentDeviceManagerRecoveryContractTest`を追加。headless device作成後に公開`markDeviceLost()`→destroy→再初期化し、新device/contextを確認する。backend device removalやPresent失敗を注入しないため、ArtifactIRendererの自動recovery経路・実GPU喪失は別途未カバー。GPU依存・未ビルド/未実行 |
| 38 | Export中のUI応答 | Export/UI integration | 既存EXR RenderQueue統合ケースを48-frame出力に拡張し、読み取り可能なEXRと描画中のGUI timer event処理を同時に検査。GPU依存。未ビルド・未実行 |

### 2026-10-16 追加確認

- `CompositionNodeStore::fromJson()` は配列内の各要素を順次 `addNode()` に通す。重複IDは最初の項目が残り、存在しない親・空ID・オブジェクト以外は捨てられる。独立CPU契約テストでこの復元挙動を確認するケースを追加。
- `ArtifactInOutPoints::effectiveRange()` はIn/Outが逆順なら端点を並べ替えるが、Composition境界外の明示点は clamp しない。この契約の追加ケースを登録。
- #2既存suiteを再監査。FrameTimeのFramePosition/TimeCode変換と正負境界、RationalTimeのoverflow/rounding/reference grid、FrameRate storage scaleの境界・飽和ケースが既にあり、追加候補の重複を避けた。
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
- #31の既存Composition Layer Surface Cache harnessは`setLayerSurfaceCacheDiagnosticsEnabled()`とhit/miss/build/invalidation countersを使用する。cache有効/無効切替や同一条件での画像出力比較は行っていない。controllerはArtifact child repoにあり、test bypass追加には子repo編集が必要なため現依頼の許可範囲から除外する。
- #37のrenderer recoveryはPresent失敗文言のdevice-lost判定に接続される。追加した独立GPU testはmanagerのlost teardown後にheadless再初期化できることだけを確認し、Present由来の自動recoveryやbackendの実device removalは検査しない。
- 失敗Projectのload時、未ロード状態の維持だけでなく既存の有効Project・Composition・Layerを保持することをProject round-trip suiteで確認するケースを追加。Project importerが成功後にだけcurrent projectを差し替える既存実装と対応付けた。
- 新規テストは既存コードの読み取りに基づき追加した。ビルド・テスト・CMakeは実行していない。

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
