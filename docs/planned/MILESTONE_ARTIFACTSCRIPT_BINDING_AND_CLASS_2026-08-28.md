# マイルストーン: ArtifactScript Binding API 強化 & クラス対応（Phase 2/3 実走）

**最終更新:** 2026-08-30
**ステータス:** Phase A / C 実装済み、Phase B / D〜H 未着手。ビルド・テスト実行待ち
**優先度:** High
**関連:**
- `docs/planned/MILESTONE_ARTIFACTSCRIPT_LANGUAGE_EVOLUTION_2026-08-21.md`（全体計画。本書は Phase 2 残 / Phase 3 の具体的スライス）
- `docs/planned/MILESTONE_ARTIFACTSCRIPT_ENGINE_2026-07-21.md`（完了済みコア）
- `docs/planned/MILESTONE_AUTOMATED_TESTING_FOUNDATION_2026-08-21.md`（テスト基盤）
- `MILESTONE_ARTIFACTSCRIPT_LANGUAGE_EVOLUTION_2026-08-21.md` 6 受入条件: ① 文法(C++/C# 風) ② クラス ③ C++ ホストバインディング ④ 簡単編集→実行 ⑤ Unity 風シリアライズ ⑥ ホットリロード
- 関連ソース: `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm`, `ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx`

## 目的

既存 Phase 1（文法 C++/C# 寄せ）は 2026-08-22 時点で大半実装済み。残る **Phase 2（バインディング API 実用化）** と **Phase 3（ユーザークラス対応）** を、評価器を壊さずに段階的に実装する。**堅実路線**で、(1) 既存テストの回帰なし (2) 新規ユニットテスト追加 (3) スクリプト→C++ 単方向の最小実用 I/F の 3 つを各スライスで確認する。AGENTS.md 制約によりビルド・テスト実行はユーザー指示待ち。

## Update 2026-08-30 — Phase A / C reconciliation

- `ArtifactScriptHost::installCompositionApi()` は `getLayer`、`getLayerCount`、`getTime`、`getProperty`、`setProperty` を既存のhost registryへ登録しており、Phase Aの最小標準ライブラリは実装済みだった。
- `ArtifactScriptMethod` に宣言位置のline / columnを保存し、`executeMethod()` 経由でメソッド実行中に発生した評価エラーへ `line:column` 接頭辞を付けるようにした。`ArtifactScriptTest` に未知関数エラーの位置回帰ケースを追加した。
- body内部の各statement単位の精密な構文診断、host object method dispatch、user class value model以降は未実装である。

## 現状(2026-08-28 実測)

### 実装済み（Phase 2 基盤）

| 機能 | 根拠 |
|---|---|
| `ArtifactScriptHost::global()` グローバルレジストリ | `ArtifactScript.cppm:???`（要再確認） |
| `registerFunction(name, fn)` で C++ ラムダを登録 | 同上 |
| `print` / `log` を bounded ログリング(256行)へ接続 | 同上 |
| 評価器が `組込 → ユーザーメソッド → ホストレジストリ` の順で解決 | 既存 milestone 記載 |
| `setLastError()` 経由のエラー伝播 | 同上 |

### 未実装（Phase 2 残り / Phase 3）

| 項目 | 根拠 |
|---|---|
| 標準ライブラリ第一弾（`getLayer / setProperty / getTime` 等の C++ 側実装） | milestone `MILESTONE_ARTIFACTSCRIPT_LANGUAGE_EVOLUTION_2026-08-21.md` Phase 2 セクション |
| ホストオブジェクトのメソッド呼び出し（`obj.method(args)`） | 同上 |
| `ObjectInstance` 値型（クラスインスタンス） | Phase 3 |
| `new ClassName(...)` 式・コンストラクタ | Phase 3 |
| `this` 参照、フィールドアクセス `this.x` / `obj.field` | Phase 3 |
| メソッドディスパッチ（インスタンスのクラス定義から解決） | Phase 3 |
| 継承（`class A : B`、線形探索で十分） | Phase 3 |
| 1 ファイル複数クラス | Phase 3 |
| `is` 型チェック組込 | Phase 3 |
| Phase 1 残: メソッド本体内構文エラーへの行番号付与 | milestone Phase 1 末尾 |

### 設計判断（AGENTS.md / taste 整合）

- **評価器はツリーウォーク継続**。バイトコード VM / JIT は対象外（既存 milestone の除外条項を踏襲）。
- **公開契約は `ArtifactScriptValue` variant のみ**。`std::function` ベースの登録 API を採用し、ABI 安定性を優先。
- **DLL 境界は当面越えない**。越える必要が出たら既存 PImpl 規約に従う。
- **既存 `rootClass` 単一前提からの離脱**を Phase 3 で行うが、`Class` テーブルへの一般化で表現し、シングルクラスプロジェクトの後方互換は維持。
- **追加 import は実装 `.cppm` 側に限定**。`.ixx` の import 集合を増やさない（cppm-modules taste に従う）。

## フェーズ構成

各 Phase は 1 スライス = 1 つの独立したマージ可能な実装。完了条件に新規ユニットテストを含める。

### Phase A — ホスト API 標準ライブラリ第一弾（Phase 2 残り）

- `getLayer(name) -> ObjectRef`
- `getLayerCount() -> int`
- `getTime() -> float`（comp フレーム / fps 評価済秒）
- `getProperty(target, path) -> Any` / `setProperty(target, path, value)`
- `log(message)` 既存
- 登録は `ArtifactScriptHost::installCompositionApi(ICompositionApi&)` の単一フックで一括登録
- 追加ファイル: `ArtifactCore/src/Script/ArtifactScript/ArtifactScriptHostApi.cppm`（既存 `ArtifactScript.cppm` の肥大化を避ける）
- テスト: `tests/ArtifactCore/ArtifactScriptHostApiTest.cpp`
  - 各 API を登録 → スクリプトから呼ぶ → 期待値一致
  - C++ 側モックを `ArtifactScriptHost::installCompositionApi` に渡す
  - 既存 `ArtifactScriptTest.cpp` の 13 件と並走

### Phase B — ホストオブジェクトのメソッド呼び出し（Phase 2 残）

- `ArtifactScriptValue` に `ObjectRef` 値型は既存
- `ObjectRef` に対して `obj.method(args)` をディスパッチする評価器拡張
- 解決順: (1) インスタンスフィールドのクロージャ風呼び出し (将来) → (2) ホスト登録の `registerMethod(className, name, fn)` → (3) エラー
- `registerMethod` は Phase A の標準ライブラリ実装の素地
- テスト: `ArtifactScriptHostMethodTest.cpp`
  - `registerMethod("Composition", "addLayer", fn)` → スクリプトから `comp.addLayer("ShapeLayer")`
  - 存在しないメソッド呼び出しが `setLastError` 経由で診断化されること

### Phase C — メソッド本体の行番号付与（Phase 1 残）

- パーサーに `ParseCtx` 拡張: `currentLine()`, `currentColumn()` を `line/column` メンバで保持
- `Method::line` / `Method::column` を `cppm:parseMethod` で記録
- 評価時の診断メッセージに `line:col` 接頭辞を付与
- 既存メソッドは `line=0` 既定値で後方互換
- テスト: `ArtifactScriptDiagnosticsTest.cpp`
  - 構文エラーを含むスクリプトを評価し、診断に line:col が出る

### Phase D — ユーザークラスの値モデル `ObjectInstance`（Phase 3 開始）

- `ArtifactScriptObjectInstance` 構造体: `className` + `fields: vector<ArtifactScriptValue>`
- `ArtifactScriptValue` variant に追加
- クラス定義テーブル: `ArtifactScriptClassDef { name, parent?, fields: vector<FieldDef> }`
- 既存の単一 `rootClass` 構造は `ClassRegistry` へ一般化。`rootClass` は「最初のクラス」のシンタックスシュガーとして残す
- テスト: `ArtifactScriptObjectInstanceTest.cpp`
  - 値モデルの clone / equal / variant 訪問

### Phase E — `new` 式と `this` 参照

- パーサーに `new ClassName(args)` 式追加
- 評価器: コンストラクタ呼び出し（クラスに `OnConstruct(name, args)` フックがあれば実行、無ければフィールドのデフォルト値で初期化）
- `this` 参照を `MethodScope` に追加
- テスト: `ArtifactScriptNewExpressionTest.cpp`
  - `var p = new Point(1, 2)` → フィールドが設定される
  - `OnConstruct` フックが呼ばれ、`this` が引数として渡る

### Phase F — フィールドアクセス `obj.field` とメソッドディスパッチ

- 評価器: フィールドアクセス式を処理
- メソッド呼び出しの解決順を拡張: (1) インスタンスのクラス定義 → (2) Phase B のホストメソッド → (3) エラー
- テスト: `ArtifactScriptFieldAccessTest.cpp`
  - `p.x = 5` / `var v = p.x`
  - `p.distance(other)` 形式のメソッド呼び出し

### Phase G — 継承と 1 ファイル複数クラス

- パーサ: クラスヘッダに `: Parent` を受け付ける
- 評価器: メソッド/フィールド解決を親へ線形探索
- ルートクラスを「ファイル内の最初のクラス」と定義し、ファイル内の `class A { ... } class B { ... }` を許可
- テスト: `ArtifactScriptInheritanceTest.cpp`
  - 単一継承のメソッドオーバーライド
  - 複数クラスの共存

### Phase H — `is` 型チェック組込（Phase 3 完了）

- `obj is ClassName` 評価式
- 評価器: インスタンスの `className` と一致すれば true
- テスト: `ArtifactScriptIsOperatorTest.cpp`

## 受け入れ条件

1. Phase A〜H の各 Phase が独立マージ可能（既存テスト 13 件+各 Phase 新規テストが ctest で緑）
2. `ArtifactScriptHost::installCompositionApi` 経由で `getLayer / setProperty / getTime` がスクリプトから呼べる
3. スクリプト内で `class Point { ... }` 定義 → `new Point(...)` → `this.x` 読み書き → メソッド呼び出し が一連で動く
4. 構文エラーの診断に行番号が出る
5. 1 ファイル内に複数クラスが定義できる

## 対象外

- バイトコード VM / JIT、ジェネリクス、インターフェース、デリゲート/ラムダ、AngelScript / CSharpScriptEngine からの移行
- マルチスレッド実行モデル
- 既存 `MILESTONE_ARTIFACTSCRIPT_LANGUAGE_EVOLUTION_2026-08-21.md` の Phase 4（Unity 風シリアライズ）/ Phase 5（エディタ UI） — 別途起票
- ビルド・テスト実行（AGENTS.md 制約）

## リスクと確認方法

- **Phase D の `ArtifactScriptValue` 拡張は既存テストへの波及が大きい**。variant 追加は ABI を変えないが、`std::visit` 系コードが新 case を要求する可能性があるため、`cppm` を一通り grep してから着手する。
- **Phase E の `this` スコープはクロージャ実装の踏み台**。将来意図しない capture を生まないよう、`this` はメソッド呼出し中のみ有効とし、関数引数への代入は禁止する設計を明示する。
- **Phase G のファイル内複数クラスは、`rootClass` 後方互換を維持**しつつ、`ClassRegistry` への一般化を行う。一度に置換せず、`rootClass` getter を deprecated 化して一定期間併存させる。
- **ビルド・テスト実行はユーザー指示待ち**。各 Phase 完了時に「ここまで実装した、次のビルド指示を求む」と明示する。

## 想定スライスサイズ

| Phase | 規模感 |
|---|---|
| A | 中（標準ライブラリ API 設計 + 5〜7 関数 + テスト） |
| B | 中（評価器の `obj.method` 経路 + レジストリ拡張 + テスト） |
| C | 小（パーサー拡張 + 評価器診断整形 + テスト） |
| D | 中（値モデル追加 + 既存 variant 訪問の監査 + テスト） |
| E | 中（パーサー + 評価器 + this スコープ + テスト） |
| F | 中（評価器拡張 + メソッド解決順拡張 + テスト） |
| G | 中（パーサー拡張 + 親探索 + 後方互換 + テスト） |
| H | 小（評価器 1 ケース追加 + テスト） |

堅実路線の意図として、Phase A → C → B → D → E → F → G → H の順に進め、各 Phase 完了時点で 1 つのマージ可能な差分にする。
