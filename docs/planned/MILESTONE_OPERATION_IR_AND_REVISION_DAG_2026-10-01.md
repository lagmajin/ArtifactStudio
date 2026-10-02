# マイルストーン: Operation IR 正本化と Revision DAG

**最終更新:** 2026-10-01

**ステータス:** Not Started

## 目的

長期的な設計である「Git らしい仕組み」を、**新しい VCS を追加する**のではなく、
既存の二系統（永続化用 `serialize()` / wire 用 `buildCollaborationOperation()`）を
一本の正本に畳むことで実現する。

本マイルストーンは `MILESTONE_LIGHTWEIGHT_VCS_AND_LAYER_VARIANTS_2026-04-17.md` の
後継として、同文書/design の snapshot 中心前提を置き換える。

---

## 背景と現状（2026-10-01 実測）

### 既に動いているもの

| 領域 | 実装 | 状態 |
|---|---|---|
| Revision 台帳・保存・復元・diff | `Artifact/src/Project/ArtifactRevisionService.cppm` | 動作 |
| Snapshot 比較 UI | `Artifact/src/Widgets/ArtifactSnapshotCompareWidget.cppm` | 動作 |
| Undo 永続化 | `Artifact/include/Undo/UndoManager.ixx` の `canSerialize()` / `serialize()` / `deserialize()` | 全コマンドに枠組みあり |
| 共同編集 operation | `ArtifactCore/src/Collaborate/CollabOperations.cppm` に 34 種の型付き operation | 動作 |
| WebSocket 同期・ロック | `ArtifactCore/src/Network/CollaborationWebSocket.ixx` | 動作 |

つまり「版を残す」「差分を出す」「履歴を復元する」は**既に成立している**。
足りないのは Git の残り（分岐・合流・対象を絞った差分）でなく、
その下にある **データの持ち胯** である。

### 二系統の符号化（最大の負債）

`UndoCommand` は現在、同じ事実を二種類の符号化で持っている。

- `Artifact/include/Undo/UndoManager.ixx:113-117` — `commandType()` / `canSerialize()` /
  `serialize()` / `deserialize()`（永続化用）
- `Artifact/include/Undo/UndoManager.ixx:100-109` — `buildCollaborationOperation()`
  （wire 用、`type` + `layerId` + `payload` を生成）
- `Artifact/include/Undo/UndoManager.ixx:85-97` — `collaborationTargetLayerIds()` /
  `collaborationTargetScopeResolved()` / `collaborationDispatchCommand()`（送信対象の解決）

`:98-99` のコメントはこの分離を意図として明記している
（「wire encoding is separate from durable Undo persistence」）。
しかし長期的には、これは**同じ編集内容を 2 か所で定義し直す**構造になっている。

二者の完成度は対等ではない。実測:

- `buildCollaborationOperation()` の実装は `Artifact/src/Undo/UndoManager.cppm` に **17 件**
- `canSerialize()` の実装は `Artifact/include/Undo/UndoManager.ixx` に **74 件**

つまり **wire IR のほうが永続化より壊れている**。
「IR を正本に昇格する」場合、先に埋めるべきは IR の空白であり、IR を消すではない。

---

## 設計原則

### 1. 三つの版の層を混在させない

| 層 | 問い | 既存 |
|---|---|---|
| Undo | 直前の操作を戻す | `UndoManager` |
| Recovery | クラッシュから戻す | `ArtifactAutoSaveManager` / `SessionLedger` |
| Revision | 意味のある節目を版にする | `ArtifactRevisionService` |

前マイルストーン（`MILESTONE_LIGHTWEIGHT_VCS_AND_LAYER_VARIANTS_2026-04-17.md`）の
原則 1〜3 は引き続き有効であり、本マイルストーンでも引き継ぐ。

### 2. ID は差分のキーとして使える

実測で確認済み。`LayerID` は 128bit UUID（`ArtifactCore/include/Utils/Id.ixx:20-50`）で、
保存・復元が完全対称（`Artifact/src/Layer/ArtifactLayerFactory.cppm:599-609`、
`Artifact/src/Composition/ArtifactAbstractComposition.cppm:5909-5911`）。

