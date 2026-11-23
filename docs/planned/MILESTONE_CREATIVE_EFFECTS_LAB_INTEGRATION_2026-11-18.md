# CreativeEffectsLab 研究成果の製品反映メモ

**最終更新:** 2026-11-18
**ステータス:** In Progress — Contour Poster の実験用製品導線と CPU fallback を実装。品質・GPU・実アプリ評価は継続中。

## 対象と研究元

独立リポジトリ `CreativeEffectsLab` の `studies/ink-reveal/` と `studies/contour-poster/`。研究プログラム・評価計画・ノートを正とする。2026-11-18 時点で研究ファイルは未コミットであり、固定 revision はまだない。製品実装に着手する前に revision と入力・出力 hash を固定する。

## 調査結果

- インク拡散は固定の空間閾値場を使う2画像間の reveal。Python の人工素材では端点一致とマスク単調性を確認。進行再マッピングは coverage を均すが、動きの自然さは未判定。
- `ArtifactCore/include/Video/AbstractTransition.ixx` は `TransitionContext` と左右画像を受け、`GradientWipeTransition.ixx` は CPU フレームを処理する。既存の Transition R&D は製品側の複数経路の統合と実 renderer capture が未確認と記録する。この研究結果をただちにその経路へ実装する根拠にはならない。
- 等高線ポスターは輝度の等値線を画像内部に重ねる。`Artifact/src/Effects/ColorCorrection/PosterizeEffect.cpp` は RGB 各チャンネルを段階化するため同一処理ではない。合成画像では平坦部の偽線を勾配利用で抑えたが、subpixel 移動で局所差が残り、画質判定は未実施。
- `Artifact/src/Render/ArtifactRenderLayerPipeline.cpp` の `applySpatialEffect` は Generic node を扱い、`LiftGammaGainEffect.cpp` などに node 登録例がある。現行コードに GPU 常駐の候補経路が存在することを示すが、対象研究をそのまま実行可能とは確認していない。旧 `MILESTONE_EFFECT_GPU_RESIDENCY_AND_ASYNC_PREVIEW_2026-09-11.md` は設計時点の記録であり、現状の全経路を表すものとして使わない。
- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md` は canonical な内部色・alpha と CPU/GPU 値順を定義する提案文書。Python は linear straight 入力から premultiplied 出力を作るため、実際の host 経路の descriptor と alpha を確認し、近透明の除算条件と境界・HDR を個別に評価する。

## 反映前の作業

1. 両研究の評価条件と判定者を固定し、典型・失敗素材、全画像・crop、alpha 背景、時間安定性、性能を記録する。必須項目が未検証の間は品質達成としない。
2. 現行コードから対象 layer、effect stack、Generic node の条件、shader の入力と resource lifetime、fallback、保存・Undo、UI 登録を追跡し、対象経路を確定する。GPU優先とし、ホットパスでの確保、暗黙 transfer、同期 readback を増やさない。
3. 研究 candidate の採否と受入条件を記録したうえで、必要な製品側変更を限定する。CPU Python の数値は D3D12/Vulkan、preview/final、実アプリの品質・性能を証明しない。
4. 実装後は研究 fixture と製品経路を同条件で比較し、D3D12/Vulkan、透明境界、HDR、パラメータと保存、実画像、時間方向、性能とメモリを検証する。ビルド・テスト・CMake および GUI 操作はリポジトリ指示に従いユーザーの明示指示まで行わない。

## 2026-11-18 研究フォルダ分離と Artifact build tree 棚卸し

`CreativeEffectsLab` は `J:\dev\CreativeEffectsLab` にあり、独立した Git root（現在 branch `main`）。ArtifactStudio の `.gitmodules`、`Artifact/CMakeLists.txt`、親 CMake から submodule・source・build dependency として参照されていない。研究の Python、capture、依存は製品ツリーに含めず、兄弟 workspace のまま保つ。ラボの研究ファイルは現在未コミットで、Git branch `main` 上の作業ツリー変更として残る。

Artifact child の最上位 source tree は `App`、`src`、`include`、`shaders` などが Git 管理される。`DockFoundation` は `Artifact/CMakeLists.txt` の `add_subdirectory(DockFoundation)` と public source registration から使われるため未使用ではない。ソースフォルダは参照・登録を確認せず削除しない。

未追跡生成物では `Artifact/out/` が `.gitignore` 対象で、合計 23,399,953,574 bytes（約21.79 GiB）。内訳は `out/build/x64-debug` が 12,608 files / 23,050,783,563 bytes（約21.47 GiB）、`j-cli-vs17` が 1,690 files / 348,440,914 bytes、`j-cli-debug` が12 files / 729,097 bytes。`CMakePresets.json` は `out/build/${presetName}` を標準 binaryDir とし、`x64-debug` は標準 preset 名で Visual Studio workspace state にも列挙されるため保持対象。`j-cli-*` は現在の preset に存在せず、workspace state にもなく、Artifact child 内で一致する実行中 build process は見つからなかったので古い build cache の整理候補とする。`.vs` は 16 files / 622,376,262 bytes（約0.58 GiB）だが IDE state/cache を含むため今回の削除候補にしない。

`j-cli-*` の再帰削除は自動承認レビューに拒否されたため実行していない。Artifact の作業ツリーには別件の既存変更・Contour Poster 実装が残っており、削除操作の代替手段も使わない。正確な対象・容量・標準 preset の保持根拠を記録した状態で、クリーンアップは未完了として残す。親の `build/` は現在の MSBuild solution/build 出力であり別管理対象、研究 capture/もそれぞれの repository に残す。

追加の静的 source inventory では cleanup 前の tracked `src/` / `include/` の C++ source/module 1,447件のうち、13件が現行の `Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の名前照合に現れなかった。その後、別 provider と重複する `src/Render/ArtifactTextRenderTarget.cpp`、0 byteで未登録の `include/Image/ImageF32x4_RGBA.ixx` を削除した。さらにユーザーの未使用ツリー整理依頼に基づき、Color Wheels wrapper（`.ixx/.cpp`）、Stabilizer wrapper（`.ixx/.cpp`）、旧 `ArtifactTimelineClock`（`.ixx/.cpp`）、無参照 inline helper `Mask/RotoBridge.ixx` を削除した。7ファイルは全て現行CMake source manifest未登録で、Artifact/ArtifactWidgets内に定義外からのmodule/type参照がなく、生成済み Artifact project filesにも登録されていない。Color Wheels の ArtifactCore providerとColorCorrection effect、ArtifactCoreのVideo Stabilizerは変更していない。Timeline transportの唯一の所有者は `ArtifactPlaybackService` とする既存方針にも沿い、未使用旧clockだけを除いた。

