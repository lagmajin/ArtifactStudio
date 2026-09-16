**最終更新:** 2026-09-16

## 2026-09-16 — Animation Layer Inspector評価とMSVC Modules ICE

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`（`getLayerPropertyGroups()`）、`ArtifactCore/include/Animation/AnimatableValue.ixx`、`ArtifactCore/include/Geometry/Interpolate.ixx`。
- **確認できた事実:** Property Group生成中の`AnimationLayerStackT<float>`値表示が`AnimatableValueT<float>::at()`を通じて汎用`interpolate<float>()`を実体化し、MSVC 14.51のIFC import環境でC1001を起こした。通常のInspector表示は評価値ではなく永続値で十分に編集可能である。
- **対応:** InspectorのAnimation Layer値を`current()`による永続値表示へ切り替え、補間評価はレンダー／専用アニメーション経路に限定した。
- **価値または懸念:** 巨大な`ArtifactAbstractLayer`を即時にABI分割せずに、該当テンプレート実体化をコンパイル単位から除去できる。フレーム評価値をProperty Inspectorへ再導入する場合は、専用状態モジュールまたは非テンプレート評価APIが必要。
- **確認:** `cmake --build out/build/x64-Debug --target Artifact --config Debug --parallel 4` が成功した。`Impl`内のAnimation Layer状態ラッパー化、およびUtilitiesモジュールへ新しい公開Animation Layer型を出す二案は、MSVC 14.51のIFC import環境でC1001を再発させたため採用していない。
- **次に確認:** 実機でAnimation Layerの値編集・保存・復元・レンダー評価が維持されること。物理分割を再開する場合は、`Impl`の共有内部表現を先に設計し、公開テンプレート型を新規IFC境界へ出さないこと。

## 2026-09-16 — Diligent版タイムライン上部バー操作負荷の最適化

- **関連:** ArtifactTimelineWidget.cppm（syncPlayheadOverlay、syncGpuTimelineSnapshot、uildGpuTimelineSnapshot、TimelineScrubFinishedEvent）。
- **確認できた事実:** ① syncPlayheadOverlay() において gpuTimelinePreviewEnabled_ を判定していなかったため、GPU描画モード中にもかかわらずシーク毎に Qt 側の TimelinePlayheadOverlayWidget が再有効化・二重描画されていた。② スクラブ・ナビゲータードラッグ操作による連続マウス移動ごとに syncGpuTimelineSnapshot() がキュー投入され、GUIスレッドでスナップショット生成が過剰実行されていた。③ uildGpuTimelineSnapshot() 内の staticHit 判定で !view->isInteracting() を要求していたため、上部バーのシーク・スクラブ操作時にも全トラック行・全グリッド・全クリップ・UniString 含む静的ジオメトリが全件再生成されていた。
- **対応:** ① syncPlayheadOverlay() で gpuTimelinePreviewEnabled_ を加味し、GPUモード時は Qt 側オーバーレイを無効化。② syncGpuTimelineSnapshot() に最小16ms（約60FPS相当）のタイマースロットリングを導入し、TimelineScrubFinishedEvent で最終フレームを確実に同期。③ uildGpuTimelineSnapshot() の staticHit 条件から !view->isInteracting() を外して静的キャッシュのヒット率を高め、プレイヘッド移動のみの再構築負荷を最小化。
- **価値または懸念:** スクラブ・シーク中の不要なオーバーレイ描画と大量のスナップショット再構築が消え、上部バー操作の追従性・フレームレートが大幅に改善される。
- **次に確認:** ビルド許可後、Diligentプレビュー時のスクラブバー・ナビゲーター操作時の軽快感、シーク終了時のプレイヘッド正確性、通常QWidgetモードへの切り替え時の動作整合性を確認すること。

## 2026-09-16 — スクラブ中VP draft＋timeline Present(0)

- **関連:** `ArtifactCompositionRenderController.cppm`（`TimelineSeekRequestedEvent` 購読→`notifyViewportInteractionActivity`）、`ArtifactDiligentTimelineRenderWindow.cppm`（`Present(0)`）。
- **確認できた事実:** VP 本流は controller 経路で、既存の interacting/draft 機構（effect 解像度0.25・CPU readback 回避・LOD）が viewport 操作時だけ有効だった。timeline スクラブは seek を publish するが interacting に入らないためフル描画だった。また Diligent の `Present()` 既定は vsync 有効で、timeline の Present が GUI スレッドを止めていた。VP worker 経路（`CompositionRenderWidget`）は `AppMain` で生成されておらず、スクラブ時の描画本流は controller 側。
- **対応:** 全スクラブ身振り（scrub bar／overlay／track view）が publish する seek を interacting へ流し、120ms 無 seek で自動復帰（既存 tick 処理を再利用、bracket 不要）。timeline 面は `Present(0)` で GUI ブロックを除去（tearing 許容、VP は vsync 維持）。
- **価値または懸念:** スクラブ中の VP 1描画あたりが軽くなり、timeline 操作の応答が上がる。tearing が目立つ場合は要調整。ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。
- **次に確認:** スクラブ中の描画負荷・tearing の見え方、120ms 復帰の体感、release 直後のフル画質復帰を確認すること。

## 2026-09-16 — Timeline GPU面のPresent間引き（30Hz pacer）

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`（`Impl::render`、`kMinPresentInterval`）。
- **確認できた事実:** 再生・スクラブ中は snapshot 毎に submit＋Present が走り最大60Hz。静動分離で CPU 再構築は軽くなったが GPU 投入は毎 tick 残っていた。
- **対応:** `render()` 入口で前回 Present から33ms未満なら描画を捨て、残り時間後に1回だけ wakeup して最新 snapshot を描く。snapshot 差し替え自体は継続されるため中間フレームは自然に間引かれる。タイマーは window をコンテキストにし、破棄時は自動キャンセル。resize/expose 直後も最大33ms遅延する。
- **価値または懸念:** 独立 device 上の submit 量が半減し、VP との GPU 奪い合いが減る。単発シークの表示遅延は最大33ms。ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。
- **次に確認:** 再生・スクラブ中の滑らかさと遅延感、30Hz で不足なら間隔調整、Qt fallback 面には影響なし（同クラスだが pacer は GPU render 経路のみ）。

## 2026-09-16 — GPU timeline snapshot の静動分離（static cache＋dynamic tail）

- **関連:** `ArtifactTimelineTrackPainterView`（`timelineVisualRevision`／`isInteracting`／`touchTimelineVisuals`）、`ArtifactTimelineWidget::buildGpuTimelineSnapshot`（`gpuTimelineStatic_`）。
- **確認できた事実:** 従来は再生 tick 毎（16ms）に snapshot 全体（行・グリッド・全クリップ・UniString タイトル・波形64本・マーカー）を作り直していた。text クリップのタイトル等は tick 毎に変わらない（`refreshTracks` 時のみ更新）ため static 化しても描画は一致する。
- **対応:** 行・グリッド・クリップ・comp/marker を `(ppf, h/vOff, viewport, revision)` キーでキャッシュし、再生 tick は frame 依存のマーカー強調＋playhead のみ再構築（QVector 暗黙共有で static 部は無コピー）。revision は View 内の全 visual 変更点＋`mouseReleaseEvent` で bump、操作中（drag/marquee/scrub/pan）は `isInteracting()` で毎回再構築。`audioMuted` のみ snapshot に効くためそこだけ個別 bump（rate/pan/gain/reverse は snapshot 非消費）。
- **価値または懸念:** tick 毎の O(n) 再構築と UniString 生成が消える。GPU 全描画＋Present 自体は残る（次の Present 間引き対象）。atCurrentFrame 強調は dynamic へ移動し画素等価を維持。
- **次に確認:** ビルド・`check_module_hygiene`・実機で再生中の表示一致と負荷減を確認すること。**注意：本ツリーでは別作業の未コミット変更が同一関数に重なっている**（色・グリッド密度等）。コミット時は分離し、本キャッシュのキー／bump 点を壊さないこと。

## 2026-09-16 — Timeline GPU面を独立D3D12 deviceへ分離（shared immediate競合の解消）

- **関連:** `Artifact/src/Render/DiligentDeviceManager.cppm`（`createIndependentRenderDevice`、`createSwapChainForIndependentDevice`）、`Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`（`Impl::initialize`）。
- **確認できた事実:** shared device は device＋immediate各1個のプロセス共用で、submit 時の同期が acquire/release の mutex だけだった。VP worker と timeline GUI が同一 immediate へ並行 submit していた（viewport/RTV ステート踏みの可能性あり）。
- **対応:** DeviceManager に独立払い出し（共有なし・失敗時フォールバックなし・契約コメント付き）を追加し、timeline 初期化を独立 device へ切替。失敗時は従来どおり Qt painter へ戻る（`setGpuTimelinePreviewEnabled` の既存処理）。curve 面は同クラスなので自動で独立する（＝timeline＋curve＋VP で最大3 device）。
- **価値または懸念:** context 競合のクラスごと消える。代償に VRAM・PSO・シェーダコンパイルが device 数分、D3D12 trim 通知は shared のみ（独立分は未登録・未検証）、起動時間増の可能性。ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。
- **次に確認:** ビルド・`check_module_hygiene`、timeline/curve GPU 初期化・表示、VP と同時操作時の停滞解消、複数 device 時の VRAM・起動時間、trim 登録の要否。

## 2026-09-13 — 任意Mesh Shader PSO失敗を3Dレイヤー生成から隔離する

- **関連:** `ArtifactCore/src/Graphics/MeshRenderer.cppm`（`createPSO`、Meshlet LOD Mesh Shader PSO）。
- **確認できた事実:** D3D12では通常のindexed mesh PSOが成功した後、任意のmeshlet PSO生成だけが失敗していた。従来はデバイスがMesh Shaderを広告すれば未検証の任意PSOを常に作り、失敗後にも汎用の「PSO created successfully」を出すため、3Dレイヤーの必須経路と誤認しやすかった。
- **対応:** Mesh Shader経路を`ARTIFACT_ENABLE_EXPERIMENTAL_MESH_SHADERS=1`の明示opt-inへ戻し、PSO例外時は参照を破棄して通常のindexed GPU描画へfail-softする。成功ログもindexed PSOを明示する。可変light loop内のGoboサンプルはimplicit gradientを使わない`SampleLevel(..., 0)`へ変更した。
- **価値または懸念:** 3Dレイヤー作成は実績のあるDiligent indexed GPU経路を使い続け、任意高速化のPSO拒否で落ちない。報告されたheap破損の検出地点はQt repaintであり、同じ操作で再現しないことを実機確認するまで破損元を断定しない。
- **次に確認すること:** 通常環境で3Dレイヤー作成・描画・終了時解放を再確認し、mesh shaderはD3D12／Vulkan個別のPSO validationを通した後にopt-inで再検証する。

## 2026-09-13 — Composition Viewerの3Dグリッド面はXYへ合わせる

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`drawThreeDimensionalGroundGrid`）、`ArtifactCore/include/Grid/ArtifactGridSystem.ixx`（`computeGroundGridLines`）。
- **確認できた事実:** 共通`GridSystem`のground gridは3Dシーン用のXZ面を生成する。Composition Viewerの平面レイヤー／コンポジションはXY面にあるため、FrontではXZグリッドがedge-onになり、赤いX軸だけがコンポジションを横切る線として見えていた。
- **対応:** Coreの`GroundGridSettings`へ既定XZを維持する`GridPlane3D`（XY／XZ／YZ）を追加し、共通生成器が指定平面の座標を直接返すようにした。Composition ViewerはXYを指定し、提出側の座標読み替えを撤去した。軸もX/Yの2本へ揃えた。
- **価値または懸念:** Frontではコンポジションと平行な編集グリッドになり、斜視ではコンポジション面と同じ傾きを持つ。グリッドは既存どおりコンポジション背景より先に描くため、面の上へ重ならない。
- **次に確認すること:** ビルド許可後、Front／Top／Right／Perspectiveで面の向き、コンポジション外の表示、D3D12／Vulkanの線描画を確認する。

## 2026-09-13 — Unity Orientation overlayとTimeline GPU面の視覚責務

- **関連:** `Artifact/src/Widgets/Render/ArtifactViewOrientationWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelinePresentation.cppm`。
- **確認できた事実:** Unity公式のOrientation overlayは中央キューブ、円錐状のXYZ軸アーム、キューブ下のPersp/Iso表示を基本形とする。既存Simple表示はpresentation切替バッジと面内`Persp`が支配的で、回転後も軸クリック判定だけ固定位置だった。Timelineは環境変数未設定時にDiligent面が既定だが、GPU面のclearがcomposition canvasにも使う汎用背景色だった。
- **対応:** Simple表示を円錐軸、面ラベルと投影ラベルの分離、低コントラストのstyle切替へ寄せ、クリック判定も表示中のorientationから投影する。Timeline GPU面はsecondary background由来のcharcoalへ変更し、採用モックどおりcache/work areaを上、navigatorを下へ置いた。
- **価値または懸念:** ViewCubeの向き表示とクリック対象が一致し、Timelineの空状態でも明るいcanvasに見えない。投影切替そのものは既存のorientation snap契約を維持しており、Unity完全互換のcenter-click projection toggleは未実装。
- **次に確認すること:** 実機でSimpleの各軸click／drag／Front表示、timeline GPU初期化成功時の暗色面・下部navigator、Qt fallbackとの操作同等性を確認する。

## 2026-09-13 — VP Cryptomatte表示は生のfloat payloadを保持する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`composeViewportChannelOverlayImage`、Object ID / Material ID AOV）、`Artifact/src/Render/ArtifactIRenderer.cppm`（通常画像readback）。
- **確認できた事実:** Object ID / Material ID は `uint32_t` を float bit pattern として格納するCryptomatte形式だが、従来のVP表示は通常の`readbackChannelToImage()`経由で0〜1へclampして8-bit化していた。このためIDが失われ、擬似色がほぼ単色になった。加えてGPU ID passは現時点で`layer->is3D()`だけを描画対象にしている。
- **対応:** VP表示だけは`readbackToMultiChannelImage()`から生float payloadを取得し、`floatToId()`後のhashで安定した識別色を作るようにした。クリック選択も、host全域ではなく表示画像のaspect-fit矩形からAOV座標へ変換する。export・クリック選択の生AOV値は変更しない。
- **価値または懸念:** 同一IDは常に同色、背景ID 0は黒で表示される。これは表示の復旧であり、2D/Textのalpha形状へIDを書く機能はまだ未実装のため、2Dレイヤーだけのシーンで正しいマットになる保証はない。
- **次に確認:** ビルド許可後、複数3Dレイヤーで色が分かれること、Object ID上のクリック選択、2Dテキストへalpha付きID passを追加するための既存2D draw/mask経路を確認すること。

## 2026-09-13 — RGB成分表示はfinalize後のdisplay surfaceを保持する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`finalizeGpuRenderToViewport`、AOV source公開、`drawViewportChannelOverlayImage`）。
- **確認できた事実:** Red/Green/Blue/Alphaは`finalizeGpuRenderToViewport()`でcompute変換済みの`viewportChannelDisplaySRV_`を作るが、同フレーム後半のAOV source公開処理が無条件にnullへ戻していた。結果としてGreenなどは表示用surfaceでなく、途中までpresentされたBeautyのreadbackへフォールバックし、VP内に再帰的な見た目が出た。
- **対応:** 表示surfaceはフレーム開始時にのみclearし、finalizeが作ったsurfaceがある場合は後段で保持する。Emission等の専用AOVはsurface未設定時だけ既存のsource公開を行う。
- **次に確認:** ビルド許可後、RGB／Alphaの単成分表示がグレースケールで安定し、pan／zoom／Perspectiveの表示状態に依存してBeautyやグリッドが混入しないことを確認する。

## 2026-09-12 — テキストレイヤーの移動・リサイズ不能（2D TextGizmo の未バインド）

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`drawViewportOverlayPass` のギズモ選択、`handleMousePress`）、`Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`。
- **確認できた事実:** `viewportOrientationActive_` は宣言時 `true` で、以後どこでも `false` に代入されない（grep で全書き込みが `= true`）。そのため `use2DTransformGizmo = !viewportOrientationActive_ && layerUsesTextGizmo(...)` は常に false になり、テキストレイヤーの 2D TextGizmo は描画・バインドされず `sync2DGizmosForLayer(nullptr)` で解除される。一方で 3D ギズモは `showProjected3DGizmo = true` で全レイヤーに描画されるが、そのヒットテストは `!layerUsesTextGizmo` でテキストを除外しているため、テキストは「フレームは見えるが移動・リサイズ不能」になっていた。
- **対応:** ① `use2DTransformGizmo = layerUsesTextGizmo(selectedLayer)` に変更（テキストは常に 2D TextGizmo を使う）。② `showProjected3DGizmo = !layerUsesTextGizmo(selectedLayer)` に変更し、テキストへの 3D ギズモ二重描画を回避。③ テキストギズモの `handleMousePress` 前に `setLayer(gizmoLayer)` を追加（contentGizmo と同様の防御）。
- **価値または懸念:** `viewportOrientationActive_` が常に true のため、コード内の `!viewportOrientationActive_` ガード（`layerUsesProjectedFrameGizmo && !viewportOrientationActive_` 等）は複数箇所で実質デッドコード化しており、「3D 統一フレームギズモ」への移行が未完の状態。また `use2DTransformGizmo` ブロック内の contentGizmo（シェイプ等のコンテンツ編集）も同様に遮断されたままで、これは別の潜在バグ。
- **次に確認:** ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。テキストレイヤー選択時の移動（Offset）とボックス角・辺ハンドルのリサイズ、回転・アンカーのドラッグを確認すること。シェイプのコンテンツ編集モードの動作も確認（本修正では対象外）。

## 2026-09-12 — グリッド描画のビューポート全域化とcanvasSize欠落の修正

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`(`drawViewportCanvasOverlay`)、`Artifact/src/Render/PrimitiveRenderer2D.cppm`(`drawGrid`)、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`(`submitGrid`)、`Artifact/include/Render/ViewerHelperShaders.ixx`(`g_gridPS`)。
- **確認できた事実:** 2D矩形グリッドは `drawGrid(0,0,cw,ch,…)` とコンポ寸法でquadを切っており、ズームアウト時にコンポ塗りつぶし領域にしか出なかった。加えて `submitGrid` が `helper._pad`(=シェーダーの `canvasSize`) を `{0,0}` で上書きしていたため、グリッドは線でなく塗りつぶしで描画されていた。また `ArtifactCore` の `Artifact::Grid::GridSystem`(`computeVisibleLines`) は `visibleCanvasRect` 基準でビューポート全域グリッドを正しく計算できるが、コントローラは独自のインライン描画(`gridPolarMode_`/`gridIsometricMode_`/autoStep)で `GridSettings` を別経路に再実装しており重複している。
- **対応:** `drawGrid` に grid quad のcanvas起点を `color2` で渡し、シェーダーは `gridOrigin.xy + uv*canvasSize` でコンポ原点基準に再構築、負座標対応の対称距離判定へ変更。`submitGrid` は `canvasSize`/`gridOrigin` を正しく転送。`drawViewportCanvasOverlay` は pan/zoom から可視ビューポート矩形を計算して矩形グリッド・原点軸・数値ラベルをビューポート全域に拡張、太さは `thickness/zoom`(canvas単位)へ正規化。
- **価値または懸念:** `GridSettings`(`Artifact::Grid::GridSettings`) を唯一の設定源として維持し、新規設定や並行機構は追加していない。長期的にはコントローラのインライン実装を `GridSystem`/`GridLayer` へ寄せる余地がある(重複解消)。polar/isometric は依然コンポ中心基準でビューポート全域化は未実施。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。ズームアウトでビューポート全体に線が出ること、パンで線がコンポ原点に固定されること、グリッドが線として描画されることを確認すること。

## 2026-09-12 — バインドなし固定キーの残存棚卸しと最小4点の局所化

- **関連:** `ArtifactCore/.../ShortcutBindings`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`。
- **確認できた事実:** `matches()` は `ArtifactPr` のみ使用で `Artifact` 本流の `keyPressEvent` 約365件・`new QShortcut` 約54件は固定のまま。今回分は既存ID流用とコンテキスト縮小のみで新規ID・新規接続なし。`immersiveExit Esc` に `WidgetWithChildrenShortcut` 付与、`clearSearch Esc` を `WidgetShortcut` 化、`複製` を `LayerDuplicate(Ctrl+D)`、`Edit Text` を `LayerRename(F2)` へ統一(既定値同一)。第2弾で `Timeline` 検索2件に `WidgetWithChildrenShortcut`、ツール `V/H/Z/R/S` を `Timeline{Selection,Hand,Zoom,Rotate,Slide}Tool` へ統一、`WorkCursorPlace/Center/Clear`+`ViewUndo/Redo` 5件に `WidgetWithChildrenShortcut` 付与。
- **第3弾(①〜④):** 新ID `ProjectClearSearch`/`TimelineFocusSearch`/`TimelineClearSearch`/`CompositionImmersiveExit`(既定Esc/Esc/Find/Esc、表示名・永続化キー・設定画面コンテキスト付き)を追加。変更通知はQtシグナルではなく `addChangeListener`/`removeChangeListener`+`revision()` の軽量オブザーバで実装(AGENTS.mdの新規シグナルスロット禁止に配慮、単一GUIスレッド前提)。4ウィジェット(Timeline/Project/CompositionEditor/EditMenu)に `updateShortcuts()` を設け、作成時のコピーを通知駆動で再適用。`EditMenu::rebuildMenu()` から `setShortcut` を除去し、有効/文言のみに分離。
- **価値または懸念:** Esc/Find横取りと `Ctrl+D`/`F2` 二重定義の競合表示を縮小。設定変更が再起動なしで4面へ反映。残存は `CompositionRenderWidget` モーダルEsc群、`LayerPanelWidget`、`ContentsViewer J/K/L`、`File/Animation/EffectMenu` 固定群と比較系QShortcut群(新規IDが必要)。`loadFromJson` はID毎に通知が飛ぶため一括適用時は複数回更新が走る(安価だが将来集約可)。
- **次に確認:** ビルド・`check_module_hygiene`・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。設定変更→4面即時反映、空バインド無効化、同一コンテキスト競合検出を確認すること。

## 2026-09-11 — Clone生成数の上限(巨大グリッドのハング防止)

- **関連:** `Artifact/src/Layer/ArtifactCloneLayer.cppm`(`generateCloneData`)。
- **確認できた事実:** clone数・grid各次元・radial数は下限1のみで上限なし。Gridは`cols*rows*depth`がint64オーバーフローし得て、毎drawの生成でハングする。描画側は4096でclamp済みだが生成側が無制限だった。
- **対応:** `kMaxGeneratedClones=4096`を全モードへ適用。Gridはint64で積算しreserveはcap、3重loopはprefix順でbreak。既定小規模の見た目は不変。
- **価値または懸念:** 上限超過は黙って切捨て(描画clampと同一方針)。UIへの上限表示は未追加。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。大grid指定時の打切りを確認すること。

## 2026-09-11 — カメラのクリックフォーカス(Alt+ダブルクリック)

- **関連:** `ArtifactCompositionRenderController.ixx/.cppm`(`focusActiveCameraAtViewportPos`)、`ArtifactCompositionEditor.cppm`(`mouseDoubleClickEvent`+操作ヒント)。
- **確認できた事実:** 3Dレイ・三角ヒット距離(`intersectModelLayerPickingRay`)とactive camera解決が既存。フォーカス面ギズモ(`ArtifactCameraLayer.cppm:179-182`)・DOF/MB実配線も済みで、欠落はヒット点→フォーカス距離の導線だけだった。
- **対応:** ヒット点をactive camera前方軸へ投影した真のフォーカス面距離を near/far でclampし、既存property path(`Camera Options/Focus Distance`)経由で設定。Alt+単クリック=オービット・素ダブルクリック=既存動作は不変。新規signalなし。
- **価値または懸念:** depth readback不要でview非依存。2D層・空振り・非正ヒットは無操作。skinning変形後の頂点には未対応(静止mesh基準)。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。Alt+ダブルクリックでのフォーカス変化とHUD表示を確認すること。

## 2026-09-11 — 3Dマテリアル環境強度(IBL間接光の個別スケール)

- **関連:** `ArtifactCore/.../Material`、`MeshRenderer`(`MaterialConstants` 32→36 floats、`EnvFactors`、PS乗算)、`ArtifactIRenderer::drawMesh`、`Artifact3DLayer`(JSON/property/signature)。
- **確認できた事実:** 環境強度はグローバル(`EnvironmentSettings.y`)のみで、マテリアル別の反射調整(E3Dの常用操作)がなかった。
- **対応:** `environmentIntensity_` (0〜4、既定1)を追加し、間接diffuse/specular/transmission合算へ乗算。ベースambient・直接光は不変。既定値1で既存見た目不変。
- **価値または懸念:** Look-devの基本操作を低コストで補完。環境なし時は効果なし。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。0/1/2での間接光変化と保存再読込を確認すること。

## 2026-09-11 — 3Dアニメ再生モード/速度(Loop固定の拡張)

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` (Impl/draw/toJson/fromJson/property)。
- **確認できた事実:** draw時のclip評価は`fmod`固定Loopで、Holdや速度調整がなかった。毎フレーム`loadFromFileAtTime`再importは既存仕様のため対象外。
- **対応:** `animationPlaybackMode_` (0 Loop/1 Hold/2 PingPongの無状態三角波)+`animationSpeed_` (0〜8、0は静止)を追加し、評価・JSON・property・setterへ接続。変更時は`lastSkinAnimationFrame_`を無効化して同フレームでも再評価。
- **価値または懸念:** E3D式の再生制御の最小形。PingPong・逆再生・Bakeは対象外。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。Loop/Hold/速度0・2倍と保存再読込を確認すること。

## 2026-09-11 — Cloner 3D最小接続(Phase2→描画の接続)

- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`、`Artifact/src/Render/ArtifactIRenderer.cppm`(`drawMesh`/`drawMeshInstanced`)、`ArtifactCore/.../MeshRenderer`(`maxInstances`)、`Artifact/include/Layer/Artifact3DModelLayer.ixx`(`material()`追加)、`Artifact/src/Layer/ArtifactCloneLayer.cppm`(`drawInstancedSource`)。
- **確認できた事実:** `getInstanceData()`は呼出し元ゼロ、`CloneLayer::draw()`は2D矩形のみでソース内容を見ない。`MeshRenderer::draw(ctx,count)`はN instance対応済みだが`initialize(1,...)`でinstance bufferが1固定だった。
- **対応:** `Impl::drawMesh`へinstance配列引数を追加し、容量不足時は`initialize(wanted,...)`+再upload、ray登録・mesh-shader LODは単一時のみ、ID pass時のみ複写。Clone側は`sourceLayerId→layerById→Artifact3DLayer`解決(`Procedural3D`と同型)、clone行列×globalのworld化+層opacity bake、`clone3d|src|rev`キーで`drawMeshInstanced`。Phase2変換器は転置なし複写のためGPU提出に再利用せず新helperで置換(旧関数は残す)。
- **価値または懸念:** Grid/Random/Linear等の既存配置・effectorが3Dメッシュにそのまま適用、頂点色/UV変換/影は同一経路で効く。4096上限、source可視時の二重描画は仕様未定、debug shading mode・rayは単一のみ。
- **次に確認:** ビルド・`check_module_hygiene`・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。3Dソース指定時の散布・影・保存再読込を確認すること。

## 2026-09-11 — 3D読込時PBR係数(metallic/roughness factor)の適用

- **関連:** `ArtifactCore/include/Geometry/MeshImporter.ixx`、`ArtifactCore/src/Geometry/MeshImporter.cppm`(`detectTexturesFromUfbx`)、`Artifact/src/Layer/Artifact3DModelLayer.cppm`(`loadFromFile`)。
- **確認できた事実:** 読込はテクスチャパスのみ採取し、glTF/FBXのmetallic/roughnessスカラー係数を捨てていた。`ufbx_material_map`は`has_value`/`value_real`を持つため未指定と既定値1.0を区別できる。base-color係数はsRGB/linear曖昧のため対象外。
- **対応:** 先勝ちで係数採取(`hasLastMetallicFactor`/`lastMetallicFactor`等、ufbxパスのみ有効)。層側はufbx系backendかつマテリアルが既定値(metallic 0.0/roughness 0.5)の場合のみ適用し、ユーザー編集の上書きとtimed再評価経路への波及なし。
- **価値または懸念:** 金属質glTFが誘電体表示になる誤りを解消。色係数は色空間メタ欠落(`FloatColor`素float4)のため別途要設計。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。metallic glTFでの質感と既存JSON再読込を確認すること。

## 2026-09-11 — 3D層Transformの3軸露出(Core済み・UI未整理の接続)

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`(`getLayerPropertyGroups`、`transformChannelProperty`)。Core `AnimatableTransform3D`はX/Y/Z・保存・行列・ギズモ済み。
- **確認できた事実:** 共有Transformグループは2D subset(posX/Y・scaleX/Y・単一rotation・anchorX/Y)のみで、`transform.rotation.x/y`等のchannel pathは解決できるのにUIに露出していなかった。`transform.rotation.z`のpath自体が未定義だった(Rotation=Z互換の別名なし)。
- **対応:** `transform.rotation.z`→Rotation channelの別名を追加。`is3D()`時のみ同TransformグループへPosition Z・Rotation X・Rotation Y・Rotation Z(alias)・Scale Z・Anchor Zを追加し、既存Rotation表示を「Rotation Z」へ(3Dのみ)。新規グループ・signalなし。
- **価値または懸念:** 3軸編集・キーフレーム・式解決が既存Property経路で通る。2D層の表示は不変。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。3D層での表示・編集・保存再読込を確認すること。

## 2026-09-11 — 3Dソフトシャドウ仕上げ(UI飽和の解消)

- **関連:** `Artifact/src/Layer/ArtifactLightLayer.cppm`。受渡しは`ArtifactCompositionRenderController.cppm:4945-4946`(radius/10→softness)、`ArtifactIRenderer.cppm:1098-1106`、`MeshRenderer.cppm:892-903,3143-3160`で接続済み。
- **確認できた事実:** UI hard 0〜500・soft 0〜200に対し、MeshRendererはsoftness 0〜2へclampするため、radius 20超は描画不変だった。`setShadowRadius()`にfinite/clampがなく、fromJsonも無制限だった。
- **対応:** 格納を0〜20へclamp(既定10)、UI hard 0〜20・soft 0〜10へ整合、tooltip/whatsthisを「20 = softest」へ修正。fromJsonとproperty setterは同setter経由で自動整合。
- **価値または懸念:** 単一caster・Directional/Spotのみ・CSM/Point cubeなしの範囲は不変(コントローラ側に明記)。見た目の既定値は不変。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。radius 0/10/20の影 edge を確認すること。

## 2026-09-11 — 3Dテクスチャトランスフォーム共有UV(offset/scale/rotation)

- **関連:** `ArtifactCore/include/Material/Material.ixx`、`ArtifactCore/src/Material/Material.cppm`、`ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- **確認できた事実:** テクスチャトランスフォームは存在せず、glTF `KHR_texture_transform`相当も未対応。全テクスチャが`In.UV`直サンプリングだった。
- **対応:** Materialへ共有UV `offsetU/V(-10〜10)`・`scaleU/V(0.01〜10)`・`rotation度(-360〜360)`を追加。`MaterialConstants`へ`uvTransformA/B` 8 floats追加(24→32 floats、static_assert更新)。PSは`transformMeshUv()`でscale→UV中心回転→offsetを適用し、全6テクスチャと法線マップ接線フレームへ反映。`ArtifactIRenderer::drawMesh()`で受渡し、3D層のJSON/property/signatureへ接続。既定値(0,0,1,1,0)は恒等で既存見た目不変。
- **価値または懸念:** E3D/他DCCの質感調整の基本操作を低コストで補完。per-texture個別変換・glTF import時自動反映は対象外。
- **次に確認:** ビルド・`check_module_hygiene`・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。回転中心・法線接線・旧JSON既定値を確認すること。

## 2026-09-11 — 3D頂点カラー描画反映(MeshImporter保持→Mesh描画断絶の接続)

- **関連:** `ArtifactCore/include/Mesh/Mesh.ixx`、`ArtifactCore/src/Mesh/Mesh.cppm`、`ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`。メモ:`docs/analysis/THREED_AE_E3D_C4D_GAP_IMPROVEMENT_2026-09-11.md`。
- **確認できた事実:** `MeshImporter.cppm:880-884`はufbx頂点カラーを`color`属性へ保持するが、`Mesh::generateRenderData()`はcolorsを持たず、`MeshRenderer::updateMeshGeometry()`も位置/法線/UVのみで頂点カラーバッファ・シェーダ入力がなかった。Wicked系`surfaceHF/objectHF`の頂点色対応とは別系統のMesh PBR経路が対象。
- **対応:** `RenderData`へ`colors`追加、生成時に`color`属性から展開(欠落時白)、meshlet remap用`PackedVertex`へcolorを含めて異色頂点の統合を防止。`MeshRenderer`へ`pColorBuffer_`追加、VS `ATTRIB3`/PS `TEXCOORD7`で受渡し、baseColorへ`vertexColor(rgb linear, a)`乗算。欠落時は白で既存見た目不変。`ArtifactIRenderer::drawMesh()`でcolors構築・hash・upload。
- **価値または懸念:** glTF/PLY等の頂点色付き資産がそのままPBR表示される。確保は形状キャッシュ更新時のコールドパスのみ。ホットパスはバインド済みバッファ参照のみ。
- **次に確認:** ビルド・`check_module_hygiene`・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。頂点色付きglTF/PLYでの色反映、白資産の不変、D3D12/VulkanのATTRIB3レイアウトを確認すること。

## 2026-09-10 — シェイプレイヤーの画像エフェクト適用時のサーフェスキャッシュ統合と最適化

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`ArtifactShapeLayer.cppm`。
- **確認できた事実:** シェイプレイヤー（`ArtifactShapeLayer`）に画像エフェクト（`EffectPipelineStage::Rasterizer`）やマスクが適用された際、`drawLayerForCompositionView` 内に ShapeLayer の専用分岐がなくフォールバック描画（`layer->draw(renderer)`）に落ちていた。また、エフェクト適用時のラスタライズ結果をキャッシュするキー（`buildLayerSurfaceCacheKey`）にシェイプの形状・アニメーション判定が含まれておらず、毎フレームCPUでのフルラスタライズおよびGPU再アップロードが走って激重となっていた。
- **対応:** `buildLayerSurfaceCacheKey` に ShapeLayer の幅・高さ・タイプおよびアニメーションプロパティ（パスキーフレーム等）判定を追加し、静止時はキャッシュヒットするように改善。さらに `drawLayerForCompositionView`（コントローラ側およびビューポート描画側）に `dynamic_cast<ArtifactShapeLayer*>` 分岐を新設し、エフェクトまたはマスクが存在する場合にのみ `toQImage()` / `downsampleForLOD` を経由して `applySurfaceAndDraw`（サーフェスキャッシュと低解像度LODスケール）を通すように接続した。エフェクトなし時は従来の高速GPUベクターダイレクト描画を維持。
- **価値または懸念:** プレビュー時の解像度スケーリング（ドラフト時1/4など）や静止フレームでのサーフェス再利用が効くようになり、大幅なプレビュー軽量化を実現。将来的な完全オフスクリーンFBOレンダリング（GPU上でのラスタライズ＆コンピュートエフェクト結合）へのステップとなる。
- **次に確認:** ビルド不可環境ルールに基づきコンパイル・手動確認の要否をユーザーと連携。エフェクト適用時のプレビュー滑らかさおよびアニメーション更新時のキャッシュ破棄を確認すること。
- **2026-09-11 追記・確認事実:** 対応済みのRasterizer effect列については、Shape分岐が `shapeLayer->draw(renderer)` を再利用可能な layer RTV へ直接出力し、F32 GPU texture上で処理できる。既存の個別GPU effectは入力upload／staging readback／`WaitForIdle()` を含むものがあり、GPU実装であってもGPU常駐とは限らない。
- **対応:** `ArtifactAbstractEffect` に具体型非依存の `GpuRasterEffectDomain`／固定容量 `GpuSpatialEffectNode` を設け、pointwiseとspatialを順序どおり実行するGPU planへ接続した。Gaussian Blur、Sharpen、Vignette、Chromatic Aberration、Stripes、Hex Grid をDiligent共通のSRV/UAV compute passへ移し、対応ShapeではCPU画像境界を通さない。
- **懸念・次に確認:** LayerMask は `LayerMask::applyToImage()` のOpenCV実装だけで、Bezier path、feather、invert、各modeのGPU契約は未確立。マスクありを無理に部分GPU化せず、mask raster／alpha-composite passを仕様化してからGPU化する。対応エフェクトのCPU/GPU pixel parity、アニメーション時のframe time、D3D12/Vulkan両backendでのshader compilationをビルド後に確認する。

## 2026-09-10 — Shape F6 open/closed・smooth/corner-bezier メインVP移植

- **関連:** `Artifact/src/Widgets/LayerEditorGeometry.cppm`（新`togglePathVertexSmooth`）・`LayerEditorContextMenu.cppm`（Solo TogglePathSmooth）・`ArtifactCompositionRenderController.ixx/.cppm`（hovered頂点5メソッド）・`ArtifactCompositionEditor.cppm`（右クリックShapeメニュー）。Artifactリポ内のみ、Core不変・新規ファイルなし・新規シグナルなし。
- **確認できた事実:** Solo側はOpen/Close・Smooth切替＋Undoが既存だがsmooth反転はフラグのみでtangent初期化なしだった（handle非表示のまま）。メインVP右クリックはmask分岐のみでshape分岐なし。`evaluatePathAt`はtangentを直接cubic評価するため、tangent初期化が描画・補間に直結する。`contextMenuEvent`は先頭で`handleMouseMove`済みのためhoverは新鮮。
- **対応:** Geometry共有ヘルパー追加（smooth化は隣接弦方向へ±ハンドル初期化・長さは隣接距離25%を4〜64pxにクランプ・既存非ゼロハンドルは保持、corner化は両ハンドル破棄、開パス端点は単一隣接方向）。Solo切替をヘルパーへ寄せ。メインVPは`hasHoveredShapePathVertex`/`hoveredShapePathVertexSmooth`/`isSelectedShapePathClosed`/`toggleHoveredShapePathClosed`/`toggleHoveredShapePathSmooth`を追加し、既存`ShapePathVertexEditCommand`＋delete系と同一ガード（pending作成中は無効・lock・drag中・3頂点未満のclose抑止）・同一Undo末尾処理で接続。右クリックはmask分岐踏襲のQMenu（Make Smooth/Corner＋Open/Close Path）で確定。
- **価値または懸念:** marquee・multi-move・proportional・handle-only選択はF12残件として対象外。smooth化のハンドル長は固定ヒューリスティック（ズーム非依存・local px）。
- **次に確認:** ビルド・`check_module_hygiene`・実機runtimeは未実施（AGENTS.md制約でユーザー許可待ち）。特に`.cppm`追加import（Geometry）のdyndep、右クリック時のhover更新、`contextMenuEvent`のShape/menuフォールバック順、旧JSON再読込（ix/iy/ox/oy/smoothキーは既存のため互換のはず）を確認すること。

## 2026-09-10 — Shape Core縦断（WavePaths新設・Repeater複合順・SVG多段化）

- **関連:** `ArtifactCore/.../ShapeOperator.ixx`、`AeOperators.ixx`、`Repeater.ixx`、`ShapeTypes.ixx`、`ShapeGroup.cppm`、`ShapeLayer.cppm`、`ArtifactShapeLayer.cppm`、`LayerEditorContextMenu.*`、`ArtifactCompositionRenderOverlay.cppm`、`ArtifactCompositionRenderController.cppm`。子リポ編集はユーザー許可済み。
- **確認できた事実:** Wave operatorはCoreに不存在、Repeater複合順フィールドも不存在、SVG出力は2-stop固定だった。Trim同時/個別・Repeater本体は実装済み。親子ともoperator type分岐はdefault付きで新enum値に安全だった。
- **対応:** Coreに`WavePaths`（振幅・周波数・位相、弧長一様サイン変位、clone/JSON/process、`ShapeGroup` factory含む）、Repeater `compositeBelow`（clone/JSON/process末尾反転）、`FillSettings::gradientStops`+SVG `<stop>`列出力（空=従来2-stop、キャッシュキーにstops混入）を追加。親側はcreate/name/value読取/property群/setter/正規化/KF検出（`phase`/`composite`含む）/時刻評価/Solo追加メニュー/HUD表示・詳細/VPダイヤ量編集/`toCoreShapeLayer` stops受渡を接続。
- **価値または懸念:** 新規`.ixx`なし・CMake変更なし・旧ファイルは未知type/欠落キーを無視して読める。Wave processは~4px細分（上限128/seg）で滑らかさを確保。
- **次に確認:** ビルド不可PCのため未検証。特にCore側`W_OBJECT_IMPL(WavePaths)`、Q_PROPERTY NOTIFY配線、SVGの`<stop>`列とキャッシュキー、旧版での新type=int 11読飛ばしを確認すること。

## 2026-09-10 — Audio Mini をモーショングラフィックス用時間ナビゲーターとして分離

- **関連:** `ArtifactAudioWaveform`、`AudioSyncTools::detectBeats()`、`ArtifactTimelineWidget`、`ArtifactTimelineTrackPainterView`、composition marker / keyframe snapshot Undo。
- **確認できた事実:** 音声レイヤーの peak/RMS 波形キャッシュとタイムライン描画は既存の正規経路である。`AudioSyncTools` はサンプル位置の beat 検出と tempo 推定を持ち、Keyframe Pattern には手入力 BPM の Beat Sync がある。一方、検出 beat / transient / section を composition 時刻・marker・snap target・選択キー操作へ結ぶ編集契約と常設の全体波形 UI は未実装。
- **対応（2026-09-10）:** `ArtifactAudioMiniWidget` を通常 Timeline から独立した dock として追加した。最初に見つかる loaded audio layer の全体 waveform と、peak envelope のローカル最大値から得る transient cue を composition frame へ正規化して表示する。クリックで seek、`B` / `Shift+B` で次／前 cue へ移動できる。`ArtifactAnimationTimelineWidget` も別 dock とし、選択レイヤーの keyframe 時間域を `ENTER` / `ANIMATE` / `EXIT` の読み取り用 semantic span として表示・クリック seek できる。
- **価値または懸念:** main timeline に巨大な audio lane を常設せず、motion の時間合わせを速くできる。複数 audio layer 時の guide source 選択、tempo half/double の信頼度、section 推定の誤認、trim / slip / time-remap 後の sample→composition frame 対応、解析のバックグラウンド実行とキャッシュ無効化は先に固定が必要。
- **次に確認:** Waveform cache が保持する source offset と layer timing の対応を確認し、beat marker と keyframe snap の Undo 境界を定義する。transient / section 推定と自動 marker 大量生成は信頼度表示・preview / undo policy を決めてから追加する。

## 2026-09-10 — Shape F11/F13 表現系（stroke波・テーパーイーズ・Trim同時/個別・モーフ再サンプル）

- **関連:** `ArtifactShapeLayer.ixx/.cppm`、`ArtifactCompositionRenderOverlay.cppm`、`LayerEditorContextMenu.ixx/.cppm`。子リポ（ArtifactCore）のoperatorクラスはAGENTS.md制約で不変とした。
- **確認できた事実:** Trim同時/個別とRepeater本体はCore側に完成済みで親側の露出だけが不足、Wave operatorはCoreに存在せず、テーパーは線形rampのみ、頂点数不一致のパスKFはsnapだった。GPU/ソフトのstroke描画は`drawTaperedPolylineGPU`/`drawStrokePath`の2経路に集約され、legacy GPUは`GpuPaintItem.stroke`（=ShapeContentStroke）経由だった。
- **対応:** strokeに`waveEnabled/Amount/Frequency/Phase`+`taperEase`を追加し、新旧両描画経路・新旧JSON・property（wave系とeaseはキーフレーム可、`hasAnimatedShapeGeometry`の検出にも追加）へ接続。wave無効/量ゼロ時は従来経路と同一結果になる早期設計。HUDのTrim行へM:Sim/Ind表示、Solo ViewのManage Operatorsへ同時/個別トグル（JSONスナップショットUndo再利用）。頂点数不一致KFは`ShapePath::interpolate`+等間隔再サンプルでモーフ補間し、失敗時のみsnapへ縮退。
- **価値または懸念:** Repeater Composite順・SVG多段出力・contents stroke KF・式/pick-whip配線は親だけでは完結しないため対象外（Core改修または別器が必要）。モーフ再サンプルは不一致KF区間のみ毎フレーム64点評価する。
- **次に確認:** ビルド不可PCのため未検証（ユーザー申告）。特に描画シグネチャ変更の呼び出し3件、property order id -191〜-187、旧JSON再読込、wave+Dash併用時のDash優先を確認すること。

## 2026-09-10 — Shape F1/F2/F4/F5/F9/F10 メインVP移植とfill拡充

- **関連:** `ArtifactCompositionRenderController.cppm`、`ArtifactCompositionRenderOverlay.cppm`(+`.ixx`)、`ArtifactShapeLayer.ixx/.cppm`、`ArtifactCompositionEditor.cppm`、`ArtifactLayerMenu.cppm`、`ArtifactCompositionLayerUndoCommands.cppm`、`ArtifactPropertyWidgetShared.cppm`。
- **確認できた事実:** Solo View側のShape編集資産（頂点/tangent/segment grammar、角丸/星ハンドル、operator stack、Pen生成、頂点KF）は実装済みだったが、メインVP側は頂点ドラッグの断片と点描画のみで、ホバー・選択保持・パラメータハンドル・operator HUD・SVG入力導線・多段グラデーションが未接続だった。SVGパース（`parseShapeContentsFromSvg`）と`ShapeContent`モデル自体は存在し、UI呼び出しだけがなかった。
- **対応:** F1 overlay強調（頂点/tangent/選択/挿入マーカー/開閉、DTO渡し）+ホバー更新、F2 角丸/星ドラッグ（既存`ShapeCornerRadiusUndoCommand`+新`ShapeStarInnerRadiusUndoCommand`）とポリゴン頂点ドラッグ/Shift挿入（新`ShapePolygonPointsUndoCommand`）、F4 選択文法（Shift toggle/Ctrl add/置換、ボディクリック解除、Delete/Backspace削除、Ctrl+A、Escape解除）、F5 operator HUD（常時パネル）+Trim三角/ダイヤハンドル（新`ShapeOperatorValueUndoCommand`、`shapeOperatorValue`読取API）、F9 レイヤーメニュー2導線（SVG取込は新`ShapeSvgImportUndoCommand`、ベクターから作成は`AddLayerCommand`取引）、F10 マルチストップ（`ShapeGradientStop`+`gradientStops`、CPU/QGradient/JSON/property露出、空=従来2色に縮退）。
- **価値または懸念:** いずれも既存関数の拡張と既存パターンの再利用に留め、新規モジュール・CMake変更・シグナル追加なし。`WigglePaths`無条件キャッシュ回避、SVGグラデーションのCore単色縮退、stroke多段・Noise/pattern fill、Repeater対話編集は対象外として残る。
- **次に確認:** ビルド（`check_module_hygiene`含む）と実機runtime検証は未実施（AGENTS.md制約でユーザー許可待ち）。特にF5のTrim百分率クランプ、F2 Starクランプ（Solo View踏襲0.05〜0.99）、F4 Deleteのmask優先順位、F10旧JSON再読込互換を確認すること。

## 2026-09-10 — Composition VP の直接編集導線（V1〜V6）

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`ArtifactCompositionRenderController.cppm`、`TransformGizmo.cppm`。
- **確認できた事実:** 画像クロップ、平面サイズ／グラデーション、Fit／Align／Distribute、画像ソース差し替え、アンカー編集のコア処理は途中実装として既存ギズモ／数値プロパティと接続済みだったが、差し替え時の配置選択、9点プリセット、Comp／他レイヤーガイドへのアンカースナップ、Fit後の結果枠表示、VP下部の整列導線が不足していた。
- **対応（2026-09-10）:** 無修飾ドロップは変形を維持し、Shiftドロップは新素材を原寸・Comp中央へ置くUndo付き差し替えへ拡張。アンカー9点を既存Pivotメニューから選択できるようにし、CtrlスナップはComp端／中心と他レイヤーの可視境界へ拡張。Fit／Fill／Stretch後はComp枠と結果枠をVPへ重ね、下部Arrange HUDから同操作へ到達できるようにした。
- **懸念／未検証:** Shiftの配置リセットは現在フレームのTransform3Dへ書き込み、既存アニメーションの他時刻は変更しない。Qt／C++20 moduleのビルド、D&Dの実機修飾キー取得、回転・親子変形下のアンカーガイド位置、Fit結果枠の複数選択表示は未検証（ビルド・実行はユーザー許可後）。

## 2026-09-09 — ポイントトラッカーのコンポジション切替境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `setComposition()` / `trackerDelete()`。
- **確認できた事実:** トラッカーは `CompositionRenderController` が `TrackerManager` から一時生成し、現在の選択レイヤーをオフスクリーン取得して解析するコントローラローカル状態である。
- **対応:** 同一コンポジションの再バインド時は解析状態を維持し、別コンポジション（同一 ID の置換インスタンスを含む）へ切り替える場合だけトラッカーを停止・破棄する。
- **価値／懸念:** 前コンポジションの軌跡や ROI が新しいコンポジションに残る誤表示と、`TrackerManager` に残る一時トラッカーを防ぐ。永続保存が必要な場合は、コントローラ一時状態とは別のプロジェクト保存契約を設計する必要がある。
- **次に確認:** コンポジション切替を含む UI 操作でトラッカーパネルの表示・非表示とギズモ再接続が期待通りになるかを手動確認する（ビルド／実行はユーザー許可後）。

## 2026-09-09 — 単一フレームのポイントトラッキング

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm` の `trackRange()` / `trackBackwardRange()`。
- **確認できた事実:** 既存の成功条件は「シード後に少なくとも 1 ステップの信頼できる測定があること」だったため、フレームが 1 枚だけの静止画では有効なシード結果まで `false` になっていた。
- **対応:** サンプル数が 1 の場合はシードフレームを有効結果として扱い、2 フレーム以上では従来どおり測定ステップを要求する。
- **価値／懸念:** 単一フレームでも位置／アンカー適用を使える。移動量や品質を推定したわけではないため、長い範囲の品質判定は緩めていない。
- **次に確認:** 単一フレームの UI 操作で適用ボタンがシード位置を使うことを手動確認する（ビルド／実行はユーザー許可後）。

## 2026-09-09 — コントローラ破棄時の一時トラッカー解放

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `destroy()`。
- **確認できた事実:** 解析ジョブの停止・待機は行われていたが、`TrackerManager` に登録したコントローラ専用トラッカーを破棄する経路がコンポジション切替以外にはなかった。
- **対応:** 破棄時も `trackerDelete()` を通し、ジョブ待機後に manager から除去し、ギズモ参照を切る。
- **価値／懸念:** エディタ再生成や renderer 再初期化を繰り返しても、古いポイント軌跡と manager 所有オブジェクトが残らない。破棄中は再描画を要求するだけで、GPU リソース解放順序は従来のまま。
- **次に確認:** エディタタブの閉じる／再オープンを繰り返したときの tracker 数とパネル状態を手動確認する（ビルド／実行はユーザー許可後）。

# Insight Register

