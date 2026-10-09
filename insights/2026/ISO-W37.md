**最終更新:** 2026-10-09

# Insight Register — 2026-W37

期間: 2026-09-07 – 2026-09-13

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



## 2026-09-10 — History Timeline の部分復元には command payload 契約が必要

- **関連:** `ArtifactHistoryTimelineWidget` / `UndoManager` / Source Patch History
- **確認できた事実:** 現在の `UndoManager` は履歴ラベル、Undo/Redo、シリアライズ可能なコマンドを扱えるが、任意の履歴点から「Blur 設定だけ」のようなプロパティ単位payloadを共通形式で列挙する公開 API はない。
- **気づき:** History Timeline の安全な部分復元は、UI側でコマンド型を推測するのではなく、コマンドが復元可能payloadの種類・対象ID・preview値を明示する契約を持つと Project History と Source Patch History の双方で再利用できる。
- **価値／懸念:** 共通契約があれば部分復元ボタンを実データにのみ有効化できる。契約なしで実装すると、型別分岐がUIへ漏れ、誤った対象への適用や復元不能状態を招く。
- **次に確認すること:** `UndoCommand` の serialization schema と AI patch metadata を横断し、read-only の `restorablePayloads()` 相当を追加できるか設計レビューする。現段階では未対応コマンドに対する部分復元を無効表示に留める。



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



## 2026-09-08 — setComposition overload の再入リスク

- **関連:** `Artifact/src/Layer/ArtifactAdjustableLayer.cppm`、`Artifact/src/Layer/ArtifactPaintLayer.cppm`、`Artifact/src/Layer/ArtifactSwitchLayer.cppm`。
- **確認事実:** `QObject*` overload が `void*` overload を呼び、その `void*` overload が `ArtifactAbstractLayer::setComposition(void*)` を呼ぶと、base 実装内の virtual `QObject*` dispatch により派生 overload へ戻る。Adjustment Layer の実行スタックでこの循環を確認した。
- **対応:** Adjustment Layer は base の `QObject*` 実装を明示呼出しするよう修正した。
- **懸念・次に確認:** Paint Layer と Switch Layer に同じ実装パターンが残る。今回の依頼範囲外のため未変更であり、各レイヤー追加・composition attach の実機確認後に同じ修正を適用するか判断する。



## 2026-09-08 — エフェクトのGPU常駐チェーン契約

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Effects/ArtifactAbstractEffect.cppm`、`ArtifactCore::LayerBlendPipeline`。
- **確認事実:** Composition View の通常レイヤー用 raster surface builder は CPU の `ImageF32x4_RGBA` を入出力とする。一方、AUTO/GPU の各エフェクト実装は入力を個別アップロードし、dispatch後に staging texture、`WaitForIdle()`、CPU readbackを行うため、複数エフェクトでは同期往復が段数分発生する。調整レイヤーの対応済みpointwise処理だけは `LayerBlendPipeline` 内でGPU常駐する。
- **対応:** CPU所有のsurface builderではCPU実装を明示利用し、GPU専用エフェクトだけ従来経路へフォールバックすることで同期往復を除去した。さらに通常レイヤーでも、完全に表現できる Exposure / Hue・Saturation / Levels / Brightness / White Balance(tintのみ) / Invert / Grayscale を既存 `LayerBlendPipeline` のF32 SRV/UAV pointwise passへ接続した。
- **価値／次に確認:** 通常レイヤーの対応カラー処理は `layerFloat → pointwise → matte → blend` でGPU常駐する。region、effect mask、mix、未対応パラメータ、CPU明示、GPU専用エフェクトは互換性優先で既存経路を使う。残る根本拡張は、任意のエフェクトAPIへSRV/UAVまたはrender-graph resourceを渡すGPU常駐チェーン契約である。D3D12/Vulkan共通のDiligent境界、ping-pong texture寿命、mask/region/mixの適用順を先に確定する。


## 2026-09-08 — Composition controller の旧画像境界と色順

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `buildRasterizedSurfaceBuffer`、`ArtifactCore/src/Image/ImageF32x4_RGBA.cppm` の `setFromCVMat`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx`。
- **確認事実:** controllerのARGB32画像はCV_32FC4へ数値変換した後、descriptorなしのsetFromCVMatへ渡る。一方、同関数はCV_32FC4をRGBAとして記録する。controllerのコメントはupload側でBGRA変換すると説明しており、現行のdescriptor依存変換との不整合がある。
- **未検証:** 実機で赤青が反転する条件と、もう一つのCompositionViewDrawing経路との差。新しい単色GPU経路では従来controllerの格納順・transfer境界を維持し、この調査を色補正変更に広げていない。
- **価値／次に確認:** 赤・青・半透明の固定入力で両描画経路を比較し、色descriptor修正を別途扱う。GPU常駐化の性能比較と色仕様修正を混ぜない。



