# Artifact.exe 非依存テストスイートと UI ビジュアル回帰計画

**最終更新:** 2026-10-16

## 進捗 2026-10-15 — ArtifactScript / Layer integration fixture

- `ArtifactScriptLayerIntegrationTest` を追加し、CMake option `ARTIFACT_ENABLE_SCRIPT_LAYER_INTEGRATION_TEST` で単独選択できるようにした。FBX test と同様に fixture file path を `ARTIFACT_SCRIPT_LAYER_TEST_FILE` で差し替えられ、既定 fixture は `tests/fixtures/artifact-script-layer/write-target.layer-script`。ArtifactAppRuntime依存のWindowsテスト用に、runtime DLL・Qt platform plugin・vcpkg runtimeの配置も登録した。
- 小さな Composition に Driver / Target の実 Solid layer を作り、Driver の `OnStart` スクリプトから `getLayer` → `getProperty` → `setProperty` を通して Target の `layer.opacity` を変更する。C++ 側で最終値を確認する。

## 進捗 2026-10-16 — ArtifactScript / Layer integration fixture runtime acceptance

- 既存 `out/build/x64-Debug` を再configureしてfixture単独suiteを登録し、`ArtifactScriptLayerIntegrationTest` をbuild。初回実行は `QGuiApplication` のGTest環境setup中にSEH access violationで終了した。画面・GUI platformを使わないsuiteなので `QCoreApplication` に変更し、再build後はCTest 1/1 passed。
- build後の `windeployqt` はvcpkg翻訳カタログ／release-bin DLL探索の警告を出したが、続くruntime DLL・Qt platform plugin・vcpkg DLL配置は完了し、suite実行に影響しなかった。

## 進捗 2026-10-11 — ArtifactExpected assignment state transitions

- 独立`ArtifactExpected` suiteへ2ケースを追加。copy assignmentでvalue/errorを切替・self-assignし、move-only value/errorでmove assignmentを切り替えた後のdestination/source stateを確認する。実行未確認。

## 進捗 2026-10-11 — ArtifactVariant storage and lifetime edges

- 独立`ArtifactVariant` suiteへ2ケースを追加し24ケースへ拡張。64-byte alignment alternativeのoriginal/copy storage alignmentと、600回のalternative replacementでTrackedValue lifetime balanceを確認する。実行未確認。

## 進捗 2026-10-11 — ArtifactTuple recursive and ownership coverage

- 独立`ArtifactTuple` suiteへ3ケースを追加。8要素の再帰access、nested move-only値のmove construct／assign、値コピー＋`reference_wrapper`を持つ生成結果を確認する。実行未確認。

## 進捗 2026-10-11 — ArtifactVariant model-based state transitions

- 独立`ArtifactVariant` suiteへ1ケースを追加し22ケースへ拡張。4 instance間の2000回のint/string代入、copy、swap、unsetを`std::variant`モデルと各stepで比較する。実行未確認。

## 進捗 2026-10-11 — ArtifactHashMap collision and model checks

- 独立`ArtifactHashMap` suiteへ2ケースを追加し23ケースへ拡張。定数hashによる同一bucket内のiteration完全性と、固定seedの3000操作を`std::unordered_map`モデルと各stepで照合する。collision iteratorは実装読みで欠落疑いを見つけた回帰case。実行未確認。

## 進捗 2026-10-11 — HashSet model-based verification

- 独立`ArtifactSet` suiteへ1ケースを追加し23ケースへ拡張。固定seedの5000操作（追加・削除・検索）ごとに`std::unordered_set`とsizeおよびキー範囲全体のmembershipを照合する。テスト実行は未実施。

## 進捗 2026-10-11 — ArtifactAlgorithms differential coverage

- 既存独立suiteへ6ケースを追加し28ケースへ拡張。長さ0〜6の全入力列を標準sortと比較し、長さ0〜5の全入力列についてlower_bound／binary_searchを標準アルゴリズムと照合する。逆順比較器、container overload、隣接削除とreverseのinvolutionも追加。実行未確認。

## 進捗 2026-10-11 — SmallVector standalone suite

- `tests/ArtifactCore/SmallVectorStandalone` に28ケースを追加。インライン容量境界、heap成長、inline／heap copy・move、挿入・削除、検索、文字列要素の寿命を確認する。debug snapshot／watch経路は未対象。ビルド・実行は未実施。

## 進捗 2026-10-11 — NameMap standalone suite

- `tests/ArtifactCore/NameMapStandalone` に23ケースを追加。CRUD、tryEmplace／tryTake、predicate削除、key/value抽出、sorted map変換、反復、メタデータを確認する。Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — MultiIndexContainer standalone suite

- `tests/ArtifactCore/MultiIndexContainerStandalone` に23ケースを追加。Qt6 CoreとGTestだけで独立し、順序／ID／型index、safe操作、移動、削除、clearを確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Container.Algorithms standalone suite

- `tests/ArtifactCore/ContainerAlgorithmsStandalone` に18ケースを追加。NamedVector／SmallVectorのeach、隣接要素を含むremoveIf、全削除・一致なし・singleton、toStdVectorコピーを確認する。Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — IdMap standalone suite

- `tests/ArtifactCore/IdMapStandalone` に独立suiteを追加。24ケースでCRUD、tryEmplace／tryTake、predicate削除、key/value抽出、標準map変換、メタデータを確認する。unordered iterationの順序には依存しない。Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — NamedList standalone suite

- `tests/ArtifactCore/NamedListStandalone` に独立suiteを追加。22ケースで両端の追加・取得・削除、重複除去、predicate削除、反復、変更反復、空状態を確認する。Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — NamedVector standalone suite

- `tests/ArtifactCore/NamedVectorStandalone` に独立suiteを追加。27ケースで基本操作、境界index、検索、順序変更、削除、変換、iterationを確認する。debug snapshot / watch経路は対象外。Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — CppLinq standard range coverage

- 既存の独立 `CppLinqStandalone` に5ケースを追加し、30ケースへ拡張。`std::list` の重複保持、`std::span` の遅延projection、`std::string_view` の文字range、`vector<bool>` proxyからのowned value取得を確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ContainerDebug standalone suite

- `tests/ArtifactCore/ContainerDebugStandalone` に GTest と標準ライブラリだけで独立する17ケースを追加。ContainerName／owner／source location、domain・severity・author名と未知値、watch rule、snapshot value helpersと時刻を検証する。Qt／Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — ContainerDebugRegistry standalone suite

- `tests/ArtifactCore/ContainerDebugRegistryStandalone` に GTest と標準ライブラリだけで独立する26ケースを追加。登録／重複／解除、再検査ごとのsnapshot reader、readerからregistryへの再照会、note writerのvalidation・exception・長さ境界、scoped registrationのmove／lifetime、container adapterを検証する。Qt／Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — DiagnosticSnapshot standalone suite

- `tests/ArtifactCore/DiagnosticSnapshotStandalone` に GTest と標準ライブラリだけで独立する26ケースを追加。sequence／timestamp順、severity集計、failure選択、event上限、merge、ErrorContext変換、JSON field／全制御文字escapingを検証する。Qt／Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — DebugIdentity standalone suite

- `tests/ArtifactCore/DebugIdentityStandalone` に Qt Core / GTest で独立する10ケースを追加。IDと生成位置、label、owner設定、registry snapshotへの登録／破棄後除去、非copy／move契約を検証する。Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — DiagnosticRecorder standalone suite

- `tests/ArtifactCore/DiagnosticRecorderStandalone` に GTest と標準ライブラリだけで独立する37ケースを追加。record／enabled、event metadata・sequence、Result／Status／ErrorContext、component/object filter、since・snapshot・missed history、bounded retention、drain／JSON、DiagnosticScopeの終了経路を検証する。Qt／Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — CrashReportParser standalone suite

- `tests/ArtifactCore/CrashReportParserStandalone` に GTest と標準ライブラリだけで独立する21ケースを追加。report header／exception／stack／system情報の解析、CRLF・最終改行なし、file APIのエラー、snapshot変換のfield mappingとfallbackを検証する。Qt／Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — SessionLedger standalone suite

- `tests/ArtifactCore/SessionLedgerStandalone` に Qt Core / GTest だけで独立する23ケースを追加。イベント記録と recoverable 抽出、recovery point、JSON／ファイル往復、schema・JSON・session ID 不正時の拒否、既定field、欠落配列での置換、entry順序、未知 event kind fallbackを検証する。Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — DiagnosticEngine standalone suite

- `tests/ArtifactCore/DiagnosticEngineStandalone` に独立 CMake executable と25ケースを追加。ルール登録・重複名・有効無効・解除、全量／差分検証・キャッシュ再利用、`DiagnosticResult` の集計／filter／merge、全診断factoryを検証する。Qt Core と診断／コンテナ基盤のみで、Artifact.exe には依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — FallbackPolicy standalone suite

- `tests/ArtifactCore/FallbackPolicyStandalone` にQt Coreだけで独立する15ケースを追加。既定policy、resolver成功／失敗、Fallback/Bypass/Strict/disabled、tracker記録・category/time filter・warning flag・clearを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — FontLicenseRegistry standalone suite

- `tests/ArtifactCore/FontLicenseRegistryStandalone` にQt Coreだけで独立する22ケースを追加。FontDescriptorのvalidity、family/style正規化、fallback、attestation、remove/clear、JSON round trip・normalized key・不正familyのskip、copy分離、entries snapshotを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ConfigLayer standalone suite

- `tests/ArtifactCore/ConfigLayerStandalone` に独立 CMake executable と8ケースを追加。列挙値、各層の永続化判定、優先順位、constexpr利用を検証する。Qt Core以外にArtifact.exeへの依存はない。ビルド・実行は未実施。

## 進捗 2026-10-11 — ColorSwatch standalone suite

- `tests/ArtifactCore/ColorSwatchStandalone` に独立 CMake executable と26ケースを追加。paletteの追加・削除・copy・検索・sortと、GPL import/exportのheader、malformed/empty row、RGB clamp、missing file、unsupported extensionを検証する。Artifact.exeやGPUには依存しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — XYZColor standalone suite

- `tests/ArtifactCore/XYZColorStandalone` に独立 CMake executable と15ケースを追加。XYZ成分・copy独立性・clamp・算術・deltaE・補間・FloatColor往復・文字列parse/formatを検証する。Qt Core以外にArtifact.exeへの依存はない。ビルド・実行は未実施。

## 進捗 2026-10-11 — FileWatcher standalone suite

- `tests/ArtifactCore/FileWatcherStandalone` に独立 CMake executable と10ケースを追加。存在しないpathの無視、watch/unwatch、全解除、初期通知なし、更新通知、callback差し替え、解除後の通知抑止、複数pathの通知分離、callback内の自己解除、削除済みpathのpollingを検証する。非同期通知ケースは上限付き待機を使う。ビルド・実行は未実施。

## 進捗 2026-10-11 — DataSource registry standalone suite

- `tests/ArtifactCore/DataSourceStandalone` に独立 CMake executable と8ケースを追加。kind名、未登録拡張子、拡張子大小文字、factory置換、path overload、登録拡張子一覧、元URI伝播を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — CsvDataSource standalone suite

- `tests/ArtifactCore/CsvDataSourceStandalone` に独立 CMake executable を追加し、CsvDataSource の読み込み、再読込、失敗時の状態、metadata、registry 経由のopenを検証する10ケースを用意した。
- ビルド・テスト・CMake configure は未実施。

## 進捗 2026-10-11 — DataCache standalone suite

- `tests/ArtifactCore/DataCacheStandalone` に独立 CMake executable と12ケースを追加。cache hit/miss、URI別保持、stale判定、invalidate/clear、同一key更新、上限 eviction、設定値、TTL期限切れとhitでTTLが延長されない挙動を検証する。ビルド・実行は未実施。

**ステータス:** In Progress

## 進捗 2026-10-11 — Quaternion standalone executable

- `Math.Quaternion` に独立CMake実行プログラムと22ケースを追加。Hamilton積、正規化／inverse、point回転、Euler／axis-angle変換、nlerp/slerp/squad境界を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Matrix4x4 standalone executable

- `Math.Matrix4x4` に独立CMake実行プログラムと12ケースを追加。row-major 配置、identity／translation／scale／rotation、乗算順序、`*=`、全要素 equality を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — RandomStream standalone executable

- `Math.Random` の `RandomStream` に独立CMake実行プログラムと17ケースを追加。固定seed再現性、unit／inclusive bounds、chance端点、fork、Gaussian、jitter、pick、shuffleを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactMinMax standalone executable

- `Math.ArtifactMinMax` に独立CMake実行プログラムと21ケースを追加。NaN／infinity、混合型、clamp、範囲正規化とepsilon境界、lerpの補間・外挿を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — FrameSkipTracker standalone executable

- `FrameSkipTracker` に Qt Core のみで独立リンクする実行プログラムと16ケースを追加。dispatch／skip記録、連続skip集計、tracking無効化、最新イベント、履歴200件上限、summaryを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — BezierPathSampler standalone executable

- `BezierPathSampler`／`BezierCalculator` に Qt Core だけで独立リンクする実行プログラムと25ケースを追加。二次／三次曲線評価、open／closed path、standard／arc-length／adaptive sampling、境界入力、接線サンプリングを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — SpatialGrid standalone executable

- `Math.SpatialGrid` に `Particle` 値型だけで独立リンクする実行プログラムと10ケースを追加。セル境界、負座標のfloor、候補index、build再実行、clearを検証する。query はセル候補を返す現行実装の契約に沿い、距離フィルタを前提にしない。ビルド・実行は未実施。

## 進捗 2026-10-11 — InterpolatorFactory standalone executable

- `InterpolatorFactory` の `LinearInterpolator`／`CubicInterpolator` に Qt Gui だけで独立リンクする実行プログラムと12ケースを追加。float／QVector3D、端点、外挿、smoothstep 対称性、仮想interfaceを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Math.Vec standalone executable

- `Math.Vec` の tagged coordinates に Qt Core／Gui と GLM だけで独立リンクする実行プログラムと15ケースを追加。DPR変換、Qt境界変換、point/vector演算、cross product、space間の暗黙変換禁止を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — DepthMap standalone executable

- 既存の `DepthMapContractTest.cpp` 27ケースを、Qt Core／Gui と GTest だけで独立実行できる CMake target に登録。resize／clamp、copy、bilinear sampling、画像load／失敗時clearを単独 suite で扱う。ビルド・実行は未実施。

## 進捗 2026-10-11 — ColumnType standalone executable

- `Data.ColumnType` に独立CMake実行プログラムと11ケースを追加。列挙値名、正規名往復、型別alias、大文字小文字、未知値のフォールバックを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — TypeInference standalone executable

- `Data.TypeInference` に独立CMake実行プログラムと22ケースを追加。空値、boolean／数値の昇格、数値形式、date／time／datetime形式、混在・不正値を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — TypedColumn standalone executable

- `Data.TypedColumn` に独立CMake実行プログラムと20ケースを追加。各推論型の保持・数値変換、bool alias、空行、日時文字列、範囲外getter fallbackを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — CsvParser standalone executable

- `Data.CsvParser` に独立CMake実行プログラムと29ケースを追加。区切り文字検出、header／header-only、quoted／multiline field、escape、CR改行、空cell／末尾delimiter、trim、列数不一致を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — DataTable standalone executable

- `DataTable` に独立CMake実行プログラムと32ケースを追加。CSV構築、headerless schema、全variant変換、row access、fallback、行／列追加、duplicate header、範囲外アクセス、clearを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — NoiseGenerator standalone executable

- `NoiseGenerator`／`NoiseField` に独立CMake実行プログラムと15ケースを追加。lattice境界、seed独立性・再現性、period、fBm、legacy overload、Worley、GLSL source生成を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ResolutionRemap standalone executable

- `ResolutionRemap` に独立CMake実行プログラムと16ケースを追加。5 policyのposition／QTransform一致、bounds、scale、impact flags、空サイズ境界を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — CameraGuide standalone executable

- `makeNukeStyleCameraGuidePrimitive` に独立CMake実行プログラムと9ケースを追加。頂点／line／triangle topology、対称性、既定・custom寸法、body-depth最小値を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — LayerBoundsCalculator standalone executable

- `LayerBoundsCalculator2D` に独立CMake実行プログラムと13ケースを追加。anchor/rotation/scale後のcorners、AABB、回転矩形と境界点のcontainsを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — LayerAlignment standalone executable

- `LayerAlignment` と LayerBounds 実装を含む独立CMake実行プログラムに14ケースを追加。selection/composition/key-object整列、分布間隔、優先度付きcollision回避、安全領域、scale fallbackを確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Float3 standalone executable

- 未テストだった `float3<T>` に独立CMake実行プログラムと9ケースを追加。構築、加算、scale、dot/length、正規化、double precision、stream出力を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — DiskSize standalone executable

- `DiskSize` に独立CMake実行プログラムと6ケースを追加。OSの空き容量に依存しない既定値とB/KB/MB/GB/TB境界、端数、uint64上限の表示を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — RotationTurns standalone executable

- 未テストだった `RotationTurns` に PImpl 実装を含めた独立CMake実行プログラムと13ケースを追加。正負のturn正規化、constructor/setter、total角、move constructor/assignment/self-moveを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioEffect base standalone executable

- `AudioEffect` 基底クラスに独立CMake実行プログラムと8ケースを追加。schedule metadata、bypass、既定parameter API、JSON、仮想dispatch／destructionを確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Audio Spatial Renderer standalone executable

- `Audio.Spatial.Renderer` と必要な Panner/SpeakerLayout 実装を含む独立CMake実行プログラムに16ケースを追加。出力layout・frame境界、speaker/headphone経路、明示LFE send、非有限入力、reset再現性を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Audio Modulation Router standalone executable

- `Audio.Modulation.Router` と Modulator moduleだけで個別に構築できる独立CMake実行プログラムと17ケースを追加。source/assignment管理、Add/Multiply、smoothing、frame seek再現性、snapshot restoreを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Audio Modulators standalone executable

- `Audio.Modulation.Modulator` に独立CMake実行プログラムと22ケースを追加。LFO波形・位相、ADSR遷移、Macro/Constant境界、seed再現性、Noise/Stepsの範囲と設定正規化をRouterを介さず検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Audio Speaker Layout standalone executable

- `speakerLayout` と `calculateSpeakerGains` に、実装 `.cppm` を含めた独立CMake実行プログラムと12ケースを追加。Mono/Stereo/5.1/7.1/7.1.4配置、LFE除外、正規化、上下半球とfallbackを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Audio Spatial Params standalone executable

- `sanitizedSpatialParams` に独立CMake実行プログラムと13ケースを追加。NaN／無限値、距離・角度・送信量の境界、enum fallbackを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Audio Spatial Math standalone executable

- 未テストだった `Audio.Spatial.Math` に独立CMake実行プログラムと14ケースを追加。距離減衰モデル、円錐ゲイン補間、逆平方根の不正値、ベクトル／クォータニオン演算、球面座標を対象とする。ビルド・実行は未実施。

## 進捗 2026-10-11 — Size_2D standalone executable

- `Size_2D`／`HasWidthHeightLike`に独立CMake実行プログラムと13ケースを新設。QSizeと6種のgeneric shape変換、concept acceptance、empty/equality/area/aspect ratioを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — FormantExtractor standalone executable

- `FormantExtractor`に独立CMake実行プログラムと12ケースを新設。phoneme分類、vowel threshold、mouth-shape mapping、empty/silent frame、track segmentationと入力境界を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioAnalyzer standalone executable

- `AudioAnalyzer`に独立CMake実行プログラムと12ケースを新設。FFT size正規化、RMS/peak、mono/stereo mix、spectrum bin数・有限性、non-finite値、sample-rate境界、zero padding、FFT truncationを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioWriter standalone executable

- `AudioWriter`に独立CMake実行プログラムと12ケースを新設。16/24-bit RIFF header・PCM payload、clamp、複数segment、sample-rate不一致、channel pad/trim、close/destructor finalize、空segmentを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — SimpleWav standalone executable

- `SimpleWav`に独立CMake実行プログラムと11ケースを新設。8/16/24-bit PCMと32-bit float読み込み、非有限/範囲外float、maxFrames、float WAV保存往復、破損/未対応入力、空saveを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioBackend API standalone executable

- `AudioBackend`の公開value/API契約に独立CMake実行プログラムと8ケースを新設。device/format validity、sample format enum、mutable callback buffer、interface抽象性を検証する。実装のないconverter呼出しは対象外。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioCache standalone executable

- `AudioCache`に独立CMake実行プログラムと11ケースを新設。frame入出力、無効値拒否、replacement、memory usage、capacity cleanup、clear、sequential prefetch、invalid range、provider例外継続を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Audio DSP AllPassFilter standalone executable

- `Audio::DSP::AllPassFilter`に独立CMake実行プログラムと9ケースを新設。impulse tap、feedback clamp、非有限値、秒/sample設定同値、modulation、clear、無効delayを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Audio DSP LFO standalone executable

- `Audio::DSP::LFO`に独立CMake実行プログラムと9ケースを新設。sinusoid cycle、phase reset、frequency change、reverse/zero/non-finite frequency、sample-rate fallback、高周波および長時間の有限性を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — FractionalDelayLine standalone executable

- `FractionalDelayLine`に独立CMake実行プログラムと10ケースを新設。遅延読出し、fractional interpolation、秒単位変換、ring wrap、clear/reinitialize、非有限・負遅延、初期化fallbackを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioVolume standalone executable

- 基本オーディオ値型`AudioVolume`に独立CMake実行プログラムと15ケースを新設。linear/dB変換、clamp、補間、preset、valid/mute/normalized、算術・比較、copy/move所有権移譲を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioDecibels standalone executable

- 基本オーディオ値型`AudioDecibels`に独立CMake実行プログラムと13ケースを新設。dB/linear変換、clamp、補間、fade、preset、predicates、operator arithmetic/comparison、copy independenceを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioRasterizer standalone executable

- `AudioRasterizer`に独立CMake実行プログラムと10ケースを新設。min/max bin、端数分割、非有限値、stereo interleaved、無効引数、末尾partial frameの扱いを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioWaveform standalone executable

- 未テストのCPU `AudioWaveform`に独立CMake実行プログラムと9ケースを新設。resolution境界、mono/stereo RMS bin、非有限値、空block、frame数より高い解像度、bypass、空channelを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioSpectrum standalone executable

- `AudioSpectrum`に独立CMake実行プログラムと13ケースを新設。bin境界、mono/stereo loudness・peak、impulse、非有限値、空block/reset、discontinuity、normalization、bypassを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioParametricEQ standalone executable

- 未テストのCPU `AudioParametricEQ`に独立CMake実行プログラムと12ケースを新設。default bands、sanitize/clamp、band count/index、parameter metadata/control、neutral/disabled signal、bypass、非有限値、sample-rate境界、JSON roundtripを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioBassTreble standalone executable

- 未テストのCPU `AudioBassTreble`に独立CMake実行プログラムと9ケースを新設。defaults/clamps、neutral constant signal、bass boost／treble cut、bypass、非有限値、sample-rate境界、空blockを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioChorus standalone executable

- 未テストのCPU `AudioChorus`に独立CMake実行プログラムと8ケースを新設。defaults/clamps、最小遅延前のdry blend、bypass、非有限値、sample-rate境界、空block、JSON roundtripを対象にする。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioReverb standalone executable

- 未テストのCPU `AudioReverb`に独立CMake実行プログラムと9ケースを新設。defaults/clamps、dry/wet、impulse tail、decay、bypass、non-finite、sample-rate境界、JSON roundtripを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioStereoMixer standalone executable

- 未テストのCPU `AudioStereoMixer`に独立CMake実行プログラムと9ケースを新設。center／端点／中間LR balance、extra channel保持、mono/bypass、非有限sample、setter境界を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioHighLowPass standalone executable

- 未テストのCPU `AudioHighLowPass`に独立CMake実行プログラムと9ケースを新設。disabled/bypass、low-pass impulseとblock状態継続、high-pass DC rejection、非有限入力、invalid sample rate、cutoff境界を検証する。high-passのDC rejectionは現sourceと異なる可能性あり。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioDelay standalone executable

- 未テストのCPU `AudioDelay`に独立CMake実行プログラムと7ケースを新設。default／clamp、1-frame impulse delay、dry mix、bypass、invalid sample rate、JSON roundtripを対象にする。ソース読解で遅延読出しindexに不整合の疑いあり。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioCompressor standalone executable

- 未テストのCPU `AudioCompressor`に独立CMake実行プログラムと10ケースを新設。parameter defaults/clamps、ratio 1、threshold動作、stereo link、sidechainとfallback、bypass、non-finite samples、JSON roundtripを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioTone standalone executable

- 未テストのCPU `AudioTone`に独立CMake実行プログラムと9ケースを新設。既定値／parameters、sine・square・sawtooth・noise、bypass位相、non-finite setter、wave type clamp、JSON roundtrip、invalid sample rateを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioPanner standalone executable

- 未テストのCPU `AudioPanner`に独立CMake実行プログラムと8ケースを新設。StereoBalance角度境界、定電力円のエネルギー、clamp、mode名、非有限値とoverflowを含むchannel別適用を検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioDownMixer standalone executable

- 未テストのCPU `AudioDownMixer`に独立CMake実行プログラムと8ケースを新設。同layout sanitation、mono→stereo、5.1→stereo係数、stereo→mono平均、explicit channel map、7.1→5.1 fold-down、empty inputを検証する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioSegment standalone executable

- `AudioSegment`の直接契約を検証する独立CMake実行プログラムと7ケースを新設。既定値、最短channelのframe count、resize／負数clamp、sample zero、metadataを保つclear、const sample pointer範囲を対象にする。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioRingBuffer standalone executable

- 未テストの`AudioRingBuffer`に独立CMake実行プログラムを新設。9ケースで容量最小値、部分read、wrap-around、容量超過時の既存データ保持、clear世代境界、capacity変更、空write／zero read、mono→stereo複製、ch数増加／縮小抑止を確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AudioBufferQueue standalone executable

- 既存テストのない`AudioBufferQueue`に独立CMake実行プログラムと6ケースを新設。空状態／pop失敗、payload・metadata保持、invalid input、FIFO、100-segment上限とpop後の再投入、clearを対象にする。ビルド・実行は未実施。

## 進捗 2026-10-11 — StringView null versus empty conversion

- 独立 `ArtifactCoreStringViewContractTest` に、QStringからStringViewを経てQStringへ変換した後もnullとemptyの区別が保たれるcaseを追加し、15から16 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — SimulationManager reset registration order

- 独立 `ArtifactCoreSimulationSystemContractTest` に、`resetAll()`が登録順に各systemをresetするcaseを追加し、7から8 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetSequence maximum-padding convenience fields

- 独立 `ArtifactCoreAssetSequenceStandaloneTest` に、18桁フレームでpath patternとdisplay nameがpadding幅を保つcaseを追加し、31から32 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — SimulationManager non-finite delta forwarding

- 独立 `ArtifactCoreSimulationSystemContractTest` に、正負の無限大とNaNのdeltaが値を変えず各simulationへ転送されるcaseを追加し、6から7 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FloatRGBA component division zero semantics

- 独立 `ArtifactCoreFloatRGBAContractTest` に、色同士のチャンネル別除算と0／負の0除算に対するIEEE NaN・符号付き無限大を確認するcaseを追加し、17から18 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — JsonLike explicit conversion rejection

- 独立 `ArtifactCoreJsonLikeContractTest` に、`QJsonValue` へのexplicit変換しか持たないproxyを`JsonReadable`／`JsonLike`が拒否するケースを追加し、10から11 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — HashValue all algorithms at file chunk boundaries

- 独立 `ArtifactCoreHashValueContractTest` に、空／8191／8192／8193／16384／16385 byteのファイルをSHA-256・SHA-1・MD5で読み、Qt digestおよび`fromData`と照合するcaseを追加し、18から19 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetFingerprint sample-window edge coverage

- 独立 `ArtifactCoreAssetFingerprintContractTest` に、large FastHash のhead／middle／tail各サンプル窓の両端6点を変更するとハッシュが変わるcaseを追加し、16から17 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactString exhaustive ascii split/join bytes

- 独立 `ArtifactCoreArtifactStringContractTest` に、custom Array/String実装の `asciiSplit`／`asciiJoin` で長さ0〜6の全byte列をcomma／NUL separatorで往復するcaseを追加し、27から28 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Path exhaustive normalization idempotence

- 独立 `ArtifactCorePathStandaloneTest` に、長さ0〜6の短い文字列全て（通常文字・dot・slash・backslash）のseparator正規化が冪等であることを確認するcaseを追加し、17から18 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactString explicit-length NUL search

- 独立 `ArtifactCoreArtifactStringContractTest` に、embedded NULを含む明示長文字列でNUL横断検索・開始offset・count／lastIndexOfを確認するcaseを追加し、26から27 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactString growing aliased replacement

- 独立 `ArtifactCoreArtifactStringContractTest` に、内部viewを置換元にして元文字列を拡張する場合の内容・長さ・終端NULを確認するcaseを追加し、25から26 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactString multibyte UTF-8 byte offsets

- 独立 `ArtifactCoreArtifactStringContractTest` に、2つの3-byte UTF-8文字の長さ・検索位置・部分抽出をbyte offset契約として確認するcaseを追加し、24から25 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — TextString exhaustive split/join byte patterns