削除しなかった残りは、Inspector/`ArtifactEffectService` から明示importされる `ImageMorphEffect.ixx/.cpp`（source manifest未登録のため別途build registration調査が必要）、legacy compatibility shimと明記された `ArtifactTestProjectService.ixx`、テスト資産として保持する `ArtifactTestRenderQueue.cppm/.h`。これらは単に未登録という理由で削除しない。

削除後、既存の生成済み project `build/Artifact/ArtifactTextRenderTargetRuntime.vcxproj` を `Debug|x64`, `BuildProjectReferences=false` で build し、`ArtifactTextRenderTarget.ixx` から static library が成功生成された。CMake 再実行・tests・アプリ実行は行っていない。この build は `.cpp` 削除後も専用 target が残ることを確認する範囲であり、他の構成や製品 runtime は検証しない。

空 placeholder 削除後、既存生成済み `build/ArtifactCore/ArtifactCore.vcxproj` の Debug/x64 build も成功し、`ArtifactCore.lib` を再生成した。これは実 provider 側が現在の build に残る確認であり、別ターゲット・アプリ runtime・テストは含まない。

アイコン資産は tracked `Artifact/App/Icon` 1,890 files / 4,041,807 bytes（3.85 MiB）。そのうち CMake filter 対象の SVG/PNG/JSON は1,882 files / 3,927,723 bytes（3.75 MiB）で、`ArtifactAppRuntime` Qt resourceへ追加される。アイコンは filename による動的参照もあるため単純なコード全文字列検索だけで未使用判定しない。ディスク量は小さいため今回の容量整理対象にはしない。

## 2026-11-18 Contour Poster 実験導線と CPU 照合

`Artifact` の既存 `PosterizeEffect` module を使い、Experimental Contour Poster を factory と Inspector effect catalog に登録した。Composition JSON decoder にも復元 factory を加え、既存の `Levels` property serialization を使えるようにした。共通 property setter は基底 effect に委譲し、mix/mask/region を共通適用段階へ渡す。CPU fallback は linear premultiplied RGBA を logical RGBA view 経由で処理し、近傍読み取りは immutable source、書き込みは destination に分ける。別の画像サイズの作業バッファは追加せず、既存の `dst = src` 複製を利用する。Generic resident shader snapshot は研究側生成 script から更新し、元 HLSL SHA-256 を埋め込んだ。通常レイヤーの resident generic 経路で実行可能な候補だが、調整レイヤー経路、GPU runtime、fallback 条件や実アプリ到達は未検証である。

