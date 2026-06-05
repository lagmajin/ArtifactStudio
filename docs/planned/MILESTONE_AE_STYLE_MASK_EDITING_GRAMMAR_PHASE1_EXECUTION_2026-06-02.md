# Phase 1 Execution: AE Style Mask Editing Grammar

> 2026-06-02 作成

`MILESTONE_AE_STYLE_MASK_EDITING_GRAMMAR_2026-06-02.md` の Phase 1 を、頂点選択と mask path 選択の分離に絞って実装するための実行計画。

---

## Objective

- 頂点編集の入口を作る
- `mask path` と `vertex` の選択を混同しない
- `Delete` と `Shift` の意味を先に固める
- 追加操作の土台を作る

---

## Scope

### In

- 頂点選択と mask path 選択の整理
- `Shift` による複数頂点選択の扱い
- `Delete` による頂点削除の導線
- 辺上クリックでの頂点追加の入口
- 選択文脈の表示文言

### Out

- 角 ⇔ ベジェ切替の完成
- ハンドル片側操作の完成
- ショートカット再編の全面導入
- backend / render path の変更

---

## First Files

1. `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
   - mask selection の状態表示と削除操作の導線を確認する
   - 頂点編集の入口を作るなら、まずここで selection 文脈を整理する

2. `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
   - mask path / vertex の見え方を確認する
   - 選択状態の表現が不足していればここで補う

3. `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
   - ヘッダ要約や empty state 文言を、頂点編集の文脈に寄せる

---

## Implementation Notes

- `Shift` は複数選択の補助として一貫させる
- `Delete` は「いま選ばれている頂点」にだけ効かせる
- 頂点追加はペンツールの文脈に乗せる
- mask path の選択と vertex の選択が同じ見た目になりすぎないようにする

---

## Checklist

- [ ] 頂点選択の状態遷移を確認する
- [ ] `Shift` 複数選択の UI 文脈を決める
- [ ] `Delete` の対象を明確にする
- [ ] 頂点追加の入口を決める
- [ ] 選択文言が mask path と vertex で混ざらないか確認する

---

## Done Criteria

- 頂点編集の最初の一歩が迷わず見える
- 選択文脈が壊れにくい
- 次の Phase に進めるだけの土台ができる

