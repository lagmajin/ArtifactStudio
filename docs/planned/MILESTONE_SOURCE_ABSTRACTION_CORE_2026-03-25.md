# マイルストーン: Core Source Abstraction (`ISource`)

> 2026-03-25 作成

## 目的

将来的に layer の種類で処理を分けるのではなく、**`ISource` を持つかどうか** で入出力を整理する。

このマイルストーンは、アプリ層の layer 実装を触らずに、まず **基盤だけを `ArtifactCore` に置く** ことを目的とする。

狙いは次の通り。

- file source と generated source を同じ契約で扱う
- image / audio / vector / video を「source + layer adapter」に寄せる
- `loadFromPath()` 系の散在を Core 側に集約する
- 保存・再リンク・メタデータ取得を layer 種別より source 中心にする

### 進捗

- Core 側の `ISource` / `FileSource` / `GeneratedSource` は実装済み
- アプリ層の layer は未変更
- layer bridge / persistence / relink の統合は次段階

---

## 方針

### 原則

1. まずは Core に抽象を置く
2. App 層の layer クラス名や UI 責務は変えない
3. 既存の `loadFromPath()` は当面残す
4. `layer type` は表示・編集の都合として残してよい
5. ただし source の真実は Core に寄せる

### 想定する役割分担

- `ArtifactCore`
  - `ISource`
  - `FileSource`
  - `GeneratedSource`
  - source metadata / capabilities / cache key
  - load / reopen / resolve / relink の基盤
- `Artifact` アプリ層
  - 既存 layer クラス
  - 既存 property / inspector / timeline
  - 既存 import / replace UI

---

## Phase 1: Core Source Contract

### 目的

`ISource` の契約を Core に定義する。

### 機能

- source id
- source kind
- source path / uri
- metadata
- capability flags
- load state
- relink state

### 完了条件

- Core 側で source を表現できる
- layer 種別に依存せず source の性質を問い合わせできる
- file / generated の差を契約で表せる

---

## Phase 2: Concrete Sources

### 目的

ファイル入力と生成入力を同じ interface 配下に置く。

### 機能

- `FileSource`
  - disk file
  - path based relink
  - file metadata
- `GeneratedSource`
  - solid / procedural / synthetic data
  - path 非依存
- optional source cache

### 完了条件

- `source->load()` 系で開ける
- `source->kind()` で file / generated を見分けられる
- metadata を layer から剥がせる

---

## Phase 3: Layer Bridge

### 目的

既存 layer を壊さずに source を差し込む。

### 機能

- layer は source を 1 個保持する
- layer 側の `loadFromPath()` は source 生成へ委譲できる
- `sourcePath()` は互換 API として残せる
- render / audio / thumbnail は source から読む

### 完了条件

- アプリ層の layer の外形は変えない
- source を差し替えても layer は同じまま動く
- `Image / Audio / Video / SVG` の扱いを source として統一できる

---

## Phase 4: Persistence and Relink

### 目的

保存・復元・再リンクを source 中心にする。

### 機能

- source descriptor の保存
- file path の再解決
- missing source の検出
- relink candidate の列挙
- generated source の再生成条件

### 完了条件

- layer JSON が source descriptor を持つ
- missing asset 復元が source 単位でできる
- asset browser / project view は既存のままでも source 整理が進む

---

## Phase 5: Capability-Based Dispatch

### 目的

layer type ではなく capability で再利用経路を決める。

### 機能

- `canRasterize()`
- `canProduceAudio()`
- `canProduceVideo()`
- `canProvideThumbnail()`
- `canReloadFromPath()`

### 完了条件

- render / export / preview の if 分岐が layer type 依存から減る
- SVG / image / video / audio の共通処理を増やせる

---

## Phase 6: Optional App Adapter Cleanup

### 目的

Core が安定したあとにだけ、アプリ層の重複ロジックを少しずつ削る。

### 注意

この段階は必須ではない。
まずは Core に `ISource` が定着してから判断する。

---

## 何をしないか

- アプリ層の layer を新しい構造へ一気に書き換えない
- UI 名称を先に変えない
- 既存の `loadFromPath()` を即時廃止しない
- `ImageLayer` / `AudioLayer` / `VideoLayer` を即座に統合しない

---

## 優先順位

### 最優先

1. Core Source Contract
2. Concrete Sources
3. Layer Bridge

### 次点

1. Persistence and Relink
2. Capability-Based Dispatch
3. Optional App Adapter Cleanup

---

## 実装順の提案

1. `ArtifactCore` に `ISource` を置く
2. `FileSource` と `GeneratedSource` を作る
3. `ArtifactAbstractLayer` から source を参照できる最小 bridge を作る
4. `Image / Audio / Video / SVG` を source backend へ寄せる
5. 保存・再リンクを source descriptor に移す
6. アプリ層の重複を後追いで削る