`ArtifactPosterizeEffectContractTest` を Debug でビルドし、共通 mix property と fixture capture を含む7 contract case が通過した。linear premultiplied RGBA float32 の人工 fixture 48x64 を CPU `applyCPUOnly` と Python `contour_sqrt_smooth` で比較し、alpha > 1e-6 の 2,240 px で RGB 最大差 2.98e-8、p99 2.98e-8、alpha 最大差 0、non-finite 0。これは単一 fixture での CPU 数値照合であり、画質・GPU parity・実画像・D3D12/Vulkan・時間安定性・性能の合格を示さない。証跡は `captures/contour-poster-product-cpu/` にある。`ArtifactAppRuntime` 全体 target は複数ソースの `Animation.Transform3D` IFC import で MSVC C1116 (`stop_token(248,6)`) により失敗した。一方、変更した `ArtifactAbstractComposition.cpp` は後続の選択ビルドで個別コンパイルできた。app runtime link・JSON round-trip runtime は未確認。

## 2026-11-18 Contour Poster の通常レイヤー経路を静的追跡

`ArtifactCompositionRenderController.cpp::buildGpuRasterEffectPlan` は adjustment layer を除外する一方、通常 layer の enabled Rasterizer / Spatial effect について `appendGpuSpatialNodes` の Generic node を plan に保持する。layer の rasterizer effects を defer した呼び出しで layer surface を描画し、`prepareGpuLayerForBlend` から `RenderPipeline::applySpatialEffect` へ渡す。Generic handler は key から登録 shader record を引き、filter input と scratch/output を分離して dispatch し、成功後に output へ GPU copy する。Mix・region・mask は node 後の共通 composite plan に載る。従って登録コードと静的な通常レイヤー呼び出しは接続していると判断できる。

この経路は source code の制御フロー確認であり、shader の実コンパイル成功、record lookup 成功、D3D12/Vulkan dispatch、色 descriptor、CPU fallback への遷移、preview/final の描画結果を実証しない。Adjustment layer 用 spatial path の現行 preflight は SeparableGaussianBlur のみ許可するため、Contour Poster はその GPU path の対象外。実アプリで通常レイヤーへの追加・保存/再読込・描画を確かめるまでは製品到達性を runtime verified としない。

## 2026-11-18 Contour Poster の生成・保存経路

`ArtifactEffectService` の `effect.colorcorrection.contour_poster` factory と `InspectorEffectCatalog` は同じ stable ID を使う。Composition の `serializeEffect` は effect ID と `getProperties()` の値を出力し、editable property の expression/keyframes/envelopes も存在すれば追加する。`deserializeEffect` は同 ID から `ExperimentalContourPosterEffect` を作り、保存 property を `setPropertyValue` に戻す。Contour Poster は `Levels` を専用 setter に通し、共通項目は基底クラスへ委譲している。この静的経路は追加・保存・復元の構造を接続している。

ただし Composition の JSON round-trip を直接検証する既存契約テストは今回見つからず、現在の `ArtifactPosterizeEffectContractTest` は effect の node / CPU mix / fixture capture を対象とする。対象 translation unit の個別コンパイルは成功したが、全体の `ArtifactAppRuntime` は他 translation units の `Animation.Transform3D` IFC C1116 で失敗し、保存・再読込値の実測も未確認。したがって保存互換性の合格や実アプリの復元成功は主張しない。

2026-11-18 の追加ビルド確認: `MSBuild.exe build/Artifact/ArtifactAppRuntime.vcxproj /t:ClCompile /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:SelectedFiles=<ArtifactAbstractComposition.cpp> /p:SelectedFilesBuildModules=false` で当該 translation unit を切り出した。最初は古い `ArtifactVideoLayer.ixx.ifc` と現行 `ArtifactAbstractLayer` の不一致により C3668 を出したため、同じ選択ビルド機構で `ArtifactVideoLayer.ixx` を先に再コンパイルし、その後 `ArtifactAbstractComposition.cpp` はエラーなく object を生成した。object の更新時刻は 2026-11-18 04:32:04。これは Composition factory 変更を含む TU のコンパイル確認であり、全 `ArtifactAppRuntime` build、link、project save/load、JSON round-trip は証明しない。全体 build の C1116 は別 translation units で継続している。

同じ選択ビルドで `ArtifactAbstractLayerTransformPersistence.cpp` も MSVC 18.11 で個別コンパイルできた。全体 `ArtifactAppRuntime` を `/m:1` にしても、`ArtifactAbstractLayerBounds.cpp`、`ArtifactLayerDefaultsSupport.cpp` など複数の importer で `Animation.Transform3D` / `stop_token` C1116 が再現した。従って並列数だけでは説明できない。これらの TU は単体では成功するものもあり、全体の module/import 構成または MSVC IFC import 相互作用が残るが、具体的な根因は未確定。`ArtifactCore` のソース変更は許可範囲外のため行っていない。

## 未解決

インクの2入力接続と GPU 経路、等高線の近透明・HDR・画素幅と subpixel 安定性、実画像での見た目、reviewer、性能予算、製品側の導線は未確定。研究結果の実装・統合完了は主張しない。

## 2026-11-18 通常 Debug ビルド再確認

