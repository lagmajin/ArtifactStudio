# Artifact.exe 非依存テストスイートと UI ビジュアル回帰計画

**最終更新:** 2026-10-10
**ステータス:** In Progress

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

- `ColorACESContractTest.cpp` を9つ目の独立CTest target `ArtifactCoreColorACESTest` として追加した。既存のColorSpace / gamut closureに`Color.TransferFunction`と`Color.ACES` interfaceを加えて再利用し、ArtifactCore全体はリンクしない。
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