**ただし次の制約を必ず符号化ルールに反映すること。**

- duplicate は必ず新 UUID を発行する（`Artifact/src/Project/ArtifactProject.cppm:1851`。
  `copyLayerProperties` は `id` をコピーしない）
- copy/paste は JSON の `id` キーを物理削除してから生成する
  （`Artifact/src/AppMain.cppm:6046-6050`、`Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm:420-422`、
  `Artifact/src/Application/ArtifactProjectBundleIpc.cppm:129-131`、
  `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:3300-3302`）

→ **差分は「ID 集合の等価性」ではなく `(compositionId, layerId, add/remove)` の
イベント列として表現する。** 位置ベースや ID 集合の全量突き合わせは破綻する。

### 3. 差分は ID 照合で行う（配列インデックスで比較しない）

現状の `appendDiffRecursive`
（`Artifact/src/Project/ArtifactRevisionService.cppm:91-143`）は
配列を `for (i = 0; i < maxCount; ++i)` で位置ペアする（`:120-127`）。
レイヤー1枚を削除すると以降がすべて `modified` に化ける。
日常の「レイヤー並べ替え」「レイヤーを間引く」が頻出の DCC では diff として使えない。

---

## 既存の欠陥（優先度順）

### D1. ledger の全量書き直しが O(n²)

`saveLedger()`（`Artifact/src/Project/ArtifactRevisionService.cppm:306-353`）は
**全 revision レコードを毎回シリアライズして書き直す**。
自動コミットの間隔は `autoCommitDelayMs_ = 1500`（`:169`）。
コミット数 n に対して書き込み量は O(n²)。

長いセッションでは、まずここにディスク容量の圧迫が現れる。
スナップショットのデルタ化が解決できる問題はこれだが、
**ledger 自体は O(1) 追記へ改めることで切り分けられる。**

### D2. diff の位置比較（原則 3）

### D3. 履歴が完全に線形

`makeRecord()` の `record.parentId = headRevisionId_`（`:361`）と
`restoreRevision()` の `impl_->headRevisionId_ = revisionId`（`:563`）が
常に単一親を指す。
branch は `"branch"` という**タグ文字列に退避するだけ**
（`Artifact/src/Widgets/ArtifactSnapshotCompareWidget.cppm:459`）で、DAG ではない。

### D4. merge が存在しない

`diffRevisions()` は2本の比較のみで、合成する API が無い。
かつ merge は VCS だけの必要ではない。
共同編集側が既に merge を必要としている（下記）。

### D5. 共同編集は「検出するが解決しない」

競合解決は CAS（compare-and-set）による**拒否**のみ。
`docs/analysis/COLLABORATION_IMPLEMENTATION_AUDIT_2026-08-22.md:59` に
「競合検出は行うが、自動 merge や競合解決 UI はない」と明記。
`ArtifactCore/src/Collaborate/` に OT / CRDT / LWW の実装は無い。

**これが分岐を決める。** merge は VCS 側のオプションではなく
共同編集側の必須要件**でもある**。
そのため merge 機構を VCS と collab で二重に実装することは避けたい。

---

## 方針: Operation IR の正本化

`buildCollaborationOperation()` の生成物（`type` + `layerId` + `payload`）を
唯一の編集記述（IR）とし、その上に VCS と collab を載せる。

理由:

- collab は既に operation を送受信している（導入済み）
- merge が要る.Consumed も同じ operation 列に対して書ける
- snapshot 比較が「全量 JSON 比較」から「commit 列の再演繹」に変わり、
  デルタ化が自然に解決する

代价: IR の空白を埋める必要がある（17 実装 → 全コマンド）。
これは必要コストであり、D2 の diff 修正も同時に解決できる。

### 未解決の判断（Phase 1 前に決める）

- inverse operation を IR に持たせるか。Undo との二重定義になるか
- `serialize()` を IR の永続化へ寄せるか、`serialize()` を廃止するか
- 既存 snapshot 履歴の移行はどうするか（現状 `AppDataLocation` 配下）

---

## 実装フェーズ