ユーザー許可の下、既存の Visual Studio 生成済み `ArtifactAppRuntime.vcxproj` に対して `MSBuild.exe /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1` を実行した。CMake configure は行っていない。依存の再構築を含み、`ArtifactCore`、`ArtifactWidgets.dll`、Contour Poster を含む `ArtifactEffectsColor.lib` は生成できた。一方 `ArtifactAppRuntime` は複数の Layer translation unit で MSVC 18.11 の C1116（`Animation.Transform3D` IFC import 中、`stop_token(248,6)`）を再現し、target 全体は失敗した。これは前回の失敗と同じ阻害要因で、Contour Poster の Color effects library までのビルド成功をアプリ全体の compile/link 成功とは扱わない。CMake 再生成、テスト実行、ArtifactCore ソース変更は行っていない。

全体ビルド後に Generic resident prelude と shader body を連結して DXC に渡し、D3D12 用 DXIL / Vulkan 1.2 用 SPIR-V のコンパイルを確認した。初回は HLSL 予約語 `line` を変数名に使っていたため失敗し、shader を `lineStrength` に修正して研究所の `prepare_contour_resident.py` で resident snapshot と source SHA-256 を同期した。修正後は両 compiler target が成功し、SPIRV-Tools `spirv-val` の Vulkan 1.2 検証と DXC `-dumpbin` も通過した。既存 `ArtifactEffectsColor.vcxproj` の Debug Build は `ArtifactEffectsColor.lib` を再生成して成功した。入力・compiler版・source/output hash・DXIL/SPIR-V binary・validator 結果は `captures/contour-poster-resident-shader-compile-20261118/manifest.json` に保存した（Git管理外）。これは shader のコンパイル可能性とライブラリ生成の証拠に限り、Diligent runtime dispatch、D3D12/Vulkan GPU画像、CPU/GPU parity、実アプリの表示や品質を証明しない。

## 2026-11-18 CC0 写真の線強度レビュー

過去の CC0 crop capture は共有 renderer hash が現行コードと異なっていたため、既知hashの A/B 入力を現行 `photo_study.py` で再生成し、さらに `line_strength_sweep.py` で平方根量子化の線強度 0.35/0.50/0.65/0.80 を比較した。共通ROIがcrop Aの壁を切り出していたため顔・髪 [160,64,256,160] と車体・輪郭 [80,32,176,128] に分け、全体と3倍ROI画像を作成。0.35はcrop Bの車・服柄への線の干渉を弱めたが、細部の混雑は残る。線強度0.35対0.80でpixel-wise max-channel RGB error>0.01となる画素は顔・髪ROIで690/9216 (7.49%)、車体ROIで2584/9216 (28.04%)。応答は素材領域で偏る。研究ノートに定量差、静止画のみの目視所見、未検証項目を記録。暫定判断は要修正・採用保留で、現行固定強度を品質確定値としない。証跡は `CreativeEffectsLab/captures/contour-poster-line-strength-sweep-20261118-roi/manifest.json`。これはPython静止比較であり製品GPU出力・実アプリ評価ではない。

## 2026-11-18 CC0 geometry sweep

線強度を0.5に固定して levels=3/4/6/8 と距離半径=0.50/0.75/1.00/1.50pxを個別比較。levels=3/4は顔・車cropの暗部を広く黒化し、8は細かな点状輪郭を増やす。半径0.75pxは1pxより細部への干渉を抑える候補に見えるが、まだ2 cropのみの静止評価である。暫定判断はlevels=6を比較基準として保持、半径0.75pxを追加検討し、製品値は未決。証跡は `CreativeEffectsLab/captures/contour-poster-geometry-sweep-20261118/manifest.json`。既定Python出力は以前の2 cropとbitwise一致。GPU・HDR/alpha境界・時間・実アプリ・性能は未検証。

各候補の alpha 最大誤差は0、non-finiteは0。研究機上の256x160 Python medianは約2.7–2.9msであり、製品GPU性能の根拠ではない。geometry manifest は現在の `geometry_sweep.py` と `temporal.py` のhashを記録する。更新後inventoryでは本runと alpha-dense run は現行hash一致、line-strength比較・alpha sweep・epsilon model 等の一部は旧hashで `needs review`。既定値の2 crop出力はbitwise一致を直接照合したが、旧runのcode hashは現行ではない。hash inventoryは品質評価ではない。

## 2026-11-18 インク候補の接続調査

静的検索範囲 `Artifact/src`・`Artifact/include`・`tests` では、`applyNLETransition` の呼び出しは同じ `ArtifactNLETransitionBridge.cpp` 内の `applyNLETransitionByName` だけで、`applyNLETransitionByName` は宣言と定義以外に呼び出しを検出しなかった。`TransitionFactory::instance().create` は同 bridge と `tests/ArtifactTestTransitionBridge.h` で確認できる。これは検索範囲内の静的所見であり、動的な登録・外部呼び出しの不存在や実行時の到達不能を証明しない。

