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

<<<<<<< HEAD
Cinema 4D / Unreal Motion Design 的な field 制作体験を、そのまま巨大な generator system にせず、まずは既存 layer 群へ非破壊で作用する live field として育てる。
=======
Cinema 4D / Unreal Motion Design 的な field 制作体験を、そのまま巨大な generator system にせず、まずは **既存 layer 群へ非破壊で作用する live field** として育てる。
>>>>>>> 6a05302 (chore_parent_repo_sync_all)

今回の先行実装で、次までは到達済み:

- 選択 layer から live radial field を作成
- composition 保存
- enabled / disabled / edit / remove
- non-destructive transform evaluation
- viewport overlay 表示
- one-shot radial transform

一方で、日常的に使うにはまだ次が不足している:

- viewport 上で field center / radius を直接ドラッグできない
- field の選択状態や hover が弱い
- strength / blend / invert / reorder など stack 的な操作がない
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

### 2.2 未完了

| 軸 | 状況 | 影響 |
|---|---|---|
| Viewport drag | 未実装 | center / radius の編集が毎回ダイアログ寄り |
| Hover / active state | 弱い | どの field を触るか迷いやすい |
| Field stack controls | 未実装 | 複数 field を重ねた制作に移れない |
| Blend / weight / invert | 未実装 | C4D 的な influence 設計に広がらない |
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
<<<<<<< HEAD

---

## 4. 次の実装候補

- viewport direct manipulation の drag handle を小さく追加する
- field list で active / hover / reorder を読めるようにする
- `strength / blend / invert` の最小パラメータを先に通す
- radial 以外の shape は 1 種だけでも追加して契約を広げる

=======
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

この順なら、既存 overlay と menu 実装を活かしながら最小差分で実用域へ持っていける。
>>>>>>> 6a05302 (chore_parent_repo_sync_all)
