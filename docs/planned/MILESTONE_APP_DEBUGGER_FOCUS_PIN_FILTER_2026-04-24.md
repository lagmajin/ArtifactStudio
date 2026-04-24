# Phase 6 Execution: App Debugger Focus / Pin / Filter

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の見やすさ改善を、実際に「いま見る対象を絞れる」操作へ落とし込むための実行メモ。

この段階では、情報の見せ方だけでなく、情報の量そのものを局所的に減らす。

---

## 方針

1. 重要なものだけを pin できるようにする
2. 比較対象は compare として分離する
3. filter は検索ではなく視線整理として扱う
4. 一度に全部見せるのではなく、必要な断面だけ出す

---

## 現状の課題

- 画面を見ても、どの情報が今の調査対象か分かりにくい
- `frame / pass / resource / trace` が同時に多く出てしまう
- 異常箇所を固定して追い続ける導線が弱い
- compare と通常表示が混ざると、読む負荷が上がる
- filter があっても「何を絞ったのか」が分かりづらい

---

## 実装タスク

### 1. Pin を導入する

やること:

- current frame を固定して追えるようにする
- 重要 pass / resource / warning を pin する
- pin したものは常時上位表示に寄せる

完了条件:

- 調査対象を 1 つに固定できる

### 2. Compare を独立させる

やること:

- baseline と target を明示する
- compare 対象は通常表示と混ぜない
- diff は要約から入る

完了条件:

- 比較しているのか、今の値を見ているのかが一目で分かる

### 3. Filter を視線整理として扱う

やること:

- `failed`
- `warning`
- `fallback`
- `missing`
- `selected`

のような検索しやすい軸を出す。

完了条件:

- filter した理由が説明可能になる

### 4. Focus Mode を作る

やること:

- pin された frame / pass / resource だけを濃く見せる
- その他は薄くしてノイズを下げる
- raw text は focus 時でも折りたたむ

完了条件:

- 調査対象が少数のとき、読むコストが下がる

---

## レイアウト方針

- 上部: current focus / compare / filter summary
- 左側: filter chips / pin list
- 中央: focused frame / primary evidence
- 右側: compare / raw detail / notes

補助ルール:

- pin は常に見える
- compare は今の値と分ける
- filter は状態が分かる文言にする

---

## 表示ルール

- `pin` は固定対象として強調する
- `compare` は差分対象として色を分ける
- `filter` は絞り込み理由が読めるようにする
- `focus` は主役を 1 つに絞る

---

## 実装メモ

- `AppDebuggerWidget` に pin summary を寄せる
- `Frame Debug View` に compare の見出しを入れる
- `Resource` と `Pass` の filter chip を分ける
- pinned item は top of list に固定する
- filtered out の件数を summary に出す

---

## 完了条件

- 調べたい対象を 1 つに固定できる
- compare が通常閲覧と混ざらない
- filter の意図が見て分かる
- 情報量を減らしながら原因追跡できる

---

## リスク

- pin を増やしすぎると結局見づらくなる
- compare を強くしすぎると通常の確認がしづらくなる
- filter の自由度を上げすぎると導線が複雑になる

---

## 参照

- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE2_EXECUTION_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE2_EXECUTION_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE3_EXECUTION_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE3_EXECUTION_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE4_EXECUTION_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE4_EXECUTION_2026-04-24.md)
