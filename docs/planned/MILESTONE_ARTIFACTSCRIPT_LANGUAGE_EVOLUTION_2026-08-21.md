# マイルストーン: ArtifactScript Language Evolution

**最終更新:** 2026-08-22
**ステータス:** In Progress
**優先度:** High
**関連:** `docs/planned/MILESTONE_ARTIFACTSCRIPT_ENGINE_2026-07-21.md`(完了済みコア), `docs/planned/MILESTONE_SCRIPT_CONSOLE_2026-06-16.md`, `docs/planned/MILESTONE_AUTOMATED_TESTING_FOUNDATION_2026-08-21.md`

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