- 独立 `ArtifactCoreTextStringContractTest` に、長さ0〜6の短いbyte列全てを2種separator（comma／NUL）でsplit/joinし、入力byte列へ戻ることを確認するcaseを追加し、16から17 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — TextString trim view aliasing

- 独立 `ArtifactCoreTextStringContractTest` に、`trimView` がtrim後範囲を元文字列内のviewとして返し、開始pointer・長さを保持するcaseを追加し、15から16 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactSpan exhaustive valid subranges

- 独立 `ArtifactCoreArtifactSpanContractTest` に、小さな固定範囲の全てのvalid offset/count組み合わせでdata pointer・size・要素列がsourceと一致するcaseを追加し、19から20 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactOptional move-only rvalue fallback

- 独立 `ArtifactCoreArtifactOptionalContractTest` に、empty optionalのrvalue `value_or` がmove-only fallbackの所有権を移すcaseを追加し、24から25 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactOptional move-only swap transitions

- 独立 `ArtifactCoreArtifactOptionalContractTest` に、move-only owning valueを保持状態／empty状態間で両方向swapするcaseを追加し、23から24 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactExpected fallback conversion laziness

- 独立 `ArtifactCoreArtifactExpectedContractTest` に、`valueOr`／`errorOr` のfallback変換が必要なalternativeの場合だけ実行される2ケースを追加し、22から24 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactExpected transform branch laziness

- 独立 `ArtifactCoreArtifactExpectedContractTest` に、`transform`／`transformError` callbackが非対応のactive alternativeでは呼ばれず、元alternativeを保持するcaseを追加し、21から22 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunctionRef clear and rebind lifecycle

- 独立 `ArtifactCoreArtifactFunctionRefContractTest` に、参照を複数回clearした後に別callableへrebindし、再度clearできるcaseを追加し、16から17 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FrameRate float JSON update clears rational metadata

- 独立 `ArtifactCoreFrameRateContractTest` に、既存exact rateへfloat JSONを適用すると通常float値へ切り替わり、rational metadataが解除されるcaseを追加し、33から34 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FrameRate invalid JSON mutation preserves exact value

- 独立 `ArtifactCoreFrameRateContractTest` に、既存exact rateへ不正な完全rational JSONをsetした際、rational metadataと値が保持されるcaseを追加し、32から33 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — SimulationManager negative delta forwarding

- 独立 `ArtifactCoreSimulationSystemContractTest` に、unpaused systemへ負のdeltaTimeも値を変えず一度転送するcaseを追加し、5から6 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — NameGenerator literal pattern global uniqueness

- 独立 `ArtifactCoreNameGeneratorContractTest` に、name placeholderを含まない固定patternでも別baseからの生成結果が一意になるcaseを追加し、15から16 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetSequence empty directory scan

- 独立 `AssetSequenceStandaloneTest` に、存在する空ディレクトリを走査した場合にsequence／singleのどちらも返さないcaseを追加し、30から31 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Defaults bitrate positivity

- 独立 `ArtifactCoreDefaultsContractTest` に、project／render bitrate既定値が正値であることを確認するcaseを追加し、6から7 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FrameRate speed adjustment clears exact metadata

- 独立 `ArtifactCoreFrameRateContractTest` に、exact rational rateへのspeed adjustment後にfloat rateへ移行し、rational metadataが標準値へ戻るcaseを追加し、31から32 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — MultipleTag set-operation idempotence

- 独立 `ArtifactCoreMultipleTagContractTest` に、同一集合を両オペランドにしたunion／intersectionの冪等性とself-subtractionの空集合性を追加し、21から22 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Id copy semantics for moved-from value

- 独立 `ArtifactCoreIdContractTest` に、ムーブ元の `Id` をcopy構築／copy代入した結果が利用可能なNil UUIDとなり、移動済み値を保持するcaseを追加し、21から22 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — MultipleTag move assignment into moved-from destination

- 独立 `ArtifactCoreMultipleTagContractTest` に、ムーブ元になった空の `MultipleTag` をムーブ代入先として再利用でき、元の移動先オブジェクトも保持されることを確認するcaseを追加し、20から21 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Tag move assignment from moved-from source

- 独立 `ArtifactCoreTagContractTest` に、ムーブ元になった空の `Tag` を別の `Tag` へムーブ代入した場合の空状態と、先に移動済みの値が保たれることを確認するcaseを追加し、17から18 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunction move-only heap callable

- 独立 `ArtifactCoreArtifactFunctionContractTest` に、大きなmove-only captureをheap storageへ置き、呼び出して結果を得られるcaseを追加し、20から21 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunction move-only inline callable

- 独立 `ArtifactCoreArtifactFunctionContractTest` に、move-only `unique_ptr` captureをinline storageへ格納して呼び出せるcaseを追加し、19から20 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — TextNumber padded integer extremes

- 独立 `ArtifactCoreTextNumberContractTest` に、長い先頭ゼロ付きのsigned／unsigned最大値・最小値が同じ極値へ解析されるcaseを追加し、19から20 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Result ifValue observation semantics

- 独立 `ArtifactCoreResultContractTest` に、`ifValue` callbackが成功時だけ実行され、同じResultへのconst参照を返す契約を追加し、33から34 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactThread restart without join

- 独立 `ArtifactCoreArtifactThreadContractTest` に、worker終了後に明示joinせず次のstartを行い、前workerの回収と再実行を確認するcaseを追加し、16から17 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — CppLinq forward-only range coverage

- 独立 `ArtifactCoreCppLinqContractTest` に、`std::forward_list` でのAny／FirstOrDefaultと遅延Where+Select pipelineを確認するcaseを追加し、24から25 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunction reference and exception behavior

- 独立 `ArtifactCoreArtifactFunctionContractTest` に、reference returnのalias保持とcallable例外の伝播を確認する2ケースを追加し、17から19 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — DepthMap infinite sample clamping

- 独立 `ArtifactCoreDepthMapContractTest` に、resize初期値とsetValueへ正負の無限大を与えたとき深度値が0／1にクランプされるcaseを追加し、26から27 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FileTypeDetector extension precedence

- 独立 `ArtifactCoreFileTypeDetectorContractTest` に、PNGの強いmagic signatureでもModel3D／Audio／Archive／Videoの意味を持つ拡張子が優先されるcaseを追加し、18から19 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile invalid imported timestamp

- 独立 `ArtifactCoreAssetMetaFileContractTest` に、無効な `QDateTime` を保存・再読込しても無効状態を保つcaseを追加し、18から19 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile nested custom values

- 独立 `ArtifactCoreAssetMetaFileContractTest` に、nested `QVariantMap`／`QStringList`をcustom valueとしてsidecarへ保存し、再読込後の型と値を確認するcaseを追加し、17から18 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — SimulationManager update ordering

- 独立 `ArtifactCoreSimulationSystemContractTest` に、複数systemが登録順にupdateされることを確認するcaseを追加し、4から5 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile blocked destination save

- 独立 `ArtifactCoreAssetMetaFileContractTest` に、sidecar保存先がディレクトリで塞がれている場合にsaveが失敗し、既存ディレクトリが残ることを確認するcaseを追加し、16から17 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Keyframe nonuniform Hermite curve

- 独立 `ArtifactCoreKeyframeContractTest` に、時間間隔と値差が非線形な3キーHermite曲線で区間長に応じた有限差分接線の期待値を確認するcaseを追加し、15から16 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetFingerprint large streamed SHA-256

- 独立 `ArtifactCoreAssetFingerprintContractTest` に、5 MiB超のファイルSHA-256をQtの固定入力digestと比較するcaseを追加し、15から16 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactCallback copy assignment

- 独立 `ArtifactCoreArtifactCallbackContractTest` に、copy代入による旧capture解放／新capture所有と、empty callback代入によるtarget clearを追加し、18から20 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — JsonLike combined constraint round trip

- 独立 `ArtifactCoreJsonLikeContractTest` に、`JsonLike` 制約で値を書き込み同じ制約内で読み戻すcaseを追加し、9から10 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactSpan const-view and copy semantics

- 独立 `ArtifactCoreArtifactSpanContractTest` に、constなspan objectでも可変要素を編集できる性質と、copy／assignment後も同一の借用範囲を参照する性質を追加し、17から19 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunction swap storage coverage

- 独立 `ArtifactCoreArtifactFunctionContractTest` に、inline格納／heap格納 callable 間のswapと空functionとのswapを追加し、15から17 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactHashMap value destruction

- 独立 `ArtifactCoreArtifactHashMapContractTest` に、eraseとclearがmapped valueを一度だけ破棄し、繰り返し操作でも二重破棄しないことを確認するcaseを追加し、20から21 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactHashMap move operations

- 独立 `ArtifactCoreArtifactHashMapContractTest` に、ムーブ構築後の要素移管と、ムーブ代入での移動先置換・移動元要素移管を確認する2ケースを追加し、18から20 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetFingerprint sampled size contract

- 独立 `ArtifactCoreAssetFingerprintContractTest` に、サンプル領域が同じゼロ埋めファイル同士でもファイルサイズを含めて高速ハッシュが異なることを確認するケースを追加し、14から15 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FastSettingsStore sync retry after path repair

- 独立 `ArtifactCoreFastSettingsStoreContractTest` に、sync失敗後に親パスをdirectoryへ修復すると保持中のdirty値をsync・再読込できるcaseを追加し、23から24 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FastSettingsStore repeated key updates

- 独立 `ArtifactCoreFastSettingsStoreContractTest` に、同じkeyへの2回のsetを2 operationsとして数え、sync／再読込後に最後の値を保持するcaseを追加し、22から23 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FileTypeDetector empty-file extension fallback

- 独立 `ArtifactCoreFileTypeDetectorContractTest` に、既存の空`.png`はmagic unknownでもextension fallbackでImageとなるcaseを追加し、17から18 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FastSettingsStore nested QVariant round trip

- 独立 `ArtifactCoreFastSettingsStoreContractTest` に、nested `QVariantMap`内のboolとUnicode `QStringList`をCBORへsyncして再読込するcaseを追加し、21から22 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FastSettingsStore empty file open

- 独立 `ArtifactCoreFastSettingsStoreContractTest` に、0-byte CBOR fileをclean empty storeとして開けるcaseを追加し、20から21 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — RationalTime common-scale overflow subtraction

- 独立 `ArtifactCoreRationalTimeContractTest` に、LCMがint64範囲を超えるscale同士の減算でseconds fallback相当の結果を返すcaseを追加し、26から27 casesへ拡張。現実装のLCM乗算はoverflow検査されていない。ビルド・実行は未実施。

## 進捗 2026-10-11 — RationalTime common-scale overflow fallback

- 独立 `ArtifactCoreRationalTimeContractTest` に、互いに異なる大きなscaleのLCMがint64範囲を超える加算でseconds fallback相当の結果を返すcaseを追加し、25から26 casesへ拡張。現実装はLCM乗算をoverflow検査せず、誤結果／未定義動作の可能性がある。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactExpected copy and move state

- 独立 `ArtifactCoreArtifactExpectedContractTest` に、value/error両alternativeのcopy後moveでactive stateとpayloadを保持するcaseを追加し、20から21 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetSequence very large frame gaps

- 独立 `AssetSequenceStandaloneTest.cpp` に、18桁frame番号同士の巨大gapを連番化せずsinglesへ分割するcaseを追加し、29から30 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetSequence Unicode prefix

- 独立 `AssetSequenceStandaloneTest.cpp` に、Unicode prefixを含むファイル名から連番を検出し、prefixとsort済みfilenameを保持するcaseを追加し、28から29 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetSequence standalone uppercase extension

- 独立 `AssetSequenceStandaloneTest.cpp` に、ASCII大文字拡張子`.EXR`をsequenceとして認識し、original suffixをpath patternに保持するcaseを追加し、27から28 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — HashValue unknown algorithm data and file factories

- 独立 `ArtifactCoreHashValueContractTest` に、`fromFile`でも`Unknown`をMD5 fallbackせず空のinvalid結果にするcaseを追加し、17から18 casesへ拡張。現実装はMD5 digest bytesを格納するため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — HashValue unknown algorithm input

- 独立 `ArtifactCoreHashValueContractTest` に、`fromData`へ`Unknown`を渡した場合は無効で空のdigestを返すcaseを追加し、16から17 casesへ拡張。現実装はfallbackのMD5 bytesを格納するため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — Tag empty-name clearing

- 独立 `ArtifactCoreTagContractTest` に、空の`UniString`を設定して既存Tag名をclearできるcaseを追加し、16から17 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — DepthMap infinite sampling coordinates

- 独立 `ArtifactCoreDepthMapContractTest` に、正負の無限大座標を画像端へclampして正しいedge sampleを返すcaseを追加し、25から26 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FloatRGBA copy and move construction

- 独立 `ArtifactCoreFloatRGBAContractTest` に、copy／move construction後も4 channelが保持されるcaseを追加し、16から17 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactAlgorithms empty predicate ranges

- 独立 `ArtifactCoreArtifactAlgorithmsContractTest` に、空rangeで`findIf`／`anyOf`／`allOf`／`noneOf`がpredicateを呼ばず、それぞれend／false／true／trueを返すcaseを追加し、21から22 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactAlgorithms empty count boundaries

- 独立 `ArtifactCoreArtifactAlgorithmsContractTest` に、empty rangeで`artifactCount`と`artifactCountIf`が0を返し、predicateを呼ばないcaseを追加し、20から21 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactMath negative-zero absolute value

- 独立 `ArtifactCoreArtifactMathContractTest` に、`artifactAbs(-0.0)`がzeroのまま符号bitを消す契約caseを追加し、17から18 casesへ拡張。現実装は比較による符号反転のためnegative zeroを返し、失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — RawImage type and buffer validity boundaries

- 独立 `ArtifactCoreRawImageContractTest` に未知pixel type拒否と、記述された画像サイズに対する不足buffer拒否を追加し、7から9 casesへ拡張。現実装はいずれの整合性も確認しないため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — RawImage unsupported pixel type validity

- 独立 `ArtifactCoreRawImageContractTest` に、形状とpixel dataが揃っていても未対応pixel typeなら無効とするcaseを追加し、7から8 casesへ拡張。現実装はpixel type名が非emptyであれば受理するため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — FrameRate invalid rational string updates

- 独立 `ArtifactCoreFrameRateContractTest` に、ゼロ分母・負の分子／分母を含む文字列で更新した場合、既存のexact rational rateを保持するcaseを追加し、30から31 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — CppLinq Where source mutation visibility

- 独立 `ArtifactCoreCppLinqContractTest` に、`Where` view生成後・走査前に元rangeを変更すると新しい要素列を参照するlazy non-owning契約を追加し、23から24 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — PathUtils QStringView Unicode filesystem conversion

- 独立 `ArtifactCorePathUtilsContractTest` に補助平面文字を含む`QStringView`からfilesystem pathへの往復変換caseを追加し、21から22 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Path rooted and relative-segment boundaries

- 独立 `PathContractTest` にseparator-only root pathsと`.`／空セグメントを含むrelative pathの契約を2ケース追加し、15から17 casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Id embedded NUL parse boundary

- 独立 `IdStandalone` にvalid UUIDの後ろへembedded NULとsuffixを加えた入力を拒否するcaseを追加し、20から21 runtime casesへ拡張。現実装はUTF-8 C stringのNULで後続を無視するため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetSequence empty and minimum length bounds

- 独立 `AssetSequenceStandalone` にempty入力の空結果と、0／負の`minFrames`でも最低2 frame必要な境界を2ケース追加し、8から10 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetSequence multiple split runs

- 独立 `AssetSequenceStandalone` に1 group内の複数連続runと、最小長未満の中間runがsingleへ戻るcaseを追加し、7から8 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — TextEncoding empty checked conversions

- 独立 `TextEncodingStandalone` にempty UTF-8のString／UTF-32変換とBOMのみ入力のBOM-aware変換が成功し空値を返すcaseを追加し、20から21 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — MultipleTag parsing and filter boundaries

- 独立 `MultipleTagStandalone` にpatternのcase-sensitive substring検索、multi-character separatorのparse/serialize、empty-string parseによる既存tag消去を3ケース追加し、17から20 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ShapeVertex normalize threshold neighborhood

- 独立 `ShapeVertexStandalone` にnormalize閾値`1e-6`の直下では不変、直上ではunit lengthへ正規化するcaseを追加し、14から15 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactOptional reference specialization

- 独立 `ArtifactOptionalStandalone` にempty reference optionalの`value()`例外と、`value_or`がfallbackへの参照aliasを返す契約を2ケース追加し、21から23 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Result status/value consistency

- 独立 `ResultStandalone` に`Status::ok()`から値なしの`Result<int>`を構築しても成功状態にならない契約caseを追加し、32から33 runtime casesへ拡張。現実装はsuccess=true/value absentとなるため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactString null pointer mutation boundaries

- 独立 `ArtifactStringStandalone` にnull pointer appendのno-op／終端維持と、null replacementによる指定範囲削除を2ケース追加し、22から24 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunctionRef assignment semantics

- 独立 `ArtifactFunctionRefStandalone` に別borrowed callableへのcopy assignment再bindと、`nullptr` assignment後のinvalid化を2ケース追加し、14から16 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactCallback capture lifetime

- 独立 `ArtifactCallbackStandalone` にclear時のowned capture解放とreset時の旧target解放／新target保持を2ケース追加し、16から18 runtime casesへ拡張。weak pointerで所有権の終了を検査する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactHashMap duplicate try-emplace laziness

- 独立 `ArtifactHashMapStandalone` に重複keyでvariadic `tryEmplace`がmapped値を構築せず既存値を維持するcaseを追加し、17から18 runtime casesへ拡張。現実装は重複確認前に構築し値を代入するため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactArray extraction and selective destruction

- 独立 `ArtifactArrayStandalone` にnon-trivial middle要素の`takeAt()`移送・shift lifetime balanceと、value指定`removeAll()`で一致値だけが一度ずつ破棄される2ケースを追加し、20から22 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactSet lifetime and values snapshot

- 独立 `ArtifactSetStandalone` にremove時の単一破棄、clear後の破棄と再利用、`values()`返却snapshotの独立性を3ケース追加し、19から22 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Buffer after-last missing delimiter

- 独立 `BufferStandalone` にdelimiter不在時のafterLast系helperが入力全体を保持するcaseを追加し、26から27 runtime casesへ拡張。現実装は先頭byteを落とすため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactRegex recompile metadata and length bound

- 独立 `ArtifactRegexStandalone` にcapture付きregexからinvalid patternへの再compile後にcapture数が0になること、および16,385-byte patternの`PatternTooLong`拒否を2ケース追加し、23から25 runtime casesへ拡張。前者は現実装が旧group countを保持するため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactOptional value and self-assignment

- 独立 `ArtifactOptionalStandalone` にempty時の`value()`例外、engaged値のself copy assignment／self move assignment保持を3ケース追加し、18から21 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactTimer unit consistency

- 独立 `ArtifactTimerStandalone` にmsを先に取得し、後続ns値がそのms値以上になる単位整合caseを追加し、14から15 runtime casesへ拡張。固定sleepは使わない。ビルド・実行は未実施。

## 進捗 2026-10-11 — Easing bounce landmarks and reversal

- 独立 `EasingStandalone` にBounce Outの3つの区間谷値と終端、およびBounce InがBounce Outの時間反転となるdense sample caseを追加し、15から17 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactChrono duration unit conversion endpoints

- 独立 `ArtifactChronoStandalone` に`INT64_MAX/MIN`からmicroseconds・milliseconds・secondsへの変換が符号を保ち、想定レンジに収まる2ケースを追加し、26から28 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AspectRatio small positive decimal

- 独立 `AspectRatioStandalone` に`0.004`が分母上限100内で最良の正の近似`1:100`となるcaseを追加し、22から23 runtime casesへ拡張。現実装では分子候補が0のとき全候補を飛ばし、初期値`1:1`を返すため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactThread condition predicate notification

- 独立 `ArtifactThreadStandalone` にpredicateが最初falseで通知後trueへ変わるcaseを追加し、15から16 runtime casesへ拡張。promiseでpredicate初回評価と通知側を同期し、固定sleepなし。ビルド・実行は未実施。

## 進捗 2026-10-11 — Audio profiler upper clamp and reset metadata

- 独立 `PerformanceProfilerStandalone` にbuffer level 100%上限clampと、`resetStats()`後のlast requested/provided frame count消去を個別caseで追加し、22から24 runtime casesへ拡張。現実装では後者は値を残すため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — Owned self-move lifetime

- 独立 `ArtifactPtrStandalone` に`Owned`のself move assignment後もpointerとvalueが保持され、destructorが一度だけ動くcaseを追加し、31から32 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactExpected default and move-only error

- 独立 `ArtifactExpectedStandalone` にdefault構築時のUnknown errorと、move-only `unique_ptr` errorの保持・取り出しを個別caseで追加し、18から20 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — MultipleTag self-assignment

- 独立 `MultipleTagStandalone` にself copy/move assignment後のtag集合保持を確認するcaseを追加し、16から17 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — SharedPtr self-copy ownership

- 独立 `SharedPtrStandalone` に唯一ownerの自己copy代入後もpointer・値・use countが維持されるcaseを追加し、20から21 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactRandom finite double extremes

- 独立 `ArtifactRandomStandalone` に有限double全域のbetween結果が有限かつ指定範囲内となるcaseを追加し、15から16 runtime casesへ拡張。現実装の`max - min`はdouble overflowするため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — HashValue fixed digest vectors

- 独立 `HashValueStandalone` に`abc`のSHA-256 / SHA-1 / MD5固定digestを確認するcaseを追加し、15から16 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FileTypeDetector four-byte header boundary

- 独立 `FileTypeDetectorStandalone` に3-byte unknownとちょうど4-byte ASCII textのmagic判定境界を追加し、17から18 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — JsonLike convertible read proxy

- 独立 `JsonLikeStandalone` に `QJsonValue`へ暗黙変換できるproxy戻り値が `JsonReadable` conceptを満たすcompile-time/runtime caseを追加し、8から9 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Path safe-relative embedded NUL

- 独立 `PathStandalone` にNULを含むパスがsafe-relative判定で拒否されるcaseを追加し、14から15 runtime casesへ拡張。現実装はNULを検査せずtrueを返すため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — TextEncoding exhaustive two-byte consistency

- 独立 `TextEncodingStandalone` に全65,536通りの2-byte列で `isValidUtf8` と `firstInvalidUtf8Offset` の判定が一致するproperty caseを追加。suite件数を19から20へ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — TextNumber unsigned negative zero and bool slices

- 独立 `TextNumberStandalone` にunsigned `-0` / `-000`の拒否とbool parserの非null終端`string_view` sliceを個別caseで追加し、17から19 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AspectRatio maximum equal dimensions

- 独立 `AspectRatioStandalone` に `INT_MAX:INT_MAX` が安全に `1:1` へ簡約されるcaseを追加し、21から22 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile duplicate legacy tags

- 独立 `AssetMetaFileStandalone` にJSON由来の重複tagを`removeTag`で全件除去し、他tagを保つcaseを追加し、15から16 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Position2DF signed zero and subnormal

- 独立 `Position2DStandalone` に負のゼロの符号と最小正subnormal値の保持を確認するcaseを追加し、17から18 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Id moved-from nil value semantics

- 独立 `IdStandalone` にムーブ元がnilとの比較・順序・hashでnilと同等であるcaseを追加し、19から20 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetFingerprint same-size content change

- 独立 `AssetFingerprintStandalone` にサイズが等しく内容が異なる小容量ファイル間でFastHashが変わるcaseを追加し、13から14 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Handle null comparisons across tags

- 独立 `HandleStandalone` に `ProjectHandle` / `LayerHandle` のnil・valid値についてnullptrとの両方向`==` / `!=`を確認するcaseを追加し、13から14 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — StringView owned UTF-8 embedded NUL

- 独立 `StringViewStandalone` に `QStringView` から所有 `std::string` へのUTF-8変換で埋め込みNULと長さを保つcaseを追加し、14から15 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — NameGenerator placeholder-like base text

- 独立 `NameGeneratorStandalone` にベース名内の`***`がnumber placeholderとして誤解釈されず維持される契約caseを追加し、14から15 runtime casesへ拡張。現実装では入力内の`***`を先に置換するため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — Numeric min/max NaN argument order

- 独立 `NumericStandalone` にquiet NaNが第1・第2引数にある場合の `min_same` / `max_same` 戻り値を確認するcaseを追加し、14から15 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ColorHarmonizer intermediate gray

- 独立 `ColorHarmonizer` suite に中間輝度の無彩色入力が hue scheme 後も RGB と alpha を保つcaseを追加し、10から11 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — SteadyClock extreme reversed ticks

- 独立 `ArtifactChronoStandalone` に`INT64_MAX`から`INT64_MIN`への逆順tick差分が0へclampされるcaseを追加し、25から26 runtime casesへ拡張。Windows経路はclamp前に減算するためoverflow見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — PathUtils Unicode filesystem round trip

- 独立 `PathUtilsStandalone` に日本語と補助平面文字を含む相対pathのQString/filesystem往復caseを追加し、20から21 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — NameGenerator wide zero-padding

- 独立 `NameGeneratorStandalone` にpadding幅40が要求幅を満たすcaseを追加し、13から14 runtime casesへ拡張。現実装は32 byteのformat bufferを使用するため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunctionRef null function pointer

- 独立 `ArtifactFunctionRefStandalone` にnull関数ポインタを渡したwrapperがinvalidとなる契約caseを追加し、13から14 runtime casesへ拡張。現constructorはinvoke thunkを常に設定するため失敗見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — Result move-only transformations

- 独立 `ResultStandalone` に`transform`と`andThen`が`unique_ptr`成功値を返すcaseを追加し、31から32 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — Optional move-only transforms

- 独立 `OptionalStandalone` に`Transform`と`AndThen`が`unique_ptr`のようなmove-only resultを扱うcaseを追加し、20から21 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactTuple reference elements

- 独立 `ArtifactTupleStandalone` にmutable `Tuple<T&>`のaliasingと`Tuple<const T&>`のconst参照保持を確認する2 casesを追加し、17から19 runtime casesへ拡張。`artifactMakeTuple`のdecay behaviorとは別に、明示reference要素の契約を固定する。ビルド・実行は未実施。

## 進捗 2026-10-11 — SharedPtr standard smart-pointer bridges

- 独立 `SharedPtrStandalone` に`std::shared_ptr`のlvalue factory共有、rvalue ownership transfer、`WeakPtr`へのstd owner assignment/null reset、`std::weak_ptr` wrap/lock/expireを確認する4 casesを追加し、16から20 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — UniformScale PImpl ownership traits

- 独立 `UniformScaleStandalone` にraw `Impl*`所有型が暗黙copy/move可能になっていないことを確認するcaseを追加し、1から2 runtime casesへ拡張。現interfaceはcopy operationsを宣言しておらず、期待に反する可能性がある。ビルド・実行は未実施。

## 進捗 2026-10-11 — SimulationManager singleton identity

- 独立 `SimulationSystemStandalone` に`SimulationManager::instance()`が同一manager参照を返すcaseを追加し、3から4 runtime casesへ拡張。singleton状態は変更せず検査する。ビルド・実行は未実施。

## 進捗 2026-10-11 — RationalTime minimum numerator rescaling

- 独立 `RationalTimeStandalone` に`INT64_MIN`をscale変換する恒等／1/2境界caseを追加し、24から25 runtime casesへ拡張。現実装の負数経路は`-numerator`を直接計算するためoverflowの可能性がある。ビルド・実行は未実施。

## 進捗 2026-10-11 — RationalTime rounding-bias overflow

- 独立 `RationalTimeStandalone` に、`numerator`は表現可能でも端数丸めbias加算がoverflowする正負の`int64_t`境界caseを追加し、23から24 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — FrameRateStorageScale upper saturation

- 独立 `FrameRateScaleStandalone` に`double(INT64_MAX)`と`DBL_MAX`のscale saturationを追加し、10から11 runtime casesへ拡張。caller fallbackの有無を両方確認する。既存実装で通るか未検証。ビルド・実行は未実施。

## 進捗 2026-10-11 — FileTypeDetector invalid UTF-8 scalar forms

- 独立 `FileTypeDetectorStandalone` にoverlong、surrogate、Unicode範囲外、途中で切れたcontinuation列をBinary分類するtable caseを追加し、15から16 runtime casesへ拡張。既存実装で期待が満たされるか未検証。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile timestamp persistence

- 独立 `AssetMetaFileStandalone` にoffset付きimport日時のsidecar save/load後のUTC保持を確認するcaseを追加し、14から15 runtime casesへ拡張。ビルド・実行は未実施。

## 進捗 2026-10-11 — ACES manager configuration and labels

- 独立 `ColorACESStandalone` にworking-space setter/getter round tripと全output preset name/unknown fallbackの2 casesを追加し、10から12 runtime casesへ拡張。suiteは既にgamut、EOTF/OETF、16-bit ramp、HDR値を網羅しているため、残りの小さな公開APIも固定する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile absent asset attributes

- 独立 `AssetMetaFileStandalone` にversionが有効でasset属性が欠落した文書のgetter境界caseを追加し、13から14 runtime casesへ拡張。path/UUID/dateの空値とUnknown typeを確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile extension-field round trip

