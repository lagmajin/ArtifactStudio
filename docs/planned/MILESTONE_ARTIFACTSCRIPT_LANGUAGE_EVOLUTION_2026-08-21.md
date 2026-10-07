# マイルストーン: ArtifactScript Language Evolution

**最終更新:** 2026-10-07
**ステータス:** In Progress
**優先度:** High
**関連:** `docs/planned/MILESTONE_ARTIFACTSCRIPT_ENGINE_2026-07-21.md`(完了済みコア), `docs/planned/MILESTONE_SCRIPT_CONSOLE_2026-06-16.md`, `docs/planned/MILESTONE_AUTOMATED_TESTING_FOUNDATION_2026-08-21.md`

## 進捗 2026-10-07

- expressionの後置index/member/methodを連鎖可能にし、`matrix[0][1]`、`nodes[0].score`、`nodes[0].getScore()`を追加。既存のindex/field/method evaluatorを再利用し、新しいAST種別や実行時lookupは増やしていない。統合fixtureで3経路の値を確認し、ArtifactScript関連5 CTest suitesは5/5 passed。既存4 KiB string parser fixtureのMSVC Debug測定は未escape 51.3、末尾escape 48.5 µs/parse（各1回）で、過去記録の52.8 / 55.7より悪化は見えないが、単回測定のため速度向上とは断定しない。
- 配列indexのint/double変換を共通の境界検査へ集約。負数、NaN、無限大、配列長を超える値は`size_t`変換前に拒否し、正の小数は従来どおり切り捨てる。通常読み込み、binary比較の参照最適化、代入の3経路で不正値診断を維持する。回帰テストを追加し、ArtifactScript関連5 CTest suitesは5/5 passed。
- `==` / `!=`を型に応じて比較するよう修正。bool同士はbool値、int同士は整数値、int/float混在は数値、string・Vec2/3/4・Colorは値、ObjectRefはid、script objectとarrayは参照identityで判定し、異なる非数値型を数値0へcoerceしない。従来は`true == true`がfalse、別々のarrayも等しい判定になり得た。各型の回帰テストとArtifactScript関連5 suitesは5/5 passed。
- string literalでC#式の引用符・slash・制御文字escapeと4桁/8桁Unicode escapeをdecodeし、UnicodeはUTF-8化、UTF-16 surrogate pairも結合する。未知escape、不正hex、孤立surrogateは式と文字列field initializerでescape位置を診断する。escapeなし文字列の終端探索は`find_first_of`より単純な2条件ループが速かったため採用。4 KiB unescaped literalを含むMSVC Debug parse 200回×3では`find_first_of`版の94.9 / 96.1 / 106.6 µs/parseから単純ループ版の54.2 / 49.0 / 53.3へ（平均約47%短縮）。Unicode対応後も同fixtureは52.8 µs/parse、末尾にnewline escapeを置くと55.7 µs/parse。限定したDebug fixtureの結果であり、Release・短い文字列・実script全般には一般化しない。既定フィールド文字列もescapeをdecodeし、field initializer末尾の`;`を値から除外してquote/semicolon混入の既存不具合を修正。escape位置・Unicode・初期値・brace scan testsを含むArtifactScript関連5 suitesは5/5 passed。
- `null`リテラルを追加し、`null == null`をtrue、`null == 0`と`null == false`をfalseとして評価する。`null` prefixの識別子（`null_value`）をキーワードと誤認しない境界判定も修正。null比較はgeneric numeric coercionを通らない早期分岐にした（性能差は未計測）。null値、異なる型との比較、underscore識別子をまとめた実行テストと、ArtifactScript関連5 CTest suitesは5/5 passed。
- foreach bodyの配列mutation判定を同一immutable `ArtifactScriptInstance` definitionのhook間で64-entry固定cacheする。mutable definition accessorとdirect evaluator pathは従来の毎回scanへfallbackし、collisionも再scanする。空配列・256 statement bodyのMSVC Debug fixtureは2.52 / 2.49 / 2.81から1.75 / 1.73 / 1.73 µs/hook（平均約33%短縮）。ASTをhook後に差し替えるfallback testと、mutation foreach既存testsを含むArtifactScript関連5 suitesは5/5 passed。Release/workload性能とfixed memory costは今後確認する。
- function/new/vector/arrayの引数リストをdelimiter-awareにparseし、空リストを維持しつつ抜け引数・不正separator・missing closing delimiterを診断する。index式、dot member名、unary operandも診断対象。呼び出し引数、constructor引数、空indexの各failure offsetと、valid zero-argument call / constructorをテスト。ArtifactScript関連5 CTest suitesは5/5 passed。
- binary operand / assignment・declaration RHS / `if`・`while` conditionで必須式が欠落した場合、partial ASTのまま受理せずsource位置付きdiagnosticを出す。`value = 1.0 + ;` の失敗位置が6行23列であることを確認。`return;`と省略可能な`for` conditionは許容。引数・indexの診断は後続entryで拡張し、他のdelimiter positionsは引き続き確認する。
- method bodyを所有済みdefinition sourceの`std::string_view`で直接解析し、一時string copyを除去。長いliteralを含む本文の100-parse MSVC Debug fixtureは87 allocations / 5,246 bytesから84 / 4,686 bytes per parseへ減少。method brace scanはstringとcomment内のbraceを無視し、parser stall診断に正確なsource line/columnを付ける。parse CPU時間とRelease profileは未確認。
- pureなbinary operandにscript object `FieldAccess`を加え、field mapの値を再帰的にconst参照で比較へ渡す。`target.value == expected` のlong-string fixtureはMSVC Debug CRTで変更前4 allocations / 160 bytes per hook、変更後0 / 0。top-level `this`、呼び出し、objectでない値、missing fieldは従来評価へfallback。ArtifactScript関連5 CTest suitesは5/5 passed。CPU時間・Release性能は未確認。
- pure `variable/literal/index` operandsをbinary expressionから参照解決し、array itemやlong stringを中間`ArtifactScriptValue`へcopyせず`evalBinary()`へ渡す。call・`this`・field access・invalid indexは従来の評価/fallback pathを維持する。`values[0] == target` のMSVC Debug CRT allocation baselineは4 allocations / 160 bytes per hook、直接参照後は0 / 0。数値array index結果、長文string一致回数をassertし、ArtifactScript関連5 CTest suitesは5/5 passed。CPU時間は他プロセス負荷で揺れたため改善を主張せず、Release/optimized workloadでのtime comparisonは未確認。
- `+`・比較などのsimple `variable/literal` binary nodeは、値参照を一度ずつ解決して`evalBinary()`へ直接渡す。string/string additionのみ既存のreserve済みfast pathを維持し、`&&` / `||`は短絡挙動を保つため除外。parse後にAST variable名を変える回帰fixtureも追加。MSVC Debugの変更前後各2回計測は、32-method lookupが平均60.30→52.40 µs/hook（約13%短縮）、object method lookup 75.80→68.59（約9.5%）、3-class polymorphic call 106.31→96.97（約8.8%）、4-class case 111.79→103.61（約7.3%）。ArtifactScript関連5 CTest suitesは5/5 passed。測定幅を含むためReleaseや実scriptへ一般化しない。
- double targetへの `+= -= *= /= %=` は、右辺がdouble/int64なら `ArtifactScriptValue` の計算結果一時値を経由せずtargetを直接更新する。division/modulo by zeroの診断とfallbackを保ち、field/local/array item、int RHS、各演算の回帰テストを追加。変更前後各2回のMSVC Debug hook benchmarkでは、8要素foreachが平均6.05→5.18 µs/hook（約14%短縮）、257要素numeric foreachが104.12→平均76.92（約26%短縮）、257個の128文字string比較fixtureが252.33→平均230.71（約9%短縮）。ArtifactScript関連5 CTest suitesは5/5 passed。fixture測定のためRelease・実script性能への一般化は未確認。
- `a + b + c + d` のような加算ASTは、全leafが既存literalまたはstring variableのときに限り、固定深さ上限64の2-pass走査で最終長を数え、1つの結果stringへ一度だけappendする。callや非string leaf、上限超過は従来評価へfallbackし、式の副作用を重複実行しない。128文字string 4個のfixtureはMSVC Debugで15 allocations / 2366 bytesから3 / 560 per hookへ減少。call副作用が1回であることもテスト。
- string targetの `+=` は右辺がstringでなくてもtargetへ直接appendする。整数 / bool / doubleは従来と同じ `to_string` / `ostringstream` 表記で追記し、その他の型は従来同様空文字相当で維持する。128文字string + int fixtureはMSVC Debugで6 allocations / 352 bytesから3 / 176 per hookへ半減。string・int・bool・doubleの結果を回帰テストで確認。
- 文字列同士の `+` は、simple variable / literal operandなら `evalExpr()` の値copyを避け、合計サイズをreserveした結果stringへ直接appendする。複合式やmixed scalar/stringも `evalBinary()` 内で一時文字列を介さず結果へappendする。128文字×2の連結fixtureはMSVC Debugで11 allocations / 944 bytesから3 / 304 per hookへ減少。整数・boolとの連結結果を確認するテストを追加。
- `=` の単純代入では、評価済み右辺を `ArtifactScriptValue` から返して再コピーせず、local / field overlay / array item の所有先へ直接moveする。128文字stringをfieldへ代入するMSVC Debug fixtureで4 allocations / 320 bytesから2 / 160 per hookへ減少し、割当を半減した。localの再代入とarray itemへのstring代入を含む回帰テストを追加。ArtifactScript関連5 suitesはCTest **5/5 passed**。
- `tests/ArtifactCore/LayerScriptComponentContractTest.cpp` と `ArtifactCoreLayerScriptComponentTest` を追加。2件の契約テストで、コア ArtifactScript ランタイムのライフサイクルフック順、空フック、public フィールドの初期値、複数フレームにわたる `dt` / `time` / `frame` の受け渡しと状態保持、行／ブロックコメントの解析を検証する。
- ビルドを妨げていた `ArtifactScript.cppm` の `EnvironmentVariable` モジュール参照・ビルド順依存の CMake 登録漏れを修正。クラス `{` 単独行でのパーサー停止、字下げ後のインラインメソッド本体位置、複数行メソッド本体の括弧走査、空メソッドを定義済みとして扱わない問題を修正し、メソッド本体の `//` / `/* */` コメントを読み飛ばすようにした。
- object method call-site cacheをcall-site address基準の3-way set-associativeに変更。runtime class nameのhashをhit経路から外しつつ、同一call-siteの3種類までのruntime classを固定容量cacheに保持できる。増加分はsetごとに1 entry（32 entries）で、heap allocationはない。
- expression parserの乗除算層へ`%`を接続し、式内moduloを実行可能にした。また、method body parserでstatementが入力位置を進めない場合に解析を打ち切り、diagnosticを返すようにした。未知operatorによるparse停滞・AST増殖を回帰テストで検出する。
- MSVC Debugで同じ交互クラスcaseを一時的な1-way call-site cacheと2-way cacheでA/B比較した。1-wayは107.68 µs/hook、2-wayは92.57 µs/hook（各3,000 hook、各16 calls、約14%短縮）。単一クラスのobject method lookupは1-way 68.66、2-way 71.29 µs/hookで測定揺れを含む小差、5-field object methodは120.00対119.58 µs/hookだった。各variant 1 runのDebug結果なのでrelease性能の断定には使わない。交互case込みのbenchmark suite全体は約19秒で完了した。
- 同じ3-class script・16 calls/hookのMSVC Debug A/Bを実行した。2-wayは145.30 / 145.81（平均145.56）、3-wayは95.31 / 92.28 / 95.01（平均94.20）µs/hook、約35.3%短縮した（各3,000 hook）。32-method object lookupは2-way 98.82 / 94.98、3-way 70.67 / 70.13 / 68.59、5-field object methodは159.10 / 159.64対120.74 / 121.77 / 121.45 µs/hook。Debugの少数runのためrelease性能は未検証。ArtifactScript関連5 targetsは3-way化後もCTest 5/5 passed。
- 4-class交互caseも追加し、3-way（106.33 / 102.31）と4-way（100.05 / 102.10 µs/hook）を比較した。4-wayは4-classで約3.1%短い一方、3-class caseでは4-wayの平均が3-wayより約6.1%遅かったため、Debug計測の範囲ではcache容量・hit比較数との総合的な利点が確認できず、3-wayを維持した。
- script objectのhost-method fallbackで `instance->className` を毎hook copyする経路を参照渡しへ変更。long-name host objectのDebug allocation testでは変更前3 allocations / 64 bytes、変更後1 / 16 per hookとなり、残る1 allocationも上限としてassertする。ArtifactScript関連5 targetsは変更後もCTest 5/5 passed。
- MSVC DebugでArtifactScript関連5 test targetsをビルドし、CTest **5/5 passed**（`ArtifactCoreArtifactScriptTest`、`ArtifactCoreLayerScriptComponentTest`、`ArtifactCoreArtifactScriptObjectTest`、`ArtifactCoreArtifactScriptHostMethodTest`、`ArtifactCoreArtifactScriptHostApiTest`）。call-site cache変更後も同一call-siteでのruntime class切替を含めて通過した。
- `ArtifactScriptInstance::findLifecycleHookInDefinition()` に、クラス数9〜64の定義だけで使う固定128-slot class indexを追加。毎hook invocationで行う継承チェーンのクラス検索を平均定数時間にし、8クラス以下は従来の線形検索、65クラス以上もbounded fallbackの線形検索を維持する。definitionは呼び出しごとに再索引するため、mutable definition APIの変更も反映される。
- 30クラス継承チェーンでbase classの`OnUpdate`を探索するMSVC Debug benchmarkを追加。linear lookup 27.91 µs/hookに対しindex後6.53 / 7.11 µs/hook（約75%短縮）。通常の単純hookは1.80 / 1.82 µs/hook。ArtifactScript関連5 suitesは変更後もCTest **5/5 passed**。
- root class自身にhookがある場合はclass index構築前に返すようにし、派生クラスが多数あっても不要な索引構築を省略。30-class定義でroot hookを呼ぶMSVC Debug benchmarkは変更前2.81、変更後1.98 µs/hook（約30%短縮）。派生hook優先順位とbase fallbackを既存契約テストで確認し、関連5 suitesはCTest **5/5 passed**。
- `ArtifactScriptLocals` のinline容量を12から16へ変更。MSVC Debugで12枠版に対するmethod/local比を2回測り、`locals(12)`が平均約2.7%、`locals(20)`が約3.7%短縮した。8枠版は有利でなかったため採用せず、16枠を超える値は既存の固定32-entry evaluator workspaceへ送る。追加stack領域は4 local binding / active call frame、最大call depth 64でbounded。
- `ArtifactScriptInstance` のlifecycle method解決結果をhook種別ごとの固定6-entry cacheへ保持し、class indexも不変definitionでのhook execution間に再利用する。`ArtifactScriptInstance::definition()` のmutable overloadは参照がescapeする前にlookup cache reuseを恒久disableし、definition/ASTを書き換えた後のstale pointer・class mappingを避ける。30-class inherited hook benchmarkは従来6.35 / 7.11 µs/hookから1.56 / 1.59 µs/hookへ（約75%短縮）、31-class root hookは1.98 / 2.09から1.55 / 1.58 µs/hookへ（約21〜25%短縮）。mutable accessor経由でnew class nameを変更し、さらにcached hook bodyをremoveしても新しいdefinition状態が反映されるテストを追加。ArtifactScript関連5 suitesはCTest **5/5 passed**。
- `Decl` statementで評価済み初期値をlocal bindingへcopyしていた経路をmoveへ変更。MSVC Debugの長いstring local declaration fixtureでは10 allocations / 672 bytesから8 / 512 per hookへ減少（2 allocations / 160 bytes削減）。source/observed両fieldの値一致を確認する回帰fixtureを追加。ArtifactScript関連5 suitesはCTest **5/5 passed**。
- user method / script object methodのreturn slotから結果を取り出すcopyをmoveへ変更。MSVC Debugで両methodから128文字stringを返すfixtureは20 allocations / 1344 bytesから17 / 912 per hookへ減少（3 allocations / 432 bytes削減）。source/observedの文字列一致を確認する回帰fixtureを追加。ArtifactScript関連5 suitesはCTest **5/5 passed**。
- 両辺がstringの`+=`では、既存local / field overlay / array itemのstringへ直接appendし、結合用一時文字列を作らないようにした。他型へのcompound assignmentは従来経路を維持。MSVC Debugの128+128文字fixtureでは6.60から4.82 µs/hook（約27%短縮）、13 allocations / 1104 bytesから7 / 752 per hookへ減少。local・field・array itemの結果確認fixtureを追加し、ArtifactScript関連5 suitesはCTest **5/5 passed**。
- このテストは ArtifactCore のスクリプトランタイムを対象とし、Artifact サブモジュールの `ArtifactAbstractLayer` にあるレイヤーコンポーネント連携や UI / プロジェクト読込を通した統合動作は対象外。統合受入確認は未完了。

