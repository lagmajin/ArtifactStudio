# マイルストーン: 内蔵軽量VCSとレイヤーバリエーション

> 2026-04-17 作成

## 目的

ArtifactStudio 内部に、Git の代替ではないが、編集履歴を「版」として扱える軽量VCSを導入する。
同時に、レイヤー単位で複数の派生案を保持できるバリエーション機構を追加し、
複製の乱立ではなく、base + override による差分管理へ寄せる。

このマイルストーンの狙いは次の 2 つ。

- project / composition / layer の「意味のある節目」を版として残す
- 1つのレイヤーに対して複数の案を安全に切り替えられるようにする

---

## 背景

現在のアプリには、すでに次の土台がある。

- `UndoManager` による短期操作履歴
- `AutoSaveManager` による recovery point
- `currentProjectSnapshotJson()` による project snapshot
- `SnapshotCompareWidget` の UI 器

しかし、これらは役割が分かれており、次の要望を満たしきれていない。

- 「この編集の節目を残したい」
- 「前の案と今の案を比較したい」
- 「レイヤーの別案を複製ではなく派生として持ちたい」
- 「project 全体の branch と layer 単位の variant を区別したい」

そこで、Undo / Autosave / Versioning を混ぜずに、上位に Revision Ledger を置く。

---

## 設計原則

### 1. Undo と VCS は別物

Undo は「直前の操作を戻す」ための短期履歴。
軽量VCS は「意味のある節目を固定する」ための版管理。

Undo の粒度をそのまま VCS に流用しない。

### 2. Autosave と VCS も別物

Autosave は crash recovery のための安全網。
VCS は人間が見て扱うための版履歴。

### 3. 保存は snapshot 中心

初期段階では、差分圧縮よりも snapshot の正確性を優先する。
まずは正規化済み JSON を版の実体とし、後から delta 保存へ拡張できる形にする。

### 4. レイヤーバリエーションは clone ではなく override

レイヤーの別案は、丸ごと複製ではなく、
base layer に対する差分を保持する VariantSet として扱う。

### 5. 既存の通知経路を壊さない

新しい公開 signal / slot を増やさず、既存の service と pull 型更新を優先する。

---

## 用語

- **Revision**: 保存された 1 つの版
- **Ledger**: Revision を管理する台帳
- **Branch Ref**: 特定 Revision を指す名前付き参照
- **Checkpoint**: 意味のある節目として明示的に残した Revision
- **Variant**: レイヤーの派生案
- **VariantSet**: 1つの base layer に紐づく Variant の集合
- **Override**: base に対して差分で上書きする値

---

## 全体アーキテクチャ

### Layer 1: Undo 層

既存の `UndoManager` をそのまま使う。

- 操作の即時反映
- 直前操作の undo / redo
- UI の操作感維持

### Layer 2: Recovery 層

既存の `AutoSaveManager` を使う。

- crash 後の復旧
- 定期的な snapshot 退避
- 保存失敗時の救済

### Layer 3: Revision Ledger 層

新規に導入する軽量VCS。

- checkpoint
- branch
- compare
- restore
- variant 由来の派生管理

### Layer 4: Variant 層

レイヤーの見た目や属性の別案を持つ。

- base layer を維持
- variant ごとの差分だけ保存
- 活性化した variant を effective state として適用

---

## Revision Ledger のデータモデル

### RevisionNode

各 revision は次の情報を持つ。

- `revisionId`
- `parentRevisionId` または `parentRevisionIds`
- `scope`
- `name`
- `message`
- `timestamp`
- `snapshotRef`
- `contentHash`
- `origin`
- `tags`

### Scope

対象は 4 種類に分ける。

- `Project`
- `Composition`
- `Layer`
- `Variant`

### BranchRef

branch は「履歴そのもの」ではなく「head を指すラベル」として扱う。

- `main`
- `work`
- `review`
- `client-a`

### Ledger Store

保存形式は当面、次の構成を想定する。

