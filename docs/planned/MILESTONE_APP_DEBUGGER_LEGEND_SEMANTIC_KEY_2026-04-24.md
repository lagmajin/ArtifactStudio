# Phase 8 Execution: App Debugger Legend / Semantic Key

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の改善を、色・記号・ラベルの意味がすぐ分かる `legend / semantic key` に落とし込むための実行メモ。

この段階では、見た目を増やすよりも、意味の読み違いを減らすことを優先する。

---

## 方針

1. 色の意味を 1 画面内で再掲する
2. badge は装飾ではなく意味として扱う
3. `now / warning / next / pin / compare / filter` の語義を固定する
4. 略語や記号は legend で逃がす

---

## 現状の課題

- 色を見ても何を表すかが即答しづらい
- badge が増えると意味が推測頼みになる
- `overlay` / `present` / `fallback` などのラベルが埋もれやすい
- `pin` / `compare` / `filter` の状態表示が一目で分かりにくい
- 補助テキストが多いと、主見出しの意味がぼやける

---

## 実装タスク

### 1. Color Legend を置く

やること:

- `normal`
- `info`
- `warning`
- `error`
- `success`

を画面内で再掲する。

完了条件:

- 色の意味を探しに行かなくてよい

### 2. Badge Grammar を固定する

やること:

- `state`
- `cause`
- `mode`
- `scope`
- `scope`
- `status`

のような役割を badge の文法として固定する。

完了条件:

- badge の見た目だけで役割が分かる

### 3. Semantic Key をまとめる

やること:

- `now`
- `warning`
- `next`
- `pin`
- `compare`
- `filter`
- `raw`

を短い辞書として出す。

完了条件:

- 画面の主要なラベルを推測しなくてよい

### 4. Abbreviation Helper を入れる

やること:

- `fallback`
- `stale`
- `missing`
- `compare`
- `bundle`

などの略語や短縮文言に補足を付ける。

完了条件:

- 短いラベルでも読み違えにくい

---

## レイアウト方針

- 上部: semantic key / color legend
- 中央: main state and warning
- 下部: detailed explanation
- 右側: short glossary / note

補助ルール:

- legend は邪魔にならないが、常に見つけられる
- badge は色だけに頼らない
- ラベルは短く、説明は補助へ寄せる

---

## 表示ルール

- 色は意味に直結させる
- badge は状態の要約に使う
- legend は常に画面の文法として機能させる
- 説明文は長くしすぎない

---

## 実装メモ

- `AppDebuggerWidget` の上部に legend strip を置く
- `Frame Debug View` と `State` の badge tone を合わせる
- `warning` と `error` の差を明確にする
- `pin` / `compare` / `filter` のラベルを固定する
- 省略語には hover で補足を出す

---

## 完了条件

- 色とラベルの意味がすぐ分かる
- badge が推測頼みにならない
- 補助語が画面の意味を邪魔しない
- `now / warning / next` を迷わず読める

---

## リスク

- legend を強くしすぎると本体を圧迫する
- badge を増やしすぎると逆に騒がしくなる
- 略語補助を増やしすぎると読みにくくなる

---

## 参照

- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md)