### Phase 0: 事実の固定（小さく、コード変更なし）

- `buildCollaborationOperation()` 未実装コマンドの一覧
- `canSerialize()` 未実装コマンドの一覧
- `INTERNAL_VCS_ARCHITECTURE.md` の記述修正（下記「文書の訂正」参照）

完了条件: 2つの一覧が `file:line` で提示できる。

### Phase 1: diff を ID 照合へ（2026-10-01 実装）

**変更ファイル:** `Artifact/src/Project/ArtifactRevisionService.cppm`

- `appendDiffIdentityAware()` を追加（`:127`）
- `appendDiffRecursive()` の配列分岐を位置比較から ID 照合へ差し替え
- `jsonIdentityOf()`（`:94`）— `id` があれば `id:<uuid>`、
  無ければ `assetId` で `asset:<uuid>` を返す。どちらも無ければ空
- `jsonDisplayName()`（`:113`）— パスの可読性用に `name` を取得

パスの表記は `compositions{id:…:Main}.layers{id:…:Mid}.transform.x` のように
ID とレイヤー名を添える。UI 側の
`ArtifactSnapshotCompareWidget.cppm:140-155` が読むキーは
`path` / `change` / `beforeText` / `afterText` の 4 つのみで、
**変更していない**ため UI の修正は不要。

ID を持たない配列（keyframes、effects 配列など）は
従来どおり位置比較にフォールバックする。

**挙動検証（`temp/diffsim.py` で同等のロジックを Python 実装として確認）**

| ケース | 結果 |
|---|---|
| L2 の x だけ変更 | `…layers{id:L2:Mid}.transform.x` のみ 1 件 |
| L2 削除 + L9 追加 | `removed …{id:L2:Mid}` と `added …{id:L9:New}` の 2 件のみ |
| 並び替えのみ | 差分 0 件（旧実装では 3 件すべて modified になっていた） |
| 完全に一致 | 差分 0 件 |
| アセット 1 本のリリンク | `…sourceRegistry{id:a1}.path` のみ |

**未実施:** ビルドと実機確認は行っていない。
GMF に `QHash`（`:10`）と `QStringList`（`:17`）の include を追加済み。

完了条件: レイヤー1枚削除しても、それ以外のレイヤーに `modified` が出ない。
→ **ロジックとしては満たす。ビルド確認は未実施。**

### Phase 2: ledger の追記化（D1）— 2026-10-01 実装

**変更ファイル:** `Artifact/src/Project/ArtifactRevisionService.cppm`

head ポインタはコミットと復元のたびに変わるため全量書き直しが避けられない。
そこで **履歴と head を分離**し、履歴側だけを追記専用にした。

| ファイル | 役割 | 書き込み回数 |
|---|---|---|
| `ledger.jsonl`（新規） | 1 行 1 レコードの履歴 | 追記 1 回 |
| `head.json`（新規） | `headRevisionId` のみ | 全量（小さいので O(1)） |
| `ledger.json`（旧） | 読み取り専用の移行元 | 触らない |

- `saveLedger()` を削除し、`appendRevisions()`（`:564`）と `saveHead()`（`:436`）に分割
- `commitCurrentProject()`（`:775`）は**新規レコード 1 件だけ**を追記する
- `restoreRevision()`（`:814`）は `saveHead()` のみ
- `recordFromJson()` / `recordToJson()`（`:369` / `:398`）で
  新旧いずれの形式でも同じレコード表現を使えるようにした

**旧形式からの移行**

`loadLedger()`（`:503`）は新しい `ledger.jsonl` が空だった場合のみ
旧 `ledger.json` を読む（`loadLegacyLedger()` `:458`）。
移行後、履歴を `ledger.jsonl` に書き出し、`head.json` を作る。
旧ファイルは削除せず残す（移行元として保持する）。

**クラッシュ耐性（検証済み）**

追記ファイルは書き込み途中のクラッシュで**末尾が不完全な行**を残し得る。
読み込み側は最後の完全な改行まで adopt し、不完全な行だけ捨てる
（`:512-517`）。

