# MILESTONE: State Layer / Visible Child Selection

## Purpose

子レイヤーの中から、いま表示するものを状態として切り替える `StateLayer` の最小実装を整理する。

このマイルストーンでは、表示制御に責務を絞り、`Group` や `Composition`、`Variant` の代替にはしない。

## Goal

- 子レイヤー群のうち、どれを表示するかを state として管理できる
- 表示ルールをレイヤー階層の外側に逃がして、プリセット切替や比較表示をやりやすくする
- 既存の `visible` / `solo` / `shy` と責務がぶつからない最小形を作る

## Scope

- `Artifact/include/Layer/*`
- `Artifact/src/Layer/*`
- composition / hierarchy / inspector / timeline surfaces

## Non-Goals

- transform の代理管理
- effect stack の置き換え
- variant system の全面再設計
- 共同編集ロックとの統合

## Minimal Data Model

- state name
- active state id
- state ごとの visible child list
- optional rule metadata for future extension

## Suggested Phases

### Phase 1: State Model Freeze

- `StateLayer` の責務を「表示選択」に限定する
- state の名前と選択中 state を固定する
- 子レイヤーへの表示割り当てを保存できるようにする

### Phase 2: View Integration

- 現在の state に応じて、子レイヤーの表示可否を決める
- viewport / timeline / hierarchy で見え方が一致するようにする

### Phase 3: Inspector / UX

- state の切り替えを inspector から触れるようにする
- 表示対象の child list を確認できるようにする

## Success Criteria

- 1つの state で表示する child を切り替えられる
- hidden children と state-managed children の違いが UI 上で追える
- 既存の visibility 操作を壊さない

