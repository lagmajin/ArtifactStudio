# マイルストーン: Artifact* 標準ライブラリ拡張（C++23/26 ギャップ補完）

**最終更新:** 2026-08-28
**ステータス:** Not Started
**優先度:** Medium
**関連:**
- 計画: `~/.commandcode/plans/artifact-cxx23-26-library-gaps.md`
- `ArtifactCore/include/Core/ArtifactFoundation.ixx`（集約モジュール、現状 17 モジュール re-export）
- `ArtifactCore/include/Core/ArtifactArray.ixx` / `ArtifactString.ixx` / `ArtifactTuple.ixx` / `ArtifactOptional.ixx` / `ArtifactMath.ixx` / `ArtifactUtility.ixx` / `ArtifactAlgorithms.ixx`
- `ArtifactCore/include/Utils/Result.ixx`（既存 Result/ErrorContext — ArtifactExpected とは別システム）
- `tests/ArtifactCore/CMakeLists.txt`（明示リスト方式、`artifact_add_test` 1 ターゲット=1 ファイル）
- 既存 `ArtifactCoreArtifactFoundationTest`(`ArtifactFoundationTest.cpp`)
- 仕様ソース: cppreference.com C++26 ページ（2026-03 IS 完了）

## 目的

ArtifactStudio の `Core.ArtifactFoundation` は std 互換ユーティリティを `ArtifactCore` namespace + `artifact*` / `Artifact*` 命名規約で集約している。C++23/26 で標準入りした「自前実装がない or 薄い」型を追加し、同パターンで `Foundation` 経由で 1 ライナー `import` で使えるようにする。**堅実路線**：(1) 既存テストの回帰なし (2) 新規ユニットテスト追加 (3) 既存 std/Artifact 資産との共存（置換しない）。

## 現状(2026-08-28 実測)

### 既存 Artifact* ユーティリティ
- `String / StringView` (SBO 23B), `Array<T>` (capacity 管理), `Tuple<Ts...>`, `Span<T>`, `HashMap/HashSet` (chained bucket, insertion-ordered)
- `Variant` (type-erased), `Function` (SBO 32B, 所有), `Callback` (std::function 薄ラップ), `Ptr<T>` (RefCount ベース)
- `Atomic<T>` (std::atomic 薄ラップ), `Optional<T>` (std::optional type alias), `Chrono::Duration` (自前 nanos), `Random` (QRandom ベース)
- `Foundation.ixx` に 17 モジュール `export import`（HashMap/Atomic/Optional は .cppm 単体）。`Set.ixx` / `Function.ixx` / `Random.ixx` / `Timer.ixx` / `Queue.ixx` は Foundation 未登録

### 設計パターン
- 純粋 template は `.ixx` のみ、非 template ヘッダオンリーは `.ixx` 推奨、std への薄い依存 + 自前 impl は `.cppm`
- 安全性フック: `ArtifactDict::get` は `Optional<V>` 戻り、`tryGet`/`getOr` 推奨パターン
- 名前空間: `ArtifactCore`、クラス `Artifact*` / 関数 `artifact*`、モジュール名 `Core.Artifact*`
- std 薄ラップと自前 type-erasure のハイブリッド

### C++23/26 で未実装の主要標準ライブラリ候補
| 候補 | 標準 | Artifact 価値 |
|---|---|---|
| `std::flat_map / flat_set` | C++23 | 高（キャッシュ局所性、serialization 親和性） |
| `std::inplace_vector` | C++26 | 中（固定容量 SBO 発展系） |
| `std::function_ref` | C++26 | 高（非所有 callable、所有コストなし） |
| `std::expected` | C++23 | 高（Result 強化、エラー値型） |
| `std::philox_engine` | C++26 | 中（決定論的 RNG、replay/並列） |
| Saturation Arithmetic | C++26 | 中（クリッピング多用） |
| `std::string::subview()` | C++26 | 中（既存 mid 拡張） |
| `views::concat` | C++26 | 中（既存 CppLinq 統合） |
| `<meta>` reflection | C++26 | 別マイルストーン（Phase 4 Unity 風シリアライズ用） |
| `<linalg>` / `<simd>` / `<hive>` | C++26 | 除外（glm 競合、用途限定） |

## フェーズ構成

### Phase 1 — エラー値型 / 非所有 callable / 飽和演算（即着手）

#### 1-A. `ArtifactExpected<T, E>`
- 配置: `ArtifactCore/include/Core/ArtifactExpected.ixx`（template のみ）
- 機能: std::expected 互換
  - `hasValue() / value() / error() / valueOr(default) / errorOr(default)`
  - `andThen(fn) / orElse(fn) / transform(fn) / transformError(fn)`
  - `ArtifactOptional<T> toOptional() const`（エラー時 nullopt）
