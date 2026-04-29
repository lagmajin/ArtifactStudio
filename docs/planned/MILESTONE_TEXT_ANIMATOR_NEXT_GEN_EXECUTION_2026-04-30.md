# MILESTONE: Text Animator Next Gen - Execution Slice

**Date**: 2026-04-30  
**Source**: [`MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md`](./MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md)  
**Related**: [`MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md`](../MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md)

---

## Purpose

`TextAnimatorEngine` は core 側でかなり完成しているため、この execution slice では「残っている UI / timeline / preset の接続」を順番に埋める。
新しい表現を増やすより、既存の構造を壊さずに使える状態へ寄せるのが目的。

---

## Recommended Start Order

### 1. Animator UI Finish

まずは Inspector 側の操作を固める。

- animator type の選択導線を明確にする
- add / remove / rename の導線を統一する
- selector 設定の一覧性を上げる

狙い:
- どの animator が何をしているかを一目で追えるようにする
- timeline に進む前に、設定モデルの見通しを整える

実行メモ:
- [`MILESTONE_TEXT_ANIMATOR_NEXT_GEN_PHASE1_EXECUTION_2026-04-30.md`](./MILESTONE_TEXT_ANIMATOR_NEXT_GEN_PHASE1_EXECUTION_2026-04-30.md)

### 2. Timeline Exposure

次に、animator プロパティを timeline に流す。

- `AbstractProperty` として公開できるかを確認する
- keyframe 追加 / 編集 / 削除の最小経路を通す
- 再生中の反映を確認する

狙い:
- AE 風の「触れる animator」ではなく「時間変化する animator」にする
- 先に UI を固定した分だけ、timeline 側の配線が単純になる

### 3. Preset Browser

最後に、よく使う animator の入口をまとめる。

- built-in preset の候補を整理する
- JSON 保存 / 読み込みの責務を切る
- Inspector から呼べる最小ブラウザを用意する

狙い:
- 毎回ゼロから animator を作らなくてよい状態にする
- 後続の text work でも再利用しやすくする

---

## Phase Boundaries

### In Scope

- `ArtifactTextLayer` の既存 animator state を活かす
- Inspector の既存 property group を壊さず拡張する
- タイムライン連携は `TextAnimatorEngine` の公開経路に限定する
- preset はまず core data と最小 UI に分ける

### Out of Scope

- Text on Path
- 3D Text / per-character 3D
- GPU text rendering の backend work
- 新しい selection model の大改造

---

## Suggested Implementation Order

1. animator type picker の導線を整理
2. selector / property group の表示密度を揃える
3. timeline から触れる最小プロパティを1つ通す
4. preset を1つ保存・復元できるようにする
5. 既存の integration milestone を更新して完了条件を詰める

---

## Success Criteria

- Inspector だけで animator の構造が理解できる
- 少なくとも1つの animator property が timeline に出る
- preset を作って再利用できる
- 既存の `TextAnimatorEngine` を壊さずに拡張できる