- 独立 `AssetMetaFileStandalone` に標準field更新後も未知のroot/asset JSON fieldを保存・再読込で保持するcaseを追加し、12から13 runtime casesへ拡張。将来version metadataの互換性を確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — RawImage independent contract suite

- `Image.Raw`と`Memory.SharedPtr`、Qt Core、OpenCVだけを使う独立 `RawImageStandalone` projectを追加。copy時のmetadata保持とpixel buffer detach、copy assignmentの全フィールド置換、move construction/assignment、`RawImagePtr`共有所有権を5 runtime casesで確認する。既存2 casesと合わせて7件。CMake構成と件数を静的に確認し、configure/build/実行は未実施。

## 進捗 2026-10-11 — ArtifactThread detach lifecycle

- 独立 `ArtifactThreadStandalone` にdetach後もcallbackが完了することを確認する同期caseを追加し、14から15 runtime casesへ拡張。固定sleepではなくpromiseでworkerの開始・解放・完了を同期する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactThread Lock exception-safety

- 独立 `ArtifactThreadStandalone` に`Lock`の例外巻き戻し時のunlock確認を追加し、13から14 runtime casesへ拡張。mutexが例外後も取得可能なことを時間待ちなしで検査する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactTimer inclusive timeout equality

- 独立 `ArtifactTimerStandalone` に「観測したelapsed値と同じtimeoutはexpired」の境界caseを追加し、13から14 runtime casesへ拡張。sleepなしで`>=`の等値境界を固定する。ビルド・実行は未実施。

## 進捗 2026-10-11 — SteadyClock extreme tick delta

- 独立 `ArtifactChronoStandalone` に`Tick`全域差分の飽和境界caseを追加し、24から25 runtime casesへ拡張。現実装は比較後に`to - from`を直接計算するため、`INT64_MIN`から`INT64_MAX`の差でsigned overflowが起きる見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — FrameRate storage-scale overflow boundary

- 独立 `FrameRate` suiteに極大有限FPSが`int64_t`上限へ飽和する回帰caseを追加し、29から30 runtime casesへ拡張。現実装は`llround`の結果を直接整数化しており範囲外動作のため失敗または処理系依存結果となる見込み。ビルド・実行は未実施。

## 進捗 2026-10-11 — FastSettingsStore standalone suite

- `Core.FastSettingsStore`向けに独立module interface/implementation projectと20 runtime casesを追加。missing/malformed-file open、失敗reload時の既存cache保持、sync失敗時のdirty値保持、open先切替、typed/default reads、empty key、removeと削除永続化、nested batch、deferred sync、autosync threshold/clamp、別storeによる更新reload、dirty reload guard/discard、不正数値fallback、batch内外のclear、destructor flush、4並行writerを確認する。失敗reloadの期待は現実実装と異なる見込み。CMake登録内容と件数は静的に確認し、configure/build/実行は未実施。

## 進捗 2026-10-11 — UniformScale standalone program

- 未カバーだった `Core.UniformScale` にmodule interface / implementationだけを使う独立projectと1 runtime caseを追加。default scale=1と公開戻り型`float`を確認する。CMake登録は静的に確認、configure/build/実行は未実施。

## 進捗 2026-10-11 — FileTypeDetector supplementary UTF-8

- 独立 `FileTypeDetectorStandalone` に4-byte supplementary characterを含む有効UTF-8 headerがText判定されるcaseを追加し、15 casesにした。不正UTF-8拒否ケースと対になる正常系。ビルド・実行は未実施。

## 進捗 2026-10-11 — RationalTime negative target scale

- 独立 `RationalTimeStandalone` に負のtarget scaleで0を返すcaseを追加し、23 casesにした。現在の`rescaledTo()`は負scaleを計算に使うため失敗見込み。非正fpsを0にする`toFrameCount()`との一貫性に基づく期待値であり、`Insight.md`に記録。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetDatabase canonical same-path relink

- 独立 `AssetDatabaseStandalone` に正規化後に現在pathと同一になる別表記へのrelinkを拒否し、identity・path index・nameを保持するcaseを追加し、15 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — StringView null versus empty UTF-8 view

- 独立 `StringViewStandalone` にnull `std::string_view`と非null empty viewのQString変換後のnull/empty状態を区別するcaseを追加し、14 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactChrono running reset and accumulated cycles

- 独立 `ArtifactChronoStandalone` にrunning中の`reset()`が停止・ゼロ化するcase、複数start/stop区間の累積elapsedが後退しないcase、`Duration::fromNanos()`のint64端点保持、負の1nsからの単位変換、未start stopwatchへのstop no-opを追加し、24 casesにした。待ち時間を固定せず、ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactHashMap duplicate insertion

- 独立 `ArtifactHashMapStandalone` に既存keyへの`tryEmplace`重複挿入の保持契約、move-only mapped valueの転送、自己move assignment後のentry保持を確認する3 casesを追加し、17 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile missing sidecar

- 独立 `AssetMetaFileStandalone` にsidecar未作成のsource pathを`load()`した際、空かつinvalidなmetadataを返すcaseを追加し、12 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Keyframe constant-hold speed

- CMake登録された `KeyframeContractTest.cpp` に、Constant補間区間内の`speedAt()`が0になるcaseを追加し、15 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactThread satisfied predicate fast path

- 独立 `ArtifactThreadStandalone` に `Cond::wait(lock, predicate)` が最初からtrueのpredicateを1回評価して待機しないcaseを追加し、13 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ShapeVertex mirror handle length mode

- `ShapeVertexStandalone` に `mirrorHandles(false)` が反対側ハンドルの既存長を保つ契約caseを追加し、14 casesにした。引数の意図はAPI名からの推定であり、現在の実装はtrue/falseで同じ結果となるため失敗見込み。`Insight.md` に記録。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactTimer negative timeout before start

- 独立 `ArtifactTimerStandalone` に未start timerの負timeout／正timeoutのexpired判定を確認する2 casesを追加し、13 casesにした。負timeoutは無効timerのelapsed `-1` と直接比較するため失敗見込み。`Insight.md` に記録。ビルド・実行は未実施。

## 進捗 2026-10-11 — SimulationSystem zero-delta dispatch

- CMake登録された `SimulationSystemContractTest.cpp` に、unpaused systemへの0 delta配送と、paused systemのreset後もpause状態を維持する契約を確認する2 casesを追加し、3 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetDatabase invalid JSON atomicity

- CMake登録された `AssetDatabaseContractTest.cpp` に、不正JSONおよび配列以外のJSONを拒否した際に既存asset登録とpath indexが保持されるcaseを追加し、14 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — StringView empty needles and Unicode spaces

- `StringViewStandalone` に空prefix/suffix/needleのQStringView互換動作と、NBSP・EM SPACE・IDEOGRAPHIC SPACEを含むblank判定を確認する2 casesを追加し、13 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — PathUtils dotfile naming

- CMake登録された `PathUtilsContractTest.cpp` に、拡張子のない `.gitignore`、末尾ドットの `README.` の名前・stem・suffix契約と、base自身へのrelative pathが`.`になる契約を確認する3 casesを追加し、20 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — FileTypeDetector malformed UTF-8

- 独立 `FileTypeDetectorStandalone` に不正UTF-8 lead byte / continuation byteを含むheaderがBinary判定される回帰caseを追加し、14 casesにした。実装上は現在Text判定される見込みのため `Insight.md` に記録。ビルド・実行は未実施。

## 進捗 2026-10-11 — Easing Bezier symmetry and invalid enum

- 独立 `EasingStandalone` にdefault Bezier previewの補対称性と、未知enum値のclamped-linear fallbackを確認する2 casesを追加し、15 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetFingerprint sampling thresholds

- 独立 `AssetFingerprintStandalone` に1 MiB超でsamplingへ切り替わる境界と、3 MiB超でmiddle sampleが加わる境界を検証する2 casesを追加し、13 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetDatabase whitespace-only input

- 独立 `AssetDatabaseStandalone` に空白・tab・改行だけのpathをregisterできず、indexも変化しないcaseを追加し、13 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile normalized tag removal

- 独立 `AssetMetaFileStandalone` に空白付きで追加したtagを同じ入力で削除できる契約caseを追加し、11 casesにした。実装は追加時だけtrimするため失敗見込み。`Insight.md` に記録し、ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactThread condition notification latch

- 独立 `ArtifactThreadStandalone` に待機前の`wakeOne` / `wakeAll`通知をそれぞれ1度のwaitで消費できるcaseを追加し、12 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Handle tag boundary and value semantics

- 独立 `HandleStandalone` に全tagでのuint64上限保持とcopy/move identity保持の2 cases、全tagのtrivially-copyable compile-time assertionを追加し、13 runtime casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — NameGenerator released-name reclamation

- 独立 `NameGeneratorStandalone` にrelease済み名を別baseが再取得したあと、その名が他baseに対して再び予約されるcaseを追加し、13 casesにした。ビルド・実行は未実施。

## 記録訂正 2026-10-11 — AssetSequence frame-zero boundary

- 先行記録のcaseは独立 `AssetSequenceStandalone` が使う `AssetSequenceStandaloneTest.cpp` ではなく、通常の `AssetSequenceDetectionContractTest.cpp` に追加されていたため、独立suiteのcase数として数えない。今回、独立suite本体にcaseを追加。

## 進捗 2026-10-11 — AssetSequenceStandalone frame-zero boundary

- CMake登録された `AssetSequenceStandaloneTest.cpp` に0 frame開始の連番範囲・代表名・path patternを確認するcaseを追加し、27 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactArray squeeze contract

- 独立 `ArtifactArrayStandalone` に余剰capacityの`squeeze()`後の容量と要素保持を確認するcaseを追加し、20 casesにした。現実装は`reserve(size_)`が縮小を無視するため失敗見込みで、`Insight.md` に記録。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactRegex byte offsets and capture states

- 独立 `ArtifactRegexStandalone` にUTF-8入力中のbyte offsetと、unmatched capture対matched-empty captureを区別する2 casesを追加し、23 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — CppLinq composed lazy pipelines

- 独立 `CppLinqStandalone` に `Where`→`Select` の遅延・順序保持pipelineと、`Select` projectionの再走査時再評価を確認する2 casesを追加し、23 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — TextEncoding invalid byte offsets

- 独立 `TextEncodingStandalone` に2/3/4-byte sequenceの不正continuation byte位置と、overlong/surrogate/out-of-range scalarのlead位置を確認する2 casesを追加し、19 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — HashValue binary payload and equality

- 独立 `HashValueStandalone` にembedded NUL/0xffを含むbinary payloadのdigest照合と、invalid empty HashValueのalgorithm+bytesによる構造比較を確認する2 casesを追加し、15 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactOptional move and presence swap

- 独立 `ArtifactOptionalStandalone` にmove construction後のsource再利用と、present/empty間のswap両方向を確認する2 casesを追加し、18 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactRandom maximum integer endpoint

- 独立 `ArtifactRandomStandalone` に `INT_MAX` singleton rangeのinclusive結果を確認するcaseを追加し、15 casesにした。実装は `max + 1` を計算するためoverflow懸念を `Insight.md` に記録。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactVariant unset lifecycle

- 独立 `ArtifactVariantStandalone` にdefault unset state、unsetのcopy/move、active valueからunsetへのcopy/move assignmentを確認する3 casesを追加し、21 casesにした。empty-state destructorの懸念を `Insight.md` に記録。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactSpan const container view

- 独立 `ArtifactSpanStandalone` にmutable containerをread-only element viewとして受け、元containerの変更を読み取れるcaseを追加し、17 runtime casesにした。変換可否もcompile-time assertionで固定。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactExpected move-only transforms

- 独立 `ArtifactExpectedStandalone` に `transform` がmove-only valueを生成し、`transformError` がmove-only errorを生成する2 caseを追加し、18 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Tag StringLike setter contracts

- 独立 `TagStandalone` に `std::string` / `QString` を渡す `StringLike` setterの名前更新を確認する2 caseを追加し、16 casesにした。interface template bodyが空のため失敗する見込みだが、子repoを変更せずビルド・実行も未実施。問題は `Insight.md` に記録。

## 進捗 2026-10-11 — TextNumber lexical rejection and error messages

- 独立 `TextNumberStandalone` にunsigned parserのplus/whitespace拒否と、不正入力のparser別diagnosticを確認する2 caseを追加し、17 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — RationalTime large frame conversion fallback

- 独立 `RationalTimeStandalone` に大きな正負値でframe-rate乗算がint64範囲を超えた場合の丸め結果を確認するcaseを追加し、22 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Result ownership and diagnostic fallbacks

- 独立 `ResultStandalone` に `FromOptional` の値独立性と、fallback accessor後もfailure diagnosticが保持されることを確認する2 caseを追加し、31 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Position2D numeric boundaries

- 独立 `Position2DStandalone` にint座標のmin/maxとfloatの+infinity/NaN保持を確認する2 caseを追加し、17 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — PerformanceProfiler scope/event edge cases

- 独立 `PerformanceProfilerStandalone` にactive scopeなしのend処理と、負/zero subscriber count時のevent count・peakを確認する2 caseを追加し、22 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — PerformanceProfiler empty query behavior

- 独立 `PerformanceProfilerStandalone` に未知timerの空履歴／zero stats、および0件frame履歴でframe/scope statsが空になる2 caseを追加し、20 casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Keyframe exact outer endpoints

- 独立 `KeyframeStandalone` にtrackの最初と最後の正確なkey時刻で保存値をそのまま返すcaseを追加し、14ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactHashMap erase after rehash

- 独立 `ArtifactHashMapStandalone` にautomatic rehash後のeraseでsizeと近傍keyの到達性が維持されるcaseを追加し、14ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — FloatRGBA HDR lerp channels

- 独立 `FloatRGBAStandalone` にHDR／負値を含むchannelをlerpしても色成分自体はclampされず、4成分を補間するcaseを追加し、16ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Numeric signed-zero tie behavior

- 独立 `NumericStandalone` にequalな正負zeroのtie時、min/maxが先頭operandの符号表現を保持するcaseを追加し、14ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactCallback return and exception forwarding

- 独立 `ArtifactCallbackStandalone` にmove-only return valueの転送とcallable例外の伝播を確認する2ケースを追加し、16ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunctionRef const callable borrowing

- 独立 `ArtifactFunctionRefStandalone` にconst-qualifiedなlvalue callableをcopyせず借用して呼び出せるcaseを追加し、13 runtime casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — SharedPtr const conversion

- 独立 `SharedPtrStandalone` にmutable SharedPtrから`SharedPtr<const T>`へのcopy変換でobject／control blockを共有し、const経由で最新値を読めるcaseを追加し、16ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — SharedPtr converting copy assignment

- 独立 `SharedPtrStandalone` に派生型pointerから基底型SharedPtrへのcopy assignment後もpointer identity・値・control block countが保たれるcaseを追加し、15ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — SharedPtr interoperability and weak reset

- 独立 `SharedPtrStandalone` に`std::shared_ptr`との相互変換でcontrol blockを共有するcaseと、weak resetがstrong ownerを解放しないcaseを追加し、14ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Path nonalphabetic drive prefixes

- 独立 `PathStandalone` に数字・記号で始まる`x:/`風prefixはabsolute drive path扱いされないことを追加し、14ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — FileTypeDetector UTF-8 text header

- 独立 `FileTypeDetectorStandalone` にUTF-8 BOMと日本語multibyte bytesで始まるtextをmagic／総合判定の両方でText分類するcaseを追加し、13ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Easing overshoot and bounce bounds

- 独立 `EasingStandalone` にBack／Elastic系の負側・正側overshootとBounce系の正規化範囲をdense input gridで確認する2ケースを追加し、13ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — JsonLike array and combined concept constraints

- 独立 `JsonLikeStandalone` にkeyed-object形状を持たない`QJsonArray`はread/write対象外であることと、片方向だけ対応するproxyは`JsonLike`を満たさないことをcompile-timeで固定し、8 runtime cases／15 compile-time assertionsにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactTimer unstarted timeout state

- 独立 `ArtifactTimerStandalone` に未startのtimerでは0ms timeoutがexpiredにならないcaseを追加し、11ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetDatabase missing-source registration

- 独立 `AssetDatabaseStandalone` に未作成のsource fileでもasset identityを登録でき、absolute path／basename検索が成立するcaseを追加し、12ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile asset type whitespace

- 独立 `AssetMetaFileStandalone` にasset type名の前後空白・改行をtrimして既知typeとして解釈するcaseを追加し、10ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Handle default state across tags

- 独立 `HandleStandalone` にAsset／Project／Layerの各tagでdefault構築時のraw値0・invalid状態が揃うcaseを追加し、11ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AspectRatio malformed pair components

- 独立 `AspectRatioStandalone` にwidth／heightの片方を整数にparseできないpair文字列を与えても、現在値を保持するケースを追加し、21ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — StringView supplementary character slice

- 独立 `StringViewStandalone` にUTF-16で2 code unitを占めるサロゲートペアだけをsliceし、QString／UTF-8変換後も文字を保つcaseを追加し、11ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — StringView substring conversion bounds

- 独立 `StringViewStandalone` に終端文字列全体ではない`QStringView` sliceのQString／UTF-8変換でslice範囲を守るcaseを追加し、10ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Id UUID-value construction

- `IdStandalone` に `getUuid()` が返すUUIDから再構築したIdが元と等価で、canonical文字列も保つケースを追加し、19ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Buffer empty prefix and suffix slices

- `BufferStandalone` に空prefix／suffixが値全体と一致する場合のslice契約を追加し、26ケースにした。空suffix後のsliceは空になることも確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Buffer delimiter edge slices

- `BufferStandalone` に先頭／末尾delimiterから空field sliceが得られ、反対側のsliceが正しく残るケースを追加し、25ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — DepthMap zero-height resize

- `DepthMapStandalone` に高さ0でresizeした際も既存mapが空へ戻るケースを追加し、25ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — DepthMap negative resize dimensions

- `DepthMapStandalone` に負の幅・高さでresizeした場合に既存mapが空へ戻る独立ケースを追加し、24ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — DepthMap copy independence

- `DepthMapStandalone` にコピーしたmapの片方だけを変更しても、元mapの寸法・sample値が独立して保たれるcaseを追加し、21ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — DepthMap copy assignment isolation

- `DepthMapStandalone` にcopy assignment後のshape／値の複製と、代入先をresizeしても元mapが変化しない契約を追加し、22ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactVariant self-swap and lifetime balance

- 独立 `ArtifactVariantStandalone` にself-swapと、非trivial alternativeを使ったcopy/move/assignment/swap後のlifetime balanceを確認する2ケースを追加し、18ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactVariant same-alternative operations

- 独立 `ArtifactVariantStandalone` に同一alternative内のcopy/move assignmentとswapを確認する2ケースを追加し、18ケースにした。self-swapと非trivial alternativeのlifetime balanceも検査する。ビルド・実行は未実施。

## 進捗 2026-10-11 — StringView thread-local storage isolation

- 独立 `StringViewStandalone` に別threadでの`toStdStringView`呼び出し後も呼び出し元threadのviewが保持されるcaseを追加し、9ケースにした。promise/futureでworker完了を同期する。ビルド・実行は未実施。

## 進捗 2026-10-11 — MultipleTag identity properties

- 独立 `MultipleTagStandalone` に大小文字を区別するtag identity、empty setの演算identity、empty prefix filter、invalid regex filterを4ケース追加し、16ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — StringView embedded NUL reverse conversion

- 独立 `StringViewStandalone` に、埋め込みNULを含む`std::string_view`からQStringへの変換でUTF-16長とNUL位置を保つcaseを追加し、8ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Tag standalone lifecycle suite

- `Utils.Tag`、`UniString`とその最小module依存だけを組む独立project/executableを追加し、default、UTF-8/16/32 Unicode名、embedded NUL、default Tagへのsetter、deep copy、self-copy/move、moved-from destinationへのcopy/move復旧、move後sourceのempty状態と再代入を14 runtime casesと2 compile-time assertionsで検査する。汎用StringLike setterは現行実装のno-opを確認したため対象外とし、Insightに記録。ビルド・実行は未実施。

## 進捗 2026-10-11 — FloatRGBA interpolation non-finite parameters

- 独立 `FloatRGBAStandalone` にlerpの±infinity endpoint clampとNaNの4channel伝播を確認する2ケースを追加し、15ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactThread ownership traits

- 独立 `ArtifactThreadStandalone` にThread/Mutex/Lock/Condのcopy・move禁止を確認する8 compile-time assertionsを追加。11 runtime casesは変更なし。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunctionRef reference argument

- 独立 `ArtifactFunctionRefStandalone` に`int&`引数が借用callableへ転送され、呼び出し元の値を変更できるcaseを追加し、12ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — JsonLike constrained writer values

- 独立 `JsonLikeStandalone` に、concept制約付きwriterでJSON nullとarrayを保存・読戻すcaseを追加し、8 runtime casesと12 compile-time assertionsにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactVariant move-state boundaries

- 独立 `ArtifactVariantStandalone` に、move後sourceのactive alternative有効性とself-move代入の値保持を2ケース追加し、18ケースにした。同一alternative内のassignment/swap、self-swap、non-trivial alternativeのlifetime balanceも検査する。未設定Variantの破棄は既知の未構築storage懸念があるため引き続きテストしない。ビルド・実行は未実施。

## 進捗 2026-10-11 — Defaults type and validity invariants

- 独立 `DefaultsStandalone` に正のdimension/timing/audio容量のruntime invariantを追加し、default定数3種の型をcompile-timeで固定した。suiteは6 runtime casesと13 compile-time assertions。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactRandom singleton integer ranges

- 独立 `ArtifactRandomStandalone` に0と正数の整数singleton rangeを32回ずつ確認するcaseを追加し、14ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactRandom wide integer range

- 独立 `ArtifactRandomStandalone` に`int`のほぼ全域（`INT_MIN`〜`INT_MAX-1`）で範囲内に収まることを確認する256 sample caseを追加し、13ケースにした。最大値で`max + 1`を評価する経路は避けている。ビルド・実行は未実施。

## 進捗 2026-10-11 — Path input boundary matrix

- 独立 `PathStandalone` にembedded NULの位置別拒否（先頭／末尾／separator直後）と、ASCII大文字・小文字52種すべてのdrive-letter絶対判定を2ケース追加し、13ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ACES gamut predicate channels

- 独立 `ColorACESStandalone` に、負値をR/G/B各channelへ個別に与えたとき拒否し、blackを受け入れるcaseを追加し、10ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetFingerprint standard vectors

- 独立 `AssetFingerprintStandalone` の`abc`とempty fileについて、Qtから期待値を再計算せず固定MD5/SHA-256標準digestを照合するようにした。`abc`のFastHash=MD5も同caseで確認し、suiteは11ケース。ビルド・実行は未実施。

## 進捗 2026-10-11 — Handle raw-value and nullptr boundaries

- 独立 `HandleStandalone` に最大`uint64_t`値のvalid/ordering境界と、`nullptr`比較の左右両順を確認する2 runtime casesおよび最大値のcompile-time assertionsを追加し、10ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — MultipleTag clear and custom separator

- 独立 `MultipleTagStandalone` にcustom separatorによる正規化parse/serialize往復と、`clear()`の冪等性・再利用を確認する2ケースを追加し、16ケースにした。大小文字区別、空集合identity、filter境界も検査する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Easing NaN propagation

- 独立 `EasingStandalone` にNaN入力が22種すべてのcurve評価結果へ伝播するcaseを追加し、11ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactTimer timeout boundary

- 独立 `ArtifactTimerStandalone` に負のtimeoutは開始後ただちにexpiredとなるcaseを追加し、10ケースにした。時間待ちを使わない。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunctionRef forwarding coverage

- 独立 `ArtifactFunctionRefStandalone` に参照戻り値のalias保持、参照引数による呼び出し元変更、move-only値引数の転送を確認する3ケースを追加し、12ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — StringView standalone Unicode coverage

- Qt Coreと`Utils.String.View` moduleだけで動く独立suiteに、補助平面文字を含む文字列のUTF-8往復変換caseを追加し、7 runtime casesにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetSequence standalone input ordering

- 独立 `AssetSequenceStandalone` に、複数prefixのフレーム名を意図的に混在させた入力でもgroup順と各group内のframe順が安定するcaseを追加し、26ケースにした。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetDatabase standalone and coverage

- `AssetType` / `Asset.Database` interface/implementationとQt Coreだけの独立projectを追加し、既存suiteを4から11 runtime casesへ拡張。preferred UUID衝突、canonical aliasとtype維持、未知UUID no-op、unregister再利用、type filter、clear後再登録、empty JSON arrayによる置換を確認する。singleton状態をsnapshot guardで復元する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetMetaFile standalone and coverage

- `Asset` / `AssetType` interfaceと`AssetMetaFile.cppm`、Qt Coreだけで動く独立projectを追加し、既存suiteを4から9 runtime casesへ拡張。proxy上書き、tag削除、全AssetType名のcase-insensitive parse、custom value欠損／上書き保持、source path必須のsaveを追加する。ビルド・実行は未実施。

## 進捗 2026-10-11 — JsonLike coverage expansion

- 既存独立suiteを4から7 runtime casesへ拡張。missingとexplicit nullの区別、nested objectのreadback、代入可能proxyを追加し、非const read-only shapeと型不一致writerのconcept境界を含む12 compile-time assertionsで判定する。ビルド・実行は未実施。

## 進捗 2026-10-11 — FileTypeDetector standalone and coverage

- `File.TypeDetector` interface/implementation、Qt Coreだけで動く独立projectを追加し、既存suiteを4から12 runtime casesへ拡張。代表19 magic signature、extension family/case folding、4-byte判定境界、text control byte、extension/magic precedenceとbinary fallbackを確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactCallback coverage expansion

- 既存独立suiteを10から14 runtime casesへ拡張。empty std::functionのconstruct/reset、clear冪等性、void callbackでのreferenceとmove-only value転送を追加する。API commentに存在しない`ifValid()`の記述がある点をInsightに記録。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunction standalone

- `Core.ArtifactFunction` moduleだけの独立project/executableを追加し、callable格納／呼出し、SBOとheap経路、void/reference引数、move・clear・swapを15 runtime casesと4 compile-time assertionsで固定する。既知のheap callable moveとempty invocationの危険経路はInsightに記録し、テストで実行しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactSpan coverage expansion

- 既存独立suiteを10から16 runtime casesへ拡張。const container view、empty container、first/lastのzero lengthとaliasing、全体／nested subspan、constexpr accessを追加し、compile-time assertionsを8件にした。default empty spanの`end()`でnull+0が行われる懸念はInsightに記録し、空spanのbegin/endを使うruntimeテストは未追加。ビルド・実行は未実施。

## 進捗 2026-10-11 — AspectRatio standalone

- `Core.AspectRatio` と `UniString` の必要module/実装、Qt Coreだけを組み合わせた独立project/executableを追加。constructor簡約、colon/slash/pixel/decimal parse、trim・不正入力保持、zero denominator、copy/moveを20 runtime casesと4 compile-time assertionsで確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Core Defaults standalone

- `Core.Defaults` moduleだけで動く独立project/executableを追加。Composition、audio、renderの全10定数をcompile-time assertionで固定し、全定数の型を13 compile-time assertionsで固定。用途別6 runtime casesで既定値、AudioRingBufferの8秒容量関係、正のdimension/timing/audio容量を確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Position2D standalone

- `Core.Position2D` interface/implementation、Qt Core、GoogleTestだけで動く独立project/executableを追加。整数・浮動小数点座標の既定値、Set/Get、copy/moveと独立性を15 runtime casesおよび8 compile-time assertionsで固定する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactPtr standalone

- `Core.ArtifactPtr` moduleだけを公開依存とする独立project/executableへ拡張し、`Ptr` / `Ref` / `WeakPtr` / `Owned` の寿命・状態遷移21ケースと、move-only `UniquePtr` の所有権操作10ケース、6つのcompile-time assertionを確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — SharedPtr standalone

- `Memory.SharedPtr` moduleだけを含む独立project/executableを追加し、空/null、makeShared、共有所有権、move/reset、派生型変換、static/dynamic cast、WeakPtrの期限切れ、custom deleter、swapを12ケースで確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — SimulationSystem manager standalone

- `SimulationManager`を他テストの状態から隔離する独立executableと1ケースを追加。active/paused systemのupdate選別、deltaTime転送、paused中も含めたresetAll、resume後の更新再開を確認する。managerにclear/remove APIがないため単一テスト内でsingletonを使い切る。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactThread standalone

- `Core.ArtifactThread`とOptionalだけの独立projectに11ケースを用意。初期停止状態、start/join、join冪等性、join後再start、実行中startのno-op、move-only callable capture、MutexのtryLock、別thread所有中のtryLock拒否、Lock guard解放、再帰lockのunlock balanceをpromise同期と状態検査で確認する。sleepによる時間依存は避ける。ビルド・実行は未実施。

## 進捗 2026-10-11 — Result coverage expansion

- 既存独立suiteを22から29 runtime casesへ拡張し、value accessorの参照カテゴリを3 compile-time assertionsで固定。default failure、mutable value、reference-returning transformのdecay、transform/andThen時のdiagnostic context保持、move-only payload、fallback overload、Status code normalizationを追加する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactString coverage expansion