## 2026-09-09 — Planar Tracker の連番・非同期実行境界

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm`、`ArtifactCore/src/Tracking/PlanarTracker.cppm`、`Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Tool/ArtifactPointTrackerTool.cppm`。
- **確認事実:** `MotionTracker` のPlanarモードは4点＋ROI、Shi-Tomasi/PyrLK、RANSAC、ECC fallback、homography/confidence/JSONを持つ。Composition VPにはPlanar切替、4点投影overlay、Forward/Backward/All実行、Corner Pin keyframeへのUndo付き適用が接続済みだった。一方、旧Forward/Backwardは範囲内の連番ではなく始点・終点の1ペアだけを解いており、Backwardの画像入力順も現在点から過去点への写像と逆だった。
- **対応（2026-09-09）:** 最初の受入対象を選択中の静止画／画像連番レイヤーに限定した。GPU offscreen取得はUIスレッド上で1フレームずつイベントループへ返し、OpenCV solveは共有background poolへ分離した。Forward/Allは隣接フレーム順、Backwardは新設した`trackBackwardRange()`で逆順の隣接フレームを追い、VP toolbarにBackward／Stop／Forward／Allと進捗HUDを追加した。Cancel、tracker削除、controller破棄ではjob寿命を収束させる。
- **懸念:** `setFrame()`は互換境界の`QImage`から内部`cv::Mat`へ正規化して全対象フレームを保持するため、UIの連続停止は避けられても長尺・高解像度ではCPUメモリ量が大きい。GPU readback自体は安全のためUIスレッドに残しており、1フレームのreadback時間は隠蔽しない。動画素材、部分結果の採用、problem frame reviewは今回の対象外。
- **価値／次に確認:** 短い静止画連番で4点ROI→Forward/Backward/All→途中Stop→overlay→Corner Pin Bakeを実機確認する。次段ではリングバッファによる逐次solve、native frame snapshot、失敗フレームのreview UIを検討する。

## 2026-09-09 — Point Tracker の現状とPlanar共存

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm`、`Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認事実:** Point modeはPyrLKベースの単一点追跡、feature/search boxのサイズ調整、motion path表示、path pointの手動補正、Position／Anchor／Nullへの適用を既に持つ。Tracking実行はPlanar専用に固定されておらず、Tracker typeをPointに戻せば同じ非同期範囲jobを利用できる。
- **対応（2026-09-09）:** Planar job開始時の強制的なtype変更を外し、Point／PlanarのTracker typeを保持するようにした。VPにPoint／Planarのモードボタンとコンテキストメニュー導線を追加し、Point modeではROI外のクリックで追跡点を直接配置できるようにした。Gizmoの現在位置表示もフレーム番号の丸めではなく、結果フレームのtimeに最も近いpath pointを選ぶよう修正した。Point modeではFeature枠からLK windowを設定し、Search枠をPyrLK/NCC候補の境界として解析へ渡すようにした。完了HUDにはproblem frame数を表示し、Reviewボタン／メニューから次のproblem frameへジャンプできるようにした。
- **追加対応（2026-09-09）:** Forward／Backward範囲解析は開始フレームだけで結果を有効化せず、少なくとも1つの隣接フレームが信頼度閾値を通過した場合だけ成功扱いにした。全失敗トラックが誤ってBake可能になる経路を閉じた。
- **追加対応（2026-09-09）:** Point modeでROI外へ再配置した場合は旧トラック結果をクリアし、再配置点と過去のmotion pathが混在しないようにした。
- **追加対応（2026-09-09）:** Point modeのFeature枠はNCC template sizeにも反映し、Search枠全体を候補範囲として探索できるようにした。大きなSearch枠では探索コストが増えるため、実機で上限とPreview品質のバランスを確認する。
- **追加対応（2026-09-09）:** Point modeのpath point hit-testをROI内部より先に評価し、追跡後もFeature枠内の点を直接ドラッグ補正できるようにした。
- **懸念／次に確認:** Point modeには検索品質のproblem-frame一覧、テンプレート／補正履歴のreview UI、multi-pointの個別品質表示がまだない。まず単一点の短い連番で初期点→Forward→path correction→Bakeを確認し、Planarとの差をUI上のTrack Type選択へ整理する。

## 2026-09-09 — Composition VP と Layer Solo View のマスク編集能力差

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactLayerEditorWidget.cppm`、`Artifact/src/Widgets/LayerEditorMaskOverlay.cppm`、`Artifact/src/Widgets/LayerEditorMaskDragController.cppm`。
- **確認事実:** Composition VP は複数頂点選択、ラバーバンド選択、辺上頂点挿入、cubic Bezier の表示・hit-test、in/out tangent と feather handle、linked/broken 表示、Alt 分離、Ctrl reset、Undo snapshot を持つ。一方 Layer Solo View は既存 anchor／in-out handle の hit-test・単点 drag・Delete・close・Undo・proportional edit は持つが、overlay のセグメント描画は anchor 間の直線で、複数頂点選択、辺上挿入、feather handle、linked/broken tangent 操作文法、マスク新規作成の経路は確認できなかった。
- **対応（2026-09-09）:** `MaskVertexAddress` を共通の選択アドレスとして `MaskPath` module に置き、Composition VP と Layer Solo View の選択集合を同じ型にした。Solo View は18分割Cubic Bezierの描画・segment hit-test、単一／Shift追加／矩形選択、選択集合の一括移動・削除へ対応した。overlay には選択集合をポインタで渡し、フレームごとのコピー確保を避けた。
- **価値／懸念:** Solo View でも曲線上の選択と複数頂点操作がComposition VPに近づいた。Bezierサンプラー実装自体は両面に重複しているため、完全な数値共有は今後の検討対象。プロパティ／モード導線と実機受入れも別途確認が必要。
- **次に確認:** 実機で曲線表示、セグメント選択、Shift追加、矩形選択、一括移動／削除、Undoを確認する。続いてAlt/Ctrl tangent操作とfeather handleのSolo View parityを判断する。

## 2026-09-09 — Render Queue のフレーム単位ログ flush

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` の `processFramesForJob()`。
- **確認事実:** フレームの render begin／render end／encode begin／encode end 周辺で `Logger::flushFile()` がフレームごとに複数回呼ばれている。GPU readback、画像変換、プレビュー生成と同じ逐次処理経路にあり、ストレージ待ちをフレーム処理へ直接持ち込む構造になっている。
- **未検証:** 実ジョブでの flush 所要時間、OS キャッシュやログ出力先による差、障害復旧時に必要な永続化粒度。
- **対応（2026-09-09）:** 詳細なframe begin/end・encode begin/endを既定無効の`artifact.render.queue.frames` categoryへ移し、通常時のストリーム整形を遅延評価した。同期flushはフレーム失敗、encoder拒否、ジョブ終了の境界へ集約した。
- **価値／懸念:** 通常レンダリングからフレーム単位のログ整形・mutex・ファイルflushを除外した。categoryを明示的に有効化すれば従来相当の詳細イベントは取得できるが、正常フレームごとの即時永続化は行わない。
- **次に確認:** 代表的なRender Queueジョブで通常時とcategory有効時のログ内容、失敗時ログ、終了時flushを確認する。

## 2026-09-08 — ArtifactAbstractLayer の C++20 module 実装を内部パーティションへ分割する

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/cmake/ArtifactSources.cmake`。
- **確認事実:** `ArtifactAbstractLayer.cppm` は約13,700行で、約1,000行の `ArtifactAbstractLayer::Impl` と、物理、コンポーネント、エフェクト、プロパティ、マスク、JSON保存を一つの実装モジュール単位に集約している。MSVC 14.51 は13,683行、14.52 Previewは5,026行および診断用の単純化後7,591行で IFC import を伴う C1001 を起こした。
- **対応・確認:** `Artifact.Layer.Abstract.Utilities` へ有限値clamp処理を切り出し、`QDebug` / `qWarning` のストリーム演算子を可変長書式のログ呼び出しへ置換した。MSVC 14.52 Preview と、障害を再現していた MSVC 14.51.36231 の双方で `ArtifactAbstractLayer.cppm` のオブジェクト生成が成功し、14.51では `Artifact` ターゲット全体のリンクと `bin/Debug/Artifact.exe` 再生成まで成功した。
- **価値／懸念:** 公開 `.ixx`、状態遷移、ログ内容を維持したまま、IFC境界でのストリーム型展開を除去できた。巨大な実装単位自体は残るため、将来は `Impl` 状態パーティションを導入し、物理／シリアライズ／プロパティ／マスクの順に責務別分割を進める。

## 2026-09-08 — Audio Mixer のパン編集トランザクション

- **関連:** `Artifact/src/Widgets/ArtifactCompositionAudioMixerPresentation.cppm` の `setPanChangedCallback` と `recordMixerLayerPropertyChange`。
- **確認事実:** 既存のパン編集は値変更ごとにUndoコマンドを記録する。音量フェーダーにはドラッグ開始／終了単位の記録経路があるが、パンには同じ境界がない。
- **未検証:** 長いパン操作で履歴が細分化する程度、およびLayerChangedによる行再構築がドラッグ継続へ与える影響。
- **価値／次に確認:** 実機でパンの連続ドラッグとUndo回数を確認し、必要なら既存のUndo統合仕様を調査して1操作へまとめる。今回は採用デザインの反映と操作部品の整備に留め、履歴システムの構造は変更していない。

## 2026-09-08 — プレビュー重さの主犯はテキスト毎フレーム shaping と平面グラデ再生成(対応済み3点)

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm:3057-3078` (plain stroke)、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm:643-670` (gradient分岐)、`Artifact/src/Render/PrimitiveRenderer2D.cppm:970-1024,1035-1090` (qCDebug/ sprite cache)、`Artifact/src/Render/DiligentImmediateSubmitter.cppm:1536-1780` (shape+atlas)。
- **事実:** plain text の stroke がレイヤー側で8方向 `drawTextTransformed` を発行し、submitter 側でパケット毎に `shapeGlyphsForRender + FontManager::makeFont + atlas.acquire` が再実行されていた。SolidImage の gradient 分岐が `currentFillImage()` のキャッシュを使わず `makeSolidGradientImage()` (QImage+QPainter 全画面生成) をクローン毎・毎フレーム実行していた。`drawSpriteTransformed / drawMaskedTextureLocal` が毎スプライト `qCDebug` と先頭4KB hash+IMMUTABLE 再生成を行っていた。
- **対応:** stroke 8連打を `outlineColor/outlineThickness` 付き単発 `drawTextTransformed` に集約 (submitter の8方向 outline に委譲)。gradient 分岐を `currentFillImage()` キャッシュ参照+opacity パラメータ渡しに変更 (QImage 新規生成なし)。ホットパスの `qCDebug` を撤去 (挙動不変)。
- **価値／懸念:** GPU 経路優先・QImage/QPainter 新規なし・signal 追加なし。stroke 見た目はシェーダ側 outline (対角 0.707 補正) に寄るため厳密には非同一 (未検証)。rich text (`QTextDocument` 毎フレーム再構築+run毎 shaping) は未着手で残る。
- **次に確認:** ユーザー環境でテキスト (plain/rich/stroke/shadow)・平面 (Solid/SolidImage gradient) の体感比較、rich 側の CacheKey 付き runs キャッシュ、plain 側 QFont キャッシュの要否。ビルド・実機計測は未実施 (ユーザー指示待ち)。

## 2026-09-07 — モーションパスUndoに残る24fps固定時刻

- **対応追記（2026-09-07）:** ユーザー依頼により対象コマンドをRationalTime保持（複数キーは時刻スケール保持）へ変更し、関連ドラッグ・確定処理もコンポfpsに統一。以下は修正前の調査記録。静的確認済み、実操作検証待ち。

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionMotionPathCommands.cppm` の位置・接線・複数キーUndo、`ArtifactCompositionRenderController.cppm` の過去Planeリリース処理。
- **事実:** これらには `RationalTime(frame, 24)` が残る。一方、通常の編集開始は `gizmoTransformTime` でコンポfpsを使う。
- **懸念（未検証）:** 非24fpsでUndo対象時刻がずれる可能性がある。今回追加した過去枠Scaleは開始時のRationalTimeをコマンドへ保持する。
- **次に確認:** 30/60fpsで位置・接線編集とUndoの対象キーを比較し、既存コマンドの時刻受け渡しを別途そろえる。

## 2026-09-06 — AnimatableTransform3Dの24fps固定量子化(未検証・要修正)

- **関連:** `ArtifactCore/src/Animation/AnimatableTransform3D.cppm` (`setPosition:355`、`positionXAt:498`ほか全域で`toFrameCount(24)`/`rescaledTo(24)`)、`ArtifactCore/include/Animation/AnimatableValue.ixx` (`addKeyFrame:225`は同フレーム上書き)。
- **事実:** Transform3Dのキー格納・評価がコンポfps無関係に24で量子化される。30fpsでは評価バケットが5フレームに1回重複([3,8,13,18,23,28]が前フレームと同値)、60fpsでは半数以上が重複。重複書込みは上書きでキーを潰す。コンポが24fpsの場合は無害。
- **価値／懸念:** 非24fpsコンポで平面等の移動が周期的に止まって跳ぶ「がたつき」の最有力原因(未検証)。修正はfpsの配管が必要で`AnimatableTransform3D`単体では完結しない。
- **次に確認:** ユーザーのコンポfpsと平面がアニメーション有りかを確認し、再現すればfpsパラメータ化を実施する。

## 2026-09-06 — カーブ/Gizmoの時刻スケールと二重書きの乖離

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` (`applyCurveEditorMove`、`writeBackCurveEditorStructureDiffs`、削除ハンドラ)、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`gizmoTransformTime`、`applyLiveGizmoTransform`)。
- **事実:** Gizmoは`RationalTime(frame, doubleのfps)`を暗黙のint64変換で作り(29.97→29)、カーブは`llround`(29.97→30)で作っていた。`RationalTime::operator==`は既約分数の厳密比較のため別時刻となり、カーブ移動の旧キー照合が失敗して無音破棄された。GizmoのPropertyミラー条件(`!empty || autoKey`)とTransform3D条件(`hasKey || animated || autoKey`)も不一致で片方だけ更新された。`addKeyFrame`は同時刻上書きのため移動先衝突で隣キーが消えた。
- **対応:** fpsは`llround`+下限1に統一、キー照合は`rescaledTo(fpsInt)`のフレーム番号比較に変更、移動先衝突は拒否、ミラー条件はTransform3D側に合わせた。未検証: ビルド・実機確認は未実施(ユーザー指示待ち)。
- **次に確認:** カーブ→Transform3D方向の逆同期(現状はPropertyのみ書き戻し)、`clear+再add`の一括置換のトランザクション化、既存の混合スケールキーの救済が必要か。
- **2026-09-09追加調査:** 単一フレームギズモの拡縮をアンカー固定に変更し、開始snapshotにPropertyの時間評価値を反映。通常レイヤーのtoJsonはTransform3DのpositionKeyframes/scaleKeyframes等を書き出す一方、PropertySerializationBridgeは同関数ではeffect用に使用されている。二重保存の解消は、ライブ編集のミラー削除だけでは保存データを失う恐れがある。共有チャンネルへの集約と、既存Transform3Dの初期値＋位置オフセット、補間、空間タンジェント、旧JSON、Undo snapshotの移行を一体で設計する必要がある。2026-09-09に共有Propertyチャンネルへ移行。Transform3Dは同じPropertyへの互換APIとし、JSONはchannelsへ一本化、旧配列は読込のみ。初期値読込はキーを生成しない。Undoはキーと基底値を同じPropertyへ復元する。ビルド・実機未検証。
- **保存互換性:** 新形式のTransformキーは`channelSchema: 1` / `channels`のみを正本にするため、旧アプリへの保存互換性はない。旧ファイルにそもそも保存されなかったProperty専用キーや初期オフセットを、移行処理で復元することはできない。旧配列が存在する範囲で読込を維持する。
- **2026-09-09追記（静的確認）:** 左ペインのキー切替はPropertyを直接変更し、UndoとsetDirtyを迂回していたため共通TimelineKeyframeModelへ統合。LayerChangedで最終プレビューとoverlayキャッシュの更新漏れも修正。共有化後はmotionPathPositionKeyTimesのnativeフォールバックも同じキー集合を読む。空間タンジェントは対応する位置キーが残る場合だけ有効にした。次に、位置X単独削除・Y保持・空間タンジェント・Undo/Redo・保存再読込を同じキー正本で成立させる境界を確認する（実機未検証）。

## 2026-09-06 — Render Queue全消去を永続化する

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **事実:** 個別削除は `handleJobRemoved()` 経由で `render-queue.json` を更新していたが、`removeAllRenderQueues()` は全消去後に `persistQueueState()` を呼んでいなかった。そのため再起動時の `loadPersistentQueue()` で過去ジョブが復元され得た。
- **対応:** 全消去後に `impl_->persistQueueState()` を追加した。完了履歴の削除責務は既存の `clearCompletedJobHistory()` に残した。
- **価値／懸念:** 「全削除した過去キューが再起動後に戻る」経路を塞げる。ビルド・実機確認は未実施。

## 2026-09-06 — D3D12アダプタ列挙に有効なFeature Levelを渡す

- **関連:** `Artifact/src/Render/DiligentDeviceManager.cppm`、Diligent `EngineFactoryD3DBase`。
- **事実:** D3D12の `selectGpuAdapter()` が `EnumerateAdapters(Version{})` を呼び、Diligentの `GetD3DFeatureLevel()` にMajor/Minorが0の無効なVersionを渡してDebug assertionを発生させていた。
- **対応:** D3D12最小Feature Levelである `Version{11, 0}` を2回の列挙呼び出しへ指定した。
- **価値／懸念:** DiligentのD3D12アダプタ列挙契約に一致する。Vulkan経路やDiligentEngine本体は変更していない。ビルド・実機確認は未実施。

## 2026-09-06 — TimelineのRAM previewイベントからUIスレッドへ復帰する

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Service/ArtifactPlaybackService.cppm`。
- **事実:** Render Queue由来の `PlaybackRamPreviewStatsChangedEvent` が発行元スレッドでTimeline購読コールバックを実行し、`updateCacheVisuals()` 内の `QWidget::setToolTip()` が所有スレッド外から呼ばれてQt assertで停止していた。Stateイベントも同じ経路を持ち得る。
- **対応:** 両イベントの購読コールバックから `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` でTimeline所有スレッドへ処理を転送した。
- **価値／懸念:** Render Queue実行中のUI操作をQtのスレッド規約に揃えられる。ビルド・実機再生による確認は未実施。

## 2026-09-06 — MpmSnapshotのmap値をSharedPtr化してMSVCのvector ICEを回避する

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`。
- **事実:** MSVC 14.51が `std::map<LayerID, std::map<int64_t, MpmSnapshot2D>>` の値型デストラクタ展開中に、`MpmSnapshot2D` 内の `std::vector` でC1001／Access Violationを起こしていた。
- **対応:** `materialSnapshots_` の値を既存の `SharedPtr<MpmSnapshot2D>` に変更し、保存・検証・復元箇所で明示的に生成／デリファレンスした。
- **価値／懸念:** スナップショット内容とキャッシュ制御は維持しつつ、IFC経由のvectorデストラクタ実体化をmap値型から外せる。ビルドは未実施。

## 2026-09-06 — ProjectManagerWidgetはGenerationPreset型を直接importする

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/include/Layer/ArtifactGenerationPreset.ixx`、`Artifact/include/Layer/ArtifactGenerationPresetLibrary.ixx`。
- **事実:** Widgetは `ArtifactGenerationPreset` を直接使っていたが、`GenerationPresetLibrary` は型定義モジュールを再エクスポートしていないため、利用側で型が未定義になっていた。
- **対応:** `import Artifact.Layer.GenerationPreset;` をライブラリimportの前に追加した。
- **価値／懸念:** C2065および後続のconst int誤推論を、再エクスポート拡張なしで解消できる。ビルドは未実施。

## 2026-09-06 — SolidLayerテスト実装をArtifactのモジュールmanifestへ登録する

- **関連:** `Artifact/cmake/ArtifactSources.cmake`、`Artifact/src/Test/ArtifactTestSolidLayer.cppm`、`Artifact/src/Test.cppm`。
- **事実:** `ArtifactTestSolidLayer.cppm` は `Artifact.Test.SolidLayer` をexportし、`Test.cppm`も同モジュールをimportしていたが、Artifactの明示的なソースmanifestに実装ファイルが未登録だった。
- **対応:** `ARTIFACT_APP_IMPL_SOURCES` 相当のテスト実装一覧へ `ArtifactTestSolidLayer.cppm` を追加した。
- **価値／懸念:** C2230と連鎖する `runSolidLayerTests` 未定義を、モジュール依存追加ではなく正しいソース登録で解消できる。CMake再生成・ビルドは未実施。

## 2026-09-06 — GenerationPresetのネストラムダ捕捉型を明示する

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- **事実:** プリセット追加用のネストラムダで `preset` の型解決がMSVCの診断上 `const int` として扱われ、`validateGenerationPreset` に渡せなかった。また `FrameRange` 初期化は関数宣言と解釈される形だった。型付きcapture initializerはこのMSVC環境で構文エラーになった。
- **対応:** 外側で `ArtifactGenerationPreset presetValue` をコピーして通常の値捕捉へ分離し、内側の参照を `presetValue` に統一した。さらに外側のcallbackを `std::function<void(const ArtifactGenerationPreset&)>` として明示した。`FrameRange` はブレース初期化へ変更した。
- **価値／懸念:** C2664とC4930を対象箇所だけで解消できる。ビルドによる確認は未実施。

## 2026-09-06 — AbstractLayerのMSVC内部エラーをローカルラムダ依存から分離する

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **事実:** MSVC 14.51 が巨大な `setLayerPropertyValue` 内で、ローカル `finiteClampedValue` を別のローカルラムダからcaptureする構造を処理中にC1001／アクセス違反で終了した。
- **対応:** clamp処理をArtifact名前空間内の無名名前空間関数へ移し、`setJointFloat` のcapture依存を除去した。
- **価値／懸念:** 挙動を変えずにMSVCのラムダcapture解析経路を単純化できる。再ビルドによる確認は未実施。

## 2026-09-06 — ShapePathテストのShapeOperator import名を実モジュール名に合わせる

- **関連:** `Artifact/src/Test/ArtifactTestShapePath.cppm`、`ArtifactCore/include/Shape/ShapeOperator.ixx`。
- **事実:** テストは `Shape.ShapeOperator` をimportしていたが、Coreの公開モジュール名は `Shape.Operator` だった。
- **対応:** importを `Shape.Operator` に修正した。
- **価値／懸念:** C2230を依存追加なしで解消できる。ビルドによる確認は未実施。

## 2026-09-06 — AbstractLayerの補助ラムダはローカルclamp関数を明示captureする

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **事実:** `setJointFloat` ラムダが、同じ関数スコープの `finiteClampedValue` を既定キャプチャなしで参照していた。
- **対応:** `finiteClampedValue` を参照captureに明示追加した。
- **価値／懸念:** C3493/C2326を最小修正で解消できる。ビルドによる確認は未実施。

## 2026-09-06 — AbstractLayerのRigidBody2D参照はPhysics2Dを直接importする

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`ArtifactCore/include/Physics/2D/Physics2D.ixx`。
- **事実:** `RigidBody2D` は `Physics2D` モジュールの `ArtifactCore` 型だが、AbstractLayerは `Physics.System` のimportだけで直接参照していた。`Physics.System` は再エクスポートではないため型が可視にならない。
- **対応:** 実装ファイル側に `import Physics2D;` を追加した。インターフェース側や広域依存は変更していない。
- **価値／懸念:** C2039/C2065以下の連鎖エラーを最小依存で解消できる。ビルドによる確認は未実施。

## 2026-09-06 — SpatialAudioのBooleanプロパティ名を既存enumに合わせる

- **関連:** `Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`。
- **事実:** `ArtifactCore::PropertyType` には `Bool` ではなく `Boolean` が定義されており、SpatialAudioの `muted` / `enabled` プロパティだけが存在しない列挙値を参照していた。
- **対応:** 2箇所を `PropertyType::Boolean` に修正した。
- **価値／懸念:** C2838/C2065の直接原因を依存追加なしで解消できる。ビルドによる確認は未実施。

## 2026-09-06 — SpatialAudioLayerの3D判定は基底のvirtual契約に揃える

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/include/Layer/ArtifactSpatialAudioLayer.ixx`、`Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`。
- **事実:** `ArtifactAbstractLayer::is3D()` は実装を持つ非virtual関数だったが、SpatialAudioLayerが `override` として宣言していた。
- **対応:** 基底の `is3D()` をvirtualへ変更し、SpatialAudioLayerの既存overrideを有効化した。SpatialAudioLayerは生成時に既存の `setIs3D(true)` も実行している。
- **価値／懸念:** レイヤー種別ごとの3D判定を多態的な契約で扱える。既存呼び出し側の挙動差はビルド後に確認する。

## 2026-09-06 — NoiseLayerはNoiseSourceの状態へImplアクセサ経由でアクセスする

- **関連:** `Artifact/include/Source/ArtifactNoiseSource.ixx`、`Artifact/src/Layer/ArtifactNoiseLayer.cppm`。
- **事実:** `ArtifactNoiseLayer::Impl` は `ArtifactNoiseSource` を継承しているが、`ArtifactNoiseLayer` の外側のメンバー関数から基底のprotected状態へ直接アクセスしていた。
- **対応:** 設定、カラーマッピング、色、CPUバッファ、キャッシュに対する `Impl` の公開アクセサを追加し、外側の実装をアクセサ経由へ変更した。継承関係とキャッシュ所有権は維持した。
- **価値／懸念:** protected境界を破らずLayer側の評価・保存処理を継続できる。アクセサの公開範囲が広がったため、将来はSource専用の評価サービスへ分離できるか確認する。

## 2026-09-06 — WigglePathsの拡張プロパティはCore APIを先に揃える

- **関連:** `ArtifactCore/include/Shape/AeOperators.ixx`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- **事実:** ShapeLayer側は `temporalPhase`、`detail`、`correlation`、`smooth` を編集・正規化するコードを持っていたが、Coreの `WigglePaths` は `amount` と `frequency` だけを公開していた。
- **対応:** Coreへ4値の最小アクセサ、clone、JSON保存／復元を追加し、`temporalPhase` を既存の揺らぎ位相へ反映した。新規signal／slotは追加していない。
- **懸念:** `detail`、`correlation`、`smooth` は現段階では値の保持と編集基盤までで、形状評価への詳細な意味付けは未検証。

## 2026-09-06 — GPUComputeContext実装のモジュール依存はGPUCapabilities IFCを明示する

- **関連:** `ArtifactCore/include/Graphics/GPUComputeContext.ixx`、`ArtifactCore/src/Graphics/GPUComputeContext.cppm`、`ArtifactCore/CMakeLists.txt`。
- **事実:** `GPUComputeContext.ixx` は `Graphics.GPUCapabilities` をimportしているが、実装 `.cppm` 用のMSVC `/reference` 一覧には `Graphics.GPU.Info` と自己モジュールしか登録されていなかった。そのため実装コンパイル時にGPUCapabilitiesのIFCを解決できなかった。
- **対応:** `GPUComputeContext.cppm` のモジュール依存へ `Graphics.GPUCapabilities.ifc` の明示参照を追加した。
- **追補:** `Compute.cppm` も `GPUComputeContext` 経由で同じ interface import を解決する実装単位のため、同じ `GPUCapabilities.ifc` 参照を追加した。
- **確認:** 2026-09-06の実コンパイルコマンドには追加後の `GPUCapabilities` `/reference` と `MpmCompute` の `OBJECT_DEPENDS` が反映されておらず、生成済みCMakeビルドが古いことを確認した。
- **追補:** 個別のCMake分岐だけではBoids／Compute系の漏れが再発するため、`CORE_IMPL` の実装ソースを `import Graphics.GPUcomputeContext;` で検出し、GPUCapabilitiesのIFC参照とGPU関連interfaceの順序依存を後段で共通追加する。
- **再追補:** 実装 `.cppm` は同名の `.ixx` のimportを暗黙に引き継ぐため、実装本文だけのスキャンでは `BoidsCompute` や `LayerBlendPipeline` を検出できない。対応する `src/...cppm`→`include/...ixx` も走査対象にする。
- **価値／懸念:** C++20 moduleの実装単位でもinterface側importの依存を解決できる。CMake再生成後の実ビルド確認は未実行。

## 2026-09-06 — シェイプ形状からマスクを生成する責務はShapeLayerへ集約する