さらに重要な点として、**不完全な行の直後へ追記すると
書きかけの行と新しい行が結合して両方が失われる**。
そのため `appendRevisions()`（`:594-609`）は追記前に
末尾 1 バイトを読み、改行でなければ改行を補ってから追記する。

`temp/ledgertest.py` で同等のロジックを検証:

| ケース | 結果 |
|---|---|
| 5 件追記して読み込み | 5 件 |
| 6 件目の途中でクラッシュ（模擬） | 5 件（先行分は保持） |
| その状態から次のコミット | **6 件**（r6 が復帰） |
| 200 件目の 1 コミットあたりの増加バイト | 58 バイト（履歴長に依存しない） |

完了条件: コミット数が多い状態でも ledger 書き時間が線形に伸びない。
→ **満たす（1 コミットあたり約 58 バイトの追記のみ）。ビルド確認は未実施。**

**未確認:** 旧 `ledger.json` からの移行は実データで試していない。
移行時に旧形式が壊れている場合のフォールバックは「履歴なし」で開始する。

### Phase 3: IR 正本化 （2026-10-01 途中経過）

**変更ファイル:**
- `Artifact/include/Undo/UndoManager.ixx`（宣言）
- `Artifact/src/Undo/UndoManager.cppm`（実装）

**追加した 5 つ**（Phase 0 で「AppMain が文字列分岐していた」5 つの内、
`ClonerTransformStackSnapshotCommand` は既に実装済みのため対象外）:

| コマンド | operation type | 実装位置 |
|---|---|---|
| `SetLayerPropertyValueCommand` | `property.set` | `:4126` |
| `SetLayerPropertyKeyframesCommand` | `property.keyframes` | `:4390` |
| `SetLayerPropertyExpressionCommand` | `property.expression` | `:4355` |
| `LayerComponentDescriptorSnapshotCommand` | `layer.components` | `:1287` |
| `CloneEffectorStackSnapshotCommand` | `layer.stack`（`stackKind: cloneEffectors`） | `:1348` |

すべて既存の `ChangeLayerOpacityCommand::buildCollaborationOperation`（`:8308` 付近）
と同じ形式（`action` が `undo` なら `before`/`after` を反転）。

**実装中に見つけた注意**

keyframe の payload は `serialize()` と**同一のエンコードでなければならない**。そうでなければ
リモート側が情報損失を受けます。当初 `frame` と `value` のみを持つ
簡略エンコードにしたところ、`timeValue` / `timeScale` / `interpolation` /
ベジェの `cp1_x`〜`cp2_y` / `roving` / `anchor` / `colorLabel` が落ちるため、
`serialize()` の lambda と同じ形に揃えました（`:4402-4431` にコメントあり）。

**検証（静的のみ）**

- 宣言 22 件 = 実装 22 件（欠落・余剰なし）
- 生成 payload 8 種（5 コマンド × push/undo）が
  `CollabOperations.cppm` のバリデータの要求キーを満たす
  （`temp/payloadcheck.py` で確認後削除）

### Phase 3 完了: AppMain の一本化（2026-10-01）

**変更ファイル:** `Artifact/src/AppMain.cppm`（`-106 行` 程度）

単発の編集は `buildCollaborationOperation()` の戻り値だけを使う形になり、
`AppMain` に command type の文字列比較が残らなくなった。

削除したもの:

- `singlePropertyCommand` / `singleKeyframeCommand` / `singleExpressionCommand` /
  `singleComponentsCommand` / `singleStackCommand` の 5 変数（`:441-457`）
- これらの payload 組み立て（`keyframePayload` / `expressionPayload` /
  `componentsPayload` / `stackPayload` と layer Id 変数）
- request 組み立て側の 4 個の `else if`

**残した东西と理由**

- `MacroUndoCommand` の子集約（`:527-556`）。
  マクロは複数の子編集を1つの operation にまとめる必要があり、
  IR は1コマンド1 operation の形なので、ここは `serialize()` の
  `children` 配列を読む必要がある。子 type は
  value / keyframe / expression の 3 種に限定されている。
