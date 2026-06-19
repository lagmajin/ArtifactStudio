# Standard Library Replacement Priority Guide

作成日: 2026-06-19  
対象: `ArtifactCore/include/Utils/*`, `ArtifactCore/include/Result/*`, `Artifact/include/Result/*`, `Artifact/include/Composition/*`, `Artifact/include/Asset/*`, `Artifact/include/Layer/*`, `Artifact/include/Project/*`

---

## 1. 目的

このガイドは、`std::` を完全に消すためのものではなく、**どこから専用型へ置き換えると効果が大きいか** を先に決めるためのもの。

優先順位は「テンプレートが表層に出ると事故りやすい」「Qt / module 境界をまたぎやすい」「既存コードへの波及が大きい」の3軸で決める。

---

## 2. 置き換え優先順位

### Priority 1

1. `std::string`
1. `std::string_view`
1. `std::optional<T>`
1. `std::shared_ptr<T>` / `std::weak_ptr<T>`
1. `std::unique_ptr<T>`

理由:
- 文字列と所有権は API 露出の中心で、変換漏れがもっとも起きやすい
- `Result` / `Id` / `Path` / `UniString` の土台になりやすい

### Priority 2

1. `std::vector<T>`
1. `std::array<T, N>`
1. `std::span<T>`
1. `std::function<...>`
1. `std::variant<...>`

理由:
- 領域別 API の表面で最も見かけやすい
- 返り値や callback に直接出ると、ヘッダの見通しが悪くなる

### Priority 3

1. `std::map<K, V>`
1. `std::unordered_map<K, V>`
1. `std::set<T>`
1. `std::unordered_set<T>`
1. `std::filesystem::*`

理由:
- lookup 容器は domain alias があると読みやすくなる
- path / file system は Qt や project 独自の contract に寄せたほうが安全

---

## 3. 推奨する専用型

### 3.1 文字列

- `UniString`
- `String` 系の Qt 境界 wrapper
- `StringLike` concept

使い分け:
- UI と保存境界: `UniString` を優先
- Qt 内部: `QString`
- 実装内部: `std::string` 可。ただし public API では露出を抑える

### 3.2 識別子

- `Id`
- `CompositionID`
- `LayerID`
- 必要なら `AssetID` / `ProjectID` / `SourceID` を追加

使い分け:
- 共有キーや検索キーは `Id` 系に寄せる
- raw string UUID を API に出しっぱなしにしない

### 3.3 パス

- `Path`
- `PathView` 相当が必要なら別設計

使い分け:
- パス解決や resource root の責務は `Path` に閉じる
- file system 操作は path contract を通して行う

### 3.4 Result

- `Artifact::*Result`
- `Status`
- `ErrorCode`

使い分け:
- 単純成功失敗は `bool` でもよいが、呼び出し側が判断材料を必要とするなら `Result`
- 失敗理由が複数ある API は `ErrorCode` を持つ

---

## 4. 変換ルール

1. `QString` と `UniString` の変換点は集約する
1. `Id` と raw UUID の相互変換は集中させる
1. `std::string` への変換は boundary で一回だけにする
1. `std::vector<T>` を返すなら、なぜ専用型にしないかを説明できる場合だけにする
1. callback は `std::function` を直接ばらまかず、意味のある alias を付ける

---

## 5. 実装順

1. `Result` と `ErrorCode` の共通基盤
1. `Id` 系 alias の整備
1. `UniString` と `QString` の変換整理
1. `Path` と source / file contract の整理
1. `vector` / `optional` / `function` の表層縮約

---

## 6. 非推奨

- public API で `std::string` をそのまま多用する
- public API で `std::vector<std::variant<...>>` のようなネストを増やす
- `std::shared_ptr` を意味のない所有契約で広げる
- `std::filesystem` を Qt 専用境界に混ぜる

---

## 7. 関連

- [M-AR-4 Standard Library Replacement Scale-Out](/X:/Dev/ArtifactStudio/docs/planned/MILESTONE_STD_REPLACEMENT_LIBRARY_SCALEOUT_2026-06-19.md)
- [M-AR-2 import std Rollout](/X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)