- 既存独立suiteを16から22 runtime casesへ拡張。explicit-length StringViewのNUL、small/heap assignmentとself assignment、同一String内replacement参照、aliasing view appendのinline growth、zero-length view、ASCII helperのhigh-bit byte保持を追加する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactAlgorithms coverage expansion

- 既存独立suiteを12から20 runtime casesへ拡張。container contains、predicate short-circuit回数、min/max同値時index、removeIf empty/none/all、sort 0/1/already-sorted、fill empty/full、reverse 2/odd、lowerBound中間挿入位置を追加する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactRegex standalone

- `ArtifactRegex`、String、Array、Utility、Optionalだけを含む独立projectと21ケースを追加。ASCII class/shorthand、anchors/boundaries、capture/noncapture、greedy/lazy、lookahead/backreference、findAll offset/zero-width進行、replacement capture escaping、compile errorと再compileを対象にする。ビルド・実行は未実施。

## 進捗 2026-10-11 — CppLinq coverage expansion

- 既存standalone suiteを15から21 runtime casesへ拡張。empty Any predicate未実行、All全件評価、Where反復ごとのlazy predicate、全件通過filter、Selectの要素型変更と順序、FirstOrDefaultのowned resultを追加する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactOptional core standalone

- `Core.ArtifactOptional`そのものをimportする依存なし独立projectと16ケースを追加。値／参照specializationのアクセス、copy/move presence遷移、fallback、resetと破棄回数、present同士のswap、factory forwardingを対象にする。empty/present swapと空initializer-list構築は実装上の未構築union member操作を避けて対象外。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactHashMap coverage expansion

- 独立suiteを9から13 runtime casesへ拡張。mapped constructor argument forwarding、custom hash/equalityでの同値key、rehash後のmapped value pointer安定性、find iteratorのpost-incrementを追加する。衝突bucket内iterationとmoved-from破棄の既知不具合は別途記録済みで、この変更では未修正。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactArray coverage expansion

- 既存独立suiteを14から19 runtime casesへ拡張。replaceの有効／無効index、insert境界、reserve時の要素保持と縮小要求のno-op、mutable find参照、const data/iterationを追加する。既知の`squeeze()`不具合はこの追加suiteの期待値に含めない。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactSet coverage expansion

- 既存独立suiteを16から19 runtime casesへ拡張。collision chain内の複数equality class、self-move assignment、chain全削除後の再利用を追加し、iterator dereferenceのconst性を2つのcompile-time assertionsで固定する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactTuple coverage expansion

- 独立suiteを13から17 runtime casesへ拡張し、tuple size・constexpr access/equalityに加えてcopy/move traits、string literal decayのstatic assertionsを9件にした。default initialization、direct lvalue copy、string literal decay、self-copy assignmentを追加する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactExpected coverage expansion

- 既存独立suiteを9から16 runtime casesへ拡張し、value accessorの参照カテゴリを3 compile-time assertionsで固定。mutable access、custom error型でのandThen/orElse/errorOr、成功時transformError、move-only値の収納とmove取得を追加する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactSaturation boundary matrix expansion

- 既存独立suiteを11から18 runtime casesへ増やし、11 compile-time assertionsへ拡張。int8/uint8のadd・sub・mul境界、-1/zero乗算、int64/uint64相互castの全幅端点と隣接値を追加する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactMath coverage expansion

- 既存の`ArtifactMath`独立suiteを12ケースから17ケースへ拡張。mixed-type overloadのpromotion、4象限のatan2、float extrema、move-only値のswapとself-swap、float lerpの型と端点を追加し、既存ケースにもrounding／符号の境界値を追加する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactSpan standalone

- `Core.ArtifactSpan` だけを依存する独立projectと10ケースを追加。空状態、pointer/count・container構築、参照性、const要素、iteration、subspan/first/last、0長subspanを扱う。範囲外アクセスは前提が未定義なので対象外。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactFunctionRef standalone

- `Core.ArtifactFunctionRef` だけを依存する独立projectと11ケースを追加。null/default、関数ポインタ、借用 callable の状態変化、void/参照戻り値、move-only引数、clear、copy、非copy可能lvalue、および一時callableを拒否するcompile-time contractを確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactCallback standalone

- `Core.ArtifactCallback` だけを依存する独立projectと10ケースを追加。validity、引数・move-only引数転送、mutable callable、参照戻り値、clear/reset、copy状態の独立性、Action aliasを確認する。無効callbackのinvokeはabortするため呼び出さず、無効状態は validity API で検査する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ShapeVertex standalone

- `ShapeVertex` moduleだけを依存する独立projectと13ケースを追加。Vec2の加減算・スカラー積・長さ・正規化、ゼロ／閾値ベクトル、頂点とhandleの状態、world位置、handle反転、corner flagを確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactVariant standalone

- `Core.ArtifactVariant` だけを依存する独立projectと14ケースを追加。複数alternativeの構築・識別・取得、const/mutableアクセス、copy/moveとmove後source状態、異種代入、自己copy/move代入、異種swapを確認する。未設定状態の破棄は実装の未構築storage寿命に関わる懸念を避けるため未実行。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactTimer standalone

- Qt Coreと`Core.ArtifactTimer`だけを依存する独立projectと10ケースを追加。無効／開始／restart状態、elapsedの非負・単調性、負／0／最大timeout判定を待ち時間依存なしで確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactChrono standalone

- OS monotonic clock APIだけを使う`Core.ArtifactChrono`独立projectと19ケースを追加。Durationのunit conversion／rounding／算術／比較、SteadyClockのtick換算と逆順クランプ、Stopwatchの状態遷移・停止中の安定性を確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — ArtifactRandom standalone

- Qt Coreと`Core.ArtifactRandom`だけに依存する独立suiteを10から12 runtime casesへ拡張。整数rangeのINT_MIN下限・INT_MAX直前、およびint/double overloadの戻り型を確認する。乱数列の固定順序や両端の出現確率は要求しない。ビルド・実行は未実施。

## 進捗 2026-10-11 — Qt Path utility standalone

- Qt Coreのみで動く `Utils.Path` 独立projectと17ケースを追加。filesystem/string conversion、icon resource fallback、file name/base/suffix、parent/absolute/relative、path normalization、互換alias群を確認する。実行fileやcurrent directoryに依存する値は一時directoryまたは相互整合で確認。ビルド・実行は未実施。

## 進捗 2026-10-11 — Performance profiler standalone

- `PerformanceProfiler` の独立projectと15ケースを追加。StartupProfilerのsum/override/report、timer registryのlatest・100件上限、scoped timer、frame/scope/eventのmetadata・peak・統計・reportを確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — Buffer standalone contracts

- Qt Coreのみで動く `Utils.Buffer` 独立projectと24ケースを追加。byte／QStringView変換、NUL保持、prefix/suffix/contains、trim、split群、alias、repeat、appendを確認する。QString→view lifetimeとdelimiter未検出のafterLast動作は、既知の要調査点として扱い本suiteでは不安定・未確定な期待値を置かない。ビルド・実行は未実施。

## 進捗 2026-10-11 — CppLinq standalone

- ranges-only `CppLinq` を独立project化し、Any／All短絡、空range、FirstOrDefault、Where／Selectの順序とlazy evaluationを確認する12ケースを追加。ビルド・実行は未実施。

## 進捗 2026-10-11 — PatternNameGenerator standalone

- `PatternNameGenerator` をcontainer moduleだけに依存する独立projectで確認する10ケースを追加。placeholder置換、zero padding、base別counter、name衝突回避、release、幅超過を対象にする。ビルド・実行は未実施。

## 進捗 2026-10-11 — UUID Id standalone

- `Utils.Id` をQt Core／Boost.UUID依存だけの独立projectにし、random／Nil／parse、copy・move、self assignment、ordering、typed wrappers、qHashを確認する独立suite。後続進捗を含めて19ケース。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetFingerprint standalone

- Qt Coreのみで動く `Utils.Fingerprint` の独立projectと10ケースを追加。既知digest、empty／missing file、小サイズ時のMD5同一性、1 MiB閾値、大容量fast hashの決定性・sample変更感度・sample外変更の扱いを確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — HashValue standalone

- Qt Coreのみで動く `Utils.HashValue` の独立projectと11ケースを追加。SHA-256／SHA-1／MD5照合、空data／file、hex・等価性、大容量fileのchunked read、全algorithmのfile/data一致、open失敗を対象にする。ビルド・実行は未実施。

## 進捗 2026-10-11 — Text string helpers standalone

- `Utils.Text.String` の独立projectと15ケースを追加。ASCII whitespace、NUL／非ASCII保持、prefix/suffix境界、split空要素、join、round trip、view backing storageを対象にした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Path contract standalone

- `Utils.Text.Path` の既存6ケースを独立projectに載せ、empty／mixed separator／dot segment／rooted form／非ASCII byte保持を確認する5ケースを追加。計11ケースを単独実行できる構成にした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Text encoding standalone

- `Utils.Text.Encoding` の独立projectと17ケースを追加。全256 single-byte分類、UTF-8境界／拒否シーケンス、invalid offset、BOM検出・除去、UTF-32 decode、codepoint count、エラーcontextを分離実行できる構成にした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Text number parsing standalone

- `Utils.Text.Number` をResult系moduleのみの独立projectで確認する12ケースを追加。int64／uint64の端点、overflow、部分文字列・空白・符号、bool表記、contextとsource locationを含む。ビルド・実行は未実施。

## 進捗 2026-10-11 — Optional standalone contracts

- `Utils.Optional` の独立suiteを追加し、値／空値、fallbackの遅延、transform参照decay、andThen、filter、Result変換、pointer helper、存在alias群を16ケースで確認する構成にした。ビルド・実行は未実施。

## 進捗 2026-10-11 — Handle value-type standalone

- `Utils.Handle` をQt Coreだけに依存する独立suite化し、nil／valid、64-bit保持、型分離、比較順序、nullptr互換、helper、Qt hashの10ケースを追加。ビルド・実行は未実施。

## 進捗 2026-10-11 — Utility concepts standalone

- `Numeric.Like`、`Utils.Point.Like`、`Vector.Like`、`Utils.String.Like` を独立suite化し、numeric arithmetic constraint、size-like method／width category、vector coordinate member、QString／標準文字列変換の成立・拒否条件を確認する21ケースを追加。Qt Coreのみを追加依存とする。ビルド・実行は未実施。

## 進捗 2026-10-11 — Numeric utility standalone

- 依存なしの `Utils.Numeric` を独立 CMake project 化し、min/maxの順序・等値・負数・異種数値型・constexprと型推論を確認する9ケースを追加。ビルド・実行は未実施。

## 進捗 2026-10-11 — Result standalone contracts

- `Utils.Result` を独立 CMake project から確認する18ケースを追加。成功／失敗、fallback遅延、transform／andThen伝播、orElse回復、error context、source location、VoidResult、factory／optional helperを含む。ビルド・実行は未実施。

## 進捗 2026-10-11 — FloatRGBA standalone contract

- `FloatRGBAStandalone` を追加し、既存の大きな ColorBridge suite に含まれていた値型契約を独立 CMake project から実行できるようにした。構成子、channel setter/indexing、境界例外、RGB/alpha演算、lerpの有限／非有限parameter clampとNaN伝播、FloatColor双方向変換、copy/move assignment、swap/equalityなど15ケース。ビルド・実行は未実施。

## 進捗 2026-10-11 — SurfacePixelConversion unknown-metadata paths

- 独立 `SurfacePixelConversion` suiteにtransferKnown=false時のstale curve無視と、Unknown primaries入力のlinear sample保持／明示出力descriptorを2ケース追加し、31ケースにした。

## 進捗 2026-10-11 — Keyframe curve parameter coverage

- 独立 `KeyframeStandalone` にunequal-time Hermite linear-motion propertyと、outgoing keyのcustom Bezier handles反映を2ケース追加し、13ケースにした。

## 進捗 2026-10-11 — FrameRate adjustment and JSON fallback

- 独立 `FrameRateStandalone` に正負fractional speed deltaと、不完全rational JSONからlegacy fpsへfallbackする2ケースを追加し、26ケースにした。

## 進捗 2026-10-11 — DepthMap invalid input paths

- 独立 `DepthMapStandalone` にempty mapへのsetValue no-opと、既存mapを壊れた画像fileのloadでclearするcaseを追加し、20ケースにした。

## 進捗 2026-10-11 — Keyframe speed boundary contract

- 独立 `KeyframeStandalone` にtrack外／最初／最後のkeyでのfinite-difference speedを確認するcaseを追加し、11ケースにした。

## 進捗 2026-10-11 — Easing monotonicity properties

- 独立 `EasingStandalone` にovershoot／bounceを意図しない12 curveの[0,1] boundednessと単調性を確認する1001点gridを追加し、11ケースにした。NaN入力が22 curveすべてに伝播することも確認する。ビルド・実行は未実施。

## 進捗 2026-10-11 — AssetSequence directory ordering

- 独立 `AssetSequenceStandalone` に作成順とfilesystem列挙順に依存せず、directory wrapperがgroupとsinglesを決定順で返すcaseを追加し、25ケースにした。

## 進捗 2026-10-11 — SurfacePixelConversion alpha-mode matrix

- 独立 `SurfacePixelConversion` suiteにStraight／Premultiplied／Opaque × RGBA／BGRA × float／half／sRGB-byte全18組合せを固定するcaseを追加し、29ケースにした。Opaqueの非finite alpha、論理RGBA順、half bit、byte quantizationも確認する。

## 進捗 2026-10-11 — SurfacePixelConversion metadata matrix

- 独立 `SurfacePixelConversion` suiteに非対応primaries全種の拒否と、17種の宣言済みTransferFunctionがdescriptorからdecodeへdispatchされることを照合する2ケースを追加した。suiteは26から28ケース。

## 進捗 2026-10-11 — AssetSequence token-length boundaries

- 独立 `AssetSequenceStandalone` に18桁paddingでframe 0から続くgroup、int64には収まる19桁tokenの拒否、拡張子内数字をframe tokenから除外する3ケースを追加し、24ケースにした。

## 進捗 2026-10-11 — ImageSurfaceView semantic-layout coverage

- 独立 `ImageSurfaceViewTest` にunknown color metadata保持、BGRA straight-sRGB raw sample保持、float／byte surface validity境界の3ケースを追加した。layout viewが色変換やalpha変換を行わない契約も確認対象にした。

## 進捗 2026-10-11 — DepthMap interpolation and load coverage

- 独立 `DepthMapStandalone` にresize後の全sample再初期化、5×4 affine planeの21×21 bilinear sample grid、RGB PNG loadとQt grayscale conversion referenceの3ケースを追加し、18ケースにした。

## 進捗 2026-10-11 — AssetSequence group metadata contracts

- 独立 `AssetSequenceStandalone` にSequenceGroupのrepresentative、printf path pattern、表示名、Split／Preserve時のmissing frame metadataを確認する2ケースを追加し、20ケースにした。

## 進捗 2026-10-11 — TimeCode boundary contracts

- 既存の `FrameTimeTest` にTimeCodeのinvalid FPS fallback、drop-frame設定可能範囲、24/25/29.97/59.94 nominal-rate HMSF変換、実FPS秒変換、RationalTime往復とhalf-frame丸め、copy/moveを7ケース追加した。TimeCode専用ケースは4から11へ増加。フルArtifactCore依存の既存suiteに追加し、独立ターゲット／Artifact.exe非依存suiteとは数えない。

## 進捗 2026-10-11 — standalone FrameRate display and JSON coverage

- `FrameRateStandalone` にdisplay string、stored-float equality、JSON legacy field優先順位／無効rational、FrameRateOffsetの比較・変更、drop-frame全rational rateとfloat近似境界を7ケース追加し、24ケースにした。Easing独立suiteには全22曲線×1001点のfinite sweepを追加した（9ケース）。

## 進捗 2026-10-10 — RationalTime signed-boundary expansion

- 独立 `RationalTimeStandalone` に負scale正規化、複数scale比のhalf-away-from-zero境界、異scale cancellation、正負frame-count tie、加算／減算の逆操作grid、signed seconds conversion gridを6ケース追加し、合計21ケースにした。

## 進捗 2026-10-10 — standalone keyframe interpolation suite

- `tests/ArtifactCore/KeyframeStandalone` を追加し、`Math.Interpolate` moduleだけで動く独立CTest実行ファイルを用意した。空／単一track、挿入順、端点clamp、不均一時間のlinear速度、constantから次segmentへのexact-key境界、Hermite smoothstepとextrapolation、Catmull-Romのlinear/control-point特性、default Bezier track、clearを10ケースで検査する。ArtifactCore全体・Artifact.exeにはリンクしない。configure/build/CTestは未実施。

## 進捗 2026-10-10 — standalone easing contract suite

- `tests/ArtifactCore/EasingStandalone` を追加し、`Animation.EasingCurveUtil` と `Math.Interpolate` のみを使う独立CTest実行ファイルを用意した。22種すべての端点、入力クランプ、代表curveのin/out対称性、dense finite sweep、代表値、表示名の一意性、候補一覧網羅、保存用interpolation ordinalを9ケースで検査する。ArtifactCore全体・Artifact.exeにはリンクしない。configure/build/CTestは未実施。

## 進捗 2026-10-10 — standalone FrameRate implementation suite

- `tests/ArtifactCore/FrameRateStandalone` を追加し、FrameRate実装、UniString実装、JSON adapter/registry/schema migrationの必要moduleだけを束ねた独立CTest実行ファイルを用意した。FrameRateのrational/drop-frame契約、文字列とJSON入出力、copy/move/swap、invalid値、speed操作、登録状態、FrameRateOffsetを24ケースで検査する。ArtifactCore全体・Artifact.exeにはリンクしない。AGENTS.mdの制約に従いconfigure/build/CTestは未実施。

## 進捗 2026-10-09 — standalone gamut conversion suite

- `Color.GamutConversion` 専用module targetとCTestを追加し、行列積・matrix×RGB、gamut変換の加法性・スカラー倍性、sRGB/Rec.709 primaries互換、Rec.709/Rec.2020/Display P3 primariesとwhiteのXYZ reference、Rec.2020 inverse matrixのXYZ軸referenceとD65 white→neutral、Display P3 D65 white→neutral、DCI-P3/Adobe RGB preset primaries・row-sum white referenceと各preset white→neutral、DWG forward/inverse matrix referenceとrow-sum whiteのinverse結果、DWG→Rec.709のprimary/signed-HDR sample reference、ACES AP0/AP1のD60→D65 XYZ adaptationとAP0↔AP1 primary reference、11 gamutのidentity matrix、未知enum値のD65 XYZ fallback、D60/D65 Bradford whitepoint往復、ACES AP0/AP1とRec.709間のneutral変換、sRGB/Rec.709/Rec.2020/DCI-P3/Display P3/Adobe RGB間のsigned/HDR往復、全11 gamut pairのsigned/HDR finite出力を検査する。
- `Color.GamutConversion` 専用module targetとCTestを追加し、行列積・matrix×RGB、gamut変換の加法性・スカラー倍性、sRGB/Rec.709 primaries互換、Rec.709/Rec.2020/Display P3 primariesとwhiteのXYZ reference、Rec.2020 inverse matrixのXYZ軸referenceとD65 white→neutral、Display P3 D65 white→neutral、DCI-P3/Adobe RGB preset primaries・row-sum white referenceと各preset white→neutral、DWG forward/inverse matrix referenceとrow-sum whiteのinverse結果、DWG→Rec.709のprimary/signed-HDR sample reference、ACES AP0/AP1のD60→D65 XYZ adaptationとAP0↔AP1 primary reference、ACESとD65 gamut間のsigned/HDR往復、非有限入力の伝播、11 gamutのidentity matrix、未知enum値のD65 XYZ fallback、D60/D65 Bradford whitepoint往復、ACES AP0/AP1とRec.709間のneutral変換、sRGB/Rec.709/Rec.2020/DCI-P3/Display P3/Adobe RGB間のsigned/HDR往復、全11 gamut pairのsigned/HDR finite出力を検査する。
- Visual Studio 2026 / MSVC 19.51でtargetをビルドし、個別suiteとstandalone CTest全11 suiteの実行結果を確認。

## 進捗 2026-10-09 — blend mode dispatch境界

- 2026-10-09: S-Log3 normalized code [0,1]を2048分割してdecode finite/non-decreasingを検査。black code未満のnegative linear decodeとblack code clamp、black以上の2e-6 code roundtripを確認。ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: Cineon normalized code [0,1]を2048分割しdecode finite/non-decreasingを検査。black code未満はnegative linearにdecodeされ再encodeでblack codeへclamp、black以上は2e-6以内でcode roundtrip。ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: DaVinci Intermediate normalized code [0,1]を2048分割し、decode finite/non-decreasingとencode/decode code roundtrip 2e-6を確認。専用ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: ACEScct normalized code [0,1]を2048分割し、decode finite/non-decreasingとencode/decode code roundtrip 2e-6を検査。専用ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: ACEScc normalized code [0,1]を2048分割し、decode finite/non-increasingとencode/decode code roundtrip 2e-6を検査。専用ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: ACESccのpositive linear 0.001..16を2048分割したdense gridでencode有限性と非増加性を検査。専用ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: HDR log/camera curve property coverageへACESccを追加。1..16 HDR roundtripを検査し、scene/HDR ordered testはACESccの非増加curve性質、他curveの非減少を個別に確認。ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: ACEScct/Canon Log 2/Canon Log 3のsigned scene-linear浅い負値(-1e-4)・black・toe近傍正値をencode/decodeし、finiteと2e-6以内のroundtripを検査。専用ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: PQ/HLGでnormalized linear [0,1]を1024分割し、encode/decodeの有限性・単調性・往復誤差を検査。PQのf32実測最大誤差約6.95e-5を踏まえabsolute tolerance 1e-4、専用ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: standalone ColorBridge suiteにsRGB/Gamma22/24/26/Rec.709/Rec.2020/HLGの1.0〜4.0線形HDR headroom encode/decode roundtripを追加。PQは正規化上限の異なるcurveのため対象外。専用ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: TaggedColor signed/HDR scene-linear RGBAをalpha 1.1e-6超〜1.0でpremultiplied→straight往復し、RGB/alpha・transfer/primaries metadata復元とsource不変性を確認。ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: TaggedColor premultiplicationでsigned/HDR RGBとalpha {-0.5,0,0.25,1,1.5}を組み合わせ、alpha乗数だけが[0,1] clampされ、元alpha・RGB metadataとsource値が保持される契約を追加。ColorBridge CTestとstandalone全11 suiteが合格。
- 2026-10-09: `calculatePerceptual`へsigned/HDR RGBを入力し、二乗式reference、全channel符号反転不変性、正のscaleに対する比例性を検査。ColorLuminance CTestとstandalone全11 suiteが合格。
- 2026-10-09: Broadcast-safe inspectionのlegalBlack/legalWhiteとchannelMin/channelMaxを混在させた8 RGB cornerを追加し、全て違反なし・weighted luminanceが範囲内であることを確認。ColorLuminance CTestとstandalone全11 suiteが合格。
- 2026-10-09: 未知`LuminanceStandard`のRec.709 fallbackを`inspectBroadcastSafe`経由でも検査。返却luminanceとluma/gamutの個別flag、総合flagを確認。ColorLuminance CTestとstandalone全11 suiteが合格。
- 2026-10-09: Broadcast-safe inspectionにlower/upperのlegal luminance範囲外かつchannel gamut範囲外となるRec.709 RGBを追加。返却weighted luminance reference、両violation flag、hasViolationを確認。ColorLuminance CTestとstandalone全11 suiteが合格。
- 2026-10-09: standalone Luminance suiteへ全5標準×signed/HDR RGB 3 sampleを追加。weighted luminance referenceおよびgrayscaleの3 channel一致を確認。専用ColorLuminance CTestとstandalone全11 suiteが合格。
- 2026-10-09: HSL inverse conversionへ hue 360°/420°/-60°のwrap境界を追加し、red/yellow/magenta referenceをchannel toleranceで確認。専用ColorConversion CTestとstandalone全11 suiteが合格。
- 2026-10-09: signed/HDR RGBのHSV/HSL往復テストを拡張し、中間HSV/HSLの有限性と色相・彩度・value/lightnessのreferenceも検査（HSV saturation >1、HSL saturation >1を含む）。ColorConversion CTestとstandalone全11 suiteが合格。
- 2026-10-09: standalone `Color.Conversion` suiteへsigned/HDR RGB（負値および1超えを含む4色）のHSV/HSL往復referenceを追加。専用ColorConversion CTestとstandalone全11 suiteが合格。
- 2026-10-09: 非対称2³ LUTをQColorへ適用し、alpha 0/0.125/0.5/0.875/1.0のRGB変換・alpha保持・source不変性を検査。QColor float channelの丸め差を許容する1e-5 toleranceで対象ColorLUTとstandalone全11 suiteが合格。

- 2026-10-09: CUBE save出力を再読込に頼らず直接検証。TITLE/comment/size header、header後のblank、末尾newline、全27 RGB triplet値とx-fastest/y/z走査順がsource格子と一致し、ColorLUT対象とstandalone全11 suiteが合格。
- 2026-10-09: headerless CUBEの非立方sample点数7/9/15および8 RGB triplet + 余分channel 1件を拒否する契約を追加。ColorLUT対象とstandalone全11 suiteが合格。
- 2026-10-09: inverted LUTにfull-range非線形monotonic 17³ curveを作り、0/1端点を含む9³=729 input sampleをsource→inverseで往復、全RGB誤差0.01以内を確認。既存shifted-range curveはinterior sample契約を維持。ColorLUT対象とstandalone全11 suiteが合格。
- 2026-10-09: `withIntensity(0.4)`の非対称3³全27 sample補間、validity/name/path/format/dimensions/dataSize保持、sourceとresultの相互編集独立性を追加。ColorLUT対象とstandalone全11 suiteが合格。
- 2026-10-09: `combine()`へ3³→2³と2³→3³の両方向を追加し、結果gridがreceiver解像度となり、全sampleがreceiver→argument順の直接適用値と一致する契約を確認。ColorLUT対象とstandalone全11 suiteが合格。
- 2026-10-09: ColorLUT合成にidentity左/右の全27格子点則と、invalid receiver/argument時にreceiver状態を返すfallbackを追加。対象ColorLUTとstandalone全11 suiteが合格。
- 2026-10-09: ColorLUT move constructor/assignmentの移譲先をsource破棄後に検査。validity、name、grid size、sampleを保持することを確認し、対象とstandalone全11 suiteが合格。
- 2026-10-09: mutable `rawData()`でlut cornerへNaN/±Infとfinite out-of-range RGBを注入し、public `apply()`が出力を有限[0,1]へsanitizeする契約を追加。private `sample()`は直接呼ばず、ColorLUT対象とstandalone全11 suiteが合格。

