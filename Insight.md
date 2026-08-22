# Insight Log

未解決の設計判断・runtime 検証待ちだけを記録する。実装済みの局所修正と履歴は `docs/analysis/INSIGHT_ARCHIVE_2026-08-11.md` を参照する。

## 2026-08-22 — 乱数生成の統合（RandomStream統一）

- **関連:** `ArtifactCore/include/Math/Random.ixx`、`ArtifactCore/src/ImageProcessing/OpenCV/*.cppm`、`ArtifactCore/src/Geometry/Fracture.cppm`、`ArtifactCore/src/Generate/StarfieldGenerator.cppm`、`ArtifactCore/src/Animation/KeyframeEditingTools.cppm`
- **事実:** 乱数生成が4系統混在(RandomStream/mt19937+random_device/QRandomGenerator/std::rand)。OpenCVエフェクト群(Noise/Glitch/VHS/Glow)とStarfield/Fracture/Scatterはrandom_deviceまたは固定seed mt19937で決定論性が分断。MpmSolver2Dの「rand()使用」は誤検出(関数名のみ)。
- **対応状況 (2026-08-22):** (1) `Random` singletonにmutex追加でthread safety確保。(2) 8ファイルのmt19937+distributionをRandomStreamへ置換: Noise.cppm(gaussian+range)、GlitchCV.cppm(rangeInclusive+unitFloat)、VHS_CV.cppm(unitFloat)、Glow.cppm(unitFloat)、Scatter.cppm(nextU32)、Fracture.cppm(range、全関数signature変更)、StarfieldGenerator.cppm(regex一括置換で全distribution解消)、KeyframeEditingTools.cppm(gaussian+range、chrono entropy seed)。残るmt19937: TextAnimator(wiggly、既存seed決定論のため互換性維持)、ExpressionEvaluator(randomSeeded、同様)。残るrandom_device: ParticleSystem(TurbulenceForce)、SandSim2D、SoftwareRayTracer(thread_local、適切)、AudioTone。
- **次の確認:** ビルド後、各エフェクトのseed再現性を同一seed+同一入力で確認。TextAnimator/ExpressionEvaluatorのRandomStream移行は互換性ブレーキがあるため別判断。

## 2026-08-21 — ArtifactPr 二重モデル(NLEストア/legacy Demo*)の乖離とメタデータ欠落（未検証）

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`ArtifactPr/include/ArtifactPrEditorEngine.ixx`
- **事実:** ArtifactPr は NLEProjectStore(真のモデル)と legacy Demo*(UI 読み取りビュー)の二重管理で、rebuildLegacySnapshotFromNLE() が一方向同期するが、splitClipAtPlayhead/duplicateSelectedClip/pasteClip/marker系/transition系は legacy 側のみ編集し nleStore_ を更新しない(乖離が蓄積)。通常インポート経路(addMediaToPool)は SourceRef を NLE ストアに登録せず、loadDemoProject 経由のみ固定ダミー(timeBase 30fps 固定・availableRange 0-100000f)で登録。saveProject/loadProject は legacy JSON のみで NLE ストアは非永続化。autoSaveIntervalSec_(エンジン)と MainWindow 固定60秒タイマーが二重管理。DemoClip には enabled フィールドがなく(NLE Clip.enabled は転記されず捨てられる)、トラック solo も合成側で未反映。
- **仮説（未検証）:** シーケンス合成プレビュー(MILESTONE_ARTIFACTPR_SEQUENCE_PREVIEW_2026-08-21)は現状 legacy ビューを読んでいるため split/paste 系操作後に表示が実データとずれ得る。中期的には「legacy Demo* 廃止→NLE ストア直接読み」か「全操作の NLE ストア経由化」のどちらかが必要。SourceRef の実ファイルプローブ(frameSize/timeBase)も未実装で、速度変更クリップのソースフレーム→時間対応が正確でない。
- **次の確認:** ビルド復帰後、split/duplicate/paste 操作直後のプレビュー表示ずれを再現確認する。

## 2026-08-21 — コラボレーション機能の現状とセッションモデル新設（進行中）

- **関連:** `ArtifactCore/src/Collaborate/CollaborationSession.cppm`(新設)、`ArtifactCore/src/Collaborate/CollaborationProtocol.cppm`、`ArtifactCore/include/Network/CollaborationWebSocket.ixx/.cppm`、`tools/collaboration-server/server.js`
- **事実:** WebSocketクライアント(再接続/heartbeat/5メッセージ+rule sync)は完備だがアプリから未接続。サーバーはprojectId毎セッション+操作履歴中継+ロック権限+プレゼンス中継のプロトタイプ。セッションモデル(参加者名簿・ロック台帳・バージョン管理・エコー重複排除)が存在しなかった。operationペイロードはopaque JSONでスキーマ未定義。
- **対応状況 (2026-08-21):** `CollaborationSession`(module `Collaborate.Session`)を新設。トランスポート非依存の純粋状態モデルとして、(1)参加者名簿(join/left/presence、離脱時にpeerロック自動解放)、(2)操作ログ(server版号採番・エコー重複排除・pending local op への版号確定)、(3)ロック台帳(grant/deny/release+local要求pending追跡+isLayerLockedByOther)、を提供。QObject/signal非依存でAGENTS.mdのsignal接続制約を回避、全ロジックunit test可能(`CollaborationSessionTest.cpp` 4ケース+CMake登録)。
- **仮説（未検証）:** 次段階は (1) WebSocket→Session のアダプタ(signal接続のためAGENTS.md設計レビュー要)、(2) operation スキーマ定義(transform/property/layer追加削除のJSON契約)、(3) プレゼンスpayload標準化(カーソル・選択レイヤー・ビューポート)、(4) リモート操作とUndoの統合。
- **対応状況 (2026-08-22):** `CollabPresenceState`(型付きpresence、未知キーraw保持)をSessionに追加。`CollabReview.cppm` 新設(コメント: composition/layer/frameアンカー+スレッド1階層+解決/所有者限定編集削除、提案: Pending→Accepted/Rejected/Withdrawn遷移強制・acceptは操作配列返しのみで状態非変更)。監査でエコー照合キー欠陥を発見→`opSeq`フィールド追加で修正(同一ミリ秒衝突とサーバーtimestamp上書きの双方に耐性、回帰テスト追加)。監査全文は `docs/analysis/COLLABORATION_IMPLEMENTATION_AUDIT_2026-08-22.md`。テスト合計35ケース。
- **対応状況 (2026-08-22 追加):** `CollabOperations.cppm` 新設(operationスキーマ: property.set/layer.transform/layer.add/layer.remove のビルダー+バリデータ、未知type前方互換)。Sessionに `createLocalOperation(CollabOperationData)` オーバーロードと `staleParticipantClientIds(nowMs, timeoutMs)`(heartbeatタイムアウト検出)を追加。テスト合計38ケース。
- **対応状況 (2026-08-22 追加2):** `CollabOperations.cppm` に提案検証パイプラインを追加(`validateCollabProposal`/`acceptValidatedProposal` — 全操作検証通過までacceptを遮断)。サーバー(server.js)の `clientTimestamp` 保持修正(監査#6)。Sessionに `createLocalOperation(CollabOperationData)` オーバーロードと staleParticipantClientIds を追加。テスト合計41ケース。
- **対応状況 (2026-08-22 追加3):** `CollaborationSessionAdapter`(module `Collaborate.SessionAdapter`)新設 — WebSocket信号↔Sessionモデルのpoint-to-point接着(グローバル配線なし、接続はアダプタdtorで全解除)。signal 3種(userJoined/userLeft/remoteLockGranted)にclientIdパラメータを追加(セッションモデルに必須、既存使用箇所ゼロのため破壊的影響なし)。CollaborateターゲットがArtifactCoreNetworkへ依存。テスト合計42ケース(アダプタ経由のinboundルーティング: opSeq往復・roster・ロック・presence・離脱解放)。アダプタのoutbound(sendLocalOperation等)のruntime検証は実サーバー結合時に行う。
- **次の確認:** ビルド・テスト実行後、実サーバー(tools/collaboration-server)に対する結合確認。UI層(presence描画・コメントパネル)は別マイルストーン。

## 2026-08-21 — std代替レイヤー(Core.Artifact*)の充実方針と現状（進行中）

- **関連:** `ArtifactCore/include/Core/Artifact*.ixx`、`ArtifactCore/src/Core/*.cppm`、親 `CMakeLists.txt`(import std gate)
- **事実:** プロジェクトは実験的な `import std;`(C++23 std modules)を149ファイルで使用しており、これがコンパイルエラーの主要源。ユーザーの方針は std 依存の削減で、std再発明と思われた `Core.Artifact*` ファミリーは削除対象ではなく置換レイヤーとして完成させる方向。ハウススタイルは `artifact*` 接頭辞付き関数(artifactExchange/artifactBitCast/artifactCmp*)。import stdファイル内の実際の使用トップは max/clamp(算法~1000)、vector(331)、move/make_unique(~290)、string(117)、function(61)。既存: Array(自己完結vector)/String(SSO)/Span/Variant/Function/Dict(QHashラッパー)/Ptr+Ref/Mutex。
- **対応状況 (2026-08-21):** `Core.ArtifactMath.ixx` を新設(artifactMax/Min/Clamp/Abs/IsFinite/IsNaN/Sqrt/Pow/Sin/Cos/Tan/Atan2/Floor/Ceil/Lround/Llround/Lerp)。ArtifactUtilityに artifactMove/artifactForward を追加。ArtifactPtrに UniquePtr/makeUnique を追加。Arrayに operator[]/data() を補完(at()はOptional返しのまま)。Foundationが全モジュールをexport import。テスト `tests/ArtifactCore/ArtifactFoundationTest.cpp` 新設(Math/Utility/UniquePtr/Array/String/Dict)。未着手: import std 149ファイルの段階移行、unordered_mapの自己完結版検討、chrono/mutex系の整理。
- **対応状況 (2026-08-21 第2回):** `Core.ArtifactAlgorithms.ixx` を新設(artifactSort=heapsort/Find/FindIf/Contains/AllOf/AnyOf/NoneOf/MinElement/MaxElement/Fill/Reverse/RemoveIf/LowerBound/BinarySearch/IsSorted+コンテナオーバーロード)。ArtifactMathに `NumericTraits<T>` を追加。ArtifactUtilityに `Pair`/`artifactMakePair` を追加。Arrayに `StaticArray<T,N>`(std::array代替)を追加。既存の ArtifactHashMap が自己完結チェインバケット実装として完備済みであることを確認(Dict のQHash依存は別経路)。
- **対応状況 (2026-08-21 第3回):** `Core.ArtifactTuple.ixx` を新設(再帰Tuple/artifactGet<I>/artifactMakeTuple/tupleSizeV/等値比較)。StringにASCIIユーティリティを追加(asciiLower/Upper/Trimmed/StartsWith/EndsWith/Split/Join — ロケール非依存)。ArtifactAlgorithmsに Accumulate/Iota/Count/CountIf/MinMaxElement を追加。ArtifactUtilityに `artifactHashCombine` を追加。
- **対応状況 (2026-08-21 第4回):** `Core.ArtifactChrono.ixx` を新設(Duration(ns分解能)+SteadyClock(QueryPerformanceCounter/clock_gettimeのプラットフォーム分岐、秒毎tickキャッシュ)+Stopwatch)。`Core.ArtifactRegex.ixx` を新設(パーサ→AST→バイトコード→明示スタックbacktracking VM。文字クラス/量詞lazy含む/選択/グループ/アンカー/エスケープ対応。ステップ上限400万・プログラム8192命令・グループ9個上限)。replaceAllは $0-$9/$$ 置換対応。
- **対応状況 (2026-08-21 第5回):** `ArtifactSet.ixx` を自己完結HashSetへ全面書き換え(チェインバケット+挿入順イテレーション、load factor 0.75で自動rehash、`HashSet<T>` + `ArtifactSet` エイリアス。旧raw()は削除—使用箇所ゼロ確認済み)。Regexに **lookahead `(?=...)` `(?!...)`** を追加(AST→子ノードをサブプログラムとして別コンパイル、VMはLookahead命令でネスト実行。positive成功時はキャプチャ保持・negativeは常にslot復元)と**後方参照 `\1`-`\9`**(パース時に既出グループ番号検証、VMはバイト一致比較、未設定グループはfail)。非対応: lookbehind・条件分岐・再帰をヘッダ明記。テスト合計31ケース(Set操作/lookahead消費なし検証/negative位置/後方参照繰り返し語/未開放グループエラー)。

## 2026-08-21 — Frame/Timeクラス全面PImplのホットパスヒープ確保（未検証）

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`ArtifactCore/src/Frame/FramePosition.cppm`、`Artifact/include/Layer/ArtifactAbstractLayer.ixx`(L433 currentFrame/inPoint)、`ArtifactCore/include/Property/AbstractProperty.ixx`(L296 evaluateValue)
- **事実:** FramePosition/FrameRange/FrameRate/RationalTime/TimeCode/FrameOffset/FrameTime/Durationが全てインスタンスごとに`new Impl`。プロパティ評価`interpolateValue(RationalTime)`はレイヤー×プロパティ×フレームで呼ばれ毎回ヒープ確保。レイヤーAPIは`currentFrame()=int64_t`と`inPoint()/outPoint()=FramePosition`で混在。対照的にFloatColorは直接メンバ16バイト。RationalTimeの比較は連分数で正確だが、`operator+/−`は`value*(lcm/scale)`展開でfromSeconds(scale=1e7)長尺時にint64オーバーフローリスク、`rescaledTo`は切り捨て(llround非整合)。Durationはほぼコメントアウトの死にクラス、TimeCodeRangeは未接続。
- **仮説（未検証）:** value型化(PImpl廃止)が性能の根本解だがABI影響大。まず実測(profiler)でRationalTime生成がフレーム時間に占める割合を確認し、ホットパス限定でint64オーバーロードやキャッシュを導入する段階的移行が安全。
- **対応状況 (2026-08-21):** `rescaledTo` を半分離れ丸め(round-half-away-from-zero)に変更し、極端な大きさではdouble丸めへフォールバック。`operator+/−` は約分→checkedMul/checkedAdd/checkedNegate(移植可能なオーバーフロー検出)→失敗時 `fromSeconds` 経由のdouble合算、の順に修正。テストは `FrameTimeTest.cpp`(丸め・クロススケール・巨大値安全性)に追加。既存約90呼び出しのうち同一スケール変換は挙動不変、クロススケールは境界±1フレームがより正確な方向へ変化。

## 2026-08-21 — 色域語彙の三重化(Gamut/SurfaceColorPrimaries/ColorSpace)とFloatColor::toLinearのsRGB固定（未検証）

- **関連:** `ArtifactCore/include/Color/ColorGamutConversion.ixx`(Gamut enum+行列)、`ArtifactCore/include/Graphics/SurfaceColorContract.ixx`(SurfaceColorPrimaries)、`ArtifactCore/include/Color/ColorSpace.ixx`(ColorSpace enum)、`ArtifactCore/src/Color/FloatColor.cppm`(toLinear/fromLinear)
- **事実:** 同じ「色域」概念に3つのenum(Gamut/SurfaceColorPrimaries/ColorSpace)が存在。ガムット変換行列はGamut側にのみ実装。FloatColor::toLinear/fromLinearはsRGBハードコードで、17種TransferFunctionを持つColorTransferFunction::encode/decodeとは接続されていない(TaggedColor::toTransferが正規経路として昨日追加)。LabColor/XYZColorはPImplヒープでCore内部のみ使用。FloatColorは直接メンバ16バイトで軽量(FloatRGBA統合の障害は低い)。
- **仮説（未検証）:** Gamut↔SurfaceColorPrimariesのマップ関数を追加しTaggedColorにgamut変換を提供するのが正道。FloatColor::toLinearは[[deprecated]]化してTaggedColorへ誘導。ColorSpace enumはColor.ColorSpace利用者との互換確認後にGamutへ統合。
- **対応状況 (2026-08-21):** `gamutForPrimaries()` / `primariesForGamut()` をTaggedColor.ixxに追加(SRGB_Rec709/DisplayP3/Rec2020/ACES_AP0-1のみ対応。DCI-P3/AdobeRGB/DWG/XYZは対応なしを明示)。`TaggedColor::toPrimaries()` が線形化→Bradford白色点適応済みの `ColorGamutConversion::convert`→再エンコードでgamut変換を提供(transfer維持、unknown transferは無変換通過)。`FloatColor::toLinear/fromLinear` は呼び出しゼロを確認の上 `[[deprecated]]` 化し、実装を `ColorTransferFunction` に委譲して数学を単一源へ統一。テストは `ColorBridgeTest.cpp`(語彙マップ・Rec709↔Rec2020往復1e-4・no-op/通過ケース)に追加。

## 2026-08-21 — Color系のFloatColor/FloatRGBA重複とQColor境界変換の散在（未検証）

- **関連:** `ArtifactCore/include/Color/FloatColor.ixx`、`ArtifactCore/include/Color/FloatRGBA.ixx`、`ArtifactCore/include/Color/ColorConversion.ixx`(HSVColor/HSLColor)、`Artifact/src/Layer/*`(toQColor/toFloatRGBA/colorFromJsonValue等86箇所)
- **事実:** FloatColor(PImpl・Artifact側1425箇所)とFloatRGBA(constexpr・75/112箇所)がほぼ同一のfloat RGBA型として重複。QColor↔float色変換とJSON直列化(colorToJson/colorFromJson系)がレイヤー・エフェクト各ファイルで局所再実装されており、NLE marker色対応でも同様のローカル実装を追加した。NamedColor enum(FloatColor.ixx)は使用箇所ゼロ、FloatColor.ixxの前方宣言 `class HSV;` は未定義でデッド。LabColor/XYZColorはCore内部のみ。色値型に色空間タグはなく、sRGB↔linearのtoLinear/fromLinearのみ。画像側は ImageF32x4_RGBA::colorDescriptor が primaries/transfer/alphaMode/range を保持し値側と分離。
- **対応状況 (2026-08-21):** `ArtifactCore/include/Color/ColorBridge.ixx`(module `Color.Bridge`)にQColor/JSON/hex境界を一元化(toQColor/toFloatColor/toFloatRGBA/colorToJson/floatColorFromJson/floatRgbaFromJson/colorToHexArgb)。JSONは `{"r","g","b","a"}` とhex文字列を受け付け、不正入力はfallback返し。`include/Color/TaggedColor.ixx`(module `Color.Tagged`)に色空間タグ付き値型を追加(SurfaceColorPrimaries/TransferFunction/SurfaceAlphaModeをSurfaceColorDescriptorと同一語彙で保持、toTransfer/premultiplied/straight/surfaceDescriptor)。gamut変換は未実装(次段階)。既存86箇所のローカル変換の一括置換は未実施(ビルド検証後に段階移行)。unit test `tests/ArtifactCore/ColorBridgeTest.cpp` 新設。
- **価値・懸念:** 境界変換の一元化で直列化の微妙な不一致(丸め・alpha既定・16進形式)が解消される一方、FloatRGBA統合はテンプレート/constexpr利用箇所の書き換えが必要で影響大。

## 2026-08-21 — Frame/Time系クラスの断片化とtimecode三重実装（未検証）

- **関連:** `ArtifactCore/include/Frame/*`（FramePosition/FrameRange/FrameRate/FrameOffset/FrameTime）、`ArtifactCore/include/Time/*`（RationalTime/TimeCode/TimeRemap）、`ArtifactCore/include/NLE/Core.ixx`（TimeBase）
- **事実:** 時間表現が6系統存在し相互変換APIが非対称(FramePosition→RationalTimeなし、FrameOffset→はあり)。FrameRateはfloat保持のみで30000/1001を表現できず、`hasDropframe()` は23.976を検出しない。timecode生成/解析が `FrameRange::toTimecode`(ノンドロップのみ)/`TimeCode`(drop対応だがtoStringが';'を出力せずsetFromQStringが';'をパースできない)/`NLE::TimeBase`(round-trip可能)の3重実装で挙動不一致。FrameTimeとFramePositionはほぼ重複しFrameTimeはArtifact側で62箇所現役使用。Frame/Time数学のunit testは存在しない。
- **対応状況 (2026-08-21):** FrameRateに有理数保持(`fromRational/setRationalRate/numerator/denominator/hasExactRational/exactFps`)を追加し、分数文字列・JSONでexact維持。`hasDropframe()` は23.976/47.952も検出。TimeCodeのtoString/toStdStringがdrop時に';'を出力し、setFromQStringが';'をパースするよう修正。`FrameRange::toTimecode` はTimeCodeに委譲しdrop対応。`FramePosition↔RationalTime` 変換と `qHash(FramePosition)` を追加。unit test `tests/ArtifactCore/FrameTimeTest.cpp` を新設。未着手: TimeBase timecodeのTimeCode集約(strict validation維持のため現状分離)、FrameTime統合。
- **仮説（未検証）:** FrameTime統合は使用箇所が多く別段階で機械的に行う必要がある。
- **価値・懸念:** 放置するとUI表示のタイムコードが経路ごとに食い違い、NLE統合時にレート変換誤差がクリップ位置ずれとして表面化する。一方FrameRateの内部変更は全レート比較コードに波及するため段階的導入が必要。
- **次の確認:** FrameRate有理化→timecode集約→FramePosition/RationalTime変換→unit test新設の順で段階実装するかを決める。

## 2026-08-21 — 2Dリグ利用導線の3箇所の断絶（skinMesh生成・ボーン追加・キーフレーム）（未検証の改善案）

- **関連:** `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`（handleCreateRig L2414）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（RigSelect/RigWeight 入力 L22579 / ウェイト操作 L17620 / オーバーレイ L38485）、`Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`（skinMesh JSON L320）、`ArtifactCore/include/Rig/Rig2D.ixx`（createSkinMesh L803）
- **事実:** リグ導線は実装済み（Layer→リグレイヤー作成→RigSelect自動切替、Ctrl+Tabでモード切替、ボーン回転ドラッグ＋Undo、Bでウェイト、N/S/M、ポーズクリップボード/スロット）。一方コード検索で (1) `Rig2D::createSkinMesh()` / `setSkinMesh()` の呼び出し元がアプリ側にゼロ（skinMesh は JSON 読込経路でしか存在しない）ため RigWeight・正規化/スムーズ/ミラー・スキン変形描画はユーザーが到達不能なデッド導線、(2) `addRigBone` / `addRigPoint` の呼び出し元は handleCreateRig のみで、VP・右クリック・パネルのいずれにもボーン追加/削除/リネーム導線が無い（RigHierarchyPanel は未実装、VP の HIERARCHY は読み取り専用 HUD）、(3) `Bone2D::evaluate(time)` とキーフレーム API は Core 実装済みだが、ボーンドラッグは静的 localTransform 編集のみでキー追加導線が無く、タイムラインにボーントラック非表示。
- **仮説（未検証）:** P0 は「画像レイヤー→SkinMesh 生成＋autoBind」の導線追加（spec SPEC_RIG_SYSTEM_UI_TASKS §8 相当）。これがないとリグ機能はユーザーから見て何も変形しない。次点で VP ダブルクリック/右クリックでの子ボーン追加、auto-key または手動キー追加＋タイムライン統合。Ctrl+Tab は ShortcutBindings 経由でないハードコードであり、プロジェクトのショートカット整合ルール（AGENTS.md）からも登録先の明示が望ましい。
- **価値・懸念:** Core の評価・Undo・オーバーレイは完成度が高い半分、UI 導線の断絶により機能価値がユーザーに伝わらない。リグレイヤーは solid ベース（opacity 0.18 の平面が合成に残る）で、spec が意図した画像バインド型と食い違う。
- **次の確認:** スキンメッシュ生成導線の設計（対象レイヤー選択 UI、メッシュ解像度、autoBind のボーン距離閾値）、リグレイヤーとソース画像レイヤーの関係（同一レイヤーか参照か）、ボーンキーの auto-key 有無の設計レビュー。

## 2026-08-21 — Text Animator のキーフレーム可視化と Undo 漏れ（未検証）

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm`（`addAnimator` L2212 / `removeAnimator` L2220 / `updateGlyphEvaluation` L4577）、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`（`isTimelineHiddenLayerPropertyGroup` L166）、`Artifact/src/Widgets/Timeline/*`
- **事実:** Text Animator の全プロパティ（Range start/end/offset、position/scale/opacity/tracking/fillColor等21項目）と Source Text キーフレームはキーフレーム化・時間評価・JSON round-trip まで実装済み。一方、(1) `isTimelineHiddenLayerPropertyGroup` が Transform 以外のグループ行を全て隠すため animator のキーがタイムラインに表示されず、用意済みの `displayLabelForPropertyPath` ラベルが標準経路で到達不能、(2) `addAnimator` / `removeAnimator` / Inspector の animator 値編集 / ◆トグル / タイムライン「Add/Remove Keyframe」に Undo push がなく、削除時はキーフレームごと失われる、(3) gpu-glyph パスの早期リターンは `rasterize==true` 条件のため、GPU描画では毎フレーム全再シェーピング＋animator評価＋不要な `animatedGlyphBounds`（per-glyph QPainterPath）が走る。surface cache キーは `animatorCount()>0` で `|frame=N` が付き enabled 依存なし。
- **対応状況 (2026-08-21):** (2) は `SetTextAnimatorStackCommand`（animatorスタック全体のJSON snapshot/restore）と ◆トグル・タイムラインキー操作の `SetLayerPropertyKeyframesCommand` push で解消済み。auto-key連打のUndo化は履歴氾濫のため未実施。(3) はアニメーター無し・Source Text静的な場合にGPUパスでもglyph評価をキャッシュする保守版を適用済み（`applyAnimatorStack` は空スタックで即returnするためフィールド影響なしを確認）。有効animator＋静的値スタックのキャッシュ（Fix B）は、field×transformの時間依存とenvelope検出コストの分析が必要なため未実施。
- **仮説（未検証）:** (2) のUndo対応がデータロス防止として最優先。(1) はAGENTS.mdの「左ペイン標準グループはTransformのみ」ルールと衝突するため、mask/matte型の専用行としての露出を設計レビューで決める必要がある。(3) はGPU glyph評価のキャッシュ（フレームキー＋source/styleキー）で圧縮可能。
- **価値・懸念:** AEユーザーの基本ワークフロー（テキスト→animator→キー打ち→タイムライン調整）のうち、タイムライン調整の導線が事実上欠落している。削除のUndo不可は事故につながる。
- **次の確認:** animator操作のUndo command化、タイムライン専用行の設計判断、glyph評価キャッシュの効果測定の順に扱う。

## 2026-08-21 — GPU blend pipeline のレイヤー毎全画面パスは実測後に融合を判断する（未検証）

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`prepareGpuLayerForBlend` L11572、`blendGpuLayerIntoAccum` L11889、`drawGpuLayerToIntermediate` 内 `renderer_->flush()` L11394、`finalizeGpuRenderToViewport` L12002）、`Artifact/src/Render/GPUTextureCacheManager.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`
- **事実:** 非Normalブレンド・3D・SSGI・マルチチャネル構成のGPUパイプラインは、レイヤー毎に全画面 `convertLayerToFloat`（compute）＋ `blendLayers`（compute、ping-pong）＋ `ctx->Flush()` を発行し、Nレイヤーで 2N+1 回の全画面パスになる。3DレイヤーはAOV分で最大6回 `layer->draw()`。ゲート条件は L32150-32215。Normalブレンドのみの2D構成はフォールバック直描画で対象外。`beginFrameGpuProfiling` / `ProfileScope` / `RenderPerformanceMonitor` が実装済みで計測可能。
- **事実（2026-08-21修正済み）:** `GPUTextureCacheManager::acquireOrCreate` はキャッシュ照会前にフルイメージ変換（QImage→RGBA8888、F32→Rgba32LinearStraight）しており、3Dカード・深度パスの毎フレーム無条件呼び出し（コントローラ L8622, L8699）と合わさってヒット時も全画像変換が発生していた。cache-first peek（`tryAcquireExistingLocked`）とpending早期リターンで解消。`PrimitiveRenderer2D` のスプライトキャッシュpruneもドロー数基準（60ドロー）だったため、高ドロー構成で恒久再アップロードの恐れがあり、pruneサイクル基準へ変更済み。両修正のruntime効果は未計測。
- **仮説（未検証）:** convert+blendのcompute融合、flushのフレーム末尾集約（UAV barrier置換）でレイヤー数に比例するGPUパス時間を圧縮できる。ただし依存関係（レイヤー間のaccum直列化）とDiligentのバリア意味論を要確認で、AGENTS.mdのシビアなコード扱い。
- **価値・懸念:** レイヤー数の多い3D/エフェクト構成でのプレビューfpsとRender Queue時間に直結する。一方、計測前に触ると表示品質リスクが高く、既存profilerでの実測が先。
- **次の確認:** (1) 修正済みcache-first経路の効果を、3Dカードを含む構成でGPUタイム・フレーム時間を実測比較。(2) レイヤー数10/30/60の非Normal構成で `layerToFloatConvertCount` / `blendDispatchCount` とフレーム時間の相関を取得。(3) 融合の要否と安全な導入順（flush集約から先行）を判断する。

## 2026-08-20 — 自動トランジション挿入は再生ヘッド操作の反復ではなく、候補計画の一括適用にする（未検証）

- **関連:** `ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`ArtifactPr/src/ArtifactPrMainWindow.cppm`、`plans/transition-effects-expansion-2026-07-09.md`
- **事実:** `EditorEngine::addTransitionAtPlayhead()` は選択クリップと現在の再生ヘッド位置から隣接クリップを解決して 1 件を追加する。映像トランジションの追加・削除・長さ変更は `TransitionStateCommand` により Undo/Redo の対象となる。既存の `addTransition()` は重複、隣接性、ハンドル長を検証しない。
- **仮説（未検証）:** 複数の編集点に対して同 API を繰り返すと、再生ヘッドおよび選択状態に依存して対象がずれ、部分失敗時に一括 Undo できない。候補を先に固定してから、重複・ロック・隣接性・最小ハンドルを検証し、1 個の state command で適用する bulk operation が安全である。
- **価値・懸念:** 制作時の「選択範囲に既定クロスフェード」を高速化しつつ、既存トランジションの二重配置と意図しない編集を防げる。一方、映像の実際の source handle と render 経路の接続は別課題であり、動画解析や AI 選択を初期範囲に含めるべきではない。
- **次の確認:** `Auto Transition Plan`（候補、採用、スキップ理由、既定長）を純粋な計算として定義し、選択トラック／選択範囲／マーカー範囲の3入力で候補が安定すること、bulk apply の Undo/Redo・保存／再読込・重複回避を確認する。

## 2026-08-15 — Two-panel native dock MVP の境界（未検証）

- **関連:** `Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`、`docs/planned/MILESTONE_INDEPENDENT_DOCK_MANAGER_2026-08-13.md`
- **事実:** `ARTIFACT_NATIVE_DOCK_MVP=1` の opt-in 経路では、Composition Viewer と Inspector を `NativeDockSurface` に登録し、表示、activate、pinned、tab、portable layout の経路を native surface 側へ渡している。通常起動時の QADS 経路は維持している。
- **仮説（未検証）:** 現在の native surface は QADS の central dock 内にホストされるため、MVP は native panel の内部挙動と保存契約を検証できるが、top-level の splitter、floating、drag/drop を含む QADS 置換性までは証明しない。
- **価値・懸念:** 実運用リスクを限定したまま backend-neutral API を検証できる一方、native backend を QADS の代替と表現しすぎると検証範囲を誤認する。MVP の runtime 確認では「QADS 内に埋め込まれた native surface」と「独立 workspace backend」を分けて評価する必要がある。
- **次の確認:** runtime で2面の resize、tab、visibility、portable save/restore を確認した後、native surface を QADS manager の外側へ昇格できる root layout seam を設計する。

## 2026-08-15 — ドック追加メニューは registry facade の UX 層に限定する（未検証）

- **関連:** `docs/planned/MILESTONE_DOCK_PANEL_ADD_MENU_2026-08-15.md`、`ArtifactWorkspaceWidget`、DockManager / dock registry
- **事実:** トップレベル widget architecture は workspace / DockManager をレイアウト所有者として整理中で、各パネルは個別の責務を持つ。
- **仮説（未検証）:** 追加メニューを個別 widget の一覧管理にせず、安定した panel ID を持つ registry facade の薄い UX 層として実装すると、重複 dock、表示名依存、パネル責務の混線を避けやすい。
- **価値・懸念:** 最近使用・お気に入り・再表示を追加しても、Components / Effects / Properties などの専用面を汎用 inspector に戻さずに済む。現行 registry API と保存境界が十分かは未検証。
- **次の確認:** 現行の dock 登録・生成・activate・save/restore 経路を一覧化し、既存 API で Phase 1 の契約を満たせるか確認する。

## 2026-08-14 — 静止画・連番画像の受入ギャップ棚卸し（未検証）

- **関連:** `docs/analysis/STILL_IMAGE_LAYER_ACCEPTANCE_MATRIX_2026-08-08.md`、`docs/planned/MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md`、`docs/planned/MILESTONE_IMAGE_SEQUENCE_WORKFLOW_COMPLETION_2026-07-27.md`、`ArtifactImageLayer`、`ImageSequenceSource`
- **事実:** 静止画は OIIO header preflight、非同期 float decode、入力色解釈、GPU cache、JSON 保存／復元、crop を含む `toQImage()` 境界まで静的実装済みと整理されている。一方、受入マトリクスの IMG-01〜14 と OP-01〜10 は、ほぼすべて実素材・runtime 未確認である。
- **事実:** 連番は Asset Browser の単一素材表示、展開、欠番／読込失敗／relink 診断、Composition 投入時の関係保存、bounded cache、時刻依存の frame switching が実装済みと整理されている。残りは保存／再読込、欠番、範囲外、cache hit/miss、実機性能の検証である。
- **仮説（未検証）:** 次の価値が最も高い作業は新規機能追加ではなく、同一の最小受入素材セットを使って静止画と連番の Preview／Software Preview／Render Queue を比較し、失敗段階を受入表へ反映すること。ここで差異が出れば、source／color／cache／composite のどの境界を直すべきかを限定できる。
- **価値・懸念:** 静的実装済みと制作利用可能を混同せず、動画対応や低レベル backend へ広げる前に、現在の優先対象である静止画・連番画像の品質を測定できる。ビルド・テスト・runtime 検証はユーザー許可が必要なため未実施。
- **次の確認:** 8-bit sRGB、alpha付き、16-bit／float、grayscale、missing／corrupt の静止画素材に加え、正常連番、欠番、範囲外、異解像度、差し替え連番を用意し、(1) frame advance、(2) stale frame 非表示、(3) bounded cache、(4) 保存／再読込、(5) Preview／Render Queue の一致を順に確認する。

## 2026-08-13 — Point2D キーフレームの JSON 復元型（未検証）

- **関連:** `ArtifactCore/include/Property/PropertySerializationBridge.ixx`、`PropertyType::Point2D`
- **事実:** `Point2D` の通常値は JSON object から `QPointF` へ明示復元される一方、キーフレーム値は汎用の `QJsonValue::toVariant()` を通り、object の場合は map 系の QVariant になる。今回確認した Color にはキーフレーム専用の型復元を追加したが、Point2D は依頼範囲外のため変更していない。
- **仮説（未検証）:** Point2D キーフレームを保存・再読込すると、補間側が期待する `QPointF` へ変換できず、値が欠落または既定値化する可能性がある。
- **価値・懸念:** 汎用 Property の位置系アニメーションの保存互換性に影響し得る。既存ファイル形式との互換性を保った局所復元が必要。
- **次の確認:** Point2D のキーフレームを含む最小 round-trip を許可されたテストで確認し、再現時は Color と同様に型別復元を追加する。

## 2026-08-13 — focused pack の module 名検査

- **関連:** `scripts/check_source_manifests.py`、`Artifact/CMakeLists.txt`
- **事実:** 既存 checker は focused pack の path 重複、件数一致、source の存在だけを検証していた。
- **対応:** focused pack ごとに interface の `export module` と implementation の `module` 名を読み取り、pack 間の module 名衝突と interface/implementation の不一致を報告する検査を追加した。
- **価値・懸念:** 異なるファイルに同じ module 名を割り当てる事故を、CMake configure 前の静的検査で検出できる。既存の全モジュールを対象にせず、複数 implementation unit が正当な既存モジュールへ過剰適用しない。
- **次の確認:** 新しい focused effect pack を追加する際に checker を実行し、module 名と source ownership を同時に確認する。

## 2026-08-13 — focused pack target wiring の検査

- **関連:** `scripts/check_source_manifests.py`、`Artifact/CMakeLists.txt`
- **事実:** source set の存在だけでは、対応する CMake target が作られ、両 source list が `target_sources` に登録されていることまでは保証できない。
- **対応:** `ArtifactEffectsColor` を含む全 focused pack を検査対象に戻し、target 名（`SurfaceFX` の大文字略称を含む）、`add_library(... STATIC)`、`target_sources` と module/implementation set の参照を検証するようにした。
- **価値・懸念:** source ownership と target wiring の片側だけが更新される分割漏れを configure 前に検出できる。互換 umbrella（Spatial/Rasterizer/Residual）は focused pack の target wiring 検査から除外している。
- **次の確認:** CMake configure 時に target graph と module BMI 参照が静的 checker の想定どおり解決することを確認する。

## 2026-08-13 — legacy RadialBlur の residual 漏れ

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffectsRasterizer`、`ArtifactEffectsResidual`
- **事実:** legacy Rasterizer-path の `RadialBlur` は Rasterizer umbrella の source list から除外されていたが、residual source list の明示除去には含まれていなかった。
- **対応:** legacy `RadialBlurEffect.ixx/.cppm` を residual の module / implementation 除去リストにも追加した。
- **価値・懸念:** 旧 RadialBlur が canonical な `ArtifactEffectsFinishing` と residual で二重コンパイルされる経路を閉じた。CMake configure / build は未実施。
- **次の確認:** residual の静的評価で module / implementation が空になり、focused pack と重複しないことを確認する。

## 2026-08-13 — focused pack の link 到達性検査

- **関連:** `scripts/check_source_manifests.py`、`Artifact/CMakeLists.txt`
- **事実:** 各 focused pack target が定義されていても、`Artifact` または互換 umbrella から link graph 上で到達できることは別の条件である。
- **対応:** `target_link_libraries` を静的に収集し、`Artifact` を起点に全 focused pack target を辿れるか checker で検証するようにした。
- **価値・懸念:** source が target に登録されているだけで実行ファイルへ伝播しない wiring 漏れを検出できる。実際の CMake target 解決・link order は configure / build 未検証。
- **次の確認:** ビルド許可後に CMake configure と link で、静的 graph と実 target graph の一致を確認する。

## 2026-08-13 — compatibility umbrella の集約検査

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffectsSpatial`、`ArtifactEffectsRasterizer`、`ArtifactEffectsResidual`
- **事実:** focused pack target の到達性だけでは、旧 umbrella 名が意図した pack 群をすべて伝播させることまでは保証できない。
- **対応:** checker に Spatial 11 pack、Rasterizer 8 pack、Residual 全 22 packの期待リンク集合を追加し、umbrella からの欠落を報告するようにした。
- **価値・懸念:** 互換 target の更新漏れによる機能欠落を静的に検出できる。期待集合は現行の責務分割を固定するため、将来の再分類時は同時更新が必要。
- **次の確認:** CMake configure 後に実際の target link interface と静的期待集合を照合する。

## 2026-08-13 — base effect source の ownership 検査

- **関連:** `Artifact/CMakeLists.txt`、`scripts/check_source_manifests.py`
- **事実:** focused pack と umbrella の検査だけでは、元の `ARTIFACT_EFFECTS_MODULES/IMPL` に残った source がどこかの target から除去・移管されたことまでは保証できない。
- **対応:** base effect source と `list(REMOVE_ITEM ...)` の変数展開を静的に追跡し、focused/residual ownership に入らない source を報告する検査を追加した。1 行形式と複数行形式の CMake list の両方に対応した。
- **価値・懸念:** source が無所属になって静かにビルド対象から消える回帰を検出できる。CMake の完全な評価器ではないため、configure / build による最終確認は必要。
- **次の確認:** 新規 effect source 追加時に checker が未移管 source を報告することを確認する。

## 2026-08-13 — app-side effect source の二重所有検査

- **関連:** `Artifact/CMakeLists.txt`、`APP_MODULES` / `APP_IMPL`、`ARTIFACT_EFFECTS_MODULES` / `ARTIFACT_EFFECTS_IMPL`
- **事実:** focused pack source が base effect list に存在しない場合、explicit app manifest 側に残って app target と focused target の二重所有になる可能性がある。
- **対応:** focused pack の全 source が base effect list に所属することと、base list が `APP_MODULES/APP_IMPL` から除去されることを checker で検証するようにした。
- **価値・懸念:** pack 分割後の二重コンパイル・BMI重複の回帰を早期検出できる。CMake の実評価や MSVC module scan は未検証。
- **次の確認:** source 追加・pack移動時に checker が app-side 除去漏れを検出することを確認する。

## 2026-08-13 — focused pack の共通直接依存検査

- **関連:** `Artifact/CMakeLists.txt`、22 focused effect target
- **事実:** source と umbrella の wiring が正しくても、pack target 自身の共通 link dependency が欠けると依存が別 target 経由の偶然に委ねられる。
- **対応:** 全 focused pack target が `ArtifactCore`、`ArtifactRender`、`ArtifactEffectContract` を直接 link しているか checker で検証するようにした。
- **価値・懸念:** pack 単体の再利用性と依存宣言の明示性を保ち、umbrella 経由だけで成立する不安定な link graph を検出できる。個別 effect の追加依存までは自動推論していない。
- **次の確認:** CMake configure / link 後に各 pack の実際の usage requirement と static checker の共通依存が一致することを確認する。

## 2026-08-13 — focused target の C++ module file set 検査

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffects*` focused targets
- **事実:** `target_sources` が source 変数を参照していても、module interface を `PUBLIC FILE_SET CXX_MODULES` として登録していなければ BMI / module dependency graph に入らない。
- **対応:** 全 focused target に private implementation section と public C++ module file set が存在することを checker で検証するようにした。
- **価値・懸念:** source list の参照だけでは不十分な CMake target wiring を検出できる。実際の CMake file-set 解決は configure / build 未検証。
- **次の確認:** CMake configure 後に生成された module dependency graph と各 target の file set を確認する。

## 2026-08-13 — ArtifactCore pack wiring の親側検査

- **関連:** `ArtifactCore/CMakeLists.txt`、`scripts/check_source_manifests.py`
- **事実:** ArtifactCore には 52 個の `ARTIFACTCORE_*_MODULES/IMPL` pack variable と対応する static target がある。子リポジトリの source は今回変更していない。
- **対応:** 親リポジトリの checker から ArtifactCore の pack variable を読み取り、対応 target、`target_sources` の variable 参照、source path の存在を検証するようにした。`AI` / `IPC` / `NLE` / `VST` / `VST3` / `ColorCollection` / `FileSystem` の target 名例外も扱う。
- **価値・懸念:** Artifact 側だけが正しく分割されても Core pack の target wiring が崩れると全体が壊れるため、親の source check で早期検出できる。CMake configure / build は未実施。
- **次の確認:** configure 後に ArtifactCore の実 target graph、module BMI、親 Artifact の transitive link を確認する。

## 2026-08-13 — ArtifactCore Acoustic / Platform の親 link 漏れ

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactCore/CMakeLists.txt`、`ArtifactCoreAcoustic`、`ArtifactCorePlatform`
- **事実:** Core pack target と source set は存在していたが、親 `Artifact` の link graph を静的に辿ると `ArtifactCoreAcoustic` と `ArtifactCorePlatform` だけが未到達だった。
- **対応:** 親 `Artifact` の内部ライブラリ link に両 target を追加し、親・子 CMake を合成した checker で全 52 Core pack target の到達性を検証するようにした。
- **価値・懸念:** Acoustic / Platform module が target 定義だけ存在して実行ファイルへ伝播しない状態を解消した。CMake configure / build による実際の transitive link と module BMI 解決は未検証。
- **次の確認:** configure 後に両 target の link interface と Artifact の最終 link line を確認する。

## 2026-08-13 — ArtifactCore pack の基盤依存検査

- **関連:** `ArtifactCore/CMakeLists.txt`、52 Core pack targets
- **事実:** Core pack が親から到達できても、pack 自身が `ArtifactCore` を直接 link していない場合は、依存が別 target の transitive link に依存する。
- **対応:** 全 Core pack target の `target_sources` に public C++ module file set があり、`ArtifactCore` を直接 link していることを checker で検証するようにした。
- **価値・懸念:** Core pack を単独で再利用できる依存契約を保ち、link 順依存の回帰を検出できる。個別外部ライブラリ依存の完全な推論は行っていない。
- **次の確認:** configure / link 後に各 Core pack の実 usage requirements と static checker の依存契約を確認する。

## 2026-08-13 — 合成 target link graph の循環検査

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactCore/CMakeLists.txt`、`scripts/check_source_manifests.py`
- **事実:** 親 Artifact と ArtifactCore の target link graph は静的評価で 92 nodes / 323 edges、循環 0 件だった。
- **対応:** 親・子 CMake の link edge を合成し、checker に循環検出を追加した。
- **価値・懸念:** pack 分割後に target 相互依存が発生し、link order や module dependency 解決を不安定にする回帰を検出できる。CMake の実 target graph は未生成。
- **次の確認:** configure 後の実 target graph と static graph の循環判定が一致することを確認する。

## 2026-08-13 — ArtifactCore module 重複検査の分類

- **関連:** `ArtifactCore/CMakeLists.txt`、`scripts/check_source_manifests.py`
- **事実:** ArtifactCore の `.ixx` と `.cppm` は同じ named module を正当に共有するため、interface と implementation を一つの重複集合として扱えない。
- **対応:** checker の module 名検査を interface / implementation 別に分離した。実測では両分類とも pack 間の重複は 0 件。
- **価値・懸念:** 正常な interface / implementation 対を誤検出せず、同じ分類内の二重定義だけを検出できる。
- **次の確認:** 新しい Core pack 追加時に同一分類の module 名衝突が checker で検出されることを確認する。

## 2026-08-13 — ArtifactCoreLocalization の cross-pack module reference

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Localization/LocaleFormatting.ixx`、`ArtifactCore/include/Utils/Localization.ixx`、`ArtifactCore/src/Localization/Localization.cppm`
- **事実:** `ArtifactCoreLocalization` は `LocaleFormatting.ixx`（`Localization.LocaleFormatting`）を module interface として登録する一方、implementation set は `Localization.cppm`（`Core.Localization`）を登録している。`Core.Localization` の interface `Utils/Localization.ixx` は `ARTIFACTCORE_MODULES` 側に残っている。
- **事実の補強:** `ArtifactCoreModuleReferences.cmake` に `Localization.cppm|Core.Localization|include/Utils/Localization.ixx` が明示登録され、CMake は implementation に `/reference` と interface object dependency を付与する設計になっている。`TranslationManager.cppm` と `AppMain.cppm` は `Core.Localization` を import する。
- **価値・懸念:** これは通常の同一 pack interface/implementation 対ではなく、base target の interface BMIを分割 implementation targetから参照する特殊経路である。明示 reference が実 configure / MSVC module generation で正しく解決するかは未検証で、現時点で確定バグとは断定しない。
- **次に必要:** CMake configure / build 許可後に `ArtifactCoreLocalization` の `/reference`、interface object dependency、親 Artifact の module BMI 解決を確認する。失敗時のみ pack 境界の再整理を検討する。

## 2026-08-13 — configure-time source scan の残存確認

- **関連:** root `CMakeLists.txt`、`Artifact/CMakeLists.txt`、`ArtifactCore/CMakeLists.txt`
- **事実:** source tree の `.ixx/.cppm/.cpp` を列挙する `GLOB_RECURSE` は残っていない。残存する GLOB は MSVC `modules.json` / Windows SDK の toolchain discovery と Artifact icon resource discovery に限定されている。
- **価値・懸念:** explicit source manifest 化による configure-time source scan 削減の方針は維持されている。SDK/toolchain discovery は環境依存のため別途 configure 検証が必要。
- **次の確認:** configure 後に source manifest が実際の target source と一致し、resource/toolchain discoveryだけが動作することを確認する。

## 2026-08-13 — ArtifactCore explicit module reference の stale entry

- **関連:** `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`、`ArtifactCore/src/AI/CloudAgent.cppm`、`ArtifactCore/src/ImageProcessing/NoiseImageGenerator.cppm`
- **事実:** reference table の `CloudAgent` entry は interface path が `ICloudAIAgent.ixx` で `include/` を欠き、`NoiseImageGenerator` entry は存在しない `Generator.ixx` を指定している。実際の primary interface は `include/AI/ICloudAIAgent.ixx` と `include/Channel/Generator.ixx` で、後者の implementation は `module Generator;` と宣言されている。
- **価値・懸念:** explicit `/reference` の path stale により、configure 後の interface object dependency が誤る可能性がある。reference table は子リポジトリ内のため、編集は明示承認待ち。
- **次に必要:** 承認後に2 entryを実在する interface pathへ修正し、reference table の全 entryで path / module declaration 検査を追加する。
- **切り分け:** Artifact 側には同形式の explicit module reference table は存在せず、この stale path 問題は現在 ArtifactCore 側に限定される。

## 2026-08-11 — Shared render device lease の段階移行

- **関連:** `Artifact/include/Render/DiligentDeviceManager.ixx`、`Artifact/src/Effects/`
- **事実:** shared render device は Diligent smart pointer と独立した手動 refCount を持つ。effect群には acquire/release の非対称な経路が残る。`SharedRenderDeviceLease` を導入し、`InvertEffect` の一時利用を移行した。
- **価値・懸念:** device loss と backend 切替時に共有deviceが解放されないリスクを減らす。永続resourceを持つ effect と一時利用を混ぜて機械移行してはならない。
- **次の確認:** effectごとの所有期間を分類し、leaseへ段階移行した後、shared refCountが0へ戻るruntimeケースを確認する。

## 2026-08-11 — ImageF32 GPU dirty 通知契約

- **関連:** `ArtifactCore/src/Image/ImageF32x4_With_Cache.cppm`
- **事実:** CPU/GPU同期は実装済み。外部GPU passがUAVへ書いた後の `MarkGpuDataDirty()` 呼び出し元は静的検索で0件だった。
- **価値・懸念:** 将来のGPU直書きでCPU readbackを省略すると、古いCPU画像をGPUへ再uploadする可能性がある。readbackは同期的なためhot pathへ増やさない。
- **次の確認:** UAV直書き導入時は同一スコープでdirty通知を必須にし、CPU読取りの頻度をruntime計測する。

## 2026-08-11 — 3D 描画の行列スコープとflush契約

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`
- **事実:** `PrimitiveRenderer3D` はflush時のカメラ行列でキューを送信する。Controller内の3Dレイヤー／Cardは局所RAIIスコープへ移行済みで、Overlay側は個別flushで保護している。
- **価値・懸念:** set/resetとflushの順序依存を減らす。RT/DSV・viewport復元は別責務であり、同一スコープへ安易に統合しない。
- **次の確認:** すべての3D matrix設定経路を静的監査し、複数カメラ・ライト・選択overlayの実機ケースを確認する。

## 2026-08-11 — Viewport shortcut context

- **関連:** `Artifact/src/Widgets/Render/`、`ArtifactCore/include/UI/ShortcutBindings.ixx`
- **事実:** AE式ツール切替とBlender式 `G/R/S` モーダル操作は競合する。
- **価値・懸念:** 単一キーの場当たり的追加を避け、Viewport focus・テキスト入力・専用ツールを区別する入力コンテキストが必要。
- **次の確認:** 変換セッションの状態機械と、`G/R/S`、`X/Y/Z`、確定／キャンセル操作の優先順位を設計する。

## 2026-08-11 — Layer-type property presentation migration

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Layer/ArtifactImageLayer.cppm`
- **事実:** 標準profileは `Initial` / `Transform` のみを表示し、Imageレイヤー固有の `Image` / `Source Reframe` グループを除外していた。Imageと固定Planeを初回対象として明示profileへ追加し、表示順を `Transform` 優先にした。ImageのCrop / Panは未有効時にTransformの追加ボタンだけを表示し、有効化後に専用グループを挿入する。
- **価値・懸念:** 既存のComponents専用面を露出させず、型固有の主要項目を段階的に表示できる。profileはまだWidget側の暫定定義で、モデル側の契約へは未移行。
- **次の確認:** ImageとPlaneの編集・保存再読込を確認後、Text、Shape、Solidの順で同じ最小変更を行う。

## 2026-08-11 — Dock focus outline and current-tab indicator

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Dock/DockGlowStyle.cppm`
- **事実:** PropertiesとTimelineの白い外周はQADSではなく、各widgetに追加された `QFocusFrame` だったため撤去した。QADSの外周Glowを無効にすると、同じフラグで保護されていたcurrent-tab下線も描画されなかった。
- **価値・懸念:** Dock外周Glowなしでもcurrent-tab下線を残せる。QADSのstyle dispatchが `PE_Widget` 以外を通る環境での描画はruntime未確認。
- **次の確認:** Dock領域ごとのタブ切替、非フォーカスDock、floating/re-dock、およびDPI変更後に下線と枠が残らないことを確認する。

## 2026-08-11 — Numeric property focus selection

- **関連:** `Artifact/include/Widgets/ArtifactRelativeSpinBox.ixx`
- **事実:** 数値editorはQt標準SpinBoxの内部LineEditを使用し、focus/通常クリック時にsuffixを除く数値部分だけが自動選択される。共通relative spinboxで自動選択を解除した。
- **価値・懸念:** 値欄はcaret状態で開き、明示ドラッグ等の選択は維持する意図。Tabフォーカスから即時入力する既存操作のruntime挙動は未確認。
- **次の確認:** float/int/rotationのクリック、Tab移動、ドラッグ選択、suffix付き値、相対入力（`+` / `-`）を確認する。

## 2026-08-11 — Runtime verification backlog

- **関連:** GPU effect、Diligent binding、audio/FFmpeg、render job
- **事実:** 多数の防御修正はビルド・実機未確認で、履歴はアーカイブへ移設した。
- **価値・懸念:** 個別の全消化ではなく、device lifecycle、GPU effect、seek/EOS、render jobをまとめた代表回帰ケースが必要。
- **次の確認:** ビルド許可後に代表ケースを定義し、診断ログとともに実行する。

## 2026-08-12 — MultiChannelImage copyFrom後のチャンネル参照

- **関連:** `Artifact/src/Effect/ArtifactCreativeEffects.cppm`、`ArtifactCore/include/Image/MultiChannelImage.ixx`
- **事実:** `MultiChannelImage::copyFrom()` は内部channel mapをclearして再構築する。copyFrom前に取得した `SharedPtr<VideoChannel>` は旧チャンネルを保持し続けるため、処理結果を読む前に `getChannel()` で再取得する必要がある。Creative共通アダプタは再取得するよう修正した。
- **価値・懸念:** Core effectが正しく処理してもArtifact出力が元画像のままになる静かなバイパスを防ぐ。同じcopyFromパターンが別経路にある可能性は未検証。
- **次の確認:** `MultiChannelImage::copyFrom()` の全呼び出し元を監査し、copyFrom前のチャンネル参照を処理後に再利用していないことを確認する。

## 2026-08-12 — Residual Rasterizer effect pack boundary

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/src/Effects/Rasterizer/`
- **事実:** P1〜P3の分割後に残った35組のうち、Rasterizer配下の30組は、インターフェース上で共通契約以外の個別effect moduleを直接importしていなかった。Temporal pack対象は除外し、30組を`ArtifactEffectsRasterizer`へ分離した。
- **閃き・仮説:** ディレクトリ名だけでなく、履歴状態を持つTemporal群とstatelessなRasterizer operator群を別targetにすると、通常のラスター処理の変更が履歴系・色補正系のBMI再構築へ波及しにくくなる。
- **価値・懸念:** 最大の残存`ArtifactEffects` targetを35組から5組へ縮小できる。一方、静的ライブラリのobject pull-in、factoryのlink order、各effectの実装側依存はビルド未検証である。
- **次の確認:** ビルド許可後にCMake configureと代表的effect factoryを含むリンクを検証し、P4 packのBMI境界を確認する。

## 2026-08-12 — ArtifactCore Audio domain boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/cmake/ArtifactCoreSources.cmake`
- **事実:** Core source manifestにはAudio 43 module / 30 implementationがあり、Audio moduleを直接importする非Audio moduleは`Media.Encoder.FFmpegAudioDecoder`と`Particle.System`の2組だった。これらを含めて`ArtifactCoreAudio`へ移し、分割後のCore本体からAudio moduleへのimport edgeが0件になることを静的確認した。
- **閃き・仮説:** domainディレクトリ単位の移動だけでなく、直接importする少数のconsumerを同じpackへ閉じ込めると、base targetが抽出targetへ逆依存する循環を避けやすい。
- **価値・懸念:** Core本体の再コンパイル範囲をAudio変更から切り離せる可能性がある。一方、Qt Multimedia / FFmpeg、MSVC module reference、静的ライブラリのlink順は未検証である。
- **次の確認:** ビルド許可後にconfigure、Audio moduleのBMI生成、FFmpeg decoderとParticle systemを含むリンクを確認する。

## 2026-08-12 — ArtifactCore AI leaf boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`Artifact/src/AppMain.cppm`、`Artifact/src/Widgets/AIChatWidget.cppm`
- **事実:** AI系33 module / 6 implementationは、Core内の非AI moduleからの直接importが0件だった。`ArtifactCoreAI`を追加し、AI moduleを利用するArtifact本体へリンクした。
- **閃き・仮説:** optional backend（ONNX、llama、Python）を含むleaf domainを分離すると、AIコード変更やその依存探索を通常のCore targetのBMI再構築から切り離しやすい。
- **価値・懸念:** AI domainの変更範囲とoptional link依存を局所化できる。一方、実際のoptional backend構成、静的archiveのobject pull-in、MSVC module referenceは未検証である。
- **次の確認:** ビルド許可後にAI packのconfigure、optional backend有無ごとのリンク、AppMain / AIChatWidgetのmodule解決を確認する。

## 2026-08-12 — ArtifactCore Video boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`Artifact/src/Render/`、`Artifact/src/Layer/ArtifactNLETransitionBridge.cppm`
- **事実:** Video系31 module / 20 implementationとFFmpeg video decoderを`ArtifactCoreVideo`へ移した。Video.VideoFrameを直接利用するMedia 3組は、P8のMedia packへ整理した。分割後のCore本体からVideo packへのmodule import edgeは0件だった。
- **閃き・仮説:** frame型を利用するMedia decoder/controllerをVideo pack側へ閉じ込めることで、Video domainを単独targetとして成立させられる。
- **価値・懸念:** Video transition / decoder変更のBMI再構築範囲をCore本体から切り離せる可能性がある。FFmpeg link、Render targetのmodule reference、static archive解決は未検証。
- **次の確認:** ビルド許可後にVideo packのconfigure、FFmpeg video decoder、Render GPUTextureCacheManager、NLE transition bridgeのmodule/link解決を確認する。

## 2026-08-12 — ArtifactCore Media boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`
- **事実:** Media系16 module / 12 implementationを`ArtifactCoreMedia`へ移し、Media.Infoを直接利用する`Codec.FFmpegThumbnailExtractor`も同じpackへ含めた。MediaはVideo frameを利用するため、Media targetはVideo targetへ依存させた。
- **閃き・仮説:** Video frameを利用するMedia controller/decoderをVideo packへ混在させずMedia packへ戻すことで、Video（codec/transition）とMedia（source/playback）の責務境界を明確にできる。
- **価値・懸念:** source/asset/render側のMedia変更をCore本体から分離できる可能性がある。Media→Videoのmodule reference、FFmpeg thumbnail link、static archive順序は未検証。
- **次の確認:** ビルド許可後にMedia / VideoのBMI生成、thumbnail extractor、AssetBrowser、RenderQueueのlink解決を確認する。

## 2026-08-12 — ArtifactCore Composition leaf boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`Artifact/src/Composition/`、`Artifact/src/Layer/ArtifactCompositionLayer.cppm`
- **事実:** Composition系9 module / 8 implementationを`ArtifactCoreComposition`へ移した。Core内の非Composition moduleからComposition moduleへの逆importは0件で、Composition targetはCoreとMediaへ依存させた。
- **閃き・仮説:** Composition buffer / pre-compose / template契約は、基盤Coreから分離しても利用側へ一方向に提供できるleaf domainである。
- **価値・懸念:** project/composition機能の変更時にCore本体のBMI再構築を抑えられる可能性がある。保存形式、Artifact側の複数利用者、static archive link順は未検証。
- **次の確認:** ビルド許可後にComposition packのBMI生成、Project/Layer/RenderQueue利用者のmodule解決とlinkを確認する。

## 2026-08-12 — ArtifactCore Analyze boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Analyze/`、`ArtifactCore/include/Time/TimeRemap.ixx`
- **事実:** Analyze系5 module / 4 implementationに`Time.TimeRemap`を加え、`ArtifactCoreAnalyze`へ6 module / 5 implementationを移した。TimeRemapはAnalyze.OpticalFlowを直接利用するため同じpackへ閉じ込めた。
- **閃き・仮説:** optical-flowや画像解析と時間再マップは、再生・解釈側へ一方向に提供する分析packとして分離できる。
- **価値・懸念:** Analyze/TimeRemap変更時のCore本体BMI再構築を抑えられる可能性がある。FootageInterpretService、CurveEditor、SmartPalette利用側のlinkは未検証。
- **次の確認:** ビルド許可後にAnalyze packのmodule生成、OpticalFlow、TimeRemap、Artifact利用側のlink解決を確認する。

## 2026-08-13 — ArtifactCore Tracking boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Tracking/`、`ArtifactCore/src/Tracking/`
- **事実:** Tracking系3 module / 2 implementationを`ArtifactCoreTracking`へ移した。Core内の逆向き参照はなく、Transformへの一方向依存だけを持つ。
- **閃き・仮説:** motion / planar / camera trackingは、画像・レイヤー処理から独立した解析サービス境界としてCore本体から切り離せる。
- **価値・懸念:** Tracking変更時のCore本体BMI再構築を抑えられる可能性がある。現時点のArtifact側直接利用とstatic link順は未検証。
- **次の確認:** ビルド許可後にTracking packのmodule生成、OpenCV依存、Transform参照、Artifact利用側のlink解決を確認する。

## 2026-08-13 — ArtifactCore IPC boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/IPC/`、`ArtifactCore/src/IPC/`
- **事実:** IPC系3 module / 3 implementationを`ArtifactCoreIPC`へ移した。Core内の逆向き参照はなく、Image型を利用する共有メモリ・render-farm transportのpackとして閉じ込めた。
- **閃き・仮説:** IPC transportは画像処理・レンダリングの実装本体から分離し、必要な利用側だけが明示的にリンクする境界にできる。
- **価値・懸念:** IPC変更時のCore本体BMI再構築を抑えられる可能性がある。隠れたrender-farm利用者とstatic link順は未検証。
- **次の確認:** ビルド許可後にIPC packのmodule生成、Image参照、render-farm利用側のlink解決を確認する。

## 2026-08-13 — ArtifactCore NLE / Playback / Preview boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/NLE/`、`ArtifactCore/include/Playback/`、`ArtifactCore/include/Preview/`
- **事実:** NLE 2 module / 2 implementation、Playback 2 module / 1 implementation、Preview 2 module / 2 implementationを個別packへ移した。Video→NLE、Media→Playbackの一方向参照をtarget linkへ反映し、Previewは逆向き参照なしでArtifact本体へ明示リンクした。
- **閃き・仮説:** 編集形式、再生状態、プレビュー設定は、それぞれ利用側へ契約を提供するleaf domainとしてCore本体から切り離せる。
- **価値・懸念:** NLE / playback / preview変更時のCore本体BMI再構築を抑えられる可能性がある。OTIO、Media再生、Preview設定利用側のmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後に3 packのmodule生成、Video/Mediaの依存解決、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactCore Export / VST3 / Localization / Coordinate boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Export/`、`ArtifactCore/include/VST3/`、`ArtifactCore/include/Localization/`、`ArtifactCore/include/Coordinate/`
- **事実:** Export 3 module / 2 implementation、VST3 1/1、Localization 1/1、Coordinate 1/1を個別packへ移した。ExportはRig、CoordinateはSerializationへ依存し、Artifact側のLottie、VST host、Project Memo利用者には明示リンクを追加した。
- **閃き・仮説:** 形式出力、外部plugin ABI、表示ローカライズ、座標プロファイルは、Core本体へ常時伝播させず用途別packとして保持できる。
- **価値・懸念:** これらの変更時にCore本体のBMI再構築を抑えられる可能性がある。Lottie/VST3/Project Memo/coordinate利用側のmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後に4 packのmodule生成、外部依存、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactCore Event / File / Plugin / Control / Database boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Event/`、`ArtifactCore/include/File/`、`ArtifactCore/include/Plugin/`、`ArtifactCore/include/Control/`、`ArtifactCore/include/Database/`
- **事実:** Event 3/2、File 3/2、Plugin 3/2、Control 3/2、Database 2/1を個別packへ移した。UI/Playback→Event、Asset→Fileの依存をtarget linkへ反映し、Plugin/Control/DatabaseはCore内の逆向き参照なしでArtifact本体へ明示リンクした。
- **閃き・仮説:** event transport、file detection、plugin registry、external control、database storageは、Core本体の共通基盤から分離して必要な利用側だけへ公開できる。
- **価値・懸念:** 各境界の変更時にCore本体BMI再構築を抑えられる可能性がある。UI/Playback/Assetのlink順、外部control/backendの実装条件は未検証。
- **次の確認:** ビルド許可後に5 packのmodule生成、依存解決、Artifact本体のstatic linkを確認する。

## 2026-08-13 — ArtifactCore Mask / Configuration boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Mask/`、`ArtifactCore/include/Configuration/`、`ArtifactCore/include/Application/ArtifactAppSettings.ixx`
- **事実:** Mask 4/4を`ArtifactCoreMask`へ、Configuration 3/2とApplication.AppSettingsを`ArtifactCoreConfiguration`へ移した。UI→Mask、AI/Asset→Configurationの依存をtarget linkへ反映した。
- **閃き・仮説:** mask計算と設定／AppSettingsはCore本体へ混在させず、UI・AI・Assetなどの利用側へ一方向に提供できる。
- **価値・懸念:** mask・設定変更時のCore本体BMI再構築を抑えられる可能性がある。RotoMaskEditor、AI API key、Asset importer、Artifact側設定利用者のlink順は未検証。
- **次の確認:** ビルド許可後に2 packのmodule生成、Color/Grid依存、UI/AI/Asset/Artifactのlink解決を確認する。

## 2026-08-13 — ArtifactCore Text boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Font/`、`ArtifactCore/include/Text/`、`ArtifactCore/include/Shape/`
- **事実:** Font 3 moduleとText 6 module / 5 implementationを`ArtifactCoreText`へ統合し、Shapeから暫定配置のGlyphLayout / TextAnimatorを除去した。Shape→Textの依存をtarget linkへ反映した。
- **閃き・仮説:** font descriptor、shaping、glyph atlas、layout、animatorは単一のText ABI境界として扱う方が、ShapeやRenderの実装へ漏れにくい。
- **価値・懸念:** Text変更時のCore本体BMI再構築を抑えられる可能性がある。FreeType/Qt font、Shape、Render、Artifact text layerのmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後にText packのBMI生成、Shape依存、PrimitiveRenderer/ArtifactTextLayerのlink解決を確認する。

## 2026-08-13 — ArtifactCore Generate / Simulation / Track / Source / Project boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Generate/`、`ArtifactCore/include/Simulation/`、`ArtifactCore/include/Track/`、`ArtifactCore/include/Source/`、`ArtifactCore/include/Project/`
- **事実:** Generate 2/2、Simulation 2/2、Track 2/1、Source 1/1、Project 2/0を個別packへ移した。TrackとProjectはArtifact側の利用者があるため、Artifact本体へ明示リンクした。
- **閃き・仮説:** 生成・シミュレーション・トラック・source abstraction・project metadataは、共通Coreの一部として常時ビルドせずleaf packへ切り出せる。
- **価値・懸念:** 各機能変更時のCore本体BMI再構築を抑えられる可能性がある。OpenVDB、NCC tracker、Project statistics利用側のstatic link順は未検証。
- **次の確認:** ビルド許可後に5 packのmodule生成、Geometry/Image/Memory/Utils参照、Artifact利用側のlink解決を確認する。

## 2026-08-13 — ArtifactCore Scene / Rig / Grid / ColorCollection / Sound / Sequence boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Scene/`、`ArtifactCore/include/Rig/`、`ArtifactCore/include/Grid/`、`ArtifactCore/include/ColorCollection/`、`ArtifactCore/include/Sound/`、`ArtifactCore/include/Sequence/`
- **事実:** Scene 2/1、Rig 1/1、Grid 1/0、ColorCollection 1/1、Sound 2/0、Sequence 2/0を個別packへ移した。Composition→Scene、Export→Rig、Configuration→Gridの依存をtarget linkへ反映した。
- **閃き・仮説:** scene graph、rig、grid、color grading collection、sound/sequence contractsを必要な利用側だけへ提供するleaf境界として扱える。
- **価値・懸念:** これらの変更時にCore本体BMI再構築を抑えられる可能性がある。Composition/Export/Configurationのlink順とColorCollection利用側のstatic linkは未検証。
- **次の確認:** ビルド許可後に6 packのmodule生成、依存解決、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactCore Material / Environment / Light / Crowd / Domain / FileSystem / Icon / VST boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Material/`、`ArtifactCore/include/EnvironmentVariable/`、`ArtifactCore/include/Light/`、`ArtifactCore/include/Crowd/`、`ArtifactCore/include/Domain/`、`ArtifactCore/include/FileSystem/`、`ArtifactCore/include/Icon/`、`ArtifactCore/include/VST/`
- **事実:** Material 1/1、Environment 1/1、Light 1/1、Crowd 1/0、Domain 1/0、FileSystem 1/0、Icon 1/0、VST 2/0を個別packへ移した。Scene→Materialの依存をtarget linkへ反映し、OpenXRはoptional条件を維持するため分割対象から除外した。
- **閃き・仮説:** material、environment、IES、crowd、domain、filesystem、icon、VST契約はそれぞれ小さなleaf packとしてCore本体から隔離できる。
- **価値・懸念:** optional backendや各契約の変更時にCore本体BMI再構築を抑えられる可能性がある。Qt/OS/VST利用側のstatic link順は未検証。
- **次の確認:** ビルド許可後に8 packのmodule生成、Scene/Artifact利用側、optional backend条件のlink解決を確認する。

## 2026-08-13 — ArtifactCore Network boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/NetworkRPCClient.ixx`、`ArtifactCore/NetworkRPCServer.ixx`、`ArtifactCore/include/Network/`
- **事実:** Network 3 module / 3 implementationを`ArtifactCoreNetwork`へ移し、ArtifactRender、ArtifactWorker、Artifact本体からNetwork targetへのリンクを追加した。
- **閃き・仮説:** RPC/WebSocket transportはRenderやworkerの実装本体から切り離し、必要な実行経路だけへ提供できる。
- **価値・懸念:** network transport変更時のCore本体BMI再構築を抑えられる可能性がある。Qt Network、RPC ABI、Render/Workerのstatic link順は未検証。
- **次の確認:** ビルド許可後にNetwork packのmodule生成、RenderFarmMaster、FarmWorkerMain、WebSocket利用側のlink解決を確認する。

## 2026-08-13 — ArtifactCore Collaborate boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/src/Collaborate/CollaborationProtocol.cppm`
- **事実:** CollaborationProtocol 1 moduleを`ArtifactCoreCollaborate`へ移した。既存のReactive.Events moduleを利用するだけで、ReactiveEvents本体は変更していない。
- **閃き・仮説:** collaboration protocolのserialization契約は、凍結中のReactiveEvents実装を動かさず、独立した上位packとして切り離せる。
- **価値・懸念:** collaboration protocol変更時のCore本体BMI再構築を抑えられる可能性がある。Reactive.Eventsのmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後にCollaborate packのmodule生成とReactive.Events参照解決を確認する。

## 2026-08-13 — ArtifactEffectsResidual boundary

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/src/Effects/`
- **事実:** 分割後に残った5 effect module / 5 implementationを`ArtifactEffectsResidual`として明示化し、既存の`ArtifactEffects`名はaliasにした。
- **閃き・仮説:** TimeDisplacement、Noise、OpticsCompensation、RadialShadow、SurfaceFXは共通effect契約へ収束し、既存の大きなEffects target名から切り離せる。
- **価値・懸念:** 残存effectの変更時にtarget責務を明確化できる。Diligent、Image、Property依存とstatic link順は未検証。
- **次の確認:** ビルド許可後にResidual packのmodule生成、既存alias利用、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactRenderSupportContracts boundary

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Render/ArtifactRenderContext.ixx`、`Artifact/include/Render/ArtifactRenderROI.ixx`
- **事実:** RenderSupportのContext、ROI、Foundation、PerformanceMonitorの4 moduleを`ArtifactRenderSupportContracts`へ移し、Scheduler/Controller等の実装はSupport本体に残した。
- **閃き・仮説:** RenderSupportの契約層を実装層から分離すると、EffectContractやRenderがscheduler実装へ依存せずに共有契約だけを利用できる。
- **価値・懸念:** render context変更時のSupport実装全体のBMI再構築を抑えられる可能性がある。Render/EffectContractとのstatic link順は未検証。
- **次の確認:** ビルド許可後にContracts packのBMI生成、Support本体、Render、EffectContractのmodule/link解決を確認する。

## 2026-08-13 — ArtifactColor Palette / Node boundaries

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Color/ColorPaletteManager.ixx`、`Artifact/include/Color/ArtifactColorNode.ixx`、`Artifact/include/Color/ArtifactColorNodeGraph.ixx`
- **事実:** ColorPaletteManagerを`ArtifactColorPalette`へ、ColorNode/NodeGraphを`ArtifactColorNode`へ移した。既存ArtifactColorにはOCIO、Science、Settings、Management、Gradingを残した。
- **閃き・仮説:** palette persistenceとnode graphはOCIOの重い管理層から独立した変更単位として分離できる。
- **価値・懸念:** palette/node変更時のArtifactColor全体のBMI再構築を抑えられる可能性がある。Serialization、Core Color、NodeGraphのstatic link順は未検証。
- **次の確認:** ビルド許可後に2 packのmodule生成、Palette/NodeGraph利用側、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactColor Settings / Science boundaries

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Color/ArtifactColorSettings.ixx`、`Artifact/include/Color/ArtifactColorScienceManager.ixx`
- **事実:** ColorSettings 1/1を`ArtifactColorSettings`へ、ColorScience 1/1を`ArtifactColorScience`へ移した。ArtifactColor本体はScienceを利用するため明示依存を追加した。
- **閃き・仮説:** 設定契約とLUT/ACES科学計算をOCIO・Management・Grading実装から分けることで、Color変更の再構築範囲をさらに縮小できる。
- **価値・懸念:** ColorSettings/Scienceのmodule referenceとArtifactColorのstatic link順は未検証。
- **次の確認:** ビルド許可後に2 packのBMI生成、OCIO managerのScience参照、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactColor Management / Grading boundaries

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Color/ArtifactColorManagement.ixx`、`Artifact/include/Color/ArtifactColorGradingEngine.ixx`
- **事実:** ColorManagement 1/1を`ArtifactColorManagement`へ、ColorGradingEngine 1/1を`ArtifactColorGrading`へ移した。既存ArtifactColorから両packへの依存を追加した。
- **閃き・仮説:** management helperとgrading engineをOCIO manager・science・node層から独立した変更単位として扱える。
- **価値・懸念:** Color管理・grading変更時のArtifactColor全体のBMI再構築を抑えられる可能性がある。Core Color/Parallelとstatic link順は未検証。
- **次の確認:** ビルド許可後に2 packのBMI生成、ArtifactColorの依存解決、Artifact本体のlinkを確認する。

## 2026-08-13 — ArtifactCore Material / Environment / Light / Crowd / Domain / FileSystem / Icon / VST boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Material/`、`ArtifactCore/include/EnvironmentVariable/`、`ArtifactCore/include/Light/`、`ArtifactCore/include/Crowd/`、`ArtifactCore/include/Domain/`、`ArtifactCore/include/FileSystem/`、`ArtifactCore/include/Icon/`、`ArtifactCore/include/VST/`
- **事実:** Material 1/1、Environment 1/1、Light 1/1、Crowd 1/0、Domain 1/0、FileSystem 1/0、Icon 1/0、VST 2/0を個別packへ移した。Scene→Materialの依存をtarget linkへ反映し、OpenXRはoptional条件を維持するため分割対象から除外した。
- **閃き・仮説:** material、environment、IES、crowd、domain、filesystem、icon、VST契約はそれぞれ小さなleaf packとしてCore本体から隔離できる。
- **価値・懸念:** optional backendや各契約の変更時にCore本体BMI再構築を抑えられる可能性がある。Qt/OS/VST利用側のstatic link順は未検証。
- **次の確認:** ビルド許可後に8 packのmodule生成、Scene/Artifact利用側、optional backend条件のlink解決を確認する。

## 2026-08-12 — ArtifactCore Thread boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Thread/`、`ArtifactCore/include/Media/ImageSequenceSource.ixx`
- **事実:** Thread系5 module / 2 implementationを`ArtifactCoreThread`へ移した。Core内の唯一の利用者はMedia.ImageSequenceSourceで、Media targetからThread targetへの依存を追加した。
- **閃き・仮説:** background task / ticker / thread helperはMedia source cacheのような上位domainへ一方向に提供するleaf utilityとして分離できる。
- **価値・懸念:** thread utility変更時のCore本体BMI再構築を抑えられる可能性がある。Media側のmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後にThread packのmodule生成、ImageSequenceSourceの参照、Media link解決を確認する。

## 2026-08-12 — ArtifactCore Platform boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Platform/`、`ArtifactCore/src/Platform/`
- **事実:** Platform系6 module / 4 implementationを`ArtifactCorePlatform`へ移した。Core内の逆importは0件で、Artifact側の有効なPlatform module利用者も静的検索で確認されなかった。
- **閃き・仮説:** OS/process/shell utilityはdomain依存が薄いleaf packにしてもAPI境界を保ちやすく、将来のplatform条件分岐をCore本体から隔離できる。
- **価値・懸念:** platform-specific変更のBMI再構築を局所化できる可能性がある。現時点でlink伝播を追加していないため、隠れたmodule利用者はビルド時に確認が必要。
- **次の確認:** ビルド許可後にPlatform packのWindows条件分岐、module生成、実利用者の有無を確認する。

## 2026-08-12 — ArtifactCore Shape boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Shape/`、`ArtifactCore/include/IO/VectorExport.ixx`、`ArtifactCore/include/Text/GlyphLayout.ixx`
- **事実:** Shapeのprimary module 12組とShape利用側のIO/Text facade 4組を`ArtifactCoreShape`へ移した。Shape利用者だった`IO.ixx`と`Text.TextAnimator`も同じpackへ含め、Core本体からShape packへの逆import closureを閉じた。
- **閃き・仮説:** Shapeを単独で切り出すのではなく、直接のfacade consumerまで同梱することで、geometry / vector export / text layoutのtarget境界を保てる。
- **価値・懸念:** Shape変更時のCore本体BMI再構築を抑えられる可能性がある。IO facadeの再exportとTextAnimator利用側、static link順は未検証。
- **次の確認:** ビルド許可後にShape packのmodule生成、VectorExport、TextAnimator、ArtifactのShape利用者を確認する。

## 2026-08-12 — ArtifactCore Acoustic boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/src/Acoustic/`、`ArtifactCore/src/Diagnostic/DiagnosticRegistry.cppm`
- **事実:** Acoustic系7 moduleとAcoustic snapshotを保持する`Artifact.Diagnostic.Registry`を`ArtifactCoreAcoustic`へ移した。Acousticの唯一のCore内consumerだったregistryを同梱し、逆importを解消した。
- **閃き・仮説:** telemetry registryが特定domainの型を直接保持する場合、そのregistryをdomain packへ置く方がbase targetへの逆依存を避けられる。
- **価値・懸念:** Acoustic変更をCore本体から分離できる可能性がある。registryの他利用者が将来追加される場合はtarget依存を再評価する必要がある。ビルド・linkは未検証。
- **次の確認:** ビルド許可後にAcoustic packのmodule生成とDiagnosticRegistry利用を確認する。

## 2026-08-12 — ArtifactCore Command boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Command/`、`ArtifactCore/include/UI/InteractiveActions.ixx`
- **事実:** Command系7 moduleとUIの`InteractiveActions`を`ArtifactCoreCommand`へ移した。Command targetは`ArtifactCore`へ依存し、`ArtifactCoreUI`はCommand targetへ依存する一方向構成にした。
- **閃き・仮説:** UI facadeが利用するcommand session/action契約をcommand pack側へ置くと、UI input層と編集履歴層の責務境界をtargetでも表現できる。
- **価値・懸念:** command実装変更のBMI再構築をUI以外のCore domainから切り離せる可能性がある。UI/Commandのmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後にCommand packのmodule生成、UI.InteractiveActions、shortcut/action利用側のlink解決を確認する。

## 2026-08-12 — ArtifactCore Data leaf boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Data/`、`ArtifactCore/include/Asset/DataAssetFile.ixx`
- **事実:** Data系12 moduleを`ArtifactCoreData`へ移した。`Asset.DataAssetFile`はP13のAsset packへ戻し、Data targetは`ArtifactCore`へ一方向に依存する。
- **閃き・仮説:** implementationを持たないdata contract群は、consumerを同梱すれば独立module packとして切り出しやすい。
- **価値・懸念:** CSV/table/type inference変更によるCore本体のBMI再構築を抑えられる可能性がある。Asset targetのmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後にData packのinterface生成、Asset.DataAssetFile、ArtifactAssetのlinkを確認する。

## 2026-08-12 — ArtifactCore Asset boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Asset/`、`ArtifactCore/include/Utils/AssetManager.ixx`
- **事実:** Asset系11 moduleに`Utils.AssetManager`とimplementationを加え、`ArtifactCoreAsset`へ12 module / 7 implementationを移した。`Asset.DataAssetFile`はData packからAsset packへ戻し、Asset targetはData targetへ依存する。
- **閃き・仮説:** Asset managerがAsset domainの唯一のCore consumerであるため、同じpackに閉じ込めるとAsset database/source lifecycleの境界をtargetで表現できる。
- **価値・懸念:** Asset変更時のCore本体BMI再構築を抑えられる可能性がある。ArtifactAssetのmodule reference、Data/Assetのstatic link順は未検証。
- **次の確認:** ビルド許可後にAsset packのdatabase、DataAssetFile、ArtifactAssetのlink解決を確認する。

## 2026-08-12 — ArtifactCore UI leaf boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`Artifact/include/Widgets/`、`Artifact/src/Widgets/`
- **事実:** UI系19 module / 8 implementationを`ArtifactCoreUI`へ移した。Core内の非UI moduleからUI moduleへの逆importは0件で、input operator、shortcut、selection、layout契約を分離した。
- **閃き・仮説:** UI state / input contractをCore本体から分けると、shortcutやviewport操作の変更を他domainのBMI再構築から切り離しやすい。
- **価値・懸念:** Artifactのアプリ本体は`ArtifactCoreUI`をリンクして既存APIを維持できる。一方、ShortcutBindingsの実際の利用target、MSVC module reference、static archive link順は未検証。
- **次の確認:** ビルド許可後にUI packのBMI生成、AppMain / timeline / composition editorのmodule解決とlinkを確認する。

## 2026-08-12 — VolumetricShineの入力は事前抽出済みバッファを要求する

- **関連:** `ArtifactCore/include/ImageProcessing/VolumetricShine.ixx`、`Artifact/src/Effects/Glow/GlowEffect.cppm`
- **事実:** `VolumetricShine::process()` はサンプル輝度を計算するが選別には使わず、渡されたRGB全体を放射状に蓄積し、さらに入力バッファ自身へ加算する。Artifact側のVolumetric Shineは、しきい値で明部を事前抽出し、処理後から抽出元を差し引いて元画像へ合成している。
- **価値・懸念:** 未抽出の通常画像を直接渡すと画面全体が光線化し、処理済みバッファをそのまま加算すると明部が二重加算される。API名だけではこの前提が読み取りにくい。
- **次の確認:** Core側Settingsへ明示的なthresholdを追加するか、入力契約を型またはコメントで明示し、既存呼び出し元との互換性を確認する。

## 2026-08-12 — 合成補助エフェクトは既存の名前付き入力基盤を再利用できる

- **関連:** `Artifact/include/Effects/ArtifactEffectFrameSampler.ixx`、`Artifact/src/Effects/ArtifactEffectFrameSampler.cppm`、`Artifact/include/Effects/EffectHostContract.ixx`
- **事実:** `IEffectFrameSampler::sampleNamedInput()` と `EffectInputBundle` は、レイヤーIDで保持した同一フレーム画像を補助入力として取得できる。Depth Bokehに加え、Light Wrap Pro、Match Grain、Wire / Object Remover、Depth Relight、Matte Refine、Pixel / Dust Fixer、Atmospheric Depth、Edge Color Compositeがこの経路を利用する実装になった。
- **価値・懸念:** 背景、参照素材、クリーンプレート、除去・修復マスク、depth/normal入力を、新しいイベント配線なしで共有できる。現状の入力指定は文字列ID中心で、既存の `ObjectReference` editorは値を`qint64`へ変換するため、任意文字列のLayerIDをそのまま安全に往復できない。UIでのレイヤー選択・欠損時表示・保存再読込・キャッシュ依存の明示は未検証。
- **次の確認:** 補助入力レイヤーを変更した際のキャッシュ無効化を確認し、文字列LayerIDを失わない既存Property Editor上の選択UIへ安全に写像できるか設計する。

## 2026-08-13 — Keying effects は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Keying/`、`Artifact/src/Effects/Keying/`
- **事実:** LumaKey / ChromaKey / DifferenceKey / IBKKeyer の4 module / 4 implementationを`ArtifactEffectsKeying`へ移し、Spatial packから除去した。4実装のimportは共通Effect contract、Image、Property、Core Parallel、IBKKeyerのみRender/Diligentへ収束している。
- **閃き・仮説:** matte生成をSpatial画像処理から分離すると、keyerの変更によるSpatial packのBMI再構築を抑えつつ、GPU keyerだけを独立して検証できる可能性がある。
- **価値・懸念:** Keyingという責務がtarget構成にも現れ、今後のmatte/refinement拡張の依存方向を明確にできる。実際のmodule BMI生成とstatic link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsKeying`のmodule生成、IBKKeyerのDiligent依存、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Blur effects は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Blur/`、`Artifact/src/Effects/Blur/`
- **事実:** AnisotropicFlowBlur / ApertureShapeBlur / ReactionDiffusionBlur の3 module / 3 implementationを`ArtifactEffectsBlur`へ移し、Spatial packから除去した。共通のEffect contract、Image / Property / Core Parallelを中心とする依存で、Blur packのtarget linkをArtifact本体へ追加した。
- **閃き・仮説:** Blur系をSpatialの汎用残余から分離すると、ぼかしアルゴリズムの変更を他の空間効果のBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Blur責務をtarget構成にも表現できる。一方、Blur実装の未登録補助moduleやstatic archiveの実際のpull-inは未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsBlur`のmodule生成、ImageProcessing / Core Parallelの参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Procedural generators は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Generate/`、`Artifact/src/Effects/Generate/`
- **事実:** SimpleRain / RadioWaves の2 module / 2 implementationを`ArtifactEffectsGenerate`へ移し、Spatial packから除去した。両実装は共通のEffect contract、Image、Property、Core Parallelを中心に依存する。
- **閃き・仮説:** procedural generatorを小さなpackに閉じ込めると、生成系エフェクトの変更をSpatialの他のオペレータから切り離せる可能性がある。
- **価値・懸念:** Generate責務をtarget構成にも表現できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsGenerate`のmodule生成、Image/Parallel参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Distort effects は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Distort/`、`Artifact/include/Effects/TurbulentDisplace/`、`Artifact/src/Effects/`
- **事実:** DisplacementMap / TurbulentDisplace の2 module / 2 implementationを`ArtifactEffectsDistort`へ移し、Spatial packから除去した。両実装は共通のEffect contract、Image、Property、Core Parallelを中心に依存する。
- **閃き・仮説:** distortion operatorを独立packにすると、画像変位系の変更を他のSpatial operatorのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Distort責務をtarget構成にも表現できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsDistort`のmodule生成、Image/Parallel参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Stylize effects は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/{Kaleidoscope,Dithering,Kuwahara,Bevel}/`、`Artifact/src/Effects/`
- **事実:** Kaleidoscope / Dithering / Kuwahara / Bevel の4 module / 4 implementationを`ArtifactEffectsStylize`へ移し、Spatial packから除去した。4実装は共通のEffect contract、Image、Property、GPU compute、Render境界に収まる。
- **閃き・仮説:** stylize operatorを独立packにすると、GPUベースの画調・質感効果の変更をSpatialの残余operatorから切り離せる可能性がある。
- **価値・懸念:** Stylize責務をtarget構成にも表現できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsStylize`のmodule生成、GPU compute/Render参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Glow effects は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Glow/`、`Artifact/src/Effects/Glow/`、`Artifact/include/Effects/DirectionalGlowEffect.ixx`
- **事実:** DirectionalGlow、Glow、EdgeBloom、ChromaticGlow、ReactiveGlow、LiquidGlow、ResidualGlow、PhysicalHalation、LuminescenceCausticsの9 module / 9 implementationを`ArtifactEffectsGlow`へ移し、Spatial packから除去した。Glow packはImage/Property、GPU compute、Renderを主な依存境界とする。
- **閃き・仮説:** Glow系を一つのpackに閉じると、光学・発光アルゴリズムの変更をSpatial残余operatorのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Glow責務をtarget構成にも表現できる。PhysicalHalationのParticle依存と、既存Glow variantの自動登録との重複は静的確認済みだが、実際のmodule/link解決は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsGlow`のmodule生成、Particle/GPU compute/Render参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Optical distortion effects は複数の旧packをまたぐ境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/LensDistortion/`、`Artifact/include/Effects/OpticsCompensation/`、`Artifact/src/Effects/`
- **事実:** LensDistortion / OpticsCompensation の2 module / 2 implementationを`ArtifactEffectsOptics`へ集約した。LensDistortionはSpatial側、OpticsCompensationはresidual側にあったため、両方の元source listから除去し、ImageProcessing.Distortionを共通依存とした。
- **閃き・仮説:** sourceの物理配置や旧分類ではなく、共有する画像変形契約でpackを切ると、光学補正の変更範囲を一つのtargetに閉じ込められる可能性がある。
- **価値・懸念:** Spatial/residual間の責務重複を解消できる。OpticsCompensation.cppmは通常のmodule implementation形式のため、CXX_MODULES登録とMSVCのmodule参照は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsOptics`のmodule生成、ImageProcessing.Distortion参照、両旧packからの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Wave effect は独立した GPU operator pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Wave/WaveEffect.ixx`、`Artifact/src/Effects/Wave/WaveEffect.cppm`
- **事実:** Wave の1 module / 1 implementationを`ArtifactEffectsWave`へ移し、Spatial packから除去した。Wave実装はImage、Property、GPU compute、Render、Core Parallelを主な依存とする。
- **閃き・仮説:** 単一でも責務と変更頻度が独立したGPU operatorは専用packにすると、Spatial残余の再構築範囲を明確に抑えられる可能性がある。
- **価値・懸念:** Wave責務をtarget構成にも表現できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsWave`のmodule生成、GPU compute/Render参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Image filters は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/{LinearWipe,Liquify,Mosaic,Spherize,Sharpen,FindEdges}/`、`Artifact/src/Effects/`
- **事実:** LinearWipe / Liquify / Mosaic / Spherize / Sharpen / FindEdges の6 module / 6 implementationを`ArtifactEffectsFilters`へ移し、Spatial packから除去した。共通のImage、Property、GPU compute、Render、Core依存をtargetで表現した。
- **閃き・仮説:** 画像フィルタ群を残余Spatialから分離すると、フィルタアルゴリズムの変更を他のSpatial operatorのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Filters責務を明示できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsFilters`のmodule生成、GPU compute/Render参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — AddNoise は独立した GPU/image operator pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/AddNoise/AddNoiseEffect.ixx`、`Artifact/src/Effects/AddNoise/AddNoiseEffect.cppm`
- **事実:** AddNoise の1 module / 1 implementationを`ArtifactEffectsNoise`へ移し、Spatial packから除去した。Image upload、GPU compute、Render、Core Parallelを主な依存とする。
- **閃き・仮説:** 単一moduleでもGPU uploadを伴う独立operatorは専用packに分けることで、ノイズ実装変更の再構築範囲を明確化できる可能性がある。
- **価値・懸念:** Noise責務をtarget構成にも表現できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsNoise`のmodule生成、Image upload/GPU compute参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — AutoMosaic は独立した CV operator pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/AutoMosaicEffect.ixx`、`Artifact/src/Effects/AutoMosaicEffect.cppm`
- **事実:** AutoMosaic の1 module / 1 implementationを`ArtifactEffectsAutoMosaic`へ移し、Spatial packから除去した。FaceDetection、CvUtils、Property、Core Parallelを主な依存とする。
- **閃き・仮説:** 顔検出を伴うCV operatorを一般的なSpatial残余から分離すると、検出依存の変更を画像効果群から切り離せる可能性がある。
- **価値・懸念:** AutoMosaicのCV責務をtarget構成にも表現できる。実際のFaceDetection/CvUtils link解決は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsAutoMosaic`のmodule生成、FaceDetection/CvUtils参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Motion/flow rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{OpticalFlowBlur,VectorBlur,VectorFlowGlitch,LightTrails,MotionTrail}Effect.ixx`、対応する`Artifact/src/Effects/Rasterizer/`実装
- **事実:** OpticalFlowBlur / VectorBlur / VectorFlowGlitch / LightTrails / MotionTrail の5 module / 5 implementationを`ArtifactEffectsMotion`へ移し、Rasterizer packから除去した。Effect.Context、Image、Property、GPU compute、Renderを共通依存とする。
- **閃き・仮説:** motion/flow operatorをRasterizer残余から分離すると、履歴・ベクトル系の変更を他のstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Motion責務をtarget構成にも表現できる。実際のEffect.Context module参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsMotion`のmodule生成、Effect.Context/GPU compute/Render参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — プロ向けエフェクトUIのボトルネックはパラメータ記述契約にある

- **関連:** `ArtifactCore/include/Property/AbstractProperty.ixx`、`Artifact/src/Effects/ArtifactAbstractEffect.cppm`、`Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorShared.cppm`、`Artifact/src/Effect/ArtifactEffectPreset.cppm`、各 `Artifact/src/Effects/**/getProperties()`
- **事実:** 共通 `PropertyMetadata` は表示名、単位、tooltip、hard/soft range、step を保持できるが、静的検索では `getProperties()` を持つ76ファイルに対し、unit / tooltip / step / soft range のいずれかを設定するファイルは12件だった。Gaussian Blurはrange/stepを定義する一方、Glow、Levels、Curvesなどには値だけの項目が多い。列挙候補は専用metadataではなくtooltip文字列またはproperty名のハードコードで推定される。effect presetの値型はFloat / Color / Stringのみで、Boolean / Integerを型付きで保持しない。
- **閃き・仮説:** エフェクト数や個別UIを増やす前に、stable parameter ID、表示label、型、単位、hard/soft range、step/precision、enum choices、section、visibility dependency、animatable、quality cost、preset inclusionを一つのdescriptor契約へ集約すると、Inspector、Property Editor、preset、OFX bridge、automationが同じ意味を共有できる。
- **価値・懸念:** 代表的な5エフェクトから段階導入すれば、数値操作の精度、意味の理解、プリセット再現性、将来の互換性を小さい変更範囲で改善できる。表示名を識別子としている既存effectがあるため、一括renameや全effect移行は保存互換性を壊す懸念がある。
- **実装状況:** Curvesの制御点editor、Levelsのmaster range editor、GlowのContribution Onlyとrange/quality metadata、effect preset schema 2のInteger / Boolean / Double型保持、および旧schema読込を追加した。既存property名は互換aliasとして維持し、新しい複合controlだけstable IDを採用した。ビルド・runtime確認は未実施。
- **次の確認:** Curves / Levelsのdrag previewと保存再読込、GlowのCPU/GPU Contribution表示一致、schema 1/2 presetの往復を確認する。その後Gaussian Blur / White Balanceへ同じmetadata契約を展開する。

## 2026-08-13 — Digital artifact rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{DataMosh,Glitch,FilmDamage,Deflicker}Effect.ixx`、対応する`Artifact/src/Effects/Rasterizer/`実装
- **事実:** DataMosh / Glitch / FilmDamage / Deflicker の4 module / 4 implementationを`ArtifactEffectsDigital`へ移し、Rasterizer packから除去した。Effect.Context、Image、Property、Core Parallelを共通依存とする。
- **閃き・仮説:** digital artifact系を独立packにすると、glitch/film damage/deflicker変更を他のstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Digital責務をtarget構成にも表現できる。実際のEffect.Context参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsDigital`のmodule生成、Effect.Context/Image/Parallel参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Pattern rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{Bricks,HexGrid,Halftone,Stripes,Voronoi}Effect.ixx`、対応する`Artifact/src/Effects/Rasterizer/`実装
- **事実:** Bricks / HexGrid / Halftone / Stripes / Voronoi の5 module / 5 implementationを`ArtifactEffectsPatterns`へ移し、Rasterizer packから除去した。Image、Property、Core Parallel、GPU compute、Renderを共通依存とする。
- **閃き・仮説:** pattern generatorを独立packにすると、テクスチャ生成系の変更を他のstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Patterns責務をtarget構成にも表現できる。実際のGPU compute/Render参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsPatterns`のmodule生成、Image/Property/Parallel/GPU compute参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Chromatic rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{ChromaticAberration,ChromaticRelief}Effect.ixx`、対応する`Artifact/src/Effects/Rasterizer/`実装
- **事実:** ChromaticAberration / ChromaticRelief の2 module / 2 implementationを`ArtifactEffectsChromatic`へ移し、Rasterizer packから除去した。Image、Property、Core Parallel、GPU compute、Renderを共通依存とする。
- **閃き・仮説:** chromatic operatorを独立packにすると、色収差・色レリーフの変更を他のstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Chromatic責務をtarget構成にも表現できる。実際のGPU compute/Render参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsChromatic`のmodule生成、Image/Property/Parallel/GPU compute参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Shadow rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/DropShadowEffect.ixx`、`Artifact/include/Effects/Rasterizer/InnerShadowEffect.ixx`、対応する実装
- **事実:** DropShadow / InnerShadow の2 module / 2 implementationを`ArtifactEffectsShadows`へ移し、Rasterizer packから除去した。Image、Property、Core Parallel、GPU compute、Renderを共通依存とする。
- **閃き・仮説:** shadow operatorを独立packにすると、影生成の変更を他のstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Shadows責務をtarget構成にも表現できる。実際のGPU compute/Render参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsShadows`のmodule生成、Image/Property/Parallel/GPU compute参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Context-aware rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{DifferenceMatte,Edge,Ghost,PixelSort}Effect.ixx`、対応する実装
- **事実:** DifferenceMatte / Edge / Ghost / PixelSort の4 module / 4 implementationを`ArtifactEffectsContextual`へ移し、Rasterizer packから除去した。Effect.Context、Image、Property、Core Parallelを共通依存とする。
- **閃き・仮説:** frame-contextを参照するraster operatorを独立packにすると、入力コンテキスト連携の変更をstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Contextual責務をtarget構成にも表現できる。実際のEffect.Context参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsContextual`のmodule生成、Effect.Context/Image/Property/Parallel参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Temporal context rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{PosterizeTime,ScreenShake}Effect.ixx`、対応する実装
- **事実:** PosterizeTime / ScreenShake の2 module / 2 implementationを`ArtifactEffectsTemporalContext`へ移し、Rasterizer packから除去した。Effect.Context、Image、Property、Core Parallelを共通依存とする。
- **閃き・仮説:** 時間・フレームコンテキストを持つraster operatorを独立packにすると、時間サンプリング変更をstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** TemporalContext責務をtarget構成にも表現できる。実際のEffect.Context参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsTemporalContext`のmodule生成、Effect.Context/Image/Property/Parallel参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Finishing rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{RadialBlur,Stroke,Vignette}Effect.ixx`、`Artifact/include/Effects/Satin/SatinEffect.ixx`、対応する実装
- **事実:** RadialBlur / Satin / Stroke / Vignette の4 module / 4 implementationを`ArtifactEffectsFinishing`へ移し、Rasterizer packから除去した。Image、Property、Core Parallel、GPU compute、Renderを共通依存とする。
- **閃き・仮説:** 仕上げ処理を独立packにすると、最終画調・輪郭処理の変更を他のraster operatorのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Finishing責務をtarget構成にも表現できる。実際のGPU compute/Render参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsFinishing`のmodule生成、Image/Property/Parallel/GPU compute参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Legacy Rasterizer path の同名 module 二重定義を target から除外した

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/GlowEffect.ixx`、`Artifact/include/Effects/Rasterizer/KaleidoscopeEffect.ixx`、canonicalな`ArtifactEffectsGlow` / `ArtifactEffectsStylize`
- **事実:** Rasterizer pathのGlow/Kaleidoscopeはcanonical pack側と同じmodule名を持つ別sourceだった。ファイルは削除せず、Rasterizer targetのsource listから除外し、canonical packだけがmoduleを提供するようにした。FinishingのSatin interface pathも実在ファイルへ修正した。
- **閃き・仮説:** 分割ではtarget追加だけでなく、同一module名の旧経路を明示的に閉じないと、BMI/リンクのownershipが不定になる可能性がある。
- **価値・懸念:** moduleの二重提供を静的に避けられる。canonical sourceとlegacy sourceの内容差分を保持したままなので、legacy側を完全廃止できるかは未検証。
- **次の確認:** ビルド許可後にGlow/Kaleidoscopeのmodule定義が一つずつ生成されること、Satin interfaceと実装の対応、Rasterizer residualの実source ownershipを確認する。

## 2026-08-13 — Residual effect source ownership を閉じた

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffectsDistort`、`ArtifactEffectsShadows`、`ArtifactEffectsNoise`、`ArtifactEffectsSurfaceFX`
- **事実:** residualに残っていたTimeDisplacementをDistort、RadialShadowをShadows、NoiseEffectをNoiseへ統合し、SurfaceFXを`ArtifactEffectsSurfaceFX`へ分離した。さらに全 focused packを`ARTIFACT_EFFECTS_MODULES/IMPL`から明示的に除去した。
- **閃き・仮説:** pack用変数を狭めた後に汎用残余リストを除去すると、CMake変数の評価順によってsplit sourceがresidualへ戻るため、ownership除去はfocused pack単位で明示する必要がある。
- **価値・懸念:** residual targetのsource ownershipを4 module / 4 implementationまで縮退させ、二重コンパイルを静的に防げる可能性がある。CMake configure / module scanは未検証。
- **次の確認:** ビルド許可後にresidualがTimeDisplacement/RadialShadow/Noise/SurfaceFXを含まないこと、4 residual sourceの実際のlink解決、focused packとの重複がないことを確認する。

## 2026-08-13 — Empty residual target を compatibility umbrella に変更した

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffectsResidual`、`ArtifactEffects` alias、focused ArtifactEffects packs
- **事実:** focused packへのsource移管後にresidual sourceが0/0になったため、`ArtifactEffectsResidual`をSTATICからINTERFACEへ変更し、全focused packへのlink委譲だけを持たせた。既存の`ArtifactEffects` alias名は維持した。
- **閃き・仮説:** 空archiveを互換入口として残すより、INTERFACE umbrellaにすると既存link名を保ちながら不要なbinary targetを生成せずに済む。
- **価値・懸念:** source ownershipをfocused packへ一意化できる。umbrella経由のtransitive link順と既存consumerのarchive pull-inは未検証。
- **次の確認:** ビルド許可後にresidual archiveが生成されないこと、`ArtifactEffects` aliasからfocused packが伝播すること、既存のeffect consumerが解決することを確認する。

## 2026-08-13 — Empty Spatial/Rasterizer targets を compatibility umbrella に変更した

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffectsSpatial`、`ArtifactEffectsRasterizer`、focused effect packs
- **事実:** source 0/0になった`ArtifactEffectsSpatial`と`ArtifactEffectsRasterizer`をSTATICからINTERFACEへ変更し、それぞれfocused pack群へのtransitive link入口として維持した。
- **閃き・仮説:** source ownershipを全てfocused packへ移した後も旧target名をumbrellaとして残すと、既存consumerのtarget参照を保ちながら空archive生成を避けられる。
- **価値・懸念:** Spatial/Rasterizerの互換入口を維持しつつ、実体targetを増やさずに済む。umbrella経由のlink順と既存consumerの解決は未検証。
- **次の確認:** ビルド許可後に両targetがarchiveを生成しないこと、旧target名から各focused packが伝播すること、全effect consumerが解決することを確認する。

## 2026-08-13 — Focused pack ownership を manifest checker に追加した

- **関連:** `scripts/check_source_manifests.py`、`Artifact/CMakeLists.txt`、Artifact focused effect packs
- **事実:** checkerにfocused `ARTIFACT_EFFECTS_*_MODULES/IMPL`の件数一致、source path存在、pack間重複検査を追加した。実行結果は全pack pass、重複0、missing path 0だった。
- **閃き・仮説:** explicit source manifestだけではCMake target間の二重ownershipを検出できないため、pack変数の静的検査を同じcheckerに置くと分割変更の回帰を早期検出できる。
- **価値・懸念:** 今回のresidual再登録問題のようなCMake評価順の回帰を、configure前に検出できる可能性がある。CMakeの実configure解釈そのものは未検証。
- **次の確認:** CIまたはsource追加後にcheckerを実行し、focused packの件数・重複・path検査が継続してgreenであることを確認する。

## 2026-08-13 — Focused pack の link reachability を静的確認した

- **関連:** `Artifact/CMakeLists.txt`、22 focused `ArtifactEffects*` STATIC pack、Spatial/Rasterizer/Residual INTERFACE umbrella
- **事実:** 22 focused packすべてがArtifact本体または互換umbrellaの`target_link_libraries`から到達可能で、未リンクpackは0件だった。
- **価値・懸念:** source ownershipを分離してもArtifact executableから孤立するpackがないことを静的に確認できた。実際のstatic archive pull-in、module BMI、link orderは未検証。
- **次の確認:** ビルド許可後に各packのmodule生成と、umbrella経由を含む実リンク解決を確認する。

## 2026-08-13 — Ownership checker を root custom target の経路へ統合した

- **関連:** `CMakeLists.txt`、`scripts/check_source_manifests.py`
- **事実:** rootの`check_source_manifests` custom targetは既存のPython checkerを実行しており、checker拡張後はexplicit manifestに加えてfocused packの件数・重複・path ownershipも同じ経路で検査する。targetのコメントを実際の責務に合わせて更新した。
- **価値・懸念:** CIや開発者が既存の検査targetを呼ぶだけで、CMake source ownershipの回帰も検出できる。CMake configureそのものは未実行。
- **次の確認:** ビルド許可後にroot custom target経由でcheckerが起動することを確認する。
### 2026-08-13: RadialBlur の旧 Rasterizer 重複も所有リストから除外
- **関連:** `Artifact/CMakeLists.txt`、RadialBlur の canonical / legacy source paths
- **事実:** `Artifact.Effect.Rasterizer.RadialBlur` は `Effects/RadialBlur` と `Effects/Rasterizer` の両方にインターフェース・実装が存在し、モジュール名が重複していた。
- **対応:** `ArtifactEffectsFinishing` が現在所有する `Effects/Rasterizer/RadialBlurEffect.ixx/.cppm` を Rasterizer umbrella と residual の source list から除外し、未使用の `Effects/RadialBlur` 側は manifest exclusion のまま保持した。ファイル自体は削除していない。
- **価値/懸念:** 二重定義を避けつつ、履歴上の旧ファイルを保全できる。CMake configure / build による実際の target 解決は未検証。
- **次に確認:** 他の module-name 重複は既存の分割実装かを確認し、同様に明確な二重定義だけを所有リストから除外する。
## 2026-08-13 — QADS adapter と native dock surface の段階移行境界

- **関連:** `Artifact/include/Widgets/ArtifactDockManager.ixx`、`Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **事実:** 公開widget moduleからQADS型を除去し、QADS adapterとbackend-neutralな `DockLayoutRegistry` を分離した。native surfaceは5領域、tab化、portable layout、visible／pinned／activate／area移動を持つが、floatingとdrag/dropは未対応としてcapabilityで明示している。
- **仮説:** QADS state blobを既定のモデルにし続けると、native backendへの切替時にfloatingやtab groupの差異が暗黙に失われるため、portable modelを先に正規化し、未対応機能は復元時に診断ログへ出す方が安全。
- **価値・懸念:** adapter交換の境界と部分復元の失敗条件を明示できる。一方、native surfaceは現在ArtifactMainWindowの既定backendへ接続しておらず、実機表示・module hygiene・QADS完全撤去は未検証。
- **次に確認:** ビルド許可後に新規moduleのコンパイル、native surfaceの実機表示、portable復元、既存QADS layoutとの比較を検証する。

## 2026-08-13 — AI write 結果は既存 CommandResult を再利用する

- **関連:** `ArtifactCore/include/AI/CommandIR.ixx`、`Artifact/include/AI/WorkspaceAutomation.ixx`
- **事実:** `ArtifactCore::CommandResult` は `success`、`valid`、`executed`、`type`、`error`、`undoLabel`、`diagnostics`、`details` を持ち、`toVariantMap()` と `commandResultFromVariantMap()` を備えている。`WorkspaceAutomation` の `validateCommand` / `executeCommand` はこの型を経由している。
- **対応:** AI 側の共通判定に合わせ、既存フィールドを保持したまま `validateCommand` は `ok = valid`、`executeCommand` は `ok = success` を追加した。
- **価値/懸念:** 新しい結果型を増やさず、既存の write 実行経路を AI から一貫して判定できる。`errorCode` の体系はまだ存在せず、自由文 `error` を機械分類する設計は未着手。
- **次に確認:** command type ごとの error taxonomy を設計し、`error` の自由文と互換な `errorCode` を段階的に追加する。ビルド・runtime確認は未実施。

## 2026-08-13 — CommandResult の error taxonomy は段階導入する

- **関連:** `ArtifactCore/include/AI/CommandIR.ixx`、`Artifact/src/AI/CommandIRExecutor.cppm`
- **事実:** 現在の `CommandResult` は `error` を自由文で保持し、validation failure、unsupported command、target/property failure、render failure が同じ文字列フィールドに入る。既存の command 実装には安定した `errorCode` フィールドはない。
- **提案:** 既存の `error` を保持したまま、まず `COMMAND_INVALID`、`UNSUPPORTED_COMMAND`、`TARGET_NOT_FOUND`、`PROPERTY_INVALID`、`EXECUTION_FAILED`、`RENDER_FAILED` の粗い分類を追加する。詳細な command-specific code は後段にする。
- **価値/懸念:** AI が再試行・ユーザー確認・入力修正を選べるようになる。一方、自由文からの自動分類は誤判定し得るため、各 executor の失敗分岐で明示設定する必要がある。
- **実装状況:** `WorkspaceAutomation` の `validateCommand` / `executeCommand` で、既存 `error` を保持したまま `COMMAND_INVALID`、`UNSUPPORTED_COMMAND`、`PROPERTY_INVALID`、`TARGET_NOT_FOUND`、`RENDER_FAILED`、`EXECUTION_FAILED` の粗い分類を段階導入した。`CommandResult` に `errorCode` と `retryable` を追加し、validation と `CommandIRExecutor` の明示的な property / effect-index failure は executor 側で直接設定する。facade は既存呼び出しとの互換 fallback として残している。
- **次に確認:** 残りの executor failure branch を、意味が確定するものだけ段階移行する。ビルド・runtime確認は未実施。

## 2026-08-14 — GPU文字 atlas はカラー絵文字を単色QRawFont経路で表現できない

- **関連:** `ArtifactCore/src/Text/GlyphAtlas.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`、`experiments/TextAnimatorLab/artifact_gpu_text_smoke.cpp`
- **事実:** DX12のGPUスモークで日本語と通常ラテン文字は描画できるが、`U+1F9EA`（🧪）はQRawFontのalpha atlas経路では□になる。UTF-8ファイル入力で引数変換を排除しても再現したため、PowerShellのUnicode transportだけが原因ではない。
- **仮説:** Windowsのカラー絵文字フォントをalpha-only atlasへ落とす現在の設計では、カラーレイヤー情報を失うか、代替グリフの輪郭を取得している。絵文字は単色フォールバック、カラーbitmap atlas、または別の絵文字描画契約を選択できる必要がある。
- **価値・懸念:** 「文字が存在する」ことと「GPU atlasで正しく描画できる」ことを分離して監査できる。絵文字を通常文字と同じGlyphKeyだけで扱うと、カラー情報とgrapheme/ZWJ単位を失う。
- **次に確認:** QRawFontのglyph index・alphaMapサイズ・font familyを絵文字ケースごとに記録し、単色記号（★）とカラー絵文字（🧪、😀、ZWJ）を比較する。

## 2026-08-14 — Segoe UI Emoji はalpha取得可能、欠落点はカラー転送

- **関連:** `experiments/TextAnimatorLab/artifactcore_text_smoke.cpp`、`ArtifactCore/src/Text/GlyphAtlas.cppm`
- **事実:** `🧪` は `Segoe UI Emoji` のglyph index 3620として解決され、`QRawFont::alphaMapForGlyph` は86x88のbitmapを返し、`pathForGlyph`も空ではなかった。alpha画像は `artifactcore_emoji_alpha.png` として保存できた。
- **結論:** 「絵文字glyphを取得できない」は誤り。現在の単色coverage atlasは輪郭を取得できるが、カラーbitmapの色レイヤーを保持しない。GPU側の未完了範囲はカラーatlas形式、転送、shader分岐である。
- **次に確認:** alpha-only絵文字をGPUで描画する経路を最新ArtifactCore/ArtifactRenderビルドで再検証し、その後カラーbitmap取得方式を選定する。

## 2026-08-14 — Windowsカラーglyphの実装候補はDirectWrite 3

- **関連:** `ArtifactCore/src/Text/GlyphAtlas.cppm`、Windows SDK `um/dwrite_3.h`
- **事実:** 現行Windows SDKには `IDWriteFontFace5`、`DWRITE_COLOR_GLYPH_RUN1`、カラーglyph列挙APIが存在する。Qtの`QRawFont::alphaMapForGlyph`だけでは色レイヤーを取得できない。
- **提案:** Windows実装ではDirectWriteのカラーglyph runをRGBA bitmapへラスタライズする専用providerを設け、`GlyphRenderMode::ColorBitmap`だけをそのproviderへ分岐する。通常glyphは既存QRawFont coverage経路を維持する。
- **価値・懸念:** モノクロ経路を壊さず、カラー／COLR／SVG系をOSのフォント実装に合わせられる。一方、DirectWriteのfont faceとQtのfont family・glyph indexの対応、およびGPU atlas更新のスレッド境界は未検証。
- **次に確認:** DirectWrite font face生成と`DWRITE_COLOR_GLYPH_RUN1`のbitmap化を小さなWindows専用Coreスモークで検証し、QImageは入力境界に限定してRGBAバッファへ明示変換する。

## 2026-08-14 — DirectWriteカラーglyph run列挙は実機で成立

- **関連:** `experiments/TextAnimatorLab/directwrite_color_glyph_smoke.cpp`
- **事実:** Windows SDKの`IDWriteFactory2::TranslateColorGlyphRun`で`🧪`（glyph 3620）を実行時に列挙し、5つのカラーglyph runとパレットインデックスを取得できた。
- **結論:** カラー情報の取得不能ではなく、残る実装範囲はrunのRGBAラスタライズ、GlyphAtlasへの明示コピー、GPU shaderでのカラーサンプル分岐である。
- **次に確認:** DirectWriteカラーrunを一時RGBAターゲットへ描画する方法を、既存のQt合成禁止・GPU本流優先ルールに沿って選定する。まずCPU診断用の最小RGBAバッファで座標・透明度・パレット合成を検証する。

## 2026-08-14 — DirectWriteカラーrunはalpha textureへラスタライズ可能

- **関連:** `experiments/TextAnimatorLab/directwrite_color_glyph_smoke.cpp`
- **事実:** `IDWriteFactory3::CreateGlyphRunAnalysis` と `IDWriteGlyphRunAnalysis::CreateAlphaTexture` を使い、`🧪` の5カラーrunから合計3841個の非透明alpha pixelを取得できた。
- **結論:** DirectWriteカラーrunは、runごとのパレット色とalpha textureを明示合成してRGBAアトラスへ変換できる。QtのQPainterをGPU本流へ追加する必要はない。
- **次に確認:** palette entry取得、runごとのtexture boundsの共通キャンバス合成、GlyphAtlasのカラー専用入力APIを実装する。

## 2026-08-14 — Segoe UI EmojiのカラーrunはrunColorを直接提供する

- **関連:** `experiments/TextAnimatorLab/directwrite_color_glyph_smoke.cpp`
- **事実:** `🧪` の5runは大きな`paletteIndex`値を返すが、各runの`runColor`には有効なRGBA色が入っている（例: `(0.765, 0.937, 0.235, 1.0)`）。`imageFormats=0x5`、alpha textureも生成済み。
- **結論:** カラー合成ではpalette indexを通常CPAL indexとして解釈せず、DirectWriteが返す`runColor`を優先する。特殊palette indexはそのままGPU契約へ持ち込まない。
- **次に確認:** runColor×alpha textureのCPU合成を診断バッファで検証し、GlyphAtlasのカラー矩形へ保存するデータ形式を固定する。

## 2026-08-14 — DirectWriteカラーrunのRGBA合成スモークが成立

- **関連:** `experiments/TextAnimatorLab/directwrite_color_glyph_smoke.cpp`、`directwrite_color_glyph.ppm`
- **事実:** 5つのカラーrunを各texture boundsの共通キャンバス（93x92）へ配置し、`runColor`とalphaをpremultiplied相当のsource-overで合成できた。PPM出力は25,681 bytes、合成入力はalphaPixels=3841。
- **結論:** GlyphAtlas側で必要な最小データは、カラー矩形、RGBA8画素、bearing/advance、render modeで固定できる。Qt合成やQImageのホットパス追加は不要。
- **次に確認:** この合成処理をCoreのWindows専用providerへ移し、DirectWrite非対応環境では既存coverageまたは明示的unsupportedへフォールバックする。
## 2026-08-14: ArtifactCore の分割ターゲット重複が全体GPUビルドを阻害

- 関連: `ArtifactCore/CMakeLists.txt`, `src/AI/OnnxDmlLocalAgent.cppm`
- 事実: `OnnxDmlLocalAgent.cppm` が統合 `ArtifactCore` と分割 `ArtifactCoreAI` の双方のコンパイル対象になり、モジュール実装の宣言解決エラーが発生している。
- 影響: テキスト/GPU実装とは独立した既存ビルド構成の問題だが、アプリ全体の `ArtifactRender` ビルドを止める。
- 次に確認: 分割ターゲット移行時の重複ソース除去方針を設計し、全体ビルドの別マイルストーンとして扱う。

## 2026-08-14: 旧ArtifactRenderと新Diligent/Coreの混在はGPUスモークを起動直後に壊す

- 関連: `experiments/TextAnimatorLab/gpu_smoke_standalone/CMakeLists.txt`, `Artifact/ArtifactRender.lib`
- 事実: 既存のArtifactRender静的ライブラリ（2026-08-11）を複数世代のDiligent/Coreライブラリと組み合わせると、APIバージョン不一致または起動直後のアクセス違反になり、実画像が生成されない。
- 影響: GPU合否はソース修正だけでは判定できず、ArtifactRender・ArtifactCore・Diligentを同一ビルド世代で再生成する必要がある。
- 次に確認: 全体ビルドが完了した世代のライブラリだけで専用スモークを再リンクし、`image=幅x高さ saved=1`を監査の必須条件にする。

## 2026-08-14: ArtifactRenderTextSmokeは現状でもArtifactRender/全Core依存を引き込む

- 関連: `Artifact/CMakeLists.txt`、`ArtifactCore/CMakeLists.txt`
- 事実: `ArtifactRenderTextSmoke`は`ArtifactRender`にリンクし、`ArtifactRender`は全体のCore依存を通るため、コメントにある「UIなしの軽量GPUスモーク」でもArtifactCore全体のモジュール生成をスケジュールする。
- 影響: テキスト専用GPU検証のビルド時間と失敗範囲が、Particle/Audio等の無関係なCore境界に広がる。
- 次に確認: 本番Rendererからテキスト描画に必要なGPU契約・atlas upload・readbackを独立したRendererTextRuntimeへ分離できるか設計し、既存ArtifactRenderとのABI混在を避ける。
## 2026-08-14: ArtifactIRendererはテキストGPU実験の最小依存ではない
- related: Artifact/src/Render/ArtifactIRenderer.cppm, Artifact/src/Render/DiligentImmediateSubmitter.cppm, Artifact/CMakeLists.txt
- fact: ArtifactRenderTextRuntimeからPostProcess/MotionBlur/GPUTextureCacheを除外しても、ArtifactIRendererがMesh/Material/LayerBlend/RayTracing/Particle/LOD等を直接importするため、ArtifactCore全体のモジュールグラフを再び広げる
- impact: 既存IRendererをそのまま再利用する分離では、TextSmokeの高速・安定した検証目標を満たせない
- hypothesis: テキストGPU経路には、Device/Shader/CommandBuffer/Primitive2D/ glyph submit/readbackだけの専用Facadeが必要
- next: ArtifactIRendererのAPIをTextRenderContext等へ分解し、既存Renderer本体とスモーク依存を切り離す
## 2026-08-14: GPUテキスト経路の次の依存ボトルネックはImmediateSubmitter
- related: Artifact/src/Render/PrimitiveRenderer2D.cppm, Artifact/src/Render/DiligentImmediateSubmitter.cppm, Artifact/include/Render/DiligentImmediateSubmitter.ixx
- fact: PrimitiveRenderer2D::drawGlyphs は GlyphAtlasSprite packet を RenderCommandBuffer に積むだけで、GPU実行は DiligentImmediateSubmitter::submitAtlasSprite に委譲される。
- fact: DiligentImmediateSubmitter は glyph path 以外にも PrimitiveRenderer3D、ParticleRenderer、全Sprite/Rect/Line PSO群を公開・実装依存として import している。
- impact: ArtifactIRenderer を外しても、現状の Submitter をそのまま使う限り最小GPUテキストターゲットは全描画依存を再び取り込む。
- hypothesis: GlyphText/AtlasSprite のsubmit処理、必要なShaderManagerのglyph PSO、RenderCommandBufferの該当packetだけを専用Submitterへ分離すれば、Core/Renderer全体を避けた実GPU smokeを構築できる。
- next: glyph-only submitterの依存グラフと、ShaderManagerからglyph PSO生成に必要な最小シェーダー群を抽出する。
## 2026-08-14: 独立Glyph GPU経路でatlas upload後のalpha監査が必要
- related: experiments/TextAnimatorLab/artifact_text_glyph_smoke.cpp, ArtifactCore/src/Text/GlyphAtlas.cppm, Artifact/include/Render/ArtifactTextGlyphShaderSources.ixx
- fact: D3D12 device、Glyph PSO/SRB、Core GlyphAtlasの `T` rect (63x90)、quad draw、640x180 readbackまでは同一Debug出力で成功した。
- observation: readback画像は非透明の矩形として見え、文字形状のalphaマスクとして期待する結果ではない。CPU atlasのrect取得自体は `atlasRect=0,0 63x90` で成立している。
- hypothesis: QImage RGBA upload、Alpha8からRGBA8へのcoverage展開、またはshaderのalpha/blend/resource-state境界のいずれかで透明度が失われている。未検証。
- fact: 原因はスモーク側のGPU texture descriptorが1x1のまま2048x2048 atlasを渡していたことだった。descriptorをatlas実寸へ修正後、GPU alphaは `min=0 max=255` となり、readback画像でT形状を確認できた。
- next: スモークを単一Tから実際のTextLayout glyph列とカラーemojiへ拡張し、複数rect・カラー保持・アニメータ変形を同じGPU経路で検証する。
- fact: `Text Sample1 🧪` をCore GlyphAtlasから12 glyphとして生成し、D3D12 readbackで白文字とカラー試験管emojiを確認できた。カラーglyphは1件、GPU alphaは0..255。
- fact: 正式Submitter APIで `offsetRotation` / `offsetScale` / `offsetOpacity` を各Glyphへ設定し、回転した文字列とカラーemojiのreadback画像を確認した。これはタイムライン依存なしのGlyph単位GPU変形の実証になる。

## 2026-08-14: FloatColorへの汎用Variant埋め込みはカラー統合の初手にしない
- 関連: `ArtifactCore/src/Color/FloatColor.cppm`、`Artifact/src/Color/ArtifactColorScienceManager.cppm`、`Artifact/src/Effects/Rasterizer/VectorBlurEffect.cppm`
- 事実: `FloatColor` は加減乗除、補間、色変換、UIパレット、合成処理で広く使われている。一方、`SurfaceColorDescriptor` は少なくともエフェクト側で既に色の格納形式・原色・伝達関数・参照方式を表現している。
- 結論: `ColorAny` / 無制限 `std::variant` を `FloatColor` の代替として導入すると、描画内部へ型判定と変換責務が拡散する。まず `SurfaceColorDescriptor` を入力・画像バッファ境界の正規メタデータとして採用し、演算内部の `FloatColor` は当面維持する方が変更範囲と循環依存を抑えられる。
- 次に確認: ピッカー、LUT、コンポジットの各入口で、色値とdescriptorを別々に受け渡せる既存APIを棚卸しし、変換が暗黙に起きている境界から段階的に整理する。

## 2026-08-14: FloatColorPickerはHDR編集不能をUI仕様として固定している
- 関連: `ArtifactWidgets/src/Dialog/FloatColorPicker.cppm`
- 事実: RGB/HSB/HSL/明度/アルファのスライダーは全て0〜1000の範囲で、値を0〜1へ変換する。HEX表示・入力も8bit（0〜255）で、`FloatColor` に1.0超の値を保持していてもUIから編集・往復できない。
- 結論: HDR対応は `FloatColor` の型変更だけでは解決せず、ピッカーにシーン参照モード、露出表示、1.0超の数値入力、表示用HEXとの分離が必要。既存のArtifactWidgetsを変更する作業として独立して扱うべき。
- 次に確認: HDR用UIを既存ピッカーへ追加するか、通常ピッカーとシーン参照ピッカーを分離するかを設計レビューで決める。Qt QColorへの変換は表示専用境界に限定する。

## 2026-08-14: OCIOは現行本線、旧ColorManagerは未接続候補
- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`、`Artifact/src/Color/ArtifactColorManagement.cppm`、`Artifact/src/Widgets/Render/ViewportColorPipeline.cppm`、`Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: `ArtifactOCIOManager` は画像入力変換、ビューポート表示変換、プロジェクト保存/読込から参照されている。Artifact側の `ColorManager` は定義と自身の実装以外の呼び出し箇所が検索上確認できず、`ArtifactColorScienceManager` はカラーサイエンスパネルと旧LUT管理を保持している。
- 追加事実: `ArtifactCore/include/Color/ColorSpace.ixx` には公開 `ColorManager` API が存在するため、Artifact側実装の未使用だけを根拠にColorManager全体を削除してはならない。
- 追加事実: `ColorManager::instance()` の呼び出しは `ArtifactCore` / `Artifact` / `ArtifactPr` のソース検索で見つからず、Core側の公開宣言とArtifact側の実装が残っている。一方、レンダリング契約など複数のインターフェースが `Color.ColorSpace` をimportしているため、モジュール境界そのものは依存されている。
- 結論: 3系統を同列に統合するのではなく、まずOCIOを現行の正規経路として明文化する。Artifact側の旧実装を整理する場合も、ArtifactCoreの公開ColorManagerとの互換境界を先に定義する。
- 次に確認: 削除ではなく、`ColorManager` のAPIを互換層として残し、実装をOCIO設定・変換サービスへ委譲できるかを設計する。`ArtifactColorScienceManager` のLUT責務はOCIO設定・ビュー変換責務と分離して整理する。

## 2026-08-14: ColorLUTの既存CPU経路はHDRを明示的に失う
- 関連: `ArtifactCore/src/Color/ColorLUT.cppm`、`ArtifactCore/include/Color/ColorLUT.ixx`
- 事実: `ColorLUT::apply(float&, float&, float&)` は入力と補間結果を0〜1へクランプする。`applyToImage()` は入力を `QImage::Format_ARGB32` に変換し、8bit RGBAへ書き戻す。
- 結論: HDR対応はピッカーだけでなくLUT適用経路にも必要。既存のQImage APIの意味を変えず、F32画像／バッファ向けにHDR値を保持する別APIを追加し、表示用変換とシーン値のLUT適用を分離するのが安全。
- 次に確認: `ImageF32x4_RGBA` または既存のF32バッファ型へLUTを適用する境界を確認し、クランプが必要なのはLUTサンプル座標だけか、出力値もクランプする仕様かを決める。

## 2026-08-14: 3D回転のX/Y/Z項目は現状モデルへ保存されていない
- 関連: `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`、`ArtifactCore/src/Animation/AnimatableTransform3D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- 事実: `ArtifactAbstractLayer::setRotation3D(QVector3D)` は `rot.x()` だけを `AnimatableTransform3D::setRotation()` に渡す。`AnimatableTransform3D` の公開setterも単一の `float degrees` で、JSON/UIには `rotationX/Y/Z` が現れる箇所があるが、内部のアニメーション値と評価経路は単一角度。
- 結論: 3D回転対応はUI項目の追加ではなく、X/Y/Z各軸のアニメーション値、シリアライズ、補間、描画行列を一貫して拡張するモデル変更。既存の`rotation`をZ軸互換として扱う移行仕様が必要。
- 次に確認: 3Dレイヤーの描画行列生成箇所と、既存JSONの`rotation`/`rotationX/Y/Z`読込優先順位を棚卸しし、互換変換を先に定義する。
- 追加事実: `Artifact3DModelLayer.cppm` と `ArtifactProcedural3DLayer.cppm` はいずれも `QMatrix4x4::rotate(angle, 0, 0, 1)` を使う。共通の `ArtifactAbstractLayer::getLocalTransform4x4()` も現状は単一回転前提で、個別3Dレイヤーだけを修正しても2D/3D共通変換や親子変換との整合を失う。
- 次に確認: まず共通のローカル行列生成をEuler順序またはQuaternionに置き換える設計を決め、その後に3Dモデル・Procedural3D・Gizmo・Undoの各経路を同じ回転値へ接続する。

## 2026-08-14: Transform3Dの通常行列はZ位置を落としている
- 関連: `ArtifactCore/src/Animation/AnimatableTransform3D.cppm`
- 事実: `getMatrix()` のtranslationは `(currentX_, currentY_, 0.0f)` を使う一方、`getAllMatrix()` は `(currentX_, currentY_, currentZ_)` を使う。`getMatrixAt()` もZ位置を0固定で生成する。
- 懸念: 3Dレイヤーの評価経路によってZ位置が反映されたり失われたりする可能性がある。3軸回転の実装前に、`getMatrix` / `getAllMatrix` / `getMatrixAt` / `getAllMatrixAt` の責務とZ位置の扱いを統一する必要がある。
- 追加事実: `getMatrixAt()` はアニメーションのoffset値（`x_`/`y_`/`z_`、`scaleX_`等）を直接行列へ入れる一方、`getAllMatrixAt()` はinitial値との合成値を使う。`getMatrix()` と `getMatrixAt()` の責務差はコードコメントだけでは明確でなく、3D化時に初期値・offset値の合成規約を確定する必要がある。

## 2026-08-14: バッチ再リンクは既存relink APIの単純拡張では足りない
- 関連: `Artifact/include/Service/ArtifactProjectService.ixx`、`Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 事実: 現在は `relinkFootageByPath(old,new)` と `relinkFootageItems(items,new)` があり、Asset Browserには単一アセットのUndo付き再リンク導線がある。旧パスから候補ファイルを探索するbasename、相対パス、サイズ、mtime、ハッシュ等の解決器は見当たらない。
- 結論: バッチ再リンクは既存APIにループを足すだけでは不十分。候補探索結果、曖昧候補、連番グループ、全参照更新、Undo単位をまとめる専用サービス境界が必要。
- 次に確認: まず候補探索を副作用なしの `RelinkCandidateResolver` として定義し、確定後に既存の `relinkFootageByPath` を呼ぶ二段階構成にする。自動確定ではなく候補提示を初期仕様とする。
- 追加事実: `relinkFootage()` の現行実装は `FootageItem::filePath` と連番の `sequencePaths` を更新するが、`image.sourcePath` / `video.sourcePath` / `audio.sourcePath` 等のレイヤー側パスを同じ操作内で更新していない。
- 懸念: レイヤーがFootageItemを参照して再解決する経路が別に存在する可能性はあるが、コード上は再リンク直後の全参照伝播が保証されていない。なお、再確認により `ArtifactAbstractComposition::allLayer()` / `allLayerRef()` は既に公開されていることが判明したため、先の「全レイヤー列挙APIがない」という見立ては誤り。候補探索より先に、AssetDatabase・FootageItem・レイヤーsourcePathの正規参照関係を確認する必要がある。
- 追加事実: Asset Browserの `RelinkAssetCommand` は `relinkFootageByPath()` だけをredo/undoしている。
- 懸念: relinkFootage内でレイヤーsourcePathまで直接変更すると、既存UndoがFootageItemのパスしか戻さず、レイヤー参照だけが取り残される。伝播を実装する場合はFootageItem変更と全レイヤー変更を同一Undoコマンドにまとめる必要がある。

## 2026-08-14: QPA障害ではなくZWJ描画単位の未接続が残る
- 関連: `experiments/TextAnimatorLab/run_gpu_smoke.ps1`、`experiments/TextAnimatorLab/artifact_text_glyph_smoke.cpp`、`ArtifactCore/src/Text/TextShapingBackend.cppm`
- 事実: Debug QPAを明示してRTX 4070 Ti / D3D12上で `Text1`、CJK、`👩‍💻` を同一GPUスモークへ通せる。通常文字とCJKは描画できるが、ZWJは現行のQt glyph列生成で3 glyph（カラー2件）として出力される。
- 結論: QPA探索とGPU起動は解決済み。ZWJ・variation selector・modifierを単一の描画／アニメーション単位として扱うには、Unicode grapheme契約とglyph atlasのsequence rasterization境界を一致させる必要がある。部分表示を成功扱いにせず、sequence対応を独立した完了条件にする。
- 次に確認: `GlyphKey` / `GlyphAtlas::acquire()` が単一code point前提のため、sequence keyとDirectWrite color glyph runの合成結果をキャッシュできる最小APIを設計する。
- 追加事実: DirectWriteへsequence全体のglyph配列を試験的に渡すと、現状のQt由来glyph列とは位置・合字結果が一致せず、同じsequence画像を複数回描画する危険がある。Submitterは現在、ZWJ/variation selectorをスキップし、scalar color glyphを明示的な暫定フォールバックとして使う。
- 結論: sequence rasterizerを有効化するには、DirectWriteのshape結果（glyph index、原点、advance、run bounds）をCore layoutへ戻し、Submitterが1 cluster 1 quadを生成する契約まで一体で検証する必要がある。単に`sequenceUtf8`をキーへ渡すだけでは製品品質にならない。
- 追加事実: Qt `QGlyphRun` のstring indexを使ってshaped glyph indexをCore `GlyphItem`へ保持し、DirectWriteへglyph index 1623を渡すと、`👩‍💻`の合成済みcolor glyphを実GPUで1描画単位としてreadbackできた。Qtが返さない継続codepointはSubmitterでスキップする。
- 更新結論: sequence対応の最小実装は「Coreのshape結果を捨てず、Atlas keyにshaped glyph indexを含める」ことで成立する。複数run、異なるfont fallback、modifier sequenceは引き続き追加ケースとして検証が必要。
- 追加事実: `GlyphItem.shapedGlyphIndices` を追加し、同一cluster内のshaped glyph indexをCoreで集約した。家族絵文字は実行時に1 cluster / 4 shaped glyphとして取得できる。
- 残課題: Submitter/Atlasはまだscalar indexを描画単位にしているため、配列契約は接続済みだが家族clusterの合成画像化は未完了。配列全体のDirectWrite run rasterizationと、cluster bounds/advanceの伝搬が次の実装境界。
- 追加事実: `GlyphKey.shapedGlyphIndices`へcluster配列を伝搬し、Submitterがcluster先頭だけをAtlasへ渡す経路を実装した。家族絵文字の4 glyphはDirectWriteの1 runとして処理されるが、run boundsの左端／レイヤー境界の扱いによりreadback画像にclipが残る。
- 次に確認: DirectWrite color runの各layer boundsをglyph runの原点へ戻す座標変換を検証し、union boundsのminX/minYをbearingとして保持する。単純にscalar QRawFont boundingRectへ置換するだけでは不十分。
- 追加検証: 家族clusterをx=120へ移動して実GPU描画したところ、合成run画像は欠けずに表示できた。先の左端clipはAtlas union boundsではなく、Smokeのx=0付近で回転したquadが画面端で切れた結果だった。実アプリではcluster boundsを考慮した安全な画面配置／自動フレーム内判定が別途必要。
- 追加事実: 前後文字を含む複合Smokeでは、通常glyphのAtlas rectはvalidでもGPU readbackから消える。`A B`だけでも再現するため、家族cluster固有ではない。単独`Text1`との差分は、現行Submitterの複数glyph／変形描画状態にある可能性が高い。
- 次に確認: rotation/scale/opacityを無効にした同一Submitter試験と、1 draw callに全quadをまとめる方式を比較し、DrawAttribsのvertex offsetまたは変形後座標の問題を分離する。
- 原因確定: 複合ケースで通常文字が消えた原因はGPU draw状態ではなく、`QImage::Format_Alpha8`をGrayscale8へ変換してcoverageを読んでいたことだった。Alpha8はalpha channelを直接読む必要があり、元の分岐へ戻すと無変形`A B`および`A 👨‍👩‍👧‍👦 B`が実GPUで復旧した。
- 追加原因確定: 前後Latin文脈で家族emojiが一部になったのは、`QGlyphRun`の重複したcluster先頭string indexをfallbackが行頭0から割り当てていたため。run内の有効なstring indexをfallback開始位置に使うと、`A 👨‍👩‍👧‍👦 B`でA・家族emoji・Bの全てを実GPU表示できた。

## 2026-08-14: 3D回転モデルと連番再リンクの実装反映
- 関連: `ArtifactCore/src/Animation/AnimatableTransform3D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 事実: `AnimatableTransform3D` にX/Yの独立値・キーフレーム評価を追加し、既存 `rotation` をZ軸互換として共通行列、スナップショット、保存／再読込、3Dモデル、Procedural3D、ギズモ、Undoへ接続した。Euler適用順序はZ→Y→X。
- 事実: Asset Browserの複数選択再リンクは候補を素材ごとに確認してから一括適用し、途中失敗時にロールバックする複合Undoを持つ。同一連番の複数フレーム選択はFootageItem単位へ正規化した。
- 懸念: いずれもビルド・実行検証は未実施。旧 `rotation` と新X/Y/Zの初期値・offset合成、およびレイヤー固有プロパティUIの3軸編集契約は引き続き確認が必要。

## 2026-08-14: 再リンク参照一致は正規化絶対パスで行う
- 関連: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 事実: FootageItemの絶対パスとレイヤーJSONのsourcePathは、相対表記や区切り文字の違いを含み得る。
- 結論: バッチ再リンクのsourcePath伝播では生文字列比較を避け、`QFileInfo(...).absoluteFilePath()` と `QDir::cleanPath()` を通した比較を使う。
- 次に確認: 大文字小文字の扱いはOS依存のため、Windows上のケース差を含む実行検証が必要。

## 2026-08-14: バッチ再リンク候補には参照数を併記する
- 関連: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 事実: 候補選択前に全コンポジションのレイヤーJSONを走査し、旧パスを参照するレイヤー数を集計できる。
- 結論: 候補のスコア・理由だけでなく参照数も表示し、影響範囲を確認してから確定できるようにした。候補探索と参照集計は適用前に実行されるため、副作用はない。

## 2026-08-14: AssetDatabaseにも再リンクのID維持移行が必要
- 関連: `ArtifactCore/include/Asset/AssetDatabase.ixx`、`ArtifactCore/src/Asset/AssetDatabase.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`
- 事実: `AssetManager::acquireSource(newPath, ...)` は新しいAssetDatabase登録を作れるが、旧パスのAssetInfoを自動移行・削除するAPIは存在しなかった。
- 対応: Asset IDを維持したままpathToIdとAssetInfoのパスを移す `relinkAssetPath()` を追加し、連番は全フレームの移行に失敗した場合に逆順ロールバックする。
- 未検証: 実プロジェクトでのAssetDatabase永続化、既存newPath衝突、ビルド・実行挙動。

## 2026-08-14: RAMプレビューは二経路が存在し、PlaybackService側は既に先読み接続済み
- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`、`Artifact/src/Render/ArtifactRamPreviewController.cppm`、`docs/analysis/AE_PAIN_POINT_IMPROVEMENT_MAP_2026-08-13.md`
- 事実: `ArtifactPlaybackService` はRAMキャッシュ、周辺フレーム先読み、世代番号による要求キャンセル、進捗／ヒット率、再生開始をキャッシュ準備で待たせない経路を持つ。複数のWidgetもPlaybackServiceの状態を参照している。
- 事実: `ArtifactProjectService::setPreviewQualityPreset()` 内には旧 `progressiveRenderer_` 呼び出しのコメントが残るが、実際の `CompositionRenderController::setPreviewQualityPreset()` は Draft/Preview/Final を 4/2/1 倍の downsample に変換し、品質変更時にRAM preview cacheをinvalidateして再描画を要求する。
- 事実: 別の `ArtifactRamPreviewController::startBuild()` はレンダーコールバックを同一スレッドのwhileループで処理する。CMakeには登録されているが、現状のアプリ実行コードからの利用箇所は確認できず、既存の階層キャッシュ計画でもlegacy initial controller扱いになっている。
- 結論: 改善の主眼は新しいRAMプレビュー機構を追加することではなく、PlaybackServiceを正規経路として二経路を整理し、旧Controllerには新機能を追加せず、PlaybackService側の実際の非同期性と品質プリセットを検証すること。
- 未検証: ビルド・実行時に旧Controllerがリンク対象／外部利用されていないこと、およびPlaybackServiceのフレーム生成がUIスレッドを長時間ブロックしないこと。

## 2026-08-14: 連番再リンクでは同一フレームのAssetDatabase移行を無操作成功にする
- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`、`ArtifactCore/src/Asset/AssetDatabase.cppm`
- 事実: 連番再リンクでは、移行先の一部フレームが既存パスと同一になることがある。`AssetDatabase::relinkAssetPath()` は同一パスを拒否するため、移行不要なフレームまで失敗扱いにすると全体ロールバックへ入る。
- 対応: `ArtifactProjectService::relinkFootage()` の移行ヘルパーで正規化絶対パスが同一の場合は成功扱いにし、AssetDatabase APIを呼ばずに続行する。
- 未検証: 混在した連番の実プロジェクトでのAsset ID維持、衝突時ロールバック、ビルド・実行挙動。

## 2026-08-14: 再リンク同一判定はAssetDatabaseと同じWindows大小文字規則が必要
- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`、`ArtifactCore/src/Asset/AssetDatabase.cppm`
- 事実: `AssetDatabase::normalizedAssetPath()` はWindowsでcase foldingを行うが、再リンクサービス側の同一パス判定は当初 `cleanPath` のみだった。
- 対応: 再リンク移行ヘルパーでもWindowsではcase foldingしてから同一パスを無操作成功と判定するようにした。
- 未検証: Windows上で大文字小文字だけ異なる既存連番のAsset ID維持とロールバック。

## 2026-08-14: 再リンク移行の同一判定はcanonical pathを優先する
- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`、`ArtifactCore/src/Asset/AssetDatabase.cppm`
- 事実: `AssetDatabase` は実在ファイルのcanonical pathをAsset identityに使うが、サービス側の移行前比較はabsolute pathだけだった。
- 対応: サービス側の移行ヘルパーもcanonical path、空の場合はabsolute path、clean path、Windows case foldingの順に正規化するようにした。
- 未検証: シンボリックリンクを含む連番のAsset ID維持と、移行失敗時の逆順ロールバック。

## 2026-08-14: 再リンク検索入口も同一のcanonical path正規化へ統一
- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`
- 事実: `findFootageItemByPath()` と `relinkFootageByPath()` は移行ヘルパーとは別にabsolute path比較を持っていたため、symlink・Windows大小文字差で対象FootageItemを見失う余地があった。
- 対応: 匿名名前空間の `normalizeRelinkPath()` を追加し、検索・同一判定・AssetDatabase移行前判定で共有するようにした。
- 未検証: 実ファイルのsymlink、Windowsケース差、連番の混在パスを含む検索から移行までの実行確認。

## 2026-08-14: AI操作の初期ハンドシェイクを契約化
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`、`design/user-personas/api-agent.md`
- 事実: WorkspaceAutomationにはスナップショット、コマンド検証、dry-run、監査ログ、診断が既に存在するが、AIが起動直後に安全な利用順序と必須レスポンス項目を一括取得する入口はなかった。
- 対応: `agentContract()` を追加し、発見・安全実行順序・観測・高リスク操作・失敗レスポンス項目・運用原則を機械可読な `QVariantMap` で返すようにした。
- 未検証: 実行時の登録経路から `agentContract` を呼び出せること、外部AIクライアントが契約情報を利用すること、ビルド・実行挙動。

## 2026-08-14: AI操作契約を共通システムプロンプトにも反映
- 関連: `ArtifactCore/include/AI/AIPromptGenerator.ixx`
- 事実: `AIPromptGenerator` はCore層にあり、Artifact層のWorkspaceAutomationを直接importできない。一方、全AIバックエンドが共通の操作方針を受け取る入口になっている。
- 対応: 状態観測、安定ID解決、validateCommand、preview/dry-run、明示確認、実行後再観測、失敗情報保持の順序を日本語・英語のシステムプロンプトへ追加した。
- 未検証: 各バックエンドが生成済みシステムプロンプトを実際に使用すること、ビルド・実行挙動。

## 2026-08-14: クラウドAIへ実行時のエージェント契約を注入
- 関連: `Artifact/src/AI/AIClient.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`
- 事実: クラウドチャットは共通システムプロンプトとツールスキーマを使用するが、契約の具体的なバージョン・観測メソッド・安全メソッドはプロンプトに含まれていなかった。
- 対応: `agentContract()` の現在値をCompact JSON化し、クラウドAIのシステムプロンプトへ追加した。
- 未検証: QVariantからJSONへの変換結果、各クラウドプロバイダのプロンプト受け渡し、ビルド・実行挙動。

## 2026-08-14: AI起動時の読み取りをagentPreflightへ集約
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`、`design/user-personas/api-agent.md`
- 事実: AIは契約、現在状態、診断を別々に取得すると、呼び出し順や一部取得漏れを起こしやすい。
- 対応: 読み取り専用の `agentPreflight()` を追加し、契約・workspace snapshot・diagnosticsを一括返却するようにした。
- 未検証: 実行時のJSONシリアライズ、AIクライアント側での自動利用、ビルド・実行挙動。

## 2026-08-14: AI契約とpreflightをツールブリッジ検査へ固定
- 関連: `Artifact/src/Test/ArtifactTestAIToolBridge.cppm`
- 事実: AI向けメソッドはツールスキーマへ登録されるため、登録漏れや返却形状の退行は起動後まで見つからない可能性がある。
- 対応: `agentContract` / `agentPreflight` のスキーマ登録、契約バージョン、読み取り専用フラグ、主要返却項目を既存のAIツールブリッジテストで検査するようにした。
- 未検証: テストの実行結果、ビルド・実行挙動。

## 2026-08-14: AI APIリファレンスにpreflight契約を公開
- 関連: `Artifact/docs/AI_API_EXTENDED_REFERENCE.md`、`Artifact/docs/AI_API_CLOUD_WIDGET_NOTES.md`
- 事実: 実装とテストに追加したagentContract / agentPreflightが、既存のAI APIリファレンスには記載されていなかった。
- 対応: 起動時の推奨呼び出し順、読み取り専用preflight、検証・確認・実行後観測の契約を公開ドキュメントへ追記した。
- 未検証: ドキュメントからのサンプルJSONが各外部クライアントでそのまま解釈されること、ビルド・実行挙動。

## 2026-08-14: クラウドツール実行後にpreflightを再観測
- 関連: `Artifact/src/AI/AIClient.cppm`
- 事実: クラウドのツールループは実行結果のtraceを次の応答へ渡していたが、変更後のworkspace状態を同じ応答に含めていなかった。
- 対応: ツール呼び出し成功直後に`agentPreflight()`を読み取り、`post_tool_preflight`として次のAI応答へ渡すようにした。
- 未検証: ツール実行後のsnapshot内容、長いpreflight JSONによるコンテキスト増加、ビルド・実行挙動。

## 2026-08-14: 共通プロンプトでagentPreflightの発見性を明示
- 関連: `ArtifactCore/include/AI/AIPromptGenerator.ixx`
- 事実: 共通プロンプトは安全な観測順序を説明していたが、ローカルAIが具体的な一括入口を選ぶにはメソッド名の手掛かりが不足していた。
- 対応: WorkspaceAutomation利用時は`agentPreflight()`を最初の読み取りハンドシェイクとして優先する指示を日本語・英語へ追加した。
- 未検証: 各ローカルモデルがこの優先順位を守ること、ビルド・実行挙動。

## 2026-08-14: AI Cloud Widgetの実行経路にもpost-tool観測を追加
- 関連: `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- 事実: AIClientのクラウドループとは別に、Cloud Widgetが承認付きでツールを直接実行する経路を持っていた。
- 対応: 承認済みツール実行の結果へ`post_tool_preflight`を付加し、UI経由でも次のAI応答が変更後状態を観測できるようにした。
- 未検証: MCP外部ツールを含む場合のpreflight適用範囲、ビルド・実行挙動。

## 2026-08-14: Python Workspace APIへagentPreflightを公開
- 関連: `Artifact/src/Script/ArtifactPythonHookManager.cppm`、`Artifact/docs/AI_API_EXTENDED_REFERENCE.md`
- 事実: Python bridgeにはworkspaceSnapshotや各種編集操作が登録されていたが、AIエージェント向けの契約・状態・診断の一括取得入口がなかった。
- 対応: `artifact.workspace.agentPreflight()` を追加し、C++側と同じcompact JSONを返すようにした。
- 未検証: PythonEngine初期化後の関数登録、JSON受け渡し、ビルド・実行挙動。

## 2026-08-14: WorkspaceAutomationの説明文にAI安全入口を明示
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`
- 事実: `agentPreflight` はスキーマ登録されていても、コンポーネント詳細説明だけを読むクライアントには優先入口として伝わらなかった。
- 対応: 詳細説明に、読み取り専用preflight、書き込み検証、完了前の再観測を明記した。
- 未検証: 各クライアントが詳細説明を表示・利用すること、ビルド・実行挙動。

## 2026-08-14: agentContractにPython代替入口を記載
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`、`design/user-personas/api-agent.md`
- 事実: C++のWorkspaceAutomationとPythonの`artifact.workspace`は同じpreflightを提供するが、契約情報にはPython名がなかった。
- 対応: `alternateEntryPoints.python` に `artifact.workspace.agentPreflight` を追加し、ペルソナ文書にも併記した。
- 未検証: PythonEngine未初期化時の利用可否、ビルド・実行挙動。

## 2026-08-14: agentPreflightに観測時刻を付加
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/Test/ArtifactTestAIToolBridge.cppm`
- 事実: preflightはworkspace・診断・契約をまとめて返していたが、AIが結果の新しさを判定する時刻情報がなかった。
- 対応: `observedAtUtc` をISO 8601 millisecond形式で追加し、ブリッジテストでも空でないことを検査するようにした。
- 未検証: 長時間処理中のsnapshotと実際の編集時刻の差、ビルド・実行挙動。

## 2026-08-15: 直近レンダリング調査レポートの妥当性確認
- 関連: `docs/analysis/BATCH_RENDER_FAILURE_2026-08-13.md`、`docs/analysis/IMAGE_BUFFER_PRECISION_AUDIT_2026-08-13.md`、`docs/analysis/OCCLUSION_CULLING_IMPLEMENTATION_MEMO_2026-08-13.md`、`docs/analysis/ADVANCED_RENDERING_GAP_2026-08-13.md`
- 事実: `useMfr = false`、フレーム全体を覆う `compositionFrameStateMutex_`、`ArtifactBatchRenderer` の未初期化設定、RT の BLAS no-op、RenderGraph の診断専用経路など、主要な指摘は一次ソース上で確認できた。
- 判断: レポートは概ね妥当。ただし、並列レンダー・float/HDR 化・Hi-Z・RenderGraph実行化はいずれも子リポジトリの広範な変更を伴い、現時点で一括実装すべき単一修正ではない。
- 次に確認すべきこと: ユーザーが対象サブモジュールと優先順位を明示した後、最小の縦切り（まずバッチ設定バグ修正、または性能基盤の設計分離）を選定する。ビルド・実行検証は別途許可が必要。

## 2026-08-15: 既存フレームパス実装と共有RenderGraphの接続点
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`ArtifactCore/include/Graphics/RenderGraph.ixx`
- 事実: Composition 側には `FunctionalRenderPass` / `RenderPassExecutor` による既存の段階的パス実行があり、RenderGraph は診断グラフだけでなく、フレームパス順序の検証・共有スケジューラとして段階導入できる。
- 対応: `renderOneFrameImpl` のフレームパス計画から共有 RenderGraph を構築し、依存チェーンの compile 検証を追加した。既存 executor の資源所有・実行は維持している。
- 未検証: RenderGraph executor から実 GPU パスを直接駆動した場合のリソース状態遷移、実行時間、runtime 表示。

## 2026-08-15: レイヤー縦切りをRenderGraph executorへ移行
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 対応: Layer Raster → Mask / Track Matte → Blend の3パスについて、既存 `FunctionalRenderPass` を共有 `RenderGraph::execute()` の executor から実行する `runAllWithRenderGraph()` を追加した。
- 未検証: 1レイヤーごとの graph compile コスト、GPU resource barrier の実装、複数レイヤー間での transient resource aliasing。

## 2026-08-15: RTウォームアップをTLAS参照経路へ拡張
- 関連: `ArtifactCore/src/Graphics/RayTracingManager.cppm`
- 対応: 固定色だけを書いていた ray-generation shader に TLAS、TraceRay、miss、triangle closest-hit を追加し、PSO/SBT に hit group と TLAS binding を登録した。
- 未検証: 実メッシュ登録後の DXR/Vulkan runtime shader compilation、空 TLAS での TraceRay、GPU 出力のヒット色。

## 2026-08-15: RT登録対象を不透明メッシュに限定
- 関連: `Artifact/src/Render/ArtifactIRenderer.cppm`
- 対応: BLAS登録条件に実効 opacity、base color alpha、opacity texture の判定を追加し、透明メッシュを RT の不透明ジオメトリ経路へ登録しないようにした。
- 未検証: 同一 geometry の複数 instance 管理、透明化／不透明化がフレーム中に切り替わる場合の BLAS/TLAS 更新。

## 2026-08-15: RenderGraph transient allocation slot 計画
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`
- 対応: compile 結果に各 resource の生存区間と allocation slot を付加し、区間が重ならない transient resource を同一 slot に割り当てる greedy aliasing 計画を追加した。External/Persistent resource は再利用対象外とした。
- 未検証: Diligent texture/buffer 実体への slot 適用、フォーマット・サイズ互換性を考慮した aliasing、backend barrier との連携。

## 2026-08-15: RenderGraph aliasing 予算を診断へ公開
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`
- 対応: compiled graph に allocation slot 数を追加し、diagnostic snapshot に論理 resource 総量とは別の alias 後推定 byte 数を追加した。
- 未検証: 実 backend allocation との差、アライメント・メモリ heap 制約、FrameDebug JSON への表示統合。

## 2026-08-15: aliasing メモリ見積もりを FrameDebug に公開
- 関連: `ArtifactCore/include/Frame/FrameDebug.ixx`、`Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm`
- 対応: `estimatedAliasedResourceBytes` と resource の `allocationSlot` を JSON 往復・診断表示へ追加した。
- 未検証: 実 GPU allocation との差、古い capture JSON との表示互換性、UI上の長文レイアウト。

## 2026-08-15: RenderGraph executorへcompiled allocation計画を伝播
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`
- 対応: `RenderGraphExecutionContext` に `CompiledRenderGraph` を追加し、executor が resource lifetime と allocation slot を参照できるようにした。handle から lifetime を引く accessor も追加した。
- 未検証: backend allocator が実際に slot を使う実装、pass間の resource state transition、executor callback の runtime 性能。

## 2026-08-15: compiled graph に allocation slot descriptor を追加
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`
- 対応: slot ごとに resource 種別、寸法、format、最大 byteSize を保持する `RenderAllocationSlotDescriptor` と accessor を追加した。executor は slot descriptor を参照して backend resource を確保できる。
- 未検証: Diligent の実 texture/buffer pool 実装、heap alignment、alias slot の state transition。

## 2026-08-15: allocation slot descriptor を FrameDebug 往復化
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`、`ArtifactCore/include/Frame/FrameDebug.ixx`、`Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm`
- 対応: compiled graph の slot descriptor 一覧を diagnostic snapshot、JSON 往復、Frame Pipeline の slot 数表示へ追加した。
- 未検証: slot 個別の UI 詳細表示、実 backend pool と descriptor の一致、旧 capture の migration 表示。
- 対応: Frame Pipeline に各 allocation slot の種別・寸法・format・byteSize の詳細行を追加した。

## 2026-08-15: MeshRenderer の BLAS buffer bind 不整合を修正
- 関連: `ArtifactCore/src/Graphics/MeshRenderer.cppm`、`ArtifactCore/src/Graphics/RayTracingManager.cppm`
- 事実: MeshRenderer の position/index buffer は vertex/index bind のみで作成されていたが、Diligent の BLAS build source buffer には `BIND_RAY_TRACING` が必要だった。
- 対応: position/index buffer に `BIND_RAY_TRACING` を追加し、buffer pointer または geometry 数が変わった場合は既存 BLAS を再生成するようにした。
- 未検証: 実デバイスの BLAS build 成功、同一 geometry の複数 instance、buffer rebuild 中の GPU lifetime。
- 追記: `BIND_RAY_TRACING` は Ray Tracing 対応デバイスでのみ付与し、非対応デバイスの通常メッシュ作成を維持する。
- 対応: MeshRenderer の RT 対応判定を RayTracingManager と同じ feature state + `STANDALONE_SHADERS` capability 判定へ統一した。
- 対応: TLAS に `ALLOW_UPDATE` を付け、同一 instance 数のフレーム更新では update scratch size を使った TLAS update を選択する。instance 数が変わる場合は full build に戻す。
- 対応: BLAS/TLAS scratch buffer・instance buffer の生成失敗と TLAS 最大 instance 数超過を build 前に拒否する。
- 対応: BLAS 登録時に vertex/index buffer の `BIND_RAY_TRACING` を検査し、診断カウンタの BLAS build 数を実 build 数単位に修正した。
- 対応: BLAS ごとの dirty 状態を追加し、geometry layout が変わった BLAS だけを再構築するようにした。transform 更新時は BLAS build を省略し TLAS update へ進める。
- 対応: `updateInstanceTransform()` / `hasBLAS()` を追加し、geometry が同じでも transform 変更時だけ TLAS update を発行するようにした。透明状態から不透明状態へ戻るメッシュも再登録できる。
- 対応: 不透明でない mesh instance は TLAS mask=0 で無効化し、再び不透明になった際は transform update 経由で再有効化する。
- 対応: `RayTracingCapabilities` に登録 BLAS 数、有効 instance 数、直近 build 成否を追加し、初期化ログへ出力した。
- 対応: TLAS が未構築の初期化段階では `traceUnitQuad()` が TraceRays を発行しないようにした。

## 2026-08-15: BLAS/TLAS 静的整合性監査
- 関連: `ArtifactCore/include/Graphics/RayTracingManager.ixx`、`ArtifactCore/src/Graphics/RayTracingManager.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`
- 事実: 新規 pure virtual API の実装は `RayTracingManager` に集約され、呼び出し側も ArtifactIRenderer のみだった。
- 対応: BLAS 登録数を有効な BLAS 実体数として数えるよう修正し、TLAS build 失敗時の `lastBuildSucceeded` を必ず false に戻すようにした。TLAS scratch の build/update 最大サイズ判定も統一した。
- 未検証: コンパイラによる C++20 module 整合性、Diligent 実デバイス上の BLAS/TLAS build、複数 instance の表現。

## 2026-08-15: 現行 mesh 呼び出しの RT 識別子確認
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`、`Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`、`Artifact/include/Render/ArtifactIRenderer.ixx`
- 事実: `drawMesh()` の `cacheKey` は通常の 3D モデルでは source path と layer ID、procedural mesh では layer ID から生成されるため、現行のレイヤー描画単位では TLAS instance 識別子として機能する。
- 判断: 直ちに別の instance map を導入する必要はない。将来、同一 layer が 1 frame 内で複数回描画される機能を追加する場合は、`drawMesh()` API に明示的な instance ID を導入する。
- 未検証: 実行時に同一 layer が複数回 submit される特殊経路の有無。

## 2026-08-15: Diligent RT API 参照照合
- 関連: `ArtifactCore/src/Graphics/RayTracingManager.cppm`、`libs/DiligentEngine/DiligentSamples/Tutorials/Tutorial22_HybridRendering/src/Tutorial22_HybridRendering.cpp`
- 事実: BLAS/TLAS の source buffer に `BIND_RAY_TRACING` を付与すること、scratch / instance buffer の用途、`BuildBLASAttribs`・`BuildTLASAttribs` の主要フィールド、transform の設定方法は Diligent の公式サンプルと一致している。
- 未検証: Artifact の C++20 module コンパイル、使用 GPU backend 固有の RT shader / SBT 制約、実フレームでの API 呼び出し順。

## 2026-08-15: RAM preview も RenderGraph executor 経由へ移行
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 対応: RAM preview の Base / Composite 2-pass 実行を、既存の `RenderPassExecutor::runAllWithRenderGraph()` に統一した。GPU pipeline の主要 layer 3-pass に加え、fallback branch でも compiled pass order と executor failure propagation を通す。
- 未検証: 実フレームの pass resource state transition、GPU pipeline 全体の各 pass を RenderGraph へ置き換える作業。

## 2026-08-15: Composition の単一 pass 実行も RenderGraph 経由へ統一
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 対応: Setup、GPU Base、Resolve、RAM/direct fallback、Overlay、Present の単一 pass 実行に `runWithRenderGraph()` を導入した。複数 pass の layer 実行と合わせ、旧 `RenderPassExecutor::run()` の直接呼び出しを除去した。
- 未検証: RenderGraph が実 GPU resource allocation や state barrier を所有する段階への移行、実フレームの描画結果。

## 2026-08-15: フレーム診断グラフの resource 見積りを実寸化
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 対応: フレーム pass plan の診断用 resource をゼロ寸法の Buffer から viewport 寸法の Texture へ変更し、RGBA8 相当の byteSize を設定した。allocation slot の alias 見積りが実際の画面サイズを反映する。
- 未検証: 実 backend の format mapping、MSAA / HDR / AOV ごとの実際の resource 分割、GPU allocation との一致。

## 2026-08-15: RenderGraph executor に graph 本体を公開
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`
- 対応: `RenderGraphExecutionContext` に `const RenderGraph& graph` を追加した。pass executor は compiled graph の allocation slot だけでなく、resource descriptor を handle から解決できるため、将来の backend allocator / barrier adapter を context から接続できる。
- 未検証: 実 backend 側の allocator 実装、resource state transition、context ABI 変更の module build。

## 2026-08-15: RT pipeline resource variable 数の不整合修正
- 関連: `ArtifactCore/src/Graphics/RayTracingManager.cppm`
- 事実: RT warmup PSO の `Variables` 配列には `g_OutputTex` と `g_TLAS` の2項目があったが、`NumVariables` が1だった。
- 対応: `NumVariables = 2` に修正し、TLAS static resource variable が resource layout に含まれるようにした。
- 未検証: Diligent PSO 作成、static binding、SBT / TraceRays の実 backend 動作。

## 2026-08-15: RenderGraph executor 移行時の null pass 防御を維持
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 対応: `runAllWithRenderGraph()` の callback に null pass 検査を追加した。旧 `runAll()` と同様、無効な pass pointer を dereference せず失敗伝播する。
- 未検証: 実フレームでの executor failure propagation。

## 2026-08-15: drawMesh の RT 分岐を再整形・再確認
- 関連: `Artifact/src/Render/ArtifactIRenderer.cppm`
- 対応: BLAS/TLAS 分岐のインデントとブロック構造を明確化した。透明 instance の無効化、不透明化時の再登録、transform 更新、geometry 更新時の BLAS 再構築の範囲を読み違えにくくした。
- 未検証: C++20 module compile、GPU 実行時の TLAS 更新結果。

## 2026-08-15: RT warmup shader の payload / hit group 整合性確認
- 関連: `ArtifactCore/src/Graphics/RayTracingManager.cppm`
- 事実: RayGen / Miss / ClosestHit が同一 `Payload { float4 color; }` を使用し、SBT 登録名は PSO の shader 名と一致している。Miss と ClosestHit の双方が payload を初期化し、RayGen が UAV へ書き込む。
- 未検証: DXC コンパイル、各 backend の shader model / SBT 制約、実際の TraceRays 出力。

## 2026-08-15: VP監査で確認したcache・同期境界の分離
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Widgets/Render/ArtifactSoftwareRenderInspectors.cppm`
- **事実:** Composition のcamera-only GPU cacheは実装済みだが、静止画／solid・Normal blend・effect/maskなし等に限定され、設定opt-inかつruntime検証待ちである。通常2D direct pathではレイヤー後に`ArtifactIRenderer::flush()`が呼ばれ、surface cacheがhitしてもeffect/mask付きimage・SVG・text・videoでは入力の`QImage`化やmatte解決が先に残る。Software Composition TestはQPainter系の別実装で、3Dは実描画せずfallback card、videoは情報カードになり得る。
- **仮説（未検証）:** VP改善を一つの「高速化」変更として扱うと、camera cache、layer surface cache、RTV/UAV flush境界、software parityの問題を混同する。まず2D direct pathのflush削減可能条件、次にcache hit前のsource変換、最後に3D/software parityを個別に受入する必要がある。
- **価値・懸念:** 表示品質と性能の証拠を同じ指標に混ぜず、DiligentのD3D12/Vulkan共通境界を壊さずに、最小の改善単位を選べる。`QImage`／QPainterの新規ホットパス拡大や、子リポジトリ変更を誘発しない。
- **次に確認すべきこと:** ビルド・runtime許可後、(1) 2D direct pathでflush回数とGPU frame time、(2) effect/mask付き静止画でsource変換回数、(3) 3D／video／software previewのfresh captureと画素差、(4) focus移動・overlay外クリック・selection同期のUI sessionを分けて計測する。

## 2026-08-15: VPのflush診断値が常時ゼロになる経路
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- **事実:** `renderOneFrameImpl()` のoverlay pass後に `flushMs = 0` が無条件代入され、その後 `lastFlushMs_` とFrameDebugのflush passへ渡される。`renderer_->flush()` の呼び出しは複数あるが、現行の`flushMs`単独では総flush時間を表さない。
- **仮説（未検証）:** flush削減の性能判断を現行ログだけで行うと、実際のsubmissionコストを見落とす。frame全体の累積flush時間、またはflush回数と最終flushの区別が必要。
- **価値・懸念:** 先に診断の意味を修正しないと、direct pathのflush集約前後を比較できない。計測追加はDiligentの`submitQueuedDraws()`と`IImmediateContext::Flush()`の境界を壊さず、待機を導入しない形に限定する。
- **次に確認すべきこと:** `flush()` wrapper入口で累積時間／回数を記録し、frame endでリセットする案と、既存の`Submit2D` profiler計測との重複を比較する。ビルド・runtime検証は許可後に行う。
## 2026-08-15: ShapePath の fill rule はレイヤー境界で明示保存が必要
- 関連: `Artifact/include/Layer/ArtifactShapeLayer.ixx`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Core の `ShapePath` は既に Winding／EvenOdd と triangulation を持つが、`ArtifactShapeLayer` の custom Bézier 設定には fill rule がなく、JSON・Property Editor・native geometry 間で選択値を保持できなかった。
- 対応: custom path fill rule をレイヤー設定、native geometry／operator経路、JSON保存／復元へ接続し、既定値はWindingに維持した。
- 未検証: C++20 module compile、穴を含むEvenOdd描画のpixel parity、Preview／Render Queueのruntime結果。
## 2026-08-15: Final Post Process の未適用成功扱い
- 関連: `Artifact/src/Render/ArtifactFinalPostProcess.cppm`
- 事実: view transform が有効でもLUTが未設定の場合、GPU出力を書かずに `apply()` が `true` を返していた。
- 対応: 実際にpost-processを適用できない場合は `false` を返し、呼び出し側がstale destinationを採用しないようにした。
- 未検証: GPU runtime、LUT適用、OCIO/ACES display transform の実出力。
## 2026-08-15: 3D layer の source-less JSON stale restore
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: `fromJsonProperties()` は `sourcePath` が空で `fixedGeometry=Auto` の場合、既存レイヤーに読み込まれていたmeshを置き換えなかった。
- 対応: sourceのない復元ではCubeへ戻し、旧モデルが表示に残らないようにした。
- 未検証: C++20 module compile、モデル欠落／再読込のruntime、3D遮蔽parity。
## 2026-08-15: 3D missing source 復元時の旧mesh残留
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: `fromJsonProperties()` のmissing model pathで `loadFromFile()` が早期returnし、既存レイヤーのmeshが表示に残る可能性があった。
- 対応: source pathを保持したまま `meshLoaded_ = false` とし、missing状態を描画へ持ち越さないようにした。
- 未検証: missing／relink runtime、UIのmissing表示、3D render queue parity。
## 2026-08-15: 3D transform snapshot の固定30fps
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: 現在／前フレームの `RationalTime` が固定30fpsで作られ、非30fps compositionで3Dアニメーションの時刻がずれる可能性があった。
- 対応: `compositionFrameRate()` を安全なフォールバック付きで使うようにした。
- 未検証: 24／25／29.97／60fpsのruntime、モーションブラー／velocity連携。
## 2026-08-15: Camera／Light のfps整数丸め
- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`、`Artifact/src/Layer/ArtifactLightLayer.cppm`
- 事実: composition fpsを `int64_t` へ丸めており、29.97fpsなどの時刻基準が30fpsへ変わっていた。
- 対応: 実数fpsを `RationalTime` へ渡すよう変更し、Model3D／Camera／Lightの時間基準を揃えた。
- 未検証: 29.97fpsのruntime、カメラシェイク／ライトアニメーションの実機結果。
## 2026-08-15: 3D編集補助経路の固定30fps
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: selection outline、固定平面の投影／ray hit、Model3D pickingのtransform snapshotに固定30fpsが残っていた。
- 対応: 各レイヤーの`compositionFrameRate()`を使い、描画本体と編集補助の時刻基準を統一した。
- 未検証: 非30fpsのruntime選択・picking・投影、3D gizmo parity。
## 2026-08-15: Layer component JSON の stale state
- 関連: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- 事実: components／componentGraph を持たないJSONを既存レイヤーへ復元すると、以前のcomponent activation・追加modifier・descriptor graphが残る可能性があった。
- 対応: 欠落ブロック時にlegacy activation、追加modifier、script binding、component host graphを明示クリアしてからbuiltin descriptorを再同期するようにした。
- 未検証: C++20 module compile、component runtime phase parity、旧JSON互換。
## 2026-08-15: Precomp source composition の stale restore
- 関連: `Artifact/src/Layer/ArtifactCompositionLayer.cppm`
- 事実: `composition.sourceId` がないJSONを既存precomp layerへ復元すると、以前のsource composition IDが残る可能性があった。
- 対応: source IDを常に復元し、欠落時は空IDへ明示的に戻すようにした。
- 未検証: precompose／unprecompose runtime、nested compositionの描画・undo parity。
## 2026-08-15: Clone Layer source／effector stale restore
- 関連: `Artifact/src/Layer/ArtifactCloneLayer.cppm`
- 事実: JSONに`sourceLayerId`または`useEffector`がない場合、既存Clone Layerの以前の設定が残る可能性があった。
- 対応: 欠落時はsource layer IDを空、effector使用をfalseへ明示的に戻すようにした。
- 未検証: Clone Layerのpartial JSON互換、runtime generator／effector parity。
## 2026-08-15: Render Preflight の出力安全チェック不足
- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: preflightは出力ディレクトリの存在までで、書込み不可と既存出力ファイルを診断していなかった。
- 対応: 書込み不可をError、既存ファイルを上書きWarningとして追加した。
- 未検証: Windows／ネットワークドライブの権限判定、sequence／video出力の実書込み。
## 2026-08-15: Timeline playhead の非有限値伝播
- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- 事実: `setCurrentFrame()` がNaN／Infを直接`std::clamp`へ渡し、current frameとdirty rectangle計算へ不正値が伝播する余地があった。
- 対応: 有限値でない入力は現在フレームへ戻してから範囲clampするようにした。
- 未検証: UI scrub／外部transportからのNaN入力、長時間再生のruntime。
## 2026-08-15: Timeline viewport値の非有限値伝播
- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- 事実: duration、pixels-per-frame、scroll offsetも非有限値を直接clamp／座標計算へ渡す余地があった。
- 対応: 各入力を有限値へ正規化してからclampし、Timelineの描画・スクロール状態を安定化した。
- 未検証: レイアウト復元、外部transport、長時間scrubのruntime。
## 2026-08-15: Timeline duration短縮時のplayhead残留
- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- 事実: durationを短縮してもcurrent frameが旧終端のまま残り、playheadが表示範囲外になる可能性があった。
- 対応: duration更新時にcurrent frameを新終端へclampした。
- 未検証: duration変更中の外部transport同期、runtime再生／scrub。
## 2026-08-15: InputSurface capture後のtarget／context残留
- 関連: `ArtifactCore/src/UI/InputOperatorManager.cppm`
- 事実: commit／cancel後にmodeはOffへ戻るが、前回のtargetIdとcontextがstateに残っていた。
- 対応: Off正規化時にtarget／contextをクリアし、次回captureへのstale対象混入を防いだ。
- 未検証: Timeline／Inspector UIの状態表示、property書込み、runtime capture連続操作。
## 2026-08-15: InputSurface の負フレーム入力
- 関連: `ArtifactCore/src/UI/InputOperatorManager.cppm`
- 事実: transport／step frameとcapture開始引数を負値のまま状態へ保存できた。
- 対応: setterおよびcapture開始時に0未満を0へ正規化した。
- 未検証: 外部transport、step keyframe書込み、runtime scrub境界。
# 2026-08-15 — InputSurface の確定・取消でコンテキストを残さない

- 関連: `ArtifactCore/src/UI/InputOperatorManager.cppm` の `commitCapture()` / `cancelCapture()`。
- 事実: capture 終了時に mode と armed 等は Off 相当に戻していたが、`targetId` と `context` は明示的に消去されていなかった。
- 対応: Off の共通正規化を確定・取消経路にも通し、次の入力セッションへ対象・文脈が残留しないようにした。
- 価値/懸念: DAW-style 入力の再利用時に、前回の編集対象へ誤って書き込むリスクを下げる。ビルド未実施のため、呼び出し側の期待値は未検証。
- 次に確認: 実装をビルド／実行できる段階で、commit/cancel 後の stateChanged payload と再開始時の target/context を確認する。
# 2026-08-15 — TransformGizmo の対象差し替え境界

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`。
- 事実: マルチターゲット変換はドラッグ開始時に全対象の Undo スナップショットを保持するため、ドラッグ中の `setLayer()` / `setTargetLayers()` は旧スナップショットと新対象を混在させ得た。
- 対応: 対象差し替え前に進行中の操作を `cancelInteraction()` で復元・終了する。
- 価値/懸念: 選択変更時に誤ったレイヤーへ変換や Undo を適用するリスクを下げる。ビルド未実施のため、選択変更イベントとの実行順序は未検証。
- 次に確認: 実行時にドラッグ中の選択変更、取消後の dirty/event 通知、Undo 履歴の増加がないことを確認する。
# 2026-08-15 — TransformGizmo のターゲット配列正規化

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`。
- 事実: `setTargetLayers()` は null や同一 ID の重複を受け入れられ、マルチドラッグ時の変換・Undo対象が重複し得た。
- 対応: 対象差し替え時に null と重複 ID を除去し、入力順は維持する。
- 価値/懸念: 同一レイヤーへの二重適用を防ぐ。ビルド未実施のため、呼び出し側が null を件数として扱う前提は未検証。
- 次に確認: 複数選択の順序、同一 ID の重複入力、全件無効入力時の Gizmo 非表示を実行時に確認する。
# 2026-08-15 — マスクスタックの並べ替え API

- 関連: `Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- 事実: マスクの追加・削除・置換は存在したが、Phase 1 のスタック順変更を表すモデル API がなかった。
- 対応: `moveMask(fromIndex, toIndex)` を追加し、無効 index／同一 index は no-op、成功時は順序と `maskRevision` を更新する。
- 価値/懸念: UI の Drag&Drop 並べ替えを既存レイヤー責務内で実装できる。ビルド未実施のため、公開モジュール宣言との整合は未検証。
- 次に確認: パネル側からの Undo 接続と、マスク合成順が UI 順序と一致するかを確認する。
# 2026-08-15 — マスク順変更の Undo 境界

- 関連: `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`。
- 事実: マスク順変更 API は追加できたが、Undo 層に対応コマンドがなかった。
- 対応: `MoveMaskCommand` を追加し、弱参照レイヤーに対して old/new index を反転適用する。
- 価値/懸念: マスクスタック UI は順変更を履歴化できる。現時点では Drag&Drop UI からの push 接続は未実装。
- 次に確認: マスクスタック UI の並べ替えイベントから、変更成功時だけ `UndoManager::push()` する。
# 2026-08-15 — Inspector からマスク順変更を履歴化

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`。
- 事実: `MoveMaskCommand` は存在したが、ユーザーが実行できる導線がなかった。
- 対応: 既存 Inspector コンテキストメニューに、複数マスクの各項目の Up/Down 操作を追加し、`UndoManager::push()` 経由で順変更する。
- 価値/懸念: 新規シグナルなしでマスク順変更と Undo を接続できる。専用 Drag&Drop パネルは未実装。
- 次に確認: マスク順の表示名、Undo/Redo 後の合成順、選択レイヤー更新を実行時に確認する。
# 2026-08-15 — マスク一括状態操作の Undo

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`。
- 事実: マスク順変更の導線は追加済みだったが、Phase 1 の一括 Enable/Disable/Invert 操作はなかった。
- 対応: 既存 `MaskEditCommand` に before/after のマスク配列を渡し、変更がある場合だけ履歴化する。
- 価値/懸念: 複数マスクの状態変更を一回の Undo で戻せる。専用 Drag&Drop パネルと個別選択 UI は未実装。
- 次に確認: 一括操作後のマスク合成結果、Undo/Redo、0件／全同値状態で不要な履歴が積まれないことを実行時に確認する。
# 2026-08-15 — マスクパス合成モードの一括変更

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`MaskMode`。
- 事実: `LayerMask` は複数 `MaskPath` の合成モードを保持するが、Inspector から全パスを一括変更する導線がなかった。
- 対応: Add/Subtract/Intersect/Difference の一括操作を追加し、変更がある場合だけマスク配列の before/after を `MaskEditCommand` に渡す。
- 価値/懸念: マスクスタックの合成ルールをまとめて調整できる。個別パス選択・専用パネルは未実装。
- 次に確認: 複数パスの合成結果と Undo/Redo が一致するかを実行時に確認する。
# 2026-08-15 — CompositionCompareMode の責務境界

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 事実: `CompositionCompareMode` と A/B state variant の切替、`Diff` 値の保持は存在するが、レンダー本体で compare mode に応じた2出力・差分合成を行う分岐は確認できない。
- 仮説: 現状の compare mode は state variant 選択の準備段階で、Phase 3 の DiffComposite／SplitView を直接提供するものではない。
- 価値/懸念: UI に差分モードを露出する前に、フル合成と選択対象の2つのレンダー結果を保持する境界を追加する必要がある。推測を実装に広げず、今回のターンではコード変更を見送った。
- 次に確認: `RenderPassResources` または既存 offscreen render target を比較用に再利用できるか、GPU readback を増やさずに2パスを合成できるかを調査する。
# 2026-08-15 — 比較レンダー用レイヤーフィルター

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 事実: 既存のレイヤー合成ループは全レイヤーを処理しており、選択レイヤーのみを再描画する指定がなかった。
- 対応: `CompositionLayerRenderFilter` と `setLayerRenderFilter()` を追加し、`SelectedOnly` 時は選択集合外をスキップする。既定値は `All`。
- 価値/懸念: DiffComposite／SplitView の2パス目へ進むための最小境界を追加した。ただし比較用の別レンダーターゲットと差分合成は未実装。
- 次に確認: フィルター切替時の base composite 無効化、選択なし時の空出力、既存 solo／visibility 判定との順序を確認する。
# 2026-08-15 — 比較フィルターのコンポジション境界

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の composition reset。
- 事実: 比較用 `SelectedOnly` は controller の状態として保持されるため、コンポジション切替時に明示リセットしないと次のコンポジションにも残り得た。
- 対応: 既存 compare state の reset と同じ境界で `CompositionLayerRenderFilter::All` に戻す。
- 価値/懸念: コンポジション切替後の表示欠落を防ぐ。2パス差分合成自体は未実装。
- 次に確認: controller destroy／再initialize と composition 差し替えの両方で filter getter が All を返すことを実行時に確認する。
# 2026-08-15 — CompositionRenderController destroy 時の比較状態初期化

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 事実: composition 差し替え時の filter reset は追加済みだったが、controller の `destroy()` では compare mode／filter の明示リセットがなかった。
- 対応: destroy 境界でも `CompositionCompareMode::Off` と `CompositionLayerRenderFilter::All` に戻す。
- 価値/懸念: renderer 再初期化後に古い比較表示状態が復活しない。2パス合成は未実装。
- 次に確認: destroy→initialize の後に通常全レイヤー描画へ戻ることを実行時に確認する。
# 2026-08-15 — SelectedOnly の単一選択フォールバック

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 事実: 選択集合 API が空でも単一の `selectedLayerId_` は設定される経路があり、集合だけを見ると SelectedOnly が全レイヤーを除外していた。
- 対応: 複数選択集合が空の場合は selectedLayerId と比較し、単一選択を描画対象にする。
- 価値/懸念: 単一選択と複数選択で比較用フィルターの意味が一致する。ビルド未実施のため、selection manager の更新順序は未検証。
- 次に確認: 単一選択、複数選択、選択解除の3状態で SelectedOnly の描画対象を確認する。
# 2026-08-15 — 比較2パス向けレイヤー判定の共通化

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 事実: SelectedOnly の複数／単一選択フォールバック判定がレイヤー描画ループ内に埋め込まれていた。
- 対応: `passesLayerRenderFilter()` に切り出し、filter・selectedIds・selectedLayerId・layer を同じ契約で評価する。
- 価値/懸念: フル／選択のみの2パス化で対象判定が分岐しない。今回の動作は従来と同じで、別ターゲット描画は未接続。
- 次に確認: 2つのレンダーパスが同じ選択集合と単一選択フォールバックを共有することを確認する。
# 2026-08-15 — Timeline キーフレームスニペット基盤

- 関連: `Artifact/include/Widgets/ArtifactTimelineWidget.ixx`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: 選択キーフレームの JSON Copy/Paste と Undo は既存だったが、名前付きの一時保存はなかった。
- 対応: Timeline 内に `QHash<QString, QJsonArray>` を追加し、保存／適用／削除 API を実装。適用は既存 Clipboard／Paste 経路を通す。
- 価値/懸念: スニペット適用時も既存の複数レイヤー適用と Undo を再利用できる。現時点では名前入力・一覧 UI と永続化は未実装。
- 次に確認: UI からの名前入力、同名上書き確認、Timeline 再生成時の保持、プロジェクト保存との境界を設計する。
# 2026-08-15 — キーフレームスニペット UI 接続

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: スニペット API は追加済みだったが、名前入力と一覧操作の UI がなかった。
- 対応: Curve Editor ヘッダーに Snippet ボタンを追加し、保存・適用・削除を既存 API へ接続した。
- 価値/懸念: 既存 Paste 経路で Undo を維持できる。スニペットは現在 Timeline widget の寿命内だけ保持し、プロジェクト永続化は未実装。
- 次に確認: 同名保存の上書き確認、widget 再生成、プロジェクト保存／再読込への統合を確認する。
# 2026-08-15 — キーフレームスニペットの設定保存

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: スニペットは widget のメモリ内だけに保持され、再生成・再起動で失われていた。
- 対応: `QSettings` の `Timeline/KeyframeSnippets` グループに各 JSON 配列を保存し、Impl コンストラクタで復元する。
- 価値/懸念: プロジェクト形式を変更せずユーザー設定として再利用できる。プロジェクト単位の共有・移行は未実装。
- 次に確認: 壊れた JSON、空名、同名上書き、設定削除後の復元を実行時に確認する。
# 2026-08-15 — Alt ドラッグの自動スムージング

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`。
- 事実: Shift／Ctrl の軸拘束と複数キー移動は実装済みだったが、Alt ドラッグ確定時に補間を自動調整していなかった。
- 対応: Alt（Ctrl なし）でドラッグしたキーについて、前後キーの速度から `tryComputeEasyEaseHandles()` を使い、Bezier 補間とハンドルをスナップショットへ反映する。
- 価値/懸念: 既存 Easy Ease と同じ計算・Undo 経路を再利用できる。隣接キーがない／非スカラー値では従来補間を維持する。ビルド未実施。
- 次に確認: Alt 単独、Alt+Ctrl、隣接キーなし、複数選択の各ケースを実行時に確認する。

# 2026-08-15 — Timeline チャンネルフィルターの最小導入

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: 既存検索はレイヤー行を絞り込むが、キーフレームマーカーのプロパティチャンネル絞り込みはなかった。
- 対応: `transform:` / `audio:` / `effect:` を既存検索欄で解釈し、マーカー収集時にチャンネル選別する API を追加。
- 価値/懸念: 新規シグナルを増やさず既存更新経路を使える。Property 行とマーカーの両方を同じ分類で更新する。
- 次に確認: 実 UI で接頭辞入力時の行表示、空グループの非表示、既存検索語との併用を確認する。

# 2026-08-15 — チャンネルフィルターとカーブエディタの同期

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: Property 行とマーカーを絞り込んでも、Curve Editor は選択レイヤーの全プロパティを収集していた。
- 対応: Curve／Speed Graph の両方へ同じ `PropertyChannelFilter` を渡し、対象トラックを同期した。
- 価値/懸念: フィルター変更後にカーブだけ別チャンネルが残る不整合を防げる。プロパティ分類は既存パス命名に基づく簡易判定である。
- 次に確認: Transform／Audio／Effect 各モードで選択・カーブ編集・Undo の対象が一致することを実行時に確認する。

# 2026-08-15 — フィルター変更時のカーブ更新保証

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: 検索欄からチャンネルを変更した際、Curve Editor の再構築は EventBus の遅延更新に依存していた。
- 対応: 検索変更処理で Timeline と Curve Editor を明示的に再同期し、非表示チャンネルの選択・フォーカスが残らない経路を確保した。
- 価値/懸念: UI 操作直後の表示遅延を減らせる。既存の更新処理を直接呼ぶため、頻繁な検索入力時の負荷は実行時に確認が必要。
- 次に確認: 連続入力、空検索への復帰、フィルター中のカーブ編集後 Undo を確認する。

# 2026-08-15 — チャンネル接頭辞とプロパティ検索の併用

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: 接頭辞を単独トークンとして解釈していたため、`transform:position` のような検索はチャンネル指定として認識されなかった。
- 対応: `transform:`／`audio:`／`effect:` の後続文字列を通常のプロパティ検索語として左ペインへ渡すようにした。
- 価値/懸念: チャンネル指定とプロパティ名検索を一つの検索欄で併用できる。分類は引き続きプロパティパス命名に依存する。
- 次に確認: 大文字小文字、空白付き接頭辞、未知の接頭辞を含む検索を確認する。

# 2026-08-15 — Timeline からのアニメーションレイヤーベイク

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`, `Artifact/include/Undo/UndoManager.ixx`。
- 事実: レイヤー側には Work Area 範囲のアニメーションレイヤーベイクとスナップショット Undo が既にあったが、Timeline の選択レイヤーから直接呼ぶ導線がなかった。
- 対応: 既存 Pattern ボタンのメニューに範囲ベイクを追加し、選択レイヤーごとに Work Area をベイクして既存 Undo コマンドへ登録するようにした。
- 価値/懸念: 複数レイヤーを同じ範囲で一括ベイクできる。Undo が利用可能な場合はレイヤーごとに履歴へ積み、利用できない場合もベイク結果を保持する。
- 次に確認: 空 Work Area、非選択状態、複数レイヤーの Undo／Redo、ベイク後の Curve 更新を確認する。

# 2026-08-15 — 選択キーフレームのフリンジ生成

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: Phase 7 の範囲ベイク導線はあったが、選択範囲の端で補間を安定させる近接キーフレーム生成はなかった。
- 対応: Pattern メニューから、選択プロパティごとの最初／最後の選択値を範囲端の隣接フレームへ複製する機能を追加した。既存のキーフレームスナップショット Undo を利用する。
- 価値/懸念: 範囲端の補間値を固定しやすくなる。既存キーがある場合、またはコンポジション範囲外では追加しない。
- 次に確認: 単一／複数プロパティ、範囲端、既存キー、Undo／Redo を確認する。

# 2026-08-15 — Phase 8 ブロック移動の既存実装監査

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `ArtifactCore/src/UI/ShortcutBindings.cppm`。
- 事実: Keyframe Area の Body／Edge ドラッグは選択キー群をまとめて移動・伸縮し、複数トラックを扱える。`Ctrl+G` は Curve Editor 切替に既に予約されている。
- 対応: 既存のブロック操作を再利用対象として確認し、ショートカット競合を避けるため `Ctrl+G` の上書きは行わなかった。
- 価値/懸念: 既存 Undo・スナップ経路を維持できる。永続的な名前付きグループはまだなく、Phase 8 の「グループ化」は Area 操作ベースである。
- 次に確認: Phase 9 のプロパティブロックコピー／ペーストへ進む。

# 2026-08-15 — Property Block Copy/Paste の既存経路監査

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`, `ArtifactCore/include/Clipboard/ClipboardManager.ixx`。
- 事実: 選択キーフレームの Clipboard レコードは各要素に `propertyPath` を持ち、Paste 時に対象レイヤーごとに同じプロパティパスを解決する。
- 対応: 複数プロパティを含む既存 Copy／Paste を Property Block の実装として確認し、別形式や重複 UI は追加しなかった。
- 価値/懸念: 既存の JSON／システムクリップボード／Undo 経路を維持できる。プロパティ値全体（非キーフレーム）のブロックコピーは別機能として未実装。
- 次に確認: Phase 10 の数値入力スピニングを監査する。

# 2026-08-15 — 修飾ホイールによる数値スピニング

- 関連: `Artifact/include/Widgets/ArtifactRelativeSpinBox.ixx`。
- 事実: 相対 SpinBox は誤操作防止のためホイールを無条件に無視していた。
- 対応: 通常ホイールは従来どおり無効のまま、Shift=0.1x、Ctrl=10x、Alt=0.01x の修飾時だけ Double／Integer SpinBox を更新するようにした。
- 価値/懸念: 意図しないスクロール変更を避けつつ、Inspector の微調整・粗調整を共通化できる。Integer SpinBox は整数丸めのため極小倍率でも最小 1 step となる。
- 次に確認: 各修飾キー、上下方向、範囲端、通常ホイール無効の挙動を確認する。

# 2026-08-15 — Timeline マルチプロパティ検索

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`。
- 事実: 検索対象はレイヤー名とプロパティグループ名が中心で、個別プロパティ名や正規表現による行絞り込みはなかった。
- 対応: プロパティ名／表示ラベルを検索キャッシュへ追加し、通常文字列と `/正規表現/` の両方で Property 行を絞り込むようにした。
- 価値/懸念: `transform:position` など上位の Timeline フィルターと組み合わせて、実際に編集対象となる行だけを表示できる。正規表現が不正な場合は一致なしとして扱う。
- 次に確認: 正規表現、表示ラベル、空検索復帰、保存済み検索フィルターの導線を確認する。

# 2026-08-15 — Timeline 検索フィルターの保存

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`。
- 事実: Phase 11 の検索は実装済みだったが、検索語を名前付きで保存・再適用する導線がなかった。
- 対応: 検索欄のコンテキストメニューに保存／適用を追加し、`QSettings` の `Timeline/SavedSearchFilters` に名前付きフィルターを保存するようにした。
- 価値/懸念: 正規表現やチャンネル接頭辞を含む検索条件を再利用できる。削除 UI はまだなく、同名保存は上書きする。
- 次に確認: 保存・再起動後の復元、同名上書き、空検索の保存拒否を確認する。

# 2026-08-15 — Dock Add Menu の registry 境界監査

- 関連: `Artifact/src/Widgets/ArtifactMainWindow.cppm`, `Artifact/include/Widgets/ArtifactDockManager.ixx`。
- 事実: Dock manager は dock ID の登録・重複拒否・一覧取得を既に持ち、MainWindow 側には既存 dock の再表示／activate 経路がある。
- 対応: Add Menu の Phase 1 として、表示名ではなく objectName／dock ID を永続キーにする責務境界を確認した。
- 価値/懸念: 新規 dock registry を重複作成せず既存管理を再利用できる。カテゴリ／表示名 descriptor はまだない。
- 次に確認: 現行 dock 登録箇所を一覧化し、Phase 2 の descriptor と追加メニューを最小範囲で実装する。

# 2026-08-15 — Dock パネル再表示メニューの最小導線

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。
- 事実: 既存の Window Panels メニューは表示切替を持っていたが、追加／再表示の意図が独立していなかった。
- 対応: 登録済み dock だけを列挙する「パネルを追加／再表示」サブメニューを追加し、既存 dock の表示・activate API を再利用した。
- 価値/懸念: 未登録 panel の見せかけや重複生成を避けられる。タイトルバーの専用 `+` 導線と ID ベースの履歴は未実装。
- 次に確認: MainWindow title bar の適切なホスト位置を特定し、同じ submenu を `+` 入口へ移す。

# 2026-08-15 — Dock 最近使用／お気に入りの ID 保存

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。
- 事実: Dock の再表示メニューはあったが、頻繁に使う面を ID ベースで再利用する保存層がなかった。
- 対応: 最近使用（最大8件）とお気に入りを `QSettings` に Dock ID で保存し、View メニューから activate／切替できるようにした。存在しない Dock ID は表示時に除外する。
- 価値/懸念: 表示名変更や未登録面の混入に強い。専用 title-bar `+` とカテゴリ descriptor はまだ未実装。
- 次に確認: Dock title bar の公開拡張 API を依存ヘッダで確認し、可能なら同じメニューを `+` に接続する。

# 2026-08-15 — Dock 追加メニューのカテゴリ整理

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。
- 事実: 登録済み Dock の再表示項目は単一のフラット一覧だった。
- 対応: Project / Assets、Editing、Animation、Render / Diagnostics、Other のカテゴリ submenu に分け、各項目の activate 経路は既存 API を維持した。
- 価値/懸念: パネル数が増えても探索しやすい。分類は現行表示名のキーワードに基づくため、将来は Dock descriptor の明示カテゴリへ移行する。

# 2026-08-15 — Dock メニュー設定の再読込修正

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。
- 事実: Dock 一覧が変わらない場合にメニュー再構築を早期終了していたため、最近使用順の更新が次回表示へ反映されなかった。
- 対応: Window Panels メニューを表示時に設定から再構築し、最近使用順・お気に入りの変更を即時反映するようにした。
- 価値/懸念: 設定と UI の stale 表示を防げる。Dock 数が非常に多い場合の再構築コストは runtime で確認する。

# 2026-08-15 — Dock メニューのアクセシビリティ metadata

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。
- 事実: 新しい Dock サブメニューは表示名だけで、スクリーンリーダー向けの役割説明がなかった。
- 対応: 最近使用／お気に入り／追加・再表示メニューに accessible name／description、個別 action に tooltip を追加した。
- 価値/懸念: メニューの目的と操作結果を識別しやすくなる。狭幅レイアウトと実際のキーボード受入は未確認。

# 2026-08-15 — MainWindow 上部 chrome への Dock `+` 入口

- 関連: `Artifact/src/Widgets/ArtifactMenuBar.cppm`, `Artifact/include/Widgets/ArtifactMainWindow.ixx`。
- 事実: ADS 内部 title bar の公開拡張 API は workspace から確認できなかったが、MainWindow の QMenuBar には右上 corner widget の拡張点がある。
- 対応: 右上に `+` QToolButton を追加し、登録済み Dock を表示時に列挙して既存 `setDockVisible()`／`activateDock()` へ接続した。
- 価値/懸念: 新規 Dock 生成や ADS 本体変更なしで追加導線を提供できる。狭幅メニューバーでの表示密度は runtime 未確認。

# 2026-08-15 — Dock `+` 入口の最近使用／お気に入り同期

- 関連: `Artifact/src/Widgets/ArtifactMenuBar.cppm`, `docs/planned/MILESTONE_DOCK_PANEL_ADD_MENU_2026-08-15.md`。
- 事実: 上部 chrome の `+` 入口は登録済み Dock のフラット一覧だけを持っていた。
- 対応: View メニューと同じ `QSettings` の最近使用／お気に入り ID を表示時に読み込み、既存の Dock activate 経路と最近使用更新を共有した。
- 価値/懸念: 入口が違っても利用頻度の高い Dock に同じ手順で到達できる。カテゴリ分類の完全な parity は未実装で、狭幅表示は runtime 未確認。

# 2026-08-15 — Render Queue の単一フレーム表記

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 事実: Render Queue の frame range mode `4` は単一フレーム出力だが、UI 表記が `Single Frame` で現在の playhead との関係が曖昧だった。
- 対応: 内部 mode／保存形式を変更せず、一覧 summary と combo の表示を `Current Frame` に統一し、選択肢の accessible description を追加した。
- 価値/懸念: Current Frame が Composition／Work Area／Selected Frames と並ぶ出力範囲の意味を読み取りやすくなる。実際の queue 実行時 frame 解決は runtime 未確認。

# 2026-08-15 — Composition Settings の共通 finalize 経路

- 関連: `Artifact/include/Service/ArtifactProjectService.ixx`, `Artifact/src/Service/ArtifactProjectService.cppm`, `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`, `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- 事実: Composition Menu と Project View は設定フォームと解像度 remap 判定を個別に持つ一方、確定後の project dirty 通知・playback range／FPS 同期もそれぞれ実装していた。
- 対応: `finalizeCompositionSettingsChange()` を Project Service に追加し、両 UI から共通利用するようにした。解像度変更の Undo／remap、フォーム責務、新規 signal 配線は変更していない。
- 価値/懸念: 片方だけ同期処理が抜ける divergence を減らせる。設定フォームと remap 判定そのものの共通化、および runtime 受入は未完了。

# 2026-08-15 — Composition Menu の Render Queue 追加導線整理

- 関連: `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`。
- 事実: 全範囲／Current Frame／Work Area／選択レイヤー系の6 action が Composition Menu の同じ階層に並び、範囲とレイヤー対象の違いが一覧で追いにくかった。
- 対応: 6 action を「レンダーキューに追加」submenu にまとめ、既存 QAction、shortcut、handler、enable 判定は変更せず、submenu の accessible metadata を追加した。
- 価値/懸念: 追加操作の探索性を上げつつ command 互換性を維持できる。submenu の狭幅表示と runtime 操作確認は未実施。

# 2026-08-15 — Timeline audio waveform の layout 同期ブロック遅延化

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: waveform cache が未構築または signature 不一致の場合、`refreshTracks()`／`updateLayout()` の同期中に `buildAudioWaveformForLayer()` が実行されていた。
- 対応: cache miss を per-layer の pending set で重複抑制し、次の UI event loop tick に生成を遅延。完了後に同じ composition の track を再構築する。composition 切替時は pending を破棄する。
- 価値/懸念: レイアウト更新入口の同期ブロックと重複生成を減らせる。layer snapshot の安全な worker 契約が未定義のため、decode／生成自体の別スレッド化と runtime 負荷検証は残る。

# 2026-08-15 — Property Reset の値／キーフレーム Undo 単位統一

- 関連: `Artifact/include/Undo/UndoManager.ixx`, `Artifact/src/Undo/UndoManager.cppm`, `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`。
- 事実: Property Editor の Reset は keyframe を削除する Undo だけを作り、default value の変更自体は同じ Undo 単位に含めていなかった。
- 対応: layer property value 用の `SetLayerPropertyValueCommand` を追加し、keyframe command と `MacroUndoCommand` にまとめた。keyframe がない Reset も値変更を Undo 対象にした。
- 価値/懸念: Reset 前の値とアニメーション状態を1回の Undo で復元できる。通常の複数選択編集と runtime 受入は未完了。

# 2026-08-15 — Render Queue 履歴 metadata と行アクション

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 事実: 履歴行は時刻付きテキストだけで、service event の job ID／frame range／failure stage が履歴から読み取りにくく、行から Retry／Reveal を直接実行できなかった。
- 対応: service event の履歴表示に job metadata を付加し、source index を `QListWidgetItem::UserRole` に保持。履歴行の context menu から Retry Job／Reveal Output を既存 service API へ接続した。
- 価値/懸念: 失敗履歴から次の操作へ直接進める。既存保存履歴の metadata 復元と service が公開する永続 stable job ID は未完了。

# 2026-08-15 — Screenshot async readback の失敗段階表示

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- 事実: async readback の完了後に null image／保存失敗を通知していたが、readback と encode/write のどちらで失敗したかが表示されなかった。
- 対応: null image に `Stage: readback`、保存失敗に `Stage: encode/write` を付加し、readback 完了後の進捗表示を `Saving ...` に更新した。
- 価値/懸念: UI 操作だけで失敗段階を切り分けやすくなる。Whole Window／multi-channel の同期経路と runtime 受入は未確認。

# 2026-08-15 — Four-Up deferred start の世代管理

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- 事実: Four-Up の controller start は event loop に段階分散されていたが、切替直後の古い `QTimer::singleShot` callback が新しい layout に対して残る可能性があった。
- 対応: viewport layout generation を追加し、世代が変わった deferred callback を無効化。各 pane start の遅延時間を debug log に記録する。
- 価値/懸念: レイアウト切替時の stale renderer 起動と不要な初期化を減らせる。view／controller の完全な lazy materialization と runtime 計測は未完了。

# 2026-08-15 — Viewport display-transform clear state

- 関連: `Artifact/src/Widgets/Render/ViewportColorPipeline.cppm`。
- 事実: `clear()` は baked LUT を破棄していたが、post-process の view-transform enabled flag は明示的に戻していなかった。
- 対応: LUT と flag を同時に clear し、OCIO config／display transform 無効化後の状態を一致させた。
- 価値/懸念: stale display-transform state の残留を防げる。実素材での HDR／log round-trip と preview／export parity は未検証。

# 2026-08-15 — Layer Component dependency graph validation

- 関連: `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`。
- 事実: `LayerComponentHost::validate()` は missing／disabled／late dependency を検出していたが、空の required type と循環依存は検出していなかった。
- 対応: 空 dependency type をエラー化し、descriptor type graph を DFS して循環依存を validation issue として返すようにした。
- 価値/懸念: phase evaluator に曖昧な依存グラフが入る前に診断できる。実 component graph と runtime phase parity は未検証。

# 2026-08-15 — Generator／Field／Modifier stack descriptor validation

- 関連: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/docs/MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md`。
- 事実: 追加 descriptor stack は保存・再読込・UI 表示へ接続されていたが、stack ごとの空 id／type と重複 id の validation は builtin component host と分離されていた。
- 対応: `validateLayerComponents()` に generator／field／modifier 共通の id／type validation を追加し、既存 diagnostics surface へ統合した。
- 価値/懸念: descriptor merge／評価へ進む前に不正な stack identity を検出できる。field binding・merge／weight 契約と runtime parity は未検証。

# 2026-08-15 — Live field noise／solid shape parity

- 関連: `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `docs/planned/MILESTONE_LIVE_FIELD_AUTHORING_UX_2026-07-04.md`。
- 事実: composition field の保存形式と共通評価器は radial／box／linear のみを shape として扱っていた。
- 対応: `noise` と `solid` を JSON round-trip と共通 scalar evaluator に追加し、既存の target／coordinate parent／blend／invert 経路へ接続した。
- 価値/懸念: field descriptor の shape 拡張を renderer 側の大改修なしで先行できる。noise は決定的 CPU 評価のみで、時間変化・GPU parity・viewport handle は未検証。

# 2026-08-15 — App Debugger goal-first capture summary

- 関連: `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`, `docs/planned/MILESTONE_HARNESS_ENGINEERING_2026-05-12.md`。
- 事実: Debug Render Harness は goal-first の report summary を持つ一方、App Debugger の Capture Details は capture／baseline の比較情報中心だった。
- 対応: App Debugger 側にも `goal / expected / actual / nextAction` を追加し、既存の capture／failure／compare 情報を再利用した。
- 価値/懸念: 診断 surface 間で次の行動を読み取りやすくなる。status taxonomy の完全統合と runtime smoke は未検証。

# 2026-08-15 — Command IR keyframe preflight validation

- 関連: `Artifact/src/AI/CommandIRExecutor.cppm`, `docs/planned/MILESTONE_COMMAND_IR_AUTOMATION_FOUNDATION_2026-06-28.md`。
- 事実: keyframe command は各 setter を順番に呼び出すため、入力 payload の不備を mutation 前に一括確認していなかった。
- 対応: 単一／batch keyframe command に property path、batch、frame、value の preflight validation を追加した。
- 価値/懸念: malformed request による partial mutation を防げる。setter の runtime failure を跨ぐ rollback は別契約として未実装。

# 2026-08-15 — Command Palette MRU restore normalization

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`。
- 事実: JSON から MRU を復元する経路は文字列をそのまま追加し、空 ID／重複 ID を許容していた。
- 対応: trim、空 ID 除外、重複除外を復元時に追加した。
- 価値/懸念: 再起動後の palette ranking が安定する。Recipe 全体の再起動後復元と runtime 受入れは未検証。

# 2026-08-15 — Workspace layout structural fallback

- 関連: `Artifact/src/Core/ArtifactWorkspaceManager.cppm`。
- 事実: session／preset JSON が空でない場合、`layout` オブジェクト欠落でも復元成功扱いになり得た。
- 対応: `applyWindowState()` で layout object の存在を必須化し、不完全な状態は default-layout recovery に委譲するようにした。
- 価値/懸念: 壊れた session が部分復元状態を成功として固定するのを防ぐ。破損 session の UI 通知と runtime 受入れは未検証。

# 2026-08-15 — Interactive Shell source recursion guard

- 関連: `Artifact/src/Application/ArtifactInteractiveShell.cppm`。
- 事実: nested `source` は再帰を検出していたが、top-level script が active set に登録されず、自己 source と symlink 経由の再帰を防げなかった。
- 対応: top-level／nested source で共有する active script set と canonical path を導入した。
- 価値/懸念: script include の無限再帰を抑止できる。外部 script sandbox／権限と runtime 受入れは未検証。

# 2026-08-15 — Asset Browser search history completer

- 関連: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`。
- 事実: 検索は incremental filter を持っていたが、過去の検索語を再利用する候補／永続化経路がなかった。
- 対応: `QCompleter` と bounded `QSettings` history を既存 search field に接続し、2文字以上の検索語を重複排除して保存するようにした。
- 価値/懸念: 大量素材の再検索を短縮できる。runtime UX と検索履歴切替の受入れは未検証。

# 2026-08-15 — Timeline playhead hit radius ownership

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: playhead overlay の hit radius が event filter と通常 mouse press に重複定義されていた。
- 対応: 共通定数へ集約し、入力経路間の調整値のずれを防いだ。
- 価値/懸念: 今後の不感帯調整を一箇所で行える。実機入力とテーマ別の視認性は未検証。

# 2026-08-15 — Property row label width alignment

- 関連: `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`、`Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorShared.cppm`。
- 事実: shared row layout は label 幅 132px だが、concrete editor row は 124px だった。
- 対応: concrete row の標準 label 幅を 132px に統一した。
- 価値/懸念: Property Editor と section／channel／transform／effect row の値列開始位置を揃えられる。実機での長いラベルと狭幅レイアウトは未検証。

# 2026-08-16 — Audio monitor/export responsibility boundary

- 関連: `ArtifactCore/src/Audio/AudioMixer.cppm`、`Artifact/src/Service/ArtifactPlaybackService.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: ExportはAudioMixerの最終segmentを直接取得し、PlaybackだけがAudioRendererのmaster volumeを通るため、現状のmonitor音量とexport音量は別経路になっている。
- 仮説: Cue／Control Room出力を追加する場合は、既存Masterを再利用せず、明示的なmonitor／cue出力役割をAudioMixerまたはPlayback境界に追加する必要がある。
- 価値/懸念: exportへmonitor補正が混入する事故を避けられる。Cue出力のルーティング、複数デバイス、UI責務は未設計・未検証。

# 2026-08-17 — 単一画像レイヤー化は「解析」より生成契約が重要（未検証）

- 関連: `docs/planned/MILESTONE_SINGLE_IMAGE_LAYERIZATION_2026-08-17.md`、`ArtifactImageLayer`、`LayerMask`、`OpenCVRotoBrushEngine`、`MaskCutoutPipeline`
- 事実: 画像バッファ、マスク、マスクからパスへの変換、RotoBrush補助、GPU cutoutの部品は存在する。一方、それらを複数の通常画像レイヤー、背景補完、保存／再読込へまとめる生成契約は確認できない。
- 仮説（未検証）: 最初にモデル精度を追うより、候補のmask／bounds／confidence／provenanceと、一括Undo・source identity・推定画素の保存契約を固定した方が、モデル交換やCPU／GPU fallbackに耐える制作機能になる。
- 価値・懸念: 一枚の画像から復元できない隠し画素を確定情報として扱う事故を避け、AI結果を通常のマスク編集へ安全に引き渡せる。候補の前後順と背景補完品質は素材依存で、runtime評価が必要。
- 次の確認: Phase 0の代表素材で、単一候補のmask生成から画像レイヤー作成、保存／再読込、Preview／Render Queue一致までの最小往復を確認する。モデル選定と新規モジュール追加は、その接続点を確認してから決める。

# 2026-08-18 — Solid2Dグラデーション平面の直接GPU描画

- 関連: `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`、`ArtifactIRenderer::drawGradientRectTransformed`
- 事実: Solid2Dの非単色塗りは、レイヤー自身の描画経路では一度`QImage`のグラデーションを生成してからスプライトとしてGPUへ送っていた。既存のGPUグラデーション矩形APIは同じパラメータを受け取れる。
- 対応: グラデーション画像生成を廃止し、既存のGPU矩形シェーダー経路へ直接送るようにした。単色平面と同じく、変換・クローン重み・不透明度をGPU描画コマンドへ渡す。
- 価値/懸念: 通常の平面描画でCPU画像生成とテクスチャ作成を避けられる。マスク／ラスタライズエフェクト付きの合成経路は別途CPUフォールバックが残り、runtime画質・色補間・fillTypeの一致確認は未実施。
- 次の確認: GPU／CPU出力のグラデーション境界、透明度、反転、中心・スケール・オフセット、クローン表示を実機で比較する。

# 2026-08-18 — 画像デプス機能のCore分離

- 関連: `ArtifactCore/include/Image/DepthMap.ixx`、`ArtifactCore/include/AI/ImageDepthEstimator.ixx`
- 対応: 画像由来の深度値を保持する`DepthMap`と、AIモデルを差し替え可能にする`IImageDepthEstimator`／オプション契約をCoreへ追加した。深度の正規化・反転・範囲制限はCoreで扱い、ONNX／DirectMLの具体的なモデル接続とGPU描画はArtifact側へ分離する。
- 価値・懸念: 手動深度マップ、AI推定深度、将来のクラウド推定を同じデータ契約で扱える。現時点では推定モデル実装とGPU視差描画への接続は未完了で、`DepthMask`は既知のワールド深度用であり画像推定とは別責務。
- 次の確認: Artifact側で深度テクスチャを3Dカード／細分化メッシュへ接続し、最初のローカル推定プロバイダを選定する。
- 追記: `DepthMap::fromQImage()`／`toQImage()`を追加し、グレースケール画像を明示的な入力・表示境界として扱えるようにした。深度メッシュの法線も隣接頂点から近似計算する。
- 追記: `ArtifactImageLayer`に深度マップとメッシュ設定のAPIを追加し、ファイル参照画像では既存3Dカメラ＋`drawMesh()`経路へ切り替えられるようにした。現在はメモリ上の深度設定で、深度マップの保存復元とインメモリGPUテクスチャ材質は未対応。
- 追記: `DepthMap`の画像ファイル入出力と、`ArtifactImageLayer`の深度マップパス・メッシュ設定のJSON保存／復元を追加した。復元時に深度ファイルが欠落している場合は通常画像へ安全にフォールバックする。
- 追記: `MeshRenderer::setBaseColorTextureView()`と`ArtifactIRenderer::drawMesh()`のテクスチャビュー受け渡しを追加した。GPUキャッシュ済みのインメモリ画像をファイル再読込なしで深度メッシュへ接続するための境界ができたが、現在の`ArtifactImageLayer`は引き続きファイルパスを優先する。
- 追記: CompositionRenderControllerで深度有効な`ArtifactImageLayer`の`ImageF32x4_RGBA`をGPUテクスチャキャッシュから取得し、深度メッシュ＋直接SRVの`drawMesh()`へ渡す経路を追加した。これにより、フレームバッファ画像もファイル再読込なしで深度メッシュへ合流する。
- 追記: `OnnxImageDepthEstimator`を追加し、ONNX Runtime + DirectMLで一般的なRGB NCHW入力／1ch深度出力モデルを実行できるCoreプロバイダを用意した。バックエンド未導入、モデル不在、推論失敗時はエラーを保持して呼び出し側がCPUフォールバックへ切り替えられる。
- 2026-08-18 — ObjectDetectorのONNX接続
- 関連: `ArtifactCore/include/AI/ObjectDetector.ixx`、`ArtifactCore/src/AI/ObjectDetector.cppm`
- 対応: 既存の仮検出を維持しつつ、ONNX／DirectMLモデルを初期化して、一般的なN行6列（`x1,y1,x2,y2,score,class`）またはYOLO系の検出テンソルを読み取る経路を追加した。推論失敗時は既存フォールバックへ戻る。
- 懸念: YOLOv8の転置出力、ラベルファイル、NMS、モデル固有の前処理はまだモデルアダプター側で吸収していない。モデルごとの入出力契約を固定してから本番モデルを選ぶ必要がある。
- 追記: YOLOv8系の`[1,channels,boxes]`転置出力とYOLOv5系の`[boxes,85]`形式を判別し、クラス別NMSを追加した。ラベルファイルとレターボックス補正は引き続き未対応。
- 追記: `ObjectDetector`へクラスラベルファイル読み込みとレターボックス前処理／座標逆変換を追加した。これにより、アスペクト比を保持した推論結果を元画像へ戻し、`class_7`ではなくモデルラベル名を返せる。
- 2026-08-18 — ONNX画像セグメンテーション契約
- 関連: `ArtifactCore/include/AI/ImageSegmenter.ixx`、`ArtifactCore/include/AI/OnnxImageSegmenter.ixx`
- 対応: 1chマスク出力を`DepthMap`形式で受ける`IImageSegmenter`と、ONNX／DirectMLのRGB入力・マスク出力プロバイダを追加した。正規化、反転、しきい値処理をCore側で行い、既存のマスク／切り抜き処理へ渡せる。
- 懸念: U2Net等のモデル固有の出力チャネル、アルファ合成、人物ラベル、複数クラスマスクの扱いはモデルアダプターで追加する必要がある。
- 追記: `applySegmentationMask()`を追加し、推定マスクを双線形サンプリングして`ImageF32x4_RGBA`のアルファへ適用できるようにした。切り抜き用途へ渡す前のCore合成段階を固定した。
- 追記: `ArtifactImageLayer::applySegmentationMask()`を追加し、画像レイヤーの現在バッファへマスク切り抜きを明示的に適用できるようにした。元ソースパスは変更せず、レイヤーの編集バッファとキャッシュを更新する。
- 追記: `LuminanceImageSegmenter`を追加し、ONNXモデルがない場合も同じ`IImageSegmenter`契約で簡易マスクを生成できるようにした。これは人物認識ではなく、明度しきい値による明示的な低品質フォールバックである。
- 追記: `ImageSegmentationOptions::applySigmoid`を追加し、確率出力とlogit出力の両方をモデル設定で扱えるようにした。
- 追記: `OnnxImageSegmenter`がモデルの入力テンソル形状を読み取り、固定512ではなくモデル指定の幅・高さで前処理するようにした。動的形状や不正値は安全な既定範囲へフォールバックする。
- 追記: `OnnxImageDepthEstimator`も入力テンソル形状を取得する方式へ統一し、固定384の前処理をモデル指定サイズへ変更した。

# 2026-08-18 — コンポジションのソースエリアと独立ノイズレイヤー（アイデア）

- 関連: コンポジション構成、参照素材、画像処理・レイヤー合成
- 事実: 制作中に参照画像、カラーチャート、HDRI、マスク作成元などを本番レイヤーと同じ場所へ置くと、編集対象と参照対象の区別が曖昧になりやすい。現在、専用のソースエリアは未確定。
- アイデア: コンポジション内に「ソースエリア」を設け、参照専用レイヤーを本番レンダーから除外する。さらに、粒状感・フィルムグレイン・ディザ・TVノイズなどを担う「ノイズレイヤー」を通常の画像／エフェクトとは独立したレイヤー種別として検討する。
- 仮説（未検証）: ノイズを独立レイヤーにすると、適用範囲、合成順、強度、シード、アニメーション、プレビュー／書き出し差分を管理しやすくなる。一方、単なる画像レイヤーとして実装すると、時間変化するシードや色空間、合成モードの責務が不明確になりやすい。
- 価値・懸念: 参照素材と本番素材の誤編集を防ぎ、ノイズを再利用可能な制作要素として扱える。専用レイヤー化は保存形式、GPU／CPU実装、レンダー順、キャッシュ無効化条件を増やすため、最初から広いノイズ種類を抱えない方が安全。
- 次の確認: ソースエリアはグループ＋非レンダー属性で足りるか、ノイズはまずGPU生成の単一方式（例: film grain）から始めるか、またノイズをレイヤー単位・コンポジション単位のどちらで適用するかを決める。
# 2026-08-20 — PBR環境光の色空間と事前フィルタリング

- 関連: `ArtifactCore/src/Graphics/MeshRenderer.cppm`、PBR環境キューブ、Material IBL
- 事実: 既存の環境キューブは equirectangular 画像から生成され、専用 irradiance cube と BRDF LUT を持っていた。specular mip は単純な面内平均だったため、GGX prefilter としては近似に留まっていた。
- 対応: 8bit LDR環境 RGB の sRGB→線形変換、mipごとの GGX importance sampling、環境単独時の IBL 経路、Clearcoat／Transmission／AO／HDR出力の整合を追加した。
- 価値/懸念: HDRI の diffuse、specular、clearcoat、transmission が同じ線形環境値を基準に評価できる。GGX生成はCPU起動時処理であり、サンプル数・環境解像度によって初期化コストが増える。Transmission は厚み・内部散乱を持たない薄い近似である。
- 次の確認: build/runtime を省略しているため、Diligent のキューブサブリソース順序、HLSL `refract` の backend 差、HDR／LDR環境の見た目、起動時コストを実機で確認する。

- 追記: `MaterialAlphaMode` を MeshRenderer の alpha-test／blend 選択へ接続し、`alphaCutoff`（既定値 0.5）を Material、GPU 定数、JSON、3Dレイヤーの数値プロパティまで通した。現時点では enum 専用 UI と glTF インポータの alphaCutoff 読み込みは未対応。
- 追記: 同一 `ArtifactIRenderer` 内の MeshRenderer 間で cubemap／irradiance／BRDF LUT を参照共有する経路を追加した。共有元と共有先は同じ `GpuContext`／device 所有範囲で使う前提で、異なる device 間の共有、LRU解放、プロセス全体キャッシュは未対応。
- 追記: equirectangular 環境画像の cubemap 初期化、GGX prefilter、irradiance convolution のサンプルを bilinear＋水平ラップへ統一した。極方向は clamp するため、極付近の立体角補正と実機画質は未検証。
- 追記: Skybox は IBL と別の描画経路だったため、環境 intensity と Y 回転を同じ定数契約で渡すようにした。背景の露出・方向は揃うが、skybox 専用の露出／トーンマップ設定はまだ分離していない。
- 追記: `ArtifactEnvironmentMapLayer::setHdriPath()` でパスを trim し、レイヤー保存値と `ArtifactIRenderer` の環境ロードキーを一致させた。空白のみの入力は空環境として扱われる。
- 追記: 共通PBR経路でも `refract` の無効方向を検出し、全反射時の transmission Fresnel を 1.0 に固定した。環境反射のフォールバックは維持し、透過寄与だけを抑制する。

# 2026-08-20 — 環境マップ共有キャッシュの次段階

- 関連: `ArtifactCore/src/Graphics/MeshRenderer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、PBR IBL リソース管理
- 事実: 現在は同一 `ArtifactIRenderer`／同一 device 内でのみ、生成済み cubemap・irradiance・prefilter・BRDF LUT を MeshRenderer 間で参照共有している。同じ環境を別 renderer が読み込む場合は、renderer ごとに生成が発生し得る。
- 仮説（未検証）: device をキーにした共有環境リソース表と、正規化パス＋画像更新世代をキーにした参照カウント付きキャッシュを導入すれば、複数コンポジション／プレビュー間の重複生成を抑えられる。ただし Diligent の device 寿命、スレッド境界、画像変更時の世代破棄、LRU上限を明示しないまま static 所有へ移行するとリークや stale SRV の原因になる。
- 価値・懸念: HDRI の高価なCPU prefilterを再利用できる一方、プロセス全体キャッシュは renderer の単純な所有モデルを変えるため、先に cache key・device lifetime・eviction 契約を固定する必要がある。
- 次の確認: `GpuContext`／device の安定した識別子、環境画像の更新世代、既存 renderer 破棄時の参照解放を確認し、まず同一 device 限定の小さな共有レジストリから設計する。

# 2026-08-20 — PBRプレビューの光学係数整合

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`、`Artifact3DModelViewer` の PBR プレビュー
- 事実: プレビューは誘電体 F0 を固定値 0.04 としており、本レンダラーが Material の specular／IOR から F0 を計算する経路と一致していなかった。
- 対応: プレビューの ColorCB に optical factors を追加し、specular と IOR を既定値付き API から渡して、本レンダラーと同じ F0 の組み立てへ寄せた。
- 価値/懸念: Material Inspector とプレビューの反射量・ガラス感の差を縮められる。プレビューは依然として簡略化した環境光であり、完全な IBL／透過の見た目一致ではない。
- 次の確認: build/runtime を省略しているため、ColorCB の Diligent constant-buffer layout と specular／IOR の実機表示を確認する。
- 追記: プレビューの transmission fallback を固定色から IOR による `refract` 方向ベースの簡略環境サンプルへ変更した。厚み・吸収・内部反射は持たないため、ガラスの物理的な透過表現ではなく、preview readability を目的とした近似である。

# 2026-08-20 — ufbxスキニング属性の共通化

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`、`ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: ufbx は `skin_deformers` の頂点ウェイトとクラスタを提供しているが、既存 importer はそれを Mesh 属性へ変換していなかった。既存 `applySkinning()` は内部 `BoneWeight` 型を探す一方、PMD importer は `QVector4D` 属性を書いていた。
- 対応: ufbx の最大4影響を `boneIndices` / `boneWeights` の `QVector4D` 属性へ正規化し、`applySkinning()` がこの共通形式を処理できるようにした。
- 価値/懸念: FBX/glTF のウェイトデータを後段のCPU LBSへ渡せる。ただしクラスタと骨ノードの名前・階層・アニメーションを Artifact のリグ契約へ保持する処理は未実装で、GPU skinning接続も未検証。
- 次の確認: Mesh の骨格メタデータ契約を定め、clusterのbind matrix／bone node階層／animation stackを保持して、glTF/FBXの実ファイルで変形結果を確認する。

# 2026-08-20 — 非PMDモデルの初期ポーズ表示接続

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`、`Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 対応: ufbx cluster の `geometry_to_world` を初期ポーズ行列として `Mesh::SkinBone` に保持し、モデル設定時に既存CPU LBSを適用してからDiligentの静的頂点バッファへアップロードする経路を追加した。
- 価値/懸念: FBX/glTFの現在ポーズを既存viewerへ渡せる。現時点は読み込み時の一回適用で、アニメーション時間変更ごとの再評価・GPU skinning・bind/rest頂点の非破壊保持は未実装。
- 次の確認: 実ファイルで座標系と `geometry_to_world` の二重変換がないことを確認し、時間サンプラーを導入する場合は元頂点から再計算するキャッシュ契約を追加する。

# 2026-08-21 — 非PMDアニメーションクリップ契約

- 関連: `ArtifactCore/include/Mesh/Mesh.ixx`、`ArtifactCore/src/Geometry/MeshImporter.cppm`
- 対応: ufbx `anim_stacks` の名前・開始時刻・終了時刻を `Mesh::SkinAnimationClip` として保持するAPIを追加した。
- 価値/懸念: FBX/glTFアセットのアニメーション候補を importer から後段へ渡せる。まだ `ufbx_evaluate_scene()` による時刻サンプリングとボーン行列更新は接続していない。
- 次の確認: クリップ選択・時刻評価の所有者をviewerか再生サービスか決め、評価済みsceneの寿命とbind頂点キャッシュを同じ契約で管理する。

# 2026-08-21 — RenderWindowのスキン姿勢更新API

- 関連: `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`、同 cppm
- 対応: 任意のボーン行列列を受け取り、元頂点からCPU LBSを再適用してGPU頂点バッファをdirty化する `setSkinPoseMatrices()` を追加した。初期ポーズも同じAPIを通す。
- 価値/懸念: タイムラインやアニメーション評価側が毎フレーム同じ描画入口を使える。行列生成側はまだ未接続で、GPU skinningではなくCPU頂点再アップロードのため大規模メッシュではコストがある。

# 2026-08-21 — Viewer経由のスキン姿勢更新

- 関連: `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`、同 cppm
- 対応: Viewerから `setSkinPoseMatrices()` を呼べる薄い委譲APIを追加した。Viewerは現在のMeshとRenderWindowの存在を確認し、更新後にステータスと再描画を要求する。
- 価値/懸念: 上位のアニメーション評価側がRenderWindow実装へ直接依存せずに姿勢を反映できる。時間サンプラー自体と、CPU再アップロードを間引く更新ポリシーは未実装。

# 2026-08-21 — ufbx時刻評価付きMeshImporter入口

- 関連: `ArtifactCore/include/Geometry/MeshImporter.ixx`、`ArtifactCore/src/Geometry/MeshImporter.cppm`
- 対応: `importMeshFromFileAtTime(path, time, clipIndex)` を追加し、FBX/glTF/GLBでは対象anim stackを選択して `ufbx_evaluate_scene()` を通した評価済みシーンからメッシュ・ウェイト・ポーズ行列を抽出できるようにした。通常のimportは従来どおり未評価シーンを使う。
- 価値/懸念: 時刻指定の非PMDスキニング評価をImporter単体で再現できる入口になった。評価済みsceneは元sceneを参照するため、解放順を維持する必要があり、毎フレーム再読込は高コスト。
- 次の確認: viewer／再生サービスからこのAPIを呼ぶ更新方式か、sceneを長寿命保持するruntime evaluator方式かを選ぶ。

# 2026-08-21 — Viewerの非PMDアニメーション再生入口

- 関連: `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`、同 cppm
- 対応: `setAnimationPlaybackEnabled()`、`setAnimationClipIndex()` を追加し、既存の表示タイマーから `loadModelAtTime()` を呼ぶ再生経路を接続した。再生は既定OFF。
- 価値/懸念: 評価済みsceneの現在姿勢をViewerへ反映できるため、非PMDのクリップ再生の最低限の動作経路ができた。現実装はフレームごとにファイルを再評価するため高コストで、長寿命ufbx scene保持またはGPU skinningへの置換が必要。
# 2026-08-20 — hostfxr反復実行セッションの境界

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`、`Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 事実: 既存の CSX 実行は毎回 `CSharpScript.EvaluateAsync` を呼び、Roslyn の `ScriptState` を保持していなかった。そのため Unity の Play Mode のような変数・実行状態の継続ができなかった。
- 対応: C++ 側に `beginScriptSession` / `stepScriptSession` / `endScriptSession` を追加し、C# 側に `ScriptState` を保持する `EvaluateSession` / `ResetSession` を追加した。既存の単発実行 API は維持している。
- 価値/懸念: 同一セッション内の反復評価が可能になる。一方、ソース変更検知、コンパイル失敗時の旧状態維持、AssemblyLoadContext の完全なアンロードは未実装であり、次段階でセッション管理と再コンパイルポリシーを追加する必要がある。
- 次の確認: hostfxr 有効環境で `begin → step → step → end` の状態継続、失敗後の状態、再初期化を実行確認する。
# 2026-08-20 — hostfxrセッション再ロードのトランザクション境界

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`、`Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 事実: Roslyn `ScriptState` を継続するだけでは、ソース変更時に新しいスクリプトを検証してから旧状態を交換する契約がなかった。
- 対応: `reloadScriptSession()` と `ReloadSession` を追加し、新しい `ScriptState` の評価成功後にだけ静的セッションを交換するようにした。失敗時は旧セッションを保持する。
- 価値/懸念: Unity の再コンパイル失敗時に直前の実行状態を維持する挙動へ近づく。AssemblyLoadContext の分離・アンロードとファイル監視はまだ別段階。
- 次の確認: 成功 reload、コンパイル失敗 reload 後の旧変数参照、次ステップ継続を実行確認する。
# 2026-08-20 — hostfxrセッションのソース更新検知

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `reloadScriptSessionFile()` と `isScriptSessionSourceChanged()` を追加し、ファイルの更新時刻を成功した再ロードと一緒に記録するようにした。
- 価値/懸念: UI／既存の更新ループから変更検知とトランザクション再ロードを呼べる。監視スレッドや新規シグナルは導入していないため、呼び出し側がポーリング周期を決める必要がある。ファイルシステムのタイムスタンプ精度に依存する。
- 次の確認: 更新時刻変更、再ロード失敗時の旧セッション維持、ファイル削除時の扱いを hostfxr 有効環境で確認する。
# 2026-08-20 — hostfxr delegate解決エラーの分離

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: hostfxr の export 解決失敗時にロード済みライブラリを後始末し、`get_function_pointer` の戻り値を保存して型名・メソッド名とともに返すようにした。
- 価値/懸念: セッション再ロード失敗と CLR delegate 解決失敗を診断しやすくする。実際の hostfxr バージョン別エラーコードと C# 例外の対応表は未整備。
- 次の確認: hostfxr 有効環境で不在メソッド、未ロードアセンブリ、delegate 解決失敗のエラー文を確認する。
# 2026-08-20 — hostfxrファイルセッションの初回ロード

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `beginScriptSessionFile()` を追加し、初回の `.csx` 読み込み時からソースパスと更新時刻をセッションへ結び付けた。
- 価値/懸念: ファイル開始 → 反復 step → 更新検知 → トランザクション再ロードの一貫した導線ができる。初期コードも `EvaluateSession` で評価するため、最初の宣言・変数を次の step から利用できる。
- 次の確認: `.csx` ファイルを使った初回ロードと再ロードの runtime 検証。
# 2026-08-20 — Roslynブートストラップの明示ビルドターゲット

- 関連: `Artifact/CMakeLists.txt`、`Artifact/scripts/dotnet/Artifact.Scripting/Artifact.Scripting.csproj`
- 事実: CMake は `dotnet` の存在を検出していたが、Roslyn ブートストラップ DLL を生成する custom target は存在せず、C++ 側の探索候補が手動ビルド前提になっていた。
- 対応: NuGet restore をネイティブビルドの副作用にしないため、明示実行用の `ArtifactScriptingDotnet` target を追加した。
- 価値/懸念: `ArtifactScriptingDotnet` を個別に実行してから hostfxr セッションを利用できる。アプリ本体への自動依存は付けていないため、配布パッケージへの DLL コピーは次段階の課題。
- 次の確認: CMake 再生成後に target が見え、Debug／Release の DLL 出力先が C++ の探索候補と一致することを確認する。
# 2026-08-20 — RoslynホストDLLの明示パス指定

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `setScriptHostAssemblyPath()` を追加し、設定された `Artifact.Scripting.dll` を最優先でロードするようにした。未設定時は既存の相対候補探索へフォールバックする。
- 価値/懸念: 起動ディレクトリやインストール配置が異なる環境でもブートストラップ DLL を解決できる。パス設定の保存・UI 統合と、配布時の既定パス決定は別途必要。
- 次の確認: 明示パスでの hostfxr ロードと、存在しない明示パスから既定候補へ戻る挙動を runtime 確認する。
# 2026-08-21 — hostfxrセッションのフレーム時間コンテキスト

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`、`Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 対応: `updateScriptSession(code, timeSeconds, deltaSeconds, frame)` を追加し、Roslyn globals の `Time`、`DeltaTime`、`Frame` を更新してから同一セッションを評価するようにした。
- 価値/懸念: アプリの更新ループから Unity の `Update` 相当の時間情報をスクリプトへ渡せる。スクリプトの実行スレッド、停止処理、例外後のフレームポリシーはまだ呼び出し側の責務である。
- 次の確認: 時間値の継続、フレーム番号、`updateScriptSession` 失敗時の状態保持を runtime 確認する。
# 2026-08-21 — hostfxrセッションの明示ライフサイクルコールバック

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `invokeScriptSessionCallback(functionName)` を追加し、識別子として検証した関数名を同一 Roslyn セッション内で呼び出せるようにした。
- 価値/懸念: `OnEnable`／`Update`／`OnDisable` 相当の規約を呼び出し側で選択できる。関数が存在しない場合の CLR 例外は返すが、コールバックの自動発火順序や停止時の保証はまだ定義していない。
- 次の確認: セッション内で定義した callback の呼び出し、無効な識別子の拒否、例外後の状態継続を runtime 確認する。
# 2026-08-21 — hostfxrセッションの状態スナップショット

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `ScriptSessionSnapshot` と `scriptSessionSnapshot()` を追加し、active 状態、ソースパス、時間、delta、フレーム番号、直近エラーを取得できるようにした。
- 価値/懸念: UI／診断面が内部 `Impl` を直接参照せずにセッション状態を表示・記録できる。スナップショットは同期なしの値コピーであり、現時点ではメインスレッドからの利用を前提とする。
- 次の確認: セッション開始・更新・再ロード・終了でスナップショットが期待どおり遷移することを runtime 確認する。
# 2026-08-21 — hostfxrセッションの直列化

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: セッション操作へ `std::recursive_mutex` を追加し、開始、step、フレーム更新、callback、reload、終了、更新検知、スナップショット取得を直列化した。ファイル開始／ファイル再ロードが内部 API を呼ぶため recursive mutex を選択した。
- 価値/懸念: UI 更新と別の監視処理が同時にセッションへ入っても、hostfxr コンテキストと Roslyn `ScriptState` の同時利用を防げる。スクリプト実行そのものはロック中に同期実行されるため、長時間処理では UI 停滞が起こり得る。
- 次の確認: 実行スレッドと状態取得スレッドを分けた場合の待ち時間、例外後のロック解放を runtime 確認する。
# 2026-08-21 — hostfxrセッションの協調停止

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `requestScriptSessionStop()` と `isScriptSessionStopRequested()` を追加し、停止要求後の step／update／callback を拒否するようにした。`endScriptSession()` と新しい session 開始で停止フラグをクリアする。
- 価値/懸念: Play／Stop の外側の状態機械から安全に次回実行を止められる。現在実行中の同期 C# 呼び出しを強制中断する機能ではなく、長時間処理の中断は別途協調キャンセル API が必要。
- 次の確認: 停止要求後の状態スナップショットと再開始時のフラグ初期化を runtime 確認する。
# 2026-08-21 — runtimeconfig.jsonの一時フォールバック

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: DLL 隣接 config、`Artifact.runtimeconfig.json` の順に探索し、両方がない場合は検出した `shared/Microsoft.NETCore.App` の最新 runtime version から一時 `runtimeconfig.json` を生成するようにした。hostfxr shutdown 時に生成ファイルを削除する。
- 価値/懸念: runtimeconfig を出力しない単純な .NET DLL でも hostfxr 初期化を試行できる。ターゲット DLL の TFM や依存 framework が runtime と一致する保証はなく、互換性エラーは hostfxr の診断へ委ねる。
- 次の確認: config なし DLL、既存 `Artifact.runtimeconfig.json`、無効な runtime version の各ケースを runtime 確認する。
# 2026-08-21 — ソース削除を変更として扱う監視契約

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `isScriptSessionSourceChanged()` は `last_write_time` の取得エラーも変更ありとして返すようにした。
- 価値/懸念: ソース削除・一時的なアクセス失敗を監視側が検出できる。再ロード自体は失敗するため、呼び出し側はエラー表示や復旧待ちを行う必要がある。旧 `ScriptState` は破棄しない。
- 次の確認: ファイル削除、再作成、短時間の置換保存を runtime 確認する。
# 2026-08-21 — ソース内容ハッシュによる再ロード検知

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: セッション開始／ファイル再ロード時にソース内容のハッシュを保存し、`isScriptSessionSourceChanged()` で内容差分も確認するようにした。ファイルが開けない場合は変更ありとして扱う。
- 価値/懸念: 同一タイムスタンプや同一サイズで置換された保存も検出しやすくなる。ポーリングごとにファイル内容を読むため、大きなスクリプトや高頻度監視では呼び出し周期を抑える必要がある。
- 次の確認: 同一サイズ・短時間保存、改行だけの変更、削除後の再作成を runtime 確認する。
# 2026-08-21 — CSharpScriptEngine診断状態の直列化

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `isInitialized()`、出力 callback 設定、`getLastError()`、`hasError()`、`clearError()` をセッション mutex で保護した。
- 価値/懸念: 更新処理と診断 UI／ログ取得が同時に走っても、エラー文字列や callback の参照競合を避けられる。単発 `executeScript()` 自体の完全な非同期実行モデルはまだ導入していない。
- 次の確認: 実行失敗と診断取得を別スレッドから行った場合の runtime 挙動を確認する。
# 2026-08-21 — hostfxr context再利用

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: 既存の host context と `load_assembly`／`get_function_pointer` delegate が有効な場合は、runtime を再初期化せず Assembly の追加ロードへ進むようにした。
- 価値/懸念: セッション開始後の追加 DLL ロードで host context を上書きしてリークするリスクを抑えられる。異なる runtimeconfig／TFM の Assembly を同一 context にロードできるかは CLR の解決結果に依存する。
- 次の確認: 同一 runtime 上での複数 Assembly ロードと、異なる TFM の拒否時エラーを runtime 確認する。
# 2026-08-21 — hostfxr load_assembly ABIの是正

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: 自己定義していた `load_assembly_fn` は coreclr delegate の呼び出し ABI と一致していなかった。
- 対応: ローカルの .NET 8／9／10 SDK 付属 `coreclr_delegates.h` と照合し、assembly path・load context・reserved の 3 引数契約へ修正した。
- 価値/懸念: hostfxr 有効環境での Assembly ロード時のスタック／レジスタ不整合リスクを下げる。実 SDK ヘッダを使った ABI 照合と runtime 検証は未実施。
- 次の確認: `coreclr_delegates.h` の対象 SDK 版と宣言を照合し、実 DLL のロードを確認する。
# 2026-08-21 — UnmanagedCallersOnly評価ABIの統一

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `get_function_pointer` で取得した `UnmanagedCallersOnly` メソッドを、hostfxr の 6 引数 component entry point として直接呼ぶ旧 `evaluate()` 経路が実メソッド ABI と一致していなかった。
- 対応: `evaluate()` を `DotnetRuntimeHost::invokeUtf8()` へ統一し、C# 側の `IntPtr argument`／`IntPtr result`／`int capacity` 契約で呼び出すようにした。
- 価値/懸念: Windows／Linux の文字幅分岐と誤った 6 引数呼び出しを除去できる。任意の C# メソッドシグネチャを呼ぶ汎用 API ではなく、UnmanagedCallersOnly の UTF-8 bridge 専用である。
- 次の確認: `EvaluateCode`／`EvaluateSession`／任意の UnmanagedCallersOnly メソッドを runtime 確認する。
# 2026-08-21 — hostfxr delegate typeとUnmanagedCallersOnly指定の是正

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: 自己定義 enum は `hdt_load_assembly=1`／`hdt_get_function_pointer=3` としていたが、SDK の enum 連番では `hdt_get_function_pointer=6`／`hdt_load_assembly=7`。また `get_function_pointer` の delegate type に `nullptr` を渡すと既定 component delegate になり、`UnmanagedCallersOnly` 指定にならない。
- 対応: .NET SDK の `hostfxr.h`／`coreclr_delegates.h` と照合し、enum 値を修正。`(const char_t*)-1` 相当の `UNMANAGEDCALLERSONLY_METHOD` を渡すようにした。
- 価値/懸念: 正しい delegate 取得と C# bridge 呼び出しに必要な ABI 契約へ近づけた。SDK 版ごとの ABI 変更がないことは runtime で確認する必要がある。
- 次の確認: `EvaluateCode` と `EvaluateSession` の function pointer 解決を runtime 確認する。
# 2026-08-21 — hostfxr／runtime version選択の数値比較

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: hostfxr directory と `shared/Microsoft.NETCore.App` の候補選択が文字列比較だったため、`10.0.x` と `9.0.x` のようなメジャーバージョン順を誤る可能性があった。
- 対応: ドット区切りの数値部分を比較する `runtimeVersionGreater()` を追加し、hostfxr と runtime config fallback の両方で使用するようにした。
- 価値/懸念: 複数メジャー／パッチ版が共存する環境でも数値上の最新を選択できる。preview suffix の厳密な SemVer precedence は未定義で、数値部分を優先する。
- 次の確認: 8／9／10 共存環境で選択された DLL と runtime version を runtime 確認する。
# 2026-08-21 — hostfxr失敗時のcontext後始末

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: delegate 取得または初回 `load_assembly` 失敗時に `hostfxr_close` を呼び、`hostContext_` と delegate pointer をクリアする `resetRuntimeContext()` を追加した。ライブラリ自体の解放は従来どおり shutdown 時に行う。
- 価値/懸念: 失敗した CLR context を次回ロードへ持ち越さず、再初期化可能な状態へ戻せる。hostfxr が部分初期化状態でも close が安全であることは SDK runtime の確認が必要。
- 次の確認: delegate 解決失敗、Assembly ロード失敗後の再初期化を runtime 確認する。
# 2026-08-21 — runtimeconfig生成先の依存解決整合

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: 自動生成する runtimeconfig をまず DLL 隣接の標準名 `<assembly>.runtimeconfig.json` とし、書き込み不可の場合だけ hash 付き temp パスへフォールバックするようにした。
- 価値/懸念: Assembly と依存ファイルの基準ディレクトリを揃えやすくなる。DLL 配置先が読み取り専用の場合は temp config となり、依存 Assembly の解決は runtime／deps 契約に依存する。
- 次の確認: 書き込み可能・読み取り専用の DLL 配置で Roslyn 依存 Assembly が解決されることを runtime 確認する。
# 2026-08-21 — セッション終了時の時刻状態リセット

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `finalize()` は `Time`／`DeltaTime`／`Frame` を初期化していたが、通常の `endScriptSession()` はソース監視だけを破棄し、時刻状態を保持していた。
- 対応: `endScriptSession()` でも時刻・デルタ・フレームをゼロへ戻し、終了後の snapshot が次回セッションの状態を誤って示さないようにした。
- 価値/懸念: セッション境界の状態が明確になる。実行中の C# 側 globals は `ResetSession` 後の次回評価で再利用されるため、runtime で再開始時の観測値を確認する必要がある。
- 次の確認: セッション終了→再開始直後の `Time`／`DeltaTime`／`Frame` の runtime 確認。
# 2026-08-21 — CSharpScriptEngine公開操作のhost状態保護

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: セッション API と診断 API は mutex を取得していたが、`initialize()`、`execute()`、`executeScript()`、`executeScriptFile()`、`evaluate()` は hostfxr state と `lastError_` を無保護で操作していた。
- 対応: 既存の再帰 mutex をこれらの公開操作にも適用した。`loadAssembly()` や file wrapper の再入は `std::recursive_mutex` で許容する。
- 価値/懸念: UI スレッドと preview／script 更新経路が同時に host state を触る場合の競合を抑えられる。C# callback が同一 engine を再入するケースは runtime で確認が必要。
- 次の確認: 並行ロード・評価と callback 再入時の deadlock／再入挙動を runtime 確認する。
# 2026-08-21 — ファイル駆動tick APIの集約

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`, `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: Unity 風のファイル反復には、呼び出し側が毎フレーム source 読込、変更検知、reload、globals 更新、評価を個別に組み立てる必要があった。
- 対応: `updateScriptSessionFile()` を追加し、active session の source path が変わった場合または内容が変わった場合に reload してから、同じ tick の code を `Time`／`DeltaTime`／`Frame` と共に評価するようにした。
- 価値/懸念: preview／editor update loop からの導入面を一つにできる。reload 成功後の tick 評価が失敗した場合は新 source metadata が先に更新されるため、旧状態へ戻すトランザクション性は今後の検討対象。
- 次の確認: 同一 path の変更、別 path への切替、reload 成功後の tick 失敗を runtime 確認する。
# 2026-08-21 — C#例外本文のC++側伝播

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: C# の各 bridge は例外文字列を result buffer に書いていたが、C++ の `invokeUtf8()` は non-zero return 時に buffer を `result` へコピーせず、数値コードだけを保存していた。
- 対応: return code 判定前に buffer を result へコピーし、例外本文がある場合は `lastError_` にも含めるようにした。
- 価値/懸念: CSX の compile／runtime error を editor 側で表示しやすくなる。固定 64 KiB buffer を超える例外本文は従来どおり切り詰められる。
- 次の確認: compile error、callback error、reload error で例外本文が UI まで届くことを runtime 確認する。
# 2026-08-21 — Frame型の符号なし契約整合

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`, `Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 事実: C++ の session frame は `std::uint64_t` だが、C# `SessionGlobals.Frame` と `SetSessionTime()` の parser は `long` だった。
- 対応: C# 側を `ulong`／`ulong.Parse` へ変更し、非負のフレーム番号契約を揃えた。
- 価値/懸念: 大きなフレーム番号を符号反転させず保持できる。C# script 側で `Frame` を `long` 引数へ渡す場合は明示 cast が必要になる。
- 次の確認: 通常範囲、`long.MaxValue` 近傍、符号付き変換エラーの runtime 確認。
# 2026-08-21 — File session読み込みエラーの遮断

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: file open 成功後の read error を確認していなかったため、部分的な source を session bootstrap／reload／tick に渡す可能性があった。
- 対応: `beginScriptSessionFile()`、`reloadScriptSessionFile()`、`updateScriptSessionFile()` で `file.bad()` を確認し、I/O error 時は評価せずエラーを返すようにした。
- 価値/懸念: 部分 source による不可逆な状態更新を避けられる。replace-save 中の一時的な空／不完全ファイルは read 成功扱いになり得るため、変更検知と reload の runtime policy は残る。
- 次の確認: read error、replace-save、file lock 中の reload で旧状態が維持されることを runtime 確認する。
# 2026-08-21 — 単発CSX読み込みエラーの整合

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: session file API では read error を遮断したが、単発の `executeScriptFile()` は部分読み込み後にそのまま評価する可能性が残っていた。
- 対応: `executeScriptFile()` にも `file.bad()` 検査を追加し、session／単発のファイル入力契約を揃えた。
- 価値/懸念: 壊れた CSX source を Roslyn へ渡す経路を減らせる。replace-save の一時的な完全ファイルは引き続き通常の compile error として扱われる。
- 次の確認: 単発 CSX の read error と replace-save 中の評価結果を runtime 確認する。
# 2026-08-21 — Source変更検知のread error扱い

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `isScriptSessionSourceChanged()` は source hash 用の iterator read 後に stream error を確認していなかった。
- 対応: binary read 後に `file.bad()` を確認し、I/O error は変更ありとして reload／旧状態保持の判断へ渡すようにした。
- 価値/懸念: 読み込み失敗を「内容が変わっていない」と誤認しない。呼び出し側が reload を試みるため、アクセス不能状態では毎 tick 変更ありになる可能性がある。
- 次の確認: source lock／一時アクセス不能時に旧 ScriptState が保持され、復旧後に reload できることを runtime 確認する。
# 2026-08-21 — Session time payloadのlocale固定

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`, `Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 事実: C++ の `ostringstream` は process locale の影響を受け得る一方、C# `SetSessionTime()` は `InvariantCulture` で time／delta を解析していた。
- 対応: `updateScriptSession()`、変更 source reload、`tickScriptSession()` の payload stream を `std::locale::classic()` に固定した。
- 価値/懸念: 小数点がカンマになる環境でも C# parser と契約が一致する。NaN／Infinity の扱いは C# の `double.Parse` と runtime 契約に依存する。
- 次の確認: comma-decimal locale 下で time／delta が正しく渡ること、特殊浮動小数値のエラー方針を runtime 確認する。
# 2026-08-21 — Session timeの有限値検証

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: C# `double.Parse` は `NaN`／`Infinity` も入力として扱えるため、非有限の time／delta が globals に入る可能性があった。
- 対応: `updateScriptSession()`、`updateScriptSessionFile()`、`tickScriptSession()` の入口で `std::isfinite()` を検証し、非有限値を host に渡さずエラーにした。
- 価値/懸念: session の時間状態を有限値に限定できる。負の time／delta は逆再生や seek の用途があるため、現時点では許可している。
- 次の確認: NaN／Infinity の拒否と、負の time／delta の意図した利用を runtime 確認する。
# 2026-08-21 — Session timeの往復精度固定

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: session payload の `ostringstream` は既定 precision 6 で、time／delta の小数桁を丸める可能性があった。
- 対応: 全 `SetSessionTime` payload に `std::numeric_limits<double>::max_digits10` の precision を設定し、locale と合わせて double の往復可能精度を確保した。
- 価値/懸念: 高精度 delta と長時間 time の tick でも C# 側の値を安定させられる。C# 側で計算した値の再量子化は別契約である。
- 次の確認: 小さい delta、長時間 time、倍精度 round-trip の runtime 確認。
# 2026-08-21 — Session time payload生成の共通化

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: update、file reload、standard tick が locale／precision 設定を個別に持っていた。
- 対応: `makeSessionTimePayload()` に classic locale、`max_digits10`、`time;delta;frame` 形式を集約し、3 経路から共通利用するようにした。
- 価値/懸念: 将来の session tick API 追加時に数値直列化契約がずれにくい。payload の delimiter escape は不要な数値専用形式として維持する。
- 次の確認: 共通 helper を通る全経路の C# round-trip を runtime 確認する。
# 2026-08-21 — C#セッションglobalsの境界リセット

- 関連: `Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 事実: C++ の `endScriptSession()` は時刻状態をリセットしていたが、C# 側の static `SessionGlobals` は `ResetSession()` 後も前セッションの値を保持していた。
- 対応: `ResetSession()` で `Time`／`DeltaTime`／`Frame` もゼロ化した。
- 価値/懸念: 次の session bootstrap が前セッションの時間情報を誤って観測しない。初回 tick まで globals はゼロ値になる。
- 次の確認: 終了→再開始→bootstrap 評価時の globals がゼロであることを runtime 確認する。
# 2026-08-21 — 標準Update tickの追加

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`, `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: 既存の `updateScriptSession()` は呼び出し側から毎フレーム評価コードを渡す汎用経路で、Unity 風の標準 `Update()` 呼び出しは別途組み立てる必要があった。
- 対応: `tickScriptSession()` を追加し、`Time`／`DeltaTime`／`Frame` を更新してからセッション内の `Update()` を評価するようにした。
- 価値/懸念: preview loop が固定ライフサイクルへ接続しやすくなる。`Update()` を定義しないスクリプトはエラーになるため、標準スクリプト契約または optional callback 方針を後続で決める必要がある。
- 次の確認: `Update()` 定義あり／なし、停止要求中、callback 例外時の runtime 挙動を確認する。
# 2026-08-21 — 変更フレームの二重評価回避

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `updateScriptSessionFile()` は source 変更時に `reloadScriptSessionFile()` を実行した後、同じ source code を通常 update として再評価していた。
- 対応: source 変更時は `SetSessionTime` を先に行い、reload 自体をその tick の評価として扱う。同一 source が継続している場合だけ `updateScriptSession()` を実行する。
- 価値/懸念: 変更フレームの副作用・ログ・状態更新の二重実行を避けられる。reload code 内で `Update()` を明示的に呼ぶ設計では、その呼び出しが一度だけ行われる。
- 次の確認: source 変更フレームの副作用が一回だけで、reload 失敗時に旧状態が維持されることを runtime 確認する。
# 2026-08-21 — セッション停止要求の再開API

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`, `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `requestScriptSessionStop()` は stop flag を立てるだけで、session を破棄せずに再開する公開経路がなかった。
- 対応: `clearScriptSessionStopRequest()` を追加し、active session の stop flag を明示的に解除できるようにした。
- 価値/懸念: editor の pause／resume 操作を session lifetime と分離できる。停止要求は実行中コードを中断せず、次の host 呼び出しを拒否する協調停止である。
- 次の確認: stop→clear→tick の再開、stop 中の reload／callback 拒否を runtime 確認する。
# 2026-08-21 — PlaybackServiceとのscript tick境界

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`, `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `ArtifactPlaybackService` の frame tick は playback engine、composition 同期、RAM／disk preview、音声状態を横断しており、C# session lifecycle を直接受け取る専用境界は現状存在しない。
- 判断: playback service に C# session を直接埋め込まず、`CSharpScriptEngine::tickScriptSession()`／`updateScriptSessionFile()` を独立した editor／preview tick API として維持する。
- 価値/懸念: 既存再生・キャッシュ経路の責務拡大を避けられる。将来統合する場合は、再生フレーム通知・script 実行順序・停止時の session policy を先に定義する必要がある。
- 次の確認: script tick を受け渡す専用 service または明示的な playback observer の設計が必要になった時点で再評価する。
# 2026-08-21 — File session sourceの単一読込

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `beginScriptSessionFile()`／`reloadScriptSessionFile()` は `ostringstream::str()` を評価、hash、metadata 設定で複数回呼び出していた。
- 対応: 1 回生成した `code` を session 評価と source hash の両方へ渡すようにした。
- 価値/懸念: 同一 file read の内容を評価と変更検知の基準に揃え、不要な一時 string 生成を減らす。ファイルが評価中に更新される race 自体は別途残る。
- 次の確認: replace-save／同時書き換え時の source metadata と reload 結果を runtime 確認する。

# 2026-08-21 — ufbxスキニング行列の契約（訂正）

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`, `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: ufbxのヘッダ実装では `geometry_to_world` がすでに `bone_node_to_world * geometry_to_bone` として更新される。別途 `pose * inverseBind` を合成すると逆バインドを二重適用する。
- 対応: `Mesh::skinPoseMatrices()` は `geometry_to_world` に相当する `poseMatrix` をそのまま返す契約へ戻した。
- 価値/懸念: FBX/glTFの初期姿勢とアニメーション時刻評価でufbxの行列契約を保てる。実ファイルごとの座標系・評価結果はruntime確認が必要。
- 次の確認: 非TポーズのFBX/glTF/GLBでbind姿勢、clip先頭、clip中間時刻の変形を確認する。

# 2026-08-21 — ufbx論理頂点と展開頂点の対応

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: ufbxのskin weightsは論理頂点単位だが、MeshImporterは面コーナー単位へ展開している。`vertex_indices` を介さずにウェイトを書き込むと、共有頂点・UV分割時に別頂点のウェイトが割り当たる。
- 対応: いったん論理頂点用のウェイト配列へ保持し、面コーナー展開時に `vertex_indices[idx]` で出力頂点へコピーするようにした。
- 価値/懸念: FBX/glTFのスキニング対象で頂点ウェイトと変形対象の対応が安定する。実ファイルの複数メッシュ・UV seamを含む受入れはruntime確認が必要。
- 次の確認: 複数メッシュ、共有頂点、UV seamを含むモデルでウェイト対応を確認する。

# 2026-08-21 — CPU LBSのゼロウェイト復元

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: 複数フレーム評価で有効なbone influenceがない頂点を何もしないままにすると、前フレームの変形位置が残る。
- 対応: 各評価で元位置・元法線を基準にし、総ウェイトがゼロなら元データへ復元、1未満/超過なら位置を総ウェイトで正規化するようにした。
- 価値/懸念: 部分的なskin deformerや壊れた入力でも、変形がフレーム間で蓄積しない。壊れた入力の診断表示は別途必要。
- 次の確認: 無ウェイト頂点と非正規化ウェイトを含むモデルでフレーム往復を確認する。

# 2026-08-21 — 3D Layer JSON復元コードの配置

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: アニメーション設定のJSON復元コードが時刻指定ロード関数内に混入し、同関数のスコープに存在しない `obj` を参照していた。
- 対応: 時刻指定ロードから除去し、`fromJsonProperties()` のモデルロード後にclip数で範囲制限して復元するようにした。
- 価値/懸念: 非PMDアニメーションのロード経路とプロジェクト復元経路の責務を分離できる。JSON復元の実動作はruntime確認が必要。
- 次の確認: 保存→再読込で animation.enabled と animation.clipIndex が維持されることを確認する。

# 2026-08-21 — ufbxウェイトの有限値検証

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: skin weight の単純な `<= 0` 判定はNaNを除外できず、合計ウェイトの正規化結果を非有限値にする可能性がある。
- 対応: influence と合計ウェイトを `std::isfinite` で検証し、有限かつ正の値だけを最大4 influenceへ採用するようにした。
- 価値/懸念: 壊れたFBX/glTF入力で頂点位置へNaNが伝播する可能性を下げる。入力ファイル単位の警告・診断表示は未実装。
- 次の確認: 不正ウェイトを含むファイルで、読み込み後のboundsと描画データが有限であることを確認する。

# 2026-08-21 — ufbxアニメーション時刻の有限値フォールバック

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: 公開時刻評価APIへNaNが渡ると、`std::clamp` 後も非有限時刻のまま評価へ伝播する可能性がある。
- 対応: 非有限時刻は選択clipの開始時刻へフォールバックしてから範囲clampするようにした。
- 価値/懸念: 外部制御や壊れたキー値から評価結果が不定になるリスクを下げる。時刻入力元の診断表示は未実装。
- 次の確認: 不正時刻入力時にclip開始姿勢が安定して返ることを確認する。

# 2026-08-21 — Viewerの非PMDアニメーション既定再生

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: Viewerにはclip再生APIがあったが、既定の再生フラグがfalseで、clipを持つFBX/glTF/GLBも静止表示になっていた。
- 対応: モデル読み込み結果にanimation clipがある場合だけ、自動再生を有効化するようにした。clipのないモデルや読み込み失敗時は無効のままにする。
- 価値/懸念: 非PMDスキニング対応がViewerの既定導線で視認できる。現状は時刻ごとに再importするため、複雑なモデルではCPU/I/O負荷のruntime確認が必要。
- 次の確認: clipありモデルの初回表示、再生停止、clip切替、読み込み失敗後の再読み込みを確認する。

# 2026-08-21 — Viewerアニメーション状態の可視化

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: Viewer statusにはbone数とclip数は表示されていたが、再生フラグの状態は表示されていなかった。
- 対応: `Animation: Playing/Stopped` をstatusへ追加した。
- 価値/懸念: 非PMDモデルが「clipを持つが停止中」なのか「再生中」なのかを、追加のイベント配線なしで確認できる。
- 次の確認: clipなしモデル、clipあり自動再生、明示停止の各status表示を確認する。

# 2026-08-21 — Packed bone indexの有限値ガード

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: packed `QVector4D` のbone indexはfloatで保持されるため、壊れた入力のNaNを整数へ変換すると未定義な変換になり得る。
- 対応: packed indexを整数化する前に有限値を確認し、非有限slotをスキップするようにした。
- 価値/懸念: 不正な非PMDウェイト属性がskin評価を壊す可能性を下げる。小数indexの診断・拒否は未実装。
- 次の確認: NaN/小数/範囲外indexを含む属性で、評価がクラッシュせず元頂点へ復元されることを確認する。

# 2026-08-21 — LBSウェイトの有限値ガード

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: `+∞` のweightは `w > 0` を通過し、総ウェイトと変形位置の正規化を壊す可能性がある。
- 対応: LBSへ加算するweightを有限かつ正の値に限定した。
- 価値/懸念: 不正なpacked／歴史的weight属性からNaN位置が生成される経路をさらに狭める。
- 次の確認: NaN/∞ weightを含む属性でboundsが有限に保たれることを確認する。

# 2026-08-21 — LBS総ウェイトのオーバーフローガード

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: 個別weightが有限でも、異常に大きい値の累積で総ウェイトが∞になる可能性がある。
- 対応: 総ウェイトが有限かつ正の場合だけ変形結果を採用し、それ以外は元頂点復元へ落とすようにした。
- 価値/懸念: 異常な属性から無限大除算やNaN位置が生成される経路を閉じる。
- 次の確認: 極端なweight合計でboundsと頂点配列が有限に保たれることを確認する。

# 2026-08-21 — Packed bone indexの整数値検証

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: packed bone indexはfloat属性で運ばれるが、1.5のような小数を整数へ暗黙変換すると別boneを誤参照する。
- 対応: 有限値に加えて `std::trunc(index) == index` を満たすslotだけをLBSへ採用するようにした。
- 価値/懸念: 壊れたpacked属性によるbone誤参照を防ぐ。入力診断をUIへ出す経路は未実装。
- 次の確認: 小数indexを含む属性で該当slotだけが無効化されることを確認する。

# 2026-08-21 — Viewerのアクティブclip名表示

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: Viewer statusはclip数を表示していたが、複数clipを持つモデルで現在の選択対象を識別できなかった。
- 対応: 選択中clipの名前を `Clips: count (name)` として表示し、名前が空の場合は `-` を表示するようにした。
- 価値/懸念: FBX/glTFのclip選択状態を追加操作なしで確認できる。clip選択UI自体は未追加。
- 次の確認: 名前付きclip、空名clip、clip index変更後のstatus表示を確認する。

# 2026-08-21 — Preview GPU skinningの大規模rig fallback

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`, `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`
- 事実: Solid viewportの `SkinningCB` は128行列固定で、129本以上のboneをGPUへ渡すと高いindexのinfluenceが無視される。
- 対応: 128本以下だけGPUスキニングを使い、超過rigは既存CPU LBSへフォールバックするようにした。
- 価値/懸念: リグサイズによる部分変形を避け、DiligentのGPU経路とCPU互換経路の境界を明示できる。大規模rigのGPU palette拡張は未実装。
- 次の確認: 128本・129本のrigで変形結果が連続し、後者が欠損しないことを確認する。

# 2026-08-21 — Diligent PreviewのGPU skinning接続

- 関連: `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: ViewerのDiligent Solid viewportは従来、MeshをCPU変形してから通常頂点だけを描画していた。既存のMesh importerが持つbone属性とpose行列はGPUへ渡されていなかった。
- 対応: 頂点にbone index/weight入力を追加し、128本のSkinningCBをDiligent dynamic uniform bufferへ更新するGPU LBS VSを接続した。128本超はCPU fallbackとした。
- 価値/懸念: FBX/glTF/GLBのViewer再生で、通常フレームはCPU頂点再書き込みを避けてGPUでスキニングできる。行列レイアウト、D3D12/Vulkan shader portability、実機表示は未検証。
- 次の確認: D3D12/Vulkan双方でbind姿勢・clip中間姿勢・wireframeが一致し、GPU/CPU fallbackの境界が正しいことを確認する。

# 2026-08-21 — GPU pose配列の完全性fallback

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: GPU経路へrigより少ない行列配列を渡すと、未提供boneがSkinningCBのidentity値を使い、部分的に誤変形する。
- 対応: pose行列数がbone数以上の場合だけGPU LBSを使い、不足時はCPU LBSへfallbackするようにした。
- 価値/懸念: 不完全な外部pose入力で静かにidentity変形される挙動を避ける。呼び出し側のpose不足を診断するUIは未実装。
- 次の確認: 完全pose、不足pose、128本超rigの3ケースで変形結果を確認する。

# 2026-08-21 — GPU pose更新時の頂点再upload抑制

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: GPU LBSへ切り替えた後もpose更新で `meshDirty_` を立てており、毎フレームbase頂点バッファを再uploadしていた。
- 対応: 完全poseかつ128本以下のGPU経路ではSkinningCBだけを更新し、`meshDirty_` はCPU fallback時だけ立てるようにした。
- 価値/懸念: GPUスキニングのCPU upload削減効果を保てる。GPU定数更新・描画同期の実機確認は未実施。
- 次の確認: 連続pose更新中に頂点VB uploadが初回以外発生せず、GPU結果だけが更新されることを確認する。

# 2026-08-21 — CPU fallback時のGPU bone属性無効化

- 関連: `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: 128本超rigをCPU LBSへfallbackしてもbone属性をGPUへ残すと、GPUが128本未満のinfluenceだけ部分再評価し、混合ウェイトを壊す可能性がある。
- 対応: `gpuSkinningActive_` を導入し、CPU fallback時はVBへゼロweightを出してGPU skinningを無効化するようにした。
- 価値/懸念: CPUで完成したposeをGPUが再変形しない。GPU/CPU切替時のVB再uploadと実機表示は未確認。
- 次の確認: 128本以下、129本以上、混合高indexウェイトの3ケースで結果が一致することを確認する。

# 2026-08-21 — GPU LBS weight overflow guard

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: CPU LBSでは有限値を検証していたが、埋め込みGPU VSは正の無限大weightを受け入れる条件だった。
- 対応: GPU VSでも極端なweightと総ウェイトを上限比較で除外し、CPU経路と同じく異常値を変形計算へ入れないようにした。
- 価値/懸念: D3D12/Vulkanで共通のHLSL比較だけを使い、NaN/∞由来の頂点破壊を抑える。実機shader compilerの受入れは未確認。
- 次の確認: GPU pathで異常weightを含む属性を描画し、頂点がNaN化しないことを確認する。

# 2026-08-21 — Viewer skinning経路の可視化

- 関連: `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: 128本以下のrigはGPU、超過rigや不完全poseはCPUへfallbackするが、Viewer statusから経路を識別できなかった。
- 対応: `gpuSkinningActive()` を追加し、statusへ `Skinning: GPU/CPU/-` を表示するようにした。
- 価値/懸念: 実機での経路確認と性能比較が容易になる。表示は経路選択を示すだけで、shader実行成功までは証明しない。
- 次の確認: GPU対応rig、CPU fallback rig、非スキンmeshのstatus表示を確認する。

# 2026-08-21 — SkinningCB dirty更新

- 関連: `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: GPU LBS導入後も、poseが変化しない描画フレームで128行列のSkinningCBを毎回mapしていた。
- 対応: mesh/pose変更時だけ `skinPoseDirty_` を立て、CB upload成功後にクリアするようにした。
- 価値/懸念: 静止フレームのCPU submissionとbuffer mapを削減する。device lossやMap失敗時の再試行はdirtyを維持する。
- 次の確認: 静止描画、pose更新、Map失敗後の再uploadを実機で確認する。

# 2026-08-21 — 停止中のアニメーションクリップ切替

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: クリップ番号だけを変更すると、再生中は次フレームで評価されるが、停止中は表示中のポーズが更新されなかった。
- 対応: クリップ変更時に選択クリップの `timeBegin` を即時評価し、Viewerでは再生停止状態を保持するようにした。
- 価値/懸念: InspectorやViewerのクリップ選択が停止中でも視覚的に反映される。再インポートによるコストは既存の時刻評価経路と同じ。
- 次の確認: 再生停止中のクリップ切替、再生中の切替、クリップなしmeshの3ケースを実機で確認する。

# 2026-08-21 — Viewerアニメーション状態表示の同期

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: クリップ切替時の再インポート処理が一時的に再生状態を有効化するため、停止中に切り替えるとstatusが一瞬または継続してPlaying表示になる可能性があった。
- 対応: 切替後に元の再生状態を復元し、statusと再描画要求を明示的に更新する。再生／停止操作自体もstatusへ反映する。
- 価値/懸念: UI表示と実際の再生状態のずれを抑える。実機でのタイマー・再インポート競合は未確認。
- 次の確認: 停止中のクリップ切替後にStopped表示と先頭ポーズが一致することを確認する。

# 2026-08-21 — Viewerアニメーション時刻の異常delta保護

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: タイマー停止復帰や時計異常で大きな負値・非有限のdeltaが入ると、クリップ時刻のfmodへ不正値が伝播し得た。
- 対応: deltaを有限値かつ0〜0.25秒へ制限し、animationTimeが非有限の場合はclip先頭へ戻す。
- 価値/懸念: 一時停止復帰時の大きなジャンプとNaN伝播を抑える。実際のタイマー再接続挙動は未確認。
- 次の確認: 長時間停止後の再開、時計差分異常、通常再生の3ケースを実機で確認する。

# 2026-08-21 — 3Dレイヤーのclipループ評価

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: Viewerはclip範囲をループしていたが、3Dレイヤーのdraw経路はコンポジション時刻をそのまま渡し、ufbx側のclamp後に終端ポーズで停止していた。
- 対応: 有効なclip範囲を使って相対時刻を `fmod` し、clip先頭から継続評価するようにした。不正な範囲や非有限時刻は評価をスキップする。
- 価値/懸念: Viewerと3Dレイヤーでアニメーション再生の継続挙動を揃えられる。実ファイルのclip境界を跨ぐ実機確認は未実施。
- 次の確認: clip開始・終端直前・終端超過の3時点でポーズが連続することを確認する。

# 2026-08-21 — ufbx clip範囲の正規化

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: 外部ファイルのanim stackが終端より前の開始時刻を持つ保証は、インポート側では明示されていなかった。
- 対応: 評価時刻のclampと公開clip metadataの双方で `min(begin,end)` / `max(begin,end)` を使い、逆順範囲を正規化した。
- 価値/懸念: 逆順metadataがViewer・3Dレイヤーの時刻評価へ伝播しない。実ファイルでの異常metadataは未確認。
- 次の確認: 正常範囲、同一時刻、逆順範囲のanim stackを読み込んで評価結果を確認する。

# 2026-08-21 — ufbx null anim stackのclip番号整合

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: metadata公開側はnull stackを除外していた一方、時刻評価側は元の配列番号を直接参照しており、null stackが混在すると選択clipがずれる可能性があった。
- 対応: 有効stackだけを数える選択処理に変更し、範囲外は最後の有効stackへ丸めた。
- 価値/懸念: Viewer/3Dレイヤーが表示するclip番号と評価対象のstackを一致させる。null stack混在ファイルは未確認。
- 次の確認: null stackを含むanim stack配列でclip選択と時刻評価を確認する。

# 2026-08-21 — 3DレイヤーのSkin Animation無効化pose

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: 再生中にSkin Animationを無効化すると、従来は最後に評価したアニメーションposeが残っていた。
- 対応: 無効化時に選択clipの `timeBegin` を再評価し、初期poseへ戻すようにした。
- 価値/懸念: 設定を無効化した状態の表示が静止した初期poseになり、Viewerの再生停止操作とは役割を分けられる。実機UI操作は未確認。
- 次の確認: 再生中の無効化、再有効化、clipなしmeshの3ケースを確認する。

# 2026-08-21 — Viewer clip選択番号の可視化

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: statusはclip名だけを表示しており、無名clipや同名clipでは選択対象を区別しにくかった。
- 対応: clip総数に加えて現在のclip index（0-based）を表示するようにした。
- 価値/懸念: UI操作なしでも選択clipの評価対象を確認できる。表示変更のみで、clip選択UI自体は追加していない。
- 次の確認: 無名・同名clipを含むモデルでstatusの番号が評価対象と一致することを確認する。

# 2026-08-21 — Mesh rig変更時のskin base cache無効化

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: `setSkinBones()` はrig paletteだけを更新し、既存のLBS基準頂点cacheを保持していた。
- 対応: ボーンpalette変更時に基準position/normal cacheを消去し、次回LBSで現在の頂点を再取得するようにした。
- 価値/懸念: Mesh再利用時に旧rig由来の基準データが混ざる可能性を抑える。setter経由の複雑なrig差し替えは未確認。
- 次の確認: 同一Meshへ異なるbone paletteを設定して再評価するケースを確認する。

# 2026-08-21 — Mesh clip setterの時刻範囲正規化

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: importer経由ではclip範囲を正規化しているが、公開setterへ直接渡されるclipには同じ保証がなかった。
- 対応: `setSkinAnimationClips()` でも有限値の逆順範囲を入れ替えるようにした。
- 価値/懸念: importer外で生成されたMeshもViewer／レイヤーの時刻評価契約を満たす。非有限値はゼロ長clipへ正規化し、再生を停止状態にする。
- 次の確認: setterへ正常・逆順・非有限clipを渡した場合の評価挙動を確認する。

# 2026-08-21 — timed importerの非有限時刻入口

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: 非有限時刻をclip先頭へfallbackする処理は存在したが、評価条件が `evaluationTime >= 0` のみでNaN/Inf時には到達しなかった。
- 対応: `-1` の予約された非評価値を除き、非有限時刻も評価経路へ通し、clip先頭へ正規化するようにした。
- 価値/懸念: 公開timed APIへ異常時刻が入ってもbind poseへ不意に戻らず、選択clipの初期poseを返す。実ファイルruntimeは未確認。
- 次の確認: 正常時刻、NaN/Inf、`-1` の3入力で結果が意図どおり分かれることを確認する。

# 2026-08-21 — Viewer clip metadata API

- 関連: `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: Viewerはclip選択setterとstatus表示を持つが、外部UIがclip数や名前を取得するAPIはなかった。
- 対応: `animationClipCount()` と `animationClipName()` を追加し、既存のclip index setterと組み合わせて利用できるようにした。
- 価値/懸念: 新規signal/slotなしで、将来の既存UIやホスト側UIからclip選択を構成できる。UI自体は追加していない。
- 次の確認: モデル未読込、範囲内、範囲外indexの戻り値を確認する。

# 2026-08-21 — 3Dレイヤー clip metadata API

- 関連: `Artifact/include/Layer/Artifact3DModelLayer.ixx`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: 3Dレイヤーはclip indexを保持・編集できたが、外部のInspector／ホストがclip数と名前を取得するAPIはなかった。
- 対応: `skinAnimationClipCount()` と `skinAnimationClipName()` を追加した。
- 価値/懸念: 既存のinteger propertyを置き換えずに、将来のclip selector実装へ接続できる。UIとruntime検証は未実施。
- 次の確認: 未読込、範囲内、範囲外indexの戻り値を確認する。

# 2026-08-21 — GPU/CPU skinning経路切替時の頂点属性再upload

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: 不完全poseのCPU fallback後に完全poseへ戻ると、GPU経路は有効化されても、直前のCPU uploadでゼロ化されたbone属性VBが再uploadされない可能性があった。
- 対応: GPU/CPU経路の状態が変わった場合に `meshDirty_` を立て、bone属性を含む頂点VBを再uploadするようにした。
- 価値/懸念: pose更新ごとの不要なVB uploadは増やさず、経路切替時だけ属性を同期する。実機での切替描画は未確認。
- 次の確認: 完全pose→不完全pose→完全poseの順でGPU/CPU表示と変形結果を確認する。

# 2026-08-21 — GPU skinning boundsの保守的フレーミング

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: GPU LBSではMeshのCPU boundsがbind poseのままなので、アニメーションposeがbind boundsを超えるとpreviewのworld framingで切れる可能性があった。
- 対応: GPU pose行列でbind boundsの8隅を変換した保守的boundsをworld framingに使うようにした。CPU fallbackは既存の変形済みMesh boundsを使用する。
- 価値/懸念: shader変形中の大きなposeでもpreview内に収まりやすい。実機でのbounds精度と過剰拡大は未確認。
- 次の確認: bind pose、四肢が広がるpose、非uniform scale poseで表示範囲を確認する。

# 2026-08-21 — GPU boundsの非有限pose保護

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: 保守的bounds計算へ異常なpose行列が入ると、NaN/Infがworld transformへ伝播する可能性があった。
- 対応: 変換cornerが有限値のときだけboundsへ取り込み、全て不正なら元のbind boundsを維持する。
- 価値/懸念: 異常poseでpreviewのカメラ行列が壊れる可能性を抑える。shader側の異常値挙動は別途runtime確認が必要。
- 次の確認: 正常poseと異常poseを混在させたbounds計算を確認する。

# 2026-08-21 — GPU boundsへのbind範囲union

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: bone変換cornerだけをbounds化すると、無ウェイト／不正ウェイトでbind位置に残る頂点を範囲から落とす可能性があった。
- 対応: GPU skinned boundsの初期値にbind boundsを含め、変換後範囲とunionするようにした。
- 価値/懸念: 部分的なスキン属性や異常ウェイトを含むmeshでも、静止頂点がpreview外へ出ない。保守的な範囲拡大は残る。
- 次の確認: 無ウェイト頂点と大きく移動するweighted頂点が混在するmeshでフレーミングを確認する。

# 2026-08-21 — Viewer再生再開時の先頭pose同期

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: 再生有効化時はanimationTimeだけをclip先頭へ戻しており、表示中の旧poseは次のtimer tickまで残っていた。
- 対応: 再生を有効化した時点でclip先頭を即時再importし、表示poseと時刻を同期するようにした。
- 価値/懸念: 再生ボタン相当のAPI操作に対する表示遅延をなくす。再importコストは既存の時刻評価経路と同じ。
- 次の確認: 停止中の途中poseから再生を有効化した直後に先頭poseが表示されることを確認する。

# 2026-08-21 — Viewer clip切替時のclock再基準化

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: 再生中のclip切替で再importに要した時間がanimation deltaへ混ざり、切替直後に不要な時間ジャンプが発生し得た。
- 対応: clip切替と先頭pose再評価の完了後にanimation clockを現在時刻へ再基準化した。
- 価値/懸念: clip切替直後の再生開始が安定する。実時間の再import負荷自体は変わらない。
- 次の確認: 再生中にclipを切り替えた直後の時刻連続性を確認する。

# 2026-08-21 — 通常モデル読み込み時のanimation clock同期

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: 時刻付き読み込みやclip切替ではclockを再基準化していたが、通常の`loadModel()`では読み込み時間が初回deltaへ混ざる可能性があった。
- 対応: 通常読み込みで初期animation stateを設定した直後にclockを現在時刻へ同期した。
- 価値/懸念: モデル読込直後の再生開始が読み込み時間に依存しない。実タイマー接続は未確認。
- 次の確認: アニメーション付きモデルの通常読み込み直後に初期poseから再生が始まることを確認する。

# 2026-08-21 — timed load完了時のanimation clock同期

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: 外部callerから`loadModelAtTime()`を直接呼ぶ場合、再import時間が次回の再生deltaへ混ざる余地が残っていた。
- 対応: timed loadの成功・失敗を問わず、状態更新前にanimation clockを現在時刻へ再基準化した。
- 価値/懸念: timer経路と外部時刻設定経路でclock初期化を統一する。再importコストは変わらない。
- 次の確認: 外部時刻設定直後の再生再開とtimer再生のdelta連続性を確認する。

# 2026-08-21 — Viewer内部animation timeのclip範囲正規化

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: timed Importerは時刻をclip範囲へclampしていたが、Viewerの`animationTime_`には呼び出し元の範囲外／非有限値が残る可能性があった。
- 対応: 読み込み後のactive clip metadataを使い、内部animation timeも有限値かつclip範囲内へ正規化した。
- 価値/懸念: 表示poseとViewer内部時刻の不一致を抑える。clip metadataが無効な場合は0へ戻す。
- 次の確認: 範囲内、範囲外、NaN/Infのtimed loadで内部時刻と表示poseが一致することを確認する。

# 2026-08-21 — Viewer clear時のanimation clock初期化

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: `clearModel()` は再生状態と時刻を初期化していたが、clock自体は古い時刻を保持していた。
- 対応: モデル削除時にもclockを現在時刻へ再基準化した。
- 価値/懸念: clear→loadの境界で古いdeltaが再利用されない。実タイマー接続は未確認。
- 次の確認: 再生中のclear→新規モデルloadで初回poseが安定することを確認する。
# 2026-08-21 — packed bone indexの整数範囲検証

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm` の `Mesh::applySkinning`
- **確認できた事実:** packed `QVector4D` のbone indexはfloatで保持され、finiteかつ整数であることだけを確認してから `int` に変換していた。
- **対応:** `int` の表現範囲外の値を変換前に除外し、破損したスキニング属性による範囲外変換を防いだ。
- **価値または懸念:** 不正なインデックスは従来どおりその頂点の有効影響から除外され、通常のFBX/glTF/PMD経路には影響しない。
- **次に確認すべきこと:** 実データでのGPU/CPU経路の表示確認は、ビルド・実行許可後に行う。
# 2026-08-21 — skin cluster単位のbone palette

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm` のufbx skin import
- **確認できた事実:** 以前はbone node単位でskin clusterを重複排除していたため、複数メッシュで同一nodeを使いながらgeometry行列が異なる場合、最初のclusterの行列を共有してしまう可能性があった。
- **対応:** paletteとweight lookupを`ufbx_skin_cluster*`単位に変更し、node単位の対応は親階層の補助情報だけに限定した。
- **価値または懸念:** 複数メッシュのgeometry-to-bone行列を保持でき、GPU上限を超えた場合も既存のCPUフォールバックへ自然に移行する。重複clusterによりbone数が増える可能性がある。
- **次に確認すべきこと:** 複数メッシュ・共有boneを含むFBX/glTFで、実行時の姿勢とboundsを確認する。
# 2026-08-21 — skin cluster lookupのelement ID fallback

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm` のufbx weight lookup
- **確認できた事実:** weightが参照するclusterはdeformerのリスト要素であり、sceneのclusterリストと同一ポインタであることを前提にしていた。
- **対応:** ポインタlookupに加えて`element_id` lookupを用意し、参照が別ポインタになっても同じclusterへ解決できるようにした。
- **価値または懸念:** 既存のcluster単位paletteの精度を維持しつつ、ufbxリスト間の参照差によるウェイト消失を防ぐ。
- **次に確認すべきこと:** 実FBX/glTFでウェイト数と表示姿勢を確認する。
# 2026-08-21 — CPU LBSの非有限変換結果保護

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm` のCPU skinning
- **確認できた事実:** bone indexとweightの検証はあったが、外部から渡された行列の変換結果がNaN/Infになる場合は頂点へ加算され得た。
- **対応:** 各influenceの変換後position/normalを検証し、非有限な影響だけをスキップするようにした。
- **価値または懸念:** 不正なpose行列によるbounds汚染と表示破綻を局所化する。GPU経路の行列検証は別途runtime確認が必要。
- **次に確認すべきこと:** 異常poseを含むCPU fallbackで、bind頂点復元とboundsが維持されることを確認する。
# 2026-08-21 — GPU pose行列の有限値検証

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** CPU LBSでは変換結果を検証したが、GPU経路はpose行列をそのままuniform bufferへ送っていた。
- **対応:** 初期poseと更新poseの16要素を検証し、非有限値を含む場合はGPU skinningを無効化してCPU fallbackへ切り替える。
- **価値または懸念:** shader内でNaNが頂点・boundsへ伝播するのを防ぐ。CPU fallback側でも不正influenceは既存の有限値検証で除外される。
- **次に確認すべきこと:** 異常pose入力時のGPU/CPU切り替えとmesh属性再uploadをruntimeで確認する。
# 2026-08-21 — Mesh boundsの非有限頂点スキップ

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm` の`Mesh::updateBounds`
- **確認できた事実:** スキニング入力や外部mesh属性に非有限positionがあると、先頭頂点を初期値に使うbounds計算全体がNaN/Infへ汚染され得た。
- **対応:** 有限なpositionだけでmin/maxを計算し、有限頂点が一つもない場合は既存boundsを保持する。
- **価値または懸念:** CPU/GPU previewのframingへ不正頂点が伝播する範囲を抑える。入力データ自体の修復は行わない。
- **次に確認すべきこと:** 異常poseと部分的に壊れたmesh属性で、既存bounds保持と再評価を確認する。
# 2026-08-21 — CPU fallbackからGPU skinningへ戻す際のbind復元

- **関連ファイル・機能:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** 129骨以上・不正poseなどでCPU fallbackを通った後、128骨以下の有効poseへ戻ると、mesh属性がCPU変形済みのままGPU shaderへ渡る可能性があった。
- **対応:** `Mesh::restoreSkinningBase()`を追加し、CPU→GPU切替時にbind-space position/normalを復元してからGPU属性を再uploadする。
- **価値または懸念:** GPU/CPU切替やpose異常からの復帰で二重変形を防ぐ。runtimeでの切替確認は未実施。
- **次に確認すべきこと:** 128骨境界と不正poseからの復帰で、GPU表示が一度だけposeを適用することを確認する。
# 2026-08-21 — CPU LBSの加算結果finite検証

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm` の`Mesh::applySkinning`
- **確認できた事実:** 各matrix変換結果がfiniteでも、極端なweightや複数influenceの加算でposition/normalの累積値がoverflowする可能性があった。
- **対応:** 累積position/normalを頂点属性へ代入する前にfinite検証し、異常時は既存のbind-space復元分岐へ送る。
- **価値または懸念:** CPU fallbackからNaN/Infがboundsやrendererへ伝播する経路をさらに抑える。
- **次に確認すべきこと:** 極端なweightを含む破損meshでbind復元が機能することをruntime確認する。
# 2026-08-21 — ufbx blend shape offsetのMesh取り込み基盤

- **関連ファイル・機能:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`, `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** ufbxはblend deformer/channel/shapeとsource vertex単位のposition/normal offsetを公開しているが、Meshには保持契約がなかった。
- **対応:** `Mesh::BlendShape`を追加し、ufbxのshape offsetをflatten後のface-corner vertexへ展開して保持する。target shapeとchannel keyframe shapeを重複統合する。
- **価値または懸念:** 既存bone skinningとは独立した入力データとして、将来のmorph適用・アニメーション評価へ接続できる。現段階ではoffsetを頂点へ適用していない。
- **次に確認すべきこと:** blend offsetをskin前に適用する順序、channel weightとkeyframe補間、複数meshの同名shape統合を実装・確認する。
# 2026-08-21 — evaluated blend shapeのImporter適用

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** ufbxのchannel weightは評価済みsceneに含まれるため、時間指定ロードごとに現在のblend状態を取得できる。
- **対応:** channel weightを`Mesh::BlendShape::weight`へ保持し、flatten済みposition/normalへoffsetをImporter段階で適用してから既存bone skinningへ渡す。
- **価値または懸念:** FBX/glTFの単純なblend shapeは、時間指定ロードとbone skinningの順序を維持したまま表示できる基盤になった。複数keyframeの厳密な補間とruntime UI制御は未実装。
- **次に確認すべきこと:** keyframe effective weightの扱い、shape weight範囲、skin→morph順序が必要なデータでの実機確認。
# 2026-08-21 — blend keyframeごとのeffective weight

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** channelのtarget shapeとkeyframe shapeは同一weightではなく、ufbxがkeyframeごとに`effective_weight`を保持している。
- **対応:** targetにはchannel weight、keyframe shapeには`effective_weight`を使用してMeshのblend shape weightへ反映する。
- **価値または懸念:** 時間指定で評価された中間morphが、全keyframeへ一律channel weightを掛けるより正確になる。同名shapeの統合は最大weight方式のまま。
- **次に確認すべきこと:** 複数keyframeの同時出現と同名shapeの合成規則をruntimeで確認する。
# 2026-08-21 — Viewer statusへのmorph数表示

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **確認できた事実:** blend shapeをMeshへ取り込んでもViewer statusではbone数とclip数しか確認できなかった。
- **対応:** 既存statusへ`Morphs`件数を追加し、読み込まれたdeformerデータの存在をinspection時に確認できるようにした。
- **価値または懸念:** 新しいsignal/slotやQt CSSを追加せず、既存statusだけで診断情報を増やした。
- **次に確認すべきこと:** 実ファイルでmorph数表示と実際の形状変化が一致することをruntime確認する。
# 2026-08-21 — blend shapeの同一pointer二重加算防止

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** ufbx channelはtarget shapeとkeyframe listの両方から同じshape pointerを返す場合があり、単純収集ではoffsetを二重加算する可能性があった。
- **対応:** channel内のshape referenceをpointer単位で重複排除してからoffsetを収集する。
- **価値または懸念:** 単純morphの形状量がtarget/keyframeの列挙形式に依存しなくなる。同名だが別pointerのshape合成規則は未確定。
- **次に確認すべきこと:** 複数channelで同一shapeを共有するデータのweight合成をruntime確認する。
# 2026-08-21 — mesh内共有blend shapeのoffset重複防止

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** 複数channelが同じufbx blend shape pointerを参照する場合、channel単位の重複排除だけではoffsetがmesh内で複数回加算され得た。
- **対応:** source mesh単位で収集済みshape pointerを記録し、offsetは一度だけ加算する。weight更新は既存の統合処理へ委ねる。
- **価値または懸念:** shared corrective shapeの位置offsetがchannel数に比例して膨らむ問題を防ぐ。複数channelのweight合成は引き続き最大値規則。
- **次に確認すべきこと:** 同一shapeを共有する複数channelの意図的な加算が必要なデータをruntime確認する。
# 2026-08-21 — morph再適用順序の公開境界

- **関連ファイル・機能:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`
- **確認できた事実:** blend shapeのbase cacheはImporterでmorphをskin前に適用するため成立するが、skin後にweightだけを変更するとbone poseとの再評価順序を別途管理する必要がある。
- **対応:** `applyBlendShapes()`は追加したが、順序管理なしの公開weight setterは追加しない。runtime編集はmorph→skinを一体で再評価できるAPI設計後に接続する。
- **価値または懸念:** morph weight変更による二重変形を未完成APIから発生させない。
- **次に確認すべきこと:** morph weight、skin pose、base cacheを一つのdeformer評価スナップショットへまとめる。
# 2026-08-21 — deformer評価順序の統一

- **関連ファイル・機能:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** 3D layerの初期pose、時間pose、手動poseが個別に`applySkinning()`を呼び、morphを再評価する順序が共通化されていなかった。
- **対応:** `Mesh::applyDeformers()`を追加し、skin base復元→blend shape適用→bone skinningの順序へ統一した。
- **価値または懸念:** 将来のruntime morph weight編集でも、morphとskinの二重変形を避ける評価入口になる。Viewer専用GPU経路は既存のpose uploadを維持する。
- **次に確認すべきこと:** morph weight変更とclip再生を同一meshで行った場合のbase cache更新をruntime確認する。
# 2026-08-21 — runtime morph weightのskin pose再適用

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** Meshは最後に適用したskin poseを保持しておらず、weight変更後に同じposeを安全に再適用する経路が不足していた。
- **対応:** `activeSkinMatrices`を保持し、`setBlendShapeWeight()`から`applyDeformers()`を呼ぶことで、base復元→morph→skinを一体で再評価する。
- **価値または懸念:** runtime weight変更時の二重変形を避けられる。GPU preview側のmorph編集UIと実ファイルでのruntime確認は未実施。
- **次に確認すべきこと:** clip再生中のweight変更、GPU/CPU切替後の再評価、morph名・weight編集UIの接続を確認する。
# 2026-08-21 — 非スキンmeshのblend shape頂点対応

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** face-corner展開時のblend shape offset転送がskin属性配列の存在条件に内包され、skinを持たないmorph meshではoffsetが全て失われていた。
- **対応:** source vertex indexの取得をskin転送から分離し、skinなしでもblend shapeのposition/normal offsetをflattened vertexへ転送する。
- **価値または懸念:** FBX/glTF/GLBの非スキンmorphとスキンmorphで同じoffset経路を使える。実ファイルでのruntime形状確認は未実施。
- **次に確認すべきこと:** 複数source mesh、UV seamによるface-corner複製、skin有無混在モデルでoffset対応を確認する。
# 2026-08-21 — blend shape weightの読取契約

- **関連ファイル・機能:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`, `Artifact/include/Layer/Artifact3DModelLayer.ixx`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** runtime weight setterは存在したが、現在値のgetterがなく、Inspectorや保存処理が編集状態を読み取れなかった。
- **対応:** Meshと3D layerにindex検証付き`blendShapeWeight()`を追加した。
- **価値または懸念:** 非PMD deformerをUI・JSONへ接続するための最小の読書き契約が揃う。実際のUI接続と永続化は未実装。
- **次に確認すべきこと:** blend shape weightをProperty Editorの責務へ接続するか、専用Morph UIとして設計する。
# 2026-08-21 — 3D deformer weightのJSON復元

- **関連ファイル・機能:** `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** blend shapeのruntime setter/getterは揃ったが、レイヤーJSONにはweightが保存されず、プロジェクト再読込でdeformer編集状態が失われていた。
- **対応:** `deformers.blendShapes`配列へ名前とweightを保存し、モデル読込後に名前一致でweightを復元する。復元後はboundsも更新する。
- **価値または懸念:** FBX/glTF/GLBのmorph編集状態をモデル再読込後も維持できる。未知のshape名は安全に無視し、複数同名shapeの編集規則は未定義。
- **次に確認すべきこと:** JSON schemaの命名統一、同名shapeの扱い、skin animation再生と保存weightの組み合わせをruntime確認する。
# 2026-08-21 — Morph weightのProperty Editor接続

- **関連ファイル・機能:** `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** 3D layerには既存の`setLayerPropertyValue()` overrideがあり、Morph専用の動的PropertyGroupを追加できる。
- **対応:** `deformers.blendShapes.<index>.weight`を動的生成し、shape名を表示ラベルに設定。編集値は既存のdeformer再評価APIへ接続した。
- **価値または懸念:** FBX/glTF/GLBのMorph weightを既存Property Editorから編集・保存できる導線が成立する。shape名変更や同名shapeの編集規則は未定義。
- **次に確認すべきこと:** property cacheの再構築タイミング、アニメーション中のweight編集、0〜1以外のDCC weight表現をruntime確認する。
# 2026-08-21 — timed skin reload時のMorph weight保持

- **関連ファイル・機能:** `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** `setAnimationTime()`は毎回importerでmeshを再生成するため、runtimeまたはProperty Editorで設定したMorph weightが新しい評価meshの初期値に戻っていた。
- **対応:** timed load前に既存shapeの名前とweightを保存し、skin pose初期化後に同名shapeへ再適用する。
- **価値または懸念:** skin animation再生中もMorph編集状態を維持できる。名前変更・同名shape・shape追加削除時の対応は名前一致の範囲に限定される。
- **次に確認すべきこと:** Property Editorのweight animationとclip切替を組み合わせたruntime確認。
# 2026-08-21 — Contents ViewerのMorph操作API

- **関連ファイル・機能:** `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`, `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** Model ViewerはMorph数をstatus表示するだけで、shape名・weightの読書きAPIがなかった。Mesh更新後にGPU preview geometryを再uploadする明示入口も不足していた。
- **対応:** ViewerへMorph count/name/weight getter/setterを追加し、RenderWindowに`refreshMeshGeometry()`を追加してrevision更新後のgeometry再uploadを要求する。
- **価値または懸念:** Contents Viewerや将来の専用Morph UIから非PMD deformerを操作できる。既存animation poseを保持したままmesh geometryだけを更新するruntime確認は未実施。
- **次に確認すべきこと:** GPU skin pose中のMorph変更、CPU fallbackからの復帰、Viewer UIからの操作導線。
# 2026-08-21 — Morph後のLBS入力cache更新

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **確認できた事実:** `applyDeformers()`はMorph適用後に`applySkinning()`を呼ぶが、LBS側の`skinBasePositions`が初回skin入力のままだと、後から変更したMorph offsetがskin計算へ入らない。
- **対応:** Morph適用直後のposition/normalを当該skin評価の入力cacheへ更新する。次回評価では`applyBlendShapes()`がblend baseから再構成するため累積変形は起きない。
- **価値または懸念:** MorphとLBSの順序が実際の評価データにも反映され、runtime weight変更・timed reload・CPU fallbackで同じ挙動になる。dual-quaternion等の別deformer順序は未対応。
- **次に確認すべきこと:** Morph weight変更後の骨pose、weightを0へ戻す操作、GPU/CPU切替のruntime確認。
# 2026-08-21 — Viewer CPU fallbackのdeformer入口統一

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** Diligent Viewerの129本以上または不正pose時のCPU fallbackだけが`applySkinning()`を直接呼び、Morph→LBSの共通順序を迂回していた。
- **対応:** 初期CPU fallbackとpose更新時のCPU fallbackを`applyDeformers()`へ変更した。
- **価値または懸念:** GPU ViewerのCPU fallbackでもMorph付き非PMDモデルのdeformer順序が3D layerと一致する。GPU shader側のblend shape直接評価は行わず、Morphはgeometry upload済みsourceへ適用する設計。
- **次に確認すべきこと:** 128本境界でのMorph保持、CPU→GPU再入場、pose不正時の復帰をruntime確認する。
# 2026-08-21 — Contents Viewer timed reload時のMorph保持

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **確認できた事実:** Viewerのanimation playbackも各時刻でmeshを再importするため、Viewer APIで設定したMorph weightだけが再評価時に初期化されていた。
- **対応:** `loadModelAtTime()`で旧meshの名前付きweightを保存し、新meshの同名shapeへ再適用してからRenderWindowへ渡す。
- **価値または懸念:** 3D layerとContents ViewerでMorph保持の挙動を統一できる。再import失敗時やshape名変更時のruntime挙動は未確認。
- **次に確認すべきこと:** Viewer再生中のweight変更とGPU geometry refreshの組み合わせをruntime確認する。
# 2026-08-21 — Morph animationと手動overrideの分離

- **関連ファイル・機能:** `Artifact/src/Layer/Artifact3DModelLayer.cppm`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **確認できた事実:** timed reload時に全Morph weightを保持すると、DCC側でアニメーションしているblend channelの評価結果まで固定してしまう。
- **対応:** layerとViewerに名前ベースの手動weight override mapを持たせ、timed reloadではoverrideだけを再適用する。新規load/clearではViewer overrideを破棄する。
- **価値または懸念:** DCCのMorph animationを維持しつつ、Property Editor／Viewerから明示的に編集したshapeだけを固定できる。override解除APIと同名shapeの規則は未実装。
- **次に確認すべきこと:** animated Morphと手動overrideの混在、JSON復元後のclip再生、override解除UXをruntime確認する。
# 2026-08-21 — Morph手動overrideの解除経路

- **関連ファイル・機能:** `Artifact/include/Layer/Artifact3DModelLayer.ixx`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`, `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **確認できた事実:** 手動Morph overrideを設定できても解除APIがなく、DCC側のMorph animationへ戻すにはモデル再読込が必要だった。
- **対応:** Layer／Viewerに`clearBlendShapeWeightOverride()`を追加。Layerは現在フレームで再評価し、Viewerは現在時刻・clipで再importしてDCC評価へ戻す。
- **価値または懸念:** 手動編集とDCCアニメーションの切替が明示的に可能になった。再評価時のファイルI/Oコストと同名shapeの扱いはruntime確認が必要。
- **次に確認すべきこと:** 複数overrideの一部解除、animation disabled時の解除、Property Editorからの解除導線。
# 2026-08-21 — JSONに評価済みMorph値を固定しない

- **関連ファイル・機能:** `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** JSONがmeshの全Morph weightを保存すると、DCC animationの現在フレーム評価値まで手動overrideとして復元され、保存後にMorph animationが停止する。
- **対応:** JSONへは`blendShapeWeightOverrides_`の手動編集値だけを`override: true`付きで保存する。既存形式のname/weight entriesは後方互換として読み込む。
- **価値または懸念:** animated Morphは保存・再読込後も評価継続し、手動編集だけを固定できる。旧JSONに含まれる評価値は手動値として解釈されるため、旧形式の完全な判別はできない。
- **次に確認すべきこと:** animated Morphを含むJSONの保存・再読込、旧JSON互換、override解除後の保存状態をruntime確認する。
# 2026-08-21 — LBS法線のnormal matrix適用

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **確認できた事実:** skinning法線がbone matrixの`mapVector()`を直接使っており、非均一scaleを含むFBX/glTF/GLBリグでは法線が正しく直交化されない可能性があった。
- **対応:** 各bone influenceの法線変換を`QMatrix4x4::normalMatrix()`（逆転置3x3）で評価し、既存のweight合成・正規化へ渡す。
- **価値または懸念:** 非PMDモデルのスケール付きskin poseでライティング法線の破綻を抑えられる。GPU Viewerのshader側法線変換は別経路のため、GPU/CPUの非均一scale parityはruntime確認が必要。
- **次に確認すべきこと:** 非均一scale骨、zero-scaleに近い行列、不正行列の有限値ガードをruntime確認する。
# 2026-08-21 — Diligent GPU法線のnormal matrix parity

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** CPU LBSはnormal matrixへ改善した一方、Viewer HLSLはbone／world matrixの3x3を直接法線へ適用していた。
- **対応:** HLSLでboneごと、およびworld transformに逆転置3x3を適用し、CPU経路と非均一scale時の法線変換前提を揃える。
- **価値または懸念:** GPU previewとCPU fallbackのライティング法線差を縮小できる。singular matrix時のGPU inverse挙動はruntime未確認。
- **次に確認すべきこと:** zero-scale近傍のpose、D3D12/Vulkan shader compiler parity、CPU fallbackとの画像比較。
# 2026-08-21 — GPU normal matrixのsingular guard

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** HLSLの`inverse()`はzero-scaleまたは特異なbone/world matrixで未定義値を生成する可能性があり、CPU側の有限値ガードと一致していなかった。
- **対応:** bone/worldの3x3行列式を検査し、閾値以下では入力法線をfallbackとして使う。通常行列では逆転置normal matrixを維持する。
- **価値または懸念:** 不正poseでGPU previewのNaN伝播を抑制できる。GPU shader compilerごとの`determinant/inverse`挙動は未検証。
- **次に確認すべきこと:** D3D12/Vulkan shader compile、zero-scale pose、CPU fallbackとの法線一致をruntime確認する。
## 2026-08-21: JSON復元Morphはoverride mapにも登録する

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` / 3DモデルMorphのJSON復元
- **気づき:** 保存対象を手動Morph overrideだけに分離しても、JSON復元時にMeshのweightだけを書き換えると、後続の時刻再評価でoverride mapにない値が失われる。復元時は名前解決後に同じoverride mapへ登録し、Meshの評価値も更新する必要がある。
- **価値・懸念:** Morphアニメーションを保持したまま、保存した手動編集値を再評価後も維持できる。重複名がある場合は既存の名前ベース仕様に従うため、名前一意性は別途確認が必要（未検証）。
- **次に確認:** 実モデルのJSON再読込後に時刻を進めても手動Morph値が維持されることを実行確認する。
## 2026-08-21: 初回GPUスキニング選択時もCPU変形を戻す

- **関連:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm` / `setMesh()`
- **気づき:** モデル読込側はソフトウェア経路でも利用できるよう初期ポーズをCPU評価している。その直後に初回GPUスキニングへ切り替えると、CPU変形済み頂点へGPUが同じポーズを再適用して二重変形になる可能性があった。
- **対応:** 有限な128本以下の初期ポーズをGPU経路へ採用する直前に`restoreSkinningBase()`を呼び、Morph適用後・スキニング前のソース形状をGPUへ渡す。
- **次に確認:** GPU初回表示とCPUフォールバック復帰で、同一ポーズが一度だけ適用されることをruntime確認する。
## 2026-08-21: GPU表示中のMorph編集でも二重スキニングを防ぐ

- **関連:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm` / `refreshMeshGeometry()`
- **気づき:** `Mesh::setBlendShapeWeight()`はCPU経路を維持するためMorph適用後に現在のskin poseも評価する。GPUスキニング中にその結果をそのまま再アップロードすると、GPUが同じskin poseを二度適用する。
- **対応:** GPUが有効なジオメトリ更新時は`restoreSkinningBase()`でMorph後・スキニング前へ戻してから頂点バッファを再構築する。
- **次に確認:** GPU表示でMorph weightを変更したとき、CPU表示と同じ形状になることをruntime確認する。
## 2026-08-21: PMD/PMXローダーとAsset Browserの形式一覧を一致させる

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`, `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`
- **確認できた事実:** `MeshImporter`にはPMDローダーがある一方、Asset Browserと共通ファイルフィルタの3D形式一覧にはPMDが含まれていなかった。旧来のPMX分岐はPMDバイナリ判定を共有しており、PMX対応を保証していなかった。
- **対応:** 3Dフィルタとモデルアイコン判定へ実装済みの`pmd`を追加し、ファイルメニューの対応アセット／3Dフィルタにも追加した。PMXは実装済みと誤認しないよう入口へ追加していない。
- **価値または懸念:** 既存のPMD系スキニングをAsset Browser経由でも選択できる。PMXは別形式のため、専用パーサーなしでは対応済みと扱えない。
- **次に確認:** PMDファイルをAsset Browserから選択し、実際の読込結果と表示アイコンをruntime確認する。
- **補足:** PMXは未対応としてProperty Editorのファイル選択候補からも除外した。
- **追加確認:** `AssetDirectoryModel`の3D判定にも`pmd`を追加し、ファイルツリー段階でPMDが除外されないようにした。
- **追加対応:** Viewerのbackend表示にも`PMD`を追加し、読込成功時に`none`と誤表示しないようにした。
## 2026-08-21: GPUスキニング法線のゼロ長フォールバック

- **関連:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm` / solid vertex shader
- **気づき:** normal matrixや入力法線が退化している場合、GPU shaderの`normalize(0)`は不定値になり、NaN法線として表示へ伝播する可能性がある。
- **対応:** bone合成後とworld変換後の法線長を検査し、閾値未満なら入力法線または固定Z法線へフォールバックする。
- **次に確認:** 退化法線・特異変換を含むモデルで、GPU表示がNaN化せずCPU経路と同様に安定することをruntime確認する。
## 2026-08-21: CPU LBS入力頂点・法線の有限値ガード

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm` / `Mesh::applySkinning()`
- **気づき:** GPU shaderには退化法線のフォールバックがあっても、CPU fallbackで入力position/normalがNaN・Infまたはゼロ長なら、無効値をそのまま復元してしまう可能性がある。
- **対応:** LBS評価前にpositionを原点、normalを+Yへフォールバックし、CPU経路でも有限値と非ゼロ法線を保証する。
- **次に確認:** 壊れた入力属性を含むFBX/glTFで、CPU fallbackがNaNを出力しないことをruntime確認する。
## 2026-08-21: Blend Shape offsetの有限値ガード

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm` / `Mesh::applyBlendShapes()`
- **気づき:** shape weightだけを検証しても、position/normal offset自体がNaN・InfならMorph後の頂点・法線が壊れ、その後のLBSへ伝播する。
- **対応:** base属性と各offsetを有限値検証し、退化法線は+Yへ戻す。normal offset適用後もゼロ長なら既存法線を保持する。
- **次に確認:** 不正なMorph属性を含むモデルでCPU/GPU経路が無効値を出力しないことをruntime確認する。
## 2026-08-21: Position offsetなしのMorphでもnormal offsetを適用

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm` / `Mesh::applyBlendShapes()`
- **気づき:** position offset配列のサイズをshape適用の入口条件にしていると、normal offsetだけを持つBlend Shapeが無視される。
- **対応:** position offsetとnormal offsetを独立して範囲検証し、どちらか一方だけ存在するshapeも適用可能にした。
- **次に確認:** normal-only shapeを含むFBX/glTFで法線変化が反映されることをruntime確認する。
## 2026-08-21: JSON復元失敗時に旧Morph overrideを破棄

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` / `fromJsonProperties()`
- **気づき:** 新しいsourcePathが存在しない場合、表示は停止しても旧モデルのMorph override mapが残り、後続保存へ誤ったdeformer状態が混入する可能性があった。
- **対応:** 欠落sourceの復元分岐でMorph override mapを明示的にクリアする。
- **次に確認:** 既存モデル表示中に存在しないsourceをJSON復元した場合、旧Morph値が再保存されないことをruntime確認する。
## 2026-08-21: 欠落source復元時に内部Meshも空にする

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` / `fromJsonProperties()`
- **気づき:** `meshLoaded_`だけをfalseにして旧Meshを保持すると、非表示状態でもboundsや動的Morph Propertyが旧モデル由来になる可能性がある。
- **対応:** sourceが存在しない復元分岐でMeshを初期化し、source sizeも空Meshから再計算する。
- **次に確認:** 欠落sourceを復元後にProperty Editorや保存処理が旧Morph情報を参照しないことをruntime確認する。
## 2026-08-21: Morph JSONのoverrideフラグを復元時に尊重

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` / Morph JSON restore
- **気づき:** 保存形式に`override`フラグがあるのに、復元側が常にweightを手動overrideとして適用していた。
- **対応:** `override:false`を明示したエントリは復元対象から除外し、未指定は従来互換でtrueとして扱う。
- **次に確認:** DCC評価値を保存した旧JSONと手動override JSONの双方で、再評価後のMorph値が意図どおりになることをruntime確認する。
## 2026-08-21: 同名MorphのJSON復元規則をtimed reloadと一致

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` / Morph JSON restore
- **気づき:** timed reloadは名前一致の全shapeへoverrideを適用する一方、JSON復元は最初の1件でループを終了していた。
- **対応:** JSON復元の`break`を除去し、同名Morph全件へ同じweightを適用する挙動に統一した。
- **次に確認:** 同名shapeを含むモデルで、初回JSON復元と時刻再評価後のMorph値が一致することをruntime確認する。
## 2026-08-21: CPU/GPU LBS法線の退化フォールバックを一致

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm` / `Mesh::applySkinning()`
- **気づき:** GPU側は合成法線のゼロ長を入力法線へ戻す一方、CPU側は無条件に`normalized()`してゼロ法線を出す可能性があった。
- **対応:** CPU側も合成法線の有限値・長さを検証し、退化時は元法線へフォールバックする。
- **次に確認:** 同一モデル・同一poseでCPU fallbackとGPU skinningの法線が一致することをruntime確認する。
## 2026-08-21: ufbxのnull mesh要素を安全にスキップ

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` / FBX・glTF mesh抽出
- **気づき:** ufbxのmesh配列にnullまたは空meshが含まれる場合、頂点数集計・skin cluster抽出前の参照でクラッシュする可能性があった。
- **対応:** 頂点数集計と実体抽出の双方でnull／空meshをスキップする。
- **次に確認:** 部分的または破損したFBX/glTFを読み込んでも、残りの有効meshだけで安全に表示できることをruntime確認する。
## 2026-08-21: ufbx抽出直後のposition/normalを有限化

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` / FBX・glTF vertex extraction
- **気づき:** ViewerのGPU経路ではMeshがCPU LBSを通らず直接頂点バッファへ入る場合があり、インポート属性のNaN・Inf・ゼロ法線がCPU側ガードを経由しない可能性がある。
- **対応:** ufbxからposition/normalを取り出した直後に有限値と法線長を検証し、positionは原点、normalは+Yへフォールバックする。
- **次に確認:** 不正頂点属性を含むFBX/glTFをGPU経路へ直接渡しても表示が破綻しないことをruntime確認する。
## 2026-08-21: 非LBS skinning方式をインポート時に明示

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` / ufbx skin deformer
- **気づき:** ufbxはDual Quaternion／Blended DQを表現できるが、現在のMesh評価はLBSのみで、非LBSモデルを無言でLBSとして扱うと品質差を見落としやすい。
- **対応:** 非LBS方式を検出したときに警告を出す。既存のLBS抽出・CPU/GPU経路は変更しない。
- **次に確認:** 非LBSモデルのログで警告が出ること、LBSモデルでは警告が出ないことをruntime確認する。
## 2026-08-21: Meshへskinning method metadataを保持

- **関連:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`, `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **気づき:** 非LBS方式を警告するだけでは、後続のDual Quaternion実装やViewer診断が入力方式を参照できない。
- **対応:** MeshにLinearBlend/Rigid/DualQuaternion/BlendedDualQuaternionの方式metadataを追加し、ufbx deformer方式をimport時に記録する。現行の評価自体は引き続きLBS。
- **次に確認:** ViewerやCPU fallbackがmetadataを利用して方式表示・DQ経路選択へ発展できることを確認する。
## 2026-08-21: skinning method metadataをViewer診断へ表示

- **関連:** `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **気づき:** Meshへ入力方式を記録しても、ViewerではGPU/CPUの実行経路しか表示されず、LBSとして評価されたDQ入力を見分けられなかった。
- **対応:** statusへ実行経路（GPU/CPU）と入力方式（LBS/Rigid/DualQuaternion/BlendedDQ）を併記する。
- **次に確認:** 各方式のモデルでstatus表示がImporter metadataと一致することをruntime確認する。
## 2026-08-21: 非LBS metadataでGPUスキニングを無効化

- **関連:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **気づき:** GPU shaderはLBS専用なのに、DQ/Blended DQ入力でも128本以下ならGPU経路を選択し、入力方式と実装方式を混同する可能性があった。
- **対応:** GPU選択条件をLinearBlend/Rigidに限定し、DQ系metadataはCPU fallbackへ送る。CPU側は現状LBS評価のため、DQ本体対応は未完了のまま明示される。
- **次に確認:** DQ入力がGPUではなくCPU経路として表示され、LBS入力だけがGPUへ入ることをruntime確認する。
## 2026-08-21: 混在deformerのskinning method優先順位を固定

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **気づき:** 1 meshに複数skin deformerがある場合、検出順によってDual QuaternionとBlended DQのmetadataが後勝ちし、方式表示・GPU fallback判定が不安定になる可能性があった。
- **対応:** Blended DQを最優先、次にDQ、Rigid、LBSの順でmetadataを保持する。
- **次に確認:** 複数deformerを含むFBX/glTFで、入力方式表示とGPU/CPU選択が順序に依存しないことをruntime確認する。
## 2026-08-21: 非LBS警告をimport単位で抑制

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **気づき:** 複数meshを持つモデルでは、同じ非LBS方式の警告がmeshごとに繰り返され、診断ログが埋もれる。
- **対応:** 1回のimport処理につき非LBS警告を1回だけ出すようにした。方式metadataの記録とLBS fallbackは維持する。
- **次に確認:** 複数meshのDQ入力で警告が過剰出力されず、方式表示は維持されることをruntime確認する。
## 2026-08-21: CPU Dual Quaternion skinningを追加

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm` / `Mesh::applySkinning()`
- **対応:** `SkinningMethod::DualQuaternion`では、bone行列をreal/dual quaternionへ変換し、符号整合した4 influenceを正規化ブレンドしてpositionとrotation-only normalを評価する。GPUは引き続きCPU fallback。
- **制限:** Blended DQは頂点ごとの`dq_weight`をMeshへ保持していないため、現状LBS fallback。scale/shearを含む行列のDQ品質と実ファイルruntimeは未検証。
- **次に確認:** DQモデルのCPU表示がLBS警告だけの状態から改善し、GPU選択されないことをruntime確認する。
## 2026-08-21: Blended Dual Quaternionの頂点ブレンド率を保持

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` / `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** ufbxの`ufbx_skin_vertex::dq_weight`を`skinDQWeight`属性へコピーし、CPU側でLBS結果とDQ結果を頂点単位で補間する。DQ=0はLBS、DQ=1はDual Quaternionとなる。
- **制限:** 複数deformerの値は最大値を採用。scale/shearを含む行列と実ファイルruntimeは未検証。
- **次に確認:** runtimeで混合率が期待どおり変化することを確認する。
## 2026-08-21: 実装対象外PMXのファイルフィルタ残骸を除去

- **関連:** `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`
- **気づき:** PMX importerを撤去した後もレイヤー追加ダイアログの3DフィルタだけがPMXを提示していた。
- **対応:** 実際に対応しているPMD、FBX、glTF等と一致するようPMXをフィルタから除去した。
- **次に確認:** 他の3Dファイル選択導線にPMX表記が残っていないことを確認する。
## 2026-08-21: flatten後のメッシュ方式を頂点属性で保持

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` / `ArtifactCore/src/Mesh/Mesh.cppm`
- **気づき:** ufbxの複数メッシュを一つのMeshへflattenする構造では、ファイル全体のskinning methodだけで評価すると、DQメッシュの方式が別メッシュへ波及し得る。
- **対応:** `skinMethod`属性へ各ソースメッシュの方式を保存し、CPU deformerは頂点属性を優先してLBS/Rigid/DQ/Blended DQを選択する。
- **制限:** 一つのソースメッシュに複数skin deformerがある場合は最も非線形な方式を採用。runtime未検証。
## 2026-08-21: Blended DQの変換失敗時にLBSへ復帰

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **気づき:** DQ行列を生成できない頂点でBlended DQが元頂点へ戻ると、LBS成分まで失われる。
- **対応:** Blended DQでは有効なLBS積分があればそれをフォールバック出力にする。純粋なDQ方式は従来どおり元頂点へ戻す。
## 2026-08-21: Dual Quaternion入力行列の有限値検証

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** DQ変換前にボーン行列16要素を検査し、NaN/Infを含む行列をQuaternion化しない。
- **次に確認:** runtimeで破損行列が出ても変形結果が有限値を維持することを確認する。
## 2026-08-21: SkinningMethod APIの未知値をLBSへ正規化

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** `setSkinningMethod()` の未知enum値をLinearBlendへ戻し、未定義の評価経路を防止する。
## 2026-08-21: DQ実装状況のInsight記述を現行状態へ補正

- **関連:** 上記のskinning metadata / CPU DQ / Blended DQ記録
- **補正:** 過去の中間記録にある「DQはLBS評価」「Blended DQは未対応」は実装途中時点の記述。現在はCPU DQ、頂点`dq_weight`によるBlended DQ、メッシュ方式属性、LBSフォールバックまで実装済み。
- **未検証:** 実ファイルruntimeとビルド確認は、ユーザー指定どおり未実施。
## 2026-08-21: SkinningMethod変更をMesh revisionへ反映

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** `setSkinningMethod()` が実際に方式を変更したときだけMesh revisionを進め、描画側の更新監視へ変更を伝播する。
## 2026-08-21: 8 influence超過の切り捨てをimport警告

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **対応:** CPU用の追加4 influence属性を追加し、最大8本まで保持する。8本を超える頂点がある場合はimportごとに一度だけ警告する。
- **制限:** GPU shaderは4本入力のため、追加influenceがあるMeshはCPU経路へ送る。超過分は最弱影響から切り捨てる。
## 2026-08-21: ufbxの追加skin influenceをCPUで最大8本保持

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`, `ArtifactCore/src/Mesh/Mesh.cppm`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **対応:** `boneIndicesExtra` / `boneWeightsExtra` を追加し、LBS・DQ・Blended DQのCPU評価を最大8 influenceへ拡張。追加属性を持つMeshは4本入力のGPU shaderを使わずCPUへ送る。
- **制限:** 8本を超える影響は最弱から切り捨てる。runtime未検証。
## 2026-08-21: DCCギャップ分析を8 influence/DQ実装へ同期

- **関連:** `docs/analysis/REPORT_DCC_GAP_3D_TEXT_2026-08-18.md`
- **対応:** 旧来の「最大4 influence・CPU LBSのみ」という記述を、最大8 influence、CPU LBS/Rigid/DQ系、GPU4本＋CPU fallbackの現行実装へ更新した。
- **未検証:** 実ファイルruntime受入れとビルド確認は未実施。
## 2026-08-21: 3D Model milestoneを現行skinning実装へ同期

- **関連:** `docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`
- **対応:** 旧来の最大4 influence / CPU LBS記述を、最大8 influence、方式別CPU評価、LinearBlend限定GPU、その他CPU fallbackの現行状態へ更新した。
## 2026-08-21: Viewer statusへskin influence幅を表示

- **関連:** `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **対応:** 既存statusに4本/8本以上のinfluence幅を追加し、追加influence時のCPU fallbackを画面上で判別できるようにした。
- **補正:** ボーンを持たない静的Meshでは influence幅を `-` と表示し、未スキニング状態を明示する。
## 2026-08-21: skinMethod属性の非整数値を拒否

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** 頂点方式属性は有限・整数・0〜3の値だけをenumへ変換し、それ以外はMesh既定方式へフォールバックする。
## 2026-08-21: Rigid skinningを最大weightの単一bone評価へ分離

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** `SkinningMethod::Rigid`では複数influenceが入力されても最大weightのboneだけを選び、通常LBSと異なる方式の意味をCPU評価へ反映する。
## 2026-08-21: Rigid方式のGPU LBS誤差を回避

- **関連:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **対応:** RigidはCPUで単一最大weight評価を行うため、GPUのLBS shaderとは意味が異なる。GPU互換方式をLinearBlendだけに限定し、RigidはCPU fallbackへ送る。
## 2026-08-21: scale/shear行列をDQ変換から除外

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** DQ化前に行列の有限性、行列式≈1、各回転軸の単位長・相互直交性を確認する。scale/shear/反転を含む行列は失敗扱いとし、Blended DQではLBSへ戻す。
- **未検証:** 実モデルのアニメーション行列が常にこの許容範囲に収まるかはruntime未確認。
## 2026-08-21: Blended DQで無効DQ影響のLBS寄与を維持

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** 各影響のLBS変換をDQ変換より先に評価し、scale/shear等でDQだけが失敗してもBlended DQのLBS側合成から影響を落とさないようにした。
## 2026-08-21: ufbx疎形式のBlended DQ重みを取り込み

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **対応:** 頂点ごとの`dq_weight`に加えて、ufbxの`dq_vertices/dq_weights`疎配列表現も`skinDQWeight`へ統合する。

## 2026-08-21: Preview Stopの非同期APIは既存だったが呼び出し側が同期経路を使用

- **関連:** `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`, `ArtifactCore/src/Audio/AudioRenderer.cppm`, `ArtifactCore/src/Audio/WASAPIBackend.cppm`
- **事実:** `AudioRenderer::requestStop()` と `WASAPIBackend::requestStop()` は既に存在し、backend threadをjoinせず停止要求だけを出す契約になっていた。一方、再生エンジンの通常Stopは同期`audioRenderer_->stop()`を呼んでいた。
- **対応:** 再生エンジンの通常Stopを`requestStop()`へ切り替え、joinは次回startまたはclose側に残した。
- **未検証:** WASAPI実機でStop応答時間、Stop→Play競合、close時のjoin完了はruntime未確認。

## 2026-08-21: File Menu recent project pathの重複を正規化

- **関連:** `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`
- **事実:** recent projectは保存時と表示時で絶対化・clean化されておらず、相対パスや表記差が重複項目になる可能性があった。
- **対応:** 追加・pruneの両方でabsolute + clean pathへ正規化し、存在確認・重複排除・保存値を同じ契約へ揃えた。
- **未検証:** 設定に残る旧相対パスを実環境で再構築した場合の表示と再オープンはruntime未確認。

## 2026-08-21: Async project load/saveのパス契約を同期経路と統一

- **関連:** `Artifact/src/Project/ArtifactProjectManager.cppm`
- **事実:** `loadFromFileAsync` と `saveToFileAsync` は入力をtrimするだけで、完了後のcurrent pathやexporterへ相対パスが渡り得た。
- **対応:** 両経路の入口でabsolute + clean pathへ正規化し、存在確認、保存先、project root、hook通知の基準を統一した。
- **未検証:** 相対パスを指定した async save/load の実動作と、project移動後の全source relinkはruntime未確認。

## 2026-08-21: Async save完了通知をcurrent path更新後へ移動

- **関連:** `Artifact/src/Project/ArtifactProjectManager.cppm`
- **事実:** 保存成功時の`onFinished`が、queuedな`currentProjectPath_`更新より先に呼ばれていた。
- **対応:** 成功コールバックを状態更新・dirty解除・after-save hookの後へ移動し、保存直後のFile Menuが新しいproject pathを観測できる順序にした。
- **未検証:** 保存直後の連続Save/Save As操作、UI thread上のcallback順序はruntime未確認。