- project 本体: 既存 JSON
- revision 台帳: 付属 JSON
- snapshot payload: 正規化済み JSON
- preview thumb: 任意

---

## ストレージ方針

### Phase 1: snapshot 保存

最初は full snapshot を基本とする。

利点:

- 実装が単純
- restore が確実
- diff 失敗時の復旧が容易

### Phase 2: 差分保存

後から必要になったら、以下を追加する。

- JSON patch
- property-level diff
- layer-level structural diff

### Phase 3: 圧縮 / GC

履歴が増えたら、古い revision の圧縮や間引きを行う。

- checkpoint は保持
- 中間 revision は要約可能
- preview thumb は別キャッシュ化

---

## レイヤーバリエーション設計

### 基本方針

1つのレイヤーに対して、複数の variant を保持する。
variant は base layer の状態を丸ごと持たず、差分だけ持つ。

### LayerVariant

各 variant は次の値を持つ。

- `variantId`
- `baseLayerId`
- `parentVariantId`
- `displayName`
- `notes`
- `overrides`
- `previewThumb`
- `isActive`

### Override 対象

初期段階では、次の項目を対象にする。

- `layer.name`
- `layer.visible`
- `layer.locked`
- `layer.solo`
- `layer.shy`
- `layer.opacity`
- `layer.blendMode`
- `layer.transform`

次の段階で拡張する。

- source / footage
- effect stack
- masks
- in/out points
- parent relation

### Effective State

variant 適用時は、

`effectiveState = baseLayer + activeVariantOverrides`

として解決する。

---

## 版と Variant の関係

この設計では、次のように役割を分ける。

- **Revision**: project / composition / layer の節目
- **Variant**: 1つの layer の複数案

つまり、

- revision は「いつの状態か」
- variant は「どの案か」

を表す。

同じ layer の variant を切り替えることは、revision を生やす契機にはなるが、variant 自体は revision ではない。

---

## UI 設計

### SnapshotCompareWidget の拡張

既存の器を次の用途へ発展させる。

- compare
- diff
- restore
- branch from snapshot
- create variant from snapshot

### Project Manager

Project の履歴管理を表示する。

- branch list
- checkpoint list
- recent revisions
- recovery points

### Inspector

選択中レイヤーに対して variant を扱う。

- variant selector
- create variant
- duplicate as variant
- restore variant
- promote variant to active

### Timeline / Context

必要であれば、現在の revision / branch / variant を状態表示する。

---

## 既存機能との接続

### UndoManager

- undo / redo は今のまま維持する
- revision の作成条件は別に持つ
- saved state の比較に version counter を利用できる

### AutoSaveManager

- recovery point は crash recovery 専用
- revision ledger とは保存先と責務を分ける

### Project serialization

- 既存 project JSON を壊さない
- revision ledger は付属データとして追加する
- 未知の revision metadata は破棄せず保持する

### SnapshotCompareWidget

- compare UI の実体として使う
- branch / variant の起点にも使う

---

## 実装フェーズ

### Phase 1: Revision Ledger Core

目的: project snapshot を版として記録できるようにする。

作業項目:

- `RevisionNode` / `BranchRef` / `LedgerStore`
- snapshot の登録
- branch の作成
- restore
- list / lookup

完了条件:

- project の節目を保存できる
- 版一覧を取得できる
- 任意 revision に戻せる

### Phase 2: Layer Variant Core

目的: layer の派生案を保持できるようにする。

作業項目:

- `LayerVariantSet`
- `LayerVariant`
- override apply / extract
- active variant 切り替え

完了条件:

- 1 layer に複数 variant を持てる
- variant の切替で state が再現できる

### Phase 3: UI Integration

目的: ユーザーが版と variant を見分けて扱えるようにする。

作業項目:

- SnapshotCompareWidget 実装
- Project Manager への履歴表示
- Inspector への variant 操作追加

完了条件:

- compare / restore / branch が操作できる
- variant の作成と切替ができる

### Phase 4: Compact Storage

目的: 容量と性能を整える。