- 単一プロパティ変更が `property.set` に**降格される**挙動（`:575`）。
  マクロが1件だけだった場合に `property.batch` ではなく
  `property.set` を送る既存仕様を維持した。
- 受信側の apply 分岐（`:694` 以降の `kOpPropertyBatch` /
  `kOpPropertyKeyframes` / `kOpPropertyExpression` /
  `kOpLayerComponents` / `kOpLayerStack`）。
  これは送信側とは責務が別である。

**検証（静的のみ）**

- 削除した識別子の残存 0 件（13 変数すべて）
- Macro 用の `appendPropertyChange` / `appendKeyframeChange` /
  `appendExpressionChange` は定義と使用の 2 箇所ずつで生存
- 受信側の `kOp*` 定数は import 必要のまま

**このことで変わったもの**

- 1つの編集に対する符号化の定義が 1 箇所になる。
- ただし `MacroUndoCommand` の子集約だけは依然として
  `serialize()` を前提としているため、**完全には一本化していない**。

**未実施**

- **ビルド未実施**。`commandType()` の文字列分岐を消したため、
  undo/redo の双方で IR が false を返した場合の
  拒绝メッセージが「This edit type is not synchronized」に統一される。
  これが UI 上の案内として妥当かは実機確認が要る。

### Phase 4: BranchRef と複数親 commit（2026-10-01 実装）

スコープはユーザー選択により **BranchRef と複数親 commit のみ**に絞った。
merge の合成アルゴリズムと conflict 解決 UI は次フェーズに残す。

**変更ファイル:**
- `Artifact/include/Project/ArtifactRevisionService.ixx`
- `Artifact/src/Project/ArtifactRevisionService.cppm`

**データモデル**

`ProjectRevisionRecord` に `parentIds`（`QStringList`）を追加。
単一親の commit も `parentIds` に1件入れるため、祖先 walk は
常に `parentIds` だけを見ればよい。

後方互換のため `parentId` は残す。読み込み時は
`parentIds` が空なら `parentId` から1件復元する
（`recordFromJson()` :381）。

`RevisionBranchRef` を新設（`ixx` :36）。
`id` / `displayName` / `headRevisionId` / `baseRevisionId` /
`createdAt` / `updatedAt`。

**永続化**

`branches.json`（新規）を追加。branch は commit より変化が少ないため
追記形式ではなく全量書きでよい。

```json
{ "version": "1", "currentBranchId": "branch-main",
  "branches": [ { "id": "...", "headRevisionId": "...", ... } ] }
```

**API**

| 関数 | 役割 |
|---|---|
| `branches()` / `currentBranchId()` / `setCurrentBranchId()` | 参照の参照 |
| `createBranch(name, fromRevisionId, branchId)` | 分岐を作る |
| `renameBranch()` / `moveBranchHead()` / `deleteBranch()` | 参照の操作 |
| `commitMerge(snapshot, parentIds, ...)` | 複数親の commit を作る |
| `ancestorRevisions(id)` | DAG を walk して祖先を列挙 |
| `commonAncestorRevisions(a, b)` | 2 つの revision の共通祖先 |

`deleteBranch()` は**最後の1本は削除できない**。
branch が0本になると `currentBranchId` が無効になるため。

**head の追従**

`commitCurrentProject()` / `commitMerge()` / `restoreRevision()` の
すべてで、active branch の `headRevisionId` を進める。
これがないと commit しても branch の head が古いまま残る。

**初回起動時**

`ensureDefaultBranch()`（:458）で `branch-main` を自動生成する。
branch 導入前に作られたプロジェクトでも API が壊れないようにするため。

**検証（`temp/dagtest.py` で同等のロジックを確認後削除）**

| ケース | 結果 |
|---|---|
| 旧形式（`parentId` のみ）の3連鎖 | `r3 → r2 → r1` と解決 |
| 分岐 + merge commit の DAG | `m1` から `r4, r3, r2, r1` に到達 |
| 共通祖先 | `r3, r1` |
| **循環した不正台帳** | 終了する（無限ループしない） |
| branch ref の保存と再読込 | 2 件を復元、current 維持 |

**未実施**