- 2026-10-09: `ColorLUT::setValue`にNaNと±InfをRGB各channelへ置いた9 invalid tripletを与え、全sampleが不変かつLUT有効を維持する契約を追加。対象とstandalone全11 suiteが合格。
- 2026-10-09: CUBE save/reload契約を2³の1 sample比較から、非対称3³ LUTの全27 grid sample RGB比較へ拡張。reload後の名前・format・dimensions・data byte数も確認し、対象とstandalone全11 suiteが合格。
- 2026-10-09: LUTManager scan candidateとColorLUT load dispatchのextension差をテスト化。`.jpeg`/`.tif`はdirectory filterに列挙されるがloaderが`Unknown LUT format`で拒否し、`.png`だけ登録される。ColorLUT対象とstandalone全11 suiteが合格。
- 2026-10-09: LUTManager directory scanに33³ Hald PNG、壊れたPNG、未対応BMPを混在させるfixtureを追加。正常PNGのみbasenameで登録され、format/grid/sampleが正しいことを検査。ColorLUT対象とstandalone全11 suiteが合格。
- 2026-10-09: HaldCLUT PNG file loadの成功fixtureを追加。33³ RGB/alphaパターンをPNGへ保存し、memory image importとのgrid metadata/data size・8つの角/内部sample一致、既定name保持を検査。ColorLUT対象とstandalone全11 suiteが合格。
- 2026-10-09: `loadFromHaldCLUT()`へ存在しないPNG pathを渡す失敗ケースを追加し、既存LUTのsamples・validity・name・format・sizeが維持され、errorMessageのみ設定される挙動を確認。ColorLUT対象とstandalone全11 suiteが合格。
- 2026-10-09: ColorLUT image importで、引数検証を通過するがtile面積不足となる画像を既存有効LUTへ渡した場合、LUTがinvalid化・sample消去され、format/grid sizeは要求画像側へ更新、nameは保持、source画像は不変となる契約を追加。対象と全11 suiteが合格。
- 2026-10-09: ColorLUT image importに、null QImageとlutSize 1 / 257の失敗を既存有効LUTへ与えてもsamples・validity・name・grid sizeが維持される契約を追加。対象とstandalone全11 suiteが合格。
- 2026-10-09: ColorLUT suiteに、有効LUTをロードした同一instanceで不正CUBEを再読込したときの状態遷移を追加。失敗後にsamples/validityが消え、file path/name/errorが失敗側へ更新されること、過去のgrid sizeとformatは残ること、applyが入力値を保持することを確認。対象と全11 suiteが合格。
- 2026-10-09: ColorLUT standalone suiteに、無効なLUTへのQColor適用と有限強度（0 / 0.35 / 1.0）の適用でRGB・alphaおよびsource colorが維持される契約を追加。対象CTestとsuite全体（11/11）を実行して合格。
- 2026-10-09: LUTManager directory loadに同じbasenameの有効CUBE/CSPを置き、両ファイルを読み込んだ件数2とregistry key 1件を確認。格納LUTがいずれかのvalid fixtureに一致し、ColorLUT対象CTestとstandalone全11 suiteが合格。
- 2026-10-09: LUTManagerへ存在しないdirectoryを渡した場合に0件を返し、既存registry entryとsampleが保持されるケースを追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: 有効CUBEがなく壊れたCUBEと未対応JPEGだけのdirectory loadで、0件・新規keyなし・既存registry/sample維持を確認。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: 同basenameの有効CUBEとinvalid CSPを併置し、Name順で後になるinvalid候補が登録済みvalid LUTを上書きしないことを確認。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: 同basenameの有効CSP/CUBEを個別validityも検証して併置。QDir::Name順（CSP後にCUBE）で後のCUBEがregistryを置換し、load件数2・key 1件となる契約を追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: 同名CUBEのdirectory reloadをA→B→Aで行い、各回load件数1・registry sampleの置換・key数1を確認。各reload前に取得したA/B LUT copyは後続reload後も元sampleを保持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: registryに既存の`grade`/`unrelated`を置き、壊れた同名`grade.cube`のみを含むdirectory loadが0件となり、両keyと既存sampleおよび取得済みcopyを保持する契約を追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: LUTManagerへの登録時にsourceを変更してもregistry値が保たれ、getLUTで取得したcopyを編集してもregistryへ漏れないsample所有権テストを追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: 未登録名のgetLUTが有効な既定Identity LUT（33³/Cube）を独立値で返し、編集してもregistryへkey追加・変更しない契約を追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: getLUT済みLUTがremove後・同名再登録後・clear後も元sampleを保つ一方、registryにはreplacement値だけが反映されるライフサイクルテストを追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: LUTManager登録名の前後にtab/newline/spaceを付け、QString trim後のkeyだけが列挙・取得可能となり、LUT sampleが保持されることを追加検査。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: trim登録後の`hasLUT/getLUT/removeLUT`は呼び出し側名を正規化せず、空白付き名はmiss/default Identity/no-op、正確なtrim済み名は削除される契約を追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: 別keyを事前登録したmanagerにdirectory loadし、対象basenameのみfile LUTへreplaceされ、無関係key/sampleが維持されるケースを追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: 存在keyを連続removeし、さらにmissing keyもremoveしても操作がidempotentで、他key/sampleと既取得LUT copyが保持されるケースを追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: directory rootと1階層下へ有効CUBEを配置し、loadFromDirectoryが直下root fileのみ1件登録しnested fileを無視する非再帰契約を追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: 有限範囲外値（-0.5 / 0.25 / 1.5）を持つ全格子CUBEが保持される一方、public apply出力は[0,1]へclampされる契約を追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: CSPでも有限範囲外値（-0.5 / 0.25 / 1.5）の格子保持とapply時clampを検査。3DLはunit-range sampleを最大値正規化せず保持する分岐を追加検査し、対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: 全8点がゼロの3DLファイルを有効な黒LUTとして受理し、format/size/全格子値とapply結果が黒である契約を追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: `createIdentity`の2/3/17点格子について全5,112 sampleの各軸値がx/y/z正規化座標に一致する検査を追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: identity factoryの上限256を実割当で確認（サイズ/約192 MiB data size/非対称座標3 sample）、257は2³へfallback。巨大gridの全走査やcopyは避け、対象ColorLUT CTest（約2.1秒）とstandalone全11 suiteが合格。
- 2026-10-09: 非対称3³ LUTにxy/xyz/xz/yz multilinear成分を与え、セル境界・内部を含む9³=729入力でtrilinear結果が解析式と誤差2e-6以内に一致する検査を追加。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: `applyWithIntensity`へ-0.5/1.5を渡すとQColor RGBがclampされずLUT/original間を外挿し、alphaは保持されるcharacterization testを追加。QColorのchannel量子化を考慮した許容差で対象ColorLUTとstandalone全11 suiteが合格。
- 2026-10-09: `withIntensity`へfinite -0.25/1.25を渡した全27格子sampleで、identity側/full LUT側へclampされることを追加検査。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: `getValue`の各軸について-1/sizeと大きな±座標を与えた12 out-of-grid caseがzero vectorを返し、valid corner値は維持される境界を追加検査。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: `getValue`境界表にx/y/z各軸の`INT_MIN`/`INT_MAX`を加え、全18 out-of-grid coordinateでzero vector、valid corners維持を確認。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: `setValue`にも各軸の`INT_MIN`/`INT_MAX`を追加し、6 extreme writesが既存center/cornersを変えずLUT validを維持する境界を確認。対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: directory APIへ実在する通常file pathを渡すと0件で既存registry/sampleを保持するケースを追加。ACL依存fixtureを避け、対象ColorLUT CTestとstandalone全11 suiteが合格。
- 2026-10-09: LUTManagerへ複数keyを登録後clearを連続実行し、空registryが保たれ、取得済みcopyは生存し、その後の新規登録が機能する再利用ケースを追加。対象ColorLUT CTestとstandalone全11 suiteが合格。

- `ColorBlendModeStandaloneTest` に未知の`BlendMode`値がpositive opacityでbaseを返すことと、opacityが0以下ならmode dispatchより先にbaseを返すことを追加した。
- Visual Studio 2026 / MSVC 19.51でblend targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — harmonizerの無彩色と複数回転角

- `ColorHarmonizerContractTest` にblack/whiteが各色相scheme後も無彩色とalphaを保つこと、Analogousの-400°が複数turnを越えて350°/100°へwrapすることを追加した。
- Visual Studio 2026 / MSVC 19.51でHarmonizer targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — luminance enum fallbackとinfinity検査

- `ColorLuminanceContractTest` に未知の`LuminanceStandard`がRec.709係数へfallbackすることと、broadcast inspectionが+∞/−∞をluminance・channel両違反として検出することを追加した。
- Visual Studio 2026 / MSVC 19.51でLuminance targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — LUT factoryサイズと非有限intensity

- `ColorLUTContractTest` にidentity LUT要求サイズ0/1/負値が2点格子へfallbackすること、`withIntensity` のNaN/±∞すべてがfull LUT側へ正規化されることを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — LUT load失敗後の復旧

- `ColorLUTContractTest` に有効なLUTを保持するobjectで不明拡張子のloadが失敗した後、正しいCUBEをloadしてvalidity・error・format・size・nameが復旧することを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — LUT format切替とcopy assignment

- 同一`ColorLUT` objectへCUBE後にCSPをloadし、format/name/path/error/sampleが新しいファイルの状態に更新されることを追加検査した。
- `ColorLUT` copy assignmentが格子サイズを置き換え、コピー後のsample編集が元LUTへ漏れないことを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — CUBEサイズ推定と範囲境界

- `ColorLUTContractTest` にCUBEのサイズヘッダー省略時にsample数から立方gridを推定するケース、最小2未満と最大256超のsize拒否を追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — 3DL sample normalizationと非有限値

- `ColorLUTContractTest` に3DL sampleが観測最大値で正規化されるchannel別referenceと、infinity sampleを含むファイルの拒否を追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — CSP宣言サイズとsample数の拒否境界

- `ColorLUTContractTest` にCSP size 1/257、壊れたsize文字列、宣言より不足／超過するdata sampleの拒否を追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — HaldCLUT縦タイルと入力保持

- `ColorLUTContractTest` に2×2 LUTを縦2タイルで格納したARGB imageからz slice順とRGB値を読むケースを追加し、alpha非依存とsource image不変を確認する。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — QColor適用時の透明RGBとalpha

- `ColorLUTContractTest` にalpha 0のQColorへ非identity LUTを適用し、RGBが変換されalphaは0のまま保持されること、入力QColorが不変であることを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — 3DLサイズ境界とCUBE保存失敗

- `ColorLUTContractTest` に3DL size 1/257の拒否、書き込み先parentが存在しない場合のCUBE保存失敗とsource LUT状態保持を追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — headerless CSPサイズ推定

- `ColorLUTContractTest` にCSPの宣言サイズ省略時、8 sampleから2×2×2 gridを推定し、終端sampleを維持することを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — CSP END DATA境界

- `ColorLUTContractTest` にEND DATA後のRGB tripletを無視し、data block内sampleだけからLUTを構築するケースを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — CSP sample row parsing

- `ColorLUTContractTest` にCSP data内の2要素／4要素行を無視する実装挙動と、3要素だが数値化できないsample行を診断付きで拒否する挙動を追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — ColorLUT moveと自己代入

- `ColorLUTContractTest` にmove constructor／move assignmentでmetadataとsampleが移ること、self-copy／self-move assignment後もLUTが保たれることを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — LUT Manager directory reload

- `ColorLUTManagerContractTest` に同じbasenameのLUTを再loadしたとき既存registry entryが新sampleへ置換されること、空directoryは0件かつregistryを維持することを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — LUT Manager name case behavior

- `ColorLUTManagerContractTest` に名前の大小文字を区別して別登録すること、`QMap`のkey順で名前一覧が返ること、大文字小文字が異なるremoveは一致しないことを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — built-in LUT generator contracts

- `ColorLUTManagerContractTest` にbuilt-in名9種の一意性と全名の登録、各LUTの17³有効格子および全float sampleの有限・[0,1]範囲を検査するケースを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — RGB888画像適用とpadded row stride

- `ColorLUTContractTest` に3×2のRGB888 source（row paddingあり）へLUTを適用し、ARGB32変換、RGB結果、alpha=255、source immutabilityを確認するケースを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT targetをビルドし、個別suiteとstandalone CTest全11 suiteで確認。

## 進捗 2026-10-09 — alpha byte量子化の隣接float

- `SurfacePixelConversionTest` に、alpha 0.5の直前・ちょうど・直後のfloat値を8-bit出力し、127/128の境界で量子化が切り替わるケースを追加した。
- SurfacePixelConversion standalone targetと色standalone CTest全10 suiteで確認。

## 進捗 2026-10-09 — half-float出力の丸め境界

- `SurfacePixelConversionTest` にbinary16通常値の中点直下・中点・直上、最小subnormalへのunderflow境界、最大有限値とinfinityへのoverflow境界を追加した。
- SurfacePixelConversion standalone targetと色standalone CTest全10 suiteで確認。

## 進捗 2026-10-09 — linear floatから8-bit sRGB出力へのclamp

- `SurfacePixelConversionTest` に、負値・1超過のlinear RGBと範囲外alphaを8-bit sRGBへ変換し、RGBの[0,1] clampとalpha clamp後の量子化を確認するケースを追加した。
- SurfacePixelConversion standalone targetと色standalone CTest全10 suiteで確認。

## 進捗 2026-10-09 — read-only padded multi-pixel surface rows

- `ImageSurfaceViewTest` に、2 pixel幅・2行・行末paddingありの読み取り専用RGBA viewを追加し、各行の最初と最後のpixelがpadding値を読まず正しい行から取得されることを検査する。
- `ImageSurfaceView` standalone targetと色standalone CTest全10 suiteで確認。

## 進捗 2026-10-09 — toe round trips and Canon Log 2 negative toe

- `ColorBridgeTest` にACEScct／S-Log3 toeの隣接float、Canon Log 2の負値と0付近のround-tripを追加した。
- Canon Log 2の計算上のnegative toe thresholdではencode/decode往復が約`0.00194065`ずれる現行挙動を個別characterizationとして固定。production code未変更。
- Visual Studio 2026 / MSVC 19.51でBridge targetをビルドし、個別suiteとCTest全10 suite成功。

## 進捗 2026-10-09 — ACEScct / S-Log3 / Canon Log 3 encode/decode breakpoints

- `ColorBridgeTest` にACEScct、S-Log3、Canon Log 3のencode/decode branch境界それぞれの隣接float検査を追加した。
- Canon Log 3のlinear encode low boundaryには約`0.0062386`、encoded decode low boundaryには約`0.0027042`の現行出力段差を検出し、characterizationとして固定。production codeは未変更。
- Visual Studio 2026 / MSVC 19.51でBridge targetをビルドし、個別suiteとCTest全10 suite成功。

## 進捗 2026-10-09 — near-achromatic HSV threshold

- `ColorConversionTest` にRGB deltaが1e-5未満のHSV無彩色扱いと、同入力をHSLでは有彩色として保持するケース、閾値より上のHSV hue/saturationとRGB復元を追加した。
- Visual Studio 2026 / MSVC 19.51でColorConversion targetをビルドし、個別suiteとCTest全10 suite成功。

## 進捗 2026-10-09 — log / camera curve absolute reference codes

- `ColorBridgeTest` にACEScct、DaVinci Intermediate、S-Log3、Cineon、Canon Log 2/3のlinear 0.18 / 1 / 4に対するencoded絶対値を6曲線×3値で追加した。
- Visual Studio 2026 / MSVC 19.51でBridge targetをビルドし、個別suiteとCTest全10 suite成功。

## 進捗 2026-10-09 — log / camera curve HDR headroom round trips

- `ColorBridgeTest` にACEScct、DaVinci Intermediate、S-Log3、Cineon、Canon Log 2/3それぞれでscene-linear 1 / 2 / 4 / 16のencode/decode往復を追加し、finiteと誤差2e-3以内を確認した。
- Visual Studio 2026 / MSVC 19.51でBridge targetをビルドし、個別suiteとCTest全10 suite成功。

## 進捗 2026-10-09 — log / camera curves across scene and HDR range

- `ColorBridgeTest` にACEScct、DaVinci Intermediate、S-Log3、Cineon、Canon Log 2/3を0.001〜4.0のscene-linear/HDR sampleで走査し、finiteかつ非減少であることを確認するケースを追加した。
- 初期探索ではDaVinci Intermediateの0と微小正値（0.0001）間で出力順序が崩れる現行挙動を検出した。今回のケースは代表値テストと同じ0.001以上を対象にし、別Insightへ記録した。
- Visual Studio 2026 / MSVC 19.51でBridge targetをビルドし、個別suiteとCTest全10 suite成功。

## 進捗 2026-10-09 — unknown transfer dispatch fallback

- `ColorBridgeTest` にenum外の`TransferFunction`をdispatchした場合、encode/decodeとも有限サンプル値をidentityで返すケースを追加した。
- Visual Studio 2026 / MSVC 19.51でBridge targetをビルドし、個別suiteとCTest全10 suite成功。

## 進捗 2026-10-09 — PQ / HLG black-white endpoints

- `ColorBridgeTest` にPQ／HLGのencode/decode双方で非正入力からblackへの写像と、linear/code 1のwhite endpointを個別確認するケースを追加した。
- Visual Studio 2026 / MSVC 19.51でBridge targetをビルドし、個別suiteとCTest全10 suite成功。

## 進捗 2026-10-09 — PQ / HLG absolute reference values

- `ColorBridgeTest` にPQ encodeのlinear 0.01/0.18/1.0、PQ decodeのcode 0.5、HLG encodeのlinear 0.5/1.0、HLG decodeのcode 0.5/1.0を既知値で確認するケースを追加した。
- Visual Studio 2026 / MSVC 19.51でBridge targetをビルドし、個別suiteとCTest全10 suite成功。

## 進捗 2026-10-09 — unknown GammaFunction fallback

- 未定義`GammaFunction`値で有限値をそのまま返すencode/decode fallbackと、NaN・infinityを事前sanitizeして0を返す動作を追加検査した。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetをビルドし、CTest全10 suite成功。

## 進捗 2026-10-09 — ACES HDR ordering and highlight saturation

- `ColorSpaceTest` に+8 stopのHDR入力3段階の近似出力値・順序・[0,1]範囲、および+16 stopで全チャンネルが1へ飽和するケースを追加した。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetをビルドし、CTest全10 suite成功。

## 進捗 2026-10-09 — ACES exposure-floor signal retention

- `ColorSpaceTest` にexposure -16で異なる大きさのscene-linear RGBを通した期待値と、-100が同じfloor結果になることを追加した。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetをビルドし、CTest全10 suite成功。

## 進捗 2026-10-09 — ACES lower exposure limit and toe clamp

- `ColorSpaceTest` に露出-100と下限-16の同値、 fitted curveの暗部で負になる領域が表示出力で0 clampされることを確認するケースを追加した。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetをビルドし、CTest全10 suite成功。

## 進捗 2026-10-09 — ACES per-channel isolation

- `ColorSpaceTest` に異なるR/G/B入力のfitted curve参照値と、各channelへNaN・負値・+infinityを個別に与えた際、該当channelだけが0になり他channelの結果を維持するケースを追加した。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetをビルドし、CTest全10 suite成功。

## 進捗 2026-10-09 — ACES display curve references and exposure

- `ColorSpaceTest` に fitted curve の0.18 / 1 / 4 scene-linear reference値と、+1 stopと入力2倍の等価性、非有限露出の0扱い、露出+16上限clampを検査する2ケースを追加した。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetをビルドし、CTest全10 suite成功。

## 進捗 2026-10-09 — signed sRGB and non-finite transfer inputs

- `ColorSpaceTest` に負のscene-linear値でのsRGB encode/decode往復と、7 transfer関数すべてのNaN／正無限大に対するencode/decode結果を追加した。PQ encodeは入力sanitize後に規格式を評価するため、NaN・無限大で`pow(c1,m2)`となる現行結果を明示的に期待値化した。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetをビルドし、CTest全10 suite成功。

## 進捗 2026-10-09 — unknown ColorSpace metadata fallback

- 未定義`ColorSpace` enum値に対する白色点X/Yとgamma exponentのsRGB fallbackを個別ケースで固定した。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetをビルドし、CTest全10 suite成功。

## 進捗 2026-10-09 — ColorSpace metadata table coverage

- `ColorSpaceTest` に7種類のColorSpaceすべてについて、白色点X/Yと既定gamma exponentを表駆動で確認するケースを追加した。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetをビルドし、CTest全10 suite成功。

## 進捗 2026-10-09 — HLG piecewise boundary and PQ reference

- `ColorSpaceTest` に HLG encode のscene-linear `1/12`前後、decode encoded `0.5`境界の分岐値と、PQ 18% luminance のST 2084 reference（約`0.81594`）を確認する2ケースを追加した。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetをビルドし、CTest全10 suite成功。

## 進捗 2026-10-09 — LUT file dispatch edge coverage

- `ColorLUTContractTest` に大文字拡張子`.CUBE`のcase-insensitive dispatch、未対応`.look`拡張子の失敗診断、サイズヘッダーなし3DLの立方サイズ推定を3ケース追加した。
- Visual Studio 2026 / MSVC 19.51でColorLUT targetとCTest全9 suiteが成功。

## 進捗 2026-10-09 — LUT composition order and inverse approximation

- `ColorLUTContractTest` に非可換なaffine red curveを使ったLUT合成順の期待値ケースと、単調縮小linear curveでsource→inverseを通す3 interior sampleケースを追加した。
- inverse誤差はmapped output domain内で各RGB channel 0.01以内。ColorLUT target buildとCTest全9 suiteが成功。

## 進捗 2026-10-09 — ColorLuminance property and broadcast-boundary cases

- `ColorLuminanceContractTest` に全5標準の黒/白端点、RGB primary係数、channel単調性、broadcast legal endpoints、NaNでの両違反判定を5ケース追加した。
- Visual Studio 2026 / MSVC 19.51でColorLuminance targetをビルドし、CTest全9 suite成功。

## 進捗 2026-10-09 — transfer curve round-trip coverage

- Bridge suiteに15種類のdispatch済みTransferFunctionそれぞれの5 luminance値round-trip、Rec.709/2020の異なるtoe slope、ACEScc/ACEScct log blackと主要log curveの有限性確認を3ケース追加した。
- Visual Studio 2026 / MSVC 19.51でBridge targetをビルドし、CTest全9 suite成功。Bridge suiteは21 cases。
- 検証でACESccのzero special encodingをdecoderへ戻すと131072になる不一致を確認。テストはこの誤った期待値を固定せず、ACESccの正値round-tripのみを保証する。実装修正は別範囲。

## 進捗 2026-10-09 — all color-space matrix-pair round trips

- `ColorSpaceTest`に7×7の全ColorSpace変換ペアを通すmatrix契約を追加した。各ペアで7入力（黒・白・RGB原色・混色・HDR/負channel）を往復し、有限な行列値、4x4 affine alpha element、RGB復元誤差1e-3以内を確認する。
- Visual Studio 2026 / MSVC 19.51でColorSpace targetとCTest全9 suite成功。

## 進捗 2026-10-09 — HSV/HSL conversion grid coverage

- `ColorConversionTest` にdense unit RGB cube 1,331点を通すHSV/HSL往復ケースと、HSL primary/secondary hue sector 7境界の期待値検査を追加した。
- Visual Studio 2026 / MSVC 19.51でColorConversion targetをビルドし、CTest全9 suite成功。grid各点はHSV/HSL双方でRGB各channel誤差`1e-5`以内。

## 進捗 2026-10-09 — FloatRGBA arithmetic and conversion contracts

- 既存の`ArtifactCoreColorBridgeTest`にFloatRGBAの四成分加算・乗算・スカラー積、alpha込みlerpとtの端点clamp、FloatColor変換の3ケースを追加した。`FloatRGBA.cppm`実装もstandalone libraryの依存closureへ登録した。
- Visual Studio 2026 / MSVC 19.51でBridge targetをビルドし、全9 suite成功。Bridge suiteは18 cases。

## 進捗 2026-10-09 — ACES transform standalone suite

- `ColorACESContractTest.cpp` を独立CTest target `ArtifactCoreColorACESTest` として追加した。既存のColorSpace / gamut closureに`Color.TransferFunction`と`Color.ACES` interfaceを加えて再利用し、ArtifactCore全体はリンクしない。gamut predicateの各channel負値拒否を含む10ケース。
- encoded sRGBとlinear sRGB入力の一致、入力黒、黒と中間値の出力有限性、soft clip、ACES scene-linear gamut predicateを5ケースで確認する。
- Visual Studio 2026 / MSVC 19.51でtargetをビルドし、全9 suite成功。出力transformはコード内の簡易RRT/filmic近似を検査し、Academy ACES reference transformとのparityは主張しない。

## 進捗 2026-10-09 — Color Harmonizer standalone suite

- `ColorHarmonizerContractTest.cpp` を8つ目の独立CTest target `ArtifactCoreColorHarmonizerTest` として追加した。Qt Core、`Color.Float`、`Color.Conversion`、`Color.Harmonizer`のみを利用し、Artifact / 全ArtifactCoreへ依存しない。
- complementary / analogous / triadic / tetradic / split-complementary / monochromaticの返却数・色相角・alpha保持を5ケースで確認する。
- Visual Studio 2026 / MSVC 19.51で個別targetをビルドし、全8 suite成功。Monochromaticは値が黒へ折り返すとHueが定義できないため、色相保持ケースでは黒点に達しないcountを用いた。

## 進捗 2026-10-09 — Color Luminance standalone suite

- `ColorLuminanceContractTest.cpp` を7つ目の独立CTest target `ArtifactCoreColorLuminanceTest` として追加した。単一 `Color.Luminance` moduleだけをビルドし、QtやArtifactCore全体に依存しない。
- Rec.601/709/2020、Display P3、ACES AP1の重み、perceptual brightness、grayscale、broadcast legal-range検査、非有限値のchannel clampを検査する6ケースを追加した。
- Visual Studio 2026 / MSVC 19.51でsuiteをビルドし、CTest全7 suite成功。

## 進捗 2026-10-09 — Color Blend Mode standalone suite

- `ColorBlendModeStandaloneTest.cpp` を6つ目の独立CTest target `ArtifactCoreColorBlendModeTest` として追加した。対象は `Color.BlendMode`、`Color.Float`、RGB/HSL conversion、luminance、transfer interfaceと各 production implementationだけで、Artifactアプリや全ArtifactCoreには依存しない。
- opacityゼロの恒等、opaque Normal、Multiplyのチャンネル積、transparent base上のsource alpha、端点での算術mode出力の有限性・[0,1]範囲を5ケース追加した。
- Visual Studio 2026 / MSVC 19.51でsuiteをビルドし、CTest全6 suite成功。blend suite 5 cases。

## 進捗 2026-10-09 — Color Blend Mode opacity・HSL contracts

- `ColorBlendModeStandaloneTest` に半透明baseを含むNormal opacity格子、Hue/Saturation/Color/Luminosityの成分置換、Stencil/Silhouetteのbase alpha変換を3ケース追加した。
- BlendMode targetを再ビルドして個別CTestを実行し、全9 color suiteも成功。

## 進捗 2026-10-09 — Color Blend Mode enum grid

- 34種類すべての宣言modeをRGB値・opacityの格子で走査し、RGBA出力の有限性と[0,1]範囲を確認するケースを追加。ClassicColorBurn / LinearDodge / ClassicColorDodge / ClassicDifference の4 aliasはcanonical modeとの一致を検査する。
- Visual Studio 2026 / MSVC 19.51でBlendMode targetをビルドし、単独CTestと全9 suiteが成功。

## 進捗 2026-10-09 — ImageSurfaceView standalone suite

- 既存の`ImageSurfaceViewTest.cpp`を独立CTest target `ArtifactCoreImageSurfaceViewTest` として登録した。依存は`Graphics.SurfaceColorContract`と`Image.ImageSurfaceView`のみで、全ArtifactCoreやQtに依存しない。
- BGRA viewにRGBA descriptorを渡した場合の拒否と、read-only viewが所有側bufferの変更をそのまま観測する非所有契約を2ケース追加。Visual Studio 2026 / MSVC 19.51で個別suiteと全10 suiteが成功。

## 進捗 2026-10-09 — Surface pixel conversion boundary grid

- `SurfacePixelConversionTest` にbyte sRGBからlinear floatへのdecode、premultiplied alpha epsilonの直下/直上、linear float出力での負値/HDR保持を3ケース追加した。
- Visual Studio 2026 / MSVC 19.51でpixel conversion単独suiteとstandalone全10 suiteが成功。

## 進捗 2026-10-09 — FloatRGBA operator edges

- Bridge suiteにRGBA全成分のcompound加減乗除・scalar演算・swap、添字の有効範囲と例外、default constructorとRGB constructorのalpha初期値を3ケース追加した。
- 小数演算は各channelを許容誤差で比較。Visual Studio 2026 / MSVC 19.51でBridge targetとstandalone全10 suiteが成功。

## 進捗 2026-10-09 — ColorBridge serialization boundaries

- Bridge suiteにFloatRGBA JSONの4成分保存・復元、空白付き`#RRGGBB`文字列のtrimとopaque alpha既定、無効QColorからFloatColor / FloatRGBAへの異なるfallbackを3ケース追加した。
- Visual Studio 2026 / MSVC 19.51でBridge targetとstandalone全10 suiteが成功。

## 進捗 2026-10-09 — ImageSurfaceView padded-row write coverage

- 2×2 RGBA viewをrowごとに1 float padding付きで作り、mutable dispatch経由の2行目アクセス・pixel channel mapping・row padding不変を1ケースで確認した。
- Visual Studio 2026 / MSVC 19.51でImageSurfaceView単独suiteとstandalone全10 suiteが成功。

## 進捗 2026-10-09 — HSV/HSL deterministic property samples

- `ColorConversionTest` に固定seed LCGで生成する4,096 RGB sampleを追加。HSV/HSL成分の有限性・定義域とRGB往復誤差`1e-5`以内を確認する。
- Visual Studio 2026 / MSVC 19.51でColorConversion単独suiteとstandalone全10 suiteが成功。seedと生成式を固定し、失敗を再現可能にした。

## 進捗 2026-10-09 — ColorSpace matrix alpha axis

- 全7×7 ColorSpace matrix pairでRGB/alphaの交差項とalpha行の恒等値を検査し、代表RGBAのalpha適用結果がsource alphaを保持するassertionを追加した。
- Visual Studio 2026 / MSVC 19.51でColorSpace単独suiteとstandalone全10 suiteが成功。

## 進捗 2026-10-09 — Luminance neutral and clamp properties

- 5 standard×5 neutral-gray値でgrayscale保持を確認し、HSP perceptual brightnessのneutral axisも検査。broadcast clampには有限性・legal bounds・二度適用時の冪等性とNaN/±Inf入力を追加した。
- Visual Studio 2026 / MSVC 19.51でColorLuminance単独suiteとstandalone全10 suiteが成功。

## 進捗 2026-10-09 — TaggedColor transfer / primaries composition

- Bridge suiteにdispatch済みtransfer 16種類のTaggedColor encode/decode往復と、Gamma22 encoded値のRec.709→Rec.2020→Rec.709往復を2ケース追加した。alpha、alphaMode、primaries/transfer metadataの保持も検査する。
- Visual Studio 2026 / MSVC 19.51でBridge単独suiteとstandalone全10 suiteが成功。未dispatchのACESlogは対象から除外。

## 進捗 2026-10-09 — LUT tiled image application

- `ColorLUTContractTest` に37×35 QImageの全pixelを調べるケースを追加。32×32 tile境界を越えてred inversion、green/blue保持、全pixel alpha保持、source不変を確認する。
- Visual Studio 2026 / MSVC 19.51でLUT単独suiteとstandalone全10 suiteが成功。