- **関連:** `Artifact/include/Layer/ArtifactShapeLayer.ixx`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`。
- **事実:** シェイプの評価済みジオメトリを `MaskPath::fromShapePath()` へ渡す一回限りの変換処理がメニュー側に存在していた。`nativeShapePaths()` は複数コンテンツ、シェイプ演算子、現在フレームのパス評価後の形状を返す。
- **対応:** `ArtifactShapeLayer::createMaskFromShape()` を追加し、変換と空結果の無効化をShapeLayer側へ集約した。メニューはUndo付きの既存導線を維持したまま新APIを利用する。
- **価値／懸念:** 将来のライブ形状マット、自動化、別UIから同じ変換契約を再利用できる。現時点ではスナップショット変換であり、形状変更への自動追従や専用の保存形式は未実装。
- **次に確認:** ライブ追従を導入する場合の所有関係、フレーム評価時の再生成コスト、マスクとシェイプの座標空間・反転／穴あきパスの受入れを定義する。ビルド・テスト・実機確認は未実行。

## 2026-09-06 — Solver横断Physics Snapshotはruntime handleではなくauthoring topologyを識別子にする

- **関連:** `ArtifactCore/include/Physics/2D/Physics2D.ixx`、`ArtifactCore/src/Physics/Physics2D.cppm`、`ArtifactCore/include/Physics/FluidSolver2D.ixx`、`ArtifactCore/src/Physics/PhysicsSystem.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **事実:** Soft Body／MPMには既存Snapshotがあったが、FluidとBox2D worldはSolver横断の復元契約を持たなかった。Box2Dのbody IDはworld再構築で変わるruntime handleである。
- **対応:** Rigid Bodyはbody index、LayerID、cloneIndex、transform、速度、typeをSnapshot化し、topology一致を検証してから復元する。Fluidもgrid dimensionsと作業バッファを含むSnapshotを追加し、PhysicsSystemに登録されたRigid／Soft／Fluid／MPMを同一frame keyでcapture／restoreする入口を追加した。既存のSoft/MPM専用復元APIは後方互換のため残した。
- **価値／懸念:** スクラブ／ループの共通基盤を作れる。現在のRigid Snapshotはshape／joint topologyそのものを再構築するものではないため、body追加・削除・collider変更時はcacheを無効化し、将来はauthoring revisionをcache keyへ追加する必要がある。
- **次に確認:** 現在Layer内で直接更新されるFluidSolver2Dを、入力注入とSolver更新の二重実行なしにPhysicsSystemへ移す。続いてPhysicsSystemの共通fixed-stepからframe indexを管理し、nearest snapshotからの前方向replay、loop range／cache offset、Fluidを含む実ランタイムのseek復元を接続する。ビルド・テスト・実機確認は未実行。

## 2026-09-06 — 環境変数をスクリプトへ公開 (getEnv/setEnv/hasEnv)・unsetVariable整備

