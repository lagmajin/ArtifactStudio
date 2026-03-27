# マイルストーン: レイヤーグループ導入

> 2026-03-27 作成

## 現状サマリー

`ArtifactLayerGroup` / `ArtifactLayerGroupCollection` は既に存在するが、現状は「データ構造の骨組み」に近い。
グループの名前、親子関係、折りたたみ、ミュート、ロック、色、opacity は持てる一方で、UI と描画と操作の責務がまだ分離されていない。

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

- 目的:
  - レイヤーグループのデータモデルを安定させる
  - project 保存 / 読み込みで group 状態を失わないようにする

- 作業項目:
  - `group id` / `parent group id` の永続化
  - `expanded / muted / locked / color / opacity` の保存
  - root group の扱いを明示する
  - delete / move / reparent の整合性チェック

- 完了条件:
  - 再起動後も group 構造が壊れない
  - group の削除 / 移動で参照が壊れない

## Phase 2: Display Group UI

- 目的:
  - レイヤーを整理しやすい表示専用グループを導入する
  - group の存在が UI 上で見えるようにする

- 作業項目:
  - timeline / layer panel に group row を表示
  - collapse / expand
  - group color / mute / lock の可視化
  - group 名の rename
  - group 内の layer count 表示

- 完了条件:
  - group が見える
  - group の折りたたみと状態把握ができる

## Phase 3: Transform Hierarchy Integration

- 目的:
  - 「見た目整理」と「変換親子」を完全に同一視しない
  - 必要な場合だけ transform 階層として使えるようにする

- 作業項目:
  - display group と transform group の役割を分ける
  - parent child の dependency overlay
  - 親を選ぶと子を強調表示
  - transform への影響範囲を明示

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

- 目的:
  - group を複数 layer の操作単位として使えるようにする
  - 選択・移動・rename をまとめやすくする

- 作業項目:
  - group 単位の rename / move / delete
  - group 選択時の layer 強調
  - batch property 操作の入口
  - drag-and-drop による group 再編成

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

- [ ] group の保存 / 読み込みが安定する
- [ ] group の折りたたみと状態表示が見える
- [ ] group 単位で layer の見え方を追える
- [ ] parent / child の関係が UI 上で追える
- [ ] group 単位の操作で layer tree が壊れない