- **デフォルトエラー型は自前 `ArtifactExpectedError`**（`ErrorCode` + `ZeroString` メッセージの軽量 POD）。`Utils::ErrorContext` には依存しない独立システム
- 安全性フック: `value()` は契約違反で `abort`/`assert`、`getOr` 推奨
- Foundation 登録: `export import Core.ArtifactExpected;` 追加

#### 1-B. `ArtifactFunctionRef<Sig>`
- 配置: `ArtifactCore/include/Core/ArtifactFunctionRef.ixx`（template のみ）
- 機能: std::function_ref 互換（非所有、所有コストなし）
  - 構築: `ArtifactFunctionRef<Sig>(callable)` / 関数ポインタ
  - `invoke(args...)` / `operator()(args...)`
  - `isValid() / clear()`
- 内部: `(storage_, invokeFn_)` 2 ポインタ + 呼び出し側所有
- Foundation 登録: `export import Core.ArtifactFunctionRef;` 追加

#### 1-C. `ArtifactSaturation`
- 配置: `ArtifactCore/include/Core/ArtifactSaturation.ixx`（template のみ）
- 機能: C++26 Saturation Arithmetic
  - `addSat<T>(a, b)`, `subSat<T>(a, b)`, `mulSat<T>(a, b)`
  - `saturateCast<To>(from)`（`uint8` 等へのクリップキャスト）
  - `std::is_integral_v<T>` 制約
- 用途: 画像/音声/物理演算のクリップ多用箇所で `std::clamp` の代替
- Foundation 登録: `export import Core.ArtifactSaturation;` 追加

#### Phase 1 完了条件
- 新規 `.ixx` 3 本追加、Foundation に `export import` 3 行追加
- `tests/ArtifactCore/CMakeLists.txt` に `artifact_add_test` 3 ターゲット明示追加
  - `ArtifactCoreArtifactExpectedTest` / `ArtifactCoreArtifactFunctionRefTest` / `ArtifactCoreArtifactSaturationTest`
- 各 I/F について 1〜2 アサート、既存テスト回帰なし
- ビルド・テスト実行はユーザー指示待ち

### Phase 2 — Flat 連想配列 / Inplace Vector

#### 2-A. `ArtifactFlatMap<K, V, Cmp>`
- 内部: `ArtifactArray<ArtifactPair<K, V>>` ソート維持
- I/F: `insert / insert_or_assign / emplace / erase / find / count / contains / lower_bound / upper_bound / equal_range`
- `tryGet(key) -> Optional<V&>`（ArtifactDict 流の安全性フック）
- `Cmp` 既定 `std::less<K>`
- Foundation 登録

#### 2-B. `ArtifactFlatSet<K, Cmp>`
- 内部: `ArtifactArray<K>` ソート維持
- I/F: `insert / erase / find / count / contains / lower_bound / ...`
- Foundation 登録

#### 2-C. `ArtifactInplaceVector<T, Capacity>`
- 固定容量 SBO、ヒープ確保なし
- `tryPushBack -> Optional<T&>` / `pushBack`（容量超過時 `std::bad_alloc`）
- Foundation 登録

#### Phase 2 完了条件
- 新規 `.ixx` 3 本、テスト 3 本、`Foundation` 3 行追加
- ソート維持・二分探索・容量超過の挙動テスト

### Phase 3 — 決定論的 RNG / String subview / Range concat

#### 3-A. `ArtifactPhiloxEngine`
- カウンタベース、並列・決定論的・再現性
- I/F: `seed(Key) / discard(Count) / operator() / generate`
- 用途: 並列レンダリング、replay/スクラブ、ArtifactRandom の Qt 非依存版
- Foundation 登録

#### 3-B. `ArtifactString::subview()` 拡張
- 既存 `String::mid(pos, count)` の `subview` alias
- `StringView::removePrefix(n) / removeSuffix(n)` 追加（安全性: 範囲外 noop）
- 既存 `ArtifactString.ixx` のメソッド追加のみ

#### 3-C. `ArtifactViews::concat`
- C++26 `views::concat` 相当
- `concat(range1, range2, ...)` で連結 range adaptor
- 既存 `CppLinq` との統合
- Foundation 登録

#### Phase 3 完了条件
- 新規 `.ixx` 2 本、テスト 3 本、既存 `ArtifactString.ixx` 拡張、Foundation 2 行追加

## 修正対象ファイル