bridge の `makeDecodedFrame` は float を 0〜1 に clamp して RGBA8 の配列に変換し、transition を生成・CPU処理して画像に戻す。したがって、この bridge をそのまま研究候補の HDR float 結果の実行経路と見なすことはできない。コードの RGBA コメントと現行 channel-order 契約の整合、alpha、呼び出し頻度、実際の経路は別途検証する。新たに CPU fallback のみを製品経路へ繋ぐ方針は GPU 優先・hot-path 規則と品質要件を満たす根拠がない。

次は現在のコンポジションレンダーで2入力を保有する箇所と transition の駆動元を特定し、GPU 側の入力寿命・色／alpha・時刻・資源再利用の設計を明らかにする。研究候補の品質判定および実 renderer の比較は引き続き未実施。

## 2026-11-18 GPU の2入力候補の制約

`ArtifactRenderLayerPipeline.cpp::applySpatialEffect` の Generic filter は `g_InputTexture` と、`historyFrameOffset != 0` の時だけ `g_HistoryTexture` を bind する。後者は同一 layer の近傍 frame という既存契約で、履歴が無効な場合は現在画像を代用し `historyValid` を false にする。研究の A/B は同じ layer の時間履歴ではなく、transition の左右素材なので、その slot に別素材を渡せると仮定しない。`params.time` と `params.frame` はこの箇所で0に設定されており、transition progress が自動的に渡るとも言えない。

Filter の出力は scratch UAV を経由して output へ copy される。資源再利用と Diligent の state transition の既存動作はコード上にあるが、2入力の所有権、同期、異なる frame の取得・寸法・alpha・進行値、seek と stale rejection は未設計。D3D12 と Vulkan の双方で実行と画像品質を評価する必要がある。GPU frame time・メモリの測定は未実施。これらの制約から、既存 history 経路を流用する実装は現段階では決めない。

## 2026-11-18 composition timeline の既存駆動

`ArtifactAbstractComposition` の `CompositionTimelineTransition` は kind、easing、左右の clip **name**、range、enabled を持ち、`timelineTransitionProgressAtFrame` は range 内の progress と easing を計算する（duration<=1 は1）。`ArtifactCompositionRenderController.cpp` の layer pass はこの値を読み、`isOpacityOnly()` が認める Cut / Crossfade / Dissolve について layer 名と左右の name を照合し、各 layer の opacity を変更する。静的に確認したこの処理は各 layer の通常描画への opacity 適用であり、研究候補が必要とする A/B の画像を同時に受け取る GPU pass は確認できていない。

別の NLE `Transition` は `leftClipId` / `rightClipId` を持つ。2つのモデルを同一視せず、どちらを対象にするか、name の重複・rename・保存後の参照、左右の可視性／時間範囲・順序、複数 transition の競合、片側不在時の意味を確定する必要がある。現行 `timelineTransitionAtFrame` は一致する最初の enabled transition を返す。研究の31 frame で固定した progress 端点との対応も製品 range と照合する。

この追跡はソースの静的調査であり、実アプリの挙動、すべての描画 path、保存互換性を証明しない。2枚の GPU resource を生成・保持する箇所と、render graph に混ぜる位置は未確定。品質・資源と D3D12/Vulkan の runtime 比較も未実施。

## 2026-11-18 ディザ溶解研究と既存 Dissolve の差

研究 `CreativeEffectsLab/studies/dither-dissolve/NOTES.md` の人工 256x160・31 frame では、固定 Bayer 4x4 に coverage が変わらない区間が30区間中14、固定 Bayer 8x8 は0だった。8x8 の隣接増分は0.03125〜0.046875。静止 crop では周期的な点模様が格子・斜線と重なり、固定乱数は不規則な粒として見える。研究は端点・alpha・mask単調性を示すが、連続再生の品質は未評価。

現行 `ArtifactCore/include/Video/Transitions/DissolveTransition.ixx` は `frameIndex + fps` から seed を作り、毎フレームの閾値場を変える。研究側 `temporal_hash.py` の式モデルでは31 frameの隣接比較で B→A の反転合計229,215画素（最大12,232画素/frame）を観測した。これは製品のframe index供給や実 renderer 出力を捕捉した結果ではないが、固定場候補との重要な差である。さらに `Artifact/src/Layer/ArtifactNLETransitionBridge.cpp::applyNLETransition` は線形float画像を clamp して RGBA8 化し、CPU transition を処理するため、HDR float品質やGPU/実時間経路の根拠にならない。