- **ビルド未実施**

---

### Phase 5: merge 合成と conflict 解決（2026-10-01 実装）

**変更ファイル:**
- `Artifact/include/Project/ArtifactRevisionService.ixx`
- `Artifact/src/Project/ArtifactRevisionService.cppm`
- `Artifact/include/Widgets/ArtifactSnapshotCompareWidget.ixx`
- `Artifact/src/Widgets/ArtifactSnapshotCompareWidget.cppm`

**レイヤー順序が意味を持つという前提**

`ArtifactAbstractComposition.cppm:5877-5883` は
`layerMultiIndex_.all()` の並びをそのまま JSON 配列に書いている。
つまり並び順がタイムライン上の順序として復元されるため、
merge は集合の一致ではなく**列の合成**でなければならない。

**アルゴリズム（プロトタイプで先に検証してから実装）**

| ルール | 内容 |
|---|---|
| 同一性 | `id`、無ければ `assetId` |
| 並び替え | 編集として扱う。片側だけが並び替えたならその側を採用 |
| 追加 | **追加した側の直前の要素の直後**に挿入する |
| 削除の競合 | 残る側が base から**変更していた**場合だけ delete/modify |
| 値の競合 | ours を採用（必ず結果が決まるように） |

追加のルールについて補足。並び替えと追加が交差する場面では
「正解」は存在せず（Git も同じ問題を持つ）、決定性であることが
要件になる。このルールは常に同じ結果になる。

**実装した関数**

- `mergeById()` — オブジェクト単位の 3-way
- `mergeIdentityOrder()` — 配列の列合成と delete/modify 検出
- `mergeSnapshotThreeWay()` — snapshot 全体の再帰 merge
- `mergeRevisions(ours, theirs, oursPreferredPaths)` — 結果を返すだけ
- `mergeRevisionIntoProject(...)` — merge commit を作成して読み込み

merge base は `commonAncestorRevisions()` の先頭（BFS で最初に到達した
共通祖先）。`r1←r2←r3←r4` と `r2→b1` のとき `r2` が選ばれることを
`temp/basecheck.py` で確認済み。

**conflict 解決の UI**

- ツールバーに `Merge B into A` ボタン
- conflict リスト（既定は非表示。競合があったときだけ出す）
- `Keep Mine` / `Keep Theirs` で個別に選択
- 2 段階: 1 回目でプレビュー、選択後に 2 回目で確定

未解決の conflict は ours を採用するので、選択なしで
「すべて自分側」を選ぶ正当な操作もある。

**`onBranch()` も更新**

旧実装は「復元して tag を付ける」だけで、branch という概念が
なかった。新実装は `createBranch()` で名前付きの参照を作る
（元の履歴は書き換えない）。

**検証（`temp/mergeproto.py` で 10 ケース、`temp/basecheck.py` で 4 ケース）**

独立編集 / 双方追加 / 独立削除 / delete-modify 競合 /
同一編集 / 並び替え＋追加 / 互いに素な履歴 / 値の競合 /
theirs のみ変更 / 変更なし。全て期待どおり。

**実装中に見つけた誤り（テスト側のケースが誤っていた）**

- 最初の実装では delete/modify を ID 集合だけで判定しており、
  内容を変更したレイヤーを「削除＋新規追加」と誤認していた
- 並び替えが base 順に固定されたまま反映されていなかった
- いずれもプロトタイプのテストが先に検出した

**未実施**

- **ビルド未実施。UI のレイアウト（ラベル文字列、ボタン幅）は
  実機で確認が要る**
- **heuristic の妥当性は実データで未評価。** 特に `same-direction` は
  「大きい方」を採るので、意図と違う可能性がある。
  実プロジェクトでの使捨選択を確認してから増やすべき

---

### Phase 6: content 競合の自動解決（2026-10-01 実装）

スコープはユーザー選択により **content 競合のみ**に限定した。
delete/modify と order は人的判断のままにした。理由は、
「復活したレイヤー」や「黙って消されたレイヤー」は
「確認が1回増える」ことより悪い結果になるため。