## 2026-09-07 — モーションパスUndoに残る24fps固定時刻

- **対応追記（2026-09-07）:** ユーザー依頼により対象コマンドをRationalTime保持（複数キーは時刻スケール保持）へ変更し、関連ドラッグ・確定処理もコンポfpsに統一。以下は修正前の調査記録。静的確認済み、実操作検証待ち。

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionMotionPathCommands.cppm` の位置・接線・複数キーUndo、`ArtifactCompositionRenderController.cppm` の過去Planeリリース処理。
- **事実:** これらには `RationalTime(frame, 24)` が残る。一方、通常の編集開始は `gizmoTransformTime` でコンポfpsを使う。
- **懸念（未検証）:** 非24fpsでUndo対象時刻がずれる可能性がある。今回追加した過去枠Scaleは開始時のRationalTimeをコマンドへ保持する。
- **次に確認:** 30/60fpsで位置・接線編集とUndoの対象キーを比較し、既存コマンドの時刻受け渡しを別途そろえる。



## 2026-09-07 — ツールバーの表示モード操作の実行先

- **関連:** `Artifact/src/Widgets/ArtifactToolBar.cppm` の Normal / Grid / Detail actions。
- **事実:** これらは既存のQActionGroupで選択状態を保持するが、同ファイル内に `viewModeChanged` の発火や表示サービスへの委譲が見当たらない。今回の配置変更では既存QActionをメニューに再利用した。
- **懸念・未検証:** 表示切替が実際のビューに反映されない可能性がある。今回の外観変更とは分けて確認する必要がある。
- **価値／次に確認:** 実アプリで3種の表示操作を確認し、必要なら既存の表示コマンドとの対応を調査する。



## 2026-09-07 — カラーピッカーの色空間契約
- 関連: Artifact/src/Widgets/Dialog/FloatColorPickerHooks.cppm、ArtifactCore/src/Color/LabColor.cppm、XYZColor.cppm。
- 確認事実: Lab/XYZの既存変換はsRGB符号値・D65を前提とし、戻りRGBを0–1にクリップする。ピッカーのFloatColor引数には色空間タグがない。
- 未検証: 全呼び出し元が同じ符号値契約であるかは未確認。
- 懸念・次の確認: 将来のHDRや作業色空間対応時は、呼び出し元の色空間を明示してから変換へ渡す必要がある。今回の追加UIにはsRGB/D65基準を明記した。



## 2026-09-07 — 3D回転の操作数学と表示の区別
- 関連: Artifact/src/Widgets/Render/Artifact3DGizmo.cppm の updateDrag。
- 確認事実: 現行回転は開始角との差をEuler成分へ加算し、スナップは各Euler成分へ適用する。atan2境界の差の連続化はこの経路にはない。
- 未検証: ±180度をまたぐドラッグ、傾いたView回転、非ゼロ開始角でのスナップの操作整合性。
- 懸念・次の確認: 今回は外観変更のため数学を変更していない。上記操作を再現してから、必要なら回転更新とピボット更新の一致を別途修正する。


## 2026-09-07 — MSVC IFC C1001 と initializer-list append
- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `cloth3DDeformationMesh()`。
- **確認事実:** C1001 の報告位置は namespace 終端直後の空行だが、直前の追加処理には import された ClothSolver3D の値を `std::vector::insert(..., { ... })` で追加する箇所があった。
- **仮説・未検証:** 大規模な module implementation unit での initializer-list overload 解決が MSVC の IFC 処理を誘発している可能性がある。`push_back` の明示列へ分解して回避した。
- **価値／次に確認:** 再ビルドで C1001 が消えるか確認し、再発時は Cloth3D 実装を別の既存 `.cppm` 境界へ移す切り分けを行う。
- **2026-09-08 追記・確認事実:** 同じ C1001 が継続し、当該実装unitには未使用の `Physics2D`、`Artifact.Composition.Nodes`、`Artifact.Effect.Generator.Cloner` import が残っていた。利用箇所がないことを静的確認して除去した。
- **次に確認:** この依存グラフ縮小後も再現する場合は、次の候補を当てずっぽうに変えず、物理・component runtimeの大きな実装ブロックを既存moduleの実装unitへ分離する。
