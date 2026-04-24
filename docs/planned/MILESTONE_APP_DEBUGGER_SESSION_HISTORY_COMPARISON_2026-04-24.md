# Phase 11 Execution: App Debugger Session History / Comparison

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の改善を、過去の状態に戻って比較できる `session history / comparison` に落とし込むための実行メモ。

この段階では、今見えているものだけでなく、少し前の状態と並べて見られるようにする。

---

## 方針

1. 最近見た frame を残す
2. 重要な capture を session history に積む
3. 比較は current / previous の 2 点から始める
4. history は探すためではなく戻るために使う

---

## 現状の課題

- 今の状態は見えても、少し前とどう違うかが分かりにくい
- 一度見つけた異常を再確認しづらい
- capture が増えると、どれが重要だったか見失いやすい
- compare の入り口が弱いと、差分確認の手間が大きい
- 会話や共有の中で、どの frame を話しているかが曖昧になりやすい

---

## 実装タスク

### 1. Recent History を置く

やること:

- 最近見た frame を保持する
- 最近 pin した対象を残す
- 最近 failed した frame を残す

完了条件:

- 直近の調査対象をすぐ呼び戻せる

### 2. Comparison Pair を固定する

やること:

- current
- previous

の 2 点を基本にする。

完了条件:

- 何と何を比べているかが明確になる

### 3. Session Capture を束ねる

やること:

- timestamp
- frame index
- render backend
- warnings
- focus target

を 1 セッションとしてまとめる。

完了条件:

- 後から見返して意味が分かる

### 4. History Summary を出す

やること:

- last good frame
- last failed frame
- last heavy frame
- last compare target

を短く表示する。

完了条件:

- 履歴のどこを見ればよいか分かる

---

## レイアウト方針

- 上部: current / previous / last failure / last good
- 左側: recent history list
- 中央: focused frame / compare pair
- 右側: session notes / raw detail

補助ルール:

- history は短く、詳細は後ろに置く
- compare は 2 点を固定する
- pinned history は先頭に寄せる

---

## 表示ルール

- `history` は戻るための記録
- `compare` は差を読むための記録
- `session` は 1 回の調査のまとまり
- `recent` は一時的な文脈

---

## 実装メモ

- `AppDebuggerWidget` に recent history strip を置く
- `Frame Debug View` に previous frame を追加する
- `compare` の対象を history から再選択できるようにする
- capture entry に last cause を残す
- session note を export bundle に載せる

---

## 完了条件

- 最近の frame をすぐ呼び戻せる
- current / previous の比較が簡単になる
- session の文脈が残る
- 1 回の調査の流れを追える

---

## リスク

- history が増えすぎるとノイズになる
- compare pair を増やしすぎると混乱する
- session を重くしすぎると記録が邪魔になる

---

## 参照

- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_LEGEND_SEMANTIC_KEY_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_LEGEND_SEMANTIC_KEY_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_AUTO_FOCUS_SMART_RANKING_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_AUTO_FOCUS_SMART_RANKING_2026-04-24.md)