**変更ファイル:**
- `Artifact/include/Project/ArtifactRevisionService.ixx`
- `Artifact/src/Project/ArtifactRevisionService.cppm`
- `Artifact/src/Widgets/ArtifactSnapshotCompareWidget.cppm`

**heuristic（`autoResolveConflictValue()` :293）**

| ルール | 条件 | 採る値 | 理由 |
|---|---|---|---|
| identical | 両側が同じ | ours | 合意済み |
| ours-only | theirs が base のまま | ours | 一方だけが動かした |
| theirs-only | ours が base のまま | theirs | 同上 |
| numeric-tolerance | 数値の差が相対 1e-6 以下 | ours | スライダーの微差 |
| same-direction | 数値が base から同方向 | 移動量が大きい側 | どちらが正当か不明なら消さない |
| string-ignoring-space | 空白のみ違う文字列 | ours | 実質同じ |
| （対象外） | 文字列が別物 | — | 推測すると編集が黙って消える |
| （対象外） | 片側が空にした | — | 「既定に戻した」と「編集中」が区別できない |
| （対象外） | bool が両側で反転 | — | 多数決が成り立たない |

数値の許容は既存 `qJsonValueEquals()` の絶対 1e-6（`:86`）ではなく
**相対**にした。絶対値だと 100.0 と 100.0000002 を
「同じ位置」と判定してしまう。

**透明性の確保**

自動解決した項目は conflict リストから消えるため、
`RevisionMergeResult::autoResolved`（`(path, reason)` の list）に記録し、
ステータスに `(%1 auto-resolved)` として件数を示す。
編集が黙って消えたように見えないようにするため。

`ours-only` / `theirs-only` / `identical` は「片側が動かしただけ」という
通常のケースなので記録しない（記録すると件数にノイズになるため）。

**検証（`temp/automerge.py` で 13 ケース）**

全ケース期待どおり。2 件の初回失敗は**テスト側の期待値ミス**で、
ロジック自体は正しかった（float noise の ours 採用、bool の theirs が base 同一）。

**未実施**

- **ビルド未実施**
- **heuristic の妥当性は実データで未評価**

---

## Phase 0 の結果（2026-10-01 実測）

`UndoManager.ixx` に定義された `UndoCommand` 派生クラスは **59 個**。

| 項目 | 数 | 備考 |
|---|---|---|
| クラス総数 | 59 | `Artifact/include/Undo/UndoManager.ixx` |
| `commandType()` 実装 | 55 | 未実装 4 個は後述 |
| `canSerialize()` 実装 | 55 | 同上 |
| `buildCollaborationOperation()` 宣言 | 17 | すべて `Artifact/src/Undo/UndoManager.cppm` に実装あり |

`canSerialize()` を実装していない **4 個**は以下。

- `LayerSelectionSnapshotCommand`（`:596`）
- `ToggleLocalizedSourceCommand`（`:858`）
- `SetCompositionWorkAreaCommand`（`:1264`）
- `LayoutSnapshotCommand`（`:1797`）

→ いずれも **Undo 履歴の永続化対象外**という意図 codified されている。
ここでは「欠陥」とは数えない。ただし Phase 3 で IR を正本化する際、
これらは「IR にも載せない」判断が要る。

### 訂正: 符号化は二系統ではなく三系統

当初「二系統（永続化 / wire）」と記述したが、実測では**三系統**ある。

1. **`buildCollaborationOperation()` による自前生成** — 17 コマンド。
   `type` + `layerId` + `payload` を直接組み立てる
   （`Artifact/src/Undo/UndoManager.cppm:1120` ほか）
2. **`serialize()` の流用による AppMain 側の変換** —
   `SetLayerPropertyValueCommand` / `SetLayerPropertyKeyframesCommand` /
   `SetLayerPropertyExpressionCommand` / `LayerComponentDescriptorSnapshotCommand` /
   `ClonerTransformStackSnapshotCommand` / `CloneEffectorStackSnapshotCommand` は
   **仮想を実装せず**、`Artifact/src/AppMain.cppm:441-459` が
   `commandType()` の**文字列比較**で分岐し、
   `:567` / `:571` / `:608` / `:621` / `:634` で
   `serialize()` の出力（`layerId` / `propertyPath` / `beforeValue` / `afterValue`）を
   読み取って wire payload に変換している
