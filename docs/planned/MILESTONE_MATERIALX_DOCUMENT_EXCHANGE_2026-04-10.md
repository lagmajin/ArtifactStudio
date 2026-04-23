# MILESTONE: MaterialX Document / Exchange Bridge

> 2026-04-10 作成

## 目的

MaterialX を、単なる XML 文字列の保持ではなく、Artifact の material asset / inspector / export に接続しやすい交換形式として扱う。

現状の `ArtifactCore::Material` は `materialXDocument()` を保持できるが、
文書の有無、正規化、import/export、3D material UI との接続までは未整備である。

---

## 背景

MaterialX は、DCC 間で material をやり取りする際の強い共通形式になりうる。
Artifact では既に `MaterialType::MaterialX` と `materialXDocument` の格納先があるため、
ゼロから新しい material system を作るより、既存の `Material` asset に exchange layer を足す方が自然である。

---

## 方針

1. MaterialX を `Material` の派生情報ではなく、明示的な document payload として扱う
2. inspector では「XML の存在」「要点の要約」「外部書き出し」を見せる
3. renderer / shader 側はまず MaterialX を内部 PBR へ落とす bridge として扱う
4. 完全な MaterialX graph evaluator は後段に回す

---

## 既存の土台

- `ArtifactCore::MaterialType::MaterialX`
- `ArtifactCore::Material::setMaterialXDocument()`
- `ArtifactCore::Material::materialXDocument()`
- `ArtifactCore::Material::hasMaterialXDocument()`
- `ArtifactCore::Material::clearMaterialXDocument()`

---

## 実装フェーズ

### Phase 1: Document Presence And Storage

- MaterialX XML の有無を判定できるようにする
- 空 document の扱いを統一する
- material asset の save/load で XML が壊れないようにする

### Phase 2: Inspector Summary

- Material inspector に MaterialX summary を出す
- document size / root node / source kind を表示する
- plain XML の直編集ではなく、まず状態把握を簡単にする

### Phase 3: Import / Export Bridge

- MaterialX XML を import/export できる経路を作る
- Artifact material preset と MaterialX document の相互変換を用意する
- PBR fallback へ安全に落とす

### Phase 4: Renderer / Shader Bridge

- MaterialX から抽出した parameter を renderer に渡す
- 3D layer の material assignment と連携する
- 必要なら shader graph の入口へ拡張する

---

## 非目標

- 完全な MaterialX graph evaluator
- すべての MaterialX node の実装
- いきなり外部 DCC と完全互換になること

---

## 完了条件

- MaterialX document の有無を安全に扱える
- MaterialX XML を保存・復元しても壊れない
- inspector から MaterialX 状態を確認できる
- 将来の MaterialX import/export 入口を追加しやすい

---

## Current Status

- `ArtifactCore::Material` は MaterialX XML を保持できる
- `hasMaterialXDocument()` / `clearMaterialXDocument()` を追加済み
- まだ inspector / export / renderer への橋渡しは未着手