## 目的

自作言語 ArtifactScript(`ArtifactCore/src/Script/ArtifactScript/`)を、C++/C# ライクな文法・クラス対応・ホストバインディング・簡単な編集実行サイクル・Unity 風シリアライズ・高速な編集反映を備えた実用言語へ進化させる。ユーザーが設定した6つの受入条件:

1. **① 文法を C++/C# に近づける**
2. **② クラス対応**(ユーザースクリプト内のクラス)
3. **③ 簡単にエクスポート = スクリプトから C++ ホスト側へ関数を公開するバインディング API**(2026-08-21 ユーザー確定)
4. **④ 編集→コンパイル→実行が簡単**
5. **⑤ Unity 風シリアライズ**(public フィールド自動露出)
6. **⑥ すばやい編集と実行**(ホットリロード)

## 現状(2026-08-21 実測)

### 動いているもの

| 機能 | 根拠 |
|---|---|
| クラス宣言 + public/private フィールド(型+デフォルト値) | `ArtifactScript.cppm:265-320` |
| ライフサイクルフック構文(OnCreate〜OnDestroy の名前判定) | `cppm:46-54` |
| メソッド定義・呼び出し・return、if/else、while、for、配列リテラル/インデックス代入 | `cppm:191-240`(パーサー)、`cppm:716-803`(評価器) |
| 組込関数(clamp/lerp/push 等)、配列組込 8種 | `cppm:617-684` |
| ホットリロード(ファイル監視→再パース→public フィールド型一致マイグレーション) | `cppm:805-931`、テスト `ArtifactScriptTest.cpp:219-288` |
| gtest 13本登録済み | `tests/ArtifactCore/CMakeLists.txt:17-22` |