3. **`MacroUndoCommand` の子集約** — `AppMain.cppm:651-674` が
   `command.serialize().value("children")` を読み取り、子の `type` で
   3 種（value / keyframe / expression）に振り分ける

つまり `serialize()` は**永続化用であると同時に wire 生成の材料にもなっている**。
これが設計上の混乱の中核であり、当初見立ての「二系統が別物である」보다
**実際にはは耦合している**。Phase 3 の IR 正本化は、まずこの結合を解く作業になる。

### 未同期コマンドの扱い

`AppMain.cppm:460-469` は、上記のいずれにも該当しないコマンドを
**拒否**する（`"This edit type is not synchronized in the current collaboration session"`）。
つまり未実装の 42 個は「対応漏れ」ではなく **明示的に拒否される状態**。

これは正しい fail-closed 設計であり、監査也POINT留する。
ただし Phase 3 で IR を拡充する際、拒否されるコマンドを減らすか否かは
製品判断が要る。

---

## 文書の訂正

`docs/technical/INTERNAL_VCS_ARCHITECTURE.md` は現状と矛盾する。設計の前提に使うと
誤誘導されるため、訂正が必要。

| 記述 | 実測 |
|---|---|
| `ArtifactRevisionService` 「100% 完成」 | snapshot 式であり branch 機能・デルタ圧縮は未実装（同文書も併記） |
| 「バックグラウンド保存、UI スレッドを一切ブロックしない」 | **誤り。** `QTimer::singleShot`（`ArtifactRevisionService.cppm:608-613`）からの主スレッド**同期**実行で、`project->toJson()` と `QSaveFile::commit()` がそのまま走る（`:373-388`） |
| 「スナップショット作成 < 50ms」「コミット保存 < 100ms」 | 未検証。测量記録が無い |
| 「差分ビューワーの接続のみ未実装」 | 比較 UI は接続済み（`ArtifactSnapshotCompareWidget.cppm:288-292`）。diff は接続済みだが壊れている（D2） |

---

## リスク

1. **IR 覆盖面が想定より小さい。** 17 実装しかなければ Phase 3 は当初想定より大きい。
   → Phase 0 で正確な一覧を作ることで回避
2. **既存 snapshot 履歴の移行**が要る。`AppDataLocation` 配下の既存
   `ArtifactVCS/<key>/` が放置される
   → 移行を別フェーズに切り出す（新形式でのみ新規作成する）
3. **merge は DCC 固有の難しさを持つ。** レイヤーの並べ替えと時刻を含み、
   単純な JSON レベル merge では解決しない
   → Phase 1 の ID 照合 diff がその土台になる

---

## 成功条件

- branch を切れる
- 2つの branch を合流でき、衝突を UI で解決できる
- レイヤーを削除しても無関係なレイヤーに差分が出ない
- 履歴量が増えても保存時間が線形に悪化したない
- 1つの編集に対して符号化が1つだけ定義されている

---

## 関連

- `docs/planned/MILESTONE_LIGHTWEIGHT_VCS_AND_LAYER_VARIANTS_2026-04-17.md`（前身。snapshot 中心）
- `docs/planned/MILESTONE_COLLABORATION_FEATURES_2026-03-28.md`
- `docs/analysis/COLLABORATION_IMPLEMENTATION_AUDIT_2026-08-22.md`
- `docs/technical/INTERNAL_VCS_ARCHITECTURE.md`（要訂正）
- `docs/planned/MILESTONE_CRASH_SAFE_SAVE_2026-06-16.md`
- `Artifact/src/Project/ArtifactRevisionService.cppm`
- `Artifact/include/Undo/UndoManager.ixx`
- `ArtifactCore/src/Collaborate/CollabOperations.cppm`
- `ArtifactCore/include/Utils/Id.ixx`

---

## Current Status

2026-10-01 時点では設計段階。コード変更なし。
Phase 0（事実の固定）から着手する。
