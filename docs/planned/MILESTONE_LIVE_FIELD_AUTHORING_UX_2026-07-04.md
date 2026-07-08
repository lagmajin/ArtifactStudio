# M-LC-3 Live Field Authoring UX

**ステータス:** In Progress

作成日: 2026-07-04  
対象: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `Artifact/include/Composition/ArtifactAbstractComposition.ixx`  
位置づけ: `docs/planned/MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md` の field authoring を、既存 layer transform に対する lightweight な live field として先行実装する。  
参照:
- `docs/analysis/REPORT_CROSS_APP_FEATURE_OPPORTUNITIES_2026-07-04.md`
- `docs/planned/MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md`
- `docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md`
- `docs/COMPOSITION_EDITOR_CONTRACT.md`

---

## 1. 目的

Cinema 4D / Unreal Motion Design 的な field 制作体験を、そのまま巨大な generator system にせず、まずは **既存 layer 群へ非破壊で作用する live field** として育てる。

今回の先行実装で、次までは到達済み:

- 選択 layer から live radial field を作成
- composition 保存
- enabled / disabled / edit / remove
- non-destructive transform evaluation
- viewport overlay 表示
- one-shot radial transform

一方で、日常的に使うにはまだ次が不足している:

- viewport 上で field center / radius を直接ドラッグできるが、ハンドル演出はまだ最小限
- field の選択状態や hover が見える
- strength / blend / invert / reorder など stack 的な操作は一部 menu で扱える
- radial 以外の field shape がない
- generator / modifier 側の field mask 契約へまだ接続していない

この milestone は、その未完了分を次回すぐ再開できる形に固定する。

---

## 2. 現在地

### 2.1 実装済み

- `CompositionTransformField` を composition 直下に保持
- `transformFields` の JSON serialize / deserialize
- layer local transform に対する field evaluation
- add / update / remove の undo
- menu からの作成、編集、有効/無効、削除
- 選択対象に関係する field の viewport overlay
- viewport 上での center / radius 直接ドラッグ

### 2.2 未完了

| 軸 | 状況 | 影響 |
|---|---|---|
| Viewport drag | 一部実装 | center / radius を viewport で直接ドラッグできるが、専用ハンドルの見た目はまだ簡素 |
| Hover / active state | 実装済み | viewport hover と active 選択の導線があり、active は badge と menu list でも見える |
| Field stack controls | 一部実装 | active 巡回、順序変更、menu 経由の enable/edit/delete と最小 list UI はあるが、独立 panel はまだ必要 |
| Blend / weight / invert | 一部実装 | strength / blendMode / invert を保持・編集できるが、UI surface はまだ簡素 |
| Shape variety | radial のみ | box / linear / noise 系に発展できない |
| Modifier integration | 未実装 | generator / modifier / dynamics 共通 contract へ繋がっていない |

---

## 3. スコープ

### In Scope

- live transform field の viewport 直接編集
- field の active / hover / hit-test
- radial field の parameter surface 改善
- field list の最小 stack 操作
- 今後の `generator / modifier / field` 共有契約へ繋がるデータ拡張

### Out of Scope

- 完全な C4D clone/effector system
- 3D field
- 新しい global signal 配線
- field ごとの専用 dock 大量追加
- bake system 全体の設計完了

---

## 4. 実装フェーズ

### Phase 1: Viewport Direct Manipulation

- field center handle を hit-test 可能にする
- radius handle を hit-test 可能にする
- drag 中は composition overlay を live 更新する
- release 時に 1 undo で戻せる command を積む
- field local space と parent layer transform の往復を明示化する

**Done criteria:**

- center を viewport 上で直接動かせる
- radius を viewport 上で直接変えられる
- drag 中の見た目と release 後の値が一致する
- 1 undo で drag 前へ戻せる

### Phase 2: Field Stack Controls

- field list UI を最小限追加
- active field の選択を持てるようにする
- strength / enabled / delete を list から直接操作できるようにする
- 順序変更に備えて stable order を持たせる

Current status:

- menu から active field を前後に巡回できる
- active field の順序を上下へ動かせる
- field 選択ダイアログは active field を初期選択にする
- arrange menu に live field の一覧があり、クリックで active を切り替えられる
- viewport の active field badge が出る
- viewport 上で center / radius を直接ドラッグできる
- live field list と badge で strength / invert の要点が見える
- viewport の active field に center-radial ガイドと handle 差がある
- active なしのときは active 順序操作を抑制する
- active field の削除 Undo で active も復元される
- drag 開始で target field が active に追従する

**Done criteria:**

- 複数 field があっても対象を迷わず選べる
- menu だけでなく list から状態操作できる
- 将来の reorder 実装を阻害しないデータ構造になる

### Phase 3: Influence Controls

- `strength`
- `blendMode`
- `invert`
- radial 専用の `edgeScale / expansion` と共存できる parameter へ整理

**Done criteria:**

- 1 つの field が「どれだけ効くか」を明示的に制御できる
- 複数 field を将来合成しても契約が破綻しない

### Phase 4: Shape Expansion

- `box`
- `linear`
- 必要なら `solid`

最初から noise まで広げず、overlay / hit-test / evaluation の共通契約を先に固める。

**Done criteria:**

- radial 以外の 2 種以上で同じ authoring 流れが成立する
- shape ごとの差分が descriptor と overlay に閉じる

### Phase 5: Generator / Modifier Bridge

- field を transform layer 専用の裏機能で終わらせず、
  `generator / modifier / future dynamics` から読める contract へ寄せる
- `position only` 前提から、将来の weight / scale / color / time offset に拡張できる influence 出力へ整理する

**Done criteria:**

- field の出力契約が layer transform 専用に閉じない
- `M-LC-2 Generator / Modifier / Field Stack Migration` と自然に接続できる

---

## 5. 設計メモ

- 直接 manipulation は `ArtifactCompositionRenderController` の既存 input path 内に閉じる
- 新規 signal / slot は追加しない
- overlay は今の composition overlay pass を再利用する
- drag 中の一時値更新と undo command の最終確定を分ける
- `ArtifactAbstractLayer` の transform evaluation は non-destructive のまま維持する

---

## 6. 次回の再開点

次に着手するなら **Phase 1: Viewport Direct Manipulation** から再開する。

着手順:

1. field handle hit-test の導入
2. center drag
3. radius drag
4. drag undo の確定

---

## Next Execution Slice

### Phase 1A の着手点

- field center / radius の hit-test を `ArtifactCompositionRenderController` の既存 input path に閉じて追加し、overlay の live 更新だけ先に通す
- center drag と radius drag を分け、どちらを掴んでいるかを viewport で迷わない最小ガイドにする
- drag 開始時に active field を固定し、release 時に 1 undo へまとめる

### Phase 2A の着手点

- field list は独立 panel 化する前に、既存 menu と badge で active / hover / enabled を追える状態に揃える
- strength / invert / enabled は list から直接触れるようにし、edit ダイアログ増殖を避ける
- stable order は reorder 実装の前提だけ先に持たせ、見た目の順序と保存順序を分離する

### Phase 5 前提

- generator / modifier への field mask 接続は、viewport 直接編集と stack controls が安定してからにする
- radial 以外の shape 拡張は、overlay / hit-test / evaluation の共通契約が固まってから進める
- 新しい global signal / slot は追加せず、現行の input / menu / overlay 経路を再利用する

この順なら、既存 overlay と menu 実装を活かしながら最小差分で実用域へ持っていける。
