# MILESTONE: Effect System Bridge

**Date**: 2026-05-25
**Status**: Proposed
**Priority**: High
**Related**: `docs/EFFECT_SYSTEM_SPECIFICATION.md`, `Artifact/src/Widgets/Menu/ArtifactEffectMenu.cppm`, `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`, `Artifact/docs/MILESTONE_COMPOSITION_FINAL_EFFECT_2026-04-14.md`

---

## 概要

`Effect` 系は、現状かなり大きい UI 面積を持ちながら、実際には骨組み寄りの項目が多い。
`Layer` や `Composition` のように、メニュー項目・Inspector・レイヤー状態・Undo/Redo が一貫してつながるレベルまではまだ届いていない。

このマイルストーンでは、**エフェクトスタックを UI から普通に使える状態へ橋渡しする**。
既存の効果カテゴリを削るのではなく、まずは「選択中レイヤーに対して、何がどこまでできるのか」を明確にする。

---

## 現状整理

| 項目 | 現状 |
|---|---|
| Effect menu | カテゴリ数は多いが、実処理に接続していない項目が多い |
| Inspector | エフェクト専用の編集導線が薄い |
| Layer selection | 選択中レイヤーに対する effect stack の編集経路が弱い |
| Animation bridge | expression / keyframe 周辺は整備済みだが、effect 側とは未統合 |
| Preset / reorder | effect 単位の保存・並べ替え・削除の UX がまだ固まっていない |

---

## フェーズ

### Phase 1: Effect Stack Contract
**目標**: 選択中レイヤーの effect stack を UI から扱える共通契約を作る。

- [ ] active layer から effect stack を安全に取得する導線を確認する
- [ ] add / remove / reorder / enable / disable の最小 API を整理する
- [ ] effect の undo 単位を明確にする
- [ ] selection change に応じて menu state を再計算する

### Phase 2: Menu-to-Logic Bridge
**目標**: `Effect` メニューの主要項目を実装へ接続する。

- [ ] `エフェクトコントロール` から現在の effect inspector を開く
- [ ] `すべてを削除` を active layer の effect stack クリアへ接続する
- [ ] 主要カテゴリのうち、実装済み effect を優先して追加できるようにする
- [ ] 実体のない項目は無効化するか、明確な placeholder に寄せる

### Phase 3: Inspector Surface
**目標**: effect のパラメータ編集を Inspector 側で見える形にする。

- [ ] effect item の名前 / 有効 / 順序 / blend / opacity を見せる
- [ ] effect ごとのパラメータ UI を再利用可能にする
- [ ] keyframe / expression と effect parameter の見た目を揃える
- [ ] selection を変えても編集対象が迷子にならないようにする

### Phase 4: Preset and Batch Workflow
**目標**: effect の再利用フローを作る。

- [ ] effect preset の保存・読み込み導線を作る
- [ ] よく使う effect セットを layer / composition 単位で再適用できるようにする
- [ ] `Animation` の expression preset と見た目のルールを揃える
- [ ] 既存の obsolete 項目を整理して、使うものだけを残す

---

## 完了条件

1. 選択中レイヤーに対して effect を追加・削除できる
2. `Effect` メニューから effect inspector を自然に開ける
3. effect の有効/無効と順序が UI 上で分かる
4. 少なくとも一部の effect はパラメータ編集まで到達できる
5. 使えない項目が「押しても無反応」にならない

---

## 補足

- この milestone は、`Composition Final Effect` と競合させず、**レイヤー単位の effect stack** を先に整える。
- いきなり全カテゴリを実装しない。まずは「本当に使う effect」を数個つないで、導線を固める。
- `Animation` の expression / preset 連携は、effect 側のパラメータ編集が見えるようになってから統合する方が混乱しにくい。