作業項目:

- revision compression
- preview thumbnail cache
- diff 保存
- old revision cleanup

---

## 実装順序

### Step 1: Revision の最小台帳を作る

最初に作るのは「保存できる」「一覧できる」「戻せる」の 3 つだけ。

- `RevisionNode` の定義
- `RevisionLedger` の in-memory 管理
- `LedgerStore` の JSON 読み書き
- snapshot 登録
- revision 一覧取得
- 任意 revision の restore

この段階では diff や branch 合流は作らない。

### Step 2: Project 単位の checkpoint を置く

project 全体の checkpoint を作成できるようにする。

- manual checkpoint
- auto checkpoint
- save point / recovery point の区別
- UndoManager の version counter との対応づけ

### Step 3: BranchRef を追加する

revision の head を指す名前付き参照を追加する。

- branch 作成
- branch rename
- branch delete
- branch head の移動

### Step 4: LayerVariantSet を追加する

レイヤー単位の別案を扱う。

- variant 作成
- variant 選択
- variant の override 解決
- active variant の切替

### Step 5: UI をつなぐ

既存の器を機能化する。

- SnapshotCompareWidget
- Project Manager の revision 表示
- Inspector の variant selector

### Step 6: 保存最適化

必要になったら圧縮と差分保存を入れる。

- delta 保存
- preview cache
- 古い中間 revision の間引き

---

## Phase 1 型設計

Phase 1 では、まず project 単位の revision を安全に保存・復元できる最小セットを定義する。
ここでは layer variant はまだ実データを持たず、後続フェーズのための識別子だけを予約する。

### 1. 基本型

#### `RevisionId`

revision を一意に識別する値。

- UUID ベース
- 表示用 string とは分離
- project 内で安定して比較できる

#### `BranchId`

branch / ref を一意に識別する値。

- 人間向けの `main` / `work` とは別に内部 ID を持つ
- rename に耐える

#### `RevisionScope`

revision の対象範囲。

- `Project`
- `Composition`
- `Layer`
- `Variant`

Phase 1 は `Project` のみを主要対象にする。

### 2. RevisionNode

revision 1 件の本体。

持つべき項目:

- `revisionId`
- `scope`
- `name`
- `message`
- `timestamp`
- `parentRevisionId`
- `snapshotFormat`
- `snapshotRef`
- `contentHash`
- `sourceVersion`
- `tags`

Phase 1 では `parentRevisionId` を 1 本に限定する。
merge commit 相当は後回しにする。

### 3. RevisionSnapshot

実体として保存される snapshot のメタ情報。

- `formatVersion`
- `payloadBytes`
- `payloadHash`
- `payloadKind`

payload は当面 JSON 文字列でよい。

### 4. BranchRef

名前付き参照。

- `branchId`
- `displayName`
- `headRevisionId`
- `baseRevisionId`
- `createdAt`
- `updatedAt`

Phase 1 では branch は「head を指すラベル」として扱う。

### 5. RevisionLedger

Revision を管理する中心クラス。

責務:

- revision の追加
- branch の参照管理
- lookup
- restore 用の解決
- scope ごとの一覧

Phase 1 の API は小さく保つ。

- `addSnapshot(...)`
- `getRevision(...)`
- `listRevisions(scope)`
- `createBranch(...)`
- `moveBranchHead(...)`
- `restoreRevision(...)`

### 6. LedgerStore

永続化層。

責務:

- ledger の save / load
- JSON の version 管理
- 破損時の安全な失敗

Phase 1 では 1 ファイル構成を優先する。

- `project.json`
- `project.ledger.json`

必要なら later phase で分割する。

### 7. SnapshotAdapter

既存 project JSON を revision 用に正規化する薄いアダプタ。

責務:

- `ArtifactProject::toJson()` を revision payload に流す
- `fromJson()` で restore する
- version 差分を吸収する

この層は、project の内部構造を revision 層に漏らさないための防波堤になる。

---

## Phase 1 の保存フォーマット案

### project.json

