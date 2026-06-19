# M-AR-4 Standard Library Replacement Scale-Out

作成日: 2026-06-19  
ステータス: Draft  
対象: `ArtifactCore/include/Utils/*`, `ArtifactCore/include/Result/*`, `Artifact/include/Result/*`, `Artifact/include/Composition/*`, `Artifact/include/Asset/*`, `Artifact/include/Layer/*`, `Artifact/include/Project/*`, `Artifact/include/Render/*`, `Artifact/include/Audio/*`, `Artifact/include/Video/*`
位置づけ: `std::` の全面置換ではなく、**表層 API にテンプレートが露出しすぎる領域を順に専用型へ寄せる**。既存の `Utils.String / Utils.Path / Utils.Id / UniString` と各種 `Result` を土台に、事故りやすい境界から拡張する。
参照:
- `docs/planned/MILESTONES_BACKLOG.md` の `M-AR-2 import std Rollout`
- `ArtifactCore/include/Utils/String.ixx`
- `ArtifactCore/include/Utils/StringLike.ixx`
- `ArtifactCore/include/Utils/Path.ixx`
- `ArtifactCore/include/Utils/Id.ixx`
- `ArtifactCore/include/Utils/JsonLike.ixx`
- `ArtifactCore/include/Utils/VectorLike.ixx`
- `ArtifactCore/include/Utils/UniString.ixx`
- `Artifact/include/Result/ArtifactResult.ixx`
- `Artifact/include/Composition/ArtifactCompositionResult.ixx`
- `Artifact/include/Asset/ArtifactAssetResult.ixx`
- `Artifact/include/Layer/ArtifactLayerResult.ixx`
- `Artifact/include/Project/ArtifactProjectResult.ixx`
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md`

---

## 1. 目的

現在のコードベースでは、`std::vector` / `std::optional` / `std::variant` / `std::function` / `std::shared_ptr` / `std::unique_ptr` / `std::filesystem` / `std::map` / `std::unordered_map` が、API 表層や module interface に広く露出している。

これは実装上は便利だが、次の問題を起こしやすい。

- テンプレート引数や allocator、可変テンプレート、SFINAE がヘッダ表面に漏れる
- モジュール境界で依存が太くなる
- 例外・所有権・空値・失敗の表現が領域ごとに揺れる
- UI 側が `std` 型を直接扱いすぎて変換点が増える

この milestone は、`std` を完全に排除するのではなく、**公開 API に出す型を固定化し、内部実装は `std` を使ってもよい** という方針で置換ライブラリを大きくする。

---

## 2. 現状整理

### 2.1 既にある土台

- `Utils.String.Like` で `StringLike` concept がある
- `Utils.Path` がアプリケーションパス・アイコンパス解決を持っている
- `Utils.Id` が `Id / CompositionID / LayerID` と `qHash` を持っている
- `Utils.UniString` が Qt / `std::u16string` / `std::u32string` をつなぐ
- `Artifact.Result` / `Artifact.Composition.Result` / `Artifact.Project.Result` / `Artifact.Layer.Result` など、領域別の result 型が既に存在する
- `ArtifactCore/include/Utils/*Like` が、テンプレート表層露出を抑えるための概念層として機能している

### 2.2 露出が大きい領域

以下は `std` 依存が表層に残りやすい。

- result / status 系
- optional / nullable 系
- small container 系
- string / view / encoding 系
- id / handle / key 系
- file / path / source 系
- callback / action 系
- map / set / lookup 系

### 2.3 先に触るべき理由

- `ArtifactCore` の基盤に寄せれば、アプリ層の変更点が小さくなる
- `Result` と `Id` と `String` は、他の置換型の親になりやすい
- 既存の `M-AR-2 import std Rollout` と衝突せず、むしろ補完できる

---

## 3. 設計方針

### 3.1 基本原則

1. **公開 API から `std::` を減らす**
   - ヘッダの見た目を単純にする
   - テンプレートの引数列を外へ漏らさない

2. **所有と参照を型で分ける**
   - `String` / `StringView`
   - `Buffer` / `BufferView`
   - `Path` / `PathRef`
   - `Result<T>` / `Status`

3. **失敗は明示する**
   - 例外に寄せるか `Result` に寄せるかを領域ごとに決める
   - 暗黙変換で失敗を隠さない

4. **実装は `std` でもよいが、境界は固定**
   - `.ixx` の表面で `std::vector<T>` を直接返さない
   - 内部実装は `std` を使ってよい

5. **Qt との境界を明示する**
   - `QString` / `QByteArray` / `QJsonObject` は boundary type として扱う
   - 変換関数を集中させる

---

## 4. 置換対象の優先順位

### Tier 1: まず固める

- `Result<T>` / `Status`
- `String` / `StringView`
- `Id` / `Handle`
- `Path` / `PathView`
- `Buffer` / `BufferView`

### Tier 2: 表層テンプレートを減らす

- `Array<T>` / `Span<T>`
- `Map<Key, Value>`
- `Set<T>`
- `Function<Signature>`
- `Optional<T>`

### Tier 3: 領域別の専用型

- `AssetKey`, `AssetRef`
- `LayerKey`, `LayerRef`
- `ProjectKey`, `ProjectRef`
- `SourceKey`, `SourceRef`
- `FrameRange`, `TimeRange`, `TimePoint`
- `DecodeResult`, `LoadResult`, `SaveResult`

---

## 5. フェーズ計画

### Phase 1: Core contract cleanup

対象:
- `ArtifactCore/include/Utils/String.ixx`
- `ArtifactCore/include/Utils/UniString.ixx`
- `ArtifactCore/include/Utils/Path.ixx`
- `ArtifactCore/include/Utils/Id.ixx`
- `Artifact/include/Result/*`
- `Artifact/include/Composition/ArtifactCompositionResult.ixx`
- `Artifact/include/Asset/ArtifactAssetResult.ixx`
- `Artifact/include/Layer/ArtifactLayerResult.ixx`
- `Artifact/include/Project/ArtifactProjectResult.ixx`

作業:
- result 型の命名と意味を揃える
- `success/message` だけで済むものと、`error code` を持つものを分ける
- `UniString` / `Id` / `Path` の役割を再確認し、重複する wrapper を整理する
- `ArtifactCore` 側で共通に使う `Status` / `Result` / `ErrorCode` の最小セットを定義する
- `Find*Result` / `Create*Result` / `Remove*Result` のような領域別 result の差分を吸収できる共通命名を作る
- `Id` 系は `CompositionID` / `LayerID` のような domain alias を維持しつつ、保存・検索・hash の入口を揃える
- `UniString` は Qt 境界の文字列箱として維持し、表層で `std::string` を直接返す API を増やさない
- `Path` は path 解決の責務に閉じ、ファイル実体や asset 対応は別型へ逃がさない

Done criteria:
- 領域別 result 型が「bool だけ」ではなく、失敗理由を持つものは明示的に持つ
- `ArtifactCore` の境界型が、Qt と `std` の変換ハブとして機能する
- 新しい公開 API を作るときに `std::string` / `std::vector` をそのまま返さなくてよい状態になる

### Phase 2: Generic container surface

対象:
- `ArtifactCore/include/Utils/VectorLike.ixx`
- `ArtifactCore/include/Utils/JsonLike.ixx`
- `ArtifactCore/include/Utils/StringLike.ixx`
- `ArtifactCore/include/Container/*`
- `ArtifactCore/include/Common/*`

作業:
- `Array` / `Span` / `SmallVector` 相当の薄い表層を追加するか、既存の Qt コンテナとの役割分担を明文化する
- `StringLike` / `VectorLike` を単なる concept ではなく、変換入口として使えるようにする
- container の返り値は `const&` / view / dedicated result のいずれかに寄せる

Done criteria:
- 「公開 API が `std::vector` を返すだけ」の箇所を減らせる
- 代表的な一覧返却や検索 API で、API 表面が `std` テンプレートに埋もれない

### Phase 3: Domain aliases and keys

対象:
- `Artifact/include/Asset/*`
- `Artifact/include/Layer/*`
- `Artifact/include/Project/*`
- `Artifact/include/Render/*`
- `ArtifactCore/include/Media/*`
- `ArtifactCore/include/Video/*`

作業:
- `Id` を基準に、領域別 key/ref alias を整える
- source / asset / layer / project の識別子を混在させない
- `std::shared_ptr` / `std::weak_ptr` の露出を減らし、意味のある alias へ寄せる

Done criteria:
- 重要な参照型が `AssetRef` / `LayerRef` / `ProjectRef` のようなドメイン名で読める
- 呼び出し側が「これは所有か参照か」を名前で判断できる

### Phase 4: Public API shrink

対象:
- `Artifact/include/*`
- `ArtifactCore/include/*`

作業:
- 返り値と引数の `std::optional` / `std::variant` / `std::function` を段階的に整理する
- 失敗しうる API を `Result` 系へ寄せる
- callback 型は明示 alias にまとめる

Done criteria:
- 新規 public API の多くが `std` テンプレート直書きではなくなる
- 例外、null、失敗、空値のルールが領域ごとに統一される

### Phase 5: Documentation and adoption guide

作業:
- `docs/technical/` に置換ライブラリの使い分けガイドを作る
- 既存コードの移行優先順位を明文化する
- 「使うべき型」と「避けるべき型」を一覧化する

Done criteria:
- 新規実装時に迷ったら参照できるガイドがある
- `std` を使ってよい場所と、使うべきでない場所が明確になる

---

## 6. 実装ルール

- 新しい public ヘッダで `std::vector<T>` / `std::map<K, V>` を安易に増やさない
- `std::function` は boundary 用 alias に閉じる
- `std::shared_ptr` / `std::unique_ptr` は所有境界のみに使い、意味のある型名を用意する
- `QString` / `QJsonObject` / `QByteArray` との変換点を散らさない
- `QImage` の hot path 流入は禁止の既存方針を維持する
- `setStyleSheet()` と新規 signal-slot はこのマイルストーンでは扱わない

---

## 7. リスク

1. **互換性コスト**
   - 既存コードが広く `std` 型を使っているため、一気に変えると波及が大きい

2. **ラッパー乱立**
   - 型を増やしすぎると、今度は変換先が増えすぎる

3. **名前の重複**
   - `Result` / `Status` / `Outcome` / `Error` のような名前が散ると逆に読みづらい

4. **境界の曖昧化**
   - 内部実装に閉じるはずの型が、いつの間にか public API へ漏れる

5. **モジュール依存の膨張**
   - `.ixx` にテンプレートや import を足しすぎると、ビルド依存が太くなる

---

## 8. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `M-AR-2 import std Rollout` | 依存削減の逆ではなく、役割分担。実装側では `import std;` を進めつつ、公開境界は専用型へ寄せる |
| `M-CORE-4 Module Hygiene / Build Stabilization` | module boundary の安定化に直結 |
| `M-AR-3 Serialization Cleanup` | result / key / path の統一が保存形式整理に効く |
| `M-APP ApplicationLayer completeness` | app 層の API 表面を単純化できる |
| `M-EXPR-1 / M-EXPR-2` | expression 系の callback / optional / result 整理に関係する |

---

## 9. 推奨順

1. `Phase 1` で result / key / string / path を揃える
2. `Phase 2` で container surface を整理する
3. `Phase 3` で domain alias を固める
4. `Phase 4` で public API を縮める
5. `Phase 5` で移行ガイドを書く

---

## 10. 更新履歴

- 2026-06-19: 初版作成。`Utils/*` と `Result/*` を起点に、標準ライブラリ置換を「表層 API の縮約」として再定義した。