### 欠落・バグ

| 問題 | 根拠 |
|---|---|
| **`invokeHook()` がスタブ**(呼び出し記録のみでスクリプトを実行しない) | `ArtifactScript.cppm:471-477` |
| **文字列連結不可**(`"a" + "b"` が double 演算で 0) | `evalBinary` Add が常に数値加算 `cppm:575` |
| 複合代入 `+= -= *= /=`、インクリメント `++ --` なし | パーサーの演算子セットに存在しない |
| `break` / `continue` なし | `ArtifactScriptStmt::Kind` に不在 |
| 三項演算子、`var`、文字列比較演算なし | 同上 |
| `obj.method()` / `this.x` のメンバアクセス構文なし | FieldAccess ノードが未使用(`ixx:143`) |
| 継承は文字列検出のみ(`contains("ArtifactBehaviour")`)、複数クラス不可 | `cppm:277`、`rootClass` 単一 |
| メソッド本体の構文エラーに行番号がない | ミニパーサーが診断を生成しない |
| アプリ本体から未接続(Artifact 側 import 0件) | UI/Inspector/プロジェクト保存すべて未着手 |

## 実施フェーズ

### Phase 1 — 文法の C#/C++ 寄せ(要件①)+ 即効修正

2026-08-22 更新: 当初未着手扱いだった項目の大半が実装済みであることをソース確認
(`invokeHook` 実行化 cppm:547、文字列連結 cppm:660-675、文字列比較 cppm:676-688、
複合代入 cppm:846-850、`++`/`--` cppm:222、break/continue cppm:206)。
三項演算子・短絡評価・`var`・`foreach` を本日実装し、テスト追加済み。