Composition timeline の `Dissolve` は現状 `isOpacityOnly()` の対象で、2枚の layer を通常合成する opacity transition である。研究の同一座標 A/B を閾値で選ぶ遷移と同じではない。固定 Bayerを本体へ載せるには2入力を同時に扱う専用処理、進行・alpha・blend規則、resource lifetime と GPU実行の設計が必要で、NLEの `DissolveTransition` 自体を変更するだけでは Composition Viewport の候補到達を証明しない。transition 実装を所有する `ArtifactCore` の編集は現セッションで許可されていないため未変更。採用前に既存 path の実出力・連続再生を測り、静的周期と時間変化の候補を同条件で比較した上で、許可されたモジュール範囲とGPU設計を決める。

## 2026-11-18 描画順と研究式の違い

`ArtifactCompositionRenderController.cpp` の GPU layer pass は `layerRTV`、`layerSRV`、`layerFloatSRV/UAV`、`accum`、`temp` を取得し、各 layer を順に処理する。既存の opacity-only transition は layer の opacity を補正して通常合成に通し、コメント上も複雑な kind は専用 GPU pass まで metadata とされている。`docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md` は layer を1枚ずつ描き、layer-local mask/matte 後に accum と blend する設計上の契約であり、文書の形式・alpha の記述は現行実装を改めて照合する必要がある。

研究の `A*(1-m)+B*m` は premultiplied 画像 A/B の補間である。2 layer の opacity をそれぞれ `1-m` と `m` にして順に over 合成すると、A は B の alpha に応じて追加で遮蔽されるため、一般に研究式と一致しない（不透明 A/B・m=0.5 でも A の係数は0.25、Bは0.5）。現行 opacity 経路を単に空間 mask に置換すれば研究出力になるとは主張できない。背景との合成、各 layer の opacity/mask/matte/effect、非 Normal blend、間に別 layer がある場合の順序と、両 layer の寄与を二重描画しない規則を決める必要がある。

これは数式と静的コードの調査であり、実際の blend pipeline の全分岐や visual parity の検証ではない。対象 backend、資源確保・再利用、2入力の同時寿命と transition 終端、欠落素材の振る舞いも引き続き未検証。

## 2026-11-18 等高線候補の接続調査

`Artifact/src/Effects/ColorCorrection/ColorBalanceEffect.cpp` は constructor で generic resident shader を登録し、`appendGpuSpatialNodes` で固定16要素の parameter 配列を埋める例である。`Artifact/include/Effects/ArtifactAbstractEffect.ixx` の node stack は固定容量8で、Generic filter の shader registry により render pipeline から検索される。等高線の単一入力候補には構造上参考になり得るが、近傍参照・alpha・元画像の端処理、色 descriptor、parameter 保存、既存 shader prelude が必要な契約を満たすかは未確認。

着手対象の候補は `Artifact` の effect 実装・宣言、effect service の登録と必要な module 登録、および研究資産の一致を示す資料。追加の種類やファイルを確定する前に既存登録と shader 実例の caller を読む。品質判定が未完了であるため、研究の近透明対策を実証済みの完成効果とは扱わない。Diligent の D3D12/Vulkan 両 backend、HDR、alpha、境界、保存・復元、時間安定性、割当・GPU時間を個別に確認する。ビルド・テストの実行には明示指示が必要。

## 2026-11-18 3研究の反映判断台帳

| 研究 | Python で得た根拠 | 製品反映の主な未解決点 | 判断 |
|---|---|---|---|
| インク拡散 | 人工2素材で31 frame、端点・alpha・マスク単調性を確認。coverage再マッピングは固定場の開始・終了の停滞を減らす | A/B GPU資源の同時所有、描画順、blend/mask・layer参照、HDR、連続再生の見た目 | 実装候補。品質合否・製品経路は未検証 |
| 等高線ポスター | 人工素材で平坦部の偽線を比較。hidden RGB反例と近透明での重み正規化の不連続を検出し、固定分母候補で該当 fixture を再評価 | premultiplied→直色の近透明処理、連続変化、視覚品質、GPU処理のコストと保存/UI | 実装候補。固定分母も製品品質未検証 |
| ディザ溶解 | Bayer 4×4 と 8×8・固定乱数の31 frame比較。8×8 は人工条件で coverage 停滞0。既存 Dissolve の式モデルは frame ごとに選択が反転する条件を示した | 既存経路の実出力、固定パターンの見た目とモアレ、2入力・alpha、保存・UI、GPU | 実装候補。モデルは製品挙動や知覚品質の証明ではない |

## 2026-11-18 CC0 全frame filmstrip の追加確認

CreativeEffectsLab に全31 frameを1枚へ並べる静的filmstrip生成器を追加し、Ink Reveal と Dither Dissolve の CC0 capture を一次確認した。Ink の noise は初期から画面全域の孤立領域が現れ、radial は中央起点の順序を維持し coverage equalization が初期の可視量を変える。Dither の Bayer は規則的な周期、random は粒子状selectionが全区間に現れる。証跡は `CreativeEffectsLab/captures/sequence-review-20261118/` にあり、capture manifest・generator・contact sheetのhashをsidecarへ記録する。縮小静止filmstripによる確認で、連続再生・知覚的flicker・モアレ・実時間速度の合否は未判定。製品反映の採否は従来どおり保留する。