### 新規作成
- `ArtifactCore/include/Core/ArtifactExpected.ixx`
- `ArtifactCore/include/Core/ArtifactFunctionRef.ixx`
- `ArtifactCore/include/Core/ArtifactSaturation.ixx`
- `ArtifactCore/include/Core/ArtifactFlatMap.ixx`
- `ArtifactCore/include/Core/ArtifactFlatSet.ixx`
- `ArtifactCore/include/Core/ArtifactInplaceVector.ixx`
- `ArtifactCore/include/Core/ArtifactPhilox.ixx`
- `ArtifactCore/include/Core/ArtifactViews.ixx`
- `tests/ArtifactCore/ArtifactExpectedTest.cpp`
- `tests/ArtifactCore/ArtifactFunctionRefTest.cpp`
- `tests/ArtifactCore/ArtifactSaturationTest.cpp`
- `tests/ArtifactCore/ArtifactFlatMapTest.cpp`
- `tests/ArtifactCore/ArtifactFlatSetTest.cpp`
- `tests/ArtifactCore/ArtifactInplaceVectorTest.cpp`
- `tests/ArtifactCore/ArtifactPhiloxTest.cpp`
- `tests/ArtifactCore/ArtifactStringSubviewTest.cpp`
- `tests/ArtifactCore/ArtifactViewsTest.cpp`

### 既存編集
- `ArtifactCore/include/Core/ArtifactFoundation.ixx` — `export import` 計 8 行追加
- `ArtifactCore/include/Core/ArtifactString.ixx` — `subview()` alias、`StringView::removePrefix/removeSuffix` 追加
- `tests/ArtifactCore/CMakeLists.txt` — `artifact_add_test` 計 8 ターゲット追加
- 既存 `ArtifactCoreArtifactFoundationTest` に新規モジュール取得確認を 1 アサート追加

## 既存資産の再利用

- `ArtifactArray<T>` → `ArtifactFlatMap/Set` の内部ソート済み vector
- `ArtifactTuple.ixx` の `Pair<K, V>` → `ArtifactFlatMap<K, V>` の value ペア
- `ArtifactOptional.ixx` → `tryGet / getOr` 戻り型
- `ArtifactMath.ixx` の `artifactClamp` → `saturateCast` の実装
- `ArtifactUtility.ixx` の `artifactForward / artifactMove` → 全新規 template

## AGENTS.md / taste 整合

- C++20 module ルール（cppm-modules）: `#include` は GMF のみ、purview に置かない
- 命名規約（project-code-style）に従う
- 既存 `ArtifactHashMap/HashSet/Function` との整合（共存、置換しない）
- 新規シグナル/グローバルなし
- ビルド・テスト実行はユーザー指示待ち

## リスク

- **Phase 2 のソート済み vector** は自前 `artifactSort`（`ArtifactAlgorithms.ixx` 既存在）を使う。`std::sort` 非依存方針を維持
- **`ArtifactFlatMap::find` は O(log n)**、`HashMap` の O(1) と使い分け必要
- **Phase 3 の Philox** はカウンタ+キーのコア実装 + `discard(N)` の並列シード対応で完結
- **`removePrefix/removeSuffix` は `StringView` 側のみ**、`String` 側は破壊的変更を避ける

## 検証手順

1. ファイル作成後、ユーザーがビルド実行を許可したら `cmake --build build --target ArtifactCore` を実施
2. `ctest --test-dir build -R ArtifactCore --output-on-failure`
3. 既存テスト回帰なし + 新規テスト緑
4. `Insight.md` に実装メモを追記

## 想定スライスサイズ

| Phase | 新規ファイル | 編集ファイル | 規模 |
|---|---|---|---|
| 1 | 3 (.ixx) + 3 (test) | Foundation + test cmake | 中 |
| 2 | 3 (.ixx) + 3 (test) | Foundation + test cmake | 中 |
| 3 | 2 (.ixx) + 3 (test) + String.ixx | Foundation + test cmake | 中 |

**Phase 1 → 2 → 3** の順で着手。各 Phase 完了時点で 1 マージ可能な差分。

## 対象外（明示）

- `<meta>` reflection ベースの Unity 風シリアライズ — 別マイルストーン
- `ArtifactHashMap/HashSet` の置換・統合 — 共存
- `ArtifactOptional` の自前化（std 薄ラップで十分）
- `ArtifactString` の全面書き換え — subview 追加のみ
- std 利用コードの置換 — 別タスク（段階的移行）
- 既存 Result/Optional の expected 化 — 別タスク（共存）
- ビルド・テスト実行（AGENTS.md 制約によりユーザー指示待ち）

## ユーザー確定事項（2026-08-28）

- マイルストーン文書化（本書）
- デフォルトエラー型は自前 `ArtifactExpectedError`（`ErrorCode` + メッセージ軽量 POD）
- テスト登録は `tests/ArtifactCore/CMakeLists.txt` の明示リストへ追加
- Phase 1 から着手、Phase 1 完了後に Phase 2/3 着手を判断