- [x] `invokeHook()` を実実行化(内部で evaluator の executeMethod を呼ぶ)
- [x] 文字列連結(`string + string`、`string + 数値`)と文字列比較(`== != < >`)
- [x] 複合代入 `+= -= *= /= %=`
- [x] `++` / `--`(後置・前置)
- [x] `break` / `continue`
- [x] 三項演算子 `cond ? a : b`、論理短絡評価の明確化(2026-08-22)
- [x] `var x = ...` 型推論宣言(2026-08-22)
- [x] `foreach (x in array)`(2026-08-22。本体での push/clear に備え要素をコピー走査)
- [ ] メソッド本体内構文エラーへの行番号付与(パーサーに line/column 追跡を導入)

### Phase 2 — バインディング API(要件③、最重要のアーキテクチャ決定)

C++ ホスト側の関数・型をスクリプトから呼べるようにする。設計方針:

2026-08-22: `ArtifactScriptHost` レジストリの基盤を実装。グローバルレジストリ
(`ArtifactScriptHost::global()`)に `registerFunction(name, fn)` で登録すると、
スクリプトから通常の呼び出し構文で呼べる。evalCall は組込→ユーザーメソッド→
ホスト関数の順に解決。`print`/`log` がホストの bounded ログリング(256行)に
出力するよう実体化。ホスト側エラーは `setLastError()` → evaluator の
`error_ = "host: ..."` 経由で診断化。

