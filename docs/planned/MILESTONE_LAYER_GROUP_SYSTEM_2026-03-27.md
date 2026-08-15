# マイルストーン: レイヤーグループ導入

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

`ArtifactGroupLayer` は composition-owned／embedded child の両経路、`childrenForRender()`、offscreen composite と fallback draw、group opacity／blend／mask の単位評価を持ちます。`ArtifactHierarchyModel` は parent chain／子行を扱い、Timeline の group row／collapse／rename／selection／parent column、保存・再読込・循環拒否も実装されています。group の hidden／locked 等の state reason と Workspace Automation の作成導線も確認できます。

今回、Timeline のグループメニューから Undo 付きの「グループを解除」を実行できる導線を追加しました。残課題は、group transform の専用可視化、group 単位の move／delete／drag-and-drop、solo／shy／lock の全 runtime 組み合わせ、project tree との共通表現、再起動後の実 UI／描画 parity の検証です。したがってモデル・表示・基本レンダーは進行済みですが、Phase 4〜6 は未完了・未検証です。

## Update 2026-08-15

- 現行コードでは group の composition 所有／embedded child、offscreen composite、opacity／blend／mask、Timeline の折りたたみ・rename・選択・parent 表示、保存・再読込・循環拒否まで確認できる。
- group 単位の transform 可視化・移動／削除／drag-and-drop、solo／shy／lock の組み合わせ、Project View との共通表現、再起動後の UI／描画 parity は未完了または runtime 未確認。

> 2026-03-27 作成

## 現状サマリー

作成時点では `ArtifactLayerGroup` / `ArtifactLayerGroupCollection` は「データ構造の骨組み」に近かったが、現行コードでは `ArtifactGroupLayer`、Timeline の階層表示、描画合成、保存／再読込、循環拒否まで接続されている。残る課題は、group transform の専用可視化、group 単位の操作、Project View との責務・表示統一、実 UI／描画の runtime parity である。

このマイルストーンでは、レイヤーグループを「見た目整理」「変換階層」「可視性制御」の3役に分けて、将来の複雑な構成でも追えるようにする。

---

## Scope

