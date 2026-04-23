# Milestone: Composition Editor Playback Feel Refinement (2026-04-23)

**Status:** Planning
**Goal:** コンポジットエディタの再生・スクラブ・playhead 追従を軽く見せ、ワープ感や重さを減らす。

---

## ねらい

いまの Composite Editor は描画そのものだけでなく、playhead の追従や preview 更新の見え方が重く感じやすい。
操作体感が悪いと、実際の処理速度以上に「不快」に見えるため、まずは体感を先に整える。

このマイルストーンでは、再生の正確さを壊さずに、見え方と応答感を改善する。

---

## 現状の課題

- playhead が重く見える
- スクラブ中にワープしたように見える
- preview 更新と playhead 表示のタイミングがずれて見える
- 画面全体の再描画が過剰だと、実際以上に遅く感じる
- 操作中と通常時で preview の負荷制御が揃っていない

既存の調査メモでは、`previewDownsample_` や render key 生成、layer pass の重さが候補になっている。

---

## 改善方針

### Phase 1: playhead 表示の安定化

- playhead 更新を描画本体と分離して扱う
- 連続移動時の見た目を滑らかにする
- 位置の古い残像や重複描画を避ける

### Phase 2: 操作中の preview 軽量化

- 再生中 / スクラブ中 / 停止中の表示負荷を分ける
- 操作中は低負荷の preview に寄せる
- 余計な再計算や再初期化を抑える

### Phase 3: 再描画の集約

- 変更がないときは描画要求をまとめる
- playhead 更新と composition redraw を必要以上に同期させない
- UI の更新頻度を上げすぎない

### Phase 4: 体感の補助表示

- 現在の再生状態を簡潔に示す
- preview 低負荷モードを必要時だけ見せる
- 何が重いかを App Debugger へ返しやすくする

---

## 実装の着手候補

1. playhead overlay の更新経路を見直す
2. 操作中の preview downsample の扱いを再点検する
3. render request の coalescing を強める
4. frame debug に再生状態を載せる

---

## 完了条件

- playhead が滑らかに見える
- スクラブ中のワープ感が減る
- preview が必要以上に重く感じない
- 体感改善とデバッグ観測の両方が揃う

---

## リスク

- 再描画を抑えすぎると追従が鈍くなる
- playhead と preview の更新分離が進みすぎると同期ズレが目立つ
- 体感改善を急ぎすぎると正確性を落とす可能性がある

---

## 参照

- [`COMPOSITE_EDITOR_PERF_INVESTIGATION_2026-04-11.md`](x:/Dev/ArtifactStudio/docs/experiments/COMPOSITE_EDITOR_PERF_INVESTIGATION_2026-04-11.md)
- [`BUG_FIX_COMPOSITION_VIEWPORT_INTERACTION_PERF_2026-03-25.md`](x:/Dev/ArtifactStudio/docs/bugs/BUG_FIX_COMPOSITION_VIEWPORT_INTERACTION_PERF_2026-03-25.md)
- [`MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md)

---

## 次の一手

1. playhead 更新経路を一本化する
2. 操作中 preview の下限を決める
3. 再描画 coalescing の条件を決める