## 進捗 2026-10-09 — LUT float apply invalid/nonfinite inputs

- `ColorLUTContractTest` に非有限RGB入力をzeroへsanitizeしてsamplingするケースと、不正CUBE file由来のinvalid LUTがfloat入力を変更しないケースを2件追加した。
- default `ColorLUT` は有効なidentity LUTを構築するため、invalid caseは明示的に不完全なファイルをfixtureに使う。MSVCでLUT単独suiteと全10 suiteが成功。

## 進捗 2026-10-09 — TaggedColor alpha conversion boundaries

- `ColorBridgeTest` にpremultiplied alpha `0` / `1e-6` / `nextafter(1e-6,+inf)`でstraight変換が黒化/復元を切り替える境界ケースと、Opaque alpha modeで両変換がno-opとなるケースを2件追加した。
- Visual Studio 2026 / MSVC 19.51でBridge単独suiteとstandalone全10 suiteが成功。

## 進捗 2026-10-09 — LUT QColor intensity interpolation grid

- `applyWithIntensity`を0、0.25、0.5、0.75、1.0で検査し、RGBの線形補間とalpha不変を確認するケースを追加した。
- Visual Studio 2026 / MSVC 19.51でLUT単独suiteとstandalone全10 suiteが成功。

## 進捗 2026-10-09 — Harmonizer hue-wrap and channel invariants

- Complementary / analogous / triadic / split-complementary / tetradicの5方式をHue 350°の色で検査し、0°跨ぎ後もSaturation、Value、alphaを保持することを追加確認。Monochromaticの負countはemptyとなることも検査する。
- Visual Studio 2026 / MSVC 19.51でHarmonizer単独suiteとstandalone全10 suiteが成功。

## 進捗 2026-10-09 — ColorSpace transfer normalized grids

- Linear / sRGB / Gamma22 / Gamma24 / Gamma26 / PQ / HLGの7 dispatch branchを0〜1の257点で走査し、finite・[0,1] bounds・単調性・encode/decode往復を検査するケースを追加した。
- PQの観測最大往復差は約`7.9e-5`だったため許容誤差を`1e-4`に設定。Visual Studio 2026 / MSVC 19.51でColorSpace単独suiteと全10 suiteが成功。

## 進捗 2026-10-09 — BlendMode opacity endpoints across enum

- 34 declared modes×RGB格子でopacity `-0.5` / `0` / interior / `1` / `1.5`を走査し、opacity≤0はbaseに完全一致、opacity≥1はopacity=1結果に一致するassertionを追加した。
- Visual Studio 2026 / MSVC 19.51でBlendMode単独suiteと全10 suiteが成功。

## 進捗 2026-10-09 — Color LUT standalone suite

- `ColorLUTContractTest.cpp` を5つ目の独立CTest target `ArtifactCoreColorLUTTest` として登録した。対象は `Color.LUT` と直接必要な `Core.Parallel`、`Container.NamedVector` のmodule閉包のみで、Artifactアプリ、全ArtifactCore、OpenImageIOには依存しない。
- 既存テストが非公開 `ColorLUT::sample()` を呼んでいた6箇所を公開 `apply()` 経由へ変更した。`inverted()` は強い単調性・一対一性がない定数写像を一般逆関数として期待できないため、既存テストを「有効で有限かつ範囲内の近似LUTを返す」契約に修正した。Color LUTの逆変換品質改善は実装変更を含む別作業として扱う。
- Visual Studio 2026 / MSVC 19.51でColor LUT targetをビルドし、5 suiteすべてのCTest成功（5/5）。LUT suiteは25 cases。

## 進捗 2026-10-09

- 既存アプリの UI を通常版とは別プロセスで開く `ArtifactUiTest.exe` target を追加した。画面実装と依存 runtime は `ArtifactAppRuntime` object library として一度だけコンパイルし、`Artifact.exe` と UI Test exe は別 entry point から同じ実画面を起動する。UI Test 起動では Qt の test-mode 標準書込先と crash report path を分け、ウィンドウタイトルも識別する。明示した organization/application を使う Windows `QSettings` は通常版と共有されるため、設定全体の隔離ではない。ビルド・起動確認は未実施。
- `tools/capture_ui_test.py` は UI Test exe を起動し、`ARTIFACT_STARTUP_SCREENSHOT_PATH` で指定した PNG が保存されるまで待ってからプロセスを終了する。既存 `ArtifactStartupScreenshot` の `window->grab()` を再利用し、capture helper に外部 Python package は追加しない。`tools/ui_visual_loop.py --capture-command` から `{actual}` を出力先として渡せる。exe のビルド・起動確認は未実施。
- この exe は実 UI の比較・調整用 host であり、unit / interaction test target の代替ではない。通常 capture は起動時の main window を撮影する。`tools/capture_ui_test.py --timeline` は UI Test 専用の一時プロジェクトに 5 枚の Solid レイヤーを作り、他の dock を隠して Timeline dock を広く表示する。capture runner が一時プロジェクト領域を作成・破棄し、`ArtifactProjectManager` は UI Test プロセスで指定された領域だけを使う。mockup baseline と environment manifest は引き続き未整備。

## 進捗 2026-10-10 — Render Manager UI capture fixture

- `tools/capture_ui_test.py --render-manager` を追加し、UI Test process の一時 `APPDATA` / `LOCALAPPDATA` と出力先配下の一時 fixture root を使う。Render Manager の固定キューは attention 状態1件と pending 状態2件で構成し、レンダー処理は開始しない。
- `ArtifactUiTest.exe` 起動時に Render Manager を開き、`renderManagerUiFixtureWidget` をスクリーンショット対象にする経路を追加した。比較ループは既存の `tools/ui_visual_loop.py` にこの capture command を渡して使う。
- Visual Studio 2026 / MSVC 19.51 の既存 Debug build tree で `ArtifactUiTest` をビルドし、`--render-manager` capture が 1800×1125 PNG を生成することを確認した。`ui_visual_loop.py` も仮の同一 fixture capture を reference にpixel-exact比較し、toolbar / queue / details / statusの4領域を含め0 changed pixelsで通過した。これは runner/capture の smoke 比較で、承認済み baseline との合格ではない。
- `python -m unittest discover -s tests/ui_visual -v` を再実行し、現在の comparator / loop runner suite 20件が成功した。Render Manager interaction testではない。
- これは実 UI capture を開始できる fixture であり、承認済み baseline、environment manifest、正式な Visual Regression gate の合格を意味しない。interaction suite と service injection seam も引き続き未実装。

Render Manager の単独 capture は次のコマンドで実行できる（固定 Windows Debug build tree の例）：

```powershell
python tools/capture_ui_test.py `
  --render-manager `
  --exe out/build/x64-Debug-Agent/bin/Debug/ArtifactUiTest.exe `
  --output artifacts/ui/render-manager.png
```

## 進捗 2026-10-10 — 再読込後の編集保存契約

- `ArtifactProjectRoundTripTest.ResaveAfterLoadIsStable` を拡張し、保存→読込後にComposition名とnoteを変更し、別ファイルへ再保存→再読込して両方が保持されることを確認する。既存のbackup分岐・同一path再保存の確認も維持する。テストは未実行。

## 進捗 2026-10-10 — 静止画レイヤー往復契約

- 連番設定テストを追加。フレームpathのtrim／重複除去、空入力時に既存設定を変えないこと、frame rateの下限・上限clampとNaN／Infの無効化、JSON復元時の空白・重複・非文字列要素除去とframe rate上限clampを独立ケースで確認する。ビルド・実行は未実施。
- SourceCrop snapshotの正常復元と、不正snapshot拒否時に現在状態を保持する2ケースを追加。ビルド・実行は未実施。
- SourceCrop revisionの実変更時だけの更新と、source texture共有可否がcrop／連番状態で切り替わる契約を追加。GPU実体は使わずCPU側のpredicateのみ確認する。ビルド・実行は未実施。
- maskをlayerへ追加するとsource texture共有が無効になり、削除すると再び有効になるpredicate条件を確認。GPU実体は使わない。ビルド・実行は未実施。
- `setFromImageBuffer`の有効buffer置換時にfile asset identity／連番状態が解除されることと、空bufferでframe stateがclearされることを検査する2ケースを追加。ビルド・実行は未実施。
- SourceCrop layoutの回転行列で指定anchor pivotが固定され、0度でidentity matrixへ戻る契約をCPUテストに追加。ビルド・実行は未実施。
- property setterの未知path／読み取り専用path／負値・NaNのframe rate拒否と、拒否時の状態不変を確認するケースを追加。ビルド・実行は未実施。
- SourceCrop panの有限範囲clamp／NaN・Inf無視と、crop rectangleのsource右端・下端clampを2ケースで確認する。ビルド・実行は未実施。
- SourceCrop anchor両軸がNaN／Inf更新を保持状態のまま無視し、有限値は0〜1へclampされることを追加確認。ビルド・実行は未実施。
- active SourceCropをlayer JSONから復元し、復元後も元画像の指定画素選択とcrop外の透明化が一致することを追加確認する。ビルド・実行は未実施。
- SourceCrop layoutでpreserve aspect有効時のletterbox出力矩形と、無効時のlayer frame全体への配置を確認する。ビルド・実行は未実施。
- SourceCropのzoom縮小とpan移動を組み合わせ、anchor基準で算出されるeffective source rectangleの位置・寸法をCPU契約テストで確認。ビルド・実行は未実施。
- Active SourceCropのpan／zoom後、CPU画像のcrop矩形内だけが元画素となり、それ以外が透明となる統合ケースを追加。ビルド・実行は未実施。
- crop／pan／zoomを変更してもImageLayerのlocalBoundsが不変で、レイヤーフレームが維持される契約を確認。ビルド・実行は未実施。
- 一度生成したCPU crop出力の後にcrop位置を変更すると選択画素が更新され、crop無効化で元画像へ戻るcache revision契約を追加。ビルド・実行は未実施。
- Temporary source overrideが有効なときlayer source resolverがoverride bufferを返し、空overrideでは本来のsource bufferへ戻ることを確認。ビルド・実行は未実施。
- OpenCV `cv::Mat`入力のBGR→RGB pixel変換と空Matによる既存画像解除を独立2ケースで確認する。ビルド・実行は未実施。
- `ArtifactImageLayer::setFromCvMat` が現状の `CvUtils::cvMatToQImage` 対応外である `CV_16UC1` / `CV_16UC3` を受けた場合、bufferを作らずplaceholderを返すことを2ケースで確認。OpenCV BGRの非連続ROIがrow strideを保って各画素を変換するケースも追加。ビルド・実行は未実施。
- `ImageF32x4_RGBA::setFromCVMat`のCore直通経路では`CV_16UC1` / `CV_16UC3`を正規化し、RGBA floatへ展開／reorderすることを別の2ケースで確認。ArtifactImageLayerのQImage bridgeとは別契約として扱う。ビルド・実行は未実施。
- 32-bit float BGRAの非連続OpenCV ROIでもrow strideに従い、RGB順とalphaを維持するケースを追加。ビルド・実行は未実施。
- `Format_ARGB32_Premultiplied` の半透明QImageを直接取り込み、CPU出力でstraight RGBとalphaを復元するケースを追加。ビルド・実行は未実施。
- `setFromImageBuffer`後に呼び出し側バッファを再設定しても、レイヤーが元のRGBA32F画素を保持するDeepCopy契約を追加確認。ビルド・実行は未実施。
- ImageBuffer入力のJSON metadataが4-channel RGBA、alpha index 3、32-bit/channelとして記録されることを確認。ビルド・実行は未実施。
- `setFromQImage`後に呼び出し側QImageをfillで変更しても、レイヤー内の元画素が独立して保持されることを確認。ビルド・実行は未実施。
- alphaなし `Format_RGB888` を直接設定したとき、CPU出力をopaqueに保ち、source metadataではalpha channelを持たないことを確認。ビルド・実行は未実施。
- DepthMap bilinear samplingの幅1・高さ1・単一画素の退化寸法で、補間値と端点clampが安定することを2ケースで確認。ビルド・実行は未実施。
- 2×2 DepthMapの非対称off-center座標で、bilinearのX／Y重みを適用した期待値を手計算と比較。ビルド・実行は未実施。
- DepthMap bilinear samplingへ正負Infinityの座標を渡した場合に、各端の画素値へclampすることを追加確認。ビルド・実行は未実施。
- 空segmentation maskの適用拒否時に、元画像の全画素・alphaが不変であることを独立確認。ビルド・実行は未実施。
- Core segmentation maskのthreshold／softnessからalpha 0・1を生成し、ReplaceAlphaがRGBを変えない契約を追加確認。ビルド・実行は未実施。
- Core segmentation maskのinvert後にopacity 0.5で既存alphaへMultiplyAlpha適用し、RGBを保持する計算を別ケースで確認。ビルド・実行は未実施。
- Segmentation helperのcoverage／confidence／padded bounds／acceptance、4種mask combine代数、異解像度resample、非破壊invert previewを4ケース追加。ビルド・実行は未実施。
- Segmentationのhole fill／small component除去、異解像度temporal stabilization、frame差分統計のnormalized resamplingを4ケース追加。ビルド・実行は未実施。
- DepthMapのresizeで幅または高さが負の場合に、以前のデータを残さずemptyへ戻る契約を追加確認。ビルド・実行は未実施。
- `setDepthMap`後にcaller側mapを変更しても、mesh options再適用時の頂点Z値がlayer保持map由来であることを確認。ビルド・実行は未実施。
- CPU DepthMesh生成の頂点座標でwidth／heightの範囲、depthScale、invertDepthの符号を直接検証。ビルド・実行は未実施。
- DepthMesh generatorへ負／ゼロのgrid resolutionを渡した際、最低2×2の頂点とquadを生成する契約を確認。ビルド・実行は未実施。
- 画像シーケンスをfps=0でレイヤーフレーム移動し、要求frameの画像表示と範囲外の先頭／末尾clampを実画像で確認。ビルド・実行は未実施。
- 静止画sourceのasset versionをinvalidateした後、同じpath上の置換済みPNGを再decodeして新しい画素へ切り替える契約を確認。ビルド・実行は未実施。
- 欠落画像を含むシーケンスでそのframeへ移動した際、前frameのstale画素を残さずmissing placeholderへ切り替える契約を確認。ビルド・実行は未実施。
- 一度decodeしたシーケンスframeを同じpath上で破損payloadに置き換えた後、同じindexへ戻ると古いdecode cacheを使わずplaceholderへ切り替えることを追加確認。成功frameはcache indexをpollして非同期decode完了後に比較する。ビルド・実行は未実施。
- float `cv::Mat`のNaN→0置換、0〜1 clamp、BGR→RGB変換を追加確認する。ビルド・実行は未実施。
- float grayscale `cv::Mat`のRGB展開・opaque alphaとfloat BGRAのchannel順・alpha保持もテストする。ビルド・実行は未実施。
- 8-bit grayscaleのRGB展開・opaque alphaと8-bit BGRAのchannel順・alpha保持も検査し、CvUtilsの全対応型分岐をカバーする。ビルド・実行は未実施。
- 幅上限を超えたin-memory QImage／float buffer入力を拒否し、既存画素を保持する2ケースを追加する。ビルド・実行は未実施。

- `ArtifactStillImageLayerRoundTripTest` を専用実行ファイル／CTest targetとして追加。一時PNGから静止画レイヤーを作り、保存・再読込後のsource path、RGBA画素、layer名、表示状態、opacity、fit mode、2D position/rotation/scale、inactive source-crop設定の保持を確認する。thumbnailのtarget内aspect維持・最小寸法clamp・missing placeholder縮小、temporary source overrideの選択・空override／解除時の元buffer復帰も検査する。別ケースでactive crop領域外を透明化するCPU画像出力と色別source pixelの選択一致、空・有効depth mapのmesh生成とclear後の2D状態復帰、失敗depth map relink時の既存path／mesh保持、DepthMapのgrayscale PNG読込・正規化値・失敗読込時clear、範囲外write無視・値／座標clamp・bilinear補間・無効resize解除、PSD subimage indexとdepth mesh optionsのJSON復元・境界clamp、segmentation maskの空入力拒否とopacity適用時のRGB維持・alpha補間・opacity境界clamp・異寸法maskのbilinear再サンプル、欠落sourceからのplaceholder移行、空sourceによる画像状態解除、破損payload時のpath保持、missing source pathのプロジェクト保存・再読込、source identityのlocalize/relink往復と無効操作拒否、crop propertyのNaN/Inf・寸法境界のsanitize、16-bit grayscale PNGの隣接階調保持・RGB展開・opaque alpha、2-channel gray+alpha PNGのRGB展開・alpha保持、3-channel RGB PNGのRGB画素保持・opaque alpha補完、画像寸法上限とdecoded pixel budget超過をsource path付きFallbackTracker診断で拒否すること、pixel aspectによる表示幅とfit mode時の正方ピクセル表示、入力色解釈文字列のtrim・長さ上限・JSON復元、missing source時の保存metadataに対するorientation・channel/alpha・pixel aspect・bit depth・ICC byte count・文字列・channel name配列のsanitizeを検査する。読み込みはバックグラウンド先読み完了をQt event processingで最大5秒待つ。GPU描画と `Artifact.exe` の起動には依存しないが、Artifact production moduleのため `ArtifactAppRuntime` をリンクする。ビルド・実行は未実施。

- 10月8日ブランチの追加分として、Asset database/import/metadata/sequence、Image Analyzer、Color LUT、Core diagnostics、render pipeline / pointwise fusion、Artifact color effects の独立 contract suite を登録した。ArtifactScript composition host API のテストも拡張した。これらの追加テストは今回実行していない。

## 進捗 2026-10-09 — Timeline 単体表示

- `ArtifactUiTest` に `ARTIFACT_UI_TEST_TIMELINE` 起動経路を追加し、通常アプリや標準 UI Test 起動には影響させず、Fixture 専用プロジェクトと実際の Timeline dock を表示する。
- キャプチャ例: `python tools/capture_ui_test.py --timeline --exe out/build/x64-Debug-Agent/bin/Debug/ArtifactUiTest.exe --output temp/timeline-ui.png`
- runner は一時 project-root を作成し、プロセス終了後に同 root を削除する。Timeline fixture はレイヤー構成と表示比較用で、interaction automation や承認済み baseline は含まない。

## 進捗 2026-10-09 — 独立カラー・モジュールテスト

- `tests/ArtifactCore/StandaloneTestHelpers.cmake` に、C++ module interface / implementation / GoogleTest executable 用の共通helperを追加した。standalone projectはトップレベルのArtifactアプリや完全な`ArtifactCore` libraryをロードせず、対象モジュールの依存閉包だけを列挙する。
- `tests/ArtifactCore/ColorSpaceStandalone` は既存の色域・transfer functionテストに加え、同じ実行可能なstandalone projectで`Color.Conversion`のRGB↔HSV/HSLテストも個別CTest targetとして選べるようにした。各suiteは別々にビルド・実行でき、共通のGoogleTest link helperを使う。
- Visual Studio 2026 / MSVC 19.51、vcpkg Qt 6 / GoogleTest環境でconfigure、2 target build、CTestを実行し2/2 suite成功。Artifact.exeと全ArtifactCoreは起動・リンクしていない。
- 受入れ時に、外部module file setには`BASE_DIRS`が必要なこと、`.cppm`はMSVC/CMakeで実装unitも`/interface`扱いとなり実装コードの宣言解決に失敗することを確認した。helperは`.cppm`をconfigure時にビルド領域内の`.cpp`へコピーして実装unitとしてコンパイルする。production sourceは唯一の編集元であり、変更は次回configureでコピーへ反映される。module interface targetはimplementation libraryの依存としてlinkし、MSVCが供給するBMI参照を重複指定しない。
- `PqAndHlgClampOutOfRangeValues` はPQ blackのOETF値が約`7.31e-7`であるため、出力値0固定ではなく、clamp前の範囲外入力と対応するendpoint入力が一致する契約に修正した。これはArtifactCore実装を変更せず、テスト期待値を規格式・現行処理に合わせた。
- `ColorBridgeTest.cpp`も同じstandalone projectで個別target化した。ColorBridge、FloatColor/FloatRGBA、TaggedColor、transfer/gamut、surface color contractのmodule閉包だけをビルドする。Visual Studio 2026 / MSVC 19.51で全3 targetをビルドしCTest 3/3成功。Bridge suiteは15 cases。QColorが必要なためBridgeの依存だけQt Guiまで加えた。
- QColor文字列は8bit/channelなのでfloat入力を完全な精度で往復しない。hex経由の期待をround-trip量子化値（26/255、102/255）に修正し、JSON float object経路の高精度検証と分けた。
- `SurfacePixelConversionTest.cpp`を4つ目のCTest target `ArtifactCoreSurfacePixelConversionTest`として追加した。`Color.TransferFunction`、`Graphics.SurfaceColorContract`、`Image.SurfacePixelConversion`の公開moduleだけを登録し、Qtやアプリをリンクせずにpixel-buffer変換契約を検査する。Visual Studio 2026 / MSVC 19.51でtarget buildとCTestを実行し、全4 suite成功。

## 進捗 2026-10-08

- 独立テスト選択を追加した。`ARTIFACT_ENABLE_LAYER_TEST` は Adjustment / Group / Shape / Solid を個別CTest項目として登録する（各テストはArtifact実行体のテストモードを使う）。`AppMain` は各環境変数から対応する1種類のrunnerだけを呼び出して終了し、通常の `runAllTests()` へ進まない。`ARTIFACT_ENABLE_OFFLINE_RENDER_TEST` は単体レンダラーのclear/readback、矩形描画/readback契約とCompositionのオフスクリーン描画を登録する。`ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS` はCore creative effects、Exposure、Color Correction、Aperture Shape Blurを個別GTest targetとして選択できる。Blur契約はRGBの近傍拡散、alpha保持、入力不変、寸法を確認する。CMake configure・ビルド・テストは未実施。

独立テストをまとめて登録するには、GTest対応のビルド環境で次を使う。

```powershell
cmake -S . -B build-independent-tests `
  -DARTIFACT_ENABLE_LAYER_TEST=ON `
  -DARTIFACT_ENABLE_OFFLINE_RENDER_TEST=ON `
  -DARTIFACT_ENABLE_IMAGE_EFFECT_TESTS=ON
cmake --build build-independent-tests --config Debug --target Artifact ArtifactCoreCreativeEffectTest ArtifactExposureEffectContractTest ArtifactColorCorrectionEffectContractTest ArtifactSpatialImageEffectContractTest ArtifactOfflineRendererContractTest
ctest --test-dir build-independent-tests -C Debug --output-on-failure -L image-effect
ctest --test-dir build-independent-tests -C Debug --output-on-failure -R "Artifact(AdjustmentLayer|LayerGroup|ShapeLayer|SolidLayer)Test"
ctest --test-dir build-independent-tests -C Debug --output-on-failure -R "ArtifactOffline(Render|RendererContract)Test"
```

レイヤーテストとComposition描画統合テストは `Artifact.exe` のテストモードを起動する。`ArtifactOfflineRendererContractTest` と画像エフェクトは専用テスト実行ファイルで実行する。ここに記載したコマンドは案内用で、今回実行していない。

## 進捗 2026-10-07

- オブジェクト経由のユーザーメソッドdispatchに、call-site／実行時class／definition世代を使う評価器内キャッシュを追加した。32 methodを持つclassで同じmethodをhook内16回呼ぶMSVC Debug fixtureは、キャッシュ前110.19、キャッシュ後85.03 µs/hookだった。続けて `callInstanceMethod` が毎回オブジェクト全field mapを複製していたため、既存のfield overlayで読み書き分だけ保持し、成功時のみ親mapへcommitするようにした。これにより同fixtureは初回71.97、再計測66.59 µs/hookとなり、64 allocation / hookから0 allocation / hookへ変化した。失敗時のscalar field rollback、8件を超えるfield overlayのcommit、1 call-siteへ基底／派生instanceを順に渡すdispatchを回帰テスト化した。ArtifactScript関連5 suiteはすべて成功。測定は単一MSVC Debugの合成fixtureであり、Releaseや実scriptの性能保証ではない。

- field overlayの固定容量を4から8 bindingへ拡張した。5-field object methodをhook内16回呼ぶfixtureは16 allocation / 3584 bytesから0 allocation / hookになったが、Debugの時間は122.95から120.35 µs/hookでほぼ同じだった。各 `ArtifactScriptFields` scopeは4 binding分大きくなり、foreach用の8 workspaceも各4 binding分増える。8件を超えるfieldは従来のoverflowへfallbackし、9-field commitテストで確認する。overlay名の固定hash indexも試したがforeach fixtureの時間が悪化したため採用せず、線形検索を維持した。計測は単一MSVC Debug合成fixtureであり、Releaseの性能保証ではない。

- オブジェクトmethod cacheのslot選択を実行時class名hashへ変え、cache hit時はdefinition所有のclass名を比較してから `findClass` を省くようにした。同じcall-site／classの反復では、1 hook内のclass registry線形検索を初回だけにする。単一class／32 methodsのMSVC Debug fixtureは変更前70.46、変更後69.98 µs/hookで差は計測揺れの範囲、CRT allocationは0のまま。8以上のclass registryでの短縮量は未確認であり、この値から性能向上率は主張しない。base／derived classを同じcall-siteで切り替える契約テストでdispatch結果を確認する。

## 進捗 2026-10-06

