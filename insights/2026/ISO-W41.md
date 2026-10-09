**最終更新:** 2026-10-09


# Insight Register — 2026-W41

期間: 2026-10-05 – 2026-10-11

## 2026-10-09 — HLG折れ点の隣接float単調性

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、`ColorTransferFunction::encode/decode(HLG)`。
- **確認済み:** OETFのfloat `1/12` とEOTFの`0.5`それぞれについて、`std::nextafter`で得た直前・一致・直後のfloatを評価し、float閾値に合わせて枝を選ぶ独立double参照値と一致すること、出力が非減少であることをstandaloneテストで確認した。
- **価値／懸念:** 数学的折れ点のdouble近傍テストに加えて、実際のAPI入力型であるfloatの枝境界も回帰検出できる。隣接3点の確認であり、全域の単調性証明ではない。
- **次に確認すべきこと:** 他のpiecewise transfer functionでもfloat表現の閾値と実装比較型が一致しているか、隣接floatテストで確認する。

## 2026-10-09 — S-Log3折れ点の隣接float単調性

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、`ColorTransferFunction::encode/decode(SonySLog3)`。
- **確認済み:** OETFの線形閾値`0.01125f`とEOTFの正規化コード閾値`171.2102946929f / 1023.0f`について、直前・一致・直後のfloatを評価し、float閾値に合わせた独立double式および非減少順序と一致した。standaloneターゲットで実行確認済み。
- **価値／懸念:** S-Log3の既存double近傍確認を実API入力精度まで補い、ブランチ境界の丸め変更を検知できる。対象は隣接3点のみ。
- **次に確認すべきこと:** Canon Log 2/3など他の複数toe transfer functionにも、枝ごとの連続性・順序・意図的な段差を個別に記録する。

## 2026-10-09 — Canon Log 2 toeのfloat隣接値とゼロ丸め

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、`ColorTransferFunction::encode/decode(CanonLog2)`。
- **確認済み:** 実装のfloat演算で得る負の線形toeと`0.035388128f`コードtoeの前後各1 ULPを評価し、各枝の独立double参照と一致した。線形toeを跨ぐOETFは既存観測どおり0.014超の段差があり、コードtoe近傍のdecodeは隣接入力がfloatのゼロへ丸められるplateauを含む。standalone suite 64 cases成功。
- **価値／懸念:** 負値toeの境界実装をfloat精度で保護し、単純な厳密増加を期待すると失敗するゼロ丸めを記録する。OETFの段差は現行実装の特性であり、規格適合の判断をこのテストだけで行うものではない。
- **次に確認すべきこと:** Canon Log 3の低toe／線形領域／高toe境界もfloat隣接値で評価し、意図した枝順序と既知段差を整理する。

## 2026-10-09 — Canon Log 3の3枝境界をfloatで確認

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、`ColorTransferFunction::encode/decode(CanonLog3)`。
- **確認済み:** 実装と同じfloat演算で導出する低線形toe・高線形toe、および低・高コード閾値の直前／一致／直後を、各枝を独立に評価するdouble式と照合した。低toe接続にはOETFで0.006超、EOTFで0.002超の段差がある一方、高側の線形／log接続差は両方向とも`1e-6`未満だった。standalone suite成功。
- **価値／懸念:** 3領域を持つ曲線の枝境界を入力型floatで確認し、定数や比較演算の変更による隣接値の枝ずれを検出できる。ここでの段差は現行実装の特性であり、規格適合の判断をこのテストだけで行うものではない。GPU実装との一致も保証しない。
- **次に確認すべきこと:** 段差があるCanon Log 2/3のlow toeについて、参照資料の期待値と実装意図を別途照合し、挙動変更の要否を判断する。

## 2026-10-09 — DaVinci Intermediateのゼロ直近float挙動

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、`ColorTransferFunction::encode/decode(DaVinciIntermediate)`。
- **確認済み:** OETFは負値と`-0`を0へ返す一方、最小正subnormalをlog式へ通し、ゼロ入力との間に10超の出力差がある。負のsubnormal・最小normalも負値側と同じゼロ出力。EOTFは負の最小subnormalから0、正の最小subnormal・最小normalまで独立指数参照に一致し非減少。standalone suite成功。
- **価値／懸念:** 通常の10-bit格子や正値round-tripでは見えにくいゼロ境界の挙動を固定した。ゼロで定義を切り替えるOETFの不連続を記録するが、規格・製品仕様上望ましいかの判断は含まない。
- **次に確認すべきこと:** `log2`実装が異なるビルド構成でもsubnormal入力を保持するか、別環境のstandalone実行で比較する。

## 2026-10-09 — Rec.2020 EOTFのfloat閾値で局所的な低下

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、`ColorTransferFunction::decode(Rec2020_10)`。
- **確認済み:** 既存のdouble隣接値テストはfloatへ変換した時点で同じ値になり得るため、`0.081242858298635f`と実際の前後floatを追加検査した。OETFはこの3点で非減少。EOTFは閾値直前から閾値入力へ移ると出力が1e-8超だけ低下し、その低下は独立参照式と一致した。suite成功。
- **価値／懸念:** 実APIのfloat境界にある非単調性を検出可能にした。これは既存係数・枝条件の現在の挙動を記録する特性テストで、ここでは修正や規格適合判断をしていない。
- **次に確認すべきこと:** BT.2020の規格定数と実装係数・閾値の整合を一次資料で照合し、許容差として維持するか修正するか判断する。

## 2026-10-09 — PQ EOTF分母ガード直近のfloat精度差

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、`ColorTransferFunction::decode(Rec2084_PQ)`。
- **確認済み:** floatの特異点近傍から隣接入力を下向きに走査すると、正のdecode出力が初めて現れるコードの直上は0を返す。その最初の正出力はfiniteで`1e20`超だが、独立double式の値はその100倍超となる。floatの`pow`・分母計算の量子化が特異点近傍の値を大きく変えることをstandaloneテストで確認した。
- **価値／懸念:** 10-bit領域外のfloat特異点近傍に、ゼロから巨大値へ遷移する箇所と大きな高精度参照差がある。現テストは現在のfloat実装を特性化し、修正の妥当性やHDR用途での許容性を判断しない。
- **次に確認すべきこと:** PQ EOTFの特異点近傍に要求される入力領域・数値精度を仕様と利用箇所で確認し、必要なら安定化した式または入力範囲制約を別途検討する。

## 2026-10-09 — 単純ガンマ曲線のsubnormal入出力

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、Gamma 2.2 / 2.4 / 2.6 のencode/decode。
- **確認済み:** 0、最小subnormal、`nextafter(0,1)`、最小normalをdouble `pow`参照と比較した。encodeはfloat指数係数とdouble参照の差を含め相対3 ppm以内。decodeはdouble参照をfloatへ丸めた結果と一致し、負の最小subnormalは0にclampされる。standalone suite成功。
- **価値／懸念:** 標準10-bit格子や通常明度テストでは通らないアンダーフロー近傍を検査する。相対許容差はencodeのfloat指数近似を許容し、一般的な色差精度を主張するものではない。
- **次に確認すべきこと:** 別のコンパイラ／数学ライブラリでsubnormal保持とpow精度が同じか比較する。

## 2026-10-09 — 未知TransferFunction値のidentity fallback

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、`ColorTransferFunction::encode/decode` のswitch default。
- **確認済み:** 明示列挙された未実装`ACESlog`と`static_cast<TransferFunction>(0x7fffffff)`は、有限範囲外値・±∞・NaN・負のゼロを含めencode/decodeが入力をidentityで返す。負のゼロの符号も保持される。standalone suite成功。
- **価値／懸念:** 将来のenum追加漏れや未知値を黙って線形扱いする現在のAPI動作を固定する。未対応値をidentityへ流すのが望ましいという設計判断ではない。
- **次に確認すべきこと:** TransferFunctionが永続化データや外部入力から復元される場合、未知値を拒否・警告・identityのどれにするべきか契約を確認する。

## 2026-10-09 — clamp系curveの負のゼロ入力

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、sRGB・単純ガンマ・Rec.709/2020・PQ・HLG。
- **確認済み:** 8つの非negative clamp系curveすべてで、`-1e-7`・負の最小subnormal・`-0.0f`をencode/decodeすると数値結果は0。standalone suite成功。
- **価値／懸念:** 通常の負数テストでは覆わない符号付きゼロとsubnormalのclamp挙動を回帰保護する。テストはゼロの符号bit保持までは契約せず、数値ゼロを確認する。
- **次に確認すべきこと:** Cineonおよびsigned-log曲線は負値を別の規則で扱うため、それぞれのゼロ境界を独立参照とともに確認する。

## 2026-10-09 — signed-log curveの符号付きゼロとsubnormal

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、S-Log3・Canon Log 2/3・ACEScc/cct。
- **確認済み:** 5曲線について、encodeとdecodeへ負の最小subnormal・`-0.0f`・`+0.0f`・正の最小subnormalを与え、各曲線の独立参照式と比較した。すべてstandalone suiteで成功。
- **価値／懸念:** 異なる黒コード／toe規則を持つsigned-log曲線のゼロ近傍を一括して保護し、Clamp系曲線と同じゼロ扱いを誤って期待しない。許容誤差は通常値と大きなACEScc decodeに対する相対3 ppmを含む。
- **次に確認すべきこと:** Cineonの負のゼロ・subnormal入力はblack-offset式に入るため、別途その境界を参照と照合する。

## 2026-10-09 — Cineon黒オフセットと符号付きゼロ

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、`ColorTransferFunction::encode/decode(Cineon)`。
- **確認済み:** OETF/EOTF双方で負の最小subnormal・`-0.0f`・`+0.0f`・正の最小subnormalを独立したCineon式と照合した。OETFは負のsubnormalと負のゼロを0入力と同じblack codeへ写す。decodeのゼロコードは0ではなくblack offset相当の負のlinear値を返す。standalone suite成功。
- **価値／懸念:** Cineonの黒は線形ゼロと同値ではないという符号化契約を回帰保護する。ゼロ近傍の絶対差やフィルム規格としての妥当性は別途の判断事項。
- **次に確認すべきこと:** Cineon black codeのfloat隣接plateauとゼロコードの負値応答を、対象ビット深度や実際の入出力経路と照合する。

## 2026-10-09 — Cineonの線形値往復

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、Cineon OETF/EOTF。
- **確認済み:** 線形0、`1e-8`〜100の10点をencode/decodeし、各出力が独立したdouble OETF/EOTF参照に一致し、入力線形値へ相対3 ppmまたは`2e-7`絶対幅以内で戻る。standalone suite成功。
- **価値／懸念:** Cineonの両変換を個別に検証するだけでなく、線形値の合成往復も確認できる。実装の連続float経路を対象とし、10-bit整数量子化・コード丸めを伴う往復精度は保証しない。
- **次に確認すべきこと:** 実際に10-bit codeへ量子化する経路があれば、量子化段を含む誤差も別suiteで測る。

## 2026-10-09 — Cineon 10-bit code往復とblack floor

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、Cineon EOTF→OETF。
- **確認済み:** 10-bitコード0〜1023をnormalized floatへ変換してdecode→encodeし、再量子化したコードを検査した。0〜94はblack code 95へ集約され、95〜1023は元の整数コードへ戻る。全1024コードでstandalone suite成功。
- **価値／懸念:** 個別の参照比較に加え、Cineonの10-bit black floorと整数コード往復契約を通しで検査できる。量子化はテスト側の`lround(encoded * 1023)`でモデル化し、画像ファイルI/Oの丸め規則は対象外。
- **次に確認すべきこと:** 実I/Oで使う丸め・clamp規則とこのnormalized codeモデルが一致するか確認する。

## 2026-10-09 — ACEScc 10-bit codeのlinear-light往復

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、ACEScc EOTF→OETF。
- **確認済み:** normalized 10-bit code 0〜1023をdecode→encodeして`lround(code * 1023)`で再量子化すると、全コードが元の整数値へ戻る。code 0は既存の独立参照が示す131072 linear sentinelを経由するが、再encode後も0へ戻る。standalone suite成功。
- **価値／懸念:** black sentinelを含む全10-bit codeを線形光経由で連結検証し、個別関数の参照比較だけでは見えない往復関係を保証する。`lround`はテスト側の量子化モデルであり、画像I/Oの丸め仕様は対象外。
- **次に確認すべきこと:** ACEScc code範囲やbit depthが異なる外部コンテナとの変換境界を確認する。

## 2026-10-09 — ACEScct 10-bit codeのsigned-toe往復

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、ACEScct EOTF→OETF。
- **確認済み:** normalized 10-bit code 0〜1023をdecode→encodeして再量子化すると、全コードが元の整数値へ戻る。低コード域では線形値が負になるtoeも含め、float実装と独立double参照の合成結果が一致した。standalone suite成功。
- **価値／懸念:** ACESccとは異なるsigned linear toeを含め、全10-bitコードを変換経路として検証できる。再量子化はテストの`lround`モデルで、外部I/Oの量子化仕様は対象外。
- **次に確認すべきこと:** float code以外の整数bit depthや実ファイル経路でも、negative linear toeを保持できるか確認する。

## 2026-10-09 — S-Log3・Canon Log 2/3の10-bit合成往復

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、S-Log3 / Canon Log 2 / Canon Log 3 EOTF→OETF。
- **確認済み:** 3曲線の全1024 normalized codesをlinear経由でencodeし、再量子化結果を評価した。S-Log3はコード0〜94が95 black codeへ集約され、95〜1023は原コードへ戻る。Canon Log 2は0〜28が原コードからずれ、29〜1023では一致する。ずれる値は独立OETF/EOTF参照の合成を丸めたコードに一致。Canon Log 3は低toe・線形・高toeを含む全コードで独立参照の合成結果に一致した。standalone suite成功。
- **価値／懸念:** 3つのlog curveを各関数単独だけでなく全10-bit codeの往復として検査し、S-Log3のblack floorとCanon Log 2低コードの往復差を明示する。Canon Log 2の差は現挙動の特性で、規格適合の判断や補正方針を示すものではない。
- **次に確認すべきこと:** Canon Log 2の低コード0〜28が外部仕様で許容されるか、一次資料と利用コード経路に照らして判断する。

## 2026-10-09 — DaVinci Intermediate 10-bit code往復

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、DaVinci Intermediate EOTF→OETF。
- **確認済み:** normalized code 0〜1023をdecode→encodeして再量子化し、全コードが元の整数値へ戻ること、また独立double EOTF/OETFの合成結果と一致することを確認した。code 0のdecodeは正のscene-linear値だが、再encodeでcode 0へ戻る。standalone suite成功。
- **価値／懸念:** encode(0)=0という特別分岐だけでなく、10-bit最暗コードとの整合を線形光往復で検証する。テスト側の`lround(code * 1023)`モデルであり外部I/Oの量子化は対象外。
- **次に確認すべきこと:** 負のcodeや1超のcodeでも独立参照と単調性を別途確認し、scene-linear外挿との接続を整理する。

## 2026-10-09 — ACEScc / ACEScct / Canon Log 2の範囲外入力

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、3つのlog curve encode/decode。
- **確認済み:** normalized codeの負値・0・1・1超と、scene-linearの負値・0・正HDR値について、ACEScc、ACEScct、Canon Log 2双方の入出力を独立double式と照合した。対象点はすべてfiniteで参照に一致し、standalone suite成功。
- **価値／懸念:** 通常の10-bit 0〜1格子外にあるlog符号化の数学的外挿と範囲外値を明示的に回帰保護する。実データで許容される入力範囲やクリップ方針を決めるものではない。
- **次に確認すべきこと:** CineonおよびS-Log3/Canon Log 3の負code・HDR codeについても同様の包括ケースを揃え、curveごとのclamp／外挿差を整理する。

## 2026-10-09 — 6 log curveの広域negative/HDR外挿

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、S-Log3・Canon Log 2/3・Cineon・ACEScc/cct。
- **確認済み:** 6 curveについてcode ±2、linear -1e6〜1e7を独立参照式と照合した。S-Log3 OETFは負のlinearを0へclampするため、負値入力の参照を0入力へ正規化する必要があり、テストでその動作を確認した。他のcurveは負値／HDR外挿式と一致。standalone suite成功。
- **価値／懸念:** ±1付近の既存確認を大きく越える入力領域を検査し、S-Log3の負値clampとsigned-log曲線の外挿差を残す。対象範囲内がfiniteで式に合うことを確認するだけで、実運用上の入力許容域を決めるものではない。
- **次に確認すべきこと:** 統一した入力上限／clamp方針が必要か、各曲線の呼び出し元と製品仕様を照合する。

## 2026-10-09 — Cineon / ACEScc / ACEScctのNaNと無限大

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、Cineon・ACEScc・ACEScct encode/decode。
- **確認済み:** 3曲線ともNaNをencode/decodeしてNaNを返す。Cineonは正の無限大を正の無限大へ通し、負の無限大encodeはblack codeへclamp、decodeは有限な負のblack offsetを返す。ACESccは正の無限大encodeが負の無限大、decodeは有限値となり、負の無限大encodeは黒sentinel、decodeは正の無限大。ACEScctは±∞が各式の符号で発散する。standalone suite成功。
- **価値／懸念:** 非有限入力が一様なclampにならず、各式の演算順序・sentinel・offsetの違いを持つことを記録する。非有限値を通常画像経路で許可する契約かどうかを定めるものではない。
- **次に確認すべきこと:** 非有限入力を生成する呼び出し元がある場合、曲線ごとの現挙動がアプリ側のエラー処理や画像境界と整合するか確認する。

## 2026-10-09 — 対数HDR格子で連続curveの往復誤差を確認

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`、8つの連続scene-linear transfer curve。
- **確認済み:** sRGB、Gamma 2.2/2.4/2.6、Rec.2020、PQ、HLG、DaVinci Intermediateを`1e-6`〜`1e3`の対数間隔257点で往復検査した。PQは高輝度で相対誤差約`2.1e-4`まで増え、suiteで`3e-4`上限を満たす。他7曲線は`3e-5`を満たす。最初にPQへ一律`1e-4`を要求した試行では複数点が失敗し、誤差域を測った後にPQ個別上限を設定した。
- **価値／懸念:** 少数の代表値では見えにくい暗部からHDR上端までの数値往復特性を継続検査できる。許容差はfloat実装の現行精度を測った契約で、PQの性能・精度改善を意味しない。
- **次に確認すべきこと:** PQ高輝度の誤差をST 2084の独立値と別精度実装でも評価し、必要なHDR上限と精度を決める。

## 2026-10-09 — 独立C++ moduleテストの共通登録口

- **関連:** `tests/ArtifactCore/StandaloneTestHelpers.cmake`、`tests/ArtifactCore/ColorSpaceStandalone/`。
- **確認済み:** 対象の`.ixx` interfaceと`.cppm`実装の依存閉包だけを独立CMake projectへ登録し、Artifactアプリ／全ArtifactCore libraryをロードせずGoogleTest executableを構成できる。Visual Studio 2026 / MSVC 19.51で色空間、HSV/HSL、ColorBridgeの3 suiteを実行し3/3成功。`.cppm`はMSVC/CMakeで`/interface`が付くため、helperはconfigure時にビルド領域へ`.cpp`として複製する。interface targetへのlinkでBMI参照は一度だけ供給でき、固定IFC pathの手動追加は重複解決を起こした。ColorBridgeのQColor境界にはQt Guiが必要。
- **テスト契約の修正:** PQの黒はOETF式上ゼロ入力でも約`7.31e-7`を出力する。範囲外入力のclamp確認は出力値0固定でなく、対応する端点入力との一致で検証する。
- **テスト契約の修正:** QColor文字列化は8bit/channel量子化を伴うため、10進float入力との直接比較ではなく、channel endpointの26/255・102/255に一致することを確認する。
- **次に確認すべきこと:** 別の小さなmodule依存閉包でもhelperが再利用できるか、次のstandalone test追加時に確認する。

## 2026-10-09 — Surface pixel conversion suiteの依存境界

- **関連:** `tests/ArtifactCore/SurfacePixelConversionTest.cpp`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx`。
- **確認済み:** pixel buffer変換は公開module内で完結し、依存は`Color.TransferFunction`と`Graphics.SurfaceColorContract`だけ。CMake/MSVCでArtifactCoreやQtなしのmodule targetとして登録・実行でき、既存suiteへ4つ目として追加して4/4成功。
- **価値／懸念:** CPU画像境界変換をアプリやImageF32x4実装へリンクせず、軽量で反復可能に検査できる。GPU upload/readback parityや完全な画像pipeline挙動を示すsuiteではない。
- **次に確認すべきこと:** `Color.LUT`を独立化する場合、`Core.Parallel`と`Container.NamedVector`およびそのmodule依存・実装所有者をどこまで追加せずに済むかを調べ、ArtifactCore全体への依存を避けられるtarget境界を特定する。

## 2026-10-09 — Color LUT moduleも小さな閉包で独立実行可能

- **関連:** `tests/ArtifactCore/ColorSpaceStandalone/CMakeLists.txt`、`ArtifactCore/src/Color/ColorLUT.cppm`。
- **確認済み:** `Color.LUT` の単体targetは `Core.Parallel` と `Container.NamedVector`（さらにその `Core.ArtifactArray` / optional・debug interface）を含めれば構成でき、Artifact / 全ArtifactCore / OpenImageIOには依存しない。Visual Studio 2026 / MSVC 19.51で既存LUT契約25 casesを含むstandalone suiteを実行し、全5 CTest suiteが成功した。
- **テスト境界:** 既存テストは非公開 `sample()` を呼んでいたため公開 `apply()` 経由に修正。`inverted()` はsmooth LUTに対するbounded fixed-point近似の実装である。定数写像の値に対し厳密な逆関数を期待する契約は不適切なので、テストをvalid・finite・unit rangeの出力確認に変更した。これは一般写像の逆変換品質を保証しない。
- **価値／懸念:** ファイル読み込み、保存、LUT操作、画像適用を速く反復確認できる。Qt Guiを必要とし、`applyToImage()`はQImage pathの契約であるためGPUや浮動小数点画像経路のparityを示さない。
- **次に確認すべきこと（未検証）:** smooth monotonicな非線形LUTで forward→inverse 誤差を定量化し、必要精度を仕様化する。改善実装は別途明示された作業範囲で行う。

## 2026-10-09 — Blend modeの小規模module閉包

- **関連:** `tests/ArtifactCore/ColorBlendModeStandaloneTest.cpp`、`ArtifactCore/src/Color/ColorBlendMode.cppm`。
- **確認済み:** `Color.BlendMode` は `Color.Float`、`Color.Conversion`、`Color.Luminance` の直接依存だけをstandalone projectへ加えてビルドできた。6つのカラーCTest suiteをMSVCで実行し成功。
- **契約:** `ColorBlendMode::blend` のopacityはforeground contributionをスケールする値であり、source `FloatColor::alpha` をopacityと掛けた値がforeground alpha。transparent base・source alpha 0.5・opacity 0.5の結果alphaは0.5と確認した。
- **次に確認すべきこと:** non-opaque baseと各blend modeの既知式を広げ、GPU shader側に対応modeがある場合は別途 parity suiteを検討する。

## 2026-10-09 — Blend modeの成分置換とbase alpha動作

- **関連:** `tests/ArtifactCore/ColorBlendModeStandaloneTest.cpp`、`ArtifactCore/src/Color/ColorBlendMode.cppm`。
- **確認済み:** opacityを0〜1の格子で変えたNormal合成は、透明baseを含めsource RGBとbase alphaをopacityで線形合成する。Hue/Saturation/Color/LuminosityはHSL成分のうち各mode名に対応する成分だけを置換する。Stencil/Silhouette系はbase alphaだけを変え、base RGBは維持する。
- **価値:** 単一modeの代表値に偏らず、合成カテゴリ別の振る舞いを実装式に対して独立suiteで固定できる。
- **次に確認すべきこと:** GPU側に同名modeの実装がある場合、alphaとopacityの意味を同じ入力表で別途照合する。

## 2026-10-09 — BlendMode enum全体を格子走査

- **関連:** `tests/ArtifactCore/ColorBlendModeStandaloneTest.cpp`、`ArtifactCore/include/Color/ColorBlendMode.ixx`。
- **確認済み:** 34個の宣言済みmodeを5×5 RGB・opacity gridで実行し、各出力RGBAの有限性と[0,1]範囲を検査。4つのlegacy aliasは対応canonical modeと完全一致した。MSVCでBlendMode単独CTestと全9カラーsuiteが成功。
- **価値:** 新たなenum値が追加されたとき、網羅配列への登録漏れをレビューで拾えるほか、端点付近の算術modeや分岐型modeで非有限出力を検出できる。
- **次に確認すべきこと:** BlendMode enum追加時にこの列挙テスト配列も更新する。GPU parityとは別のCPU契約である。

## 2026-10-09 — ImageSurfaceViewはColor module closureで単独実行できる

- **関連:** `tests/ArtifactCore/ImageSurfaceViewTest.cpp`、`tests/ArtifactCore/ColorSpaceStandalone/CMakeLists.txt`。
- **確認済み:** `ImageSurfaceViewTest`の依存moduleは`Graphics.SurfaceColorContract`と`Image.ImageSurfaceView`だけで、既存のsurface conversion interface targetへ加えてQtなしのstandalone targetとして実行できた。BGRA viewによるRGBA descriptor拒否、owner bufferを変更した際のread-only view観測ケースを追加し、個別CTestと全10 suiteが成功。
- **価値:** C++ moduleとGoogleTestの軽量閉包を、カラー変換以外のCPU画像境界型にも再利用できると確認した。
- **次に確認すべきこと:** 他の`Image.*View` / descriptor-only APIも依存閉包を個別に評価し、Qtや画像本体を不要とするものから同じ方式で登録する。

## 2026-10-09 — SurfacePixelConversionのalpha閾値とHDR保持

- **関連:** `tests/ArtifactCore/SurfacePixelConversionTest.cpp`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx`。
- **確認済み:** byte sRGB inputをlinear floatへdecodeし、alphaは8bit値をnormalized floatとして保持する。Premultiplied alphaが`1e-6`以下の場合RGBをゼロにし、直上ではRGBをunpremultiplyする。linear float outputは負値・HDR値をclampせず保持する。単独suiteと全10 standalone suiteがMSVCで成功。
- **価値／境界:** byte/float間で意図したtransfer境界と極小alpha扱いを固定し、display encodingのclampとscene-linear float pathを混同しにくくする。別primaries変換やGPU parityはこのAPI・suiteの対象外。
- **次に確認すべきこと:** conversion追加時はalpha modeとtarget encodingごとに同じ入力fixtureで往復・量子化誤差を計測する。

## 2026-10-09 — FloatRGBA compound演算と添字境界

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ArtifactCore/include/Color/FloatRGBA.ixx`。
- **確認済み:** add/subtract/multiply/divide compound演算とscalar演算、swapがRGBA4成分すべてへ作用する。添字0〜3は読み書きでき、-1/4はconst・mutable双方で`std::out_of_range`。default constructorのalphaは0、RGB constructorのalphaは1。Bridge suiteと全10 standalone suiteが成功。
- **価値:** 画像境界で使われる小さな値型の成分順・初期値・失敗境界を維持できる。小数の演算結果はexact equalityではなくchannel toleranceで比較する。
- **次に確認すべきこと:** 他のRGBA value typeでoperator[]やalpha defaultが同じ契約を持つかは、型ごとに明示仕様を確認してからテストする。

## 2026-10-09 — ColorBridge JSON / QColorの型別fallback

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ArtifactCore/include/Color/ColorBridge.ixx`。
- **確認済み:** FloatRGBA overloadのJSON object round-tripは4 channelを保持する。`#RRGGBB`は空白をtrimして解釈しalpha=1を使う。無効QColorはFloatColorでdefault value、FloatRGBAでは不透明黒になる。Bridge suiteと全10 standalone suiteが成功。
- **価値／境界:** RGB hexとARGB hexのalpha順序、JSON objectの型、Qt境界での異なる既定値をケース分けした。数値範囲外JSONの正規化はAPIに明示契約がなく、今回固定していない。
- **次に確認すべきこと:** JSON objectでchannel値が数値型以外だった場合の受理・fallback挙動を仕様確認し、必要なら別途契約化する。

## 2026-10-09 — ImageSurfaceView mutable row stride境界

- **関連:** `tests/ArtifactCore/ImageSurfaceViewTest.cpp`、`ArtifactCore/include/Image/ImageSurfaceView.ixx`。
- **確認済み:** 2×2 RGBA imageをrowStride=9 floats（8 channel floats + 1 padding float）でview化し、2行目・2つ目pixelへのwrite-through channel mappingと両row paddingの不変を確認。個別suiteと全10 standalone suiteが成功。
- **価値:** multi-row view利用時のbyte strideからfloat offsetへの変換、padding越境の誤書き込みを検出できる。
- **次に確認すべきこと:** BGRA padded-row mutable viewも同じ複数pixel配置で対称性を必要とする利用箇所が出たら追加する。

## 2026-10-09 — HSV/HSL固定seed property samples

- **関連:** `tests/ArtifactCore/ColorConversionTest.cpp`、`ArtifactCore/src/Color/ColorConversion.cppm`。
- **確認済み:** 固定seed LCGから4,096点のunit RGBを生成し、HSV/HSL各成分がfiniteかつ定義域内で、RGB往復が各channel 1e-5以内。ColorConversion targetと全10 standalone suiteがMSVCで成功。
- **価値:** 1,331点のregular gridとは独立の小数sampleで、sector内のround-tripとtie近傍を再現可能な形で補完する。
- **次に確認すべきこと:** toleranceまたは変換式変更時は、固定seedを維持して失敗sampleをtraceから再現できるようにする。

## 2026-10-09 — ColorSpace gamut matrixはalphaを変換しない

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ArtifactCore/src/Color/ColorSpace.cppm`。
- **確認済み:** 全7×7 color-space matrix pairでRGBAのalpha交差項は0、alpha行は`[0,0,0,1]`。任意の代表alphaに行列を適用しても値が維持されることをMSVCで確認し、全10 standalone suiteが成功。
- **価値／境界:** gamut変換はRGB primariesの3×3変換を4×4へ埋め込むため、alphaは対象外であることをmatrix構造と適用値の両方で検査できる。
- **次に確認すべきこと:** future matrix APIがpremultiplied RGBへ適用される場合、alpha不変だけでは色値の適切性を保証しないのでalpha/unpremultiply順はpipeline側で別途検査する。

## 2026-10-09 — Luminance neutral axisとclamp冪等性

- **関連:** `tests/ArtifactCore/ColorLuminanceContractTest.cpp`、`ArtifactCore/src/Color/ColorLuminance.cppm`。
- **確認済み:** 5 luminance standardsそれぞれで5段階のneutral inputをgrayscale化して元値を保つ。HSP perceptual functionもneutral axisを保つ。broadcast clampは複数finite範囲値で有限・bounds内・idempotent、NaN/±Infはlower boundに写る。個別CTestと全10 suiteが成功。
- **価値／境界:** primary単体係数からは分からない、係数の総和とchannel処理の一貫性をpropertyとして保つ。clamp testは`channelMin <= channelMax`の有効範囲に限定する。
- **次に確認すべきこと:** custom legal-range入力でlower>upperを許すかは未定義であり、別仕様なしに契約化しない。

## 2026-10-09 — TaggedColor transfer dispatchとprimaries往復

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ArtifactCore/include/Color/TaggedColor.ixx`。
- **確認済み:** TaggedColorからdispatch済み16 transfer curvesへの変換後、sRGBへ戻すRGB誤差が5e-4以内。alpha、alphaMode、primaries、known flagを検査した。Gamma22 encoded colorのRec.709↔Rec.2020 primaries往復もtransfer metadataとalphaを保持しRGB誤差1e-4以内。Bridge単独と全10 suiteが成功。
- **価値／境界:** scalar transfer functionsだけでなく、TaggedColorがmetadataを書き換えず変換を合成する経路を確認する。ACESlogはenumにあるがencode/decode dispatchがないため対象外。
- **次に確認すべきこと:** ACESlogを使うTaggedColorの意味はdispatch欠落の別実装課題と切り分け、ここでは未保証のままにする。

## 2026-10-09 — ColorLUT QImage tiled applyの全pixel境界

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ArtifactCore/src/Color/ColorLUT.cppm`。
- **確認済み:** 37×35 QImageをproductionの32×32 `Parallel::ForTiles` apply pathに通し、全1,295 pixelsでred inversion、green/blue保持、alpha保持を確認。source imageは不変。LUT単独suiteと全10 standalone suiteが成功。
- **価値／境界:** 以前の2×1 fixtureでは通らないtile分割、複数row、非tile倍数寸法、画像端の反復を同時に検査できる。QImage/8bit pathであり、float image/GPU parityは主張しない。
- **次に確認すべきこと:** LUT image applyのparallel implementationを変更する場合、tileサイズ変更を含むodd-size image fixtureを維持する。

## 2026-10-09 — ColorLUT default constructorはvalid identity

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ArtifactCore/src/Color/ColorLUT.cppm`。
- **確認済み:** default `ColorLUT`に`isValid()==false`を期待する仮テストは失敗した。production construction pathは有効なidentity LUTを提供する。無効LUTのno-op検査には不完全なCUBE fileを読み込むfixtureが必要。非有限float入力は有効identity LUTでゼロへsanitizeされる。LUT単独と全10 suiteが成功。
- **価値:** test fixtureの前提違いで実API契約を誤って捉えないよう、default-constructed valueとparse failureを区別する。
- **次に確認すべきこと:** 無効状態はload/parse error経路でのみfixtureし、default valueの契約変更時は関連default testsと合わせて更新する。

## 2026-10-09 — TaggedColor unpremultiply epsilonはstrict greater-than

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ArtifactCore/include/Color/TaggedColor.ixx`。
- **確認済み:** premultiplied alpha 0および1e-6以下はstraight化時にRGBを黒へし、`nextafter(1e-6,+inf)`では除算して元RGBを復元する。stored alphaは保持する。Opaque alpha modeは`premultiplied()` / `straight()`の双方で完全no-op。Bridge単独と全10 suiteが成功。
- **価値／境界:** 透明近傍でのゼロ除算回避閾値とalpha-mode idempotenceを代表値ではなく隣接浮動小数点値で固定する。
- **次に確認すべきこと:** threshold変更は既存透明画像の色復元可否を変えるため、意図的な契約変更としてこの境界を更新する。

## 2026-10-09 — ColorLUT applyWithIntensityはalphaを固定してRGBを混合

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ArtifactCore/src/Color/ColorLUT.cppm`。
- **確認済み:** QColor pathでintensity 0〜1の5点を検査し、RGBはoriginalとLUT resultの線形補間、alphaはsource alpha維持。LUT単独suiteおよび全10 suiteが成功。
- **価値／境界:** 中間値1点だけでなく両端と中間密度で混合式を固定した。intensity範囲外はAPI契約が明示されていないため対象外。
- **次に確認すべきこと:** QColor内部の量子化差を踏まえ、色成分の比較は2e-4許容を使う。

## 2026-10-09 — ColorHarmonizer hue rotation preserves HSV S/V

- **関連:** `tests/ArtifactCore/ColorHarmonizerContractTest.cpp`、`ArtifactCore/src/Color/ColorHarmonizer.cppm`。
- **確認済み:** Hue 350°、S=0.72、V=0.63、alpha=0.38から5 hue-rotation schemesを生成し、expected hue wrap、HSV saturation/value、alpha保持を確認。Monochromatic count=-3はempty。単独suiteと全10 suiteが成功。
- **価値／境界:** RGB原色では露呈しにくい色相0° crossingと非最大S/Vで、Hue以外のHSV軸が不変であることを検査する。
- **次に確認すべきこと:** angle引数の有効範囲や非常に大きい負角度のwrap契約は未定義のため、仕様確認なしには固定しない。

## 2026-10-09 — ColorSpace transfer gridsとPQ float誤差

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ArtifactCore/src/Color/ColorSpace.cppm`。
- **確認済み:** Linear / sRGB / Gamma22 / Gamma24 / Gamma26 / PQ / HLGで257 normalized samplesを走査し、出力finite・unit range・単調非減少・round-tripを確認。PQの最大round-trip差は約7.9e-5で、1e-4 tolerance内。個別ColorSpace suiteと全10 suiteが成功。
- **価値／境界:** 6 representative pointsでは見えない全域の低次元形状と曲線境界の誤差を固定できる。GammaFunction enumのRec709 / Rec2020はこのconverter実装で現状passthrough defaultなので「implemented transfer curves」配列には含めない。
- **次に確認すべきこと:** PQのfloat32誤差がcompiler/backendで大きく変わる場合は誤差分布と式の精度を調べ、無根拠にtoleranceを広げず参照実装と比較する。

## 2026-10-09 — BlendMode全enum opacity saturation boundaries

- **関連:** `tests/ArtifactCore/ColorBlendModeStandaloneTest.cpp`、`ArtifactCore/src/Color/ColorBlendMode.cppm`。
- **確認済み:** 34 modes×RGB gridでopacity=-0.5/0/1/1.5を含む7点を適用。opacity<=0はbaseと完全一致し、opacity>=1はclamped full-opacity resultと一致。unit bounds・finiteも継続検査。単独suiteと全10 suite成功。
- **価値:** 既存の中間opacity domain testに加え、enum全体のclamp端点を同時に固定する。
- **次に確認すべきこと:** opacity NaNの動作はstd::clampの扱いに依存するため、製品契約が必要になるまではテスト対象にしない。

## 2026-10-09 — Color Luminance単体suiteはmodule一つで成立

- **関連:** `tests/ArtifactCore/ColorLuminanceContractTest.cpp`、`ArtifactCore/include/Color/ColorLuminance.ixx`。
- **確認済み:** luminance / perceptual brightness / grayscale / broadcast safe inspectionとclampは単一module interfaceとimplementationでstandalone CTest targetにできる。QtやArtifactCore全体を不要とし、MSVCで実行した。
- **契約上の境界:** `inspectBroadcastSafe` はコメント上も全Y'CbCr matrix gamut mapperではなく、選択された輝度係数と独立のper-channel legal範囲検査である。テストは両判定を分けている。
- **次に確認すべきこと:** compositionやimage importでの色解釈、GPU monitor pathとの一致を主張するには別の統合境界テストが必要。

## 2026-10-09 — Color Harmonizerの色相suiteはQt Coreだけで独立可能

- **関連:** `tests/ArtifactCore/ColorHarmonizerContractTest.cpp`、`ArtifactCore/src/Color/ColorHarmonizer.cppm`。
- **確認済み:** `Color.Harmonizer` は `QList`、`Color.Float`、`Color.Conversion`で構成でき、standalone targetは全ArtifactCoreをリンクせずMSVCで通過した。補色・類似色・三補色・四補色・分裂補色・単色配色の5テストを追加し、全8カラーsuite成功。
- **境界:** 色相は色差がない黒・グレーでは不定となる。monochromatic色相維持テストはvalueのwrapが黒点を作らないcountで検査する。
- **次に確認すべきこと:** 無彩色入力に対して調和色生成が維持すべき振る舞いを別途仕様化すると、Hue=0扱いの現状を契約化すべきか判断できる。

## 2026-10-09 — ACESのヘッダ実装も既存color module closureを使って独立検査可能

- **関連:** `tests/ArtifactCore/ColorACESContractTest.cpp`、`ArtifactCore/include/Color/ColorACES.ixx`。
- **確認済み:** ACES managerのAPIは既存のColorSpace / gamut conversionとTransferFunction interfaceだけでcompileでき、standalone projectに登録後、CTest全9 suiteが成功。
- **境界:** `applyOutputTransform`内のRRTはproduction sourceコメントにある簡易filmic近似であり、標準Academy ACES RRT+ODTとの一致を示さない。suiteは線形入力変換関係、黒、有限出力、soft clippingなどの現行API契約に限定した。
- **次に確認すべきこと:** ACEScg/ACEScctのworking-space分岐が実装上同じAP1に写されている点を、利用側の期待仕様と照合する。今回のテスト追加では変更していない。

## 2026-10-09 — FloatRGBA単体演算はalphaも通常成分として処理する

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ArtifactCore/include/Color/FloatRGBA.ixx`、`ArtifactCore/src/Color/FloatRGBA.cppm`。
- **確認済み:** standalone ColorBridge libraryにFloatRGBA実装を加え、RGBA四成分の加算・乗算・scalar multiplication、alphaを含むlerp/clamp、FloatColor変換をテストした。CTest全9 suite成功しBridge suiteは18 cases。
- **境界:** 浮動小数点演算結果は許容誤差で比較し、lerp端点のみ完全一致を確認する。
- **次に確認すべきこと:** `setFromFloatColor`宣言に対応する実装が見当たらないため、呼び出し元の有無とAPI契約を別途調べる（未検証）。

## 2026-10-09 — RGB cube gridでHSV/HSL往復を検査

- **関連:** `tests/ArtifactCore/ColorConversionTest.cpp`。
- **確認済み:** [0,1]³を各軸11点に分けた1,331 RGB sampleでHSV/HSL round-tripを検査し、両方の色相表現で全channel誤差は1e-5以内。HSL primary / secondary / full-turnの7境界も個別期待値と比較し、MSVC上のColorConversion suite・全カラーCTestが成功。
- **価値:** 少数の代表色だけでは通りにくいsector分岐やmax/min channel tieを含め、unit cube全域の標本契約を継続監視できる。
- **次に確認すべきこと:** float16境界・ランダムproperty testingを追加する場合は固定seedと失敗時の入力表示を維持し、再現性を確保する。

## 2026-10-09 — ColorSpace全matrix pairはHDR/負値も往復できる

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ArtifactCore/src/Color/ColorSpace.cppm`。
- **確認済み:** 7 ColorSpace値の全49 source/destination pairについて、7 RGB sampleをforward + reverse matrixで復元するテストを追加。黒・白・RGB primaries・mixed color・HDR/negative channelを含み、matrix要素finite、alpha diagonal=1、RGB誤差1e-3以内をMSVCで確認。全9カラーCTest suite成功。
- **価値／懸念:** 色域外値をclipしないlinear primary conversionの契約と、同義primariesを持つspace aliasのmatrix安定性を広く押さえる。
- **次に確認すべきこと:** 既知の別実装や標準行列へのreference checkはRec709→Rec2020以外にも追加できるが、white-point adaptationを含む期待値の出典を明確にしてから固定する。

## 2026-10-09 — TransferFunction enum/dispatch gapとACEScc zero decode mismatch

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`tests/ArtifactCore/ColorBridgeTest.cpp`。
- **確認済み:** dispatch済み15 transfer曲線を5つの正値でencode/decode往復し、Rec.709/2020 toe slopeとACEScc/ACEScctのlog blackをテストした。全9カラーsuite成功。`TransferFunction::ACESlog` enum値は汎用encode/decode switchに未接続。
- **確認済みの不一致:** `linearToACEScc(0)` は`-0.3584474886`を返すが、現行`acesccToLinear`へ渡すと131072になる。該当テストは誤った往復を期待せず、値をInsightへ報告するだけに留めた。通常のpositive sampleはround-tripテストを通る。
- **価値／懸念:** wide-gamut/HDR/log curve APIで宣言済み関数とdispatcherの完全性、特例境界の不一致を可視化した。ACEScc fixとACESlog実装は利用仕様・標準値照合が必要で、今回はsourceを変更していない。
- **次に確認すべきこと:** ACESccの正式な負値/zero decoding ruleとACESlog transfer curveの仕様・命名を一次資料から確認して、対応とテストを個別に判断する。

## 2026-10-09 — Luminance standard endpointとlegal-range境界

- **関連:** `tests/ArtifactCore/ColorLuminanceContractTest.cpp`。
- **確認済み:** 5 luminance standardそれぞれの黒=0/白≈1、red/green/blue単独寄与、各channel増加に対する単調性、broadcast legal black/white inclusive endpoints、NaN時にluma/gamut両違反となる契約を追加。MSVCで該当targetと全9 suiteが通過。
- **境界:** monotonicityは[0,1] channel samplesのみ。negative/HDR input値でのluminance自体は線形重み式だが、broadcast legal-range suitabilityとは別契約。
- **次に確認すべきこと:** `calculatePerceptual`のHSP近似は色管理lumaではないため、用途別呼び出し点に混同がないかは将来の別監査対象。

## 2026-10-09 — LUT inverse approximationはmapped range内で誤差評価

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ArtifactCore/src/Color/ColorLUT.cppm`。
- **確認済み:** 非可換linear LUT合成で`receiver`を先に、`argument`を次に適用する契約を追加。単調な縮小linear channel mapsに対してmapped range内の3サンプルforward→inverseを検査し、各channel誤差は0.01以下。MSVC全9 suite成功。
- **境界:** `inverted()` はoutput cube全体へ逆写像を構築するfixed-point近似のため、元LUTが覆わないrangeの逆値は定義できない。テストサンプルはmapped range内に置く。
- **次に確認すべきこと:** 非線形で相互channel mixingがあるLUTの収束・誤差を別途特徴付ける。

## 2026-10-09 — LUT file dispatchとheaderless 3DL branch

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ArtifactCore/src/Color/ColorLUT.cppm`。
- **確認済み:** `.CUBE` suffixの大小文字を無視したdispatch、未対応`.look`拡張子の明示エラー、ヘッダーなし3DLのデータ数から立方rootを推定する分岐をテストした。MSVC全9 suite成功。
- **境界:** `LUTFormat` enumはMga / Lookも列挙するが、file dispatchはcube/csp/3dlとPNG/JPG/TIFF画像のみ。今回、unsupported extensionが失敗する現在の挙動を確認し、追加形式の実装は行っていない。
- **次に確認すべきこと:** `tif` aliasやMga/Look file formatsをサポート対象にする必要があるか、利用要件と照合する。

## 2026-10-09 — 起動時先行投入で初回CPU期間ごと消去

- **関連:** `ParticleRenderer::prewarmCommonPipelines`、`ArtifactIRenderer::Impl::initialize` 末尾。
- **今回の対応:** 初期化完了時に共有particle rendererを先行生成し、常用2系統（Additive既定＋Alpha）＋カリングをワーカーへ投入する。層到着時にはキャッシュ済みのため、非同期待ちのCPU期間自体が発生しない。失敗・キャッシュ済みは投入除外で冪等。
- **未検証:** ビルド・実機未実施。
- **次に確認すべきこと:** 起動直後の層追加で待機診断が出ないこと、バックグラウンドコンパイル中の操作性を確認する。



## 2026-10-09 — 外部動画プロキシの登録境界

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`（proxy controller 切替と `sourceFrame` 要求）、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`（固定 cache path と生成 queue）、`Artifact/src/Worker/ArtifactProxyWorker.cpp`（生成専用 request protocol）。
- **確認できた事実:** `ArtifactVideoLayer` は proxy path と品質を保存し、独立 controller で proxy を開ける。Project View は `.proxy/<basename>_proxy_<quality>.mp4` の固定 path を参照する。現在の切替は元動画の source frame index を proxy controller に渡す。Worker はプロキシ生成用で、外部ツール成果物を登録する protocol／Project View 操作はない。
- **推測・未検証:** 外部生成動画の尺・フレームレート・開始PTS・VFR timestamp が素材と異なる場合、同一 frame index の参照では時間同期がずれる可能性が高い。任意 path 登録、ストリーム情報検査、ソースと proxy の対応検証を持つ明示的な import contract が必要。
- **価値または懸念:** 規定名への手動配置だけで試行できる一方、同 basename の衝突、stale 判定、音声／色差、非 CFR の同期ずれを検知しない。既存生成 worker を汎用外部 proxy plugin と誤認して利用すると、要求した生成レシピや外部実行結果を管理できない。
- **次に確認すべきこと:** ユーザーの想定する外部プログラムと必要な自動化範囲を確認し、任意 proxy path を永続化するか、規定 cache path へ取り込むかを選んだ上で、manifest／probe 検証・同期条件・置換復旧を設計する。

### 実装候補（外部 proxy 連携の契約）

- **案:** Project View に外部 proxy の登録入口を設け、元 footage と出力動画を指定する。登録前に ffprobe 相当のメディア情報を読み、video stream、寸法、rational frame rate、duration/frame count、start time、pixel/color tags、audio stream の有無を source snapshot と照合する。厳密な frame-for-frame 対応が保証できない VFR／retime ケースは自動承認せず、外部ツール manifest の source hash または frame timestamp map を要求する。
- **運用境界:** 選択肢は (A) 出力を Studio 管理 `.proxy` cache へ原子的に copy/move して既存の固定パス・clear・rebuild UI を再利用する、(B) FootageItem に任意 proxy path + manifest を保存して外部場所を参照する、の2つ。A は既存統合を再利用しやすいが同名衝突・追加容量・生成毎のコピーがある。B は外部ツールの出力管理に自然だが、Project/View status、clear/relink、save/load、proxy metadata を一貫して拡張する必要がある。
- **実装順:** まず A の「外部出力を規定 cache path へ import + probe + atomic replace + 全composition反映」を MVP とし、同期問題を解決した後で B の外部パス永続化を検討する。worker protocol は生成用として維持し、任意の外部 executable 起動機能には一般化しない。
- **時間軸要件:** 既存 decoder が元 source frame index を proxy に直接渡す事実から、CFR の fps と開始時刻一致だけでは VFR の厳密同期を証明できない。可能なら proxy の PTS/frame sequence を source と同数に保持し、失敗時は「proxy に切替えず full source に戻す」。audio は preview上で必要かを用途ごとに決め、必要ならストリームの存在・開始時刻・duration を検証する。
- **注意:** 以上は調査に基づく設計案で未実装。ユーザーが想定するツール名、手動生成か自動起動か、copy/move/外部参照の選好が固まったら、それに合わせて入口と運用を決める。



## 2026-10-08 — 粒子PSOの非同期コンパイル化（追加時フリーズ解消）

- **関連:** `ArtifactCore/src/Graphics/ParticleRenderer.cppm`（ワーカー・キャッシュ・prepare）、同`include/.../ParticleRenderer.ixx`（宣言・`PrepareWaitingPipeline`）、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`（待機中の警告抑制）。
- **確認できた事実:** 層生成自体は軽く、初回描画のGUIスレッド同期dxc×3本がフリーズ原因だった。カリング遅延化だけではVS＋PSの2本が残るため、バックグラウンド1スレッド＋ジョブ合流＋結果スロット＋オプション別PSOキャッシュ（8件）に変更。ビルド中は既存CPUフォールバックで描画し、完成フレームからGPUへ無継ぎ目移行する。失敗ラッチ・世代タグ・ pending 取消で再試行嵐と陳腐結果を防ぐ。ワーカーはデバイス生成系のみ触り、結果反映はGUI側pumpに限定。
- **未検証:** ビルド・実機未実施。Diligentデバイス生成系のスレッド安全性は設計前提（要実機確認）。残リスクは静的目視のみ。
- **次に確認すべきこと:** ビルド後、層追加の固まり有無、初回数十フレームのCPU→GPU切替、ブレンド混在層でのキャッシュヒット、待機診断 `prepare-waiting-pipeline` の出方を確認する。



## 2026-10-08 — レイヤー追加時の固まりは初回描画の同期シェーダコンパイル

- **関連:** `ArtifactCore/src/Graphics/ParticleRenderer.cppm`（`createPSO`／`ensureCullPipeline`／`prepare`）。
- **確認できた事実:** 層生成自体は軽い（fireプリセット割当のみ、シミュなし）。追加直後の初回描画で `initialize(100000)` → VS＋PS＋カリングCSの3本をGUIスレッドで同期dxcコンパイルしていた。カリング用は64粒子以上＋Additiveでしか使わないのに全層で払っていた。
- **今回の対応:** カリングパイプラインを初回使用時まで遅延ビルド化（`ensureCullPipeline`、成功時1回きり・失敗ラッチ付き、バッファ再生成でリセット）。ついでにブレンド切替毎のカリング再コンパイルも消えた。残り2本（VS＋PS）は描画に必須のため初回に残る。
- **未検証:** ビルド・実機未実施。完全な解消には非同期／事前コンパイルが必要だが、スレッド・状態機械の構造変更になるため未着手。
- **次に確認すべきこと:** 固まる層種・初回か毎回か・秒数を聞き取り、残り2本の事前コンパイルかプロパティ／タイムライン側かを切り分ける。



## 2026-10-08 — 粒子GPU第一経路化の完成確認（viewport全経路）

- **関連:** ビューポート粒子分岐（`drawLayerForCompositionView`）、サムネイル（`renderLayerSurface`／`getThumbnail`）、書出キュー、デバッグハーネス。
- **確認できた事実:** 本番viewportの粒子ラスタは全てGPU優先になった。通常→`draw()` 直接、マスク／エフェクト付き→GPU面＋既存CPUエフェクト、失敗・未初期化時のみCPU。サムネイル系（`renderLayerSurface`）は粒子分岐自体がなくプレースホルダ表示のためGPU化対象なし。書出キューはGPU readback経路。CPUに残るのはフォールバック・診断ハーネス（GPU正当性の参照用に残す）・ソフトウェアテスト用ウィジェットのみ。
- **次に確認すべきこと:** ビルド許可後に実機で通常／エフェクト付き／trail／stretchの各表示とフォールバック到達性を確認する。現時点ではビルド・実機未実施。



## 2026-10-08 — GPU統一へ向けた機能差埋め（trail／ストレッチ見た目）

- **関連:** `ArtifactCore` の `ParticleVertex`（96バイト化）・`ParticleRenderer.cppm` のVS／PS、`Artifact/src/Layer/ArtifactParticleLayer.cppm`（`emitParticleTrails`／`draw`／`drawSurfaceGPU`）、各層の頂点生成箇所。
- **確認できた事実:** ライブCPU／GPU間の実機能差は trail のみだった。テクスチャflipbookとdepth-softはライブCPU側にも実装がなく（死んだ `updateAndRenderSoftwareFrame` にだけある）、差分ではなかった。stretchは両方あるが形状が別物（GPU楕円 vs CPU角丸 rect）だった。
- **今回の対応:** (1) Core頂点へ `prevPosition`（float4、シェーダはstride用のみ）を末尾追加し96バイト化。static_assert 更新、全生成箇所は prev=pos・積分／写像箇所は prev 追従。(2) GPU trail を line パケットで送出（`draw`／`drawSurfaceGPU` の accept 確認後のみ。棄却時に先送りするとゴースト化するため）。trail幅は写像スケール追従、trail色・fadeはCPUと同一式。(3) PSにストレッチ分岐を追加し、CPUと同ストップの垂直グラデ＋カプセルSDFで角丸rect化。円形経路は無変更。
- **確認できた事実:** 実シェーダ3本をdxcでDXIL＋SPIRV両コンパイル成功。DXIL／SPIRVともオフセット (0,16,32,48,52,56,60,64,68,72,76,80)・96バイトでC++と一致。
- **意図的な見た目差:** GPU trail はバッチの都合でスプライトより後に合成される（加算では等価、αではCPUと重なり順が違う）。線端は butt cap（CPUは round）。GPU面色はワーキング系のまま。
- **未検証:** ビルド・実機未実施。3D分岐のtrailは未対応（従来通りなし）。テクスチャ粒子のGPU対応は両経路とも未実装のため対象外。
- **次に確認すべきこと:** ビルド後、trail有効プリセットのGPU表示、ストレッチ有効時のGPU／CPU見比べ、Vulkanでの96バイト描画を確認する。



## 2026-10-08 — ソフト粒子の律速3点とGPUサーフェス経路・スプライト化

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm`（`ParticleSystem::render`）、`Artifact/src/Layer/ArtifactParticleLayer.cppm`（`renderFrame`／`drawSurfaceGPU`）、`Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm`（`renderParticleSurfaceToQImage`）、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`（粒子分岐）。
- **確認できた事実:** CPUフォールバックの律速は (1) フレーム毎のフルHD QImage再確保（約8MB）、(2) 画面外粒子を含む全粒子への毎回グラデーション構築、(3) 透明・ゼロサイズ粒子の無駄な painter 処理だった。(1) は cachedFrame 再利用、(2) はデバイス座標の境界円カリング（trail有効時は線分が画面を横切るため停止）、(3) は先頭スキップで対応。いずれも出力不変。
- **今回の対応:** 形状別（円形／ストレッチ）・整数サイズ・厳密8bit RGBAをキーにしたスプライトキャッシュを追加（上限 4Mpx／1024件のLRU、512px超は直接描画）。意図的な見た目差は整数丸め±0.5pxと非等倍時のリサンプルのみ。回転はストレッチ側だけ維持（円形は対称のため省略しても同一）。
- **今回の対応:** マスク／ラスタライザエフェクト付きでもGPUを使う経路を追加。層ローカル面への写像だけを変えた `drawSurfaceGPU`（不透明度は合成側に委譲、3DはCPUへ返す）を、永続オフスクリーンヘルパー（層ID×デバイスで保持、16件超で破棄、mutex直列化）経由で描画し、readback後に既存CPUエフェクト処理へ流す。`readbackToImage()` がキュー submit を内包するため追加の submit 配線は不要。4096px超・失敗時はCPUへフォールバック。
- **未検証:** ビルド・実機未実施（指示待ち）。残リスクは (a) 新規 `import Artifact.Layer.Particle` によるモジュール循環（静的目視では往路なし）、(b) GPU面とCPU面の色の出自差（GPU面はワーキング系、CPU面はsRGB。ビューポートGPU描画とは一致方向）、(c) 同一層への viewport／offline 並行描画は従来から競合しうる（ParticleSystem の resume 状態共有）。
- **次に確認すべきこと:** ビルド後、エフェクト付き粒子層でGPU面が選ばれること・CPUフォールバックとの見た目差・スプライト有無でのfpsを測定する。



## 2026-10-08 — 2D粒子GPUの色反転・明滅3件修正（stride／smoothstep／カリング）

- **関連:** `ArtifactCore/include/Graphics/ParticleData.ixx`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`（VS／カリングCS／PS）、`Artifact/src/Layer/ArtifactParticleLayer.cppm`（static_assert）。
- **確認できた事実:** 旧 `ParticleData`（float3 position＋float3 velocity）は DXIL では72バイト・タイトだが、Vulkan向けSPIRV（std430）では vec3 が16バイトアラインで80バイトになることを、VulkanSDKのdxc＋spirv-disで旧構造・新構造の両方を実測して確認した。C++側72バイトのままだとVulkanでは先頭粒子から color が (b,a,size,stretch) 位置ずれで読まれ、オレンジがシアン系に・size が rotation 値（0〜360乱数）を読んで巨大円 flicker になる。CPU送信が橙でPSが素通しという既存観察とも整合する。
- **今回の修正:** C++ `ParticleVertex` に明示padding2個で80バイト化、HLSLのVS／カリングCSを `float4 position/velocity`＋`.xyz` 参照化、static_assert を80バイト系へ更新。実シェーダ3本をdxcでDXIL＋SPIRV両コンパイル成功、SPIRVオフセット (0,16,32,48,52,56,60,64,68,72,76) がC++と一致することを確認。`sizeof` 追従のためバッファ stride／snapshotバイト数は自動追従、全使用箇所はフィールド単位代入のためpaddingは0のまま。
- **確認できた事実:** PS の `smoothstep(0.5, 0.4, dist)` は edge0>=edge1 でHLSL未定義動作のため `1.0 - smoothstep(0.4, 0.5, dist)` に修正。カリングマージン `*6.0` は描画半幅 `size*10` より小さく画面端でpopするため `*10.0` に合わせた。
- **未検証:** プロジェクトのビルド・実機確認（ユーザー指示待ちのため未実施）。`ParticleCompute.cppm` 側の独自 `ParticleData`（float3連続・別バッファ）は今回対象外で、同種のVulkanずれリスクが残る。
- **次に確認すべきこと:** ビルド許可後に static_assert 通過、D3D12とVulkanの両方で fire 系オレンジ描画・バースト系の明滅有無・画面端pan時のpop有無を目視確認する。



## 2026-10-08 — Soft Body を独立 Physics Testbench で先行検証

- **関連:** `ArtifactCore/src/Physics/SoftBodySolver.cppm`、`ArtifactCore/src/Physics/PhysicsSystem.cppm`、`tests/ArtifactCore/PhysicsDeterminismTest.cpp`。
- **確認できた事実:** SoftBodySolver は固定 timestep、snapshot/restore、collider 等を持ち、PhysicsSystem は layer ID をキーに solver を所有する。既存の ArtifactCore テストには決定性・重力・減衰の契約確認がある。制作向けの Soft Body / Cloth マイルストーンは GPU deformation や bake parity など別の未完了範囲を持つ。
- **追加確認:** `enableSoftBodyPhysicsGrid()` は `localBounds()` から独立した格子を作るので、元の平面ポリゴンの頂点数はsolver格子の密度を制限しない。一方、現在 `drawSoftBodyGrid()` を使って変形格子を描くのは `ArtifactShapeLayer` だけであり、SolidImage/Image系へ同じ変形描画が通る根拠は見つからない。
- **気づき:** 物理を独立して試したい要望に対して、PhysicsSystem/Compositionへ実験用状態を混ぜるより、solverを専有する独立テストベンチを設けると、制作レイヤーの複雑さから切り離して調整・再現できる可能性がある。
- **価値または懸念（未検証）:** GUI上で条件を変えて動きを観察できれば、solverの体感調整と再現可能な検証条件を両立できる。一方、UI登録先、描画方法、条件ファイル形式は調査・設計が必要。
- **次に確認すべきこと:** 独立ドック/ツールウィンドウの既存登録経路、Solver APIの初期化/reset契約をコードで確認し、P0設計を固める。



## 2026-10-08 — Spatial Room/初期反射/残響の物理化設計（M-AU-9.7具体化）



## 2026-10-08 — 空間画像エフェクトの独立CPU契約テスト

- **関連:** `tests/Artifact/SpatialImageEffectContractTest.cpp`、`ArtifactEffectsBlur`、`Artifact.Effect.Rasterizer.ApertureShapeBlur`。
- **確認できた事実:** `ArtifactEffectsBlur` は独立した静的ライブラリで、Aperture Shape Blur のCPU implementationを所有する。画像をぼかすとRGBは近傍へ広がり、実装はalpha planeをそのまま出力へ戻す。
- **対応:** 個別GTest targetでRGBの広がり、alpha保持、入力不変、出力寸法を固定する契約テストを追加した。独立画像エフェクトオプションから既存のCore creative effect suiteも単独登録するようにした。実行は未確認。
- **価値または懸念（未検証）:** 点処理の露出・色補正、Coreのcreative effect群に加え、近傍サンプリングのCPU処理もArtifact本体のテスト起動入口に依存せず検査できる。既存 `BlurEffect` は専用 `ArtifactEffectsBlur` targetの所有範囲外のため、このsuiteでは対象にしない。
- **次に確認すべきこと:** ビルド後にこのtargetを単独実行し、FFT畳み込みの端部挙動と複数入力alpha descriptorでalpha保持を確認する。



## 2026-10-08 — Fuzz 専用入口と編集 runtime の独立化は別段階

- **関連:** `Artifact/src/AppMain.cppm`、`Artifact/src/Test.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`、Artifact CMake target 構成。
- **確認できた事実:** ProjectService と UndoManager は Artifact 実行ファイル側の実装ソースにあり、ルートのテスト runtime target からリンクされない。既存 ArtifactCore / Artifact のライブラリ分割にも、この編集サービス群を含む独立 runtime target はない。
- **価値または懸念（未検証）:** fuzz を独立 opt-in 起動入口にすれば通常 built-in tests の失敗や実行時間から切り離せる。一方、完全な独立ビルドには編集・Undo の実装所有権と依存グラフのライブラリ化が必要となり、単なるテスト移動では成立しない。
- **次に確認すべきこと:** 専用起動入口で fuzz だけを実行して通常 suite を呼ばないことをソースと実行で確認する。将来独立 runtime target が必要なら、編集 API が所有する最小モジュール群と既存 Artifact target との共有方法を依存グラフから設計する。
- **2026-10-08 実装結果:** root CMake の `ArtifactCoreAnalyze` source property で `ImageAnalyzer.cppm` が必要とする `Image.ImageSurfaceView`、その transitive module imports、および interface object の順序依存を補うことで、ArtifactCore 子リポジトリを変更せず Analyze module error を越えられた。Artifact 全体は別の既存 `ArtifactPropertyEditorNumeric.cppm` の Qt API compile error で停止する。fuzz / TestRunner / AppMain の各 object compile は成功したが、exe link と runtime は未検証。
- **2026-10-08 実行確認:** Asset reload の画素テストは、ランダムに選んだ変更色が前回と同じになる可能性があったため、直前画素に基づく赤 / 緑交互選択へ変更した。6144 操作の3 seedすべてが不変条件検証を完了し、wrapper return trace まで記録するが、その後のプロセス終了で Access Violation が発生してCTestが失敗する。通常 suite から独立したCTest / 起動入口は動作している。アプリ終了クラッシュの正確な破棄箇所と専用runtime targetの必要性は未検証。
- **2026-10-08 cleanup 調査:** Project close 後に CompositionRegistry 名が残ることを確認した。Active Context / Selection を解除し、Playback Service が Composition の null 化を playback engine まで伝えるよう修正すると、6144 操作の CTest は正常終了した。registry 残留は trace に診断情報として残し、次 seed へ持ち越さないよう seed ごとに別 process で起動する CTest に分割した。分割後の3 CTest は3/3成功、合計17.57秒で終了した。



## 2026-10-08 — TextLayer animator integration tests can use the built-in app runner

- **関連:** `Artifact/src/Test.cppm`、`Artifact/src/Layer/ArtifactTextLayer.cppm`、`tests/Artifact/TextGlyphRenderContractTest.cpp`。
- **確認できた事実:** `ArtifactTextLayer` はArtifactアプリ実行ファイルのモジュールであり、`Artifact.TestRunner::runAllTests()` が `ARTIFACT_RUN_BUILTIN_TESTS` 起動経路から呼ばれる。レイヤーを直接生成し、Animator property pathを設定して `updateImage()` / `currentFrameBuffer()` を読むことができる。
- **価値:** Coreのglyph render contractに加え、レイヤー公開API、画像ラスタライズ、Animator stack snapshot restore、プロジェクトJSON round-tripを同じアプリ内テストでつなげて検証できる。テストにselectorを先頭glyphへ絞る操作を加え、選択glyphの移動はalpha重心で、追加したopacity Animatorの描画反映は総alphaの低下で判定する。複数Animatorの復元後はalpha画像全体の各pixelも比較する。
- **確認できた事実:** 既存のArtifactCoreテスト実行ファイルはvcpkg Debug DLLへのPATHがない状態だと起動できず、Debug binをPATHへ加えると起動する。ArtifactCore Animator実行ファイルは現行ソースより古く、`--gtest_list_tests` の63ケースとソース中の62 test macroに1ケース差があり、現行ソースをまだ検証できていない。
- **2026-10-08 テスト結果:** PATHを補って既存animation CTest 6件を2回、text CTest 4件を3回実行し成功。ArtifactCore shaping/rasterと既存GPU CTest実行ファイルは通過した。GPU画像テストの現行ソースは直接コンパイル・リンクした更新版で **7/7 passed**、重ねたposition/opacity Animatorケースも単独5回成功。GPU readbackは `ArtifactTextRenderTarget::readback()` 内で `WaitForIdle()` を実行する。
- **2026-10-08 再開後:** 現行ソースと一致しない既存Animator実行ファイルは63件を3回実行して成功したが、現行ソースにない古いケースを含むため、現行ソースの証拠とはしない。ソース更新後に作成されたShaping／Glyph Raster実行ファイルは各3回成功、RenderTarget GPU contractも3回成功。現行GPU GlyphRender実行ファイルは7/7件を2回成功。
- **2026-10-08 再開監査:** 生成済みビルドにある現行ソース対応の `ArtifactCoreTextShapingTest`、`ArtifactCoreTextGlyphRasterTest`、`ArtifactTextRenderTargetContractTest` を再実行し3/3成功。`ArtifactTextGlyphRenderContractTest.updated.exe` も7/7成功。これはCore／Glyph GPU画像経路の確認であり、今回追加したArtifactアプリ統合CTestの実行証拠ではない。
- **2026-10-08 追加:** Artifact組み込みレイヤーテストに opacity Animator を追加し、position＋opacity のstack適用後の実ラスタライズalpha、stack snapshot復元、project JSON round-tripを確認する。position／opacityともselector unitsをIndexとして明示し、end=0で先頭glyphを選ぶため、percentage domainとfixtureの文字数には依存しない。ユーザーの指示によりビルドせず、現行ソースのruntime検証は未実施。
- **独立実行についての確認・実装:** `ArtifactTextLayer.cppm` は `Artifact` 実行ファイル側の広いモジュール依存を持つため、専用の小さなテスト実行ファイルへ直接リンクするには本体側の依存分離が必要。一方、親CMakeは `Artifact` を定義した後でテスト設定に入る。専用環境変数でTextLayerケースだけ実行するアプリモードと、GTest検出前に登録する `ArtifactTextLayerAnimatorIntegrationTest` を追加した。さらに `ARTIFACT_ENABLE_TEXT_LAYER_ANIMATOR_TEST=ON` で全GTestスイートを有効にせず登録できるようにした。これはCTest上では個別選択できるが、独立バイナリではなくArtifactアプリを起動する統合テストである。CMake再生成・ビルド・実行はユーザーの指示により未実施。
- **2026-10-08 レイヤー独立バイナリの依存調査:** `ArtifactTestAdjustmentLayer.cppm` / `ArtifactTestLayerGroup.cppm` / `ArtifactTestShapePath.cppm` / `ArtifactTestSolidLayer.cppm` と対象レイヤー実装は `Artifact/cmake/ArtifactSources.cmake` の `APP_IMPL` / `APP_MODULES` にあり、専用 `ArtifactLayerRuntime` ライブラリは存在しない。特にGroup/Solid/Adjustmentの現行契約は `ArtifactLayerFactory` を使うため、factory実装と複数レイヤーの登録依存も加わる。現状の選択的なCTest入口はテストごとにArtifactプロセスを分けるが、専用バイナリ化にはproduction module ownershipと依存閉包の分離が必要。APPソース全体を複製するテスト実行体は独立性の代わりに大規模な重複ビルドを招くため、次段階では採用しない。
- **未確認:** 現行 `TextAnimatorContractTest.cpp` のビルド・実行、およびArtifact組み込みテストのリンク・実行。生成済みNinjaのドライランでは対象ビルドの前にCMake再実行が必要と出たため、再生成を行ってよいか確認したがユーザーは拒否。生成済みコマンドによる直接ビルドもユーザーが拒否したため、両方とも未実施。
- **次に確認すべきこと:** CMake再生成後、`ctest -R ArtifactTextLayerAnimatorIntegrationTest` で専用経路を確認し、Artifact本体を依存に含める統合コストが許容されるか判断する。現状では既存実行ファイルはソースと一致しないため代用実行しない。



## 2026-10-08 — 外部描画委譲と PSO キャッシュの整合

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm` の `submitParticles` / `submitBillboard` / `submitBillboardImage`。
- **確認できた事実:** 粒子の `prepare()` は submitter 外で PSO を変更する。今回、呼び出し前に `m_currentPSO_` を無効化し、成功・失敗どちらでも後続 packet が PSO を再設定できるよう修正した。Billboard の2経路も外部 renderer に描画を委譲するが、submitter の PSO キャッシュ無効化を行っていない。
- **2026-10-08 追加対応:** 粒子画面で巨大なシアン円と青い楕円が交互に出る報告を受け、Billboard の2経路にも外部renderer呼出前のPSOキャッシュ無効化を追加した。委譲先の `PrimitiveRenderer3D` が `SetPipelineState` を呼ぶことを確認済み。症状との因果・修正効果は未検証。
- **価値／次に確認すべきこと:** Billboard直後に同じ2D PSOを使う packet を置き、委譲先の状態変更と画面を確認する。粒子の全面シアン症状との因果および修正効果は実機未確認。



## 2026-10-08 — ビューポート補助線の座標契約

- **関連:** `Artifact/src/Widgets/Render/ViewportOverlay.cppm` の `drawSafeAreaAndOrigin` と `Artifact/src/Render/PrimitiveRenderer2D.cppm` の `drawThickLineLocal` / `drawQuadLocal`。
- **確認できた事実:** ローカル線・polylineはpacketにpan/zoomを保持してキャンバス座標を描く。一方、`drawSafeAreaAndOrigin` は `canvasToViewport` で変換済みの点を同じローカル線・polylineへ渡している。今回の赤いグリッド原点軸には同じ二重変換があり、原点軸だけを修正した。
- **懸念（未検証）:** セーフエリア矩形と原点マークにもFit／pan時の二重変換がある可能性が高い。添付画像の赤い軸とは別の表示項目なので、今回はこちらを未変更。
- **価値／次に確認すべきこと:** Safe Area / Originを個別に有効化し、100%・Fit・pan後の矩形端点と原点をコンポジション座標と比較する。画面座標でsnapした点を戻して描くか、既存の画面座標描画経路へ統一する。



## 2026-10-08 — 粒子シミュレーション再利用と外部編集境界

- **関連:** `ArtifactParticleLayer::draw` / `goToFrame` / `clearFrameCache`、`ParticleSystem::goToFrame`。
- **確認できた事実:** GPU描画ごとに先頭から再シミュレーションし、レイヤーのフレーム同期も別にupdateしていた。今回、フレーム同期の重複updateを撤去し、描画で同一フレームを再利用、120Hz境界からの前進を継続する経路を追加した。既存の2引数呼出は従来の再計算を維持する。
- **懸念（未検証）:** `particleSystem()` / `emitter->params()` は外部に可変状態を公開している。レイヤーの `clearFrameCache()` を通さない同一フレームの外部編集では再利用の無効化が不足する可能性がある。
- **価値／次に確認すべきこと:** 外部編集callerの無効化契約を確認する。実機で新規2D粒子、同一フレーム再描画、30fps前進、逆スクラブ、非120Hz整合fps、プリセット変更を確認し、GPU非表示原因とCPU時間を測定する。現時点ではビルド・実機未確認。



## 2026-10-08 — 粒子のCPU送信診断とGPU実行証拠の境界

- **関連:** `Artifact/src/Render/DiligentDeviceManager.cppm`、`ArtifactIRenderer.cppm`、`ArtifactCompositionRenderController.cppm`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`。
- **確認できた事実:** 保存例の粒子数44／54ではCoreの64個以上というcompute culling条件を満たさず、直接描画を使う。CPU送信サンプルは橙、HLSLのPSは入力RGBをそのまま返す。`drawn` は描画命令を発行した状態を表し、実GPUの読み取り内容を検証していない。
- **懸念（未検証）:** 正しいCPU送信値と異なる色・形状が出る場合は、GPU側のSRV内容、実際に結合したPSO／定数、描画先、および後続描画のどこで差が生まれたかを区別する必要がある。ソースの一致や検証エラーがないことだけで正常とは断定できない。
- **今回の対応:** 停止後の保存診断にD3D12 InfoQueueの警告以上を最大32件追加する。デバイス所有境界内の参照だけで、GPU待機・読み戻し・キュー消去は行わない。共有デバイス全体の履歴であり、粒子への帰属は未検証。ビルド・実機未確認。
- **価値／次に確認すべきこと:** 保存した検証メッセージと描画先フォーマットを照合する。どちらでも判別できなければ、実描画のGPUキャプチャで粒子SRV、VS出力、PS出力、後続drawを比較する。


## 2026-10-07 — ArtifactTextLayer の独立した統合テスト境界

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm`、`Artifact/CMakeLists.txt`、`tests/Artifact/`。
- **確認できた事実:** アニメーター評価エンジンは `ArtifactCore` の単体テストターゲットから利用でき、Core側には `TextAnimatorContractTest` がある。`tests/Artifact/` には `TextGlyphRenderContractTest` もあるが、これは `TextAnimatorEngine` の結果をGlyph Submitterへ渡して描画する経路で、`ArtifactTextLayer.cppm` 自体は利用していない。レイヤー実装は `Artifact/cmake/ArtifactSources.cmake` の `ARTIFACT_APP_IMPL_SOURCES` に登録され、アプリ本体の実行ターゲット `Artifact` に属する。
- **追加確認:** root管理のGPU glyph testの `Text.Animator` import修正後、Artifact submoduleの `ArtifactTextGlyphSubmitter.cppm` にprimary interfaceがないことを確認した。Artifact submoduleは変更せず、root管理のテスト用module shimと `ArtifactGpuFoundation` providerを追加してsubmitter runtimeをbuildできるようにした。当初のD3D12専用test deviceはVulkan-only構成でlinkできなかったため、test内にDiligent Vulkan headless deviceを用意し、GPU画像testを `VULKAN_SUPPORTED` 構成で登録するように変更した。
- **画像テスト確認:** オフスクリーンGPU readback画像で要求色・決定性・zero opacity・非有限transform拒否・アニメーション変換・shaped `AB` にAnimatorを適用した結果の画像差を検査する。最後のケースは先頭glyphだけを右へ18 pxずらし、他glyphのtransformが中立であることと画像のalpha重心が右へ動くことも確認する。RTX 4070 TiのVulkan構成で6 cases全てが実行され、成功した。CPU側には実シェーピング後の折り返し行へLine selectorを適用するケースと、production `GlyphAtlas` のcoverage pixelsを確認するケースがある。`-L animation --repeat until-fail:3` の6 suitesは全て3回成功した。
- **CPUラスターテスト:** GPU実行できない構成でもproduction `GlyphAtlas` の実ラスタライズ画像を検証できるよう、Atlas alpha coverageと同一グリフ再取得時のcache/dirty契約を確認するCPU testを追加した。さらに `QtShapingBackend` の2 glyphへ `TextAnimatorEngine` を適用し、各glyphがAtlas coverage pixelへ到達するケースを加えた。`ArtifactCoreTextGlyphRasterTest` はbuild／実行に成功し、最新のtext3 suitesは `-L text -LE gpu --repeat until-fail:3` で3回すべて成功した。ビルド時のNinja assertionは、再構成で更新されたdyndep入力に対し563件の生成済みmodule map等のmtimeが古い状態だったために発生した。最新スキャン済み内容を保持したまま対象生成出力のmtimeだけを揃えると、リビルドとテストは通った。
- **価値または懸念（未検証）:** Core評価・シェーピング・GlyphAtlas・Glyph Submitter GPU画像にはカバレッジがある一方、レイヤー本体の `text.animators` 保存復元、プロパティパス更新、Animator stack snapshot の統合契約は直接検証できない。テスト専用primary module shimはroot管理runtime targetだけの補助であり、Artifact production targetのmodule boundaryを直すものではない。
- **追加のテスト境界:** `ArtifactTextRenderTargetContractTest` もroot管理のVulkan test-deviceへ切り替えた。共通device helperを `tests/Artifact/DiligentVulkanTestDevice.hpp` に置き、Vulkan構成ではglyph submitter runtimeへの不要なlinkを外した。RenderTargetとGlyphRenderのGPU contract両方がVulkan-only構成で成功する。
- **追加確認（2026-10-08）:** レイヤーテスト用の実行入口を種類ごとの CTest 項目へ分けられる一方、`ArtifactGroupLayer` は Composition / Renderer / Texture / Property、`ArtifactShapeLayer` は Shape / Physics / Renderer / Composition に依存し、いずれも `Artifact/cmake/ArtifactSources.cmake` のアプリソースとして所有されている。少数のレイヤーだけを簡単に別ライブラリ化する境界は現状見つからず、独立プロセスの統合テストを維持している。
- **価値または懸念（未検証）:** Artifact 実行体を起動せず本物のレイヤークラスを単体テストするには、単一クラスの試験用shimより、production が使うレイヤー runtime dependency slice を正式に分割する方が保守可能と考えられる。分割対象と ABI / module ownership の影響範囲は未調査。
- **次に確認すべきこと:** `ArtifactTextLayer` の直接依存を調査し、レイヤー統合テストが必要なら最小のruntime境界を設計する。依存グラフが大きい場合は、クラス実装を動かさずに保存／復元・property routing helperを抽出できるか検討する。レイヤーテストを独立バイナリ化する判断時は、Group / Shape / Solid の共有依存を対象にruntime sliceの実際の依存閉包を測る。



## 2026-10-07 — ArtifactScriptの局所名ハッシュ事前計算は未採用

- **関連:** `ArtifactCore::ArtifactScriptLocals::find()`、`ArtifactScriptExpr::variableName`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の `HookExecutionMicrobenchmark`。
- **確認できた事実:** ローカル検索は呼び出しごとに名前のFNV hashを計算する。AST名ハッシュをparse時に保持する候補をDebugで試作し、可変ASTアクセス時は再計算へfallbackする経路とテストも追加した。
- **計測:** 同一MSVC Debug microbenchmarkの1回ずつの結果で、OnUpdateは1.73→1.76 µs/hook、method/localは6.63→6.81、12 localsは13.14→13.03、20 localsは24.20→23.01。ほかのケースも上下し、結果は一貫しなかった。
- **判断:** 可変AST名の回帰テストは2/2成功したが、コストモデル上の有望さに対し単回Debug比較では改善を示せず、分岐・ASTサイズの増加を正当化できないため実装を戻した。Release性能や統計的な差は未確認。
- **次に確認すべきこと:** Release構成でローカル数別の専用microbenchmarkを反復し、中央値とばらつきを取る。再検討時は小数localsの基準ケースも含める。



## 2026-10-07 — ArtifactScriptのnull合体はfallbackを遅延評価する

- **関連:** `ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx` のbinary operator、`ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のparser/evaluator、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** evaluatorは `&&` / `||` の右辺を短絡評価するが、null fallbackを書く演算子はなかった。
- **対応:** 右結合の `??` を追加し、左辺がnullのときだけfallback式を実行する。`false`・0・空文字はnullとして扱わず左辺を返す。
- **確認結果:** fallbackの実行回数、右結合AST、三項演算子との優先順位、false・0・空文字の保持を実行テスト化。ArtifactScript関連5 CTest suitesは5/5成功。
- **価値または懸念:** fallbackが不要なケースで式や呼び出しを丸ごと省略できる。実ワークロードでの速度向上量は未計測であり、速さを数値では主張しない。
- **次に確認すべきこと:** Releaseビルドの代表的なfallback workloadを測り、実行系の次段階（再利用可能な中間表現やtiered execution）を決める。



## 2026-10-07 — ArtifactScriptの hot reload は SerializeField も移行する

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptHotReload::reload()` / `reloadWithSaved()` / `addFile()`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `migrateScriptFields()`。
- **確認できた事実:** Core hot reloadとArtifact layer独自のreload helperが、fieldの永続化フラグ `serialized` ではなく `isPublic` を条件にしていた。`[SerializeField] private` は `ArtifactScriptComponent::serializedFields()` の保存対象なのに、reload時は移行されず値を失う。また、新しく追加されたprivate serialized fieldは、public-only defaults適用経路ではmigration mapに含まれない。Core layer runtimeの `bind()` もpublic-only defaultsだけをinstance fieldsへコピーし、private fieldの宣言初期値を実行時に作っていなかった。
- **対応:** old/new両definitionの `serialized` と型一致をmigration条件にし、migration/default mapへ全serialized fieldを反映する。初期file registrationもserialized private overrideを受け付ける。runtime bind / definition replacementは全fieldをdefault初期化した後に移行値を上書きし、unserialized private fieldはreloadごとに初期値へ戻す。
- **確認結果:** Core testでpublic値とprivate SerializeField値の保持、新規private serialized fieldのdefault、runtime-only fieldの除外、private field値をhookから読む動作を検証した。ArtifactScript関連5 CTest suitesは5/5成功。Artifact側の重複helperは同じ条件へ修正したが、Artifact app moduleのcompile/runtimeは既存ImageAnalyzer依存エラーで未確認。
- **価値または懸念:** 保存APIが示すserialized契約とreload経路の判定が一致した。アプリ層の実レイヤー reloadでprivate fieldが維持されることは、Artifact側module/runtime確認後に再確認する。
- **次に確認すべきこと:** Artifact layerのproject save/restoreからlive reloadまで、public・SerializeField private・unserialized privateを跨いだ実データを統合テストする。



## 2026-10-07 — ArtifactScript field attributes の括弧と値を厳密に解析する

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のfield attribute classification/parser、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** 属性行の分類は行内に `(` と `)` があるかを先に見ていたため、`Range(...)` / `Header(...)` / `Tooltip(...)` を含む行をmethod宣言扱いにしていた。`Range` は `std::stod` の部分parseを使うため末尾の不正文字を受け入れ得た。quoted metadata内の `]` は属性終端と区別されず、public Array defaultは同じfield parseで2度生成されていた。
- **対応:** 属性行を括弧形状に依存せず分類し、quote/escapeを考慮して閉じ括弧を探す。Header/TooltipのtextはArtifactScriptのstring escape decoderを通し、Rangeは `from_chars` で値全体を消費する有限の数値かつ `min <= max` を検証する。未閉じ・不正値は属性行の位置でdiagnostic化。Array default parseも1回にした。属性行の誤分類をテスト追加時に再現し、修正後ArtifactScript関連5 suitesが通過した。
- **価値または懸念:** serialized field metadataが値と範囲を正しく保持できるparser契約になり、public Array fieldのcold parseで不要な一時Array生成を除いた。未知attributeのスキーマ検査や属性のInspector表示・保存との統合は未着手。
- **次に確認すべきこと:** Range/Header/Tooltipを受け取るInspector側の利用箇所を調べ、範囲制約やセクション表示がmetadataから実際の編集UIへ接続しているかを別途統合確認する。



## 2026-10-07 — ArtifactScriptの定義差し替えで古い実行エラーを消す

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptLayerRuntime::replaceDefinition()`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm` のreload成功経路、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** レイヤーreloadのArtifact側callerは差し替え直後に `setLastError({})` を呼んでいたが、共有Core runtimeの `replaceDefinition()` 自身は直前のhook失敗を示す `lastError_` を保持していた。runtimeを直接利用するcallerでは修正版へreloadした後にも古い診断が見える契約不整合だった。
- **対応:** Coreの `replaceDefinition()` で成功した定義置換時にerrorをclearし、失敗hook→定義置換→同じframeの抑止→次frame成功の契約テストを追加した。Lifecycle状態とframe guard、移行済みfield値を同時にassertし、対象CTestは通過。
- **価値または懸念:** Artifact側にしかなかったerror reset責務を、テスト可能な共有runtimeへ揃えた。差し替え後に保持する既存Lifecycle/frame状態は変更していない。Artifactアプリ全体の再ビルド・実機reloadは未確認。
- **次に確認すべきこと:** Artifact側ビルドが可能になったら、実Layerのreload成功時に同じerror clear契約が表示・診断経路でも成立することを確認する。



## 2026-10-07 — ArtifactScript foreachのmutation解析結果をhook間で再利用

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`statementMayMutateArray()`とforeach execution、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** foreach本体が配列を変更する可能性があるかを、hook実行ごとにAST全体へ再帰走査していた。空配列でも走査するため、loop bodyが256個のliteral declarationだけのfixtureではこの解析が毎hook発生していた。
- **対応:** 64-entry固定direct-mapのstatement-pointer cacheを追加。通常の`ArtifactScriptInstance`が同じ不変definitionをhook間で使う場合だけ結果を再利用し、直接Evaluator実行・mutable definition経路は従来の毎回scanへfallbackする。cache collisionも正しさを保って再scanする。固定領域の追加はx64で概ね1 KiB/evaluator、heap allocationなし。
- **性能確認:** MSVC Debug、256文の空foreachを10,000 hook×3回。変更前2.52 / 2.49 / 2.81 µs/hook、変更後1.75 / 1.73 / 1.73。平均で約33%短縮した targeted fixtureの結果であり、Releaseや実script一般への短縮率とは扱わない。
- **正しさ確認:** 初回read-only hook後に`ArtifactScriptInstance::definition()`経由で本体を配列書き込みへ差し替えるfixtureを追加。mutable accessでcacheを無効化し、snapshotを再解析するため結果は3（stale read-only判定なら10）となることを確認。ArtifactScript関連5 CTest suitesは5/5 passed。
- **次に確認すべきこと:** 1, 2, 8, 64以上のforeach statementを持つmethodでcache collision時の再scan頻度と損益を測り、Release/optimized profileで固定1 KiBのtradeoffを検証する。



## 2026-10-07 — ArtifactScriptの呼び出し引数・index構文を位置診断

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のprimary expression parser、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** function/object-method/new/vector/arrayのdelimited expression listは、要素parseが失敗してもnullを捨てて続行し、`f(,x)`や`array[]`などを不完全なASTとして受理する可能性があった。closing delimiterの欠落も一部silentだった。
- **対応:** 一つのboundedなdelimited-expression parserを共用し、空リストと要素列、カンマ、閉じ括弧の順序を検査する。required index expression、dot後のmember名、unary operandもparse errorとしてoffsetを記録する。既存`parseRequiredExpr`の位置付き診断へ統合。
- **互換性確認:** 0引数function / constructor、空array、vector constructorの既存挙動を維持する。末尾カンマや引数抜けは不正扱いにする。
- **確認結果:** function引数、constructor引数、indexの欠落位置をassertし、valid empty-argument、array literal、builtin callを通過。ArtifactScript関連5 CTest suitesは5/5 passed。
- **価値または懸念:** 不正な引数を無言で除去して呼び出し引数列をずらす誤動作を、実行前に局所診断できる。method call receiver pathや未閉じdelimiter全種の網羅は未完了。
- **次に確認すべきこと:** call/method-call/new/vectorごとに引数separatorとclosing delimiterの欠落を増やし、call-site/constructor evaluatorへ不完全ASTが到達しないことを確認する。



## 2026-10-07 — ArtifactScriptの必須式欠落を位置付き構文エラーにする

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の式parser / method body diagnostics、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** binary operatorの右辺や代入右辺が欠けた式（例 `value = 1.0 + ;`）はnull operandの部分ASTを作り、従来はparse errorとして必ずしも拒否されなかった。
- **対応:** 必須の二項演算子右辺、`if` / `while`条件、declaration / assignment右辺をrequired-expressionとして検証し、失敗source offsetを既存line/column診断へ渡す。省略可能な`return;`や`for`条件はrequired扱いしない。
- **確認結果:** `value = 1.0 + ;` に対して正確な6行23列の診断をassertし、既存のif/else/for・文字列式・parser stallテストとArtifactScript関連5 suitesが通過。
- **価値または懸念:** 実行時にnull値へすり替わる構文ミスをparse時に検出できる。関数引数、index、ternary、未閉じ括弧など他の必須構文位置への適用範囲は未完了。
- **次に確認すべきこと:** call/new/vector argumentsとindex欠落は後続の同日entryで対応済み。ternary colon、loop/function delimitersなど残る必須syntax positionsをfixture化し、正当な省略構文を維持する。



## 2026-10-07 — ArtifactScript method bodyをsource viewで解析し位置診断を正確化

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の method body scanner / `parseMethodBody()`、`tests/ArtifactCore/ArtifactScriptTest.cpp`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** parserは各method bodyをsourceから`std::string`へ複製してから構文木を作っていた。MSVC Debug CRTで、512文字literalを含む本文を100回parseするfixtureは87 allocations / 5,246 bytes per parseだった。
- **対応:** parse元`ArtifactScriptDefinition::source`が生存している間、method bodyを`std::string_view`として直接解析する。script本体のbrace scannerも文字列・line comment・block commentを認識する。unsupported token / parser stallの位置をsource offsetからline/columnに変換する。
- **確認結果:** 同じ100-parse fixtureは84 allocations / 4,686 bytes per parse（3 allocations / 560 bytes削減）。unsupported tokenの正確な行・列、string/comment内の波括弧を含むmethodの実行をテストした。
- **価値または懸念:** script parse / hot reload時のbody source copy allocationを除き、本文中の波括弧に対する誤ったmethod終端判定も避ける。CPU parse時間やRelease profileの差は未計測。
- **次に確認すべきこと:** 典型的な複数method scriptでRelease parse/hot-reload時間を計測し、parser diagnosticsが他のmalformed expressionも適切な位置で報告する範囲を広げる。



## 2026-10-07 — ArtifactScriptのscript object field比較でstring copyを避ける

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のBinary operand reference resolution、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** `target.value == expected` のようなpureなscript object field比較は、通常評価で両方のlong stringを値copyしていた。MSVC Debug CRT allocation hookの1000-hook fixtureでは4,000 allocations / 160,000 bytes（4 / 160 per hook）だった。
- **対応:** Binary expressionがread-only reference expressionとして扱える場合、通常のvariable/indexに加えて、script object field mapを再帰的にconst参照で辿る。`this`、top-level host property、call、non-object、missing field、invalid indexは直接参照できないため従来評価へfallbackする。script objectの長いfield stringと一致数を確認するallocation regression testを追加した。
- **確認結果:** 対象fixtureは直接参照後0 allocations / 0 bytes per hook。ArtifactScript関連5 CTest suitesは5/5 passed。
- **価値または懸念:** object field readのcopyを除き、式やobjectの意味は変更しない。CPU wall time、Release build、nested object chainとmissing/null field fallbackの追加確認は未実施。
- **次に確認すべきこと:** nested field chainとnull/missing fallbackの正しさをfixture化し、CPU負荷が安定した環境でallocation差とCPU時間を別々に計測する。



## 2026-10-07 — ArtifactScriptの配列index値を比較式へ直接渡す

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の Binary evaluation / pure operand reference resolution、`tests/ArtifactCore/ArtifactScriptTest.cpp`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** `values[0] == target` は通常、array handleとindexを評価してから要素のlong stringをcopyし、さらにfield stringもcopyして比較していた。MSVC Debug CRT allocation hookの1000-hook対照fixtureではindex参照経路なしが4000 allocations / 160000 bytes（4 / 160 per hook）だった。
- **対応:** literal/variable/indexだけで構成されるread-only reference expressionを両operandが満たす場合、配列elementと他operandをconst referenceとして`evalBinary()`へ渡す。callを含むindex expression・field access・`this`・無効なindexは従来評価へfallbackする。配列へのfield/index read、mutable AST name、short-circuit、long-string comparison allocationsをテストした。
- **確認結果:** 同fixtureは直接参照経路でsteady-state 0 allocations / 0 bytes per hook。ArtifactScript関連5 CTest suitesは5/5 passed。
- **性能上の注意:** このターンのCPU microbenchmarkは他プロセスの高いCPU使用で値が大きく揺れたため、time improvementとは主張しない。広いFieldAccess no-copy案も試験したが、独立した速度効果を確認できず撤回し、今回の変更はindex copy削減に限定した。
- **次に確認すべきこと:** CPU負荷が落ち着いたときに、同じlong-string index fixtureのbaseline/candidate wall timeとshort numeric array indexを測り、Release/optimized profileでもallocation以外の損益を確認する。



## 2026-10-07 — ArtifactScriptの単純な二項式で値を再検索・copyしない

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptEvaluator::Impl::evalExpr()` Binary path、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** `+` と比較のstring専用fast pathは、variable/literalが数値だった場合もstringとして直接読めるかを調べた後、通常の`evalExpr()`へ戻り、同じvariableをもう一度検索して値copyを作っていた。構文がliteral/variableだけのbinary nodeなら両方とも副作用なく参照解決できる。
- **対応:** simple variable/literal pairは参照を一度ずつ解決し、string/string additionだけは既存のreserve済み経路、それ以外は`evalBinary()`へ直接渡す。`&&` / `||` は短絡評価と既存の戻り値挙動を維持するため除外。AST variable nameをparse後に変更する回帰テストを追加し、キャッシュを持たず毎回現在名を解決することも確認した。
- **Debug性能計測:** MSVC Debug `HookExecutionMicrobenchmark`を変更前後各2回実行。32-method lookup（16 calls/hook）は平均60.30→52.40 µs/hook（約13%短縮）、object method lookupは75.80→68.59（約9.5%短縮）、3-class polymorphic callは106.31→96.97（約8.8%短縮）、4-class caseは111.79→103.61（約7.3%短縮）。run間の揺れがあるため目安として扱い、Release性能は未検証。
- **確認結果:** ArtifactScript関連5 CTest suitesは5/5 passed。literal/variable pair以外と未定義名は従来の評価経路を維持する。
- **次に確認すべきこと:** variable/literal以外（index / field access）のbinary評価は副作用・診断・transaction semanticsを確認してから別fixtureで検討する。現状のDebug値だけでJIT要否を判断しない。



## 2026-10-07 — ArtifactScriptのdouble複合代入をin-place化

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptEvaluator::Impl::execStmt()`、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** `double` targetへの `+= -= *= /= %=` は右辺評価後、`evalBinary()`で計算結果variantを一度作り、targetへ代入していた。foreach fixtureの内側も `total += item` を反復する。
- **対応:** targetがdoubleかつ右辺がdouble/int64の場合に限り、targetを直接更新する。ゼロ除算は同じ`div0`診断を返し、その他の型・演算は既存経路を維持する。field、local、array item、int RHSと各算術演算をテストし、division-by-zeroも確認した。
- **Debug性能計測:** 既存hook benchmarkを変更前後各2回実行。8要素foreachは平均約6.05から5.18 µs/hook（約14%短縮）、257要素numeric foreachは104.12から70.48 / 83.35（変更後平均76.92、約26%短縮）。257個の128文字string比較fixtureは252.33から219.28 / 242.13（変更後平均230.71、約9%短縮）。fixture間に測定揺らぎがあり、Release性能や汎用的な短縮率は未確認。
- **確認結果:** MSVC DebugでArtifactScript関連5 CTest suitesが5/5 passed。計測値は実装の速度差を示すが、allocation数の計測は行っていない。
- **次に確認すべきこと:** Release buildが既に利用可能になった時点で同じfixtureを再測定し、double compound assignmentが支配的な実script workloadでも効果を確認する。JIT判断にはこのDebug計測だけを使わない。



## 2026-10-07 — ArtifactScript foreach のloop binding探索を一度にする

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptFields::prepareLoopBinding()` / `bindLoopValue()` と foreach実行。
- **確認できた事実:** 旧ループは要素ごとにloop item名をoverlayから再検索していた。foreach scopeはbody実行前に新規作成され、item bindingが最初に追加されるため、そのbinding entryをループ期間中は固定参照できる。
- **対応:** loop scope作成後にbinding entryを一度だけ準備し、各要素ではそのentryのread-only aliasだけを更新する。既存のループitem書き込みは `findForWrite()` を通じてcopy-on-writeのまま。
- **暫定計測:** Debugの257 long-string comparison fixtureで、固定binding版214.97 µs/hook、binding名を各要素で引き直す対照版260.83 µs/hook。別の固定binding runは248.63 µs/hookだった。測定のrun間分散があるため効果量は確定しない。
- **次に確認すべきこと:** Release/最適化buildと複数runで短い・長い配列の損益を確認する。scope生成順やoverlay容量を変える場合は、binding entryへのポインタが固定領域内に残る契約を再検証する。



## 2026-10-07 — ArtifactScript の文字列比較で一時値コピーを省く

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptFields::findWithoutCaching()` と binary expression evaluation、ArtifactScript の foreach loop item alias。
- **確認できた事実:** Variable / literal の string比較は両側を `evalExpr()` で `ArtifactScriptValue` 化してから `evalBinary()` に渡すため、比較だけでも文字列値を複製していた。単純な変数・リテラルを直接参照する経路なら、必要な比較結果だけを返して一時stringを作らずに済む。
- **対応:** `== != < > <= >=` で単純なstring Variable / Literal同士の場合、const・非materializing lookupで元値を参照して直接比較する。他の式、非string、未定義変数は従来の評価経路へ戻し、式の副作用やエラー処理を維持する。string比較契約テストで6演算子を確認し、foreach内の長い文字列比較fixtureで反復結果も検証する。
- **性能計測:** Debugの257要素×128文字、string field比較＋scalar集計fixture（各測定20,000 hook×3回）では、値copy版576.86 µs/hook、aliasのみ556.46、alias＋直接比較248.63。個々は別runでノイズを含むが、直接比較を加えたfixtureではcopy版に対して観測上約57%短い。Release測定と実利用scriptの分布は未確認。
- **次に確認すべきこと:** Releaseまたは最適化buildで複数回測定し、単純比較以外のloop・string concatenation・property accessのprofilingと合わせ、実script上の効果と互換性を確認する。



## 2026-10-07 — ArtifactScript foreach のloop itemをcopy-on-write aliasにする

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptFields::bindLoopValue()` と `ForeachSnapshotScope`、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** snapshotを省略できるread-only foreachでも、各要素をloop item overlayへ値コピーしていた。loop itemの値を書き換える必要がある場合は、field overlayの既存 `inheritedValue` を使えば書き込み時だけmaterializeできる。
- **対応:** loop item bindingは要素への一時aliasとし、書き込み時は既存の `findForWrite()` / `operator[]` がprivate copyを作る。foreach workspace再利用時にalias pointerをclearし、完了後に配列やsnapshotへぶら下がったまま残さない。追加テストでloop itemへの複合代入が元配列や同名fieldへ漏れないことを確認する。
- **性能計測:** 257個の128文字stringを回すDebug fixture（各測定は20,000 hook×3回）で、copy版は26.44 µs/hook、alias版は別runで23.62および14.36 µs/hook。run間の差が大きいため時間短縮率は確定値として扱わない。大配列のread-only経路がalias利用であることと、この測定fixtureのcaseを追加した。
- **制約・次に確認すべきこと:** 値型・短いloopでの時間差はノイズと区別できていない。Release測定と複数runでの再確認が必要。aliasは現在の同期的なforeach statementの実行期間だけ有効とし、workspaceを再利用する前に必ず破棄する。



## 2026-10-07 — 読み取り専用 ArtifactScript foreach の配列snapshotを省略

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の foreach 実行、`tests/ArtifactCore/ArtifactScriptTest.cpp` と `tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** 読み取り専用ループでも、各 hook で配列の全要素を evaluator workspace にコピーしていた。Debug の257要素・合計60,000 hook計測は変更前115.78 µs/hookだった。
- **対応・計測:** ループ本体ASTを保守的に調べ、call/newまたは添字書込みなど配列変更の可能性がある場合はsnapshot経路を維持し、それ以外は元配列を共有参照して走査する。読み取り専用fixtureは変更後95.71 µs/hook（同じDebug fixture、約17%短縮）。後続runは97.25 µs/hookで、測定揺れを含む。配列source field自体の差し替え後も元配列の全要素を走査する回帰テストを追加。既存のpushを含むforeach snapshotテストも ArtifactScript関連5 suites で通過した。
- **価値・制約:** 257要素ループのDebug fixtureで配列値コピーを避け、時間が減った。ASTの保守的な検査はループ呼び出しごとに行うため、短いループでの利点は未計測。Release測定と実script workloadの効果も未確認。mutation可能性を検出できない新しい副作用構文を追加する場合は、判定関数も更新する必要がある。
- **次に確認すべきこと:** より長い実用scriptでread-only foreachの配列サイズ別ベンチマークを複数runし、loop本体解析コストとコピー回避の損益分岐を確認する。副作用を持つ式・statementを追加したらsnapshot維持条件を更新する。



## 2026-10-07 — ArtifactScript の読み取りoverlayは親値を参照する

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptFields`、nested object method dispatch。
- **確認できた事実:** nested method の外側が `this.value` を読んだ後、同一objectの内側methodがそのfieldを書き換えると、従来実装は回帰テストでアクセス違反を起こした。読み取りoverlayを親値へのaliasとして保持し、書き込み時だけmaterializeする実装では同ケースが通り、`outer()` の戻り値3と最終field値2を確認した。従来実装に戻す対照実験では同じテストが再びアクセス違反となった。ArtifactScript関連5 CTest suiteは修正版で全件通過。
- **性能上の注意:** 128文字fieldのread-only benchmarkでは基準3.923 µs/hook、修正版4.028 µs/hook（OnUpdate基準は1.738対1.756 µs/hook）で、今回の測定では速度改善を確認できなかった。CRT計測もread-only method fixtureで両方6 allocations / 352 bytes per hookだった。したがって現段階ではこの変更を正しさの修正として扱い、実行効率向上とは主張しない。別fixtureでの差と測定揺らぎは未検証。
- **懸念・次に確認すること:** 親fieldが安定したアドレスを保つ間だけaliasが有効という前提を維持する。将来、nested object methodを含む長時間・複数fieldのfixtureで命令数やwall timeを計測し、alias lookup costとmaterializeされた文字列copy数を分離して確認する。性能改善を目的にする場合は、この実験とは別にプロファイルで支配的な経路を特定する。



## 2026-10-07 — ArtifactScript call-site cache を不変定義のhook間で再利用

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `findMethodAtCallSite()` / `findObjectMethodAtCallSite()` と `executeResolvedMethod()`。
- **仮説・変更:** `ArtifactScriptInstance` が不変の `ArtifactScriptDefinition` を使うhookでは、call-siteと解決済みmethodの対応はフレームをまたいで変わらない。definition pointerとreuse状態を保持し、`executeResolvedMethod(..., reuseDefinitionCache=true)` の同一定義ではcall-site cache generationを維持する。通常の `execute()` / `executeMethod()`、定義変更可能APIに触れたinstanceは毎回cacheを無効化する。
- **正しさの確認:** 既存のruntime class切替テストに加え、hookを一度実行した後にmutable definition経由でcallNameを変更し、次回hookで変更後methodへ切り替わるテストを追加して通過した。ArtifactScript関連5 CTest suiteを変更後に再実行する。
- **性能計測:** MSVC Debugの同一HookExecutionMicrobenchmarkで、cache無効化版はOnUpdate 1.715 µs/hook、method/local 6.129、5引数method 5.626、6引数nested 10.683、32-method lookup 52.577だった。cache保持版2 runはOnUpdate 1.730 / 1.739、method/local 5.771 / 5.844、5引数 5.328 / 5.356、6引数 10.055 / 10.126、32-method lookup 50.890 / 50.941。単純hookで正規化するとmethod/local・5引数・6引数で約6%短縮し、32-method lookupで約4〜5%短縮。通常object methodは約1〜2%、polymorphic 3/4 classは約3〜5%短縮。baselineは1 runのため小さい差は暫定値で、Release未計測。
- **懸念・次に確認すること:** 長寿命のinstanceと定義変更後のcache無効化は回帰テストで確認した。mutable definitionを取得した後のinstanceは性能より正しさを優先し、以後cache reuseを行わない。Release測定と実scriptの呼び出し分布を今後確認する。



## 2026-10-07 — ArtifactScript method 引数の長いstring copyを削減

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptCallArguments`、`callUserMethod()`、`callInstanceMethod()`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の長いstring引数allocation tests。
- **確認できた事実・変更:** evaluatorが所有する評価済みcall argumentはscript methodへ渡した後に呼び出し側で再利用されない。直接methodとobject methodのparameter slotへmoveし、host function / host methodへ渡すconst argument pathは維持した。MSVC Debug CRT allocation hookでは128文字stringの両fixtureで6から4 allocations/hookへ減少。直接method fixtureは352から192 bytes/hook。warm-up後1,000回の計測で、元source fieldの長さ128も維持された。
- **暫定時間比較:** 直接method fixtureを3×10,000 hookで測り、非move版4.906 µs/hook、move版4.442 µs/hook（約9.5%短縮）だった。ただし各variant 1 runのみで測定揺れを含むため、allocation削減ほど確かな結果ではない。
- **constructor確認:** 128文字引数を `new Sink(source)` → `OnConstruct(string input)` へ渡し、sink fieldに同じ内容が格納されることを確認した。MSVC Debug allocation hookでは非move版24 allocations / 1,448 bytesから22 / 1,288へ減少（1,000 hook）。時間は未計測。
- **複数引数確認:** 128文字stringを2個、直接methodの `consume(first, second)` に渡すfixtureで、非move版12 allocations / 704 bytesからmove版8 / 384へ減少（1,000 hook）。引数ごとにコピー由来のallocationが減り、両source fieldは長さ128のまま保持された。
- **限界・次に確認すること:** ArtifactScript関連5 CTest suiteは変更後に全件成功。より長い実用scriptと複数run、Release workloadでcopy数・実時間を分けて確認する。



## 2026-10-07 — Host dispatch の残存allocation切り分け

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptHost::callMethodView` と evaluator の Host method dispatch。
- **確認できた事実:** method registryから `className + "." + methodName` の検索用文字列をなくし、Host functionの重複lookupも1回へまとめた。5引数のwarm-up済みMSVC Debug hookはHost function 2 allocation / 32 bytesから1 / 16 bytes、Host method 6 / 112 bytesから3 / 48 bytesへ減った。評価器外の直接Host API呼び出しはそれぞれ1 / 16、2 / 32。script経由はfunctionで同数、methodで1 allocation / 16 bytes多い。Host APIの既存vector overloadとHost method contract suiteは維持されている。
- **価値または懸念（未検証）:** Host method pathに追加される1 allocation / 16 bytesと、直接API呼び出しに残る1 / 16、2 / 32 bytesのcallsiteは未特定。単一Debug fixture以外で同様の差が出るかも未検証。
- **次に確認すべきこと:** CRT allocation hookでcall stackを記録するか、評価器処理を段階的に迂回してallocation callsiteを特定する。通常のComposition API callbackや長いclass/method名でも計測する。definitionまたはHost registry更新を伴うcacheを導入する場合は、登録置換後のinvalidate契約も確認する。



## 2026-10-07 — AsyncImageWriterManager は無界 post で imgOptions を捨てるため既定にはできない

- **関連:** `ArtifactCore/src/IO/Image/AsyncImageWriterManager.cppm:176-186（無界 post）, 168（ImageExportOptions{} 固定）, 169-173（失敗は qWarning のみ）`、`Artifact/src/Render/ArtifactRenderQueueService.cppm:8028-8060（manager 経由と std::async 経由の分岐）, 8079-8134（bounded drain と最後の join で失敗伝播）`。
- **確認できた事実:** manager 経由は (1) `boost::asio::post` の無界投入で bounded がない、(2) 内部書込が既定 `ImageExportOptions{}` でジョブの imgOpts を捨てる、(3) 書込失敗が `qWarning` のみで上位に伝播しない。対照的に std::async 既定経路は bounded drain（2×maxInFlight）＋ imgOpts 完全引継ぎ ＋ join 付き失敗伝播で設計どおり正しい。
- **価値または懸念:** 連番書き出しの非同期化を既定 ON にする（第1修正パス 2026-10-05 で実施済み）にあたり、既定経路を std::async のままにする根拠。manager を既定にする未来には bounded queue、imgOpts 引通し、失敗の公開 API 化が先行条件。
- **次に確認すべきこと:** manager 実装の bounded 化の必要性の確認（実測で disk が詰まる占有率が観測された場合）。



## 2026-10-07 — ArtifactScript AST local-name bucket cache

- **関連:** `ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx` の `ArtifactScriptExpr`、`ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptLocals::find()`。
- **確認できた事実:** parserがvariable expressionを作る時に固定32 bucketの開始位置を保存し、評価器がその位置からprobeする実験を行った。対象5 suitesは通過した。Debug microbenchmarkの単発結果は12 localsで8.94 µs/hook、20 localsで15.72 µs/hook（前回記録値はそれぞれ9.13、17.46）だった。単発計測のため時間差は改善の確証ではない。
- **設計上の懸念:** `ArtifactScriptExpr::variableName` は公開・可変フィールドであり、parse後に名前が変更された場合に保存bucketが古くなる。リポジトリ内に現在のparse後変更callerは見つからなかったが、API自体は変更を防がない。
- **対応:** キャッシュ案は正しさの契約が不足しているため実装から戻した。外部callerも含めASTを実行中不変とする契約を定めるか、変更を検知できる内部表現が必要。
- **次に確認すべきこと:** ArtifactScript ASTの公開変更互換性と実行時所有権を整理し、キャッシュを安全に持たせる方法が決まってから、同じ計測を複数回・同一条件で比較する。



## 2026-10-07 — ArtifactScript object method cache のruntime class hash

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `findObjectMethodAtCallSite()`、`tests/ArtifactCore/ArtifactScriptObjectTest.cpp` の `ObjectMethodCallSiteCacheTracksRuntimeClass`。
- **確認できた事実:** cache hit前にruntime class name全体をFNV hashしてslotを計算していた。hit条件はcall-site、definition、cached target class nameの一致をすでに確認している。
- **対応:** 最初はcall-site addressの1-way slotへ変え、次の比較で2-way cacheに拡張した。runtime classの一致確認は維持し、同じcall-siteでChild/Baseを交互に呼ぶtestを16回へ増やした。
- **価値または懸念:** class hashを毎回計算せず、2つのruntime classを同時保持する。Debug A/Bでは同一poly fixtureが1-way 107.68、2-way 92.57 µs/hook（各3,000 hook）だった。単一class lookupは1-way 68.66、2-way 71.29 µs/hook、5-field object methodは120.00対119.58 µs/hookで、短縮はpoly workloadに現れた。各variant 1 runのためrelease性能は未検証。
- **次に確認すべきこと:** 3種類以上のruntime classとcall-site set衝突を計測し、必要なら固定way数または置換方針を調整する。



## 2026-10-07 — ArtifactScript modulo parsing と non-progress guard

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `parseMulDiv()` / `parseMethodBody()`、`tests/ArtifactCore/ArtifactScriptTest.cpp`、`tests/ArtifactCore/ArtifactScriptObjectTest.cpp`。
- **確認できた事実:** evaluatorには`ArtifactScriptBinaryOp::Mod`があるが、parserの乗除算層は`*`と`/`しか受け付けていなかった。未知tokenで`parseStmt()`が位置を進めない場合、`parseMethodBody()`は同じ位置を繰り返し読み、AST statement配列を増やし続ける。`%`を含む大きな実験scriptは実際に過剰なメモリを消費した。
- **対応:** `%`を既存のMod評価へ接続し、method bodyとnested block parsingにprogress guardを追加した。停滞時は部分ASTを返さずdiagnosticにする。object cacheを2-wayにし、同じcall-siteでChild/Baseを16回交互に呼ぶtestで結果24をassertする。別testで未知operatorがdiagnosticになることも確認する。
- **価値または懸念:** 有効なmodulo式が実行可能になり、未対応tokenでparserが無限にASTを増やす経路を防ぐ。Debug関連5 suitesは5/5 pass。malformed-tokenはdiagnosticで止まり、部分ASTを返さない。
- **次に確認すべきこと:** malformed inputを複数token（演算子、閉じ括弧欠落、空expression）で検証し、method body以外のlexer/parser loopにもnon-progress guardが必要か調べる。2-way cacheは3種類以上のruntime classでhit率を計測する。



## 2026-10-07 — ArtifactScript polymorphic call-site benchmark

- **関連:** `tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の `HookExecutionMicrobenchmark`、`ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の2-way object method cache。
- **確認できた事実:** `%` parser対応後、Child/Baseを1 call-siteから交互に呼ぶbenchmarkが正常終了した。3,000 hook（各16 calls）で92.57 µs/hook、同一runの32-method object lookupは71.29 µs/hook、5-field object methodは119.58 µs/hook。新case込みのbenchmark test全体は約19.1秒で完了し、CTestのArtifactScript関連suiteは5/5 passした。
- **価値または懸念:** 2-way cacheのpolymorphic workloadを継続計測できるfixtureができた。各workloadは処理内容が異なるため、数値をそのまま相対速度の証拠には使わない。
- **次に確認すべきこと:** 3種類以上のruntime class時のevictionとcache set衝突を測る。benchmarkにhit/miss countersを付けるなら、disabled時にhookのhot pathへコストを持ち込まない設計にする。



## 2026-10-07 — ArtifactScript 3-way call-site cache

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `findObjectMethodAtCallSite()`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の `HookExecutionMicrobenchmark`。
- **確認できた事実:** 2-way版は各setに2 entryを持ち、cache miss時に最初のstale entry、または常に最後のwayを置き換えていた。3-way版は3 runtime classまで同じcall-siteに保持する。Base / Child / Siblingの同一scriptを用いたMSVC Debug A/Bで、3-class caseは2-way 145.30 / 145.81、3-way 95.31 / 92.28 / 95.01 µs/hookだった。3-way版で該当suiteは通過し、ArtifactScript関連CTestは5/5 passed。
- **価値または懸念:** 観測平均は3-class workloadで約35.3%短縮した。setごとに固定cache entryを1つ（全体32 entries）増やし、実行中のheap allocationは増やさない。4-class workloadも計測したところ、3-way平均104.32、4-way平均101.08 µs/hookで4-wayは約3.1%短かった一方、3-classでは3-way平均93.31に対し4-way平均99.00 µs/hookだった。Debugの少数runではway増加の総合的な利点が確認できず、3-wayを維持する。
- **次に確認すべきこと:** Release相当の安定した計測とcall-site set collisionを調べる。4-wayは3-classやmonomorphic workloadを遅くする可能性があり、現時点では採用根拠が不足している。



## 2026-10-07 — ArtifactScript `is` inheritance check copied class names

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `isInstanceOf()`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の `ScriptIsOperatorAvoidsSteadyStateAllocations`。
- **確認できた事実:** 継承判定はinstance class nameを`std::string`にコピーし、parentへ進む時も同じstringへparent nameをコピーしていた。長い名前の派生クラスに対するMSVC Debug testで、元実装は2 allocations / 48 bytes per hook、`std::string_view`化後は0 / 0となった。結果と継承判定の両方をassertする。
- **対応:** 判定中の`current`をinstanceまたはactive definitionが所有するnameへの`std::string_view`にした。所有元は判定期間中生存し、文字列値は変更しない。
- **価値または懸念:** `is`演算子のsteady-state class-name heap allocationsを除去した。判定深度上限と継承検索順は維持する。Release計測は未実施。
- **次に確認すべきこと:** ArtifactScript関連5 suitesを再実行し、Release相当のCPU時間とallocationを計測する。



## 2026-10-07 — ArtifactScript deep calls allocated argument and local workspaces

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptCallArguments::Workspace` / `ArtifactScriptLocals::Workspace`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` のdeep-call workspace tests。
- **確認できた事実:** evaluatorの最大call depthは64だが、両overflow workspaceは8段までだった。7引数recursive fixtureは変更前2 allocations / 352 bytes per hook、変更後1,000 hookで0 / 0。13引数の9-method call chainは変更前2 / 512 per hook、10 warm-up後の測定hookで0 / 0だった。
- **対応:** 引数とlocal workspaceを最大call depthまで拡張し、argument slotは`std::vector`から既存の`ArtifactCore::Array`へ置き換えた。64個の空vectorが持ち込むMSVC Debug proxy確保は避けた。
- **価値または懸念:** 64段までのrecursive callでoverflow argument/local buffersを再利用できる。2つの固定workspaceはevaluatorあたり合計約2.7 KiB増える。fallback側の上限（32 arguments / 32 overflow locals）は維持しており、その上限を越えるcallのsteady-state allocationは未解決。
- **次に確認すべきこと:** 32 arguments / 32 overflow locals境界を測る。Release相当のCPU時間は未確認。



## 2026-10-07 — ArtifactScript host allocation probe includes temporary names

- **関連:** `tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の `HookExecutionMicrobenchmark`、`ArtifactScriptHost::callFunctionView()` / `callMethodView()`。
- **確認できた事実:** allocation probeは各反復で文字列リテラルを `const std::string&` 引数へ渡していた。名前を反復前に作った永続 `std::string` へ置き換えた試行では、直接function / method APIとdirect-bodyの計測値が0 allocation / 0 bytesになった。その後、no-op / host-function / host-method計測は通ったが、simple script計測中にSEH access violationが起き、その差分は戻した。
- **価値または懸念:** 従来のdirect-host baselineにはAPI内dispatchだけでなく、呼出側の一時文字列生成も含まれる。1 / 2 allocationsをそのままcallback registryのコストと解釈できない。
- **次に確認すべきこと:** direct-host API用のpersistent-name基準値を独立fixtureへ移し、script経由の計測も安定させる。現行の直接API値には呼出側の一時文字列生成が含まれる。



## 2026-10-07 — ArtifactScript host dispatch copied strings per hook

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のscript-object host method fallbackとhost callback error path、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` のsteady-state allocation tests。
- **確認できた事実:** host fallbackは元々 `instance->className` を毎回 `std::string` へコピーしていた。class名が長いscript objectを1,000 hook実行し、変更前は3 allocations / 64 bytes per hook、class名を参照する変更後は1 / 16だった。今回、allocation hook内でstackを採取し、残る16-byte allocationは `ArtifactScriptHost::lastError()` が空の `std::string` を値返却する経路と確認した。MSVC Debugのstring copyは空でもcontainer proxyを確保していた。
- **対応:** `lastErrorView()` を追加し、evaluatorのhost function / method成功経路で文字列コピーを避けた。1,000 hookの独立テストでhost method / host functionの双方が0 allocations / 0 bytesとなることを確認した。既存の時間ベンチマーク内で繰り返していたallocation counterはSEHの再現箇所だったため、計測を独立テストへ分離した。
- **価値または懸念:** class名コピー削減と合わせ、host method fallbackは3 allocations / 64 bytesから0 / 0へ減少。公開 `lastError()` の互換性を保ちつつ、ホットパス内ではnon-owning viewを使う。viewは次回のhost callまたは`setLastError()`までのみ有効。
- **次に確認すべきこと:** Release相当構成では未計測。Debug関連ArtifactScript 5 suitesは今回の変更後に5/5 passed。



## 2026-10-07 — ArtifactScript overflow locals used linear lookup

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptLocals::find()` / `append()`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** 12個を超えたローカルは再利用workspaceへ保持されるが、検索はoverflow全体の線形走査だった。MSVC Debug `locals(20)` microbenchmark baselineは15.53 µs/hook。固定64-slot indexを実装し、20-local caseを複数回測ると14.62、14.70、14.78 µs/hookだった。12-local caseはbaseline 8.54、変更後8.70〜8.76 µs/hookで、目立つ効果はない。変数の正しさを確認する50-local / 10-loop fixtureは通過し、32 overflow entriesを超えるtransient fallbackの末尾も照合した。
- **対応:** workspaceの最大保持数32件を固定open-addressed indexで引けるようにした。これを越えるtransient fallback分は既存の線形走査に残し、table拡張や新たなheap確保はしない。空scopeのlookupはhash計算前に返す。
- **価値または懸念:** 観測された20-local Debug hook時間はbaselineより約4.8〜5.9%短い。ローカル変数検索の意味、名前のAST参照寿命、最大深度64、allocation-free steady stateは維持。indexは生存中のlocals scopeごとに64 bytes増え、最大64 call framesで約4 KiBの追加stack使用となる。Debug測定はrun間に時間揺れがあり、Release性能は未検証。
- **次に確認すべきこと:** Release相当で再測定する。44 localsを越える大きなscopeが実用scriptにあるかを利用例で確認し、必要性が見つかる場合だけfallback側もbounded index化を検討する。



## 2026-10-07 — ArtifactScript object construction allocated a temporary inheritance chain

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptExpr::Kind::New` 評価、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp`、`tests/ArtifactCore/ArtifactScriptObjectTest.cpp`。
- **確認できた事実:** `new Class()` は初期化フィールドをbase-firstで列挙するため、毎回 `std::vector<const ArtifactScriptClass*>` を作り、通常の継承深度でもheap確保を1回行っていた。MSVC Debug allocation probeでは`new Thing()` が8 allocations / hookだった。加えて、各初期化fieldで`unordered_map::find()`後に`emplace()`を呼び、同じキーを2回検索していた。8 default field object construction benchmarkは変更前19.04 µs/hookだった。
- **対応:** 最初の32クラスを固定`std::array`へ収め、32段を超えた残りだけ既存vectorへ退避する。base-firstの適用順は維持。field登録を`try_emplace()`へ統合し、既存fieldのfirst-wins挙動を保ったまま二重lookupを除去した。probeは7 allocations / hookをassertし、一時chain確保1回の除去を検証する。33段の継承fixtureで全base fieldが初期化されることも確認する。
- **追加で確認した問題:** 不正なクラスメンバーを読むtop-level parser loopに進捗保証がなく、入力位置を進めないまま回るケースがあった。クラスメンバー行のfallback guardを追加し、diagnosticを返して次行へ進むテストを追加した。33段fixture生成で試した同一行class bodyはこのパーサーの対応形式ではなく、その入力がこの停止経路を発見した。
- **価値または懸念:** 通常の0〜32段のconstructor evaluationから一時vector確保を外した。8-default benchmarkは`try_emplace()`後17.57 µs/hook（約7.7%短縮）。固定stack領域は256 bytes / active `new` expression。33段以上では超過部分用vectorが残る。`new`自体のobject/shared ownershipやfield map allocationは残る。比較・テストはMSVC DebugのみでRelease未計測。変更後のArtifactScript関連5 suitesは5/5 passed。
- **次に確認すべきこと:** 深い継承の実用script有無とRelease性能を確認する。既存ビルドディレクトリにはNinja Debug構成のみあり、CMake再生成なしでのRelease検証はできていない。



## 2026-10-07 — ArtifactScript variable-name hash cache trial did not improve timing

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptLocals::find()` / variable-expression evaluation、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の`locals(20)` microbenchmark。
- **仮説:** 同じAST variable expressionはhookごとに同じ識別子を検索するため、固定長のevaluator-side cacheでFNV hashを再利用できる。
- **実験:** expression pointerをdirect-map keyにし、16 byteまでの名前をinline snapshotと比較してAST名変更も検知する32-entry cacheを試した。`ArtifactScriptTest.VariableHashCacheTracksMutatedAstName`でparse後にvariableNameを変更した場合の正しい再検索を確認した。
- **確認できた事実:** MSVC Debug `locals(20)`はcache前15.81 µs/hook、cache後16.44と16.42 µs/hook。対照となる複数の未変更benchmarkもcache後runでは約4〜6%遅くなった。`locals(20) / method-local` 比はbaseline約2.50、cache後約2.50で、環境揺れを補正しても効果が確認できなかった。
- **対応:** variable-name hash cacheと専用mutation testを採用せず戻した。実行後のArtifactScript関連5 suitesは前回のtry_emplace版で5/5 passしている。追加cacheはevaluatorあたり約1.25 KiBの固定状態も必要としていた。
- **次に確認すべきこと:** Release profileか、より長い名前の専用fixtureでhash計算が実測ボトルネックとなると判明した場合にだけ、別の安全な方法を再評価する。現行環境のbuild treeはDebugだけなのでRelease値はない。



## 2026-10-07 — ArtifactScript evaluator-scoped class lookup index

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptEvaluator::Impl::findClass()` と `new ClassName()` の継承field初期化、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の30-class chain benchmark、`tests/ArtifactCore/ArtifactScriptTest.cpp` の定義変更回帰テスト。
- **仮説:** クラス定義を線形検索し続けると、深い継承のobject constructionで同じ名前検索を繰り返す。評価開始時にクラス名 index を固定領域へ作れば、名前検索を平均定数時間にできる。
- **実装:** evaluatorごとに128-slot open-address tableを保持し、定義が64クラス以下の場合だけ`executeResolvedMethod()`の開始時に構築する。root classの既存優先順位と重複名のfirst-match挙動を維持する。65クラス以上は従来の線形lookupへfallback。定義ごとにgenerationを進め、同じevaluatorの実行間でASTのclass nameが変更されたケースはindexを再構築して追従する。索引は固定配列で、構築・lookupに動的確保はない。
- **確認できた事実:** MSVC Debugで30-class chain object constructionはindex前62.03 µs/hook、index後14.95 µs/hook（約75.9%短縮、各60,000 calls）。定義の同一ASTをFirstからSecondへ書き換えて再実行するテスト、およびArtifactScript関連5 suitesがすべてpassした。
- **価値または懸念:** 固定table領域は約2 KiB/evaluator。効果はクラス検索を多数行う大きなdefinitionに偏り、65クラス以上では従来性能のまま。測定はMSVC DebugのみでRelease値ではない。
- **次に確認すべきこと:** 実際の大規模scriptでclass count分布を調べ、64-class cutoffが現実的か確認する。Release profileはRelease build configurationを用意した時点で再測定する。



## 2026-10-07 — ArtifactScript instance field transaction wrapper trial

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptEvaluator::Impl::callInstanceMethod()` と`ArtifactScriptFields`、`tests/ArtifactCore/ArtifactScriptObjectTest.cpp` の`FailedMethodRollsBackInstanceFieldWrites`。
- **仮説:** instance methodごとにfield wrapperを二段構成するため、root mapとtransaction overlayを一つのwrapperで扱えばstack stateを減らせる。
- **確認できた事実:** root mapへ直接書く単純化では、メソッド内エラー時にinstance field変更をrollbackする契約を維持できず、`FailedMethodRollsBackInstanceFieldWrites` が失敗した。rollbackを維持するtransactional-root variantも試したが、MSVC Debugの全体時間はrun間で揺れ、5-field object methodはbaseline 126.23 µs/hookに対して157.56 / 165.30 µs/hookだった。同時にobject constructionや30-class chainも遅いrunとなり、全体負荷の差を切り分けられず、性能向上を立証できなかった。
- **対応:** 両variantを採用せず、既存の二段wrapperとrollback動作を維持した。rollbackを外す最適化は意味を変えるため不採用とする。
- **次に確認すべきこと:** field transactionのallocation/CPU costを分離測定できる専用benchmarkを作り、構造を変更する場合は失敗時rollbackと成功時commitを別々に測る。



## 2026-10-07 — ArtifactScript lifecycle hook lookup in deep class hierarchies

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptInstance::findLifecycleHookInDefinition()` と、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の30-class inherited-hook benchmark。
- **仮説:** evaluator内のclass indexは`new`やmethod dispatchを速くするが、各`invokeHook()`前に走るlifecycle method探索は引き続き各継承段で全class listを線形走査している。深い継承では約O(depth × class count)の探索が重複する。
- **実装:** lifecycle hook探索時、class数9〜64の範囲だけ固定128-slot indexをスタック上のoptional storageへ構築し、root優先・duplicate first-matchを保って検索する。8以下はindex構築を避けた線形lookup、65以上も無制限なtable拡張をせず従来線形lookupへfallbackする。各hook呼び出しでindexを構築するため、mutable definition APIで変更されたclass名・継承関係も次の呼び出しで反映される。
- **確認できた事実:** 30クラス鎖からbase `OnUpdate`を探すMSVC Debug benchmarkは線形時27.91、index後6.53 / 7.11 µs/hook（約75%短縮）。単純hookは1.80対1.82 µs/hookでほぼ同じ。関連5 suitesは5/5 passed。stack領域はtable約1 KiBで、9〜64 classの場合のみtableを初期化する。
- **価値または懸念:** 深い継承スクリプトの毎hook lookupが短くなった。65 class以上では引き続き線形であり、Release性能は未検証。
- **次に確認すべきこと:** 実際のscriptで継承深度分布を確認し、64-class cutoffを維持するか判断する。MSVC Release構成が利用可能になった時点で再計測する。



## 2026-10-07 — Skip lifecycle index for root-defined hooks

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptInstance::findLifecycleHookInDefinition()`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の31-class root-hook benchmark。
- **仮説:** 多くのscriptではroot class自身がlifecycle hookを定義する。毎回継承indexを作ってからrootを探すのは不要で、root優先の仕様を使えば先に即時解決できる。
- **実装:** root classのmethod listを最初に調べ、一致するhook bodyがあれば直ちに返す。その後だけ派生先のindex構築／継承検索を行う。root methodにbodyがない場合は既存どおりbase hookへ続く。
- **確認できた事実:** 31-class定義のroot hook benchmarkはMSVC Debugで2.81から1.98 µs/hook（約30%短縮）。単純root hookは1.80から1.70 µs/hook。派生hookがbase hookをoverrideする既存テストとArtifactScript関連5 suitesがすべてpassした。
- **価値または懸念:** class数の多い通常root hookではindex構築・継承走査が不要になる。差はDebug benchmarkで測定し、Release値は未確認。
- **次に確認すべきこと:** root hookを持つ大規模definitionの分布を計測し、index cutoffsと合わせて評価する。



## 2026-10-07 — ArtifactScript local inline capacity A/B

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptLocals::inlineCapacity_` と、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の`locals(12)` / `locals(20)` benchmark。
- **仮説:** 12 localsで一部がoverflowへ回る一方、毎call frameに12個のvariant slotを初期化している。8または16への変更がsteady-state時間を改善するか比較する。
- **確認できた事実:** MSVC Debugで8枠は`locals(12) / method-local`比1.556、`locals(20) / method-local`比2.590となり、12枠版baseline比で有利でなかった。16枠は2 runで比が各1.432 / 1.444および2.409 / 2.430、baseline 12枠版の1.478および2.513から約2.7% / 3.7%改善。関連5 suitesは16枠版で5/5 passed。
- **対応:** 16枠を採用し、8枠は戻した。追加stack領域は4 binding per active method frame、最大call depth 64でbounded。大きいlocals集合は既存のworkspace-backed overflow経路を維持する。
- **価値または懸念:** 12〜20 localsを使うhookのDebug評価が少し短くなった。差は数%の範囲であり、Release測定はない。
- **次に確認すべきこと:** 実scriptのlocals分布が16枠を支持するかを計測し、Release buildで再確認する。



## 2026-10-07 — ArtifactScript inline locals linear-scan trial rejected

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptLocals::find()`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の`locals(12)` / `locals(20)` benchmark。
- **仮説:** 最大16個のinline localsなら、FNV hashとopen-address indexの代わりに小さな線形走査を使うとlookup・local insertionが軽くなる可能性がある。
- **実験:** inline local index tableを一時的に削除し、mutable/const `find()`をinline entriesの線形比較、overflow entriesを既存hash table検索に分けた。関連5 suitesは変更variantで5/5 passした。
- **確認できた事実:** Debug benchmark 2 runの`locals(12) / method-local`比は1.684 / 1.657で、hash table版の復帰後baselineは1.436 / 1.428。`locals(20) / method-local`比は3.190 / 3.179で、復帰後は2.433 / 2.401。線形variantは相対で約12〜16%および約32%遅かった。
- **対応:** 線形variantを採用せず、16-capacity inline locals向けopen-address tableへ戻した。同一復帰版を2回再測定し、性能差が走査方式に由来することを確認した。
- **次に確認すべきこと:** 別の改善候補では、variable name lookupのhash計算再利用を再試行する前に、実行単位の名前／スロット解決を計測・設計する。既存AST hash cache試行は以前の実測で無効だったため、同じ方式は再利用しない。



## 2026-10-07 — Reuse ArtifactScript lifecycle lookup for immutable definitions

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptInstance::findLifecycleHookInDefinition()` / `invokeHook()` とevaluator class lookup、`ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx` の`ArtifactScriptInstance::definition()` mutable accessor。
- **仮説:** class indexを毎hookで再構築し、継承hook methodを毎フレーム再探索するのは、定義を保持する`ArtifactScriptInstance`の通常実行では不要なコールドデータ処理である。
- **実装:** evaluator内で固定128-slot class indexをdefinition単位で再利用し、lifecycle hook種別ごとの固定6-entry cacheへ解決methodを保持する。`ArtifactScriptInstance::definition()` のmutable overloadは参照を返す前に両cacheをdisable/clearする。取得したmutable referenceが呼び出し元に保持されてもstale cacheを使わないよう、そのinstanceでは以降の再利用を行わない。一般`ArtifactScriptEvaluator::executeMethod()`はcaller definitionが変更可能なため既存どおりexecutionごとにindexを再構築する。
- **確認できた事実:** MSVC Debugで30-class inherited hookはclass-index-only 6.35 / 7.11からpersistent cache 1.56 / 1.59 µs/hook（約75%短縮）。31-class root hookは1.98 / 2.09から1.55 / 1.58（約21〜25%短縮）。root hookの探索は早期終了し、通常の1-class hookには固定index構築がない。mutable accessor後に`new First()`を`new Second()`へ変更し、さらにcached hook bodyをremoveする回帰テストとArtifactScript関連5 suitesがpassした。
- **価値または懸念:** 深い継承hookのper-frame lookupとclass index再構築がなくなる。不変定義ならhook cacheは固定6 entry、indexは固定128 slot。mutable accessorを一度でも使うと安全のためcache reuseはinstance寿命中無効になり、深い継承では以前の再構築コストへ戻る。MSVC Debugのみの測定。
- **次に確認すべきこと:** 実際のscript authoring flowがmutable definition accessorをhook実行前後に利用する頻度を調べる。将来dirty tracking APIへ移行できるなら、無制限にescapeするmutable referenceより明示的なmutation boundaryを設けられるか検討する。



## 2026-10-07 — ArtifactScript no-copy field reads did not improve hook timing

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptFields::find()`とnested field scope、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の5-field object method benchmark。
- **仮説:** expression readが親fieldをtransaction overlayへコピーしているため、const readで親値を直接参照すればfield-heavy scriptのコピーとoverlay登録を避けられる。
- **実験:** variable、`this.field`、foreach collection readにreadonly parent traversalを使い、5フィールドを16回呼ぶobject method workloadへfield-readを追加してDebug時間を比較した。
- **確認できた事実:** MSVC Debugのread/write workloadはreadonly traversalなしで149.6 µs/hook、ありで163.8 µs/hookと約9.5%遅くなった。通常のmicrobenchmarkにも数%のrun間揺れがあるが、この主要workloadでは逆方向だった。readonly実装とbenchmark変更は戻した。別のfor-loop中心fixtureは既存のaccess violationを再現し、安全な測定ケースとして使えなかった。
- **価値または懸念:** overlayの小さな線形検索と親hash lookupを比べた場合、毎回親へ抜ける方が高コストになり得る。読み取りコピーを無条件に外す最適化は採用しない。
- **次に確認すべきこと:** field name解決が実測ボトルネックか、またtransaction scope単位の読み取りスロット解決をbounded・allocation-freeで再利用できるかを、意味論を変えないfixtureで調べる。



## 2026-10-07 — Reduce ArtifactScript call argument inline capacity to four

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptCallArguments`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` のmethod/local、5-argument、6-argument benchmark。
- **仮説:** 全callで5個の`ArtifactScriptValue` inline slotを構築するより4個に減らすと、4引数以下の一般的なscript callのstack初期化を減らせる。5引数以上は既存のworkspace overflowへ移る。
- **実験:** inline capacity 5と4をMSVC Debugで比較し、hook benchmark内のno-call baselineに対するmethod/local時間を複数回比較した。6引数benchmarkは外側6引数と内側6引数のnested callを追加し、workspace再利用経路も測る。
- **確認できた事実:** method/local（4 arguments）のbaseline ratioは3.539、capacity 4の4 runは3.475 / 3.486 / 3.520 / 3.490で平均約1.2%低かった。6 nested argumentsはbaseline ratio 6.309、capacity 4の2 runは6.153 / 6.174で約2.3%低かった。5 argumentsはbaseline ratio 3.213、capacity 4は3.200〜3.283で概ね同等だがrun揺れを含む。関連ArtifactScript 5 suitesはcapacity 4で5/5 passed。
- **対応:** `ArtifactScriptCallArguments` のinline capacityを4へ変更。引数が5以上の経路は既存のworkspace-backed overflowを維持し、追加heap確保や動的workspace拡張は導入しない。
- **価値または懸念:** call frameごとの初期化slotを1つ減らし、測定した4・6引数ケースは少し短くなった。5引数は同等〜少し遅い可能性があり、benchmarkはMSVC Debugのみ。実scriptでの引数個数分布は未計測。
- **次に確認すべきこと:** 実用scriptの呼び出し引数個数分布を取得し、4枠が実 workload に合うかを確認する。Release構成でも再測定する。



## 2026-10-07 — Avoid duplicate hashing when inserting ArtifactScript locals

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptLocals::operator[]` / `emplace` / `append()`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` のmethod-localとlocals(12/20) benchmark。
- **仮説:** 新しいlocalの挿入で`find()`と`append()`が同じ名前hashを二度計算するため、挿入側でhashを共有すればparameter・declarationのframe setupを短縮できる。
- **実験:** precomputed hashを`findHashed()`と`append()`へ渡す実装を試した。最初は通常read検索もhelperへ委譲したため、MSVC Debugでmethod/localとlocals(12)の正規化時間が悪化した。read検索を従来どおり関数内に戻し、挿入経路だけhelperを使うvariantも測った。
- **確認できた事実:** baselineに対し、read pathをhelper化したvariantではmethod/local ratioが約3.49から3.64〜3.70、locals(12) ratioが約5.08から5.37〜5.78へ悪化した。read pathを戻したvariantもmethod/local ratioは約3.56、locals(12)/locals(20)は約5.14 / 9.06で、baseline約3.49 / 5.08 / 8.98より少し遅かった。MSVC Debug run間の揺れはあるが、安定した改善は観測できず、試行コードは戻した。
- **価値または懸念:** 重複hashを除いても、追加helper境界とinsert検索のコストが相殺し得る。共通read pathへhelperを持ち込む案は特に遅かった。
- **次に確認すべきこと:** local-name slot化のようなlookup回数自体を減らす案を検討する場合、mutable ASTとmethod scope/shadowingの意味を先に整理し、DebugだけでなくRelease計測も用意して評価する。



## 2026-10-07 — Dirty tracking for ArtifactScript field overlays was not worthwhile

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptFields` overlay read/writeとcommit、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の128-character string field assignment workload。
- **仮説:** read-only field lookupが作るoverlay entryもscope終了時に親へcopy-backしているため、write intentをdirty bitで区別すれば不要な書き戻しを避けられる。
- **実験:** `findForWrite()`でassignmentをmarkし、read-created overlayはcommitしないvariantを試した。baselineと同じ128-character string field assignmentを測るtiming fixtureと、allocation profileも計測した。
- **確認できた事実:** baselineは3.247 µs/hook、dirty variantは3.178 µs/hookだったが、simple hookも1.776から1.739へ同程度動いたためnormalized comparisonでは明確な改善がなかった。allocation profileは両variantとも4 allocations / 320 bytes per hook。dirty variantの変更と一時allocation testは戻し、long-string timing/correctness fixtureはbenchmark suiteに残した。元の実装でArtifactScript関連5 suitesは5/5 passed。
- **価値または懸念:** 長いstringでも書き戻しコピーが割当数を増やしているわけではなく、dirty tracking用stateとcallsite変更の複雑さに見合う計測効果は出なかった。
- **次に確認すべきこと:** field valueをoverlayに複製する時点そのものを避けるなら、scope snapshot・nested mutation・共有Array/Object参照の意味を保つlazy copy-on-write方式を設計し、numeric/string両方の比較fixtureを用意する。




## 2026-10-07 — Move ArtifactScript array values in push/pop

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の組み込み `push()` / `pop()`、`tests/ArtifactCore/ArtifactScriptTest.cpp` と `tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **仮説:** 引数評価後の `push()` は値を配列へコピーし、`pop()` は末尾値を戻り値へコピーしている。所有権を移動できる箇所なので、特に長いstringの一時コピーと割当を減らせる。
- **実装:** `push()` は評価済み第2引数を引数領域から配列へmoveし、`pop()` は末尾値を戻り値へmoveする。長いstringのpop結果と、push後もsource fieldが維持されることをテストで確認する。
- **確認できた事実:** MSVC Debugの128文字stringを配列へpushするwarm hook計測で、従来copyは5 allocations / 336 bytes per hook、move後は4 allocations / 192 bytes per hookとなり、hookあたり1 allocation / 144 bytesを削減した。ArtifactScript関連5 suitesは5/5 passed。
- **価値または懸念:** 評価済み引数のコピーを避け、配列の意味・source fieldの値は維持する。allocation測定とテストはMSVC Debugのみで、Release計測はない。
- **次に確認すべきこと:** 実scriptのpush/pop頻度と格納値の型を測り、長いstring以外でも実 workload に効果があるか、Release構成が利用可能になった際に再確認する。



## 2026-10-07 — Move evaluated ArtifactScript declaration values into locals

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptStmt::Kind::Decl` と、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の長いstring local declaration allocation probe。
- **仮説:** 宣言初期値は`evalExpr()`で既に値になっているが、`locals[name] = init`がもう一度`ArtifactScriptValue`をcopyする。ローカルbindingへ所有権をmoveすれば、文字列の再割当を避けられる。
- **実装:** 初期化式または既定Arrayがある宣言は`init`をlocal bindingへmoveし、初期化なしの宣言は既定値を設定する。
- **確認できた事実:** MSVC Debugで128文字のfield値をlocalへ読み、別fieldへ書く1000-hook計測は旧実装10 allocations / 672 bytes per hook、新実装8 / 512。hookあたり2 allocations / 160 bytes減り、sourceと観測fieldの文字列一致も確認した。ArtifactScript関連5 suitesはCTest **5/5 passed**。測定はMSVC Debug。
- **価値または懸念:** string・arrayなど所有値を含む宣言で不要なvariant copyを省く。Release実行時間および他型の実workloadは未測定。
- **次に確認すべきこと:** ローカル宣言の実script分布とRelease時のCPU時間を測り、Debug allocation減少が通常workloadでも有益か確認する。



## 2026-10-07 — Move ArtifactScript method results out of return slots

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`runUserMethodBody()` / `callInstanceMethod()` と、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` のlong-string return allocation probe。
- **仮説:** メソッド戻り値を`returnValue_`から外側へ返す前にcopyし、その直後にnested call用の以前の値をrestoreしている。return slotの値は一時所有なので、restore前にmoveすれば戻り値copyを避けられる。
- **実装:** user methodとscript object methodの両経路で、`returnValue_`を結果へmoveしてから以前のnested-call stateをrestoreする。
- **確認できた事実:** MSVC Debugで128文字stringをuser methodとobject methodの両方から返す1000-hook fixtureは、旧実装20 allocations / 1344 bytes、新実装17 / 912 per hook。hookあたり3 allocations / 432 bytes減り、source・observed fieldの内容一致を確認した。ArtifactScript関連5 suitesはCTest **5/5 passed**。
- **価値または懸念:** string等所有値を返すcallのtemporary copyを減らす。測定は両method経路を使うMSVC Debug fixtureで、Release時間および片方だけの寄与は未測定。
- **次に確認すべきこと:** 実scriptの戻り値型・call頻度を調べ、Release構成が使える時にCPU時間も測る。



## 2026-10-07 — Append string compound assignments in place

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptEvaluator::Impl::execStmt()` compound assignment pathsと、`tests/ArtifactCore/ArtifactScriptTest.cpp` / `tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **仮説:** `string += string` は現在、左値・右値の文字列化コピーと結合後の一時文字列を作ってからtargetへ代入する。target bindingを直接appendすれば一時copyと再割当を避けられる。
- **実装:** 両辺がstringの`+=`だけをlocal binding、field overlay、array itemへ直接appendし、数値・型変換を含む他のcompound assignmentは既存の`evalBinary()`経路を維持する。
- **確認できた事実:** MSVC Debugの128文字sourceとsuffixを連結するfixtureで、旧処理6.60、新処理4.82 µs/hook（約27%短縮）。allocationは13 / 1104 bytesから7 / 752 per hookへ減った。local・field・array itemでの結果一致テストを追加し、ArtifactScript関連5 suitesはCTest **5/5 passed**。
- **価値または懸念:** string同士のcompound appendが既存target容量を再利用できる。数値等からのstring変換と別型のcompound演算は従来の意味・経路を保つ。測定はMSVC Debugのみ。
- **次に確認すべきこと:** Release構成が使える時に通常CPU時間を測り、短いstringと混在型の`+=` workloadでも効果を確認する。



## 2026-10-07 — Move evaluated ArtifactScript simple assignments into targets

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptEvaluator::Impl::execStmt()` `Assign` 経路と、`tests/ArtifactCore/ArtifactScriptTest.cpp` / `tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** 代入右辺は `evalExpr()` で値として評価済みだが、単純代入は `applyCompound()` がその `ArtifactScriptValue` をコピーして返し、さらに代入先へコピーしていた。既存のcompound演算とは所有権の扱いを分けられる。
- **実装・計測:** `=` / 空operatorの場合だけ評価済み値をlocal、field overlay、array itemへ直接moveし、compound代入の算術・append経路はそのまま維持した。128文字stringのfield代入MSVC Debug fixtureは4 allocations / 320 bytesから2 / 160 per hookへ減少した。
- **価値または懸念:** 文字列等の所有値の一時コピーを減らせる。測定はMSVC Debugのみで、CPU時間とRelease workloadは未測定。
- **確認結果:** localの再代入・array item・fieldで値とsource保持を検証し、ArtifactScript関連5 CTest suitesは **5/5 passed**。最適化がlocal declaration、method return、string `+=` のfixtureにも影響し、既存期待値はlocal declaration 8→6 allocations / 512→352 bytes、method returns 17→13 / 912→592、string `+=` 7→5 / 752→592 per hookへ更新した。
- **次に確認すべきこと:** Release計測を後日行い、文字列以外の大きな所有値でも同じく無駄なcopyを避けられるか確認する。



## 2026-10-07 — Build ArtifactScript string addition directly into its result

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の Binary expression path と `ArtifactScriptEvaluator::Impl::evalBinary()`、`tests/ArtifactCore/ArtifactScriptTest.cpp` / `tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** `source + suffix` はvariable評価でstring値をcopyした後、`evalBinary()` の `toString()` でも両文字列をcopyし、さらに `operator+` が結合結果を作っていた。単純なVariable / Literalのstring operandは読み取り専用lookupで直接参照でき、式副作用はない。
- **実装・計測:** simple string operandsは必要な連結長をreserveした結果stringへ直接appendし、complex/mixed operandsのfallbackも各operandを一時 `std::string` 化せず結果bufferへappendする。128+128文字MSVC Debug fixtureは11 allocations / 944 bytesから3 / 304 per hookへ削減（8 allocations / 640 bytes少ない）。integerとboolのstring conversion、および既存の連結・比較fixtureを確認した。
- **価値または懸念:** 長いstring同士の通常 `+` でoperand copyと中間連結stringを省く。allocation削減は大きいがDebug計測のみで、CPU時間・Release性能は未測定。double変換は従来と同じ `ostringstream` 表現を維持する。
- **確認結果:** string addition変更後もArtifactScript関連5 CTest suitesは **5/5 passed**。他のallocation fixture期待値への変動はなかった。Release測定と短いstring workloadは未確認。
- **次に確認すべきこと:** Release測定でCPU時間とallocation削減の関係を確認し、短いstring workloadでreserveが過剰にならないか測る。



## 2026-10-07 — Append scalar values directly for ArtifactScript string +=

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `execStmt()` 内 `appendStringCompound` と、ArtifactScriptのcompound assignment tests。
- **確認できた事実:** `string += integer/bool/double` は通常の `evalBinary(Add)` を経由し、左文字列と結合結果を作ってからtargetを置換していた。targetがstringである場合、右辺は `evalBinary` と同じ文字列表現へその場でappendできる。
- **実装・計測:** `+=` のstring targetはstring、bool、int64、doubleをtargetへ直接appendするようにした。非文字列同士や他operatorは従来pathを維持。128文字stringへint64 `7` を足すMSVC Debug fixtureは6 allocations / 352 bytesから3 / 176 per hookへ減少した。
- **価値または懸念:** 文字列とscalarのcompound appendで結合結果用stringを省く。double conversionは従来同様 `ostringstream` を使うためconversion自身の確保は残る。計測はMSVC DebugのみでCPU時間・Releaseは未測定。
- **確認結果:** field targetでstring・int・bool・doubleの組み合わせを確認し、ArtifactScript関連5 CTest suitesは **5/5 passed**。既存allocation assertionsに予期しない変化はなかった。
- **次に確認すべきこと:** Release profilingでCPU時間とallocation削減の関係を確認する。



## 2026-10-07 — Fuse pure multi-string ArtifactScript addition trees

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の Binary expression evaluator、`tests/ArtifactCore/ArtifactScriptTest.cpp` / `tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** 左結合の `first + second + third + fourth` は、既存の2 operand fast pathだけではnested resultを毎段作り直していた。literalとVariableだけのstring operandなら式評価の副作用がなく、実際のstring値を参照して一括連結できる。
- **実装・計測:** nested `+` ASTを最大深さ64まで2回走査し、全leafがliteralまたはruntime string variableの場合だけ合計長をreserveして一度の結果stringへappendする。call/non-string/undefined leafや深さ超過は一切評価せずに元の evaluator へfallbackするため、callを重複実行しない。128文字string 4個のMSVC Debug fixtureは15 allocations / 2366 bytesから3 / 560 per hookへ減少。side-effecting methodが一度だけ呼ばれる回帰テストも追加した。
- **価値または懸念:** 複数の長いstringを式内連結するときの中間stringとvariable result copiesを避ける。測定はMSVC DebugのみでCPU時間・Releaseは未測定。純粋な文字列式に限定するため、scalar conversionを含む鎖は従来pathになる。
- **確認結果:** ArtifactScript関連5 CTest suitesは **5/5 passed**。side-effecting callは1回だけ実行され、数値だけのdeep recursive / call-chain fixtureは従来どおり0 allocationを維持した。文字列treeがstring-onlyと分かる前の一時buffer作成は避け、numeric fallbackに確保を持ち込まない。
- **次に確認すべきこと:** Release profilingが可能になったら、flatten preflight overheadとallocation削減を併せて評価する。



## 2026-10-07 — JITは計測に基づく別段階として検討

- **関連:** `docs/planned/MILESTONE_ARTIFACTSCRIPT_LANGUAGE_EVOLUTION_2026-08-21.md` の対象外項目と、`ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のparser AST / `evalExpr()` tree-walk実行。
- **確認できた事実:** 現行マイルストーンはbytecode VM / JITへの置換を対象外とし、「速度不足が実測されたら別途起票」としている。現在の実行器はASTを直接評価しており、JIT用依存やcodegen pathは調査範囲で見つからなかった。
- **推論（未検証）:** JIT自体は設計上可能だが、ArtifactScriptの型変換・diagnostic・host binding・定義変更/cache invalidationを保つ専用IR、tier-up基準、fallbackとの意味一致が必要になる。日々の小さな最適化と計測は並行できるが、JITを同時に製品経路へ追加すると性能差の原因と互換性境界が混ざる。
- **次に確認すべきこと:** Releaseまたは代表的scriptでCPU profileを取り、tree-walkが支配的かを確認した後に、hotness threshold付きの限定prototypeを別作業として比較する。LLVM等の外部codegen選定はその段階で現行依存・配布条件も確認する。



## 2026-10-07 — ArtifactScriptの構文追加を遅延評価による実行短縮にも使う

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `??=` assignment evaluator と ArtifactScript language evolution milestone。
- **確認できた事実:** 新しい `??=` は対象がnullでないとき右辺を実行せず、nullのときだけfallback式を評価する。これは言語機能追加が、fallback内部の処理や副作用を省く実行経路も同時に作れる例になる。
- **確認できた事実:** 最初の統合では通常代入の右辺を空の`ArtifactScriptValue`へ後から代入し、既存のallocation contract tests 7件で各hookあたり1 allocation増加した。通常経路を条件式による直接初期化に戻すと7件すべて従来の期待値へ戻り、5つのArtifactScript関連CTest suitesが通った。
- **確認できた事実:** `??=` regression fixtureでも、既存値に対するoverflowing fallbackを省略し、side-effecting index expressionを1回だけ評価することを確認するよう追加した。
- **確認できた事実:** assignment variantでfalse・整数0・空文字もnull扱いされず保持され、fallbackを呼ばないことを専用assertで確認する。
- **推論（未検証）:** 実scriptでnullでない頻度が高く、fallbackが高コストなら実行時間短縮につながる可能性がある。追加分岐・target resolutionを含む全体差は計測していない。
- **次に確認すべきこと:** fallbackが常にnull / 非nullとなる小さな対照fixtureと実script profileでCPU時間を測り、意味上の短絡と速度上の効果を別々に確認する。



## 2026-10-07 — ArtifactScript array literalはAST要素数を使って一括確保できる

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のArrayLiteral評価と `LayerScriptComponentContractTest.ArrayLiteralBuildsTheExpectedValuesAcrossRepeatedHooks`。
- **確認できた事実:** array literal ASTは実行時の要素数が固定なのに、評価器は空vectorへ順次`push_back()`していた。4要素の数値literalをhookごとに作るMSVC Debug fixtureでは、容量予約前は6 allocations / 544 bytes/hook、予約後は3 / 256だった。50,000 hooksの各3回測定中央値も4.74から3.57 µs/hookへ変化した。
- **価値または懸念:** 成長再確保をなくし、計測fixtureでは割当回数を半分、割当bytesを約53%削減した。CPU時間は単一のDebug workloadのみで、string要素・空/大規模literal・Release構成では未確認。
- **次に確認すべきこと:** 長さ別とstring要素を含むliteralでallocation/CPUを測り、予約の影響を確認する。array objectと最終buffer自体のhook内確保はまだ残る。



## 2026-10-07 — Reserving wide ArtifactScript object field maps traded speed for bytes

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ArtifactScriptExpr::Kind::New`で行う継承field登録、24-default-field object construction fixture。
- **仮説:** field mapの複数回rehashを避けるため、継承フィールド数を先に数えて`reserve()`すればconstructorを速くできる。
- **実験:** 24 default fieldsを持つscript objectを各hookで生成し、reserveなしとfield-count reserveをDebug A/Bした。
- **確認できた事実:** MSVC Debugではreserveなし40.86 µs/hook・56 allocations / 4336 bytes、reserveあり41.90 / 41.88 µs/hook・56 / 3824 bytesだった。割当bytesは減るが割当回数は変わらず、steady-state時間は約2.5%遅いrunとなった。
- **対応:** CPU executionの改善を立証できないため、reserve変更と専用fixtureは採用せず戻した。
- **次に確認すべきこと:** object field map容量の実workload分布が重要なら、同じA/BをRelease profileでも測り直す。現行build treeにはRelease構成がない。



## 2026-10-07 — ArtifactScript integer expressions lost int64 precision

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のnumeric literal parser、`evalBinary()` / `evalUnary()`、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** `ArtifactScriptValue` と`int` field defaultsは`std::int64_t`を保持する一方、式の整数literalは常に`double`へ解析され、数値演算もdoubleへcoerceされていた。このため`9007199254740993`は正確に表せず、int同士の割り算も浮動小数点値を返していた。
- **実装:** 整数literalと負の`INT64_MIN`を`int64`として解析し、int同士の加減乗除・剰余・比較を整数のまま行う。整数overflowと`INT64_MIN / -1`は未定義動作を避けてscript errorにする。混合int/floatは従来どおりdouble経路。
- **確認結果:** `2^53+1`とその加算、負の最小値、正負の整数除算・剰余、加算・乗算・除算overflowをテスト。既存のdynamic numeric result testはint/double双方を許容するよう更新。変更後のArtifactScript関連5 CTest suitesは **5/5 passed**。
- **価値または懸念:** int64を公開している言語データモデルと式評価の精度が一致する。整数literalのvariant型がdoubleからint64へ変わるため、外部host bindingや既存scriptが値のvariant型に依存していないかは受け入れ確認が必要。整数workloadの実行速度は計測しておらず、高速化効果は未確認。
- **次に確認すべきこと:** 実レイヤースクリプトとhost callbackの数値型期待を確認し、int literal変更の互換性を調査する。性能改善は代表的なscriptで別途profileする。



## 2026-10-07 — Mixed ArtifactScript integer/double comparisons also rounded

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`evalBinary()` mixed numeric equality / ordering pathsと、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** int literalをint64化した後も、mixed `int64` / `double` equalityはintegerをdoubleへ変換していた。binary64では`2^53+1`を表現できないため、`9007199254740993 == 9007199254740992.0`が誤ってtrueになり、大小比較も誤り得る。
- **実装:** int/doubleの比較をdouble境界`2^63`で先に範囲判定し、安全な範囲のみdoubleをint64へtruncating castして比較する。fractional doubleはtruncated integerとの残差方向から順序を決める。int-intとdouble-double、mixed arithmeticの経路は維持。
- **確認結果:** `2^53+1`と隣接doubleの`== != < >`、順序を逆にした比較、`INT64_MIN`との等値、`INT64_MAX`と`2^63`の比較をテスト。ArtifactScript関連5 CTest suitesは **5/5 passed**。
- **価値または懸念:** int64精度をmixed comparisonでも保つ。比較用分岐のCPU差は未計測。
- **次に確認すべきこと:** NaN・±infinityを含むmixed comparisonとRelease workloadのprofileを確認する。



## 2026-10-07 — ArtifactScript increment syntax bypassed int64 arithmetic

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のprefix/postfix `++` / `--` parser sugarと、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** 3種類のincrement/decrement文法は、`+=` / `-=`の右辺としてdouble literal `1.0`をASTへ埋め込んでいた。直近のint64 arithmeticを導入しても、これらはmixed int/double経路に入って値の型・精度を失い得る。
- **実装:** 合成literalを`std::int64_t{1}`へ変更。int64のoverflowチェックを含む整数演算経路をそのまま使う。
- **確認結果:** `2^53+1`のpostfix increment、結果のprefix increment/postfix decrementとint64 variant保持、`INT64_MAX++`のoverflow diagnosticを追加。ArtifactScript関連5 CTest suitesは **5/5 passed**。
- **価値または懸念:** 明示的な整数増減が整数演算意味と揃い、doubleへの不要な変換も避ける。速度差は計測していない。
- **次に確認すべきこと:** host-provided数値やint fieldの実利用で`++` / `--` variant変更が期待どおりか確認し、他の合成numeric AST literalも同様の型漏れがないか監査する。



## 2026-10-07 — ArtifactScript logical operators returned operand values

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`evalExpr()` short-circuit branch、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** `&&` / `||`は評価器内の`evalBinary()`ではboolを返すが、短絡対応の早期分岐では左または右operandをそのまま返していた。結果型が式の実行経路で不一致になり、たとえばnumeric operandsの`1 && 2`はint値`2`になっていた。
- **実装:** operand truthinessと短絡条件を保ちつつ、決定値または評価済み右辺をboolへ変換して返す。右辺のscript errorは引き続き伝播する。
- **確認結果:** `1 && 2`と`0 || 2`がboolを返し、`0 && sideEffect()` / `1 || sideEffect()`で右辺を評価しないことを確認。ArtifactScript関連5 CTest suitesは **5/5 passed**。
- **価値または懸念:** 論理式の結果型をboolに揃え、短絡の意味は維持する。現在のtruthiness受理型はbool/int/doubleに限定しており、C#のbool-only operator semanticsとは異なる。numeric operandsを許す既存契約を今回維持したが、これは言語仕様として未整理。
- **次に確認すべきこと:** 他の式コンテキストでlogical resultがoperand valueとして使われている依存を調べ、truthinessとbool-only演算子の方針を言語仕様で確定する。



## 2026-10-07 — ArtifactScript JSON numbers erased serialized int64 precision

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の`ScriptJsonValue` / `parseJsonNumber()` / `jsonToScriptValue()` / `deserializeScriptComponent()`。
- **確認できた事実:** JSON parserは数値tokenを`double`だけで保持していた。serialized int64 `9007199254740993`は再読込時にdouble roundingされ、typed int deserializationとgeneric array conversionは範囲確認のないdouble→int64 castを行っていた。int fieldへのfractional numberも暗黙に切り捨てる。
- **実装:** 整数表記のJSON numberは可能ならexact `int64` payloadを保持する。typed int conversionはfractional/out-of-rangeを拒否し、generic fields/arraysはexact integer tokenを使い、integer token外ではfinite・integral・int64範囲内を検証してからcastする。JSON number parserにも完全token消費を要求する。
- **確認結果:** `2^53+1`、`INT64_MIN/MAX`、array内のlarge intとfractional doubleのcomponent serialization往復、typed intの最大値・overflow・fractional rejectionをテスト。ArtifactScript関連5 CTest suitesは **5/5 passed**。



## 2026-10-07 — Artifact layer project files omitted live script field values

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayerPersistence.cppm`、`ArtifactAbstractLayerImpl.cppm`、`ArtifactAbstractLayer.cppm`、Core `ArtifactScriptSerializedComponent` codec。
- **確認できた事実:** Artifact layer project JSONにはscript componentのenabled状態とbindingだけが保存され、実行中instanceの公開field値と `[SerializeField]` private field値は保存されていなかった。Core codecはserialized field mapとunknown fieldの往復を既にサポートする。
- **実装:** layer JSONに `scriptState` payloadを追加し、binding objectを照合してから同一scriptの状態を復元する。現行definitionでserializedかつvariant型が合うfieldだけを適用し、未宣言unknown値は保持する。scriptファイルが未解決ならpayloadをpendingで維持する。
- **価値または懸念（未検証）:** Project save/loadでスクリプト値を失う統合欠落を埋める。Artifact全体のビルドは変更ファイル到達前に `ArtifactCore/Analyze/ImageAnalyzer.cppm` の既存不完全型エラーで停止したため、Artifact側module compileと実際のproject round-tripは未確認。
- **次に確認すべきこと:** Core dependency failure解消後にArtifact側compileを通し、公開field・`[SerializeField]` private・型変更・missing script・異なるbindingの保存/復元をproject-level integration testで確認する。
- **価値または懸念:** int64を使ったscript stateがproject保存・再読込で正確に戻る。過去に不正なfractional/out-of-range JSONがtyped intとして読めたケースは今後default fallbackになるため、互換性の差は不正値入力に限定される想定。
- **次に確認すべきこと:** Artifact側の実project save/load pathでもserialized componentのlarge int保持を確認し、非標準JSON number tokenを追加でfuzz/testする。



## 2026-10-07 — ArtifactScript split builtin can pre-size its result

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `evalCall()` builtin dispatch、`tests/ArtifactCore/ArtifactScriptTest.cpp`。
- **確認できた事実:** `ArtifactScriptArray::values` は `std::vector<ArtifactScriptValue>` で、既知要素数の配列リテラルでは事前 `reserve()` によりhook中のvector拡張を削減できた。splitの結果要素数も、delimiterの出現回数から生成前に確定できる。
- **実装:** `split(source, delimiter)` はliteral separatorの非重複位置を2回走査し、一度目で要素数を数えてから配列を正確にreserveする。空delimiterはscript errorとし、leading / repeated / trailing separatorsによる空要素を保持する。
- **確認結果:** empty source、delimiter不一致、非重複一致、空要素保持、型不一致、空delimiterを含むテストに加え、layer hookから複数回呼ぶ契約テストを追加。ArtifactScript関連5 CTest suitesは **5/5 passed**。後者は今回focused testを実行して通過。
- **価値または懸念:** 言語機能の追加と同時に、結果vectorの成長再確保を避ける設計にできる。一方、文字列ごとの`substr`と配列value自体の所有 allocationは残る。現時点でsplit固有のallocation / CPU benchmarkはなく、全体の高速化効果は未検証。
- **次に確認すべきこと:** 代表的な短・長文字列や区切り数に対してsplitの割当量とCPU時間を測り、2回走査と文字列コピーのコストを比較する。必要性を計測で確認してからstring-view的な所有権変更を検討する。



## 2026-10-07 — ArtifactScript join can size its output before appending

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `evalCall()` builtin dispatch、`tests/ArtifactCore/ArtifactScriptTest.cpp`、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** 既存の`print` / `log`は複数型を文字列化するが、要素ごとに一時`std::string`を作る。`split`と対になる再結合用途では文字列配列に限定すれば、各要素を直接appendでき、必要な出力サイズを事前に計算できる。
- **実装:** `join(Array<string>, delimiter)` は総バイト数をoverflow-check付きで算出して一度reserveし、その後、要素参照とdelimiterを直接appendする。空配列は空文字列、空delimiterと空要素は許容し、null array・型不一致・混在要素はエラーにする。
- **確認結果:** 空／単一／複数要素、空delimiter、mixed element・argument errorをテストし、layer hookでsplit→joinしたpathの完全往復を確認。join関連のfocused test 3件が通過。87-byte出力を同等のscript `+=` loopと比較するMSVC Debug allocation contractは5 allocations / 160 bytes対12 / 455 per hook。3回×3,000 hooksのCPU中央値（join vs loop）は3×16 chars: 5.14 vs 17.11、8×64: 5.58 vs 37.73、32×256: 6.25 vs 123.96 µs/hook。
- **価値または懸念:** 文字列化の一時値を避け、必要量を一度だけ確保する設計になり、87-byte fixtureではhook内割当を約58%、割当byte数を約65%減らした。CPU fixtureでは要素数増加に伴い差が拡大したが、MSVC Debugの特定script loop比較であり、Releaseや実script全般の短縮率ではない。
- **次に確認すべきこと:** Release workloadと、実際にjoinが使われるlayer scriptでCPU・allocation双方をprofileする。generic stringify joinが必要になった場合は、変換契約と一時割当を別途定める。



## 2026-10-07 — ArtifactScript JIT should follow a stable execution IR

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `evalExpr()`、`execStmt()`、`executeResolvedMethod()`、lookup caches。
- **確認できた事実:** 現在の実行経路は保存済みASTを `evalExpr()` / `execStmt()` で直接再帰評価する。hook / method lookup等の限定cacheはあるが、bytecodeやnative codeを保持する層は見当たらない。`ArtifactScriptInstance::definition()` はmutable accessを許すため、definition lookup cacheのreuseを無効化する経路も存在する。
- **仮説（未検証）:** 言語機能追加とJITを同時に進めるなら、ASTから直接machine codeを出すより、まず意味を一つに保てるbytecode / execution IRとAST interpreterとの共有契約を作る方が、機能間の意味ずれとcache invalidationを抑えやすい。Hot methodだけを後段でnative compileする段階構成が候補。
- **価値または懸念:** 性能改善を継続しながら、JIT導入時に構文・演算子ごとの二重実装を避ける道筋になる。ただしIR設計、definition revisioning、デバッグ位置情報、native backend選定、実 workloadでの損益は未調査であり、JIT実装の採用根拠にはまだならない。
- **次に確認すべきこと:** 代表的なlayer scriptをprofileし、AST dispatchが主要コストかを確かめる。次にbytecode化するstatement/expressionの最小集合と、mutable definition / hot reload時のcompiled cache invalidation契約を調査する。



## 2026-10-07 — ArtifactScript replace can pre-size literal replacement output

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `evalCall()` builtin dispatch、ArtifactScript / layer contract tests。
- **確認できた事実:** `split`と`join`のliteral string handlingが既にあり、string valuesは`std::string`で所有される。標準的な左からの非重複置換では、search回数と最終byte長をappend前に決められる。
- **実装:** `replace(source, search, replacement)` はempty searchを拒否し、一致数を数えてsize overflowを検査した後、出力を一度reserveしてsourceの範囲とreplacementを直接appendする。対象型は3つのstringに限定。
- **確認結果:** 全置換、no match、deletion、overlapping candidate、日本語literal、bad argument / empty searchをテストし、layer hook上の `split` / `join` と組み合わせたpath normalizationを確認。ArtifactScript関連5 CTest suitesは **5/5 passed**。50-byte出力のMSVC Debug allocation fixtureではreplaceが10 allocations / 256 bytes、`join(split(...))` が25 / 704 per hook。3回×3,000 hooksのCPU中央値（replace vs split+join）は3×16 chars: 7.07 vs 13.24、8×64: 6.49 vs 16.83、32×256: 10.49 vs 59.78 µs/hook。
- **価値または懸念:** script側での置換処理に中間配列・部分文字列を作らない専用経路を作れた。このDebug fixtureではhook内割当を60%、割当byte数を約64%減らし、CPU時間も各サイズで短かった。固定Debug fixtureの結果であり、Releaseや実script全般への一般化は未検証。
- **次に確認すべきこと:** Release workloadと実際にreplaceが使われるlayer scriptでCPU・allocationをprofileする。UTF-8 code point基準ではなくliteral byte sequence searchである点を言語仕様に明記するか検討する。



## 2026-10-07 — ArtifactScript contains can overload literal string search

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `evalCall()` builtin `contains` / `indexOf` dispatch、ArtifactScriptとlayer contract tests。
- **確認できた事実:** `contains(array, value)` は既にscript equality semanticsを使う配列検索として実装済みだったが、文字列の部分一致には別の標準関数経路がなかった。`std::string::find` は空needleを含むliteral substring判定を提供する。
- **実装:** `contains(string, string)` を追加し、配列overloadを保つ。case-sensitiveなUTF-8 byte sequenceとして検索し、空substringはtrueを返す。
- **確認結果:** ASCII一致・case mismatch・空substring・日本語substringを検証し、layer hookでseparator検出を確認。ArtifactScript関連5 CTest suitesは **5/5 passed**。
- **価値または懸念:** 配列・文字列の双方に馴染みのある `contains` APIを提供し、検索loopをscriptで書かずに済む。固有の速度・割当量は未計測。検索単位はUnicode code pointではなくUTF-8 byte sequence。
- **次に確認すべきこと:** 長いsourceをfieldから検索する場合の引数コピーをprofileし、文字列評価と検索コストを分ける。必要なら呼び出し引数の参照評価経路を検討する。



## 2026-10-07 — ArtifactScript contains can resolve simple string operands by reference

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptEvaluator::Impl::evalCall()` と、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** 通常のbuiltin call経路では、引数を `ArtifactScriptCallArguments` に評価してからbuiltin dispatchへ進む。simple local / field / `this.field` / literalのstring値であれば、call前に既存保存場所からconst参照で解決できる。現在はこの経路を追加し、他の式・型・lookup failureは従来評価へfallbackする。
- **確認結果:** CMake再実行を避けるため既存build.ninjaの compile / link recipesとmodule mapを使い、変更対象の実装・テストtranslation unitを直接ビルドしてlayer contract executableを更新した。ArtifactBehaviour layer hookとscript object method内の `this.field` を含むcontract suiteは **42/42 passed**。MSVC Debug 134-byte field fixtureでdirect-reference / forced-copyは6 / 21 allocations/hook、96 / 720 bytes/hook、CPU中央値9.44 / 17.48 µs/hook（各3,000 hooks×3回）。
- **価値または懸念:** この固定fixtureではcall argument用string copiesを避け、割当を71%、割当byteを87%、CPU中央値を46%減らした。MSVC Debug測定でありReleaseや実script全般への一般化は未検証。通常のArtifactBehaviourの `this.field` はhost property解決になるため、`this.field` fast pathの統合確認はscript object method内で行う必要がある。
- **次に確認すべきこと:** Release構成または実script workloadでCPU・allocation差をprofileし、literal / local / fieldの各形を分離して比較する。



## 2026-10-07 — Text Animator diagnostics include normal range status messages

- **関連:** `ArtifactCore/src/Text/TextAnimator.cppm` の `evaluateAnimatorWeights()`、`tests/ArtifactCore/TextAnimatorContractTest.cpp`。
- **確認できた事実:** `evaluateSelector()` は正常な範囲評価でも `range evaluated in logical glyph order` などの情報文を返す。`evaluateAnimatorWeights()` は regex 成功時の `regex on ...` だけを除外し、正常な range 情報文は `outDiagnostics` に追加する。複数セットで確認したところ、エラー数より診断数が増える。
- **価値または懸念:** `TextAnimator.ixx` のAPIコメントは「selector problem」を診断へ追加し、`applyAnimatorSets()` は問題を報告したセットごとの診断を説明しているため、利用側がエラーだけのリストと誤認する可能性がある。今回は子リポジトリの実装を変更せず、テストでは無効regex診断とセット順、有効セットの継続適用を別々に検証した。
- **次に確認すべきこと:** 診断APIが情報文も含める設計か、問題だけを返す設計かを決め、必要なら実装またはコメントを揃える。現時点では子リポジトリの変更承認がないため未修正。



## 2026-10-07 — Shaped grapheme metadata can be contract-tested through Text Animator

- **関連:** `tests/ArtifactCore/TextShapingTest.cpp` の Qt shaping fixture と `TextAnimatorEngine::evaluateSelector()`。
- **確認できた事実:** `QtShapingBackend::shape()` が返す `glyphs` を `SelectorEvaluationContext` と `applyAnimatorSets()` へ渡せる。mixed Latin/Hebrew のscript tag selectorが選んだglyphだけにpositionを適用できる。ZWJ family emoji fixtureはshaping contract上1 clusterとなり、AnimatorのCluster単位rangeで全glyphに変換を適用できる。combining mark と1 astral codepointへのregex matchも同じgrapheme cluster内全glyphを選択し、単独astral glyphでは対象glyphだけにposition／opacityが適用された。これらを既存Shaping testに追加し、繰り返し実行で通過した。
- **未検証の観察:** Arabicサンプルでは `logicalToVisual` が `{0, 1, 2, 3, 4}`、`GlyphItem::index` が `{0, 1, 2, 3, 4}` だった。追加したAnimator順序テストは、shaped glyphのコピーをreverseしてlogical順が `GlyphItem::index` で復元されることを確認する。実際のテキストレイヤーがAnimatorへ渡す順序とglyph座標の関係、および `TextSelectorOrder::Visual` の製品仕様は未検証であり、実shaperがvisual並びを返すとは結論づけない。
- **価値または懸念:** 手作りglyph fixtureに加えて実際のshaping metadataをAnimatorへ渡すテストがあると、cluster識別子・source indexの受け渡しずれを早期に検出できる。bidiの最終統合契約は、TextLayerからAnimatorまでglyph配列と座標がどう渡るか確認してから決める必要がある。
- **次に確認すべきこと:** `ArtifactTextLayer` のglyph配列生成・Animator適用順を追い、RTL glyphsの `index`、`clusterIndex`、`basePosition`、`logicalToVisual` の関係を比較してからvisual-order contractを追加する。



## 2026-10-07 — ArtifactScript string indexOf can share reference lookup with contains

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のbuiltin `indexOf` / `contains` dispatch、`ArtifactScriptTest.cpp`、`LayerScriptComponentContractTest.cpp`。
- **確認できた事実:** `indexOf(Array, value)` は既存で、文字列 `contains` はcase-sensitiveなUTF-8 byte sequence検索を行う。文字列 `indexOf` は存在していなかった。
- **実装:** `indexOf(string, string)` を追加。最初の一致を0-based byte offsetで返し、空needleは0、未一致は-1。literal・local・field・script object method内の `this.field` は `contains` と共通の参照fast pathを通し、その他は従来の値評価へfallbackする。
- **確認結果:** ArtifactScriptTest **85/85**、LayerScriptComponentContractTest **42/42**。134-byte fixture内でcontainsとindexOfを各1回/hook呼ぶMSVC Debug比較はdirect-reference / forced-copyで9 / 39 allocations/hook、144 / 1,392 bytes/hook、CPU中央値15.99 / 32.68 µs/hook（3,000 hooks×3回）。
- **価値または懸念:** 固定Debug fixtureではcopy用のallocationが減り、割当を約77%、bytesを約90%、CPU中央値を約51%削減した。Releaseおよび実script全般への一般化は未検証。Unicode code pointではなくUTF-8 byte offsetを返す。
- **次に確認すべきこと:** Releaseまたは代表的なlayer scriptでallocationとCPUを分けて計測し、byte offset契約が利用側のpath/text用途に合うか確認する。



## 2026-10-07 — ArtifactScript JIT は interpreter の実測後に判断する

- **関連:** `docs/planned/MILESTONE_ARTIFACTSCRIPT_LANGUAGE_EVOLUTION_2026-08-21.md` の対象外項目、`ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` のAST tree-walk evaluator、`tests/ArtifactCore/LayerScriptComponentContractTest.cpp` の固定Debug microbenchmarks。
- **確認できた事実:** 現行実行器はASTを再帰評価し、言語Evolution計画はbytecode VM / JIT置換を速度不足の実測後まで保留している。今回の文字列検索4種fixtureでは参照経路のほうがコピー経路より割当とCPU中央値が低かったが、JIT導入可否を示す比較ではない。
- **価値または懸念（未検証）:** JITは実装コストだけでなく、module/ABI境界・ホットリロード・診断位置・デバッグ実行の維持が必要になる。まずReleaseまたは代表的なLayerScriptでCPU、割当、AST node別の実行頻度を計測し、時間の大半を占める経路を特定する。必要性が確認された場合も、既存AST evaluatorを残したbytecode実験経路からA/B比較する方が退行を見分けやすい可能性がある。
- **次に確認すべきこと:** Release構成のベンチマーク実行方法を整え、複数の実スクリプト相当workloadでAST評価の時間内訳を採取してからbytecode/JITの試作範囲を判断する。



## 2026-10-07 — Seeded edit-sequence stress tests can cover cross-feature lifecycle faults

- **関連:** `tests/Artifact/`, `ArtifactProjectService`, Layer / Keyframe / Precompose / Undo / Asset reload workflows。
- **確認できた事実:** ルート `tests/` は GoogleTest / CTest を使うが、`ArtifactProjectService.cppm` は `Artifact.exe` の実装ソースに属し、現在の test runtime target からリンクされない。service constructor は file watcher / external-control / event / selection 経路を初期化する。`Impl::handleFileChanged()` は AssetManager の source version を無効化して変更イベントを発行する。`AssetManager::invalidateSource()` 単体は外部ファイルを再読込しない。Undo API には stack depth と layer property keyframe command がある。既存 `ARTIFACT_RUN_BUILTIN_TESTS` は `QApplication` 後・MainWindow 作成前に動き、help 上は exit を約束する。
- **実装:** Artifact built-in test suite に3 seeds × 2048 stepsの決定論的な edit-sequence fuzzer を追加。Layer add/remove、Keyframe command、Precompose、Undo、Redo、静止画 Asset の file watcher reload を通し、毎操作後に ID、nest source、keyframe、Asset identity を検証する。Asset reload は source version 更新だけでなく、Image Layer の非同期再デコード後にバッファ画素が変わることも確認する。失敗ログにはseed、step、直近16操作と対象IDを出す。seed / steps の環境変数指定と、異常終了時にも操作列が残る逐次 trace 保存を追加し、test runner は成功・失敗どちらも終了コードでプロセスを閉じる。
- **2026-10-08追記:** `ARTIFACT_EDIT_SEQUENCE_FUZZ_SEED` / `_STEPS` による再実行範囲の指定を加えた。trace file 指定時は各操作の実行前に step と操作を追記して flush するため、プロセスクラッシュ後も最後に開始した操作を特定できる。複数seedのtraceは1ファイルへ連結する。Precompose検証は子Composition内のLayer IDと親Layerへの逆参照まで見る。Undo/Redoも履歴stackの増減を検証し、no-opを通過扱いしない。Layer add/remove/Precomposeも、成功戻り値だけでなく親Compositionの構造差分を確認する。Undo履歴消去が成立しない場合は外部履歴を操作せず、中断する。
- **2026-10-08 ビルド確認:** `Artifact` build は `ArtifactCore/src/Analyze/ImageAnalyzer.cppm` の `Image.ImageSurfaceView` module reference 欠落（MSVC C2230）で停止した。fuzz source / runtime は未確認。ArtifactCore は変更せず、ログを `temp/edit_sequence_fuzz_build_2026-10-08.log` に保存。
- **未確認:** fuzz module / Artifact.exe のソースコンパイルと実行による runtime確認。
- **価値または懸念:** seed と操作 trace を固定した短いシーケンスに加え、数千〜数万回の soak 実行があると、個々のコマンドテストでは見えにくい所有権・Undo・ネスト・Asset 参照の組み合わせ不整合を再現可能にできる。大量操作の常時 CI 化は実行時間が増えるため、短い通常ケースと長い soak を分けるのがよい。
- **次に確認すべきこと:** `ARTIFACT_RUN_BUILTIN_TESTS=1` で実行し、3 seedすべてで全操作数・Asset watcher更新・不変条件が通ることを確認する。失敗したらseedとtraceで再現し、traceを縮小する運用を整える。



## 2026-10-06 — Python スクリプトの ARTIFACT 環境参照

- **関連:** `Artifact/src/Export/Python/ArtifactPythonAPI.cppm` の `registerUtilityAPI()`。
- **確認できた事実・対応:** embedded Python 用に `artifact.environment.get(name, default)` / `has(name)` / `names()` を追加し、`ARTIFACT_*` 環境変数だけを読み取り専用で公開した。一般のプロセス環境には資格情報などが含まれる可能性があるため、アプリ固有 prefix で境界を設けた。値は実行時に読むため、プロセス環境に対する既存の動的な参照性を保つ。`ArtifactStartup.json` の設定値は環境変数とは別系統のためこの API には含めない。
- **価値または懸念（未検証）:** アプリの Python menu / hook / CLI script から診断用 `ARTIFACT_*` 値を同じ API で確認できる。外部 Python fallback はこの in-process API を持たず、実行検証もしていない。
- **次に確認すべきこと:** ビルド許可後、未設定・設定済み・空文字・既定値・非 `ARTIFACT_` 名と Windows の Unicode 値を GUI / CLI の embedded Python で確認する。



## 2026-10-06 — ArtifactScript コンポーネントの環境参照

- **関連:** `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` の `ArtifactScriptHost`。
- **確認できた事実・対応:** レイヤーの Script component は Python ではなく `ArtifactScriptHost::global()` を通る独自ランタイムで、環境参照関数が未登録だった。既存の `EnvironmentVariableManager` を通じて `getEnv(name[, default])` / `hasEnv(name)` を登録し、Python API と同じ `ARTIFACT_*` 名だけを読み取り可能にした。
- **価値または懸念（未検証）:** Python menu scripts と Layer Script component scripts の両方でアプリ固有の環境変数を参照できる。ArtifactScript の host は process-wide global なので、この関数は同ランタイムを使う全スクリプトから利用可能になる。実行・ビルドは未確認。
- **次に確認すべきこと:** ビルド許可後、Layer Script component の hook 内で既知の `ARTIFACT_*` 値、未設定値の default、一般環境名の拒否を確認する。



## 2026-10-06 — UI visual loop の capture 依存と Widget test seam

- **関連:** `tools/ui_visual_loop.py`、`tools/ui_visual_compare.py`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **確認できた事実（静的読み取り）:** visual loop は capture command を外部必須依存としており、Timeline 専用 fixture/capture と Render Queue 専用 fixture/capture は repo に見当たらない。Timeline constructor は多数のグローバルサービス／widgetを集成し、RenderQueueManagerWidget は `ArtifactRenderQueueService::instance()` を生成時に取得する。同 service constructor は `ArtifactAppSettings::instance()` と永続キューを読み込む。RenderJobModel contract suite は存在するが、Widget interaction / pixels の証拠ではない。
- **改善:** visual loop に待機なしの単発比較と、指定秒数ごとの自動再撮影を加えた。capture コマンド自身が固定状態を再現する責務は変わらない。
- **価値または懸念（未検証）:** 人手修正を挟む既定フローに加えて、アプリ起動・captureを外部で繰り返すrunnerを接続可能になった。一方、実UIの状態注入、service隔離、承認基準画像がないため、UI gate 完了とは扱えない。
- **次に確認すべきこと:** (1) service/fixture境界をCMake独立ターゲットに抽出できるかArtifact側の依存閉包を確認する。(2) TimelineとRender Managerそれぞれの固定状態生成・操作・capture runnerを用意する。(3) runner環境を固定し、承認済みbaselineを用意する。(4) ユーザーが許可した後に限りUI suiteを実行し、interactionとpixel exactの結果を得る。



## 2026-10-06 — テスト契約と実装改善候補の区別

- **関連:** `tests/ArtifactCore/TextAnimatorContractTest.cpp`、`tests/ArtifactCore/RenderImageContractTest.cpp`、ArtifactCoreのText Animator／Image実装。
- **確認できた事実:** Text Animatorのfield extra weightへNaNを渡すと最終weightへNaNが伝播し、+Infは`std::clamp`により上限へ丸められる。Image cropのAPIコメントは範囲外なら空画像を返す契約を明記し、現実装もそう動作する。`alphaBlend`は色descriptorを更新しないが、異なるdescriptor入力時の結果descriptor契約は公開コメントから確認できない。
- **設計判断:** このテスト整備では、既存の公開契約に記載のないsanitizeやdescriptor変更を期待するcaseは登録しない。既知の差異・未定義の契約は計画書に残し、子repo実装修正が許可される場合に仕様を先に決める。
- **価値または懸念（未検証）:** 期待動作を先取りしたテストによる誤検出を減らせる一方、非有限値の扱いやalpha blend後の色意味論は仕様化が必要。
- **次に確認すべきこと:** sanitize方針とdescriptor伝播の規則を決めてから、Core実装とcontract suiteを同じ変更で更新する。



## 2026-10-06 — UI visual comparison の領域別合否

- **関連:** `tools/ui_visual_compare.py`、`tools/ui_visual_loop.py`。
- **確認できた事実:** 比較器は従来、region metricsをreportへ記録するだけでregionごとのpass/fail制約がなく、画像寸法が異なる場合はregionsを一切出力していなかった。
- **対応:** `--region-limit NAME,MAX_DIFF_PIXELS,MAX_DIFF_FRACTION` を追加し、全体制約に加えregion固有制約も合格条件へ含めた。寸法不一致は常に全体不合格としながら、透明canvas上のregion metricsと各region gateの判定も保存する。寸法差時も未知region名を拒否する。loop runnerからもregion limitsを渡せる。
- **価値または懸念（未検証）:** UIの主要領域（例: Timeline ruler / tracks）ごとに厳しいpixel gateを持てる。real UI baseline/capture fixture上での実行はまだない。
- **次に確認すべきこと:** 承認済みTimeline／Render Manager baselineと固定runner環境で領域境界・gate値をレビューし、pixel-exact loopを接続する。



## 2026-10-06 — UI visual comparator の回帰用 unittest

- **関連:** `tests/ui_visual/test_ui_visual_compare.py`。
- **対応:** region gate、pixel-exact成功、per-channel tolerance境界、寸法差診断、同寸法／寸法差双方の未定義region指定拒否を検査する6ケースを追加した。
- **価値または懸念（未検証）:** UI本体のcapture fixtureが未完成でも比較ゲート自体の契約を継続的に保護できる。今回のturnではAGENTS.mdの明示許可条件に従いsuiteを実行していない。
- **次に確認すべきこと:** `python tests/ui_visual/test_ui_visual_compare.py` を実行し、続いて実UI fixture上でcapture loopとregion boundaryを確認する。



## 2026-10-06 — ArtifactScript プロパティ／キーフレーム API 拡充（スライス1〜3）

- **関連:** `ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx`（`ArtifactScriptCompositionApi` に任意フィールド + `KeyframeRow` 追加）、`ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm`（`installCompositionApi` の登録ラッパ）、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`（`installCompositionScriptApi()` への実注入）、`tests/ArtifactCore/ArtifactScriptHostApiTest.cpp`。
- **確認できた事実・対応:** スライス1（`hasProperty` / `getPropertyNames` / `isAnimatable` / `hasKeyframes` / `getKeyframeCount` / `hasKeyframeAt` / `getValueAtFrame`）、スライス2（`addKeyframe` / `removeKeyframe` / `clearKeyframes`）、スライス3（`getKeyframes` → クラス名 `Keyframe` の ObjectInstance 行配列）を追加。キー時刻は `layer->keyframeTimeAtFrame(frame)` に固定し、Insight 2026-09-17 の「レイヤーがキー時刻ドメインの唯一の定義元」規約に従った。interp 名は `WorkspaceAutomation::setKeyframe` の別名表と揃え、追加時 `setAnimatable(true)` + `layer->changed()` で Timeline 経路と挙動を一致させた。コールバック未設定時は登録を省略する optional 契約で既存ホスト互換を維持。ターゲット解決は既存 `getProperty` と同一規約。
- **価値または懸念（未検証）:** スクリプトからキーの一括生成・整え（例: OnUpdate での補助キー付与）が可能になった。一方 (1) キー書込みは Undo 履歴を bypass する（既存スクリプト `setProperty` と同じ直接経路。OnUpdate 毎フレーム書込みを履歴化すると履歴が爆発するため意図的に非履歴）、(2) interp の未知名は警告なく Linear に落ちる、(3) `getKeyframes` の戻りを `rows[i].frame` のように Index→FieldAccess 連鎖で読む構文はパーサ未検証、(4) ビルド・ctest 未実行。
- **次に確認すべきこと:** ビルド許可後に `ArtifactCoreArtifactScriptHostApiTest` を含む ArtifactScript 系テストの実行。実機で OnUpdate 中の `addKeyframe` が Timeline 表示・保存／再読込・再生評価と一致すること。interp 未知名の診断化（`setLastError`）は必要性を見て追加検討。



## 2026-10-06 — WorkspaceAutomation::setKeyframe の時刻 scale がコンポ fps double をそのまま切捨てている疑い（既存）

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx:5088-5089`（`const auto frameRate = std::max<double>(1.0, ...); const RationalTime time(frameNumber, frameRate);`）。
- **確認できた事実（静的）:** `RationalTime(int64_t, int64_t)` コンストラクタに double を渡しているため、29.97fps など小数 fps では scale が 29 へ切り捨てられる。Insight 2026-09-17 では VP／Undo 系の同種バグ（double fps を RationalTime へ直接渡す切り捨て）が修正済みで、キー時刻ドメインは `FrameRate::storageScaleForFps`（丸め）＋レイヤー `keyframeTimeScale()` に統一された。この経路はその対象外だった。
- **価値または懸念（未検証）:** 事実なら、AI / automation 経由で NTSC 系 fps のコンポに書いたキーだけが別 scale に置かれ、以降の `hasKeyFrameAt` / 評価が別フレームを指す。24/30/60fps では無害。
- **次に確認すべきこと:** 29.97fps コンポで `WorkspaceAutomation.setKeyframe` を呼び、`getKeyFrames()` の `time.scale()` とレイヤーの `keyframeTimeScale()` の一致を確認。不一致なら `layer->keyframeTimeAtFrame(frame)` へ統一する。



## 2026-10-06 — Blur GPU descriptor と実行 backend の境界

- **関連:** `Artifact/include/Effects/Blur/BlurEffect.ixx`、`Artifact/src/Effects/Blur/BlurEffect.cppm`、`Artifact/src/Render/ArtifactCompositionRenderController.cppm`、`tests/Artifact/BlurEffectContractTest.cpp`。
- **確認できた事実（静的読み取り）:** Blur のresident GPU空間ノード採用条件は premultiplied、strengthほぼ1、sigma<3、GPU stack容量内のpass数で制限される。条件外はdomainがNoneとなり、CPU implementationへ戻る。旧 `BlurEffectGPUImpl` には upload/dispatch/readback と `WaitForIdle()` が残るが、同ファイルの説明では現composition経路から到達せず、条件外でもCPU fallbackする。CMakeを読むと、`ArtifactEffectsBlur` targetが所有するのは別の3つのblur実装であり、通常の `BlurEffect` moduleはこのtargetに含まれずArtifact executableのsource群に残っている。
- **価値または懸念:** GPU node descriptorだけのテストは、実GPU shader parityやbackend選択の証明にならない。suite結果にはdescriptor contractとpixel backend parityを分けて報告する必要がある。
- **次に確認すべきこと:** 通常Blur implementationをArtifact.exe非依存でlinkできるtarget seamを設計レビュー付きで抽出する（現時点ではArtifact submoduleを編集せず、実行テストを追加していない）。その後、実際のrender planを通す小画像GPU fixtureを用意し、選択backend/fallback理由とCPU基準との差分を記録する。旧readback実装を新たなテスト経路から呼び出さない。



## 2026-10-06 — Text Animator extra weight の非有限値がClampを通過

- **関連:** `ArtifactCore/src/Text/TextAnimator.cppm` の `evaluateAnimatorWeights()`、`tests/ArtifactCore/TextAnimatorContractTest.cpp`。
- **確認できた事実（静的読み取り）:** 合成値は `std::clamp(weight * extraWeight, 0, 1)` に直接渡される。C++ comparator semanticsではNaNは比較がfalseとなりNaNのまま返り、+Infは上限1へclampされる。これによりglyph weightのfinite invariantを破り、invalid external layer/effect weightがselector結果に影響する。
- **設計判断:** 非有限extra weightの処理契約は公開仕様に記載されていないため、実装変更を伴わない現テストsuiteには期待値として追加しない。
- **価値または懸念（未検証）:** NaNの伝播はglyph transform/opacityへ波及する可能性があるが、対応値と互換性を先に定義する必要がある。
- **次に確認すべきこと:** Core変更が許可された作業でsanitize規則を仕様化し、contract suiteと実装を同時に更新して実行する。



## 2026-10-06 — float RGBA Image API のweight/opacity境界

- **関連:** `ArtifactCore/src/Image/ImageF32x4_RGBA.cppm` の `blend()` / `alphaBlend()`、`tests/ArtifactCore/RenderImageContractTest.cpp`。
- **確認できた事実（静的読み取り）:** Image API はweight/opacityを引数のまま `cv::addWeighted` またはalpha演算へ使い、finite検証・0..1 clampを行わない。既存のLayerBlend Pipelineには独立してopacity finite guard/clampを行った記録があるが、低レベルImage APIでは同じ入力契約を保証していない。
- **対応:** RGBA unit contract suiteでは、公開契約に記載された範囲内の画素処理とdescriptor動作を検証する。範囲外weight/opacityと負crop寸法は、API仕様が未確定のためsuiteへ含めていない。
- **価値または懸念（未検証）:** 実装はweight/opacityをclampせず画像演算へ渡す。呼び出し側が範囲を保証しているか、低level APIがsanitizeすべきかを決める必要がある。
- **次に確認すべきこと:** 公開仕様と既存callerを調査し、range policyを決めてから対応pixel casesを追加する。



## 2026-10-06 — Levels per-channel property path のvalidation差

- **関連:** `Artifact/src/Effects/ColorCorrection/LevelsEffect.cppm` の `LevelsEffect::setPropertyValue()`、`tests/Artifact/ColorCorrectionEffectContractTest.cpp`。
- **確認できた事実（静的読み取り）:** master の `levels.master` string property は数値をfinite確認し、 black/white/gamma/output rangeを整合させる。一方 `Red/Green/Blue ...` 個別 property path は `QVariant::toDouble()` の結果を対応フィールドへ直接代入し、finite・相互範囲・gamma範囲を検証しない。 per-channel CPU処理はその値を `applyLevels()` へ渡す。
- **対応:** per-channel各RGB curveをproperty API経由で独立制御するpixel contract caseを追加した。追加caseでは各channelが正常値のため、invalid property値の修正までは行わない。
- **価値または懸念（未検証）:** Property editor経由で不正・非有限値が到達した場合、出力画素へNaN/Infが伝播する可能性がある。有限値を直接セットできるテストだけではこの安全性を証明できない。
- **次に確認すべきこと:** Artifact変更が許可された作業で per-channel property のfinite fallbackとrange invariantを定義し、master parserと同じ正規化契約にできるか確認する。ビルド/CPU画素テストでNaN/±Inf・逆転範囲・zero gammaを検証する。



## 2026-10-06 — Color effect の独立target ownership gap

- **関連:** `Artifact/cmake/ArtifactSources.cmake`、`Artifact/CMakeLists.txt` の `ArtifactEffectsColor`、`tests/Artifact/CMakeLists.txt`。
- **確認できた事実（静的読み取り）:** `VibranceEffect` / `PosterizeEffect` / `ThresholdEffect` はapp source manifestのmodule/implementationに存在するが、`ArtifactEffectsColor` targetの明示source listから除外されている。Color Correction suiteは同targetだけをlinkする。
- **対応:** これら3 moduleのimport・pixel/descriptor casesを軽量Color suiteから除いた。suiteに存在しないtarget-owned BMIへ依存する可能性を避けた。planにownershipと必要なseamを記録した。
- **価値または懸念:** suiteをArtifact.exe相当の依存へ広げず、テスト対象library所有境界とmodule importを一致できる。3 effectの直接テストcoverageは独立target seamができるまで欠ける。
- **次に確認すべきこと:** Artifact submodule変更が許可された場合、これらが他 effect packと共通の小さなtargetへ安全に抽出できるか依存closureを確認する。代替として親側からsource moduleを専用test libraryへ登録する方法はCMake module/file-set境界のレビュー後に判断する。現時点ではArtifact submodule・CMakeを変更しない。



## 2026-10-06 — Lift/Gamma/Gain の CPU と resident GPU alpha 処理差（未検証）

- **関連:** `Artifact/src/Effects/LiftGammaGainEffect.cppm`、`tests/Artifact/ColorCorrectionEffectContractTest.cpp`。
- **確認できた事実（静的読み取り）:** CPU実装 `applyLiftGammaGainCore()` は source descriptor がpremultipliedなら alpha でRGBをunpremultiplyし、処理後にalphaを戻す。一方、同ファイル冒頭の `kLiftGammaGainResidentHlsl` は `pixel.rgb` を直接処理してから `pixel.a` を維持する。`appendGpuSpatialNodes()` はresident generic keyと3群parameterをspatial stackへ登録する。
- **価値または懸念（未検証）:** resident spatial path がpremultiplied surfaceへ適用される場合、低alpha画素のCPU結果とのRGB差が大きくなり得る。別GPU実装のHLSLにはalpha-aware処理があるため、実際の経路選択とresident shaderの入力alpha契約を確認するまで不具合とは断定できない。
- **次に確認すべきこと:** 固定の半透明・premultiplied画素で `applyCPUOnly()` と実resident spatial dispatchを比較し、選択backend、descriptor、alpha mode、fallback理由を記録する。GPU実行はユーザーの明示指示後に行う。



## 2026-10-06 — Text Render Target test のmodule link owner

- **関連:** `tests/Artifact/TextRenderTargetContractTest.cpp`、`tests/Artifact/CMakeLists.txt`、Artifactの`ArtifactTextRenderTargetRuntime` / `ArtifactTextGlyphSubmitterRuntime`。
- **確認できた事実（静的読み取り）:** test sourceは`Artifact.Render.TextRenderTarget`と`Artifact.Render.TextGpuDevice`をimportする。前者のCMake module interfaceは`ArtifactTextRenderTargetRuntime`が所有するが、test targetは後者だけをlinkしていた。`TextGpuDevice`はsubmitter runtimeが公開する。
- **対応:** 親側test targetから両runtimeをlinkするよう修正し、module ownerとAPI提供元を明示した。
- **価値または懸念（未検証）:** C++ modulesのBMI依存を正しいtarget dependencyへ接続できる。実際のconfigure/buildでBMI解決とlinkが通るかは未確認。
- **次に確認すべきこと:** 許可後にArtifactTextRenderTargetContractTestのtargetをconfigure/buildし、D3D12 hostでCTestを実行する。



## 2026-10-06 — GPU glyph submitter の画素契約suite

- **関連:** `tests/Artifact/TextGlyphRenderContractTest.cpp`、`tests/Artifact/CMakeLists.txt`、既存Artifact text glyph submitter API。
- **対応:** headless QGuiApplicationと既存D3D12 glyph submitter境界を利用し、5つのGPU pixel integration caseを追加した。要求色を持つ実グリフ画素、同一入力の決定性、zero opacity、非有限transform拒否、`TextAnimatorEngine::applyAnimatorSets`を通したAnimator→GPU glyph描画とalpha総量低下を確認する内容。
- **価値または懸念（未検証）:** これまでのGPU target clear/readback smokeを越え、shader/atlas/vertex/pipeline/submitter/render target/readback経路を同じsuiteで通せる。機械的なフォント選択差を避けるためpixel golden hashではなく、同一process再現性と色・可視性契約をassertする。GPUが使えない環境はskipとなるためCIでは実GPU runnerを別に必要とする。
- **次に確認すべきこと:** configure/build/CTest許可後にMSVC modules依存を確認し、対応Windows D3D12 runnerで実行する。次段階でArtifactTextLayer／composition統合とkeyframe時系列render goldenを追加する。



## 2026-10-06 — ArtifactCore text runtime target の旧名参照

- **関連:** root `CMakeLists.txt`、`Artifact/CMakeLists.txt` のGPU text runtimes、`ArtifactCore/CMakeLists.txt`。
- **確認できた事実（静的読み取り）:** ArtifactCoreはtext module ownerを`ArtifactCoreText`として定義するが、Artifactの`ArtifactTextGlyphSubmitterRuntime`と`ArtifactTextGlyphSmoke`は`ArtifactCoreTextRuntime`をlinkする。repoのCMake定義中に後者のtargetは存在しない。
- **対応:** 子repoを変更せず、rootでArtifactCore追加後・Artifact追加前に`ArtifactCoreTextRuntime`を`ArtifactCoreText`へのALIASとして公開した。
- **価値または懸念（未検証）:** 軽量glyph renderer runtimeのmodule/implementation依存を現target ownerに接続できる。CMake configureでALIAS scopeとMSVC BMI linkが成立するかは未確認。
- **次に確認すべきこと:** 許可後にtests有効／無効の両方でconfigureし、2つのArtifact GPU text runtimeがArtifactCoreText BMIを消費できることを確認する。



## 2026-10-06 — Text Animator GPU fixture のIndex domain

- **関連:** `tests/Artifact/TextGlyphRenderContractTest.cpp`、`ArtifactCore/src/Text/TextAnimator.cppm`。
- **確認できた事実:** `SelectorUnits::Index` は`glyphIndex`を0-basedで位置として使用する。fixtureは単一glyph（index 0）だったため、範囲1を指定するとAnimatorは適用されない。
- **対応:** Animator→GPU render integration caseのstart/endを0へ修正し、assertするposition/rotation/scale/opacityが選択glyphへ実際に適用されるようにした。
- **価値または懸念（実行未確認）:** 未選択の入力をGPUへ渡し、別理由の画素差だけで誤合格する可能性を防ぐ。C++ suiteは未実行なので、実際のmodule/CTest結果は未確認。
- **次に確認すべきこと:** 許可後にTextAnimatorEngineの適用値assertとalpha総量差をD3D12 runnerで実行する。




## 2026-10-05 — 効果の「②型（登録済みで無言の素通し）」機械検査を完了、144/145 が健全

- **関連:** `Artifact/src/Service/ArtifactEffectService.cppm`（`createEffect()` が生成する 145 個の concrete 型）、`Artifact/include/Effects/Render/PBRMaterialEffect.ixx`、`Artifact/src/Widgets/InspectorEffectCatalog.cppm:46`。
- **着手の経緯:** 2026-09-30 のエントリが「① CompileEnum misspelling / ② Impl 未登録で apply の素通し / ③ UI 未登録」の3分類を実例で確定し、次回の検査手順として「`createEffect` が生成しうる全 effect 型に対し `setCPUImpl|setGPUImpl` の呼出が1つ以上あるか」を grep で機械検査すると提案していた。本日これを実行した。
- **検査方法（実測・再現可能）:** `createEffect()` 内の `std::make_unique<T>` から型名を抽出し（抽象基底 `ArtifactAbstractEffect` と Core-creative adapter `ArtifactCoreCreativeEffect` は除外）、`Artifact/include`・`Artifact/src`・`ArtifactCore/include`・`ArtifactCore/src` の全 2768 ファイルに対して **その型名を mention する全ファイル**を対象に `setCPUImpl` / `setGPUImpl` / `apply()` override / `appendGpuPointwiseNodes` の有無を集計した。教訓として記録する価値がある点: 効果の宣言（`.ixx`）と実際の登録（ctor 内の `setCPUImpl`）は別ディレクトリに分離して存在することがあるため、宣言ファイルとその sibling だけを見ると誤検出になる。初回実行で 94 件を誤検出したのはこのため。
- **結果:** 145 型中、CPU+GPU 両実装登録 94 / CPU のみ 48（GPU 未登録は正常） / **②型（`setCPUImpl`・`setGPUImpl`・`apply()` のいずれもなく、無言で `dst = src.DeepCopy()` の素通しになる）の残存は 0**。9/30 の `OpticsCompensationEffect` は既に修正済みで、同種の残存 defect は無い。**既存テストでは捕まりにくいという 9/30 の懸念は、在庫としては解消された**。
- **残る1件の性質（②型ではない別カテゴリ）:** `PBRMaterialEffect`（`Artifact/include/Effects/Render/PBRMaterialEffect.ixx:48-281`）は②型ではない。ctor で `setPipelineStage(EffectPipelineStage::MaterialRender)` するのみで、`setCPUImpl`/`setGPUImpl` も `apply()` override も持たない **パラメータキャリア型**。15項目の material パラメータを保持し `toMaterial()` で `ArtifactCore::Material` を構築するが、**`toMaterial()` の呼び出し元はリポジトリ全体でゼロ**（grep で `Artifact/src` にヒットなし。ヒットは `Artifact_dev_review/` の旧 fork とドキュメントのみ）。UI 登録は揃っている（`ArtifactEffectService.cppm:747` の `createEffect("pbr_material")`、`:1297` の `availableEffects()` push、`InspectorEffectCatalog.cppm:46` のカタログエントリ）。したがって現状は「Inspector から追加でき、15項目のパラメータを編集できるが、レンダラーはどこもそれを読みない」状態。**本件の真の意味は「3D レイヤーの材質適用経路が未接続」である**（UI 側の箱は完成済みなので、本効果クラスを触っても結果は出ない）。3D は開発優先方針上低位のため、現時点では着手しない判断が妥当。
- **未検証:** 本検査はソース静的な grep であり、コンパイル・実行はしていない（AGENTS.md によりビルド禁止）。`setCPUImpl` は呼んでいるが引数の中身が空のような「登録はあるが中身が空」の変種は、本検査では検出できない。検出範囲は「登録が1本も無い」ケースに限る。
- **次に確認すべきこと:** 将来①型（CompileEnum 誤り）や③型（UI 未登録）を機械検査する場合は、本検査の走査方法（型名を mention する全ファイルを横断）を再利用する価値がある。PBR を着手するなら、先に「3D レイヤーの材質適用経路が存在するか」を別途確認する。



## 2026-10-05 — シェイプレイヤーの全プロパティが Inspector に到達できない

- **関連:** `Artifact/src/Widgets/ArtifactPropertyPresentation.cppm:26-63`（`propertyPresentationProfile()`）、`Artifact/src/Layer/ArtifactShapeLayer.cppm:5669-6400`（`getLayerPropertyGroups()`）、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm:403-429` / `:431-462`。
- **確認できた事実（実コード読みで確定）:** `propertyPresentationProfile()` の分岐は Text（`:28`）/ `solid.color` 所持（`:33`）/ `image.sourcePath` 所持（`:41`）/ `geometry.width` かつ `geometry.depth` 無し（`:55`）/ `basic` フォールバック（`:61`）の5系統のみで、`shape` プロファイルは存在しない。`ArtifactShapeLayer` は `solid.color` / `image.sourcePath` / `geometry.width` をいずれも持たない（プロパティは全て `shape.*` 名で、`ArtifactShapeLayer.cppm:5685` 以降）。したがって必ず `basic` に落ち、許可リストは `{"Initial","Transform"}` だけになる。その他のグループ名は `Shape`（`:5683`）/ `Appearance`（`:5721`）/ `Shape Parameters`（`:5977`）/ `Contents`（`:6056`）/ `Shape Stack`（`:6220`）/ `Operator %1 (%2)`（`:6259`）で、全て `ArtifactPropertyWidget.cppm:2542-2545` の `presentationAllowsGroup()` 判定で `continue` スキップされる。
- **失われているもの（実測）:** Stroke の taper 3 項目（`shape.strokeTaperStart/End`・`shape.taperEase`, `:5841-5863`）、wave 4 項目（`:5870-5897`）、stroke gradient 3 項目（`:5903-5930`）、`shape.dashOffset`（`:5964`）、shape parameters（`shape.cornerRadius` / `starPoints` / `starInnerRadius` / `polygonSides`, `:5979-6036`）、Contents の複数図形合成と個別トランスフォーム（`:6056-6217`）、Operator 群のパラメータ（`:6259` 以降、Repeater は 9 項目）。幅と高さのみツールバー経由で変更可能。`ArtifactPropertyWidgetShared.cppm:403-429`（Appearance のグラデーションフィルタ）と `:431-462`（Contents の `shape.content.N.fillType` フィルタ）は Shape 専用に書かれた完成コードだが到達不能の死にコードになっている。
- **修正方針と唯一の継ぎ目:** `ArtifactPropertyPresentation.cppm` に `shape` プロファイルを追加し、`Artifact.Layer.Shape` の import を1行足すのが基本形。**ただし Operator グループは `setName("Operator %1 (%2)")` と動的命名のため、許可リストの完全一致（`presentationAllowsGroup()` は `QStringList::contains()`）では書けない。** 第三次案として「許可リストに無いグループでも接頭辞一致で通す」例外ルールを1つ追加する案が有力で、既存の Text Animator 例外（`shouldHideTimelinePropertyGroup` の `PropertyGroup` 版オーバーロイド、`text.animators.<index>.*` のパス判定）も表示名一致ではなくパス判定で実装されているため、同じ形式に寄せるのが整合的。
- **影響範囲:** Inspector のみ。タイムライン左ペインは `computeTimelineHiddenLayerPropertyGroup`（`ArtifactAbstractLayerUtilities.cppm:99-102`）が `Transform` 以外を隠す別経路なので変更なし。AGENTS.md の「タイムライン左ペインは Transform のみ」規制には抵触しない。
- **未検証:** 本日ビルド・実機確認はしていない（AGENTS.md によりビルド禁止）。上記はソース読解に基づく。GUI 上での実際の見え方（グループが展開可能か、Operator の行数増による可読性）は未確認。
- **次に確認すべきこと:** ビルド許可後に (1) シェイレイヤーを選択して6グループが現れること、(2) Operator が動的名前でも通ること、(3) Contents のグラデーションフィルタが実際に効くこと、(4) 各プロパティの Undo/Redo と保存再読込が成立すること、を実機で確認する。



## 2026-10-05 — キーの仮状態は表示側だけに持たせない

- **関連:** `Property.Abstract::KeyFrame`、`PropertySerializationBridge`、Timeline snapshot/clipboard、layer/text/deformationの独自保存、gizmo/Undoのキー復元。
- **確認できた事実・今回の対応:** キーは複数の編集経路でaddKeyFrameから再構築される。Timelineのmarkerにだけ仮状態を持たせると移動やUndoで失われるため、Coreのキーに既定falseのsoft属性を追加し、既存metadataを引き継ぐ経路にも伝播した。通常の補間評価は変えず、選択キーのソフト化・確定・破棄を既存snapshot commandで扱う。新規soft追加は既存キーを上書きせず、破棄はsoftだけを削除する。
- **懸念・仮説（未検証）:** キー属性の復元が多数箇所に分散しており、今後の属性追加でも漏れが発生し得る。メタデータを含むキー全体の復元APIを将来整理できる可能性があるが、今回は既存保存形式・module依存・編集契約を広く変更しない。旧版アプリで再保存した場合の新属性保持は保証できない。
- **次に確認すべきこと:** 許可後にCore/Artifactをビルドし、仮/通常の混在選択、Undo/Redo、移動・複製・保存再読込、独自deformation/text経路を実機確認する。今回はビルド・テスト・実機確認をスキップしている。



## 2026-10-05 — 調整レイヤーのGPU常駐対応を限定拡張

- **関連:** `ArtifactCompositionRenderController::drawGpuLayerToIntermediate()`、`ChannelMixerEffect::appendGpuPointwiseNodes()`、`RenderPipeline::applySpatialEffect()`。
- **確認できた事実・変更:** 調整レイヤーのpointwise計画でeffect所有の既存契約を利用し、Channel Mixerの通常行列処理を追加した。CPU実装にあるRGBの0..1 clampをGPUノードにも追加した。Normal blend、非MSAA、CPU指定なし、effect個別の範囲・マスク・Mixなしに限定する。調整レイヤー自体のマスク・opacityは既存pointwise経路で扱う。調整マスクのcache hitでは読み取り専用uploadのための全画像copyを省いた。
- **Blurの限定範囲:** 高品質・等倍pipeline、非MSAA、変換なし、レイヤーマスクなし、opacity 1、Normal blendの全Blur stackのみ既存resident shaderを使う。effectの既存premultiplied/strength/幅/容量契約に加え、CPUのkernel幅とGPUのceil(3*sigma)が一致することを各effect内の全passで確認する。GPU内の専用snapshotで背景を保存し、途中失敗では復元して従来経路へ進む。混在stack・overscan・時間参照は対応を広げない。
- **懸念・未検証:** SharpenはCPUとresident shaderでstraight/premultiplied alphaやthresholdの扱いが異なるため追加対象から除外した。Blurの数値誤差、色変換境界、半透明・HDRの見た目、D3D12/Vulkan resource stateと性能は未検証。既存shader/PSOの初回遅延初期化は残るが、毎フレームの新規画像確保やreadbackは追加しない。GPU snapshot分のcopyは成功時にも必要であり、ゼロcopyではない。
- **次に確認すべきこと:** ユーザーが確認を再開した際に対象設定と対象外設定、GPU途中失敗、mask/opacity、半透明・HDR、backend別の描画を比較し、readback・待ち時間とGPU snapshot費用を計測する。今回は依頼によりビルド・テスト・実機計測をスキップした。



## 2026-10-05 — レンダリング性能修正の所有権と安全な範囲

- **関連:** `ArtifactEffectFrameSampler`、`buildRasterizedSurfaceBuffer()`、`ArtifactAbstractEffect::applyConfigured()`、`RenderPipeline::applySpatialEffect()` / `applyPointwise()`。
- **確認できた事実・変更:** CPU履歴はnamed inputにも使われるため保存対象は減らさず、出力を独立コピーした後に完成CPU画像を履歴へ移譲する経路を追加した。既存のコピー保存API、サンプル取得時の独立コピー、保持数・容量・revision・seekの無効化は維持する。既存の履歴mapは保存値だけをCPU画像へ変更し、GPU資源を保持しない。履歴無効化の一時vector、pointwiseのsegment複製vectorを撤去した。HexGrid / Voronoi / Bricks / StripesとGenericのGenerator契約は入力を読まないので、最終UAVへ直接出力する。FilterやBlurの中間バッファは維持する。CPUマスク/Mixは入力・出力のメモリ領域が重ならない場合だけsource cloneを省き、重なる場合はsnapshotを残す。
- **例外的に残る確保:** CPU履歴のmap/listノードと、`ImageF32x4_RGBA`既存move実装が移譲元に作る小さな1x1初期画像は残る。画像サイズに比例する履歴保存cloneは撤去したが、ゼロアロケーション化を達成したとは扱わない。固定pool化は既存の保持・named input・eviction契約を確認してから別途検討する。
- **2026-10-05 続行分:** layer pointwiseのsegmentは同形式・同寸法の既存output/scratchを交互に使い、最終結果がscratchに残った場合だけ固定outputへコピーする。追加マスクのRGBA32F作業領域をpipeline所有の独自`Array<float>`で再利用し、初回cache構築時のみ確保、resize/destructionで解放する。保持するCPU容量は現在のpipeline寸法に制限される（RGBA32F相当の作業領域を保持するメモリ増は未計測）。参照可能なsurface/static cacheがない場合はlookupのsignature構築を省略する。画像のsource/crop更新はcallerの既存経路で維持する。ロックの除去、非対応effectの省略、GPU contextの並列操作は行っていない。
- **未検証・残る候補:** ビルド、テスト、実機計測は未実施。GPU非対応条件の拡張、調整レイヤーのreadback撤去、フレームロック分離、キャッシュキー契約変更には画質・マスク・履歴・context所有権の受入れ確認が必要。既存ロックを単純に外す根拠はない。
- **次に確認すべきこと:** 時間参照とnamed input、seek/逆再生、容量超過のeviction、マスク/Mixの別バッファ・同一バッファ、生成系の後のFilter/Blur/マスク、D3D12/Vulkanのresource stateを実機確認し、CPU/GPU/Present時間、コピー量、アロケーションを比較する。



## 2026-10-05 — Effect profiling 設定参照のホットパス候補

- **関連:** `Artifact/src/Effects/ArtifactAbstractEffect.cppm` の `effectProfilingEnabled()` / `applyConfigured()`。
- **確認できた事実:** effect適用ごとに `QSettings` を構築し、`Diagnostics/EffectProfiling` のcontains/valueを参照する。設定がなければ環境変数を読み、`std::string` も構築する。profiling無効時にも判定処理は毎段実行される。
- **懸念・仮説（未検証）:** 設定アクセスや内部同期が軽いeffectを多数重ねた場合の余分なCPUコストになり得る。今回の修正には含めていない。動的に切り替えるユーザー設定か起動時の開発フラグかを確認せず、単純なstatic cacheへ変更してはならない。
- **次に確認すべきこと:** 設定変更の正規の導線と通知、`STARTUP_FLAGS_CONTRACT_2026-09-29.md` との責務を確認し、無効時の設定参照コストを計測する。
- **2026-10-05 追加確認・最小修正:** `ArtifactPerformanceProfilerWidget::mousePressEvent()` が実行中にこの設定を切り替える。固定化はせず、環境変数比較の一時 `std::string` と無効時の時計取得だけを除去した。設定参照の集約には、この動的操作を保つ更新経路が必要（未実装・未計測）。



## 2026-10-05 — Effect CPU capability と adapter 所有の区別

- **関連:** `Artifact/include/Effects/ArtifactAbstractEffect.ixx`、`Artifact/src/Effects/ArtifactAbstractEffect.cppm`、`Artifact/src/Effect/ArtifactCreativeEffects.cppm`、CPU画像を消費する Controller / Composition View / Preview。
- **確認できた事実・今回の対応:** Glitch / Halftone / Old TV は `apply()` 内にCPU参照を持つが `cpuImpl_` を登録しない。adapter pointer の有無だけでCPU能力を判定すると、GPU常駐計画からのCPUフォールバックでもGPU upload・global cache mutex・同期待ち・readbackを実行していた。`supportsCPU()` と共通の `applyToCpuSurface()` を追加し、この3種のCPU能力を明示した。
- **再利用できる知見・懸念:** 参照実装の能力と adapter の実装形式は別の契約として扱う。今後 `apply()` をoverrideしてCPU/GPUを両方実装するeffectはCPU能力も明示する。旧式GPUヘルパーのmutexはslotの再利用と共有context操作を囲むため、ロック範囲だけの短縮を並列化の根拠にできない。速度改善幅とCPU/GPU出力一致は未検証。
- **次に確認すべきこと:** ビルド許可後に混在stackでGPU常駐→CPU参照の境界、modeの復元、入力／出力alias、mask/mix、αとchannel orderを確認し、GPU submission / readback / copyの時間を分けて計測する。



## 2026-10-05 — Checked float color views: first effect-boundary migration

- **関連:** `ArtifactCore/include/Image/ImageSurfaceView.ixx`; CPU kernels in ColorWheels, LiftGammaGain, Curves, ColorBalance and WhiteBalance.
- **対応・確認できた事実:** Added distinct read-only/mutable RGBA32F and BGRA32F view types with private construction and checked optional-returning factories. Factories validate declared order, float32 storage/precision, pointer/stride alignment, minimum row bytes and arithmetic bounds; mutable creation also requires the same pointer as the described surface. Views expose logical r/g/b/a references, support row strides and have no implicit cross-order or const-to-mutable conversion. Static assertions encode these type restrictions. The five CPU effect paths dispatch once through a checked mutable view, then use logical channel references instead of numeric red/blue indices. Unknown or invalid layouts leave the already-copied source unchanged.
- **性能・互換性:** Views, optional results and channel references are non-owning inline values; this change adds no pixel copy, conversion or heap allocation. Existing effect arithmetic, transfer functions, alpha association and image ownership are preserved. Only the existing surface-view interface and five implementation units changed; no new module/CMake registration was needed.
- **限界・未検証:** This is an incremental boundary migration, not a full removal of raw-pointer APIs. Existing metadata setters and legacy GPU adapters remain; factories validate declared metadata but cannot infer actual channel semantics or allocation extent from foreign raw bytes. Owners must preserve buffer lifetime/layout while a view exists. Build/runtime verification was not authorized and was not run; source review and diff whitespace checks only.
- **次に確認すべきこと:** After authorization, compile Core and migrated effects, then compare RGBA/BGRA and translucent images at matching settings. Later migrate remaining raw CPU/GPU boundaries and separately audit metadata-only setters before restricting their behavior; do not combine this with working-space conversion.



## 2026-10-05 — Proposal: checked typed views for channel-order boundaries

- **関連:** `ArtifactCore/include/Image/ImageF32x4_RGBA.ixx`, `ArtifactCore/include/Image/ImageSurfaceView.ixx`, `ArtifactCore/include/Graphics/SurfaceColorContract.ixx`, effect CPU/GPU boundaries.
- **確認できた事実:** ImageF32x4_RGBA exposes raw float pointers in backing-memory order, explicitly permitting BGRA despite its name. ImageSurfaceView accepts void pointers with independently supplied metadata; isValid checks dimensions/pointer/stride but not channel order. setColorDescriptor can relabel memory without converting pixels. Existing canonical descriptor helpers express the intended working representation but do not enforce it in function parameter types.
- **提案（未実装・未検証）:** Introduce lightweight distinct checked RGBA/BGRA views with private construction, no implicit cross-order conversion, and validated factories for runtime-described images. Effect kernels accept the required view rather than raw float pointers. Explicit conversion writes into caller-owned/reused storage at import/upload boundaries; metadata relabeling is restricted so changing order requires physical conversion. A separate checked linear-premultiplied working view can enforce the renderer contract without treating channel order, transfer and alpha as interchangeable.
- **価値・次の確認:** Compile-time rejection protects typed consumers, while runtime checks remain necessary at external raw-buffer boundaries; actual pixel semantics cannot be inferred from unlabeled bytes. Plan an incremental effect-boundary migration before changing Core public APIs; audit callers relying on metadata setters and ensure no per-effect/frame normalization allocation. User discussion requested feasibility, so no implementation or build was performed.



## 2026-10-05 — Color Balance and White Balance: channel, alpha and GPU parameter contracts

- **関連:** `Artifact/src/Effects/ColorCorrection/ColorBalanceEffect.cppm`, its interface, `Artifact/src/Effects/WhiteBalanceEffect.cppm`, `ArtifactCore/src/ImageProcessing/ColorTransform/ColorBalance.cppm` (read-only reference).
- **確認できた事実:** ColorBalance's legacy C++ constant buffer placed midtoneR where HLSL expected shadowRange and was 52 bytes against a 64-byte shader layout. Its shader also used different luma weights, tonal transition ranges and additive luminance preservation from the Core CPU processor. WhiteBalance CPU hard-coded BGRA, its legacy shader assumed RGBA, and its GPU implementation's CPU fallback retained default values rather than the edited settings.
- **対応:** ColorBalance now contributes a resident generic node; both GPU variants use the Core processor's luma/weight/preservation equations. Its compatibility constant buffer now has four aligned 16-byte groups. Both effects use descriptor-selected RGB indices, grade straight color then restore premultiplied alpha, and preserve neutral pixels exactly. Legacy shaders retain raw source channel order with an explicit BGRA flag, avoiding an additional full-image normalization allocation. WhiteBalance forwards current values to its CPU fallback. Cached GPU resources retain one existing SharedRenderDeviceLease for their lifetime rather than accumulating a shared-device acquire each call.
- **確保・範囲:** ColorBalance's shared shader math is joined into two bounded source strings once at module/effect initialization; edits and resident dispatch never rebuild those strings. This small cold-path allocation keeps the two shader equations identical without duplicating their implementation. No new collection, QImage path, signal/slot, CMake or Core/Diligent implementation change was introduced. The existing image-returning GPU APIs still allocate/transfer full images and read back synchronously; those compatibility costs were not presented as removed.
- **影響・未検証:** ColorBalance GPU output intentionally changes to match the CPU contract; its Cinematic default is preserved. Build, D3D12/Vulkan runtime parity and measured edit latency remain unverified. Next authorized verification should cover Neutral/Cinematic, strength 0/0.5/1, Preserve Luma on/off, warm/cool WhiteBalance, RGBA/BGRA and translucent inputs, plus GPU-failure CPU fallback.



## 2026-10-05 — Lift/Gamma/Gain and Curves: resident processing and shared LUT

- **関連:** `Artifact/src/Effects/LiftGammaGainEffect.cppm`, `Artifact/src/Effects/ColorCorrection/CurvesEffect.cppm`, their interfaces, `Artifact/include/Effects/ArtifactAbstractEffect.ixx`, `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`.
- **対応・確認できた事実:** Both effects now publish resident generic GPU nodes. Their image-returning GPU adapters physically restore BGRA channels before source metadata is restored, and CPU/GPU paths unpremultiply before grading and premultiply afterward. Neutral LGG and Identity Curves copy the input exactly. Curves prepares a combined 256-entry master/channel LUT when parameters change; CPU rows no longer copy curves or rebuild LUTs. CPU and both GPU paths use the same linear LUT interpolation (CPU previously used Core's nearest-entry lookup).
- **確保の例外:** A changed Curves LUT allocates one immutable approximately 4 KiB snapshot, with shared ownership so an existing render plan retains its table during edits. This occurs in parameter synchronization, not per image row or render dispatch; unchanged LUT contents retain their snapshot. The resident renderer reuses one LUT texture per shader key and uploads 4 KiB when the snapshot changes. Multiple distinct Curves nodes can cause successive small uploads, but do not allocate one texture per edit. Existing custom-point std::vector storage is retained for compatibility with ColorCurves; no new standard collection was introduced.
- **影響・未検証:** Native nodes use the existing Diligent compute path and remove full-image readback from eligible resident stacks; legacy image-returning APIs still require readback. CPU Curves interpolation changes slightly to match the existing GPU implementation. Build, D3D12/Vulkan runtime parity, parameter-drag latency and allocations have not been measured. ColorBalance and WhiteBalance remain separate audit candidates.
- **次に確認すべきこと:** When build/runtime verification is authorized, compare RGBA/BGRA and translucent swatches across all paths; verify Curves Identity/SCurve/Custom/Posterize, multiple Curves nodes, masks/mix and LGG default/adjusted values. Profile identical-resolution edits before claiming a measured speedup.



## 2026-10-05 — Adjacent effect audit: grading channel order and CPU fallback costs

- **関連:** `Artifact/src/Effects/LiftGammaGainEffect.cppm:128`, `Artifact/src/Effects/ColorCorrection/CurvesEffect.cppm:51,168,432`, `Artifact/src/Effects/ColorCorrection/ColorBalanceEffect.cppm:43,270`, `Artifact/src/Effects/WhiteBalanceEffect.cppm:116`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:3955,4374`。
- **確認できた事実:** LiftGammaGain and Curves publish canonical RGBA readback while `ArtifactAbstractEffect::applyConfigured` restores a fully-specified source descriptor. On a BGRA source this relabels the pixels without physically swapping channels. Their interfaces, and ColorBalance's, declare GPU support but no resident GPU domain. The controller rejects unsupported resident stacks and its CPU surface path calls `applyCPUOnly` when a CPU implementation exists; their legacy GPU image APIs separately retain blocking readbacks. Curves additionally copies four vector-owned curves for every image row; the CPU preset setup leaves its LUT invalid, and `ColorCurves::process` builds the LUT on each row-local copy. ColorBalance feeds raw channel 0 as red without consulting channel order. WhiteBalance CPU always treats channel 2 as red, whereas its resident GPU shader treats channel 0 as red.
- **影響の解釈（実機未検証）:** BGRA inputs can exhibit the same red/blue display reversal in LiftGammaGain/Curves image-returning GPU paths. ColorBalance and WhiteBalance adjustments can address the wrong red/blue channel depending on source storage. Missing resident support and Curves row-local copies/repeated LUT construction are concrete candidates for parameter-edit stalls; no measured latency is claimed. A WaitForIdle occurrence alone is not proof that the ordinary viewport invokes it.
- **優先度・範囲:** LiftGammaGain and Curves first (high confidence from source, medium scope for resident integration); then ColorBalance (medium scope) and WhiteBalance CPU channel dispatch (small scope). Existing Curves SCurve and ColorBalance Cinematic defaults are deliberate preset selections, not classified as bugs in this audit.
- **次に確認すべきこと:** After user authorization to implement/verify, compare BGRA/RGBA orange swatches across CPU/image-returning GPU/resident GPU, and inspect a representative parameter drag with the same backend, resolution and effect stack. No effect implementation, build, test or runtime was changed/executed for this audit; this note is recorded under AGENTS.md's Insight requirement.



## 2026-10-05 — Color Wheels: resident grading and source channel-order restoration

- **関連:** `Artifact/src/Effects/ColorCorrection/ColorWheelsEffect.cppm`, corresponding `.ixx`, `Artifact/src/Effects/ArtifactAbstractEffect.cppm`, `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`。
- **確認できた事実:** Color Wheels did not contribute resident GPU nodes. Its image API allocated an upload buffer/input texture and staging texture, then called `Flush()` / `WaitForIdle()` and cloned readback pixels. Its canonical RGBA output was subsequently relabelled with the fully-specified source descriptor in `applyConfigured`; a BGRA source therefore displayed red and blue exchanged. UI Gain Master defaults to 1, while Core interprets it as an additive luminance term with neutral 0. CPU used Lift → Gamma → Gain; the legacy GPU used Lift → Gain → Gamma.
- **対応:** Registered one resident generic shader and fixed-size parameter node for all wheel models. Expanded the existing generic constant buffer from 8 to all 16 slots already present in the node, with matching HLSL layout and 96-byte alignment. No new per-frame collections or resources are added to this resident path. Grading decodes/re-encodes premultiplied alpha, preserves default pixels exactly, maps UI Gain Master to the Core neutral convention, and uses the same operation order. The CPU image API restores the actual source channel order before the host restores metadata; the legacy image-returning GPU adapter still performs its explicit synchronous readback when that compatibility path is needed.
- **価値または懸念（未検証）:** The resident path removes the per-edit GPU→CPU→GPU round trip for GPU-eligible layer stacks through the shared Diligent backend, without reducing resolution or disabling effects. The generic constant-buffer extension also affects existing generic shaders; their shared prelude and C++ layout were updated together. Core/DiligentEngine submodules were not modified. Existing non-neutral Gain Master values now follow the advertised neutral-1 contract.
- **次に確認すべきこと:** Build and runtime profiling remain unperformed under the user's no-build/test rule. Rebuild, then confirm orange solids retain their color when the effect is added; drag Lift/Gamma/Gain in all modes; check semi-transparent BGRA/RGBA inputs, masks/mix, and an existing generic effect beside Color Wheels. Compare matched viewport latency on D3D12 and Vulkan before claiming a measured speedup.



## 2026-10-05 — TextGizmo の press / move 所有者不一致を調査

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`docs/bugs/TEXT_GIZMO_INPUT_OWNERSHIP_INVESTIGATION_2026-10-05.md`。2026-09-12 の未バインド対策、2026-10-03 の Text ツール押下対策の後にも残る別経路。
- **確認できた事実:** 投影フレーム press（29224）の条件だけテキスト除外がなく、orientation matrices が valid ならテキストで 3D beginDrag が成立し得る。一方 3D move（31822）、hover（39260）、draw（50446）はテキスト除外済み。TextGizmo press より先に return すると両ギズモとも変換を更新しない。
- **価値または懸念:** 同じ操作所有者の判定を draw / hover / press / move に分散させると、1か所のガード漏れで「表示されるが操作不能」になる。将来の再利用候補は所有者判定の共通化だが、この調査で構造変更は行っていない。
- **未検証:** 実行中バイナリとの一致、実機の具体的 hit、混在複数選択の期待動作。ビルド・テスト・実機確認は未実施。
- **次に確認すべきこと:** 単独テキストを Selection と Text ツールで比較し、press 後の textGizmo / gizmo3D の isDragging を確認。最小修正は投影フレーム press のテキスト除外を揃えること。
- **2026-10-05 実装:** controller 内の所有者 resolver と操作 session に、draw / hover / press / move / release / cancel を統合。Text / Content の重複ドラッグフラグを撤去し、既存3D snapshot / Undo を再利用。選択・frame・tool・lock・対象削除で取消、終了時は再入を抑止して viewport の mouse capture を解放する。Text の4種 Position 更新を共通化し、静的 anchor の不要なキー生成も避けた。
- **追加確認できた事実:** `ArtifactAbstractLayer::getProperty` は transform path で `transform3D().channelProperty` を返し、Core の `TransformPropertyChannel` が絶対 property と相対 track 値を変換する。transform と cached property が別所有という旧コメントは現行コードに合わないため更新した。Core は読み取りのみ。
- **検証残:** 差分・API・開始終了経路の静的確認のみ。ビルド・テスト・実機は未実施。初回 box property 準備は layer binding の cold 境界に限定。混在 selection の Text primary は既存単体編集を維持し、group 対応追加はしていない。



## 2026-10-05 — 開発ブランチ取り込み時の画像コピー契約確認

- **関連:** `origin/codex/2026-10-02-dev`、`ArtifactCore/src/Image/ImageF32x4_RGBA.cppm`、`Artifact/src/Effect/ArtifactCreativeEffects.cppm`。
- **確認できた事実:** 通常の画像コピーも `Impl` の copy constructor で `cv::Mat::clone()` を呼ぶ。Glitch / Halftone / Old TV の変更は source snapshot と `SetCpuImage` の2回の画像複製であり、元の4回（source snapshot、出力 DeepCopy、一時 wrapper 構築、wrapper 代入）より2回減る。shallow snapshot / 1回というコメントと文書を訂正した。重複領域に対する `sourceSnapshot.emplace(sourceImage)` も独立コピーになるため、追加の DeepCopy は不要。
- **価値・懸念:** `auto` や move の表記だけでコピー費用・alias の安全性を判断せず、所有型の実装まで確認する必要がある。既存画像 move の1x1再初期化確保も残る。
- **次に確認すべきこと（未検証）:** 許可後のビルド、CPU/GPU parity、履歴保持、ソフトキーのUndo/保存、追加マスク初回確保の計測。取り込みの個別判断は `docs/analysis/REMOTE_BRANCH_INTEGRATION_REVIEW_2026-10-05.md` に記録。



## 2026-10-05 — オフライン GPU readback は同期版固定で、非同期 3-slot ring が未接続

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm`（`readbackToImageAsync` `:2679-2940`、リング `:584-589`）、`Artifact/src/Render/ArtifactRenderQueueService.cppm` `:6967/6990/7002/7019`（同期版直呼び）。`temp/offline_render_performance_2026-10-05.md` の第1便。
- **確認できた事実:** オフライン GPU フレームは同期 readback（CopyTexture→EnqueueSignal→Flush→`fence->Wait`、`:2239-2242` / `:2452`）で毎フレーム最低 1 回の CPU ハードストールが入る。3 スロットの非同期リングと QtConcurrent 消化は実装済みだが、呼び出し元はライブビュー系のみ。単一 GPU オフラインは `numWorkers = 1` の意図的直列化（`ArtifactRenderQueueService.cppm:7142-7147`）。
- **価値または懸念（未検証）:** 非同期版への差し替えで fence 待ちをフレーム並列の背後に隠せ、HOT_PATH_RULES の staging ring 契約と整合。readback 完了順の前倒れに世代照合が必要。Deep AOV は最大 9＋1 回の同期 readback 反復で、非同期化以上にバッチ化の調査が望ましい。
- **次に確認すべきこと:** AOV 有効時のフレーム内 readback 回数と `flushCount_ / flushContextTimeUs_` によるストール実割合の確認。



## 2026-10-05 — GPUTextureCacheManager の upload gate がオフライン経路で beginFrame 未呼び出しの疑い

- **関連:** `Artifact/src/Render/GPUTextureCacheManager.cppm:243-247（beginFrame で解除）, 811-816（gate）, 905-934（stats）`、フォールバック経路 `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:1743-1773`。
- **確認できた事実（静的）:** `processPendingUploadsLocked` は one-shot フラグで早退し、フラグを解除する `beginFrame()` の呼び出し元はライブビューのみ。オフライン Render Queue は呼ばないため、ジョブ初回の 1 回しか upload が捌かれず、以降は AOV binding invalid のまま直接スプライト upload フォールバックに偏向する可能性がある。
- **価値または懸念（未確認）:** 事実なら、オフライン GPU のテクスチャ更新が毎フレーム「即時生成＋直接 upload」に落ちて GPU パスの性能を食んでいる。既存 stats（hit/miss/pendingUploadCount）で静的に判別できる「半分は実装済み」型の課題。
- **次に確認すべきこと:** stats の実測と、必要ならオフライン側への upload 消化ループの接続。



## 2026-10-05 — 静的レイヤー GPU キャッシュが process-global QHash でロック無し

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:312-316, 1688-1700`。
- **確認できた事実（静的）:** `drawLayerForCompositionView` が使う `staticLayerGpuCache()` はロック無しの `static QHash`。マルチ GPU ワーカーは `renderSingleFrameNoLock` をロック通過せずに直呼びするため、発動条件（`usesStaticGpuCache`）が満たされると並行アクセスになり得る。
- **価値または懸念（未確認）:** データ競合の正確性リスク。GPU 経路の並列化（マルチ GPU / 後続の MFR 拡張）を広げる前の前提条件。
- **次に確認すべきこと:** 発動条件の特定と、renderer-per-worker への局所化要否の判断。
- **第2修正パス追記（2026-10-07）:** 事実を追加特定した。(1) `GPUTextureCacheHandle` は `{quint64 id, quint64 generation}` の整数対でマネージャ紐付けを持たない（`Artifact/include/Render/GPUTextureCacheManager.ixx:53-58`）ため、別マネージャの stale handle が同マネージャ内の同一 (id, generation) エントリと衝突し得る。(2) `clearStaticLayerGpuCache()` 両オーバーロードの呼び出し元がゼロ（Artifact 配下全域確認）で、静的キャッシュは stale handle を正規に flush されない。(3) `layerUsesStaticLayerGpuCacheForCompositionView` は連番レイヤーだけ除外する形（`:1295-1307`）で、オフライン GPU ジョブでも静的レイヤーで発動する。(4) オフライン実行中もライブビューは停止しない（QtConcurrent バックグラウンド描画）ため、競合はマルチ GPU 専用ではなく mainline で発生し得る。**同セッションで実装済み:** 静的エントリから `gpuTextureHandle` を撤去して「毎フレーム現行マネージャで acquireOrCreate し直す」方式に一本化（manager 内 LRU texture cache がコストを吸収）し、操作単位のグローバル `staticLayerGpuCacheMutex()` と hit 時の内容 copy-out（entry ポインタをロック外に保持しない）を導入。当初想定した per-manager handle map は handle を共有しない設計により不要になった。未確認: `acquireOrCreate` のヒット時 lastUsedFrame 更新と expiration との整合。`temp/offline_render_performance_2026-10-05.md` §8。



## 2026-10-05 — `renderSingleFrameGPU` と FFmpeg 遺物メンバーがデッドコード（第1修正パスで frameBuffer は撤去済み）

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm:4421, 2410-2427, 2483`。
- **確認できた事実（静的）:** 呼び出し元がファイル内に見つからず、Impl の private 成員のため外部接続もない（`frameBuffer` は FFmpeg エンコーダ無効化に伴い実質未使用）。
- **価値または懸念:** 保守負担のみ。本レポートの改善候補とは独立。プリプロ条件込みで最終確認して削除または接続の判断材料にする。
- **次に確認すべきこと:** 条件コンパイルの有無の最終確認。


## 2026-10-09 — HLG分岐境界とPQ referenceの独立テスト

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、standalone target `ArtifactCoreColorSpaceTest`。
- **確認できた事実:** HLG encode のscene-linear `1/12`とその隣接float、decode のencoded `0.5`と直上値の式を個別に検査し、PQ 18%入力の出力が約`0.81594`となる既知referenceも検査した。Visual Studio 2026 / MSVC 19.51で個別targetをビルドし、色standalone CTest全10 suiteが成功。
- **価値:** 広い格子による単調性・往復検査に加えて、piecewise関数の実際の分岐条件と規格referenceを個別ケースで保護できる。
- **次に確認すべきこと（未検証）:** 次の独立suite候補として、`ColorSpaceConverter::getWhitePointX/Y`と`getGammaExponent`の全ColorSpace対応値を表駆動で固定する。


## 2026-10-09 — ColorSpace metadataの全enum対応値を固定

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ColorSpaceConverter::getWhitePointX/Y`・`getGammaExponent`。
- **確認できた事実:** Linear / sRGB / Rec.709 / Rec.2020 / P3 / ACES AP0 / AP1の7値について白色点X/Yと既定gamma exponentを表駆動で検査した。Visual Studio 2026 / MSVC 19.51で個別targetをビルドし、色standalone CTest全10 suiteが成功。
- **価値:** metadata switchのcase誤記や意図しない値変更を、広い変換往復テストとは独立に検出できる。
- **次に確認すべきこと（未検証）:** ColorSpace enumに未定義値を渡したときの fallback値を契約として固定する必要があるか、API利用側の前提を調べる。


## 2026-10-09 — 未定義ColorSpace metadata fallbackを検査

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ColorSpaceConverter::getWhitePointX/Y`・`getGammaExponent`。
- **確認できた事実:** enum外の値`static_cast<ColorSpace>(-1)`に対し、現行実装は白色点`(0.3127, 0.3290)`とgamma exponent `2.2`を返す。個別ColorSpace targetとCTest全10 suiteがVisual Studio 2026 / MSVC 19.51で成功した。
- **価値・懸念:** 現行の防御的fallbackを回帰から守る一方、これは明示ドキュメント化された公開契約ではなく実装挙動の固定である。将来enum検証を導入する場合はエラー返却などを含めて再設計が必要。
- **次に確認すべきこと:** metadata APIのfallback契約を公開仕様にする必要性を別途判断する。


## 2026-10-09 — signed sRGBと全transferの非有限値入力

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ColorSpaceConverter::applyGamma/removeGamma`。
- **確認できた事実:** sRGBは負のscene-linear値をlinear toe slopeで符号付きencodeし、decodeで復元する。Linear / sRGB / Gamma22 / Gamma24 / Gamma26 / PQ / HLGすべてのencode/decodeへNaN・正無限大を入れたときの結果を検査した。PQ encodeだけはsanitize後の0を式へ通すため約`7.31e-7`を返す現行挙動を式から確認し、テストで固定した。ColorSpace targetと色standalone CTest全10 suiteが成功。
- **価値・懸念:** Transferごとのsanitize差を可視化できた。PQの非有限入力に対する微小正値は「ゼロへsanitize」という一般的な説明とは異なるが、ここでは実装挙動を記録しただけで仕様承認を意味しない。
- **次に確認すべきこと:** PQの非有限値出力を公開契約として許容するか、別の仕様変更として判断する。


## 2026-10-09 — ACES display fitted curve referenceと露出制御

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ColorSpaceConverter::applyACESDisplayTransform`。
- **確認できた事実:** scene-linear 0.18 / 1 / 4 のfitted curve出力を約0.10559125 / 0.61911543 / 0.90901377で固定。+1 stopが入力RGBの2倍と等価、非有限露出は0 stop扱い、露出100は+16上限と同値であることを検査した。ColorSpace targetと色standalone CTest全10 suiteがVisual Studio 2026 / MSVC 19.51で成功。
- **価値・懸念:** 既存の有限性・単調性から一歩進み、近似式と露出引数の数値契約を回帰検出できる。基準点はArtifact内のfitted近似に対する値で、Academy reference transformとの同一性は主張しない。
- **次に確認すべきこと（未検証）:** 各RGB channelへ異なる入力を与えたときの独立channel処理と、負値・非有限pixelのchannelごとの隔離動作を追加確認する。


## 2026-10-09 — ACES fitted transformのchannel独立性

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ColorSpaceConverter::applyACESDisplayTransform`。
- **確認できた事実:** 異なるRGB値の出力を個別reference値で検査し、R/G/Bの各位置にNaN・負値・+infinityを一つずつ与えた場合、当該channelのみ0になり残るchannelは正常入力時と同値であることを確認した。個別targetと色standalone CTest全10 suiteが成功。
- **価値:** channel indexの取り違えや、1 channelの破損がpixel全体へ伝播する回帰を防ぐ。
- **次に確認すべきこと（未検証）:** exposureの下限-16 clampと、十分大きな入力の上限clampを独立に試す。


## 2026-10-09 — ACES露出下限と暗部toe clamp

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ColorSpaceConverter::applyACESDisplayTransform`。
- **確認できた事実:** exposure `-100`と下限`-16`が同一出力となり、fitted curveの暗部にある負のmapped値（入力0 / 1e-5 / 1e-4）が表示用出力では0 clampされることを確認した。ColorSpace個別targetと全10 color standalone suitesが成功。
- **価値:** 露出の両端clampと暗部の最終display clampを別々に保護する。
- **次に確認すべきこと（未検証）:** 露出の下限を通る正の信号で、異なるRGB channelが下限後に再現性ある値へ写ること。


## 2026-10-09 — ACES露出floorでの正信号保持

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ColorSpaceConverter::applyACESDisplayTransform`。
- **確認できた事実:** exposure floor `-16`でRGB `{300, 500, 1000}`は約`{0.00017881, 0.00064277, 0.00211229}`へ写る。exposure `-100`は`-16`と同じ結果になり、明るさの順も保たれる。個別targetと全10色standalone suiteが成功。
- **価値:** floor-clamp確認を、すべて0になる暗い入力だけでなく、正の信号を保持する入力でも検証できる。
- **次に確認すべきこと（未検証）:** 高露出側でも異なるHDR入力の順序・非飽和値が保たれることを検査する。


## 2026-10-09 — ACES high exposure HDR orderとhighlight saturation

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ColorSpaceConverter::applyACESDisplayTransform`。
- **確認できた事実:** exposure +8でscene-linear `{0.01, 0.1, 1}`は約`{0.8489785, 0.9999556, 1}`へ写り、channel順と[0,1]範囲を保つ。+16では全channelが1へ飽和する。個別targetと全10色standalone suiteが成功。
- **価値:** ハイライトが完全飽和するまでのHDR範囲と、露出上限での最終clampを明示的に保護する。
- **次に確認すべきこと:** なし（この項目の期待値検査で完了）。


## 2026-10-09 — 未定義GammaFunctionのsanitize済みidentity fallback

- **関連:** `tests/ArtifactCore/ColorSpaceTest.cpp`、`ColorSpaceConverter::applyGamma/removeGamma`。
- **確認できた事実:** enum外の`GammaFunction`値では有限の負値・0・通常値・1超過値をencode/decodeともそのまま返し、NaNおよび+infinityは事前sanitizeにより0を返す。個別ColorSpace suiteと色standalone全10 suiteが成功。
- **価値・懸念:** switch defaultのidentity fallbackとsafe input処理の組み合わせを保護する。これも明示的なAPI仕様というより現在の防御的実装を記録するテスト。
- **次に確認すべきこと:** なし。


## 2026-10-09 — PQとHLG transferの絶対reference値

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ColorTransferFunction::linearToPQ/pqToLinear/linearToHLG/hlgToLinear`。
- **確認できた事実:** PQ encodeのlinear 0.01 / 0.18 / 1.0、decode code 0.5、HLG encode linear 0.5 / 1.0、decode code 0.5 / 1.0のreference値を追加し、色standalone CTest全10 suiteで成功した。初回のPQ decode期待値は約2.01e-7ずれて失敗したため、float実装での値`0.00922437`に補正して再確認。
- **価値:** 往復一致だけでは検出できない、encode/decode双方が同方向にずれた回帰をabsolute pointで検出できる。
- **次に確認すべきこと:** なし。


## 2026-10-09 — PQ / HLG black-white endpoints

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ColorTransferFunction` PQ / HLG encode・decode。
- **確認できた事実:** PQ / HLG双方でlinear/codeの-1と0がencode/decode後に0となり、linear/code 1は双方向とも1になるendpoint契約を確認した。個別Bridge targetと色standalone全10 suiteが成功。
- **価値:** 往復格子・中間referenceとは別にblack clampとwhite normalizationの退行を直接検出する。
- **次に確認すべきこと:** なし。


## 2026-10-09 — TransferFunction dispatchの未知値fallback

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ColorTransferFunction::encode/decode`。
- **確認できた事実:** enum外の`TransferFunction`値をdispatchしたとき、有限の負値・0・通常値・1超過値はencode/decode双方でidentity passthroughされる。個別Bridge targetと色standalone全10 suiteが成功。
- **価値・懸念:** 汎用switch defaultのfallbackを保護する。未知値のidentity動作は現行実装の挙動であり、公開仕様として固定済みとは限らない。
- **次に確認すべきこと:** なし。


## 2026-10-09 — DaVinci Intermediateのzero-toe順序についての仮説

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx` の`linearToDaVinciIntermediate`、`tests/ArtifactCore/ColorBridgeTest.cpp`。
- **確認できた事実:** 実装はlinear `<=0`を0へ写し、正値では`(log2(linear)+12.473931188)/12.900429241`を使う。テスト探索で0の出力0に対してlinear `0.0001`の出力が約`-0.06308`となることを観測したため、この二点間は単調減少する。現在の一般的transfer suiteは0.001以上から代表値を試しており、このtoeの形状を検査していなかった。
- **未検証の仮説:** DaVinci Intermediate仕様にあるlinear toeが省かれている可能性。基準仕様との照合は未実施で、今回のユーザー依頼はテスト追加のため、production codeは変更していない。
- **価値または懸念:** 低輝度入力がblackより負のencoded valueへ写る可能性がある。
- **次に確認すべきこと:** `ColorTransferFunction::linearToDaVinciIntermediate` が意図する入力domainと仕様上のtoe / minimum codeを確認し、修正はArtifactCore child repositoryへの変更が明示依頼された場合に別作業で行う。


## 2026-10-09 — log / camera transfer HDR headroom round trip

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、ACEScct / DaVinci Intermediate / S-Log3 / Cineon / Canon Log 2/3 dispatch。
- **確認できた事実:** 各曲線でscene-linear 1 / 2 / 4 / 16をencode/decodeし、finiteかつ元値との差2e-3以内を確認した。個別Bridge suiteと全10色standalone suiteが成功。
- **価値:** normalized whiteを超えるHDR headroomの往復誤差を曲線ごとに保護できる。
- **次に確認すべきこと:** なし。


## 2026-10-09 — log / camera curve absolute encoded references

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、ACEScct / DaVinci Intermediate / S-Log3 / Cineon / Canon Log 2/3。
- **確認できた事実:** 6曲線それぞれのlinear 0.18 / 1 / 4 encode値を独立reference表で検査し、Bridge suiteと色standalone全10 suiteが成功した。
- **価値:** encode/decode往復が両側で同じ誤りを持つ場合もabsolute code pointの差として検出できる。
- **次に確認すべきこと:** なし。


## 2026-10-09 — near-achromatic HSV thresholdとHSL差

- **関連:** `tests/ArtifactCore/ColorConversionTest.cpp`、`ColorConversion::RGBToHSV/RGBToHSL`。
- **確認できた事実:** RGB delta `4e-6`の青寄り近無彩色はHSVでhue/saturationが0になるが、HSLではhue 240°と正のsaturationが保持される。delta約`2e-5`のHSVではhue/saturationが保持され、HSV逆変換で元RGBへ戻る。個別Conversion suiteと全10色standalone suitesが成功。
- **価値または懸念:** HSVの1e-5 achromatic thresholdが微小色差を捨てる挙動と、HSL側との非対称性を明示的に回帰保護する。
- **次に確認すべきこと（未検証）:** アプリの色選択・変換経路が1e-5未満差の保持を必要とするかを確認し、閾値を仕様化する場合は別途判断する。


## 2026-10-09 — Canon Log 3 lower breakpointの出力段差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx` `linearToCanonLog3` lowLinear分岐、`tests/ArtifactCore/ColorBridgeTest.cpp`。
- **確認できた事実:** `lowLinear = -(10^((toe-low)/slope)-1)/scale` の直下ではlinearToCanonLog3が約`0.0407616`を返すが、境界点は`linear <= highLinear`のmiddle branchへ入り約`0.0470002`を返す。float隣接値で約`0.0062386`の段差を観測し、テストへ現行behaviorとして固定した。Bridge suiteと全10 color standalone suite成功。
- **未検証の仮説:** low branchからmiddle branchの式の接続が意図どおりか、Canon Log 3定義資料との照合が必要。
- **価値または懸念:** branch境界付近の色変化が不連続になる可能性。
- **次に確認すべきこと:** 公式Canon Log 3 referenceによる境界の確認。production修正はArtifactCore child repoへ触るため別途明示依頼が必要。
- **decode境界追記:** `canonLog3ToLinear` encoded low threshold `0.04076162`でも段差がある。直下の次表現可能floatで約`-0.0112958`、境界点と直上で約`-0.0140`となり、差は約`0.0027042`。ACEscct (`0.155251...`)とS-Log3 (`171.2102946929/1023`)のdecode境界、Canon Log 3 high境界は隣接floatで連続だった。
- **次に確認すべきこと追記:** encodeとdecode両側のCanon Log 3 low branchの式・thresholdがreferenceに沿うか照合する。


## 2026-10-09 — Canon Log 2 negative toeの往復誤差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx` の`linearToCanonLog2/canonLog2ToLinear`、`tests/ArtifactCore/ColorBridgeTest.cpp`。
- **確認できた事実:** 通常負値`-1e-4`、0、正値0.01ではdispatch round-tripが通るが、`linearToe = -(10^(toe/slope)-1)/scale`で算出した負toe thresholdではencode約`-0.01459125`、decode約`-0.00578928`となり、入力との差は約`0.00194065`。この点をcharacterizationとしてテストへ固定し、Bridgeと全10 standalone suiteは成功。
- **未検証の仮説:** Canon Log 2のnegative toe threshold導出またはencode/decode branch thresholdが整合していない可能性。Canon仕様参照との照合は未実施。
- **価値または懸念:** toe周辺の負scene-linear信号でround-tripが失われる可能性。
- **次に確認すべきこと:** Canon公式curve定義と負値domainの照合。production修正はArtifactCore child repoへの明示依頼がある場合に扱う。
## 2026-10-09 — 読み取り専用surface viewの複数pixel行padding

- **関連:** `tests/ArtifactCore/ImageSurfaceViewTest.cpp`、`ArtifactCore/include/Image/ImageSurfaceView.ixx`。
- **確認できた事実:** 独立テストで幅2 pixel・高さ2行・各行8 float分のpixelデータに4 float分のpaddingを加え、読み取り専用RGBA viewが行末pixelと次行先頭pixelを正しく参照することを確認した。ImageSurfaceView suiteおよび色standalone全10 suiteが成功。
- **価値:** 1 pixel幅のread testや可変viewのpadding testとは別に、const viewで複数pixelを含むrow strideの境界を保護する。
- **次に確認すべきこと:** なし。
## 2026-10-09 — floatから8-bit sRGBへの出力clamp

- **関連:** `tests/ArtifactCore/SurfacePixelConversionTest.cpp`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx`。
- **確認できた事実:** linear floatの負値・1超過RGB、および範囲外alphaを8-bit sRGB targetへ変換し、RGBが[0,255]へclampされ、alphaもclamp後に量子化される独立ケースを追加した。target buildと色standalone全10 suiteの実行で結果を確認。
- **価値:** 中間値の量子化referenceやfloat targetのHDR保持とは別に、byte出力の範囲端での動作を保護する。
- **次に確認すべきこと:** なし。
## 2026-10-09 — binary16変換の丸め境界テスト

- **関連:** `tests/ArtifactCore/SurfacePixelConversionTest.cpp`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx` のfloatToHalf。
- **確認できた事実:** half 1.0の隣接値中点直下・中点・直上、ゼロと最小subnormalの中点直下および中点、最小subnormal、最大有限値65504、overflow境界65520を期待binary16 bit patternで検査する独立ケースを追加した。
- **価値または懸念:** 通常値のexact conversionだけでは見えないrounding, underflow, overflow境界の回帰を検出できる。丸め中点は現行実装で上側へ丸める挙動としてcharacterizeする。
- **次に確認すべきこと:** target buildと全standalone suitesで実行結果を確認する。
## 2026-10-09 — alpha byte量子化の半コード境界

- **関連:** `tests/ArtifactCore/SurfacePixelConversionTest.cpp`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx` のRGBA8出力。
- **確認できた事実:** alpha `0.5f`の直前、ちょうど、直後の隣接float値をRGBA8へ変換し、alpha byteが127 / 128 / 128となる独立ケースを追加した。
- **価値:** 0.5の単一点referenceだけでは覆えないbyte量子化境界前後を明示できる。
- **次に確認すべきこと:** target buildと色standalone suitesで実行結果を確認する。
## 2026-10-09 — gamut conversionの独立suite化

- **関連:** `ArtifactCore/include/Color/ColorGamutConversion.ixx`、`tests/ArtifactCore/ColorGamutConversionTest.cpp`、standalone CMake target。
- **確認できた事実:** 実装は11 gamut presetとACES AP0/AP1対D65 gamut間のD60/D65 adaptation分岐を持つ。既存color standalone suiteにこのmodule専用のテストsource/targetがなかったため、単一module依存targetを追加し、3x3行列積・matrix-vector積、gamut変換の加法性・スカラー倍性、sRGB/Rec.709 primaries互換、Rec.709/Rec.2020/Display P3 primariesとwhiteのXYZ基準値、Rec.2020 inverse matrixのXYZ軸referenceとD65 whiteからneutralへの変換、Display P3 D65 whiteからneutralへの変換、DCI-P3/Adobe RGB preset primariesとmatrix row-sum white XYZおよび各preset white→neutral、DWG forward/inverse matrix axesとrow-sum whiteのinverse結果、DWG→Rec.709のprimaryとsigned/HDR sample reference、ACES AP0/AP1→D65 XYZ adapted primary referencesとAP0↔AP1 D60 matrix reference、自己変換matrix、未知enum値のD65 XYZ fallback、Bradford matrixのwhitepoint往復、ACES AP0/AP1とRec.709間のneutral変換、sRGB/Rec.709/Rec.2020/DCI-P3/Display P3/Adobe RGB matrix間のsigned/HDR往復、全gamut pairのfinite出力を検査する。DCI-P3 presetのwhite row sumは(0.8945869, 1.0000001, 0.9544160)、Adobe RGB presetは(0.9642200, 0.9999999, 0.8252105)で、どちらもD65 white(0.9504559, 1, 1.0890578)とは一致しない。DWG row-sum whiteは(0.9505677, 1.0000001, 0.9602983)で、inverse matrixへの適用結果は(1.0436125, 0.9698435, 0.9996207)となりneutralへ戻らない。これらのテストは外部規格whitepointの正しさではなく、現行matrix値のcharacterizationである。全gamut pairに一律2e-3のround-trip許容差を仮置きした探索では複数pairで0.02〜0.9程度の残差が観測されたため、その期待値は仕様根拠がなく採用しなかった。
- **価値:** ColorSpace transfer/matrix testsとは別APIであるgamut conversionと白色点変換の依存閉包をArtifactCore全体やQtなしで検査できる。
- **次に確認すべきこと:** D60/D65 adaptationを含むgamut pairの正確性許容差を仕様または独立referenceで定める。production修正は別途明示依頼が必要。

## 2026-10-09 — gamut conversionのD60往復と非有限入力

- **関連:** `tests/ArtifactCore/ColorGamutConversionTest.cpp`、`ColorGamutConversion::convert`。
- **確認できた事実:** AP0/AP1とRec.709またはXYZ_D65間の負値・通常値・HDR sampleを往復し、各channelが3e-4以内に戻ることを追加検査した。NaNまたは+infinityを入力すると3出力channelすべてへ非有限値が伝播する。個別suiteとstandalone CTest全11 suiteが成功。
- **価値または懸念:** D60 adaptationを含む行列対の往復と、入力sanitizeを行わない現行APIの非有限値挙動を明示できる。後者は現行characterizationであり、NaN/Infinity sanitizeが公開契約かは未確認。
- **次に確認すべきこと:** sanitizeをAPI要件にする必要性が出た場合に仕様を定める。production修正はArtifactCore child repositoryへの別途明示依頼が必要。


## 2026-10-09 — BlendMode未知値とopacity早期return

- **関連:** `tests/ArtifactCore/ColorBlendModeStandaloneTest.cpp`、`ColorBlendMode::blend`。
- **確認できた事実:** 実装はopacityを[0,1]へclampし、0以下ならmode dispatch前にbaseを返す。未知の`BlendMode`値はswitch defaultでbaseを返す。これらの独立ケースを追加し、blend targetとstandalone全11 suiteが成功。
- **価値または懸念:** enumの未知値fallbackとopacity境界の挙動を保護する。未知値fallbackは現行挙動のcharacterizationであり、公開仕様として固定されているかは未確認。
- **次に確認すべきこと:** なし。


## 2026-10-09 — Harmonizer無彩色と複数回転angle

- **関連:** `tests/ArtifactCore/ColorHarmonizerContractTest.cpp`、`ColorHarmonizer::getAnalogous`ほか。
- **確認できた事実:** black/whiteへcomplementary、analogous、triadic、split-complementary、tetradicを適用しても各出力は無彩色のままalphaを保持する。base hue 30°にanalogous angle -400°を指定すると出力hueは350°と70°。Harmonizer targetとstandalone全11 suiteが成功。
- **価値:** undefined hueを持つ無彩色の中立性、alpha保持、360°を複数回越える負angleのwrapを保護する。
- **次に確認すべきこと:** なし。


## 2026-10-09 — Luminance未知standardとinfinity inspection

- **関連:** `tests/ArtifactCore/ColorLuminanceContractTest.cpp`、`ColorLuminance::calculate/inspectBroadcastSafe`。
- **確認できた事実:** 未知の`LuminanceStandard`値はcalculateとtoGrayscale双方でRec.709係数へfallbackする。broadcast inspectionは+∞または−∞channelをluminance violationとgamut violationの両方として報告する。個別targetとstandalone全11 suiteで確認。
- **価値または懸念:** enum fallbackと両符号infinityの異常値検出を保護する。未知enumのfallbackは現行characterization。
- **次に確認すべきこと:** なし。


## 2026-10-09 — ColorLUT factoryとintensity異常値

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::createIdentity/withIntensity`。
- **確認できた事実:** identity LUT factoryへ0、1、−4を渡すと有効な2×2×2 gridを返す。`withIntensity` はNaNと±∞をすべて1へ正規化し、full LUT側へ補間する。LUT targetとstandalone全11 suiteが成功。
- **価値:** 低い無効サイズと異常なintensity値に対するfactory／copy operationの既存境界を保護する。
- **次に確認すべきこと:** なし。


## 2026-10-09 — ColorLUT load failure recovery

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::load`。
- **確認できた事実:** valid identity LUTに未知拡張子loadを行うと失敗してinvalid stateになる。その後、同じobjectへvalid CUBEをloadするとvalidity、error message、format、size、nameが再設定される。
- **価値:** 連続loadの失敗状態が次の正常loadに残らないことを保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — invalid LUT QColor identity behavior

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::apply(QColor)` / `applyWithIntensity`。
- **確認できた事実:** 不正なCUBEデータで生成された無効LUTに対し、QColor適用と強度0 / 0.35 / 1.0の適用がRGB・alphaを維持し、source QColorも変更しない。対象CTestとstandalone全11 suiteが合格。
- **価値:** 読み込み失敗後の無効LUTが色を破壊しない適用境界を保護する。
- **次に確認すべきこと:** 強度の非有限値は別契約として扱う必要性を判断してからテスト対象を決める。


## 2026-10-09 — failed ColorLUT reload state transition

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::load/loadFromCube`。
- **確認できた事実:** valid CUBEの後にsample不足CUBEを同一instanceへloadすると、sample bufferは空になりvalidityはfalse、filePath/nameは失敗入力に更新されerrorが設定される。formatと前回grid sizeは残り、applyは入力RGBを変えない。LUT targetとstandalone全11 suiteが合格。
- **価値:** 成功したLUTを更新しようとして失敗した場合に、残存sampleが有効データとして誤適用されない現在の状態遷移を保護する。
- **次に確認すべきこと:** format間の失敗再読込でも同じmetadata保持規則か、既存fixtureで差分確認する。


## 2026-10-09 — invalid image import preserves existing LUT

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromImage`。
- **確認できた事実:** 有効なLUTへnull QImageまたはlutSize 1 / 257を渡すとfalseを返し、既存sample、validity、name、grid dimensionsを維持する。errorMessageは失敗診断として設定される。対象ColorLUT CTestおよびstandalone全11 suiteが合格。
- **価値:** 引数検証段階のimage import失敗が既存LUTデータを消さない契約を保護する。
- **次に確認すべきこと:** 面積不足など検証後段の失敗と状態遷移が異なる点は既存ケースで引き続き監視する。


## 2026-10-09 — insufficient Hald tile import invalidates LUT

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromImage`。
- **確認できた事実:** 有効な2³ LUTへ5×4画像とlutSize 3を指定するとtile面積不足でfalseとなり、validityはfalse、samplesは消去、formatはPNG、gridは3³、nameは維持される。source imageは変更されない。対象ColorLUT CTestおよびstandalone全11 suiteが合格。
- **価値:** 引数拒否と画像構造不足の異なる失敗経路が別々の状態遷移を持つことを回帰保護する。
- **次に確認すべきこと:** image pixel抽出の境界は水平/垂直tile配置ケースで検証済み。より大きなlutSizeの整数除算端条件を必要に応じて追加する。


## 2026-10-09 — Hald file decode failure preserves LUT

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromHaldCLUT`。
- **確認できた事実:** 存在しないPNG pathのdecode失敗はfalseを返し、既存LUTのvalidity、grid samples、name、format、size、dataSizeを保持し、errorMessageを設定する。ColorLUT対象CTestおよびstandalone全11 suiteが合格。
- **価値:** Hald file I/O失敗時に既存の有効データが保持される公開API契約を直接保護する。
- **次に確認すべきこと:** 画像ファイルの正常ロード結果は、QImageからの既存Hald tile/channel ordering testsと同じfixtureをファイル保存経由でも検証できるか確認する。


## 2026-10-09 — Hald PNG load matches in-memory import

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromHaldCLUT/loadFromImage`。
- **確認できた事実:** 33³の非対称RGBA gridをPNG保存後loadFromHaldCLUTし、memory image importとvalidity、PNG format、33³ dimensions、data size、選択した8 corner/interior RGB samplesが一致する。file decode/importはdefault name `Identity`を保持し、errorを空にする。ColorLUT対象CTestとstandalone全11 suiteが合格。
- **価値:** PNG decodeとfile APIの委譲経路を、Hald tile orderingとsample conversionを通して直接回帰保護する。
- **次に確認すべきこと:** fixtureはアルファ値も変化させるが、LUTはRGBのみ保持する契約を別途明記する場合はalphaを無視するload検査を追加する。


## 2026-10-09 — LUTManager directory scan imports Hald PNG

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory`。
- **確認できた事実:** 33×33 tileの33³ Hald PNG、壊れたPNG、未対応BMPを同じdirectoryに置くとload countは1で、PNGはbasenameでregistryへ入り、PNG format・格子寸法・選択sampleを保持する。壊れた/未対応ファイルは登録されない。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値:** directory filterからColorLUT decode・validity判定・manager登録までを通した統合境界を保護する。
- **次に確認すべきこと:** scan filterが列挙するjpeg/tif suffixとColorLUTのload dispatch対応suffixの整合を別ケースで確認する。


## 2026-10-09 — directory image suffix and loader dispatch mismatch

- **関連:** `ArtifactCore/src/Color/ColorLUT.cppm`、`tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory/ColorLUT::load`。
- **確認できた事実:** directory scan filterには`.jpeg`と`.tif`が含まれるが、ColorLUT load dispatchの画像拡張子は`.png`、`.jpg`、`.tiff`のみ。`.jpeg`/`.tif`候補は`Unknown LUT format`で拒否され、同ディレクトリの有効PNGのみ登録されるテストがstandalone全11 suiteで成功。
- **価値または懸念:** filter上サポートされるように見えるsuffixが実際には登録されず、ユーザーのファイル選択・directory scan結果が一致しない。
- **次に確認すべきこと:** `.jpeg`と`.tif`を正式サポートするか、scan filterから除外するかを仕様判断してから実装・テスト契約を更新する。今回の変更ではproduction実装は変更していない。


## 2026-10-09 — CUBE full-grid save and reload

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::saveToCube/loadFromCube`。
- **確認できた事実:** 非対称3×3×3 LUTの全27 RGB tripletをCUBEへ保存・再読込し、各channel誤差1e-6以内、name、Cube format、3D dimensions、dataSize一致を確認。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値:** corner/interiorを含む全格子の書き出し順・読み込み順・serialization精度の回帰を検出する。
- **次に確認すべきこと:** CUBE保存の出力行数・header roundtripを独立に確認する場合は、fixtureのgrid全sample検証と重複しないheader/comment契約を対象にする。


## 2026-10-09 — ColorLUT setValue rejects non-finite channels

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::setValue`。
- **確認できた事実:** NaNと正負InfをRGB各channelのいずれかへ置いた9 tripletを既存sampleへsetValueしてもsampleは変化せず、LUT validityも維持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値:** どのchannelの非有限値もtriplet全体の更新を拒否する境界を保護する。
- **次に確認すべきこと:** rawData mutable accessorを通じた非有限値注入後のsample/apply sanitize挙動は別の公開低レベル契約として検査可能。


## 2026-10-09 — ColorLUT format reloadとcopy assignment

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::load` / copy assignment。
- **確認できた事実:** 同じobjectへCUBEを読み込んだ後、CSPを読み込むとformat、name、filePath、error state、sampleがCSP側へ切り替わる。copy assignment後はdestinationがsource格子サイズを持ち、destinationのsample編集はsourceへ影響しない。
- **価値:** 再利用時のmetadata/data stale stateとassignmentの所有権独立を保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — CUBE header size inference and limits

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromCube`。
- **確認できた事実:** CUBEに`LUT_3D_SIZE`がなく8 RGB tripletある場合、loaderは2×2×2を推定する。dimension 1および257はinvalidとしてdiagnostic付きで拒否される。
- **価値:** headerless cube inferenceと2..256 dimension acceptance boundsを保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — 3DL normalization and non-finite rejection

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFrom3dl`。
- **確認できた事実:** 3DL maximum sample 8に対し他channelの2/4/8が0.25/0.5/1.0へ正規化される。sampleに`inf`が含まれるfileはinvalidとなりerror messageが設定される。
- **価値:** global max normalizationと非有限sample拒否を、既存16段階の均等fixtureとは異なる値分布で保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — CSP size and sample-count rejection

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromCsp`。
- **確認できた事実:** CSP loaderはsize 1/257、数値でない宣言サイズ、2³ gridに不足または余分なsampleがあるfixtureをinvalidとして拒否する。
- **価値:** parserのdimension bounds、header parse、sample triplet数一致を一括して回帰保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — HaldCLUT vertical tile layout

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromImage`。
- **確認できた事実:** 2×2 LUTを2×4 ARGB imageへzごとの縦積み配置にして読み込むと各格子点のRGB値とz slice順が保持され、alpha値はLUT RGBへ入らず、入力imageも変化しない。
- **価値:** 既存の横方向タイルケースにないvertical tile indexingとread-only input behaviorを保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — QColor LUT application with zero alpha

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::apply(const QColor&)`。
- **確認できた事実:** alpha 0のQColorへred inversion LUTを適用するとRGBは変換され、alpha 0と入力QColorの各channelは保持される。
- **価値:** 透明pixelでもstraight RGBの適用結果を返しつつalphaを維持するQColor API経路を保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — 3DL dimension limits and CUBE save failure

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFrom3dl/saveToCube`。
- **確認できた事実:** 3DL header dimension 1と257はinvalidとして診断付きで拒否される。存在しないparent directoryへのCUBE保存はfalseを返し、source LUTのvalidity/name/sampleを変更しない。
- **価値:** 3DL dimension boundsと保存open failureの非破壊性を保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — headerless CSP size inference

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromCsp`。
- **確認できた事実:** CSPの`LUT_3D_SIZE`を省略しBEGIN/END DATA内に8 tripletを置くと、loaderは有効な2×2×2 gridを推定し終端値を保持する。
- **価値:** CUBE/3DLと同様に存在するCSPのcubic sample count inferenceを独立に保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — CSP END DATA boundary

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromCsp`。
- **確認できた事実:** explicit BEGIN DATA/END DATAが存在するCSPでEND DATA後のRGB tripletは格子sampleへ含まれず、block内の8点だけで有効な2×2×2 LUTが構築される。BEGIN DATAより前のtripletは取り込まれるため、テスト対象をEND後に限定した。
- **価値:** parserがEND DATA以降の行を読み飛ばす状態境界を保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — CSP sample row parsing

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromCsp`。
- **確認できた事実:** data block内で要素数が2または4の行はsampleとして追加されず、3要素だが数値変換に失敗する行は`Invalid CSP LUT sample`でloadを失敗させる。
- **価値:** row token countによるskipとsample parse failureの異なる扱いを保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — ColorLUT move and self-assignment

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT` move/copy special members。
- **確認できた事実:** move constructorとmove assignment後、destinationはsourceのname、格子サイズ、sampleを保持する。self-copyおよびself-move assignmentでは有効性とsampleが保たれる。moved-from objectにはアクセスせず、破棄だけを確認対象とする。
- **価値:** PImpl所有権の移譲とself-assignment guardを回帰保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — LUT Manager directory reload

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory/registerLUT`。
- **確認できた事実:** 同じbasenameの有効CUBEを同じmanagerへ再loadすると返却数1のまま既存registry entryのgrid valuesが新ファイルの値へ置換される。空directoryは0を返し既存登録を保持する。
- **価値:** directory scanを繰り返したときのregistry更新と空scanの非破壊性を保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — LUT Manager case-sensitive names

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager`のQMap registry。
- **確認できた事実:** `Alpha`と`alpha`は別キーとして存在し、removeもcase-sensitive。`lutNames()`はQMap key順で`Alpha`, `Beta`, `alpha`, `beta`を返す。
- **価値:** registry name lookupのcase behaviorと一覧順を保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — built-in LUT generator contracts

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`BuiltinLUTs::builtinLUTNames/registerBuiltins`。
- **確認できた事実:** built-in一覧は重複のない9名で、manager登録後に全名が存在する。各LUTは有効な17×17×17格子で、float sample全件が有限かつ[0,1]内。
- **価値:** built-inの列挙と登録表の同期、および生成データの妥当性を全格子走査で保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — RGB888 LUT image application

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::applyToImage`。
- **確認できた事実:** odd width 3のRGB888 imageをARGB32へ変換してLUT適用し、2行すべてでRGB変換・opaque alpha 255・source画像不変を確認した。odd widthは入力のrow paddingを持つ。
- **価値:** RGBA source以外のformat変換とpadded RGB row traversalを保護する。
- **次に確認すべきこと:** LUT targetとstandalone全suiteで実行する。


## 2026-10-09 — ColorLUT apply sanitizes mutable raw samples

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::rawData/apply`。
- **確認できた事実:** mutable rawDataでlut gridにNaN、±Inf、範囲外finite値を注入後、public applyは該当cornerを通るRGB結果を有限かつ[0,1]へ戻す。非有限結果は0、-0.5は0、1.5は1へなる。private sample APIには触れていない。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値:** writable low-level bufferを使う利用側が壊れたsampleを残した場合も、色適用結果が非有限化・範囲逸脱しない契約を保護する。
- **次に確認すべきこと:** mutable rawData pointer経由でgridを更新した後にsaveToCubeがその変更を反映する契約も、必要なら公開APIで検証できる。


## 2026-10-09 — ColorLUT move result outlives source

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、ColorLUT move constructor/assignment。
- **確認できた事実:** move constructorのsourceをlambda終了で破棄した後も戻り値LUTがvalid・name・sampleを保持する。move assignmentもsource scope終了後にdestinationがvalid・name・2³ size・sampleを保持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値:** PImpl raw pointer所有権移譲後のmoved-from destructorが移譲先の状態を破壊しないことを検証する。
- **次に確認すべきこと:** copy後のrawData直接変更に対する独立性は既存setValue copy testsと補完関係にあるため、必要になった場合に追加する。


## 2026-10-09 — ColorLUT combine identity and invalid operand

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::combine`。
- **確認できた事実:** 非線形3³ LUTへidentityを左または右から合成した結果が全27 grid samplesで元LUT値と一致する。どちらかのoperandがinvalidならreceiver相当を返し、valid receiverのsampleは変更されず、invalid receiverのvalidity/dataSize/filePathも維持される。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値:** composition identity lawとinvalid operand fallbackの状態保持を保護する。
- **次に確認すべきこと:** 異なるgrid resolution同士のcombineはreceiver resolutionへ結果を作る現仕様があるため、異解像度resampling契約を別途固定する余地がある。


## 2026-10-09 — ColorLUT combine across grid resolutions

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::combine`。
- **確認できた事実:** 3³ receiver + 2³ argumentでは結果は3³、2³ receiver + 3³ argumentでは結果は2³。両方で全出力格子sampleがreceiver sampleへargument LUTを直接applyしたRGBと一致する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値:** 解像度差を持つLUT compositionの出力解像度・resampling・適用順を固定する。
- **次に確認すべきこと:** 非立方LUTは現行loader/factoryが公開経路で生成しないため、異軸寸法の動作を検査するには入力formatサポートが必要。


## 2026-10-09 — ColorLUT withIntensity copy independence

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::withIntensity`。
- **確認できた事実:** 非対称3³ sourceへ0.4 intensityを適用し、全27 grid samplesがidentityとの線形補間式に一致する。resultはvalidity/name/path/format/grid dimensions/data sizeを保持。sourceを編集してもresultは変わらず、result編集もsourceへ伝播しない。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値:** intensity variant生成時の格子補間とdeep-copy所有権の双方を保護する。
- **次に確認すべきこと:** 既存endpoint testが0/1補間を扱い、このケースはinterior intensityとコピー分離を補う。


## 2026-10-09 — inverse LUT full-range round-trip coverage

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::inverted`。
- **確認できた事実:** 17³のfull-domain非線形monotonic curve（各channelが0→0、1→1）に対し、全軸端点を含む9³=729 samplesでsource→inverse RGB誤差0.01以内が成功した。一方、既存のshifted output range curveをcube端点まで試すと最大約0.086の誤差が出たため、そのfixtureは現在の3 interior sample契約に留めた。
- **価値または懸念:** inverse LUTのround-trip品質はsource LUTのoutput gamutが[0,1]全域を使うかで大きく変わる。特に端点外挿を期待する契約は現行固定点＋有限格子実装では成立しない可能性がある。
- **次に確認すべきこと:** shifted output rangeで要求する逆挙動（範囲内のみ、endpoint extrapolation、clamp）を仕様で決める前に、既存テストの許容差を広げない。


## 2026-10-09 — headerless CUBE sample count validation

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromCube` headerless size inference。
- **確認できた事実:** headerless CUBEの7、9、15 RGB points（2³/3³の近傍だが立方数ではない）はinvalidとなり、8 triplet + 1 scalar remainderもinvalidとなる。立方grid推定の前後でsample countが一致しないデータを受理しない。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値:** size inferenceの丸めが不完全・余剰sample dataを隠してvalid LUTにしない境界を保護する。
- **次に確認すべきこと:** explicit LUT_3D_SIZEありのCUBEで余分 scalar/channelと非立方の宣言sizeは既存sample-count検査がカバーしているか照合を続ける。


## 2026-10-09 — CUBE save text and sample traversal

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::saveToCube`。
- **確認できた事実:** saveしたCUBEのTITLE/comment/size header、blank line、末尾newlineを直接比較し、全27 tripletをsource格子のz/y/x loop順（x fastest）で値照合した。reload側実装との対称性に依存しない。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値:** header形式・sample行数・シリアライズ走査順を外部ファイル契約として保護する。
- **次に確認すべきこと:** quoted title中のquote escapingは現save implementationにescapingがないため、利用可能文字の仕様を決める場合に別検討する。

## 2026-10-09 — LUTManager duplicate basename registry behavior

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory`。
- **確認できた事実:** 同じbasenameの有効CUBEとCSPを別ファイルとしてdirectory loadすると、読み込み件数は2だがbasename keyのregistry entryは1件となり、格納値はどちらかのvalid fixtureに一致する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** ファイル読み込み件数とbasename registry件数が異なる衝突ケースを明示的に保護する。
- **次に確認すべきこと:** 同basename衝突時のwinner決定順をAPI契約にする必要があるか、他のmanager利用箇所と併せて確認する。

## 2026-10-09 — LUTManager missing directory preserves registry

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory`。
- **確認できた事実:** 既存登録とsampleを設定した後、存在しないdirectory pathを読み込むと0件を返し、registry namesと登録LUT sampleは維持される。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** directory path誤りや消失でmanagerの既存状態が不用意に消えない挙動を保護する。
- **次に確認すべきこと:** 既存directory内の全ファイルがinvalidな場合も同じく既存registryを維持するかを別ケースで確認する。

## 2026-10-09 — LUTManager all-invalid directory preserves registry

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory`。
- **確認できた事実:** 壊れたCUBEと未対応JPEGだけを含む既存directoryを読み込むと0件となり、どちらのbasenameも登録されず、既存entryとsample値が維持される。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** scan候補に入るがdecodeできないファイルを含むdirectory loadの非破壊的な失敗挙動を保護する。
- **次に確認すべきこと:** 有効ファイルとinvalidな同basenameが併存する場合のregistry結果はcollision testとは別の順序依存ケースとして必要か検討する。

## 2026-10-09 — LUTManager invalid basename collision preserves valid file

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory`。
- **確認できた事実:** `QDir::Name`順で有効`shared.cube`の後にinvalid `shared.csp`を走査してもload件数は1で、registryの`shared`は有効CUBE formatと全白corner sampleを保持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** invalid candidateはbasename衝突時にも登録済みvalid値を上書きしないことを保護する。
- **次に確認すべきこと:** 逆にvalid候補が同basenameで複数ある場合は後のvalidがreplaceするため、winner順を仕様にする必要があれば別途明示テストする。

## 2026-10-09 — LUTManager valid basename collision winner order

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory`。
- **確認できた事実:** 有効な同basename `shared.csp` と `shared.cube` をdirectory scanすると両方を読み込み件数に数えるが、`QDir::Name`順で後から処理される`.cube`がbasename keyを置換し、registryはCUBEの全白cornerを返す。両fixtureの単独validityも確認し、対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** basename衝突時に後のvalid fileがwinnerとなる現在の順序依存動作を明示する。QtのName sortに依存するため、OS間の大小文字規則を含む一般的な順序保証とは区別する。
- **次に確認すべきこと:** 複数extension間のwinnerを安定API契約とすべきか、またQt/OS差を避ける明示tie-breakが必要かを将来の仕様検討で判断する。

## 2026-10-09 — LUTManager registration and retrieval copy isolation

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::registerLUT/getLUT`。
- **確認できた事実:** source LUTをregistryへ登録した後sourceを編集してもregistry sampleは初期値を保持し、`getLUT`の戻り値を編集しても次の取得結果は変化しない。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** managerの値保持APIが外部のLUT編集から独立したcopyとして振る舞う契約を保護する。
- **次に確認すべきこと:** `getLUT`で存在しないキーを得た場合のinvalid sentinelのmetadata/sample値を確認する。

## 2026-10-09 — LUTManager missing key returns default identity

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::getLUT`。
- **確認できた事実:** 未登録keyのgetLUTはinvalid sentinelではなく、有効な`Identity` / Cube / 33³ LUTを返し、端点sampleは黒/白。戻り値編集後もそのkeyはregistryに追加されず、次の取得はidentity端点を保持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 存在確認`hasLUT`なしにgetLUTだけ使う呼び出し側が、identityを「未登録」状態と誤認する余地を明確にする。
- **次に確認すべきこと:** 実利用箇所でhasLUT/getLUTの組み合わせを確認し、missing-key identityが意図されたAPI契約かは別途判断する。今回のテストでは既存挙動を記録し、production APIは変更しない。

## 2026-10-09 — LUTManager retrieved value lifetime across registry changes

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::getLUT/removeLUT/registerLUT/clear`。
- **確認できた事実:** getLUTで保持した値コピーはregistryからremoveした後もvalidでsampleを保持する。同名keyへ異なるLUTを再登録するとregistryはreplacement値を返す一方、先に取得したcopyは元値を維持し、clear後も同様に生存する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** manager registryの変更と呼び出し側が保持したLUT値の寿命・状態が独立していることを保護する。
- **次に確認すべきこと:** 必要ならcopyのname/path/format metadataも同じ lifecycleで比較する。既存のcopy testsと重複する場合は追加せず統合する。

## 2026-10-09 — LUTManager trims surrounding control whitespace

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::registerLUT`。
- **確認できた事実:** 登録名をtab/newline/spaceで囲んで登録すると、registryはtrim済み`grade` keyだけを列挙・検索でき、sample値を保持する。元の空白付き名では検索できない。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** UIやファイル由来の行末制御文字を含む登録名の正規化を保護する。
- **次に確認すべきこと:** Unicode separatorや内部空白の正規化は現APIの要件がない限り追加で変換しない。

## 2026-10-09 — LUTManager lookup and removal use exact keys

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::registerLUT/hasLUT/getLUT/removeLUT`。
- **確認できた事実:** registerLUTのみ名前をtrimする。trim済みkeyを前後空白付きでhas/get/removeするとmiss、getは既定Identity、removeはno-opとなり、正確なtrim済みkeyでremoveすると削除される。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** API間の名前正規化非対称性を既存挙動として可視化する。呼び出し側が異なる空白を渡すと存在中の値を見落とす可能性がある。
- **次に確認すべきこと:** manager API全体で呼び出し側nameも正規化すべきかは別の仕様判断であり、今回のテストでは挙動のみを固定した。

## 2026-10-09 — LUTManager directory load preserves unrelated keys

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory/registerLUT`。
- **確認できた事実:** directory対象basenameと無関係な既存keyを保持した状態でvalid CUBEをloadすると、件数1で対象key sampleがファイル値にreplaceされる一方、無関係keyとsampleはそのまま維持される。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** directory loadがregistry全体を置換せず、各有効ファイルbasenameだけをupsertする挙動を保護する。
- **次に確認すべきこと:** 重複basenameで同keyを複数回更新する場合は別のcollision testsがすでに扱っている。削除ファイルをregistryから自動削除する契約は現実装にない。

## 2026-10-09 — LUTManager repeated removal is idempotent

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::removeLUT`。
- **確認できた事実:** 既存keyを削除後に同keyを再削除し、別のmissing keyも削除しても例外・状態変化はなく、他keyとsampleは維持される。先にgetLUTした値コピーもvalid/sampleを保持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** manager remove APIの繰り返し呼び出し耐性と他entry/copyへの非干渉を保護する。
- **次に確認すべきこと:** removeの戻り値はAPIにないため、成功/未存在の区別を前提にした契約は追加しない。

## 2026-10-09 — LUTManager directory scan is non-recursive

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory`。
- **確認できた事実:** root直下に有効`root-grade.cube`、1階層下の`nested` directoryに有効`nested-grade.cube`を置くと、load件数は1でroot basenameのみ登録される。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 読み込み対象が指定dir直下のfileであり、subdirectoryへ再帰しない現実装を保護する。
- **次に確認すべきこと:** 再帰scanを将来要件にする場合は明示的なAPI仕様変更と既存flat directory挙動の保持が必要。

## 2026-10-09 — CUBE finite extended-range samples and apply clamp

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromCube/apply`。
- **確認できた事実:** 有限な範囲外sample (-0.5, 0.25, 1.5) のCUBEは有効として格子値をそのまま保持し、格子cornerへの適用結果は(0, 0.25, 1)へclampされる。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** loaderのHDR/negative値保持とpublic applyの表示出力clampを分けて保護し、raw LUT値をloader時点で破壊しない。
- **次に確認すべきこと:** CSP/3DLも同じsample range contractか、各formatの個別規則に沿って確認する。

## 2026-10-09 — CSP extended range and 3DL unit range behavior

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFromCsp/loadFrom3dl/apply`。
- **確認できた事実:** CSPは有限範囲外sample (-0.5, 0.25, 1.5) を格子値として保持し、public applyで(0, 0.25, 1)へclampする。3DLの最大sampleが1のfixtureでは0.5/0.25/0.75等の小数sampleが変更されず、既存のmax>1 fixtureは最大値正規化を検査している。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** format別sample range policy（CUBE/CSP raw finite、3DLはmax>1時scale）を押さえる。
- **次に確認すべきこと:** 3DLでmaxが0以下のall-zero fixtureは正常なblack LUTとして有効か、既存guardと合わせて確認できる。

## 2026-10-09 — 3DL all-zero black LUT remains valid

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::loadFrom3dl/apply`。
- **確認できた事実:** dimension 2で全8 RGB tripletがゼロの3DLはvalid、format `_3dl`、8格子点を保持し、全格子sampleと任意入力のapply結果はblackになる。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** max=0のとき割り算を行わない経路が正常なblack LUTを受理することを保護する。
- **次に確認すべきこと:** なし。`maxValue > 1`時だけscaleする現在の条件と整合している。

## 2026-10-09 — identity LUT full-grid sample coverage

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::createIdentity/getValue`。
- **確認できた事実:** 2³、3³、17³ identity LUTの全5,112 grid samplesを走査し、各点RGBがx/(N-1), y/(N-1), z/(N-1)とfloat完全一致する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 初期化loopのaxis順、stride、端点と中間分割値をサンプル点spot-checkより広く保護する。
- **次に確認すべきこと:** さらに大きい256³全点走査はテスト時間・診断量が大きいため、境界factory sizeと別の代表gridを組み合わせる必要性を検討する。

## 2026-10-09 — identity factory upper size boundary

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::createIdentity`。
- **確認できた事実:** size 256はvalidな256³ LUTとして約192 MiBのsample bufferを確保し、dimensions/data byte size、black/white endpoints、3つの非対称座標でRGB各軸値を保持する。257は2³ fallbackとなる。強化後の対象ColorLUT CTestは約2.1秒、standalone全11 suiteも合格。
- **価値または懸念:** factoryで許される最大sizeと上限超過fallback、非対称座標でのaxis/stride設定を実割当で保護する。一方、約192 MiBの一時メモリと約2秒の実行時間が生じる。
- **次に確認すべきこと:** CI memory budgetが厳しい環境では、この境界テストをlow-resource profileへ分離すべきかをsuite実行時間と併せて判断する。

## 2026-10-09 — trilinear multilinear-field grid coverage

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::trilinearInterpolation/apply`。
- **確認できた事実:** 3³格子の非対称な各RGB出力へxy/xyz/xz/yz交差項を含むmultilinear polynomialを設定し、各軸9値（端点、格子境界、セル内部）の729入力について解析式と比較、全channel誤差2e-6以内を確認。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 2³単一中心値spot checkを越え、複数cellの座標切替、軸重み、交差項を決定的に検証する。
- **次に確認すべきこと:** 任意の非-multilinear dataの近似精度はLUT解像度依存なので、必要な場合は別の interpolation error budgetとして扱う。

## 2026-10-09 — applyWithIntensity finite out-of-range extrapolation

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::applyWithIntensity`。
- **確認できた事実:** red inversion LUTとsource red 0.2でintensity 1.5はred約1.1、intensity -0.5はred約-0.1を返し、green/blueとalphaはQColor channel quantization誤差内で保持される。Qt QColorはこのRGBを範囲外のままvalid colorとして保持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** `withIntensity`は強度を[0,1]へclampする一方、`applyWithIntensity`は有限範囲外値を外挿するAPI差を明示する。testは現挙動をcharacterizeし、範囲外強度を仕様として推奨する意図はない。
- **次に確認すべきこと:** 呼び出し側がuser-provided intensityをどちらのAPIへ渡すか確認し、両APIのclamp policy統一が必要か仕様判断する。

## 2026-10-09 — withIntensity finite out-of-range clamping

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::withIntensity`。
- **確認できた事実:** 非対称3³ source LUTの全27格子sampleでintensity -0.25のresultがidentity格子と一致し、1.25のresultがsource LUTと一致する。source自身も未変更。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** `withIntensity`の有限範囲外値clampを全格子で保護し、前回characterizeした`applyWithIntensity`の外挿挙動とのAPI差を明瞭にする。
- **次に確認すべきこと:** intensity API間のpolicy統一は別途仕様判断事項として既存Insightに記録済み。

## 2026-10-09 — ColorLUT getValue axis bounds

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::getValue`。
- **確認できた事実:** 3³ gridでx/y/z各軸へ-1、size、-32、32を個別に渡した12 invalid coordinateはいずれもzero vectorを返し、(0,0,0)/(2,2,2)のvalid cornersはblack/whiteを保持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 軸ごとの下限・上限比較と広い範囲外整数値の安全fallbackを保護する。
- **次に確認すべきこと:** 極端なint min/maxもbounds checkのshort-circuit後にindex演算へ進まないか、必要なら境界を追加する。

## 2026-10-09 — ColorLUT getValue integer extremes

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::getValue`。
- **確認できた事実:** 各軸へ`std::numeric_limits<int>::min()`と`max()`を個別に与えた6ケースもzero vectorを返し、通常座標の両cornerは正しいidentity sampleを保持する。既存の近接範囲外12ケースと併せ計18 case、対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 異常に大きい座標をindex演算へ進めず拒否するsigned integer境界を明示する。
- **次に確認すべきこと:** なし。実装はindex計算前に負値とdimension上限を判定している。

## 2026-10-09 — ColorLUT setValue integer extremes

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::setValue`。
- **確認できた事実:** 各軸へ`INT_MIN`/`INT_MAX`を個別に指定した6 writeはいずれもno-opで、事前設定したcenter sampleとidentity corner値が変わらず、LUT validityも保たれる。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 不正signed coordinateが負数をsize_tへcastするindex演算へ入る前に拒否されることをmutation非発生で保護する。
- **次に確認すべきこと:** なし。`setValue`はrange guardでreturnしてからindex計算する。

## 2026-10-09 — LUTManager file path rejected as directory

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory`。
- **確認できた事実:** 実在する通常file pathをdirectory pathとして渡すと0件を返し、既存registry keyとsampleを維持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 存在確認だけではdirectory扱いできないpath入力に対しmanager stateが非破壊であることを確認する。ACL変更に頼るテストではない。
- **次に確認すべきこと:** unreadable directoryのACL挙動はplatform依存で、現standalone cross-platform contract testには含めない。

## 2026-10-09 — LUTManager repeated clear and reuse

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::clear/registerLUT/getLUT`。
- **確認できた事実:** 複数key登録後のclearを2回呼んでもregistryは空を維持し、先に取得したLUT copyはvalid/sampleを保持する。その後、新keyを登録して再取得できる。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** clearのidempotencyだけでなく、空状態からのregistry再利用と外部copy寿命を一つのsequenceで保護する。
- **次に確認すべきこと:** なし。公開managerにはclear後の追加初期化を要求する契約がない。

## 2026-10-09 — LUTManager repeated directory reload snapshots

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory/getLUT`。
- **確認できた事実:** 同名CUBE fileの内容をA→B→Aへ上書きし、各reloadが1件を返し、registry sampleが各版へ置換され、key数は1のままであることを確認した。A/B各版の`getLUT` copyは後続reload後も取得時のsampleを保持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** directory reloadのregistry置換と、既に公開されたby-value LUT snapshotの寿命を同一sequenceで検査する。
- **次に確認すべきこと:** なし。公開APIは`getLUT`を値返却し、reloadは既存nameを登録値で置換する。

## 2026-10-09 — Invalid same-name directory LUT preserves registry

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`LUTManager::loadFromDirectory`。
- **確認できた事実:** 有効な`grade`と無関係な`unrelated`を事前登録し、壊れた`grade.cube`だけを含むdirectoryをloadすると0件で、両key/sampleが残る。事前に取得した`grade` copyも同じsampleを保持する。対象ColorLUT CTestとstandalone全11 suiteが合格。
- **価値または懸念:** invalid fileのbasenameが既存registry keyと衝突しても、部分的なparse失敗が既存値や無関係entryを破壊しないことを確認する。
- **次に確認すべきこと:** なし。実装は`isValid()`の場合に限り`registerLUT`する。

## 2026-10-09 — QColor LUT alpha range

- **関連:** `tests/ArtifactCore/ColorLUTContractTest.cpp`、`ColorLUT::apply(const QColor&)`。
- **確認できた事実:** 非対称2³ LUTをalpha 0、0.125、0.5、0.875、1.0の色へ適用し、RGBの変換、alpha維持、source QColor不変を検査した。初回の1e-6 toleranceはQColorのfloat channel丸め（観測最大約3.8e-6）で不合格となり、1e-5 toleranceで対象とstandalone全11 suiteが合格。
- **価値または懸念:** 完全透明を含むopacity range全体で色変換がalphaを変更せずsourceにも書き戻さないことを保護する。toleranceはQt QColorのfloat表現差を考慮する。
- **次に確認すべきこと:** なし。alpha値とRGB値の実測許容差内で一致する。

## 2026-10-09 — Signed/HDR RGB through HSV and HSL

- **関連:** `tests/ArtifactCore/ColorConversionTest.cpp`、`ColorConversion::RGBToHSV/HSVToRGB/RGBToHSL/HSLToRGB`。
- **確認できた事実:** 負のchannelと1超えchannelを含む4色について、RGB→HSV→RGBおよびRGB→HSL→RGBが各channel誤差2e-6以内でsourceへ戻る。独立ColorConversion CTestおよびstandalone全11 suiteが合格。
- **価値または懸念:** unit RGB gridの往復に加え、線形/HDR処理から来るsigned/out-of-range RGBがHSV/HSL変換で不用意にclampされない性質を保護する。
- **次に確認すべきこと:** なし。この追加は指定有限sampleでの往復結果を固定する。

## 2026-10-09 — Signed/HDR HSV/HSL intermediate values

- **関連:** `tests/ArtifactCore/ColorConversionTest.cpp`、signed/HDR RGB roundtrip test。
- **確認できた事実:** 4つのsigned/HDR RGB sampleでHSV/HSL中間成分がfiniteで、hue reference（90/210/330度）、HSV saturation/value、HSL saturation/lightnessが期待値と一致し、RGB往復誤差は2e-6以内。ColorConversion CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 往復だけでは相互に補償する誤りを検出できないため、中間表現も独立referenceで検査する。
- **次に確認すべきこと:** なし。各成分の参照値は指定sampleから直接計算できる。

## 2026-10-09 — HSL hue wrap boundary

- **関連:** `tests/ArtifactCore/ColorConversionTest.cpp`、`ColorConversion::HSLToRGB`。
- **確認できた事実:** HSL hue 0°、360°、420°、-60°を検査し、360°は0°のred、420°はyellow、-60°はmagentaと一致する。専用ColorConversion CTestおよびstandalone全11 suiteが合格。±60°のRGB結果は計算上のfloat誤差があり、channelごと2e-6 toleranceで比較する。
- **価値または懸念:** 既存のHSV wrap coverageに加え、HSL inverse pathの一回転境界と隣接するpositive/negative hueを独立に保護する。
- **次に確認すべきこと:** なし。この追加は単一の隣接turn boundaryを対象とする。

## 2026-10-09 — Signed/HDR luminance standards

- **関連:** `tests/ArtifactCore/ColorLuminanceContractTest.cpp`、`ColorLuminance::calculate/toGrayscale`。
- **確認できた事実:** Rec.601、Rec.709、Rec.2020、Display P3、ACES AP1の各標準について、signed/HDR RGB 3 sampleで重み付きreferenceとluminanceが一致し、`toGrayscale`の全channelがluminanceと一致する。専用ColorLuminance CTestとstandalone全11 suiteが合格。
- **価値または懸念:** unit range外の線形/HDR RGBでも、luma計算とgrayscale化が値をclampせず標準の係数を適用する性質を保護する。
- **次に確認すべきこと:** なし。既存公開関数は入力channelのclampを行わずweighted sumを返す。

## 2026-10-09 — Simultaneous broadcast luma and gamut violations

- **関連:** `tests/ArtifactCore/ColorLuminanceContractTest.cpp`、`ColorLuminance::inspectBroadcastSafe`。
- **確認できた事実:** Rec.709のlower/upper sampleで、legal luminance rangeとchannel rangeを同時に外すケースを検査した。返却luminanceはweighted referenceと一致し、`luminanceViolation`、`gamutViolation`、`hasViolation()`が全てtrue。ColorLuminance CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 片方だけの違反ではなく、同時違反時にも独立flagと総合flagが両立することを下限・上限の両側で保護する。
- **次に確認すべきこと:** なし。検査関数は両条件を別計算してORする。

## 2026-10-09 — Unknown luminance standard in broadcast inspection

- **関連:** `tests/ArtifactCore/ColorLuminanceContractTest.cpp`、`ColorLuminance::inspectBroadcastSafe`。
- **確認できた事実:** 未知enum値を渡したbroadcast inspectionはRec.709 weighted luminanceを返す。sampleはluma legal range内だがblueがchannel maxを超えるため、luminance flag=false、gamut flag=true、aggregate flag=trueとなる。ColorLuminance CTestとstandalone全11 suiteが合格。
- **価値または懸念:** calculateのfallbackだけでなく、その値を使う上位inspection APIでもunknown enum時の判定が安定する。
- **次に確認すべきこと:** なし。inspectionは`calculate`の結果を使い、channel比較はstandardに依存しない。

## 2026-10-09 — Mixed legal RGB endpoints in broadcast inspection

- **関連:** `tests/ArtifactCore/ColorLuminanceContractTest.cpp`、`ColorLuminance::inspectBroadcastSafe`。
- **確認できた事実:** Rec.709の legal black/white と channel min/max の両端値から作る8通りのRGB corner全てで、luminanceとgamut violationがfalse、weighted luminanceがlegal range内になる。専用ColorLuminance CTestとstandalone全11 suiteが合格。
- **価値または懸念:** neutral black/white endpointだけでは見つけにくい、異なるchannelが異なる端点を取る合法な境界組合せを保護する。
- **次に確認すべきこと:** なし。Rec.709 weightsは非負で合計1のため、各channelが同一range内なら加重和も同範囲に入る。

## 2026-10-09 — Perceptual brightness signed/HDR properties

- **関連:** `tests/ArtifactCore/ColorLuminanceContractTest.cpp`、`ColorLuminance::calculatePerceptual`。
- **確認できた事実:** signed/HDR sampleの結果は平方重みreferenceと一致し、全channelの符号を反転しても不変、全channelを正の係数2.5でscaleすると出力も2.5倍になる。ColorLuminance CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 単一fixtureの式比較に加え、HSP近似関数の符号対称性と一次同次性をpropertyとして保護する。
- **次に確認すべきこと:** なし。関数は重み付き二乗和の平方根を返す。

## 2026-10-09 — TaggedColor premultiplication alpha bounds

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`TaggedColor::premultiplied`。
- **確認できた事実:** signed/HDR RGBに負値・0・部分alpha・1・1超えalphaを組み合わせると、乗算係数のみが[0,1]へclampされ、元のalpha値とsource RGBAは保持され、出力alpha modeはPremultipliedになる。ColorBridge CTestとstandalone全11 suiteが合格。
- **価値または懸念:** alpha範囲外metadataとscene-linear signed/HDR色の間で、暗黙のRGB clippingやsource mutationが起きないことを保護する。
- **次に確認すべきこと:** なし。`TaggedColor::premultiplied`はalpha factorだけをclampし、RGBへそのfactorを乗算する。

## 2026-10-09 — TaggedColor signed/HDR alpha-mode round trip

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`TaggedColor::premultiplied/straight`。
- **確認できた事実:** signed/HDR scene-linear RGBでalphaを1.1e-6超、0.125、0.5、1.0としたとき、premultiplied→straightでRGBが2e-6以内に復元し、alpha、transfer、known flag、primaries、alpha modeも保持される。source RGBAは変わらない。ColorBridge CTestとstandalone全11 suiteが合格。
- **価値または懸念:** alpha epsilon境界を超える値で、単純なdisplay-range sampleだけでなくsigned/HDR色とcolor interpretation metadataも可逆であることを保護する。
- **次に確認すべきこと:** なし。逆変換はclamp alphaで除算し、対象sampleはepsilonより大きい。

## 2026-10-09 — Display/video transfer curves preserve HDR headroom

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ColorTransferFunction::encode/decode`。
- **確認できた事実:** sRGB、Gamma 2.2/2.4/2.6、Rec.709、Rec.2020、HLGの7 curveについて、linear値1.0/1.25/2.0/4.0のencode/decode結果はfiniteで、元値からの誤差2e-4以内。ColorBridge CTestとstandalone全11 suiteが合格。PQは別の正規化peak基準のためfixture対象外。
- **価値または懸念:** 従来のdispatch roundtripはlinear 1.0までだったため、display/video curveのscene-linear HDR headroom保持を追加で保護する。
- **次に確認すべきこと:** なし。対象sampleはtransfer curveの現行公開式で往復可能なfinite範囲にある。

## 2026-10-09 — Dense PQ/HLG normalized transfer grid

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、PQ/HLG transfer encode/decode。
- **確認できた事実:** normalized linear [0,1]の1025 sampleでPQ/HLG encode/decodeがfiniteかつ単調である。roundtripのPQ最大誤差は約6.95e-5で、1e-4 toleranceの専用ColorBridge CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 数点のreference fixtureでは見逃すcurve内部の非単調や局所的な往復不良をdense deterministic gridで検出する。PQの高コード域はf32誤差がHLGより大きいため許容差を分ける必要がある。
- **次に確認すべきこと:** なし。このgridはPQ正規化範囲内をカバーする。

## 2026-10-09 — Signed scene-linear transfer toe coverage

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、ACEScct/Canon Log 2/Canon Log 3 encode/decode。
- **確認できた事実:** -1e-4、0、0.001のsampleは3 curve全てでfiniteかつ2e-6以内に往復する。探索時にCanon Log 2の-0.001はroundtrip差約9.54e-5を示したため、一般往復sampleから外し、既存negative-toe characterizationの対象と分けた。専用ColorBridge CTestとstandalone全11 suiteが合格。
- **価値または懸念:** signed scene-linear supportがあるtoe curveの負値往復を追加で保護する。同時にCanon Log 2のtoe付近では一様な往復精度を仮定できない。
- **次に確認すべきこと:** Canon Log 2 toe boundary付近の差を許容する仕様か、reference curveとの比較が必要かは未検証。ArtifactCore childは変更せず、本タスクではテストで既知範囲を固定した。

## 2026-10-09 — ACEScc monotonic direction

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ColorTransferFunction::linearToACEScc`。
- **確認できた事実:** ACEScc formula encodes larger positive linear values to smaller code values (e.g. 0.001→4.0 in the existing grid is non-increasing); ACEScc decode roundtrips HDR 1/2/4/16 within existing tolerance. ColorBridge CTest and standalone all 11 suites pass.
- **価値または懸念:** transfer curves do not all share an increasing encoding direction; a generic monotonic test must encode this per-curve behavior rather than assume increasing.
- **次に確認すべきこと:** None. Tests now branch only for ACEScc, with other listed curves remaining non-decreasing.

## 2026-10-09 — Dense ACEScc monotonic grid

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、ACEScc encode。
- **確認できた事実:** positive linear範囲0.001〜16の2049点でACEScc encodeはfiniteかつnon-increasing。専用ColorBridge CTestとstandalone全11 suiteが合格。
- **価値または懸念:** 少数sampleの順序比較に加え、ACESccの逆向きlog codeの内部区間をdenseに確認する。
- **次に確認すべきこと:** なし。0以下入力のblack pinはこの正値gridと別の既存black/toe testsで扱う。

## 2026-10-09 — Dense ACEScc decode code grid

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、ACEScc decode/encode。
- **確認できた事実:** code [0,1]の2049 sampleをdecodeすると全値finiteかつ非増加で、各decoded linearをencodeし直したcodeは2e-6以内に戻る。専用ColorBridge CTestとstandalone全11 suiteが合格。
- **価値または懸念:** encode側の正値HDR monotonicityだけでなく、normalized code domainからinverseを通る方向も検査し、互いに独立した密なpropertyを持つ。
- **次に確認すべきこと:** なし。調査対象はACESccのnormalized code範囲内。

## 2026-10-09 — Dense ACEScct decode code grid

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、ACEScct decode/encode。
- **確認できた事実:** normalized code [0,1]の2049点でACEScct decodeはfiniteかつnon-decreasingで、再encodeしたcodeは2e-6以内に戻る。専用ColorBridge CTestとstandalone全11 suiteが合格。
- **価値または懸念:** piecewise toeを含むinverse code domain全体をdenseに通し、toeの両側と各区間の逆変換を検査する。
- **次に確認すべきこと:** なし。gridはtoe codeをまたぎ、normalized code domain全域を含む。

## 2026-10-09 — Dense DaVinci Intermediate decode code grid

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、DaVinci Intermediate decode/encode。
- **確認できた事実:** normalized code [0,1]の2049 sampleでdecode結果がfiniteかつnon-decreasingで、再encodeしたcodeは2e-6以内で一致する。専用ColorBridge CTestとstandalone全11 suiteが合格。
- **価値または懸念:** log transferのnormalized decode domainをdenseに走査し、内部値と往復の破綻を検出する。
- **次に確認すべきこと:** なし。この範囲で公開式のdecode/encodeが互いに逆になる。

## 2026-10-09 — Dense Cineon decode code grid

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、Cineon decode/encode。
- **確認できた事実:** normalized code [0,1]の2049 sampleでdecodeはfiniteかつnon-decreasing。Cineon black code未満はnegative linearへdecodeされ、再encodeでblack codeへclampされる。black以上ではcode roundtrip誤差2e-6以内。ColorBridge CTestとstandalone全11 suiteが合格。
- **価値または懸念:** print-density transferのblack offsetがあるため、全normalized code域で単純roundtripを要求せず、negative decode/clamp境界を明示して保護する。
- **次に確認すべきこと:** なし。black codeは公開encode関数の0入力から算出する。

## 2026-10-09 — Light Layer: Spot reach は Cone Length 単一、Point/Area は影キャスタ非対応

- **関連:** `Artifact/src/Widgets/Dialog/CreateLightLayerDialog.cppm`、`Artifact/src/Layer/ArtifactLightLayer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実（実装後）:** Spot のリーチは `Cone Length` が唯一の authored value。`ArtifactLightLayer::getLayerPropertyGroups()` は Spot で `Light/Range` を生成せず、ギズモ（`ArtifactLightLayer.cppm` の Spot cone 描画）と shadow frustum far plane（`ArtifactCompositionRenderController.cppm` の `entry.source->coneLength().value`）がどちらも Cone Length を読む。作成ダイアログも Spot ページの Range 欄を Cone Length 欄へ置換し、`applyTo` は Spot に `setConeLength`、Point/Area に `setRange` を適用する形にした。影キャスタ選定は `castsShadows() && enabled() && (Directional | Spot)` のみで、Point と Area は照明のみ。
- **価値／懸念:** Range と Cone Length が併存していた頃は「どの値が実際の届く距離か」が UI 上 2 箇所にあり、shadow frustum とギズモがずれる余地があった。今回で作成導線は単一ソースに揃った。
- **次に確認すべきこと:** Area の `Cast Shadows` チェックは既定 ON のままだが、Area も影キャスタにならない。Inspector 側も含めて Point/Area ではこのスイッチを出さない／既定 OFF に揃えるべきかは未判断（ユーザー確認事項）。

## 2026-10-09 — Dense S-Log3 decode code grid

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、Sony S-Log3 decode/encode。
- **確認できた事実:** normalized code [0,1]の2049 sampleでdecodeはfiniteかつnon-decreasing。black code未満はnegative linearへdecodeされ、再encodeでblack codeへclampされる。black以上のcode roundtrip誤差は2e-6以内。ColorBridge CTestとstandalone全11 suiteが合格。
- **価値または懸念:** S-Log3 toeのnormalized-code側を密に通し、negative toeとclamped encoderの関係を保護する。
- **次に確認すべきこと:** なし。black codeは公開encode関数のzero入力から取得する。

## 2026-10-09 — Canon Log 3 decode の低code境界ジャンプ

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、Canon Log 3 decode/encode。
- **確認できた事実:** normalized code [0,1]の2049 sampleはfinite。low/middle/high各decode区間内ではnon-decreasingで、low/high log区間はencode/decode roundtrip誤差2e-6以内。low code `0.04076162` の直前から境界値へ移るとdecode linear値が下降する。既存の breakpoint test はencode側でlow境界に約`0.0062386`の上向きジャンプを確認している。standalone全11 suiteは合格。
- **価値または懸念:** 全域を単調かつ相互逆と誤認せず、区間内の性質と境界の既存挙動をテストで分けて可視化できる。decode側の下降が意図された仕様か不具合かは未検証。
- **次に確認すべきこと:** Canon Log 3 の採用規格／参照式と照らし、境界挙動を保持するか修正するかを判断する。

## 2026-10-09 — gamut変換の合成経路とDaVinci Wide Gamut行列

- **関連:** `tests/ArtifactCore/ColorGamutConversionTest.cpp`、`ArtifactCore/include/Color/ColorGamutConversion.ixx`。
- **確認できた事実:** 全gamut組み合わせの試行で、DaVinci Wide Gamutから同gamutへ別gamutを経由する経路が直接変換と一致しない例を確認。ファイルにあるforward行列とinverse行列の積も恒等行列にならず、例えば中間XYZを使った戻りで値が大きくずれる。ArtifactCoreは子リポジトリなので今回変更せず、追加したtransitivity propertyはDaVinci Wide Gamutと明示XYZ_D60を除く9参照gamutで検証する。
- **価値または懸念:** gamut間の代表値・往復試験だけでは、3段経路の不整合や基底行列の不一致を見逃す。DaVinci Wide Gamutの期待行列値と白色点／経由gamutの意味を一次資料と照合する必要がある。
- **次に確認すべきこと:** ユーザーが子リポジトリ修正を依頼した場合、行列出典とXYZ_D60の適応契約を先に確認し、既存reference testと併せて最小修正する。

## 2026-10-09 — Rec.709 OETF decode toe の小さな下降

- **関連:** `tests/ArtifactCore/ColorBridgeTest.cpp`、`ColorTransferFunction::rec709ToLinear` / `rec2020ToLinear`。
- **確認できた事実:** normalized code [0,1]の4097点を走査したところ、Rec.709 decodeはcode 0.081で線形枝からpower枝へ切り替わる時に小さく下降する。Rec.2020の対応閾値では隣接float値が連続範囲にある。各分岐内のdecodeはnon-decreasingで、encode/decode往復誤差は4e-4以内。独立suite全11件合格。
- **価値または懸念:** 代表値の往復確認のみではtoeのごく小さい不連続を見逃す。Rec.709式とbreakpointの値が意図的な近似かは未検証。
- **次に確認すべきこと:** 仕様値に照らしてRec.709 decode閾値とpower係数の整合を確認する。

## 2026-10-09 — ACESlog enum の未実装dispatch

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`tests/ArtifactCore/ColorBridgeTest.cpp`。
- **確認できた事実:** `TransferFunction::ACESlog` はenumに宣言されているが、`ColorTransferFunction::encode` / `decode` のswitchにcaseがないため、default identity fallbackになる。characterization testで負値、0、18% gray、1、1.5の現動作を固定した。
- **価値または懸念:** 宣言済みの名前だけを見てACESlog curveが利用可能と誤認する可能性がある。規格式未確認のまま実装を推測するのは危険。
- **次に確認すべきこと:** ACESlogが意図的な互換enumか、特定のACES2065-1 log encodingを追加すべきか一次仕様とAPI利用箇所を照合する。現段階では未検証。

## 2026-10-09 — PQ transfer independent reference precision

- **関連:** `tests/ArtifactCore/ColorPipelineStandaloneTest.cpp`, `ColorTransferFunction::linearToPQ` / `pqToLinear`。
- **確認できた事実:** PQのm1/m2/c1/c2/c3定数による独立double参照で、負値・黒・低輝度・18% gray・基準白・1を超えるHDR値までencode/decodeを比較できる。float実装はlinear 100で往復誤差が約0.0065となり、relative tolerance 1e-4ならサンプル集合に通る。テスト構築中、参照式のm2係数を誤記すると非常に大きな不一致になり、定数は仕様の有理数表記に沿って明記した。
- **価値または懸念:** PQの低値とHDR側を独立実装で確認できる。ここでは代表値だけを使っており、コード値全域の単調性や10,000-nit近傍は未検証。
- **次に確認すべきこと:** 必要に応じてPQ code-domainの密な単調性と上端飽和近傍のcharacterizationを追加する。

## 2026-10-09 — PQ transfer 10-bit domain monotonicity

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`, `ColorTransferFunction::encode` / `decode` for `Rec2084_PQ`。
- **確認できた事実:** 10-bit normalized code domainの全1,024点を走査し、encode/decode出力がすべてfinite、各系列がnon-decreasing、両系列の終端が1.0である独立テストを追加・実行した。
- **価値または懸念:** 既存の各コード値とST 2084参照式の比較に、有限性・単調性・端点という別の不変条件が加わった。連続する任意float全域やPQの1.0超のdecodeは対象外。
- **次に確認すべきこと:** 必要なら10-bit domainを超えるencoded値の現行挙動をcharacterizeし、規格範囲との境界を記録する。

## 2026-10-09 — PQ decode denominator singularity outside normalized code range

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, `pqToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** ST 2084参照式の分母 `c2 - c3 * code^(1/m2)` が0になるコードは約1.00496で、正規化範囲[0,1]より外にある。decode(1)は約1.0を返す一方、float入力で特異点付近およびそれ以上は実装の `den <= 0` ガードにより0を返す。独立テストで特異点前後を固定した。
- **価値または懸念:** 無制限のscene-linear/HDR値をPQ decodeへ渡した場合の現行結果を明示できる。1より上のPQコード値をどう扱うべきかはST 2084の運用契約とAPIの想定範囲を確認しておらず未検証。
- **次に確認すべきこと:** PQ decode APIの許容入力範囲を呼び出し側と照合し、[0,1]外のclamp／保護方針が必要か判断する。
## 2026-10-09 — HLG scene-linear round-trip over toe and HDR range

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`, HLG OETF/EOTF。
- **確認できた事実:** HLG encode→decodeの独立往復テストを追加し、zero、低輝度、1/12 toeの前後、18% gray、基準値1、HDR値16までfiniteな結果と相対許容差内の復元を確認した。転送関数standalone suiteは合格。
- **価値または懸念:** 既存の10-bit全コード参照試験と折れ点参照に、linear-light側での往復確認が加わった。ディスプレイsystem gammaやpeak luminanceによるHLG OOTFはこのtransfer関数の対象外。
- **次に確認すべきこと:** 必要ならHLG OOTF/ display renderingが別の契約として実装されているかを調査し、transfer関数テストへ混ぜない。
## 2026-10-09 — HLG out-of-range code behavior

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, `hlgToLinear` / `linearToHLG`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 独立テストでHLG decodeの負値は0へクランプされ、1を超えるcodeはEOTFの指数式を外挿して1を超えるlinear値を返すことを確認した。encodeの負linearも0へクランプする。`-1`、`-0.25`、`0`、`0.5`、`1`、`1.25`を参照式と照合し、suiteは合格。
- **価値または懸念:** HLG transfer関数がnormalized [0,1]外で持つ現行契約を明示できる。HLGのsystem gamma/OOTFや実表示輝度への意味付けはここでは扱っていない。
- **次に確認すべきこと:** 必要なら呼び出し側でHLG normalized-code範囲を保証しているか確認し、クランプをこの低レベル曲線に追加する必要性を判断する。
## 2026-10-09 — S-Log3 encode/decode round-trip around toe and HDR

- **関連:** `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`, S-Log3 OETF/EOTF。
- **確認できた事実:** encode→decode往復をzero、低値、線形toeの隣接double値、18% gray、基準白、HDR 16まで追加し、finite出力と許容差内の復元を確認した。既存suiteには全10-bit code参照比較とencode toe境界比較があり、このケースで往復軸を補った。transfer suiteは合格。
- **価値または懸念:** 固定normalized-code参照のほか、linear scene値を一連の処理に通した時の整合を保護する。ここでの負値はS-Log3 encodeでblack-code側へclampされるため、signed scene値の保持は保証しない。
- **次に確認すべきこと:** 必要ならencode/decodeのclampとblack-code挙動を、負値を含む別のcharacterization testで明示する。
## 2026-10-09 — Canon Log 2 round-trip in negative toe and HDR regions

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Canon Log 2 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** encode→decode往復を負側log toeの領域、非負の暗部からHDR 1000まで追加し、finite出力と相対許容差内の復元を確認した。独立transfer suiteは合格。既存テストがcharacterizeする負側toe境界の不連続点そのものは往復サンプルから除外した。
- **価値または懸念:** 負のscene-linear域が全て同じように扱われるのではなく、toe境界の外側区間では往復可能なことを確認できた。境界ジャンプの妥当性は未検証。
- **次に確認すべきこと:** Canon Log 2の公開仕様と境界式を照合し、現行characterizationを維持するか将来修正するか判断する。
## 2026-10-09 — DaVinci Intermediate black sentinel and positive HDR round-trip

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, DaVinci Intermediate OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 正のscene-linear 1e-6から1000までのencode→decode往復を追加し、finite出力と相対許容差内の復元を確認した。またencode(0)は実装上のblack sentinel 0だがdecode(0)は0へ戻らないことを明示した。独立suiteは合格。
- **価値または懸念:** ゼロ入力の特例と、正の実用域の逆変換を混同せず検証できる。DaVinci Intermediate仕様でblack sentinelのdecode契約をどう定めるかは未検証。
- **次に確認すべきこと:** 必要ならBlackmagic公開仕様のdomain/rangeと照らし、zero codeを特別扱いするAPI契約が必要か検討する。
## 2026-10-09 — Canon Log 3 round-trip by continuous piecewise region

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Canon Log 3 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Canon Log 3のlow log toe領域、middle affine領域、high log領域をまたぐlinear sampleでencode→decode往復を追加した。low toeの既知不連続点そのものは避け、middle/high境界の直前・一致・直後も含めてfinite出力と復元を確認した。suiteは合格。
- **価値または懸念:** 既存のコード値参照試験とlow toe discontinuity characterizationに、各連続区間の逆変換整合が加わった。low toe境界ジャンプの規格上の意図は未検証。
- **次に確認すべきこと:** Canon Log 3係数と負値toeの規格資料を照合し、既知ジャンプを保持すべきか判断する。
## 2026-10-09 — Cineon negative-domain asymmetry and HDR reference

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Cineon OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 独立式と照合するnegative/positive linear encodeとnegative/over-range code decodeを追加した。encodeは負linearをclampし、black code `95/1023`を返す一方、decode(0)はnegative linearを返し、1.25 codeもfiniteな値へ外挿される。追加suiteは合格。
- **価値または懸念:** OETF/EOTFの負値契約が対称ではないことを、代表的な10-bit域テストとは別に確認できる。Cineon normalized inputの運用範囲外で外挿を許容するかは未検証。
- **次に確認すべきこと:** 呼び出し側がCineon codeを[0,1]へ制限しているか、negative linear decodeを保持する用途があるか確認する。
## 2026-10-09 — sRGB negative clamp and positive HDR round-trip

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, sRGB OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** sRGB encode/decodeが負入力を0へclampすること、decode(1.25)が有限かつ1超になること、および0からlinear 100までの正値サンプルでencode→decodeが許容差内に戻ることを追加検証した。breakpoint直近は既存の別テストでcharacterizeしている。suiteは合格。
- **価値または懸念:** normalized範囲外のclamp/extrapolationとscene-linear HDRの往復を、折れ点の微小段差試験と切り分けて固定できる。
- **次に確認すべきこと:** 必要なら全transfer curveに共通する有限値入力／NaN/Inf挙動の一貫性を別の契約として確認する。
## 2026-10-09 — Rec.2020 transfer negative clamp and HDR round-trip

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Rec.2020 10-bit OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Rec.2020 negative linear/code inputs clamp to zero; decode(1.25) extrapolates to a finite value above 1. Positive scene-linear values from zero through 100 round-trip within tolerance and encode agrees with an independent double OETF reference. Suite passed; published breakpoint behavior remains covered by a dedicated existing test.
- **価値または懸念:** normalized domain, clamp behavior, and scene-linear HDR round-trip are covered independently from the breakpoint test. Exact BT.2020 branch discontinuity policy remains outside this characterization.
- **次に確認すべきこと:** 必要に応じてfloat adjacent-value monotonicityをbreakpoint周辺で別途固定し、現行係数による微小な段差を記録する。
## 2026-10-09 — simple gamma negative clamp and HDR power reference

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Gamma 2.2/2.4/2.6、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 3つのpower gammaすべてでnegative encode/decodeがzeroへclampされること、および0.01〜100の値でOETF/EOTFが独立doubleのpower式と一致することを確認した。既存の全10-bit参照とlinear-light round-trip gridに加える形でsuiteは合格。
- **価値または懸念:** normalized範囲外の負値とHDR power extrapolationを個別比較できる。大きなHDR入力でのfloat精度は入力値に応じたrelative toleranceを用いる。
- **次に確認すべきこと:** 必要なら極端なfinite値やNaN/Infの共通契約を、各transfer curve別に確認する。
## 2026-10-09 — Rec.709 negative clamp and HDR/out-of-range reference

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Rec.709 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Negative linear/code inputs clamp to zero. Positive linear values through 100 and encoded codes through 1.25 match independent BT.709 piecewise equations within float tolerance. Existing tests separately characterize the known OETF/EOTF breakpoint mismatch; this new sample set avoids those exact branch thresholds. Suite passed.
- **価値または懸念:** Range clamp and HDR extrapolation are covered separately from the known breakpoint discontinuity. Whether over-range Rec.709 code extrapolation is expected by all callers remains unverified.
- **次に確認すべきこと:** 呼び出し側がRec.709 codeを[0,1]へ制限しているか確認し、外挿が意図された共通curve API契約か判断する。
## 2026-10-09 — ACEScct signed toe and HDR round-trip

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, ACEScct OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** ACEScct encode→decode往復をnegative scene values、zero、toe、positive dark values、18% gray、HDR 100まで追加し、finiteなcodeと許容差内のlinear復元を確認した。独立transfer suiteは合格。
- **価値または懸念:** ACEScctのlinear toeが負値を含む範囲で逆変換可能であることを既存の10-bit code/reference・breakpoint試験に加えて固定した。
- **次に確認すべきこと:** 必要ならACESccのzero sentinelのような特殊値が他のlog curveにもあるか、encode/decode対で横断点検する。
## 2026-10-09 — ACEScc black sentinel and positive HDR round-trip

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, ACEScc OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Negative scene values encode to the same black sentinel as zero. Decoding that sentinel returns approximately 131072, as already shown by the reference test. Positive scene values from 1e-6 to 1000 round-trip within relative tolerance. The standalone transfer suite passed.
- **価値または懸念:** Black sentinel's non-invertibility is clearly separated from positive-domain round-trip, including HDR. Callers must not assume encode(0) is an ordinary inverse-mapped code.
- **次に確認すべきこと:** ACES ST 2065-4のnegative/zero encoding contractと sentinel constant precisionを照合する。
## 2026-10-09 — DaVinci Intermediate decode extrapolation below and above nominal codes

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, DaVinci Intermediate EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Independent exponential reference checks finite decode output for codes -2 through 1.5. Negative code -2 yields a tiny positive value, not a negative result, because the inverse is an exponential; code 1.5 decodes above 1. The first test expectation incorrectly assumed a negative result and was corrected from observed output. Standalone suite then passed.
- **価値または懸念:** 範囲外コードの符号・有限性を実測し、式の形から誤って負値を予測しないようにした。極端な負codeではunderflow zero、極端な正codeではoverflowの可能性があり、その限界は未検証。
- **次に確認すべきこと:** 必要ならfloat有限範囲の端点付近でunderflow/overflowをcharacterizeする。
## 2026-10-09 — Canon Log 2 intermediate negative-domain inverse check

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Canon Log 2 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 既存round-trip testで疎だったnegative linear interval from just above the toe through zeroを追加した。OETFは独立piecewise referenceと一致し、encoded outputをEOTFに通した結果もreference compositionと許容差内で一致した。個別ケースとtransfer suiteの両方が合格。
- **価値または懸念:** Toe外側に加え、負値境界からzeroまでの区間の現行数式を確認できた。既存のtoeちょうどのOETF jump characterizationは引き続き別に保持する。
- **次に確認すべきこと:** 必要ならCanon Log 2 EOTFのnegative normalized-code domainを追加し、0未満codeの外挿をfinite範囲で確認する。
## 2026-10-09 — S-Log3 normalized-code out-of-range behavior

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, S-Log3 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Decode for codes -1, -0.1, black code, midrange, 1, and 1.25 matches the independent inverse equation and remains finite. Negative normalized code extrapolates to negative scene-linear values; encode of negative linear input clamps to the 95/1023 black code. The standalone suite passed.
- **価値または懸念:** S-Log3 code range behavior outside [0,1] is now explicitly separated from its 10-bit in-range comparison. Caller-side range guarantees and the intended use of negative code values remain unverified.
- **次に確認すべきこと:** 必要ならAPI callerがS-Log3 normalized codeをclampする責務を担うかを確認する。
## 2026-10-09 — Canon Log 3 out-of-range encode and decode references

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Canon Log 3 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Negative and >1 code values, plus negative linear values and HDR 100, were compared with the independent piecewise equations; outputs remained finite and the transfer suite passed. Existing low-toe boundary discontinuity characterization remains separate.
- **価値または懸念:** Canon Log 3's extrapolation and negative-value branch behavior are covered beyond its normalized 10-bit domain. Range guarantees from callers remain unverified.
- **次に確認すべきこと:** 必要ならCanon Log 3入力値域の規格・API境界を確認し、外挿を許す責務がどこにあるか判断する。
## 2026-10-09 — Rec.709 adjacent-float branch jumps

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Rec.709 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Float values immediately below/at/above the implementation's float breakpoints were checked with references using the same branch threshold and double-precision branch equations. OETF rises by about 0.000248 at linear 0.018; EOTF drops by about 0.000055 at encoded 0.081, with local recovery on the next float. The test passes with the suite.
- **価値または懸念:** Reference tests must preserve the implementation's float comparison boundary; using the mathematically rounded double threshold routes the exact float breakpoint into a different reference branch and gives a misleading mismatch.
- **次に確認すべきこと:** 仕様で採用する正確なRec.709 breakpoint pairを一次資料と照合し、修正の依頼があれば枝条件をまとめて直す。
## 2026-10-09 — simple gamma maximum-float encode/decode range

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Gamma 2.2/2.4/2.6、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** For `float::max`, encode remains finite and matches the double power reference, while decode returns positive infinity for all three gamma exponents. The standalone suite passed.
- **価値または懸念:** Finite input does not imply finite gamma decode output; downstream callers using extreme linear/code values need range discipline. This test records current float overflow rather than defining a saturation policy.
- **次に確認すべきこと:** 必要ならピクセル処理呼び出し側でtransfer前後のfinite/range contractを確認する。
## 2026-10-09 — HLG large-finite input overflow points

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, HLG OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** The initial assumption that HLG encode(max finite float) stays finite was disproven: the implementation computes `12.0f * linear` before `log`, so max finite input returns +Inf. At max/16 the encode result remains finite and matches a double reference. Decode(16) remains finite while decode(17) overflows to +Inf. The corrected characterization and full transfer suite pass.
- **価値または懸念:** A finite input can produce a non-finite result from an intermediate multiply or exponential; this is distinct from intended HLG code-range extrapolation. No saturation policy is added.
- **次に確認すべきこと:** 呼び出し側がHLG curveに渡すlinear値を有限float内部積範囲に抑えているか必要に応じて確認する。
## 2026-10-09 — PQ extreme finite input behavior

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, PQ OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Encoding `float::max` remains finite and approaches `(c2/c3)^m2`, greater than 1. Decoding `float::max` returns 0 because the computed denominator is non-positive and the implementation's guard fires. The targeted test and full standalone transfer suite pass.
- **価値または懸念:** At extreme finite values the PQ encoder approaches its equation's asymptote, while decoder's denominator guard collapses out-of-domain values to black. This behavior is outside normalized PQ code range; no clamp or saturation change was introduced.
- **次に確認すべきこと:** PQ decode callers should be checked for normalized [0,1] input assumptions if extreme code values can reach this API.
## 2026-10-09 — Cineon extreme finite input limits

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Cineon OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Encoding max finite float remains finite and matches a double reference; negative max encodes to the same black code as zero. Decoding max finite code overflows to +Inf. Decoding negative max underflows the power term to zero and returns the finite negative black-offset constant. Targeted and full transfer suites pass.
- **価値または懸念:** Extreme finite normalized inputs can overflow or underflow Cineon decoding, while encode has a distinct negative clamp. This records current behavior without imposing saturation or input validation.
- **次に確認すべきこと:** 必要ならCineon decodeの呼び出し側でnormalized code rangeを保証するか確認する。
## 2026-10-09 — Canon Log 2 signed overflow for extreme finite values

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Canon Log 2 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** `max_float/256` encode remains finite and agrees with the independent double equation. At positive/negative max float, encode produces signed infinity from float intermediate arithmetic; decode at positive/negative max code also produces signed infinity. Targeted test and full transfer suite pass.
- **価値または懸念:** Extreme finite values do not guarantee finite log-curve outputs; both sign branches overflow in their scale/exponent arithmetic. No saturation or domain clamp was introduced.
- **次に確認すべきこと:** 必要ならCanon Log 2 callersが想定scene/code rangeをfloat安全域に制限しているか確認する。
## 2026-10-09 — Canon Log 3 signed overflow at extreme finite values

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, Canon Log 3 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** `max_float/32` encode remains finite and matches the double reference. At ±max float, encode and decode both produce signed infinities because the log input product or exponent overflows float. Targeted test and full transfer suite pass.
- **価値または懸念:** Negative scene-linear support in the normal toe region does not imply extreme finite safety; no saturation behavior is applied at float limits.
- **次に確認すべきこと:** 必要ならCanon Log 3 callersの値域前提を確認し、transfer API前後でfinite validationが必要か判断する。
## 2026-10-09 — non-finite input behavior across S-Log3 and Canon Log curves

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, S-Log3/Canon Log 2/Canon Log 3、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** NaN propagates through encode/decode for all three curves. Positive infinity maps to positive infinity; negative infinity decode maps to negative infinity. Negative infinity encode differs: S-Log3 clamps to its 95/1023 black code, while Canon Log 2 and Canon Log 3 return negative infinity. Targeted and full transfer suite pass.
- **価値または懸念:** The log curves do not share one non-finite policy; callers cannot assume a uniform clamp. Tests record current formula behavior only.
- **次に確認すべきこと:** 必要ならtransfer API callersがNaN/Infを事前に検査する責務を持つか確認する。
## 2026-10-09 — DaVinci Intermediate non-finite input behavior

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, DaVinci Intermediate OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** NaN propagates through encode/decode. Positive infinity returns positive infinity in both directions. Negative infinity encode clamps to 0, and negative infinity decode underflows to 0. Targeted test and full transfer suite pass.
- **価値または懸念:** Log encode and exponential decode have different non-finite handling; this is current behavior characterization, not a shared validation contract.
- **次に確認すべきこと:** 必要なら呼び出し側がtransfer前にfiniteを検査するか確認する。
## 2026-10-09 — non-finite behavior across clamped display transfer curves

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, sRGB/Gamma 2.2/2.4/2.6/Rec.709/Rec.2020、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** All six curves propagate NaN through encode/decode, propagate positive infinity to positive infinity, and clamp negative infinity to zero in both directions. Targeted characterization and full transfer suite pass.
- **価値または懸念:** Despite sharing negative clamp logic, NaN remains unfiltered because `std::max(NaN, 0)` preserves the NaN operand under these operations. This is current low-level behavior, not a broad application safety guarantee.
- **次に確認すべきこと:** 必要ならpixel conversion境界が非有限channelを拒否・sanitizeする責務を担うか確認する。
## 2026-10-09 — ACEScc and ACEScct extreme finite range behavior

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, ACEScc/ACEScct OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** ACEScc encode(+max float) remains finite and negative max maps to the existing zero sentinel. Decode(+max) underflows its power term and returns a small finite negative offset; decode(-max) overflows positive. ACEScct encode(+max) remains finite, encode(-max) overflows negative, decode(+max) overflows positive, while decode(-max) stays finite negative because it uses the linear toe. Targeted tests and full transfer suite pass.
- **価値または懸念:** ACEScc/ACEScct have materially different extreme-range behavior despite both being ACES log curves; finite extreme input may produce signed infinity or sentinel behavior depending on direction and branch.
- **次に確認すべきこと:** 必要ならACEScc/ACEScct caller-side expected scene/code rangeを明確にし、float extremesが実運用で到達可能か確認する。
## 2026-10-09 — S-Log3 extreme finite input branches

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, S-Log3 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** `max_float/64` encode remains finite and matches the double logarithmic reference. Positive max encode overflows to +Inf; negative max encode clamps to the 95/1023 black code. Decode at positive/negative max returns signed infinity from normalized-code multiplication and the selected exponential/affine branch. Targeted and full transfer suites pass.
- **価値または懸念:** S-Log3's negative clamp on encode differs from its signed decode extrapolation; extreme finite inputs can still overflow in either direction.
- **次に確認すべきこと:** 必要ならS-Log3 caller-side scene/code range guaranteesを確認する。
## 2026-10-09 — S-Log3 subnormal input quantization at black code

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, S-Log3 OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Encode of zero, the smallest positive subnormal, smallest positive normal, and next representable value above zero all produce exactly the 95/1023 black code. Decode immediately below that code is negative, at the code is approximately zero, and immediately above is positive; all three match the independent EOTF reference. Targeted and full transfer suites pass.
- **価値または懸念:** Very small positive scene values are indistinguishable from black in the float implementation's normalized S-Log3 code. The decode branch preserves signed values around black rather than clamping them.
- **次に確認すべきこと:** 必要ならlinear pixelの低値domainでこのblack-code量子化が用途上想定されているか確認する。
## 2026-10-09 — ACEScct adjacent-float decode toe drop

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, `acescctToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Adjacent float values around encoded breakpoint `0.155251141552511f` match the independent reference equations, but decode at the float breakpoint is about 0.93e-9 greater than decode at the next float. OETF samples around linear breakpoint remain non-decreasing. The EOTF implementation compares a float input against a double literal, so the float breakpoint rounds slightly above the threshold and selects the power branch. Test characterizes the drop; standalone suite passes.
- **価値または懸念:** The decode curve has a tiny local downward step at the toe even though each branch matches its reference. This is caused by float/double threshold representation and remains unmodified.
- **次に確認すべきこと:** ACEScct breakpoint precision and intended branch convention should be checked against the adopted ACEScct spec if a production fix is requested.
## 2026-10-09 — ACEScc black-sentinel decode float plateau

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, `acesccToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** The float immediately below, sentinel value, and float immediately above ACEScc black code `-0.3584474886f` all decode to exactly 131072. Each result matches the independent double equation within its tolerance. Targeted and full transfer suite pass.
- **価値または懸念:** The decode's intermediate float exponent calculation collapses adjacent code values into a local plateau at the sentinel, while the sentinel itself is grossly non-invertible to black. No formula change was made.
- **次に確認すべきこと:** ACEScc zero sentinel constant/precision and negative code handling should be checked against the adopted SMPTE ACEScc contract if a fix is requested.
## 2026-10-09 — GPU粒子キャプチャの未初期化Z値

- **関連:** `tools/Particle2DPlayground/main.cpp` の `captureGpuParticle` / `ParticleLayerWindow` GPU診断。
- **確認できた事実:** `ArtifactCore::ParticleVertex` は `px/py/pz` や色などの一部フィールドにデフォルト初期値がない。GPUキャプチャで `ParticleVertex particle;` を使って `pz` を設定しない場合、実行ごとに値が不定になり、今回は射影後Zが負のクリップ範囲外となって粒子が見えなかった。`ParticleVertex{}` と明示 `pz=0` にすると、production shader／projectionのままGPU readbackで44,324粒子ピクセルを確認した。
- **判断:** この再現ではParticleRenderer本体の射影修正は不要。キャプチャfixtureの未初期化値が原因だった。
- **次に確認すべきこと:** 2D粒子の他のキャプチャfixtureでも全入力を値初期化する。3D粒子は必要なZ座標を明示し、乱数初期状態に依存しないGPU画像検査にする。

## 2026-10-09 — Cineon adjacent-float black-code decode plateau

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, `cineonToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** Float code immediately below, at, and above `95/1023` all decode to the same value, approximately `-9.4149e-10`, and each agrees with the independent equation. The float multiply by 1023 collapses adjacent codes at this point. Targeted and full transfer suites pass.
- **価値または懸念:** Cineon nominal black has a tiny negative decode bias and a one-ULP input plateau. This is much smaller than existing color tolerances but gives an exact low-end characterization.
- **次に確認すべきこと:** 必要ならCineon black-code offset and normalization constantsを採用仕様と照合する。

## 2026-10-09 — Extended-code ordering sweeps for log curves

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, S-Log3/Canon Log 2/Canon Log 3/Cineon/ACEScc/ACEScct、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 6曲線をcode値 -2〜2 の401点で走査し、各実装値が独立参照式と許容差内で一致すること、隣接点の増減方向が参照式と矛盾しないことを確認した。float丸めによる等値plateauは許容する。厳密な増加を要求する初回版はCineonの局所plateauで失敗した。修正版を含むstandalone suiteは成功。
- **価値または懸念:** 拡張コード値でも各曲線の参照式との一致と局所的な順序関係を一括確認できる。これは401点の標本検査であり、連続領域全体の単調性を証明するものではない。参照式自体が持つ小さな逆行もそのまま比較対象となる。
- **次に確認すべきこと:** 値域や刻み幅を増やす場合は、参照式の固有の折れ・plateauとfloat丸めを区別したまま追加する。

## 2026-10-09 — Cross-transfer conversions across curve pairs

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 13種類のtransfer curveからなる異なる169組について、正のlinear HDR値9点をsource encode→source decode→destination encodeし、float実装値を独立double式の合成結果と比較した。standalone transfer suiteは成功。
- **価値または懸念:** 曲線単体のOETF/EOTF検査に加え、異なる符号化間を移す基本経路の組み合わせを確認できる。サンプルは正値1e-5〜4に限定し、負値clamp、ACEScc/DaVinci zero sentinel、全定義域の色管理意味論までは検証しない。
- **次に確認すべきこと:** 必要ならnegative toe、zero sentinel、out-of-range入力を含むペア別の期待動作を独立に定義して追加する。

## 2026-10-09 — Decode ordering across the full 10-bit code grid

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, 16 transfer curves、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 16曲線それぞれについてnormalized code 0〜1023を全走査し、decode結果とdouble参照がfiniteであること、隣接codeごとの実装の増減方向が参照EOTFの方向と逆転しないことを確認した。Canon Log 3など参照式にある局所下降を許容し、float丸めのplateauも許容する。standalone suiteは成功。
- **価値または懸念:** 既知の曲線固有の非単調区間を隠さず、全10-bit格子で追加の方向反転がないかを検出できる。HLG/Canon Log 3のdouble参照との数値差は別の曲線別精度テストの責務とし、この走査は値一致許容差の判定を重複させない。
- **次に確認すべきこと:** 10-bit以外のbit depthを扱うAPIが加わった場合は、そのコード格子も個別に走査する。

## 2026-10-09 — Implemented transfer dispatch coverage

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, `tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** LinearとACESlog/unknown identity fallbackを除く15個の実装済みtransferについて、代表4点（0.01、0.18、0.5、4.0）でencode/decode双方がfiniteとなり、Linear以外はidentity応答でないことを検査した。standalone transfer suiteは成功。
- **価値または懸念:** switchのcase漏れや片方向のidentity fallback混入を基本サンプルで捉えられる。ただし網羅対象enumはテスト内の明示配列なので、enum追加時の自動追随ではない。
- **次に確認すべきこと:** enum追加時に配列と参照式を更新する。Canon Log 2/3のlinear 0.01近傍は既知のtoe/low-code非可逆挙動に入り得るため、全曲線共通の往復assertは避ける。

## 2026-10-09 — Dense positive scene grid for signed-log OETFs

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, S-Log3/Canon Log 2/Canon Log 3/Cineon/ACEScc/DaVinci Intermediate、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 6曲線をlinear 1e-5〜1e4の1001点logarithmic gridで走査し、OETF結果と独立double参照式の差が許容範囲内であること、隣接点の方向が参照式と逆転しないこと、全出力がfiniteであることを確認した。standalone transfer suiteは成功。
- **価値または懸念:** 代表値や等間隔gridより広いscene/HDR範囲を対数密度で確認できる。linear zero・negative toeと非有限入力は既存の専用ケースが担当し、このgridには含めない。
- **次に確認すべきこと:** さらに広い値域でoverflow境界を変える場合は、既存の極値専用テストを更新し、finite性を要求する範囲を明示する。

## 2026-10-09 — Full 16-bit grid for display transfer curves

- **関連:** sRGB、Gamma 2.2/2.4/2.6、Rec.709、Rec.2020、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 6曲線のencode/decodeをnormalized 16-bit code 0〜65535全域で独立参照式と比較し、finite性も確認した。float実装の隣接順序はOETFでは非減少、EOTFではdouble参照式と同方向であることを検査し、standalone suiteは成功。Rec.2020 EOTFはcode 5309近辺で参照式にもある局所下降を示した。Rec.2020の参照差は3e-7を超える点があったため、その曲線のみ5e-7を許容した。
- **価値または懸念:** 10-bit格子の64倍の点数で、表示系曲線の精度・順序を確認できる。局所下降は修正対象とせず、参照式に対する挙動として記録した。
- **次に確認すべきこと:** Rec.2020 EOTFしきい値の規範上のbranch定義を別途確認する場合は、意図したしきい値・係数・float精度を同時に比較する。

## 2026-10-09 — Full 16-bit grids for PQ and HLG

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`, PQ/HLG OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** PQ/HLG decodeを16-bit normalized code全域で参照式と比較し、実装と参照の隣接方向が一致することを確認した。PQ encodeとHLG encodeも16-bit linear gridで走査。standalone suiteは成功。float実装のPQ OETFは最大参照差約1.36e-5、局所下降2793回（最大約1.67e-5）。PQ EOTF最大差は約5.47e-5（code 65425）、HLG EOTF最大差は約5.47e-5（code 65425）。HLG OETF格子は参照許容内で単調だった。
- **価値または懸念:** HDR曲線の高code側ではfloat係数・中間演算により参照差が増え、PQ OETFには参照式にない局所下降も現れる。テストは現行挙動を数値上限で特徴づけ、式を変更していない。
- **次に確認すべきこと:** PQ OETFの高域下降が要求精度上許容されるか評価する場合は、仕様精度・GPU/CPU実装間の目標・演算精度を決めたうえで別途修正対象を判断する。

## 2026-10-09 — Full 16-bit encoded-code round trips for log curves

- **関連:** Cineon、DaVinci Intermediate、ACEScct、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 各曲線のnormalized 16-bit code 0〜65535をdecode→encodeし、Cineonはblack floor（16-bit換算code 6089）または元codeに丸めて戻ること、DaVinci IntermediateとACEScctは元codeへ戻ることを確認した。transfer standalone suiteは成功。
- **価値または懸念:** 10-bitで確認済みだった量子化往復契約を、16-bit格子の各codeに拡張した。Cineonのblack floorより低い入力codeは同じfloorへ写るため、元codeへの完全な往復は期待しない。
- **次に確認すべきこと:** S-Log3等ほかのlog曲線にも同様の高bit-depth roundtrip契約が必要なら、既知のblack floorやtoe aliasingを先に定義して追加する。

## 2026-10-09 — Remaining log-curve 16-bit round trips

- **関連:** S-Log3、Canon Log 2、Canon Log 3、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 各曲線のnormalized 16-bit code 0〜65535全域をdecode→encodeした。S-Log3はblack floor（16-bit換算code 6089）または元codeへ戻る。Canon Log 2/3は各double参照式のEOTF→OETF合成結果を16-bit量子化したcodeと一致する。standalone transfer suiteは成功。
- **価値または懸念:** Cineon/DaVinci Intermediate/ACEScctに続き、主要6 log curveの高bit-depth往復格子を検査できる。Canon Log toe付近では元codeと異なる丸め先を参照式に基づいて許容する。
- **次に確認すべきこと:** もし16-bit roundtripを元code完全再現の契約に強める場合は、Canon Log toeの規範曲線を先に確認する必要がある。

## 2026-10-09 — ACEScc full 16-bit round trip

- **関連:** ACEScc OETF/EOTF、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** normalized code 0〜65535をdecode→encodeし、全codeが元の16-bit整数codeへ丸めて戻ること、途中のlinear/encoded値がfiniteであることを確認した。standalone transfer suiteは成功。ACEScc black sentinelを含む。
- **価値または懸念:** 以前の10-bit格子で確認していたcode往復を、16-bit全格子に拡張した。sentinel codeのlinear値自体は非可逆でも、同じOETF/EOTF実装の再量子化結果は入力codeと一致することを記録する。
- **次に確認すべきこと:** ACEScc値域外やnegative codeの再量子化は、このnormalized 16-bit格子とは別の契約として扱う。

## 2026-10-09 — Quantized display-code centers over a scene grid

- **関連:** sRGB、Gamma 2.2/2.4/2.6、Rec.709、Rec.2020、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 各曲線でlinear 0〜1を4097点に分け、float OETF encode→16-bit整数codeへ量子化→code centerをdecodeした値をdouble参照のcode center復号と比較した。復元値は元linear入力とも設定誤差内で一致。standalone suiteは成功。sRGB/Rec.709/Rec.2020は既知breakpoint差により最大5e-5、simple gammaはpow精度差を含み最大4e-5の参照許容値を使った。
- **価値または懸念:** code往復の整数一致に留まらず、scene-linear値としての16-bit quantization/reconstruction errorを確認できる。参照許容値は各曲線のfloat実装精度・piecewise境界差を含む。
- **次に確認すべきこと:** HDR/log curveにも同じcode-center基準を追加する場合はPQ OETFの既知局所下降と、log curveのblack sentinel/toe範囲を個別に織り込む。

## 2026-10-09 — Quantized HDR-code centers over a scene grid

- **関連:** PQ、HLG、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** linear 0〜1を4097点で走査し、float OETF encode結果を16-bit整数codeへ丸めた。量子化codeは独立double OETF参照の丸めcodeから最大1 code以内で、実際に選択したcode centerをdecodeした結果は、そのcenterの独立double EOTF参照およびscene-linear入力に対する誤差範囲内だった。standalone suiteは成功。
- **価値または懸念:** PQ/HLGのscene値を実際の量子化codeから復元した誤差を確認できる。参照codeと異なる隣接codeが選ばれた場合は、別codeのEOTF値を誤って期待値にしないよう、選択されたcode centerを基準に比較する。
- **次に確認すべきこと:** PQのより高密度またはHDR 1超のscene gridを追加する場合は、既知のOETF局所下降や定義域を越えた外挿を分けて扱う。

## 2026-10-09 — Quantized log-code centers over an extended scene grid

- **関連:** S-Log3、Canon Log 2/3、Cineon、ACEScc、DaVinci Intermediate、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 6曲線でlinear 1e-5〜1e4を4097点走査し、float OETFで量子化した16-bit codeが独立double参照の丸めcodeから最大1 code以内であることを確認した。選択したcode centerのdecode値も同じcenterに対するdouble EOTF参照と比較し、scene入力への復元誤差を確認した。standalone suiteは成功。
- **価値または懸念:** 広いscene/HDR範囲で、量子化後のcodeから復元する値まで検査できる。高域外挿では6曲線すべてで相対差が現れたため、code-center参照比較には最大2e-6の相対許容を設けた。
- **次に確認すべきこと:** 外挿域を含む許容値を変更する際は、curveごとの係数精度と定義域を確認し、通常の定義域の精度基準と分けて評価する。

## 2026-10-09 — Full 16-bit neutral cross-gamut pipeline

- **関連:** `tests/ArtifactCore/ColorPipelineStandaloneTest.cpp`、sRGB decode → sRGB-to-Rec.2020 gamut conversion → Rec.2020 encode。
- **確認済み:** 16-bit code 0〜65535の全グレースケール値を独立doubleパイプライン参照と比較し、Rec.2020の各出力channelが参照差2e-5以内、各channelで単調非減少、相互の差2e-6以内であることをstandaloneテストで確認した。
- **価値／懸念:** 個別transfer曲線・gamut行列テストでは見えにくい、decode→matrix→encode統合時の量子化全域の回帰を検出できる。入力はneutral rampに限定され、飽和色や負値／HDRの全域精度は別テストの範囲である。
- **次に確認すべきこと:** 同じ16-bit全code基準を異なるsource/target gamutやHDR transferの組み合わせへ広げる際は、行列後の負channelとtransferの負値契約を明示する。

## 2026-10-09 — Full 16-bit ACES output-preset pipeline

- **関連:** `ArtifactCore/include/Color/ColorACES.ixx`、`tests/ArtifactCore/ColorACESContractTest.cpp`。
- **確認済み:** 5つのACES output presetそれぞれで、linear neutral input 0〜65535/65535の全65,536段階について、`applyOutputTransform`の結果を独立に組み立てた「ACES AP1→output gamut→simple RRT→output OETF」参照と比較した。差3e-6以内、全channel finite、出力0〜1範囲をstandaloneテストで確認した。
- **価値／懸念:** 代表点テストを超えて、output preset dispatch・transform順序・transferの統合回帰を全16-bit neutral gridで検出する。単調性や出力neutralityはassertせず、PQの既知float局所下降やgamut white pointの振る舞いを仕様判断なしに不具合扱いしない。
- **未検証の気づき:** 初回にneutralityを仮定した試験では、`SDR_P3_D65` presetのコード上の出力gamut `DCI_P3` において、暗部のRGB差が2e-5を超えた。DCI-P3行列の白色点とpreset名のD65表記の意図・規範上の関係は未検証であり、このテストではneutralityを契約化していない。
- **次に確認すべきこと:** P3 presetのwhite point/primary規範とACESCOLOR preset名を確認し、neutralityを要件化するか判断する。

## 2026-10-09 — Full 16-bit round trips for display transfer codes

- **関連:** sRGB、Gamma 2.2/2.4/2.6、Rec.709、Rec.2020、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認済み:** 6曲線のnormalized 16-bit code全65,536点についてfloat decode→encode後の丸めcodeをdouble参照EOTF→OETFの丸め結果と比較し、全曲線で最大1 code以内に一致した。元codeとの差はRec.709以外が最大1 code、Rec.709は最大16 code（code 5309近辺）だった。
- **価値／懸念:** 参照式自体のpiecewise丸め特性と、float実装誤差による追加差を分離して監視する。Rec.709の最大差はOETF/EOTFのbreakpoint・係数が完全な逆写像でないことに由来する挙動として記録した。曲線式や閾値は変更していない。
- **次に確認すべきこと:** Rec.709のcode 5309近傍の往復差を規範契約として変更する必要がある場合は、適用規格のbranch定義を確認してから判断する。

## 2026-10-09 — Full 16-bit PQ and HLG code round trips

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、PQ/HLG、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認済み:** normalized 16-bit code全65,536点をfloat EOTF→OETFし、再量子化codeを独立double EOTF→OETF参照と比較した。両曲線とも参照往復とのcode差は最大1以内。元codeとの差はPQが最大2、HLGが最大1以内でstandalone suiteが成功した。
- **価値／懸念:** PQ/HLGのEOTF精度・OETF精度の個別走査に加えて、全codeをまたぐ量子化往復誤差を監視できる。PQは既知OETF局所下降のため、元code完全一致を契約にせず独立参照との一致を基準にする。
- **次に確認すべきこと:** HDR出力bit depthや伝達関数の係数を変更した場合は、最大code誤差と位置を再評価する。

## 2026-10-09 — Multi-row surface pixel conversion layout

- **関連:** `ArtifactCore/include/Image/SurfacePixelConversion.ixx`、`tests/ArtifactCore/SurfacePixelConversionTest.cpp`。
- **確認済み:** 3×2 BGRA 8-bit straight-sRGB入力をRGBA32 linear straightへ変換し、幅・高さ・rowStride・全6画素のchannel reordering、sRGB decode、alphaを独立double参照と比較した。alpha 0画素のRGB zeroingも含めstandaloneテストが成功した。
- **価値／懸念:** 既存の一行pixel群では確認できなかった複数rowのflattened pixel順序と出力stride契約を検査できる。source APIは連続bufferを受け取り、入力row strideは指定できないため、入力paddingはこのテストの対象外。
- **次に確認すべきこと:** もし入力rowStrideをAPIへ加える場合は、padding sentinelを置いた各rowの変換と不読領域を独立にテストする。

## 2026-10-09 — Opaque source alpha across surface output formats

- **関連:** `ArtifactCore/include/Image/SurfacePixelConversion.ixx`、`tests/ArtifactCore/SurfacePixelConversionTest.cpp`。
- **確認済み:** Opaque metadataを持つlinear float入力のalphaにNaNを設定し、RGBA32 float/half/sRGB byteの3 targetすべてでRGB値を期待形式に変換し、出力alphaをopaque 1（byteでは255）に固定することを確認した。standaloneテスト成功。
- **価値／懸念:** opaque descriptorが入力alpha値より優先される契約を全出力形式で検査する。入力pixel bufferそのものの不正RGB値の扱いは、このcaseでは対象外。
- **次に確認すべきこと:** SurfaceColorDescriptorのOpaque扱いを変更する際は3 target共通の期待を維持する。

## 2026-10-09 — Unknown transfer legacy surface boundary

- **関連:** `ArtifactCore/include/Image/SurfacePixelConversion.ixx`、`tests/ArtifactCore/SurfacePixelConversionTest.cpp`。
- **確認済み:** `transferKnown=false`のfloat sourceで、[0,1]内の値はlegacy sRGB EOTFでdecodeされ、範囲外のnegative/HDR値はlinear境界値として維持され、NaN/±infinityのRGBはzeroへsanitizationされることを独立参照と照合した。standaloneテスト成功。
- **価値／懸念:** transfer metadata欠落時の互換境界を、既知transferのdecode経路と区別して固定する。unknown transfer時の色域変換などは対象にしていない。
- **次に確認すべきこと:** legacy fallbackの移行・撤去時は、非有限値と正規化範囲外の値の扱いを含めて明示的に判断する。

## 2026-10-09 — Gamut identity matrix contract

- **関連:** `ArtifactCore/include/Color/ColorGamutConversion.ixx`、`tests/ArtifactCore/ColorGamutConversionTest.cpp`。
- **確認済み:** 11個の全Gamut enumで、同一Gamut間の変換行列9要素が厳密なidentityであること、および3つのRGB基底ベクトルがその行列で厳密に保持されることを独立standaloneテストで確認した。
- **価値／懸念:** sRGB/Rec.709の別名やXYZ/ACESを含むすべての自己変換分岐を、単一の代表例に頼らず検査できる。これは同一Gamutのshortcut契約だけを対象にし、異なるGamut間の数値精度を追加保証するものではない。
- **次に確認すべきこと:** 新しいGamut enumを加えた際は`kGamuts`へ追加し、この全件契約を維持する。