既存の project 保存形式を維持する。

### project.ledger.json

revision 台帳を保持する付属ファイル。

想定項目:

- `ledgerVersion`
- `currentBranchId`
- `branches`
- `revisions`
- `activeRevisionId`
- `latestCheckpointId`

### revision payload

revision 1 件ごとの snapshot。

Phase 1 の payload は次のいずれか。

- project JSON の全文
- 正規化済み project JSON
- 圧縮前の生 JSON

推奨は「まず全文 JSON」。  
これは restore の信頼性を上げるため。

---

## Phase 1 JSON スキーマ案

### ルート構成

`project.ledger.json` のルートは、以下の 4 ブロックに分ける。

- `ledgerMeta`
- `branches`
- `revisions`
- `checkpoints`

この分割により、検索しやすさと将来の拡張性を両立する。

### ルート例

```json
{
  "ledgerMeta": {
    "ledgerVersion": 1,
    "projectId": "b4d8a0e3-7d4a-4ff5-9d39-3f7d5b2f2f63",
    "currentBranchId": "branch-main",
    "activeRevisionId": "rev-000001",
    "latestCheckpointId": "rev-000001",
    "createdAt": "2026-04-17T12:34:56Z",
    "updatedAt": "2026-04-17T12:34:56Z"
  },
  "branches": [
    {
      "branchId": "branch-main",
      "displayName": "main",
      "headRevisionId": "rev-000001",
      "baseRevisionId": "",
      "createdAt": "2026-04-17T12:34:56Z",
      "updatedAt": "2026-04-17T12:34:56Z"
    }
  ],
  "checkpoints": [
    "rev-000001"
  ],
  "revisions": [
    {
      "revisionId": "rev-000001",
      "scope": "Project",
      "name": "Initial checkpoint",
      "message": "Created from current project snapshot",
      "timestamp": "2026-04-17T12:34:56Z",
      "parentRevisionId": "",
      "snapshotFormat": "project-json-v1",
      "snapshotRef": "snapshots/rev-000001.json",
      "contentHash": "sha256:...",
      "sourceVersion": 3,
      "tags": ["checkpoint", "initial"]
    }
  ]
}
```

### ledgerMeta

台帳全体の管理情報。

- `ledgerVersion`: 台帳フォーマットの版番号
- `projectId`: project を識別する安定 ID
- `currentBranchId`: 現在選択中の branch
- `activeRevisionId`: 現在の head revision
- `latestCheckpointId`: 最後に明示保存した checkpoint
- `createdAt`: 台帳作成時刻
- `updatedAt`: 台帳更新時刻

### branches

branch の一覧。

各要素は次を持つ。

- `branchId`
- `displayName`
- `headRevisionId`
- `baseRevisionId`
- `createdAt`
- `updatedAt`

`baseRevisionId` は任意で、branch 元の起点を示す。
Phase 1 では空文字を許容する。

### checkpoints

人間が復帰点として扱いたい revisionId の一覧。

- 配列は revisionId のみ
- 重複禁止
- `revisions` に存在しない ID は不可

### revisions

revision の本体一覧。

各要素は次を持つ。

- `revisionId`
- `scope`
- `name`
- `message`
- `timestamp`
- `parentRevisionId`
- `snapshotFormat`
- `snapshotRef`
- `contentHash`
- `sourceVersion`
- `tags`

### snapshot payload

`snapshotRef` が指す payload は、Phase 1 では project JSON の全文を推奨する。

```json
{
  "kind": "project",
  "format": "project-json-v1",
  "payload": {
    "name": "My Project",
    "author": "Artist",
    "version": "1.1",
    "compositions": [],
    "projectItems": []
  }
}
```

ここでの `payload` は、既存の `ArtifactProject::toJson()` の結果をそのまま格納してよい。

### Variant の予約フィールド

Phase 1 では Layer Variant の実体は作らないが、将来の拡張のために予約フィールドを許容する。

- `variantId`
- `baseLayerId`
- `parentVariantId`
- `displayName`
- `notes`
- `overrides`