- 既存 `ArtifactCoreKeyframeSplineTest` に4ケース追加した。空track、逆順挿入後のsortと範囲外時刻clamp、Constant segmentから次のexact keyへの境界、frame単位のlinear speedを固定する（現12 cases）。別target `ArtifactCoreKeyframePatternGeneratorTest` を追加し、12 preset全種の有限・昇順・frame scale、Ramp/Stagger/Stepの境界、seeded Shakeの再現性、trajectoryの非有限sample除去と等間隔再サンプルを6ケースで検査する。両suiteは共通Core APIの契約であり、Text Animator layerのproperty keyframe保存・読込・seek連携を証明するものではない。
- `tests/ArtifactCore/TextAnimatorContractTest.cpp` に Core contract suite を追加し、percentage ramp endpoints、全6 shapeの境界・中間値、high/low ease、inverted/non-finite range/offset/ease、invalid regex、BMP/astral Unicode source regex（UTF-16 surrogate pair をまたぐ絵文字と複数 shaped glyph のcluster展開を含む）、Percentage/Cluster/Line/Tag/Index+offset domain、expression index/clamp/time/error（評価中エラーで全weightをゼロ化）、各order permutation、logical/visual順、seeded random order、Wigglyの時刻変化・zero-rate freeze・full correlation・異常値安全性、全combine modeとextra weight、combine後の境界clamp、単体および複数animatorのtransform/opacity合成、per-axis scale、tracking、color/stroke/blur/skew/z channel、empty glyph domainを対象にした（現30 cases）。extra weightの非有限値sanitizeは公開契約が未記載で、現実装は `std::clamp(weight * extraWeight, 0, 1)` へ直送するため、NaNが伝播し、+Infは1になる。実装を変更しない前提からテスト期待値には加えず、改善候補として扱う。ArtifactCoreサブモジュールは編集していない。
- `tests/ArtifactCore/CMakeLists.txt` に `ArtifactCoreTextAnimatorContractTest` を登録した。`Artifact.exe` はリンクせず `ArtifactCore` のみを対象にする。
- 既存 `ArtifactCoreCreativeEffectTest` は18種類すべてのCPU creative effectを25ケースで画素検証する。Color Vibranceのneutral RGBA、disabled passthrough、matte alphaとRGB保持、必須RGB channel欠落、有限な範囲外RGBのclampに加え、Posterizeの量子化境界／最小levels、Solarizeのstrict threshold／下限clamp／disabled passthrough／必須channel欠落・alpha非変更を固定する。Fisheyeは3×3 zoom時の参照pixel写像、Mirrorは縦軸で反射する片側、Pixelateは画像端の不完全ブロック平均、Halftoneはdot center／corner値、Kaleidoscopeはcount=1のwedge fold、Chromatic Aberrationはred/blueの逆方向shift、Embossは対角差分・height・clampと端行列の保持を検証し、各空間効果でalpha保持も確認する。Edge Echo／Light Pressure／Old TVの中立設定での完全no-opとalpha保持、Surface Memory／Temporal Fossilの初回frame履歴初期化とpixel保持、Depth Meltの一様gray保持、Glitchの固定時刻再現性／共通RGB grain、Pigment Separationの固定画素式も対象にする。これは `VideoFrame` ベースのCore APIのみであり、`Artifact/src/Effects` の `ImageF32x4_RGBA` layer effect、stack、GPU parityは未対応。
- `ArtifactCoreRenderImageContractTest` はfloat RGBA blendの計算・端点、alpha blendの計算・透明/不透明端点、入力非変更、cropの座標/alpha保持・全域/最終pixel境界・ゼロ幅・範囲外、両blend APIの寸法不一致、weighted blend時の互換しない色記述子unknown化、crop時の完全な色記述子保持を検査する（現13 cases。今回、複数寸法・重み／opacityの固定入力行列でblend式を全pixel検証する2ケースを追加）。Image APIはweight/opacityをclamp/sanitizeしないが、範囲外値の契約は公開API文書から確認できないため、Clampを期待する2つのcaseは外し、設計判断待ちとして残した。crop実装は負width/heightを明示拒否せずOpenCV ROI生成に進むため、危険な入力をテストから直接投げるケースはsuiteに入れず、実装側の入力検証後に追加する。範囲外 crop はAPIコメントに反してdefault画像を返す。alphaBlendは異なるdescriptorを混ぜてもbase descriptorを維持するが、その結果のdescriptor契約は明記されていないため期待値を追加していない。確認したRender Queueの2 callerはcrop前に矩形を画像境界へ交差させており、crop欠陥を呼び出さない。これらの実装変更はArtifactCoreサブモジュールにあるため行っていない。
- `ArtifactCoreRenderJobModelContractTest` を追加し、Render Managerが表示するCore行モデルの4列、status alias、progress clamp/non-finite、無効index/role、frame range/MFR設定の原子的拒否、row追加削除を検査する（8 cases）。永続化の副作用がある`ArtifactRenderQueueService`やWidget bootstrapは生成しないため、これはUI操作テストではない。Widgetのsearch/filter/selection/status更新と状態表示の結合試験は未実装。
- `tests/Artifact/ExposureEffectContractTest.cpp` を追加し、Artifact.exeを起動せず `ArtifactEffectsColor` の Exposure layer effectをCPU固定で画素検証する（10 cases）。EV、offset→gamma、alpha保持、mix/effect-region、pixel centerによる小数境界、mix=0かつregionあり、無効regionの解除、透明pixelでのdisabled完全保持、setterの有限値境界、GPU pointwise descriptorとparameter slotに加え、EV／offset／gamma／mixの3×3×3×3 parameter gridを6画素すべてで式比較する。`tests/Artifact/CMakeLists.txt` から登録する。リンク閉包は `ArtifactEffectsColor` とその依存 `ArtifactRender` 等を含むため、Core-only suite より重い。GPU実計算とのparityは別suiteであり未対応。Artifact側VibranceEffect等の全effect packはまだ対象外。
- `tests/Artifact/ColorCorrectionEffectContractTest.cpp` は `ArtifactEffectsColor` が所有する Invert、Brightness、Grayscale、Channel Mixer、White Balance、Color Balance、Levels、Fill、Curves、Gradient Ramp、Color Wheels、Colorama、Hue/Saturation、Photo Filter、Selective Color、Tritone、Lift/Gamma/Gain、Shadow/HighlightをCPU固定で画素検証する（現42 cases）。無効effectのdeep-copyとdescriptor維持、各effectの代表parameter/alpha契約、Fillのalpha保持切替、Curvesのpremultiplied/透明画素、Gradient Rampの空間補間/alpha切替、Color Wheelsのstraight RGB gamma処理とpremultiplied alpha復元、ColoramaのHue→Rainbow palette endpoint/alpha保持、Hue/Saturationのhue rotation、Photo Filterのtint density、Selective Colorの色域group選択、Tritoneのmidtone band、Color Balanceのtone bands/preserve-luma/premultiplied alpha、Levelsのmaster/per-channel curve、Lift/Gamma/Gainの3 channel groupsとspatial parameter descriptor、Shadow/Highlightのneutral passthroughおよびshadow tonal weight、input/output bounds・gamma・alpha・GPU pointwise descriptor・非有限setter fallbackを対象にする。Levels setterの現状確認ではgamma以外のmaster bounds setterは有限値をそのまま保持し、UI propertyの0–255 hard rangeとserialized master parserの制約との差がある。per-channel property pathには同じ有限値/範囲validationがない。黒白入力点が等しく境界画素に一致するとCore処理が0/0を作る経路もあり、finite invariant caseで回帰を固定する。これらはArtifact submodule内の実装であり、この作業では変更しない。GPU descriptor確認は実GPU実行とのparityではない。effect stack interaction、GPU実計算とのparityは未対応。
- `VibranceEffect` / `PosterizeEffect` / `ThresholdEffect` のCPU・descriptor testsは独立suiteから保留した。3 moduleの `.ixx` / `.cppm` は `Artifact/cmake/ArtifactSources.cmake` の app source manifest に含まれるが、`Artifact/CMakeLists.txt` の `ArtifactEffectsColor` module/implementation listsには含まれず、現 suite は `ArtifactEffectsColor` のみをlinkするためである。Artifact.exeを含む重い依存へsuiteを拡張せずに試験するには、親側から可能な独立library seamか、module所有先をArtifact submoduleで整理する作業が必要。
- Effect packの追加前にtarget source閉包も静的照合した。`ArtifactEffectsBlur` は AnisotropicFlow/ApertureShape/ReactionDiffusion の3実装のみで、通常の `BlurEffect` interface/implementation は `Artifact` executable 側の `ARTIFACT_EFFECTS_MODULES` / `APP_IMPL` に残る。このため通常Blur suiteを `ArtifactEffectsBlur` へリンクする案は成立せず、未解決シンボルになるため登録していない。Blur pixel/GPU node suiteにはimplementationを所有する独立library seamが必要で、Artifact submodule変更禁止の範囲では未対応。
- `ArtifactTextRenderTargetContractTest` を追加し、既存の軽量Diligent D3D12 text runtimeでnull/不正寸法、無効readback、7x5 targetのRGBA clear/readback全画素一致、target再作成後の透明clearを検証する（4 cases）。テストがimportする`Artifact.Render.TextRenderTarget`のmodule ownerである`ArtifactTextRenderTargetRuntime`と、GPU device APIを提供する`ArtifactTextGlyphSubmitterRuntime`の両方をlinkするよう静的配線を修正した（configure/build未実施）。`Artifact.exe`やArtifactIRenderer全体は起動しない。headless GPUが利用できない環境では明示skipし、CTestに`renderer;gpu;headless` labelを付けた。shader glyph submission／composition／effectを含むfull renderer parityは未対応。
- `ArtifactTextGlyphRenderContractTest` を追加した。既存のglyph submitter・shader source・render targetを直接組み合わせ、offscreen QGuiApplicationからD3D12 headless targetへ文字を実描画する。要求色のpixel、同一入力のpixel deterministic、zero opacity、非有限transformの拒否、`TextAnimatorEngine::applyAnimatorSets`で生成したrotation/scale/offset/opacity変換の可視pixel・alpha総量差を検査する（5 cases）。Index selectorは実装の0-based domainに合わせglyph index 0を指定する。target module ownerとsubmitter runtimeを直接linkし、`renderer;gpu;text;headless;contract` labelを付けた。Artifact.exe／全Composition rendererを通すgolden parityではなく、GPU glyph submitter boundaryのintegration contractである。configure/build/CTest未実施。
- Artifact CMakeの2つの軽量GPU text runtimeは現ArtifactCoreが定義する`ArtifactCoreText`ではなく旧名`ArtifactCoreTextRuntime`をlinkしていた。子repoを変更せずに依存を成立させるため、親の`CMakeLists.txt`で実在する`ArtifactCoreText`を指すcompatibility ALIASをArtifact追加前に定義した。CMake configure/buildは行っておらず、target解決は未確認。
- `tools/ui_visual_compare.py` はpixel-exactを既定とし、明示的なchannel tolerance / changed-pixel count / fraction gate、50% overlay、差分画像、SHA-256と環境manifestを含むJSON reportを生成する。名前付きregionごとに `--region-limit NAME,MAX_DIFF_PIXELS,MAX_DIFF_FRACTION` を指定でき、全体gateを通ってもregion gateに違反すれば失敗する。画像寸法が異なる場合も透明RGBA canvasへ左上揃えし、必ず不合格にしたうえでoverlay/diff/metricsとregion metricsを保存する。screenshot capture、UI interaction、承認基準画像自体はまだ用意していない。
- `tools/ui_visual_loop.py` は実行ごとにランダムなrun prefixを付け、同じ `--output-prefix` で再実行しても前回のcapture/overlay/diff/reportを上書きしないようにした。外部captureコマンドを各iterationのPNG出力先付きで実行する。`--region-limit` を比較器へ渡し、差分時はoverlay/diffを保存する。既定では修正担当者のEnterを待つ。`--retry-delay-seconds` 指定時は自動で再撮影し、`--max-iterations` で上限を設定できる。`--compare-only` は1回で終了する。pixel exactが既定。Timeline／Render Manager専用capture・操作fixtureと基準画像はまだ未整備。
- `tests/ui_visual/test_ui_visual_compare.py` に比較器の回帰用Python unittestを追加した。全体budget内でもregion limit違反なら失敗、exact matchのartifact生成、channel tolerance境界、寸法違い時のregion metricsと個別gate状態、同寸法／寸法違い双方での未定義region limit拒否の6項目を固定する。runner suiteと合わせた実行コマンドは `python -m unittest discover -s tests/ui_visual -v`。
- Timeline constructor はグローバルなwidget/service群を集成し、RenderQueueManagerWidget は永続化する `ArtifactRenderQueueService::instance()` を生成時に取得する。静的所有を再確認すると、現行 `ArtifactRenderCenterWindow` が使う `Artifact.Widgets.Render.QueueManager`、`Artifact.Render.Queue.Service`、Timeline implementationはいずれも `Artifact/cmake/ArtifactSources.cmake` の app module/implementation群にあり、`Artifact.exe` 非依存でそのWidgetをlinkする現行test targetはない。`ArtifactWidgets` childには `setService(QObject*)` を持つ別のlegacy `RenderQueueManagerWidget` があるが、現行 `ArtifactRenderCenterWindow` はそれをimportしないため、現行UIの試験代用にしてはならない。RenderJobModel suite はWidget interactionの代わりにはならない。親側単独では固定UI状態を注入できないため、Artifact childにtestable library/service seamと隔離可能な保存先を用意し、capture runner・承認基準画像を加えるまでUI visual gateは未実装扱いとする。
- UI比較器とloop runnerは一時的な5x4/3x2 RGBA fixtureで動作確認した。同一画像はpixel-exactで合格、1 channel値差の全20画素は不合格となり、region metrics/overlay/diff/JSONを生成。runnerはcompare-only成功と、意図的な差分で2回の自動再撮影後に上限で失敗終了することを確認した。これはツール自身のfixture確認であり、Timeline/Render ManagerのUIテスト結果ではない。
- CMake configure/build/CTestとArtifact UI/GPU/renderの実行は未実施。Layer save/restore、時間依存 keyframe、render parity、全 effect pack coverage、通常Blur独立target seam、effect stack integration、CPU/GPU実計算parity、full composition render golden、UI interaction/visual suites は未実装。

## 進捗 2026-10-07

- Method localsの事前確保見積もりで毎回ASTを再帰走査していた処理を除き、12個超のlocal bindingを評価器ごとの再利用workspaceへ移した。1 methodあたりinline 12件に加え最大32件のoverflow領域を、最大8段のnested method callまでlazyに保持し、45 locals（inline 12 + workspace 32を超える）または9段以降はtransient fallbackへspillする。20-local hookはwarm-up後0 CRT allocation / hookを計測し、45 localsと10段nested callのfallbackも実行テストした。20-localのDebug計測は17.46 µs/hookだが変更前値は取得しておらず、時間改善率は主張しない。ArtifactScript関連5 suiteはすべて成功。計測は単一MSVC Debugプロセスで、Releaseや実scriptの性能保証ではない。
- 6個以上のscript呼び出し引数で毎回確保されていたoverflow vectorを、評価器ごとの再利用workspaceへ移した。5引数までは従来のinline領域を使い、6〜32引数を最大8段までlazyなvector容量として再利用し、32個超または8段超は一時vectorへfallbackする。nestedな6引数ユーザーメソッドhookはwarm-up後0 CRT allocation / hookを計測し、33引数と10段nested callのfallback動作もテストした。各vectorは最大32 `ArtifactScriptValue` 分の要求でのみ再利用し、保持値は呼び出し終了時にclearして文字列等の動的内容を解放する。ArtifactScript関連5 suiteは全成功。計測は単一MSVC Debugプロセスであり、Releaseや実scriptの性能保証ではない。
- ArtifactScript の Host function / Host method 呼び出しで、固定容量 `span` 引数をvectorへコピーしていた経路を除去した。`ArtifactScriptHost` に `callFunctionView` / `callMethodView` を追加し、既存のvector APIは互換用wrapperとして維持する。5引数のwarm-up済みMSVC Debug hookではHost functionが2 allocation / 32 bytes、Host methodが6 allocation / 112 bytesとなり、いずれも5個の `ArtifactScriptValue` を格納するvector相当より小さいことを回帰条件にした。残るallocationの発生源は未特定で、ゼロ確保とは主張しない。ArtifactScript関連5 suiteはすべて成功。計測は単一DebugプロセスのCRT allocatorで、Releaseや実scriptの性能保証ではない。
- Host dispatchの重複lookupとmethod registry keyの都度合成を取り除いた。Host functionは1回のregistry lookupにまとめ、Host methodはclass/methodの所有キーとtransparent queryを分けた。旧形式で衝突し得た `("A.B", "C")` と `("A", "B.C")` の独立dispatchも契約テスト化した。5引数のwarm-up済みMSVC Debug hookで、Host functionは2 allocation / 32 bytesから1 / 16 bytes、Host methodは6 / 112 bytesから3 / 48 bytesに減少した（それぞれ allocation数50%減）。Host APIを評価器外から直接呼ぶ比較ではfunctionが1 / 16、methodが2 / 32で、script経由の追加はfunction 0、method 1 allocation / 16 bytesだった。benchmark時間は比較していない。残るallocationの発生箇所は未特定で、ゼロ確保とは主張しない。ArtifactScript関連5 suiteは全成功。計測は単一DebugプロセスのCRT allocatorであり、Releaseや実scriptの性能保証ではない。
- `tests/ui_visual/test_ui_visual_loop.py` にfake capture processを使ったrunnerのend-to-end unittestを用意した。compare-only成功、同じoutput prefixで連続実行した際の成果物分離、region gateを含む自動再撮影後の成功、Enter後の再撮影、`q`での停止、iteration上限での失敗と診断画像保持、capture exit code伝播、PNG未生成時の失敗、`{actual}` 欠落の早期拒否に加え、`{iteration}` 展開とenvironment manifestの比較レポート伝播を検査する（runner 11 cases）。fixture・出力・スクリプトのパスに空白を含め、Windowsの引数分割も通す。実行コマンドは `python -m unittest discover -s tests/ui_visual -v`。
- 上記コマンドを Windows / Python 3.14.2 / Pillow の環境で再実行し、比較器6ケースとrunner11ケースの計17ケースが成功した。Timeline/Render Managerのcapture fixtureやbaselineを使った実UI回帰ではない。
- ArtifactScript の既存 `ArtifactCoreArtifactScriptTest` が22件中18件で失敗していた状態を最新ソースで再ビルドして調査した。型付きメソッド引数が `ArtifactScriptMethod::parameters` に登録されず、引数参照とネストしたユーザーメソッド呼び出しが失敗していた。また、初期値なしフィールド宣言の末尾 `;` がフィールド名に残り、配列既定値と `foreach` の収集元フィールドを解決できなかった。パーサを修正し、短絡評価テストも先頭メソッドの直呼び出しから `OnUpdate` の名前解決実行へ直した。ArtifactScript 22件とLayerScriptComponent 2件を Windows / Debug でビルド・CTest実行し、両suiteが成功した。ビルドには `VSLANG=1033` と Windows SDK `rc.exe` のPATH、実行にはvcpkg/VS debug runtimeのPATHが必要だった。
- Object／Host contract suiteも実行対象に加え、コンパイル不能だった `executeMethod()` の真偽値assertと、root class／Host callbackの不足したfixtureを修正した。テストで `this.field` がメソッド実行用の一時fields mapではなく元インスタンスへ書かれて破棄される不具合、および式中の `this.field` をパーサが消費できない不具合を発見・修正した。ArtifactScript、LayerScriptComponent、ArtifactScriptObject、ArtifactScriptHostMethod、ArtifactScriptHostApi の5 suiteを再ビルド・CTest実行し、計33ケースが成功した。
- ArtifactScript の保存契約テストを追加した。公開フィールドと `[SerializeField]` 付き private フィールドの保存対象、未指定 private フィールドの除外、コンポーネント JSON の round-trip、未知キーの保持、保存型が既定値と異なる場合の既定値復帰を確認した。ArtifactScript を含む5 suiteを再実行し、全 suite 成功（新規2ケースを含む計35ケース）。
- レイヤースクリプトの統合寄り契約として、保存 JSON の復元 → ArtifactScriptInstance へのフィールド引き渡し → OnCreate / OnUpdate 実行 → 実行状態の再シリアライズまでを1ケースで通した。複数フレーム相当の dt/frame、SerializeField の状態維持、未シリアライズ private field の除外も確認。5 suite 全36ケース成功。これは Core の保存・実行 API 間の統合テストであり、ArtifactAbstractLayer やアプリ起動を通した実機統合確認ではない。
- OnUpdate の軽量スクリプト（フィールドに `dt` を加算）の Debug microbenchmark を追加し、60,000 hook の各計測前に2,000 hookをwarm-upして5回計測した。ArtifactScriptInstance がフックごとに新規 ArtifactScriptEvaluator / Impl を確保していた経路を、instance所有 evaluator の再利用へ変更した。変更前中央値5.73 µs/hook、変更後中央値5.20 µs/hook（約9%短縮）。加えて、実行エラー後に同じ evaluator で次のフックが正常実行できる回復テストを追加した。この性能値は単一ワークロードのローカル Debug 測定であり、Releaseや複雑なスクリプト、ヒープ確保数は計測していない。ArtifactScript関連5 suiteは再実行し全成功（38ケース）。
- 続けてhook/method lookupの重複を除去した。ライフサイクルhookを1回だけ解決して評価器へ渡し、評価器内でも名前で解決したmethodをそのまま実行する。スクリプト内呼び出しとオブジェクトmethod呼び出しも、解決済みmethodを再探索しない。追加した継承hookの回帰ケースで派生クラスより基底クラスを優先してしまう検索不具合も見つかり、derived-firstへ修正した。軽量Debug microbenchmarkは変更前中央値5.18 µs/hook、変更後3.38 µs/hook（追加約35%短縮）。累積では直近の元版5.73から約41%短縮。測定は同じローカルDebug単一ワークロードのみで、Release、実レイヤー、複雑なscriptの性能保証ではない。ArtifactScript関連5 suiteは39ケースすべて成功。
- 引数付きユーザーメソッドとローカル変数を含むhookを追加計測し、`evalCall` の引数vectorを実引数数でreserveした。さらにmethodごとのローカル表を、独自 `ArtifactCore::Array` の連続binding配列に置き換え、引数数と構文上のローカル宣言数で事前確保してハッシュノード確保を避ける。引数付きDebug microbenchmarkはvector reserve前中央値13.98 µs/hook、後12.74 µs/hook、Array locals後は5回測定中央値11.92 µs/hook（reserve後から約6.4%短縮）。軽量hookも同測定実行で中央値約2.28 µs/hookだったが、以前の測定との差は環境揺れを排除できないため改善値として扱わない。ArtifactScript関連5 suiteはすべて成功。いずれもローカルDebugの合成microbenchmarkであり、Release／実レイヤー／複雑なスクリプトの性能保証や確保回数の実測ではない。
- 引数vectorの確保を避けるため、引数を5個まで固定容量の値型領域に保持し、内部ユーザーメソッド／コンストラクタへ `span` で渡す。6個以上は事前確保vectorへfallbackし、6引数の境界テストで確認する。5引数fixtureは初期容量4でwarm-up後2 allocation / 256 bytes per hookだった。容量8は0 allocationにしたが固定領域が大きいため、観測した5引数までの容量5へ縮小した。現在も0 allocation / hookで、5引数fixtureは容量8時5.55、容量5時5.44 µs/hookだった（単一run差から時間短縮を主張しない）。固定領域は初期容量4より `ArtifactScriptValue` 1個分大きく、容量8案より3個分小さい。MSVC Debugの合成fixtureであり、Releaseや実scriptの性能保証ではない。`foreach` 元配列を `push` で変更する際の初期snapshot反復も回帰テスト化しており、snapshot再利用／無コピー案は速度改善を測定できなかったため採用していない。
- `foreach` の全field map複製を遅延overlayへ置き換えた。親map／親scopeを参照し、読み書きされたfieldだけを独自 `ArtifactCore::Array` の小さなbinding領域へコピー、成功時にiteration変数以外を親へcommitする。error時はscalar変更をcommitしない。nested foreach、iteration変数名と既存fieldの衝突、loop中の元配列push snapshot、失敗時rollbackの契約テストを追加した。8要素Debug benchmarkを5回計測した中央値は、2-field fixtureで12.80→7.78 µs/hook（約39%短縮）、追加48 fieldを持つ50-field fixtureで51.90→7.81 µs/hook（約85%短縮）。ローカルDebug合成測定であり、Release・複雑な実scriptの保証ではない。ArtifactScript関連5 suiteは全成功。
- Debug/MSVCテストにCRT allocation hook計測を追加し、空のhook lookupと空method bodyの直接評価が各0 allocationであることを確認した。クラス名を辿る3つの検索で所有文字列を作らず `std::string_view` を使うようにし、引数付きmethod fixtureは15 allocation / 664 bytes per hook、`foreach` fixtureは6 allocation / 744 bytes per hookを観測した。これは直列DebugテストプロセスのCRT heap計測で、Release allocatorや全スレッド／全allocatorを代表する性能保証ではない。ArtifactScript関連5 suiteは全成功。
- 上記のallocation計測を使ってsteady-state確保を追加削減した。`foreach` のelement snapshotは評価器所有の作業領域で深さ最大8・合計容量最大1024 elementsまで再利用し、上限外は従来どおり一時snapshotへfallbackする。最初の256要素上限では257要素hookに毎回2 allocation / 12,399 bytesが残ったため、合計容量上限へ変更した。field overlayは現在8 bindingを固定領域へ、localsは12 bindingを固定領域へ置き、local名は実行中AST所有の `string_view` にした。foreach反復変数をlocal数のreserve見積もりから除き、未使用のempty vector／stringは必要な経路だけで遅延生成する。Debug/MSVCでwarm-up済みのno-op、field arithmetic、4引数method/local、12-local method、8要素foreach、50-field foreach、257要素foreachはいずれも0 CRT allocation / hookを計測し、テストで回帰assertする。1025要素配列、9段nesting、overlay 8件超、local 12件超のfallback/overflow契約も検査し、ArtifactScript関連5 suiteはすべて成功。値コピー自体は保持し、作業領域の再利用上限を固定している。計測は単一DebugプロセスのCRT allocatorに限られ、Release、他allocator、並行実行の性能保証ではない。
- 文字列を含む2要素 `foreach` では、以前のwarm-up後計測が7 allocation / 240 bytes per hookだった。snapshotの高水位配列・文字列容量とiteration overlayを評価器内で再利用し、scope overlayへの文字列bindingを既存値へ直接代入するようにして3 allocation / 96 bytesまで削減した。さらに計測を空配列・短い文字列・長い文字列に分けると、残りは反復変数を通常のfield write経由で設定する処理に集中していたため、loop binding専用の直接設定経路を追加した。warm-up後は空・1要素短文・1要素長文・2要素長文のすべてで0 CRT allocation / hookを計測し、テストで回帰assertする。小さな保持容量は各workspaceあたりsnapshot 1 KiB、overlay 8×256 bytesに制限し、保持上限を超える文字列や非文字列参照はscope終了時に解放する。snapshot semanticsは文字列配列のpush mutationと連続hookで検査した。ArtifactScript関連5 suiteはすべて成功。計測は単一DebugプロセスのCRT allocatorであり、Releaseや他allocatorの性能保証ではない。
- 12個のlocal宣言を持つhookでwarm-up後も1 allocation / 256 bytes per hookが残ることを計測し、固定local binding容量を8から12へ広げて0 allocation / hookにした。13個超は従来のoverflow pathで処理し、14 localsの実行テストで契約を確認する。固定容量増加はArtifactScriptLocalsのstack footprintを1呼び出しにつき4 binding分だけ増やす。計測は単一DebugプロセスのCRT allocatorに限られ、複雑な実スクリプトでの速度保証ではない。
- 12個のlocalを持つmethodの実行時間が15.72 µs/hookだったため、inline localsの名前検索を線形走査から32-slot固定open-addressed indexへ置き換えた。warm-up後の再計測は8.57／8.44 µs/hook（2回の実行中央値8.51、約46%減）、既存のmethod/local fixtureは6.79から5.84 µs/hook（約14%減）だった。CRT確保は引き続き0 / hookで、indexは固定32 byte、overflow localsは従来のlinear fallbackを使う。これはMSVC Debugの合成fixture測定であり、Releaseや実プロジェクトscriptで同じ短縮率を保証しない。
- unqualified user-method callは毎回class method配列を探索していたため、呼び出しAST nodeとdefinitionをkeyにする32-slot direct-mapped cacheを評価器へ追加した。cache generationはtop-level evaluationごとに進め、definitionを差し替えた次のhookで古いmethod pointerを使わない回帰テストも追加した。32 methodsから同じmethodを16回呼ぶfixtureは75.20から50.50／50.40 µs/hook（再計測中央値50.45、約33%減）となり、CRT確保は0 / hookのまま。object-target method callとconstructor lookupはこのcacheの対象外。計測はMSVC Debugの合成fixtureに限る。
- Pythonビジュアルsuiteは17/17成功。Artifact.exe、UI実撮影、GPU/render parity、全Core suiteは今回の検証範囲外。

## 目的

`Artifact.exe` の起動・初期化に頼らず、テキストアニメーション、画像エフェクト、レンダリングの正しさを繰り返し確認できるテスト群を整備する。Timeline と Render Manager は、UI が完成するまで基準画像との透過比較を含む反復レビュー対象とする。

「独立実行」はアプリ本体の実行ファイルを起動しない意味とする。テスト対象ライブラリのコンパイル・リンクは必要になる。各ケースごとの CMake プロジェクトは作らず、ルートの `ARTIFACT_BUILD_TESTS`、`tests/CMakeLists.txt`、`artifact_add_test()`、CTest の構成を使う。

## 現状と既存計画

- ルート `CMakeLists.txt` の `ARTIFACT_BUILD_TESTS` は既定 OFF。ON の場合に限り GTest が見つかれば `tests/` を追加する。
- `tests/ArtifactCore/CMakeLists.txt` には個別 GTest ターゲットがあり、キーフレーム補間、カラーエフェクト、テキストシェーピング等のテストが一部存在する。
- `TextShapingTest.cpp` は RTL/Bidi、複数 script、結合文字等の有用な契約テストだが、Text Animator の selector/property evaluation や layer round trip の代用にはならない。
- 現在の `ArtifactCoreCreativeEffectTest` は `ColorVibranceEffectTest.cpp` の少数ケースであり、画像エフェクト全体や CPU/GPU parity の網羅根拠にはならない。
- `Artifact/CMakeLists.txt` には `ArtifactRenderTextSmoke` と `ArtifactTextGlyphSmoke` が `EXCLUDE_FROM_ALL` の個別 smoke target としてある。これは `Artifact.exe` 非起動の既存先例だが、現時点で GTest/CTest の assertion suite ではなく、GPU parity の包括的な合格証拠にもならない。
- composition render 経路の境界を追加確認した。`Artifact.Render.OffscreenComposition` は Diligent device/context/render target と readback を使う GPU integration 経路。`Artifact.Render.SoftwareCompositor` は Qt image/OpenCV を使う別の CPU 経路で、Core-only suite ではない。前者の結果は GPU 条件付き suite、後者は既存 CPU fallback の pixel contract として分離し、どちらか一方で最終 composition renderer 全体を代表したことにしない。
- `Artifact` ターゲットは多数の Core/Widget/Effect/Render target にリンクする構成である。`Artifact.exe` を起動しないことと、`Artifact` 全体をコンパイルせず軽量に済むことは別条件。Render/UI suite の target を選ぶ際は実際に必要な target closure を記録し、名前だけの小型 runner に全 Artifact 依存を持ち込まない。
- `docs/planned/MILESTONE_GPU_TEXT_ANIMATOR_TRANSFORM_BUFFER_2026-08-13.md` に Smoke / Contract / Stress fixture と Software/GPU 比較軸がすでに定義されている。本計画はその fixture をテスト運用へつなぐ。
- `docs/planned/MILESTONE_TEXT_ANIMATOR_ADD_WORKFLOW_2026-09-21.md` の runtime 受入条件、`docs/planned/MILESTONE_GPU_EFFECT_PERF_FIXES_2026-07-22.md` と `docs/planned/MILESTONE_EFFECT_GPU_RESIDENCY_AND_ASYNC_PREVIEW_2026-09-11.md` の GPU エフェクト経路も参照する。
- Timeline と Render Manager の専用 UI 撮影・基準画像比較は、現在の `tests/` に確認できない。

## テスト層と実行単位