研究側の計画・Python・ノートは独立 `CreativeEffectsLab/studies/` にある。現時点で作業ツリーは未コミット、生成 captures は Git 管理外であり、上記の数値だけでは第三者が元の成果画像を取得できない。採用前に研究リビジョンを固定し、入力・スクリプト・出力 hash、評価者、同条件画像・ROI・時間・性能の証跡を確保する。研究の完了と製品統合の完了は別々に判定する。

## 2026-11-18 GPU シェーダー候補の追加

`Artifact/App/shaders/experimental_contour_posterCS.hlsl` に平方根領域の量子化・位相・alpha重み付き固定分母勾配を使う独立した compute shader 候補を追加した。BGRA の linear premultiplied 入出力、明示的な alpha epsilon と端 clamp、入力と出力を別資源にする契約をコードに記載。研究の Python 式と異なり、alpha が epsilon 以下の中心はそのまま返し、隣接画素は勾配から除外する。この差は未評価であり、正確な parity を主張しない。Artifact の `.gitattributes` に hlsl の LF 指定を追加した。

## 2026-11-18 Line Strength property の研究反映

研究で0.35/0.50/0.65/0.80を比較したが、Artifact候補は0.8をCPU/HLSLに固定していた。`ExperimentalContourPosterEffect` に `Line Strength` property（0〜1、既定0.8）を追加し、CPUとresident node parameter P2を同じ設定値で駆動するようにした。共通 `getProperties` / `setPropertyValue` は既存Composition effect serialization経路から保存対象になる構造だが、実際のJSON round-trip、UI表示、Undo、GPU parameter到達は未確認。既定0.8は旧出力との互換を維持する初期値で、品質を採用した意味ではない。研究側 `compare_product_cpu.py` は値を指定してreferenceを合わせられる。`ArtifactEffectsColor` Debug build成功。最新shader snapshotはDXC 1.9.0.5180でD3D12 DXILとVulkan 1.2 SPIR-Vにcompileし、`spirv-val` / `dxc -dumpbin` が成功。証跡は `captures/contour-poster-line-strength-compile-20261118/manifest.json`。GPU実行・画像・parity・性能は未検証。

## 2026-11-18 Line Radius property の研究反映

CC0 geometry sweepで比較した距離半径0.50/0.75/1.00/1.50pxを、`Line Radius` property（範囲0.5〜1.5、既定1.0）として追加し、CPU距離式とgeneric resident P3を連動させた。radius=1.0は既存の `(1-distance)/0.5` 式と一致するよう定義し、0.75pxを設定して研究条件と揃えられる。既定式同一性は式の静的比較であり、実画像のbitwise同一を測っていない。`ArtifactEffectsColor` Debug build成功、DXC D3D12/Vulkan SPIR-V compileと検証成功。manifestは `captures/contour-poster-line-geometry-compile-20261118/manifest.json`。UI/JSON round-trip、P3 runtime値、CPU capture、GPU実画像、品質・性能は未検証。

## 2026-11-18 調整propertyのUI/serialization静的経路

`InspectorEffectCatalog` の候補IDからComposition effectを作成し、`ArtifactPropertyWidget` のComposition effect sectionは `editableProperties()` を行として渡す。`ArtifactAbstractEffect::editableProperties()` はeffect `getProperties()` の各スナップショットからeditable objectを作成し、共通 editor row builderが値変更をcommitする時に `setPropertyValue()` を呼ぶ。`serializeEffect` は `getProperties()` の各name/type/valueをJSONへ書き、`deserializeEffect` は同じnameごとに `setPropertyValue()` を呼ぶ。よってLine Strength/Radiusのproperty名とsetterは静的な作成・表示・保存経路に接続している。これはcaller pathの読み取りだけで、画面上のslider/range表示、編集操作、Undo、JSON実測、再読込、keyframe、実GPU node値は証明しない。

2026-11-18: range の詳細確認で、数値 editor は `PropertyMetadata::hardMin/hardMax` を参照し、`AbstractProperty::setMinValue/setMaxValue` は別の内部 clamp 値だけを設定することを確認。Contour Poster の Line Strength/Radius に `setHardRange` を追加し、property clamp とUI editor range双方に同じ境界を渡すよう修正した。ArtifactCore は変更していない。修正は静的な型・呼び出し経路に基づくもので、実UIでの上下限操作とJSON round-tripは未確認。