- [x] `ArtifactScriptHost` 登録レジストリ新設(2026-08-22):
      ```cpp
      // C++ 側
      ArtifactScriptHost::global().registerFunction("setLayerOpacity",
          [](std::span<const ArtifactScriptValue> args) -> ArtifactScriptValue { ... });
      ```
- [x] スクリプト側は通常の関数呼び出し構文で呼べる。evalCall の解決順を
      組込 → ユーザーメソッド → ホストレジストリに変更(2026-08-22)
- [ ] コンポジション操作の標準ライブラリ第一弾(`getLayer/setProperty/getTime`
      のホスト実装。`print`/`log` のログ収集は実体化済み、UI 出力先は未接続)
- [ ] ホストオブジェクトのメソッド呼び出し(`obj.method(args)`)— ObjectRef 値に紐づくメンバ呼び出し解決
- [x] エラー伝播(バインド関数内エラー → `setLastError` → evaluator 診断化。2026-08-22)

### Phase 3 — ユーザークラスの実質対応(要件②)

- [ ] インスタンス値型 `ObjectInstance`(クラス定義 + フィールド辞書)を値モデルへ追加
- [ ] `new ClassName(...)` 式、コンストラクタ
- [ ] `this` 参照、フィールドアクセス `this.speed` / ローカルオブジェクト `p.x`
- [ ] メソッドディスパッチ(インスタンスのクラス定義から解決)
- [ ] 継承チェーン(`class A : B` → B のフィールド/メソッド継承、仮想呼び出しは単一親の線形探索で足りる)
- [ ] 複数クラス定義(1ファイル複数クラス、rootClass の一般化)
- [ ] `is` / 型チェック組込