ただし、Phase 1 の保存には必須ではない。

---

## Phase 1 ルール

### 1. revisionId は再利用しない

削除した revisionId を再利用しない。

### 2. branchId は rename 可能

表示名変更と内部 ID は分離する。

### 3. checkpoint は branch と独立

checkpoint は branch head と同義ではない。
1つの branch に複数 checkpoint が存在してよい。

### 4. snapshotRef は相対参照を優先

保存場所を移しやすくするため、絶対パスではなく相対参照を使う。

### 5. restore は snapshot 主導

ledger から直接 state を復元するのではなく、snapshot payload を再読込する。

### 6. 破損時は本体を優先

ledger が壊れていても project 本体の読込は続行する。

---

## Phase 1 のクラス配置案

### ArtifactCore 側

まずは純粋データと台帳ロジックをここに置く。

- `RevisionId`
- `BranchId`
- `RevisionNode`
- `BranchRef`
- `RevisionLedger`
- `LedgerStore`

### Artifact 側

UI と project 連携をここに置く。

- `ProjectRevisionService`
- `SnapshotCompareWidget` の実装
- Inspector / Project Manager への接続

### 置かないもの

Phase 1 では次を作らない。

- layer variant の実体
- diff merge
- compact storage
- 共同編集同期

---

## Phase 1 の完了条件

- project snapshot を revision として登録できる
- revision を一覧できる
- branch ref を作成できる
- 任意 revision を restore できる
- 既存 autosave と undo の責務を壊さない

---

## Phase 1 の実装メモ

- `UndoManager::currentVersion()` は ledger のトリガー判定に使うが、revision ID そのものにはしない
- snapshot は project 本体の JSON から作る
- 破損した ledger は project 本体の読込を止めない
- restore は「今の project に上書き」ではなく「snapshot を再読込」方式を優先する
- UI からはまず `compare` と `restore` を出し、variant 操作は後続に回す

---

## 非目標

- 完全な Git 互換
- 分散協調編集の同期基盤
- マージコンフリクト解決の全面実装
- 全レイヤーを variant 化すること
- Undo を VCS の代替にすること

---

## リスク

### 1. 保存データの肥大化

snapshot 中心のため、revision が増えると容量が増える。

対策:

- checkpoint ベースの保持
- 古い中間 revision の圧縮
- thumb の別キャッシュ化

### 2. UI が複雑化する

branch / checkpoint / variant が混ざると分かりにくくなる。

対策:

- Project は revision
- Layer は variant
- 表示ラベルを明確に分ける

### 3. Undo と version の意味が重なる

version counter をそのまま版と誤解しやすい。

対策:

- UndoManager の version は内部カウンタとして扱う
- ledger の revisionId は別体系にする

### 4. 差分適用の順序が壊れる

variant の override が property 依存を壊す可能性がある。

対策:

- effective state の適用順を固定する
- property group ごとに順序を定義する

---

## 成功条件

- project の意味ある節目を revision として残せる
- snapshot から restore できる
- branch を作れる
- layer に複数 variant を持てる
- variant 切替で見た目と主要属性を再現できる
- Undo / Autosave / VCS の責務が混ざらない

---

## 関連

- `docs/planned/MILESTONE_SESSION_LEDGER_RECOVERY_WORKSPACE_2026-04-09.md`
- `docs/planned/MILESTONE_LAYER_COMPONENT_SYSTEM_UNITY_LIKE_2026-04-08.md`
- `Artifact/src/Undo/UndoManager.cppm`
- `Artifact/include/Undo/UndoManager.ixx`
- `Artifact/src/AppMain.cppm`
- `Artifact/include/Project/ArtifactAutoSaveManager.ixx`
- `Artifact/src/Widgets/ArtifactSnapshotCompareWidget.cppm`

## Current Status

2026-04-17 時点では設計段階。
まずは Revision Ledger と Layer Variant の責務分離を固め、その後に UI へ接続する。