| 層 | 実行方法 | 対象 | 合格の証拠 |
|---|---|---|---|
| Contract / Unit | GTest + CTest | 純粋な Core 評価、シリアライズ、エフェクト画素処理 | 数値・状態・不変条件を直接 assert |
| Render Integration | 独立テスト executable | レイヤーから composition/render result までの決定的な小シーン | 固定入力の画素・alpha・frame metadata と golden を比較 |
| Backend Parity | GPU 対応テスト executable | CPU reference と GPU 実出力 | 同一入力、明示 backend、許容誤差、fallback の有無を記録 |
| UI Interaction | Qt Test / offscreen または固定 desktop runner | Timeline、Render Manager の操作と状態表示 | 操作後の model/service 状態と widget 状態を assert |
| UI Visual Regression | UI runner + screenshot comparator | Timeline、Render Manager の配置・見た目 | overlay、差分画像、数値レポートを保存し、承認済み基準と照合 |

機能ロジックと UI の見た目を一つの重いテストにまとめない。unit / contract は GPU やウィンドウ表示を要求せず短時間で実行し、GPU parity と screenshot 比較は明示的な別ラベルの suite にする。テストは可能な限り各 suite の独立 executable とし、全機能を一つの `Artifact.exe` 内蔵テストへ集約しない。

## 必須スイート

### 1. Text Animator Contract

主に `ArtifactCore/src/Text/TextAnimator.cppm` とテキストレイヤーの公開評価契約を対象にする。既存の GPU Text Animator fixture 案を Smoke / Contract / Stress に分け、各 fixture に再現可能な seed と期待値を持たせる。

- Selector: Percentage / Index / Cluster / Line / Tag、各 shape、各 order、Start/End の一致・逆転、Offset の範囲外。
- Animator 合成: 単体・複数 animator、Selector combine mode、重複適用、全アニメーション可能な値、開始／中間／終了および境界の直前・直後。
- 入力: 空文字、複数行、折返し、日本語、RTL 混在、結合文字、合字、絵文字 modifier / ZWJ、font fallback。
- 時間: 非整数フレーム、FPS 変更、seek、逆再生、expression/envelope を使う経路。
- 永続化: preset 適用・編集、JSON round trip、欠落・未知・不正値、Animator 削除後の property/keyframe 参照。
- Stress: 数百〜数千 glyph、複数 animator、長時間評価。性能は固定環境の基準値と比較し、単なる成功終了で代用しない。
- 数値の全出力について NaN / Inf を拒否または仕様どおり処理する。

Layer/UI 統合受入は別 suite にし、Animator 追加 → 値編集 → keyframe → playback/seek → undo/redo → 保存・再読込まで通す。ASCII/CJK/emoji 等の形状差も確認する。

### 2. Image Effect Contract and Parity

各 effect の descriptor/parameter contract と、小さな既知画像に対する画素処理を確認する。

- 基準画像は opaque、半透明、完全透明、負値・HDR を含む float RGBA を用意する。
- 各 parameter の default/no-op、代表値、上下限、境界直前・直後、無効化状態を網羅する。
- effect 単体だけでなく、順序を変えた複数 effect stack、effect-local region、mask、solo/fallback の組合せを含める。
- Alpha を保持・変更する契約を effect ごとに明示し、premultiplied/straight alpha を混同しない。
- CPU/GPU の両方が実装された effect は同一入力で parity 比較する。RGB/alpha ごとに absolute と relative tolerance を定義し、最大誤差・平均誤差・逸脱画素率をレポートする。
- GPU 非対応や失敗時は fallback を暗黙に合格扱いしない。実際の backend、fallback reason、対象 effect を結果に残す。
- 小画像での golden pixel assertion を基本とし、アルゴリズム全体の画像 fixture も別途保持する。

### 3. Render Contract and Golden Output

小さな固定 composition を fixture とし、最終画像生成の contract を検査する。preview UI のスクリーンショットだけを renderer の正しさの証拠にしない。

- 解像度、pixel format/color space、alpha、frame rate、開始・終了・範囲境界を明示して検証する。
- 単層、複数層、順序、transform、mask、effect stack、透明背景を段階的に組み合わせる。
- 代表フレームの golden RGBA と metadata を比較し、差分を画像化する。
- 同じ入力を複数回 render して決定性を確認する。必要な非決定要因（GPU/driver 等）は記録し、許容幅を分離する。
- キュー／encoder 統合では生成ファイルの存在だけで合格にせず、フレーム数、寸法、alpha/codec contract、異常入力時の失敗状態を検証する。
- Render Manager UI の表示確認と、render engine の出力確認を別々に報告する。

### 4. Timeline / Render Manager UI

#### Interaction suite

- Timeline: 表示範囲、zoom/scroll、選択、keyframe 表示、scrub 後の frame state、Text Animator 行、disabled/empty 状態。
- Render Manager: queue の空／複数 job、選択と詳細、status/progress/filter、並べ替え、pause/cancel/failed 表示。
- UI の表示だけでなく、操作後のモデル／service 状態が期待どおりかも検証する。アプリ本体の main window や project 全初期化を起動せず、対象 widget に必要な最小 fixture/service を注入する。
- 現在のfixture blocker: `ArtifactTimelineWidget` constructor はtimeline配下の複数widgetをその場で生成し、Render Managerの `Impl` は `ArtifactRenderQueueService::instance()` とAppDataLocation上のhistory/preset storeを取得する。実UI suiteは存在せず、モデルsuiteだけではこの初期化/永続化/操作経路を証明しない。まず service注入と保存先隔離の seam を作り、固定job/layer状態を持つ専用fixtureからinteractionを検証する。
- Visual gate blocker: `tools/ui_visual_loop.py`、比較器、`ArtifactUiTest.exe` 起動 capture helper はあるが、Timeline/Render Managerを固定状態化するfixture、承認baseline、環境manifestの実値は未整備。したがって「完全一致まで繰り返す」は仕組みの動作確認までで、両UIの完成を示すgateではない。fixture/capture/baselineを整えた後、UI修正ごとにpixel-exactで一致するまで再撮影し、明示レビュー済みのbaseline更新以外は許容差を広げない。

#### Visual comparison loop

1. UI 状態を fixture で固定し、承認済み基準画像を記録する。
2. 同一の viewport size、DPI/scale、font、theme、locale、Qt version、widget state で screenshot を取得する。
3. 基準を半透明で重ねた overlay、絶対差分画像、領域別差分レポートを出力する。
4. レイアウト・余白・文字・色・選択/進捗状態のずれを修正する。
5. 同じ fixture で再撮影し、差分が合格条件を満たすまで繰り返す。
6. 基準画像を更新する場合は、意図したデザイン変更としてレビュー・承認されたものだけを新基準にする。

単発比較例（Python Pillow が必要。許容差の省略時は完全一致）：

```powershell
python tools/ui_visual_compare.py `
  --reference docs/design/ui-visual-regression/timeline-baseline.png `
  --actual artifacts/ui/timeline-current.png `
  --output-prefix artifacts/ui/timeline-review `
  --region ruler=0,0,1536,80 `
  --region tracks=0,80,1536,944 `
  --region-limit ruler,0,0 `
  --region-limit tracks,0,0 `
  --environment-json artifacts/ui/timeline-environment.json
```

UI修正と再撮影をpixel-exact合格まで繰り返す例。captureコマンドは `{actual}` の位置へ画像を書き出し、`{iteration}` も任意で利用できる。各試行の実画像・overlay・diff・JSONは別ファイルで保持する。

```powershell
python tools/ui_visual_loop.py `
  --reference docs/design/ui-visual-regression/timeline-baseline.png `
  --output-prefix artifacts/ui/timeline-loop `
  --capture-command "python tools/capture_timeline.py --output {actual}" `
  --region ruler=0,0,1536,80 `
  --region tracks=0,80,1536,944 `
  --region-limit ruler,0,0 `
  --region-limit tracks,0,0 `
  --environment-json artifacts/ui/timeline-environment.json
```

Render Manager では同じ loop を使い、regionを `toolbar`、`queue`、`details`、`status` のように分けて固定viewport内の矩形と各pixel-exact上限を指定する。基準画像とcapture fixtureが用意されるまでは、上記は運用例であり実UI gateの実行結果ではない。

許容差は設計レビューで根拠を決めた場合だけ追加する。`--channel-tolerance` はその差以下の channel 値を changed pixel に数えず、`--max-diff-pixels` と `--max-diff-fraction` は両方の上限を満たす必要がある。繰り返し指定できる `--region NAME=X,Y,WIDTH,HEIGHT` は各矩形の差分画素数・率・最大差・平均差を JSON に記録し、`--region-limit NAME,MAX_DIFF_PIXELS,MAX_DIFF_FRACTION` で各領域固有の合格上限も設定できる。領域gateと全体gateの両方を満たす必要がある。領域座標は同じ固定 viewport の pixel 座標を使う。環境 JSON には DPI/scale、画面寸法、Qt/Pillow version、font、theme、locale、対象 widget の寸法と状態を記録する。雛形は [`tests/ui_visual/environment.example.json`](../../tests/ui_visual/environment.example.json)。loop runnerは外部captureコマンドを実行し、差分時に修正担当者の入力を待って再撮影する。UI固有の操作・fixture・captureプログラムは引き続き必要。

「完全一致」はレンダリング環境まで固定できる範囲でのみ要求する。固定環境では pixel-exact gate を使い、それ以外では DPI/フォント差を含む環境差を切り分けた上で、領域別 threshold と重要要素の位置・文言 assert を用いる。閾値を広げて見た目の回帰を隠す運用は禁止する。比較に使った基準、実画像、overlay、diff、環境 manifest は CI artifact またはローカル成果物として保存する。

## CMake / 実行設計

Core `RenderImageContractTest`にCV_32FC3 BGR変換を追加し、R/B swap、HDR RGB値の保持、opaque alpha補完、unknown RGBA descriptorを固定した。

ImageSequenceのvalid same-path replacementを追加し、解像度を保った差し替えでfile size/content keyが変わり、再選択後に新しい画素を返すことを検査する。

GPU source texture共有条件のeffect分岐を補い、BrightnessEffect追加時に共有不可、clear後に共有可能へ戻ることを独立ケースで確認する。

ImageSequenceの追加ケースとして、単一pathが通常still-image扱いとなる境界と、FPS property setterのupper clamp・負値/NaN拒否時の既存値保持・zero許容を確認する。

SourceCropのsource-to-output transformを直接テストし、disabled/invalid size時のidentity、aspect保持時の中央配置とletterbox、aspect無効時のframe全域stretchを座標対応で確認する。

SourceCrop JSON値型に、空objectで全項目が既定値へ復帰する動作と、負寸法cropの正規化・型不正point/anchorや負zoomのfallbackを確認する独立ケースを追加した。

SourceCrop値型の直接境界として、reset後の全default値とrevision冪等性、無効source寸法でcrop geometry/revisionを保持しつつeffective rectを空にする動作を追加した。

既存の独立Core `RenderImageContractTest` にresize補間ケースを追加し、2x2→1x1の4 channel平均と2x1→3x1の中間pixel補間値・端点保持を確認する。

2026-10-10: `RenderImageContractTest` にempty `cv::Mat` 入力時のno-op契約を追加した。既存pixel値とcolor descriptorが維持されることを固定する。

同suiteにdescriptor付き `setFromCVMat` overloadのempty入力ケースも追加し、要求descriptorが渡されても既存pixel値・descriptorを変更しないことを確認対象にした。

`RenderImageContractTest` のcrop寸法境界に、画像下端でのzero-height cropがempty画像になるケースを追加した。既存のzero-width境界と対にして幅・高さ両軸を個別検査する。

2026-10-10: `StillImageLayerRoundTripTest` に、有効なsource寸法に対する `SourceCrop::clampToSource` の部分はみ出し補正値と再適用時のrevision不変を検査するケースを追加した。

`tests/ArtifactCore/DepthMapStandalone` を新設し、`Image.DepthMap` module単体を使う独立CTest実行ファイルを追加した。resize/default値・上下clamp、resize後の全sample再初期化、無効resize、setValue境界とempty map no-op、clearの冪等性、非対称4隅・非正方形3x2 gridと5x4 affine plane 21x21 gridのbilinear補間、端点・単一行・単一列・単一画素sample、grayscaleおよびRGB画像load、既存mapからの寸法/値置換、missingおよびcorrupt image load時clearの20ケースを含む。ArtifactCore全体・Artifact.exeにはリンクせず、configure/build/CTestは未実施。

`tests/ArtifactCore/AssetSequenceStandalone` にも依存moduleだけを組み立てる独立CTest実行ファイルを追加し、決定的group順、prefix中の数字と最終frame tokenの区別、suffix/paddingによるgroup分離、空prefixと拡張子必須境界、18桁上限token・frame 0開始・int64内19桁token拒否、拡張子内数字の扱い、連続runのgap分割、複数missing frame列挙、minFrames下限とgap分割後の閾値適用、Preserve時のminimum適用、大文字拡張子、未解析名のsingles化、SequenceGroupのrepresentative/path pattern/display name、Preserve missing-frame metadata、Qtなし一時fixtureによるdirectory scan、wrapperのgroup/singles決定順とminFrames引数転送、欠落directoryの空結果に加え、Split/Preserve両policyで全256通りの8-frame presence patternを照合し、minFrames要求値1〜9を網羅するケースを含む25ケース。Qt・ArtifactCore全体・Artifact.exeには依存しない。configure/build/CTestは未実施。

`tests/ArtifactCore/RationalTimeStandalone` に`Time.Rational` interface/implementationだけを使う独立CTest実行ファイルを追加した。scale補正、符号付きhalf-away丸めとseconds半tick、無効fpsとfactory fallback、異scale算術および小整数reference grid、加算の結合則grid、比較の交差積grid、rescaleのround reference grid、大きな分数比較とINT64_MIN境界、seconds変換、copy、負scale正規化、異scale cancellation、frame-count tie、signed入力の加算／減算逆操作、seconds conversion gridを21ケースで検査する。ArtifactCore全体・Artifact.exeには依存せず、configure/build/CTestは未実施。

`tests/ArtifactCore/FrameRateScaleStandalone` に`FrameRate::storageScaleForFps`専用の独立CTest実行ファイルを追加した。FPSの四捨五入、half境界の隣接値とnominal rate 1〜120でのsweep、正の小数の最小scale、NaN/±Inf/非正fpsの指定・既定fallbackとfallback補正（int64両端を含む）、大きな表現可能値とint64上限直下、固定刻みfinite rate gridを10ケースで検査する。ArtifactCore全体やFrameRate実装libraryには依存せず、Qt CoreとFrameRate/UniStringの限定module依存closureを使う。configure/build/CTestは未実施。

既存standalone `ImageSurfaceViewTest` にBGRAのmutable dispatchとpadded row strideを組み合わせたcaseを追加し、論理RGBA順の書き込み先、行間padding保持、未変更channelを確認対象にした。

ImageF32x4_RGBA file I/Oの独立ケースとして、純赤PNG保存、純緑PNG読込、失敗loadで既存buffer保持、empty EXR保存拒否を追加した。

typed surface viewのmemory safety境界として、float pointerの未整列、float単位で割り切れないrow stride、address span overflowの拒否を追加した。

`withMutableColorFloat4View`のdispatchをRGBA/BGRA別に検査し、1回だけcallbackを実行することとunknown descriptorではpixelを触らず拒否することを追加した。

ImageSurfaceViewのtyped channel accessについて、RGBA/BGRA別のlogical channel参照、mutable viewからのbuffer書き込み、channel order・stride・precision不一致と別buffer pointerの拒否を追加した。

ImageF32x4_RGBA合成APIに、opaque base上のopacity付きalphaBlendと、alphaBlend/blend双方の寸法不一致時の既定動作を独立ケースで追加した。

ImageF32x4_RGBAの入力・pixel access境界として、RGBA8の正規化とcaller memory非共有、不正RGBA32F入力時のpixel/descriptor clear、範囲外pixel操作、empty canonical conversion、CV MatのBGRA descriptor付きcanonical化を独立テストにした。

ImageF32x4_RGBAのsurface境界として、BGRA backingからcanonical RGBA/BGRAへのchannel順、float surface viewの寸法・stride・descriptor、およびempty viewのinvalid判定を追加した。

ImageF32x4_RGBAの所有権とalpha helperについて、DeepCopy後の画素独立、fillAlpha時のRGB保持、createMaskLikeの寸法／fill値、move construct/assign後の移動元再利用を独立ケースで追加した。

ImageF32x4_RGBAの未検査基本操作を補い、行・列反転、範囲cropの画素とdeep-copy性、範囲外cropのempty結果、4 channel weighted blendを個別ケースにした。

DepthMeshGeneratorについて、低解像度depth mapをより細かな格子へ展開する頂点深度・quad数と、empty mapから空meshを返す契約を独立テストに追加した。

background合成の残るalpha契約として、前景・背景双方が半透明なstraight-alpha重み、invert maskによる背景選択、透明画像背景上での前景alpha保持を別々の小画像fixtureで固定した。

追加で、empty sourceを受けたluminance segmenterの既存mask clear、batch内のthreshold refinement反映、DepthMapの負初期値clamp・clear冪等性・empty bilinear sampleを独立ケースにした。

segmentation後処理の境界として、最大hole面積超過時の保持、背景thresholdと等しい値の扱い、small-component除去の0面積上限での非変更拒否、empty maskのrefinement拒否を追加した。

segmentation refinementの独立ケースを追加し、soft/hard threshold、1pxのforeground expand/contract、横縦両方向のfeather平均を固定値で確認する。

segmentation batchの独立ケースを追加し、null/empty sourceを含む部分失敗のcompleted/failed/error集計と、segmenter未準備時に推論を呼ばず全件失敗として返す契約を固定した。

segmentationの追加契約として、luminance segmenterのgamma・dark foreground・alpha無視設定、低解像度maskをsource座標へ対応させるtight crop、および空mask拒否時の出力保持を独立テストにした。

`ArtifactStillImageLayerRoundTripTest` の segmentation合成契約として、単色背景へのmask端点・soft blend、異解像度背景画像のnormalized bilinear sampling、入力が空の場合にdestinationを保持する拒否動作を追加した。

`ArtifactStillImageLayerRoundTripTest` にCore segmentation helperの独立ケースを追加し、luminance maskのblack/white pointとsource alpha、soft green edgeだけを対象にするdespillとalpha保持、foreground extraction時のsource非変更を検査する。

`tests/ArtifactCore/StringViewStandalone` に Qt Core と `Utils.String.View` moduleだけを使う独立suiteを追加した。QString/UTF-8往復、非ASCII文字と埋め込みNUL、prefix/suffix/contains、nullとemptyの区別、Unicode whitespace判定を6ケースで固定する。CMake configure/build/CTestは未実施。

`tests/ArtifactCore/JsonLikeStandalone` に Qt Core と `Utils.Json.Like` moduleだけを使う独立suiteを追加した。QJsonObjectの読み書きと、読み取り専用・書き込み専用・不適合型に対するconcept境界を4ケースおよびcompile-time assertionで固定する。CMake configure/build/CTestは未実施。

`tests/ArtifactCore/MultipleTagStandalone` に Qt Core、`Utils.MultipleTag` interface/implementationだけで構成する独立suiteを追加した。大小文字区別、重複・空白正規化、any/all検索、決定的ソート、空prefix／invalid regexを含むprefix/regex filter、空集合を含む集合演算、文字列parse/serialize、custom separator、clear後の再利用、copy/moveの16ケースを用意する。CMake configure/build/CTestは未実施。

`tests/ArtifactCore/HandleStandalone` に Qt Core と `Utils.Handle` moduleだけを使う独立suiteを追加した。nil/valid判定、factoryとraw値、比較順、最大`uint64_t`値、tag型による非変換性、左右両順の`nullptr`比較、free validity helper、hash seed伝播を10 runtime casesとcompile-time assertionsで固定する。CMake configure/build/CTestは未実施。

既存の独立 `Numeric` suiteへ境界ケースを4件追加した。整数limits、±infinity、最小positive subnormal、異なる算術型で同値となる比較についてmin/max結果とcommon typeを固定する。テストは未実行。

`tests/ArtifactCore/ArtifactExpectedStandalone` に必要なOptional/Array/String moduleだけを構成する独立suiteを追加した。値・エラー状態、fallback、Optional変換、andThen/orElseの分岐、value/error変換、独自error型、rvalue value取得を9ケースで確認する。CMake configure/build/CTestは未実施。

既存の独立 `Optional` suiteへ4件追加した。`ToResult` の既定エラー、`TransformOrElse` の mapped type 変換と遅延実行、present時のvalue pointer identity／変更反映、所有型の`ValueOr`を確認する。テストは未実行。

既存の独立 `Id` suiteへ5件追加した。clearの冪等性と再代入、move元のclearによる再初期化、異なるtyped wrapper間のUUID同一性、nil hashのseed一致、空・短い桁数・非hexのparse拒否を確認する。テストは未実行。

既存の独立 `Result` suiteへ4件追加した。unwrapOrの成否分岐、ErrorContextの診断情報全保持、status/getStatusの同一参照、rvalueからの値取得を確認する。テストは未実行。

既存の独立 `HashValue` suiteへ2件追加した。file hashの8192-byte read chunk直前・一致・直後・2倍境界、およびnonempty bytesでもUnknown algorithmならinvalidとなる契約を確認する。テストは未実行。

既存の独立 `NameGenerator` suiteへ2件追加した。空のbase nameでも番号slotが進むこと、および日本語base nameがUTF-8変換を通して保持されることを確認する。番号slotのない固定patternを重複生成すると実装の重複回避ループが終了しない点はInsightへ記録し、hangするテストは追加していない。

既存の独立 `PerformanceProfiler` suiteに未検査だった`AudioEngineProfiler`ケースを3件追加した。callback/fill集計とunderflow、reset後の累積統計、negative buffer level clampを確認する。実装上`resetStats()`がlast callback/requested framesを維持する点はInsightに記録し、reset契約の確認待ちとした。

既存の独立 `CppLinq` suiteへ3件追加した。`FirstOrDefault` の最初の一致後短絡、`Where` のempty/no-match view、`Select` のempty viewでtransform未実行を確認する。テストは未実行。

既存の独立 `TextNumber` suiteへ3件追加した。null終端に依存しない`string_view` sliceのparse、先頭ゼロと負のゼロ、整数・bool入力内のembedded NUL拒否を確認する。テストは未実行。

`tests/ArtifactCore/ArtifactArrayStandalone` に `ArtifactArray` とOptional moduleだけで動く独立executableを追加した。append/insert、copy/move、optional参照access、remove/take/search、slice/reverse、resize/fill、capacity/swap、比較、StaticArray、非自明要素のlifetime balanceの14ケースを用意する。CMake configure/build/CTestは未実施。

`tests/ArtifactCore/ArtifactStringStandalone` に Optional/Array/String moduleだけで動く独立executableを追加した。SSO境界、copy/move、自己alias appendとheap growth、embedded NUL、substring/search/replace、数値変換・format、ASCII case/trim/split/joinの16ケースを用意する。CMake configure/build/CTestは未実施。

既存の独立 `FrameRate` suiteへ3件追加した。`storageScaleForFps()` の整数／NTSC変換、`llround` half boundary、invalid FPS時のpositive fallback正規化を確認する。テストは未実行。

`tests/ArtifactCore/ArtifactHashMapStandalone` に Optional と `ArtifactHashMap` moduleだけの独立suiteを追加した。基本lookup、mutable/const access、bracket insertion、collision chain lookup/erase、rehash後の200要素保持、non-collision iteration、clear/reuseを9ケースで確認する。2026-10-11にcollision iteration回帰caseと`std::unordered_map` model testを追加し、合計23ケース。collision iteration欠落とmove後破棄の疑いはInsightに記録。suiteは未実行。

`tests/ArtifactCore/ArtifactAlgorithmsStandalone` に Optional/Array/Utility/Algorithms moduleだけの独立suiteを追加した。find/predicate、min/max、accumulate、iota/count、fill/reverse、lowerBound/binarySearch、sort、removeIf fixture、container findを12ケースで検査する。`artifactRemoveIf` と `Array::removeAt` のAPI不一致はInsightに記録。CMake configure/build/CTestは未実施。

`tests/ArtifactCore/ArtifactSetStandalone` に Optional/Array/Utility/Set moduleだけの独立suiteを追加した。重複、参照lookup、iterationとvaluesの順序、collision chain、rehash、copy/move後のsource再利用、clear、swapを10ケースで検査する。CMake configure/build/CTestは未実施。

`tests/ArtifactCore/ArtifactTupleStandalone` にUtility/Tuple moduleだけの独立suiteを追加した。empty tuple、const/mutable element access、lvalue decay、move-only forwarding、各要素の等値性、copy独立性、nested tupleを7ケースで検査する。CMake configure/build/CTestは未実施。

`tests/ArtifactCore/ArtifactMathStandalone` にMath moduleだけの独立suiteを追加した。NumericTraits、extrema tie、clamp、finite/NaN、型promotion、trigonometry、rounding、fmod/hypot、swap、lerpの12ケースを用意する。CMake configure/build/CTestは未実施。

`tests/ArtifactCore/ArtifactSaturationStandalone` にSaturation moduleだけの独立suiteを追加した。signed/unsigned add/sub/mul境界、overflow符号組合せ、same/widen/narrowのsaturateCastを11 runtimeケースと7 compile-time assertionで確認する。CMake configure/build/CTestは未実施。

2026-10-09時点で、`tests/ArtifactCore/ColorSpaceStandalone` に色変換・画像surface関連の独立CTest suiteを11件登録し、Debug構成で全11件が合格した。ColorBridgeにはACEScc/ACEScct、DaVinci Intermediate、Cineon、S-Log3、Canon Log 3のdense normalized-code gridを追加済み。Canon Log 3はlow code境界でdecodeの下降を確認したため、全域単調性を前提にせず区間ごとの単調性と境界挙動を検査する。これは色変換の追加検証結果であり、下記の未実行UI/renderer段階の完了を意味しない。

- per-test の CMakeLists は作らない。現行の `artifact_add_test()` を使い、既存 suite の粒度に合わせて追加する。
- Text Animator Core、effect contract、renderer integration、UI interaction、GPU parity、UI visual regression は個別に実行・選択できる名前/CTest label を持たせる。現在のCTest labelは `animation;keyframe;cpu;contract`、`animation;text;cpu;contract`、`effect;cpu;contract`、`renderer;image;cpu;contract`、`render-manager;model;cpu;contract`、`renderer;gpu;headless`。これは実行選択用分類で、CTest実行済みを意味しない。
- 構成済みbuild treeでの選択例は `ctest --test-dir <build> -L animation`、`-L effect`、`-L renderer`、`-L render-manager`。個別target名は `-R ArtifactCoreTextAnimatorContractTest` のように指定できる。ここに示すのは運用例であり、現時点では build tree を生成せずCTestも実行していない。
- `tests/Artifact` はrootのtest CMakeから有効化済みで、Effect suiteは`ArtifactEffectsColor`等の個別static libraryをlinkする。現在も`Artifact.exe`自体はlinkしていない。`tests/ArtifactWidgets` は未登録で、Timeline/Render Manager widgetのservice・永続化依存をほどいてから個別suiteとして登録する。
- UI screenshot runner は unit test の通常実行から分離する。UI backend / GPU device がない環境でスキップする場合は、理由と未実行 suite を明示し、合格として数えない。
- CMake configure/build/test 実行は別途ユーザーが明示したときに行う。

## 推奨実装順

1. **Core contract を厚くする:** 既存 `KeyframeSplineTest` / `TextShapingTest` を再利用し、Text Animator 専用 GTest と effect parameter/alpha fixture を追加する。まず GPU・window 非依存で契約を固定する。
2. **小画像 render integration:** 既存の effect/render library を直接 link できる範囲で、固定 RGBA fixture と golden comparison を追加する。テストから `Artifact.exe`、global app 起動、ユーザー設定を要求しない。
3. **GPU parity smoke を正式化:** 既存 `ArtifactRenderTextSmoke` / `ArtifactTextGlyphSmoke` と GPU effect 診断を棚卸しし、終了コード・backend・fallback reason・差分 report を明示する。テスト済みの実行ファイル／shader が現ソースと一致する鮮度条件を置く。
4. **Widget interaction:** `tests/Artifact` / `tests/ArtifactWidgets` の CTest 登録方法と widget が必要とする service 境界を確認し、Timeline と Render Manager を最小 fixture で起動する。アプリ全体の bootstrap は依存にしない。
5. **Screenshot regression loop:** 固定 runner 環境と承認基準を決めてから、capture → overlay/diff → 閾値判定 → artifact 保存を自動化する。UI interaction が成功しても visual suite が未実行なら UI gate は未完了とする。

各段階の変更後は対象 suite を個別に build/run し、実行していない suite を「合格」と数えない。ビルド・テスト・CMake の実行はユーザーの明示指示を得てから行う。

## 完了ゲート

1. 各 suite が `Artifact.exe` の起動なしで個別実行可能。
2. Text Animator の Core contract、保存復元、Layer 統合、Software/GPU parity の結果がそれぞれ報告される。
3. Effect の入力・alpha・stack contract と、対応 backend の parity が全対象 effect について追跡可能。
4. 固定 render fixture の golden output と再現性が確認される。
5. Timeline と Render Manager の interaction suite が状態遷移を確認し、visual loop が基準画像・overlay・diff・environment manifest を生成する。
6. 失敗時に、失敗 suite・fixture・backend・画像差分を見て原因を特定できる。

この計画はテスト基盤・テストケースの実装完了を意味しない。CMake 登録、fixture/golden 作成、UI runner 実装、実行結果は各 Phase で別途記録する。

\n
