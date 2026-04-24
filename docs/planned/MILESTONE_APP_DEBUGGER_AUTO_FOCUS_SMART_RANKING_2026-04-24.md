# Phase 10 Execution: App Debugger Auto Focus / Smart Ranking

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の改善を、怪しい箇所を自動で前に出す `auto focus / smart ranking` に落とし込むための実行メモ。

この段階では、見やすくするだけでなく、どこを先に見るべきかを機械的に決められるようにする。

---

## 方針

1. failed は最優先で前に出す
2. fallback / stale / missing は warning 扱いで寄せる
3. stall / slow path は time-based に前景化する
4. score は説明可能な単純さを保つ

---

## 現状の課題

- 似た情報が多く、見る順番を人が毎回決める必要がある
- 警告があっても下の方に埋もれやすい
- `overlay` / `present` / `upload` など、重い箇所がひと目で分かりにくい
- 成功している行と失敗している行の優先度が同じに見える
- 長い一覧を上から順に読むコストが高い

---

## 実装タスク

### 1. Auto Focus を入れる

やること:

- failed pass を自動で選ぶ
- missing resource を自動で選ぶ
- fallback path を自動で選ぶ

完了条件:

- 何かおかしいときに最初の候補が分かる

### 2. Smart Ranking を入れる

やること:

- severity
- recency
- duration
- fallback count
- missing count

でスコアを作る。

完了条件:

- 怪しいものが上位に並ぶ

### 3. Slow Path Highlight を入れる

やること:

- overlay / present / upload / decode が重いときに強調する
- duration threshold を超えた行を警告扱いにする

完了条件:

- 重い箇所が埋もれにくい

### 4. Suggestion Line を出す

やること:

- `check playback`
- `inspect resource`
- `open frame diff`
- `copy report`

のような次の一手を 1 つ出す。

完了条件:

- 何をすればよいかが画面から読める

---

## レイアウト方針

- 上部: auto focus result / ranking reason / next action
- 中央: ranked evidence list
- 左側: severity / filter / pin
- 右側: compare / raw detail

補助ルール:

- 自動選出は押しつけすぎない
- rank の理由を短く表示する
- 人が pin で上書きできるようにする

---

## 表示ルール

- `auto focus` は最初に見るべき対象を示す
- `rank` は怪しさの順番を示す
- `highlight` は重さと失敗を示す
- `suggestion` は次の行動を示す

---

## 実装メモ

- `AppDebuggerWidget` の top strip に auto focus を置く
- `Frame` / `Pass` / `Resource` に score を持たせる
- `duration` threshold を視覚的に出す
- `pin` と auto focus を併記する
- `next action` は 1 行で固定する

---

## 完了条件

- 怪しい箇所が自動で前に出る
- 選ばれた理由が読める
- 人が pin で上書きできる
- 次に何をするかが分かる

---

## リスク

- score を複雑にしすぎると逆に信用しにくい
- 自動選出が強すぎると手動調査の自由度が落ちる
- 重い行の強調が多すぎると騒がしくなる

---

## 参照

- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md)