- `Artifact/src/Layer/ArtifactLayerGroup.cppm`
- `Artifact/src/Layer/ArtifactCompositionLayer.cppm`
- `Artifact/src/Project/ArtifactProject.cppm`
- `Artifact/src/Service/ArtifactProjectService.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

## Non-Goals

- property group の再定義
- 既存の layer / effect / mask モデルを壊す全面再設計
- 1 回で「理想の階層 UI」を完成させること

## Background

現在の課題は、同じ "group" という言葉が複数の責務に使われやすいこと。
レイヤーの見た目整理、親子による transform 階層、effect の整理、project tree の分類が混ざると、UI 上で「どこを触っているのか」が分かりにくくなる。

このため、導入段階ではまず「表示グループ」と「変換階層」の責務を分け、必要に応じて effect group や仮想グループへ拡張できる土台を作る。

---

## Phase 1: Group Model / Serialization

**進捗:** group layer の child／parent ID と group 固有状態の composition JSON round-trip は静的実装済み。UI と実運用での再起動確認は未検証。

- 目的:
  - レイヤーグループのデータモデルを安定させる
  - project 保存 / 読み込みで group 状態を失わないようにする

- 作業項目:
  - `group id` / `parent group id` の永続化
  - `expanded / muted / locked / color / opacity` の保存
  - root group の扱いを明示する
- delete / move / reparent の整合性チェック

### 静的実装済み

- [x] group layer の child count／collapsed／output mode／active child の保存・復元
- [x] composition-owned group の child を composition JSON 経路で保持
- [x] child の parent layer ID と composition pointer の round-trip
- [x] composition 上で祖先を子へ追加する循環を拒否
- [x] ProjectService の通常 reparent 経路でも parent-chain 循環を拒否

- 完了条件:
  - 再起動後も group 構造が壊れない
  - group の削除 / 移動で参照が壊れない

## Phase 2: Display Group UI

**進捗:** Timeline layer panel の group row／icon／collapse／child count／rename／child selection／output mode は静的実装済み。group 固有の color／mute／lock 表示と runtime 確認は未検証。

- 目的:
  - レイヤーを整理しやすい表示専用グループを導入する
  - group の存在が UI 上で見えるようにする

- 作業項目:
  - timeline / layer panel に group row を表示
  - collapse / expand
  - group color / mute / lock の可視化
  - group 名の rename
- group 内の layer count 表示

### 静的実装済み

- [x] Timeline layer panel の group row と group icon
- [x] group の展開／折りたたみ
- [x] child count、rename、child selection、output mode の操作導線
- [x] group row の子数バッジへ Hidden／Locked／Solo／Shy 状態を併記
- [x] 親 group の Hidden／Locked を子 row の state reason として表示

- 完了条件:
  - [x] group が見える（Timeline group row／icon）
  - [x] group の折りたたみと基本状態把握ができる

## Phase 3: Transform Hierarchy Integration

**進捗:** Timeline の parent column／parent-whip 表示と parent-chain state reason は静的実装済み。transform 影響範囲の専用 overlay と runtime確認は未検証。

- 目的:
  - 「見た目整理」と「変換親子」を完全に同一視しない
  - 必要な場合だけ transform 階層として使えるようにする

- 作業項目:
  - display group と transform group の役割を分ける
  - parent child の dependency overlay
  - 親を選ぶと子を強調表示
- transform への影響範囲を明示

### 静的実装済み

- [x] display group と `parentLayerId` を別の責務として保持
- [x] Timeline parent column／parent-whip による parent の有無表示
- [x] parent chain を辿った group hidden／locked reason の表示

- 完了条件:
  - group が見た目整理だけでなく、変換文脈でも追える
  - 親子と表示整理の責務が分離できる

## Phase 4: Visibility / Solo / Lock Integration

- 目的:
  - なぜ見えるか / 見えないかを group 単位でも分かるようにする
  - solo / shy / lock と group の関係を整理する

- 作業項目:
  - state banner との連携
  - group 単位の solo / shy / lock 表示
  - visibility inspector への group 理由の追加
  - 非表示理由チップの表示

- 完了条件:
  - group が原因で layer が見えない状況を追跡できる
  - solo / lock / shy と group の相互作用が説明できる

## Phase 5: Batch Operations / Selection Sync

**進捗:** group の child selection と、選択 layer を group 化する既存導線は静的実装済み。group 単位の移動／削除、batch property、drag-and-drop、runtime確認は未検証。

- 目的:
  - group を複数 layer の操作単位として使えるようにする
  - 選択・移動・rename をまとめやすくする

- 作業項目:
  - group 単位の rename / move / delete
  - group 選択時の layer 強調
  - batch property 操作の入口
- drag-and-drop による group 再編成

### 静的実装済み

- [x] group context menu から子レイヤーを選択
- [x] 選択 layer を group 化する batch 操作の入口

- 完了条件:
  - group が単なる表示枠でなく、操作単位として使える
  - 選択同期が破綻しない

## Phase 6: Effect / Virtual Group Extension

- 目的:
  - 将来の effect group / virtual group に拡張できるようにする
  - project tree と layer tree の扱いを共通化する

- 作業項目:
  - effect group の導入可否を検討できるデータ枠
  - 仮想 group の表示方法を定義
  - render / project / timeline で共通の group 表現に寄せる

- 完了条件:
  - group の概念を layer 専用に閉じない
  - UI 拡張の余地が残る

---

## Recommended Order

1. Phase 1: Group Model / Serialization
2. Phase 2: Display Group UI
3. Phase 4: Visibility / Solo / Lock Integration
4. Phase 3: Transform Hierarchy Integration
5. Phase 5: Batch Operations / Selection Sync
6. Phase 6: Effect / Virtual Group Extension

---

## Validation Checklist

- [x] group の保存 / 読み込みの基本 round-trip（静的テスト経路）
- [ ] group の折りたたみと状態表示が見える
- [ ] group 単位で layer の見え方を追える
- [ ] parent / child の関係が UI 上で追える
- [ ] group 単位の操作で layer tree が壊れない
