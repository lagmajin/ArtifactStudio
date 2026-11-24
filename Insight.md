**最終更新:** 2026-11-20

## 2026-11-20: 既存 alpha→パス変換は穴内の island を保持できない可能性
- **関連:** `Artifact/src/Mask/MaskPath.cpp::fromAlphaMask`、`docs/planned/MILESTONE_AUTO_TRACE_2026-06-02.md`、`docs/planned/MILESTONE_MASK_ALPHA_ROUNDTRIP_LITE_2026-09-10.md`、`experiments/VectorTraceLab/`（本日作成）。
- **確認できた事実:** `fromAlphaMask` は `cv::findContours(RETR_CCOMP)` + hierarchy の 1 段で外側=Add / 穴=Subtract を付与する。`RETR_CCOMP` は 2 階層までしか表現しないため、穴の内部に別の前景 island がある入れ子形状（例: リングの穴の中に星）を、穴ごと潰してしまう。本日のオフライン fixture では naive な色量子化＋輪郭＋1 段穴処理で blue 領域の IoU 0.915 に対し境界距離が 47 px に達した（GT を可視領域に修正後は IoU 0.819 / 16.5 px）。
- **推測・未検証:** 実際の `fromAlphaMask` に同じ入力を通した場合の挙動は未確認（オフライン Python mirror での観測）。`ShapePath::triangulate()` は穴と fill rule に対応するため、多段 hierarchy を渡せば形状側で保持できる可能性があるが未検証。
- **価値または懸念:** ロゴ・テロップのベクター化やキー結果→マスク生成で、穴を含むロゴの再現性が落ちる恐れ。Auto-Trace 実装前にトポロジー契約を決める必要がある。
- **次に確認すべきこと:** `RETR_TREE` による多段 hierarchy と `ShapePath` の fill rule の対応、実 `fromAlphaMask` への同一 fixture 投入、mask roundtrip の Undo/保存への影響。
- **未検証:** 実 C++ 経路、GPU/Shape レンダリング、人手視覚判定、ビルド・テスト（いずれも本日は未実施）。

## 2026-11-20: Virtual Transitionは既存composition transitionを出発点にstack/evaluation契約を固める
- **関連:** `Artifact/include/Composition/ArtifactAbstractComposition.ixx::CompositionTimelineTransition`、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`、`docs/planned/MILESTONE_VIRTUAL_TRANSITION_STACK_2026-11-20.md`。
- **確認できた事実:** compositionにはstable ID・kind・easing・左右layer名・range・enabledを持つ独立transition値型とJSON/APIが存在し、Timelineには追加・管理・Transition clip描画経路がある。別の `ArtifactCore::Video.Transitions` R&Dは画像遷移群が対象で、composition統合済みの証明ではない。
- **追加で確認できた事実:** `ArtifactCompositionRenderController` はpreviewでactive transitionを取得し、同一frame上のCut/Crossfade/Dissolveに限って左右layer opacityを調整する。ほかのkindは専用GPU pass待ちのmetadata扱い。LayerPanelの常設Transition rowはsource A行上のT markerへ置換し、Timelineの既存選択eventを使ってselected range barとsource highlightを表示する実装に更新した。Diligent static snapshotにも同じ marker/range/source highlightを加え、context menuへstack position editorを追加した。`ArtifactVirtualTransitionStackContractTest` targetにはoffscreen表示差テストも追加した。
- **追加で確認できた事実:** `FrameRange::contains` と `frameCount` はinclusive endを採用する一方、transition overlap判定はexclusive endの比較で、progress分母もduration-1を使っていた。overlap判定をinclusiveへ合わせ、progressの0/endを正確にし、duplicate startを元rangeの終端+1へ移す修正とcontract assertionsを追加した。
- **追加で確認できた事実:** transition sourceをnameだけで保持するとrenameに追従できず、同名layerでは描画対象とTimeline highlightが複数へ曖昧化する。新しいtransitionは左右LayerIDを保存し、legacy JSONはcomposition読込時に一意な名前だけをLayerIDへ移行する。ambiguous nameは未解決に保ち、frame evaluator内で全layer走査しない。
- **追加で確認できた事実:** previewとrender queue CPU/GPU pathは、同じopacity-only transition係数を共有できる既存のlayer-opacity境界を持つ。compositionのframe evaluatorを共通化し、Cut/Crossfade/Dissolveについてsource IDとprogressから係数を返すようにした。Render Queueの追加係数は既存layer draw opacityへ乗算し、matte用の一時描画には適用しない。
- **追加で確認できた事実:** `evaluateTimelineTransitionAtFrame()` はframe範囲に入った最初のtransitionだけを返す一方、modelはsourceを共有しないtransition同士の時間重複を許可する。そのため別source pairのtransitionが同時にactiveなら、後続transitionがPreview/Render Queueのopacity適用から漏れる。これは任意の将来案ではなく現在のAPIとvalidation規則の不一致である。
- **価値または懸念:** 同時に進行する別source pairが無視されると、保存済みtransitionと出力画素が一致しない。caller-owned `ArtifactArray` を再利用し、layerごとの係数を集約するcomposition APIとcontract testを追加した。さらにPreview controllerとCPU/GPU Render Queueのopacity pathを全active評価へ切り替えた。
- **次に確認すべきこと:** Preview controllerの既存CompositionChangedEventで容量reserveがframe開始前に行われること、Render Queueのthread-local workspaceがjob間で再利用されることをruntime allocation計測で確認する。A/B groupの実合成とinsertion順は別途render lifecycleを設計して統合する。
- **未検証:** workspace増加がrender frame中に起きないruntime evidence、GPU/CPU pixel-level parity、同時activeの実画像出力、stack insertion。
- **追加で確認できた事実:** `ArtifactLayerPanelWidget` の右クリックmenuは`QAction::data()`を読む既存chosenAction dispatcherを持つ。stack-position編集を新しいaction lambda接続で実装していた差分をこのdispatcherへ移し、source pair編集も同経路で追加した。新しいsignal/slot接続なしで管理動作を追加できる。
- **追加で確認できた事実:** per-frame transition evaluationから`CompositionTimelineTransition`やPImpl `LayerID`をコピーすると隠れたheap確保を持ち込む。固定長UUID byte arrayを返すvalue evaluatorへ変え、legacy name lookupをcomposition load時の移行に限定した。これでframe評価はtransition範囲判定と小さなvalue copyだけでsource係数を判定する構造になった（実allocation計測は未実施）。
- **追加で確認できた事実:** Previewの通常layer loopは各layerのraster/effect/matte結果を既存`RenderPipeline`のlayer surfaceへ作り、`LayerBlendPipeline`でcomposition accumulatorへ即時blendする。現行transition係数はこのblend opacityへ掛かるだけで、source layerを通常passから外したり、transition出力をstack位置に挿入したりしない。`CompositionTimelineTransitionEvaluation`へstackPosition値を渡す変更はできたが、render orderの評価は未接続。
- **追加で確認できた事実:** Previewの`RenderPipeline::Impl`はadjustment/effect-mask等の用途別`TextureBundle`を所有し、pipelineのサイズ初期化・resizeでまとめて管理する。`ArtifactIRenderer::clearTextureRegion`から`LayerBlendPipeline::clearRegion`を呼べる。transition scratchを毎フレーム生成せずpersistentなbounded GPU資源として管理する既存例はあるが、render queue workerには別のowner/poolがある。
- **追加で確認できた事実:** `LayerBlendPipeline`のNormal shaderはsource-overでopacityを合成するため、透明なA/B間のcrossfade契約（premultiplied RGBとalphaをともに補間）とは異なる。既存LayerBlendPipelineへ独立compute executorを追加し、straight-alpha BGRA入力を内部でpremultiplied補間しstraight-alpha BGRAへ戻すGPU APIを実装した。GPU headless contract test sourceはprogress 0/0.5/1の表示値を確認するが、未実行でありcomposition render loopにも未接続。
- **追加で確認できた事実:** `allLayerRef()` は`layerMultiIndex_.all()`そのものを返し、`moveLayerToIndex()`は同containerの配列順を変更する。だが`layerMultiIndex_.indexOf()`は配列の`QList::indexOf`で線形探索するため、sourceのstack indexを毎frame evaluatorで求めるのはHOT_PATH_RULESの全layer走査禁止に触れる。
- **追加で確認できた事実:** `CompositionTimelineTransition` に非シリアライズruntime A/B indexを持たせ、layer add/insert/remove/move/clear、source変更、transition追加・一括設定、composition読込のコールド境界で更新する実装を追加した。frame evaluatorはcached indexだけを読み、`stackPosition=-1`を現在のA index、明示値をstack末尾範囲へclampした`insertionIndex`へ解決する。契約test sourceにはlayer reorder後のsource index/default insertionとexplicit insertion位置のassertionを追加した（実行未確認）。
- **未検証:** layerのorderを変えるすべての経路（特に別クラスからの直接container mutation、Undo/Redo、group/reparent操作）でcache refreshされるか、test compile/runtimeで正しくindexが更新されるか。
- **推測・未検証:** Preview側の既存ping-pong accumulatorはtransition group結果を通常背景へ後からblendする足場になりうる。ただし、A/B sourceの中間面を保持する固定容量resource、frameごとの初期化、source Bが未到着の間の周辺layer順、AOV/depth/3D/matte/adjustment layerの意味を定義しないまま流用すると画質や資源寿命を壊す可能性がある。
- **価値または懸念:** preview pathは離れたsourceを個別時間評価せずlayer list内でopacityを変えるだけで、selected insertion positionは未使用。source namesで参照するため同名layerのrelationも複数強調になる。新fieldやeditorだけでは画像のStack合成動作を実現しない。
- **次に確認すべきこと:** 新しいruntime index cacheのmutation coverageを静的監査し、必要なら既存order mutation APIへrefreshを追加する。Previewの用途別`TextureBundle`とresize lifecycleへtransition group用persistent scratchを追加し、GPU crossfade APIをsource A/Bの一時surfaceへ接続して通常描画から除外しstack indexで一度だけ挿入する。Render Queueは`GpuMatteResourcePool`と別所有なので共有前提にしない。matte/AOV/depth等の対応範囲を明示してからfinal parityへ広げる。これは実装範囲を限定する案であり、製品要求の最終範囲を縮める許可ではない。
- **未検証:** 追加したC++ test source/targetのビルド・実行、source highlightとoffscreen painter結果、GPU snapshot表示とのruntime parity、Undo/Redo統合、final render経路とのparity、GPU stack insertion。

## インデックス（2026-11-20 整理）

- 本文は **210 件**。見出しが前の行へ結合されていた 157 件を 2026-11-20 に修復し、日付の降順に並べ替えた（本文の削除・書換えはなし）。
- 日付別件数（修復後・分離前）: 2026-11-13: 25 / 2026-11-14: 17 / 2026-11-15: 47 / 2026-11-16: 29 / 2026-11-17: 70 / 2026-11-18: 20 / 2026-11-19: 15 / 2026-11-20: 7。
- 解決済み・旧記録 20 件は `docs/analysis/INSIGHT_ARCHIVE_2026-11-20.md` へ移動（全文保持）。
- ルートには未完・未検証・判断待ちの項目を残す。新しい気づきはこのファイルへ追記する。

## 2026-11-20: MSVC v143 toolchain discovery must prefer VS 2022 and module partitions must distinguish interface from implementation units
- **関連:** `cmake/toolchain-msvc-v143.cmake`、`Artifact/src/Layer/ArtifactAbstractLayerImpl.cpp`、`Artifact/src/Layer/ArtifactAbstractLayerLayoutRouting.cpp`、2D particle playground の隔離評価ビルド。
- **確認できた事実:** toolchain はVS 18をVS 2022より先に探索し、VS 2022 Developer Command PromptからconfigureしてもMSVC 19.51を選択した。探索順をコピー内だけで逆転するとMSVC 19.44.35229が選択され、対象ビルドはVS 18で出た `Animation.Transform3D` のC1116を再現しなくなった。Artifact側では `ArtifactAbstractLayerImpl.cpp` が `module Artifact.Layer.Abstract:Impl;` の実装partitionを定義し、`ArtifactAbstractLayerLayoutRouting.cpp` が `import :Impl;` しているが、実装partitionがコンパイルされた後も後者で `ArtifactAbstractLayerImpl` の不完全型エラーが出た。v31評価ではさらに、ArtifactWidgetsのCMake利用側コマンドへ `WIDGET_LIBRARY_DLL_IMPORT` が渡っているのに、MSVC 19.44でコンパイルされた2つの利用側objectが `FloatColorPicker::staticMetaObject` を `__imp_` 経由でなく直接参照した。DLLは実体をexportし、import libraryの該当項目は `__imp_...` だけだったため、利用側参照形式の不整合が通常リンク失敗と一致する。
- **価値または懸念:** ツールチェーン名をv143に固定しても、候補探索順によって別Visual Studioの新しいMSVCが選ばれる可能性がある。また、import側でクラス定義が見えないケースは、CMakeのBMI順序だけでなくpartitionのinterface/implementation区分も点検する手掛かりになる。
- **次に確認すべきこと:** 製品ツリーでの変更前に、toolchain探索優先順位の正式な意図と、`:Impl` が他translation unitからimport可能なinterface partitionとして定義されるべきかを設計レビューする。FloatColorPickerはimport定義が実際にconsumerへ渡るため、export/import属性がC++ module BMI境界でどう維持されるかを、module interfaceをexport/import双方で分けて生成する最小再現でArtifactWidgets所有者と切り分ける。現時点でBMI属性の伝播は仮説であり未検証。コピー内の修正は製品へ反映していない。
- **未検証:** partitionのinterface化やクラスexportによる修正案の妥当性、当該C1116が別のv143構成・完全クリーンビルドでも消えるか、通常リンクでのv31実アプリ挙動・Shape Target UI・GPU再生性能。

## 2026-11-20: CPU エフェクトカーネルはチャンネル順を固定してはいけない — 入力源で backing order が変わる
- **関連:** `ArtifactCore/include/Graphics/SurfaceColorContract.ixx`（`SurfaceChannelOrder`）、`ArtifactCore/src/Image/ImageF32x4_RGBA.cpp`（`getPixel` は `FloatRGBA(pixel[2], pixel[1], pixel[0], pixel[3])`、`rgba32fData()` は「canonical B,G,R,A」と明記しながら**実体は descriptor 依存**）、`Artifact/src/Effects/BurnAway/BurnAwayEffect.cpp`、`Artifact/src/Effects/LinearWipe/LinearWipeEffect.cpp`（GPU 側は `.bgra` スワizzle）。
- **確認できた事実:** Burn Away の CPU カーネルは当初「内部バッファは B,G,R,A」と仮定して色演算を書いていた。しかし `ImageF32x4_RGBA` の backing order は入力源によって異なり、**QImage 由来は BGRA、OIIO デコード由来は RGBA**。実アプリでは画像レイヤーの面が OIIO 由来になり得るため、**そのままでは R/B が入れ替わって出荷される故障**になる。GPU 側は `makeGpuImageUploadBuffer` で canonical RGBA に正規化されるため、CPU だけ合っていない状態だった。
- **価値または懸念:** **「BGRA で確保されているから」で raw バッファのチャンネル順を決めてはいけない。** 色の演算をする CPU エフェクトは `colorDescriptor().channelOrder` を見て読み書きを分岐するか、`getPixel/setPixel` を使う。GPU 側は canonical RGBA で upload されるため、**CPU は GPU の契約（RGBA）に合わせる**のが正解。**検出は画像評価でしかできない**（alpha だけのテストは通る）。契約テストは「中立色の面」で R>G>B の順序を検証すると、入れ替わりの回帰に効く。
- **次に確認すべきこと:** 他エフェクトの CPU カーネルに同じ固定前提が残っていないか（`rgba32fData()` を色として直接扱う箇所を grep して descriptor 対応になっているかを確認）。本件は Burn Away のみ修正済み。
- **未検証:** 実際の画像レイヤー（OIIO 由来 RGBA 面）を通したアプリ経路での色順は未確認（GUI テストは swap chain 失敗で GPU 出力が出なかった）。

## 2026-11-20: Module interface 経由の DLL data export は consumer object の参照形式まで確認する
- **関連:** `ArtifactWidgets/include/Dialog/FloatColorPicker.ixx`、`ArtifactWidgets/src/Dialog/FloatColorPicker.cpp`、`Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cpp`、隔離評価ビルドの `ArtifactParticle2DPlayground` link。
- **追加で確認できた事実:** `ArtifactWidgets/CMakeLists.txt` は `WIDGET_LIBRARY_DLL_EXPORT` をPRIVATE、`WIDGET_LIBRARY_DLL_IMPORT` をINTERFACE compile definitionにしており、`FloatColorPicker.ixx` のクラス宣言は `WIDGET_LIBRARY_DLL_API` で注釈される。よってmodule interfaceの提供側はexport定義でBMIを生成し、consumer側はそのBMIをimportする構成である。
- **追加で確認できた事実:** 製品ファイルを変えない隔離再現をMSVC 14.51で作成し、Qt `QObject`/Verdigris `W_OBJECT`付きのexport module classを`dllexport`で宣言したBMIをconsumerからimportした。通常のglobal-module consumer、別のmodule implementation unit、dummy class間のtyped `QObject::connect`の全ケースで、consumer objectの未定義参照は `__imp_?staticMetaObject...` になった。さらに実際の`FloatColorPickerDialog.ifc`を直接importしたstatic data参照もMSVC 14.51では `__imp_?staticMetaObject...` だった。一方、MSVC 19.44で同じ実IFCをimportすると `?staticMetaObject...` の直接参照となり、実製品のリンクエラーを再現した。原因は少なくとも一般的なBMI importやtyped connectではなく、MSVC 19.44と14.51のC++ module/DLL import差であることが確認できた。
- **追加で確認できた事実:** MSVC 19.44上で、実際の `FloatColorPicker.ixx` を隔離再コンパイルし `WIDGET_LIBRARY_DLL_EXPORT` を外して（`WIDGET_LIBRARY_DLL_API` が `dllimport` となる条件で）生成したIFCを別module interface consumerからimportすると、consumer objectは `__imp_?staticMetaObject...` を参照した。通常のexport設定で生成したIFCでは直接参照になる。この比較はIFC属性と参照形式の関係を示すが、同一moduleのexport providerと併用できる正規ビルド構成や修正案はまだ検証していない。
- **追加で確認できた事実:** export module classを `extern "C++"` linkage specificationで包む隔離変更はconsumer側の `staticMetaObject` 参照からmodule名suffixを外したが、参照は直接symbolのままで、import libraryの `__imp_...` と一致せずリンク修正にならなかった。さらにconsumer用 `dllimport` IFCを同moduleの `W_OBJECT_IMPL` providerにも使う試験では、`staticMetaObject` 定義にC2491となった。`extern "C++"` の追加とprovider/consumer IFCの単純な共有切替は修正候補から除外する。
- **仮説・未検証:** MSVC 19.44の制約を回避する正式な製品修正は未確定。MSVC 14.51で通常製品リンクを回せるか隔離buildを試したが、リンク段階の前に`Animation.Transform3D` IFC importで`stop_token(248)`のC1116となり停止したため、ツールセット更新は現状すぐ使える代替確認経路にならなかった。診断目的で隔離コピーのみ `/FORCE:UNRESOLVED` を指定するとheadless粒子captureは完走し、Vulkan/D3D12各150枚とframe60逐次／直接シーク一致を取得できた。frame149は両backendで下端余白0px・差分率3.347%となり、既定ゲートに不合格。これは通常リンクが直った証明ではなく、未解決箇所を粒子経路が実行しなかった場合の隔離診断証跡に限る。Artifact submoduleは変更していない。
- **価値または懸念:** C++ moduleとDLL境界の問題を「implementation objectがビルドされたか」だけで判断すると、symbol import/exportの不一致を見落とす。consumer objectのundefined symbolとDLL export/import libraryを比較し、どの形式がずれているかまで記録する必要がある。
- **次に確認すべきこと:** 19.44のconsumerだけが `dllimport` 属性を含むBMIを使い、provider実装は `dllexport` BMIを使うCMake module ownershipを成立させられるか、または既存C++ module APIの変更なしにツールセット制約を回避できるかを検証する。ユーザーからArtifactWidgets submodule変更の許可を得たが、修正設計は未確定であり、まだ追跡ファイルは変更していない。正規リンク後、キャプチャを再実行して診断証跡との一致を確認する。
- **未検証:** 他のArtifactWidgets module classで同様のdata symbol不一致が起きるか、`FloatColorPicker`固有か、全体リンクを成立させる正式な修正、診断リンクで得たピクセルと通常リンク製品の一致。

## 2026-11-20: MSVC 19.44 module IFCでQ_OBJECTのdata exportを明示する
- **関連:** `ArtifactWidgets/include/Dialog/FloatColorPicker.ixx`、Particle2DPlaygroundの通常リンク評価、`experiments/Particle2DShapeTarget/v33_diagnostic_manifest.json`。
- **確認できた事実:** MSVC 19.44ではclass-level `dllexport`のC++ module IFCをimportしたconsumer objectが`staticMetaObject`を直接参照し、DLL import libraryの`__imp_`と一致しなかった。Qt6の`Q_OBJECT` expansion内でstatic data memberだけを`WIDGET_LIBRARY_DLL_API`付きで宣言し、クラス全体ではなく公開メソッドを個別exportするとconsumer objectが`__imp_`参照になった。通常リンクから`/FORCE:UNRESOLVED`を外したArtifactParticle2DPlayground targetが成功し、Vulkan/D3D12のv33 150フレームcaptureも完走した。
- **価値または懸念:** QtのQ_OBJECT macro expansionをローカルで上書きするため、Qt更新時に`qtmetamacros.h`の宣言と一致するか再確認が必要。影響はFloatColorPickerに限定し、他module classへ一般化しない。
- **次に確認すべきこと:** Qt更新時にmacro expansionを照合し、MSVC 19.44/19.51両方でconsumer objectのimport symbolと通常リンクを確認する。Artifactアプリ本体UI、Particle Inspector、Composition View、実時間GPU再生はこのheadless Playground評価で証明されていない。

## 2026-11-20: XPU 動画 pipeline の lookahead は encoder backend のスレッド親和性で可否が決まる
- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cpp`（`XpuVideoEncodePipeline`、consumer ループの encode 分岐）、`Artifact/src/Render/ArtifactRenderQueueEncoder.cpp`（`NativeFFmpegBackend` / `PipeFFmpegExeBackend`）、`Artifact/include/Render/ArtifactRenderQueueEncoder.ixx`（`encodeOnWorkerThread()`）。
- **確認できた事実:** 動画 consumer は 1 フレームずつ `addFrame` を同期実行しており、`pipeline=on` は警告だけで未実装だった。`NativeFFmpegBackend` は `FFmpegEncoder` の純 CPU 状態で QObject 親和性がないが、`PipeFFmpegExeBackend` は `QProcess` をメンバに持つため、生成スレッド以外から `addFrame` を呼ぶと安全でない。よって bounded lookahead は backend が明示的に許可した場合だけ有効化し、pipe 系は従来の同期経路を維持する。
- **価値または懸念:** 変換・sws・encode を consumer から切り離すと、プレビュー更新・進捗・次フレーム受領が encode と重なる。ただし既定の `Auto` は GPU native → pipe-hw → pipe → native の順で選ぶため、環境によっては pipe が選ばれ lookahead が効かない。効果を測る前に「どの backend が選ばれたか」を必ず確認する必要がある。
- **次に確認すべきこと:** `XPU video pipeline active` と backend 名を同時に記録した bench で、native/GPU 経路の wall-time と出力 hash を比較する。pipe 経路にも適用したければ、backend の open を encode スレッド側で行う設計（生成スレッドの移動）が必要で、単純なフラグ追加では扱えない。
- **未検証:** 実機での wall-time 短縮量、lookahead 深度 2 のメモリ影響（4K で最大 3 フレーム分）、encode 失敗時の進捗表示（最大 2 フレーム先行）。

## 2026-11-20: XPU のマルチプロセス化は「合成を描ける headless EXE」が無いため farm 経路をそのまま流用できない
- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cpp`（`const bool useFarm = false;`、`initializeHeadlessWithAdapter`、`runExternalRendererJob`）、`Artifact/src/Worker/FarmWorkerMain.cpp`、`ArtifactRenderer/src/ExternalFrameRenderer.cpp`、`ArtifactCore/include/IPC/RenderFarmSharedBuffer.ixx`、`ArtifactCore/src/IPC/SharedMemoryRingBuffer.cpp`、`docs/planned/MILESTONE_RENDER_THREAD_SEPARATION_FEASIBILITY_2026-10-16.md`。
- **確認できた事実:** Render Queue の farm 経路は `useFarm = false` で明示的に無効。`ArtifactWorker`（`ARTIFACT_FEATURE_FARM_WORKER`）は JSON の `rendererExecutable` と `renderPayload` を受けて子プロセスを起動するだけで、自身は描画しない。`artifact-renderer.exe` は Qt Core/Gui/Svg の `QPainter` ベース diagnostic レンダラで `unsupportedLayerKinds` を返すため、本番 composition の代替にならない。一方で typed な `RenderFarmSharedBuffer`（`ImageF32x4_RGBA`）と `SharedMemoryRingBuffer` は実装済みだが caller がゼロ。Vulkan は Diligent の process-global function pointer 制約により 1 プロセス内に独立 device を複数作れない（render-thread separation 文書に実測記録あり）。
- **価値または懸念:** 同一 GPU 上でプロセスを増やしても GPU throughput は増えない。プロセス分割が効くのは (a) Vulkan の複数 device 制約の回避、(b) TDR/VRAM/クラッシュの分離、(c) CPU 側前処理とスレッド予算の分離。ただし frame-range 分散の前提である「project を開いて composition を headless 描画する EXE」が未実装で、`OffscreenCompositionRenderer` は production にあるものの `render` CLI は「headless executor is not implemented」で止まる。ここが実装コストの中心であり、farm／RPC／checkpoint の再利用だけでは埋まらない。
- **次に確認すべきこと:** 2 プロセス × 1 adapter で Vulkan device が共存できるか、`OffscreenCompositionRenderer` に project 読み込みと frame range 出力を付けた EXE が成立するか、`RenderFarmSharedBuffer` の 4K 実 throughput とメモリ上限。per-process の XPU スレッド予算（現在は `QThread::idealThreadCount()` 前提）も分割時に再設計が要る。
- **未検証:** ビルド・実測なし。手元に複数物理 GPU/iGPU がないため per-adapter 分離の効果と VRAM 重複コストは推測。

## 2026-11-20: WebP は「単体連番」と「アニメーション」で同じコンテナ名を奪い合う
- **関連:** `Artifact/src/Render/ArtifactRenderQueueEncoder.cpp`（`isImageSequenceContainer` / `defaultOutputPathForJob` / `sequenceExtension`）、`Artifact/src/Render/ArtifactRenderQueueService.cpp`（`const bool isVideo = !isImageSequenceContainer(format) && ...`）、`Artifact/src/Render/ArtifactRenderQueuePresets.cpp`（Animated WebP プリセット）。
- **確認できた事実:** `isImageSequenceContainer()` に `webp` を追加すると、`deriveContainerFromJob()` が拡張子から `webp` を返すジョブの `isVideo` が false になり、Animated WebP プリセットが画像連番経路へ流れる。さらに `defaultOutputPathForJob()` も `.webp` ファイルではなく `*_sequence` ディレクトリを返すようになり、既存プリセットの既定出力先が壊れる。
- **価値または懸念:** 出力形式表を一元化するとき「対応形式を増やす」と「既存の別経路を壊す」が同じ行で起きる。コーデック（`webp` は animation、`png` は still）で経路を分岐させる必要があるが、今回はリスクを避けて WebP を連番コンテナに追加せず、`sequenceExtension()` の拡張子解決だけを追加した。
- **次に確認すべきこと:** Animated WebP と WebP 連番を意図的に共存させるなら、`isVideo` と `defaultOutputPathForJob` を codec で分岐させたうえで UI に両方の選択肢を出すこと。未検証（ビルド・実機なし）。

## 2026-11-19: Layer Variant の専用テスト分割で責務を見失わない
- **関連:** `tests/Artifact/VariationSystemContractTest.cpp`、`tests/Artifact/CMakeLists.txt`、`Artifact/src/Layer/ArtifactAbstractLayerPersistence.cpp`。
- **確認できた事実:** Layer Variant の契約テストは既に `ArtifactVariationSystemContractTest` として登録済みで、opacity override、reset、extract/insert、JSON round-trip を確認している。同ファイルには Composition responsive/state variant も同居する。JSON reader は active index を `size_t` へ変換した後、variant 数との範囲比較で範囲外を 0 に戻す。負数も unsigned 変換により範囲外となるが、この挙動を直接固定するケースは既存 suite にない。
- **価値または懸念:** 新しいテストプロジェクトを重ねる前に、既存ターゲットを専用 suite として整理し、異常入力・Undo/Redo・保存再読込・協調切替の契約ギャップを埋めると重複を避けられる。
- **次に確認すべきこと:** 負の index を含む不正 JSON の期待動作を専用 suite で固定し、variant create/activate undo-redo と collaborative apply の既存経路をテスト可能な形で確認する。テスト実行はユーザーの明示許可後に行う。

## 2026-11-19: 合成仕上げの「UI も中身も無い」は古い、残るは既知ギャップと runtime のみ
- **関連:** `docs/analysis/COMPOSITING_FINISHING_ACCEPTANCE_MATRIX_2026-11-19.md`、`docs/technical/MATTE_BLEND_COMPOSITING_ORDER_2026-07-03.md`、`docs/analysis/KEYING_EFFECTS_RD_2026-10-26.md`、`docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md` 2026-08-25 項。
- **確認できた事実:** Track Matte は評価順・4 MatteType・Alt-drag/cycle 拒否・Undo・BGRA 論理化が静的に充足。Keyer は 39 件契約テストが 2026-10-27 に pass。Motion Blur は velocity ターゲット未生成バグが 2026-08-25 に修正済みで、shutter/phase UI も存在する。旧ギャップ表（06/07 月）の「未実装」表現は実態と一致しない。
- **価値または懸念:** 真の残課題は (1) runtime 受入れ（複数 matte の実コンポ、CPU/GPU parity、Motion Blur ON/OFF）と (2) 実装側の既知ギャップ（IBK の copy ID 保存復元、clean plate の packaging 未対応、Opaque メタデータ不一致、2D velocity AOV 欠如、layer transform multi-sample 未完了）の 2 層に分かれる。特に IBK の保存・移動系はユーザー被害に直結する。
- **未検証:** runtime 受入れ（ビルド・実機禁止のため未実施）。既知ギャップは静的コード-path  finding であり実素材での再現は未確認。
- **次に確認すべきこと:** 子リポジトリ変更（B-7/B-8）は AGENTS.md の明示依頼ルールに従い先に許可を取る。ビルド許可が得られたら A-4/A-5 と C-1/2/3 の新規実装不要ケースから受入れる。


## 2026-11-19: パーティクルの「地形由来放出」は 1 個の Surface エミッターより ridge 上の複数 Point エミッターが均質になる
- **関連:** `tools/ParticleMirPlayground/main.cpp`（`makeRidgeEmitterPositions` / `makeEmberParams`）、`tools/ParticleMirPlayground/EVALUATION_PLAN.md`、`captures/mir-particle-20261119-v16/IMAGE_EVALUATION_REPORT.md`。
- **確認できた事実:** `ParticleEmitter::setEmissionTriangles()` に地形全体の三角形列を渡す（`EmitterShape::Surface`）と、面積重みサンプリングの偏りにより ridge の一部に明確な隙間（x=15-20% / 50-55%）が残った。このため 64 個の `EmitterShape::Point` エミッターを ridge 上へ等間隔配置すると隙間は解消した。ただし**全エミッターが同一 `randomSeed` を使うと同一乱数列が使われるため、周期的な鋸歯状反復と二重点（縦に並んだペア）が現れた**。エミッターごとに `randomSeed = base + i*7919` で分散すると反復も解消した。
- **価値または懸念:** 「1 エミッターにメッシュを渡す」方式は面積分布に支配され、形状が均一な帯（ridge）だと端に粒子が集まる。**同一シーン内で複数のエミッターを使う場合、シード分散は必須**（同一シードは正しさではなく「見た目の周期」として破綻として出る）。これは Surface 放出に固有ではなく、等間隔 Point 配置全般に通用する。
- **次に確認すべきこと:** `setEmissionTriangles()` を使う他の実装箇所（Shape/Surface （FromMesh シェーダ経路）が同じ偏りを共有していないか。`ParticleEmitter::setParams()` がシードを保持するか（現状 `setParams` は randomSeed を触らないため、setParams 呼び出しごとに乱数が巻き戻る可能性）。
- **未検証:** 2D で Point 化してから 3D は Surface のまま（3D は地形全体を覆うため面積重みでも偏りが出ない）。複数エミッター時の性能と、同一シードのテスト確認。

## 2026-11-19: 2D 合成座標系の粒子は「フレーム番号 × 初速」で到達距離が律速される（重力だけでは登らない）
- **関連:** `tools/ParticleMirPlayground/main.cpp`（`capture2D` の 2D パラメータ）、`captures/mir-particle-20261119-v16/IMAGE_EVALUATION_REPORT.md`。
- **確認できた事実:** 2D キャプチャのフレーム 45 は 30fps で 1.5 秒のシミュレーションに相当する。初期設定（初速 60-190、重力 55 下向き、drag 0.35）では 1.5 秒間の到達距離は上昇 90px 程度に収まり、視覚採点で「上部の 90% がベースライン付近に留まる」と判定された。重力を 0、初速 170-430、drag 0.02 へ変更すると到達距離が 250-640px となり、Y 分布が [813, -102] まで広がって鉛直分散が可視になった。**life 後半の scaleEnd / opacityEnd を 0 に近づけると上方の粒子が背景と同化して見えなくなる**ため、scaleEnd 1.8-4.0 / opacityEnd 0.45 に緩める必要があった。
- **価値または懸念:** 「煙が登って見える」かどうかは速度設定だけでは決まらず、**キャプチャするフレーム時刻（=到達可能な距離）と life 後半の減衰**の組合せで決まる。画像評価の「鉛直分散不足」は物理不具合ではなく、パラメータと評価フレームの不整合として読むべき。粒子上端の減衰を緩めるのは 시각적視認性のための意図的な選択であり、寿命曲线として正しい Porter-Duff 減衰ではない点に注意。
- **次に確認すべきこと:** 評価フレームを 45 固定ではなく「フレーム 45/60/90」の複数점으로採点し、到達距離とキャンバス外抜けのバランスを別途指標化する。
- **未検証:** ソフトレンダラー経路、粒子テクスチャ／アトラス経路、HDR 値域。

## 2026-11-19: 3D の「地形が画面下部に浮く」問題は地形サイズとカメラ距離の比で決まる
- **関連:** `tools/ParticleMirPlayground/main.cpp`（`makeMirTerrainSettings` / `setCamera` / `capture3D`）、`captures/mir-particle-20261119-v14/IMAGE_EVALUATION_REPORT.md`。
- **確認できた事実:** 初期状態は地形 1920×1080・カメラ距離 1450・画角 45° でごく、視覚採点で「地形が画面中央下部に孤立した浮遊スラブとして見え、下端の生えたポリゴン境界が露出する」「上部に 50% 以上の空白」と判定された。地形を 2600×2600 に拡張し、注視点を地形面（Y=-60）へ寄せ、画角 58°・距離 1150 に詰めたところ、地形は画面高の約 75-80% を占めるようになり、下端も画面外へ抜けた（境界露出なし）。同時に 2 倍解像度で描画してダウンサンプルしたところ、遠景稜線のサブピクセル aliasing も解消した。
- **価値または懸念:** 3D 地形がフレーム内で「浮遊スラブ」として見えるのは**地形の有限サイズが露出する**のが原因で、ジオメトリの破綻ではない。したがって対策はジオメトリ修正ではなく、地形サイズの拡張かカメラ構図の詰めである。手続き地形はカメラが常に有限 patch を見る限り、この「浮遊」現象は避けられない。スーパーサンプリングは遠景 aliasing の低コストな対策で、描画コストは 4 倍まで増えるが、R&D のキャプチャ用途では許容範囲。
- **次に確認すべきこと:** 実際の Mir/Tao レイヤ（`ArtifactProcedural3DLayer`）も同じカメラ設定で同じ問題を起こさないか（本次は `Procedural3DGenerators` を直接呼び出したため、既存レイヤの `drawMesh` 経路とは組成が異なる）。
- **未検証:** 実アプリ経路（コンポジション内でのカメラ）での確認、既存 `ArtifactProcedural3DLayer` 経由での同条件確認。
## 2026-11-19: Shape Target の形状キャッシュは Generator ではなく Particle Layer 境界で解決する
- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cpp`、`Artifact/src/Generator/ArtifactParticleGenerator.cpp`、`Artifact/src/Layer/ArtifactShapeLayer.cpp`、`Artifact/src/Layer/ArtifactTextLayer.cpp`、`docs/planned/MILESTONE_PARTICLE_2D_SHAPE_TARGET_2026-11-18.md`。
- **確認できた事実:** `ParticleEmitter::simulateStep()` は粒子ごとの `updateParticle()` と effector を固定ステップで処理し、`Particle::id` は emitter ごとの生成順で割り当てられる。ShapeはParticle Layerで正規化済み可視fillをtriangulateして点を作る。`ShapePath::contains()`は毎呼び出しflattenSubpathsとsegment buffer構築を行っており、4096候補点ごとに呼ぶ経路を削除した。Textのglyph snapshot API自体はoutlineを返さず、layerの静的samplerを使う。Form ParticleのLayerMapは画像入力でShape/Text pathを直接サンプルするAPIではない。
- **価値または懸念:** composition参照・geometry・文字outlineの解決をGeneratorに追加すると共有3D経路と固定ステップの責務を広げる。Particle Layer側でbounded point arrayを作り、Generatorへ値として渡す境界で実装した。Shape/Textのcontent revisionとlayer transformsを使い、geometry変更時だけ点列を作り直す。重複`contains()`検査を除いた結果、DebugのShape準備時間は約1025msから約11msへ下がり、2回の再評価でVulkan/D3D12画像と粒子位置の一致を維持した。
- **次に確認すべきこと:** Release測定と性能閾値の設定、GPU memory/cache容量計測、Shape triangulationの複雑輪郭・穴と画像maskの一致、Textの複数font/言語・細線保持、実アプリUI経路。
- **未検証:** keyframe geometry/text animation、legacy/非stack multi-content/stroke-only、複数emitter、性能合否とmemory budget、実アプリUIでの操作。

## 2026-11-19: パーティクルのソフトフォールバック経路は GPU 経路より粒子が大きく密に描かれる（背景の alpha 契約も異なる）
- **関連:** `tools/ParticleMirPlayground/main.cpp`（`capture2DSoftware`）、`Artifact/src/Layer/ArtifactParticleLayer.cpp`（`draw()` のソフトウェア分岐）、`Artifact/src/Generator/ArtifactParticleGenerator.cpp`（`render()` の `size = p->scale` と `diameter = size * 2`）、`ArtifactCore/src/Graphics/ParticleRenderer.cpp`（`halfWidth = max(0.375, p.size * 10.0)`）、`captures/mir-particle-20261119-software/`。
- **確認できた事実:** 同一シーン（Mir ridge、64 Point エミッター、frame 45）を GPU 経路とソフトフォールバック経路（renderer 未初期化 → `draw()` のソフトウェア分岐 → `getCachedFrame`）で取得した結果、全画面 covered px は GPU 2073600（背景不透明）/ ソフト 484609（背景透過）、同 crop（700,700 400x300）では covered 比 3.07% 対 15.89%、中央連続 run 長（粒径近似）10px 対 17px、run 数 386 対 922 だった。**背景の alpha 契約が違う**（GPU は不透明クリア、ソフトは透過）ため画素差 max 255 は直接比較にならず、粒径・被覆率で評価した。コード上、ソフト経路は `size = p->scale` を半径として直径 `size * 2` で描くのに対し、GPU 経路の VS は `halfWidth = p.size * 10` で quad 全幅 `2 * halfWidth`（コメント上は `radius scale*10` と一致する意図）。さらに 2D レイヤ経路は `transformParticleRenderData()` で `size` にレイヤ変換 scale を掛けるため、両経路で同じ size でも最終的な見かけ径が変わりうる。
- **価値または懸念:** 画像認識採点を加えた結果、**ソフト経路には描画そのものの破綻がある**ことが判明した。粒子が「柔らかな円形グロー」ではなく中空の輪（リング）／細胞状スペックルとして描画され、明るい核の周囲に硬いオレンジの輪郭が出る。下部には指状の垂れ（finger-like drip）も出る。GPU 経路は同シーンで PASS。**フォールバックを「粗い近似」と見なすと、GPU 非対応環境（CI、ヘッドレス、backend 初期化失敗時）で破綻画像が出ることに気づけない**。粒径・被覆率を route 間で並べて測るのは安い（`compare_backends.py` の ideas を流用できる）上、**視覚モデルにも通す**と輪郭化や不自然な形を検出できる。ただし修正は**製品コード側の parity 問題の可能性**があり、本 R&D の範囲内では修正しない。別 issue として `Artifact` モジュール側で確認すべき。
- **次に確認すべきこと:** `Artifact` 側でソフト経路の半径規約（`size` を半径とするか、`size * 10` とするか）と 2D 経路の scale 乗算の整合を確認する。フォールバック到達条件（GPU draw 拒否時・`requiresCpuFlipbookPath`・texture 読み込み失敗）を洗い、フォールバックが実際にプロダクションで 겪う条件を特定する。
- **未検証:** 3D のソフト経路（`ArtifactParticle3DLayer` は 3D カメラ経路を持たないため未取得）、alpha 合成後の背景差を補正した定量比較、フォールバックが実運用で発火する条件。

## 2026-11-19: generic resident GPU エフェクトは時刻を受け取れない（必要ならパラメータ枠で自前で運ぶ）
- **関連:** `Artifact/src/Render/ArtifactRenderLayerPipeline.cpp`（`ResidentGenericParams` の `params.time = 0.0f` / `params.frame = 0.0f`、`kResidentGenericPrelude`）、`Artifact/include/Effects/ArtifactAbstractEffect.ixx`（`GpuSpatialEffectNode.parameters` は 16 枠）、`Artifact/src/Effects/BurnAway/BurnAwayEffect.cpp`。
- **確認できた事実:** generic resident 経路の cbuffer には `g_Time` / `g_Frame` が宣言されているが、値を書き込む箇所は `params.time = 0.0f; params.frame = 0.0f;` のままで、**常に 0 が渡る**。また `GpuSpatialEffectNode::resolutionScaledParameterMask` を消費するコードは `Artifact/src` および `ArtifactCore` のいずれにも存在せず、ピクセル単位パラメータは LOD 補正されていない。
- **価値または懸念:** 時間アニメ（炎のちらつき、進行ノイズ）を持つエフェクトは generic resident 経路では `g_Time` を使えず、**`EffectContext.timeSeconds` を受け取ってパラメータ枠へ詰める**必要がある。16 枠は埋まりやすいため、設計段階から「時刻・色・数をどの順で 16 枠に収めるか」を決める必要がある。generic 経路を使う限り、スクリーンスペースの時間依存はパラメータ経由に統一される。「ピクセル単位パラメータは preview と final で見た目が変わる」リスクは現時点で回避不能であり、正解は**空間パラメータをすべて正規化する設計**（Burn Away は層高正規化）と「将来マスクが実装されたら再評価」の二段構えになる。
- **次に確認すべきこと:** `resolutionScaledParameterMask` の消費側が未実装なら、pixel 単位パラメータを持つエフェクトは preview と final で差が出る。どの downstream が scalar をスケールする意図だったのか（設計メモ）是否存在を確認する。Burn Away は全て層高正規化のため影響を受けない。
- **未検証:** generic resident 経路の実行時間依存挙動（g_Time が 0 のままでも既存エフェクトが問題を起こしていないか）、`resolutionScaledParameterMask` が将来実装されるか。

## 2026-11-19: レイヤーエフェクト面 Buffers は「B,G,R,A ＋ straight alpha」— 色は channel 跨ぎと alpha 底上げで壊れる
- **関連:** `ArtifactCore/src/Image/ImageF32x4_RGBA.cpp`（`getPixel` は `FloatRGBA(pixel[2], pixel[1], pixel[0], pixel[3])`、`rgba32fData()` は canonical B,G,R,A）、`Artifact/src/Preview/ArtifactPreviewLayerEffects.cpp`（`toQImage()` → `SurfacePixelTarget::Rgba8SrgbStraight`）、`Artifact/src/Effects/LinearWipe/LinearWipeEffect.cpp`（`c.a *= alpha` のみで RGB は据え置き）、`Artifact/src/Effects/BurnAway/BurnAwayEffect.cpp`。
- **確認できた事実:** ① `rgba32fData()` が返すのは **B,G,R,A** 順（`getPixel` は mat[2]=R, mat[1]=G, mat[0]=B にマップ）。② レイヤーエフェクト面の合成は **straight alpha**（プレビュー経路が `toQImage()` で straight sRGB 変換する。premultiplied 前提の合成は `LinearWipe` の「alpha だけ減らして RGB は据え置き」と整合しない）。③ GPU generic resident シェーダは `.bgra` スワizzleで同じ BGRA 規約に従う（LinearWipe シェーダの `.bgra` 入出力を参照）。
- **価値または懸念:** **CPU エフェクト実装で色（HUD、合成、発光）を触ると、channel 順の误解は「色相が青に抜ける」という形でしか現れず、alpha だけを見るテストでは検出できない。** 逆に straight alpha で「発光分を alpha 乗算」「alpha の底上げ」を入れると、消失領域に紙の色が残るか発光が半減する。**結果は「合成前提を誤ると画像評価でしか気づけない」ため、画像エフェクトは alpha だけでなく色相と合成結果まで検証する設計にしないと壊れたまま出荷される。** なお、Buffer 順と alpha 契約はエフェクトごとに同じである保証がなく、既存エフェクトが RGB を据え置くのは「straight alpha だから正しい」ためで、premultiplied を前提に RGB を据え置く実装は正解にならない。
- **次に確認すべきこと:** `SurfaceColorDescriptor` が持つ `SurfaceAlphaMode`（Premultiplied / Straight / Opaque）をエフェクト側で参照すべきか。現状は `ImageF32x4_RGBA` の既定 descriptor が unknown で、エフェクトは「straight alpha が前提」の暗黙契約に乗っている。descriptor を参照して alpha 契約を明示的に固定できるなら、Straight 固定か動的対応かを設計する。
- **未検証:** GPU 経路の CPU parity（Burn Away は未計測）、premultiplied を前提とした既存エフェクト（該当コードは未検出だが、OFX ホスト経路は未精査）。

## 2026-11-19: 共有リポジトリを他セッションが編集中だと「全体ビルドの失敗」と「自分の変更の失敗」を区別できない
- **関連:** `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cpp`（他セッションが `ChangeCompositionStatesCommand` を匿名名前空間からモジュール級クラスへ移設中）、`out/build/x64-Debug-Agent`（Ninja ツリー）、`tools/BurnAwayLab`。
- **確認できた事実:** 他セッションの未完成リファクタが `error C2888: シンボルは名前空間 'anonymous-namespace' 内で定義できません` を出し、`ArtifactAppRuntime` を含む全ターゲットがリンクできなくなった。原因は自分の変更（`Artifact/include/Generator/ArtifactParticleGenerator.ixx` の ash/ember プリセット）と無関係で、`git status` にも他セッションのファイル（`ArtifactCompositionMenu.ixx/.cpp`）が未コミットで出ていた。**自分の変更側のコンパイルは `cmake --build <tree> --target "Artifact/CMakeFiles/ArtifactAppRuntime.dir/src/Generator/ArtifactParticleGenerator.cpp.obj"` のように TU 単位のターゲット指定で切り分けられる**（このSpecify方法では his のファイルはコンパイルされないため成功した）。
- **価値または懸念:** 共有ツリーでは **「全体の赤 ≠ 自分の赤」**。エラー文件名が自分の担当ファイルでないことを確認してから、次の一手を決める（待つ／自分の TU だけビルドする）。逆に **自分が作業中購のファイルを他セッションに編集されるのは禁止**で、競合解消のために相手のファイルを直すと、そのセッションの in-flight 状態を壊す。
- **次に確認すべきこと:** 同じツリーを複数セッションが共有している場合の進捗共有方法（ビルドロック、作业中ファイルの宣言、分離したビルドツリー）。現状は各セッションが同じ `out/build/x64-Debug-Agent` を共有しており、片方の未完了変更がもう片方の検証をブロックする。
- **未検証:** 単一セッション前提の設計がそのまま運用に適しているか（他セッション数・同時ビルドの頻度）。

## 2026-11-19: 粒子シミュレーションは「1 回の update」でしか進めない — 大きな delta を 1 回渡すと静かに粒子が消える
- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cpp`（`EmitterParams::fixedTimeStep` / `maxSubSteps`）、`Artifact/src/Layer/ArtifactParticleLayer.cpp`（`renderToImage` が `particleSystem->update(deltaTime)` を 1 回呼ぶ）、`tools/BurnAwayLab/main.cpp`（評価ラボ）。
- **確認できた事実:** `ParticleSystem::update(deltaTime)` は固定タイムステップ（既定 1/120 秒）と最大サブステップ数（既定 8）でクランプされるため、**delta 1.5 秒を渡しても実際に進むのは約 0.067 秒分**。粒子レイヤーを `renderFrame(w, h, 1.5f)` のように 1 回で進めると、**粒子が出ない・出方が薄い**状態が「エラーなし」で返る（自分の ash/ember プリセットでは 38 粒しか出なかった）。**1/60 秒刻みで 90 回進ませると 94 粒になり、見た目も Properly になる**。
- **価値または懸念:** ヘッダに `fixedTimeStep` / `maxSubSteps` があるのに、**「1 回の update でどこまで進むか」の上限が API として露出していない**。大きな delta を渡す利用者は「simulation 落ちて particle が出ない」症状を、**エラーを出さずに**受け取る。**ヘッドレス検証で粒子を再現する際は、フレーム単位で進めるのが必須**であり、既存ラボ（`ParticleMirPlayground`）も `goToFrame` を 1 フレームずつ回していたのはこの制約のためと推定される。
- **次に確認すべきこと:** `maxSubSteps` を越えた delta を「時間どおりに成立させる」_large- delta 対応を追加すべきか（コストは dt の切り詰め）、それとも「要求時間が trunc された」ことを呼び出し側に返す API が必要か。現状はどちらも無く、**利用者が particle の出_expected 量から時間発展を推測できない**。
- **未検証:** GPU 経路（Diligent billboard）でも同じクランプが効くか（`updateAndRenderSoftwareFrame` はソフトウェア経路の別実装）。

## 2026-11-19: Shape particle target の点群は描画経路と同じ可視 fill を検証する
- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cpp`（ShapePath内の候補点フィルタ）、`Artifact/src/Layer/ArtifactShapeLayer.cpp`（`nativeShapeFillPathsForParticleTarget()`）、`tools/Particle2DPlayground/main.cpp`（ordered Shape Stack試験素材）、`docs/planned/MILESTONE_PARTICLE_2D_SHAPE_TARGET_2026-11-18.md`。
- **確認できた事実:** VulkanとD3D12 headlessで外側・内側ellipse、Merge Paths/Subtract、Fill/Strokeのordered Shape Stackを描画し、穴中心alpha=0、ring sample alpha=184、sampler pathの穴中心contains=falseを確認した。固定LayerIDのfixtureではGPU描画、ターゲット形状、粒子中心が両backendで一致した。Shape attractionの最新frame 240は穴内1/360、可視ring mask内359/360。穴内の粒子index 320はframe 90の楕円正規化半径0.893からframe 240で0.978へ移動し、内周境界へ近づいていた。ArtifactCoreのShapePath winding-hole triangulation testでは全三角形内のbarycentric gridを走査し、穴被覆は0だったため、現時点の漏れはサンプラー誤生成より有限時間の粒子移動と整合するが、実際のShapeLayer target点列との直接照合は未実施。以前のlegacy subtractive child fixtureは描画 parityを満たさず、現行評価から除外した。
- **価値または懸念:** サンプラー点群だけでなく本番描画から取得したmaskと同じ座標で比較する必要がある。また、別プロセスでのbackend比較ではLayerIDを固定しないと、LayerID由来sampling seedが変化し、backend差とfixture差を混同する。
- **次に確認すべきこと:** legacy / 非stack multi-content / stroke-only / opacity / animated geometryを個別素材化し、対応する実描画経路のmaskとの一致を測る。Composition View / Render Queue経路と複数素材を評価する。
- **未検証:** Composition View / Render Queue、全Shape構成に対するfill-rule parity、性能閾値・Release比較・GPUメモリ予算。

## 2026-11-19: unscanned C++ module implementation は自分の interface IFC producer を先行させないと fresh Release build で解決できない場合がある
- **関連:** `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt`、`ArtifactCore/src/Graphics/Compute/*Computer.cpp` と対応する `include/Graphics/Compute/*.ixx`、`docs/planned/MILESTONE_PARTICLE_2D_SHAPE_TARGET_2026-11-18.md`。
- **確認できた事実:** Release Ninja のfresh buildで `CurveComputer.cpp` / `DuotoneComputer.cpp` が、自身の module interface IFCを見つけられず失敗した。Ninjaのobject queryでは、imported interface `.ixx.obj` は `OBJECT_DEPENDS` に並ぶが、当該実装が属する対応interface `.ixx.obj` は入力に含まれていなかった。対応interfaceを先に明示buildすると各Compute実装が通り、Particle変更の7 sourceもv143 Releaseで個別に通過した。全体ビルドは後続の別Fracture Runtime sourceで停止した。
- **価値または懸念:** source scanを無効にしたmodule implementationで、`/reference` 指定があることだけではproducer IFC生成との順序が保証されない可能性がある。clean buildと既存cache buildで挙動が違うと、Releaseの検証が不安定になり、別のsource errorに見えることもある。
- **次に確認すべきこと:** 対応関係が明示されている全unscanned implementationについて、Ninja graphでown interface object dependencyがあるかを監査し、必要ならArtifactCoreのCMakeでsource pairから依存を生成する。clean Releaseのparallel buildで再確認する。
- **未検証:** 対応interface依存がないimplementation全体の件数、Debug clean buildにも同じraceがあるか、CMake修正の影響範囲。ArtifactCore submodule自体は今回変更していない。

## 2026-11-19: Text particle target の初回準備時間は測定対象を分けて確認する
- **関連:** `tools/Particle2DPlayground/main.cpp`（`renderToImage()` による準備計測）、`Artifact/src/Layer/ArtifactTextLayer.cpp` のglyph outline sampling、`docs/planned/MILESTONE_PARTICLE_2D_SHAPE_TARGET_2026-11-18.md`。
- **確認できた事実:** 現行Debug headless captureの1回/backend計測で、Text target preparation + software layer draw全体が約749ms (Vulkan) / 766ms (D3D12)だった。計測はsampler単体ではなく初回 `renderToImage()` 全体を含む。
- **仮説・未検証:** glyph outline simplification / triangulation がcold-path時間の一部を占める可能性があるが、現計測では内訳未取得のため原因は未確定。旧探索runでは別値だったため、単一fixture・単一processでの再現性も未検証。
- **価値または懸念:** target更新時に長い同期停止が生じる可能性があり、性能合否を付ける前にsampler、target cache更新、layer drawを別々に計時する必要がある。現時点の約0.75秒をcache生成だけの時間として扱わない。
- **次に確認すべきこと:** 計測区間を分解し、複数runとReleaseで再測定して許容時間を事前に定める。

## 2026-11-19: ネイティブ／GPU エンコーダは alpha pix_fmt を設定できない
- **関連:** `Artifact/src/Render/ArtifactRenderQueueEncoder.cpp` の `NativeFFmpegBackend` / `createVideoEncodeBackend()`、`ArtifactCore/include/Video/FFMpegEncoder.ixx` の `FFmpegEncoderSettings`。
- **確認できた事実:** `FFmpegEncoderSettings` に pixel format／alpha state フィールドが無く、`FFmpegEncoder.cpp` が codec/profile から `AV_PIX_FMT_*` を決める。そのため pipe 側で alpha 対応 pix_fmt を選んでも、native/GPU/auto（GPU 優先）経路では alpha が落ちる。今回は alpha 明示ジョブを software pipe へ自動ルーティングして回避した。
- **価値 / 懸念:** ユーザー明示許可により `FFmpegEncoderSettings::pixelFormat` を追加し、ソフトウェアエンコーダーにのみ適用する形で解決した。HW エンコーダーは固定形式のため対象外で、今も pipe ルーティングで回避している。
- **未検証:** 実ファイルでの alpha 保持、HW エンコーダの capability、H.264/MP4 の alpha 互換性は未検証。
- **次に確認すること:** ビルド許可が出たら `native` バックエンドと software pipe の両方で alpha 保持を実機検証し、HW ルートは capability probe で切り分ける。

## 2026-11-18: 継ぎ目のないテクスチャは「トーラス低域化」で継ぎ目は消えるが質感が落ちる
- **関連:** `experiments/SeamlessTextureLab/`（`EVALUATION_PLAN.md` / `seamless_reference.py` / `captures/rd1/review/RD1_TABLES.md` / `captures/rd1/review/rd1_candidate_blind_sheet.png`）。
- **確認できた事実:** 素の反復 baseline の継ぎ目勾配エネルギー比は 10.3〜62.6（境界が内部より 10〜60 倍強い）。候補 3 種の 3×3 タイリングでの境界輝度段差（clouds_small）は baseline 0.2049 に対し、mirror 0.1148、quilt 0.2046、periodic 0.0364。periodic（巡回 FFT によるトーラス低域化）は境界モードエネルギーがほぼ 0（alpha fixture で 1.9e-17）になり境界が構造的に連続するが、局所コントラストも落ちて質感が弱る。mirror は段差を約半分にするが motif の反転重複が避けられない。descriptor 差（straight / premultiplied 入力の往復）は全候補 0、非有限値 0、近透明 >1 は 0 チャンネル。
- **価値または懸念:** 「継ぎ目比」を同一出力内の比で測ると、低域化で内部エネルギーも落ちるため比は 1 に近づくが改善を隠す。比ではなく**境界輝度段差の絶対値**を主指標にすべき。quilt は 2 次元オフセットの領域を 1 本の縦 DP 経路で接合する単純実装では分離できず、継ぎ目も feather 帯の白線も残った（手法ではなく実装の限界）。
- **次に確認すべきこと:** 閉じた継ぎ目でオフセット領域を囲む quilt の正しい実装。実写テクスチャ（ライセンス確認済み）の取得と、実写での継ぎ目の視認性評価。継ぎ目除去と質感保持のトレードオフをどこに置くか（gate 記入前に決める）。
- **未検証:** gate 未設定のため品質判定ではない。盲レビュー未採点。製品コード／UI／GPU 実装、D3D12／Vulkan parity、実性能は未検証。

## 2026-11-18: レビュー受け入れの run 数ハードコードは R&D を 1 段追加すると壊れる
- **関連:** `experiments/review_harness/cleanroom_acceptance.py`、`experiments/review_harness/validate_runs.py`。
- **確認できた事実:** `cleanroom_acceptance.py` の検査に `validation_19_ok`（runs==19）と `templates_18_pairs`（run 18・生成 36）のように run 数を直接書いたものがあり、PencilPrintLab に RD10 を追加すると validation は 20 run、テンプレートは 19 run になるため両方が FAIL する。run 列挙は `generate_review_templates.py` が `validation.json` から動的に行うため、検査側だけを動的化（`validation_all_ok` と `templates_match_runs`。unique (lab, rd) 数から期待値を計算）した。変更後クリーンルーム受け入れは 16 ツール再生成・8 チェックすべて OK で、実ゲートの閾値記入 0 と findings 空も維持された。
- **価値または懸念:** R&D を 1 段増やすだけで「検証済み」の状態機械が壊れるため、run 数をハードコードした検査は受け入れの前提を暗黙に固定してしまう。検査は「ツール間の整合」を見る形にし、run 数は動的から求めるべき。
- **次に確認すべきこと:** 新規ラボ（MaterialSeparationLab）をレビュー運用へ登録した（2026-11-18）。登録後は validation 22 run すべて OK、クリーンルーム受け入れは 16 ツール再生成・8 チェックすべて OK、実ゲートの閾値記入 0 と findings 空を維持。次は 新規ラボ_material_separation の blind シートと gate 票を別レビュー者が実際に記入する運用をはじめること。`LABS` 定義は `build_review_page.py` / `manage_gate_forms.py` / `validate_runs.py` / `cleanroom_acceptance.py` / `intake_cleanroom_rehearsal.py` の 5 箇所にあり、ラボを増やすたびに 5 箇所すべてを更新する運用が必要（更新漏れは run 数の不整合として cleanroom 受け入れが検出する）。
- **未検証:** 登録後のレビュー者による実ゲート記入（閾値・所見の提出）と再受け入れは未実施。

## 2026-11-18: 整列参照の作り方を全ラボで監査し、欠陥は PencilPrintLab と WatercolorLab の 2 件に絞られた
- **関連:** `docs/analysis/IMAGE_RD_ALIGNMENT_REFERENCE_AUDIT_2026-11-18.md`、`tools/audit_alignment_references.py`、`docs/analysis/alignment_reference_audit.json`。
- **確認できた事実:** `ast` で `experiments/` と `tools/` の関数を単位に解析し、整列メトリクスの参照がどう作られているかを 4 規則（RAW_REFERENCE / TRANSLATION_WARP / NEAREST_WARP / MOTION_REMAP）で判定した。ヒットは 7 件で、人が読むと 2 件が実害、1 件が誤検知、3 件は情報、1 件（RetroImageLab の texture_frequency_suite_study）はマスク回転であり測定参照ではない。実害は PencilPrintLab RD9（生入力を参照 → 撤回・RD10 で是正済み）と WatercolorLab の整列 MAE（参照クラスは正しいが bilinear 0.5k で blur 限界、キー名 `integer_alignment_probe` が整数整列を誤解させる）の 2 件。RetroImageLab の temporal dither study は処理済み `rendered[0]` を numpy の整数スライスで参照しており RD10 の「content-lock 残差 0」と整合して問題なし。GlowLab / RetroImageLab の grain 系は光学フロー remap で参照は処理済み出力のため参照クラスの誤りはない。
- **価値または懸念:** 監査ツールは誤検知を含むため判定そのものではなくレビュー候補の抽出に留めている（誤検知 1 件を実読で潰した実績あり）。WatercolorLab の既存数値は再実行しておらず、構造的な確認にとどまる。
- **次に確認すべきこと:** WatercolorLab に補間なしの厳密シフト系列を追加して整列 MAE を取り直すか、RD10 の方式へ統一するか。WatercolorLab の blind シートは実写素材未取得のため、参照水性画の追加可否。
- **未検証:** 人手 gate と blind review の採点は未実施。既存数値の妥当性判定はしていない。製品 runtime・GPU 実装は対象外。

## 2026-11-18: 解析的な連続閾値場は 0.5px オフセットでも dither コントラストを保つ
- **関連:** `experiments/PencilPrintLab/pencil_reference_v7.py`、`experiments/PencilPrintLab/rd10_continuous_threshold.py`、`experiments/PencilPrintLab/captures/rd10/review/RD10_TABLES.md`。
- **確認できた事実:** Bayer 4×4 テーブルを隣接位相で bilinear 補間すると、0.5 px オフセットの閾値コントラスト比が 0.4728 まで落ちる（dither 強度が半減）。IGN（Jimenez, SIGGRAPH 2014）と R2 低分散列は任意の実数座標で評価でき、比は 1.0000 を維持した。整数シフト系列（1 px/frame、参照は補間なしの厳密シフト）では、dither 単独の content-lock の内側平均 lock-aligned MAE が bayer / ign / r2 いずれも 0.00000000、screen-lock は 0.125〜0.148 だった。descriptor 差 5.96e-08、近透明 >1 は 0、非有限値 0。1 フレーム（512×320）の Python 処理は 0.055〜0.103 秒、tracemalloc peak 30〜42 MB（Python heap のみ）。
- **価値または懸念:** RD8 の bilinear 補間は「位相連続化」を兼ねたが、dither の強度を黙って落としていた。GPU 実装では閾値場の式評価（IGN / R2）がテーブル参照より軽く、content-lock の実装コストも下がる。ただし IGN は hash ノイズ型で順序 Bayer と印象が違い、見た目の評価は未実施。
- **次に確認すべきこと:** 参照 artworks を使った blind review で IGN / R2 と Bayer の見た目の差を評価。motion estimation 付き content-lock を GPU 側で実装しての per-step MAE と lock-aligned MAE の実測。
- **未検証:** gate 未設定のため品質判定ではない。製品 C++／HLSL、実アプリ経路、D3D12／Vulkan parity、実性能。

## 2026-11-18: cv2.warpAffine の INTER_NEAREST は厳密な整数平行移動ではない
- **関連:** `experiments/PencilPrintLab/rd10_continuous_threshold.py::integer_shift`。
- **確認できた事実:** `cv2.warpAffine` を `INTER_NEAREST` + `BORDER_REPLICATE` で整数 dx だけtranslation したところ、dx=1,2 は numpy の完全一致式と一致するが、dx=3 以上は内部座標の固定小数点変換により最大 0.125 の差が生じた。RD10 では numpy のインデックス付け（`idx = clip(arange(w) - dx)`）に置き換えた。
- **価値または懸念:** エフェクト系のオフライン計測で「厳密な整数シフト」を前提にした比較を行うと、わずかな misalignment が測定値を支配しうる。参照を作る処理は、測定したい motion と厳密に一致する実装に揃えるべき。
- **次に確認すべきこと:** 他のラボ（WatercolorLab / RetroImageLab / OutlineLab）に warpAffine INTER_NEAREST を整数参照として使っている箇所があるか。
- **未検証:** 他のラボの該当有無は未調査。

## 2026-11-18: composition 生成・削除が Undo 履歴に載らない可能性
- **関連:** `docs/analysis/DCC_THIN_AREA_SURVEY_2026-11-18.md`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Service/ArtifactProjectService.cpp`。
- **確認できた事実:** composition 系 Undo コマンドは Note / Rename / WorkArea / Settings / ResponsiveLayout / Resolution の 6 件のみ。`CreateCompositionCommand` / `RemoveCompositionCommand` / `AddCompositionCommand` / `DeleteCompositionCommand` は 0 件。`ArtifactProjectService::createComposition()` は `manager.createComposition(params)` を直接呼び、`removeComposition()` も `removeCompositionById` を直接呼ぶ。既存の precomp / unprecomp Undo は別経路（composition 内の入れ子操作）。
- **価値または懸念:** DCC ツールではドキュメント（ここでは composition）の生成・削除も Undo 対象にするのが一般的。マルチシーン導入時は、複数 composition の同時保持に加えて生成・削除の Undo 単位を新たに定義する必要がある。
- **次に確認すべきこと:** UI 側の別 Undo ラッパー、`ArtifactProjectService` を通らない生成経路、runtime の Undo 履歴。
- **未検証:** runtime / build、既存 `CreateCompositionResult` の契約との整合。

## 2026-11-18: effect は asset 参照を共通 property として持たない
- **関連:** `docs/analysis/DCC_THIN_AREA_SURVEY_2026-11-18.md`、`Artifact/src/Composition/ArtifactAbstractComposition.cpp`、`Artifact/src/Effects/ColorCorrection/LUTEffect.cpp`、`Artifact/src/Effects/SurfaceFX/SurfaceFXEffect.cpp`。
- **確認できた事実:** `serializeEffect()` は id / displayName / enabled / mix / properties / modulation 等を保存するが、参照先 asset の共通フィールドを持たない。`LUTEffect` は `LUT File` を文字列 property、`SurfaceFXEffect` は要素ごとに `texturePath` + `assetId` を保持する。効果ごとに形式が異なり、横断的な参照レコードとして扱えない。
- **価値または懸念:** footage/effect の参照元逆引きを実装する前に、effect 側の asset 参照を共通契約（参照 ID + path + 解決規則）へ寄せる必要がある。これは既存の `ObjectReference` property や `SurfaceFX` の assetId を土台にできる。
- **確認結果:** `PropertyType::ObjectReference` は Core で `canConvert<QString>()` として扱われるだけで、参照 ID の検証・解決ロジックは無い。UI のピッカーはあるが共通の参照契約（type/id/解決規則）は無く、effect の asset 参照共通化の土台としては不十分。
- **確認結果:** `SurfaceFXEffect.cpp:100-108` は `assetId` を `AssetDatabase::getAssetInfo(assetId).absolutePath` で解決しており、最も近い既存パターン。ただし effect ごとに手実装で共通契約にはなっていない。`LUTEffect` の `LUT File` は `PropertyType::String` で `setPropertyValue()` から直接 `setFilePath()` を呼ぶだけで、asset identity との接続は無い。
- **次に確認すべきこと:** `assetId` パターンを property contract へ昇格させる場合の後方互換（旧 `LUT File` 文字列 property からの移行）と、Undo / dirty 通知への接続。
- **未検証:** runtime / UI 受入れ、旧プロジェクトの後方互換。

## 2026-11-18: マルチシーン／シーンブレンドが DCC の薄い分野として残る
- **関連:** `docs/analysis/DCC_THIN_AREA_SURVEY_2026-11-18.md`、`Artifact/include/Composition/ArtifactCompositionManager.ixx`、`ArtifactCore/include/Composition/CompositionRegistry.ixx`。
- **確認できた事実:** `Artifact/include/**/*.ixx` を grep し `SceneBlend` / `multiScene` / `MultiScene` / `SceneManager` / `activeScene` / `setActiveScene` は 0 件。存在するのは `ArtifactCompositionManager` と `CompositionRegistry`、`WorkspaceAutomation::changeCurrentComposition` 等、1 つの composition を active に切替える経路と、`ArtifactCompositionLayer` による precomp 入れ子のみ。複数 composition を同時保持し重み付きでブレンドするモデルは現行コードにない。
- **価値または懸念:** 既存の DCC ギャップ文書は 3D / viewport / カラー / キーイング / ワークフローを広くカバーしているが、「コンポジション切替」と「precomp 入れ子」の間のマルチシーン同時保持は未調査だった。Maya scene blend / UE level blending / Blender linked scene に相当する空白。
- **次に確認すべきこと:** `ArtifactCompositionManager` と `CompositionRegistry` の lifecycle、Undo 境界、render controller が複数 composition を扱えるかを実コードで追う。
- **未検証:** runtime / UI 受入れ、既存スレッド契約との整合。
- **補足:** `AssetInstance` / `instanceId` / `refCount` も 0 件で、WORKFLOW_GAP のアセット共有指摘は現在も未解消（既存指摘の再確認であり新規候補ではない）。

## 2026-11-18: Contour Poster CPU R&D照合で読み書き分離の必要性を確認
- **関連:** `Artifact/src/Effects/ColorCorrection/PosterizeEffect.cpp`、`CreativeEffectsLab/studies/contour-poster/compare_product_cpu.py`、`captures/contour-poster-product-cpu/`。
- **確認できた事実:** 近傍勾配を計算するCPU処理が並列行ループ中に同じ画像へ書き込むと、隣接行の値が更新済みかどうかで結果が変わり得る。source viewから読む形へ変更後、48x64のlinear premultiplied RGBA float32 fixtureでPython参照との可視画素RGB最大差は2.98e-8、alpha差0だった。
- **価値または懸念:** 近傍フィルタは in-place 並列化するとデータ競合・非決定性を生じる。source/destination分離なら追加の全画像scratch確保を避け、ホットパスの再確保を増やさずに参照を固定できる。
- **未検証:** GPU shader parity、D3D12/Vulkan、実画像の画質、性能。
- **次に確認すべきこと:** GPU経路の実画像 capture と時間・backend比較を同じfixture契約で行う。
## 2026-11-18: CC0 人物写真の ICC 記録と実ファイルが不一致
- **関連:** `experiments/ColorMatchLab/assets/WikimediaCC0/SOURCES.md`、`Pexels-photo-707448-1280.jpg`。
- **確認できた事実:** 文書は ICC なしと記載するが、記録された SHA-256 と一致するローカル JPEG を Pillow 12.2.0 で読み取ると `icc_profile` 524 bytes が存在する（ICC SHA-256 `61bb24bd075ae1179b562cb484a2737fa63bcf35133a951843db1f1f7c97ce7f`）。
- **価値または懸念:** sRGB と仮定した既存の画像研究の色条件の説明に影響する可能性がある。実際の色差・既存評価への影響は未検証。
- **次に確認すべきこと:** ICC の内容と既存の画像読み込み経路、変換の有無を監査し、必要なら出所記録と影響する評価を訂正する。

## 2026-11-18: D3D12/Vulkan 描画効率の既知ボトルネックは「計測済み」ではなく「静的診断済み」
- **関連:** `docs/analysis/DILIGENT_PREVIEW_BOTTLENECK_MEMO_2026-08-27.md:9-16,31-44`、`docs/technical/GPU_CULLING_DESIGN_2026-10-18.md:5,13-27`、`docs/technical/GPU_RENDER_OPTIMIZATION_TECHNIQUES_AE_STYLE.md:9-15`、`docs/planned/MILESTONE_RENDER_THREAD_SEPARATION_FEASIBILITY_2026-10-16.md:18,645-649`、`Artifact/src/Render/DiligentDeviceManager.cpp:1151-1223`、`Artifact/src/Render/ArtifactIRenderer.cpp:5644-5786`。
- **確認できた事実:** プレビューの `2N+1` 全画面 convert/blend/Flush、同期待ち readback 2〜8ms/レイヤー、スプライトの IMMUTABLE テクスチャ生成は静的トレースで特定済み。カリング設計文書は「実装なし」とする一方、古い AE 風最適化文書は「実装済み」と記載し矛盾する。描画スレッド分離は独立テストのみ通過で本番は両 backend とも GUI スレッド。Artifact 側の GPU timestamp / RenderCostStats / Profiler は配線済みだが既定オフで baseline 未取得。
- **価値または懸念:** 「実装済み」と書かれた数値（`docs/technical/GPU_RENDER_OPTIMIZATION_TECHNIQUES_AE_STYLE.md` の 100× 等）は計測根拠がなく、設計判断に使うと誤る。Vulkan は共有 immediate context・`FinishFrame()` ヒープ回収・グローバル Vulkan 初期化の制約があり、D3D12 と同じ改善がそのまま効かない。
- **未検証:** いずれも実機 budget / backend parity 未計測。Present が VSync 固定のため、現行フレーム時間からパス単体の重さを分離できない。
- **次に確認すること:** ビルド許可が得られたら、まず D3D12/Vulkan 同一 Fixture で baseline を取得し、2N+1 パスと readback を分離してから改善候補を選ぶ。古い最適化文書の「実装済み」記載はコード照合で更新する。

## 2026-11-18: 自動 UI Fixture の frame-debug JSON は timing 未取得のまま出力される
- **関連:** `Artifact/src/AppMain.cpp:10179-10215`、`Artifact/src/Render/ArtifactIRenderer.cpp:5285-5348`、`C:/Users/lagma/AppData/Local/Temp/opencode/glow-dx12.png.frame-debug.json`。
- **確認できた事実:** `ARTIFACT_UI_TEST_GLOW=1` で `ArtifactUiTest.exe` を起動すると `*.frame-debug.json` が自動保存されるが、`renderLastFrameMs` / `renderAverageFrameMs` / `renderGpuFrameMs` はすべて 0、`renderGpuTimingAvailable=false`。Fixture は `renderOneFrame()` を2回呼ぶだけであり、DURATION query は非同期で数フレーム後にしか結果が返らない。添付 viewport テクスチャも 1x1。
- **価値または懸念:** この JSON は「計測できた」証拠にならない。0 値を baseline として扱うと 2N+1 パスや readback の改善判断を誤る。
- **未検証:** 通常 GUI で十分なフレームを描画した後の `FrameDebugSnapshot` では timing が更新されるか。`ARTIFACT_ENABLE_GPU_FRAME_QUERY` と `beginFrameGpuProfiling()` の接続はコード上存在する。
- **次に確認すること:** 実測ではウォームアップ後に複数フレーム描画してから snapshot を取得する。または製品側の Perf Baseline エクスポートを人手で取得し、入力・解像度・backend・フレーム番号を揃えて比較する。

## 2026-11-18: VP チャンネル表示の 3 モードと Isolation スナップショットは「器だけ」だった
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp`（presenter switch と `composeViewportChannelOverlayImage()`、`isolatedLayerIds_` / `isolationSetRevision_`）、`ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`、`docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`。
- **確認できた事実:** `ViewportChannelDisplayMode` の Unpremultiplied / Luminance / Matte は enum・メニュー・HUD ラベルがあるだけで、presenter と CPU fallback の switch に case がなく beauty が表示されていた（2026-11-18 に配線して修正済み）。Isolation についても `isolatedLayerIds_` / `isolationSetRevision_` が Impl に宣言されている一方で参照箇所が 0 件で、`setShowIsolationOverlay()` は bool トグルのみ。
- **価値 / 懸念:** 「UI 項目がある＝実装済み」とドキュメントのステータスだけを信じると誤る。両者とも静的コード照合で初めてギャップが判明した。
- **次に確認すること:** P1-1（Isolate 状態復元）と P1-9（Local View 分離）は snapshot 機構を共有すべきなので、どちらを先に作るか決めてから着手する。
- **未検証:** 3 モードの実機表示と D3D12/Vulkan parity は未確認（ビルドは成功）。

## 2026-11-18: 質感分離のベース/ディテール分解では「premul だけ層分解して alpha を分解しない」と近透明で除算が破綻する
- **関連:** `experiments/MaterialSeparationLab/material_reference.py`（`decompose` / `recompose` / `unpremultiply`）、`experiments/MaterialSeparationLab/captures/rd1/metrics_rd1.json`。
- **確認できた事実:** 質感分離ラボの当初実装は premultiplied RGB のみ guided filter / gaussian で層分解し、alpha は未分解の入力を unpremultiply の除数にしていた。この状態で `detail_gain=0`（base のみ）の出力を alpha 境界 fixture で測ると、近透明領域の straight RGB 最大値が A 6.25 / A(wide) 8.24 / C 4.85 / B 0.60 となり、入力からの最大差も同程度（6.22 / 8.21 / 4.82 / 0.53）に跳ねた。alpha も同じ近傍演算で分解し、recompose した層に対応する alpha を除数に使う（出力 alpha も同じ値）形に変更すると、neutral（gain 全 1.0）の premultiplied 再合成誤差は最大 2.33e-10（A）〜5.96e-08（B）、straight 入力と premultiplied 入力から復元した straight RGB の最大差は全 10 ケースで 0.0 になった。
- **価値または懸念:** 「RGB と alpha を同時に近傍処理し最後に一度だけ unpremultiply する」だけでは不十分で、**除数として使う alpha は premul と同じ層分解に由来する必要がある**。層分解の実装研究发现は「どの三次元で layers を切るか」だけでなく「除数の provenance」まで契約に含めるべき。upper bound は MAX_STRAIGHT=16.0 のクランプで担保しており、NaN / 非有限値は 0 のまま。
- **次に確認すべきこと:** base のみ出力の近透明 6〜8 倍は guided filter の base が alpha の急変点で alpha に対して非比例に energie を運ぶためと考えられるが、原因の切り分けは未実施（guided filter の係数が alpha 境界で線形伝播しないためと推測）。目視で halo として見えるかは blind シート未採点のため判断不能。
- **未検証:** gate 未設定のため品質判定ではない。GPU 実装、実アプリ経路、D3D12/Vulkan parity、実写素材での確認。

## 2026-11-18: 分離された base 層は「局所コントラストの比」を維持しないため、質感の有無は比ではなく絶対 std で測る必要がある
- **関連:** `experiments/MaterialSeparationLab/rd0_baseline.py`（`local_contrast`）、`experiments/MaterialSeparationLab/rd1_candidates.py`、`experiments/MaterialSeparationLab/captures/rd1/review/RD1_TABLES.md`。
- **確認できた事実:** texture fixture（多オクターブ value noise）の平坦部 luma std は 0.000446、テクスチャ部 luma std は 0.016512 で比は約 37。候補の出力（gain=1.0）でこの比は 37.024 のまま変わらない（層分解が両者を同じ量だけ変調するため）。一方強調型 C（coarse 1.4 / fine 1.8）では平坦部 std が 0.000446 → 0.000794、テクスチャ部 std が 0.016512 → 0.018012、比は 37.0 → 22.7 と**下がる**。つまり「局所コントラスト比」を質感量の指標として使うのは不適。
- **価値または懸念:** 局所コントラスト比は「平坦部に質感が入っていないか」を見る定性指標としては使えるが、質感量の指標としては不適。用いるなら「入力比 / 出力比」の比を採るなど正規化が必要で，最好は平坦部 std とテクスチャ部 std の**絶対値**を別々に記録する形（RD1 は既に両方を記録している）。また fixture の平坦部は厳密にノイズ 0 にすると std=0 となり比が無限大になるため、センサノイズ級の floor（0.0006）を入れておく必要がある。
- **次に確認すべきこと:** 実写素材（質感を持つ実画像）が未取得のため、absolute std が実際の質感差とどう対応するかは未検証。gate 設定時に「局所コントラスト保持」の判定方法を確定する必要がある。
- **未検証:** gate 未設定・人手レビュー未実施。

## 2026-11-18: 質感分離ラボ（候補 18）を新設し、RD0/RD1 のオフライン測定を完了した
- **関連:** `experiments/MaterialSeparationLab/`（`EVALUATION_PLAN.md` / `README.md` / `generate_fixtures.py` / `material_reference.py` / `rd0_baseline.py` / `rd1_candidates.py`）、`docs/planned/MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md`（候補 18）。
- **確認できた事実:** 決定論的 fixture 11 件（seed 20261118、512×320 と 1536×1024、alpha fixture の premultiplied 版を含む、`--check` でハッシュ一致確認済み）と baseline 30 出力（RD0）、候補 5 種（A 自己二層分解 ×2 / B 周波数分離 / C 強調型 ×2）の出力 200 点と記述的測定（RD1）を完了。候補 5 種すべてで descriptor 差（straight 入力と premultiplied 入力から復元した straight RGB の最大差）は 0.0、neutral 設定の premultiplied 再合成誤差は最大 2.33e-10（A）〜5.96e-08（B）、非有限値 0、近透明領域の >1 チャンネル数 0。実行時間は 512×320 で 0.017〜0.059 秒、1536×1024 で 0.20〜0.59 秒（Python heap の tracemalloc peak 17〜246 MB）。中間調ランプは候補 4 種で入力と同一（step mean 0.002093、step std 0.000081、単調 fraction 1.0000）、強調型 C のみ step std 0.000124 に増える。
- **価値または懸念:** 「大まかな色と細かな質感を独立に調整する」入口として、base / detail を同一 fixture 上で比較できる状態になった。ただし **gate（合否閾値）は未設定**であり、blind シートと採点フォームも空のままなので、候補の優劣・採用判断はしていない。DetailSharpeningLab（ディテール強調）とは対象が重複しないことを確認した（分離 vs 増幅）。
- **次に確認すべきこと:** 別レビュー者による baseline 閲覧後の gate 記入（§5.0）、人手 blind review の実施、実写質感素材の取得。
- **未検証:** 人手レビュー、順位付け・採用判断、製品コード／UI／Undo・保存再読込、preview/final 経路、D3D12/Vulkan parity、1080p/4K 実性能、Undo 経路。

## 2026-11-18: モーショングラフィックスの素材は揃っているが「一括で良い感じにする」統合レイヤーが無い
- **関連:** `ArtifactCore/include/Animation/KeyframePatternGenerator.ixx`（:19-53 の 12 パターン、:268-445 の生成実装）、`ArtifactCore/include/Animation/EasingCurveUtil.ixx`（:15-38 の 22 種）、`Artifact/src/FxStudio/ArtifactFxPresetCatalog.cpp`（:21-35 の 5 プリセット）、`Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cpp`（:1489-1510 のプリセットライブラリ、:1324-1331 の EasingLab／Key Pattern Dialog）、`ArtifactCore/include/Text/TextAnimator.ixx`、`docs/analysis/AE_PAIN_POINT_IMPROVEMENT_MAP_2026-08-13.md`（:84-88）。
- **確認できた事実:** パターン自動生成・イージング・テキストアニメーター・Expression（`wiggle` / `loopOut`）・エフェクト群・35 種ブレンド・16 種トランジションは実装済みで UI にも配線されている。一方、ピックウィップは未実装、`.mogrt` 相当のワンクリック完成演出は構想のみ。FX Studio の `ArtifactFxStudioDialog.cpp:24` の `sample()` は Cue をテキスト表示するだけで、コンポジション実描画への接続はこのコードからは確認できない。
- **価値または懸念:** 「かっこよさ」の材料ではなく**組み合わせの導線**が不足している。新規エフェクトを増やすより、`KeyframePatternGenerator` / `EasingCurveUtil` / `FxPresetCatalog` / `EffectService` を束ねるだけで体感速度が上がる可能性が高く、既存部品の再利用でリスクも低い。
- **次に確認すべきこと:** FX Studio の Cue が実際にレンダリングへ反映される経路の有無。プリセットライブラリ→EasingLab→FX Studio の連携 UX。ピックウィップの実際の需要。
- **未検証:** 実機 UI 操作・ビルド・描画反映は未確認（ユーザー明示指示がないため実行していない）。一部ドキュメントの古い判定（TextAnimator 30%／Graph Editor 未着手等）は現行コードで訂正済みで、コード側を正とする。
## 2026-11-17: サンドボックスで閾値記入済みの真の scoreable 遷移を検証
- **関連:** `experiments/review_harness/threshold_entry_rehearsal.py`、`experiments/review_harness/downstream_gate_report.py`、`experiments/review_harness/out/threshold_entry.json`。
- **確認できた事実:** ハーネスと rd5 を `out/sandbox/` へ複製し、複製の GATE_FORM 7 欄へ閾値を記入、gates_fixed=true の所見を置いて実パイプラインを実行した。6 ステップすべて OK で、5 チェック（閾値記入 7 欄、thresholds_entered_runs=1、findings_state=scored、gate_claim_mismatch=false、Consumer の scoreable に含まれる）がすべて成功。下流 Consumer は gate_status.json から CSV/Markdown を生成（18 run）。一方、実リポジトリは thresholds entered 0・findings_state すべて none・GATE_FORM 記入欄 0・scoreable 空を維持。
- **価値または懸念:** 「報告上の scored」と「実際に閾値が記入された scoreable」を同じパイプラインで両方検証できた。実ゲートを汚さずに移行経路をテストできるため、レビュー運用のリハーサルとして使える。
- **次に確認すべきこと:** レビュー者による実 GATE_FORM への閾値記入と所見提出。
- **未検証:** ブラウザー表示、実際のレビュー判定。

## 2026-11-17: 状態遷移リハーサルで「報告上の scored」と「実際の閾値記入」を分離
- **関連:** `experiments/review_harness/state_transition_rehearsal.py`、`experiments/review_harness/example_api_consumer.py`、`experiments/review_harness/out/state_transition.json`。
- **確認できた事実:** `findings_state`（none/advisory/scored）と `gate_claim_mismatch` を導入し、一時サンプル所見で9チェックの状態遷移リハーサルを実行した。gates_fixed=false で advisory、true で scored（報告上）へ遷移する一方、GATE_FORM の閾値欄は空のまま（thresholds_entered_runs=0、gate_form_status=thresholds_unset）。不整合フラグが立ち、API Consumer はその run を scoreable として扱わなかった。サンプル削除後は none に戻り、統合リハーサルも5/5 OK。
- **価値または懸念:** 「レビュー者が gates_fixed と報告した」ことと「実際に閾値が記入された」ことは別であり、工具側で突き合わせないと未設定ゲートが採点済みと誤認される。この分離により、ゲート未設定という未達状態が正直に保持される。
- **次に確認すべきこと:** レビュー者が実際に GATE_FORM.md へ閾値を記入した後の再リハーサル。
- **未検証:** ブラウザー表示、実際のレビュー判定。

## 2026-11-17: 整数シフト参照では content-lock の整列優位が消え per-step MAE が増える
- **関連:** `experiments/PencilPrintLab/rd9_integer_lock.py`、`experiments/PencilPrintLab/captures/rd9/review/RD9_TABLES.md`。
- **確認できた事実:** frame 0 を整数 k px だけ INTER_NEAREST で動かした画像を参照とし、content-lock 側にも同じ k を与えて dither 追従を測った。整列 MAE は screen 0.1492〜0.1493、content_nearest 0.1492〜0.1493、content_interp 0.1492〜0.1493 で候補差なし。per-step global MAE は screen 0.0023〜0.0026 に対し content 系 0.253〜0.254 と約 100 倍。descriptor 差 5.96e-08、近透明 >1 は 0。
- **価値または懸念:** RD6 の「content-lock で偶数フレームの整列 MAE が改善」は、0.5px warp のボケ具合と dither 位相の偶然の一致によるものだった可能性が高い。参照を正しく定義すると、content-lock は dither パターンを動かす分だけフレーム間差を増加させる。整列 MAE の比較だけでは「追従しているか」を判定できず、知覚品質には per-step の揺れも含めた評価が必要。
- **次に確認すべきこと:** 整数シフト版の blind レビュー、dither を動かさずに高速化する方針、実動画での知覚確認。
- **未検証:** モーション推定、実作品参照、製品経路、GPU parity、実性能。

## 2026-11-17: サブピクセル位相補間だけでは奇数フレームの整列残差はほぼ消えない
- **関連:** `experiments/PencilPrintLab/pencil_reference_v6.py`、`experiments/PencilPrintLab/captures/rd8/review/RD8_TABLES.md`。
- **確認できた事実:** Bayer の隣接位相を offset の小数部で双線形補間する v6 を、整数切り捨ての nearest と比較した。奇数フレーム（0.5px オフセット）の整列 MAE は nearest 0.127〜0.134 に対し interp 0.1266〜0.1267 で、改善は 1% 未満。偶数フレームは nearest 0.005〜0.015 が interp 0.008〜0.021 より小さく、補間はむしろわずかに悪化。不透明差は nearest に対する interp の最大が 0.333〜0.733、descriptor 差 5.96e-08、近透明 >1 は 0 を維持。
- **価値または懸念:** 位相の連続化はコンテンツ追従の必要条件だが十分ではない。frame 0 を平行移動して作る比較自体が双線形補間でボケるため、dither 側の位相を補間しても「 warp 後の画像に dither が共変する」関係は単純に成り立たない。知覚品質の差は blind レビューが必要。
- **次に確認すべきこと:** warp 比較の方法自体の検討（サブピクセルシフトの参照生成をどう定義するか）、gate 固定後の blind 比較、実動画でのオフセット推定。
- **未検証:** モーション推定、実作品参照、製品経路、GPU parity、実性能。

## 2026-11-17: content-lock は偶数フレームに効くが 0.5px offset の整数切り捨てが残る
- **関連:** `experiments/PencilPrintLab/pencil_reference_v5.py`、`experiments/PencilPrintLab/captures/rd7/review/RD7_TABLES.md`。
- **確認できた事実:** Bayer 閾値をコンテンツ座標 `(x - 0.5*k, y)` からサンプルする content-locked dither を、既知シフトの制御実験として screen-locked と比較した。整列 MAE は偶数フレーム（k=2,4,6 → offset 1,2,3px）で 0.253→0.008、0.043→0.015、0.253→0.021 へ大幅低減。一方、奇数フレーム（k=1,3,5,7 → offset 0.5,1.5,2.5,3.5px）は 0.127〜0.140 で改善しなかった。現在の閾値ルックアップが offset を int32 に切り捨てるため、サブピクセル成分が失われる。descriptor 差と近透明 >1 は v4/v5 とも健全（差は float 丸め、>1 は 0）。
- **価値または懸念:** 動きに dither を追従させるには、整数座標切り捨てではなくサブピクセル補間（複数位相の閾値を混ぜる等）が必要。実映像ではオフセットを推定する必要があり、本実験は既知シフトの制御条件に限られる。
- **次に確認すべきこと:** サブピクセル位相補間方式の別 run、実動画でのオフセット推定、知覚品質 gate。
- **未検証:** モーション推定、実作品参照、製品経路、GPU parity、実性能。

## 2026-11-17: 整数位置整列 MAE は dither 候補で大きく異なる
- **関連:** `experiments/PencilPrintLab/rd6_temporal_deep.py`、`experiments/PencilPrintLab/captures/rd6/review/RD6_TABLES.md`。
- **確認できた事実:** 0.5px/frame の連番へ binary / multi4 / multi5 の v4 パイプラインを適用した。per-step global MAE は 0.0014〜0.0017 で候補間ほぼ同程度。一方、frame 0 を 0.5*k px 平行移動して frame k へ整列した MAE は binary 0.070〜0.449、multi4 0.043〜0.254、multi5 0.031〜0.107 と候補で差が出た。flicker proxy（step MAE の std）も binary 9.0e-5 / multi4 7.3e-5 / multi5 5.1e-5。フレーム時間は binary 0.031s に対し multi 候補 0.039s 前後。
- **価値または懸念:** 動きに沿って整列しても残る差は、画面固定の dither パターンが動きに対して共変しないことを示唆する。per-step MAE だけでは見えない時間方向の不整合を整数整列で捉えられる。知覚影響は未判定。
- **次に確認すべきこと:** gate 固定後の temporal blind 比較、content-locked dither パターンの案、実動画素材での確認。
- **未検証:** 実作品参照、製品経路、GPU parity、実性能。

## 2026-11-17: dither 段階 3/5/6/8 の sweep でも binary との差は単調でない
- **関連:** `experiments/PencilPrintLab/rd5_dither_sweep.py`、`experiments/PencilPrintLab/captures/rd5/review/RD5_TABLES.md`。
- **確認できた事実:** dither 段階 3/4/5/6/8 と binary を同一条件で比較した。近透明 >1 と非有限値は 6 候補すべてで 0。可視／近透明の最大値は binary と multi3 が 1.0、multi4 0.667、multi5 0.75、multi6 0.8、multi8 0.857。binary からの不透明 max 差は multi4 0.667、multi5 0.75、multi6 0.8、multi8 0.857 だが、flat_highlight では multi5 が 0.55 で最小、multi3 は 1.23 で最大。per-step global MAE は候補間で 0.0014〜0.0017 と同程度。実行時間は multi 候補が binary よりわずかに大きい（alpha_boundary_small で 0.030s に対し 0.036〜0.037s）。
- **価値または懸念:** 「binary に近いこと」は品質ではない。段階数は素材と ROI によって最適値が変わり得るため、単一の既定値を決めるには gate と知覚評価が必要。
- **次に確認すべきこと:** gate 固定後の sweep blind 比較、実作品での網点の粗さ、製品性能。
- **未検証:** 実作品参照、製品経路、GPU parity、実性能。

## 2026-11-17: 多値 Bayer は binary より差が小さいが段階数に最適値がある
- **関連:** `experiments/PencilPrintLab/pencil_reference_v4.py`、`experiments/PencilPrintLab/captures/rd4/review/RD4_TABLES.md`。
- **確認できた事実:** pp_b の binary Bayer を、Bayer 行列を丸め閾値とする多値 ordered dither（4 / 8 段階）へ置き換えて比較した。binary の不透明 v3 差 1.0 に対し、multi4 は 0.667、multi8 は 0.857 だった。近透明 >1 と非有限値は v4 も 0、descriptor 差は 5.96e-08 以下を維持。flat_highlight では multi4 0.733 / multi8 0.943 で、段階数を増やしても差が単調に減らない。
- **価値または懸念:** 差の指標は「binary からの距離」であり美的品質ではない。multi8 が binary に近いのは、段階が増えるほど quantum が細かくなり、値の取りうる範囲が binary に近づくため。gate 未設定のため優劣は判定しない。
- **次に確認すべきこと:** gate 固定後の blind 比較、段階数 3/5/6 のスキャン、知覚上の網点の粗さ評価。
- **未検証:** 実作品参照、製品経路、GPU parity、実性能。

## 2026-11-17: データ由来上限と per-pixel cap で HDR を保ちつつ近透明 >1 を防ぐ
- **関連:** `experiments/PencilPrintLab/pencil_reference_v3.py`、`experiments/PencilPrintLab/captures/rd3/review/RD3_TABLES.md`。
- **確認できた事実:** v3 は量子化上限を `max(1, 直色の最大値)` から求め、段階化結果を各ピクセル `min(q, max(1, 元の直色))` に制限し、alpha は変更しない。v1/v2/v3 の比較で、v3 は近透明 >1 チャンネル 0・非有限値 0・descriptor 差 5.96e-08 以下を維持した。flat_highlight の v1 差は v2 で 1.255〜1.471 だったのが v3 で 0.303〜0.330 に低減。ただし pp_b（Bayer 2 値化）の v1 差は 1.0 のまま。
- **価値または懸念:** 「1 以下を 1 以上に持ち上げない」という per-pixel cap は、HDR を保持しながら alpha 境界の発散を防ぐ一般的なパターンになり得る。2 値化のように段階数が 2 の成分では v1 との差が残り、知覚品質の判断は gate が必要。
- **次に確認すべきこと:** gate 固定後の v1/v2/v3 blind 比較、Bayer 成分の段階数を増やした場合の差、実作品参照。
- **未検証:** gate 未設定、製品経路、GPU parity、実性能。

## 2026-11-17: 段階化を直色側で行うと近透明の 1 超えは消えるが HDR が丸まる
- **関連:** `experiments/PencilPrintLab/pencil_reference_v2.py`、`experiments/PencilPrintLab/captures/rd2/review/RD2_TABLES.md`。
- **確認できた事実:** RD1 の v1 ではテクスチャ変調を premul 段階化の後に掛けており、近透明の復元 RGB が pp_a で 1.159（>1 が 247 画素、large では 1.177 / 2186 画素）に達した。v2 では線ゲートのみ premul+alpha から作り、それ以降を直色側で「テクスチャ変調 → 線の暗化 → 段階化 → 再 premultiply」の順にしたところ、近透明 >1 は全 6 ケースで 0、最大値は pp_a で 0.8 に低下。descriptor 差は 5.96e-08 以下を維持。実行時間は color_ramp_large/pp_a で 0.4451s→0.3051s、tracemalloc peak も 231MB→171MB に低下。
- **価値または懸念:** 見た目は v1 と大きく変わり、不透明部の最大 RGB 差は pp_b で 1.0、flat_highlight で 1.1〜1.47。これは v2 が直色を [0,1] に丸めるためで、HDR スポットの情報が失われる。gate 未設定のため優劣は判定しない。
- **次に確認すべきこと:** gate 固定後に v1/v2 の blind 比較、HDR を保持したまま 1 超えを防ぐ方式（段階化の前に上限を設定する等）の別 run。
- **未検証:** 実作品参照、製品経路、GPU parity、実性能。

## 2026-11-17: 鉛筆画・版画 R&D は RD0/RD1 まで実行、近透明で 1.137 の復元値
- **関連:** `experiments/PencilPrintLab/captures/rd1/metrics_rd1.json`、`experiments/PencilPrintLab/pencil_reference.py`。
- **確認できた事実:** 鉛筆画・版画風の6候補（line / tone / texture とアブレーション）を生成した。descriptor 差は 5.96e-08 以下、非有限値 0、近透明 >10 チャンネル 0。一方、pp_a（Sobel + posterize + grain）の近透明領域で復元 RGB 最大 1.137、可視領域でも 0.839 だった。段階化成分単体でも近透明 1.0。premul と alpha を同じ格子で量子化しても、alpha が小さい側での除算により直色が 1 を超え得る。
- **価値または懸念:** 水彩 R&D の「共有カーネルで発散を防ぐ」だけでは、pointwise な段階化の量子化誤差による 1 超えは防げない。評価では descriptor 差と絶対値を分けて見る必要がある。
- **次に確認すべきこと:** gate 固定後に 1.0 超えの知覚影響を判定する。必要なら量子化を直色側で行い premul へ戻す方式を別 run で比較する。
- **未検証:** gate 未設定、実作品参照、製品経路、性能。

## 2026-11-17: 高速フィルタのパラメータ調整で v3 差を 0.598→0.484 に低減
- **関連:** `experiments/WatercolorLab/rd7_tune_fast.py`、`experiments/WatercolorLab/rd7_v5_comparison.py`、`experiments/WatercolorLab/captures/rd7/review/RD7_TABLES.md`。
- **確認できた事実:** v4 の共有重み近似を 80 通りのパラメータ（k, sigma_space, sigma_color）で v3 出力に合わせた。最良は k=11 / sigma_space=1.0 / sigma_color=0.6（max RGB delta 0.4842、平均 MAE 0.00498）。既定 v4 の同条件での不透明差は 0.598 だった。v5 は descriptor 差 0（12/12）、近透明 >10 チャンネル 0、非有限値 0 を維持。only_bleed の実行時間は v3 2.9049s / v4 0.2076s / v5 0.1915s（1536×1024）で v3 比約 15 倍高速。連番 global MAE は wc_a で v3 0.001040 に対し v5 0.001075 と同程度。
- **価値または懸念:** 参数調整で差は縮むが、bilateral を含む候補の不透明部では依然として 0.484 の最大差が残り、v3 の完全な置換にはならない。max delta を優先して選んだため MAE が最良の組み合わせとは限らない（2 位は MAE 0.004968 でほぼ同値）。tracemalloc peak は v4/v5 とも v3 より約 14% 大きい。
- **次に確認すべきこと:** gate 固定後の v3/v4/v5 blind 比較、max delta と MAE の重み付け変更、エッジ保持の知覚評価。
- **未検証:** gate 未設定、製品 C++／HLSL、実アプリ経路、D3D12/Vulkan parity、実性能。

## 2026-11-17: 共有重み近似は 12 倍速いが v3 と見た目が等価ではない
- **関連:** `experiments/WatercolorLab/watercolor_reference_v4.py`、`experiments/WatercolorLab/rd6_fast_bilateral.py`、`experiments/WatercolorLab/captures/rd6/review/RD6_TABLES.md`。
- **確認できた事実:** v3 の joint bilateral を、局所エネルギー gate と分離可能 Gaussian で「4チャンネルに1つの正規化重みを掛ける」近似（v4）に置き換えた。only_bleed の実行時間は 512×320 で 0.2876s→0.0221s、1536×1024 で 3.1430s→0.2695s（約 12 倍高速）。tracemalloc peak は v4 の方が約 13〜14% 大きい。v4 も descriptor 差 0・近透明 >10 チャンネル 0・非有限値 0 を維持。時間安定性の global MAE も v3 と同程度（wc_a は v3 0.00104 に対し v4 0.00103）。一方、不透明 36 ケースの v3/v4 最大 RGB 差は bilateral を含む候補で 0.185〜0.598（wc_b / wc_c / paper / edge のみの候補は 0）。
- **価値または懸念:** 高速化と alpha 契約は両立できるが、共有重みの作り方を変えると不透明部の見た目が大きく変わる。v4 を v3 の drop-in 高速化として扱うことはできず、知覚品質のゲートで別途判断が必要。tracemalloc は Python heap のみ。
- **次に確認すべきこと:** gate 固定後の v3/v4 blind 比較、実画像でのエッジ保持差、GPU 実装時の重み共有方法。
- **未検証:** gate 未設定、製品 C++／HLSL、実アプリ経路、D3D12/Vulkan parity、実性能。

## 2026-11-17: v3 の joint bilateral は時間的に安定だがオフラインで 19〜25 倍重い
- **関連:** `experiments/WatercolorLab/rd5_temporal_perf.py`、`experiments/WatercolorLab/captures/rd5/review/RD5_TABLES.md`。
- **確認できた事実:** v3 を 8 フレーム・0.5px/frame の連番に適用したところ、全候補の per-step global MAE は 0.001018〜0.001403 でほぼ一定、静止領域の MAE も 0.0008〜0.0016 だった。整数位置へ再整列した MAE は 0.000001〜0.002316。性能計測では、joint bilateral 単体（only_bleed）が v2 比で 512×320 は 0.0173s→0.3299s、1536×1024 は 0.1075s→2.7319s（約 19〜25 倍）で、tracemalloc peak も約 12.5MB→23.7MB、119.5MB→226.8MB（約 2 倍）。 quantize_gauss を含む wc_b は v2 と v3 でほぼ同じ（0.346〜0.361s）で、重いのは手実装 joint bilateral に限定される。
- **価値または懸念:** 近透明の健全性を joint bilateral で得る代償が時間的に大きい。製品 renderer へ持ち込むなら、共有重みを保ったまま GPU 実装または近似が必要になる。tracemalloc は Python heap のみで OpenCV のネイティブ確保は見えていない。
- **次に確認すべきこと:** 共有重みを保つ GPU／SIMD 実装案の比較、実解像度 1080p/4K と呼出頻度、知覚品質 gate。
- **未検証:** gate 未設定、製品 C++／HLSL、実アプリ経路、D3D12/Vulkan parity。

## 2026-11-17: bleed も alpha と同じカーネルで処理すれば近透明の発散は消える
- **関連:** `experiments/WatercolorLab/watercolor_reference_v3.py`、`experiments/WatercolorLab/rd4_v3_comparison.py`、`experiments/WatercolorLab/captures/rd4/review/COMPARISON_RD4.md`。
- **確認できた事実:** v2 の bleed（bilateral / quantize_gauss）が premul RGB のみを動かし alpha を動かさなかったことが近透明発散の原因だった。v3 では bilateral を premul RGB＋alpha の4チャンネル joint bilateral（全世界で共有する重み、4ch float32 非対応のため手実装）に、quantize_gauss を premul と alpha を同一12段階で量子化して同一 Gaussian を掛ける方式に置き換えた。RD4 の alpha fixture 12 ケースで、v3 は descriptor 差 0・近透明 >10 チャンネル 0・非有限値 0。v2 は近透明 >10 が合計 730 チャンネル（最大 258.2）。可視領域の最大値は v1/v2/v3 いずれも 0.60〜0.70 で同程度。
- **価値または懸念:** 不透明 36 ケースの v1-v2 差は 0 だが、v2-v3 差は 17 ケースで非ゼロ（最大 0.0118）。joint bilateral の重みが v2 の cv2 bilateral と異なるためで、近透明の健全性を得る代償に不透明部の見た目がわずかに変わる。gate 未設定のためどちらが良いかは判定しない。
- **次に確認すべきこと:** 実アプリの premultiplied 合成で同じ共有重みが使えるか、joint bilateral のコスト（手実装のため重い）、知覚品質の gate。
- **未検証:** gate 未設定、製品 C++／HLSL、実アプリ経路、性能、実水彩参照。

## 2026-11-17: descriptor 差 0 は絶対的な健全性を意味しない（v2 の caveat）
- **関連:** `experiments/WatercolorLab/captures/rd3/metrics_rd3.json` の `descriptive_findings`、`experiments/WatercolorLab/watercolor_reference_v2.py`。
- **確認できた事実:** RD3 の alpha 境界 fixture で、v2 出力の可視領域（alpha>0.5）は全候補 max 0.60〜0.70・非有限値 0 で健全だった。一方、透明近傍の生 RGB は wc_a で最大 216.7（>10 が 37,542 画素）、wc_b で最大 2.80e5（11,048 画素）、only_bleed で最大 2.78e4（40,702 画素）に発散した。wc_c / only_paper / only_edge は発散なし。共通点は、bleed 成分（bilateral / quantize_gauss）が premul RGB のみを処理し alpha を同じカーネルで処理していないこと。edge 成分（softstep / edge_weighted）は alpha も同時に処理しており発散しない。
- **価値または懸念:** RD2 で「v1→v2 で descriptor 差 12/12 が 0」としたのは、straight 入力と premul 入力が同じ経路を通ることを示しただけで、復元値自体の健全性を証明しない。両者が同じように発散していても差は 0 になる。評価記録でこの二つを混同しないこと。
- **次に確認すべきこと:** bleed 成分も alpha を同じカーネルで処理する v3 を別 run で比較する。その際は復元 RGB の絶対値（可視領域・透明近傍の両方）を必ず併記する。
- **未検証:** gate 未設定のため品質判定ではない。透明画素の RGB は表示結果ではないため、目視では可視領域のみを判定する。

## 2026-11-17: premultiplied 空間処理で alpha 境界の直色依存を 12/12 で解消
- **関連:** `experiments/WatercolorLab/watercolor_reference_v2.py`、`experiments/WatercolorLab/rd2_descriptor_fix.py`、`experiments/WatercolorLab/captures/rd2/review/DESCRIPTOR_COMPARISON.md`。
- **確認できた事実:** 全成分を premultiplied 空間で処理し、alpha と premul RGB に同じ空間変換を適用して最後に一度だけ unpremultiply する v2 を実装した。同一 fixture・候補・seed で straight／premultiplied 入力から復元した straight RGB の最大差は v1 が最大 0.389（12ケース）だったのに対し、v2 は 12/12 で 0（完全一致）。opaque 6 fixture × 6 候補 = 36 ケースでは v1 と v2 の RGB 差が 0 で、不透明部の見た目は変わらない。
- **価値または懸念:** エフェクト横断の alpha 契約として、「pointwise 色変換は unpremultiply してよいが、近傍演算は premultiplied のまま空間変換し alpha も同じカーネルで処理して最後に戻す」という指針が具体的な数値で示された。
- **次に確認すべきこと:** 実際の GPU 合成経路で premultiplied  bilinear / gather がこの契約を守るか、D3D12／Vulkan parity、知覚品質の gate。
- **未検証:** gate 未設定のため品質合格ではない。製品 C++／HLSL 実装、実アプリ経路、性能は未検証。

## 2026-11-17: 近傍演算は unpremultiply 後に適用すると alpha 境界で straight 色が壊れる
- **関連:** `experiments/WatercolorLab/watercolor_reference.py`、`experiments/WatercolorLab/rd1_candidates.py`、`experiments/WatercolorLab/captures/rd1/metrics_rd1.json` の `descriptor_check`、`docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`。
- **確認できた事実:** RD1 で同一の alpha 境界 fixture に straight／premultiplied の両 descriptor から6候補を適用し、復元 straight RGB の最大差を測った。bleed(bilateral) と paper 単体は 1.192e-7（float32 丸め）だったが、edge(softstep) 単体 0.134、これを含む wc_a 0.138、wc_c 0.199、wobble を含む wc_b 0.383。premultiplied では透明近傍の RGB が 0 になり、unpremultiply すると発散／0 の straight が近傍演算（box blur / remap）へ混ざる。bilateral は色差で重みを付けるため透明近傍を除外できた。
- **価値または懸念:** 「unpremultiply → process → re-premultiply」は pointwise な色変換（LUT / Tritone 等）では成立しても、近傍を参照する blur / remap では一般性を持たない。alpha 契約をエフェクト横断で考える際の再利用価値が高い。
- **次に確認すべきこと:** premultiplied のまま近傍演算して最後に unpremultiply する案と、alpha／色を考慮した重み付け案を RD2 の別 run で比較する。候補 source を変える場合は blind key とシートを再生成する。
- **未検証:** 製品の実 descriptor、GPU 経路、知覚上の許容差。gate 未設定のため本件は「未判定」であり合格／不合格の判断ではない。

## 2026-11-17: ImageLayerのメモリ内生成画素は通常のproject JSONでは再読込できない
- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cpp::setFromImageBuffer`、`toJson`、`fromJsonProperties`、単一画像レイヤー化。
- **確認できた事実:** `setFromImageBuffer()` は既存のsource assetを解放し、`sourcePath_`を空にして画素をmemory cacheへ置く。`toJson()` はsource path・寸法・metadata等を保存するが、image buffer自体はJSONへ出力しない。`fromJsonProperties()` はcacheをリセットし、保存されたsource pathから再読込する。
- **価値または懸念:** レイヤー化結果をmemory-onlyの`ArtifactImageLayer`として追加すると、保存／再読込後に生成画素が戻らない。Phase 1のUndo・source identity・再読込要件を満たすには、project/asset保存境界を通す方法を先に決める必要がある。外部temp file依存で隠さない。
- **次に確認すべきこと:** 既存のasset import／project bundleが生成画像を永続化する正規経路か確認し、候補maskと生成画素を1回のUndo単位で登録できるAPI境界を決める。
- **未検証:** project bundleにおけるbuffer-backed imageの保存経路の有無、AssetManagerへのin-memory image登録・書き出しAPI、生成レイヤーの再読込受入。

## 2026-11-17: Histogram方式はpeak workspaceより累積allocation trafficが大きい
- **関連:** `ArtifactCore/src/Color/AutoColorMatch.cpp`、`experiments/ColorMatchLab/captures/rd28_core_allocations/`、RD23 raw-buffer benchmark。
- **確認できた事実:** MSVC Debug CRT hookでwarm-up後のraw `match()` 1回を2run取得した。1280×851 Histogramは両方24 events / 27,777,780 requested bytes。1536×1024はdirectが25 / 40,109,187、CTest repeatが24 / 40,108,788。ソース由来vector payloadとの差はHistogramで756〜1,155 bytes。Reinhard/MeanStddevは両runとも36 eventsで要求payloadとの差570 bytes。Histogramではチャンネルごとにsource/referenceフルフレームvectorを作り直すため、累積要求byteはlive peak workspaceより約3倍になる。
- **価値または懸念:** allocation trafficはメモリ帯域負荷やallocator pressureに関係し得るが、ここで測った要求byteはpeak resident memoryや性能影響そのものではない。Debug CRT hookが見たevent countはThreadPool等を含み、Histogramの1536 event数はrun間で1件揺れた。
- **次に確認すべきこと:** 実アプリcallerとRelease allocator/実解像度が特定された後、繰返し時間を含むallocation traceとpeak live/process memoryを観測し、必要なら品質を保つ範囲のbuffer reuse案を別途検討する。
- **未検証:** request数の実装内訳、Release/allocator差、allocation trafficが時間へ与える因果影響、実アプリでのpeak memory・呼び出し頻度。
## 2026-11-17: Middlebury自然シーンでCoreと色合わせ式ミラーがfloat精度で一致
- **関連:** `tests/ArtifactCore/AutoColorMatcherContractTest.cpp`、`experiments/ColorMatchLab/captures/rd27_core_natural_scene/`、RD18 lamp-shade masks。
- **確認できた事実:** 登録済みEOS 5D warm-lit kitchen RAW/JPEG pairに対してCore image overloadと現在の式ミラーを3方式×3強度で比較し、各条件で2,001,000 RGB float値を取得した。Histogramのabsolute diffは全て0、Reinhardの最大は2.23e-6、MeanStddevは1.20e-7。既存source-only reviewed shade ROIのstrength-1 Core mean reference ΔE76は、left/rightでHistogram 1.62/1.31、Reinhard 4.81/5.18、MeanStddev 5.50/5.50。Middlebury提供元は利用と公開をcitation付きで許可している。
- **価値または懸念:** 合成テストだけでなく自然画像対で、式ミラーが現在のCore実装を高いfloat精度で再現することを示す。camera JPEGは測色WB正解ではなく、shadeの中立度も測定していないためreference距離の小ささは正しさを意味しない。
- **次に確認すべきこと:** 実画像の肌と測色済み中立・白を含む権利確認済みsource/referenceを受領したら、測色・色空間・mask・事前gateを確定し、同じCore出力/ミラー差分手順で測定する。
- **未検証:** 別シーン・カメラへの一般化、肌・真のneutral white、独立blind review、アプリ経路、品質許容値。visual reviewは一人の非盲検確認に限られる。
## 2026-11-17: AutoColorMatcherは解像度に比例する大きなCPU一時配列を作る
- **関連:** `ArtifactCore/src/Color/AutoColorMatch.cpp`、`experiments/ColorMatchLab/EVALUATION_PLAN.md`、画像間色合わせR&D。
- **確認できた事実:** 実装上、Reinhard/MeanStddevはsource/reference双方の3チャンネルfloat vectorを同時に保持し、Histogramはチャンネルごとにsource/referenceのfloat vectorを保持する。RD23 timingで使った等寸法の条件に当てはめると、明示vector payload下限はReinhard/MeanStddevが1280×851で24.93 MiB、1536×1024で36 MiB、Histogramが8.31 MiB/12 MiB。明示vectorのresi
## 2026-11-17: LUT effects must grade straight RGB when surfaces are premultiplied
- **関連:** `Artifact/src/Effects/ColorCorrection/LUTEffect.cpp`, `experiments/CinematicLooksLab/contract_probe.py`, `experiments/CinematicLooksLab/captures/rd1_lut_alpha_wrapper/float_contract_probe.json`.
- **確認できた事実:** Composition can pass a `Premultiplied` surface descriptor to the CPU LUT effect. The old wrapper sent premultiplied RGB directly into nonlinear LUT sampling and intensity mixing. The updated wrapper branches on the descriptor, unpremultiplies, grades/mixes straight RGB, and re-premultiplies; non-positive alpha keeps the copied pixel. The source mirror records <2.3e-16 maximum RGB recovery delta for all 9 LUTs across 8 colors, 6 alpha patches, and 3 strengths, versus 1.321154 for the prior equation.
- **価値または懸念:** Nonlinear grading of premultiplied channels makes the recovered straight color depend on alpha and can create visible edge color shifts after compositing. This change is limited to the CPU implementation because LUTEffect currently selects CPU compute mode; runtime parity and image quality remain unverified.
- **次に確認すべきこと:** Build/runtime authori
## 2026-11-17: Looks Browserは未接続状態をApply済みと表示しない
- **関連:** `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cpp`、`Artifact/include/Widgets/ArtifactLooksPresetBrowser.ixx`、`docs/planned/MILESTONE_CINEMATIC_LOOKS_RND_2026-11-16.md`。
- **確認できた事実:** ブラウザーApplyはeffectを生成・変更せず、以前は選択IDを`appliedId_`に保存してbadgeだけを適用済み表示にしていた。ダイアログの生成callsiteも見つからず、公開APIは選択画像設定と適用ID getterだけ。製品変更後はApplyボタンを無効化し、未接続を説明し、applied IDを空のままにする。
- **価値または懸念:** 処理されないlookが適用済みと誤認される状態を避けられる。一方、これでブラウザーが製品機能として接続されたわけではなく、表示inventory、生成thumbnail、実画像previewとeffect適用は引き続き未実装。
- **次に確認すべきこと:** 正式な24 ID→effect/preset/property mappingと適用先ownerを製品仕様として決定し、実行時Composition/Layer反映および画像評価計画を満たした上で再接続する。
- **未検証:** コンパイル、画面上のdisable/status表現、既存呼び出し元からのdialog利用、effect mappingと画像品質。
## 2026-11-17: Cinematic Looks probeのrun IDは出力先と同期させる
- **関連:** `experiments/CinematicLooksLab/contract_probe.py`、`experiments/CinematicLooksLab/captures/rd3_lut_alpha_wrapper/float_contract_probe.json`。
- **確認できた事実:** contract probeは任意の`--output` pathを受けて別runを出力できたが、JSON内の`rd`欄は`rd1_lut_alpha_wrapper`に固定されていた。rd3 pathへ出した直後のJSONが誤ったrun labelを含んだため、出力先parent directory名、または明示`--rd`をmanifestへ渡すよう修正し、rd3を再生成した。
- **価値または懸念:** source hash・計測値が一致してもrun IDが誤ると異なるrevision/runの証跡を取り違える。再生成時はoutput pathとmanifest identity双方を確認する。
- **次に確認すべきこと:** Cinematic Looks内の他recorderが出力先からmanifest IDを導出するか点検し、新規runを作る際には関連pathと内部IDの一致を照合する。
- **未検証:** すべてのrecorderでのrun-ID整合性監査。
## 2026-11-17: Looks Browserはeffect設定後にtargetへ追加する
- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cpp`、`Artifact/src/Widgets/ArtifactLooksPresetBrowser.cpp`、`Artifact/src/Service/ArtifactEffectService.cpp`、`Artifact/src/Service/ArtifactProjectService.cpp`。
- **確認できた事実:** Inspectorの既存Effect picker経路にLooks Browser entryを追加でき、既存のpicker選択callbackからdialogを起動できる。Effect ServiceのfactoryでTritoneを生成し、追加前にPresetとEffect Mixを設定した後、Project ServiceのCompositionまたはLayer undoable-add APIへ渡す構造を追加した。これによりpreset/intensity変更ごとの別property Undo commandを作らず、effect追加は1件のundoable mutationになる。
- **製品仮説:** browser ID `teal_orange`をTritone `TealAndOrange`へ対応させた。これはラベルの直接一致から作った暫定1件であり、既存sourceに正式なID mappingがあったことや、美的品質の受入を意味しない。他23 IDは未対応。
- **価値または懸念:** 既存の選択中Composition/Layerとeffect factory、既存picker callbackを再利用し、状態を完全に構成してから1回で追加できる。dialogの実表示、layer/Composition側の生成先、undo/redo、描画結果はまだ動作確認されていない。
- **次に確認すべきこと:** 明示許可後にビルド・UI操作し、picker到達、Composition/Layer target、single-step undo/redo、save/reload、preview/final outputを検証する。ユーザーが正式mappingを示した場合は仮説tableを置き換える。
- **未検証:** module integration、実UI到達、Composition/Layer追加、undo/redo、persist/reload、Tritoneのruntime画像品質とCPU/GPU parity。
## 2026-11-17: 近いFilm/Core preset名だけではLooks Browserへ接続できない
- **関連:** `Artifact/src/Effect/ArtifactFilmEffects.cpp`、`Artifact/include/Effect/ArtifactFilmEffects.ixx`、`ArtifactCore/include/ImageProcessing/ColorTransform/TriChromaticShift.ixx`、`experiments/CinematicLooksLab/REGISTRY_AUDIT.json`。
- **確認できた事実:** `FilmPresets::crossProcessed()` は複数film処理設定を持つ `FilmEffectPreset` (`QObject`) を返し、`FilmEffectProcessor` がフレームbufferへ適用する。`TriChromaticTriad::cyberpunk()` はCoreの色変換設定値で、`TriChromaticProcessor` が適用する。両方とも現時点のソースでは `ArtifactAbstractEffect` / Inspector effect factoryとして登録された型ではない。
- **価値または懸念:** `cross_process` や `cyberpunk` と近い表示名があっても、effect stackへの永続化・undo・Composition render経路が同じとは限らない。名称一致での暫定mappingも、対応型が別アーキテクチャの場合は機能実装を伴わない。
- **次に確認すべきこと:** 正式なbrowser ID mappingを決める際、同じInspector/Effect Service/Project Service経路を持つか確認する。別processorを採用するなら、まず製品effect adapter・property・保存復元・render経路を設計してからmappingする。
- **未検証:** FilmEffectProcessor / TriChromaticProcessorの現行製品UI到達、プロジェクト保存、GPU/CPU画像品質、Looks Browserからの適用可能性。
## 2026-11-17: Tritoneの現在のGPU経路はフレームごとにreadbackで同期する
- **関連:** `Artifact/src/Effects/ColorCorrection/TritoneEffect.cpp::TritoneEffectGPUImpl::applyGPU`、`docs/technical/HOT_PATH_RULES.md`、Cinematic Looks R&D性能評価。
- **確認できた事実:** 現在のGPU経路は各apply呼び出しで入力textureを作成し、出力textureからstaging textureを作成・copyし、`Flush()`と`WaitForIdle()`後にCPU readbackする。今回のTritone式/alpha差分との比較では、staging/readback構造は変更前のHEADにも存在し、今回の差分で追加されたものではない。
- **価値または懸念:** Composition previewの反復適用でこの経路へ到達する場合、CPU/GPU同期とresource作成が性能・応答性の制約になり得る。これはTritoneの式mirrorが一致しても解消されず、R&Dの1080p/4K性能評価や製品GPU経路の判断を別途必要にする。
- **次に確認すべきこと:** 明示されたrenderer/GPU改善の作業範囲で、実call frequency、resource reuse、readback要否を経路ごとに測り、backend-neutralな非同期/再利用設計を評価する。今回のsource equation修正に混ぜて大規模なGPU経路変更をしない。
- **未検証:** GPU runtimeでの呼出回数、WaitForIdleのstall時間、各backendでのstaging allocation費用、再利用・readback削除の効果。
## 2026-11-17: AutoColorMatcher Core ROI計測は合成cast recoveryと肌彩度低下を同時に示す
- **関連:** `tests/ArtifactCore/AutoColorMatcherContractTest.cpp`、`tests/ArtifactCore/CMakeLists.txt`、`tests/ArtifactCore/ColorSpaceStandalone/CMakeLists.txt`。
- **確認できた事実:** full-core target `ArtifactCoreAutoColorMatcherContractTest`は元々5件の`float*` BGRA契約ケースを持つ。6件目では`ImageF32x4_RGBA` overloadとraw-buffer出力を全3方式×強度0.25/0.5/1.0、sRGB/linear straight descriptorで比較し、強度1.0固定6画素出力を独立Python式mirrorの期待値とも照合する。独立ColorSpaceStandaloneのmodule/implementation target listは`Color.AutoMatch`/`AutoColorMatch.cpp`を含めない。
- **価値または懸念:** Debug実行は小さな固定fixtureで画像overload/raw-buffer経路と式mirrorの数値一致を確認する。descriptorを変えても値が同じなのは現状のdescriptor非参照を示し、色管理が正しい証拠ではない。自然画像の肌/白品質や実経路の証明にもならない。
- **次に確認すべきこと:** 権利確認済みの撮影WB portrait pair、測色済み白、独立ROIレビューを用意して品質評価を続ける。単に契約テストを通しただけでR&D品質gateを閉じない。
- **追加観測:** 同じ合成portrait/castでstrength 0.0–1.0を0.1刻みで3方式×5 ROI測定（165行）した。combined skinのsource C*ab 14.23 / synthetic target 11.77に対し、strength 1 outputはMeanStddev 11.75、Histogram 11.78、Reinhard 12.71。強度が上がるとsource相対ΔEが増しsynthetic target相対ΔEが下がる。これは同一画像から作った合成targetに対するCore挙動だけで、許容肌変化や自然WB品質の証拠ではない。
- **追加観測:** 既存ImageF32x4_RGBA::save()境界でsource/referenceと3方式のCore出力PNGを保存し、全体/顔/腕の固定crop sheetを作成。単一unblinded reviewerの表示倍率確認では違いは微妙だが見え、明白な構造損失は見えなかった。これは受容判定・白保持証拠ではない。
- **実行結果:** 2026-11-17にユーザー許可のもとVisual Studio 18 Debugで既存targetを再build、CTest 1/1・GoogleTest 9/9 pass。RD9拡張テストはCore出力の165 ROI strength sweep、pixelwise ΔE76 p95/max/閾値超過率、source/reference/output PNGを記録。test source SHA-256 `b2af3f825f8422b298c03b360270140d0e7a4b7d8ab38e95086609f166977866`。詳細: `experiments/ColorMatchLab/captures/rd20_core_skin_white/manifest.json`。
- **未検証:** 自然画像ROIのCore出力と画質、人によるblind review、profile-aware behaviorの正しさ、実経路/performance、ユーザー提供画像pairの評価。
## 2026-11-17: Histogram mirror CDF precision must follow Core float arithmetic
- **関連:** `ArtifactCore/src/Color/AutoColorMatch.cpp`, `experiments/ColorMatchLab/run_rd1_source_mirror.py`, RD6/RD8/RD19 captures, `tests/ArtifactCore/AutoColorMatcherContractTest.cpp`.
- **確認できた事実:** Full-resolution Core-vs-RD6 preview comparison initially failed only for Histogram. Core uses float channel binning, float CDF entries and sequential float accumulation; the prior Python mirror used a float64 cumulative sum. At intensities 0.25 / 0.5 / 1.0, the earlier maximum RGB differences were 0.012745 / 0.025490 / 0.047059. After changing the Python equation mirror to float32 arithmetic, RD6 and RD8 regenerated/validated, and all 7 Core contract tests pass, including the full 606×404 pair comparison for 3 methods × 3 intensities within 1.5/255 preview-decode tolerance. Follow-up route checks on 1000×667 RD7 and 1280×851 RD9 source/cast inputs also pass image-overload versus raw-buffer equivalence for all nine conditions; these checks do not validate the Python mirror.
- **価値または懸念:** Numeric mirrors used to produce high-resolution evidence must preserve implementation precision and tie behavior; otherwise a mirror discrepancy can be mistaken for a product defect. The old untracked output arrays were not recoverable, so only previously measured maxima remain; no aggregate mean/p99 is claimed.
- **次に確認すべきこと:** When building future Core differentials, preserve exact float/rounding behavior in both vector and image-scale fixtures, and keep preview quanti
## 2026-11-17: Core image fixtures must distinguish logical RGBA input from BGRA backing
- **関連:** `ArtifactCore/include/Image/ImageF32x4_RGBA.ixx` (`setFromRGBA32F` / `rgba32fData` contract)、`tests/ArtifactCore/AutoColorMatcherContractTest.cpp`、`experiments/ColorMatchLab/captures/rd23_shadowed_portrait/`。
- **確認できた事実:** `ImageF32x4_RGBA`の`rgba32fData()`はBGRA backingを返し、`setFromRGBA32F()`はlogical RGBA inputを受けてBGRA storageへ正規化する。RD23 fixture実装中に同じ配列順と誤認しかけたが、既存契約と照合してから数値を採り直した。最終テストではcast構築時にBGRA backingからlogical RGBへ明示対応し、Lab測定でもRGB順へ戻す。
- **価値または懸念:** 画像版とraw buffer版のテストは、名前が似たアクセス関数でもチャンネル契約を別々に明記しないと、実装ではなくfixtureの順序誤りを測る恐れがある。中間測定は破棄し、修正後のCore runだけをmanifestへ記録した。
- **次に確認すべきこと:** 新しいfloat画像fixtureでは、宣言コメントとセット/取得APIの両方を読み、非対称RGBの小さいチャンネル識別fixtureで順序を確定してから色差集計する。
- **未検証:** 他の画像クラスの名称・channel orderが同じ契約を持つとは限らない。各型の定義ごとに確認する。
## 2026-11-17: AutoColorMatcher方式ごとにCore処理時間が大きく異なる可能性
- **関連:** `ArtifactCore/src/Color/AutoColorMatch.cpp`、`tests/ArtifactCore/AutoColorMatcherContractTest.cpp`、`experiments/ColorMatchLab/captures/rd23_shadowed_portrait/core_outputs/performance.csv`。
- **確認できた事実:** 24 hardware threads表示のVisual Studio Debugで4回のraw-buffer `match()`測定を行った。直近runの7-sample中央値は1280×851のReinhard/MeanStddev/Histogramが44.02/14.72/220.33 ms、1536×1024が57.74/21.98/313.47 ms。前3 run中央値は43.93/14.28/217.06・58.08/21.53/326.27、44.66/14.24/219.45・55.51/21.46/319.13、44.62/14.33/218.08・58.07/22.35/327.08 ms。4 runではtail latencyや再現性を確立できない。実装にはHistogram用のpartial histogram/vectorとCDF scanがある。
- **価値または懸念:** この1回の測定ではHistogramが他方式より遅いが、Debug build・マシン・並列backend・入力分布依存があり、実app pathの遅延や処理時間予算への適否を結論できない。処理時間差の原因をvector allocationだけに帰属できるプロファイル証拠もない。
- **次に確認すべきこと:** 実callerとinteractive/final予算が決まった後、同じ入力/CPU affinity/Release buildで反復測定し、Profilerでinclusive/exclusive timeとallocation bytesを方式別に分解する。Histogram構成変更を検討するなら、出力 parityを保持して独立benchmarkで測る。
- **未検証:** Release/GPU、アプリのpreview/export経路、CPU負荷変動時のp95/p99、peak memory/allocations、計測差の統計的有意性。
## 2026-11-17: ColorChecker WB pairは肌/白の保持ではなく参照レンディションへの距離を示す
- **関連:** `experiments/ColorMatchLab/captures/rd24_colorchecker_core/`、`ArtifactCore::AutoColorMatcher`。
- **確認できた事実:** Middlebury ColorCheckerのCanon EOS 20D WB1固定tungstenとWB2 camera auto-WBのペアをCoreで処理し、skin-tone proxy、white、neutral grayの中央patch ROIを各3方式×3強度で測定した。strength 0.25から1.0へWB2距離は低下し、source距離は上昇する傾向があった。強度1ではHistogramが全3群のWB2平均Lab ΔE76を相対的に小さくしたが、目標WB2はカメラJPEGであり測色ground truthではない。全候補と2軸の非支配frontierを`captures/rd24_colorchecker_core/tradeoff_summary.json`へ記録した。追加の平均Lab chroma集計ではsource・WB2 reference・outputを同じCore float測定ループから算出し、保存PNGの8-bit量子化差を排除した。skin proxy C*abはsource 15.23から25.83/29.11/29.81へ増え、white patchはsource 7.85から6.93/12.24/9.77へ、neutral grayは5.70から8.08/7.97/7.96へ変化した（Reinhard/MeanStddev/Histogram、strength 1）。
- **価値または懸念:** 実Coreでチャート群の色合わせ量とsource色変化量、平均chroma変化を同時に比較できる一方、WB2距離の低下やchartのC*abを肌/白保持やWB正解と解釈できない。色票は標準化patchで、人肌・自然白のテクスチャや材質を含まず、Lab値もunknown descriptorの画像に対するsRGB仮定である。
- **次に確認すべきこと:** 製品品質評価には権利とWB pair semanticsが明確な実撮影source/reference、測色中立・自然白ROI、独立注釈と盲検baselineレビューを揃え、候補を開く前に受入範囲を固定する。
- **未検証:** どの方式が自然肌/白に対して好ましいか、実撮影WB pair全般への一般化、独立画質レビュー、製品callerでの結果。
## 2026-11-17: 公開AWB datasetのコードlicenseと画像データ利用権は別に確認する
- **関連:** `experiments/ColorMatchLab/EVALUATION_PLAN.md`、Samsung S24 Raw-sRGB、DAST Skin Tone Database。
- **確認できた事実:** S24 READMEは画像rendition、neutral/user-preference WB annotation、二次照明mask、個人情報blurを説明するがdataset-specific license欄がない。関連GitHub forkのCC BY-NC-SA 4.0 licenseはコードrepo側であり、画像データに適用されるとは確認できない。DAST公式READMEは80人・960枚、CST 1–9、5部位のCortex DSM-4測色、WB-card calibrated sRGBを記載し、全データはacademic/commercial use向けに申請可能とする。各subjectの12枚はnominal/under/over exposure各4枚で、WB-paired renderとは説明されていない。
- **価値または懸念:** 有望なWB metadataや論文公開だけでは製品R&Dでの画像複製・変換・評価・保存の権利を確定できない。ソフトウェアlicenseとdataset/image licenseを混同すると、適切な評価素材と利用許諾の両方を誤認する。
- **次に確認すべきこと:** ユーザーから権利確認済みの同一scene pairを受領するか、利用条件が明示されたprovider素材を選ぶ。問い合わせが必要なら許諾範囲（社内製品R&D、画像加工、派生crop/metrics保存、再配布）を明記する。
- **未検証:** S24 datasetの独立した画像ライセンス、DASTの契約に基づく派生物保存条件・pair可用性、各候補がskinと測色neutral/whiteを同じimage pair内で満たすか。
## 2026-11-17: Core image loadの色descriptorを実測ログへ残す
- **関連:** `ArtifactCore/src/Image/ImageF32x4_RGBA.cpp`、RD24、`tests/ArtifactCore/AutoColorMatcherContractTest.cpp`。
- **確認できた事実:** RD24の実Core実行では、ロードしたMiddlebury source/referenceの`SurfaceColorDescriptor`がどちらも`transfer_known=0`、`primaries=0`だった。PNGファイルにも埋め込みICC profileはなかった。`ImageF32x4_RGBA::load`はOpenImageIO経路でunknown descriptorを設定する分岐と、QImage fallbackでsRGB/premultipliedを設定する分岐を持つ。ログで実測した今回のdescriptorは前者と整合する。
- **価値または懸念:** encoded sRGBを仮定したLab結果はCore descriptorに裏付けられていない。別入力ではloader経路やprofile状態が変わり得るため、ファイル拡張子だけで測色変換を決めるとRGB値の意味を取り違える可能性がある。
- **次に確認すべきこと:** pair受領ごとに元profileとload後descriptorを記録し、未確定な場合は色変換根拠を確定してから色差を計算する。将来loaderのdescriptor挙動を変える場合は、入力profileを含む専用契約評価を行う。
- **未検証:** 他format/decoderでの実際のloader分岐、ICC profileが存在する画像の変換挙動、別primaries/transferを持つmatcher入力の妥当性。
## 2026-11-17: AutoColorMatcherの可視RGB出力は完全透明画素のhidden RGBにも依存する
- **関連:** `ArtifactCore/src/Color/AutoColorMatch.cpp`、`tests/ArtifactCore/AutoColorMatcherContractTest.cpp`、`experiments/ColorMatchLab/captures/rd25_core_alpha/`。
- **確認できた事実:** 同一の可視RGB・alphaを持つCC0 portrait fixtureで、alpha=0 strip内のhidden RGBだけをblack/magentaに変え、同一cast referenceに対するCore image overloadを3方式×strength 0.5/1.0で実行した。可視領域のpaired-output mean Lab ΔE76はReinhard 7.42/14.82、MeanStddev 13.61/26.82、Histogram 12.65/24.83（strength 0.5/1.0）。各出力はfiniteでalphaは完全保存された。
- **価値または懸念:** このfixtureではalpha=0画素のRGBが全画面source統計へ入り、表示上同一の入力でも可視出力が変わる。RGB MAE/p95と最大差もmanifestへ保存した。これは出力適合性の判断ではなく、透明色を含む画像でsource統計の対象画素をどう扱うかという未解決契約の証拠。
- **次に確認すべきこと:** ユーザー提供の権利確認済みWB pairの受領後、別の自然画像と実ROIで主要評価を継続する。alpha評価を製品要件とする場合、straight/premultiplied、semitransparent edge、複数background上の可視結果を事前ゲート付きで確認する。
- **未検証:** typical projectにおける発生頻度、semitransparent edge/premultiplied behavior、独立visual acceptance、許容差・製品閾値。
## 2026-11-17: AutoColorMatcherはpremultiplied alphaのRGB範囲を保持しない
- **関連:** `ArtifactCore/src/Color/AutoColorMatch.cpp`、`SurfaceColorDescriptor::canonicalLinearPremultipliedBgra`、`tests/ArtifactCore/AutoColorMatcherContractTest.cpp`、`experiments/ColorMatchLab/captures/rd26_core_premultiplied/`。
- **確認できた事実:** 64×64のsynthetic linear premultiplied BGRA32F gradient/alpha rampをdescriptor付きimage overloadへ通した。全6条件で出力はfinite、alphaは不変だが、RGB<=alphaの範囲を超えたchannelはReinhard 155/229、MeanStddev 128/136、Histogram 64/64（各method strength 0.5/1.0）。alpha=0でnon
## 2026-11-17: AutoColorMatcherのimage overloadはtransfer descriptorを解釈しない
- **関連:** `ArtifactCore/src/Color/AutoColorMatch.cpp:417-420`、`tests/ArtifactCore/AutoColorMatcherContractTest.cpp`の`ImageOverloadMatchesRawBufferAndEquationMirrorForTaggedDescriptors`。
- **確認できた事実:** 同じ6画素BGRA float bufferをsRGB tagとlinear tagで画像化してimage overloadへ入力し、全3方式×strength 0.25/0.5/1.0で各floatが`EXPECT_FLOAT_EQ`一致する。image overloadはdescriptorを見ず、buffer pointerと寸法をraw APIへ渡す。
- **価値または懸念:** 記述子が違っても同じnumeric valuesなら出力が同じことを確認した。これは異なるtransferで表現された同じ色を正規化する証拠ではなく、Coreが色符号化を解釈しないことを示す。RD15のsRGB/linear比較は式ミラーのみ。
- **次に確認すべきこと:** caller契約が明らかになった後、入力を同一scene/色値から作ったprofile-known sRGB/linear representationへ揃え、Core image overloadの意味的同等性と各ROI結果を比較する。
- **未検証:** 同一色を表す異符号化Core画像間の数値/parity差、ICC変換、primary adaptation、製品callerの実入力transfer。
## 2026-11-17: AutoColorMatcherの公開APIは寸法とmethod選択に制約がある
- **関連:** `ArtifactCore/include/Color/AutoColorMatch.ixx`、`ArtifactCore/src/Color/AutoColorMatch.cpp`、`tests/ArtifactCore/AutoColorMatcherContractTest.cpp`、Color Match Lab評価計画。
- **確認できた事実:** 公開raw `match` dispatcherは`width,height`一組だけを受け取り、両方の入力寸法として3 algorithmを呼ぶ。`match(ImageF32x4_RGBA&, const ImageF32x4_RGBA&)`もsource寸法だけをdispatcherへ渡す。個別の`reinhardTransfer` / `meanStddevMatch` / `histogramMatch` raw overloadにはsource/reference別寸法があるが、既存の異寸法contract testは`meanStddevMatch`のみを確認している。さらに`computeMatch(..., Method)`はmethod引数を未使用にしており、既定値がReinhardでも常にRGB各channelのmean/stddevからscale/offsetを計算する。confidenceは平均`abs(scale-1)`のみから算出しoffsetを参照しない。
- **価値または懸念:** source/reference寸法が違うペアでconvenience overloadを使う場合、reference側画素数が小さければ過読込、大きければ統計から一部が欠落する可能性がある（sourceからの推論で、実行再現はしていない）。`computeMatch`をmethod選択のpreviewとして使う場合も、返る変換は選択methodと一致しない。等しいsource/reference標準偏差と非
## 2026-11-17: AutoColorMatcherの現行3方式は空間フィルターを使わない
- **関連:** `ArtifactCore/src/Color/AutoColorMatch.cpp`の`reinhardTransfer`、`meanStddevMatch`、`histogramMatch`と画像間色合わせR&D。
- **確認できた事実:** 3方式はいずれも画像全体または各channelの統計/CDFを先に構築し、その後、各source pixelの同じ座標だけを読み書きする。近傍pixel参照、畳み込み、spatial resamplingは実装されていない。RD29ではRD27の保存Core PNGをsRGB→linear Rec.709 Yへ変換し、全画面・左右lamp masks・18px contextで勾配を集計した。全画面のsource/output p95勾配比はReinhard/MeanStddev/Histogramで2.478/2.440/2.528、強エッジ平均方向cosineは0.9912/0.9924/0.9995だった。lamp mask内の方向cosineはleft/right順にReinhard 0.481/0.522、MeanStddev 0.504/0.621、Histogram 0.9965/0.9981。保存PNG上のRGB端点値（どれかのchannelが0または255）の割合はsourceの2.263%から、全画面で10.653/10.023/0.027%、lamp maskでReinhard 26.76/27.87%、MeanStddev 22.67/23.23%、Histogram 0/0%になった。RD29初回のlamp edge thresholdはp90が0となる定義だったため、positive source gradientsのp90へ修正して15行を再計算した。RD30ではRD9のCC0 portrait equation-mirrorを使い、strength 1のface mask gradient p95比は1.020/1.011/1.013、3×3 RMS局所コントラストp95比は1.017/1.010/1.012。arm maskではそれぞれ1.020/1.010/1.014と1.020/1.013/1.017（Reinhard/MeanStddev/Histogram）だった。
- **価値または懸念:** 現行経路がblur/haloをspatial kernelで発生させる構造ではない。一方、RD29では色変換と保存が混ざる勾配コントラスト変化を観測した。初回の`clipped_pixel_fraction`という名前は保存PNGから断定できないため`endpoint_pixel_fraction`へ直し、自然なendpointとclippingを区別できないとmanifestに記録した。endpoint occupancyはclippingそのものを証明せず、low-texture unmeasured lampshadeの小ROIでの方向差もdetail lossや方式優位の品質証拠ではない。pointwise実装だけから良好な見た目を結論できない。
- **次に確認すべきこと:** 権利確認済みpairでは色保持と合わせて、source-onlyで固定した細線/低コントラスト/高輝度detail ROIにおけるfloat値域clip、gradient/edge contrast、階調段差を複数強度で測り、blind reviewで許容条件を決める。RD29のmanifestは`experiments/ColorMatchLab/captures/rd29_core_detail/manifest.json`、RD30は`experiments/ColorMatchLab/captures/rd30_portrait_skin_detail/manifest.json`。
- **未検証:** 自然肌/測色白を含む各ROIでのedge/detail保持、Core float pathのclipping率、8-bit保存後のbanding、用途に応じた許容差。
## 2026-11-17: AutoColorMatcherのportrait skin candidate上の色応答を方式別に記述できる
- **関連:** `experiments/ColorMatchLab/record_rd31_portrait_skin_color_response.py`、RD9 CC0 portrait、RD31 manifest/CSV。
- **確認できた事実:** 1枚のunpaired warm/mixed-light CC0 portraitとRD9の既存face/arm mask candidateで、3方式×3強度の式ミラー出力をunchanged sourceに対するper-pixel ΔE76、signed Lab a*/b*/C*ab変化、source/outputともC*ab≥5のpixelに限る絶対円環hue変化として記録した。strength 1 face-maskのmedian ΔE76/hue変化はReinhard 1.68/0.90°、MeanStddev 2.50/4.92°、Histogram 2.53/5.32°。arm-maskは1.63/0.32°、2.38/4.31°、2.17/4.67°。計20 ROI-condition rowsとinput/mask/script hashesがRD31 captureにある。
- **価値または懸念:** 同一素材でmethod/intensityごとの数値色変化量を分布で比較できる。しかしsourceはdesired skin appearanceではなく、ΔE/hue変化の大小から許容肌色や自然さを判断できない。Hue eligibility cutoffは低chromaでの角度不安定を避ける計算条件だけで、skin classifierではない。出力はCore実測でなく式ミラー、maskは独立注釈されていない。
- **次に確認すべきこと:** 権利確認済みpaired photographic source/referenceで肌・測色neutral/whiteを同時に固定し、候補表示前に独立ROIとblind review criteriaを合意して、色保持/detail/white preservationを実Core routeで評価する。
- **未検証:** 意図した肌色、肌tone群を跨ぐ公平性、別人物・照明への一般化、Core出力とのcolor-response一致、測色ground truthへの誤差、独立盲検の許容判断。
## 2026-11-17: 同じCore-saved sourceを共通基準にしてCore previewとmirrorを比較できる
- **関連:** RD22 saved Core PNGs、RD9 equation mirrors、`experiments/ColorMatchLab/record_rd32_core_portrait_skin_color_response.py`。
- **確認できた事実:** 同じCore-saved source PNGを基準に、RD22 strength 0.5/1.0 actual-Core PNG previewsとRD9 float equation mirrorsのface/arm ROIを再集計した。strength 1のface median ΔE76 Core/mirrorはReinhard 1.91/1.68、MeanStddev 2.55/2.50、Histogram 2.59/2.53、median hue deltaは2.35°/0.90°、5.18°/4.92°、6.37°/5.32°。armではmedian ΔE76 1.94/1.63、2.17/2.38、2.57/2.17、hue delta 1.96°/0.32°、4.48°/4.31°、5.71°/4.67°。RD32 manifestに24行、mask/source/output/test/Core hashesを記録した。
- **価値または懸念:** 同じ数値source baselineに対する保存Core-previewと既存mirrorの分布を直接並べた。ただしCore側は8-bit PNG、mirrorは別に準備されたsource由来float outputを同じCore baselineに対して評価するため、量子化・入力作成経路・実装差が混ざる。数値差はfloat parity testでも画質不良証明でもない。
- **次に確認すべきこと:** 必要なら同じexact float bufferをCoreとmirrorへ入力した差分captureを行い、表示変換/保存を介さずCore float parityを評価する。製品品質の肌・白判断には引き続き権利確認済みpair、測色白、独立ROI、事前合意blind reviewが必要。
- **未検証:** 同一float inputに対する3方式全strengthのpixelwise Core/mirror parity、別portrait/illumination、適切な肌色保持、neutral-whiteとの相互作用、製品caller結果。
## 2026-11-17: 候補肌ROIのsource-relative Lab lightness変化も出力route別に記録できる
- **関連:** RD22 saved Core previews、RD9 equation mirrors、`experiments/ColorMatchLab/record_rd33_portrait_skin_lightness_response.py`。
- **確認できた事実:** RD32と同じCore-saved source、face/arm mask、Core PNG preview/mirror出力を使い、Lab L* signed/absolute deltaとoutput L*分位点を24行記録した。strength 1 median signed ΔL*はfaceでCore/mirrorの順にReinhard +0.88/+0.79、MeanStddev +0.46/+0.44、Histogram +0.31/+0.47。armは+0.89/+0.79、+0.45/+0.39、+0.29/+0.45。Core previewのROI/method別absolute ΔL* p95は0.54–1.01、mirrorは0.63–0.80だった。
- **価値または懸念:** 全strength-1条件のmedian lightness shiftが正であること、分布裾の範囲、Core-previewとmirrorの差を同じbaselineで記述できる。ただしsourceをdesired exposure/skin targetと見なせず、単一unpaired image・8-bit preview・異なるsource generationのmirrorを使うため自然さや正しさを意味しない。
- **次に確認すべきこと:** 権利確認済みpaired captureが揃ったら、measured neutral/whiteに基づく露出/WB targetを事前に固定し、skin ROIでsource-relative detail/color変化とtarget-relative correctionを分けて盲検評価する。
- **未検証:** 望ましいL*範囲、別人物・照明の肌明度、一貫した露出補正、測色白への誤差、float-output parity、製品画面での知覚。
## 2026-11-17: 画像診断recordersは上流manifestのhashをpreflightすべき
- **関連:** `experiments/ColorMatchLab/record_rd32_core_portrait_skin_color_response.py`、`record_rd33_portrait_skin_lightness_response.py`、RD20/RD9 capture manifests。
- **確認できた事実:** RD32/RD33のrecordersはCore source/output PNG、mask、equation-mirror出力の実hashを、それぞれRD20/RD9 upstream manifestの期待値と照合してから集計するよう更新した。RD33はRD20/RD9/RD32 manifest hashes、Core/test/asset hashes、分析scriptとLab conversion script hashesもcaptureへ記録する。再実行後も両CSVの24 rowsと既存CSV SHA-256は変わらなかった。
- **価値または懸念:** 同じファイル名の差し替え、古いmaskや別run outputの誤集計を数値処理前に検出できる。manifest一致はデータの品質・ライセンス・意味的適切さ自体を保証しない。
- **次に確認すべきこと:** Color Match Labの他のrecordersにも期待hashのpreflightがあるか棚卸しし、手動の再現手順へ実用的な範囲で揃える。
- **未検証:** 全RD recorderの入力改変検知網羅、外部素材provider状態、manifestを超えたpipeline決定性。
## 2026-11-17: 2枚目のshadowed portraitでもCore skin responseを分布で追加できる
- **関連:** RD23 shadowed CC0 portrait、`experiments/ColorMatchLab/record_rd34_second_portrait_core_skin_response.py`、RD34 capture。
- **確認できた事実:** RD23の保存Core previewと25,913px face candidate maskでstrength 1のsource-relative median ΔE76はReinhard/MeanStddev/Histogramで5.18/2.96/3.16、median eligible absolute hue shiftは5.00°/1.90°/2.02°、median signed ΔL*は+0.69/−0.42/−0.59。保存8-bit endpoint occupancyは16.73%/2.01%/3.37%。mask・source baseline・各method PNG hashはRD23 manifestと一致することをpreflightで検証した。7 rowsを保存。
- **価値または懸念:** 先のRD9 portraitとは異なるshadow/highlight構成の1例を同じ色・明度統計へ加えられた。応答の幅が素材/ROIに依存することが見える一方、単一追加例で一般化できず、constructed castは正解色でない。endpoint値はclippingの証拠ではない。
- **次に確認すべきこと:** さらに異なる肌色・照明を含む権利確認済みpaired capturesと測色白を揃え、ROIを独立注釈してから実Core品質基準を事前合意する。
- **未検証:** second portraitの望ましい肌色、別集団への妥当性、測色WB accuracy、neutral-white保持、blind acceptance、全float output pixel parity。
## 2026-11-17: ポートレート間の色応答差は単一素材の要約値を一般化できないことを示す
- **関連:** RD32/RD33 RD9 portrait、RD34 RD23 shadowed portrait、`summari
## 2026-11-17: 顔＋中立カードの公開研究データは利用権確認が先になる
- **関連:** `experiments/ColorMatchLab/EVALUATION_PLAN.md`、`docs/planned/MILESTONE_COLOR_MATCH_SKIN_WHITE_RND_2026-11-16.md`、Color Constancy with Faces、Zenodo skin-colour workbook、DAST。
- **確認できた事実:** Color Constancy with Facesの研究ページ/論文は472組の顔・neutral-card present/absent写真と手動ROIを記述するが、公開ページ上で画像利用ライセンスは確認できない。Zenodo記録はCM-2600dで測ったskin XYZとRAW/JPG由来RGBを含むworksheetを説明するが、dataset license欄を確認できず画像一式でもない。DASTは測色肌データを持つ一方、正式なdataset agreementを求め、公開cropで測色white/WB target pairを確認できない。
- **価値または懸念:** 必要な「顔＋中立基準」の構成が研究データに存在する手がかりを得たが、公開研究物だから製品R&Dに自由利用できるとは限らない。取得前に利用範囲、商用利用、派生ROI/統計の保持、センシティブ画像の管理を確認する必要がある。
- **次に確認すべきこと:** ユーザー提供の明示許諾済みpairを先に使う。外部セットを候補化する場合はproviderの明示的termsを確認し、測色skin targetとwhite/reference mappingが成立するかを画像取得前に確認する。
- **未検証:** 当該データの具体的利用権、撮影/render条件のpair equivalence、測色ターゲットの座標対応、製品評価としての妥当性。
## 2026-11-17: Color Match intake preflightは権利gateと素材gateを分ける
- **関連:** `experiments/ColorMatchLab/validate_pair_intake.py`、`PAIR_INTAKE_TEMPLATE.json`。
- **確認できた事実:** 一時生成の2×2合成PNGを使ったpreflightで、権利情報未入力時は画像を開かず停止すること、利用許可欄を満たした場合は画像のhash/寸法確認後にROI・評価条件不足を列挙することを確認した。寸法比較で `reference` recordをPillow image handleへ上書きする変数衝突も同じfixtureで露見し、修正した。候補PNGやCoreは実行していない。
- **価値または懸念:** 権利がない素材に触れず、許諾済み素材の入力不備も一回のpreflightで診断できる。synthetic fixtureは分岐と例外修正の検証に限り、実際のライセンス審査や画像品質を証明しない。
- **次に確認すべきこと:** 実ペア受領後に文書の利用範囲・被写体同意・mask保管可否を人手確認し、原本hash/format/profile、登録、独立ROI、閾値の全項目をpreflightとレビューで照合する。
- **未検証:** 実素材での権利証跡確認、ICC/profile内容の自動検査、ROIsや測色値の意味的妥当性、候補採点・Core品質。
## 2026-11-17: Cinematic Looks 仮mappingは正式表待ちに留める
- **関連:** `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cpp`、`Artifact/src/Composition/ArtifactAbstractComposition.cpp`、`Artifact/src/Effects/ColorCorrection/TritoneEffect.cpp`、`experiments/CinematicLooksLab/REGISTRY_AUDIT.json`。
- **確認できた事実:** Cinematic Looksブラウザーはmapping descriptorを持つpresetだけApplyを有効化し、Layer/Compositionの追加をそれぞれProjectServiceのundo対応APIへ渡す。Composition復元実装から追加したLUT/ColorBalance/Tritone module importは見つかったが、確認した各effect interface/implementationからComposition Abstractへの直接importは見つからなかった。TritoneEffectのGPU CPU-fallbackは当初premultiplied alphaをそのまま色処理していたが、同じunpremultiply/process/re-premultiply契約へ修正した。GPU定数bufferはpremultiplied flag追加で76 byteになっていた箇所を、HLSLの80-byte cbuffer配置に合わせてpaddingを戻した（source-level field-si
## 2026-11-17: 連番の「受入れ契約未固定」は実際には分散定義済みで未実行が本質
- **関連:** `docs/analysis/IMAGE_SEQUENCE_ACCEPTANCE_MATRIX_2026-11-17.md`、`docs/planned/MILESTONE_SOURCE_RELOCATION_CONTRACT_2026-08-21.md`、`docs/planned/MILESTONE_IMAGE_SEQUENCE_WORKFLOW_COMPLETION_2026-07-27.md`、`tests/ArtifactCore/ImageAssetMetadataContractTest.cpp`。
- **確認できた事実:** `REPORT_DCC_GAP_UPDATE_2026-08-15.md` は「欠番・開始番号・fps・保存再読込・キャッシュ・フレーム外参照を受入条件として固定する必要がある」と記載するが、実コード照合では `ImageSequenceSourceContractTest`（20+ 件）と M-SRC-RELOC-1 の解決優先順位／保存キー棚卸しで大部分が既に固定されている。真の残課題は app 側の runtime 受入れ（E-1〜E-4/F-1〜F-3/C-5/A-7）と、16bit 連番が `QImage` 経由で 8bit 化される点（10-04 レポート B-10、未着手）だけである。
- **価値または懸念:** 旧ギャップ表の文言だけを根拠に「契約を新規作成」すると既存の Core 契約テスト／Undo／JSON／relink 導線を二重定義し、保守コストを増やす。逆にコード接続のみを「完成」と誤認すると未受入 runtime 項目を除外してしまう。
- **未検証:** runtime 受入れ（ビルド・実機禁止のため未実施）。16bit 連番の実素材での挙動も未確認。
- **次に確認すべきこと:** ビルド許可が得られたら受入表の RT-未 ケースを順に実行し、失敗段階を source/decode/color/cache/composite で分類する。B-6 は別途「QImage を経由しない連番デコード経路」の設計レビューを先に行う。
## 2026-11-17: 透過 PNG の素材移設・再リンク・書き出し受入プロジェクト

- **関連:** [静止画レイヤー受入マイルストーン](docs/planned/MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md)、`tests/Artifact/StillImageLayerRoundTripTest.cpp`、`tests/Artifact/RenderQueueLayerImageIntegrationTest.cpp`。
- **確認できた事実:** 保存・再読込、欠落・再リンク、実素材による Preview／Render Queue の比較はマイルストーン上で実機受入が残っている。
- **提案:** 半透明の縁と非対称な模様を持つ sRGB PNG 1 枚を、明暗の背景に重ねる。クロップとマスクを適用した 1 コンポジションの `.artifact` と素材を同梱し、確認対象は 1 フレームに絞る。
- **価値:** 同じフレームを各操作後に比較することで、素材パスの復元、欠落からの復旧、クロップ位置・マスク境界・半透明の縁の描画差を切り分けられる。
- **次に確認すべきこと:** 元素材への参照を切ってプロジェクト一式を移設し、相対パスで復元する。次に移設先の素材をリネームして欠落表示を確認し、再リンクする。移設前・再読込後・再リンク後の同一フレームを、色空間と出力条件をそろえて Preview／Render Queue で比較する。
- **未検証:** プロジェクト・素材の作成と実機受入は未実施。
## 2026-11-17: モーショングラフィックス機能のギャップ文書が実コードより大幅に古い
- **関連:** `docs/analysis/GAP_AE_NUKE_2026-08-01.md`、`docs/spec/SPEC_TEXT_TOOL_REQUIREMENTS_2026-07-31.md`、`docs/planned/MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md`、`Artifact/src/Layer/ArtifactShapeLayer.cpp`、`Artifact/src/Widgets/CompositionTextEditor.cpp`、`ArtifactCore/src/Script/Expression/ExpressionEvaluator.cpp`。
- **確認できた事実:** ソース直読では、パスモーフ評価（`evaluatePathAt` の `ShapePath::interpolate` 経由）、フラットな複数シェイプ／コンテンツとスタック、TextTool のキャンバス生成、`ToolType::Text`、ダブルクリック→ダイアログ編集（Ctrl+Enter/Esc＋Undo）、TextAnimator 配線、`wiggle/loopOut/loopIn/smooth` の実装・登録・エディタ補完まで存在する。一方ギャップ文書は Text Tool 20% / VP編集 5% / Wiggle・loopOut 0% / パスモーフィングUI未着手と記載しておりコードと食い違う。
- **価値または懸念:** 古いギャップ表から工数を見積もると「新規実装」を繰り返し、既存の Undo／JSON／レンダリング導線を壊す恐れがある。逆に実コードだけを根拠にすると未受入の runtime 項目（ビルド・実機・Undo往復）を実装済みと誤認する。両方を照合して残作業を定義すべき。
- **未検証:** runtime 受入（今回はビルド禁止のため未実行）。差分が意図的な廃止か追記漏れかも未確認。
- **次に確認すべきこと:** ギャップ文書の該当行を実コード照合で更新する独立タスクを立てる。今回のシェイプ／テキスト作業では仕様書のチェックボックスをそのまま受入条件に使わない。
## 2026-11-17: layer→layerFloatの明示数式にはsRGB decodeがなく順序はSRV Load依存
- **関連:** `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx:106-137`、`Artifact/src/Render/ArtifactRenderLayerPipeline.cpp:3611,1249-1251`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx:240-268`、`docs/planned/MILESTONE_BUFFER_PRECISION_ALPHA_CHAIN_RD_2026-11-17.md:15`。
- **確認できた事実:** GPU昇格シェーダーはunpremultiplyと`saturate`、straight保存、BGRAスワi
## 2026-11-17: 精度誤差は順序・クランプ・量子化で支配要因が別れる
- **関連:** `experiments/PrecisionLab/separate_contributions.py`、`experiments/PrecisionLab/captures/contributions-rd2/contributions.json`。
- **確認できた事実:** reference→clamp→order→quantの段階差で分離。alpha_fringeのalpha境界はorder 0.06932に対しclamp 0.00225/quant 0.00051、chromatic_edgeもorder 0.06943が支配。hdr_rampはclamp 0.76451でorder 0、banding_rampはquant 0.001で他0。free
## 2026-11-17: premultiplied sRGBの順序違反が部分alphaエッジの主因、隠れRGBは主因ではない
- **関連:** `experiments/PrecisionLab/analy
## 2026-11-17: OpenCV inpaintの8-bit往復はマスク外不変契約を壊す
- **関連:** `experiments/InpaintLab/compare_inpaint_candidates.py`、`experiments/InpaintLab/captures/candidates-rd2/candidate-metrics.json`、`docs/planned/MILESTONE_INPAINT_CONTENT_AWARE_FILL_RD_2026-11-17.md`。
- **確認できた事実:** cv2.inpaint 4.13は8-bit 3-channelと32-bit float 1-channelのみ対応。8-bit sRGB往復の候補は14 fixtureすべてでマスク外RGB/alphaが変化しG1失敗。per-channel float32 NSはG1-G5通過でradius1が細線・エッジに最良（line ROI MAE 7.5e-6）。Telea float32は定数フィールドで半径依存の値（0.1→1.39、2.5→2.999、10→10.499、NSは一定保持）を出しG4/G5/G6失敗。NS float32でもHDRディスクは再現できず領域MAE 2.98、マスク内alphaはsourceコピーでGT差0.5。
- **価値または懸念:** 8-bit表示経由のinpaintは見た目が近くてもマスク外を変えるため契約違反。HDR/alphaの期待値を定義せずに実装へ進むと受入で破綻する。
- **未検証:** Artifact runtime経路、preview/final parity、性能、Telea float32不安定挙動の原因。
- **次に確認すべきこと:** R3でHDR/alphaの期待契約を先に定義し、ns_f32_r1を実装候補として受入条件を決める。
## 2026-11-17: 画像エフェクトR&Dのfixture規約はblur-rdに集約されつつある
- **関連:** `experiments/blur-rd/build_gaussian_fixture_bundle.py`、`docs/planned/MILESTONE_BLUR_EFFECTS_RD_2026-11-12.md`、`docs/planned/MILESTONE_CONTENT_AWARE_FILL_2026-07-08.md`。
- **確認できた事実:** Blur R&Dはfloat32 RGBA NPYを測定源、PNGを表示用と分け、fixture／スクリプト／解析器／設定にSHA-256を付けるmanifest方式を使っている。inpaintは`MILESTONE_CONTENT_AWARE_FILL_2026-07-08.md`でDRAFT／未実装が繰り返し確認されており、実装より先に評価計画を置く余地がある。
- **価値または懸念:** 新規エフェクトR&Dが独自形式のfixtureを作るとalpha associationと表示変換の前提が案件ごとに割れ、横断比較ができなくなる。測定用floatと表示用PNGの分離、hash付きmanifest、未検証項目の明示は再利用すべき規約。
- **未検証:** inpaint R&Dでこの規約を実際に運用した結果、Artifact runtime captureとの接続方法は未確認。
- **実行結果（2026-11-17）:** fixture生成を実行し、14 fixture／84ファイルのmanifest hash不一致0、別ディレクトリ再実行でhash集合完全一致を確認。thin_lineの表示用PNG目視も実施。アルゴリズム適用・閾値・runtime captureは未実施。
- **次に確認すべきこと:** R1の統計と目視用contact sheetを追加し、実装段階のcapture adapterを設計する。
## 2026-11-17: 3スクリプト基盤の「世界への窓口」に共通のギャップがある
- **関連:** `Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`、`Artifact/src/Export/Python/ArtifactPythonAPI.cpp`、`ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cpp`、`docs/planned/DESIGN_REVIEW_SCRIPT_API_P2_2026-11-17.md`。
- **確認できた事実:** C# は `Time`/`DeltaTime`/`Frame` のみで composition・selection への API が無い。Python は `WorkspaceAutomation` の 100 超の操作の一部しか露出していない。ArtifactScript は property/keyframe はあるが Transport とエフェクトが未バインド。戻り値契約も基盤ごとに異なる。
- **価値 / 懸念:** 今回は P0/P1（参照解決、構造化エラー、AE ラッパー再帰、数値プロパティ、文字列／配列ユーティリティ）を実装し、P2 は設計レビュー資料に留めた。P2 のうち C# の `ArtifactApi` facade は delegate 寿命と例外・停止ポリシーの定義が先決で、コルーチンは評価器の根幹に触れるため最後に回すべき。
- **追加の既知リスク（未修正）:** C# の `_session` / `_globals` は static 1 つなので、複数レイヤー／composition の同時実行で状態が衝突する。session ID 付き辞書化が必要。
- **次に確認すること:** 設計レビュー資料の承認後、項目3（Transport read-only）から着手する。ビルド・hostfxr 有効環境での実機確認はユーザー明示指示待ち。
- **未検証:** いずれも静的調査のみ。Roslyn の実参照解決、hostfxr 実行、CTest は未実施。
## 2026-11-17: FSR1 の既存シェーダは「配線するだけ」ではなく、埋め込み前に移植が必要
- **関連:** `Artifact/shaders/fsr_upscalingCS.hlsl`、`fsr_sharpenCS.hlsl`、`ArtifactCore/src/Graphics/Compute.cpp:63-73`、`ArtifactCore/include/FSR/ffx_fsr1.h`、`docs/planned/MILESTONE_FSR1_SPATIAL_UPSCALE_RD_2026-11-17.md`。
- **確認できた事実:** 両 HLSL は Wicked Engine の `CONSTANTBUFFER` マクロ、`sampler_linear_clamp : s100`、`globals.hlsli` / `ffx-*.h` の include、`register(spaceN)` を前提とする。`ComputeExecutor::build()` は `ShaderCreateInfo` に include ハンドラを設定せず生文字列を渡すため、そのまま埋め込めない。さらに `[numthreads(64,1,1)]` で 1 グループ 16×16px のため、`makeDispatchAttribs(w,h,1,8,8,1)` は誤り。Resolve 後の `accumSRV` は preview 解像度かつ BGRA で、全解像度 RT と色契約の整理も必要。
- **事実（訂正）:** 既存の「動的解像度」`ArtifactIRenderer::setUpscaleConfig()` は源代码に呼び出し元ゼロ。実プレビューの低解像度化は `previewDownsample_` が駆動している。旧調査の「setUpscaleConfig の拡大パスに接続」という想定はそのままでは使えない。
- **価値 / 懸念:** シェーダ資産の存在だけで「低リスク・すぐ配線可」と判断すると、ffx ヘッダ展開・全解像度 RT のホットパス外確保・BGRA→RGBA 契約・backend parity を低估する。画像評価計画が必須なのもこの種の変更。
- **未検証:** 実 GPU でのシェーダコンパイル、D3D12/Vulkan parity、画質、性能。ビルド・キャプチャ禁止のため未実施。
- **次に確認すること:** ビルド許可が得られたら、自己完結 HLSL のコンパイルを最初に確認し、その後 baseline A/B を取得して閾値を確定する。定数生成は `tools/fsr_constants_rd.py --self-test` で静的に検証済み。

## 2026-11-16: York/Adobe WB pair set is technically strong but scope-restricted
- **関連:** `experiments/ColorMatchLab/EVALUATION_PLAN.md`、`docs/planned/MILESTONE_COLOR_MATCH_SKIN_WHITE_RND_2026-11-16.md`。
- **確認できた事実:** York/Adobe公式ページは62,535組のDSLR入力・chart-derived targetと、追加のcross-camera/Cube+ pairを説明する一方、データをpost-capture white-balance correctionの評価以外の目的で提供し、画像はreasonable academic fair useとする。
- **価値または懸念:** 技術的には肌/中立パッチを含む強いWB評価候補だが、プロダクトR&Dの評価目的と提供範囲が合わない。研究限定のrepository説明だけでは目的制限や画像ごとの権利・派生物条件が解消しない。
- **次に確認すべきこと:** このデータで評価を行う場合、提供者から目的とcrop/mask/output/metrics保存を含む書面許諾を得てから扱う。未許諾の間は利用候補から除外する。
- **未検証:** 個別画像ライセンス、派生物の保存・共有条件、別目的の書面許諾。
## 2026-11-16: Runtime capture should compare full vector lanes for material math
- **関連:** `tests/ArtifactCore/RayTracingAcceptanceTest.cpp::CapturesLegacyAndDynamicRampAndRoughnessMathCandidatesOnVulkanMeshRenderer`、`tests/fixtures/shader-graph-rd/math_add.json`、`experiments/ShaderGraphLab/core_math_add_projection_candidate.json`。
- **確認できた事実:** 既存のRGBA32F Vulkan harnessにBaseColor Vector4 Add / Subtract / Multiply / positive-floor Divideのlegacy-versus-typed候補source caseを追加した。candidate generatorは全variantのtyped document / HLSL / goldenを出力する。さらにreport-driven analy
## 2026-11-16: Legacy BaseColor arithmetic can be projected without collapsing RGBA lanes
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::project_legacy_core_material_input`、`tools/shader_graph_rd/compile_legacy_core_projection_candidates.py`、Core `MathNode` material lowering。
- **確認できた事実:** The selected BaseColor lane now projects legacy componentwise Vector4 Math Add, Subtract, Multiply, and positive-floor Divide into distinct typed operations. DXC/HLSL and glslang/GLSL Vulkan 1.2 constant-fold outputs match all four binary32 lanes for the generated RGB-node fixture variants; a portable 10/10 compiler evidence report is pinned under `experiments/ShaderGraphLab/` and checked by the manifest validator.
- **価値または懸念:** The adapter can carry more legacy material arithmetic through a typed graph while preserving alpha and the unusual positive-floor division rule. This does not settle Color4 transfer/premultiplication policy or the product decision that graph BaseColor replaces fixed-PBR tint/factor contributions.
- **再現性:** `generate_legacy_core_projection_candidate.py --check` now byte-compares an in-memory regeneration with the checked-in candidate without modifying it; the current four-operation candidate matches.
- **次に確認すべきこと:** Run the authori
## 2026-11-16: Vulkan graph capture source compares legacy and typed scalar lanes
- **関連:** `tests/ArtifactCore/RayTracingAcceptanceTest.cpp::CapturesLegacyAndDynamicRampAndRoughnessMathCandidatesOnVulkanMeshRenderer`、`tests/fixtures/shader-graph-rd/texcoord_math_roughness.json`、`experiments/ShaderGraphLab/core_math_add_projection_candidate.json`。
- **確認できた事実:** 既存headless Vulkan MeshRenderer capture harnessへ、legacy JSON loweringとtyped candidate HLSL injectionを同じmesh UV・RGBA32F readbackで比較するsource caseを追加した。五つのUV0.x値とfixture/candidate/capture SHA-256を記録する設計で、数値比較はBaseColor AOV赤channelを使う。これはC++ source上の事実で、captureは未実行。
- **価値または懸念:** CPU golden/compiler constant-foldだけだったscalar migrationに、Core renderer上で選択入力を比較する次の評価段階ができた。一方、typed candidate注入はtest-only host bindingで、製品adapter経路・BRDF appearance・roughness clamp・backend parityは証明しない。
- **次に確認すべきこと:** 許可されたbuild/testでVulkan captureを実行し、reportとraw outputを確認する。値が一致した場合もD3D12 paired run、product adapter、save/reload、performance gateを別に満たしてからCore API昇格を判断する。
- **未検証:** capture sourceのコンパイル、Vulkan実行値、production adapter経路、D3D12 parity、persist/reload。
## 2026-11-16: Clip bar design prototype should keep GPU-path evidence separate
- **関連:** `experiments/TimelineClipBarLab/`、`tools/TimelineClipBarPlayground/`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`。
- **確認できた事実:** `ArtifactIRenderer` は Diligent 経由の矩形・パネル・線・文字描画APIを公開しており、タイムラインWidgetを使わない比較用ネイティブPlaygroundを構成できる。製品Timelineには別途 clip hit-test、選択、trim、サムネイル／波形のデータ供給がある。
- **価値または懸念:** ブラウザーのデザイン探索とGPU描画確認を分離すれば、視覚案の比較結果を製品経路やGPU parityの証拠と誤認しにくい。
- **確認結果:** `ArtifactTimelineClipBarPlayground` をDebug構成でビルドし、D3D12／NVIDIA GeForce RTX 4070 Tiで3案を含む1200×650 PNGをGPU描画・readbackできた。出力は `experiments/TimelineClipBarLab/captures/diligent_clip_bars.png`。
- **次に確認すべきこと:** 製品Timelineへの採用は別レビューとし、実際のTimeline input・選択・trim経路は統合時に確認する。
- **未検証:** Vulkan経路、実タイムライン連携、GPU性能計測。
## 2026-11-16: Tritone wrappers should preserve premultiplied color association
- **関連:** `Artifact/src/Effects/ColorCorrection/TritoneEffect.cpp`, `Artifact/src/Effects/ColorCorrection/ColorBalanceEffect.cpp`, `experiments/CinematicLooksLab/captures/rd2_effect_extension/metrics.json`.
- **確認できた事実:** Color Balance effect wrapper already checks the surface alpha descriptor, transforms straight RGB, and re-premultiplies. Tritone CPU/GPU wrappers were updated to follow that contract; the HLSL parameter block remains 80 bytes with an explicit premultiplied flag. rd2 source-mirror alpha sweeps measured maximum recovered-RGB deltas of about 1.2e-7 for Tritone CPU and GPU equations.
- **価値または懸念:** Applying nonlinear color transforms directly to premultiplied channels makes the recovered straight color depend on alpha, which can expose halos after compositing. The mirror does not establish the live Composition descriptor or actual renderer behavior.
- **次に確認すべきこと:** After an authori
## 2026-11-16: Looks browser presets and executable LUT presets are separate inventories
- **関連:** `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cpp`、`Artifact/src/Effects/ColorCorrection/LUTEffect.cpp`、`ArtifactCore/src/Color/ColorLUT.cpp`、`experiments/CinematicLooksLab/`。
- **確認できた事実:** Looks browser owns a static name/category/pack list and generated placeholder thumbnails. Independently, LUTEffect selects built-in cinematic / vintage / cold / warm / contrast / desaturated / film-stock LUT implementations and exposes intensity. No adapter between the two registries was observed in the inspected source paths.
- **追加確認:** `LooksPresetBrowserDialog` にリポジトリ内の生成 callsite はなく、Apply は `appliedId_` と画面 badge の更新だけで終わる。外部へ返るのは選択ID getterのみで、IDからeffect/presetへ変換する source-backed mapping もない。
- **追加確認:** Composition `Impl::addEffect` は同種effectのinstance IDへ `-2`, `-3`, ... を付ける。color-effect deseriali
## 2026-11-16: Legacy Shader Graph divide floors negative denominators to a positive epsilon
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::MathNode::Op::Divide`、`experiments/ShaderGraphLab/registry.json`、`dynamic_divide.json`。
- **確認できた事実:** legacy HLSL emits `a / max(b, float4(1e-5f,...))`. This floors each denominator to a positive epsilon; a negative input is not divided by its signed value and is not handled by `abs(b)` epsilon. The R&D registry now exposes the scalar projection under the explicitly named `float.divide.positive_floor_1e5` implementation and uses a negative divisor in its focused document.
- **価値または懸念:** Naming the operation after its actual denominator policy prevents a conventional divide node from silently inheriting asymmetric legacy behavior. The difference is visible around negative and near-
## 2026-11-16: Typed IR now feeds the prototype HLSL emitter
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py` (`compile_document_from_data`), `experiments/ShaderGraphLab/README.md`, `docs/planned/MILESTONE_SHADER_GRAPH_RND_2026-11-13.md`, `libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineVulkan/src/ShaderVkImpl.cpp`.
- **確認できた事実:** The prototype has independent HLSL and Vulkan GLSL 4.5 verbatim emitters consuming the same typed IR. All 21 GLSL fragments compiled with glslang, all 21 HLSL wrappers compiled with DXC, and all 42 SPIR-V modules passed `spirv-val` for Vulkan 1.2. `spirv-dis` interface parsing found matching descriptor names/set/bindings, parameter member offsets, and stage input/output locations/types for 21/21 pairs. The compile tool reproduces this and records hashes under `temp/shader-graph-glsl450-compile/summary.json`. Diligent's Vulkan source path accepts `SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM`, but the application path was not run. The HLSL path still matches all 21 saved HLSL bodies and typed-IR hashes. A post-projection Document mutation probe re-emitted identical HLSL from saved IR/layout/context. The full manifest validator was not run.
- **価値または懸念:** Compiler acceptance and shader-interface agreement are now evidenced, but arithmetic equivalence and rendered parity remain unproven. Descriptor/register assignments are still a candidate proposal, and host inputs are supplied by compile wrappers.
- **次に確認すべきこと:** Compare numeric outputs for shared deterministic operation cases, then run application binding/resource and GPU image checks before resolving failure/fallback behavior or considering Core promotion.
## 2026-11-16: Ramp の2コンパイラ出力は一致するが CPU golden と1 ULPずれる
- **関連:** `tools/shader_graph_rd/probe_vulkan_ramp_numeric_parity.py`、`temp/shader-graph-glsl450-compile/ramp-numeric-parity.json`、`tests/fixtures/shader-graph-rd/dynamic_ramp_formula_golden.json`、`ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::ColorRampNode`。
- **確認できた事実:** 事前登録済み5 factorを定数UVとしてHLSL/DXCとGLSL/glslangからVulkan 1.2 SPIR-Vへコンパイルし、`spirv-opt -O` 後の定数出力を比較した。両frontendは5/5でRGBA binary32が完全一致した。CPU binary32 goldenとは factor 0.4999 の alpha だけ1 ULP異なる（shader `0.200159982`、golden `0.2001599669456482`）。goldenとの厳密一致ゲートは変更せず、プローブは `failed` を返す。ソース調査では Core ColorRamp がsorted stopsへの逐次 clamped `lerp` を生成する一方、dynamic candidate は半開区間を選択し、重複位置の境界方針を明示している。
- **価値または懸念:** コンパイラ間の数値一貫性は小さなfixtureで確認できたが、CPU式との厳密一致は示せず、さらにcandidateと現行Coreのstop選択・補間手順が同一とも言えない。許容誤差を事後に導入すると差を隠すため、今はstrict failureとして記録する。
- **次に確認すべきこと:** `ArtifactCore/RayTracingAcceptanceTest.cpp` の既存Vulkan capture caseで、同一入力の現行Core ColorRampとcandidate HLSLをRGBA32Fで比較し、stop境界・alphaを別々に解析する。結果に基づき、Coreへ昇格するoperation semanticsとCPU goldenの役割を決める。
- **未検証:** GPU/runtime値、Core capture case、D3D12/Vulkan実描画、linear/display変換、alpha association、Blender実挙動。今回のコンパイラ constant-fold probeはこれらの代替ではない。
## 2026-11-16: R&D validator の先行例外が後続契約検査を隠す
- **関連:** `tools/shader_graph_rd/validate_shader_graph_manifest.py`、`experiments/ShaderGraphLab/dynamic_graph_prototype.py`。
- **確認できた事実:** dynamic graph の全検査を包む try ブロックで、最初の編集スナップショットが tuple を厳格 JSON として拒否し、その時点で検査を中断していた。JSON 値境界へ tuple→array 変換を入れた後に全検査が進み、node-scoped resource ID 欠落、asset snapshot JSON 深さ制限漏れ、複数の古い manifest assertion も見つかった。修正後の manifest validator は一致を報告した。
- **価値または懸念:** 一括 validator が例外で終了すると、後続に並ぶ独立契約の状態は観測されない。最初の例外を直した後に必ず全 validator を再実行し、summary message が「failed checksなし」でも追加の外側条件があることを確認する必要がある。
- **次に確認すべきこと:** 大きな validation block を契約単位で実行・報告できる構造へ分割する設計を検討する。ただし今回は validator 構造自体のリファクタリングは行わない。GPU capture と Core promotion gates は別の実証として残す。
- **未検証:** 失敗を複数収集する validator 方式が現行 R&D の実行時間・診断品質を改善するか。
## 2026-11-16: Ramp の1 ULP差は線形補間の演算順で再現できる
- **関連:** `tools/shader_graph_rd/probe_vulkan_ramp_numeric_parity.py`、`dynamic_ramp_formula_golden.json`、HLSL `lerp` / GLSL `mix` candidate。
- **確認できた事実:** CPU golden の `left + (right-left)*t` は factor `0.4999` のalphaを `0.2001599669456482` とし、各演算をbinary32丸めする `left*(1-t)+right*t` は `0.2001599818468094` を得る。DXCとglslangの最適化済みSPIR-V定数出力は後者とRGBA全成分で5/5一致し、互いにも5/5で一致する。
- **価値または懸念:** 1 ULPはコンパイラ間の不一致やランダムノイズではなく、代数的に同じ補間式の評価順による差と説明できる。候補に対する参照式を一つに固定し、別の数学的等価式を誤差と混ぜない検査が必要。ただしCore ColorRampの逐次合成は別アルゴリズムで、GPU実行値や製品許容差は未確定。
- **次に確認すべきこと:** Core captureで現行ColorRamp、dynamic shader-mix candidate、両CPU式を同じRGBA32F座標で比較し、厳密参照と実用許容差を事前登録する。
- **未検証:** 実GPUのFMA/精度、Diligent runtime、Coreとの画素差、D3D12 parity、Blender parity。
## 2026-11-16: Dynamic material inputs can be projected as host-context bindings
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp`、`experiments/ShaderGraphLab/dynamic_graph_prototype.py`、`tools/shader_graph_rd/compile_legacy_core_projection_candidates.py`。
- **確認できた事実:** legacy `TexCoord.UV` exposes a Vector2 and the audited Core conversion to Float selects component `.x`. The read-only R&D adapter maps this to UV0.x and compiles it as `In.UV.x` (HLSL) / `artifactHostUV.x` (GLSL); both Vulkan 1.2 frontend paths validate and produce matching SPIR-V interface projections. The manifest validator now also pins the Core `In.UV` lowering, `.x` conversion, and MeshRenderer `In.uv` → `Out.UV` source bridge. A six-point UV0.x speciali
## 2026-11-16: Target closeness can increase skin-chart chroma
- **関連:** `experiments/ColorMatchLab/run_rd8_skin_white_chroma.py`, RD6 ColorChecker WB pair, `AutoColorMatcher` equation mirrors.
- **確認できた事実:** In the registered Canon EOS 20D ColorChecker pair, the camera WB2 target's combined skin-tone proxy mean-vector C*ab is 29.74 vs 15.23 in WB1. At intensity 1.0 the three mirrors produce C*ab 25.83 / 29.11 / 29.81 and mean-Lab ΔE76 to WB2 4.898 / 1.217 / 0.077. The bright patch hue moves from 11.73° in WB1 to 43.71° in WB2; the three outputs reach 40.16° / 40.88° / 42.37°. The white patch source and WB2 mean-Lab hue directions differ by 166°, so WB-target matching also changes white-patch chroma/direction.
- **価値または懸念:** Lower error against a camera rendition target can require a large chroma increase on standardi
## 2026-11-16: Skin protection is a regional target-error tradeoff
- **関連:** `experiments/ColorMatchLab/captures/rd9_real_skin_white_scored/manifest.json`, `captures/rd10_roi_strength_tradeoff/manifest.json`, `ArtifactCore/src/Color/AutoColorMatch.cpp`。
- **確認できた事実:** On a licensed CC0 real portrait with a known constructed RGB cast, intensity-1 Histogram mirror reduces skin ROI mean ΔE76 to that constructed reference from 2.944 to 0.007 while combined skin mean-vector C*ab shifts 14.23 to 11.77 and face hue shifts 42.33° to 48.50°. MeanStddev also exceeded 1.0 in 0.732% of full-frame RGB channel samples (1.481% of pixels) and in 6.281% of channel samples / 11.346% of pixels in the light-blouse ROI. In a separate fixed-mask post-match control, 0.5 skin blend factor yields skin source/target ΔE76 1.480/1.465 and C*ab 12.98; factor 1 yields 2.946/0.007 and C*ab 11.77. Full-frame target error moves only 0.098 to 0.013, so global averages attenuate this ROI-level tradeoff. Across cool / warm / green synthetic targets, target skin C*ab shifts to 11.77 / 15.87 / 13.69 from 14.23; target blouse C*ab shifts to 5.41 / 12.65 / 12.08 from 7.70. The paired-face dataset literature describes 472 manually marked face/card pairs, but the accessible page does not state data license or download; PPR10K explicitly restricts images and derived data to non-commercial research.
- **価値または懸念:** Color-match accuracy and source skin preservation are independent objectives. A future safe candidate should expose or internally constrain regional influence only if users can inspect the source/target tradeoff; fixed-mask results are oracle upper bounds and do not imply automatic skin segmentation.
- **次に確認すべきこと:** Compare reviewed masks and blind judgments on multiple people, lighting conditions and skin tones; test the actual Core path and define an accepted skin hue/chroma tolerance before considering implementation.
- **未検証:** The target is synthetic rather than an actual second capture; the single blouse ROI is patterned/shaded; masks are analyst-reviewed, not independent; no Core differential, blind quality gate, or performance data exists.
## 2026-11-16: Cast strength changes the apparent skin-preservation direction
- **関連:** `experiments/ColorMatchLab/run_rd12_cast_strength_sensitivity.py`、`captures/rd12_cast_strength_sensitivity/manifest.json`、RD9 CC0 portrait masks。
- **確認できた事実:** At the same fixed cool direction, increasing only the constructed RGB offset multiplier from 0.5 to 1.5 moves target skin C*ab 10.85→12.74 and blouse C*ab 4.90→6.02. For the fixed warm direction those values move 16.97→14.78 and 13.28→12.03; for green, skin stays near 13.6–13.8 while blouse falls 12.93→11.24. Histogram at intensity 1.0 remains close to each synthetic target (mean ROI ΔE76 below 0.42), but output distance from the original remains non
## 2026-11-16: AutoColorMatcher statistics ignore alpha but include hidden RGB
- **関連:** `ArtifactCore/src/Color/AutoColorMatch.cpp`、`tests/ArtifactCore/AutoColorMatcherContractTest.cpp`、`experiments/ColorMatchLab/alpha_statistics_counterfactual.py`。
- **確認できた事実:** Reinhard extracts Lab values from every source/reference pixel's RGB channels; MeanStddev and Histogram extract every RGB sample into their channel statistics/CDF. The alpha channel is not consulted in those statistics and is left unchanged during output mapping; existing contract tests cover alpha preservation. Consequently RGB stored under alpha 0 still affects the global match. The separate alpha counterfactual currently describes this mechanism but does not run the Core output.
- **RD13で確認できた追加事実:** Source-equation mirrors were run on two portrait buffers with identical alpha and visible RGB, but black vs magenta RGB under a synthetic 20%-width alpha-
## 2026-11-16: AutoColorMatcher has no checked-in application call site
- **関連:** `ArtifactCore/include/Color/AutoColorMatch.ixx`、`ArtifactCore/src/Color/AutoColorMatch.cpp`、repository-wide search for `AutoColorMatcher` and `import Color.AutoMatch`。
- **確認できた事実:** The repository search found the class declaration, Core definitions, one contract-test source, and R&D/docs mentions, but no application code importing or calling the API. The public API mutates either an `ImageF32x4_RGBA` source or caller-provided raw BGRA float pixels. This is source-search evidence, not proof against runtime reflection/dynamic loading.
- **価値または懸念:** Product preview/export/backend/performance requirements cannot be evaluated as current behavior until a real caller exists. R&D of the algorithm should remain separated from product integration; otherwise offline mirror results could be mistaken for an editor feature assessment.
- **次に確認すべきこと:** If a product-facing feature is requested, first identify its owning UI/editor and image-buffer lifecycle, then decide whether to create a caller only after explicit scope and review. For this R&D, record Core source equation observations and leave actual route parity/performance as open gates.
- **未検証:** Dynamic/plugin-generated call sites, downstream consumers outside this workspace, and any intended editor path.
## 2026-11-16: AutoColorMatcher output depends on float transfer encoding
- **関連:** `experiments/ColorMatchLab/run_rd15_encoding_sensitivity.py`、`captures/rd15_encoding_sensitivity/manifest.json`、`ArtifactCore/src/Color/AutoColorMatch.cpp`、`ArtifactCore/src/Image/ImageF32x4_RGBA.cpp`。
- **確認できた事実:** `ImageF32x4_RGBA::setColorDescriptor` はtransfer/primariesを保持してstorage/orderをcanonicali
## 2026-11-16: AutoColorMatcherの各方式はHDR範囲を異なる形で扱う
- **関連:** `experiments/ColorMatchLab/run_rd16_float_range_sensitivity.py`、`captures/rd16_float_range_sensitivity/manifest.json`、`ArtifactCore/src/Color/AutoColorMatch.cpp`。
- **確認できた事実:** ソースではReinhardがLab変換前と逆変換後にRGBを0〜1へclampし、Histogramは値を256-binの0〜1 LUTへ写像し、MeanStddevは生float統計と無clamp出力を使う。有限値の固定合成fixtureにおける強度1ミラーは、Reinhard/Histogramが全出力を0〜1へ収め、MeanStddevはunderflow/1超の値を保持した。source/referenceをアフィン関係で構成したためMeanStddevがほぼ完全復元した。
- **価値または懸念:** float型であることはHDR保持契約を意味しない。手法ごとの値域動作が異なり、SDR前提・Scene-Referred運用のどちらをmatcherに求めるかで結果解釈が変わる。ただしRD16は39画素の合成値列であり、実画像における望ましさや目立ちやすさは未検証。
- **次に確認すべきこと:** Core differentialを許可された評価段階で、実際のlinear scene-referred画像と正しいdescriptorを用い、共通のdisplay transform前後で比較し、高輝度ディテールと肌/白ROIをレビューする。必要なHDR挙動と受入範囲をそのレビュー前に定める。
- **未検証:** 実Core出力、HDR表示変換と見た目、実写高輝度ディテール、NaN/Inf・巨大値、primaries/ICC、alpha、性能、製品経路。
## 2026-11-16: 自然画像の白ROIはsource-only色しきい値だけでは分離できなかった
- **関連:** `experiments/ColorMatchLab/prepare_rd17_fridge_roi.py`、`captures/rd17_white_object_roi/roi_review.json`、Middlebury Canon EOS 5D kitchen pair。
- **確認できた事実:** 冷蔵庫右扉の固定矩形内にsource-onlyの明度/chroma thresholdを適用したところ15,613画素が選ばれたが、mask overlayにはマグネット、写真/ステッカー、花/茎、扉境界が残った。事前登録したreject ruleに従いROI別候補評価は実行していない。
- **価値または懸念:** 色だけのしきい値は白い物体と、その上にある明るい物・反射・境界を区別できない。汚染ROIの統計を「白保持」として扱うと誤った結論になるため、ROI採用の視覚ゲート自体が必要。
- **次に確認すべきこと:** 次のROIを試すなら、baselineだけで物体領域を手動ポリゴン注釈し、候補出力を見ずに別ROI条件として固定してから測る。複数物体/場面と独立確認も必要。
- **未検証:** 手動ポリゴンによるROIの再現性、実際のニュートラル度、別シーンへの代表性、Core出力、盲検レビュー、性能。
## 2026-11-16: カメラJPEGへの白ROI一致は入力白の保持と異なる
- **関連:** `experiments/ColorMatchLab/captures/rd18_lamp_white_roi/manifest.json`、Middlebury EOS 5D kitchen RAW-render/JPEG pair。
- **確認できた事実:** source-onlyで採用した左右ペンダントシェードROI（1,222/1,141画素）のsource→camera-JPEG ΔE76は22.60/22.92。JPEG L*は84.25/85.13、source L*は61.76/62.37。Histogram強度1ミラーはJPEG ΔE76を1.62/1.31に下げる一方、source相対ΔE76は23.12/23.27、mean-vector C*abはsource 0.67/2.72から出力1.93/0.45へ動いた。カメラJPEGとの近さと白の保持は同じ尺度ではない。
- **価値または懸念:** RAW render/JPEG pairにはWB以外の大きな露出・現像差が含まれる。ターゲット誤差を下げても入力の見た目は大きく変わり得るため、R&Dでは両距離を併記し、参照の由来を明確にする必要がある。ROI自体も未測色で、低chromaの色相は安定した保持指標にならない。
- **次に確認すべきこと:** 露出/現像差を統制した同一カメラ・同一露出の実撮影WB対と測色済みニュートラル面を用い、source相対変化と目標一致の両方に対する受入範囲を独立レビュー前に定める。
- **未検証:** 真のニュートラル度、実撮影別WBペアへの一般化、Core出力、独立盲検、性能、製品経路。
## 2026-11-16: 肌スペクトル資料は画像間評価の代替にならない
- **関連:** ISSA paper/figshare DOI、`experiments/ColorMatchLab/EVALUATION_PLAN.md`、ColorMatch R&D milestone。
- **確認できた事実:** ISSA論文のデータ説明は2,113人由来の15,256件の皮膚スペクトル/測色値と表計算datasheetを記載するが、顔画像ペアや空間ROI付き画像は説明していない。Figshare item-level licenseは直接確認できず、データは取得していない。
- **価値または懸念:** 多様な肌色の測色レンジや照明別色計算の背景資料にはなり得るが、画像合わせの局所保持・質感・照明応答を評価できない。論文のCC BY表示をデータ本体の利用許諾とみなさない。
- **次に確認すべきこと:** item-level licenseと利用条件を確認し、明示的に許された場合のみ色彩参照用候補として扱う。画像品質評価には引き続き権利確認済みの実撮影paired portraitとROIが必要。
- **未検証:** データファイルの内容・ライセンス・測色値再計算・ColorMatchへの適用可能性。
## 2026-11-16: DASTはWBペアでなく肌露出ストレス素材候補
- **関連:** DAST公式README、`experiments/ColorMatchLab/EVALUATION_PLAN.md`のDAST計画。
- **確認できた事実:** 公式READMEは80人・960枚のsRGB TIFF顔画像、2カメラ、人物別12撮影、白カードWB校正、露出ブラケット、5測色部位を説明し、フルデータは申請制で商用利用可能と記載する。公開example archiveの個別条件は確認しておらず、データは未取得。
- **価値または懸念:** 同一人・同一カメラの肌ROIで露出差に対するglobal matchingの応答と測色部位を調べられる可能性がある。一方、露出ブラケットは色温度WBペアでもpixel GTでもなく、照明/カメラtone曲線も残る。ニュートラルカードが各画像に見えなければ白保持は評価できない。
- **次に確認すべきこと:** example/fullデータの利用条件と評価用使用許諾を確認してから取得可否を判断し、候補出力を見る前にsame-person/camera/EL3基準、ROI、指標を固定する。
- **未検証:** 公開例の具体的素材構成、配布物のライセンス、局所ROIの対応と測色位置、matcherのCore出力・品質・一般化。
## 2026-11-16: ColorMatchのデータ受入は派生成果物の権利も個別確認する
- **関連:** `experiments/ColorMatchLab/EVALUATION_PLAN.md`素材適合性マトリクス、ColorMatch R&D milestone。
- **確認できた事実:** 利用条件が比較的明確な素材も、その範囲/制約が別々。Middleburyは画像利用と公開を引用付きで許可、CC0 portraitは実写人物素材だがcast referenceは合成、DASTは申請で学術・商用利用可能と記載する一方、example archive個別条件は未確認、ISSA論文とdataset itemのtermsも別確認が必要。
- **価値または懸念:** 元画像の取得許可とcrop・ROI mask・派生出力・メトリクスの保存/共有許可を一括推定すると再現用evidenceの保持が曖昧になる。利用許可と測色上のground truth性、pair task適合は独立軸としてゲート化すべき。
- **次に確認すべきこと:** データ提供者への問い合わせ前に必要な利用目的・社内保管・共有・保持期間を具体化し、回答/契約を試料manifestへリンクする。現状、外部連絡はしていない。
- **未検証:** DAST example/full dataの個別利用条件、ISSA item-level license、派生物保持権利、アプリ組込み用素材の適合。
## 2026-11-16: DAST評価では契約条件と測色スポット対応を分けて扱う
- **関連:** DAST README、[participant consent form](https://dasec.h-da.de/wp-content/uploads/2025/06/DAST-HDA-consent-form-250401.pdf)、2026-04-23 acquisition presentation、ColorMatch DAST evaluation plan。
- **確認できた事実:** READMEはDAST-EXがHochschule Darmstadtとのdataset license agreementで研究者へ提供されると記し、参加者同意書は利用範囲を同契約で規定する。presentationはDSM-4が約50 mm²を測り、DAST取得を単一sessionの制約ありと説明する。顔画像ROIと測色器spotのpixel座標対応の具体的仕様は公開説明で確認できなかった。
- **価値または懸念:** 利用可能性の一般表記だけではimage crop・rendered candidate・mask・metricsの製品R&D利用/保持を確定できない。またinstrument Labと広い画像ROIの色差はspatial matchがなければpixel-ground-truthにならず、単一sessionは一般化の限界になる。
- **次に確認すべきこと:** 実データ取得前に契約文で用途と派生成果物の保持/共有条件を確認し、測色点の位置仕様または不確かさを取得する。候補画像を開く前にROIを固定する。
- **未検証:** example/full archive個別条件、測色点から画像座標への対応、候補素材の色/露出実測、matcherのCore出力。
## 2026-11-16: 多照明顔画像は全体色合わせの局所GTにならない
- **関連:** PolyU Face Color Constancy Dataset (CIC 2025)、`experiments/ColorMatchLab/EVALUATION_PLAN.md`。
- **確認できた事実:** CIC論文は8人・1,299画像の前景/背景dual-illuminant顔シーンを説明し、ColorChecker撮影から照明条件を定義する。paper本文でpixel-wise illuminant mapではなくforeground/background条件を提供すると区別している。dataset download URL/licenseは論文に見つからない。
- **価値または懸念:** 同一場面内の照明が異なる顔・背景をストレス素材にできる可能性があるが、`AutoColorMatcher`はグローバル変換なので二つの領域に同時に一つの物理的正解を当てられるとは限らない。照明条件をpixel ground truthや修正済み参照画像と混同しない。
- **次に確認すべきこと:** 提供者許諾後、ColorChecker設定とportraitの対応を検証し、source-only face/background ROIを手動注釈する。局所GTがない場合は参照誤差を設定せず、同時変化を限定的なストレス観察として記録する。
- **未検証:** 配布/利用条件、画像と色票の対応、フレーム別データ、ROI mask、AutoColorMatcherとの採点妥当性。
## 2026-11-16: 全画面統計の連番処理はフレーム構図変化に敏感な可能性
- **関連:** `ArtifactCore/src/Color/AutoColorMatch.cpp`の全source/reference画素統計、`experiments/ColorMatchLab/EVALUATION_PLAN.md`必須評価項目監査。
- **確認できた事実:** Reinhard/MeanStddev/Histogramは入力1枚ごとの全画素分布から変換を作る実装で、同一画面の各フレームを独立処理するAPI利用を仮定すると、frameごとに統計母集団が変化する余地がある。
- **価値または懸念:** 連番で物体の出入り/構図が変わると固定物体の出力色まで揺れる可能性がある。静止画のsource/reference評価から時間安定性を外挿できない。
- **次に確認すべきこと:** 動画/連番適用が明示スコープになる場合のみ、固定露出/照明の同期sequenceで静止ROIのframe-to-frame ΔEとglobal statistic driftを測る。単一画像R&Dでは時間安定性を対象外と記録する。
- **未検証:** Matcherが実際にsequenceで呼ばれるか、frame stats driftの見えやすさ、変動が欠陥か意図した適応か、許容値。
## 2026-11-16: 公開AWBデータでも製品R&Dの肌ペア条件を満たさない
- **関連:** `docs/planned/MILESTONE_COLOR_MATCH_SKIN_WHITE_RND_2026-11-16.md` の追加dataset調査。
- **確認できた事実:** SamsungLabs S24 Raw-sRGBはRAWと複数sRGB rendition、neutral/preference illuminantラベルを説明するが、公開repoのLICENSE.mdはCC BY-NC-SA 4.0で、dataset READMEは顔等の個人情報をぼかすと記載する。READMEは別個の画像データライセンスを示していない。Color Constancy Using Faces論文は472人のカード有/無RAWペアと顔ラベルを説明する一方、アクセスできた資料からダウンロード条件・ライセンスは確認できなかった。
- **価値または懸念:** AWB研究に有用なデータであっても、そのまま製品R&Dに再利用できるとは限らず、ぼかし済みデータは肌色評価に向かない。既存候補は権利・ROI・タスク適合性を別々に確認する必要がある。
- **次に確認すべきこと:** 明示的に商用利用できる権利を確認した実撮影paired portraitと実中立ROIを探すか、利用許諾と同意を記録できる社内撮影セットを作る。
- **未検証:** S24データの独立したdataset license、特定画像の内容、顔ぼかし前の元データ利用許諾、Faces datasetの配布・権利状態。
## 2026-11-15: UV mapping and image resources should remain graph-authored
- **関連:** `experiments/ShaderGraphLab/registry.json`、`dynamic_graph_prototype.py`、`local_graph_lab.py`、`local_graph_lab.html`、`dynamic_generic_image_mapping.json`。
- **確認できた事実:** 閉じた scale/offset 演算を固定 Base Color sample と schema-v4 generic Image Texture adapter の両方に接続する候補がある。mapped/direct graph はresource requirementsとresolved binding keyを保ちつつ、semantic/HLSL source identityを変える。local editorで開けるschema-v1例はUV0→Mapping→Image Textureを保持するが、resourceは未解決である。Image Texture node向けの編集UIはnode参照、document resource tableのAsset UUID、sampler、色解釈を単一undoable actionで保存する。optional `AssetDatabase::toJson()` snapshot adapter はImage型だけを名前とUUIDで表示し、パスを捨ててsnapshot hashを示す。これは静的な選択肢で、live AssetDatabase接続、revision解決、画像load、GPU bindingは行わない。
- **価値または懸念:** UV変換とtexture参照の authored state を接続可能なshader graphへ置き、resource identityとshader semanticsを分離できる。production material path、Asset picker、回転・wrap・GPU動作は未検証。
- **次に確認すべきこと:** resource編集/clear/undo/redo/save/reloadをaction契約として検証可能になった後に確認する。production resource adapter接続後はchannel-probe direct-UV baselineを5回以上取得し、候補画像を見る前に領域別許容差を固定してからMapping・runtime/backend/性能ゲートを実施する。
## 2026-11-15: Shader graph retains documents when image bindings disappear
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::validate_document_for_editing`、`tools/shader_graph_rd/validate_shader_graph_manifest.py`、`tests/fixtures/shader-graph-rd/evaluation_manifest.json`。
- **確認できた事実:** Image Texture の active resource が host binding に解決できない場合、編集用診断で graph document を保持したまま `resource.binding.unresolved` warning を resource key ごとに出し、status を `repairable`、`compilationAvailable` を false にする契約を追加した。resolved binding の供給時は診断なしで `valid` になる source assertion も追加した。
- **価値または懸念:** Asset が削除・未ロードでも編集状態を保ち、未解決 binding をコンパイル可能と誤認しない。production Asset resolver/runtime bindingとの接続は未実装であり、assertionは未実行。
- **次に確認すべきこと:** ビルド／テスト実行が明示許可された後、manifest validatorでmissing/resolved両状態と既存resource lifecycle契約を確認する。
## 2026-11-15: Shader graph R&D should add scalar subtraction as a closed operation
- **関連:** `experiments/ShaderGraphLab/registry.json`、`dynamic_graph_prototype.py`、dynamic graph capability manifest/candidate。
- **確認できた事実:** prototypeのtrusted implementation setとtyped arithmetic loweringはregistryから任意コードを実行せず、既知IDの演算子だけを選ぶ設計である。Scalar `float.subtract` を閉じた実装IDと `artifact.rd.math.subtract.float32@1` operationとして追加し、必須Float `a/b`→Float `result` signatureを既存validatorへ含めた。式は`a - b`でclampしない。root capability listとregistry digestを更新し、既存Add/Mul fixtureに加え`0.75 - 0.25`のfocused document/candidateをgeneratorで作成した。候補HLSLは`(n_minuend_value - n_subtrahend_value)`を出力する。
- **価値または懸念:** prototypeのpalette/loweringに基本scalar arithmeticをひとつ増やし、registry-selected operationとtrusted native implementationの境界を保つ。shader runtime/finite arithmetic結果、Subtract専用graphの実行証拠はまだない。
- **次に確認すべきこと:** manifest verifierにfocused subtract candidateの再生成・payload・digest照合を追加した。許可された検証時にregistry lock/negative signature、将来HLSL compiler/runtimeとbinary32 edge behaviorを評価する。
## 2026-11-15: Shader graph action envelopes should validate diagnostics before display
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html::isStateEnvelope/isActionEnvelope/diagnosticText`。
- **確認できた事実:** state envelopeはvalidation/compilation diagnostics配列の存在だけを確認し、各itemの型を検証していなかった。action responseは`ok` boolの必須性を別契約にせず、reject時にdiagnosticsへ直接`.map`していた。診断itemのcode/severity/path/messageを検査し、action用envelopeの`ok` boolと安全なstatus formatterを追加した。
- **価値または懸念:** malformed diagnosticsがrender/status表示経路で例外になる可能性を下げる。JSON stateのbrowser runtime検証は未実施。
- **次に確認すべきこと:** 正常warning/error arrays、malformed item、missing action `ok`、empty reject diagnosticsをUIへ渡してstate回復とstatus表示を確認する。
## 2026-11-15: Shader graph UI should validate the full state envelope before adoption
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html::isStateEnvelope/refreshState`。
- **確認できた事実:** UI state採用前のguardは`document`と`palette`だけを確認していたが、renderはvalidation/compilation diagnostics、history capabilities、document hash、dirty/save statusも必須として参照する。必要フィールドを型付きで確認するguardへ拡張し、初期state取得も同じrefresh関数経由にした。
- **価値または懸念:** incomplete payloadがstateへ採用されてrender中に失敗する事態を避け、初期ロードと通信回復の契約を統一する。browser runtimeでの正常state採用・不完全payload拒否は未確認。
- **次に確認すべきこと:** 現serverの正常/compile unavailable stateと各required field欠落payloadを使い、render動作およびstate保持をbrowserで確認する。
## 2026-11-15: Shader graph parameter APIs should reject malformed IDs at ingress
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::expose_node_parameter/edit_graph_parameter/unexpose_node_parameter/edit_node_parameters`。
- **確認できた事実:** APIは不正なnode/parameter IDをlookup失敗へ進めた後、JSON Pointer用 `_pointer_segment` でstring methodを呼ぶ箇所があり、HTTP payload由来のlist/dict IDがstructured diagnosticではなくAttributeErrorになり得た。Expose/Detachのnode IDとnode parameter ID、shared editのgraph parameter ID、node parameter update keyを既存stable identifier形式で入口検証するよう変更した。
- **価値または懸念:** malformed actionでもserverの通常ContractError経路とUIのresponse envelopeを維持しやすくなる。API runtime応答は未確認。
- **次に確認すべきこと:** object/array/bool/不正文字列IDを各APIに渡し、ContractError code/pathとdocument不変性を確認する。
## 2026-11-15: Shader graph editor should resync after an ambiguous action response
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html::postRequest/refreshState`。
- **確認できた事実:** action fetchまたはresponse JSON parseが失敗すると、serverがactionを処理済みか不明でもUIは旧stateを維持したままqueueを進める。incomplete envelopeも同様にstate更新をせず次操作へ進んでいた。失敗・不完全応答では次actionの前に`/api/state`を取得し、正しいenvelopeならstateを置き換えてrenderするよう変更した。再同期失敗時は最後のUI stateを維持し、失敗をstatusへ表示する。
- **価値または懸念:** 通信結果が曖昧な後に旧document hashを根拠としてactionを継続する可能性を減らす。serverが応答不能の間は同期できず、後続操作はserver validationによりrejectされる可能性が残る。ブラウザ通信障害は未実験。
- **次に確認すべきこと:** action commit後のresponse切断、invalid JSON response、state route失敗を模擬し、再同期後のundo/redoとdirty stateを確認する。
## 2026-11-15: Shader graph Expose must validate existing shared bindings before joining
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::expose_node_parameter`。
- **確認できた事実:** 既存graph parameterへnode parameterを追加する操作は対象bindingの形状のみ検査し、既存bindingのnode/operation/parameter/type/value整合や同record内重複は検証せずappendしていた。既存targetに限ってbinding一意性、node一意性、operation/parameter availability、型、canonical value一致を確認してから追加するよう変更した。node ID→record indexも事前構築して既存bindingごとの全node検索を避ける。無関係なrepairable graph parameter recordは走査時に編集拒否の理由としない。
- **価値または懸念:** 共有parameterの既存意味が壊れている状態へ新しいbindingを連結して問題を広げることを防ぐ。実動作・原本不変性は未検証。
- **次に確認すべきこと:** validな共有target、target内重複、missing/unknown bound node、type/value mismatch、無関係malformed recordを使い、成功/rejectと入力不変性を確認する。
## 2026-11-15: Shader graph parameter edits must reject duplicate bindings before mutation
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::edit_graph_parameter`。
- **確認できた事実:** shared value editは対象parameter内の各bindingを同期するが、同じnode parameterが同じrecord内で重複、または別recordにも登録されていても書き換えを進める経路があった。編集前にnode ID→node recordとwell-formed binding→graph parameter indexを一度索引化し、対象binding内の重複と他recordとの重複をrejectする。不正な無関係bindingは索引対象外にしてrepair可能性を保つ。
- **価値または懸念:** 無効なmultiple-writer parameter relationを値編集でさらに更新するのを防ぎ、照合用のnodeとbinding scanをO(N+B+P)へまとめる。API runtime結果は未確認。
- **次に確認すべきこと:** 有効shared edit、同record内重複、別record重複、無関係malformed recordを含む操作で、reject時に入力documentが不変であることを確認する。
## 2026-11-15: Shader graph repair validation should include graph parameter bindings
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::validate_document_for_editing`。
- **確認できた事実:** validatorはnode/link診断を集める一方、graphParametersの厳密検証はエラーがない場合に限って呼ぶcompiler経路へ委ねていた。そのため独立したnode/linkエラーが存在すると、duplicate graph parameter ID、binding欠落/重複、node parameter type/value mismatch等が同時に見えない。編集validatorへgraph parameter独立走査を追加し、既存エラーの有無にかかわらず診断を集め、registryにない操作へ結び付いたbindingはrepairable warningにした。node parameter canonicali
## 2026-11-15: Shader graph node drag should coalesce wire redraws by frame
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html::renderNodes/drawWires`。
- **確認できた事実:** ノード移動の各 `pointermove` が全wireの削除・socket座標読取・SVG path再生成を同期実行していた。また、canvas scrollとwindow resi
## 2026-11-15: Shader graph arrange selection lookup can use a set
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::arrange_nodes_by_flow`。
- **確認できた事実:** arrange は重複のない文字列 node ID を検証した後、全nodeを走査する選択照合で `selected_ids` list membership を使っていた。選択数をS、node数をNとするとこの照合は最大O(N×S)回の文字列比較になり、選択ID set を事前生成する形へ変更した。
- **価値または懸念:** 大きな部分選択グラフでレイアウト操作の選択判定を平均O(N+S)へ抑えられる見込み。実測しておらず、全体のarrange時間が改善するかは未検証。
- **次に確認すべきこと:** 将来許可された性能計測で、全node選択と小部分選択を含む大規模fixtureのarrange時間を比較し、選択set構築コストも含めて測る。
## 2026-11-15: Shader graph link batches should snapshot the document once
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::connect_nodes_batch`。
- **確認できた事実:** batch は各 `connect_nodes` 呼び出しでdocument全体のJSON snapshotに加え、既存nodes/links、operation/socket定義、writer set、adjacencyを再構築していた。detached working copy、node/operation/socket index、writer set、adjacencyを一度だけ作り、edgeが通るたびwriter/adjacencyを更新する形に変更した。edgeごとの接続型検査とcycle searchは残る。後続edgeが失敗すればworking copyは返らずcaller documentは元のまま。`rewire_input` も削除後のdetached copyを直接indexして検証するよう変更し、再snapshotを省いた。
- **価値または懸念:** batch全体のnode/link/registry scanを一度にし、document-wide snapshotも1回にできる。cycle searchは各edgeで残るため全処理が線形という主張はしない。性能とsingle-link behavior同値性は未計測・未実行。
- **次に確認すべきこと:** 許可された性能計測でbatch長・graph規模を変えてwall timeを測り、single-link、batch success/failure、duplicate writer、cycle rejectionの動作同値を確認する。
## 2026-11-15: Shader graph disconnect batches can index existing edges once
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::disconnect_nodes_batch`。
- **確認できた事実:** 各要求edgeについてlinks全体をfilterし直していたため、B件を外す処理の照合はO(B×E)だった。string endpointだけを exact edge tuple→link index list に一度索引化し、要求は索引から照合して、事前検証後に一括除去するよう変更した。
- **価値または懸念:** 照合部分は平均O(E+B)となり、リンク数の多い編集で重複走査を減らせる。JSON snapshotと最終除去のコストは残り、wall-time改善は未計測。
- **次に確認すべきこと:** 許可された性能計測で、リンク数・削除数・重複edge数を変えたときのindex構築とbatch全体時間を測る。
## 2026-11-15: Shader graph duplicate ID checks should remain linear
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::duplicate_nodes_batch`。
- **確認できた事実:** 複製先IDの重複確認に毎回 `source_to_clone.values()` を走査しており、複製数Sに対してO(S²)比較になっていた。専用setへ追加して平均O(1)照合に変更した。graph parameter bindingの重複抑止setも各record単位からbatch全体へ移し、1つのnode parameterを複数recordから複製bindingしないようにした。
- **価値または懸念:** 大きな複製batchのID validationを線形にでき、壊れた入力で同一node parameterのgraph-scope二重bindingを新規複製しない。改善量と既存 malformed duplicate binding の選択規則はruntime未検証。
- **次に確認すべきこと:** 許可されたfocused確認で、重複dest ID、recordを跨ぐduplicate parameter binding、large selectionを確認し、既存bindingは保持しつつclone側だけに重複を増やさないことを照合する。
## 2026-11-15: Acyclic shader graph batches can skip forward-edge reachability walks
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::_connection_validation_state`、`_topological_ranks`、`_connect_with_state`。
- **確認できた事実:** batch edgeのcycle判定は各edgeでtargetから全reachable adjacencyをwalkしていた。複数edgeのbatchで既存graphがDAGならdeterministic topological rankを一度作り、source rank < target rankなら追加edgeはcycleを作れないためwalkを省略する実装にした。rank逆転edgeのみ既存DFSを使い、通過した後rankを再構築する。1-edge batchは初期rank構築を避け、初期graphがcyclicならrankは作られず既存DFS経路を保つ。
- **価値または懸念:** 一般的な前方向batch edgeはcycle探索を不要にできる。逆転edgeごとのrank rebuildはO(V+E log V)、topology構築コストもあるため、batch全体の高速化は測定していない。
- **次に確認すべきこと:** 許可後、forward edge、non-cyclic backward edge、cycle-forming edge、既存cyclic graph、single-edge APIを比較し、判定同値性とlarge batch性能を測る。
## 2026-11-15: Shader graph socket hints should share one adjacency snapshot
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html::updateSocketHints`、`wouldCreateCycle`。
- **確認できた事実:** 以前は互換socket候補ごとに `wouldCreateCycle` が全link配列を各訪問nodeで走査していた。hint更新ごとに有効string endpointから adjacency mapとinput別link indexを一度作り、候補node pairごとのDFS結果を同更新中にcacheするよう変更した。socket click確定経路にも新hint stateを渡すよう揃え、旧signatureの呼び出しが残っていた例外経路を除去した。追加レビューで、UI hintはsingle-writer inputもblockedにしていたがclick確定はrewire可能だった不一致を確認し、単一既存edgeを除外したcycle判定で候補表示と確定を一致させた。multi-writerだけをblockedとする。さらにhint passのbuttonごとに node配列とpaletteを再検索していたため、node/operation indexを同一snapshotへ追加した。
- **価値または懸念:** hint対象socket数をC、link数をE、reachability訪問量をRとすると、旧処理は候補あたり最大O(R×E) link比較に加えoccupied check O(E)、socket definitionごとにnode/palette lookupがあった。新処理はO(N+P+E)のindex構築とnode-pairごとのO(R)探索で、同一pairのsocket候補は結果を再利用し、buttonごとのlookupは平均O(1)にする。single-writer rewireでは対象input edgeだけを除外し、同じnode-pairを結ぶ他のinput edgeが残る場合はadjacencyを保持する。browser実測・描画負荷検証は未実施。
- **次に確認すべきこと:** 大きなgraphでsocket候補の多いnodeを選び、hint更新時間と結果正確性をruntime/browserで確認する。サイクル・孤立node・壊れたendpointを含む画面ケースも見る。
## 2026-11-15: Shader graph diagnostics should share node and link indexes
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html::render` diagnostic list builder。
- **確認できた事実:** diagnostic itemごとに `/nodes`, `/links`, output pathを解決する際、`documentNodes()` / `documentLinks()` を再取得し、link endpointごとのfindとnode IDごとのduplicate filterも再走査していた。diagnostic render passでnode array、link array、string node ID→matches mapを一度構築し、unique valid IDだけをaction対象にするよう変更した。
- **価値または懸念:** node数N、link数E、diagnostic数Dに対し、対象解決にあったおおむねO(D×(N+E))の再走査をO(N+E+D)にまとめる。診断表示・action target同値性はsource-reviewedだがbrowser未検証。
- **次に確認すべきこと:** `/nodes/i`、`/links/i/from|to`、`/output`、duplicate/invalid IDを含むdiagnosticのボタン有効状態と遷移先をbrowserで比較する。
## 2026-11-15: Shader graph parameter rendering should index exposed bindings
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html::renderNodes`。
- **確認できた事実:** node parameterごとにgraph parameter arrayを走査し、全bindingを `.find(...some(...))` で再評価していた。render passで先にnode identity + parameter ID→最初に一致したexposureを索引化し、元のrecord順優先を保持してlookupするようにした。JSON scalar node identityの編集不能な修復表示も元のstrict equality意味を保つようstring/number/boolean/nullだけを索引対象にし、object identityをJSON文字列化で同一視しない。
- **価値または懸念:** node parameter数Q、graph parameter record数G、binding数Bに対する概ねO(Q×(G+B))検索をO(G+B+Q)にする。表示値・最初の一致規則はsource上同値だが、UI runtime未確認。
- **次に確認すべきこと:** 複数exposure重複、scalar malformed node ID、通常shared Float/Vector4値の表示とdisabled editorをbrowserで確認する。
## 2026-11-15: Vector4 divide candidate maps the entire legacy epsilon probe
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::MathNode::Op::Divide`、`tests/fixtures/shader-graph-rd/math_divide.json`、`tests/fixtures/shader-graph-rd/math_operation_formula_golden.json`、`experiments/ShaderGraphLab/dynamic_vector4_divide.json`。
- **確認できた事実:** The existing four-lane divide probe uses B values `0`, `-2e-5`, `2e-5`, and `-2`, with source-bound finite binary32 outputs `100000`, `100000`, `50000`, and `100000`. The dynamic prototype now has a versioned Vector4 operation emitting component-wise division by `max(B, float4(1e-5f,...))`, and its validator contract maps the fixture arrays to all four golden lanes.
- **価値または懸念:** This gives the R&D typed Vector4 path direct correspondence to the vectori
## 2026-11-15: Vector4 divide candidate maps the entire legacy epsilon probe
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::MathNode::Op::Divide`、`tests/fixtures/shader-graph-rd/math_divide.json`、`tests/fixtures/shader-graph-rd/math_operation_formula_golden.json`、`experiments/ShaderGraphLab/dynamic_vector4_divide.json`。
- **確認できた事実:** The existing four-lane divide probe uses B values `0`, `-2e-5`, `2e-5`, and `-2`, with source-bound finite binary32 outputs `100000`, `100000`, `50000`, and `100000`. The dynamic prototype now has a versioned Vector4 operation emitting component-wise division by `max(B, float4(1e-5f,...))`, and its validator contract maps the fixture arrays to all four golden lanes.
- **価値または懸念:** This gives the R&D typed Vector4 path direct correspondence to the vectori
## 2026-11-15: Texture slot semantics and asset binding should remain separate
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py::TEXTURE_SAMPLE_SLOTS`、`tests/fixtures/shader-graph-rd/evaluation_manifest.json::textureSampleResourceContract`、Artifact material texture slots。
- **確認できた事実:** The dynamic prototype identifies the sample operation by a fixed material slot and emits a symbolic resource key. The renderer-facing resource handle, asset identity/revision, sampler settings, UV transform, and missing-resource behavior are not represented by this candidate. The legacy source audit maps each material slot to a resource symbol but uses the same sampler symbol.
- **価値または懸念:** Keeping shader operation semantics separate from runtime asset identity should allow asset replacement/revision to invalidate resource bindings without changing shader structure. A fixed slot operation is useful for source characteri
## 2026-11-15: Shader texture asset identity and renderer cache identity currently diverge
- **関連:** `ArtifactCore/include/Asset/AssetDatabase.ixx::AssetInfo`、`Artifact/include/Project/ArtifactProjectItems.ixx::FootageItem`、`ArtifactCore/include/Utils/AssetManager.ixx::sourceVersion`、`ArtifactCore/src/Graphics/MeshRenderer.cpp::setMaterialRanges`。
- **確認できた事実:** AssetDatabase exposes a logical QUuid and resolved path, FootageItem persists assetId and filePath separately, and AssetManager tracks an in-memory source version. The inspected MeshRenderer material-slot cache compares texture paths and reuses SRVs by matching paths; generic graph resource-key binding is absent. The dynamic graph R&D now prototypes an external resource-binding key without including runtime asset metadata in shader source/IR structure identity.
- **価値または懸念:** Core shader graph can preserve stable project identity while the renderer updates texture resources independently, but a path-keyed renderer cache may not observe every asset-ID relink or source-revision change unless the adapter explicitly invalidates/rebinds the slot. Putting asset paths directly into graph shader source would couple relocation to recompilation.
- **次に確認すべきこと:** With child-repo implementation explicitly in scope, trace asset relink and source invalidation through material-slot SRB updates, then define a binding adapter keyed by assetId + sourceVersion + sampler/UV/color interpretation. Keep this separated from shader compile and PSO keys unless a binding choice speciali
## 2026-11-15: Shader graph Image Texture needs a persisted resource reference per node
- **関連:** `tests/fixtures/shader-graph-rd/generic_image_resource_reference_proposal.json`、`experiments/ShaderGraphLab/dynamic_graph_prototype.py`、`ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx`。
- **確認できた事実:** The current R&D registry exposes six fixed material-slot Image Texture operations. Its host binding projection can hash an Asset UUID and runtime settings but receives them outside the document. The existing Core `ImageTextureNode` likewise selects one of six slots and explicitly excludes custom file-backed slots from its phase-one scope. The R&D Core promotion proposal already describes resource handles/dependencies but has no persisted node-to-image selection contract. An isolated schema-v4 prototype maps per-node `resourceId` references to project Asset UUIDs for two independently sampled images feeding a Vector4 Add. Its logical binding projection hashes Asset UUID/type, source revision, sampler state, and color interpretation; mutation projections keep symbolic requirement hash stable and change binding identity for each field. A source comparison also confirms this v4 compiler is separate from the data-driven registry compiler: the latter started with six fixed material-slot texture operations and external binding inputs, while v4 stores sampler/color on each image node and UV through graph links. The R&D registry now adds `texture.sample.image2d`; its adapter persists node `resourceReference` fields and the document resourceId-to-Asset-UUID table, with compiler checks that host bindings match both. `edit_node_resource_reference` and `set_graph_resource_asset` provide separate atomic paths for changing node selection and relinking the resource table. The candidate points two nodes at one logical resource ID while keeping node-scoped keys and shader symbols distinct. Clipboard schema v4 carries referenced table rows and rejects conflicting same-ID mappings. Source-level manifest contracts describe JSON round-trip, command undo/redo, clipboard/duplicate key remapping, stale binding rejection, and shader-source versus binding identity. Candidate generation confirms the adapter compiles through the dynamic compiler; the manifest persistence checks have not been executed.
- **価値または懸念:** A node-level resource reference plus a document resource table would allow arbitrary image datablocks and intentional sharing without embedding paths or GPU objects in Core. Persisted Asset UUID, symbolic shader resource usage, and resolved binding state need distinct identities; otherwise relinking either recompiles unnecessarily or silently leaves stale GPU bindings. The current equality probe only proves the resource-requirement projection is stable, not that a production compiler's shader/PSO keys behave correctly.
- **次に確認すべきこと:** Run the manifest persistence contracts when test/validator execution is authori
## 2026-11-15: Vector3 needs a first-class typed path before Principled can be promoted
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py`、`tests/fixtures/shader-graph-rd/typed_ir_vector4_to_vector3_expression.json`、`tests/fixtures/shader-graph-rd/typed_ir_projection_readiness.json`。
- **確認できた事実:** The dynamic registry compiler currently accepts Float/Vector2/Vector4 sockets, Float/Vector4 socket defaults, and Float32/Vector4F32 graph parameters. Its HLSL root type map has no Vector3. Separately, the typed-IR fixture corpus already proposes Vector4→Vector3 `truncate_xy
## 2026-11-15: Vector3 graph-parameter local editor path was still node-only
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html`, `tools/shader_graph_rd/validate_shader_graph_manifest.py`, `docs/planned/MILESTONE_SHADER_GRAPH_RND_2026-11-13.md`.
- **確認できた事実:** Graph parameter schema, lowering, Vector3 candidate ABI, and three-lane graph-parameter value editor existed, but the node row rendered Vector3 as a permanent node-only label. The existing-parameter picker also validated every non-Vector4 parameter as a scalar, so it rejected Vector3 records.
- **変更した事実:** Vector3 node rows now use the standard Expose/Unexpose control, and the picker validates a three-lane finite value for Vector3. The validator source contract now declares the UI conditions plus a Graph Lab session sequence covering shared bindings, three-lane edits, detach, final-binding cleanup, and undo/redo. A stale validator expectation that Vector3 exposure must fail was replaced with the now-supported behavior.
- **価値または懸念:** The editor surface and declared session coverage now match the source candidate contract. The declared session sequence has not been executed; browser behavior, cbuffer reflection, and host upload remain unverified.
- **次に確認すべきこと:** Under the authori
## 2026-11-15: Vector3 socket-default UI needed an explicit persistence/undo gate
- **関連:** `experiments/ShaderGraphLab/dynamic_optional_vector3_default.json`、`tools/shader_graph_rd/validate_shader_graph_manifest.py`、`experiments/ShaderGraphLab/local_graph_lab.html`。
- **確認できた事実:** The browser renderer already maps array defaults to lane editors, and the host action routes `socketDefault` to `edit_node_socket_default`; source coverage had pinned Float and Vector4 but not Vector3 lane edits. A schema-v3 Vector3 fixture now pins a non
## 2026-11-15: Dynamic graph validator had orphaned diagnostic goldens
- **関連:** `tools/shader_graph_rd/validate_shader_graph_manifest.py::checks/diagnostic_probes`、`tests/fixtures/shader-graph-rd/evaluation_manifest.json::dynamicGraphPrototype.contractChecks`。
- **確認できた事実:** Static AST comparison showed 11 diagnostic case IDs in the manifest's ordered `contractChecks` and `diagnosticGoldens` but absent from the validator's `checks` dictionary, making the final exact-list comparison fail. The validator now maps each named probe result to its pinned diagnostic golden as an explicit boolean check. A static order audit confirms all 140 check names match; the Vector3 fixture and candidate SHA-256 values also match the manifest.
- **価値または懸念:** The manifest describes evidence only when the validator actually asserts it. Orphaned case names can otherwise make an apparently complete matrix fail closed or leave the validator permanently inconsistent.
- **次に確認すべきこと:** Run the manifest validator when authori
## 2026-11-15: Vector3 operation substitution needs typed-link history coverage
- **関連:** `experiments/ShaderGraphLab/registry.json` Vector3 Add/Subtract/Multiply、`dynamic_graph_prototype.py::replace_node_operation`、`local_graph_lab.py` replace/undo/redo actions。
- **確認できた事実:** Vector3 Subtract and Multiply declare the same required Vector3 input IDs and Vector3 output ID, so the prototype's compatibility rule permits substitution while retaining existing links. Validator source now describes replacing the middle operation in a persisted arithmetic graph, checking typed IR/HLSL, then undoing and redoing through both document commands and the local Graph Lab session.
- **価値または懸念:** A node editor that supports operation replacement must preserve graph topology and history across compatible typed nodes; only the Float route had an explicit history contract before this addition.
- **次に確認すべきこと:** Execute the manifest validator when authori
## 2026-11-15: Resource-limit corpus generation must itself be stack-safe
- **関連:** `tools/shader_graph_rd/generate_document_limit_corpus.py::_candidate_math_depth/validate_case_set`、`validate_shader_graph_manifest.py` のgenerator sample。
- **確認できた事実:** The corpus generator recursively walked output ancestors for depth and recursively visited all nodes for cycle detection. A sufficiently deep probe could therefore fail in the Python generator before testing the target implementation. Both now use iterative topological ordering; depth propagation follows that order. A 12-case corpus with depth limit 1600 generated and passed the generator's internal consistency validation, and the manifest validator's generator sample is raised to depth 2048.
- **価値または懸念:** A stress harness that has the same recursion failure mode as the target can mask the target's actual threshold and produce misleading resource evidence.
- **次に確認すべきこと:** When authori
## 2026-11-15: Resource-limit measurement records must separate stages and phases
- **関連:** `tools/shader_graph_rd/generate_document_limit_corpus.py::measurementRecordFields`、manifest `measurements.resourceBounds.measurementProtocol`。
- **確認できた事実:** The protocol asked for separate parse/validation/canonicali
## 2026-11-15: Measurement records require stable IDs on generated cases
- **関連:** `tools/shader_graph_rd/generate_document_limit_corpus.py` case generation/validation、manifest `measurementRecordSchema`。
- **確認できた事実:** Schema v2 required a `caseId` in each measurement record while generated boundary case objects had only `limitField` and `boundary`. Cases now have unique stable IDs in `<limitField>.<boundary>` form; generator validation checks format and uniqueness, and the manifest validator source checks the required format and 12 unique sample IDs.
- **価値または懸念:** Without stable case identity, measurements cannot be joined unambiguously to the exact boundary input and effective limits that produced them.
- **次に確認すべきこと:** Regenerate the corpus and confirm record references resolve exactly once; include the case ID and payload hash in any future raw measurement export.
- **未検証:** No runtime measurement record has been produced and no Core performance evidence exists.
## 2026-11-15: Measurement schema semantics need end-to-end equality checks
- **関連:** `generate_document_limit_corpus.py::measurementRecordSemantics`、manifest `measurementRecordSchema.recordSemantics`、validator generator sample。
- **確認できた事実:** The manifest previously checked field names, phase values, and stage duration names but did not mirror all generator semantics. It now pins the complete semantics object and the validator source compares the generated object for exact equality, including null-stage meaning, allocator scope, rejection-stage vocabulary, destination preservation, sample indexing, and warmup.
- **価値または懸念:** Identical field names can still encode incompatible null, timing, or rejection meanings; equality across generator, manifest, and validator keeps future measurements interpretable.
- **次に確認すべきこと:** Run the manifest validator when authori
## 2026-11-15: Raw resource samples need deterministic summaries
- **関連:** `tools/shader_graph_rd/generate_document_limit_corpus.py` measurement records、`tests/fixtures/shader-graph-rd/evaluation_manifest.json` resource-bound protocol。
- **確認できた事実:** 計画は30以上の反復とmedian/p95/p99を要求していたが、分位点方式、集計時のgroup key、null値と不足sampleの扱い、境界accept/reject結果のsummary formatは固定されていなかった。generator/manifestに、nearest-rank集計、run/case/phase/build/platform/instrumentation/effective-limit別集計、nullを0にしない規則、境界契約違反数とdecision statusのsummary schemaを追加し、validator sourceで一致確認するようにした。
- **価値または懸念:** 手動集計の実装差やcold/warm混在による判断ぶれを抑え、異なる環境の結果を誤って同じ母集団として扱わない。仕様のみであり、集計器やCore計測はまだない。
- **次に確認すべきこと:** manifest validator実行の許可後にschema整合を検証し、計測runnerを追加する段階ではraw recordからsummaryを生成する独立ツールと既知値fixtureを用意する。
- **未検証:** 今回は仕様/source更新まで。集計ロジックの実装・実行、実測値の妥当性、production limit選定はいずれも未実施。
## 2026-11-15: Resource measurement summaries need an executable, corpus-bound reducer
- **関連:** `tools/shader_graph_rd/summari
## 2026-11-15: Legacy C++ graph contract candidates must not silently become a green suite
- **関連:** `tests/ArtifactCore/ShaderGraphContractTest.cpp`、manifest `evidence.contractTestLanePlan`、validator test-source inventory check。
- **確認できた事実:** 37件のC++ contract sourceには現行挙動のcharacteri
## 2026-11-15: GrabCut 矩形初期化は細線主体の素材で前景を膨張させる
- **関連:** `experiments/MatteLab/`（rd0）、`ArtifactCore/src/ImageProcessing/OpenCV/OpenCVRotoBrushEngine.cpp`。
- **確認できた事実:** 合成 8 fixture の rd0 で、`cv2.grabCut`（矩形 = 全画面 2% 内側、5 iteration）は `hair_strands` で予測 α 平均 0.480（GT 0.043、SAD 118.38）、`glass_panel` で 0.576（GT 0.105、SAD 123.67）と矩形内を広く前景化した。一方 `disc_aa` / `fur_edge` は SAD 0.28 / 1.16 と良好で、破綻は素材依存。GT trimap を与えても境界帯 MAE は 0.09〜0.33 に留まる。
- **価値または懸念:** 現行 Roto Brush の GrabCut 初期化は「塊状の被写体」を暗黙の前提にしている可能性がある。髪・細線・半透明では初期マスクが膨張し、後段の optical flow 伝播にも誤りが乗る。ただしこれはオフライン合成 fixture の結果で、製品の入力解像度・前処理・ユーザーストロークとは条件が違う。
- **次に確認すべきこと:** 実素材（髪・ガラス・煙）での `grabcut_rect` 再現と、初期化を trimap 入力または学習マットへ置き換えた場合の境界帯 MAE の比較。
- **未検証:** 製品経路（Composition Render Controller 経由）での再現、動画伝播、実素材の GT matte。
## 2026-11-15: 1px alpha ランプは拡大で復元できず、alpha 合成の絶対ゲートが成立しない
- **関連:** `experiments/UpscaleLab/`（rd0）、`experiments/UpscaleLab/gates.json`。
- **確認できた事実:** straight alpha の 1px ランプを持つ fixture を ×2 拡大しただけの bicubic baseline でも、3 背景合成誤差は黒背景 max 60.5 / mean 4.5、×4 では max 83.9 / mean 13.9。事前登録した 2/255 の絶対ゲートは baseline 自身が超過し、候補の識別に使えない。
- **価値または懸念:** alpha を別解像度で拡大する限り、α の幾何精度が合成誤差を支配する。超解像の良否を alpha 合成誤差で測るなら、baseline 比の相対ゲートにするか、α を同じ手法で拡大する前提を明示する必要がある。
- **次に確認すべきこと:** rd1 前に相対ゲート（baseline 比）へ変更するか、α 拡大を候補側の責務から外すかを決める。
- **未検証:** 学習モデルが α を同時に扱う場合の挙動、製品の α 拡大経路。
## 2026-11-15: saliency モデルの SAD 改善は「空マスク」で達成できてしまう
- **関連:** `experiments/MatteLab/`（rd1）、`captures/rd1/metrics.json`、`captures/rd1/gates.json`。
- **確認できた事実:** U2Net（320×320 固定入力、CPU）を 8 fixture で測ると、SAD は 5/8 で `grabcut_rect` 比 40% 以上改善したが、予測 α 平均は `hair_strands` 0.015（GT 0.043）、`thin_wire` 0.011（GT 0.078）、`photo_composite` 0.013（GT 0.346）。前景を取り逃して 0 に寄った結果で、境界帯 MAE は 0.101 / 0.216 / 0.376 に留まり、部分 α 回収は 0.290 / 0.206（ゲート 0.50）。GT の α 平均が小さい fixture ほど「何も出さない」が SAD で有利になる。
- **価値または懸念:** SAD 単独のゲートは saliency/segmentation モデルに対して退化した解（空マスク）を許す。合否は SAD ではなく、境界帯 MAE・部分 α 回収・合成誤差の同時成立で見る必要がある。今回のゲート表はそれを満たしていたため 0/8 と正しく落ちたが、SAD だけを見る要約は誤読を招く。
- **次に確認すべきこと:** レポートや比較表で SAD を出す場合は、予測 α 平均と部分 α 回収率を必ず併記する。可能なら SAD を GT の前景量で正規化する。
- **未検証:** matte 専用モデル（MODNet / BiRefNet / RMBG）で同じ退化が起きるか、製品の Roto Brush で同種の失敗が起きるか。
## 2026-11-15: saliency→trimap→GrabCut 併用はモデルの失敗を継承する
- **関連:** `experiments/MatteLab/`（rd2）、`captures/rd2/metrics.json` の `trimap_stats`。
- **確認できた事実:** U2Net の α から trimap（α≥0.90 を収縮 = 確実前景、α≤0.10 を膨張 = 確実背景）を作って GrabCut に渡すと、確実前景率が十分ある `disc_aa` は SAD 0.94 → 0.34、境界帯 MAE 0.079 → 0.029 と回復し、`fur_edge` も SAD 1.26 → 0.92 になった。一方、確実前景率が 0 / 0.0001 / 0.003 の `hair_strands` / `thin_wire` / `photo_composite` では GrabCut に前景シードが無く、出力はモデル単体とほぼ同じ空マスク（SAD 11.22 / 20.29 / 137.94）。
- **価値または懸念:** 「学習モデル → trimap → 既存 GrabCut」の併用は退行の安全弁にはなるが、モデルが失敗している素材を救わない。確実前景率（または前景面積）が閾値未満なら併用をやめる／矩形初期化へ落とす等のガードが無いと、空マスクをそのまま信頼することになる。
- **次に確認すべきこと:** 前景面積ガードの閾値と、ガード発動時に何を返すか（矩形 GrabCut / 失敗として返す / 手動 trimap 要求）。製品の Roto Brush に学習マットを組み込む場合も同じ分岐が要る。
- **未検証:** matte 専用モデル（MODNet / BiRefNet / RMBG）で確実前景率が同様に 0 になるか、実素材での前景面積分布。
## 2026-11-15: matte 専用モデルは輪郭が閉じた形状で初めて軟らかい α を出した
- **関連:** `experiments/MatteLab/`（rd3）、`captures/rd3/metrics.json`、`captures/rd3/review/disc_aa/modnet.png`。
- **確認できた事実:** MODNet（512×512 動的入力、0.30〜0.33 s/枚）は `disc_aa` で境界帯 MAE 0.017、半透明 MAE 0.075、部分 α 回収 0.693 を出し、事前登録ゲートのうち境界帯 0.05 と半透明 0.08 を初めて満たした。U2Net の同 fixture は境界帯 0.079、半透明 0.234、部分 α 回収 0.276。ただし MODNet は SAD が 0.28 → 0.21（27% 改善）で 40% 改善ゲートに届かず、connectivity は 0.022 → 0.050 と悪化した。
- **価値または懸念:** 「学習型では軟らかい α が出ない」のではなく「saliency モデルでは出ない」が正しい。モデル選択で結果が質的に変わる。一方 MODNet は人物 matting 用で、`hair_strands` / `soft_shadow` の予測 α 平均は 0.000、`fur_edge` は SAD 1.16 → 6.90 と GrabCut より悪化した。ドメイン外で無条件に使うと退行する。
- **次に確認すべきこと:** 人物・実写 fixture を追加して MODNet 本来のドメインで測る。BiRefNet / RMBG 系の高精度モデルで、`disc_aa` 型の成功が複雑な輪郭へ広がるかを見る。
- **未検証:** 実写人物での品質、モデルの学習データと本ラボ fixture のドメイン距離、解像度依存（512 固定への縮小が境界に与える影響）。
## 2026-11-15: 半透明 α のゲートはどのモデルでも満たせず、oracle も届かない
- **関連:** `experiments/MatteLab/`（rd1〜rd4）、`captures/rd4/metrics.json`、`captures/rd4/gates.json`。
- **確認できた事実:** 半透明 MAE ゲート（0.05<α<0.95 で 0.08 以下）は 3 モデルすべてで全 fixture 不合格だった。`glass_panel` は U2Net 0.607 / MODNet 0.614 / BiRefNet 0.552 で、最も良い BiRefNet でも 6.9 倍。GT trimap を与えた GrabCut oracle でも 0.331 で、事前登録ゲートを満たさない。一方 BiRefNet は同じ fixture で境界帯 MAE を 2 つ（`disc_aa` 0.011、`fur_edge` 0.0496）ゲート以下にできた。
- **価値または懸念:** 「α の連続値」と「境界の位置精度」は別物で、ゲートを分けた設計は正しかった。しかし半透明 α を扱う手法（半透明専用のマットモデル、層分離、複数露出の合成）が候補に入っていないため、このゲートは現状の手法集合では到達不能に近い。ゲート値か fixture の α 分布を設計判断で見直す必要がある。
- **次に確認すべきこと:** 半透明 α を扱える手法を 1 つ候補に入れる（例: 半透明専用モデル、または `glass_panel` 型の fixture を半透明ゲートの対象から外す）。fixture の α 分布（0.05〜0.95 の画素割合）とゲート値の対応を再設計する。
- **未検証:** 半透明専用モデルでの挙動、`glass_panel` 型の α 分布が実素材を代表するか、1024 入力版 BiRefNet での変化。
## 2026-11-15: guided filter は「解像度不足のぼけ」を直すが「構造の取り違え」は直さない
- **関連:** `experiments/MatteLab/`（rd5）、`captures/rd5/metrics.json`。
- **確認できた事実:** 入力画像をガイドにした guided filter（radius 2）でモデルの α を refinement すると、U2Net（320 入力）の `disc_aa` は SAD 0.94 → 0.34、境界帯 MAE 0.079 → 0.027 まで回復した。一方 BiRefNet（512 入力）は `hair_strands` で SAD 9.69 → 7.46・部分 α 回収 0.345 → 0.464 と改善する一方、`disc_aa`（0.13 → 0.19）と `fur_edge`（0.68 → 0.78）で既に正しい境界を壊した。SAD ゲート不合格は 1 → 3 に増加した。
- **価値または懸念:** refinement の効果は「モデルの出力解像度 < 入力解像度」かどうかで符号が変わる。低解像度モデルには必須級、高解像度モデルには有害な場合がある。raw / refined を選ぶ分岐（またはエッジ活動量による選択）無しに一律適用すると、良い画を壊す。
- **次に確認すべきこと:** 選択基準（エッジ活動量、raw と refined の差の大きさ、局所的な α の勾配）で raw / refined を出し分ける実装。半径依存の再確認（実写・高解像度素材）。
- **未検証:** 1024 入力版 BiRefNet、実写素材での半径依存、選択基準の精度。
## 2026-11-14 — Shader graph resource-bound stress case

- **関連:** `tests/ArtifactCore/ShaderGraphContractTest.cpp`, `tests/fixtures/shader-graph-rd/evaluation_manifest.json`, `docs/planned/MILESTONE_SHADER_GRAPH_RND_2026-11-13.md`
- **確認できた事実:** 現行graph APIにnode/link数上限はなく、material loweringは再帰DFSでOutput到達ノードを処理する。2048個のMath nodeを連結したsource-level stress caseを追加し、runtime未実施をmanifestへ明記した。
- **価値 / 懸念:** 深い文書がprocess stackやメモリを圧迫しないかを、将来の許可済みCPU testで測る入口ができる。ただし1点のstress caseは安全性や製品上限を証明しない。
- **次に確認すること:** 許可されたtest実行時にpeak allocation、parse/traversal時間、stack使用量を計測する。結果からJSON bytes/node/link/depth上限を決め、limit−1/limit/limit+1と深いcycleを追加評価する。
- **未検証の仮説:** 2048-deep chainが現行recursive compileでstack failureまたは著しい遅延を起こすか。
## 2026-11-14 — Shader graph image evaluation matrix gaps

- **関連:** `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`, `tests/fixtures/shader-graph-rd/evaluation_manifest.json`, `tests/fixtures/shader-graph-rd/README.md`
- **確認できた事実:** 既存capture sourceはVulkan RGBA32Fの単一sceneとproduction PNG smokeが中心で、black/white alpha composites、semantic crop export、Preview/final比較、resolution pair、D3D12は取得しない。現graph contractにはtime inputがない。
- **価値 / 懸念:** 必須の画像評価項目をmanifestで別々に追跡できるようにし、source-only captureを実績と誤認しないようにする。時間安定性は現範囲では対象外だが、time-dependent nodeやanimated parameterを追加したら再評価が必要。
- **次に確認すること:** capture adapterを拡張できる許可済み作業で、背景別alpha composite、意味crop、preview/final、256²/1024²、D3D12/Vulkan結果を保存し、実行者・環境・欠陥ログをmanifestへ記録する。
- **未検証の仮説:** 現production renderer経路でPreviewとfinalを独立して固定・取得できるか、alpha associationを保持したraw captureから複数背景合成が可能か。
## 2026-11-14 — Capture evidence record schema

- **関連:** `tests/fixtures/shader-graph-rd/evaluation_manifest.json`, `tools/shader_graph_rd/validate_shader_graph_manifest.py`
- **確認できた事実:** Manifestにcapture routesと測定計画はあるが、実行時に環境・revision・input/output hash・reviewer・performance値をまとめる実績record欄と必須field検査はなかった。
- **価値 / 懸念:** 実capture後の証跡を1 run単位で再現可能にし、計画値を実測として誤記するのを防げる。source validatorは値の真偽やoutput file内容までは証明しない。
- **次に確認すること:** 最初の許可済みcapture run後、recordのpath/hashが実在ファイルと一致し、解析JSON・画像・環境情報の再現性を手動レビューする。
## 2026-11-14 — Project and clipboard share graph document payload

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cpp`, `Artifact/src/Widgets/ArtifactShaderGraphWidget.cpp`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp`
- **確認できた事実:** 3D layerは非empty graph JSONを`material.graph`へverbatim保存し、load時はgraph validationせず保持する。Widget clipboardもnode/link/parameterとlayoutを含む同じJSONを使う。Controller applyのboolはlayerへの代入とrender invalidationを示すだけで、shader/backend成功を示さない。
- **価値 / 懸念:** Schema migrationやCore化でsemantic graphとeditor layoutを同時に変えると、project、clipboard、undoを一緒に壊す恐れがある。また無効payloadを保存時に落とすと修復可能性がなくなる。
- **次に確認すること:** 明示許可されたruntime検証でsave/reload、layoutを含むcopy/paste、invalid clipboard atomicity、legacy no-layout migration、undo/redoのlayer identityとeffective shader stateを確認する。
- **未検証の仮説:** clipboard invalid pasteは`fromJson()` staging semanticsで既存editor graphを保持するはずだが、UI item rebuildやselectionの副作用まで含む実機確認はない。
## 2026-11-14 — PSO names are not shader graph cache keys

- **関連:** `ArtifactCore/src/Graphics/MeshRenderer.cpp`, `docs/planned/MILESTONE_SHADER_GRAPH_RND_2026-11-13.md`
- **確認できた事実:** `makeMeshPipelineCacheName()`はPSO生成ごとにatomic serialを付ける。graph lowering hashはこのname関数へ渡されない。MeshRenderer内cacheはrender-target formatでPipelineSetを持ち、graph splice変更時に全消去される。Diligent `IPipelineStateCache`の実identityは未確認。
- **価値 / 懸念:** semantic IR hash、shader compile key、resource binding revision、PSO descriptor keyを分離し、texture変更とshader compile invalidationを混同しない契約が必要。serial名だけからcache hitを結論できない。
- **次に確認すること:** 許可済みruntime計測で同一graph反復、layout-only edit、texture revision、render-target format、backend/compiler option変更のcache hit/missとresource churnを観測する。
- **未検証の仮説:** Diligentのpersistent pipeline cacheが名前以外のshader bytecode/descriptor情報から実効的な再利用を行う可能性。
## 2026-11-14 — PNG review metrics for ROI and alpha backgrounds

- **関連:** `tools/shader_graph_rd/analy
## 2026-11-14 — Contract test inventory had undeclared source cases

- **関連:** `tests/ArtifactCore/ShaderGraphContractTest.cpp`, `tests/fixtures/shader-graph-rd/evaluation_manifest.json`, `tools/shader_graph_rd/validate_shader_graph_manifest.py`
- **確認できた事実:** 27件の`TEST(ShaderGraphContractTest, ...)`宣言のうち、従来manifestには12件だけが載っていた。残り15件を追加し、validatorはsource宣言とmanifest listの完全一致を検査する。
- **価値 / 懸念:** contract suiteの範囲と未登録状態を正確に追跡できる。宣言の一致はテストがCMake targetへ登録済み、compile可能、または実行済みである証拠ではない。
- **次に確認すること:** 明示的なbuild/test許可がある段階でtarget登録と実行を行い、現在source expectationで現行挙動が落ちる箇所を実証し、Core implementation taskへ分類する。
## 2026-11-14 — Shader graph CPU contract target registration path

- **関連:** `tests/ArtifactCore/CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/ArtifactCore/ShaderGraphContractTest.cpp`
- **確認できた事実:** 既存`ArtifactCoreMaterialContractTest`と共通`artifact_add_test()`が、ArtifactCore linkとGTest/Qt test executable/CTest登録の形を示す。shader graph contract sourceは`Artifact.ShaderNode.Core` moduleをimportするがCMake target登録されていない。
- **価値 / 懸念:** 最小登録target案をR&D計画とmanifestに固定し、将来の明示許可後に差分範囲を絞れる。module artifactの解決は実buildを通すまで未確認であり、source listingだけでは実行証拠にならない。
- **次に確認すること:** 明示許可後に`ArtifactCoreShaderGraphContractTest`を既存CMake test blockへ追加し、configure/build/CTestを順に実行してmodule importとGoogleTest case discoveryを確認する。
## 2026-11-14 — Principled numeric fields need isolated coverage

- **関連:** `tests/fixtures/shader-graph-rd/surface_numeric_semantics.json`, `tests/ArtifactCore/ShaderGraphContractTest.cpp`, `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp`, `ArtifactCore/src/Graphics/MeshRenderer.cpp`
- **確認できた事実:** 既存runtime graph capturesはchartreuse/emission中心で、Metallic/Roughness/EmissionStrengthを個別に分けたsurface semantic fixtureはなかった。現 loweringはMetallic/Roughnessを`saturate`し、Emission RGBへ`max(strength,0)`を乗じる。Raster epilogueはRoughnessを0.04..1へclampしEmissionをsampleへ置換する。
- **価値 / 懸念:** 新fixtureとcontract sourceがnode loweringとrenderer境界の値契約を区別して固定する。source testはfinal lighting、roughness response、HDR/clipping、shader parityの証拠にならない。
- **次に確認すること:** 明示許可後のGPU captureでMetallic 0/0.75/1、Roughness 0/0.25/1、EmissionStrength 0/1/2.5のresponse sweepを同一sceneで取得し、roughnessの0.04 floorとemission HDR pathを測る。
## 2026-11-14 — Pin lowering differs from final Raster application

- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp`, `ArtifactCore/src/Graphics/MeshRenderer.cpp`, shader graph R&D source contracts
- **確認できた事実:** Metallic/RoughnessはPrincipled node loweringとRaster epilogueの両方でclampされる。EmissionStrengthはlowering時にnonnegative化され、Renderer側ではgraph emissionをsample変数へ代入しtint/strengthを1へ設定する。
- **価値 / 懸念:** fixtureのsocket lowering式だけでは最終適用意味を証明できない。manifest source contractが両境界の代表式を機械的に追うようにしたが、source fragmentが存在することはcompile/runtime responseの証拠ではない。
- **次に確認すること:** GPU response sweepでclamp境界、metallic response、roughness 0.04 floor、emission HDRが実際のshader/pipelineでどう出るか測る。
## 2026-11-14 — HLSL order determinism is separate from JSON canonicali
## 2026-11-14 — Malformed graph determinism is not valid-graph determinism

- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp`, `tests/fixtures/shader-graph-rd/evaluation_manifest.json`
- **確認できた事実:** duplicate incoming linkはlink storage順で最初のsourceを採用し、multiple Material Outputはnode storage順で最初を採用する。JSON loaderはinvalid linkをskipしてwarning付き`ok=true`にし、unreachable nodes/cyclesはoutput-rooted loweringで無視される。direct `link()`はforeign Pin ownerを検査しない。
- **価値 / 懸念:** valid single-writer graphのlowering deterministic caseと、invalid/permissive inputのorder sensitivityを分離して評価できる。現APIでinvalid payloadをcompileしても安定したmaterial意味がない。
- **次に確認すること:** Core validatorでunsafe links・ambiguous outputsをlowering前に拒否し、unused node保持時のdiagnostic policyを決める。runtimeより先にCPU contract testsでatomic rejectionを実証する。
## 2026-11-14 — Principled numeric fixture is source coverage, not an isolated image sweep
- **関連:** `tests/fixtures/shader-graph-rd/surface_numeric_semantics.json`, `tests/fixtures/shader-graph-rd/evaluation_manifest.json`, `ArtifactCore/src/Graphics/MeshRenderer.cpp`。
- **確認できた事実:** numeric fixtureはMetallic=0.75、Roughness=0.25、Emission RGB=(0.25,0.5,1.0)、Strength=2.5を同時に接続する。既存float routeは別のgraph fixtureを使う。現Raster epilogueはMetallicを[0,1]へsaturate、Roughnessを[0.04,1]へclampし、graph emission RGBを既存sampleへ代入した後tint/strengthを1にする。
- **価値 / 懸念:** combined graphのbaseline差だけでは3パラメータの個別応答を分離できず、8-bit review imageだけではHDR emission値域の評価にもならない。manifestにsource boundaryと未接続状態を記し、将来評価では固定条件のfloat baselineと各1変数 sweep、highlight/metallic/emission ROI、明示display transformを求める。
- **次に確認すること:** fixtureを実capture adapterへ接続する許可後、値を一項目ずつ変えるcapture対を準備し、baseline noiseの反復測定後に許容差を定める。
- **未検証:** GPU上のlighting応答、roughness floorの視覚影響、HDR headroom、計測時間・性能は未確認。
## 2026-11-14: Shader Graph Principled NormalはNormal Textureフラグに条件依存する
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::compileMaterialGraph`、`ArtifactCore/src/Graphics/MeshRenderer.cpp::computeNormalMapped` / `MeshRenderer::setNormalTexture`。
- **確認できた事実:** Graph epilogueはgraph接続があればsampled `normalSample`を`graphNormalSample`で置換する。実際の`computeNormalMapped()`は`PbrTextureFlags.y`が0.5未満ならgeometric normalを返すだけでGraph Normal入力を使わない。このflagはNormal Textureを設定した時にtrueになる。flag有効時はgraph valueを`sample * 2 - 1`でtangent normalへdecodeし、normal strengthを適用する。つまり接続中のNormal Texture画像はgraph値に置換され、そのbindingは現在normal map branchのenable条件として残る。Material OutputのBump/Displacementは別のworld-normal override経路を持つ。
- **追加で確認できた事実:** Core float capture用sphereは以前`updateMeshGeometry(..., uvs=nullptr, ...)`だった。`computeNormalMapped()`はpositionと選択UVのscreen derivativesからT/Bを導出し、UV determinantがほぼ0ならgeometric normalへ戻る。Normal slot有効差を測るcaptureではlat-long UV0/UV1を渡すsourceを追加した。Authored tangentは別のshading-frame処理に使われるためこちらも初期化した。UV1/Tangent bufferはnull updateでは未初期化のままなので明示入力とsi
## 2026-11-14: Shader Graph Principled Occlusionもslot enable flagに条件依存する
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::compileMaterialGraph`、`ArtifactCore/src/Graphics/MeshRenderer.cpp` graph epilogue/AO calculation、`MeshRenderer::setOcclusionTexture`。
- **確認できた事実:** graph `Occlusion`値はsampled `occlusionSample`を置換し、Principled loweringで`saturate`される。しかしRaster AOは`lerp(1.0, lerp(1.0, occlusionSample, saturate(PbrFactors.w)), PbrTextureFlags.
## 2026-11-13: Invisible UI automation should separate widget capture from GPU viewport capture
- **関連:** `tools/capture_ui_test.py`、`Artifact/src/AppMain.cpp` の `ARTIFACT_UI_TEST_COMPONENTS_NO_WINDOW`、`Artifact/src/Widgets/ArtifactStartupScreenshot.cpp`。
- **確認できた事実:** 既存 `ArtifactUiControlMcpServer` と `ArtifactVirtualInput` を再利用し、`--ui-headless`、不可視 top-level widget flag、headless時のnative screenshot拒否、`ui.quit` を追加した。`tools/run_ui_offline.py` はCREATE_NO_WINDOW・localhost endpoint・isolated app data/logを設定し、startup screenshot既定値を明示的に無効化する。既存 `tools/ui_control_client.py` から構造照会、仮想入力、PNG captureを利用できる。Composition controllerにはdesktop fallbackを持たない`readbackPresentedFrameImage()`があり、明示診断用`ui.compositionCapture`から呼ぶよう接続した。`ui.state`にはreadbackなしでrenderer/frame診断値も含めた。
- **価値または懸念:** Qt widget操作は既存入力経路でOS入力注入から分離できる。GPU viewportはDiligent swapchain/offscreen targetの別条件を持つため、widget PNGが得られてもGPU描画captureを保証しない。
- **追加根拠:** Qt公式仕様は内部Show eventが可視化直前に届くこと、`QWidget::grab()`がwidgetと子widgetをpixmapへ描画することを定義する。これは設計/API契約を支えるが、Windows QPA上の実画面非表示、GPU子widget captureを証明しない。
- **未検証:** Windows上でmain window・dialogs・menusが一切desktop表示されないこと、不可視widgetの`QWidget::grab()`画像と入力反映、endpoint shutdown、不可視起動時に completed Diligent viewport frame/readback が有効か。実ビルド・runtime確認は未実施。
- **次に確認すべきこと:** 許可後にDebug Artifactでhidden launchし、widget列挙・virtual click・text/key・PNG capture・modal dialog・clean quitを確認する。GPU viewportは後続の別laneとしてswapchain-free surfaceを調べる。
## 2026-11-13: X-Ray 減衰係数 0.35f がハードコードで永続化もない
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp` L21210–21222, L45327–45331, L46424–46428, `docs/planned/DESIGN_VIEWPORT_SHADING_MODES_XRAY_2026-11-13.md` §D4.
- **確認できた事実:** X-Ray は bool トグルのみ。減衰はレイヤー描画ループ内で係数 0.35f が 2 か所にハードコード。`LayeredConfigStore` への保存なし（Rig overlay の `Viewport/RigOverlayVisible` は保存されるが X-Ray は非対称）。減衰基準は「選択レイヤーあり時のみ選択以外を減衰」の単一選択前提。
- **価値 / 懸念:** 不透明度 API 追加は係数差し替えだけで済む低コスト変更。ただし 3D の depth 共有条件 `opacity >= 0.9999f` は減衰係数 <1 で外れるため、不透明度化しても係数既定 0.35 維持なら挙動変化は起きない（現行と同一）。複数選択時の挙動と Blender 型「全体半透明」の対応は未決。
- **次に確認すること:** P1-1（Isolate 状態復元）/ P1-2（Ghosted）と一緒に X-Ray の選択基準・複数選択仕様を設計する。
- **未検証:** 減衰が RAM preview キャッシュ・depth 共有経路へ与える現行の実際の影響範囲、スライダ UI の使い勝手。
## 2026-11-13: Shader Graphを段階評価してCoreへ昇格
- **関連:** `ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx`、`ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp`、`Artifact/src/Widgets/ArtifactShaderGraphWidget.cpp`、`ArtifactCore/include/Material/Material.ixx`、`docs/planned/MILESTONE_SHADER_GRAPH_RND_2026-11-13.md`
- **確認できた事実:** 既にtyped node graph、HLSL lowering、graph JSON、Qt UI、3D material保存、Raster Renderer受け渡しの経路が存在する。Core graph実装は`std::vector`/`unique_ptr`を使い、JSON codecはQt JSON型に依存する。R&D計画時点ではRaster/RT parity、D3D12/Vulkan runtime、性能証跡が未確認。
- **仮説:** Graph document/validation/canonical seriali
## 2026-11-13: Shader Graph link/load/compileの失敗契約
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::NodeGraph::link/fromJson/compileMaterialGraph`、`ArtifactCore/src/Graphics/MeshRenderer.cpp::MeshRenderer::setMaterialGraphJson`、`tests/ArtifactCore/ShaderGraphContractTest.cpp`
- **確認できた事実:** `link()`はgraph ownership・型・single-inputを検証しない。JSON loaderは不正linkをskipして`ok=true`、compile loweringはoutput到達範囲のみ処理。JSON parse/load失敗はMeshRendererの以前のcompiled graphを維持する一方、compile `ok=false`のmagenta fallbackは適用される。失敗JSONは保存済み文字列へ反映されず再解析される。Widget ApplyはControllerのlayer mutation true後にUI側compile probeし、Renderer setterはload成功後compile `ok`を無視してmagenta blockも保存・適用する。Renderer boolは`ArtifactIRenderer.cpp`で未使用。PSO creationの有効性はこのAPI結果で報告されない。
- **価値または懸念:** UIへ診断を出す前にgraph validatorとcompile/apply/fallback状態を一貫させる必要がある。controller storage success、graph lowering、backend build、active pipelineを個別resultにしないとユーザー表示と実効shaderがずれる。保存順とhashの意味も区別する。同期PSO再生成の頻度・安全性はruntime未検証。
- **未検証:** ShaderNode compile/JSONの契約test sourceはあるがCMake target未登録、compile/run未実施。GPU shader compile failure後のresource rollbackも未検証。
- **次に確認すべきこと:** 許可された作業でCMake target登録後、CPU contract testsを実行する。続いてD3D12/Vulkanでcompile failure注入と既存material fallback、PSO cache churnを計測する。
## 2026-11-13: Shader Graphの数値画像評価はlinear floatと表示clip測定を分ける
- **関連:** `tools/shader_graph_rd/analy
## 2026-11-13: Shader GraphのBaseColor alphaは透明blend前にrange clampされない
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::compileMaterialGraph`、`ArtifactCore/src/Graphics/MeshRenderer.cpp` material graph epilogue / transparent PSO、`tests/fixtures/shader-graph-rd/basecolor_alpha_overflow_negative.json`。
- **確認できた事実:** Principled `Alpha`はloweringで`saturate`される。一方、RGB/BaseColor nodeのalpha literalは有限値のみで[0,1]範囲制限を受けず、Renderer epilogueは`materialAlpha = graphBaseColor.a * graphAlpha`を計算し、その後transparent blendへ出力する。出力alpha全体へのclampは該当経路に見当たらない。
- **価値または懸念:** BaseColor alpha=1.5かつgraphAlpha=1ではmaterial output alphaが1.5になり、transparent PSOのsource-alpha blend factorが標準範囲外へ出る可能性がある。画像やalpha parameterが通常値という前提だけでなく、document/lowering時のrange validationが必要。
- **次に確認すべきこと:** alpha=1.5 negative fixtureをCore contract testへ加え、rejectまたはdiagnostic付きclamp policyを決める。runtimeが許可されたら0,1境界・範囲外値でRGBA32Fとtransparent compositionを測る。
- **未検証:** Diligent backendのblend factorが1を超えた場合の具体動作、NaN/Inf以外のrange policy、C++ contract testとGPU captureは未実行。
## 2026-11-13: Shader Graphの保存成功とshader適用成功は別状態
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp::setMaterialGraphJsonOnSelectedLayer`、`ArtifactCore/src/Graphics/MeshRenderer.cpp::setMaterialGraphJson`、`Artifact/src/Widgets/ArtifactShaderGraphWidget.cpp::applyGraph`、Shader Graph R&D promotion gate。
- **確認できた事実:** ControllerはlayerへJSONを保存してrender dirtyを立てた後にtrueを返す。MeshRendererはJSON load成功後にcompile resultの`ok`を確認せず、fallback HLSLを含む文字列を設定する。Widgetはlayer setter後に独立compileし、失敗ならmagenta fallback適用をstatus表示する。
- **価値または懸念:** 呼び出し側boolだけではdocument保存・graph validation・shader compile・effective materialの状態を判定できず、UIとrendererが別々の結果解釈を持つ。無効JSONが保存済みprojectへ残り、fallback shaderになる挙動はlast-known-good維持とは異なる。
- **次に確認すべきこと:** compile/apply resultを段階別に区別するCore候補契約を、malformed/version/node/link/cycle/HLSL compiler failureと修正・clearのケースで検証する。policyとして明示magenta previewを残す場合も、production apply状態を混同しない。
- **未検証:** compile failureの実shader compiler結果、PSO失敗、GPU上のmaterial残存、project reload後の挙動は未確認。
## 2026-11-13: Shader Graphの同一JSON比較前にrender setupで文字列コピーが発生する
- **関連:** `Artifact/src/Render/ArtifactIRenderer.cpp` のMeshRenderer material setup、`ArtifactCore/include/Material/Material.ixx::materialGraphJson`、`ArtifactCore/src/Utils/UniString.cpp`、`ArtifactCore/src/Graphics/MeshRenderer.cpp::setMaterialGraphJson`。
- **確認できた事実:** renderer setupは`static_cast<std::string>(material.materialGraphJson())`を呼ぶ。getterは`UniString`値返却、コピーctorは`new Impl(other)`、std::string変換はQStringのUTF-8化とstd::string構築を行う。MeshRendererの等値JSON比較はその後に実行されるため、compile/lowerをskipしても上流の取得・変換コストは避けない。
- **価値または懸念:** 保存JSONを含む3D materialでは、render setupが反復されるたびに少なくともPImpl copy allocationが発生し、UTF-8/std::string変換も追加allocationになる可能性が高い。Shader compile cacheの早期returnはsteady-state allocation-freeを意味しない。
- **次に確認すべきこと:** 明示許可されたArtifact/ArtifactCore変更で参照/viewまたはrevision境界に変え、same-graph steady-stateをallocator計測しallocation-freeを確認する。graph変更時のJSON parse/HLSL lowering/PSO生成はcold pathとして別計測する。
- **未検証:** draw/render setupの正確な呼出頻度、release buildでのallocator回数とframe-time影響は実測していない。
## 2026-11-13: 多くのShader Graph入力既定値はJSONではなくlowering sourceが所有する
- **関連:** `ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx::Pin`、`ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::matWriteParams/matReadParams/compileMaterialGraph`、`tests/ArtifactCore/ShaderGraphContractTest.cpp`。
- **確認できた事実:** `Pin`はname/type/owner/directionのみを保持し、per-pin default valueを保存しない。JSONのnode-owned parameterはValue、RGB/LegacyColor、Math、MixRGB、ColorRamp、ImageTexture、MapRangeだけで、Fresnel IOR、Noise Scale、Blackbody Temperature、NormalMap strength等の未接続値はHLSL lowering内の定数である。Widgetはpin名/typeを描画するが、編集dialogはnode-owned fieldのみで、socketごとのdefault値編集UIは見つからなかった。R&D source testに既存node-owned fieldsのround-tripとNoise Scale fallbackのcaseを追加した。
- **価値または懸念:** 既定値の変更は既存JSONの見た目を変え、socket値としてUI編集・保存できない。Core schema昇格ではper-socket defaultを文書値として持たせるか、compiler contract versionを明示して既存projectの再現性を守る必要がある。
- **次に確認すべきこと:** 有効な全node typeと全editable fieldのround-trip、各unconnected input defaultのgolden HLSL/parameter responseを実行可能testにし、old-version migrationとの組合せを検証する。
- **未検証:** 追加したC++ testsは未登録・未コンパイル・未実行。UIが未接続pinの値を別途保持する隠れた経路も未確認。
## 2026-11-13: Color Ramp stopsの空配列・上限超過はload時に黙って変形する
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::matReadParams`、`tests/fixtures/shader-graph-rd/empty_ramp_stops_negative.json`、`oversi
## 2026-11-13: Shader Graph link APIは別graphのPinをraw pointerで保持しうる
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::NodeGraph::link/compileMaterialGraph/toJson`、`ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx::NodeGraph`、`tests/ArtifactCore/ShaderGraphContractTest.cpp`。
- **確認できた事実:** `link()`はnullとinput/output方向のみ検査し、Pin ownerのgraph membershipやtype compatibilityを検証せずraw Pin pointerを保存する。compile/seriali
## 2026-11-13: Shader Graph node IDのHLSLサニタイズが衝突する
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::saniti
## 2026-11-13: Shader GraphをCoreへ移すにはHLSL文字列境界を分ける必要がある
- **関連:** `ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx::NodeGraph/MaterialGraphResult`、`ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp`、`ArtifactCore/include/Container/NamedVector.ixx`、Shader Graph R&D milestoneのCore境界案。
- **確認できた事実:** 既存public moduleは`std::vector` ownershipとraw `Pin*` linksを公開し、material compile resultにHLSL splice stringsを含む。JSON codec実装はQt JSON typesを使い、`.cpp`実装は`Container.NamedVector`もimportしている。現Material Outputの`Displacement`はgeometry displacementでなくworld-normal override flagへlowerされる。ImageTextureは選択slotのtexture viewを使うが、graph loweringはraw input UV/default `In.UV`と共有`g_BaseColorSampler`を使い、通常PBR pathのslot別UV transformを反映しない。またnode出力は各slotのraw RGBAで、固定PBR側はBaseColor RGBA、Opacity A、Emission RGB、MetallicRoughness B/G、Normal RGBA、Occlusion Rを使う。現node inventoryにSeparate Color/XYZはなく、Vector4→Floatは`.x`へ変換される。
- **価値または懸念:** moduleがArtifactCoreに置かれているだけではUI/backend-neutralなCore契約ではない。document、pure validation、typed lowering IR、Artifact renderer adapterを分ける必要がある。IR proposalでは既存field semantics、Phase-1 opcode families、resource requirements、canonical float encoding、node familyごとのpromotion matrixを明記した。Noise等はtyped opに変換可能でもalgorithm/backend parity証拠が別途必要。ImageTextureのresource identityだけでなくraw swi
## 2026-11-13: ShaderNode実装moduleに未使用らしいNamedVector importがある
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp`、`ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx`、Core昇格のmodule境界。
- **確認できた事実:** ShaderNode実装cppmは`import Container.NamedVector;`を持つが、両ShaderNode source内の`NamedVector`文字列はimport行以外に現れず、公開宣言・実装の所有コレクションは`std::vector`/`std::unique_ptr`である。
- **価値または懸念:** 実際に未使用なら、Core module dependencyとdyndep graphを不要に広げている可能性がある。一方、文字列検索だけではmacro/ADL/間接利用を排除できないため断定しない。Core昇格時にはNodeGraph ownershipを自作containerへ機械置換せず、stable handle・bounded growth・copy/move・ABI条件を比較する必要がある。
- **次に確認すべきこと:** child repo変更が明示許可された段階で、名前解決・依存スキャンを含む狭いbuild確認のうえimport削除可否を判定する。NodeGraph public APIは新しいbackend-neutral documentへ分離し、既存graphは互換facadeとして段階移行する案を維持する。
- **未検証:** MSVC module scannerがこのimportをどの依存として扱うか、削除によるbuild graph/compile time差、Container.NamedVectorの間接的な必要性。
## 2026-11-13: Shader Graph Applyは保存後にcompile probeし、明示UndoCommandが見当たらない
- **関連:** `Artifact/src/Widgets/ArtifactShaderGraphWidget.cpp::applyGraph`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp::setMaterialGraphJsonOnSelectedLayer/clearMaterialGraphOnSelectedLayer`、`Artifact/src/Layer/Artifact3DModelLayer.cpp::toJsonProperties/fromJsonProperties`。
- **確認できた事実:** Applyはgraph JSON/layoutをlayerへ設定した後にcompile probeする。probe failureでもdocument stringはすでに変更済み。layer seriali
## 2026-11-13: Shader GraphのPBR適用とPSOキャッシュは別の意味境界
- **関連:** `ArtifactCore/src/Graphics/MeshRenderer.cpp::createPSO/setMaterialGraph/setMaterialGraphJson`、Shader Graph R&D promotion gate。
- **確認できた事実:** Graph spliceは固定pixel shaderのmaterial localsへBaseColor、Metallic(saturate)、Roughness(0.04..1 clamp)、Alpha(baseColor.a * graphAlpha)、Emission(RGB, strength=1)、world-normal override、Occlusionを反映する。pixel `CompileShader`結果は呼び出し元で検査されず、その後PSO作成へ進む。opaque PSOが空ならwarning後にreturnし、transparent PSO失敗はopaque fallbackで続行する。成功時のpipelineSetsはrender-target formatで保持され、Graph変更はcache全消去後にcreatePSOを呼ぶ。
- **価値または懸念:** NodeGraphのlowering出力が正しくてもPBR固定境界のclamp・alpha・emission規約で見た目が変わる。API setterのtrueはpixel shader compileやPSO成功を保証せず、PSO失敗後の有効pipeline保持もsource上明らかでない。PSO family nameやformat-keyed mapだけではbackend cache keyの完全性・shader content再利用を証明できない。
- **次に確認すべきこと:** runtime failure injectionでCompileShader/opaque PSO/transparent PSO失敗を分けてeffective PSOと次frame outputを記録する。graph変更・format切替・device/backend切替時のcache hit/missとPSO generation、既存pipelineの寿命を計測し、PBR field clamp/alpha/emissionをfixtureのgoldenへ固定する。
- **未検証:** `IPipelineStateCache` のcontent-key・backend invalidation、CompileShaderのfailure return/log semantics、failed graph PSO後にdrawがどうなるか、GPU output/PSO countsは未確認。
## 2026-11-13: Shader Graph JSON loaderはenvelopeと部分link loadを厳密検証しない
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::NodeGraph::fromJson`、`tests/fixtures/shader-graph-rd/*negative.json`、`tests/ArtifactCore/ShaderGraphContractTest.cpp`。
- **確認できた事実:** `fromJson()`はroot `version`を検査するが`kind`は読まない。missing `nodes`/`links`は`QJsonValue::toArray()`で空配列になり、loadが成功する経路がある。unknown nodeは全体rejectする一方、missing node/pin/type mismatch linkはskip後にgraphをcommitし、warning文字列を設定しながら`ok=true`を返す。これらのケースをwrong-kind/missing-arrays/skipped-type-mismatch負例fixtureとcontract test sourceに追加した。
- **価値または懸念:** `ok`だけを見るcallerは別種documentや不完全graphを正常loadと扱う可能性があり、persist済みgraphを置換し得る。warning文字列はstable diagnostic code/pathではない。staged graphはハードなload error時の保持を支えるが、現在はskip warningがerror扱いにならない。
- **次に確認すべきこと:** Core許可後にroot kind/required array/schema shapeを検証し、link違反をstructured diagnosticとしてrejectする契約を実装する。全negative corpusでprevious graphが不変であることをC++ runtime testで確かめる。
- **未検証:** C++ contract testsは未登録・未コンパイル・未実行。negative fixtureのloader acceptanceはsource inspectionに基づく予測であり、実行結果ではない。
## 2026-11-13: Shader Graph JSONのfinite doubleをfloat cast前に範囲検証していない
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::matFiniteDouble/matReadParams`、`tests/fixtures/shader-graph-rd/float_overflow_negative.json`、`tests/ArtifactCore/ShaderGraphContractTest.cpp`。
- **確認できた事実:** `matFiniteDouble()`はJSON numberをdoubleとしてfiniteか確認するが、Value/RGB/ColorRamp値の保存先はfloatであり、binary32の有限範囲を確認せずcastする。finite double `1e100`はbinary32で表現可能な有限値を大きく超える。fixtureとreject-or-normali
## 2026-11-13: Shader Graphの未知enum値は有効値へ無警告clampされる
- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp::matReadParams`、`tests/fixtures/shader-graph-rd/out_of_range_enum_negative.json`、`tests/ArtifactCore/ShaderGraphContractTest.cpp`。
- **確認できた事実:** Math op、MixRGB blend、ColorRamp interpolation、ImageTexture slot、MapRange interpolationのJSON enum integerは各有効範囲へ`std::clamp`される。Math op=17は16へsilent clampされる。out-of-range fixtureとreject-or-explicit-normali
## 2026-11-13: Shader Graph BaseColorは固定material tintをreplacementする
- **関連:** `ArtifactCore/src/Graphics/MeshRenderer.cpp` Pixel Shader `baseColor` assembly / `__ARTIFACT_MATERIAL_GRAPH_OVERRIDE__`、`tests/ArtifactCore/RayTracingAcceptanceTest.cpp::CapturesPbrSphereSweepUnderStinsonHdr`。
- **確認できた事実:** fixed pathはsRGB decodeした`In.Color`、`In.VertexColor`、`MaterialBaseColorFactor`をsample済みbase RGBへ乗算する。Graph epilogueはその後`baseColor = graphBaseColor`を代入するため、Graph BaseColor出力はfixed pathのこれらRGB factorを上書きする。Core float probe capture sourceは暗いinstance tintをbaselineへ設定し、このpath differenceを測定対象にした。
- **価値または懸念:** node graphのBaseColor値をmaterial tintと合成するのか完全置換するのかで、既存PBR編集値とtexture previewの期待が変わる。backend-neutral IRでもこの優先順位が曖昧だと、Graph適用後にlayer tint/vertex tintが消える互換性差を再現できない。
- **次に確認すべきこと:** product contractとしてreplace/multiplyを決め、同一textureのneutral/dark layer tint/vertex colorを使うfloat goldensで固定PBRとの差を測る。Graph enable/clear時の視覚的遷移もcaptureする。
- **未検証:** 実frameでこの差が見えるか、vertex color factorの実fixture、ユーザー向け期待、D3D12/Vulkan parityは未確認。今回のcapture sourceは未実行。
## 2026-11-13: Shader Graphのbackend失敗時はlast-good PSOを保持しない経路
- **関連:** `ArtifactCore/src/Graphics/MeshRenderer.cpp::setMaterialGraph/createPSO/prepare/draw`、`tests/fixtures/shader-graph-rd/identifier_collision_compile_failure.json`、Shader Graph R&D milestone。
- **確認できた事実:** `setMaterialGraph()` はhelper/blockを置換後に`prepared_`をfalseにし、pipeline set全体をclearしてからPSOを作る。`createPSO()`は対象opaque PSO/SRBも先にreleaseし、opaque PSOが得られなければSRB/pipelineSets登録前にreturnする。`prepare()`は有効PSO/SRBがなければ終了し、`draw()`はpreparedでない場合にskipする。source上、失敗前の有効pipelineへrollbackする処理は確認できなかった。
- **価値または懸念:** shader graphのcompile failureが一時的でも、last-known-good shaderではなく描画欠落へつながる可能性がある。またsetterはbackend build成否を返さないため、document適用と画面反映の不一致を呼び出し側で判別しにくい。
- **次に確認すべきこと:** `base-color` / `base_color` の識別子サニタイズ衝突fixtureでbackend compiler failureを誘発し、ログ・PSO結果・clear-only frame・次のvalid graphでの回復をGPU captureする。失敗が確実なら、Core昇格前にbuild resultとlast-good保持/fallback方針を明示する。
- **未検証:** 衝突HLSLを実backend compilerが拒否するか、CreateGraphicsPipelineStateの失敗表現、例外有無、capture pixelがclear colorになるか、回復可否はいずれも未実行の仮説。
## 2026-11-13: Render Queue camera shake 更新が const 制約と不整合
- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cpp:7100`、`Artifact/include/Layer/ArtifactCameraLayer.ixx:160`、DoF production GUI capture。
- **確認できた事実:** Debug `Artifact` target のビルドで C2662 が発生。GPU Render Queue path の `activeCamera` は snapshot composition の mutable `allLayerRef()` から取得した camera を指し、同じ path が評価 frame の shake state を進めている。`activeCamera` の pointer qualification を非 const にし、該当 Render Queue object はビルドできた。
- **価値または懸念:** snapshot 内の camera を更新する設計は意図的に見えるが、frame evaluation の反復時に shake state が再現可能かは別途確認が必要。非 const pointer はこの既存更新をコンパイル可能にする最小修正であり、`const_cast` は使っていない。
- **追加で確認できた事実:** 全体 Debug build は後続して Composition Render Controller の未定義 `appSettings` と、未コミットの AppMain 変更内の構文／名前解決エラー、および Composition Editor の X-Ray opacity menu の型・API エラーで停止した。既存 GUI binary の起動後、Computer Use window activation は 2 回失敗し、production capture は未取得。
- **未検証:** Render Queue shake 更新の deterministic contract、他 backend／build configuration への影響、修正後の production DoF comparison。
- **次に確認すべきこと:** 上記の残コンパイルエラーを最小修正し、現行 Artifact binary を再リンクした後、同一 DoF scene／frame／resolution の preview と Render Queue を比較する。
## 2026-11-13: Crop position property updates depend on edit order at source bounds
- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cpp::setLayerPropertyValue` の `sourceCrop.cropX/Y/Width/Height`、`Artifact/src/Layer/ArtifactSourceCrop.cpp::clampRectToSource`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp::setSelectedCropRect`。
- **確認できた事実:** 4×3 sourceのfull rectで位置を寸法より先に設定すると、各property update後の境界clampが入力位置を変えてしまう。`CompositionRenderController::setSelectedCropRect()`も同じ順で値を適用していた。controllerの適用・rollbackを寸法→位置順へ変更。回帰テストで要求rect=(3,2,2,2)が有効rect=(2,1,2,2)となり、画素境界を含めPASS。
- **価値または懸念:** 個別Inspector値の編集順は別経路なので、同じ問題が残る可能性がある。controllerの選択crop操作は修正済み。
- **未検証:** Inspectorの個別数値編集順、実GPU Previewの境界位置。
- **次に確認すべきこと:** Inspector経路とComposition GPU Preview captureを照合する。
## 2026-11-13: X-Ray 減衰係数 0.35f がハードコードで永続化もない
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp` L21210–21222, L45327–45331, L46424–46428, `docs/planned/DESIGN_VIEWPORT_SHADING_MODES_XRAY_2026-11-13.md` §D4.
- **確認できた事実:** X-Ray は bool トグルのみ。減衰はレイヤー描画ループ内で係数 0.35f が 2 か所にハードコード。`LayeredConfigStore` への保存なし（Rig overlay の `Viewport/RigOverlayVisible` は保存されるが X-Ray は非対称）。減衰基準は「選択レイヤーあり時のみ選択以外を減衰」の単一選択前提。
- **価値 / 懸念:** 不透明度 API 追加は係数差し替えだけで済む低コスト変更。ただし 3D の depth 共有条件 `opacity >= 0.9999f` は減衰係数 <1 で外れるため、不透明度化しても係数既定 0.35 維持なら挙動変化は起きない（現行と同一）。複数選択時の挙動と Blender 型「全体半透明」の対応は未決。
- **次に確認すること:** P1-1（Isolate 状態復元）/ P1-2（Ghosted）と一緒に X-Ray の選択基準・複数選択仕様を設計する。
- **未検証:** 減衰が RAM preview キャッシュ・depth 共有経路へ与える現行の実際の影響範囲、スライダ UI の使い勝手。
## 2026-11-13: Gaussian fog slice occupancyで透過率誤差を減らせる
- **関連:** `tools/fog_local_froxel_gaussian_optical_depth_rd.py`、F5 froxel CPU proxy。
- **確認できた事実:** erf閉形式line integralでdepth slice平均密度を作ると、thin Gaussianのaffected-pixel T RMSEは32-slice center sample`0.00607`からline-integral XY1×1`0.000261`へ低下。2×2/4×4 XY sampleはこのfixtureで誤差を増やし、32/64 slices差も小さかった。
- **価値または懸念:** 密度profileが解析可能なら高密度marchingより低slice数で透過率を保てる可能性がある。erf特殊関数のGPU costと任意fieldへの一般化は別途評価が必要。
- **未検証:** 点光源散乱と透過率を同時に解析積分した画質、GPU時間、arbitrary density textureや時間変動mediumでの安定性。

## 2026-11-20: Python layer move APIが負indexを先頭移動として受理する
- **関連:** `Artifact/src/Export/Python/ArtifactPythonAPI.cpp::_native_move_layer`。
- **確認できた事実:** `artifact.move_layer(layer, index)` の native bridge は、invalid/missing indexを `-1` に変換した直後に `std::max(0, index)` を適用していた。そのため負数や数値でない文字列が index 0 になり、入力エラーを先頭への移動として実行する。
- **価値または懸念:** スクリプトの typo や不正データで意図しない並べ替えが起きる。変換結果を clamp せず、既存の負 index error path で拒否するよう修正した。
- **次に確認すべきこと:** Python runtime 受入れで、負数／非数値は False、0 は先頭移動、正しい index は指定位置への移動になることを確認する。
- **未検証:** ビルド・runtime 実行は行っていない（AGENTS.md の制約）。

## 2026-11-20: Artifact system APIはbounded file accessと非同期process handleを基礎にする
- **関連:** `ArtifactCore/src/Script/Python/CorePythonAPI.cpp::registerSystemAPI`、`docs/technical/ARTIFACT_PYTHON_API_GUIDE.md`。
- **確認できた事実:** 既存 `artifact.core.system` はversion／platform／path resolveのみで、埋め込みPythonには標準 `os` などがある一方、Artifact側のfilesystem／process契約は未整備だった。初期APIに、read上限・no-overwrite default・file-only deletion・shell-less process launch・pollable PID handleを追加した。
- **価値または懸念:** Path / process操作を明示したmethod名と戻り契約で提供でき、同期的なprocess waitやshell stringをAPIに持ち込まない。任意path accessのsandbox、process lifetime cleanup、stdout/stderr capture、large file streaming、settings mutation はこの初期範囲で解決していない。
- **次に確認すべきこと:** 許可path policyと権限確認を製品仕様として決め、process終了時／engine finalization時のhandle lifecycle、異常時の例外契約をruntime受入れで確認する。
- **未検証:** Python wrapperのcompile/runtime実行、Windowsのcreation flags、path encoding／symlink挙動、process cleanupはいずれも未検証。

## 2026-11-19: 粒子シミュレーションは「1 回の update」でしか進めない — 大きな delta を 1 回渡すと静かに粒子が消える
- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cpp`（`EmitterParams::fixedTimeStep` / `maxSubSteps`）、`Artifact/src/Layer/ArtifactParticleLayer.cpp`（`renderToImage` が `particleSystem->update(deltaTime)` を 1 回呼ぶ）、`tools/BurnAwayLab/main.cpp`（評価ラボ）。
- **確認できた事実:** `ParticleSystem::update(deltaTime)` は固定タイムステップ（既定 1/120 秒）と最大サブステップ数（既定 8）でクランプされるため、**delta 1.5 秒を渡しても実際に進むのは約 0.067 秒分**。粒子レイヤーを `renderFrame(w, h, 1.5f)` のように 1 回で進めると、**粒子が出ない・出方が薄い**状態が「エラーなし」で返る（自分の ash/ember プリセットでは 38 粒しか出なかった）。**1/60 秒刻みで 90 回進ませると 94 粒になり、見た目も Properly になる**。
- **価値または懸念:** ヘッダに `fixedTimeStep` / `maxSubSteps` があるのに、**「1 回の update でどこまで進むか」の上限が API として露出していない**。大きな delta を渡す利用者は「simulation 落ちて particle が出ない」症状を、**エラーを出さずに**受け取る。**ヘッドレス検証で粒子を再現する際は、フレーム単位で進めるのが必須**であり、既存ラボ（`ParticleMirPlayground`）も `goToFrame` を 1 フレームずつ回していたのはこの制約のためと推定される。
- **次に確認すべきこと:** `maxSubSteps` を越えた delta を「時間どおりに成立させる」_large- delta 対応を追加すべきか（コストは dt の切り詰め）、それとも「要求時間が trunc された」ことを呼び出し側に返す API が必要か。現状はどちらも無く、**利用者が particle の出_expected 量から時間発展を推測できない**。
- **未検証:** GPU 経路（Diligent billboard）でも同じクランプが効くか（`updateAndRenderSoftwareFrame` はソフトウェア経路の別実装）。

## 2026-11-20: CPU エフェクトカーネルはチャンネル順を固定してはいけない — 入力源で backing order が変わる
- **関連:** `ArtifactCore/include/Graphics/SurfaceColorContract.ixx`（`SurfaceChannelOrder`）、`ArtifactCore/src/Image/ImageF32x4_RGBA.cpp`（`getPixel` は `FloatRGBA(pixel[2], pixel[1], pixel[0], pixel[3])`、`rgba32fData()` は「canonical B,G,R,A」と明記しながら**実体は descriptor 依存**）、`Artifact/src/Effects/BurnAway/BurnAwayEffect.cpp`、`Artifact/src/Effects/LinearWipe/LinearWipeEffect.cpp`（GPU 側は `.bgra` スワizzle）。
- **確認できた事実:** Burn Away の CPU カーネルは当初「内部バッファは B,G,R,A」と仮定して色演算を書いていた。しかし `ImageF32x4_RGBA` の backing order は入力源によって異なり、**QImage 由来は BGRA、OIIO デコード由来は RGBA**。実アプリでは画像レイヤーの面が OIIO 由来になり得るため、**そのままでは R/B が入れ替わって出荷される故障**になる。GPU 側は `makeGpuImageUploadBuffer` で canonical RGBA に正規化されるため、CPU だけ合っていない状態だった。
- **価値または懸念:** **「BGRA で確保されているから」で raw バッファのチャンネル順を決めてはいけない。** 色の演算をする CPU エフェクトは `colorDescriptor().channelOrder` を見て読み書きを分岐するか、`getPixel/setPixel` を使う。GPU 側は canonical RGBA で upload されるため、**CPU は GPU の契約（RGBA）に合わせる**のが正解。**検出は画像評価でしかできない**（alpha だけのテストは通る）。契約テストは「中立色の面」で R>G>B の順序を検証すると、入れ替わりの回帰に効く。
- **次に確認すべきこと:** 他エフェクトの CPU カーネルに同じ固定前提が残っていないか（`rgba32fData()` を色として直接扱う箇所を grep して descriptor 対応になっているかを確認）。本件は Burn Away のみ修正済み。
- **未検証:** 実際の画像レイヤー（OIIO 由来 RGBA 面）を通したアプリ経路での色順は未確認（GUI テストは swap chain 失敗で GPU 出力が出なかった）。

## 2026-11-19: Shape particle target の点群は描画経路と同じ可視 fill を検証する
- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cpp`（ShapePath内の候補点フィルタ）、`Artifact/src/Layer/ArtifactShapeLayer.cpp`（`nativeShapeFillPathsForParticleTarget()`）、`tools/Particle2DPlayground/main.cpp`（ordered Shape Stack試験素材）、`docs/planned/MILESTONE_PARTICLE_2D_SHAPE_TARGET_2026-11-18.md`。
- **確認できた事実:** VulkanとD3D12 headlessで外側・内側ellipse、Merge Paths/Subtract、Fill/Strokeのordered Shape Stackを描画し、穴中心alpha=0、ring sample alpha=184、sampler pathの穴中心contains=falseを確認した。固定LayerIDのfixtureではGPU描画、ターゲット形状、粒子中心が両backendで一致した。Shape attractionの最新frame 240は穴内1/360、可視ring mask内359/360。穴内の粒子index 320はframe 90の楕円正規化半径0.893からframe 240で0.978へ移動し、内周境界へ近づいていた。ArtifactCoreのShapePath winding-hole triangulation testでは全三角形内のbarycentric gridを走査し、穴被覆は0だったため、現時点の漏れはサンプラー誤生成より有限時間の粒子移動と整合するが、実際のShapeLayer target点列との直接照合は未実施。以前のlegacy subtractive child fixtureは描画 parityを満たさず、現行評価から除外した。
- **価値または懸念:** サンプラー点群だけでなく本番描画から取得したmaskと同じ座標で比較する必要がある。また、別プロセスでのbackend比較ではLayerIDを固定しないと、LayerID由来sampling seedが変化し、backend差とfixture差を混同する。
- **次に確認すべきこと:** legacy / 非stack multi-content / stroke-only / opacity / animated geometryを個別素材化し、対応する実描画経路のmaskとの一致を測る。Composition View / Render Queue経路と複数素材を評価する。
- **未検証:** Composition View / Render Queue、全Shape構成に対するfill-rule parity、性能閾値・Release比較・GPUメモリ予算。

## 2026-11-19: unscanned C++ module implementation は自分の interface IFC producer を先行させないと fresh Release build で解決できない場合がある
- **関連:** `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt`、`ArtifactCore/src/Graphics/Compute/*Computer.cpp` と対応する `include/Graphics/Compute/*.ixx`、`docs/planned/MILESTONE_PARTICLE_2D_SHAPE_TARGET_2026-11-18.md`。
- **確認できた事実:** Release Ninja のfresh buildで `CurveComputer.cpp` / `DuotoneComputer.cpp` が、自身の module interface IFCを見つけられず失敗した。Ninjaのobject queryでは、imported interface `.ixx.obj` は `OBJECT_DEPENDS` に並ぶが、当該実装が属する対応interface `.ixx.obj` は入力に含まれていなかった。対応interfaceを先に明示buildすると各Compute実装が通り、Particle変更の7 sourceもv143 Releaseで個別に通過した。全体ビルドは後続の別Fracture Runtime sourceで停止した。
- **価値または懸念:** source scanを無効にしたmodule implementationで、`/reference` 指定があることだけではproducer IFC生成との順序が保証されない可能性がある。clean buildと既存cache buildで挙動が違うと、Releaseの検証が不安定になり、別のsource errorに見えることもある。
- **次に確認すべきこと:** 対応関係が明示されている全unscanned implementationについて、Ninja graphでown interface object dependencyがあるかを監査し、必要ならArtifactCoreのCMakeでsource pairから依存を生成する。clean Releaseのparallel buildで再確認する。
- **未検証:** 対応interface依存がないimplementation全体の件数、Debug clean buildにも同じraceがあるか、CMake修正の影響範囲。ArtifactCore submodule自体は今回変更していない。

## 2026-11-19: Text particle target の初回準備時間は測定対象を分けて確認する
- **関連:** `tools/Particle2DPlayground/main.cpp`（`renderToImage()` による準備計測）、`Artifact/src/Layer/ArtifactTextLayer.cpp` のglyph outline sampling、`docs/planned/MILESTONE_PARTICLE_2D_SHAPE_TARGET_2026-11-18.md`。
- **確認できた事実:** 現行Debug headless captureの1回/backend計測で、Text target preparation + software layer draw全体が約749ms (Vulkan) / 766ms (D3D12)だった。計測はsampler単体ではなく初回 `renderToImage()` 全体を含む。
- **仮説・未検証:** glyph outline simplification / triangulation がcold-path時間の一部を占める可能性があるが、現計測では内訳未取得のため原因は未確定。旧探索runでは別値だったため、単一fixture・単一processでの再現性も未検証。
- **価値または懸念:** target更新時に長い同期停止が生じる可能性があり、性能合否を付ける前にsampler、target cache更新、layer drawを別々に計時する必要がある。現時点の約0.75秒をcache生成だけの時間として扱わない。
- **次に確認すべきこと:** 計測区間を分解し、複数runとReleaseで再測定して許容時間を事前に定める。

## 2026-11-20: Module interface 経由の DLL data export は consumer object の参照形式まで確認する
- **関連:** `ArtifactWidgets/include/Dialog/FloatColorPicker.ixx`、`ArtifactWidgets/src/Dialog/FloatColorPicker.cpp`、`Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cpp`、隔離評価ビルドの `ArtifactParticle2DPlayground` link。
- **追加で確認できた事実:** `ArtifactWidgets/CMakeLists.txt` は `WIDGET_LIBRARY_DLL_EXPORT` をPRIVATE、`WIDGET_LIBRARY_DLL_IMPORT` をINTERFACE compile definitionにしており、`FloatColorPicker.ixx` のクラス宣言は `WIDGET_LIBRARY_DLL_API` で注釈される。よってmodule interfaceの提供側はexport定義でBMIを生成し、consumer側はそのBMIをimportする構成である。
- **追加で確認できた事実:** 製品ファイルを変えない隔離再現をMSVC 14.51で作成し、Qt `QObject`/Verdigris `W_OBJECT`付きのexport module classを`dllexport`で宣言したBMIをconsumerからimportした。通常のglobal-module consumer、別のmodule implementation unit、dummy class間のtyped `QObject::connect`の全ケースで、consumer objectの未定義参照は `__imp_?staticMetaObject...` になった。さらに実際の`FloatColorPickerDialog.ifc`を直接importしたstatic data参照もMSVC 14.51では `__imp_?staticMetaObject...` だった。一方、MSVC 19.44で同じ実IFCをimportすると `?staticMetaObject...` の直接参照となり、実製品のリンクエラーを再現した。原因は少なくとも一般的なBMI importやtyped connectではなく、MSVC 19.44と14.51のC++ module/DLL import差であることが確認できた。
- **追加で確認できた事実:** MSVC 19.44上で、実際の `FloatColorPicker.ixx` を隔離再コンパイルし `WIDGET_LIBRARY_DLL_EXPORT` を外して（`WIDGET_LIBRARY_DLL_API` が `dllimport` となる条件で）生成したIFCを別module interface consumerからimportすると、consumer objectは `__imp_?staticMetaObject...` を参照した。通常のexport設定で生成したIFCでは直接参照になる。この比較はIFC属性と参照形式の関係を示すが、同一moduleのexport providerと併用できる正規ビルド構成や修正案はまだ検証していない。
- **追加で確認できた事実:** export module classを `extern "C++"` linkage specificationで包む隔離変更はconsumer側の `staticMetaObject` 参照からmodule名suffixを外したが、参照は直接symbolのままで、import libraryの `__imp_...` と一致せずリンク修正にならなかった。さらにconsumer用 `dllimport` IFCを同moduleの `W_OBJECT_IMPL` providerにも使う試験では、`staticMetaObject` 定義にC2491となった。`extern "C++"` の追加とprovider/consumer IFCの単純な共有切替は修正候補から除外する。
- **仮説・未検証:** MSVC 19.44の制約を回避する正式な製品修正は未確定。MSVC 14.51で通常製品リンクを回せるか隔離buildを試したが、リンク段階の前に`Animation.Transform3D` IFC importで`stop_token(248)`のC1116となり停止したため、ツールセット更新は現状すぐ使える代替確認経路にならなかった。診断目的で隔離コピーのみ `/FORCE:UNRESOLVED` を指定するとheadless粒子captureは完走し、Vulkan/D3D12各150枚とframe60逐次／直接シーク一致を取得できた。frame149は両backendで下端余白0px・差分率3.347%となり、既定ゲートに不合格。これは通常リンクが直った証明ではなく、未解決箇所を粒子経路が実行しなかった場合の隔離診断証跡に限る。Artifact submoduleは変更していない。
- **価値または懸念:** C++ moduleとDLL境界の問題を「implementation objectがビルドされたか」だけで判断すると、symbol import/exportの不一致を見落とす。consumer objectのundefined symbolとDLL export/import libraryを比較し、どの形式がずれているかまで記録する必要がある。
- **次に確認すべきこと:** 19.44のconsumerだけが `dllimport` 属性を含むBMIを使い、provider実装は `dllexport` BMIを使うCMake module ownershipを成立させられるか、または既存C++ module APIの変更なしにツールセット制約を回避できるかを検証する。ユーザーからArtifactWidgets submodule変更の許可を得たが、修正設計は未確定であり、まだ追跡ファイルは変更していない。正規リンク後、キャプチャを再実行して診断証跡との一致を確認する。
- **未検証:** 他のArtifactWidgets module classで同様のdata symbol不一致が起きるか、`FloatColorPicker`固有か、全体リンクを成立させる正式な修正、診断リンクで得たピクセルと通常リンク製品の一致。

## 2026-11-20: MSVC 19.44 module IFCでQ_OBJECTのdata exportを明示する
- **関連:** `ArtifactWidgets/include/Dialog/FloatColorPicker.ixx`、Particle2DPlaygroundの通常リンク評価、`experiments/Particle2DShapeTarget/v33_diagnostic_manifest.json`。
- **確認できた事実:** MSVC 19.44ではclass-level `dllexport`のC++ module IFCをimportしたconsumer objectが`staticMetaObject`を直接参照し、DLL import libraryの`__imp_`と一致しなかった。Qt6の`Q_OBJECT` expansion内でstatic data memberだけを`WIDGET_LIBRARY_DLL_API`付きで宣言し、クラス全体ではなく公開メソッドを個別exportするとconsumer objectが`__imp_`参照になった。通常リンクから`/FORCE:UNRESOLVED`を外したArtifactParticle2DPlayground targetが成功し、Vulkan/D3D12のv33 150フレームcaptureも完走した。
- **価値または懸念:** QtのQ_OBJECT macro expansionをローカルで上書きするため、Qt更新時に`qtmetamacros.h`の宣言と一致するか再確認が必要。影響はFloatColorPickerに限定し、他module classへ一般化しない。
- **次に確認すべきこと:** Qt更新時にmacro expansionを照合し、MSVC 19.44/19.51両方でconsumer objectのimport symbolと通常リンクを確認する。Artifactアプリ本体UI、Particle Inspector、Composition View、実時間GPU再生はこのheadless Playground評価で証明されていない。

## 2026-11-19: ネイティブ／GPU エンコーダは alpha pix_fmt を設定できない
- **関連:** `Artifact/src/Render/ArtifactRenderQueueEncoder.cpp` の `NativeFFmpegBackend` / `createVideoEncodeBackend()`、`ArtifactCore/include/Video/FFMpegEncoder.ixx` の `FFmpegEncoderSettings`。
- **確認できた事実:** `FFmpegEncoderSettings` に pixel format／alpha state フィールドが無く、`FFmpegEncoder.cpp` が codec/profile から `AV_PIX_FMT_*` を決める。そのため pipe 側で alpha 対応 pix_fmt を選んでも、native/GPU/auto（GPU 優先）経路では alpha が落ちる。今回は alpha 明示ジョブを software pipe へ自動ルーティングして回避した。
- **価値 / 懸念:** ユーザー明示許可により `FFmpegEncoderSettings::pixelFormat` を追加し、ソフトウェアエンコーダーにのみ適用する形で解決した。HW エンコーダーは固定形式のため対象外で、今も pipe ルーティングで回避している。
- **未検証:** 実ファイルでの alpha 保持、HW エンコーダの capability、H.264/MP4 の alpha 互換性は未検証。
- **次に確認すること:** ビルド許可が出たら `native` バックエンドと software pipe の両方で alpha 保持を実機検証し、HW ルートは capability probe で切り分ける。

## 2026-11-20: Virtual Transition の一時合成は既存の事前確保済み offscreen 系を再利用できる
- **関連:** `Artifact/include/Render/ArtifactRenderLayerPipeline.ixx:72-319,192-206,270-280`（accum/layer/temp の事前確保と history pool）、`Artifact/src/Layer/ArtifactGroupLayer.cpp:245-457`（TextureManager の cached temp RT / scratch RT）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp:11436-11507`（precomp 2D GPU 出力キャッシュ）、`ArtifactCore/src/Graphics/LayerBlendPipeline.cpp:1011-1077`（crossfade compute）。
- **確認できた事実:** transition の A/B 一時合成に必要な「別 SRV 2 枚 + 出力 UAV 1 枚 + 既存 accum への 1 回 blend」は、新規の汎用 group composite を書かずに既存 RenderPipeline のスクラッチ/target プールと `crossfadeLayers()` で構成できそう。group layer と precomp は既に同様の cached offscreen 方式を使う。crossfade kernel は入力/出力エイリアスを既に拒否する。
- **仮説・未検証:** 既存スクラッチを transition 用に流用すると accum ping-pong や effect mask の temporaries と競合する可能性があり、専用スロット追加が必要かどうかは未検証。同時 active の上限とプール枯渇時の drop 契約も未計測。
- **価値または懸念:** 新規 `QImage`/`QPainter` やフレーム中 GPU resource 生成を避けつつ stack insertion を実装する近道。逆に、ここを誤るとホットパスで target のエイリアス／再利用による画素汚染が起きる。
- **次に確認すべきこと:** 実装時に RenderPipeline の既存 target と transition 用スロットの生存期間を図で固定し、D3D12/Vulkan 両 backend でエイリアス無しを契約テストにすること。設計方針は `docs/design/virtual-transition-stack/RENDER_INTEGRATION_DESIGN_2026-11-20.md` D2/D8。

## 2026-11-21: HEAD の tests/Artifact/CMakeLists.txt に実体しない .cpp の登録があり、CMake 構成が失敗する
- **関連:** `tests/Artifact/CMakeLists.txt:160`（`artifact_add_test(ArtifactLayerGroupingUiContractTest ...)`）、コミット `d9c84ac2`（"feat: define virtual transition stack project", 2026-11-20）。
- **確認できた事実:** `d9c84ac2` は `tests/Artifact/CMakeLists.txt` に `ArtifactLayerGroupingUiContractTest` の登録（`SOURCES ArtifactLayerGroupingUiContractTest.cpp`）を追加したが、**当該 `.cpp` は同コミットにも後続コミットにも含まれず**、HEAD (`c7da0405`) でも作業ツリーでも実体が存在しない。`git log --all --diff-filter=A -- '*LayerGroupingUi*'` は 0 件、`git fsck --dangling` でも復元不能、`git worktree list` にある 8 ワークツリーのいずれにも存在しない。CMake 構成は `Cannot find source file: ArtifactLayerGroupingUiContractTest.cpp` で失敗し、`ARTIFACT_BUILD_TESTS=ON` の全ターゲット（tools/ 配下の Playground を含む）のビルドが不可になる。
- **価値または懸念:** **テストの CMake 登録と実体を同一コミットで揃えない**と、リポジトリ全体が構成不能になる（此次は 1 ファイルの欠落で `tools/` 配下の作業も全部止まる）。AGENTS.md の「新規ソースは CMake に明示登録」「子リポジトリは明示依頼時のみ編集」と同じ軸の話だが、**親リポジトリ側のテスト登録**も「実体と同一コミット」の原則を持つべき。テスト無効化（`ARTIFACT_BUILD_TESTS=OFF`）で自分のターゲットだけビルドする回避策が使えるが、恒久対応は別セッションの `.cpp` 追加か登録の差し戻し。
- **次に確認すべきこと:** 当該 `.cpp` を誰がどのブランチで作成予定だったか（`d9c84ac2` の作者セッションが作業中か）。本 R&D では他セッションのファイルを touch せず、別ビルドツリー（`ARTIFACT_BUILD_TESTS=OFF`）で自前ターゲットのみをビルドして切り分けを進めた。
- **未検証:** 欠落ファイルがどの作業中だったか（未特定）。

## 2026-11-21: J: ドライブが 0 byte になり、git index.lock 書き込みと CMake 構成が失敗する（Recycle Bin が同一ドライブ）
- **関連:** `out/build/`（Ninja ツリー群）、`build/`（VS generator ツリー 203.5 GB）、`C:\Users\lagma\.minimax\bin\mavis-trash.js`（Recycle Bin 経由の削除）。
- **確認できた事実:** 作業中の `git status` が `fatal: sha1 file '.git/index.lock' write error. Out of diskspace` で失敗し、`Win32_LogicalDisk` の `FreeSpace` が J: で **0 byte**（サイズ 953.9 GB）になっていた。容量を食っていたのは `build/` 203.5 GB（VS generator ツリー。AGENTS.md と作業記憶のとおり ArtifactWidgets の module 実装単位が衝突して**使用不能**）と、`out/build/` 配下の古いツリー群（`glow-lab-*`、`x64-Debug-Agent-V143-ImageRND*`、`x64-Debug-Clang*`、`x64-Release-*`、`rq-particle-v143`、失敗した `x64-Debug-MirPlayground` 9.7 GB）。**`mavis-trash` は Windows Recycle Bin を使うため同一ドライブ（J:）の空きは戻らない**（削除後に `Clear-RecycleBin -DriveLetter J -Force` が必要）。実行後 J: は 0 → **260.96 GB** まで回復した。
- **価値または懸念:** 「trash に送った＝容量が戻った」わけではない。**同一ドライブの Recycle Bin は容量を保持し続ける**ため、ディスク起因の障害対処には Recycle Bin の空化まで含めるべき。使用不能と判明済みのビルドツリー（VS generator の `build/`）を温存し続けるのは容量の無駄であり、確認済みの使用不可に基づく再生成は不可避。
- **次に確認すべきこと:** `build/`（VS generator）は trash は成功扱いだが `Test-Path` が true のままで、回収が不完全だった。J: の空きは回復しているので実害はないが定期的なビルドツリー清理（現在有効なツリーの一覧保持）を検討する。
- **未検証:** Recycle Bin 空化後に `build/` の内容がどうなっているか（未確認）。

## 2026-11-21: Artifact 子リポジトリの HEAD がビルド不能（Python API の型不一致 + ArtifactCore に存在しない API 参照）
- **関連:** `Artifact/src/Export/Python/ArtifactPythonAPI.cpp:280,288,295`（`toBool`/`toDouble` は `std::string` を受けるのに `QString rawValue` を渡している）、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp:5296`（`ArtifactCore::FileTypeDetector::suffixPatterns` / `FileCapability` を呼ぶが `ArtifactCore/include/File/FileTypeDetector.ixx` には存在しない）、`Artifact/include/Export/Python/ArtifactPythonAPI.ixx`。
- **確認できた事実:** (1) Python API の型不一致は子リポジトリの `b24d5a61` で導入され、`origin/main`（`83427f12`）でも**そのまま残っている**（`git show origin/main:...` で同一コードを確認）。(2) `FileTypeDetector::suffixPatterns` / `FileCapability` は `Artifact` 側の `41437d90`（"feat: evaluate multiple virtual transitions"）が `ArtifactLayerPanelWidget.cpp` に追加した呼び出しだが、`ArtifactCore` の**全参照（main / origin/main / origin/master / 全履歴）で実装が存在しない**（`git log --all -S 'FileCapability'` は 0 件）。(3) この 2 件で `ArtifactAppRuntime` のビルドが失敗し、`ArtifactAppRuntime` にリンクする `tools/Particle*Playground` も一緒にビルド不能になる。(4) 親リポジトリ側の `tests/Artifact/CMakeLists.txt` にも `ArtifactLayerGroupingUiContractTest.cpp` の登録だけがあり実体が存在せず、これは CMake **Generate 段階**の失敗（configure 自体が通らない）になる。(5) `Artifact`/`ArtifactCore` は子リポジトリのため AGENTS.md により明示依頼なしには編集できず、回避として親 `CMakeLists.txt` に `ARTIFACT_DISABLE_BROKEN_PYTHON_API` オプション（`HEADER_FILE_ONLY` による該当 TU の除外）と `ARTIFACT_BUILD_TESTS=OFF` 構成を使って自前ターゲットのみを通す運用、確認できた。
- **価値または懸念:** **子リポジトリの HEAD がビルド不能な状態では、亲子構成の全体ビルドが成立しない**ため、孫（tools/ 配下のプレビュー用ターゲット）も含めて作業不能になる。Python API の型不一致は `origin/main` にもあるため、リモート側が壊れたままの可能性がある。`ArtifactCore` の API を `Artifact` 側が先行して使う形は、**子リポジトリ間の依存順序（ArtifactCore → Artifact）を守らない変更**の典型的な失敗例。
- **次に確認すべきこと:** `41437d90` の作者セッションが `ArtifactCore` 側の `FileCapability` / `suffixPatterns` 実装を別の作業ツリー（未コミット）で持っているなら、それを同梱した 子リポジトリ bump コミットが本来的な形。`toBool`/`toDouble` の `QString` 受けオーバーロード追加か `toStdString()` 変換が必要。
- **未検証:** 自分の区画（`tools/ParticleMirPlayground`）の最終ビルドと新しい `--capture-2d-software-sparse` モードの実測（Python API 回避で再構成は通ったが、リンクまで到達していない）。

## 2026-11-21: ソフトフォールバックの「中空リング」は单个粒子ではなく高密度の重なりによる見かけ（画素プロファイルで確定）
- **関連:** `tools/ParticleMirPlayground/profile_region.py`（alpha 基準）、`tools/ParticleMirPlayground/profile_luma.py`（輝度基準）、`captures/mir-particle-20261119-software/mir_2d_software_frame_45.png` と `mir_2d_gpu_frame_45.png`、`captures/mir-particle-20261119-v16/IMAGE_EVALUATION_REPORT.md`。
- **確認できた事実:** 高密度キャプチャの単離ブローブ（周辺より 25 以上明るい孤立点）を探し、その行の alpha / 輝度プロファイルを分類した（中心平均 vs 外周平均）。ソフト経路は 15 ブローブすべてが **solid**（alpha ピーク 243〜255、中心平均 138〜162、外周平均 101〜123 で単調減衰）、GPU 経路も 8 ブローブすべて **solid**（輝度ピーク 250〜255、中心 250〜253、外周 199〜207）。**hollow（中心が外周より 25% 以上暗い）は両経路で 0 件**。視覚採点が「中空リング／細胞状スペックル／指状垂れ」と報告したのは、64 個の Point エミッターが高密度で重なり、1 粒子あたりのスプライト径が GPU 経路より大きい（crop 中央 run 長 17px 対 10px）ソフト経路で、加算合成が飽和した領域の輪郭として見えたため。
- **価値または懸念:** **視覚モデルの「リング」「スペックル」「垂れ」という形状指摘は、実測プロファイルと突き合わせないと誤診に結び付く**。今回のようにブローブ単位で alpha/輝度プロファイルを取ると「単離ブローブは正常か、高密度の重なりか」を 1 つの数値（solid/hollow/flat の分類数）で切り分けられる。画像認識は見た目の異常を指摘する強い観測者だが、**「単個 instances の破綻」か「集合体の見かけ」かは区別しない**ので、形状指摘には必ず画素プロファイルによる裏取りを添えるべき。
- **次に確認すべきこと:** 単一粒子条件で再キャプチャすれば密度依存を除いて確定できる（`--capture-2d-software-sparse` は実装済みだが、子リポジトリ起因のビルド不能で未到達）。粒子径の parity（ソフト `size*2` vs GPU `size*10`）は製品コード側の判断が必要。
- **未検証:** 単粒子条件の実キャプチャ、粒子径 parity の修正方針、フォールバック到達条件の特定。

## 2026-11-21: 翻訳カタログは「選択可能ロケール」だけ追えばよいが、そのロケールの中のキー欠落は別問題
- **関連:** `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cpp:236-244`（`minKeys = max(50, en の 1/10)` で低カバレッジ言語を選択肢から除外）、`Artifact/translations/*.json`、`docs/analysis/TIMELINE_PROPERTY_TERMINOLOGY_AUDIT_2026-11-21.md` Slice A。
- **確認できた事実:** 11 個のカタログのうち実キー数は `en`/`ja` 1850、`zh`/`zh-TW` 341、`ko`/`fr`/`de`/`es`/`pt`/`ru`/`ar` 68。閾値 185 のため設定画面で選べるのは **en/ja/zh/zh-TW の 4 つだけ**で、残り 7 言語はファイルが存在しても選択不能。一方で `zh`/`zh-TW` は `en` の 1850 キーに対し 341 しか持たず、`layer_panel.menu_trans_wipe` / `menu_trans_slide` のように **en にあって zh に無いキー**が既に存在する（今回の変更前から）。
- **価値または懸念:** 「選択可能＝カバレッジ十分」ではない。新規キーを 4 言語に揃えるのは必須だが、**既存の欠落を全 locale parity と混同しない**こと。欠落キーは `tt(key, fallback)` により英語へフォールバックするため、日本語/中国語 UI に英語が 1 語だけ混ざる形で現れる。`en`/`ja` も `_meta` を持つが `zh`/`zh-TW` は持たないなど、ファイル構造自体が非対称。
- **次に確認すべきこと:** `timeline` / `property` / `layer_panel` セクション全体の 4 言語 parity を機械的に差分し、欠落キー一覧を出す（`tools/i18n/check_locale_keysets.py` が `en/ja/zh/zh-TW/ko/...` を列挙する骨組みを持っている）。
- **未検証:** 欠落キーが実画面でフォールバック表示されているか（UI 実行・キャプチャ未実施）、`zh`/`zh-TW` の欠落が意図的な段階的翻訳かどうか。
## 2026-11-20: External Python fallbackでは各executeにCore API preludeを再投入する
- **関連:** `ArtifactCore/src/Script/Python/PythonEngine.cpp::execute/registerPreludeCode`、`ArtifactCore/src/Script/Python/CorePythonAPI.cpp::registerAll`。
- **確認できた事実:** embedded Pythonを無効にした `PythonEngine::execute()` は呼び出しごとに新しい `QProcess` を作り、Python module／global stateを保持しない。従来 `CorePythonAPI` が `execute()` で作成していた `artifact.core` objectとhelper functionは登録時の subprocess 終了とともに失われ、後続スクリプトから利用できなかった。`registerPreludeCode()` を追加し、Core API定義をexternal実行ごとに再投入するようにした。
- **価値または懸念:** pure Python の system／math／color／DSP helperはexternal fallbackでも利用できる。一方、`ArtifactPythonAPI` やWorkspace bridgeのC++ callbacksは別processへ渡せず、external runtimeには存在しない。
- **次に確認すべきこと:** external fallbackとembedded runtimeの両方で、Core API bootstrap、複数macro実行、prelude重複登録、初期化エラー報告を実行確認する。
- **未検証:** preludeの実行・例外転送・Windows／POSIXの両runtime、および反復登録の実測は未実施。