### Phase 4 — Unity 風シリアライズ(要件⑤)

- [ ] 属性構文 `[Range(0,1)] [Header("Movement")] [SerializeField] private float x;` のパース
- [ ] Inspector への public フィールド自動露出(既存 PropertyWidget の仕組みに接続)
- [ ] レイヤーへの ScriptComponent 追加導線 + プロジェクト保存/復元(toJson/fromJson)
- [ ] ホットリロード時のフィールド移行をプロジェクト保存データにも適用(既存 migrate ロジック流用)

### Phase 5 — 編集→実行サイクル(要件④⑥)

- [ ] スクリプトエディタウィジェット(シンタックスハイライト、診断表示。既存 ExpressionCopilotWidget の UI 資産を参考に)
- [ ] 保存 → ホットリロード → 再生中インスタンスへ即反映(コア完成済み、UI 配線のみ)
- [ ] エラー時は旧定義を維持し Problem リスト表示(既存 reload の success=false 経路を利用)
- [ ] 実行ログウィンドウ(print/log の出力先)

## 受入条件

1. Phase 1〜5 の全チェックボックスが完了
2. 既存13テスト+各Phaseの新規テストが ctest で緑
3. 「エディタで編集 → 保存 → 再生中の動作が変わる」がアプリ内で完結する
4. C++ 側10関数以上がスクリプトから呼べ、ドキュメント化されている
5. Unity 的体験: スクリプトの public フィールドが Inspector に出て、値を変えて保存するとプロジェクトに残る

## 対象外

- バイトコード VM / JIT への置換(ツリーウォークのまま。速度不足が実測されたら別途起票)
- マルチスレッド実行モデル
- ジェネリクス、インターフェース、デリゲート/ラムダ(C# 相当の高度機能)
- AngelScript/CSharpScriptEngine からの移行(並存のまま)

## リスクと確認方法

- **②のクラス対応は評価器の環境モデル改修が必要**: 現在 locals + fields の2層だが、インスタンス導入で「オブジェクトごとのフィールド空間」が必要になる。Phase 3 着手前に Phase 1 の言語基盤(break/continue/複合代入)を固めておくことで影響を最小化する。
- **③のバインディングは ABI 安定性が主題**: ArtifactScriptValue(variant)を公開契約とし、std::function ベースの登録 API にする。DLL 境界を越える場合は AGENTS.md の PImpl/所有規約に従う。
- **⑥のホットリロードは再生中の状態整合**: OnUpdate 実行中の再読み込みはフレーム境界で遅延させる。
- **ビルド・テスト実行**: AGENTS.md 制約によりユーザー指示が必要。