- **関連:** `ArtifactCore/include+src/EnvironmentVariable/EnvironmentVariable.{ixx,cppm}`、`ArtifactCore/include/Script/Expression/ExpressionEvaluator.ixx`、`ArtifactCore/src/Script/Expression/ExpressionEvaluator.cppm`、`ArtifactCore/CMakeLists.txt`、`tests/ArtifactCore/EnvScriptTest.cpp`。
- **事実:** `EnvironmentVariableManager` に単体削除がなく `clear()` 全消去のみだった。スクリプト (ExpressionEvaluator) から環境変数を読む手段がなく、OS直読み (`qEnvironmentVariable`) が各所に散在していた。ArtifactCore→ArtifactCoreEnvironment の参照は静的ライブラリのため終端リンクで解決し、CMakeのターゲット循環にはならない。モジュール参照は既存の `/reference` + `OBJECT_DEPENDS` パターンで配線できた。
- **対応:** マネージャに `unsetVariable` (revision bump付き) を追加。式ビルトイン `getEnv(name[, default])` / `setEnv(name, value)` / `hasEnv(name)` を `registerStandardFunctions` に登録。setEnvはマネージャのオーバーレイのみに書き、OSプロセス環境は変更しない。新規テスト6件を追加。
- **価値／懸念:** TokenExpansion と同じマネージャを参照するため `$VAR` 展開とスクリプトが一貫する。一方、ビルドツリーには無関係の作業中変更 (DebugIdentity/ArtifactRegex等) による既存コンパイルエラーがあり、検証時は一時退避→復元した。
- **次に確認:** 実機での式エディタ経由の利用、OS環境への書戻しが必要かの判断、他スクリプト種別 (Python/C#/AngelScript) への公開要否。テスト実行は実績あり (6/6 passed)。

## 2026-09-06 — Alembicは既存MeshImporterの静的経路とキャッシュ経路を分けるべき

- **関連:** `ArtifactCore/include/Geometry/MeshImporter.ixx`、`ArtifactCore/src/Geometry/MeshImporter.cppm`、`Artifact/src/Layer/Artifact3DModelLayer.cppm`、`ArtifactCore/src/File/FileTypeDetector.cppm`。
- **事実:** `.abc` はFileTypeDetectorとAssetImporterで認識されるが、MeshImporterのBackendと拡張子分岐にはAlembicがない。MeshImporterには既に`importMeshFromFileAtTime()`がある一方、Artifact3DLayerは読み込み済み単一`Mesh`を保持する。
- **判断:** Alembicの対応準備では、代表時刻の静的ジオメトリ読み込みと、時間サンプルを再評価するキャッシュ再生を別フェーズにする必要がある。前者は既存MeshImporterへ閉じ込めやすいが、後者はframe/time変換、サンプルキャッシュ、メッシュ更新世代の契約が必要になる。
- **価値／懸念:** 既存のOBJ／FBX／glTF経路を広げずにレベル1の受入れを作れる。一方、複数オブジェクトや階層を単一Meshへ早期に押し込むと、後のキャッシュ／シーン対応で再設計になる可能性がある。
- **次に確認:** 採用ライブラリの配布条件、代表Alembicサンプルの分類、既存Meshのトポロジー更新API、GPUバッファ更新の必要範囲。今回、依存追加・ビルド・テストは未実施。

## 2026-09-06 — 既存MeshとloadFromFileAtTimeはAlembicの初期接続点になる

- **関連:** `ArtifactCore/include/Mesh/Mesh.ixx`、`Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- **事実:** `Mesh`はN-gon、頂点／face／face-vertex属性、revision、bounds更新、GPU向け三角形化を持つ。`Artifact3DLayer::loadFromFileAtTime()`は時間指定import結果をレイヤーのMeshへ差し替える既存入口である。
- **判断:** Alembicの静的サンプルは既存Meshへ変換できる可能性が高い。時間キャッシュは既存入口を使って最小実装を試せるが、再生性能が必要になった時点でreaderのサンプルキャッシュとGPU更新境界を分離するべきである。
- **価値／懸念:** 新規レンダラー経路を作らずに初期対応できる。一方、毎サンプルのMesh丸ごと差し替えを製品版の再生経路とみなすと、大きなキャッシュでCPUコピー・bounds計算・GPU再アップロードがボトルネックになる可能性がある（未検証）。
- **次に確認:** Alembicサンプルのトポロジー固定／可変、既存GPU uploadがMesh revisionをどう扱うか、代表キャッシュの1秒再生時の更新量。公式依存情報はAlembicリポジトリと公式ドキュメントを参照した。

## 2026-09-06 — 4分割VPは単一Swapchainのpresentation段で扱う

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`。
- **事実:** 実際のComposition Editorは`CompositionRenderController`が単一の`ArtifactIRenderer`と物理pixelサイズのswapchainを所有する。`ArtifactCompositionRenderWidget`は同名目的の軽量surfaceだが、現行Editorから生成・参照されていない。Diligentはbackend-neutralな`SetViewports`/`SetScissorRects`をD3D12とVulkanの双方で実装している。
- **対応:** rendererにoffset付きviewport/scissor APIを追加し、軽量surfaceには同一RT上で各paneをflushするQuad layoutの基礎を追加した。main controllerではGPU resolve済みのpresentation textureだけを4回drawし、重いcomposition-space cache／layer再合成は共有する。既存command infrastructureへ`View: Toggle Quad Presentation`を追加した。追加のswapchain、QSplitter、QImage合成は作らない。
- **価値／懸念:** QuadはDiligentの単一swapchain上で動作し、GPU合成を4回実行しない。一方、現在は同一の最終表示を4ペインに表示するpresentation sliceであり、gizmo／hit test／独立camera stateはまだpane routingされない。
- **次に確認:** D3D12/Vulkan双方でscissor復元、resize、overlay、GPU frame timeを実機確認する。次段階でpane固有cameraとinput routingを、controllerの既存camera stateを複製して接続する。ビルド・テストは未実行。

## 2026-09-05 — Box2D接触イベントはstep内で正規化する

- **関連:** `ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **事実:** Box2D 3.1.1のbegin/end/hit配列はworld step後の一時データで、shape削除後のend eventには無効shapeが含まれ得る。接触／hit eventはshapeごとに既定OFFである。
- **対応:** レイヤーbodyとfloor shapeでcontact/hitを有効化し、step直後に`PhysicsContactEvent`へコピーする。compositionが対象レイヤーへ配布し、レイヤーはstep単位のbegin/end/hit数、継続接触数、最大接近速度、直近hit情報を保持する。保存・Undo・新規signalは追加しない。
- **価値／懸念:** 将来の衝突particle／sound／break判定は同一の正規化済みデータを利用できる。shapeが削除されたend eventは安全のため解決不能なら破棄するので、その稀な経路ではactive数はworld resetまで残り得る。
- **次に確認:** 床・Dynamic/Static/Kinematicの組合せ、複数同時接触、body削除直後、hit speed閾値、将来の視覚／音反応のrate limit。ビルド・実機確認は未実行。

## 2026-09-05 — 再生中の物理ドラッグは保存済みJointと分離する

- **関連:** `ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **事実:** Box2D 3.1.1のMouse Jointは静的bodyとDynamic bodyの間で、world targetを追従させるランタイム拘束である。既存の`layerJoints`はComponentsから復元される保存済みconstraintの寿命を管理する。
- **対応:** `mouseJoints`を別のowner管理にし、再生中のSelectionツールで選択済みDynamic bodyだけに作成する。停止・release・body破棄で除去され、authoring transform、Components JSON、Undo履歴を変更しない。
- **価値／懸念:** 通常のVP Transformと競合せず、Spring/Rope/Sliderを維持したまま直接演技を調整できる。一方、現在は選択済みレイヤー全体を掴むため、collision shapeの厳密なポインタhit testやドラッグ強度のUI調整は未実装。
- **次に確認:** 再生中のbody質量ごとの追従感、ウィンドウ外release、再生停止・layer削除中のjoint除去、既存joint併用時の安定性。ビルド・実機確認は未実行。

## 2026-09-05 — 次の物理機能は「固定／操作可能なbody」と「スライド拘束」が最短

- **関連:** `ArtifactCore/include/Physics/2D/Physics2D.ixx`、`ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **事実:** 2D剛体はcomposition共有world、owner別jointの破棄、fixed-step更新まで接続済み。現行ラッパーが露出するjointはDistance/Revoluteのみだが、導入済みBox2D 3.1.1にはPrismatic、Mouse、Motor、Weld等のAPIがある。bodyにはsleep、CCD、damping、gravity scaleも既にある。LiquidSolver2Dはcontainer、opening、spill、checkpoint、collision layerへの衝突、surface snapshotまで存在し、SoftBodyはsnapshot、wind、collider、tear基盤を持つ。
- **判断:** 次の小さく実用的な追加は、Static/Kinematic/Dynamic body modeとViewportのMouse Joint操作、その次にPrismatic（軸・移動範囲・motor）である。新エンジンを導入せず、既存Box2DのCore ownershipとComponents面を維持できる。
- **価値／懸念:** kinematic targetは動く親・衝突壁・接続先の明示に使え、mouse jointは再生中の物理演出を直接調整できる。joint追加はanchor座標・Undo・seek再生成・joint別寿命管理を共有する必要がある。MPM、Fluid、SoftBody、PyroのGPU化はDiligent/CPU parityとsnapshot検証を伴うため別規模。
- **次に確認:** body typeのJSON／Components UI、編集時にkinematicへ安全にtransform同期する経路、mouse dragと既存VP transform toolの入力競合、Prismatic jointのlocal axis／limit／motorの最小契約。実機・ビルド未実行。

## 2026-09-05 — 2D body種別／Slider／破断を既存Box2Dへ追加

- **関連:** `ArtifactCore/include/Physics/2D/Physics2D.ixx`、`ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **対応:** CollisionにBody Type（Dynamic/Static/Kinematic）、JointにSlider（Prismatic: axis/limit/motor）とBreak Forceを追加。Kinematic/Staticは各fixed step前に編集Transformから位置／角度を同期する。Box2Dのconstraint forceがBreak Forceに達するとownerのjointだけを破棄し、runtime broken stateで再生成を抑止する。
- **寿命:** 閾値・body/joint設定はComponents JSONへ保存する。broken stateは保存せず、seek/reset／joint設定編集でfalseへ戻す。新規signalは追加せず、既存のLayerDirty/Components descriptor更新を使う。
- **次に確認:** Sliderのローカル軸・limitの視覚的な向き、kinematic bodyのアニメーション速度、破断境界値、Dynamic/Static/Kinematicの複数body衝突、Undo/Redo後の再接続。ビルド・実機未実行。

## 2026-09-05 — レイヤー間Spring／Ropeと接続点

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`ArtifactCore/src/Physics/Physics2D.cppm`。
- **事実:** 既存Springはhertz/dampingだけを指定し、Box2DのenableSpringを設定していなかった。joint専用layer worldは接続先をstatic proxyで追従し、Composition共有worldとは分離される。
- **対応:** Spring=3を有効化、Rope=4はenableSpring=true/hertz=0/enableLimit=trueで最大長のみ拘束する。owner body中心／target layer中心からのlocal X/Y offsetをComponents・JSON・descriptor・joint生成に接続。名前は一意な場合だけIDへ解決して保存する。床proxy(-3)をprimary bodyに選ばないよう修正。
- **根拠:** 既存依存Box2D 3.1.1、公式 `src/distance_joint.c`（MIT、 https://github.com/erincatto/box2d/blob/v3.1.1/src/distance_joint.c ）とインストール済み `types.h`。既存API利用であり外部コードの複製なし。CPU物理のみ、Diligent/D3D12/Vulkan資源・同期に変更なし。
- **双方向対応:** Collision / Joint有効の2D layerをcomposition共有worldへ統合し実body同士を接続。CoreのNamedVectorでowner別jointを所有し、body破棄時のBox2Dによるjoint破棄に登録情報を追従させる。固定targetはowner別proxyを維持する。
- **確認した問題:** bodyの位置をレイヤーpositionへ直接戻していたほか、Box2Dのradianを表示のdegreeへ直接代入していた。初期transform＋body差分で合成し、初期値を含むsnapshotを使用。PlaybackServiceの通常再生はgoToFrameを使うため、setFramePositionだけを直してもfixed-stepに到達しない。両入口のclockを統合した。
- **制限／次の確認:** 逆行／大きなseekは移動先編集値から再生成し、rigid snapshot完全復元は未実装。非一様scale親によるshearとcolliderの一致、layout/modifierとの併用、ネストcompositionの時間サンプリングは未検証。必要なら将来rigid snapshotとauthoring revisionによる無効化を分離する。今回ビルド・テスト・実機操作は未実行。双方への反作用、三者連鎖、片側無効化・削除、ばね振動、ロープ最大長、開始フレーム復帰、Undo・保存復元を次に確認する。

## 2026-09-05 — Construction Layerの描画と吸着を接続

- **関連:** `ArtifactConstructionLayer.cppm`、`ArtifactCompositionRenderController.cppm`、`ArtifactSmartGuidesManager.cppm`。
- **事実:** itemsは保存だけでdrawが参照していなかった。Smart GuidesはGuideSetの座標を親子transformなしで利用し、VPのprojected-frame候補はconstructionの内容を使っていなかった。最終出力包含ONでもguide除外条件と競合し得た。
- **対応:** Line/Circle/Annotationの描画・Inspector値編集と共通のlocal snap pointsを追加。VPとSmart Guides双方でtransformを適用する。包含ON時のguideフラグを整合させた。
- **価値／懸念:** スナップと表示が同じconstruction設定を使える。Inspector項目の追加Undoは非表示状態と値を復元し、配列内の無効項目は残す。既存APIからitemsを並べ替えるとordinalプロパティの対応が変わるため、将来の並べ替えUIにはIDベースのUndoが必要（未実装）。
- **次の確認:** ビルド・実機操作は未実行。保存復元、親子変換、編集Undo、出力ON/OFFを確認する。追加専用UI・寸法線・VP個別制御点編集は別作業。
- **VP編集追補:** 個別制御点ドラッグを実装。表示・pickは同じlocal handle座標を使い、press時のカメラと逆world transformでlocal z=0に交差させる。端点／平行移動／半径は開始時スナップショットから計算し、releaseで単一Undo、Esc／右クリックで復元する。Undo時に既存LayerChangedEventを再利用し、内容更新にはSource dirtyを使う。新規作図・文字入力はInspectorに残す。ビルド・実機操作は未実行。

## 2026-09-05 — VPフレーム吸着の残課題

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `snapProjectedFramePointer` とmouseMoveの呼出し。
- **事実:** コンポジションと他レイヤーの端・中央を候補化する。呼出しはFrontに限定され、ローカル軸移動はaxisMoveSnap対象外。候補は各pointer更新で全レイヤーのboundsを投影して再生成し、各軸の最短距離で選ぶ。
- **未検証の仮説:** 多数レイヤーでは候補再生成が入力負荷に寄与し、近接ガイド間では吸着先が切り替わりやすい可能性がある。平面1枚時の引っかかりの原因とは断定しない。
- **価値／次の確認:** ドラッグ中の候補計算時間を確認し、必要なら候補キャッシュと吸着の保持・解除閾値を導入する。今回は調査のみ。
- **実装追補:** ユーザー指定の1〜3に対応。吸着10 logical px／解除16 logical pxの保持帯を導入。候補配列を既存Core.Arrayへ移し、カメラ・viewport・選択・composition変更時と250ms間隔で再生成、ソートして二分探索する。press/modal開始/終了でキャッシュを無効化し、Alt/OFFで保持を解除する。Front限定を外し、ドラッグrayと同じカメラ行列で画面上のboundsへ吸着する。World/Local/ViewのX/Y/Z移動はギズモ内部のdragAxisDirectionを参照し、一つの画面ガイドへ軸方向の補正を行う。GPU資源・Diligent backend・同期経路は変更しない。
- **未検証／制限:** ビルド・テスト・実機操作は未実行。透視投影下の吸着は画面上の整列であり、3D面・頂点への吸着ではない。候補の外部変更は最大250msの更新遅延がある。透視ビュー・親付きローカル軸・Shift精密操作・Ctrl量子化の組合せで、ガイドと確定結果の一致を次に確認する。負荷軽減量は未計測。

## 2026-09-05 — Native Dock のQADS相当操作

- **関連:** `Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`。
- **事実:** Native DockはQADSを実行時に生成せず、Qtの `QTabWidget` / `QSplitter` と所有する `QDialog` によるbackend-neutral surfaceである。既存実装は他タブ面へのdropは持つが、タブ順の永続化、タブを外へドラッグして分離、フローティング状態からの再ドックを一貫して扱っていなかった。
- **対応:** タブ面の表示順をportable layoutへ保存・復元し、同一面のdropを順序変更として扱う。受け取り先のないtab dragは所有panelを保持したままフローティング化し、浮動ウィンドウのDock backボタンで元のareaへ戻す。復元時にもembedded/floating間を変換する。closeは非破壊の非表示を維持する。ドラッグ中は、候補tabまたはdock areaに半透明のアクセント色プレビューをowner-drawで表示する。tab面とtab barの双方でdock MIMEを受け、既存tab上はtab化、空白部はその領域へ追加する。tabのダブルクリックも同じ安全なフローティング経路へ接続した。
- **未検証:** 実機でのtab drag、外部dropのcancelと分離の境界、Dock back、ドロッププレビュー、再起動後の順序／geometry、各既存panel（viewportを含む）のfloating再親子化とサイズ更新。Build/testは未実行。

## 2026-09-05 — App Debuggerの自動更新とVP入力停止の候補

- **関連:** `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm` のtimerEvent/refresh、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のframeDebugSnapshot。
- **事実:** App Debuggerは250msのtimerでrefreshし、controllerのsnapshotを取得する。snapshotにはGPU画像のreadback、最終effect処理、preview差分画像作成が含まれる。refresh入口は再入ガードのみで非表示チェックがない。VPの定期描画はrenderOneFrameを通らずrenderOneFrameImplを直接呼ぶため、追加したイベント6/8はその経路を計測していない。
- **未検証の仮説:** 診断UIの周期的な更新がUIスレッドを占有し、パン入力の約397msの空白や描画開始間隔の約570ms〜1.09秒の空白へ寄与している可能性。実際に当該widgetが生成済みか、更新の実測時間は未確認。
- **価値／次の確認:** 診断機能自身の観測負荷を分離する。App Debuggerを生成しない起動で比較し、必要ならrefresh前後とtickキュー投入/受信、renderOneFrameImpl全体を計測する。修正は今回行っていない。
- **追加確認・対応:** Native DockのaddLazyDockedWidgetFloatingはfactoryを即時呼び出すため、App Debuggerは未表示でも生成されていた。ユーザー依頼によりconstructorでのtimer開始を削除、非表示refreshを抑止、show/hideでtimerを開始/停止。初期登録と保存レイアウト復元後は非表示へ固定した。Native Dockのタブに標準closeアイコンを持つボタンを追加し、既存eventFilterからsetDockVisibleへ渡す（新しいsignal/slot接続なし）。タブの非表示・再表示はsetTabVisibleで保持し、内容widgetは破棄しない。ビルド・非表示時の負荷・全タブを閉じた後のメニュー再表示・キーボードSpace操作は実機未検証。

## 2026-09-04 — 物体検出バックエンドの共通契約

- **関連:** `ArtifactCore/include/AI/ObjectDetector.ixx`。
- **確認できた事実:** 既存の物体検出は具体クラスしかなく、ONNX等の実検出器を同じ呼び出し側へ接続する抽象契約がなかった。`IObjectDetector` に ready、detect、error 状態を定義し、既存検出器を適合させた。
- **価値／懸念:** 将来の実モデルを App API の変更なしに差し替えられる。現行の輝度ベース検出はフォールバックであり、物体認識の品質を保証しない。
- **次に確認すべきこと:** 実ONNX検出モデルのラベル・矩形・NMS出力をこの契約へ正規化する。

## 2026-09-04 — 連番マスクの変化診断

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** 連番のマスクを正規化座標で比較し、平均差分、最大差分、大きく変化した面積率を返す診断APIを追加した。
- **価値／懸念:** App側は急なマット変化を検出して再推論や手動確認を促せる。これは動き補償を行わないため、被写体が移動する連番ではRoto Brush伝播後の比較を前提とする。
- **次に確認すべきこと:** 実連番で警告閾値と、再推論・安定化のUI方針を決める。

## 2026-09-04 — セグメンテーションマスクの非破壊プレビュー

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** `DepthMap` は正規化された単一チャンネル値を保持する。これを直接 `ImageF32x4_RGBA` のグレースケール画像として生成するプレビュー API を追加した。
- **価値／懸念:** App側は元画像やalphaを変更せず、推論・Roto Brush・手動補正のマスクを共通表示できる。GPUプレビューとの最終的な見え方の一致は実機確認が必要。
- **次に確認すべきこと:** App の既存マスク表示導線へ接続し、比較表示と反転表示を確認する。

## 2026-09-04 — ONNX セグメンテーション設定契約のテンプレート化

- **関連:** `ArtifactCore/docs/ONNX_IMAGE_SEGMENTATION_CONFIG.md`。
- **確認できた事実:** モデル固有の入力サイズ・正規化・色順・出力選択は `loadOptionsFromJson()` で外部化されている。設定ファイルの最小テンプレートと許可値を文書化した。
- **価値／懸念:** モデル導入時にコード変更ではなくモデル配布物だけで契約を更新できる。テンプレート値は特定モデルの推奨値ではないため、実モデル仕様との照合が必須。
- **次に確認すべきこと:** 最初の採用モデルについて、モデル／設定／ライセンス情報を同じ配布単位にまとめる。

## 2026-09-04 — 一括セグメンテーション後処理へのクリーンアップ統合

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** batch API は `SegmentationMaskRefinementOptions` を通じて後処理を実行する。穴埋めと小領域除去も同設定に統合し、モデル推論後の各フレームへ一貫して適用できるようにした。
- **価値／懸念:** 単一画像と連番バッチでマット整形の条件がずれない。既定では両方無効であり、形状を変える処理は明示設定時だけ適用される。
- **次に確認すべきこと:** App側で素材カテゴリに応じたプリセットを設けるか検討する。

## 2026-09-04 — セグメンテーションマスクの小領域除去

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** 推論マスクには孤立した小さな前景島が現れる。閾値以上の連結成分を走査し、指定面積以下だけを透明化する処理を追加した。
- **価値／懸念:** 背景上の小さな誤検出を、モデル変更なしにプレビュー段階で抑えられる。小物や細部まで消す可能性があるため、既定値は無効で明示的な面積指定を必要とする。
- **次に確認すべきこと:** 人物の髪・アクセサリー、製品写真で妥当な面積範囲を確認する。

## 2026-09-04 — セグメンテーションマスクの穴埋め

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** AIマットには前景内の小さな背景穴が発生する。外周へ到達しない背景連結成分だけを検出し、面積上限を指定可能な穴埋め処理を追加した。
- **価値／懸念:** 人物・製品の内側に残る小穴をモデル非依存で整えられる。細いリング状オブジェクトの内側を消してしまうため、面積上限とプレビューでの確認が必要。
- **次に確認すべきこと:** 文字、メガネ、穴のある製品素材で既定の面積上限を決める。

## 2026-09-04 — ONNX セグメンテーションの入力色順設定

- **関連:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。
- **確認できた事実:** ONNX の画像モデルには RGB だけでなく BGR の入力テンソルを前提とするものがある。`inputColorOrder` を JSON 設定および型付き option に追加し、色順に対応する mean/stddev も正しい色成分へ適用する。
- **価値／懸念:** モデル固有の色順のためだけに変換ノードを増やさず、U²-Net系などの導入候補を設定で試せる。実モデルでの色順・正規化仕様の確認は未実施。
- **次に確認すべきこと:** 導入する実モデルの preprocessing 定義を JSON と照合する。

## 2026-09-04 — 物体検出から共通マットへの接続

- **関連:** `ArtifactCore/include/AI/ObjectDetector.ixx`、`ArtifactCore/src/AI/ObjectDetector.cppm`。
- **確認できた事実:** 検出結果はラベル、信頼度、矩形を持つが、既存コードにはマスク処理へ渡す経路がなかった。検出矩形をsoft edge対応の`DepthMap`へ rasterize するAPIを追加した。
- **価値／懸念:** 将来のYOLO等の実検出器でも、矩形を初期選択・保護領域・Roto Brushの開始マットとして共通利用できる。矩形は物体輪郭ではないため、最終切り抜きにはセグメンテーションとの合成が必要。
- **次に確認すべきこと:** 実検出モデルを導入後、複数検出のラベル選択とセグメンテーション初期化のUI導線を設計する。

## 2026-09-04 — AI マスクの切り抜き・背景置換を共通 CPU 経路に集約

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** セグメンテーション結果は `DepthMap` の正規化マスクとして扱える。切り抜き、自動前景クロップ、単色背景、背景画像の合成を `ImageF32x4_RGBA` と `FloatRGBA` の直接操作で追加し、Qt 合成・`QImage` 変換を経由しない。非AIの輝度フォールバックも既存の透明領域を前景として復活させないよう、入力alphaを既定で尊重する。
- **価値／懸念:** ONNX、Roto Brush、将来のモデルが同じ出力処理を共有できる。背景画像はバイリニアでサンプルし、異解像度の置換で段差を作らない。将来は GPU 経路で同じ straight-alpha 契約を維持する必要がある（未検証）。
- **次に確認すべきこと:** GPU 合成経路へ接続する際、straight-alpha の契約を保持してプレビューと書き出しの結果を一致させる。

## 2026-09-04 — セグメンテーション境界の色かぶり補正

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** 前景マスクの半透明境界では、グリーン／ブルースクリーンの色が残る。mask coverage が中間値の画素だけに green/blue の過剰成分を抑える処理を追加した。
- **価値／懸念:** 切り抜きの縁をモデル非依存で改善できる。人物固有の緑／青を過度に変えないよう、完全不透明領域には適用しない。強さと境界幅は実素材で調整が必要。
- **次に確認すべきこと:** 緑髪・青い衣装を含む素材で、補正量とエッジ幅の既定値を決める。

## 2026-09-04 — 高解像度向けマスク後処理の分離パス化

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** foreground の expand／contract は矩形要素の max/min 演算、feather は矩形 box blur として実装されている。どちらも水平・垂直の二段に分離しても edge-clamp を含む結果は同じになる。
- **価値／懸念:** 半径に対する計算量を二次から線形へ下げ、4K素材のマスク調整を実用的にする。CPU処理のままなので、GPU経路が整った段階で置き換え候補として確認する。
- **次に確認すべきこと:** 実素材で既存 GPU 経路とのエッジ見え方とCPU処理時間を確認する。

未着手の設計判断、現在の優先方針に直結する実装候補、実機検証待ちだけを記録する。実装済みの詳細履歴と過去の調査は [Insight Archive (through 2026-09-01)](docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md) を参照。

## 現在の優先検証

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

### 2026-09-06: 個別 proxy playback toggle の未接続を解消

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` の Project View footage context menu、`ProxyMeta::enabled`、`syncProxyPathToProject()`。
- **事実:** `ProxyMeta::enabled` と global proxy toggle は存在していたが、footage 単位で enabled を変更する操作がなく、生成完了時は常に `enabled=true` として同期していた。
- **対応:** footage context menu に `Enable/Disable Proxy Playback` を追加し、個別設定を `syncProxyPathToProject()` へ接続。worker 成功時も既存の個別 enabled 設定を尊重し、生成時の source timestamp を成功メタデータへ確定するよう修正した。
- **未検証:** runtime の context menu 操作、global toggle との組み合わせ、保存／再読込後の個別設定保持は未確認。ビルド・テストはユーザー指示待ち。

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
## 2026-09-04 — Spatial Audio Object契約を既存3D Audio Layerへ接続

- **関連:** `Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`、`ArtifactCore/include/Audio/Spatial/SpatialParams.ixx`、`ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`
- **事実:** 既存のSpatialRendererは距離減衰・azimuth panningを実装済みだったが、Audio Objectのstable ID、spread、gain、mute、enabledの保存契約が不足していた。
- **対応:** stable UUIDとAudio Object状態を追加し、JSON保存/復元、Property経路、最小ステレオspreadを既存レンダラーへ接続した。
- **未検証:** ビルド・実機再生・旧プロジェクトfixtureによる復元は未実行（ユーザー許可待ち）。
- **次の確認:** M-AU-9.2としてcallback境界でのallocation/lock不在、mono/stereo入力、seek/restart時の状態リセットを確認する。

## 2026-09-04 — mono空間音源のstereo preview拡張

- **関連:** `ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`
- **事実:** 入力がmonoの場合、従来の出力チャンネル数判定が1chを維持し、左右のazimuth gainを利用できなかった。
- **対応:** Phase 1のpreview契約として、出力バッファが1ch以下でも最低2chを確保し、stereo layoutを設定するよう修正した。
- **未検証:** 実機再生とサンプルレート別の音量・位相確認は未実行。

## 2026-09-04 — 7.1.4レイアウト契約の共通化

- **関連:** `ArtifactCore/include/Audio/AudioSegment.ixx`、`ArtifactCore/src/Audio/AudioBus.cppm`、`ArtifactCore/src/Audio/AudioDownMixer.cppm`、`ArtifactCore/src/Codec/FFMpegAudioDecoder.cppm`
- **事実:** 既存レイアウト列挙には 7.1.4 がなく、12ch入力が汎用Stereoへフォールバックしていた。
- **対応:** 12chを `Surround714` として識別し、バス確保、downmixerの恒等マッピング、リングバッファ、LipSync、FFmpeg decoder、LFE判定へ接続した。
- **価値／懸念:** 7.1.4素材のチャンネル数とレイアウトを失わず保持できる。現時点では各スピーカーへの object 配分係数、UI／Render Queue選択、7.1.4からの明示的downmix係数は未実装・未検証。
- **次に確認:** 7.1.4 bedの標準チャンネル順を固定し、Render QueueとPreviewの出力選択へ接続する。

## 2026-09-05 — Particle 3D のレイヤー変換境界

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`ArtifactCore/include/Graphics/ParticleData.ixx`
- **事実:** `ArtifactParticle3DLayer` は `is3D=true` で camera view/projection 経路へ入り、3D分岐では2D `QTransform` を避けている。`ArtifactFormParticleLayer` も Grid3D 時に同じ経路へ入るが、レイヤーのモデル行列は未設定だった。
- **対応:** `ParticleRenderData`、Core constant buffer、GPU cull shader、vertex shader、Diligent submitterを通るmodel matrix経路を追加し、Form Particle のGrid3Dにも接続した。2D経路はidentity model matrixを維持する。
- **価値／懸念:** camera orbit とlayer transformの責務を分離できる。constant-buffer layoutを変更したため、D3D12/Vulkan双方でshader/PSO再生成を伴う。
- **次に確認:** runtimeで Particle 3D の位置・回転・scale を個別に変更し、camera orbitとは独立して反映されるか、GPU cullと2D Particleに回帰がないか確認する。

## 2026-09-05 — PhysicsSystem のモジュール内コンテナ破棄

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`、`ArtifactCore/include/Physics/MpmSolver2D.ixx`、`ArtifactCore/src/Memory/SharedPtr.cppm`
- **事実:** MSVC 19.51 は、exported `PhysicsSystem` の `std::map<LayerID, SharedPtr<MpmSolver2D>>` を破棄するテンプレート展開中に C1001 を発生させた。`MpmSolver2D` は import 済みの完全型であり、所有契約の不備は確認されていない。
- **対応:** `LayerID` には `std::hash` がないため `IdMap` は採用せず、該当ストアを既存の `NamedVector` による小さなキー付きエントリへ移した。デストラクタもクラス外 default 定義へ置き、キー別の登録・取得・削除・列挙の契約を保持しつつ、MSVCが落ちる `std::_Tree` と `std::_Hash` の実体化を排除した。
- **未検証:** 影響する最小経路は Core の起動・PhysicsSystem singleton の終了時破棄。ユーザーのビルドで C1001 が解消すること、および Physics/Material solver の生成・破棄を確認する。

## 2026-09-05 — Native Dock の右端編集面を限定

- **関連:** `Artifact/src/AppMain.cppm`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **事実:** Native Dock Surface では Inspector 系に加え、Layer View、Contents Viewer、Audio Mixer も右端へ登録されていた。AI Cloud は既に起動時非表示だった。
- **対応:** Layer View を Composition Viewer の中央タブ、Contents Viewer を Project の左タブ、Audio Mixer を Timeline の下部タブへ移した。右端は Inspector / Properties / Components / Effects を優先し、AI Cloud は非表示のままにした。
- **未検証:** 初期レイアウト、既存保存レイアウトからの復元、Timeline 未生成時の Audio Mixer の下部配置は未実行。

## 2026-09-05 — Default ワークスペースの初期可視性を軽量化

- **関連:** `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **事実:** Default ワークスペースは Composition Viewer / Project / Asset Browser / Inspector / Effects / Properties と Timeline を同時に可視化していた。
- **対応:** Default は Composition Viewer / Project / Inspector のみを表示し、Asset Browser、Effects、Properties と Timeline を初期非表示にした。Animation ワークスペースは Timeline の自動表示を維持する。
- **未検証:** 保存済みレイアウト復元後の可視状態、および明示的に開いた Asset Browser / Timeline の操作性は未確認。

## 2026-09-05 — ギズモ入力座標と即時描画の境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`ArtifactCompositionRenderOverlay.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`
- **事実:** press/move の入口で物理ピクセルへ変換済みだが、projected frame press と3D dragで DPR を再乗算していた。フレームの8ハンドルより3D軸判定が優先されていた。描画は即時ではなく頂点を蓄積し、flush 時のカメラ行列を使う。即時描画と判断して末尾 flush を削除したのは誤りだった。
- **対応:** 二重変換を削除し、近いフレームハンドルを優先。フレーム描画前に flush し、選択イベントでも base composite を無効化。標準モードは移動軸とフレームに整理した。
- **未検証の別件:** 過去フレームの平面操作にも同様の DPR 再乗算が見える。今回の現行フレーム修正範囲から分離し、motion frame操作を次に確認する。共通矢印プリミティブの形状変更はライト等の矢印にも反映されるため外観確認が必要。
- **確認待ち:** 平面追加直後、100/150/200%表示倍率での8方向ドラッグ、回転モード、Undo/キャンセル。ビルド・実機操作は未実行。
- **水色全面表示への修正:** フレーム末尾の `flushGizmo3D()` を復元し、カメラ行列のリセット前にハンドル頂点を送信する。水色全面表示の解消は実機未確認。
- **初期フィット・リサイズ追補:** 通常2D平面のフレームに scene camera が使われる経路を canvas pan/zoom に統一。外側余白を描画・pick・snap・固定点の全箇所から除去した。ドラッグ中の単一レイヤー再同期を止め、固定点補正は2Dのvisual/local scale比と3D回転を考慮する。単一レイヤーのdrag通知は既存の間引き処理を使用し、releaseで最終値を強制通知する。負荷改善と初期フィット、回転・親付きレイヤーの対辺固定は実機未検証。
- **位置飛びの再調査:** `positionXAt/YAt` は初期位置を含まないoffsetで、`snapshotAt` が初期位置を加算することをCore実装で確認。ギズモのpress/modal/group/release保存を後者へ変更した。`UndoManager::push` は即redoするため、releaseでoffsetを絶対位置として再適用すると位置が飛ぶ。通常ビュー行列は実際の `canvasToViewport` から構成し、ギズモ描画による2D行列上書きを撤去、drag rayは開始カメラを保持する。直交ギズモのサイズは描画viewの倍率から計算する。ユーザー報告の3症状について実機での改善確認は未完了。
- **正面直交ビューへの統一:** ユーザー指定により起動時・2D復帰をFrontへ統一。正面の投影は直交、その他の方向は既存透視投影を使用する。直交カメラはprojection側にzoomを保持するため、ギズモのサイズ補償もviewport zoomへ合わせた。コンポジション境界の水色重ね描きを中立色へ変更。panにも既存のinteraction通知を追加し、操作中のLODとreadback抑制を有効にする。パン・ズームが重い主因と改善量は未測定で、実機確認が必要。
- **スナップ設定:** 既存ViewメニューのsnapGuidesチェックと永続化経路を再利用し「コンポジション／ガイドにスナップ」としてprojected frameの移動・リサイズへ接続。Frontのみ、Altで一時解除、10 logical pxの閾値。コンポジション境界はフレームと同じ投影を使用する。OFF時は吸着ガイドを消去。定規は未設定時に非表示、保存済み設定は維持。実機でON/OFF・再起動・Alt・ズーム倍率別の吸着・Undoは未検証。
- **正面消失・スナップ追補:** Qt直交行列の近側は負のNDC深度になるが、PrimitiveRenderer3Dのshaderは投影結果をそのままSV_Positionへ送っていた。Frontの直交投影をD3D12/Vulkanの0..1深度へ補正した。移動軸のpress経路はprojectedFrameMoveを立てないため吸着対象外だったので、位置適用方式は変えずScreen移動とWorld/ViewのX/Y移動を吸着対象へ追加した。軸拘束と直交するガイドは表示しない。ローカル軸の吸着、Shift精密操作・Ctrl量子化との併用は未検証。正面での再表示、通常移動・8方向リサイズの吸着、OFF/Alt、ドラッグ確定・Undoを実機で確認する必要がある。GPUリソース・同期・キャッシュの寿命は変更していない。

## 2026-09-05 — 頂点のみPLY点群の最小受入

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (loadPly)、Artifact/src/Layer/Artifact3DModelLayer.cppm (draw)、3D系ファイルフィルタ3箇所
- **事実:** loadPly は aceCount<=0 を拒否していたため頂点のみPLYが読めず、FileTypeDetector は既に ply を Model3D 扱いなのに開く側のフィルタ3箇所に *.ply が無かった。Mesh::isValid() は polygon>0 必須だが Artifact3DLayer::loadFromFile は ertexCount>0 判定なので importer 側の修正だけで meshLoaded_ まで届く。generateRenderData() は polygon 無しで空を返すため drawMesh 経路では何も出ない。
- **対応:** loadPly で face無しを受入れ、頂点色 (red/green/blue 系・uchar/float両対応) を color アトリビュートへ格納、頂点上限200万で拒否。Artifact3DLayer::draw の Solid/Wireframe 両経路に polygon==0 時の点描画フォールバック (3軸クロス、32768点cap・stride間引き、頂点色・opacity反映、trace points-submitted) を追加。フィルタ3箇所へ *.ply 追加。.ixx 変更なし。
- **未検証:** ビルド・実機表示 (頂点のみPLY、色付きPLY、既存ポリゴンPLYの回帰)、200万超・バイナリPLYのエラー表示、大規模点群の描画負荷。バイナリPLY/LAS/LAZ/E57 は対象外のまま。

## 2026-09-05 — 点群フォールバックの継続 (選択表示・テスト)

- **関連:** Artifact/src/Layer/Artifact3DModelLayer.cppm (drawSelectionOutline)、	ests/ArtifactCore/MeshImporterPlyTest.cpp、	ests/ArtifactCore/CMakeLists.txt
- **事実:** polygon==0 の点群は選択アウトラインの polygon ループが空振りで何も出なかった。MeshImporter への gtest は存在せず、	ests/models/test.obj を参照するテストも無かった。drawFractureOverlay は mesh 非依存のため点群でも安全。
- **対応:** 選択時は world-space の bounds box (12辺) を outline 色で描画。PLY は一時ファイル自己完結の gtest 6件 (face無し/face0/uchar色/ポリゴン回帰/欠損/バイナリ拒否) を追加し、ArtifactCoreMeshImporterPlyTest として登録。
- **未検証:** ビルド・テスト実行はユーザー指示で保留 (ArtifactCore の増分ビルドは MeshImporter.cppm のコンパイル・リンクまで成功済み、Artifact 側は中断)。

## 2026-09-05 — バイナリPLYと点サイズ調整

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (loadPly)、Artifact/src/Layer/Artifact3DModelLayer.cppm、	ests/ArtifactCore/MeshImporterPlyTest.cpp
- **事実:** バイナリPLYは finiteness・色・face を含め未対応だった。点の見た目は bounds 由来の自動サイズのみで調整手段が無かった。
- **対応:** loadPly をヘッダ構造体解析へ組替え、binary_little/big_endian に対応 (型: char〜double・list face・uchar/float等の色正規化、200万点上限・truncated 検出は ASCII と共通)。Artifact3DLayer に 
ender.pointSize (0.25〜8.0、既定1.0、JSON保存・Inspector・set 反映、crossHalf に乗算) を追加。.ixx 変更なし (normalLength と同方式)。テストはバイナリ実ペイロード (LE/BE/truncated) へ置換え。
- **検証:** ArtifactCore 増分ビルド成功、Artifact3DModelLayer.cppm.obj 単体コンパイル成功。Python ミラーで LE/BE 値と byteswap 経路を確認。テスト実行・実機表示は未実施。LAS/LAZ/E57 は対象外のまま。

## 2026-09-05 — 非圧縮LASの最小受入とE57見送り判断

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (loadLas)、ArtifactCore/include/Geometry/MeshImporter.ixx (Backend::Las)、ArtifactCore/src/File/FileTypeDetector.cppm、3D系フィルタ3箇所、	ests/ArtifactCore/MeshImporterLasTest.cpp
- **事実:** LAS 1.0-1.4 の非圧縮 point format 0-8 はヘッダ固定オフセット (scale/offset/count) と record 先頭の XYZ・format別RGB位置で読める。LAZ は同一シグネチャのまま圧縮本体のため別 decoder が要る。E57 (ASTM E2807) は XML＋packet/codec バイナリ構成で、Qt の XML だけでは binary section の実装が数百行規模になる。
- **対応:** loadLas を追加 (format 0-8、XYZ+RGB/intensity→gray、上限200万点、waveform/未知format・不正scale・truncated は明示エラー、LAZ は非対応メッセージ)。dispatch・Backend::Las (末尾追加)・FileType・フィルタ3箇所へ las 配線。gtest 4件 (format2 RGB / format0 intensity / waveform拒否 / 非LAS拒否) を登録。E57 は外部libなしの自前実装を見送り。
- **検証:** ArtifactCore・ArtifactCoreFile 増分ビルド成功。Python ミラーで header/record オフセットと期待値を照合。テスト実行・実機表示は未実施。

## 2026-09-05 — 空間音声の時刻評価と広域出力の境界

- **関連:** `Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`
- **事実:** 音声要求は start frame を持つが、`getGlobalTransform4x4()` は現在タイムライン時刻を参照する。SpatialRenderer の係数配列は 8 要素で、出力 scratch のチャンネル数を元にループする。
- **今回の対応:** SpatialAudio の出力 scratch を空から開始し、mono/stereo 素材の stereo preview に限定した。
- **懸念（未検証）:** 先読み／export 時の位置評価が要求音声時刻からずれる可能性がある。汎用レンダラーへ 12ch scratch を直接渡す将来経路では固定長配列の範囲外アクセスに注意が必要。
- **価値／次の確認:** 任意時刻の親子 transform 評価を共通 API で提供できるか確認する。7.1.4 接続前に出力 layout と係数容量の契約を確定する。


## 2026-09-10 — プリコンポーズ改善 B/F 採用と A/C/D/E 見送り

- **関連:** `docs/planned/MILESTONE_PRECOMPOSE_BREADCRUMB_DUPLICATE_2026-09-10.md`、`docs/planned/COMPOSITION_PRECOMPOSE_ANALYSIS_2026-04-17.md`、`docs/planned/GROUP_CONTAINER_MIGRATION_PLAN_2026-08-27.md`、`docs/analysis/AE_PAIN_POINT_IMPROVEMENT_MAP_2026-08-13.md`。
- **確認できた事実:** ユーザー指示は B (Breadcrumb + In-place) と F (Duplicate Deep / Instance / Un-precompose) を採用、残りは検討。`PreComposeManager::precompose()` は未実装部あり、`GroupContainer` 移行は Phase 0/1 済み・実体化未着手。Viewport モックは周辺 UI のみ採用でキャンバス内変更不可。
- **対応:** B/F の契約定義 milestone を `docs/planned/` に新規作成。親ゴースト表示は既定範囲外、A/C/D/E は着手条件付きの検討事項として記録。子 repo 変更・ビルド実行なし。

## 2026-09-10 — AE メニューバー不満の付録化

- **関連:** `docs/planned/MILESTONE_PRECOMPOSE_BREADCRUMB_DUPLICATE_2026-09-10.md` 付録。
- **確認できた事実:** ユーザー承認で前回回答のメニュー別不満・改善を同 milestone へ付録追記した。B/F 本体との対応表付き。コード変更なし。
- **価値／懸念:** Navigate と複製命名を B/F と同一に縛り、メニュー肥大・用語分裂を防ぐ。コマンドパレット・配置保存は範囲外として分離。

## 2026-09-10 — AE タイムライン不満の付録化

- **関連:** `docs/planned/MILESTONE_PRECOMPOSE_BREADCRUMB_DUPLICATE_2026-09-10.md` 付録。
- **確認できた事実:** ユーザー承認でタイムライン不満・改善を同 milestone へ付録追記した。左ペイン痩身・Baseline 維持・B/F 連動・ホットパス制約を含む。コード変更なし。
- **価値／懸念:** 親子切替・複製命名を B/F と同一に縛り、Timeline 肥大・用語分裂を防ぐ。時間集約・検索は定義のみで独立スライス候補。
- **次に確認:** 付録が肥大化したら独立 milestone へ分割する。

- **次に確認:** 付録が肥大化したら独立 milestone へ分割する。

- **価値／懸念:** 往復コストと複製事故の解消に絞り、GroupContainer 移行との依存を契約先行で吸収する。一方 F-2 実装は PreCompose 未実装部と Container 実体化に依存するため、定義だけでは動作確認できない。
- **次に確認:** B-2 共有状態の保存先、F-1 既定選択、Tracker 状態除外の維持をレビューする。

## 2026-09-05 — 点群のvoxel間引き (LOD最小スライス)

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (decimatePointCloud)、	ests/ArtifactCore/MeshImporterPlyTest.cpp
- **事実:** 取込上限200万点でも描画は32768クロスへstride間引きするだけで、メモリ (2M点で約100MB超) と形状代表性に課題があった。octree/out-of-core は別規模の設計になる。
- **対応:** 頂点のみ点群に voxel 間引きを追加 (budget 262144、初回cellは体積/上限の立方根、不足時は最大8回半分化、セル先着・入力順決定性・色連動、NaN/極端座標ガード)。PLY/LAS 両経路で適用し、間引き時は qInfo で before→after を出す。gtest に30万点バイナリの間引き・決定性テストを追加。
- **検証:** ArtifactCore・ArtifactCoreFile 増分ビルド成功 (Property.ixx の C5202 は既存)。Python ミラーで cell・kept数・先頭点保持を確認。テスト実行・実機表示は未実施。

## 2026-09-05 — 点群テスト13件が実実行で成功・QDataStreamの罠

- **関連:** 	ests/ArtifactCore/MeshImporterPlyTest.cpp (9件)、	ests/ArtifactCore/MeshImporterLasTest.cpp (4件)
- **事実:** ARTIFACT_BUILD_TESTS=OFF のため再configure (-DARTIFACT_BUILD_TESTS=ON) して実行。初回は binary 4件が失敗したが、原因はローダーではなく QDataStream の既定 DoublePrecision (float が8バイトで書かれる) だった。デバッグテストで bodyHex を確認し、setFloatingPointPrecision(SinglePrecision) で解決。LAS側は整数/double明示書きのため当初から成功。
- **対応:** binary fixture 全件に SinglePrecision を指定、デバッグテストは削除。PLY 9件・LAS 4件の全成功を確認。
- **教訓:** バイナリ fixture を QDataStream で書く場合は SinglePrecision を必ず指定すること。
- **未検証:** 実機での点群表示・選択・Inspector (アプリ全体ビルドは未実施)。

## 2026-09-05 — 3D Stroke (PathTube trim) と Lux (Light glow) の最小実装

- **関連:** ArtifactCore/.../Procedural3DGenerators.{ixx,cppm}、Artifact/src/Layer/ArtifactProcedural3DLayer.cppm、Artifact/src/Layer/ArtifactLightLayer.cppm、Artifact/include/Layer/ArtifactLightLayer.ixx、	ests/ArtifactCore/PathTubeTrimTest.cpp
- **事実:** PathTube に trim 概念が無く、Light層に可視 glow が無かった。ArtifactLightLayer::shouldIncludeInFinalRender() は false のため Light の draw 内容はビューポート限り。3D層描画時は Scoped3DLayerCamera により particle 3D カメラが有効 (ArtifactCompositionRenderController.cppm:7924) のため、Light の draw 内 drawParticles は world 座標で解釈される。
- **対応:** PathTube に 	rimStart/trimEnd (既定0/1で旧挙動一致、taper/twist/UV は trim 範囲追従、反転は空メッシュ) を追加し、層の JSON・Inspector・setter を配線。Light層に Light/Glow・Glow Size・Glow Intensity (既定ON/1/1、Point/Spot/Area のみ、range/cone連動サイズ・additive billboard 1 sprite) を追加し、JSON・Inspector・setter を配線。最終レンダーへの glow は render-queue 側の別件として残す。
- **検証:** ArtifactCore ビルド、ArtifactProcedural3DLayer・ArtifactLightLayer 単体TUコンパイル、trim gtest 4件全成功。実機表示は未実施。

## 2026-09-05 — Mesh法線生成とbounds sphere

- **関連:** ArtifactCore/include/Mesh/Mesh.ixx、ArtifactCore/src/Mesh/Mesh.cppm、ArtifactCore/src/Geometry/MeshImporter.cppm (loadPly)、	ests/ArtifactCore/MeshGeometryTest.cpp
- **事実:** PLYポリゴンは法線ゼロで書き出され、ライティングが壊れていた (STLはfacet法線を自前計算、ufbxはソース保持)。Mesh にトポロジからの法線生成が無く、bounds も AABB のみだった。
- **対応:** Mesh::computeVertexNormals() (面積重みスムーズ法線、縮退面スキップ、未使用頂点は(0,0,1)、revision bump) と oundingSphereCenter/Radius() (AABB版、updateBoundsで同時更新) を追加。loadPly は面あり時のみ法線生成。gtest 5件 (quad/縮退/点群no-op/sphere/PLY経由) を追加。
- **検証:** MeshGeometryTest 4件・MeshImporterPlyTest 10件の全成功を確認。

## 2026-09-05 — 真の弧長サンプリングと解析的接線

- **関連:** ArtifactCore/.../BezierCalculator.{ixx,cppm}、ArtifactCore/.../BezierPathSampler.{ixx,cppm}、	ests/ArtifactCore/BezierArcLengthTest.cpp
- **事実:** evaluatePath はセグメント毎t均等であり、sampleEquidistant/sampleByCount/sampleWithTangents の「等間隔」は不正確だった (不等長セグメントで偏る)。接線はeps差分近似だった。外部呼出しはサンプラ内部のみで、変更の波及は閉じている。
- **対応:** 弧長テーブル (32分割/segment・二分探索・t補間) を追加し、sampleEquidistant/sampleByCount/sampleWithTangents を弧長経路へ切替 (シグネチャ不変)。pointAt はt均等のまま互換維持し、pointAtArcLength/	angentAtArcLength/sampleArcLength を新設。evaluateTangent (解析的導関数・縮退時(1,0)) を追加し、	angentAt を含め差分近似から置換え。縮退パスは有限値を返す。
- **検証:** gtest 6件 (不等長均等・端点・直線接線・縮退・閉ループ・解析接線) の全成功を確認。

## 2026-09-05 — CatmullRom/Hermite実装とbezierEvaluateの式バグ修正

- **関連:** ArtifactCore/include/Geometry/Interpolate.ixx、	ests/ArtifactCore/KeyframeSplineTest.cpp
- **事実:** InterpolationType::CatmullRom/Hermite は宣言のみで dispatch は Linear 落ちだった。また ezierEvaluate の Newton ソルバとy評価式に余分な mt3 項があり (P0=(0,0) なのに +mt3)、全Bezierイージングがずれていた。テストが easy-ease 中点 5.0 に対し 6.25→39.32 を返したことで発覚。Pythonミラーで正値 (0.1292/0.5/0.8708) を確認。
- **対応:** hermiteInterpolate/catmullRomInterpolate を追加し、KeyframeInterpolator::evaluate で隣接キー参照の CR (均一) と有限差分接線 Hermite (非均一対応) を実装。2点版 interpolate() は Linear 維持。bezierEvaluate は mt3 を除去し正規形へ。gtest 8件を追加。
- **検証:** 8件全成功。既存 Linear/Bezier 挙動の回帰テストを含む。Bezier全般の値が変わるため、既存プロジェクトの見た目差分は実機で要確認。

## 2026-09-05 — CatmullRom/Hermiteのメニュー露出

- **関連:** Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm、Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm
- **事実:** pplyInterpolationToSelectedKeyframesImpl は type を汎用設定するため、メニュー追加だけで CR/Hermite が適用可能だった。キーフレーム色・ラベル・形状は型別 switch で、新規型は default 落ちだった。
- **対応:** Animationメニュー (キーフレーム補間) と Timeline右クリック (Interpolation) に Catmull-Rom/Hermite を追加。ショートカットは追加しない。キーフレーム色 (紫系2色)・ラベル・形状 (六角/五角) を追加。EasingLab は単区間previewのため対象外。
- **検証:** 両TUの単体コンパイル成功。実機のメニュー表示・適用・保存復元は未実施。

## 2026-09-05 — 不足easingの一括実装

- **関連:** ArtifactCore/include/Geometry/Interpolate.ixx、	ests/ArtifactCore/EasingFunctionsTest.cpp
- **事実:** enumにありながら dispatch が Linear 落ちだった型が多数 (Smooth/EaseOutIn/Quadratic/Cubic/Quartic/Quintic/Exponential/Logarithmic/Sine/Circular/Cosine)。2点版 interpolate() の Bezier は 
eturn start のままだった。
- **対応:** 純alpha系19種を追加 (CubicIn/InOut、Quartic/Quintic各3種、SineIn/InOut、Circular各3種、Exponential各3種、Logarithmic、Cosine、EaseOutIn、Smooth; Quadratic→EaseIn、Cosine/Smoothは同一曲線)。新規は alpha clamp 付き。Bezierスタブは Linear フォールバックへ (呼び出し側はbezierInterpolateへ迂回済み)。gtest 5件 (端点・既知中点・dispatch・Keyframe経由・範囲外有限) を追加。
- **検証:** 5件全成功。Spring/SmoothDamp (状態持ち)、CustomCurve/Polynomial (係数要)、色文脈系、2点Hermite/CR は対象外のまま。

## 2026-09-05 — EasingLab候補の拡張とBezierプレビュー修正

- **関連:** ArtifactCore/include/Animation/EasingCurveUtil.ixx、Artifact/src/Widgets/Timeline/EasingLabWidget.cppm、	ests/ArtifactCore/EasingLabCurveTest.cpp
- **事実:** EasingLab候補は16種で、新規 easing (Quartic/Quintic/Sine/Circular 等) が未露出だった。同ファイルの Bezier プレビューにも ezierEvaluate と同じ余分な mt3 項があった。
- **対応:** EasingType に EaseOutIn/Smooth/Quartic/Quintic/Sine/Circular を追加し、評価・名称・Interpolation対応・候補一覧を配線。Bezier の mt3 を除去。EasingLab の対応ラベルを追加。gtest 4件を追加。
- **検証:** 4件全成功、EasingLabWidget 単体TUコンパイル成功。実機のダイアログ表示は未実施。

## 2026-09-05 — マット検証 (順序・premult・GPU上限・輝度)

- **関連:** ArtifactCore/.../LayerMatte.{ixx,cppm}、Artifact/src/Render/ArtifactCompositionViewDrawing.cppm、Artifact/src/Render/ArtifactRenderQueueService.cppm、	ests/ArtifactCore/MatteStackTest.cpp
- **事実:** stack順序 (参照順・初回引継) と premult 一貫性 (RGBA一律乗算) は CPU/GPU/Core で一致。相違点: (1) GPU は3マット超で stack 全体を無適用化 (CPU は全数適用)、(2) GPU は Stretch 以外・ネスト/3D/adjustment ソースで stack 全体を無適用化、(3) Core evaluateMatteStack は alpha のみで luma 未対応、(4) 実働経路 (preview CPU・GPU queue) は BT.601 で一致する一方 Core 既定は 709、(5) Core MatteStackMode に Difference が無かった。evaluateMatteStack/MatteEvaluator に実働呼出しは無い。
- **対応:** MatteStackMode::Difference を Core + view の switch に追加 (末尾追加・既定値不変)。>3マット時に preflight Warning 診断を追加。gtest 11件 (sample/combine/apply/順序/skip/反転/空passthrough/roundtrip) を追加。
- **検証:** 11件全成功。TU単体コンパイル成功。実コンポでの受入れ・GPU実機は未実施。

## 2026-09-05 — マスク編集ハンドルは画面座標で判定する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、VP マスク編集
- **事実:** レイヤーローカル座標でハンドルのヒット距離を測ると、レイヤーの非一様スケールやズームにより見た目のクリック領域と判定がずれる。フェザー値ゼロでは実ハンドルが頂点と重なり、直接ドラッグを開始できない。
- **対応:** ハンドル判定を viewport のピクセル距離に統一し、競合時は最も近い候補を選ぶ。ゼロ・フェザーには画面上だけの最小距離ハンドルを表示し、ドラッグ開始点からの法線方向差分でフェザーを立ち上げる。
- **価値／次の確認:** マスク頂点とセグメントのヒット判定も同じ画面距離モデルへ段階的に揃えると、極端なレイヤー変形時の操作一貫性をさらに高められる。実機で非一様スケールしたレイヤーの操作感を確認する。

## 2026-09-05 — 生成プリセットの既存基盤と拡張境界

- **関連:** `Artifact/include/Project/ArtifactPresetManager.ixx`、`Artifact/include/Layer/ArtifactGenerationPreset.ixx`
- **事実:** 既存の `ArtifactPresetManager` は平面・画像・Shape・Text と基本マスクの作成定義を JSON 化できるが、レイヤーエフェクトと Text Animator を同一トランザクションに含める表現を持たない。
- **対応:** レイヤー／マスク／エフェクト／Animator を同じ JSON レシピで表す `ArtifactGenerationPreset` を追加し、`New > Presets` 実行時は既存 `AddLayerCommand`・`MaskEditCommand`・`AddLayerEffectCommand` を `MacroUndoCommand` に集約した。
- **価値／次の確認:** 基本作成プリセットと生成プリセットの JSON 統合は将来候補。現時点では両者の機能範囲が異なるため、既存スキーマを変更せず併存させている。実機で redo/undo 後の選択状態とユーザー JSON 再読込を確認する。

## 2026-09-06 — マルチスレッド基盤の段階移行 1+2: ThreadPoolのTBB shim化とTaskflow TaskSystemの併存

- **関連:** ArtifactCore/include/Common/ThreadPool.ixx、ArtifactCore/include/Common/TaskSystem.ixx、ArtifactCore/CMakeLists.txt、ArtifactCore/cmake/ArtifactCoreSources.cmake、cpkg.json。
- **事実:** ThreadPool は集中キュー+mutexでfine-grainedに弱く、Core.Parallel は既に tbb::parallel_for で TBB 依存だった。vcpkg には tbb のみで taskflow は未導入。ArtifactCore→ArtifactCoreEnvironment は静的リンクで参照循環しないが、モジュールは /reference + OBJECT_DEPENDS の既存パターンを踏襲する必要があった。
- **対応:** 1) ThreadPool を TBB task_arena/task_group 背景の shim に置換。API(enqueue/enqueueTask/waitAll/globalInstance)は完全維持し、内部のみ work-stealing化。concurrency は hardware_concurrency()で統一。globalInstance は deprecated 付与し DAG は TaskSystem 推奨へ誘導。2) Taskflow (header-only) を vcpkg.json に追加し、Core.TaskSystem (tf::Executor ラッパ、async/silent_async/run/corun/wait_for_all、for_each_index ヘルパ)を新設。3) CMake は Taskflow::Taskflow を ArtifactCore にリンクし、TaskSystem.ixx を CORE_MODULES へ追加。configure/build の疎通を確認。無関係な作業中変更(ArtifactRegex等)によるビルド破綻は一時退避で分離し、検証後は復元せず除外。
- **価値／懸念:** 既存呼び出しは再コンパイル不要で即座に work-stealing の恩恵。Taskflow は DAG/協調実行(corun)でデッドロック回避が可能。3プール(QThreadPool/TBB/Taskflow)が一時併存するためスレッド過剰生成に注意が必要だが concurrency 統一で緩和。P2300 全面採用は見送り、データ並列は TBB、DAG は Taskflow の分担で最新知見を段階導入。
- **次に確認:** 実機での ThreadPool 呼び出しの順序依存有無、TaskSystem を用いた matte/DAG の PoC、QThreadPool との最終一本化、P2300 のコンパイラ対応推移。

## 2026-09-06 — TaskSystem DAGのマットPoC

- **関連:** ArtifactCore/include/Common/TaskSystem.ixx、	ests/ArtifactCore/TaskSystemMattePoCTest.cpp。
- **事実:** ThreadPool はデータ並列向け、TaskSystem は DAG/協調実行向けに分担したが、マット処理は依然逐次の evaluateMatteStack のみだった。Taskflow の for_each_index はモジュール境界で ODR/link 問題を起こしやすい(今回も LNK2019)。
- **対応:** PoC として TaskSystem を用いたマット並列評価を gtest 4件で実証。DAG依存(A→B,C→D)、corun デッドロック回避、Taskflow DAGで3ソースのマスク生成を並列化して逐次結果と一致(64x64 Add, 期待1.0)、並列 for 相当の動作。task_for ヘルパはモジュール内テンプレートのリンク問題で一旦除去し、直接 emplace で代替。テストは <taskflow/taskflow.hpp> を直接 include してモジュール透過問題を回避。
- **検証:** 4/4 passed。MatteStack の実運用への組み込みは未着手だが、PoCで並列化の等価性と協調実行の有効性を確認。

## 2026-09-06 — Phase2: 専用プールのTBB一本化 (AsyncAssetRead / RenderScheduler)

- **関連:** Artifact/src/IO/AsyncAssetReadScheduler.cppm、Artifact/src/Render/ArtifactRenderScheduler.cppm、ArtifactCore/include/Common/TaskSystem.ixx。
- **事実:** 専用プール2つが QThreadPool に依存していた。AsyncAssetRead は I/O バーストで priority 付き start、RenderScheduler は QThreadPool::start で並列実行し invokeMethod でメインへ帰着。どちらも QThread のイベントループには依存せず、ScopedThreadName/Trace は TBB スレッドでも有効。
- **対応:** AsyncAssetRead: QThreadPool → TaskSystem (1-8 arena), setMaxThreadCount/expiryTimeout を除去、waitForDone→wait_for_all、priority は一旦無視 (別arenaで優先度エミュレート可能だが初期は均一)。RenderScheduler: unique_ptr<QThreadPool> → TaskSystem, ensureTaskSystem()で遅延生成+concurrence変更時は再生成、maxThreadCount 参照を concurrency() に、start→silent_async に。Thread.Helper の ScopedThreadName は維持。
- **検証:** cmake configure 成功 (33s)。ビルドはユーザ指示で中断、オブジェクト個別の dyndep 生成は確認。QtConcurrent 13箇所・QThreadPool globalInstance は Phase3 で残置。

## 2026-09-06 — Phase3ヘルパ: QFutureWatcher代替の asyncPostToObject

- **関連:** ArtifactCore/include/Common/TaskSystem.ixx。
- **事実:** 残る13箇所の QtConcurrent::run(&sharedBackgroundThreadPool()) は QFutureWatcher::finished でメインへ帰着していた。TaskSystem::async は std::future を返すため QFutureWatcher と非互換。
- **対応:** TaskSystem に asyncPostToObject<T>(QObject* context, work, onFinished) を追加。TaskSystem::silent_async で実行し、結果を QPointer ガード付きで QMetaObject::invokeMethod(QueuedConnection) で context スレッドへ配送。QThreadPool 依存の prewarm/専用プールは Phase1/2 で除去済みのため、残りはこのヘルパで1行置換可能。ArtifactCore ビルド確認済み。
- **次に確認:** VideoLayer/ImageLayer/AssetBrowser 等の各サイトを同ヘルパで順次置換し、QThreadPool globalInstance への依存を完全に除去するか検証。
## 2026-09-06 — GPUジョブプール基盤の初期境界

- **関連:** `ArtifactCore/include/Graphics/GPUThreadPool.ixx`、`ArtifactCore/src/Graphics/GPUThreadPool.cppm`
- **事実:** GPU側には個別のCompute dispatch経路はあるが、共通のジョブ投入・容量制限・診断契約は無かった。
- **対応:** Diligentを公開APIに露出させないホスト側キューを追加。`enqueue`、`drain(executor)`、キャンセル、ジョブハンドル、キュー統計を提供し、既存のレンダリング経路には接続していない。
- **価値／懸念:** 将来のDiligent Compute executorを差し込めるが、現時点の完了状態はGPU fence完了ではなくexecutor受理結果である。GPU非同期完了を扱う段階でfence世代を追加する必要がある。
- **次に確認:** 実際のDiligent command recording／submission境界と、D3D12・Vulkan共通のfence再利用契約を確定する。

## 2026-09-06 — View メニューの情報設計整理

- **関連:** `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`、`Artifact/src/Widgets/ArtifactMenuBar.cppm`。
- **事実:** View の直下にはズーム、viewport 保存、比較、preview 品質、表示オーバーレイ、Rig 操作、workspace、個別パネル起動、パネル一覧、追加アセットブラウザ、secondary preview が同居する。さらに `Color Science` action は同一の QAction が直下へ二度追加されている。パネル追加／再表示は専用の `Window Panels` submenu とメニューバー右上 `+` にも既に存在する。
- **対応:** 直下を Navigation / Preview / Overlays / Rig / Workspace / Window Panels に限定し、Grid & Snap と Rig 操作を各 submenu に入れた。個別パネル起動と新規 Asset Browser は Window Panels 内の Utility Panels に統合し、重複していた Color Science entry は一つにした。既存 QAction とショートカット、Dock registry の再表示経路は維持した。
- **価値／次の確認:** 直下項目の走査負荷と重複を減らした。ビルド・runtime 確認は未実施のため、メニュー階層、アクセラレータ、パネル作成・再表示、狭幅表示を実機で確認する。


## 2026-09-06 — エフェクト詳細の共有所有と複数展開

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/src/Widgets/ArtifactInspectorInteraction.cppm`。
- **事実:** Effects面は1つのPropertyWidgetとSurfaceFX専用編集部を共有している。今回のインライン化でも1件だけを展開し、リスト項目の削除に編集部の寿命を連動させていない。
- **仮説・未検証:** 将来複数エフェクトを同時展開する場合、単純な編集部の複製はfocusedEffectIdや専用編集操作の対象を混線させる可能性がある。
- **価値／次に確認:** 同時展開を追加する前に、編集対象・コールバック・所有権を各エフェクト単位に分離できるか調べる。


## 2026-09-06 — 左ペインのキー操作とUndo経路

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の `togglePropertyKeyframeAtCurrentTime` と今回追加した値編集。
- **事実:** 既存の菱形クリックはプロパティのキーを直接追加・削除する。今回の値編集は既存のUndoコマンド／KeyframeModelを利用するため、同じペイン内でも操作経路が異なる。
- **懸念・未検証:** 菱形クリックのUndo体験が値編集と一致しない可能性がある。今回はリデザイン対象の既存操作を保ち、この経路は変更していない。
- **次に確認:** 菱形のキー追加・削除のUndo/Redoを確認し、別途KeyframeModelの共通経路へ統一する範囲を判断する。

## 2026-09-07 — ツールバーの表示モード操作の実行先

- **関連:** `Artifact/src/Widgets/ArtifactToolBar.cppm` の Normal / Grid / Detail actions。
- **事実:** これらは既存のQActionGroupで選択状態を保持するが、同ファイル内に `viewModeChanged` の発火や表示サービスへの委譲が見当たらない。今回の配置変更では既存QActionをメニューに再利用した。
- **懸念・未検証:** 表示切替が実際のビューに反映されない可能性がある。今回の外観変更とは分けて確認する必要がある。
- **価値／次に確認:** 実アプリで3種の表示操作を確認し、必要なら既存の表示コマンドとの対応を調査する。

### 2026-09-07 — カラーピッカーの色空間契約
- 関連: Artifact/src/Widgets/Dialog/FloatColorPickerHooks.cppm、ArtifactCore/src/Color/LabColor.cppm、XYZColor.cppm。
- 確認事実: Lab/XYZの既存変換はsRGB符号値・D65を前提とし、戻りRGBを0–1にクリップする。ピッカーのFloatColor引数には色空間タグがない。
- 未検証: 全呼び出し元が同じ符号値契約であるかは未確認。
- 懸念・次の確認: 将来のHDRや作業色空間対応時は、呼び出し元の色空間を明示してから変換へ渡す必要がある。今回の追加UIにはsRGB/D65基準を明記した。

### 2026-09-07 — 3D回転の操作数学と表示の区別
- 関連: Artifact/src/Widgets/Render/Artifact3DGizmo.cppm の updateDrag。
- 確認事実: 現行回転は開始角との差をEuler成分へ加算し、スナップは各Euler成分へ適用する。atan2境界の差の連続化はこの経路にはない。
- 未検証: ±180度をまたぐドラッグ、傾いたView回転、非ゼロ開始角でのスナップの操作整合性。
- 懸念・次の確認: 今回は外観変更のため数学を変更していない。上記操作を再現してから、必要なら回転更新とピボット更新の一致を別途修正する。
### 2026-09-07 — MSVC IFC C1001 と initializer-list append
- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `cloth3DDeformationMesh()`。
- **確認事実:** C1001 の報告位置は namespace 終端直後の空行だが、直前の追加処理には import された ClothSolver3D の値を `std::vector::insert(..., { ... })` で追加する箇所があった。
- **仮説・未検証:** 大規模な module implementation unit での initializer-list overload 解決が MSVC の IFC 処理を誘発している可能性がある。`push_back` の明示列へ分解して回避した。
- **価値／次に確認:** 再ビルドで C1001 が消えるか確認し、再発時は Cloth3D 実装を別の既存 `.cppm` 境界へ移す切り分けを行う。
- **2026-09-08 追記・確認事実:** 同じ C1001 が継続し、当該実装unitには未使用の `Physics2D`、`Artifact.Composition.Nodes`、`Artifact.Effect.Generator.Cloner` import が残っていた。利用箇所がないことを静的確認して除去した。
- **次に確認:** この依存グラフ縮小後も再現する場合は、次の候補を当てずっぽうに変えず、物理・component runtimeの大きな実装ブロックを既存moduleの実装unitへ分離する。

### 2026-09-08 — setComposition overload の再入リスク

- **関連:** `Artifact/src/Layer/ArtifactAdjustableLayer.cppm`、`Artifact/src/Layer/ArtifactPaintLayer.cppm`、`Artifact/src/Layer/ArtifactSwitchLayer.cppm`。
- **確認事実:** `QObject*` overload が `void*` overload を呼び、その `void*` overload が `ArtifactAbstractLayer::setComposition(void*)` を呼ぶと、base 実装内の virtual `QObject*` dispatch により派生 overload へ戻る。Adjustment Layer の実行スタックでこの循環を確認した。
- **対応:** Adjustment Layer は base の `QObject*` 実装を明示呼出しするよう修正した。
- **懸念・次に確認:** Paint Layer と Switch Layer に同じ実装パターンが残る。今回の依頼範囲外のため未変更であり、各レイヤー追加・composition attach の実機確認後に同じ修正を適用するか判断する。

### 2026-09-08 — エフェクトのGPU常駐チェーン契約

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Effects/ArtifactAbstractEffect.cppm`、`ArtifactCore::LayerBlendPipeline`。
- **確認事実:** Composition View の通常レイヤー用 raster surface builder は CPU の `ImageF32x4_RGBA` を入出力とする。一方、AUTO/GPU の各エフェクト実装は入力を個別アップロードし、dispatch後に staging texture、`WaitForIdle()`、CPU readbackを行うため、複数エフェクトでは同期往復が段数分発生する。調整レイヤーの対応済みpointwise処理だけは `LayerBlendPipeline` 内でGPU常駐する。
- **対応:** CPU所有のsurface builderではCPU実装を明示利用し、GPU専用エフェクトだけ従来経路へフォールバックすることで同期往復を除去した。さらに通常レイヤーでも、完全に表現できる Exposure / Hue・Saturation / Levels / Brightness / White Balance(tintのみ) / Invert / Grayscale を既存 `LayerBlendPipeline` のF32 SRV/UAV pointwise passへ接続した。
- **価値／次に確認:** 通常レイヤーの対応カラー処理は `layerFloat → pointwise → matte → blend` でGPU常駐する。region、effect mask、mix、未対応パラメータ、CPU明示、GPU専用エフェクトは互換性優先で既存経路を使う。残る根本拡張は、任意のエフェクトAPIへSRV/UAVまたはrender-graph resourceを渡すGPU常駐チェーン契約である。D3D12/Vulkan共通のDiligent境界、ping-pong texture寿命、mask/region/mixの適用順を先に確定する。
## 2026-09-08 — Composition controller の旧画像境界と色順

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `buildRasterizedSurfaceBuffer`、`ArtifactCore/src/Image/ImageF32x4_RGBA.cppm` の `setFromCVMat`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx`。
- **確認事実:** controllerのARGB32画像はCV_32FC4へ数値変換した後、descriptorなしのsetFromCVMatへ渡る。一方、同関数はCV_32FC4をRGBAとして記録する。controllerのコメントはupload側でBGRA変換すると説明しており、現行のdescriptor依存変換との不整合がある。
- **未検証:** 実機で赤青が反転する条件と、もう一つのCompositionViewDrawing経路との差。新しい単色GPU経路では従来controllerの格納順・transfer境界を維持し、この調査を色補正変更に広げていない。
- **価値／次に確認:** 赤・青・半透明の固定入力で両描画経路を比較し、色descriptor修正を別途扱う。GPU常駐化の性能比較と色仕様修正を混ぜない。

## 2026-09-09 — Light Layer 作成時の初期設定境界

- **関連:** `Artifact/src/Widgets/Dialog/CreateLightLayerDialog.cppm`、`Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/include/Layer/ArtifactLightLayer.ixx`。
- **確認できた事実:** Light Layer は Point / Spot / Parallel / Ambient / Area、色、強度、距離、Spot cone、Area shape/size、影を既存APIとして持つ一方、2つの作成導線はいずれも `Light 1` の既定値を即時作成していた。
- **対応:** 作成ダイアログで初期値だけを選べるようにし、GOBO / Glow / Light Linking は既存Inspector責務として残した。要約表示はダイアログ入力値から導出するだけで、GPU preview / readback / texture確保を増やさない。
- **価値／懸念:** 作成時に重要な種別差を明示できるが、作成直後の設定適用は既存Camera作成と同じ選択レイヤー取得経路に依存する。
- **次に確認:** 実機でLayerメニュー／Composition Editor双方から各5種を作成し、選択同期、保存・再読込、Spot/Areaの描画、影の有効状態を確認する。

## 2026-09-09 — Quick Layer 作成ダイアログの再配置境界

- **関連:** `Artifact/src/Widgets/Dialog/QuickLayerCreationDialog.cppm`、`docs/design/quick-layer-creation-dialog/README.md`。
- **確認できた事実:** 既存ダイアログは Source、Size、Mask、Envelope、Placement を縦一列に表示していたが、作成オプションと `QSettings` 保存は UI の並び順に依存しない。
- **対応:** Source / Size と Mask / Placement を二列化し、Entry / Exit Envelope を下段に残した。入力値、既存の接続、設定キー、作成オプション、画像選択経路は変更していない。
- **価値／懸念:** 視線移動を減らせる一方、狭い画面では最小幅が従来より広くなる。
- **次に確認:** 実機でPlane/Image切替時の画像入力有効化、各Placement、Mask、Entry/Exitの保存復元と作成結果を確認する。

## 2026-09-09 — ダイアログモック反映時の選択モデル維持

- **関連:** `ArtifactResolutionRemapDialog`、`ArtifactImportAssetsDialog`、`ArtifactObjectPickerDialog`。
- **確認できた事実:** 3ダイアログとも表示構造と選択結果の取得が分離されており、追加イベント配線なしでレイアウトと選択面を整理できる。Resolution Remap の policy は index 順が `RemapPolicy` の列挙値と一致する。
- **対応:** Remap policy を同じ順序の単一選択リストへ置換し、Import と Object Picker は既存モデル／接続を保ったまま視覚階層のみ変更した。
- **価値／懸念:** モックに近い一覧性を得つつ処理境界は維持できる。Remap policy の列挙順変更時はリスト構築と結果変換を同時に確認する必要がある。
- **次に確認:** 実機でキーボード選択、ダブルクリック、Cancel／Skip、各policyのApply結果、狭い画面での最小サイズを確認する。

## 2026-09-09 — Point / Planar Tracker のモード永続化

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm` の `setTrackerType`、`setSettings`、`fromJson`。
- **確認できた事実:** トラッカー種別はトップレベルの `trackerType` と設定内の `settings.type` の二箇所へ保存される。切替 setter が片方だけを更新すると、保存・再読込時に Point / Planar が食い違う余地があった。
- **対応:** setter 同士で両フィールドを同期し、旧形式で `settings.type` が欠落した JSON はトップレベル種別を既定値として復元するようにした。
- **価値／懸念:** UI のモード切替とプロジェクト再読込の整合性を保てる。既存ファイルの不正な種別値は従来どおり setter の範囲 clamp に委ねる。
- **次に確認:** 実機で Point / Planar 切替後に保存・再読込し、ツールバー選択状態、検索領域、結果フレームが同じモードで復元されるかを確認する。

## 2026-09-09 — Tracker キャプチャ失敗の受け入れ境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `trackerCaptureNextFrame`。
- **確認できた事実:** オフスクリーンレンダラーが `QImage` を返せない場合でも、従来はフレーム数だけを進めて不完全な画像列を解く可能性があった。
- **対応:** null 画像を検出した時点でキャプチャとジョブを停止し、ユーザーへ失敗フレームを表示する。部分的なトラッキング結果を完了扱いにしない。
- **懸念／次に確認:** 実機でGPU初期化失敗・対象レイヤー範囲外・画像シーケンス欠落の各ケースを確認し、必要なら再試行導線を追加する。

## 2026-09-09 — Tracker point ID の固定値依存

- **関連:** `MotionTracker::addTrackPoint`、`ArtifactPointTrackerGizmo`、`ArtifactPointTrackerTool`。
- **確認できた事実:** `MotionTracker` の点ID採番は1始まりだが、軌跡表示・補正・単一点書き出しがID `0` を固定参照していたため、最初の点が表示／適用されない経路があった。
- **対応:** Coreに最初の登録点の実ID取得APIを追加し、GizmoとApply経路がそのIDを使うようにした。Toolの既定値も「最初の登録点を解決」に変更した。
- **価値／次に確認:** 既存の1始まりIDとJSON復元を壊さず、Point Trackerの軌跡・補正・Bakeが同じ点を参照できる。複数点の明示選択UIは別途検討する。
- **追記:** JSON復元時に次の点／領域IDも最大既存IDの後ろへ再同期し、追加登録時のID衝突を避けるようにした。

## 2026-09-09 — Motion path と結果フレームの対応

- **関連:** `ArtifactPointTrackerGizmo` の軌跡描画・PathPoint補正。
- **確認できた事実:** `motionPath(pointId)` は点が存在するフレームだけを返すため、path index と `result.frames` index は常に一致するとは限らない。
- **対応:** 点IDの存在を基準に結果フレームを解決してから信頼度表示・現在点表示・補正時刻を決めるようにした。
- **価値／次に確認:** 欠落点を含む結果でも別フレームへ補正を書き込まない。欠落フレームの補間表示は既存 `TrackResult::interpolateAt` の責務として残す。

## 2026-09-09 — Tracker入力のレイヤー境界

- **関連:** `ArtifactCompositionRenderController::trackerCaptureNextFrame`、`OffscreenCompositionRenderer`。
- **確認できた事実:** 追跡キャプチャがコンポジション全体を入力にしていたため、選択画像レイヤー以外の模様が特徴点候補へ混ざる可能性があった。
- **対応:** 指定レイヤーだけを透明背景へ描画して読み戻す `renderLayerToQImage` を追加し、Point／Planar Trackerのキャプチャを選択画像レイヤーに限定した。
- **懸念／次に確認:** レイヤー変換・親子階層・マスクを含む画像レイヤーで、VP座標と読み戻し画像の座標が一致するか実機で確認する。
- **追記:** レイヤー専用読み戻し後は元の `currentFrame()` へ戻し、キャプチャだけで編集対象レイヤーの表示時刻を変更しないようにした。

## 2026-09-09 — TrackPoint のVP操作面

- **関連:** `ArtifactCompositionEditor` の上部ツールバーと `compositionTrackerPanel`。
- **確認できた事実:** 追跡コマンド自体はツールバー／コンテキストメニューに接続済みだったが、モックアップの右側Tracker操作面は未実装だった。
- **対応:** VP内にTrackerパネルを追加し、Point設定、Planar切替、前後／全範囲解析、停止、問題フレーム確認、Position／Anchor／全ポイント／Corner Pin適用を既存controllerへ接続した。信頼度・問題数・結果フレーム数はcontrollerの読み取りAPIで表示する。
- **懸念／次に確認:** パネルはVP上に重ねる方式のため、Four-Up時の占有範囲と狭い画面での折り返しを実機確認する。QtCSSや新規signal/slotは追加していない。
- **追記:** ネイティブswap-chain面による子Widgetの遮蔽を避けるため、パネルをviewportHostの子から外側の横レイアウトへ移し、表示時はVP幅を確保して並べる構造に変更した。

## 2026-09-10 — History Timeline の部分復元には command payload 契約が必要

- **関連:** `ArtifactHistoryTimelineWidget` / `UndoManager` / Source Patch History
- **確認できた事実:** 現在の `UndoManager` は履歴ラベル、Undo/Redo、シリアライズ可能なコマンドを扱えるが、任意の履歴点から「Blur 設定だけ」のようなプロパティ単位payloadを共通形式で列挙する公開 API はない。
- **気づき:** History Timeline の安全な部分復元は、UI側でコマンド型を推測するのではなく、コマンドが復元可能payloadの種類・対象ID・preview値を明示する契約を持つと Project History と Source Patch History の双方で再利用できる。
- **価値／懸念:** 共通契約があれば部分復元ボタンを実データにのみ有効化できる。契約なしで実装すると、型別分岐がUIへ漏れ、誤った対象への適用や復元不能状態を招く。
- **次に確認すること:** `UndoCommand` の serialization schema と AI patch metadata を横断し、read-only の `restorablePayloads()` 相当を追加できるか設計レビューする。現段階では未対応コマンドに対する部分復元を無効表示に留める。

## 2026-09-11 — Glyph atlas の差分アップロード境界

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`、`ArtifactCore/include/Text/GlyphAtlas.ixx`。
- **確認できた事実:** `GlyphAtlas` は固定 2048×2048 の CPU atlas と dirty bool を持つ。従来の GPU 側は新規 glyph の追加ごとに immutable texture を破棄・再作成していた。
- **対応:** Artifact 側の command-buffer と immediate-submitter の両経路を updateable texture の再利用へ移し、既存 texture へ upload するようにした。glyph 提出用の一時配列も renderer lifetime の scratch buffer として初期化時に確保し、通常のテキスト編集では再確保しない。
- **対応（追記）:** `GlyphAtlasDirtyRegion` を公開し、追加 glyph の矩形を union、clear と初回を全量更新として表す契約を追加した。
- **対応（追記）:** `PrimitiveRenderer2D` と `DiligentImmediateSubmitter` は、Diligent の `UpdateTexture` に image stride を保った矩形 source pointer と destination box を渡す。新規 GPU texture は dirty state にかかわらず全量初期化する。
- **価値／懸念:** texture object の再生成を避けたまま、通常の新規 glyph 追加では更新領域だけを転送できる。全量／矩形の実転送量と backend parity は未検証である。
- **次に確認すること:** atlas reset 時は全量、それ以外は矩形 upload になること、CJK／emoji glyph と複数 glyph の同フレーム追加で union 範囲が正しいことを実機プロファイルで測る。

## 2026-09-11 — 基本テキストの shaping 再利用境界

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- **確認できた事実:** fill-only の通常テキストは layout cache に `GlyphItem` を持つ一方、従来は `drawTextTransformed()` が immediate submit 時に同じ文字列を再 shaping していた。
- **対応:** plain text の fill / stroke / shadow を cached glyph direct-draw へ切り替えた。stroke の 8 方向 outline と shadow offset は既存 immediate path と同じ値を glyph quad へ渡す。underline / strikethrough は既存 immediate glyph submit でも独立線として描かれていないため、今回の経路切替で表示を増減させない。
- **価値／懸念:** 静的な通常テキストでは layout 成果を再利用できる。実機で alignment・CJK fallback・stroke / shadow・cloner transform・長文の frame cost を比較するまで parity / 性能は未検証。

## 2026-09-11 — glyph 単位 font fallback の再利用

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **確認できた事実:** cached glyph direct-draw は `GlyphAtlas` が hit しても、各 glyph ごとに `FontManager::makeFont()` を呼び、fallback family を解決していた。
- **対応:** `TextStyle` が同一の間は code point ごとの解決済み `QFont` を renderer lifetime cache に保持する。キャッシュは最大 2,048 glyph で clear し、font style 変更時にも clear する。
- **価値／懸念:** CJK fallback の glyph 単位意味論を維持したまま、静的テキストの font database 問い合わせを避ける。font install/uninstall 中の動的更新は未検証。

## 2026-09-11 — transformed glyph key の再利用

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **確認できた事実:** transformed glyph の提出は、font fallback cache が hit しても glyph ごとに `GlyphKey` と `fontFamily` の UTF-8 文字列を再構築していた。
- **対応:** style と code point ごとの fallback cache に `GlyphKey` も保持し、atlas acquire にその値を渡すようにした。
- **価値／懸念:** static text の CPU submit で繰り返される文字列確保を避ける。atlas clear 時の glyph rect は保持せず、従来どおり atlas から都度取得する。
- **次に確認すること:** CJK fallback、emoji、style切替と atlas clear 後に正しい key で再取得されることを実機で確認し、長文の CPU submission cost を測定する。

## 2026-09-11 — animator glyph 提出の一時表撤去

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm` の `drawGlyphs()`。
- **確認できた事実:** animator などが使う pre-laid-out glyph 経路は、呼び出しごとに unique glyph table を確保し、同じ fallback font と atlas key を作っていた。
- **対応:** renderer lifetime の style / code point cache を使い、atlas acquire の二段階処理は維持したまま局所 vector を撤去した。
- **価値／懸念:** 動的 text でも glyph 提出に伴う局所ヒープ確保を避ける。cache は2,048 entryで clear するため、それを超える多言語文書の warm-up は未検証。
- **次に確認すること:** animator 有効なCJK・emoji長文で、glyph color override、opacity、atlas reset 後の表示と frame cost を確認する。

## 2026-09-11 — 未接続の ArtifactTextGlyphSubmitter

- **関連:** `Artifact/src/Render/ArtifactTextGlyphSubmitter.cppm`。
- **確認できた事実:** この submitter は atlas を `clear()` して immutable texture、vertex buffer、constant buffer を submit ごとに作る。一方、現行の `ArtifactTextLayer` GPU 経路は `PrimitiveRenderer2D` と `DiligentImmediateSubmitter` を使い、検索上この submitter の呼び出し元は確認できなかった。
- **判断:** 稼働中の GPU text 経路を重複実装へ切り替えず、部分 upload を既存二経路へ適用した。
- **価値／懸念:** 未接続コードを性能根拠にして現行経路を誤って置換しない。将来この contract を有効化する場合は、resource reuse と呼び出し ownership を先に設計する必要がある。
- **次に確認すること:** module registration と将来の consumer を監査し、不要なら削除、必要なら既存 atlas uploader へ統合する判断を別作業として行う。

## 2026-09-11 — rich GPU run の callback copy

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm`、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`。
- **確認できた事実:** rich text の GPU run は `drawWithClonerEffect` へ値 capture され、同期 callback を受ける `std::function` の構築時に glyph 配列全体をコピーしていた。
- **対応:** run を参照 capture に変更した。clone helper は callback をその呼び出し内で直ちに実行し、保持しない。
- **価値／懸念:** rich text の clone pass ごとに発生していた run copy を除去する。QTextDocument の再構築・run 分解は静的 rich GPU run cache の対象として別途整理した。

## 2026-09-11 — 静的 rich GPU run の再利用境界

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- **確認できた事実:** rich text の GPU 経路は、内容・書式が変わらないフレームでも `QTextDocument` の構築、block / fragment / line の分解、各 run の shaping を実行していた。
- **対応:** animator 未使用時だけ、既存のテキスト cache key と同じ入力で GPU run を保持して再利用する。画像 object を含む rich text と animator 有効時は cache を使わず、従来どおり毎フレーム構築する。
- **価値／懸念:** 静的な rich text の CPU 側レイアウト作業を避けられる。一方、run の実フレーム時間と画像 object を含む文書の fallback parity は未検証。
- **次に確認すること:** 実機でHTML書式、複数行・box alignment、CJK fallback、underline / strikethrough、animator 有効／無効切替を確認し、長文の frame cost を計測する。

## 2026-09-11 — Shape F12 主ビューポートの頂点マーキー選択

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実:** Shape のカスタムパス／ポリゴンにはクリック選択と単一頂点ドラッグがある一方、主ビューポートで頂点を矩形選択し、複数頂点を同じデルタで移動する導線がなかった。
- **対応:** 空キャンバスからの通常ドラッグ、または選択Shape上でのShift/Ctrlドラッグを頂点マーキーとして追加し、Replace/Add/Toggleを既存の選択文法へ接続した。カスタムパスとポリゴンの両方で選択でき、選択済み複数頂点の移動は既存の単一Undoトランザクション内で相対移動する。Clonerと重複するRepeater操作や新規ショートカットは追加していない。
- **価値／懸念:** レイヤー内部の通常クリック移動、Altオービット、既存ギズモ／Pen操作を優先順位ごと維持したまま、Shape編集の基本選択文法を補完した。ハンドルのみ、比例編集、分割、ミラーなどF12残項目は未実装。
- **次に確認すること:** ビルド・`check_module_hygiene`・実機runtimeは未実施（AGENTS.md制約でユーザー許可待ち）。変形済みShape、Replace/Add/Toggle、空キャンバス上の既存レイヤーマーキー、Undo／再読込時の選択状態を確認する。

## 2026-09-11 — 911開発ブランチ統合後の3D開発順序

- **関連:** `Artifact/docs/MILESTONE_3D_GIZMO_IMPLEMENTATION_2026-03-25.md`、`Artifact/include/Layer/Artifact3DModelLayer.ixx`、`ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/include/Geometry/MeshImporter.ixx`。
- **確認できた事実:** `origin/codex/2026-09-11-dev` は3Dレイヤー、クローン、カメラ、メッシュ描画・マテリアルの基盤を親子リポジトリへ追加した。一方、3Dギズモ文書は実機操作確認を保留し、メッシュ／レンダー側には読み込み・GPU資源・環境／材質の統合入口が増えた段階である。
- **気づき:** 次の高価値な実装単位は、3D機能をさらに広げる前に「モデル読み込み→シーン配置→カメラ操作→保存／再読込→GPU描画」の最小縦切りを受入れ可能にすること。これにより未検証の3D基盤を制作フローへ接続できる。
- **価値／懸念:** 911の変更効果を実際のユーザー操作で確認でき、後続のライト、マテリアル、クローン拡張の回帰範囲を小さくできる。ビルド／実機検証は未実施であり、低レベルDiligent変更を広げる前に既存経路の責務確認が必要。
- **次に確認すること:** 3Dレイヤー生成ダイアログ、プロパティ編集面、プロジェクト保存形式、`ArtifactCompositionRenderController` の3D入力・描画呼び出しをつなぎ、最小のruntime受入れ項目を定義する。

## 2026-09-11 — サイドカーUUIDを再リンク候補の一次キーにする

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`。
- **確認できた事実:** ファイル名・拡張子・サイズだけでは、移動や改名後の3Dモデルを確実に特定できない。Project Item、Asset Database、旧パス／候補パスのサイドカーから論理UUIDを取得できる。
- **対応:** 一致する論理UUIDを再リンク候補の最上位スコアにし、複数選択時のID一覧コピーも追加した。
- **価値／懸念:** 命名規則に依存せずアセットを復旧できる。一方、サイドカーが欠落・複製された場合は従来のファイル特徴量へフォールバックするため、同一UUIDの重複検出は別途必要。
- **次に確認すること:** 実ファイルを移動・改名し、サイドカーを保持した状態で候補順位と再リンク後のID維持をruntimeで確認する。

## 2026-09-11 — Nuke型ワークフローのグラフ正本境界

- **関連:** `Artifact/src/Engine/DAG/CompositionGraphBuilder.cppm`、`Artifact/include/Engine/DAG/CompositionGraph.ixx`、`Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm`、`Artifact/include/Color/ArtifactColorNodeGraph.ixx`。
- **確認できた事実:** 現行のComposition graph builderはレイヤー順・親子・各レイヤーのeffect列から評価用DAGを都度生成する。Composition Graph WidgetはComposition／Layer／Effectの可視化を目的とし、ColorNodeGraphは独立した色補正DAGである。
- **気づき（未検証）:** Nuke型の任意接続を追加する際、表示用DAGを直接編集可能にすると、タイムラインのレイヤー順・effect stack・保存形式との正本が二重化する。まずは「レイヤー合成の正本を維持した read-only graph + viewer／channel／AOV検査」を制作導線にし、任意ノード接続は明示的なNode Comp資産として別の正本・入出力契約・Undo／保存を設計してから導入するのが安全である。
- **価値／懸念:** 既存のレイヤー制作フローを壊さず、Nukeの強みである中間結果の観察とAOV利用を早く提供できる。グラフ編集を先行すると、レンダー順・キャッシュ無効化・親子関係・project round-tripの不整合が起きやすい。
- **次に確認すること:** `CompositionGraphBuilder`のノードID安定性、render controllerの中間surface／AOV寿命、project JSONにNode Compを独立assetとして保存できるかを、実装前の設計レビューで確認する。

## 2026-09-11 — Cryptomatte draft の識別子正規化

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/include/Image/Cryptomatte/CryptoPixel.ixx`。
- **確認できた事実:** AOV passはlayer IDとmaterial keyから24bit値を描画する一方、Render Queueのmanifestは別実装のhashを保持していた。3D layerでは描画時に`materialSignature()`を使うが、manifestはlayer名由来だった。
- **対応:** 描画とmanifestの両方を`CryptoSample::nameToId()`へ統一し、3D material entryは描画時と同じ`materialSignature()`を使うようにした。
- **価値／懸念:** draft EXRのID値とname manifestが一致する。これはrank付きcoverage、Cryptomatte標準のfloat payload変換、pick-to-matte UIを実装する前提であり、それらの未実装を完了扱いにはしない。
- **次に確認すること:** 3D／2D混在sceneをEXR出力して外部Cryptomatte consumerでmanifestとIDが一致すること、半透明edgeの複数coverageを保持するGPU passとpick UIの契約を検証する。

## 2026-09-12 — Flat AOVからDeepへの明示変換境界

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/include/Image/DeepImageBuffer.ixx`。
- **確認できた事実:** rendererはBeauty RGBAとDepthを`MultiChannelImage`へ一度readbackでき、CoreはRGBA+depthから`DeepImageBuffer`を構築する明示APIを持つ。
- **対応:** Render Queueでreadback済みのRGBAとDepthから一サンプル／pixelのDeep bufferを構築し、同じAOVの二重GPU readbackを避けた。
- **価値／懸念:** Deep merge、holdout、Deep EXRの既存基盤へ通常render結果を接続できる。これは複数visibility sampleを持つnative Deep rendererではなく、書き出し時の冷経路で一時バッファを確保する。
- **次に確認すること:** flat-to-deepを示すmetadata、Depthの空間・単位契約、native Deep sample passとflat bridgeの選択基準を設計レビューする。

## 2026-09-12 — Chroma Key GPUのフレーム資源再利用

- **関連:** `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`。
- **確認できた事実:** GPU Chroma Keyはパイプライン、定数バッファ、入力／出力／readback staging textureを再利用する。入力は同一サイズなら`UpdateTexture`で更新し、device/context変更時は全GPU資源を破棄して再構築する。
- **価値／懸念:** 解像度が安定した連続プレビューではtexture生成を避けられる。一方、GPU readbackは同期`WaitForIdle()`を維持しており、effect-chain全体のCPU待機は残る。
- **次に確認すること:** effect frame samplerと共有resourceの寿命・resize契約を調査し、D3D12/Vulkan双方でallocationとGPU待機を計測してから、非同期readbackまたはchain内GPU保持を設計する。

## 2026-09-12 — Project Viewタイルの混合グリッド境界

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`docs/design/project_view_redesign_2026-09-08/project-view-tile-workbench.png`。
- **確認できた事実:** 現行Tile表示は全visible rowを同じ`tileRectForRow()`格子へ配置し、描画、ヒットテスト、スクロール範囲がその単一寸法を共有する。09-08タイルモックはフォルダーを低い上段カード、アセットをサムネイル付き下段カードとして表現している。
- **気づき:** モックへ正しく寄せるには、描画だけフォルダーを小さくせず、項目種別を考慮した共通layout結果を描画・選択・ドラッグ・スクロールで再利用する必要がある。
- **価値／懸念:** 構造を上段で読み、素材を下段で見渡せる一方、paintだけの局所変更ではクリック対象と表示位置がずれる。
- **次に確認すること:** filtered visible rowsからboundedな配置情報を更新時に構築し、同じ配置をpaint、hit test、scroll extent、drop indicatorへ渡せるかを確認する。

## 2026-09-12 — Voronoi/Bricks GPU Spatial化（HexGrid/Stripes型）

- **関連:** Artifact/include/Effects/ArtifactAbstractEffect.ixx（Kind追加）、Artifact/include/Effects/Rasterizer/VoronoiEffect.ixx、BricksEffect.ixx、Artifact/src/Render/ArtifactRenderLayerPipeline.cppm（kVoronoiShader/kBricksShader＋executor分岐）。
- **確認できた事実:** Spatial GPUパスは全effectがGPU対応・mix=1・mask/regionなしのときだけ選択され（uildGpuRasterEffectPlan）、失敗時はレイヤー拒否でfail-closed（effect落としなし）。CPU実装は参照用に残る。GpuSpatialEffectKind末尾追加なので既存値の採番は不変。
- **価値／懸念:** Voronoi HLSLはCPUのhash11/hash12・3x3探索を鏡写し、Bricksは行オフセット＋fmod判定を鏡写し。CPU/GPU画素一致は未検証。Kaleidoscope/Halftone（入力サンプリング型）とGlow（2パス）はKindのみ先行追加し実装は残課題。
- **次に確認すること:** ビルド許可後にVoronoi/BricksのCPU/GPU画素差分を計測し、toleranceを決めてからKaleidoscope→Halftone→Glowの順で spatial 化する。

## 2026-09-12 — 正規KaleidoscopeのGPU常駐化（Kind配線）

- **関連:** Artifact/include/Effects/Kaleidoscope/KaleidoscopeEffect.ixx、同 src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm:197 kKaleidoscopeHlsl、Artifact/src/Render/ArtifactRenderLayerPipeline.cppm。
- **確認できた事実:** 表のkaleidoscope（Service登録あり）は旧式単体GPU済みだが常駐未対応だった。常駐HLSLは旧式 kKaleidoscopeHlsl をbilinearヘルパごと流用しentryのみ KaleidoscopeCS 化。7パラメータは parameters[8] に収まりpixel単位なしのためmask=0。旧式applyGPUと常駐は共存する。RasterizerKaleidoscopeEffect はimportゼロ・Service登録なしのデッドコードのため対象外。
- **価値／懸念:** 常駐化で他GPU effectとの連鎖時にper-effectのupload/readback/WaitForIdleが消える。一方atan2/cos/sinのCPU/GPU精度差がセグメント境界で1px級差分を出す可能性あり。
- **次に確認すること:** ビルド許可後にCPU/旧GPU/常駐GPUの3者画素差分を計測し、toleranceを決めてからHalftone→Glowへ進む。

## 2026-09-12 — 汎用GPU常駐の設計メモ＋Halftoneパイロット

- **関連:** docs/analysis/GENERIC_RESIDENT_GPU_DESIGN_2026-09-12.md、Artifact/include/Effects/ArtifactAbstractEffect.ixx（Kind::Generic・共有レジストリ）、Artifact/src/Render/ArtifactRenderLayerPipeline.cppm（汎用PSOキャッシュ＋分岐）、ArtifactHalftoneEffect。
- **確認できた事実:** 汎用GPU実行基盤は既存（
unCreativeCompute＋labelキーキャッシュ、ArtifactCreativeEffects.cppm:3725）だが常駐でない・パラメータなし。新設でなく拡張する方針にした。登録済みHalftone/Glitch/OldTVは既にcreative GPU済み。Rasterizer::HalftoneEffect はimportゼロ・Service登録なしのデッドコード。旧式単体GPU・creative・常駐の3経路が共存する。
- **価値／懸念:** 新規effectはRenderPipeline無改修で常駐化可能（HLSL＋登録のみ）。g_Time/g_Frame は0埋めのTODO。旧式・creative経路の一本化はPhase 2に先送り。
- **次に確認すること:** ビルド許可後にCPU/creative/常駐の3者画素差分とフォールバック動作を確認し、受入基準（設計メモ§6）を満たしたらGlitch/OldTVへ横展開する。

## 2026-09-12 — GlowのGeneric常駐化（params受け渡し実証）

- **関連:** Artifact/include/Effects/Glow/GlowEffect.ixx、src/Effects/Glow/GlowEffect.cppm（kGlowResidentHlsl）。
- **確認できた事実:** 登録済み glow は旧式GPU済みの多層ブルーム。常駐bodyは kGlowHlsl のcbuffer除去＋ g_P0..g_P6 置換で再現し、旧b0とのregister衝突を回避。baseSigmaのみpixel単位のためmaskはbit2だけ。旧b0付きHLSLをそのまま登録するとpreludeと衝突する制約を設計メモへ反映要。
- **次に確認すること:** ビルド許可後にCPU/旧GPU/常駐の3者画素差分を計測する。

## 2026-09-12 — LiquidGlowの新規HLSL常駐化（radiusゲート先例）

- **関連:** Artifact/include/Effects/Glow/LiquidGlowEffect.ixx、src/Effects/Glow/LiquidGlowEffect.cppm（kLiquidGlowResidentHlsl）。
- **確認できた事実:** 旧GPUなしの初GPU化。CPUのthreshold→separable Gaussian→flow remap→加算を単一ノードで再現。フル分離形のbilinearは計算量が爆発するため、まだらサンプルではなく水平ブラー＋垂直ガウス＋x方向bilinearの厳密分離形にした。luma重み（R*0.114の変則）もCPU鏡写し。
- **価値／懸念:** コスト超過パラメータは ppendGpuSpatialNodes で alse を返してCPUに残す先例を作った（radius>8）。見た目を変えずに適用範囲だけ絞るfail-closedの応用。remap境界はclamp/reflect101差あり。
- **次に確認すること:** ビルド許可後にtolerance計測。ResidualGlow等も同型で続ける。

## 2026-09-12 — 用途別ファイルピッカーの共通ブラウザ基盤

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`Artifact/src/Widgets/Dialog/ArtifactImportAssetsDialog.cppm`、`Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- **確認できた事実:** Asset Browser はファイル探索、サムネイル、Favorites、Recent を持つ一方、Project Open、Import、Relink、LUT／OCIO、export destination などは複数箇所から `QFileDialog` を直接呼ぶ。`ArtifactImportAssetsDialog` は既に選ばれたパスを確認する段階を担当し、選択前のブラウザではない。
- **気づき:** OSファイルダイアログを全面置換する単一巨大Widgetではなく、Asset Browserの探索／履歴／サムネイル基盤を再利用し、Open Project、Import Media、Relink、Reference/LUT、Export Destinationを用途別モードとして構成する方が責務を保ちやすい。ネットワーク共有やOS固有場所のため、native pickerへの退避導線は残す。
- **価値／懸念:** DCC固有のsequence検出、color space、proxy、missing media候補、output token／上書き衝突を選択前に示せる。一方、import／relink／saveを一つのモデルに押し込むと状態と検証規則が肥大化する。
- **次に確認すること:** Asset Browserのdirectory model、thumbnail cache、recent/favorite保存APIを非Dockのdialog shellから安全に再利用できるかを確認し、まずImport Media Pickerを最小縦切り候補にする。
- **対応:** `ArtifactMediaImportPickerDialog` を既存Import dialog module内に追加し、FileメニューとImport requestの選択前段を統一した。選択・検索・種別filter・連番展開のみを行い、Project mutationとlogical Asset ID登録は既存の確認／非同期Import経路へ残した。新規signal/slot、QImage decode、thumbnail再生成は追加していない。

## 2026-09-12 — LUTライブラリ選択をColor Science Managerの前段へ分離

- **関連:** `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`、`ArtifactColorScienceManager`、`docs/design/lut-color-reference-picker/`。
- **確認できた事実:** Color Science Managerはbuilt-in／外部LUTの列挙とload、現在LUTのformat／size／errorを持つ一方、preview-only routing、domain metadata、favorite／recent保存APIを公開していない。
- **対応:** `ArtifactLutColorReferencePickerDialog` は一覧・検索・種別別閲覧・互換性確認だけを受け持ち、accept後に既存managerの`loadBuiltinLUT`／`loadLUT`へ委譲するようにした。存在しないstateをUI上で操作可能に見せず、working-preview checkboxはdisabledとした。新規signal/slot、QImage、QPainter描画、Core変更なし。
- **価値／懸念:** 既存Color Science Panelの単一list／file dialogをDCC型library chooserへ置き換えつつ、LUT適用の責務を複製しない。同一reference thumbnailとbefore/after splitはGPU preview境界を設計してから追加する必要がある。
- **次に確認すること:** ビルド許可後、built-in／外部／破損LUTの選択、検索とcategory切替、accept後のmanager更新、panel preview更新を実機確認する。

## 2026-09-12 — PhysicalHalation coreの2層拡散が互いを消す（要判断）

- **関連:** ArtifactCore/include/ImageProcessing/Halation.ixx:51-54、Artifact/src/Effects/Glow/PhysicalHalationEffect.cppm。
- **確認できた事実:** process() は同一バッファに diffuse(..., spread*redDiffusion, 1,0,0) → diffuse(..., spread, 0,0.1,0.05) を順に掛ける。1回目でG/Bが0になり、2回目でRが0になるため、最終highlightsは既定値で (0,0,0) に潰れる。composite加算も0。意図（層別バッファ）と実装（同一バッファ破壊）が矛盾。softness設定も process() 未参照。
- **価値／懸念:** 鏡写し移植は無意味（no-opの再現になる）。core修正はCPU挙動の可視変化を伴うため独断不可。
- **次に確認すること:** ユーザー判断（core修正してからGPU化／現状維持）。常駐化は見送り。

## 2026-09-12 — Open Project の事前表示は未検証状態を明示する

- **関連:** `Artifact/include/Widgets/ArtifactImportAssetsDialog.ixx`、`Artifact/src/Widgets/Dialog/ArtifactImportAssetsDialog.cppm`、`Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`。
- **確認できた事実:** 既存のOpen Project導線はfile pathを得て既存の非同期loadへ渡すだけであり、migration、project health、missing sourceの事前検証結果を提供していない。
- **対応:** `ArtifactProjectOpenPickerDialog` はrecent projectの探索・選択とSystem Picker fallbackだけを担当し、詳細欄では状態を`Not checked until opening`と表示する。load／migration／検証／recent更新の責務は既存Project Serviceに残した。
- **価値／懸念:** mockのhealth表現を根拠のない正常表示にせず、open前のUIで保証できる情報だけを示せる。tile preview、favorite、実データに基づくhealthは将来のProjectメタデータ境界が必要。
- **次に確認すること:** ビルド許可後、recentなし／recentあり／missing recent／System Picker／load失敗時の既存エラー経路を実機確認する。

## 2026-09-12 — 出力先の衝突回避は選択時に確定pathへ反映する

- **関連:** `Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm`、`docs/design/export-destination-picker/`。
- **確認できた事実:** Render Output Settingsはformat／codec／frame rangeを保持し、BrowseはOSのsave dialogだけを開いていた。出力衝突を避けるversion policyのUIはなかった。
- **対応:** Browseをdestination pickerに変更し、folder、base name、version、frame token、extension、最終pathを確認できるようにした。候補が既存なら、返却する最終path自体を次の空きversionへ移すため、表示だけが安全で実際は上書きする不整合を防ぐ。
- **価値／懸念:** Render／queue mutationやformat決定を複製せず、出力命名だけに責務を限定する。複数ジョブのtoken規則、容量見積り、空き容量表示はRender Queueの実データ境界を定義してから追加する。
- **次に確認すること:** ビルド許可後、既存versionの連番、image-sequence token、format切替後のextension更新、System Picker fallbackを実機確認する。

## 2026-09-12 — Relink候補は理由を読んでから採用する

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`、`docs/design/asset-relink-picker/`。
- **確認できた事実:** 既存の候補検索はlogical Asset ID、filename、extension、sequence pattern、asset typeなどをscoreとreasonに保持しているが、Asset Browserは`QInputDialog`の文字列選択で表示していた。
- **対応:** Candidate Pickerにscore、full path、Match Reasons、sequence frame一致数を表示し、採用後は既存の`relinkFootageByPath`とUndo commandへ渡す。filename単独で「Best match」と表示する処理は追加していない。
- **価値／懸念:** 採用判断を観測可能にしつつ、logical Asset IDとcache／Undoの既存責務を変えない。Project Viewのbulk relinkも同じ候補UIへ統一するには、対象セットとmappingを渡す別のbatch contractが必要。
- **次に確認すること:** ビルド許可後、同名別content、logical Asset ID一致、連番の部分一致／完全一致、Undo失敗時のrollbackを実機確認する。

## 2026-09-13 — GPU常駐Blurの実surface契約が設計記述とずれている

- **関連:** `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx:89-104`、`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm:181-220`、`Artifact/include/Effects/GauusianBlur.ixx`。
- **確認できた事実:** `RenderConfig::PipelineColor` は canonical linear-premultiplied と宣言する一方、`layerToFloatShaderText` は入力を unpremultiply して `float4(straight, alpha)` を `layerFloat` へ書き込む。既存の常駐Gaussian shader はその実体に合わせて RGB×alpha の平均後に unpremultiply しており、CPU Gaussian fallback も同じ前提である。
- **価値／懸念:** 通常 `BlurEffect` を常駐Gaussianへ安易に接続すると、sRGB/premultiplied を明示変換する旧GPU実装との出力差が透明境界で発生し得る。descriptorだけを根拠に常駐shaderを premultiplied平均との差し替えると、現行 `layerToFloat` の実体と逆行する。
- **対応:** ユーザーの明示許可を得て、`layerToFloat` の unpremultiply を撤去し、Core blend shader と resident Gaussian を premultiplied 契約へ切り替えた。通常 `BlurEffect` は premultiplied・mix 100%・CPU half-resolution未使用（sigma < 3）に限り、Gaussian反復は最大8 pass、Edge Preservingは二段Gaussianとして最大4反復まで常駐化した。
- **次に確認すること:** ビルド許可後に `GaussianBlur`／通常Blur／blend/matte のCPU・旧GPU・常駐GPU fixtureを比較し、encoded-sRGB ingressのlinearizationが一度だけ行われることも確認する。

## 2026-09-13 — Lens Distortionを常駐Spatialへ移す際のstage境界

- **関連:** `Artifact/include/Effects/LensDistortion/LensDistortionEffect.ixx`、`Artifact/src/Effects/LensDistortion/LensDistortionEffect.cppm`、`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`。
- **確認できた事実:** Lens Distortionの旧GPU実装は単一フレームの画像warpだが、毎回のtexture生成→staging copy→`Flush`／`WaitForIdle`→CPU readbackを含む。CPU式とGPU式は同じ radial/tangential/zoom/invert/edge-fill と bilinear sampling を持つ。
- **対応:** 専用 `GpuSpatialEffectKind::LensDistortion` と16-slot固定ノードを追加し、常駐CSでlinear-premultiplied RGBAを直接bilinear処理する。画像を変形するラスター処理として `EffectPipelineStage::Rasterizer` に変更し、composition GPU planから呼べるようにした。
- **価値または懸念:** effect chain内のCPU readbackとGPU waitを除去できる。一方、stage変更はGeometryTransformとして保存された既存stackの順序意味に影響し得るため、runtimeでLensとTwist/Bendの混在順序を確認する必要がある。旧GPU経路は互換fallbackとして残置。
- **次に確認すること:** D3D12/VulkanのPSOコンパイル、CPU/旧GPU/常駐GPUの画素差分、透明端とcenter/zoom/invert境界、stage順序、sRGB ingressのlinearize回数をfixtureで確認する。

## 2026-09-13 — Chroma Keyのstraight/premultiplied境界をCoreで統一

- **関連:** `ArtifactCore/src/ImageProcessing/ChromaKey.cppm`、`Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`、`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`。
- **確認できた事実:** 旧Chroma KeyはRGBをそのままYCbCrへ送り、matte finishing後もRGBをstraightのまま残していたため、canonical `layerFloat`（linear-premultiplied）と契約がずれていた。旧GPU経路は同じ式をstaging readback付きで実行していた。
- **対応:** Core／旧GPU／常駐CSともに、入力をalphaで明示unpremultiply→key/despill→最終matteでpremultiplyする順序へ統一した。常駐はYCbCr、clip、despill、3種のdiagnostic viewを12パラメータで実行し、choke/matte blurは追加alpha面を表現できるまで旧経路へfail-closedする。
- **価値または懸念:** 半透明のgreen-screen境界で色漏れを抑え、通常keyer連鎖のCPU実行・readback・GPU waitを除去できる。直接Core APIへstraight RGBを渡していた外部呼出しは意図的にcanonicalへ移行したため、互換性の実測が必要。
- **次に確認すること:** D3D12/Vulkan PSO、CPU/旧GPU/常駐GPU parity、view mode、clip／despill、半透明入力、choke／blur fallbackをfixtureで検証する。

## 2026-09-13 — ArtifactAbstractLayerのMSVC C1001はEOFではなくIFC依存境界を疑う

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm:12481`、`Artifact/include/Layer/ArtifactAbstractLayer.ixx`、直近の`TimeRemap`／`PosterizeTime`追加。
- **確認できた事実:** 報告された12481行は名前空間の閉じ括弧直後で、構文エラーになる実体がない。MSVC 14.51の診断にも`IFC インポートが検出`と出ている。`QVector`は公開宣言と実装で使用していたが、当該ファイル自身のglobal module fragmentから直接includeしていなかった。
- **対応:** `ArtifactAbstractLayer.ixx` と `.cppm` に`<QVector>`を直接includeし、Qt型の可視性をtransitive include／IFCの副作用に依存しないようにした。CMake再生成やビルドは未実行（AGENTSの明示許可待ち）。
- **価値または懸念:** 巨大moduleのIFC importerが暗黙依存を展開する経路を一つ減らせる。再発時は`PosterizeTime` importとTimeRemap JSON処理を別module／file-local helperへ分離するのが次の最小切り分け候補であり、ユーザーの既存dirty変更を巻き戻さない。
- **次に確認すること:** 許可後に`ArtifactAbstractLayer.cppm`だけを再ビルドし、同じC1001が残るかを確認する。残る場合はimportを一時的に外す診断ビルドで原因moduleを二分する。

## 2026-09-13 — Projected frameのハンドルとUnity Simple ViewCubeは別の固定座標系が必要

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`、`Artifact/src/Widgets/Render/ArtifactViewOrientationWidget.cppm`。
- **確認できた事実:** projected frameの枠線とscale/rotateハンドルは同じ`world.map(localPoint)`経路で描かれていたため、レイヤーの回転・非一様scaleでハンドルの四辺まで傾く。Unity Simple navigatorは軸deltaを固定値で描き、`orientation_`を更新しても中央面と三軸の見た目が静止していた。
- **対応:** 枠線は従来どおりレイヤー変換へ追従させ、scale/rotateハンドルだけをview逆行列から得たcamera-facing billboardへ分離した。ハンドル中心はレイヤー上の実点を維持し、サイズはlocal-to-world平均scaleで補正する。Unity Simpleは同じorientation quaternionからX/Y/Zの投影deltaと中央面polygonを再計算するようにした。
- **価値または懸念:** DCCの「操作点は対象上、操作面は画面に安定」という読みやすさを守り、ViewCubeのviewport orbitとの視覚的な不一致を解消できる。billboardの遠近差や極端な非一様scaleは実画面での確認が必要で、入力hit-testは既存の投影中心判定を維持している。
- **次に確認すること:** ビルド許可後、同一の3D planeでrotate／non-uniform scale／oblique viewを比較し、scale handleが正方形を保つこと、Unity SimpleのドラッグおよびAlt-orbitで軸と中央面が同じ向きへ追従することを確認する。

## 2026-09-13 — 3Dグリッドをcomposition平面からworld groundへ戻し、Dockタブ右端にPinを追加

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`。
- **確認できた事実:** 3Dグリッドは`(0..cw, 0..ch)`のcomposition矩形へ描画する経路に変わっており、斜めViewでは白いcomposition平面の外側へ到達しない。CoreにはX-Z平面の`computeGroundGridLines`があり、Native Dock側には`pinned_`とlayout JSONの`pinned`保存、ViewメニューのPin操作が既に存在したが、タブ上の直接操作はなかった。
- **対応:** 3D表示ではcomposition矩形ではなく、既存のworld-space X-Z ground gridをアクティブカメラへ描画する経路へ戻した。spacing・extent・fadeはviewport scale／カメラ距離から決め、軸線もground plane上へ出す。Dockタブの右側ボタンをPin＋Closeの複合ボタンへ変更し、既存の`setDockPinned`／保存状態と同期するイベントフィルタを追加した。新規signal/slot、CSS、GPU resource生成は追加していない。
- **価値または懸念:** グリッドがcomposition平面へ貼り付かず、3Dシーンの基準面としてviewportの外側まで連続する。ピン操作はDock責務内でViewメニューと同じ状態へ収束し、QADS側と同じくピン中はCloseを無効化する。ground-gridの見え方はカメラ極角とカメラ距離、絵文字Pin glyphのfont依存は実画面で確認が必要。
- **次に確認すること:** ビルド許可後、oblique／perspective／top-down viewでcomposition外にも線が出ること、Pinのクリック・キーボード・Viewメニュー・layout再読込の相互同期、D3D12/Vulkan共通経路を確認する。

## 2026-09-13 — 初期ワークスペースと空状態の意味をスクリーンショット基準で整理

- **関連:** `Artifact/src/AppMain.cppm`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/src/Widgets/ArtifactProjectItemPresentation.cppm`、`Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/CommonStyle.cppm`。
- **確認できた事実:** 起動時は保存済みDockグラフをworkspace mode適用後に復元していたため、Default設定でも過去セッションのTimeline／Curve Editorが再表示され得た。Project Viewの空状態イラストは外部ファイル探索だけで、パッケージ済みリソースへのfallbackがなく、プロジェクト未接続でも「Asset Browser linked」と表示していた。右側のProject情報面も未接続時に一般的な「Project／PREVIEW」を表示していた。Effectsの空状態アイコンは「No effects yet」以外で意図的に隠れていた。Composition breadcrumbは解決不能な親compositionのUUIDを表示し得た。選択中ツールバーのunderlineはラベル直下の2px帯だった。
- **対応:** Dock復元後にも選択workspaceを再適用し、DefaultのTimeline再開を抑止した。Project Viewは`composition_empty_composition.svg`を空状態イラストのfallbackにし、未接続時の一覧・右側情報面・下部同期表示を「No project」系へ揃えた。Effectsは未接続／未選択状態でも意味のあるアイコン、短い見出し、次の操作説明を表示するようにした。breadcrumbの未解決IDは表示対象から除外し、選択underlineは最下端へ1pxで移動して文字との間隔を確保した。
- **価値または懸念:** 起動時の「何を編集すべきか」がDefault workspaceと一致し、空状態でも次の操作が読み取れる。UUIDを隠すのは表示層のみで内部identity・診断ログは変更しない。fallback iconとowner-draw変更の実画面密度、既存ユーザーが明示的にAnimation workspaceを保存した場合の可視性はruntime確認が必要。
- **次に確認すること:** ビルド許可後、保存済みAnimation／Defaultの再起動、Project Viewの無プロジェクト／検索0件、Effectsの各未選択段階、nested composition breadcrumb、Motion Path選択時のunderline間隔を確認する。

## 2026-09-13 — Timeline Diligent snapshotの同一イベント内再構築を抑制

- **関連:** `Artifact/include/Widgets/ArtifactTimelineWidget.ixx`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- **確認できた事実:** Diligent timeline previewは既に共有Diligent device／D3D12・Vulkan swap chain／`PrimitiveRenderer2D`で表示できるが、`refreshTracks()`中にplayhead・vertical offset・selection同期が連続して`syncGpuTimelineSnapshot()`を呼び、同じUIイベント内でもsnapshotのQVector構築とGPU render event投稿を複数回行っていた。
- **対応:** snapshot要求をUI event queueへcoalesceし、同一イベント内の連続要求は1回の`buildGpuTimelineSnapshot()`へまとめた。Diligent windowの初期化・submit・present、既存QWidget/QPainterの編集正規経路は変更していない。新規Qt signal/slot、CPU readback、GPU waitは追加していない。
- **価値または懸念:** GPU preview opt-in時の不要なCPU snapshot確保と重複submit要求を減らせる。snapshotは1つのqueued turn分だけ遅延するため、再生ヘッドやscrollの可視応答はruntimeで確認が必要。
- **次に確認すること:** ビルド許可後、`ARTIFACT_GPU_TIMELINE_PREVIEW=1`でscroll／zoom／selection／playhead更新を連続操作し、snapshot generationの増分がイベント単位でまとまり、D3D12／Vulkan表示が最新状態へ追従することを確認する。

## 2026-09-13 — Timeline Diligent snapshotをムーブ引き渡しにして一時QVectorコピーを削減

- **関連:** `Artifact/include/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.ixx`、`Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- **確認できた事実:** `buildGpuTimelineSnapshot()`はそのイベント内でrect／line／triangle配列を構築した後、const参照版`setSnapshot()`が`make_shared`時に配列をもう一度深いコピーしていた。
- **対応:** `DiligentTimelineVisualSnapshot&&` overloadを追加し、GPU windowの共有snapshotへムーブして所有権を移す経路を追加した。UI側は完成したlocal snapshotを`std::move`で渡し、既存のconst参照APIとgenerationのlatest-wins判定は維持した。
- **価値または懸念:** 同一snapshotのQVector配列コピーを1回減らし、GPU submit／presentや既存の非待機設計は変更しない。共有snapshot自体の1回の確保と、描画中の読み取りは残るため、ムーブだけで全フレームallocationが解消するわけではない。
- **次に確認すること:** ビルド許可後、D3D12／VulkanのGPU previewでsnapshot generation、表示の最新性、scroll／zoom連続操作時のCPU allocationとフレーム時間を計測し、const／rvalue overloadのABIとmodule再スキャン影響を確認する。

## 2026-09-13 — 通常タイムラインの採用モックを先に固定し、Graph Editorを分離

- **関連:** `docs/design/timeline/README.md`、`docs/design/timeline/approved-normal-timeline-2026-09-13.png`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- **確認できた事実:** 統合案では通常タイムラインとGraph Editorの操作が同じ画面へ重なり、レイヤー行・時間バー・カーブ操作の優先順位が読み取りにくかった。既存資料には左ペイン採用案、右ペイン比較案、カーブ参考案が別々に存在する。
- **対応:** ユーザーが承認した案2の改訂版を通常レイヤーバーモードの正本モックとして保存した。採用範囲を左ペイン、右時間領域、ルーラー、キャッシュ、ワークエリア、下部ナビゲーターに限定し、Graph Editor／Dope Sheetは専用面の切替責務として残した。
- **価値または懸念:** Diligent表示面はまず通常タイムラインのrow／bar／key整列へ集中できる。モックに描かれたキー操作位置は視覚案であり、Parent列の責務や新規ショートカットを暗黙に変更しないよう実装時に再配置が必要。
- **次に確認すること:** ビルド許可後、通常Timelineの左列とDiligent右面で同じ行高・時刻変換・選択状態が一致すること、キャッシュ状態の実データ表示が正しいことをruntimeで確認する。

## 2026-09-13 — 採用タイムラインの確認対象をDiligent表示面へ切り替え

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`docs/design/timeline/README.md`。
- **確認できた事実:** Diligent timeline pageは環境変数指定時だけ表示され、採用モックとの目視比較には毎回の明示切替が必要だった。GPU初期化失敗時のpainter復帰は既存の`setGpuTimelinePreviewEnabled(true)`に実装済み。
- **対応:** 環境変数が未設定ならDiligent pageを既定表示にし、`ARTIFACT_GPU_TIMELINE_PREVIEW=0`で従来面を強制できるようにした。初期化失敗時の既存fallback、編集・入力のQWidget正本、D3D12／Vulkan共通のDiligent経路は維持する。
- **価値または懸念:** 採用モックに対するGPU面の行／バー／キー調整を起動直後から確認できる。GPU初期化が重い環境ではTimeline生成時のcold pathが増える可能性があるため、初回表示の遅延とfallback理由はruntime確認が必要。
- **次に確認すること:** ビルド許可後、環境変数未設定／`=0`／GPU初期化失敗の3条件で表示切替、編集入力の正本維持、D3D12／Vulkanの表示を確認する。

## 2026-09-13 — Diligent snapshot用にTimeline visual配列の参照取得口を追加

- **関連:** `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- **確認できた事実:** Diligent snapshot生成は`clips()`と`keyframeMarkers()`の値返しを使っており、GPU表示を有効にした各更新で可視判定前にQVector全体をコピーしていた。
- **対応:** 既存の値返しAPIを互換維持したまま、UIスレッドのDiligent生成専用に`clipsView()`／`keyframeMarkersView()`のconst参照APIを追加した。GPU snapshot builderだけが参照口を使い、編集・選択・既存呼出しの所有権契約は変更していない。
- **価値または懸念:** snapshot生成前の一時QVectorコピーを削減し、GPU面のCPU負荷を下げられる。参照はUIスレッド内の同期済みviewに限定しており、他スレッドからのview mutationやGPU resource lifetimeを拡張していない。
- **次に確認すること:** ビルド許可後、Timelineのselection／scroll／refresh中に参照の寿命と行同期を確認し、値返し経路との表示 parity、D3D12／VulkanのGPU描画を実機で比較する。

## 2026-09-13 — Diligent Timelineの選択表現を採用モックへ寄せる

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` の `buildGpuTimelineSnapshot()`。
- **対応:** GPU面のclipに左右のin/out端線を常時描き、selected clipだけ上下線もアクセント色で描く。selected keyframeには細い明色diamond outlineを重ね、通常keyとの差をQWidget版と同じ読み順にした。
- **価値または懸念:** 期間バーの境界と選択キーが暗い背景上で読みやすくなる。テキストラベルや新規GPUリソースは追加せず、既存のDiligent primitive batchへ線分を加えただけである。
- **次に確認すること:** ビルド許可後、D3D12／Vulkanでclip端が1px相当で過度に明るくならないこと、selected outlineがplayhead／隣接keyと干渉しないことを確認する。

## 2026-09-13 — 3D ground gridを後段overlayからworld backgroundへ移動

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実:** world ground gridは`drawViewportCanvasOverlay()`からレイヤー合成後に`draw3DLine()`／`flushGizmo3D()`されていたため、コンポジション面とレイヤーを深度・合成順に関係なく横切った。
- **対応:** grid生成を専用関数へ分離し、GPU resolve、RAM preview、direct fallbackの各経路でviewport背景の直後、コンポジション背景・レイヤー合成の直前に実行するようにした。
- **価値または懸念:** gridはコンポジション外のworld背景として残り、コンポジション／レイヤーが前面になる。既存の3D gizmo線は従来どおり後段overlayであり、挙動を変えない。未検証: Quad presentationで各pane固有のgrid passが必要かはruntimeで確認する。
- **次に確認すること:** ビルド許可後、D3D12／Vulkan、GPU resolve／RAM preview／direct fallbackで、perspectiveのコンポジション面・3D layerがgridを隠し、外側だけにgridが見えることを確認する。

## 2026-09-13 — Project Viewの未選択状態と操作surfaceを分離

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/src/Widgets/ArtifactProjectItemPresentation.cppm`、`docs/design/project_view_redesign_2026-09-08/project-view-focused-workbench.png`。
- **確認できた事実:** 右detail railは未選択時にもpreview説明、selection説明、無効なitem／proxy操作を同時表示し、中央empty stateと意味が重複していた。また作成toolboxがstatus barの直前に独立していたため、状態表示より強い別帯に見えていた。
- **対応:** projectなし／未選択時は右railを単一empty stateへ切り替え、選択時だけ詳細と操作を表示する。作成toolboxはbrowse context行へ移し、status barをproject health／Asset Browser同期の表示専用に戻した。選択詳細はtitle／metadata／previewの縦構成へ変更した。
- **価値または懸念:** Project Viewの「構造と状態を読む」責務が明瞭になり、Asset Browserとの責務境界を増やさず採用モックへ近づく。未検証: 狭幅時のsearch／filter row、選択したcompositionでinline editorを同時表示した場合の右rail高さはruntime確認が必要。
- **次に確認すること:** ビルド許可後、projectなし／projectあり未選択／composition／footage／複数選択、Tree／Tile、狭幅dockでレイアウトとselection同期を確認する。

## 2026-09-16 — Timelineトランジション範囲編集だけUndo経路が分離している

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`TimelineClipMoveRequestedEvent`、`TimelineClipResizeRequestedEvent`。
- **確認できた事実:** 通常レイヤーのクリップ移動／トリムは既存の`MoveLayerToFrameCommand`／`TrimLayerToFrameCommand`へ接続できる一方、トランジションは同じイベント購読内で`setTimelineTransitionRange()`を直接呼び、Undo snapshotを作成していない。
- **価値または懸念:** Diligent面とQPainter面は同じ入力経路を共有するため、トランジションだけUndo不能という差は両表示面に現れる。今回の通常クリップ編集対応へ混在させるとcomposition-owned transition契約まで範囲が広がる。
- **次に確認すること:** transition範囲・関連レイヤー・重なり制約を復元できる既存command／snapshot所有者を調べ、単一ドラッグを1 Undoへまとめる。未検証のため今回の実装対象外。