同じ確認で Levels の CPU setter 範囲2〜64と property metadata の欠落も見つけたため、既定6・内部clamp・UI hard rangeを2〜64に揃えた。GPU nodeでは levels/epsilon/strength/radius を P0〜P3 に積み、resident parameter buffer が同indexをコピーする静的経路を確認した。これは実GPU dispatch時に各値が正しく適用された証拠ではない。

（当時の状態。後続記録を参照。）このファイルは当時 factory、renderer、UI に未登録だった。GPU resource・alpha descriptor・nonresident path・fallback と parameter 保存の契約を確定するまで本体から使えない。D3D12/Vulkan での shader compile・画像 parity・実行時間・境界/near-alpha・HDR・時間品質・実アプリは未検証。研究の採否および製品統合は未完了。

## 2026-11-18 shader の近透明遷移を改訂

研究の Python 式モデルで epsilon 直後の中心処理に不連続が現れたため、当時未接続だった shader 候補を epsilon〜2epsilon の smoothstep に変更した。隣接 alpha の勾配寄与にも同じ重みを掛ける。epsilon<=0 または中心 alpha<=epsilon は入力を返す。RGB最大100の人工条件では Python モデルの閾値直後の差が 1.52e-6 から 1.46e-11 になったが、実 shader の結果ではない。色descriptor、alpha、精度、資源、GPU・アプリ経路、fallback・画質・性能は未検証。後続の製品登録状況は後段の進捗記録を参照。

## 2026-11-18 Generic resident の研究アダプター

`Artifact` の既存 `PosterizeEffect` module に `ExperimentalContourPosterEffect` を追加し、生成時に generic resident shader record を登録し、固定パラメータ `levels=6`, `alphaEpsilon=1e-6` の spatial node を供給する。既存 Posterize の設定・実装は変更していない。shader body は `src/Effects/ColorCorrection/ExperimentalContourPosterShader.hpp` に保存し、単独 `.hlsl` から cbuffer を除いて generic prelude の `g_P0/P1` に対応させた snapshot で、元ファイル hash を記載した。shader を変更した際は snapshot の再生成・同期が必要。

（当時の状態。後続記録を参照。）この研究アダプターは当時 factory/catalog 未登録、CPU/nonresident 実装なしだった。効果region・mix・mask・preview/final・D3D12/Vulkan・near-alpha・HDR・時間・性能・shader compile・C++ module/link/runtime は未検証。後続の進捗記録は製品統合完了を意味しない。

## 2026-11-18 resident 色契約と snapshot 管理

現行 `ArtifactRenderLayerPipeline.cpp` は Gaussian 等で `layerFloat` を linear premultiplied と記述し、ChromaKey の境界では logical RGBA と stored BGRA の入出力変換を明記している。これは当該経路の shader ソース上の契約であり、すべての producer、preview/final、全 layer で descriptor が正しいことの実行証拠ではない。新候補も BGRA↔logical RGBA と alpha を意識するが、各 caller の実際の到達・値は未確認。

standalone `.hlsl` を generic resident の prelude に適応したヘッダ snapshot は、独立研究側の `tools/prepare_contour_resident.py` で再作成する手順を用意した。生成時には source hash をヘッダへ記録する。これは同期の補助であり、shader compile・module build・実画像 parity を証明しない。当時は factory/catalog 未登録だった。現在の登録と CPU parity 状況は後段の進捗記録を参照。

## 2026-11-18 接続先の静的確認と公開条件

通常レイヤー向け `buildGpuRasterEffectPlan` は調整レイヤーを除外し、CPU 指定または混在する非対応エフェクト、パス容量超過などでプラン全体を拒否する。spatial エフェクトの mix・領域・マスクは最終ノードに付け、別の合成処理に渡す。一方、調整レイヤーの spatial 経路は現状 `SeparableGaussianBlur` のみに限定され、Generic の等高線ノードは対象外である。したがって、この候補を一覧登録するだけでは通常レイヤー以外やプラン失敗時の利用を保証できない。`ArtifactAbstractEffect::apply` は GPU/CPU の実装が未設定の場合に入力を deep copy するため、研究アダプターはこれらの経路で無変化になり得る。

`ArtifactRenderLayerPipeline` の Generic 実行は入力と scratch を分けて dispatch し、結果を出力 texture にコピーする。初回または shader record 変更時には executor、parameter buffer、shader/pipeline を遅延生成し、以降は entry を再利用するコードである。この初回処理の発生時点、失敗後の挙動、フレーム時間とメモリへの影響は未測定で、ホットパス規則の適合を推定しない。

公開判断前に、通常レイヤーの resident 採用条件と stack 失敗時の実際の制御経路を確認し、非resident および調整レイヤーで効果を失わない実装か、明示的に利用できないと伝える製品仕様を確定する。加えて mix・領域・mask・stack順序、初回と定常の GPU コスト、D3D12/Vulkan と preview/final を実画像で評価する。これはソースの読み取りに基づく調査であり、描画の到達や見た目の実行証拠ではない。
